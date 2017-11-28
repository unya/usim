#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/uio.h>

#include "usim.h"
#include "ucode.h"
#include "chaos.h"

#ifndef CHAOS_MY_ADDRESS
#define CHAOS_MY_ADDRESS 0401
#endif

#define CHAOS_DEBUG_PKT 0

#define CHAOS_BUF_SIZE_BYTES 8192

int chaos_csr;
int chaos_addr = CHAOS_MY_ADDRESS;
int chaos_bit_count;
int chaos_lost_count = 0;
unsigned short chaos_xmit_buffer[CHAOS_BUF_SIZE_BYTES / 2];
int chaos_xmit_buffer_size;
int chaos_xmit_buffer_ptr;
unsigned short chaos_rcv_buffer[CHAOS_BUF_SIZE_BYTES / 2];
unsigned short chaos_rcv_buffer_toss[CHAOS_BUF_SIZE_BYTES / 2];
int chaos_rcv_buffer_ptr;
int chaos_rcv_buffer_size;
int chaos_rcv_buffer_empty;

int chaos_fd;
int chaos_need_reconnect;
int reconnect_delay;
static int reconnect_time;
void chaos_force_reconect(void);
int chaos_send_to_chaosd(char *buffer, int size);
int chaos_reconnect(void);

#define CHAOS_CSR_TIMER_INTERRUPT_ENABLE (1<<0)
#define CHAOS_CSR_LOOP_BACK (1<<1)
#define CHAOS_CSR_RECEIVE_ALL (1<<2)
#define CHAOS_CSR_RECEIVER_CLEAR (1<<3)
#define CHAOS_CSR_RECEIVE_ENABLE (1<<4)
#define CHAOS_CSR_TRANSMIT_ENABLE (1<<5)
#define CHAOS_CSR_INTERRUPT_ENABLES (3<<4)
#define CHAOS_CSR_TRANSMIT_ABORT (1<<6)
#define CHAOS_CSR_TRANSMIT_DONE (1<<7)
#define CHAOS_CSR_TRANSMITTER_CLEAR (1<<8)
#define CHAOS_CSR_LOST_COUNT (017<<9)
#define CHAOS_CSR_RESET (1<<13)
#define CHAOS_CSR_CRC_ERROR (1<<14)
#define CHAOS_CSR_RECEIVE_DONE (1<<15)

// RFC1071: Compute Internet Checksum for COUNT bytes beginning at
// location ADDR.
unsigned short
ch_checksum(const unsigned char *addr, int count)
{
	long sum = 0;
	
	while (count > 1) {
		sum += *(addr) << 8 | *(addr + 1);
		addr += 2;
		count -= 2;
	}
	
// Add left-over byte, if any.
	if (count > 0)
		sum += *(unsigned char *) addr;
	
// Fold 32-bit sum to 16 bits.
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	
	return (~sum) & 0xffff;
}

void
chaos_rx_pkt(void)
{
	chaos_rcv_buffer_ptr = 0;
	chaos_bit_count = (chaos_rcv_buffer_size * 2 * 8) - 1;
	if (chaos_rcv_buffer_size > 0) {
		tracenet("chaos: set RDN, generate interrupt\n");
		chaos_csr |= CHAOS_CSR_RECEIVE_DONE;
		if (chaos_csr & CHAOS_CSR_RECEIVE_ENABLE)
			assert_unibus_interrupt(0270);
	} else
		tracenet("chaos_rx_pkt: called, but no data in buffer\n");
}

void
char_xmit_done_intr(void)
{
	chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	if (chaos_csr & CHAOS_CSR_TRANSMIT_ENABLE)
		assert_unibus_interrupt(0270);
}

#if CHAOS_DEBUG_PKT
char *opcodetable[256] = {
	"UNKNOWN",
	"RFC - Request for Connection",
	"OPN - Open Connection",
	"CLS - Close Connection",
	"FWD - Forward a Request for Connection",
	"ANS - Answer a Simple Transaction",
	"SNS - Sense Status",
	"STS - Status",
	"RUT - Routing Information",
	"LOS - Lossage",
	"LSN - Listen",
	"MNT - Maintenance",
	"EOF",
	"UNC - Uncontrolled Data",
	"BRD - Broadcast",
	"UNKNOWN",
};

void
dumpbuffer(unsigned short *buffer, int size)
{
	int j;
	int offset;
	int skipping;
	char cbuf[17];
	char line[80];
	unsigned char *buf = (unsigned char *) &buffer[8];
	int cnt = (buffer[1] & 0x0fff);
	int opcode = (buffer[0] & 0xff00) >> 8;

	size = size - (int) (8 * sizeof(short));	// Subtract off the size of packet header.
	if (size < cnt)
		printf("ERROR: packet size mismatch: size %d < cnt %d\n", size, cnt);
	else if (size > cnt) {
		printf("extra data: %d bytes\n", size - cnt);
		cnt = size;
	}

	printf("opcode: %d %s\n", opcode, opcode == 128 ? "DAT" : opcode == 129 ? "SYN" : opcode == 192 ? "DWD" : opcodetable[opcode]);
	if ((buffer[0] & 0x00ff) > 0)
		printf("version: %d\n", (buffer[0] & 0x00ff));
	if (((buffer[1] & 0xf000) >> 12) > 0)
		printf("forwarding count: %d\n", (buffer[1] & 0xf000) >> 12);
	printf("data length: %d\n", (buffer[1] & 0x0fff));
	printf("destination address: %o index: %04x\n", buffer[2], buffer[3]);
	printf("source address: %o index: %04x\n", buffer[4], buffer[5]);
	printf("packet number: %d\n", buffer[6]);
	printf("acknowledgement: %d\n", buffer[7]);

	offset = 0;
	skipping = 0;
	while (cnt > 0) {
		if (offset > 0 && memcmp(buf, buf - 16, 16) == 0) {
			skipping = 1;
		} else {
			if (skipping) {
				skipping = 0;
				printf("...\n");
			}
		}

		if (!skipping) {
			for (j = 0; j < 16; j++) {
				char *pl = line + j * 3;

				if (j >= cnt) {
					strcpy(pl, "xx ");
					cbuf[j] = 'x';
				} else {
					sprintf(pl, "%02x ", buf[j]);
					cbuf[j] = buf[j] < ' ' || buf[j] > '~' ? '.' : (char) buf[j];
				}
				pl[3] = 0;
			}
			cbuf[16] = 0;

			printf("%08x %s %s\n", offset, line, cbuf);
		}

		buf += 16;
		cnt -= 16;
		offset += 16;
	}

	if (skipping) {
		printf("%08x ...\n", offset - 16);
	}
}
#endif

void
chaos_xmit_pkt(void)
{
	tracenet("chaos_xmit_pkt() %d bytes, data len %d\n", chaos_xmit_buffer_ptr * 2, (chaos_xmit_buffer_ptr > 0 ? chaos_xmit_buffer[1] & 0x3f : -1));

	chaos_xmit_buffer_size = chaos_xmit_buffer_ptr;

#if CHAOS_DEBUG_PKT
	dumpbuffer(chaos_xmit_buffer, chaos_xmit_buffer_size * 2);
#endif

	// Dest is already in the buffer.

	chaos_xmit_buffer[chaos_xmit_buffer_size++] = (unsigned short) chaos_addr;	// Source.
	chaos_xmit_buffer[chaos_xmit_buffer_size] = ch_checksum((unsigned char *) chaos_xmit_buffer, chaos_xmit_buffer_size * 2);	// Checksum.
	chaos_xmit_buffer_size++;

	chaos_send_to_chaosd((char *) chaos_xmit_buffer, chaos_xmit_buffer_size * 2);

	chaos_xmit_buffer_ptr = 0;
	char_xmit_done_intr();
}

int
chaos_get_bit_count(void)
{
	if (chaos_rcv_buffer_size > 0)
		return chaos_bit_count;
	else {
		tracenet("chaos_get_bit_count: returned empty count\n");
		return 07777;
	}
}

int
chaos_get_rcv_buffer(void)
{
	int v = 0;

	if (chaos_rcv_buffer_ptr < chaos_rcv_buffer_size) {
		v = chaos_rcv_buffer[chaos_rcv_buffer_ptr++];
		if (chaos_rcv_buffer_ptr == chaos_rcv_buffer_size) {
			chaos_rcv_buffer_empty = 1;
			tracenet("chaos_get_rcv_buffer: marked buffer as empty\n");
		}
	} else {
		// Read last word, clear receive done.
		chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
		chaos_rcv_buffer_size = 0;
		tracenet("chaos_get_rcv_buffer: cleared CHAOS_CSR_RECEIVE_DONE\n");
	}

	return v;
}

void
chaos_put_xmit_buffer(int v)
{
	if (chaos_xmit_buffer_ptr < (int) sizeof(chaos_xmit_buffer) / 2)
		chaos_xmit_buffer[chaos_xmit_buffer_ptr++] = (unsigned short) v;
	chaos_csr &= ~CHAOS_CSR_TRANSMIT_DONE;
}

int
chaos_get_csr(void)
{
	static int old_chaos_csr = 0;

	if (chaos_csr != old_chaos_csr) {
		old_chaos_csr = chaos_csr;
		tracenet("unibus: chaos read csr %o\n", chaos_csr);
	}

	return chaos_csr | ((chaos_lost_count << 9) & 017);
}

int
chaos_get_addr(void)
{
	return chaos_addr;
}

void
print_csr_bits(int csr)
{
	if (csr & CHAOS_CSR_LOOP_BACK)
		printf(" LUP");
	if (csr & CHAOS_CSR_RECEIVE_ALL)
		printf(" SPY");
	if (csr & CHAOS_CSR_RECEIVER_CLEAR)
		printf(" RCL");
	if (csr & CHAOS_CSR_RECEIVE_ENABLE)
		printf(" REN");
	if (csr & CHAOS_CSR_TRANSMIT_ENABLE)
		printf(" TEN");
	if (csr & CHAOS_CSR_TRANSMIT_ABORT)
		printf(" TAB");
	if (csr & CHAOS_CSR_TRANSMIT_DONE)
		printf(" TDN");
	if (csr & CHAOS_CSR_TRANSMITTER_CLEAR)
		printf(" TCL");
	if (csr & CHAOS_CSR_RESET)
		printf(" RST");
	if (csr & CHAOS_CSR_RECEIVE_DONE)
		printf(" RDN");
	if (csr & CHAOS_CSR_CRC_ERROR)
		printf(" ERR");
	if (csr & CHAOS_CSR_LOST_COUNT)
		printf(" Lost %d.", (csr & CHAOS_CSR_LOST_COUNT) >> 9);
	csr &= ~(CHAOS_CSR_LOST_COUNT |
		 CHAOS_CSR_RESET |
		 CHAOS_CSR_TRANSMITTER_CLEAR | CHAOS_CSR_TRANSMIT_ABORT |
		 CHAOS_CSR_RECEIVE_DONE | CHAOS_CSR_RECEIVE_ENABLE |
		 CHAOS_CSR_TRANSMIT_DONE | CHAOS_CSR_TRANSMIT_ENABLE |
		 CHAOS_CSR_CRC_ERROR |
		 CHAOS_CSR_LOOP_BACK |
		 CHAOS_CSR_RECEIVE_ALL | CHAOS_CSR_RECEIVER_CLEAR);
	if (csr)
		printf(" unk bits 0%o", csr);
}

int
chaos_set_csr(int v)
{
	int mask;

	v &= 0xffff;
	// Writing these don't stick.
	mask =
		CHAOS_CSR_TRANSMIT_DONE |
		CHAOS_CSR_LOST_COUNT |
		CHAOS_CSR_CRC_ERROR |
		CHAOS_CSR_RECEIVE_DONE | CHAOS_CSR_RECEIVER_CLEAR;

	tracenet("chaos: set csr bits 0%o, old 0%o\n", v, chaos_csr);

	chaos_csr = (chaos_csr & mask) | (v & ~mask);
	if (chaos_csr & CHAOS_CSR_RESET) {
		tracenet("reset ");
		chaos_rcv_buffer_size = 0;
		chaos_xmit_buffer_ptr = 0;
		chaos_lost_count = 0;
		chaos_bit_count = 0;
		chaos_rcv_buffer_ptr = 0;
		chaos_csr &= ~(CHAOS_CSR_RESET | CHAOS_CSR_RECEIVE_DONE);
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
		reconnect_delay = 200;	// Do it right away.
		chaos_force_reconect();
	}

	if (v & CHAOS_CSR_RECEIVER_CLEAR) {
		chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
		chaos_lost_count = 0;
		chaos_bit_count = 0;
		chaos_rcv_buffer_ptr = 0;
		chaos_rcv_buffer_size = 0;
	}

	if (v & (CHAOS_CSR_TRANSMITTER_CLEAR | CHAOS_CSR_TRANSMIT_DONE)) {
		chaos_csr &= ~CHAOS_CSR_TRANSMIT_ABORT;
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
		chaos_xmit_buffer_ptr = 0;
	}

	if (chaos_csr & CHAOS_CSR_RECEIVE_ENABLE) {
		tracenet("rx-enable ");
		if (chaos_rcv_buffer_empty) {
			chaos_rcv_buffer_ptr = 0;
			chaos_rcv_buffer_size = 0;
		}
		// If buffer is full, generate status and interrupt again.
		if (chaos_rcv_buffer_size > 0) {
			tracenet("\n rx-enabled and buffer is full\n");
			chaos_rx_pkt();
		}
	}

	if (chaos_csr & CHAOS_CSR_TRANSMIT_ENABLE) {
		tracenet("tx-enable ");
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	}

	tracenet(" New csr 0%o\n", chaos_csr);

	return 0;
}

#define UNIX_SOCKET_PATH	"/var/tmp/"
#define UNIX_SOCKET_CLIENT_NAME	"chaosd_"
#define UNIX_SOCKET_SERVER_NAME	"chaosd_server"
#define UNIX_SOCKET_PERM	S_IRWXU

static struct sockaddr_un unix_addr;

chaos_packet *
chaos_allocate_packet(chaos_connection *conn, int opcode, ssize_t len)
{
    return 0;
}

void
chaos_queue(chaos_packet *packet)
{
}

chaos_packet *chaos_connection_dequeue(chaos_connection *conn)
{
    return 0;
}

chaos_connection *
chaos_open_connection(int co_host, char *contact, int mode, int async, int rwsize)
{
    return 0;
}

int chaos_connection_queue(chaos_connection *conn, chaos_packet *packet)
{    
    return 0;
}

void
chaos_delete_connection(chaos_connection *conn)
{
}

void chaos_interrupt_connection(chaos_connection *conn)
{
}

void
chaos_force_reconect(void)
{
#if CHAOS_DEBUG || 1
	printf("chaos: forcing reconnect to chaosd\n");
#endif
	close(chaos_fd);
	chaos_fd = 0;
	chaos_need_reconnect = 1;
}

int
chaos_poll(void)
{
	int ret;
	struct pollfd pfd[1];
	int nfds, timeout;

	if (chaos_need_reconnect) {
		chaos_reconnect();
	}

	if (chaos_fd == 0) {
		return 0;
	}

	timeout = 0;
	nfds = 1;
	pfd[0].fd = chaos_fd;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	ret = poll(pfd, nfds, timeout);
	if (ret < 0) {
#if CHAOS_DEBUG
		printf("chaos: Polling, nothing there (RDN=%o)\n",
		       chaos_csr & CHAOS_CSR_RECEIVE_DONE);
#endif
		chaos_need_reconnect = 1;
		return -1;
	}

	if (ret > 0) {
		u_char lenbytes[4];
		int len;

		/* is rx buffer full? */
		if (!chaos_rcv_buffer_empty &&
		    (chaos_csr & CHAOS_CSR_RECEIVE_DONE))
		{
#ifndef CHAOS_TOSS_IF_RXBUFF_FULL
			printf("chaos: polling, but unread data exists\n");
			return 0;
#else
			/*
			 * Toss packets arriving when buffer is already in use
			 * they will be resent
			 */
#if CHAOS_DEBUG
			printf("chaos: polling, unread data, drop "
			       "(RDN=%o, lost %d)\n",
			       chaos_csr & CHAOS_CSR_RECEIVE_DONE,
			       chaos_lost_count);
#endif
			chaos_lost_count++;
			read(chaos_fd, lenbytes, 4);
			len = (lenbytes[0] << 8) | lenbytes[1];
#if CHAOS_DEBUG
			printf("chaos: tossing packet of %d bytes\n", len);
#endif
			if (len > sizeof(chaos_rcv_buffer_toss)) {
				printf("chaos packet won't fit");
				chaos_force_reconect();
				return -1;
			}

			/* toss it */
			read(chaos_fd, (char *)chaos_rcv_buffer_toss, len);
			return -1;
#endif
		}

		/* read header from chaosd */
		ret = read(chaos_fd, lenbytes, 4);
		if (ret <= 0) {
			perror("chaos: header read error");
			chaos_force_reconect();
			return -1;
		}

		len = (lenbytes[0] << 8) | lenbytes[1];

		if (len > sizeof(chaos_rcv_buffer)) {
			printf("chaos: packet too big: "
			       "pkt size %d, buffer size %lu\n",
			       len, sizeof(chaos_rcv_buffer));

			/* When we get out of synch break socket conn */
			chaos_force_reconect();
			return -1;
		}

		ret = read(chaos_fd, (char *)chaos_rcv_buffer, len);
		if (ret < 0) {
			perror("chaos: read");
			chaos_force_reconect();
			return -1;
		}

#if CHAOS_DEBUG
		printf("chaos: polling; got chaosd packet %d\n", ret);
#endif

		if (ret > 0) {
		  int dest_addr;

		  chaos_rcv_buffer_size = (ret+1)/2;
		  chaos_rcv_buffer_empty = 0;

#if __BIG_ENDIAN__
		  /* flip shorts to host order */
		  int w;
		  for (w = 0; w < chaos_rcv_buffer_size; w++) {
		    chaos_rcv_buffer[w] = SWAP_SHORT(chaos_rcv_buffer[w]);
		  }
#endif		  

		  dest_addr = chaos_rcv_buffer[chaos_rcv_buffer_size-3];

		  /* if not to us, ignore */
		  if (dest_addr != chaos_addr) {
		    chaos_rcv_buffer_size = 0;
		    chaos_rcv_buffer_empty = 1;
		    return 0;
		  }

#if CHAOS_DEBUG
            printf("chaos rx: to %o, my %o\n", dest_addr, chaos_addr);
#endif
            
#if CHAOS_DEBUG_PKT
            dumpbuffer(chaos_rcv_buffer, chaos_rcv_buffer_size * 2);
#endif
            
		  chaos_rx_pkt();
		}
	}

	return 0;
}

int
chaos_send_to_chaosd(char *buffer, int size)
{
	int ret, wcount, dest_addr;

	/* local loopback */
	if (chaos_csr & CHAOS_CSR_LOOP_BACK) {

		printf("chaos: loopback %d bytes\n", size);
		memcpy(chaos_rcv_buffer, buffer, size);

		chaos_rcv_buffer_size = (size+1)/2;
		chaos_rcv_buffer_empty = 0;

		chaos_rx_pkt();

		return 0;
	}

	wcount = (size+1)/2;
	dest_addr = ((u_short *)buffer)[wcount-3];

	//printf("chaos_send_to_chaosd() dest_addr %o\n", dest_addr);

#if __BIG_ENDIAN__
	/* flip host order to network order */
	int w;
	for (w = 0; w < wcount; w++) {
		u_short *ps = &((u_short *)buffer)[w];
		*ps = SWAP_SHORT(*ps);
	}
#endif

#if CHAOS_DEBUG
	printf("chaos tx: dest_addr = %o, chaos_addr=%o, size %d, wcount %d\n", 
	       dest_addr, chaos_addr, size, wcount);
#endif

	/* recieve packets address to ourselves */
	if (dest_addr == chaos_addr) {
		memcpy(chaos_rcv_buffer, buffer, size);

		chaos_rcv_buffer_size = (size+1)/2;
		chaos_rcv_buffer_empty = 0;

		chaos_rx_pkt();
	}

	/* chaosd server */
	if (chaos_fd) {
		struct iovec iov[2];
		unsigned char lenbytes[4];
        int ret;
        
		lenbytes[0] = size >> 8;
		lenbytes[1] = size;
		lenbytes[2] = 1;
		lenbytes[3] = 0;
        
		iov[0].iov_base = lenbytes;
		iov[0].iov_len = 4;
        
		iov[1].iov_base = buffer;
		iov[1].iov_len = size;
        
		ret = writev(chaos_fd, iov, 2);
		if (ret < 0) {
			perror("chaos write");
			return -1;
		}
	}
    
	return 0;
}


/*
 * connect to server using specificed socket type
 */
static int
chaos_connect_to_server(void)
{
	int len;

	if (0) printf("connect_to_server()\n");

	if ((chaos_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket(AF_UNIX)");
		chaos_fd = 0;
		return -1;
	}

	memset(&unix_addr, 0, sizeof(unix_addr));

	sprintf(unix_addr.sun_path, "%s%s%05u",
		UNIX_SOCKET_PATH, UNIX_SOCKET_CLIENT_NAME, getpid());

	unix_addr.sun_family = AF_UNIX;
	len = SUN_LEN(&unix_addr);

	unlink(unix_addr.sun_path);

	if ((bind(chaos_fd, (struct sockaddr *)&unix_addr, len) < 0)) {
		perror("bind(AF_UNIX)");
		return -1;
	}

	if (chmod(unix_addr.sun_path, UNIX_SOCKET_PERM) < 0) {
		perror("chmod(AF_UNIX)");
		return -1;
	}

	memset(&unix_addr, 0, sizeof(unix_addr));
	sprintf(unix_addr.sun_path, "%s%s",
		UNIX_SOCKET_PATH, UNIX_SOCKET_SERVER_NAME);
	unix_addr.sun_family = AF_UNIX;
	len = SUN_LEN(&unix_addr);

	if (connect(chaos_fd, (struct sockaddr *)&unix_addr, len) < 0) {
		printf("chaos: no chaosd server\n");
		return -1;
	}

    socklen_t value = 0;
    socklen_t length = sizeof(value);
    
    if (getsockopt(chaos_fd, SOL_SOCKET, SO_RCVBUF, &value, &length) == 0)
    {
        value = value * 4;
        if (setsockopt(chaos_fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)) != 0)
            printf("setsockopt(SO_RCVBUF) failed\n");
    }

	if (0) printf("chaos_fd %d\n", chaos_fd);
        
	return 0;
}

int
chaos_init(void)
{
	if (chaos_connect_to_server()) {
		close(chaos_fd);
		chaos_fd = 0;
		return -1;
	}

	chaos_rcv_buffer_empty = 1;

	return 0;
}

int
chaos_reconnect(void)
{
	/* */
	if (++reconnect_delay < 200) {
		return 0;
	}
	reconnect_delay = 0;

	/* try every 5 seconds */
	if (reconnect_time &&
	    time(NULL) < (reconnect_time + 5))
	{
		return 0;
	}
	reconnect_time = time(NULL);

# if CHAOS_DEBUG || 1
	printf("chaos: reconnecting to chaosd\n");
#endif
	if (chaos_init() == 0) {
		printf("chaos: reconnected\n");
		chaos_need_reconnect = 0;
		reconnect_delay = 0;
	}

	return 0;
}
