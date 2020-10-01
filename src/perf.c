/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include "perf.h"

static long perf_event_open(struct perf_event_attr* event_attr, pid_t pid,
                            int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, event_attr, pid,
                   cpu, group_fd, flags);
}

int perf_init(struct __perf_info *ctx)
{
    ctx->fd = perf_event_open(&ctx->attr, ctx->pid, ctx->cpu, ctx->group_fd, ctx->flags);
    if (ctx->fd == -1) {
        perror("perf_event_open");
        return -1;
    }

    ioctl(ctx->fd, PERF_EVENT_IOC_RESET, 0);

    return 0;
}

ssize_t perf_read_pmu(struct __perf_info *ctx, uint64_t *value)
{
    /*
     * Workaround:
     *   The expected value cannot be obtained when reading continuously.
     *   This can be avoided by executing nanosleep with 0.
     */
    struct timespec zero = {0};
    nanosleep(&zero, NULL);
    ssize_t r = read(ctx->fd, value, sizeof(*value));
    if (r < 0) {
        perror("read");
    }
    return r;
}

int perf_start(struct __perf_info *ctx)
{
    if (ioctl(ctx->fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        perror("ioctl");
        return -1;
    }

    return 0;
}

int perf_stop(struct __perf_info *ctx)
{
    if (ioctl(ctx->fd, PERF_EVENT_IOC_DISABLE, 0) < 0) {
        perror("ioctl");
        return -1;
    }
    return 0;
}

void perf_fini(struct __perf_info *ctx)
{
    if (ctx->fd != -1) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}
