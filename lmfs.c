// lmfs --- crude hack to manage Symbolics LMFS partitions

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int do_dir;
int do_read;
int do_write;

char *img_filename;
char *path;

void dumpmem(char *ptr, int len);

typedef struct {
	unsigned char buffer[8192];

	unsigned char *base;
	int offset;
	int record_no;
	int fd;
} baccess;

void
init_access(baccess *pb, int fd)
{
	pb->base = pb->buffer;
	pb->offset = 0;
	pb->record_no = -1;
	pb->fd = fd;
}

void *
ensure_access(baccess *pb, int offset, int size)
{
	unsigned char *ptr;
	int blknum;
	int boff;
	int left;

	blknum = offset / 1008;
	boff = offset % 1008;
	left = 1008 - boff;

	if (blknum > 3) {
		printf("ensure_access: past end!\n");
		exit(1);
	}

	if (left < size) {
		blknum++;
		boff = 0;
	}

	ptr = pb->base + (1024 * blknum) + 8 + boff;

	pb->offset = offset;

	return ptr;
}

void *
advance_access(baccess *pb, int size)
{
	unsigned char *ptr;
	int blknum;
	int boff;
	int left;

	pb->offset += size;

	blknum = pb->offset / 1008;
	boff = pb->offset % 1008;
	left = 1008 - boff;

	if (blknum > 3) {
		return 0;
	}

	printf("left %d, size %d\n", left, size);
	if (left < size) {
		blknum++;
		boff = 0;
		pb->offset = blknum * 1008;

		if (blknum == 4) {
			return 0;
		}
	}

	ptr = pb->base + (1024 * blknum) + 8 + boff;

	printf("offset %d\n", pb->offset);

	return ptr;
}

int
remaining_access(baccess *pb)
{
	int boff;
	int left;

	boff = pb->offset % 1008;
	left = 1008 - boff;
	return left;
}

int
remaining_buffer(baccess *pb)
{
	return (1008 * 4) - pb->offset;
}

typedef int fixnum;
typedef int address;
typedef int date;
typedef char flag;

typedef struct tapeinfo_s {
	date date;
	char tapename[8];
} tapeinfo;

typedef struct fh_element_s {
	char name[4];
	short location;		// In words.
	short length;		// In words.
} fh_element;

struct partition_label_s {
	fixnum version;
	short name_len;
	char name[30];
	fixnum label_size;
	fixnum partition_id;
	struct {
		address primary_root;
		address free_store_info;
		address bad_track_info;
		address volume_table;
		address aspare[4];
	} disk_address;

	struct {
		date label_accepted;
		date shut_down;
		date free_map_reconstructed;
		date structure_salvaged;
		date scavenged;
		date tspare[4];
	} update_times;

	fixnum uid_generator;
	fixnum monotone_generator;
	fh_element root_list;
};

typedef struct link_transparencies_s {
	struct {
		flag read_thru;
		flag write_thru;
		flag create_thru;
		flag rename_thru;
		flag delete_thru;
		flag lpad[6];
		int ignore;
	} attributes;
} link_transparencies;

struct file_map_s {
	fixnum allocated_length;
	short valid_length;
	short link;
	fixnum element[1];
};

struct dir_header_s {
	short version;
	short size;
	short name_len;
	char name[30];

	short number_of_entries;
	short free_entry_list;
	short entries_index_offset;
	short direntry_size;
	short entries_per_block;
	short default_generation_retention_count;
	short uid_path_offset;
	short uid_path_length;
	short hierarch_depth;
	fixnum default_volid;
	flag auto_expunge_p;
	date auto_expunge_interval;
	date auto_expunge_last_time;

	link_transparencies default_link_transparencies;
};

struct directory_entry_s {
	short file_name_len;
	char file_name[30];
	short file_type_len;
	char file_type[14];
	char file_version[3];
	char bytesize;
	short author_len;
	char author[14];
	short file_name_true_length;
	fixnum number_of_records;

	struct {
		date date_time_created;
		date date_time_deleted;
		date date_time_reconstructed;
		date date_time_modified;
		date date_time_used;
	} dates;

	fixnum unique_ID;
	fixnum byte_length;
	char generation_retention_count;
	char partition_index[3];
	address record_0_address;

	tapeinfo archive_a;
	tapeinfo archive_b;
	short ignore_mode_word;
};

struct file_header_s {
	fixnum version;
	fixnum logical_size;
	fixnum bootload_generation;
	fixnum version_in_bootload;
	short number_of_elements;
	short ignore_mode_word;
	fh_element parts_array[8];
};

char
tohex(char b)
{
	b = b & 0xf;
	if (b < 10)
		return '0' + b;
	return 'a' + (b - 10);
}

void
dumpmem(char *ptr, int len)
{
	char line[80];
	char chars[80];
	char *p;
	char b;
	char *c;
	char *end;
	int j;
	int offset;

	end = ptr + len;
	offset = 0;
	while (ptr < end) {
		p = line;
		c = chars;
		printf("%04x ", offset);
		*p++ = ' ';
		for (j = 0; j < 16; j++) {
			if (ptr < end) {
				b = *ptr++;
				*p++ = tohex(b >> 4);
				*p++ = tohex(b);
				*p++ = ' ';
				*c++ = ' ' <= b && b <= '~' ? b : '.';
			} else {
				*p++ = 'x';
				*p++ = 'x';
				*p++ = ' ';
				*c++ = 'x';
			}
		}
		*p = 0;
		*c = 0;
		printf("%s %s\n", line, chars);
		offset += 16;
	}
}

int
read_block(int fd, int block_no, unsigned char *buf)
{
	off_t offset;
	off_t ret;
	int size;

	offset = block_no * (256 * 4);
	ret = lseek(fd, offset, SEEK_SET);
	if (ret != offset) {
		perror("lseek");
		return -1;
	}
	size = 256 * 4;
	ret = read(fd, buf, size);
	if (ret != size) {
		printf("disk read error; ret %d, size %d\n", (int) ret, size);
		perror("read");
		return -1;
	}
	return 0;
}

int
write_block(int fd, int block_no, unsigned char *buf)
{
	off_t offset;
	off_t ret;
	int size;

	offset = block_no * (256 * 4);
	ret = lseek(fd, offset, SEEK_SET);
	if (ret != offset) {
		perror("lseek");
		return -1;
	}
	size = 256 * 4;
	ret = write(fd, buf, size);
	if (ret != size) {
		printf("disk write error; ret %d, size %d\n", (int) ret, size);
		perror("write");
		return -1;
	}
	return 0;
}

int
read_record(baccess *pb, int record_no)
{
	int i;
	int block_no;
	unsigned char *pbuf;

	block_no = record_no * 4;
	pbuf = pb->buffer;
	pb->record_no = record_no;
	pb->offset = 0;

	for (i = 0; i < 4; i++) {
		read_block(pb->fd, block_no, pbuf);
		pbuf += 256 * 4;
		block_no++;
	}

	return 0;
}

#define STANDARD_BLOCK_SIZE 256
#define STANDARD_BLOCKS_PER_RECORD 4

int
show_file(int fd, struct directory_entry_s *de, int record_no)
{
	baccess b;
	int i;
	int woffset;
	int wlast;
	int bl;
	int tot;
	struct file_header_s *fh;
	int n;
	int blocks[64];
	char *p;

	init_access(&b, fd);
	read_record(&b, record_no);

	fh = (struct file_header_s *) ensure_access(&b, 0, sizeof(struct file_header_s));

	printf("logical_size %d\n", fh->logical_size);
	printf("number_of_elements %d\n", fh->number_of_elements);
	for (i = 0; i < 8; i++) {
		char n[5];
		memcpy(n, fh->parts_array[i].name, 4);
		n[4] = 0;
		printf("%d: %s loc %d len %d\n", i, n, fh->parts_array[i].location, fh->parts_array[i].length);
	}
	for (i = 0; i < 8; i++) {
		woffset = fh->parts_array[i].location;
		wlast = fh->parts_array[i].location;

		if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
			struct file_map_s *fm;
			int j;
			fm = (struct file_map_s *) ensure_access(&b, woffset * 4, sizeof(struct file_map_s));
			printf("allocated_length %d\n", fm->allocated_length);
			printf("valid_length %d\n", fm->valid_length);
			printf("link %d\n", fm->link);
			printf("element[0] %d\n", fm->element[0]);
			for (j = 0; j < fm->valid_length; j++)
				blocks[j] = fm->element[j];
		}
	}

	bl = de->byte_length;
	printf("wlast %d\n", wlast);
	p = (char *) ensure_access(&b, wlast * 4, 0);
	tot = 0;

	int fdd;
	{
		char nn[1024];

		sprintf(nn, "tmp/%s", de->file_name);
		unlink(nn);
		fdd = open(nn, O_RDWR | O_TRUNC | O_CREAT, 0666);
	}

	n = 0;
	while (1) {
		int left;
		int use;

		left = remaining_access(&b);
		use = bl < left ? bl : left;
		tot += use;

		if (fdd > 0)
			write(fdd, p, use);

		bl -= use;
		p = (char *) advance_access(&b, use);

		if (remaining_buffer(&b) == 0) {
			printf("read record %d\n", blocks[n]);
			read_record(&b, blocks[n++]);
			p = (char *) ensure_access(&b, 0, 0);
		}

		if (bl <= 0)
			break;
	}

	printf("done\n");
	if (fdd > 0)
		close(fdd);

	return 0;
}

int
show_de(int fd, int record_no)
{
	baccess b;
	struct directory_entry_s *de;
	int i;
	int woffset;
	int wlast;
	int numentries;
	struct file_header_s *fh;
	int n;
	int blocks[64];

	init_access(&b, fd);
	read_record(&b, record_no);

	printf("%d\n", record_no);

	fh = (struct file_header_s *) ensure_access(&b, 0, sizeof(struct file_header_s));
	printf("number_of_elements %d\n", fh->number_of_elements);
	for (i = 0; i < 8; i++) {
		char n[5];

		memcpy(n, fh->parts_array[i].name, 4);
		n[4] = 0;
		printf("%d: %s loc %d len %d\n", i, n, fh->parts_array[i].location, fh->parts_array[i].length);
	}

	for (i = 0; i < 8; i++) {
		woffset = fh->parts_array[i].location;
		wlast = fh->parts_array[i].location;

		if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
			struct file_map_s *fm;
			int j;

			fm = (struct file_map_s *) ensure_access(&b, woffset * 4, sizeof(struct file_map_s));
			printf("allocated_length %d\n", fm->allocated_length);
			printf("valid_length %d\n", fm->valid_length);
			printf("link %d\n", fm->link);
			printf("element[0] %d\n", fm->element[0]);
			for (j = 0; j < fm->valid_length; j++)
				blocks[j] = fm->element[j];
		}

		if (memcmp(fh->parts_array[i].name, "dire", 4) == 0) {
			struct directory_entry_s *de;

			de = (struct directory_entry_s *) ensure_access(&b, woffset * 4, sizeof(struct directory_entry_s));
			printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
			printf("byte_length %d\n", de->byte_length);
			printf("number_of_records %d\n", de->number_of_records);
			printf("record_0_address %d\n", de->record_0_address);
		}
	}

	struct dir_header_s *dh;

	woffset = wlast;
	dh = (struct dir_header_s *) ensure_access(&b, woffset * 4, sizeof(struct dir_header_s));
	printf("version %d, size %d, name '%s'\n", dh->version, dh->size, dh->name);
	printf("number_of_entries %d\n", dh->number_of_entries);
	printf("entries_index_offset %d\n", dh->entries_index_offset);
	printf("uid_path_offset %d\n", dh->uid_path_offset);
	printf("uid_path_length %d\n", dh->uid_path_length);

	numentries = dh->number_of_entries;
	n = 0;
	for (i = 0; i < numentries; i++) {
		de = (struct directory_entry_s *) advance_access(&b, sizeof(struct directory_entry_s));
		if (de == 0) {
			read_record(&b, blocks[n++]);
			de = (struct directory_entry_s *) ensure_access(&b, 0, sizeof(struct directory_entry_s));
		}

		printf("#%d:\n", i + 1);
		dumpmem((char *) de, 16);
		printf("\n");
		printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
		printf("byte_length %d\n", de->byte_length);
		printf("number_of_records %d\n", de->number_of_records);
		printf("record_0_address %d\n", de->record_0_address);
		printf("size %ld\n", sizeof(struct directory_entry_s));
		show_file(fd, de, de->record_0_address);
	}

	return 0;
}

int
lmfs_open(char *img_filename, int offset)
{
	int fd;
	int ret;
	unsigned char buffer[256 * 4];
	struct partition_label_s *pl;
	baccess b;

	fd = open(img_filename, O_RDONLY, 0666);
	if (fd < 0) {
		perror(img_filename);
		return -1;
	}

	ret = read(fd, buffer, 256 * 4);
	if (ret != 256 * 4) {
		perror(img_filename);
		return -1;
	}

	pl = (struct partition_label_s *) buffer;
	printf("version %d\n", pl->version);
	printf("name %s\n", pl->name);
	printf("primary_root %d\n", pl->disk_address.primary_root);
	printf("free_store_info %d\n", pl->disk_address.free_store_info);
	printf("bad_track_info %d\n", pl->disk_address.bad_track_info);
	printf("volume_table %d\n", pl->disk_address.volume_table);
	printf("\n");

	init_access(&b, fd);
	read_record(&b, pl->disk_address.primary_root);

	{
		struct file_header_s *fh;
		int i;
		int woffset;
		int wlast;

		fh = (struct file_header_s *) ensure_access(&b, 0, sizeof(struct file_header_s));
		printf("number_of_elements %d\n", fh->number_of_elements);

		for (i = 0; i < 8; i++) {
			char n[5];

			memcpy(n, fh->parts_array[i].name, 4);
			n[4] = 0;
			printf("%d: %s loc %d len %d\n", i, n, fh->parts_array[i].location, fh->parts_array[i].length);
			wlast = fh->parts_array[i].location;
		}

		for (i = 0; i < 8; i++) {
			woffset = fh->parts_array[i].location;

			if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
				struct file_map_s *fm;

				fm = (struct file_map_s *) ensure_access(&b, woffset * 4, sizeof(struct file_map_s));
				printf("allocated_length %d\n", fm->allocated_length);
				printf("valid_length %d\n", fm->valid_length);
				printf("link %d\n", fm->link);
				printf("element[0] %d\n", fm->element[0]);
			}
		}

		for (i = 0; i < 8; i++) {
			woffset = fh->parts_array[i].location;

			if (memcmp(fh->parts_array[i].name, "dire", 4) == 0) {
				struct directory_entry_s *de;
				int i;

				de = (struct directory_entry_s *) ensure_access(&b, woffset * 4, sizeof(struct directory_entry_s));

				printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
				printf("number_of_records %d\n", de->number_of_records);
				printf("record_0_address %d\n", de->record_0_address);

				struct dir_header_s *dh;
				woffset = wlast;
				dh = (struct dir_header_s *) ensure_access(&b, woffset * 4, sizeof(struct dir_header_s));

				printf("version %d, size %d, name '%s'\n", dh->version, dh->size, dh->name);
				printf("number_of_entries %d\n", dh->number_of_entries);
				printf("entries_index_offset %d\n", dh->entries_index_offset);
				printf("uid_path_offset %d\n", dh->uid_path_offset);
				printf("uid_path_length %d\n", dh->uid_path_length);
				for (i = 0; i < dh->number_of_entries; i++) {
					de = (struct directory_entry_s *) advance_access(&b, sizeof(struct directory_entry_s));
					printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
					printf("number_of_records %d\n", de->number_of_records);
					printf("record_0_address %d\n", de->record_0_address);
					printf("size %ld\n", sizeof(struct directory_entry_s));
					show_de(fd, de->record_0_address);
				}
			}
		}
	}

	close(fd);

	return 0;
}

int
lmfs_show_dir(char *path)
{
	return 0;
}

int
lmfs_read_file(char *path)
{
	return 0;
}

int
lmfs_write_file(char *path)
{
	return 0;
}

void
usage(void)
{
	fprintf(stderr, "usage: lmfs [OPTION]...\n");
	fprintf(stderr, "LMFS file extract\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -f FILE        LMFS image\n");
	fprintf(stderr, "  -d DIR         show files in directory\n");
	fprintf(stderr, "  -r FILE        read file\n");
	fprintf(stderr, "  -w FILE        write file\n");
	exit(1);
}

extern char *optarg;

int
main(int argc, char *argv[])
{
	int c;

	if (argc <= 1)
		usage();

	while ((c = getopt(argc, argv, "d:f:r:w:")) != -1) {
		switch (c) {
		case 'd':
			do_dir++;
			path = strdup(optarg);
			break;
		case 'f':
			img_filename = strdup(optarg);
			break;
		case 'r':
			do_read++;
			path = strdup(optarg);
			break;
		case 'w':
			do_write++;
			path = strdup(optarg);
			break;
		default:
			usage();
		}
	}

	if (img_filename == 0) {
		img_filename = strdup("FILE");
	}

	lmfs_open(img_filename, 0);

	if (do_dir) {
		lmfs_show_dir("");
		exit(0);
	}

	if (do_read) {
		lmfs_read_file(path);
		exit(0);
	}

	if (do_write) {
		lmfs_write_file(path);
		exit(0);
	}

	exit(0);
}
