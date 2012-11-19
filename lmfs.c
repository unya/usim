/*
 * hack to read/write files in a lmfs file partition
 */

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

void dumpmem(/*int dbg_level, */char *ptr, int len);

/* ---------------------------------------------------------------- */

typedef struct {
  char buffer[8192];

  char *base;
  int offset;
  int record_no;
  int fd;
} baccess;


void init_access(baccess *pb, int fd)
{
  pb->base = pb->buffer;
  pb->offset = 0;
  pb->record_no = -1;
  pb->fd = fd;
}

/*
  0    1023 :    0 1007
  1024 2047 : 1008 2015
  2048 3071 : 2016 3025
  3072 4095 : 3026 4033
*/
void *ensure_access(baccess *pb, int offset, int size)
{
  char *ptr;
  int blknum, boff, left, do_pad;

  blknum = offset / 1008;
  boff = offset % 1008;
  left = 1008 - boff;
  do_pad = 0;

  if (blknum > 3) {
    printf("ensure_access: past end!\n");
    exit(1);
  }

  if (left < size) {
    do_pad = 1;
    blknum++;
    boff = 0;
  }

  ptr = pb->base+(1024*blknum) + 8 + boff;

  pb->offset = offset;

  return ptr;
}

void *advance_access(baccess *pb, int size)
{
  char *ptr;
  int blknum, boff, left;

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

  ptr = pb->base+(1024*blknum) + 8 + boff;

  printf("offset %d\n", pb->offset);

  return ptr;
}

int
remaining_access(baccess *pb)
{
  int blknum, boff, left;

  blknum = pb->offset / 1008;
  boff = pb->offset % 1008;
  left = 1008 - boff;
  return left;
}

int
remaining_buffer(baccess *pb)
{
  return (1008*4) - pb->offset;
}

/* ---------------------------------------------------------------- */

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
  short location;	/* in words */
  short length;		/* in words */
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

#if 0
  struct {
    flag safety_switch;
    flag deleted;
    flag directory;
    flag link;
    flag migrated;
    flag read_only;
    flag incrementally_backed_up;
    flag completely_backed_up;
    flag properties_were_put;
    flag successfully_backed_up;
    flag binary_p;
    flag user_damaged;
    flag damaged_header_missing;
    flag damaged_header_malformed;
    flag damaged_records_missing;
    flag damaged_records_incongruous;
    flag damaged_records;
    flag dont_reap;
    flag create_open;
  } switches;
#endif
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

char tohex(char b)
{
    b = b & 0xf;
    if (b < 10) return '0' + b;
    return 'a' + (b - 10);
}

void
dumpmem(/*int dbg_level, */char *ptr, int len)
{
    char line[80], chars[80], *p, b, *c, *end;
    int /*i, */j, offset;

    end = ptr + len;
    offset = 0;
    while (ptr < end) {

	p = line;
	c = chars;
	//debugf(dbg_level, "%x ", ptr);
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
	//debugf(dbg_level, "%s %s\n", line, chars);
        printf("%s %s\n", line, chars);
	offset += 16;
    }
}

int
read_block(int fd, int block_no, unsigned char *buf)
{
	off_t offset, ret;
	int size;

	offset = block_no * (256*4);
	ret = lseek(fd, offset, SEEK_SET);

	if (ret != offset) {
		perror("lseek");
		return -1;
	}

	size = 256*4;
	ret = read(fd, buf, size);
	if (ret != size) {
		printf("disk read error; ret %d, size %d\n",
		       (int)ret, size);
		perror("read");
		return -1;
	}

	return 0;
}

int
write_block(int fd, int block_no, unsigned char *buf)
{
	off_t offset, ret;
	int size;

	offset = block_no * (256*4);
	//printf("block_no %d (%x), offset %lld\n", block_no, block_no, offset);

	ret = lseek(fd, offset, SEEK_SET);
	//printf("lseek ret %lld\n", ret);

	if (ret != offset) {
		perror("lseek");
		return -1;
	}

	size = 256*4;
	ret = write(fd, buf, size);
	if (ret != size) {
		printf("disk write error; ret %d, size %d\n",
		       (int)ret, size);
		perror("write");
		return -1;
	}

	return 0;
}

int
read_record(baccess *pb, int record_no)
{
  int i, block_no;
  char *pbuf;

  block_no = record_no * 4;
  pbuf = pb->buffer;
  pb->record_no = record_no;
  pb->offset = 0;

  for (i = 0; i < 4; i++) {
    read_block(pb->fd, block_no, pbuf);
    pbuf += 256*4;
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
  int i, woffset, wlast, bl, tot;
  struct file_header_s *fh;
  int n, num_blocks, blocks[64];
  char *p;

  init_access(&b, fd);
  read_record(&b, record_no);
  //dumpmem(b.buffer, 4096);

  fh = (struct file_header_s *)ensure_access(&b, 0, sizeof(struct file_header_s));

  printf("logical_size %d\n", fh->logical_size);
  printf("number_of_elements %d\n", fh->number_of_elements);
  for (i = 0; i < 8; i++) {
    char n[5];
    int woffset;

    memcpy(n, fh->parts_array[i].name, 4);
    n[4] = 0;
    printf("%d: %s loc %d len %d\n", i, n,
	   fh->parts_array[i].location, fh->parts_array[i].length);

    woffset = fh->parts_array[i].location;
  }

  for (i = 0; i < 8; i++) {

    woffset = fh->parts_array[i].location;
    wlast = fh->parts_array[i].location;

    if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
      struct file_map_s *fm;
      int j;

      fm = (struct file_map_s *)ensure_access(&b, woffset*4, sizeof(struct file_map_s));
      printf("allocated_length %d\n", fm->allocated_length);
      printf("valid_length %d\n", fm->valid_length);
      printf("link %d\n", fm->link);
      printf("element[0] %d\n", fm->element[0]);

      for (j = 0; j < fm->valid_length; j++)
	blocks[j] = fm->element[j];
      num_blocks = fm->valid_length;
    }

  }

  bl = de->byte_length;
printf("wlast %d\n", wlast);
  p = (char *)ensure_access(&b, wlast*4, 0);
  tot = 0;

    int fdd;

  {
    char nn[1024];
    sprintf(nn, "tmp/%s", de->file_name);
    unlink(nn);
    fdd = open(nn, O_RDWR|O_TRUNC|O_CREAT, 0666);
  }

  n = 0;

  while (1) {
    int left, use;

    left = remaining_access(&b);
    //printf("bl %d, remaining_access %d, remaining_buffer %d\n",
    //bl, left, remaining_buffer(&b));

    use = bl < left ? bl : left;

    tot += use;
    //printf("write %d, total %d\n", use, tot);
if (fdd > 0) write(fdd, p, use);

    bl -= use;
    left -= use;

    p = (char *)advance_access(&b, use);

    if (remaining_buffer(&b) == 0) {
printf("read record %d\n", blocks[n]);
      read_record(&b, blocks[n++]);
      p = (char *)ensure_access(&b, 0, 0);
      //dumpmem(b.buffer, 4096);
    }

    if (bl <= 0)
      break;
  }

  printf("done\n");
if (fdd > 0) close(fdd);

// exit(2);

  return 0;
}

int
show_de(int fd, int record_no)
{
  baccess b;
  struct directory_entry_s *de;
  int i, woffset, wlast, numentries;
  struct file_header_s *fh;
  int n, num_blocks, blocks[64];

  init_access(&b, fd);
  read_record(&b, record_no);

  printf("%d\n", record_no);

  fh = (struct file_header_s *)ensure_access(&b, 0, sizeof(struct file_header_s));

  printf("number_of_elements %d\n", fh->number_of_elements);
  for (i = 0; i < 8; i++) {
    char n[5];
    int woffset;

    memcpy(n, fh->parts_array[i].name, 4);
    n[4] = 0;
    printf("%d: %s loc %d len %d\n", i, n,
	   fh->parts_array[i].location, fh->parts_array[i].length);

    woffset = fh->parts_array[i].location;
  }

  for (i = 0; i < 8; i++) {

    woffset = fh->parts_array[i].location;
    wlast = fh->parts_array[i].location;

    if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
      struct file_map_s *fm;
      int j;

      fm = (struct file_map_s *)ensure_access(&b, woffset*4, sizeof(struct file_map_s));
      printf("allocated_length %d\n", fm->allocated_length);
      printf("valid_length %d\n", fm->valid_length);
      printf("link %d\n", fm->link);
      printf("element[0] %d\n", fm->element[0]);

      for (j = 0; j < fm->valid_length; j++)
	blocks[j] = fm->element[j];
      num_blocks = fm->valid_length;
    }

    if (memcmp(fh->parts_array[i].name, "dire", 4) == 0) {
      struct directory_entry_s *de;

      de = (struct directory_entry_s *)ensure_access(&b, woffset*4, sizeof(struct directory_entry_s));

      printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n",
	     de->file_name, de->file_type, de->bytesize,
	     de->author);
      printf("byte_length %d\n", de->byte_length);
      printf("number_of_records %d\n", de->number_of_records);
      printf("record_0_address %d\n", de->record_0_address);

    }
  }

  struct dir_header_s *dh;
  woffset = wlast;

  dh = (struct dir_header_s *)ensure_access(&b, woffset*4, sizeof(struct dir_header_s));

  printf("version %d, size %d, name '%s'\n",
	 dh->version, dh->size, dh->name);
  printf("number_of_entries %d\n", dh->number_of_entries);
  printf("entries_index_offset %d\n", dh->entries_index_offset);
  printf("uid_path_offset %d\n", dh->uid_path_offset);
  printf("uid_path_length %d\n", dh->uid_path_length);

  numentries = dh->number_of_entries;
  n = 0;

  for (i = 0; i < numentries; i++) {
    de = (struct directory_entry_s *)advance_access(&b, sizeof(struct directory_entry_s));

    if (de == 0) {
      read_record(&b, blocks[n++]);
      de = (struct directory_entry_s *)ensure_access(&b, 0, sizeof(struct directory_entry_s));
    }

    printf("#%d:\n", i+1);
    dumpmem((char *)de, 16); printf("\n");

    printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n",
	   de->file_name, de->file_type, de->bytesize,
	   de->author);
    printf("byte_length %d\n", de->byte_length);
    printf("number_of_records %d\n", de->number_of_records);
    printf("record_0_address %d\n", de->record_0_address);
    printf("size %d\n", sizeof(struct directory_entry_s));

    show_file(fd, de, de->record_0_address);
    //    show_de(fd, de->record_0_address);
  }
  return 0;
}

int
lmfs_open(char *img_filename, int offset)
{
  int fd, ret;
  unsigned char buffer[256*4];
  struct partition_label_s *pl;
  baccess b;

  fd = open(img_filename, O_RDONLY, 0666);
  if (fd < 0) {
    perror(img_filename);
    return -1;
  }

  ret = read(fd, buffer, 256*4);
  if (ret != 256*4) {
    perror(img_filename);
    return -1;
  }

  pl = (struct partition_label_s *)buffer;

  printf("version %d\n", pl->version);
  printf("name %s\n", pl->name);
  printf("primary_root %d\n", pl->disk_address.primary_root);
  printf("free_store_info %d\n", pl->disk_address.free_store_info);
  printf("bad_track_info %d\n", pl->disk_address.bad_track_info);
  printf("volume_table %d\n", pl->disk_address.volume_table);

  //  dumpmem(&pl->root_list, 8);
  //  dumpmem((char *)&pl->uid_generator, 64);
  printf("\n");

  init_access(&b, fd);
  read_record(&b, pl->disk_address.primary_root);

  {
    struct file_header_s *fh;
    int i;
    int woffset, wlast;

    fh = (struct file_header_s *)ensure_access(&b, 0, sizeof(struct file_header_s));

    printf("number_of_elements %d\n", fh->number_of_elements);
    for (i = 0; i < 8; i++) {
      char n[5];
      memcpy(n, fh->parts_array[i].name, 4);
      n[4] = 0;
      printf("%d: %s loc %d len %d\n", i, n,
	     fh->parts_array[i].location, fh->parts_array[i].length);

      wlast = fh->parts_array[i].location;
    }

    for (i = 0; i < 8; i++) {

      woffset = fh->parts_array[i].location;

      if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
	struct file_map_s *fm;

	fm = (struct file_map_s *)ensure_access(&b, woffset*4, sizeof(struct file_map_s));
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

	de = (struct directory_entry_s *)ensure_access(&b, woffset*4, sizeof(struct directory_entry_s));

	printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n",
	       de->file_name, de->file_type, de->bytesize,
	       de->author);
	printf("number_of_records %d\n", de->number_of_records);
	printf("record_0_address %d\n", de->record_0_address);

	struct dir_header_s *dh;
	//woffset = 252;
	woffset = wlast;

	dh = (struct dir_header_s *)ensure_access(&b, woffset*4, sizeof(struct dir_header_s));

	printf("version %d, size %d, name '%s'\n",
	       dh->version, dh->size, dh->name);
	printf("number_of_entries %d\n", dh->number_of_entries);
	printf("entries_index_offset %d\n", dh->entries_index_offset);
	printf("uid_path_offset %d\n", dh->uid_path_offset);
	printf("uid_path_length %d\n", dh->uid_path_length);

	woffset += sizeof(struct directory_entry_s) / 4;

	for (i = 0; i < dh->number_of_entries; i++) {
	  de = (struct directory_entry_s *)advance_access(&b, sizeof(struct directory_entry_s));

	  printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n",
		 de->file_name, de->file_type, de->bytesize,
		 de->author);
	  printf("number_of_records %d\n", de->number_of_records);
	  printf("record_0_address %d\n", de->record_0_address);
	  printf("size %d\n", sizeof(struct directory_entry_s));

	  show_de(fd, de->record_0_address);
	}
      }
    }
  }

  //  read_record(fd, pl->disk_address.free_store_info, rbuffer);
  //  dumpmem(rbuffer, 256);

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
	fprintf(stderr, "LMFS file extract\n");
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "-f <disk-image-filename>\n");
	fprintf(stderr, "-d <path>	show files in dir\n");
	fprintf(stderr, "-r <path>	read file\n");
	fprintf(stderr, "-w <path>	write file\n");

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
	  //	  img_filename = strdup("disk.img");
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



/*
 * Local Variables:
 * indent-tabs-mode:nil
 * c-basic-offset:4
 * End:
*/
