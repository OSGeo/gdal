/******************************************************************************
 * $Id$
 *
 * Project:  GDAL High Performance Warper
 * Purpose:  Prototypes, and definitions for warping related work.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.10  2003/07/04 11:50:57  dron
 * GRA_CubicSpline added to the list of resampling algorithms.
 *
 * Revision 1.9  2003/06/12 11:21:33  dron
 * Few additional comments.
 *
 * Revision 1.8  2003/05/27 20:49:25  warmerda
 * added REPORT_TIMINGS support
 *
 * Revision 1.7  2003/05/07 19:13:06  warmerda
 * added pre and post warp chunk processor
 *
 * Revision 1.6  2003/04/23 05:18:57  warmerda
 * added multithread support
 *
 * Revision 1.5  2003/03/02 05:25:59  warmerda
 * added some source nodata support
 *
 * Revision 1.4  2003/02/22 02:04:11  warmerda
 * added dfMaxError to reproject function
 *
 * Revision 1.3  2003/02/21 15:41:37  warmerda
 * added progressbase/scale for operation
 *
 * Revision 1.2  2003/02/20 21:53:06  warmerda
 * partial implementation
 *
 * Revision 1.1  2003/02/18 17:25:50  warmerda
 * New
 *
 */

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

CPL_C_START

/*! Warp Resampling Algorithm */
typedef enum {
  /*! Nearest neighbour (select on one input pixel) */ GRA_NearestNeighbour=0,
  /*! Bilinear (2x2 kernel) */                         GRA_Bilinear=1,
  /*! Cubic Convolution Approximation (4x4 kernel) */  GRA_Cubic=2,
  /*! Cubic B-Spline Approximation (4x4 kernel) */     GRA_CubicSpline=3,
} GDALResampleAlg;

typedef int 
(*GDALMaskFunc)( void *pMaskFuncArg,
                 int nBandCount, GDALDataType eType, 
                 int nXOff, int nYOff, 
                 int nXSize, int nYSize,
                 GByte **papabyImageData, 
                 int bMaskIsFloat, void *pMask );

CPLErr GDALWarpNoDataMasker( void *pMaskFuncArg, int nBandCount, 
                             GDALDataType eType,
                             int nXOff, int nYOff, int nXSize, int nYSize,
                             GByte **papabyImageData, int bMaskIsFloat,
                             void *pValidityMask );

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

} GDALWarpOptions;

GDALWarpOptions CPL_DLL *GDALCreateWarpOptions();
void CPL_DLL GDALDestroyWarpOptions( GDALWarpOptions * );
GDALWarpOptions CPL_DLL *GDALCloneWarpOptions( const GDALWarpOptions * );

/************************************************************************/
/*                         GDALReprojectImage()                         */
/************************************************************************/

CPLErr CPL_DLL 
GDALReprojectImage( GDALDatasetH hSrcDS, const char *pszSrcWKT, 
                    GDALDatasetH hDstDS, const char *pszDstWKT,
                    GDALResampleAlg eResampleAlg, double dfWarpMemoryLimit,
                    double dfMaxError,
                    GDALProgressFunc pfnProgress, void *pProgressArg, 
                    GDALWarpOptions *psOptions );

CPLErr CPL_DLL 
GDALCreateAndReprojectImage( GDALDatasetH hSrcDS, const char *pszSrcWKT, 
                    const char *pszDstFilename, const char *pszDstWKT,
                    GDALDriverH hDstDriver, char **papszCreateOptions,
                    GDALResampleAlg eResampleAlg, double dfWarpMemoryLimit,
                    double dfMaxError,
                    GDALProgressFunc pfnProgress, void *pProgressArg, 
                    GDALWarpOptions *psOptions );

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

class CPL_DLL GDALWarpKernel
{
public:
                       GDALWarpKernel();
    virtual           ~GDALWarpKernel();

    char              **papszWarpOptions;

    GDALResampleAlg	eResample;
    GDALDataType        eWorkingDataType;
    int                 nBands;

    int                 nSrcXSize;
    int                 nSrcYSize;
    GByte               **papabySrcImage;

    GUInt32           **papanBandSrcValid;
    GUInt32            *panUnifiedSrcValid;
    float              *pafUnifiedSrcDensity;

    int                 nDstXSize;
    int                 nDstYSize;
    GByte             **papabyDstImage;
    GUInt32            *panDstValid;
    float              *pafDstDensity;
    
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

class CPL_DLL GDALWarpOperation {
private:
    GDALWarpOptions *psOptions;

    double          dfProgressBase;
    double          dfProgressScale;

    void            WipeOptions();
    int             ValidateOptions();

    CPLErr          ComputeSourceWindow( int nDstXOff, int nDstYOff, 
                                         int nDstXSize, int nDstYSize,
                                         int *pnSrcXOff, int *pnSrcYOff, 
                                         int *pnSrcXSize, int *pnSrcYSize );

    CPLErr          CreateKernelMask( GDALWarpKernel *, int iBand, 
                                      const char *pszType );

    void            *hThread1Mutex;
    void            *hThread2Mutex;
    void            *hIOMutex;
    void            *hWarpMutex;

    int             nChunkListCount;
    int             nChunkListMax;
    int            *panChunkList;

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
                                int nSrcXSize=0, int nSrcYSize=0 );
    
    CPLErr          WarpRegionToBuffer( int nDstXOff, int nDstYOff, 
                                        int nDstXSize, int nDstYSize, 
                                        void *pDataBuf, 
                                        GDALDataType eBufDataType,
                                        int nSrcXOff=0, int nSrcYOff=0,
                                        int nSrcXSize=0, int nSrcYSize=0 );
};

#endif /* def __cplusplus */

#endif /* ndef GDAL_ALG_H_INCLUDED */
