/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

/************************************************************************/
/* ==================================================================== */
/*                            WCSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class WCSRasterBand final: public GDALPamRasterBand
{
    friend class WCSDataset;

    int            iOverview;
    int            nResFactor;

    WCSDataset    *poODS;

    int            nOverviewCount;
    WCSRasterBand **papoOverviews;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;

  public:

                   WCSRasterBand( WCSDataset *, int nBand, int iOverview );
    virtual ~WCSRasterBand();

    virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
};
