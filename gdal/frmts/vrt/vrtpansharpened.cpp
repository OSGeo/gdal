/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTPansharpenedRasterBand and VRTPansharpenedDataset.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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
#include "gdal_vrt.h"
#include "gdalpansharpen.h"
#include "ogr_spatialref.h"
#include <map>
#include <set>
#include <assert.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                    GDALCreatePansharpenedVRT()                       */
/************************************************************************/

/**
 * Create a virtual pansharpened dataset.
 *
 * This function will create a virtual pansharpened dataset.
 *
 * Note that no reference will be taken on the passed bands. Consequently,
 * they or their dataset to which they belong to must be kept open until
 * this virtual pansharpened dataset is closed.
 *
 * The returned dataset will have no associated filename for itself.  If you
 * want to write the virtual dataset description to a file, use the
 * GDALSetDescription() function (or SetDescription() method) on the dataset
 * to assign a filename before it is closed.  
 *
 * @param pszXML Pansharpened VRT XML where &lt;SpectralBand&gt; elements have
 * no explicit SourceFilename and SourceBand. The spectral bands in the XML will be assigned
 * the successive values of the pahInputSpectralBands array. Must not be NULL.
 *
 * @param hPanchroBand Panchromatic band. Must not be NULL.
 *
 * @param nInputSpectralBands Number of input spectral bands.  Must be greater than zero.
 *
 * @param pahInputSpectralBands Array of nInputSpectralBands spectral bands.
 *
 * @return NULL on failure, or a new virtual dataset handle on success to be closed
 * with GDALClose().
 * 
 * @since GDAL 2.1
 */

GDALDatasetH GDALCreatePansharpenedVRT( const char* pszXML,
                                            GDALRasterBandH hPanchroBand,
                                            int nInputSpectralBands,
                                            GDALRasterBandH* pahInputSpectralBands )
{
    VALIDATE_POINTER1( pszXML, "GDALCreatePansharpenedVRT", NULL );
    VALIDATE_POINTER1( hPanchroBand, "GDALCreatePansharpenedVRT", NULL );
    VALIDATE_POINTER1( pahInputSpectralBands, "GDALCreatePansharpenedVRT", NULL );

    CPLXMLNode* psTree = CPLParseXMLString(pszXML);
    if( psTree == NULL )
        return NULL;
    VRTPansharpenedDataset* poDS = new VRTPansharpenedDataset(0,0);
    CPLErr eErr = poDS->XMLInit(psTree, NULL, hPanchroBand, 
                                  nInputSpectralBands, pahInputSpectralBands);
    CPLDestroyXMLNode(psTree);
    if( eErr != CE_None )
    {
        delete poDS;
        return NULL;
    }
    return (GDALDatasetH) poDS;
}

/************************************************************************/
/* ==================================================================== */
/*                        VRTPansharpenedDataset                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                       VRTPansharpenedDataset()                       */
/************************************************************************/

VRTPansharpenedDataset::VRTPansharpenedDataset( int nXSize, int nYSize )
        : VRTDataset( nXSize, nYSize )

{
    nBlockXSize = MIN(nXSize, 512);
    nBlockYSize = MIN(nYSize, 512);
    eAccess = GA_Update;
    poPansharpener = NULL;
    poMainDataset = this;
    bLoadingOtherBands = FALSE;
    bHasWarnedDisableAggressiveBandCaching = FALSE;
    pabyLastBufferBandRasterIO = NULL;
    nLastBandRasterIOXOff = 0;
    nLastBandRasterIOYOff = 0;
    nLastBandRasterIOXSize = 0;
    nLastBandRasterIOYSize = 0;
    eLastBandRasterIODataType = GDT_Unknown;
    eGTAdjustment = GTAdjust_Union;
    bNoDataDisabled = FALSE;
}

/************************************************************************/
/*                    ~VRTPansharpenedDataset()                         */
/************************************************************************/

VRTPansharpenedDataset::~VRTPansharpenedDataset()

{
    CloseDependentDatasets();
    CPLFree(pabyLastBufferBandRasterIO);
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTPansharpenedDataset::CloseDependentDatasets()
{
    if( poMainDataset == NULL )
        return FALSE;
    FlushCache();

    VRTPansharpenedDataset* poMainDatasetLocal = poMainDataset;
    poMainDataset = NULL;
    int bHasDroppedRef = VRTDataset::CloseDependentDatasets();

/* -------------------------------------------------------------------- */
/*      Destroy the raster bands if they exist.                         */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    if( poPansharpener != NULL )
    {
        // Delete the pansharper object before closing the dataset
        // because it may have warped the bands into an intermediate VRT
        delete poPansharpener;
        poPansharpener = NULL;

        // Close in reverse order (VRT firsts and real datasets after)
        for(int i=(int)apoDatasetsToClose.size()-1;i>=0;i--)
        {
            bHasDroppedRef = TRUE;
            GDALClose(apoDatasetsToClose[i]);
        }
        apoDatasetsToClose.resize(0);
    }
    
    for( size_t i=0; i<apoOverviewDatasets.size();i++)
    {
        bHasDroppedRef = TRUE;
        delete apoOverviewDatasets[i];
    }
    apoOverviewDatasets.resize(0);
    
    if( poMainDatasetLocal != this )
    {
        // To avoid killing us
        for( size_t i=0; i<poMainDatasetLocal->apoOverviewDatasets.size();i++)
        {
            if( poMainDatasetLocal->apoOverviewDatasets[i] == this )
            {
                poMainDatasetLocal->apoOverviewDatasets[i] = NULL;
                break;
            }
        }
        bHasDroppedRef |= poMainDatasetLocal->CloseDependentDatasets();
    }

    return bHasDroppedRef;
}


/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** VRTPansharpenedDataset::GetFileList()
{
    char** papszFileList = GDALDataset::GetFileList();

    if( poPansharpener != NULL )
    {
        GDALPansharpenOptions* psOptions = poPansharpener->GetOptions();
        if( psOptions != NULL )
        {
            std::set<CPLString> oSetNames;
            GDALDatasetH hDS;
            if( psOptions->hPanchroBand != NULL &&
                (hDS = GDALGetBandDataset(psOptions->hPanchroBand)) != NULL )
            {
                papszFileList = CSLAddString(papszFileList, GDALGetDescription(hDS));
                oSetNames.insert(GDALGetDescription(hDS));
            }
            for(int i=0;i<psOptions->nInputSpectralBands;i++)
            {
                if( psOptions->pahInputSpectralBands[i] != NULL &&
                    (hDS = GDALGetBandDataset(psOptions->pahInputSpectralBands[i])) != NULL &&
                    oSetNames.find(GDALGetDescription(hDS)) == oSetNames.end() )
                {
                    papszFileList = CSLAddString(papszFileList, GDALGetDescription(hDS));
                    oSetNames.insert(GDALGetDescription(hDS));
                }
            }
        }
    }

    return papszFileList;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTPansharpenedDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPath )

{
    return XMLInit(psTree, pszVRTPath, NULL, 0, NULL );
}

CPLErr VRTPansharpenedDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPath,
                                        GDALRasterBandH hPanchroBandIn,
                                        int nInputSpectralBandsIn,
                                        GDALRasterBandH* pahInputSpectralBandsIn )
{
    CPLErr eErr;
    GDALPansharpenOptions* psPanOptions;

/* -------------------------------------------------------------------- */
/*      Initialize blocksize before calling sub-init so that the        */
/*      band initializers can get it from the dataset object when       */
/*      they are created.                                               */
/* -------------------------------------------------------------------- */
    nBlockXSize = atoi(CPLGetXMLValue(psTree,"BlockXSize","512"));
    nBlockYSize = atoi(CPLGetXMLValue(psTree,"BlockYSize","512"));

/* -------------------------------------------------------------------- */
/*      Parse PansharpeningOptions                                      */
/* -------------------------------------------------------------------- */

    CPLXMLNode* psOptions = CPLGetXMLNode(psTree, "PansharpeningOptions");
    if( psOptions == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing PansharpeningOptions");
        return CE_Failure;
    }

    CPLString osSourceFilename;
    GDALDataset* poPanDataset = NULL;
    GDALDataset* poPanDatasetToClose = NULL;
    GDALRasterBand* poPanBand = NULL;
    std::map<CPLString, GDALDataset*> oMapNamesToDataset;
    int nPanBand;

    if( hPanchroBandIn == NULL )
    {
        CPLXMLNode* psPanchroBand = CPLGetXMLNode(psOptions, "PanchroBand");
        if( psPanchroBand == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "PanchroBand missing");
            return CE_Failure;
        }
        
        const char* pszSourceFilename = CPLGetXMLValue(psPanchroBand, "SourceFilename", NULL);
        if( pszSourceFilename == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "PanchroBand.SourceFilename missing");
            return CE_Failure;
        }
        int bRelativeToVRT = atoi(CPLGetXMLValue( psPanchroBand, "SourceFilename.relativetoVRT", "0"));
        if( bRelativeToVRT )
        {
            const char* pszAbs = CPLProjectRelativeFilename( pszVRTPath, pszSourceFilename );
            oMapToRelativeFilenames[pszAbs] = pszSourceFilename;
            pszSourceFilename = pszAbs;
        }
        osSourceFilename = pszSourceFilename;
        poPanDataset = (GDALDataset*)GDALOpen(osSourceFilename, GA_ReadOnly);
        if( poPanDataset == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s not a valid dataset",
                     osSourceFilename.c_str());
            return CE_Failure;
        }
        poPanDatasetToClose = poPanDataset;

        const char* pszSourceBand = CPLGetXMLValue(psPanchroBand,"SourceBand","1");
        nPanBand = atoi(pszSourceBand);
        if( poPanBand == NULL )
            poPanBand = poPanDataset->GetRasterBand(nPanBand);
        if( poPanBand == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s invalid band of %s",
                     pszSourceBand, osSourceFilename.c_str());
            GDALClose(poPanDatasetToClose);
            return CE_Failure;
        }
        oMapNamesToDataset[osSourceFilename] = poPanDataset;
        apoDatasetsToClose.push_back(poPanDataset);
    }
    else
    {
        poPanBand = (GDALRasterBand*)hPanchroBandIn;
        nPanBand = poPanBand->GetBand();
        poPanDataset = poPanBand->GetDataset();
        if( poPanDataset )
            oMapNamesToDataset[CPLSPrintf("%p", poPanDataset)] = poPanDataset;
    }

    // Figure out which kind of adjustment we should do if the pan and spectral
    // bands do not share the same geotransform
    const char* pszGTAdjustment = CPLGetXMLValue(psOptions, "SpatialExtentAdjustment", "Union");
    if( EQUAL(pszGTAdjustment, "Union") )
        eGTAdjustment = GTAdjust_Union;
    else if( EQUAL(pszGTAdjustment, "Intersection") )
        eGTAdjustment = GTAdjust_Intersection;
    else if( EQUAL(pszGTAdjustment, "None") )
        eGTAdjustment = GTAdjust_None;
    else if( EQUAL(pszGTAdjustment, "NoneWithoutWarning") )
        eGTAdjustment = GTAdjust_NoneWithoutWarning;
    else
    {
        eGTAdjustment = GTAdjust_Union;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported value for GeoTransformAdjustment. Defaulting to Union");
    }
    
    const char* pszNumThreads = CPLGetXMLValue(psOptions, "NumThreads", NULL);
    int nThreads = 0;
    if( pszNumThreads != NULL )
    {
        if( EQUAL(pszNumThreads, "ALL_CPUS") )
            nThreads = -1;
        else
            nThreads = atoi(pszNumThreads);
    }

    const char* pszAlgorithm = CPLGetXMLValue(psOptions, 
                                              "Algorithm", "WeightedBrovey");
    if( !EQUAL(pszAlgorithm, "WeightedBrovey") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Algorithm %s unsupported", pszAlgorithm);
        GDALClose(poPanDatasetToClose);
        apoDatasetsToClose.resize(0);
        return CE_Failure;
    }

    std::vector<double> adfWeights;
    CPLXMLNode* psAlgOptions = CPLGetXMLNode(psOptions, "AlgorithmOptions");
    if( psAlgOptions != NULL )
    {
        const char* pszWeights = CPLGetXMLValue(psAlgOptions, "Weights", NULL);
        if( pszWeights != NULL )
        {
            char** papszTokens = CSLTokenizeString2(pszWeights, " ,", 0);
            for(int i=0; papszTokens && papszTokens[i]; i++)
                adfWeights.push_back(CPLAtof(papszTokens[i]));
            CSLDestroy(papszTokens);
        }
    }

    GDALRIOResampleAlg eResampleAlg = GDALRasterIOGetResampleAlg(
                    CPLGetXMLValue(psOptions, "Resampling", "Cubic"));
    
    std::vector<GDALRasterBand*> ahSpectralBands;
    std::map<int, int> aMapDstBandToSpectralBand;
    std::map<int,int>::iterator aMapDstBandToSpectralBandIter;
    int nBitDepth = 0;
    int bFoundNonMachingGT = FALSE;
    double adfPanGT[6];
    int bPanGeoTransformValid = FALSE;
    if( poPanDataset )
        bPanGeoTransformValid = ( poPanDataset->GetGeoTransform(adfPanGT) == CE_None );
    int nPanXSize = poPanBand->GetXSize();
    int nPanYSize = poPanBand->GetYSize();
    double dfMinX = 0.0, dfMinY = 0.0, dfMaxX = 0.0, dfMaxY = 0.0;
    int bFoundRotatingTerms = FALSE;
    int bHasNoData = FALSE;
    double dfNoData = poPanBand->GetNoDataValue(&bHasNoData);
    double dfLRPanX = adfPanGT[0] +
                        nPanXSize * adfPanGT[1] +
                        nPanYSize * adfPanGT[2];
    double dfLRPanY = adfPanGT[3] +
                        nPanXSize * adfPanGT[4] +
                        nPanYSize * adfPanGT[5];
    if( bPanGeoTransformValid )
    {
        bFoundRotatingTerms |= (adfPanGT[2] != 0.0 || adfPanGT[4] != 0.0);
        dfMinX = adfPanGT[0];
        dfMaxX = dfLRPanX;
        dfMaxY = adfPanGT[3];
        dfMinY = dfLRPanY;
    }
    
    CPLString osPanProjection, osPanProjectionProj4;
    if( poPanDataset && poPanDataset->GetProjectionRef() )
    {
        osPanProjection = poPanDataset->GetProjectionRef();
        char* pszProj4 = NULL;
        OGRSpatialReference oSRS(osPanProjection);
        if( oSRS.exportToProj4(&pszProj4) == OGRERR_NONE )
            osPanProjectionProj4 = pszProj4;
        CPLFree(pszProj4);
    }

/* -------------------------------------------------------------------- */
/*      First pass on spectral datasets to check their georeferencing.  */
/* -------------------------------------------------------------------- */
    int iSpectralBand = 0;
    for(CPLXMLNode* psIter = psOptions->psChild; psIter; psIter = psIter->psNext )
    {
        GDALDataset* poDataset;
        if( psIter->eType != CXT_Element || !EQUAL(psIter->pszValue, "SpectralBand") )
            continue;
        
        if( nInputSpectralBandsIn  )
        {
            if( iSpectralBand == nInputSpectralBandsIn )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "More SpectralBand elements than in source array");
                goto error;
            }
            poDataset = ((GDALRasterBand*)pahInputSpectralBandsIn[iSpectralBand])->GetDataset();
            if( poDataset )
                osSourceFilename = poDataset->GetDescription();
            
            oMapNamesToDataset[CPLSPrintf("%p", poDataset)] = poDataset;
        }
        else
        {
            const char* pszSourceFilename = CPLGetXMLValue(psIter, "SourceFilename", NULL);
            if( pszSourceFilename == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "SpectralBand.SourceFilename missing");
                goto error;
            }
            int bRelativeToVRT = atoi(CPLGetXMLValue( psIter, "SourceFilename.relativetoVRT", "0"));
            if( bRelativeToVRT )
            {
                const char* pszAbs = CPLProjectRelativeFilename( pszVRTPath, pszSourceFilename );
                oMapToRelativeFilenames[pszAbs] = pszSourceFilename;
                pszSourceFilename = pszAbs;
            }
            osSourceFilename = pszSourceFilename;
            poDataset = oMapNamesToDataset[osSourceFilename];
            if( poDataset == NULL )
            {
                poDataset = (GDALDataset*)GDALOpen(osSourceFilename, GA_ReadOnly);
                if( poDataset == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s not a valid dataset",
                             osSourceFilename.c_str());
                    goto error;
                }
                oMapNamesToDataset[osSourceFilename] = poDataset;
                apoDatasetsToClose.push_back(poDataset);
            }
        }
        
        if( poDataset != NULL )
        {
            // Check that the spectral band has a georeferencing consistant
            // of the pan band. Allow an error of at most the size of one pixel
            // of the spectral band.
            if( bPanGeoTransformValid )
            {
                CPLString osProjection;
                if( poDataset->GetProjectionRef() )
                    osProjection = poDataset->GetProjectionRef();

                if( osPanProjection.size() )
                {
                    if( osProjection.size() )
                    {
                        if( osPanProjection != osProjection )
                        {
                            CPLString osProjectionProj4;
                            char* pszProj4 = NULL;
                            OGRSpatialReference oSRS(osProjection);
                            if( oSRS.exportToProj4(&pszProj4) == OGRERR_NONE )
                                osProjectionProj4 = pszProj4;
                            CPLFree(pszProj4);

                            if( osPanProjectionProj4 != osProjectionProj4 )
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Pan dataset and %s do not seem to have same projection. Results might be incorrect",
                                         osSourceFilename.c_str());
                            }
                        }
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Pan dataset has a projection, whereas %s not. Results might be incorrect",
                                 osSourceFilename.c_str());
                    }
                }
                else if( osProjection.size() )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Pan dataset has no projection, whereas %s has one. Results might be incorrect",
                             osSourceFilename.c_str());
                }

                double adfSpectralGeoTransform[6];
                if( poDataset->GetGeoTransform(adfSpectralGeoTransform) == CE_None )
                {
                    int bIsThisOneNonMatching = FALSE;
                    double dfPixelSize = MAX(adfSpectralGeoTransform[1], fabs(adfSpectralGeoTransform[5]));
                    if( fabs(adfPanGT[0] - adfSpectralGeoTransform[0]) > dfPixelSize ||
                        fabs(adfPanGT[3] - adfSpectralGeoTransform[3]) > dfPixelSize )
                    {
                        bIsThisOneNonMatching = TRUE;
                        if( eGTAdjustment == GTAdjust_None )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "Georeferencing of top-left corner of pan dataset and %s do not match",
                                    osSourceFilename.c_str());
                        }
                    }
                    bFoundRotatingTerms |= (adfSpectralGeoTransform[2] != 0.0 || adfSpectralGeoTransform[4] != 0.0);
                    double dfLRSpectralX = adfSpectralGeoTransform[0] +
                                      poDataset->GetRasterXSize() * adfSpectralGeoTransform[1] +
                                      poDataset->GetRasterYSize() * adfSpectralGeoTransform[2];
                    double dfLRSpectralY = adfSpectralGeoTransform[3] +
                                      poDataset->GetRasterXSize() * adfSpectralGeoTransform[4] +
                                      poDataset->GetRasterYSize() * adfSpectralGeoTransform[5];
                    if( fabs(dfLRPanX - dfLRSpectralX) > dfPixelSize ||
                        fabs(dfLRPanY - dfLRSpectralY) > dfPixelSize )
                    {
                        bIsThisOneNonMatching = TRUE;
                        if( eGTAdjustment == GTAdjust_None )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "Georeferencing of bottom-right corner of pan dataset and %s do not match",
                                    osSourceFilename.c_str());
                        }
                    }

                    if( bIsThisOneNonMatching && eGTAdjustment == GTAdjust_Union )
                    {
                        dfMinX = MIN(dfMinX, adfSpectralGeoTransform[0]);
                        dfMinY = MIN(dfMinY, dfLRSpectralY);
                        dfMaxX = MAX(dfMaxX, dfLRSpectralX);
                        dfMaxY = MAX(dfMaxY, adfSpectralGeoTransform[3]);
                    }
                    else if( bIsThisOneNonMatching && eGTAdjustment == GTAdjust_Intersection )
                    {
                        dfMinX = MAX(dfMinX, adfSpectralGeoTransform[0]);
                        dfMinY = MAX(dfMinY, dfLRSpectralY);
                        dfMaxX = MIN(dfMaxX, dfLRSpectralX);
                        dfMaxY = MIN(dfMaxY, adfSpectralGeoTransform[3]);
                    }

                    bFoundNonMachingGT |= bIsThisOneNonMatching;
                }
            }
        }
        
        iSpectralBand ++;
    }
    
    if( nInputSpectralBandsIn && iSpectralBand != nInputSpectralBandsIn )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Less SpectralBand elements than in source array");
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      On-the-fly spatial extent adjustment if needed and asked.       */
/* -------------------------------------------------------------------- */

    if( bFoundNonMachingGT &&
            (eGTAdjustment == GTAdjust_Union || eGTAdjustment == GTAdjust_Intersection) )
    {
        if( bFoundRotatingTerms )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "One of the panchromatic or spectral datasets has rotating "
                     "terms in their geotransform matrix. Adjustment not possible");
            goto error;
        }
        if( eGTAdjustment == GTAdjust_Intersection &&
            (dfMinX >= dfMaxX || dfMinY >= dfMaxY) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "One of the panchromatic or spectral datasets has rotating "
                     "terms in their geotransform matrix. Adjustment not possible");
            goto error;
        }
        if( eGTAdjustment == GTAdjust_Union )
            CPLDebug("VRT", "Do union of bounding box of panchromatic and spectral datasets");
        else
            CPLDebug("VRT", "Do intersection of bounding box of panchromatic and spectral datasets");

        // If the pandataset needs adjustments, make sure the coordinates of the
        // union/intersection properly align with the grid of the pandataset
        // to avoid annoying sub-pixel shifts on the panchro band.
        double dfPixelSize = MAX( adfPanGT[1], fabs(adfPanGT[5]) );
        if( fabs(adfPanGT[0] - dfMinX) > dfPixelSize ||
            fabs(adfPanGT[3] - dfMaxY) > dfPixelSize ||
            fabs(dfLRPanX - dfMaxX) > dfPixelSize ||
            fabs(dfLRPanY - dfMinY) > dfPixelSize )
        {
            dfMinX = adfPanGT[0] + floor((dfMinX - adfPanGT[0]) / adfPanGT[1] + 0.5) * adfPanGT[1];
            dfMaxY = adfPanGT[3] + floor((dfMaxY - adfPanGT[3]) / adfPanGT[5] + 0.5) * adfPanGT[5];
            dfMaxX = dfLRPanX + floor((dfMaxX - dfLRPanX) / adfPanGT[1] + 0.5) * adfPanGT[1];
            dfMinY = dfLRPanY + floor((dfMinY - dfLRPanY) / adfPanGT[5] + 0.5) * adfPanGT[5];
        }

        std::map<CPLString, GDALDataset*>::iterator oIter = oMapNamesToDataset.begin();
        for(; oIter != oMapNamesToDataset.end(); ++oIter)
        {
            GDALDataset* poSrcDS = oIter->second;
            double adfGT[6];
            if( poSrcDS->GetGeoTransform(adfGT) != CE_None )
                continue;

            // Check if this dataset needs adjustments
            double dfPixelSize = MAX(adfGT[1], fabs(adfGT[5]));
            dfPixelSize = MAX( adfPanGT[1], dfPixelSize );
            dfPixelSize = MAX( fabs(adfPanGT[5]), dfPixelSize );
            if( fabs(adfGT[0] - dfMinX) <= dfPixelSize &&
                fabs(adfGT[3] - dfMaxY) <= dfPixelSize &&
                fabs(adfGT[0] + poSrcDS->GetRasterXSize() * adfGT[1] - dfMaxX) <= dfPixelSize &&
                fabs(adfGT[3] + poSrcDS->GetRasterYSize() * adfGT[5] - dfMinY) <= dfPixelSize )
            {
                continue;
            }

            double adfAdjustedGT[6];
            adfAdjustedGT[0] = dfMinX;
            adfAdjustedGT[1] = adfGT[1];
            adfAdjustedGT[2] = 0;
            adfAdjustedGT[3] = dfMaxY;
            adfAdjustedGT[4] = 0;
            adfAdjustedGT[5] = adfGT[5];
            int nAdjustRasterXSize = (int)(0.5 + (dfMaxX - dfMinX) / adfAdjustedGT[1]);
            int nAdjustRasterYSize = (int)(0.5 + (dfMaxY - dfMinY) / (-adfAdjustedGT[5]));

            VRTDataset* poVDS = new VRTDataset(nAdjustRasterXSize, nAdjustRasterYSize);
            poVDS->SetWritable(FALSE);
            poVDS->SetDescription(poSrcDS->GetDescription());
            poVDS->SetGeoTransform(adfAdjustedGT);
            poVDS->SetProjection(poPanDataset->GetProjectionRef());

            for(int i=0;i<poSrcDS->GetRasterCount();i++)
            {
                GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand(i+1);
                poVDS->AddBand(poSrcBand->GetRasterDataType(), NULL);
                VRTSourcedRasterBand* poVRTBand = (VRTSourcedRasterBand*) poVDS->GetRasterBand(i+1);

                const char* pszNBITS = poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
                if( pszNBITS )
                    poVRTBand->SetMetadataItem("NBITS", pszNBITS, "IMAGE_STRUCTURE");

                VRTSimpleSource* poSimpleSource = new VRTSimpleSource();
                poVRTBand->ConfigureSource( poSimpleSource,
                                            poSrcBand,
                                            FALSE,
                                            (int)floor((dfMinX - adfGT[0]) / adfGT[1] + 0.001),
                                            (int)floor((dfMaxY - adfGT[3]) / adfGT[5] + 0.001),
                                            (int)(0.5 + (dfMaxX - dfMinX) / adfGT[1]),
                                            (int)(0.5 + (dfMaxY - dfMinY) / (-adfGT[5])),
                                            0, 0,
                                            nAdjustRasterXSize, nAdjustRasterYSize );

                poVRTBand->AddSource( poSimpleSource );
            }

            oIter->second = poVDS;
            if( poSrcDS == poPanDataset )
            {
                memcpy(adfPanGT, adfAdjustedGT, 6*sizeof(double));
                poPanDataset = poVDS;
                poPanBand = poPanDataset->GetRasterBand(nPanBand);
                nPanXSize = poPanDataset->GetRasterXSize();
                nPanYSize = poPanDataset->GetRasterYSize();
            }
            
            apoDatasetsToClose.push_back(poVDS);
        }
    }

    if( nRasterXSize == 0 && nRasterYSize == 0 ) 
    {
        nRasterXSize = nPanXSize;
        nRasterYSize = nPanYSize;
    }
    else if( nRasterXSize != nPanXSize ||
             nRasterYSize != nPanYSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistant declared VRT dimensions with panchro dataset");
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Initialize all the general VRT stuff.  This will even           */
/*      create the VRTPansharpenedRasterBands and initialize them.      */
/* -------------------------------------------------------------------- */
    eErr = VRTDataset::XMLInit( psTree, pszVRTPath );

    if( eErr != CE_None )
    {
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Inherit georeferencing info from panchro band if not defined    */
/*      in VRT.                                                         */
/* -------------------------------------------------------------------- */

    {
        double adfOutGT[6];
        if( GetGeoTransform(adfOutGT) != CE_None &&
            GetGCPCount() == 0 &&
            GetProjectionRef()[0] == '\0' )
        {
            if( bPanGeoTransformValid )
            {
                SetGeoTransform(adfPanGT);
            }
            if( poPanDataset &&
                poPanDataset->GetProjectionRef() != NULL &&
                poPanDataset->GetProjectionRef()[0] != '\0' )
            {
                SetProjection(poPanDataset->GetProjectionRef());
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse rest of PansharpeningOptions                              */
/* -------------------------------------------------------------------- */
    iSpectralBand = 0;
    for(CPLXMLNode* psIter = psOptions->psChild; psIter; psIter = psIter->psNext )
    {
        GDALDataset* poDataset;
        GDALRasterBand* poBand;
        if( psIter->eType != CXT_Element || !EQUAL(psIter->pszValue, "SpectralBand") )
            continue;

        const char* pszDstBand = CPLGetXMLValue(psIter, "dstBand", NULL);
        int nDstBand = -1;
        if( pszDstBand != NULL )
        {
            nDstBand = atoi(pszDstBand);
            if( nDstBand <= 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "SpectralBand.dstBand = '%s' invalid",
                         pszDstBand);
                goto error;
            }
        }
        
        if( nInputSpectralBandsIn  )
        {
            poBand = (GDALRasterBand*)pahInputSpectralBandsIn[iSpectralBand];
            poDataset = poBand->GetDataset();
            if( poDataset )
            {
                poDataset = oMapNamesToDataset[CPLSPrintf("%p", poDataset)];
                CPLAssert(poDataset);
                if( poDataset )
                    poBand = poDataset->GetRasterBand(poBand->GetBand());
            }
        }
        else
        {
            const char* pszSourceFilename = CPLGetXMLValue(psIter, "SourceFilename", NULL);
            CPLAssert(pszSourceFilename);
            int bRelativeToVRT = atoi(CPLGetXMLValue( psIter, "SourceFilename.relativetoVRT", "0"));
            if( bRelativeToVRT )
                pszSourceFilename = CPLProjectRelativeFilename( pszVRTPath, pszSourceFilename );
            osSourceFilename = pszSourceFilename;
            poDataset = oMapNamesToDataset[osSourceFilename];
            CPLAssert(poDataset);
            const char* pszSourceBand = CPLGetXMLValue(psIter,"SourceBand","1");
            int nBand = atoi(pszSourceBand);
            poBand = poDataset->GetRasterBand(nBand);
            if( poBand == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s invalid band of %s",
                        pszSourceBand, osSourceFilename.c_str());
                goto error;
            }
        }
        
        if( bHasNoData )
        {
            double dfSpectralNoData = poPanBand->GetNoDataValue(&bHasNoData);
            if( bHasNoData && dfSpectralNoData != dfNoData )
                bHasNoData = FALSE;
        }

        ahSpectralBands.push_back(poBand);
        if( nDstBand >= 1 )
        {
            if( aMapDstBandToSpectralBand.find(nDstBand-1) != aMapDstBandToSpectralBand.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Another spectral band is already mapped to output band %d",
                         nDstBand);
                goto error;
            }
            aMapDstBandToSpectralBand[nDstBand-1] = (int)ahSpectralBands.size()-1;
        }
        
        iSpectralBand ++;
    }

    if( ahSpectralBands.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No spectral band defined");
        goto error;
    }

    {
        const char* pszNoData = CPLGetXMLValue(psOptions, "NoData", NULL);
        if( pszNoData )
        {
            if( EQUAL(pszNoData, "NONE") )
            {
                bNoDataDisabled = TRUE;
                bHasNoData = FALSE;
            }
            else
            {
                bHasNoData = TRUE;
                dfNoData = CPLAtof(pszNoData);
            }
        }
    }

    if( GetRasterCount() == 0 )
    {
        for(int i=0;i<(int)aMapDstBandToSpectralBand.size();i++)
        {
            if( aMapDstBandToSpectralBand.find(i) == aMapDstBandToSpectralBand.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Hole in SpectralBand.dstBand numbering");
                goto error;
            }
            GDALRasterBand* poInputBand =
                (GDALRasterBand*)ahSpectralBands[aMapDstBandToSpectralBand[i]];
            GDALRasterBand* poBand = new VRTPansharpenedRasterBand(this, i+1,
                                            poInputBand->GetRasterDataType());
            poBand->SetColorInterpretation(poInputBand->GetColorInterpretation());
            if( bHasNoData )
                poBand->SetNoDataValue(dfNoData);
            SetBand(i+1, poBand);
        }
    }
    else
    {
        int nIdxAsPansharpenedBand = 0;
        for(int i=0;i<nBands;i++)
        {
            if( ((VRTRasterBand*)GetRasterBand(i+1))->IsPansharpenRasterBand() )
            {
                if( aMapDstBandToSpectralBand.find(i) == aMapDstBandToSpectralBand.end() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Band %d of type VRTPansharpenedRasterBand, but no corresponding SpectralBand",
                            i+1);
                    goto error;
                }
                else
                {
                    ((VRTPansharpenedRasterBand*)GetRasterBand(i+1))->
                        SetIndexAsPansharpenedBand(nIdxAsPansharpenedBand);
                    nIdxAsPansharpenedBand ++;
                }
            }
        }
    }

    // Figure out bit depth
    {
        const char* pszBitDepth = CPLGetXMLValue(psOptions, "BitDepth", NULL);
        if( pszBitDepth == NULL )
            pszBitDepth = ((GDALRasterBand*)ahSpectralBands[0])->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
        if( pszBitDepth )
            nBitDepth = atoi(pszBitDepth);
        if( nBitDepth )
        {
            for(int i=0;i<nBands;i++)
            {
                if( !((VRTRasterBand*)GetRasterBand(i+1))->IsPansharpenRasterBand() )
                    continue;
                if( GetRasterBand(i+1)->GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == NULL )
                {
                    if( nBitDepth != 8 && nBitDepth != 16 && nBitDepth != 32 )
                    {
                        GetRasterBand(i+1)->SetMetadataItem("NBITS",
                                        CPLSPrintf("%d", nBitDepth), "IMAGE_STRUCTURE");
                    }
                }
                else if( nBitDepth == 8 || nBitDepth == 16 || nBitDepth == 32 )
                {
                    GetRasterBand(i+1)->SetMetadataItem("NBITS", NULL, "IMAGE_STRUCTURE");
                }
            }
        }
    }

    if( GDALGetRasterBandXSize(ahSpectralBands[0]) > GDALGetRasterBandXSize(poPanBand) ||
        GDALGetRasterBandYSize(ahSpectralBands[0]) > GDALGetRasterBandYSize(poPanBand) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Dimensions of spectral band larger than panchro band");
    }

    aMapDstBandToSpectralBandIter = aMapDstBandToSpectralBand.begin();
    for(; aMapDstBandToSpectralBandIter != aMapDstBandToSpectralBand.end();
          ++aMapDstBandToSpectralBandIter )
    {
        int nDstBand = 1 + aMapDstBandToSpectralBandIter->first;
        if( nDstBand > nBands ||
            !((VRTRasterBand*)GetRasterBand(nDstBand))->IsPansharpenRasterBand() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SpectralBand.dstBand = '%d' invalid",
                        nDstBand);
            goto error;
        }
    }

    if( adfWeights.size() == 0 )
    {
        for(int i=0;i<(int)ahSpectralBands.size();i++)
        {
            adfWeights.push_back(1.0 / ahSpectralBands.size());
        }
    }
    else if( adfWeights.size() != ahSpectralBands.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%d weights defined, but %d input spectral bands",
                 (int)adfWeights.size(),
                 (int)ahSpectralBands.size());
        goto error;
    }
    
    if( aMapDstBandToSpectralBand.size() == 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No spectral band is mapped to an output band");
    }

/* -------------------------------------------------------------------- */
/*      Instanciate poPansharpener                                      */
/* -------------------------------------------------------------------- */
    psPanOptions = GDALCreatePansharpenOptions();
    psPanOptions->ePansharpenAlg = GDAL_PSH_WEIGHTED_BROVEY;
    psPanOptions->eResampleAlg = eResampleAlg;
    psPanOptions->nBitDepth = nBitDepth;
    psPanOptions->nWeightCount = (int)adfWeights.size();
    psPanOptions->padfWeights = (double*)CPLMalloc(sizeof(double)*adfWeights.size());
    memcpy(psPanOptions->padfWeights, &adfWeights[0],
           sizeof(double)*adfWeights.size());
    psPanOptions->hPanchroBand = poPanBand;
    psPanOptions->nInputSpectralBands = (int)ahSpectralBands.size();
    psPanOptions->pahInputSpectralBands =
        (GDALRasterBandH*)CPLMalloc(sizeof(GDALRasterBandH)*ahSpectralBands.size());
    memcpy(psPanOptions->pahInputSpectralBands, &ahSpectralBands[0],
           sizeof(GDALRasterBandH)*ahSpectralBands.size());
    psPanOptions->nOutPansharpenedBands = (int)aMapDstBandToSpectralBand.size();
    psPanOptions->panOutPansharpenedBands =
            (int*)CPLMalloc(sizeof(int)*aMapDstBandToSpectralBand.size());
    aMapDstBandToSpectralBandIter = aMapDstBandToSpectralBand.begin();
    for(int i = 0; aMapDstBandToSpectralBandIter != aMapDstBandToSpectralBand.end();
          ++aMapDstBandToSpectralBandIter, ++i )
    {
        psPanOptions->panOutPansharpenedBands[i] = aMapDstBandToSpectralBandIter->second;
    }
    psPanOptions->bHasNoData = bHasNoData;
    psPanOptions->dfNoData = dfNoData;
    psPanOptions->nThreads = nThreads;
    psPanOptions->dfMSShiftX = CPLAtof(CPLGetXMLValue(psOptions, "MSShiftX", "0"));
    psPanOptions->dfMSShiftY = CPLAtof(CPLGetXMLValue(psOptions, "MSShiftY", "0"));

    if( nBands == psPanOptions->nOutPansharpenedBands )
        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    poPansharpener = new GDALPansharpenOperation();
    eErr = poPansharpener->Initialize(psPanOptions);
    if( eErr != CE_None )
    {
        // Close in reverse order (VRT firsts and real datasets after)
        for(int i=(int)apoDatasetsToClose.size()-1;i>=0;i--)
        {
            GDALClose(apoDatasetsToClose[i]);
        }
        apoDatasetsToClose.resize(0);

        delete poPansharpener;
        poPansharpener = NULL;
    }
    GDALDestroyPansharpenOptions(psPanOptions);

    return eErr;
    
error:
    // Close in reverse order (VRT firsts and real datasets after)
    for(int i=(int)apoDatasetsToClose.size()-1;i>=0;i--)
    {
        GDALClose(apoDatasetsToClose[i]);
    }
    apoDatasetsToClose.resize(0);
    return CE_Failure;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTPansharpenedDataset::SerializeToXML( const char *pszVRTPath )

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
        CXT_Text, "VRTPansharpenedDataset" );

/* -------------------------------------------------------------------- */
/*      Serialize the block size.                                       */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( psTree, "BlockXSize",
                                 CPLSPrintf( "%d", nBlockXSize ) );
    CPLCreateXMLElementAndValue( psTree, "BlockYSize",
                                 CPLSPrintf( "%d", nBlockYSize ) );

/* -------------------------------------------------------------------- */
/*      Serialize the options.                                          */
/* -------------------------------------------------------------------- */
    if( poPansharpener == NULL )
        return psTree;
    GDALPansharpenOptions* psOptions = poPansharpener->GetOptions();
    if( psOptions == NULL )
        return psTree;

    CPLXMLNode* psOptionsNode = CPLCreateXMLNode(psTree, CXT_Element, "PansharpeningOptions");

    if( psOptions->ePansharpenAlg == GDAL_PSH_WEIGHTED_BROVEY )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "Algorithm", "WeightedBrovey" );
    }
    else
    {
        CPLAssert(FALSE);
    }
    if( psOptions->nWeightCount )
    {
        CPLString osWeights;
        for(int i=0;i<psOptions->nWeightCount;i++)
        {
            if( i ) osWeights += ",";
            osWeights += CPLSPrintf("%.16g", psOptions->padfWeights[i]);
        }
        CPLCreateXMLElementAndValue(
            CPLCreateXMLNode(psOptionsNode, CXT_Element, "AlgorithmOptions"),
            "Weights", osWeights.c_str() );
    }
    CPLCreateXMLElementAndValue( psOptionsNode, "Resampling",
                                 GDALRasterIOGetResampleAlg(psOptions->eResampleAlg) );

    if( psOptions->nThreads == -1 )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "NumThreads", "ALL_CPUS" );
    }
    else if( psOptions->nThreads > 1 )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "NumThreads",
                                     CPLSPrintf("%d", psOptions->nThreads) );
    }
    
    
    if( psOptions->nBitDepth )
        CPLCreateXMLElementAndValue( psOptionsNode, "BitDepth",
                                     CPLSPrintf("%d", psOptions->nBitDepth) );

    const char* pszAdjust = NULL;
    switch( eGTAdjustment )
    {
        case GTAdjust_Union:
            pszAdjust = "Union";
            break;
        case GTAdjust_Intersection:
            pszAdjust = "Intersection";
            break;
        case GTAdjust_None:
            pszAdjust = "None";
            break;
        case GTAdjust_NoneWithoutWarning:
            pszAdjust = "NoneWithoutWarning";
            break;
        default:
            break;
    }

    if( psOptions->bHasNoData )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "NoData",
                                     CPLSPrintf("%.16g", psOptions->dfNoData) );
    }
    else if( bNoDataDisabled )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "NoData", "None" );
    }
    
    if( psOptions->dfMSShiftX )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "MSShiftX",
                                     CPLSPrintf("%.16g", psOptions->dfMSShiftX) );
    }
    if( psOptions->dfMSShiftY )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "MSShiftY",
                                     CPLSPrintf("%.16g", psOptions->dfMSShiftY) );
    }

    if( pszAdjust )
        CPLCreateXMLElementAndValue( psOptionsNode, "SpatialExtentAdjustment", pszAdjust);

    if( psOptions->hPanchroBand )
    {
         CPLXMLNode* psBand = CPLCreateXMLNode(psOptionsNode, CXT_Element, "PanchroBand");
         GDALRasterBand* poBand = (GDALRasterBand*)psOptions->hPanchroBand;
         if( poBand->GetDataset() )
         {
             std::map<CPLString,CPLString>::iterator oIter = 
                oMapToRelativeFilenames.find(poBand->GetDataset()->GetDescription());
             if( oIter == oMapToRelativeFilenames.end() )
             {
                CPLCreateXMLElementAndValue( psBand, "SourceFilename", poBand->GetDataset()->GetDescription() );
             }
             else
             {
                 CPLXMLNode* psSourceFilename =
                    CPLCreateXMLElementAndValue( psBand, "SourceFilename", oIter->second );
                 CPLCreateXMLNode( 
                    CPLCreateXMLNode( psSourceFilename, 
                                    CXT_Attribute, "relativeToVRT" ), 
                    CXT_Text, "1" );
             }
             CPLCreateXMLElementAndValue( psBand, "SourceBand", CPLSPrintf("%d", poBand->GetBand()) );
         }
    }
    for(int i=0;i<psOptions->nInputSpectralBands;i++)
    {
        CPLXMLNode* psBand = CPLCreateXMLNode(psOptionsNode, CXT_Element, "SpectralBand");

        for(int j=0;j<psOptions->nOutPansharpenedBands;j++)
        {
            if( psOptions->panOutPansharpenedBands[j] == i )
            {
                for(int k=0;k<nBands;k++)
                {
                    if( ((VRTRasterBand*)GetRasterBand(k+1))->IsPansharpenRasterBand() )
                    {
                        if( ((VRTPansharpenedRasterBand*)GetRasterBand(k+1))->GetIndexAsPansharpenedBand() == j )
                        {
                            CPLCreateXMLNode( 
                                CPLCreateXMLNode( psBand, 
                                                CXT_Attribute, "dstBand" ), 
                                CXT_Text, CPLSPrintf("%d", k+1) );
                            break;
                        }
                    }
                }
                break;
            }
        }

        GDALRasterBand* poBand = (GDALRasterBand*)psOptions->pahInputSpectralBands[i];
        if( poBand->GetDataset() )
        {
            std::map<CPLString,CPLString>::iterator oIter = 
                oMapToRelativeFilenames.find(poBand->GetDataset()->GetDescription());
            if( oIter == oMapToRelativeFilenames.end() )
            {
                CPLCreateXMLElementAndValue( psBand, "SourceFilename", poBand->GetDataset()->GetDescription() );
            }
            else
            {
                CPLXMLNode* psSourceFilename =
                    CPLCreateXMLElementAndValue( psBand, "SourceFilename", oIter->second );
                CPLCreateXMLNode( 
                    CPLCreateXMLNode( psSourceFilename, 
                                      CXT_Attribute, "relativeToVRT" ), 
                    CXT_Text, "1" );
            }
            CPLCreateXMLElementAndValue( psBand, "SourceBand", CPLSPrintf("%d", poBand->GetBand()) );
        }
    }

    return psTree;
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

void VRTPansharpenedDataset::GetBlockSize( int *pnBlockXSize, int *pnBlockYSize )

{
    assert( NULL != pnBlockXSize );
    assert( NULL != pnBlockYSize );

    *pnBlockXSize = nBlockXSize;
    *pnBlockYSize = nBlockYSize;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTPansharpenedDataset::AddBand( CPL_UNUSED GDALDataType eType,
                                        CPL_UNUSED char **papszOptions )

{
    CPLError(CE_Failure, CPLE_NotSupported, "AddBand() not supported");

    return CE_Failure;
}


/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr VRTPansharpenedDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    if( eRWFlag == GF_Write )
        return CE_Failure;

    /* Try to pass the request to the most appropriate overview dataset */
    if( nBufXSize < nXSize && nBufYSize < nYSize )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace,
                                    nBandSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;
    }

    int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
    if( nXSize == nBufXSize &&
        nYSize == nBufYSize &&
        nDataTypeSize == nPixelSpace &&
        nLineSpace == nPixelSpace * nBufXSize &&
        nBandSpace == nLineSpace * nBufYSize &&
        nBandCount == nBands )
    {
        for(int i=0;i<nBands;i++)
        {
            if( panBandMap[i] != i + 1 ||
                !((VRTRasterBand*)GetRasterBand(i+1))->IsPansharpenRasterBand() )
            {
                goto default_path;
            }
        }

        //{static int bDone = 0; if (!bDone) printf("(2)\n"); bDone = 1; }
        return poPansharpener->ProcessRegion(
                    nXOff, nYOff, nXSize, nYSize, pData, eBufType);

    }

default_path:
    //{static int bDone = 0; if (!bDone) printf("(3)\n"); bDone = 1; }
    return VRTDataset::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
}

/************************************************************************/
/* ==================================================================== */
/*                        VRTPansharpenedRasterBand                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTPansharpenedRasterBand()                   */
/************************************************************************/

VRTPansharpenedRasterBand::VRTPansharpenedRasterBand( GDALDataset *poDS, int nBand,
                                                      GDALDataType eDataType )

{
    Initialize( poDS->GetRasterXSize(), poDS->GetRasterYSize() );

    this->poDS = poDS;
    this->nBand = nBand;
    this->eAccess = GA_Update;
    this->eDataType = eDataType;
    nIndexAsPansharpenedBand = nBand - 1;

    ((VRTPansharpenedDataset *) poDS)->GetBlockSize( &nBlockXSize, 
                                                     &nBlockYSize );

}

/************************************************************************/
/*                        ~VRTPansharpenedRasterBand()                  */
/************************************************************************/

VRTPansharpenedRasterBand::~VRTPansharpenedRasterBand()

{
    FlushCache();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTPansharpenedRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                              void * pImage )

{
    int nReqXOff = nBlockXOff * nBlockXSize;
    int nReqYOff = nBlockYOff * nBlockYSize;
    int nReqXSize = nBlockXSize;
    int nReqYSize = nBlockYSize;
    if( nReqXOff + nReqXSize > nRasterXSize )
        nReqXSize = nRasterXSize - nReqXOff;
    if( nReqYOff + nReqYSize > nRasterYSize )
        nReqYSize = nRasterYSize - nReqYOff;

    //{static int bDone = 0; if (!bDone) printf("(4)\n"); bDone = 1; }
    int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    if( IRasterIO( GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                   pImage, nReqXSize, nReqYSize, eDataType,
                   nDataTypeSize, nDataTypeSize * nReqXSize,
                   &sExtraArg ) != CE_None )
    {
        return CE_Failure;
    }

    if( nReqXSize < nBlockXSize )
    {
        for(int j=nReqYSize-1;j>=0;j--)
        {
            memmove( (GByte*)pImage + j * nDataTypeSize * nBlockXSize,
                     (GByte*)pImage + j * nDataTypeSize * nReqXSize,
                     nReqXSize * nDataTypeSize );
            memset( (GByte*)pImage + (j * nBlockXSize + nReqXSize) * nDataTypeSize,
                    0,
                    (nBlockXSize - nReqXSize) * nDataTypeSize );
        }
    }
    if( nReqYSize < nBlockYSize )
    {
        memset( (GByte*)pImage + nReqYSize * nBlockXSize * nDataTypeSize,
                0,
                (nBlockYSize - nReqYSize ) * nBlockXSize * nDataTypeSize );
    }

    // Cache other bands
    CPLErr eErr = CE_None;
    VRTPansharpenedDataset* poGDS = (VRTPansharpenedDataset *) poDS;
    if( poGDS->nBands != 1 && !poGDS->bLoadingOtherBands )
    {
        int iOtherBand;

        poGDS->bLoadingOtherBands = TRUE;

        for( iOtherBand = 1; iOtherBand <= poGDS->nBands; iOtherBand++ )
        {
            if( iOtherBand == nBand )
                continue;

            GDALRasterBlock *poBlock;

            poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff);
            if (poBlock == NULL)
            {
                eErr = CE_Failure;
                break;
            }
            poBlock->DropLock();
        }

        poGDS->bLoadingOtherBands = FALSE;
    }

    return eErr;
}

/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr VRTPansharpenedRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    VRTPansharpenedDataset* poGDS = (VRTPansharpenedDataset *) poDS;

    if( eRWFlag == GF_Write )
        return CE_Failure;

    /* Try to pass the request to the most appropriate overview dataset */
    if( nBufXSize < nXSize && nBufYSize < nYSize )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nPixelSpace, nLineSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;
    }

    int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
    if( nXSize == nBufXSize &&
        nYSize == nBufYSize &&
        nDataTypeSize == nPixelSpace &&
        nLineSpace == nPixelSpace * nBufXSize )
    {
        GDALPansharpenOptions* psOptions = poGDS->poPansharpener->GetOptions();

        // Have we already done this request for another band ?
        // If so use the cached result
        size_t nBufferSizePerBand = (size_t)nXSize * nYSize * nDataTypeSize;
        if( nXOff == poGDS->nLastBandRasterIOXOff &&
            nYOff >= poGDS->nLastBandRasterIOYOff &&
            nXSize == poGDS->nLastBandRasterIOXSize &&
            nYOff + nYSize <= poGDS->nLastBandRasterIOYOff + poGDS->nLastBandRasterIOYSize &&
            eBufType == poGDS->eLastBandRasterIODataType )
        {
            //{static int bDone = 0; if (!bDone) printf("(6)\n"); bDone = 1; }
            if( poGDS->pabyLastBufferBandRasterIO == NULL )
                return CE_Failure;
            size_t nBufferSizePerBandCached = (size_t)nXSize * poGDS->nLastBandRasterIOYSize * nDataTypeSize;
            memcpy(pData,
                   poGDS->pabyLastBufferBandRasterIO +
                        nBufferSizePerBandCached * nIndexAsPansharpenedBand +
                        (nYOff - poGDS->nLastBandRasterIOYOff) * nXSize * nDataTypeSize,
                   nBufferSizePerBand);
            return CE_None;
        }

        int nYSizeToCache = nYSize;
        if( nYSize == 1 && nXSize == nRasterXSize )
        {
            //{static int bDone = 0; if (!bDone) printf("(7)\n"); bDone = 1; }
            // For efficiency, try to cache at leak 256 K
            nYSizeToCache = (256 * 1024) / (nXSize * nDataTypeSize);
            if( nYSizeToCache == 0 )
                nYSizeToCache = 1;
            else if( nYOff + nYSizeToCache > nRasterYSize )
                nYSizeToCache = nRasterYSize - nYOff;
        }
        GIntBig nBufferSize = (GIntBig)nXSize * nYSizeToCache * nDataTypeSize * psOptions->nOutPansharpenedBands;
        if( (GIntBig)(size_t)nBufferSize != nBufferSize )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory error while allocating working buffers");
            return CE_Failure;
        }
        GByte* pabyTemp = (GByte*)VSIRealloc(poGDS->pabyLastBufferBandRasterIO,
                                             (size_t)nBufferSize);
        if( pabyTemp == NULL )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory error while allocating working buffers");
            return CE_Failure;
        }
        poGDS->nLastBandRasterIOXOff = nXOff;
        poGDS->nLastBandRasterIOYOff = nYOff;
        poGDS->nLastBandRasterIOXSize = nXSize;
        poGDS->nLastBandRasterIOYSize = nYSizeToCache;
        poGDS->eLastBandRasterIODataType = eBufType;
        poGDS->pabyLastBufferBandRasterIO = pabyTemp;

        CPLErr eErr = poGDS->poPansharpener->ProcessRegion(
                            nXOff, nYOff, nXSize, nYSizeToCache,
                            poGDS->pabyLastBufferBandRasterIO, eBufType);
        if( eErr == CE_None )
        {
            //{static int bDone = 0; if (!bDone) printf("(8)\n"); bDone = 1; }
            size_t nBufferSizePerBandCached = (size_t)nXSize * poGDS->nLastBandRasterIOYSize * nDataTypeSize;
            memcpy(pData,
                   poGDS->pabyLastBufferBandRasterIO +
                        nBufferSizePerBandCached * nIndexAsPansharpenedBand,
                   nBufferSizePerBand);
        }
        else
        {
            VSIFree(poGDS->pabyLastBufferBandRasterIO);
            poGDS->pabyLastBufferBandRasterIO = NULL;
        }
        return eErr;
    }

    //{static int bDone = 0; if (!bDone) printf("(9)\n"); bDone = 1; }
    CPLErr eErr = VRTRasterBand::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nPixelSpace, nLineSpace, psExtraArg);

    return eErr;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTPansharpenedRasterBand::XMLInit( CPLXMLNode * psTree, 
                                  const char *pszVRTPath )

{
    return VRTRasterBand::XMLInit( psTree, pszVRTPath );
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTPansharpenedRasterBand::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode *psTree;

    psTree = VRTRasterBand::SerializeToXML( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Set subclass.                                                   */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode( 
        CPLCreateXMLNode( psTree, CXT_Attribute, "subClass" ), 
        CXT_Text, "VRTPansharpenedRasterBand" );

    return psTree;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int VRTPansharpenedRasterBand::GetOverviewCount()
{
    VRTPansharpenedDataset* poGDS = (VRTPansharpenedDataset *) poDS;

    // Build on-the-fly overviews from overviews of pan and spectral bands
    if( poGDS->poPansharpener != NULL &&
        poGDS->apoOverviewDatasets.size() == 0 &&
        poGDS->poMainDataset == poGDS )
    {
        GDALPansharpenOptions* psOptions = poGDS->poPansharpener->GetOptions();

        GDALRasterBand* poPanBand = (GDALRasterBand*)psOptions->hPanchroBand;
        int nPanOvrCount = poPanBand->GetOverviewCount();
        if( nPanOvrCount > 0 )
        {
            for(int i=0;i<poGDS->GetRasterCount();i++)
            {
                if( !((VRTRasterBand*)poGDS->GetRasterBand(i+1))->IsPansharpenRasterBand() )
                {
                    return 0;
                }
            }

            int nSpectralOvrCount = ((GDALRasterBand*)psOptions->pahInputSpectralBands[0])->GetOverviewCount();
            // JP2KAK overviews are not bound to a dataset, so let the full resolution bands
            // and rely on JP2KAK IRasterIO() to select the appropriate resolution
            if( nSpectralOvrCount && ((GDALRasterBand*)psOptions->pahInputSpectralBands[0])->GetOverview(0)->GetDataset() == NULL )
                nSpectralOvrCount = 0;
            for(int i=1;i<psOptions->nInputSpectralBands;i++)
            {
                if( ((GDALRasterBand*)psOptions->pahInputSpectralBands[i])->GetOverviewCount() != nSpectralOvrCount )
                {
                    nSpectralOvrCount = 0;
                    break;
                }
            }
            for(int j=0;j<nPanOvrCount;j++)
            {
                GDALRasterBand* poPanOvrBand = poPanBand->GetOverview(j);
                VRTPansharpenedDataset* poOvrDS = new VRTPansharpenedDataset(poPanOvrBand->GetXSize(),
                                                                            poPanOvrBand->GetYSize());
                poOvrDS->poMainDataset = poGDS;
                for(int i=0;i<poGDS->GetRasterCount();i++)
                {
                    GDALRasterBand* poSrcBand = poGDS->GetRasterBand(i+1);
                    GDALRasterBand* poBand = new VRTPansharpenedRasterBand(poOvrDS, i+1,
                                                poSrcBand->GetRasterDataType());
                    const char* pszNBITS = poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
                    if( pszNBITS )
                        poBand->SetMetadataItem("NBITS", pszNBITS, "IMAGE_STRUCTURE");
                    poOvrDS->SetBand(i+1, poBand);
                }

                GDALPansharpenOptions* psPanOvrOptions = GDALClonePansharpenOptions(psOptions);
                psPanOvrOptions->hPanchroBand = poPanOvrBand;
                if( nSpectralOvrCount > 0 )
                {
                    for(int i=0;i<psOptions->nInputSpectralBands;i++)
                    {
                        psPanOvrOptions->pahInputSpectralBands[i] =
                            ((GDALRasterBand*)psOptions->pahInputSpectralBands[i])->GetOverview(
                                    (j < nSpectralOvrCount) ? j : nSpectralOvrCount - 1 );
                    }
                }
                poOvrDS->poPansharpener = new GDALPansharpenOperation();
                CPLErr eErr = poOvrDS->poPansharpener->Initialize(psPanOvrOptions);
                CPLAssert( eErr == CE_None );
                GDALDestroyPansharpenOptions(psPanOvrOptions);

                poOvrDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

                poGDS->apoOverviewDatasets.push_back(poOvrDS);
            }
        }
    }
    return (int)poGDS->apoOverviewDatasets.size();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand* VRTPansharpenedRasterBand::GetOverview(int iOvr)
{
    if( iOvr < 0 || iOvr >= GetOverviewCount() )
        return NULL;
    VRTPansharpenedDataset* poGDS = (VRTPansharpenedDataset *) poDS;
    return poGDS->apoOverviewDatasets[iOvr]->GetRasterBand(nBand);
}
