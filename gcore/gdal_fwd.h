/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Forward definitions of GDAL/OGR/OSR C handle types.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002 Frank Warmerdam
 * Copyright (c) 2007-2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_FWD_H_INCLUDED
#define GDAL_FWD_H_INCLUDED

/* clang-format off */
#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \file gdal_fwd.h
 *
 * Forward definitions of GDAL/OGR/OSR C handle types.
 *
 * Users should only rely on the type name, and not its definition, which
 * could change in later versions.
 *
 * @since GDAL 3.11
 */

/*!
\section gdal_fwd_raster Raster related types.
*/

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

/** Opaque type used for the C bindings of the C++ GDALRasterAttributeTable
 * class */
typedef void *GDALRasterAttributeTableH;

/** Opaque type used for the C bindings of the C++ GDALAsyncReader class */
typedef void *GDALAsyncReaderH;

/** Opaque type used for the C bindings of the C++ GDALRelationship class
 *  @since GDAL 3.6
 */
typedef void *GDALRelationshipH;

/** Opaque type for C++ GDALExtendedDataType */
typedef struct GDALExtendedDataTypeHS *GDALExtendedDataTypeH;
/** Opaque type for C++ GDALEDTComponent */
typedef struct GDALEDTComponentHS *GDALEDTComponentH;
/** Opaque type for C++ GDALGroup */
typedef struct GDALGroupHS *GDALGroupH;
/** Opaque type for C++ GDALMDArray */
typedef struct GDALMDArrayHS *GDALMDArrayH;
/** Opaque type for C++ GDALAttribute */
typedef struct GDALAttributeHS *GDALAttributeH;
/** Opaque type for C++ GDALDimension */
typedef struct GDALDimensionHS *GDALDimensionH;

/**
 *  Opaque type used for the C bindings of the C++ GDALSubdatasetInfo class
 *  @since GDAL 3.8
*/
typedef struct GDALSubdatasetInfo *GDALSubdatasetInfoH;

/*!
\section gdal_fwd_geometry Geometry related types.
*/

#if defined(GDAL_DEBUG)
/** Opaque type for a geometry */
typedef struct OGRGeometryHS *OGRGeometryH;
#else
/** Opaque type for a geometry */
typedef void *OGRGeometryH;
#endif

/** Opaque type for a geometry transformer. */
typedef struct OGRGeomTransformer *OGRGeomTransformerH;

/** Opaque type for OGRGeomCoordinatePrecision */
typedef struct OGRGeomCoordinatePrecision *OGRGeomCoordinatePrecisionH;

/** Opaque type for WKB export options */
typedef struct OGRwkbExportOptions OGRwkbExportOptions;

/** Opaque type for a prepared geometry */
typedef struct _OGRPreparedGeometry *OGRPreparedGeometryH;

/*!
\section gdal_fwd_field Attribute field, geometry field and layer definitions.
*/

#if defined(GDAL_DEBUG)
/** Opaque type for a field definition (OGRFieldDefn) */
typedef struct OGRFieldDefnHS *OGRFieldDefnH;
/** Opaque type for a feature definition (OGRFeatureDefn) */
typedef struct OGRFeatureDefnHS *OGRFeatureDefnH;
#else
/** Opaque type for a field definition (OGRFieldDefn) */
typedef void *OGRFieldDefnH;
/** Opaque type for a feature definition (OGRFeatureDefn) */
typedef void *OGRFeatureDefnH;
#endif

/** Opaque type for a geometry field definition (OGRGeomFieldDefn) */
typedef struct OGRGeomFieldDefnHS *OGRGeomFieldDefnH;

/** Opaque type for a field domain definition (OGRFieldDomain) */
typedef struct OGRFieldDomainHS *OGRFieldDomainH;

/*!
\section gdal_fwd_feature Vector feature type.
*/
#if defined(GDAL_DEBUG)
/** Opaque type for a feature (OGRFeature) */
typedef struct OGRFeatureHS *OGRFeatureH;
#else
/** Opaque type for a feature (OGRFeature) */
typedef void *OGRFeatureH;
#endif

/*!
\section gdal_fwd_layer Vector layer related types.
*/

#if defined(GDAL_DEBUG)
/** Opaque type for a layer (OGRLayer) */
typedef struct OGRLayerHS *OGRLayerH;
/** Opaque type for a OGR datasource (OGRDataSource) (deprecated) */
typedef struct OGRDataSourceHS *OGRDataSourceH;
/** Opaque type for a OGR driver (OGRSFDriver) (deprecated) */
typedef struct OGRDriverHS *OGRSFDriverH;
#else
/** Opaque type for a layer (OGRLayer) */
typedef void *OGRLayerH;
/** Opaque type for a OGR datasource (OGRDataSource) (deprecated) */
typedef void *OGRDataSourceH;
/** Opaque type for a OGR driver (OGRSFDriver) (deprecated) */
typedef void *OGRSFDriverH;
#endif

/*!
\section gdal_fwd_style Vector styling related types.
*/

#if defined(GDAL_DEBUG)
/** Style manager opaque type */
typedef struct OGRStyleMgrHS *OGRStyleMgrH;
/** Opaque type for a style table (OGRStyleTable) */
typedef struct OGRStyleTableHS *OGRStyleTableH;
/** Style tool opaque type */
typedef struct OGRStyleToolHS *OGRStyleToolH;
#else
/** Style manager opaque type */
typedef void *OGRStyleMgrH;
/** Style tool opaque type */
typedef void *OGRStyleToolH;
/** Opaque type for a style table (OGRStyleTable) */
typedef void *OGRStyleTableH;
#endif

/*!
\section gdal_fwd_crs CRS and coordinate transformation related types.
*/

#if defined(GDAL_DEBUG)
/** Opaque type for a spatial reference system */
typedef struct OGRSpatialReferenceHS *OGRSpatialReferenceH;
/** Opaque type for a coordinate transformation object */
typedef struct OGRCoordinateTransformationHS *OGRCoordinateTransformationH;
#else
/** Opaque type for a spatial reference system */
typedef void *OGRSpatialReferenceH;
/** Opaque type for a coordinate transformation object */
typedef void *OGRCoordinateTransformationH;
#endif

/** Coordinate transformation options. */
typedef struct OGRCoordinateTransformationOptions
    *OGRCoordinateTransformationOptionsH;

/*!
\section gdal_fwd_gnm GNM (Geography Network Models) related types.
*/

/** Opaque type for a GNMNetwork */
typedef void *GNMNetworkH;

/** Opaque type for a GNMGenericNetwork */
typedef void *GNMGenericNetworkH;

#ifdef __cplusplus
}
#endif
/* clang-format on */

#endif  // GDAL_FWD_H_INCLUDED
