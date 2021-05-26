/******************************************************************************
 *
 * Project:  JPEG-2000
 * Purpose:  Implementation of the ISO/IEC 15444-1 standard based on Kakadu.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

// $Id$

#ifdef DEBUG_BOOL
#define DO_NOT_USE_DEBUG_BOOL
#endif

#include <cstring>
#include <algorithm>
#include <cmath>

#include "../mem/memdataset.h"
#include "cpl_multiproc.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdaljp2abstractdataset.h"
#include "gdaljp2metadata.h"
#include "jp2kak_headers.h"
#include "subfile_source.h"

// Before v7.5 Kakadu does not advertise its version well
// After v7.5 Kakadu has KDU_{MAJOR,MINOR,PATCH}_VERSION defines so it is easier
// For older releases compile with them manually specified.  e.g.:
// -DKDU_MAJOR_VERSION=7 -DKDU_MINOR_VERSION=3 -DKDU_PATCH_VERSION=2
#ifndef KDU_MAJOR_VERSION
#  error Compile with Kakadu library version.
#endif

#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 5)
    using namespace kdu_core;
    using namespace kdu_supp;
#endif

/************************************************************************/
/* ==================================================================== */
/*                            JP2KAKDataset                             */
/* ==================================================================== */
/************************************************************************/

class JP2KAKDataset final: public GDALJP2AbstractDataset
{
    friend class JP2KAKRasterBand;

    kdu_codestream oCodeStream;
    kdu_compressed_source *poInput = nullptr;
    kdu_compressed_source *poRawInput = nullptr;
    jp2_family_src  *family = nullptr;
    kdu_client      *jpip_client = nullptr;
    kdu_dims dims;
    int            nResCount = 0;
    bool           bPreferNPReads = false;
    kdu_thread_env *poThreadEnv = nullptr;

    bool           bCached = false;
    bool           bResilient = false;
    bool           bFussy = false;
    bool           bUseYCC = false;

    bool           bPromoteTo8Bit = false;

    bool        TestUseBlockIO( int, int, int, int, int, int,
                                GDALDataType, int, int * );
    CPLErr      DirectRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg);

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

  public:
             JP2KAKDataset();
    virtual ~JP2KAKDataset() override;

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc,
                                    void * ) override;

    static void KakaduInitialize();
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            JP2KAKRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class JP2KAKRasterBand final: public GDALPamRasterBand
{
    friend class JP2KAKDataset;

    JP2KAKDataset *poBaseDS;

    int         nDiscardLevels;
    kdu_dims    band_dims;
    int         nOverviewCount;
    JP2KAKRasterBand **papoOverviewBand;

    kdu_client      *jpip_client;
    kdu_codestream oCodeStream;

    GDALColorTable oCT;
    GDALColorInterp eInterp;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

    bool           HasExternalOverviews()
                   { return GDALPamRasterBand::GetOverviewCount() != 0; }

  public:
                JP2KAKRasterBand( int, int, kdu_codestream, int, kdu_client *,
                                  jp2_channels, JP2KAKDataset * );
    virtual ~JP2KAKRasterBand() override;

    virtual CPLErr IReadBlock( int, int, void * ) override;

    virtual int    GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview( int ) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;

    // Internal.

    void        ApplyPalette( jp2_palette oJP2Palette );
    void        ProcessYCbCrTile( kdu_tile tile, GByte *pabyBuffer,
                                  int nBlockXOff, int nBlockYOff,
                                  int nTileOffsetX, int nTileOffsetY );
    void        ProcessTile(kdu_tile tile, GByte *pabyBuffer );
};

/************************************************************************/
/* ==================================================================== */
/*                     Set up messaging services                        */
/* ==================================================================== */
/************************************************************************/

class kdu_cpl_error_message final: public kdu_thread_safe_message
{
  public:  // Member classes.
    using kdu_thread_safe_message::put_text;

    explicit kdu_cpl_error_message( CPLErr eErrClass ) :
        m_eErrClass(eErrClass),
        m_pszError(nullptr)
    {}

    void put_text(const char *string) override
    {
        if( m_pszError == nullptr )
        {
            m_pszError = CPLStrdup(string);
        }
        else
        {
            m_pszError = static_cast<char *>(CPLRealloc(
                m_pszError, strlen(m_pszError) + strlen(string) + 1));
            strcat(m_pszError, string);
        }
    }

    class JP2KAKException {};

    void flush(bool end_of_message = false) override
    {
        kdu_thread_safe_message::flush(end_of_message);

        if( m_pszError == nullptr )
            return;
        if( m_pszError[strlen(m_pszError) - 1] == '\n' )
            m_pszError[strlen(m_pszError) - 1] = '\0';

        CPLError(m_eErrClass, CPLE_AppDefined, "%s", m_pszError);
        CPLFree(m_pszError);
        m_pszError = nullptr;

        if( end_of_message && m_eErrClass == CE_Failure )
        {
            throw JP2KAKException();
        }
    }

  private:
    CPLErr m_eErrClass;
    char *m_pszError;
};
