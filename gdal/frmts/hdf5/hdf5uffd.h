/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Header file for userfaultfd support.
 * Author:   James McClain <james.mcclain@gmail.com>
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

#ifdef ENABLE_UFFD

#include "cpl_userfaultfd.h"

#ifndef HDF5_UFFD_MAP

#define HDF5_UFFD_MAP(filename, handle, context) { \
    void * pVma = nullptr; \
    uint64_t nVmaSize = 0; \
    context = nullptr; \
    if ( !strncmp(filename, "/vsi", strlen("/vsi")) && CPLIsUserFaultMappingSupported() ) \
      context = CPLCreateUserFaultMapping(filename, &pVma, &nVmaSize); \
    if (context != nullptr && pVma != nullptr && nVmaSize > 0) \
      handle = H5LTopen_file_image(pVma, nVmaSize, H5LT_FILE_IMAGE_DONT_COPY|H5LT_FILE_IMAGE_DONT_RELEASE); \
    else \
      handle = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT); \
}

#endif // HDF5_UUFD_MAP

#ifndef HDF5_UFFD_UNMAP

#define HDF5_UFFD_UNMAP(context) { \
  CPLDeleteUserFaultMapping(context); \
  context = nullptr; \
}

#endif // HDF5_UFFD_UNMAP


#endif // ENABLE_UFFD
