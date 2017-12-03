#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>

#include "usim.h"
#include "mem.h"

struct ether_desc {
	uint16_t status;
	uint16_t len;
	uint32_t ptr;
};

#define ETHER_DESC_TX_READY	(1 << 15)
#define ETHER_DESC_TX_IRQ	(1 << 14)
#define ETHER_DESC_TX_WRAP	(1 << 13)
#define ETHER_DESC_TX_PAD	(1 << 12)
#define ETHER_DESC_TX_CRC	(1 << 11)

#define ETHER_DESC_RX_EMPTY	(1 << 15)
#define ETHER_DESC_RX_IRQ	(1 << 14)
#define ETHER_DESC_RX_WRAP	(1 << 13)

#define ETHER_INT_RXC		(1 << 6)
#define ETHER_INT_TXC		(1 << 5)
#define ETHER_INT_BUSY		(1 << 4)
#define ETHER_INT_RXE		(1 << 3)
#define ETHER_INT_RXB		(1 << 2)
#define ETHER_INT_TXE		(1 << 1)
#define ETHER_INT_TXB		(1 << 0)

#define ETHER_MODE_RECSMALL	(1 << 16)
#define ETHER_MODE_PAD		(1 << 15)
#define ETHER_MODE_HUGEN	(1 << 14)
#define ETHER_MODE_CRCEN	(1 << 13)
#define ETHER_MODE_DLYCRCEN	(1 << 12)
#define ETHER_MODE_FULLD	(1 << 10)
#define ETHER_MODE_EXDFREN	(1 << 9)
#define ETHER_MODE_NOBCKOF	(1 << 8)
#define ETHER_MODE_LOOPBCK	(1 << 7)
#define ETHER_MODE_IFG		(1 << 6)
#define ETHER_MODE_PRO		(1 << 5)
#define ETHER_MODE_IAM		(1 << 4)
#define ETHER_MODE_BRO		(1 << 3)
#define ETHER_MODE_NOPRE	(1 << 2)
#define ETHER_MODE_TXEN		(1 << 1)
#define ETHER_MODE_RXEN		(1 << 0)

struct ether_descs {
	union {
		uint32_t _desc_array[256];
		struct ether_desc _desc_structs[128];
	} ed_u;
};

#define desc_array ed_u._desc_array
#define desc_structs ed_u._desc_structs

struct ether_descs descs;

uint32_t moder;
uint32_t int_source;
uint32_t int_mask;
uint32_t ipgt;
uint32_t ipgr1;
uint32_t ipgr2;
uint32_t packetlen;
uint32_t collconf;
uint32_t tx_bd_num;
uint32_t ctrlmoder;
uint32_t miimoder;
uint32_t miicommand;
uint32_t miiaddress;
uint32_t miitx_data;
uint32_t miirx_data;
uint32_t miistatus;
uint8_t eaddr[6];
uint32_t hash0;
uint32_t hash1;
uint32_t txctrl;
uint32_t packet[375];
int enabled;

int
ether_init(void)
{
	moder = 0x0000a000;
	int_source = 0x0;
	int_mask = 0x0;
	ipgt = 0x12;
	ipgr1 = 0x0c;
	ipgr2 = 0x12;
	packetlen = 0x00400600;
	collconf = 0x000f003f;
	tx_bd_num = 0x00000040;
	miimoder = 0x64;
	enabled = 0;
	
	return 0;
}

void
ether_poll(void)
{
	uint16_t status;
	
	if (!enabled)
		return;
	
	if (moder & ETHER_MODE_TXEN) {
		for (uint32_t i = 0; i < tx_bd_num; i++) {
			status = descs.desc_structs[i].status;
			if (status & ETHER_DESC_TX_READY) {
				size_t len;
				uint32_t ptr;
				int words;
				
				len = (size_t) descs.desc_structs[i].len;
				ptr = descs.desc_structs[i].ptr;
				words = (int)((len + 3) >> 2);
				
				for (int j = 0; j < words; j++)
					read_phy_mem(ptr + j, &packet[j]);
				
				if (status & ETHER_DESC_TX_PAD)
					len = MAX(len, 60);
				
				status &= ~ETHER_DESC_TX_READY;
				descs.desc_structs[i].status = status;
				
				if (status & ETHER_DESC_TX_IRQ)
					int_source |= ETHER_INT_TXB;
			}
			
			if (status & ETHER_DESC_TX_WRAP) break;
		}
	}
	
	if (moder & ETHER_MODE_RXEN) {
		for (uint32_t i = tx_bd_num; i < 0x80; i++) {
			status = descs.desc_structs[i].status;
			
			if (status & ETHER_DESC_RX_EMPTY)
				break;
			
			if (status & ETHER_DESC_RX_WRAP)
				break;
		}
	}
}

void
ether_xbus_reg_read(int offset, unsigned int *pv)
{
	switch (offset) {
	case 0: *pv = moder; break;
	case 1: *pv = int_source; break;
	case 2: *pv = int_mask; break;
	case 3: *pv = ipgt; break;
	case 4: *pv = ipgr1; break;
	case 5: *pv = ipgr2; break;
	case 6: *pv = packetlen; break;
	case 7: *pv = collconf; break;
	case 8: *pv = tx_bd_num; break;
	case 9: *pv = ctrlmoder; break;
	case 10: *pv = miimoder; break;
	case 11: *pv = miicommand; break;
	case 12: *pv = miiaddress; break;
	case 13: *pv = miitx_data; break;
	case 14: *pv = miirx_data; break;
	case 15: *pv = miistatus; break;
	case 16: *pv = ((eaddr[2] << 24) | (eaddr[3] << 16) |
			(eaddr[4] << 8) | eaddr[5]); break;
	case 17: *pv = ((eaddr[0] << 8) | eaddr[1]); break;
	case 18: *pv = hash0; break;
	case 19: *pv = hash1; break;
	case 20: *pv = txctrl; break;
	default: *pv = 0; break;
	}
}

void
ether_xbus_reg_write(int offset, unsigned int v)
{
	switch (offset) {
	case 0: moder = v; break;
	case 1: int_source ^= v; break;
	case 2: int_mask = v; break;
	case 3: ipgt = v; break;
	case 4: ipgr1 = v; break;
	case 5: ipgr2 = v; break;
	case 6: packetlen = v; break;
	case 7: collconf = v; break;
	case 8: tx_bd_num = v; break;
	case 9: ctrlmoder = v; break;
	case 10: miimoder = v; break;
	case 11: miicommand = v; break;
	case 12: miiaddress = v; break;
	case 13: miitx_data = v; break;
	case 16:
		eaddr[2] = (v >> 24) & 0xff;
		eaddr[3] = (v >> 16) & 0xff;
		eaddr[4] = (v >> 8) & 0xff;
		eaddr[5] = v & 0xff;
		break;
	case 17:
		eaddr[0] = (v >> 8) & 0xff;
		eaddr[1] = v & 0xff;
		break;
	case 18: hash0 = v; break;
	case 19: hash1 = v; break;
	case 20: txctrl = v; break;
	default: break;
	}
}

void
ether_xbus_desc_read(int offset, unsigned int *pv)
{
	*pv = descs.desc_array[offset];
}

void
ether_xbus_desc_write(int offset, unsigned int v)
{
	descs.desc_array[offset] = v;
}
