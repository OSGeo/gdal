/******************************************************************************
 * $Id$
 *
 * Project:  Memory Array Translator
 * Purpose:  Declaration of MEMDataset, and MEMRasterBand.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.5  2002/03/01 16:45:53  warmerda
 * added support for retaining nodata value
 *
 * Revision 1.4  2001/10/26 20:03:28  warmerda
 * added C entry point for creating MEMRasterBand
 *
 * Revision 1.3  2000/07/20 13:38:48  warmerda
 * make classes public with CPL_DLL
 *
 * Revision 1.2  2000/07/19 19:07:04  warmerda
 * break linkage between MEMDataset and MEMRasterBand
 *
 * Revision 1.1  2000/07/19 15:55:11  warmerda
 * New
 *
 */

#ifndef MEMDATASET_H_INCLUDED
#define MEMDATASET_H_INCLUDED

#include "gdal_priv.h"

CPL_C_START
void	GDALRegister_MEM(void);
GDALRasterBandH CPL_DLL MEMCreateRasterBand( GDALDataset *, int, GByte *,
                                             GDALDataType, int, int, int );
CPL_C_END

/************************************************************************/
/*				MEMDataset				*/
/************************************************************************/

class MEMRasterBand;

class CPL_DLL MEMDataset : public GDALDataset
{
  public:
                 MEMDataset();
                 ~MEMDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            MEMRasterBand                             */
/************************************************************************/

class CPL_DLL MEMRasterBand : public GDALRasterBand
{
    GByte      *pabyData;
    int         nPixelOffset;
    int         nLineOffset;
    int         bOwnData;

    int         bNoDataSet;
    double      dfNoData;

  public:

                   MEMRasterBand( GDALDataset *poDS, int nBand,
                                  GByte *pabyData, GDALDataType eType,
                                  int nPixelOffset, int nLineOffset,
                                  int bAssumeOwnership );
    virtual        ~MEMRasterBand();

    // should override RasterIO eventually.

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr SetNoDataValue( double );
};

#endif /* ndef MEMDATASET_H_INCLUDED */

