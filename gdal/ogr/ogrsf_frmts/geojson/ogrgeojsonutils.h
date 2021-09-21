/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private utilities within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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
#ifndef OGR_GEOJSONUTILS_H_INCLUDED
#define OGR_GEOJSONUTILS_H_INCLUDED

#include "ogr_core.h"

#include "cpl_json_header.h"
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

GeoJSONSourceType GeoJSONGetSourceType( GDALOpenInfo* poOpenInfo );
GeoJSONSourceType GeoJSONSeqGetSourceType( GDALOpenInfo* poOpenInfo );
GeoJSONSourceType ESRIJSONDriverGetSourceType( GDALOpenInfo* poOpenInfo );
GeoJSONSourceType TopoJSONDriverGetSourceType( GDALOpenInfo* poOpenInfo );

/************************************************************************/
/*                           GeoJSONIsObject                            */
/************************************************************************/

bool GeoJSONIsObject( const char* pszText );
bool GeoJSONSeqIsObject( const char* pszText );
bool ESRIJSONIsObject(const char *pszText);
bool TopoJSONIsObject(const char *pszText);

/************************************************************************/
/*                           GeoJSONPropertyToFieldType                 */
/************************************************************************/

OGRFieldType GeoJSONPropertyToFieldType( json_object* poObject,
                                         OGRFieldSubType& eSubType,
                                         bool bArrayAsString = false );

/************************************************************************/
/*                      GeoJSONStringPropertyToFieldType                */
/************************************************************************/

OGRFieldType GeoJSONStringPropertyToFieldType( json_object* poObject );

/************************************************************************/
/*                           OGRGeoJSONGetGeometryName                  */
/************************************************************************/

const char* OGRGeoJSONGetGeometryName( OGRGeometry const* poGeometry );

#endif  // OGR_GEOJSONUTILS_H_INCLUDED
