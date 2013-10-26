#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <fcntl.h>
#include "signal_t.h"
#include "read.h"
#include "conf.h"
#include "master.h"
#include "log.h"
#include "socket.h"
#include "average.h"
#include "version.h"

static char	*PPID_FILENAME = "/var/run/traffic-guard.ppid";
static char	*PID_FILENAME = "/var/run/traffic-guard.pid";

static char *conf_path = "/etc/traffic_guard/traffic-guard.conf";
char *err_path = "/var/log/traffic_guard_error.log";

FILE		*FILE_ERR = NULL;

pthread_t g_pthread_t[CHK_CONF_MAX_THREAD] = {0};

extern int CHK_EXITED;

void chk_start_programe();

int usage(char *name)
{
	fprintf(stdout, "%s\t -c config.conf  [options].....\n", basename(name));
	fprintf(stdout, "options:\n");
	fprintf(stdout, "\t-d  daemon mode\n");
	fprintf(stdout, "\t-h  help\n");
	fprintf(stdout, "\t-v  version\n");
	return 0;
}


int not_running(char *file)
{
    int fd = open(file, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    if (fd < 0)
    {
        check_error_log("%s", "open lock file fail");
        return -1;
    }
    if (flock(fd, LOCK_EX|LOCK_NB) < 0)
    {
        if (errno == EWOULDBLOCK)
        {
            close(fd);
            return 0;
        }
        check_error_log("can't lock %s", file);
		close(fd);
        return -1;
    }
    
    return fd;
}

void write_pid(int fd)
{
	int len = 0;
    char buf[16] = {0};
    ftruncate(fd, 0);
    len = snprintf(buf, 16, "%d", getpid());
    write(fd, buf, len);
}


void pthread_close_other(pthread_t thread_id)
{
	int	i	=	0;
	
	for (i = 1; i < CHK_CONF_MAX_THREAD; i ++) {
		if (!g_pthread_t[i]) {
			continue;
		}
		if (pthread_equal(thread_id, g_pthread_t[i])) {
			continue;
		}
		if (pthread_cancel(g_pthread_t[i])) {
			check_error_log("%s", "cancle thread fail");
			return ;
		}
	}
}


int set_daemon()
{	
	int i = 0;
	int	pid = 0;
	int fd = 0;
	int fd0, fd1, fd2;
	struct rlimit rl;

	umask(0);
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		check_error_log("%s", "getrlimit fail");
        return -1;
    }
	
    pid = fork();
    if (pid < 0) {
		check_error_log("%s", "fork fail");
        return -1;
    }
    else if (pid > 0) {
        exit(0);    
    }
    
    setsid();     
 	
    if (chdir("/") < 0) {
		check_error_log("%s", "chdir fail");
        return -1;
	}
    
    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;
	
	fd = fileno(FILE_ERR);
	if (fd == -1) 
		return -1;
    for (i = 0; i < rl.rlim_max; i ++) { 
		if (i != fd)
			close(i);
	}
    
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);
    
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
		check_error_log("%s", "open fd 0 1 2 fail");
		return -1;
	}
	return 0;
	
}


int chk_modify_conf_path()
{
	char 	dirname[1024] 	= { 0 };
	int 	dir_len 	= 0;
	int 	name_len 	= 0;
	int 	len 		= 0;

	conf_path = basename(conf_path);
	getcwd(dirname, 1024);
	dir_len = strlen(dirname);
	name_len = strlen(conf_path);
	
	char *tmp = malloc(dir_len + name_len + 1 + 1);
	if (!tmp) {
		return -1;
	}
	len = snprintf(tmp, dir_len + name_len + 1 + 1, "%s/%s", dirname, conf_path);
	if (len < 0) {
		free(tmp);
		return -1;
	}
	conf_path = tmp;
	return 0;
}



int chk_destroy_exit()
{	
	int i = 0;
	
	if (g_chk_conf_info.read_path) {
		free(g_chk_conf_info.read_path);
		g_chk_conf_info.read_path = NULL;
	}
	if (g_chk_conf_info.error_log) { 
		free(g_chk_conf_info.error_log);
		g_chk_conf_info.error_log = NULL;
	}
	if (g_chk_conf_info.cmd_string) {
		free(g_chk_conf_info.cmd_string);
		g_chk_conf_info.cmd_string = NULL;
	}
		
	if (g_chk_conf_info.list) {
		for (i = 0; i < g_chk_conf_info.list_num + 1; i ++) {
			free(g_chk_conf_info.list[i].array);
			g_chk_conf_info.list[i].array = NULL;
			g_chk_conf_info.list[i].cur = 0;
			g_chk_conf_info.list[i].max = 0; 
		}
		free(g_chk_conf_info.list);
		g_chk_conf_info.list = NULL;
	}

	if (g_chk_socket_info.ip) {
		free(g_chk_socket_info.ip);
		g_chk_socket_info.ip = NULL;
	}
	if (g_chk_socket_info.list) {
		for (i = 0; i < g_chk_socket_info.list->cur; i ++) {
			free(g_chk_socket_info.list->ptr[i]);
			g_chk_socket_info.list->ptr[i] = NULL;
		}
		g_chk_socket_info.list->cur = 0;
		g_chk_socket_info.list->max = 0;
		free(g_chk_socket_info.list);
		g_chk_socket_info.list = NULL;
	}
	g_chk_socket_info.fd = 0;
	g_chk_socket_info.port = 0;


	memset(&g_chk_socket_info, 0, sizeof(chk_socket_info_t));
	memset(&g_chk_conf_info, 0, sizeof(g_chk_conf_info_t));
	memset(&g_chk_average_conf, 0, sizeof(chk_average_conf_t));
	return 0;
}



int chk_restart_process()
{
	int			ret = 0;
	
	ret = parse_conf_file(conf_path);
	if (ret) {
		check_error_log("%s", "re-parse conf file fail");
		goto ERROR;
	}

	ret = check_conf_file();
	if (ret) {
		check_error_log("%s", "re-process select conf fail");
		goto ERROR;
	}

	if (g_chk_conf_info.error_log) {
		if (strcmp(g_chk_conf_info.error_log, err_path)) {
			fclose(FILE_ERR);
			err_path = g_chk_conf_info.error_log;
			FILE_ERR = fopen(err_path, "a");
			if (!FILE_ERR) {
				fprintf(stderr, "open error log file fail.\n");
				goto ERROR;
			}
		}
	}
	
	ret = process_master_conf();
	if (ret) {
		check_error_log("%s", "re-process master conf fail");
		goto ERROR;
	}
	chk_start_programe();
	return 0;
ERROR:
	chk_destroy_exit();
	return ret;

}

void chk_start_programe()
{
	void	*r 		= NULL;
	int		ret		= 0;

	pthread_create(&g_pthread_t[1], NULL, &chk_traffic_read, NULL);
	pthread_create(&g_pthread_t[2], NULL, &chk_master_handler, NULL);
	pthread_create(&g_pthread_t[3], NULL, &chk_log_handler, NULL);
	pthread_create(&g_pthread_t[4], NULL, &chk_udp_server, NULL);
	
	int i = 0;
	for (i = 1; i < 5; i ++) {
		pthread_join(g_pthread_t[i], &r);
		if (r !=  PTHREAD_CANCELED) {
			check_error_log("thread %d abnormally quit", i);
			ret = 1;
			break;
		}
	}

	chk_destroy_exit();
	if (!ret) {
		chk_restart_process();
	}
}



void *sig_thread(void *arg)
{
	sigset_t	*set = (sigset_t*)arg;
	int ret = 0;
	int sig;

	for (;;) {
		ret = sigwait(set, &sig);
		if (ret) {
			pthread_close_other(pthread_self());
			pthread_exit((void*)-1);
		}
		switch(sig) {
			case SIGUSR1:
				g_chk_process_status = 0;
				break;
			case SIGUSR2:
				g_chk_process_status = 1;
				break;
			case SIGHUP:
				pthread_close_other(pthread_self());
				break;
			default:
				check_error_log("recvive error signal %d", sig);
				break;
		}
	}
}


void work_process()
{
	int		ret = 0;
	if (g_chk_conf_info.error_log) {
		if (strcmp(g_chk_conf_info.error_log, err_path)) {
			fclose(FILE_ERR);
			err_path = g_chk_conf_info.error_log;
			FILE_ERR = fopen(err_path, "a");
			if (!FILE_ERR) {
				fprintf(stderr, "open error log file fail.\n");
				exit(0);
			}
		}
	}
	
	ret = not_running(PID_FILENAME);
	if (ret == 0 || ret == -1) {
		check_error_log("%s", "is already running");
		exit(0);
	}
	write_pid(ret);
	
	ret = process_master_conf();
	if (ret) {
		check_error_log("%s", "process master conf fail");
		exit(-1);
	}
	ret = 0;

	sigset_t		set;
	sigemptyset(&set);
	ret = pthread_sigmask(SIG_SETMASK, &set, NULL);
	if (ret) {
		exit(-1);
	}
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGUSR2);
	sigaddset(&set, SIGHUP);

	ret = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (ret) {
		check_error_log("%s", "block signal fail");
		exit(-1);
	}

	pthread_attr_t	attr;
	pthread_attr_init(&attr);
	
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret != 0) {
		check_error_log("%s", "pthread_attr_setdetachstate");
		exit(-1);
	}

	pthread_create(&g_pthread_t[0], &attr, &sig_thread, (void*)&set);

	pthread_attr_destroy(&attr);
	
	chk_start_programe();

	exit(0);
}


int	start_work_process()
{
	int	pid = fork();
	if (pid < 0)  {
		check_error_log("%s", "create work process fail");
		return -1;
	}
	else if (pid == 0) {
		work_process();
	} 
	return pid;
}


int main(int argc, char **args)
{
	char 			c = 0;
	int 			daemon = 0;
	int 			ret = 0;	
	pid_t			pid = 0;

	while ((c = getopt(argc, args, "vhc:d")) != -1) {
		switch (c) {
			case 'c':
				conf_path = strdup(optarg);
				break;
			case 'd':
				daemon = 1;
				break;
			case 'v':
				printf("Version: %s\n", PACKET_VERSION);
				return 0;
			case 'h':
				printf("Version: %s\n", PACKET_VERSION);
			default:
				return usage(args[0]);
		}
	}	
	FILE_ERR = fopen(err_path, "a");
	if (!FILE_ERR) {
		fprintf(stderr, "open err conf file fail.\n");
		goto ERROR;
	}

	if (*(conf_path + 0) != '/') {
		if (chk_modify_conf_path(conf_path)) {
			fprintf(stderr, "modify conf file prefix fail.\n");
			goto ERROR;
		}
	}
	
	ret = parse_conf_file(conf_path);
	if (ret) {
		fprintf(stderr, "parse conf file fail.\n");
		goto ERROR;
	}

	ret = check_conf_file();
	if (ret) {
		fprintf(stderr, "process select conf fail.\n");
		goto ERROR;
	}

	if (chk_init_signal()) {
		fprintf(stderr, "init signal fail.\n");
		goto ERROR;
	}

	if (!g_chk_conf_info.daemon) {
		g_chk_conf_info.daemon = daemon;
	}
	
	if (g_chk_conf_info.daemon) {
		ret = set_daemon();
		if (ret) {
			fprintf(stderr, "set daemon fail.\n");
			check_error_log("%s", "set deamon fail");
			goto ERROR;
		}
	}
	
	ret = not_running(PPID_FILENAME);
	if (ret == 0 || ret == -1) {
		return -1;
	}
	write_pid(ret);


	sigset_t	mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigaddset(&mask, SIGUSR2);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		goto ERROR;
	}
	
	sigemptyset(&mask);
	pid = start_work_process();
	if (pid < 0) {
		goto ERROR;
	}
	
	for (;;) {
		if (CHK_EXITED) 
			break;
		
		sigsuspend(&mask); 
		chk_signal_worker_process(pid, start_work_process);
	}

ERROR:
	chk_destroy_exit();
	return ret;
}
