#***************************************************************
#* Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
#* Copyright (C) 2026, Advanced Micro Devices, Inc. All Rights Reserved
#* SPDX-License-Identifier: MIT
#***************************************************************

if(TARGET_PLATFORM STREQUAL "ZYNQ")
    set (CMAKE_SYSTEM_PROCESSOR "arm"                    CACHE STRING "")
    set (CROSS_PREFIX           "arm-linux-gnueabihf-"   CACHE STRING "")
else()
    set (CMAKE_SYSTEM_PROCESSOR "aarch64"                CACHE STRING "")
    set (CROSS_PREFIX           "aarch64-linux-gnu-"     CACHE STRING "")
endif()

set (CMAKE_SYSTEM_NAME  "Linux"              CACHE STRING "")
set (CMAKE_C_COMPILER   "${CROSS_PREFIX}gcc" CACHE STRING "")
set (CMAKE_CXX_COMPILER "${CROSS_PREFIX}g++" CACHE STRING "")
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER CACHE STRING "")
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER CACHE STRING "")
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER CACHE STRING "")
