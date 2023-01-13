/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * Copyright (C) 2023, Advanced Micro Devices, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 ***************************************************************/

/*This example programs the FPGA to have The base FPGA Region(static)
 * with  One PR region (This PR region has two RM's).
 *
 * Configure Base Image
 * 	-->Also called the "static image"
 * 	   An FPGA image that is designed to do full reconfiguration of the FPGA
 * 	   A base image may set up a set of partial reconfiguration regions that
 *	   may later be reprogrammed. 
 *	-->DFX_EXTERNAL_CONFIG flag should be set if the FPGA has already been
 *	   configured prior to OS boot up.
 *
 * Configure the PR Images
 *	-->An FPGA set up with a base image that created a PR region.
 *	   The contents of each PR may have multiple Reconfigurable Modules
 *	   This RM's(PR0-RM0, PR0-RM1) are reprogrammed independently while
 *	   the rest of the system continues to function.
 */

#include <stdio.h>
#include "libdfx.h"

int main()
{
	int  package_id_full, package_id_pr0_rm0, package_id_pr0_rm1, ret;

	/* package FULL Initilization */
    	package_id_full = dfx_cfg_init("/media/full/", 0, DFX_EXTERNAL_CONFIG_EN);
    	if (package_id_full < 0)
		return -1;
	printf("dfx_cfg_init: FULL Package completed successfully\r\n");

        /* package PR0-RM0 Initilization */
        package_id_pr0_rm0 = dfx_cfg_init("/media/pr0-rm0/", 0, 0);
        if (package_id_pr0_rm0 < 0)
                return -1;
	printf("dfx_cfg_init: PR0-RM0 Package completed successfully\r\n");

	/* package PR0-RM1 Initilization */
        package_id_pr0_rm1 = dfx_cfg_init("/media/pr0-rm1/", 0, 0);
        if (package_id_pr0_rm1 < 0)
                return -1;
	printf("dfx_cfg_init: PR0-RM1 Package completed successfully\r\n");
	
	/* Package FULL load */
	ret = dfx_cfg_load(package_id_full);
	if (ret) {
		dfx_cfg_destroy(package_id_full);
		return -1;
    	}
	printf("dfx_cfg_load: FULL Package completed successfully\r\n");

        /* Package PR0-RM0 load */
        ret = dfx_cfg_load(package_id_pr0_rm0);
        if (ret) {
                dfx_cfg_destroy(package_id_pr0_rm0);
                return -1;
        }
	printf("dfx_cfg_load: PR0-RM0 Package completed successfully\r\n");

        /* Remove PR0-RM0 package */
        ret = dfx_cfg_remove(package_id_pr0_rm0);
        if (ret)
                return -1;
	printf("dfx_cfg_remove: PR0-RM0 Package completed successfully\r\n"); 

        /* Package PR0-RM1 load */
        ret = dfx_cfg_load(package_id_pr0_rm1);
        if (ret) {
                dfx_cfg_destroy(package_id_pr0_rm1);
                return -1;
        }
	printf("dfx_cfg_load: PR0-RM1 Package completed successfully\r\n");
	
	/* Remove PR0-RM1 package */
    	ret = dfx_cfg_remove(package_id_pr0_rm1);
	if (ret)
		return -1;
	printf("dfx_cfg_remove: PR0-RM1 Package completed successfully\r\n");

        /* Remove Full package */
        ret = dfx_cfg_remove(package_id_full);
        if (ret)
                return -1;
	printf("dfx_cfg_remove: FULL Package completed successfully\r\n");

	/* Destroy PR0_RM0 package */
    	ret = dfx_cfg_destroy(package_id_pr0_rm0);
	if (ret)
		return -1;
	printf("dfx_cfg_destroy: PR0-RM0 Package completed successfully\r\n");

        /* Destroy PR0_RM1 package */
        ret = dfx_cfg_destroy(package_id_pr0_rm1);
        if (ret)
                return -1;
	printf("dfx_cfg_destroy: PR0-RM1 Package completed successfully\r\n");

        /* Destroy Full package */
        ret = dfx_cfg_destroy(package_id_full);
        if (ret)
                return -1;
	printf("dfx_cfg_destroy: FULL Package completed successfully\r\n");

	return 0;
}	
