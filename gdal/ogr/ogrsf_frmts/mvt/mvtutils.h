/******************************************************************************
 *
 * Project:  MVT Translator
 * Purpose:  MapBox Vector Tile decoder
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

void OGRMVTInitFields(OGRFeatureDefn* poFeatureDefn,
                      const CPLJSONObject& oFields);

OGRwkbGeometryType OGRMVTFindGeomTypeFromTileStat(
                                const CPLJSONArray& oTileStatLayers,
                                const char* pszLayerName);

OGRFeature* OGRMVTCreateFeatureFrom(OGRFeature* poSrcFeature,
                                    OGRFeatureDefn* poTargetFeatureDefn,
                                    bool bJsonField,
                                    OGRSpatialReference* poSRS);

#endif // MVTUTILS_H
