/******************************************************************************
 * $Id$
 *
 * Project:  GDAL High Performance Warper
 * Purpose:  Prototypes, and definitions for warping related work.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GDALWARPER_H_INCLUDED
#define GDALWARPER_H_INCLUDED

/**
 * \file gdalwarper.h
 *
 * GDAL warper related entry points and definitions.  Eventually it is
 * expected that this file will be mostly private to the implementation,
 * and the public C entry points will be available in gdal_alg.h.
 */

#include "gdal_alg.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"

CPL_C_START

/* Note: values are selected to be consistant with GDALRIOResampleAlg of gcore/gdal.h */ 
/*! Warp Resampling Algorithm */
typedef enum {
  /*! Nearest neighbour (select on one input pixel) */ GRA_NearestNeighbour=0,
  /*! Bilinear (2x2 kernel) */                         GRA_Bilinear=1,
  /*! Cubic Convolution Approximation (4x4 kernel) */  GRA_Cubic=2,
  /*! Cubic B-Spline Approximation (4x4 kernel) */     GRA_CubicSpline=3,
  /*! Lanczos windowed sinc interpolation (6x6 kernel) */ GRA_Lanczos=4,
  /*! Average (computes the average of all non-NODATA contributing pixels) */ GRA_Average=5, 
  /*! Mode (selects the value which appears most often of all the sampled points) */ GRA_Mode=6,
  // GRA_Gauss=7 reserved.
  /*! Max (selects maximum of all non-NODATA contributing pixels) */ GRA_Max=8,
  /*! Min (selects minimum of all non-NODATA contributing pixels) */ GRA_Min=9,
  /*! Med (selects median of all non-NODATA contributing pixels) */ GRA_Med=10,
  /*! Q1 (selects first quartile of all non-NODATA contributing pixels) */ GRA_Q1=11,
  /*! Q3 (selects third quartile of all non-NODATA contributing pixels) */ GRA_Q3=12
} GDALResampleAlg;

/*! GWKAverageOrMode Algorithm */
typedef enum {
    /*! Average */ GWKAOM_Average=1,
    /*! Mode */ GWKAOM_Fmode=2,
    /*! Mode of GDT_Byte, GDT_UInt16, or GDT_Int16 */ GWKAOM_Imode=3,
    /*! Maximum */ GWKAOM_Max=4,
    /*! Minimum */ GWKAOM_Min=5,
    /*! Quantile */ GWKAOM_Quant=6
} GWKAverageOrModeAlg;

typedef int 
(*GDALMaskFunc)( void *pMaskFuncArg,
                 int nBandCount, GDALDataType eType, 
                 int nXOff, int nYOff, 
                 int nXSize, int nYSize,
                 GByte **papabyImageData, 
                 int bMaskIsFloat, void *pMask );

CPLErr CPL_DLL 
GDALWarpNoDataMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
                      int nXOff, int nYOff, int nXSize, int nYSize,
                      GByte **papabyImageData, int bMaskIsFloat,
                      void *pValidityMask, int* pbOutAllValid );

CPLErr CPL_DLL 
GDALWarpDstAlphaMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
                        int nXOff, int nYOff, int nXSize, int nYSize,
                        GByte ** /*ppImageData */,
                        int bMaskIsFloat, void *pValidityMask );
CPLErr CPL_DLL 
GDALWarpSrcAlphaMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
                        int nXOff, int nYOff, int nXSize, int nYSize,
                        GByte ** /*ppImageData */,
                        int bMaskIsFloat, void *pValidityMask, int* pbOutAllOpaque );

CPLErr CPL_DLL 
GDALWarpSrcMaskMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       GByte ** /*ppImageData */,
                       int bMaskIsFloat, void *pValidityMask );

CPLErr CPL_DLL 
GDALWarpCutlineMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       GByte ** /* ppImageData */,
                       int bMaskIsFloat, void *pValidityMask );

/************************************************************************/
/*                           GDALWarpOptions                            */
/************************************************************************/

/** Warp control options for use with GDALWarpOperation::Initialize()  */
typedef struct {
    
    char              **papszWarpOptions;  

    /*! In bytes, 0.0 for internal default */
    double              dfWarpMemoryLimit; 

    /*! Resampling algorithm to use */
    GDALResampleAlg     eResampleAlg;

    /*! data type to use during warp operation, GDT_Unknown lets the algorithm
        select the type */
    GDALDataType        eWorkingDataType;

    /*! Source image dataset. */
    GDALDatasetH	hSrcDS;

    /*! Destination image dataset - may be NULL if only using GDALWarpOperation::WarpRegionToBuffer(). */
    GDALDatasetH        hDstDS;

    /*! Number of bands to process, may be 0 to select all bands. */
    int                 nBandCount;
    
    /*! The band numbers for the source bands to process (1 based) */
    int                *panSrcBands;

    /*! The band numbers for the destination bands to process (1 based) */
    int                *panDstBands;

    /*! The source band so use as an alpha (transparency) value, 0=disabled */
    int                nSrcAlphaBand;

    /*! The dest. band so use as an alpha (transparency) value, 0=disabled */
    int                nDstAlphaBand;

    /*! The "nodata" value real component for each input band, if NULL there isn't one */
    double             *padfSrcNoDataReal;
    /*! The "nodata" value imaginary component - may be NULL even if real 
      component is provided. */
    double             *padfSrcNoDataImag;

    /*! The "nodata" value real component for each output band, if NULL there isn't one */
    double             *padfDstNoDataReal;
    /*! The "nodata" value imaginary component - may be NULL even if real 
      component is provided. */
    double             *padfDstNoDataImag;

    /*! GDALProgressFunc() compatible progress reporting function, or NULL
      if there isn't one. */
    GDALProgressFunc    pfnProgress;

    /*! Callback argument to be passed to pfnProgress. */
    void               *pProgressArg;

    /*! Type of spatial point transformer function */
    GDALTransformerFunc pfnTransformer;

    /*! Handle to image transformer setup structure */
    void                *pTransformerArg;

    GDALMaskFunc       *papfnSrcPerBandValidityMaskFunc;
    void              **papSrcPerBandValidityMaskFuncArg;
    
    GDALMaskFunc        pfnSrcValidityMaskFunc;
    void               *pSrcValidityMaskFuncArg;
    
    GDALMaskFunc        pfnSrcDensityMaskFunc;
    void               *pSrcDensityMaskFuncArg;

    GDALMaskFunc        pfnDstDensityMaskFunc;
    void               *pDstDensityMaskFuncArg;

    GDALMaskFunc        pfnDstValidityMaskFunc;
    void               *pDstValidityMaskFuncArg;

    CPLErr              (*pfnPreWarpChunkProcessor)( void *pKern, void *pArg );
    void               *pPreWarpProcessorArg;
    
    CPLErr              (*pfnPostWarpChunkProcessor)( void *pKern, void *pArg);
    void               *pPostWarpProcessorArg;

    /*! Optional OGRPolygonH for a masking cutline. */
    void               *hCutline;

    /*! Optional blending distance to apply across cutline in pixels, default is zero. */
    double              dfCutlineBlendDist;

} GDALWarpOptions;

GDALWarpOptions CPL_DLL * CPL_STDCALL GDALCreateWarpOptions(void);
void CPL_DLL CPL_STDCALL GDALDestroyWarpOptions( GDALWarpOptions * );
GDALWarpOptions CPL_DLL * CPL_STDCALL
GDALCloneWarpOptions( const GDALWarpOptions * );

CPLXMLNode CPL_DLL * CPL_STDCALL
      GDALSerializeWarpOptions( const GDALWarpOptions * );
GDALWarpOptions CPL_DLL * CPL_STDCALL
      GDALDeserializeWarpOptions( CPLXMLNode * );

/************************************************************************/
/*                         GDALReprojectImage()                         */
/************************************************************************/

CPLErr CPL_DLL CPL_STDCALL
GDALReprojectImage( GDALDatasetH hSrcDS, const char *pszSrcWKT, 
                    GDALDatasetH hDstDS, const char *pszDstWKT,
                    GDALResampleAlg eResampleAlg, double dfWarpMemoryLimit,
                    double dfMaxError,
                    GDALProgressFunc pfnProgress, void *pProgressArg, 
                    GDALWarpOptions *psOptions );

CPLErr CPL_DLL CPL_STDCALL
GDALCreateAndReprojectImage( GDALDatasetH hSrcDS, const char *pszSrcWKT, 
                    const char *pszDstFilename, const char *pszDstWKT,
                    GDALDriverH hDstDriver, char **papszCreateOptions,
                    GDALResampleAlg eResampleAlg, double dfWarpMemoryLimit,
                    double dfMaxError,
                    GDALProgressFunc pfnProgress, void *pProgressArg, 
                    GDALWarpOptions *psOptions );

/************************************************************************/
/*                           VRTWarpedDataset                           */
/************************************************************************/

GDALDatasetH CPL_DLL CPL_STDCALL
GDALAutoCreateWarpedVRT( GDALDatasetH hSrcDS, 
                         const char *pszSrcWKT, const char *pszDstWKT, 
                         GDALResampleAlg eResampleAlg, 
                         double dfMaxError, const GDALWarpOptions *psOptions );

GDALDatasetH CPL_DLL CPL_STDCALL 
GDALCreateWarpedVRT( GDALDatasetH hSrcDS, 
                     int nPixels, int nLines, double *padfGeoTransform,
                     GDALWarpOptions *psOptions );

CPLErr CPL_DLL CPL_STDCALL
GDALInitializeWarpedVRT( GDALDatasetH hDS, 
                         GDALWarpOptions *psWO );

CPL_C_END

#ifdef __cplusplus 

/************************************************************************/
/*                            GDALWarpKernel                            */
/*                                                                      */
/*      This class represents the lowest level of abstraction.  It      */
/*      is holds the imagery for one "chunk" of a warp, and the         */
/*      pre-prepared masks.  All IO is done before and after it's       */
/*      operation.  This class is not normally used by the              */
/*      application.                                                    */
/************************************************************************/

// This is the number of dummy pixels that must be reserved in source arrays
// in order to satisfy assumptions made in GWKResample(), and more specifically
// by GWKGetPixelRow() that always read a even number of pixels. So if we are
// in the situation to read the last pixel of the source array, we need 1 extra
// dummy pixel to avoid reading out of bounds.
#define WARP_EXTRA_ELTS    1

class CPL_DLL GDALWarpKernel
{
public:
    char              **papszWarpOptions;

    GDALResampleAlg	eResample;
    GDALDataType        eWorkingDataType;
    int                 nBands;

    int                 nSrcXSize;
    int                 nSrcYSize;
    int                 nSrcXExtraSize; /* extra pixels (included in nSrcXSize) reserved for filter window. Should be ignored in scale computation */
    int                 nSrcYExtraSize; /* extra pixels (included in nSrcYSize) reserved for filter window. Should be ignored in scale computation */
    GByte               **papabySrcImage; /* each subarray must have WARP_EXTRA_ELTS at the end */

    GUInt32           **papanBandSrcValid; /* each subarray must have WARP_EXTRA_ELTS at the end */
    GUInt32            *panUnifiedSrcValid; /* must have WARP_EXTRA_ELTS at the end */
    float              *pafUnifiedSrcDensity; /* must have WARP_EXTRA_ELTS at the end */

    int                 nDstXSize;
    int                 nDstYSize;
    GByte             **papabyDstImage;
    GUInt32            *panDstValid;
    float              *pafDstDensity;

    double              dfXScale;   // Resampling scale, i.e.
    double              dfYScale;   // nDstSize/nSrcSize.
    double              dfXFilter;  // Size of filter kernel.
    double              dfYFilter;
    int                 nXRadius;   // Size of window to filter.
    int                 nYRadius;
    int                 nFiltInitX; // Filtering offset
    int                 nFiltInitY;
    
    int                 nSrcXOff;
    int                 nSrcYOff;

    int                 nDstXOff;
    int                 nDstYOff;
        
    GDALTransformerFunc pfnTransformer;
    void                *pTransformerArg;

    GDALProgressFunc    pfnProgress;
    void                *pProgress;

    double              dfProgressBase;
    double              dfProgressScale;
    
    double              *padfDstNoDataReal;

                       GDALWarpKernel();
    virtual           ~GDALWarpKernel();

    CPLErr              Validate();
    CPLErr              PerformWarp();
};

/************************************************************************/
/*                         GDALWarpOperation()                          */
/*                                                                      */
/*      This object is application created, or created by a higher      */
/*      level convenience function.  It is responsible for              */
/*      subdividing the operation into chunks, loading and saving       */
/*      imagery, and establishing the varios validity and density       */
/*      masks.  Actual resampling is done by the GDALWarpKernel.        */
/************************************************************************/

typedef struct _GDALWarpChunk GDALWarpChunk;

class CPL_DLL GDALWarpOperation {
private:
    GDALWarpOptions *psOptions;

    void            WipeOptions();
    int             ValidateOptions();

    CPLErr          ComputeSourceWindow( int nDstXOff, int nDstYOff, 
                                         int nDstXSize, int nDstYSize,
                                         int *pnSrcXOff, int *pnSrcYOff, 
                                         int *pnSrcXSize, int *pnSrcYSize,
                                         int *pnSrcXExtraSize, int *pnSrcYExtraSize,
                                         double* pdfSrcFillRatio );

    CPLErr          CreateKernelMask( GDALWarpKernel *, int iBand, 
                                      const char *pszType );

    CPLMutex        *hIOMutex;
    CPLMutex        *hWarpMutex;

    int             nChunkListCount;
    int             nChunkListMax;
    GDALWarpChunk  *pasChunkList;

    int             bReportTimings;
    unsigned long   nLastTimeReported;

    void            WipeChunkList();
    CPLErr          CollectChunkList( int nDstXOff, int nDstYOff, 
                                      int nDstXSize, int nDstYSize );
    void            ReportTiming( const char * );
    
public:
                    GDALWarpOperation();
    virtual        ~GDALWarpOperation();

    CPLErr          Initialize( const GDALWarpOptions *psNewOptions );

    const GDALWarpOptions         *GetOptions();

    CPLErr          ChunkAndWarpImage( int nDstXOff, int nDstYOff, 
                                       int nDstXSize, int nDstYSize );
    CPLErr          ChunkAndWarpMulti( int nDstXOff, int nDstYOff, 
                                       int nDstXSize, int nDstYSize );
    CPLErr          WarpRegion( int nDstXOff, int nDstYOff, 
                                int nDstXSize, int nDstYSize,
                                int nSrcXOff=0, int nSrcYOff=0,
                                int nSrcXSize=0, int nSrcYSize=0,
                                double dfProgressBase=0.0, double dfProgressScale=1.0);
    CPLErr          WarpRegion( int nDstXOff, int nDstYOff, 
                                int nDstXSize, int nDstYSize,
                                int nSrcXOff, int nSrcYOff,
                                int nSrcXSize, int nSrcYSize,
                                int nSrcXExtraSize, int nSrcYExtraSize,
                                double dfProgressBase, double dfProgressScale);
    CPLErr          WarpRegionToBuffer( int nDstXOff, int nDstYOff, 
                                        int nDstXSize, int nDstYSize, 
                                        void *pDataBuf, 
                                        GDALDataType eBufDataType,
                                        int nSrcXOff=0, int nSrcYOff=0,
                                        int nSrcXSize=0, int nSrcYSize=0,
                                        double dfProgressBase=0.0, double dfProgressScale=1.0);
    CPLErr          WarpRegionToBuffer( int nDstXOff, int nDstYOff, 
                                        int nDstXSize, int nDstYSize, 
                                        void *pDataBuf, 
                                        GDALDataType eBufDataType,
                                        int nSrcXOff, int nSrcYOff,
                                        int nSrcXSize, int nSrcYSize,
                                        int nSrcXExtraSize, int nSrcYExtraSize,
                                        double dfProgressBase, double dfProgressScale);
};

#endif /* def __cplusplus */

CPL_C_START

typedef void * GDALWarpOperationH;

GDALWarpOperationH CPL_DLL GDALCreateWarpOperation(const GDALWarpOptions* );
void CPL_DLL GDALDestroyWarpOperation( GDALWarpOperationH );
CPLErr CPL_DLL GDALChunkAndWarpImage( GDALWarpOperationH, int, int, int, int );
CPLErr CPL_DLL GDALChunkAndWarpMulti( GDALWarpOperationH, int, int, int, int );
CPLErr CPL_DLL GDALWarpRegion( GDALWarpOperationH,
                               int, int, int, int, int, int, int, int );
CPLErr CPL_DLL GDALWarpRegionToBuffer( GDALWarpOperationH, int, int, int, int,
                                       void *, GDALDataType,
                                       int, int, int, int );

/************************************************************************/
/*      Warping kernel functions                                        */
/************************************************************************/

int GWKGetFilterRadius(GDALResampleAlg eResampleAlg);

typedef double (*FilterFuncType)(double dfX);
FilterFuncType GWKGetFilterFunc(GDALResampleAlg eResampleAlg);

typedef double (*FilterFunc4ValuesType)(double* padfVals);
FilterFunc4ValuesType GWKGetFilterFunc4Values(GDALResampleAlg eResampleAlg);

CPL_C_END

#endif /* ndef GDAL_ALG_H_INCLUDED */
