/*
 * chaosnet adapter emulation
 * $Id$
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

int chaos_csr;
int chaos_addr = 0401;
int chaos_bit_count;
unsigned short chaos_xmit_buffer[1600/2];
int chaos_xmit_buffer_size;
int chaos_xmit_buffer_ptr;

unsigned short chaos_rcv_buffer[1600/2];
int chaos_rcv_buffer_ptr;
int chaos_rcv_buffer_size;
int chaos_rcv_buffer_empty;

int chaos_fd;

int chaos_sent_to_chaosd(char *buffer, int size);

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
	chaos_csr |= CHAOS_CSR_RECEIVE_DONE;
	assert_unibus_interrupt(0404);
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

	printf("chaos_xmit_pkt() %d bytes\n", chaos_xmit_buffer_ptr * 2);
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

	chaos_xmit_buffer_size = chaos_xmit_buffer_ptr;
	chaos_xmit_buffer_ptr = 0;

	chaos_xmit_buffer[chaos_xmit_buffer_size++] = chaos_addr; /* source */
	chaos_xmit_buffer[chaos_xmit_buffer_size++] = 0; /* checksum */

	char_xmit_done_intr();

	chaos_sent_to_chaosd((char *)chaos_xmit_buffer,
			     chaos_xmit_buffer_size*2);

#if 0
	/* set back to ourselves - only for testing */
	chaos_rcv_buffer_size = chaos_xmit_buffer_size + 2;
	memcpy(chaos_rcv_buffer, chaos_xmit_buffer, chaos_xmit_buffer_size*2);

	chaos_rcv_buffer[chaos_xmit_buffer_size+0] = 0; /* source */
	chaos_rcv_buffer[chaos_xmit_buffer_size+1] = 0; /* checksum */

	chaos_rx_pkt();
#endif
}

void
chaos_fake_rx(void)
{
	static int done = 0;

	printf("chaos_fake_rx() done %d\n", done);

	if (done < 5) {
		done++;
		chaos_rcv_buffer_size = 0;

		printf("sending fake time packet\n");
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 5; /* op = ANS */
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 16+8; /* count*/
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = ('T' << 8) | 'I';
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = ('M' << 8) | 'E';
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0;

		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0401; /* dest */
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0; /* source */
		chaos_rcv_buffer[chaos_rcv_buffer_size++] = 0; /* checksum */

		chaos_rx_pkt();
	}
}

int
chaos_get_bit_count(void)
{
	return chaos_bit_count;
}

int
chaos_get_rcv_buffer(void)
{
	int v = 0;
	if (chaos_rcv_buffer_ptr < chaos_rcv_buffer_size) {
		v = chaos_rcv_buffer[chaos_rcv_buffer_ptr++];

		if (chaos_rcv_buffer_ptr == chaos_rcv_buffer_size)
			chaos_rcv_buffer_empty = 1;

	}
	chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
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

	return chaos_csr;
}

int
chaos_get_addr(void)
{
	return chaos_addr;
}

int
chaos_set_csr(int v)
{
	int mask, old_csr;
	int send_fake;

	v &= 0xffff;

	mask = CHAOS_CSR_TRANSMIT_DONE |
		CHAOS_CSR_LOST_COUNT |
		CHAOS_CSR_CRC_ERROR |
		CHAOS_CSR_RECEIVE_DONE;

	old_csr = chaos_csr;

	chaos_csr = (chaos_csr & mask) | (v & ~mask);

	send_fake = 0;

	printf("chaos: ");
	if (chaos_csr & CHAOS_CSR_RESET) {
		printf("reset ");
		chaos_xmit_buffer_ptr = 0;
		chaos_csr &= ~CHAOS_CSR_RESET;
	}
	if (chaos_csr & CHAOS_CSR_RECEIVE_ENABLE) {
		printf("rx-enable ");

		if (chaos_rcv_buffer_empty) {
//			send_fake = 1;
			chaos_rcv_buffer_ptr = 0;
			chaos_rcv_buffer_size = 0;
#if 1
			chaos_poll_from_chaosd();
		}
#endif
	}
	if (chaos_csr & CHAOS_CSR_TRANSMIT_ENABLE) {
		printf("tx-enable ");
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
char_xmit_done_intr();
	} else {
		chaos_csr &= ~CHAOS_CSR_TRANSMIT_DONE;
	}
	printf("\n");

	if (send_fake) {
		chaos_fake_rx();
	}
}

#define UNIX_SOCKET_PATH	"/var/tmp/"
#define UNIX_SOCKET_CLIENT_NAME	"chaosd_"
#define UNIX_SOCKET_SERVER_NAME	"chaosd_server"
#define UNIX_SOCKET_PERM	S_IRWXU

static struct sockaddr_un unix_addr;

int
chaos_poll_from_chaosd(void)
{
	int ret;
	struct pollfd pfd[1];
	int nfds, timeout;

	if (chaos_rcv_buffer_size > 0)
		return 0;

	timeout = 0;
	nfds = 1;
	pfd[0].fd = chaos_fd;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	ret = poll(pfd, nfds, timeout);
	if (ret < 0)
		return -1;

	if (ret > 0) {
		u_char lenbytes[4];
		int len;

		ret = read(chaos_fd, lenbytes, 4);
		if (ret <= 0) {
			perror("chaos read");
			return -1;
		}

		len = (lenbytes[0] << 8) | lenbytes[1];

		ret = read(chaos_fd, (char *)chaos_rcv_buffer, len);
		if (ret < 0) {
			perror("chaos read");
			return -1;
		}

		if (ret != len) {
			printf("chaos read; length error\n");
			return -1;
		}

		printf("polling; got chaosd packet %d\n", ret);

		chaos_rcv_buffer_size = (ret+1)/2;
		chaos_rcv_buffer_empty = 0;

#if 0
		printf("rx to %o, my %o\n",
		       chaos_rcv_buffer[chaos_rcv_buffer_size-3], chaos_addr);
		printf("   from %o, crc %o\n",
		       chaos_rcv_buffer[chaos_rcv_buffer_size-2],
		       chaos_rcv_buffer[chaos_rcv_buffer_size-1]);
#endif

		if (chaos_rcv_buffer[chaos_rcv_buffer_size-3] != chaos_addr) {
			chaos_rcv_buffer_size = 0;
			chaos_rcv_buffer_empty = 1;
			return 0;
		}

		chaos_rx_pkt();
	}

	return 0;
}

int
chaos_sent_to_chaosd(char *buffer, int size)
{
	int ret;
	if (chaos_fd) {
		struct iovec iov[2];
		unsigned char lenbytes[4];

		lenbytes[0] = size >> 8;
		lenbytes[1] = size;
		lenbytes[2] = 0;
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

