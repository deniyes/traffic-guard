#ifndef _TRAFFIC_CHICK_READ_H_
#define _TRAFFIC_CHICK_READ_H_


typedef struct chk_read_data_s{
	char				ip[16];
	unsigned long		all_send_packet;
	unsigned long 		all_send_bit;
	unsigned long 		all_recv_packet;
	unsigned long		all_recv_bit;

	unsigned long 		tcp_send_packet;
	unsigned long 		tcp_send_bit;
	unsigned long		tcp_recv_packet;
	unsigned long		tcp_recv_bit;

	unsigned long		udp_send_packet;
	unsigned long		udp_send_bit;
	unsigned long		udp_recv_packet;
	unsigned long		udp_recv_bit;

	
	unsigned long		icmp_send_packet;
	unsigned long		icmp_send_bit;
	unsigned long		icmp_recv_packet;
	unsigned long		icmp_recv_bit;

	unsigned long		other_send_packet;
	unsigned long		other_send_bit;
	unsigned long		other_recv_packet;
	unsigned long		other_recv_bit;

	struct chk_read_data_s		*next;
}chk_read_data_t;

void *chk_traffic_read(void *unused) ;


#endif
