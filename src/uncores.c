/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */

#include <limits.h>
#include "uncores.h"
#include "perf.h"
#include "common.h"

int freeze_counters_cbo_all(struct __pmu_info *pmu)
{
    int i, r;

    for (i = 0; i < num_of_cbo(); i++) {
        r = perf_stop(&pmu->cbos[i].perf);
        if (r < 0) {
            fprintf(stderr, "%s perf_stop failed. cbo:%d\n", __func__, i);
            return r;
        }
    }
    return 0;
}

int unfreeze_counters_cbo_all(struct __pmu_info *pmu)
{
    int i, r;

    for (i = 0; i < num_of_cbo(); i++) {
        r = perf_start(&pmu->cbos[i].perf);
        if (r < 0) {
            fprintf(stderr, "%s perf_start failed. cbo:%d\n", __func__, i);
            return r;
        }
    }
    return 0;
}

int init_cbo(struct __uncore *unc, const uint32_t unc_idx)
{
    int ret, fd;
    ssize_t r;
    unsigned long value;
    char path[64], buf[32];

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path) - 1,
             perf_config.path_format_cbo_type,
             unc_idx);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    r = read(fd, buf, sizeof(buf) - 1);
    if (r < 0) {
        perror("read");
        close(fd);
        return -1;
    }
    close(fd);

    value = strtoul(buf, NULL, 10);
    if (value == ULONG_MAX) {
        perror("strtoul");
        return -1;
    }

    unc->perf.cpu      = (int)unc_idx;
    unc->perf.pid      = -1; /* when using uncore, pid must be -1. */
    unc->perf.group_fd = -1;
    memset(&unc->perf.attr, 0, sizeof(struct perf_event_attr));
    unc->perf.attr.type   = (uint32_t) value;
    unc->perf.attr.config = perf_config.cbo_config;
    unc->perf.attr.size   = sizeof(struct perf_event_attr);
    unc->perf.attr.inherit  = 1;
    unc->perf.attr.disabled = 1;
    unc->perf.attr.enable_on_exec = 1;
    /* when using uncore, don't set exclude_xxx flags. */

    ret = perf_init(&unc->perf);
    if (ret < 0) {
        fprintf(stderr, "%s cbo:%u init perf counter failed.\n", __func__, unc_idx);
    }

    return ret;
}

void fini_cbo(struct __uncore *unc)
{
    perf_fini(&unc->perf);
}

int init_all_cbos(struct __pmu_info *pmu)
{
    int r;
    uint32_t i, n;

    n = num_of_cbo();
    pmu->cbos = (struct __uncore *)calloc(sizeof(struct __uncore), n);
    if (pmu->cbos == NULL) {
        handle_error("malloc");
    }

    for (i = 0; i < n; i++) {
        r = init_cbo(&pmu->cbos[i], i);
        if (r < 0) {
            fprintf(stderr, "%s init_cbo failed i:%u\n", __func__, i);
            return r;
        }
    }

    // unfreese counters
    r = unfreeze_counters_cbo_all(pmu);
    if (r < 0) {
        fprintf(stderr, "%s unfreeze_counters_cbo_all failed.\n", __func__);
    }

    return r;
}

void fini_all_cbos(struct __pmu_info *pmu)
{
    uint32_t i;

    for (i = 0; i < num_of_cbo(); i++) {
        fini_cbo(&pmu->cbos[i]);
    }
    free(pmu->cbos);
}

int read_cbo_elems(struct __uncore *unc, struct __cbo_elem *elem)
{
    ssize_t r;

    r = perf_read_pmu(&unc->perf, &elem->llc_wb);
    if (r < 0) {
        fprintf(stderr, "%s perf_read_pmu failed.\n", __func__);
    }

    DEBUG_PRINT("llc_wb:%lu\n", elem->llc_wb);
    return r;
}
