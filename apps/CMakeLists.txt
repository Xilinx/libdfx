#***************************************************************
#* Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
#* Copyright (C) 2023, Advanced Micro Devices, Inc. All Rights Reserved.
#* SPDX-License-Identifier: MIT
#***************************************************************

# Specify the minimum version for CMake
cmake_minimum_required(VERSION 2.8.9)	
# Project's name
project(dfx_app)

add_executable(dfx_app libdfx_app.c)
target_link_libraries(dfx_app dfx_static)
