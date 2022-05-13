/******************************************************************************
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

#include "cpl_port.h"
#include "gdalpansharpen.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_vsi.h"
#include "../frmts/mem/memdataset.h"
#include "../frmts/vrt/vrtdataset.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
// #include "gdalsse_priv.h"

// Limit types to practical use cases.
#define LIMIT_TYPES 1

CPL_CVSID("$Id$")

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
    GDALPansharpenOptions* psOptions = static_cast<GDALPansharpenOptions *>(
        CPLCalloc(1, sizeof(GDALPansharpenOptions)));
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
    if( psOptions == nullptr )
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
        psNewOptions->padfWeights = static_cast<double *>(
            CPLMalloc(sizeof(double) * psOptions->nWeightCount));
        memcpy(psNewOptions->padfWeights,
               psOptions->padfWeights,
               sizeof(double) * psOptions->nWeightCount);
    }
    psNewOptions->hPanchroBand = psOptions->hPanchroBand;
    psNewOptions->nInputSpectralBands = psOptions->nInputSpectralBands;
    if( psOptions->pahInputSpectralBands )
    {
        const size_t nSize = sizeof(GDALRasterBandH) *
                                psOptions->nInputSpectralBands;
        psNewOptions->pahInputSpectralBands = static_cast<GDALRasterBandH *>(
            CPLMalloc(nSize));
        memcpy(psNewOptions->pahInputSpectralBands,
               psOptions->pahInputSpectralBands,
               nSize);
    }
    psNewOptions->nOutPansharpenedBands = psOptions->nOutPansharpenedBands;
    if( psOptions->panOutPansharpenedBands )
    {
        psNewOptions->panOutPansharpenedBands =  static_cast<int *>(
            CPLMalloc(sizeof(int) * psOptions->nOutPansharpenedBands));
        memcpy(psNewOptions->panOutPansharpenedBands,
               psOptions->panOutPansharpenedBands,
               sizeof(int) * psOptions->nOutPansharpenedBands);
    }
    psNewOptions->bHasNoData = psOptions->bHasNoData;
    psNewOptions->dfNoData = psOptions->dfNoData;
    psNewOptions->nThreads = psOptions->nThreads;
    psNewOptions->dfMSShiftX = psOptions->dfMSShiftX;
    psNewOptions->dfMSShiftY = psOptions->dfMSShiftY;
    return psNewOptions;
}

/************************************************************************/
/*                        GDALPansharpenOperation()                     */
/************************************************************************/

/** Pansharpening operation constructor.
 *
 * The object is ready to be used after Initialize() has been called.
 */
GDALPansharpenOperation::GDALPansharpenOperation() = default;

/************************************************************************/
/*                       ~GDALPansharpenOperation()                     */
/************************************************************************/

/** Pansharpening operation destructor.
 */

GDALPansharpenOperation::~GDALPansharpenOperation()
{
    GDALDestroyPansharpenOptions(psOptions);
    for( size_t i = 0; i < aVDS.size(); i++ )
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
CPLErr
GDALPansharpenOperation::Initialize( const GDALPansharpenOptions* psOptionsIn )
{
    if( psOptionsIn->hPanchroBand == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hPanchroBand not set");
        return CE_Failure;
    }
    if( psOptionsIn->nInputSpectralBands <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No input spectral bands defined");
        return CE_Failure;
    }
    if( psOptionsIn->padfWeights == nullptr ||
        psOptionsIn->nWeightCount != psOptionsIn->nInputSpectralBands )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No weights defined, or not the same number as input "
                 "spectral bands");
        return CE_Failure;
    }
    GDALRasterBandH hRefBand = psOptionsIn->pahInputSpectralBands[0];
    int bSameDataset = psOptionsIn->nInputSpectralBands > 1;
    if( bSameDataset )
        anInputBands.push_back(GDALGetBandNumber(hRefBand));
    for( int i = 1; i < psOptionsIn->nInputSpectralBands; i++ )
    {
        GDALRasterBandH hBand = psOptionsIn->pahInputSpectralBands[i];
        if( GDALGetRasterBandXSize(hBand) != GDALGetRasterBandXSize(hRefBand) ||
            GDALGetRasterBandYSize(hBand) != GDALGetRasterBandYSize(hRefBand) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Dimensions of input spectral band %d different from "
                     "first spectral band",
                     i);
            return CE_Failure;
        }
        if( bSameDataset )
        {
            if( GDALGetBandDataset(hBand) == nullptr ||
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
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No output pansharpened band defined");
    }
    for( int i = 0; i < psOptionsIn->nOutPansharpenedBands; i++ )
    {
        if( psOptionsIn->panOutPansharpenedBands[i] < 0 ||
            psOptionsIn->panOutPansharpenedBands[i] >=
            psOptionsIn->nInputSpectralBands )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value panOutPansharpenedBands[%d] = %d",
                     i, psOptionsIn->panOutPansharpenedBands[i]);
            return CE_Failure;
        }
    }

    GDALRasterBand* poPanchroBand = GDALRasterBand::FromHandle(
                                                    psOptionsIn->hPanchroBand);
    GDALDataType eWorkDataType = poPanchroBand->GetRasterDataType();
    if( psOptionsIn->nBitDepth )
    {
        if( psOptionsIn->nBitDepth < 0 || psOptionsIn->nBitDepth > 31 ||
            (eWorkDataType == GDT_Byte && psOptionsIn->nBitDepth > 8) ||
            (eWorkDataType == GDT_UInt16 && psOptionsIn->nBitDepth > 16) )
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
          eWorkDataType == GDT_UInt32 || eWorkDataType == GDT_UInt64) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Ignoring nBitDepth = %d for type %s",
                 psOptions->nBitDepth, GDALGetDataTypeName(eWorkDataType));
        psOptions->nBitDepth = 0;
    }

    // Detect negative weights.
    for( int i = 0; i<psOptions->nInputSpectralBands; i++ )
    {
        if( psOptions->padfWeights[i] < 0.0 )
        {
            bPositiveWeights = FALSE;
            break;
        }
    }

    for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
    {
        aMSBands.push_back( GDALRasterBand::FromHandle(
                                        psOptions->pahInputSpectralBands[i]) );
    }

    if( psOptions->bHasNoData )
    {
        bool bNeedToWrapInVRT = false;
        for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
        {
            GDALRasterBand* poBand = GDALRasterBand::FromHandle(
                                        psOptions->pahInputSpectralBands[i]);
            int bHasNoData = FALSE;
            double dfNoData = poBand->GetNoDataValue(&bHasNoData);
            if( !bHasNoData || dfNoData != psOptions->dfNoData )
                bNeedToWrapInVRT = true;
        }

        if( bNeedToWrapInVRT )
        {
            // Wrap spectral bands in a VRT if they don't have the nodata value.
            VRTDataset* poVDS = nullptr;
            for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
            {
                GDALRasterBand* poSrcBand = aMSBands[i];
                int iVRTBand = 1;
                if( anInputBands.empty() || i == 0 )
                {
                    poVDS = new VRTDataset(poSrcBand->GetXSize(), poSrcBand->GetYSize());
                    aVDS.push_back(poVDS);
                }
                if( !anInputBands.empty() )
                {
                    anInputBands[i] = i + 1;
                    iVRTBand = i + 1;
                }
                poVDS->AddBand(poSrcBand->GetRasterDataType(), nullptr);
                VRTSourcedRasterBand* poVRTBand =
                    dynamic_cast<VRTSourcedRasterBand*>(
                        poVDS->GetRasterBand(iVRTBand));
                if( poVRTBand == nullptr )
                    return CE_Failure;
                aMSBands[i] = poVRTBand;
                poVRTBand->SetNoDataValue(psOptions->dfNoData);
                const char* pszNBITS =
                    poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
                if( pszNBITS )
                    poVRTBand->SetMetadataItem("NBITS", pszNBITS,
                                               "IMAGE_STRUCTURE");

                VRTSimpleSource* poSimpleSource = new VRTSimpleSource();
                poVRTBand->ConfigureSource(poSimpleSource,
                                           poSrcBand,
                                           FALSE,
                                           0, 0,
                                           poSrcBand->GetXSize(),
                                           poSrcBand->GetYSize(),
                                           0, 0,
                                           poSrcBand->GetXSize(),
                                           poSrcBand->GetYSize());
                poVRTBand->AddSource( poSimpleSource );
            }
        }
    }

    // Setup thread pool.
    int nThreads = psOptions->nThreads;
    if( nThreads == -1 )
        nThreads = CPLGetNumCPUs();
    else if( nThreads == 0 )
    {
        const char* pszNumThreads =
            CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
        if( pszNumThreads )
        {
            if( EQUAL(pszNumThreads, "ALL_CPUS") )
                nThreads = CPLGetNumCPUs();
            else
                nThreads = std::max(0, std::min(128, atoi(pszNumThreads)));
        }
    }
    if( nThreads > 1 )
    {
        CPLDebug("PANSHARPEN", "Using %d threads", nThreads);
        poThreadPool = new (std::nothrow) CPLWorkerThreadPool();
        // coverity[tainted_data]
        if( poThreadPool == nullptr ||
            !poThreadPool->Setup( nThreads, nullptr, nullptr ) )
        {
            delete poThreadPool;
            poThreadPool = nullptr;
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
            (eResampleAlg == GRIORA_RMS) ? "RMS" :
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
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     WorkDataType nMaxValue) const
{
    WorkDataType noData, validValue;
    GDALCopyWord(psOptions->dfNoData, noData);

    if( !(std::numeric_limits<WorkDataType>::is_integer) )
        validValue = static_cast<WorkDataType>(noData + 1e-5);
    else if( noData == std::numeric_limits<WorkDataType>::min() )
        validValue = std::numeric_limits<WorkDataType>::min() + 1;
    else
        validValue = noData - 1;

    for( size_t j = 0; j < nValues; j++ )
    {
        double dfPseudoPanchro = 0.0;
        for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
        {
            WorkDataType nSpectralVal =
                pUpsampledSpectralBuffer[i * nBandValues + j];
            if( nSpectralVal == noData )
            {
                dfPseudoPanchro = 0.0;
                break;
            }
            dfPseudoPanchro += psOptions->padfWeights[i] * nSpectralVal;
        }
        if( dfPseudoPanchro != 0.0 && pPanBuffer[j] != noData )
        {
            const double dfFactor = pPanBuffer[j] / dfPseudoPanchro;
            for( int i = 0; i < psOptions->nOutPansharpenedBands; i++ )
            {
                WorkDataType nRawValue =
                    pUpsampledSpectralBuffer[
                        psOptions->panOutPansharpenedBands[i] * nBandValues +
                        j];
                WorkDataType nPansharpenedValue;
                GDALCopyWord(nRawValue * dfFactor, nPansharpenedValue);
                if( nMaxValue != 0 && nPansharpenedValue > nMaxValue )
                    nPansharpenedValue = nMaxValue;
                // We don't want a valid value to be mapped to NoData.
                if( nPansharpenedValue == noData )
                    nPansharpenedValue = validValue;
                GDALCopyWord(nPansharpenedValue, pDataBuf[i * nBandValues + j]);
            }
        }
        else
        {
            for( int i = 0; i < psOptions->nOutPansharpenedBands; i++ )
            {
                GDALCopyWord(noData, pDataBuf[i * nBandValues + j]);
            }
        }
    }
}

/************************************************************************/
/*                           ComputeFactor()                            */
/************************************************************************/

template<class T> static inline double ComputeFactor(T panValue,
                                                     double dfPseudoPanchro)
{
    if( dfPseudoPanchro == 0.0 )
        return 0.0;

    return panValue / dfPseudoPanchro;
}

/************************************************************************/
/*                           ClampAndRound()                            */
/************************************************************************/

template<class T> static inline T ClampAndRound(double dfVal, T nMaxValue)
{
    if( dfVal > nMaxValue )
        return nMaxValue;
    else
        return static_cast<T>(dfVal + 0.5);
}

/************************************************************************/
/*                         WeightedBrovey()                             */
/************************************************************************/

template<class WorkDataType, class OutDataType, int bHasBitDepth>
void GDALPansharpenOperation::WeightedBrovey3(
    const WorkDataType* pPanBuffer,
    const WorkDataType* pUpsampledSpectralBuffer,
    OutDataType* pDataBuf,
    size_t nValues,
    size_t nBandValues,
    WorkDataType nMaxValue) const
{
    if( psOptions->bHasNoData )
    {
        WeightedBroveyWithNoData<WorkDataType, OutDataType>
                                (pPanBuffer, pUpsampledSpectralBuffer,
                                 pDataBuf, nValues, nBandValues, nMaxValue);
        return;
    }

    for( size_t j = 0; j < nValues; j++ )
    {
        double dfFactor = 0.0;
        // if( pPanBuffer[j] == 0 )
        //     dfFactor = 1.0;
        // else
        {
            double dfPseudoPanchro = 0.0;
            for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
                dfPseudoPanchro += psOptions->padfWeights[i] *
                                pUpsampledSpectralBuffer[i * nBandValues + j];
            dfFactor = ComputeFactor(pPanBuffer[j], dfPseudoPanchro);
        }

        for( int i = 0; i < psOptions->nOutPansharpenedBands; i++ )
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

#include "gdalsse_priv.h"

template<class T, int NINPUT, int NOUTPUT>
size_t GDALPansharpenOperation::WeightedBroveyPositiveWeightsInternal(
                                                     const T* pPanBuffer,
                                                     const T* pUpsampledSpectralBuffer,
                                                     T* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     T nMaxValue) const
{
    CPL_STATIC_ASSERT( NINPUT == 3 || NINPUT == 4 );
    CPL_STATIC_ASSERT( NOUTPUT == 3 || NOUTPUT == 4 );
    const XMMReg4Double w0 = XMMReg4Double::Load1ValHighAndLow(psOptions->padfWeights + 0);
    const XMMReg4Double w1 = XMMReg4Double::Load1ValHighAndLow(psOptions->padfWeights + 1);
    const XMMReg4Double w2 = XMMReg4Double::Load1ValHighAndLow(psOptions->padfWeights + 2);
    const XMMReg4Double w3 = (NINPUT == 3) ? XMMReg4Double::Zero() :
                    XMMReg4Double::Load1ValHighAndLow(psOptions->padfWeights + 3);

    const XMMReg4Double zero = XMMReg4Double::Zero();
    double dfMaxValue = nMaxValue;
    const XMMReg4Double maxValue =
        XMMReg4Double::Load1ValHighAndLow(&dfMaxValue);

    size_t j = 0;  // Used after for.
    for( ; j + 3 < nValues; j += 4 )
    {
        XMMReg4Double pseudoPanchro = zero;

        XMMReg4Double val0 = XMMReg4Double::Load4Val(pUpsampledSpectralBuffer + 0 * nBandValues + j);
        XMMReg4Double val1 = XMMReg4Double::Load4Val(pUpsampledSpectralBuffer + 1 * nBandValues + j);
        XMMReg4Double val2 = XMMReg4Double::Load4Val(pUpsampledSpectralBuffer + 2 * nBandValues + j);
        XMMReg4Double val3;
        if( NINPUT == 4 || NOUTPUT == 4 )
        {
            val3 = XMMReg4Double::Load4Val(pUpsampledSpectralBuffer + 3 * nBandValues + j);
        }

        pseudoPanchro += w0 * val0;
        pseudoPanchro += w1 * val1;
        pseudoPanchro += w2 * val2;
        if( NINPUT == 4 )
            pseudoPanchro += w3 * val3;

        /* Little trick to avoid use of ternary operator due to one of the branch being zero */
        XMMReg4Double factor = XMMReg4Double::And(
            XMMReg4Double::NotEquals(pseudoPanchro, zero),
            XMMReg4Double::Load4Val(pPanBuffer + j) / pseudoPanchro );

        val0 = XMMReg4Double::Min(val0 * factor, maxValue);
        val1 = XMMReg4Double::Min(val1 * factor, maxValue);
        val2 = XMMReg4Double::Min(val2 * factor, maxValue);
        if( NOUTPUT == 4 )
        {
            val3 = XMMReg4Double::Min(val3 * factor, maxValue);
        }
        val0.Store4Val(pDataBuf + 0 * nBandValues + j);
        val1.Store4Val(pDataBuf + 1 * nBandValues + j);
        val2.Store4Val(pDataBuf + 2 * nBandValues + j);
        if( NOUTPUT == 4 )
        {
            val3.Store4Val(pDataBuf + 3 * nBandValues + j);
        }
    }
    return j;
}

#else

template<class T, int NINPUT, int NOUTPUT>
size_t GDALPansharpenOperation::WeightedBroveyPositiveWeightsInternal(
    const T* pPanBuffer,
    const T* pUpsampledSpectralBuffer,
    T* pDataBuf,
    size_t nValues,
    size_t nBandValues,
    T nMaxValue) const
{
    // cppcheck-suppress knownConditionTrueFalse
    CPLAssert( NINPUT == 3 || NINPUT == 4 );
    const double dfw0 = psOptions->padfWeights[0];
    const double dfw1 = psOptions->padfWeights[1];
    const double dfw2 = psOptions->padfWeights[2];
    // cppcheck-suppress knownConditionTrueFalse
    const double dfw3 = (NINPUT == 3) ? 0 : psOptions->padfWeights[3];
    size_t j = 0;  // Used after for.
    for( ; j + 1 < nValues; j += 2 )
    {
        double dfFactor = 0.0;
        double dfFactor2 = 0.0;
        double dfPseudoPanchro = 0.0;
        double dfPseudoPanchro2 = 0.0;

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

        dfFactor = ComputeFactor(pPanBuffer[j], dfPseudoPanchro);
        dfFactor2 = ComputeFactor(pPanBuffer[j+1], dfPseudoPanchro2);

        for( int i = 0; i < NOUTPUT; i++ )
        {
            T nRawValue =
                pUpsampledSpectralBuffer[i * nBandValues + j];
            double dfTmp = nRawValue * dfFactor;
            pDataBuf[i * nBandValues + j] = ClampAndRound(dfTmp, nMaxValue);

            T nRawValue2 =
                pUpsampledSpectralBuffer[i * nBandValues + j + 1];
            double dfTmp2 = nRawValue2 * dfFactor2;
            pDataBuf[i * nBandValues + j + 1] = ClampAndRound(dfTmp2, nMaxValue);
        }
    }
    return j;
}
#endif

template <class T>
void GDALPansharpenOperation::WeightedBroveyPositiveWeights(
    const T* pPanBuffer,
    const T* pUpsampledSpectralBuffer,
    T* pDataBuf,
    size_t nValues,
    size_t nBandValues,
    T nMaxValue) const
{
    if( psOptions->bHasNoData )
    {
        WeightedBroveyWithNoData<T, T>
                                (pPanBuffer, pUpsampledSpectralBuffer,
                                 pDataBuf, nValues, nBandValues, nMaxValue);
        return;
    }

    if( nMaxValue == 0 )
        nMaxValue = std::numeric_limits<T>::max();
    size_t j;
    if( psOptions->nInputSpectralBands == 3 &&
        psOptions->nOutPansharpenedBands == 3 &&
        psOptions->panOutPansharpenedBands[0] == 0 &&
        psOptions->panOutPansharpenedBands[1] == 1 &&
        psOptions->panOutPansharpenedBands[2] == 2 )
    {
        j = WeightedBroveyPositiveWeightsInternal<T, 3, 3>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
            nBandValues, nMaxValue);
    }
    else if( psOptions->nInputSpectralBands == 4 &&
        psOptions->nOutPansharpenedBands == 4 &&
        psOptions->panOutPansharpenedBands[0] == 0 &&
        psOptions->panOutPansharpenedBands[1] == 1 &&
        psOptions->panOutPansharpenedBands[2] == 2 &&
        psOptions->panOutPansharpenedBands[3] == 3 )
    {
        j = WeightedBroveyPositiveWeightsInternal<T, 4, 4>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
            nBandValues, nMaxValue);
    }
    else if( psOptions->nInputSpectralBands == 4 &&
        psOptions->nOutPansharpenedBands == 3 &&
        psOptions->panOutPansharpenedBands[0] == 0 &&
        psOptions->panOutPansharpenedBands[1] == 1 &&
        psOptions->panOutPansharpenedBands[2] == 2 )
    {
        j = WeightedBroveyPositiveWeightsInternal<T, 4, 3>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
            nBandValues, nMaxValue);
    }
    else
    {
        for( j = 0; j + 1 < nValues; j += 2 )
        {
            double dfFactor = 0.0;
            double dfFactor2 = 0.0;
            double dfPseudoPanchro = 0.0;
            double dfPseudoPanchro2 = 0.0;
            for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
            {
                dfPseudoPanchro +=
                    psOptions->padfWeights[i] *
                    pUpsampledSpectralBuffer[i * nBandValues + j];
                dfPseudoPanchro2 +=
                    psOptions->padfWeights[i] *
                    pUpsampledSpectralBuffer[i * nBandValues + j + 1];
            }

            dfFactor = ComputeFactor(pPanBuffer[j], dfPseudoPanchro);
            dfFactor2 = ComputeFactor(pPanBuffer[j+1], dfPseudoPanchro2);

            for( int i = 0; i < psOptions->nOutPansharpenedBands; i++ )
            {
                const T nRawValue =
                    pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nBandValues + j];
                const double dfTmp = nRawValue * dfFactor;
                pDataBuf[i * nBandValues + j] = ClampAndRound(dfTmp, nMaxValue);

                const T nRawValue2 =
                    pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nBandValues + j + 1];
                const double dfTmp2 = nRawValue2 * dfFactor2;
                pDataBuf[i * nBandValues + j + 1] = ClampAndRound(dfTmp2, nMaxValue);
            }
        }
    }
    for( ;j<nValues ;j++)
    {
        double dfFactor = 0.0;
        double dfPseudoPanchro = 0.0;
        for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
            dfPseudoPanchro += psOptions->padfWeights[i] *
                            pUpsampledSpectralBuffer[i * nBandValues + j];
        dfFactor = ComputeFactor(pPanBuffer[j], dfPseudoPanchro);

        for( int i = 0; i < psOptions->nOutPansharpenedBands; i++ )
        {
            T nRawValue =
                pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] *
                                         nBandValues + j];
            double dfTmp = nRawValue * dfFactor;
            pDataBuf[i * nBandValues + j] = ClampAndRound(dfTmp, nMaxValue);
        }
    }
}

template<class WorkDataType, class OutDataType> void
GDALPansharpenOperation::WeightedBrovey(
    const WorkDataType* pPanBuffer,
    const WorkDataType* pUpsampledSpectralBuffer,
    OutDataType* pDataBuf,
    size_t nValues,
    size_t nBandValues,
    WorkDataType nMaxValue ) const
{
    if( nMaxValue == 0 )
        WeightedBrovey3<WorkDataType, OutDataType, FALSE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
            nBandValues, 0);
    else
    {
        WeightedBrovey3<WorkDataType, OutDataType, TRUE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
            nBandValues, nMaxValue);
    }
}

template<class T>
void GDALPansharpenOperation::WeightedBroveyGByteOrUInt16(
    const T* pPanBuffer,
    const T* pUpsampledSpectralBuffer,
    T* pDataBuf,
    size_t nValues,
    size_t nBandValues,
    T nMaxValue ) const
{
    if( bPositiveWeights )
    {
        WeightedBroveyPositiveWeights(
                pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
                nBandValues, nMaxValue);
    }
    else if( nMaxValue == 0 )
    {
        WeightedBrovey3<T, T, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
                nBandValues, 0);
    }
    else
    {
        WeightedBrovey3<T, T, TRUE>(
            pPanBuffer, pUpsampledSpectralBuffer, pDataBuf, nValues,
            nBandValues, nMaxValue);
    }
}


template<>
void GDALPansharpenOperation::WeightedBrovey<GByte, GByte>(
    const GByte* pPanBuffer,
    const GByte* pUpsampledSpectralBuffer,
    GByte* pDataBuf,
    size_t nValues,
    size_t nBandValues,
    GByte nMaxValue ) const
{
    WeightedBroveyGByteOrUInt16(pPanBuffer, pUpsampledSpectralBuffer,
                                pDataBuf, nValues,
                                nBandValues, nMaxValue);
}

template<>
void GDALPansharpenOperation::WeightedBrovey<GUInt16, GUInt16>(
    const GUInt16* pPanBuffer,
    const GUInt16* pUpsampledSpectralBuffer,
    GUInt16* pDataBuf,
    size_t nValues,
    size_t nBandValues,
    GUInt16 nMaxValue ) const
{
    WeightedBroveyGByteOrUInt16(pPanBuffer, pUpsampledSpectralBuffer,
                                pDataBuf, nValues,
                                nBandValues, nMaxValue);
}

template<class WorkDataType> CPLErr GDALPansharpenOperation::WeightedBrovey(
    const WorkDataType* pPanBuffer,
    const WorkDataType* pUpsampledSpectralBuffer,
    void *pDataBuf,
    GDALDataType eBufDataType,
    size_t nValues,
    size_t nBandValues,
    WorkDataType nMaxValue ) const
{
    switch( eBufDataType )
    {
        case GDT_Byte:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<GByte *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

        case GDT_UInt16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<GUInt16 *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

#ifndef LIMIT_TYPES
        case GDT_Int16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<GInt16 *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

        case GDT_UInt32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<GUInt32 *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

        case GDT_Int32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<GInt32 *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

        case GDT_UInt64:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<std::uint64_t *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

        case GDT_Int64:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<std::int64_t *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

        case GDT_Float32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<float *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;
#endif

        case GDT_Float64:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           static_cast<double *>(pDataBuf),
                           nValues, nBandValues, nMaxValue);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported,
                     "eBufDataType not supported");
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
    size_t nValues, size_t nBandValues ) const
{
    switch( eBufDataType )
    {
        case GDT_Byte:
            WeightedBrovey3<WorkDataType, GByte, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<GByte *>(pDataBuf), nValues, nBandValues, 0);
            break;

        case GDT_UInt16:
            WeightedBrovey3<WorkDataType, GUInt16, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<GUInt16 *>(pDataBuf), nValues, nBandValues, 0);
            break;

#ifndef LIMIT_TYPES
        case GDT_Int16:
            WeightedBrovey3<WorkDataType, GInt16, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<GInt16 *>(pDataBuf), nValues, nBandValues, 0);
            break;

        case GDT_UInt32:
            WeightedBrovey3<WorkDataType, GUInt32, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<GUInt32 *>(pDataBuf), nValues, nBandValues, 0);
            break;

        case GDT_Int32:
            WeightedBrovey3<WorkDataType, GInt32, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<GInt32 *>(pDataBuf), nValues, nBandValues, 0);
            break;

        case GDT_UInt64:
            WeightedBrovey3<WorkDataType, std::uint64_t, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<std::uint64_t *>(pDataBuf), nValues, nBandValues, 0);
            break;

        case GDT_Int64:
            WeightedBrovey3<WorkDataType, std::int64_t, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<std::int64_t *>(pDataBuf), nValues, nBandValues, 0);
            break;

        case GDT_Float32:
            WeightedBrovey3<WorkDataType, float, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<float *>(pDataBuf), nValues, nBandValues, 0);
            break;
#endif

        case GDT_Float64:
            WeightedBrovey3<WorkDataType, double, FALSE>(
                pPanBuffer, pUpsampledSpectralBuffer,
                static_cast<double *>(pDataBuf), nValues, nBandValues, 0);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported,
                     "eBufDataType not supported");
            return CE_Failure;
            break;
    }

    return CE_None;
}

/************************************************************************/
/*                           ClampValues()                              */
/************************************************************************/

template< class T >
static void ClampValues( T* panBuffer, size_t nValues, T nMaxVal )
{
    for( size_t i = 0; i < nValues; i++ )
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
 *                 GDALGetDataTypeSizeBytes(eBufDataType) *
 *                 psOptions->nOutPansharpenedBands large.
 *                 It begins with all values of the first output band, followed
 *                 by values of the second output band, etc...
 * @param eBufDataType data type of the output buffer
 *
 * @return CE_None in case of success, CE_Failure in case of failure.
 *
 * @since GDAL 2.1
 */
CPLErr GDALPansharpenOperation::ProcessRegion( int nXOff, int nYOff,
                                               int nXSize, int nYSize,
                                               void *pDataBuf,
                                               GDALDataType eBufDataType )
{
    if( psOptions == nullptr )
        return CE_Failure;

    // TODO: Avoid allocating buffers each time.
    GDALRasterBand* poPanchroBand = GDALRasterBand::FromHandle(
                                                    psOptions->hPanchroBand);
    GDALDataType eWorkDataType = poPanchroBand->GetRasterDataType();
#ifdef LIMIT_TYPES
    if( eWorkDataType != GDT_Byte && eWorkDataType != GDT_UInt16 )
        eWorkDataType = GDT_Float64;
#endif
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eWorkDataType);
    GByte* pUpsampledSpectralBuffer = static_cast<GByte *>(
        VSI_MALLOC3_VERBOSE(nXSize, nYSize,
                            psOptions->nInputSpectralBands * nDataTypeSize));
    GByte* pPanBuffer = static_cast<GByte *>(
        VSI_MALLOC3_VERBOSE(nXSize, nYSize, nDataTypeSize));
    if( pUpsampledSpectralBuffer == nullptr || pPanBuffer == nullptr )
    {
        VSIFree(pUpsampledSpectralBuffer);
        VSIFree(pPanBuffer);
        return CE_Failure;
    }

    CPLErr eErr =
        poPanchroBand->RasterIO(GF_Read,
                nXOff, nYOff, nXSize, nYSize, pPanBuffer, nXSize, nYSize,
                eWorkDataType, 0, 0, nullptr);
    if( eErr != CE_None )
    {
        VSIFree(pUpsampledSpectralBuffer);
        VSIFree(pPanBuffer);
        return CE_Failure;
    }

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
    // cppcheck-suppress redundantAssignment
    sExtraArg.eResampleAlg = eResampleAlg;
    sExtraArg.bFloatingPointWindowValidity = TRUE;
    double dfRatioX =
        static_cast<double>(poPanchroBand->GetXSize()) /
        aMSBands[0]->GetXSize();
    double dfRatioY =
        static_cast<double>(poPanchroBand->GetYSize()) /
        aMSBands[0]->GetYSize();
    sExtraArg.dfXOff = (nXOff + psOptions->dfMSShiftX) / dfRatioX;
    sExtraArg.dfYOff = (nYOff + psOptions->dfMSShiftY) / dfRatioY;
    sExtraArg.dfXSize = nXSize / dfRatioX;
    sExtraArg.dfYSize = nYSize / dfRatioY;
    if( sExtraArg.dfXOff + sExtraArg.dfXSize > aMSBands[0]->GetXSize() )
        sExtraArg.dfXOff = aMSBands[0]->GetXSize() - sExtraArg.dfXSize;
    if( sExtraArg.dfYOff + sExtraArg.dfYSize > aMSBands[0]->GetYSize() )
        sExtraArg.dfYOff = aMSBands[0]->GetYSize() - sExtraArg.dfYSize;
    int nSpectralXOff = static_cast<int>(sExtraArg.dfXOff);
    int nSpectralYOff = static_cast<int>(sExtraArg.dfYOff);
    int nSpectralXSize = static_cast<int>(0.49999 + sExtraArg.dfXSize);
    int nSpectralYSize = static_cast<int>(0.49999 + sExtraArg.dfYSize);
    if( nSpectralXSize == 0 )
        nSpectralXSize = 1;
    if( nSpectralYSize == 0 )
        nSpectralYSize = 1;

    // When upsampling, extract the multispectral data at
    // full resolution in a temp buffer, and then do the upsampling.
    if( nSpectralXSize < nXSize && nSpectralYSize < nYSize &&
        eResampleAlg != GRIORA_NearestNeighbour && nYSize > 1 )
    {
        // Take some margin to take into account the radius of the
        // resampling kernel.
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

        GByte* pSpectralBuffer = static_cast<GByte *>(
            VSI_MALLOC3_VERBOSE(
                nXSizeExtract, nYSizeExtract,
                psOptions->nInputSpectralBands * nDataTypeSize));
        if( pSpectralBuffer == nullptr )
        {
            VSIFree(pUpsampledSpectralBuffer);
            VSIFree(pPanBuffer);
            return CE_Failure;
        }

        if( !anInputBands.empty() )
        {
            // Use dataset RasterIO when possible.
            eErr = aMSBands[0]->GetDataset()->RasterIO(
                GF_Read,
                nXOffExtract, nYOffExtract,
                nXSizeExtract, nYSizeExtract,
                pSpectralBuffer,
                nXSizeExtract, nYSizeExtract,
                eWorkDataType,
                static_cast<int>(anInputBands.size()), &anInputBands[0],
                0, 0, 0, nullptr);
        }
        else
        {
            for( int i = 0;
                 eErr == CE_None && i < psOptions->nInputSpectralBands;
                 i++ )
            {
                eErr = aMSBands[i]->RasterIO(
                    GF_Read,
                    nXOffExtract, nYOffExtract,
                    nXSizeExtract, nYSizeExtract,
                    pSpectralBuffer +
                    static_cast<size_t>(i) *
                    nXSizeExtract * nYSizeExtract * nDataTypeSize,
                    nXSizeExtract, nYSizeExtract,
                    eWorkDataType, 0, 0, nullptr);
            }
        }
        if( eErr != CE_None )
        {
            VSIFree(pSpectralBuffer);
            VSIFree(pUpsampledSpectralBuffer);
            VSIFree(pPanBuffer);
            return CE_Failure;
        }

        // Create a MEM dataset that wraps the input buffer.
        GDALDataset* poMEMDS = MEMDataset::Create("", nXSizeExtract, nYSizeExtract, 0,
                                                  eWorkDataType, nullptr);

        char szBuffer0[64] = {};
        char szBuffer1[64] = {};
        char szBuffer2[64] = {};
        snprintf(szBuffer1, sizeof(szBuffer1), "PIXELOFFSET=" CPL_FRMT_GIB,
                 static_cast<GIntBig>(nDataTypeSize));
        snprintf(szBuffer2, sizeof(szBuffer2), "LINEOFFSET=" CPL_FRMT_GIB,
                 static_cast<GIntBig>(nDataTypeSize) * nXSizeExtract);
        char* apszOptions[4] = {};
        apszOptions[0] = szBuffer0;
        apszOptions[1] = szBuffer1;
        apszOptions[2] = szBuffer2;
        apszOptions[3] = nullptr;

        for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
        {
            char szBuffer[32] = {};
            int nRet = CPLPrintPointer(
                szBuffer,
                pSpectralBuffer +
                static_cast<size_t>(i) * nDataTypeSize * nXSizeExtract *
                nYSizeExtract,
                sizeof(szBuffer));
            szBuffer[nRet] = 0;

            snprintf(szBuffer0, sizeof(szBuffer0), "DATAPOINTER=%s", szBuffer);

            poMEMDS->AddBand(eWorkDataType, apszOptions);

            const char* pszNBITS =
                aMSBands[i]->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
            if( pszNBITS )
                poMEMDS->GetRasterBand(i+1)->SetMetadataItem("NBITS", pszNBITS,
                                                             "IMAGE_STRUCTURE");

            if( psOptions->bHasNoData )
                poMEMDS->GetRasterBand(i+1)
                    ->SetNoDataValue(psOptions->dfNoData);
        }

        if( nTasks <= 1 )
        {
            nSpectralXOff -= nXOffExtract;
            nSpectralYOff -= nYOffExtract;
            sExtraArg.dfXOff -= nXOffExtract;
            sExtraArg.dfYOff -= nYOffExtract;
            CPL_IGNORE_RET_VAL(poMEMDS->RasterIO(GF_Read,
                              nSpectralXOff,
                              nSpectralYOff,
                              nSpectralXSize, nSpectralYSize,
                              pUpsampledSpectralBuffer, nXSize, nYSize,
                              eWorkDataType,
                              psOptions->nInputSpectralBands, nullptr,
                              0, 0, 0,
                              &sExtraArg));
        }
        else
        {
            // We are abusing the contract of the GDAL API by using the
            // MEMDataset from several threads. In this case, this is safe. In
            // case, that would no longer be the case we could create as many
            // MEMDataset as threads pointing to the same buffer.

            // To avoid races in threads, we query now the mask flags,
            // so that implicit mask bands are created now.
            for( int i = 0; i < poMEMDS->GetRasterCount(); i++ )
            {
                poMEMDS->GetRasterBand(i+1)->GetMaskFlags();
            }

            std::vector<GDALPansharpenResampleJob> asJobs;
            asJobs.resize( nTasks );
            GDALPansharpenResampleJob* pasJobs = &(asJobs[0]);
            {
                std::vector<void*> ahJobData;
                ahJobData.resize( nTasks );

#ifdef DEBUG_TIMING
                struct timeval tv;
#endif
                for( int i=0;i<nTasks;i++)
                {
                    const size_t iStartLine =
                        (static_cast<size_t>(i) * nYSize) / nTasks;
                    const size_t iNextStartLine =
                        (static_cast<size_t>(i+1) * nYSize) / nTasks;
                    pasJobs[i].poMEMDS = poMEMDS;
                    pasJobs[i].eResampleAlg = eResampleAlg;
                    pasJobs[i].dfXOff = sExtraArg.dfXOff - nXOffExtract;
                    pasJobs[i].dfYOff =
                        (nYOff + psOptions->dfMSShiftY + iStartLine) /
                        dfRatioY - nYOffExtract;
                    pasJobs[i].dfXSize = sExtraArg.dfXSize;
                    pasJobs[i].dfYSize =
                        (iNextStartLine - iStartLine) / dfRatioY;
                    if( pasJobs[i].dfXOff + pasJobs[i].dfXSize >
                        aMSBands[0]->GetXSize() )
                    {
                        pasJobs[i].dfXOff =
                            aMSBands[0]->GetXSize() - pasJobs[i].dfXSize;
                    }
                    if( pasJobs[i].dfYOff + pasJobs[i].dfYSize >
                        aMSBands[0]->GetYSize() )
                    {
                        pasJobs[i].dfYOff =
                            aMSBands[0]->GetYSize() - pasJobs[i].dfYSize;
                    }
                    pasJobs[i].nXOff = static_cast<int>(pasJobs[i].dfXOff);
                    pasJobs[i].nYOff = static_cast<int>(pasJobs[i].dfYOff);
                    pasJobs[i].nXSize =
                        static_cast<int>(0.4999 + pasJobs[i].dfXSize);
                    pasJobs[i].nYSize =
                        static_cast<int>(0.4999 + pasJobs[i].dfYSize);
                    if( pasJobs[i].nXSize == 0 )
                        pasJobs[i].nXSize = 1;
                    if( pasJobs[i].nYSize == 0 )
                        pasJobs[i].nYSize = 1;
                    pasJobs[i].pBuffer =
                        pUpsampledSpectralBuffer +
                        static_cast<size_t>(iStartLine) *
                        nXSize * nDataTypeSize;
                    pasJobs[i].eDT = eWorkDataType;
                    pasJobs[i].nBufXSize = nXSize;
                    pasJobs[i].nBufYSize =
                        static_cast<int>(iNextStartLine - iStartLine);
                    pasJobs[i].nBandCount = psOptions->nInputSpectralBands;
                    pasJobs[i].nBandSpace =
                        static_cast<GSpacing>(nXSize) * nYSize * nDataTypeSize;
#ifdef DEBUG_TIMING
                    pasJobs[i].ptv = &tv;
#endif
                    ahJobData[i] = &(pasJobs[i]);
                }
#ifdef DEBUG_TIMING
                gettimeofday(&tv, nullptr);
#endif
                poThreadPool->SubmitJobs(PansharpenResampleJobThreadFunc,
                                         ahJobData);
                poThreadPool->WaitCompletion();
            }
        }

        GDALClose(poMEMDS);

        VSIFree(pSpectralBuffer);
    }
    else
    {
        if( !anInputBands.empty() )
        {
            // Use dataset RasterIO when possible.
            eErr = aMSBands[0]->GetDataset()->RasterIO(
                GF_Read,
                nSpectralXOff, nSpectralYOff,
                nSpectralXSize, nSpectralYSize,
                pUpsampledSpectralBuffer,
                nXSize, nYSize,
                eWorkDataType,
                static_cast<int>(anInputBands.size()), &anInputBands[0],
                0, 0, 0, &sExtraArg);
        }
        else
        {
            for( int i = 0;
                 eErr == CE_None && i < psOptions->nInputSpectralBands;
                 i++ )
            {
                eErr = aMSBands[i]->RasterIO(
                    GF_Read,
                    nSpectralXOff, nSpectralYOff,
                    nSpectralXSize, nSpectralYSize,
                    pUpsampledSpectralBuffer +
                    static_cast<size_t>(i) * nXSize * nYSize * nDataTypeSize,
                    nXSize, nYSize,
                    eWorkDataType, 0, 0, &sExtraArg);
            }
        }
        if( eErr != CE_None )
        {
            VSIFree(pUpsampledSpectralBuffer);
            VSIFree(pPanBuffer);
            return CE_Failure;
        }
    }

    // In case NBITS was not set on the spectral bands, clamp the values
    // if overshoot might have occurred.
    int nBitDepth = psOptions->nBitDepth;
    if( nBitDepth && (eResampleAlg == GRIORA_Cubic ||
                      eResampleAlg == GRIORA_CubicSpline ||
                      eResampleAlg == GRIORA_Lanczos) )
    {
        for( int i = 0; i < psOptions->nInputSpectralBands; i++ )
        {
            GDALRasterBand* poBand = aMSBands[i];
            int nBandBitDepth = 0;
            const char* pszNBITS =
                poBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
            if( pszNBITS )
                nBandBitDepth = atoi(pszNBITS);
            if( nBandBitDepth < nBitDepth )
            {
                if( eWorkDataType == GDT_Byte )
                {
                    ClampValues(reinterpret_cast<GByte*>(pUpsampledSpectralBuffer) + static_cast<size_t>(i) * nXSize * nYSize,
                               static_cast<size_t>(nXSize)*nYSize,
                               static_cast<GByte>((1 << nBitDepth)-1));
                }
                else if( eWorkDataType == GDT_UInt16 )
                {
                    ClampValues(reinterpret_cast<GUInt16*>(pUpsampledSpectralBuffer) + static_cast<size_t>(i) * nXSize * nYSize,
                               static_cast<size_t>(nXSize)*nYSize,
                               static_cast<GUInt16>((1 << nBitDepth)-1));
                }
#ifndef LIMIT_TYPES
                else if( eWorkDataType == GDT_UInt32 )
                {
                    ClampValues(reinterpret_cast<GUInt32*>(pUpsampledSpectralBuffer) +
                                static_cast<size_t>(i) * nXSize * nYSize,
                                static_cast<size_t>(nXSize)*nYSize,
                                (static_cast<GUInt32>((1 << nBitDepth)-1));
                }
#endif
            }
        }
    }

    GUInt32 nMaxValue = (1 << nBitDepth) - 1;

    double* padfTempBuffer = nullptr;
    GDALDataType eBufDataTypeOri = eBufDataType;
    void* pDataBufOri = pDataBuf;
    // CFloat64 is the query type used by gdallocationinfo...
#ifdef LIMIT_TYPES
    if( eBufDataType != GDT_Byte && eBufDataType != GDT_UInt16 )
#else
    if( eBufDataType == GDT_CFloat64 )
#endif
    {
        padfTempBuffer = static_cast<double * >(
            VSI_MALLOC3_VERBOSE(
                nXSize, nYSize,
                psOptions->nOutPansharpenedBands * sizeof(double)));
        if( padfTempBuffer == nullptr )
        {
            VSIFree(pUpsampledSpectralBuffer);
            VSIFree(pPanBuffer);
            return CE_Failure;
        }
        pDataBuf = padfTempBuffer;
        eBufDataType = GDT_Float64;
    }

    if( nTasks > 1 )
    {
        std::vector<GDALPansharpenJob> asJobs;
        asJobs.resize( nTasks );
        GDALPansharpenJob* pasJobs = &(asJobs[0]);
        {
            std::vector<void*> ahJobData;
            ahJobData.resize( nTasks );
#ifdef DEBUG_TIMING
            struct timeval tv;
#endif
            for( int i=0;i<nTasks;i++)
            {
                const size_t iStartLine =
                    (static_cast<size_t>(i) * nYSize) / nTasks;
                const size_t iNextStartLine =
                    (static_cast<size_t>(i + 1) * nYSize) / nTasks;
                pasJobs[i].poPansharpenOperation = this;
                pasJobs[i].eWorkDataType = eWorkDataType;
                pasJobs[i].eBufDataType = eBufDataType;
                pasJobs[i].pPanBuffer =
                    pPanBuffer + iStartLine *  nXSize * nDataTypeSize;
                pasJobs[i].pUpsampledSpectralBuffer =
                    pUpsampledSpectralBuffer +
                    iStartLine * nXSize * nDataTypeSize;
                pasJobs[i].pDataBuf =
                    static_cast<GByte*>(pDataBuf) +
                    iStartLine * nXSize *
                    GDALGetDataTypeSizeBytes(eBufDataType);
                pasJobs[i].nValues =
                    (iNextStartLine - iStartLine) * nXSize;
                pasJobs[i].nBandValues = static_cast<size_t>(nXSize) * nYSize;
                pasJobs[i].nMaxValue = nMaxValue;
#ifdef DEBUG_TIMING
                pasJobs[i].ptv = &tv;
#endif
                ahJobData[i] = &(pasJobs[i]);
            }
#ifdef DEBUG_TIMING
            gettimeofday(&tv, nullptr);
#endif
            poThreadPool->SubmitJobs(PansharpenJobThreadFunc, ahJobData);
            poThreadPool->WaitCompletion();
        }

        eErr = CE_None;
        for( int i=0;i<nTasks;i++)
        {
            if( pasJobs[i].eErr != CE_None )
                eErr = CE_Failure;
        }
    }
    else
    {
        eErr = PansharpenChunk( eWorkDataType, eBufDataType,
                                pPanBuffer,
                                pUpsampledSpectralBuffer,
                                pDataBuf,
                                static_cast<size_t>(nXSize) * nYSize,
                                static_cast<size_t>(nXSize) * nYSize,
                                nMaxValue);
    }

    if( padfTempBuffer )
    {
        GDALCopyWords64(padfTempBuffer, GDT_Float64, sizeof(double),
                      pDataBufOri, eBufDataTypeOri,
                      GDALGetDataTypeSizeBytes(eBufDataTypeOri),
                      static_cast<size_t>(nXSize)*nYSize*psOptions->nOutPansharpenedBands);
        VSIFree(padfTempBuffer);
    }

    VSIFree(pUpsampledSpectralBuffer);
    VSIFree(pPanBuffer);

    return eErr;
}

/************************************************************************/
/*                   PansharpenResampleJobThreadFunc()                  */
/************************************************************************/

// static int acc=0;

void GDALPansharpenOperation::PansharpenResampleJobThreadFunc(void* pUserData)
{
    GDALPansharpenResampleJob* psJob = static_cast<GDALPansharpenResampleJob*>(pUserData);

#ifdef DEBUG_TIMING
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    const GIntBig launch_time =
        static_cast<GIntBig>(psJob->ptv->tv_sec) * 1000000 +
        static_cast<GIntBig>(psJob->ptv->tv_usec);
    const GIntBig start_job =
        static_cast<GIntBig>(tv.tv_sec) * 1000000 +
        static_cast<GIntBig>(tv.tv_usec);
#endif

#if 0
    for(int i=0;i<1000000;i++)
        acc += i * i;
#else
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    // cppcheck-suppress redundantAssignment
    sExtraArg.eResampleAlg = psJob->eResampleAlg;
    sExtraArg.bFloatingPointWindowValidity = TRUE;
    sExtraArg.dfXOff = psJob->dfXOff;
    sExtraArg.dfYOff = psJob->dfYOff;
    sExtraArg.dfXSize = psJob->dfXSize;
    sExtraArg.dfYSize = psJob->dfYSize;

    CPL_IGNORE_RET_VAL(psJob->poMEMDS->RasterIO(GF_Read,
                             psJob->nXOff,
                             psJob->nYOff,
                             psJob->nXSize,
                             psJob->nYSize,
                             psJob->pBuffer,
                             psJob->nBufXSize,
                             psJob->nBufYSize,
                             psJob->eDT,
                             psJob->nBandCount,
                             nullptr,
                             0, 0, psJob->nBandSpace,
                             &sExtraArg));
#endif

#ifdef DEBUG_TIMING
    struct timeval tv_end;
    gettimeofday(&tv_end, nullptr);
    const GIntBig end =
        static_cast<GIntBig>(tv_end.tv_sec) * 1000000 +
        static_cast<GIntBig>(tv_end.tv_usec);
    if( start_job - launch_time > 500 )
        /*ok*/printf("Resample: Delay before start=" CPL_FRMT_GIB
               ", completion time=" CPL_FRMT_GIB "\n",
               start_job - launch_time, end - start_job);
#endif
}

/************************************************************************/
/*                      PansharpenJobThreadFunc()                       */
/************************************************************************/

void GDALPansharpenOperation::PansharpenJobThreadFunc(void* pUserData)
{
    GDALPansharpenJob* psJob = static_cast<GDALPansharpenJob*>(pUserData);

#ifdef DEBUG_TIMING
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    const GIntBig launch_time =
        static_cast<GIntBig>(psJob->ptv->tv_sec) * 1000000 +
        static_cast<GIntBig>(psJob->ptv->tv_usec);
    const GIntBig start_job =
        static_cast<GIntBig>(tv.tv_sec) * 1000000 +
        static_cast<GIntBig>(tv.tv_usec);
#endif

#if 0
    for( int i = 0; i < 1000000; i++ )
        acc += i * i;
    psJob->eErr = CE_None;
#else
    psJob->eErr = psJob->poPansharpenOperation->PansharpenChunk(
        psJob->eWorkDataType,
        psJob->eBufDataType,
        psJob->pPanBuffer,
        psJob->pUpsampledSpectralBuffer,
        psJob->pDataBuf,
        psJob->nValues,
        psJob->nBandValues,
        psJob->nMaxValue);
#endif

#ifdef DEBUG_TIMING
    struct timeval tv_end;
    gettimeofday(&tv_end, nullptr);
    const GIntBig end =
        static_cast<GIntBig>(tv_end.tv_sec) * 1000000 +
        static_cast<GIntBig>(tv_end.tv_usec);
    if( start_job - launch_time > 500 )
        /*ok*/printf("Pansharpen: Delay before start=" CPL_FRMT_GIB
               ", completion time=" CPL_FRMT_GIB "\n",
               start_job - launch_time, end - start_job);
#endif
}

/************************************************************************/
/*                           PansharpenChunk()                          */
/************************************************************************/

CPLErr
GDALPansharpenOperation::PansharpenChunk( GDALDataType eWorkDataType,
                                          GDALDataType eBufDataType,
                                          const void* pPanBuffer,
                                          const void* pUpsampledSpectralBuffer,
                                          void* pDataBuf,
                                          size_t nValues,
                                          size_t nBandValues,
                                          GUInt32 nMaxValue) const
{
    CPLErr eErr = CE_None;

    switch( eWorkDataType )
    {
        case GDT_Byte:
            eErr = WeightedBrovey(static_cast<const GByte*>(pPanBuffer),
                                  static_cast<const GByte*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues, static_cast<GByte>(nMaxValue));
            break;

        case GDT_UInt16:
            eErr = WeightedBrovey(static_cast<const GUInt16*>(pPanBuffer),
                                  static_cast<const GUInt16*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues, static_cast<GUInt16>(nMaxValue));
            break;

#ifndef LIMIT_TYPES
        case GDT_Int16:
            eErr = WeightedBrovey(static_cast<const GInt16*>(pPanBuffer),
                                  static_cast<const GInt16*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues);
            break;

        case GDT_UInt32:
            eErr = WeightedBrovey(static_cast<const GUInt32*>(pPanBuffer),
                                  static_cast<const GUInt32*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues, nMaxValue);
            break;

        case GDT_Int32:
            eErr = WeightedBrovey(static_cast<const GInt32*>(pPanBuffer),
                                  static_cast<const GInt32*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues);
            break;

        case GDT_UInt64:
            eErr = WeightedBrovey(static_cast<const std::uint64_t*>(pPanBuffer),
                                  static_cast<const std::uint64_t*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues, nMaxValue);
            break;

        case GDT_Int64:
            eErr = WeightedBrovey(static_cast<const std::int64_t*>(pPanBuffer),
                                  static_cast<const std::int64_t*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues);
            break;

        case GDT_Float32:
            eErr = WeightedBrovey(static_cast<const float*>(pPanBuffer),
                                  static_cast<const float*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues);
            break;
#endif
        case GDT_Float64:
            eErr = WeightedBrovey(static_cast<const double*>(pPanBuffer),
                                  static_cast<const double*>(pUpsampledSpectralBuffer),
                                  pDataBuf, eBufDataType,
                                  nValues, nBandValues);
            break;

        default:
            CPLError( CE_Failure, CPLE_NotSupported,
                      "eWorkDataType not supported");
            eErr = CE_Failure;
            break;
    }

    return eErr;
}

/************************************************************************/
/*                             GetOptions()                             */
/************************************************************************/

/** Return options.
 * @return options.
 */
GDALPansharpenOptions* GDALPansharpenOperation::GetOptions()
{
    return psOptions;
}

/************************************************************************/
/*                     GDALCreatePansharpenOperation()                  */
/************************************************************************/

/** Instantiate a pansharpening operation.
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
        return reinterpret_cast<GDALPansharpenOperationH>(psOperation);
    delete psOperation;
    return nullptr;
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
    delete reinterpret_cast<GDALPansharpenOperation*>(hOperation);
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
 *                 GDALGetDataTypeSizeBytes(eBufDataType) *
 *                 psOptions->nOutPansharpenedBands large.
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
    return reinterpret_cast<GDALPansharpenOperation*>(hOperation)->
                                          ProcessRegion(nXOff, nYOff,
                                                        nXSize, nYSize,
                                                        pDataBuf, eBufDataType);
}
