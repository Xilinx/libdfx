/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * Copyright (C) 2023, Advanced Micro Devices, Inc. All Rights Reserved
 * SPDX-License-Identifier: MIT
 ***************************************************************/

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <asm/types.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <err.h>

#include <linux/dma-buf.h>

#include <drm/drm.h>

#include "dmabuf_alloc.h"
#include "libdfx.h"
#include "dma-heap.h"

#define DFX_IOCTL_LOAD_DMA_BUFF        _IOWR('R', 1, __u32)

#define INVALID_PLATFORM	0x0U
#define ZYNQMP_PLATFORM		0x2U
#define VERSAL_PLATFORM		0x3U

#define MAX_CMD_LEN		512U
#define MAX_AES_KEY_LEN         64U
#define PLATFORM_STR_LEN	128U
#define FPGA_WORD_SIZE		4U
#define FPGA_DUMMY_BYTE		0xFFU

#define ZYNQMP_MAX_ERR	27U

struct dfx_package_node {
	int  flags;
	int  xilplatform;
	unsigned long  package_id;
	char *aes_key;
	char *package_name;
	char *package_path;
	char *load_image_name;
	char *load_image_path;
	char *load_aes_file_name;
	char *load_aes_file_path;
	char *load_image_dtbo_name;
	char *load_image_dtbo_path;
	char *load_drivers_dtbo_name;
	char *load_drivers_dtbo_path;
	char *load_image_overlay_pck_path;
	char *load_drivers_overlay_pck_path;
	struct dma_buffer_info *dmabuf_info;
	struct dfx_package_node *next;
};

typedef struct dfx_package_node FPGA_NODE;

FPGA_NODE *head_node, *first_node, *temp_node = NULL, *prev_node, next_node;

typedef struct {
        int err_code;
        char *err_str;
} dfx_err;

static dfx_err zynqmp_err[] = {
	[0] = { .err_code =  XFPGA_ERROR_CSUDMA_INIT_FAIL, .err_str = "Failed to initialize the CSUDMA module" },
	[1] = { .err_code =  XFPGA_ERROR_PL_POWER_UP, .err_str = " Failed to power-up the PL" },
	[2] = { .err_code =  XFPGA_ERROR_PL_ISOLATION, .err_str = "Failed to perform the PS-PL isolation " },
	[3] = { .err_code =  XPFGA_ERROR_PCAP_INIT, .err_str = "Failed to initialize the PCAP IP Initialization" },
	[4] = { .err_code =  XFPGA_ERROR_BITSTREAM_LOAD_FAIL, .err_str = " PL Configuration failed" },
	[5] = { .err_code =  XFPGA_ERROR_CRYPTO_FLAGS, .err_str = "Failed due to incorrect user crypto flags" },
	[6] = { .err_code =  XFPGA_ERROR_HDR_AUTH, .err_str = "Failed to authenticate the image headers" },
	[7] = { .err_code =  XFPGA_ENC_ISCOMPULSORY, .err_str = "Support only encrypted Image loading" },
	[8] = { .err_code =  XFPGA_PARTITION_AUTH_FAILURE, .err_str = "Image authentication failed" },
	[9] = { .err_code =  XFPGA_STRING_INVALID_ERROR, .err_str = "Firmware internal error" },
	[10] = { .err_code =  XFPGA_ERROR_SECURE_CRYPTO_FLAGS, .err_str = "Failed due to incorrect user crypto flags" },
	[11] = { .err_code =  XFPGA_ERROR_SECURE_MODE_EN, .err_str = "Supports only secure Images loading" },
	[12] = { .err_code =  XFPGA_HDR_NOAUTH_PART_AUTH, .err_str = "Image header authentication failed" },
	[13] = { .err_code =  XFPGA_DEC_WRONG_KEY_SOURCE, .err_str = "Decryption failed due to wrong key source" },
	[14] = { .err_code =  XFPGA_ERROR_DDR_AUTH_VERIFY_SPK, .err_str = "DDR Image authentication failed due to Invalid keys" },
	[15] = { .err_code =  XFPGA_ERROR_DDR_AUTH_PARTITION, .err_str = "Failed to authentication DDR image partition" },
	[16] = { .err_code =  XFPGA_ERROR_DDR_AUTH_WRITE_PL, .err_str = "DDR Image authentication failed" },
	[17] = { .err_code =  XFPGA_ERROR_OCM_AUTH_VERIFY_SPK, .err_str = "OCM Image authentication failed due to Invalid keys" },
	[18] = { .err_code =  XFPGA_ERROR_OCM_AUTH_PARTITION, .err_str = "Failed to authentication OCM image partition" },
	[19] = { .err_code =  XFPGA_ERROR_OCM_REAUTH_WRITE_PL, .err_str = "OCM Image authentication failed" },
	[20] = { .err_code =  XFPGA_ERROR_PCAP_PL_DONE, .err_str = "Failed to get the PCAP done status" },
	[21] = { .err_code =  XFPGA_ERROR_AES_DECRYPT_PL, .err_str = "Image AES decryption failed" },
	[22] = { .err_code =  XFPGA_ERROR_CSU_PCAP_TRANSFER, .err_str = "PCAP failed to transfer the Image" },
	[23] = { .err_code =  XFPGA_ERROR_PLSTATE_UNKNOWN, .err_str = "PL is in Unknow state" },
	[23] = { .err_code =  XFPGA_ERROR_BITSTREAM_FORMAT, .err_str = "Bitstream format error" },
	[24] = { .err_code =  XFPGA_ERROR_UNALIGN_ADDR, .err_str = "Error: Received Unaligned Bitstream address" },
	[25] = { .err_code =  XFPGA_ERROR_AES_INIT, .err_str = "AES initialization failed" },
	[26] = { .err_code =  XFPGA_ERROR_EFUSE_CHECK, .err_str = "Support only secure image configuration" },
};

static struct dfx_package_node *create_package(void);
static struct dfx_package_node *get_package(int package_id);
static int destroy_package(int package_id);
static int read_package_folder(struct dfx_package_node *package_node);
static int dfx_state(char *cmd, char *state);
static int dfx_package_load_dmabuf(struct dfx_package_node *package_node);
static int dfx_getplatform(void);
static int find_key(struct dfx_package_node *package_node);
static int lengthOfLastWord2(const char *input);
static void strlwr(char *destination, const char *source);
static int dfx_get_error(char *cmd);
static void zynqmp_print_err_msg(int err);
static int dfx_cfg_init_common(const char *dfx_package_path, const char *dfx_bin_file,
			       const char *dfx_dtbo_file, const char *dfx_driver_dtbo_file,
			       const char *dfx_aes_key_file, const char *devpath,
			       unsigned long flags);
static int read_package_byname(struct dfx_package_node *package_node,
			       const char *dfx_bin_file, const char *dfx_dtbo_file,
			       const char *dfx_driver_dtbo_file,
			       const char *dfx_aes_key_file);
static char *get_file_name_from_path(char *full_path);
static void copy_file_to_firmware(const char *file);
static int validate_input_files(const char *dfx_bin_file, const char *dfx_dtbo_file,
				const char *dfx_driver_dtbo_file, const char *dfx_aes_key_file,
				unsigned long flags);
static bool file_exists(const char *filename);
#ifdef ENABLE_LIBDFX_TIME
static inline double gettime(struct timeval  t0, struct timeval t1);
#endif

/* Provide a generic interface to the user to specify the required parameters
 * for the library.The calling process must call this API before it performs
 * fpga-load/remove.
 *
 * const char *dfx_package_path: The folder path of the fpga package.
 *                                fpga package should look something as below:
 *                                fpga-package: /nava/fpga-package1
 *                                                      |--> Bit_file.bin
 *                                                      |--> DT_Overlayfile.dtbo
 * const char *devpath: The dev interface is exposed at /dev/fpga-deviceN.
 *                      where N is the interface-device number.
 * unsigned long flags: Flags to specify any special instructions for library
 *			to perform.
 *
 * Return: returns unique package_Id, or Error code on failure.
 */
int dfx_cfg_init(const char *dfx_package_path,
		  const char *devpath, unsigned long flags)
{
	int ret = 0;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif

	if (dfx_package_path == NULL) {
		printf("%s: Invalid input args\n", __func__);
		return -DFX_INVALID_PARAM;
	}

	ret = dfx_cfg_init_common(dfx_package_path, NULL, NULL, NULL, NULL,
				  devpath, flags);

#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&t1, NULL);
	time = gettime(t0, t1);
	printf("%s API Time taken: %f Milli Seconds\n\r", __func__, time);
#endif
	return ret;
}

/* Provide a generic interface to the user to specify the required parameters
 * for FPGA programming.It takes absolute paths of all individual files as
 * arguments and perform the required init functionality.
 * The calling process must call this API before it performs -load/remove.
 *
 * const char *dfx_bin_file: Absolute pdi/bistream file path(The one user wants to load).
 *             -Ex:/lib/firmware/xilinx/example/example.pdi
 *
 * const char *dfx_dtbo_file: Absolute relevant dtbo file path
 *             -Ex: /lib/firmware/xilinx/example/example.dtbo
 *
 * const char *dfx_driver_dtbo_file: Absolute relevant dtbo file path
 *             - Ex: /lib/firmware/xilinx/example/drivers.dtbo (or) NULL
 * Note: To use the deferred probe functionality Both Image DTBO and relevant
 * Drivers DTBO files are mandatory for other use cases user should pass "NULL".
 *
 * char *dfx_aes_key_file: Absolute relevant aes key file path
 *             Ex: /lib/firmware/xilinx/example/Aes_key.nky (or) NULL
 * Note: If the bitstream is encrypted with the user-key then the user needs to
 * pass relevant aes_key.key file for other use cases user should pass "NULL".
 *
 * char *devpath: The dev interface is exposed at /dev/fpga-deviceN.
 * Where N is the interface-device number.
 *
 * unsigned long flags: Flags to specify any special instructions for the
 * library to perform.
 *
 * Return: returns unique package_Id or Error code on failure.
 */
int dfx_cfg_init_file(const char *dfx_bin_file, const char *dfx_dtbo_file,
		      const char *dfx_driver_dtbo_file, const char *dfx_aes_key_file,
		      const char *devpath, unsigned long flags)
{
	int len, ret = 0;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif
	/* Validate Inputs */
	ret = validate_input_files(dfx_bin_file, dfx_dtbo_file,
				   dfx_driver_dtbo_file, dfx_aes_key_file,
				   flags);
	if (ret) {
		printf("%s: Invalid input args\n", __func__);
		return ret;
	}

	ret = dfx_cfg_init_common(NULL, dfx_bin_file, dfx_dtbo_file,
				  dfx_driver_dtbo_file, dfx_aes_key_file,
				  devpath, flags);
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&t1, NULL);
	time = gettime(t0, t1);
	printf("%s API Time taken: %f Milli Seconds\n\r", __func__, time);
#endif
	return ret;
}

/* This API is Responsible for the following things.
 *      -->Load bitstream into the PL
 *      -->Probe the Drivers which are relevant to the Bitstream as per
 *	   DT overlay(mentioned in dfx_package folder)
 *
 * int package_id: Unique package_id value which was returned by dfx_cfg_init.
 *
 * Return: returns zero on success or Error code on failure.
 */
int dfx_cfg_load(int package_id)
{
	FPGA_NODE *package_node;
	int len, fd, buffd, ret = 0, err = 0;
	char command[MAX_CMD_LEN];
	char *str;
	DIR *FD;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval total_t1, total_t0, load_t1, load_t0;
	double total_time, load_time;

	gettimeofday(&total_t0, NULL);
#endif
	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		ret = -DFX_INVALID_PACKAGE_ID_ERROR;
		goto END;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		ret = -DFX_GET_PACKAGE_ERROR;
		goto END;
	}

	if (!(package_node->flags & DFX_EXTERNAL_CONFIG_EN)) {
		fd = open("/dev/fpga0", O_RDWR);
		if (fd < 0) {
			printf("%s: Cannot open device file...\n",
			       __func__);
			ret = -DFX_FAIL_TO_OPEN_DEV_NODE;
			goto END;
		}

		snprintf(command, sizeof(command),
			 "echo %x > /sys/class/fpga_manager/fpga0/flags",
			 package_node->flags);
		system(command);
		if (package_node->flags & DFX_ENCRYPTION_USERKEY_EN) {
			snprintf(command, sizeof(command),
				 "echo %s > /sys/class/fpga_manager/fpga0/key",
				 package_node->aes_key);
			system(command);
		}

		buffd = package_node->dmabuf_info->dma_buffd;
		/* Send dmabuf-fd to the FPGA Manager */
		ioctl(fd, DFX_IOCTL_LOAD_DMA_BUFF, &buffd);
		close(fd);
	}

	snprintf(command, sizeof(command),
		 "/configfs/device-tree/overlays/%s_image_%d",
		 package_node->package_name, package_node->package_id);

	len = strlen(command) + 1;
	str = (char *) calloc((len), sizeof(char));
	strncpy(str, command, len);
	package_node->load_image_overlay_pck_path = str;
	snprintf(command, sizeof(command), "mkdir -p %s",
		 package_node->load_image_overlay_pck_path);
	system(command);

	snprintf(command, sizeof(command), "echo -n %s > %s/path",
		 package_node->load_image_dtbo_name,
		 package_node->load_image_overlay_pck_path);
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&load_t0, NULL);
#endif
	system(command);
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&load_t1, NULL);
#endif

	if (!(package_node->flags & DFX_EXTERNAL_CONFIG_EN)) {
		snprintf(command, sizeof(command),
			 "cat /sys/class/fpga_manager/fpga0/state >> state.txt");
		ret = dfx_state(command, "operating");
		if (ret) {
			err = dfx_get_error(command);
			snprintf(command, sizeof(command), "rmdir %s",
				 package_node->load_image_overlay_pck_path);
			system(command);
			printf("%s: Image configuration failed with error: 0x%x\n",
			        __func__, err);
			if (package_node->xilplatform == ZYNQMP_PLATFORM)
				zynqmp_print_err_msg(err);
			system(command);
			ret = -DFX_IMAGE_CONFIG_ERROR;
			goto END;
		}
	}

	snprintf(command, sizeof(command), "cat %s/path >> state.txt",
		 package_node->load_image_overlay_pck_path);
	ret = dfx_state(command, package_node->load_image_dtbo_name);
	if (ret) {
		snprintf(command, sizeof(command), "rmdir %s",
			 package_node->load_image_overlay_pck_path);
		system(command);
		printf("%s: Image configuration failed\n", __func__);
		ret = -DFX_IMAGE_CONFIG_ERROR;
		goto END;
	}

END:
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&total_t1, NULL);
	total_time = gettime(total_t0, total_t1);
	load_time =  gettime(load_t0, load_t1);
	printf("%s: Image load time from pre-allocated buffer: %f Milli Seconds\n\r",
	       __func__, load_time);
	printf("%s API Total time taken: %f Milli Seconds\n\r", __func__, total_time);
#endif
	return ret;
}

/* This API is Responsible for loading the drivers corresponding to a package
 *
 * package_id: Unique package_id value which was returned by dfx_cfg_init.
 *
 * Return: returns zero on success or Error code on failure.
 */
int dfx_cfg_drivers_load(int package_id)
{
	FPGA_NODE *package_node;
	char command[MAX_CMD_LEN];
	int len, ret = 0;
	char *str;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif
	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		ret = -DFX_INVALID_PACKAGE_ID_ERROR;
		goto END;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		ret = -DFX_GET_PACKAGE_ERROR;
		goto END;
	}

	if (package_node->load_drivers_dtbo_path == NULL) {
		ret = -DFX_NO_VALID_DRIVER_DTO_FILE;
		goto END;
	}

	snprintf(command, sizeof(command),
		 "/configfs/device-tree/overlays/%s_driver_%d",
		 package_node->package_name, package_node->package_id);
	len = strlen(command) + 1;
	str = (char *) calloc((len), sizeof(char));
	strncpy(str, command, len);
	package_node->load_drivers_overlay_pck_path = str;
	snprintf(command, sizeof(command), "mkdir -p %s",
		 package_node->load_drivers_overlay_pck_path);
	system(command);
	snprintf(command, sizeof(command), "echo -n %s > %s/path",
		 package_node->load_drivers_dtbo_name,
		 package_node->load_drivers_overlay_pck_path);
	system(command);

	snprintf(command, sizeof(command), "cat %s/path >> state.txt",
		 package_node->load_drivers_overlay_pck_path);
	ret = dfx_state(command, package_node->load_drivers_dtbo_name);
	if (ret) {
		snprintf(command, sizeof(command), "rmdir %s",
			 package_node->load_drivers_overlay_pck_path);
		system(command);
		printf("%s: Drivers DTBO config failed\n", __func__);
		ret = -DFX_DRIVER_CONFIG_ERROR;
	}

END:
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&t1, NULL);
	time = gettime(t0, t1);
	printf("%s API Time taken: %f Milli Seconds\n\r", __func__, time);
#endif
	return ret;
}

/* This API is Responsible for unloading the drivers corresponding to a package
 *
 * int package_id: Unique package_id value which was returned by dfx_cfg_init.
 *
 * Return: returns zero on success or Error code on failure.
 */
int dfx_cfg_remove(int package_id)
{
	FPGA_NODE *package_node;
	char command[MAX_CMD_LEN];
	int ret = 0;
	DIR *FD;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif
	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		ret = -DFX_INVALID_PACKAGE_ID_ERROR;
		goto END;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		ret = -DFX_GET_PACKAGE_ERROR;
		goto END;
	}

	if (package_node->load_drivers_overlay_pck_path != NULL) {
		FD = opendir(package_node->load_drivers_overlay_pck_path);
		if (FD) {
			closedir(FD);
			snprintf(command, sizeof(command), "rmdir %s",
				 package_node->load_drivers_overlay_pck_path);
			system(command);

		}
	}

	if (package_node->load_image_overlay_pck_path != NULL) {
		FD = opendir(package_node->load_image_overlay_pck_path);
		if (FD) {
			closedir(FD);
			snprintf(command, sizeof(command), "rmdir %s",
				 package_node->load_image_overlay_pck_path);
			system(command);
		}
	}

END:
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&t1, NULL);
	time = gettime(t0, t1);
	printf("%s API Time taken: %f Milli Seconds\n\r", __func__, time);
#endif
	return ret;
}

/* This API is Responsible for release/destroy the resouces allocated
 * by dfx_cfg_init().
 *
 * int package_id: Unique package_id value which was returned by dfx_cfg_init.
 *
 * Return: returns zero on success or Error code on failure.
 */
int dfx_cfg_destroy(int package_id)
{
	FPGA_NODE *package_node;
	char command[MAX_CMD_LEN];
	int ret = 0;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif
	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		ret = -DFX_INVALID_PACKAGE_ID_ERROR;
		goto END;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		ret = -DFX_GET_PACKAGE_ERROR;
		goto END;
	}

	if (package_node->load_image_overlay_pck_path != NULL) {
		snprintf(command, sizeof(command), "rm /lib/firmware/%s",
			 package_node->load_image_dtbo_name);
		system(command);
	}

	if (!(package_node->flags & DFX_EXTERNAL_CONFIG_EN)) {
		/* This call will do the following things
		 * unmap the buffer properly
		 * close the buffer fd (Free the Dmabuf memory)
		 * Finally, close the client fd
		 */
		close_dma_buffer(package_node->dmabuf_info);
	}

	ret = destroy_package(package_node->package_id);
END:
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&t1, NULL);
	time = gettime(t0, t1);
	printf("%s API Time taken: %f Milli Seconds\n\r", __func__, time);
#endif
	return ret;
}

/* This API populates buffer with {Node ID, Unique ID, Parent Unique ID, Function ID}
 * for each applicable NodeID in the system.
 *
 * buffer: User buffer address
 *
 * Return: Number of bytes read from the firmware in case of success.
 *         or Negative value on failure.
 */
int dfx_get_active_uid_list(int *buffer)
{
	const char* filename = "/sys/devices/platform/firmware:versal-firmware/uid-read";
	int platform, ret = 0, count = 0;
	FILE* fd;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif
	platform = dfx_getplatform();
	if (platform != VERSAL_PLATFORM) {
		ret = -DFX_INVALID_PLATFORM_ERROR;
		goto END;
	}

	fd = fopen(filename, "rb");
	if (!fd) {
		printf("Unable to open file!");
		ret = -DFX_FAIL_TO_OPEN_BIN_FILE;
		goto END;
	}

	while(!feof(fd)) {
		fread(&buffer[count], sizeof(int), 1,fd);
		count++;
	}

	fclose(fd);
	ret = (count - 1) *  sizeof(int);
END:
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&t1, NULL);
	time = gettime(t0, t1);
	printf("%s API Time taken: %f Milli Seconds\n\r", __func__, time);
#endif
	return ret;
}

/* This API populates buffer with meta-header info related to the user
 * provided PDI.
 *
 * binfile: PDI Image.
 * buffer: User buffer address
 * buf_size : User buffer size.
 *
 * Return: Number of bytes read from the firmware in case of success.
 *         or Negative value on failure.
 */
int dfx_get_meta_header(char *binfile, int *buffer, int buf_size)
{
	const char* filename = "/sys/devices/platform/firmware:versal-firmware/meta-header-read";
	char command[2048], *token, *tmp, *tmp1;
	int platform, ret = 0, count = 0;
	FILE* fd;
	DIR *FD;
#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif
	platform = dfx_getplatform();
	if (platform != VERSAL_PLATFORM) {
		ret = -DFX_INVALID_PLATFORM_ERROR;
		goto END;
	}

	fd = fopen(binfile, "rb");
	if (!fd) {
		printf("Unable to open binary file!");
		ret = -DFX_FAIL_TO_OPEN_BIN_FILE;
		goto END;
	}
	fclose(fd);

	FD = opendir("/lib/firmware");
	if (FD)
		closedir(FD);
	else
		system("mkdir -p /lib/firmware");

	snprintf(command, sizeof(command), "cp %s /lib/firmware", binfile);
	system(command);
	tmp = strdup(binfile);
	while((token = strsep(&tmp, "/")))
		tmp1 = token;

	snprintf(command, sizeof(command), "echo %s > /sys/devices/platform/firmware:versal-firmware/firmware", tmp1);
	system(command);

	free(tmp);

	fd = fopen(filename, "rb");
	if (!fd) {
		printf("Unable to open sysfs binary file!");
		ret = -DFX_FAIL_TO_OPEN_BIN_FILE;
		goto END;
	}

	while(!feof(fd)) {
		if(buf_size < count) {
			count = -DFX_INSUFFICIENT_MEM;
			break;
		}

		fread(&buffer[count], sizeof(int), 1,fd);
		count++;
	}

	fclose(fd);
	closedir(FD);

	ret = count * sizeof(int);
END:
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&t1, NULL);
	time = gettime(t0, t1);
	printf("%s API Time taken: %f Milli Seconds\n\r", __func__, time);
#endif
	return ret;
}

static int read_package_folder(struct dfx_package_node *package_node)
{
	int bin_count = 0, dtbo_count = 0, driver_dtbo_count = 0, nky_count = 0;
	char command[MAX_CMD_LEN];
	struct dirent *dir;
	int len, pcklen;
	char *bin = ".bin";
	char *pdi = ".pdi";
	char *extension;
	char *file_name;
	char *str;
	DIR *FD;

	if (package_node->xilplatform == VERSAL_PLATFORM)
		extension = pdi;
	else
		extension = bin;

	pcklen = strlen(package_node->package_path);

	FD = opendir(package_node->package_path);
	if (FD) {
		while ((dir = readdir(FD)) != NULL) {
			len = strlen(dir->d_name);
			file_name = (char *) calloc((len + 1), sizeof(char));
			strlwr(file_name, dir->d_name);
			if (len > 4) {
				if (!strcmp(file_name + (len - 4), extension)) {
					str = (char *) calloc(
							(len + pcklen + 1),
							sizeof(char));
					strcpy(str, package_node->package_path);
					strcat(str, dir->d_name);
					package_node->load_image_path = str;
					str = (char *) calloc((len + 1),
								sizeof(char));
					strcpy(str, dir->d_name);
					package_node->load_image_name = str;
					bin_count++;
				} else if ((!strcmp(file_name + (len - 4),
					   ".bit")) && package_node->xilplatform
					   == ZYNQMP_PLATFORM) {
					str = (char *) calloc(
							(len + pcklen + 1),
							sizeof(char));
					strcpy(str, package_node->package_path);
					strcat(str, dir->d_name);
					package_node->load_image_path = str;
					str = (char *) calloc((len + 1),
							sizeof(char));
					strcpy(str, dir->d_name);
					package_node->load_image_name = str;
					bin_count++;
				} else if (!strcmp(file_name + (len - 7),
								"_i.dtbo")) {
					str = (char *) calloc(
							(len + pcklen + 1),
							sizeof(char));
					strcpy(str, package_node->package_path);
					strcat(str, dir->d_name);
					package_node->load_image_dtbo_path =
									str;
					str = (char *) calloc((len + 1),
								sizeof(char));
					strcpy(str, dir->d_name);
					package_node->load_image_dtbo_name =
									str;
					dtbo_count++;
				} else if (!strcmp(file_name + (len - 7),
					   "_d.dtbo")) {
					str = (char *) calloc(
							(len + pcklen + 1),
							sizeof(char));
					strcpy(str, package_node->package_path);
					strcat(str, dir->d_name);
					package_node->load_drivers_dtbo_path =
									str;
					str = (char *) calloc((len + 1),
								sizeof(char));
					strcpy(str, dir->d_name);
					package_node->load_drivers_dtbo_name =
									str;
					driver_dtbo_count++;
				} else if (!strcmp(file_name + (len - 5),
								".dtbo")) {
					str = (char *) calloc(
							(len + pcklen + 1),
							sizeof(char));
					strcpy(str, package_node->package_path);
					strcat(str, dir->d_name);
					package_node->load_image_dtbo_path =
									str;
					str = (char *) calloc((len + 1),
					sizeof(char));
					strcpy(str, dir->d_name);
					package_node->load_image_dtbo_name =
									str;
					dtbo_count++;
				} else if (!strcmp(file_name + (len - 4),
								".nky")) {
					str = (char *) calloc(
							(len + pcklen + 1),
							sizeof(char));
					strcpy(str, package_node->package_path);
					strcat(str, dir->d_name);
					package_node->load_aes_file_path = str;
					str = (char *) calloc((len + 1),
							sizeof(char));
					strcpy(str, dir->d_name);
					package_node->load_aes_file_name = str;
					nky_count++;
				}
			}
			free(file_name);
		}
		closedir(FD);
	}

	if (bin_count > 1) {
		printf("libdfx: Error: %s* has multiple Bitstream files!\n",
		       package_node->package_path);
		return -DFX_DUPLICATE_FIRMWARE_ERROR;
	}

	if (dtbo_count > 1) {
		printf("libdfx: Error: %s* has multiple overlay files!\n",
		       package_node->package_path);
		return -DFX_DUPLICATE_DTBO_ERROR;
	}

	if (driver_dtbo_count > 1) {
		printf("libdfx: warning: %s* has multiple Drivers overlay files(Deferred probe)!\n",
		       package_node->package_path);
		return -DFX_DUPLICATE_DRIVERS_DTBO_ERROR;
	}

	if (nky_count > 1) {
		printf("libdfx: warning: %s* has multiple AES key files!\n",
		       package_node->package_path);
		return -DFX_DUPLICATE_AES_KEY_ERROR;
	}

	if (package_node->load_image_dtbo_path != NULL) {
		len = strlen(package_node->load_image_dtbo_path);
		str = (char *) calloc((len + 1), sizeof(char));
		strcpy(str, package_node->load_image_dtbo_path);
		dirname(str);
		snprintf(command, sizeof(command), "%s", basename(str));
		free(str);
		len = strlen(command) + 1;
		str = (char *) calloc((len), sizeof(char));
		strncpy(str, command, len);
		package_node->package_name = str;
		snprintf(command, sizeof(command), "cp %s /lib/firmware/",
			 package_node->load_image_dtbo_path);
		system(command);

		if (package_node->load_drivers_dtbo_path != NULL) {
			snprintf(command, sizeof(command),
				 "cp %s /lib/firmware/",
				 package_node->load_drivers_dtbo_path);
			system(command);
		}
	} else {
		printf("%s: Invalid package\n", __func__);
		return -DFX_READ_PACKAGE_ERROR;
	}

	return 0;
}

static struct dfx_package_node *create_package()
{
	FPGA_NODE *package_node;
	DIR *FD;

	FD = opendir("/lib/firmware");
	if (FD)
		closedir(FD);
	else
		system("mkdir -p /lib/firmware");

	FD = opendir("/configfs/device-tree/overlays/");
	if (FD)
		closedir(FD);
	else {
		system("mkdir -p /configfs");
		system("mount -t configfs configfs /configfs");
	}

	temp_node = first_node;
	package_node = (FPGA_NODE *) calloc(1, sizeof(FPGA_NODE));
	package_node->next = NULL;
	if (first_node == NULL) {
		package_node->package_id = 1;
		first_node = package_node;
	} else {
		while (temp_node != NULL) {
			if (temp_node->next == NULL) {
				package_node->package_id =
						temp_node->package_id + 1;
				temp_node->next = package_node;
				break;
			}
			temp_node = temp_node->next;
		}
	}

	return package_node;
}

static struct dfx_package_node *get_package(int package_id)
{
	temp_node = first_node;

	while (temp_node != NULL) {

		if (temp_node->package_id == package_id)
			break;
		temp_node = temp_node->next;
	}

	return temp_node;
}

static int destroy_package(int package_id)
{
	FPGA_NODE *package_node = NULL;

	temp_node = first_node;

	if (first_node == NULL)
		return -DFX_DESTROY_PACKAGE_ERROR;

	if (first_node->package_id == package_id) {
		if (first_node->next != NULL)
			first_node = first_node->next;
		else
			first_node = NULL;
		package_node = temp_node;
	} else {
		while (temp_node->next != NULL) {
			if (temp_node->next->package_id == package_id) {
				package_node = temp_node->next;
				temp_node->next = package_node->next;
				break;
			}
			temp_node = temp_node->next;
		}
	}

	if (package_node != NULL) {
		if (package_node->package_name != NULL)
			free(package_node->package_name);
		if (package_node->package_path != NULL)
			free(package_node->package_path);
		if (package_node->load_image_name != NULL)
			free(package_node->load_image_name);
		if (package_node->load_image_path != NULL)
			free(package_node->load_image_path);
		if (package_node->load_image_dtbo_name != NULL)
			free(package_node->load_image_dtbo_name);
		if (package_node->load_image_dtbo_path != NULL)
			free(package_node->load_image_dtbo_path);
		if (package_node->load_drivers_dtbo_name != NULL)
			free(package_node->load_drivers_dtbo_name);
		if (package_node->load_drivers_dtbo_path != NULL)
			free(package_node->load_drivers_dtbo_path);
		if (package_node->load_image_overlay_pck_path != NULL)
			free(package_node->load_image_overlay_pck_path);
		if (package_node->load_drivers_overlay_pck_path != NULL)
			free(package_node->load_drivers_overlay_pck_path);
		if (package_node->dmabuf_info != NULL)
			free(package_node->dmabuf_info);
		if (package_node->aes_key != NULL)
			free(package_node->aes_key);

		free(package_node);
	} else
		return -DFX_DESTROY_PACKAGE_ERROR;

	return 0;
}

static int dfx_state(char *cmd, char *state)
{
	char buf[PLATFORM_STR_LEN];
	FILE *fptr;
	int len;

	system(cmd);
	len = strlen(state) + 1;
	fptr = fopen("state.txt", "r");
	if (fptr) {
		fgets(buf, len, fptr);
		fclose(fptr);
		system("rm state.txt");
		if (!strcmp(buf, state))
			return 0;
		else
			return 1;
	}

	return 1;
}

static int dfx_get_error(char *cmd)
{
	char string[PLATFORM_STR_LEN];
	FILE *fp;
	int c;

	system(cmd);
	fp = fopen("state.txt", "r");
	c = getc(fp);
	while(c!=EOF) {
		fscanf(fp, "%s", string);
		c = getc(fp);
	}

	fclose(fp);

	system("rm state.txt");

    return (int)strtol(string, NULL, 0);
}

static int dfx_package_load_dmabuf(struct dfx_package_node *package_node)
{
	int word_align = 0, index, fd, ret;
	struct dma_buf_sync sync = { 0 };
	struct dma_buffer_info info;
	long fileLen, count;
	char *dma_buf;
	FILE *fp;

	fp = fopen(package_node->load_image_path, "rb");
	if (fp == NULL) {
		printf("%s: File open failed\n", __func__);
		return -1;
	}

	//Get Bitstream/PDI Image Size
	fseek(fp, 0, SEEK_END);
	fileLen = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (package_node->xilplatform == ZYNQMP_PLATFORM) {
		word_align = fileLen % FPGA_WORD_SIZE;
		if(word_align)
			word_align = FPGA_WORD_SIZE - word_align;

		fileLen = fileLen + word_align;
	}

	package_node->dmabuf_info = (struct dma_buffer_info *) calloc(1,
					sizeof(struct dma_buffer_info));
	package_node->dmabuf_info->dma_buflen = fileLen;

	/* This call will do the following things
	 *   1. Allocate memory from the DMA pool and return a valid buffer fd
	 *   2. Create memory mapped buffer for the buffer fd
	 */
	ret = export_dma_buffer(package_node->dmabuf_info);
	if (ret < 0) {
		printf("%s: DMA buffer alloc failed\n", __func__);
		goto err_update;
	}

	/* DO Memory access synchronization */
	sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
	ret = ioctl(package_node->dmabuf_info->dma_buffd,
		    DMA_BUF_IOCTL_SYNC, &sync);
	if (ret) {
		printf("%s: sync start failed\n", __func__);
		goto unmap_buf;
	}

	/* Copy Bitfile/PDI image into the Dmabuf */
	if (word_align) {
		dma_buf = (char *) package_node->dmabuf_info->dma_buffer;
		for (index = 0; index < word_align; index++)
			dma_buf[index] = FPGA_DUMMY_BYTE;
		fileLen = fileLen - word_align;
		count = fread(&dma_buf[index], 1, fileLen, fp);
	} else
		count = fread((char *) package_node->dmabuf_info->dma_buffer, 1,
			      fileLen, fp);
	if (count != fileLen) {
		printf("%s: Image copy failed\n", __func__);
		goto unmap_buf;
	}

	sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
	ret = ioctl(package_node->dmabuf_info->dma_buffd,
		    DMA_BUF_IOCTL_SYNC, &sync);
	if (ret) {
		printf("%s: sync end failed\n", __func__);
		goto unmap_buf;
	}

	fclose(fp);

	return 0;

unmap_buf:
	close_dma_buffer(package_node->dmabuf_info);
err_update:
	fclose(fp);
	return -DFX_DMABUF_ALLOC_ERROR;

}

static int dfx_getplatform(void)
{
	char *zynqmpstr = "Xilinx ZynqMP FPGA Manager";
	char *Versalstr = "Xilinx Versal FPGA Manager";
	char fpstr[PLATFORM_STR_LEN];
	FILE *fptr;

	fptr = fopen("/sys/class/fpga_manager/fpga0/name", "r");
	if (fptr == NULL) {
		printf("Error! opening the platform file");
		return INVALID_PLATFORM;
	}

	// reads text until newline
	fscanf(fptr, "%[^\n]", fpstr);
	fclose(fptr);
	if (!strcmp(zynqmpstr, fpstr))
		return ZYNQMP_PLATFORM;
	else if (!strcmp(Versalstr, fpstr))
		return VERSAL_PLATFORM;
	else
		return INVALID_PLATFORM;
}

static int lengthOfLastWord2(const char *input)
{
	int result = 0;

	while (*input != '\0') {
		if (*input != ' ')
			result++;
		else
			result = 0;
		input++;
	}

	return result;
}

static int find_key(struct dfx_package_node *package_node)
{
	int len;
	char line[MAX_CMD_LEN];
	int find_result = 0;
	FILE *fp = fopen(package_node->load_aes_file_path, "r");

	package_node->aes_key = (char *) calloc(MAX_AES_KEY_LEN, sizeof(char));
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (strstr(line, "Key") != NULL) {
			len = strlen(line) - lengthOfLastWord2(line);
			strncpy(package_node->aes_key, line + len,
				MAX_AES_KEY_LEN);
			package_node->aes_key[MAX_AES_KEY_LEN] = '\0';
			find_result++;
			break;
		}
	}

	//Close the file if still open.
	if (fp)
		fclose(fp);

	if (find_result == 0) {
		free(package_node->aes_key);
		return -DFX_AESKEY_READ_ERROR;
	}

	return 0;
}

static inline void strlwr(char *destination, const char *source)
{
	while (*source) {
		*destination = tolower((unsigned char)*source);
		source++;
		destination++;
	}
}

static void zynqmp_print_err_msg(int err)
{
	int i, err_found = 0;

	if ((err & 0xFF) == XFPGA_VALIDATE_ERROR)
		printf("Error: Image validation");
	if ((err & 0xFF) == XFPGA_PRE_CONFIG_ERROR)
		printf("Error: Image Pre-configuration");
	if ((err & 0xFF) == XFPGA_WRITE_BITSTREAM_ERROR)
                printf("Error: Image write:");
	if ((err & 0xFF) == XFPGA_POST_CONFIG_ERROR)
                printf("Error: Image Post-configuration");
	if ((err & 0xFF) == XFPGA_OPS_NOT_IMPLEMENTED)
                printf("Error: Operation not supported");
	if ((err & 0xFF) == XFPGA_INVALID_PARAM)
                printf("Error: Invalid input parameters");

	for (i = 0; i < ZYNQMP_MAX_ERR; i++) {
		if (zynqmp_err[i].err_code == (err >> 8) & 0xFF) {
			printf(": %s\r\n", zynqmp_err[i].err_str);
			err_found++;
			break;
		}
	}

	if ((!err_found) && (err & 0xFF))
		printf("\r\n");
}

static int dfx_cfg_init_common(const char *dfx_package_path, const char *dfx_bin_file,
			       const char *dfx_dtbo_file, const char *dfx_driver_dtbo_file,
			       const char *dfx_aes_key_file, const char *devpath,
			       unsigned long flags)
{
	FPGA_NODE *package_node;
	int err, ret = 0;
	int platform;
	size_t len;

	platform = dfx_getplatform();
	if (platform == INVALID_PLATFORM) {
		printf("%s: fpga manager not enabled in the kernel Image\r\n", __func__);
		ret = -DFX_INVALID_PLATFORM_ERROR;
		goto END;
	}

	package_node = create_package();
	if (package_node == NULL) {
		printf("%s: create_package failed\r\n", __func__);
		ret = -DFX_CREATE_PACKAGE_ERROR;
		goto END;
	}

	package_node->xilplatform = platform;
	package_node->flags = flags;

	if (dfx_package_path == NULL) {
		ret = read_package_byname(package_node, dfx_bin_file,
					  dfx_dtbo_file, dfx_driver_dtbo_file,
					  dfx_aes_key_file);
		if (ret) {
			printf("%s: package read failed\r\n", __func__);
			goto destroy_package;
		}
	} else {
		/*Update package path */
		len = strlen(dfx_package_path);
		if (dfx_package_path[len-1] != '/') {
			/* one for extra char, one for trailing zero */
			package_node->package_path = (char *)malloc(len + 1 + 1);
			strcpy(package_node->package_path, dfx_package_path);
			package_node->package_path[len] = '/';
			package_node->package_path[len + 1] = '\0';
		} else {
			package_node->package_path = (char *)malloc(len + 1);
			strcpy(package_node->package_path, dfx_package_path);
		}

		ret = read_package_folder(package_node);
		if (ret) {
			printf("%s: package read failed\r\n", __func__);
			goto destroy_package;
		}
	}

	if (flags & DFX_ENCRYPTION_USERKEY_EN) {
		ret = find_key(package_node);
		if (ret) {
			printf("%s: fail to get key info\r\n", __func__);
			goto destroy_package;
		}
	}

	if (!(flags & DFX_EXTERNAL_CONFIG_EN)) {
		ret = dfx_package_load_dmabuf(package_node);
		if (ret) {
			printf("%s: load dmabuf failed\r\n", __func__);
			goto destroy_package;
		}
	}

	return package_node->package_id;

destroy_package:
	err = destroy_package(package_node->package_id);
	if (err)
		printf("%s:Destroy package failed \r\n", __func__);
END:
	return ret;
}

static int read_package_byname(struct dfx_package_node *package_node,
			       const char *dfx_bin_file, const char *dfx_dtbo_file,
			       const char *dfx_driver_dtbo_file,
			       const char *dfx_aes_key_file)
{
	int ret;
	char *str;
	char slen;

	if(dfx_bin_file != NULL) {
		package_node->load_image_path = strdup(dfx_bin_file);
		str = strdup(get_file_name_from_path(package_node->load_image_path));
		package_node->load_image_name = str;
		copy_file_to_firmware(dfx_bin_file);
	} else {
		return -DFX_READ_PACKAGE_ERROR;
	}

	if (dfx_dtbo_file != NULL) {
		package_node->load_image_dtbo_path = strdup(dfx_dtbo_file);
		str = strdup(get_file_name_from_path(package_node->load_image_dtbo_path));
		package_node->load_image_dtbo_name = str;
		slen = strlen(str) - 4;
		str = strndup(package_node->load_image_dtbo_name, slen);
		str[slen - 1] = '\0';
		package_node->package_name = str;
		copy_file_to_firmware(dfx_dtbo_file);
	} else {
		return -DFX_READ_PACKAGE_ERROR;
	}

	if(dfx_driver_dtbo_file != NULL) {
		package_node->load_drivers_dtbo_path = strdup(dfx_driver_dtbo_file);
		str = strdup(get_file_name_from_path(package_node->load_drivers_dtbo_path));
		package_node->load_drivers_dtbo_name = str;
		copy_file_to_firmware(dfx_driver_dtbo_file);
	} else {
		package_node->load_drivers_dtbo_path = NULL;
		package_node->load_drivers_dtbo_name = NULL;
	}

	if (dfx_aes_key_file != NULL) {
		package_node->load_aes_file_path = strdup(dfx_aes_key_file);
		str = strdup(get_file_name_from_path(package_node->load_aes_file_path));
		package_node->load_aes_file_name = str;
	} else {
		package_node->load_aes_file_path = NULL;
		package_node->load_aes_file_name = NULL;
	}

	return 0;
}

static char *get_file_name_from_path(char *full_path)
{
	char *ssc;
	int l = 0;
	char *path = full_path;

	ssc = strstr(path, "/");
	do{
		l = strlen(ssc) + 1;
		path = &path[strlen(path)-l+2];
		ssc = strstr(path, "/");
	}while(ssc);

	return path;
}

static void copy_file_to_firmware(const char *file)
{
	char command[MAX_CMD_LEN];

	snprintf(command, sizeof(command), "cp %s /lib/firmware/", file);
	system(command);
}

static bool file_exists(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    bool is_exist = false;
    if (fp != NULL)
    {
        is_exist = true;
        fclose(fp); // close the file
    }
    return is_exist;
}

static int validate_input_files(const char *dfx_bin_file,
				const char *dfx_dtbo_file,
				const char *dfx_driver_dtbo_file,
				const char *dfx_aes_key_file,
				unsigned long flags)
{
        int len, ret = 0;

        /* Validate Inputs */
	len = strlen(dfx_bin_file);
	if ((strcmp(dfx_bin_file + (len - 4), ".bit")) &&
	    (strcmp(dfx_bin_file + (len - 4), ".bin")) &&
            (strcmp(dfx_bin_file + (len - 4), ".pdi"))) {
		printf("%s: Invalid bitstream file extension\r\n", __func__);
		printf("%s: File extension should be .bit (or) .bin (or) .pdi\n", __func__);
		return -DFX_INVALID_PARAM;
	}

	if (!file_exists(dfx_bin_file)) {
		printf("%s: User provided bitstream file doesn't exist\r\n", __func__);
		return -DFX_INVALID_PARAM;
	}

	len = strlen(dfx_dtbo_file);
	if (strcmp(dfx_dtbo_file + (len - 5), ".dtbo")) {
		printf("%s: Invalid Overlay file extension\r\n", __func__);
		printf("%s: File extension should be .dtbo\n", __func__);
		return -DFX_INVALID_PARAM;
	}

	if (!file_exists(dfx_dtbo_file)) {
		printf("%s: User provided Overlay file doesn't exist\r\n", __func__);
		return -DFX_INVALID_PARAM;
	}

	if (dfx_driver_dtbo_file != NULL) {
		len = strlen(dfx_driver_dtbo_file);
		if (strcmp(dfx_driver_dtbo_file + (len - 5), ".dtbo")) {
			printf("%s: Invalid PL IP's Overlay file extension\r\n", __func__);
			printf("%s: File extension should be .dtbo\n", __func__);
			return -DFX_INVALID_PARAM;
		}

		if (!file_exists(dfx_driver_dtbo_file)) {
			printf("%s: User provided PL IP's Overlay file doesn't exist\r\n", __func__);
			return -DFX_INVALID_PARAM;
		}
	}

	if (flags & DFX_ENCRYPTION_USERKEY_EN) {
		len = strlen(dfx_aes_key_file);
		if (strcmp(dfx_aes_key_file + (len - 4), ".nky")) {
			printf("%s: Invalid AES key file extension\r\n", __func__);
			printf("%s: File extension should be .nky\n", __func__);
			return -DFX_INVALID_PARAM;
		}

		if (!file_exists(dfx_aes_key_file)) {
			printf("%s: User provided AES key file doesn't exist\r\n", __func__);
			return -DFX_INVALID_PARAM;
		}
	}
}

#ifdef ENABLE_LIBDFX_TIME
static inline double gettime(struct timeval  t0, struct timeval t1)
{
	return ((t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec -t0.tv_usec) / 1000.0f);
}
#endif
