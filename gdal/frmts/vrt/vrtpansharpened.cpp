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
    nBlockXSize = MIN(nXSize, 256);
    nBlockYSize = MIN(nYSize, 256);
    eAccess = GA_Update;
    poPansharpener = NULL;
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
    FlushCache();

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
        if( psOptions != NULL )
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
    nBlockXSize = atoi(CPLGetXMLValue(psTree,"BlockXSize","256"));
    nBlockYSize = atoi(CPLGetXMLValue(psTree,"BlockYSize","256"));

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

    double adfGeoTransform[6];
    if( GetGeoTransform(adfGeoTransform) != CE_None &&
        GetGCPCount() == 0 &&
        GetProjectionRef()[0] == '\0' )
    {
        if( poDataset->GetGeoTransform(adfGeoTransform) == CE_None )
        {
            SetGeoTransform(adfGeoTransform);
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
        memcpy( (GByte*)pImage + j * nBlockXSize ,
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
