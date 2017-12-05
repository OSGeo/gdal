/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTSourcedRasterBand
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gdal_vrt.h"
#include "vrtdataset.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_geometry.h"

CPL_CVSID("$Id$")

/*! @cond Doxygen_Suppress */

/************************************************************************/
/* ==================================================================== */
/*                          VRTSourcedRasterBand                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand( GDALDataset *poDSIn, int nBandIn ) :
    m_nRecursionCounter(0),
    m_papszSourceList(NULL),
    nSources(0),
    papoSources(NULL),
    bSkipBufferInitialization(FALSE)
{
    VRTRasterBand::Initialize( poDSIn->GetRasterXSize(),
                               poDSIn->GetRasterYSize() );

    poDS = poDSIn;
    nBand = nBandIn;
}

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand( GDALDataType eType,
                                            int nXSize, int nYSize ) :
    m_nRecursionCounter(0),
    m_papszSourceList(NULL),
    nSources(0),
    papoSources(NULL),
    bSkipBufferInitialization(FALSE)
{
    VRTRasterBand::Initialize( nXSize, nYSize );

    eDataType = eType;
}

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand( GDALDataset *poDSIn, int nBandIn,
                                            GDALDataType eType,
                                            int nXSize, int nYSize ) :
    m_nRecursionCounter(0),
    m_papszSourceList(NULL),
    nSources(0),
    papoSources(NULL),
    bSkipBufferInitialization(FALSE)
{
    VRTRasterBand::Initialize( nXSize, nYSize );

    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eType;
}

/************************************************************************/
/*                       ~VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::~VRTSourcedRasterBand()

{
    CloseDependentDatasets();
    CSLDestroy(m_papszSourceList);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr VRTSourcedRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                        int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        void *pData,
                                        int nBufXSize, int nBufYSize,
                                        GDALDataType eBufType,
                                        GSpacing nPixelSpace,
                                        GSpacing nLineSpace,
                                        GDALRasterIOExtraArg *psExtraArg )

{
    if( eRWFlag == GF_Write )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Writing through VRTSourcedRasterBand is not supported." );
        return CE_Failure;
    }

    // When using GDALProxyPoolDataset for sources, the recursion will not be
    // detected at VRT opening but when doing RasterIO. As the proxy pool will
    // return the already opened dataset, we can just test a member variable.
    // We allow 1, since IRasterIO() can be called from ComputeStatistics(),
    // which itself increments the recursion counter.
    if( m_nRecursionCounter > 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRTSourcedRasterBand::IRasterIO() called "
                  "recursively on the same band. "
                  "It looks like the VRT is referencing itself." );
        return CE_Failure;
    }

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 )
    {
        if( OverviewRasterIO(
               eRWFlag, nXOff, nYOff, nXSize, nYSize,
               pData, nBufXSize, nBufYSize,
               eBufType, nPixelSpace, nLineSpace, psExtraArg ) == CE_None )
            return CE_None;
    }

    // If resampling with non-nearest neighbour, we need to be careful
    // if the VRT band exposes a nodata value, but the sources do not have it
    if( eRWFlag == GF_Read &&
        (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour &&
        m_bNoDataValueSet )
    {
        for( int i = 0; i < nSources; i++ )
        {
            bool bFallbackToBase = false;
            if( !papoSources[i]->IsSimpleSource() )
            {
                bFallbackToBase = true;
            }
            else
            {
                VRTSimpleSource* const poSource
                    = reinterpret_cast<VRTSimpleSource *>( papoSources[i] );
                // The window we will actually request from the source raster band.
                double dfReqXOff = 0.0;
                double dfReqYOff = 0.0;
                double dfReqXSize = 0.0;
                double dfReqYSize = 0.0;
                int nReqXOff = 0;
                int nReqYOff = 0;
                int nReqXSize = 0;
                int nReqYSize = 0;

                // The window we will actual set _within_ the pData buffer.
                int nOutXOff = 0;
                int nOutYOff = 0;
                int nOutXSize = 0;
                int nOutYSize = 0;

                if( !poSource->GetSrcDstWindow( nXOff, nYOff, nXSize, nYSize,
                                      nBufXSize, nBufYSize,
                                      &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                                      &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                                      &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) )
                {
                    continue;
                }
                int bSrcHasNoData = FALSE;
                const double dfSrcNoData =
                    poSource->GetBand()->GetNoDataValue(&bSrcHasNoData);
                if( !bSrcHasNoData || dfSrcNoData != m_dfNoDataValue )
                    bFallbackToBase = true;
            }
            if( bFallbackToBase )
            {
                return GDALRasterBand::IRasterIO( eRWFlag,
                                                  nXOff, nYOff, nXSize, nYSize,
                                                  pData, nBufXSize, nBufYSize,
                                                  eBufType,
                                                  nPixelSpace,
                                                  nLineSpace,
                                                  psExtraArg );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize the buffer to some background value. Use the         */
/*      nodata value if available.                                      */
/* -------------------------------------------------------------------- */
    if( bSkipBufferInitialization )
    {
        // Do nothing
    }
    else if( nPixelSpace == GDALGetDataTypeSizeBytes(eBufType) &&
         (!m_bNoDataValueSet || m_dfNoDataValue == 0.0) )
    {
        if( nLineSpace == nBufXSize * nPixelSpace )
        {
             memset( pData, 0, static_cast<size_t>(nBufYSize * nLineSpace) );
        }
        else
        {
            for( int iLine = 0; iLine < nBufYSize; iLine++ )
            {
                memset( reinterpret_cast<GByte *>( pData )
                        + static_cast<GIntBig>(iLine) * nLineSpace,
                        0,
                        static_cast<size_t>(nBufXSize * nPixelSpace) );
            }
        }
    }
    else
    {
        double dfWriteValue = 0.0;
        if( m_bNoDataValueSet )
            dfWriteValue = m_dfNoDataValue;

        for( int iLine = 0; iLine < nBufYSize; iLine++ )
        {
            GDALCopyWords( &dfWriteValue, GDT_Float64, 0,
                           reinterpret_cast<GByte *>( pData )
                           + static_cast<GIntBig>( nLineSpace ) * iLine,
                           eBufType, static_cast<int>(nPixelSpace), nBufXSize );
        }
    }

    m_nRecursionCounter++;

    GDALProgressFunc const pfnProgressGlobal = psExtraArg->pfnProgress;
    void * const pProgressDataGlobal = psExtraArg->pProgressData;

/* -------------------------------------------------------------------- */
/*      Overlay each source in turn over top this.                      */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    for( int iSource = 0; eErr == CE_None && iSource < nSources; iSource++ )
    {
        psExtraArg->pfnProgress = GDALScaledProgress;
        psExtraArg->pProgressData =
            GDALCreateScaledProgress( 1.0 * iSource / nSources,
                                      1.0 * (iSource + 1) / nSources,
                                      pfnProgressGlobal,
                                      pProgressDataGlobal );
        if( psExtraArg->pProgressData == NULL )
            psExtraArg->pfnProgress = NULL;

        eErr =
            papoSources[iSource]->RasterIO( nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize,
                                            eBufType, nPixelSpace, nLineSpace,
                                            psExtraArg);

        GDALDestroyScaledProgress( psExtraArg->pProgressData );
    }

    psExtraArg->pfnProgress = pfnProgressGlobal;
    psExtraArg->pProgressData = pProgressDataGlobal;

    m_nRecursionCounter--;

    return eErr;
}

/************************************************************************/
/*                         IGetDataCoverageStatus()                     */
/************************************************************************/

#ifndef HAVE_GEOS
int  VRTSourcedRasterBand::IGetDataCoverageStatus( int /* nXOff */,
                                                   int /* nYOff */,
                                                   int /* nXSize */,
                                                   int /* nYSize */,
                                                   int /* nMaskFlagStop */,
                                                   double* pdfDataPct)
{
    // TODO(rouault): Should this set pdfDataPct?
    if( pdfDataPct != NULL )
        *pdfDataPct = -1.0;
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED | GDAL_DATA_COVERAGE_STATUS_DATA;
}
#else
int  VRTSourcedRasterBand::IGetDataCoverageStatus( int nXOff,
                                                   int nYOff,
                                                   int nXSize,
                                                   int nYSize,
                                                   int nMaskFlagStop,
                                                   double* pdfDataPct)
{
    if( pdfDataPct != NULL )
        *pdfDataPct = -1.0;
    int nStatus = 0;

    OGRPolygon* poPolyNonCoveredBySources = new OGRPolygon();
    OGRLinearRing* poLR = new OGRLinearRing();
    poLR->addPoint( nXOff, nYOff );
    poLR->addPoint( nXOff, nYOff + nYSize );
    poLR->addPoint( nXOff + nXSize, nYOff + nYSize );
    poLR->addPoint( nXOff + nXSize, nYOff );
    poLR->addPoint( nXOff, nYOff );
    poPolyNonCoveredBySources->addRingDirectly(poLR);

    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        if( !papoSources[iSource]->IsSimpleSource() )
        {
            delete poPolyNonCoveredBySources;
            return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
                   GDAL_DATA_COVERAGE_STATUS_DATA;
        }
        VRTSimpleSource* poSS = reinterpret_cast<VRTSimpleSource*>(papoSources[iSource]);
        // Check if the AOI is fully inside the source
        if( nXOff >= poSS->m_dfDstXOff &&
            nYOff >= poSS->m_dfDstYOff &&
            nXOff + nXSize <= poSS->m_dfDstXOff + poSS->m_dfDstXSize &&
            nYOff + nYSize <= poSS->m_dfDstYOff + poSS->m_dfDstYSize )
        {
            if( pdfDataPct )
                *pdfDataPct = 100.0;
            delete poPolyNonCoveredBySources;
            return GDAL_DATA_COVERAGE_STATUS_DATA;
        }
        // Check intersection of bounding boxes.
        if( poSS->m_dfDstXOff + poSS->m_dfDstXSize > nXOff &&
            poSS->m_dfDstYOff + poSS->m_dfDstYSize > nYOff &&
            poSS->m_dfDstXOff < nXOff + nXSize &&
            poSS->m_dfDstYOff < nYOff + nYSize )
        {
            nStatus |= GDAL_DATA_COVERAGE_STATUS_DATA;
            if( poPolyNonCoveredBySources != NULL )
            {
                OGRPolygon oPolySource;
                poLR = new OGRLinearRing();
                poLR->addPoint( poSS->m_dfDstXOff,
                                poSS->m_dfDstYOff );
                poLR->addPoint( poSS->m_dfDstXOff,
                                poSS->m_dfDstYOff + poSS->m_dfDstYSize );
                poLR->addPoint( poSS->m_dfDstXOff + poSS->m_dfDstXSize,
                                poSS->m_dfDstYOff + poSS->m_dfDstYSize );
                poLR->addPoint( poSS->m_dfDstXOff + poSS->m_dfDstXSize,
                                poSS->m_dfDstYOff );
                poLR->addPoint( poSS->m_dfDstXOff,
                                poSS->m_dfDstYOff );
                oPolySource.addRingDirectly(poLR);
                OGRGeometry* poRes = poPolyNonCoveredBySources->Difference(&oPolySource);
                if( poRes != NULL && poRes->IsEmpty() )
                {
                    delete poRes;
                    if( pdfDataPct )
                        *pdfDataPct = 100.0;
                    delete poPolyNonCoveredBySources;
                    return GDAL_DATA_COVERAGE_STATUS_DATA;
                }
                else if( poRes != NULL && poRes->getGeometryType() == wkbPolygon )
                {
                    delete poPolyNonCoveredBySources;
                    poPolyNonCoveredBySources = reinterpret_cast<OGRPolygon*>(poRes);
                }
                else
                {
                    delete poRes;
                    delete poPolyNonCoveredBySources;
                    poPolyNonCoveredBySources = NULL;
                }
            }
        }
        if( nMaskFlagStop != 0 && (nStatus & nMaskFlagStop) != 0 )
        {
            delete poPolyNonCoveredBySources;
            return nStatus;
        }
    }
    if( poPolyNonCoveredBySources != NULL )
    {
        if( !poPolyNonCoveredBySources->IsEmpty() )
            nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
        if( pdfDataPct != NULL )
            *pdfDataPct = 100.0 * (1.0 - poPolyNonCoveredBySources->get_Area() / nXSize / nYSize);
    }
    delete poPolyNonCoveredBySources;
    return nStatus;
}
#endif  // HAVE_GEOS

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTSourcedRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                         void * pImage )

{
    const int nPixelSize = GDALGetDataTypeSizeBytes(eDataType);

    int nReadXSize = 0;
    if( (nBlockXOff+1) * nBlockXSize > GetXSize() )
        nReadXSize = GetXSize() - nBlockXOff * nBlockXSize;
    else
        nReadXSize = nBlockXSize;

    int nReadYSize = 0;
    if( (nBlockYOff+1) * nBlockYSize > GetYSize() )
        nReadYSize = GetYSize() - nBlockYOff * nBlockYSize;
    else
        nReadYSize = nBlockYSize;

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO( GF_Read,
                      nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                      nReadXSize, nReadYSize,
                      pImage, nReadXSize, nReadYSize, eDataType,
                      nPixelSize, nPixelSize * nBlockXSize, &sExtraArg );
}

/************************************************************************/
/*                    CanUseSourcesMinMaxImplementations()              */
/************************************************************************/

bool VRTSourcedRasterBand::CanUseSourcesMinMaxImplementations()
{
    const char* pszUseSources =
        CPLGetConfigOption("VRT_MIN_MAX_FROM_SOURCES", NULL);
    if( pszUseSources )
        return CPLTestBool(pszUseSources);

    // Use heuristics to determine if we are going to use the source
    // GetMinimum() or GetMaximum() implementation: all the sources must be
    // "simple" sources with a dataset description that match a "regular" file
    // on the filesystem, whose open time and GetMinimum()/GetMaximum()
    // implementations we hope to be fast enough.
    // In case of doubt return FALSE.
    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        if( !(papoSources[iSource]->IsSimpleSource()) )
            return false;
        VRTSimpleSource * const poSimpleSource
            = reinterpret_cast<VRTSimpleSource *>( papoSources[iSource] );
        GDALRasterBand* poBand = poSimpleSource->GetBand();
        if( poBand == NULL )
            return false;
        if( poBand->GetDataset() == NULL )
            return false;
        const char* pszFilename = poBand->GetDataset()->GetDescription();
        if( pszFilename == NULL )
            return false;
        // /vsimem/ should be fast.
        if( STARTS_WITH(pszFilename, "/vsimem/") )
            continue;
        // but not other /vsi filesystems
        if( STARTS_WITH(pszFilename, "/vsi") )
            return false;
        char ch = '\0';
        // We will assume that filenames that are only with ascii characters
        // are real filenames and so we will not try to 'stat' them.
        for( int i = 0; (ch = pszFilename[i]) != '\0'; i++ )
        {
            if( !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == ':' ||
                  ch == '/' || ch == '\\' ||
                  ch == ' ' || ch == '.') )
                break;
        }
        if( ch != '\0' )
        {
            // Otherwise do a real filesystem check.
            VSIStatBuf sStat;
            if( VSIStat(pszFilename, &sStat) != 0 )
                return false;
        }
    }
    return true;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTSourcedRasterBand::GetMinimum( int *pbSuccess )
{
    if( !CanUseSourcesMinMaxImplementations() )
        return GDALRasterBand::GetMinimum(pbSuccess);

    const char * const pszValue = GetMetadataItem("STATISTICS_MINIMUM");
    if( pszValue != NULL )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;

        return CPLAtofM(pszValue);
    }

    if( m_nRecursionCounter > 0 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "VRTSourcedRasterBand::GetMinimum() called "
            "recursively on the same band. "
            "It looks like the VRT is referencing itself." );
        if( pbSuccess != NULL )
            *pbSuccess = FALSE;
        return 0.0;
    }
    m_nRecursionCounter ++;

    double dfMin = 0;
    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        int bSuccess = FALSE;
        double dfSourceMin
            = papoSources[iSource]->GetMinimum(GetXSize(), GetYSize(),
                                               &bSuccess);
        if( !bSuccess )
        {
            dfMin = GDALRasterBand::GetMinimum(pbSuccess);
            m_nRecursionCounter --;
            return dfMin;
        }

        if( iSource == 0 || dfSourceMin < dfMin )
            dfMin = dfSourceMin;
    }

    m_nRecursionCounter --;

    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return dfMin;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTSourcedRasterBand::GetMaximum( int *pbSuccess )
{
    if( !CanUseSourcesMinMaxImplementations() )
        return GDALRasterBand::GetMaximum(pbSuccess);

    const char * const pszValue = GetMetadataItem("STATISTICS_MAXIMUM");
    if( pszValue != NULL )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;

        return CPLAtofM(pszValue);
    }

    if( m_nRecursionCounter > 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRTSourcedRasterBand::GetMaximum() called "
                  "recursively on the same band. "
                  "It looks like the VRT is referencing itself." );
        if( pbSuccess != NULL )
            *pbSuccess = FALSE;
        return 0.0;
    }
    m_nRecursionCounter ++;

    double dfMax = 0;
    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        int bSuccess = FALSE;
        const double dfSourceMax =
            papoSources[iSource]->GetMaximum( GetXSize(), GetYSize(),
                                              &bSuccess );
        if( !bSuccess )
        {
            dfMax = GDALRasterBand::GetMaximum(pbSuccess);
            m_nRecursionCounter--;
            return dfMax;
        }

        if( iSource == 0 || dfSourceMax > dfMax )
            dfMax = dfSourceMax;
    }

    m_nRecursionCounter--;

    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return dfMax;
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr VRTSourcedRasterBand::ComputeRasterMinMax( int bApproxOK, double* adfMinMax )
{
/* -------------------------------------------------------------------- */
/*      Does the driver already know the min/max?                       */
/* -------------------------------------------------------------------- */
    if( bApproxOK )
    {
        int bSuccessMin = FALSE;
        int bSuccessMax = FALSE;

        const double dfMin = GetMinimum( &bSuccessMin );
        const double dfMax = GetMaximum( &bSuccessMax );

        if( bSuccessMin && bSuccessMax )
        {
            adfMinMax[0] = dfMin;
            adfMinMax[1] = dfMax;
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for min/max.                */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        GDALRasterBand * const poBand
            = GetRasterSampleOverview( GDALSTAT_APPROX_NUMSAMPLES );

        if( poBand != this )
            return poBand->ComputeRasterMinMax( TRUE, adfMinMax );
    }

/* -------------------------------------------------------------------- */
/*      Try with source bands.                                          */
/* -------------------------------------------------------------------- */
    if( m_nRecursionCounter > 0 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "VRTSourcedRasterBand::ComputeRasterMinMax() called "
            "recursively on the same band. "
            "It looks like the VRT is referencing itself." );
        return CE_Failure;
    }
    m_nRecursionCounter ++;

    adfMinMax[0] = 0.0;
    adfMinMax[1] = 0.0;
    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        double adfSourceMinMax[2] = { 0.0, 0.0 };
        const CPLErr eErr =
            papoSources[iSource]->ComputeRasterMinMax(
                GetXSize(), GetYSize(), bApproxOK, adfSourceMinMax );
        if( eErr != CE_None )
        {
            const CPLErr eErr2 =
                GDALRasterBand::ComputeRasterMinMax( bApproxOK, adfMinMax );
            m_nRecursionCounter --;
            return eErr2;
        }

        if( iSource == 0 || adfSourceMinMax[0] < adfMinMax[0] )
            adfMinMax[0] = adfSourceMinMax[0];
        if( iSource == 0 || adfSourceMinMax[1] > adfMinMax[1] )
            adfMinMax[1] = adfSourceMinMax[1];
    }

    m_nRecursionCounter--;

    return CE_None;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr
VRTSourcedRasterBand::ComputeStatistics( int bApproxOK,
                                         double *pdfMin, double *pdfMax,
                                         double *pdfMean, double *pdfStdDev,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData )

{
    if( nSources != 1 || m_bNoDataValueSet )
        return GDALRasterBand::ComputeStatistics(  bApproxOK,
                                              pdfMin, pdfMax,
                                              pdfMean, pdfStdDev,
                                              pfnProgress, pProgressData );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for statistics.             */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        GDALRasterBand * const poBand
            = GetRasterSampleOverview( GDALSTAT_APPROX_NUMSAMPLES );

        if( poBand != this )
            return poBand->ComputeStatistics( TRUE,
                                              pdfMin, pdfMax,
                                              pdfMean, pdfStdDev,
                                              pfnProgress, pProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Try with source bands.                                          */
/* -------------------------------------------------------------------- */
    if( m_nRecursionCounter > 0 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "VRTSourcedRasterBand::ComputeStatistics() called "
            "recursively on the same band. "
            "It looks like the VRT is referencing itself." );
        return CE_Failure;
    }
    m_nRecursionCounter ++;

    double dfMin = 0.0;
    double dfMax = 0.0;
    double dfMean = 0.0;
    double dfStdDev = 0.0;

    const CPLErr eErr =
        papoSources[0]->ComputeStatistics( GetXSize(), GetYSize(), bApproxOK,
                                           &dfMin, &dfMax,
                                           &dfMean, &dfStdDev,
                                           pfnProgress, pProgressData );
    if( eErr != CE_None )
    {
        const CPLErr eErr2 =
            GDALRasterBand::ComputeStatistics( bApproxOK,
                                               pdfMin, pdfMax,
                                               pdfMean, pdfStdDev,
                                               pfnProgress, pProgressData );
        m_nRecursionCounter --;
        return eErr2;
    }

    m_nRecursionCounter --;

    SetStatistics( dfMin, dfMax, dfMean, dfStdDev );

/* -------------------------------------------------------------------- */
/*      Record results.                                                 */
/* -------------------------------------------------------------------- */
    if( pdfMin != NULL )
        *pdfMin = dfMin;
    if( pdfMax != NULL )
        *pdfMax = dfMax;

    if( pdfMean != NULL )
        *pdfMean = dfMean;

    if( pdfStdDev != NULL )
        *pdfStdDev = dfStdDev;

    return CE_None;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTSourcedRasterBand::GetHistogram( double dfMin, double dfMax,
                                           int nBuckets, GUIntBig *panHistogram,
                                           int bIncludeOutOfRange, int bApproxOK,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData )

{
    if( nSources != 1 )
        return VRTRasterBand::GetHistogram( dfMin, dfMax,
                                             nBuckets, panHistogram,
                                             bIncludeOutOfRange, bApproxOK,
                                             pfnProgress, pProgressData );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      If we have overviews, use them for the histogram.               */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        // FIXME: Should we use the most reduced overview here or use some
        // minimum number of samples like GDALRasterBand::ComputeStatistics()
        // does?
        GDALRasterBand *poBestOverview = GetRasterSampleOverview( 0 );

        if( poBestOverview != this )
        {
            return poBestOverview->GetHistogram( dfMin, dfMax, nBuckets,
                                                 panHistogram,
                                                 bIncludeOutOfRange, bApproxOK,
                                                 pfnProgress, pProgressData );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try with source bands.                                          */
/* -------------------------------------------------------------------- */
    if( m_nRecursionCounter > 0 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "VRTSourcedRasterBand::GetHistogram() called recursively on the "
            "same band. It looks like the VRT is referencing itself." );
        return CE_Failure;
    }
    m_nRecursionCounter ++;

    const CPLErr eErr =
        papoSources[0]->GetHistogram( GetXSize(), GetYSize(), dfMin, dfMax,
                                      nBuckets,
                                      panHistogram,
                                      bIncludeOutOfRange, bApproxOK,
                                      pfnProgress, pProgressData );
    if( eErr != CE_None )
    {
        const CPLErr eErr2 =
            GDALRasterBand::GetHistogram( dfMin, dfMax,
                                          nBuckets, panHistogram,
                                          bIncludeOutOfRange, bApproxOK,
                                          pfnProgress, pProgressData );
        m_nRecursionCounter --;
        return eErr2;
    }

    m_nRecursionCounter--;

    SetDefaultHistogram( dfMin, dfMax, nBuckets, panHistogram );

    return CE_None;
}

/************************************************************************/
/*                             AddSource()                              */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddSource( VRTSource *poNewSource )

{
    nSources++;

    papoSources = static_cast<VRTSource **>(
        CPLRealloc( papoSources, sizeof(void*) * nSources ) );
    papoSources[nSources-1] = poNewSource;

    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    if( poNewSource->IsSimpleSource() )
    {
        VRTSimpleSource* poSS = reinterpret_cast<VRTSimpleSource*>( poNewSource );
        if( GetMetadataItem("NBITS", "IMAGE_STRUCTURE") != NULL)
        {
            int nBits = atoi(GetMetadataItem("NBITS", "IMAGE_STRUCTURE"));
            if( nBits >= 1 && nBits <= 31 )
            {
                poSS->SetMaxValue( static_cast<int>((1U << nBits) -1) );
            }
        }

        CheckSource( poSS );
    }

    return CE_None;
}

/*! @endcond */

/************************************************************************/
/*                              VRTAddSource()                          */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddSource().
 */

CPLErr CPL_STDCALL VRTAddSource( VRTSourcedRasterBandH hVRTBand,
                                 VRTSourceH hNewSource )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddSource", CE_Failure );

    return reinterpret_cast<VRTSourcedRasterBand *>( hVRTBand )->
        AddSource( reinterpret_cast<VRTSource *>( hNewSource ) );
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTSourcedRasterBand::XMLInit( CPLXMLNode * psTree,
                                      const char *pszVRTPath,
                                      void* pUniqueHandle )

{
    {
        const CPLErr eErr = VRTRasterBand::XMLInit( psTree, pszVRTPath,
                                                    pUniqueHandle );
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Process sources.                                                */
/* -------------------------------------------------------------------- */
    VRTDriver * const poDriver = reinterpret_cast<VRTDriver *>(
        GDALGetDriverByName( "VRT" ) );

    for( CPLXMLNode *psChild = psTree->psChild;
         psChild != NULL && poDriver != NULL;
         psChild = psChild->psNext)
    {
        if( psChild->eType != CXT_Element )
            continue;

        CPLErrorReset();
        VRTSource * const poSource =
            poDriver->ParseSource( psChild, pszVRTPath, pUniqueHandle );
        if( poSource != NULL )
            AddSource( poSource );
        else if( CPLGetLastErrorType() != CE_None )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Done.                                                           */
/* -------------------------------------------------------------------- */
    const char* pszSubclass = CPLGetXMLValue( psTree, "subclass",
                                              "VRTSourcedRasterBand" );
    if( nSources == 0 && !EQUAL(pszSubclass,"VRTDerivedRasterBand") )
        CPLDebug( "VRT", "No valid sources found for band in VRT file %s",
                  GetDataset() ? GetDataset()->GetDescription() : "" );

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTSourcedRasterBand::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode *psTree = VRTRasterBand::SerializeToXML( pszVRTPath );
    CPLXMLNode* psLastChild = psTree->psChild;
    while( psLastChild != NULL && psLastChild->psNext != NULL )
        psLastChild = psLastChild->psNext;

/* -------------------------------------------------------------------- */
/*      Process Sources.                                                */
/* -------------------------------------------------------------------- */
    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        CPLXMLNode * const psXMLSrc
            = papoSources[iSource]->SerializeToXML( pszVRTPath );

        if( psXMLSrc != NULL )
        {
            if( psLastChild == NULL )
                psTree->psChild = psXMLSrc;
            else
                psLastChild->psNext = psXMLSrc;
            psLastChild = psXMLSrc;
        }
    }

    return psTree;
}

/************************************************************************/
/*                            CheckSource()                             */
/************************************************************************/

void VRTSourcedRasterBand::CheckSource( VRTSimpleSource *poSS )
{
/* -------------------------------------------------------------------- */
/*      Check if we can avoid buffer initialization.                    */
/* -------------------------------------------------------------------- */

    // Note: if one day we do alpha compositing, we will need to check that.
    if( strcmp(poSS->GetType(), "SimpleSource") == 0 &&
        poSS->m_dfSrcXOff >= 0.0 &&
        poSS->m_dfSrcYOff >= 0.0 &&
        poSS->m_dfSrcXOff + poSS->m_dfSrcXSize <= poSS->m_poRasterBand->GetXSize() &&
        poSS->m_dfSrcYOff + poSS->m_dfSrcYSize <= poSS->m_poRasterBand->GetYSize() &&
        poSS->m_dfDstXOff <= 0.0 &&
        poSS->m_dfDstYOff <= 0.0 &&
        poSS->m_dfDstXOff + poSS->m_dfDstXSize >= nRasterXSize &&
        poSS->m_dfDstYOff + poSS->m_dfDstYSize >= nRasterYSize )
    {
        bSkipBufferInitialization = TRUE;
    }
}

/************************************************************************/
/*                          ConfigureSource()                           */
/************************************************************************/

void VRTSourcedRasterBand::ConfigureSource( VRTSimpleSource *poSimpleSource,
                                            GDALRasterBand *poSrcBand,
                                            int bAddAsMaskBand,
                                            double dfSrcXOff, double dfSrcYOff,
                                            double dfSrcXSize, double dfSrcYSize,
                                            double dfDstXOff, double dfDstYOff,
                                            double dfDstXSize,
                                            double dfDstYSize )
{
/* -------------------------------------------------------------------- */
/*      Default source and dest rectangles.                             */
/* -------------------------------------------------------------------- */
    if( dfSrcYSize == -1 )
    {
        dfSrcXOff = 0;
        dfSrcYOff = 0;
        dfSrcXSize = poSrcBand->GetXSize();
        dfSrcYSize = poSrcBand->GetYSize();
    }

    if( dfDstYSize == -1 )
    {
        dfDstXOff = 0;
        dfDstYOff = 0;
        dfDstXSize = nRasterXSize;
        dfDstYSize = nRasterYSize;
    }

    if( bAddAsMaskBand )
        poSimpleSource->SetSrcMaskBand( poSrcBand );
    else
        poSimpleSource->SetSrcBand( poSrcBand );

    poSimpleSource->SetSrcWindow( dfSrcXOff, dfSrcYOff,
                                  dfSrcXSize, dfSrcYSize );
    poSimpleSource->SetDstWindow( dfDstXOff, dfDstYOff,
                                  dfDstXSize, dfDstYSize );

    CheckSource( poSimpleSource );

/* -------------------------------------------------------------------- */
/*      If we can get the associated GDALDataset, add a reference to it.*/
/* -------------------------------------------------------------------- */
    if( poSrcBand->GetDataset() != NULL )
        poSrcBand->GetDataset()->Reference();
}

/************************************************************************/
/*                          AddSimpleSource()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddSimpleSource(
    GDALRasterBand *poSrcBand,
    double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize,
    double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize,
    const char *pszResampling,
    double dfNoDataValueIn )

{
/* -------------------------------------------------------------------- */
/*      Create source.                                                  */
/* -------------------------------------------------------------------- */
    VRTSimpleSource *poSimpleSource = NULL;

    if( pszResampling != NULL && STARTS_WITH_CI(pszResampling, "aver") )
        poSimpleSource = new VRTAveragedSource();
    else
    {
        poSimpleSource = new VRTSimpleSource();
        if( dfNoDataValueIn != VRT_NODATA_UNSET )
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "NODATA setting not currently supported for nearest  "
                "neighbour sampled simple sources on Virtual Datasources." );
    }

    ConfigureSource( poSimpleSource,
                     poSrcBand,
                     FALSE,
                     dfSrcXOff, dfSrcYOff,
                     dfSrcXSize, dfSrcYSize,
                     dfDstXOff, dfDstYOff,
                     dfDstXSize, dfDstYSize );

    if( dfNoDataValueIn != VRT_NODATA_UNSET )
        poSimpleSource->SetNoDataValue( dfNoDataValueIn );

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    return AddSource( poSimpleSource );
}

/************************************************************************/
/*                         AddMaskBandSource()                          */
/************************************************************************/

// poSrcBand is not the mask band, but the band from which the mask band is
// taken.
CPLErr VRTSourcedRasterBand::AddMaskBandSource(
    GDALRasterBand *poSrcBand,
    double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize,
    double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize )
{
/* -------------------------------------------------------------------- */
/*      Create source.                                                  */
/* -------------------------------------------------------------------- */
    VRTSimpleSource* poSimpleSource = new VRTSimpleSource();

    ConfigureSource(poSimpleSource,
                    poSrcBand,
                    TRUE,
                    dfSrcXOff, dfSrcYOff,
                    dfSrcXSize, dfSrcYSize,
                    dfDstXOff, dfDstYOff,
                    dfDstXSize, dfDstYSize);

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    return AddSource( poSimpleSource );
}

/*! @endcond */

/************************************************************************/
/*                         VRTAddSimpleSource()                         */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddSimpleSource().
 */

CPLErr CPL_STDCALL VRTAddSimpleSource( VRTSourcedRasterBandH hVRTBand,
                                       GDALRasterBandH hSrcBand,
                                       int nSrcXOff, int nSrcYOff,
                                       int nSrcXSize, int nSrcYSize,
                                       int nDstXOff, int nDstYOff,
                                       int nDstXSize, int nDstYSize,
                                       const char *pszResampling,
                                       double dfNoDataValue )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddSimpleSource", CE_Failure );

    return
        reinterpret_cast<VRTSourcedRasterBand *>( hVRTBand )->AddSimpleSource(
            reinterpret_cast<GDALRasterBand *>( hSrcBand ),
            nSrcXOff, nSrcYOff,
            nSrcXSize, nSrcYSize,
            nDstXOff, nDstYOff,
            nDstXSize, nDstYSize,
            pszResampling, dfNoDataValue );
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                          AddComplexSource()                          */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddComplexSource(
    GDALRasterBand *poSrcBand,
    double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize,
    double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize,
    double dfScaleOff,
    double dfScaleRatio,
    double dfNoDataValueIn,
    int nColorTableComponent )

{
/* -------------------------------------------------------------------- */
/*      Create source.                                                  */
/* -------------------------------------------------------------------- */
    VRTComplexSource * const poSource = new VRTComplexSource();

    ConfigureSource( poSource,
                     poSrcBand,
                     FALSE,
                     dfSrcXOff, dfSrcYOff,
                     dfSrcXSize, dfSrcYSize,
                     dfDstXOff, dfDstYOff,
                     dfDstXSize, dfDstYSize );

/* -------------------------------------------------------------------- */
/*      Set complex parameters.                                         */
/* -------------------------------------------------------------------- */
    if( dfNoDataValueIn != VRT_NODATA_UNSET )
        poSource->SetNoDataValue( dfNoDataValueIn );

    if( dfScaleOff != 0.0 || dfScaleRatio != 1.0 )
        poSource->SetLinearScaling(dfScaleOff, dfScaleRatio);

    poSource->SetColorTableComponent(nColorTableComponent);

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    return AddSource( poSource );
}

/*! @endcond */

/************************************************************************/
/*                         VRTAddComplexSource()                        */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddComplexSource().
 */

CPLErr CPL_STDCALL VRTAddComplexSource( VRTSourcedRasterBandH hVRTBand,
                                        GDALRasterBandH hSrcBand,
                                        int nSrcXOff, int nSrcYOff,
                                        int nSrcXSize, int nSrcYSize,
                                        int nDstXOff, int nDstYOff,
                                        int nDstXSize, int nDstYSize,
                                        double dfScaleOff,
                                        double dfScaleRatio,
                                        double dfNoDataValue )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddComplexSource", CE_Failure );

    return
        reinterpret_cast<VRTSourcedRasterBand *>( hVRTBand )->AddComplexSource(
            reinterpret_cast<GDALRasterBand *>( hSrcBand ),
            nSrcXOff, nSrcYOff,
            nSrcXSize, nSrcYSize,
            nDstXOff, nDstYOff,
            nDstXSize, nDstYSize,
            dfScaleOff, dfScaleRatio,
            dfNoDataValue );
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                           AddFuncSource()                            */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddFuncSource(
    VRTImageReadFunc pfnReadFunc, void *pCBData, double dfNoDataValueIn )

{
/* -------------------------------------------------------------------- */
/*      Create source.                                                  */
/* -------------------------------------------------------------------- */
    VRTFuncSource * const poFuncSource = new VRTFuncSource;

    poFuncSource->fNoDataValue = static_cast<float>( dfNoDataValueIn );
    poFuncSource->pfnReadFunc = pfnReadFunc;
    poFuncSource->pCBData = pCBData;
    poFuncSource->eType = GetRasterDataType();

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    return AddSource( poFuncSource );
}

/*! @endcond */

/************************************************************************/
/*                          VRTAddFuncSource()                          */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddFuncSource().
 */

CPLErr CPL_STDCALL VRTAddFuncSource( VRTSourcedRasterBandH hVRTBand,
                                     VRTImageReadFunc pfnReadFunc,
                                     void *pCBData, double dfNoDataValue )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddFuncSource", CE_Failure );

    return reinterpret_cast<VRTSourcedRasterBand *>( hVRTBand )->
        AddFuncSource( pfnReadFunc, pCBData, dfNoDataValue );
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **VRTSourcedRasterBand::GetMetadataDomainList()
{
    return
        CSLAddString( GDALRasterBand::GetMetadataDomainList(),
                      "LocationInfo" );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *VRTSourcedRasterBand::GetMetadataItem( const char * pszName,
                                                   const char * pszDomain )

{
/* ==================================================================== */
/*      LocationInfo handling.                                          */
/* ==================================================================== */
    if( pszDomain != NULL
        && EQUAL(pszDomain,"LocationInfo")
        && (STARTS_WITH_CI(pszName, "Pixel_") ||
            STARTS_WITH_CI(pszName, "GeoPixel_")) )
    {
/* -------------------------------------------------------------------- */
/*      What pixel are we aiming at?                                    */
/* -------------------------------------------------------------------- */
        int iPixel = 0;
        int iLine = 0;

        if( STARTS_WITH_CI(pszName, "Pixel_") )
        {
            // TODO(schwehr): Replace sscanf.
            if( sscanf( pszName+6, "%d_%d", &iPixel, &iLine ) != 2 )
                return NULL;
        }
        else if( STARTS_WITH_CI(pszName, "GeoPixel_") )
        {
            const double dfGeoX = CPLAtof(pszName + 9);
            const char * const pszUnderscore = strchr(pszName + 9, '_');
            if( !pszUnderscore )
                return NULL;
            const double dfGeoY = CPLAtof(pszUnderscore + 1);

            if( GetDataset() == NULL )
                return NULL;

            double adfGeoTransform[6] = { 0.0 };
            if( GetDataset()->GetGeoTransform( adfGeoTransform ) != CE_None )
                return NULL;

            double adfInvGeoTransform[6] = { 0.0 };
            if( !GDALInvGeoTransform( adfGeoTransform, adfInvGeoTransform ) )
                return NULL;

            iPixel = static_cast<int>( floor(
                adfInvGeoTransform[0]
                + adfInvGeoTransform[1] * dfGeoX
                + adfInvGeoTransform[2] * dfGeoY ) );
            iLine = static_cast<int>( floor(
                adfInvGeoTransform[3]
                + adfInvGeoTransform[4] * dfGeoX
                + adfInvGeoTransform[5] * dfGeoY ) );
        }
        else
        {
            return NULL;
        }

        if( iPixel < 0 || iLine < 0
            || iPixel >= GetXSize()
            || iLine >= GetYSize() )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Find the file(s) at this location.                              */
/* -------------------------------------------------------------------- */
        char **papszFileList = NULL;
        int nListSize = 0; // keep it in this scope
        int nListMaxSize = 0; // keep it in this scope
        CPLHashSet * const hSetFiles = CPLHashSetNew( CPLHashSetHashStr,
                                                      CPLHashSetEqualStr,
                                                      NULL );

        for( int iSource = 0; iSource < nSources; iSource++ )
        {
            if( !papoSources[iSource]->IsSimpleSource() )
                continue;

            VRTSimpleSource * const poSrc
                = reinterpret_cast<VRTSimpleSource *>( papoSources[iSource] );

            double dfReqXOff = 0.0;
            double dfReqYOff = 0.0;
            double dfReqXSize = 0.0;
            double dfReqYSize = 0.0;
            int nReqXOff = 0;
            int nReqYOff = 0;
            int nReqXSize = 0;
            int nReqYSize = 0;
            int nOutXOff = 0;
            int nOutYOff = 0;
            int nOutXSize = 0;
            int nOutYSize = 0;

            if( !poSrc->GetSrcDstWindow( iPixel, iLine, 1, 1, 1, 1,
                                         &dfReqXOff, &dfReqYOff,
                                         &dfReqXSize, &dfReqYSize,
                                         &nReqXOff, &nReqYOff,
                                         &nReqXSize, &nReqYSize,
                                         &nOutXOff, &nOutYOff,
                                         &nOutXSize, &nOutYSize ) )
                continue;

            poSrc->GetFileList( &papszFileList, &nListSize, &nListMaxSize,
                                hSetFiles );
        }

/* -------------------------------------------------------------------- */
/*      Format into XML.                                                */
/* -------------------------------------------------------------------- */
        m_osLastLocationInfo = "<LocationInfo>";
        for( int i = 0; i < nListSize; i++ )
        {
            m_osLastLocationInfo += "<File>";
            char * const pszXMLEscaped = CPLEscapeString( papszFileList[i], -1,
                                                          CPLES_XML );
            m_osLastLocationInfo += pszXMLEscaped;
            CPLFree(pszXMLEscaped);
            m_osLastLocationInfo += "</File>";
        }
        m_osLastLocationInfo += "</LocationInfo>";

        CSLDestroy( papszFileList );
        CPLHashSetDestroy( hSetFiles );

        return m_osLastLocationInfo.c_str();
    }

/* ==================================================================== */
/*      Other domains.                                                  */
/* ==================================================================== */

    return GDALRasterBand::GetMetadataItem( pszName, pszDomain );
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **VRTSourcedRasterBand::GetMetadata( const char *pszDomain )

{
/* ==================================================================== */
/*      vrt_sources domain handling.                                    */
/* ==================================================================== */
    if( pszDomain != NULL && EQUAL(pszDomain,"vrt_sources") )
    {
        CSLDestroy(m_papszSourceList);
        m_papszSourceList = NULL;

/* -------------------------------------------------------------------- */
/*      Process SimpleSources.                                          */
/* -------------------------------------------------------------------- */
        for( int iSource = 0; iSource < nSources; iSource++ )
        {
            CPLXMLNode * const psXMLSrc =
                papoSources[iSource]->SerializeToXML( NULL );
            if( psXMLSrc == NULL )
                continue;

            char * const pszXML = CPLSerializeXMLTree( psXMLSrc );

            m_papszSourceList =
                CSLSetNameValue( m_papszSourceList,
                                 CPLSPrintf( "source_%d", iSource ), pszXML );
            CPLFree( pszXML );
            CPLDestroyXMLNode( psXMLSrc );
        }

        return m_papszSourceList;
    }

/* ==================================================================== */
/*      Other domains.                                                  */
/* ==================================================================== */

    return GDALRasterBand::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::SetMetadataItem( const char *pszName,
                                              const char *pszValue,
                                              const char *pszDomain )

{
#if DEBUG_VERBOSE
    CPLDebug( "VRT", "VRTSourcedRasterBand::SetMetadataItem(%s,%s,%s)\n",
              pszName, pszValue, pszDomain );
#endif

    if( pszDomain != NULL
        && EQUAL(pszDomain,"new_vrt_sources") )
    {
        VRTDriver * const poDriver = reinterpret_cast<VRTDriver *>(
            GDALGetDriverByName( "VRT" ) );

        CPLXMLNode * const psTree = CPLParseXMLString( pszValue );
        if( psTree == NULL )
            return CE_Failure;

        VRTSource * const poSource = poDriver->ParseSource( psTree, NULL,
                                                            GetDataset() );
        CPLDestroyXMLNode( psTree );

        if( poSource != NULL )
            return AddSource( poSource );

        return CE_Failure;
    }
    else if( pszDomain != NULL
        && EQUAL(pszDomain,"vrt_sources") )
    {
        int iSource = 0;
        // TODO(schwehr): Replace sscanf.
        if( sscanf(pszName, "source_%d", &iSource) != 1 || iSource < 0 ||
            iSource >= nSources )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s metadata item name is not recognized. "
                      "Should be between source_0 and source_%d",
                      pszName, nSources - 1 );
            return CE_Failure;
        }

        VRTDriver * const poDriver = reinterpret_cast<VRTDriver *>(
            GDALGetDriverByName( "VRT" ) );

        CPLXMLNode * const psTree = CPLParseXMLString( pszValue );
        if( psTree == NULL )
            return CE_Failure;

        VRTSource * const poSource = poDriver->ParseSource( psTree, NULL,
                                                            GetDataset() );
        CPLDestroyXMLNode( psTree );

        if( poSource != NULL )
        {
            delete papoSources[iSource];
            papoSources[iSource] = poSource;
            reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();
            return CE_None;
        }

        return CE_Failure;
    }

    return VRTRasterBand::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTSourcedRasterBand::SetMetadata( char **papszNewMD, const char *pszDomain )

{
    if( pszDomain != NULL
        && (EQUAL(pszDomain,"new_vrt_sources")
            || EQUAL(pszDomain,"vrt_sources")) )
    {
        VRTDriver * const poDriver
            = reinterpret_cast<VRTDriver *>( GDALGetDriverByName( "VRT" ) );

        if( EQUAL(pszDomain,"vrt_sources") )
        {
            for( int i = 0; i < nSources; i++ )
                delete papoSources[i];
            CPLFree( papoSources );
            papoSources = NULL;
            nSources = 0;
        }

        for( int i = 0; i < CSLCount(papszNewMD); i++ )
        {
            const char * const pszXML =
                CPLParseNameValue( papszNewMD[i], NULL );

            CPLXMLNode * const psTree = CPLParseXMLString( pszXML );
            if( psTree == NULL )
                return CE_Failure;

            VRTSource * const poSource = poDriver->ParseSource( psTree, NULL,
                                                                GetDataset() );
            CPLDestroyXMLNode( psTree );

            if( poSource == NULL )
                return CE_Failure;

            const CPLErr eErr = AddSource( poSource );
            if( eErr != CE_None )
                return eErr;
        }

        return CE_None;
    }

    return VRTRasterBand::SetMetadata( papszNewMD, pszDomain );
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTSourcedRasterBand::GetFileList( char*** ppapszFileList, int *pnSize,
                                        int *pnMaxSize, CPLHashSet* hSetFiles )
{
    for( int i = 0; i < nSources; i++ )
    {
        papoSources[i]->GetFileList( ppapszFileList, pnSize,
                                     pnMaxSize, hSetFiles );
    }

    VRTRasterBand::GetFileList( ppapszFileList, pnSize,
                                pnMaxSize, hSetFiles );
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTSourcedRasterBand::CloseDependentDatasets()
{
    if( nSources == 0 )
        return FALSE;

    for( int i = 0; i < nSources; i++ )
        delete papoSources[i];

    CPLFree( papoSources );
    papoSources = NULL;
    nSources = 0;

    return TRUE;
}

/************************************************************************/
/*                               FlushCache()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::FlushCache()
{
    CPLErr eErr = VRTRasterBand::FlushCache();
    for( int i = 0; i < nSources && eErr == CE_None; i++ )
    {
        eErr = papoSources[i]->FlushCache();
    }
    return eErr;
}

/*! @endcond */
