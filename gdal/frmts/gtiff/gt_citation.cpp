/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements special parsing of Imagine citation strings, and
 *           to encode PE String info in citation fields as needed.
 * Author:   Xiuguang Zhou (ESRI)
 *
 ******************************************************************************
 * Copyright (c) 2008, Xiuguang Zhou (ESRI)
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

#include "cpl_port.h"
#include "cpl_string.h"

#include "geo_normalize.h"
#include "geovalues.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

#define nCitationNameTypes 9
typedef enum 
{
  CitCsName = 0,
  CitPcsName = 1,
  CitProjectionName = 2,
  CitLUnitsName = 3,
  CitGcsName = 4,
  CitDatumName = 5,
  CitEllipsoidName = 6,
  CitPrimemName = 7,
  CitAUnitsName = 8
} CitationNameType;

char* ImagineCitationTranslation(const char* psCitation, geokey_t keyID);
char** CitationStringParse(const char* psCitation);
void SetLinearUnitCitation(GTIF* psGTIF, char* pszLinearUOMName);
void SetGeogCSCitation(GTIF * psGTIF, OGRSpatialReference *poSRS, char* angUnitName, int nDatum, short nSpheroid);
OGRBoolean SetCitationToSRS(GTIF* hGTIF, char* szCTString, int nCTStringLen,
                            geokey_t geoKey,  OGRSpatialReference* poSRS, OGRBoolean* linearUnitIsSet);
void GetGeogCSFromCitation(char* szGCSName, int nGCSName,
                           geokey_t geoKey, 
                          char	**ppszGeogName,
                          char	**ppszDatumName,
                          char	**ppszPMName,
                          char	**ppszSpheroidName,
                          char	**ppszAngularUnits);

/************************************************************************/
/*                     ImagineCitationTranslation()                     */
/*                                                                      */
/*      Translate ERDAS Imagine GeoTif citation                         */
/************************************************************************/
char* ImagineCitationTranslation(const char* psCitation, geokey_t keyID)
{
    char* ret = NULL;
    if(!psCitation)
        return ret;
    if(EQUALN(psCitation, "IMAGINE GeoTIFF Support", strlen("IMAGINE GeoTIFF Support")))
    {
        CPLString osName;

        // this is a handle IMAGING style citation
        const char* p = NULL;
        p = strchr(psCitation, '$');
        if(p)
            p = strchr(p, '\n');
        if(p)
            p++;
        const char* p1 = NULL;
        if(p)
            p1 = strchr(p, '\n');
        if(p && p1)
        {
            switch (keyID)
            {
              case PCSCitationGeoKey:
                osName = "PCS Name = ";
                break;
              case GTCitationGeoKey:
                osName = "CS Name = ";
                break;
              case GeogCitationGeoKey:
                if(!strstr(p, "Unable to"))
                    osName = "GCS Name = ";
                break;
              default:
                break;
            }
            if(strlen(osName)>0)
            {
                osName.append(p, p1-p);
                osName += "|";
            }
        }
        p = strstr(psCitation, "Projection Name = ");
        if(p)
        {
            p += strlen("Projection Name = ");
            p1 = strchr(p, '\n');
            if(!p1)
                p1 = strchr(p, '\0');
        }
        if(p && p1)
        {
            osName.append(p, p1-p);
            osName += "|";
        }
        p = strstr(psCitation, "Datum = ");
        if(p)
        {
            p += strlen("Datum = ");
            p1 = strchr(p, '\n');
            if(!p1)
                p1 = strchr(p, '\0');
        }
        if(p && p1)
        {
            osName += "Datum = ";
            osName.append(p, p1-p);
            osName += "|";
        }
        p = strstr(psCitation, "Ellipsoid = ");
        if(p)
        {
            p += strlen("Ellipsoid = ");
            p1 = strchr(p, '\n');
            if(!p1)
                p1 = strchr(p, '\0');
        }
        if(p && p1)
        {
            osName += "Ellipsoid = ";
            osName.append(p, p1-p);
            osName += "|";
        }
        p = strstr(psCitation, "Units = ");
        if(p)
        {
            p += strlen("Units = ");
            p1 = strchr(p, '\n');
            if(!p1)
                p1 = strchr(p, '\0');
        }
        if(p && p1)
        {
            osName += "LUnits = ";
            osName.append(p, p1-p);
            osName += "|";
        }
        if(strlen(osName) > 0)
        {
            ret = CPLStrdup(osName);
        }
    }
    return ret;
}

/************************************************************************/
/*                        CitationStringParse()                         */
/*                                                                      */
/*      Parse a Citation string                                         */
/************************************************************************/
char** CitationStringParse(const char* psCitation)
{
    char ** ret = NULL;
    if(!psCitation)
        return ret;

    ret = (char **) CPLCalloc(sizeof(char*), nCitationNameTypes); 
    const char* pDelimit = NULL;
    const char* pStr = psCitation;
    CPLString osName;
    int nCitationLen = strlen(psCitation);
    OGRBoolean nameFound = FALSE;
    while((pStr-psCitation+1)< nCitationLen)
    {
        if( (pDelimit = strstr(pStr, "|")) != NULL )
        {
            osName = "";
            osName.append(pStr, pDelimit-pStr);
            pStr = pDelimit+1;
        }
        else
        {
            osName = pStr;
            pStr += strlen(pStr);
        }
        const char* name = osName.c_str();
        if( strstr(name, "PCS Name = ") )
        {
            if (!ret[CitPcsName])
                ret[CitPcsName] = CPLStrdup(name+strlen("PCS Name = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Projection Name = "))
        {
            if (!ret[CitProjectionName])
                ret[CitProjectionName] = CPLStrdup(name+strlen("Projection Name = "));
            nameFound = TRUE;
        }
        if(strstr(name, "LUnits = "))
        {
            if (!ret[CitLUnitsName])
                ret[CitLUnitsName] = CPLStrdup(name+strlen("LUnits = "));
            nameFound = TRUE;
        }
        if(strstr(name, "GCS Name = "))
        {
            if (!ret[CitGcsName])
                ret[CitGcsName] = CPLStrdup(name+strlen("GCS Name = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Datum = "))
        {
            if (!ret[CitDatumName])
                ret[CitDatumName] = CPLStrdup(name+strlen("Datum = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Ellipsoid = "))
        {
            if (!ret[CitEllipsoidName])
                ret[CitEllipsoidName] = CPLStrdup(name+strlen("Ellipsoid = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Primem = "))
        {
            if (!ret[CitPrimemName])
                ret[CitPrimemName] = CPLStrdup(name+strlen("Primem = "));
            nameFound = TRUE;
        }
        if(strstr(name, "AUnits = "))
        {
            if (!ret[CitAUnitsName])
                ret[CitAUnitsName] = CPLStrdup(name+strlen("AUnits = "));
            nameFound = TRUE;
        }
    }
    if(!nameFound)
    {
        CPLFree( ret );
        ret = (char**)NULL;
    }
    return ret;
}

/************************************************************************/
/*                       SetLinearUnitCitation()                        */
/*                                                                      */
/*      Set linear unit Citation string                                 */
/************************************************************************/
void SetLinearUnitCitation(GTIF* psGTIF, char* pszLinearUOMName)
{
    char szName[512];
    CPLString osCitation;
    int n = 0;
    if( GTIFKeyGet( psGTIF, PCSCitationGeoKey, szName, 0, sizeof(szName) ) )
        n = strlen(szName);
    if(n>0)
    {
        osCitation = szName;
        if(osCitation[n-1] != '|')
            osCitation += "|";
        osCitation += "LUnits = ";
        osCitation += pszLinearUOMName;
        osCitation += "|";
    }
    else
    {
        osCitation = "LUnits = ";
        osCitation += pszLinearUOMName;
    }
    GTIFKeySet( psGTIF, PCSCitationGeoKey, TYPE_ASCII, 0, osCitation.c_str() );
    return;
}

/************************************************************************/
/*                         SetGeogCSCitation()                          */
/*                                                                      */
/*      Set geogcs Citation string                                      */
/************************************************************************/
void SetGeogCSCitation(GTIF * psGTIF, OGRSpatialReference *poSRS, char* angUnitName, int nDatum, short nSpheroid)
{
    int bRewriteGeogCitation = FALSE;
    char szName[256];
    CPLString osCitation;
    size_t n = 0;
    if( GTIFKeyGet( psGTIF, GeogCitationGeoKey, szName, 0, sizeof(szName) ) )
        n = strlen(szName);
    if (n == 0)
        return;

    if(!EQUALN(szName, "GCS Name = ", strlen("GCS Name = ")))
    {
        osCitation = "GCS Name = ";
        osCitation += szName;
    }
    else
    {
        osCitation = szName;
    }

    if(nDatum == KvUserDefined )
    {
        const char* datumName = poSRS->GetAttrValue( "DATUM" );
        if(datumName && strlen(datumName) > 0)
        {
            osCitation += "|Datum = ";
            osCitation += datumName;
            bRewriteGeogCitation = TRUE;
        }
    }
    if(nSpheroid == KvUserDefined )
    {
        const char* spheroidName = poSRS->GetAttrValue( "SPHEROID" );
        if(spheroidName && strlen(spheroidName) > 0)
        {
            osCitation += "|Ellipsoid = ";
            osCitation += spheroidName;
            bRewriteGeogCitation = TRUE;
        }
    }

    const char* primemName = poSRS->GetAttrValue( "PRIMEM" );
    if(primemName && strlen(primemName) > 0)
    {
        osCitation += "|Primem = ";
        osCitation += primemName;
        bRewriteGeogCitation = TRUE;

        double primemValue = poSRS->GetPrimeMeridian(NULL);
        if(angUnitName && !EQUAL(angUnitName, "Degree"))
        {
            double aUnit = poSRS->GetAngularUnits(NULL);
            primemValue *= aUnit;
        }
        GTIFKeySet( psGTIF, GeogPrimeMeridianLongGeoKey, TYPE_DOUBLE, 1, 
                    primemValue );
    } 
    if(angUnitName && strlen(angUnitName) > 0 && !EQUAL(angUnitName, "Degree"))
    {
        osCitation += "|AUnits = ";
        osCitation += angUnitName;
        bRewriteGeogCitation = TRUE;
    }

    if (osCitation[strlen(osCitation) - 1] != '|')
        osCitation += "|";

    if (bRewriteGeogCitation)
        GTIFKeySet( psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0, osCitation.c_str() );

    return;
}

/************************************************************************/
/*                          SetCitationToSRS()                          */
/*                                                                      */
/*      Parse and set Citation string to SRS                            */
/************************************************************************/
OGRBoolean SetCitationToSRS(GTIF* hGTIF, char* szCTString, int nCTStringLen,
                            geokey_t geoKey,  OGRSpatialReference*	poSRS, OGRBoolean* linearUnitIsSet)
{
    OGRBoolean ret = FALSE;
    *linearUnitIsSet = FALSE;
    char* imgCTName = ImagineCitationTranslation(szCTString, geoKey);
    if(imgCTName)
    {
        strncpy(szCTString, imgCTName, nCTStringLen);
        szCTString[nCTStringLen-1] = '\0';
        CPLFree( imgCTName );
    }
    char** ctNames = CitationStringParse(szCTString);
    if(ctNames)
    {
        if( poSRS->GetRoot() == NULL)
            poSRS->SetNode( "PROJCS", "unnamed" );
        if(ctNames[CitPcsName])
        {
            poSRS->SetNode( "PROJCS", ctNames[CitPcsName] );
            ret = TRUE;
        }
        else if(geoKey != GTCitationGeoKey) 
        {
            char    szPCSName[128];
            if( GTIFKeyGet( hGTIF, GTCitationGeoKey, szPCSName, 0, sizeof(szPCSName) ) )
            {
                poSRS->SetNode( "PROJCS", szPCSName );
                ret = TRUE;
            }
        }
    
        if(ctNames[CitProjectionName])
            poSRS->SetProjection( ctNames[CitProjectionName] );

        if(ctNames[CitLUnitsName])
        {
            double unitSize;
            if (GTIFKeyGet(hGTIF, ProjLinearUnitSizeGeoKey, &unitSize, 0,
                           sizeof(unitSize) ))
            {
                poSRS->SetLinearUnits( ctNames[CitLUnitsName], unitSize);
                *linearUnitIsSet = TRUE;
            }
        }
        for(int i= 0; i<nCitationNameTypes; i++) 
            CPLFree( ctNames[i] );
        CPLFree( ctNames );
    }
    return ret;
}

/************************************************************************/
/*                       GetGeogCSFromCitation()                        */
/*                                                                      */
/*      Parse and get geogcs names from a Citation string               */
/************************************************************************/
void GetGeogCSFromCitation(char* szGCSName, int nGCSName,
                           geokey_t geoKey, 
                           char	**ppszGeogName,
                           char	**ppszDatumName,
                           char	**ppszPMName,
                           char	**ppszSpheroidName,
                           char	**ppszAngularUnits)
{
    *ppszGeogName = *ppszDatumName = *ppszPMName = 
        *ppszSpheroidName = *ppszAngularUnits = NULL;

    char* imgCTName = ImagineCitationTranslation(szGCSName, geoKey);
    if(imgCTName)
    {
        strncpy(szGCSName, imgCTName, nGCSName);
        szGCSName[nGCSName-1] = '\0';
        CPLFree( imgCTName );
    }
    char** ctNames = CitationStringParse(szGCSName);
    if(ctNames)
    {
        if(ctNames[CitGcsName])
            *ppszGeogName = CPLStrdup( ctNames[CitGcsName] );

        if(ctNames[CitDatumName])
            *ppszDatumName = CPLStrdup( ctNames[CitDatumName] );

        if(ctNames[CitEllipsoidName])
            *ppszSpheroidName = CPLStrdup( ctNames[CitEllipsoidName] );

        if(ctNames[CitPrimemName])
            *ppszPMName = CPLStrdup( ctNames[CitPrimemName] );

        if(ctNames[CitAUnitsName])
            *ppszAngularUnits = CPLStrdup( ctNames[CitAUnitsName] );

        for(int i= 0; i<nCitationNameTypes; i++)
            CPLFree( ctNames[i] );
        CPLFree( ctNames );
    }
    return;
}


