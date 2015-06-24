#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "read.h"
#include "conf.h"
#include "master.h"
#include "socket.h"
#include "log.h"


extern pthread_mutex_t        g_mutex_t;
extern pthread_cond_t        g_cond_t;
extern pthread_mutex_t        g_mutex_data_t;

int process_master_conf()
{
    int                i = 0;

    g_chk_conf_info.list = calloc(g_chk_conf_info.list_num + 1, sizeof(chk_result_t));
    if (!g_chk_conf_info.list) {
        check_error_log("%s", "calloc fail");
        return -1;
    }

    for (i = 0; i < g_chk_conf_info.list_num; i ++) {
        g_chk_conf_info.list[i].max = g_chk_conf_info.interval[i]/g_chk_conf_info.min_pin;
        g_chk_conf_info.list[i].cur = 0;
        g_chk_conf_info.list[i].array = calloc(g_chk_conf_info.list[i].max + 1, sizeof(chk_read_data_t));
        if (!g_chk_conf_info.list[i].array) {
            check_error_log("%s", "calloc fail");
            return -1;
        }
    }
    
    g_chk_conf_info.list[i].max = g_chk_conf_info.min_pin/g_chk_conf_info.rate;
    g_chk_conf_info.list[i].cur = 0;
    g_chk_conf_info.list[i].array = calloc(g_chk_conf_info.list[i].max + 1, sizeof(chk_read_data_t));
    if (!g_chk_conf_info.list[i].array) {
        check_error_log("%s", "calloc fail");
        return -1;
    }
    
    return 0;
}


void process_clean_data()
{
    int        i = 0;
    int     j = 0;
    for (i = 0; i < g_chk_conf_info.list_num + 1; i ++) {
        for (j = 0; j < g_chk_conf_info.list[i].max + 1; j ++) {
            memset(&g_chk_conf_info.list[i].array[j], 0, sizeof(chk_read_data_t));
        }
        g_chk_conf_info.list[i].cur = 0;
    }
}


void chk_set_array_data(chk_read_data_t *data)
{
    int                i = 0;
    double            all = 0;
    double            last = 0;
    chk_result_t    *p = 0;
    chk_read_data_t    *t = 0;

    
    for (i = 0; i < g_chk_conf_info.list_num; i ++) {
        p = &g_chk_conf_info.list[i];
        t = p->array;

        memcpy(&t[p->cur], data, sizeof(chk_read_data_t));
        p->cur ++;
        
        if (i == g_chk_conf_info.pin_level \
         && t[p->max].all_recv_bit && t[p->max].all_send_bit) {
         
             all  = (double)(data->all_recv_bit + data->all_send_bit);
            last = (double)(t[p->max].all_recv_bit + t[p->max].all_send_bit);
            if (all * g_chk_conf_info.min_ratio > last) {
                system(g_chk_conf_info.cmd_string);
                check_error_log("%s", "traffic overflow, change!");
            }
        }
        if (p->cur == p->max) {
            g_chk_conf_info.select->handler(p, i);
            p->cur = 0;
        }
    }

}



void diff_array_data(chk_result_t *st, chk_read_data_t *res)
{    
    chk_read_data_t            *u = NULL;
    chk_read_data_t            *first = NULL;
    chk_read_data_t            *last = NULL;
    unsigned long            *a = NULL;
    unsigned long            *b = NULL;
    unsigned long            *c = NULL;
    int                     i = 0;

    u = st->array;
    first = &u[st->max];
    last = &u[st->max - 1];

    if (!first->all_send_bit && !first->all_recv_bit) {
        first = &u[0];
    }

    a = &res->all_send_packet;
    b = &last->all_send_packet;
    c = &first->all_send_packet;
    
    for (i = 0; i < CHK_CONF_STATISTIC_NUM; i ++) {
        *(a ++) = *(b ++) - *(c ++);
    }
}


void master_exit(void* unused)
{
    pthread_mutex_trylock(&g_mutex_t);
    pthread_mutex_unlock(&g_mutex_t);
    
    pthread_mutex_trylock(&g_mutex_data_t);
    pthread_mutex_unlock(&g_mutex_data_t);
}
void *chk_master_handler(void* unused) 
{
    chk_result_t        *st = NULL;
    chk_read_data_t        *u = NULL;
    chk_read_data_t        t;
    int                    i;
    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    pthread_cleanup_push(master_exit, (void*)-1);
    while (1) {
        pthread_testcancel();
        pthread_mutex_lock(&g_mutex_t);
        st = &g_chk_conf_info.list[g_chk_conf_info.list_num];
        u = st->array;

        while (st->cur < st->max) {
            pthread_testcancel();
            pthread_cond_wait(&g_cond_t, &g_mutex_t);
        }
        memset(&t, 0, sizeof(chk_read_data_t));
        
        diff_array_data(st, &t);
        memcpy(&u[st->max], &u[st->max - 1], sizeof(chk_read_data_t));
        for (i = 0; i < st->max; i ++) {
            memset(&u[i], 0, sizeof(chk_read_data_t));
        }
        st->cur = 0;

        pthread_mutex_unlock(&g_mutex_t);
        pthread_mutex_lock(&g_mutex_data_t);
        chk_set_array_data(&t);
        pthread_mutex_unlock(&g_mutex_data_t);
    }
    pthread_cleanup_pop(1);
    return NULL;
}

