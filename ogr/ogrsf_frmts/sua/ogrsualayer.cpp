/******************************************************************************
 * $Id$
 *
 * Project:  SUA Translator
 * Purpose:  Implements OGRSUALayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_sua.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_xplane_geo_utils.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRSUALayer()                             */
/************************************************************************/

OGRSUALayer::OGRSUALayer( VSILFILE* fp )

{
    fpSUA = fp;
    nNextFID = 0;
    bEOF = FALSE;
    bHasLastLine = FALSE;

    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);

    poFeatureDefn = new OGRFeatureDefn( "layer" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPolygon );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    OGRFieldDefn    oField1( "TYPE", OFTString);
    poFeatureDefn->AddFieldDefn( &oField1 );
    OGRFieldDefn    oField2( "CLASS", OFTString);
    poFeatureDefn->AddFieldDefn( &oField2 );
    OGRFieldDefn    oField3( "TITLE", OFTString);
    poFeatureDefn->AddFieldDefn( &oField3 );
    OGRFieldDefn    oField4( "TOPS", OFTString);
    poFeatureDefn->AddFieldDefn( &oField4 );
    OGRFieldDefn    oField5( "BASE", OFTString);
    poFeatureDefn->AddFieldDefn( &oField5 );
}

/************************************************************************/
/*                            ~OGRSUALayer()                            */
/************************************************************************/

OGRSUALayer::~OGRSUALayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    poFeatureDefn->Release();

    VSIFCloseL( fpSUA );
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSUALayer::ResetReading()

{
    nNextFID = 0;
    bEOF = FALSE;
    bHasLastLine = FALSE;
    VSIFSeekL( fpSUA, 0, SEEK_SET );
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSUALayer::GetNextFeature()
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
/*                              GetLatLon()                             */
/************************************************************************/

static int GetLatLon(const char* pszStr, double& dfLat, double& dfLon)
{
    if (pszStr[7] != ' ')
        return FALSE;
    if (pszStr[0] != 'N' && pszStr[0] != 'S')
        return FALSE;
    if (pszStr[8] != 'E' && pszStr[8] != 'W')
        return FALSE;

    char szDeg[4], szMin[3], szSec[3];
    szDeg[0] = pszStr[1];
    szDeg[1] = pszStr[2];
    szDeg[2] = 0;
    szMin[0] = pszStr[3];
    szMin[1] = pszStr[4];
    szMin[2] = 0;
    szSec[0] = pszStr[5];
    szSec[1] = pszStr[6];
    szSec[2] = 0;

    dfLat = atoi(szDeg) + atoi(szMin) / 60. + atoi(szSec) / 3600.;
    if (pszStr[0] == 'S')
        dfLat = -dfLat;

    szDeg[0] = pszStr[9];
    szDeg[1] = pszStr[10];
    szDeg[2] = pszStr[11];
    szDeg[3] = 0;
    szMin[0] = pszStr[12];
    szMin[1] = pszStr[13];
    szMin[2] = 0;
    szSec[0] = pszStr[14];
    szSec[1] = pszStr[15];
    szSec[2] = 0;

    dfLon = atoi(szDeg) + atoi(szMin) / 60. + atoi(szSec) / 3600.;
    if (pszStr[8] == 'W')
        dfLon = -dfLon;

    return TRUE;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSUALayer::GetNextRawFeature()
{
    const char* pszLine;
    CPLString osTYPE, osCLASS, osTITLE, osTOPS, osBASE;
    OGRLinearRing oLR;
    double dfLastLat = 0, dfLastLon = 0;
    int bFirst = TRUE;

    if (bEOF)
        return NULL;

    while(TRUE)
    {
        if (bFirst && bHasLastLine)
        {
            pszLine = osLastLine.c_str();
            bFirst = FALSE;
        }
        else
        {
            pszLine = CPLReadLine2L(fpSUA, 1024, NULL);
            if (pszLine == NULL)
            {
                bEOF = TRUE;
                if (oLR.getNumPoints() == 0)
                    return NULL;
                break;
            }
            osLastLine = pszLine;
            bHasLastLine = TRUE;
        }

        if (pszLine[0] == '#' || pszLine[0] == '\0')
            continue;

        if (EQUALN(pszLine, "TYPE=", 5))
        {
            if (osTYPE.size() != 0)
                break;
            osTYPE = pszLine + 5;
        }
        else if (EQUALN(pszLine, "CLASS=", 6))
        {
            if (osCLASS.size() != 0)
                break;
            osCLASS = pszLine + 6;
        }
        else if (EQUALN(pszLine, "TITLE=", 6))
        {
            if (osTITLE.size() != 0)
                break;
            osTITLE = pszLine + 6;
        }
        else if (EQUALN(pszLine, "TOPS=", 5))
            osTOPS = pszLine + 5;
        else if (EQUALN(pszLine, "BASE=", 5))
            osBASE = pszLine + 5;
        else if (EQUALN(pszLine, "POINT=", 6))
        {
            pszLine += 6;
            if (strlen(pszLine) != 16)
                continue;

            double dfLat, dfLon;
            if (!GetLatLon(pszLine, dfLat, dfLon))
                continue;

            oLR.addPoint(dfLon, dfLat);
            dfLastLat = dfLat;
            dfLastLon = dfLon;
        }
        else if (EQUALN(pszLine, "CLOCKWISE", 9) || EQUALN(pszLine, "ANTI-CLOCKWISE", 14))
        {
            if (oLR.getNumPoints() == 0)
                continue;

            int bClockWise = EQUALN(pszLine, "CLOCKWISE", 9);

            /*const char* pszRADIUS = strstr(pszLine, "RADIUS=");
            if (pszRADIUS == NULL)
                continue;
            double dfRADIUS = atof(pszRADIUS + 7) * 1852;*/

            const char* pszCENTRE = strstr(pszLine, "CENTRE=");
            if (pszCENTRE == NULL)
                continue;
            pszCENTRE += 7;
            if (strlen(pszCENTRE) < 17 || pszCENTRE[16] != ' ')
                continue;
            double dfCenterLat, dfCenterLon;
            if (!GetLatLon(pszCENTRE, dfCenterLat, dfCenterLon))
                continue;

            const char* pszTO = strstr(pszLine, "TO=");
            if (pszTO == NULL)
                continue;
            pszTO += 3;
            if (strlen(pszTO) != 16)
                continue;
            double dfToLat, dfToLon;
            if (!GetLatLon(pszTO, dfToLat, dfToLon))
                continue;

            double dfStartDistance = OGRXPlane_Distance(dfCenterLat, dfCenterLon, dfLastLat, dfLastLon);
            double dfEndDistance = OGRXPlane_Distance(dfCenterLat, dfCenterLon, dfToLat, dfToLon);
            double dfStartAngle = OGRXPlane_Track(dfCenterLat, dfCenterLon, dfLastLat, dfLastLon);
            double dfEndAngle = OGRXPlane_Track(dfCenterLat, dfCenterLon, dfToLat, dfToLon);
            if (bClockWise && dfEndAngle < dfStartAngle)
                dfEndAngle += 360;
            else if (!bClockWise && dfStartAngle < dfEndAngle)
                dfEndAngle -= 360;

            int nSign = (bClockWise) ? 1 : -1;
            double dfAngle;
            for(dfAngle = dfStartAngle; (dfAngle - dfEndAngle) * nSign < 0; dfAngle += nSign)
            {
                double dfLat, dfLon;
                double pct = (dfAngle - dfStartAngle) / (dfEndAngle - dfStartAngle);
                double dfDist = dfStartDistance * (1-pct) + dfEndDistance * pct;
                OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon, dfDist, dfAngle, &dfLat, &dfLon);
                oLR.addPoint(dfLon, dfLat);
            }
            oLR.addPoint(dfToLon, dfToLat);

            dfLastLat = oLR.getY(oLR.getNumPoints() - 1);
            dfLastLon = oLR.getX(oLR.getNumPoints() - 1);
        }
        else if (EQUALN(pszLine, "CIRCLE", 6))
        {
            const char* pszRADIUS = strstr(pszLine, "RADIUS=");
            if (pszRADIUS == NULL)
                continue;
            double dfRADIUS = atof(pszRADIUS + 7) * 1852;

            const char* pszCENTRE = strstr(pszLine, "CENTRE=");
            if (pszCENTRE == NULL)
                continue;
            pszCENTRE += 7;
            if (strlen(pszCENTRE) != 16)
                continue;
            double dfCenterLat, dfCenterLon;
            if (!GetLatLon(pszCENTRE, dfCenterLat, dfCenterLon))
                continue;

            double dfAngle;
            double dfLat, dfLon;
            for(dfAngle = 0; dfAngle < 360; dfAngle += 1)
            {
                OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon, dfRADIUS, dfAngle, &dfLat, &dfLon);
                oLR.addPoint(dfLon, dfLat);
            }
            OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon, dfRADIUS, 0, &dfLat, &dfLon);
            oLR.addPoint(dfLon, dfLat);

            dfLastLat = oLR.getY(oLR.getNumPoints() - 1);
            dfLastLon = oLR.getX(oLR.getNumPoints() - 1);
        }
        else if (EQUALN(pszLine, "INCLUDE", 7) || EQUALN(pszLine, "END", 3))
        {
        }
        else
        {
            CPLDebug("SUA", "Unexpected content : %s", pszLine);
        }
    }

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField(0, osTYPE.c_str());
    poFeature->SetField(1, osCLASS.c_str());
    poFeature->SetField(2, osTITLE.c_str());
    poFeature->SetField(3, osTOPS.c_str());
    poFeature->SetField(4, osBASE.c_str());

    OGRPolygon* poPoly = new OGRPolygon();
    poPoly->assignSpatialReference(poSRS);
    oLR.closeRings();
    poPoly->addRing(&oLR);
    poFeature->SetGeometryDirectly(poPoly);
    poFeature->SetFID(nNextFID++);

    return poFeature;
}
/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSUALayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

