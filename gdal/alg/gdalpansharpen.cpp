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
}

/************************************************************************/
/*                       ~GDALPansharpenOperation()                     */
/************************************************************************/

/** Pansharpening operation destructor.
 */

GDALPansharpenOperation::~GDALPansharpenOperation()
{
    GDALDestroyPansharpenOptions(psOptions);
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

    psOptions = GDALClonePansharpenOptions(psOptionsIn);
    return CE_None;
}

/************************************************************************/
/*                         WeightedBrovey()                             */
/************************************************************************/

template<class WorkDataType, class OutDataType> void GDALPansharpenOperation::WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     int nValues)
{
    for(int j=0;j<nValues;j++)
    {
        double dfPseudoPanchro = 0;
        for(int i=0;i<psOptions->nInputSpectralBands;i++)
            dfPseudoPanchro += psOptions->padfWeights[i] *
                               pUpsampledSpectralBuffer[i * nValues + j];
        double dfFactor;
        if( dfPseudoPanchro )
            dfFactor = pPanBuffer[j] / dfPseudoPanchro;
        else
            dfFactor = 0.0;
        for(int i=0;i<psOptions->nOutPansharpenedBands;i++)
        {
            WorkDataType nRawValue =
                pUpsampledSpectralBuffer[psOptions->panOutPansharpenedBands[i] * nValues + j];
            WorkDataType nPansharpendValue;
            GDALCopyWord(nRawValue * dfFactor, nPansharpendValue);
            GDALCopyWord(nPansharpendValue, pDataBuf[i * nValues + j]);
        }
    }
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
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GByte*)pDataBuf, nValues);
            break;

        case GDT_UInt16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt16*)pDataBuf, nValues);
            break;

        case GDT_Int16:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt16*)pDataBuf, nValues);
            break;

        case GDT_UInt32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GUInt32*)pDataBuf, nValues);
            break;

        case GDT_Int32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (GInt32*)pDataBuf, nValues);
            break;

        case GDT_Float32:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (float*)pDataBuf, nValues);
            break;

        case GDT_Float64:
            WeightedBrovey(pPanBuffer, pUpsampledSpectralBuffer,
                           (double*)pDataBuf, nValues);
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported, "eBufDataType not supported");
            return CE_Failure;
            break;
    }

    return CE_None;
}

/************************************************************************/
/*                         ProcessRegion()                              */
/************************************************************************/

/** Executes a pansharpening operation on a rectangular region of the
 * resulting dataset.
 *
 * @param nXOff pixel offset.
 * @param nYOff pixel offset.
 * @param nXSize width of the pansharpened region to compute.
 * @param nYSize height of the pansharpened region to compute.
 * @param pDataBuf output buffer. Must be nXSize * nYSize * 
 *                 GDALGetDataTypeSize(eBufDataType) / 8 * psOptions->nOutPansharpenedBands large
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
    int nDataTypeSize = GDALGetDataTypeSize(eWorkDataType) / 8;
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
    // TODO: use dataset rasterio when possible
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.eResampleAlg = psOptions->eResampleAlg;
    sExtraArg.bFloatingPointWindowValidity = TRUE;
    GDALRasterBand* poFirstSpectralBand =
                        (GDALRasterBand*)psOptions->pahInputSpectralBands[0];
    double dfRatioX = (double)poPanchroBand->GetXSize() / poFirstSpectralBand->GetXSize();
    double dfRatioY = (double)poPanchroBand->GetYSize() / poFirstSpectralBand->GetYSize();
    sExtraArg.dfXOff = nXOff / dfRatioX;
    sExtraArg.dfYOff = nYOff / dfRatioY;
    sExtraArg.dfXSize = nXSize / dfRatioX;
    sExtraArg.dfYSize = nYSize / dfRatioY;
    int nSpectralXOff = (int)(0.49999 + sExtraArg.dfXOff);
    int nSpectralYOff = (int)(0.49999 + sExtraArg.dfYOff);
    int nSpectralXSize = (int)(0.49999 + sExtraArg.dfXSize);
    int nSpectralYSize = (int)(0.49999 + sExtraArg.dfYSize);
    if( nSpectralXSize == 0 )
        nSpectralXSize = 1;
    if( nSpectralYSize == 0 )
        nSpectralYSize = 1;
    
    if( anInputBands.size() )
    {
        eErr = ((GDALRasterBand*)psOptions->pahInputSpectralBands[0])->GetDataset()->RasterIO(GF_Read,
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
            eErr = 
            ((GDALRasterBand*)psOptions->pahInputSpectralBands[i])->RasterIO(GF_Read,
                    nSpectralXOff, nSpectralYOff, nSpectralXSize, nSpectralYSize,
                    pUpsampledSpectralBuffer + i * nXSize * nYSize * nDataTypeSize,
                    nXSize, nYSize,
                    eWorkDataType, 0, 0, &sExtraArg);
        }
    }
    
    switch( eWorkDataType )
    {
        case GDT_Byte:
            eErr = WeightedBrovey ((GByte*)pPanBuffer,
                                   (GByte*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize);
            break;

        case GDT_UInt16:
            eErr = WeightedBrovey ((GUInt16*)pPanBuffer,
                                   (GUInt16*)pUpsampledSpectralBuffer,
                                   pDataBuf, eBufDataType,
                                   nXSize * nYSize);
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
                                   nXSize * nYSize);
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
 * @param hOperation a valid pansharpening operation.
 * @param nXOff pixel offset.
 * @param nYOff pixel offset.
 * @param nXSize width of the pansharpened region to compute.
 * @param nYSize height of the pansharpened region to compute.
 * @param pDataBuf output buffer. Must be nXSize * nYSize * 
 *                 GDALGetDataTypeSize(eBufDataType) / 8 * psOptions->nOutPansharpenedBands large
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
