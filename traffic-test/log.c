#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "read.h"
#include "conf.h"
#include "log.h"
#include "socket.h"

typedef struct {
	int 	rate;
	char	*statistic_log;
}chk_log_t;

extern FILE *FILE_ERR;

static FILE *FILE_LOG = NULL;


chk_log_t g_chk_log_conf;

static chk_conf_name_func_t chk_conf_log_item[] = {

	{"statistic_log",	
 	 13,		
 	 &g_chk_log_conf,
 	 offsetof(chk_log_t, statistic_log),
 	 chk_set_path },
 	 
	{"statistic_rate",
	 14,
 	 &g_chk_log_conf,
 	 offsetof(chk_log_t, rate),
 	 chk_set_time },
 	 
 	 {NULL, 0, NULL, 0, NULL}
};


static pthread_mutex_t	g_errlog_lock = PTHREAD_MUTEX_INITIALIZER;

int check_log_file_test(char *path, FILE **fp) 
{
	struct stat 		stat_info;
	int 				size = 0;
	char				new_path[1024] = {0};
	int					ret = 0;
	FILE				*fp_new = NULL;

	if (!(*fp)) 
		goto AGAIN;
	
	if (access(path, F_OK)) {
		fclose(*fp);
		goto AGAIN;
	}
	if(stat(path, &stat_info) < 0){  
		return -1;  
	} else {  
		size = stat_info.st_size;  
    }

	if (size < MAX_LOG_SIZE)
		return 0;

	fclose (*fp);
	ret = snprintf(new_path, 1024, "%s.bak", path);
	if (ret < 0) 
		return -1;
	if (rename(path, new_path))
		return -1;
	
AGAIN:
	fp_new = fopen(path, "a");
	if (!fp_new) 
		return -1;
	*fp = fp_new;
	return 0;
}


void check_error_log(char *fmt, ...)
{
	time_t		s;
	size_t		len = 0;
	char		buf[1024];
	char		*p = NULL;
	va_list		args;

	pthread_mutex_lock(&g_errlog_lock);
	if (check_log_file_test(err_path, &FILE_ERR)) {
		return;
	}
	
	s = time(NULL);
	ctime_r(&s, buf);

	len = strlen(buf);
	buf[len] = buf[len - 1] = '\t';

	p = buf + len + 1;
	va_start(args, fmt);
	vsprintf(p, fmt, args);
	va_end(args);

	fprintf(FILE_ERR, "%s.\n", buf);
	fflush(FILE_ERR);
	pthread_mutex_unlock(&g_errlog_lock);
}

void log_exit(void *unused)
{
	if (g_chk_log_conf.statistic_log) {
		free(g_chk_log_conf.statistic_log);
		g_chk_log_conf.statistic_log = NULL;
	}
	g_chk_log_conf.rate = 0;
	if (FILE_LOG) {
		fclose(FILE_LOG);
		FILE_LOG = NULL;
	}
}

void chk_static_list(chk_result_t *p, chk_read_data_t	*res)
{
	int					i = 0;
	int					j = 0;
	unsigned long		*a = NULL;
	unsigned long		*s = NULL;
	
	memset(res, 0, sizeof(chk_read_data_t));
	for (i = 0; i < p->max; i ++) {
		a = &res->all_send_packet;
		s = &p->array[i].all_send_packet;
		for (j = 0; j < CHK_CONF_STATISTIC_NUM; j ++) {
			*(a ++) += *(s ++);
		}
	}
}
void *chk_log_handler(void *unused)
{
	
	chk_result_t		*p = 0;
	chk_read_data_t		t;	
	time_t				s;
	int					len = 0;
	char				buf[1024];
	char				*path = "/var/log/traffic_guard_statistic.log";

	if (g_chk_log_conf.statistic_log) {
		path = g_chk_log_conf.statistic_log;
	} 
	FILE_LOG = fopen(path, "a");
	if (!FILE_LOG) {
		goto ERROR;
	}
	
	pthread_cleanup_push(log_exit, (void*)-1);
	for (;;)
	{
		while (!g_chk_process_status) {
			pthread_testcancel();
			sleep(1);
		}
		s = time(NULL);
		ctime_r(&s, buf);
		len = strlen(buf);
		buf[len - 1] = '\0';

		if (check_log_file_test(path, &FILE_LOG)) {
			goto ERROR;
		}
		
		p = &g_chk_conf_info.list[0];
		chk_static_list(p, &t);
		fprintf(FILE_LOG, "%s\t\t%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n", buf, 
			t.all_send_packet, t.all_send_bit, t.all_recv_packet, t.all_recv_bit,
			t.tcp_send_packet, t.tcp_send_bit, t.tcp_recv_packet, t.tcp_recv_bit,
			t.udp_send_packet, t.udp_send_bit, t.udp_recv_packet, t.udp_recv_bit,
			t.icmp_send_packet, t.icmp_send_bit, t.icmp_recv_packet, t.icmp_recv_bit,
			t.other_send_packet, t.other_send_bit, t.other_recv_packet, t.other_recv_bit);
		fflush(FILE_LOG);
		pthread_testcancel();
		sleep(g_chk_log_conf.rate);
	}
	pthread_cleanup_pop(1);
	fclose(FILE_LOG);
	return NULL;

ERROR:
	//pthread_close_other(pthread_self());
	if (FILE_LOG) {
		fclose(FILE_LOG);
		FILE_LOG = NULL;
	}
	pthread_exit((void*)-1);
}

int chk_log_process()
{
	if (!g_chk_log_conf.rate) {
		check_error_log("%s", "need log rate");
		return -1;
	}
	return 0;
}

chk_conf_hander_t chk_log =  {
	.name = "log",
	.conf = chk_conf_log_item,
	.check = chk_log_process,
	.handler = NULL,
};

