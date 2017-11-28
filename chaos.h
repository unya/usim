#ifndef USIM_CHAOS_H
#define USIM_CHAOS_H

#include <stdio.h>
#include <pthread.h>

extern void chaos_rx_pkt(void);
extern void char_xmit_done_intr(void);
extern void chaos_xmit_pkt(void);
extern int chaos_get_bit_count(void);
extern int chaos_get_rcv_buffer(void);
extern void chaos_put_xmit_buffer(int v);
extern int chaos_get_csr(void);
extern int chaos_get_addr(void);
extern void print_csr_bits(int csr);
extern int chaos_set_csr(int v);
extern void chaos_force_reconect(void);
extern int chaos_poll(void);
extern int chaos_send_to_chaosd(char *buffer, int size);
extern int chaos_init(void);
extern int chaos_reconnect(void);

#define CHAOS_SERVER_ADDRESS 0404
#define CHAOS_SERVER_NAME "server"

#define CHAOS_OPCODE_RFC 1
#define CHAOS_OPCODE_OPN 2
#define CHAOS_OPCODE_CLS 3
#define CHAOS_OPCODE_FWD 4
#define CHAOS_OPCODE_ANS 5
#define CHAOS_OPCODE_SNS 6
#define CHAOS_OPCODE_STS 7
#define CHAOS_OPCODE_RUT 8
#define CHAOS_OPCODE_LOS 9
#define CHAOS_OPCODE_LSN 10
#define CHAOS_OPCODE_MNT 11
#define CHAOS_OPCODE_EOF 12
#define CHAOS_OPCODE_UNC 13
#define CHAOS_OPCODE_BRD 14
#define CHAOS_OPCODE_DATA 128
#define CHAOS_OPCODE_DWD 192

// Compare packet numbers which wrap to zero.
#define cmp_gt(a,b) (0100000 & (b - a))

#define CHAOS_PACKET_HEADER_SIZE (8 * sizeof(unsigned short))

typedef struct chaos_packet {
	unsigned short opcode;
	unsigned short length;
	unsigned short destaddr;
	unsigned short destindex;
	unsigned short sourceaddr;
	unsigned short sourceindex;
	unsigned short number;
	unsigned short acknowledgement;
	unsigned char data[488];
} chaos_packet;

#define CHSTATNAME 32		// Length of node name in STATUS protocol.

typedef struct chaos_status {
	char name[CHSTATNAME];
	unsigned short subnetident;
	unsigned short subnetnumshorts;
	int received;
	int transmitted;
	int abort;
	int lost;
	int crcr;
	int crci;
	int length;
	int rejected;
} chaos_status;

typedef struct packet_queue {
	struct packet_queue *next;
	chaos_packet *packet;
} packet_queue;

typedef enum connection_state {
	cs_closed,
	cs_rfc,
	cs_rfcsent,
	cs_open,
} connection_state;

typedef struct chaos_connection {
	unsigned short packetnumber;
	unsigned short lastacked;
	unsigned short localaddr;
	unsigned short localindex;
	unsigned short remoteaddr;
	unsigned short remoteindex;
	unsigned short lastreceived;
	unsigned short lastsent;
	unsigned short remotelastreceived;
	unsigned short rwsize;
	unsigned short twsize;
	connection_state state;
	packet_queue *queuehead;
	packet_queue *queuetail;
	pthread_mutex_t queuelock;
	pthread_mutex_t queuesem;
	pthread_cond_t queuecond;
	pthread_mutex_t twsem;
	pthread_cond_t twcond;
	packet_queue *orderhead;
	packet_queue *ordertail;
	chaos_packet *lastpacket;
} chaos_connection;

extern void chaos_queue(chaos_packet *packet);
extern chaos_connection *chaos_make_connection(void);
extern void chaos_delete_connection(chaos_connection *conn);
extern chaos_connection *chaos_find_connection(unsigned short index);
extern int chaos_connection_queue(chaos_connection *conn, chaos_packet *packet);
extern chaos_packet *chaos_connection_dequeue(chaos_connection *conn);
extern chaos_connection *chaos_open_connection(int co_host, char *contact, int mode, int async, int rwsize);
extern chaos_packet *chaos_allocate_packet(chaos_connection *conn, int opcode, ssize_t len);
extern void chaos_dump_connection(chaos_connection *conn);
extern void chaos_interrupt_connection(chaos_connection *conn);

#endif
