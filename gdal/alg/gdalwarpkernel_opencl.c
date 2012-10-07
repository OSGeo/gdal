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

/* The following line may be uncommented for increased debugging traces and profiling */
/* #define DEBUG_OPENCL 1 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>
#include "cpl_string.h"
#include "gdalwarpkernel_opencl.h"

CPL_CVSID("$Id$");

#define handleErr(err) if((err) != CL_SUCCESS) { \
    CPLError(CE_Failure, CPLE_AppDefined, "Error at file %s line %d: %s", __FILE__, __LINE__, getCLErrorString(err)); \
    return err; \
}

#define handleErrRetNULL(err) if((err) != CL_SUCCESS) { \
    (*clErr) = err; \
    CPLError(CE_Failure, CPLE_AppDefined, "Error at file %s line %d: %s", __FILE__, __LINE__, getCLErrorString(err)); \
    return NULL; \
}

#define handleErrGoto(err, goto_label) if((err) != CL_SUCCESS) { \
    (*clErr) = err; \
    CPLError(CE_Failure, CPLE_AppDefined, "Error at file %s line %d: %s", __FILE__, __LINE__, getCLErrorString(err)); \
    goto goto_label; \
}

#define freeCLMem(clMem, fallBackMem)  do { \
    if ((clMem) != NULL) { \
        handleErr(err = clReleaseMemObject(clMem)); \
        clMem = NULL; \
        fallBackMem = NULL; \
    } else if ((fallBackMem) != NULL) { \
        CPLFree(fallBackMem); \
        fallBackMem = NULL; \
    } \
} while(0)

static const char* getCLErrorString(cl_int err)
{
    switch (err)
    {
        case CL_SUCCESS:
            return("CL_SUCCESS");
            break;
        case CL_DEVICE_NOT_FOUND:
            return("CL_DEVICE_NOT_FOUND");
            break;
        case CL_DEVICE_NOT_AVAILABLE:
            return("CL_DEVICE_NOT_AVAILABLE");
            break;
        case CL_COMPILER_NOT_AVAILABLE:
            return("CL_COMPILER_NOT_AVAILABLE");
            break;
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:
            return("CL_MEM_OBJECT_ALLOCATION_FAILURE");
            break;
        case CL_OUT_OF_RESOURCES:
            return("CL_OUT_OF_RESOURCES");
            break;
        case CL_OUT_OF_HOST_MEMORY:
            return("CL_OUT_OF_HOST_MEMORY");
            break;
        case CL_PROFILING_INFO_NOT_AVAILABLE:
            return("CL_PROFILING_INFO_NOT_AVAILABLE");
            break;
        case CL_MEM_COPY_OVERLAP:
            return("CL_MEM_COPY_OVERLAP");
            break;
        case CL_IMAGE_FORMAT_MISMATCH:
            return("CL_IMAGE_FORMAT_MISMATCH");
            break;
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:
            return("CL_IMAGE_FORMAT_NOT_SUPPORTED");
            break;
        case CL_BUILD_PROGRAM_FAILURE:
            return("CL_BUILD_PROGRAM_FAILURE");
            break;
        case CL_MAP_FAILURE:
            return("CL_MAP_FAILURE");
            break;
        case CL_INVALID_VALUE:
            return("CL_INVALID_VALUE");
            break;
        case CL_INVALID_DEVICE_TYPE:
            return("CL_INVALID_DEVICE_TYPE");
            break;
        case CL_INVALID_PLATFORM:
            return("CL_INVALID_PLATFORM");
            break;
        case CL_INVALID_DEVICE:
            return("CL_INVALID_DEVICE");
            break;
        case CL_INVALID_CONTEXT:
            return("CL_INVALID_CONTEXT");
            break;
        case CL_INVALID_QUEUE_PROPERTIES:
            return("CL_INVALID_QUEUE_PROPERTIES");
            break;
        case CL_INVALID_COMMAND_QUEUE:
            return("CL_INVALID_COMMAND_QUEUE");
            break;
        case CL_INVALID_HOST_PTR:
            return("CL_INVALID_HOST_PTR");
            break;
        case CL_INVALID_MEM_OBJECT:
            return("CL_INVALID_MEM_OBJECT");
            break;
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
            return("CL_INVALID_IMAGE_FORMAT_DESCRIPTOR");
            break;
        case CL_INVALID_IMAGE_SIZE:
            return("CL_INVALID_IMAGE_SIZE");
            break;
        case CL_INVALID_SAMPLER:
            return("CL_INVALID_SAMPLER");
            break;
        case CL_INVALID_BINARY:
            return("CL_INVALID_BINARY");
            break;
        case CL_INVALID_BUILD_OPTIONS:
            return("CL_INVALID_BUILD_OPTIONS");
            break;
        case CL_INVALID_PROGRAM:
            return("CL_INVALID_PROGRAM");
            break;
        case CL_INVALID_PROGRAM_EXECUTABLE:
            return("CL_INVALID_PROGRAM_EXECUTABLE");
            break;
        case CL_INVALID_KERNEL_NAME:
            return("CL_INVALID_KERNEL_NAME");
            break;
        case CL_INVALID_KERNEL_DEFINITION:
            return("CL_INVALID_KERNEL_DEFINITION");
            break;
        case CL_INVALID_KERNEL:
            return("CL_INVALID_KERNEL");
            break;
        case CL_INVALID_ARG_INDEX:
            return("CL_INVALID_ARG_INDEX");
            break;
        case CL_INVALID_ARG_VALUE:
            return("CL_INVALID_ARG_VALUE");
            break;
        case CL_INVALID_ARG_SIZE:
            return("CL_INVALID_ARG_SIZE");
            break;
        case CL_INVALID_KERNEL_ARGS:
            return("CL_INVALID_KERNEL_ARGS");
            break;
        case CL_INVALID_WORK_DIMENSION:
            return("CL_INVALID_WORK_DIMENSION");
            break;
        case CL_INVALID_WORK_GROUP_SIZE:
            return("CL_INVALID_WORK_GROUP_SIZE");
            break;
        case CL_INVALID_WORK_ITEM_SIZE:
            return("CL_INVALID_WORK_ITEM_SIZE");
            break;
        case CL_INVALID_GLOBAL_OFFSET:
            return("CL_INVALID_GLOBAL_OFFSET");
            break;
        case CL_INVALID_EVENT_WAIT_LIST:
            return("CL_INVALID_EVENT_WAIT_LIST");
            break;
        case CL_INVALID_EVENT:
            return("CL_INVALID_EVENT");
            break;
        case CL_INVALID_OPERATION:
            return("CL_INVALID_OPERATION");
            break;
        case CL_INVALID_GL_OBJECT:
            return("CL_INVALID_GL_OBJECT");
            break;
        case CL_INVALID_BUFFER_SIZE:
            return("CL_INVALID_BUFFER_SIZE");
            break;
        case CL_INVALID_MIP_LEVEL:
            return("CL_INVALID_MIP_LEVEL");
            break;
        case CL_INVALID_GLOBAL_WORK_SIZE:
            return("CL_INVALID_GLOBAL_WORK_SIZE");
            break;
    }

    return "unknown_error";
}

static const char* getCLDataTypeString( cl_channel_type dataType )
{
    switch( dataType )
    {
        case CL_SNORM_INT8: return "CL_SNORM_INT8";
        case CL_SNORM_INT16: return "CL_SNORM_INT16";
        case CL_UNORM_INT8: return "CL_UNORM_INT8";
        case CL_UNORM_INT16: return "CL_UNORM_INT16";
#if 0
        case CL_UNORM_SHORT_565: return "CL_UNORM_SHORT_565";
        case CL_UNORM_SHORT_555: return "CL_UNORM_SHORT_555";
        case CL_UNORM_INT_101010: return "CL_UNORM_INT_101010";
        case CL_SIGNED_INT8: return "CL_SIGNED_INT8";
        case CL_SIGNED_INT16: return "CL_SIGNED_INT16";
        case CL_SIGNED_INT32: return "CL_SIGNED_INT32";
        case CL_UNSIGNED_INT8: return "CL_UNSIGNED_INT8";
        case CL_UNSIGNED_INT16: return "CL_UNSIGNED_INT16";
        case CL_UNSIGNED_INT32: return "CL_UNSIGNED_INT32";
        case CL_HALF_FLOAT: return "CL_HALF_FLOAT";
#endif
        case CL_FLOAT: return "CL_FLOAT";
        default: return "unknown";
    }
}

/*
 Finds an appropirate OpenCL device. If the user specifies a preference, the
 code for it should be here (but not currently supported). For debugging, it's
 always easier to use CL_DEVICE_TYPE_CPU because then printf() can be called
 from the kernel. If debugging is on, we can print the name and stats about the
 device we're using.
 */
cl_device_id get_device(OCLVendor *peVendor)
{
    cl_int err = 0;
    cl_device_id device = NULL;
    size_t returned_size = 0;
    cl_char vendor_name[1024] = {0};
    cl_char device_name[1024] = {0};
    
    cl_platform_id platforms[10];
    cl_uint num_platforms;

    err = clGetPlatformIDs( 10, platforms, &num_platforms );
    if( err != CL_SUCCESS || num_platforms == 0 )
        return NULL;

    // Find the GPU CL device, this is what we really want
    // If there is no GPU device is CL capable, fall back to CPU
    err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS)
    {
        // Find the CPU CL device, as a fallback
        err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_CPU, 1, &device, NULL);
        if( err != CL_SUCCESS || device == 0 )
            return NULL;
    }
    
    // Get some information about the returned device
    err = clGetDeviceInfo(device, CL_DEVICE_VENDOR, sizeof(vendor_name), 
                          vendor_name, &returned_size);
    err |= clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), 
                           device_name, &returned_size);
    assert(err == CL_SUCCESS);
    CPLDebug( "OpenCL", "Connected to %s %s.", vendor_name, device_name);

    if (strncmp((const char*)vendor_name, "Advanced Micro Devices", strlen("Advanced Micro Devices")) == 0)
        *peVendor = VENDOR_AMD;
    else if (strncmp((const char*)vendor_name, "Intel(R) Corporation", strlen("Intel(R) Corporation")) == 0)
        *peVendor = VENDOR_INTEL;
    else
        *peVendor = VENDOR_OTHER;

    return device;
}

/*
 Given that not all OpenCL devices support the same image formats, we need to
 make do with what we have. This leads to wasted space, but as OpenCL matures
 I hope it'll get better.
 */
cl_int set_supported_formats(struct oclWarper *warper,
                             cl_channel_order minOrderSize,
                             cl_channel_order *chosenOrder,
                             unsigned int *chosenSize,
                             cl_channel_type dataType )
{
    cl_image_format *fmtBuf = (cl_image_format *)calloc(256, sizeof(cl_image_format));
    cl_uint numRet;
    int i;
    int extraSpace = 9999;
    cl_int err;
    int bFound = FALSE;
    
    //Find what we *can* handle
    handleErr(err = clGetSupportedImageFormats(warper->context,
                                               CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                               CL_MEM_OBJECT_IMAGE2D,
                                               256, fmtBuf, &numRet));
    for (i = 0; i < numRet; ++i) {
        int thisOrderSize = 0;
        switch (fmtBuf[i].image_channel_order)
        {
            //Only support formats which use the channels in order (x,y,z,w)
          case CL_R:
          case CL_INTENSITY:
          case CL_LUMINANCE:
            thisOrderSize = 1;
            break;
          case CL_RG:
            thisOrderSize = 2;
            break;
          case CL_RGB:
            thisOrderSize = 3;
            break;
          case CL_RGBA:
            thisOrderSize = 4;
            break;
        }
        
        //Choose an order with the least wasted space
        if (fmtBuf[i].image_channel_data_type == dataType &&
            minOrderSize <= thisOrderSize &&
            extraSpace > thisOrderSize - minOrderSize ) {
			
            //Set the vector size, order, & remember wasted space
            (*chosenSize) = thisOrderSize;
            (*chosenOrder) = fmtBuf[i].image_channel_order;
            extraSpace = thisOrderSize - minOrderSize;
            bFound = TRUE;
        }
    }
    
    free(fmtBuf);
    
    if( !bFound )
    {
        CPLDebug("OpenCL",
                 "Cannot find supported format for dataType = %s and minOrderSize = %d",
                 getCLDataTypeString(dataType), (int)minOrderSize);
    }
    return (bFound) ? CL_SUCCESS : CL_INVALID_OPERATION;
}

/*
 Allocate some pinned memory that we can use as an intermediate buffer. We're
 using the pinned memory to assemble the data before transferring it to the
 device. The reason we're using pinned RAM is because the transfer speed from
 host RAM to device RAM is faster than non-pinned. The disadvantage is that
 pinned RAM is a scarce OS resource. I'm making the assumption that the user
 has as much pinned host RAM available as total device RAM because device RAM
 tends to be similarly scarce. However, if the pinned memory fails we fall back
 to using a regular memory allocation.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int alloc_pinned_mem(struct oclWarper *warper, int imgNum, size_t dataSz,
                        void **wrkPtr, cl_mem *wrkCL)
{
	cl_int err = CL_SUCCESS;
    wrkCL[imgNum] = clCreateBuffer(warper->context,
                                   CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR,
                                   dataSz, NULL, &err);

    if (err == CL_SUCCESS) {
        wrkPtr[imgNum] = (void *)clEnqueueMapBuffer(warper->queue, wrkCL[imgNum],
                                                    CL_FALSE, CL_MAP_WRITE,
                                                    0, dataSz, 0, NULL, NULL, &err);
        handleErr(err);
    } else {
        wrkCL[imgNum] = NULL;
#ifdef DEBUG_OPENCL
        CPLDebug("OpenCL", "Using fallback non-pinned memory!");
#endif
        //Fallback to regular allocation
        wrkPtr[imgNum] = (void *)VSIMalloc(dataSz);
        
        if (wrkPtr[imgNum] == NULL)
            handleErr(err = CL_OUT_OF_HOST_MEMORY);
    }
    
    return CL_SUCCESS;
}

/*
 Allocates the working host memory for all bands of the image in the warper
 structure. This includes both the source image buffers and the destination
 buffers. This memory is located on the host, so we can assemble the image.
 Reasons for buffering it like this include reading each row from disk and
 de-interleaving bands and parts of bands. Then they can be copied to the device
 as a single operation fit for use as an OpenCL memory object.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int alloc_working_arr(struct oclWarper *warper,
                         size_t ptrSz, size_t dataSz, size_t *fmtSz)
{
	cl_int err = CL_SUCCESS;
    int i, b;
    size_t srcDataSz1, dstDataSz1, srcDataSz4, dstDataSz4;
    const int numBands = warper->numBands;
    
    //Find the best channel order for this format
    err = set_supported_formats(warper, 1,
                                &(warper->imgChOrder1), &(warper->imgChSize1),
                                warper->imageFormat);
    handleErr(err);
    if(warper->useVec) {
        err = set_supported_formats(warper, 4,
                                    &(warper->imgChOrder4), &(warper->imgChSize4),
                                    warper->imageFormat);
        handleErr(err);
    }
    
    //Alloc space for pointers to the main image data
    warper->realWork.v = (void **)VSICalloc(ptrSz, warper->numImages);
    warper->dstRealWork.v = (void **)VSICalloc(ptrSz, warper->numImages);
    if (warper->realWork.v == NULL || warper->dstRealWork.v == NULL)
        handleErr(err = CL_OUT_OF_HOST_MEMORY);
    
    if (warper->imagWorkCL != NULL) {
        //Alloc space for pointers to the extra channel, if it exists
        warper->imagWork.v = (void **)VSICalloc(ptrSz, warper->numImages);
        warper->dstImagWork.v = (void **)VSICalloc(ptrSz, warper->numImages);
        if (warper->imagWork.v == NULL || warper->dstImagWork.v == NULL)
            handleErr(err = CL_OUT_OF_HOST_MEMORY);
    } else {
        warper->imagWork.v = NULL;
        warper->dstImagWork.v = NULL;
    }
    
    //Calc the sizes we need
    srcDataSz1 = dataSz * warper->srcWidth * warper->srcHeight * warper->imgChSize1;
    dstDataSz1 = dataSz * warper->dstWidth * warper->dstHeight;
    srcDataSz4 = dataSz * warper->srcWidth * warper->srcHeight * warper->imgChSize4;
    dstDataSz4 = dataSz * warper->dstWidth * warper->dstHeight * 4;
    
    //Allocate pinned memory for each band's image
    for (b = 0, i = 0; b < numBands && i < warper->numImages; ++i) {
        if(warper->useVec && b < numBands - numBands % 4) {
            handleErr(err = alloc_pinned_mem(warper, i, srcDataSz4,
                                             warper->realWork.v,
                                             warper->realWorkCL));
            
            handleErr(err = alloc_pinned_mem(warper, i, dstDataSz4,
                                             warper->dstRealWork.v,
                                             warper->dstRealWorkCL));
            b += 4;
        } else {
            handleErr(err = alloc_pinned_mem(warper, i, srcDataSz1,
                                             warper->realWork.v,
                                             warper->realWorkCL));
            
            handleErr(err = alloc_pinned_mem(warper, i, dstDataSz1,
                                             warper->dstRealWork.v,
                                             warper->dstRealWorkCL));
            ++b;
        }
    }
    
    if (warper->imagWorkCL != NULL) {
        //Allocate pinned memory for each band's extra channel, if exists
        for (b = 0, i = 0; b < numBands && i < warper->numImages; ++i) {
            if(warper->useVec && b < numBands - numBands % 4) {
                handleErr(err = alloc_pinned_mem(warper, i, srcDataSz4,
                                                 warper->imagWork.v,
                                                 warper->imagWorkCL));
                
                handleErr(err = alloc_pinned_mem(warper, i, dstDataSz4,
                                                 warper->dstImagWork.v,
                                                 warper->dstImagWorkCL));
                b += 4;
            } else {
                handleErr(err = alloc_pinned_mem(warper, i, srcDataSz1,
                                                 warper->imagWork.v,
                                                 warper->imagWorkCL));
                
                handleErr(err = alloc_pinned_mem(warper, i, dstDataSz1,
                                                 warper->dstImagWork.v,
                                                 warper->dstImagWorkCL));
                ++b;
            }
        }
    }
    
    return CL_SUCCESS;
}

/*
 Assemble and create the kernel. For optimization, portabilaty, and
 implimentation limitation reasons, the program is actually assembled from
 several strings, then compiled with as many invariants as possible defined by
 the preprocessor. There is also quite a bit of error-catching code in here
 because the kernel is where many bugs show up.
 
 Returns CL_SUCCESS on success and other CL_* errors in the error buffer when
 something goes wrong.
 */
cl_kernel get_kernel(struct oclWarper *warper, char useVec,
                     double dfXScale, double dfYScale, double dfXFilter, double dfYFilter,
                     int nXRadius, int nYRadius, int nFiltInitX, int nFiltInitY,
                     cl_int *clErr )
{
	cl_program program;
    cl_kernel kernel;
	cl_int err = CL_SUCCESS;
    char *buffer = (char *)CPLCalloc(128000, sizeof(char));
    char *progBuf = (char *)CPLCalloc(128000, sizeof(char));
    float dstMinVal, dstMaxVal;
    
    const char *outType;
    const char *dUseVec = "";
    const char *dVecf = "float";
    const char *kernGenFuncs =
// ********************* General Funcs ********************
"#ifdef USE_CLAMP_TO_DST_FLOAT\n"
"void clampToDst(float fReal,\n"
                "__global outType *dstPtr,\n"
                "unsigned int iDstOffset,\n"
                "__constant float *fDstNoDataReal,\n"
                "int bandNum)\n"
"{\n"
    "dstPtr[iDstOffset] = fReal;\n"
"}\n"
"#else\n"
"void clampToDst(float fReal,\n"
                "__global outType *dstPtr,\n"
                "unsigned int iDstOffset,\n"
                "__constant float *fDstNoDataReal,\n"
                "int bandNum)\n"
"{\n"
	"fReal *= dstMaxVal;\n"
    
    "if (fReal < dstMinVal)\n"
        "dstPtr[iDstOffset] = (outType)dstMinVal;\n"
    "else if (fReal > dstMaxVal)\n"
        "dstPtr[iDstOffset] = (outType)dstMaxVal;\n"
    "else\n"
        "dstPtr[iDstOffset] = (dstMinVal < 0) ? (outType)floor(fReal + 0.5f) : (outType)(fReal + 0.5f);\n"
    
    "if (useDstNoDataReal && bandNum >= 0 &&\n"
        "fDstNoDataReal[bandNum] == dstPtr[iDstOffset])\n"
    "{\n"
        "if (dstPtr[iDstOffset] == dstMinVal)\n"
            "dstPtr[iDstOffset] = dstMinVal + 1;\n"
        "else\n"
            "dstPtr[iDstOffset] --;\n"
    "}\n"
"}\n"
"#endif\n"

"void setPixel(__global outType *dstReal,\n"
              "__global outType *dstImag,\n"
              "__global float *dstDensity,\n"
              "__global int *nDstValid,\n"
              "__constant float *fDstNoDataReal,\n"
              "const int bandNum,\n"
              "vecf fDensity, vecf fReal, vecf fImag)\n"
"{\n"
    "unsigned int iDstOffset = get_global_id(1)*iDstWidth + get_global_id(0);\n"

"#ifdef USE_VEC\n"
    "if (fDensity.x < 0.00001f && fDensity.y < 0.00001f &&\n"
        "fDensity.z < 0.00001f && fDensity.w < 0.00001f ) {\n"
    
        "fReal = 0.0f;\n"
        "fImag = 0.0f;\n"
    
    "} else if (fDensity.x < 0.9999f || fDensity.y < 0.9999f ||\n"
               "fDensity.z < 0.9999f || fDensity.w < 0.9999f ) {\n"
        "vecf fDstReal, fDstImag;\n"
        "float fDstDensity;\n"
    
        "fDstReal.x = dstReal[iDstOffset];\n"
        "fDstReal.y = dstReal[iDstOffset+iDstHeight*iDstWidth];\n"
        "fDstReal.z = dstReal[iDstOffset+iDstHeight*iDstWidth*2];\n"
        "fDstReal.w = dstReal[iDstOffset+iDstHeight*iDstWidth*3];\n"
        "if (useImag) {\n"
            "fDstImag.x = dstImag[iDstOffset];\n"
            "fDstImag.y = dstImag[iDstOffset+iDstHeight*iDstWidth];\n"
            "fDstImag.z = dstImag[iDstOffset+iDstHeight*iDstWidth*2];\n"
            "fDstImag.w = dstImag[iDstOffset+iDstHeight*iDstWidth*3];\n"
        "}\n"
"#else\n"
    "if (fDensity < 0.00001f) {\n"
    
        "fReal = 0.0f;\n"
        "fImag = 0.0f;\n"
    
    "} else if (fDensity < 0.9999f) {\n"
        "vecf fDstReal, fDstImag;\n"
        "float fDstDensity;\n"
    
        "fDstReal = dstReal[iDstOffset];\n"
        "if (useImag)\n"
            "fDstImag = dstImag[iDstOffset];\n"
"#endif\n"
    
        "if (useDstDensity)\n"
            "fDstDensity = dstDensity[iDstOffset];\n"
        "else if (useDstValid &&\n"
                 "!((nDstValid[iDstOffset>>5] & (0x01 << (iDstOffset & 0x1f))) ))\n"
            "fDstDensity = 0.0f;\n"
        "else\n"
            "fDstDensity = 1.0f;\n"
        
        "vecf fDstInfluence = (1.0f - fDensity) * fDstDensity;\n"
        
        // Density should be checked for <= 0.0 & handled by the calling function
        "fReal = (fReal * fDensity + fDstReal * fDstInfluence) / (fDensity + fDstInfluence);\n"
        "if (useImag)\n"
            "fImag = (fImag * fDensity + fDstImag * fDstInfluence) / (fDensity + fDstInfluence);\n"
    "}\n"
    
"#ifdef USE_VEC\n"
    "clampToDst(fReal.x, dstReal, iDstOffset, fDstNoDataReal, bandNum);\n"
    "clampToDst(fReal.y, dstReal, iDstOffset+iDstHeight*iDstWidth, fDstNoDataReal, bandNum);\n"
    "clampToDst(fReal.z, dstReal, iDstOffset+iDstHeight*iDstWidth*2, fDstNoDataReal, bandNum);\n"
    "clampToDst(fReal.w, dstReal, iDstOffset+iDstHeight*iDstWidth*3, fDstNoDataReal, bandNum);\n"
    "if (useImag) {\n"
        "clampToDst(fImag.x, dstImag, iDstOffset, fDstNoDataReal, -1);\n"
        "clampToDst(fImag.y, dstImag, iDstOffset+iDstHeight*iDstWidth, fDstNoDataReal, -1);\n"
        "clampToDst(fImag.z, dstImag, iDstOffset+iDstHeight*iDstWidth*2, fDstNoDataReal, -1);\n"
        "clampToDst(fImag.w, dstImag, iDstOffset+iDstHeight*iDstWidth*3, fDstNoDataReal, -1);\n"
    "}\n"
"#else\n"
    "clampToDst(fReal, dstReal, iDstOffset, fDstNoDataReal, bandNum);\n"
    "if (useImag)\n"
        "clampToDst(fImag, dstImag, iDstOffset, fDstNoDataReal, -1);\n"
"#endif\n"
"}\n"

"int getPixel(__read_only image2d_t srcReal,\n"
             "__read_only image2d_t srcImag,\n"
             "__global float *fUnifiedSrcDensity,\n"
             "__global int *nUnifiedSrcValid,\n"
             "__constant char *useBandSrcValid,\n"
             "__global int *nBandSrcValid,\n"
             "const int2 iSrc,\n"
             "int bandNum,\n"
             "vecf *fDensity, vecf *fReal, vecf *fImag)\n"
"{\n"
    "int iSrcOffset = 0, iBandValidLen = 0, iSrcOffsetMask = 0;\n"
    "int bHasValid = FALSE;\n"
    
    // Clamp the src offset values if needed
    "if(useUnifiedSrcDensity || useUnifiedSrcValid || useUseBandSrcValid){\n"
        "int iSrcX = iSrc.x;\n"
        "int iSrcY = iSrc.y;\n"
        
        // Needed because the offset isn't clamped in OpenCL hardware
        "if(iSrcX < 0)\n"
            "iSrcX = 0;\n"
        "else if(iSrcX >= iSrcWidth)\n"
            "iSrcX = iSrcWidth - 1;\n"
            
        "if(iSrcY < 0)\n"
            "iSrcY = 0;\n"
        "else if(iSrcY >= iSrcHeight)\n"
            "iSrcY = iSrcHeight - 1;\n"
            
        "iSrcOffset = iSrcY*iSrcWidth + iSrcX;\n"
        "iBandValidLen = 1 + ((iSrcWidth*iSrcHeight)>>5);\n"
        "iSrcOffsetMask = (0x01 << (iSrcOffset & 0x1f));\n"
    "}\n"
    
    "if (useUnifiedSrcValid &&\n"
        "!((nUnifiedSrcValid[iSrcOffset>>5] & iSrcOffsetMask) ) )\n"
        "return FALSE;\n"
    
"#ifdef USE_VEC\n"
    "if (!useUseBandSrcValid || !useBandSrcValid[bandNum] ||\n"
        "((nBandSrcValid[(iSrcOffset>>5)+iBandValidLen*bandNum    ] & iSrcOffsetMask)) )\n"
        "bHasValid = TRUE;\n"
    
    "if (!useUseBandSrcValid || !useBandSrcValid[bandNum+1] ||\n"
        "((nBandSrcValid[(iSrcOffset>>5)+iBandValidLen*(1+bandNum)] & iSrcOffsetMask)) )\n"
        "bHasValid = TRUE;\n"
    
    "if (!useUseBandSrcValid || !useBandSrcValid[bandNum+2] ||\n"
        "((nBandSrcValid[(iSrcOffset>>5)+iBandValidLen*(2+bandNum)] & iSrcOffsetMask)) )\n"
        "bHasValid = TRUE;\n"
    
    "if (!useUseBandSrcValid || !useBandSrcValid[bandNum+3] ||\n"
        "((nBandSrcValid[(iSrcOffset>>5)+iBandValidLen*(3+bandNum)] & iSrcOffsetMask)) )\n"
        "bHasValid = TRUE;\n"
"#else\n"
    "if (!useUseBandSrcValid || !useBandSrcValid[bandNum] ||\n"
        "((nBandSrcValid[(iSrcOffset>>5)+iBandValidLen*bandNum    ] & iSrcOffsetMask)) )\n"
        "bHasValid = TRUE;\n"
"#endif\n"
    
    "if (!bHasValid)\n"
        "return FALSE;\n"
    
    "const sampler_t samp =  CLK_NORMALIZED_COORDS_FALSE |\n"
                            "CLK_ADDRESS_CLAMP_TO_EDGE |\n"
                            "CLK_FILTER_NEAREST;\n"
    
"#ifdef USE_VEC\n"
    "(*fReal) = read_imagef(srcReal, samp, iSrc);\n"
    "if (useImag)\n"
        "(*fImag) = read_imagef(srcImag, samp, iSrc);\n"
"#else\n"
    "(*fReal) = read_imagef(srcReal, samp, iSrc).x;\n"
    "if (useImag)\n"
        "(*fImag) = read_imagef(srcImag, samp, iSrc).x;\n"
"#endif\n"
    
    "if (useUnifiedSrcDensity) {\n"
        "(*fDensity) = fUnifiedSrcDensity[iSrcOffset];\n"
    "} else {\n"
        "(*fDensity) = 1.0f;\n"
        "return TRUE;\n"
    "}\n"
    
"#ifdef USE_VEC\n"
    "return  (*fDensity).x > 0.0000001f || (*fDensity).y > 0.0000001f ||\n"
            "(*fDensity).z > 0.0000001f || (*fDensity).w > 0.0000001f;\n"
"#else\n"
    "return (*fDensity) > 0.0000001f;\n"
"#endif\n"
"}\n"

"int isValid(__global float *fUnifiedSrcDensity,\n"
            "__global int *nUnifiedSrcValid,\n"
            "float2 fSrcCoords )\n"
"{\n"
    "if (fSrcCoords.x < 0.0f || fSrcCoords.y < 0.0f)\n"
        "return FALSE;\n"
    
    "int iSrcX = (int) (fSrcCoords.x - 0.5f);\n"
    "int iSrcY = (int) (fSrcCoords.y - 0.5f);\n"
    
    "if( iSrcX < 0 || iSrcX >= iSrcWidth || iSrcY < 0 || iSrcY >= iSrcHeight )\n"
        "return FALSE;\n"
    
    "int iSrcOffset = iSrcX + iSrcY * iSrcWidth;\n"
    
    "if (useUnifiedSrcDensity && fUnifiedSrcDensity[iSrcOffset] < 0.00001f)\n"
        "return FALSE;\n"
    
    "if (useUnifiedSrcValid &&\n"
        "!(nUnifiedSrcValid[iSrcOffset>>5] & (0x01 << (iSrcOffset & 0x1f))) )\n"
        "return FALSE;\n"
    
    "return TRUE;\n"
"}\n"

"float2 getSrcCoords(__read_only image2d_t srcCoords)\n"
"{\n"
    // Find an appropriate place to sample the coordinates so we're still
    // accurate after linear interpolation.
    "int nDstX = get_global_id(0);\n"
    "int nDstY = get_global_id(1);\n"
    "float2  fDst = (float2)((0.5f * (float)iCoordMult + nDstX) /\n"
                                "(float)((ceil((iDstWidth  - 1) / (float)iCoordMult) + 1) * iCoordMult), \n"
                            "(0.5f * (float)iCoordMult + nDstY) /\n"
                                "(float)((ceil((iDstHeight - 1) / (float)iCoordMult) + 1) * iCoordMult));\n"
    
    // Check & return when the thread group overruns the image size
    "if (nDstX >= iDstWidth || nDstY >= iDstHeight)\n"
        "return (float2)(-99.0f, -99.0f);\n"

    "const sampler_t samp =  CLK_NORMALIZED_COORDS_TRUE |\n"
                            "CLK_ADDRESS_CLAMP_TO_EDGE |\n"
                            "CLK_FILTER_LINEAR;\n"

    "float4  fSrcCoords = read_imagef(srcCoords,samp,fDst);\n"
    
    "return (float2)(fSrcCoords.x, fSrcCoords.y);\n"
"}\n";
    
    const char *kernBilinear =
// ************************ Bilinear ************************
"__kernel void resamp(__read_only image2d_t srcCoords,\n"
                    "__read_only image2d_t srcReal,\n"
                    "__read_only image2d_t srcImag,\n"
                    "__global float *fUnifiedSrcDensity,\n"
                    "__global int *nUnifiedSrcValid,\n"
                    "__constant char *useBandSrcValid,\n"
                    "__global int *nBandSrcValid,\n"
                    "__global outType *dstReal,\n"
                    "__global outType *dstImag,\n"
                    "__constant float *fDstNoDataReal,\n"
                    "__global float *dstDensity,\n"
                    "__global int *nDstValid,\n"
                    "const int bandNum)\n"
"{\n"
    "int i;\n"
    "float2  fSrc = getSrcCoords(srcCoords);\n"
    "if (!isValid(fUnifiedSrcDensity, nUnifiedSrcValid, fSrc))\n"
        "return;\n"

    "int     iSrcX = (int) floor(fSrc.x - 0.5f);\n"
    "int     iSrcY = (int) floor(fSrc.y - 0.5f);\n"
    "float   fRatioX = 1.5f - (fSrc.x - iSrcX);\n"
    "float   fRatioY = 1.5f - (fSrc.y - iSrcY);\n"
    "vecf    fReal, fImag, fDens;\n"
    "vecf    fAccumulatorReal = 0.0f, fAccumulatorImag = 0.0f;\n"
    "vecf    fAccumulatorDensity = 0.0f;\n"
    "float   fAccumulatorDivisor = 0.0f;\n"
    
    "if ( iSrcY >= 0 && iSrcY < iSrcHeight ) {\n"
        "float fMult1 = fRatioX * fRatioY;\n"
        "float fMult2 = (1.0f-fRatioX) * fRatioY;\n"
    
		// Upper Left Pixel
		"if ( iSrcX >= 0 && iSrcX < iSrcWidth\n"
			 "&& getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
						"useBandSrcValid, nBandSrcValid, (int2)(iSrcX, iSrcY),\n"
						"bandNum, &fDens, &fReal, &fImag))\n"
		"{\n"
			"fAccumulatorDivisor += fMult1;\n"
			"fAccumulatorReal += fReal * fMult1;\n"
			"fAccumulatorImag += fImag * fMult1;\n"
			"fAccumulatorDensity += fDens * fMult1;\n"
		"}\n"
	
		// Upper Right Pixel
		"if ( iSrcX+1 >= 0 && iSrcX+1 < iSrcWidth\n"
			"&& getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
						"useBandSrcValid, nBandSrcValid, (int2)(iSrcX+1, iSrcY),\n"
						"bandNum, &fDens, &fReal, &fImag))\n"
		"{\n"
			"fAccumulatorDivisor += fMult2;\n"
			"fAccumulatorReal += fReal * fMult2;\n"
			"fAccumulatorImag += fImag * fMult2;\n"
			"fAccumulatorDensity += fDens * fMult2;\n"
		"}\n"
    "}\n"
    
    "if ( iSrcY+1 >= 0 && iSrcY+1 < iSrcHeight ) {\n"
        "float fMult1 = fRatioX * (1.0f-fRatioY);\n"
        "float fMult2 = (1.0f-fRatioX) * (1.0f-fRatioY);\n"
    
        // Lower Left Pixel
		"if ( iSrcX >= 0 && iSrcX < iSrcWidth\n"
			"&& getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
						"useBandSrcValid, nBandSrcValid, (int2)(iSrcX, iSrcY+1),\n"
						"bandNum, &fDens, &fReal, &fImag))\n"
		"{\n"
			"fAccumulatorDivisor += fMult1;\n"
			"fAccumulatorReal += fReal * fMult1;\n"
			"fAccumulatorImag += fImag * fMult1;\n"
			"fAccumulatorDensity += fDens * fMult1;\n"
		"}\n"
	
		// Lower Right Pixel
		"if ( iSrcX+1 >= 0 && iSrcX+1 < iSrcWidth\n"
			"&& getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
						"useBandSrcValid, nBandSrcValid, (int2)(iSrcX+1, iSrcY+1),\n"
						"bandNum, &fDens, &fReal, &fImag))\n"
		"{\n"
			"fAccumulatorDivisor += fMult2;\n"
			"fAccumulatorReal += fReal * fMult2;\n"
			"fAccumulatorImag += fImag * fMult2;\n"
			"fAccumulatorDensity += fDens * fMult2;\n"
		"}\n"
    "}\n"
    
    // Compute and save final pixel
    "if ( fAccumulatorDivisor < 0.00001f ) {\n"
        "setPixel(dstReal, dstImag, dstDensity, nDstValid, fDstNoDataReal, bandNum,\n"
                "0.0f, 0.0f, 0.0f );\n"
    "} else if ( fAccumulatorDivisor < 0.99999f || fAccumulatorDivisor > 1.00001f ) {\n"
        "setPixel(dstReal, dstImag, dstDensity, nDstValid, fDstNoDataReal, bandNum,\n"
                "fAccumulatorDensity / fAccumulatorDivisor,\n"
                "fAccumulatorReal / fAccumulatorDivisor,\n"
"#if useImag != 0\n"
                "fAccumulatorImag / fAccumulatorDivisor );\n"
"#else\n"
                "0.0f );\n"
"#endif\n"
    "} else {\n"
        "setPixel(dstReal, dstImag, dstDensity, nDstValid, fDstNoDataReal, bandNum,\n"
                "fAccumulatorDensity, fAccumulatorReal, fAccumulatorImag );\n"
    "}\n"
"}\n";
    
    const char *kernCubic =
// ************************ Cubic ************************
"vecf cubicConvolution(float dist1, float dist2, float dist3,\n"
                       "vecf f0, vecf f1, vecf f2, vecf f3)\n"
"{\n"
    "return   (  -f0 +    f1  - f2 + f3) * dist3\n"
           "+ (2.0f*(f0 - f1) + f2 - f3) * dist2\n"
           "+ (  -f0          + f2     ) * dist1\n"
           "+             f1;\n"
"}\n"

// ************************ Cubic ************************
"__kernel void resamp(__read_only image2d_t srcCoords,\n"
                     "__read_only image2d_t srcReal,\n"
                     "__read_only image2d_t srcImag,\n"
                     "__global float *fUnifiedSrcDensity,\n"
                     "__global int *nUnifiedSrcValid,\n"
                     "__constant char *useBandSrcValid,\n"
                     "__global int *nBandSrcValid,\n"
                     "__global outType *dstReal,\n"
                     "__global outType *dstImag,\n"
                     "__constant float *fDstNoDataReal,\n"
                     "__global float *dstDensity,\n"
                     "__global int *nDstValid,\n"
                     "const int bandNum)\n"
"{\n"
    "int i;\n"
    "float2  fSrc = getSrcCoords(srcCoords);\n"
    
    "if (!isValid(fUnifiedSrcDensity, nUnifiedSrcValid, fSrc))\n"
        "return;\n"
    
    "int     iSrcX = (int) floor( fSrc.x - 0.5f );\n"
    "int     iSrcY = (int) floor( fSrc.y - 0.5f );\n"
    "float   fDeltaX = fSrc.x - 0.5f - (float)iSrcX;\n"
    "float   fDeltaY = fSrc.y - 0.5f - (float)iSrcY;\n"
    "float   fDeltaX2 = fDeltaX * fDeltaX;\n"
    "float   fDeltaY2 = fDeltaY * fDeltaY;\n"
    "float   fDeltaX3 = fDeltaX2 * fDeltaX;\n"
    "float   fDeltaY3 = fDeltaY2 * fDeltaY;\n"
    "vecf    afReal[4], afDens[4];\n"
"#if useImag != 0\n"
    "vecf    afImag[4];\n"
"#else\n"
    "vecf    fImag = 0.0f;\n"
"#endif\n"
    
    // Loop over rows
    "for (i = -1; i < 3; ++i)\n"
    "{\n"
        "vecf    fReal1 = 0.0f, fReal2 = 0.0f, fReal3 = 0.0f, fReal4 = 0.0f;\n"
        "vecf    fDens1 = 0.0f, fDens2 = 0.0f, fDens3 = 0.0f, fDens4 = 0.0f;\n"
        "int hasPx;\n"
"#if useImag != 0\n"
        "vecf    fImag1 = 0.0f, fImag2 = 0.0f, fImag3 = 0.0f, fImag4 = 0.0f;\n"
        
        //Get all the pixels for this row
        "hasPx  = getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                        "useBandSrcValid, nBandSrcValid, (int2)(iSrcX-1, iSrcY+i),\n"
                        "bandNum, &fDens1, &fReal1, &fImag1);\n"
        
        "hasPx |= getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                        "useBandSrcValid, nBandSrcValid, (int2)(iSrcX  , iSrcY+i),\n"
                        "bandNum, &fDens2, &fReal2, &fImag2);\n"
        
        "hasPx |= getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                        "useBandSrcValid, nBandSrcValid, (int2)(iSrcX+1, iSrcY+i),\n"
                        "bandNum, &fDens3, &fReal3, &fImag3);\n"
        
        "hasPx |= getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                        "useBandSrcValid, nBandSrcValid, (int2)(iSrcX+2, iSrcY+i),\n"
                        "bandNum, &fDens4, &fReal4, &fImag4);\n"
"#else\n"
        //Get all the pixels for this row
        "hasPx  = getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                "useBandSrcValid, nBandSrcValid, (int2)(iSrcX-1, iSrcY+i),\n"
                "bandNum, &fDens1, &fReal1, &fImag);\n"
        
        "hasPx |= getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                "useBandSrcValid, nBandSrcValid, (int2)(iSrcX  , iSrcY+i),\n"
                "bandNum, &fDens2, &fReal2, &fImag);\n"
        
        "hasPx |= getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                "useBandSrcValid, nBandSrcValid, (int2)(iSrcX+1, iSrcY+i),\n"
                "bandNum, &fDens3, &fReal3, &fImag);\n"
        
        "hasPx |= getPixel(srcReal, srcImag, fUnifiedSrcDensity, nUnifiedSrcValid,\n"
                "useBandSrcValid, nBandSrcValid, (int2)(iSrcX+2, iSrcY+i),\n"
                "bandNum, &fDens4, &fReal4, &fImag);\n"
"#endif\n"
        
        // Shortcut if no px
        "if (!hasPx) {\n"
            "afDens[i+1] = 0.0f;\n"
            "afReal[i+1] = 0.0f;\n"
"#if useImag != 0\n"
            "afImag[i+1] = 0.0f;\n"
"#endif\n"
            "continue;\n"
        "}\n"
        
        // Process this row
        "afDens[i+1] = cubicConvolution(fDeltaX, fDeltaX2, fDeltaX3, fDens1, fDens2, fDens3, fDens4);\n"
        "afReal[i+1] = cubicConvolution(fDeltaX, fDeltaX2, fDeltaX3, fReal1, fReal2, fReal3, fReal4);\n"
"#if useImag != 0\n"
        "afImag[i+1] = cubicConvolution(fDeltaX, fDeltaX2, fDeltaX3, fImag1, fImag2, fImag3, fImag4);\n"
"#endif\n"
    "}\n"
    
    // Compute and save final pixel
    "setPixel(dstReal, dstImag, dstDensity, nDstValid, fDstNoDataReal, bandNum,\n"
             "cubicConvolution(fDeltaY, fDeltaY2, fDeltaY3, afDens[0], afDens[1], afDens[2], afDens[3]),\n"
             "cubicConvolution(fDeltaY, fDeltaY2, fDeltaY3, afReal[0], afReal[1], afReal[2], afReal[3]),\n"
"#if useImag != 0\n"
             "cubicConvolution(fDeltaY, fDeltaY2, fDeltaY3, afImag[0], afImag[1], afImag[2], afImag[3]) );\n"
"#else\n"
             "fImag );\n"
"#endif\n"
"}\n";

    const char *kernResampler =
// ************************ LanczosSinc ************************

"float lanczosSinc( float fX, float fR )\n"
"{\n"
    "if ( fX > fR || fX < -fR)\n"
        "return 0.0f;\n"
    "if ( fX == 0.0f )\n"
        "return 1.0f;\n"
    
    "float fPIX = PI * fX;\n"
    "return ( sin(fPIX) / fPIX ) * ( sin(fPIX / fR) * fR / fPIX );\n"
"}\n"

// ************************ Bicubic Spline ************************

"float bSpline( float x )\n"
"{\n"
    "float xp2 = x + 2.0f;\n"
    "float xp1 = x + 1.0f;\n"
    "float xm1 = x - 1.0f;\n"
    "float xp2c = xp2 * xp2 * xp2;\n"
    
    "return (((xp2 > 0.0f)?((xp1 > 0.0f)?((x > 0.0f)?((xm1 > 0.0f)?\n"
                                                     "-4.0f * xm1*xm1*xm1:0.0f) +\n"
                                         "6.0f * x*x*x:0.0f) +\n"
                           "-4.0f * xp1*xp1*xp1:0.0f) +\n"
             "xp2c:0.0f) ) * 0.166666666666666666666f;\n"
"}\n"

// ************************ General Resampler ************************

"__kernel void resamp(__read_only image2d_t srcCoords,\n"
                     "__read_only image2d_t srcReal,\n"
                     "__read_only image2d_t srcImag,\n"
                     "__global float *fUnifiedSrcDensity,\n"
                     "__global int *nUnifiedSrcValid,\n"
                     "__constant char *useBandSrcValid,\n"
                     "__global int *nBandSrcValid,\n"
                     "__global outType *dstReal,\n"
                     "__global outType *dstImag,\n"
                     "__constant float *fDstNoDataReal,\n"
                     "__global float *dstDensity,\n"
                     "__global int *nDstValid,\n"
                     "const int bandNum)\n"
"{\n"
    "float2  fSrc = getSrcCoords(srcCoords);\n"
    
    "if (!isValid(fUnifiedSrcDensity, nUnifiedSrcValid, fSrc))\n"
        "return;\n"
    
    "int     iSrcX = floor( fSrc.x - 0.5f );\n"
    "int     iSrcY = floor( fSrc.y - 0.5f );\n"
    "float   fDeltaX = fSrc.x - 0.5f - (float)iSrcX;\n"
    "float   fDeltaY = fSrc.y - 0.5f - (float)iSrcY;\n"
    
    "vecf  fAccumulatorReal = 0.0f, fAccumulatorImag = 0.0f;\n"
    "vecf  fAccumulatorDensity = 0.0f;\n"
    "float fAccumulatorWeight = 0.0f;\n"
    "int   i, j;\n"

    // Loop over pixel rows in the kernel
    "for ( j = nFiltInitY; j <= nYRadius; ++j )\n"
    "{\n"
        "float   fWeight1;\n"
        "int2 iSrc = (int2)(0, iSrcY + j);\n"
        
        // Skip sampling over edge of image
        "if ( iSrc.y < 0 || iSrc.y >= iSrcHeight )\n"
            "continue;\n"
    
        // Select the resampling algorithm
        "if ( doCubicSpline )\n"
            // Calculate the Y weight
            "fWeight1 = ( fYScale < 1.0f ) ?\n"
                "bSpline(((float)j) * fYScale) * fYScale :\n"
                "bSpline(((float)j) - fDeltaY);\n"
        "else\n"
            "fWeight1 = ( fYScale < 1.0f ) ?\n"
                "lanczosSinc(j * fYScale, fYFilter) * fYScale :\n"
                "lanczosSinc(j - fDeltaY, fYFilter);\n"
        
        // Iterate over pixels in row
        "for ( i = nFiltInitX; i <= nXRadius; ++i )\n"
        "{\n"
            "float fWeight2;\n"
            "vecf fDensity = 0.0f, fReal, fImag;\n"
            "iSrc.x = iSrcX + i;\n"
            
            // Skip sampling at edge of image
            // Skip sampling when invalid pixel
            "if ( iSrc.x < 0 || iSrc.x >= iSrcWidth || \n"
                  "!getPixel(srcReal, srcImag, fUnifiedSrcDensity,\n"
                            "nUnifiedSrcValid, useBandSrcValid, nBandSrcValid,\n"
                            "iSrc, bandNum, &fDensity, &fReal, &fImag) )\n"
                "continue;\n"
    
            // Choose among possible algorithms
            "if ( doCubicSpline )\n"
                // Calculate & save the X weight
                "fWeight2 = fWeight1 * ((fXScale < 1.0f ) ?\n"
                    "bSpline((float)i * fXScale) * fXScale :\n"
                    "bSpline(fDeltaX - (float)i));\n"
            "else\n"
                // Calculate & save the X weight
                "fWeight2 = fWeight1 * ((fXScale < 1.0f ) ?\n"
                    "lanczosSinc(i * fXScale, fXFilter) * fXScale :\n"
                    "lanczosSinc(i - fDeltaX, fXFilter));\n"
            
            // Accumulate!
            "fAccumulatorReal += fReal * fWeight2;\n"
            "fAccumulatorImag += fImag * fWeight2;\n"
            "fAccumulatorDensity += fDensity * fWeight2;\n"
            "fAccumulatorWeight += fWeight2;\n"
        "}\n"
    "}\n"

    "if ( fAccumulatorWeight < 0.000001f ) {\n"
        "setPixel(dstReal, dstImag, dstDensity, nDstValid, fDstNoDataReal, bandNum,\n"
                 "0.0f, 0.0f, 0.0f);\n"
    "} else if ( fAccumulatorWeight < 0.99999f || fAccumulatorWeight > 1.00001f ) {\n"
        // Calculate the output taking into account weighting
        "setPixel(dstReal, dstImag, dstDensity, nDstValid, fDstNoDataReal, bandNum,\n"
                 "fAccumulatorDensity / fAccumulatorWeight,\n"
                 "fAccumulatorReal / fAccumulatorWeight,\n"
"#if useImag != 0\n"
                 "fAccumulatorImag / fAccumulatorWeight );\n"
"#else\n"
                 "0.0f );\n"
"#endif\n"
    "} else {\n"
        "setPixel(dstReal, dstImag, dstDensity, nDstValid, fDstNoDataReal, bandNum,\n"
                 "fAccumulatorDensity, fAccumulatorReal, fAccumulatorImag);\n"
    "}\n"
"}\n";
    
    //Defines based on image format
    switch (warper->imageFormat) {
        case CL_FLOAT:
            dstMinVal = -FLT_MAX;
            dstMaxVal = FLT_MAX;
            outType = "float";
            break;
        case CL_SNORM_INT8:
            dstMinVal = -128.0;
            dstMaxVal = 127.0;
            outType = "char";
            break;
        case CL_UNORM_INT8:
            dstMinVal = 0.0;
            dstMaxVal = 255.0;
            outType = "uchar";
            break;
        case CL_SNORM_INT16:
            dstMinVal = -32768.0;
            dstMaxVal = 32767.0;
            outType = "short";
            break;
        case CL_UNORM_INT16:
            dstMinVal = 0.0;
            dstMaxVal = 65535.0;
            outType = "ushort";
            break;
    }
    
    //Use vector format?
    if (useVec) {
        dUseVec = "-D USE_VEC";
        dVecf = "float4";
    }
    
    //Assemble the kernel from parts. The compiler is unable to handle multiple
    //kernels in one string with more than a few __constant modifiers each.
    if (warper->resampAlg == OCL_Bilinear)
        sprintf(progBuf, "%s\n%s", kernGenFuncs, kernBilinear);
    else if (warper->resampAlg == OCL_Cubic)
        sprintf(progBuf, "%s\n%s", kernGenFuncs, kernCubic);
    else
        sprintf(progBuf, "%s\n%s", kernGenFuncs, kernResampler);
    
    //Actually make the program from assembled source
    program = clCreateProgramWithSource(warper->context, 1, (const char**)&progBuf,
                                        NULL, &err);
    handleErrGoto(err, error_final);
    
    //Assemble the compiler arg string for speed. All invariants should be defined here.
    sprintf(buffer, "-cl-fast-relaxed-math -Werror -D FALSE=0 -D TRUE=1 "
            "%s"
            "-D iSrcWidth=%d -D iSrcHeight=%d -D iDstWidth=%d -D iDstHeight=%d "
            "-D useUnifiedSrcDensity=%d -D useUnifiedSrcValid=%d "
            "-D useDstDensity=%d -D useDstValid=%d -D useImag=%d "
            "-D fXScale=%015.15lff -D fYScale=%015.15lff -D fXFilter=%015.15lff -D fYFilter=%015.15lff "
            "-D nXRadius=%d -D nYRadius=%d -D nFiltInitX=%d -D nFiltInitY=%d "
            "-D PI=%015.15lff -D outType=%s -D dstMinVal=%015.15lff -D dstMaxVal=%015.15lff "
            "-D useDstNoDataReal=%d -D vecf=%s %s -D doCubicSpline=%d "
            "-D useUseBandSrcValid=%d -D iCoordMult=%d ",
            /* FIXME: Is it really a ATI specific thing ? */
            (warper->imageFormat == CL_FLOAT && (warper->eCLVendor == VENDOR_AMD || warper->eCLVendor == VENDOR_INTEL)) ? "-D USE_CLAMP_TO_DST_FLOAT=1 " : "",
            warper->srcWidth, warper->srcHeight, warper->dstWidth, warper->dstHeight,
            warper->useUnifiedSrcDensity, warper->useUnifiedSrcValid,
            warper->useDstDensity, warper->useDstValid, warper->imagWorkCL != NULL,
            dfXScale, dfYScale, dfXFilter, dfYFilter,
            nXRadius, nYRadius, nFiltInitX, nFiltInitY,
            M_PI, outType, dstMinVal, dstMaxVal, warper->fDstNoDataRealCL != NULL,
            dVecf, dUseVec, warper->resampAlg == OCL_CubicSpline,
            warper->nBandSrcValidCL != NULL, warper->coordMult);

    (*clErr) = err = clBuildProgram(program, 1, &(warper->dev), buffer, NULL, NULL);
    
    //Detailed debugging info
    if (err != CL_SUCCESS)
    {
        const char* pszStatus = "unknown_status";
        err = clGetProgramBuildInfo(program, warper->dev, CL_PROGRAM_BUILD_LOG,
                                    128000*sizeof(char), buffer, NULL);
        handleErrGoto(err, error_free_program);
        
        CPLError(CE_Failure, CPLE_AppDefined, "Error: Failed to build program executable!\nBuild Log:\n%s", buffer);

        err = clGetProgramBuildInfo(program, warper->dev, CL_PROGRAM_BUILD_STATUS,
                                    128000*sizeof(char), buffer, NULL);
        handleErrGoto(err, error_free_program);

        if(buffer[0] == CL_BUILD_NONE)
            pszStatus = "CL_BUILD_NONE";
        else if(buffer[0] == CL_BUILD_ERROR)
            pszStatus = "CL_BUILD_ERROR";
        else if(buffer[0] == CL_BUILD_SUCCESS)
            pszStatus = "CL_BUILD_SUCCESS";
        else if(buffer[0] == CL_BUILD_IN_PROGRESS)
            pszStatus = "CL_BUILD_IN_PROGRESS";

        CPLDebug("OpenCL", "Build Status: %s\nProgram Source:\n%s", pszStatus, progBuf);
        goto error_free_program;
    }
    
    kernel = clCreateKernel(program, "resamp", &err);
    handleErrGoto(err, error_free_program);
    
    err = clReleaseProgram(program);
    handleErrGoto(err, error_final);
    
    CPLFree(buffer);
    CPLFree(progBuf);
    return kernel;

error_free_program:
    err = clReleaseProgram(program);

error_final:
    CPLFree(buffer);
    CPLFree(progBuf);
    return NULL;
}

/*
 Alloc & copy the coordinate data from host working memory to the device. The
 working memory should be a pinned, linear, array of floats. This allows us to
 allocate and copy all data in one step. The pointer to the device memory is
 saved and set as the appropriate argument number.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int set_coord_data (struct oclWarper *warper, cl_mem *xy)
{
    cl_int err = CL_SUCCESS;
    cl_image_format imgFmt;
    
    //Copy coord data to the device
    imgFmt.image_channel_order = warper->xyChOrder;
    imgFmt.image_channel_data_type = CL_FLOAT;
    (*xy) = clCreateImage2D(warper->context,
                            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &imgFmt,
                            (size_t) warper->xyWidth,
                            (size_t) warper->xyHeight,
                            (size_t) sizeof(float) * warper->xyChSize * warper->xyWidth,
                            warper->xyWork, &err);
    handleErr(err);
    
    //Free the source memory, now that it's copied we don't need it
    freeCLMem(warper->xyWorkCL, warper->xyWork);
    
    //Set up argument
    if (warper->kern1 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern1, 0, sizeof(cl_mem), xy));
    }
    if (warper->kern4 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern4, 0, sizeof(cl_mem), xy));
    }
    
    return CL_SUCCESS;
}

/*
 Sets the unified density & valid data structures. These are optional structures
 from GDAL, and as such if they are NULL a small placeholder memory segment is
 defined. This is because the spec is unclear on if a NULL value can be passed
 as a kernel argument in place of memory. If it's not NULL, the data is copied
 from the working memory to the device memory. After that, we check if we are
 using the per-band validity mask, and set that as appropriate. At the end, the
 CL mem is passed as the kernel arguments.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int set_unified_data(struct oclWarper *warper,
                        cl_mem *unifiedSrcDensityCL, cl_mem *unifiedSrcValidCL,
                        float *unifiedSrcDensity, unsigned int *unifiedSrcValid,
                        cl_mem *useBandSrcValidCL, cl_mem *nBandSrcValidCL)
{
    cl_int err = CL_SUCCESS;
    size_t sz = warper->srcWidth * warper->srcHeight;
    int useValid = warper->nBandSrcValidCL != NULL;
    //32 bits in the mask
    int validSz = sizeof(int) * (1 + (sz >> 5));
    
    //Copy unifiedSrcDensity if it exists
    if (unifiedSrcDensity == NULL) {
        //Alloc dummy device RAM
        (*unifiedSrcDensityCL) = clCreateBuffer(warper->context, CL_MEM_READ_ONLY, 1, NULL, &err);
        handleErr(err);
    } else {
        //Alloc & copy all density data
        (*unifiedSrcDensityCL) = clCreateBuffer(warper->context,
                                                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                sizeof(float) * sz, unifiedSrcDensity, &err);
        handleErr(err);
    }
    
    //Copy unifiedSrcValid if it exists
    if (unifiedSrcValid == NULL) {
        //Alloc dummy device RAM
        (*unifiedSrcValidCL) = clCreateBuffer(warper->context, CL_MEM_READ_ONLY, 1, NULL, &err);
        handleErr(err);
    } else {
        //Alloc & copy all validity data
        (*unifiedSrcValidCL) = clCreateBuffer(warper->context,
                                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                              validSz, unifiedSrcValid, &err);
        handleErr(err);
    }
    
    // Set the band validity usage
    if(useValid) {
        (*useBandSrcValidCL) = clCreateBuffer(warper->context,
                                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                              sizeof(char) * warper->numBands,
                                              warper->useBandSrcValid, &err);
        handleErr(err);
    } else {
        //Make a fake image so we don't have a NULL pointer
        (*useBandSrcValidCL) = clCreateBuffer(warper->context, CL_MEM_READ_ONLY, 1, NULL, &err);
        handleErr(err);
    }
    
    //Do a more thorough check for validity
    if (useValid) {
        int i;
        useValid = FALSE;
        for (i = 0; i < warper->numBands; ++i)
            if (warper->useBandSrcValid[i])
                useValid = TRUE;
    }
    
    //And the validity mask if needed
    if (useValid) {
        (*nBandSrcValidCL) = clCreateBuffer(warper->context,
                                            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                            warper->numBands * validSz,
                                            warper->nBandSrcValid, &err);
        handleErr(err);
    } else {
        //Make a fake image so we don't have a NULL pointer
        (*nBandSrcValidCL) = clCreateBuffer(warper->context, CL_MEM_READ_ONLY, 1, NULL, &err);
        handleErr(err);
    }

    //Set up arguments
    if (warper->kern1 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern1, 3, sizeof(cl_mem), unifiedSrcDensityCL));
        handleErr(err = clSetKernelArg(warper->kern1, 4, sizeof(cl_mem), unifiedSrcValidCL));
        handleErr(err = clSetKernelArg(warper->kern1, 5, sizeof(cl_mem), useBandSrcValidCL));
        handleErr(err = clSetKernelArg(warper->kern1, 6, sizeof(cl_mem), nBandSrcValidCL));
    }
    if (warper->kern4 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern4, 3, sizeof(cl_mem), unifiedSrcDensityCL));
        handleErr(err = clSetKernelArg(warper->kern4, 4, sizeof(cl_mem), unifiedSrcValidCL));
        handleErr(err = clSetKernelArg(warper->kern4, 5, sizeof(cl_mem), useBandSrcValidCL));
        handleErr(err = clSetKernelArg(warper->kern4, 6, sizeof(cl_mem), nBandSrcValidCL));
    }
    
    return CL_SUCCESS;
}

/*
 Here we set the per-band raster data. First priority is the real raster data,
 of course. Then, if applicable, we set the additional image channel. Once this
 data is copied to the device, it can be freed on the host, so that is done
 here. Finally the appropriate kernel arguments are set.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int set_src_rast_data (struct oclWarper *warper, int iNum, size_t sz,
                          cl_channel_order chOrder, cl_mem *srcReal, cl_mem *srcImag)
{
    cl_image_format imgFmt;
    cl_int err = CL_SUCCESS;
    int useImagWork = warper->imagWork.v != NULL && warper->imagWork.v[iNum] != NULL;
    
    //Set up image vars
    imgFmt.image_channel_order = chOrder;
    imgFmt.image_channel_data_type = warper->imageFormat;

    //Create & copy the source image
    (*srcReal) = clCreateImage2D(warper->context,
                                 CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &imgFmt,
                                 (size_t) warper->srcWidth, (size_t) warper->srcHeight,
                                 sz * warper->srcWidth, warper->realWork.v[iNum], &err);
    handleErr(err);
    
    //And the source image parts if needed
    if (useImagWork) {
        (*srcImag) = clCreateImage2D(warper->context,
                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &imgFmt,
                                     (size_t) warper->srcWidth, (size_t) warper->srcHeight,
                                     sz * warper->srcWidth, warper->imagWork.v[iNum], &err);
        handleErr(err);
    } else {
        //Make a fake image so we don't have a NULL pointer
        if (warper->eCLVendor == VENDOR_AMD || warper->eCLVendor == VENDOR_INTEL)
        {
            /* The code in the else clause generates a CL_INVALID_IMAGE_SIZE with ATI SDK 2.2 */
            /* while theoretically correct and working on other SDKs. The following is a */
            /* workaround */
            char dummyImageData[16];
            (*srcImag) = clCreateImage2D(warper->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &imgFmt,
                                        1, 1, sz, dummyImageData, &err);
        }
        else
        {
            (*srcImag) = clCreateImage2D(warper->context, CL_MEM_READ_ONLY, &imgFmt,
                                         1, 1, sz, NULL, &err);
        }
        handleErr(err);
    }

    //Free the source memory, now that it's copied we don't need it
    freeCLMem(warper->realWorkCL[iNum], warper->realWork.v[iNum]);
    if (warper->imagWork.v != NULL) {
        freeCLMem(warper->imagWorkCL[iNum], warper->imagWork.v[iNum]);
    }
    
    //Set up per-band arguments
    if (warper->kern1 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern1, 1, sizeof(cl_mem), srcReal));
        handleErr(err = clSetKernelArg(warper->kern1, 2, sizeof(cl_mem), srcImag));
    }
    if (warper->kern4 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern4, 1, sizeof(cl_mem), srcReal));
        handleErr(err = clSetKernelArg(warper->kern4, 2, sizeof(cl_mem), srcImag));
    }
    
    return CL_SUCCESS;
}

/*
 Set the destination data for the raster. Although it's the output, it still
 is copied to the device because some blending is done there. First the real
 data is allocated and copied, then the imag data is allocated and copied if
 needed. They are then set as the appropriate arguments to the kernel.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int set_dst_rast_data(struct oclWarper *warper, int iImg, size_t sz,
                         cl_mem *dstReal, cl_mem *dstImag)
{
    cl_int err = CL_SUCCESS;
    sz *= warper->dstWidth * warper->dstHeight;
    
    //Copy the dst real data
    (*dstReal) = clCreateBuffer(warper->context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                sz, warper->dstRealWork.v[iImg], &err);
    handleErr(err);
    
    //Copy the dst imag data if exists
    if (warper->dstImagWork.v != NULL && warper->dstImagWork.v[iImg] != NULL) {
        (*dstImag) = clCreateBuffer(warper->context,
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sz, warper->dstImagWork.v[iImg], &err);
        handleErr(err);
    } else {
        (*dstImag) = clCreateBuffer(warper->context, CL_MEM_READ_WRITE, 1, NULL, &err);
        handleErr(err);
    }
    
    //Set up per-band arguments
    if (warper->kern1 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern1, 7, sizeof(cl_mem), dstReal));
        handleErr(err = clSetKernelArg(warper->kern1, 8, sizeof(cl_mem), dstImag));
    }
    if (warper->kern4 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern4, 7, sizeof(cl_mem), dstReal));
        handleErr(err = clSetKernelArg(warper->kern4, 8, sizeof(cl_mem), dstImag));
    }
    
    return CL_SUCCESS;
}

/*
 Read the final raster data back from the graphics card to working memory. This
 copies both the real memory and the imag memory if appropriate.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int get_dst_rast_data(struct oclWarper *warper, int iImg, size_t wordSz,
                         cl_mem dstReal, cl_mem dstImag)
{
    cl_int err = CL_SUCCESS;
    size_t sz = warper->dstWidth * warper->dstHeight * wordSz;
    
    //Copy from dev into working memory
    handleErr(err = clEnqueueReadBuffer(warper->queue, dstReal,
                                        CL_FALSE, 0, sz, warper->dstRealWork.v[iImg],
                                        0, NULL, NULL));
    
    //If we are expecting the imag channel, then copy it back also
    if (warper->dstImagWork.v != NULL && warper->dstImagWork.v[iImg] != NULL) {
        handleErr(err = clEnqueueReadBuffer(warper->queue, dstImag,
                                            CL_FALSE, 0, sz, warper->dstImagWork.v[iImg],
                                            0, NULL, NULL));
    }
    
    //The copy requests were non-blocking, so we'll need to make sure they finish.
    handleErr(err = clFinish(warper->queue));
    
    return CL_SUCCESS;
}

/*
 Set the destination image density & validity mask on the device. This is used
 to blend the final output image with the existing buffer. This handles the
 unified structures that apply to all bands. After the buffers are created and
 copied, they are set as kernel arguments.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int set_dst_data(struct oclWarper *warper,
                    cl_mem *dstDensityCL, cl_mem *dstValidCL, cl_mem *dstNoDataRealCL,
                    float *dstDensity, unsigned int *dstValid, float *dstNoDataReal)
{
    cl_int err = CL_SUCCESS;
    size_t sz = warper->dstWidth * warper->dstHeight;
    
    //Copy the no-data value(s)
    if (dstNoDataReal == NULL) {
        (*dstNoDataRealCL) = clCreateBuffer(warper->context, CL_MEM_READ_ONLY, 1, NULL, &err);
        handleErr(err);
    } else {
        (*dstNoDataRealCL) = clCreateBuffer(warper->context,
                                         CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                         sizeof(float) * warper->numBands, dstNoDataReal, &err);
        handleErr(err);
    }
    
    //Copy unifiedSrcDensity if it exists
    if (dstDensity == NULL) {
        (*dstDensityCL) = clCreateBuffer(warper->context, CL_MEM_READ_ONLY, 1, NULL, &err);
        handleErr(err);
    } else {
        (*dstDensityCL) = clCreateBuffer(warper->context,
                                         CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                         sizeof(float) * sz, dstDensity, &err);
        handleErr(err);
    }
    
    //Copy unifiedSrcValid if it exists
    if (dstValid == NULL) {
        (*dstValidCL) = clCreateBuffer(warper->context, CL_MEM_READ_ONLY, 1, NULL, &err);
        handleErr(err);
    } else {
        (*dstValidCL) = clCreateBuffer(warper->context,
                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       sizeof(int) * ((1 + sz) >> 5), dstValid, &err);
        handleErr(err);
    }
    
    //Set up arguments
    if (warper->kern1 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern1,  9, sizeof(cl_mem), dstNoDataRealCL));
        handleErr(err = clSetKernelArg(warper->kern1, 10, sizeof(cl_mem), dstDensityCL));
        handleErr(err = clSetKernelArg(warper->kern1, 11, sizeof(cl_mem), dstValidCL));
    }
    if (warper->kern4 != NULL) {
        handleErr(err = clSetKernelArg(warper->kern4,  9, sizeof(cl_mem), dstNoDataRealCL));
        handleErr(err = clSetKernelArg(warper->kern4, 10, sizeof(cl_mem), dstDensityCL));
        handleErr(err = clSetKernelArg(warper->kern4, 11, sizeof(cl_mem), dstValidCL));
    }
    
    return CL_SUCCESS;
}

/*
 Go ahead and execute the kernel. This handles some housekeeping stuff like the
 run dimensions. When running in debug mode, it times the kernel call and prints
 the execution time.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int execute_kern(struct oclWarper *warper, cl_kernel kern, size_t loc_size)
{
    cl_int err = CL_SUCCESS;
    cl_event ev;
    size_t ceil_runs[2];
    size_t group_size[2];
#ifdef DEBUG_OPENCL
    size_t start_time = 0;
    size_t end_time;
    char *vecTxt = "";
#endif
    
    // Use a likely X-dimension which is a power of 2
    if (loc_size >= 512)
        group_size[0] = 32;
    else if (loc_size >= 64)
        group_size[0] = 16;
    else if (loc_size > 8)
        group_size[0] = 8;
    else
        group_size[0] = 1;
    
    if (group_size[0] > loc_size)
        group_size[1] = group_size[0]/loc_size;
    else
        group_size[1] = 1;
    
    //Round up num_runs to find the dim of the block of pixels we'll be processing
    if(warper->dstWidth % group_size[0])
        ceil_runs[0] = warper->dstWidth + group_size[0] - warper->dstWidth % group_size[0];
    else
        ceil_runs[0] = warper->dstWidth;
    
    if(warper->dstHeight % group_size[1])
        ceil_runs[1] = warper->dstHeight + group_size[1] - warper->dstHeight % group_size[1];
    else
        ceil_runs[1] = warper->dstHeight;
    
#ifdef DEBUG_OPENCL
    handleErr(err = clSetCommandQueueProperty(warper->queue, CL_QUEUE_PROFILING_ENABLE, CL_TRUE, NULL));
#endif
    
    // Run the calculation by enqueuing it and forcing the 
    // command queue to complete the task
    handleErr(err = clEnqueueNDRangeKernel(warper->queue, kern, 2, NULL, 
                                           ceil_runs, group_size, 0, NULL, &ev));
    handleErr(err = clFinish(warper->queue));
    
#ifdef DEBUG_OPENCL
    handleErr(err = clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_START,
                                            sizeof(size_t), &start_time, NULL));
    handleErr(err = clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_END,
                                            sizeof(size_t), &end_time, NULL));
    assert(end_time != 0);
    assert(start_time != 0);
    handleErr(err = clReleaseEvent(ev));
    if (kern == warper->kern4)
        vecTxt = "(vec)";
    
    CPLDebug("OpenCL", "Kernel Time: %6s %10lu", vecTxt, (long int)((end_time-start_time)/100000));
#endif
    return CL_SUCCESS;
}

/*
 Copy data from a raw source to the warper's working memory. If the imag
 channel is expected, then the data will be de-interlaced into component blocks
 of memory.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int set_img_data(struct oclWarper *warper, void *srcImgData,
                    unsigned int width, unsigned int height, int isSrc,
                    unsigned int bandNum, void **dstRealImgs, void **dstImagImgs)
{
    unsigned int imgChSize = warper->imgChSize1;
    unsigned int iSrcY, i;
    unsigned int vecOff = 0;
    unsigned int imgNum = bandNum;
    void *dstReal = NULL;
    void *dstImag = NULL;
    
    // Handle vector if needed
    if (warper->useVec && bandNum < warper->numBands - warper->numBands % 4) {
        imgChSize = warper->imgChSize4;
        vecOff = bandNum % 4;
        imgNum = bandNum / 4;
    } else if(warper->useVec) {
        imgNum = bandNum / 4 + bandNum % 4;
    }
    
    // Set the images as needed
    dstReal = dstRealImgs[imgNum];
    if(dstImagImgs == NULL)
        dstImag = NULL;
    else
        dstImag = dstImagImgs[imgNum];
    
    // Set stuff for dst imgs
    if (!isSrc) {
        vecOff *= height * width;
        imgChSize = 1;
    }
    
    // Copy values as needed
    if (warper->imagWorkCL == NULL && !(warper->useVec && isSrc)) {
        //Set memory size & location depending on the data type
        //This is the ideal code path for speed
        switch (warper->imageFormat) {
            case CL_UNORM_INT8:
            {
                unsigned char *realDst = &(((unsigned char *)dstReal)[vecOff]);
                memcpy(realDst, srcImgData, width*height*sizeof(unsigned char));
                break;
            }
            case CL_SNORM_INT8:
            {
                char *realDst = &(((char *)dstReal)[vecOff]);
                memcpy(realDst, srcImgData, width*height*sizeof(char));
                break;
            }
            case CL_UNORM_INT16:
            {
                unsigned short *realDst = &(((unsigned short *)dstReal)[vecOff]);
                memcpy(realDst, srcImgData, width*height*sizeof(unsigned short));
                break;
            }
            case CL_SNORM_INT16:
            {
                short *realDst = &(((short *)dstReal)[vecOff]);
                memcpy(realDst, srcImgData, width*height*sizeof(short));
                break;
            }
            case CL_FLOAT:
            {
                float *realDst = &(((float *)dstReal)[vecOff]);
                memcpy(realDst, srcImgData, width*height*sizeof(float));
                break;
            }
        }
    } else if (warper->imagWorkCL == NULL) {
        //We need to space the values due to OpenCL implementation reasons
        for( iSrcY = 0; iSrcY < height; iSrcY++ )
        {
            int pxOff = width*iSrcY;
            int imgOff = imgChSize*pxOff + vecOff;
            //Copy & deinterleave interleaved data
            switch (warper->imageFormat) {
                case CL_UNORM_INT8:
                {
                    unsigned char *realDst = &(((unsigned char *)dstReal)[imgOff]);
                    unsigned char *dataSrc = &(((unsigned char *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i)
                        realDst[imgChSize*i] = dataSrc[i];
                }
                    break;
                case CL_SNORM_INT8:
                {
                    char *realDst = &(((char *)dstReal)[imgOff]);
                    char *dataSrc = &(((char *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i)
                        realDst[imgChSize*i] = dataSrc[i];
                }
                    break;
                case CL_UNORM_INT16:
                {
                    unsigned short *realDst = &(((unsigned short *)dstReal)[imgOff]);
                    unsigned short *dataSrc = &(((unsigned short *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i)
                        realDst[imgChSize*i] = dataSrc[i];
                }
                    break;
                case CL_SNORM_INT16:
                {
                    short *realDst = &(((short *)dstReal)[imgOff]);
                    short *dataSrc = &(((short *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i)
                        realDst[imgChSize*i] = dataSrc[i];
                }
                    break;
                case CL_FLOAT:
                {
                    float *realDst = &(((float *)dstReal)[imgOff]);
                    float *dataSrc = &(((float *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i)
                        realDst[imgChSize*i] = dataSrc[i];
                }
                    break;
            }
        }
    } else {
        //Copy, deinterleave, & space interleaved data
        for( iSrcY = 0; iSrcY < height; iSrcY++ )
        {
            int pxOff = width*iSrcY;
            int imgOff = imgChSize*pxOff + vecOff;
            //Copy & deinterleave interleaved data
            switch (warper->imageFormat) {
                case CL_FLOAT:
                {
                    float *realDst = &(((float *)dstReal)[imgOff]);
                    float *imagDst = &(((float *)dstImag)[imgOff]);
                    float *dataSrc = &(((float *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i) {
                        realDst[imgChSize*i] = dataSrc[i*2  ];
                        imagDst[imgChSize*i] = dataSrc[i*2+1];
                    }
                }
                    break;
                case CL_SNORM_INT8:
                {
                    char *realDst = &(((char *)dstReal)[imgOff]);
                    char *imagDst = &(((char *)dstImag)[imgOff]);
                    char *dataSrc = &(((char *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i) {
                        realDst[imgChSize*i] = dataSrc[i*2  ];
                        imagDst[imgChSize*i] = dataSrc[i*2+1];
                    }
                }
                    break;
                case CL_UNORM_INT8:
                {
                    unsigned char *realDst = &(((unsigned char *)dstReal)[imgOff]);
                    unsigned char *imagDst = &(((unsigned char *)dstImag)[imgOff]);
                    unsigned char *dataSrc = &(((unsigned char *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i) {
                        realDst[imgChSize*i] = dataSrc[i*2  ];
                        imagDst[imgChSize*i] = dataSrc[i*2+1];
                    }
                }
                    break;
                case CL_SNORM_INT16:
                {
                    short *realDst = &(((short *)dstReal)[imgOff]);
                    short *imagDst = &(((short *)dstImag)[imgOff]);
                    short *dataSrc = &(((short *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i) {
                        realDst[imgChSize*i] = dataSrc[i*2  ];
                        imagDst[imgChSize*i] = dataSrc[i*2+1];
                    }
                }
                    break;
                case CL_UNORM_INT16:
                {
                    unsigned short *realDst = &(((unsigned short *)dstReal)[imgOff]);
                    unsigned short *imagDst = &(((unsigned short *)dstImag)[imgOff]);
                    unsigned short *dataSrc = &(((unsigned short *)srcImgData)[pxOff]);
                    for (i = 0; i < width; ++i) {
                        realDst[imgChSize*i] = dataSrc[i*2  ];
                        imagDst[imgChSize*i] = dataSrc[i*2+1];
                    }
                }
                    break;
            }
        }
    }
    
    return CL_SUCCESS;
}

/*
 Creates the struct which inits & contains the OpenCL context & environment.
 Inits wired(?) space to buffer the image in host RAM. Chooses the OpenCL
 device, perhaps the user can choose it later? This would also choose the
 appropriate OpenCL image format (R, RG, RGBA, or multiples thereof). Space
 for metadata can be allocated as required, though.
 
 Supported image formats are:
 CL_FLOAT, CL_SNORM_INT8, CL_UNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT16
 32-bit int formats won't keep precision when converted to floats internally
 and doubles are generally not supported on the GPU image formats.
 */
struct oclWarper* GDALWarpKernelOpenCL_createEnv(int srcWidth, int srcHeight,
                                                 int dstWidth, int dstHeight,
                                                 cl_channel_type imageFormat,
                                                 int numBands, int coordMult,
                                                 int useImag, int useBandSrcValid,
                                                 float *fDstDensity,
                                                 double *dfDstNoDataReal,
                                                 OCLResampAlg resampAlg, cl_int *clErr)
{
    struct oclWarper *warper;
    int i;
    size_t maxWidth = 0, maxHeight = 0;
    cl_int err = CL_SUCCESS;
    size_t fmtSize, sz;
    cl_device_id device;
    cl_bool bool_flag;
    OCLVendor eCLVendor = VENDOR_OTHER;
    
    // Do we have a suitable OpenCL device? 
    device = get_device(&eCLVendor);
    if( device == NULL )
        return NULL;
        
    err = clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT, 
                          sizeof(cl_bool), &bool_flag, &sz);
    if( err != CL_SUCCESS || !bool_flag )
    {
        CPLDebug( "OpenCL", "No image support on selected device." );
        return NULL;
    }

    // Set up warper environment.
    warper = (struct oclWarper *)CPLCalloc(1, sizeof(struct oclWarper));

    warper->eCLVendor = eCLVendor;
    
    //Init passed vars
    warper->srcWidth = srcWidth;
    warper->srcHeight = srcHeight;
    warper->dstWidth = dstWidth;
    warper->dstHeight = dstHeight;
    
    warper->coordMult = coordMult;
    warper->numBands = numBands;
    warper->imageFormat = imageFormat;
    warper->resampAlg = resampAlg;

    warper->useUnifiedSrcDensity = FALSE;
    warper->useUnifiedSrcValid = FALSE;
    warper->useDstDensity = FALSE;
    warper->useDstValid = FALSE;
    
    warper->imagWorkCL = NULL;
    warper->dstImagWorkCL = NULL;
    warper->useBandSrcValidCL = NULL;
    warper->useBandSrcValid = NULL;
    warper->nBandSrcValidCL = NULL;
    warper->nBandSrcValid = NULL;
    warper->fDstNoDataRealCL = NULL;
    warper->fDstNoDataReal = NULL;
    warper->kern1 = NULL;
    warper->kern4 = NULL;

    warper->dev = device;
    
    warper->context = clCreateContext(0, 1, &(warper->dev), NULL, NULL, &err);
    handleErrGoto(err, error_label);
    warper->queue = clCreateCommandQueue(warper->context, warper->dev, 0, &err);
    handleErrGoto(err, error_label);
    
    //Ensure that we hand handle imagery of these dimensions
    err = clGetDeviceInfo(warper->dev, CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(size_t), &maxWidth, &sz);
    handleErrGoto(err, error_label);
    err = clGetDeviceInfo(warper->dev, CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(size_t), &maxHeight, &sz);
    handleErrGoto(err, error_label);
    if (maxWidth < srcWidth || maxHeight < srcHeight) {
        err = CL_INVALID_IMAGE_SIZE;
        handleErrGoto(err, error_label);
    }
    
    // Split bands into sets of four when possible
    // Cubic runs slower as vector, so don't use it (probably register pressure)
    // Feel free to do more testing and come up with more precise case statements
    if(numBands < 4 || resampAlg == OCL_Cubic) {
        warper->numImages = numBands;
        warper->useVec = FALSE;
    } else {
        warper->numImages = numBands/4 + numBands % 4;
        warper->useVec = TRUE;
    }
    
    //Make the pointer space for the real images
    warper->realWorkCL = (cl_mem *)CPLCalloc(sizeof(cl_mem), warper->numImages);
    warper->dstRealWorkCL = (cl_mem *)CPLCalloc(sizeof(cl_mem), warper->numImages);
    
    //Make space for the per-channel Imag data (if exists)
    if (useImag) {
        warper->imagWorkCL = (cl_mem *)CPLCalloc(sizeof(cl_mem), warper->numImages);
        warper->dstImagWorkCL = (cl_mem *)CPLCalloc(sizeof(cl_mem), warper->numImages);
    }
    
    //Make space for the per-band BandSrcValid data (if exists)
    if (useBandSrcValid) {
        //32 bits in the mask
        size_t sz = warper->numBands * (1 + (warper->srcWidth * warper->srcHeight >> 5));
        
        //Allocate some space for the validity of the validity mask
        err = alloc_pinned_mem(warper, 0, warper->numBands*sizeof(char),
                               (void **)&(warper->useBandSrcValid),
                               &(warper->useBandSrcValidCL));
        handleErrGoto(err, error_label);
        
        for (i = 0; i < warper->numBands; ++i)
            warper->useBandSrcValid[i] = FALSE;
        
        //Allocate one array for all the band validity masks
        //Remember that the masks don't use much memeory (they're bitwise)
        err = alloc_pinned_mem(warper, 0, sz * sizeof(int),
                               (void **)&(warper->nBandSrcValid),
                               &(warper->nBandSrcValidCL));
        handleErrGoto(err, error_label);
    }
    
    //Make space for the per-band 
    if (dfDstNoDataReal != NULL) {
        alloc_pinned_mem(warper, 0, warper->numBands,
                         (void **)&(warper->fDstNoDataReal), &(warper->fDstNoDataRealCL));
        
        //Copy over values
        for (i = 0; i < warper->numBands; ++i)
            warper->fDstNoDataReal[i] = dfDstNoDataReal[i];
    }
    
    //Alloc working host image memory
    //We'll be copying into these buffers soon
    switch (imageFormat) {
      case CL_FLOAT:
        err = alloc_working_arr(warper, sizeof(float *), sizeof(float), &fmtSize);
        break;
      case CL_SNORM_INT8:
        err = alloc_working_arr(warper, sizeof(char *), sizeof(char), &fmtSize);
        break;
      case CL_UNORM_INT8:
        err = alloc_working_arr(warper, sizeof(unsigned char *), sizeof(unsigned char), &fmtSize);
        break;
      case CL_SNORM_INT16:
        err = alloc_working_arr(warper, sizeof(short *), sizeof(short), &fmtSize);
        break;
      case CL_UNORM_INT16:
        err = alloc_working_arr(warper, sizeof(unsigned short *), sizeof(unsigned short), &fmtSize);
        break;
    }
    handleErrGoto(err, error_label);
    
    //Find a good & compable image channel order for the Lat/Long arr
    err = set_supported_formats(warper, 2,
                                &(warper->xyChOrder), &(warper->xyChSize),
                                CL_FLOAT);
    handleErrGoto(err, error_label);
    
    //Set coordinate image dimensions
    warper->xyWidth  = ceil(((float)warper->dstWidth  + (float)warper->coordMult-1)/(float)warper->coordMult);
    warper->xyHeight = ceil(((float)warper->dstHeight + (float)warper->coordMult-1)/(float)warper->coordMult);
    
    //Alloc coord memory
    sz = sizeof(float) * warper->xyChSize * warper->xyWidth * warper->xyHeight;
    err = alloc_pinned_mem(warper, 0, sz, (void **)&(warper->xyWork),
                           &(warper->xyWorkCL));
    handleErrGoto(err, error_label);
    
    //Ensure everything is finished allocating, copying, & mapping
    err = clFinish(warper->queue);
    handleErrGoto(err, error_label);
    
    (*clErr) = CL_SUCCESS;
    return warper;
    
error_label:
    GDALWarpKernelOpenCL_deleteEnv(warper);
    return NULL;
}

/*
 Copy the validity mask for an image band to the warper.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int GDALWarpKernelOpenCL_setSrcValid(struct oclWarper *warper,
                                        int *bandSrcValid, int bandNum)
{
    //32 bits in the mask
    int stride = 1 + (warper->srcWidth * warper->srcHeight >> 5);
    
    //Copy bandSrcValid
    assert(warper->nBandSrcValid != NULL);
    memcpy(&(warper->nBandSrcValid[bandNum*stride]), bandSrcValid, sizeof(int) * stride);
    warper->useBandSrcValid[bandNum] = TRUE;
    
    return CL_SUCCESS;
}

/*
 Sets the source image real & imag into the host memory so that it is
 permuted (ex. RGBA) for better graphics card access.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int GDALWarpKernelOpenCL_setSrcImg(struct oclWarper *warper, void *imgData,
                                      int bandNum)
{
    void **imagWorkPtr = NULL;
    
    if (warper->imagWorkCL != NULL)
        imagWorkPtr = warper->imagWork.v;
    
    return set_img_data(warper, imgData, warper->srcWidth, warper->srcHeight,
                        TRUE, bandNum, warper->realWork.v, imagWorkPtr);
}

/*
 Sets the destination image real & imag into the host memory so that it is
 permuted (ex. RGBA) for better graphics card access.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int GDALWarpKernelOpenCL_setDstImg(struct oclWarper *warper, void *imgData,
                                      int bandNum)
{
    void **dstImagWorkPtr = NULL;
    
    if (warper->dstImagWorkCL != NULL)
        dstImagWorkPtr = warper->dstImagWork.v;
    
    return set_img_data(warper, imgData, warper->dstWidth, warper->dstHeight,
                        FALSE, bandNum, warper->dstRealWork.v, dstImagWorkPtr);
}

/*
 Inputs the source coordinates for a row of the destination pixels. Invalid
 coordinates are set as -99.0, which should be out of the image bounds. Sets
 the coordinates as ready to be used in OpenCL image memory: interleaved and
 minus the offset. By using image memory, we can use a smaller texture for
 coordinates and use OpenCL's built-in interpolation to save memory.
 
 What it does: generates a smaller matrix of X/Y coordinate transformation
 values from an original matrix. When bilinearly sampled in the GPU hardware,
 the generated values are as close as possible to the original matrix.
 
 Complication: matricies have arbitrary dimensions and the sub-sampling factor
 is an arbitrary integer greater than zero. Getting the edge cases right is 
 difficult.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int GDALWarpKernelOpenCL_setCoordRow(struct oclWarper *warper,
                                        double *rowSrcX, double *rowSrcY,
                                        double srcXOff, double srcYOff,
                                        int *success, int rowNum)
{
    int coordMult = warper->coordMult;
    int width = warper->dstWidth;
    int height = warper->dstHeight;
    int xyWidth = warper->xyWidth;
    int i;
    int xyChSize = warper->xyChSize;
    float *xyPtr, *xyPrevPtr = NULL;
    int lastRow = rowNum == height - 1;
    double dstHeightMod = 1.0, dstWidthMod = 1.0;
    
    //Return if we're at an off row
    if(!lastRow && rowNum % coordMult != 0)
        return CL_SUCCESS;
    
    //Standard row, adjusted for the skipped rows
    xyPtr = &(warper->xyWork[xyWidth * xyChSize * rowNum / coordMult]);
    
    //Find our row
    if(lastRow){
        //Setup for the final row
        xyPtr     = &(warper->xyWork[xyWidth * xyChSize * (warper->xyHeight - 1)]);
        xyPrevPtr = &(warper->xyWork[xyWidth * xyChSize * (warper->xyHeight - 2)]);
        
        if((height-1) % coordMult)
            dstHeightMod = (double)coordMult / (double)((height-1) % coordMult);
    }
    
    //Copy selected coordinates
    for (i = 0; i < width; i += coordMult) {
        if (success[i]) {
            xyPtr[0] = rowSrcX[i] - srcXOff;
            xyPtr[1] = rowSrcY[i] - srcYOff;
            
            if(lastRow) {
                //Adjust bottom row so interpolator returns correct value
                xyPtr[0] = dstHeightMod * (xyPtr[0] - xyPrevPtr[0]) + xyPrevPtr[0];
                xyPtr[1] = dstHeightMod * (xyPtr[1] - xyPrevPtr[1]) + xyPrevPtr[1];
            }
        } else {
            xyPtr[0] = -99.0f;
            xyPtr[1] = -99.0f;
        }
        
        xyPtr += xyChSize;
        xyPrevPtr += xyChSize;
    }
    
    //Copy remaining coordinate
    if((width-1) % coordMult){
        dstWidthMod = (double)coordMult / (double)((width-1) % coordMult);
        xyPtr -= xyChSize;
        xyPrevPtr -= xyChSize;
    } else {
        xyPtr -= xyChSize*2;
        xyPrevPtr -= xyChSize*2;
    }
    
    if(lastRow) {
        double origX = rowSrcX[width-1] - srcXOff;
        double origY = rowSrcY[width-1] - srcYOff;
        double a = 1.0, b = 1.0;
        
        // Calculate the needed x/y values using an equation from the OpenCL Spec
        // section 8.2, solving for Ti1j1
        if((width -1) % coordMult)
            a = ((width -1) % coordMult)/(double)coordMult;
        
        if((height-1) % coordMult)
            b = ((height-1) % coordMult)/(double)coordMult;
        
        xyPtr[xyChSize  ] = (((1.0 - a) * (1.0 - b) * xyPrevPtr[0]
                              + a * (1.0 - b) * xyPrevPtr[xyChSize]
                              + (1.0 - a) * b * xyPtr[0]) - origX)/(-a * b);
        
        xyPtr[xyChSize+1] = (((1.0 - a) * (1.0 - b) * xyPrevPtr[1]
                              + a * (1.0 - b) * xyPrevPtr[xyChSize+1]
                              + (1.0 - a) * b * xyPtr[1]) - origY)/(-a * b);
    } else {
        //Adjust last coordinate so interpolator returns correct value
        xyPtr[xyChSize  ] = dstWidthMod * (rowSrcX[width-1] - srcXOff - xyPtr[0]) + xyPtr[0];
        xyPtr[xyChSize+1] = dstWidthMod * (rowSrcY[width-1] - srcYOff - xyPtr[1]) + xyPtr[1];
    }
    
    return CL_SUCCESS;
}

/*
 Copies all data to the device RAM, frees the host RAM, runs the
 appropriate resampling kernel, mallocs output space, & copies the data
 back from the device RAM for each band. Also check to make sure that
 setRow*() was called the appropriate number of times to init all image
 data.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int GDALWarpKernelOpenCL_runResamp(struct oclWarper *warper,
                                      float *unifiedSrcDensity,
                                      unsigned int *unifiedSrcValid,
                                      float *dstDensity,
                                      unsigned int *dstValid,
                                      double dfXScale, double dfYScale,
                                      double dfXFilter, double dfYFilter,
                                      int nXRadius, int nYRadius,
                                      int nFiltInitX, int nFiltInitY)
{
    int i, nextBandNum = 0, chSize = 1;
	cl_int err = CL_SUCCESS;
    cl_mem xy, unifiedSrcDensityCL, unifiedSrcValidCL;
    cl_mem dstDensityCL, dstValidCL, dstNoDataRealCL;
    cl_mem useBandSrcValidCL, nBandSrcValidCL;
	size_t groupSize, wordSize;
    cl_kernel kern = NULL;
    cl_channel_order chOrder;
    
    warper->useUnifiedSrcDensity = unifiedSrcDensity != NULL;
    warper->useUnifiedSrcValid = unifiedSrcValid != NULL;

    //Check the word size
    switch (warper->imageFormat) {
        case CL_FLOAT:
            wordSize = sizeof(float);
            break;
        case CL_SNORM_INT8:
            wordSize = sizeof(char);
            break;
        case CL_UNORM_INT8:
            wordSize = sizeof(unsigned char);
            break;
        case CL_SNORM_INT16:
            wordSize = sizeof(short);
            break;
        case CL_UNORM_INT16:
            wordSize = sizeof(unsigned short);
            break;
    }
    
    //Compile the kernel; the invariants are being compiled into the code
    if (!warper->useVec || warper->numBands % 4) {
        warper->kern1 = get_kernel(warper, FALSE,
                                   dfXScale, dfYScale, dfXFilter, dfYFilter,
                                   nXRadius, nYRadius, nFiltInitX, nFiltInitY, &err);
        handleErr(err);
    }
    if (warper->useVec){
        warper->kern4 = get_kernel(warper, TRUE,
                                   dfXScale, dfYScale, dfXFilter, dfYFilter,
                                   nXRadius, nYRadius, nFiltInitX, nFiltInitY, &err);
        handleErr(err);
    }
    
    //Copy coord data to the device
    handleErr(err = set_coord_data(warper, &xy));
    
    //Copy unified density & valid data
    handleErr(err = set_unified_data(warper, &unifiedSrcDensityCL, &unifiedSrcValidCL,
                                     unifiedSrcDensity, unifiedSrcValid,
                                     &useBandSrcValidCL, &nBandSrcValidCL));
    
    //Copy output density & valid data
    handleErr(set_dst_data(warper, &dstDensityCL, &dstValidCL, &dstNoDataRealCL,
                           dstDensity, dstValid, warper->fDstNoDataReal));
    
    //What's the recommended group size?
    if (warper->useVec) {
        // Start with the vector kernel
        handleErr(clGetKernelWorkGroupInfo(warper->kern4, warper->dev,
                                           CL_KERNEL_WORK_GROUP_SIZE,
                                           sizeof(size_t), &groupSize, NULL));
        kern = warper->kern4;
        chSize = warper->imgChSize4;
        chOrder = warper->imgChOrder4;
    } else {
        // We're only using the float kernel
        handleErr(clGetKernelWorkGroupInfo(warper->kern1, warper->dev,
                                           CL_KERNEL_WORK_GROUP_SIZE,
                                           sizeof(size_t), &groupSize, NULL));
        kern = warper->kern1;
        chSize = warper->imgChSize1;
        chOrder = warper->imgChOrder1;
    }
    
    //Loop over each image
    for (i = 0; i < warper->numImages; ++i)
    {
        cl_mem srcImag, srcReal;
        cl_mem dstReal, dstImag;
        int bandNum = nextBandNum;
        
        //Switch kernels if needed
        if (warper->useVec && nextBandNum < warper->numBands - warper->numBands % 4) {
            nextBandNum += 4;
        } else {
            if (kern == warper->kern4) {
                handleErr(clGetKernelWorkGroupInfo(warper->kern1, warper->dev,
                                                   CL_KERNEL_WORK_GROUP_SIZE,
                                                   sizeof(size_t), &groupSize, NULL));
                kern = warper->kern1;
                chSize = warper->imgChSize1;
                chOrder = warper->imgChOrder1;
            }
            ++nextBandNum;
        }
        
        //Create & copy the source image
        handleErr(err = set_src_rast_data(warper, i, chSize*wordSize, chOrder,
                                          &srcReal, &srcImag));
        
        //Create & copy the output image
        if (kern == warper->kern1) {
            handleErr(err = set_dst_rast_data(warper, i, wordSize, &dstReal, &dstImag));
        } else {
            handleErr(err = set_dst_rast_data(warper, i, wordSize*4, &dstReal, &dstImag));
        }
        
        //Set the bandNum
        handleErr(err = clSetKernelArg(kern, 12, sizeof(int), &bandNum));
        
        //Run the kernel
        handleErr(err = execute_kern(warper, kern, groupSize));
        
        //Free loop CL mem
        handleErr(err = clReleaseMemObject(srcReal));
        handleErr(err = clReleaseMemObject(srcImag));
        
        //Copy the back output results
        if (kern == warper->kern1) {
            handleErr(err = get_dst_rast_data(warper, i, wordSize, dstReal, dstImag));
        } else {
            handleErr(err = get_dst_rast_data(warper, i, wordSize*4, dstReal, dstImag));
        }
            
        //Free remaining CL mem
        handleErr(err = clReleaseMemObject(dstReal));
        handleErr(err = clReleaseMemObject(dstImag));
    }
    
    //Free remaining CL mem
    handleErr(err = clReleaseMemObject(xy));
    handleErr(err = clReleaseMemObject(unifiedSrcDensityCL));
    handleErr(err = clReleaseMemObject(unifiedSrcValidCL));
    handleErr(err = clReleaseMemObject(useBandSrcValidCL));
    handleErr(err = clReleaseMemObject(nBandSrcValidCL));
    handleErr(err = clReleaseMemObject(dstDensityCL));
    handleErr(err = clReleaseMemObject(dstValidCL));
    handleErr(err = clReleaseMemObject(dstNoDataRealCL));

    return CL_SUCCESS;
}

/*
 Sets pointers to the floating point data in the warper. The pointers
 are internal to the warper structure, so don't free() them. If the imag
 channel is in use, it will receive a pointer. Otherwise it'll be set to NULL.
 These are pointers to floating point data, so the caller will need to
 manipulate the output as appropriate before saving the data.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int GDALWarpKernelOpenCL_getRow(struct oclWarper *warper,
                                   void **rowReal, void **rowImag,
                                   int rowNum, int bandNum)
{
    int memOff = rowNum * warper->dstWidth;
    int imgNum = bandNum;
    
    if (warper->useVec && bandNum < warper->numBands - warper->numBands % 4) {
        memOff += warper->dstWidth * warper->dstHeight * (bandNum % 4);
        imgNum = bandNum / 4;
    } else if(warper->useVec) {
        imgNum = bandNum / 4 + bandNum % 4;
    }
    
    //Return pointers into the warper's data
    switch (warper->imageFormat) {
        case CL_FLOAT:
            (*rowReal) = &(warper->dstRealWork.f[imgNum][memOff]);
            break;
        case CL_SNORM_INT8:
            (*rowReal) = &(warper->dstRealWork.c[imgNum][memOff]);
            break;
        case CL_UNORM_INT8:
            (*rowReal) = &(warper->dstRealWork.uc[imgNum][memOff]);
            break;
        case CL_SNORM_INT16:
            (*rowReal) = &(warper->dstRealWork.s[imgNum][memOff]);
            break;
        case CL_UNORM_INT16:
            (*rowReal) = &(warper->dstRealWork.us[imgNum][memOff]);
            break;
    }
    
    if (warper->dstImagWorkCL == NULL) {
        (*rowImag) = NULL;
    } else {
        switch (warper->imageFormat) {
            case CL_FLOAT:
                (*rowImag) = &(warper->dstImagWork.f[imgNum][memOff]);
                break;
            case CL_SNORM_INT8:
                (*rowImag) = &(warper->dstImagWork.c[imgNum][memOff]);
                break;
            case CL_UNORM_INT8:
                (*rowImag) = &(warper->dstImagWork.uc[imgNum][memOff]);
                break;
            case CL_SNORM_INT16:
                (*rowImag) = &(warper->dstImagWork.s[imgNum][memOff]);
                break;
            case CL_UNORM_INT16:
                (*rowImag) = &(warper->dstImagWork.us[imgNum][memOff]);
                break;
        }
    }
    
    return CL_SUCCESS;
}

/*
 Free the OpenCL warper environment. It should check everything for NULL, so
 be sure to mark free()ed pointers as NULL or it'll be double free()ed.
 
 Returns CL_SUCCESS on success and other CL_* errors when something goes wrong.
 */
cl_int GDALWarpKernelOpenCL_deleteEnv(struct oclWarper *warper)
{
    int i;
	cl_int err = CL_SUCCESS;
    
    for (i = 0; i < warper->numImages; ++i) {
        // Run free!!
        void* dummy = NULL;
        if( warper->realWork.v )
            freeCLMem(warper->realWorkCL[i], warper->realWork.v[i]);
        else
            freeCLMem(warper->realWorkCL[i], dummy);
        if( warper->realWork.v )
            freeCLMem(warper->dstRealWorkCL[i], warper->dstRealWork.v[i]);
        else
            freeCLMem(warper->dstRealWorkCL[i], dummy);
        
        //(As applicable)
        if(warper->imagWorkCL != NULL && warper->imagWork.v != NULL && warper->imagWork.v[i] != NULL) {
            freeCLMem(warper->imagWorkCL[i], warper->imagWork.v[i]);
        }
        if(warper->dstImagWorkCL != NULL && warper->dstImagWork.v != NULL && warper->dstImagWork.v[i] != NULL) {
            freeCLMem(warper->dstImagWorkCL[i], warper->dstImagWork.v[i]);
        }
    }
    
    //Free cl_mem
    freeCLMem(warper->useBandSrcValidCL, warper->useBandSrcValid);
    freeCLMem(warper->nBandSrcValidCL, warper->nBandSrcValid);
    freeCLMem(warper->xyWorkCL, warper->xyWork);
    freeCLMem(warper->fDstNoDataRealCL, warper->fDstNoDataReal);
    
    //Free pointers to cl_mem*
    if (warper->realWorkCL != NULL)
        CPLFree(warper->realWorkCL);
    if (warper->dstRealWorkCL != NULL)
        CPLFree(warper->dstRealWorkCL);
    
    if (warper->imagWorkCL != NULL)
        CPLFree(warper->imagWorkCL);
    if (warper->dstImagWorkCL != NULL)
        CPLFree(warper->dstImagWorkCL);

    if (warper->realWork.v != NULL)
        CPLFree(warper->realWork.v);
    if (warper->dstRealWork.v != NULL)
        CPLFree(warper->dstRealWork.v);
    
    if (warper->imagWork.v != NULL)
        CPLFree(warper->imagWork.v);
    if (warper->dstImagWork.v != NULL)
        CPLFree(warper->dstImagWork.v);
    
    //Free OpenCL structures
    if (warper->kern1 != NULL)
        clReleaseKernel(warper->kern1);
    if (warper->kern4 != NULL)
        clReleaseKernel(warper->kern4);
    if (warper->queue != NULL)
        clReleaseCommandQueue(warper->queue);
    if (warper->context != NULL)
        clReleaseContext(warper->context);
    
    CPLFree(warper);
    
    return CL_SUCCESS;
}

#endif /* defined(HAVE_OPENCL) */

