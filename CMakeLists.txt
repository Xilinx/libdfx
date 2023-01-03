#***************************************************************
#* Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
#* SPDX-License-Identifier: MIT
#***************************************************************

# Specify the minimum version for CMake
cmake_minimum_required(VERSION 2.8.9)

project(libdfx C)

OPTION(ENABLE_LIBDFX_TIME, OFF)
IF(ENABLE_LIBDFX_TIME)
add_compile_definitions(ENABLE_LIBDFX_TIME)
endif(ENABLE_LIBDFX_TIME)
#link_directories(${CMAKE_BINARY_DIR}/lib)
	
# Project's name
add_subdirectory(src)
add_subdirectory(apps)
