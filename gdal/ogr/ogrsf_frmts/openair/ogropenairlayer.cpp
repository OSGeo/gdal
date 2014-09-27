/******************************************************************************
 * $Id$
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

#include "ogr_openair.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_xplane_geo_utils.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGROpenAirLayer()                            */
/************************************************************************/

OGROpenAirLayer::OGROpenAirLayer( VSILFILE* fp )

{
    fpOpenAir = fp;
    nNextFID = 0;
    bEOF = FALSE;
    bHasLastLine = FALSE;

    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);

    poFeatureDefn = new OGRFeatureDefn( "airspaces" );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPolygon );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    OGRFieldDefn    oField1( "CLASS", OFTString);
    poFeatureDefn->AddFieldDefn( &oField1 );
    OGRFieldDefn    oField2( "NAME", OFTString);
    poFeatureDefn->AddFieldDefn( &oField2 );
    OGRFieldDefn    oField3( "FLOOR", OFTString);
    poFeatureDefn->AddFieldDefn( &oField3 );
    OGRFieldDefn    oField4( "CEILING", OFTString);
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
    bEOF = FALSE;
    bHasLastLine = FALSE;
    VSIFSeekL( fpOpenAir, 0, SEEK_SET );
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGROpenAirLayer::GetNextFeature()
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
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGROpenAirLayer::GetNextRawFeature()
{
    const char* pszLine;
    CPLString osCLASS, osNAME, osFLOOR, osCEILING;
    OGRLinearRing oLR;
    /* double dfLastLat = 0, dfLastLon = 0; */
    int bFirst = TRUE;
    int bClockWise = TRUE;
    double dfCenterLat = 0, dfCenterLon = 0;
    int bHasCenter = FALSE;
    OpenAirStyle sStyle;
    sStyle.penStyle = -1;
    sStyle.penWidth = -1;
    sStyle.penR = sStyle.penG = sStyle.penB = -1;
    sStyle.fillR = sStyle.fillG = sStyle.fillB = -1;

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
            pszLine = CPLReadLine2L(fpOpenAir, 1024, NULL);
            if (pszLine == NULL)
            {
                bEOF = TRUE;
                if (oLR.getNumPoints() == 0)
                    return NULL;

                if (osCLASS.size() != 0 &&
                    oStyleMap.find(osCLASS) != oStyleMap.end())
                {
                    memcpy(&sStyle, oStyleMap[osCLASS], sizeof(sStyle));
                }
                break;
            }
            osLastLine = pszLine;
            bHasLastLine = TRUE;
        }

        if (pszLine[0] == '*' || pszLine[0] == '\0')
            continue;

        if (EQUALN(pszLine, "AC ", 3) || EQUALN(pszLine, "AC,", 3))
        {
            if (osCLASS.size() != 0)
            {
                if (sStyle.penStyle != -1 || sStyle.fillR != -1)
                {
                    if (oLR.getNumPoints() == 0)
                    {
                        OpenAirStyle* psStyle;
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
            bClockWise = TRUE;
            bHasCenter = FALSE;
        }
        else if (EQUALN(pszLine, "AN ", 3))
        {
            if (osNAME.size() != 0)
                break;
            osNAME = pszLine + 3;
        }
        else if (EQUALN(pszLine, "AH ", 3))
            osCEILING = pszLine + 3;
        else if (EQUALN(pszLine, "AL ", 3))
            osFLOOR = pszLine + 3;
        else if (EQUALN(pszLine, "AT ", 3))
        {
            /* Ignored for that layer*/
        }
        else if (EQUALN(pszLine, "SP ", 3))
        {
            if (osCLASS.size() != 0)
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
        else if (EQUALN(pszLine, "SB ", 3))
        {
            if (osCLASS.size() != 0)
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
        else if (EQUALN(pszLine, "DP ", 3))
        {
            pszLine += 3;

            double dfLat, dfLon;
            if (!OGROpenAirGetLatLon(pszLine, dfLat, dfLon))
                continue;

            oLR.addPoint(dfLon, dfLat);
            /* dfLastLat = dfLat; */
            /* dfLastLon = dfLon; */
        }
        else if (EQUALN(pszLine, "DA ", 3))
        {
            pszLine += 3;

            char* pszStar = strchr((char*)pszLine, '*');
            if (pszStar) *pszStar = 0;
            char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
            if (bHasCenter && CSLCount(papszTokens) == 3)
            {
                double dfRadius = atof(papszTokens[0]) * 1852;
                double dfStartAngle = atof(papszTokens[1]);
                double dfEndAngle = atof(papszTokens[2]);

                if (bClockWise && dfEndAngle < dfStartAngle)
                    dfEndAngle += 360;
                else if (!bClockWise && dfStartAngle < dfEndAngle)
                    dfEndAngle -= 360;

                double dfStartDistance = dfRadius;
                double dfEndDistance = dfRadius;
                int nSign = (bClockWise) ? 1 : -1;
                double dfAngle;
                double dfLat, dfLon;
                for(dfAngle = dfStartAngle;
                    (dfAngle - dfEndAngle) * nSign < 0;
                    dfAngle += nSign)
                {
                    double pct = (dfAngle - dfStartAngle) /
                                    (dfEndAngle - dfStartAngle);
                    double dfDist = dfStartDistance * (1-pct) +
                                                        dfEndDistance * pct;
                    OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon,
                                             dfDist, dfAngle, &dfLat, &dfLon);
                    oLR.addPoint(dfLon, dfLat);
                }
                OGRXPlane_ExtendPosition(dfCenterLat, dfCenterLon,
                                         dfEndDistance, dfEndAngle, &dfLat, &dfLon);
                oLR.addPoint(dfLon, dfLat);

                /* dfLastLat = oLR.getY(oLR.getNumPoints() - 1); */
                /* dfLastLon = oLR.getX(oLR.getNumPoints() - 1); */
            }
            CSLDestroy(papszTokens);
        }
        else if (EQUALN(pszLine, "DB ", 3))
        {
            pszLine += 3;

            char* pszStar = strchr((char*)pszLine, '*');
            if (pszStar) *pszStar = 0;
            char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
            double dfFirstLat, dfFirstLon;
            double dfSecondLat, dfSecondLon;
            if (bHasCenter && CSLCount(papszTokens) == 2 &&
                OGROpenAirGetLatLon(papszTokens[0], dfFirstLat, dfFirstLon) &&
                OGROpenAirGetLatLon(papszTokens[1], dfSecondLat, dfSecondLon))
            {
                double dfStartDistance =OGRXPlane_Distance(dfCenterLat,
                        dfCenterLon, dfFirstLat, dfFirstLon);
                double dfEndDistance = OGRXPlane_Distance(dfCenterLat,
                        dfCenterLon, dfSecondLat, dfSecondLon);
                double dfStartAngle = OGRXPlane_Track(dfCenterLat,
                        dfCenterLon, dfFirstLat, dfFirstLon);
                double dfEndAngle = OGRXPlane_Track(dfCenterLat,
                        dfCenterLon, dfSecondLat, dfSecondLon);

                if (bClockWise && dfEndAngle < dfStartAngle)
                    dfEndAngle += 360;
                else if (!bClockWise && dfStartAngle < dfEndAngle)
                    dfEndAngle -= 360;

                int nSign = (bClockWise) ? 1 : -1;
                double dfAngle;
                for(dfAngle = dfStartAngle;
                    (dfAngle - dfEndAngle) * nSign < 0;
                    dfAngle += nSign)
                {
                    double dfLat, dfLon;
                    double pct = (dfAngle - dfStartAngle) /
                                    (dfEndAngle - dfStartAngle);
                    double dfDist = dfStartDistance * (1-pct) +
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
        else if ((EQUALN(pszLine, "DC ", 3) || EQUALN(pszLine, "DC=", 3)) &&
                 (bHasCenter || strstr(pszLine, "V X=") != NULL))
        {
            if (!bHasCenter)
            {
                const char* pszVX = strstr(pszLine, "V X=");
                bHasCenter = OGROpenAirGetLatLon(pszVX, dfCenterLat, dfCenterLon);
            }
            if (bHasCenter)
            {
                pszLine += 3;

                double dfRADIUS = atof(pszLine) * 1852;

                double dfAngle;
                double dfLat, dfLon;
                for(dfAngle = 0; dfAngle < 360; dfAngle += 1)
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
        else if (EQUALN(pszLine, "V X=", 4))
        {
            bHasCenter =
                    OGROpenAirGetLatLon(pszLine + 4, dfCenterLat, dfCenterLon);
        }
        else if (EQUALN(pszLine, "V D=-", 5))
        {
            bClockWise = FALSE;
        }
        else if (EQUALN(pszLine, "V D=+", 5))
        {
            bClockWise = TRUE;
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
            if (osStyle.size() != 0)
                osStyle += ";";
            osStyle += CPLString().Printf("BRUSH(fc:#%02X%02X%02X)",
                                 sStyle.fillR, sStyle.fillG, sStyle.fillB);
        }
        else
        {
            if (osStyle.size() != 0)
                osStyle += ";";
            osStyle += "BRUSH(fc:#00000000,id:\"ogr-brush-1\")";
        }
        if (osStyle.size() != 0)
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

int OGROpenAirLayer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}
