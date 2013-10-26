#ifndef _TRAFFIC_CHECK_LOG_H_
#define _TRAFFIC_CHECK_LOG_H_


#define MAX_LOG_SIZE  (1024 * 1024 * 500)

extern chk_conf_hander_t chk_log;
void *chk_log_handler(void *unused);

void check_error_log(char *fmt, ...);


#endif
