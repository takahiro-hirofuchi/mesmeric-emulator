/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */

#ifndef __INCORES_H
#define __INCORES_H
#include "types.h"

typedef union CPUID_INFO
{
    int array[4];
    struct { unsigned int eax, ebx, ecx, edx; } reg;
} CPUID_INFO;

bool get_cpu_info(struct __cpu_info *);
int start_all_pmcs(struct __pmu_info *pmu);
int stop_all_pmcs(struct __pmu_info *pmu);
int start_pmc(struct __incore *inc);
int stop_pmc(struct __incore *inc);
int init_all_dram_rds(struct __incore *inc, const pid_t pid, const int cpu);
int init_cpu_l2stall(struct __incore *inc, const pid_t pid, const int cpu);
int init_cpu_llcl_hits(struct __incore *inc, const pid_t pid, const int cpu);
int init_cpu_llcl_miss(struct __incore *inc, const pid_t pid, const int cpu);
int init_pmc(struct __incore *inc, const pid_t pid, const int cpu);
void fini_pmc(struct __incore *inc);
int init_all_pmcs(struct __pmu_info *pmu, const pid_t pid);
void fini_all_pmcs(struct __pmu_info *pmu);
int read_cpu_elems(struct __incore *inc, struct __cpu_elem *cpu_elem);

#endif
