/*
 * syms.c
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

struct sym_s {
	struct sym_s *next;
	char *name;
	unsigned int v;
	int mtype;
	int prom;
};

struct sym_s *syms;
int sym_count;
struct sym_s **sorted_syms;

extern int prom_enabled_flag;

int
sym_add(int memory, char *name, int v, int prom_sym)
{
	struct sym_s *s;

	if (0) printf("%d %s %o\n", memory, name, v);

	s = (struct sym_s *)malloc(sizeof(struct sym_s));
	if (s) {
		sym_count++;

		s->name = strdup(name);
		s->v = v;
		s->mtype = memory;
		s->prom = prom_sym;

		s->next = syms;
		syms = s;
	}

	return 0;
}

char *
sym_find_by_val(int memory, int v)
{
	struct sym_s *s;

	for (s = syms; s; s = s->next) {
		if (s->v == v && s->mtype == memory)
			return s->name;
	}

	return 0;
}


char *
sym_find_last(int memory, int v, int *poffset)
{
	int i;
	struct sym_s *s;

	if (sorted_syms == 0)
		return 0;

	for (i = 0; i < sym_count; i++) {

		s = sorted_syms[i];

		if (s->prom && !prom_enabled_flag)
			continue;

		if (s->prom == 0 && prom_enabled_flag)
			continue;

		if (s->mtype != memory)
			continue;

		if (v == s->v) {
			*poffset = 0;
			return s->name;
		}

		if (v < s->v || i == sym_count-1) {
			while (sorted_syms[i-1]->mtype != memory)
				i--;

			s = sorted_syms[i-1];
			*poffset = v - s->v;
			return s->name;
		}
	}

	return 0;
}

int
sym_find(int memory, char *name, int *pval)
{
	struct sym_s *s;

	if (memory != 1)
		return 0;

	for (s = syms; s; s = s->next) {
		if (strcasecmp(name, s->name) == 0) {
			*pval = s->v;
			return 0;
		}
	}

	return -1;
}

int 
sym_read_file(int prom_sym, char *filename)
{
	FILE *f;
	char line[8*1024];

	f = fopen(filename, "r");
	if (f) {
		int first = 1;

		fgets(line, sizeof(line), f);
		fgets(line, sizeof(line), f);
		fgets(line, sizeof(line), f);

		while (fgets(line, sizeof(line), f) != NULL) {
			char sym[64], symtype[64];
			int loc, n;

			if (first) {
				strcpy(line, line+3);
				first = 0;
			}

			if (0) printf("'%s'\n", line);

			n = sscanf(line, "%s %s %o", sym, symtype, &loc);
			if (n == 3) {
				n = 0;
				if (strcmp(symtype, "I-MEM") == 0) n = 1;
				if (strcmp(symtype, "D-MEM") == 0) n = 2;
				if (strcmp(symtype, "A-MEM") == 0) n = 4;
				if (strcmp(symtype, "M-MEM") == 0) n = 5;
				if (strcmp(symtype, "NUMBER") == 0) n = 6;

				if (n == 0) printf("? %s", symtype);

				sym_add(n, sym, loc, prom_sym);
			}
		}

		fclose(f);
	}
}

int sym_loc_compare(const void *p1, const void *p2)
{
	struct sym_s *s1 = *(struct sym_s **)p1;
	struct sym_s *s2 = *(struct sym_s **)p2;

	if (s1->v < s2->v)
		return -1;

	if (s1->v > s2->v)
		return 1;

	return 0;
}

int
sym_sort(void)
{
	struct sym_s *s;
	int i;

	/* make vector of ptrs to syms */
	sorted_syms = (struct sym_s **)malloc(sizeof(void *) * sym_count);
	if (sorted_syms == 0)
		return -1;

	/* fill in vector */
	i = 0;
	for (s = syms; s; s = s->next) {
		sorted_syms[i++] = s;
	}

	printf("sort %d symbols (originally %d)\n", i, sym_count);

	/* sort the vector */
	qsort((void *)sorted_syms, sym_count, sizeof(void *), sym_loc_compare);

#if 0
	for (i = 0; i < sym_count; i++) {
		printf("%s %o\n", sorted_syms[i]->name, sorted_syms[i]->v);
	}
#endif

	return 0;
}

int 
read_sym_files(void)
{
//	sym_read_file(1, "promh.sym.9");
//	sym_read_file(0, "ucadr.sym.979");
	sym_read_file(0, "ucadr.sym.841");
	sym_sort();
}

