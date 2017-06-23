int read_prom_files(void);
int show_prom(void);

void disassemble_m_src(ucw_t u, int m_src);
void disassemble_dest(int dest);
void disassemble_ucode_loc(int loc, ucw_t u);
void disassemble_prom(void);

char *find_function_name(unsigned int the_lc);

void show_list(unsigned int lp);
