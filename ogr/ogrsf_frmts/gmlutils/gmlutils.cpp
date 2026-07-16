/******************************************************************************
 *
 * Project:  GML Utils
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gmlutils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_mem_cache.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

/************************************************************************/
/*                   GML_ExtractSrsNameFromGeometry()                   */
/************************************************************************/

const char *
GML_ExtractSrsNameFromGeometry(const CPLXMLNode *const *papsGeometry,
                               std::string &osWork, bool bConsiderEPSGAsURN)
{
    if (papsGeometry[0] != nullptr && papsGeometry[1] == nullptr)
    {
        const char *pszSRSName =
            CPLGetXMLValue(papsGeometry[0], "srsName", nullptr);
        if (pszSRSName)
        {
            const int nLen = static_cast<int>(strlen(pszSRSName));

            if (STARTS_WITH(pszSRSName, "EPSG:") && bConsiderEPSGAsURN)
            {
                osWork.reserve(22 + nLen - 5);
                osWork.assign("urn:ogc:def:crs:EPSG::", 22);
                osWork.append(pszSRSName + 5, nLen - 5);
                return osWork.c_str();
            }
            else if (STARTS_WITH(pszSRSName,
                                 "http://www.opengis.net/gml/srs/epsg.xml#"))
            {
                osWork.reserve(5 + nLen - 40);
                osWork.assign("EPSG:", 5);
                osWork.append(pszSRSName + 40, nLen - 40);
                return osWork.c_str();
            }
            else
            {
                return pszSRSName;
            }
        }
    }
    return nullptr;
}

/************************************************************************/
/*                       GML_IsSRSLatLongOrder()                        */
/************************************************************************/

bool GML_IsSRSLatLongOrder(const char *pszSRSName)
{
    if (pszSRSName == nullptr)
        return false;

    if (STARTS_WITH(pszSRSName, "urn:") &&
        strstr(pszSRSName, ":4326") != nullptr)
    {
        // Shortcut.
        return true;
    }
    else if (!EQUALN(pszSRSName, "EPSG:", 5))
    {
        OGRSpatialReference oSRS;
        if (oSRS.SetFromUserInput(
                pszSRSName,
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) ==
            OGRERR_NONE)
        {
            if (oSRS.EPSGTreatsAsLatLong() ||
                oSRS.EPSGTreatsAsNorthingEasting())
                return true;
        }
    }
    return false;
}

/************************************************************************/
/*            OGRGML_SRSCacheEntry::~OGRGML_SRSCacheEntry()             */
/************************************************************************/

OGRGML_SRSCacheEntry::~OGRGML_SRSCacheEntry()
{
    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                           OGRGML_SRSCache                            */
/************************************************************************/

class OGRGML_SRSCache
{
  public:
    lru11::Cache<std::string, std::shared_ptr<OGRGML_SRSCacheEntry>>
        oSRSCache{};
};

/************************************************************************/
/*                       OGRGML_SRSCache_Create()                       */
/************************************************************************/

OGRGML_SRSCache *OGRGML_SRSCache_Create()
{
    return new OGRGML_SRSCache();
}

/************************************************************************/
/*                      OGRGML_SRSCache_Destroy()                       */
/************************************************************************/

void OGRGML_SRSCache_Destroy(OGRGML_SRSCache *hSRSCache)
{
    delete hSRSCache;
}

/************************************************************************/
/*                      OGRGML_SRSCache_GetInfo()                       */
/************************************************************************/

std::shared_ptr<OGRGML_SRSCacheEntry>
OGRGML_SRSCache_GetInfo(OGRGML_SRSCache *hSRSCache, const char *pszSRSName)
{
    std::shared_ptr<OGRGML_SRSCacheEntry> entry;
    if (!hSRSCache->oSRSCache.tryGet(pszSRSName, entry))
    {
        entry = std::make_shared<OGRGML_SRSCacheEntry>();
        auto poSRS = new OGRSpatialReference();
        entry->poSRS = poSRS;
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRS->SetFromUserInput(
                pszSRSName,
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
            OGRERR_NONE)
        {
            return nullptr;
        }
        entry->nAxisCount = poSRS->GetAxesCount();
        entry->bIsGeographic = poSRS->IsGeographic();
        entry->bIsProjected = poSRS->IsProjected();
        entry->bInvertedAxisOrder = !STARTS_WITH(pszSRSName, "EPSG:") &&
                                    (poSRS->EPSGTreatsAsLatLong() ||
                                     poSRS->EPSGTreatsAsNorthingEasting());
        entry->dfSemiMajor = poSRS->GetSemiMajor();
        if (entry->bIsProjected)
            entry->dfLinearUnits = poSRS->GetLinearUnits(nullptr);
        if (entry->bIsGeographic)
        {
            entry->bAngularUnitIsDegree =
                fabs(poSRS->GetAngularUnits(nullptr) -
                     CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
        }
        hSRSCache->oSRSCache.insert(pszSRSName, entry);
    }
    return entry;
}

/************************************************************************/
/*                    GML_BuildOGRGeometryFromList()                    */
/************************************************************************/

OGRGeometry *GML_BuildOGRGeometryFromList(
    const CPLXMLNode *const *papsGeometry, bool bTryToMakeMultipolygons,
    bool bInvertAxisOrderIfLatLong, const char *pszDefaultSRSName,
    bool bConsiderEPSGAsURN, GMLSwapCoordinatesEnum eSwapCoordinates,
    int nPseudoBoolGetSecondaryGeometryOption, OGRGML_SRSCache *hSRSCache,
    bool bFaceHoleNegative)
{
    OGRGeometry *poGeom = nullptr;
    OGRGeometryCollection *poCollection = nullptr;
#ifndef WITHOUT_CPLDEBUG
    static const bool bDebugGML =
        EQUAL(CPLGetConfigOption("CPL_DEBUG", ""), "GML");
#endif
    for (int i = 0; papsGeometry[i] != nullptr; i++)
    {
#ifndef WITHOUT_CPLDEBUG
        if (bDebugGML)
        {
            char *pszXML = CPLSerializeXMLTree(papsGeometry[i]);
            CPLDebug("GML", "Parsing: %s", pszXML);
            CPLFree(pszXML);
        }
#endif
        OGRGeometry *poSubGeom = GML2OGRGeometry_XMLNode(
            papsGeometry[i], nPseudoBoolGetSecondaryGeometryOption, hSRSCache,
            0, 0, false, true, bFaceHoleNegative);
        if (poSubGeom)
        {
            if (poGeom == nullptr)
            {
                poGeom = poSubGeom;
            }
            else
            {
                if (poCollection == nullptr)
                {
                    if (bTryToMakeMultipolygons &&
                        wkbFlatten(poGeom->getGeometryType()) == wkbPolygon &&
                        wkbFlatten(poSubGeom->getGeometryType()) == wkbPolygon)
                    {
                        OGRGeometryCollection *poGeomColl =
                            new OGRMultiPolygon();
                        poGeomColl->addGeometryDirectly(poGeom);
                        poGeomColl->addGeometryDirectly(poSubGeom);
                        poGeom = poGeomColl;
                    }
                    else if (bTryToMakeMultipolygons &&
                             wkbFlatten(poGeom->getGeometryType()) ==
                                 wkbMultiPolygon &&
                             wkbFlatten(poSubGeom->getGeometryType()) ==
                                 wkbPolygon)
                    {
                        OGRMultiPolygon *poGeomColl = poGeom->toMultiPolygon();
                        poGeomColl->addGeometryDirectly(poSubGeom);
                    }
                    else if (bTryToMakeMultipolygons &&
                             wkbFlatten(poGeom->getGeometryType()) ==
                                 wkbMultiPolygon &&
                             wkbFlatten(poSubGeom->getGeometryType()) ==
                                 wkbMultiPolygon)
                    {
                        OGRMultiPolygon *poGeomColl = poGeom->toMultiPolygon();
                        for (auto &&poMember : poSubGeom->toMultiPolygon())
                        {
                            poGeomColl->addGeometry(poMember);
                        }
                        delete poSubGeom;
                    }
                    else if (bTryToMakeMultipolygons &&
                             wkbFlatten(poGeom->getGeometryType()) ==
                                 wkbMultiPolygon)
                    {
                        delete poGeom;
                        delete poSubGeom;
                        return GML_BuildOGRGeometryFromList(
                            papsGeometry, false, bInvertAxisOrderIfLatLong,
                            pszDefaultSRSName, bConsiderEPSGAsURN,
                            eSwapCoordinates,
                            nPseudoBoolGetSecondaryGeometryOption, hSRSCache);
                    }
                    else
                    {
                        poCollection = new OGRGeometryCollection();
                        poCollection->addGeometryDirectly(poGeom);
                        poGeom = poCollection;
                    }
                }
                if (poCollection != nullptr)
                {
                    poCollection->addGeometryDirectly(poSubGeom);
                }
            }
        }
    }

    if (poGeom == nullptr)
        return nullptr;

    std::string osWork;
    const char *pszSRSName = GML_ExtractSrsNameFromGeometry(
        papsGeometry, osWork, bConsiderEPSGAsURN);
    const char *pszNameLookup = pszSRSName;
    if (pszNameLookup == nullptr)
        pszNameLookup = pszDefaultSRSName;

    if (pszNameLookup != nullptr)
    {
        auto entry = OGRGML_SRSCache_GetInfo(hSRSCache, pszNameLookup);
        if (entry)
        {
            poGeom->assignSpatialReference(entry->poSRS);
            if ((eSwapCoordinates == GML_SWAP_AUTO &&
                 entry->bInvertedAxisOrder && bInvertAxisOrderIfLatLong) ||
                eSwapCoordinates == GML_SWAP_YES)
            {
                poGeom->swapXY();
            }
        }
    }
    else if (eSwapCoordinates == GML_SWAP_YES)
    {
        poGeom->swapXY();
    }

    return poGeom;
}

/************************************************************************/
/*                           GML_GetSRSName()                           */
/************************************************************************/

char *GML_GetSRSName(const OGRSpatialReference *poSRS,
                     OGRGMLSRSNameFormat eSRSNameFormat, bool *pbCoordSwap)
{
    *pbCoordSwap = false;
    if (poSRS == nullptr)
        return CPLStrdup("");

    const auto &map = poSRS->GetDataAxisToSRSAxisMapping();
    if (eSRSNameFormat != SRSNAME_SHORT && map.size() >= 2 && map[0] == 2 &&
        map[1] == 1)
    {
        *pbCoordSwap = true;
    }

    const char *pszAuthName = poSRS->GetAuthorityName(nullptr);
    const char *pszAuthCode = poSRS->GetAuthorityCode(nullptr);
    if (nullptr != pszAuthName && nullptr != pszAuthCode)
    {
        if (eSRSNameFormat == SRSNAME_SHORT)
        {
            return CPLStrdup(
                CPLSPrintf(" srsName=\"%s:%s\"", pszAuthName, pszAuthCode));
        }
        else if (eSRSNameFormat == SRSNAME_OGC_URN)
        {
            return CPLStrdup(CPLSPrintf(" srsName=\"urn:ogc:def:crs:%s::%s\"",
                                        pszAuthName, pszAuthCode));
        }
        else if (eSRSNameFormat == SRSNAME_OGC_URL)
        {
            return CPLStrdup(CPLSPrintf(
                " srsName=\"http://www.opengis.net/def/crs/%s/0/%s\"",
                pszAuthName, pszAuthCode));
        }
    }
    return CPLStrdup("");
}

/************************************************************************/
/*                         GML_IsLegitSRSName()                         */
/************************************************************************/

bool GML_IsLegitSRSName(const char *pszSRSName)
{

    if (STARTS_WITH_CI(pszSRSName, "http"))
    {
        if (!(STARTS_WITH_CI(pszSRSName, "http://opengis.net/def/crs") ||
              STARTS_WITH_CI(pszSRSName, "http://www.opengis.net/def/crs")))
        {
            return false;
        }
    }
    return true;
}
