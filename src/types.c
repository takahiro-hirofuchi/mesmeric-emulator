/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */

#include "types.h"
#include "common.h"

/* CPU Models */
enum {
    CPU_MDL_BDX = 79,
    CPU_MDL_SKX = 85,
    CPU_MDL_END = 0x0ffff
};

const struct __model_context model_ctx[] = {
    /*
     * Tested with
     * - BroadwellX, Intel(R) Xeon(R) CPU E5-2630 v4 @ 2.20GHz
     * - BroadwellX, Intel(R) Xeon(R) CPU E5-2650 v4 @ 2.20GHz
     */
    {CPU_MDL_BDX, {
        "/sys/bus/event_source/devices/uncore_cbox_%u/type",
        /*
         * cbo_config:
         *    unc_c_llc_victims.m_state
         *    umask=0x1,event=0x37
         */
        0x0137,
        /*
         * all_dram_rds_config:
         *   offcore_response.all_reads.llc_miss.local_dram
         *   cpu/umask=0x1,event=0xb7,offcore_rsp=0x40007f7/
         */
        0x01b7, 0x6040007f7,
        /*
         * cpu_l2stall_config:
         *   cycle_activity.stalls_l2_pending
         *   cpu/umask=0x5,cmask=0x5,event=0xa3/
         */
        0x50005a3,
        /*
         * cpu_llcl_hits_config:
         *   mem_load_uops_l3_hit_retired.xsnp_none
         *   cpu/umask=0x8,event=0xd2/
         */
        0x08d2,
        /*
         * cpu_llcl_miss_config:
         *   mem_load_uops_l3_miss_retired.local_dram
         *   cpu/umask=0x1,event=0xd3/
         */
        0x01d3,
        }
    },
    /*
     * Tested with
     * - SkylakeX, Intel(R) Xeon(R) Gold 6126 CPU @ 2.60GHz
     * - SkylakeX, Intel(R) Xeon(R) Gold 6230 CPU @ 2.10GHz
     */
    {CPU_MDL_SKX, {
        "/sys/bus/event_source/devices/uncore_cha_%u/type",
        /*
         * cbo_config:
         *   UNC_C_LLC_VICTIMS
         *   umask=0x21,event=37
         */
        0x2137,
        /*
         * all_dram_rds_config:
         *   OCR.ALL_READS.L3_MISS.SNOOP_NONE
         *   cpu/umask=0x1,event=0xb7,offcore_rsp=0xbc0007f7/
         */
        0x01b7, 0xbc0007f7,
        /*
         * cpu_l2stall_config:
         *   cycle_activity.stalls_l2_miss
         *   cpu/umask=0x5,cmask=0x5,event=0xa3/
         */
        0x50005a3,
        /*
         * cpu_llcl_hits_config:
         *   mem_load_l3_hit_retired.xsnp_none
         *   cpu/umask=0x8,event=0xd2/
         */
        0x08d2,
        /*
         * cpu_llcl_miss_config:
         *   mem_load_l3_miss_retired.local_dram
         *   cpu/umask=0x1,event=0xd3/
         */
        0x01d3,
        }
    },
    {CPU_MDL_END, {0}}
};

struct __perf_configs perf_config;

static int ncpu = 0;

int num_of_cpu(void)
{
    if (ncpu) {
        return ncpu;
    }
    ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 0) {
        handle_error("sysconf");
    }
    DEBUG_PRINT("num_of_cpu=%d\n", ncpu);
    return ncpu;
}

static int ncbo = 0;

static int
is_uncore_cbox(const struct dirent *de)
{
    return (!fnmatch("uncore_cbox_[0-9]*", de->d_name, FNM_PATHNAME)||
            !fnmatch("uncore_cha_[0-9]*", de->d_name, FNM_PATHNAME));
}

int num_of_cbo(void)
{
    struct dirent **namelist;
    int i;

    if (ncbo) {
        return ncbo;
    }

    ncbo = scandir("/sys/bus/event_source/devices/", &namelist, is_uncore_cbox, NULL);
    if (ncbo <= 0) // It is also an error if the number of cbox is 0.
        handle_error("scandir");
    else {
        for (i = 0; i < ncbo; i++) {
            free(namelist[i]);
        }
        free(namelist);
    }
    DEBUG_PRINT("num_of_cbo=%d\n", ncbo);
    return ncbo;
}

static double cpu_MHz = 0;

double cpu_frequency(void)
{
    int i;
    FILE *fp;
    char line[1024];

    if (cpu_MHz) {
        return cpu_MHz;
    }

    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
        handle_error("fopen");
    }

    i = 0;
    while (fgets(line, sizeof(line), fp)) {
        // get frequency of core 0
        i = sscanf(line, "cpu MHz : %lf", &cpu_MHz);
        if (i == 1) {
            break;
        }
        if (i == EOF) {
            exit_with_message("Failed to get CPU frequency.\n");
        }
    }
    DEBUG_PRINT("cpu MHz: %lf\n", cpu_MHz);

    fclose(fp);
    return cpu_MHz;
}

int detect_model(const uint32_t model) {
    int ret = -1;
    int i = 0;
    while (model_ctx[i].model != CPU_MDL_END) {
        if (model_ctx[i].model == model) {
            perf_config = model_ctx[i].perf_conf;
            ret = 0;
            break;
        }
        i++;
    }
    return ret;
}
