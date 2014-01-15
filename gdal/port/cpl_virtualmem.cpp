/**********************************************************************
 * $Id$
 *
 * Name:     cpl_virtualmem.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Virtual memory
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2013, Even Rouault, <even dot rouault at mines dash paris dot org>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cpl_virtualmem.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_atomic_ops.h"
#include "cpl_conv.h"

#if defined(__linux) && defined(CPL_MULTIPROC_PTHREAD)

#include <sys/mman.h>   /* mmap, munmap, mremap */
#include <sys/select.h> /* select */
#include <sys/stat.h>   /* open() */
#include <sys/types.h>  /* open() */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>      /* open() */
#include <signal.h>     /* sigaction */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     /* read, write, close, pipe */
#include <pthread.h>

#define HAVE_5ARGS_MREMAP

#ifndef HAVE_5ARGS_MREMAP
#include "cpl_atomic_ops.h"
#endif

/* Linux specific (i.e. non POSIX compliant) features used :
   - returning from a SIGSEGV handler is clearly a POSIX violation, but in
     practice most POSIX systems should be happy.
   - mremap() with 5 args is Linux specific. It is used when the user callback is invited
     to fill a page, we currently mmap() a writable page, let it filled it,
     and afterwards mremap() that temporary page onto the location where the
     fault occured.
     If we have no mremap(), the workaround is to pause other threads that
     consume the current view while we are updating the faulted page, otherwise
     a non-paused thread could access a page that is in the middle of being
     filled... The way we pause those threads is quite original : we send them
     a SIGUSR1 and wait that they are stuck in the temporary SIGUSR1 handler...
   - MAP_ANONYMOUS isn't documented in Posix, but very commonly found (sometimes called MAP_ANON)
   - dealing with the limitation of number of memory mapping regions, and the 65536 limit.
   - other things I've not identified
*/


#define ALIGN_DOWN(p,pagesize)  (void*)(((size_t)(p)) / (pagesize) * (pagesize))
#define ALIGN_UP(p,pagesize)    (void*)(((size_t)(p) + (pagesize) - 1) / (pagesize) * (pagesize))

#define DEFAULT_PAGE_SIZE       (256*256)
#define MAXIMUM_PAGE_SIZE       (32*1024*1024)

/* Linux Kernel limit */
#define MAXIMUM_COUNT_OF_MAPPINGS   65536

#define BYEBYE_ADDR             ((void*)(~(size_t)0))

#define MAPPING_FOUND           "yeah"
#define MAPPING_NOT_FOUND       "doh!"

#define SET_BIT(ar,bitnumber)       ar[(bitnumber)/8] |= 1 << ((bitnumber) % 8)
#define UNSET_BIT(ar,bitnumber)     ar[(bitnumber)/8] &= ~(1 << ((bitnumber) % 8))
#define TEST_BIT(ar,bitnumber)      (ar[(bitnumber)/8] & (1 << ((bitnumber) % 8)))

typedef enum
{
    OP_LOAD,
    OP_STORE,
    OP_MOVS_RSI_RDI,
    OP_UNKNOWN
} OpType;

struct CPLVirtualMem
{
    struct CPLVirtualMem *pVMemBase;
    int          nRefCount;

    int          bFileMemoryMapped; /* if TRUE, only eAccessMode, pData, pDataToFree, nSize and nPageSize are valid */
    CPLVirtualMemAccessMode eAccessMode;

    size_t       nPageSize;
    void        *pData;                  /* aligned on nPageSize */
    void        *pDataToFree;            /* returned by mmap(), potentially lower than pData */
    size_t       nSize;                  /* requested size (unrounded) */

    GByte       *pabitMappedPages;
    GByte       *pabitRWMappedPages;

    int          nCacheMaxSizeInPages;   /* maximum size of page array */
    int         *panLRUPageIndices;      /* array with indices of cached pages */
    int          iLRUStart;              /* index in array where to write next page index */
    int          nLRUSize;               /* current isze of the array */

    int          iLastPage;              /* last page accessed */
    int          nRetry;                 /* number of consecutive retries to that last page */
    
    int          bSingleThreadUsage;

    CPLVirtualMemCachePageCbk     pfnCachePage;       /* called when a page is mapped */
    CPLVirtualMemUnCachePageCbk   pfnUnCachePage;     /* called when a (writable) page is unmapped */
    void                   *pCbkUserData;
    CPLVirtualMemFreeUserData     pfnFreeUserData;
#ifndef HAVE_5ARGS_MREMAP
    void                   *hMutexThreadArray;
    int                     nThreads;
    pthread_t              *pahThreads;
#endif
};

typedef struct
{
    /* hVirtualMemManagerMutex protects the 2 following variables */
    CPLVirtualMem        **pasVirtualMem;
    int              nVirtualMemCount;

    int              pipefd_to_thread[2];
    int              pipefd_from_thread[2];
    int              pipefd_wait_thread[2];
    void            *hHelperThread;

    struct sigaction oldact;
} CPLVirtualMemManager;

typedef struct
{
    void            *pFaultAddr;
    OpType           opType;
    pthread_t        hRequesterThread;
} CPLVirtualMemMsgToWorkerThread;

static CPLVirtualMemManager* pVirtualMemManager = NULL;
static void* hVirtualMemManagerMutex = NULL;

static void CPLVirtualMemManagerInit();

#ifdef DEBUG_VIRTUALMEM

/************************************************************************/
/*                           fprintfstderr()                            */
/************************************************************************/

static void fprintfstderr(const char* fmt, ...)
{
    char buffer[80];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    int offset = 0;
    while(TRUE)
    {
        int ret = write(2, buffer + offset, strlen(buffer + offset));
        if( ret < 0 && errno == EINTR )
            ;
        else
        {
            if( ret == (int)strlen(buffer + offset) )
                break;
            offset += ret;
        }
    }
}

#endif

/************************************************************************/
/*                         CPLGetPageSize()                             */
/************************************************************************/

size_t CPLGetPageSize(void)
{
    return (size_t) sysconf(_SC_PAGESIZE);
}

/************************************************************************/
/*              CPLVirtualMemManagerRegisterVirtualMem()                */
/************************************************************************/

static void CPLVirtualMemManagerRegisterVirtualMem(CPLVirtualMem* ctxt)
{
    CPLVirtualMemManagerInit();

    assert(ctxt);
    CPLAcquireMutex(hVirtualMemManagerMutex, 1000.0);
    pVirtualMemManager->pasVirtualMem = (CPLVirtualMem**) CPLRealloc(
        pVirtualMemManager->pasVirtualMem, sizeof(CPLVirtualMem*) * (pVirtualMemManager->nVirtualMemCount + 1) );
    pVirtualMemManager->pasVirtualMem[pVirtualMemManager->nVirtualMemCount] = ctxt;
    pVirtualMemManager->nVirtualMemCount ++;
    CPLReleaseMutex(hVirtualMemManagerMutex);
}

/************************************************************************/
/*               CPLVirtualMemManagerUnregisterVirtualMem()             */
/************************************************************************/

static void CPLVirtualMemManagerUnregisterVirtualMem(CPLVirtualMem* ctxt)
{
    int i;
    CPLAcquireMutex(hVirtualMemManagerMutex, 1000.0);
    for(i=0;i<pVirtualMemManager->nVirtualMemCount;i++)
    {
        if( pVirtualMemManager->pasVirtualMem[i] == ctxt )
        {
            if( i < pVirtualMemManager->nVirtualMemCount - 1 )
            {
                memmove( pVirtualMemManager->pasVirtualMem + i,
                         pVirtualMemManager->pasVirtualMem + i + 1,
                    sizeof(CPLVirtualMem*) * (pVirtualMemManager->nVirtualMemCount - i - 1) );
            }
            pVirtualMemManager->nVirtualMemCount --;
            break;
        }
    }
    CPLReleaseMutex(hVirtualMemManagerMutex);
}

/************************************************************************/
/*                           CPLVirtualMemNew()                         */
/************************************************************************/

CPLVirtualMem* CPLVirtualMemNew(size_t nSize,
                                size_t nCacheSize,
                                size_t nPageSizeHint,
                                int bSingleThreadUsage,
                                CPLVirtualMemAccessMode eAccessMode,
                                CPLVirtualMemCachePageCbk pfnCachePage,
                                CPLVirtualMemUnCachePageCbk pfnUnCachePage,
                                CPLVirtualMemFreeUserData pfnFreeUserData,
                                void *pCbkUserData)
{
    CPLVirtualMem* ctxt;
    void* pData;
    size_t nMinPageSize = CPLGetPageSize();
    size_t nPageSize = DEFAULT_PAGE_SIZE;
    size_t nCacheMaxSizeInPages;
    size_t nRoundedMappingSize;
    FILE* f;
    int nMappings = 0;

    assert(nSize > 0);
    assert(pfnCachePage != NULL);

    if( nPageSizeHint >= nMinPageSize && nPageSizeHint <= MAXIMUM_PAGE_SIZE )
    {
        if( (nPageSizeHint % nMinPageSize) == 0 )
            nPageSize = nPageSizeHint;
        else
        {
            int nbits = 0;
            nPageSize = (size_t)nPageSizeHint;
            while(nPageSize > 0)
            {
                nPageSize >>= 1;
                nbits ++;
            }
            nPageSize = (size_t)1 << (nbits - 1);
            if( nPageSize < (size_t)nPageSizeHint )
                nPageSize <<= 1;
        }
    }

    if( (nPageSize % nMinPageSize) != 0 )
        nPageSize = nMinPageSize;

    if( nCacheSize > nSize )
        nCacheSize = nSize;
    else if( nCacheSize == 0 )
        nCacheSize = 1;

    /* Linux specific */
    /* Count the number of existing memory mappings */
    f = fopen("/proc/self/maps", "rb");
    if( f != NULL )
    {
        char buffer[80];
        while( fgets(buffer, sizeof(buffer), f) != NULL )
            nMappings ++;
        fclose(f);
    }

    while(TRUE)
    {
        /* /proc/self/maps must not have more than 65K lines */
        nCacheMaxSizeInPages = (nCacheSize + 2 * nPageSize - 1) / nPageSize;
        if( nCacheMaxSizeInPages >
                    (size_t)((MAXIMUM_COUNT_OF_MAPPINGS * 9 / 10) - nMappings) )
            nPageSize <<= 1;
        else
            break;
    }
    nRoundedMappingSize = ((nSize + 2 * nPageSize - 1) / nPageSize) * nPageSize;
    pData = mmap(NULL, nRoundedMappingSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if( pData == MAP_FAILED )
    {
        perror("mmap");
        return NULL;
    }
    ctxt = (CPLVirtualMem* )CPLCalloc(1, sizeof(CPLVirtualMem));
    ctxt->nRefCount = 1;
    ctxt->bFileMemoryMapped = FALSE;
    ctxt->eAccessMode = eAccessMode;
    ctxt->pDataToFree = pData;
    ctxt->pData = ALIGN_UP(pData, nPageSize);
    ctxt->nPageSize = nPageSize;
    ctxt->pabitMappedPages = (GByte*)CPLCalloc(1, (nRoundedMappingSize / nPageSize + 7) / 8);
    assert(ctxt->pabitMappedPages);
    ctxt->pabitRWMappedPages = (GByte*)CPLCalloc(1, (nRoundedMappingSize / nPageSize + 7) / 8);
    assert(ctxt->pabitRWMappedPages);
    /* we need at least 2 pages in case for a rep movs instruction */
    /* that operate in the view */
    ctxt->nCacheMaxSizeInPages = nCacheMaxSizeInPages;
    ctxt->panLRUPageIndices = (int*)CPLMalloc(ctxt->nCacheMaxSizeInPages * sizeof(int));
    assert(ctxt->panLRUPageIndices);
    ctxt->nSize = nSize;
    ctxt->iLRUStart = 0;
    ctxt->nLRUSize = 0;
    ctxt->iLastPage = -1;
    ctxt->nRetry = 0;
    ctxt->bSingleThreadUsage = bSingleThreadUsage;
    ctxt->pfnCachePage = pfnCachePage;
    ctxt->pfnUnCachePage = pfnUnCachePage;
    ctxt->pfnFreeUserData = pfnFreeUserData;
    ctxt->pCbkUserData = pCbkUserData;
#ifndef HAVE_5ARGS_MREMAP
    if( !ctxt->bSingleThreadUsage )
    {
        ctxt->hMutexThreadArray = CPLCreateMutex();
        assert(ctxt->hMutexThreadArray != NULL);
        CPLReleaseMutex(ctxt->hMutexThreadArray);
        ctxt->nThreads = 0;
        ctxt->pahThreads = NULL;
    }
#endif

    CPLVirtualMemManagerRegisterVirtualMem(ctxt);

    return ctxt;
}

/************************************************************************/
/*                   CPLIsVirtualMemFileMapAvailable()                  */
/************************************************************************/

int CPLIsVirtualMemFileMapAvailable(void)
{
    return TRUE;
}

/************************************************************************/
/*                       CPLVirtualMemFileMapNew()                      */
/************************************************************************/

CPLVirtualMem *CPLVirtualMemFileMapNew( VSILFILE* fp,
                                        vsi_l_offset nOffset,
                                        vsi_l_offset nLength,
                                        CPLVirtualMemAccessMode eAccessMode,
                                        CPLVirtualMemFreeUserData pfnFreeUserData,
                                        void *pCbkUserData )
{
#if SIZEOF_VOIDP == 4
    if( nLength != (size_t)nLength )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "nLength = " CPL_FRMT_GUIB " incompatible with 32 bit architecture",
                 nLength);
        return NULL;
    }
#endif

    int fd = (int) (size_t) VSIFGetNativeFileDescriptorL(fp);
    if( fd == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot operate on a virtual file");
        return NULL;
    }

    off_t nAlignedOffset = (nOffset / CPLGetPageSize()) * CPLGetPageSize();
    size_t nAligment = nOffset - nAlignedOffset;
    size_t nMappingSize = nLength + nAligment;

    /* We need to ensure that the requested extent fits into the file size */
    /* otherwise SIGBUS errors will occur when using the mapping */
    vsi_l_offset nCurPos = VSIFTellL(fp);
    VSIFSeekL(fp, 0, SEEK_END);
    vsi_l_offset nFileSize = VSIFTellL(fp);
    if( nFileSize < nOffset + nLength )
    {
        if( eAccessMode != VIRTUALMEM_READWRITE )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Trying to map an extent outside of the file");
            VSIFSeekL(fp, nCurPos, SEEK_SET);
            return NULL;
        }
        else
        {
            char ch = 0;
            if( VSIFSeekL(fp, nOffset + nLength - 1, SEEK_SET) != 0 ||
                VSIFWriteL(&ch, 1, 1, fp) != 1 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot extend file to mapping size");
                VSIFSeekL(fp, nCurPos, SEEK_SET);
                return NULL;
            }
        }
    }
    VSIFSeekL(fp, nCurPos, SEEK_SET);

    void* addr = mmap(NULL, nMappingSize,
                      (eAccessMode == VIRTUALMEM_READWRITE) ? PROT_READ | PROT_WRITE : PROT_READ,
                      MAP_SHARED, fd, nAlignedOffset);
    if( addr == MAP_FAILED )
    {
        int myerrno = errno;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "mmap() failed : %s", strerror(myerrno));
        return NULL;
    }

    CPLVirtualMem* ctxt = (CPLVirtualMem* )CPLCalloc(1, sizeof(CPLVirtualMem));
    ctxt->nRefCount = 1;
    ctxt->eAccessMode = eAccessMode;
    ctxt->bFileMemoryMapped = TRUE;
    ctxt->pData = (GByte*) addr + nAligment;
    ctxt->pDataToFree = addr;
    ctxt->nSize = nLength;
    ctxt->nPageSize = CPLGetPageSize();
    ctxt->bSingleThreadUsage = FALSE;
    ctxt->pfnFreeUserData = pfnFreeUserData;
    ctxt->pCbkUserData = pCbkUserData;

    return ctxt;
}

/************************************************************************/
/*                       CPLVirtualMemDerivedNew()                      */
/************************************************************************/

CPLVirtualMem *CPLVirtualMemDerivedNew(CPLVirtualMem* pVMemBase,
                                       vsi_l_offset nOffset,
                                       vsi_l_offset nSize,
                                       CPLVirtualMemFreeUserData pfnFreeUserData,
                                       void *pCbkUserData)
{
    if( nOffset + nSize > pVMemBase->nSize )
        return NULL;

    CPLVirtualMem* ctxt = (CPLVirtualMem* )CPLCalloc(1, sizeof(CPLVirtualMem));
    ctxt->nRefCount = 1;
    ctxt->pVMemBase = pVMemBase;
    pVMemBase->nRefCount ++;
    ctxt->eAccessMode = pVMemBase->eAccessMode;
    ctxt->bFileMemoryMapped = pVMemBase->bFileMemoryMapped;
    ctxt->pData = (GByte*) pVMemBase->pData + nOffset;
    ctxt->pDataToFree = NULL;
    ctxt->nSize = nSize;
    ctxt->nPageSize = pVMemBase->nPageSize;
    ctxt->bSingleThreadUsage = pVMemBase->bSingleThreadUsage;
    ctxt->pfnFreeUserData = pfnFreeUserData;
    ctxt->pCbkUserData = pCbkUserData;

    return ctxt;
}

/************************************************************************/
/*                        CPLVirtualMemFree()                           */
/************************************************************************/

void CPLVirtualMemFree(CPLVirtualMem* ctxt)
{
    size_t nRoundedMappingSize;

    if( ctxt == NULL || --(ctxt->nRefCount) > 0 )
        return;

    if( ctxt->pVMemBase != NULL )
    {
        CPLVirtualMemFree(ctxt->pVMemBase);
        if( ctxt->pfnFreeUserData != NULL )
            ctxt->pfnFreeUserData(ctxt->pCbkUserData);
        CPLFree(ctxt);
        return;
    }

    if( ctxt->bFileMemoryMapped )
    {
        size_t nMappingSize = ctxt->nSize + (GByte*)ctxt->pData - (GByte*)ctxt->pDataToFree;
        assert(munmap(ctxt->pDataToFree, nMappingSize) == 0);
        if( ctxt->pfnFreeUserData != NULL )
            ctxt->pfnFreeUserData(ctxt->pCbkUserData);
        CPLFree(ctxt);
        return;
    }

    CPLVirtualMemManagerUnregisterVirtualMem(ctxt);

    nRoundedMappingSize = ((ctxt->nSize + 2 * ctxt->nPageSize - 1) /
                                            ctxt->nPageSize) * ctxt->nPageSize;
    if( ctxt->eAccessMode == VIRTUALMEM_READWRITE &&
        ctxt->pfnUnCachePage != NULL )
    {
        size_t i;
        for(i = 0; i < nRoundedMappingSize / ctxt->nPageSize; i++)
        {
            if( TEST_BIT(ctxt->pabitRWMappedPages, i) )
            {
                void* addr = (char*)ctxt->pData + i * ctxt->nPageSize;
                ctxt->pfnUnCachePage(ctxt,
                                 i * ctxt->nPageSize,
                                 addr,
                                 ctxt->nPageSize,
                                 ctxt->pCbkUserData);
            }
        }
    }
    assert(munmap(ctxt->pDataToFree, nRoundedMappingSize) == 0);
    CPLFree(ctxt->pabitMappedPages);
    CPLFree(ctxt->pabitRWMappedPages);
    CPLFree(ctxt->panLRUPageIndices);
#ifndef HAVE_5ARGS_MREMAP
    if( !ctxt->bSingleThreadUsage )
    {
        CPLFree(ctxt->pahThreads);
        CPLDestroyMutex(ctxt->hMutexThreadArray);
    }
#endif
    if( ctxt->pfnFreeUserData != NULL )
        ctxt->pfnFreeUserData(ctxt->pCbkUserData);

    CPLFree(ctxt);
}

/************************************************************************/
/*                      CPLVirtualMemGetAddr()                          */
/************************************************************************/

void* CPLVirtualMemGetAddr(CPLVirtualMem* ctxt)
{
    return ctxt->pData;
}

/************************************************************************/
/*                     CPLVirtualMemIsFileMapping()                     */
/************************************************************************/

int CPLVirtualMemIsFileMapping(CPLVirtualMem* ctxt)
{
    return ctxt->bFileMemoryMapped;
}

/************************************************************************/
/*                     CPLVirtualMemGetAccessMode()                     */
/************************************************************************/

CPLVirtualMemAccessMode CPLVirtualMemGetAccessMode(CPLVirtualMem* ctxt)
{
    return ctxt->eAccessMode;
}

/************************************************************************/
/*                      CPLVirtualMemGetPageSize()                      */
/************************************************************************/

size_t CPLVirtualMemGetPageSize(CPLVirtualMem* ctxt)
{
    return ctxt->nPageSize;
}

/************************************************************************/
/*                        CPLVirtualMemGetSize()                        */
/************************************************************************/

size_t CPLVirtualMemGetSize(CPLVirtualMem* ctxt)
{
    return ctxt->nSize;
}

#ifndef HAVE_5ARGS_MREMAP

static volatile int nCountThreadsInSigUSR1 = 0;
static volatile int nWaitHelperThread = 0;

/************************************************************************/
/*                   CPLVirtualMemSIGUSR1Handler()                      */
/************************************************************************/

static void CPLVirtualMemSIGUSR1Handler(int signum_unused, 
                                     siginfo_t* the_info_unused,
                                     void* the_ctxt_unused)
{
    /* fprintfstderr("entering CPLVirtualMemSIGUSR1Handler %X\n", pthread_self()); */
    (void)signum_unused;
    (void)the_info_unused;
    (void)the_ctxt_unused;
    /* I guess this is only POSIX correct if it is implemented by an intrinsic */
    CPLAtomicInc(&nCountThreadsInSigUSR1);
    while( nWaitHelperThread )
        usleep(1); /* not explicitely indicated as signal-async-safe, but hopefully ok */
    CPLAtomicDec(&nCountThreadsInSigUSR1);
    /* fprintfstderr("leaving CPLVirtualMemSIGUSR1Handler %X\n", pthread_self()); */
}
#endif

/************************************************************************/
/*                   CPLVirtualMemIsAccessThreadSafe()                  */
/************************************************************************/

int CPLVirtualMemIsAccessThreadSafe(CPLVirtualMem* ctxt)
{
    return !ctxt->bSingleThreadUsage;
}

/************************************************************************/
/*                      CPLVirtualMemDeclareThread()                    */
/************************************************************************/

void CPLVirtualMemDeclareThread(CPLVirtualMem* ctxt)
{
    if( ctxt->bFileMemoryMapped )
        return;
#ifndef HAVE_5ARGS_MREMAP
    assert( !ctxt->bSingleThreadUsage );
    CPLAcquireMutex(ctxt->hMutexThreadArray, 1000.0);
    ctxt->pahThreads = (pthread_t*) CPLRealloc(ctxt->pahThreads,
                                (ctxt->nThreads + 1) * sizeof(pthread_t));
    ctxt->pahThreads[ctxt->nThreads] = pthread_self();
    ctxt->nThreads ++;

    CPLReleaseMutex(ctxt->hMutexThreadArray);
#endif
}

/************************************************************************/
/*                     CPLVirtualMemUnDeclareThread()                   */
/************************************************************************/

void CPLVirtualMemUnDeclareThread(CPLVirtualMem* ctxt)
{
    if( ctxt->bFileMemoryMapped )
        return;
#ifndef HAVE_5ARGS_MREMAP
    int i;
    pthread_t self = pthread_self();
    assert( !ctxt->bSingleThreadUsage );
    CPLAcquireMutex(ctxt->hMutexThreadArray, 1000.0);
    for(i = 0; i < ctxt->nThreads; i++)
    {
        if( ctxt->pahThreads[i] == self )
        {
            if( i < ctxt->nThreads - 1 )
                memmove(ctxt->pahThreads + i + 1,
                        ctxt->pahThreads + i,
                        (ctxt->nThreads - 1 - i) * sizeof(pthread_t));
            ctxt->nThreads --;
            break;
        }
    }

    CPLReleaseMutex(ctxt->hMutexThreadArray);
#endif
}


/************************************************************************/
/*                     CPLVirtualMemGetPageToFill()                     */
/************************************************************************/

/* Must be paired with CPLVirtualMemAddPage */
static
void* CPLVirtualMemGetPageToFill(CPLVirtualMem* ctxt, void* start_page_addr)
{
    void* pPageToFill;

    if( ctxt->bSingleThreadUsage )
    {
        pPageToFill = start_page_addr;
        assert(mprotect(pPageToFill, ctxt->nPageSize, PROT_READ | PROT_WRITE) == 0);
    }
    else
    {
#ifndef HAVE_5ARGS_MREMAP
        CPLAcquireMutex(ctxt->hMutexThreadArray, 1000.0);
        if( ctxt->nThreads == 1 )
        {
            pPageToFill = start_page_addr;
            assert(mprotect(pPageToFill, ctxt->nPageSize, PROT_READ | PROT_WRITE) == 0);
        }
        else
#endif
        {
            /* Allocate a temporary writable page that the user */
            /* callback can fill */
            pPageToFill = mmap(NULL, ctxt->nPageSize,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            assert(pPageToFill != MAP_FAILED);
        }
    }
    return pPageToFill;
}

/************************************************************************/
/*                        CPLVirtualMemAddPage()                        */
/************************************************************************/

static
void CPLVirtualMemAddPage(CPLVirtualMem* ctxt, void* target_addr, void* pPageToFill,
                       OpType opType, pthread_t hRequesterThread)
{
    size_t iPage = (int)(((char*)target_addr - (char*)ctxt->pData) / ctxt->nPageSize);
    if( ctxt->nLRUSize == ctxt->nCacheMaxSizeInPages )
    {
        /* fprintfstderr("uncaching page %d\n", iPage); */
        int nOldPage = ctxt->panLRUPageIndices[ctxt->iLRUStart];
        void* addr = (char*)ctxt->pData + nOldPage * ctxt->nPageSize;
        if( ctxt->eAccessMode == VIRTUALMEM_READWRITE &&
            ctxt->pfnUnCachePage != NULL &&
            TEST_BIT(ctxt->pabitRWMappedPages, nOldPage) )
        {
            size_t nToBeEvicted = ctxt->nPageSize;
            if( (char*)addr + nToBeEvicted >= (char*) ctxt->pData + ctxt->nSize )
                nToBeEvicted = (char*) ctxt->pData + ctxt->nSize - (char*)addr;

            ctxt->pfnUnCachePage(ctxt,
                                 nOldPage * ctxt->nPageSize,
                                 addr,
                                 nToBeEvicted,
                                 ctxt->pCbkUserData);
        }
        /* "Free" the least recently used page */
        UNSET_BIT(ctxt->pabitMappedPages, nOldPage);
        UNSET_BIT(ctxt->pabitRWMappedPages, nOldPage);
        /* Free the old page */
        /* Not sure how portable it is to do that that way... */
        assert(mmap(addr, ctxt->nPageSize, PROT_NONE,
                    MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == addr);
    }
    ctxt->panLRUPageIndices[ctxt->iLRUStart] = iPage;
    ctxt->iLRUStart = (ctxt->iLRUStart + 1) % ctxt->nCacheMaxSizeInPages;
    if( ctxt->nLRUSize < ctxt->nCacheMaxSizeInPages )
    {
        ctxt->nLRUSize ++;
    }
    SET_BIT(ctxt->pabitMappedPages, iPage);

    if( ctxt->bSingleThreadUsage )
    {
        if( opType == OP_STORE && ctxt->eAccessMode == VIRTUALMEM_READWRITE )
        {
            /* let (and mark) the page writable since the instruction that triggered */
            /* the fault is a store */
            SET_BIT(ctxt->pabitRWMappedPages, iPage);
        }
        else if( ctxt->eAccessMode != VIRTUALMEM_READONLY )
        {
            assert(mprotect(target_addr, ctxt->nPageSize, PROT_READ) == 0);
        }
    }
    else
    {
#ifdef HAVE_5ARGS_MREMAP
        (void)hRequesterThread;

        if( opType == OP_STORE && ctxt->eAccessMode == VIRTUALMEM_READWRITE )
        {
            /* let (and mark) the page writable since the instruction that triggered */
            /* the fault is a store */
            SET_BIT(ctxt->pabitRWMappedPages, iPage);
        }
        else if( ctxt->eAccessMode != VIRTUALMEM_READONLY )
        {
            /* Turn the temporary page read-only before remapping it. We will only turn it */
            /* writtable when a new fault occurs (and that the mapping is writable) */
            assert(mprotect(pPageToFill, ctxt->nPageSize, PROT_READ) == 0);
        }
        /* Can now remap the pPageToFill onto the target page */
        assert(mremap(pPageToFill, ctxt->nPageSize, ctxt->nPageSize,
                    MREMAP_MAYMOVE | MREMAP_FIXED, target_addr) == target_addr);

#else
        if (ctxt->nThreads > 1 )
        {
            int i;

            /* Pause threads that share this mem view */
            CPLAtomicInc(&nWaitHelperThread);

            /* Install temporary SIGUSR1 signal handler */
            struct sigaction act, oldact;
            act.sa_sigaction = CPLVirtualMemSIGUSR1Handler;
            sigemptyset (&act.sa_mask);
            /* We don't want the sigsegv handler to be called when we are */
            /* running the sigusr1 handler */
            assert(sigaddset(&act.sa_mask, SIGSEGV) == 0);
            act.sa_flags = 0;
            assert(sigaction(SIGUSR1, &act, &oldact) == 0);

            for(i = 0; i < ctxt->nThreads; i++)
            {
                if( ctxt->pahThreads[i] != hRequesterThread )
                {
                    /* fprintfstderr("stopping thread %X\n", ctxt->pahThreads[i]); */
                    assert(pthread_kill( ctxt->pahThreads[i], SIGUSR1 ) == 0);
                }
            }

            /* Wait that they are all paused */
            while( nCountThreadsInSigUSR1 != ctxt->nThreads-1 )
                usleep(1);

            /* Restore old SIGUSR1 signal handler */
            assert(sigaction(SIGUSR1, &oldact, NULL) == 0);

            assert(mprotect(target_addr, ctxt->nPageSize, PROT_READ | PROT_WRITE) == 0);
            /*fprintfstderr("memcpying page %d\n", iPage);*/
            memcpy(target_addr, pPageToFill, ctxt->nPageSize);
            
            if( opType == OP_STORE && ctxt->eAccessMode == VIRTUALMEM_READWRITE )
            {
                /* let (and mark) the page writable since the instruction that triggered */
                /* the fault is a store */
                SET_BIT(ctxt->pabitRWMappedPages, iPage);
            }
            else
            {
                assert(mprotect(target_addr, ctxt->nPageSize, PROT_READ) == 0);
            }

            /* Wake up sleeping threads */
            CPLAtomicDec(&nWaitHelperThread);
            while( nCountThreadsInSigUSR1 != 0 )
                usleep(1);

            assert(munmap(pPageToFill, ctxt->nPageSize) == 0);
        }
        else
        {
            if( opType == OP_STORE && ctxt->eAccessMode == VIRTUALMEM_READWRITE )
            {
                /* let (and mark) the page writable since the instruction that triggered */
                /* the fault is a store */
                SET_BIT(ctxt->pabitRWMappedPages, iPage);
            }
            else if( ctxt->eAccessMode != VIRTUALMEM_READONLY )
            {
                assert(mprotect(target_addr, ctxt->nPageSize, PROT_READ) == 0);
            }
        }

        CPLReleaseMutex(ctxt->hMutexThreadArray);
#endif
    }
}

/************************************************************************/
/*                    CPLVirtualMemGetOpTypeImm()                       */
/************************************************************************/

#if defined(__x86_64__) || defined(__i386__)
static OpType CPLVirtualMemGetOpTypeImm(GByte val_rip)
{
    OpType opType = OP_UNKNOWN;
    if( (/*val_rip >= 0x00 &&*/ val_rip <= 0x07) ||
        (val_rip >= 0x40 && val_rip <= 0x47) ) /* add $,(X) */
        opType = OP_STORE;
    if( (val_rip >= 0x08 && val_rip <= 0x0f) ||
        (val_rip >= 0x48 && val_rip <= 0x4f) ) /* or $,(X) */
        opType = OP_STORE;
    if( (val_rip >= 0x20 && val_rip <= 0x27) ||
        (val_rip >= 0x60 && val_rip <= 0x67) ) /* and $,(X) */
        opType = OP_STORE;
    if( (val_rip >= 0x28 && val_rip <= 0x2f) ||
        (val_rip >= 0x68 && val_rip <= 0x6f) ) /* sub $,(X) */
        opType = OP_STORE;
    if( (val_rip >= 0x30 && val_rip <= 0x37) ||
        (val_rip >= 0x70 && val_rip <= 0x77) ) /* xor $,(X) */
        opType = OP_STORE;
    if( (val_rip >= 0x38 && val_rip <= 0x3f) ||
        (val_rip >= 0x78 && val_rip <= 0x7f) ) /* cmp $,(X) */
        opType = OP_LOAD;
    return opType;
}
#endif

/************************************************************************/
/*                      CPLVirtualMemGetOpType()                        */
/************************************************************************/

/* We don't need exhaustivity. It is just a hint for an optimization : */
/* if the fault occurs on a store operation, then we can directly put */
/* the page in writable mode if the mapping allows it */

static OpType CPLVirtualMemGetOpType(const GByte* rip)
{
    OpType opType = OP_UNKNOWN;

#if defined(__x86_64__) || defined(__i386__)
    switch(rip[0])
    {
        case 0x00: /* add %al,(%rax) */
        case 0x01: /* add %eax,(%rax) */
            opType = OP_STORE;
            break;
        case 0x02: /* add (%rax),%al */
        case 0x03: /* add (%rax),%eax */
            opType = OP_LOAD;
            break;

        case 0x08: /* or %al,(%rax) */
        case 0x09: /* or %eax,(%rax) */
            opType = OP_STORE;
            break;
        case 0x0a: /* or (%rax),%al */
        case 0x0b: /* or (%rax),%eax */
            opType = OP_LOAD;
            break;

        case 0x0f:
        {
            switch(rip[1])
            {
                case 0xb6: /* movzbl (%rax),%eax */
                case 0xb7: /* movzwl (%rax),%eax */
                case 0xbe: /* movsbl (%rax),%eax */
                case 0xbf: /* movswl (%rax),%eax */
                    opType = OP_LOAD;
                    break;
                default:
                    break;
            }
            break;
        }
        case 0xc6: /* movb $,(%rax) */
        case 0xc7: /* movl $,(%rax) */
            opType = OP_STORE;
            break;

        case 0x20: /* and %al,(%rax) */
        case 0x21: /* and %eax,(%rax) */
            opType = OP_STORE;
            break;
        case 0x22: /* and (%rax),%al */
        case 0x23: /* and (%rax),%eax */
            opType = OP_LOAD;
            break;

        case 0x28: /* sub %al,(%rax) */
        case 0x29: /* sub %eax,(%rax) */
            opType = OP_STORE;
            break;
        case 0x2a: /* sub (%rax),%al */
        case 0x2b: /* sub (%rax),%eax */
            opType = OP_LOAD;
            break;

        case 0x30: /* xor %al,(%rax) */
        case 0x31: /* xor %eax,(%rax) */
            opType = OP_STORE;
            break;
        case 0x32: /* xor (%rax),%al */
        case 0x33: /* xor (%rax),%eax */
            opType = OP_LOAD;
            break;

        case 0x38: /* cmp %al,(%rax) */
        case 0x39: /* cmp %eax,(%rax) */
            opType = OP_LOAD;
            break;
        case 0x40:
        {
            switch(rip[1])
            {
                case 0x00: /* add %spl,(%rax) */
                    opType = OP_STORE;
                    break;
                case 0x02: /* add (%rax),%spl */
                    opType = OP_LOAD;
                    break;
                case 0x28: /* sub %spl,(%rax) */
                    opType = OP_STORE;
                    break;
                case 0x2a: /* sub (%rax),%spl */
                    opType = OP_LOAD;
                    break;
                case 0x3a: /* cmp (%rax),%spl */
                    opType = OP_LOAD;
                    break;
                case 0x8a: /* mov (%rax),%spl */
                    opType = OP_LOAD;
                    break;
                default:
                    break;
            }
            break;
        }
#if defined(__x86_64__)
        case 0x41: /* reg=%al/%eax, X=%r8 */
        case 0x42: /* reg=%al/%eax, X=%rax,%r8,1 */
        case 0x43: /* reg=%al/%eax, X=%r8,%r8,1 */
        case 0x44: /* reg=%r8b/%r8w, X = %rax */
        case 0x45: /* reg=%r8b/%r8w, X = %r8 */
        case 0x46: /* reg=%r8b/%r8w, X = %rax,%r8,1 */
        case 0x47: /* reg=%r8b/%r8w, X = %r8,%r8,1 */
        {
            switch(rip[1])
            {
                case 0x00: /* add regb,(X) */
                case 0x01: /* add regl,(X) */
                    opType = OP_STORE;
                    break;
                case 0x02: /* add (X),regb */
                case 0x03: /* add (X),regl */
                    opType = OP_LOAD;
                    break;
                case 0x0f:
                {
                    switch(rip[2])
                    {
                        case 0xb6: /* movzbl (X),regl */
                        case 0xb7: /* movzwl (X),regl */
                        case 0xbe: /* movsbl (X),regl */
                        case 0xbf: /* movswl (X),regl */
                            opType = OP_LOAD;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case 0x28: /* sub regb,(X) */
                case 0x29: /* sub regl,(X) */
                    opType = OP_STORE;
                    break;
                case 0x2a: /* sub (X),regb */
                case 0x2b: /* sub (X),regl */
                    opType = OP_LOAD;
                    break;
                case 0x38: /* cmp  regb,(X) */
                case 0x39: /* cmp  regl,(X) */
                    opType = OP_LOAD;
                    break;
                case 0x80: /* cmpb,... $,(X) */
                case 0x81: /* cmpl,... $,(X) */
                case 0x83: /* cmpl,... $,(X) */
                    opType = CPLVirtualMemGetOpTypeImm(rip[2]);
                    break;
                case 0x88: /* mov regb,(X) */
                case 0x89: /* mov regl,(X) */
                    opType = OP_STORE;
                    break;
                case 0x8a: /* mov (X),regb */
                case 0x8b: /* mov (X),regl */
                    opType = OP_LOAD;
                    break;
                case 0xc6: /* movb $,(X) */
                case 0xc7: /* movl $,(X) */
                    opType = OP_STORE;
                    break;
                case 0x84: /* test %al,(X) */
                    opType = OP_LOAD;
                    break;
                case 0xf6: /* testb $,(X) or notb (X) */
                case 0xf7: /* testl $,(X) or notl (X)*/
                {
                    if( rip[2] < 0x10 ) /* test (X) */
                        opType = OP_LOAD;
                    else /* not (X) */
                        opType = OP_STORE;
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case 0x48: /* reg=%rax, X=%rax or %rax,%rax,1 */
        case 0x49: /* reg=%rax, X=%r8 or %r8,%rax,1 */
        case 0x4a: /* reg=%rax, X=%rax,%r8,1 */
        case 0x4b: /* reg=%rax, X=%r8,%r8,1 */
        case 0x4c: /* reg=%r8, X=%rax or %rax,%rax,1 */
        case 0x4d: /* reg=%r8, X=%r8 or %r8,%rax,1 */
        case 0x4e: /* reg=%r8, X=%rax,%r8,1 */
        case 0x4f: /* reg=%r8, X=%r8,%r8,1 */
        {
            switch(rip[1])
            {
                case 0x01: /* add reg,(X) */
                    opType = OP_STORE;
                    break;
                case 0x03: /* add (X),reg */
                    opType = OP_LOAD;
                    break;

                case 0x09: /* or reg,(%rax) */
                    opType = OP_STORE;
                    break;
                case 0x0b: /* or (%rax),reg */
                    opType = OP_LOAD;
                    break;
                case 0x0f:
                {
                    switch(rip[2])
                    {
                        case 0xc3: /* movnti reg,(X) */
                            opType = OP_STORE;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case 0x21: /* and reg,(X) */
                    opType = OP_STORE;
                    break;
                case 0x23: /* and (X),reg */
                    opType = OP_LOAD;
                    break;

                case 0x29: /* sub reg,(X) */
                    opType = OP_STORE;
                    break;
                case 0x2b: /* sub (X),reg */
                    opType = OP_LOAD;
                    break;

                case 0x31: /* xor reg,(X) */
                    opType = OP_STORE;
                    break;
                case 0x33: /* xor (X),reg */
                    opType = OP_LOAD;
                    break;

                case 0x39: /* cmp reg,(X) */
                    opType = OP_LOAD;
                    break;

                case 0x81:
                case 0x83:
                    opType = CPLVirtualMemGetOpTypeImm(rip[2]);
                    break;

                case 0x85: /* test reg,(X) */
                    opType = OP_LOAD;
                    break;

                case 0x89: /* mov reg,(X) */
                    opType = OP_STORE;
                    break;
                case 0x8b: /* mov (X),reg */
                    opType = OP_LOAD;
                    break;

                case 0xc7: /* movq $,(X) */
                    opType = OP_STORE;
                    break;

                case 0xf7:
                {
                    if( rip[2] < 0x10 ) /* testq $,(X) */
                        opType = OP_LOAD;
                    else /* notq (X) */
                        opType = OP_STORE;
                    break;
                }
                default:
                    break;
            }
            break;
        }
#endif
        case 0x66:
        {
            switch(rip[1])
            {
                case 0x01: /* add %ax,(%rax) */
                    opType = OP_STORE;
                    break;
                case 0x03: /* add (%rax),%ax */
                    opType = OP_LOAD;
                    break;
                case 0x0f:
                {
                    switch(rip[2])
                    {
                        case 0x2e: /* ucomisd (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x6f: /* movdqa (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x7f: /* movdqa %xmm0,(%rax) */
                            opType = OP_STORE;
                            break;
                        case 0xb6: /* movzbw (%rax),%ax */
                            opType = OP_LOAD;
                            break;
                        case 0xe7: /* movntdq %xmm0,(%rax) */
                            opType = OP_STORE;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case 0x29: /* sub %ax,(%rax) */
                    opType = OP_STORE;
                    break;
                case 0x2b: /* sub (%rax),%ax */
                    opType = OP_LOAD;
                    break;
                case 0x39: /* cmp %ax,(%rax) */
                    opType = OP_LOAD;
                    break;
#if defined(__x86_64__)
                case 0x41: /* reg = %ax (or %xmm0), X = %r8 */
                case 0x42: /* reg = %ax (or %xmm0), X = %rax,%r8,1 */
                case 0x43: /* reg = %ax (or %xmm0), X = %r8,%r8,1 */
                case 0x44: /* reg = %r8w (or %xmm8), X = %rax */
                case 0x45: /* reg = %r8w (or %xmm8), X = %r8 */
                case 0x46: /* reg = %r8w (or %xmm8), X = %rax,%r8,1 */
                case 0x47: /* reg = %r8w (or %xmm8), X = %r8,%r8,1 */
                {
                    switch(rip[2])
                    {
                        case 0x01: /* add reg,(X) */
                            opType = OP_STORE;
                            break;
                        case 0x03: /* add (X),reg */
                            opType = OP_LOAD;
                            break;
                        case 0x0f:
                        {
                            switch(rip[3])
                            {
                                case 0x2e: /* ucomisd (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0x6f: /* movdqa (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0x7f: /* movdqa reg,(X) */
                                    opType = OP_STORE;
                                    break;
                                case 0xb6: /* movzbw (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0xe7: /* movntdq reg,(X) */
                                    opType = OP_STORE;
                                    break;
                                default:
                                    break;
                            }
                            break;
                        }
                        case 0x29: /* sub reg,(X) */
                            opType = OP_STORE;
                            break;
                        case 0x2b: /* sub (X),reg */
                            opType = OP_LOAD;
                            break;
                        case 0x39: /* cmp reg,(X) */
                            opType = OP_LOAD;
                            break;
                        case 0x81: /* cmpw,... $,(X) */
                        case 0x83: /* cmpw,... $,(X) */
                            opType = CPLVirtualMemGetOpTypeImm(rip[3]);
                            break;
                        case 0x85: /* test reg,(X) */
                            opType = OP_LOAD;
                            break;
                        case 0x89: /* mov reg,(X) */
                            opType = OP_STORE;
                            break;
                        case 0x8b: /* mov (X),reg */
                            opType = OP_LOAD;
                            break;
                        case 0xc7: /* movw $,(X) */
                            opType = OP_STORE;
                            break;
                        case 0xf7:
                        {
                            if( rip[3] < 0x10 ) /* testw $,(X) */
                                opType = OP_LOAD;
                            else /* notw (X) */
                                opType = OP_STORE;
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
#endif
                case 0x81: /* cmpw,... $,(%rax) */
                case 0x83: /* cmpw,... $,(%rax) */
                    opType = CPLVirtualMemGetOpTypeImm(rip[2]);
                    break;

                case 0x85: /* test %ax,(%rax) */
                    opType = OP_LOAD;
                    break;
                case 0x89: /* mov %ax,(%rax) */
                    opType = OP_STORE;
                    break;
                case 0x8b: /* mov (%rax),%ax */
                    opType = OP_LOAD;
                    break;
                case 0xc7: /* movw $,(%rax) */
                    opType = OP_STORE;
                    break;
                case 0xf3:
                {
                    switch( rip[2] )
                    {
                        case 0xa5: /* rep movsw %ds:(%rsi),%es:(%rdi) */
                            opType = OP_MOVS_RSI_RDI;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case 0xf7: /* testw $,(%rax) or notw (%rax) */
                {
                    if( rip[2] < 0x10 ) /* test */
                        opType = OP_LOAD;
                    else /* not */
                        opType = OP_STORE;
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case 0x80: /* cmpb,... $,(%rax) */
        case 0x81: /* cmpl,... $,(%rax) */
        case 0x83: /* cmpl,... $,(%rax) */
            opType = CPLVirtualMemGetOpTypeImm(rip[1]);
            break;
        case 0x84: /* test %al,(%rax) */
        case 0x85: /* test %eax,(%rax) */
            opType = OP_LOAD;
            break;
        case 0x88: /* mov %al,(%rax) */
            opType = OP_STORE;
            break;
        case 0x89: /* mov %eax,(%rax) */
            opType = OP_STORE;
            break;
        case 0x8a: /* mov (%rax),%al */
            opType = OP_LOAD;
            break;
        case 0x8b: /* mov (%rax),%eax */
            opType = OP_LOAD;
            break;
        case 0xd9: /* 387 float */
        {
            if( rip[1] < 0x08 ) /* flds (%eax) */
                opType = OP_LOAD;
            else if( rip[1] >= 0x18 && rip[1] <= 0x20 ) /* fstps (%eax) */
                opType = OP_STORE;
            break;
        }
        case 0xf2: /* SSE 2 */
        {
            switch(rip[1])
            {
                case 0x0f:
                {
                    switch(rip[2])
                    {
                        case 0x10: /* movsd (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x11: /* movsd %xmm0,(%rax) */
                            opType = OP_STORE;
                            break;
                        case 0x58: /* addsd (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x59: /* mulsd (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x5c: /* subsd (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x5e: /* divsd (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        default:
                            break;
                    }
                    break;
                }
#if defined(__x86_64__)
                case 0x41: /* reg=%xmm0, X=%r8 or %r8,%rax,1 */
                case 0x42: /* reg=%xmm0, X=%rax,%r8,1 */
                case 0x43: /* reg=%xmm0, X=%r8,%r8,1 */
                case 0x44: /* reg=%xmm8, X=%rax or %rax,%rax,1*/
                case 0x45: /* reg=%xmm8, X=%r8 or %r8,%rax,1 */
                case 0x46: /* reg=%xmm8, X=%rax,%r8,1 */
                case 0x47: /* reg=%xmm8, X=%r8,%r8,1 */
                {
                    switch(rip[2])
                    {
                        case 0x0f:
                        {
                            switch(rip[3])
                            {
                                case 0x10: /* movsd (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0x11: /* movsd reg,(X) */
                                    opType = OP_STORE;
                                    break;
                                case 0x58: /* addsd (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                 case 0x59: /* mulsd (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0x5c: /* subsd (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0x5e: /* divsd (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                default:
                                    break;
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
#endif
                default:
                    break;
            }
            break;
        }
        case 0xf3:
        {
            switch(rip[1])
            {
                case 0x0f: /* SSE 2 */
                {
                    switch(rip[2])
                    {
                        case 0x10: /* movss (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x11: /* movss %xmm0,(%rax) */
                            opType = OP_STORE;
                            break;
                        case 0x6f: /* movdqu (%rax),%xmm0 */
                            opType = OP_LOAD;
                            break;
                        case 0x7f: /* movdqu %xmm0,(%rax) */
                            opType = OP_STORE;
                            break;
                        default:
                            break;
                    }
                    break;
                }
#if defined(__x86_64__)
                case 0x41: /* reg=%xmm0, X=%r8 */
                case 0x42: /* reg=%xmm0, X=%rax,%r8,1 */
                case 0x43: /* reg=%xmm0, X=%r8,%r8,1 */
                case 0x44: /* reg=%xmm8, X = %rax */
                case 0x45: /* reg=%xmm8, X = %r8 */
                case 0x46: /* reg=%xmm8, X = %rax,%r8,1 */
                case 0x47: /* reg=%xmm8, X = %r8,%r8,1 */
                {
                    switch(rip[2])
                    {
                        case 0x0f: /* SSE 2 */
                        {
                            switch(rip[3])
                            {
                                case 0x10: /* movss (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0x11: /* movss reg,(X) */
                                    opType = OP_STORE;
                                    break;
                                case 0x6f: /* movdqu (X),reg */
                                    opType = OP_LOAD;
                                    break;
                                case 0x7f: /* movdqu reg,(X) */
                                    opType = OP_STORE;
                                    break;
                                default:
                                    break;
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case 0x48:
                {
                    switch(rip[2])
                    {
                        case 0xa5: /* rep movsq %ds:(%rsi),%es:(%rdi) */
                            opType = OP_MOVS_RSI_RDI;
                            break;
                        default:
                            break;
                    }
                    break;
                }
#endif
                case 0xa4: /* rep movsb %ds:(%rsi),%es:(%rdi) */
                case 0xa5: /* rep movsl %ds:(%rsi),%es:(%rdi) */
                    opType = OP_MOVS_RSI_RDI;
                    break;
                case 0xa6: /* repz cmpsb %es:(%rdi),%ds:(%rsi) */
                    opType = OP_LOAD;
                    break;
                default:
                    break;
            }
            break;
        }
        case 0xf6: /* testb $,(%rax) or notb (%rax) */
        case 0xf7: /* testl $,(%rax) or notl (%rax) */
        {
            if( rip[1] < 0x10 ) /* test */
                opType = OP_LOAD;
            else /* not */
                opType = OP_STORE;
            break;
        }
        default:
            break;
    }
#endif
    return opType;
}

/************************************************************************/
/*                    CPLVirtualMemManagerPinAddrInternal()             */
/************************************************************************/

static int CPLVirtualMemManagerPinAddrInternal(CPLVirtualMemMsgToWorkerThread* msg)
{
    char wait_ready;
    char response_buf[4];

    /* Wait for the helper thread to be ready to process another request */
    while(TRUE)
    {
        int ret = read(pVirtualMemManager->pipefd_wait_thread[0], &wait_ready, 1);
        if( ret < 0 && errno == EINTR )
            ;
        else
        {
            assert(ret == 1);
            break;
        }
    }

    /* Pass the address that caused the fault to the helper thread */
    assert(write(pVirtualMemManager->pipefd_to_thread[1], msg, sizeof(*msg))
            == sizeof(*msg));

    /* Wait that the helper thread has fixed the fault */
    while(TRUE)
    {
        int ret = read(pVirtualMemManager->pipefd_from_thread[0], response_buf, 4);
        if( ret < 0 && errno == EINTR )
            ;
        else
        {
            assert(ret == 4);
            break;
        }
    }

    /* In case the helper thread did not recognize the address as being */
    /* one that it should take care of, just rely on the previous SIGSEGV */
    /* handler (with might abort the process) */
    return( memcmp(response_buf, MAPPING_FOUND, 4) == 0 );
}

/************************************************************************/
/*                      CPLVirtualMemPin()                              */
/************************************************************************/

void CPLVirtualMemPin(CPLVirtualMem* ctxt,
                      void* pAddr, size_t nSize, int bWriteOp)
{
    if( ctxt->bFileMemoryMapped )
        return;

    CPLVirtualMemMsgToWorkerThread msg;
    size_t i = 0, n;

    memset(&msg, 0, sizeof(msg));
    msg.hRequesterThread = pthread_self();
    msg.opType = (bWriteOp) ? OP_STORE : OP_LOAD;

    char* pBase = (char*)ALIGN_DOWN(pAddr, ctxt->nPageSize);
    n = ((char*)pAddr - pBase + nSize + ctxt->nPageSize - 1) / ctxt->nPageSize;
    for(i=0; i<n; i++)
    {
        msg.pFaultAddr = (char*) pBase + i * ctxt->nPageSize;
        CPLVirtualMemManagerPinAddrInternal(&msg);
    }
}

/************************************************************************/
/*                   CPLVirtualMemManagerSIGSEGVHandler()               */
/************************************************************************/

#if defined(__x86_64__)
#define REG_IP      REG_RIP
#define REG_SI      REG_RSI
#define REG_DI      REG_RDI
#elif defined(__i386__)
#define REG_IP      REG_EIP
#define REG_SI      REG_ESI
#define REG_DI      REG_EDI
#endif

/* We must take care of only using "asynchronous-signal-safe" functions in a signal handler */
/* pthread_self(), read() and write() are such. See */
/* https://www.securecoding.cert.org/confluence/display/seccode/SIG30-C.+Call+only+asynchronous-safe+functions+within+signal+handlers */
static void CPLVirtualMemManagerSIGSEGVHandler(int the_signal,
                                             siginfo_t* the_info,
                                             void* the_ctxt)
{
    CPLVirtualMemMsgToWorkerThread msg;

    memset(&msg, 0, sizeof(msg));
    msg.pFaultAddr = the_info->si_addr;
    msg.hRequesterThread = pthread_self();
    msg.opType = OP_UNKNOWN;

#if defined(__x86_64__) || defined(__i386__)
    ucontext_t* the_ucontext = (ucontext_t* )the_ctxt;
    const GByte* rip = (const GByte*)the_ucontext->uc_mcontext.gregs[REG_IP];
    msg.opType = CPLVirtualMemGetOpType(rip);
    /*fprintfstderr("at rip %p, bytes: %02x %02x %02x %02x\n",
                          rip, rip[0], rip[1], rip[2], rip[3]);*/
    if( msg.opType == OP_MOVS_RSI_RDI )
    {
        void* rsi = (void*)the_ucontext->uc_mcontext.gregs[REG_SI];
        void* rdi = (void*)the_ucontext->uc_mcontext.gregs[REG_DI];

        /*fprintfstderr("fault=%p rsi=%p rsi=%p\n", msg.pFaultAddr, rsi, rdi);*/
        if( msg.pFaultAddr == rsi )
        {
            /*fprintfstderr("load\n");*/
            msg.opType = OP_LOAD;
        }
        else if( msg.pFaultAddr == rdi )
        {
            /*fprintfstderr("store\n");*/
            msg.opType = OP_STORE;
        }
    }
#ifdef DEBUG_VIRTUALMEM
    else if( msg.opType == OP_UNKNOWN )
    {
        static int bHasWarned = FALSE;
        if( !bHasWarned )
        {
            bHasWarned = TRUE;
            fprintfstderr("at rip %p, unknown bytes: %02x %02x %02x %02x\n",
                          rip, rip[0], rip[1], rip[2], rip[3]);
        }
    }
#endif
#endif

    /*fprintfstderr("entering handler for %X (addr=%p)\n", pthread_self(), the_info->si_addr); */

    if( the_info->si_code != SEGV_ACCERR )
    {
        pVirtualMemManager->oldact.sa_sigaction(the_signal, the_info, the_ctxt);
        return;
    }

    if( !CPLVirtualMemManagerPinAddrInternal(&msg) )
    {
        /* In case the helper thread did not recognize the address as being */
        /* one that it should take care of, just rely on the previous SIGSEGV */
        /* handler (with might abort the process) */
        pVirtualMemManager->oldact.sa_sigaction(the_signal, the_info, the_ctxt);
    }

    /*fprintfstderr("leaving handler for %X (addr=%p)\n", pthread_self(), the_info->si_addr);*/
}

/************************************************************************/
/*                      CPLVirtualMemManagerThread()                    */
/************************************************************************/

static void CPLVirtualMemManagerThread(void* unused_param)
{
    (void)unused_param;

    while(TRUE)
    {
        char i_m_ready = 1;
        int i;
        CPLVirtualMem* ctxt = NULL;
        int bMappingFound = FALSE;
        CPLVirtualMemMsgToWorkerThread msg;

        /* Signal that we are ready to process a new request */
        assert(write(pVirtualMemManager->pipefd_wait_thread[1], &i_m_ready, 1) == 1);

        /* Fetch the address to process */
        assert(read(pVirtualMemManager->pipefd_to_thread[0], &msg,
                    sizeof(msg)) == sizeof(msg));

        /* If CPLVirtualMemManagerTerminate() is called, it will use BYEBYE_ADDR as a */
        /* means to ask for our termination */
        if( msg.pFaultAddr == BYEBYE_ADDR )
            break;

        /* Lookup for a mapping that contains addr */
        CPLAcquireMutex(hVirtualMemManagerMutex, 1000.0);
        for(i=0;i<pVirtualMemManager->nVirtualMemCount;i++)
        {
            ctxt = pVirtualMemManager->pasVirtualMem[i];
            if( (char*)msg.pFaultAddr >= (char*) ctxt->pData &&
                (char*)msg.pFaultAddr < (char*) ctxt->pData + ctxt->nSize )
            {
                bMappingFound = TRUE;
                break;
            }
        }
        CPLReleaseMutex(hVirtualMemManagerMutex);

        if( bMappingFound )
        {
            char* start_page_addr = (char*)ALIGN_DOWN(msg.pFaultAddr, ctxt->nPageSize);
            int iPage = (int)
                (((char*)start_page_addr - (char*)ctxt->pData) / ctxt->nPageSize);

            if( iPage == ctxt->iLastPage )
            {
                /* In case 2 threads try to access the same page */
                /* concurrently it is possible that we are asked to mapped */
                /* the page again whereas it is always mapped. However */
                /* if that number of successive retries is too high, this */
                /* is certainly a sign that something else happen, like */
                /* trying to write-access a read-only page */
                /* 100 is a bit of magic number. I believe it must be */
                /* at least the number of concurrent threads. 100 seems */
                /* to be really safe ! */
                ctxt->nRetry ++;
                /* fprintfstderr("retry on page %d : %d\n", (int)iPage, ctxt->nRetry); */
                if( ctxt->nRetry >= 100 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "CPLVirtualMemManagerThread: trying to "
                             "write into read-only mapping");
                    assert(write(pVirtualMemManager->pipefd_from_thread[1],
                                    MAPPING_NOT_FOUND, 4) == 4);
                    break;
                }
                else if( msg.opType != OP_LOAD &&
                         ctxt->eAccessMode == VIRTUALMEM_READWRITE &&
                         !TEST_BIT(ctxt->pabitRWMappedPages, iPage) )
                {
                    /* fprintfstderr("switching page %d to write mode\n", (int)iPage); */
                    SET_BIT(ctxt->pabitRWMappedPages, iPage);
                    assert(mprotect(start_page_addr, ctxt->nPageSize,
                                    PROT_READ | PROT_WRITE) == 0);
                }
            }
            else
            {
                ctxt->iLastPage = iPage;
                ctxt->nRetry = 0;

                if( TEST_BIT(ctxt->pabitMappedPages, iPage) )
                {
                    if( msg.opType != OP_LOAD &&
                        ctxt->eAccessMode == VIRTUALMEM_READWRITE &&
                        !TEST_BIT(ctxt->pabitRWMappedPages, iPage) )
                    {
                        /*fprintfstderr("switching page %d to write mode\n",
                                iPage);*/
                        SET_BIT(ctxt->pabitRWMappedPages, iPage);
                        assert(mprotect(start_page_addr, ctxt->nPageSize,
                                        PROT_READ | PROT_WRITE) == 0);
                    }
                    else
                    {
                        /*fprintfstderr("unexpected case for page %d\n",
                                iPage);*/
                    }
                }
                else
                {
                    void* pPageToFill;
                    size_t nToFill;
                    pPageToFill = CPLVirtualMemGetPageToFill(ctxt, start_page_addr);

                    nToFill = ctxt->nPageSize;
                    if( start_page_addr + nToFill >= (char*) ctxt->pData + ctxt->nSize )
                        nToFill = (char*) ctxt->pData + ctxt->nSize - start_page_addr;

                    ctxt->pfnCachePage(
                            ctxt,
                            start_page_addr - (char*) ctxt->pData,
                            pPageToFill,
                            nToFill,
                            ctxt->pCbkUserData);

                    /* Now remap this page to its target address and */
                    /* register it in the LRU */
                    CPLVirtualMemAddPage(ctxt, start_page_addr, pPageToFill,
                                      msg.opType, msg.hRequesterThread);
                }
            }

            /* Warn the segfault handler that we have finished our job */
            assert(write(pVirtualMemManager->pipefd_from_thread[1],
                            MAPPING_FOUND, 4) == 4);
        }
        else
        {
            /* Warn the segfault handler that we have finished our job */
            /* but that the fault didn't occur in a memory range that is under */
            /* our responsability */
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CPLVirtualMemManagerThread: no mapping found");
            assert(write(pVirtualMemManager->pipefd_from_thread[1],
                         MAPPING_NOT_FOUND, 4) == 4);
        }
    }
}

/************************************************************************/
/*                       CPLVirtualMemManagerInit()                     */
/************************************************************************/

static void CPLVirtualMemManagerInit(void)
{
    CPLMutexHolderD(&hVirtualMemManagerMutex);
    if( pVirtualMemManager != NULL )
        return;

    struct sigaction act;
    pVirtualMemManager = (CPLVirtualMemManager*) CPLMalloc(sizeof(CPLVirtualMemManager));
    pVirtualMemManager->pasVirtualMem = NULL;
    pVirtualMemManager->nVirtualMemCount = 0;
    assert(pipe(pVirtualMemManager->pipefd_to_thread) == 0);
    assert(pipe(pVirtualMemManager->pipefd_from_thread) == 0);
    assert(pipe(pVirtualMemManager->pipefd_wait_thread) == 0);

    /* Install our custom SIGSEGV handler */
    act.sa_sigaction = CPLVirtualMemManagerSIGSEGVHandler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    assert(sigaction(SIGSEGV, &act, &pVirtualMemManager->oldact) == 0);

    /* Starts the helper thread */
    pVirtualMemManager->hHelperThread =
            CPLCreateJoinableThread(CPLVirtualMemManagerThread, NULL);
    assert(pVirtualMemManager->hHelperThread != NULL);
}

/************************************************************************/
/*                      CPLVirtualMemManagerTerminate()                 */
/************************************************************************/

void CPLVirtualMemManagerTerminate(void)
{
    char wait_ready;
    CPLVirtualMemMsgToWorkerThread msg;

    if( pVirtualMemManager == NULL )
        return;

    msg.pFaultAddr = BYEBYE_ADDR;
    msg.opType = OP_UNKNOWN;
    memset(&msg.hRequesterThread, 0, sizeof(msg.hRequesterThread));

    /* Wait for the helper thread to be ready */
    assert(read(pVirtualMemManager->pipefd_wait_thread[0],
                &wait_ready, 1) == 1);

    /* Ask it to terminate */
    assert(write(pVirtualMemManager->pipefd_to_thread[1], &msg, sizeof(msg)) == sizeof(msg));

    /* Wait for its termination */
    CPLJoinThread(pVirtualMemManager->hHelperThread);

    /* Cleanup everything */
    while(pVirtualMemManager->nVirtualMemCount > 0)
        CPLVirtualMemFree(pVirtualMemManager->pasVirtualMem[pVirtualMemManager->nVirtualMemCount - 1]);
    CPLFree(pVirtualMemManager->pasVirtualMem);

    close(pVirtualMemManager->pipefd_to_thread[0]);
    close(pVirtualMemManager->pipefd_to_thread[1]);
    close(pVirtualMemManager->pipefd_from_thread[0]);
    close(pVirtualMemManager->pipefd_from_thread[1]);
    close(pVirtualMemManager->pipefd_wait_thread[0]);
    close(pVirtualMemManager->pipefd_wait_thread[1]);

    /* Restore previous handler */
    sigaction(SIGSEGV, &pVirtualMemManager->oldact, NULL);

    CPLFree(pVirtualMemManager);
    pVirtualMemManager = NULL;

    CPLDestroyMutex(hVirtualMemManagerMutex);
    hVirtualMemManagerMutex = NULL;
}

#else /* defined(__linux) && defined(CPL_MULTIPROC_PTHREAD) */

size_t CPLGetPageSize(void)
{
    return 0;
}

CPLVirtualMem *CPLVirtualMemNew(size_t nSize,
                                size_t nCacheSize,
                                size_t nPageSizeHint,
                                int bSingleThreadUsage,
                                CPLVirtualMemAccessMode eAccessMode,
                                CPLVirtualMemCachePageCbk pfnCachePage,
                                CPLVirtualMemUnCachePageCbk pfnUnCachePage,
                                CPLVirtualMemFreeUserData pfnFreeUserData,
                                void *pCbkUserData)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "CPLVirtualMemNew() unsupported on this operating system / configuration");
    return NULL;
}

int CPLIsVirtualMemFileMapAvailable(void)
{
    return FALSE;
}

CPLVirtualMem *CPLVirtualMemFileMapNew(VSILFILE* fp,
                                    vsi_l_offset nOffset,
                                    vsi_l_offset nLength,
                                    CPLVirtualMemAccessMode eAccessMode,
                                    CPLVirtualMemFreeUserData pfnFreeUserData,
                                    void *pCbkUserData)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "CPLVirtualMemFileMapNew() unsupported on this operating system / configuration");
    return NULL;
}

CPLVirtualMem *CPLVirtualMemDerivedNew(CPLVirtualMem* pVMemBase,
                                       vsi_l_offset nOffset,
                                       vsi_l_offset nSize,
                                       CPLVirtualMemFreeUserData pfnFreeUserData,
                                       void *pCbkUserData)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "CPLVirtualMemDerivedNew() unsupported on this operating system / configuration");
    return NULL;
}

void CPLVirtualMemFree(CPLVirtualMem* ctxt)
{
}

void* CPLVirtualMemGetAddr(CPLVirtualMem* ctxt)
{
    return NULL;
}

size_t CPLVirtualMemGetSize(CPLVirtualMem* ctxt)
{
    return 0;
}

int CPLVirtualMemIsFileMapping(CPLVirtualMem* ctxt)
{
    return FALSE;
}

CPLVirtualMemAccessMode CPLVirtualMemGetAccessMode(CPLVirtualMem* ctxt)
{
    return VIRTUALMEM_READONLY;
}

size_t CPLVirtualMemGetPageSize(CPLVirtualMem* ctxt)
{
    return 0;
}

int CPLVirtualMemIsAccessThreadSafe(CPLVirtualMem* ctxt)
{
    return FALSE;
}

void CPLVirtualMemDeclareThread(CPLVirtualMem* ctxt)
{
}

void CPLVirtualMemUnDeclareThread(CPLVirtualMem* ctxt)
{
}

void CPLVirtualMemPin(CPLVirtualMem* ctxt,
                      void* pAddr, size_t nSize, int bWriteOp)
{
}

void CPLVirtualMemManagerTerminate(void)
{
}

#endif /* defined(__linux) && defined(CPL_MULTIPROC_PTHREAD) */
