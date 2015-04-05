/******************************************************************************
 * $Id$
 *
 * Project:  SEG-P1 / UKOOA P1-90 Translator
 * Purpose:  Implements OGRUKOOAP190Layer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMSEGUKOOAS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_segukooa.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ExtractField()                            */
/************************************************************************/

static void ExtractField(char* szField, const char* pszLine, int nOffset, int nLen)
{
    memcpy(szField, pszLine + nOffset, nLen);
    szField[nLen] = '\0';
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSEGUKOOABaseLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(TRUE)
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                         OGRUKOOAP190Layer()                          */
/************************************************************************/

typedef struct
{
    const char*     pszName;
    OGRFieldType    eType;
} FieldDesc;

static const FieldDesc UKOOAP190Fields[] =
{
    { "LINENAME", OFTString },
    { "VESSEL_ID", OFTString },
    { "SOURCE_ID", OFTString },
    { "OTHER_ID", OFTString },
    { "POINTNUMBER", OFTInteger },
    { "LONGITUDE", OFTReal },
    { "LATITUDE", OFTReal },
    { "EASTING", OFTReal },
    { "NORTHING", OFTReal },
    { "DEPTH", OFTReal },
    { "DAYOFYEAR", OFTInteger },
    { "TIME", OFTTime },
    { "DATETIME", OFTDateTime }
};

#define FIELD_LINENAME      0
#define FIELD_VESSEL_ID     1
#define FIELD_SOURCE_ID     2
#define FIELD_OTHER_ID      3
#define FIELD_POINTNUMBER   4
#define FIELD_LONGITUDE     5
#define FIELD_LATITUDE      6
#define FIELD_EASTING       7
#define FIELD_NORTHING      8
#define FIELD_DEPTH         9
#define FIELD_DAYOFYEAR     10
#define FIELD_TIME          11
#define FIELD_DATETIME      12

OGRUKOOAP190Layer::OGRUKOOAP190Layer( const char* pszFilename,
                                      VSILFILE* fp )

{
    this->fp = fp;
    nNextFID = 0;
    bEOF = FALSE;
    poSRS = NULL;
    nYear = 0;

    poFeatureDefn = new OGRFeatureDefn( CPLGetBasename(pszFilename) );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    for(int i=0;i<(int)(sizeof(UKOOAP190Fields)/sizeof(UKOOAP190Fields[0]));i++)
    {
        OGRFieldDefn    oField( UKOOAP190Fields[i].pszName,
                                UKOOAP190Fields[i].eType );
        poFeatureDefn->AddFieldDefn( &oField );
    }

    bUseEastingNorthingAsGeometry =
        CSLTestBoolean(CPLGetConfigOption("UKOOAP190_USE_EASTING_NORTHING", "NO"));

    ParseHeaders();

    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
}

/************************************************************************/
/*                         ~OGRUKOOAP190Layer()                         */
/************************************************************************/

OGRUKOOAP190Layer::~OGRUKOOAP190Layer()

{
    poFeatureDefn->Release();

    VSIFCloseL( fp );

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                          ParseHeaders()                              */
/************************************************************************/

void OGRUKOOAP190Layer::ParseHeaders()
{
    while(TRUE)
    {
        const char* pszLine = CPLReadLine2L(fp,81,NULL);
        if (pszLine == NULL || EQUALN(pszLine, "EOF", 3))
        {
            break;
        }

        int nLineLen = strlen(pszLine);
        while(nLineLen > 0 && pszLine[nLineLen-1] == ' ')
        {
            ((char*)pszLine)[nLineLen-1] = '\0';
            nLineLen --;
        }

        if (pszLine[0] != 'H')
            break;

        if (nLineLen < 33)
            continue;

        if (!bUseEastingNorthingAsGeometry &&
            strncmp(pszLine, "H1500", 5) == 0 && poSRS == NULL)
        {
            if (strncmp(pszLine + 33 - 1, "WGS84", 5) == 0 ||
                strncmp(pszLine + 33 - 1, "WGS-84", 6) == 0)
            {
                poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
            }
            else if (strncmp(pszLine + 33 - 1, "WGS72", 5) == 0)
            {
                poSRS = new OGRSpatialReference();
                poSRS->SetFromUserInput("WGS72");
            }
        }
        else if (!bUseEastingNorthingAsGeometry &&
                 strncmp(pszLine, "H1501", 5) == 0 && poSRS != NULL &&
                 nLineLen >= 32 + 6 * 6 + 10)
        {
            char aszParams[6][6+1];
            char szZ[10+1];
            int i;
            for(i=0;i<6;i++)
            {
                ExtractField(aszParams[i], pszLine, 33 - 1 + i * 6, 6);
            }
            ExtractField(szZ, pszLine, 33 - 1 + 6 * 6, 10);
            poSRS->SetTOWGS84(CPLAtof(aszParams[0]),
                              CPLAtof(aszParams[1]),
                              CPLAtof(aszParams[2]),
                              CPLAtof(aszParams[3]),
                              CPLAtof(aszParams[4]),
                              CPLAtof(aszParams[5]),
                              CPLAtof(szZ));
        }
        else if (strncmp(pszLine, "H0200", 5) == 0)
        {
            char** papszTokens = CSLTokenizeString(pszLine + 33 - 1);
            for(int i = 0; papszTokens[i] != NULL; i++)
            {
                if (strlen(papszTokens[i]) == 4)
                {
                    int nVal = atoi(papszTokens[i]);
                    if (nVal >= 1900)
                    {
                        if (nYear != 0 && nYear != nVal)
                        {
                            CPLDebug("SEGUKOOA",
                                     "Several years found in H0200. Ignoring them!");
                            nYear = 0;
                            break;
                        }
                        nYear = nVal;
                    }
                }
            }
            CSLDestroy(papszTokens);
        }
    }
    VSIFSeekL( fp, 0, SEEK_SET );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRUKOOAP190Layer::ResetReading()

{
    nNextFID = 0;
    bEOF = FALSE;
    VSIFSeekL( fp, 0, SEEK_SET );
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRUKOOAP190Layer::GetNextRawFeature()
{
    if (bEOF)
        return NULL;

    const char* pszLine;

    while(TRUE)
    {
        pszLine = CPLReadLine2L(fp,81,NULL);
        if (pszLine == NULL || EQUALN(pszLine, "EOF", 3))
        {
            bEOF = TRUE;
            return NULL;
        }

        int nLineLen = strlen(pszLine);
        while(nLineLen > 0 && pszLine[nLineLen-1] == ' ')
        {
            ((char*)pszLine)[nLineLen-1] = '\0';
            nLineLen --;
        }

        if (pszLine[0] == 'H' || nLineLen < 46)
            continue;

        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFID(nNextFID ++);

        char szLineName[12 + 1];
        ExtractField(szLineName, pszLine, 2-1, 12);
        int i = 11;
        while (i >= 0)
        {
            if (szLineName[i] == ' ')
                szLineName[i] = '\0';
            else
                break;
            i --;
        }
        poFeature->SetField(FIELD_LINENAME, szLineName);

        char szVesselId[1+1];
        szVesselId[0] = pszLine[17-1];
        if (szVesselId[0] != ' ')
        {
            szVesselId[1] = '\0';
            poFeature->SetField(FIELD_VESSEL_ID, szVesselId);
        }

        char szSourceId[1+1];
        szSourceId[0] = pszLine[18-1];
        if (szSourceId[0] != ' ')
        {
            szSourceId[1] = '\0';
            poFeature->SetField(FIELD_SOURCE_ID, szSourceId);
        }

        char szOtherId[1+1];
        szOtherId[0] = pszLine[19-1];
        if (szOtherId[0] != ' ')
        {
            szOtherId[1] = '\0';
            poFeature->SetField(FIELD_OTHER_ID, szOtherId);
        }

        char szPointNumber[6+1];
        ExtractField(szPointNumber, pszLine, 20-1, 6);
        poFeature->SetField(4, atoi(szPointNumber));

        char szDeg[3+1];
        char szMin[2+1];
        char szSec[5+1];

        ExtractField(szDeg, pszLine, 26-1, 2);
        ExtractField(szMin, pszLine, 26+2-1, 2);
        ExtractField(szSec, pszLine, 26+2+2-1, 5);
        double dfLat = atoi(szDeg) + atoi(szMin) / 60.0 + CPLAtof(szSec) / 3600.0;
        if (pszLine[26+2+2+5-1] == 'S')
            dfLat = -dfLat;
        poFeature->SetField(FIELD_LATITUDE, dfLat);

        ExtractField(szDeg, pszLine, 36-1, 3);
        ExtractField(szMin, pszLine, 36+3-1, 2);
        ExtractField(szSec, pszLine, 36+3+2-1, 5);
        double dfLon = atoi(szDeg) + atoi(szMin) / 60.0 + CPLAtof(szSec) / 3600.0;
        if (pszLine[36+3+2+5-1] == 'W')
            dfLon = -dfLon;
        poFeature->SetField(FIELD_LONGITUDE, dfLon);

        OGRGeometry* poGeom = NULL;
        if (!bUseEastingNorthingAsGeometry)
            poGeom = new OGRPoint(dfLon, dfLat);

        if (nLineLen >= 64)
        {
            char szEasting[9+1];
            ExtractField(szEasting, pszLine, 47-1, 9);
            double dfEasting = CPLAtof(szEasting);
            poFeature->SetField(FIELD_EASTING, dfEasting);

            char szNorthing[9+1];
            ExtractField(szNorthing, pszLine, 56-1, 9);
            double dfNorthing = CPLAtof(szNorthing);
            poFeature->SetField(FIELD_NORTHING, dfNorthing);

            if (bUseEastingNorthingAsGeometry)
                poGeom = new OGRPoint(dfEasting, dfNorthing);
        }

        if (poGeom)
        {
            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly(poGeom);
        }

        if (nLineLen >= 70)
        {
            char szDepth[6+1];
            ExtractField(szDepth, pszLine, 65-1, 6);
            double dfDepth = CPLAtof(szDepth);
            poFeature->SetField(FIELD_DEPTH, dfDepth);
        }

        int nDayOfYear = 0;
        if (nLineLen >= 73)
        {
            char szDayOfYear[3+1];
            ExtractField(szDayOfYear, pszLine, 71-1, 3);
            nDayOfYear = atoi(szDayOfYear);
            poFeature->SetField(FIELD_DAYOFYEAR, nDayOfYear);
        }

        if (nLineLen >= 79)
        {
            char szH[2+1], szM[2+1], szS[2+1];
            ExtractField(szH, pszLine, 74-1, 2);
            ExtractField(szM, pszLine, 74-1+2, 2);
            ExtractField(szS, pszLine, 74-1+2+2, 2);
            poFeature->SetField(FIELD_TIME, 0, 0, 0, atoi(szH), atoi(szM), atoi(szS) );

            if (nYear != 0)
            {
                #define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
                static const int mon_lengths[2][12] = {
                    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
                    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
                } ;
                int bIsLeap = isleap(nYear);
                int nMonth = 0;
                int nDays = 0;
                if ((bIsLeap && nDayOfYear >= 1 && nDayOfYear <= 366) ||
                    (!bIsLeap && nDayOfYear >= 1 && nDayOfYear <= 365))
                {
                    while(nDayOfYear > nDays + mon_lengths[bIsLeap][nMonth])
                    {
                        nDays += mon_lengths[bIsLeap][nMonth];
                        nMonth ++;
                    }
                    int nDayOfMonth = nDayOfYear - nDays;
                    nMonth ++;

                    poFeature->SetField(FIELD_DATETIME, nYear, nMonth, nDayOfMonth,
                                        atoi(szH), atoi(szM), atoi(szS) );
                }

            }
        }

        return poFeature;
    }
}

/************************************************************************/
/*                           OGRSEGP1Layer()                            */
/************************************************************************/

static const FieldDesc SEGP1Fields[] =
{
    { "LINENAME", OFTString },
    { "POINTNUMBER", OFTInteger },
    { "RESHOOTCODE", OFTString },
    { "LONGITUDE", OFTReal },
    { "LATITUDE", OFTReal },
    { "EASTING", OFTReal },
    { "NORTHING", OFTReal },
    { "DEPTH", OFTReal },
#if 0
    { "YEAR", OFTInteger },
    { "DAYOFYEAR", OFTInteger },
    { "TIME", OFTTime },
    { "DATETIME", OFTDateTime }
#endif
};

#define SEGP1_FIELD_LINENAME      0
#define SEGP1_FIELD_POINTNUMBER   1
#define SEGP1_FIELD_RESHOOTCODE   2
#define SEGP1_FIELD_LONGITUDE     3
#define SEGP1_FIELD_LATITUDE      4
#define SEGP1_FIELD_EASTING       5
#define SEGP1_FIELD_NORTHING      6
#define SEGP1_FIELD_DEPTH         7
#define SEGP1_FIELD_DAYOFYEAR     8
#define SEGP1_FIELD_TIME          9
#define SEGP1_FIELD_DATETIME      10

OGRSEGP1Layer::OGRSEGP1Layer( const char* pszFilename,
                              VSILFILE* fp,
                              int nLatitudeCol )

{
    this->fp = fp;
    this->nLatitudeCol = nLatitudeCol;
    nNextFID = 0;
    bEOF = FALSE;
    poSRS = NULL;

    poFeatureDefn = new OGRFeatureDefn( CPLGetBasename(pszFilename) );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    for(int i=0;i<(int)(sizeof(SEGP1Fields)/sizeof(SEGP1Fields[0]));i++)
    {
        OGRFieldDefn    oField( SEGP1Fields[i].pszName,
                                SEGP1Fields[i].eType );
        poFeatureDefn->AddFieldDefn( &oField );
    }

    bUseEastingNorthingAsGeometry =
        CSLTestBoolean(CPLGetConfigOption("SEGP1_USE_EASTING_NORTHING", "NO"));

    ResetReading();
}

/************************************************************************/
/*                            ~OGRSEGP1Layer()                          */
/************************************************************************/

OGRSEGP1Layer::~OGRSEGP1Layer()

{
    poFeatureDefn->Release();

    VSIFCloseL( fp );

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSEGP1Layer::ResetReading()

{
    nNextFID = 0;
    bEOF = FALSE;
    VSIFSeekL( fp, 0, SEEK_SET );

    /* Skip first 20 header lines */
    const char* pszLine = NULL;
    for(int i=0; i<20;i++)
    {
        pszLine = CPLReadLine2L(fp,81,NULL);
        if (pszLine == NULL)
        {
            bEOF = TRUE;
            break;
        }
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSEGP1Layer::GetNextRawFeature()
{
    if (bEOF)
        return NULL;

    const char* pszLine = NULL;
    while(TRUE)
    {
        pszLine = CPLReadLine2L(fp,81,NULL);
        if (pszLine == NULL || EQUALN(pszLine, "EOF", 3))
        {
            bEOF = TRUE;
            return NULL;
        }

        int nLineLen = strlen(pszLine);
        while(nLineLen > 0 && pszLine[nLineLen-1] == ' ')
        {
            ((char*)pszLine)[nLineLen-1] = '\0';
            nLineLen --;
        }

        char* pszExpandedLine = ExpandTabs(pszLine);
        pszLine = pszExpandedLine;
        nLineLen = strlen(pszLine);

        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFID(nNextFID ++);

        OGRGeometry* poGeom = NULL;

        if (nLatitudeCol-1 + 19 <= nLineLen)
        {
            char szDeg[3+1];
            char szMin[2+1];
            char szSec[4+1];

            ExtractField(szDeg, pszLine, nLatitudeCol-1, 2);
            ExtractField(szMin, pszLine, nLatitudeCol+2-1, 2);
            ExtractField(szSec, pszLine, nLatitudeCol+2+2-1, 4);
            double dfLat = atoi(szDeg) + atoi(szMin) / 60.0 + atoi(szSec) / 100.0 / 3600.0;
            if (pszLine[nLatitudeCol+2+2+4-1] == 'S')
                dfLat = -dfLat;
            poFeature->SetField(SEGP1_FIELD_LATITUDE, dfLat);

            ExtractField(szDeg, pszLine, nLatitudeCol+9-1, 3);
            ExtractField(szMin, pszLine, nLatitudeCol+9+3-1, 2);
            ExtractField(szSec, pszLine, nLatitudeCol+9+3+2-1, 4);
            double dfLon = atoi(szDeg) + atoi(szMin) / 60.0 + atoi(szSec) / 100.0 / 3600.0;
            if (pszLine[nLatitudeCol+9+3+2+4-1] == 'W')
                dfLon = -dfLon;
            poFeature->SetField(SEGP1_FIELD_LONGITUDE, dfLon);

            if (!bUseEastingNorthingAsGeometry)
                poGeom = new OGRPoint(dfLon, dfLat);
        }

        /* Normal layout -> extract other fields */
        if (nLatitudeCol == 27)
        {
            char szLineName[16 + 1];
            ExtractField(szLineName, pszLine, 2-1, 16);
            int i = 15;
            while (i >= 0)
            {
                if (szLineName[i] == ' ')
                    szLineName[i] = '\0';
                else
                    break;
                i --;
            }
            poFeature->SetField(SEGP1_FIELD_LINENAME, szLineName);

            char szPointNumber[8+1];
            ExtractField(szPointNumber, pszLine, 18-1, 8);
            poFeature->SetField(SEGP1_FIELD_POINTNUMBER, atoi(szPointNumber));

            char szReshootCode[1+1];
            ExtractField(szReshootCode, pszLine, 26-1, 1);
            poFeature->SetField(SEGP1_FIELD_RESHOOTCODE, szReshootCode);

            if (nLineLen >= 61)
            {
                char szEasting[8+1];
                ExtractField(szEasting, pszLine, 46-1, 8);
                double dfEasting = CPLAtof(szEasting);
                poFeature->SetField(SEGP1_FIELD_EASTING, dfEasting);

                char szNorthing[8+1];
                ExtractField(szNorthing, pszLine, 54-1, 8);
                double dfNorthing = CPLAtof(szNorthing);
                poFeature->SetField(SEGP1_FIELD_NORTHING, dfNorthing);

                if (bUseEastingNorthingAsGeometry)
                    poGeom = new OGRPoint(dfEasting, dfNorthing);
            }

            if (nLineLen >= 66)
            {
                char szDepth[5+1];
                ExtractField(szDepth, pszLine, 62-1, 5);
                double dfDepth = CPLAtof(szDepth);
                poFeature->SetField(SEGP1_FIELD_DEPTH, dfDepth);
            }
        }

        if (poGeom)
        {
            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly(poGeom);
        }

        CPLFree(pszExpandedLine);

        return poFeature;
    }
}

/************************************************************************/
/*                           ExpandTabs()                               */
/************************************************************************/

char* OGRSEGP1Layer::ExpandTabs(const char* pszLine)
{
    char* pszExpandedLine = (char*)CPLMalloc(strlen(pszLine) * 8 + 1);
    int j = 0;
    for(int i=0; pszLine[i] != '\0'; i++)
    {
        /* A tab must be space-expanded to reach the next column number */
        /* which is a multiple of 8 */
        if (pszLine[i] == 9)
        {
            do
            {
                pszExpandedLine[j ++] = ' ';
            } while ((j % 8) != 0);
        }
        else
            pszExpandedLine[j ++] = pszLine[i];
    }
    pszExpandedLine[j] = '\0';

    return pszExpandedLine;
}

/************************************************************************/
/*                        DetectLatitudeColumn()                        */
/************************************************************************/

/* Some SEG-P1 files have unusual offsets for latitude/longitude, so */
/* we try our best to identify it even in case of non-standard layout */
/* Return non-0 if detection is successful (column number starts at 1) */

int OGRSEGP1Layer::DetectLatitudeColumn(const char* pszLine)
{
    int nLen = strlen(pszLine);
    if (nLen >= 45 && pszLine[0] == ' ' &&
        (pszLine[35-1] == 'N' || pszLine[35-1] == 'S') &&
        (pszLine[45-1] == 'E' || pszLine[45-1] == 'W'))
        return 27;

    for(int i=8;i<nLen-10;i++)
    {
        if ((pszLine[i] == 'N' || pszLine[i] == 'S') &&
            (pszLine[i+10] == 'E' || pszLine[i+10] == 'W'))
            return i - 8 + 1;
    }

    return 0;
}


/************************************************************************/
/*                        OGRSEGUKOOALineLayer()                        */
/************************************************************************/

OGRSEGUKOOALineLayer::OGRSEGUKOOALineLayer(const char* pszFilename,
                                           OGRLayer *poBaseLayer)
{
    nNextFID = 0;
    bEOF = FALSE;

    poFeatureDefn = new OGRFeatureDefn( CPLSPrintf("%s_lines",
                                                   CPLGetBasename(pszFilename)) );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbLineString );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poBaseLayer->GetSpatialRef());

    OGRFieldDefn    oField( "LINENAME", OFTString );
    poFeatureDefn->AddFieldDefn( &oField );

    this->poBaseLayer = poBaseLayer;
    poNextBaseFeature = NULL;
}

/************************************************************************/
/*                       ~OGRSEGUKOOALineLayer()                        */
/************************************************************************/

OGRSEGUKOOALineLayer::~OGRSEGUKOOALineLayer()
{
    delete poNextBaseFeature;
    delete poBaseLayer;

    poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSEGUKOOALineLayer::ResetReading()

{
    nNextFID = 0;
    bEOF = FALSE;
    delete poNextBaseFeature;
    poNextBaseFeature = NULL;
    poBaseLayer->ResetReading();
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSEGUKOOALineLayer::GetNextRawFeature()
{
    if (bEOF)
        return NULL;

    /* Merge points of base layer that have same value for attribute(0) */
    /* into a single linestring */

    OGRFeature* poFeature = NULL;
    OGRLineString* poLS = NULL;

    if (poNextBaseFeature == NULL)
        poNextBaseFeature = poBaseLayer->GetNextFeature();

    while(poNextBaseFeature != NULL)
    {
        if (poNextBaseFeature->IsFieldSet(0) &&
            poNextBaseFeature->GetFieldAsString(0)[0] != '\0')
        {
            if (poFeature != NULL &&
                strcmp(poFeature->GetFieldAsString(0),
                    poNextBaseFeature->GetFieldAsString(0)) != 0)
            {
                return poFeature;
            }

            OGRPoint* poPoint =
                (OGRPoint*) poNextBaseFeature->GetGeometryRef();
            if (poPoint != NULL)
            {
                if (poFeature == NULL)
                {
                    poFeature = new OGRFeature(poFeatureDefn);
                    poFeature->SetFID(nNextFID ++);
                    poFeature->SetField(0,
                        poNextBaseFeature->GetFieldAsString(0));
                    poLS = new OGRLineString();
                    if (poBaseLayer->GetSpatialRef())
                        poLS->assignSpatialReference(
                                    poBaseLayer->GetSpatialRef());
                    poFeature->SetGeometryDirectly(poLS);
                }

                poLS->addPoint(poPoint);
            }
        }

        delete poNextBaseFeature;
        poNextBaseFeature = poBaseLayer->GetNextFeature();
    }

    bEOF = TRUE;
    return poFeature;
}
