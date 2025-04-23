/******************************************************************************
 *
 * Project:  GML Utils
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_GMLUTILS_H_INCLUDED
#define CPL_GMLUTILS_H_INCLUDED

#include <vector>
#include <string>
#include "cpl_minixml.h"

#include "ogr_geometry.h"

typedef enum
{
    GML_SWAP_AUTO,
    GML_SWAP_YES,
    GML_SWAP_NO,
} GMLSwapCoordinatesEnum;

typedef enum
{
    SRSNAME_SHORT,
    SRSNAME_OGC_URN,
    SRSNAME_OGC_URL
} OGRGMLSRSNameFormat;

const char CPL_DLL *
GML_ExtractSrsNameFromGeometry(const CPLXMLNode *const *papsGeometry,
                               std::string &osWork, bool bConsiderEPSGAsURN);

bool CPL_DLL GML_IsSRSLatLongOrder(const char *pszSRSName);
bool CPL_DLL GML_IsLegitSRSName(const char *pszSRSName);

void CPL_DLL *GML_BuildOGRGeometryFromList_CreateCache();
void CPL_DLL GML_BuildOGRGeometryFromList_DestroyCache(void *hCacheSRS);

OGRGeometry CPL_DLL *GML_BuildOGRGeometryFromList(
    const CPLXMLNode *const *papsGeometry, bool bTryToMakeMultipolygons,
    bool bInvertAxisOrderIfLatLong, const char *pszDefaultSRSName,
    bool bConsiderEPSGAsURN, GMLSwapCoordinatesEnum eSwapCoordinates,
    int nPseudoBoolGetSecondaryGeometryOption, void *hCacheSRS,
    bool bFaceHoleNegative = false);

char CPL_DLL *GML_GetSRSName(const OGRSpatialReference *poSRS,
                             OGRGMLSRSNameFormat eSRSNameFormat,
                             bool *pbCoordSwap);

#endif /* _CPL_GMLREADERP_H_INCLUDED */
