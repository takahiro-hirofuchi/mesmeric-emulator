/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai: */
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <locale.h> // comma per 3

#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/wait.h> // fork-exec wait
#include <time.h>
#include <errno.h>

// #define DEBUG
// #define ONLY_CALCULATION	// not inserting the calculated delay to processes

#include "common.h"
#include "types.h"
#include "monitor.h"
#include "uncores.h"
#include "incores.h"
#include "pebs.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>

#define SOCKET_PATH "/tmp/mesmeric_socket"

static void
noop_handler(int sig)
{
    ;
}

static void
detach_children(void)
{
    struct sigaction sa;

    sa.sa_handler = noop_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDWAIT;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        DEBUG_PRINT("Failed to sigaction: %s", strerror(errno));
    }
}

int main(int argc, char **argv)
{
    int i, j, ret;
    struct __elem *swap;
    struct __monitor *mons;
    struct __monitor *mon;
    struct __pmu_info pmu;
    int cur_processes = 0;                    // total number of executed application exectued by mesmerics
    pid_t t_process = 0;
    int ncpu = num_of_cpu();
    int ncbo = num_of_cbo();
    uint32_t intrval = 20; // default is 20ms
    uint32_t tnum = ncpu;  // default is num of cpu
    uint64_t pebs_sample_period = 1;
    uint64_t use_cpus = 0;
    cpu_set_t use_cpuset;
    CPU_ZERO(&use_cpuset);
    /* calculate nvm latency */
    double dram_latency = 85.7; // default: Broadwell Xeon (Gen 5). XEON_E5_2654_V4
    double weight = 4.2;        // default: Broadwell Xeon (Gen 5). XEON_E5_2654_V4
    double cpu_freq = cpu_frequency();
    bool oneshot = false;

    setlocale(LC_NUMERIC, "");

    char* target_path = NULL;
    char* target_argv[128];
    int target_argc = 1;

    /* get args */
    struct option longopts[] = {
        { "target",     required_argument, NULL, 't' },
        { "targetargs", required_argument, NULL, 'a' },
        { "interval",   required_argument, NULL, 'i' },
        { "cpuset",     required_argument, NULL, 'c' },
        { "pebsperiod", required_argument, NULL, 'p' },
        { "latency",    required_argument, NULL, 'l' },
        { "weight",     required_argument, NULL, 'w' },
        { "cpufreq",    required_argument, NULL, 'f' },
        { "oneshot",    no_argument,       NULL, 'o' },
        { 0,        0,                 0,     0  },
    };
    bool usage = false;
    int opt;
    int longindex;
    while ((opt = getopt_long(argc, argv, "t:a:i:c:p:l:w:f:o", longopts, &longindex)) != -1) {
        switch (opt) {
            case 't':
                target_path = optarg;
                target_argv[0] = optarg;
                DEBUG_PRINT("t:%s\n", optarg);
                break;
            case 'a':
                target_argv[target_argc] = optarg;
                target_argc++;
                DEBUG_PRINT("a:%s\n", optarg);
                break;
            case 'i':
                intrval   = (uint32_t)strtol(optarg, NULL, 10);
                DEBUG_PRINT("i:%s\n", optarg);
                break;
            case 'c':
                use_cpus = (uint64_t)strtoul(optarg, NULL, 16);
                DEBUG_PRINT("m:%s\n", optarg);
                break;
            case 'p':
                pebs_sample_period   = (uint64_t)strtoull(optarg, NULL, 10);
                DEBUG_PRINT("p:%s\n", optarg);
                if (pebs_sample_period == 0) {
                    usage = true;
                }
                break;
            case 'l':
                dram_latency = (double)strtod(optarg, NULL);
                DEBUG_PRINT("s:%s\n", optarg);
                if (dram_latency <= 0) {
                    usage = true;
                }
                break;
            case 'w':
                weight = (double)strtod(optarg, NULL);
                DEBUG_PRINT("w:%s\n", optarg);
                if (weight < 0) {
                    usage = true;
                }
                break;
            case 'f':
                cpu_freq = (double)strtod(optarg, NULL);
                DEBUG_PRINT("f:%s\n", optarg);
                if (cpu_freq < 0) {
                    usage = true;
                }
                break;
            case 'o':
                oneshot = true;
                break;
            default:
                usage = true;
        }
    }
    for (i = 0; i < ncpu; i++) {
        if (!use_cpus || use_cpus & 1UL << i) {
            CPU_SET(i, &use_cpuset);
            DEBUG_PRINT("use cpuid: %d\n", i);
        }
    }
    tnum = CPU_COUNT(&use_cpuset);

    DEBUG_PRINT("tnum:%u, intrval:%u\n", tnum, intrval);
    DEBUG_PRINT("dram_latency:%lf\n", dram_latency);
    DEBUG_PRINT("weight:%lf\n", weight);
    DEBUG_PRINT("cpu_freq:%lf\n", cpu_freq);
    target_argv[target_argc] = NULL;
    i = argc - optind;
    if (usage || i <= 0 || (i % 2)) {
        printf("Usage: mes [ -f ${CPU_FREQUENCY} ] [ -l ${LATENCY} ] [ -w ${WEIGHT} ] [-i ${EMUL_PERIOD_MS} ] [-c ${CPU_SET} ] [ -p ${PEBS_SAMPLING_PERIOD} ] [ -t ${TARGET_PATH} ] [ -a ${TARGET_ARGS} [ -a ...] ] ${ALPHA_RD_LAT_NS} ${ALPHAWR_LAT_NS} [ ${BETA_RD_LAT_NS} ${BETA_WR_LAT_NS} [...] ]\n");
        exit(0);
    }
    int nmem = i / 2;
    struct emul_nvm_latency *emul_nvm_lats = (struct emul_nvm_latency *)calloc(sizeof(struct emul_nvm_latency), nmem);
    if (emul_nvm_lats == NULL) {
        handle_error("calloc");
    }
    i = optind;
    for (j = 0; j < nmem; j++) {
        emul_nvm_lats[j].read = (double)strtod(argv[i++], NULL);
        emul_nvm_lats[j].write = (double)strtod(argv[i++], NULL);
        DEBUG_PRINT("nvm%d: latency read:%lf, write:%lf\n", j, emul_nvm_lats[j].read, emul_nvm_lats[j].write);
        if (emul_nvm_lats[j].read <= dram_latency || emul_nvm_lats[j].write <= dram_latency) {
            exit_with_message("The latency specified for the emulation was less than the actual latency.\n");
        }
    }

    /* check of the limitation */
    if (tnum > ncpu) {
        exit_with_message("Failed to execute. The number of processes/threads of the target application is more than physical CPU cores.\n");
    }

    /* create unix socket */
    int sock;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    remove(addr.sun_path);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        handle_error("Failed to execute. Can't bind to a socket.");
        exit(1);
    }

    initMon(tnum, &use_cpuset, &mons, nmem);

    if (target_path != NULL) {
        /* zombie avoid */
        detach_children();
        /* create target process */
        t_process = fork();
        if (t_process < 0) {
            handle_error("Fork: failed to create target process");
            exit(1);
        } else if(t_process == 0) {
            execv(target_path, target_argv);
            /* We do not need to check the return value */
            handle_error("Exec: failed to create target process");
            exit(1);
        }

        // In case of process, use SIGSTOP.
        i = enable_mon(t_process, t_process, true, 0, tnum, mons);
        if (i == -1) {
            exit_with_message("Failed to enable monitor\n");
        } else if (i < 0) {
            // pid not found. might be already terminated.
            DEBUG_PRINT("pid(%ul) not found. might be already terminated.", t_process);
        }
        cur_processes++;
        DEBUG_PRINT("pid of mes = %d, cur process=%d\n", t_process, cur_processes);
    } else {
        oneshot = false;
    }

    if (cur_processes >= ncpu) {
        exit_with_message("Failed to execute. The number of processes/threads of the target application is more than physical CPU cores.\n");
    }

    // Wait all the target processes until emulation process initialized.
    stop_all_mons(cur_processes, mons);

    /* get CPU information */
    if (!get_cpu_info(&mons[0].before->cpuinfo)) {
        exit_with_message("Failed to obtain CPU information.\n");
    }

    /* check the CPU model */
    if (detect_model(mons[0].before->cpuinfo.cpu_model)) {
        exit_with_message("Failed to execute. This CPU model is not supported. Update src/types.c\n");
    }

    init_all_pmcs(&pmu, t_process);
    init_all_cbos(&pmu);

    /* Caculate epoch time */
    struct timespec waittime;
    waittime.tv_sec  = intrval / 1000;
    waittime.tv_nsec = (intrval % 1000) * 1000000;

    printf("NVM Read  latency=%lf\n", emul_nvm_lats[0].read);
    printf("NVM Write latency=%lf\n", emul_nvm_lats[0].write);
    printf("The target process starts running.\n");
    printf("set nano sec = %lu\n", waittime.tv_nsec);

    /* read CBo params */
    for (i = 0; i < cur_processes; i++) {
        mon = &mons[i];
        for (j = 0; j < ncbo; j++) {
            read_cbo_elems(&pmu.cbos[j], &mon->before->cbos[j]);
        }
        for (j = 0; j < ncpu; j++) {
            read_cpu_elems(&pmu.cpus[j], &mon->before->cpus[j]);
        }
    }

    uint32_t diff_nsec = 0;
    struct timespec start_ts, end_ts;
    struct timespec sleep_start_ts, sleep_end_ts;
#ifdef VERBOSE_DEBUG
    struct timespec recv_ts;
#endif
    // Wait all the target processes until emulation process initialized.
    run_all_mons(cur_processes, mons);
    for (i = 0; i < cur_processes; i++) {
        clock_gettime(CLOCK_MONOTONIC, &mons[i].start_exec_ts);
    }

    /*
     * The format for receiving an tgid,tid,opcode via a socket is as follows.
     *   |  tgid:32bit  |  tid:32bit  |  opcode:32bit  |  num_of_region:32bit  |
     * To emulate the hybrid memory, specify 2 or more for num_of_region.
     * When specifying 1 or more in num_of_region, add the following format to
     * as repeatedly as the num_of_region in addition to the above.
     *   |  address:64bit  |  size:64bit  |
     */
    enum opcode {
        MES_PROCESS_CREATE = 0,
        MES_THREAD_CREATE = 1,
        MES_THREAD_EXIT = 2,
    };
    struct op_data {
        uint32_t tgid;
        uint32_t tid;
        uint32_t opcode;
        uint32_t num_of_region;
    };
    size_t regs_size = sizeof(struct __region_info) * nmem;
    size_t sock_data_size = sizeof(struct  op_data) + regs_size;
    size_t sock_buf_size = sock_data_size + 1/*for size check*/;
    char *sock_buf = (char *)malloc(sock_buf_size);

    while(1) {
        /* wait for pre-defined interval */
        clock_gettime(CLOCK_MONOTONIC, &sleep_start_ts);

#ifdef VERBOSE_DEBUG
        DEBUG_PRINT("sleep_start_ts: %010lu.%09lu\n", sleep_start_ts.tv_sec, sleep_start_ts.tv_nsec);
#endif
        int n;
        do {
            memset(sock_buf, 0, sock_buf_size);
            // without blocking
            n = recv(sock, sock_buf, sock_buf_size, MSG_DONTWAIT);
            if (n < 1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // no data
                    break;
                } else {
                    handle_error("Failed to recv");
                }
            } else if (n >= sizeof(struct op_data) && n <= sock_data_size) {
                struct op_data *opd = (struct op_data *)sock_buf;
                DEBUG_PRINT("received data: size=%d, tgid=%u, tid=%u, opcode=%u, num_of_region=%u\n",
                            n, opd->tgid, opd->tid, opd->opcode, opd->num_of_region);

                if (opd->opcode == MES_THREAD_CREATE || opd->opcode == MES_PROCESS_CREATE) {
                    int target;
                    bool is_process = (opd->opcode == MES_PROCESS_CREATE) ? true : false;
                    uint64_t period = (opd->num_of_region >= 2) ? pebs_sample_period : 0 ; // is hybrid
                    // register to monitor
                    target = enable_mon(opd->tgid, opd->tid, is_process, period, tnum, mons);
                    if (target == -1) {
                        exit_with_message("Failed to enable monitor\n");
                    } else if (target < 0) {
                        // tid not found. might be already terminated.
                        continue;
                    }
                    mon = &mons[target];
                    if (opd->num_of_region >= 2) { // Ignored if num_of_region is 1 or less
                        // pebs sampling
                        if ((n - sizeof(struct op_data)) != (sizeof(struct __region_info) * opd->num_of_region)) {
                            exit_with_message("Received data is invalid.\n");
                        }
                        struct __region_info *ri = (struct __region_info *)(sock_buf + sizeof(struct op_data));
                        if (set_region_info_mon(mon, opd->num_of_region, ri) < 0) {
                            exit_with_message("Received data is invalid.\n");
                        }
                    }
                    // Wait the target processes until emulation process initialized.
                    stop_mon(mon);
                    /* read CBo params */
                    for (j = 0; j < ncbo; j++) {
                        read_cbo_elems(&pmu.cbos[j], &mon->before->cbos[j]);
                    }
                    for (j = 0; j < ncpu; j++) {
                        read_cpu_elems(&pmu.cpus[j], &mon->before->cpus[j]);
                    }
                    // Run the target processes.
                    run_mon(mon);
                    clock_gettime(CLOCK_MONOTONIC, &mon->start_exec_ts);
                } else if (opd->opcode == MES_THREAD_EXIT) {
                    // unregister from monitor, and display results.
                    if (terminate_mon(opd->tgid, opd->tid, tnum, mons) < 0) {
                        DEBUG_PRINT("It might be already terminated.\n");
                    }
                }
            } else {
                exit_with_message("received data is invalid size: size=%d\n", n);
            }
        } while (n > 0); // check the next message.

#ifdef VERBOSE_DEBUG
        clock_gettime(CLOCK_MONOTONIC, &recv_ts);
        DEBUG_PRINT("recv_ts       : %010lu.%09lu\n", recv_ts.tv_sec, recv_ts.tv_nsec);
        if (recv_ts.tv_nsec < sleep_start_ts.tv_nsec) {
            DEBUG_PRINT("start - recv  : %10lu.%09lu\n", recv_ts.tv_sec - sleep_start_ts.tv_sec - 1, \
                        recv_ts.tv_nsec + 1000000000 - sleep_start_ts.tv_nsec);
        } else {
            DEBUG_PRINT("start - recv  : %10lu.%09lu\n", recv_ts.tv_sec - sleep_start_ts.tv_sec, \
                        recv_ts.tv_nsec - sleep_start_ts.tv_nsec);
        }
#endif

        struct timespec req = waittime;
        struct timespec rem = {0};
        while (1) {
            ret = nanosleep(&req, &rem);
            if (ret == 0) { // success
                break;
            } else { // ret < 0
                if (errno == EINTR) {
                    // The pause has been interrupted by a signal that was delivered to the thread.
                    DEBUG_PRINT("nanosleep: remain time %ld.%09ld(sec)\n", (long)rem.tv_sec, (long)rem.tv_nsec);
                    req = rem; // call nanosleep() again with the remain time.
                } else {
                    // fatal error
                    handle_error("Failed to wait nanotime");
                }
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &sleep_end_ts);

#ifdef VERBOSE_DEBUG
        DEBUG_PRINT("sleep_end_ts  : %010lu.%09lu\n", sleep_end_ts.tv_sec, sleep_end_ts.tv_nsec);
        if (sleep_end_ts.tv_nsec < sleep_start_ts.tv_nsec) {
            DEBUG_PRINT("start - end   : %10lu.%09lu\n", sleep_end_ts.tv_sec - sleep_start_ts.tv_sec - 1, \
                        sleep_end_ts.tv_nsec + 1000000000 - sleep_start_ts.tv_nsec);
        } else {
            DEBUG_PRINT("start - end   : %10lu.%09lu\n", sleep_end_ts.tv_sec - sleep_start_ts.tv_sec, \
                        sleep_end_ts.tv_nsec - sleep_start_ts.tv_nsec);
        }
#endif

        for (i = 0; i < tnum; i++) {
            mon = &mons[i];
            if (mon->status == MONITOR_DISABLE) {
                continue;
            }
            if (mon->status == MONITOR_ON) {
                clock_gettime(CLOCK_MONOTONIC, &start_ts);
                DEBUG_PRINT("[%d:%u:%u] start_ts: %010lu.%09lu\n", i, mon->tgid, mon->tid, start_ts.tv_sec, start_ts.tv_nsec);

#ifndef ONLY_CALCULATION
                /*stop target process group: send SIGSTOP */
                stop_mon(mon);
#endif
                /* read CBo values */
                uint64_t wb_cnt = 0;
                for (j = 0; j < ncbo; j++) {
                    read_cbo_elems(&pmu.cbos[j], &mon->after->cbos[j]);
                    wb_cnt += mon->after->cbos[j].llc_wb - mon->before->cbos[j].llc_wb;
                }
                DEBUG_PRINT("[%d:%u:%u] LLC_WB = %" PRIu64 "\n", i, mon->tgid, mon->tid, wb_cnt);

                /* read CPU params */
                uint64_t cpus_dram_rds=0;
                uint64_t target_l2stall=0, target_llcmiss=0, target_llchits=0;
                for (j = 0; j < ncpu; ++j) {
                    read_cpu_elems(&pmu.cpus[j], &mon->after->cpus[j]);
                    cpus_dram_rds += mon->after->cpus[j].all_dram_rds - mon->before->cpus[j].all_dram_rds;
                }

                if (mon->num_of_region >= 2) {
                    /* read PEBS sample */
                    if (pebs_read(&mon->pebs_ctx, mon->num_of_region, mon->region_info, &mon->after->pebs) < 0) {
                        fprintf(stderr, "[%d:%u:%u] Warning: Failed PEBS read\n", i, mon->tgid, mon->tid);
                    }
                    target_llcmiss = mon->after->pebs.llcmiss - mon->before->pebs.llcmiss;
                } else {
                    target_llcmiss = mon->after->cpus[mon->cpu_core].cpu_llcl_miss - mon->before->cpus[mon->cpu_core].cpu_llcl_miss;
                }

                target_l2stall = mon->after->cpus[mon->cpu_core].cpu_l2stall_t - mon->before->cpus[mon->cpu_core].cpu_l2stall_t;
                target_llchits = mon->after->cpus[mon->cpu_core].cpu_llcl_hits - mon->before->cpus[mon->cpu_core].cpu_llcl_hits;

                if (cpus_dram_rds < target_llcmiss) {
                    DEBUG_PRINT("[%d:%u:%u]warning: target_llcmiss is more than cpus_dram_rds. target_llcmiss %ju, cpus_dram_rds %ju\n",
                                i, mon->tgid, mon->tid, target_llcmiss, cpus_dram_rds);
                }
                uint64_t llcmiss_wb = 0;
                // To estimate the number of the writeback-involving LLC
                // misses of the CPU core (llcmiss_wb), the total number of
                // writebacks observed in L3 (wb_cnt) is devided
                // proportionally, according to the number of the ratio of
                // the LLC misses of the CPU core (target_llcmiss) to that
                // of the LLC misses of all the CPU cores and the
                // prefetchers (cpus_dram_rds).
                if (wb_cnt <= cpus_dram_rds && target_llcmiss <= cpus_dram_rds && cpus_dram_rds > 0) {
                    // Equation (9) in the IEICE paper
                    llcmiss_wb = wb_cnt * ((double) target_llcmiss / cpus_dram_rds);
                } else {
                    fprintf(stderr, "[%d:%u:%u]warning: wb_cnt %ju, target_llcmiss %ju, cpus_dram_rds %ju\n",
                            i, mon->tgid, mon->tid, wb_cnt, target_llcmiss, cpus_dram_rds);
                    llcmiss_wb = target_llcmiss;
                }

                uint64_t llcmiss_ro = 0;
                if(target_llcmiss < llcmiss_wb) {
                    DEBUG_PRINT("[%d:%u:%u] cpus_dram_rds %lu, llcmiss_wb %lu, target_llcmiss %lu\n",
                                i, mon->tgid, mon->tid, cpus_dram_rds, llcmiss_wb, target_llcmiss);
                    printf("!!!!llcmiss_ro is %lu!!!!!\n", llcmiss_ro);
                    llcmiss_wb = target_llcmiss;
                    llcmiss_ro = 0;
                } else {
                    llcmiss_ro = target_llcmiss - llcmiss_wb;
                }
                DEBUG_PRINT("[%d:%u:%u]llcmiss_wb=%lu, llcmiss_ro=%lu\n", i, mon->tgid, mon->tid ,llcmiss_wb, llcmiss_ro);

                uint64_t mastall_wb = 0;
                uint64_t mastall_ro = 0;
                // If both target_llchits and target_llcmiss are 0, it means that hit in L2.
                // Stall by LLC misses is 0.
                if (target_llchits || target_llcmiss) {
                    mastall_wb = (double)(target_l2stall / cpu_freq) * ( (double)(weight * llcmiss_wb) / (double)(target_llchits + (weight * target_llcmiss)) ) * 1000;
                    mastall_ro = (double)(target_l2stall / cpu_freq) * ( (double)(weight * llcmiss_ro) / (double)(target_llchits + (weight * target_llcmiss)) ) * 1000;
                }
                DEBUG_PRINT("l2stall=%" PRIu64 ", mastall_wb=%" PRIu64 ", mastall_ro=%" PRIu64 ", target_llchits=%" PRIu64 ", target_llcmiss=%" PRIu64 ", weight=%lf\n", \
                        target_l2stall, mastall_wb, mastall_ro, target_llchits, target_llcmiss, weight);

                uint64_t ma_wb = (double)mastall_wb / dram_latency;
                uint64_t ma_ro = (double)mastall_ro / dram_latency;

                uint64_t emul_delay = 0;
                if (mon->num_of_region < 2) {
                    emul_delay = (double)(ma_ro) * (emul_nvm_lats[0].read - dram_latency) + (double)(ma_wb) * (emul_nvm_lats[0].write - dram_latency);
                } else { // Emulate Hybrid Memory
                    bool total_is_zero = (mon->after->pebs.total - mon->before->pebs.total) ? false : true;
                    double sample = 0;
                    double sample_prop = 0;
                    double sample_total = (double)(mon->after->pebs.total - mon->before->pebs.total);
                    if (total_is_zero) {
                        // If the total is 0, divide equally.
                        sample_prop = (double)1 / (double)mon->num_of_region;
                    }
                    DEBUG_PRINT("[%d:%u:%u] pebs: total=%lu, \n", i, mon->tgid, mon->tid, mon->after->pebs.total);
                    for (j = 0; j < mon->num_of_region; j++) {
                        if (!total_is_zero) {
                            sample = (double)(mon->after->pebs.sample[j] - mon->before->pebs.sample[j]);
                            sample_prop = sample / sample_total;
                        }
                        emul_delay += (double)(ma_ro) * sample_prop * (emul_nvm_lats[j].read - dram_latency) +
                                      (double)(ma_wb) * sample_prop * (emul_nvm_lats[j].write - dram_latency);
                        mon->before->pebs.sample[j] = mon->after->pebs.sample[j];
                        DEBUG_PRINT("[%d:%u:%u] pebs sample[%d]: =%lu, \n", i, mon->tgid, mon->tid, j, mon->after->pebs.sample[j]);
                    }
                    mon->before->pebs.total = mon->after->pebs.total;
                }

                DEBUG_PRINT("ma_wb=%" PRIu64 ", ma_ro=%" PRIu64 ", delay=%" PRIu64 "\n", ma_wb, ma_ro, emul_delay);

                /* compensation of delay END(1) */
                clock_gettime(CLOCK_MONOTONIC, &end_ts);
                diff_nsec += (end_ts.tv_sec - start_ts.tv_sec)*1000000000 +
                             (end_ts.tv_nsec - start_ts.tv_nsec );
                DEBUG_PRINT("dif:%'12u\n", diff_nsec);

                uint64_t calibrated_delay = (diff_nsec > emul_delay) ? 0: emul_delay - diff_nsec;
                // uint64_t calibrated_delay = emul_delay;
                mon->total_delay += (double)calibrated_delay / 1000000000;
                diff_nsec = 0;

#ifndef ONLY_CALCULATION
                /* insert emulated NVM latency */
                mon->injected_delay.tv_sec  += (calibrated_delay / 1000000000);
                mon->injected_delay.tv_nsec += (calibrated_delay % 1000000000);
                DEBUG_PRINT("[%d:%u:%u]delay:%'10lu , total delay:%'lf\n", i, mon->tgid, mon->tid, calibrated_delay, mon->total_delay);
#endif
                swap        = mon->before;
                mon->before = mon->after;
                mon->after  = swap;

#ifndef ONLY_CALCULATION
                /* continue suspended processes: send SIGCONT */
                // unfreeze_counters_cbo_all(fds.msr[0]);
                // start_pmc(&fds, i);
                if (calibrated_delay == 0) {
                    clear_mon_time(&mon->wasted_delay);
                    clear_mon_time(&mon->injected_delay);
                    run_mon(mon);
                }
#endif

            } else if (mon->status == MONITOR_OFF) {
                // Wasted epoch time
                clock_gettime(CLOCK_MONOTONIC, &start_ts);
                uint64_t sleep_diff = (sleep_end_ts.tv_sec - sleep_start_ts.tv_sec)*1000000000 + \
                (sleep_end_ts.tv_nsec - sleep_start_ts.tv_nsec );
                struct timespec sleep_time;
                sleep_time.tv_sec = sleep_diff / 1000000000;
                sleep_time.tv_nsec = sleep_diff % 1000000000;
                mon->wasted_delay.tv_sec += sleep_time.tv_sec;
                mon->wasted_delay.tv_nsec += sleep_time.tv_nsec;
                DEBUG_PRINT("[%d:%u:%u][OFF] total: %'lu | wasted : %'lu | waittime : %'lu | squabble : %'lu\n", \
                            i, mon->tgid, mon->tid, mon->injected_delay.tv_nsec, mon->wasted_delay.tv_nsec, waittime.tv_nsec, mon->squabble_delay.tv_nsec);
                if(check_continue_mon(i, mons, sleep_time)) {
                    clear_mon_time(&mon->wasted_delay);
                    clear_mon_time(&mon->injected_delay);
                    run_mon(mon);
                }
                clock_gettime(CLOCK_MONOTONIC, &end_ts);
                diff_nsec += (end_ts.tv_sec - start_ts.tv_sec)*1000000000 + \
                (end_ts.tv_nsec - start_ts.tv_nsec );
            }

            if(mon->status == MONITOR_OFF && mon->injected_delay.tv_nsec != 0) {
                long remain_time = mon->injected_delay.tv_nsec - mon->wasted_delay.tv_nsec;
                /* do we need to get squabble time ? */
                if (mon->wasted_delay.tv_sec >= waittime.tv_sec && \
                    remain_time < waittime.tv_nsec) {
                        mon->squabble_delay.tv_nsec += remain_time;
                        if (mon->squabble_delay.tv_nsec < 40000000) {
                            DEBUG_PRINT("[SQ]total: %'lu | wasted : %'lu | waittime : %'lu | squabble : %'lu\n", \
                                        mon->injected_delay.tv_nsec, mon->wasted_delay.tv_nsec, waittime.tv_nsec, mon->squabble_delay.tv_nsec);
                            clear_mon_time(&mon->wasted_delay);
                            clear_mon_time(&mon->injected_delay);
                            run_mon(mon);
                        } else {
                            mon->injected_delay.tv_nsec += mon->squabble_delay.tv_nsec;
                            clear_mon_time(&mon->squabble_delay);
                        }
                }
            }
        } // End for-loop for all target processes
        if (check_all_mons_terminated(tnum, mons)) {
#ifdef VERBOSE_DEBUG
            DEBUG_PRINT("All processes have already been terminated.\n");
#endif
            if (oneshot) {
                break;
            }
        }
    } // End while-loop for emulation

    /* cleanup */
    fini_all_pmcs(&pmu);
    fini_all_cbos(&pmu);
    freeMon(tnum, &mons);
    free(sock_buf);
    free(emul_nvm_lats);

    close(sock);

    return 0;
}
