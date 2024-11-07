// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#ifndef OGRESRIJSONGEOMETRY_H_INCLUDED
#define OGRESRIJSONGEOMETRY_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include "cpl_port.h"
#include "cpl_json_header.h"

#include "ogr_api.h"

class OGRGeometry;
class OGRPoint;
class OGRMultiPoint;
class OGRSpatialReference;

OGRGeometry CPL_DLL *OGRESRIJSONReadGeometry(json_object *poObj);
OGRSpatialReference CPL_DLL *
OGRESRIJSONReadSpatialReference(json_object *poObj);
OGRwkbGeometryType CPL_DLL OGRESRIJSONGetGeometryType(json_object *poObj);

/*! @endcond */

#endif
