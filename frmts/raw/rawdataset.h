/******************************************************************************
 * $Id$
 *
 * Project:  Raw Translator
 * Purpose:  Implementation of RawDataset class.  Indented to be subclassed
 *           by other raw formats.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.8  2002/03/21 16:22:03  warmerda
 * fixed friend declarations
 *
 * Revision 1.7  2001/12/12 18:15:46  warmerda
 * preliminary update for large raw file support
 *
 * Revision 1.6  2001/03/23 03:25:32  warmerda
 * Added nodata support
 *
 * Revision 1.5  2000/08/16 15:51:17  warmerda
 * allow floating (datasetless) raw bands
 *
 * Revision 1.4  2000/07/20 13:38:56  warmerda
 * make classes public with CPL_DLL
 *
 * Revision 1.3  2000/03/31 13:36:41  warmerda
 * RawRasterBand no longer depends on RawDataset
 *
 * Revision 1.2  1999/08/13 02:36:57  warmerda
 * added write support
 *
 * Revision 1.1  1999/07/23 19:34:34  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/* ==================================================================== */
/*				RawDataset				*/
/* ==================================================================== */
/************************************************************************/

class RawRasterBand;

class CPL_DLL RawDataset : public GDALDataset
{
    friend class RawRasterBand;

  public:
                 RawDataset();
                 ~RawDataset();

};

/************************************************************************/
/* ==================================================================== */
/*                            RawRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class CPL_DLL RawRasterBand : public GDALRasterBand
{
    friend class RawDataset;

    FILE	*fpRaw;
    int         bIsVSIL;

    vsi_l_offset nImgOffset;
    int		nPixelOffset;
    int		nLineOffset;
    int		bNativeOrder;

    int		bNoDataSet;
    double	dfNoDataValue;
    
    int		nLoadedScanline;
    void	*pLineBuffer;

    int         Seek( vsi_l_offset, int );
    size_t      Read( void *, size_t, size_t );
    size_t      Write( void *, size_t, size_t );

  public:

                 RawRasterBand( GDALDataset *poDS, int nBand, FILE * fpRaw, 
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int bIsVSIL = FALSE );

                 RawRasterBand( FILE * fpRaw, 
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int nXSize, int nYSize, int bIsVSIL = FALSE );

                 ~RawRasterBand();

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual double GetNoDataValue( int *pbSuccess = NULL );

    CPLErr       AccessLine( int iLine );

    void	 StoreNoDataValue( double );
};

