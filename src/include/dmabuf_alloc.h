/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 *
 * This file is written by taking reference file from Linux kernel
 * at: tools/testing/selftests/dmabuf-heaps/dmabuf-heap.c
 *
 ***************************************************************/

#ifndef __DMABUF_ALLOC_H
#define __DMABUF_ALLOC_H

struct dma_buffer_info {
	int devfd;
	int dma_buffd;
	unsigned char *dma_buffer;
	unsigned long dma_buflen;
};


/* This API is used to allocate dmaable memory in the kernel
 * and export to others
 */
int export_dma_buffer(struct dma_buffer_info *dma_data);

/* This API is used to close all references related to dmaable
 * memory allocated by the dma_heap.
 */
int close_dma_buffer(struct dma_buffer_info *dma_data);

#endif
