/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTWarpedRasterBand *and VRTWarpedDataset.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "vrtdataset.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdalwarper.h"
#include "gdal_alg_priv.h"
#include <cassert>

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
    GDALWarpOptions *psWO;
    int i;

    VALIDATE_POINTER1( hSrcDS, "GDALAutoCreateWarpedVRT", NULL );

/* -------------------------------------------------------------------- */
/*      Populate the warp options.                                      */
/* -------------------------------------------------------------------- */
    if( psOptionsIn != NULL )
        psWO = GDALCloneWarpOptions( psOptionsIn );
    else
        psWO = GDALCreateWarpOptions();

    psWO->eResampleAlg = eResampleAlg;

    psWO->hSrcDS = hSrcDS;

    psWO->nBandCount = GDALGetRasterCount( hSrcDS );
    psWO->panSrcBands = (int *) CPLMalloc(sizeof(int) * psWO->nBandCount);
    psWO->panDstBands = (int *) CPLMalloc(sizeof(int) * psWO->nBandCount);

    for( i = 0; i < psWO->nBandCount; i++ )
    {
        psWO->panSrcBands[i] = i+1;
        psWO->panDstBands[i] = i+1;
    }

    /* TODO: should fill in no data where available */

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
    double adfDstGeoTransform[6];
    int    nDstPixels, nDstLines;
    CPLErr eErr;

    eErr = 
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
    GDALDatasetH hDstDS;

    hDstDS = GDALCreateWarpedVRT( hSrcDS, nDstPixels, nDstLines, 
                                  adfDstGeoTransform, psWO );

    GDALDestroyWarpOptions( psWO );

    if( pszDstWKT != NULL )
        GDALSetProjection( hDstDS, pszDstWKT );
    else if( pszSrcWKT != NULL )
        GDALSetProjection( hDstDS, pszDstWKT );
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
 * @param padfGeoTransform Geotransform matrix of the virtual warped dataset to create
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

/* -------------------------------------------------------------------- */
/*      Create the VRTDataset and populate it with bands.               */
/* -------------------------------------------------------------------- */
    VRTWarpedDataset *poDS = new VRTWarpedDataset( nPixels, nLines );
    int i;

    psOptions->hDstDS = (GDALDatasetH) poDS;

    poDS->SetGeoTransform( padfGeoTransform );

    for( i = 0; i < psOptions->nBandCount; i++ )
    {
        VRTWarpedRasterBand *poBand;
        GDALRasterBand *poSrcBand = (GDALRasterBand *) 
            GDALGetRasterBand( hSrcDS, i+1 );

        poDS->AddBand( poSrcBand->GetRasterDataType(), NULL );

        poBand = (VRTWarpedRasterBand *) poDS->GetRasterBand( i+1 );
        poBand->CopyCommonInfoFrom( poSrcBand );
    }

/* -------------------------------------------------------------------- */
/*      Initialize the warp on the VRTWarpedDataset.                    */
/* -------------------------------------------------------------------- */
    poDS->Initialize( psOptions );
    
    return (GDALDatasetH) poDS;
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTWarpedDataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTWarpedDataset()                          */
/************************************************************************/

VRTWarpedDataset::VRTWarpedDataset( int nXSize, int nYSize )
        : VRTDataset( nXSize, nYSize )

{
    poWarper = NULL;
    nBlockXSize = MIN(nXSize, 512);
    nBlockYSize = MIN(nYSize, 128);
    eAccess = GA_Update;

    nOverviewCount = 0;
    papoOverviews = NULL;
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

    int bHasDroppedRef = VRTDataset::CloseDependentDatasets();

/* -------------------------------------------------------------------- */
/*      Cleanup overviews.                                              */
/* -------------------------------------------------------------------- */
    int iOverview;

    for( iOverview = 0; iOverview < nOverviewCount; iOverview++ )
    {
        GDALDatasetH hDS = (GDALDatasetH) papoOverviews[iOverview];

        if( GDALDereferenceDataset( hDS ) < 1 )
        {
            GDALReferenceDataset( hDS );
            GDALClose( hDS );
            bHasDroppedRef = TRUE;
        }
    }

    CPLFree( papoOverviews );
    nOverviewCount = 0;
    papoOverviews = NULL;

/* -------------------------------------------------------------------- */
/*      Cleanup warper if one is in effect.                             */
/* -------------------------------------------------------------------- */
    if( poWarper != NULL )
    {
        const GDALWarpOptions *psWO = poWarper->GetOptions();

/* -------------------------------------------------------------------- */
/*      We take care to only call GDALClose() on psWO->hSrcDS if the    */
/*      reference count drops to zero.  This is makes it so that we     */
/*      can operate reference counting semantics more-or-less           */
/*      properly even if the dataset isn't open in shared mode,         */
/*      though we require that the caller also honour the reference     */
/*      counting semantics even though it isn't a shared dataset.       */
/* -------------------------------------------------------------------- */
        if( psWO->hSrcDS != NULL )
        {
            if( GDALDereferenceDataset( psWO->hSrcDS ) < 1 )
            {
                GDALReferenceDataset( psWO->hSrcDS );
                GDALClose( psWO->hSrcDS );
                bHasDroppedRef = TRUE;
            }
        }

/* -------------------------------------------------------------------- */
/*      We are responsible for cleaning up the transformer outselves.   */
/* -------------------------------------------------------------------- */
        if( psWO->pTransformerArg != NULL )
            GDALDestroyTransformer( psWO->pTransformerArg );

        delete poWarper;
        poWarper = NULL;
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
/*                             Initialize()                             */
/*                                                                      */
/*      Initialize a dataset from passed in warp options.               */
/************************************************************************/

CPLErr VRTWarpedDataset::Initialize( void *psWO )

{
    if( poWarper != NULL )
        delete poWarper;

    poWarper = new GDALWarpOperation();

    GDALWarpOptions* psWO_Dup = GDALCloneWarpOptions((GDALWarpOptions *) psWO);

    /* Avoid errors when adding an alpha band, but source dataset has */
    /* no alpha band (#4571) */
    if (CSLFetchNameValue( psWO_Dup->papszWarpOptions, "INIT_DEST" ) == NULL)
        psWO_Dup->papszWarpOptions = CSLSetNameValue(psWO_Dup->papszWarpOptions, "INIT_DEST", "0");

    // The act of initializing this warped dataset with this warp options
    // will result in our assuming ownership of a reference to the
    // hSrcDS.

    if( ((GDALWarpOptions *) psWO)->hSrcDS != NULL )
        GDALReferenceDataset( psWO_Dup->hSrcDS );

    CPLErr eErr = poWarper->Initialize( psWO_Dup );

    GDALDestroyWarpOptions(psWO_Dup);

    return eErr;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** VRTWarpedDataset::GetFileList()
{
    char** papszFileList = GDALDataset::GetFileList();
    
    if( poWarper != NULL )
    {
        const GDALWarpOptions *psWO = poWarper->GetOptions();
        const char* pszFilename;
        
        if( psWO->hSrcDS != NULL &&
            (pszFilename =
                    ((GDALDataset*)psWO->hSrcDS)->GetDescription()) != NULL )
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
/* ==================================================================== */
/*                    VRTWarpedOverviewTransformer                      */
/* ==================================================================== */
/************************************************************************/

typedef struct {
    GDALTransformerInfo sTI;

    GDALTransformerFunc pfnBaseTransformer;
    void              *pBaseTransformerArg;
    int                bOwnSubtransformer;

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

/************************************************************************/
/*                VRTSerializeWarpedOverviewTransformer()               */
/************************************************************************/

static CPLXMLNode *
VRTSerializeWarpedOverviewTransformer( void *pTransformArg )

{
    CPLXMLNode *psTree;
    VWOTInfo *psInfo = (VWOTInfo *) pTransformArg;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "WarpedOverviewTransformer" );

    CPLCreateXMLElementAndValue( psTree, "XFactor",
                                 CPLString().Printf("%g",psInfo->dfXOverviewFactor) );
    CPLCreateXMLElementAndValue( psTree, "YFactor",
                                 CPLString().Printf("%g",psInfo->dfYOverviewFactor) );

/* -------------------------------------------------------------------- */
/*      Capture underlying transformer.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTransformerContainer;
    CPLXMLNode *psTransformer;

    psTransformerContainer =
        CPLCreateXMLNode( psTree, CXT_Element, "BaseTransformer" );

    psTransformer = GDALSerializeTransformer( psInfo->pfnBaseTransformer,
                                              psInfo->pBaseTransformerArg );
    if( psTransformer != NULL )
        CPLAddXMLChild( psTransformerContainer, psTransformer );

    return psTree;
}

/************************************************************************/
/*           VRTWarpedOverviewTransformerOwnsSubtransformer()           */
/************************************************************************/

static void VRTWarpedOverviewTransformerOwnsSubtransformer( void *pTransformArg,
                                                          int bOwnFlag )
{
    VWOTInfo *psInfo =
            (VWOTInfo *) pTransformArg;

    psInfo->bOwnSubtransformer = bOwnFlag;
}

/************************************************************************/
/*            VRTDeserializeWarpedOverviewTransformer()                 */
/************************************************************************/

void* VRTDeserializeWarpedOverviewTransformer( CPLXMLNode *psTree )

{
    double dfXOverviewFactor = atof(CPLGetXMLValue( psTree, "XFactor",  "1" ));
    double dfYOverviewFactor = atof(CPLGetXMLValue( psTree, "YFactor",  "1" ));
    CPLXMLNode *psContainer;
    GDALTransformerFunc pfnBaseTransform = NULL;
    void *pBaseTransformerArg = NULL;

    psContainer = CPLGetXMLNode( psTree, "BaseTransformer" );

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
        VRTWarpedOverviewTransformerOwnsSubtransformer( pApproxCBData, TRUE );

        return pApproxCBData;
    }
}


/************************************************************************/
/*                   VRTCreateWarpedOverviewTransformer()               */
/************************************************************************/

static
void* VRTCreateWarpedOverviewTransformer( GDALTransformerFunc pfnBaseTransformer,
                                          void *pBaseTransformerArg,
                                          double dfXOverviewFactor,
                                          double dfYOverviewFactor)

{
    VWOTInfo *psSCTInfo;

    if (pfnBaseTransformer == NULL)
        return NULL;

    psSCTInfo = (VWOTInfo*)
                    CPLMalloc(sizeof(VWOTInfo));
    psSCTInfo->pfnBaseTransformer = pfnBaseTransformer;
    psSCTInfo->pBaseTransformerArg = pBaseTransformerArg;
    psSCTInfo->dfXOverviewFactor = dfXOverviewFactor;
    psSCTInfo->dfYOverviewFactor = dfYOverviewFactor;
    psSCTInfo->bOwnSubtransformer = FALSE;

    strcpy( psSCTInfo->sTI.szSignature, "GTI" );
    psSCTInfo->sTI.pszClassName = "VRTWarpedOverviewTransformer";
    psSCTInfo->sTI.pfnTransform = VRTWarpedOverviewTransform;
    psSCTInfo->sTI.pfnCleanup = VRTDestroyWarpedOverviewTransformer;
    psSCTInfo->sTI.pfnSerialize = VRTSerializeWarpedOverviewTransformer;

    return psSCTInfo;
}

/************************************************************************/
/*               VRTDestroyWarpedOverviewTransformer()                  */
/************************************************************************/

static
void VRTDestroyWarpedOverviewTransformer(void* pTransformArg)
{
    VWOTInfo *psInfo = (VWOTInfo *) pTransformArg;

    if( psInfo->bOwnSubtransformer )
        GDALDestroyTransformer( psInfo->pBaseTransformerArg );

    CPLFree( psInfo );
}

/************************************************************************/
/*                     VRTWarpedOverviewTransform()                     */
/************************************************************************/

int VRTWarpedOverviewTransform( void *pTransformArg, int bDstToSrc,
                                int nPointCount,
                                double *padfX, double *padfY, double *padfZ,
                                int *panSuccess )

{
    VWOTInfo *psInfo = (VWOTInfo *) pTransformArg;
    int i, bSuccess;

    if( bDstToSrc )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            padfX[i] *= psInfo->dfXOverviewFactor;
            padfY[i] *= psInfo->dfYOverviewFactor;
        }
    }

    bSuccess = psInfo->pfnBaseTransformer( psInfo->pBaseTransformerArg,
                                           bDstToSrc,
                                           nPointCount, padfX, padfY, padfZ,
                                           panSuccess );

    if( !bDstToSrc )
    {
        for( i = 0; i < nPointCount; i++ )
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
VRTWarpedDataset::IBuildOverviews( const char *pszResampling, 
                                   int nOverviews, int *panOverviewList, 
                                   int nListBands, int *panBandList,
                                   GDALProgressFunc pfnProgress, 
                                   void * pProgressData )
    
{
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
    int   i, nNewOverviews, *panNewOverviewList = NULL;

    nNewOverviews = 0;
    panNewOverviewList = (int *) CPLCalloc(sizeof(int),nOverviews);
    for( i = 0; i < nOverviews; i++ )
    {
        int   j;

        for( j = 0; j < nOverviewCount; j++ )
        {
            int    nOvFactor;
            VRTWarpedDataset *poOverview = papoOverviews[j];
            
            nOvFactor = (int) 
                (0.5+GetRasterXSize() / (double) poOverview->GetRasterXSize());

            if( nOvFactor == panOverviewList[i] 
                || nOvFactor == GDALOvLevelAdjust( panOverviewList[i], 
                                                   GetRasterXSize() ) )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
            panNewOverviewList[nNewOverviews++] = panOverviewList[i];
    }

/* -------------------------------------------------------------------- */
/*      Create each missing overview (we don't need to do anything      */
/*      to update existing overviews).                                  */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nNewOverviews; i++ )
    {
        int    nOXSize, nOYSize, iBand;
        VRTWarpedDataset *poOverviewDS;
        
/* -------------------------------------------------------------------- */
/*      What size should this overview be.                              */
/* -------------------------------------------------------------------- */
        nOXSize = (GetRasterXSize() + panNewOverviewList[i] - 1) 
            / panNewOverviewList[i];
                                 
        nOYSize = (GetRasterYSize() + panNewOverviewList[i] - 1) 
            / panNewOverviewList[i];

/* -------------------------------------------------------------------- */
/*      Create the overview dataset.                                    */
/* -------------------------------------------------------------------- */
        poOverviewDS = new VRTWarpedDataset( nOXSize, nOYSize );
        
        for( iBand = 0; iBand < GetRasterCount(); iBand++ )
        {
            GDALRasterBand *poOldBand = GetRasterBand(iBand+1);
            VRTWarpedRasterBand *poNewBand = 
                new VRTWarpedRasterBand( poOverviewDS, iBand+1, 
                                         poOldBand->GetRasterDataType() );
            
            poNewBand->CopyCommonInfoFrom( poOldBand );
            poOverviewDS->SetBand( iBand+1, poNewBand );
        }

        nOverviewCount++;
        papoOverviews = (VRTWarpedDataset **)
            CPLRealloc( papoOverviews, sizeof(void*) * nOverviewCount );

        papoOverviews[nOverviewCount-1] = poOverviewDS;
        
/* -------------------------------------------------------------------- */
/*      Prepare update transformation information that will apply       */
/*      the overview decimation.                                        */
/* -------------------------------------------------------------------- */
        GDALWarpOptions *psWO = (GDALWarpOptions *) poWarper->GetOptions();

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
                                        GetRasterXSize() / (double) nOXSize,
                                        GetRasterYSize() / (double) nOYSize );

        poOverviewDS->Initialize( psWO );

        psWO->pfnTransformer = pfnTransformerBase;
        psWO->pTransformerArg = pTransformerBaseArg;
    }

    CPLFree( panNewOverviewList );

/* -------------------------------------------------------------------- */
/*      Progress finished.                                              */
/* -------------------------------------------------------------------- */
    pfnProgress( 1.0, NULL, pProgressData );

    SetNeedsFlush();

    return CE_None;
}

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

    return ((VRTWarpedDataset *) hDS)->Initialize( psWO );
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTWarpedDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPath )

{
    CPLErr eErr;

/* -------------------------------------------------------------------- */
/*      Initialize blocksize before calling sub-init so that the        */
/*      band initializers can get it from the dataset object when       */
/*      they are created.                                               */
/* -------------------------------------------------------------------- */
    nBlockXSize = atoi(CPLGetXMLValue(psTree,"BlockXSize","512"));
    nBlockYSize = atoi(CPLGetXMLValue(psTree,"BlockYSize","128"));

/* -------------------------------------------------------------------- */
/*      Initialize all the general VRT stuff.  This will even           */
/*      create the VRTWarpedRasterBands and initialize them.            */
/* -------------------------------------------------------------------- */
    eErr = VRTDataset::XMLInit( psTree, pszVRTPath );

    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Find the GDALWarpOptions XML tree.                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psOptionsTree;
    psOptionsTree = CPLGetXMLNode( psTree, "GDALWarpOptions" );
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
    int bRelativeToVRT = 
        atoi(CPLGetXMLValue(psOptionsTree,
                            "SourceDataset.relativeToVRT", "0" ));

    const char *pszRelativePath = CPLGetXMLValue(psOptionsTree,
                                                 "SourceDataset", "" );
    char *pszAbsolutePath;

    if( bRelativeToVRT )
        pszAbsolutePath = 
            CPLStrdup(CPLProjectRelativeFilename( pszVRTPath, 
                                                  pszRelativePath ) );
    else
        pszAbsolutePath = CPLStrdup(pszRelativePath);

    CPLSetXMLValue( psOptionsTree, "SourceDataset", pszAbsolutePath );
    CPLFree( pszAbsolutePath );

/* -------------------------------------------------------------------- */
/*      And instantiate the warp options, and corresponding warp        */
/*      operation.                                                      */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO;

    psWO = GDALDeserializeWarpOptions( psOptionsTree );
    if( psWO == NULL )
        return CE_Failure;

    /* Avoid errors when adding an alpha band, but source dataset has */
    /* no alpha band (#4571) */
    if (CSLFetchNameValue( psWO->papszWarpOptions, "INIT_DEST" ) == NULL)
        psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions, "INIT_DEST", "0");

    this->eAccess = GA_Update;

    if( psWO->hDstDS != NULL )
    {
        GDALClose( psWO->hDstDS );
        psWO->hDstDS = NULL;
    }

    psWO->hDstDS = this;

/* -------------------------------------------------------------------- */
/*      Instantiate the warp operation.                                 */
/* -------------------------------------------------------------------- */
    poWarper = new GDALWarpOperation();

    eErr = poWarper->Initialize( psWO );
    if( eErr != CE_None)
    {
/* -------------------------------------------------------------------- */
/*      We are responsible for cleaning up the transformer outselves.   */
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
        delete poWarper;
        poWarper = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Generate overviews, if appropriate.                             */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeString( 
        CPLGetXMLValue( psTree, "OverviewList", "" ));
    int iOverview;

    for( iOverview = 0; 
         papszTokens != NULL && papszTokens[iOverview] != NULL;
         iOverview++ )
    {
        int nOvFactor = atoi(papszTokens[iOverview]);

        if (nOvFactor > 0)
            BuildOverviews( "NEAREST", 1, &nOvFactor, 0, NULL, NULL, NULL );
        else
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad value for overview factor : %s", papszTokens[iOverview]);
    }

    CSLDestroy( papszTokens );

    return eErr;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTWarpedDataset::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode *psTree;

    psTree = VRTDataset::SerializeToXML( pszVRTPath );

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
                                 CPLSPrintf( "%d", nBlockXSize ) );
    CPLCreateXMLElementAndValue( psTree, "BlockYSize",
                                 CPLSPrintf( "%d", nBlockYSize ) );

/* -------------------------------------------------------------------- */
/*      Serialize the overview list.                                    */
/* -------------------------------------------------------------------- */
    if( nOverviewCount > 0 )
    {
        char *pszOverviewList;
        int iOverview;
        
        pszOverviewList = (char *) CPLMalloc(nOverviewCount*8 + 10);
        pszOverviewList[0] = '\0';
        for( iOverview = 0; iOverview < nOverviewCount; iOverview++ )
        {
            int nOvFactor;

            nOvFactor = (int) 
                (0.5+GetRasterXSize() 
                 / (double) papoOverviews[iOverview]->GetRasterXSize());

            sprintf( pszOverviewList + strlen(pszOverviewList), 
                     "%d ", nOvFactor );
        }

        CPLCreateXMLElementAndValue( psTree, "OverviewList", pszOverviewList );

        CPLFree( pszOverviewList );
    }

/* ==================================================================== */
/*      Serialize the warp options.                                     */
/* ==================================================================== */
    CPLXMLNode *psWOTree;

    if( poWarper != NULL )
    {
/* -------------------------------------------------------------------- */
/*      We reset the destination dataset name so it doesn't get         */
/*      written out in the serialize warp options.                      */
/* -------------------------------------------------------------------- */
        char *pszSavedName = CPLStrdup(GetDescription());
        SetDescription("");

        psWOTree = GDALSerializeWarpOptions( poWarper->GetOptions() );
        CPLAddXMLChild( psTree, psWOTree );

        SetDescription( pszSavedName );
        CPLFree( pszSavedName );

/* -------------------------------------------------------------------- */
/*      We need to consider making the source dataset relative to       */
/*      the VRT file if possible.  Adjust accordingly.                  */
/* -------------------------------------------------------------------- */
        CPLXMLNode *psSDS = CPLGetXMLNode( psWOTree, "SourceDataset" );
        int bRelativeToVRT = FALSE;
        VSIStatBufL  sStat;

        if( VSIStatExL( psSDS->psChild->pszValue, &sStat, 
                        VSI_STAT_EXISTS_FLAG) == 0 ) 
        {
            char *pszRelativePath;
        
            pszRelativePath = CPLStrdup(
                CPLExtractRelativePath( pszVRTPath, psSDS->psChild->pszValue, 
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
    assert( NULL != pnBlockXSize );
    assert( NULL != pnBlockYSize );

    *pnBlockXSize = nBlockXSize;
    *pnBlockYSize = nBlockYSize;
}

/************************************************************************/
/*                            ProcessBlock()                            */
/*                                                                      */
/*      Warp a single requested block, and then push each band of       */
/*      the result into the block cache.                                */
/************************************************************************/

CPLErr VRTWarpedDataset::ProcessBlock( int iBlockX, int iBlockY )

{
    if( poWarper == NULL )
        return CE_Failure;

    const GDALWarpOptions *psWO = poWarper->GetOptions();

/* -------------------------------------------------------------------- */
/*      Allocate block of memory large enough to hold all the bands     */
/*      for this block.                                                 */
/* -------------------------------------------------------------------- */
    int iBand;
    GByte *pabyDstBuffer;
    int   nDstBufferSize;
    int   nWordSize = (GDALGetDataTypeSize(psWO->eWorkingDataType) / 8);

    // FIXME? : risk of overflow in multiplication if nBlockXSize or nBlockYSize are very large
    nDstBufferSize = nBlockXSize * nBlockYSize * psWO->nBandCount * nWordSize;

    pabyDstBuffer = (GByte *) VSIMalloc(nDstBufferSize);

    if( pabyDstBuffer == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Out of memory allocating %d byte buffer in VRTWarpedDataset::ProcessBlock()",
                  nDstBufferSize );
        return CE_Failure;
    }				

    memset( pabyDstBuffer, 0, nDstBufferSize );

/* -------------------------------------------------------------------- */
/*      Process INIT_DEST option to initialize the buffer prior to      */
/*      warping into it.                                                */
/* NOTE:The following code is 99% similar in gdalwarpoperation.cpp and  */
/*      vrtwarped.cpp. Be careful to keep it in sync !                  */
/* -------------------------------------------------------------------- */
    const char *pszInitDest = CSLFetchNameValue( psWO->papszWarpOptions,
                                                 "INIT_DEST" );

    if( pszInitDest != NULL && !EQUAL(pszInitDest, "") )
    {
        char **papszInitValues = 
            CSLTokenizeStringComplex( pszInitDest, ",", FALSE, FALSE );
        int nInitCount = CSLCount(papszInitValues);
                                                           
        for( iBand = 0; iBand < psWO->nBandCount; iBand++ )
        {
            double adfInitRealImag[2];
            GByte *pBandData;
            int nBandSize = nBlockXSize * nBlockYSize * nWordSize;
            const char *pszBandInit = papszInitValues[MIN(iBand,nInitCount-1)];

            if( EQUAL(pszBandInit,"NO_DATA")
                && psWO->padfDstNoDataReal != NULL )
            {
                adfInitRealImag[0] = psWO->padfDstNoDataReal[iBand];
                adfInitRealImag[1] = psWO->padfDstNoDataImag[iBand];
            }
            else
            {
                CPLStringToComplex( pszBandInit,
                                    adfInitRealImag + 0, adfInitRealImag + 1);
            }

            pBandData = ((GByte *) pabyDstBuffer) + iBand * nBandSize;
            
            if( psWO->eWorkingDataType == GDT_Byte )
                memset( pBandData, 
                        MAX(0,MIN(255,(int)adfInitRealImag[0])), 
                        nBandSize);
            else if( !CPLIsNan(adfInitRealImag[0]) && adfInitRealImag[0] == 0.0 &&
                     !CPLIsNan(adfInitRealImag[1]) && adfInitRealImag[1] == 0.0 )
            {
                memset( pBandData, 0, nBandSize );
            }
            else if( !CPLIsNan(adfInitRealImag[1]) && adfInitRealImag[1] == 0.0 )
            {
                GDALCopyWords( &adfInitRealImag, GDT_Float64, 0, 
                               pBandData,psWO->eWorkingDataType,nWordSize,
                               nBlockXSize * nBlockYSize );
            }
            else
            {
                GDALCopyWords( &adfInitRealImag, GDT_CFloat64, 0, 
                               pBandData,psWO->eWorkingDataType,nWordSize,
                               nBlockXSize * nBlockYSize );
            }
        }

        CSLDestroy( papszInitValues );
    }

/* -------------------------------------------------------------------- */
/*      Warp into this buffer.                                          */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    eErr = 
        poWarper->WarpRegionToBuffer( 
            iBlockX * nBlockXSize, iBlockY * nBlockYSize, 
            nBlockXSize, nBlockYSize,
            pabyDstBuffer, psWO->eWorkingDataType );

    if( eErr != CE_None )
    {
        VSIFree( pabyDstBuffer );
        return eErr;
    }
                        
/* -------------------------------------------------------------------- */
/*      Copy out into cache blocks for each band.                       */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < MIN(nBands, psWO->nBandCount); iBand++ )
    {
        GDALRasterBand *poBand;
        GDALRasterBlock *poBlock;

        poBand = GetRasterBand(iBand+1);
        poBlock = poBand->GetLockedBlockRef( iBlockX, iBlockY, TRUE );

        if( poBlock != NULL )
        {
            if ( poBlock->GetDataRef() != NULL )
            {
                GDALCopyWords( pabyDstBuffer + iBand*nBlockXSize*nBlockYSize*nWordSize,
                            psWO->eWorkingDataType, nWordSize, 
                            poBlock->GetDataRef(), 
                            poBlock->GetDataType(), 
                            GDALGetDataTypeSize(poBlock->GetDataType())/8,
                            nBlockXSize * nBlockYSize );
            }

            poBlock->DropLock();
        }
    }

    VSIFree( pabyDstBuffer );
    
    return CE_None;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTWarpedDataset::AddBand( GDALDataType eType, char **papszOptions )

{
    UNREFERENCED_PARAM( papszOptions );

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

VRTWarpedRasterBand::VRTWarpedRasterBand( GDALDataset *poDS, int nBand,
                                          GDALDataType eType )

{
    Initialize( poDS->GetRasterXSize(), poDS->GetRasterYSize() );

    this->poDS = poDS;
    this->nBand = nBand;
    this->eAccess = GA_Update;

    ((VRTWarpedDataset *) poDS)->GetBlockSize( &nBlockXSize, 
                                               &nBlockYSize );

    if( eType != GDT_Unknown )
        this->eDataType = eType;
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
    CPLErr eErr;
    VRTWarpedDataset *poWDS = (VRTWarpedDataset *) poDS;
    GDALRasterBlock *poBlock;

    poBlock = GetLockedBlockRef( nBlockXOff, nBlockYOff, TRUE );
    if( poBlock == NULL )
        return CE_Failure;

    eErr = poWDS->ProcessBlock( nBlockXOff, nBlockYOff );

    if( eErr == CE_None && pImage != poBlock->GetDataRef() )
    {
        int nDataBytes;
        nDataBytes = (GDALGetDataTypeSize(poBlock->GetDataType()) / 8)
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
    CPLErr eErr;
    VRTWarpedDataset *poWDS = (VRTWarpedDataset *) poDS;

    /* This is a bit tricky. In the case we are warping a VRTWarpedDataset */
    /* with a destination alpha band, IWriteBlock can be called on that alpha */
    /* band by GDALWarpDstAlphaMasker */
    /* We don't need to do anything since the data will be kept in the block */
    /* cache by VRTWarpedRasterBand::IReadBlock */
    if (poWDS->poWarper->GetOptions()->nDstAlphaBand == nBand)
    {
        eErr = CE_None;
    }
    else
    {
        /* Otherwise, call the superclass method, that will fail of course */
        eErr = VRTRasterBand::IWriteBlock(nBlockXOff, nBlockYOff, pImage);
    }

    return eErr;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTWarpedRasterBand::XMLInit( CPLXMLNode * psTree, 
                                  const char *pszVRTPath )

{
    return VRTRasterBand::XMLInit( psTree, pszVRTPath );
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTWarpedRasterBand::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode *psTree = VRTRasterBand::SerializeToXML( pszVRTPath );

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
    VRTWarpedDataset *poWDS = (VRTWarpedDataset *) poDS;

    return poWDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *VRTWarpedRasterBand::GetOverview( int iOverview )

{
    VRTWarpedDataset *poWDS = (VRTWarpedDataset *) poDS;

    if( iOverview < 0 || iOverview >= poWDS->nOverviewCount )
        return NULL;
    else
        return poWDS->papoOverviews[iOverview]->GetRasterBand( nBand );
}

