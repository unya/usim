struct sym_s {
	struct sym_s *next;
	char *name;
	unsigned int v;
	int mtype;
};

struct symtab_s {
	char *name;
	struct sym_s *syms;
	int sym_count;
	struct sym_s **sorted_syms;
};

int read_sym_files(void);

int sym_find(int mcr, char *name, int *pval);
char *sym_find_by_val(int mcr, int v);
char *sym_find_by_type_val(int mcr, int t, int v);
char *sym_find_last(int mcr, int v, int *poffset);

int _sym_read_file(struct symtab_s *tab, const char *filename);
int _sym_sort(struct symtab_s *tab);
