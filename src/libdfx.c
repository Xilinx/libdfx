/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
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
	FPGA_NODE *package_node;
	int platform;
	int ret, err;
	size_t len;

	platform = dfx_getplatform();
	if (platform == INVALID_PLATFORM) {
		printf("%s: fpga manager not enabled in the kernel Image\r\n",
			__func__);
		return -DFX_INVALID_PLATFORM_ERROR;
	}

	package_node = create_package();
	if (package_node == NULL) {
		printf("%s: create_package failed\r\n", __func__);
		return -DFX_CREATE_PACKAGE_ERROR;
	}

	package_node->xilplatform = platform;
	package_node->flags = flags;

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
	int len, ret, fd, buffd;
	char command[MAX_CMD_LEN];
	char *str;
	DIR *FD;

	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		return -DFX_INVALID_PACKAGE_ID_ERROR;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		return -DFX_GET_PACKAGE_ERROR;
	}

	if (!(package_node->flags & DFX_EXTERNAL_CONFIG_EN)) {
		fd = open("/dev/fpga0", O_RDWR);
		if (fd < 0) {
			printf("%s: Cannot open device file...\n",
			       __func__);
			return -DFX_FAIL_TO_OPEN_DEV_NODE;
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
	system(command);

	if (!(package_node->flags & DFX_EXTERNAL_CONFIG_EN)) {
		snprintf(command, sizeof(command),
			 "cat /sys/class/fpga_manager/fpga0/state >> state.txt");
		ret = dfx_state(command, "operating");
		if (ret) {
			snprintf(command, sizeof(command), "rmdir %s",
				 package_node->load_image_overlay_pck_path);
			system(command);
			printf("%s: Image configuration failed\n", __func__);
			snprintf(command, sizeof(command),
				 "cat /sys/class/fpga_manager/fpga0/state");
			system(command);
			return -DFX_IMAGE_CONFIG_ERROR;
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
		return -DFX_IMAGE_CONFIG_ERROR;
	}

	return 0;
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
	int len, ret;
	char *str;

	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		return -DFX_INVALID_PACKAGE_ID_ERROR;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		return -DFX_GET_PACKAGE_ERROR;
	}

	if (package_node->load_drivers_dtbo_path == NULL)
		return -DFX_NO_VALID_DRIVER_DTO_FILE;

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
		return -DFX_DRIVER_CONFIG_ERROR;
	}

	return 0;
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
	DIR *FD;

	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		return -DFX_INVALID_PACKAGE_ID_ERROR;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		return -DFX_GET_PACKAGE_ERROR;
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

	return 0;
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

	if (package_id < 0) {
		printf("%s: Invalid package id\n", __func__);
		return -DFX_INVALID_PACKAGE_ID_ERROR;
	}

	package_node = get_package(package_id);
	if (package_node == NULL) {
		printf("%s: fail to get package_node\n", __func__);
		return -DFX_GET_PACKAGE_ERROR;
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

	return destroy_package(package_node->package_id);
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
	int platform, count = 0;
	FILE* fd;

	platform = dfx_getplatform();
	if (platform != VERSAL_PLATFORM)
		return -DFX_INVALID_PLATFORM_ERROR;

	fd = fopen(filename, "rb");
	if (!fd) {
		printf("Unable to open file!");
		return -DFX_FAIL_TO_OPEN_BIN_FILE;
	}

	while(!feof(fd)) {
		fread(&buffer[count], sizeof(int), 1,fd);
		count++;
	}

	fclose(fd);
	count = (count - 1) *  sizeof(int);

	return count;
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
	int platform, count = 0;
	FILE* fd;
	DIR *FD;

	platform = dfx_getplatform();
	if (platform != VERSAL_PLATFORM)
		return -DFX_INVALID_PLATFORM_ERROR;

	fd = fopen(binfile, "rb");
	if (!fd) {
		printf("Unable to open binary file!");
		return -DFX_FAIL_TO_OPEN_BIN_FILE;
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
		return -DFX_FAIL_TO_OPEN_BIN_FILE;
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

	return count * sizeof(int);
}

static int read_package_folder(struct dfx_package_node *package_node)
{
	bool is_image_dtbo = false, is_dtbo = false;
	bool is_bin = false, is_bit = false;
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
					is_bin = true;
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
					is_bit = true;
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
					is_image_dtbo = true;
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
					is_dtbo = true;
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
				}
			}
			free(file_name);
		}
		closedir(FD);
	}

	if (is_bit == true && is_bin == true)
		return -DFX_DUPLICATE_FIRMWARE_ERROR;
	if (is_image_dtbo == true && is_dtbo == true)
		return -DFX_DUPLICATE_DTBO_ERROR;

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

static int dfx_package_load_dmabuf(struct dfx_package_node *package_node)
{
	struct dma_buf_sync sync = { 0 };
	struct dma_buffer_info info;
	long fileLen, count;
	int fd, ret;
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

	package_node->dmabuf_info = (struct dma_buffer_info *) calloc(1,
					sizeof(struct dma_buffer_info));
	package_node->dmabuf_info->dma_buflen = fileLen;

	/* This call will do the following things
	 *      1. Create an ION client
	 *      2. Query ION heap_id_mask from ION heap
	 *      3. Allocate memory for this ION client as per heap_type and
	 *         return a valid buffer fd
	 *      4. Create memory mapped buffer for the buffer fd
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
