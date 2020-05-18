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

    /*! Resampling algorithm to upsample spectral bands to pan band resolution. */
    GDALRIOResampleAlg   eResampleAlg;

    /*! Bit depth of the spectral bands. Can be let to 0 for default behavior. */
    int                  nBitDepth;

    /*! Number of weight coefficients in padfWeights. */
    int                  nWeightCount;

    /*! Array of nWeightCount weights used by weighted Brovey. */
    double              *padfWeights;

    /*! Panchromatic band. */
    GDALRasterBandH      hPanchroBand;

    /*! Number of input spectral bands. */
    int                  nInputSpectralBands;

    /** Array of nInputSpectralBands input spectral bands. The spectral band have
     *  generally a coarser resolution than the panchromatic band, but they
     *  are assumed to have the same spatial extent (and projection) at that point.
     *  Necessary spatial adjustments must be done beforehand, for example by wrapping
     *  inside a VRT dataset.
     */
    GDALRasterBandH     *pahInputSpectralBands;

    /*! Number of output pansharpened spectral bands. */
    int                  nOutPansharpenedBands;

    /*! Array of nOutPansharpendBands values such as panOutPansharpenedBands[k] is a value in the range [0,nInputSpectralBands-1] . */
    int                 *panOutPansharpenedBands;

    /*! Whether the panchromatic and spectral bands have a noData value. */
    int                  bHasNoData;

    /** NoData value of the panchromatic and spectral bands (only taken into account if bHasNoData = TRUE).
        This will also be use has the output nodata value. */
    double               dfNoData;

    /** Number of threads or -1 to mean ALL_CPUS. By default (0), single threaded mode is enabled
      * unless the GDAL_NUM_THREADS configuration option is set to an integer or ALL_CPUS. */
    int                  nThreads;

    /** Shift in pixels of multispectral bands w.r.t panchromatic band, in X direction */
    double               dfMSShiftX;

    /** Shift in pixels of multispectral bands w.r.t panchromatic band, in Y direction */
    double               dfMSShiftY;

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
#include "gdal_priv.h"
#include "cpl_worker_thread_pool.h"

#ifdef DEBUG_TIMING
#include <sys/time.h>
#endif

class GDALPansharpenOperation;

//! @cond Doxygen_Suppress
typedef struct
{
    GDALPansharpenOperation* poPansharpenOperation;
    GDALDataType eWorkDataType;
    GDALDataType eBufDataType;
    const void* pPanBuffer;
    const void* pUpsampledSpectralBuffer;
    void* pDataBuf;
    size_t nValues;
    size_t nBandValues;
    GUInt32 nMaxValue;

#ifdef DEBUG_TIMING
    struct timeval* ptv;
#endif

    CPLErr eErr;
} GDALPansharpenJob;

typedef struct
{
    GDALDataset* poMEMDS;
    int          nXOff;
    int          nYOff;
    int          nXSize;
    int          nYSize;
    double       dfXOff;
    double       dfYOff;
    double       dfXSize;
    double       dfYSize;
    void        *pBuffer;
    GDALDataType eDT;
    int          nBufXSize;
    int          nBufYSize;
    int          nBandCount;
    GDALRIOResampleAlg eResampleAlg;
    GSpacing     nBandSpace;

#ifdef DEBUG_TIMING
    struct timeval* ptv;
#endif
} GDALPansharpenResampleJob;
//! @endcond

/** Pansharpening operation class.
 */
class GDALPansharpenOperation
{
        CPL_DISALLOW_COPY_ASSIGN(GDALPansharpenOperation)

        GDALPansharpenOptions* psOptions = nullptr;
        std::vector<int> anInputBands{};
        std::vector<GDALDataset*> aVDS{}; // to destroy
        std::vector<GDALRasterBand*> aMSBands{}; // original multispectral bands potentially warped into a VRT
        int bPositiveWeights = TRUE;
        CPLWorkerThreadPool* poThreadPool = nullptr;
        int nKernelRadius = 0;

        static void PansharpenJobThreadFunc(void* pUserData);
        static void PansharpenResampleJobThreadFunc(void* pUserData);

        template<class WorkDataType, class OutDataType> void WeightedBroveyWithNoData(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     WorkDataType nMaxValue) const;
        template<class WorkDataType, class OutDataType, int bHasBitDepth> void WeightedBrovey3(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     WorkDataType nMaxValue) const;

        // cppcheck-suppress functionStatic
        template<class WorkDataType, class OutDataType> void WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     OutDataType* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     WorkDataType nMaxValue) const;
        template<class WorkDataType> CPLErr WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     void *pDataBuf,
                                                     GDALDataType eBufDataType,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     WorkDataType nMaxValue) const;

        // cppcheck-suppress functionStatic
        template<class WorkDataType> CPLErr WeightedBrovey(
                                                     const WorkDataType* pPanBuffer,
                                                     const WorkDataType* pUpsampledSpectralBuffer,
                                                     void *pDataBuf,
                                                     GDALDataType eBufDataType,
                                                     size_t nValues,
                                                     size_t nBandValues) const;
        template<class T> void WeightedBroveyPositiveWeights(
                                                     const T* pPanBuffer,
                                                     const T* pUpsampledSpectralBuffer,
                                                     T* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     T nMaxValue) const;

        template<class T, int NINPUT, int NOUTPUT> size_t WeightedBroveyPositiveWeightsInternal(
                                                     const T* pPanBuffer,
                                                     const T* pUpsampledSpectralBuffer,
                                                     T* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     T nMaxValue) const;

        // cppcheck-suppress unusedPrivateFunction
        template<class T> void WeightedBroveyGByteOrUInt16(
                                                     const T* pPanBuffer,
                                                     const T* pUpsampledSpectralBuffer,
                                                     T* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     T nMaxValue ) const;

        // cppcheck-suppress functionStatic
        CPLErr PansharpenChunk( GDALDataType eWorkDataType, GDALDataType eBufDataType,
                                                     const void* pPanBuffer,
                                                     const void* pUpsampledSpectralBuffer,
                                                     void* pDataBuf,
                                                     size_t nValues,
                                                     size_t nBandValues,
                                                     GUInt32 nMaxValue) const;
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
