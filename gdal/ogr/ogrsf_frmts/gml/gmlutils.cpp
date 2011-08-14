/******************************************************************************
 * $Id$
 *
 * Project:  GML Utils
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#include "cpl_string.h"

#include "gmlutils.h"
#include <string>

/************************************************************************/
/*                GML_ExtractSrsNameFromGeometry()                      */
/************************************************************************/

char* GML_ExtractSrsNameFromGeometry(char** papszGeometryList,
                                     int bConsiderEPSGAsURN)
{
    if (papszGeometryList != NULL &&
        *papszGeometryList != NULL &&
        papszGeometryList[1] == NULL)
    {
        const char* pszSRSName = strstr(*papszGeometryList, "srsName=\"");
        if (pszSRSName)
        {
            pszSRSName += strlen("srsName=\"");

            const char* pszEndQuote = strchr(pszSRSName, '"');
            if (pszEndQuote == NULL)
                return NULL;

            int nLen = pszEndQuote - pszSRSName;

            char* pszRet;
            if (strncmp(pszSRSName, "EPSG:", 5) == 0 &&
                bConsiderEPSGAsURN)
            {
                pszRet = (char*) CPLMalloc(22 + nLen-5 + 1);
                memcpy(pszRet, "urn:ogc:def:crs:EPSG::", 22);
                memcpy(pszRet + 22, pszSRSName+5, nLen-5);
                pszRet[22 + nLen-5] = '\0';
            }
            else if (strncmp(pszSRSName, "http://www.opengis.net/gml/srs/epsg.xml#", 40) == 0)
            {
                pszRet = (char*) CPLMalloc(5 + nLen-40 + 1);
                memcpy(pszRet, "EPSG:", 5);
                memcpy(pszRet + 5, pszSRSName+40, nLen-40);
                pszRet[5 + nLen-40] = '\0';
            }
            else
            {
                pszRet = (char*) CPLMalloc(nLen + 1);
                memcpy(pszRet, pszSRSName, nLen);
                pszRet[nLen] = '\0';
            }

            return pszRet;
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
                if (oSRS.EPSGTreatsAsLatLong())
                    return TRUE;
            }
        }
    }
    return FALSE;
}


/************************************************************************/
/*                GML_BuildOGRGeometryFromList_CreateCache()            */
/************************************************************************/

class SRSCache
{
public:
    std::string osLastSRSName;
    int   bAxisInvertLastSRSName;
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

OGRGeometry* GML_BuildOGRGeometryFromList(char** papszGeometryList,
                                          int bTryToMakeMultipolygons,
                                          int bInvertAxisOrderIfLatLong,
                                          const char* pszDefaultSRSName,
                                          int bConsiderEPSGAsURN,
                                          void* hCacheSRS)
{
    OGRGeometry* poGeom = NULL;
    if( papszGeometryList != NULL )
    {
        char** papszIter = papszGeometryList;
        OGRGeometryCollection* poCollection = NULL;
        while(*papszIter)
        {
            OGRGeometry* poSubGeom = OGRGeometryFactory::createFromGML( *papszIter );
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
                            return GML_BuildOGRGeometryFromList(papszGeometryList, FALSE,
                                                                bInvertAxisOrderIfLatLong,
                                                                pszDefaultSRSName,
                                                                bConsiderEPSGAsURN,
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
            papszIter ++;
        }
    }

    if ( poGeom != NULL && bInvertAxisOrderIfLatLong )
    {
        char* pszSRSName = GML_ExtractSrsNameFromGeometry(papszGeometryList,
                                                          bConsiderEPSGAsURN);
        const char* pszNameLookup = pszSRSName ? pszSRSName : pszDefaultSRSName;
        if (pszNameLookup != NULL)
        {
            SRSCache* poSRSCache = (SRSCache*)hCacheSRS;
            int bSwap;
            if (strcmp(poSRSCache->osLastSRSName.c_str(), pszNameLookup) == 0)
            {
                bSwap = poSRSCache->bAxisInvertLastSRSName;
            }
            else
            {
                int bSwap = GML_IsSRSLatLongOrder(pszNameLookup);
                poSRSCache->osLastSRSName = pszNameLookup;
                poSRSCache->bAxisInvertLastSRSName= bSwap;
            }
            if (bSwap)
                poGeom->swapXY();
        }
        CPLFree(pszSRSName);
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
                if (bLongSRS && !((OGRSpatialReference*)poSRS)->EPSGTreatsAsLatLong())
                {
                    OGRSpatialReference oSRS;
                    if (oSRS.importFromEPSGA(atoi(pszAuthCode)) == OGRERR_NONE)
                    {
                        if (oSRS.EPSGTreatsAsLatLong())
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
