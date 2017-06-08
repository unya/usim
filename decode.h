int read_prom_files(void);
int show_prom(void);

char *disassemble_m_src(ucw_t u, int m_src);
char *disassemble_dest(int dest);
char *disassemble_ucode_loc(unsigned int loc, ucw_t u);
void disassemble_prom(void);

char *find_function_name(unsigned int the_lc);

void show_list(unsigned int lp);
