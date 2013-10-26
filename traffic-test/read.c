#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "read.h"
#include "conf.h"
#include "socket.h"
#include "master.h"


pthread_mutex_t		g_mutex_t = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t		g_cond_t = PTHREAD_COND_INITIALIZER;
pthread_mutex_t		g_mutex_data_t = PTHREAD_MUTEX_INITIALIZER;


void chk_read_file_data(chk_read_data_t *total)
{
	int 				i = 0;
	char				s = 0;
	char				*p = NULL;
	char				*c = NULL;	
	char				line[2048] = {0};		
	unsigned long		*t = NULL;
	unsigned long		*k = NULL;
	chk_read_data_t		st;
		
	FILE *fp = popen("cat /proc/net/tc_counter", "r");
	if (!fp) {
		return;
	}
	
	while (!feof(fp)) {
		p = fgets(line, 2048, fp);
		if (!p) {
			if (ferror(fp)) {
				return;
			} else {
				break;
			}
		}
		c = line;
		while (isspace(*c)) {
			c ++;
		}
		if (!isdigit(*c)) {
			continue;
		}

		memset(&st, 0, sizeof(st));
		p = st.ip;
		while (*c != ',') {
			*p ++ = *c ++;
		}
		
		t = &st.all_send_packet;
		s = 0;
		while (*c != '\r' && *c != '\n') {
			
			c ++;
			p = c;
			while (*c != ',' && *c != '\r' && *c != '\n') {
				c ++;
			}
			s = *c;
			*c = '\0';
			*t ++ = atol(p);
			*c = s;
		}	

		t = &st.all_send_packet;
		k = &total->all_send_packet;

		for (i = 0; i < CHK_CONF_STATISTIC_NUM; i ++) {
			*(k ++) += *(t ++);
		}
	}
	pclose(fp);
	
}

void read_exit(void *arg)
{
	pthread_mutex_trylock(&g_mutex_t);
	pthread_mutex_unlock(&g_mutex_t);
	pthread_cond_broadcast(&g_cond_t);
	
	pthread_mutex_trylock(&g_mutex_data_t);
	pthread_mutex_unlock(&g_mutex_data_t);
}


void *chk_traffic_read(void *unused) {

	chk_result_t	*p_st = NULL;
	chk_read_data_t	*u = NULL;
	
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	
	pthread_cleanup_push(read_exit, (void*)-1);
	for (;;) {
		if (!g_chk_process_status)	{	
			pthread_mutex_lock(&g_mutex_data_t);
			process_clean_data();
			pthread_mutex_unlock(&g_mutex_data_t);
		}
		while (!g_chk_process_status) {
			pthread_testcancel();
			sleep(1);
		}
		pthread_mutex_lock(&g_mutex_t);
		p_st = &g_chk_conf_info.list[g_chk_conf_info.list_num];
		u = p_st->array;
		
		while (p_st->cur < p_st->max) {
			chk_read_file_data(u ++);
			p_st->cur ++;
			if (p_st->cur < p_st->max) 
				sleep(g_chk_conf_info.rate);
			else 
				break;
		}
		pthread_mutex_unlock(&g_mutex_t);
		pthread_cond_broadcast(&g_cond_t);
		sleep(g_chk_conf_info.rate);
	}
	pthread_cleanup_pop(1);
	return NULL;
}
