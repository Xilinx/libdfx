#******************************************************************************
#* Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
#* Copyright (C) 2023 - 2025, Advanced Micro Devices, Inc. All Rights Reserved
#* SPDX-License-Identifier: MIT
#******************************************************************************

include(GNUInstallDirs)

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

# ---- Library versioning ----
set(LIBDFX_VERSION_MAJOR 1)
set(LIBDFX_VERSION_MINOR 0)
set(LIBDFX_VERSION_PATCH 1)
set(LIBDFX_VERSION ${LIBDFX_VERSION_MAJOR}.${LIBDFX_VERSION_MINOR}.${LIBDFX_VERSION_PATCH})

# Copy header to build directory for IDE support
file(COPY include/libdfx.h DESTINATION ${CMAKE_BINARY_DIR}/include)

# ---- Shared library (with SONAME) ----
add_library(dfx_shared SHARED ${libdfx_sources})
set_target_properties(dfx_shared PROPERTIES
    VERSION ${LIBDFX_VERSION}
    SOVERSION ${LIBDFX_VERSION_MAJOR}
    OUTPUT_NAME "dfx"
    CLEAN_DIRECT_OUTPUT 1
)

# ---- Static library ----
add_library(dfx_static STATIC ${libdfx_sources})
set_target_properties(dfx_static PROPERTIES
    OUTPUT_NAME "dfx"
    CLEAN_DIRECT_OUTPUT 1
)

# ---- Include directories ----
target_include_directories(dfx_shared PUBLIC ${LIBDFX_INCLUDE_DIRS})
target_include_directories(dfx_static PUBLIC ${LIBDFX_INCLUDE_DIRS})

# ---- Install rules ----
install(FILES "include/libdfx.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS dfx_shared dfx_static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
