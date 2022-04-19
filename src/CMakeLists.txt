#***************************************************************
#* Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
#* SPDX-License-Identifier: MIT
#***************************************************************

# Specify the minimum version for CMake
cmake_minimum_required(VERSION 2.8.9)	
# Project's name
project(dfx)
enable_language(C ASM)

set(libdfx_sources
        dmabuf_alloc.c
        libdfx.c
)

set(LIBDFX_INCLUDE_DIRS
        "include/"
)

file(COPY include/libdfx.h DESTINATION ${CMAKE_BINARY_DIR}/include)
add_library(dfx ${libdfx_sources})
set_target_properties(dfx PROPERTIES PUBLIC_HEADER "include/libdfx.h")
target_include_directories(dfx PUBLIC ${LIBDFX_INCLUDE_DIRS})
install(TARGETS dfx ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER ${CMAKE_INSTALL_INCLUDEDIR})
