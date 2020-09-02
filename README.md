 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
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
5. cmake  -DCMAKE_TOOLCHAIN_FILE="cmake tool chain file(complete path)" ..
6. make

The required libdfx.a is available in build folder.

For more information refer doc/README.txt.
