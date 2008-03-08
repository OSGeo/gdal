/******************************************************************************
 * $Id: ogr_xplane_apt_reader.cpp
 *
 * Project:  X-Plane apt.dat file reader
 * Purpose:  Implements OGRXPlaneAptReader class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "ogr_xplane_apt_reader.h"
#include "ogr_xplane_geo_utils.h"

/************************************************************************/
/*                        OGRXPlaneParseAptFile                         */
/************************************************************************/

int OGRXPlaneParseAptFile( OGRXPlaneDataSource* poDataSource, const char* pszFilename)
{
    return OGRXPlaneAptReader(poDataSource).ParseFile(pszFilename);
}

/************************************************************************/
/*                         OGRXPlaneAptReader()                         */
/************************************************************************/

OGRXPlaneAptReader::OGRXPlaneAptReader( OGRXPlaneDataSource* poDataSource )
{
    poAPTLayer = new OGRXPlaneAPTLayer();
    poRunwayLayer = new OGRXPlaneRunwayLayer();
    poRunwayThresholdLayer = new OGRXPlaneRunwayThresholdLayer();
    poWaterRunwayLayer = new OGRXPlaneWaterRunwayLayer();
    poWaterRunwayThresholdLayer = new OGRXPlaneWaterRunwayThresholdLayer();
    poHelipadLayer = new OGRXPlaneHelipadLayer();
    poHelipadPolygonLayer = new OGRXPlaneHelipadPolygonLayer();
    poTaxiwayRectangleLayer = new OGRXPlaneTaxiwayRectangleLayer();
    poATCFreqLayer = new OGRXPlaneATCFreqLayer();
    poStartupLocationLayer = new OGRXPlaneStartupLocationLayer();
    poAPTLightBeaconLayer = new OGRXPlaneAPTLightBeaconLayer();
    poAPTWindsockLayer = new OGRXPlaneAPTWindsockLayer();
    poTaxiwaySignLayer = new OGRXPlaneTaxiwaySignLayer();
    poVASI_PAPI_WIGWAG_Layer = new OGRXPlane_VASI_PAPI_WIGWAG_Layer();

    poDataSource->RegisterLayer(poAPTLayer);
    poDataSource->RegisterLayer(poRunwayLayer);
    poDataSource->RegisterLayer(poRunwayThresholdLayer);
    poDataSource->RegisterLayer(poWaterRunwayLayer);
    poDataSource->RegisterLayer(poWaterRunwayThresholdLayer);
    poDataSource->RegisterLayer(poHelipadLayer);
    poDataSource->RegisterLayer(poHelipadPolygonLayer);
    poDataSource->RegisterLayer(poTaxiwayRectangleLayer);
    poDataSource->RegisterLayer(poATCFreqLayer);
    poDataSource->RegisterLayer(poStartupLocationLayer);
    poDataSource->RegisterLayer(poAPTLightBeaconLayer);
    poDataSource->RegisterLayer(poAPTWindsockLayer);
    poDataSource->RegisterLayer(poTaxiwaySignLayer);
    poDataSource->RegisterLayer(poVASI_PAPI_WIGWAG_Layer);
}


/************************************************************************/
/*                         ParseFile()                                  */
/************************************************************************/

int OGRXPlaneAptReader::ParseFile( const char * pszFilename )
{
    fp = VSIFOpen( pszFilename, "rt" );
    if (!fp)
        return FALSE;

    const char* pszLine = CPLReadLine(fp);
    if (!pszLine || (strcmp(pszLine, "I") != 0 &&
                     strcmp(pszLine, "A") != 0))
    {
        VSIFClose(fp);
        return FALSE;
    }

    pszLine = CPLReadLine(fp);
    if (!pszLine || EQUALN(pszLine, "850 Version", 11) == FALSE &&
                    EQUALN(pszLine, "810 Version", 11) == FALSE)
    {
        VSIFClose(fp);
        return FALSE;
    }

    nLineNumber = 2;
    CPLDebug("XPlane", "Version/Copyright : %s", pszLine);

    bAptHeaderFound = FALSE;
    bTowerFound = FALSE;
    dfLatTower = 0;
    dfLonTower = 0;
    dfHeightTower = 0;
    bRunwayFound = FALSE;
    dfLatFirstRwy = 0;
    dfLonFirstRwy = 0;

    while((pszLine = CPLReadLine(fp)) != NULL)
    {
        int nType;
        papszTokens = CSLTokenizeString(pszLine);
        nTokens = CSLCount(papszTokens);

        nLineNumber ++;

        if (nTokens == 0)
        {
            goto next_line;
        }

        if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
        {
            if (bAptHeaderFound)
            {
                poAPTLayer->AddFeature(osAptICAO, osAptName, dfElevation,
                                       bTowerFound || bRunwayFound,
                                       (bTowerFound) ? dfLatTower : dfLatFirstRwy,
                                       (bTowerFound) ? dfLonTower : dfLonFirstRwy, 
                                       bTowerFound, dfHeightTower, osTowerName);
            }
            CSLDestroy(papszTokens);
            break;
        }

        if (!assertMinCol(2))
            goto next_line;

        nType = atoi(papszTokens[0]);
        if (nType == APT_AIRPORT_HEADER)
        {
            ParseAptHeaderRecord();
        }
        else if (nType == APT_RUNWAY_TAXIWAY_V_810)
        {
            ParseRunwayTaxiwayV810Record();
        }
        else if (nType == APT_PAVEMENT_HEADER)
        {
            ParsePavementHeader();
        }
        else if (nType == APT_RUNWAY)
        {
            ParseRunwayRecord();
        }
        else if (nType == APT_WATER_RUNWAY)
        {
            ParseWaterRunwayRecord();
        }
        else if (nType == APT_HELIPAD)
        {
            ParseHelipadRecord();
        }
        else if (nType == APT_TOWER)
        {
            ParseTowerRecord();
        }
        else if (nType >= APT_ATC_AWOS_ASOS_ATIS && nType <= APT_ATC_DEP)
        {
            ParseATCRecord(nType);
        }
        else if (nType == APT_STARTUP_LOCATION)
        {
            ParseStartupLocationRecord();
        }
        else if (nType == APT_LIGHT_BEACONS)
        {
            ParseLightBeaconRecord();
        }
        else if (nType == APT_WINDSOCKS)
        {
            ParseWindsockRecord();
        }
        else if (nType == APT_TAXIWAY_SIGNS)
        {
            ParseTaxiwaySignRecord();
        }
        else if (nType == APT_VASI_PAPI_WIGWAG)
        {
            ParseVasiPapiWigWagRecord();
        }

next_line:
        CSLDestroy(papszTokens);
    }

    VSIFClose(fp);

    return TRUE;
}


/************************************************************************/
/*                         ParseAptHeaderRecord()                       */
/************************************************************************/

void    OGRXPlaneAptReader::ParseAptHeaderRecord()
{
    if (bAptHeaderFound)
    {
        poAPTLayer->AddFeature(osAptICAO, osAptName, dfElevation,
                                bTowerFound || bRunwayFound,
                                (bTowerFound) ? dfLatTower : dfLatFirstRwy,
                                (bTowerFound) ? dfLonTower : dfLonFirstRwy, 
                                bTowerFound, dfHeightTower, osTowerName);
    }

    bAptHeaderFound = FALSE;
    bTowerFound = FALSE;
    bRunwayFound = FALSE;

    RET_IF_FAIL(assertMinCol(6));

    /* feet to meter */
    RET_IF_FAIL(readDoubleWithBoundsAndConversion(&dfElevation, 1, "elevation", FEET_TO_METER, -1000., 10000.));
    bControlTower = atoi(papszTokens[2]);
    // papszTokens[3] ignored
    osAptICAO = papszTokens[4];
    osAptName = readStringUntilEnd(5);

    bAptHeaderFound = TRUE;
}

/************************************************************************/
/*                    ParseRunwayTaxiwayV810Record()                    */
/************************************************************************/

void    OGRXPlaneAptReader::ParseRunwayTaxiwayV810Record()
{
    double dfLat, dfLon, dfTrueHeading, dfLength, dfWidth;
    double dfDisplacedThresholdLength1, dfDisplacedThresholdLength2;
    double dfStopwayLength1, dfStopwayLength2;
    const char* pszRwyNum;
    int eVisualApproachLightingCode1, eRunwayLightingCode1, eApproachLightingCode1;
    int eVisualApproachLightingCode2, eRunwayLightingCode2, eApproachLightingCode2;
    int eSurfaceCode, eShoulderCode, eMarkings;
    double dfSmoothness;
    double dfVisualGlidePathAngle1, dfVisualGlidePathAngle2;
    int bHasDistanceRemainingSigns;

    RET_IF_FAIL(assertMinCol(15));

    RET_IF_FAIL(readDoubleWithBounds(&dfLat, 1, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLon, 2, "longitude", -180., 180.));
    pszRwyNum = papszTokens[3];
    RET_IF_FAIL(readDoubleWithBounds(&dfTrueHeading, 4, "true heading", 0., 360.));
    RET_IF_FAIL(readDouble(&dfLength, 5, "length"));
    dfLength *= FEET_TO_METER;
    dfDisplacedThresholdLength1 = atoi(papszTokens[6]) * FEET_TO_METER;
    if (strchr(papszTokens[6], '.') != NULL)
        dfDisplacedThresholdLength2 = atoi(strchr(papszTokens[6], '.') + 1) * FEET_TO_METER;
    dfStopwayLength1 = atoi(papszTokens[7]) * FEET_TO_METER;
    if (strchr(papszTokens[7], '.') != NULL)
        dfStopwayLength2 = atoi(strchr(papszTokens[7], '.') + 1) * FEET_TO_METER;
    RET_IF_FAIL(readDouble(&dfWidth, 8, "width"));
    dfWidth *= FEET_TO_METER;
    if (strlen(papszTokens[9]) == 6)
    {
        eVisualApproachLightingCode1 = papszTokens[9][0] - '0';
        eRunwayLightingCode1 = papszTokens[9][1] - '0';
        eApproachLightingCode1 = papszTokens[9][2] - '0';
        eVisualApproachLightingCode2 = papszTokens[9][3] - '0';
        eRunwayLightingCode2 = papszTokens[9][4] - '0';
        eApproachLightingCode2 = papszTokens[9][5] - '0';
    }
    eSurfaceCode = atoi(papszTokens[10]);
    eShoulderCode = atoi(papszTokens[11]);
    eMarkings = atoi(papszTokens[12]);
    RET_IF_FAIL(readDoubleWithBounds(&dfSmoothness, 13, "runway smoothness", 0., 1.));
    bHasDistanceRemainingSigns = atoi(papszTokens[14]);
    if (nTokens == 16)
    {
        dfVisualGlidePathAngle1 = atoi(papszTokens[15]) / 100.;
        if (strchr(papszTokens[15], '.') != NULL)
            dfVisualGlidePathAngle2 = atoi(strchr(papszTokens[15], '.') + 1) / 100.;
    }

    if (strcmp(pszRwyNum, "xxx") == 000)
    {
        /* Taxiway */
        poTaxiwayRectangleLayer->AddFeature(osAptICAO, dfLat, dfLon,
                                dfTrueHeading, dfLength, dfWidth,
                                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                dfSmoothness, eRunwayLightingCode1 == 1);
    }
    else if (pszRwyNum[0] >= '0' && pszRwyNum[0] <= '9' && strlen(pszRwyNum) >= 2)
    {
        /* Runway */
        double dfLat1, dfLon1;
        double dfLat2, dfLon2;
        CPLString osRwyNum1;
        CPLString osRwyNum2;
        OGRFeature* poFeature;
        int bReil1, bReil2;

        int num1 = atoi(pszRwyNum);
        int num2 = (num1 < 18) ? num1 + 18 : num1 - 18;
        if (pszRwyNum[2] == '0' || pszRwyNum[2] == 'x')
        {
            osRwyNum1.Printf("%02d", num1);
            osRwyNum2.Printf("%02d", num2);
        }
        else
        {
            osRwyNum1 = pszRwyNum;
            osRwyNum2.Printf("%02d%c", num2,
                                (osRwyNum1[2] == 'L') ? 'R' :
                                (osRwyNum1[2] == 'R') ? 'L' : osRwyNum1[2]);
        }

        OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading + 180, &dfLat1, &dfLon1);
        OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading, &dfLat2, &dfLon2);

        bReil1 = (eRunwayLightingCode1 >= 3 && eRunwayLightingCode1 <= 5) ;
        bReil2 = (eRunwayLightingCode2 >= 3 && eRunwayLightingCode2 <= 5) ;

        poFeature =
            poRunwayThresholdLayer->AddFeature  
                (osAptICAO, osRwyNum1,
                dfLat1, dfLon1, dfWidth,
                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                RunwayShoulderEnumeration.GetText(eShoulderCode),
                dfSmoothness,
                (eRunwayLightingCode1 == 4 || eRunwayLightingCode1 == 5) /* bHasCenterLineLights */,
                (eRunwayLightingCode1 >= 2 && eRunwayLightingCode1 <= 5) /* bHasMIRL*/,
                bHasDistanceRemainingSigns,
                dfDisplacedThresholdLength1, dfStopwayLength1,
                RunwayMarkingEnumeration.GetText(eMarkings),
                RunwayApproachLightingEnumerationV810.GetText(eApproachLightingCode1),
                (eRunwayLightingCode1 == 5) /* bHasTouchdownLights */,
                (bReil1 && bReil2) ? "Omni-directional" :
                (bReil1 && !bReil2) ? "Unidirectional" : "None" /* eReil */);
        poRunwayThresholdLayer->SetRunwayLengthAndHeading(poFeature, dfLength, dfTrueHeading);

        poFeature =
            poRunwayThresholdLayer->AddFeature  
                (osAptICAO, osRwyNum2,
                dfLat2, dfLon2, dfWidth,
                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                RunwayShoulderEnumeration.GetText(eShoulderCode),
                dfSmoothness,
                (eRunwayLightingCode2 == 4 || eRunwayLightingCode2 == 5) /* bHasCenterLineLights */,
                (eRunwayLightingCode2 >= 2 && eRunwayLightingCode2 <= 5) /* bHasMIRL*/,
                bHasDistanceRemainingSigns,
                dfDisplacedThresholdLength2, dfStopwayLength2,
                RunwayMarkingEnumeration.GetText(eMarkings),
                RunwayApproachLightingEnumerationV810.GetText(eApproachLightingCode2),
                (eRunwayLightingCode2 == 5) /* : bHasTouchdownLights */,
                (bReil1 && bReil2) ? "Omni-directional" :
                (!bReil1 && bReil2) ? "Unidirectional" : "None" /* eReil */);
        poRunwayThresholdLayer->SetRunwayLengthAndHeading(poFeature, dfLength,
                (dfTrueHeading < 180) ? dfTrueHeading + 180 : dfTrueHeading - 180);

        poRunwayLayer->AddFeature(osAptICAO, osRwyNum1, osRwyNum2,
                                dfLat1, dfLon1, dfLat2, dfLon2,
                                dfWidth,
                                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                RunwayShoulderEnumeration.GetText(eShoulderCode),
                                dfSmoothness,
                                (eRunwayLightingCode1 == 4 || eRunwayLightingCode1 == 5),
                                (eRunwayLightingCode1 >= 2 && eRunwayLightingCode1 <= 5),
                                bHasDistanceRemainingSigns);

        if (eApproachLightingCode1)
        {
            poVASI_PAPI_WIGWAG_Layer->AddFeature(osAptICAO, osRwyNum1,
                    RunwayVisualApproachPathIndicatorEnumerationV810.GetText(eApproachLightingCode1),
                    dfLat1, dfLon1, dfTrueHeading, dfVisualGlidePathAngle1);
        }

        if (eApproachLightingCode2)
        {
            poVASI_PAPI_WIGWAG_Layer->AddFeature(osAptICAO, osRwyNum2,
                    RunwayVisualApproachPathIndicatorEnumerationV810.GetText(eApproachLightingCode2),
                    dfLat2, dfLon2,
                    (dfTrueHeading < 180) ? dfTrueHeading + 180 : dfTrueHeading- 180,
                    dfVisualGlidePathAngle2);
        }
    }
    else if (pszRwyNum[0] == 'H')
    {
        /* Helipad */
        CPLString osHelipadName = pszRwyNum;
        if (strlen(pszRwyNum) == 3 && pszRwyNum[2] == 'x')
            osHelipadName[2] = 0;

        poHelipadLayer->AddFeature(osAptICAO, osHelipadName, dfLat, dfLon,
                                dfTrueHeading, dfLength, dfWidth,
                                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                RunwayMarkingEnumeration.GetText(eMarkings),
                                RunwayShoulderEnumeration.GetText(eShoulderCode),
                                dfSmoothness, eRunwayLightingCode1 == 2);

        poHelipadPolygonLayer->AddFeature(osAptICAO, osHelipadName, dfLat, dfLon,
                                dfTrueHeading, dfLength, dfWidth,
                                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                RunwayMarkingEnumeration.GetText(eMarkings),
                                RunwayShoulderEnumeration.GetText(eShoulderCode),
                                dfSmoothness, eRunwayLightingCode1 == 2);
    }
    else
    {
        CPLDebug("XPlane", "Line %d : Unexpected runway number : %s",
                    nLineNumber, pszRwyNum);
    }

}

/************************************************************************/
/*                        ParseRunwayRecord()                           */
/************************************************************************/

void OGRXPlaneAptReader::ParseRunwayRecord()
{
    double dfWidth;
    int eSurfaceCode, eShoulderCode;
    double dfSmoothness;
    int bHasCenterLineLights, bHasMIRL, bHasDistanceRemainingSigns;
    int nCurToken;
    int nRwy = 0;
    double adfLat[2], adfLon[2];
    OGRFeature* apoRunwayThreshold[2];
    double dfLength;
    CPLString aosRunwayId[2];

    RET_IF_FAIL(assertMinCol(8 + 9 + 9));

    RET_IF_FAIL(readDouble(&dfWidth, 1, "runway width"));
    eSurfaceCode = atoi(papszTokens[2]);
    eShoulderCode = atoi(papszTokens[3]);
    RET_IF_FAIL(readDoubleWithBounds(&dfSmoothness, 4, "runway smoothness", 0., 1.));
    bHasCenterLineLights = atoi(papszTokens[5]);
    bHasMIRL = atoi(papszTokens[6]) == 2;
    bHasDistanceRemainingSigns = atoi(papszTokens[7]);

    for( nRwy=0, nCurToken = 8 ; nRwy<=1 ; nRwy++, nCurToken += 9 )
    {
        double dfLat, dfLon, dfDisplacedThresholdLength, dfStopwayLength;
        int eMarkings, eApproachLightingCode, eREIL;
        int bHasTouchdownLights;

        aosRunwayId[nRwy] = papszTokens[nCurToken + 0]; /* for example : 08, 24R, or xxx */
        RET_IF_FAIL(readDoubleWithBounds(&dfLat, nCurToken + 1, "latitude", -90., 90.));
        RET_IF_FAIL(readDoubleWithBounds(&dfLon, nCurToken + 2, "longitude", -180., 180.));
        adfLat[nRwy] = dfLat; 
        adfLon[nRwy] = dfLon;
        RET_IF_FAIL(readDouble(&dfDisplacedThresholdLength, nCurToken + 3, "displaced threshold length"));
        RET_IF_FAIL(readDouble(&dfStopwayLength, nCurToken + 4, "stopway/blastpad/over-run length"));
        eMarkings = atoi(papszTokens[nCurToken + 5]);
        eApproachLightingCode = atoi(papszTokens[nCurToken + 6]);
        bHasTouchdownLights = atoi(papszTokens[nCurToken + 7]);
        eREIL = atoi(papszTokens[nCurToken + 8]);

        if (!bRunwayFound)
        {
            dfLatFirstRwy = dfLat;
            dfLonFirstRwy = dfLon;
        }

        apoRunwayThreshold[nRwy] =
            poRunwayThresholdLayer->AddFeature  
                (osAptICAO, aosRunwayId[nRwy],
                dfLat, dfLon, dfWidth,
                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                RunwayShoulderEnumeration.GetText(eShoulderCode),
                dfSmoothness, bHasCenterLineLights, bHasMIRL, bHasDistanceRemainingSigns,
                dfDisplacedThresholdLength, dfStopwayLength,
                RunwayMarkingEnumeration.GetText(eMarkings),
                RunwayApproachLightingEnumeration.GetText(eApproachLightingCode),
                bHasTouchdownLights,
                RunwayREILEnumeration.GetText(eREIL));
        bRunwayFound = TRUE;
    }

    dfLength = OGRXPlane_Distance(adfLat[0], adfLon[0], adfLat[1], adfLon[1]);
    poRunwayThresholdLayer->SetRunwayLengthAndHeading(apoRunwayThreshold[0], dfLength,
                                OGRXPlane_Track(adfLat[0], adfLon[0], adfLat[1], adfLon[1]));
    poRunwayThresholdLayer->SetRunwayLengthAndHeading(apoRunwayThreshold[1], dfLength,
                                OGRXPlane_Track(adfLat[1], adfLon[1], adfLat[0], adfLon[0]));

    poRunwayLayer->AddFeature(osAptICAO, aosRunwayId[0], aosRunwayId[1],
                                adfLat[0], adfLon[0], adfLat[1], adfLon[1],
                                dfWidth,
                                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                RunwayShoulderEnumeration.GetText(eShoulderCode),
                                dfSmoothness, bHasCenterLineLights, bHasMIRL, bHasDistanceRemainingSigns);

}

/************************************************************************/
/*                       ParseWaterRunwayRecord()                       */
/************************************************************************/

void OGRXPlaneAptReader::ParseWaterRunwayRecord()
{
    double adfLat[2], adfLon[2];
    OGRFeature* apoWaterRunwayThreshold[2];
    double dfWidth, dfLength;
    int bBuoys;
    CPLString aosRunwayId[2];
    int i;

    RET_IF_FAIL(assertMinCol(9));

    RET_IF_FAIL(readDouble(&dfWidth, 1, "runway width"));
    bBuoys = atoi(papszTokens[2]);

    for(i=0;i<2;i++)
    {
        aosRunwayId[i] = papszTokens[3 + 3*i];
        RET_IF_FAIL(readDoubleWithBounds(&adfLat[i], 4 + 3*i, "latitude", -90., 90.));
        RET_IF_FAIL(readDoubleWithBounds(&adfLon[i], 5 + 3*i, "longitude", -180., 180.));

        apoWaterRunwayThreshold[i] =
            poWaterRunwayThresholdLayer->AddFeature  
                (osAptICAO, aosRunwayId[i], adfLat[i], adfLon[i], dfWidth, bBuoys);

    }

    dfLength = OGRXPlane_Distance(adfLat[0], adfLon[0], adfLat[1], adfLon[1]);

    poWaterRunwayThresholdLayer->SetRunwayLengthAndHeading(apoWaterRunwayThreshold[0], dfLength,
                                OGRXPlane_Track(adfLat[0], adfLon[0], adfLat[1], adfLon[1]));
    poWaterRunwayThresholdLayer->SetRunwayLengthAndHeading(apoWaterRunwayThreshold[1], dfLength,
                                OGRXPlane_Track(adfLat[1], adfLon[1], adfLat[0], adfLon[0]));

    poWaterRunwayLayer->AddFeature(osAptICAO, aosRunwayId[0], aosRunwayId[1],
                                adfLat[0], adfLon[0], adfLat[1], adfLon[1],
                                dfWidth, bBuoys);
}

/************************************************************************/
/*                       ParseHelipadRecord()                           */
/************************************************************************/

void OGRXPlaneAptReader::ParseHelipadRecord()
{
    double dfLat, dfLon, dfTrueHeading, dfLength, dfWidth, dfSmoothness;
    int eSurfaceCode, eMarkings, eShoulderCode;
    int bEdgeLighting;
    const char* pszHelipadName;

    RET_IF_FAIL(assertMinCol(12));

    pszHelipadName = papszTokens[1];
    RET_IF_FAIL(readDoubleWithBounds(&dfLat, 2, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLon, 3, "longitude", -180., 180.));
    RET_IF_FAIL(readDoubleWithBounds(&dfTrueHeading, 4, "true heading", 0., 360.));
    RET_IF_FAIL(readDouble(&dfLength, 5, "length"));
    RET_IF_FAIL(readDouble(&dfWidth, 6, "width"));
    eSurfaceCode = atoi(papszTokens[7]);
    eMarkings = atoi(papszTokens[8]);
    eShoulderCode = atoi(papszTokens[9]);
    RET_IF_FAIL(readDoubleWithBounds(&dfSmoothness, 10, "helipad smoothness", 0., 1.));
    bEdgeLighting = atoi(papszTokens[11]);

    poHelipadLayer->AddFeature(osAptICAO, pszHelipadName, dfLat, dfLon,
                            dfTrueHeading, dfLength, dfWidth,
                            RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                            RunwayMarkingEnumeration.GetText(eMarkings),
                            RunwayShoulderEnumeration.GetText(eShoulderCode),
                            dfSmoothness, bEdgeLighting);

    poHelipadPolygonLayer->AddFeature(osAptICAO, pszHelipadName, dfLat, dfLon,
                                    dfTrueHeading, dfLength, dfWidth,
                                    RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                    RunwayMarkingEnumeration.GetText(eMarkings),
                                    RunwayShoulderEnumeration.GetText(eShoulderCode),
                                    dfSmoothness, bEdgeLighting);
}


/************************************************************************/
/*                         ParsePavementHeader()                        */
/************************************************************************/

void OGRXPlaneAptReader::ParsePavementHeader()
{
    /*
            int eSurfaceCode;
            double dfSmoothness, dfTextureHeading;
            CPLString osPavementName;

            ASSERT_MIN_COL(4);

            eSurfaceCode = atoi(papszTokens[1]);

            dfSmoothness = readDoubleWithBounds(2, "pavement smoothness", 0., 1.);

            dfTextureHeading = readDoubleWithBounds(3, "texture heading", 0., 360.);

            READ_STRING_UNTIL_END(osPavementName, 4);
            */
}

/************************************************************************/
/*                         ParseTowerRecord()                           */
/************************************************************************/

void OGRXPlaneAptReader::ParseTowerRecord()
{
    RET_IF_FAIL(assertMinCol(6));

    RET_IF_FAIL(readDoubleWithBounds(&dfLatTower, 1, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLonTower, 2, "longitude", -180., 180.));

    /* feet to meter */
    RET_IF_FAIL(readDoubleWithBoundsAndConversion(&dfHeightTower, 3, "tower height", FEET_TO_METER, 0., 300.));

    // papszTokens[4] ignored

    osTowerName = readStringUntilEnd(5);

    bTowerFound = TRUE;
}


/************************************************************************/
/*                            ParseATCRecord()                          */
/************************************************************************/

void OGRXPlaneAptReader::ParseATCRecord(int nType)
{
    double dfFrequency;
    CPLString osFreqName;

    RET_IF_FAIL(assertMinCol(2));

    RET_IF_FAIL(readDouble(&dfFrequency, 1, "frequency"));
    dfFrequency /= 100.;

    osFreqName = readStringUntilEnd(2);

    poATCFreqLayer->AddFeature(osAptICAO,
                                (nType == APT_ATC_AWOS_ASOS_ATIS) ? "ATIS" :
                                (nType == APT_ATC_CTAF) ? "CTAF" :
                                (nType == APT_ATC_CLD) ? "CLD" :
                                (nType == APT_ATC_GND) ? "GND" :
                                (nType == APT_ATC_TWR) ? "TWR" :
                                (nType == APT_ATC_APP) ? "APP" :
                                (nType == APT_ATC_DEP) ? "DEP" : "UNK",
                                osFreqName, dfFrequency);
}


/************************************************************************/
/*                      ParseStartupLocationRecord()                    */
/************************************************************************/

void OGRXPlaneAptReader::ParseStartupLocationRecord()
{
    double dfLat, dfLon, dfTrueHeading;
    CPLString osName;

    RET_IF_FAIL(assertMinCol(4));

    RET_IF_FAIL(readDoubleWithBounds(&dfLat, 1, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLon, 2, "longitude", -180., 180.));

    RET_IF_FAIL(readDoubleWithBounds(&dfTrueHeading, 3, "true heading", -180., 360.));
    if (dfTrueHeading < 0)
        dfTrueHeading += 360;

    osName = readStringUntilEnd(4);

    poStartupLocationLayer->AddFeature(osAptICAO, osName, dfLat, dfLon, dfTrueHeading);
}

/************************************************************************/
/*                       ParseLightBeaconRecord()                       */
/************************************************************************/

void OGRXPlaneAptReader::ParseLightBeaconRecord()
{
    double dfLat, dfLon;
    int eColor;
    CPLString osName;

    RET_IF_FAIL(assertMinCol(4));

    RET_IF_FAIL(readDoubleWithBounds(&dfLat, 1, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLon, 2, "longitude", -180., 180.));
    eColor = atoi(papszTokens[3]);
    osName = readStringUntilEnd(4);

    poAPTLightBeaconLayer->AddFeature(osAptICAO, osName, dfLat, dfLon,
                                        APTLightBeaconColorEnumeration.GetText(eColor));
}

/************************************************************************/
/*                         ParseWindsockRecord()                        */
/************************************************************************/

void OGRXPlaneAptReader::ParseWindsockRecord()
{
    double dfLat, dfLon;
    int bIsIllumnited;
    CPLString osName;

    RET_IF_FAIL(assertMinCol(4));

    RET_IF_FAIL(readDoubleWithBounds(&dfLat, 1, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLon, 2, "longitude", -180., 180.));
    bIsIllumnited = atoi(papszTokens[3]);
    osName = readStringUntilEnd(4);

    poAPTWindsockLayer->AddFeature(osAptICAO, osName, dfLat, dfLon,
                                    bIsIllumnited);
}

/************************************************************************/
/*                        ParseTaxiwaySignRecord                        */
/************************************************************************/

void OGRXPlaneAptReader::ParseTaxiwaySignRecord()
{
    double dfLat, dfLon;
    double dfTrueHeading;
    int nSize;
    const char* pszText;

    RET_IF_FAIL(assertMinCol(7));

    RET_IF_FAIL(readDoubleWithBounds(&dfLat, 1, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLon, 2, "longitude", -180., 180.));
    RET_IF_FAIL(readDoubleWithBounds(&dfTrueHeading, 3, "heading", 0, 360));
    /* papszTokens[4] : ignored. Taxiway sign style */
    nSize = atoi(papszTokens[5]);
    pszText = papszTokens[6];

    poTaxiwaySignLayer->AddFeature(osAptICAO, pszText, dfLat, dfLon,
                                    dfTrueHeading, nSize);
}

/************************************************************************/
/*                    ParseVasiPapiWigWagRecord()                       */
/************************************************************************/

void OGRXPlaneAptReader::ParseVasiPapiWigWagRecord()
{
    double dfLat, dfLon;
    int eType;
    double dfTrueHeading, dfVisualGlidePathAngle;
    const char* pszRwyNum;

    RET_IF_FAIL(assertMinCol(7));

    RET_IF_FAIL(readDoubleWithBounds(&dfLat, 1, "latitude", -90., 90.));
    RET_IF_FAIL(readDoubleWithBounds(&dfLon, 2, "longitude", -180., 180.));
    eType = atoi(papszTokens[3]);
    RET_IF_FAIL(readDoubleWithBounds(&dfTrueHeading, 4, "heading", 0, 360));
    RET_IF_FAIL(readDoubleWithBounds(&dfVisualGlidePathAngle, 5, "visual glidepath angle", 0, 90));
    pszRwyNum = papszTokens[6];
    /* papszTokens[7] : ignored. Type of lighting object represented */

    poVASI_PAPI_WIGWAG_Layer->AddFeature(osAptICAO, pszRwyNum, VASI_PAPI_WIGWAG_Enumeration.GetText(eType),
                                            dfLat, dfLon,
                                            dfTrueHeading, dfVisualGlidePathAngle);
}

/************************************************************************/
/*                         OGRXPlaneAPTLayer()                          */
/************************************************************************/


OGRXPlaneAPTLayer::OGRXPlaneAPTLayer() : OGRXPlaneLayer("APT")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("apt_icao", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("apt_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldHasTower("has_tower", OFTInteger );
    oFieldHasTower.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldHasTower );

    OGRFieldDefn oFieldHeightTower("hgt_tower_m", OFTReal );
    oFieldHeightTower.SetWidth( 8 );
    oFieldHeightTower.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldHeightTower );

    OGRFieldDefn oFieldTowerName("tower_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldTowerName );

}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneAPTLayer::AddFeature(const char* pszAptICAO,
                                   const char* pszAptName,
                                   double dfElevation,
                                   int bHasCoordinates,
                                   double dfLat,
                                   double dfLon,
                                   int bHasTower,
                                   double dfHeightTower,
                                   const char* pszTowerName)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszAptName );
    poFeature->SetField( nCount++, dfElevation );
    poFeature->SetField( nCount++, bHasTower );
    if (bHasCoordinates)
    {
        poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    }
    if (bHasTower)
    {
        poFeature->SetField( nCount++, dfHeightTower );
        poFeature->SetField( nCount++, pszTowerName );
    }

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*               OGRXPlaneRunwayThresholdLayer()                        */
/************************************************************************/


OGRXPlaneRunwayThresholdLayer::OGRXPlaneRunwayThresholdLayer() : OGRXPlaneLayer("RunwayThreshold")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldCenterLineLights("centerline_lights", OFTInteger );
    oFieldCenterLineLights.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldCenterLineLights );

    OGRFieldDefn oFieldMIRL("MIRL", OFTInteger );
    oFieldMIRL.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldMIRL );

    OGRFieldDefn oFieldDistanceRemainingSigns("distance_remaining_signs", OFTInteger );
    oFieldDistanceRemainingSigns.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldDistanceRemainingSigns );

    OGRFieldDefn oFieldDisplacedThreshold("displaced_threshold_m", OFTReal );
    oFieldDisplacedThreshold.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldDisplacedThreshold );

    OGRFieldDefn oFieldStopwayLength("stopway_length_m", OFTReal );
    oFieldStopwayLength.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldStopwayLength );

    OGRFieldDefn oFieldMarkings("markings", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldMarkings );

    OGRFieldDefn oFieldApproachLighting("approach_lighting", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldApproachLighting );

    OGRFieldDefn oFieldTouchdownLights("touchdown_lights", OFTInteger );
    oFieldTouchdownLights.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldTouchdownLights );

    OGRFieldDefn oFieldREIL("REIL", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldREIL );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneRunwayThresholdLayer::AddFeature  (const char* pszAptICAO,
                                        const char* pszRwyNum,
                                        double dfLat,
                                        double dfLon,
                                        double dfWidth,
                                        const char* pszSurfaceType,
                                        const char* pszShoulderType,
                                        double dfSmoothness,
                                        int bHasCenterLineLights,
                                        int bHasMIRL,
                                        int bHasDistanceRemainingSigns,
                                        double dfDisplacedThresholdLength,
                                        double dfStopwayLength,
                                        const char* pszMarkings,
                                        const char* pszApproachLightingCode,
                                        int bHasTouchdownLights,
                                        const char* pszREIL)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bHasCenterLineLights );
    poFeature->SetField( nCount++, bHasMIRL );
    poFeature->SetField( nCount++, bHasDistanceRemainingSigns );
    poFeature->SetField( nCount++, dfDisplacedThresholdLength );
    poFeature->SetField( nCount++, dfStopwayLength );
    poFeature->SetField( nCount++, pszMarkings );
    poFeature->SetField( nCount++, pszApproachLightingCode );
    poFeature->SetField( nCount++, bHasTouchdownLights );
    poFeature->SetField( nCount++, pszREIL );

    RegisterFeature(poFeature);

    return poFeature;
}

void OGRXPlaneRunwayThresholdLayer::SetRunwayLengthAndHeading(OGRFeature* poFeature,
                                                     double dfLength,
                                                     double dfHeading)
{
    int nCount = 15;
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfHeading );
}


/************************************************************************/
/*                       OGRXPlaneRunwayLayer()                         */
/************************************************************************/



OGRXPlaneRunwayLayer::OGRXPlaneRunwayLayer() : OGRXPlaneLayer("RunwayPolygon")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum1("rwy_num1", OFTString );
    oFieldRwyNum1.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum1 );

    OGRFieldDefn oFieldRwyNum2("rwy_num2", OFTString );
    oFieldRwyNum2.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum2 );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldCenterLineLights("centerline_lights", OFTInteger );
    oFieldCenterLineLights.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldCenterLineLights );

    OGRFieldDefn oFieldMIRL("MIRL", OFTInteger );
    oFieldMIRL.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldMIRL );

    OGRFieldDefn oFieldDistanceRemainingSigns("distance_remaining_signs", OFTInteger );
    oFieldDistanceRemainingSigns.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldDistanceRemainingSigns );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/


OGRFeature*
     OGRXPlaneRunwayLayer::AddFeature  (const char* pszAptICAO,
                                        const char* pszRwyNum1,
                                        const char* pszRwyNum2,
                                        double dfLat1,
                                        double dfLon1,
                                        double dfLat2,
                                        double dfLon2,
                                        double dfWidth,
                                        const char* pszSurfaceType,
                                        const char* pszShoulderType,
                                        double dfSmoothness,
                                        int bHasCenterLineLights,
                                        int bHasMIRL,
                                        int bHasDistanceRemainingSigns)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfLength = OGRXPlane_Distance(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack12 = OGRXPlane_Track(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack21 = OGRXPlane_Track(dfLat2, dfLon2, dfLat1, dfLon1);
    double adfLat[4], adfLon[4];
    
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 + 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 - 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 + 90, &adfLat[3], &adfLon[3]);
    
    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum1 );
    poFeature->SetField( nCount++, pszRwyNum2 );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bHasCenterLineLights );
    poFeature->SetField( nCount++, bHasMIRL );
    poFeature->SetField( nCount++, bHasDistanceRemainingSigns );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfTrack12 );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*               OGRXPlaneWaterRunwayThresholdLayer()                   */
/************************************************************************/


OGRXPlaneWaterRunwayThresholdLayer::OGRXPlaneWaterRunwayThresholdLayer() : OGRXPlaneLayer("WaterRunwayThreshold")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldHasBuoys("has_buoys", OFTInteger );
    oFieldHasBuoys.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldHasBuoys );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneWaterRunwayThresholdLayer::AddFeature  (const char* pszAptICAO,
                                                      const char* pszRwyNum,
                                                      double dfLat,
                                                      double dfLon,
                                                      double dfWidth,
                                                      int bBuoys)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, bBuoys );

    RegisterFeature(poFeature);

    return poFeature;
}

void OGRXPlaneWaterRunwayThresholdLayer::SetRunwayLengthAndHeading(OGRFeature* poFeature,
                                                     double dfLength,
                                                     double dfHeading)
{
    int nCount = 4;
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfHeading );
}


/************************************************************************/
/*                      OGRXPlaneWaterRunwayLayer()                     */
/************************************************************************/



OGRXPlaneWaterRunwayLayer::OGRXPlaneWaterRunwayLayer() : OGRXPlaneLayer("WaterRunwayPolygon")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum1("rwy_num1", OFTString );
    oFieldRwyNum1.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum1 );

    OGRFieldDefn oFieldRwyNum2("rwy_num2", OFTString );
    oFieldRwyNum2.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum2 );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldHasBuoys("has_buoys", OFTInteger );
    oFieldHasBuoys.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldHasBuoys );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneWaterRunwayLayer::AddFeature  (const char* pszAptICAO,
                                             const char* pszRwyNum1,
                                             const char* pszRwyNum2,
                                             double dfLat1,
                                             double dfLon1,
                                             double dfLat2,
                                             double dfLon2,
                                             double dfWidth,
                                             int bBuoys)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfLength = OGRXPlane_Distance(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack12 = OGRXPlane_Track(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack21 = OGRXPlane_Track(dfLat2, dfLon2, dfLat1, dfLon1);
    double adfLat[4], adfLon[4];
    
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 + 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 - 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 + 90, &adfLat[3], &adfLon[3]);
    
    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum1 );
    poFeature->SetField( nCount++, pszRwyNum2 );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, bBuoys );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfTrack12 );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                     OGRXPlaneHelipadLayer()                          */
/************************************************************************/


OGRXPlaneHelipadLayer::OGRXPlaneHelipadLayer() : OGRXPlaneLayer("Helipad")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldHelipadName("helipad_name", OFTString );
    oFieldHelipadName.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldHelipadName );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldMarkings("markings", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldMarkings );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldYellowEdgeLighting("edge_lighting", OFTInteger );
    oFieldYellowEdgeLighting.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldYellowEdgeLighting );

}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneHelipadLayer::AddFeature (const char* pszAptICAO,
                                        const char* pszHelipadNum,
                                        double dfLat,
                                        double dfLon,
                                        double dfTrueHeading,
                                        double dfLength,
                                        double dfWidth,
                                        const char* pszSurfaceType,
                                        const char* pszMarkings,
                                        const char* pszShoulderType,
                                        double dfSmoothness,
                                        int bYellowEdgeLights)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszHelipadNum );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszMarkings );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bYellowEdgeLights );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                 OGRXPlaneHelipadPolygonLayer()                       */
/************************************************************************/


OGRXPlaneHelipadPolygonLayer::OGRXPlaneHelipadPolygonLayer() : OGRXPlaneLayer("HelipadPolygon")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldHelipadName("helipad_name", OFTString );
    oFieldHelipadName.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldHelipadName );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldMarkings("markings", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldMarkings );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldYellowEdgeLighting("edge_lighting", OFTInteger );
    oFieldYellowEdgeLighting.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldYellowEdgeLighting );

}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneHelipadPolygonLayer::AddFeature (const char* pszAptICAO,
                                               const char* pszHelipadNum,
                                               double dfLat,
                                               double dfLon,
                                               double dfTrueHeading,
                                               double dfLength,
                                               double dfWidth,
                                               const char* pszSurfaceType,
                                               const char* pszMarkings,
                                               const char* pszShoulderType,
                                               double dfSmoothness,
                                               int bYellowEdgeLights)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfBeforeLat, dfBeforeLon;
    double dfAfterLat, dfAfterLon;
    double adfLat[4], adfLon[4];

    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading + 180, &dfBeforeLat, &dfBeforeLon);
    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading, &dfAfterLat, &dfAfterLon);

    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[3], &adfLon[3]);

    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszHelipadNum );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszMarkings );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bYellowEdgeLights );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                 OGRXPlaneTaxiwayRectangleLayer()                     */
/************************************************************************/


OGRXPlaneTaxiwayRectangleLayer::OGRXPlaneTaxiwayRectangleLayer() : OGRXPlaneLayer("TaxiwayRectangle")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldBlueEdgeLighting("edge_lighting", OFTInteger );
    oFieldBlueEdgeLighting.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldBlueEdgeLighting );

}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneTaxiwayRectangleLayer::AddFeature(const char* pszAptICAO,
                                                double dfLat,
                                                double dfLon,
                                                double dfTrueHeading,
                                                double dfLength,
                                                double dfWidth,
                                                const char* pszSurfaceType,
                                                double dfSmoothness,
                                                int bBlueEdgeLights)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfBeforeLat, dfBeforeLon;
    double dfAfterLat, dfAfterLon;
    double adfLat[4], adfLon[4];

    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading + 180, &dfBeforeLat, &dfBeforeLon);
    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading, &dfAfterLat, &dfAfterLon);

    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[3], &adfLon[3]);

    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bBlueEdgeLights );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                        OGRXPlaneATCFreqLayer()                       */
/************************************************************************/


OGRXPlaneATCFreqLayer::OGRXPlaneATCFreqLayer() : OGRXPlaneLayer("ATCFreq")
{
    poFeatureDefn->SetGeomType( wkbNone );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldATCFreqType("atc_type", OFTString );
    oFieldATCFreqType.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldATCFreqType );

    OGRFieldDefn oFieldATCFreqName("freq_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldATCFreqName );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneATCFreqLayer::AddFeature (const char* pszAptICAO,
                                        const char* pszATCType,
                                        const char* pszATCFreqName,
                                        double dfFrequency)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszATCType );
    poFeature->SetField( nCount++, pszATCFreqName );
    poFeature->SetField( nCount++, dfFrequency );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                     OGRXPlaneStartupLocationLayer()                  */
/************************************************************************/

OGRXPlaneStartupLocationLayer::OGRXPlaneStartupLocationLayer() : OGRXPlaneLayer("StartupLocation")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneStartupLocationLayer::AddFeature (const char* pszAptICAO,
                                                const char* pszName,
                                                double dfLat,
                                                double dfLon,
                                                double dfTrueHeading)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszName );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfTrueHeading );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                      OGRXPlaneAPTLightBeaconLayer()                  */
/************************************************************************/

OGRXPlaneAPTLightBeaconLayer::OGRXPlaneAPTLightBeaconLayer() : OGRXPlaneLayer("APTLightBeacon")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldColor("color", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldColor );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneAPTLightBeaconLayer::AddFeature (const char* pszAptICAO,
                                               const char* pszName,
                                               double dfLat,
                                               double dfLon,
                                               const char* pszColor)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszName );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszColor );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                        OGRXPlaneAPTWindsockLayer()                   */
/************************************************************************/

OGRXPlaneAPTWindsockLayer::OGRXPlaneAPTWindsockLayer() : OGRXPlaneLayer("APTWindsock")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldIsIlluminated("is_illuminated", OFTInteger );
    oFieldIsIlluminated.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldIsIlluminated );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneAPTWindsockLayer::AddFeature (const char* pszAptICAO,
                                            const char* pszName,
                                            double dfLat,
                                            double dfLon,
                                            int bIsIllumnited)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszName );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, bIsIllumnited );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                        OGRXPlaneTaxiwaySignLayer()                   */
/************************************************************************/

OGRXPlaneTaxiwaySignLayer::OGRXPlaneTaxiwaySignLayer() : OGRXPlaneLayer("TaxiwaySign")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldText("text", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldText );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldSize("size", OFTInteger );
    oFieldSize.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldSize );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneTaxiwaySignLayer::AddFeature (const char* pszAptICAO,
                                            const char* pszText,
                                            double dfLat,
                                            double dfLon,
                                            double dfHeading,
                                            int nSize)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszText );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfHeading );
    poFeature->SetField( nCount++, nSize );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                   OGRXPlane_VASI_PAPI_WIGWAG_Layer()                 */
/************************************************************************/

OGRXPlane_VASI_PAPI_WIGWAG_Layer::OGRXPlane_VASI_PAPI_WIGWAG_Layer() : OGRXPlaneLayer("VASI_PAPI_WIGWAG")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldType("type", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldType );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldVisualGlidePathAngle("visual_glide_deg", OFTReal );
    oFieldVisualGlidePathAngle.SetWidth( 4 );
    oFieldVisualGlidePathAngle.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldVisualGlidePathAngle );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlane_VASI_PAPI_WIGWAG_Layer::AddFeature (const char* pszAptICAO,
                                                   const char* pszRwyNum,
                                                   const char* pszObjectType,
                                                   double dfLat,
                                                   double dfLon,
                                                   double dfHeading,
                                                   double dfVisualGlidePathAngle)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, pszObjectType );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfHeading );
    poFeature->SetField( nCount++, dfVisualGlidePathAngle );

    RegisterFeature(poFeature);

    return poFeature;
}
