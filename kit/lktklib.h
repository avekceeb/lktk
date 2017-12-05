#ifndef LKTKLIB_H
#define LKTKLIB_H 1

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

//#include "lstate.h"

#include <syslog.h>

// TODO: get rid of it:
#define DEBUG 1

struct TLktkInfo {
    int failures;
    int parallel;
    int iterations;
    int timeout;
    char syslog;
    char strict;
    char verbose;
    char interactive;
};
typedef struct TLktkInfo LktkInfo;

LktkInfo kit;

void inject_lktklib(lua_State* L);

#define LKTK_stat 1
#define LKTK_timespec 2
#define LKTK_timeval 3
#define LKTK_sockaddr 4
#define LKTK_rlimit 5
#define LKTK_perf_event_attr 6
#define LKTK_sched_param 7
#define LKTK_sched_attr 8
#define LKTK_file_handle 9
#define LKTK_epoll_event 10
#define LKTK_flock 11

//#define LKTK_iattr;
//#define LKTK_inode;
//#define LKTK_iocb;
//#define LKTK_io_event;
//#define LKTK_iovec;
//#define LKTK_itimerspec;
//#define LKTK_itimerval;
//#define LKTK_kexec_segment;
//#define LKTK_linux_dirent;
//#define LKTK_linux_dirent64;
//#define LKTK_list_head;
//#define LKTK_mmap_arg_struct;
//#define LKTK_msgbuf;
//#define LKTK_user_msghdr;
//#define LKTK_mmsghdr;
//#define LKTK_msqid_ds;
//#define LKTK_new_utsname;
//#define LKTK_nfsctl_arg;
//#define LKTK___old_kernel_stat;
//#define LKTK_oldold_utsname;
//#define LKTK_old_utsname;
//#define LKTK_pollfd;
//#define LKTK_rlimit64;
//#define LKTK_rusage;
//#define LKTK_sel_arg_struct;
//#define LKTK_semaphore;
//#define LKTK_sembuf;
//#define LKTK_shmid_ds;
//#define LKTK_stat;
//#define LKTK_stat64;
//#define LKTK_statfs;
//#define LKTK_statfs64;
//#define LKTK_statx;
//#define LKTK___sysctl_args;
//#define LKTK_sysinfo;
//#define LKTK_timex;
//#define LKTK_timezone;
//#define LKTK_tms;
//#define LKTK_utimbuf;
//#define LKTK_mq_attr;
//#define LKTK_compat_stat;
//#define LKTK_compat_timeval;
//#define LKTK_robust_list_head;
//#define LKTK_getcpu_cache;
//#define LKTK_old_linux_dirent;
//#define LKTK_sigaltstack;
//#define LKTK_bpf_attr;

#define echo(x,...) do{\
        printf("\x1b[0;%s;1m", #x);\
        printf(__VA_ARGS__);\
        printf("\x1b[0m\n");}while(0);

#define echo_debug(...) echo(90,__VA_ARGS__)
#define echo_info(...) echo(36,__VA_ARGS__)
#define echo_error(...) echo(31,__VA_ARGS__)
#define echo_warn(...) echo(33,__VA_ARGS__)
#define echo_good(...) echo(32,__VA_ARGS__)

#if DEBUG
#define DBG(...) echo_info(__VA_ARGS__)
#else
#define DBG(...)
#endif

#define log_error(...)\
    do{echo_error(__VA_ARGS__);\
        if(kit.syslog){syslog(LOG_ERR, __VA_ARGS__);}\
    }while(0);

#define log_info(...)\
    do{if(kit.verbose){\
        echo_info(__VA_ARGS__);\
        if(kit.syslog){syslog(LOG_INFO, __VA_ARGS__);}\
    }}while(0);

#define LPOSIX_CONST(_f) do{\
    lua_pushinteger(L, _f);\
    lua_setfield(L, -2, #_f);}while(0)

#define LKTK_DATATYPE(x) do{\
    lua_pushinteger(L,LKTK_##x);\
    lua_setfield(L,-2,#x);}while(0)

#endif
