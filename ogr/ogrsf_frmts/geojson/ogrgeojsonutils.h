/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private utilities within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef OGR_GEOJSONUTILS_H_INCLUDED
#define OGR_GEOJSONUTILS_H_INCLUDED

#include "ogr_core.h"

#include "cpl_json_header.h"
#include "cpl_http.h"
#include "cpl_vsi.h"
#include "gdal_priv.h"

class OGRGeometry;

/************************************************************************/
/*                           GeoJSONSourceType                          */
/************************************************************************/

enum GeoJSONSourceType
{
    eGeoJSONSourceUnknown = 0,
    eGeoJSONSourceFile,
    eGeoJSONSourceText,
    eGeoJSONSourceService
};

GeoJSONSourceType GeoJSONGetSourceType(GDALOpenInfo *poOpenInfo);
GeoJSONSourceType GeoJSONSeqGetSourceType(GDALOpenInfo *poOpenInfo);
GeoJSONSourceType ESRIJSONDriverGetSourceType(GDALOpenInfo *poOpenInfo);
GeoJSONSourceType TopoJSONDriverGetSourceType(GDALOpenInfo *poOpenInfo);
GeoJSONSourceType JSONFGDriverGetSourceType(GDALOpenInfo *poOpenInfo);

/************************************************************************/
/*                           GeoJSONIsObject                            */
/************************************************************************/

bool GeoJSONIsObject(const char *pszText, GDALOpenInfo *poOpenInfo);
bool GeoJSONSeqIsObject(const char *pszText, GDALOpenInfo *poOpenInfo);
bool ESRIJSONIsObject(const char *pszText, GDALOpenInfo *poOpenInfo);
bool TopoJSONIsObject(const char *pszText, GDALOpenInfo *poOpenInfo);
bool JSONFGIsObject(const char *pszText, GDALOpenInfo *poOpenInfo);

/************************************************************************/
/*                      GeoJSONStringPropertyToFieldType                */
/************************************************************************/

OGRFieldType GeoJSONStringPropertyToFieldType(json_object *poObject,
                                              int &nTZFlag);

/************************************************************************/
/*                  GeoJSONHTTPFetchWithContentTypeHeader               */
/************************************************************************/

CPLHTTPResult *GeoJSONHTTPFetchWithContentTypeHeader(const char *pszURL);

#endif  // OGR_GEOJSONUTILS_H_INCLUDED
