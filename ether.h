int ether_init(void);
void ether_poll(void);
void ether_xbus_reg_read(int offset, unsigned int *pv);
void ether_xbus_reg_write(int offset, unsigned int v);
void ether_xbus_desc_read(int offset, unsigned int *pv);
void ether_xbus_desc_write(int offset, unsigned int v);
