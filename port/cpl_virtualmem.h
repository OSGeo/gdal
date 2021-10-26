/**********************************************************************
 * $Id$
 *
 * Name:     cpl_virtualmem.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Virtual memory
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef CPL_VIRTUAL_MEM_INCLUDED
#define CPL_VIRTUAL_MEM_INCLUDED

#include <stddef.h>

#include "cpl_port.h"
#include "cpl_vsi.h"

CPL_C_START

/**
 * \file cpl_virtualmem.h
 *
 * Virtual memory management.
 *
 * This file provides mechanism to define virtual memory mappings, whose content
 * is allocated transparently and filled on-the-fly. Those virtual memory mappings
 * can be much larger than the available RAM, but only parts of the virtual
 * memory mapping, in the limit of the allowed the cache size, will actually be
 * physically allocated.
 *
 * This exploits low-level mechanisms of the operating system (virtual memory
 * allocation, page protection and handler of virtual memory exceptions).
 *
 * It is also possible to create a virtual memory mapping from a file or part
 * of a file.
 *
 * The current implementation is Linux only.
 */

/** Opaque type that represents a virtual memory mapping. */
typedef struct CPLVirtualMem CPLVirtualMem;

/** Callback triggered when a still unmapped page of virtual memory is accessed.
  * The callback has the responsibility of filling the page with relevant values
  *
  * @param ctxt virtual memory handle.
  * @param nOffset offset of the page in the memory mapping.
  * @param pPageToFill address of the page to fill. Note that the address might
  *                    be a temporary location, and not at CPLVirtualMemGetAddr() + nOffset.
  * @param nToFill number of bytes of the page.
  * @param pUserData user data that was passed to CPLVirtualMemNew().
  */
typedef void (*CPLVirtualMemCachePageCbk)(CPLVirtualMem* ctxt,
                                    size_t nOffset,
                                    void* pPageToFill,
                                    size_t nToFill,
                                    void* pUserData);

/** Callback triggered when a dirty mapped page is going to be freed.
  * (saturation of cache, or termination of the virtual memory mapping).
  *
  * @param ctxt virtual memory handle.
  * @param nOffset offset of the page in the memory mapping.
  * @param pPageToBeEvicted address of the page that will be flushed. Note that the address might
  *                    be a temporary location, and not at CPLVirtualMemGetAddr() + nOffset.
  * @param nToBeEvicted number of bytes of the page.
  * @param pUserData user data that was passed to CPLVirtualMemNew().
  */
typedef void (*CPLVirtualMemUnCachePageCbk)(CPLVirtualMem* ctxt,
                                      size_t nOffset,
                                      const void* pPageToBeEvicted,
                                      size_t nToBeEvicted,
                                      void* pUserData);

/** Callback triggered when a virtual memory mapping is destroyed.
  * @param pUserData user data that was passed to CPLVirtualMemNew().
 */
typedef void (*CPLVirtualMemFreeUserData)(void* pUserData);

/** Access mode of a virtual memory mapping. */
typedef enum
{
    /*! The mapping is meant at being read-only, but writes will not be prevented.
        Note that any content written will be lost. */
    VIRTUALMEM_READONLY,
    /*! The mapping is meant at being read-only, and this will be enforced
        through the operating system page protection mechanism. */
    VIRTUALMEM_READONLY_ENFORCED,
    /*! The mapping is meant at being read-write, and modified pages can be saved
        thanks to the pfnUnCachePage callback */
    VIRTUALMEM_READWRITE
} CPLVirtualMemAccessMode;

/** Return the size of a page of virtual memory.
 *
 * @return the page size.
 *
 * @since GDAL 1.11
 */
size_t CPL_DLL CPLGetPageSize(void);

/** Create a new virtual memory mapping.
 *
 * This will reserve an area of virtual memory of size nSize, whose size
 * might be potentially much larger than the physical memory available. Initially,
 * no physical memory will be allocated. As soon as memory pages will be accessed,
 * they will be allocated transparently and filled with the pfnCachePage callback.
 * When the allowed cache size is reached, the least recently used pages will
 * be unallocated.
 *
 * On Linux AMD64 platforms, the maximum value for nSize is 128 TB.
 * On Linux x86 platforms, the maximum value for nSize is 2 GB.
 *
 * Only supported on Linux for now.
 *
 * Note that on Linux, this function will install a SIGSEGV handler. The
 * original handler will be restored by CPLVirtualMemManagerTerminate().
 *
 * @param nSize size in bytes of the virtual memory mapping.
 * @param nCacheSize   size in bytes of the maximum memory that will be really
 *                     allocated (must ideally fit into RAM).
 * @param nPageSizeHint hint for the page size. Must be a multiple of the
 *                      system page size, returned by CPLGetPageSize().
 *                      Minimum value is generally 4096. Might be set to 0 to
 *                      let the function determine a default page size.
 * @param bSingleThreadUsage set to TRUE if there will be no concurrent threads
 *                           that will access the virtual memory mapping. This can
 *                           optimize performance a bit.
 * @param eAccessMode permission to use for the virtual memory mapping.
 * @param pfnCachePage callback triggered when a still unmapped page of virtual
 *                     memory is accessed. The callback has the responsibility
 *                     of filling the page with relevant values.
 * @param pfnUnCachePage callback triggered when a dirty mapped page is going to
 *                       be freed (saturation of cache, or termination of the
 *                       virtual memory mapping). Might be NULL.
 * @param pfnFreeUserData callback that can be used to free pCbkUserData. Might be
 *                        NULL
 * @param pCbkUserData user data passed to pfnCachePage and pfnUnCachePage.
 *
 * @return a virtual memory object that must be freed by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 1.11
 */

CPLVirtualMem CPL_DLL *CPLVirtualMemNew(size_t nSize,
                                        size_t nCacheSize,
                                        size_t nPageSizeHint,
                                        int bSingleThreadUsage,
                                        CPLVirtualMemAccessMode eAccessMode,
                                        CPLVirtualMemCachePageCbk pfnCachePage,
                                        CPLVirtualMemUnCachePageCbk pfnUnCachePage,
                                        CPLVirtualMemFreeUserData pfnFreeUserData,
                                        void *pCbkUserData);

/** Return if virtual memory mapping of a file is available.
 *
 * @return TRUE if virtual memory mapping of a file is available.
 * @since GDAL 1.11
 */
int CPL_DLL CPLIsVirtualMemFileMapAvailable(void);

/** Create a new virtual memory mapping from a file.
 *
 * The file must be a "real" file recognized by the operating system, and not
 * a VSI extended virtual file.
 *
 * In VIRTUALMEM_READWRITE mode, updates to the memory mapping will be written
 * in the file.
 *
 * On Linux AMD64 platforms, the maximum value for nLength is 128 TB.
 * On Linux x86 platforms, the maximum value for nLength is 2 GB.
 *
 * Supported on Linux only in GDAL <= 2.0, and all POSIX systems supporting
 * mmap() in GDAL >= 2.1
 *
 * @param  fp       Virtual file handle.
 * @param  nOffset  Offset in the file to start the mapping from.
 * @param  nLength  Length of the portion of the file to map into memory.
 * @param eAccessMode Permission to use for the virtual memory mapping. This must
 *                    be consistent with how the file has been opened.
 * @param pfnFreeUserData callback that is called when the object is destroyed.
 * @param pCbkUserData user data passed to pfnFreeUserData.
 * @return a virtual memory object that must be freed by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 1.11
 */
CPLVirtualMem CPL_DLL *CPLVirtualMemFileMapNew( VSILFILE* fp,
                                                vsi_l_offset nOffset,
                                                vsi_l_offset nLength,
                                                CPLVirtualMemAccessMode eAccessMode,
                                                CPLVirtualMemFreeUserData pfnFreeUserData,
                                                void *pCbkUserData );

/** Create a new virtual memory mapping derived from an other virtual memory
 *  mapping.
 *
 * This may be useful in case of creating mapping for pixel interleaved data.
 *
 * The new mapping takes a reference on the base mapping.
 *
 * @param pVMemBase Base virtual memory mapping
 * @param nOffset   Offset in the base virtual memory mapping from which to start
 *                  the new mapping.
 * @param nSize     Size of the base virtual memory mapping to expose in the
 *                  the new mapping.
 * @param pfnFreeUserData callback that is called when the object is destroyed.
 * @param pCbkUserData user data passed to pfnFreeUserData.
 * @return a virtual memory object that must be freed by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 1.11
 */
CPLVirtualMem CPL_DLL *CPLVirtualMemDerivedNew(CPLVirtualMem* pVMemBase,
                                               vsi_l_offset nOffset,
                                               vsi_l_offset nSize,
                                               CPLVirtualMemFreeUserData pfnFreeUserData,
                                               void *pCbkUserData);

/** Free a virtual memory mapping.
 *
 * The pointer returned by CPLVirtualMemGetAddr() will no longer be valid.
 * If the virtual memory mapping was created with read/write permissions and that
 * they are dirty (i.e. modified) pages, they will be flushed through the
 * pfnUnCachePage callback before being freed.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 *
 * @since GDAL 1.11
 */
void CPL_DLL CPLVirtualMemFree(CPLVirtualMem* ctxt);

/** Return the pointer to the start of a virtual memory mapping.
 *
 * The bytes in the range [p:p+CPLVirtualMemGetSize()-1] where p is the pointer
 * returned by this function will be valid, until CPLVirtualMemFree() is called.
 *
 * Note that if a range of bytes used as an argument of a system call
 * (such as read() or write()) contains pages that have not been "realized", the
 * system call will fail with EFAULT. CPLVirtualMemPin() can be used to work
 * around this issue.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 * @return the pointer to the start of a virtual memory mapping.
 *
 * @since GDAL 1.11
 */
void CPL_DLL *CPLVirtualMemGetAddr(CPLVirtualMem* ctxt);

/** Return the size of the virtual memory mapping.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 * @return the size of the virtual memory mapping.
 *
 * @since GDAL 1.11
 */
size_t CPL_DLL CPLVirtualMemGetSize(CPLVirtualMem* ctxt);

/** Return if the virtual memory mapping is a direct file mapping.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 * @return TRUE if the virtual memory mapping is a direct file mapping.
 *
 * @since GDAL 1.11
 */
int CPL_DLL CPLVirtualMemIsFileMapping(CPLVirtualMem* ctxt);

/** Return the access mode of the virtual memory mapping.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 * @return the access mode of the virtual memory mapping.
 *
 * @since GDAL 1.11
 */
CPLVirtualMemAccessMode CPL_DLL CPLVirtualMemGetAccessMode(CPLVirtualMem* ctxt);

/** Return the page size associated to a virtual memory mapping.
 *
 * The value returned will be at least CPLGetPageSize(), but potentially
 * larger.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 * @return the page size
 *
 * @since GDAL 1.11
 */
size_t CPL_DLL CPLVirtualMemGetPageSize(CPLVirtualMem* ctxt);

/** Return TRUE if this memory mapping can be accessed safely from concurrent
 *  threads.
 *
 * The situation that can cause problems is when several threads try to access
 * a page of the mapping that is not yet mapped.
 *
 * The return value of this function depends on whether bSingleThreadUsage has
 * been set of not in CPLVirtualMemNew() and/or the implementation.
 *
 * On Linux, this will always return TRUE if bSingleThreadUsage = FALSE.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 * @return TRUE if this memory mapping can be accessed safely from concurrent
 *         threads.
 *
 * @since GDAL 1.11
 */
int CPL_DLL CPLVirtualMemIsAccessThreadSafe(CPLVirtualMem* ctxt);

/** Declare that a thread will access a virtual memory mapping.
 *
 * This function must be called by a thread that wants to access the
 * content of a virtual memory mapping, except if the virtual memory mapping has
 * been created with bSingleThreadUsage = TRUE.
 *
 * This function must be paired with CPLVirtualMemUnDeclareThread().
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 *
 * @since GDAL 1.11
 */
void CPL_DLL CPLVirtualMemDeclareThread(CPLVirtualMem* ctxt);

/** Declare that a thread will stop accessing a virtual memory mapping.
 *
 * This function must be called by a thread that will no longer access the
 * content of a virtual memory mapping, except if the virtual memory mapping has
 * been created with bSingleThreadUsage = TRUE.
 *
 * This function must be paired with CPLVirtualMemDeclareThread().
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 *
 * @since GDAL 1.11
 */
void CPL_DLL CPLVirtualMemUnDeclareThread(CPLVirtualMem* ctxt);

/** Make sure that a region of virtual memory will be realized.
 *
 * Calling this function is not required, but might be useful when debugging
 * a process with tools like gdb or valgrind that do not naturally like
 * segmentation fault signals.
 *
 * It is also needed when wanting to provide part of virtual memory mapping
 * to a system call such as read() or write(). If read() or write() is called
 * on a memory region not yet realized, the call will fail with EFAULT.
 *
 * @param ctxt context returned by CPLVirtualMemNew().
 * @param pAddr the memory region to pin.
 * @param nSize the size of the memory region.
 * @param bWriteOp set to TRUE if the memory are will be accessed in write mode.
 *
 * @since GDAL 1.11
 */
void CPL_DLL CPLVirtualMemPin(CPLVirtualMem* ctxt,
                              void* pAddr, size_t nSize, int bWriteOp);

/** Cleanup any resource and handlers related to virtual memory.
 *
 * This function must be called after the last CPLVirtualMem object has
 * been freed.
 *
 * @since GDAL 2.0
 */
void CPL_DLL CPLVirtualMemManagerTerminate(void);

CPL_C_END

#endif /* CPL_VIRTUAL_MEM_INCLUDED */
