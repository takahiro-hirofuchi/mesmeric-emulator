/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai */

#ifndef __PEBS_H
#define __PEBS_H
#include <linux/perf_event.h>
#include "common.h"
#include "types.h"

struct pebs_context {
	int           fd;
	int           pid;
	uint64_t      sample_period;
	uint32_t      seq;
	size_t        rdlen;
	size_t        mplen;
	struct perf_event_mmap_page *mp;
};

int pebs_init(struct pebs_context *, pid_t, uint64_t);
int pebs_read(struct pebs_context *, const int,  struct __region_info *, struct __pebs_elem *);
int pebs_start(struct pebs_context *);
int pebs_stop(struct pebs_context *);
int pebs_fini(struct pebs_context *);

#endif
