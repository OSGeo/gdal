/******************************************************************************
 * $Id$
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

#include "gmlutils.h"

#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include <string>
#include <map>

/************************************************************************/
/*                GML_ExtractSrsNameFromGeometry()                      */
/************************************************************************/

const char* GML_ExtractSrsNameFromGeometry(const CPLXMLNode* const * papsGeometry,
                                     std::string& osWork,
                                     int bConsiderEPSGAsURN)
{
    if (papsGeometry[0] != NULL && papsGeometry[1] == NULL)
    {
        const char* pszSRSName = CPLGetXMLValue((CPLXMLNode*)papsGeometry[0], "srsName", NULL);
        if (pszSRSName)
        {
            int nLen = strlen(pszSRSName);

            if (strncmp(pszSRSName, "EPSG:", 5) == 0 &&
                bConsiderEPSGAsURN)
            {
                osWork.reserve(22 + nLen-5);
                osWork.assign("urn:ogc:def:crs:EPSG::", 22);
                osWork.append(pszSRSName+5, nLen-5);
                return osWork.c_str();
            }
            else if (strncmp(pszSRSName, "http://www.opengis.net/gml/srs/epsg.xml#", 40) == 0)
            {
                osWork.reserve(5 + nLen-40 );
                osWork.assign("EPSG:", 5);
                osWork.append(pszSRSName+40, nLen-40);
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

int GML_IsSRSLatLongOrder(const char* pszSRSName)
{
    if (pszSRSName == NULL)
        return FALSE;

    if (strncmp(pszSRSName, "urn:", 4) == 0)
    {
        if (strstr(pszSRSName, ":4326") != NULL)
        {
            /* Shortcut ... */
            return TRUE;
        }
        else
        {
            OGRSpatialReference oSRS;
            if (oSRS.importFromURN(pszSRSName) == OGRERR_NONE)
            {
                if (oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting())
                    return TRUE;
            }
        }
    }
    return FALSE;
}


/************************************************************************/
/*                GML_BuildOGRGeometryFromList_CreateCache()            */
/************************************************************************/

class SRSDesc
{
public:
    std::string          osSRSName;
    int                  bAxisInvert;
    OGRSpatialReference* poSRS;

    SRSDesc() : bAxisInvert(FALSE), poSRS(NULL)
    {
    }
};

class SRSCache
{
    std::map<std::string, SRSDesc> oMap;
    SRSDesc                        oLastDesc;

public:

    SRSCache()
    {
    }

    ~SRSCache()
    {
        std::map<std::string, SRSDesc>::iterator oIter;
        for( oIter = oMap.begin(); oIter != oMap.end(); ++oIter )
        {
            if( oIter->second.poSRS != NULL )
                oIter->second.poSRS->Release();
        }
    }

    SRSDesc& Get(const std::string& osSRSName)
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
        if( oLastDesc.poSRS->SetFromUserInput(osSRSName.c_str()) != OGRERR_NONE )
        {
            delete oLastDesc.poSRS;
            oLastDesc.poSRS = NULL;
        }
        oMap[osSRSName] = oLastDesc;
        return oLastDesc;
    }
};

void* GML_BuildOGRGeometryFromList_CreateCache()
{
    return new SRSCache();
}

/************************************************************************/
/*                 GML_BuildOGRGeometryFromList_DestroyCache()          */
/************************************************************************/

void GML_BuildOGRGeometryFromList_DestroyCache(void* hCacheSRS)
{
    delete (SRSCache*)hCacheSRS;
}

/************************************************************************/
/*                 GML_BuildOGRGeometryFromList()                       */
/************************************************************************/

OGRGeometry* GML_BuildOGRGeometryFromList(const CPLXMLNode* const * papsGeometry,
                                          int bTryToMakeMultipolygons,
                                          int bInvertAxisOrderIfLatLong,
                                          const char* pszDefaultSRSName,
                                          int bConsiderEPSGAsURN,
                                          int bGetSecondaryGeometryOption,
                                          void* hCacheSRS,
                                          int bFaceHoleNegative)
{
    OGRGeometry* poGeom = NULL;
    int i;
    OGRGeometryCollection* poCollection = NULL;
    for(i=0;papsGeometry[i] != NULL;i++)
    {
        OGRGeometry* poSubGeom = GML2OGRGeometry_XMLNode( papsGeometry[i],
                                                          bGetSecondaryGeometryOption,
                                                          0, 0, FALSE, TRUE,
                                                          bFaceHoleNegative );
        if (poSubGeom)
        {
            if (poGeom == NULL)
                poGeom = poSubGeom;
            else
            {
                if (poCollection == NULL)
                {
                    if (bTryToMakeMultipolygons &&
                        wkbFlatten(poGeom->getGeometryType()) == wkbPolygon &&
                        wkbFlatten(poSubGeom->getGeometryType()) == wkbPolygon)
                    {
                        OGRGeometryCollection* poGeomColl = new OGRMultiPolygon();
                        poGeomColl->addGeometryDirectly(poGeom);
                        poGeomColl->addGeometryDirectly(poSubGeom);
                        poGeom = poGeomColl;
                    }
                    else if (bTryToMakeMultipolygons &&
                                wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon &&
                                wkbFlatten(poSubGeom->getGeometryType()) == wkbPolygon)
                    {
                        OGRGeometryCollection* poGeomColl = (OGRGeometryCollection* )poGeom;
                        poGeomColl->addGeometryDirectly(poSubGeom);
                    }
                    else if (bTryToMakeMultipolygons &&
                                wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon &&
                                wkbFlatten(poSubGeom->getGeometryType()) == wkbMultiPolygon)
                    {
                        OGRGeometryCollection* poGeomColl = (OGRGeometryCollection* )poGeom;
                        OGRGeometryCollection* poGeomColl2 = (OGRGeometryCollection* )poSubGeom;
                        int nCount = poGeomColl2->getNumGeometries();
                        int i;
                        for(i=0;i<nCount;i++)
                        {
                            poGeomColl->addGeometry(poGeomColl2->getGeometryRef(i));
                        }
                        delete poSubGeom;
                    }
                    else if (bTryToMakeMultipolygons &&
                                wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
                    {
                        delete poGeom;
                        delete poSubGeom;
                        return GML_BuildOGRGeometryFromList(papsGeometry, FALSE,
                                                            bInvertAxisOrderIfLatLong,
                                                            pszDefaultSRSName,
                                                            bConsiderEPSGAsURN,
                                                            bGetSecondaryGeometryOption,
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
    const char* pszSRSName = GML_ExtractSrsNameFromGeometry(papsGeometry, osWork,
                                                            bConsiderEPSGAsURN);
    const char* pszNameLookup = pszSRSName;
    if( pszNameLookup == NULL )
        pszNameLookup = pszDefaultSRSName;

    if (pszNameLookup != NULL)
    {
        SRSCache* poSRSCache = (SRSCache*)hCacheSRS;
        SRSDesc& oSRSDesc = poSRSCache->Get(pszNameLookup);
        poGeom->assignSpatialReference(oSRSDesc.poSRS);
        if (oSRSDesc.bAxisInvert && bInvertAxisOrderIfLatLong)
            poGeom->swapXY();
    }

    return poGeom;
}

/************************************************************************/
/*                           GML_GetSRSName()                           */
/************************************************************************/

char* GML_GetSRSName(const OGRSpatialReference* poSRS, int bLongSRS, int *pbCoordSwap)
{
    *pbCoordSwap = FALSE;
    if (poSRS == NULL)
        return CPLStrdup("");

    const char* pszAuthName = NULL;
    const char* pszAuthCode = NULL;
    const char* pszTarget = NULL;

    if (poSRS->IsProjected())
        pszTarget = "PROJCS";
    else
        pszTarget = "GEOGCS";

    char szSrsName[50];
    szSrsName[0] = 0;

    pszAuthName = poSRS->GetAuthorityName( pszTarget );
    if( NULL != pszAuthName )
    {
        if( EQUAL( pszAuthName, "EPSG" ) )
        {
            pszAuthCode = poSRS->GetAuthorityCode( pszTarget );
            if( NULL != pszAuthCode && strlen(pszAuthCode) < 10 )
            {
                if (bLongSRS && !(((OGRSpatialReference*)poSRS)->EPSGTreatsAsLatLong() ||
                                  ((OGRSpatialReference*)poSRS)->EPSGTreatsAsNorthingEasting()))
                {
                    OGRSpatialReference oSRS;
                    if (oSRS.importFromEPSGA(atoi(pszAuthCode)) == OGRERR_NONE)
                    {
                        if (oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting())
                            *pbCoordSwap = TRUE;
                    }
                }

                if (bLongSRS)
                {
                    sprintf( szSrsName, " srsName=\"urn:ogc:def:crs:%s::%s\"",
                        pszAuthName, pszAuthCode );
                }
                else
                {
                    sprintf( szSrsName, " srsName=\"%s:%s\"",
                            pszAuthName, pszAuthCode );
                }
            }
        }
    }

    return CPLStrdup(szSrsName);
}
