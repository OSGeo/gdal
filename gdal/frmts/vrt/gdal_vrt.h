/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  C/Public declarations of virtual GDAL dataset objects.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
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

#ifndef GDAL_VRT_H_INCLUDED
#define GDAL_VRT_H_INCLUDED

/**
 * \file gdal_vrt.h
 *
 * Public (C callable) entry points for virtual GDAL dataset objects.
 */

#include "gdal.h"
#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_minixml.h"

#define VRT_NODATA_UNSET -1234.56

CPL_C_START

void    GDALRegister_VRT(void);
typedef CPLErr
(*VRTImageReadFunc)( void *hCBData,
                     int nXOff, int nYOff, int nXSize, int nYSize,
                     void *pData );

/* -------------------------------------------------------------------- */
/*      Define handle types related to various VRT dataset classes.     */
/* -------------------------------------------------------------------- */
typedef void *VRTDriverH;
typedef void *VRTSourceH;
typedef void *VRTSimpleSourceH;
typedef void *VRTAveragedSourceH;
typedef void *VRTComplexSourceH;
typedef void *VRTFilteredSourceH;
typedef void *VRTKernelFilteredSourceH;
typedef void *VRTAverageFilteredSourceH;
typedef void *VRTFuncSourceH;
typedef void *VRTDatasetH;
typedef void *VRTWarpedDatasetH;
typedef void *VRTRasterBandH;
typedef void *VRTSourcedRasterBandH;
typedef void *VRTWarpedRasterBandH;
typedef void *VRTDerivedRasterBandH;
typedef void *VRTRawRasterBandH;

/* ==================================================================== */
/*      VRTDataset class.                                               */
/* ==================================================================== */

VRTDatasetH CPL_DLL CPL_STDCALL VRTCreate( int, int );
void CPL_DLL CPL_STDCALL VRTFlushCache( VRTDatasetH );
CPLXMLNode CPL_DLL * CPL_STDCALL VRTSerializeToXML( VRTDatasetH, const char * );
int CPL_DLL CPL_STDCALL VRTAddBand( VRTDatasetH, GDALDataType, char ** );

/* ==================================================================== */
/*      VRTSourcedRasterBand class.                                     */
/* ==================================================================== */

CPLErr CPL_STDCALL VRTAddSource( VRTSourcedRasterBandH, VRTSourceH );
CPLErr CPL_DLL CPL_STDCALL VRTAddSimpleSource( VRTSourcedRasterBandH,
                                               GDALRasterBandH,
                                               int, int, int, int,
                                               int, int, int, int,
                                               const char *, double );
CPLErr CPL_DLL CPL_STDCALL VRTAddComplexSource( VRTSourcedRasterBandH,
                                                GDALRasterBandH, 
                                                int, int, int, int,
                                                int, int, int, int,
                                                double, double, double );
CPLErr CPL_DLL CPL_STDCALL VRTAddFuncSource( VRTSourcedRasterBandH,
                                             VRTImageReadFunc,
                                             void *, double );


CPL_C_END

#endif /* GDAL_VRT_H_INCLUDED */

