/******************************************************************************
 * $Id: gt_wkt_srs.cpp 15643 2008-10-29 21:18:47Z warmerdam $
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
#include "cpl_serv.h"
#include "geo_tiffp.h"
#define CPL_ERROR_H_INCLUDED

#include "geo_normalize.h"
#include "geovalues.h"
#include "ogr_spatialref.h"
#include "gdal.h"
#include "xtiffio.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id: gt_wkt_srs.cpp 15643 2008-10-29 21:18:47Z warmerdam $");

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

char* ImagineCitationTranslation(char* psCitation, geokey_t keyID);
char** CitationStringParse(char* psCitation);
void SetLinearUnitCitation(GTIF* psGTIF, char* pszLinearUOMName);
void SetGeogCSCitation(GTIF * psGTIF, OGRSpatialReference *poSRS, char* angUnitName, int nDatum, short nSpheroid);
OGRBoolean SetCitationToSRS(GTIF* hGTIF, char* szCTString, geokey_t geoKey, 
              OGRSpatialReference* poSRS, OGRBoolean* linearUnitIsSet);
void GetGeogCSFromCitation(char* szGCSName, geokey_t geoKey, 
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
char* ImagineCitationTranslation(char* psCitation, geokey_t keyID)
{
    char* ret = NULL;
    if(!psCitation)
        return ret;
    if(EQUALN(psCitation, "IMAGINE GeoTIFF Support", strlen("IMAGINE GeoTIFF Support")))
    {
        char name[256];
        name[0] = '\0';
        // this is a handle IMAGING style citation
        char* p = NULL;
        p = strchr(psCitation, '$');
        if(p)
            p = strchr(p, '\n');
        if(p)
            p++;
        char* p1 = NULL;
        if(p)
            p1 = strchr(p, '\n');
        if(p1)
            p1++;
        if(p && p1)
        {
            switch (keyID)
            {
              case PCSCitationGeoKey:
                strcpy(name, "PCS Name = ");
                break;
              case GTCitationGeoKey:
                strcpy(name, "CS Name = ");
                break;
              case GeogCitationGeoKey:
                if(!strstr(p, "Unable to"))
                    strcpy(name, "GCS Name = ");
                break;
              default:
                break;
            }
            if(strlen(name)>0)
            {
                strncat(name, p, (p1-p));
                strcat(name, "|");
                name[strlen(name)] = '\0';
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
            p1++;
            strncat(name, p, p1-p);
            strcat(name, "|");
            name[strlen(name)] = '\0';
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
            strcat(name, "Datum = ");
            p1++;
            strncat(name, p, p1-p);
            strcat(name, "|");
            name[strlen(name)] = '\0';
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
            strcat(name, "Ellipsoid = ");
            p1++;
            strncat(name, p, p1-p);
            strcat(name, "|");
            name[strlen(name)] = '\0';
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
            strcat(name, "LUnits = ");
            p1++;
            strncat(name, p, p1-p);
            strcat(name, "|");
            name[strlen(name)] = '\0';
        }
        if(strlen(name) > 0)
        {
            ret = CPLStrdup(name);
        }
    }
    return ret;
}

/************************************************************************/
/*                        CitationStringParse()                         */
/*                                                                      */
/*      Parse a Citation string                                         */
/************************************************************************/
char** CitationStringParse(char* psCitation)
{
    char ** ret = NULL;
    if(!psCitation)
        return ret;

    ret = (char **) CPLCalloc(sizeof(char*), nCitationNameTypes); 
    char* pDelimit = NULL;
    char* pStr = psCitation;
    char name[512];
    int nameLen = strlen(psCitation);
    OGRBoolean nameFound = FALSE;
    while((pStr-psCitation+1)< nameLen)
    {
        if( (pDelimit = strstr(pStr, "|")) )
        {
            strncpy( name, pStr, pDelimit-pStr );
            name[pDelimit-pStr] = '\0';
            pStr = pDelimit+1;
        }
        else
        {
            strcpy (name, pStr);
            pStr += strlen(pStr);
        }
        if( strstr(name, "PCS Name = ") )
        {
            ret[CitPcsName] = CPLStrdup(name+strlen("PCS Name = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Projection Name = "))
        {
            ret[CitProjectionName] = CPLStrdup(name+strlen("Projection Name = "));
            nameFound = TRUE;
        }
        if(strstr(name, "LUnits = "))
        {
            ret[CitLUnitsName] = CPLStrdup(name+strlen("LUnits = "));
            nameFound = TRUE;
        }
        if(strstr(name, "GCS Name = "))
        {
            ret[CitGcsName] = CPLStrdup(name+strlen("GCS Name = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Datum = "))
        {
            ret[CitDatumName] = CPLStrdup(name+strlen("Datum = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Ellipsoid = "))
        {
            ret[CitEllipsoidName] = CPLStrdup(name+strlen("Ellipsoid = "));
            nameFound = TRUE;
        }
        if(strstr(name, "Primem = "))
        {
            ret[CitPrimemName] = CPLStrdup(name+strlen("Primem = "));    
            nameFound = TRUE;
        }
        if(strstr(name, "AUnits = "))
        {
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
    int n = 0;
    if( GTIFKeyGet( psGTIF, PCSCitationGeoKey, szName, 0, sizeof(szName) ) )
        n = strlen(szName);
    if(n>0)
    {
        if(szName[n-1] != '|')
            strcat(szName, "|");
        strcat(szName, "LUnits = ");
        strcat(szName, pszLinearUOMName);
        strcat(szName, "|");
    }
    else
    {
        strcpy(szName, "LUnits = ");
        strcat(szName, pszLinearUOMName);
    }
    GTIFKeySet( psGTIF, PCSCitationGeoKey, TYPE_ASCII, 0, szName );
    return;
}

/************************************************************************/
/*                         SetGeogCSCitation()                          */
/*                                                                      */
/*      Set geogcs Citation string                                      */
/************************************************************************/
void SetGeogCSCitation(GTIF * psGTIF, OGRSpatialReference *poSRS, char* angUnitName, int nDatum, short nSpheroid)
{
    char szName[256];
    size_t n = 0;
    if( GTIFKeyGet( psGTIF, GeogCitationGeoKey, szName, 0, sizeof(szName) ) )
        n = strlen(szName);
    if(n > 0 )
    {
        if(!EQUALN(szName, "GCS Name = ", strlen("GCS Name = ")))
        {
            char newName[256];
            strcpy(newName, "GCS Name = ");
            strcat(newName, szName);
            strcpy(szName, newName);
            n += strlen("GCS Name = ");
        }
    }
    if(nDatum == KvUserDefined )
    {
        const char* datumName = poSRS->GetAttrValue( "DATUM" );
        if(n > 0 && datumName && strlen(datumName) > 0)
        {
            strcat(szName, "|Datum = ");
            strcat(szName, datumName);
            strcat(szName, "|");
            GTIFKeySet( psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0, 
                        szName );
        }             
    }
    if(nSpheroid == KvUserDefined )
    {
        const char* spheroidName = poSRS->GetAttrValue( "SPHEROID" );
        if(n == strlen(szName))
            strcat(szName, "|");
        if(n > 0 && spheroidName && strlen(spheroidName) > 0)
        {
            strcat(szName, "Ellipsoid = ");
            strcat(szName, spheroidName);
            strcat(szName, "|");
            GTIFKeySet( psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0, 
                        szName );
        }             
    }
    const char* primemName = poSRS->GetAttrValue( "PRIMEM" );
    if(n == strlen(szName))
        strcat(szName, "|");
    if(n > 0 && primemName && strlen(primemName) > 0)
    {
        strcat(szName, "Primem = ");
        strcat(szName, primemName);
        strcat(szName, "|");
        GTIFKeySet( psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0, 
                    szName );
        double primemValue = poSRS->GetPrimeMeridian(NULL);
        if(angUnitName && !EQUAL(angUnitName, "Degree"))
        {
            double aUnit = poSRS->GetAngularUnits(NULL);
            primemValue *= aUnit;
        }
        GTIFKeySet( psGTIF, GeogPrimeMeridianLongGeoKey, TYPE_DOUBLE, 1, 
                    primemValue );
    } 
    if(angUnitName && !EQUAL(angUnitName, "Degree"))
    {
        if(n == strlen(szName))
            strcat(szName, "|");
        if(n > 0 && strlen(angUnitName) > 0)
        {
            strcat(szName, "AUnits = ");
            strcat(szName, angUnitName);
            strcat(szName, "|");
            GTIFKeySet( psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0, 
                        szName );
        } 
    }
    return;
}

/************************************************************************/
/*                          SetCitationToSRS()                          */
/*                                                                      */
/*      Parse and set Citation string to SRS                            */
/************************************************************************/
OGRBoolean SetCitationToSRS(GTIF* hGTIF, char* szCTString,  geokey_t geoKey,  OGRSpatialReference*	poSRS, OGRBoolean* linearUnitIsSet)
{
    OGRBoolean ret = FALSE;
    *linearUnitIsSet = FALSE;
    char* imgCTName = ImagineCitationTranslation(szCTString, geoKey);
    if(imgCTName)
    {
        strcpy(szCTString, imgCTName);
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
            GTIFKeyGet(hGTIF, ProjLinearUnitSizeGeoKey, &unitSize, 0,
                       sizeof(unitSize) );
            poSRS->SetLinearUnits( ctNames[CitLUnitsName], unitSize);
            *linearUnitIsSet = TRUE;
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
void GetGeogCSFromCitation(char* szGCSName,  geokey_t geoKey, 
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
        strcpy(szGCSName, imgCTName);
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


