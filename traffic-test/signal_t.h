#ifndef _CHK_SIGNAL_T_H_T
#define _CHK_SIGNAL_T_H_T


extern int		CHK_CHILD;
extern int		CHK_STOP;
extern int		CHK_START;
extern int		CHK_RELOAD;

void chk_signal_worker_process(int pid, int (*func)(void));
int chk_init_signal();
void sig_handler(int);

#endif
