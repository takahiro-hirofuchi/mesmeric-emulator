/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */

/*
   Copyright (c) 2009-2013, Intel Corporation
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// written by Roman Dementiev
//

#ifndef __TYPES_H
#define __TYPES_H

#define _XOPEN_SOURCE 700
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <fnmatch.h>

struct emul_nvm_latency {
    double read;
    double write;
};

struct __perf_info {
    int   fd;
    int   group_fd;
    int   cpu;
    pid_t pid;
    unsigned long flags;
    struct perf_event_attr attr;
};

struct __cbo_elem {
    uint64_t llc_wb;
};

struct __cpu_elem {
    uint64_t all_dram_rds;
    uint64_t cpu_l2stall_t;
    uint64_t cpu_llcl_hits;
    uint64_t cpu_llcl_miss;
};

struct __pebs_elem {
    uint64_t total;
    uint64_t llcmiss;
    uint64_t *sample;
};

struct __cpu_info {
    uint32_t max_cpuid;
    uint32_t cpu_family;
    uint32_t cpu_model;
    uint32_t cpu_stepping;
};

struct __elem {
    struct __cpu_info cpuinfo;
    struct __cbo_elem *cbos;
    struct __cpu_elem *cpus;
    struct __pebs_elem pebs;
};

struct __uncore {
    uint32_t unc_idx;
    struct __perf_info perf;
};

struct __incore {
    struct __perf_info perf[4];
};

struct __pmu_info {
    struct __uncore *cbos;
    struct __incore *cpus;
};

struct __region_info {
    uint64_t addr;
    uint64_t size;
};

int num_of_cpu(void);
int num_of_cbo(void);
double cpu_frequency(void);
int detect_model(const uint32_t);

struct __perf_configs {
    const char *path_format_cbo_type;
    uint64_t cbo_config;
    uint64_t all_dram_rds_config;
    uint64_t all_dram_rds_config1;
    uint64_t cpu_l2stall_config;
    uint64_t cpu_llcl_hits_config;
    uint64_t cpu_llcl_miss_config;
};

struct __model_context {
    uint32_t model;
    struct __perf_configs perf_conf;
};

extern struct __perf_configs perf_config;

#endif
