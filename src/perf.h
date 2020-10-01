/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */

#ifndef __PERF_H
#define __PERF_H

#include "types.h"

int perf_init(struct __perf_info *ctx);
ssize_t perf_read_pmu(struct __perf_info *ctx, uint64_t *value);
int perf_start(struct __perf_info *ctx);
int perf_stop(struct __perf_info *ctx);
void perf_fini(struct __perf_info *ctx);
#endif

