/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * Copyright (C) 2026, Advanced Micro Devices, Inc. All Rights Reserved
 * SPDX-License-Identifier: MIT
 ***************************************************************/

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
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

#ifndef DTBO_ROOT_DIR
#define DTBO_ROOT_DIR "/sys/kernel/config/device-tree/overlays"
#endif


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
static int dfx_package_load_dmabuf(struct dfx_package_node *package_node,
				   const char *cma_file);
static int dfx_getplatform(void);
static int find_key(struct dfx_package_node *package_node);
static int lengthOfLastWord2(const char *input);
static void strlwr(char *destination, const char *source);
static int dfx_get_error(const char *state_buf);
static void zynqmp_print_err_msg(int err);
static int dfx_cfg_init_common(const char *dfx_package_path, const char *cma_file,
			       const char *dfx_bin_file, const char *dfx_dtbo_file,
			        const char *dfx_driver_dtbo_file,
			       const char *dfx_aes_key_file, const char *devpath,
			       unsigned long flags);
static int read_package_byname(struct dfx_package_node *package_node,
			       const char *dfx_bin_file, const char *dfx_dtbo_file,
			       const char *dfx_driver_dtbo_file,
			       const char *dfx_aes_key_file);
static char *get_file_name_from_path(char *full_path);
static int validate_input_files(const char *dfx_bin_file, const char *dfx_dtbo_file,
				const char *dfx_driver_dtbo_file, const char *dfx_aes_key_file,
				unsigned long flags);
static bool file_exists(const char *filename);
#ifdef ENABLE_LIBDFX_TIME
static inline double gettime(struct timeval  t0, struct timeval t1);
#endif

static void strip_trailing(char *haystack, char needle);
static int read_single_line(const char *path, char *buffer, size_t buf_size);
static int write_string_to_file(const char *path, const char *src);
static void remove_overlay_dir(const char *dir);

/**
 * strip_trailing() - Remove one trailing character from a string
 * @haystack:	 The null-terminated string to modify (in-place).
 * @needle:  The character to remove from the end of the string.
 *
 * Strips trailing needle from haystack - e.g. `\n` from file read
 * results or `/` from paths before concatenating.
 */
static void strip_trailing(char *haystack, const char needle)
{
	if (!haystack)
		return;

	const size_t len = strlen(haystack);
	if (len == 0)
		return;

	if (haystack[len - 1] == needle) {
		haystack[len - 1] = '\0';
	}
}

/**
 * read_single_line() - Read a single line from a file into a buffer.
 *
 * @path:			path of the file to read from
 * @buffer:			buffer to write the data into
 * @buf_size:		length of the provided `buffer` in bytes
 *
 * Reads exactly one line (up to newline or EOF) from @path.
 * Trailing newline is removed if present.
 *
 * Return:	0 on success
 *			-1 on failure
 */
static int read_single_line(const char *path,
							char *buffer,
							const size_t buf_size)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		printf("%s: Failed to open `%s` for reading\n", __func__, path);
		return -1;
	}

	if (!fgets(buffer, (int) buf_size, f)) {
		printf("%s: Failed to read from `%s`\n", __func__, path);
		fclose(f);
		return -1;
	}

	if (fclose(f) != 0) {
		printf("%s: Failed to close `%s`\n", __func__, path);
		return -1;
	}

	strip_trailing(buffer, '\n');
	return 0;
}

/**
 * write_string_to_file() - Write a string to a file safely
 * @path:	Path to the file to write
 * @src:	Null-terminated buffer containing the string to write
 *
 * This function opens @path for writing, writes the contents of @data,
 * and closes the file. All steps are checked for errors. On failure,
 * a detailed error message including errno is logged.
 *
 * Return:	0 on success
 *			-1 on failure
 */
static int write_string_to_file(const char *path, const char *src)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		printf("%s: Failed to open `%s` for writing\n", __func__, path);
		return -1;
	}

	if (fputs(src, f) == EOF) {
		printf("%s: Failed to write to `%s`\n", __func__, path);
		fclose(f); // attempt to close anyway
		return -1;
	}

	if (fclose(f) != 0) {
		printf("%s: Failed to close `%s` after writing\n", __func__, path);
		return -1;
	}

	printf("%s: `%s` written to `%s`\n", __func__, src, path);
	return 0;
}

/**
 * remove_overlay_dir() - remove device tree overlay from configfs interface.
 *
 * @dir:	the overlay directory to be removed
 *
 * This function attempts to remove the directory provided, with additional
 * logging.
 *
 */
static void remove_overlay_dir(const char *dir)
{
	if (rmdir(dir) != 0) {
		printf("%s: Failed to remove directory `%s`\n", __func__, dir);
	} else {
		printf("%s: Directory `%s` removed\n", __func__, dir);
	}
}

/**
 * dfx_get_fpga_state() - read the fpga state into `buffer`
 *
 * @buffer:			buffer to write the state into
 * @buf_size:		length of the provided `buffer` in bytes
 *
 * This static function checks the operational state of the FPGA by reading
 * the state from the sysfs interface.
 *
 * Return:	number of bytes read on success
 *			-1 on failure
 */
int dfx_get_fpga_state(char *buffer, const size_t buf_size)
{
	const char *state_file_path = "/sys/class/fpga_manager/fpga0/state";
	return read_single_line(state_file_path, buffer, sizeof(char) * buf_size);
}

/**
 * dfx_set_overlay_path(...) - write a requested overlay path to configfs
 *
 * @overlay_dir:	  Path to the overlay directory in configfs.
 * @requested_path:   The overlay path to write to the `path` attribute.
 *
 * This function writes the specified overlay path to the overlay's `path`
 * attribute in configfs. It attempts to write `requested_path` into the
 * `<overlay_dir>/path` file.
 *
 * Return:	0 on success,
 *			-1 on error (e.g., failed to open, write, or close the file)
 */
int dfx_set_overlay_path(const char *overlay_dir, const char *requested_path)
{
	char full_path[256];
	if (sizeof(full_path) < strlen(overlay_dir) + 6) { // '/' + '\0' + "path"
		printf("%s: Resulting path `%s` is too long for internal buffer (max: "
			   "%d)\n",
			   __func__, overlay_dir, MAX_CMD_LEN);
		return -1;
	}

	strcpy(full_path, overlay_dir);
	strip_trailing(full_path, '/');
	strcat(full_path, "/path");

	if (write_string_to_file(full_path, requested_path)) {
		printf("%s: Failed to apply the overlay - could not write to path file\n",
			   __func__);
		return -1;
	}
	return 0;
}

/**
 * dfx_set_fpga_firmware(...) - write a firmware binary name to the FPGA
 * manager
 *
 * @requested_binary_name: name of the firmware binary to load
 *
 * This function writes the specified firmware binary name to the FPGA manager's
 * firmware attribute in sysfs (`/sys/class/fpga_manager/fpga0/firmware`). This
 * triggers the FPGA manager to load the specified firmware onto the FPGA. All
 * file operations (open, write, close) are checked, and detailed error messages
 * including errno are reported if any operation fails.
 *
 * Return:	0 on success (firmware name successfully written),
 *			-1 on error (e.g., failed to open, write, or close the sysfs file)
 */
int dfx_set_fpga_firmware(const char *requested_binary_name)
{
	if (write_string_to_file("/sys/class/fpga_manager/fpga0/firmware",
							 requested_binary_name)) {
		printf("%s: Failed to write the bitstream ,-"
			   " could not write to firmware file\n",
			   __func__);
		return -1;
	}

	return 0;
}

/**
 * dfx_set_fpga_flags(...) - write a firmware binary name to the FPGA manager
 *
 * @flags: flag value to write - see user_load for more information.
 *
 * This function converts `flags` to a hex formatted string before writing
 * that string to the fpga flags attribute
 * (`/sys/class/fpga_manager/fpga0/flags`)
 *
 * Return:	0 on success (firmware name successfully written),
 *			-1 on error (e.g., failed to open, write, or close the sysfs file)
 */
int dfx_set_fpga_flags(const int flags)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%x", flags); // convert to hex
	if (write_string_to_file("/sys/class/fpga_manager/fpga0/flags", buf)) {
		printf("%s: Failed to set fpga flags - could not write to flags file\n",
			   __func__);
		return -1;
	}

	return 0;
}

/**
 * dfx_set_fpga_key(...) - write a AES key to the FPGA via the sysfs
 * interface
 *
 * @key: AES key string to write
 *
 * Return:	0 on success (key successfully written),
 *			-1 on error (e.g., failed to open, write, or close the sysfs file)
 */
int dfx_set_fpga_key(const char *key)
{
	if (write_string_to_file("/sys/class/fpga_manager/fpga0/key", key)) {
		printf("%s: Failed to set fpga flags - could not write to flags file\n",
			   __func__);
		return -1;
	}

	return 0;
}


/**
 * dfx_get_overlay_path(...) - read an overlay's path variable into
 * `dest_buffer`
 *
 * @overlay_dir:	path to the overlay directory in configfs
 * @buffer:			buffer to write the path into
 * @buf_size:		length of the provided `buffer` in bytes
 *
 * Return:	number of bytes read on success
 *			-1 on failure
 */
int dfx_get_overlay_path(const char *overlay_dir, char *buffer,
						 const size_t buf_size)
{
	char full_path[MAX_CMD_LEN];
	if (sizeof(full_path) < strlen(overlay_dir) + 6) { // + '/' + '\0' + "path"
		printf("%s: Resulting path `%s` is too long for internal buffer (max: "
			   "%d)\n",
			   __func__, overlay_dir, MAX_CMD_LEN);
		return -1;
	}

	strcpy(full_path, overlay_dir);
	strip_trailing(full_path, '/');
	strcat(full_path, "/path");

	return read_single_line(full_path, buffer, buf_size);
}

/**
 * dfx_get_overlay_status(...) - read an overlay's status variable into
 * `dest_buffer`
 *
 * @overlay_dir:	path to the overlay directory in configfs
 * @buffer:			buffer to write the status into
 * @buf_size:		length of the provided `buffer` in bytes
 *
 * Return:	number of bytes read on success
 *			-1 on failure
 */
int dfx_get_overlay_status(const char *overlay_dir, char *buffer,
						   const size_t buf_size)
{
	char full_path[MAX_CMD_LEN];
	if (sizeof(full_path) < strlen(overlay_dir) + 8) { // '/' + '\0' + "status"
		printf("%s: Resulting path `%s` is too long for internal buffer (max: "
			   "%d)\n",
			   __func__, overlay_dir, MAX_CMD_LEN);
		return -1;
	}

	strcpy(full_path, overlay_dir);
	strip_trailing(full_path, '/');
	strcat(full_path, "/status");

	return read_single_line(full_path, buffer, buf_size);
}

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
 * Optional parameters (using variadic arguments):
 * char *cma_file: (Optional) Custom CMA file path for DMA buffer allocation.
 *                 If NULL or not provided, defaults to standard paths:
 *                 "/dev/dma_heap/reserved" or "/dev/dma_heap/cma_reserved@800000000"
 *
 * Return: returns unique package_Id, or Error code on failure.
 */
int dfx_cfg_init(const char *dfx_package_path,
                 const char *devpath, unsigned long flags,
                 ...)
{
	int ret = 0;
	va_list args;
	const char *cma_file = NULL;

#ifdef ENABLE_LIBDFX_TIME
	struct timeval t1, t0;
	double time;

	gettimeofday(&t0, NULL);
#endif

	if (dfx_package_path == NULL) {
		printf("%s: Invalid input args\n", __func__);
		return -DFX_INVALID_PARAM;
	}

	va_start(args, flags);

	// Attempt to fetch next arg (optional cma_file)
	cma_file = va_arg(args, const char *);

	va_end(args);

	ret = dfx_cfg_init_common(dfx_package_path, cma_file,  NULL, NULL,
				  NULL, NULL, devpath, flags);

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
 * Optional parameters (using variadic arguments):
 * char *cma_file: (Optional) Custom CMA file path for DMA buffer allocation.
 *                 If NULL or not provided, defaults to standard paths:
 *                 "/dev/dma_heap/reserved" or "/dev/dma_heap/cma_reserved@800000000"
 *
 * unsigned long flags: Flags to specify any special instructions for the
 * library to perform.
 *
 * Return: returns unique package_Id or Error code on failure.
 */
int dfx_cfg_init_file(const char *dfx_bin_file, const char *dfx_dtbo_file,
		      const char *dfx_driver_dtbo_file, const char *dfx_aes_key_file,
		      const char *devpath, unsigned long flags, ...)
{
	va_list args;
	int len, ret = 0;
	const char *cma_file = NULL;
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

	va_start(args, flags);

	// Attempt to fetch next arg (optional cma_file)
	cma_file = va_arg(args, const char *);

	va_end(args);

	ret = dfx_cfg_init_common(NULL, cma_file, dfx_bin_file, dfx_dtbo_file,
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
	char path_buf[MAX_CMD_LEN];
	char *overlay_dir_path;
	char state_buf[128];
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
			printf("%s: Cannot open device file...\n", __func__);
			ret = -DFX_FAIL_TO_OPEN_DEV_NODE;
			goto END;
		}
		dfx_set_fpga_flags(package_node->flags);
		if (package_node->flags & DFX_ENCRYPTION_USERKEY_EN) {
			dfx_set_fpga_key(package_node->aes_key);
		}

        buffd = package_node->dmabuf_info->dma_buffd;
        /* Send dmabuf-fd to the FPGA Manager */
        ioctl(fd, DFX_IOCTL_LOAD_DMA_BUFF, &buffd);
        close(fd);
    }

	snprintf(path_buf, sizeof(path_buf), "%s/%s_image_%lu", DTBO_ROOT_DIR,
			 package_node->package_name, package_node->package_id);

	dfx_set_firmware_search_path(package_node->load_image_path);

	len = strlen(path_buf) + 1;
	overlay_dir_path = (char *) calloc(len, sizeof(char));
	strncpy(overlay_dir_path, path_buf, len);
	package_node->load_image_overlay_pck_path = overlay_dir_path;
	if (mkdir(package_node->load_image_overlay_pck_path, 0755)) {
		printf("%s: Failed to create overlay dir `%s`\n", __func__,
			   package_node->load_image_overlay_pck_path);
		return -1;
	}
	printf("%s: Created overlay at `%s`\n", __func__,
		   package_node->load_image_overlay_pck_path);


#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&load_t0, NULL);
#endif
	// Trigger overlay load
	dfx_set_overlay_path(package_node->load_image_overlay_pck_path,
						 package_node->load_image_dtbo_name);
#ifdef ENABLE_LIBDFX_TIME
	gettimeofday(&load_t1, NULL);
#endif
	// check FPGA state is operating
	if (!(package_node->flags & DFX_EXTERNAL_CONFIG_EN)) {
		dfx_get_fpga_state(state_buf, sizeof(state_buf));
		if (strcmp(state_buf, "operating") != 0) {
			err = dfx_get_error(path_buf);
			remove_overlay_dir(package_node->load_image_overlay_pck_path);
			printf("%s: Image configuration failed with error: 0x%x\n", __func__,
				   err);
			if (package_node->xilplatform == ZYNQMP_PLATFORM)
				zynqmp_print_err_msg(err);
			dfx_set_firmware_search_path("");
			ret = -DFX_IMAGE_CONFIG_ERROR;
			goto END;
		}
	}

	// Check that the overlay path is still written
	dfx_get_overlay_path(package_node->load_image_overlay_pck_path, state_buf,
						 sizeof(state_buf));
	if (strcmp(state_buf, package_node->load_image_dtbo_name) != 0) {
		printf("%s: Image configuration failed\n", __func__);
		remove_overlay_dir(package_node->load_image_overlay_pck_path);
		dfx_set_firmware_search_path("");
		ret = -DFX_IMAGE_CONFIG_ERROR;
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
	char path_buf[MAX_CMD_LEN];
	int len, ret = 0;
	char *overlay_dir_path;
	char state_buf[128] = {};
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

	dfx_set_firmware_search_path(package_node->load_drivers_dtbo_path);

	snprintf(path_buf, sizeof(path_buf),
			 "%s/%s_driver_%lu",
			 DTBO_ROOT_DIR,
			 package_node->package_name,
			 package_node->package_id);
	len = (int)strlen(path_buf) + 1;
	overlay_dir_path = (char *) calloc((len), sizeof(char));
	strncpy(overlay_dir_path, path_buf, len);
	package_node->load_drivers_overlay_pck_path = overlay_dir_path;

	if (mkdir(package_node->load_image_overlay_pck_path, 0755)) {
		printf("%s: Failed to create overlay dir `%s`\n",
			   __func__, package_node->load_image_overlay_pck_path);
		return -1;
	}

	dfx_set_overlay_path(package_node->load_drivers_overlay_pck_path,
						 package_node->load_drivers_dtbo_name);

	// Check that the overlay path is still written
	dfx_get_overlay_path(package_node->load_image_overlay_pck_path, state_buf,
						 sizeof(state_buf));
	if (strcmp(state_buf, package_node->load_image_dtbo_name) != 0) {
		printf("%s: Drivers DTBO config failed\n", __func__);
		remove_overlay_dir(package_node->load_image_overlay_pck_path);
		dfx_set_firmware_search_path("");
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
			remove_overlay_dir(package_node->load_drivers_overlay_pck_path);
		}
	}

	if (package_node->load_image_overlay_pck_path != NULL) {
		FD = opendir(package_node->load_image_overlay_pck_path);
		if (FD) {
			closedir(FD);
			remove_overlay_dir(package_node->load_image_overlay_pck_path);
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

	dfx_set_firmware_search_path(binfile);
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
	dfx_set_firmware_search_path("");
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

	FD = opendir(DTBO_ROOT_DIR);
	if (FD)
		closedir(FD);
	else {
		return 0;
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

/**
 *
 * @param state_buf buffer already containing the state string from fpga
 * @return the contents of state_buf converted to signed integer
 */
static int dfx_get_error(const char *state_buf)
{
	return (int) strtol(state_buf, NULL, 0);
}

static int dfx_package_load_dmabuf(struct dfx_package_node *package_node,
				   const char *cma_file)
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
	package_node->dmabuf_info->cma_file = cma_file;

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

static int dfx_cfg_init_common(const char *dfx_package_path,
			       const char *cma_file, const char *dfx_bin_file,
			       const char *dfx_dtbo_file,
			       const char *dfx_driver_dtbo_file,
			       const char *dfx_aes_key_file,
			       const char *devpath,
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
		ret = dfx_package_load_dmabuf(package_node, cma_file);
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
		dfx_set_firmware_search_path(dfx_bin_file);
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
		dfx_set_firmware_search_path(dfx_dtbo_file);
	} else {
		return -DFX_READ_PACKAGE_ERROR;
	}

	if(dfx_driver_dtbo_file != NULL) {
		package_node->load_drivers_dtbo_path = strdup(dfx_driver_dtbo_file);
		str = strdup(get_file_name_from_path(package_node->load_drivers_dtbo_path));
		package_node->load_drivers_dtbo_name = str;
		dfx_set_firmware_search_path(dfx_driver_dtbo_file);
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

/**
 * Extract the parent dir of the target path, and write that location to
 * /sys/module/firmware_class/parameters/path so that the kernel can discover
 * the firmware within
 *
 * @param file_path the full path to the file to be loaded
 *
 * Return:	0 on success
 *			-1 on failure
 */
int dfx_set_firmware_search_path(const char *file_path)
{
	char path_copy[512];
	char *parent_dir;
	int fd = -1;
	int rc = -1;
	const char *lookup_control =
		"/sys/module/firmware_class/parameters/path";

	if (!file_path) {
		printf("%s: ERROR: path provided is NULL\n", __func__);
		goto END;
	}

	if (strlen(file_path) >= sizeof(path_copy)) {
		printf("%s: WARN: path provided is too long, truncating\n", __func__);
	}

	strlcpy(path_copy, file_path, sizeof(path_copy));
	if (file_path[0]) {
		printf("%s: Setting lookup path for `%s`\n", __func__, file_path);
	}

	// derive parent directory
	parent_dir = dirname(path_copy);
	if (!strcmp(".", parent_dir)) {
		parent_dir = "";
	}

	fd = open(lookup_control, O_WRONLY);
	if (fd < 0) {
		printf("%s: ERROR: failed to open firmware path parameter\n", __func__);
		goto END;
	}

	printf("%s: Writing `%s` to %s\n", __func__, parent_dir, lookup_control);
	if (write(fd, parent_dir, strlen(parent_dir)) < 0) {
		printf("%s: ERROR: failed to write firmware lookup path\n", __func__);
		goto END;
	}

	rc = 0;

	END:
		// always close if opened, not just on fail
		if (fd >= 0) {
			close(fd);
		}

	if (rc < 0) {
		printf("%s: WARN: Failed to set firmware search path. "
			   "Success only possible if the requested files live in "
			   "defaults.\nSee "
			   "https://docs.kernel.org/driver-api/firmware/"
			   "fw_search_path.html for more information\n",
			   __func__);
	}

	return rc;
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
        int len;

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

	return 0;
}

#ifdef ENABLE_LIBDFX_TIME
static inline double gettime(struct timeval  t0, struct timeval t1)
{
	return ((t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec -t0.tv_usec) / 1000.0f);
}
#endif
