#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include "read.h"
#include "conf.h"
#include "average.h"
#include "log.h"
#include "socket.h"

g_chk_conf_info_t g_chk_conf_info;

int check_master_conf();

static chk_conf_name_func_t chk_conf_item[] = {
        {"error_log",        
         9,    
         &g_chk_conf_info,
         offsetof(g_chk_conf_info_t, error_log),
         chk_set_path },
        {"use",
          3,
          &g_chk_conf_info,
          0,
          chk_set_algorithm},
        {"daemon",            
          6,        
          &g_chk_conf_info,
          offsetof(g_chk_conf_info_t, daemon),
          chk_set_switch },
        {"rate",            
          4,        
          &g_chk_conf_info,
          offsetof(g_chk_conf_info_t, rate),
          chk_set_time },
        {"read_data",        
         9,    
         &g_chk_conf_info,
         offsetof(g_chk_conf_info_t, read_path),
         chk_set_path },
        {"cmd_string",        
         10,    
         &g_chk_conf_info,
         offsetof(g_chk_conf_info_t, cmd_string),
         chk_set_path },
        {"interval",        
         8,        
         &g_chk_conf_info,
         offsetof(g_chk_conf_info_t, interval),
         chk_set_time },
        {"min_pin",        
         7,        
         &g_chk_conf_info,
         offsetof(g_chk_conf_info_t, min_pin),
         chk_set_time }, 
        {"pin_level",
         9,
         &g_chk_conf_info,
         offsetof(g_chk_conf_info_t, pin_level),
         chk_set_num },
        {"min_ratio",        
         9,        
         &g_chk_conf_info,
         offsetof(g_chk_conf_info_t, min_ratio),
         chk_set_double },
        {NULL, 0, NULL, 0, NULL}
};


chk_conf_hander_t chk_conf_master = {
    .name     =  "master_conf",
    .conf    = chk_conf_item,
    .check = check_master_conf,
    .handler = NULL,
};

chk_conf_hander_t  chk_NULL = {
    .name    =  "NULL",
    .conf    = NULL,
    .check    = NULL,
    .handler= NULL,
};


static chk_conf_hander_t *chk_conf_hander[] = {
    &chk_conf_master, 
    &chk_average,
    &chk_log,
    &chk_socket,
    &chk_NULL
};

int chk_set_path(chk_conf_value_array *map, char *conf, int offset) 
{
    
    char        **p = NULL;
    int            len = 0;
        
    if (map->num != 1) {
        check_error_log("configure item, %s need only one value", map->name);
        return -1;
    }

    p = (char**)(conf + offset);
    len = strlen(map->value[0]);
    if (!len) {
        check_error_log("configure item, %s value should have string", map->name);
        return -1;
    }
    *p = malloc(len + 1);
    if (!*p) {
        check_error_log("%s",  "configure item, malloc fail");
        return -1;
    }
    memcpy(*p, map->value[0], len);
    (*p)[len] = '\0';
    
    return 0;
}

void *chk_push_array(chk_array_t *a)
{
    void **tmp = NULL;
    void *ret = NULL;
    if (a->cur == a->max) {
        tmp = calloc(a->max + CHK_CONF_MAX_ITEM_NUM, sizeof(void*));
        if (a->ptr) {
            return NULL;
        }
        if (a->cur) {
            memcpy(tmp, a->ptr, a->max * sizeof(void*));
            free(a->ptr);
        }
        a->ptr = tmp;
        a->max += CHK_CONF_MAX_ITEM_NUM;
    }
    ret = &(a->ptr[a->cur]);
    a->cur ++;
    return ret;
}

int chk_set_list(chk_conf_value_array *map, char *conf, int offset) 
{
    
    chk_array_t        **p = NULL;
    int            len = 0;
    int            i = 0;
    char        **t = NULL;
    
    if (map->num < 1) {
        check_error_log("configure item, %s need at least one value", map->name);
        return -1;
    }
    
    p = (chk_array_t**)(conf + offset);
    if (!(*p)) {
        *p = calloc(1, sizeof(chk_array_t));
        if (!*p) {
            check_error_log("%s", "malloc fail");
            return -1;
        }
    } 
    for (i = 0; i < map->num; i ++) {
        
        len = strlen(map->value[i]);
        if (!len) {
            check_error_log("configure item, %s value should have string", map->name);
            return -1;
        }
        t = chk_push_array(*p);
        if (!t) {
            return -1;
        }
        *t = malloc(len + 1);
        if (!*t) {
            check_error_log("%s",  "configure item, malloc fail");
            return -1;
        }
        memcpy(*t, map->value[i], len);
        (*t)[len] = '\0';
    }
    return 0;
}


int chk_set_switch(chk_conf_value_array *map, char *conf, int offset) 
{
    
    int            *p = NULL;
    int            len = 0;
        
    if (map->num != 1) {
        check_error_log("configure item, %s need only one value", map->name);
        return -1;
    }

    p = (int*)(conf + offset);
    len = strlen(map->value[0]);
    if (!len) {
        check_error_log("configure item, %s value should have string", map->name);
        return -1;
    }

    if (!strncasecmp(map->value[0], "on", 2)) {
        *p = 1;
    } else if (!strncasecmp(map->value[0], "off", 3)) {
        *p = 0;
    } else {
        check_error_log("configure item, %s value should be on or off", map->name);
        return -1;
    }
    
    return 0;
}


int chk_set_algorithm(chk_conf_value_array *map, char *conf, int offset) 
{
    int                    i = 0;
    int                    len = 0;
    char                *s = NULL;
    g_chk_conf_info_t    *master_conf = (g_chk_conf_info_t*)conf;
        
    if (map->num != 1) {
        check_error_log("configure item, %s need more value", map->name);
        return -1;
    }

    len = strlen(map->value[0]);
    if (!len) {
        check_error_log("configure item, %s value should have string", map->name);
        return -1;
    }
    for (i = 0; chk_conf_hander[i]->conf; i ++) {
        s = chk_conf_hander[i]->name;
        if (!strcasecmp(map->value[0], s)) {
            master_conf->select = chk_conf_hander[i];
            break;
        }
    }
    if (!master_conf->select) {
        check_error_log("configure item, %s value is invalid", map->name);
        return -1;
    }
    return 0;
}


int chk_set_num(chk_conf_value_array *map, char *conf, int offset) 
{
    
    int                *p = NULL;
    int                len = 0;
    int                i = 0;
        
    if (map->num < 1) {
        check_error_log("configure item, %s need more value", map->name);
        return -1;
    }

    p = (int*)(conf + offset);
    
    for (i = 0; map->value[i]; i ++) {
        len = strlen(map->value[i]);
        if (!len) {
            check_error_log("configure item, %s value should have string", map->name);
            return -1;
        }
        *(p + i) = atoi(map->value[i]);
    }
    return 0;
}



int chk_set_double(chk_conf_value_array *map, char *conf, int offset) 
{
    
    double                *p = NULL;
    int                    len = 0;
    int                    i = 0;
        
    if (map->num < 1) {
        check_error_log("configure item, %s need more value", map->name);
        return -1;
    }

    p = (double*)(conf + offset);
    
    for (i = 0; map->value[i]; i ++) {
        len = strlen(map->value[i]);
        if (!len) {
            check_error_log("configure item, %s value should have string", map->name);
            return -1;
        }
        *(p + i) = atof(map->value[i]);
    }
    return 0;
}

int chk_set_value(chk_conf_value_array *map, char *conf, int offset) 
{
    
    unsigned long    *p = NULL;
    int                len = 0;
    int                rate = 0;
    int                i = 0;
        
    if (map->num < 1) {
        check_error_log("configure item, %s need more value", map->name);
        return -1;
    }

    p = (unsigned long*)(conf + offset);
    
    for (i = 0; map->value[i]; i ++) {
        len = strlen(map->value[i]);
        switch(map->value[i][len - 1]) {
            case 'm':
            case 'M':
                rate = 1024 * 1024 * 8;
                break;
            case 'K':
            case 'k':
                rate = 1024 * 8;
                break;
            case 'B':
                rate = 8;
                break;
            case 'b':
                rate = 1;
            default:
                check_error_log("configure item, %s value should have h/m/s", map->name);
                return -1;
        }
        if (!len) {
            check_error_log("configure item, %s value should have string", map->name);
            return -1;
        }
        *(p + i) = atol(map->value[i]) * rate;
    }
    return 0;
}



int chk_set_time(chk_conf_value_array *map, char *conf, int offset) 
{
    
    int                *p = NULL;
    int                len = 0;
    int                rate = 0;
    int                i = 0;
        
    if (map->num < 1) {
        check_error_log("configure item, %s need more value", map->name);
        return -1;
    }

    p = (int*)(conf + offset);
    
    for (i = 0; map->value[i]; i ++) {
        len = strlen(map->value[i]);
        switch(map->value[i][len - 1]) {
            case 'H':
            case 'h':
                rate = 1 * 60 * 60;
                break;
            case 'm':
            case 'M':
                rate = 1 * 60;
                break;
            case 's':
            case 'S':
                rate = 1;
                break;
            default:
                check_error_log("configure item, %s value should have h/m/s", map->name);
                return -1;
        }
        if (!len) {
            check_error_log("configure item, %s value should have string", map->name);
            return -1;
        }
        *(p + i) = atoi(map->value[i]) * rate;
    }
    return 0;
}

int mapping (chk_conf_value_array *map) {

    int                        i = 0;
    int                        j = 0;
    chk_conf_name_func_t    *st = NULL;

    for (i = 0;chk_conf_hander[i]->conf; i ++) {
        st = chk_conf_hander[i]->conf;
        for (j = 0; st[j].name; j ++) {
            if (st[j].len != map->len) {
                continue;
            }
            if (!strcasecmp(st[j].name, map->name)) {
                return st[j].func(map, (char*)st[j].conf, st[j].offset);
            }
        }
    }
    return 0;
}

int parse_line(char *s) {

    char                    *c = s;
    chk_conf_value_array     map;
    
    memset(&map, 0, sizeof(chk_conf_value_array));
    
    map.name = c;
    while (!isspace(*c) && *c != ';' ) {
        c ++;
        map.len ++;
    }
    if (*c != ' ' && *c != '\t') {
        check_error_log("%s", "configure item, need value");
        return -1;
    }
    *c ++ = '\0';
    
    int i = 0;
    for (i = 0; i < CHK_CONF_MAX_ITEM_NUM; i ++) {
        map.value[i] = NULL;
    }
    
    while (*c != ';' && *c != '\0' && *c != '\r' && *c != '\n') {

        while (*c == ' ' || *c == '\b') {
            c ++;
        }
        if (map.num >= CHK_CONF_MAX_ITEM_NUM) {
            return -1;
        }
        
        if (*c == '\"') {
            c ++;
            map.value[map.num ++] = c;
            
            while (*c != '\"' && *c != '\r' && *c != '\n' && *c != '\0') 
                c ++;
            
            if (*c == '\"') 
                *c ++ = '\0';
        } else {
            map.value[map.num ++] = c;
            while (!isspace(*c) && *c != ';') 
                c ++;
        }
        if (*c == ' ' || *c == '\t' || *c == '\"') {
            *c ++ = '\0';
        } else if (*c == ';'){
            *c = '\0';
            break;
        } else {
            check_error_log("%s", "configure item, need ;");
            return -1;
        }
    }
    
    return mapping(&map);
}


int parse_conf_file(char *conf_name){

    FILE            *fp = NULL;
    int                p = 0;
    char            *s = NULL;
    char              line[2048] = {0};
    
    fp = fopen(conf_name, "r");
    if (!fp) {
        check_error_log("%s", "open conf file fail");
        goto ERROR;
    }

    while (!feof(fp)) {
        s = fgets(line, 2048, fp);
        if (!s) {
            if (ferror(fp)) {
                goto ERROR;
            } else {
                break;
            }
        }
        s = line;
        
        while (isspace(*s)) {
            s ++;
        }

        if (*s == 0 || *s == '#' || (*s == '/' && *(s + 1) == '/')) {
            continue;
        }

        p = parse_line(s);
        if (p) {
            goto ERROR;
        }
    }

    fclose(fp);
    
    return 0;
ERROR:
    if (fp) 
        fclose(fp);
    return -1;
    
}


int check_master_conf()
{
    int i = 0;
    if (!g_chk_conf_info.rate \
     || ! g_chk_conf_info.min_pin \
     || !g_chk_conf_info.select \
     || !g_chk_conf_info.read_path  \
     || !g_chk_conf_info.cmd_string \
     || g_chk_conf_info.min_pin < g_chk_conf_info.rate) {
        check_error_log("%s", "check master conf fail");
        return -1;
    }
    
    for (i = 0; g_chk_conf_info.interval[i]; i ++); 
    g_chk_conf_info.list_num = i;

    return 0;
    
}



int check_conf_file()
{
    int                        i = 0;
    int                        ret = 0;

    for (i = 0;chk_conf_hander[i]->conf; i ++) {
        if (!chk_conf_hander[i]->check) {
            continue;
        }
        ret = chk_conf_hander[i]->check();
        if (ret) {
            check_error_log("%s", "check conf item fail");
            return -1;
        }
    }
    return 0;
}


