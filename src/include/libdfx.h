/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * Copyright (C) 2023, Advanced Micro Devices, Inc. All Rights Reserved
 * SPDX-License-Identifier: MIT
 ***************************************************************/

#ifndef __LIBDFX_H
#define __LIBDFX_H

#define DFX_NORMAL_EN			(0x00000000U)
#define DFX_EXTERNAL_CONFIG_EN		(0x00000001U)
#define DFX_ENCRYPTION_USERKEY_EN	(0x00000020U)

/* Error codes */
#define DFX_INVALID_PLATFORM_ERROR		(0x1U)
#define DFX_CREATE_PACKAGE_ERROR		(0x2U)
#define DFX_DUPLICATE_FIRMWARE_ERROR		(0x3U)
#define DFX_DUPLICATE_DTBO_ERROR		(0x4U)
#define DFX_READ_PACKAGE_ERROR			(0x5U)
#define DFX_AESKEY_READ_ERROR			(0x6U)
#define DFX_DMABUF_ALLOC_ERROR			(0x7U)
#define DFX_INVALID_PACKAGE_ID_ERROR		(0x8U)
#define DFX_GET_PACKAGE_ERROR			(0x9U)
#define DFX_FAIL_TO_OPEN_DEV_NODE		(0xAU)
#define DFX_IMAGE_CONFIG_ERROR			(0xBU)
#define DFX_DRIVER_CONFIG_ERROR			(0xCU)
#define DFX_NO_VALID_DRIVER_DTO_FILE		(0xDU)
#define DFX_DESTROY_PACKAGE_ERROR		(0xEU)
#define DFX_FAIL_TO_OPEN_BIN_FILE		(0xFU)
#define DFX_INSUFFICIENT_MEM			(0x10U)

/* XILFPGA/PMUFW Error Codes */
#define XFPGA_ERROR_CSUDMA_INIT_FAIL		(0x2U)
#define XFPGA_ERROR_PL_POWER_UP			(0x3U)
#define XFPGA_ERROR_PL_ISOLATION		(0x4U)
#define XPFGA_ERROR_PCAP_INIT			(0x5U)
#define XFPGA_ERROR_BITSTREAM_LOAD_FAIL		(0x6U)
#define XFPGA_ERROR_CRYPTO_FLAGS		(0x7U)
#define XFPGA_ERROR_HDR_AUTH			(0X8U)
#define XFPGA_ENC_ISCOMPULSORY			(0x9U)
#define XFPGA_PARTITION_AUTH_FAILURE		(0xAU)
#define XFPGA_STRING_INVALID_ERROR		(0xBU)
#define XFPGA_ERROR_SECURE_CRYPTO_FLAGS		(0xCU)
#define XFPGA_ERROR_SECURE_MODE_EN		(0xDU)
#define XFPGA_HDR_NOAUTH_PART_AUTH		(0xEU)
#define XFPGA_DEC_WRONG_KEY_SOURCE		(0xFU)
#define XFPGA_ERROR_DDR_AUTH_VERIFY_SPK		(0x10U)
#define XFPGA_ERROR_DDR_AUTH_PARTITION		(0x11U)
#define XFPGA_ERROR_DDR_AUTH_WRITE_PL		(0x12U)
#define XFPGA_ERROR_OCM_AUTH_VERIFY_SPK		(0x13U)
#define XFPGA_ERROR_OCM_AUTH_PARTITION		(0x14U)
#define XFPGA_ERROR_OCM_REAUTH_WRITE_PL		(0x15U)
#define XFPGA_ERROR_PCAP_PL_DONE		(0x16U)
#define XFPGA_ERROR_AES_DECRYPT_PL		(0x17U)
#define XFPGA_ERROR_CSU_PCAP_TRANSFER		(0x18U)
#define XFPGA_ERROR_PLSTATE_UNKNOWN		(0x19U)
#define XFPGA_ERROR_BITSTREAM_FORMAT		(0x1AU)
#define XFPGA_ERROR_UNALIGN_ADDR		(0x1BU)
#define XFPGA_ERROR_AES_INIT			(0x1CU)
#define XFPGA_ERROR_EFUSE_CHECK			(0x1DU)

#define XFPGA_SUCCESS                   (0x0U)
#define XFPGA_FAILURE                   (0x1U)
#define XFPGA_VALIDATE_ERROR            (0x2U)
#define XFPGA_PRE_CONFIG_ERROR          (0x3U)
#define XFPGA_WRITE_BITSTREAM_ERROR     (0x4U)
#define XFPGA_POST_CONFIG_ERROR         (0x5U)
#define XFPGA_OPS_NOT_IMPLEMENTED       (0x6U)
#define XFPGA_INVALID_PARAM             (0x8U)


int dfx_cfg_init(const char *dfx_package_path,
                  const char *devpath, unsigned long flags);
int dfx_cfg_load(int package_id);
int dfx_cfg_drivers_load(int package_id);
int dfx_cfg_remove(int package_id);
int dfx_cfg_destroy(int package_id);
int dfx_get_active_uid_list(int *buffer);
int dfx_get_meta_header(char *binfile, int *buffer, int buf_size);

#endif
