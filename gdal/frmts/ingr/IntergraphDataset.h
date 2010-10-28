/*****************************************************************************
 * $Id$
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Read selected types of Intergraph Raster Format
 * Author:   Ivan Lucena [ivan.lucena@pmldnet.com]
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************/

#include "IngrTypes.h"

//  ----------------------------------------------------------------------------
//     Intergraph GDALDataset
//  ----------------------------------------------------------------------------

class IntergraphDataset : public GDALPamDataset
{
    friend class IntergraphRasterBand;
    friend class IntergraphRGBBand;
    friend class IntergraphBitmapBand;
    friend class IntergraphRLEBand;

private:
    VSILFILE       *fp;
    char           *pszFilename;
    double          adfGeoTransform[6];

    INGR_HeaderOne  hHeaderOne;
    INGR_HeaderTwoA hHeaderTwo;
    INGR_VirtualFile    hVirtual;

public:
    IntergraphDataset();
    ~IntergraphDataset();

    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Create( const char *pszFilename,
        int nXSize,
        int nYSize,
        int nBands, 
        GDALDataType eType,
        char **papszOptions );
    static GDALDataset *CreateCopy( const char *pszFilename, 
        GDALDataset *poSrcDS,
        int bStrict,
        char **papszOptions,
        GDALProgressFunc pfnProgress, 
        void * pProgressData );

    virtual CPLErr GetGeoTransform( double *padfTransform );
    virtual CPLErr SetGeoTransform( double *padfTransform );
    virtual CPLErr SetProjection( const char *pszProjString );
};

