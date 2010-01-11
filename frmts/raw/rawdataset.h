/******************************************************************************
 * $Id$
 *
 * Project:  Raw Translator
 * Purpose:  Implementation of RawDataset class.  Intented to be subclassed
 *           by other raw formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "gdal_pam.h"

/************************************************************************/
/* ==================================================================== */
/*				RawDataset				*/
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
                                   int, int *, int, int, int );
  public:
                 RawDataset();
                 ~RawDataset() = 0;

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

    FILE	*fpRaw;
    int         bIsVSIL;

    vsi_l_offset nImgOffset;
    int		nPixelOffset;
    int		nLineOffset;
    int         nLineSize;
    int		bNativeOrder;

    int		nLoadedScanline;
    void	*pLineBuffer;
    int         bDirty;

    GDALColorTable *poCT;
    GDALColorInterp eInterp;

    char           **papszCategoryNames;
    
    int         bOwnsFP;

    int         Seek( vsi_l_offset, int );
    size_t      Read( void *, size_t, size_t );
    size_t      Write( void *, size_t, size_t );

    CPLErr      AccessBlock( vsi_l_offset nBlockOff, int nBlockSize,
                             void * pData );
    int         IsLineLoaded( int nLineOff, int nLines );
    void        Initialize();

    virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

  public:

                 RawRasterBand( GDALDataset *poDS, int nBand, FILE * fpRaw, 
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int bIsVSIL = FALSE, int bOwnsFP = FALSE );

                 RawRasterBand( FILE * fpRaw, 
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int nXSize, int nYSize, int bIsVSIL = FALSE, int bOwnsFP = FALSE );

                 ~RawRasterBand();

    // should override RasterIO eventually.
    
    virtual CPLErr  IReadBlock( int, int, void * );
    virtual CPLErr  IWriteBlock( int, int, void * );

    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual CPLErr SetColorInterpretation( GDALColorInterp );

    virtual char **GetCategoryNames();
    virtual CPLErr SetCategoryNames( char ** );

    virtual CPLErr  FlushCache();

    CPLErr          AccessLine( int iLine );

    void            SetAccess( GDALAccess eAccess );
    
    // this is deprecated.
    void	 StoreNoDataValue( double );

    // Query methods for internal data. 
    vsi_l_offset GetImgOffset() { return nImgOffset; }
    int          GetPixelOffset() { return nPixelOffset; }
    int          GetLineOffset() { return nLineOffset; }
    int          GetNativeOrder() { return bNativeOrder; }
    int          GetIsVSIL() { return bIsVSIL; }
    FILE        *GetFP() { return fpRaw; }
    int          GetOwnsFP() { return bOwnsFP; }
};

