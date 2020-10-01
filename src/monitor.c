/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai */

#define _GNU_SOURCE
#include <sched.h>
#include <sys/syscall.h>
#include "monitor.h"
#include "pebs.h"

void disable_mon(const uint32_t target, struct __monitor* mon)
{
    mon[target].is_process = false;
    mon[target].status = MONITOR_DISABLE;
    mon[target].tgid = 0;
    mon[target].tid = 0;
    mon[target].before = &mon[target].elem[0];
    mon[target].after = &mon[target].elem[1];
    mon[target].total_delay = 0;
    mon[target].squabble_delay.tv_sec = 0;
    mon[target].squabble_delay.tv_nsec = 0;
    mon[target].injected_delay.tv_sec = 0;
    mon[target].injected_delay.tv_nsec = 0;
    mon[target].end_exec_ts.tv_sec = 0;
    mon[target].end_exec_ts.tv_nsec = 0;
    mon[target].pebs_ctx.fd     = -1;
    mon[target].pebs_ctx.pid    = -1;
    mon[target].pebs_ctx.seq    = 0;
    mon[target].pebs_ctx.rdlen  = 0;
    mon[target].pebs_ctx.seq    = 0;
    mon[target].pebs_ctx.mp     = NULL;
    mon[target].pebs_ctx.sample_period = 0;
    for (int i = 0; i < mon[target].num_of_region; i++) {
        for (int j = 0; j < 2; j++) {
            mon[target].elem[j].pebs.sample[i] = 0;
            mon[target].elem[j].pebs.total = 0;
            mon[target].elem[j].pebs.llcmiss = 0;
        }
        mon[target].region_info[i].addr = 0;
        mon[target].region_info[i].size = 0;
    }
    mon[target].num_of_region = 0;
}

int enable_mon(const uint32_t tgid, const uint32_t tid, bool is_process,
               uint64_t pebs_sample_period,
               const int32_t tnum, struct __monitor* mon)
{
    int target = -1;

    for (int i = 0; i < tnum; i++) {
        if (mon[i].tgid == tgid && mon[i].tid == tid) {
            // already exists.
            return -1;
        }
    }
    for (int i = 0; i < tnum; i++) {
        if (mon[i].status != MONITOR_DISABLE) {
            continue;
        }
        target = i;
        break;
    }
    if (target == -1) {
        // All cores are used.
        return -1;
    }

    /* set CPU affinity to not used core. */
    int s;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(mon[target].cpu_core, &cpuset);
    s = sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        if (errno == ESRCH) {
            DEBUG_PRINT("Process [%u:%u] is terminated.\n", tgid, tid);
            return -2;
        } else {
            handle_error("Failed to setaffinity");
        }
        return -1;
    }

    /* init */
    disable_mon(target, mon);
    mon[target].status = MONITOR_ON;
    mon[target].tgid = tgid;
    mon[target].tid = tid;
    mon[target].is_process = is_process;

    if (pebs_sample_period) {
        /* pebs start */
        pebs_init(&mon[target].pebs_ctx, tid, pebs_sample_period);
        DEBUG_PRINT("Process [tgid=%u, tid=%u]: enable to pebs.\n",
                    mon[target].tgid, mon[target].tid);
    }

    printf("========== Process %d[tgid=%u, tid=%u] monitoring start ==========\n",
           target, mon[target].tgid, mon[target].tid);

    return target;
}

int terminate_mon(const uint32_t tgid, const uint32_t tid,
              const int32_t tnum, struct __monitor* mon)
{
    int target = -1;

    for (int i = 0; i < tnum; i++) {
        if (mon[i].status == MONITOR_DISABLE) {
            continue;
        }
        if (mon[i].tgid != tgid || mon[i].tid != tid) {
            continue;
        }
        target = i;
        /* pebs stop */
        pebs_fini(&mon[target].pebs_ctx);

        /* Save end time */
        if (mon[target].end_exec_ts.tv_sec == 0 && mon[target].end_exec_ts.tv_nsec == 0) {
            clock_gettime(CLOCK_MONOTONIC, &mon[i].end_exec_ts);
        }
        /* display results */
        printf("========== Process %d[tgid=%u, tid=%u] statistics summary ==========\n",
               target, mon[target].tgid, mon[target].tid);
        double emulated_time = (double)(mon[target].end_exec_ts.tv_sec - mon[target].start_exec_ts.tv_sec) +
                               (double)(mon[target].end_exec_ts.tv_nsec - mon[target].start_exec_ts.tv_nsec)/1000000000;
        printf("emulated time =%lf\n", emulated_time);
        printf("total delay   =%lf\n", mon[target].total_delay);
        for (int j; j < mon[target].num_of_region; j++) {
            printf("PEBS sample %d =%lu\n", j, mon[target].before->pebs.sample[j]);
        }

        /* init */
        disable_mon(target, mon);
        break;
    }

    return target;
}

int set_region_info_mon(struct __monitor *mon, const int nreg, struct __region_info *ri)
{
    int i;

    mon->num_of_region = nreg;
    for (i = 0; i < nreg; i++) {
        mon->region_info[i].addr = ri[i].addr;
        mon->region_info[i].size = ri[i].size;
        DEBUG_PRINT("  region info[%d]: addr=%lx, size=%lx\n", i, ri[i].addr, ri[i].size);
    }

    return 0;
}

void initMon(const int tnum, cpu_set_t *use_cpuset, struct __monitor** monp, const int nmem)
{
    int i, j;
    struct __monitor *mon;

    mon = (struct __monitor *)calloc(sizeof(struct __monitor), tnum);
    if (mon == NULL) {
        handle_error("calloc");
    }
    *monp = mon;

    /* init mon */
    for (i = 0; i < tnum; i++) {
        disable_mon(i, mon);

        int cpucnt = 0;
        int cpuid = 0;
        for (cpuid = 0; cpuid < num_of_cpu(); cpuid++) {
            if (CPU_ISSET(cpuid, use_cpuset)) {
                if (i == cpucnt) {
                    mon[i].cpu_core = cpuid;
                    break;
                }
                cpucnt++;
            }
        }

        for (j = 0; j < 2; j++) {
            mon[i].elem[j].cpus = (struct __cpu_elem *)calloc(sizeof(struct __cpu_elem), num_of_cpu());
            if (mon[i].elem[j].cpus == NULL) {
                handle_error("calloc");
            }
            mon[i].elem[j].cbos = (struct __cbo_elem *)calloc(sizeof(struct __cbo_elem), num_of_cbo());
            if (mon[i].elem[j].cbos == NULL) {
                handle_error("calloc");
            }
            mon[i].elem[j].pebs.sample = (uint64_t *)calloc(sizeof(uint64_t), nmem);
            if (mon[i].elem[j].pebs.sample == NULL) {
                handle_error("calloc");
            }
        }
        mon[i].region_info = (struct __region_info *)calloc(sizeof(struct __region_info), nmem);
        if (mon[i].region_info == NULL) {
            handle_error("calloc");
        }
    }
}

void freeMon(const int tnum, struct __monitor** monp)
{
    int i, j;
    struct __monitor *mon = *monp;

    for (i = 0; i < tnum; i++) {
        for (j = 0; j < 2; j++) {
            free(mon[i].elem[j].cpus);
            free(mon[i].elem[j].cbos);
            free(mon[i].elem[j].pebs.sample);
        }
        free(mon[i].region_info);
    }
    free(mon);
}

void stop_all_mons(const uint32_t processes, struct __monitor* mons)
{
    for(uint32_t i = 0; i < processes; ++i) {
        if (mons[i].status == MONITOR_ON) {
            stop_mon(&mons[i]);
        }
    }
}

void run_all_mons(const uint32_t processes, struct __monitor* mons)
{
    for(uint32_t i = 0; i < processes; ++i) {
        if (mons[i].status == MONITOR_OFF) {
            run_mon(&mons[i]);
        }
    }
}

void stop_mon(struct __monitor* mon)
{
    int ret = -1;

    if (mon->is_process) {
        // In case of process, use SIGSTOP.
        DEBUG_PRINT("Send SIGSTOP to pid=%u\n", mon->tid);
        ret = kill(mon->tid, SIGSTOP);
    } else {
        // Use SIGUSR1 instead of SIGSTOP.
        // When the target thread receives SIGUSR1, it must stop until it receives SIGCONT.
        DEBUG_PRINT("Send SIGUSR1 to tid=%u(tgid=%u)\n", mon->tid, mon->tgid);
        ret = syscall(SYS_tgkill, mon->tgid, mon->tid, SIGUSR1);
    }

    if (ret == -1) {
        if (errno == ESRCH) {
            // in this case process or process group does not exist.
            // It might be a zombie or has terminated execution.
            mon->status = MONITOR_TERMINATED;
            DEBUG_PRINT("Process [%u:%u] is terminated.\n", mon->tgid, mon->tid);
        }
        else if (errno == EPERM) {
            mon->status = MONITOR_NOPERMISSION;
            handle_error("Failed to signal to any of the target processes. Due to does not have permission. \n \
                    It might have wrong result.");
        }
    }
    else {
        mon->status = MONITOR_OFF;
        DEBUG_PRINT("Process [%u:%u] is stopped.\n", mon->tgid, mon->tid);
    }
}

void run_mon(struct __monitor* mon)
{
    DEBUG_PRINT("Send SIGCONT to tid=%u(tgid=%u)\n", mon->tid, mon->tgid);
    if (syscall(SYS_tgkill, mon->tgid, mon->tid, SIGCONT) == -1) {
        if (errno == ESRCH) {
            // in this case process or process group does not exist.
            // It might be a zombie or has terminated execution.
            mon->status = MONITOR_TERMINATED;
            DEBUG_PRINT("Process [%u:%u] is terminated.\n", mon->tgid, mon->tid);
        }
        else if (errno == EPERM) {
            mon->status = MONITOR_NOPERMISSION;
            handle_error("Failed to signal to any of the target processes. Due to does not have permission. \n \
                    It might have wrong result.");
        }
    }
    else {
        mon->status = MONITOR_ON;
        DEBUG_PRINT("Process [%u:%u] is running.\n", mon->tgid, mon->tid);
    }
}

bool check_continue_mon(const uint32_t target, const struct __monitor* mons, const struct timespec w)
{
    /* This equation for original one. but it causes like 45ms-> 60ms */
    // calculated delay : 45ms
    // actual elapesed time : 60ms (default epoch: 20ms)
    if (mons[target].wasted_delay.tv_sec > mons[target].injected_delay.tv_sec ||
        (mons[target].wasted_delay.tv_sec >= mons[target].injected_delay.tv_sec &&
         mons[target].wasted_delay.tv_nsec >= mons[target].injected_delay.tv_nsec)) {
        return true;
    }
    return false;
}

void clear_mon_time(struct timespec* time)
{
    time->tv_sec = 0;
    time->tv_nsec = 0;
}

bool check_all_mons_terminated(const uint32_t processes, struct __monitor* mons)
{
    bool _terminated = true;
    for(uint32_t i = 0; i < processes ; ++i) {
        if (mons[i].status == MONITOR_ON || mons[i].status == MONITOR_OFF) {
            _terminated = false;
        } else if (mons[i].status != MONITOR_DISABLE) {
            if (terminate_mon(mons[i].tgid, mons[i].tid, processes, mons) < 0) {
                handle_error("Failed to terminate monitor");
                exit(1);
            }
        }
    }
    return _terminated;
}

