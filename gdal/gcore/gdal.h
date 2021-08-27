/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  GDAL Core C/Public declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002 Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDAL_H_INCLUDED
#define GDAL_H_INCLUDED

/**
 * \file gdal.h
 *
 * Public (C callable) GDAL entry points.
 */

#ifndef DOXYGEN_SKIP
#if defined(GDAL_COMPILATION)
#define DO_NOT_DEFINE_GDAL_RELEASE_DATE_AND_GDAL_RELEASE_NAME
#endif
#include "gdal_version.h"
#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_virtualmem.h"
#include "cpl_minixml.h"
#include "ogr_api.h"
#endif

#include <stdbool.h>

/* -------------------------------------------------------------------- */
/*      Significant constants.                                          */
/* -------------------------------------------------------------------- */

CPL_C_START

/*! Pixel data types */
typedef enum {
    /*! Unknown or unspecified type */          GDT_Unknown = 0,
    /*! Eight bit unsigned integer */           GDT_Byte = 1,
    /*! Sixteen bit unsigned integer */         GDT_UInt16 = 2,
    /*! Sixteen bit signed integer */           GDT_Int16 = 3,
    /*! Thirty two bit unsigned integer */      GDT_UInt32 = 4,
    /*! Thirty two bit signed integer */        GDT_Int32 = 5,
    /* TODO?(#6879): GDT_UInt64 */
    /* TODO?(#6879): GDT_Int64 */
    /*! Thirty two bit floating point */        GDT_Float32 = 6,
    /*! Sixty four bit floating point */        GDT_Float64 = 7,
    /*! Complex Int16 */                        GDT_CInt16 = 8,
    /*! Complex Int32 */                        GDT_CInt32 = 9,
    /* TODO?(#6879): GDT_CInt64 */
    /*! Complex Float32 */                      GDT_CFloat32 = 10,
    /*! Complex Float64 */                      GDT_CFloat64 = 11,
    GDT_TypeCount = 12          /* maximum type # + 1 */
} GDALDataType;

int CPL_DLL CPL_STDCALL GDALGetDataTypeSize( GDALDataType );  // Deprecated.
int CPL_DLL CPL_STDCALL GDALGetDataTypeSizeBits( GDALDataType eDataType );
int CPL_DLL CPL_STDCALL GDALGetDataTypeSizeBytes( GDALDataType );
int CPL_DLL CPL_STDCALL GDALDataTypeIsComplex( GDALDataType );
int CPL_DLL CPL_STDCALL GDALDataTypeIsInteger( GDALDataType );
int CPL_DLL CPL_STDCALL GDALDataTypeIsFloating( GDALDataType );
int CPL_DLL CPL_STDCALL GDALDataTypeIsSigned( GDALDataType );
const char CPL_DLL * CPL_STDCALL GDALGetDataTypeName( GDALDataType );
GDALDataType CPL_DLL CPL_STDCALL GDALGetDataTypeByName( const char * );
GDALDataType CPL_DLL CPL_STDCALL GDALDataTypeUnion( GDALDataType, GDALDataType );
GDALDataType CPL_DLL CPL_STDCALL GDALDataTypeUnionWithValue( GDALDataType eDT, double dValue, int bComplex );
GDALDataType CPL_DLL CPL_STDCALL GDALFindDataType( int nBits, int bSigned, int bFloating, int bComplex );
GDALDataType CPL_DLL CPL_STDCALL GDALFindDataTypeForValue( double dValue, int bComplex );
double CPL_DLL GDALAdjustValueToDataType( GDALDataType eDT, double dfValue, int* pbClamped, int* pbRounded );
GDALDataType CPL_DLL CPL_STDCALL GDALGetNonComplexDataType( GDALDataType );
int CPL_DLL CPL_STDCALL GDALDataTypeIsConversionLossy( GDALDataType eTypeFrom,
                                                       GDALDataType eTypeTo );

/**
* status of the asynchronous stream
*/
typedef enum
{
    GARIO_PENDING = 0,
    GARIO_UPDATE = 1,
    GARIO_ERROR = 2,
    GARIO_COMPLETE = 3,
    GARIO_TypeCount = 4
} GDALAsyncStatusType;

const char CPL_DLL * CPL_STDCALL GDALGetAsyncStatusTypeName( GDALAsyncStatusType );
GDALAsyncStatusType CPL_DLL CPL_STDCALL GDALGetAsyncStatusTypeByName( const char * );

/*! Flag indicating read/write, or read-only access to data. */
typedef enum {
    /*! Read only (no update) access */ GA_ReadOnly = 0,
    /*! Read/write access. */           GA_Update = 1
} GDALAccess;

/*! Read/Write flag for RasterIO() method */
typedef enum {
    /*! Read data */   GF_Read = 0,
    /*! Write data */  GF_Write = 1
} GDALRWFlag;

/* NOTE: values are selected to be consistent with GDALResampleAlg of alg/gdalwarper.h */
/** RasterIO() resampling method.
  * @since GDAL 2.0
  */
typedef enum
{
    /*! Nearest neighbour */                            GRIORA_NearestNeighbour = 0,
    /*! Bilinear (2x2 kernel) */                        GRIORA_Bilinear = 1,
    /*! Cubic Convolution Approximation (4x4 kernel) */ GRIORA_Cubic = 2,
    /*! Cubic B-Spline Approximation (4x4 kernel) */    GRIORA_CubicSpline = 3,
    /*! Lanczos windowed sinc interpolation (6x6 kernel) */ GRIORA_Lanczos = 4,
    /*! Average */                                      GRIORA_Average = 5,
    /*! Mode (selects the value which appears most often of all the sampled points) */
                                                        GRIORA_Mode = 6,
    /*! Gauss blurring */                               GRIORA_Gauss = 7,
    /* NOTE: values 8 to 13 are reserved for max,min,med,Q1,Q3,sum */
/*! @cond Doxygen_Suppress */
                                                        GRIORA_RESERVED_START = 8,
                                                        GRIORA_RESERVED_END = 13,
/*! @endcond */
    /** RMS: Root Mean Square / Quadratic Mean.
     * For complex numbers, applies on the real and imaginary part independently.
     */
                                                        GRIORA_RMS = 14,
/*! @cond Doxygen_Suppress */
                                                        GRIORA_LAST = GRIORA_RMS
/*! @endcond */
} GDALRIOResampleAlg;

/* NOTE to developers: only add members, and if so edit INIT_RASTERIO_EXTRA_ARG */
/** Structure to pass extra arguments to RasterIO() method,
 * must be initialized with INIT_RASTERIO_EXTRA_ARG
  * @since GDAL 2.0
  */
typedef struct
{
    /*! Version of structure (to allow future extensions of the structure) */
    int                    nVersion;

    /*! Resampling algorithm */
    GDALRIOResampleAlg     eResampleAlg;

    /*! Progress callback */
    GDALProgressFunc       pfnProgress;
    /*! Progress callback user data */
    void                  *pProgressData;

    /*! Indicate if dfXOff, dfYOff, dfXSize and dfYSize are set.
        Mostly reserved from the VRT driver to communicate a more precise
        source window. Must be such that dfXOff - nXOff < 1.0 and
        dfYOff - nYOff < 1.0 and nXSize - dfXSize < 1.0 and nYSize - dfYSize < 1.0 */
    int                    bFloatingPointWindowValidity;
    /*! Pixel offset to the top left corner. Only valid if bFloatingPointWindowValidity = TRUE */
    double                 dfXOff;
    /*! Line offset to the top left corner. Only valid if bFloatingPointWindowValidity = TRUE */
    double                 dfYOff;
    /*! Width in pixels of the area of interest. Only valid if bFloatingPointWindowValidity = TRUE */
    double                 dfXSize;
    /*! Height in pixels of the area of interest. Only valid if bFloatingPointWindowValidity = TRUE */
    double                 dfYSize;
} GDALRasterIOExtraArg;

#ifndef DOXYGEN_SKIP
#define RASTERIO_EXTRA_ARG_CURRENT_VERSION  1
#endif

/** Macro to initialize an instance of GDALRasterIOExtraArg structure.
  * @since GDAL 2.0
  */
#define INIT_RASTERIO_EXTRA_ARG(s)  \
    do { (s).nVersion = RASTERIO_EXTRA_ARG_CURRENT_VERSION; \
         (s).eResampleAlg = GRIORA_NearestNeighbour; \
         (s).pfnProgress = CPL_NULLPTR; \
         (s).pProgressData = CPL_NULLPTR; \
         (s).bFloatingPointWindowValidity = FALSE; } while(0)

/*! Types of color interpretation for raster bands. */
typedef enum
{
    /*! Undefined */                                      GCI_Undefined=0,
    /*! Greyscale */                                      GCI_GrayIndex=1,
    /*! Paletted (see associated color table) */          GCI_PaletteIndex=2,
    /*! Red band of RGBA image */                         GCI_RedBand=3,
    /*! Green band of RGBA image */                       GCI_GreenBand=4,
    /*! Blue band of RGBA image */                        GCI_BlueBand=5,
    /*! Alpha (0=transparent, 255=opaque) */              GCI_AlphaBand=6,
    /*! Hue band of HLS image */                          GCI_HueBand=7,
    /*! Saturation band of HLS image */                   GCI_SaturationBand=8,
    /*! Lightness band of HLS image */                    GCI_LightnessBand=9,
    /*! Cyan band of CMYK image */                        GCI_CyanBand=10,
    /*! Magenta band of CMYK image */                     GCI_MagentaBand=11,
    /*! Yellow band of CMYK image */                      GCI_YellowBand=12,
    /*! Black band of CMYK image */                       GCI_BlackBand=13,
    /*! Y Luminance */                                    GCI_YCbCr_YBand=14,
    /*! Cb Chroma */                                      GCI_YCbCr_CbBand=15,
    /*! Cr Chroma */                                      GCI_YCbCr_CrBand=16,
    /*! Max current value (equals to GCI_YCbCr_CrBand currently) */ GCI_Max=16
} GDALColorInterp;

const char CPL_DLL *GDALGetColorInterpretationName( GDALColorInterp );
GDALColorInterp CPL_DLL GDALGetColorInterpretationByName( const char *pszName );

/*! Types of color interpretations for a GDALColorTable. */
typedef enum
{
  /*! Grayscale (in GDALColorEntry.c1) */                      GPI_Gray=0,
  /*! Red, Green, Blue and Alpha in (in c1, c2, c3 and c4) */  GPI_RGB=1,
  /*! Cyan, Magenta, Yellow and Black (in c1, c2, c3 and c4)*/ GPI_CMYK=2,
  /*! Hue, Lightness and Saturation (in c1, c2, and c3) */     GPI_HLS=3
} GDALPaletteInterp;

const char CPL_DLL *GDALGetPaletteInterpretationName( GDALPaletteInterp );

/* "well known" metadata items. */

/** Metadata item for dataset that indicates the spatial interpretation of a
 *  pixel */
#define GDALMD_AREA_OR_POINT   "AREA_OR_POINT"
/** Value for GDALMD_AREA_OR_POINT that indicates that a pixel represents an
 * area */
#  define GDALMD_AOP_AREA      "Area"
/** Value for GDALMD_AREA_OR_POINT that indicates that a pixel represents a
 * point */
#  define GDALMD_AOP_POINT     "Point"

/* -------------------------------------------------------------------- */
/*      GDAL Specific error codes.                                      */
/*                                                                      */
/*      error codes 100 to 299 reserved for GDAL.                       */
/* -------------------------------------------------------------------- */
#ifndef DOXYGEN_SKIP
#define CPLE_WrongFormat        CPL_STATIC_CAST(CPLErrorNum, 200)
#endif

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

/** Opaque type used for the C bindings of the C++ GDALMajorObject class */
typedef void *GDALMajorObjectH;

/** Opaque type used for the C bindings of the C++ GDALDataset class */
typedef void *GDALDatasetH;

/** Opaque type used for the C bindings of the C++ GDALRasterBand class */
typedef void *GDALRasterBandH;

/** Opaque type used for the C bindings of the C++ GDALDriver class */
typedef void *GDALDriverH;

/** Opaque type used for the C bindings of the C++ GDALColorTable class */
typedef void *GDALColorTableH;

/** Opaque type used for the C bindings of the C++ GDALRasterAttributeTable class */
typedef void *GDALRasterAttributeTableH;

/** Opaque type used for the C bindings of the C++ GDALAsyncReader class */
typedef void *GDALAsyncReaderH;

/** Type to express pixel, line or band spacing. Signed 64 bit integer. */
typedef GIntBig GSpacing;

/** Enumeration giving the class of a GDALExtendedDataType.
 * @since GDAL 3.1
 */
typedef enum {
    /** Numeric value. Based on GDALDataType enumeration */
    GEDTC_NUMERIC,
    /** String value. */
    GEDTC_STRING,
    /** Compound data type. */
    GEDTC_COMPOUND
} GDALExtendedDataTypeClass;

/** Enumeration giving the subtype of a GDALExtendedDataType.
 * @since GDAL 3.4
 */
typedef enum {
    /** None. */
    GEDTST_NONE,
    /** JSon. Only applies to GEDTC_STRING */
    GEDTST_JSON
} GDALExtendedDataTypeSubType;

/** Opaque type for C++ GDALExtendedDataType */
typedef struct GDALExtendedDataTypeHS* GDALExtendedDataTypeH;
/** Opaque type for C++ GDALEDTComponent */
typedef struct GDALEDTComponentHS* GDALEDTComponentH;
/** Opaque type for C++ GDALGroup */
typedef struct GDALGroupHS* GDALGroupH;
/** Opaque type for C++ GDALMDArray */
typedef struct GDALMDArrayHS* GDALMDArrayH;
/** Opaque type for C++ GDALAttribute */
typedef struct GDALAttributeHS* GDALAttributeH;
/** Opaque type for C++ GDALDimension */
typedef struct GDALDimensionHS* GDALDimensionH;

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

/** Long name of the driver */
#define GDAL_DMD_LONGNAME "DMD_LONGNAME"

/** URL (relative to http://gdal.org/) to the help page of the driver */
#define GDAL_DMD_HELPTOPIC "DMD_HELPTOPIC"

/** MIME type handled by the driver. */
#define GDAL_DMD_MIMETYPE "DMD_MIMETYPE"

/** Extension handled by the driver. */
#define GDAL_DMD_EXTENSION "DMD_EXTENSION"

/** Connection prefix to provide as the file name of the open function.
 * Typically set for non-file based drivers. Generally used with open options.
 * @since GDAL 2.0
 */
#define GDAL_DMD_CONNECTION_PREFIX "DMD_CONNECTION_PREFIX"

/** List of (space separated) extensions handled by the driver.
 * @since GDAL 2.0
 */
#define GDAL_DMD_EXTENSIONS "DMD_EXTENSIONS"

/** XML snippet with creation options. */
#define GDAL_DMD_CREATIONOPTIONLIST "DMD_CREATIONOPTIONLIST"

/** XML snippet with multidimensional dataset creation options.
 * @since GDAL 3.1
 */
#define GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST "DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST"

/** XML snippet with multidimensional group creation options.
 * @since GDAL 3.1
 */
#define GDAL_DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST "DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST"

/** XML snippet with multidimensional dimension creation options.
 * @since GDAL 3.1
 */
#define GDAL_DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST "DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST"

/** XML snippet with multidimensional array creation options.
 * @since GDAL 3.1
 */
#define GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST "DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST"

/** XML snippet with multidimensional attribute creation options.
 * @since GDAL 3.1
 */
#define GDAL_DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST "DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST"

/** XML snippet with open options.
 * @since GDAL 2.0
 */
#define GDAL_DMD_OPENOPTIONLIST "DMD_OPENOPTIONLIST"

/** List of (space separated) raster data types support by the Create()/CreateCopy() API. */
#define GDAL_DMD_CREATIONDATATYPES "DMD_CREATIONDATATYPES"

/** List of (space separated) vector field types support by the CreateField() API.
 * @since GDAL 2.0
 * */
#define GDAL_DMD_CREATIONFIELDDATATYPES "DMD_CREATIONFIELDDATATYPES"

/** List of (space separated) vector field sub-types support by the CreateField() API.
 * @since GDAL 2.3
 * */
#define GDAL_DMD_CREATIONFIELDDATASUBTYPES "DMD_CREATIONFIELDDATASUBTYPES"

/** Capability set by a driver that exposes Subdatasets.
 *
 * This capability reflects that a raster driver supports child layers, such as NetCDF
 * or multi-table raster Geopackages.
 *
 * See GDAL_DCAP_MULTIPLE_VECTOR_LAYERS for a similar capability flag
 * for vector drivers.
 */
#define GDAL_DMD_SUBDATASETS "DMD_SUBDATASETS"

/** Capability set by a driver that implements the Open() API. */
#define GDAL_DCAP_OPEN       "DCAP_OPEN"

/** Capability set by a driver that implements the Create() API.
 *
 * If GDAL_DCAP_CREATE is set, but GDAL_DCAP_CREATECOPY not, a generic
 * CreateCopy() implementation is available and will use the Create() API of
 * the driver.
 * So to test if some CreateCopy() implementation is available, generic or
 * specialize, test for both GDAL_DCAP_CREATE and GDAL_DCAP_CREATECOPY.
 */
#define GDAL_DCAP_CREATE     "DCAP_CREATE"

/** Capability set by a driver that implements the CreateMultidimensional() API.
 *
 * @since GDAL 3.1
 */
#define GDAL_DCAP_CREATE_MULTIDIMENSIONAL     "DCAP_CREATE_MULTIDIMENSIONAL"

/** Capability set by a driver that implements the CreateCopy() API.
 *
 * If GDAL_DCAP_CREATECOPY is not defined, but GDAL_DCAP_CREATE is set, a generic
 * CreateCopy() implementation is available and will use the Create() API of
 * the driver.
 * So to test if some CreateCopy() implementation is available, generic or
 * specialize, test for both GDAL_DCAP_CREATE and GDAL_DCAP_CREATECOPY.
 */
#define GDAL_DCAP_CREATECOPY "DCAP_CREATECOPY"

/** Capability set by a driver that implements the CreateCopy() API, but with
 * multidimensional raster as input and output.
 *
 * @since GDAL 3.1
 */
#define GDAL_DCAP_CREATECOPY_MULTIDIMENSIONAL     "DCAP_CREATECOPY_MULTIDIMENSIONAL"

/** Capability set by a driver that supports multidimensional data.
 * @since GDAL 3.1
 */
#define GDAL_DCAP_MULTIDIM_RASTER     "DCAP_MULTIDIM_RASTER"

/** Capability set by a driver that can copy over subdatasets. */
#define GDAL_DCAP_SUBCREATECOPY "DCAP_SUBCREATECOPY"

/** Capability set by a driver that can read/create datasets through the VSI*L API. */
#define GDAL_DCAP_VIRTUALIO  "DCAP_VIRTUALIO"

/** Capability set by a driver having raster capability.
 * @since GDAL 2.0
 */
#define GDAL_DCAP_RASTER     "DCAP_RASTER"

/** Capability set by a driver having vector capability.
 * @since GDAL 2.0
 */
#define GDAL_DCAP_VECTOR     "DCAP_VECTOR"

/** Capability set by a driver having geographical network model capability.
 * @since GDAL 2.1
 */
#define GDAL_DCAP_GNM         "DCAP_GNM"

/** Capability set by a driver that can create fields with NOT NULL constraint.
 * @since GDAL 2.0
 */
#define GDAL_DCAP_NOTNULL_FIELDS "DCAP_NOTNULL_FIELDS"

/** Capability set by a driver that can create fields with UNIQUE constraint.
 * @since GDAL 3.2
 */
#define GDAL_DCAP_UNIQUE_FIELDS "DCAP_UNIQUE_FIELDS"

/** Capability set by a driver that can create fields with DEFAULT values.
 * @since GDAL 2.0
 */
#define GDAL_DCAP_DEFAULT_FIELDS "DCAP_DEFAULT_FIELDS"

/** Capability set by a driver that can create geometry fields with NOT NULL constraint.
 * @since GDAL 2.0
 */
#define GDAL_DCAP_NOTNULL_GEOMFIELDS "DCAP_NOTNULL_GEOMFIELDS"

/** Capability set by a non-spatial driver having no support for geometries. E.g. non-spatial
 * vector drivers (e.g. spreadsheet format drivers) do not support geometries,
 * and accordingly will have this capability present.
 * @since GDAL 2.3
 */
#define GDAL_DCAP_NONSPATIAL     "DCAP_NONSPATIAL"

/** Capability set by drivers which support feature styles.
 * @since GDAL 2.3
 */
#define GDAL_DCAP_FEATURE_STYLES     "DCAP_FEATURE_STYLES"

/** Capability set by drivers which support storing/retrieving coordinate epoch for dynamic CRS
 * @since GDAL 3.4
 */
#define GDAL_DCAP_COORDINATE_EPOCH   "DCAP_COORDINATE_EPOCH"

/** Capability set by drivers for formats which support multiple vector layers.
 *
 * Note: some GDAL drivers expose "virtual" layer support while the underlying formats themselves
 * do not. This capability is only set for drivers of formats which have a native
 * concept of multiple vector layers (such as GeoPackage).
 *
 * @since GDAL 3.4
 */
#define GDAL_DCAP_MULTIPLE_VECTOR_LAYERS "DCAP_MULTIPLE_VECTOR_LAYERS"

/** Value for GDALDimension::GetType() specifying the X axis of a horizontal CRS.
 * @since GDAL 3.1
 */
#define GDAL_DIM_TYPE_HORIZONTAL_X      "HORIZONTAL_X"

/** Value for GDALDimension::GetType() specifying the Y axis of a horizontal CRS.
 * @since GDAL 3.1
 */
#define GDAL_DIM_TYPE_HORIZONTAL_Y      "HORIZONTAL_Y"

/** Value for GDALDimension::GetType() specifying a vertical axis.
 * @since GDAL 3.1
 */
#define GDAL_DIM_TYPE_VERTICAL          "VERTICAL"

/** Value for GDALDimension::GetType() specifying a temporal axis.
 * @since GDAL 3.1
 */
#define GDAL_DIM_TYPE_TEMPORAL          "TEMPORAL"

/** Value for GDALDimension::GetType() specifying a parametric axis.
 * @since GDAL 3.1
 */
#define GDAL_DIM_TYPE_PARAMETRIC        "PARAMETRIC"


void CPL_DLL CPL_STDCALL GDALAllRegister( void );

GDALDatasetH CPL_DLL CPL_STDCALL GDALCreate( GDALDriverH hDriver,
                                 const char *, int, int, int, GDALDataType,
                                 CSLConstList ) CPL_WARN_UNUSED_RESULT;
GDALDatasetH CPL_DLL CPL_STDCALL
GDALCreateCopy( GDALDriverH, const char *, GDALDatasetH,
                int, CSLConstList, GDALProgressFunc, void * ) CPL_WARN_UNUSED_RESULT;

GDALDriverH CPL_DLL CPL_STDCALL GDALIdentifyDriver( const char * pszFilename,
                                            CSLConstList papszFileList );

GDALDriverH CPL_DLL CPL_STDCALL GDALIdentifyDriverEx(
    const char *pszFilename, unsigned int nIdentifyFlags,
    const char *const *papszAllowedDrivers, const char *const *papszFileList);

GDALDatasetH CPL_DLL CPL_STDCALL
GDALOpen( const char *pszFilename, GDALAccess eAccess ) CPL_WARN_UNUSED_RESULT;
GDALDatasetH CPL_DLL CPL_STDCALL GDALOpenShared( const char *, GDALAccess ) CPL_WARN_UNUSED_RESULT;

/* Note: we define GDAL_OF_READONLY and GDAL_OF_UPDATE to be on purpose */
/* equals to GA_ReadOnly and GA_Update */

/** Open in read-only mode.
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_READONLY        0x00

/** Open in update mode.
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_UPDATE          0x01

/** Allow raster and vector drivers to be used.
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_ALL             0x00

/** Allow raster drivers to be used.
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_RASTER          0x02

/** Allow vector drivers to be used.
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_VECTOR          0x04

/** Allow gnm drivers to be used.
 * Used by GDALOpenEx().
 * @since GDAL 2.1
 */
#define     GDAL_OF_GNM             0x08

/** Allow multidimensional raster drivers to be used.
 * Used by GDALOpenEx().
 * @since GDAL 3.1
 */
#define     GDAL_OF_MULTIDIM_RASTER 0x10

#ifndef DOXYGEN_SKIP
#define     GDAL_OF_KIND_MASK       0x1E
#endif

/** Open in shared mode.
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_SHARED          0x20

/** Emit error message in case of failed open.
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_VERBOSE_ERROR   0x40

/** Open as internal dataset. Such dataset isn't registered in the global list
 * of opened dataset. Cannot be used with GDAL_OF_SHARED.
 *
 * Used by GDALOpenEx().
 * @since GDAL 2.0
 */
#define     GDAL_OF_INTERNAL        0x80

/** Let GDAL decide if a array-based or hashset-based storage strategy for
 * cached blocks must be used.
 *
 * GDAL_OF_DEFAULT_BLOCK_ACCESS, GDAL_OF_ARRAY_BLOCK_ACCESS and
 * GDAL_OF_HASHSET_BLOCK_ACCESS are mutually exclusive.
 *
 * Used by GDALOpenEx().
 * @since GDAL 2.1
 */
#define     GDAL_OF_DEFAULT_BLOCK_ACCESS  0

/** Use a array-based storage strategy for cached blocks.
 *
 * GDAL_OF_DEFAULT_BLOCK_ACCESS, GDAL_OF_ARRAY_BLOCK_ACCESS and
 * GDAL_OF_HASHSET_BLOCK_ACCESS are mutually exclusive.
 *
 * Used by GDALOpenEx().
 * @since GDAL 2.1
 */
#define     GDAL_OF_ARRAY_BLOCK_ACCESS    0x100

/** Use a hashset-based storage strategy for cached blocks.
 *
 * GDAL_OF_DEFAULT_BLOCK_ACCESS, GDAL_OF_ARRAY_BLOCK_ACCESS and
 * GDAL_OF_HASHSET_BLOCK_ACCESS are mutually exclusive.
 *
 * Used by GDALOpenEx().
 * @since GDAL 2.1
 */
#define     GDAL_OF_HASHSET_BLOCK_ACCESS  0x200

#ifndef DOXYGEN_SKIP
/* Reserved for a potential future alternative to GDAL_OF_ARRAY_BLOCK_ACCESS
 * and GDAL_OF_HASHSET_BLOCK_ACCESS */
#define     GDAL_OF_RESERVED_1            0x300

/** Mask to detect the block access method */
#define     GDAL_OF_BLOCK_ACCESS_MASK     0x300
#endif

GDALDatasetH CPL_DLL CPL_STDCALL GDALOpenEx( const char* pszFilename,
                                             unsigned int nOpenFlags,
                                             const char* const* papszAllowedDrivers,
                                             const char* const* papszOpenOptions,
                                             const char* const* papszSiblingFiles ) CPL_WARN_UNUSED_RESULT;

int          CPL_DLL CPL_STDCALL GDALDumpOpenDatasets( FILE * );

GDALDriverH CPL_DLL CPL_STDCALL GDALGetDriverByName( const char * );
int CPL_DLL         CPL_STDCALL GDALGetDriverCount( void );
GDALDriverH CPL_DLL CPL_STDCALL GDALGetDriver( int );
GDALDriverH CPL_DLL CPL_STDCALL GDALCreateDriver( void );
void        CPL_DLL CPL_STDCALL GDALDestroyDriver( GDALDriverH );
int         CPL_DLL CPL_STDCALL GDALRegisterDriver( GDALDriverH );
void        CPL_DLL CPL_STDCALL GDALDeregisterDriver( GDALDriverH );
void        CPL_DLL CPL_STDCALL GDALDestroyDriverManager( void );
#ifndef DOXYGEN_SKIP
void        CPL_DLL             GDALDestroy( void );
#endif
CPLErr      CPL_DLL CPL_STDCALL GDALDeleteDataset( GDALDriverH, const char * );
CPLErr      CPL_DLL CPL_STDCALL GDALRenameDataset( GDALDriverH,
                                                   const char * pszNewName,
                                                   const char * pszOldName );
CPLErr      CPL_DLL CPL_STDCALL GDALCopyDatasetFiles( GDALDriverH,
                                                      const char * pszNewName,
                                                      const char * pszOldName);
int         CPL_DLL CPL_STDCALL GDALValidateCreationOptions( GDALDriverH,
                                                             CSLConstList papszCreationOptions);

/* The following are deprecated */
const char CPL_DLL * CPL_STDCALL GDALGetDriverShortName( GDALDriverH );
const char CPL_DLL * CPL_STDCALL GDALGetDriverLongName( GDALDriverH );
const char CPL_DLL * CPL_STDCALL GDALGetDriverHelpTopic( GDALDriverH );
const char CPL_DLL * CPL_STDCALL GDALGetDriverCreationOptionList( GDALDriverH );

/* ==================================================================== */
/*      GDAL_GCP                                                        */
/* ==================================================================== */

/** Ground Control Point */
typedef struct
{
    /** Unique identifier, often numeric */
    char        *pszId;

    /** Informational message or "" */
    char        *pszInfo;

    /** Pixel (x) location of GCP on raster */
    double      dfGCPPixel;
    /** Line (y) location of GCP on raster */
    double      dfGCPLine;

    /** X position of GCP in georeferenced space */
    double      dfGCPX;

    /** Y position of GCP in georeferenced space */
    double      dfGCPY;

    /** Elevation of GCP, or zero if not known */
    double      dfGCPZ;
} GDAL_GCP;

void CPL_DLL CPL_STDCALL GDALInitGCPs( int, GDAL_GCP * );
void CPL_DLL CPL_STDCALL GDALDeinitGCPs( int, GDAL_GCP * );
GDAL_GCP CPL_DLL * CPL_STDCALL GDALDuplicateGCPs( int, const GDAL_GCP * );

int CPL_DLL CPL_STDCALL
GDALGCPsToGeoTransform( int nGCPCount, const GDAL_GCP *pasGCPs,
                        double *padfGeoTransform, int bApproxOK )  CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPL_STDCALL
GDALInvGeoTransform( double *padfGeoTransformIn,
                     double *padfInvGeoTransformOut ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL CPL_STDCALL GDALApplyGeoTransform( double *, double, double,
                                                double *, double * );
void CPL_DLL GDALComposeGeoTransforms(const double *padfGeoTransform1,
                                      const double *padfGeoTransform2,
                                      double *padfGeoTransformOut);

/* ==================================================================== */
/*      major objects (dataset, and, driver, drivermanager).            */
/* ==================================================================== */

char CPL_DLL  ** CPL_STDCALL GDALGetMetadataDomainList( GDALMajorObjectH hObject );
char CPL_DLL  ** CPL_STDCALL GDALGetMetadata( GDALMajorObjectH, const char * );
CPLErr CPL_DLL CPL_STDCALL GDALSetMetadata( GDALMajorObjectH, CSLConstList,
                                            const char * );
const char CPL_DLL * CPL_STDCALL
GDALGetMetadataItem( GDALMajorObjectH, const char *, const char * );
CPLErr CPL_DLL CPL_STDCALL
GDALSetMetadataItem( GDALMajorObjectH, const char *, const char *,
                     const char * );
const char CPL_DLL * CPL_STDCALL GDALGetDescription( GDALMajorObjectH );
void CPL_DLL CPL_STDCALL GDALSetDescription( GDALMajorObjectH, const char * );

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

/** Name of driver metadata item for layer creation option list */
#define GDAL_DS_LAYER_CREATIONOPTIONLIST "DS_LAYER_CREATIONOPTIONLIST"

GDALDriverH CPL_DLL CPL_STDCALL GDALGetDatasetDriver( GDALDatasetH );
char CPL_DLL ** CPL_STDCALL GDALGetFileList( GDALDatasetH );
void CPL_DLL CPL_STDCALL   GDALClose( GDALDatasetH );
int CPL_DLL CPL_STDCALL     GDALGetRasterXSize( GDALDatasetH );
int CPL_DLL CPL_STDCALL     GDALGetRasterYSize( GDALDatasetH );
int CPL_DLL CPL_STDCALL     GDALGetRasterCount( GDALDatasetH );
GDALRasterBandH CPL_DLL CPL_STDCALL GDALGetRasterBand( GDALDatasetH, int );

CPLErr CPL_DLL  CPL_STDCALL GDALAddBand( GDALDatasetH hDS, GDALDataType eType,
                             CSLConstList papszOptions );

GDALAsyncReaderH CPL_DLL CPL_STDCALL
GDALBeginAsyncReader(GDALDatasetH hDS, int nXOff, int nYOff,
                     int nXSize, int nYSize,
                     void *pBuf, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount, int* panBandMap,
                     int nPixelSpace, int nLineSpace, int nBandSpace,
                     CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;

void  CPL_DLL CPL_STDCALL
GDALEndAsyncReader(GDALDatasetH hDS, GDALAsyncReaderH hAsynchReaderH);

CPLErr CPL_DLL CPL_STDCALL GDALDatasetRasterIO(
    GDALDatasetH hDS, GDALRWFlag eRWFlag,
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    void * pBuffer, int nBXSize, int nBYSize, GDALDataType eBDataType,
    int nBandCount, int *panBandCount,
    int nPixelSpace, int nLineSpace, int nBandSpace) CPL_WARN_UNUSED_RESULT;

CPLErr CPL_DLL CPL_STDCALL GDALDatasetRasterIOEx(
    GDALDatasetH hDS, GDALRWFlag eRWFlag,
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    void * pBuffer, int nBXSize, int nBYSize, GDALDataType eBDataType,
    int nBandCount, int *panBandCount,
    GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
    GDALRasterIOExtraArg* psExtraArg) CPL_WARN_UNUSED_RESULT;

CPLErr CPL_DLL CPL_STDCALL GDALDatasetAdviseRead( GDALDatasetH hDS,
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    int nBXSize, int nBYSize, GDALDataType eBDataType,
    int nBandCount, int *panBandCount, CSLConstList papszOptions );

const char CPL_DLL * CPL_STDCALL GDALGetProjectionRef( GDALDatasetH );
OGRSpatialReferenceH CPL_DLL GDALGetSpatialRef( GDALDatasetH );
CPLErr CPL_DLL CPL_STDCALL GDALSetProjection( GDALDatasetH, const char * );
CPLErr CPL_DLL GDALSetSpatialRef( GDALDatasetH, OGRSpatialReferenceH );
CPLErr CPL_DLL CPL_STDCALL GDALGetGeoTransform( GDALDatasetH, double * );
CPLErr CPL_DLL CPL_STDCALL GDALSetGeoTransform( GDALDatasetH, double * );

int CPL_DLL CPL_STDCALL  GDALGetGCPCount( GDALDatasetH );
const char CPL_DLL * CPL_STDCALL GDALGetGCPProjection( GDALDatasetH );
OGRSpatialReferenceH CPL_DLL GDALGetGCPSpatialRef( GDALDatasetH );
const GDAL_GCP CPL_DLL * CPL_STDCALL GDALGetGCPs( GDALDatasetH );
CPLErr CPL_DLL CPL_STDCALL GDALSetGCPs( GDALDatasetH, int, const GDAL_GCP *,
                                        const char * );
CPLErr CPL_DLL GDALSetGCPs2( GDALDatasetH, int, const GDAL_GCP *,
                                         OGRSpatialReferenceH );

void CPL_DLL * CPL_STDCALL GDALGetInternalHandle( GDALDatasetH, const char * );
int CPL_DLL CPL_STDCALL GDALReferenceDataset( GDALDatasetH );
int CPL_DLL CPL_STDCALL GDALDereferenceDataset( GDALDatasetH );
int CPL_DLL CPL_STDCALL GDALReleaseDataset( GDALDatasetH );

CPLErr CPL_DLL CPL_STDCALL
GDALBuildOverviews( GDALDatasetH, const char *, int, int *,
                    int, int *, GDALProgressFunc, void * ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL CPL_STDCALL GDALGetOpenDatasets( GDALDatasetH **hDS, int *pnCount );
int CPL_DLL CPL_STDCALL GDALGetAccess( GDALDatasetH hDS );
void CPL_DLL CPL_STDCALL GDALFlushCache( GDALDatasetH hDS );

CPLErr CPL_DLL CPL_STDCALL
              GDALCreateDatasetMaskBand( GDALDatasetH hDS, int nFlags );

CPLErr CPL_DLL CPL_STDCALL GDALDatasetCopyWholeRaster(
    GDALDatasetH hSrcDS, GDALDatasetH hDstDS, CSLConstList papszOptions,
    GDALProgressFunc pfnProgress, void *pProgressData ) CPL_WARN_UNUSED_RESULT;

CPLErr CPL_DLL CPL_STDCALL GDALRasterBandCopyWholeRaster(
    GDALRasterBandH hSrcBand, GDALRasterBandH hDstBand,
    const char * const * constpapszOptions,
    GDALProgressFunc pfnProgress, void *pProgressData ) CPL_WARN_UNUSED_RESULT;

CPLErr CPL_DLL
GDALRegenerateOverviews( GDALRasterBandH hSrcBand,
                         int nOverviewCount, GDALRasterBandH *pahOverviewBands,
                         const char *pszResampling,
                         GDALProgressFunc pfnProgress, void *pProgressData );

int    CPL_DLL GDALDatasetGetLayerCount( GDALDatasetH );
OGRLayerH CPL_DLL GDALDatasetGetLayer( GDALDatasetH, int );
OGRLayerH CPL_DLL GDALDatasetGetLayerByName( GDALDatasetH, const char * );
int       CPL_DLL GDALDatasetIsLayerPrivate( GDALDatasetH, int );
OGRErr    CPL_DLL GDALDatasetDeleteLayer( GDALDatasetH, int );
OGRLayerH CPL_DLL GDALDatasetCreateLayer( GDALDatasetH, const char *,
                                      OGRSpatialReferenceH, OGRwkbGeometryType,
                                      CSLConstList );
OGRLayerH CPL_DLL GDALDatasetCopyLayer( GDALDatasetH, OGRLayerH, const char *,
                                        CSLConstList );
void CPL_DLL GDALDatasetResetReading( GDALDatasetH );
OGRFeatureH CPL_DLL GDALDatasetGetNextFeature( GDALDatasetH hDS,
                                               OGRLayerH* phBelongingLayer,
                                               double* pdfProgressPct,
                                               GDALProgressFunc pfnProgress,
                                               void* pProgressData );
int    CPL_DLL GDALDatasetTestCapability( GDALDatasetH, const char * );
OGRLayerH CPL_DLL GDALDatasetExecuteSQL( GDALDatasetH, const char *,
                                     OGRGeometryH, const char * );
OGRErr CPL_DLL GDALDatasetAbortSQL( GDALDatasetH );
void   CPL_DLL GDALDatasetReleaseResultSet( GDALDatasetH, OGRLayerH );
OGRStyleTableH CPL_DLL GDALDatasetGetStyleTable( GDALDatasetH );
void   CPL_DLL GDALDatasetSetStyleTableDirectly( GDALDatasetH, OGRStyleTableH );
void   CPL_DLL GDALDatasetSetStyleTable( GDALDatasetH, OGRStyleTableH );
OGRErr CPL_DLL GDALDatasetStartTransaction(GDALDatasetH hDS, int bForce);
OGRErr CPL_DLL GDALDatasetCommitTransaction(GDALDatasetH hDS);
OGRErr CPL_DLL GDALDatasetRollbackTransaction(GDALDatasetH hDS);
void CPL_DLL GDALDatasetClearStatistics(GDALDatasetH hDS);

OGRFieldDomainH CPL_DLL GDALDatasetGetFieldDomain(GDALDatasetH hDS,
                                                  const char* pszName);
bool CPL_DLL GDALDatasetAddFieldDomain(GDALDatasetH hDS,
                                       OGRFieldDomainH hFieldDomain,
                                       char** ppszFailureReason);

/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

/**
 * SRCVAL - Macro which may be used by pixel functions to obtain
 *          a pixel from a source buffer.
 */
#define SRCVAL(papoSource, eSrcType, ii) \
      (eSrcType == GDT_Byte ? \
          CPL_REINTERPRET_CAST(const GByte*,papoSource)[ii] : \
      (eSrcType == GDT_Float32 ? \
          CPL_REINTERPRET_CAST(const float*,papoSource)[ii] : \
      (eSrcType == GDT_Float64 ? \
          CPL_REINTERPRET_CAST(const double*,papoSource)[ii] : \
      (eSrcType == GDT_Int32 ? \
          CPL_REINTERPRET_CAST(const GInt32*,papoSource)[ii] : \
      (eSrcType == GDT_UInt16 ? \
          CPL_REINTERPRET_CAST(const GUInt16*,papoSource)[ii] : \
      (eSrcType == GDT_Int16 ? \
          CPL_REINTERPRET_CAST(const GInt16*,papoSource)[ii] : \
      (eSrcType == GDT_UInt32 ? \
          CPL_REINTERPRET_CAST(const GUInt32*,papoSource)[ii] : \
      (eSrcType == GDT_CInt16 ? \
          CPL_REINTERPRET_CAST(const GInt16*,papoSource)[(ii) * 2] : \
      (eSrcType == GDT_CInt32 ? \
          CPL_REINTERPRET_CAST(const GInt32*,papoSource)[(ii) * 2] : \
      (eSrcType == GDT_CFloat32 ? \
          CPL_REINTERPRET_CAST(const float*,papoSource)[(ii) * 2] : \
      (eSrcType == GDT_CFloat64 ? \
          CPL_REINTERPRET_CAST(const double*,papoSource)[(ii) * 2] : 0)))))))))))

/** Type of functions to pass to GDALAddDerivedBandPixelFunc.
 * @since GDAL 2.2 */
typedef CPLErr
(*GDALDerivedPixelFunc)(void **papoSources, int nSources, void *pData,
                        int nBufXSize, int nBufYSize,
                        GDALDataType eSrcType, GDALDataType eBufType,
                        int nPixelSpace, int nLineSpace);

/** Type of functions to pass to GDALAddDerivedBandPixelFuncWithArgs.
 * @since GDAL 3.4 */
typedef CPLErr
(*GDALDerivedPixelFuncWithArgs)(void **papoSources, int nSources, void *pData,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eSrcType, GDALDataType eBufType,
                                int nPixelSpace, int nLineSpace,
                                CSLConstList papszFunctionArgs);

GDALDataType CPL_DLL CPL_STDCALL GDALGetRasterDataType( GDALRasterBandH );
void CPL_DLL CPL_STDCALL
GDALGetBlockSize( GDALRasterBandH, int * pnXSize, int * pnYSize );

CPLErr CPL_DLL CPL_STDCALL
GDALGetActualBlockSize( GDALRasterBandH, int nXBlockOff, int nYBlockOff,
                        int *pnXValid, int *pnYValid );

CPLErr CPL_DLL CPL_STDCALL GDALRasterAdviseRead( GDALRasterBandH hRB,
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    int nBXSize, int nBYSize, GDALDataType eBDataType, CSLConstList papszOptions );

CPLErr CPL_DLL CPL_STDCALL
GDALRasterIO( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
              int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
              void * pBuffer, int nBXSize, int nBYSize,GDALDataType eBDataType,
              int nPixelSpace, int nLineSpace ) CPL_WARN_UNUSED_RESULT;
CPLErr CPL_DLL CPL_STDCALL
GDALRasterIOEx( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
              int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
              void * pBuffer, int nBXSize, int nBYSize,GDALDataType eBDataType,
              GSpacing nPixelSpace, GSpacing nLineSpace,
              GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;
CPLErr CPL_DLL CPL_STDCALL GDALReadBlock( GDALRasterBandH, int, int, void * ) CPL_WARN_UNUSED_RESULT;
CPLErr CPL_DLL CPL_STDCALL GDALWriteBlock( GDALRasterBandH, int, int, void * ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPL_STDCALL GDALGetRasterBandXSize( GDALRasterBandH );
int CPL_DLL CPL_STDCALL GDALGetRasterBandYSize( GDALRasterBandH );
GDALAccess CPL_DLL CPL_STDCALL GDALGetRasterAccess( GDALRasterBandH );
int CPL_DLL CPL_STDCALL GDALGetBandNumber( GDALRasterBandH );
GDALDatasetH CPL_DLL CPL_STDCALL GDALGetBandDataset( GDALRasterBandH );

GDALColorInterp CPL_DLL CPL_STDCALL
GDALGetRasterColorInterpretation( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL
GDALSetRasterColorInterpretation( GDALRasterBandH, GDALColorInterp );
GDALColorTableH CPL_DLL CPL_STDCALL GDALGetRasterColorTable( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterColorTable( GDALRasterBandH, GDALColorTableH );
int CPL_DLL CPL_STDCALL GDALHasArbitraryOverviews( GDALRasterBandH );
int CPL_DLL CPL_STDCALL GDALGetOverviewCount( GDALRasterBandH );
GDALRasterBandH CPL_DLL CPL_STDCALL GDALGetOverview( GDALRasterBandH, int );
double CPL_DLL CPL_STDCALL GDALGetRasterNoDataValue( GDALRasterBandH, int * );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterNoDataValue( GDALRasterBandH, double );
CPLErr CPL_DLL CPL_STDCALL GDALDeleteRasterNoDataValue( GDALRasterBandH );
char CPL_DLL ** CPL_STDCALL GDALGetRasterCategoryNames( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterCategoryNames( GDALRasterBandH, CSLConstList );
double CPL_DLL CPL_STDCALL GDALGetRasterMinimum( GDALRasterBandH, int *pbSuccess );
double CPL_DLL CPL_STDCALL GDALGetRasterMaximum( GDALRasterBandH, int *pbSuccess );
CPLErr CPL_DLL CPL_STDCALL GDALGetRasterStatistics(
    GDALRasterBandH, int bApproxOK, int bForce,
    double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev );
CPLErr CPL_DLL CPL_STDCALL GDALComputeRasterStatistics(
    GDALRasterBandH, int bApproxOK,
    double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev,
    GDALProgressFunc pfnProgress, void *pProgressData );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterStatistics(
    GDALRasterBandH hBand,
    double dfMin, double dfMax, double dfMean, double dfStdDev );

GDALMDArrayH CPL_DLL GDALRasterBandAsMDArray(GDALRasterBandH) CPL_WARN_UNUSED_RESULT;

const char CPL_DLL * CPL_STDCALL GDALGetRasterUnitType( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterUnitType( GDALRasterBandH hBand, const char *pszNewValue );
double CPL_DLL CPL_STDCALL GDALGetRasterOffset( GDALRasterBandH, int *pbSuccess );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterOffset( GDALRasterBandH hBand, double dfNewOffset);
double CPL_DLL CPL_STDCALL GDALGetRasterScale( GDALRasterBandH, int *pbSuccess );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterScale( GDALRasterBandH hBand, double dfNewOffset );
void CPL_DLL CPL_STDCALL
GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK,
                         double adfMinMax[2] );
CPLErr CPL_DLL CPL_STDCALL GDALFlushRasterCache( GDALRasterBandH hBand );
CPLErr CPL_DLL CPL_STDCALL GDALGetRasterHistogram( GDALRasterBandH hBand,
                                       double dfMin, double dfMax,
                                       int nBuckets, int *panHistogram,
                                       int bIncludeOutOfRange, int bApproxOK,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData )
/*! @cond Doxygen_Suppress */
    CPL_WARN_DEPRECATED("Use GDALGetRasterHistogramEx() instead")
/*! @endcond */
    ;
CPLErr CPL_DLL CPL_STDCALL GDALGetRasterHistogramEx( GDALRasterBandH hBand,
                                       double dfMin, double dfMax,
                                       int nBuckets, GUIntBig *panHistogram,
                                       int bIncludeOutOfRange, int bApproxOK,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
CPLErr CPL_DLL CPL_STDCALL GDALGetDefaultHistogram( GDALRasterBandH hBand,
                                       double *pdfMin, double *pdfMax,
                                       int *pnBuckets, int **ppanHistogram,
                                       int bForce,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData )
/*! @cond Doxygen_Suppress */
    CPL_WARN_DEPRECATED("Use GDALGetDefaultHistogramEx() instead")
/*! @endcond */
    ;
CPLErr CPL_DLL CPL_STDCALL GDALGetDefaultHistogramEx( GDALRasterBandH hBand,
                                       double *pdfMin, double *pdfMax,
                                       int *pnBuckets, GUIntBig **ppanHistogram,
                                       int bForce,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
CPLErr CPL_DLL CPL_STDCALL GDALSetDefaultHistogram( GDALRasterBandH hBand,
                                       double dfMin, double dfMax,
                                       int nBuckets, int *panHistogram )
/*! @cond Doxygen_Suppress */
    CPL_WARN_DEPRECATED("Use GDALSetDefaultHistogramEx() instead")
/*! @endcond */
    ;
CPLErr CPL_DLL CPL_STDCALL GDALSetDefaultHistogramEx( GDALRasterBandH hBand,
                                       double dfMin, double dfMax,
                                       int nBuckets, GUIntBig *panHistogram );
int CPL_DLL CPL_STDCALL
GDALGetRandomRasterSample( GDALRasterBandH, int, float * );
GDALRasterBandH CPL_DLL CPL_STDCALL
GDALGetRasterSampleOverview( GDALRasterBandH, int );
GDALRasterBandH CPL_DLL CPL_STDCALL
GDALGetRasterSampleOverviewEx( GDALRasterBandH, GUIntBig );
CPLErr CPL_DLL CPL_STDCALL GDALFillRaster( GDALRasterBandH hBand,
                          double dfRealValue, double dfImaginaryValue );
CPLErr CPL_DLL CPL_STDCALL
GDALComputeBandStats( GDALRasterBandH hBand, int nSampleStep,
                             double *pdfMean, double *pdfStdDev,
                             GDALProgressFunc pfnProgress,
                             void *pProgressData );
CPLErr CPL_DLL  GDALOverviewMagnitudeCorrection( GDALRasterBandH hBaseBand,
                                        int nOverviewCount,
                                        GDALRasterBandH *pahOverviews,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData );

GDALRasterAttributeTableH CPL_DLL CPL_STDCALL GDALGetDefaultRAT(
    GDALRasterBandH hBand );
CPLErr CPL_DLL CPL_STDCALL GDALSetDefaultRAT( GDALRasterBandH,
                                              GDALRasterAttributeTableH );
CPLErr CPL_DLL CPL_STDCALL GDALAddDerivedBandPixelFunc( const char *pszName,
                                    GDALDerivedPixelFunc pfnPixelFunc );
CPLErr CPL_DLL CPL_STDCALL GDALAddDerivedBandPixelFuncWithArgs( const char *pszName,
                                                                GDALDerivedPixelFuncWithArgs pfnPixelFunc,
                                                                const char *pszMetadata);

GDALRasterBandH CPL_DLL CPL_STDCALL GDALGetMaskBand( GDALRasterBandH hBand );
int CPL_DLL CPL_STDCALL GDALGetMaskFlags( GDALRasterBandH hBand );
CPLErr CPL_DLL CPL_STDCALL
                       GDALCreateMaskBand( GDALRasterBandH hBand, int nFlags );

/** Flag returned by GDALGetMaskFlags() to indicate that all pixels are valid */
#define GMF_ALL_VALID     0x01
/** Flag returned by GDALGetMaskFlags() to indicate that the mask band is
 * valid for all bands */
#define GMF_PER_DATASET   0x02
/** Flag returned by GDALGetMaskFlags() to indicate that the mask band is
 * an alpha band */
#define GMF_ALPHA         0x04
/** Flag returned by GDALGetMaskFlags() to indicate that the mask band is
 * computed from nodata values */
#define GMF_NODATA        0x08

/** Flag returned by GDALGetDataCoverageStatus() when the driver does not
 * implement GetDataCoverageStatus(). This flag should be returned together
 * with GDAL_DATA_COVERAGE_STATUS_DATA */
#define GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED 0x01

/** Flag returned by GDALGetDataCoverageStatus() when there is (potentially)
 * data in the queried window. Can be combined with the binary or operator
 * with GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED or
 * GDAL_DATA_COVERAGE_STATUS_EMPTY */
#define GDAL_DATA_COVERAGE_STATUS_DATA          0x02

/** Flag returned by GDALGetDataCoverageStatus() when there is nodata in the
 * queried window. This is typically identified by the concept of missing block
 * in formats that supports it.
 * Can be combined with the binary or operator with
 * GDAL_DATA_COVERAGE_STATUS_DATA */
#define GDAL_DATA_COVERAGE_STATUS_EMPTY         0x04

int CPL_DLL CPL_STDCALL GDALGetDataCoverageStatus( GDALRasterBandH hBand,
                                                   int nXOff, int nYOff,
                                                   int nXSize, int nYSize,
                                                   int nMaskFlagStop,
                                                   double* pdfDataPct );

/* ==================================================================== */
/*     GDALAsyncReader                                                  */
/* ==================================================================== */

GDALAsyncStatusType CPL_DLL CPL_STDCALL
GDALARGetNextUpdatedRegion(GDALAsyncReaderH hARIO, double dfTimeout,
                         int* pnXBufOff, int* pnYBufOff,
                         int* pnXBufSize, int* pnYBufSize );
int CPL_DLL CPL_STDCALL GDALARLockBuffer(GDALAsyncReaderH hARIO,
                                        double dfTimeout);
void CPL_DLL CPL_STDCALL GDALARUnlockBuffer(GDALAsyncReaderH hARIO);

/* -------------------------------------------------------------------- */
/*      Helper functions.                                               */
/* -------------------------------------------------------------------- */
int CPL_DLL CPL_STDCALL GDALGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv,
                                         int nOptions );
void CPL_DLL CPL_STDCALL GDALSwapWords( void *pData, int nWordSize, int nWordCount,
                            int nWordSkip );
void CPL_DLL CPL_STDCALL GDALSwapWordsEx( void *pData, int nWordSize, size_t nWordCount,
                                  int nWordSkip );

void CPL_DLL CPL_STDCALL
    GDALCopyWords( const void * CPL_RESTRICT pSrcData,
                   GDALDataType eSrcType, int nSrcPixelOffset,
                   void * CPL_RESTRICT pDstData,
                   GDALDataType eDstType, int nDstPixelOffset,
                   int nWordCount );

void CPL_DLL CPL_STDCALL
    GDALCopyWords64( const void * CPL_RESTRICT pSrcData,
                     GDALDataType eSrcType, int nSrcPixelOffset,
                     void * CPL_RESTRICT pDstData,
                     GDALDataType eDstType, int nDstPixelOffset,
                     GPtrDiff_t nWordCount );

void CPL_DLL
GDALCopyBits( const GByte *pabySrcData, int nSrcOffset, int nSrcStep,
              GByte *pabyDstData, int nDstOffset, int nDstStep,
              int nBitCount, int nStepCount );

int CPL_DLL CPL_STDCALL GDALLoadWorldFile( const char *, double * );
int CPL_DLL CPL_STDCALL GDALReadWorldFile( const char *, const char *,
                                           double * );
int CPL_DLL CPL_STDCALL GDALWriteWorldFile( const char *, const char *,
                                            double * );
int CPL_DLL CPL_STDCALL GDALLoadTabFile( const char *, double *, char **,
                                         int *, GDAL_GCP ** );
int CPL_DLL CPL_STDCALL GDALReadTabFile( const char *, double *, char **,
                                         int *, GDAL_GCP ** );
int CPL_DLL CPL_STDCALL GDALLoadOziMapFile( const char *, double *, char **,
                                            int *, GDAL_GCP ** );
int CPL_DLL CPL_STDCALL GDALReadOziMapFile( const char *,  double *,
                                            char **, int *, GDAL_GCP ** );

const char CPL_DLL * CPL_STDCALL GDALDecToDMS( double, const char *, int );
double CPL_DLL CPL_STDCALL GDALPackedDMSToDec( double );
double CPL_DLL CPL_STDCALL GDALDecToPackedDMS( double );

/* Note to developers : please keep this section in sync with ogr_core.h */

#ifndef GDAL_VERSION_INFO_DEFINED
#ifndef DOXYGEN_SKIP
#define GDAL_VERSION_INFO_DEFINED
#endif
const char CPL_DLL * CPL_STDCALL GDALVersionInfo( const char * );
#endif

#ifndef GDAL_CHECK_VERSION

int CPL_DLL CPL_STDCALL GDALCheckVersion( int nVersionMajor, int nVersionMinor,
                                          const char* pszCallingComponentName);

/** Helper macro for GDALCheckVersion()
  @see GDALCheckVersion()
  */
#define GDAL_CHECK_VERSION(pszCallingComponentName) \
 GDALCheckVersion(GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR, pszCallingComponentName)

#endif

/*! @cond Doxygen_Suppress */
#ifdef GDAL_COMPILATION
#define GDALExtractRPCInfoV1 GDALExtractRPCInfo
#else
#define GDALRPCInfo        GDALRPCInfoV2
#define GDALExtractRPCInfo GDALExtractRPCInfoV2
#endif

/* Deprecated: use GDALRPCInfoV2 */
typedef struct
{
    double dfLINE_OFF;   /*!< Line offset */
    double dfSAMP_OFF;   /*!< Sample/Pixel offset */
    double dfLAT_OFF;    /*!< Latitude offset */
    double dfLONG_OFF;   /*!< Longitude offset */
    double dfHEIGHT_OFF; /*!< Height offset */

    double dfLINE_SCALE;   /*!< Line scale */
    double dfSAMP_SCALE;   /*!< Sample/Pixel scale */
    double dfLAT_SCALE;    /*!< Latitude scale */
    double dfLONG_SCALE;   /*!< Longitude scale */
    double dfHEIGHT_SCALE; /*!< Height scale */

    double adfLINE_NUM_COEFF[20]; /*!< Line Numerator Coefficients */
    double adfLINE_DEN_COEFF[20]; /*!< Line Denominator Coefficients */
    double adfSAMP_NUM_COEFF[20]; /*!< Sample/Pixel Numerator Coefficients */
    double adfSAMP_DEN_COEFF[20]; /*!< Sample/Pixel Denominator Coefficients */

    double dfMIN_LONG; /*!< Minimum longitude */
    double dfMIN_LAT;  /*!< Minimum latitude */
    double dfMAX_LONG; /*!< Maximum longitude */
    double dfMAX_LAT;  /*!< Maximum latitude */
} GDALRPCInfoV1;
/*! @endcond */

/** Structure to store Rational Polynomial Coefficients / Rigorous Projection
 * Model. See http://geotiff.maptools.org/rpc_prop.html */
typedef struct
{
    double dfLINE_OFF;   /*!< Line offset */
    double dfSAMP_OFF;   /*!< Sample/Pixel offset */
    double dfLAT_OFF;    /*!< Latitude offset */
    double dfLONG_OFF;   /*!< Longitude offset */
    double dfHEIGHT_OFF; /*!< Height offset */

    double dfLINE_SCALE;   /*!< Line scale */
    double dfSAMP_SCALE;   /*!< Sample/Pixel scale */
    double dfLAT_SCALE;    /*!< Latitude scale */
    double dfLONG_SCALE;   /*!< Longitude scale */
    double dfHEIGHT_SCALE; /*!< Height scale */

    double adfLINE_NUM_COEFF[20]; /*!< Line Numerator Coefficients */
    double adfLINE_DEN_COEFF[20]; /*!< Line Denominator Coefficients */
    double adfSAMP_NUM_COEFF[20]; /*!< Sample/Pixel Numerator Coefficients */
    double adfSAMP_DEN_COEFF[20]; /*!< Sample/Pixel Denominator Coefficients */

    double dfMIN_LONG; /*!< Minimum longitude */
    double dfMIN_LAT;  /*!< Minimum latitude */
    double dfMAX_LONG; /*!< Maximum longitude */
    double dfMAX_LAT;  /*!< Maximum latitude */

    /* Those fields should be at the end. And all above fields should be the same as in GDALRPCInfoV1 */
    double dfERR_BIAS;   /*!< Bias error */
    double dfERR_RAND;   /*!< Random error */
} GDALRPCInfoV2;

/*! @cond Doxygen_Suppress */
int CPL_DLL CPL_STDCALL GDALExtractRPCInfoV1( CSLConstList, GDALRPCInfoV1 * );
/*! @endcond */
int CPL_DLL CPL_STDCALL GDALExtractRPCInfoV2( CSLConstList, GDALRPCInfoV2 * );

/* ==================================================================== */
/*      Color tables.                                                   */
/* ==================================================================== */

/** Color tuple */
typedef struct
{
    /*! gray, red, cyan or hue */
    short      c1;

    /*! green, magenta, or lightness */
    short      c2;

    /*! blue, yellow, or saturation */
    short      c3;

    /*! alpha or blackband */
    short      c4;
} GDALColorEntry;

GDALColorTableH CPL_DLL CPL_STDCALL GDALCreateColorTable( GDALPaletteInterp ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL CPL_STDCALL GDALDestroyColorTable( GDALColorTableH );
GDALColorTableH CPL_DLL CPL_STDCALL GDALCloneColorTable( GDALColorTableH );
GDALPaletteInterp CPL_DLL CPL_STDCALL GDALGetPaletteInterpretation( GDALColorTableH );
int CPL_DLL CPL_STDCALL GDALGetColorEntryCount( GDALColorTableH );
const GDALColorEntry CPL_DLL * CPL_STDCALL GDALGetColorEntry( GDALColorTableH, int );
int CPL_DLL CPL_STDCALL GDALGetColorEntryAsRGB( GDALColorTableH, int, GDALColorEntry *);
void CPL_DLL CPL_STDCALL GDALSetColorEntry( GDALColorTableH, int, const GDALColorEntry * );
void CPL_DLL CPL_STDCALL GDALCreateColorRamp( GDALColorTableH hTable,
            int nStartIndex, const GDALColorEntry *psStartColor,
            int nEndIndex, const GDALColorEntry *psEndColor );

/* ==================================================================== */
/*      Raster Attribute Table                                          */
/* ==================================================================== */

/** Field type of raster attribute table */
typedef enum {
    /*! Integer field */                   GFT_Integer,
    /*! Floating point (double) field */   GFT_Real,
    /*! String field */                    GFT_String
} GDALRATFieldType;

/** Field usage of raster attribute table */
typedef enum {
    /*! General purpose field. */          GFU_Generic = 0,
    /*! Histogram pixel count */           GFU_PixelCount = 1,
    /*! Class name */                      GFU_Name = 2,
    /*! Class range minimum */             GFU_Min = 3,
    /*! Class range maximum */             GFU_Max = 4,
    /*! Class value (min=max) */           GFU_MinMax = 5,
    /*! Red class color (0-255) */         GFU_Red = 6,
    /*! Green class color (0-255) */       GFU_Green = 7,
    /*! Blue class color (0-255) */        GFU_Blue = 8,
    /*! Alpha (0=transparent,255=opaque)*/ GFU_Alpha = 9,
    /*! Color Range Red Minimum */         GFU_RedMin = 10,
    /*! Color Range Green Minimum */       GFU_GreenMin = 11,
    /*! Color Range Blue Minimum */        GFU_BlueMin = 12,
    /*! Color Range Alpha Minimum */       GFU_AlphaMin = 13,
    /*! Color Range Red Maximum */         GFU_RedMax = 14,
    /*! Color Range Green Maximum */       GFU_GreenMax = 15,
    /*! Color Range Blue Maximum */        GFU_BlueMax = 16,
    /*! Color Range Alpha Maximum */       GFU_AlphaMax = 17,
    /*! Maximum GFU value (equals to GFU_AlphaMax+1 currently) */ GFU_MaxCount
} GDALRATFieldUsage;

/** RAT table type (thematic or athematic)
  * @since GDAL 2.4
  */
typedef enum {
    /*! Thematic table type */            GRTT_THEMATIC,
    /*! Athematic table type */           GRTT_ATHEMATIC
} GDALRATTableType;

GDALRasterAttributeTableH CPL_DLL CPL_STDCALL
                                           GDALCreateRasterAttributeTable(void) CPL_WARN_UNUSED_RESULT;

void CPL_DLL CPL_STDCALL GDALDestroyRasterAttributeTable(
    GDALRasterAttributeTableH );

int CPL_DLL CPL_STDCALL GDALRATGetColumnCount( GDALRasterAttributeTableH );

const char CPL_DLL * CPL_STDCALL GDALRATGetNameOfCol(
    GDALRasterAttributeTableH, int );
GDALRATFieldUsage CPL_DLL CPL_STDCALL GDALRATGetUsageOfCol(
    GDALRasterAttributeTableH, int );
GDALRATFieldType CPL_DLL CPL_STDCALL GDALRATGetTypeOfCol(
    GDALRasterAttributeTableH, int );

int CPL_DLL CPL_STDCALL GDALRATGetColOfUsage( GDALRasterAttributeTableH,
                                              GDALRATFieldUsage );
int CPL_DLL CPL_STDCALL GDALRATGetRowCount( GDALRasterAttributeTableH );

const char CPL_DLL * CPL_STDCALL GDALRATGetValueAsString(
    GDALRasterAttributeTableH, int, int);
int CPL_DLL CPL_STDCALL GDALRATGetValueAsInt(
    GDALRasterAttributeTableH, int, int);
double CPL_DLL CPL_STDCALL GDALRATGetValueAsDouble(
    GDALRasterAttributeTableH, int, int);

void CPL_DLL CPL_STDCALL GDALRATSetValueAsString( GDALRasterAttributeTableH, int, int,
                                                  const char * );
void CPL_DLL CPL_STDCALL GDALRATSetValueAsInt( GDALRasterAttributeTableH, int, int,
                                               int );
void CPL_DLL CPL_STDCALL GDALRATSetValueAsDouble( GDALRasterAttributeTableH, int, int,
                                                  double );

int CPL_DLL CPL_STDCALL GDALRATChangesAreWrittenToFile( GDALRasterAttributeTableH hRAT );

CPLErr CPL_DLL CPL_STDCALL GDALRATValuesIOAsDouble( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag,
                                        int iField, int iStartRow, int iLength, double *pdfData );
CPLErr CPL_DLL CPL_STDCALL GDALRATValuesIOAsInteger( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag,
                                        int iField, int iStartRow, int iLength, int *pnData);
CPLErr CPL_DLL CPL_STDCALL GDALRATValuesIOAsString( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag,
                                        int iField, int iStartRow, int iLength, CSLConstList papszStrList);

void CPL_DLL CPL_STDCALL GDALRATSetRowCount( GDALRasterAttributeTableH,
                                             int );
CPLErr CPL_DLL CPL_STDCALL GDALRATCreateColumn( GDALRasterAttributeTableH,
                                                const char *,
                                                GDALRATFieldType,
                                                GDALRATFieldUsage );
CPLErr CPL_DLL CPL_STDCALL GDALRATSetLinearBinning( GDALRasterAttributeTableH,
                                                    double, double );
int CPL_DLL CPL_STDCALL GDALRATGetLinearBinning( GDALRasterAttributeTableH,
                                                 double *, double * );
CPLErr CPL_DLL CPL_STDCALL GDALRATSetTableType( GDALRasterAttributeTableH hRAT,
                         const GDALRATTableType eInTableType );
GDALRATTableType CPL_DLL CPL_STDCALL GDALRATGetTableType( GDALRasterAttributeTableH hRAT);
CPLErr CPL_DLL CPL_STDCALL GDALRATInitializeFromColorTable(
    GDALRasterAttributeTableH, GDALColorTableH );
GDALColorTableH CPL_DLL CPL_STDCALL GDALRATTranslateToColorTable(
    GDALRasterAttributeTableH, int nEntryCount );
void CPL_DLL CPL_STDCALL GDALRATDumpReadable( GDALRasterAttributeTableH,
                                              FILE * );
GDALRasterAttributeTableH CPL_DLL CPL_STDCALL
    GDALRATClone( const GDALRasterAttributeTableH );

void CPL_DLL* CPL_STDCALL
    GDALRATSerializeJSON( GDALRasterAttributeTableH ) CPL_WARN_UNUSED_RESULT;

int CPL_DLL CPL_STDCALL GDALRATGetRowOfValue( GDALRasterAttributeTableH, double );
void CPL_DLL CPL_STDCALL GDALRATRemoveStatistics( GDALRasterAttributeTableH );

/* ==================================================================== */
/*      GDAL Cache Management                                           */
/* ==================================================================== */

void CPL_DLL CPL_STDCALL GDALSetCacheMax( int nBytes );
int CPL_DLL CPL_STDCALL GDALGetCacheMax(void);
int CPL_DLL CPL_STDCALL GDALGetCacheUsed(void);
void CPL_DLL CPL_STDCALL GDALSetCacheMax64( GIntBig nBytes );
GIntBig CPL_DLL CPL_STDCALL GDALGetCacheMax64(void);
GIntBig CPL_DLL CPL_STDCALL GDALGetCacheUsed64(void);

int CPL_DLL CPL_STDCALL GDALFlushCacheBlock(void);

/* ==================================================================== */
/*      GDAL virtual memory                                             */
/* ==================================================================== */

CPLVirtualMem CPL_DLL* GDALDatasetGetVirtualMem( GDALDatasetH hDS,
                                                 GDALRWFlag eRWFlag,
                                                 int nXOff, int nYOff,
                                                 int nXSize, int nYSize,
                                                 int nBufXSize, int nBufYSize,
                                                 GDALDataType eBufType,
                                                 int nBandCount, int* panBandMap,
                                                 int nPixelSpace,
                                                 GIntBig nLineSpace,
                                                 GIntBig nBandSpace,
                                                 size_t nCacheSize,
                                                 size_t nPageSizeHint,
                                                 int bSingleThreadUsage,
                                                 CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;

CPLVirtualMem CPL_DLL* GDALRasterBandGetVirtualMem( GDALRasterBandH hBand,
                                         GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff,
                                         int nXSize, int nYSize,
                                         int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         int nPixelSpace,
                                         GIntBig nLineSpace,
                                         size_t nCacheSize,
                                         size_t nPageSizeHint,
                                         int bSingleThreadUsage,
                                         CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;

CPLVirtualMem CPL_DLL* GDALGetVirtualMemAuto( GDALRasterBandH hBand,
                                              GDALRWFlag eRWFlag,
                                              int *pnPixelSpace,
                                              GIntBig *pnLineSpace,
                                              CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;

/**! Enumeration to describe the tile organization */
typedef enum
{
    /*! Tile Interleaved by Pixel: tile (0,0) with internal band interleaved by pixel organization, tile (1, 0), ...  */
    GTO_TIP,
    /*! Band Interleaved by Tile : tile (0,0) of first band, tile (0,0) of second band, ... tile (1,0) of first band, tile (1,0) of second band, ... */
    GTO_BIT,
    /*! Band SeQuential : all the tiles of first band, all the tiles of following band... */
    GTO_BSQ
} GDALTileOrganization;

CPLVirtualMem CPL_DLL* GDALDatasetGetTiledVirtualMem( GDALDatasetH hDS,
                                                      GDALRWFlag eRWFlag,
                                                      int nXOff, int nYOff,
                                                      int nXSize, int nYSize,
                                                      int nTileXSize, int nTileYSize,
                                                      GDALDataType eBufType,
                                                      int nBandCount, int* panBandMap,
                                                      GDALTileOrganization eTileOrganization,
                                                      size_t nCacheSize,
                                                      int bSingleThreadUsage,
                                                      CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;

CPLVirtualMem CPL_DLL* GDALRasterBandGetTiledVirtualMem( GDALRasterBandH hBand,
                                                         GDALRWFlag eRWFlag,
                                                         int nXOff, int nYOff,
                                                         int nXSize, int nYSize,
                                                         int nTileXSize, int nTileYSize,
                                                         GDALDataType eBufType,
                                                         size_t nCacheSize,
                                                         int bSingleThreadUsage,
                                                         CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;

/* ==================================================================== */
/*      VRTPansharpenedDataset class.                                   */
/* ==================================================================== */

GDALDatasetH CPL_DLL GDALCreatePansharpenedVRT( const char* pszXML,
                                            GDALRasterBandH hPanchroBand,
                                            int nInputSpectralBands,
                                            GDALRasterBandH* pahInputSpectralBands ) CPL_WARN_UNUSED_RESULT;

/* =================================================================== */
/*      Misc API                                                        */
/* ==================================================================== */

CPLXMLNode CPL_DLL* GDALGetJPEG2000Structure(const char* pszFilename,
                                             CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;

/* ==================================================================== */
/*      Multidimensional API_api                                       */
/* ==================================================================== */

GDALDatasetH CPL_DLL GDALCreateMultiDimensional( GDALDriverH hDriver,
                                                 const char * pszName,
                                                 CSLConstList papszRootGroupOptions,
                                                 CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;

GDALExtendedDataTypeH CPL_DLL GDALExtendedDataTypeCreate(GDALDataType eType) CPL_WARN_UNUSED_RESULT;
GDALExtendedDataTypeH CPL_DLL GDALExtendedDataTypeCreateString(size_t nMaxStringLength) CPL_WARN_UNUSED_RESULT;
GDALExtendedDataTypeH CPL_DLL GDALExtendedDataTypeCreateStringEx(size_t nMaxStringLength, GDALExtendedDataTypeSubType eSubType) CPL_WARN_UNUSED_RESULT;
GDALExtendedDataTypeH CPL_DLL GDALExtendedDataTypeCreateCompound(
    const char* pszName, size_t nTotalSize,
    size_t nComponents, const GDALEDTComponentH* comps) CPL_WARN_UNUSED_RESULT;
void CPL_DLL GDALExtendedDataTypeRelease(GDALExtendedDataTypeH hEDT);
const char CPL_DLL* GDALExtendedDataTypeGetName(GDALExtendedDataTypeH hEDT);
GDALExtendedDataTypeClass CPL_DLL GDALExtendedDataTypeGetClass(GDALExtendedDataTypeH hEDT);
GDALDataType CPL_DLL GDALExtendedDataTypeGetNumericDataType(GDALExtendedDataTypeH hEDT);
size_t CPL_DLL GDALExtendedDataTypeGetSize(GDALExtendedDataTypeH hEDT);
size_t CPL_DLL GDALExtendedDataTypeGetMaxStringLength(GDALExtendedDataTypeH hEDT);
GDALEDTComponentH CPL_DLL *GDALExtendedDataTypeGetComponents(GDALExtendedDataTypeH hEDT, size_t* pnCount) CPL_WARN_UNUSED_RESULT;
void CPL_DLL GDALExtendedDataTypeFreeComponents(GDALEDTComponentH* components, size_t nCount);
int CPL_DLL GDALExtendedDataTypeCanConvertTo(GDALExtendedDataTypeH hSourceEDT,
                                             GDALExtendedDataTypeH hTargetEDT);
int CPL_DLL GDALExtendedDataTypeEquals(GDALExtendedDataTypeH hFirstEDT,
                                       GDALExtendedDataTypeH hSecondEDT);
GDALExtendedDataTypeSubType CPL_DLL GDALExtendedDataTypeGetSubType(GDALExtendedDataTypeH hEDT);

GDALEDTComponentH CPL_DLL GDALEDTComponentCreate(const char* pszName, size_t nOffset, GDALExtendedDataTypeH hType) CPL_WARN_UNUSED_RESULT;
void CPL_DLL GDALEDTComponentRelease(GDALEDTComponentH hComp);
const char CPL_DLL* GDALEDTComponentGetName(GDALEDTComponentH hComp);
size_t CPL_DLL GDALEDTComponentGetOffset(GDALEDTComponentH hComp);
GDALExtendedDataTypeH CPL_DLL GDALEDTComponentGetType(GDALEDTComponentH hComp) CPL_WARN_UNUSED_RESULT;

GDALGroupH CPL_DLL GDALDatasetGetRootGroup(GDALDatasetH hDS) CPL_WARN_UNUSED_RESULT;
void CPL_DLL GDALGroupRelease(GDALGroupH hGroup);
const char CPL_DLL *GDALGroupGetName(GDALGroupH hGroup);
const char CPL_DLL *GDALGroupGetFullName(GDALGroupH hGroup);
char CPL_DLL **GDALGroupGetMDArrayNames(GDALGroupH hGroup, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALMDArrayH CPL_DLL GDALGroupOpenMDArray(GDALGroupH hGroup, const char* pszMDArrayName, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALMDArrayH CPL_DLL GDALGroupOpenMDArrayFromFullname(GDALGroupH hGroup, const char* pszMDArrayName, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALMDArrayH CPL_DLL  GDALGroupResolveMDArray(GDALGroupH hGroup,
                                     const char* pszName,
                                     const char* pszStartingPoint,
                                     CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **GDALGroupGetGroupNames(GDALGroupH hGroup, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALGroupH CPL_DLL GDALGroupOpenGroup(GDALGroupH hGroup, const char* pszSubGroupName, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALGroupH CPL_DLL GDALGroupOpenGroupFromFullname(GDALGroupH hGroup, const char* pszMDArrayName, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **GDALGroupGetVectorLayerNames(GDALGroupH hGroup, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
OGRLayerH CPL_DLL GDALGroupOpenVectorLayer(GDALGroupH hGroup, const char* pszVectorLayerName, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALDimensionH CPL_DLL *GDALGroupGetDimensions(GDALGroupH hGroup, size_t* pnCount, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALAttributeH CPL_DLL GDALGroupGetAttribute(GDALGroupH hGroup, const char* pszName) CPL_WARN_UNUSED_RESULT;
GDALAttributeH CPL_DLL *GDALGroupGetAttributes(GDALGroupH hGroup, size_t* pnCount, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
CSLConstList CPL_DLL GDALGroupGetStructuralInfo(GDALGroupH hGroup);
GDALGroupH CPL_DLL GDALGroupCreateGroup(GDALGroupH hGroup,
                                        const char* pszSubGroupName,
                                        CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALDimensionH CPL_DLL GDALGroupCreateDimension(GDALGroupH hGroup,
                                                const char* pszName,
                                                const char* pszType,
                                                const char* pszDirection,
                                                GUInt64 nSize,
                                                CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALMDArrayH CPL_DLL GDALGroupCreateMDArray(GDALGroupH hGroup,
                                           const char* pszName,
                                           size_t nDimensions,
                                           GDALDimensionH* pahDimensions,
                                           GDALExtendedDataTypeH hEDT,
                                           CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALAttributeH CPL_DLL GDALGroupCreateAttribute(GDALGroupH hGroup,
                                                const char* pszName,
                                                size_t nDimensions,
                                                const GUInt64* panDimensions,
                                                GDALExtendedDataTypeH hEDT,
                                                CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;

void CPL_DLL GDALMDArrayRelease(GDALMDArrayH hMDArray);
const char CPL_DLL* GDALMDArrayGetName(GDALMDArrayH hArray);
const char CPL_DLL* GDALMDArrayGetFullName(GDALMDArrayH hArray);
GUInt64 CPL_DLL GDALMDArrayGetTotalElementsCount(GDALMDArrayH hArray);
size_t CPL_DLL GDALMDArrayGetDimensionCount(GDALMDArrayH hArray);
GDALDimensionH CPL_DLL* GDALMDArrayGetDimensions(GDALMDArrayH hArray, size_t *pnCount) CPL_WARN_UNUSED_RESULT;
GDALExtendedDataTypeH CPL_DLL GDALMDArrayGetDataType(GDALMDArrayH hArray) CPL_WARN_UNUSED_RESULT;
int CPL_DLL GDALMDArrayRead(GDALMDArrayH hArray,
                            const GUInt64* arrayStartIdx,
                            const size_t* count,
                            const GInt64* arrayStep,
                            const GPtrDiff_t* bufferStride,
                            GDALExtendedDataTypeH bufferDatatype,
                            void* pDstBuffer,
                            const void* pDstBufferAllocStart,
                            size_t nDstBufferllocSize);
int CPL_DLL GDALMDArrayWrite(GDALMDArrayH hArray,
                            const GUInt64* arrayStartIdx,
                            const size_t* count,
                            const GInt64* arrayStep,
                            const GPtrDiff_t* bufferStride,
                            GDALExtendedDataTypeH bufferDatatype,
                            const void* pSrcBuffer,
                            const void* psrcBufferAllocStart,
                            size_t nSrcBufferllocSize);
int CPL_DLL GDALMDArrayAdviseRead(GDALMDArrayH hArray,
                                  const GUInt64* arrayStartIdx,
                                  const size_t* count);
int CPL_DLL GDALMDArrayAdviseReadEx(GDALMDArrayH hArray,
                                    const GUInt64* arrayStartIdx,
                                    const size_t* count,
                                    CSLConstList papszOptions);
GDALAttributeH CPL_DLL GDALMDArrayGetAttribute(GDALMDArrayH hArray, const char* pszName) CPL_WARN_UNUSED_RESULT;
GDALAttributeH CPL_DLL *GDALMDArrayGetAttributes(GDALMDArrayH hArray, size_t* pnCount, CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
GDALAttributeH CPL_DLL GDALMDArrayCreateAttribute(GDALMDArrayH hArray,
                                                const char* pszName,
                                                size_t nDimensions,
                                                const GUInt64* panDimensions,
                                                GDALExtendedDataTypeH hEDT,
                                                CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
const void CPL_DLL *GDALMDArrayGetRawNoDataValue(GDALMDArrayH hArray);
double CPL_DLL GDALMDArrayGetNoDataValueAsDouble(GDALMDArrayH hArray,
                                                 int* pbHasNoDataValue);
int CPL_DLL GDALMDArraySetRawNoDataValue(GDALMDArrayH hArray, const void*);
int CPL_DLL GDALMDArraySetNoDataValueAsDouble(GDALMDArrayH hArray,
                                              double dfNoDataValue);
int CPL_DLL GDALMDArraySetScale(GDALMDArrayH hArray, double dfScale);
int CPL_DLL GDALMDArraySetScaleEx(GDALMDArrayH hArray, double dfScale, GDALDataType eStorageType);
double CPL_DLL GDALMDArrayGetScale(GDALMDArrayH hArray, int *pbHasValue);
double CPL_DLL GDALMDArrayGetScaleEx(GDALMDArrayH hArray, int *pbHasValue, GDALDataType* peStorageType);
int CPL_DLL GDALMDArraySetOffset(GDALMDArrayH hArray, double dfOffset);
int CPL_DLL GDALMDArraySetOffsetEx(GDALMDArrayH hArray, double dfOffset, GDALDataType eStorageType);
double CPL_DLL GDALMDArrayGetOffset(GDALMDArrayH hArray, int *pbHasValue);
double CPL_DLL GDALMDArrayGetOffsetEx(GDALMDArrayH hArray, int *pbHasValue, GDALDataType* peStorageType);
GUInt64 CPL_DLL *GDALMDArrayGetBlockSize(GDALMDArrayH hArray, size_t *pnCount);
int CPL_DLL GDALMDArraySetUnit(GDALMDArrayH hArray, const char*);
const char CPL_DLL *GDALMDArrayGetUnit(GDALMDArrayH hArray);
int CPL_DLL GDALMDArraySetSpatialRef( GDALMDArrayH, OGRSpatialReferenceH );
OGRSpatialReferenceH CPL_DLL GDALMDArrayGetSpatialRef(GDALMDArrayH hArray);
size_t CPL_DLL *GDALMDArrayGetProcessingChunkSize(GDALMDArrayH hArray, size_t *pnCount,
                                        size_t nMaxChunkMemory);
CSLConstList CPL_DLL GDALMDArrayGetStructuralInfo(GDALMDArrayH hArray);
GDALMDArrayH CPL_DLL GDALMDArrayGetView(GDALMDArrayH hArray, const char* pszViewExpr);
GDALMDArrayH CPL_DLL GDALMDArrayTranspose(GDALMDArrayH hArray,
                                            size_t nNewAxisCount,
                                            const int *panMapNewAxisToOldAxis);
GDALMDArrayH CPL_DLL GDALMDArrayGetUnscaled(GDALMDArrayH hArray);
GDALMDArrayH CPL_DLL GDALMDArrayGetMask(GDALMDArrayH hArray, CSLConstList papszOptions);
GDALDatasetH CPL_DLL GDALMDArrayAsClassicDataset(GDALMDArrayH hArray,
                                                 size_t iXDim, size_t iYDim);
CPLErr CPL_DLL GDALMDArrayGetStatistics(
    GDALMDArrayH hArray, GDALDatasetH, int bApproxOK, int bForce,
    double *pdfMin, double *pdfMax,
    double *pdfMean, double *pdfStdDev,
    GUInt64* pnValidCount,
    GDALProgressFunc pfnProgress, void *pProgressData );
int CPL_DLL GDALMDArrayComputeStatistics( GDALMDArrayH hArray, GDALDatasetH,
                                    int bApproxOK,
                                    double *pdfMin, double *pdfMax,
                                    double *pdfMean, double *pdfStdDev,
                                    GUInt64* pnValidCount,
                                    GDALProgressFunc, void *pProgressData );
GDALMDArrayH CPL_DLL GDALMDArrayGetResampled(GDALMDArrayH hArray,
                                     size_t nNewDimCount,
                                     const GDALDimensionH* pahNewDims,
                                     GDALRIOResampleAlg resampleAlg,
                                     OGRSpatialReferenceH hTargetSRS,
                                     CSLConstList papszOptions);
GDALMDArrayH CPL_DLL* GDALMDArrayGetCoordinateVariables(GDALMDArrayH hArray, size_t *pnCount) CPL_WARN_UNUSED_RESULT;
void CPL_DLL GDALReleaseArrays(GDALMDArrayH* arrays, size_t nCount);
int CPL_DLL GDALMDArrayCache( GDALMDArrayH hArray, CSLConstList papszOptions );

void CPL_DLL GDALAttributeRelease(GDALAttributeH hAttr);
void CPL_DLL GDALReleaseAttributes(GDALAttributeH* attributes, size_t nCount);
const char CPL_DLL* GDALAttributeGetName(GDALAttributeH hAttr);
const char CPL_DLL* GDALAttributeGetFullName(GDALAttributeH hAttr);
GUInt64 CPL_DLL GDALAttributeGetTotalElementsCount(GDALAttributeH hAttr);
size_t CPL_DLL GDALAttributeGetDimensionCount(GDALAttributeH hAttr);
GUInt64 CPL_DLL* GDALAttributeGetDimensionsSize(GDALAttributeH hAttr, size_t *pnCount) CPL_WARN_UNUSED_RESULT;
GDALExtendedDataTypeH CPL_DLL GDALAttributeGetDataType(GDALAttributeH hAttr) CPL_WARN_UNUSED_RESULT;
GByte CPL_DLL *GDALAttributeReadAsRaw(GDALAttributeH hAttr, size_t *pnSize) CPL_WARN_UNUSED_RESULT;
void CPL_DLL GDALAttributeFreeRawResult(GDALAttributeH hAttr, GByte* raw, size_t nSize);
const char CPL_DLL* GDALAttributeReadAsString(GDALAttributeH hAttr);
int CPL_DLL GDALAttributeReadAsInt(GDALAttributeH hAttr);
double CPL_DLL GDALAttributeReadAsDouble(GDALAttributeH hAttr);
char CPL_DLL **GDALAttributeReadAsStringArray(GDALAttributeH hAttr) CPL_WARN_UNUSED_RESULT;
int CPL_DLL *GDALAttributeReadAsIntArray(GDALAttributeH hAttr, size_t* pnCount) CPL_WARN_UNUSED_RESULT;
double CPL_DLL *GDALAttributeReadAsDoubleArray(GDALAttributeH hAttr, size_t* pnCount) CPL_WARN_UNUSED_RESULT;
int CPL_DLL GDALAttributeWriteRaw(GDALAttributeH hAttr, const void*, size_t);
int CPL_DLL GDALAttributeWriteString(GDALAttributeH hAttr, const char*);
int CPL_DLL GDALAttributeWriteStringArray(GDALAttributeH hAttr, CSLConstList);
int CPL_DLL GDALAttributeWriteInt(GDALAttributeH hAttr, int);
int CPL_DLL GDALAttributeWriteDouble(GDALAttributeH hAttr, double);
int CPL_DLL GDALAttributeWriteDoubleArray(GDALAttributeH hAttr, const double*, size_t);

void CPL_DLL GDALDimensionRelease(GDALDimensionH hDim);
void CPL_DLL GDALReleaseDimensions(GDALDimensionH* dims, size_t nCount);
const char CPL_DLL *GDALDimensionGetName(GDALDimensionH hDim);
const char CPL_DLL *GDALDimensionGetFullName(GDALDimensionH hDim);
const char CPL_DLL *GDALDimensionGetType(GDALDimensionH hDim);
const char CPL_DLL *GDALDimensionGetDirection(GDALDimensionH hDim);
GUInt64 CPL_DLL GDALDimensionGetSize(GDALDimensionH hDim);
GDALMDArrayH CPL_DLL GDALDimensionGetIndexingVariable(GDALDimensionH hDim) CPL_WARN_UNUSED_RESULT;
int CPL_DLL GDALDimensionSetIndexingVariable(GDALDimensionH hDim, GDALMDArrayH hArray);

CPL_C_END

#endif /* ndef GDAL_H_INCLUDED */
