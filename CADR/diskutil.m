
#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "diskutil.h"

#ifdef WIN32
typedef unsigned long off_t;
#endif

#ifdef __BIG_ENDIAN__
#define NEED_SWAP
#endif

static char *img_filename;
static unsigned int cyls, heads, blocks_per_track;
static char *brand;
static char *text;
char *comment;

static unsigned int buffer[256];


struct part_s parts[MAX_PARTITIONS];
unsigned int part_count;

const char *mcr_name;
const char *lod_name;


static void
_swaplongbytes(unsigned int *buf, int words)
{
	int i;
	unsigned char *p = (unsigned char *)buf;

	for (i = 0; i < words*4; i += 4) {
		unsigned char t, u, v;
		t = p[i];
		u = p[i+1];
		v = p[i+2];
		p[i] = p[i+3];
		p[i+1] = v;
		p[i+2] = u;
		p[i+3] = t;
	}
}

static void
swapwords(unsigned int *buf)
{
	int i;
	unsigned short *p = (unsigned short *)buf;

	for (i = 0; i < 256*2; i += 2) {
		unsigned short t;
		t = p[i];
		p[i] = p[i+1];
		p[i+1] = t;
	}
}

static int
add_partition(const char *name, unsigned int start, unsigned int size, int ptype, char label[17], const char *filename)
{
	struct part_s *p = &parts[part_count++];
	if (part_count > MAX_PARTITIONS) {
		part_count--;
		return -1;
	}
	p->name = name;
	p->start = start;
	p->size = size;
	p->ptype = ptype;
    strncpy(p->label, label, 16);
    p->label[16] = '\0';
    
    // make sure the label is padded out with spaces
    for (size_t i = strlen(p->label); i < 16; i++)
        p->label[i] = ' ';

	p->filename = filename ? strdup(filename) : 0;
	return 0;
}

static unsigned int
str4(const char *s)
{
	return (unsigned int)((s[3]<<24) | (s[2]<<16) | (s[1]<<8) | s[0]);
}

static char *
unstr4(unsigned long s)
{
	static char b[5];
	b[3] = (char)(s >> 24);
	b[2] = (char)(s >> 16);
	b[1] = (char)(s >> 8);
	b[0] = (char)s;
	b[4] = 0;
	return b;
}

static int
make_labl(int fd)
{
	printf("making LABL...\n");

	memset((char *)buffer, 0, sizeof(buffer));

	buffer[0] = str4("LABL");	/* label LABL */
	buffer[1] = 1;			/* version = 1 */
	buffer[2] = cyls;		/* # cyls */
	buffer[3] = heads;		/* # heads */
	buffer[4] = blocks_per_track;	/* # blocks */
	buffer[5] = heads*blocks_per_track; /* heads*blocks */
	buffer[6] = str4(mcr_name);	/* name of micr part */
	buffer[7] = str4(lod_name);	/* name of load part */

	{
		unsigned int i;
		int p = 0200;
		
		printf("%d partitions\n", part_count);

		buffer[p++] = part_count; /* # of partitions */
		buffer[p++] = 7; /* words / partition */

		for (i = 0; i < part_count; i++) {
			unsigned int n;
			const char *pn = parts[i].name;

			printf("%s, start %o, size %o\n",
			       pn, parts[i].start, parts[i].size);

			n = str4(pn);

			buffer[p++] = n;
			buffer[p++] = parts[i].start;
			buffer[p++] = parts[i].size;
			buffer[p++] = str4(&parts[i].label[0]);
			buffer[p++] = str4(&parts[i].label[4]);
			buffer[p++] = str4(&parts[i].label[8]);
			buffer[p++] = str4(&parts[i].label[12]);

		}
	}

//#define LABEL_PAD_CHAR '\200'
#define LABEL_PAD_CHAR '\0'
	/* pack brand text - offset 010, 32 bytes */
	memset((char *)&buffer[010], LABEL_PAD_CHAR, 32);
	if (brand) {
		memcpy((char *)&buffer[010], brand, strlen(brand)+1);
		printf("brand: '%s'\n", brand);
	}

	/* pack text label - offset 020, 32 bytes */
	memset((char *)&buffer[020], ' ', 32);
	memcpy((char *)&buffer[020], text, strlen(text)+1);
	printf("text: '%s'\n", text);

	/* comment - offset 030, 32 bytes */
	memset((char *)&buffer[030], LABEL_PAD_CHAR, 32);
	strcpy((char *)&buffer[030], comment);
	printf("comment: '%s'\n", comment);

#ifdef NEED_SWAP
	/* don't swap the text */
	_swaplongbytes(&buffer[0], 8);
	_swaplongbytes(&buffer[0200], 128);
#endif

	if (write(fd, buffer, 256*4) != 256*4)
		return -1;

	return 0;
}

int
read_labl(const char *filename)
{
    ssize_t ret;
	int fd, p;
	unsigned int i, count, size;
    
	fd = open(filename, O_RDONLY, 0666);
	if (fd < 0) {
		perror(filename);
		return -1;
	}
    
	ret = read(fd, buffer, 256*4);
	if (ret != 256*4) {
		perror(filename);
		return -1;
	}
    
#ifdef NEED_SWAP
	/* don't swap the text */
	_swaplongbytes(&buffer[0], 8);
	_swaplongbytes(&buffer[0200], 128);
#endif
    
	if (buffer[0] != str4("LABL")) {
		fprintf(stderr, "%s: no valid disk label found\n", filename);
		return -1;
	}
    
	if (buffer[1] != 1) {
		fprintf(stderr, "%s: label version not 1\n", filename);
		return -1;
	}
    
	cyls = buffer[2];		/* # cyls */
	heads = buffer[3];		/* # heads */
	blocks_per_track = buffer[4];	/* # blocks */
    if (mcr_name)
        free((void *)mcr_name);
	mcr_name = strdup(unstr4(buffer[6]));	/* name of micr part */
    if (lod_name)
        free((void *)lod_name);
	lod_name = strdup(unstr4(buffer[7]));	/* name of load part */
    
	count = buffer[0200];
	size = buffer[0201];
	p = 0202;
    
	part_count = 0;
	for (i = 0; i < count; i++) {
        char label[17];

        memcpy(&label[0], unstr4(buffer[p+3]), 4);
        memcpy(&label[4], unstr4(buffer[p+4]), 4);
        memcpy(&label[8], unstr4(buffer[p+5]), 4);
        memcpy(&label[12], unstr4(buffer[p+6]), 4);
        label[16] = '\0';
		add_partition(strdup(unstr4(buffer[p+0])),
                      buffer[p+1], buffer[p+2], 0, label, NULL);
		
		p += size;
	}
    
	brand = strdup((char *)&buffer[010]);
	text = strdup((char *)&buffer[020]);
	comment = strdup((char *)&buffer[030]);

#if 0
	printf("#\n");
	printf("output:\n");
	printf("%s:\n", filename);
    
	printf("#\n");
	printf("label:\n");
	printf("cyls\t%d\n", cyls);
	printf("heads\t%d\n", heads);
	printf("blockspertrack\t%d\n", blocks_per_track);
    
	if (brand[0] && (brand[0] & 0xff) != 0200) printf("brand\t%s\n", brand);
	if (text[0]) printf("text\t%s\n", text);
	if (comment[0]) printf("comment\t%s\n", comment);
    
	printf("mcr\t%s\n", mcr_name);
	printf("lod\t%s\n", lod_name);
    
	printf("#\n");
	printf("partitions:\n");
    
	for (i = 0; i < part_count; i++) {
		printf("%s\t0%o\t0%o\t%s\t%s\n",
		       parts[i].name,
		       parts[i].start, parts[i].size, parts[i].label,
		       parts[i].filename ? parts[i].filename : "");
	}
#endif

	close(fd);
    return 0;
}

static int
read_block(int fd, unsigned int block_no, unsigned char *buf)
{
	off_t offset, retoffset;
    ssize_t ret;
	size_t size;

	offset = block_no * (256*4);
	retoffset = lseek(fd, offset, SEEK_SET);

	if (retoffset != offset) {
		perror("lseek");
		return -1;
	}

	size = 256*4;
	ret = read(fd, buf, size);
	if (ret != (ssize_t)size) {
		printf("disk read error; ret %zd, size %zd\n", ret, size);
		perror("read");
		return -1;
	}

	return 0;
}

static int
write_block(int fd, unsigned int block_no, unsigned char *buf)
{
	off_t offset, retoffset;
    ssize_t ret;
	size_t size;

	offset = block_no * (256*4);
	//printf("block_no %d (%x), offset %lld\n", block_no, block_no, offset);

	retoffset = lseek(fd, offset, SEEK_SET);
	//printf("lseek ret %lld\n", ret);

	if (retoffset != offset) {
		perror("lseek");
		return -1;
	}

	size = 256*4;
	ret = write(fd, buf, size);
	if (ret != (ssize_t)size) {
		printf("disk write error; ret %zd, size %zd\n", ret, size);
		perror("write");
		return -1;
	}

	return 0;
}

static int
make_one_partition(int fd, unsigned int index)
{
	ssize_t ret;
    unsigned int count;
    int fd1;
    unsigned int offset;
	unsigned char b[256*4];

	printf("making %s... ", parts[index].name);

	count = 0;
	offset = parts[index].start;
	printf("offset %o, ", offset);
	fflush(stdout);

	if (parts[index].filename && parts[index].filename[0]) {
		fd1 = open(parts[index].filename, O_RDONLY);
		if (fd1 < 0) {
			perror(parts[index].filename);
			return -1;
		}


		printf("copy %s, ", parts[index].filename);
		fflush(stdout);
		
		while (1) {
			ret = read(fd1, b, 256*4);
			if (ret <= 0)
				break;

			if (memcmp(parts[index].name, "MCR", 3) == 0) {
				swapwords((unsigned int *)b);
			}

			if (write_block(fd, offset+count, b))
				break;
			count++;

			if (ret < 256*4)
				break;
		}
		close(fd1);

		memset(b, 0, sizeof(b));
		while (count < parts[index].size) {
			if (write_block(fd, offset+count, b))
				break;
			count++;
		}
	} else {
		/* zero blocks */
		memset(b, 0, sizeof(b));

		printf("zero, ");
		fflush(stdout);

		for (count = 0; count < parts[index].size; count++) {
			if (write_block(fd, offset+count, b))
				break;
		}
	}

	printf("%d blocks\n", count);
	return 0;
}

static void
make_partitions(int fd)
{
	unsigned int i;

	for (i = 0; i < part_count; i++) {
		make_one_partition(fd, i);
	}
}

/*
 * read template file, describing partitions
 */
static int
parse_template(char *template)
{
	FILE *f;
	char line[256], what[256], str[256];
	int mode;
    unsigned int start, size;

	f = fopen(template, "r");
	if (f == NULL) {
		return 0;
	}

	part_count = 0;
	mode = 0;

	while (fgets(line, sizeof(line), f)) {
		if (line[0]) {
			size_t l = strlen(line);
			line[l-1] = 0;
		}
		if (line[0] == '#') continue;

		if (strcmp(line, "output:") == 0) {
			mode = 1;
			continue;
		}
		if (strcmp(line, "label:") == 0) {
			mode = 2;
			continue;
		}
		if (strcmp(line, "partitions:") == 0) {
			mode = 3;
			continue;
		}

		if (0) printf("%d: %s\n", mode, line);

		switch(mode) {
		case 1:
			img_filename = strdup(line);
			break;
		case 2:
			sscanf(line, "%s\t%s", what, str);
			if (0) printf("what '%s', str '%s'\n", what, str);

			if (strcmp(what, "cyls") == 0)
				cyls = (unsigned int)atoi(str);
			if (strcmp(what, "heads") == 0)
				heads = (unsigned int)atoi(str);
			if (strcmp(what, "blockspertrack") == 0)
				blocks_per_track = (unsigned int)atoi(str);
			if (strcmp(what, "mcr") == 0)
				mcr_name = strdup(str);
			if (strcmp(what, "lod") == 0)
				lod_name = strdup(str);
			if (strcmp(what, "brand") == 0)
				brand = strdup(str);
			if (strcmp(what, "text") == 0)
				text = strdup(str);
			if (strcmp(what, "comment") == 0)
				comment = strdup(str);
			break;
		case 3:
			start = size = 0;
			what[0] = str[0] = 0;
			sscanf(line, "%s\t%o\t%o\t%s",
			       what, &start, &size, str);
			if (what[0] && start > 0 && size > 0) {
				add_partition(strdup(what), start, size,
					      0, "", strdup(str));
			}
			break;
		}
	}

	fclose(f);
	return 0;
}

static int
fillout_image_file(int fd)
{
	unsigned int i;
    int ret;
    unsigned int last_block_no;
	off_t offset;

	/* find the highest block # + 1 */
	last_block_no = 0;
	for (i = 0; i < part_count; i++) {
		unsigned int last = parts[i].start + parts[i].size;
		if (last > last_block_no)
			last_block_no = last;
	}

	offset = (last_block_no + 1) * (256*4);
	//if (0) printf("last block %d (0%o), expanding file to %lld\n",
	//	      last_block_no, last_block_no, offset-1);

	ret = ftruncate(fd, offset-1);
	if (0) printf("ret %d\n", ret);

	if (ret) {
		perror("ftruncate");
		return -1;
	}

#if 0
	{
		unsigned char b[256*4];

		/* zero blocks */
		memset(b, 0, sizeof(b));
		for (i = 0; i < 1000; i++) {
			if (write(fd, b, 1024))
				break;
		}
	}
#endif

	lseek(fd, (off_t)0, SEEK_SET);

	return 0;
}

static int
create_disk(char *template, char *image_filename)
{
	int fd;

	if (parse_template(template))
		return -1;

    // allow override
    if (image_filename)
        img_filename = image_filename;

	printf("creating %s\n", img_filename);

	fd = open(img_filename, O_RDWR | O_CREAT, 0666);
	if (fd < 0) {
		perror(img_filename);
		return -1;
	}

//#ifdef __APPLE__
//	fillout_image_file(fd);
//#endif

	make_labl(fd);
	make_partitions(fd);

	close(fd);

	return 0;
}

/*
 * this is dangerous, and generally will only work if you
 * modify the last partition.
 *
 * but it can be useful if you want to change the size of a partition
 * and not loose some existing info.
 */

static int
modify_disk(char *template, char *filename, char *partition_name)
{
	int fd;
    unsigned int part_index;
    unsigned int i;

	if (template == NULL) {
	  fprintf(stderr, "missing template filename\n");
	  return -1;
	}

	if (filename == NULL) {
	  fprintf(stderr, "missing image filename\n");
	  return -1;
	}

	if (parse_template(template))
		return -1;

	part_index = 0xffffffff;
	for (i = 0; i < part_count; i++) {
	  if (strcmp(parts[i].name, partition_name) == 0) {
	    part_index = i;
	    break;
	  }
	}

	if (i == part_count) {
	  fprintf(stderr, "can't find partition '%s'\n", partition_name);
	  return -1;
	}

	printf("modifying %s\n", filename);

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		perror(filename);
		return -1;
	}

	printf("re-write label\n");
	make_labl(fd);

	printf("modify partition %d, '%s'\n",
	       part_index, parts[part_index].name);

	make_one_partition(fd, part_index);

	close(fd);

	return 0;
}

static int
modify_labl(char *template, char *filename)
{
	int fd;

	if (template == NULL) {
	  fprintf(stderr, "missing template filename\n");
	  return -1;
	}

	if (filename == NULL) {
	  fprintf(stderr, "missing image filename\n");
	  return -1;
	}

	if (parse_template(template))
	  return -1;

	printf("modifying %s\n", filename);

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		perror(filename);
		return -1;
	}

	printf("re-write label\n");
	make_labl(fd);

	close(fd);

	return 0;
}

void
default_template(void)
{
	if (img_filename == NULL)
		img_filename = strdup("disk.img");

	/* try to look like a Trident T-300 */
	cyls = 815;
	heads = 19;
	blocks_per_track = 17;

    NSBundle *mainBundle = [NSBundle mainBundle];
    
#define DEFAULT_MCR_FILE	"ucadr.mcr.841"
#define DEFAULT_LOD_FILE	"partition-78.50.lod1"

	part_count = 0;
	add_partition("MCR1", 021,      0224,	0, "ucadr.mcr.841   ", [[mainBundle pathForResource:@"ucadr.mcr.841" ofType:nil] cStringUsingEncoding:NSASCIIStringEncoding]);
	add_partition("MCR2", 0245,     0224,	0, "ucadr.mcr.979   ", [[mainBundle pathForResource:@"ucadr.mcr.979" ofType:nil] cStringUsingEncoding:NSASCIIStringEncoding]);
    add_partition("MCR3", 0471,     0224,   0, "", NULL);
    add_partition("MCR4", 0715,     0224,   0, "", NULL);
	add_partition("PAGE", 01141,	0100000,0, "", NULL);
	add_partition("LOD1", 0101141,  061400,	0, "78.48           ", [[mainBundle pathForResource:@"partition-78.48-LOD1" ofType:nil] cStringUsingEncoding:NSASCIIStringEncoding]);
	add_partition("LOD2", 0162541,  061400,	0, "sys210          ", [[mainBundle pathForResource:@"partition-sys210-LOD2" ofType:nil] cStringUsingEncoding:NSASCIIStringEncoding]);
    add_partition("LOD3", 0244141,  061400, 0, "", NULL);
    add_partition("LOD4", 0325541,  061400, 0, "", NULL);
	add_partition("FILE", 0407141,  0400000,0, "sys99 sources   ", [[mainBundle pathForResource:@"FILE.sys99" ofType:nil] cStringUsingEncoding:NSASCIIStringEncoding]);

    if (mcr_name)
        free((void *)mcr_name);
	mcr_name = strdup("MCR1");
    if (lod_name)
        free((void *)lod_name);
	lod_name = strdup("LOD1");

	brand = NULL;
	text = "CADR diskmaker image";
	comment = DEFAULT_MCR_FILE;
}

/*
 * read disk image and dump info
 */
static int
show_partition_info(char *filename)
{
    ssize_t ret;
	int fd, p;
	unsigned int i, count, size;

	fd = open(filename, O_RDONLY, 0666);
	if (fd < 0) {
		perror(filename);
		return -1;
	}

	ret = read(fd, buffer, 256*4);
	if (ret != 256*4) {
		perror(filename);
		return -1;
	}

#ifdef NEED_SWAP
	/* don't swap the text */
	_swaplongbytes(&buffer[0], 8);
	_swaplongbytes(&buffer[0200], 128);
#endif

	if (buffer[0] != str4("LABL")) {
		fprintf(stderr, "%s: no valid disk label found\n", filename);
		return -1;
	}

	if (buffer[1] != 1) {
		fprintf(stderr, "%s: label version not 1\n", filename);
		return -1;
	}

	cyls = buffer[2];		/* # cyls */
	heads = buffer[3];		/* # heads */
	blocks_per_track = buffer[4];	/* # blocks */
    if (mcr_name)
        free((void *)mcr_name);
	mcr_name = strdup(unstr4(buffer[6]));	/* name of micr part */
    if (lod_name)
        free((void *)lod_name);
	lod_name = strdup(unstr4(buffer[7]));	/* name of load part */

	count = buffer[0200];
	size = buffer[0201];
	p = 0202;

	part_count = 0;
	for (i = 0; i < count; i++) {
        char label[17];
        
        memcpy(&label[0], unstr4(buffer[p+3]), 4);
        memcpy(&label[4], unstr4(buffer[p+4]), 4);
        memcpy(&label[8], unstr4(buffer[p+5]), 4);
        memcpy(&label[12], unstr4(buffer[p+6]), 4);
        label[16] = '\0';
		add_partition(strdup(unstr4(buffer[p+0])),
			      buffer[p+1], buffer[p+2], 0, label, NULL);
		
		p += size;
	}

	brand = strdup((char *)&buffer[010]);
	text = strdup((char *)&buffer[020]);
	comment = strdup((char *)&buffer[030]);

	printf("#\n");
	printf("output:\n");
	printf("%s:\n", filename);

	printf("#\n");
	printf("label:\n");
	printf("cyls\t%d\n", cyls);
	printf("heads\t%d\n", heads);
	printf("blockspertrack\t%d\n", blocks_per_track);

	if (brand[0] && (brand[0] & 0xff) != 0200) printf("brand\t%s\n", brand);
	if (text[0]) printf("text\t%s\n", text);
	if (comment[0]) printf("comment\t%s\n", comment);

	printf("mcr\t%s\n", mcr_name);
	printf("lod\t%s\n", lod_name);

	printf("#\n");
	printf("partitions:\n");

	for (i = 0; i < part_count; i++) {
		printf("%s\t0%o\t0%o\t%s\n",
		       parts[i].name,
		       parts[i].start, parts[i].size,
		       parts[i].filename ? parts[i].filename : "");
	}

	close(fd);
	return 0;
}

int
extract_partition(const char *filename, const char *extract_filename, const char *partition_name)
{
    ssize_t ret;
	int fd, fd_out, p, result;
	unsigned int i, count, offset, size;

	result = -1;

	fd = open(filename, O_RDONLY, 0666);
	if (fd < 0) {
		perror(filename);
		return -1;
	}

	ret = read(fd, buffer, 256*4);
	if (ret != 256*4) {
		perror(filename);
		close(fd);
		return -1;
	}

#ifdef NEED_SWAP
	/* don't swap the text */
	_swaplongbytes(&buffer[0], 8);
	_swaplongbytes(&buffer[0200], 128);
#endif

	if (buffer[0] != str4("LABL")) {
		fprintf(stderr, "%s: no valid disk label found\n", filename);
		close(fd);
		return -1;
	}

	if (buffer[1] != 1) {
		fprintf(stderr, "%s: label version not 1\n", filename);
		close(fd);
		return -1;
	}

	cyls = buffer[2];		/* # cyls */
	heads = buffer[3];		/* # heads */
	blocks_per_track = buffer[4];	/* # blocks */
    if (mcr_name)
        free((void *)mcr_name);
	mcr_name = strdup(unstr4(buffer[6]));	/* name of micr part */
    if (lod_name)
        free((void *)lod_name);
	lod_name = strdup(unstr4(buffer[7]));	/* name of load part */

	count = buffer[0200];
	size = buffer[0201];
	p = 0202;

	part_count = 0;
	for (i = 0; i < count; i++) {
        char label[17];
        
        memcpy(&label[0], unstr4(buffer[p+3]), 4);
        memcpy(&label[4], unstr4(buffer[p+4]), 4);
        memcpy(&label[8], unstr4(buffer[p+5]), 4);
        memcpy(&label[12], unstr4(buffer[p+6]), 4);
        label[16] = '\0';
		add_partition(strdup(unstr4(buffer[p+0])),
			      buffer[p+1], buffer[p+2], 0, label,
                      NULL);
		
		p += size;
	}

	brand = strdup((char *)&buffer[010]);
	text = strdup((char *)&buffer[020]);
	comment = strdup((char *)&buffer[030]);

	offset = 0;

	for (i = 0; i < part_count; i++) {
	  if (strcmp(parts[i].name, partition_name) == 0) {
	    offset = parts[i].start;
	    size = parts[i].size;
	    break;
	  }
	}

	if (i == part_count) {
	  fprintf(stderr, "can't find partition '%s'\n", partition_name);
	  result = -1;
	} else {
	  unsigned char b[256*4];

	  printf("extracting partition '%s' at %o from %s\n",
		 partition_name, offset, filename);

	  fd_out = open(extract_filename, O_RDWR|O_CREAT, 0666);
	  if (fd_out < 0) {
	    perror(filename);
	    close(fd);
	    return -1;
	  }

	  for (count = 0; count < size; count++) {
	    if (read_block(fd, offset+count, b))
	      break;
	    if (write_block(fd_out, count, b))
	      break;
	  }

	  close(fd_out);
	}

	close(fd);
	return result;
}

int
set_current_band(const char *filename, const char *partition_name)
{
    int fd;
    int found = 0;

    if (read_labl(filename) < 0)
        return -1;

    for (unsigned int i = 0; i < part_count; i++)
    {
        if (strcasecmp(partition_name, parts[i].name) == 0)
        {
            if (lod_name)
                free((void *)lod_name);
            lod_name = strdup(parts[i].name);
            found = 1;
            break;
        }
    }
    
    if (!found)
    {
        printf("can't find band %s\n", partition_name);
        return -1;
    }

 	fd = open(filename, O_RDWR);
	if (fd < 0) {
		perror(filename);
		return -1;
	}
    
	printf("re-write label\n");
	make_labl(fd);
    
	close(fd);

    return 0;
}

int
set_current_mcr(const char *filename, const char *partition_name)
{
    int fd;
    int found = 0;
    
    if (read_labl(filename) < 0)
        return -1;
    
    for (unsigned int i = 0; i < part_count; i++)
    {
        if (strcasecmp(partition_name, parts[i].name) == 0)
        {
            if (mcr_name)
                free((void *)mcr_name);
            mcr_name = strdup(parts[i].name);
            found = 1;
            break;
        }
    }
    
    if (!found)
    {
        printf("can't find band %s\n", partition_name);
        return -1;
    }
    
 	fd = open(filename, O_RDWR);
	if (fd < 0) {
		perror(filename);
		return -1;
	}
    
	printf("re-write label\n");
	make_labl(fd);
    
	close(fd);
    
    return 0;
}


int
create_disk_image(const char *image_filename)
{
	int fd;

	printf("creating %s\n", image_filename);
    
	fd = open(image_filename, O_RDWR | O_CREAT, 0666);
	if (fd < 0) {
		perror(image_filename);
		return -1;
	}
    
    //#ifdef __APPLE__
    //	fillout_image_file(fd);
    //#endif
    
	make_labl(fd);
	make_partitions(fd);
    
	close(fd);

    NSURL *fileURL = [NSURL fileURLWithPath:[NSString stringWithCString:image_filename encoding:NSASCIIStringEncoding]];
    NSError *error;

    [fileURL setResourceValue:@"com.cadr.disk" forKey:NSURLTypeIdentifierKey error:&error];

	return 0;
}

