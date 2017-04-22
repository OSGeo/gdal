/******************************************************************************
 * $Id$
 *
 * Project:  Raw Translator
 * Purpose:  Implementation of RawDataset class.  Intended to be subclassed
 *           by other raw formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GDAL_FRMTS_RAW_RAWDATASET_H_INCLUDED
#define GDAL_FRMTS_RAW_RAWDATASET_H_INCLUDED

#include "gdal_pam.h"

/************************************************************************/
/* ==================================================================== */
/*                              RawDataset                              */
/* ==================================================================== */
/************************************************************************/

class RawRasterBand;

/**
 * \brief Abstract Base Class dedicated to define new raw dataset types.
 */
class CPL_DLL RawDataset : public GDALPamDataset
{
    friend class RawRasterBand;

  protected:
    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int,
                                   void *, int, int, GDALDataType,
                                   int, int *,
                                   GSpacing nPixelSpace, GSpacing nLineSpace,
                                   GSpacing nBandSpace,
                                   GDALRasterIOExtraArg* psExtraArg ) CPL_OVERRIDE;
  public:
                 RawDataset();
         virtual ~RawDataset() = 0;

  private:
    CPL_DISALLOW_COPY_ASSIGN(RawDataset)
};

/************************************************************************/
/* ==================================================================== */
/*                            RawRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/**
 * \brief Abstract Base Class dedicated to define raw datasets.
 * \note It is not defined an Abstract Base Class, but it's advised to
 * consider it as such and not use it directly in client's code.
 */
class CPL_DLL RawRasterBand : public GDALPamRasterBand
{
protected:
    friend class RawDataset;

    FILE       *fpRaw;
    VSILFILE   *fpRawL;
    int         bIsVSIL;

    vsi_l_offset nImgOffset;
    int         nPixelOffset;
    int         nLineOffset;
    int         nLineSize;
    int         bNativeOrder;

    int         nLoadedScanline;
    void        *pLineBuffer;
    void        *pLineStart;
    int         bDirty;

    GDALColorTable *poCT;
    GDALColorInterp eInterp;

    char           **papszCategoryNames;

    int         bOwnsFP;

    int         Seek( vsi_l_offset, int );
    size_t      Read( void *, size_t, size_t );
    size_t      Write( void *, size_t, size_t );

    CPLErr      AccessBlock( vsi_l_offset nBlockOff, size_t nBlockSize,
                             void * pData );
    int         IsSignificantNumberOfLinesLoaded( int nLineOff, int nLines );
    void        Initialize();

    virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) CPL_OVERRIDE;

    int         CanUseDirectIO(int nXOff, int nYOff, int nXSize, int nYSize,
                               GDALDataType eBufType);

public:

                 RawRasterBand( GDALDataset *poDS, int nBand, void * fpRaw,
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int bIsVSIL = FALSE, int bOwnsFP = FALSE );

                 RawRasterBand( void * fpRaw,
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int nXSize, int nYSize, int bIsVSIL = FALSE, int bOwnsFP = FALSE );

    virtual ~RawRasterBand() /* = 0 */ ;

    // should CPL_OVERRIDE RasterIO eventually.

    virtual CPLErr  IReadBlock( int, int, void * ) CPL_OVERRIDE;
    virtual CPLErr  IWriteBlock( int, int, void * ) CPL_OVERRIDE;

    virtual GDALColorTable *GetColorTable() CPL_OVERRIDE;
    virtual GDALColorInterp GetColorInterpretation() CPL_OVERRIDE;
    virtual CPLErr SetColorTable( GDALColorTable * ) CPL_OVERRIDE;
    virtual CPLErr SetColorInterpretation( GDALColorInterp ) CPL_OVERRIDE;

    virtual char **GetCategoryNames() CPL_OVERRIDE;
    virtual CPLErr SetCategoryNames( char ** ) CPL_OVERRIDE;

    virtual CPLErr  FlushCache() CPL_OVERRIDE;

    virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions ) CPL_OVERRIDE;

    CPLErr          AccessLine( int iLine );

    void            SetAccess( GDALAccess eAccess );

    // this is deprecated.
    void         StoreNoDataValue( double );

    // Query methods for internal data.
    vsi_l_offset GetImgOffset() { return nImgOffset; }
    int          GetPixelOffset() { return nPixelOffset; }
    int          GetLineOffset() { return nLineOffset; }
    int          GetNativeOrder() { return bNativeOrder; }
    int          GetIsVSIL() { return bIsVSIL; }
    FILE        *GetFP() { return (bIsVSIL) ? reinterpret_cast<FILE *>( fpRawL ) : fpRaw; }
    VSILFILE    *GetFPL() { CPLAssert(bIsVSIL); return fpRawL; }
    int          GetOwnsFP() { return bOwnsFP; }

  private:
    CPL_DISALLOW_COPY_ASSIGN(RawRasterBand)
};

#endif // GDAL_FRMTS_RAW_RAWDATASET_H_INCLUDED
