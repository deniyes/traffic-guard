#ifndef _TRAFFIC_CHECK_CONF_H_
#define _TRAFFIC_CHECK_CONF_H_


#define CHK_CONF_MAX_THREAD		(8)

#define CHK_CONF_MAX_ITEM_NUM   (8)
#define CHK_CONF_STATISTIC_NUM  (20)


typedef struct {
	char					*name;
	int						len;
	char					*value[CHK_CONF_MAX_ITEM_NUM];
	int						num;	
}chk_conf_value_array;


typedef struct {
	char 					*name;
	int  					len;
	void 					*conf;
	int  					offset;
	int (*func)(chk_conf_value_array *map, char *conf, int offset);
}chk_conf_name_func_t;

typedef struct {
	int		max;
	int		cur;
	chk_read_data_t	*array;
}chk_result_t;


typedef struct {
	char					*name;
	chk_conf_name_func_t	*conf;
	int						(*check)();
	int						(*handler)(chk_result_t *, int);
}chk_conf_hander_t;



typedef struct {
	int						daemon;
	
	int						rate;

	int						min_pin;
	int						pin_level;
	int						list_num;

	
	double					min_ratio;
	
	chk_result_t			*list;
	
	chk_conf_hander_t		*select;
	char					*error_log;
	char					*read_path;
	char					*cmd_string;

	int 					interval[CHK_CONF_MAX_ITEM_NUM];
}g_chk_conf_info_t;


extern g_chk_conf_info_t g_chk_conf_info;
extern char *err_path;

int chk_set_path(chk_conf_value_array *map, char *conf, int offset);
int chk_set_list(chk_conf_value_array *map, char *conf, int offset);
int chk_set_switch(chk_conf_value_array *map, char *conf, int offset);
int chk_set_num(chk_conf_value_array *map, char *conf, int offset);
int chk_set_double(chk_conf_value_array *map, char *conf, int offset);
int chk_set_algorithm(chk_conf_value_array *map, char *conf, int offset);
int chk_set_time(chk_conf_value_array *map, char *conf, int offset);
int chk_set_value(chk_conf_value_array *map, char *conf, int offset);

int parse_conf_file(char *conf_name);
int check_conf_file();
int process_master_conf();
void check_get_time(char *buf);

void pthread_close_other(pthread_t thread_id);

#endif
