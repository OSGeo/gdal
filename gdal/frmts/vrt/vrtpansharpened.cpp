/******************************************************************************
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

#include "cpl_port.h"
#include "gdal_vrt.h"
#include "vrtdataset.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdalpansharpen.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

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
    VALIDATE_POINTER1( pszXML, "GDALCreatePansharpenedVRT", nullptr );
    VALIDATE_POINTER1( hPanchroBand, "GDALCreatePansharpenedVRT", nullptr );
    VALIDATE_POINTER1( pahInputSpectralBands, "GDALCreatePansharpenedVRT", nullptr );

    CPLXMLNode* psTree = CPLParseXMLString(pszXML);
    if( psTree == nullptr )
        return nullptr;
    VRTPansharpenedDataset* poDS = new VRTPansharpenedDataset(0,0);
    CPLErr eErr = poDS->XMLInit(psTree, nullptr, hPanchroBand,
                                  nInputSpectralBands, pahInputSpectralBands);
    CPLDestroyXMLNode(psTree);
    if( eErr != CE_None )
    {
        delete poDS;
        return nullptr;
    }
    return GDALDataset::ToHandle(poDS);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/* ==================================================================== */
/*                        VRTPansharpenedDataset                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                       VRTPansharpenedDataset()                       */
/************************************************************************/

VRTPansharpenedDataset::VRTPansharpenedDataset( int nXSize, int nYSize ) :
    VRTDataset( nXSize, nYSize ),
    m_nBlockXSize(std::min(nXSize, 512)),
    m_nBlockYSize(std::min(nYSize, 512)),
    m_poPansharpener(nullptr),
    m_poMainDataset(nullptr),
    m_bLoadingOtherBands(FALSE),
    m_pabyLastBufferBandRasterIO(nullptr),
    m_nLastBandRasterIOXOff(0),
    m_nLastBandRasterIOYOff(0),
    m_nLastBandRasterIOXSize(0),
    m_nLastBandRasterIOYSize(0),
    m_eLastBandRasterIODataType(GDT_Unknown),
    m_eGTAdjustment(GTAdjust_Union),
    m_bNoDataDisabled(FALSE)
{
    eAccess = GA_Update;
    m_poMainDataset = this;
}

/************************************************************************/
/*                    ~VRTPansharpenedDataset()                         */
/************************************************************************/

VRTPansharpenedDataset::~VRTPansharpenedDataset()

{
    VRTPansharpenedDataset::FlushCache(true);
    VRTPansharpenedDataset::CloseDependentDatasets();
    CPLFree(m_pabyLastBufferBandRasterIO);
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTPansharpenedDataset::CloseDependentDatasets()
{
    if( m_poMainDataset == nullptr )
        return FALSE;

    VRTPansharpenedDataset* poMainDatasetLocal = m_poMainDataset;
    m_poMainDataset = nullptr;
    int bHasDroppedRef = VRTDataset::CloseDependentDatasets();

/* -------------------------------------------------------------------- */
/*      Destroy the raster bands if they exist.                         */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    // Destroy the overviews before m_poPansharpener as they might reference
    // files that are in m_apoDatasetsToClose.
    for( size_t i=0; i<m_apoOverviewDatasets.size();i++)
    {
        bHasDroppedRef = TRUE;
        delete m_apoOverviewDatasets[i];
    }
    m_apoOverviewDatasets.resize(0);

    if( m_poPansharpener != nullptr )
    {
        // Delete the pansharper object before closing the dataset
        // because it may have warped the bands into an intermediate VRT
        delete m_poPansharpener;
        m_poPansharpener = nullptr;

        // Close in reverse order (VRT firsts and real datasets after)
        for( int i = static_cast<int>( m_apoDatasetsToClose.size() ) - 1;
             i >= 0;
             i--)
        {
            bHasDroppedRef = TRUE;
            GDALClose(m_apoDatasetsToClose[i]);
        }
        m_apoDatasetsToClose.resize(0);
    }

    if( poMainDatasetLocal != this )
    {
        // To avoid killing us
        for( size_t i=0; i<poMainDatasetLocal->m_apoOverviewDatasets.size();i++)
        {
            if( poMainDatasetLocal->m_apoOverviewDatasets[i] == this )
            {
                poMainDatasetLocal->m_apoOverviewDatasets[i] = nullptr;
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

    if( m_poPansharpener != nullptr )
    {
        GDALPansharpenOptions* psOptions = m_poPansharpener->GetOptions();
        if( psOptions != nullptr )
        {
            std::set<CPLString> oSetNames;
            if( psOptions->hPanchroBand != nullptr )
            {
                GDALDatasetH hDS = GDALGetBandDataset(psOptions->hPanchroBand);
                if( hDS != nullptr )
                {
                    papszFileList = CSLAddString(papszFileList, GDALGetDescription(hDS));
                    oSetNames.insert(GDALGetDescription(hDS));
                }
            }
            for(int i=0;i<psOptions->nInputSpectralBands;i++)
            {
                if( psOptions->pahInputSpectralBands[i] != nullptr )
                {
                    GDALDatasetH hDS = GDALGetBandDataset(psOptions->pahInputSpectralBands[i]);
                    if( hDS != nullptr && oSetNames.find(GDALGetDescription(hDS)) == oSetNames.end() )
                    {
                        papszFileList = CSLAddString(papszFileList, GDALGetDescription(hDS));
                        oSetNames.insert(GDALGetDescription(hDS));
                    }
                }
            }
        }
    }

    return papszFileList;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTPansharpenedDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPathIn )

{
    return XMLInit(psTree, pszVRTPathIn, nullptr, 0, nullptr );
}

CPLErr VRTPansharpenedDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPathIn,
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
    m_nBlockXSize = atoi(CPLGetXMLValue(psTree,"BlockXSize","512"));
    m_nBlockYSize = atoi(CPLGetXMLValue(psTree,"BlockYSize","512"));

/* -------------------------------------------------------------------- */
/*      Parse PansharpeningOptions                                      */
/* -------------------------------------------------------------------- */

    CPLXMLNode* psOptions = CPLGetXMLNode(psTree, "PansharpeningOptions");
    if( psOptions == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing PansharpeningOptions");
        return CE_Failure;
    }

    CPLString osSourceFilename;
    GDALDataset* poPanDataset = nullptr;
    GDALDataset* poPanDatasetToClose = nullptr;
    GDALRasterBand* poPanBand = nullptr;
    std::map<CPLString, GDALDataset*> oMapNamesToDataset;
    int nPanBand;

    if( hPanchroBandIn == nullptr )
    {
        CPLXMLNode* psPanchroBand = CPLGetXMLNode(psOptions, "PanchroBand");
        if( psPanchroBand == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "PanchroBand missing");
            return CE_Failure;
        }

        const char* pszSourceFilename = CPLGetXMLValue(psPanchroBand, "SourceFilename", nullptr);
        if( pszSourceFilename == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "PanchroBand.SourceFilename missing");
            return CE_Failure;
        }
        const bool bRelativeToVRT = CPL_TO_BOOL(atoi(CPLGetXMLValue(
            psPanchroBand, "SourceFilename.relativetoVRT", "0")));
        if( bRelativeToVRT )
        {
            const char* pszAbs = CPLProjectRelativeFilename( pszVRTPathIn, pszSourceFilename );
            m_oMapToRelativeFilenames[pszAbs] = pszSourceFilename;
            pszSourceFilename = pszAbs;
        }
        osSourceFilename = pszSourceFilename;
        poPanDataset = GDALDataset::FromHandle(
            GDALOpen( osSourceFilename, GA_ReadOnly ) );
        if( poPanDataset == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s not a valid dataset",
                     osSourceFilename.c_str());
            return CE_Failure;
        }
        poPanDatasetToClose = poPanDataset;

        const char* pszSourceBand = CPLGetXMLValue(psPanchroBand,"SourceBand","1");
        nPanBand = atoi(pszSourceBand);
        if( poPanBand == nullptr )
            poPanBand = poPanDataset->GetRasterBand(nPanBand);
        if( poPanBand == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s invalid band of %s",
                     pszSourceBand, osSourceFilename.c_str());
            GDALClose(poPanDatasetToClose);
            return CE_Failure;
        }
        oMapNamesToDataset[osSourceFilename] = poPanDataset;
        m_apoDatasetsToClose.push_back(poPanDataset);
    }
    else
    {
        poPanBand = GDALRasterBand::FromHandle( hPanchroBandIn );
        nPanBand = poPanBand->GetBand();
        poPanDataset = poPanBand->GetDataset();
        if( poPanDataset )
            oMapNamesToDataset[CPLSPrintf("%p", poPanDataset)] = poPanDataset;
    }

    // Figure out which kind of adjustment we should do if the pan and spectral
    // bands do not share the same geotransform
    const char* pszGTAdjustment = CPLGetXMLValue(psOptions, "SpatialExtentAdjustment", "Union");
    if( EQUAL(pszGTAdjustment, "Union") )
        m_eGTAdjustment = GTAdjust_Union;
    else if( EQUAL(pszGTAdjustment, "Intersection") )
        m_eGTAdjustment = GTAdjust_Intersection;
    else if( EQUAL(pszGTAdjustment, "None") )
        m_eGTAdjustment = GTAdjust_None;
    else if( EQUAL(pszGTAdjustment, "NoneWithoutWarning") )
        m_eGTAdjustment = GTAdjust_NoneWithoutWarning;
    else
    {
        m_eGTAdjustment = GTAdjust_Union;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported value for GeoTransformAdjustment. Defaulting to Union");
    }

    const char* pszNumThreads = CPLGetXMLValue(psOptions, "NumThreads", nullptr);
    int nThreads = 0;
    if( pszNumThreads != nullptr )
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
        m_apoDatasetsToClose.resize(0);
        return CE_Failure;
    }

    std::vector<double> adfWeights;
    CPLXMLNode* psAlgOptions = CPLGetXMLNode(psOptions, "AlgorithmOptions");
    if( psAlgOptions != nullptr )
    {
        const char* pszWeights = CPLGetXMLValue(psAlgOptions, "Weights", nullptr);
        if( pszWeights != nullptr )
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
    int bFoundNonMatchingGT = FALSE;
    double adfPanGT[6] = { 0, 0, 0, 0, 0, 0 };
    int bPanGeoTransformValid = FALSE;
    if( poPanDataset )
        bPanGeoTransformValid = ( poPanDataset->GetGeoTransform(adfPanGT) == CE_None );
    int nPanXSize = poPanBand->GetXSize();
    int nPanYSize = poPanBand->GetYSize();
    double dfMinX = 0.0;
    double dfMinY = 0.0;
    double dfMaxX = 0.0;
    double dfMaxY = 0.0;
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
        char* pszProj4 = nullptr;
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

        if( nInputSpectralBandsIn && pahInputSpectralBandsIn != nullptr )
        {
            if( iSpectralBand == nInputSpectralBandsIn )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "More SpectralBand elements than in source array");
                goto error;
            }
            poDataset = GDALRasterBand::FromHandle(
                pahInputSpectralBandsIn[iSpectralBand])->GetDataset();
            if( poDataset )
                osSourceFilename = poDataset->GetDescription();

            oMapNamesToDataset[CPLSPrintf("%p", poDataset)] = poDataset;
        }
        else
        {
            const char* pszSourceFilename = CPLGetXMLValue(psIter, "SourceFilename", nullptr);
            if( pszSourceFilename == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "SpectralBand.SourceFilename missing");
                goto error;
            }
            int bRelativeToVRT = atoi(CPLGetXMLValue( psIter, "SourceFilename.relativetoVRT", "0"));
            if( bRelativeToVRT )
            {
                const char* pszAbs = CPLProjectRelativeFilename( pszVRTPathIn, pszSourceFilename );
                m_oMapToRelativeFilenames[pszAbs] = pszSourceFilename;
                pszSourceFilename = pszAbs;
            }
            osSourceFilename = pszSourceFilename;
            poDataset = oMapNamesToDataset[osSourceFilename];
            if( poDataset == nullptr )
            {
                poDataset = GDALDataset::FromHandle(
                    GDALOpen( osSourceFilename, GA_ReadOnly ) );
                if( poDataset == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s not a valid dataset",
                             osSourceFilename.c_str());
                    goto error;
                }
                oMapNamesToDataset[osSourceFilename] = poDataset;
                m_apoDatasetsToClose.push_back(poDataset);
            }
        }

        if( poDataset != nullptr )
        {
            // Check that the spectral band has a georeferencing consistent
            // of the pan band. Allow an error of at most the size of one pixel
            // of the spectral band.
            if( bPanGeoTransformValid )
            {
                CPLString osProjection;
                if( poDataset->GetProjectionRef() )
                    osProjection = poDataset->GetProjectionRef();

                if( !osPanProjection.empty() )
                {
                    if( !osProjection.empty() )
                    {
                        if( osPanProjection != osProjection )
                        {
                            CPLString osProjectionProj4;
                            char* pszProj4 = nullptr;
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
                else if( !osProjection.empty() )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Pan dataset has no projection, whereas %s has one. Results might be incorrect",
                             osSourceFilename.c_str());
                }

                double adfSpectralGeoTransform[6];
                if( poDataset->GetGeoTransform(adfSpectralGeoTransform) == CE_None )
                {
                    int bIsThisOneNonMatching = FALSE;
                    double dfPixelSize
                        = std::max( adfSpectralGeoTransform[1],
                                    std::abs(adfSpectralGeoTransform[5] ) );
                    if( std::abs(adfPanGT[0] - adfSpectralGeoTransform[0]) > dfPixelSize ||
                        std::abs(adfPanGT[3] - adfSpectralGeoTransform[3]) > dfPixelSize )
                    {
                        bIsThisOneNonMatching = TRUE;
                        if( m_eGTAdjustment == GTAdjust_None )
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
                    if( std::abs(dfLRPanX - dfLRSpectralX) > dfPixelSize ||
                        std::abs(dfLRPanY - dfLRSpectralY) > dfPixelSize )
                    {
                        bIsThisOneNonMatching = TRUE;
                        if( m_eGTAdjustment == GTAdjust_None )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "Georeferencing of bottom-right corner of pan dataset and %s do not match",
                                    osSourceFilename.c_str());
                        }
                    }

                    if( bIsThisOneNonMatching && m_eGTAdjustment == GTAdjust_Union )
                    {
                        dfMinX = std::min(dfMinX, adfSpectralGeoTransform[0]);
                        dfMinY = std::min(dfMinY, dfLRSpectralY);
                        dfMaxX = std::max(dfMaxX, dfLRSpectralX);
                        dfMaxY = std::max(dfMaxY, adfSpectralGeoTransform[3]);
                    }
                    else if( bIsThisOneNonMatching && m_eGTAdjustment == GTAdjust_Intersection )
                    {
                        dfMinX = std::max(dfMinX, adfSpectralGeoTransform[0]);
                        dfMinY = std::max(dfMinY, dfLRSpectralY);
                        dfMaxX = std::min(dfMaxX, dfLRSpectralX);
                        dfMaxY = std::min(dfMaxY, adfSpectralGeoTransform[3]);
                    }

                    bFoundNonMatchingGT |= bIsThisOneNonMatching;
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

    if( bFoundNonMatchingGT &&
            (m_eGTAdjustment == GTAdjust_Union || m_eGTAdjustment == GTAdjust_Intersection) )
    {
        if( bFoundRotatingTerms )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "One of the panchromatic or spectral datasets has rotating "
                     "terms in their geotransform matrix. Adjustment not possible");
            goto error;
        }
        if( m_eGTAdjustment == GTAdjust_Intersection &&
            (dfMinX >= dfMaxX || dfMinY >= dfMaxY) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "One of the panchromatic or spectral datasets has rotating "
                     "terms in their geotransform matrix. Adjustment not possible");
            goto error;
        }
        if( m_eGTAdjustment == GTAdjust_Union )
            CPLDebug("VRT", "Do union of bounding box of panchromatic and spectral datasets");
        else
            CPLDebug("VRT", "Do intersection of bounding box of panchromatic and spectral datasets");

        // If the pandataset needs adjustments, make sure the coordinates of the
        // union/intersection properly align with the grid of the pandataset
        // to avoid annoying sub-pixel shifts on the panchro band.
        double dfPixelSize = std::max( adfPanGT[1], std::abs(adfPanGT[5]) );
        if( std::abs(adfPanGT[0] - dfMinX) > dfPixelSize ||
            std::abs(adfPanGT[3] - dfMaxY) > dfPixelSize ||
            std::abs(dfLRPanX - dfMaxX) > dfPixelSize ||
            std::abs(dfLRPanY - dfMinY) > dfPixelSize )
        {
            dfMinX = adfPanGT[0] + std::floor((dfMinX - adfPanGT[0]) / adfPanGT[1] + 0.5) * adfPanGT[1];
            dfMaxY = adfPanGT[3] + std::floor((dfMaxY - adfPanGT[3]) / adfPanGT[5] + 0.5) * adfPanGT[5];
            dfMaxX = dfLRPanX + std::floor((dfMaxX - dfLRPanX) / adfPanGT[1] + 0.5) * adfPanGT[1];
            dfMinY = dfLRPanY + std::floor((dfMinY - dfLRPanY) / adfPanGT[5] + 0.5) * adfPanGT[5];
        }

        std::map<CPLString, GDALDataset*>::iterator oIter = oMapNamesToDataset.begin();
        for(; oIter != oMapNamesToDataset.end(); ++oIter)
        {
            GDALDataset* poSrcDS = oIter->second;
            double adfGT[6];
            if( poSrcDS->GetGeoTransform(adfGT) != CE_None )
                continue;

            // Check if this dataset needs adjustments
            dfPixelSize = std::max( adfGT[1], std::abs(adfGT[5]) );
            dfPixelSize = std::max( adfPanGT[1], dfPixelSize );
            dfPixelSize = std::max( std::abs(adfPanGT[5]), dfPixelSize );
            if( std::abs(adfGT[0] - dfMinX) <= dfPixelSize &&
                std::abs(adfGT[3] - dfMaxY) <= dfPixelSize &&
                std::abs(adfGT[0] + poSrcDS->GetRasterXSize() * adfGT[1] - dfMaxX) <= dfPixelSize &&
                std::abs(adfGT[3] + poSrcDS->GetRasterYSize() * adfGT[5] - dfMinY) <= dfPixelSize )
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
            int nAdjustRasterXSize = static_cast<int>(0.5 + (dfMaxX - dfMinX) / adfAdjustedGT[1]);
            int nAdjustRasterYSize = static_cast<int>(0.5 + (dfMaxY - dfMinY) / (-adfAdjustedGT[5]));

            VRTDataset* poVDS = new VRTDataset(nAdjustRasterXSize, nAdjustRasterYSize);
            poVDS->SetWritable(FALSE);
            poVDS->SetDescription(poSrcDS->GetDescription());
            poVDS->SetGeoTransform(adfAdjustedGT);
            poVDS->SetProjection(poPanDataset->GetProjectionRef());

            for(int i=0;i<poSrcDS->GetRasterCount();i++)
            {
                GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand(i+1);
                poVDS->AddBand(poSrcBand->GetRasterDataType(), nullptr);
                VRTSourcedRasterBand* poVRTBand =
                    static_cast<VRTSourcedRasterBand *>(
                        poVDS->GetRasterBand(i+1) );

                const char* pszNBITS = poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
                if( pszNBITS )
                    poVRTBand->SetMetadataItem("NBITS", pszNBITS, "IMAGE_STRUCTURE");

                VRTSimpleSource* poSimpleSource = new VRTSimpleSource();
                poVRTBand->ConfigureSource( poSimpleSource,
                                            poSrcBand,
                                            FALSE,
                                            static_cast<int>( std::floor((dfMinX - adfGT[0]) / adfGT[1] + 0.001) ),
                                            static_cast<int>( std::floor((dfMaxY - adfGT[3]) / adfGT[5] + 0.001) ),
                                            static_cast<int>(0.5 + (dfMaxX - dfMinX) / adfGT[1]),
                                            static_cast<int>(0.5 + (dfMaxY - dfMinY) / (-adfGT[5])),
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

            m_apoDatasetsToClose.push_back(poVDS);
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
                 "Inconsistent declared VRT dimensions with panchro dataset");
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Initialize all the general VRT stuff.  This will even           */
/*      create the VRTPansharpenedRasterBands and initialize them.      */
/* -------------------------------------------------------------------- */
    eErr = VRTDataset::XMLInit( psTree, pszVRTPathIn );

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
                poPanDataset->GetProjectionRef() != nullptr &&
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
        if( psIter->eType != CXT_Element || !EQUAL(psIter->pszValue, "SpectralBand") )
            continue;

        GDALDataset* poDataset;
        GDALRasterBand* poBand;

        const char* pszDstBand = CPLGetXMLValue(psIter, "dstBand", nullptr);
        int nDstBand = -1;
        if( pszDstBand != nullptr )
        {
            nDstBand = atoi(pszDstBand);
            if( nDstBand <= 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "SpectralBand.dstBand = '%s' invalid",
                         pszDstBand);
                goto error;
            }
        }

        if( nInputSpectralBandsIn && pahInputSpectralBandsIn != nullptr )
        {
            poBand = GDALRasterBand::FromHandle(
                pahInputSpectralBandsIn[iSpectralBand] );
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
            const char* pszSourceFilename = CPLGetXMLValue(psIter, "SourceFilename", nullptr);
            CPLAssert(pszSourceFilename);
            const bool bRelativeToVRT = CPL_TO_BOOL(atoi(
                CPLGetXMLValue( psIter, "SourceFilename.relativetoVRT", "0")));
            if( bRelativeToVRT )
                pszSourceFilename = CPLProjectRelativeFilename( pszVRTPathIn, pszSourceFilename );
            osSourceFilename = pszSourceFilename;
            poDataset = oMapNamesToDataset[osSourceFilename];
            CPLAssert(poDataset);
            const char* pszSourceBand = CPLGetXMLValue(psIter,"SourceBand","1");
            const int nBand = atoi(pszSourceBand);
            poBand = poDataset->GetRasterBand(nBand);
            if( poBand == nullptr )
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
            aMapDstBandToSpectralBand[nDstBand-1]
                = static_cast<int>( ahSpectralBands.size() - 1 );
        }

        iSpectralBand ++;
    }

    if( ahSpectralBands.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No spectral band defined");
        goto error;
    }

    {
        const char* pszNoData = CPLGetXMLValue(psOptions, "NoData", nullptr);
        if( pszNoData )
        {
            if( EQUAL(pszNoData, "NONE") )
            {
                m_bNoDataDisabled = TRUE;
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
        for( int i = 0;
             i < static_cast<int>( aMapDstBandToSpectralBand.size()) ;
             i++ )
        {
            if( aMapDstBandToSpectralBand.find(i) == aMapDstBandToSpectralBand.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Hole in SpectralBand.dstBand numbering");
                goto error;
            }
            GDALRasterBand* poInputBand = GDALRasterBand::FromHandle(
                ahSpectralBands[aMapDstBandToSpectralBand[i]] );
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
            if( static_cast<VRTRasterBand *>( GetRasterBand(i+1) )->IsPansharpenRasterBand() )
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
                    static_cast<VRTPansharpenedRasterBand *>( GetRasterBand(i+1) )->
                        SetIndexAsPansharpenedBand(nIdxAsPansharpenedBand);
                    nIdxAsPansharpenedBand ++;
                }
            }
        }
    }

    // Figure out bit depth
    {
        const char* pszBitDepth = CPLGetXMLValue(psOptions, "BitDepth", nullptr);
        if( pszBitDepth == nullptr )
            pszBitDepth = GDALRasterBand::FromHandle(
                ahSpectralBands[0] )->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
        if( pszBitDepth )
            nBitDepth = atoi(pszBitDepth);
        if( nBitDepth )
        {
            for(int i=0;i<nBands;i++)
            {
                if( !static_cast<VRTRasterBand *>(
                       GetRasterBand(i+1) )->IsPansharpenRasterBand() )
                    continue;
                if( GetRasterBand(i+1)->GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == nullptr )
                {
                    if( nBitDepth != 8 && nBitDepth != 16 && nBitDepth != 32 )
                    {
                        GetRasterBand(i+1)->SetMetadataItem("NBITS",
                                        CPLSPrintf("%d", nBitDepth), "IMAGE_STRUCTURE");
                    }
                }
                else if( nBitDepth == 8 || nBitDepth == 16 || nBitDepth == 32 )
                {
                    GetRasterBand(i+1)->SetMetadataItem("NBITS", nullptr, "IMAGE_STRUCTURE");
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
        const int nDstBand = 1 + aMapDstBandToSpectralBandIter->first;
        if( nDstBand > nBands || !static_cast<VRTRasterBand*>(
               GetRasterBand(nDstBand) )->IsPansharpenRasterBand() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SpectralBand.dstBand = '%d' invalid",
                        nDstBand);
            goto error;
        }
    }

    if( adfWeights.empty() )
    {
        for( int i = 0; i < static_cast<int>( ahSpectralBands.size() ); i++ )
        {
            adfWeights.push_back(1.0 / ahSpectralBands.size());
        }
    }
    else if( adfWeights.size() != ahSpectralBands.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%d weights defined, but %d input spectral bands",
                 static_cast<int>( adfWeights.size() ),
                 static_cast<int>( ahSpectralBands.size() ) );
        goto error;
    }

    if( aMapDstBandToSpectralBand.empty() )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No spectral band is mapped to an output band");
    }

/* -------------------------------------------------------------------- */
/*      Instantiate poPansharpener                                      */
/* -------------------------------------------------------------------- */
    psPanOptions = GDALCreatePansharpenOptions();
    psPanOptions->ePansharpenAlg = GDAL_PSH_WEIGHTED_BROVEY;
    psPanOptions->eResampleAlg = eResampleAlg;
    psPanOptions->nBitDepth = nBitDepth;
    psPanOptions->nWeightCount = static_cast<int>( adfWeights.size() );
    psPanOptions->padfWeights = static_cast<double *>(
        CPLMalloc( sizeof(double) * adfWeights.size() ) );
    memcpy(psPanOptions->padfWeights, &adfWeights[0],
           sizeof(double)*adfWeights.size());
    psPanOptions->hPanchroBand = poPanBand;
    psPanOptions->nInputSpectralBands
        = static_cast<int>( ahSpectralBands.size() );
    psPanOptions->pahInputSpectralBands = static_cast<GDALRasterBandH *>(
        CPLMalloc( sizeof(GDALRasterBandH) * ahSpectralBands.size() ) );
    memcpy(psPanOptions->pahInputSpectralBands, &ahSpectralBands[0],
           sizeof(GDALRasterBandH)*ahSpectralBands.size());
    psPanOptions->nOutPansharpenedBands
        = static_cast<int>( aMapDstBandToSpectralBand.size() );
    psPanOptions->panOutPansharpenedBands = static_cast<int *>(
        CPLMalloc( sizeof(int) * aMapDstBandToSpectralBand.size() ) );
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

    m_poPansharpener = new GDALPansharpenOperation();
    eErr = m_poPansharpener->Initialize(psPanOptions);
    if( eErr != CE_None )
    {
        // Delete the pansharper object before closing the dataset
        // because it may have warped the bands into an intermediate VRT
        delete m_poPansharpener;
        m_poPansharpener = nullptr;

        // Close in reverse order (VRT firsts and real datasets after)
        for( int i = static_cast<int>( m_apoDatasetsToClose.size() ) - 1;
             i >= 0;
             i-- )
        {
            GDALClose(m_apoDatasetsToClose[i]);
        }
        m_apoDatasetsToClose.resize(0);
    }
    GDALDestroyPansharpenOptions(psPanOptions);

    return eErr;

error:
    // Close in reverse order (VRT firsts and real datasets after)
    for( int i = static_cast<int>( m_apoDatasetsToClose.size() ) - 1;
         i >= 0;
         i-- )
    {
        GDALClose(m_apoDatasetsToClose[i]);
    }
    m_apoDatasetsToClose.resize(0);
    return CE_Failure;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTPansharpenedDataset::SerializeToXML( const char *pszVRTPathIn )

{
    CPLXMLNode *psTree = VRTDataset::SerializeToXML( pszVRTPathIn );

    if( psTree == nullptr )
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
                                 CPLSPrintf( "%d", m_nBlockXSize ) );
    CPLCreateXMLElementAndValue( psTree, "BlockYSize",
                                 CPLSPrintf( "%d", m_nBlockYSize ) );

/* -------------------------------------------------------------------- */
/*      Serialize the options.                                          */
/* -------------------------------------------------------------------- */
    if( m_poPansharpener == nullptr )
        return psTree;
    GDALPansharpenOptions* psOptions = m_poPansharpener->GetOptions();
    if( psOptions == nullptr )
        return psTree;

    CPLXMLNode* psOptionsNode = CPLCreateXMLNode(psTree, CXT_Element, "PansharpeningOptions");

    if( psOptions->ePansharpenAlg == GDAL_PSH_WEIGHTED_BROVEY )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "Algorithm", "WeightedBrovey" );
    }
    else
    {
        CPLAssert(false);
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

    const char* pszAdjust = nullptr;
    switch( m_eGTAdjustment )
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
    else if( m_bNoDataDisabled )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "NoData", "None" );
    }

    if( psOptions->dfMSShiftX != 0.0 )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "MSShiftX",
                                     CPLSPrintf("%.16g", psOptions->dfMSShiftX) );
    }
    if( psOptions->dfMSShiftY != 0.0 )
    {
        CPLCreateXMLElementAndValue( psOptionsNode, "MSShiftY",
                                     CPLSPrintf("%.16g", psOptions->dfMSShiftY) );
    }

    if( pszAdjust )
        CPLCreateXMLElementAndValue( psOptionsNode, "SpatialExtentAdjustment", pszAdjust);

    if( psOptions->hPanchroBand )
    {
         CPLXMLNode* psBand = CPLCreateXMLNode(psOptionsNode, CXT_Element, "PanchroBand");
         GDALRasterBand* poBand = GDALRasterBand::FromHandle(
             psOptions->hPanchroBand );
         if( poBand->GetDataset() )
         {
             std::map<CPLString,CPLString>::iterator oIter =
                m_oMapToRelativeFilenames.find(poBand->GetDataset()->GetDescription());
             if( oIter == m_oMapToRelativeFilenames.end() )
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
                    if( static_cast<VRTRasterBand *>(
                           GetRasterBand(k+1) )->IsPansharpenRasterBand() )
                    {
                        if( static_cast<VRTPansharpenedRasterBand *>(
                               GetRasterBand(k+1) )->GetIndexAsPansharpenedBand() == j )
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

        GDALRasterBand* poBand = GDALRasterBand::FromHandle(
            psOptions->pahInputSpectralBands[i] );
        if( poBand->GetDataset() )
        {
            std::map<CPLString,CPLString>::iterator oIter =
                m_oMapToRelativeFilenames.find(poBand->GetDataset()->GetDescription());
            if( oIter == m_oMapToRelativeFilenames.end() )
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

void VRTPansharpenedDataset::GetBlockSize( int *pnBlockXSize, int *pnBlockYSize ) const

{
    assert( nullptr != pnBlockXSize );
    assert( nullptr != pnBlockYSize );

    *pnBlockXSize = m_nBlockXSize;
    *pnBlockYSize = m_nBlockYSize;
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

    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eBufType);
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
                !static_cast<VRTRasterBand *>(
                    GetRasterBand(i+1) )->IsPansharpenRasterBand() )
            {
                goto default_path;
            }
        }

        //{static int bDone = 0; if (!bDone) printf("(2)\n"); bDone = 1; }
        return m_poPansharpener->ProcessRegion(
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

VRTPansharpenedRasterBand::VRTPansharpenedRasterBand( GDALDataset *poDSIn, int nBandIn,
                                                      GDALDataType eDataTypeIn ) :
    m_nIndexAsPansharpenedBand(nBandIn - 1)
{
    Initialize( poDSIn->GetRasterXSize(), poDSIn->GetRasterYSize() );

    poDS = poDSIn;
    nBand = nBandIn;
    eAccess = GA_Update;
    eDataType = eDataTypeIn;

    static_cast<VRTPansharpenedDataset *>(
        poDS )->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                        ~VRTPansharpenedRasterBand()                  */
/************************************************************************/

VRTPansharpenedRasterBand::~VRTPansharpenedRasterBand()

{
    FlushCache(true);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTPansharpenedRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                              void * pImage )

{
    const int nReqXOff = nBlockXOff * nBlockXSize;
    const int nReqYOff = nBlockYOff * nBlockYSize;
    int nReqXSize = nBlockXSize;
    int nReqYSize = nBlockYSize;
    if( nReqXOff + nReqXSize > nRasterXSize )
        nReqXSize = nRasterXSize - nReqXOff;
    if( nReqYOff + nReqYSize > nRasterYSize )
        nReqYSize = nRasterYSize - nReqYOff;

    //{static int bDone = 0; if (!bDone) printf("(4)\n"); bDone = 1; }
    const int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;
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
            memmove( static_cast<GByte *>( pImage ) + j * nDataTypeSize * nBlockXSize,
                     static_cast<GByte *>( pImage ) + j * nDataTypeSize * nReqXSize,
                     nReqXSize * nDataTypeSize );
            memset( static_cast<GByte *>( pImage ) + (j * nBlockXSize + nReqXSize) * nDataTypeSize,
                    0,
                    (nBlockXSize - nReqXSize) * nDataTypeSize );
        }
    }
    if( nReqYSize < nBlockYSize )
    {
        memset( static_cast<GByte *>( pImage ) + nReqYSize * nBlockXSize * nDataTypeSize,
                0,
                (nBlockYSize - nReqYSize ) * nBlockXSize * nDataTypeSize );
    }

    // Cache other bands
    CPLErr eErr = CE_None;
    VRTPansharpenedDataset* poGDS
        = static_cast<VRTPansharpenedDataset *>( poDS );
    if( poGDS->nBands != 1 && !poGDS->m_bLoadingOtherBands )
    {
        poGDS->m_bLoadingOtherBands = TRUE;

        for( int iOtherBand = 1; iOtherBand <= poGDS->nBands; iOtherBand++ )
        {
            if( iOtherBand == nBand )
                continue;

            GDALRasterBlock *poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff);
            if (poBlock == nullptr)
            {
                eErr = CE_Failure;
                break;
            }
            poBlock->DropLock();
        }

        poGDS->m_bLoadingOtherBands = FALSE;
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
    if( eRWFlag == GF_Write )
        return CE_Failure;

    VRTPansharpenedDataset* poGDS
        = static_cast<VRTPansharpenedDataset *>( poDS );

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

    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eBufType);
    if( nDataTypeSize > 0 &&
        nXSize == nBufXSize &&
        nYSize == nBufYSize &&
        nDataTypeSize == nPixelSpace &&
        nLineSpace == nPixelSpace * nBufXSize )
    {
        GDALPansharpenOptions* psOptions = poGDS->m_poPansharpener->GetOptions();

        // Have we already done this request for another band ?
        // If so use the cached result
        const size_t nBufferSizePerBand
            = static_cast<size_t>(nXSize) * nYSize * nDataTypeSize;
        if( nXOff == poGDS->m_nLastBandRasterIOXOff &&
            nYOff >= poGDS->m_nLastBandRasterIOYOff &&
            nXSize == poGDS->m_nLastBandRasterIOXSize &&
            nYOff + nYSize <= poGDS->m_nLastBandRasterIOYOff + poGDS->m_nLastBandRasterIOYSize &&
            eBufType == poGDS->m_eLastBandRasterIODataType )
        {
            //{static int bDone = 0; if (!bDone) printf("(6)\n"); bDone = 1; }
            if( poGDS->m_pabyLastBufferBandRasterIO == nullptr )
                return CE_Failure;
            const size_t nBufferSizePerBandCached
                = static_cast<size_t>( nXSize ) * poGDS->m_nLastBandRasterIOYSize
                * nDataTypeSize;
            memcpy(pData,
                   poGDS->m_pabyLastBufferBandRasterIO +
                        nBufferSizePerBandCached * m_nIndexAsPansharpenedBand +
                        static_cast<size_t>(nYOff - poGDS->m_nLastBandRasterIOYOff) * nXSize * nDataTypeSize,
                   nBufferSizePerBand);
            return CE_None;
        }

        int nYSizeToCache = nYSize;
        if( nYSize == 1 && nXSize == nRasterXSize )
        {
            //{static int bDone = 0; if (!bDone) printf("(7)\n"); bDone = 1; }
            // For efficiency, try to cache at leak 256 K
            nYSizeToCache = (256 * 1024) / nXSize / nDataTypeSize;
            if( nYSizeToCache == 0 )
                nYSizeToCache = 1;
            else if( nYOff + nYSizeToCache > nRasterYSize )
                nYSizeToCache = nRasterYSize - nYOff;
        }
        const GUIntBig nBufferSize
            = static_cast<GUIntBig>( nXSize ) * nYSizeToCache * nDataTypeSize
            * psOptions->nOutPansharpenedBands;
        // Check the we don't overflow (for 32 bit platforms)
        if( static_cast<GUIntBig>( static_cast<size_t>( nBufferSize ) )
            != nBufferSize )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory error while allocating working buffers");
            return CE_Failure;
        }
        GByte* pabyTemp = static_cast<GByte *>(
            VSI_REALLOC_VERBOSE( poGDS->m_pabyLastBufferBandRasterIO,
                        static_cast<size_t>( nBufferSize) ) );
        if( pabyTemp == nullptr )
        {
            return CE_Failure;
        }
        poGDS->m_nLastBandRasterIOXOff = nXOff;
        poGDS->m_nLastBandRasterIOYOff = nYOff;
        poGDS->m_nLastBandRasterIOXSize = nXSize;
        poGDS->m_nLastBandRasterIOYSize = nYSizeToCache;
        poGDS->m_eLastBandRasterIODataType = eBufType;
        poGDS->m_pabyLastBufferBandRasterIO = pabyTemp;

        CPLErr eErr = poGDS->m_poPansharpener->ProcessRegion(
                            nXOff, nYOff, nXSize, nYSizeToCache,
                            poGDS->m_pabyLastBufferBandRasterIO, eBufType);
        if( eErr == CE_None )
        {
            //{static int bDone = 0; if (!bDone) printf("(8)\n"); bDone = 1; }
            size_t nBufferSizePerBandCached
                = static_cast<size_t>( nXSize ) * poGDS->m_nLastBandRasterIOYSize
                * nDataTypeSize;
            memcpy(pData,
                   poGDS->m_pabyLastBufferBandRasterIO +
                        nBufferSizePerBandCached * m_nIndexAsPansharpenedBand,
                   nBufferSizePerBand);
        }
        else
        {
            VSIFree(poGDS->m_pabyLastBufferBandRasterIO);
            poGDS->m_pabyLastBufferBandRasterIO = nullptr;
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
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTPansharpenedRasterBand::SerializeToXML( const char *pszVRTPathIn )

{
    CPLXMLNode *psTree = VRTRasterBand::SerializeToXML( pszVRTPathIn );

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
    VRTPansharpenedDataset* poGDS
        = static_cast<VRTPansharpenedDataset *>( poDS );

    // Build on-the-fly overviews from overviews of pan and spectral bands
    if( poGDS->m_poPansharpener != nullptr &&
        poGDS->m_apoOverviewDatasets.empty() &&
        poGDS->m_poMainDataset == poGDS )
    {
        GDALPansharpenOptions* psOptions = poGDS->m_poPansharpener->GetOptions();

        GDALRasterBand* poPanBand
            = GDALRasterBand::FromHandle( psOptions->hPanchroBand );
        const int nPanOvrCount = poPanBand->GetOverviewCount();
        if( nPanOvrCount > 0 )
        {
            for(int i=0;i<poGDS->GetRasterCount();i++)
            {
                if( !static_cast<VRTRasterBand *>(
                       poGDS->GetRasterBand(i+1) )->IsPansharpenRasterBand() )
                {
                    return 0;
                }
            }

            int nSpectralOvrCount = GDALRasterBand::FromHandle(
                psOptions->pahInputSpectralBands[0] )->GetOverviewCount();
            // JP2KAK overviews are not bound to a dataset, so let the full resolution bands
            // and rely on JP2KAK IRasterIO() to select the appropriate resolution
            if( nSpectralOvrCount && GDALRasterBand::FromHandle( psOptions->pahInputSpectralBands[0] )->GetOverview(0)->GetDataset() == nullptr )
                nSpectralOvrCount = 0;
            for(int i=1;i<psOptions->nInputSpectralBands;i++)
            {
                if( GDALRasterBand::FromHandle(
                       psOptions->pahInputSpectralBands[i])->GetOverviewCount()
                    != nSpectralOvrCount )
                {
                    nSpectralOvrCount = 0;
                    break;
                }
            }
            for(int j=0;j<nPanOvrCount;j++)
            {
                GDALRasterBand* poPanOvrBand = poPanBand->GetOverview(j);
                VRTPansharpenedDataset* poOvrDS
                    = new VRTPansharpenedDataset( poPanOvrBand->GetXSize(),
                                                  poPanOvrBand->GetYSize() );
                poOvrDS->m_poMainDataset = poGDS;
                for(int i=0;i<poGDS->GetRasterCount();i++)
                {
                    GDALRasterBand* poSrcBand = poGDS->GetRasterBand(i+1);
                    GDALRasterBand* poBand
                        = new VRTPansharpenedRasterBand(
                            poOvrDS, i+1, poSrcBand->GetRasterDataType());
                    const char* pszNBITS
                        = poSrcBand->GetMetadataItem("NBITS",
                                                     "IMAGE_STRUCTURE");
                    if( pszNBITS )
                        poBand->SetMetadataItem("NBITS", pszNBITS,
                                                "IMAGE_STRUCTURE");
                    poOvrDS->SetBand(i+1, poBand);
                }

                GDALPansharpenOptions* psPanOvrOptions
                    = GDALClonePansharpenOptions(psOptions);
                psPanOvrOptions->hPanchroBand = poPanOvrBand;
                if( nSpectralOvrCount > 0 )
                {
                    for(int i=0;i<psOptions->nInputSpectralBands;i++)
                    {
                        psPanOvrOptions->pahInputSpectralBands[i] =
                            GDALRasterBand::FromHandle(
                                psOptions->pahInputSpectralBands[i] )->GetOverview(
                                    (j < nSpectralOvrCount) ? j : nSpectralOvrCount - 1 );
                    }
                }
                poOvrDS->m_poPansharpener = new GDALPansharpenOperation();
                if (poOvrDS->m_poPansharpener->Initialize(psPanOvrOptions) != CE_None)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Unable to initialize pansharpener." );
                }
                GDALDestroyPansharpenOptions(psPanOvrOptions);

                poOvrDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

                poGDS->m_apoOverviewDatasets.push_back(poOvrDS);
            }
        }
    }
    return static_cast<int>( poGDS->m_apoOverviewDatasets.size() );
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand* VRTPansharpenedRasterBand::GetOverview(int iOvr)
{
    if( iOvr < 0 || iOvr >= GetOverviewCount() )
        return nullptr;

    VRTPansharpenedDataset* poGDS
        = static_cast<VRTPansharpenedDataset *>( poDS );

    return poGDS->m_apoOverviewDatasets[iOvr]->GetRasterBand(nBand);
}

/*! @endcond */
