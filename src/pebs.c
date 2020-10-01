/*
 * Copyright (c) 2016-2020: National Institute of Advanced Industrial Science
 * and Technology (AIST). All right reserved.
 */
/*  vim: set expandtab ts=4 sw=4 ai */
#define _GNU_SOURCE 1

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "pebs.h"

#define PAGE_SIZE 4096
#define DATA_SIZE PAGE_SIZE
#define MMAP_SIZE (PAGE_SIZE + DATA_SIZE)

#define barrier() _mm_mfence()

struct __attribute__((packed)) perf_sample {
	struct perf_event_header header;
	uint32_t pid;
	uint32_t tid;
	uint64_t addr;
	uint64_t value;
	uint64_t time_enabled;
	uint64_t phys_addr;
};

long
perf_event_open(struct perf_event_attr* event_attr, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, event_attr, pid,
		       cpu, group_fd, flags);
}

int
pebs_init(struct pebs_context *ctx, pid_t pid, uint64_t sample_period)
{
	ctx->pid    = pid;
	ctx->sample_period = sample_period;

	// Configure perf_event_attr struct
	struct perf_event_attr pe;
	memset(&pe, 0, sizeof(struct perf_event_attr));
	pe.type = PERF_TYPE_RAW;
	pe.size = sizeof(struct perf_event_attr);
	pe.config = 0x20d1; // mem_load_retired.l3_miss
	pe.config1 = 3;
	pe.disabled = 1;    // Event is initially disabled
	pe.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED;
	pe.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_ADDR | PERF_SAMPLE_READ | PERF_SAMPLE_PHYS_ADDR;
	pe.sample_period = ctx->sample_period;
	pe.precise_ip = 1;
	pe.exclude_kernel = 1; // excluding events that happen in the kernel-space

	int cpu = -1;   // measure on any cpu
	int group_fd = -1;
	unsigned long flags = 0;

	ctx->fd = perf_event_open(&pe, ctx->pid, cpu, group_fd, flags);
	if (ctx->fd == -1) {
		perror("perf_event_open");
		return -1;
	}

        ctx->mplen = MMAP_SIZE;
	ctx->mp = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE,
		       MAP_SHARED, ctx->fd, 0);
	if (ctx->mp == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	pebs_start(ctx);
	return 0;
}

int
pebs_read(struct pebs_context *ctx, const int nreg, struct __region_info *reg_info, struct __pebs_elem *elem)
{
	struct perf_event_mmap_page *mp = ctx->mp;

        if (ctx->fd < 0) {
            return 0;
        }

	if (mp == MAP_FAILED)
		return -1;

	int r = 0;
	int i;
	struct perf_event_header *header;
	struct perf_sample *data;
	uint64_t last_head;
	char *dp = ((char *) mp) + PAGE_SIZE;

	do {
		ctx->seq = mp->lock;
		barrier();
		last_head = mp->data_head;

		while ((uint64_t)ctx->rdlen < last_head) {
			header = (struct perf_event_header *) (dp + ctx->rdlen % DATA_SIZE);

			switch (header->type) {
			case PERF_RECORD_LOST:
				DEBUG_PRINT("received PERF_RECORD_LOST\n");
				break;
			case PERF_RECORD_SAMPLE:
				data = (struct perf_sample *) (dp + ctx->rdlen % DATA_SIZE);

				if (header->size < sizeof(*data)) {
					DEBUG_PRINT("size too small. size:%u\n", header->size);
					r = -1;
					continue;
				}
				if (ctx->pid == data->pid) {
					DEBUG_PRINT("pid:%u tid:%u time:%lu addr:%lx phys_addr:%lx llc_miss:%lu\n",
						    data->pid, data->tid, data->time_enabled,
						    data->addr, data->phys_addr, data->value);
					for (i = 0; i < nreg; i++) {
						if (reg_info[i].addr <= data->addr &&
						    reg_info[i].addr + reg_info[i].size > data->addr) {
							elem->sample[i]++;
							DEBUG_PRINT("sample: region %d (%lu)\n", i, elem->sample[i]);
							elem->llcmiss = data->value;
						}
					}
				}
				break;
			case PERF_RECORD_THROTTLE:
				DEBUG_PRINT("received PERF_RECORD_THROTTLE\n");
				break;
			case PERF_RECORD_UNTHROTTLE:
				DEBUG_PRINT("received PERF_RECORD_UNTHROTTLE\n");
				break;
			case PERF_RECORD_LOST_SAMPLES:
				DEBUG_PRINT("received PERF_RECORD_LOST_SAMPLES\n");
				break;
			default:
				DEBUG_PRINT("other data received. type:%d\n", header->type);
				break;
			}

			ctx->rdlen += header->size;
		}

		mp->data_tail = last_head;
		barrier();
	} while (mp->lock != ctx->seq);

	elem->total = 0;
	for (i = 0; i < nreg; i++) {
		elem->total += elem->sample[i];
	}

	return r;
}

int
pebs_start(struct pebs_context *ctx)
{
	if (ctx->fd < 0) {
		return 0;
	}
	if (ioctl(ctx->fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
		perror("ioctl");
		return -1;
	}

	return 0;
}

int
pebs_stop(struct pebs_context *ctx)
{
	if (ctx->fd < 0) {
		return 0;
	}
	if (ioctl(ctx->fd, PERF_EVENT_IOC_DISABLE, 0) < 0) {
		perror("ioctl");
		return -1;
	}
	return 0;
}

int
pebs_fini(struct pebs_context *ctx)
{
	pebs_stop(ctx);

	if (ctx->fd < 0) {
		return 0;
	}

	if (ctx->mp != MAP_FAILED) {
		munmap(ctx->mp, ctx->mplen);
		ctx->mp    = MAP_FAILED;
		ctx->mplen = 0;
	}

	if (ctx->fd != -1) {
		close(ctx->fd);
		ctx->fd = -1;
	}

	ctx->pid = -1;
	return 0;
}
