//
//  diskutil.h
//  usim
//
//  Created by Greg Gilley on 10/7/12.
//
//

#ifndef usim_diskutil_h
#define usim_diskutil_h

#define MAX_PARTITIONS	16

struct part_s {
	const char *name;
	unsigned int start;
	unsigned int size;
	int ptype;
    char label[17];
	const char *filename;
    
};

extern struct part_s parts[MAX_PARTITIONS];
extern unsigned int part_count;

extern const char *mcr_name;
extern const char *lod_name;

int extract_partition(const char *filename, const char *extract_filename, const char *partition_name);
int set_current_band(const char *filename, const char *partition_name);
int set_current_mcr(const char *filename, const char *partition_name);
int create_disk_image(const char *image_filename);
int read_labl(const char *filename);
void default_template(void);

#endif
