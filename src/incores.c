/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */

#include "incores.h"
#include "perf.h"
#include "common.h"
#include <string.h>

void pcm_cpuid(const unsigned leaf, CPUID_INFO* info)
{
    __asm__ __volatile__ ("cpuid" : \
                          "=a" (info->reg.eax),
                          "=b" (info->reg.ebx),
                          "=c" (info->reg.ecx),
                          "=d" (info->reg.edx) : "a" (leaf));
}

bool get_cpu_info(struct __cpu_info* cpu_info)
{
    char buffer[1024];
    union {
        char cbuf[16];
        int ibuf[16/sizeof(int)];
    } buf;
    CPUID_INFO cpuinfo;

    pcm_cpuid(0, &cpuinfo);

    memset(buffer, 0, 1024);
    memset(buf.cbuf, 0, 16);
    buf.ibuf[0] = cpuinfo.array[1];
    buf.ibuf[1] = cpuinfo.array[3];
    buf.ibuf[2] = cpuinfo.array[2];

    if (strncmp(buf.cbuf, "GenuineIntel", 4 * 3) != 0)
    {
        fprintf(stderr, "We only Support Intel CPU\n");
        return false;
    }

    cpu_info->max_cpuid = cpuinfo.array[0];

    pcm_cpuid(1, &cpuinfo);
    cpu_info->cpu_family   = (((cpuinfo.array[0]) >> 8) & 0xf) |
                              ((cpuinfo.array[0] & 0xf00000) >> 16);
    cpu_info->cpu_model    = (((cpuinfo.array[0]) & 0xf0) >> 4) |
                              ((cpuinfo.array[0] & 0xf0000) >> 12);
    cpu_info->cpu_stepping = cpuinfo.array[0] & 0x0f;

    if (cpuinfo.reg.ecx & (1UL << 31UL)) {
        fprintf(stderr,
                "Detected a hypervisor/virtualization technology. "
                "Some metrics might not be available due to configuration "
                "or availability of virtual hardware features.\n");
    }

    if (cpu_info->cpu_family != 6)
    {
        fprintf(stderr,
                "It doesn't support this CPU. CPU Family: %u\n",
                cpu_info->cpu_family);
        return false;
    }

#ifdef DEBUG
    DEBUG_PRINT("MAX_CPUID=%u, CPUFAMILY=%u, CPUMODEL=%u, CPUSTEPPING=%u\n",
                cpu_info->max_cpuid,
                cpu_info->cpu_family,
                cpu_info->cpu_model,
                cpu_info->cpu_stepping);
#endif

    return true;
}

int start_all_pmcs(struct __pmu_info *pmu)
{
    /* enable all pmcs to count */
    int i, r;

    for (i = 0; i < num_of_cpu(); i++) {
        r = start_pmc(&pmu->cpus[i]);
        if (r < 0) {
            fprintf(stderr, "%s start_pmc failed. cpu:%d\n", __func__, i);
            return r;
        }
    }
    return 0;
}


int stop_all_pmcs(struct __pmu_info *pmu)
{
    /* disable all pmcs to count */
    int i, r;

    for (i = 0; i < num_of_cpu(); i++) {
        r = stop_pmc(&pmu->cpus[i]);
        if (r < 0) {
            fprintf(stderr, "%s stop_pmc failed. cpu:%d\n", __func__, i);
            return r;
        }
    }
    return 0;
}

int start_pmc(struct __incore *inc)
{
    int i, r;

    for (i = 0; i < 4; i++) {
        r = perf_start(&inc->perf[i]);
        if (r < 0) {
            fprintf(stderr, "%s perf_start failed. i:%d\n", __func__, i);
            return r;
        }
    }
    return r;
}

int stop_pmc(struct __incore *inc)
{
    int i, r = -1;

    for (i = 0; i < 4; i++) {
        r = perf_stop(&inc->perf[i]);
        if (r < 0) {
            fprintf(stderr, "%s perf_stop failed. i:%d\n", __func__, i);
            return r;
        }
    }
    return r;
}

static int init_incore_perf(struct __perf_info *perf, const pid_t pid, const int cpu, uint64_t conf, uint64_t conf1)
{
    int r;

    if ((0 <= cpu) && (cpu < num_of_cpu())) {
        perf->pid = -1;
        perf->cpu = cpu;
    } else {
        perf->pid = pid;
        perf->cpu = -1;
    }

    perf->group_fd         = -1;
    perf->flags            = 0x08;
    memset(&perf->attr, 0, sizeof(perf->attr));
    perf->attr.type        = PERF_TYPE_RAW;
    perf->attr.size        = sizeof(perf->attr);
    perf->attr.config      = conf;
    perf->attr.config1     = conf1;
    perf->attr.disabled    = 1;
    perf->attr.inherit     = 1;

    r = perf_init(perf);
    if (r < 0) {
        fprintf(stderr,
                "%s perf_init failed. pid:%d cpu:%d\n",
                __func__, pid, cpu);
        return r;
    }
    return r;

}

int init_all_dram_rds(struct __incore *inc, const pid_t pid, const int cpu)
{
    return init_incore_perf(&inc->perf[0], pid, cpu,
                            perf_config.all_dram_rds_config,
                            perf_config.all_dram_rds_config1);
}

int init_cpu_l2stall(struct __incore *inc, const pid_t pid, const int cpu)
{
    return init_incore_perf(&inc->perf[1], pid, cpu,
                            perf_config.cpu_l2stall_config, 0);
}

int init_cpu_llcl_hits(struct __incore *inc, const pid_t pid, const int cpu)
{
    return init_incore_perf(&inc->perf[2], pid, cpu,
                            perf_config.cpu_llcl_hits_config, 0);
}

int init_cpu_llcl_miss(struct __incore *inc, const pid_t pid, const int cpu)
{
    return init_incore_perf(&inc->perf[3], pid, cpu,
                            perf_config.cpu_llcl_miss_config, 0);
}

int init_pmc(struct __incore *inc, const pid_t pid, const int cpu)
{
    int r;

    /* reset all pmc values */
    r = init_all_dram_rds(inc, pid, cpu);
    if (r < 0) {
        fprintf(stderr, "%s init_all_dram_rds failed cpu:%d\n", __func__, cpu);
        return r;
    }

    r = init_cpu_l2stall(inc, pid, cpu);
    if (r < 0) {
        fprintf(stderr, "%s init_cpu_l2stall failed cpu:%d\n", __func__, cpu);
        return r;
    }

    r = init_cpu_llcl_hits(inc, pid, cpu);
    if (r < 0) {
        fprintf(stderr, "%s init_cpu_llcl_hits failed cpu:%d\n", __func__, cpu);
        return r;
    }

    r = init_cpu_llcl_miss(inc, pid, cpu);
    if (r < 0) {
        fprintf(stderr, "%s init_cpu_llcl_miss failed cpu:%d\n", __func__, cpu);
        return r;
    }

    return r;
}


void fini_pmc(struct __incore *inc)
{
    int i;

    stop_pmc(inc);
    for (i = 0; i < 4; i++) {
        perf_fini(&inc->perf[i]);
    }
}

int init_all_pmcs(struct __pmu_info *pmu, const pid_t pid)
{
    int i, r, n;

    n = num_of_cpu();

    pmu->cpus = (struct __incore *)calloc(sizeof(struct __incore), n);
    if (pmu->cpus == NULL) {
        handle_error("malloc");
    }

    for (i = 0; i < n; i++) {
        r = init_pmc(&pmu->cpus[i], pid, i);
        if (r < 0) {
            fprintf(stderr, "%s init_pmc failed cpu:%d\n", __func__, i);
            return r;
        }
    }

    r = start_all_pmcs(pmu);
    if (r < 0) {
        fprintf(stderr, "%s start_all_pmcs failed\n", __func__);
    }
    return r;
}

void fini_all_pmcs(struct __pmu_info *pmu)
{
    uint32_t i;

    for (i = 0; i < num_of_cpu(); i++) {
        fini_pmc(&pmu->cpus[i]);
    }
    free(pmu->cpus);
}

int read_cpu_elems(struct __incore *inc, struct __cpu_elem *elem)
{
    ssize_t r;

    r = perf_read_pmu(&inc->perf[0], &elem->all_dram_rds);
    if (r < 0) {
        fprintf(stderr, "%s read all_dram_rds failed.\n", __func__);
        return r;
    }
    DEBUG_PRINT("read all_dram_rds:%lu\n", elem->all_dram_rds);

    r = perf_read_pmu(&inc->perf[1], &elem->cpu_l2stall_t);
    if (r < 0) {
        fprintf(stderr, "%s read cpu_l2stall_t failled.\n", __func__);
        return r;
    }
    DEBUG_PRINT("read cpu_l2stall_t:%lu\n", elem->cpu_l2stall_t);

    r = perf_read_pmu(&inc->perf[2], &elem->cpu_llcl_hits);
    if (r < 0) {
        fprintf(stderr, "%s read cpu_llcl_hits failed.\n", __func__);
        return r;
    }
    DEBUG_PRINT("read cpu_llcl_hits:%lu\n", elem->cpu_llcl_hits);

    r = perf_read_pmu(&inc->perf[3], &elem->cpu_llcl_miss);
    if (r < 0) {
        fprintf(stderr, "%s read cpu_llcl_miss failed.\n", __func__);
        return r;
    }
    DEBUG_PRINT("read cpu_llcl_miss:%lu\n", elem->cpu_llcl_miss);

    return 0;
}
