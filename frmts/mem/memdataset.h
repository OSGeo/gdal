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
 * Revision 1.1  2000/07/19 15:55:11  warmerda
 * New
 *
 */

#ifndef MEMDATASET_H_INCLUDED
#define MEMDATASET_H_INCLUDED

#include "gdal_priv.h"

CPL_C_START
void	GDALRegister_MEM(void);
CPL_C_END

/************************************************************************/
/*				MEMDataset				*/
/************************************************************************/

class MEMRasterBand;

class MEMDataset : public GDALDataset
{
    friend	MEMRasterBand;

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

class MEMRasterBand : public GDALRasterBand
{
    friend	MEMDataset;

    GByte      *pabyData;
    int         nPixelOffset;
    int         nLineOffset;
    int         bOwnData;

  public:

                   MEMRasterBand( MEMDataset *poDS, int nBand,
                                  GByte *pabyData, GDALDataType eType, 
                                  int nPixelOffset, int nLineOffset,
                                  int bAssumeOwnership );
    virtual        ~MEMRasterBand();

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * ); 
};

#endif /* ndef MEMDATASET_H_INCLUDED */

