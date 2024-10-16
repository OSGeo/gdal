// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#ifndef OGR_VRT_GEOMETRY_TYPES_H
#define OGR_VRT_GEOMETRY_TYPES_H

#include "ogr_api.h"

#include <string>

OGRwkbGeometryType CPL_DLL OGRVRTGetGeometryType(const char *pszGType,
                                                 int *pbError);
std::string CPL_DLL
OGRVRTGetSerializedGeometryType(OGRwkbGeometryType eGeomType);

#endif
