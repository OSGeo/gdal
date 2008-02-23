/*****************************************************************************
 * $Id: $
 *
 * Project:  TerraLib Raster Database schema support
 * Purpose:  Read/Write TerraLib Raster band (see TerraLib.org)
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

#include "gdal.h"
#include "gdal_priv.h"

//  ----------------------------------------------------------------------------
//     TerraLib TerraLibRasterBand
//  ----------------------------------------------------------------------------

class TerraLibRasterBand : public GDALRasterBand
{
    friend class TerraLibDataset;

protected:
    GDALColorTable *poColorTable;
    GByte	       *pabyBlockBuf;

public:
    TerraLibRasterBand( TerraLibDataset *poDS );
    ~TerraLibRasterBand();

    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum( int *pbSuccess = NULL );    
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );
    virtual CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void *pImage );
    virtual CPLErr SetColorTable( GDALColorTable *poColorTable ); 
    virtual CPLErr SetStatistics( double dfMin, double dfMax, double dfMean, double dfStdDev );
};
