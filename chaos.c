/*
 * chaosnet adapter emulation
 * $Id$
 */

/* TODO:
   - desynchronize network process from CPU
 */

#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/uio.h>

#include "ucode.h"

#ifndef CHAOS_MY_ADDRESS
# define CHAOS_MY_ADDRESS 0401
#endif

#ifndef CHAOS_DEBUG
# define CHAOS_DEBUG 1
#endif

#define CHAOS_DEBUG_PKT 0
//#define CHAOS_TOSS_IF_RXBUFF_FULL

#define CHAOS_BUF_SIZE_BYTES 4096
int chaos_csr;
int chaos_addr = CHAOS_MY_ADDRESS;
int chaos_bit_count;
int chaos_lost_count = 0;
unsigned short chaos_xmit_buffer[CHAOS_BUF_SIZE_BYTES/2];
int chaos_xmit_buffer_size;
int chaos_xmit_buffer_ptr;

unsigned short chaos_rcv_buffer[CHAOS_BUF_SIZE_BYTES/2];
unsigned short chaos_rcv_buffer_toss[CHAOS_BUF_SIZE_BYTES/2];
int chaos_rcv_buffer_ptr;
int chaos_rcv_buffer_size;
int chaos_rcv_buffer_empty;

int chaos_fd;
int chaos_need_reconnect;

int chaos_send_to_chaosd(char *buffer, int size);

/*
chaos csr 
	TIMER-INTERRUPT-ENABLE 1<<0
	LOOP-BACK 1<<1
	RECEIVE-ALL 1<<2
	RECEIVER-CLEAR 1<<3
	RECEIVE-ENABLE 1<<4
	TRANSMIT-ENABLE 1<<5
	INTERRUPT-ENABLES 3<<4
	TRANSMIT-ABORT 1<<6
	TRANSMIT-DONE 1<<7
	TRANSMITTER-CLEAR 1<<8
	LOST-COUNT 017<<9
	RESET 1<<13
	CRC-ERROR 1<<14
	RECEIVE-DONE 1<<15

;;; Offsets of other registers from CSR
;;; These are in words, not bytes

	MY-NUMBER-OFFSET 1
	WRITE-BUFFER-OFFSET 1
	READ-BUFFER-OFFSET 2
	BIT-COUNT-OFFSET 3
	START-TRANSMIT-OFFSET 5
*/
#define CHAOS_CSR_TIMER_INTERRUPT_ENABLE (1<<0)
#define	CHAOS_CSR_LOOP_BACK		(1<<1)
#define	CHAOS_CSR_RECEIVE_ALL		(1<<2)
#define	CHAOS_CSR_RECEIVER_CLEAR	(1<<3)
#define	CHAOS_CSR_RECEIVE_ENABLE	(1<<4)
#define	CHAOS_CSR_TRANSMIT_ENABLE	(1<<5)
#define	CHAOS_CSR_INTERRUPT_ENABLES	(3<<4)
#define	CHAOS_CSR_TRANSMIT_ABORT	(1<<6)
#define	CHAOS_CSR_TRANSMIT_DONE		(1<<7)
#define	CHAOS_CSR_TRANSMITTER_CLEAR	(1<<8)
#define	CHAOS_CSR_LOST_COUNT		(017<<9)
#define	CHAOS_CSR_RESET			(1<<13)
#define	CHAOS_CSR_CRC_ERROR		(1<<14)
#define	CHAOS_CSR_RECEIVE_DONE		(1<<15)

void
chaos_rx_pkt(void)
{
	chaos_rcv_buffer_ptr = 0;
	chaos_bit_count = (chaos_rcv_buffer_size * 2 * 8) - 1;
	if (chaos_rcv_buffer_size > 0) {
	  printf("chaos: set RDN, generate interrupt\n");
	  chaos_csr |= CHAOS_CSR_RECEIVE_DONE;
	  assert_unibus_interrupt(0404);
	}
}

void
char_xmit_done_intr(void)
{
	chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	assert_unibus_interrupt(0400);
}

void
chaos_xmit_pkt(void)
{
	int i, n;

#if CHAOS_DEBUG
	printf("chaos_xmit_pkt() %d bytes, data len %d\n",
	       chaos_xmit_buffer_ptr * 2,
	       (chaos_xmit_buffer_ptr > 0 ? chaos_xmit_buffer[1]&0x3f : -1));
#endif

#if CHAOS_DEBUG_PKT
	n = 0;
	for (i = 0; i < chaos_xmit_buffer_ptr; i++) {
		printf("%02x %02x ",
		       chaos_xmit_buffer[i] & 0xff,
		       (chaos_xmit_buffer[i] >> 8) & 0xff);
		n += 2;
		if (n > 16) {
			n = 0;
			printf("\n");
		}
	}
	if (n)
		printf("\n");
#endif

	chaos_xmit_buffer_size = chaos_xmit_buffer_ptr;

	/* dest */
	chaos_xmit_buffer[chaos_xmit_buffer_size++] = 0;
	/* source */
	chaos_xmit_buffer[chaos_xmit_buffer_size++] = 0;
	/* checksum (don't up size) */
	chaos_xmit_buffer[chaos_xmit_buffer_size] = 0;

	chaos_send_to_chaosd((char *)chaos_xmit_buffer,
			     chaos_xmit_buffer_size*2);

	chaos_xmit_buffer_ptr = 0;
	char_xmit_done_intr();

#if 0
	/* set back to ourselves - only for testing */
	chaos_rcv_buffer_size = chaos_xmit_buffer_size + 2;
	memcpy(chaos_rcv_buffer, chaos_xmit_buffer, chaos_xmit_buffer_size*2);

	chaos_rcv_buffer[chaos_xmit_buffer_size+0] = 0; /* source */
	chaos_rcv_buffer[chaos_xmit_buffer_size+1] = 0; /* checksum */

	chaos_rx_pkt();
#endif
}

int
chaos_get_bit_count(void)
{
  if (chaos_rcv_buffer_size > 0)
	return chaos_bit_count;
  else
    return 07777;
}

int
chaos_get_rcv_buffer(void)
{
	int v = 0;
	if (chaos_rcv_buffer_ptr < chaos_rcv_buffer_size) {
		v = chaos_rcv_buffer[chaos_rcv_buffer_ptr++];

		if (chaos_rcv_buffer_ptr == chaos_rcv_buffer_size)
			chaos_rcv_buffer_empty = 1;

	} else {
		/* read last word, clear receive done */
		chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
		chaos_rcv_buffer_size = 0;
	}
	return v;
}

int
chaos_put_xmit_buffer(int v)
{
	if (chaos_xmit_buffer_ptr < sizeof(chaos_xmit_buffer)/2)
		chaos_xmit_buffer[chaos_xmit_buffer_ptr++] = v;
	chaos_csr &= ~CHAOS_CSR_TRANSMIT_DONE;
}

int
chaos_get_csr(void)
{
	{
		static int old_chaos_csr = 0;
		if (chaos_csr != old_chaos_csr) {
			old_chaos_csr = chaos_csr;
			printf("unibus: chaos read csr %o\n",
			       chaos_csr);
		}
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
		printf(" Lost %d.",(csr & CHAOS_CSR_LOST_COUNT)>>9);

	csr &= ~(CHAOS_CSR_LOST_COUNT|CHAOS_CSR_RESET|
		 CHAOS_CSR_TRANSMITTER_CLEAR|CHAOS_CSR_TRANSMIT_ABORT|
		 CHAOS_CSR_RECEIVE_DONE|CHAOS_CSR_RECEIVE_ENABLE|
		 CHAOS_CSR_TRANSMIT_DONE|CHAOS_CSR_TRANSMIT_ENABLE|
		 CHAOS_CSR_CRC_ERROR|CHAOS_CSR_LOOP_BACK|
		 CHAOS_CSR_RECEIVE_ALL|CHAOS_CSR_RECEIVER_CLEAR);

	if (csr)
		printf(" unk bits 0%o",csr);
}

int
chaos_set_csr(int v)
{
	int mask, old_csr;

	v &= 0xffff;

 	/* Writing these don't stick */
	mask = CHAOS_CSR_TRANSMIT_DONE |
		CHAOS_CSR_LOST_COUNT |
		CHAOS_CSR_CRC_ERROR |
		CHAOS_CSR_RECEIVE_DONE |
	  	CHAOS_CSR_RECEIVER_CLEAR;

	printf("chaos: set csr bits 0%o (",v);
	print_csr_bits(v);
	printf ("), old 0%o ", chaos_csr);

	chaos_csr = (chaos_csr & mask) | (v & ~mask);

	if (chaos_csr & CHAOS_CSR_RESET) {
		printf("reset ");
		chaos_rcv_buffer_size = 0;
		chaos_xmit_buffer_ptr = 0;
		chaos_lost_count = 0;
		chaos_bit_count = 0;
		chaos_rcv_buffer_ptr = 0;
		chaos_csr &= ~(CHAOS_CSR_RESET | CHAOS_CSR_RECEIVE_DONE);
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	}

	if (v & CHAOS_CSR_RECEIVER_CLEAR) {
	  chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
	  chaos_lost_count = 0;
	  chaos_bit_count = 0;
	  chaos_rcv_buffer_ptr = 0;
	  chaos_rcv_buffer_size = 0;
	}

	if (v & CHAOS_CSR_TRANSMITTER_CLEAR) {
	  chaos_csr &= ~CHAOS_CSR_TRANSMIT_ABORT;
	  chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	  chaos_xmit_buffer_ptr = 0;
	}

	if (chaos_csr & CHAOS_CSR_RECEIVE_ENABLE) {
		printf("rx-enable ");

		if (chaos_rcv_buffer_empty) {
			chaos_rcv_buffer_ptr = 0;
			chaos_rcv_buffer_size = 0;
#if 0
			chaos_poll();
#endif
		}

		/* if buffer is full, generate status & interrupt again */
		if (chaos_rcv_buffer_size > 0)
			chaos_rx_pkt();
	}

	if (chaos_csr & CHAOS_CSR_TRANSMIT_ENABLE) {
		printf("tx-enable ");
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
#if 0
		char_xmit_done_intr();
	} else {
		chaos_csr &= ~CHAOS_CSR_TRANSMIT_DONE;
#endif
	}
	printf(" New csr 0%o", chaos_csr);
	print_csr_bits(chaos_csr);
	printf("\n");
}

#define UNIX_SOCKET_PATH	"/var/tmp/"
#define UNIX_SOCKET_CLIENT_NAME	"chaosd_"
#define UNIX_SOCKET_SERVER_NAME	"chaosd_server"
#define UNIX_SOCKET_PERM	S_IRWXU

static struct sockaddr_un unix_addr;

void
chaos_force_reconect(void)
{
	printf("chaos: forcing reconnect to chaosd\n");
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
			return;
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
			       "pkt size %d, buffer size %d\n",
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
		  chaos_rcv_buffer_size = (ret+1)/2;
		  chaos_rcv_buffer_empty = 0;
#if CHAOS_DEBUG
		  printf("rx to %o, my %o\n",
			 chaos_rcv_buffer[chaos_rcv_buffer_size-3],
			 chaos_addr);

//		printf("   from %o, crc %o\n",
//		       chaos_rcv_buffer[chaos_rcv_buffer_size-2],
//		       chaos_rcv_buffer[chaos_rcv_buffer_size-1]);
#endif

#if CHAOS_DEBUG_PKT
		  {
			  int i, c = 0, o = 0;
			  unsigned char cc, cb[9];
			  cb[8] = 0;
			  for (i = 0; i < ret; i++) {
				  if (c == 8) { printf("%s\n", cb); c = 0; }
				  if (c++ == 0) printf("%04d ", o);
				  cc = ((unsigned char *)chaos_rcv_buffer)[i];
				  printf("%02x ", cc);
				  cb[c-1] = (cc >= ' ' && cc <= '~') ?
					  cc : '.';
				  if (i == ret-1 && c > 0) {
					  for (; c < 8; c++) {
						  printf("xx "); cb[c]=' ';
					  }
					  printf("%s\n", cb);
					  break;
				  }
			  }
		  }
#endif

		  /* if not to us, ignore */
		  if (chaos_rcv_buffer[chaos_rcv_buffer_size-3] != chaos_addr) {
		    chaos_rcv_buffer_size = 0;
		    chaos_rcv_buffer_empty = 1;
		    return 0;
		  }

		  chaos_rx_pkt();
		}
	}

	return 0;
}

int
chaos_send_to_chaosd(char *buffer, int size)
{
	int ret, wcount;

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

#if CHAOS_DEBUG
	printf("chaos: -3 = %o, chaos_addr=%o, size %d, wcount %d\n", 
	       ((u_short *)buffer)[wcount-3], chaos_addr, size, wcount);
#endif

	/* recieve packets address to ourselves */
	if ( ((u_short *)buffer)[wcount-3] == chaos_addr) {
		memcpy(chaos_rcv_buffer, buffer, size);

		chaos_rcv_buffer_size = (size+1)/2;
		chaos_rcv_buffer_empty = 0;

		chaos_rx_pkt();
	}

	/* chaosd server */
	if (chaos_fd) {
		struct iovec iov[2];
		unsigned char lenbytes[4];

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
	len = strlen(unix_addr.sun_path) + sizeof(unix_addr.sun_family);

	unlink(unix_addr.sun_path);

	if ((bind(chaos_fd, (struct sockaddr *)&unix_addr, len) < 0)) {
		perror("bind(AF_UNIX)");
		return -1;
	}

	if (chmod(unix_addr.sun_path, UNIX_SOCKET_PERM) < 0) {
		perror("chmod(AF_UNIX)");
		return -1;
	}

//    sleep(1);
        
	memset(&unix_addr, 0, sizeof(unix_addr));
	sprintf(unix_addr.sun_path, "%s%s",
		UNIX_SOCKET_PATH, UNIX_SOCKET_SERVER_NAME);
	unix_addr.sun_family = AF_UNIX;
	len = strlen(unix_addr.sun_path) + sizeof(unix_addr.sun_family);

	if (connect(chaos_fd, (struct sockaddr *)&unix_addr, len) < 0) {
		printf("chaos: no chaosd server\n");
		return -1;
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

static int reconnect_delay;
static int reconnect_time;

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

	printf("chaos: reconnecting to chaosd\n");
	if (chaos_init() == 0) {
		printf("chaos: reconnected\n");
		chaos_need_reconnect = 0;
		reconnect_delay = 0;
	}

	return 0;
}
