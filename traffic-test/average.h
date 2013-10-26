#ifndef _TRAFFIC_CHECK_AVERAGE_H_
#define _TRAFFIC_CHECK_AVERAGE_H_

typedef struct {
	unsigned long 	value[CHK_CONF_MAX_ITEM_NUM];
	double 	ratio_1[CHK_CONF_MAX_ITEM_NUM];
	double 	ratio_2[CHK_CONF_MAX_ITEM_NUM];
}chk_average_conf_t;

extern chk_conf_hander_t chk_average;
extern chk_average_conf_t g_chk_average_conf;

#endif
