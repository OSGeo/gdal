/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTWarpedRasterBand *and VRTWarpedDataset.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
#include "vrtdataset.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "gdalwarper.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      GDALAutoCreateWarpedVRT()                       */
/************************************************************************/

/**
 * Create virtual warped dataset automatically.
 *
 * This function will create a warped virtual file representing the
 * input image warped into the target coordinate system.  A GenImgProj
 * transformation is created to accomplish any required GCP/Geotransform
 * warp and reprojection to the target coordinate system.  The output virtual
 * dataset will be "northup" in the target coordinate system.   The
 * GDALSuggestedWarpOutput() function is used to determine the bounds and
 * resolution of the output virtual file which should be large enough to
 * include all the input image
 *
 * Note that the constructed GDALDatasetH will acquire one or more references
 * to the passed in hSrcDS.  Reference counting semantics on the source
 * dataset should be honoured.  That is, don't just GDALClose() it unless it
 * was opened with GDALOpenShared().
 *
 * The returned dataset will have no associated filename for itself.  If you
 * want to write the virtual dataset description to a file, use the
 * GDALSetDescription() function (or SetDescription() method) on the dataset
 * to assign a filename before it is closed.
 *
 * @param hSrcDS The source dataset.
 *
 * @param pszSrcWKT The coordinate system of the source image.  If NULL, it
 * will be read from the source image.
 *
 * @param pszDstWKT The coordinate system to convert to.  If NULL no change
 * of coordinate system will take place.
 *
 * @param eResampleAlg One of GRA_NearestNeighbour, GRA_Bilinear, GRA_Cubic,
 * GRA_CubicSpline, GRA_Lanczos, GRA_Average or GRA_Mode.
 * Controls the sampling method used.
 *
 * @param dfMaxError Maximum error measured in input pixels that is allowed in
 * approximating the transformation (0.0 for exact calculations).
 *
 * @param psOptionsIn Additional warp options, normally NULL.
 *
 * @return NULL on failure, or a new virtual dataset handle on success.
 */

GDALDatasetH CPL_STDCALL
GDALAutoCreateWarpedVRT( GDALDatasetH hSrcDS,
                         const char *pszSrcWKT,
                         const char *pszDstWKT,
                         GDALResampleAlg eResampleAlg,
                         double dfMaxError,
                         const GDALWarpOptions *psOptionsIn )

{
    VALIDATE_POINTER1( hSrcDS, "GDALAutoCreateWarpedVRT", NULL );

/* -------------------------------------------------------------------- */
/*      Populate the warp options.                                      */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = NULL;
    if( psOptionsIn != NULL )
        psWO = GDALCloneWarpOptions( psOptionsIn );
    else
        psWO = GDALCreateWarpOptions();

    psWO->eResampleAlg = eResampleAlg;

    psWO->hSrcDS = hSrcDS;

    GDALWarpInitDefaultBandMapping( psWO, GDALGetRasterCount( hSrcDS ) );

/* -------------------------------------------------------------------- */
/*      Setup no data values                                            */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < psWO->nBandCount; i++ )
    {
        GDALRasterBandH rasterBand = GDALGetRasterBand(psWO->hSrcDS, psWO->panSrcBands[i]);
        
        int hasNoDataValue;
        double noDataValue = GDALGetRasterNoDataValue(rasterBand, &hasNoDataValue);

        if( hasNoDataValue )
        {
            GDALWarpInitNoDataReal(psWO, -1e10);
            
            psWO->padfSrcNoDataReal[i] = noDataValue;
            psWO->padfDstNoDataReal[i] = noDataValue;
        }
    }

    if( psWO->padfDstNoDataReal != NULL )
    {
        if (CSLFetchNameValue( psWO->papszWarpOptions, "INIT_DEST" ) == NULL)
        {
            psWO->papszWarpOptions =
                CSLSetNameValue(psWO->papszWarpOptions, "INIT_DEST", "NO_DATA");
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the transformer.                                         */
/* -------------------------------------------------------------------- */
    psWO->pfnTransformer = GDALGenImgProjTransform;
    psWO->pTransformerArg =
        GDALCreateGenImgProjTransformer( psWO->hSrcDS, pszSrcWKT,
                                         NULL, pszDstWKT,
                                         TRUE, 1.0, 0 );

    if( psWO->pTransformerArg == NULL )
    {
        GDALDestroyWarpOptions( psWO );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Figure out the desired output bounds and resolution.            */
/* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6] = { 0.0 };
    int nDstPixels = 0;
    int nDstLines = 0;
    CPLErr eErr =
        GDALSuggestedWarpOutput( hSrcDS, psWO->pfnTransformer,
                                 psWO->pTransformerArg,
                                 adfDstGeoTransform, &nDstPixels, &nDstLines );
    if( eErr != CE_None )
    {
        GDALDestroyTransformer( psWO->pTransformerArg );
        GDALDestroyWarpOptions( psWO );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Update the transformer to include an output geotransform        */
/*      back to pixel/line coordinates.                                 */
/*                                                                      */
/* -------------------------------------------------------------------- */
    GDALSetGenImgProjTransformerDstGeoTransform(
        psWO->pTransformerArg, adfDstGeoTransform );

/* -------------------------------------------------------------------- */
/*      Do we want to apply an approximating transformation?            */
/* -------------------------------------------------------------------- */
    if( dfMaxError > 0.0 )
    {
        psWO->pTransformerArg =
            GDALCreateApproxTransformer( psWO->pfnTransformer,
                                         psWO->pTransformerArg,
                                         dfMaxError );
        psWO->pfnTransformer = GDALApproxTransform;
        GDALApproxTransformerOwnsSubtransformer(psWO->pTransformerArg, TRUE);
    }

/* -------------------------------------------------------------------- */
/*      Create the VRT file.                                            */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS
        = GDALCreateWarpedVRT( hSrcDS, nDstPixels, nDstLines,
                               adfDstGeoTransform, psWO );

    GDALDestroyWarpOptions( psWO );

    if( pszDstWKT != NULL )
        GDALSetProjection( hDstDS, pszDstWKT );
    else if( pszSrcWKT != NULL )
        GDALSetProjection( hDstDS, pszSrcWKT );
    else if( GDALGetGCPCount( hSrcDS ) > 0 )
        GDALSetProjection( hDstDS, GDALGetGCPProjection( hSrcDS ) );
    else
        GDALSetProjection( hDstDS, GDALGetProjectionRef( hSrcDS ) );

    return hDstDS;
}

/************************************************************************/
/*                        GDALCreateWarpedVRT()                         */
/************************************************************************/

/**
 * Create virtual warped dataset.
 *
 * This function will create a warped virtual file representing the
 * input image warped based on a provided transformation.  Output bounds
 * and resolution are provided explicitly.
 *
 * Note that the constructed GDALDatasetH will acquire one or more references
 * to the passed in hSrcDS.  Reference counting semantics on the source
 * dataset should be honoured.  That is, don't just GDALClose() it unless it
 * was opened with GDALOpenShared().
 *
 * @param hSrcDS The source dataset.
 *
 * @param nPixels Width of the virtual warped dataset to create
 *
 * @param nLines Height of the virtual warped dataset to create
 *
 * @param padfGeoTransform Geotransform matrix of the virtual warped dataset
 * to create
 *
 * @param psOptions Warp options. Must be different from NULL.
 *
 * @return NULL on failure, or a new virtual dataset handle on success.
 */

GDALDatasetH CPL_STDCALL
GDALCreateWarpedVRT( GDALDatasetH hSrcDS,
                     int nPixels, int nLines, double *padfGeoTransform,
                     GDALWarpOptions *psOptions )

{
    VALIDATE_POINTER1( hSrcDS, "GDALCreateWarpedVRT", NULL );
    VALIDATE_POINTER1( psOptions, "GDALCreateWarpedVRT", NULL );

/* -------------------------------------------------------------------- */
/*      Create the VRTDataset and populate it with bands.               */
/* -------------------------------------------------------------------- */
    VRTWarpedDataset *poDS = new VRTWarpedDataset( nPixels, nLines );

    psOptions->hDstDS = poDS;
    poDS->SetGeoTransform( padfGeoTransform );

    GDALWarpResolveWorkingDataType(psOptions);

    for( int i = 0; i < psOptions->nBandCount; i++ )
    {
        int nDstBand = psOptions->panDstBands[i];
        while( poDS->GetRasterCount() < nDstBand )
        {   
            poDS->AddBand( psOptions->eWorkingDataType, NULL );
        }

        VRTWarpedRasterBand *poBand = static_cast<VRTWarpedRasterBand *>(
            poDS->GetRasterBand( nDstBand ) );
        GDALRasterBand *poSrcBand = static_cast<GDALRasterBand *>(
            GDALGetRasterBand( hSrcDS, psOptions->panSrcBands[i] ) );

        poBand->CopyCommonInfoFrom( poSrcBand );
    }

    while( poDS->GetRasterCount() < psOptions->nDstAlphaBand )
    {
        poDS->AddBand( psOptions->eWorkingDataType, NULL );
    }
    
/* -------------------------------------------------------------------- */
/*      Initialize the warp on the VRTWarpedDataset.                    */
/* -------------------------------------------------------------------- */
    const CPLErr eErr = poDS->Initialize( psOptions );
    if( eErr == CE_Failure )
    {
         psOptions->hDstDS = NULL;
         delete poDS;
         return NULL;
    }

    return poDS;
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/* ==================================================================== */
/*                          VRTWarpedDataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTWarpedDataset()                          */
/************************************************************************/

VRTWarpedDataset::VRTWarpedDataset( int nXSize, int nYSize ) :
    VRTDataset( nXSize, nYSize ),
    m_nBlockXSize(std::min( nXSize, 512 )),
    m_nBlockYSize(std::min( nYSize, 128 )),
    m_poWarper(NULL),
    m_nOverviewCount(0),
    m_papoOverviews(NULL),
    m_nSrcOvrLevel(-2)
{
    eAccess = GA_Update;
    DisableReadWriteMutex();
}

/************************************************************************/
/*                         ~VRTWarpedDataset()                          */
/************************************************************************/

VRTWarpedDataset::~VRTWarpedDataset()

{
    CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTWarpedDataset::CloseDependentDatasets()
{
    FlushCache();

    bool bHasDroppedRef = CPL_TO_BOOL( VRTDataset::CloseDependentDatasets() );

/* -------------------------------------------------------------------- */
/*      Cleanup overviews.                                              */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0; iOverview < m_nOverviewCount; iOverview++ )
    {
        GDALDatasetH hDS = m_papoOverviews[iOverview];

        if( GDALReleaseDataset( hDS ) )
        {
            bHasDroppedRef = true;
        }
    }

    CPLFree( m_papoOverviews );
    m_nOverviewCount = 0;
    m_papoOverviews = NULL;

/* -------------------------------------------------------------------- */
/*      Cleanup warper if one is in effect.                             */
/* -------------------------------------------------------------------- */
    if( m_poWarper != NULL )
    {
        const GDALWarpOptions *psWO = m_poWarper->GetOptions();

/* -------------------------------------------------------------------- */
/*      We take care to only call GDALClose() on psWO->hSrcDS if the    */
/*      reference count drops to zero.  This is makes it so that we     */
/*      can operate reference counting semantics more-or-less           */
/*      properly even if the dataset isn't open in shared mode,         */
/*      though we require that the caller also honour the reference     */
/*      counting semantics even though it isn't a shared dataset.       */
/* -------------------------------------------------------------------- */
        if( psWO != NULL && psWO->hSrcDS != NULL )
        {
            if( GDALReleaseDataset( psWO->hSrcDS ) )
            {
                bHasDroppedRef = true;
            }
        }

/* -------------------------------------------------------------------- */
/*      We are responsible for cleaning up the transformer ourselves.   */
/* -------------------------------------------------------------------- */
        if( psWO != NULL && psWO->pTransformerArg != NULL )
            GDALDestroyTransformer( psWO->pTransformerArg );

        delete m_poWarper;
        m_poWarper = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Destroy the raster bands if they exist.                         */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                         SetSrcOverviewLevel()                        */
/************************************************************************/

CPLErr VRTWarpedDataset::SetMetadataItem( const char *pszName, const char *pszValue,
                                          const char *pszDomain )

{
    if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        EQUAL(pszName, "SrcOvrLevel") )
    {
        const int nOldValue = m_nSrcOvrLevel;
        if( pszValue == NULL || EQUAL(pszValue, "AUTO") )
            m_nSrcOvrLevel = -2;
        else if( STARTS_WITH_CI(pszValue, "AUTO-") )
            m_nSrcOvrLevel = -2-atoi(pszValue + 5);
        else if( EQUAL(pszValue, "NONE") )
            m_nSrcOvrLevel = -1;
        else if( CPLGetValueType(pszValue) == CPL_VALUE_INTEGER )
            m_nSrcOvrLevel = atoi(pszValue);
        if( m_nSrcOvrLevel != nOldValue )
            SetNeedsFlush();
        return CE_None;
    }
    return VRTDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Initialize a dataset from passed in warp options.               */
/************************************************************************/

CPLErr VRTWarpedDataset::Initialize( void *psWO )

{
    if( m_poWarper != NULL )
        delete m_poWarper;

    m_poWarper = new GDALWarpOperation();

    GDALWarpOptions* psWO_Dup
        = GDALCloneWarpOptions(static_cast<GDALWarpOptions *>( psWO ) );

    /* Avoid errors when adding an alpha band, but source dataset has */
    /* no alpha band (#4571), and generally don't leave our buffer uninitialized */
    if (CSLFetchNameValue( psWO_Dup->papszWarpOptions, "INIT_DEST" ) == NULL)
        psWO_Dup->papszWarpOptions =
            CSLSetNameValue(psWO_Dup->papszWarpOptions, "INIT_DEST", "0");

    CPLErr eErr = m_poWarper->Initialize( psWO_Dup );

    // The act of initializing this warped dataset with this warp options
    // will result in our assuming ownership of a reference to the
    // hSrcDS.

    if( eErr == CE_None &&
        static_cast<GDALWarpOptions *>( psWO )->hSrcDS != NULL )
    {
        GDALReferenceDataset( psWO_Dup->hSrcDS );
    }

    GDALDestroyWarpOptions(psWO_Dup);

    return eErr;
}

/************************************************************************/
/*                        CreateImplicitOverviews()                     */
/*                                                                      */
/*      For each overview of the source dataset, create an overview     */
/*      in the warped VRT dataset.                                      */
/************************************************************************/

void VRTWarpedDataset::CreateImplicitOverviews()
{
    if( m_poWarper == NULL || m_nOverviewCount != 0 )
        return;

    const GDALWarpOptions *psWO = m_poWarper->GetOptions();

    if( psWO->hSrcDS == NULL || GDALGetRasterCount(psWO->hSrcDS) == 0 )
        return;

    GDALDataset* poSrcDS = static_cast<GDALDataset *>( psWO->hSrcDS );
    const int nOvrCount = poSrcDS->GetRasterBand(1)->GetOverviewCount();
    for( int iOvr = 0; iOvr < nOvrCount; iOvr++ )
    {
        GDALDataset* poSrcOvrDS = poSrcDS;
        if( m_nSrcOvrLevel < -2 )
        {
            if( iOvr + m_nSrcOvrLevel + 2 >= 0 )
            {
                poSrcOvrDS =
                    GDALCreateOverviewDataset( poSrcDS,
                                               iOvr + m_nSrcOvrLevel + 2,
                                               FALSE );
            }
        }
        else if( m_nSrcOvrLevel == -2 )
        {
            poSrcOvrDS = GDALCreateOverviewDataset(poSrcDS, iOvr, FALSE);
        }
        else if( m_nSrcOvrLevel >= 0 )
        {
            poSrcOvrDS = GDALCreateOverviewDataset( poSrcDS, m_nSrcOvrLevel,
                                                    TRUE );
        }
        if( poSrcOvrDS == NULL )
            break;
        if( poSrcOvrDS == poSrcDS )
            poSrcOvrDS->Reference();

        const double dfSrcRatioX = static_cast<double>(
            poSrcDS->GetRasterXSize() ) / poSrcOvrDS->GetRasterXSize();
        const double dfSrcRatioY = static_cast<double>(
            poSrcDS->GetRasterYSize() ) / poSrcOvrDS->GetRasterYSize();
        const double dfTargetRatio = static_cast<double>(
            poSrcDS->GetRasterXSize() ) /
            poSrcDS->GetRasterBand(1)->GetOverview(iOvr)->GetXSize();

/* -------------------------------------------------------------------- */
/*      Figure out the desired output bounds and resolution.            */
/* -------------------------------------------------------------------- */
        const int nDstPixels
            = static_cast<int>(nRasterXSize / dfTargetRatio + 0.5);
        const int nDstLines
            = static_cast<int>(nRasterYSize / dfTargetRatio + 0.5);

        double adfDstGeoTransform[6] = { 0.0 };
        GetGeoTransform(adfDstGeoTransform);
        if( adfDstGeoTransform[2] == 0.0 && adfDstGeoTransform[4] == 0.0 )
        {
            adfDstGeoTransform[1]
                *= static_cast<double>( nRasterXSize ) / nDstPixels;
            adfDstGeoTransform[5]
                *= static_cast<double>( nRasterYSize ) / nDstLines;
        }
        else
        {
            adfDstGeoTransform[1] *= dfTargetRatio;
            adfDstGeoTransform[2] *= dfTargetRatio;
            adfDstGeoTransform[4] *= dfTargetRatio;
            adfDstGeoTransform[5] *= dfTargetRatio;
        }

        if( nDstPixels < 1 || nDstLines < 1 )
        {
            poSrcOvrDS->ReleaseRef();
            break;
        }

/* -------------------------------------------------------------------- */
/*      Create transformer and warping options.                         */
/* -------------------------------------------------------------------- */
        void *pTransformerArg =
                         GDALCreateSimilarTransformer( psWO->pTransformerArg,
                                                       dfSrcRatioX,
                                                       dfSrcRatioY );
        if( pTransformerArg == NULL )
        {
            poSrcOvrDS->ReleaseRef();
            break;
        }

        GDALWarpOptions* psWOOvr = GDALCloneWarpOptions( psWO );
        psWOOvr->hSrcDS = poSrcOvrDS;
        psWOOvr->pfnTransformer = psWO->pfnTransformer;
        psWOOvr->pTransformerArg = pTransformerArg;

/* -------------------------------------------------------------------- */
/*      Update the transformer to include an output geotransform        */
/*      back to pixel/line coordinates.                                 */
/*                                                                      */
/* -------------------------------------------------------------------- */
        GDALSetTransformerDstGeoTransform(
            psWOOvr->pTransformerArg, adfDstGeoTransform );

/* -------------------------------------------------------------------- */
/*      Create the VRT file.                                            */
/* -------------------------------------------------------------------- */
        GDALDatasetH hDstDS = GDALCreateWarpedVRT(
            poSrcOvrDS,
            nDstPixels, nDstLines,
            adfDstGeoTransform, psWOOvr );

        poSrcOvrDS->ReleaseRef();

        GDALDestroyWarpOptions(psWOOvr);

        if( hDstDS == NULL )
        {
            GDALDestroyTransformer( pTransformerArg );
            break;
        }

        m_nOverviewCount++;
        m_papoOverviews = static_cast<VRTWarpedDataset **>(
            CPLRealloc( m_papoOverviews, sizeof(void*) * m_nOverviewCount ) );

        m_papoOverviews[m_nOverviewCount-1]
            = static_cast<VRTWarpedDataset *>( hDstDS );
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** VRTWarpedDataset::GetFileList()
{
    char** papszFileList = GDALDataset::GetFileList();

    if( m_poWarper != NULL )
    {
        const GDALWarpOptions *psWO = m_poWarper->GetOptions();
        const char* pszFilename = NULL;

        if( psWO->hSrcDS != NULL &&
            (pszFilename =
             reinterpret_cast<GDALDataset*>(psWO->hSrcDS)->GetDescription()) != NULL )
        {
            VSIStatBufL  sStat;
            if( VSIStatL( pszFilename, &sStat ) == 0 )
            {
                papszFileList = CSLAddString(papszFileList, pszFilename);
            }
        }
    }

    return papszFileList;
}

/************************************************************************/
/*                      SetApplyVerticalShiftGrid()                     */
/************************************************************************/

void  VRTWarpedDataset::SetApplyVerticalShiftGrid(const char* pszVGrids,
                                               int bInverse,
                                               double dfToMeterSrc,
                                               double dfToMeterDest,
                                               char** papszOptions )
{
    VerticalShiftGrid oVertShiftGrid;
    oVertShiftGrid.osVGrids = pszVGrids;
    oVertShiftGrid.bInverse = bInverse;
    oVertShiftGrid.dfToMeterSrc = dfToMeterSrc;
    oVertShiftGrid.dfToMeterDest = dfToMeterDest;
    oVertShiftGrid.aosOptions.Assign(papszOptions,FALSE);
    m_aoVerticalShiftGrids.push_back(oVertShiftGrid);
}


/************************************************************************/
/* ==================================================================== */
/*                    VRTWarpedOverviewTransformer                      */
/* ==================================================================== */
/************************************************************************/

typedef struct {
    GDALTransformerInfo sTI;

    GDALTransformerFunc pfnBaseTransformer;
    void              *pBaseTransformerArg;
    bool               bOwnSubtransformer;

    double            dfXOverviewFactor;
    double            dfYOverviewFactor;
} VWOTInfo;

static
void* VRTCreateWarpedOverviewTransformer( GDALTransformerFunc pfnBaseTransformer,
                                          void *pBaseTransformArg,
                                          double dfXOverviewFactor,
                                          double dfYOverviewFactor );
static
void VRTDestroyWarpedOverviewTransformer(void* pTransformArg);

static
int VRTWarpedOverviewTransform( void *pTransformArg, int bDstToSrc,
                                int nPointCount,
                                double *padfX, double *padfY, double *padfZ,
                                int *panSuccess );

#if 0  // TODO: Why?
/************************************************************************/
/*                VRTSerializeWarpedOverviewTransformer()               */
/************************************************************************/

static CPLXMLNode *
VRTSerializeWarpedOverviewTransformer( void *pTransformArg )

{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>( pTransformArg );

    CPLXMLNode *psTree
        = CPLCreateXMLNode( NULL, CXT_Element, "WarpedOverviewTransformer" );

    CPLCreateXMLElementAndValue(
        psTree, "XFactor",
        CPLString().Printf("%g",psInfo->dfXOverviewFactor) );
    CPLCreateXMLElementAndValue(
        psTree, "YFactor",
        CPLString().Printf("%g",psInfo->dfYOverviewFactor) );

/* -------------------------------------------------------------------- */
/*      Capture underlying transformer.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTransformerContainer
        = CPLCreateXMLNode( psTree, CXT_Element, "BaseTransformer" );

    CPLXMLNode *psTransformer
        = GDALSerializeTransformer( psInfo->pfnBaseTransformer,
                                    psInfo->pBaseTransformerArg );
    if( psTransformer != NULL )
        CPLAddXMLChild( psTransformerContainer, psTransformer );

    return psTree;
}

/************************************************************************/
/*           VRTWarpedOverviewTransformerOwnsSubtransformer()           */
/************************************************************************/

static void VRTWarpedOverviewTransformerOwnsSubtransformer( void *pTransformArg,
                                                            bool bOwnFlag )
{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>( pTransformArg );

    psInfo->bOwnSubtransformer = bOwnFlag;
}

/************************************************************************/
/*            VRTDeserializeWarpedOverviewTransformer()                 */
/************************************************************************/

void* VRTDeserializeWarpedOverviewTransformer( CPLXMLNode *psTree )

{
    const double dfXOverviewFactor =
        CPLAtof(CPLGetXMLValue( psTree, "XFactor",  "1" ));
    const double dfYOverviewFactor =
        CPLAtof(CPLGetXMLValue( psTree, "YFactor",  "1" ));
    GDALTransformerFunc pfnBaseTransform = NULL;
    void *pBaseTransformerArg = NULL;

    CPLXMLNode *psContainer = CPLGetXMLNode( psTree, "BaseTransformer" );

    if( psContainer != NULL && psContainer->psChild != NULL )
    {
        GDALDeserializeTransformer( psContainer->psChild,
                                    &pfnBaseTransform,
                                    &pBaseTransformerArg );
    }

    if( pfnBaseTransform == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot get base transform for scaled coord transformer." );
        return NULL;
    }
    else
    {
        void *pApproxCBData =
                       VRTCreateWarpedOverviewTransformer( pfnBaseTransform,
                                                           pBaseTransformerArg,
                                                           dfXOverviewFactor,
                                                           dfYOverviewFactor );
        VRTWarpedOverviewTransformerOwnsSubtransformer( pApproxCBData, true );

        return pApproxCBData;
    }
}
#endif  // TODO: Why disabled?

/************************************************************************/
/*                   VRTCreateWarpedOverviewTransformer()               */
/************************************************************************/

static
void* VRTCreateWarpedOverviewTransformer( GDALTransformerFunc pfnBaseTransformer,
                                          void *pBaseTransformerArg,
                                          double dfXOverviewFactor,
                                          double dfYOverviewFactor)

{
    if (pfnBaseTransformer == NULL)
        return NULL;

    VWOTInfo *psSCTInfo = static_cast<VWOTInfo*>(
        CPLMalloc( sizeof(VWOTInfo) ) );
    psSCTInfo->pfnBaseTransformer = pfnBaseTransformer;
    psSCTInfo->pBaseTransformerArg = pBaseTransformerArg;
    psSCTInfo->dfXOverviewFactor = dfXOverviewFactor;
    psSCTInfo->dfYOverviewFactor = dfYOverviewFactor;
    psSCTInfo->bOwnSubtransformer = false;

    memcpy( psSCTInfo->sTI.abySignature,
            GDAL_GTI2_SIGNATURE,
            strlen(GDAL_GTI2_SIGNATURE) );
    psSCTInfo->sTI.pszClassName = "VRTWarpedOverviewTransformer";
    psSCTInfo->sTI.pfnTransform = VRTWarpedOverviewTransform;
    psSCTInfo->sTI.pfnCleanup = VRTDestroyWarpedOverviewTransformer;
#if 0
    psSCTInfo->sTI.pfnSerialize = VRTSerializeWarpedOverviewTransformer;
#endif
    return psSCTInfo;
}

/************************************************************************/
/*               VRTDestroyWarpedOverviewTransformer()                  */
/************************************************************************/

static
void VRTDestroyWarpedOverviewTransformer(void* pTransformArg)
{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>( pTransformArg );

    if( psInfo->bOwnSubtransformer )
        GDALDestroyTransformer( psInfo->pBaseTransformerArg );

    CPLFree( psInfo );
}

/************************************************************************/
/*                     VRTWarpedOverviewTransform()                     */
/************************************************************************/

static
int VRTWarpedOverviewTransform( void *pTransformArg, int bDstToSrc,
                                int nPointCount,
                                double *padfX, double *padfY, double *padfZ,
                                int *panSuccess )

{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>( pTransformArg );

    if( bDstToSrc )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            padfX[i] *= psInfo->dfXOverviewFactor;
            padfY[i] *= psInfo->dfYOverviewFactor;
        }
    }

    const int bSuccess =
        psInfo->pfnBaseTransformer( psInfo->pBaseTransformerArg,
                                    bDstToSrc,
                                    nPointCount, padfX, padfY, padfZ,
                                    panSuccess );

    if( !bDstToSrc )
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            padfX[i] /= psInfo->dfXOverviewFactor;
            padfY[i] /= psInfo->dfYOverviewFactor;
        }
    }

    return bSuccess;
}

/************************************************************************/
/*                           BuildOverviews()                           */
/*                                                                      */
/*      For overviews, we actually just build a whole new dataset       */
/*      with an extra layer of transformation on the warper used to     */
/*      accomplish downsampling by the desired factor.                  */
/************************************************************************/

CPLErr
VRTWarpedDataset::IBuildOverviews( const char * /* pszResampling */,
                                   int nOverviews,
                                   int *panOverviewList,
                                   int /* nListBands */,
                                   int * /* panBandList */,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData )
{
    if( m_poWarper == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Initial progress result.                                        */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.                                                  */
/* -------------------------------------------------------------------- */
    int nNewOverviews = 0;
    int *panNewOverviewList = static_cast<int *>(
        CPLCalloc( sizeof(int), nOverviews ) );
    for( int i = 0; i < nOverviews; i++ )
    {
        for( int j = 0; j < m_nOverviewCount; j++ )
        {
            GDALDataset * const poOverview = m_papoOverviews[j];

            const int nOvFactor =
                GDALComputeOvFactor( poOverview->GetRasterXSize(),
                                     GetRasterXSize(),
                                     poOverview->GetRasterYSize(),
                                     GetRasterYSize() );

            if( nOvFactor == panOverviewList[i]
                || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                   GetRasterXSize(),
                                                   GetRasterYSize() ) )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
            panNewOverviewList[nNewOverviews++] = panOverviewList[i];
    }

/* -------------------------------------------------------------------- */
/*      Create each missing overview (we don't need to do anything      */
/*      to update existing overviews).                                  */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    for( int i = 0; i < nNewOverviews; i++ )
    {
/* -------------------------------------------------------------------- */
/*      What size should this overview be.                              */
/* -------------------------------------------------------------------- */
        const int nOXSize = (GetRasterXSize() + panNewOverviewList[i] - 1)
            / panNewOverviewList[i];

        const int nOYSize = (GetRasterYSize() + panNewOverviewList[i] - 1)
            / panNewOverviewList[i];

/* -------------------------------------------------------------------- */
/*      Find the most appropriate base dataset onto which to build the  */
/*      new one. The preference will be an overview dataset with a ratio*/
/*      greater than ours, and which is not using                       */
/*      VRTWarpedOverviewTransform, since those ones are slow. The other*/
/*      ones are based on overviews of the source dataset.              */
/* -------------------------------------------------------------------- */
        VRTWarpedDataset* poBaseDataset = this;
        for( int j = 0; j < m_nOverviewCount; j++ )
        {
            if( m_papoOverviews[j]->GetRasterXSize() > nOXSize &&
                m_papoOverviews[j]->m_poWarper->GetOptions()->pfnTransformer !=
                                                VRTWarpedOverviewTransform &&
                m_papoOverviews[j]->GetRasterXSize() <
                poBaseDataset->GetRasterXSize() )
            {
                poBaseDataset = m_papoOverviews[j];
            }
        }

/* -------------------------------------------------------------------- */
/*      Create the overview dataset.                                    */
/* -------------------------------------------------------------------- */
        VRTWarpedDataset *poOverviewDS
            = new VRTWarpedDataset( nOXSize, nOYSize );

        for( int iBand = 0; iBand < GetRasterCount(); iBand++ )
        {
            GDALRasterBand * const poOldBand = GetRasterBand(iBand+1);
            VRTWarpedRasterBand * const poNewBand =
                new VRTWarpedRasterBand( poOverviewDS, iBand+1,
                                         poOldBand->GetRasterDataType() );

            poNewBand->CopyCommonInfoFrom( poOldBand );
            poOverviewDS->SetBand( iBand+1, poNewBand );
        }

/* -------------------------------------------------------------------- */
/*      Prepare update transformation information that will apply       */
/*      the overview decimation.                                        */
/* -------------------------------------------------------------------- */
        GDALWarpOptions *psWO = const_cast<GDALWarpOptions *>(
            poBaseDataset->m_poWarper->GetOptions() );

/* -------------------------------------------------------------------- */
/*      Initialize the new dataset with adjusted warp options, and      */
/*      then restore to original condition.                             */
/* -------------------------------------------------------------------- */
        GDALTransformerFunc pfnTransformerBase = psWO->pfnTransformer;
        void* pTransformerBaseArg = psWO->pTransformerArg;

        psWO->pfnTransformer = VRTWarpedOverviewTransform;
        psWO->pTransformerArg = VRTCreateWarpedOverviewTransformer(
            pfnTransformerBase,
            pTransformerBaseArg,
            poBaseDataset->GetRasterXSize() / static_cast<double>( nOXSize ),
            poBaseDataset->GetRasterYSize() / static_cast<double>( nOYSize ) );

        eErr = poOverviewDS->Initialize( psWO );

        psWO->pfnTransformer = pfnTransformerBase;
        psWO->pTransformerArg = pTransformerBaseArg;

        if( eErr != CE_None )
        {
            delete poOverviewDS;
            break;
        }

        m_nOverviewCount++;
        m_papoOverviews = static_cast<VRTWarpedDataset **>(
            CPLRealloc( m_papoOverviews, sizeof(void*) * m_nOverviewCount ) );

        m_papoOverviews[m_nOverviewCount-1] = poOverviewDS;
    }

    CPLFree( panNewOverviewList );

/* -------------------------------------------------------------------- */
/*      Progress finished.                                              */
/* -------------------------------------------------------------------- */
    pfnProgress( 1.0, NULL, pProgressData );

    SetNeedsFlush();

    return eErr;
}

/*! @endcond */

/************************************************************************/
/*                      GDALInitializeWarpedVRT()                       */
/************************************************************************/

/**
 * Set warp info on virtual warped dataset.
 *
 * Initializes all the warping information for a virtual warped dataset.
 *
 * This method is the same as the C++ method VRTWarpedDataset::Initialize().
 *
 * @param hDS dataset previously created with the VRT driver, and a
 * SUBCLASS of "VRTWarpedDataset".
 *
 * @param psWO the warp options to apply.  Note that ownership of the
 * transformation information is taken over by the function though everything
 * else remains the property of the caller.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr CPL_STDCALL
GDALInitializeWarpedVRT( GDALDatasetH hDS, GDALWarpOptions *psWO )

{
    VALIDATE_POINTER1( hDS, "GDALInitializeWarpedVRT", CE_Failure );

    return reinterpret_cast<VRTWarpedDataset *>( hDS )->Initialize( psWO );
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTWarpedDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPathIn )

{

/* -------------------------------------------------------------------- */
/*      Initialize blocksize before calling sub-init so that the        */
/*      band initializers can get it from the dataset object when       */
/*      they are created.                                               */
/* -------------------------------------------------------------------- */
    m_nBlockXSize = atoi(CPLGetXMLValue(psTree, "BlockXSize", "512"));
    m_nBlockYSize = atoi(CPLGetXMLValue(psTree, "BlockYSize", "128"));

/* -------------------------------------------------------------------- */
/*      Initialize all the general VRT stuff.  This will even           */
/*      create the VRTWarpedRasterBands and initialize them.            */
/* -------------------------------------------------------------------- */
    {
        const CPLErr eErr = VRTDataset::XMLInit( psTree, pszVRTPathIn );

        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Find the GDALWarpOptions XML tree.                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode * const psOptionsTree =
        CPLGetXMLNode( psTree, "GDALWarpOptions" );
    if( psOptionsTree == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Count not find required GDALWarpOptions in XML." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Adjust the SourceDataset in the warp options to take into       */
/*      account that it is relative to the VRT if appropriate.          */
/* -------------------------------------------------------------------- */
    const bool bRelativeToVRT =
        CPL_TO_BOOL(atoi(CPLGetXMLValue(psOptionsTree,
                            "SourceDataset.relativeToVRT", "0" )));

    const char *pszRelativePath = CPLGetXMLValue(psOptionsTree,
                                                 "SourceDataset", "" );
    char *pszAbsolutePath = NULL;

    if( bRelativeToVRT )
        pszAbsolutePath =
            CPLStrdup(CPLProjectRelativeFilename( pszVRTPathIn,
                                                  pszRelativePath ) );
    else
        pszAbsolutePath = CPLStrdup(pszRelativePath);

    CPLSetXMLValue( psOptionsTree, "SourceDataset", pszAbsolutePath );
    CPLFree( pszAbsolutePath );

/* -------------------------------------------------------------------- */
/*      And instantiate the warp options, and corresponding warp        */
/*      operation.                                                      */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALDeserializeWarpOptions( psOptionsTree );
    if( psWO == NULL )
        return CE_Failure;

    /* Avoid errors when adding an alpha band, but source dataset has */
    /* no alpha band (#4571) */
    if( CSLFetchNameValue( psWO->papszWarpOptions, "INIT_DEST" ) == NULL )
        psWO->papszWarpOptions =
            CSLSetNameValue(psWO->papszWarpOptions, "INIT_DEST", "0");

    eAccess = GA_Update;

    if( psWO->hDstDS != NULL )
    {
        GDALClose( psWO->hDstDS );
        psWO->hDstDS = NULL;
    }

    psWO->hDstDS = this;

/* -------------------------------------------------------------------- */
/*      Deserialize vertical shift grids.                               */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psIter = psTree->psChild;
    for( ; psWO->hSrcDS != NULL && psIter != NULL; psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element ||
            !EQUAL(psIter->pszValue, "VerticalShiftGrids") )
        {
            continue;
        }
        const char* pszVGrids = CPLGetXMLValue(psIter, "Grids", NULL);
        if( pszVGrids )
        {
            int bInverse = CSLTestBoolean(
                CPLGetXMLValue(psIter, "Inverse", "FALSE") );
            double dfToMeterSrc = CPLAtof(
                CPLGetXMLValue( psIter, "ToMeterSrc", "1.0") );
            double dfToMeterDest = CPLAtof(
                CPLGetXMLValue( psIter, "ToMeterDest", "1.0") );
            char** papszOptions = NULL;
            CPLXMLNode* psIter2 = psIter->psChild;
            for( ; psIter2 != NULL; psIter2 = psIter2->psNext )
            {
                if( psIter2->eType != CXT_Element ||
                    !EQUAL(psIter2->pszValue, "Option") )
                {
                    continue;
                }
                const char* pszName = CPLGetXMLValue(psIter2, "name", NULL);
                const char* pszValue = CPLGetXMLValue(psIter2, NULL, NULL);
                if( pszName && pszValue )
                {
                    papszOptions = CSLSetNameValue(papszOptions, pszName,
                                                   pszValue);
                }
            }
            SetApplyVerticalShiftGrid(pszVGrids, bInverse,
                                      dfToMeterSrc, dfToMeterDest,
                                      papszOptions );
            int bError = FALSE;
            GDALDatasetH hGridDataset =
                GDALOpenVerticalShiftGrid(pszVGrids, &bError);
            if( bError && hGridDataset == NULL )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "Cannot open %s. Source dataset will no "
                            "be vertically adjusted regarding "
                            "vertical datum", pszVGrids);
            }
            else if( hGridDataset != NULL )
            {
                // Transform from source vertical datum to WGS84
                GDALDatasetH hTmpDS = GDALApplyVerticalShiftGrid(
                    psWO->hSrcDS, hGridDataset, bInverse,
                    dfToMeterSrc, dfToMeterDest, papszOptions );
                GDALReleaseDataset(hGridDataset);
                if( hTmpDS == NULL )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Source dataset will no "
                            "be vertically adjusted regarding "
                            "vertical datum %s", pszVGrids);
                }
                else
                {
                    CPLDebug("GDALWARP", "Adjusting source dataset "
                            "with vertical datum using %s", pszVGrids);
                    GDALReleaseDataset(psWO->hSrcDS);
                    psWO->hSrcDS = hTmpDS;
                }
            }

            CSLDestroy(papszOptions);
        }
    }

/* -------------------------------------------------------------------- */
/*      Instantiate the warp operation.                                 */
/* -------------------------------------------------------------------- */
    m_poWarper = new GDALWarpOperation();

    const CPLErr eErr = m_poWarper->Initialize( psWO );
    if( eErr != CE_None)
    {
/* -------------------------------------------------------------------- */
/*      We are responsible for cleaning up the transformer ourselves.   */
/* -------------------------------------------------------------------- */
        if( psWO->pTransformerArg != NULL )
        {
            GDALDestroyTransformer( psWO->pTransformerArg );
            psWO->pTransformerArg = NULL;
        }

        if( psWO->hSrcDS != NULL )
        {
            GDALClose( psWO->hSrcDS );
            psWO->hSrcDS = NULL;
        }
    }

    GDALDestroyWarpOptions( psWO );
    if( eErr != CE_None )
    {
        delete m_poWarper;
        m_poWarper = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Deserialize SrcOvrLevel                                         */
/* -------------------------------------------------------------------- */
    const char* pszSrcOvrLevel = CPLGetXMLValue( psTree, "SrcOvrLevel", NULL );
    if( pszSrcOvrLevel != NULL )
    {
        SetMetadataItem("SrcOvrLevel", pszSrcOvrLevel);
    }

/* -------------------------------------------------------------------- */
/*      Generate overviews, if appropriate.                             */
/* -------------------------------------------------------------------- */

    CreateImplicitOverviews();

    // OverviewList is historical, and quite inefficient, since it uses
    // the full resolution source dataset, so only build it afterwards.
    char **papszTokens = CSLTokenizeString(
        CPLGetXMLValue( psTree, "OverviewList", "" ) );

    for( int iOverview = 0;
         papszTokens != NULL && papszTokens[iOverview] != NULL;
         iOverview++ )
    {
        int nOvFactor = atoi(papszTokens[iOverview]);

        if (nOvFactor > 0)
            BuildOverviews( "NEAREST", 1, &nOvFactor, 0, NULL, NULL, NULL );
        else
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Bad value for overview factor : %s", papszTokens[iOverview] );
    }

    CSLDestroy( papszTokens );

    return eErr;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTWarpedDataset::SerializeToXML( const char *pszVRTPathIn )

{
    CPLXMLNode *psTree = VRTDataset::SerializeToXML( pszVRTPathIn );

    if( psTree == NULL )
        return psTree;

/* -------------------------------------------------------------------- */
/*      Set subclass.                                                   */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode(
        CPLCreateXMLNode( psTree, CXT_Attribute, "subClass" ),
        CXT_Text, "VRTWarpedDataset" );

/* -------------------------------------------------------------------- */
/*      Serialize the block size.                                       */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( psTree, "BlockXSize",
                                 CPLSPrintf( "%d", m_nBlockXSize ) );
    CPLCreateXMLElementAndValue( psTree, "BlockYSize",
                                 CPLSPrintf( "%d", m_nBlockYSize ) );

/* -------------------------------------------------------------------- */
/*      Serialize the overview list (only for non implicit overviews)   */
/* -------------------------------------------------------------------- */
    if( m_nOverviewCount > 0 )
    {
        int nSrcDSOvrCount = 0;
        if( m_poWarper != NULL  && m_poWarper->GetOptions() != NULL &&
            m_poWarper->GetOptions()->hSrcDS != NULL &&
            GDALGetRasterCount(m_poWarper->GetOptions()->hSrcDS) > 0 )
        {
            nSrcDSOvrCount
                = static_cast<GDALDataset*>(
                    m_poWarper->GetOptions()->hSrcDS)->
                GetRasterBand(1)->GetOverviewCount();
        }

        if( m_nOverviewCount != nSrcDSOvrCount )
        {
            const size_t nLen = m_nOverviewCount * 8 + 10;
            char *pszOverviewList = static_cast<char *>( CPLMalloc( nLen ) );
            pszOverviewList[0] = '\0';
            for( int iOverview = 0; iOverview < m_nOverviewCount; iOverview++ )
            {
                const int nOvFactor = static_cast<int>(
                    0.5 + GetRasterXSize()
                    / static_cast<double>(
                        m_papoOverviews[iOverview]->GetRasterXSize() ) );

                snprintf( pszOverviewList + strlen(pszOverviewList),
                          nLen - strlen(pszOverviewList),
                         "%d ", nOvFactor );
            }

            CPLCreateXMLElementAndValue( psTree, "OverviewList",
                                         pszOverviewList );

            CPLFree( pszOverviewList );
        }
    }

/* -------------------------------------------------------------------- */
/*      Serialize source overview level.                                */
/* -------------------------------------------------------------------- */
    if( m_nSrcOvrLevel != -2 )
    {
        if( m_nSrcOvrLevel < -2 )
            CPLCreateXMLElementAndValue(
                psTree, "SrcOvrLevel", CPLSPrintf("AUTO%d", m_nSrcOvrLevel+2) );
        else if( m_nSrcOvrLevel == -1 )
            CPLCreateXMLElementAndValue( psTree, "SrcOvrLevel", "NONE" );
        else
            CPLCreateXMLElementAndValue(
                psTree, "SrcOvrLevel", CPLSPrintf("%d", m_nSrcOvrLevel) );
    }

/* -------------------------------------------------------------------- */
/*      Serialize vertical shift grids.                                 */
/* -------------------------------------------------------------------- */
    for(size_t i = 0; i < m_aoVerticalShiftGrids.size(); ++i )
    {
        CPLXMLNode* psVertShiftGridNode = 
            CPLCreateXMLNode( psTree, CXT_Element, "VerticalShiftGrids" );
        CPLCreateXMLElementAndValue(psVertShiftGridNode,
                                    "Grids",
                                    m_aoVerticalShiftGrids[i].osVGrids);
        CPLCreateXMLElementAndValue(psVertShiftGridNode,
                "Inverse",
                m_aoVerticalShiftGrids[i].bInverse ? "TRUE" : "FALSE");
        CPLCreateXMLElementAndValue(psVertShiftGridNode,
                "ToMeterSrc",
                CPLSPrintf("%.18g", m_aoVerticalShiftGrids[i].dfToMeterSrc));
        CPLCreateXMLElementAndValue(psVertShiftGridNode,
                "ToMeterDest",
                CPLSPrintf("%.18g", m_aoVerticalShiftGrids[i].dfToMeterDest));
        for( int j=0; j < m_aoVerticalShiftGrids[i].aosOptions.size(); ++j )
        {
            char* pszKey = NULL;
            const char* pszValue = CPLParseNameValue(
                m_aoVerticalShiftGrids[i].aosOptions[j], &pszKey);
            if( pszKey && pszValue )
            {
                CPLXMLNode* psOption = CPLCreateXMLElementAndValue(
                    psVertShiftGridNode,
                    "Option",
                    pszValue);
                CPLCreateXMLNode(
                    CPLCreateXMLNode( psOption, CXT_Attribute, "name" ),
                    CXT_Text, pszKey );
            }
            CPLFree(pszKey);
        }
    }

/* ==================================================================== */
/*      Serialize the warp options.                                     */
/* ==================================================================== */
    if( m_poWarper != NULL )
    {
/* -------------------------------------------------------------------- */
/*      We reset the destination dataset name so it doesn't get         */
/*      written out in the serialize warp options.                      */
/* -------------------------------------------------------------------- */
        char * const pszSavedName = CPLStrdup(GetDescription());
        SetDescription("");

        CPLXMLNode * const psWOTree =
            GDALSerializeWarpOptions( m_poWarper->GetOptions() );
        CPLAddXMLChild( psTree, psWOTree );

        SetDescription( pszSavedName );
        CPLFree( pszSavedName );

/* -------------------------------------------------------------------- */
/*      We need to consider making the source dataset relative to       */
/*      the VRT file if possible.  Adjust accordingly.                  */
/* -------------------------------------------------------------------- */
        CPLXMLNode *psSDS = CPLGetXMLNode( psWOTree, "SourceDataset" );
        int bRelativeToVRT = FALSE;
        VSIStatBufL sStat;

        if( VSIStatExL( psSDS->psChild->pszValue, &sStat,
                        VSI_STAT_EXISTS_FLAG) == 0 )
        {
            char *pszRelativePath = CPLStrdup(
                CPLExtractRelativePath( pszVRTPathIn, psSDS->psChild->pszValue,
                                        &bRelativeToVRT ) );

            CPLFree( psSDS->psChild->pszValue );
            psSDS->psChild->pszValue = pszRelativePath;
        }

        CPLCreateXMLNode(
            CPLCreateXMLNode( psSDS, CXT_Attribute, "relativeToVRT" ),
            CXT_Text, bRelativeToVRT ? "1" : "0" );
    }

    return psTree;
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

void VRTWarpedDataset::GetBlockSize( int *pnBlockXSize, int *pnBlockYSize )

{
    CPLAssert( NULL != pnBlockXSize );
    CPLAssert( NULL != pnBlockYSize );

    *pnBlockXSize = m_nBlockXSize;
    *pnBlockYSize = m_nBlockYSize;
}

/************************************************************************/
/*                            ProcessBlock()                            */
/*                                                                      */
/*      Warp a single requested block, and then push each band of       */
/*      the result into the block cache.                                */
/************************************************************************/

CPLErr VRTWarpedDataset::ProcessBlock( int iBlockX, int iBlockY )

{
    if( m_poWarper == NULL )
        return CE_Failure;

    int nReqXSize = m_nBlockXSize;
    if( iBlockX * m_nBlockXSize + nReqXSize > nRasterXSize )
        nReqXSize = nRasterXSize - iBlockX * m_nBlockXSize;
    int nReqYSize = m_nBlockYSize;
    if( iBlockY * m_nBlockYSize + nReqYSize > nRasterYSize )
        nReqYSize = nRasterYSize - iBlockY * m_nBlockYSize;

    GByte *pabyDstBuffer = static_cast<GByte *>(
        m_poWarper->CreateDestinationBuffer(nReqXSize, nReqYSize));

    if( pabyDstBuffer == NULL )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Warp into this buffer.                                          */
/* -------------------------------------------------------------------- */
    
    const GDALWarpOptions *psWO = m_poWarper->GetOptions();
    const CPLErr eErr =
        m_poWarper->WarpRegionToBuffer(
            iBlockX * m_nBlockXSize, iBlockY * m_nBlockYSize,
            nReqXSize, nReqYSize,
            pabyDstBuffer, psWO->eWorkingDataType );

    if( eErr != CE_None )
    {
        m_poWarper->DestroyDestinationBuffer(pabyDstBuffer);
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Copy out into cache blocks for each band.                       */
/* -------------------------------------------------------------------- */
    const int nWordSize = GDALGetDataTypeSizeBytes(psWO->eWorkingDataType);
    for( int i = 0; i < psWO->nBandCount; i++ )
    {
        int nDstBand = psWO->panDstBands[i];
        if( GetRasterCount() < nDstBand ) { continue; }

        GDALRasterBand *poBand = GetRasterBand(nDstBand);
        GDALRasterBlock *poBlock
            = poBand->GetLockedBlockRef( iBlockX, iBlockY, TRUE );

        const GByte* pabyDstBandBuffer = 
            pabyDstBuffer + i*nReqXSize*nReqYSize*nWordSize;

        if( poBlock != NULL )
        {
            if ( poBlock->GetDataRef() != NULL )
            {
                if( nReqXSize == m_nBlockXSize && nReqYSize == m_nBlockYSize )
                {
                    GDALCopyWords(
                        pabyDstBandBuffer,
                        psWO->eWorkingDataType, nWordSize,
                        poBlock->GetDataRef(),
                        poBlock->GetDataType(),
                        GDALGetDataTypeSizeBytes(poBlock->GetDataType()),
                        m_nBlockXSize * m_nBlockYSize );
                }
                else
                {
                     GByte* pabyBlock = reinterpret_cast<GByte *>(
                        poBlock->GetDataRef() );
                    const int nDTSize =
                        GDALGetDataTypeSizeBytes(poBlock->GetDataType());
                    for(int iY=0;iY<nReqYSize;iY++)
                    {
                        GDALCopyWords(
                            pabyDstBandBuffer + iY * nReqXSize*nWordSize,
                            psWO->eWorkingDataType, nWordSize,
                            pabyBlock + iY * m_nBlockXSize * nDTSize,
                            poBlock->GetDataType(),
                            nDTSize,
                            nReqXSize );
                    }
                }
            }

            poBlock->DropLock();
        }
    }

    m_poWarper->DestroyDestinationBuffer(pabyDstBuffer);

    return CE_None;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTWarpedDataset::AddBand( GDALDataType eType, char ** /* papszOptions */ )

{
    SetBand( GetRasterCount() + 1,
             new VRTWarpedRasterBand( this, GetRasterCount() + 1, eType ) );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                        VRTWarpedRasterBand                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTWarpedRasterBand()                         */
/************************************************************************/

VRTWarpedRasterBand::VRTWarpedRasterBand( GDALDataset *poDSIn, int nBandIn,
                                          GDALDataType eType )

{
    Initialize( poDSIn->GetRasterXSize(), poDSIn->GetRasterYSize() );

    poDS = poDSIn;
    nBand = nBandIn;
    eAccess = GA_Update;

    reinterpret_cast<VRTWarpedDataset *>( poDS )->GetBlockSize( &nBlockXSize,
                                                                &nBlockYSize );

    if( eType != GDT_Unknown )
        eDataType = eType;
}

/************************************************************************/
/*                        ~VRTWarpedRasterBand()                        */
/************************************************************************/

VRTWarpedRasterBand::~VRTWarpedRasterBand()

{
    FlushCache();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTWarpedRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void * pImage )

{
    VRTWarpedDataset *poWDS = reinterpret_cast<VRTWarpedDataset *>( poDS );
    GDALRasterBlock *poBlock = GetLockedBlockRef( nBlockXOff, nBlockYOff, TRUE );
    if( poBlock == NULL )
        return CE_Failure;

    const CPLErr eErr = poWDS->ProcessBlock( nBlockXOff, nBlockYOff );

    if( eErr == CE_None && pImage != poBlock->GetDataRef() )
    {
        const int nDataBytes
            = (GDALGetDataTypeSize(poBlock->GetDataType()) / 8)
            * poBlock->GetXSize() * poBlock->GetYSize();
        memcpy( pImage, poBlock->GetDataRef(), nDataBytes );
    }

    poBlock->DropLock();

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr VRTWarpedRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                         void * pImage )

{
    VRTWarpedDataset *poWDS = reinterpret_cast<VRTWarpedDataset *>( poDS );

    // This is a bit tricky. In the case we are warping a VRTWarpedDataset
    // with a destination alpha band, IWriteBlock can be called on that alpha
    // band by GDALWarpDstAlphaMasker
    // We don't need to do anything since the data will be kept in the block
    // cache by VRTWarpedRasterBand::IReadBlock.
    if (poWDS->m_poWarper->GetOptions()->nDstAlphaBand != nBand)
    {
        /* Otherwise, call the superclass method, that will fail of course */
        return VRTRasterBand::IWriteBlock(nBlockXOff, nBlockYOff, pImage);
    }

    return CE_None;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTWarpedRasterBand::XMLInit( CPLXMLNode * psTree,
                                     const char *pszVRTPathIn )

{
    return VRTRasterBand::XMLInit( psTree, pszVRTPathIn );
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTWarpedRasterBand::SerializeToXML( const char *pszVRTPathIn )

{
    CPLXMLNode * const psTree = VRTRasterBand::SerializeToXML( pszVRTPathIn );

/* -------------------------------------------------------------------- */
/*      Set subclass.                                                   */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode(
        CPLCreateXMLNode( psTree, CXT_Attribute, "subClass" ),
        CXT_Text, "VRTWarpedRasterBand" );

    return psTree;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int VRTWarpedRasterBand::GetOverviewCount()

{
    VRTWarpedDataset * const poWDS =
        reinterpret_cast<VRTWarpedDataset *>( poDS );

    poWDS->CreateImplicitOverviews();

    return poWDS->m_nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *VRTWarpedRasterBand::GetOverview( int iOverview )

{
    VRTWarpedDataset * const poWDS =
        reinterpret_cast<VRTWarpedDataset *>( poDS );

    if( iOverview < 0 || iOverview >= GetOverviewCount() )
        return NULL;

    return poWDS->m_papoOverviews[iOverview]->GetRasterBand( nBand );
}

/*! @endcond */
