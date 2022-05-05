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

SET(LIBDFX_VERSION 1.0)

file(COPY include/libdfx.h DESTINATION ${CMAKE_BINARY_DIR}/include)

add_library(dfx_shared SHARED ${libdfx_sources})
SET_TARGET_PROPERTIES(dfx_shared PROPERTIES SOVERSION ${LIBDFX_VERSION} OUTPUT_NAME "dfx")
SET_TARGET_PROPERTIES(dfx_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)

add_library(dfx_static STATIC ${libdfx_sources})
SET_TARGET_PROPERTIES(dfx_static PROPERTIES OUTPUT_NAME "dfx")
SET_TARGET_PROPERTIES(dfx_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_include_directories(dfx_shared PUBLIC ${LIBDFX_INCLUDE_DIRS})
target_include_directories(dfx_static PUBLIC ${LIBDFX_INCLUDE_DIRS})
install(TARGETS dfx_shared; dfx_static
	ARCHIVE DESTINATION ${CMAKE_BINARY_DIR}
	LIBRARY DESTINATION ${CMAKE_BINARY_DIR})
