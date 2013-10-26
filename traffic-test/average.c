#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "read.h"
#include "conf.h"
#include "average.h"
#include "log.h"

chk_average_conf_t g_chk_average_conf;

chk_conf_name_func_t chk_average_conf_item[] = {
		{"value",			
		 5,	
		 &g_chk_average_conf,
		 offsetof(chk_average_conf_t, value),
		 chk_set_value },
		{"ratio_1",			
		 7,		
		 &g_chk_average_conf,
		 offsetof(chk_average_conf_t, ratio_1),
		 chk_set_double},
		{"ratio_2", 		
		 7,	
		 &g_chk_average_conf,
		 offsetof(chk_average_conf_t, ratio_2),
		 chk_set_double},
		{NULL, 0, NULL, 0, NULL}
};


int check_average_handler(chk_result_t *diff, int level)
{
	int					i = 0;
	int					j = 0;
	chk_read_data_t		t;
	double				all = 0;
	double				last = 0;
	double				res = 0;
	unsigned long		*a = NULL;
	unsigned long		*p = NULL;
	
	memset(&t, 0, sizeof(chk_read_data_t));
	for (i = 0; i < diff->max; i ++) {
		a = &t.all_send_packet;
		p = &diff->array[i].all_send_packet;
		for (j = 0; j < CHK_CONF_STATISTIC_NUM; j ++) {
			*(a ++) += *(p ++);
		}
	}
	a = &t.all_send_packet;
	for (i = 0; i < CHK_CONF_STATISTIC_NUM; i ++) {
		*a = *a / diff->max;
		a ++;
	}

	if (!diff->array[diff->max].all_recv_packet) {
		goto END;
	}

	all = (double)(t.all_recv_bit + t.all_send_bit);
	last = (double)(diff->array[diff->max].all_send_bit + diff->array[diff->max].all_recv_bit);

	double b = all/last;
	double z = all - last;

	res = b * g_chk_average_conf.ratio_1[level] + z * g_chk_average_conf.ratio_2[level];
	
	if ( res >  g_chk_average_conf.value[level]) {
		system(g_chk_conf_info.cmd_string);
		check_error_log("%s", "traffic overflow, change!");
	}
END:
	memcpy(&diff->array[diff->max], &t, sizeof(chk_read_data_t));
	
	return 0;
}

int check_average_process()
{
	int				i = 0;

	for (i = 0; g_chk_average_conf.value[i] ; i ++) ;
	if (i != g_chk_conf_info.list_num) {
		check_error_log("%s", "configure item, interval num is different with value num");
		return -1;
	}
	
	for (i = 0; g_chk_average_conf.ratio_1[i] ; i ++) ;
	if (i != g_chk_conf_info.list_num) {
		check_error_log("%s", "configure item, interval num is different with ratio_1 num");
		return -1;
	}
	
	for (i = 0; g_chk_average_conf.ratio_2[i] ; i ++) ;
	if (i != g_chk_conf_info.list_num) {
		check_error_log("%s", "configure item, interval num is different with ratio_2 num");
		return -1;
	}	
	return 0;
}


chk_conf_hander_t chk_average =  {
	.name = "average",
	.conf = chk_average_conf_item,
	.check = check_average_process,
	.handler = check_average_handler,
};



