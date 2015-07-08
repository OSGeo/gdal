/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Pansharpening module
 * Purpose:  Prototypes, and definitions for pansharpening related work.
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

#ifndef GDALPANSHARPEN_H_INCLUDED
#define GDALPANSHARPEN_H_INCLUDED

#include "gdal.h"

CPL_C_START

/**
 * \file gdalpansharpen.h
 *
 * GDAL pansharpening related entry points and definitions.
 *
 * @since GDAL 2.1
 */

/** Pansharpening algorithms.
 */
typedef enum
{
    /*! Weighted Brovery. */
    GDAL_PSH_WEIGHTED_BROVEY
} GDALPansharpenAlg;

/** Pansharpening options.
  */
typedef struct
{
    /*! Pan sharpening algorithm/method. Only weighed Brovey for now. */
    GDALPansharpenAlg    ePansharpenAlg;
    
    /*! Resampling algorithm to upsample spectral bands to pan band resoultion. */
    GDALRIOResampleAlg   eResampleAlg;

    /*! Bit depth of the spectral bands. Can be let to 0 for default behaviour. */
    int                  nBitDepth;

    /*! Number of weight coefficients in padfWeights. */
    int                  nWeightCount;
    
    /*! Array of nWeightCount weights used by weighted Brovey. */
    double              *padfWeights;

    /*! Panchromatic band. */
    GDALRasterBandH      hPanchroBand;
    
    /*! Number of input spectral bands. */
    int                  nInputSpectralBands;

    /*! Array of nInputSpectralBands input spectral bands. */
    GDALRasterBandH     *pahInputSpectralBands;

    /*! Number of output pansharpened spectral bands. */
    int                  nOutPansharpenedBands;

    /*! Array of nOutPansharpendBands values such as panOutPansharpenedBands[k] is a value in the range [0,nInputSpectralBands-1] . */
    int                 *panOutPansharpenedBands;
} GDALPansharpenOptions;


GDALPansharpenOptions CPL_DLL * GDALCreatePansharpenOptions(void);
void CPL_DLL GDALDestroyPansharpenOptions( GDALPansharpenOptions * );
GDALPansharpenOptions CPL_DLL * GDALClonePansharpenOptions(
                                        const GDALPansharpenOptions* psOptions);

/*! Pansharpening operation handle. */
typedef void* GDALPansharpenOperationH;

GDALPansharpenOperationH CPL_DLL GDALCreatePansharpenOperation(const GDALPansharpenOptions* );
void CPL_DLL GDALDestroyPansharpenOperation( GDALPansharpenOperationH );
CPLErr CPL_DLL GDALPansharpenProcessRegion( GDALPansharpenOperationH hOperation,
                                            int nXOff, int nYOff,
                                            int nXSize, int nYSize,
                                            void *pDataBuf, 
                                            GDALDataType eBufDataType);

CPL_C_END

#ifdef __cplusplus 

#include <vector>

/** Pansharpening operation class.
 */
class GDALPansharpenOperation
{
        GDALPansharpenOptions* psOptions;
        std::vector<int> anInputBands;
        int bWeightsWillNotOvershoot;

        template<class WorkDataType, class OutDataType, int bHasBitDepth> void WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     int nValues,
                                                     WorkDataType nMaxValue);
        template<class WorkDataType, class OutDataType> void WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     int nValues,
                                                     int nBitDepth);
        template<class WorkDataType> CPLErr WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     void *pDataBuf, 
                                                     GDALDataType eBufDataType,
                                                     int nValues,
                                                     int nBitDepth);
    public:
                             GDALPansharpenOperation();
                            ~GDALPansharpenOperation();

        CPLErr               Initialize(const GDALPansharpenOptions* psOptions);
        CPLErr               ProcessRegion(int nXOff, int nYOff,
                                           int nXSize, int nYSize,
                                           void *pDataBuf, 
                                           GDALDataType eBufDataType);
        GDALPansharpenOptions* GetOptions();
};

#endif /* __cplusplus */

#endif /* GDALPANSHARPEN_H_INCLUDED */

