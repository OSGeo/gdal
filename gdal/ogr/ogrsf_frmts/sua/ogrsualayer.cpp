/******************************************************************************
 *
 * Project:  SUA Translator
 * Purpose:  Implements OGRSUALayer class.
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

#include "ogr_sua.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_geo_utils.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRSUALayer()                             */
/************************************************************************/

OGRSUALayer::OGRSUALayer( VSILFILE* fp ) :
    poFeatureDefn(new OGRFeatureDefn( "layer" )),
    poSRS(new OGRSpatialReference(SRS_WKT_WGS84)),
    fpSUA(fp),
    bEOF(false),
    bHasLastLine(false),
    nNextFID(0)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPolygon );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    OGRFieldDefn oField1( "TYPE", OFTString);
    poFeatureDefn->AddFieldDefn( &oField1 );
    OGRFieldDefn oField2( "CLASS", OFTString);
    poFeatureDefn->AddFieldDefn( &oField2 );
    OGRFieldDefn oField3( "TITLE", OFTString);
    poFeatureDefn->AddFieldDefn( &oField3 );
    OGRFieldDefn oField4( "TOPS", OFTString);
    poFeatureDefn->AddFieldDefn( &oField4 );
    OGRFieldDefn oField5( "BASE", OFTString);
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
    bEOF = false;
    bHasLastLine = false;
    VSIFSeekL( fpSUA, 0, SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSUALayer::GetNextFeature()
{
    while( true )
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
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

static bool GetLatLon( const char* pszStr, double& dfLat, double& dfLon )
{
    if (pszStr[7] != ' ')
        return false;
    if (pszStr[0] != 'N' && pszStr[0] != 'S')
        return false;
    if (pszStr[8] != 'E' && pszStr[8] != 'W')
        return false;

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

    return true;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSUALayer::GetNextRawFeature()
{
    if( bEOF )
        return NULL;

    CPLString osTYPE;
    CPLString osCLASS;
    CPLString osTITLE;
    CPLString osTOPS;
    CPLString osBASE;
    OGRLinearRing oLR;
    double dfLastLat = 0.0;
    double dfLastLon = 0.0;
    bool bFirst = true;

    while( true )
    {
        const char* pszLine = NULL;
        if( bFirst && bHasLastLine )
        {
            pszLine = osLastLine.c_str();
            bFirst = false;
        }
        else
        {
            pszLine = CPLReadLine2L(fpSUA, 1024, NULL);
            if (pszLine == NULL)
            {
                bEOF = true;
                if (oLR.getNumPoints() == 0)
                    return NULL;
                break;
            }
            osLastLine = pszLine;
            bHasLastLine = true;
        }

        if (pszLine[0] == '#' || pszLine[0] == '\0')
            continue;

        if (STARTS_WITH_CI(pszLine, "TYPE="))
        {
            if (!osTYPE.empty())
                break;
            osTYPE = pszLine + 5;
        }
        else if (STARTS_WITH_CI(pszLine, "CLASS="))
        {
            if (!osCLASS.empty())
                break;
            osCLASS = pszLine + 6;
        }
        else if (STARTS_WITH_CI(pszLine, "TITLE="))
        {
            if (!osTITLE.empty())
                break;
            osTITLE = pszLine + 6;
        }
        else if (STARTS_WITH_CI(pszLine, "TOPS="))
            osTOPS = pszLine + 5;
        else if (STARTS_WITH_CI(pszLine, "BASE="))
            osBASE = pszLine + 5;
        else if (STARTS_WITH_CI(pszLine, "POINT="))
        {
            pszLine += 6;
            if (strlen(pszLine) != 16)
                continue;

            double dfLat = 0.0;
            double dfLon = 0.0;
            if (!GetLatLon(pszLine, dfLat, dfLon))
                continue;

            oLR.addPoint(dfLon, dfLat);
            dfLastLat = dfLat;
            dfLastLon = dfLon;
        }
        else if (STARTS_WITH_CI(pszLine, "CLOCKWISE") || STARTS_WITH_CI(pszLine, "ANTI-CLOCKWISE"))
        {
            if (oLR.getNumPoints() == 0)
                continue;

            int bClockWise = STARTS_WITH_CI(pszLine, "CLOCKWISE");

            /*const char* pszRADIUS = strstr(pszLine, "RADIUS=");
            if (pszRADIUS == NULL)
                continue;
            double dfRADIUS = CPLAtof(pszRADIUS + 7) * 1852;*/

            const char* pszCENTRE = strstr(pszLine, "CENTRE=");
            if (pszCENTRE == NULL)
                continue;
            pszCENTRE += 7;
            if (strlen(pszCENTRE) < 17 || pszCENTRE[16] != ' ')
                continue;
            double dfCenterLat = 0.0;
            double dfCenterLon = 0.0;
            if (!GetLatLon(pszCENTRE, dfCenterLat, dfCenterLon))
                continue;

            const char* pszTO = strstr(pszLine, "TO=");
            if (pszTO == NULL)
                continue;
            pszTO += 3;
            if (strlen(pszTO) != 16)
                continue;
            double dfToLat = 0.0;
            double dfToLon = 0.0;
            if (!GetLatLon(pszTO, dfToLat, dfToLon))
                continue;

            const double dfStartDistance =
                OGR_GreatCircle_Distance(dfCenterLat, dfCenterLon, dfLastLat, dfLastLon);
            const double dfEndDistance =
                OGR_GreatCircle_Distance(dfCenterLat, dfCenterLon, dfToLat, dfToLon);
            const double dfStartAngle =
                OGR_GreatCircle_InitialHeading(dfCenterLat, dfCenterLon, dfLastLat, dfLastLon);
            double dfEndAngle =
                OGR_GreatCircle_InitialHeading(dfCenterLat, dfCenterLon, dfToLat, dfToLon);

            if( bClockWise && dfEndAngle < dfStartAngle )
                dfEndAngle += 360;
            else if (!bClockWise && dfStartAngle < dfEndAngle)
                dfEndAngle -= 360;

            int nSign = (bClockWise) ? 1 : -1;
            for( double dfAngle = dfStartAngle;
                 (dfAngle - dfEndAngle) * nSign < 0;
                 dfAngle += nSign )
            {
                const double pct = (dfAngle - dfStartAngle) / (dfEndAngle - dfStartAngle);
                const double dfDist = dfStartDistance * (1-pct) + dfEndDistance * pct;
                double dfLat = 0.0;
                double dfLon = 0.0;
                OGR_GreatCircle_ExtendPosition(dfCenterLat, dfCenterLon, dfDist, dfAngle, &dfLat, &dfLon);
                oLR.addPoint(dfLon, dfLat);
            }
            oLR.addPoint(dfToLon, dfToLat);

            dfLastLat = oLR.getY(oLR.getNumPoints() - 1);
            dfLastLon = oLR.getX(oLR.getNumPoints() - 1);
        }
        else if (STARTS_WITH_CI(pszLine, "CIRCLE"))
        {
            const char* pszRADIUS = strstr(pszLine, "RADIUS=");
            if (pszRADIUS == NULL)
                continue;
            double dfRADIUS = CPLAtof(pszRADIUS + 7) * 1852;

            const char* pszCENTRE = strstr(pszLine, "CENTRE=");
            if (pszCENTRE == NULL)
                continue;
            pszCENTRE += 7;
            if (strlen(pszCENTRE) != 16)
                continue;
            double dfCenterLat = 0.0;
            double dfCenterLon = 0.0;
            if (!GetLatLon(pszCENTRE, dfCenterLat, dfCenterLon))
                continue;

            double dfLat = 0.0;
            double dfLon = 0.0;
            for( double dfAngle = 0; dfAngle < 360; dfAngle += 1 )
            {
                OGR_GreatCircle_ExtendPosition(dfCenterLat, dfCenterLon, dfRADIUS, dfAngle, &dfLat, &dfLon);
                oLR.addPoint(dfLon, dfLat);
            }
            OGR_GreatCircle_ExtendPosition(dfCenterLat, dfCenterLon, dfRADIUS, 0, &dfLat, &dfLon);
            oLR.addPoint(dfLon, dfLat);

            dfLastLat = oLR.getY(oLR.getNumPoints() - 1);
            dfLastLon = oLR.getX(oLR.getNumPoints() - 1);
        }
        else if (STARTS_WITH_CI(pszLine, "INCLUDE") || STARTS_WITH_CI(pszLine, "END"))
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

int OGRSUALayer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}
