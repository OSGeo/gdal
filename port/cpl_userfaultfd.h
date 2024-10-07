/******************************************************************************
 *
 * Name:     cpl_userfault.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Use userfaultfd and VSIL to service page faults
 * Author:   James McClain, <james.mcclain@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Dr. James McClain <james.mcclain@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_USERFAULTFD
#define CPL_USERFAULTFD

#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/userfaultfd.h>

#define GDAL_UFFD_LIMIT "GDAL_UFFD_LIMIT"

typedef struct cpl_uffd_context cpl_uffd_context;

bool CPL_DLL CPLIsUserFaultMappingSupported();
cpl_uffd_context CPL_DLL *CPLCreateUserFaultMapping(const char *pszFilename,
                                                    void **ppVma,
                                                    uint64_t *pnVmaSize);
void CPL_DLL CPLDeleteUserFaultMapping(cpl_uffd_context *ctx);

#endif
