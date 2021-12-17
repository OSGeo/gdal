/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Authors:  Denis Nadeau <denis.nadeau@gmail.com>
 *           Sam Gillingham <gillingham.sam@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2008-2018, Even Rouault <even.rouault at spatialys.com>
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

 // This file contains the Virtual File Layer implementation that calls through
 // to the VSI functions and should be included by HDF5 based drivers that wish
 // to use the VFL for /vsi file system support.

#ifndef HDF5VFL_H_INCLUDED_
#define HDF5VFL_H_INCLUDED_

#include "cpl_port.h"

#include <algorithm>
#include <mutex>

#ifdef H5FD_FEAT_SUPPORTS_SWMR_IO
#define HDF5_1_10_OR_LATER
#endif

static std::mutex gMutex;
static hid_t hFileDriver = -1;

static H5FD_t *HDF5_vsil_open(const char *name, unsigned flags, hid_t fapl_id,
            haddr_t maxaddr);
static herr_t HDF5_vsil_close(H5FD_t *_file);
static herr_t HDF5_vsil_query(const H5FD_t *_f1, unsigned long *flags);
static haddr_t HDF5_vsil_get_eoa(const H5FD_t *_file, H5FD_mem_t type);
static herr_t HDF5_vsil_set_eoa(H5FD_t *_file, H5FD_mem_t type, haddr_t addr);
static haddr_t HDF5_vsil_get_eof(const H5FD_t *_file
#ifdef HDF5_1_10_OR_LATER
                                 , H5FD_mem_t type
#endif
);
static herr_t HDF5_vsil_read(H5FD_t *_file, H5FD_mem_t type, hid_t fapl_id,
                             haddr_t addr, size_t size, void *buf);
static herr_t HDF5_vsil_write(H5FD_t *_file, H5FD_mem_t type, hid_t fapl_id,
                              haddr_t addr, size_t size, const void *buf);
static herr_t HDF5_vsil_truncate(H5FD_t *_file, hid_t dxpl_id, hbool_t closing);

static hid_t HDF5VFLGetFileDriver();
static void HDF5VFLUnloadFileDriver();

#define MAXADDR (((haddr_t)1<<(8*sizeof(haddr_t)-1))-1)

/* See https://support.hdfgroup.org/HDF5/doc/TechNotes/VFL.html */
static const H5FD_class_t HDF5_vsil_g = {
    "vsil",                     /* name */
    MAXADDR,                    /* maxaddr  */
    H5F_CLOSE_WEAK,             /* fc_degree  */
#ifdef HDF5_1_10_OR_LATER
    nullptr,                    /* terminate */
#endif
    nullptr,                    /* sb_size  */
    nullptr,                    /* sb_encode */
    nullptr,                    /* sb_decode */
    0,                          /* fapl_size */
    nullptr,                    /* fapl_get  */
    nullptr,                    /* fapl_copy */
    nullptr,                    /* fapl_free */
    0,                          /* dxpl_size */
    nullptr,                    /* dxpl_copy */
    nullptr,                    /* dxpl_free */
    HDF5_vsil_open,             /* open */
    HDF5_vsil_close,            /* close */
    nullptr,                    /* cmp  */
    HDF5_vsil_query,            /* query */
    nullptr,                    /* get_type_map */
    nullptr,                    /* alloc */
    nullptr,                    /* free */
    HDF5_vsil_get_eoa,          /* get_eoa */
    HDF5_vsil_set_eoa,          /* set_eoa */
    HDF5_vsil_get_eof,          /* get_eof */
    nullptr,                    /* get_handle */
    HDF5_vsil_read,             /* read */
    HDF5_vsil_write,            /* write */
    nullptr,                    /* flush */
    HDF5_vsil_truncate,         /* truncate */
    nullptr,                    /* lock */
    nullptr,                    /* unlock */
    H5FD_FLMAP_DICHOTOMY        /* fl_map */
};

typedef struct HDF5_vsil_t {
    H5FD_t          pub;            /* must be first */
    VSILFILE       *fp = nullptr;
    haddr_t         eoa = 0;
    haddr_t         eof = 0;
} HDF5_vsil_t;

static H5FD_t *HDF5_vsil_open(const char *name, unsigned flags,
                              hid_t /*fapl_id*/, haddr_t /*maxaddr*/)
{
    const char* openFlags = "rb";
    if( (H5F_ACC_RDWR & flags) )
        openFlags = "rb+";
    if( (H5F_ACC_TRUNC & flags) || (H5F_ACC_CREAT & flags) )
        openFlags = "wb+";

    VSILFILE* fp = VSIFOpenL(name, openFlags);
    if( !fp )
    {
        return nullptr;
    }
    if( (H5F_ACC_TRUNC & flags) )
    {
        VSIFTruncateL(fp, 0);
    }

    HDF5_vsil_t* fh = new HDF5_vsil_t;
    memset(&fh->pub, 0, sizeof(fh->pub));
    if( !fh )
    {
        VSIFCloseL(fp);
        return nullptr;
    }
    fh->fp = fp;

    VSIFSeekL(fh->fp, 0, SEEK_END);
    fh->eof = static_cast<haddr_t>(VSIFTellL(fh->fp));

    return reinterpret_cast<H5FD_t*>(fh);
}

static herr_t HDF5_vsil_close(H5FD_t *_file)
{
    HDF5_vsil_t* fh = reinterpret_cast<HDF5_vsil_t*>(_file);
    int ret = VSIFCloseL(fh->fp);
    delete fh;
    return ret;
}

static herr_t HDF5_vsil_query(const H5FD_t *, unsigned long *flags /* out */)
{
    *flags = H5FD_FEAT_AGGREGATE_METADATA |
             H5FD_FEAT_ACCUMULATE_METADATA |
             H5FD_FEAT_DATA_SIEVE |
             H5FD_FEAT_AGGREGATE_SMALLDATA;
    return 0;
}

static haddr_t HDF5_vsil_get_eoa(const H5FD_t *_file, H5FD_mem_t /*type*/)
{
    const HDF5_vsil_t* fh = reinterpret_cast<const HDF5_vsil_t*>(_file);
    return fh->eoa;
}

static herr_t HDF5_vsil_set_eoa(H5FD_t *_file, H5FD_mem_t /*type*/,
                                haddr_t addr)
{
    HDF5_vsil_t* fh = reinterpret_cast<HDF5_vsil_t*>(_file);
    fh->eoa = addr;
    return 0;
}

static haddr_t HDF5_vsil_get_eof(const H5FD_t *_file
#ifdef HDF5_1_10_OR_LATER
                                 , H5FD_mem_t /* type */
#endif
                                )
{
    const HDF5_vsil_t* fh = reinterpret_cast<const HDF5_vsil_t*>(_file);
    return fh->eof;
}

static herr_t HDF5_vsil_read(H5FD_t *_file, H5FD_mem_t /* type */,
                             hid_t /* dxpl_id */,
                             haddr_t addr, size_t size, void *buf /*out*/)
{
    HDF5_vsil_t* fh = reinterpret_cast<HDF5_vsil_t*>(_file);
    VSIFSeekL(fh->fp, static_cast<vsi_l_offset>(addr), SEEK_SET);
    return VSIFReadL(buf, size, 1, fh->fp) == 1 ? 0 : -1;
}

static herr_t HDF5_vsil_write(H5FD_t *_file, H5FD_mem_t /* type */,
                              hid_t /* dxpl_id */,
                              haddr_t addr, size_t size,
                              const void *buf /*out*/)
{
    HDF5_vsil_t* fh = reinterpret_cast<HDF5_vsil_t*>(_file);
    VSIFSeekL(fh->fp, static_cast<vsi_l_offset>(addr), SEEK_SET);
    int ret = VSIFWriteL(buf, size, 1, fh->fp) == 1 ? 0 : -1;
    fh->eof = std::max(fh->eof, static_cast<haddr_t>(VSIFTellL(fh->fp)));
    return ret;
}

static herr_t HDF5_vsil_truncate(H5FD_t *_file, hid_t /* dxpl_id*/,
                                 hbool_t /*closing*/)
{
    HDF5_vsil_t* fh = reinterpret_cast<HDF5_vsil_t*>(_file);
    if(fh->eoa != fh->eof)
    {
        if( VSIFTruncateL(fh->fp, fh->eoa) < 0 )
        {
            return -1;
        }
        fh->eof = fh->eoa;
    }
    return 0;
}

/************************************************************************/
/*                       HDF5VFLGetFileDriver()                         */
/************************************************************************/

static hid_t HDF5VFLGetFileDriver()
{
    std::lock_guard<std::mutex> oLock(gMutex);
    if( hFileDriver < 0 )
    {
        hFileDriver = H5FDregister(&HDF5_vsil_g);
#if H5E_auto_t_vers == 2
        // also, don't print error messages from KEA driver.
        // (which uses H5E_auto_t_vers=2 - the default, hdf uses 1 for some reason).
        // These tend to be meaningless - ie no GCP's found etc.
        // They didn't seem to be shown when we didn't use the VFL layer
        // - maybe VFL turns them on?
        H5Eset_auto(H5E_DEFAULT, nullptr, nullptr);
#endif
    }
    return hFileDriver;
}

/************************************************************************/
/*                     HDF5VFLUnloadFileDriver()                        */
/************************************************************************/

static void HDF5VFLUnloadFileDriver()
{
    {
        std::lock_guard<std::mutex> oLock(gMutex);
        if( hFileDriver >= 0 )
        {
            H5FDunregister(hFileDriver);
            hFileDriver = -1;
        }
    }
}

#endif /* HDF5VFL_H_INCLUDED_ */
