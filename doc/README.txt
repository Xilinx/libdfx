libdfx - README
	->Introduction
	->Limitations
	->Images required for Testing
	->API Details
	->Example Application flow
	->Build procedure

=============	
Introduction:
=============
	The library is a lightweight user-space library that provides APIs
for application to configure the PL. Built on top of the driver stack that
supports the FPGA device, the library abstracts away hardware specific details

============
Limitations:
============
	->Libdfx is currently limited to supporting Zynq UltraScale+ MPSoC and
Versal platforms
	->Input package/folder should contain only one bitstream/PDI image file
and its relevant overlay file
	->To use the deferred probe functionality Both Image DTBO and relevant
Drivers DTBO files are mandatory
		->The Image dtbo file extension should be _i.dtbo 
			Ex: design_image_i.dtbo
		->The Drivers dtbo file extension should be _d.dtbo
			Ex: design_drivers_d.dtbo

============================
Images required for Testing:
============================
	->PDI/BIN/BIT file
	->DTBO file
The Device Tree Overlay (DTO) is used to reprogram an FPGA while Linux is
running. The DTO overlay will add the child node and the fragments from the
.dtbo file to the base device tree, The newly added device node/drivers will
be probed after PDI /BIN/BIT programming.

=============
DTO contains:
=============
Target FPGA Region
- "target-path" or "target" - The insertion point where the contents of the
			      overlay will go into the live tree.target-path
			      is a full path, while target is a phandle.
FPGA Image firmware file name
- "firmware-name" - Specifies the name of the FPGA image file on the firmware
		    search path. The search path is described in the firmware
		    class documentation.

Image specific information
- fpga-config-from-dmabuf : boolean, set if the FPGA configured done
			    pre-allocated dma-buffer.

Child devices
- child nodes corresponding to hardware that will be loaded in this region
  of the FPGA.
============================================================================
Devicetree Overlay file contents example: For Only PDI/BIN/BIT configuration
============================================================================
/dts-v1/;
/plugin/;
/ {
        fragment@0 {
                target = <&fpga>;
                overlay0: __overlay__ {
                        firmware-name = "partial_1.pdi";
                        partial-fpga-config;
                        fpga-config-from-dmabuf;
                };
        };
};

========================================
PL drivers probing ( For Deferred Probe)
========================================
/dts-v1/;
/plugin/;
/ {
	fragment@0 {
		target = <&amba>;
		__overlay__ {
			axi_gpio_0: gpio@a0000000 {
                                #gpio-cells = <3>;
                                clock-names = "s_axi_aclk";
                                clocks = <&zynqmp_clk 71>;
                                compatible = "xlnx,axi-gpio-2.0", "xlnx,xps-gpio-1.00.a";
                                gpio-controller ;
                                reg = <0x0 0xa0000000 0x0 0x1000>;
                                xlnx,all-inputs = <0x0>;
                                xlnx,all-inputs-2 = <0x0>;
                                xlnx,all-outputs = <0x1>;
                                xlnx,all-outputs-2 = <0x0>;
                                xlnx,dout-default = <0x00000000>;
                                xlnx,dout-default-2 = <0x00000000>;
                                xlnx,gpio-width = <0x8>;
                                xlnx,gpio2-width = <0x20>;
                                xlnx,interrupt-present = <0x0>;
                                xlnx,is-dual = <0x0>;
                                xlnx,tri-default = <0xFFFFFFFF>;
                                xlnx,tri-default-2 = <0xFFFFFFFF>;
                        };
		};
	};
};

====================================================================================
Devicetree Overlay file contents example: For PDI configuration + PL drivers probing
====================================================================================
/dts-v1/;
/plugin/;
/ {
        fragment@0 {
                target = <&fpga>;
                overlay0: __overlay__ {
                        firmware-name = "partial_1.pdi";
                        partial-fpga-config;
                        fpga-config-from-dmabuf;
                };
		fragment@1 {
                target = <&amba>;
                overlay1: __overlay__ {
			axi_gpio_0: gpio@a0000000 {
                                #gpio-cells = <3>;
                                clock-names = "s_axi_aclk";
                                clocks = <&zynqmp_clk 71>;
                                compatible = "xlnx,axi-gpio-2.0", "xlnx,xps-gpio-1.00.a";
                                gpio-controller ;
                                reg = <0x0 0xa0000000 0x0 0x1000>;
                                xlnx,all-inputs = <0x0>;
                                xlnx,all-inputs-2 = <0x0>;
                                xlnx,all-outputs = <0x1>;
                                xlnx,all-outputs-2 = <0x0>;
                                xlnx,dout-default = <0x00000000>;
                                xlnx,dout-default-2 = <0x00000000>;
                                xlnx,gpio-width = <0x8>;
                                xlnx,gpio2-width = <0x20>;
                                xlnx,interrupt-present = <0x0>;
                                xlnx,is-dual = <0x0>;
                                xlnx,tri-default = <0xFFFFFFFF>;
                                xlnx,tri-default-2 = <0xFFFFFFFF>;
                        };
		};

	};
};

======
Note:1
======
	Upstream Linux kernel changes coming in v5.0 and above versions will
affect the use of Device Tree (DT) overlays.These changes introduce restriction
that may require users to adjust their device tree overlays. Fortunately the
adjustments required are small. However, even with adjustments users will
experience new warnings.With continued upstream improvements, the new warnings
are expected to subside in the future.

For more info please refer this below link:
https://rocketboards.org/foswiki/Documentation/UpstreamV50KernelDeviceTreeChanges

We can ignore these warnings for now.

Create Device Tree Overlay Blob (.dtbo) file from the partial.dts file
	# dtc -O dtb -o partial.dtbo -b 0 -@ partial.dtsi
	Ex: ./scripts/dtc/dtc -O dtb -o partial.dtbo -b 0 -@ partial.dts

Copy the generated PDI and dtbo files into the target package folder 

=======
Note: 2
=======
       Nodes in PR region should not be the part of Base Device-tree(system.dtb)
If these nodes are part of static dtb (system.dtb), these nodes cannot be
removed at run-time.

============
API Details:
============

=================
CMA File Support:
=================
        libdfx now supports optional custom CMA (Contiguous Memory Allocator) file
specification for DMA buffer allocation. This feature provides flexibility for
different platform configurations and reserved memory setups.

Default DMA Heap Paths:
        - /dev/dma_heap/reserved
        - /dev/dma_heap/cma_reserved@800000000

Custom CMA File Usage:
        - Users can specify a custom CMA file path as an optional parameter
        - If provided, libdfx will attempt to use the custom path first
        - If the custom path fails, it falls back to default paths
        - This allows for platform-specific memory configurations

Examples of custom CMA paths:
        - /dev/dma_heap/custom_cma
        - /dev/dma_heap/cma_reserved@specific_address
        - /dev/dma_heap/platform_specific_heap
=================================================================
 -pre-fetch:  dfx_cfg_init (const char *dfx_package_path,
			    const char *devpath, u32 flags, ...);
=================================================================

/* Provide a generic interface to the user to specify the required parameters
 * for PR programming.
 * The calling process must call this API before it performs -load/remove.
 *
 * char *dfx_package_path: The contents of the package folder should look
 * something as below:
 *                                -package: //-package1  
 *					|--> Bit_file
 *					|--> DT_Overlayfile
 *
 * char *devpath: Unused for now. The dev interface for now is always exposed
 * at /dev/fpga0
 *
 * unsigned long flags: Flags to specify any special instructions for library
 * to perform.
 *
 * Optional parameters (using variadic arguments):
 * char *cma_file: (Optional) Custom CMA file path for DMA buffer allocation.
 *                 If NULL or not provided, defaults to standard paths:
 *                 "/dev/dma_heap/reserved" or "/dev/dma_heap/cma_reserved@800000000"
 *
 * Return: returns unique package_Id or Error code on failure.
 */ 


Usage example:
#include "libdfx.h"

 package_id1, package_id2;

/* More code */
/* -store /Pre-fetch data */
package_id1 = dfx_cfg_init ("/path/package1/", "/dev/fpga0", flags);

/* More code */

/* -store /Pre-fetch data */
package_id2 = dfx_cfg_init ("/path/package2/", "/dev/fpga0", flags);

/* More code */

/* -store /Pre-fetch data  - For custom CMA file use case*/
package_id3 = dfx_cfg_init ("/path/package3/", "/dev/fpga0", flags,
                            "/dev/dma_heap/custom_cma");

/* More code */

=============================================================================
 -pre-fetch int dfx_cfg_init_file(const char *dfx_bin_file,
				  const char *dfx_dtbo_file,
				  const char *dfx_driver_dtbo_file,
				  const char *dfx_aes_key_file,
				  const char *devpath,
                                  unsigned long flags, ...);
=============================================================================

/* Provide a generic interface to the user to specify the required parameters
 * for FPGA programming. To pass the required inputs user should provide the
 * absolute file paths.
 * The calling process must call this API before it performs -load/remove.
 *
 * const char *dfx_bin_file: Absolute pdi/bistream file path.
 *The one user wants to load.
 *		-Ex:/lib/firmware/xilinx/example/example.pdi
 *
 * const char *dfx_dtbo_file: Absolute relevant dtbo file path
 *		-Ex: /lib/firmware/xilinx/example/example.dtbo
 *
 * const char *dfx_driver_dtbo_file: Absolute relevant dtbo file path
 *		- Ex: /lib/firmware/xilinx/example/drivers.dtbo (or) NULL
 * Note: To use the deferred probe functionality Both Image DTBO and relevant
 * Drivers DTBO files are mandatory for other use cases user should pass "NULL".
 *
 * char *dfx_aes_key_file: Absolute relevant aes key file path
 * 		Ex: /lib/firmware/xilinx/example/Aes_key.nky (or) NULL
 * Note: If the bitstream is encrypted with the user-key then the user needs to
 * pass relevant aes_key.key file for other use cases user should pass "NULL".
 *
 * char *devpath: The dev interface is exposed at /dev/fpga-deviceN.
 * Where N is the interface-device number.
 *
 * unsigned long flags: Flags to specify any special instructions for the
 * library to perform.
 *
 * Optional parameters (using variadic arguments):
 * char *cma_file: (Optional) Custom CMA file path for DMA buffer allocation.
 *                 If NULL or not provided, defaults to standard paths:
 *                 "/dev/dma_heap/reserved" or "/dev/dma_heap/cma_reserved@800000000"
 *
 * Return: returns unique package_Id or Error code on failure.
 */


Usage example:
#include "libdfx.h"

 package_id1, package_id2, package_id3

/* More code */
/* -store /Pre-fetch data  - For Normal Images*/
package_id1 = dfx_cfg_init_file("/lib/firmware/xilinx/example/example.bin ",
				"/lib/firmware/xilinx/example/ex/ample.dtbo",
				NULL, NULL, /dev/fpga0", flags);

/* More code */

/* -store /Pre-fetch data  - For user-key encrypted bitstream use case*/
package_id2 = dfx_cfg_init_file("/lib/firmware/xilinx/example/example.bin ",
				"/lib/firmware/xilinx/example/ex/ample.dtbo",
				NULL, "/lib/firmware/xilinx/example/Aes_key.nky",
				/dev/fpga0", flags);

/* More code */

/* -store /Pre-fetch data  - For deferred drivers probe use case*/
package_id3 = dfx_cfg_init_file("/lib/firmware/xilinx/example/example.bin ",
				"/lib/firmware/xilinx/example/ex/pl_only_config.dtbo",
				"/lib/firmware/xilinx/example/drivers.dtbo", NULL,
				/dev/fpga0", flags);
/* More code */

/* -store /Pre-fetch data  - For custom CMA file use case*/
package_id4 = dfx_cfg_init_file("/lib/firmware/xilinx/example/example.bin ",
                                "/lib/firmware/xilinx/example/example.dtbo",
                                NULL, NULL, "/dev/fpga0", flags,
                                "/dev/dma_heap/custom_cma");
/* More code */

================================================================
 -fpga-load:  int dfx_cfg_load(struct dfx_package_Id package_Id)
================================================================

/* This API is Responsible for the following things.
 *      -->Load  into the PL
 *      -->Probe the Drivers which are relevant to the Bitstream as per
 *	   DT overlay mentioned in dfx_package folder
 *
 * package_id: Unique package_id value which was returned by dfx_cfg_init.
 * 
 * Return: returns zero on success or Error code on failure.
 */ 


Usage example:
#include "libdfx.h"

/* More code */

ret = dfx_cfg_load (package_id);
if (ret)
	return -1

/* More code */

====================================================================================
 -Deferred-drivers-load:  int dfx_cfg_drivers_load(struct dfx_package_Id *package_Id)
====================================================================================

/* This API is Responsible for the following things.
 *      -->Probe the Drivers which are relevant to the Bitstream as per
 *         DT overlay mentioned in dfx_package folder(With name: *_d.dtbo)
 *
 * package_id: Unique package_id value which was returned by dfx_cfg_init.
 *
 * Return: returns zero on success or Error code on failure.
 */


Usage example:
#include "libdfx.h"

/* More code */

ret = dfx_cfg_drivers_load(package_id);
if (ret)
        return -1

/* More code */

==========================================================
 -Remove: dfx_cfg_remove(struct dfx_package_Id package_Id)
==========================================================

/* This API is Responsible for unloading the drivers corresponding to a package
 *     
 * package_id: Unique package_id value which was returned by dfx_cfg_init.
 * 
 * Return: returns zero on success or Error code on failure.
 */ 

Usage example:
#include "libdfx.h"

/* More code */

ret = dfx_cfg_remove (package_id);
 if (ret)
	return -1;
/* More code */

====================================================================
 -Destroy package: dfx_cfg_destroy(struct dfx_package_Id package_Id)
====================================================================

/* This API frees the resources allocated during dfx_cfg_init.
 *     
 * package_id: Unique package_id value which was returned by dfx_cfg_init.
 * 
 * Return: returns zero on success.  On error, -1 is returned.
 */ 


Usage example:
#include "libdfx.h"

/* More code */

 ret = dfx_cfg_destroy (package_id);
 if (ret)
	return -1

/* More code */

=========================
Example Application flow:
=========================
#include "libdfx.h"

int main()
{
 package_id, ret;


	/* package initialization */
    package_id = dfx_cfg_initpck1/, /dev/fpga0, 0);
     if (package_id < 0) 
		 return -1;
               

	/* Package load */
	ret = dfx_cfg_load(package_id);
	if (ret) 
		return -1;

	/* Remove package */
    ret = dfx_cfg_remove(package_id);
	if (ret)
		return -1;

	/* Destroy package */
    ret = dfx_cfg_destroy(package_id);

	return ret;	

}

====================================================================
 -Active UID info list: dfx_get_active_uid_list(int *buffer)
====================================================================
/* This API populates buffer with {Node ID, Unique ID, Parent Unique ID, Function ID}
 * for each applicable NodeID in the system.
 *
 * buffer: User buffer address
 *
 * Return: Number of bytes read from the firmware in case of success.
 *         or Negative value on failure.
 *
 * Note: The user buffer size should be 768 bytes.
 *
 */

Usage example:
#include "libdfx.h"

/* More code */

 ret = dfx_get_active_uid_list(&buffer);
 if (ret < 0)
        return -1

/* More code */

=================================================================================
 -Meta-header info: dfx_get_meta_header(char *binfile, int *buffer, int buf_size)
=================================================================================
/* This API populates buffer with meta-header info related to the user
 * provided binary file (BIN/PDI).
 *
 * binfile: PDI Image.
 * buffer: User buffer address
 * buf_size : User buffer size.
 *
 * Return: Number of bytes read from the firmware in case of success.
 *         or Negative value on failure.
 */

Usage example:
#include "libdfx.h"

/* More code */

  ret = dfx_get_meta_header("/media/binary.bin", &buffer, buf_size);
  if (ret < 0)
	return -1

/* More code */

=================================================================================
-FPGA state info: dfx_get_fpga_state(char *buffer, size_t buf_size)
=================================================================================

/* This API populates the user-provided buffer with the current operational
* state of the FPGA as reported by the FPGA manager sysfs interface.
*
* buffer:   User buffer address to receive the FPGA state string.
* buf_size: Size of the user-provided buffer in bytes.
*
* Return: Number of bytes read from sysfs in case of success.
*         or Negative value on failure.
*/

Usage example:
#include "libdfx.h"

/* More code */

char buffer[256];
ret = dfx_get_fpga_state(buffer, sizeof(buffer));
if (ret < 0 || strcmp(buffer, "operating") != 0)
    return -1;

/* buffer now contains the FPGA state string */

/* More code */

=================================================================================
-Overlay path config: dfx_set_overlay_path(const char *overlay_dir,
const char *requested_path)
=================================================================================

/* This API writes the requested overlay path to the overlay's `path`
* attribute in configfs.
* This function does not check that the path stays set - use
* dfx_get_overlay_path and dfx_get_overlay_status to check that the application
* of an overlay was successful.
*
* overlay_dir:     Path to the overlay directory in configfs.
* requested_path: Overlay path string to be written.
*
* Return: 0 on success or Negative value on failure.
*/

Usage example:
#include "libdfx.h"

/* More code */

ret = dfx_set_overlay_path("/sys/kernel/config/device-tree/overlays/my_overlay",
                           "/home/user/firmwares/my_overlay.dtbo");
if (ret < 0)
    return -1;

/* More code */

=================================================================================
-FPGA firmware load: dfx_set_fpga_firmware(const char *requested_binary_name)
=================================================================================

/* This API triggers the FPGA manager to load the specified firmware
* binary by writing its name to the FPGA manager sysfs interface.
* This function does not check that the firmware application succeeded.
* Use dfx_get_fpga_state to check that the fpga was successful programmed.
*
* requested_binary_name: Name of the firmware binary to load (not full path)
*
* Return: 0 on success or Negative value on failure.


*/

Usage example:
#include "libdfx.h"

/* More code */

ret = dfx_set_fpga_firmware("my_bitstream.bit.bin");
if (ret < 0)
    return -1;

/* More code */

=================================================================================
-FPGA flags config: dfx_set_fpga_flags(const int flags)
=================================================================================

/* This API sets FPGA manager flags by writing a hex-formatted flag
* value to the FPGA manager sysfs interface.
*
* flags: Integer flag value to apply.
*
* Return: 0 on success or Negative value on failure.
*/

Usage example:
#include "libdfx.h"

/* More code */

ret = dfx_set_fpga_flags(0x20);
if (ret < 0)
    return -1;

/* More code */

=================================================================================
-FPGA key config: dfx_set_fpga_key(const char *key)
=================================================================================

/* This API writes an AES key string to the FPGA manager via the
* sysfs interface.
*
* key: AES key string to be written.
*
* Return: 0 on success or Negative value on failure.
*/

Usage example:
#include "libdfx.h"

/* More code */

ret = dfx_set_fpga_key("0123456789abcdef0123456789abcdef");
if (ret < 0)
    return -1;

/* More code */

=================================================================================
-Overlay path info: dfx_get_overlay_path(const char *overlay_dir,
char *buffer, size_t buf_size)
=================================================================================

/* This API populates the user-provided buffer with the currently
* configured overlay path from configfs.
*
* overlay_dir: Path to the overlay directory in configfs.
* buffer:      User buffer address.
* buf_size:    Size of the user-provided buffer in bytes.
*
* Return: Number of bytes read from configfs in case of success.
*         or Negative value on failure.
*/

Usage example:
#include "libdfx.h"

/* More code */

char buffer[256];
ret = dfx_get_overlay_path("/sys/kernel/config/device-tree/overlays/my_overlay",
                           buffer, sizeof(buffer));
if (ret < 0 || strcmp(buffer, "my_overlay.dtbo") != 0)
    return -1;

/* More code */

=================================================================================
-Overlay status info: dfx_get_overlay_status(const char *overlay_dir,
char *buffer, size_t buf_size)
=================================================================================

/* This API populates the user-provided buffer with the current
* status of the specified overlay as reported by configfs.
*
* overlay_dir: Path to the overlay directory in configfs.
* buffer:      User buffer address.
* buf_size:    Size of the user-provided buffer in bytes.
*
* Return: Number of bytes read from configfs in case of success.
*         or Negative value on failure.
*/

Usage example:
#include "libdfx.h"

/* More code */

char buffer[256];
ret = dfx_get_overlay_status("/sys/kernel/config/device-tree/overlays/my_overlay",
                             buffer, sizeof(buffer));
if (ret < 0)
    return -1;

/* More code */

=================================================================================
-Kernel Firmware Search Path: dfx_set_firmware_search_path(const char *file_path)
=================================================================================

/* This API tells the kernel where to search for the firmware file
* (i.e. bitstream, dtbo, driver)
* see https://docs.kernel.org/driver-api/firmware/fw_search_path.html
* for more information
*
* The parent dir of the provided file is written to
* /sys/module/firmware_class/parameters/path so that the kernel can discover
* the firmware within the parent directory
*
* @param file_path the full path to the file to be loaded
*
* Return:	0 on success or Negative value on failure.
*/

Usage example:

#include "libdfx.h"

/* More code */

ret = dfx_set_firmware_search_path("/home/user/firmwares/my_firmware.bit.bin");
if (ret < 0)
    return -1;

/* More code */

================
Build procedure:
================
	1. Clone libdfx.git repo
	2. mkdir build
	3. cd build
	4. Ensure required tool chain added to your path
	5. cmake -DCMAKE_TOOLCHAIN_FILE="cmake tool chain file(complete path)" ../
			Example: cmake -DCMAKE_TOOLCHAIN_FILE="/libdfx/cmake/toolchain.cmake" ../
	6. make

Once the build is successfully completed the library static, shared object files and app elf file are available in the below paths.
-->build/src/libdfx.a
-->build/src/libdfx.so.1.0
-->build/apps/dfx_app












