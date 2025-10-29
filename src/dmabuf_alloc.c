/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * Copyright (C) 2023, Advanced Micro Devices, Inc. All Rights Reserved
 * SPDX-License-Identifier: GPL-2.0
 *
 * This file is written by taking reference file from Linux kernel
 * at: tools/testing/selftests/dmabuf-heaps/dmabuf-heap.c
 *
 ***************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "dmabuf_alloc.h"
#include "dma-heap.h"

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static int is_cma_default_node(const char *node_name)
{
	char path[512];

	snprintf(path, sizeof(path),
		 "/sys/firmware/devicetree/base/reserved-memory/%s/linux,cma-default",
		 node_name);

	/* returns 1 if property exists, 0 if not */
	return (access(path, F_OK) == 0);
}

static int open_device(const char *cma_file, int *devfd)
{
	if (!devfd) {
		printf("%s: Invalid devfd pointer\n", __func__);
		return -1;
	}

	// Try user-provided heap path first
	if (cma_file != NULL) {
		*devfd = open(cma_file, O_RDWR);
		if (*devfd >= 0)
			return 0;
	}

	// Try default kernel reserved CMA heap
	*devfd = open("/dev/dma_heap/reserved", O_RDWR);
	if (*devfd >= 0)
		return 0;

	// Dynamic scan for cma_reserved@* that also has linux,cma-default
	DIR *dir = opendir("/dev/dma_heap");
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strncmp(entry->d_name, "cma_reserved@", 13) == 0) {

				// Check if the DT node has the linux,cma-default flag
				if (!is_cma_default_node(entry->d_name))
					continue;

				char path[256];
				snprintf(path, sizeof(path),
					 "/dev/dma_heap/%s", entry->d_name);

				*devfd = open(path, O_RDWR);

				if (*devfd >= 0) {
					closedir(dir);
					return 0;
				}
			}
		}
		closedir(dir);
	}

	printf("%s: No valid CMA heap found\n", __func__);

	return -1;
}

static int alloc_dma_buffer(struct dma_buffer_info *dma_data)
{
	struct dma_heap_allocation_data alloc_data_info = {
		.len = dma_data->dma_buflen,
		.fd = 0,
		.fd_flags = O_RDWR | O_CLOEXEC,
		.heap_flags = 0,
	};
	int ret;

	ret = ioctl(dma_data->devfd, DMA_HEAP_IOCTL_ALLOC, &alloc_data_info);
	if (ret < 0) {
		printf("%s: DMA_HEAP_IOCTL_ALLOC: Failed\n", __func__);
		return -1;
	}

        if (alloc_data_info.fd < 0 || alloc_data_info.len <= 0) {
                printf("%s: Invalid mmap data\n", __func__);
                goto err;
        }

	dma_data->dma_buffd = alloc_data_info.fd;
        dma_data->dma_buflen = alloc_data_info.len;

        /* mapp buffer for the buffer fd */
        dma_data->dma_buffer = (unsigned char *)mmap(NULL, dma_data->dma_buflen,
						 PROT_READ|PROT_WRITE,
						 MAP_SHARED,
						 dma_data->dma_buffd, 0);
        if (dma_data->dma_buffer == MAP_FAILED) {
                printf("%s: mmap: Failed\n", __func__);
                goto err;
        }

        return 0;

err:
	if (alloc_data_info.fd)
                close(alloc_data_info.fd);

	return -1;
}

int export_dma_buffer(struct dma_buffer_info *dma_data)
{
	int devfd, ret;

	if (!dma_data) {
		printf("%s: Invalid input data\n", __func__);
		return -1;
	}

	ret = open_device(dma_data->cma_file, &devfd);
	if (ret)
		return ret;

	dma_data->devfd = devfd;

	ret = alloc_dma_buffer(dma_data);
	if (ret)
		goto err;

	return 0;

err:
	if (devfd)
		close(devfd);

	return -1;
}

int close_dma_buffer(struct dma_buffer_info *dma_data)
{
	if (!dma_data) {
		printf("%s: Invalid input data\n", __func__);
		return -1;
	}

	munmap(dma_data->dma_buffer, dma_data->dma_buflen);

	if (dma_data->dma_buffd > 0)
		close(dma_data->dma_buffd);

	if (dma_data->devfd > 0)
		close(dma_data->devfd);

	return 0;
}
