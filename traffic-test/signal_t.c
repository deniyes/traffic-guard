#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "signal_t.h"
#include "read.h"
#include "conf.h"
#include "socket.h"

typedef struct chk_signal_s
{
	int		sig_no;
	char	*sig_name;
	void	(*sig_handler)(int);
	
}chk_signal_t;

static chk_signal_t  g_chk_signals[] = {
		{SIGUSR1, "SIGUSR1", sig_handler},
		{SIGUSR2, "SIGUSR2", sig_handler},	
		{SIGCHLD, "SIGCHLD", sig_handler},
		{SIGHUP,  "SIGHUP", sig_handler},
		{0, NULL, NULL}
};

int		CHK_CHILD 	= 0;
int		CHK_STOP 	= 0;
int		CHK_START = 0;
int		CHK_RELOAD = 0;
int		CHK_EXITED  = 0;


int chk_init_signal()
{
	struct sigaction 	sa;
	int					i = 0;

	for (i = 0; g_chk_signals[i].sig_no; i ++) {
		memset(&sa, 0, sizeof(sigaction));
		sigemptyset(&sa.sa_mask);
		sa.sa_handler = g_chk_signals[i].sig_handler;
		sa.sa_flags = 0;
		if (sigaction(g_chk_signals[i].sig_no, &sa, NULL) == -1) {
			return -1;
		}
	}
	return 0;
}



void chk_signal_worker_process(int pid, int (*func)(void)) 
{
	int 	status = 0;
	int 	ret		= 0;
	if (CHK_START) {
		kill(pid, SIGUSR2);
		CHK_START = 0;
		g_chk_process_status= 1;

	} else if (CHK_STOP) {
		kill(pid, SIGUSR1);
		CHK_STOP = 0;
		g_chk_process_status = 0;
	
	} else if (CHK_RELOAD) {
		kill(pid, SIGHUP);
		CHK_RELOAD = 0;
		
	} else if (CHK_CHILD) {
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) {
			sleep(1);
			func();
		}
		ret = WEXITSTATUS(status);
		if (ret) {
			sleep(1);
			func();
		} else {
			CHK_EXITED = 1;
		}
	} 	
}


void sig_handler(int signo) 
{
	switch(signo) {
		case SIGUSR1:
			CHK_STOP = 1;
			break;
		case SIGUSR2:
			CHK_START = 1;
			break;
		case SIGHUP:
			CHK_RELOAD = 1;
			break;
		case SIGCHLD:
			CHK_CHILD = 1;
	}
}


