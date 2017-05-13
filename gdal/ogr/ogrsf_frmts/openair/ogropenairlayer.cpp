/******************************************************************************
 *
 * Project:  OpenAir Translator
 * Purpose:  Implements OGROpenAirLayer class.
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
#include "ogr_p.h"
#include "ogr_openair.h"
#include "ogr_srs_api.h"
#include "ogr_xplane_geo_utils.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGROpenAirLayer()                            */
/************************************************************************/

OGROpenAirLayer::OGROpenAirLayer( VSILFILE* fp ) :
    poFeatureDefn(new OGRFeatureDefn( "airspaces" )),
    poSRS(new OGRSpatialReference(SRS_WKT_WGS84)),
    fpOpenAir(fp),
    bEOF(false),
    bHasLastLine(false),
    nNextFID(0)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPolygon );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    OGRFieldDefn oField1( "CLASS", OFTString);
    poFeatureDefn->AddFieldDefn( &oField1 );
    OGRFieldDefn oField2( "NAME", OFTString);
    poFeatureDefn->AddFieldDefn( &oField2 );
    OGRFieldDefn oField3( "FLOOR", OFTString);
    poFeatureDefn->AddFieldDefn( &oField3 );
    OGRFieldDefn oField4( "CEILING", OFTString);
    poFeatureDefn->AddFieldDefn( &oField4 );
}

/************************************************************************/
/*                         ~OGROpenAirLayer()                           */
/************************************************************************/

OGROpenAirLayer::~OGROpenAirLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    poFeatureDefn->Release();

    std::map<CPLString,OpenAirStyle*>::const_iterator iter;

    for( iter = oStyleMap.begin(); iter != oStyleMap.end(); ++iter )
    {
        CPLFree(iter->second);
    }

    VSIFCloseL( fpOpenAir );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROpenAirLayer::ResetReading()

{
    nNextFID = 0;
    bEOF = false;
    bHasLastLine = false;
    VSIFSeekL( fpOpenAir, 0, SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGROpenAirLayer::GetNextFeature()
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

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGROpenAirLayer::GetNextRawFeature()
{
    if( bEOF )
        return NULL;

    CPLString osCLASS;
    CPLString osNAME;
    CPLString osFLOOR;
    CPLString osCEILING;
    OGRLinearRing oLR;
    // double dfLastLat = 0.0;
    // double dfLastLon = 0.0;
    bool bFirst = true;
    bool bClockWise = true;
    double dfCenterLat = 0.0;
    double dfCenterLon = 0.0;
    bool bHasCenter = false;
    OpenAirStyle sStyle;
    sStyle.penStyle = -1;
    sStyle.penWidth = -1;
    sStyle.penR = sStyle.penG = sStyle.penB = -1;
    sStyle.fillR = sStyle.fillG = sStyle.fillB = -1;

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
            pszLine = CPLReadLine2L(fpOpenAir, 1024, NULL);
            if (pszLine == NULL)
            {
                bEOF = true;
                if (oLR.getNumPoints() == 0)
                    return NULL;

                if (!osCLASS.empty() &&
                    oStyleMap.find(osCLASS) != oStyleMap.end())
                {
                    memcpy(&sStyle, oStyleMap[osCLASS], sizeof(sStyle));
                }
                break;
            }
            osLastLine = pszLine;
            bHasLastLine = true;
        }

        if (pszLine[0] == '*' || pszLine[0] == '\0')
            continue;

        if (STARTS_WITH_CI(pszLine, "AC ") || STARTS_WITH_CI(pszLine, "AC,"))
        {
            if (!osCLASS.empty())
            {
                if (sStyle.penStyle != -1 || sStyle.fillR != -1)
                {
                    if (oLR.getNumPoints() == 0)
                    {
                        OpenAirStyle* psStyle = NULL;
                        if (oStyleMap.find(osCLASS) == oStyleMap.end())
                        {
                            psStyle = (OpenAirStyle*)CPLMalloc(
                                                    sizeof(OpenAirStyle));
                            oStyleMap[osCLASS] = psStyle;
                        }
                        else
                            psStyle = oStyleMap[osCLASS];
                        memcpy(psStyle, &sStyle, sizeof(sStyle));
                    }
                    else
                        break;
                }
                else if (oStyleMap.find(osCLASS) != oStyleMap.end())
                {
                    memcpy(&sStyle, oStyleMap[osCLASS], sizeof(sStyle));
                    break;
                }
                else
                    break;
            }
            sStyle.penStyle = -1;
            sStyle.penWidth = -1;
            sStyle.penR = sStyle.penG = sStyle.penB = -1;
            sStyle.fillR = sStyle.fillG = sStyle.fillB = -1;
            osCLASS = pszLine + 3;
            bClockWise = true;
            bHasCenter = false;
        }
        else if (STARTS_WITH_CI(pszLine, "AN "))
        {
            if (!osNAME.empty())
                break;
            osNAME = pszLine + 3;
        }
        else if (STARTS_WITH_CI(pszLine, "AH "))
            osCEILING = pszLine + 3;
        else if (STARTS_WITH_CI(pszLine, "AL "))
            osFLOOR = pszLine + 3;
        else if (STARTS_WITH_CI(pszLine, "AT "))
        {
            /* Ignored for that layer*/
        }
        else if (STARTS_WITH_CI(pszLine, "SP "))
        {
            if (!osCLASS.empty())
            {
                char** papszTokens = CSLTokenizeString2(pszLine+3, ", ", 0);
                if (CSLCount(papszTokens) == 5)
                {
                    sStyle.penStyle = atoi(papszTokens[0]);
                    sStyle.penWidth = atoi(papszTokens[1]);
                    sStyle.penR = atoi(papszTokens[2]);
                    sStyle.penG = atoi(papszTokens[3]);
                    sStyle.penB = atoi(papszTokens[4]);
                }
                CSLDestroy(papszTokens);
            }
        }
        else if (STARTS_WITH_CI(pszLine, "SB "))
        {
            if (!osCLASS.empty())
            {
                char** papszTokens = CSLTokenizeString2(pszLine+3, ", ", 0);
                if (CSLCount(papszTokens) == 3)
                {
                    sStyle.fillR = atoi(papszTokens[0]);
                    sStyle.fillG = atoi(papszTokens[1]);
                    sStyle.fillB = atoi(papszTokens[2]);
                }
                CSLDestroy(papszTokens);
            }
        }
        else if (STARTS_WITH_CI(pszLine, "DP "))
        {
            pszLine += 3;

            double dfLat = 0.0;
            double dfLon = 0.0;
            if (!OGROpenAirGetLatLon(pszLine, dfLat, dfLon))
                continue;

            oLR.addPoint(dfLon, dfLat);
            /* dfLastLat = dfLat; */
            /* dfLastLon = dfLon; */
        }
        else if (STARTS_WITH_CI(pszLine, "DA "))
        {
            pszLine += 3;

            // TODO: Explain why writing a zero over the star.
            char* pszStar = strchr(const_cast<char *>(pszLine), '*');
            if (pszStar) *pszStar = 0;
            char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
            if (bHasCenter && CSLCount(papszTokens) == 3)
            {
                // TODO: Explain the 1852.
                const double dfRadius = CPLAtof(papszTokens[0]) * 1852;
                const double dfStartAngle = CPLAtof(papszTokens[1]);
                double dfEndAngle = CPLAtof(papszTokens[2]);

                if (bClockWise && dfEndAngle < dfStartAngle)
                    dfEndAngle += 360;
                else if (!bClockWise && dfStartAngle < dfEndAngle)
                    dfEndAngle -= 360;

                const double dfStartDistance = dfRadius;
                const double dfEndDistance = dfRadius;
                const int nSign = (bClockWise) ? 1 : -1;
                double dfLat = 0.0;
                double dfLon = 0.0;
                for(double dfAngle = dfStartAngle;
                    (dfAngle - dfEndAngle) * nSign < 0 &&
                    fabs(dfStartAngle - dfEndAngle) <= 360.0;
                    dfAngle += nSign)
                {
                    const double pct = (dfAngle - dfStartAngle) /
                        (dfEndAngle - dfStartAngle);
                    const double dfDist = dfStartDistance * (1-pct) +
                        dfEndDistance * pct;
                    OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon,
                                             dfDist, dfAngle, &dfLat, &dfLon);
                    oLR.addPoint(dfLon, dfLat);
                }
                OGRXPlane_ExtendPosition(
                    dfCenterLat, dfCenterLon,
                    dfEndDistance, dfEndAngle, &dfLat, &dfLon );
                oLR.addPoint(dfLon, dfLat);

                /* dfLastLat = oLR.getY(oLR.getNumPoints() - 1); */
                /* dfLastLon = oLR.getX(oLR.getNumPoints() - 1); */
            }
            CSLDestroy(papszTokens);
        }
        else if (STARTS_WITH_CI(pszLine, "DB "))
        {
            pszLine += 3;

            char* pszStar = strchr(const_cast<char *>(pszLine), '*');
            if (pszStar) *pszStar = 0;
            char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
            double dfFirstLat = 0.0;
            double dfFirstLon = 0.0;
            double dfSecondLat = 0.0;
            double dfSecondLon = 0.0;
            if (bHasCenter && CSLCount(papszTokens) == 2 &&
                OGROpenAirGetLatLon(papszTokens[0], dfFirstLat, dfFirstLon) &&
                OGROpenAirGetLatLon(papszTokens[1], dfSecondLat, dfSecondLon))
            {
                const double dfStartDistance = OGRXPlane_Distance(dfCenterLat,
                        dfCenterLon, dfFirstLat, dfFirstLon);
                const double dfEndDistance = OGRXPlane_Distance(dfCenterLat,
                        dfCenterLon, dfSecondLat, dfSecondLon);
                const double dfStartAngle = OGRXPlane_Track(dfCenterLat,
                        dfCenterLon, dfFirstLat, dfFirstLon);
                double dfEndAngle = OGRXPlane_Track(dfCenterLat,
                        dfCenterLon, dfSecondLat, dfSecondLon);

                if (bClockWise && dfEndAngle < dfStartAngle)
                    dfEndAngle += 360;
                else if (!bClockWise && dfStartAngle < dfEndAngle)
                    dfEndAngle -= 360;

                const int nSign = (bClockWise) ? 1 : -1;
                for(double dfAngle = dfStartAngle;
                    (dfAngle - dfEndAngle) * nSign < 0 &&
                    fabs(dfStartAngle - dfEndAngle) <= 360.0;
                    dfAngle += nSign)
                {
                    double dfLat = 0.0;
                    double dfLon = 0.0;
                    const double pct = (dfAngle - dfStartAngle) /
                        (dfEndAngle - dfStartAngle);
                    const double dfDist = dfStartDistance * (1-pct) +
                        dfEndDistance * pct;
                    OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon,
                                             dfDist, dfAngle, &dfLat, &dfLon);
                    oLR.addPoint(dfLon, dfLat);
                }
                oLR.addPoint(dfSecondLon, dfSecondLat);

                /* dfLastLat = oLR.getY(oLR.getNumPoints() - 1); */
                /* dfLastLon = oLR.getX(oLR.getNumPoints() - 1); */
            }
            CSLDestroy(papszTokens);
        }
        else if ((STARTS_WITH_CI(pszLine, "DC ") ||
                  STARTS_WITH_CI(pszLine, "DC=")) &&
                 (bHasCenter || strstr(pszLine, "V X=") != NULL))
        {
            if (!bHasCenter)
            {
                const char* pszVX = strstr(pszLine, "V X=");
                if( pszVX != NULL )
                    bHasCenter =
                        OGROpenAirGetLatLon(pszVX, dfCenterLat, dfCenterLon);
            }
            if (bHasCenter)
            {
                pszLine += 3;

                const double dfRADIUS = CPLAtof(pszLine) * 1852;
                double dfLat = 0.0;
                double dfLon = 0.0;
                for( double dfAngle = 0; dfAngle < 360; dfAngle += 1.0 )
                {
                    OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon,
                                             dfRADIUS, dfAngle, &dfLat, &dfLon);
                    oLR.addPoint(dfLon, dfLat);
                }
                OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon,
                                         dfRADIUS, 0, &dfLat, &dfLon);
                oLR.addPoint(dfLon, dfLat);

                /* dfLastLat = oLR.getY(oLR.getNumPoints() - 1); */
                /* dfLastLon = oLR.getX(oLR.getNumPoints() - 1); */
            }
        }
        else if (STARTS_WITH_CI(pszLine, "V X="))
        {
            bHasCenter =
                    OGROpenAirGetLatLon(pszLine + 4, dfCenterLat, dfCenterLon);
        }
        else if (STARTS_WITH_CI(pszLine, "V D=-"))
        {
            bClockWise = false;
        }
        else if (STARTS_WITH_CI(pszLine, "V D=+"))
        {
            bClockWise = true;
        }
        else
        {
            //CPLDebug("OpenAir", "Unexpected content : %s", pszLine);
        }
    }

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField(0, osCLASS.c_str());
    poFeature->SetField(1, osNAME.c_str());
    poFeature->SetField(2, osFLOOR.c_str());
    poFeature->SetField(3, osCEILING.c_str());

    if (sStyle.penStyle != -1 || sStyle.fillR != -1)
    {
        CPLString osStyle;
        if (sStyle.penStyle != -1)
        {
            osStyle += CPLString().Printf("PEN(c:#%02X%02X%02X,w:%dpt",
                                 sStyle.penR, sStyle.penG, sStyle.penB,
                                 sStyle.penWidth);
            if (sStyle.penStyle == 1)
                osStyle += ",p:\"5px 5px\"";
            osStyle += ")";
        }
        if (sStyle.fillR != -1)
        {
            if (!osStyle.empty())
                osStyle += ";";
            osStyle += CPLString().Printf("BRUSH(fc:#%02X%02X%02X)",
                                 sStyle.fillR, sStyle.fillG, sStyle.fillB);
        }
        else
        {
            if (!osStyle.empty())
                osStyle += ";";
            osStyle += "BRUSH(fc:#00000000,id:\"ogr-brush-1\")";
        }
        if (!osStyle.empty())
            poFeature->SetStyleString(osStyle);
    }

    OGRPolygon* poPoly = new OGRPolygon();
    oLR.closeRings();
    poPoly->addRing(&oLR);
    poPoly->assignSpatialReference(poSRS);
    poFeature->SetGeometryDirectly(poPoly);
    poFeature->SetFID(nNextFID++);

    return poFeature;
}
/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROpenAirLayer::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}
