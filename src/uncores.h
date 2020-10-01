/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */

#ifndef __UNCORES_H
#define __UNCORES_H

#include "types.h"
#include <dirent.h>
#include <string.h>
#include "common.h"

int freeze_counters_cbo_all(struct __pmu_info *pmu);
int unfreeze_counters_cbo_all(struct __pmu_info *pmu);
int init_cbo(struct __uncore *unc, const uint32_t unc_idx);
void fini_cbo(struct __uncore *unc);
int init_all_cbos(struct __pmu_info *pmu);
void fini_all_cbos(struct __pmu_info *pmu);
int read_cbo_elems(struct __uncore *unc, struct __cbo_elem *elem);
#endif

