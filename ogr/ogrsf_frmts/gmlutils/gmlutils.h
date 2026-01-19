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

#include <memory>
#include <vector>
#include <string>
#include "cpl_minixml.h"

#include "ogr_geometry.h"

class OGRSpatialReference;

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

class OGRGML_SRSCache;
OGRGML_SRSCache CPL_DLL *OGRGML_SRSCache_Create();
void CPL_DLL OGRGML_SRSCache_Destroy(OGRGML_SRSCache *hSRSCache);

class OGRGML_SRSCacheEntry
{
  public:
    OGRSpatialReference *poSRS = nullptr;
    double dfSemiMajor = 0;
    double dfLinearUnits = 0;  // only set if bIsProjected
    int nAxisCount = 0;
    bool bIsGeographic = false;
    bool bIsProjected = false;
    bool bAngularUnitIsDegree = false;  // only set if bIsGeographic
    bool bInvertedAxisOrder = false;

    OGRGML_SRSCacheEntry() = default;
    ~OGRGML_SRSCacheEntry();

    OGRGML_SRSCacheEntry(const OGRGML_SRSCacheEntry &) = delete;
    OGRGML_SRSCacheEntry &operator=(const OGRGML_SRSCacheEntry &) = delete;
};

std::shared_ptr<OGRGML_SRSCacheEntry> CPL_DLL
OGRGML_SRSCache_GetInfo(OGRGML_SRSCache *hSRSCache, const char *pszSRSName);

OGRGeometry CPL_DLL *GML2OGRGeometry_XMLNode(
    const CPLXMLNode *psNode, int nPseudoBoolGetSecondaryGeometryOption,
    OGRGML_SRSCache *hSRSCache, int nRecLevel = 0, int nSRSDimension = 0,
    bool bIgnoreGSG = false, bool bOrientation = true,
    bool bFaceHoleNegative = false, const char *pszId = nullptr);

OGRGeometry CPL_DLL *GML_BuildOGRGeometryFromList(
    const CPLXMLNode *const *papsGeometry, bool bTryToMakeMultipolygons,
    bool bInvertAxisOrderIfLatLong, const char *pszDefaultSRSName,
    bool bConsiderEPSGAsURN, GMLSwapCoordinatesEnum eSwapCoordinates,
    int nPseudoBoolGetSecondaryGeometryOption, OGRGML_SRSCache *hSRSCache,
    bool bFaceHoleNegative = false);

char CPL_DLL *GML_GetSRSName(const OGRSpatialReference *poSRS,
                             OGRGMLSRSNameFormat eSRSNameFormat,
                             bool *pbCoordSwap);

#endif /* _CPL_GMLREADERP_H_INCLUDED */
