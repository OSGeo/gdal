/******************************************************************************
 * $Id$
 *
 * Name:     gdalconst.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL constant declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
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
 *****************************************************************************/

#ifdef SWIGPYTHON
%nothread;
#endif

#if defined(SWIGCSHARP)
%module GdalConst
#else
%module gdalconst
#endif

#if defined(SWIGJAVA)
%include gdalconst_java.i
#endif

%{
#include "gdal.h"
#include "gdalwarper.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
%}

// GDALDataType
%constant GDT_Unknown   = GDT_Unknown;
%constant GDT_Byte      = GDT_Byte;
%constant GDT_UInt16    = GDT_UInt16;
%constant GDT_Int16     = GDT_Int16;
%constant GDT_UInt32    = GDT_UInt32;
%constant GDT_Int32     = GDT_Int32;
%constant GDT_UInt64    = GDT_UInt64;
%constant GDT_Int64     = GDT_Int64;
%constant GDT_Float32   = GDT_Float32;
%constant GDT_Float64   = GDT_Float64;
%constant GDT_CInt16    = GDT_CInt16;
%constant GDT_CInt32    = GDT_CInt32;
%constant GDT_CFloat32  = GDT_CFloat32;
%constant GDT_CFloat64  = GDT_CFloat64;
%constant GDT_TypeCount = GDT_TypeCount;

// GDALAccess
%constant GA_ReadOnly = GA_ReadOnly;
%constant GA_Update   = GA_Update;

// GDALRWFlag
%constant GF_Read  = GF_Read;
%constant GF_Write = GF_Write;

// GDALRIOResampleAlg
%constant GRIORA_NearestNeighbour = GRIORA_NearestNeighbour;
%constant GRIORA_Bilinear = GRIORA_Bilinear;
%constant GRIORA_Cubic = GRIORA_Cubic;
%constant GRIORA_CubicSpline = GRIORA_CubicSpline;
%constant GRIORA_Lanczos = GRIORA_Lanczos;
%constant GRIORA_Average = GRIORA_Average;
%constant GRIORA_RMS = GRIORA_RMS;
%constant GRIORA_Mode = GRIORA_Mode;
%constant GRIORA_Gauss = GRIORA_Gauss;

// GDALColorInterp
%constant GCI_Undefined     = GCI_Undefined;
%constant GCI_GrayIndex     = GCI_GrayIndex;
%constant GCI_PaletteIndex  = GCI_PaletteIndex;
%constant GCI_RedBand       = GCI_RedBand;
%constant GCI_GreenBand     = GCI_GreenBand;
%constant GCI_BlueBand      = GCI_BlueBand;
%constant GCI_AlphaBand     = GCI_AlphaBand;
%constant GCI_HueBand       = GCI_HueBand;
%constant GCI_SaturationBand= GCI_SaturationBand;
%constant GCI_LightnessBand = GCI_LightnessBand;
%constant GCI_CyanBand      = GCI_CyanBand;
%constant GCI_MagentaBand   = GCI_MagentaBand;
%constant GCI_YellowBand    = GCI_YellowBand;
%constant GCI_BlackBand     = GCI_BlackBand;
%constant GCI_YCbCr_YBand     = GCI_YCbCr_YBand;
%constant GCI_YCbCr_CrBand     = GCI_YCbCr_CrBand;
%constant GCI_YCbCr_CbBand     = GCI_YCbCr_CbBand;

// GDALResampleAlg

%constant GRA_NearestNeighbour = GRA_NearestNeighbour;
%constant GRA_Bilinear         = GRA_Bilinear;
%constant GRA_Cubic            = GRA_Cubic;
%constant GRA_CubicSpline      = GRA_CubicSpline;
%constant GRA_Lanczos          = GRA_Lanczos;
%constant GRA_Average          = GRA_Average;
%constant GRA_RMS = GRA_RMS;
%constant GRA_Mode             = GRA_Mode;
%constant GRA_Max              = GRA_Max;
%constant GRA_Min              = GRA_Min;
%constant GRA_Med              = GRA_Med;
%constant GRA_Q1               = GRA_Q1;
%constant GRA_Q3               = GRA_Q3;
%constant GRA_Sum              = GRA_Sum;

// GDALPaletteInterp
%constant GPI_Gray  = GPI_Gray;
%constant GPI_RGB   = GPI_RGB;
%constant GPI_CMYK  = GPI_CMYK;
%constant GPI_HLS   = GPI_HLS;

%constant CXT_Element   = CXT_Element;
%constant CXT_Text      = CXT_Text;
%constant CXT_Attribute = CXT_Attribute;
%constant CXT_Comment   = CXT_Comment;
%constant CXT_Literal   = CXT_Literal;

// Error classes

%constant CE_None    = CE_None;
%constant CE_Debug   = CE_Debug;
%constant CE_Warning = CE_Warning;
%constant CE_Failure = CE_Failure;
%constant CE_Fatal   = CE_Fatal;

// Error codes

%constant CPLE_None                       = CPLE_None;
%constant CPLE_AppDefined                 = CPLE_AppDefined;
%constant CPLE_OutOfMemory                = CPLE_OutOfMemory;
%constant CPLE_FileIO                     = CPLE_FileIO;
%constant CPLE_OpenFailed                 = CPLE_OpenFailed;
%constant CPLE_IllegalArg                 = CPLE_IllegalArg;
%constant CPLE_NotSupported               = CPLE_NotSupported;
%constant CPLE_AssertionFailed            = CPLE_AssertionFailed;
%constant CPLE_NoWriteAccess              = CPLE_NoWriteAccess;
%constant CPLE_UserInterrupt              = CPLE_UserInterrupt;
%constant CPLE_ObjectNull                 = CPLE_ObjectNull;
%constant CPLE_HttpResponse               = CPLE_HttpResponse;
%constant CPLE_AWSBucketNotFound          = CPLE_AWSBucketNotFound;
%constant CPLE_AWSObjectNotFound          = CPLE_AWSObjectNotFound;
%constant CPLE_AWSAccessDenied            = CPLE_AWSAccessDenied;
%constant CPLE_AWSInvalidCredentials      = CPLE_AWSInvalidCredentials;
%constant CPLE_AWSSignatureDoesNotMatch   = CPLE_AWSSignatureDoesNotMatch;

// Open flags
%constant OF_ALL     = GDAL_OF_ALL;
%constant OF_RASTER = GDAL_OF_RASTER;
%constant OF_VECTOR = GDAL_OF_VECTOR;
%constant OF_GNM = GDAL_OF_GNM;
%constant OF_MULTIDIM_RASTER = GDAL_OF_MULTIDIM_RASTER;
%constant OF_READONLY = GDAL_OF_READONLY;
%constant OF_UPDATE = GDAL_OF_UPDATE;
%constant OF_SHARED = GDAL_OF_SHARED;
%constant OF_VERBOSE_ERROR = GDAL_OF_VERBOSE_ERROR;

#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)

%constant char *DMD_LONGNAME           = GDAL_DMD_LONGNAME;
%constant char *DMD_HELPTOPIC          = GDAL_DMD_HELPTOPIC;
%constant char *DMD_MIMETYPE           = GDAL_DMD_MIMETYPE;
%constant char *DMD_EXTENSION          = GDAL_DMD_EXTENSION;
%constant char *DMD_CONNECTION_PREFIX  = GDAL_DMD_CONNECTION_PREFIX;
%constant char *DMD_EXTENSIONS         = GDAL_DMD_EXTENSIONS;
%constant char *DMD_CREATIONOPTIONLIST = GDAL_DMD_CREATIONOPTIONLIST;
%constant char *DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST         = GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST;
%constant char *DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST         = GDAL_DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST;
%constant char *DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST         = GDAL_DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST;
%constant char *DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST         = GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST;
%constant char *DMD_MULTIDIM_ARRAY_OPENOPTIONLIST         = GDAL_DMD_MULTIDIM_ARRAY_OPENOPTIONLIST;
%constant char *DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST         = GDAL_DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST;
%constant char *DMD_OPENOPTIONLIST         = GDAL_DMD_OPENOPTIONLIST;
%constant char *DMD_CREATIONDATATYPES  = GDAL_DMD_CREATIONDATATYPES;
%constant char *DMD_CREATIONFIELDDATATYPES  = GDAL_DMD_CREATIONFIELDDATATYPES;
%constant char *DMD_CREATIONFIELDDATASUBTYPES  = GDAL_DMD_CREATIONFIELDDATASUBTYPES;
%constant char *DMD_SUBDATASETS        = GDAL_DMD_SUBDATASETS;
%constant char *DMD_CREATION_FIELD_DOMAIN_TYPES    = GDAL_DMD_CREATION_FIELD_DOMAIN_TYPES;
%constant char *DMD_ALTER_GEOM_FIELD_DEFN_FLAGS    = GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS;
%constant char *DMD_SUPPORTED_SQL_DIALECTS    = GDAL_DMD_SUPPORTED_SQL_DIALECTS;

%constant char *DCAP_OPEN       = GDAL_DCAP_OPEN;
%constant char *DCAP_CREATE     = GDAL_DCAP_CREATE;
%constant char *DCAP_CREATE_MULTIDIMENSIONAL     = GDAL_DCAP_CREATE_MULTIDIMENSIONAL;
%constant char *DCAP_CREATECOPY = GDAL_DCAP_CREATECOPY;
%constant char *DCAP_CREATECOPY_MULTIDIMENSIONAL = GDAL_DCAP_CREATECOPY_MULTIDIMENSIONAL;
%constant char *DCAP_MULTIDIM_RASTER = GDAL_DCAP_MULTIDIM_RASTER;
%constant char *DCAP_SUBCREATECOPY = GDAL_DCAP_SUBCREATECOPY;
%constant char *DCAP_VIRTUALIO  = GDAL_DCAP_VIRTUALIO;
%constant char *DCAP_RASTER     = GDAL_DCAP_RASTER;
%constant char *DCAP_VECTOR     = GDAL_DCAP_VECTOR;
%constant char *DCAP_GNM     = GDAL_DCAP_GNM;
%constant char *DCAP_CREATE_LAYER = GDAL_DCAP_CREATE_LAYER;
%constant char *DCAP_DELETE_LAYER = GDAL_DCAP_DELETE_LAYER;
%constant char *DCAP_CREATE_FIELD = GDAL_DCAP_CREATE_FIELD;
%constant char *DCAP_DELETE_FIELD = GDAL_DCAP_DELETE_FIELD;
%constant char *DCAP_REORDER_FIELDS = GDAL_DCAP_REORDER_FIELDS;
%constant char *DMD_ALTER_FIELD_DEFN_FLAGS = GDAL_DMD_ALTER_FIELD_DEFN_FLAGS;
%constant char *DCAP_NOTNULL_FIELDS      = GDAL_DCAP_NOTNULL_FIELDS;
%constant char *DCAP_UNIQUE_FIELDS       = GDAL_DCAP_UNIQUE_FIELDS;
%constant char *DCAP_DEFAULT_FIELDS      = GDAL_DCAP_DEFAULT_FIELDS;
%constant char *DCAP_NOTNULL_GEOMFIELDS  = GDAL_DCAP_NOTNULL_GEOMFIELDS;
%constant char *DCAP_NONSPATIAL  = GDAL_DCAP_NONSPATIAL;
%constant char *DCAP_CURVE_GEOMETRIES  = GDAL_DCAP_CURVE_GEOMETRIES;
%constant char *DCAP_MEASURED_GEOMETRIES  = GDAL_DCAP_MEASURED_GEOMETRIES;
%constant char *DCAP_Z_GEOMETRIES  = GDAL_DCAP_Z_GEOMETRIES;
%constant char *DMD_GEOMETRY_FLAGS  = GDAL_DMD_GEOMETRY_FLAGS;
%constant char *DCAP_FEATURE_STYLES  = GDAL_DCAP_FEATURE_STYLES;
%constant char *DCAP_COORDINATE_EPOCH    = GDAL_DCAP_COORDINATE_EPOCH;
%constant char *DCAP_MULTIPLE_VECTOR_LAYERS    = GDAL_DCAP_MULTIPLE_VECTOR_LAYERS;
%constant char *DCAP_FIELD_DOMAINS    = GDAL_DCAP_FIELD_DOMAINS;
%constant char *DCAP_RELATIONSHIPS    = GDAL_DCAP_RELATIONSHIPS;
%constant char *GDAL_DCAP_CREATE_RELATIONSHIP    = GDAL_DCAP_CREATE_RELATIONSHIP;
%constant char *GDAL_DCAP_DELETE_RELATIONSHIP    = GDAL_DCAP_DELETE_RELATIONSHIP;
%constant char *GDAL_DCAP_UPDATE_RELATIONSHIP    = GDAL_DCAP_UPDATE_RELATIONSHIP;
%constant char *GDAL_DMD_RELATIONSHIP_FLAGS    = GDAL_DMD_RELATIONSHIP_FLAGS;
%constant char *DCAP_RENAME_LAYERS    = GDAL_DCAP_RENAME_LAYERS;

%constant char *DIM_TYPE_HORIZONTAL_X       = GDAL_DIM_TYPE_HORIZONTAL_X;
%constant char *DIM_TYPE_HORIZONTAL_Y       = GDAL_DIM_TYPE_HORIZONTAL_Y;
%constant char *DIM_TYPE_VERTICAL           = GDAL_DIM_TYPE_VERTICAL;
%constant char *DIM_TYPE_TEMPORAL           = GDAL_DIM_TYPE_TEMPORAL;
%constant char *DIM_TYPE_PARAMETRIC         = GDAL_DIM_TYPE_PARAMETRIC;

%constant char *GDsCAddRelationship    = "AddRelationship";
%constant char *GDsCDeleteRelationship = "DeleteRelationship";
%constant char *GDsCUpdateRelationship = "UpdateRelationship";

#else


// NOTE -- prior to 3.6 these constants included a mix of "GDAL_" prefix vs not,
// so to make things consistent we currently add variants both with and
// without the prefix.
// TODO GDAL 4.0: clean this up!

#define DMD_LONGNAME "DMD_LONGNAME"
#define GDAL_DMD_LONGNAME "DMD_LONGNAME"
#define DMD_HELPTOPIC "DMD_HELPTOPIC"
#define GDAL_DMD_HELPTOPIC "DMD_HELPTOPIC"
#define DMD_MIMETYPE "DMD_MIMETYPE"
#define GDAL_DMD_MIMETYPE "DMD_MIMETYPE"
#define DMD_EXTENSION "DMD_EXTENSION"
#define GDAL_DMD_EXTENSION "DMD_EXTENSION"
#define DMD_CONNECTION_PREFIX  "DMD_CONNECTION_PREFIX"
#define GDAL_DMD_CONNECTION_PREFIX  "DMD_CONNECTION_PREFIX"
#define DMD_EXTENSIONS "DMD_EXTENSIONS"
#define GDAL_DMD_EXTENSIONS "DMD_EXTENSIONS"
#define DMD_CREATIONOPTIONLIST "DMD_CREATIONOPTIONLIST"
#define GDAL_DMD_CREATIONOPTIONLIST "DMD_CREATIONOPTIONLIST"
#define DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST "DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST"
#define GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST "DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST"
#define DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST "DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST"
#define GDAL_DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST "DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST"
#define DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST "DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST"
#define GDAL_DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST "DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST"
#define DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST "DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST"
#define GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST "DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST"
#define DMD_MULTIDIM_ARRAY_OPENOPTIONLIST "DMD_MULTIDIM_ARRAY_OPENOPTIONLIST"
#define GDAL_DMD_MULTIDIM_ARRAY_OPENOPTIONLIST "DMD_MULTIDIM_ARRAY_OPENOPTIONLIST"
#define DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST "DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST"
#define GDAL_DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST "DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST"
#define DMD_OPENOPTIONLIST "DMD_OPENOPTIONLIST"
#define GDAL_DMD_OPENOPTIONLIST "DMD_OPENOPTIONLIST"
#define DMD_CREATIONDATATYPES "DMD_CREATIONDATATYPES"
#define GDAL_DMD_CREATIONDATATYPES "DMD_CREATIONDATATYPES"
#define DMD_CREATIONFIELDDATATYPES "DMD_CREATIONFIELDDATATYPES"
#define GDAL_DMD_CREATIONFIELDDATATYPES "DMD_CREATIONFIELDDATATYPES"
#define DMD_CREATIONFIELDDATASUBTYPES "DMD_CREATIONFIELDDATASUBTYPES"
#define GDAL_DMD_CREATIONFIELDDATASUBTYPES "DMD_CREATIONFIELDDATASUBTYPES"
#define DMD_SUBDATASETS "DMD_SUBDATASETS"
#define GDAL_DMD_SUBDATASETS "DMD_SUBDATASETS"
#define DMD_CREATION_FIELD_DOMAIN_TYPES "DMD_CREATION_FIELD_DOMAIN_TYPES"
#define GDAL_DMD_CREATION_FIELD_DOMAIN_TYPES "DMD_CREATION_FIELD_DOMAIN_TYPES"
#define DMD_ALTER_GEOM_FIELD_DEFN_FLAGS "DMD_ALTER_GEOM_FIELD_DEFN_FLAGS"
#define GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS "DMD_ALTER_GEOM_FIELD_DEFN_FLAGS"
#define DMD_SUPPORTED_SQL_DIALECTS "DMD_SUPPORTED_SQL_DIALECTS"
#define GDAL_DMD_SUPPORTED_SQL_DIALECTS "DMD_SUPPORTED_SQL_DIALECTS"

#define DCAP_OPEN       "DCAP_OPEN"
#define GDAL_DCAP_OPEN       "DCAP_OPEN"
#define DCAP_CREATE     "DCAP_CREATE"
#define GDAL_DCAP_CREATE     "DCAP_CREATE"
#define DCAP_CREATE_MULTIDIMENSIONAL "DCAP_CREATE_MULTIDIMENSIONAL"
#define GDAL_DCAP_CREATE_MULTIDIMENSIONAL "DCAP_CREATE_MULTIDIMENSIONAL"
#define DCAP_CREATECOPY "DCAP_CREATECOPY"
#define GDAL_DCAP_CREATECOPY "DCAP_CREATECOPY"
#define DCAP_CREATECOPY_MULTIDIMENSIONAL "DCAP_CREATECOPY_MULTIDIMENSIONAL"
#define GDAL_DCAP_CREATECOPY_MULTIDIMENSIONAL "DCAP_CREATECOPY_MULTIDIMENSIONAL"
#define DCAP_MULTIDIM_RASTER "DCAP_MULTIDIM_RASTER"
#define GDAL_DCAP_MULTIDIM_RASTER "DCAP_MULTIDIM_RASTER"
#define DCAP_SUBCREATECOPY "DCAP_SUBCREATECOPY"
#define GDAL_DCAP_SUBCREATECOPY "DCAP_SUBCREATECOPY"
#define DCAP_VIRTUALIO  "DCAP_VIRTUALIO"
#define GDAL_DCAP_VIRTUALIO  "DCAP_VIRTUALIO"
#define DCAP_RASTER          "DCAP_RASTER"
#define GDAL_DCAP_RASTER          "DCAP_RASTER"
#define DCAP_VECTOR          "DCAP_VECTOR"
#define GDAL_DCAP_VECTOR          "DCAP_VECTOR"
#define DCAP_GNM          "DCAP_GNM"
#define GDAL_DCAP_GNM          "DCAP_GNM"
#define DCAP_CREATE_LAYER "DCAP_CREATE_LAYER"
#define GDAL_DCAP_CREATE_LAYER "DCAP_CREATE_LAYER"
#define DCAP_DELETE_LAYER "DCAP_DELETE_LAYER"
#define GDAL_DCAP_DELETE_LAYER "DCAP_DELETE_LAYER"
#define DCAP_CREATE_FIELD "DCAP_CREATE_FIELD"
#define GDAL_DCAP_CREATE_FIELD "DCAP_CREATE_FIELD"
#define DCAP_DELETE_FIELD "DCAP_DELETE_FIELD"
#define GDAL_DCAP_DELETE_FIELD "DCAP_DELETE_FIELD"
#define DCAP_REORDER_FIELDS "DCAP_REORDER_FIELDS"
#define GDAL_DCAP_REORDER_FIELDS "DCAP_REORDER_FIELDS"
#define DMD_ALTER_FIELD_DEFN_FLAGS "DMD_ALTER_FIELD_DEFN_FLAGS"
#define GDAL_DMD_ALTER_FIELD_DEFN_FLAGS "DMD_ALTER_FIELD_DEFN_FLAGS"
#define DCAP_NOTNULL_FIELDS  "DCAP_NOTNULL_FIELDS"
#define GDAL_DCAP_NOTNULL_FIELDS  "DCAP_NOTNULL_FIELDS"
#define DCAP_UNIQUE_FIELDS   "DCAP_UNIQUE_FIELDS"
#define GDAL_DCAP_UNIQUE_FIELDS   "DCAP_UNIQUE_FIELDS"
#define DCAP_DEFAULT_FIELDS  "DCAP_DEFAULT_FIELDS"
#define GDAL_DCAP_DEFAULT_FIELDS  "DCAP_DEFAULT_FIELDS"
#define DCAP_NOTNULL_GEOMFIELDS  "DCAP_NOTNULL_GEOMFIELDS"
#define GDAL_DCAP_NOTNULL_GEOMFIELDS  "DCAP_NOTNULL_GEOMFIELDS"
#define DCAP_NONSPATIAL  "DCAP_NONSPATIAL"
#define GDAL_DCAP_NONSPATIAL  "DCAP_NONSPATIAL"
#define DCAP_CURVE_GEOMETRIES  "DCAP_CURVE_GEOMETRIES"
#define GDAL_DCAP_CURVE_GEOMETRIES  "DCAP_CURVE_GEOMETRIES"
#define DCAP_MEASURED_GEOMETRIES  "DCAP_MEASURED_GEOMETRIES"
#define GDAL_DCAP_MEASURED_GEOMETRIES  "DCAP_MEASURED_GEOMETRIES"
#define DCAP_Z_GEOMETRIES  "DCAP_Z_GEOMETRIES"
#define GDAL_DCAP_Z_GEOMETRIES  "DCAP_Z_GEOMETRIES"
#define DMD_GEOMETRY_FLAGS  "DMD_GEOMETRY_FLAGS"
#define GDAL_DMD_GEOMETRY_FLAGS  "DMD_GEOMETRY_FLAGS"
#define DCAP_FEATURE_STYLES  "DCAP_FEATURE_STYLES"
#define GDAL_DCAP_FEATURE_STYLES  "DCAP_FEATURE_STYLES"
#define DCAP_COORDINATE_EPOCH "DCAP_COORDINATE_EPOCH"
#define GDAL_DCAP_COORDINATE_EPOCH "DCAP_COORDINATE_EPOCH"
#define DCAP_MULTIPLE_VECTOR_LAYERS "DCAP_MULTIPLE_VECTOR_LAYERS"
#define GDAL_DCAP_MULTIPLE_VECTOR_LAYERS "DCAP_MULTIPLE_VECTOR_LAYERS"
#define DCAP_FIELD_DOMAINS    "DCAP_FIELD_DOMAINS"
#define GDAL_DCAP_FIELD_DOMAINS    "DCAP_FIELD_DOMAINS"
#define DCAP_RELATIONSHIPS    "DCAP_RELATIONSHIPS"
#define GDAL_DCAP_RELATIONSHIPS    "DCAP_RELATIONSHIPS"
#define DCAP_CREATE_RELATIONSHIP    "DCAP_CREATE_RELATIONSHIP"
#define GDAL_DCAP_CREATE_RELATIONSHIP    "DCAP_CREATE_RELATIONSHIP"
#define DCAP_DELETE_RELATIONSHIP    "DCAP_DELETE_RELATIONSHIP"
#define GDAL_DCAP_DELETE_RELATIONSHIP    "DCAP_DELETE_RELATIONSHIP"
#define DCAP_UPDATE_RELATIONSHIP    "DCAP_UPDATE_RELATIONSHIP"
#define GDAL_DCAP_UPDATE_RELATIONSHIP    "DCAP_UPDATE_RELATIONSHIP"
#define DMD_RELATIONSHIP_FLAGS    "DMD_RELATIONSHIP_FLAGS"
#define GDAL_DMD_RELATIONSHIP_FLAGS    "DMD_RELATIONSHIP_FLAGS"
#define DCAP_RENAME_LAYERS    "DCAP_RENAME_LAYERS"
#define GDAL_DCAP_RENAME_LAYERS    "DCAP_RENAME_LAYERS"

#define DIM_TYPE_HORIZONTAL_X "HORIZONTAL_X"
#define GDAL_DIM_TYPE_HORIZONTAL_X "HORIZONTAL_X"
#define DIM_TYPE_HORIZONTAL_Y "HORIZONTAL_Y"
#define GDAL_DIM_TYPE_HORIZONTAL_Y "HORIZONTAL_Y"
#define DIM_TYPE_VERTICAL     "VERTICAL"
#define GDAL_DIM_TYPE_VERTICAL     "VERTICAL"
#define DIM_TYPE_TEMPORAL     "TEMPORAL"
#define GDAL_DIM_TYPE_TEMPORAL     "TEMPORAL"
#define DIM_TYPE_PARAMETRIC   "PARAMETRIC"
#define GDAL_DIM_TYPE_PARAMETRIC   "PARAMETRIC"

#define GDsCAddRelationship    "AddRelationship"
#define GDsCDeleteRelationship "DeleteRelationship"
#define GDsCUpdateRelationship "UpdateRelationship"

#endif

%constant CPLES_BackslashQuotable = CPLES_BackslashQuotable;
%constant CPLES_XML               = CPLES_XML;
%constant CPLES_XML_BUT_QUOTES    = CPLES_XML_BUT_QUOTES;
%constant CPLES_URL               = CPLES_URL;
%constant CPLES_SQL               = CPLES_SQL;
%constant CPLES_SQLI              = CPLES_SQLI;
%constant CPLES_CSV               = CPLES_CSV;

// GDALRATFieldType
%constant GFT_Integer             = GFT_Integer;
%constant GFT_Real                = GFT_Real;
%constant GFT_String              = GFT_String;

// GDALRATFieldUsage
%constant GFU_Generic             = GFU_Generic;
%constant GFU_PixelCount          = GFU_PixelCount;
%constant GFU_Name                = GFU_Name;
%constant GFU_Min                 = GFU_Min;
%constant GFU_Max                 = GFU_Max;
%constant GFU_MinMax              = GFU_MinMax;
%constant GFU_Red                 = GFU_Red;
%constant GFU_Green               = GFU_Green;
%constant GFU_Blue                = GFU_Blue;
%constant GFU_Alpha               = GFU_Alpha;
%constant GFU_RedMin              = GFU_RedMin;
%constant GFU_GreenMin            = GFU_GreenMin;
%constant GFU_BlueMin             = GFU_BlueMin;
%constant GFU_AlphaMin            = GFU_AlphaMin;
%constant GFU_RedMax              = GFU_RedMax;
%constant GFU_GreenMax            = GFU_GreenMax;
%constant GFU_BlueMax             = GFU_BlueMax;
%constant GFU_AlphaMax            = GFU_AlphaMax;
%constant GFU_MaxCount            = GFU_MaxCount;

// GDALRATTableType
%constant GRTT_THEMATIC           = GRTT_THEMATIC;
%constant GRTT_ATHEMATIC          = GRTT_ATHEMATIC;

%constant GMF_ALL_VALID           = 0x01;
%constant GMF_PER_DATASET         = 0x02;
%constant GMF_ALPHA               = 0x04;
%constant GMF_NODATA              = 0x08;

%constant GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED  = 0x01;
%constant GDAL_DATA_COVERAGE_STATUS_DATA           = 0x02;
%constant GDAL_DATA_COVERAGE_STATUS_EMPTY          = 0x04;

// GDALAsyncStatusType
%constant GARIO_PENDING = GARIO_PENDING;
%constant GARIO_UPDATE = GARIO_UPDATE;
%constant GARIO_ERROR = GARIO_ERROR;
%constant GARIO_COMPLETE = GARIO_COMPLETE;

// GDALTileOrganization
%constant GTO_TIP  = GTO_TIP;
%constant GTO_BIT = GTO_BIT;
%constant GTO_BSQ = GTO_BSQ;

// GDALRelationshipCardinality
%constant GRC_ONE_TO_ONE = GRC_ONE_TO_ONE;
%constant GRC_ONE_TO_MANY = GRC_ONE_TO_MANY;
%constant GRC_MANY_TO_ONE = GRC_MANY_TO_ONE;
%constant GRC_MANY_TO_MANY = GRC_MANY_TO_MANY;

// GDALRelationshipType
%constant GRT_COMPOSITE = GRT_COMPOSITE;
%constant GRT_ASSOCIATION = GRT_ASSOCIATION;
%constant GRT_AGGREGATION = GRT_AGGREGATION;

#ifdef SWIGPYTHON
%thread;
#endif
