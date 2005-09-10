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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.13  2005/09/10 15:51:47  fwarmerdam
 * cleanup VRT overviews
 *
 * Revision 1.12  2005/08/02 22:21:47  fwarmerdam
 * fixed a whopper of a memory leak in ProcessBlock
 *
 * Revision 1.11  2005/05/23 06:58:46  fwarmerdam
 * updated to locked blockref api
 *
 * Revision 1.10  2005/05/10 04:50:42  fwarmerdam
 * GDALOvLevelAdjust now global
 *
 * Revision 1.9  2005/04/25 22:26:46  fwarmerdam
 * avoid extra copy if we are loading into the cache block in IReadBlock
 *
 * Revision 1.8  2005/04/11 17:36:26  fwarmerdam
 * make sure we cleanup up transformer(s) too
 *
 * Revision 1.7  2005/04/04 15:25:59  fwarmerdam
 * some functions now CPL_STDCALL
 *
 * Revision 1.6  2004/09/29 13:21:08  fwarmerdam
 * Don't try and use relative include paths.
 *
 * Revision 1.5  2004/08/24 20:30:28  warmerda
 * fixed size passed into GDALCopyWords() - irvine.pix bug
 *
 * Revision 1.4  2004/08/13 16:15:08  warmerda
 * added some docs
 *
 * Revision 1.3  2004/08/12 08:24:26  warmerda
 * added overview support
 *
 * Revision 1.2  2004/08/11 20:10:54  warmerda
 * fixed issue with intializing blocksizes
 *
 * Revision 1.1  2004/08/11 18:42:40  warmerda
 * New
 *
 */

#include "vrtdataset.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
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
 * @param eResampleAlg One of GRA_NearestNeighbour, GRA_Bilinear, GRA_Cubic or 
 * GRA_CubicSpline.  Controls the sampling method used. 
 *
 * @param dfMaxError Maximum error measured in input pixels that is allowed in 
 * approximating the transformation (0.0 for exact calculations).
 *
 * @param psOptions Additional warp options, normally NULL.
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
        GDALCreateGenImgProjTransformer( psWO->hSrcDS, pszSrcWKT, NULL, NULL,
                                         TRUE, 1.0, 0 );

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
 * @param nOverviewLevels The number of "power of 2" overview levels to be 
 * built.  If zero, no overview levels will be managed.  
 *
 * @param psOptions Additional warp options, normally NULL.
 *
 * @return NULL on failure, or a new virtual dataset handle on success.
 */

GDALDatasetH CPL_STDCALL
GDALCreateWarpedVRT( GDALDatasetH hSrcDS, 
                     int nPixels, int nLines, double *padfGeoTransform,
                     GDALWarpOptions *psOptions )
    
{
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
    nBlockXSize = 512;
    nBlockYSize = 128;
    eAccess = GA_Update;

    nOverviewCount = 0;
    papoOverviews = NULL;
}

/************************************************************************/
/*                         ~VRTWarpedDataset()                          */
/************************************************************************/

VRTWarpedDataset::~VRTWarpedDataset()

{
    FlushCache();

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
        }
    }

    CPLFree( papoOverviews );

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
            }
        }

/* -------------------------------------------------------------------- */
/*      We are responsible for cleaning up the transformer outselves.   */
/* -------------------------------------------------------------------- */
        if( psWO->pTransformerArg != NULL )
            GDALDestroyTransformer( psWO->pTransformerArg );

        delete poWarper;
    }
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

    // The act of initializing this warped dataset with this warp options
    // will result in our assuming ownership of a reference to the
    // hSrcDS.

    if( ((GDALWarpOptions *) psWO)->hSrcDS != NULL )
        GDALReferenceDataset( ((GDALWarpOptions *) psWO)->hSrcDS );

    return poWarper->Initialize( (GDALWarpOptions *) psWO );
}

/************************************************************************/
/*                     VRTWarpedOverviewTransform()                     */
/************************************************************************/

typedef struct {
    GDALTransformerFunc pfnBaseTransformer;
    void              *pBaseTransformerArg;

    double            dfXOverviewFactor;
    double            dfYOverviewFactor;
} VWOTInfo;

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
        VWOTInfo *psInfo;
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
        psInfo = (VWOTInfo *) CPLCalloc(sizeof(VWOTInfo),1);

        psInfo->pfnBaseTransformer = psWO->pfnTransformer;
        psInfo->pBaseTransformerArg = psWO->pTransformerArg;

        psInfo->dfXOverviewFactor = GetRasterXSize() / (double) nOXSize;
        psInfo->dfYOverviewFactor = GetRasterYSize() / (double) nOYSize;

/* -------------------------------------------------------------------- */
/*      Initialize the new dataset with adjusted warp options, and      */
/*      then restore to original condition.                             */
/* -------------------------------------------------------------------- */
        psWO->pfnTransformer = VRTWarpedOverviewTransform;
        psWO->pTransformerArg = psInfo;

        poOverviewDS->Initialize( psWO );

        psWO->pfnTransformer = psInfo->pfnBaseTransformer;
        psWO->pTransformerArg = psInfo->pBaseTransformerArg;
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

    this->eAccess = GA_Update;
    psWO->hDstDS = this;

/* -------------------------------------------------------------------- */
/*      Instantiate the warp operation.                                 */
/* -------------------------------------------------------------------- */
    poWarper = new GDALWarpOperation();

    eErr = poWarper->Initialize( psWO );

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

        BuildOverviews( "NEAREST", 1, &nOvFactor, 0, NULL, NULL, NULL );
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
        int bRelativeToVRT;
        char *pszRelativePath;

        pszRelativePath = 
            CPLStrdup(
                CPLExtractRelativePath( pszVRTPath, psSDS->psChild->pszValue, 
                                        &bRelativeToVRT ) );

        CPLFree( psSDS->psChild->pszValue );
        psSDS->psChild->pszValue = pszRelativePath;

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
    GByte *pabyDstBuffer;
    int   nDstBufferSize;
    int   nWordSize = (GDALGetDataTypeSize(psWO->eWorkingDataType) / 8);

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
    int iBand;

    for( iBand = 0; iBand < psWO->nBandCount; iBand++ )
    {
        GDALRasterBand *poBand;
        GDALRasterBlock *poBlock;

        poBand = GetRasterBand(iBand+1);
        poBlock = poBand->GetLockedBlockRef( iBlockX, iBlockY, TRUE );

        CPLAssert( poBlock != NULL && poBlock->GetDataRef() != NULL );

        GDALCopyWords( pabyDstBuffer + iBand*nBlockXSize*nBlockYSize*nWordSize,
                       psWO->eWorkingDataType, nWordSize, 
                       poBlock->GetDataRef(), 
                       poBlock->GetDataType(), 
                       GDALGetDataTypeSize(poBlock->GetDataType())/8,
                       nBlockXSize * nBlockYSize );

        poBlock->DropLock();
    }

    VSIFree( pabyDstBuffer );
    
    return CE_None;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTWarpedDataset::AddBand( GDALDataType eType, char **papszOptions )

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

