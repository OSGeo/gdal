/******************************************************************************
 * $Id: $
 *
 * Name:     georaster_rasterband.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterRasterBand methods
 * Author:   Ivan Lucena [ivan.lucena@pmldnet.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
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

#include "georaster_priv.h"

//  ---------------------------------------------------------------------------
//                                                        GeoRasterRasterBand()
//  ---------------------------------------------------------------------------

GeoRasterRasterBand::GeoRasterRasterBand( GeoRasterDataset *poGDS, int nBand )
{
    poDS                = (GDALDataset*) poGDS;
    nBand               = nBand;
    poGeoRaster         = poGDS->poGeoRaster;
    poColorTable        = new GDALColorTable();
    nRasterXSize        = poGeoRaster->nRasterColumns;
    nRasterYSize        = poGeoRaster->nRasterRows;
    nBlockXSize         = poGeoRaster->nColumnBlockSize;
    nBlockYSize         = poGeoRaster->nRowBlockSize;
    nBlocksPerColumn    = poGeoRaster->nTotalColumnBlocks;
    nBlocksPerRow       = poGeoRaster->nTotalRowBlocks;
    dfNoData            = 0.0;
    bValidStats         = false;
}

//  ---------------------------------------------------------------------------
//                                                       ~GeoRasterRasterBand()
//  ---------------------------------------------------------------------------

GeoRasterRasterBand::~GeoRasterRasterBand()
{
    ObjFree_nt( poColorTable );
}

//  ---------------------------------------------------------------------------
//                                                                 IReadBlock()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::IReadBlock( int nBlockXOff, 
                                        int nBlockYOff, 
                                        void *pImage )
{
    if( poDS->GetAccess() == GA_Update )
    {
        return CE_None;
    }

    if( poGeoRaster->GetBandBlock( nBand, nBlockXOff, nBlockYOff, pImage ) )
    {
        return CE_None;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error reading GeoRaster ofsett X (%d) offset Y (%d) band (%d)",
            nBlockXOff, nBlockYOff, nBand );

        return CE_Failure;
    }
}

//  ---------------------------------------------------------------------------
//                                                                IWriteBlock()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::IWriteBlock( int nBlockXOff, 
                                         int nBlockYOff, 
                                         void *pImage )
{
    if( poGeoRaster->SetBandBlock( nBand, nBlockXOff, nBlockYOff, pImage ) )
    {
        return CE_None;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error updating GeoRaster ofsett X (%d) offset Y (%d) band (%d)",
            nBlockXOff, nBlockYOff, nBand );

        return CE_Failure;
    }
}
//  ---------------------------------------------------------------------------
//                                                     GetColorInterpretation()
//  ---------------------------------------------------------------------------

GDALColorInterp GeoRasterRasterBand::GetColorInterpretation()
{
    GeoRasterDataset* poGDS = (GeoRasterDataset*) poDS;

    if( eDataType == GDT_Byte && poGDS->nBands > 2 )
    {
        if( nBand == poGeoRaster->iDefaultRedBand )
        {
            return GCI_RedBand;
        }
        else if ( nBand == poGeoRaster->iDefaultGreenBand )
        {
            return GCI_GreenBand;
        }
        else if ( nBand == poGeoRaster->iDefaultBlueBand )
        {
            return GCI_BlueBand;
        }
        else
        {
            return GCI_GrayIndex;
        }
    }

    if( poGeoRaster->HasColorTable( nBand ) )
    {
        return GCI_PaletteIndex;
    }
    else
    {
        return GCI_GrayIndex;
    }
}

//  ---------------------------------------------------------------------------
//                                                              GetColorTable()
//  ---------------------------------------------------------------------------

GDALColorTable *GeoRasterRasterBand::GetColorTable()
{
    poGeoRaster->GetColorTable( nBand, poColorTable );

    if( poColorTable->GetColorEntryCount() == 0 )
    {
        return NULL;
    }

    return poColorTable;
}

//  ---------------------------------------------------------------------------
//                                                              SetColorTable()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetColorTable( GDALColorTable *poInColorTable )
{
    if( poInColorTable == NULL )
    {
        return CE_None;
    }

    if( poInColorTable->GetColorEntryCount() == 0 )
    {
        return CE_None;
    }

    delete poColorTable;

    poColorTable = poInColorTable->Clone();

    poGeoRaster->SetColorTable( nBand, poColorTable );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                                 GetMinimum()
//  ---------------------------------------------------------------------------

double GeoRasterRasterBand::GetMinimum( int *pbSuccess )
{
    *pbSuccess = (int) bValidStats;

    return dfMin;
}

//  ---------------------------------------------------------------------------
//                                                                 GetMaximum()
//  ---------------------------------------------------------------------------

double GeoRasterRasterBand::GetMaximum( int *pbSuccess )
{
    *pbSuccess = (int) bValidStats;

    return dfMax;
}

//  ---------------------------------------------------------------------------
//                                                              GetStatistics()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::GetStatistics( int bApproxOK, int bForce,
                                           double *pdfMin, double *pdfMax,
                                           double *pdfMean, double *pdfStdDev )
{
    (void) bForce;
    (void) bApproxOK;

    if( ! bValidStats )
    {
        bValidStats = poGeoRaster->GetStatistics( nBand,
                                           dfMin, dfMax, dfMean, dfStdDev );
    }

    if( bValidStats )
    {
        *pdfMin     = dfMin;
        *pdfMax     = dfMax;
        *pdfMean    = dfMean;
        *pdfStdDev  = dfStdDev;

        return CE_None;
    }

    return CE_Failure;
}

//  ---------------------------------------------------------------------------
//                                                              SetStatistics()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetStatistics( double dfMin, double dfMax,
                                           double dfMean, double dfStdDev )
{
    dfMin       = dfMin;
    dfMax       = dfMax;
    dfMean      = dfMax;
    dfStdDev    = dfStdDev;
    bValidStats = true;

    poGeoRaster->SetStatistics( dfMin, dfMax, dfMean, dfStdDev, nBand );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                             GetNoDataValue()
//  ---------------------------------------------------------------------------

double GeoRasterRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
    {
        *pbSuccess = (int) poGeoRaster->GetNoData( &dfNoData );
    }

    return dfNoData;
}

//  ---------------------------------------------------------------------------
//                                                             SetNoDataValue()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetNoDataValue( double dfNoDataValue )
{
    poGeoRaster->SetNoData( dfNoDataValue );

    return CE_None;
}
