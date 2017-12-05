/******************************************************************************
 *
 * Project:  HTF Translator
 * Purpose:  Implements OGRHTFLayer class.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_htf.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

#define DIGIT_ZERO '0'

/************************************************************************/
/*                            OGRHTFLayer()                             */
/************************************************************************/

OGRHTFLayer::OGRHTFLayer( const char* pszFilename, int nZone, int bIsNorth ) :
    poFeatureDefn(NULL),
    poSRS(new OGRSpatialReference(SRS_WKT_WGS84)),
    fpHTF(VSIFOpenL(pszFilename, "rb")),
    bEOF(false),
    nNextFID(0),
    bHasExtent(false),
    dfMinX(0),
    dfMinY(0),
    dfMaxX(0),
    dfMaxY(0)
{
    poSRS->SetUTM( nZone, bIsNorth );
}

/************************************************************************/
/*                         OGRHTFPolygonLayer()                         */
/************************************************************************/

OGRHTFPolygonLayer::OGRHTFPolygonLayer( const char* pszFilename, int nZone,
                                        int bIsNorth ) :
    OGRHTFLayer(pszFilename, nZone, bIsNorth)
{
    poFeatureDefn = new OGRFeatureDefn( "polygon" );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPolygon  );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    OGRFieldDefn oField1( "DESCRIPTION", OFTString);
    poFeatureDefn->AddFieldDefn( &oField1 );
    OGRFieldDefn oField2( "IDENTIFIER", OFTInteger);
    poFeatureDefn->AddFieldDefn( &oField2 );
    OGRFieldDefn oField3( "SEAFLOOR_COVERAGE", OFTString);
    poFeatureDefn->AddFieldDefn( &oField3 );
    OGRFieldDefn oField4( "POSITION_ACCURACY", OFTReal);
    poFeatureDefn->AddFieldDefn( &oField4 );
    OGRFieldDefn oField5( "DEPTH_ACCURACY", OFTReal);
    poFeatureDefn->AddFieldDefn( &oField5 );

    ResetReading();
}

/************************************************************************/
/*                        OGRHTFSoundingLayer()                         */
/************************************************************************/

OGRHTFSoundingLayer::OGRHTFSoundingLayer( const char* pszFilename, int nZone,
                                          int bIsNorth,
                                          int nTotalSoundingsIn ) :
    OGRHTFLayer(pszFilename, nZone, bIsNorth),
    bHasFPK(false),
    nFieldsPresent(0),
    panFieldPresence(NULL),
    nEastingIndex(-1),
    nNorthingIndex(-1),
    nTotalSoundings(nTotalSoundingsIn)
{
    poFeatureDefn = new OGRFeatureDefn( "sounding" );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint  );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    const char* pszLine = NULL;
    bool bSoundingHeader = false;
    while( fpHTF != NULL &&
           (pszLine = CPLReadLine2L(fpHTF, 1024, NULL)) != NULL)
    {
        if (STARTS_WITH(pszLine, "SOUNDING HEADER"))
            bSoundingHeader = true;
        else if (bSoundingHeader && strlen(pszLine) > 10 &&
                 pszLine[0] == '[' && pszLine[3] == ']' &&
                 pszLine[4] == ' ' &&
                 strstr(pszLine + 5, " =") != NULL)
        {
            char* pszName = CPLStrdup(pszLine + 5);
            *strstr(pszName, " =") = 0;
            char* pszPtr = pszName;
            for(;*pszPtr;pszPtr++)
            {
                if (*pszPtr == ' ')
                    *pszPtr = '_';
            }
            OGRFieldType eType;
            if (strcmp(pszName, "REJECTED_SOUNDING") == 0 ||
                strcmp(pszName, "FIX_NUMBER") == 0 ||
                strcmp(pszName, "NBA_FLAG") == 0 ||
                strcmp(pszName, "SOUND_VELOCITY") == 0 ||
                strcmp(pszName, "PLOTTED_SOUNDING") == 0)
                eType = OFTInteger;
            else if (strcmp(pszName, "LATITUDE") == 0 ||
                     strcmp(pszName, "LONGITUDE") == 0 ||
                     strcmp(pszName, "EASTING") == 0 ||
                     strcmp(pszName, "NORTHING") == 0 ||
                     strcmp(pszName, "DEPTH") == 0 ||
                     strcmp(pszName, "TPE_POSITION") == 0 ||
                     strcmp(pszName, "TPE_DEPTH") == 0 ||
                     strcmp(pszName, "TIDE") == 0 ||
                     strcmp(pszName, "DEEP_WATER_CORRECTION") == 0 ||
                     strcmp(pszName, "VERTICAL_BIAS_CORRECTION") == 0)
                eType = OFTReal;
            else
                eType = OFTString;
            OGRFieldDefn    oField( pszName, eType);
            poFeatureDefn->AddFieldDefn( &oField);
            CPLFree(pszName);
        }
        else if (strcmp(pszLine, "END OF SOUNDING HEADER") == 0)
        {
            bSoundingHeader = false;
        }
        else if (strcmp(pszLine, "SOUNDING DATA") == 0)
        {
            pszLine = CPLReadLine2L(fpHTF, 1024, NULL);
            if (pszLine == NULL)
                break;
            if (pszLine[0] == '[' &&
                static_cast<int>(strlen(pszLine)) ==
                2 + poFeatureDefn->GetFieldCount())
            {
                bHasFPK = true;
                panFieldPresence = static_cast<bool *>(
                    CPLMalloc(sizeof(int) * poFeatureDefn->GetFieldCount()) );
                for( int i=0;i<poFeatureDefn->GetFieldCount();i++)
                {
                    panFieldPresence[i] = pszLine[1 + i] != DIGIT_ZERO;
                    nFieldsPresent += panFieldPresence[i] ? 1 : 0;
                }
            }
            break;
        }
    }

    if( !bHasFPK )
    {
        panFieldPresence = static_cast<bool *>(
            CPLMalloc(sizeof(int) * poFeatureDefn->GetFieldCount()) );
        for( int i=0; i < poFeatureDefn->GetFieldCount(); i++ )
            panFieldPresence[i] = true;
        nFieldsPresent = poFeatureDefn->GetFieldCount();
    }

    int nIndex = poFeatureDefn->GetFieldIndex("EASTING");
    if (nIndex < 0 || !panFieldPresence[nIndex])
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Cannot find EASTING field");
        VSIFCloseL( fpHTF );
        fpHTF = NULL;
        return;
    }
    nEastingIndex = nIndex;
    nIndex = poFeatureDefn->GetFieldIndex("NORTHING");
    if (nIndex < 0 || !panFieldPresence[nIndex])
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Cannot find NORTHING field");
        VSIFCloseL( fpHTF );
        fpHTF = NULL;
        return;
    }
    nNorthingIndex = nIndex;

    ResetReading();
}

/************************************************************************/
/*                            ~OGRHTFLayer()                            */
/************************************************************************/

OGRHTFLayer::~OGRHTFLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    poFeatureDefn->Release();

    if (fpHTF)
        VSIFCloseL( fpHTF );
}

/************************************************************************/
/*                       ~OGRHTFSoundingLayer()                         */
/************************************************************************/

OGRHTFSoundingLayer::~OGRHTFSoundingLayer()

{
    CPLFree(panFieldPresence);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRHTFLayer::ResetReading()

{
    nNextFID = 0;
    bEOF = false;
    if (fpHTF)
    {
        VSIFSeekL( fpHTF, 0, SEEK_SET );
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRHTFPolygonLayer::ResetReading()

{
    OGRHTFLayer::ResetReading();
    if (fpHTF)
    {
        const char* pszLine = NULL;
        while( (pszLine = CPLReadLine2L(fpHTF, 1024, NULL)) != NULL)
        {
            if (strcmp(pszLine, "POLYGON DATA") == 0)
            {
                break;
            }
        }
        if (pszLine == NULL)
            bEOF = true;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRHTFSoundingLayer::ResetReading()

{
    OGRHTFLayer::ResetReading();
    if (fpHTF)
    {
        const char* pszLine = NULL;
        while( (pszLine = CPLReadLine2L(fpHTF, 1024, NULL)) != NULL)
        {
            if (strcmp(pszLine, "SOUNDING DATA") == 0)
            {
                if (bHasFPK)
                    pszLine = CPLReadLine2L(fpHTF, 1024, NULL);
                break;
            }
        }
        if (pszLine == NULL)
            bEOF = true;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRHTFLayer::GetNextFeature()
{
    if (fpHTF == NULL || bEOF)
        return NULL;

    while(!bEOF)
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRHTFPolygonLayer::GetNextRawFeature()
{
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    OGRLinearRing oLR;
    bool bHasFirstCoord = false;
    double dfFirstEasting = 0;
    double dfFirstNorthing = 0;
    double dfIslandEasting = 0;
    double dfIslandNorthing = 0;
    bool bInIsland = false;
    OGRPolygon* poPoly = new OGRPolygon();

    const char* pszLine = NULL;
    while( (pszLine = CPLReadLine2L(fpHTF, 1024, NULL)) != NULL)
    {
        if (pszLine[0] == ';')
        {
            /* comment */ ;
        }
        else if (pszLine[0] == 0)
        {
            /* end of polygon is marked by a blank line */
            break;
        }
        else if (STARTS_WITH(pszLine, "POLYGON DESCRIPTION: "))
        {
            poFeature->SetField(0, pszLine + strlen("POLYGON DESCRIPTION: "));
        }
        else if (STARTS_WITH(pszLine, "POLYGON IDENTIFIER: "))
        {
            poFeature->SetField(1, pszLine + strlen("POLYGON IDENTIFIER: "));
        }
        else if (STARTS_WITH(pszLine, "SEAFLOOR COVERAGE: "))
        {
            const char* pszVal = pszLine + strlen("SEAFLOOR COVERAGE: ");
            if (*pszVal != '*')
                poFeature->SetField(2, pszVal);
        }
        else if (STARTS_WITH(pszLine, "POSITION ACCURACY: "))
        {
            const char* pszVal = pszLine + strlen("POSITION ACCURACY: ");
            if (*pszVal != '*')
                poFeature->SetField(3, pszVal);
        }
        else if (STARTS_WITH(pszLine, "DEPTH ACCURACY: "))
        {
            const char* pszVal = pszLine + strlen("DEPTH ACCURACY: ");
            if (*pszVal != '*')
                poFeature->SetField(4, pszVal);
        }
        else if (strcmp(pszLine, "END OF POLYGON DATA") == 0)
        {
            bEOF = true;
            break;
        }
        else
        {
            char** papszTokens = CSLTokenizeString(pszLine);
            if (CSLCount(papszTokens) == 4)
            {
                const double dfEasting = CPLAtof(papszTokens[2]);
                const double dfNorthing = CPLAtof(papszTokens[3]);
                if (!bHasFirstCoord)
                {
                    bHasFirstCoord = true;
                    dfFirstEasting = dfEasting;
                    dfFirstNorthing = dfNorthing;
                    oLR.addPoint(dfEasting, dfNorthing);
                }
                else if (dfFirstEasting == dfEasting &&
                         dfFirstNorthing == dfNorthing)
                {
                    if (!bInIsland)
                    {
                        oLR.addPoint(dfEasting, dfNorthing);
                        poPoly->addRing(&oLR);
                        oLR.empty();
                        bInIsland = true;
                    }
                }
                else if (bInIsland && oLR.getNumPoints() == 0)
                {
                    dfIslandEasting = dfEasting;
                    dfIslandNorthing = dfNorthing;
                    oLR.addPoint(dfEasting, dfNorthing);
                }
                else if (bInIsland && dfIslandEasting == dfEasting &&
                         dfIslandNorthing == dfNorthing)
                {
                    oLR.addPoint(dfEasting, dfNorthing);
                    poPoly->addRing(&oLR);
                    oLR.empty();
                }
                else
                {
                    oLR.addPoint(dfEasting, dfNorthing);
                }
            }
            CSLDestroy(papszTokens);
        }
    }

    if (pszLine == NULL)
        bEOF = true;

    if (oLR.getNumPoints() >= 3)
    {
        oLR.closeRings();
        poPoly->addRing(&oLR);
    }
    poPoly->assignSpatialReference(poSRS);
    poFeature->SetGeometryDirectly(poPoly);
    poFeature->SetFID(nNextFID++);

    return poFeature;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRHTFSoundingLayer::GetNextRawFeature()
{

    OGRLinearRing oLR;

    const char* pszLine = NULL;
    while( (pszLine = CPLReadLine2L(fpHTF, 1024, NULL)) != NULL)
    {
        if (pszLine[0] == ';')
        {
            /* comment */ ;
        }
        else if (pszLine[0] == 0)
        {
            bEOF = true;
            return NULL;
        }
        else if (strcmp(pszLine, "END OF SOUNDING DATA") == 0)
        {
            bEOF = true;
            return NULL;
        }
        else
            break;
    }
    if (pszLine == NULL)
    {
        bEOF = true;
        return NULL;
    }

    double dfEasting = 0;
    double dfNorthing = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    char* pszStr = const_cast<char *>(pszLine);
    for( int i=0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if (!panFieldPresence[i])
            continue;

        char* pszSpace = strchr(pszStr, ' ');
        if (pszSpace)
            *pszSpace = '\0';

        if (strcmp(pszStr, "*") != 0)
            poFeature->SetField(i, pszStr);
        if (i == nEastingIndex)
            dfEasting = poFeature->GetFieldAsDouble(i);
        else if (i == nNorthingIndex)
            dfNorthing = poFeature->GetFieldAsDouble(i);

        if (pszSpace == NULL)
            break;
        pszStr = pszSpace + 1;
    }
    OGRPoint* poPoint = new OGRPoint(dfEasting, dfNorthing);
    poPoint->assignSpatialReference(poSRS);
    poFeature->SetGeometryDirectly(poPoint);
    poFeature->SetFID(nNextFID++);
    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRHTFSoundingLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom != NULL || m_poAttrQuery != NULL)
        return OGRHTFLayer::GetFeatureCount(bForce);

    if (nTotalSoundings != 0)
        return nTotalSoundings;

    ResetReading();
    if (fpHTF == NULL)
        return 0;

    int nCount = 0;
    const char* pszLine = NULL;
    while( (pszLine = CPLReadLine2L(fpHTF, 1024, NULL)) != NULL)
    {
        if (pszLine[0] == ';')
        {
            /* comment */ ;
        }
        else if (pszLine[0] == 0)
            break;
        else if (strcmp(pszLine, "END OF SOUNDING DATA") == 0)
            break;
        else
            nCount ++;
    }

    ResetReading();
    return nCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRHTFLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, OLCFastGetExtent))
        return bHasExtent;

    return FALSE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRHTFSoundingLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL &&
            nTotalSoundings != 0;

    return OGRHTFLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                           GetExtent()                                */
/************************************************************************/

OGRErr OGRHTFLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (!bHasExtent)
        return OGRLayer::GetExtent(psExtent, bForce);

    psExtent->MinX = dfMinX;
    psExtent->MinY = dfMinY;
    psExtent->MaxX = dfMaxX;
    psExtent->MaxY = dfMaxY;
    return OGRERR_NONE;
}

/************************************************************************/
/*                             SetExtent()                              */
/************************************************************************/

void OGRHTFLayer::SetExtent( double dfMinXIn, double dfMinYIn, double dfMaxXIn,
                             double dfMaxYIn )
{
    bHasExtent = true;
    dfMinX = dfMinXIn;
    dfMinY = dfMinYIn;
    dfMaxX = dfMaxXIn;
    dfMaxY = dfMaxYIn;
}

/************************************************************************/
/*                        OGRHTFMetadataLayer()                         */
/************************************************************************/

OGRHTFMetadataLayer::OGRHTFMetadataLayer(const std::vector<CPLString>& aosMDIn) :
    poFeatureDefn(new OGRFeatureDefn( "metadata" )),
    aosMD(aosMDIn),
    nNextFID(0)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone  );

    std::vector<CPLString>::const_iterator oIter = aosMD.begin();
    std::vector<CPLString>::const_iterator oEndIter = aosMD.end();
    while(oIter != oEndIter)
    {
        const CPLString& osStr = *oIter;
        char* pszStr = CPLStrdup(osStr.c_str());
        char* pszSep = strstr(pszStr, ": ");
        if (pszSep)
        {
            *pszSep = 0;
            int i = 0;
            int j = 0;
            for(;pszStr[i];i++)
            {
                if (pszStr[i] == ' ' || pszStr[i] == '-' || pszStr[i] == '&')
                {
                    if (j > 0 && pszStr[j-1]  == '_')
                        continue;
                    pszStr[j++] = '_';
                }
                else if (pszStr[i] == '(' || pszStr[i] == ')')
                    ;
                else
                    pszStr[j++] = pszStr[i];
            }
            pszStr[j] = 0;
            OGRFieldDefn    oField( pszStr, OFTString);
            poFeatureDefn->AddFieldDefn( &oField );
        }
        CPLFree(pszStr);
        ++oIter;
    }

    poFeature = new OGRFeature(poFeatureDefn);
    oIter = aosMD.begin();
    oEndIter = aosMD.end();
    int nField = 0;
    while(oIter != oEndIter)
    {
        const CPLString& osStr = *oIter;
        const char* pszStr = osStr.c_str();
        const char* pszSep = strstr(pszStr, ": ");
        if (pszSep)
        {
            if (pszSep[2] != '*')
                poFeature->SetField( nField, pszSep + 2 );

            nField ++;
        }
        ++oIter;
    }
}

/************************************************************************/
/*                       ~OGRHTFMetadataLayer()                         */
/************************************************************************/

OGRHTFMetadataLayer::~OGRHTFMetadataLayer()
{
    delete poFeature;
    poFeatureDefn->Release();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRHTFMetadataLayer::GetNextFeature()
{
    if (nNextFID == 1)
        return NULL;

    if((m_poFilterGeom == NULL
        || FilterGeometry( poFeature->GetGeometryRef() ) )
    && (m_poAttrQuery == NULL
        || m_poAttrQuery->Evaluate( poFeature )) )
    {
        nNextFID = 1;
        return poFeature->Clone();
    }

    return NULL;
}
