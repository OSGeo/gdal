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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_VRT_H_INCLUDED
#define GDAL_VRT_H_INCLUDED

/**
 * \file gdal_vrt.h
 *
 * Public (C callable) entry points for virtual GDAL dataset objects.
 */

#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_port.h"
#include "gdal.h"

/** Special value to indicate that nodata is not set */
#define VRT_NODATA_UNSET -1234.56

CPL_C_START

/** Type for a function that returns the pixel data in a provided window */
typedef CPLErr (*VRTImageReadFunc)(void *hCBData, int nXOff, int nYOff,
                                   int nXSize, int nYSize, void *pData);

/* -------------------------------------------------------------------- */
/*      Define handle types related to various VRT dataset classes.     */
/* -------------------------------------------------------------------- */
/*! @cond Doxygen_Suppress */
typedef void *VRTAveragedSourceH;
typedef void *VRTAverageFilteredSourceH;
typedef void *VRTComplexSourceH;
typedef void *VRTDerivedRasterBandH;
typedef void *VRTDriverH;
typedef void *VRTFilteredSourceH;
typedef void *VRTFuncSourceH;
typedef void *VRTKernelFilteredSourceH;
typedef void *VRTRasterBandH;
typedef void *VRTRawRasterBandH;
typedef void *VRTSimpleSourceH;
typedef void *VRTSourceH;
typedef void *VRTWarpedDatasetH;
typedef void *VRTWarpedRasterBandH;
/*! @endcond */

/** Opaque type for a VRT dataset */
typedef void *VRTDatasetH;
/** Opaque type for a VRT sourced raster band */
typedef void *VRTSourcedRasterBandH;

/* ==================================================================== */
/*      VRTDataset class.                                               */
/* ==================================================================== */

VRTDatasetH CPL_DLL CPL_STDCALL VRTCreate(int, int);
void CPL_DLL CPL_STDCALL VRTFlushCache(VRTDatasetH);
CPLXMLNode CPL_DLL *CPL_STDCALL VRTSerializeToXML(VRTDatasetH, const char *);
int CPL_DLL CPL_STDCALL VRTAddBand(VRTDatasetH, GDALDataType, char **);

/* ==================================================================== */
/*      VRTSourcedRasterBand class.                                     */
/* ==================================================================== */

CPLErr CPL_STDCALL VRTAddSource(VRTSourcedRasterBandH, VRTSourceH);
CPLErr CPL_DLL CPL_STDCALL VRTAddSimpleSource(VRTSourcedRasterBandH,
                                              GDALRasterBandH, int, int, int,
                                              int, int, int, int, int,
                                              const char *, double);
CPLErr CPL_DLL CPL_STDCALL VRTAddComplexSource(VRTSourcedRasterBandH,
                                               GDALRasterBandH, int, int, int,
                                               int, int, int, int, int, double,
                                               double, double);
CPLErr CPL_DLL CPL_STDCALL VRTAddFuncSource(VRTSourcedRasterBandH,
                                            VRTImageReadFunc, void *, double);

CPL_C_END

#endif /* GDAL_VRT_H_INCLUDED */
