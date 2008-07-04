/******************************************************************************
 * $Id: gdalctexpander.h $
 *
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private declarations
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#ifndef GDAL_CTEXPANDER_H_INCLUDED
#define GDAL_CTEXPANDER_H_INCLUDED

#include "gdal.h"

#ifdef __cplusplus

#include "gdal_proxy.h"

/* ******************************************************************** */
/*                    GDALCTExpandedBand                               */
/* ******************************************************************** */


class GDALCTExpandedBand : public GDALRasterBand
{
    protected:
        GDALRasterBand *poPalettedRasterBand;
        int             nColors;
        GByte          *pabyLUT;

        virtual CPLErr IReadBlock( int, int, void * );
        virtual CPLErr IWriteBlock( int, int, void * ); /* not supported */

    public:
                         GDALCTExpandedBand(GDALRasterBand* poPalettedRasterBand, int nComponent);
        virtual         ~GDALCTExpandedBand();

        virtual GDALColorInterp GetColorInterpretation();
};

/* ******************************************************************** */
/*                    GDALCTExpandedDataset                            */
/* ******************************************************************** */

class GDALCTExpandedDataset: public GDALProxyDataset
{
    protected:
        GDALDataset*  poPalettedDataset;

        virtual GDALDataset *GetUnderlyingDataset();

        /* don't proxy */
        virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );

        /* don't proxy */
        virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *, int, int, int );
    public:
                      GDALCTExpandedDataset(GDALDataset* poPalettedDataset, int nBands, int bShared = FALSE);
        virtual      ~GDALCTExpandedDataset();

        /* don't proxy */
        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                   int nBufXSize, int nBufYSize, 
                                   GDALDataType eDT, 
                                   int nBandCount, int *panBandList,
                                   char **papszOptions );
};


#endif

/* ******************************************************************** */
/*            C types and methods declarations                          */
/* ******************************************************************** */


CPL_C_START

GDALDatasetH CPL_DLL GDALCTExpandedDatasetCreate( GDALDatasetH hPalettedDS, int nComponents, int bShared);


GDALRasterBandH CPL_DLL GDALCTExpandedBandCreate( GDALRasterBandH hPalettedRasterBand, int nComponent );
void CPL_DLL GDALCTExpandedBandDelete( GDALRasterBandH hCTExpandedBand );

CPL_C_END

#endif /* GDAL_CTEXPANDER_H_INCLUDED */
