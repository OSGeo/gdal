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
#include "../frmts/mem/memdataset.h"

// Limit types to practical use cases
#define LIMIT_TYPES 1

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
    psNewOptions->nThreads = psOptions->nThreads;
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
    bPositiveWeights = TRUE;
    poThreadPool = NULL;
    nKernelRadius = 0;
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
    delete poThreadPool;
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

    // Detect negative weights
    for(int i=0;i<psOptions->nInputSpectralBands; i++)
    {
        if( psOptions->padfWeights[i] < 0.0 )
        {
            bPositiveWeights = FALSE;
            break;
        }
    }

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
    
    // Setup thread pool
    int nThreads = psOptions->nThreads;
    if( nThreads == -1 )
        nThreads = CPLGetNumCPUs();
    if( nThreads > 1 )
    {
        poThreadPool = new CPLWorkerThreadPool();
        if( !poThreadPool->Setup( CPLGetNumCPUs(), NULL, NULL ) )
        {
            delete poThreadPool;
            poThreadPool = NULL;
        }
    }
    
    GDALRIOResampleAlg eResampleAlg = psOptions->eResampleAlg;
    if( eResampleAlg != GRIORA_NearestNeighbour )
    {
        const char* pszResampling =
            (eResampleAlg == GRIORA_Bilinear) ? "BILINEAR" :
            (eResampleAlg == GRIORA_Cubic) ? "CUBIC" :
            (eResampleAlg == GRIORA_CubicSpline) ? "CUBICSPLINE" :
            (eResampleAlg == GRIORA_Lanczos) ? "LANCZOS" :
            (eResampleAlg == GRIORA_Average) ? "AVERAGE" :
            (eResampleAlg == GRIORA_Mode) ? "MODE" :
            (eResampleAlg == GRIORA_Gauss) ? "GAUSS" : "UNKNOWN";

        GDALGetResampleFunction(pszResampling, &nKernelRadius);
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
                                                     int nBandValues,
                                                     WorkDataType nMaxValue) const
{
    WorkDataType noData, validValue;
    GDALCopyWord(psOptions->dfNoData, noData);

    if( !(std::numeric_limits<WorkDataType>::is_integer) )
        validValue = (WorkDataType)(noData + 1e-5);
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
            WorkDataType nSpectralVal = pUpsampledSpectralBuffer[i * nBandValues + j];
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
                    pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nBandValues + j];
                WorkDataType nPansharpenedValue;
                GDALCopyWord(nRawValue * dfFactor, nPansharpenedValue);
                if( nMaxValue && nPansharpenedValue > nMaxValue )
                    nPansharpenedValue = nMaxValue;
                // We don't want a valid value to be mapped to NoData
                if( nPansharpenedValue == noData )
                    nPansharpenedValue = validValue;
                GDALCopyWord(nPansharpenedValue, pDataBuf[i * nBandValues + j]);
            }
        }
        else
        {
            for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
            {
                GDALCopyWord(noData, pDataBuf[i * nBandValues + j]);
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
                                                     int nBandValues,
                                                     WorkDataType nMaxValue) const
{
    if( psOptions->bHasNoData )
    {
        WeightedBroveyWithNoData<WorkDataType,OutDataType>
                                (pPanBuffer, pUpsampledSpectralBuffer,
                                 pDataBuf, nValues, nBandValues, nMaxValue);
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
                                pUpsampledSpectralBuffer[i * nBandValues + j];
            if( dfPseudoPanchro )
                dfFactor = pPanBuffer[j] / dfPseudoPanchro;
            else
                dfFactor = 0.0;
        }

        for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
        {
            WorkDataType nRawValue =
                pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nBandValues + j];
            WorkDataType nPansharpenedValue;
            GDALCopyWord(nRawValue * dfFactor, nPansharpenedValue);
            if( bHasBitDepth && nPansharpenedValue > nMaxValue )
                nPansharpenedValue = nMaxValue;
            GDALCopyWord(nPansharpenedValue, pDataBuf[i * nBandValues + j]);
        }
    }
}

/* We restrict to 64bit processors because they are guaranteed to have SSE2 */
/* Could possibly be used too on 32bit, but we would need to check at runtime */
#if defined(__x86_64) || defined(_M_X64)

#include <gdalsse_priv.h>

template<int NINPUT, int NOUTPUT>
void GDALPansharpenOperation::WeightedBroveyPositiveWeightsInternal(
                                                     const GUInt16* pPanBuffer,
                                                     const GUInt16* pUpsampledSpectralBuffer,
                                                     GUInt16* pDataBuf,
                                                     int nValues,
                                                     int nBandValues,
                                                     GUInt16 nMaxValue) const
{
    CPLAssert( NINPUT == 3 || NINPUT == 4 );
    const XMMReg2Double w0 = XMMReg2Double::Load1ValHighAndLow(psOptions->padfWeights + 0);
    const XMMReg2Double w1 = XMMReg2Double::Load1ValHighAndLow(psOptions->padfWeights + 1);
    const XMMReg2Double w2 = XMMReg2Double::Load1ValHighAndLow(psOptions->padfWeights + 2);
    const XMMReg2Double w3 = (NINPUT == 3) ? XMMReg2Double::Zero() : XMMReg2Double::Load1ValHighAndLow(psOptions->padfWeights + 3);

    const XMMReg2Double zero = XMMReg2Double::Zero();
    double dfMaxValue = nMaxValue;
    const XMMReg2Double maxValue = XMMReg2Double::Load1ValHighAndLow(&dfMaxValue);
    
    for(int j=0;j<nValues-1;j+=2)
    {
        XMMReg2Double pseudoPanchro = zero;

        pseudoPanchro += w0 * XMMReg2Double::Load2Val(pUpsampledSpectralBuffer + j);
        pseudoPanchro += w1 * XMMReg2Double::Load2Val(pUpsampledSpectralBuffer + nBandValues + j);
        pseudoPanchro += w2 * XMMReg2Double::Load2Val(pUpsampledSpectralBuffer + 2 * nBandValues + j);
        if( NINPUT == 4 )
            pseudoPanchro += w3 * XMMReg2Double::Load2Val(pUpsampledSpectralBuffer + 3 * nBandValues + j);

        /* Little trick to avoid use of ternary operator due to one of the branch being zero */
        XMMReg2Double factor = XMMReg2Double::And(
            XMMReg2Double::NotEquals(pseudoPanchro, zero),
            XMMReg2Double::Load2Val(pPanBuffer + j) / pseudoPanchro );

        for(int i=0;i<NOUTPUT;i++)
        {
            XMMReg2Double rawValue = XMMReg2Double::Load2Val(pUpsampledSpectralBuffer + i * nBandValues + j);
            XMMReg2Double tmp = XMMReg2Double::Min(rawValue * factor, maxValue);
            __m128i tmp2 = _mm_cvtpd_epi32(tmp.xmm); /* Convert the 2 double values to 2 integers */
            pDataBuf[i * nBandValues + j] = (GUInt16)_mm_extract_epi16(tmp2, 0);
            pDataBuf[i * nBandValues + j + 1] = (GUInt16)_mm_extract_epi16(tmp2, 2);
        }
    }
}

#else

template<int NINPUT, int NOUTPUT>
void GDALPansharpenOperation::WeightedBroveyPositiveWeightsInternal(
                                                     const GUInt16* pPanBuffer,
                                                     const GUInt16* pUpsampledSpectralBuffer,
                                                     GUInt16* pDataBuf,
                                                     int nValues,
                                                     int nBandValues,
                                                     GUInt16 nMaxValue) const
{
    CPLAssert( NINPUT == 3 || NINPUT == 4 );
    const double dfw0 = psOptions->padfWeights[0];
    const double dfw1 = psOptions->padfWeights[1];
    const double dfw2 = psOptions->padfWeights[2];
    const double dfw3 = (NINPUT == 3) ? 0 : psOptions->padfWeights[3];
    for(int j=0;j<nValues-1;j+=2)
    {
        double dfFactor, dfFactor2;
        double dfPseudoPanchro = 0;
        double dfPseudoPanchro2 = 0;

        dfPseudoPanchro += dfw0 *
                        pUpsampledSpectralBuffer[j];
        dfPseudoPanchro2 += dfw0 *
                        pUpsampledSpectralBuffer[j + 1];

        dfPseudoPanchro += dfw1 *
                        pUpsampledSpectralBuffer[nBandValues + j];
        dfPseudoPanchro2 += dfw1 *
                        pUpsampledSpectralBuffer[nBandValues + j + 1];

        dfPseudoPanchro += dfw2 *
                        pUpsampledSpectralBuffer[2 * nBandValues + j];
        dfPseudoPanchro2 += dfw2 *
                        pUpsampledSpectralBuffer[2 * nBandValues + j + 1];

        if( NINPUT == 4 )
        {
            dfPseudoPanchro += dfw3 *
                            pUpsampledSpectralBuffer[3 * nBandValues + j];
            dfPseudoPanchro2 += dfw3 *
                            pUpsampledSpectralBuffer[3 * nBandValues + j + 1];
        }

        if( dfPseudoPanchro )
            dfFactor = pPanBuffer[j] / dfPseudoPanchro;
        else
            dfFactor = 0.0;
        if( dfPseudoPanchro2 )
            dfFactor2 = pPanBuffer[j+1] / dfPseudoPanchro2;
        else
            dfFactor2 = 0.0;

        for(int i=0;i<NOUTPUT;i++)
        {
            GUInt16 nRawValue =
                pUpsampledSpectralBuffer[i * nBandValues + j];
            double dfTmp = nRawValue * dfFactor;
            if( dfTmp > nMaxValue )
                pDataBuf[i * nBandValues + j] = nMaxValue;
            else
                pDataBuf[i * nBandValues + j] = (GUInt16)(dfTmp + 0.5);

            GUInt16 nRawValue2 =
                pUpsampledSpectralBuffer[i * nBandValues + j + 1];
            double dfTmp2 = nRawValue2 * dfFactor2;
            if( dfTmp2 > nMaxValue )
                pDataBuf[i * nBandValues + j + 1] = nMaxValue;
            else
                pDataBuf[i * nBandValues + j + 1] = (GUInt16)(dfTmp2 + 0.5);
        }
    }
}
#endif

void GDALPansharpenOperation::WeightedBroveyPositiveWeights(
                                                     const GUInt16* pPanBuffer,
                                                     const GUInt16* pUpsampledSpectralBuffer,
                                                     GUInt16* pDataBuf,
                                                     int nValues,
                                                     int nBandValues,
                                                     GUInt16 nMaxValue) const
{
    if( psOptions->bHasNoData )
    {
        WeightedBroveyWithNoData<GUInt16,GUInt16>
                                (pPanBuffer, pUpsampledSpectralBuffer,
                                 pDataBuf, nValues, nBandValues, nMaxValue);
        return;
    }

    if( nMaxValue == 0 )
        nMaxValue = std::numeric_limits<GUInt16>::max();
    int j;
    if( psOptions->nInputSpectralBands == 3 &&
        psOptions->nOutPansharpenedBands == 3 &&
        psOptions->panOutPansharpenedBands[0] == 0 &&
        psOptions->panOutPansharpenedBands[1] == 1 &&
        psOptions->panOutPansharpenedBands[2] == 2 ) 
    {
        WeightedBroveyPositiveWeightsInternal<3,3>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, nMaxValue);
        j = (nValues / 2) * 2;
    }
    else if( psOptions->nInputSpectralBands == 4 &&
        psOptions->nOutPansharpenedBands == 4 &&
        psOptions->panOutPansharpenedBands[0] == 0 &&
        psOptions->panOutPansharpenedBands[1] == 1 &&
        psOptions->panOutPansharpenedBands[2] == 2 &&
        psOptions->panOutPansharpenedBands[3] == 3 ) 
    {
        WeightedBroveyPositiveWeightsInternal<4,4>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, nMaxValue);
        j = (nValues / 2) * 2;
    }
    else if( psOptions->nInputSpectralBands == 4 &&
        psOptions->nOutPansharpenedBands == 3 &&
        psOptions->panOutPansharpenedBands[0] == 0 &&
        psOptions->panOutPansharpenedBands[1] == 1 &&
        psOptions->panOutPansharpenedBands[2] == 2 ) 
    {
        WeightedBroveyPositiveWeightsInternal<4,3>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, nMaxValue);
        j = (nValues / 2) * 2;
    }
    else
    {
        for(j=0;j<nValues-1;j+=2)
        {
            double dfFactor, dfFactor2;
            double dfPseudoPanchro = 0;
            double dfPseudoPanchro2 = 0;
            for(int i=0;i<psOptions->nInputSpectralBands;i++)
            {
                dfPseudoPanchro += psOptions->padfWeights[i] *
                                pUpsampledSpectralBuffer[i * nBandValues + j];
                dfPseudoPanchro2 += psOptions->padfWeights[i] *
                                pUpsampledSpectralBuffer[i * nBandValues + j + 1];
            }
            if( dfPseudoPanchro )
                dfFactor = pPanBuffer[j] / dfPseudoPanchro;
            else
                dfFactor = 0.0;
            if( dfPseudoPanchro2 )
                dfFactor2 = pPanBuffer[j+1] / dfPseudoPanchro2;
            else
                dfFactor2 = 0.0;

            for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
            {
                GUInt16 nRawValue =
                    pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nBandValues + j];
                double dfTmp = nRawValue * dfFactor;
                if( dfTmp > nMaxValue )
                    pDataBuf[i * nBandValues + j] = nMaxValue;
                else
                    pDataBuf[i * nBandValues + j] = (GUInt16)(dfTmp + 0.5);

                GUInt16 nRawValue2 =
                    pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nBandValues + j + 1];
                double dfTmp2 = nRawValue2 * dfFactor2;
                if( dfTmp2 > nMaxValue )
                    pDataBuf[i * nBandValues + j + 1] = nMaxValue;
                else
                    pDataBuf[i * nBandValues + j + 1] = (GUInt16)(dfTmp2 + 0.5);
            }
        }
    }
    if(j<nValues)
    {
        double dfFactor;
        double dfPseudoPanchro = 0;
        for(int i=0;i<psOptions->nInputSpectralBands;i++)
            dfPseudoPanchro += psOptions->padfWeights[i] *
                            pUpsampledSpectralBuffer[i * nBandValues + j];
        if( dfPseudoPanchro )
            dfFactor = pPanBuffer[j] / dfPseudoPanchro;
        else
            dfFactor = 0.0;

        for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
        {
            GUInt16 nRawValue =
                pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nBandValues + j];
            double dfTmp = nRawValue * dfFactor;
            if( dfTmp > nMaxValue )
                pDataBuf[i * nBandValues + j] = nMaxValue;
            else
                pDataBuf[i * nBandValues + j] = (GUInt16)(dfTmp + 0.5);
        }
    }
}

template<class WorkDataType, class OutDataType> void GDALPansharpenOperation::WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     int nValues,
                                                     int nBandValues,
                                                     WorkDataType nMaxValue) const
{
    if( nMaxValue == 0 )
        WeightedBrovey<WorkDataType, OutDataType, FALSE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, 0);
    else
    {
        WeightedBrovey<WorkDataType, OutDataType, TRUE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, nMaxValue);
    }
}

template<>
void GDALPansharpenOperation::WeightedBrovey<GUInt16,GUInt16>(
                                                     const GUInt16* pPanBuffer,
                                                     const GUInt16* pUpsampledSpectralBuffer,
                                                     GUInt16* pDataBuf,
                                                     int nValues,
                                                     int nBandValues,
                                                     GUInt16 nMaxValue) const
{
    if( bPositiveWeights )
    {
        WeightedBroveyPositiveWeights(
                pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, nMaxValue);
    }
    else if( nMaxValue == 0 )
    {
        WeightedBrovey<GUInt16, GUInt16, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, 0);
    }
    else
    {
        WeightedBrovey<GUInt16, GUInt16, TRUE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues, nBandValues, nMaxValue);
    }
}

template<class WorkDataType> CPLErr GDALPansharpenOperation::WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     void *pDataBuf, 
                                                     GDALDataType eBufDataType,
                                                     int nValues,
                                                     int nBandValues,
                                                     WorkDataType nMaxValue) const
{
    switch( eBufDataType )
    {
        case GDT_Byte:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GByte*)pDataBuf, nValues, nBandValues, nMaxValue);
            break;

        case GDT_UInt16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt16*)pDataBuf, nValues, nBandValues, nMaxValue);
            break;

#ifndef LIMIT_TYPES
        case GDT_Int16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt16*)pDataBuf, nValues, nBandValues, nMaxValue);
            break;

        case GDT_UInt32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt32*)pDataBuf, nValues, nBandValues, nMaxValue);
            break;

        case GDT_Int32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt32*)pDataBuf, nValues, nBandValues, nMaxValue);
            break;

        case GDT_Float32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (float*)pDataBuf, nValues, nBandValues, nMaxValue);
            break;
#endif

        case GDT_Float64:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (double*)pDataBuf, nValues, nBandValues, nMaxValue);
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
                                                     int nValues, int nBandValues) const
{
    switch( eBufDataType )
    {
        case GDT_Byte:
            WeightedBrovey<WorkDataType, GByte, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GByte*)pDataBuf, nValues, nBandValues, 0);
            break;

        case GDT_UInt16:
            WeightedBrovey<WorkDataType, GUInt16, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt16*)pDataBuf, nValues, nBandValues, 0);
            break;

#ifndef LIMIT_TYPES
        case GDT_Int16:
            WeightedBrovey<WorkDataType, GInt16, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt16*)pDataBuf, nValues, nBandValues, 0);
            break;

        case GDT_UInt32:
            WeightedBrovey<WorkDataType, GUInt32, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt32*)pDataBuf, nValues, nBandValues, 0);
            break;

        case GDT_Int32:
            WeightedBrovey<WorkDataType, GInt32, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt32*)pDataBuf, nValues, nBandValues, 0);
            break;

        case GDT_Float32:
            WeightedBrovey<WorkDataType, float, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (float*)pDataBuf, nValues, nBandValues, 0);
            break;
#endif

        case GDT_Float64:
            WeightedBrovey<WorkDataType, double, FALSE>(pPanBuffer, pUpsampledSpectralBuffer,
                           (double*)pDataBuf, nValues, nBandValues, 0);
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
    GDALDataType eWorkDataType = poPanchroBand->GetRasterDataType();
#ifdef LIMIT_TYPES
    if( eWorkDataType != GDT_Byte && eWorkDataType != GDT_UInt16 )
        eWorkDataType = GDT_Float64;
#endif
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
    
    int nTasks = 0;
    if( poThreadPool )
    {
        nTasks = poThreadPool->GetThreadCount();
        if( nTasks > nYSize )
            nTasks = nYSize;
    }
    
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
    int nSpectralXOff = (int)(sExtraArg.dfXOff);
    int nSpectralYOff = (int)(sExtraArg.dfYOff);
    int nSpectralXSize = (int)(0.49999 + sExtraArg.dfXSize);
    int nSpectralYSize = (int)(0.49999 + sExtraArg.dfYSize);
    if( nSpectralXSize == 0 )
        nSpectralXSize = 1;
    if( nSpectralYSize == 0 )
        nSpectralYSize = 1;
    
    // For the multi-threaded case, extract the multispectral data at
    // full resolution in a temp buffer, and then do the upsampling in chunks
    if( nTasks > 1 && nSpectralXSize < nXSize && nSpectralYSize < nYSize &&
        eResampleAlg != GRIORA_NearestNeighbour && nYSize > 1 )
    {
        // Take some margin to take into account the radius of the resampling kernel
        int nXOffExtract = nSpectralXOff - nKernelRadius;
        int nYOffExtract = nSpectralYOff - nKernelRadius;
        int nXSizeExtract = nSpectralXSize + 1 + 2 * nKernelRadius;
        int nYSizeExtract = nSpectralYSize + 1 + 2 * nKernelRadius;
        if( nXOffExtract < 0 )
        {
            nXSizeExtract += nXOffExtract;
            nXOffExtract = 0;
        }
        if( nYOffExtract < 0 )
        {
            nYSizeExtract += nYOffExtract;
            nYOffExtract = 0;
        }
        if( nXOffExtract + nXSizeExtract > aMSBands[0]->GetXSize() )
            nXSizeExtract = aMSBands[0]->GetXSize() - nXOffExtract;
        if( nYOffExtract + nYSizeExtract > aMSBands[0]->GetYSize() )
            nYSizeExtract = aMSBands[0]->GetYSize() - nYOffExtract;
        
        GByte* pSpectralBuffer = (GByte*)VSIMalloc3(nXSizeExtract, nYSizeExtract,
                            psOptions->nInputSpectralBands * nDataTypeSize);
        if( pSpectralBuffer == NULL )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory error while allocating working buffers");
            VSIFree(pUpsampledSpectralBuffer);
            VSIFree(pPanBuffer);
            return CE_Failure;
        }
        
        if( anInputBands.size() )
        {
            // Use dataset RasterIO when possible
            eErr = aMSBands[0]->GetDataset()->RasterIO(GF_Read,
                        nXOffExtract, nYOffExtract,
                        nXSizeExtract, nYSizeExtract,
                        pSpectralBuffer,
                        nXSizeExtract, nYSizeExtract,
                        eWorkDataType,
                        (int)anInputBands.size(), &anInputBands[0],
                        0, 0, 0, NULL);
        }
        else
        {
            for(int i=0; eErr == CE_None && i < psOptions->nInputSpectralBands; i++)
            {
                eErr = aMSBands[i]->RasterIO(GF_Read,
                        nXOffExtract, nYOffExtract,
                        nXSizeExtract, nYSizeExtract,
                        pSpectralBuffer + (size_t)i * nXSizeExtract * nYSizeExtract * nDataTypeSize,
                        nXSizeExtract, nYSizeExtract,
                        eWorkDataType, 0, 0, NULL);
            }
        }
    
        /* Create a MEM dataset that wraps the input buffer */
        GDALDataset* poMEMDS = MEMDataset::Create("", nXSizeExtract, nYSizeExtract, 0,
                                                  eWorkDataType, NULL);
        char szBuffer[64];
        int nRet;

        for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
        {
            nRet = CPLPrintPointer(szBuffer,
                       pSpectralBuffer + (size_t)i * nDataTypeSize * nXSizeExtract * nYSizeExtract, sizeof(szBuffer));
            szBuffer[nRet] = 0;
            char** papszOptions = CSLSetNameValue(NULL, "DATAPOINTER", szBuffer);

            papszOptions = CSLSetNameValue(papszOptions, "PIXELOFFSET",
                CPLSPrintf(CPL_FRMT_GIB, (GIntBig)nDataTypeSize));

            papszOptions = CSLSetNameValue(papszOptions, "LINEOFFSET",
                CPLSPrintf(CPL_FRMT_GIB, (GIntBig)nDataTypeSize * nXSizeExtract));

            poMEMDS->AddBand(eWorkDataType, papszOptions);
            CSLDestroy(papszOptions);

            const char* pszNBITS = aMSBands[i]->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
            if( pszNBITS )
                poMEMDS->GetRasterBand(i+1)->SetMetadataItem("NBITS", pszNBITS, "IMAGE_STRUCTURE");
        }

#if 0
        nSpectralXOff -= nXOffExtract;
        nSpectralYOff -= nYOffExtract;
        sExtraArg.dfXOff -= nXOffExtract;
        sExtraArg.dfYOff -= nYOffExtract;
        poMEMDS->RasterIO(GF_Read,
                          nSpectralXOff,
                          nSpectralYOff,
                          nSpectralXSize, nSpectralYSize,
                          pUpsampledSpectralBuffer, nXSize, nYSize,
                          eWorkDataType,
                          psOptions->nInputSpectralBands, NULL,
                          0, 0, 0,
                          &sExtraArg);
#else
        // We are abusing the contract of the GDAL API by using the MEMDataset
        // from several threads. In this case, this is safe. In case that would
        // no longer be the case we could create as many MEMDataset as threads
        // pointing to the same buffer.
        
        std::vector<GDALPansharpenResampleJob> anJobs;
        anJobs.resize( nTasks );
        for( int i=0;i<nTasks;i++)
        {
            size_t iStartLine = ((size_t)i * nYSize) / nTasks;
            size_t iNextStartLine = ((size_t)(i+1) * nYSize) / nTasks;
            anJobs[i].poMEMDS = poMEMDS;
            anJobs[i].eResampleAlg = eResampleAlg;
            anJobs[i].dfXOff = sExtraArg.dfXOff - nXOffExtract;
            anJobs[i].dfYOff = (nYOff + iStartLine) / dfRatioY - nYOffExtract;
            anJobs[i].dfXSize = sExtraArg.dfXSize;
            anJobs[i].dfYSize = (iNextStartLine - iStartLine) / dfRatioY;
            anJobs[i].nXOff = (int)anJobs[i].dfXOff;
            anJobs[i].nYOff = (int)anJobs[i].dfYOff;
            anJobs[i].nXSize = (int)(0.4999 + anJobs[i].dfXSize);
            anJobs[i].nYSize = (int)(0.4999 + anJobs[i].dfYSize);
            if( anJobs[i].nXSize == 0 )
                anJobs[i].nXSize = 1;
            if( anJobs[i].nYSize == 0 )
                anJobs[i].nYSize = 1;
            anJobs[i].pBuffer = pUpsampledSpectralBuffer + (size_t)iStartLine * nXSize * nDataTypeSize;
            anJobs[i].eDT = eWorkDataType;
            anJobs[i].nBufXSize = nXSize;
            anJobs[i].nBufYSize = (int)(iNextStartLine - iStartLine);
            anJobs[i].nBandCount = psOptions->nInputSpectralBands;
            anJobs[i].nBandSpace = (GSpacing)nXSize * nYSize * nDataTypeSize;
            poThreadPool->SubmitJob(PansharpenResampleJobThreadFunc, &(anJobs[i]));
        }
        poThreadPool->WaitCompletion();
#endif

        GDALClose(poMEMDS);
        
        VSIFree(pSpectralBuffer);
    }
    else
    {
        if( anInputBands.size() )
        {
            // Use dataset RasterIO when possible
            eErr = aMSBands[0]->GetDataset()->RasterIO(GF_Read,
                        nSpectralXOff, nSpectralYOff,
                        nSpectralXSize, nSpectralYSize,
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
                        nSpectralXOff, nSpectralYOff,
                        nSpectralXSize, nSpectralYSize,
                        pUpsampledSpectralBuffer + (size_t)i * nXSize * nYSize * nDataTypeSize,
                        nXSize, nYSize,
                        eWorkDataType, 0, 0, &sExtraArg);
            }
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
#ifndef LIMIT_TYPES
                else if( eWorkDataType == GDT_UInt32 )
                {
                    ClampValues(((GUInt32*)pUpsampledSpectralBuffer) + i * nXSize * nYSize,
                               nXSize*nYSize,
                               (GUInt32)((1 << nBitDepth)-1));
                }
#endif
            }
        }
    }

    GUInt32 nMaxValue = (1 << nBitDepth) - 1;

    double* padfTempBuffer = NULL;
    GDALDataType eBufDataTypeOri = eBufDataType;
    void* pDataBufOri = pDataBuf;
    // CFloat64 is the query type used by gdallocationinfo...
#ifdef LIMIT_TYPES
    if( eBufDataType != GDT_Byte && eBufDataType != GDT_UInt16 )
#else
    if( eBufDataType == GDT_CFloat64 )
#endif
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
    
    if( nTasks > 1 )
    {
        std::vector<GDALPansharpenJob> anJobs;
        anJobs.resize( nTasks );
        
        for( int i=0;i<nTasks;i++)
        {
            size_t iStartLine = ((size_t)i * nYSize) / nTasks;
            size_t iNextStartLine = ((size_t)(i+1) * nYSize) / nTasks;
            anJobs[i].poPansharpenOperation = this;
            anJobs[i].eWorkDataType = eWorkDataType;
            anJobs[i].eBufDataType = eBufDataType;
            anJobs[i].pPanBuffer = pPanBuffer + iStartLine *  nXSize * nDataTypeSize;
            anJobs[i].pUpsampledSpectralBuffer = pUpsampledSpectralBuffer + iStartLine * nXSize * nDataTypeSize;
            anJobs[i].pDataBuf = (GByte*)pDataBuf + iStartLine * nXSize * (GDALGetDataTypeSize(eBufDataType) / 8);
            anJobs[i].nValues = (int)(iNextStartLine - iStartLine) * nXSize;
            anJobs[i].nBandValues = nXSize * nYSize;
            anJobs[i].nMaxValue = nMaxValue;
            poThreadPool->SubmitJob(PansharpenJobThreadFunc, &(anJobs[i]));
        }
        poThreadPool->WaitCompletion();
        eErr = CE_None;
        for( int i=0;i<nTasks;i++)
        {
            if( anJobs[i].eErr != CE_None )
                eErr = CE_Failure;
        }
    }
    else
    {
        eErr = PansharpenChunk( eWorkDataType, eBufDataType,
                                pPanBuffer,
                                pUpsampledSpectralBuffer,
                                pDataBuf,
                                nXSize * nYSize,
                                nXSize * nYSize,
                                nMaxValue);
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
/*                   PansharpenResampleJobThreadFunc()                  */
/************************************************************************/

void GDALPansharpenOperation::PansharpenResampleJobThreadFunc(void* pUserData)
{
    GDALPansharpenResampleJob* psJob = (GDALPansharpenResampleJob*) pUserData;
    
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.eResampleAlg = psJob->eResampleAlg;
    sExtraArg.bFloatingPointWindowValidity = TRUE;
    sExtraArg.dfXOff = psJob->dfXOff;
    sExtraArg.dfYOff = psJob->dfYOff;
    sExtraArg.dfXSize = psJob->dfXSize;
    sExtraArg.dfYSize = psJob->dfYSize;

    psJob->poMEMDS->RasterIO(GF_Read,
                             psJob->nXOff,
                             psJob->nYOff,
                             psJob->nXSize,
                             psJob->nYSize,
                             psJob->pBuffer,
                             psJob->nBufXSize,
                             psJob->nBufYSize,
                             psJob->eDT,
                             psJob->nBandCount,
                             NULL,
                             0, 0, psJob->nBandSpace,
                             &sExtraArg);
}

/************************************************************************/
/*                      PansharpenJobThreadFunc()                       */
/************************************************************************/

void GDALPansharpenOperation::PansharpenJobThreadFunc(void* pUserData)
{
    GDALPansharpenJob* psJob = (GDALPansharpenJob*) pUserData;
    psJob->eErr = psJob->poPansharpenOperation->PansharpenChunk(psJob->eWorkDataType,
                                  psJob->eBufDataType,
                                  psJob->pPanBuffer,
                                  psJob->pUpsampledSpectralBuffer,
                                  psJob->pDataBuf,
                                  psJob->nValues,
                                  psJob->nBandValues,
                                  psJob->nMaxValue);
}

/************************************************************************/
/*                           PansharpenChunk()                          */
/************************************************************************/
        
CPLErr GDALPansharpenOperation::PansharpenChunk( GDALDataType eWorkDataType,
                                                 GDALDataType eBufDataType,
                                                 const void* pPanBuffer,
                                                 const void* pUpsampledSpectralBuffer,
                                                 void* pDataBuf,
                                                 int nValues,
                                                 int nBandValues,
                                                 GUInt32 nMaxValue) const
{
    CPLErr eErr;

    switch( eWorkDataType )
    {
        case GDT_Byte:
            eErr = WeightedBrovey ((GByte*)pPanBuffer,
                                   (GByte*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nValues, nBandValues, (GByte)nMaxValue);
            break;

        case GDT_UInt16:
            eErr = WeightedBrovey ((GUInt16*)pPanBuffer,
                                   (GUInt16*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nValues, nBandValues, (GUInt16)nMaxValue);
            break;

#ifndef LIMIT_TYPES
        case GDT_Int16:
            eErr = WeightedBrovey ((GInt16*)pPanBuffer,
                                   (GInt16*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nValues, nBandValues);
            break;

        case GDT_UInt32:
            eErr = WeightedBrovey ((GUInt32*)pPanBuffer,
                                   (GUInt32*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nValues, nBandValues, nMaxValue);
            break;

        case GDT_Int32:
            eErr = WeightedBrovey ((GInt32*)pPanBuffer,
                                   (GInt32*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nValues, nBandValues);
            break;

        case GDT_Float32:
            eErr = WeightedBrovey ((float*)pPanBuffer,
                                   (float*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nValues, nBandValues);
            break;
#endif
        case GDT_Float64:
            eErr = WeightedBrovey ((double*)pPanBuffer,
                                   (double*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nValues, nBandValues);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported, "eWorkDataType not supported");
            eErr = CE_Failure;
            break;
    }
    
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
