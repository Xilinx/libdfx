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

==============================================================
 -pre-fetch:  dfx_cfg_init (const char *dfx_package_path,
			     const char *devpath, u32 flags);
==============================================================

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

================
Build procedure:
================
	1. Clone libdfx.git repo
	2. mkdir build
	3. cd build
	4. Ensure required tool chain added to your path
	5. cmake -DCMAKE_TOOLCHAIN_FILE="cmake tool chain file(complete path)" ..
			Example: cmake -DCMAKE_TOOLCHAIN_FILE="/libdfx/cmake/toolchain.cmake" ..
	6. make












