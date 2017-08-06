#include "usim.h"

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

#include "ucode.h"
#include "endian.h"
#include "chaos.h"

#ifndef CHAOS_MY_ADDRESS
# define CHAOS_MY_ADDRESS 0401
#endif

#define CHAOS_DEBUG_PKT 0

#define CHAOS_BUF_SIZE_BYTES 8192

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
static int reconnect_delay;
static int reconnect_time;
void chaos_force_reconect(void);
int chaos_send_to_chaosd(char *buffer, int size);
int chaos_reconnect(void);

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

static unsigned short
ch_checksum(const unsigned char *addr, int count)
{
	/*
	 * RFC1071
	 * Compute Internet Checksum for "count" bytes
	 *         beginning at location "addr".
	 */
	register long sum = 0;

	while( count > 1 )  {
		/*  This is the inner loop */
		sum += *(addr)<<8 | *(addr+1);
		addr += 2;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if( count > 0 )
		sum += * (unsigned char *) addr;

	/*  Fold 32-bit sum to 16 bits */
	while (sum>>16)
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
	"UNKNOWN",                                      // 0
	"RFC - Request for Connection",                 // 1
	"OPN - Open Connection",                        // 2
	"CLS - Close Connection",                       // 3
	"FWD - Forward a Request for Connection",       // 4
	"ANS - Answer a Simple Transaction",            // 5
	"SNS - Sense Status",                           // 6
	"STS - Status",                                 // 7
	"RUT - Routing Information",                    // 8
	"LOS - Lossage",                                // 9
	"LSN - Listen",                                 // 10
	"MNT - Maintenance",                            // 11
	"EOF",                                          // 12
	"UNC - Uncontrolled Data",                      // 13
	"BRD - Broadcast",                              // 14
	"UNKNOWN",                                      // 15
};

static void
dumpbuffer(unsigned short *buffer, int size)
{
	int j, offset, skipping;
	char cbuf[17];
	char line[80];
	unsigned char *buf = (unsigned char *)&buffer[8];
	int cnt = (buffer[1] & 0x0fff);
	int opcode = (buffer[0] & 0xff00) >> 8;

	size = size - (int)(8 * sizeof(short)); // subtract off the size of packet header
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
	printf("destination address: %o  index: %04x\n", buffer[2], buffer[3]);
	printf("source address: %o  index: %04x\n", buffer[4], buffer[5]);
	printf("packet number: %d\n", buffer[6]);
	printf("acknowledgement: %d\n", buffer[7]);

	offset = 0;
	skipping = 0;
	while (cnt > 0) {
		if (offset > 0 && memcmp(buf, buf-16, 16) == 0) {
			skipping = 1;
		} else {
			if (skipping) {
				skipping = 0;
				printf("...\n");
			}
		}

		if (!skipping) {
			for (j = 0; j < 16; j++) {
				char *pl = line+j*3;

				if (j >= cnt) {
					strcpy(pl, "xx ");
					cbuf[j] = 'x';
				} else {
					sprintf(pl, "%02x ", buf[j]);
					cbuf[j] = buf[j] < ' ' ||
						buf[j] > '~' ? '.' : (char)buf[j];
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
		printf("%08x ...\n", offset-16);
	}

}
#endif // CHAOS_DEBUG_PKT

void
chaos_xmit_pkt(void)
{
	tracenet("chaos_xmit_pkt() %d bytes, data len %d\n",
		 chaos_xmit_buffer_ptr * 2,
		 (chaos_xmit_buffer_ptr > 0 ? chaos_xmit_buffer[1]&0x3f : -1));

	chaos_xmit_buffer_size = chaos_xmit_buffer_ptr;

#if CHAOS_DEBUG_PKT
	dumpbuffer(chaos_xmit_buffer, chaos_xmit_buffer_size * 2);
#endif

	/* Dest is already in the buffer */

	chaos_xmit_buffer[chaos_xmit_buffer_size++] =	/* source */
		(unsigned short)chaos_addr;

	chaos_xmit_buffer[chaos_xmit_buffer_size] =	/* checksum */
		ch_checksum((u_char *)chaos_xmit_buffer,
			    chaos_xmit_buffer_size*2);
	chaos_xmit_buffer_size++;

	chaos_send_to_chaosd((char *)chaos_xmit_buffer,
			     chaos_xmit_buffer_size*2);

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
		/* read last word, clear receive done */
		chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
		chaos_rcv_buffer_size = 0;
		tracenet("chaos_get_rcv_buffer: cleared CHAOS_CSR_RECEIVE_DONE\n");
	}
	return v;
}

void
chaos_put_xmit_buffer(int v)
{
	if (chaos_xmit_buffer_ptr < (int)sizeof(chaos_xmit_buffer)/2)
		chaos_xmit_buffer[chaos_xmit_buffer_ptr++] = (unsigned short)v;
	chaos_csr &= ~CHAOS_CSR_TRANSMIT_DONE;
}

int
chaos_get_csr(void)
{
	{
		static int old_chaos_csr = 0;
		if (chaos_csr != old_chaos_csr) {
			old_chaos_csr = chaos_csr;
			tracenet("unibus: chaos read csr %o\n", chaos_csr);
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
	int mask;

	v &= 0xffff;

	/* Writing these don't stick */
	mask = CHAOS_CSR_TRANSMIT_DONE |
		CHAOS_CSR_LOST_COUNT |
		CHAOS_CSR_CRC_ERROR |
		CHAOS_CSR_RECEIVE_DONE |
		CHAOS_CSR_RECEIVER_CLEAR;


	if (trace_net_flag) {
		tracenet("chaos: set csr bits 0%o (",v);
		print_csr_bits(v);
		tracenet ("), old 0%o ", chaos_csr);
	}

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

		reconnect_delay = 200; /* Do it right away */
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

		/* if buffer is full, generate status & interrupt again */
		if (chaos_rcv_buffer_size > 0)
		{
			tracenet("\n rx-enabled and buffer is full\n");
			chaos_rx_pkt();
		}
	}

	if (chaos_csr & CHAOS_CSR_TRANSMIT_ENABLE) {
		tracenet("tx-enable ");
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	}

	if (trace_net_flag) {
		tracenet(" New csr 0%o", chaos_csr);
		print_csr_bits(chaos_csr);
		tracenet("\n");
	}

	return 0;
}

void
chaos_force_reconect(void)
{
}

static packet_queue *queuehead;
static packet_queue *queuetail;
int connectionstate = 0;

static pthread_mutex_t recvqueue;

chaos_connection *connections[256];

#define CONNECTION_RFC      1
#define CONNECTION_RFC_OPN  2
#define CONNECTION_RFC_STS  3

void
chaos_queue(chaos_packet *packet)
{
	packet_queue *node = malloc(sizeof(packet_queue));

	node->next = 0;
	node->packet = packet;
	pthread_mutex_lock(&recvqueue);
	if (queuetail)
		queuetail->next = node;
	queuetail = node;
	if (queuehead == 0)
		queuehead = node;
	pthread_mutex_unlock(&recvqueue);
}

chaos_connection *
chaos_make_connection(void)
{
	int i;

	for (i = 0; i < 255; i++) {
		if (connections[i] == 0) {
			chaos_connection *conn = (chaos_connection *)malloc(sizeof(chaos_connection));
			static unsigned short index = 0x0101;

			conn->state = cs_closed;
			conn->packetnumber = 0;
			conn->lastacked = 0;
			conn->remoteindex = 0;
			conn->remoteaddr = 0;
			conn->localaddr = CHAOS_SERVER_ADDRESS;
			conn->localindex = index;
			index += 0x0100;
			conn->queuehead = 0;
			conn->queuetail = 0;

			pthread_mutex_init(&conn->queuelock, NULL);

			pthread_mutex_init(&conn->queuesem, NULL);
			pthread_cond_init(&conn->queuecond, NULL);
			pthread_mutex_init(&conn->twsem, NULL);
			pthread_cond_init(&conn->twcond, NULL);

			conn->lastreceived = 0;
			conn->lastsent = 0;
			conn->remotelastreceived = 0;
			conn->orderhead = 0;
			conn->ordertail = 0;
			conn->rwsize = 5;
			conn->twsize = 5;

			connections[i] = conn;
			return conn;
		}
	}

	return 0;
}

void
chaos_dump_connection(chaos_connection *conn)
{
	int i;

	for (i = 0; i < 255; i++) {
		if (connections[i] == conn) {
			printf("conn: %d\n", i);
			printf("conn: remote %04x %04x\n", conn->remoteaddr, conn->remoteindex);
			printf("conn: local  %04x %04x\n", conn->localaddr, conn->localindex);
			printf("conn: lastreceived=%d lastacked=%d\n", conn->lastreceived, conn->lastacked);
			printf("conn: lastsent=%d remotelastreceived=%d\n", conn->lastsent, conn->remotelastreceived);
			break;
		}
	}
}

void
chaos_delete_connection(chaos_connection *conn)
{
	int i;

	for (i = 0; i < 255; i++)
		if (connections[i] == conn) {
			connections[i] = 0;
			break;
		}

	pthread_mutex_destroy(&conn->queuelock);
	pthread_mutex_destroy(&conn->queuesem);
	pthread_cond_destroy(&conn->queuecond);
	pthread_mutex_destroy(&conn->twsem);
	pthread_cond_destroy(&conn->twcond);
	free(conn);
}

void chaos_interrupt_connection(chaos_connection *conn)
{
	conn->remotelastreceived = conn->lastsent;
	pthread_mutex_lock(&conn->twsem);
	pthread_cond_signal(&conn->twcond);
	pthread_mutex_unlock(&conn->twsem);
}

int chaos_connection_queue(chaos_connection *conn, chaos_packet *packet)
{
	packet_queue *node;
	unsigned short nextpacket;

	// any packet for remote gets queued
	if (packet->destaddr == conn->remoteaddr) {
		for (;;) {
			if (conn->state == cs_open && (conn->lastsent - conn->remotelastreceived) >= conn->twsize && ((packet->opcode >> 8) != CHAOS_OPCODE_STS)) {
				tracenet("waiting for remote ack packet=%d\n", packet->number);
				tracenet("lastsent = %d  remotelastreceived = %d  twsize = %d\n", conn->lastsent, conn->remotelastreceived, conn->twsize);
				pthread_mutex_lock(&conn->twsem);
				struct timespec ts;

				clock_gettime(CLOCK_REALTIME, &ts);
				ts.tv_sec += 5;
				if (pthread_cond_timedwait(&conn->twcond, &conn->twsem, &ts)) {
					if (conn->lastpacket) {
						printf("re-transmit last packet\n");
						chaos_packet *retransmit = conn->lastpacket;
						conn->lastpacket = 0;
						chaos_queue(retransmit);
					}
				}
				pthread_mutex_unlock(&conn->twsem);
			} else
				break;
		}

		if ((packet->opcode >> 8) != CHAOS_OPCODE_STS && packet->destindex == conn->remoteindex && cmp_gt(packet->number, conn->lastsent))
			conn->lastsent = packet->number;
		conn->lastpacket = packet;
		chaos_queue(packet);
		return 0;
	}

	node = malloc(sizeof(packet_queue));
	node->next = 0;
	node->packet = packet;
	nextpacket = conn->lastreceived + 1;
	if (cmp_gt(packet->number, nextpacket)) {
		tracenet("chaos_connection_queue: out-of-order nextpacket=%d packet=%d\n", nextpacket, packet->number);
		pthread_mutex_lock(&conn->queuelock);
		if (conn->ordertail)
			conn->ordertail->next = node;
		conn->ordertail = node;
		if (conn->orderhead == 0)
			conn->orderhead = node;
		pthread_mutex_unlock(&conn->queuelock);
		return 0;
	}

	{
		pthread_mutex_lock(&conn->queuelock);
		if (conn->queuetail)
			conn->queuetail->next = node;
		conn->queuetail = node;
		if (conn->queuehead == 0)
			conn->queuehead = node;
		pthread_mutex_unlock(&conn->queuelock);
	}
	pthread_mutex_lock(&conn->queuesem);
	pthread_cond_signal(&conn->queuecond);
	pthread_mutex_unlock(&conn->queuesem);
	return 0;
}

chaos_packet *chaos_connection_dequeue(chaos_connection *conn)
{
	chaos_packet *packet = NULL;
	packet_queue *node = NULL;
	unsigned short nextpacket = conn->lastreceived + 1;

	if (conn->state == cs_closed)
		return 0;

	for (;;) {
		pthread_mutex_lock(&conn->queuelock);
		if (conn->orderhead) {
			packet_queue *prev = 0;

			for (node = conn->orderhead; node; prev = node, node = node->next)
				if (node->packet->number == nextpacket) {
					tracenet("chaos_connection_deque: grabbed out-of-order packet %d\n", nextpacket);
					if (prev == 0)
						conn->orderhead = node->next;
					else
						prev->next = node->next;
					if (conn->ordertail == node)
						conn->ordertail = prev;
					packet = node->packet;
					break;
				}
		}
		if (node == 0 && conn->queuehead) {
			packet = conn->queuehead->packet;
			node = conn->queuehead;
			conn->queuehead = node->next;
			if (conn->queuehead == 0)
				conn->queuetail = 0;
		}
		pthread_mutex_unlock(&conn->queuelock);

		if (node) {
			free(node);
			node = 0;
		}
		if (packet) {
			if (cmp_gt(packet->number, conn->lastreceived))
				conn->lastreceived = packet->number;
			conn->remotelastreceived = packet->acknowledgement;
			if (3 * (short)(conn->lastreceived - conn->lastacked) > conn->rwsize) {
				chaos_packet *status = chaos_allocate_packet(conn, CHAOS_OPCODE_STS, 2 * sizeof(unsigned short));

				conn->lastacked = conn->lastreceived;
				*(unsigned short *)&status->data[0] = conn->lastreceived;
				*(unsigned short *)&status->data[2] = conn->rwsize;

				chaos_queue(status);
			}
			return packet;
		}

		pthread_mutex_lock(&conn->queuesem);
		pthread_cond_wait(&conn->queuecond, &conn->queuesem);
		pthread_mutex_unlock(&conn->queuesem);

		if (conn->state == cs_closed)
			return 0;
	}
}

chaos_packet *
chaos_allocate_packet(chaos_connection *conn, int opcode, ssize_t len)
{
	chaos_packet *packet = (chaos_packet *)malloc(CHAOS_PACKET_HEADER_SIZE + (size_t)len);

	packet->opcode = (unsigned short)(opcode << 8);
	packet->length = (unsigned short)len;
	packet->destaddr = conn->remoteaddr;
	packet->destindex = conn->remoteindex;
	packet->sourceaddr = conn->localaddr;
	packet->sourceindex = conn->localindex;
	if (opcode == CHAOS_OPCODE_STS)
		packet->number = conn->packetnumber;
	else
		packet->number = ++conn->packetnumber;
	if (packet->number == 0)
		packet->number = conn->packetnumber = 1;
	packet->acknowledgement = conn->lastreceived;
	conn->lastacked = conn->lastreceived;
	tracenet("chaos_allocate_packet: %04x number: %d\n", conn->localindex, packet->number);
	return packet;
}

chaos_connection *
chaos_find_connection(unsigned short index)
{
	int i;

	for (i = 0; i < 255; i++)
		if (connections[i] && connections[i]->localindex == index)
			return connections[i];
	return 0;
}

chaos_connection *
chaos_open_connection(int co_host, char *contact, int mode, int async, int rwsize)
{
	size_t co_clength;
	chaos_connection *conn = chaos_make_connection();
	chaos_packet *pkt;

	tracenet("chaos_open_connection(address=%o,contact=%s)\n", co_host, contact);

	co_clength = strlen(contact);

	conn->rwsize = (unsigned short)(rwsize ? rwsize : 5);

	if (co_host) {
		pkt = (chaos_packet *)malloc(CHAOS_PACKET_HEADER_SIZE + co_clength);

		pkt->opcode = CHAOS_OPCODE_RFC << 8;
		pkt->length = (unsigned short)co_clength;
		pkt->destaddr = (unsigned short)chaos_addr;
		pkt->destindex = 0;
		conn->remoteaddr = (unsigned short)chaos_addr;
		conn->remoteindex = 0;
		pkt->sourceaddr = conn->localaddr;
		pkt->sourceindex = conn->localindex;
		pkt->number = ++conn->packetnumber;
		if (pkt->number == 0)
			pkt->number = conn->packetnumber = 1;
		pkt->acknowledgement = 0;
		conn->lastacked = 0;

		memcpy(pkt->data, contact, co_clength);

		chaos_connection_queue(conn, pkt);

		conn->state = cs_rfcsent;

		// wait for answer
		chaos_packet *answer = chaos_connection_dequeue(conn);
		if (answer == 0)
			return conn;

		conn->remoteindex = answer->sourceindex;
		conn->state = cs_open;

		chaos_packet *sts = malloc(CHAOS_PACKET_HEADER_SIZE + 2 * sizeof(unsigned short));

		sts->opcode = CHAOS_OPCODE_STS << 8;
		sts->length = 2 * sizeof(unsigned short);
		sts->destaddr = (unsigned short)chaos_addr;
		sts->destindex = conn->remoteindex;
		sts->sourceaddr = conn->localaddr;
		sts->sourceindex = conn->localindex;
		sts->number = conn->packetnumber;
		if (sts->number == 0)
			sts->number = conn->packetnumber = 1;
		sts->acknowledgement = answer->number;
		conn->lastacked = answer->number;
		*(unsigned short *)&sts->data[0] = conn->lastacked;
		*(unsigned short *)&sts->data[2] = conn->rwsize;

		free(answer);
		chaos_connection_queue(conn, sts);
	} else {
		// listen
	}

	return conn;
}

int
chaos_poll(void)
{
	/* is rx buffer full? */
	if (!chaos_rcv_buffer_empty && (chaos_csr & CHAOS_CSR_RECEIVE_DONE)) {
		tracenet("chaos: polling, but unread data exists\n");
		return 0;
	}

	if (!chaos_rcv_buffer_empty) {
		tracenet("chaos: polling, but buffer not empty\n");
		return 0;
	}
	if (queuehead == 0) {
		return 0;
	}

	if (!(chaos_csr & CHAOS_CSR_RECEIVE_ENABLE)) {
		tracenet("chaos: polling but rx not enabled\n");
		return 0;
	}

	chaos_packet *packet;
	packet_queue *node;

	pthread_mutex_lock(&recvqueue);
	node = queuehead;
	packet = queuehead->packet;
	queuehead = node->next;
	if (queuehead == 0)
		queuetail = 0;
	pthread_mutex_unlock(&recvqueue);

	int size = ((packet->length & 0x0fff) + CHAOS_PACKET_HEADER_SIZE + 1) / 2;
	unsigned short dest_addr = packet->destaddr;
	unsigned short source_addr = packet->sourceaddr;

	free(node);

	memcpy(chaos_rcv_buffer, packet, (size_t)size * sizeof(unsigned short));

	// extra network header info that seems to get passed along
	chaos_rcv_buffer[size] = dest_addr;
	chaos_rcv_buffer[size + 1] = source_addr;
	chaos_rcv_buffer[size + 2] = 0;        // unused checksum
	size += 3;

	// ignore any packets not to us
	if (dest_addr != chaos_addr)
		return 0;

	chaos_rcv_buffer_size = size;
	chaos_rcv_buffer_empty = 0;

	tracenet("chaos polling: got chaosd packet of %d bytes\n", chaos_rcv_buffer_size * 2);

#if CHAOS_DEBUG_PKT
	dumpbuffer(chaos_rcv_buffer, size * 2);
#endif
	chaos_rx_pkt();

	return 0;
}

int
chaos_send_to_chaosd(char *buffer, int size)
{
	chaos_packet *packet = (chaos_packet *)buffer;

	/* local loopback */
	if (chaos_csr & CHAOS_CSR_LOOP_BACK) {
		printf("chaos: loopback %d bytes\n", size);
		memcpy(chaos_rcv_buffer, buffer, size);

		chaos_rcv_buffer_size = (size+1)/2;
		chaos_rcv_buffer_empty = 0;

		chaos_rx_pkt();

		return 0;
	}

	tracenet("chaos tx: dest_addr = %o, chaos_addr=%o, size %d, wcount %d\n",
		 packet->destaddr, chaos_addr, size, (size + 1) / 2);

	// look for packets addressed to the local server
	if (packet->destaddr == CHAOS_SERVER_ADDRESS) {
		chaos_connection *conn = chaos_find_connection(packet->destindex);

		if (((packet->opcode & 0xff00) >> 8) == CHAOS_OPCODE_RFC) {
			if (conn && conn->state == cs_rfc) {
				tracenet("Duplicate RFC\n");
				return 0;
			}
			if (conn) {
				chaos_packet *pkt = malloc((size_t)size);

				memcpy(pkt, packet, size);
				chaos_connection_queue(conn, pkt);
				return 0;
			}

			tracenet("RFC packet\n");

			if (memcmp(&packet->data, "FILE", 4) == 0) {
				void processdata(chaos_connection *conn);
				chaos_packet *answer;

				if (packet->data[4] == ' ' && packet->data[5] >= '0' && packet->data[5] <= '9') {
					extern int protocol;
					protocol = packet->data[5] - '0';
					printf("FILE protocol version=%d\n", protocol);
				}

				conn = chaos_make_connection();
				tracenet("RFC FILE\n");
				conn->remoteaddr = packet->sourceaddr;
				conn->remoteindex = packet->sourceindex;
				answer = chaos_allocate_packet(conn, CHAOS_OPCODE_OPN, 2 * sizeof(unsigned short));
				answer->acknowledgement = packet->number;
				conn->lastacked = packet->number;
				*(unsigned short *)&answer->data[0] = packet->number; // last packed received
				*(unsigned short *)&answer->data[2] = conn->rwsize;

				chaos_connection_queue(conn, answer);

				conn->state = cs_rfc;

				processdata(conn);

				return 0;
			}
			if (memcmp(&packet->data, "MINI", 4) == 0) {
				void processmini(chaos_connection *conn);
				chaos_packet *answer;

				packet->data[packet->length] = '\0';
				printf("MINI: '%s'\n", &packet->data[4]);

				conn = chaos_make_connection();
				tracenet("RFC MINI\n");
				conn->remoteaddr = packet->sourceaddr;
				conn->remoteindex = packet->sourceindex;
				answer = chaos_allocate_packet(conn, CHAOS_OPCODE_OPN, 2 * sizeof(unsigned short));
				answer->acknowledgement = packet->number;
				conn->lastacked = packet->number;
				*(unsigned short *)&answer->data[0] = packet->number; // last packed received
				*(unsigned short *)&answer->data[2] = conn->rwsize;

				chaos_connection_queue(conn, answer);

				conn->state = cs_rfc;
				conn->lastreceived = 1;

				processmini(conn);

				return 0;
			}
			if (memcmp(&packet->data, "TIME", 4) == 0) {
				time_t t;
				struct timeval time;
				chaos_packet *answer = malloc(CHAOS_PACKET_HEADER_SIZE + sizeof(long));

				gettimeofday(&time, NULL);

				t = time.tv_sec;
				t += 60UL*60*24*((1970-1900)*365L + 1970/4 - 1900/4);

				printf("time-rfc: answering\n");

				answer->opcode = CHAOS_OPCODE_ANS << 8;
				answer->length = sizeof(long);
				answer->destaddr = packet->sourceaddr;
				answer->destindex = packet->sourceindex;
				answer->sourceaddr = CHAOS_SERVER_ADDRESS;
				answer->sourceindex = 0;
				answer->number = 0;
				answer->acknowledgement = 0;
				*(long *)&answer->data[0] = t;

				chaos_queue(answer);
				return 0;
			}
			if (memcmp(&packet->data, "UPTIME", 6) == 0) {
				chaos_packet *answer = malloc(CHAOS_PACKET_HEADER_SIZE + sizeof(long));

				printf("uptime: answering\n");

				answer->opcode = CHAOS_OPCODE_ANS << 8;
				answer->length = sizeof(long);
				answer->destaddr = packet->sourceaddr;
				answer->destindex = packet->sourceindex;
				answer->sourceaddr = CHAOS_SERVER_ADDRESS;
				answer->sourceindex = 0;
				answer->number = 0;
				answer->acknowledgement = 0;
				*(long *)&answer->data[0] = 0;

				chaos_queue(answer);
				return 0;
			}
			if (memcmp(&packet->data, "STATUS", 6) == 0) {
				chaos_packet *answer = malloc(CHAOS_PACKET_HEADER_SIZE + sizeof(chaos_status));
				chaos_status *status = (chaos_status *)&answer->data[0];

				answer->opcode = CHAOS_OPCODE_ANS << 8;
				answer->length = sizeof(chaos_status);
				answer->destaddr = packet->sourceaddr;
				answer->destindex = packet->sourceindex;
				answer->sourceaddr = CHAOS_SERVER_ADDRESS;
				answer->sourceindex = 0;
				answer->number = 0;
				answer->acknowledgement = 0;

				memset(status, 0, sizeof(chaos_status));
				strncpy(status->name, CHAOS_SERVER_NAME, CHSTATNAME);
				status->name[CHSTATNAME-1] = '\0';

				status->subnetident = CHAOS_SERVER_ADDRESS;
				status->subnetnumshorts = 8 * sizeof(int) / sizeof(short);
				chaos_queue(answer);
				tracenet("statusrfc: answering\n");
				return 0;
			}

			char buffer[512];

			strncpy(buffer, (char *)packet->data, packet->length);
			buffer[packet->length] = '\0';
			printf("unknown RFC: '%s'\n", packet->data);

			return 0;
		}
		if (((packet->opcode & 0xff00) >> 8) == CHAOS_OPCODE_SNS && conn) {
			chaos_packet *sts = malloc(CHAOS_PACKET_HEADER_SIZE + 2 * sizeof(unsigned short) + 3 * sizeof(unsigned short));

			sts->opcode = CHAOS_OPCODE_STS << 8;
			sts->length = 2 * sizeof(unsigned short);
			sts->destaddr = packet->sourceaddr;
			sts->destindex = packet->sourceindex;
			conn->remoteaddr = packet->sourceaddr;
			conn->remoteindex = packet->sourceindex;
			sts->sourceaddr = conn->localaddr;
			sts->sourceindex = conn->localindex;
			sts->number = packet->number;
			if (cmp_gt(packet->acknowledgement, conn->remotelastreceived))
				conn->remotelastreceived = packet->acknowledgement;
			sts->acknowledgement = conn->lastacked;
			*(unsigned short *)&sts->data[0] = conn->lastacked;
			*(unsigned short *)&sts->data[2] = conn->rwsize;

			chaos_connection_queue(conn, sts);
#if CHAOS_DEBUG
			chaos_dump_connection(conn);
#endif
			return 0;
		}
		if (((packet->opcode & 0xff00) >> 8) == CHAOS_OPCODE_STS) {
			if (conn) {
				if (cmp_gt(packet->acknowledgement, conn->remotelastreceived))
					conn->remotelastreceived = packet->acknowledgement;

				if (conn->lastpacket && conn->remotelastreceived == conn->lastpacket->number) {
					free(conn->lastpacket);
					conn->lastpacket = 0;
				}

				conn->state = cs_open;
				conn->twsize = *(unsigned short *)&packet->data[2];
				tracenet("STS: twsize = %d\n", conn->twsize);
				pthread_mutex_lock(&conn->twsem);
				pthread_cond_signal(&conn->twcond);
				pthread_mutex_unlock(&conn->twsem);
			}
			return 0;
		}
		if (conn && (packet->opcode >> 8) == CHAOS_OPCODE_CLS) {
			tracenet("chaos: got close\n");
			conn->state = cs_closed;

			pthread_mutex_lock(&conn->queuesem);
			pthread_cond_signal(&conn->queuecond);
			pthread_mutex_unlock(&conn->queuesem);
			usleep(100000);      // wait for queue to wake up
			chaos_delete_connection(conn);
			return 0;
		}
		if (conn && (cmp_gt(conn->lastreceived, packet->number) || (conn->lastreceived == packet->number))) {
			tracenet("chaos: Duplicate data packet\n");
			chaos_packet *status = chaos_allocate_packet(conn, CHAOS_OPCODE_STS, 2 * sizeof(unsigned short));

			status->number = packet->number;
			conn->lastacked = conn->lastreceived;
			*(unsigned short *)&status->data[0] = conn->lastreceived;
			*(unsigned short *)&status->data[2] = conn->rwsize;
			chaos_connection_queue(conn, status);
			return 0;
		}

		if (conn) {
			chaos_packet *pkt = malloc((size_t)size);

			memcpy(pkt, packet, size);
			chaos_connection_queue(conn, pkt);
		}

		return 0;
	}


	/* receive packets addressed to ourselves */
	if (packet->destaddr == chaos_addr) {
		memcpy(chaos_rcv_buffer, buffer, size);

		chaos_rcv_buffer_size = (size+1)/2;
		chaos_rcv_buffer_empty = 0;

		chaos_rx_pkt();
	}

	return 0;
}

int
chaos_init(void)
{
	pthread_mutex_init(&recvqueue, NULL);

	chaos_rcv_buffer_empty = 1;

	return 0;
}
