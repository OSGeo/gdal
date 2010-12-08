/******************************************************************************
 * $Id$
 *
 * Project:  OpenCL Image Reprojector
 * Purpose:  Implementation of the GDALWarpKernel reprojector in OpenCL.
 * Author:   Seth Price, seth@pricepages.org
 *
 ******************************************************************************
 * Copyright (c) 2010, Seth Price <seth@pricepages.org>
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

#if defined(HAVE_OPENCL)

/* The following relates to the profiling calls to 
   clSetCommandQueueProperty() which are not available by default
   with some OpenCL implementation (ie. ATI) */

#define CL_USE_DEPRECATED_OPENCL_1_0_APIS

#ifdef __APPLE__
#include <OpenCL/OpenCL.h>
#else
#include <CL/opencl.h>
#endif

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

typedef enum {
    OCL_Bilinear=10,
    OCL_Cubic=11,
    OCL_CubicSpline=12,
    OCL_Lanczos=13
} OCLResampAlg;
    
struct oclWarper {
    cl_command_queue queue;
    cl_context context;
    cl_device_id dev;
    cl_kernel kern1;
    cl_kernel kern4;
    
    int srcWidth;
    int srcHeight;
    int dstWidth;
    int dstHeight;
    
    int useUnifiedSrcDensity;
    int useUnifiedSrcValid;
    int useDstDensity;
    int useDstValid;
    
    int numBands;
    int numImages;
    OCLResampAlg resampAlg;
    
    cl_channel_type imageFormat;
    cl_mem *realWorkCL;
    union {
        void **v;
        char **c;
        unsigned char **uc;
        short **s;
        unsigned short **us;
        float **f;
    } realWork;
    
    cl_mem *imagWorkCL;
    union {
        void **v;
        char **c;
        unsigned char **uc;
        short **s;
        unsigned short **us;
        float **f;
    } imagWork;
    
    cl_mem *dstRealWorkCL;
    union {
        void **v;
        char **c;
        unsigned char **uc;
        short **s;
        unsigned short **us;
        float **f;
    } dstRealWork;
    
    cl_mem *dstImagWorkCL;
    union {
        void **v;
        char **c;
        unsigned char **uc;
        short **s;
        unsigned short **us;
        float **f;
    } dstImagWork;
    
    unsigned int imgChSize1;
    cl_channel_order imgChOrder1;
    unsigned int imgChSize4;
    cl_channel_order imgChOrder4;
	char    useVec;
    
    cl_mem useBandSrcValidCL;
    char *useBandSrcValid;
    
    cl_mem nBandSrcValidCL;
    float *nBandSrcValid;
    
    cl_mem xyWorkCL;
    float *xyWork;
    
    int xyWidth;
    int xyHeight;
    int coordMult;
    
    unsigned int xyChSize;
    cl_channel_order xyChOrder;
    
    cl_mem fDstNoDataRealCL;
    float *fDstNoDataReal;

    int bIsATI;
};

struct oclWarper* GDALWarpKernelOpenCL_createEnv(int srcWidth, int srcHeight,
                                                 int dstWidth, int dstHeight,
                                                 cl_channel_type imageFormat,
                                                 int numBands, int coordMult,
                                                 int useImag, int useBandSrcValid,
                                                 float *fDstDensity,
                                                 double *dfDstNoDataReal,
                                                 OCLResampAlg resampAlg, cl_int *envErr);

cl_int GDALWarpKernelOpenCL_setSrcValid(struct oclWarper *warper,
                                        int *bandSrcValid, int bandNum);

cl_int GDALWarpKernelOpenCL_setSrcImg(struct oclWarper *warper, void *imgData,
                                      int bandNum);

cl_int GDALWarpKernelOpenCL_setDstImg(struct oclWarper *warper, void *imgData,
                                      int bandNum);

cl_int GDALWarpKernelOpenCL_setCoordRow(struct oclWarper *warper,
                                        double *rowSrcX, double *rowSrcY,
                                        double srcXOff, double srcYOff,
                                        int *success, int rowNum);

cl_int GDALWarpKernelOpenCL_runResamp(struct oclWarper *warper,
                                      float *unifiedSrcDensity,
                                      unsigned int *unifiedSrcValid,
                                      float *dstDensity,
                                      unsigned int *dstValid,
                                      double dfXScale, double dfYScale,
                                      double dfXFilter, double dfYFilter,
                                      int nXRadius, int nYRadius,
                                      int nFiltInitX, int nFiltInitY);

cl_int GDALWarpKernelOpenCL_getRow(struct oclWarper *warper,
                                   void **rowReal, void **rowImag,
                                   int rowNum, int bandNum);

cl_int GDALWarpKernelOpenCL_deleteEnv(struct oclWarper *warper);

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif

#endif /* defined(HAVE_OPENCL) */

