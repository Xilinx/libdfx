 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * Copyright (C) 2023, Advanced Micro Devices, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT

Introduction
============

libdfx.git - The library is a lightweight user-space library that provides
APIs for application to configure the PL.

The libdfx software is divided into following directories:

	- apps
		illustrating different use cases of DFX Configuration.
	- cmake
		Contains information about the toolchain.
	- doc
		Contains information about the libdfx repo.
	- src
		driver interface code implementing functionality

Building libdfx from git:
==============================
1. Clone libdfx.git repo
2. mkdir build
3. cd build
4. Ensure required tool chain added to your path
5. cmake  -DCMAKE_TOOLCHAIN_FILE="cmake tool chain file(complete path)" ../
   Example: cmake -DCMAKE_TOOLCHAIN_FILE="/libdfx/cmake/toolchain.cmake" ../
6. make

Once the build is successfully completed the library static, shared object files and app elf file are available in the below paths.

-build/src/libdfx.a

-build/src/libdfx.so.1.0

-build/apps/dfx_app

For more information refer doc/README.txt.
