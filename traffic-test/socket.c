#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "read.h"
#include "conf.h"
#include "socket.h"
#include "log.h"


chk_socket_info_t	g_chk_socket_info;

chk_conf_name_func_t chk_socket_conf_item[] = {
		{"server_port",			
		 11,	
		 &g_chk_socket_info,
		 offsetof(chk_socket_info_t, port),
		 chk_set_num },
		{"server_ip",			
		 9,	
		 &g_chk_socket_info,
		 offsetof(chk_socket_info_t, ip),
		 chk_set_path },
		{"white_list",
		 10,
		 &g_chk_socket_info,
		 offsetof(chk_socket_info_t, list),
		 chk_set_list },
		{NULL, 0, NULL, 0, NULL}
};

typedef struct {
	unsigned short	crc;
	unsigned short	len;
	unsigned short	type;
	unsigned short	unused;
	char			data[0];
}chk_format_t;


typedef struct {
	int		type;
	int		name_len;
	char	*cmd_name;
	int		(*func)(char *buf, int len);
	
}chk_socket_mapping_t;

int chk_start_process(char *buf, int len);
int chk_stop_process(char *buf, int len);


chk_socket_mapping_t	g_chk_maping_item[] = {
	{0,
	 5,
	 "start",
	 chk_start_process
	},
	{0,
	 4,
	 "stop",
	 chk_stop_process
	},
	{0, 0, NULL, NULL}
};

int	g_chk_process_status = 1;

int chk_start_process(char *buf, int len)
{
	g_chk_process_status = 1;
	return 0;
}

int chk_stop_process(char *buf, int len) 
{	
	g_chk_process_status = 0;
	return 0;
}


int chk_crc(char	*s, int len)
{
	int 			i = 0;
	int 			l = len/sizeof(unsigned short);
	int				y = len%sizeof(unsigned short);
	unsigned short	r = 0;
	unsigned short	*p = (unsigned short*)s;
	
	for (i = 0; i < y; i ++) {
		s[len + i] = 0xFF;
	}
	l = (len + y)/sizeof(unsigned short);
	
	for (i = 0; i < l; i ++) {
		r ^= *(p + i);
	}
	
	if (r) {
		return -1;
	}
	return 0;
}

int chk_process_data(char *buf, int len) {
	
	chk_format_t	*u = (chk_format_t*)buf;
	int				cmd_len = 0;
	int 			i = 0;
	
	u->len ^= 0x2F2F;
	if (len != u->len) {
		check_error_log("%s", "data len fail");
		return -1;
	}

	cmd_len = len - sizeof(chk_format_t);

	for (i = 0; g_chk_maping_item[i].cmd_name; i ++) {
		if (cmd_len != g_chk_maping_item[i].name_len \
		 || u->type != g_chk_maping_item[i].type) {
			continue;
		}
		if (!strncasecmp(u->data, g_chk_maping_item[i].cmd_name, cmd_len)) {
			return g_chk_maping_item[i].func(buf, len);
		}
	}
	
	return -1;
}


typedef struct chk_ip_mapping_s{
	unsigned int	ip;
	struct chk_ip_mapping_s *next;
}chk_ip_mapping_t;

#define CHK_WHITE_LIST_SIZE	(32)
static chk_ip_mapping_t *g_chk_ip_mapping[CHK_WHITE_LIST_SIZE] = {NULL}; 

int chk_resove_ip(char *str)
{
	char	*p = str;
	char	*ip = str;
	char	*mask = NULL;
	chk_ip_mapping_t	*s = NULL;
	
	while (*p != '\0') {
		if (!isdigit(*p) && *p != '.' && *p != '/') {
			check_error_log("ip addr %s is invalid", str);
			return -1;
		}
		if (*(p - 1) == '/') {
			*(p - 1) = '\0';
			mask = p;
		}
		p ++;
	}
	if (!mask) {
		mask = "32";
	}

	unsigned int ui_ip = ntohl(inet_addr(ip));
	unsigned int ui_mask = atoi(mask);

	ui_ip = ui_ip >> (32 - ui_mask);
	s = malloc(sizeof(chk_ip_mapping_t));
	if (!s) {
		check_error_log("%s", "malloc fail");
		return -1;
	}
	s->ip = ui_ip;
	s->next = g_chk_ip_mapping[ui_mask - 1];
	g_chk_ip_mapping[ui_mask - 1] = s;

	return 0;
}


int chk_white_hash()
{
	int		i = 0;
	int		ret = 0;
	char	**p = NULL;

	if (!g_chk_socket_info.list) {
		return 0;
	}

	p = (char**)(g_chk_socket_info.list->ptr);

	for (i = 0; i < g_chk_socket_info.list->cur; i ++) {
		ret = chk_resove_ip (*(p + i));
		if (ret) {
			return -1;
		}
	}

	return 0;
}


int chk_match_white_list(unsigned int ui_sip)
{
	int	i = 0;
	unsigned int	ip = 0;
	chk_ip_mapping_t	*s = NULL;

	for (i = 7; i < 32; i = i + 8) {
		if (!g_chk_ip_mapping[i]) {
			continue;
		}
		
		ip = ui_sip >> (32 - i - 1);
		s = g_chk_ip_mapping[i];
		
		while (s) {
			if (ip == s->ip) {
				return 0;
			}
			s = s->next;
		}
	}
	
	for (i = 31; i >= 0; i --) {
		
		if (!g_chk_ip_mapping[i]) {
			continue;
		}
		
		ip = ui_sip >> (32 - i - 1);
		s = g_chk_ip_mapping[i];
		
		while (s) {
			if (ip == s->ip) {
				return 0;
			}
			s = s->next;
		}
	}

	return -1;
}


void socket_exit(void *unused)
{
	int					i = 0;
	chk_ip_mapping_t	*t = NULL;
	chk_ip_mapping_t	*n = NULL;

	for (i = 0; i < CHK_WHITE_LIST_SIZE; i ++) {
		t = g_chk_ip_mapping[i];
		while (t) {
			n = t->next;
			free(t);
			t = n;
		}
		g_chk_ip_mapping[i] = NULL;
	}
	close(g_chk_socket_info.fd);
}

void *chk_udp_server(void *unused)
{
	int				fd = 0;
	int 			n = 0;
	int				ret = 0;
	socklen_t		len = 0;
	char			*client_ip;
	char			line[2048];
	struct sockaddr_in serv_addr;
	struct	sockaddr_in cli_addr;

	ret = chk_white_hash();
	if (ret) {
		check_error_log("%s", "create ip white list fail"); 
		goto ERROR;
	}
	
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		check_error_log("%s", "create socket fail"); 
		goto ERROR;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons((unsigned short)g_chk_socket_info.port);
	if (g_chk_socket_info.ip) {
		serv_addr.sin_addr.s_addr = inet_addr(g_chk_socket_info.ip);
	}
	else {
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
		check_error_log("%s", "bind fail");
		goto ERROR;
	}
	g_chk_socket_info.fd = fd;

	
	pthread_cleanup_push(socket_exit, (void*)-1);
	
	while (1) {
		len = sizeof(cli_addr);
		n = recvfrom(fd, line, 2048, 0, (struct sockaddr *)&cli_addr, &len);
		if (n == -1) {
			check_error_log("%s", "read socket fail");
			continue;
		}
		client_ip = inet_ntoa(cli_addr.sin_addr);
		
		ret = chk_match_white_list(ntohl(cli_addr.sin_addr.s_addr));
		if (ret) {
			check_error_log("ip addr %s deny", client_ip);
			continue;
		}
		ret = chk_crc(line, n);
		if (ret) {
			check_error_log("ip addr %s data crc check fail", client_ip);
			continue;
		}
		ret = chk_process_data(line, n);
		if (ret) {
			check_error_log("ip addr %s data process fail", client_ip);
			continue;
		}
		
	}
	pthread_cleanup_pop(1);
	return NULL;

ERROR:
	pthread_close_other(pthread_self());
	pthread_exit((void*)-1);
}


int check_socket_process()
{
	if (!g_chk_socket_info.port) {
		check_error_log("%s", "socket need port");
		return -1;
	}
	return 0;
}

chk_conf_hander_t chk_socket =  {
	.name = "socket",
	.conf = chk_socket_conf_item,
	.check = check_socket_process,
	.handler = NULL,
};



