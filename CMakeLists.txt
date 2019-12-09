#***************************************************************
#* Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
#* SPDX-License-Identifier: MIT
#***************************************************************

# Specify the minimum version for CMake
cmake_minimum_required(VERSION 2.8.9)

project(libdfx C)

#link_directories(${CMAKE_BINARY_DIR}/lib)
	
# Project's name
add_subdirectory(src)
add_subdirectory(apps)
