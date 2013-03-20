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

#include "geovalues.h"
#include "gt_citation.h"

CPL_CVSID("$Id$");

static const char *apszUnitMap[] = {
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
    NULL, NULL
};

/************************************************************************/
/*                     ImagineCitationTranslation()                     */
/*                                                                      */
/*      Translate ERDAS Imagine GeoTif citation                         */
/************************************************************************/
char* ImagineCitationTranslation(char* psCitation, geokey_t keyID)
{
    static const char *keyNames[] = {
        "NAD = ", "Datum = ", "Ellipsoid = ", "Units = ", NULL
    };

    char* ret = NULL;
    int i;
    if(!psCitation)
        return ret;
    if(EQUALN(psCitation, "IMAGINE GeoTIFF Support", strlen("IMAGINE GeoTIFF Support")))
    {
        // this is a handle IMAGING style citation
        char name[256];
        name[0] = '\0';
        char* p = NULL;
        char* p1 = NULL;

        p = strchr(psCitation, '$');
        if( p && strchr(p, '\n') )
            p = strchr(p, '\n') + 1;
        if(p)
        {
            p1 = p + strlen(p);
            char *p2 = strchr(p, '\n');
            if(p2)
                p1 = MIN(p1, p2);
            p2 = strchr(p, '\0');
            if(p2)
                p1 = MIN(p1, p2);
            for(i=0; keyNames[i]!=NULL; i++)
            {
                p2 = strstr(p, keyNames[i]);
                if(p2)
                    p1 = MIN(p1, p2);
            }
        }

        // PCS name, GCS name and PRJ name
        if(p && p1)
        {
            switch (keyID)
            {
              case PCSCitationGeoKey:
                if(strstr(psCitation, "Projection = "))
                    strcpy(name, "PRJ Name = ");
                else
                    strcpy(name, "PCS Name = ");
                break;
              case GTCitationGeoKey:
                strcpy(name, "PCS Name = ");
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
                char* p2;
                if((p2 = strstr(psCitation, "Projection Name = ")) != 0)
                    p = p2 + strlen("Projection Name = ");
                if((p2 = strstr(psCitation, "Projection = ")) != 0)
                    p = p2 + strlen("Projection = ");
                if(p1[0] == '\0' || p1[0] == '\n' || p1[0] == ' ')
                    p1 --;
                p2 = p1 - 1;
                while( p2>0 && (p2[0] == ' ' || p2[0] == '\0' || p2[0] == '\n') )
                    p2--;
                if(p2 != p1 - 1)
                    p1 = p2;
                if(p1 >= p)
                {
                    strncat(name, p, p1-p+1);
                    strcat(name, "|");
                    name[strlen(name)] = '\0';
                }
            }
        }

        // All other parameters
        for(i=0; keyNames[i]!=NULL; i++)
        {
            p = strstr(psCitation, keyNames[i]);
            if(p)
            {
                p += strlen(keyNames[i]);
                p1 = p + strlen(p);
                char *p2 = strchr(p, '\n');
                if(p2)
                    p1 = MIN(p1, p2);
                p2 = strchr(p, '\0');
                if(p2)
                    p1 = MIN(p1, p2);
                for(int j=0; keyNames[j]!=NULL; j++)
                {
                    p2 = strstr(p, keyNames[j]);
                    if(p2)
                        p1 = MIN(p1, p2);
                }
            }
            if(p && p1 && p1>p)
            {
                if(EQUAL(keyNames[i], "Units = "))
                    strcat(name, "LUnits = ");
                else
                    strcat(name, keyNames[i]);
                if(p1[0] == '\0' || p1[0] == '\n' || p1[0] == ' ')
                    p1 --;
                char* p2 = p1 - 1;
                while( p2>0 && (p2[0] == ' ' || p2[0] == '\0' || p2[0] == '\n') )
                    p2--;
                if(p2 != p1 - 1)
                    p1 = p2;
                if(p1 >= p)
                {
                    strncat(name, p, p1-p+1);
                    strcat(name, "|");
                    name[strlen(name)] = '\0';
                }
            }
        }
        if(strlen(name) > 0)
            ret = CPLStrdup(name);
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
    char ** ret = NULL;
    if(!psCitation)
        return ret;

    ret = (char **) CPLCalloc(sizeof(char*), nCitationNameTypes); 
    char* pDelimit = NULL;
    char* pStr = psCitation;
    char name[512];
    int nameSet = FALSE;
    int nameLen = strlen(psCitation);
    OGRBoolean nameFound = FALSE;
    while((pStr-psCitation+1)< nameLen)
    {
        if( (pDelimit = strstr(pStr, "|")) != NULL )
        {
            strncpy( name, pStr, pDelimit-pStr );
            name[pDelimit-pStr] = '\0';
            pStr = pDelimit+1;
            nameSet = TRUE;
        }
        else
        {
            strcpy (name, pStr);
            pStr += strlen(pStr);
            nameSet = TRUE;
        }
        if( strstr(name, "PCS Name = ") )
        {
            ret[CitPcsName] = CPLStrdup(name+strlen("PCS Name = "));
            nameFound = TRUE;
        }
        if(strstr(name, "PRJ Name = "))
        {
            ret[CitProjectionName] = CPLStrdup(name+strlen("PRJ Name = "));
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
    if( !nameFound && keyID == GeogCitationGeoKey && nameSet )
    {
        ret[CitGcsName] = CPLStrdup(name);
        nameFound = TRUE;
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
    char* lUnitName = NULL;
    
    poSRS->GetLinearUnits( &lUnitName );
    if(!lUnitName || strlen(lUnitName) == 0  || EQUAL(lUnitName, "unknown"))
        *linearUnitIsSet = FALSE;
    else
        *linearUnitIsSet = TRUE;

    char* imgCTName = ImagineCitationTranslation(szCTString, geoKey);
    if(imgCTName)
    {
        strncpy(szCTString, imgCTName, nCTStringLen);
        szCTString[nCTStringLen-1] = '\0';
        CPLFree( imgCTName );
    }
    char** ctNames = CitationStringParse(szCTString, geoKey);
    if(ctNames)
    {
        if( poSRS->GetRoot() == NULL)
            poSRS->SetNode( "PROJCS", "unnamed" );
        if(ctNames[CitPcsName])
        {
            poSRS->SetNode( "PROJCS", ctNames[CitPcsName] );
            ret = TRUE;
        }
        if(ctNames[CitProjectionName])
            poSRS->SetProjection( ctNames[CitProjectionName] );

        if(ctNames[CitLUnitsName])
        {
            double unitSize = 0.0;
            int size = strlen(ctNames[CitLUnitsName]);
            if(strchr(ctNames[CitLUnitsName], '\0'))
                size -= 1;
            for( int i = 0; apszUnitMap[i] != NULL; i += 2 )
            {
                if( EQUALN(apszUnitMap[i], ctNames[CitLUnitsName], size) )
                {
                    unitSize = atof(apszUnitMap[i+1]);
                    break;
                }
            }
            if( unitSize == 0.0 )
                GTIFKeyGet(hGTIF, ProjLinearUnitSizeGeoKey, &unitSize, 0,
                           sizeof(unitSize) );
            poSRS->SetLinearUnits( ctNames[CitLUnitsName], unitSize);
            *linearUnitIsSet = TRUE;
        }
        for(int i= 0; i<nCitationNameTypes; i++) 
            CPLFree( ctNames[i] );
        CPLFree( ctNames );
    }

    /* if no "PCS Name = " (from Erdas) in GTCitationGeoKey */
    if(geoKey == GTCitationGeoKey)
    {
        if(strlen(szCTString) > 0 && !strstr(szCTString, "PCS Name = "))
        {
            const char* pszProjCS = poSRS->GetAttrValue( "PROJCS" );
            if((!(pszProjCS && strlen(pszProjCS) > 0) && !strstr(szCTString, "Projected Coordinates"))
               ||(pszProjCS && strstr(pszProjCS, "unnamed")))
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
    char** ctNames = CitationStringParse(szGCSName, geoKey);
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


/************************************************************************/
/*               CheckCitationKeyForStatePlaneUTM()                     */
/*                                                                      */
/*      Handle state plane and UTM in citation key                      */
/************************************************************************/
OGRBoolean CheckCitationKeyForStatePlaneUTM(GTIF* hGTIF, GTIFDefn* psDefn, OGRSpatialReference* poSRS, OGRBoolean* pLinearUnitIsSet)
{
    if( !hGTIF || !psDefn || !poSRS )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      For ESRI builds we are interested in maximizing PE              */
/*      compatability, but generally we prefer to use EPSG              */
/*      definitions of the coordinate system if PCS is defined.         */
/* -------------------------------------------------------------------- */
#if !defined(ESRI_BUILD)
    if( psDefn->PCS != KvUserDefined )
        return FALSE;
#endif

    char  szCTString[512];
    szCTString[0] = '\0';

    /* Check units */
    char units[32];
    units[0] = '\0';

    OGRBoolean hasUnits = FALSE;
    if( GTIFKeyGet( hGTIF, GTCitationGeoKey, szCTString, 0, sizeof(szCTString) ) )
    {
        CPLString osLCCT = szCTString;

        osLCCT.tolower();

        if( strstr(osLCCT,"us") && strstr(osLCCT,"survey")
            && (strstr(osLCCT,"feet") || strstr(osLCCT,"foot")) )
            strcpy(units, "us_survey_feet");
        else if(strstr(osLCCT, "linear_feet")  
                || strstr(osLCCT, "linear_foot") 
                || strstr(osLCCT, "international"))
            strcpy(units, "international_feet");
        else if( strstr(osLCCT,"meter") )
            strcpy(units, "meters");

        if (strlen(units) > 0)
            hasUnits = TRUE;

        if( strstr( szCTString, "Projection Name = ") && strstr( szCTString, "_StatePlane_"))
        {
            const char *pStr = strstr( szCTString, "Projection Name = ") + strlen("Projection Name = ");
            const char* pReturn = strchr( pStr, '\n');
            char CSName[128];
            strncpy(CSName, pStr, pReturn-pStr);
            CSName[pReturn-pStr] = '\0';
            if( poSRS->ImportFromESRIStatePlaneWKT(0, NULL, NULL, 32767, CSName) == OGRERR_NONE )
            {
                // for some erdas citation keys, the state plane CS name is incomplete, the unit check is necessary.
                OGRBoolean done = FALSE;
                if (hasUnits)
                {
                    OGR_SRSNode *poUnit = poSRS->GetAttrNode( "PROJCS|UNIT" );
      
                    if( poUnit != NULL && poUnit->GetChildCount() >= 2 )
                    {
                        CPLString unitName = poUnit->GetChild(0)->GetValue();
                        unitName.tolower();

                        if (strstr(units, "us_survey_feet"))
                        {              
                            if (strstr(unitName, "us_survey_feet") || strstr(unitName, "foot_us") )
                                done = TRUE;
                        }
                        else if (strstr(units, "international_feet"))
                        {
                            if (strstr(unitName, "feet") || strstr(unitName, "foot"))
                                done = TRUE;
                        }
                        else if (strstr(units, "meters"))
                        {
                            if (strstr(unitName, "meter") )
                                done = TRUE;
                        }
                    }
                }
                if (done)
                    return TRUE;
            }
        }
    }
    if( !hasUnits )
    {
        char	*pszUnitsName = NULL;
        GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, NULL );
        if( pszUnitsName && strlen(pszUnitsName) > 0 )
        {
            CPLString osLCCT = pszUnitsName;
            GTIFFreeMemory( pszUnitsName );
            osLCCT.tolower();

            if( strstr(osLCCT, "us") && strstr(osLCCT, "survey")
                && (strstr(osLCCT, "feet") || strstr(osLCCT, "foot")))
                strcpy(units, "us_survey_feet");
            else if(strstr(osLCCT, "feet") || strstr(osLCCT, "foot"))
                strcpy(units, "international_feet");
            else if(strstr(osLCCT, "meter"))
                strcpy(units, "meters");
            hasUnits = TRUE;
        }
    }

    if (strlen(units) == 0)
        strcpy(units, "meters");

    /* check PCSCitationGeoKey if it exists */
    szCTString[0] = '\0';
    if( hGTIF && GTIFKeyGet( hGTIF, PCSCitationGeoKey, szCTString, 0, sizeof(szCTString)) )  
    {
        /* For tif created by LEICA(ERDAS), ESRI state plane pe string was used and */
        /* the state plane zone is given in PCSCitation. Therefore try Esri pe string first. */
        SetCitationToSRS(hGTIF, szCTString, strlen(szCTString), PCSCitationGeoKey, poSRS, pLinearUnitIsSet);
        const char *pcsName = poSRS->GetAttrValue("PROJCS");
        const char *pStr = NULL;
        if( (pcsName && (pStr = strstr(pcsName, "State Plane Zone ")) != NULL)
            || (pStr = strstr(szCTString, "State Plane Zone ")) != NULL )
        {
            pStr += strlen("State Plane Zone ");
            int statePlaneZone = abs(atoi(pStr));
            char nad[32];
            strcpy(nad, "HARN");
            if( strstr(szCTString, "NAD83") || strstr(szCTString, "NAD = 83") )
                strcpy(nad, "NAD83");
            else if( strstr(szCTString, "NAD27") || strstr(szCTString, "NAD = 27") )
                strcpy(nad, "NAD27");
            if( poSRS->ImportFromESRIStatePlaneWKT(statePlaneZone, (const char*)nad, (const char*)units, psDefn->PCS) == OGRERR_NONE )
                return TRUE;
        }
        else if( pcsName && (pStr = strstr(pcsName, "UTM Zone ")) != NULL )
            CheckUTM( psDefn, szCTString );
    }

    /* check state plane again to see if a pe string is available */
    if( psDefn->PCS != KvUserDefined )
    {
        if( poSRS->ImportFromESRIStatePlaneWKT(0, NULL, (const char*)units, psDefn->PCS) == OGRERR_NONE )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                               CheckUTM()                             */
/*                                                                      */
/*        Check utm proj code by its name.                              */
/************************************************************************/
void CheckUTM( GTIFDefn * psDefn, char * pszCtString )
{
    if(!psDefn || !pszCtString)
        return;

    static const char *apszUtmProjCode[] = {
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
        NULL, NULL, NULL};

    char* p = strstr(pszCtString, "Datum = ");
    char datumName[128];
    if(p)
    {
        p += strlen("Datum = ");
        char* p1 = strchr(p, '|');
        if(p1)
        {
            strncpy(datumName, p, (p1-p));
            datumName[p1-p] = '\0';
        }
        else
            strcpy(datumName, p);
    }

    char utmName[64];
    p = strstr(pszCtString, "UTM Zone ");
    if(p)
    {
        p += strlen("UTM Zone ");
        char* p1 = strchr(p, '|');
        if(p1)
        {
            strncpy(utmName, p, (p1-p));
            utmName[p1-p] = '\0';
        }
        else
            strcpy(utmName, p);
    }

    for(int i=0; apszUtmProjCode[i]!=NULL; i += 3)
    {
        if(EQUALN(utmName, apszUtmProjCode[i+1], strlen(apszUtmProjCode[i+1])) &&
           EQUAL(datumName, apszUtmProjCode[i]) )
        {
            if(psDefn->ProjCode != atoi(apszUtmProjCode[i+2]))
            {
                psDefn->ProjCode = (short) atoi(apszUtmProjCode[i+2]);
                GTIFGetProjTRFInfo( psDefn->ProjCode, NULL, &(psDefn->Projection),
                                    psDefn->ProjParm );
                break;
            }
        }
    }
    return;
}
