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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
cpl_uffd_context CPL_DLL * CPLCreateUserFaultMapping(const char * pszFilename, void ** ppVma, uint64_t * pnVmaSize);
void CPL_DLL CPLDeleteUserFaultMapping(cpl_uffd_context * ctx);

#endif
