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
#include "gdalpansharpen.h"
#include <map>
#include <set>
#include <assert.h>

CPL_CVSID("$Id$");

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
}

/************************************************************************/
/*                    ~VRTPansharpenedDataset()                         */
/************************************************************************/

VRTPansharpenedDataset::~VRTPansharpenedDataset()

{
    CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTPansharpenedDataset::CloseDependentDatasets()
{
    if( poMainDataset == NULL )
        return FALSE;
    FlushCache();

    int bIsMainDataset = (poMainDataset == this );
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
        GDALPansharpenOptions* psOptions = poPansharpener->GetOptions();
        if( psOptions != NULL && bIsMainDataset )
        {
            std::map<CPLString, GDALDatasetH> oMapNamesToDataset;
            GDALDatasetH hDS;
            if( psOptions->hPanchroBand != NULL &&
                (hDS = GDALGetBandDataset(psOptions->hPanchroBand)) != NULL )
            {
                oMapNamesToDataset[GDALGetDescription(hDS)] = hDS;
            }
            for(int i=0;i<psOptions->nInputSpectralBands;i++)
            {
                if( psOptions->pahInputSpectralBands[i] != NULL &&
                    (hDS = GDALGetBandDataset(psOptions->pahInputSpectralBands[i])) != NULL )
                {
                    oMapNamesToDataset[GDALGetDescription(hDS)] = hDS;
                }
            }
            std::map<CPLString, GDALDatasetH>::iterator oIter = oMapNamesToDataset.begin();
            for(; oIter != oMapNamesToDataset.end(); ++oIter )
            {
                bHasDroppedRef = TRUE;
                GDALClose(oIter->second);
            }
        }
        delete poPansharpener;
        poPansharpener = NULL;
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
        pszSourceFilename = CPLProjectRelativeFilename( pszVRTPath, pszSourceFilename );
    CPLString osSourceFilename(pszSourceFilename);
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(osSourceFilename, GA_ReadOnly);
    GDALDataset* poPanDataset = poDataset;
    if( poDataset == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s not a valid dataset",
                 osSourceFilename.c_str());
        return CE_Failure;
    }

    if( nRasterXSize == 0 && nRasterYSize == 0 ) 
    {
        nRasterXSize = poDataset->GetRasterXSize();
        nRasterYSize = poDataset->GetRasterYSize();
    }
    else if( nRasterXSize != poDataset->GetRasterXSize() ||
             nRasterYSize != poDataset->GetRasterYSize() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistant declared VRT dimensions with panchro dataset");
        GDALClose(poDataset);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Initialize all the general VRT stuff.  This will even           */
/*      create the VRTPansharpenedRasterBands and initialize them.      */
/* -------------------------------------------------------------------- */
    eErr = VRTDataset::XMLInit( psTree, pszVRTPath );

    if( eErr != CE_None )
    {
        GDALClose(poDataset);
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Inherit georeferencing info from panchro band if not defined    */
/*      in VRT.                                                         */
/* -------------------------------------------------------------------- */

    double adfPanGeoTransform[6];
    int bPanGeoTransformValid = ( GetGeoTransform(adfPanGeoTransform) == CE_None );
    if( !bPanGeoTransformValid &&
        GetGCPCount() == 0 &&
        GetProjectionRef()[0] == '\0' )
    {
        if( poDataset->GetGeoTransform(adfPanGeoTransform) == CE_None )
        {
            bPanGeoTransformValid = TRUE;
            SetGeoTransform(adfPanGeoTransform);
        }
        if( poDataset->GetProjectionRef() != NULL &&
            poDataset->GetProjectionRef()[0] != '\0' )
        {
            SetProjection(poDataset->GetProjectionRef());
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse rest of PansharpeningOptions                              */
/* -------------------------------------------------------------------- */

    const char* pszAlgorithm = CPLGetXMLValue(psOptions, 
                                              "Algorithm", "WeightedBrovey");
    if( !EQUAL(pszAlgorithm, "WeightedBrovey") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Algorithm %s unsupported", pszAlgorithm);
        GDALClose(poDataset);
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
    
    std::map<CPLString, GDALDataset*> oMapNamesToDataset;
    oMapNamesToDataset[osSourceFilename] = poDataset;
    const char* pszSourceBand = CPLGetXMLValue(psPanchroBand,"SourceBand","1");
    int nBand = atoi(pszSourceBand);
    GDALRasterBand* poPanBand = poDataset->GetRasterBand(nBand);
    if( poPanBand == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s invalid band of %s",
                 pszSourceBand, osSourceFilename.c_str());
        GDALClose(poDataset);
        return CE_Failure;
    }

    std::vector<GDALRasterBand*> ahSpectralBands;
    std::map<int, int> aMapDstBandToSpectralBand;
    std::map<int,int>::iterator aMapDstBandToSpectralBandIter;
    for(CPLXMLNode* psIter = psOptions->psChild; psIter; psIter = psIter->psNext )
    {
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
        pszSourceFilename = CPLGetXMLValue(psIter, "SourceFilename", NULL);
        if( pszSourceFilename == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SpectralBand.SourceFilename missing");
            goto error;
        }
        bRelativeToVRT = atoi(CPLGetXMLValue( psIter, "SourceFilename.relativetoVRT", "0"));
        if( bRelativeToVRT )
            pszSourceFilename = CPLProjectRelativeFilename( pszVRTPath, pszSourceFilename );
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

            // Check that the spectral band has a georeferencing consistant
            // of the pan band. Allow an error of at most the size of one pixel
            // of the spectral band.
            if( bPanGeoTransformValid )
            {
                double adfSpectralGeoTransform[6];
                if( poDataset->GetGeoTransform(adfSpectralGeoTransform) == CE_None )
                {
                    double dfPixelSize = MAX(adfSpectralGeoTransform[1], fabs(adfSpectralGeoTransform[5]));
                    if( fabs(adfPanGeoTransform[0] - adfSpectralGeoTransform[0]) > dfPixelSize ||
                        fabs(adfPanGeoTransform[3] - adfSpectralGeoTransform[3]) > dfPixelSize )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Georeferencing of top-left corner of pan dataset and %s do not match",
                                 osSourceFilename.c_str());
                    }
                    double dfLRPanX = adfPanGeoTransform[0] +
                                      poPanDataset->GetRasterXSize() * adfPanGeoTransform[1] +
                                      poPanDataset->GetRasterYSize() * adfPanGeoTransform[2];
                    double dfLRPanY = adfPanGeoTransform[3] +
                                      poPanDataset->GetRasterXSize() * adfPanGeoTransform[4] +
                                      poPanDataset->GetRasterYSize() * adfPanGeoTransform[5];
                    double dfLRSpectralX = adfSpectralGeoTransform[0] +
                                      poDataset->GetRasterXSize() * adfSpectralGeoTransform[1] +
                                      poDataset->GetRasterYSize() * adfSpectralGeoTransform[2];
                    double dfLRSpectralY = adfSpectralGeoTransform[3] +
                                      poDataset->GetRasterXSize() * adfSpectralGeoTransform[4] +
                                      poDataset->GetRasterYSize() * adfSpectralGeoTransform[5];
                    if( fabs(dfLRPanX - dfLRSpectralX) > dfPixelSize ||
                        fabs(dfLRPanY - dfLRSpectralY) > dfPixelSize )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Georeferencing of bottom-right corner of pan dataset and %s do not match",
                                 osSourceFilename.c_str());
                    }
                }
            }

            oMapNamesToDataset[osSourceFilename] = poDataset;
        }
        pszSourceBand = CPLGetXMLValue(psIter,"SourceBand","1");
        int nBand = atoi(pszSourceBand);
        GDALRasterBand* poBand = poDataset->GetRasterBand(nBand);
        if( poBand == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s invalid band of %s",
                    pszSourceBand, osSourceFilename.c_str());
            goto error;
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
    }

    if( ahSpectralBands.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No spectral band defined");
        goto error;
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
            int bHasNoData = FALSE;
            double dfNoData = poInputBand->GetNoDataValue(&bHasNoData);
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

    poPansharpener = new GDALPansharpenOperation();
    eErr = poPansharpener->Initialize(psPanOptions);
    if( eErr != CE_None )
    {
        std::map<CPLString, GDALDataset*>::iterator oIter = oMapNamesToDataset.begin();
        for(; oIter != oMapNamesToDataset.end(); ++oIter )
        {
            GDALClose(oIter->second);
        }
        delete poPansharpener;
        poPansharpener = NULL;
    }
    GDALDestroyPansharpenOptions(psPanOptions);

    return eErr;
    
error:
    std::map<CPLString, GDALDataset*>::iterator oIter = oMapNamesToDataset.begin();
    for(; oIter != oMapNamesToDataset.end(); ++oIter )
    {
        GDALClose(oIter->second);
    }
    return CE_Failure;
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
    GDALPansharpenOperation* poPansharpener = ((VRTPansharpenedDataset *) poDS)->GetPansharpener();
    GDALPansharpenOptions* psOptions = poPansharpener->GetOptions();
    int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;
    GByte* pabyTemp = (GByte*)VSIMalloc3(nReqXSize, nReqYSize,
        psOptions->nOutPansharpenedBands * nDataTypeSize);
    CPLErr eErr = poPansharpener->ProcessRegion(
        nReqXOff, nReqYOff, nReqXSize, nReqYSize, pabyTemp, eDataType);
    for(int j=0;j<nReqYSize;j++)
        memcpy( (GByte*)pImage + j * nBlockXSize * nDataTypeSize,
                pabyTemp + (nIndexAsPansharpenedBand * nReqYSize + j) * nReqXSize * nDataTypeSize,
                nReqXSize * nDataTypeSize);
    CPLFree(pabyTemp);
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
                    GDALRasterBand* poBand = new VRTPansharpenedRasterBand(poOvrDS, i+1,
                                                poGDS->GetRasterBand(i+1)->GetRasterDataType());
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
