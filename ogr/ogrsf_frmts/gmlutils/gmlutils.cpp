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
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

/************************************************************************/
/*                GML_ExtractSrsNameFromGeometry()                      */
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
/*                GML_BuildOGRGeometryFromList_CreateCache()            */
/************************************************************************/

namespace
{
class SRSDesc
{
  public:
    std::string osSRSName{};
    bool bAxisInvert = false;
    OGRSpatialReference *poSRS = nullptr;

    SRSDesc() = default;

    SRSDesc &operator=(SRSDesc &&other)
    {
        osSRSName = std::move(other.osSRSName);
        bAxisInvert = other.bAxisInvert;
        if (poSRS)
            poSRS->Release();
        poSRS = other.poSRS;
        other.poSRS = nullptr;
        return *this;
    }

    ~SRSDesc()
    {
        if (poSRS)
            poSRS->Release();
    }

    CPL_DISALLOW_COPY_ASSIGN(SRSDesc)
};

class SRSCache
{
    std::map<std::string, SRSDesc> oMap{};
    SRSDesc *poLastDesc = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(SRSCache)

  public:
    SRSCache() = default;

    const SRSDesc &Get(const std::string &osSRSName)
    {
        if (poLastDesc && osSRSName == poLastDesc->osSRSName)
            return *poLastDesc;

        std::map<std::string, SRSDesc>::iterator oIter = oMap.find(osSRSName);
        if (oIter != oMap.end())
        {
            poLastDesc = &(oIter->second);
            return *poLastDesc;
        }

        SRSDesc oDesc;
        oDesc.osSRSName = osSRSName;
        oDesc.bAxisInvert = GML_IsSRSLatLongOrder(osSRSName.c_str());
        oDesc.poSRS = new OGRSpatialReference();
        oDesc.poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (oDesc.poSRS->SetFromUserInput(
                osSRSName.c_str(),
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
            OGRERR_NONE)
        {
            delete oDesc.poSRS;
            oDesc.poSRS = nullptr;
        }
        oMap[osSRSName] = std::move(oDesc);
        poLastDesc = &(oMap[osSRSName]);
        return *poLastDesc;
    }
};

}  // namespace

void *GML_BuildOGRGeometryFromList_CreateCache()
{
    return new SRSCache();
}

/************************************************************************/
/*                 GML_BuildOGRGeometryFromList_DestroyCache()          */
/************************************************************************/

void GML_BuildOGRGeometryFromList_DestroyCache(void *hCacheSRS)
{
    delete static_cast<SRSCache *>(hCacheSRS);
}

/************************************************************************/
/*                 GML_BuildOGRGeometryFromList()                       */
/************************************************************************/

OGRGeometry *GML_BuildOGRGeometryFromList(
    const CPLXMLNode *const *papsGeometry, bool bTryToMakeMultipolygons,
    bool bInvertAxisOrderIfLatLong, const char *pszDefaultSRSName,
    bool bConsiderEPSGAsURN, GMLSwapCoordinatesEnum eSwapCoordinates,
    int nPseudoBoolGetSecondaryGeometryOption, void *hCacheSRS,
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
            papsGeometry[i], nPseudoBoolGetSecondaryGeometryOption, 0, 0, false,
            true, bFaceHoleNegative);
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
                            nPseudoBoolGetSecondaryGeometryOption, hCacheSRS);
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
        SRSCache *poSRSCache = static_cast<SRSCache *>(hCacheSRS);
        const SRSDesc &oSRSDesc = poSRSCache->Get(pszNameLookup);
        poGeom->assignSpatialReference(oSRSDesc.poSRS);
        if ((eSwapCoordinates == GML_SWAP_AUTO && oSRSDesc.bAxisInvert &&
             bInvertAxisOrderIfLatLong) ||
            eSwapCoordinates == GML_SWAP_YES)
        {
            poGeom->swapXY();
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
/*                       GML_IsLegitSRSName()                           */
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
