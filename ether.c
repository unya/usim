/*
 * ether.c
 *
 * simple CADR2 ethernet simulation, register definitions are for the
 * OpenCores ethernet controller.
 *
 * $Id$
 */

#include "usim.h"

#include <sys/types.h>
#include <sys/param.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#if defined(__linux__) || defined(OSX) || defined(BSD)
#include <sys/time.h>
#define uint8_t __uint8_t
#define uint16_t __uint16_t
#define uint32_t __uint32_t
#endif

#if defined(OSX)
#include <sys/socket.h>
#include <net/if.h>
#include <net/bpf.h>
#endif

#if defined(BSD) && !defined(OSX)
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_tap.h>
#include <net/if_ether.h>

int tap_fd;
#endif

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

uint32_t moder, int_source, int_mask, ipgt, ipgr1, ipgr2, packetlen;
uint32_t collconf, tx_bd_num, ctrlmoder, miimoder, miicommand;
uint32_t miiaddress, miitx_data, miirx_data, miistatus;
uint8_t eaddr[6];
uint32_t hash0, hash1, txctrl;
uint32_t packet[375];
int enabled;

extern void assert_xbus_interrupt(void);
extern int read_phy_mem(int paddr, unsigned int *pv);
extern int write_phy_mem(int paddr, unsigned int v);
void set_hwaddr(void);

int
ether_init(void)
{
#if defined(BSD) && !defined(OSX)
	int one = 1;
	struct ifreq ifr;
	int s;
#endif
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

#if defined(BSD) && !defined(OSX)
	if (geteuid() != 0) {
		printf("Not root, ethernet disabled\n");
		return -1;
	}

	if ((tap_fd = open("/dev/tap", O_RDWR)) < 0) {
		fprintf(stderr, "Couldn't open tap: %s\n", strerror(errno));
		return -1;
	}

	ioctl(tap_fd, FIONBIO, &one);
	enabled = 1;

	memset(&ifr, 0, sizeof(ifr));
	if (ioctl(tap_fd, TAPGIFNAME, &ifr) == -1) {
		fprintf(stderr, "Could not get interface name\n");
	}

	s = socket(PF_LINK, SOCK_DGRAM, 0);

	if (ioctl(s, SIOCGIFFLAGS, &ifr) == -1) {
		fprintf(stderr, "Could not get interface flags\n");
	}

	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifr.ifr_flags |= IFF_UP;

		if (ioctl(s, SIOCSIFFLAGS, &ifr) == -1) {
			fprintf(stderr, "Could not set IFF_UP\n");
		}
	}

	close(s);
#endif
	return 0;
}

void
ether_poll(void)
{
	int i, j, words;
	uint16_t status;
	uint32_t ptr;
	size_t len;
#if defined(BSD) && !defined(OSX)
	ssize_t ret;
#endif
    
	if (enabled) {

	    if (moder & ETHER_MODE_TXEN) {
		for (i = 0; i < tx_bd_num; i++) {
		    status = descs.desc_structs[i].status;
		    if (status & ETHER_DESC_TX_READY) {
			len = (size_t) descs.desc_structs[i].len;
			ptr = descs.desc_structs[i].ptr;
			words = (int)((len + 3) >> 2);
			for (j = 0; j < words; j++) {
			    read_phy_mem(ptr + j, &packet[j]);
			}
			if (status & ETHER_DESC_TX_PAD)
			    len = MAX(len, 60);

#if defined(BSD) && !defined(OSX)
			ret = write(tap_fd, packet, len);
			if (ret != len) {
			    perror("write"); 
			}
#endif

			status &= ~ETHER_DESC_TX_READY;
			descs.desc_structs[i].status = status;

			if (status & ETHER_DESC_TX_IRQ)
			    int_source |= ETHER_INT_TXB;    
		    }

		    if (status & ETHER_DESC_TX_WRAP) break;
		}
	    }

	    if (moder & ETHER_MODE_RXEN) {
		for (i = tx_bd_num; i < 0x80; i++) {
		    status = descs.desc_structs[i].status;
		    if (status & ETHER_DESC_RX_EMPTY) {
#if defined(BSD) && !defined(OSX)
			len = read(tap_fd, packet, sizeof(packet));
#else
			len = -1;
#endif
			if (len == -1) break;

			ptr = descs.desc_structs[i].ptr;
			descs.desc_structs[i].len = (uint16_t) len;
			words = (int)((len + 3) >> 2);
			for (j = 0; j < words; j++) {
			    write_phy_mem(ptr + j, packet[j]);
			}

			status &= ~ETHER_DESC_RX_EMPTY;
			descs.desc_structs[i].status = status;

			if (status & ETHER_DESC_RX_IRQ)
			    int_source |= ETHER_INT_RXB;    
		    }

		    if (status & ETHER_DESC_RX_WRAP) break;
		}
	    }
#if 0
	    if (int_source & int_mask)
		assert_xbus_interrupt();
#endif
	}
}

int
ether_xbus_reg_read(int offset, unsigned int *pv)
{
	/*printf("ether register read, offset %o\n", offset);*/

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
	return 0;
}

int
ether_xbus_reg_write(int offset, unsigned int v)
{
	/*printf("ether register write, offset %o, v %o\n", offset, v);*/
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
	return 0;
}

int
ether_xbus_desc_read(int offset, unsigned int *pv)
{

	/*printf("ether desc read, offset %o\n", offset);*/
	*pv = descs.desc_array[offset];
	return 0;
}

int
ether_xbus_desc_write(int offset, unsigned int v)
{

	/*printf("ether desc write, offset %o, v %o\n", offset, v);*/
	descs.desc_array[offset] = v;
	return 0;
}
