/******************************************************************************
 *
 * Project:  MVT Translator
 * Purpose:  Mapbox Vector Tile decoder
 * Author:   Even Rouault, Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef MVTUTILS_H
#define MVTUTILS_H

#include "cpl_json.h"
#include "ogrsf_frmts.h"

#define MVT_LCO \
"<LayerCreationOptionList>" \
"  <Option name='MINZOOM' type='int' min='0' max='22' " \
        "description='Minimum zoom level'/>" \
"  <Option name='MAXZOOM' type='int' min='0' max='22' " \
        "description='Maximum zoom level'/>" \
"  <Option name='NAME' type='string' description='Target layer name'/>" \
"  <Option name='DESCRIPTION' type='string' " \
        "description='A description of the layer'/>" \
"</LayerCreationOptionList>"

#define MVT_MBTILES_COMMON_DSCO \
"  <Option name='MINZOOM' scope='vector' type='int' min='0' max='22' " \
        "description='Minimum zoom level' default='0'/>" \
"  <Option name='MAXZOOM' scope='vector' type='int' min='0' max='22' " \
        "description='Maximum zoom level' default='5'/>" \
"  <Option name='CONF' scope='vector' type='string' " \
        "description='Layer configuration as a JSon serialized string, or a filename pointing to a JSon file'/>" \
"  <Option name='SIMPLIFICATION' scope='vector' type='float' " \
        "description='Simplification factor'/>" \
"  <Option name='SIMPLIFICATION_MAX_ZOOM' scope='vector' type='float' " \
        "description='Simplification factor at max zoom'/>" \
"  <Option name='EXTENT' scope='vector' type='unsigned int' default='4096' " \
        "description='Number of units in a tile'/>" \
"  <Option name='BUFFER' scope='vector' type='unsigned int' default='80' " \
        "description='Number of units for geometry buffering'/>" \
"  <Option name='COMPRESS' scope='vector' type='boolean' description=" \
        "'Whether to deflate-compress tiles' default='YES'/>" \
"  <Option name='TEMPORARY_DB' scope='vector' type='string' description='" \
        "Filename with path for the temporary database'/>" \
"  <Option name='MAX_SIZE' scope='vector' type='unsigned int' min='100' default='500000' " \
        "description='Maximum size of a tile in bytes'/>" \
"  <Option name='MAX_FEATURES' scope='vector' type='unsigned int' min='1' default='200000' " \
        "description='Maximum number of features per tile'/>"

void OGRMVTInitFields(OGRFeatureDefn* poFeatureDefn,
                      const CPLJSONObject& oFields);

OGRwkbGeometryType OGRMVTFindGeomTypeFromTileStat(
                                const CPLJSONArray& oTileStatLayers,
                                const char* pszLayerName);

OGRFeature* OGRMVTCreateFeatureFrom(OGRFeature* poSrcFeature,
                                    OGRFeatureDefn* poTargetFeatureDefn,
                                    bool bJsonField,
                                    OGRSpatialReference* poSRS);

#ifdef HAVE_MVT_WRITE_SUPPORT
GDALDataset* OGRMVTWriterDatasetCreate( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char ** papszOptions );
#endif

#endif // MVTUTILS_H
