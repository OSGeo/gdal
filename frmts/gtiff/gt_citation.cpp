/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements special parsing of Imagine citation strings, and
 *           to encode PE String info in citation fields as needed.
 * Author:   Xiuguang Zhou (ESRI)
 *
 ******************************************************************************
 * Copyright (c) 2008, Xiuguang Zhou (ESRI)
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "gt_citation.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "geokeys.h"
#include "geotiff.h"
#include "geovalues.h"
#include "gt_wkt_srs_priv.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

static const char * const apszUnitMap[] = {
    "meters", "1.0",
    "meter", "1.0",
    "m", "1.0",
    "centimeters", "0.01",
    "centimeter", "0.01",
    "cm", "0.01",
    "millimeters", "0.001",
    "millimeter", "0.001",
    "mm", "0.001",
    "kilometers", "1000.0",
    "kilometer", "1000.0",
    "km", "1000.0",
    "us_survey_feet", "0.3048006096012192",
    "us_survey_foot", "0.3048006096012192",
    "feet", "0.3048006096012192",
    "foot", "0.3048006096012192",
    "ft", "0.3048006096012192",
    "international_feet", "0.3048",
    "international_foot", "0.3048",
    "inches", "0.0254000508001",
    "inch", "0.0254000508001",
    "in", "0.0254000508001",
    "yards", "0.9144",
    "yard", "0.9144",
    "yd", "0.9144",
    "miles", "1304.544",
    "mile", "1304.544",
    "mi", "1304.544",
    "modified_american_feet", "0.3048122530",
    "modified_american_foot", "0.3048122530",
    "clarke_feet", "0.3047972651",
    "clarke_foot", "0.3047972651",
    "indian_feet", "0.3047995142",
    "indian_foot", "0.3047995142",
    "Yard_Indian", "0.9143985307444408",
    "Foot_Clarke", "0.30479726540",
    "Foot_Gold_Coast", "0.3047997101815088",
    "Link_Clarke", "0.2011661951640",
    "Yard_Sears", "0.9143984146160287",
    "50_Kilometers", "50000.0",
    "150_Kilometers", "150000.0",
    nullptr, nullptr
};

/************************************************************************/
/*                     ImagineCitationTranslation()                     */
/*                                                                      */
/*      Translate ERDAS Imagine GeoTif citation                         */
/************************************************************************/
char* ImagineCitationTranslation( char* psCitation, geokey_t keyID )
{
    if( !psCitation )
        return nullptr;
    char* ret = nullptr;
    if( STARTS_WITH_CI(psCitation, "IMAGINE GeoTIFF Support") )
    {
        static const char * const keyNames[] = {
            "NAD = ", "Datum = ", "Ellipsoid = ", "Units = ", nullptr };

        // This is a handle IMAGING style citation.
        CPLString osName;
        char* p1 = nullptr;

        char* p = strchr(psCitation, '$');
        if( p && strchr(p, '\n') )
            p = strchr(p, '\n') + 1;
        if( p )
        {
            p1 = p + strlen(p);
            char *p2 = strchr(p, '\n');
            if( p2 )
                p1 = std::min(p1, p2);
            p2 = strchr(p, '\0');
            if( p2 )
                p1 = std::min(p1, p2);

            for( int i = 0; keyNames[i] != nullptr; i++ )
            {
                p2 = strstr(p, keyNames[i]);
                if(p2)
                    p1 = std::min(p1, p2);
            }
        }

        // PCS name, GCS name and PRJ name.
        if( p && p1 )
        {
            switch( keyID )
            {
              case PCSCitationGeoKey:
                if( strstr(psCitation, "Projection = ") )
                    osName = "PRJ Name = ";
                else
                    osName = "PCS Name = ";
                break;
              case GTCitationGeoKey:
                osName = "PCS Name = ";
                break;
              case GeogCitationGeoKey:
                if( !strstr(p, "Unable to") )
                    osName = "GCS Name = ";
                break;
              default:
                break;
            }
            if( !osName.empty() )
            {
                // TODO(schwehr): What exactly is this code trying to do?
                // Added in r15993 and modified in r21844 by warmerdam.
                char* p2 = nullptr;
                if( (p2 = strstr(psCitation, "Projection Name = ")) != nullptr )
                    p = p2 + strlen("Projection Name = ");
                if( (p2 = strstr(psCitation, "Projection = ")) != nullptr )
                    p = p2 + strlen("Projection = ");
                if( p1[0] == '\0' || p1[0] == '\n' || p1[0] == ' ' )
                    p1--;
                p2 = p1 - 1;
                while( p2 != nullptr &&
                       (p2[0] == ' ' || p2[0] == '\0' || p2[0] == '\n') )
                {
                    p2--;
                }
                if( p2 != p1 - 1 )
                {
                    p1 = p2;
                }
                if( p1 >= p )
                {
                    osName.append(p, p1 - p + 1);
                    osName += '|';
                }
            }
        }

        // All other parameters.
        for( int i = 0; keyNames[i] != nullptr; i++ )
        {
            p = strstr(psCitation, keyNames[i]);
            if( p )
            {
                p += strlen(keyNames[i]);
                p1 = p + strlen(p);
                char *p2 = strchr(p, '\n');
                if( p2 )
                    p1 = std::min(p1, p2);
                p2 = strchr(p, '\0');
                if( p2 )
                    p1 = std::min(p1, p2);
                for( int j = 0; keyNames[j] != nullptr; j++ )
                {
                    p2 = strstr(p, keyNames[j]);
                    if( p2 )
                        p1 = std::min(p1, p2);
                }
            }
            if( p && p1 && p1>p )
            {
                if( EQUAL(keyNames[i], "Units = ") )
                    osName += "LUnits = ";
                else
                    osName += keyNames[i];
                if( p1[0] == '\0' || p1[0] == '\n' || p1[0] == ' ' )
                    p1--;
                char* p2 = p1 - 1;
                while( p2 != nullptr &&
                       (p2[0] == ' ' || p2[0] == '\0' || p2[0] == '\n') )
                {
                    p2--;
                }
                if( p2 != p1 - 1 )
                {
                    p1 = p2;
                }
                if( p1 >= p )
                {
                    osName.append(p, p1 - p + 1);
                    osName += '|';
                }
            }
        }
        if( !osName.empty() )
            ret = CPLStrdup(osName);
    }
    return ret;
}

/************************************************************************/
/*                        CitationStringParse()                         */
/*                                                                      */
/*      Parse a Citation string                                         */
/************************************************************************/

char** CitationStringParse(char* psCitation, geokey_t keyID)
{
    if( !psCitation )
        return nullptr;

    char **ret = static_cast<char **>(
        CPLCalloc(sizeof(char*), nCitationNameTypes) );
    char* pDelimit = nullptr;
    char* pStr = psCitation;
    char name[512] = { '\0' };
    bool nameSet = false;
    int nameLen = static_cast<int>(strlen(psCitation));
    bool nameFound = false;
    while( (pStr - psCitation + 1) < nameLen )
    {
        if( (pDelimit = strstr(pStr, "|")) != nullptr )
        {
            strncpy( name, pStr, pDelimit - pStr );
            name[pDelimit-pStr] = '\0';
            pStr = pDelimit + 1;
            nameSet = true;
        }
        else
        {
            strcpy (name, pStr);
            pStr += strlen(pStr);
            nameSet = true;
        }
        if( strstr(name, "PCS Name = ") && ret[CitPcsName] == nullptr )
        {
            ret[CitPcsName] = CPLStrdup(name + strlen("PCS Name = "));
            nameFound = true;
        }
        if( strstr(name, "PRJ Name = ") && ret[CitProjectionName] == nullptr )
        {
            ret[CitProjectionName] =
                CPLStrdup(name + strlen("PRJ Name = "));
            nameFound = true;
        }
        if( strstr(name, "LUnits = ") && ret[CitLUnitsName] == nullptr )
        {
            ret[CitLUnitsName] = CPLStrdup(name + strlen("LUnits = "));
            nameFound = true;
        }
        if( strstr(name, "GCS Name = ") && ret[CitGcsName] == nullptr )
        {
            ret[CitGcsName] = CPLStrdup(name + strlen("GCS Name = "));
            nameFound = true;
        }
        if( strstr(name, "Datum = ") && ret[CitDatumName] == nullptr )
        {
            ret[CitDatumName] = CPLStrdup(name + strlen("Datum = "));
            nameFound = true;
        }
        if( strstr(name, "Ellipsoid = ") && ret[CitEllipsoidName] == nullptr )
        {
            ret[CitEllipsoidName] = CPLStrdup(name + strlen("Ellipsoid = "));
            nameFound = true;
        }
        if( strstr(name, "Primem = ") && ret[CitPrimemName] == nullptr )
        {
            ret[CitPrimemName] = CPLStrdup(name + strlen("Primem = "));
            nameFound = true;
        }
        if( strstr(name, "AUnits = ") && ret[CitAUnitsName] == nullptr )
        {
            ret[CitAUnitsName] = CPLStrdup(name + strlen("AUnits = "));
            nameFound = true;
        }
    }
    if( !nameFound && keyID == GeogCitationGeoKey && nameSet )
    {
        ret[CitGcsName] = CPLStrdup(name);
        nameFound = true;
    }
    if( !nameFound )
    {
        CPLFree( ret );
        ret = nullptr;
    }
    return ret;
}

/************************************************************************/
/*                       SetLinearUnitCitation()                        */
/*                                                                      */
/*      Set linear unit Citation string                                 */
/************************************************************************/
void SetLinearUnitCitation( std::map<geokey_t, std::string>& oMapAsciiKeys,
                            const char* pszLinearUOMName )
{
    CPLString osCitation;
    auto oIter = oMapAsciiKeys.find(PCSCitationGeoKey);
    if( oIter != oMapAsciiKeys.end() )
    {
        osCitation = oIter->second;
    }
    if( !osCitation.empty() )
    {
        size_t n = osCitation.size();
        if( osCitation[n-1] != '|' )
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
    oMapAsciiKeys[PCSCitationGeoKey] = osCitation;
}

/************************************************************************/
/*                         SetGeogCSCitation()                          */
/*                                                                      */
/*      Set geogcs Citation string                                      */
/************************************************************************/
void SetGeogCSCitation( GTIF * psGTIF,
                        std::map<geokey_t, std::string>& oMapAsciiKeys,
                        const OGRSpatialReference *poSRS,
                        const char* angUnitName, int nDatum, short nSpheroid )
{
    bool bRewriteGeogCitation = false;
    CPLString osOriginalGeogCitation;
    auto oIter = oMapAsciiKeys.find(GeogCitationGeoKey);
    if( oIter != oMapAsciiKeys.end() )
    {
        osOriginalGeogCitation = oIter->second;
    }
    if( osOriginalGeogCitation.empty() )
        return;

    CPLString osCitation;
    if( !STARTS_WITH_CI(osOriginalGeogCitation, "GCS Name = ") )
    {
        osCitation = "GCS Name = ";
        osCitation += osOriginalGeogCitation;
    }
    else
    {
        osCitation = osOriginalGeogCitation;
    }

    if( nDatum == KvUserDefined )
    {
        const char* datumName = poSRS->GetAttrValue( "DATUM" );
        if( datumName && strlen(datumName) > 0 )
        {
            osCitation += "|Datum = ";
            osCitation += datumName;
            bRewriteGeogCitation = true;
        }
    }
    if( nSpheroid == KvUserDefined )
    {
        const char* spheroidName = poSRS->GetAttrValue( "SPHEROID" );
        if( spheroidName && strlen(spheroidName) > 0 )
        {
            osCitation += "|Ellipsoid = ";
            osCitation += spheroidName;
            bRewriteGeogCitation = true;
        }
    }

    const char* primemName = poSRS->GetAttrValue( "PRIMEM" );
    if( primemName && strlen(primemName) > 0 )
    {
        osCitation += "|Primem = ";
        osCitation += primemName;
        bRewriteGeogCitation = true;

        double primemValue = poSRS->GetPrimeMeridian(nullptr);
        if( angUnitName && !EQUAL(angUnitName, "Degree") )
        {
            const double aUnit = poSRS->GetAngularUnits(nullptr);
            primemValue *= aUnit;
        }
        GTIFKeySet( psGTIF, GeogPrimeMeridianLongGeoKey, TYPE_DOUBLE, 1,
                    primemValue );
    }
    if( angUnitName && strlen(angUnitName) > 0 &&
        !EQUAL(angUnitName, "Degree") )
    {
        osCitation += "|AUnits = ";
        osCitation += angUnitName;
        bRewriteGeogCitation = true;
    }

    if( osCitation.back() != '|' )
        osCitation += "|";

    if( bRewriteGeogCitation )
    {
        oMapAsciiKeys[GeogCitationGeoKey] = osCitation;
    }
}

/************************************************************************/
/*                          SetCitationToSRS()                          */
/*                                                                      */
/*      Parse and set Citation string to SRS                            */
/************************************************************************/
OGRBoolean SetCitationToSRS( GTIF* hGTIF, char* szCTString, int nCTStringLen,
                             geokey_t geoKey,  OGRSpatialReference *poSRS,
                             OGRBoolean* linearUnitIsSet)
{
    OGRBoolean ret = FALSE;
    const char* lUnitName = nullptr;

    poSRS->GetLinearUnits( &lUnitName );
    if( !lUnitName || strlen(lUnitName) == 0 ||
        EQUAL(lUnitName, "unknown") )
        *linearUnitIsSet = FALSE;
    else
        *linearUnitIsSet = TRUE;

    char* imgCTName = ImagineCitationTranslation(szCTString, geoKey);
    if( imgCTName )
    {
        strncpy(szCTString, imgCTName, nCTStringLen);
        szCTString[nCTStringLen-1] = '\0';
        CPLFree( imgCTName );
    }
    char** ctNames = CitationStringParse(szCTString, geoKey);
    if( ctNames )
    {
        if( poSRS->GetRoot() == nullptr)
            poSRS->SetNode( "PROJCS", "unnamed" );
        if( ctNames[CitPcsName] )
        {
            poSRS->SetNode( "PROJCS", ctNames[CitPcsName] );
            ret = TRUE;
        }
        if( ctNames[CitProjectionName] )
            poSRS->SetProjection( ctNames[CitProjectionName] );

        if( ctNames[CitLUnitsName] )
        {
            double unitSize = 0.0;
            int size = static_cast<int>(strlen(ctNames[CitLUnitsName]));
            if( strchr(ctNames[CitLUnitsName], '\0') )
                size -= 1;
            for( int i = 0; apszUnitMap[i] != nullptr; i += 2 )
            {
                if( EQUALN(apszUnitMap[i], ctNames[CitLUnitsName], size) )
                {
                    unitSize = CPLAtof(apszUnitMap[i+1]);
                    break;
                }
            }
            if( unitSize == 0.0 )
            {
                CPL_IGNORE_RET_VAL(
                    GDALGTIFKeyGetDOUBLE( hGTIF, ProjLinearUnitSizeGeoKey,
                                      &unitSize, 0, 1 ));
            }
            poSRS->SetLinearUnits( ctNames[CitLUnitsName], unitSize);
            *linearUnitIsSet = TRUE;
        }
        for( int i = 0; i < nCitationNameTypes; i++ )
            CPLFree( ctNames[i] );
        CPLFree( ctNames );
    }

    // If no "PCS Name = " (from Erdas) in GTCitationGeoKey.
    if( geoKey == GTCitationGeoKey )
    {
        if( strlen(szCTString) > 0 &&
            !strstr(szCTString, "PCS Name = ") )
        {
            const char* pszProjCS = poSRS->GetAttrValue( "PROJCS" );
            if((!(pszProjCS && strlen(pszProjCS) > 0)
                && !strstr(szCTString, "Projected Coordinates"))
               || (pszProjCS && strstr(pszProjCS, "unnamed")))
                poSRS->SetNode( "PROJCS", szCTString );
            ret = TRUE;
        }
    }

    return ret;
}

/************************************************************************/
/*                       GetGeogCSFromCitation()                        */
/*                                                                      */
/*      Parse and get geogcs names from a Citation string               */
/************************************************************************/
void GetGeogCSFromCitation( char* szGCSName, int nGCSName,
                            geokey_t geoKey,
                            char **ppszGeogName,
                            char **ppszDatumName,
                            char **ppszPMName,
                            char **ppszSpheroidName,
                            char **ppszAngularUnits)
{
    *ppszGeogName = nullptr;
    *ppszDatumName = nullptr;
    *ppszPMName = nullptr;
    *ppszSpheroidName = nullptr;
    *ppszAngularUnits = nullptr;

    char* imgCTName = ImagineCitationTranslation(szGCSName, geoKey);
    if( imgCTName )
    {
        strncpy(szGCSName, imgCTName, nGCSName);
        szGCSName[nGCSName-1] = '\0';
        CPLFree( imgCTName );
    }
    char** ctNames = CitationStringParse(szGCSName, geoKey);
    if( ctNames )
    {
        if( ctNames[CitGcsName] )
            *ppszGeogName = CPLStrdup( ctNames[CitGcsName] );

        if( ctNames[CitDatumName] )
            *ppszDatumName = CPLStrdup( ctNames[CitDatumName] );

        if( ctNames[CitEllipsoidName] )
            *ppszSpheroidName = CPLStrdup( ctNames[CitEllipsoidName] );

        if( ctNames[CitPrimemName] )
            *ppszPMName = CPLStrdup( ctNames[CitPrimemName] );

        if( ctNames[CitAUnitsName] )
            *ppszAngularUnits = CPLStrdup( ctNames[CitAUnitsName] );

        for( int i = 0; i < nCitationNameTypes; i++ )
            CPLFree( ctNames[i] );
        CPLFree( ctNames );
    }
    return;
}

/************************************************************************/
/*               CheckCitationKeyForStatePlaneUTM()                     */
/*                                                                      */
/*      Handle state plane and UTM in citation key                      */
/************************************************************************/
OGRBoolean CheckCitationKeyForStatePlaneUTM( GTIF* hGTIF, GTIFDefn* psDefn,
                                             OGRSpatialReference* poSRS,
                                             OGRBoolean* pLinearUnitIsSet )
{
    if( !hGTIF || !psDefn || !poSRS )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      For ESRI builds we are interested in maximizing PE              */
/*      compatibility, but generally we prefer to use EPSG              */
/*      definitions of the coordinate system if PCS is defined.         */
/* -------------------------------------------------------------------- */
#if !defined(ESRI_BUILD)
    if( psDefn->PCS != KvUserDefined )
        return FALSE;
#endif

    char szCTString[512] = { '\0' };

    // Check units.
    char units[32] = { '\0' };

    bool hasUnits = false;
    if( GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szCTString,
                             sizeof(szCTString) ) )
    {
        CPLString osLCCT = szCTString;

        osLCCT.tolower();

        if( strstr(osLCCT,"us") && strstr(osLCCT,"survey")
            && (strstr(osLCCT,"feet") || strstr(osLCCT,"foot")) )
            strcpy(units, "us_survey_feet");
        else if( strstr(osLCCT, "linear_feet")
                || strstr(osLCCT, "linear_foot")
                || strstr(osLCCT, "international") )
            strcpy(units, "international_feet");
        else if( strstr(osLCCT,"meter") )
            strcpy(units, "meters");

        if( strlen(units) > 0 )
            hasUnits = true;

        if( strstr( szCTString, "Projection Name = ") &&
            strstr( szCTString, "_StatePlane_") )
        {
            const char *pStr =
                strstr( szCTString, "Projection Name = ") +
                strlen("Projection Name = ");
            CPLString osCSName(pStr);
            const char* pReturn = strchr( pStr, '\n');
            if( pReturn )
                osCSName.resize(pReturn - pStr);
            if( poSRS->ImportFromESRIStatePlaneWKT(0, nullptr, nullptr, 32767, osCSName)
                == OGRERR_NONE )
            {
                // For some erdas citation keys, the state plane CS name is
                // incomplete, the unit check is necessary.
                bool done = false;
                if( hasUnits )
                {
                    OGR_SRSNode *poUnit = poSRS->GetAttrNode( "PROJCS|UNIT" );

                    if( poUnit != nullptr && poUnit->GetChildCount() >= 2 )
                    {
                        CPLString unitName = poUnit->GetChild(0)->GetValue();
                        unitName.tolower();

                        if( strstr(units, "us_survey_feet") )
                        {
                            if( strstr(unitName, "us_survey_feet") ||
                                strstr(unitName, "foot_us") )
                                done = true;
                        }
                        else if( strstr(units, "international_feet") )
                        {
                            if( strstr(unitName, "feet") ||
                                strstr(unitName, "foot") )
                                done = true;
                        }
                        else if (strstr(units, "meters"))
                        {
                            if( strstr(unitName, "meter") )
                                done = true;
                        }
                    }
                }
                if( done )
                    return true;
            }
        }
    }
    if( !hasUnits )
    {
        char *pszUnitsName = nullptr;
        GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, nullptr );
        if( pszUnitsName )
        {
            CPLString osLCCT = pszUnitsName;
            GTIFFreeMemory( pszUnitsName );
            osLCCT.tolower();

            if( strstr(osLCCT, "us") && strstr(osLCCT, "survey")
                && (strstr(osLCCT, "feet") || strstr(osLCCT, "foot")) )
                strcpy(units, "us_survey_feet");
            else if( strstr(osLCCT, "feet") || strstr(osLCCT, "foot") )
                strcpy(units, "international_feet");
            else if( strstr(osLCCT, "meter") )
                strcpy(units, "meters");
            // hasUnits = true;
        }
    }

    if( strlen(units) == 0 )
        strcpy(units, "meters");

    // Check PCSCitationGeoKey if it exists.
    szCTString[0] = '\0';
    if( GDALGTIFKeyGetASCII( hGTIF, PCSCitationGeoKey, szCTString,
                             sizeof(szCTString)) )
    {
        // For tif created by LEICA(ERDAS), ESRI state plane pe string was
        // used and the state plane zone is given in PCSCitation. Therefore
        // try ESRI pe string first.
        SetCitationToSRS( hGTIF, szCTString,
                          static_cast<int>(strlen(szCTString)),
                          PCSCitationGeoKey, poSRS, pLinearUnitIsSet );
        const char *pcsName = poSRS->GetAttrValue("PROJCS");
        const char *pStr = nullptr;
        if( (pcsName && (pStr = strstr(pcsName, "State Plane Zone ")) != nullptr)
            || (pStr = strstr(szCTString, "State Plane Zone ")) != nullptr )
        {
            pStr += strlen("State Plane Zone ");
            int statePlaneZone = abs(atoi(pStr));
            char nad[32];
            strcpy(nad, "HARN");
            if( strstr(szCTString, "NAD83") || strstr(szCTString, "NAD = 83") )
                strcpy(nad, "NAD83");
            else if( strstr(szCTString, "NAD27") ||
                     strstr(szCTString, "NAD = 27") )
                strcpy(nad, "NAD27");
            if( poSRS->ImportFromESRIStatePlaneWKT(
                    statePlaneZone, nad, units,
                    psDefn->PCS) == OGRERR_NONE )
                return TRUE;
        }
        else if( pcsName &&
                 (/* pStr = */ strstr(pcsName, "UTM Zone ")) != nullptr )
        {
            CheckUTM( psDefn, szCTString );
        }
    }

    // Check state plane again to see if a pe string is available.
    if( psDefn->PCS != KvUserDefined )
    {
        if( poSRS->ImportFromESRIStatePlaneWKT( 0, nullptr, units,
                                                psDefn->PCS) == OGRERR_NONE )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                               CheckUTM()                             */
/*                                                                      */
/*        Check utm proj code by its name.                              */
/************************************************************************/
void CheckUTM( GTIFDefn * psDefn, const char * pszCtString )
{
    if( !psDefn || !pszCtString )
        return;

    const char* p = strstr(pszCtString, "Datum = ");
    char datumName[128] = { '\0' };
    if( p )
    {
        p += strlen("Datum = ");
        const char* p1 = strchr(p, '|');
        if( p1 && p1 - p < static_cast<int>(sizeof(datumName)) )
        {
            strncpy(datumName, p, p1 - p);
            datumName[p1-p] = '\0';
        }
        else
        {
            CPLStrlcpy(datumName, p, sizeof(datumName));
        }
    }
    else
    {
        datumName[0] = '\0';
    }

    p = strstr(pszCtString, "UTM Zone ");
    if( p )
    {
        p += strlen("UTM Zone ");
        const char* p1 = strchr(p, '|');
        char utmName[64] = { '\0' };
        if( p1 && p1 - p < static_cast<int>(sizeof(utmName)) )
        {
            strncpy(utmName, p, p1 - p);
            utmName[p1-p] = '\0';
        }
        else
        {
            CPLStrlcpy(utmName, p, sizeof(utmName));
        }

        // Static to get this off the stack and constructed only one time.
        static const char * const apszUtmProjCode[] = {
            "PSAD56", "17N", "16017",
            "PSAD56", "18N", "16018",
            "PSAD56", "19N", "16019",
            "PSAD56", "20N", "16020",
            "PSAD56", "21N", "16021",
            "PSAD56", "17S", "16117",
            "PSAD56", "18S", "16118",
            "PSAD56", "19S", "16119",
            "PSAD56", "20S", "16120",
            "PSAD56", "21S", "16121",
            "PSAD56", "22S", "16122",
            nullptr, nullptr, nullptr };

        for( int i = 0; apszUtmProjCode[i]!=nullptr; i += 3 )
        {
            if( EQUALN(utmName, apszUtmProjCode[i+1],
                       strlen(apszUtmProjCode[i+1])) &&
                 EQUAL(datumName, apszUtmProjCode[i]) )
            {
                if( psDefn->ProjCode != atoi(apszUtmProjCode[i+2]) )
                {
                    psDefn->ProjCode =
                        static_cast<short>( atoi(apszUtmProjCode[i+2]) );
                    GTIFGetProjTRFInfo( psDefn->ProjCode, nullptr,
                                        &(psDefn->Projection),
                                        psDefn->ProjParm );
                    break;
                }
            }
        }
    }

    return;
}
