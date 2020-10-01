/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai */

#ifndef __MONITOR_H
#define __MONITOR_H
#include "types.h"
#include "common.h"
#include "pebs.h"
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sched.h>

enum MONITOR_STATUS {
    MONITOR_OFF = 0,
    MONITOR_ON = 1,
    MONITOR_TERMINATED = 2,
    MONITOR_NOPERMISSION = 3,
    MONITOR_DISABLE = 4,
    MONITOR_UNKNOWN = 0xff
};

struct __monitor {
    pid_t tgid;
    pid_t tid;
    uint32_t cpu_core;
    char status;
    struct timespec injected_delay;
    struct timespec wasted_delay;
    struct timespec squabble_delay;
    struct __elem elem[2];
    struct __elem *before, *after;
    double total_delay;
    struct timespec start_exec_ts, end_exec_ts;
    bool is_process;
    int num_of_region;
    struct __region_info *region_info;
    struct pebs_context pebs_ctx;
};

void disable_mon(const uint32_t, struct __monitor*);
int enable_mon(const uint32_t,  const uint32_t, bool, uint64_t, const int32_t, struct __monitor*);
int terminate_mon(const uint32_t, const uint32_t, const int32_t, struct __monitor*);
int set_region_info_mon(struct __monitor *, const int, struct __region_info *);
void initMon(const int, cpu_set_t *, struct __monitor**, const int);
void freeMon(const int, struct __monitor**);
void stop_all_mons(const uint32_t, struct __monitor*);
void run_all_mons(const uint32_t, struct __monitor*);
void stop_mon(struct __monitor*);
void run_mon(struct __monitor*);
bool check_continue_mon(const uint32_t, const struct __monitor*, const struct timespec);
void clear_mon_time(struct timespec*);
bool check_all_mons_terminated(const uint32_t, struct __monitor*);
#endif

