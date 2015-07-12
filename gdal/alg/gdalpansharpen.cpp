/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Pansharpening module
 * Purpose:  Implementation of pansharpening.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2015, Airbus DS Geo SA (weighted Brovey algorithm)
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

#include "gdalpansharpen.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "gdal_priv_templates.hpp"
#include "../frmts/vrt/vrtdataset.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                     GDALCreatePansharpenOptions()                    */
/************************************************************************/

/** Create pansharpening options.
 *
 * @return a newly allocated pansharpening option structure that must be freed
 * with GDALDestroyPansharpenOptions().
 *
 * @since GDAL 2.1
 */

GDALPansharpenOptions * GDALCreatePansharpenOptions()
{
    GDALPansharpenOptions* psOptions =
            (GDALPansharpenOptions*)CPLCalloc(1, sizeof(GDALPansharpenOptions));
    psOptions->ePansharpenAlg = GDAL_PSH_WEIGHTED_BROVEY;
    psOptions->eResampleAlg = GRIORA_Cubic;
    return psOptions;
}

/************************************************************************/
/*                     GDALDestroyPansharpenOptions()                   */
/************************************************************************/

/** Destroy pansharpening options.
 *
 * @param psOptions a pansharpening option structure allocated with
 * GDALCreatePansharpenOptions()
 *
 * @since GDAL 2.1
 */

void GDALDestroyPansharpenOptions( GDALPansharpenOptions* psOptions )
{
    if( psOptions == NULL )
        return;
    CPLFree(psOptions->padfWeights);
    CPLFree(psOptions->pahInputSpectralBands);
    CPLFree(psOptions->panOutPansharpenedBands);
    CPLFree(psOptions);
}

/************************************************************************/
/*                      GDALClonePansharpenOptions()                    */
/************************************************************************/

/** Clone pansharpening options.
 *
 * @param psOptions a pansharpening option structure allocated with
 * GDALCreatePansharpenOptions()
 * @return a newly allocated pansharpening option structure that must be freed
 * with GDALDestroyPansharpenOptions().
 *
 * @since GDAL 2.1
 */

GDALPansharpenOptions* GDALClonePansharpenOptions(
                                        const GDALPansharpenOptions* psOptions)
{
    GDALPansharpenOptions* psNewOptions = GDALCreatePansharpenOptions();
    psNewOptions->ePansharpenAlg = psOptions->ePansharpenAlg;
    psNewOptions->eResampleAlg = psOptions->eResampleAlg;
    psNewOptions->nBitDepth = psOptions->nBitDepth;
    psNewOptions->nWeightCount = psOptions->nWeightCount;
    if( psOptions->padfWeights )
    {
        psNewOptions->padfWeights =
            (double*)CPLMalloc(sizeof(double) * psOptions->nWeightCount);
        memcpy(psNewOptions->padfWeights,
               psOptions->padfWeights,
               sizeof(double) * psOptions->nWeightCount);
    }
    psNewOptions->hPanchroBand = psOptions->hPanchroBand;
    psNewOptions->nInputSpectralBands = psOptions->nInputSpectralBands;
    if( psOptions->pahInputSpectralBands )
    {
        psNewOptions->pahInputSpectralBands =
            (GDALRasterBandH*)CPLMalloc(sizeof(GDALRasterBandH) * psOptions->nInputSpectralBands);
        memcpy(psNewOptions->pahInputSpectralBands,
               psOptions->pahInputSpectralBands,
               sizeof(GDALRasterBandH) * psOptions->nInputSpectralBands);
    }
    psNewOptions->nOutPansharpenedBands = psOptions->nOutPansharpenedBands;
    if( psOptions->panOutPansharpenedBands )
    {
        psNewOptions->panOutPansharpenedBands =
            (int*)CPLMalloc(sizeof(int) * psOptions->nOutPansharpenedBands);
        memcpy(psNewOptions->panOutPansharpenedBands,
               psOptions->panOutPansharpenedBands,
               sizeof(int) * psOptions->nOutPansharpenedBands);
    }
    psNewOptions->bHasNoData = psOptions->bHasNoData;
    psNewOptions->dfNoData = psOptions->dfNoData;
    return psNewOptions;
}

/************************************************************************/
/*                        GDALPansharpenOperation()                     */
/************************************************************************/

/** Pansharpening operation constructor.
 *
 * The object is ready to be used after Initialize() has been called.
 */
GDALPansharpenOperation::GDALPansharpenOperation()
{
    psOptions = NULL;
    bWeightsWillNotOvershoot = TRUE;
}

/************************************************************************/
/*                       ~GDALPansharpenOperation()                     */
/************************************************************************/

/** Pansharpening operation destructor.
 */

GDALPansharpenOperation::~GDALPansharpenOperation()
{
    GDALDestroyPansharpenOptions(psOptions);
    for(size_t i=0;i<aVDS.size();i++)
        delete aVDS[i];
}

/************************************************************************/
/*                              Initialize()                            */
/************************************************************************/

/** Initialize the pansharpening operation.
 *
 * @param psOptionsIn pansharpening options. Must not be NULL.
 *
 * @return CE_None in case of success, CE_Failure in case of failure.
 */
CPLErr GDALPansharpenOperation::Initialize(const GDALPansharpenOptions* psOptionsIn)
{
    if( psOptionsIn->hPanchroBand == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hPanchroBand not set");
        return CE_Failure;
    }
    if( psOptionsIn->nInputSpectralBands <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No input spectral bands defined");
        return CE_Failure;
    }
    if( psOptionsIn->padfWeights == NULL ||
        psOptionsIn->nWeightCount != psOptionsIn->nInputSpectralBands )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No weights defined, or not the same number as input spectral bands");
        return CE_Failure;
    }
    GDALRasterBandH hRefBand = psOptionsIn->pahInputSpectralBands[0];
    int bSameDataset = psOptionsIn->nInputSpectralBands > 1;
    if( bSameDataset )
        anInputBands.push_back(GDALGetBandNumber(hRefBand));
    for(int i=1;i<psOptionsIn->nInputSpectralBands;i++)
    {
        GDALRasterBandH hBand = psOptionsIn->pahInputSpectralBands[i];
        if( GDALGetRasterBandXSize(hBand) != GDALGetRasterBandXSize(hRefBand) ||
            GDALGetRasterBandYSize(hBand) != GDALGetRasterBandYSize(hRefBand) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Dimensions of input spectral band %d different from first spectral band",
                     i);
            return CE_Failure;
        }
        if( bSameDataset )
        {
            if( GDALGetBandDataset(hBand) == NULL ||
                GDALGetBandDataset(hBand) != GDALGetBandDataset(hRefBand) )
            {
                anInputBands.resize(0);
                bSameDataset = FALSE;
            }
            else
            {
                anInputBands.push_back(GDALGetBandNumber(hBand));
            }
        }
    }
    if( psOptionsIn->nOutPansharpenedBands == 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "No output pansharpened band defined");
    }
    for(int i=0;i<psOptionsIn->nOutPansharpenedBands;i++)
    {
        if( psOptionsIn->panOutPansharpenedBands[i] < 0 ||
            psOptionsIn->panOutPansharpenedBands[i] >= psOptionsIn->nInputSpectralBands )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value panOutPansharpenedBands[%d] = %d",
                     psOptionsIn->panOutPansharpenedBands[i], i);
            return CE_Failure;
        }
    }

    GDALRasterBand* poPanchroBand = (GDALRasterBand*)psOptionsIn->hPanchroBand;
    GDALDataType eWorkDataType = poPanchroBand->GetRasterDataType();
    if( psOptionsIn->nBitDepth )
    {
        if( psOptionsIn->nBitDepth < 0 || psOptionsIn->nBitDepth > 31 ||
            (eWorkDataType == GDT_Byte && psOptionsIn->nBitDepth > 8) ||
            (eWorkDataType == GDT_UInt16 && psOptionsIn->nBitDepth > 16) ||
            (eWorkDataType == GDT_UInt32 && psOptionsIn->nBitDepth > 32) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value nBitDepth = %d for type %s",
                     psOptionsIn->nBitDepth, GDALGetDataTypeName(eWorkDataType));
            return CE_Failure;
        }
    }

    psOptions = GDALClonePansharpenOptions(psOptionsIn);
    if( psOptions->nBitDepth == GDALGetDataTypeSize(eWorkDataType) )
        psOptions->nBitDepth = 0;
    if( psOptions->nBitDepth &&
        !(eWorkDataType == GDT_Byte || eWorkDataType == GDT_UInt16 ||
          eWorkDataType == GDT_UInt32) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Ignoring nBitDepth = %d for type %s",
                 psOptions->nBitDepth, GDALGetDataTypeName(eWorkDataType));
        psOptions->nBitDepth = 0;
    }

    // If all the weights are in [0,x] and their sum is >= 1, no need
    // to try clamping
    bWeightsWillNotOvershoot = TRUE;
    double dfTotalWeights = 0.0;
    for(int i=0;i<psOptions->nInputSpectralBands; i++)
    {
        if( psOptions->padfWeights[i] < 0.0 )
            bWeightsWillNotOvershoot = FALSE;
        dfTotalWeights += psOptions->padfWeights[i];
    }
    if( dfTotalWeights < 1.0 )
        bWeightsWillNotOvershoot = FALSE;

    for(int i=0;i<psOptions->nInputSpectralBands; i++)
    {
        aMSBands.push_back((GDALRasterBand*)psOptions->pahInputSpectralBands[i]);
    }

    if( psOptions->bHasNoData )
    {
        int bNeedToWrapInVRT = FALSE;
        for(int i=0;i<psOptions->nInputSpectralBands; i++)
        {
            GDALRasterBand* poBand = (GDALRasterBand*)psOptions->pahInputSpectralBands[i];
            int bHasNoData;
            double dfNoData = poBand->GetNoDataValue(&bHasNoData);
            if( !bHasNoData || dfNoData != psOptions->dfNoData )
                bNeedToWrapInVRT = TRUE;
        }

        if( bNeedToWrapInVRT )
        {
            // Wrap spectral bands in a VRT if they don't have the nodata value
            VRTDataset* poVDS = NULL;
            for(int i=0;i<psOptions->nInputSpectralBands; i++)
            {
                GDALRasterBand* poSrcBand = aMSBands[i];
                if( anInputBands.size() == 0 || i == 0 )
                {
                    poVDS = new VRTDataset(poSrcBand->GetXSize(), poSrcBand->GetYSize());
                    aVDS.push_back(poVDS);
                }
                if( anInputBands.size() )
                    anInputBands[i] = i + 1;
                poVDS->AddBand(poSrcBand->GetRasterDataType(), NULL);
                VRTSourcedRasterBand* poVRTBand = (VRTSourcedRasterBand*) poVDS->GetRasterBand(i+1);
                aMSBands[i] = poVRTBand;
                poVRTBand->SetNoDataValue(psOptions->dfNoData);
                const char* pszNBITS = poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
                if( pszNBITS )
                    poVRTBand->SetMetadataItem("NBITS", pszNBITS, "IMAGE_STRUCTURE");

                VRTSimpleSource* poSimpleSource = new VRTSimpleSource();
                poVRTBand->ConfigureSource( poSimpleSource,
                                            poSrcBand,
                                            FALSE,
                                            0, 0,
                                            poSrcBand->GetXSize(), poSrcBand->GetYSize(),
                                            0, 0,
                                            poSrcBand->GetXSize(), poSrcBand->GetYSize() );
                poVRTBand->AddSource( poSimpleSource );
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                    WeightedBroveyWithNoData()                        */
/************************************************************************/

template<class WorkDataType, class OutDataType>
                    void GDALPansharpenOperation::WeightedBroveyWithNoData(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     int nValues,
                                                     int bHasBitDepth,
                                                     WorkDataType nMaxValue)
{
    WorkDataType noData, validValue;
    GDALCopyWord(psOptions->dfNoData, noData);

    if( !(std::numeric_limits<WorkDataType>::is_integer) )
        validValue = noData + 1e-5;
    else if (noData == std::numeric_limits<WorkDataType>::min())
        validValue = std::numeric_limits<WorkDataType>::min() + 1;
    else
        validValue = noData - 1;

    for(int j=0;j<nValues;j++)
    {
        double dfFactor;
        double dfPseudoPanchro = 0;
        for(int i=0;i<psOptions->nInputSpectralBands;i++)
        {
            WorkDataType nSpectralVal = pUpsampledSpectralBuffer[i * nValues + j];
            if( nSpectralVal == noData )
            {
                dfPseudoPanchro = 0.0;
                break;
            }
            dfPseudoPanchro += psOptions->padfWeights[i] * nSpectralVal;
        }
        if( dfPseudoPanchro && pPanBuffer[j] != noData )
        {
            dfFactor = pPanBuffer[j] / dfPseudoPanchro;
            for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
            {
                WorkDataType nRawValue =
                    pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nValues + j];
                WorkDataType nPansharpenedValue;
                GDALCopyWord(nRawValue * dfFactor, nPansharpenedValue);
                if( bHasBitDepth && nPansharpenedValue > nMaxValue )
                    nPansharpenedValue = nMaxValue;
                // We don't want a valid value to be mapped to NoData
                if( nPansharpenedValue == noData )
                    nPansharpenedValue = validValue;
                GDALCopyWord(nPansharpenedValue, pDataBuf[i * nValues + j]);
            }
        }
        else
        {
            for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
            {
                GDALCopyWord(noData, pDataBuf[i * nValues + j]);
            }
        }
    }
}

/************************************************************************/
/*                         WeightedBrovey()                             */
/************************************************************************/

template<class WorkDataType, class OutDataType, int bHasBitDepth>
                    void GDALPansharpenOperation::WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     int nValues,
                                                     WorkDataType nMaxValue)
{
    if( psOptions->bHasNoData )
    {
        WeightedBroveyWithNoData<WorkDataType,OutDataType>
                                (pPanBuffer, pUpsampledSpectralBuffer,
                                 pDataBuf, nValues, bHasBitDepth, nMaxValue);
        return;
    }

    for(int j=0;j<nValues;j++)
    {
        double dfFactor;
        //if( pPanBuffer[j] == 0 )
        //    dfFactor = 1.0;
        //else
        {
            double dfPseudoPanchro = 0;
            for(int i=0;i<psOptions->nInputSpectralBands;i++)
                dfPseudoPanchro += psOptions->padfWeights[i] *
                                pUpsampledSpectralBuffer[i * nValues + j];
            if( dfPseudoPanchro )
                dfFactor = pPanBuffer[j] / dfPseudoPanchro;
            else
                dfFactor = 0.0;
        }

        for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
        {
            WorkDataType nRawValue =
                pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nValues + j];
            WorkDataType nPansharpenedValue;
            GDALCopyWord(nRawValue * dfFactor, nPansharpenedValue);
            if( bHasBitDepth && nPansharpenedValue > nMaxValue )
                nPansharpenedValue = nMaxValue;
            GDALCopyWord(nPansharpenedValue, pDataBuf[i * nValues + j]);
        }
    }
}

template<class WorkDataType, class OutDataType> void GDALPansharpenOperation::WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     int nValues,
                                                     int nBitDepth)
{
    if( nBitDepth == 0 )
        WeightedBrovey<WorkDataType, OutDataType, FALSE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, 0);
    else
    {
        WorkDataType nMaxValue = (WorkDataType)((1 << nBitDepth) - 1);
        WeightedBrovey<WorkDataType, OutDataType, TRUE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nMaxValue);
    }
}

template<class WorkDataType> CPLErr GDALPansharpenOperation::WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     void *pDataBuf, 
                                                     GDALDataType eBufDataType,
                                                     int nValues,
                                                     int nBitDepth)
{
    switch( eBufDataType )
    {
        case GDT_Byte:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GByte*)pDataBuf, nValues, nBitDepth);
            break;

        case GDT_UInt16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt16*)pDataBuf, nValues, nBitDepth);
            break;

        case GDT_Int16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt16*)pDataBuf, nValues, nBitDepth);
            break;

        case GDT_UInt32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt32*)pDataBuf, nValues, nBitDepth);
            break;

        case GDT_Int32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt32*)pDataBuf, nValues, nBitDepth);
            break;

        case GDT_Float32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (float*)pDataBuf, nValues, nBitDepth);
            break;

        case GDT_Float64:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (double*)pDataBuf, nValues, nBitDepth);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported, "eBufDataType not supported");
            return CE_Failure;
            break;
    }

    return CE_None;
}

template<class WorkDataType> CPLErr GDALPansharpenOperation::WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     void *pDataBuf, 
                                                     GDALDataType eBufDataType,
                                                     int nValues)
{
    switch( eBufDataType )
    {
        case GDT_Byte:
            WeightedBrovey<WorkDataType, GByte, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GByte*)pDataBuf, nValues, 0);
            break;

        case GDT_UInt16:
            WeightedBrovey<WorkDataType, GUInt16, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt16*)pDataBuf, nValues, 0);
            break;

        case GDT_Int16:
            WeightedBrovey<WorkDataType, GInt16, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt16*)pDataBuf, nValues, 0);
            break;

        case GDT_UInt32:
            WeightedBrovey<WorkDataType, GUInt32, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt32*)pDataBuf, nValues, 0);
            break;

        case GDT_Int32:
            WeightedBrovey<WorkDataType, GInt32, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt32*)pDataBuf, nValues, 0);
            break;

        case GDT_Float32:
            WeightedBrovey<WorkDataType, float, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (float*)pDataBuf, nValues, 0);
            break;

        case GDT_Float64:
            WeightedBrovey<WorkDataType, double, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (double*)pDataBuf, nValues, 0);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported, "eBufDataType not supported");
            return CE_Failure;
            break;
    }

    return CE_None;
}

/************************************************************************/
/*                           ClampValues()                              */
/************************************************************************/

template< class T >
static void ClampValues(T* panBuffer, int nValues, T nMaxVal)
{
    for(int i=0;i<nValues;i++)
    {
        if( panBuffer[i] > nMaxVal )
            panBuffer[i] = nMaxVal;
    }
}

/************************************************************************/
/*                         ProcessRegion()                              */
/************************************************************************/

/** Executes a pansharpening operation on a rectangular region of the
 * resulting dataset.
 *
 * The window is expressed with respect to the dimensions of the panchromatic
 * band.
 *
 * Spectral bands are upsampled and merged with the panchromatic band according
 * to the select algorithm and options.
 *
 * @param nXOff pixel offset.
 * @param nYOff pixel offset.
 * @param nXSize width of the pansharpened region to compute.
 * @param nYSize height of the pansharpened region to compute.
 * @param pDataBuf output buffer. Must be nXSize * nYSize * 
 *                 (GDALGetDataTypeSize(eBufDataType) / 8) * psOptions->nOutPansharpenedBands large.
 *                 It begins with all values of the first output band, followed
 *                 by values of the second output band, etc...
 * @param eBufDataType data type of the output buffer
 *
 * @return CE_None in case of success, CE_Failure in case of failure.
 *
 * @since GDAL 2.1
 */
CPLErr GDALPansharpenOperation::ProcessRegion(int nXOff, int nYOff,
                                              int nXSize, int nYSize,
                                              void *pDataBuf, 
                                              GDALDataType eBufDataType)
{
    if( psOptions == NULL )
        return CE_Failure;

    // TODO: avoid allocating buffers each time
    GDALRasterBand* poPanchroBand = (GDALRasterBand*)psOptions->hPanchroBand;
    const GDALDataType eWorkDataType = poPanchroBand->GetRasterDataType();
    const int nDataTypeSize = GDALGetDataTypeSize(eWorkDataType) / 8;
    GByte* pUpsampledSpectralBuffer = (GByte*)VSIMalloc3(nXSize, nYSize,
        psOptions->nInputSpectralBands * nDataTypeSize);
    GByte* pPanBuffer = (GByte*)VSIMalloc3(nXSize, nYSize, nDataTypeSize);
    if( pUpsampledSpectralBuffer == NULL || pPanBuffer == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory error while allocating working buffers");
        VSIFree(pUpsampledSpectralBuffer);
        VSIFree(pPanBuffer);
        return CE_Failure;
    }
    
    CPLErr eErr = 
        poPanchroBand->RasterIO(GF_Read,
                nXOff, nYOff, nXSize, nYSize, pPanBuffer, nXSize, nYSize,
                eWorkDataType, 0, 0, NULL);

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    const GDALRIOResampleAlg eResampleAlg = psOptions->eResampleAlg;
    sExtraArg.eResampleAlg = eResampleAlg;
    sExtraArg.bFloatingPointWindowValidity = TRUE;
    double dfRatioX = (double)poPanchroBand->GetXSize() / aMSBands[0]->GetXSize();
    double dfRatioY = (double)poPanchroBand->GetYSize() / aMSBands[0]->GetYSize();
    sExtraArg.dfXOff = nXOff / dfRatioX;
    sExtraArg.dfYOff = nYOff / dfRatioY;
    sExtraArg.dfXSize = nXSize / dfRatioX;
    sExtraArg.dfYSize = nYSize / dfRatioY;
    int nSpectralXOff = (int)(0.49999 + sExtraArg.dfXOff);
    int nSpectralYOff = (int)(0.49999 + sExtraArg.dfYOff);
    if( nSpectralYOff == aMSBands[0]->GetYSize() && (int)sExtraArg.dfYOff == nSpectralYOff - 1)
        nSpectralYOff --;
    int nSpectralXSize = (int)(0.49999 + sExtraArg.dfXSize);
    int nSpectralYSize = (int)(0.49999 + sExtraArg.dfYSize);
    if( nSpectralXSize == 0 )
        nSpectralXSize = 1;
    if( nSpectralYSize == 0 )
        nSpectralYSize = 1;
    
    if( anInputBands.size() )
    {
        // Use dataset RasterIO when possible
        eErr = aMSBands[0]->GetDataset()->RasterIO(GF_Read,
                    nSpectralXOff, nSpectralYOff, nSpectralXSize, nSpectralYSize,
                    pUpsampledSpectralBuffer,
                    nXSize, nYSize,
                    eWorkDataType,
                    (int)anInputBands.size(), &anInputBands[0],
                    0, 0, 0, &sExtraArg);
    }
    else
    {
        for(int i=0; eErr == CE_None && i < psOptions->nInputSpectralBands; i++)
        {
            eErr = aMSBands[i]->RasterIO(GF_Read,
                    nSpectralXOff, nSpectralYOff, nSpectralXSize, nSpectralYSize,
                    pUpsampledSpectralBuffer + i * nXSize * nYSize * nDataTypeSize,
                    nXSize, nYSize,
                    eWorkDataType, 0, 0, &sExtraArg);
        }
    }

    // In case NBITS wasn't set on the spectral bands, clamp the values
    // if overshoot might have occured
    int nBitDepth = psOptions->nBitDepth;
    if( nBitDepth && (eResampleAlg == GRIORA_Cubic ||
                      eResampleAlg == GRIORA_CubicSpline ||
                      eResampleAlg == GRIORA_Lanczos) )
    {
        for(int i=0;i < psOptions->nInputSpectralBands; i++)
        {
            GDALRasterBand* poBand = aMSBands[i];
            int nBandBitDepth = 0;
            const char* pszNBITS = poBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
            if( pszNBITS )
                nBandBitDepth = atoi(pszNBITS);
            if( nBandBitDepth < nBitDepth )
            {
                if( eWorkDataType == GDT_Byte )
                {
                    ClampValues(((GByte*)pUpsampledSpectralBuffer) + i * nXSize * nYSize,
                               nXSize*nYSize,
                               (GByte)((1 << nBitDepth)-1));
                }
                else if( eWorkDataType == GDT_UInt16 )
                {
                    ClampValues(((GUInt16*)pUpsampledSpectralBuffer) + i * nXSize * nYSize,
                               nXSize*nYSize,
                               (GUInt16)((1 << nBitDepth)-1));
                }
                else if( eWorkDataType == GDT_UInt32 )
                {
                    ClampValues(((GUInt32*)pUpsampledSpectralBuffer) + i * nXSize * nYSize,
                               nXSize*nYSize,
                               (GUInt32)((1 << nBitDepth)-1));
                }
            }
        }
    }

    // If all the weights are in [0,x] and their sum is >= 1, no need
    // to try clamping
    if( bWeightsWillNotOvershoot )
    {
        nBitDepth = 0;
    }

    double* padfTempBuffer = NULL;
    GDALDataType eBufDataTypeOri = eBufDataType;
    void* pDataBufOri = pDataBuf;
    // CFloat64 is the query type used by gdallocationinfo...
    if( eBufDataType == GDT_CFloat64 )
    {
        padfTempBuffer = (double*)VSIMalloc3(nXSize, nYSize,
                    psOptions->nOutPansharpenedBands * sizeof(double));
        if( padfTempBuffer == NULL )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                "Out of memory error while allocating working buffers");
            VSIFree(pUpsampledSpectralBuffer);
            VSIFree(pPanBuffer);
            return CE_Failure;
        }
        pDataBuf = padfTempBuffer;
        eBufDataType = GDT_Float64;
    }

    switch( eWorkDataType )
    {
        case GDT_Byte:
            eErr = WeightedBrovey ((GByte*)pPanBuffer,
                                   (GByte*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize, nBitDepth);
            break;

        case GDT_UInt16:
            eErr = WeightedBrovey ((GUInt16*)pPanBuffer,
                                   (GUInt16*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize, nBitDepth);
            break;

        case GDT_Int16:
            eErr = WeightedBrovey ((GInt16*)pPanBuffer,
                                   (GInt16*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize);
            break;

        case GDT_UInt32:
            eErr = WeightedBrovey ((GUInt32*)pPanBuffer,
                                   (GUInt32*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize, nBitDepth);
            break;

        case GDT_Int32:
            eErr = WeightedBrovey ((GInt32*)pPanBuffer,
                                   (GInt32*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize);
            break;

        case GDT_Float32:
            eErr = WeightedBrovey ((float*)pPanBuffer,
                                   (float*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize);
            break;

        case GDT_Float64:
            eErr = WeightedBrovey ((double*)pPanBuffer,
                                   (double*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported, "eWorkDataType not supported");
            eErr = CE_Failure;
            break;
    }

    if( padfTempBuffer )
    {
        GDALCopyWords(padfTempBuffer, GDT_Float64, sizeof(double),
                      pDataBufOri, eBufDataTypeOri,
                      GDALGetDataTypeSize(eBufDataTypeOri)/8,
                      nXSize*nYSize*psOptions->nOutPansharpenedBands);
        VSIFree(padfTempBuffer);
    }

    VSIFree(pUpsampledSpectralBuffer);
    VSIFree(pPanBuffer);
    
    return eErr;
}

/************************************************************************/
/*                             GetOptions()                             */
/************************************************************************/

GDALPansharpenOptions* GDALPansharpenOperation::GetOptions()
{
    return psOptions;
}

/************************************************************************/
/*                     GDALCreatePansharpenOperation()                  */
/************************************************************************/

/** Instanciate a pansharpening operation.
 *
 * The passed options are validated.
 *
 * @param psOptions a pansharpening option structure allocated with
 * GDALCreatePansharpenOptions(). It is duplicated by this function.
 * @return a valid pansharpening operation handle, or NULL in case of failure.
 *
 * @since GDAL 2.1
 */

GDALPansharpenOperationH GDALCreatePansharpenOperation(
                                    const GDALPansharpenOptions* psOptions )
{
    GDALPansharpenOperation* psOperation = new GDALPansharpenOperation();
    if( psOperation->Initialize(psOptions) == CE_None )
        return (GDALPansharpenOperationH)psOperation;
    delete psOperation;
    return NULL;
}

/************************************************************************/
/*                     GDALDestroyPansharpenOperation()                 */
/************************************************************************/

/** Destroy a pansharpening operation.
 *
 * @param hOperation a valid pansharpening operation.
 *
 * @since GDAL 2.1
 */

void GDALDestroyPansharpenOperation( GDALPansharpenOperationH hOperation )
{
    delete (GDALPansharpenOperation*)hOperation;
}

/************************************************************************/
/*                       GDALPansharpenProcessRegion()                  */
/************************************************************************/

/** Executes a pansharpening operation on a rectangular region of the
 * resulting dataset.
 *
 * The window is expressed with respect to the dimensions of the panchromatic
 * band.
 *
 * Spectral bands are upsampled and merged with the panchromatic band according
 * to the select algorithm and options.
 *
 * @param hOperation a valid pansharpening operation.
 * @param nXOff pixel offset.
 * @param nYOff pixel offset.
 * @param nXSize width of the pansharpened region to compute.
 * @param nYSize height of the pansharpened region to compute.
 * @param pDataBuf output buffer. Must be nXSize * nYSize * 
 *                 (GDALGetDataTypeSize(eBufDataType) / 8) * psOptions->nOutPansharpenedBands large.
 *                 It begins with all values of the first output band, followed
 *                 by values of the second output band, etc...
 * @param eBufDataType data type of the output buffer
 *
 * @return CE_None in case of success, CE_Failure in case of failure.
 *
 * @since GDAL 2.1
 */
CPLErr GDALPansharpenProcessRegion( GDALPansharpenOperationH hOperation,
                                    int nXOff, int nYOff,
                                    int nXSize, int nYSize,
                                    void *pDataBuf, 
                                    GDALDataType eBufDataType)
{
    return ((GDALPansharpenOperation*)hOperation)->ProcessRegion(nXOff, nYOff,
                                                        nXSize, nYSize,
                                                        pDataBuf, eBufDataType);
}
