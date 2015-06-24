#ifndef _TRAFFIC_CHECK_SOCKET_H_
#define _TRAFFIC_CHECK_SOCKET_H_


typedef struct chk_array_s{
    void         **ptr;
    int            max;
    int            cur;
}chk_array_t;


typedef struct {
    char            *ip;
    int                port;
    int                fd;
    chk_array_t        *list;
}chk_socket_info_t;


extern chk_conf_hander_t     chk_socket;
extern int                     g_chk_process_status;
extern chk_socket_info_t    g_chk_socket_info;

void *chk_udp_server(void *unused);


#endif

