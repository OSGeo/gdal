/******************************************************************************
 *
 * Project:  GML Utils
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                GML_ExtractSrsNameFromGeometry()                      */
/************************************************************************/

const char *
GML_ExtractSrsNameFromGeometry(const CPLXMLNode *const *papsGeometry,
                               std::string &osWork, bool bConsiderEPSGAsURN)
{
    if (papsGeometry[0] != NULL && papsGeometry[1] == NULL)
    {
        const char *pszSRSName =
            CPLGetXMLValue(const_cast<CPLXMLNode *>(papsGeometry[0]),
                           "srsName", NULL);
        if(pszSRSName)
        {
            const int nLen = static_cast<int>(strlen(pszSRSName));

            if(STARTS_WITH(pszSRSName, "EPSG:") && bConsiderEPSGAsURN)
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
    return NULL;
}

/************************************************************************/
/*                       GML_IsSRSLatLongOrder()                        */
/************************************************************************/

bool GML_IsSRSLatLongOrder(const char *pszSRSName)
{
    if(pszSRSName == NULL)
        return false;

    if(STARTS_WITH(pszSRSName, "urn:") && strstr(pszSRSName, ":4326") != NULL)
    {
        // Shortcut.
        return true;
    }
    else if( !EQUALN(pszSRSName, "EPSG:", 5) )
    {
        OGRSpatialReference oSRS;
        if(oSRS.SetFromUserInput(pszSRSName) == OGRERR_NONE)
        {
            if(oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting())
                return true;
        }
        return false;
    }
    /* fguuid:jgd20??.bl (Japanese FGD GML v4) */
    else if (strncmp(pszSRSName, "fguuid:jgd", 10) == 0 && strstr(pszSRSName, ".bl") != NULL) {
        return true;
    }

    return false;
}

/************************************************************************/
/*                GML_BuildOGRGeometryFromList_CreateCache()            */
/************************************************************************/

class SRSDesc
{
public:
    std::string          osSRSName;
    bool                 bAxisInvert;
    OGRSpatialReference* poSRS;

    SRSDesc() : bAxisInvert(false), poSRS(NULL) {}
};

class SRSCache
{
    std::map<std::string, SRSDesc> oMap;
    SRSDesc oLastDesc;

  public:
    SRSCache() {}

    ~SRSCache()
    {
        std::map<std::string, SRSDesc>::iterator oIter;
        for( oIter = oMap.begin(); oIter != oMap.end(); ++oIter )
        {
            if( oIter->second.poSRS != NULL )
                oIter->second.poSRS->Release();
        }
    }

    SRSDesc &Get(const std::string &osSRSName)
    {
        if( osSRSName == oLastDesc.osSRSName )
            return oLastDesc;

        std::map<std::string, SRSDesc>::iterator oIter = oMap.find(osSRSName);
        if( oIter != oMap.end() )
        {
            oLastDesc = oIter->second;
            return oLastDesc;
        }

        oLastDesc.osSRSName = osSRSName;
        oLastDesc.bAxisInvert = GML_IsSRSLatLongOrder(osSRSName.c_str());
        oLastDesc.poSRS = new OGRSpatialReference();
        if( oLastDesc.poSRS->SetFromUserInput(osSRSName.c_str()) !=
            OGRERR_NONE )
        {
            delete oLastDesc.poSRS;
            oLastDesc.poSRS = NULL;
        }
        oMap[osSRSName] = oLastDesc;
        return oLastDesc;
    }
};

void *GML_BuildOGRGeometryFromList_CreateCache() { return new SRSCache(); }

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

OGRGeometry* GML_BuildOGRGeometryFromList(
    const CPLXMLNode *const *papsGeometry,
    bool bTryToMakeMultipolygons,
    bool bInvertAxisOrderIfLatLong,
    const char *pszDefaultSRSName,
    bool bConsiderEPSGAsURN,
    GMLSwapCoordinatesEnum eSwapCoordinates,
    int nPseudoBoolGetSecondaryGeometryOption,
    void *hCacheSRS,
    bool bFaceHoleNegative)
{
    OGRGeometry *poGeom = NULL;
    OGRGeometryCollection *poCollection = NULL;
    for( int i = 0; papsGeometry[i] != NULL; i++ )
    {
        OGRGeometry* poSubGeom = GML2OGRGeometry_XMLNode(
            papsGeometry[i],
            nPseudoBoolGetSecondaryGeometryOption,
            0, 0, false, true,
            bFaceHoleNegative);
        if (poSubGeom)
        {
            if (poGeom == NULL)
            {
                poGeom = poSubGeom;
            }
            else
            {
                if (poCollection == NULL)
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
                    else if(bTryToMakeMultipolygons &&
                            wkbFlatten(poGeom->getGeometryType()) ==
                                wkbMultiPolygon &&
                            wkbFlatten(poSubGeom->getGeometryType()) ==
                                wkbPolygon)
                    {
                        OGRGeometryCollection *poGeomColl =
                            static_cast<OGRGeometryCollection *>(poGeom);
                        poGeomColl->addGeometryDirectly(poSubGeom);
                    }
                    else if(bTryToMakeMultipolygons &&
                            wkbFlatten(poGeom->getGeometryType()) ==
                                wkbMultiPolygon &&
                            wkbFlatten(poSubGeom->getGeometryType()) ==
                                wkbMultiPolygon)
                    {
                        OGRGeometryCollection *poGeomColl =
                            static_cast<OGRGeometryCollection *>(poGeom);
                        OGRGeometryCollection *poGeomColl2 =
                            static_cast<OGRGeometryCollection *>(poSubGeom);
                        int nCount = poGeomColl2->getNumGeometries();
                        for(int j = 0; j < nCount; j++)
                        {
                            poGeomColl->addGeometry(
                                poGeomColl2->getGeometryRef(j));
                        }
                        delete poSubGeom;
                    }
                    else if(bTryToMakeMultipolygons &&
                            wkbFlatten(poGeom->getGeometryType()) ==
                                wkbMultiPolygon)
                    {
                        delete poGeom;
                        delete poSubGeom;
                        return GML_BuildOGRGeometryFromList(
                            papsGeometry, false,
                            bInvertAxisOrderIfLatLong,
                            pszDefaultSRSName,
                            bConsiderEPSGAsURN,
                            eSwapCoordinates,
                            nPseudoBoolGetSecondaryGeometryOption,
                            hCacheSRS);
                    }
                    else
                    {
                        poCollection = new OGRGeometryCollection();
                        poCollection->addGeometryDirectly(poGeom);
                        poGeom = poCollection;
                    }
                }
                if (poCollection != NULL)
                {
                    poCollection->addGeometryDirectly(poSubGeom);
                }
            }
        }
    }

    if( poGeom == NULL )
        return NULL;

    std::string osWork;
    const char *pszSRSName = GML_ExtractSrsNameFromGeometry(
        papsGeometry, osWork, bConsiderEPSGAsURN);
    const char *pszNameLookup = pszSRSName;
    if( pszNameLookup == NULL )
        pszNameLookup = pszDefaultSRSName;

    if (pszNameLookup != NULL)
    {
        SRSCache *poSRSCache = static_cast<SRSCache *>(hCacheSRS);
        SRSDesc &oSRSDesc = poSRSCache->Get(pszNameLookup);
        poGeom->assignSpatialReference(oSRSDesc.poSRS);
        if( (eSwapCoordinates == GML_SWAP_AUTO &&
            oSRSDesc.bAxisInvert && bInvertAxisOrderIfLatLong) ||
            eSwapCoordinates == GML_SWAP_YES )
        {
            poGeom->swapXY();
        }
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
    if (poSRS == NULL)
        return CPLStrdup("");

    const char *pszTarget = poSRS->IsProjected() ? "PROJCS" : "GEOGCS";
    const char *pszAuthName = poSRS->GetAuthorityName(pszTarget);
    const char *pszAuthCode = poSRS->GetAuthorityCode(pszTarget);
    if( NULL != pszAuthName && NULL != pszAuthCode )
    {
        if( EQUAL( pszAuthName, "EPSG" ) &&
            eSRSNameFormat != SRSNAME_SHORT &&
            !(const_cast<OGRSpatialReference *>(poSRS)->EPSGTreatsAsLatLong() ||
              const_cast<OGRSpatialReference *>(poSRS)->
                  EPSGTreatsAsNorthingEasting()))
        {
            OGRSpatialReference oSRS;
            if (oSRS.importFromEPSGA(atoi(pszAuthCode)) == OGRERR_NONE)
            {
                if(oSRS.EPSGTreatsAsLatLong() ||
                   oSRS.EPSGTreatsAsNorthingEasting())
                    *pbCoordSwap = true;
            }
        }

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
