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

CPL_CVSID("$Id$");

/************************************************************************/
/*                   OGRXPlaneCreateAptFileReader                       */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneCreateAptFileReader( OGRXPlaneDataSource* poDataSource )
{
    OGRXPlaneReader* poReader = new OGRXPlaneAptReader(poDataSource);
    return poReader;
}

/************************************************************************/
/*                         OGRXPlaneAptReader()                         */
/************************************************************************/
OGRXPlaneAptReader::OGRXPlaneAptReader()
{
    poAPTLayer = NULL;
    poRunwayLayer = NULL;
    poRunwayThresholdLayer = NULL;
    poStopwayLayer = NULL;
    poWaterRunwayLayer = NULL;
    poWaterRunwayThresholdLayer = NULL;
    poHelipadLayer = NULL;
    poHelipadPolygonLayer = NULL;
    poTaxiwayRectangleLayer =NULL;
    poPavementLayer = NULL;
    poAPTBoundaryLayer = NULL;
    poAPTLinearFeatureLayer = NULL;
    poATCFreqLayer = NULL;
    poStartupLocationLayer = NULL;
    poAPTLightBeaconLayer = NULL;
    poAPTWindsockLayer = NULL;
    poTaxiwaySignLayer = NULL;
    poVASI_PAPI_WIGWAG_Layer = NULL;
    
    Rewind();
}

/************************************************************************/
/*                         OGRXPlaneAptReader()                         */
/************************************************************************/

OGRXPlaneAptReader::OGRXPlaneAptReader( OGRXPlaneDataSource* poDataSource )
{
    poAPTLayer = new OGRXPlaneAPTLayer();
    poRunwayLayer = new OGRXPlaneRunwayLayer();
    poRunwayThresholdLayer = new OGRXPlaneRunwayThresholdLayer();
    poStopwayLayer = new OGRXPlaneStopwayLayer();
    poWaterRunwayLayer = new OGRXPlaneWaterRunwayLayer();
    poWaterRunwayThresholdLayer = new OGRXPlaneWaterRunwayThresholdLayer();
    poHelipadLayer = new OGRXPlaneHelipadLayer();
    poHelipadPolygonLayer = new OGRXPlaneHelipadPolygonLayer();
    poTaxiwayRectangleLayer = new OGRXPlaneTaxiwayRectangleLayer();
    poPavementLayer = new OGRXPlanePavementLayer();
    poAPTBoundaryLayer = new OGRXPlaneAPTBoundaryLayer();
    poAPTLinearFeatureLayer = new OGRXPlaneAPTLinearFeatureLayer();
    poATCFreqLayer = new OGRXPlaneATCFreqLayer();
    poStartupLocationLayer = new OGRXPlaneStartupLocationLayer();
    poAPTLightBeaconLayer = new OGRXPlaneAPTLightBeaconLayer();
    poAPTWindsockLayer = new OGRXPlaneAPTWindsockLayer();
    poTaxiwaySignLayer = new OGRXPlaneTaxiwaySignLayer();
    poVASI_PAPI_WIGWAG_Layer = new OGRXPlane_VASI_PAPI_WIGWAG_Layer();

    poDataSource->RegisterLayer(poAPTLayer);
    poDataSource->RegisterLayer(poRunwayLayer);
    poDataSource->RegisterLayer(poRunwayThresholdLayer);
    poDataSource->RegisterLayer(poStopwayLayer);
    poDataSource->RegisterLayer(poWaterRunwayLayer);
    poDataSource->RegisterLayer(poWaterRunwayThresholdLayer);
    poDataSource->RegisterLayer(poHelipadLayer);
    poDataSource->RegisterLayer(poHelipadPolygonLayer);
    poDataSource->RegisterLayer(poTaxiwayRectangleLayer);
    poDataSource->RegisterLayer(poPavementLayer);
    poDataSource->RegisterLayer(poAPTBoundaryLayer);
    poDataSource->RegisterLayer(poAPTLinearFeatureLayer);
    poDataSource->RegisterLayer(poATCFreqLayer);
    poDataSource->RegisterLayer(poStartupLocationLayer);
    poDataSource->RegisterLayer(poAPTLightBeaconLayer);
    poDataSource->RegisterLayer(poAPTWindsockLayer);
    poDataSource->RegisterLayer(poTaxiwaySignLayer);
    poDataSource->RegisterLayer(poVASI_PAPI_WIGWAG_Layer);
    
    Rewind();
}

/************************************************************************/
/*                        CloneForLayer()                               */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneAptReader::CloneForLayer(OGRXPlaneLayer* poLayer)
{
    OGRXPlaneAptReader* poReader = new OGRXPlaneAptReader();

    poReader->poInterestLayer = poLayer;
    SET_IF_INTEREST_LAYER(poAPTLayer);
    SET_IF_INTEREST_LAYER(poRunwayLayer);
    SET_IF_INTEREST_LAYER(poRunwayThresholdLayer);
    SET_IF_INTEREST_LAYER(poStopwayLayer);
    SET_IF_INTEREST_LAYER(poWaterRunwayLayer);
    SET_IF_INTEREST_LAYER(poWaterRunwayThresholdLayer);
    SET_IF_INTEREST_LAYER(poHelipadLayer);
    SET_IF_INTEREST_LAYER(poHelipadPolygonLayer);
    SET_IF_INTEREST_LAYER(poTaxiwayRectangleLayer);
    SET_IF_INTEREST_LAYER(poPavementLayer);
    SET_IF_INTEREST_LAYER(poAPTBoundaryLayer);
    SET_IF_INTEREST_LAYER(poAPTLinearFeatureLayer);
    SET_IF_INTEREST_LAYER(poATCFreqLayer);
    SET_IF_INTEREST_LAYER(poStartupLocationLayer);
    SET_IF_INTEREST_LAYER(poAPTLightBeaconLayer);
    SET_IF_INTEREST_LAYER(poAPTWindsockLayer);
    SET_IF_INTEREST_LAYER(poTaxiwaySignLayer);
    SET_IF_INTEREST_LAYER(poVASI_PAPI_WIGWAG_Layer);

    if (pszFilename)
    {
        poReader->pszFilename = CPLStrdup(pszFilename);
        poReader->fp = VSIFOpen( pszFilename, "rt" );
    }

    return poReader;
}

/************************************************************************/
/*                               Rewind()                               */
/************************************************************************/

void OGRXPlaneAptReader::Rewind()
{
    bAptHeaderFound = FALSE;
    bTowerFound = FALSE;
    dfLatTower = 0;
    dfLonTower = 0;
    dfHeightTower = 0;
    bRunwayFound = FALSE;
    dfLatFirstRwy = 0;
    dfLonFirstRwy = 0;
    nAPTType = -1;

    bResumeLine = FALSE;

    OGRXPlaneReader::Rewind();
}


/************************************************************************/
/*                      IsRecognizedVersion()                           */
/************************************************************************/

int OGRXPlaneAptReader::IsRecognizedVersion( const char* pszVersionString)
{
    return EQUALN(pszVersionString, "850 Version", 11) ||
           EQUALN(pszVersionString, "810 Version", 11);
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

void OGRXPlaneAptReader::Read()
{
    const char* pszLine;

    if (!bResumeLine)
    {
        CPLAssert(papszTokens == NULL);
    }

    while(bResumeLine || (pszLine = CPLReadLine(fp)) != NULL)
    {
        int nType;
        if (!bResumeLine)
        {
            papszTokens = CSLTokenizeString(pszLine);
            nTokens = CSLCount(papszTokens);
            nLineNumber ++;
            bResumeLine = FALSE;
        }

        do
        {
            bResumeLine = FALSE;

            if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
            {
                CSLDestroy(papszTokens);
                papszTokens = NULL;
                bEOF = TRUE;
                if (bAptHeaderFound)
                {
                    if (poAPTLayer)
                    {
                        poAPTLayer->AddFeature(osAptICAO, osAptName, nAPTType, dfElevation,
                                            bTowerFound || bRunwayFound,
                                            (bTowerFound) ? dfLatTower : dfLatFirstRwy,
                                            (bTowerFound) ? dfLonTower : dfLonFirstRwy, 
                                            bTowerFound, dfHeightTower, osTowerName);
                    }
                }
                return;
            }
            else if (nTokens == 0 || assertMinCol(2) == FALSE)
            {
                break;
            }

            nType = atoi(papszTokens[0]);
            switch(nType)
            {
                case APT_AIRPORT_HEADER:
                case APT_SEAPLANE_HEADER:
                case APT_HELIPORT_HEADER:
                    if (bAptHeaderFound)
                    {
                        bAptHeaderFound = FALSE;
                        if (poAPTLayer)
                        {
                            poAPTLayer->AddFeature(osAptICAO, osAptName, nAPTType, dfElevation,
                                                    bTowerFound || bRunwayFound,
                                                    (bTowerFound) ? dfLatTower : dfLatFirstRwy,
                                                    (bTowerFound) ? dfLonTower : dfLonFirstRwy, 
                                                    bTowerFound, dfHeightTower, osTowerName);
                        }
                    }
                    ParseAptHeaderRecord();
                    nAPTType = nType;

                    break;

                case APT_RUNWAY_TAXIWAY_V_810:
                    if (poAPTLayer ||
                        poRunwayLayer || poRunwayThresholdLayer ||
                        poStopwayLayer ||
                        poHelipadLayer || poHelipadPolygonLayer ||
                        poVASI_PAPI_WIGWAG_Layer || poTaxiwayRectangleLayer)
                    {
                        ParseRunwayTaxiwayV810Record();
                    }
                    break;

                case APT_TOWER:
                    if (poAPTLayer)
                        ParseTowerRecord();
                    break;

                case APT_STARTUP_LOCATION:
                    if (poStartupLocationLayer)
                        ParseStartupLocationRecord();
                    break;

                case APT_LIGHT_BEACONS:
                    if (poAPTLightBeaconLayer)
                        ParseLightBeaconRecord();
                    break;

                case APT_WINDSOCKS:
                    if (poAPTWindsockLayer)
                        ParseWindsockRecord();
                    break;

                case APT_TAXIWAY_SIGNS:
                    if (poTaxiwaySignLayer)
                        ParseTaxiwaySignRecord();
                    break;

                case APT_VASI_PAPI_WIGWAG:
                    if (poVASI_PAPI_WIGWAG_Layer)
                        ParseVasiPapiWigWagRecord();
                    break;

                case APT_ATC_AWOS_ASOS_ATIS:
                case APT_ATC_CTAF:
                case APT_ATC_CLD:
                case APT_ATC_GND:
                case APT_ATC_TWR:
                case APT_ATC_APP:
                case APT_ATC_DEP:
                    if (poATCFreqLayer)
                        ParseATCRecord(nType);
                    break;

                case APT_RUNWAY:
                    if (poAPTLayer || poRunwayLayer || poRunwayThresholdLayer || poStopwayLayer)
                        ParseRunwayRecord();
                    break;

                case APT_WATER_RUNWAY:
                    if (poWaterRunwayLayer || poWaterRunwayThresholdLayer)
                        ParseWaterRunwayRecord();
                    break;

                case APT_HELIPAD:
                    if (poHelipadLayer || poHelipadPolygonLayer)
                        ParseHelipadRecord();
                    break;

                case APT_PAVEMENT_HEADER:
                    if (poPavementLayer)
                        ParsePavement();
                    break;

                case APT_LINEAR_HEADER:
                    if (poAPTLinearFeatureLayer)
                        ParseAPTLinearFeature();
                    break;

                case APT_BOUNDARY_HEADER:
                    if (poAPTBoundaryLayer)
                        ParseAPTBoundary();
                    break;

                default:
                    break;
            }
        } while(bResumeLine);

        CSLDestroy(papszTokens);
        papszTokens = NULL;

        if (poInterestLayer && poInterestLayer->IsEmpty() == FALSE)
            return;
    }

    bEOF = TRUE;
}


/************************************************************************/
/*                         ParseAptHeaderRecord()                       */
/************************************************************************/

void    OGRXPlaneAptReader::ParseAptHeaderRecord()
{
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
    double adfDisplacedThresholdLength[2];
    double adfStopwayLength[2];
    const char* pszRwyNum;
    int aeVisualApproachLightingCode[2], aeRunwayLightingCode[2], aeApproachLightingCode[2];
    int eSurfaceCode, eShoulderCode, eMarkings;
    double dfSmoothness;
    double adfVisualGlidePathAngle[2];
    int bHasDistanceRemainingSigns;

    RET_IF_FAIL(assertMinCol(15));

    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
    pszRwyNum = papszTokens[3];
    RET_IF_FAIL(readTrueHeading(&dfTrueHeading, 4));
    RET_IF_FAIL(readDouble(&dfLength, 5, "length"));
    dfLength *= FEET_TO_METER;
    adfDisplacedThresholdLength[0] = atoi(papszTokens[6]) * FEET_TO_METER;
    if (strchr(papszTokens[6], '.') != NULL)
        adfDisplacedThresholdLength[1] = atoi(strchr(papszTokens[6], '.') + 1) * FEET_TO_METER;
    adfStopwayLength[0] = atoi(papszTokens[7]) * FEET_TO_METER;
    if (strchr(papszTokens[7], '.') != NULL)
        adfStopwayLength[1] = atoi(strchr(papszTokens[7], '.') + 1) * FEET_TO_METER;
    RET_IF_FAIL(readDouble(&dfWidth, 8, "width"));
    dfWidth *= FEET_TO_METER;
    if (strlen(papszTokens[9]) == 6)
    {
        aeVisualApproachLightingCode[0] = papszTokens[9][0] - '0';
        aeRunwayLightingCode[0] = papszTokens[9][1] - '0';
        aeApproachLightingCode[0] = papszTokens[9][2] - '0';
        aeVisualApproachLightingCode[1] = papszTokens[9][3] - '0';
        aeRunwayLightingCode[1] = papszTokens[9][4] - '0';
        aeApproachLightingCode[1] = papszTokens[9][5] - '0';
    }
    eSurfaceCode = atoi(papszTokens[10]);
    eShoulderCode = atoi(papszTokens[11]);
    eMarkings = atoi(papszTokens[12]);
    RET_IF_FAIL(readDoubleWithBounds(&dfSmoothness, 13, "runway smoothness", 0., 1.));
    bHasDistanceRemainingSigns = atoi(papszTokens[14]);
    if (nTokens == 16)
    {
        adfVisualGlidePathAngle[0] = atoi(papszTokens[15]) / 100.;
        if (strchr(papszTokens[15], '.') != NULL)
            adfVisualGlidePathAngle[1] = atoi(strchr(papszTokens[15], '.') + 1) / 100.;
    }

    if (strcmp(pszRwyNum, "xxx") == 000)
    {
        /* Taxiway */
        if (poTaxiwayRectangleLayer)
            poTaxiwayRectangleLayer->AddFeature(osAptICAO, dfLat, dfLon,
                                    dfTrueHeading, dfLength, dfWidth,
                                    RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                    dfSmoothness, aeRunwayLightingCode[0] == 1);
    }
    else if (pszRwyNum[0] >= '0' && pszRwyNum[0] <= '9' && strlen(pszRwyNum) >= 2)
    {
        /* Runway */
        double adfLat[2], adfLon[2];
        CPLString aosRwyNum[2];
        OGRFeature* poFeature;
        int abReil[2];

        int num1 = atoi(pszRwyNum);
        int num2 = (num1 > 18) ? num1 - 18 : num1 + 18;
        if (pszRwyNum[2] == '0' || pszRwyNum[2] == 'x')
        {
            aosRwyNum[0].Printf("%02d", num1);
            aosRwyNum[1].Printf("%02d", num2);
        }
        else
        {
            aosRwyNum[0] = pszRwyNum;
            aosRwyNum[1].Printf("%02d%c", num2,
                                (aosRwyNum[0][2] == 'L') ? 'R' :
                                (aosRwyNum[0][2] == 'R') ? 'L' : aosRwyNum[0][2]);
        }

        OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading + 180, &adfLat[0], &adfLon[0]);
        OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading, &adfLat[1], &adfLon[1]);

        for(int i=0;i<2;i++)
            abReil[i] = (aeRunwayLightingCode[i] >= 3 && aeRunwayLightingCode[i] <= 5) ;

        if (!bRunwayFound)
        {
            dfLatFirstRwy = adfLat[0];
            dfLonFirstRwy = adfLon[0];
            bRunwayFound = TRUE;
        }
        
        if (nAPTType == APT_SEAPLANE_HEADER || eSurfaceCode == 13)
        {
            /* Special case for water-runways. No special record in V8.10 */
            OGRFeature* apoWaterRunwayThreshold[2] = {NULL, NULL};
            int bBuoys;
            int i;

            bBuoys = TRUE;

            for(i=0;i<2;i++)
            {
                if (poWaterRunwayThresholdLayer)
                {
                    apoWaterRunwayThreshold[i] =
                        poWaterRunwayThresholdLayer->AddFeature  
                            (osAptICAO, aosRwyNum[i], adfLat[i], adfLon[i], dfWidth, bBuoys);
                }

            }

            if (poWaterRunwayThresholdLayer)
            {
                poWaterRunwayThresholdLayer->SetRunwayLengthAndHeading(apoWaterRunwayThreshold[0], dfLength,
                                            OGRXPlane_Track(adfLat[0], adfLon[0], adfLat[1], adfLon[1]));
                poWaterRunwayThresholdLayer->SetRunwayLengthAndHeading(apoWaterRunwayThreshold[1], dfLength,
                                            OGRXPlane_Track(adfLat[1], adfLon[1], adfLat[0], adfLon[0]));
            }

            if (poWaterRunwayLayer)
            {
                poWaterRunwayLayer->AddFeature(osAptICAO, aosRwyNum[0], aosRwyNum[1],
                                            adfLat[0], adfLon[0], adfLat[1], adfLon[1],
                                            dfWidth, bBuoys);
            }
        }
        else
        {
            if (poRunwayThresholdLayer)
            {
                for(int i=0;i<2;i++)
                {
                    poFeature =
                        poRunwayThresholdLayer->AddFeature  
                            (osAptICAO, aosRwyNum[i],
                            adfLat[i], adfLon[i], dfWidth,
                            RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                            RunwayShoulderEnumeration.GetText(eShoulderCode),
                            dfSmoothness,
                            (aeRunwayLightingCode[i] == 4 || aeRunwayLightingCode[i] == 5) /* bHasCenterLineLights */,
                            (aeRunwayLightingCode[i] >= 2 && aeRunwayLightingCode[i] <= 5) ? "Yes" : "None" /* pszEdgeLighting */,
                            bHasDistanceRemainingSigns,
                            adfDisplacedThresholdLength[i], adfStopwayLength[i],
                            RunwayMarkingEnumeration.GetText(eMarkings),
                            RunwayApproachLightingEnumerationV810.GetText(aeApproachLightingCode[i]),
                            (aeRunwayLightingCode[i] == 5) /* bHasTouchdownLights */,
                            (abReil[i] && abReil[i]) ? "Omni-directional" :
                            (abReil[i] && !abReil[1-i]) ? "Unidirectional" : "None" /* eReil */);
                    poRunwayThresholdLayer->SetRunwayLengthAndHeading(poFeature, dfLength,
                            (i == 0) ? dfTrueHeading : (dfTrueHeading < 180) ? dfTrueHeading + 180 : dfTrueHeading - 180);
                    if (adfDisplacedThresholdLength[i] != 0)
                        poRunwayThresholdLayer->AddFeatureFromNonDisplacedThreshold(poFeature);
                }
            }

            if (poRunwayLayer)
            {
                poRunwayLayer->AddFeature(osAptICAO, aosRwyNum[0], aosRwyNum[1],
                                        adfLat[0], adfLon[0], adfLat[1], adfLon[1],
                                        dfWidth,
                                        RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                        RunwayShoulderEnumeration.GetText(eShoulderCode),
                                        dfSmoothness,
                                        (aeRunwayLightingCode[0] == 4 || aeRunwayLightingCode[0] == 5),
                                        (aeRunwayLightingCode[0] >= 2 && aeRunwayLightingCode[0] <= 5) ? "Yes" : "None" /* pszEdgeLighting */,
                                        bHasDistanceRemainingSigns);
            }
            
            if (poStopwayLayer)
            {
                for(int i=0;i<2;i++)
                {
                    if (adfStopwayLength[i] != 0)
                    {
                        double dfHeading = OGRXPlane_Track(adfLat[i], adfLon[i],
                                                           adfLat[1-i], adfLon[1-i]);
                        poStopwayLayer->AddFeature(osAptICAO, aosRwyNum[i],
                            adfLat[i], adfLon[i], dfHeading, dfWidth, adfStopwayLength[i]);
                    }
                }
            }

            if (poVASI_PAPI_WIGWAG_Layer)
            {
                for(int i=0;i<2;i++)
                {
                    if (aeApproachLightingCode[i])
                        poVASI_PAPI_WIGWAG_Layer->AddFeature(osAptICAO, aosRwyNum[i],
                                RunwayVisualApproachPathIndicatorEnumerationV810.GetText(aeApproachLightingCode[i]),
                                adfLat[i], adfLon[i],
                                (i == 0) ? dfTrueHeading : (dfTrueHeading < 180) ? dfTrueHeading + 180 : dfTrueHeading- 180,
                                 adfVisualGlidePathAngle[i]);
                }
            }
        }
    }
    else if (pszRwyNum[0] == 'H')
    {
        /* Helipads can belong to regular airports or heliports */
        CPLString osHelipadName = pszRwyNum;
        if (strlen(pszRwyNum) == 3 && pszRwyNum[2] == 'x')
            osHelipadName[2] = 0;

        if (!bRunwayFound)
        {
            dfLatFirstRwy = dfLat;
            dfLonFirstRwy = dfLon;
            bRunwayFound = TRUE;
        }
        
        if (poHelipadLayer)
        {
            poHelipadLayer->AddFeature(osAptICAO, osHelipadName, dfLat, dfLon,
                                    dfTrueHeading, dfLength, dfWidth,
                                    RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                    RunwayMarkingEnumeration.GetText(eMarkings),
                                    RunwayShoulderEnumeration.GetText(eShoulderCode),
                                    dfSmoothness,
                                    (aeRunwayLightingCode[0] >= 2 && aeRunwayLightingCode[0] <= 5) ? "Yes" : "None" /* pszEdgeLighting */);
        }

        if (poHelipadPolygonLayer)
        {
            poHelipadPolygonLayer->AddFeature(osAptICAO, osHelipadName, dfLat, dfLon,
                                    dfTrueHeading, dfLength, dfWidth,
                                    RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                    RunwayMarkingEnumeration.GetText(eMarkings),
                                    RunwayShoulderEnumeration.GetText(eShoulderCode),
                                    dfSmoothness,
                                    (aeRunwayLightingCode[0] >= 2 && aeRunwayLightingCode[0] <= 5) ? "Yes" : "None" /* pszEdgeLighting */);
        }
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
    int bHasCenterLineLights, bHasDistanceRemainingSigns;
    int eEdgeLighting;
    int nCurToken;
    int nRwy = 0;
    double adfLat[2], adfLon[2];
    OGRFeature* apoRunwayThreshold[2] = { NULL, NULL };
    double dfLength;
    CPLString aosRunwayId[2];
    double adfDisplacedThresholdLength[2];
    double adfStopwayLength[2];

    RET_IF_FAIL(assertMinCol(8 + 9 + 9));

    RET_IF_FAIL(readDouble(&dfWidth, 1, "runway width"));
    eSurfaceCode = atoi(papszTokens[2]);
    eShoulderCode = atoi(papszTokens[3]);
    RET_IF_FAIL(readDoubleWithBounds(&dfSmoothness, 4, "runway smoothness", 0., 1.));
    bHasCenterLineLights = atoi(papszTokens[5]);
    eEdgeLighting = atoi(papszTokens[6]);
    bHasDistanceRemainingSigns = atoi(papszTokens[7]);

    for( nRwy=0, nCurToken = 8 ; nRwy<=1 ; nRwy++, nCurToken += 9 )
    {
        double dfLat, dfLon;
        int eMarkings, eApproachLightingCode, eREIL;
        int bHasTouchdownLights;

        aosRunwayId[nRwy] = papszTokens[nCurToken + 0]; /* for example : 08, 24R, or xxx */
        RET_IF_FAIL(readLatLon(&dfLat, &dfLon, nCurToken + 1));
        adfLat[nRwy] = dfLat; 
        adfLon[nRwy] = dfLon;
        RET_IF_FAIL(readDouble(&adfDisplacedThresholdLength[nRwy], nCurToken + 3, "displaced threshold length"));
        RET_IF_FAIL(readDouble(&adfStopwayLength[nRwy], nCurToken + 4, "stopway/blastpad/over-run length"));
        eMarkings = atoi(papszTokens[nCurToken + 5]);
        eApproachLightingCode = atoi(papszTokens[nCurToken + 6]);
        bHasTouchdownLights = atoi(papszTokens[nCurToken + 7]);
        eREIL = atoi(papszTokens[nCurToken + 8]);

        if (!bRunwayFound)
        {
            dfLatFirstRwy = dfLat;
            dfLonFirstRwy = dfLon;
            bRunwayFound = TRUE;
        }

        if (poRunwayThresholdLayer)
        {
            apoRunwayThreshold[nRwy] =
                poRunwayThresholdLayer->AddFeature  
                    (osAptICAO, aosRunwayId[nRwy],
                    dfLat, dfLon, dfWidth,
                    RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                    RunwayShoulderEnumeration.GetText(eShoulderCode),
                    dfSmoothness, bHasCenterLineLights,
                    RunwayEdgeLightingEnumeration.GetText(eEdgeLighting), bHasDistanceRemainingSigns,
                    adfDisplacedThresholdLength[nRwy], adfStopwayLength[nRwy],
                    RunwayMarkingEnumeration.GetText(eMarkings),
                    RunwayApproachLightingEnumeration.GetText(eApproachLightingCode),
                    bHasTouchdownLights,
                    RunwayREILEnumeration.GetText(eREIL));
        }
    }

    dfLength = OGRXPlane_Distance(adfLat[0], adfLon[0], adfLat[1], adfLon[1]);
    if (poRunwayThresholdLayer)
    {
        poRunwayThresholdLayer->SetRunwayLengthAndHeading(apoRunwayThreshold[0], dfLength,
                                    OGRXPlane_Track(adfLat[0], adfLon[0], adfLat[1], adfLon[1]));
        poRunwayThresholdLayer->SetRunwayLengthAndHeading(apoRunwayThreshold[1], dfLength,
                                    OGRXPlane_Track(adfLat[1], adfLon[1], adfLat[0], adfLon[0]));
        if (adfDisplacedThresholdLength[0] != 0)
            poRunwayThresholdLayer->AddFeatureFromNonDisplacedThreshold(apoRunwayThreshold[0]);
        if (adfDisplacedThresholdLength[1] != 0)
            poRunwayThresholdLayer->AddFeatureFromNonDisplacedThreshold(apoRunwayThreshold[1]);
    }

    if (poRunwayLayer)
    {
        poRunwayLayer->AddFeature(osAptICAO, aosRunwayId[0], aosRunwayId[1],
                                    adfLat[0], adfLon[0], adfLat[1], adfLon[1],
                                    dfWidth,
                                    RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                    RunwayShoulderEnumeration.GetText(eShoulderCode),
                                    dfSmoothness, bHasCenterLineLights,
                                    RunwayEdgeLightingEnumeration.GetText(eEdgeLighting),
                                    bHasDistanceRemainingSigns);
    }
            
    if (poStopwayLayer)
    {
        for(int i=0;i<2;i++)
        {
            if (adfStopwayLength[i] != 0)
            {
                double dfHeading = OGRXPlane_Track(adfLat[i], adfLon[i],
                                                   adfLat[1-i], adfLon[1-i]);
                poStopwayLayer->AddFeature(osAptICAO, aosRunwayId[i],
                    adfLat[i], adfLon[i], dfHeading, dfWidth, adfStopwayLength[i]);
            }
        }
    }
}

/************************************************************************/
/*                       ParseWaterRunwayRecord()                       */
/************************************************************************/

void OGRXPlaneAptReader::ParseWaterRunwayRecord()
{
    double adfLat[2], adfLon[2];
    OGRFeature* apoWaterRunwayThreshold[2] = {NULL, NULL};
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
        RET_IF_FAIL(readLatLon(&adfLat[i], &adfLon[i], 4 + 3*i));

        if (poWaterRunwayThresholdLayer)
        {
            apoWaterRunwayThreshold[i] =
                poWaterRunwayThresholdLayer->AddFeature  
                    (osAptICAO, aosRunwayId[i], adfLat[i], adfLon[i], dfWidth, bBuoys);
        }

    }

    dfLength = OGRXPlane_Distance(adfLat[0], adfLon[0], adfLat[1], adfLon[1]);

    if (poWaterRunwayThresholdLayer)
    {
        poWaterRunwayThresholdLayer->SetRunwayLengthAndHeading(apoWaterRunwayThreshold[0], dfLength,
                                    OGRXPlane_Track(adfLat[0], adfLon[0], adfLat[1], adfLon[1]));
        poWaterRunwayThresholdLayer->SetRunwayLengthAndHeading(apoWaterRunwayThreshold[1], dfLength,
                                    OGRXPlane_Track(adfLat[1], adfLon[1], adfLat[0], adfLon[0]));
    }

    if (poWaterRunwayLayer)
    {
        poWaterRunwayLayer->AddFeature(osAptICAO, aosRunwayId[0], aosRunwayId[1],
                                    adfLat[0], adfLon[0], adfLat[1], adfLon[1],
                                    dfWidth, bBuoys);
    }
}

/************************************************************************/
/*                       ParseHelipadRecord()                           */
/************************************************************************/

void OGRXPlaneAptReader::ParseHelipadRecord()
{
    double dfLat, dfLon, dfTrueHeading, dfLength, dfWidth, dfSmoothness;
    int eSurfaceCode, eMarkings, eShoulderCode, eEdgeLighting;
    const char* pszHelipadName;

    RET_IF_FAIL(assertMinCol(12));

    pszHelipadName = papszTokens[1];
    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 2));
    RET_IF_FAIL(readTrueHeading(&dfTrueHeading, 4));
    RET_IF_FAIL(readDouble(&dfLength, 5, "length"));
    RET_IF_FAIL(readDouble(&dfWidth, 6, "width"));
    eSurfaceCode = atoi(papszTokens[7]);
    eMarkings = atoi(papszTokens[8]);
    eShoulderCode = atoi(papszTokens[9]);
    RET_IF_FAIL(readDoubleWithBounds(&dfSmoothness, 10, "helipad smoothness", 0., 1.));
    eEdgeLighting = atoi(papszTokens[11]);

    if (poHelipadLayer)
    {
        poHelipadLayer->AddFeature(osAptICAO, pszHelipadName, dfLat, dfLon,
                                dfTrueHeading, dfLength, dfWidth,
                                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                RunwayMarkingEnumeration.GetText(eMarkings),
                                RunwayShoulderEnumeration.GetText(eShoulderCode),
                                dfSmoothness,
                                HelipadEdgeLightingEnumeration.GetText(eEdgeLighting));
    }

    if (poHelipadPolygonLayer)
    {
        poHelipadPolygonLayer->AddFeature(osAptICAO, pszHelipadName, dfLat, dfLon,
                                        dfTrueHeading, dfLength, dfWidth,
                                        RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                        RunwayMarkingEnumeration.GetText(eMarkings),
                                        RunwayShoulderEnumeration.GetText(eShoulderCode),
                                        dfSmoothness,
                                        HelipadEdgeLightingEnumeration.GetText(eEdgeLighting));
    }
}



/************************************************************************/
/*                          AddBezierCurve()                           */
/************************************************************************/

#define CUBIC_INTERPOL(A, B, C, D)  ((A)*(b*b*b) + 3*(B)*(b*b)*a + 3 *(C)*b*(a*a) + (D)*(a*a*a))

void OGRXPlaneAptReader::AddBezierCurve(OGRLineString& lineString,
                                        double dfLatA, double dfLonA,
                                        double dfCtrPtLatA, double dfCtrPtLonA,
                                        double dfSymCtrlPtLatB, double dfSymCtrlPtLonB,
                                        double dfLatB, double dfLonB)
{
    int step;
    for(step = 0; step <= 10; step++)
    {
        double a = step / 10.;
        double b = 1. - a;
        double dfCtrlPtLonB = dfLonB - (dfSymCtrlPtLonB - dfLonB);
        double dfCtrlPtLatB = dfLatB - (dfSymCtrlPtLatB - dfLatB);
        lineString.addPoint(CUBIC_INTERPOL(dfLonA, dfCtrPtLonA, dfCtrlPtLonB, dfLonB),
                            CUBIC_INTERPOL(dfLatA, dfCtrPtLatA, dfCtrlPtLatB, dfLatB));
    }
}


#define QUADRATIC_INTERPOL(A, B, C)  ((A)*(b*b) + 2*(B)*b*a + (C)*(a*a))

void OGRXPlaneAptReader::AddBezierCurve(OGRLineString& lineString,
                                        double dfLatA, double dfLonA,
                                        double dfCtrPtLat, double dfCtrPtLon,
                                        double dfLatB, double dfLonB)
{
    int step;
    for(step = 0; step <= 10; step++)
    {
        double a = step / 10.;
        double b = 1. - a;
        lineString.addPoint(QUADRATIC_INTERPOL(dfLonA, dfCtrPtLon, dfLonB),
                            QUADRATIC_INTERPOL(dfLatA, dfCtrPtLat, dfLatB));
    }
}

static OGRGeometry* OGRXPlaneAptReaderSplitPolygon(OGRPolygon& polygon)
{
    OGRPolygon** papoPolygons = new OGRPolygon* [1 + polygon.getNumInteriorRings()];

    papoPolygons[0] = new OGRPolygon();
    papoPolygons[0]->addRing(polygon.getExteriorRing());
    for(int i=0;i<polygon.getNumInteriorRings();i++)
    {
        papoPolygons[i+1] = new OGRPolygon();
        papoPolygons[i+1]->addRing(polygon.getInteriorRing(i));
    }

    int bIsValid;
    OGRGeometry* poGeom;
    poGeom = OGRGeometryFactory::organizePolygons((OGRGeometry**)papoPolygons,
                                                  1 + polygon.getNumInteriorRings(),
                                                  &bIsValid, NULL);

    delete[] papoPolygons;

    return poGeom;
}

/************************************************************************/
/*                           FixPolygonTopology()                       */
/************************************************************************/

/*
Intended to fix several topological problems, like when a point of an interior ring
is on the edge of the external ring, or other topological anomalies.
*/

OGRGeometry* OGRXPlaneAptReader::FixPolygonTopology(OGRPolygon& polygon)
{
    OGRLinearRing* poExternalRing = polygon.getExteriorRing();
    if (poExternalRing->getNumPoints() < 4)
    {
        CPLDebug("XPLANE", "Discarded degenerated polygon at line %d", nLineNumber);
        return NULL;
    }
        
    for(int i=0;i<polygon.getNumInteriorRings();i++)
    {
        OGRLinearRing* poInternalRing = polygon.getInteriorRing(i);
        if (poInternalRing->getNumPoints() < 4)
        {
            CPLDebug("XPLANE", "Discarded degenerated interior ring (%d) at line %d", i, nLineNumber);
            OGRPolygon polygon2;
            polygon2.addRing(poExternalRing);
            for(int j=0;j<polygon.getNumInteriorRings();j++)
            {
                if (i != j)
                    polygon2.addRing(polygon.getInteriorRing(j));
            }
            polygon = * (OGRPolygon*) (polygon2.clone());
            i --;
            continue;
        }
        
        int nOutside = 0;
        int jOutside = -1;
        for(int j=0;j<poInternalRing->getNumPoints();j++)
        {
            OGRPoint pt;
            poInternalRing->getPoint(j, &pt);
            if (poExternalRing->isPointInRing(&pt) == FALSE)
            {
                nOutside++;
                jOutside = j;
            }
        }

        if (nOutside == 1)
        {
            int j = jOutside;
            OGRPoint pt;
            poInternalRing->getPoint(j, &pt);
            OGRPoint newPt;
            int bSuccess = FALSE;
            for(int k=-1;k<=1 && !bSuccess;k+=2)
            {
                for(int l=-1;l<=1 && !bSuccess;l+=2)
                {
                    newPt.setX(pt.getX() + k * 1e-7);
                    newPt.setY(pt.getY() + l * 1e-7);
                    if (poExternalRing->isPointInRing(&newPt))
                    {
                        poInternalRing->setPoint(j, newPt.getX(), newPt.getY());
                        bSuccess = TRUE;
                    }
                }
            }
            if (!bSuccess)
            {
                CPLDebug("XPLANE",
                            "Didn't manage to fix polygon topology at line %d", nLineNumber);

                /* Invalid topology. Will split into several pieces */
                return OGRXPlaneAptReaderSplitPolygon(polygon);
            }
        }
        else
        {
            /* Two parts. Or other strange cases */
            return OGRXPlaneAptReaderSplitPolygon(polygon);
        }
    }

    /* The geometry is right */
    return polygon.clone();
}

/************************************************************************/
/*                         ParsePolygonalGeometry()                     */
/************************************************************************/

/* This function will eat records until the end of the polygon */
/* Return TRUE if the main parser must re-scan the current record */

#define RET_FALSE_IF_FAIL(x)      if (!(x)) return FALSE;

int OGRXPlaneAptReader::ParsePolygonalGeometry(OGRGeometry** ppoGeom)
{
    double dfLat, dfLon;
    double dfFirstLat = 0., dfFirstLon = 0.;
    double dfLastLat = 0., dfLastLon = 0.;
    double dfLatBezier, dfLonBezier;
    double dfFirstLatBezier = 0., dfFirstLonBezier = 0.;
    double dfLastLatBezier = 0., dfLastLonBezier = 0.;
    int bIsFirst = TRUE;
    int bFirstIsBezier = TRUE;
    int bLastIsValid = FALSE;
    int bLastIsBezier = FALSE;
    int bLastPartIsClosed = FALSE;
    const char* pszLine;
    OGRPolygon polygon;

    OGRLinearRing linearRing;

    *ppoGeom = NULL;

    while((pszLine = CPLReadLine(fp)) != NULL)
    {
        int nType = -1;
        papszTokens = CSLTokenizeString(pszLine);
        nTokens = CSLCount(papszTokens);

        nLineNumber ++;

        if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
        {
            if (bLastPartIsClosed == FALSE)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a polygon : %d",
                        nLineNumber, nType);
            }
            else
            {
                *ppoGeom = FixPolygonTopology(polygon);
            }

            return TRUE;
        }
        if (nTokens == 0 || assertMinCol(2) == FALSE)
        {
            CSLDestroy(papszTokens);
            continue;
        }

        nType = atoi(papszTokens[0]);
        if (nType == APT_NODE)
        {
            RET_FALSE_IF_FAIL(assertMinCol(3));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));

            if (bLastIsBezier && !bIsFirst &&
                !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                AddBezierCurve(linearRing,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLat, dfLon);
            }
            else
                linearRing.addPoint(dfLon, dfLat);

            bLastPartIsClosed = FALSE;
            bLastIsBezier = FALSE;
        }
        else if (nType == APT_NODE_WITH_BEZIER)
        {
            RET_FALSE_IF_FAIL(assertMinCol(5));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
            RET_FALSE_IF_FAIL(readLatLon(&dfLatBezier, &dfLonBezier, 3));

            if (bLastIsBezier)
            {
                AddBezierCurve(linearRing,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLatBezier, dfLonBezier,
                               dfLat, dfLon);
            }
            else if (!bIsFirst && !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                double dfCtrLatBezier = dfLat - (dfLatBezier - dfLat);
                double dfCtrLonBezier = dfLon - (dfLonBezier - dfLon);
                AddBezierCurve(linearRing,
                               dfLastLat, dfLastLon,
                               dfCtrLatBezier, dfCtrLonBezier,
                               dfLat, dfLon);
            }

            bLastPartIsClosed = FALSE;
            bLastIsBezier = TRUE;
            dfLastLatBezier = dfLatBezier;
            dfLastLonBezier = dfLonBezier;
        }
        else if (nType == APT_NODE_CLOSE)
        {
            RET_FALSE_IF_FAIL(assertMinCol(3));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
            if (bIsFirst)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a polygon : %d",
                        nLineNumber, nType);
                return TRUE;
            }

            if (bLastIsBezier && !bIsFirst &&
                !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                AddBezierCurve(linearRing,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLat, dfLon);
            }
            else
                linearRing.addPoint(dfLon, dfLat);

            linearRing.closeRings();

            polygon.addRing(&linearRing);
            linearRing.empty();

            bLastPartIsClosed = TRUE;
            bLastIsBezier = FALSE;
        }
        else if (nType == APT_NODE_CLOSE_WITH_BEZIER)
        {
            RET_FALSE_IF_FAIL(assertMinCol(5));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
            RET_FALSE_IF_FAIL(readLatLon(&dfLatBezier, &dfLonBezier, 3));
            if (bIsFirst)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a polygon : %d",
                        nLineNumber, nType);
                return TRUE;
            }

            if (bLastIsBezier)
            {
                AddBezierCurve(linearRing,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLatBezier, dfLonBezier,
                               dfLat, dfLon);
            }
            else if (!bIsFirst && !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                double dfCtrLatBezier = dfLat - (dfLatBezier - dfLat);
                double dfCtrLonBezier = dfLon - (dfLonBezier - dfLon);
                AddBezierCurve(linearRing,
                               dfLastLat, dfLastLon,
                               dfCtrLatBezier, dfCtrLonBezier,
                               dfLat, dfLon);
            }
            else
            {
                linearRing.addPoint(dfLon, dfLat);
            }

            if (bFirstIsBezier)
            {
                AddBezierCurve(linearRing,
                               dfLat, dfLon,
                               dfLatBezier, dfLonBezier,
                               dfFirstLatBezier, dfFirstLonBezier,
                               dfFirstLat, dfFirstLon);
            }
            else
            {
                linearRing.closeRings();
            }

            polygon.addRing(&linearRing);
            linearRing.empty();

            bLastPartIsClosed = TRUE;
            bLastIsBezier = FALSE; /* we don't want to draw an arc between two parts */
        }
        else
        {
            if (nType == APT_NODE_END || nType == APT_NODE_END_WITH_BEZIER ||
                bLastPartIsClosed == FALSE)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a polygon : %d",
                        nLineNumber, nType);
            }
            else
            {
                *ppoGeom = FixPolygonTopology(polygon);
            }

            return TRUE;
        }

        if (bIsFirst)
        {
            dfFirstLat = dfLat;
            dfFirstLon = dfLon;
            dfFirstLatBezier = dfLatBezier;
            dfFirstLonBezier = dfLonBezier;
            bFirstIsBezier = bLastIsBezier;
        }
        bIsFirst = bLastPartIsClosed;

        dfLastLat = dfLat;
        dfLastLon = dfLon;
        bLastIsValid = TRUE;

        CSLDestroy(papszTokens);
    }

    CPLAssert(0);

    papszTokens = NULL;

    return FALSE;
}


/************************************************************************/
/*                            ParsePavement()                           */
/************************************************************************/

void OGRXPlaneAptReader::ParsePavement()
{
    int eSurfaceCode;
    double dfSmoothness, dfTextureHeading;
    CPLString osPavementName;

    RET_IF_FAIL(assertMinCol(4));

    eSurfaceCode = atoi(papszTokens[1]);

    RET_IF_FAIL(readDoubleWithBounds(&dfSmoothness, 2, "pavement smoothness", 0., 1.));

    RET_IF_FAIL(readTrueHeading(&dfTextureHeading, 3, "texture heading"));

    osPavementName = readStringUntilEnd(4);

    CSLDestroy(papszTokens);
    papszTokens = NULL;

    OGRGeometry* poGeom;
    bResumeLine = ParsePolygonalGeometry(&poGeom);
    if (poGeom != NULL && poPavementLayer)
    {
        if (poGeom->getGeometryType() == wkbPolygon)
        {
            poPavementLayer->AddFeature(osAptICAO, osPavementName,
                                        RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                        dfSmoothness, dfTextureHeading,
                                        (OGRPolygon*)poGeom);
        }
        else
        {
            OGRGeometryCollection* poGeomCollection = (OGRGeometryCollection*)poGeom;
            for(int i=0;i<poGeomCollection->getNumGeometries();i++)
            {
                OGRGeometry* poSubGeom = poGeomCollection->getGeometryRef(i);
                if (poSubGeom->getGeometryType() == wkbPolygon &&
                    ((OGRPolygon*)poSubGeom)->getExteriorRing()->getNumPoints() >= 4)
                {
                    poPavementLayer->AddFeature(osAptICAO, osPavementName,
                                                RunwaySurfaceEnumeration.GetText(eSurfaceCode),
                                                dfSmoothness, dfTextureHeading,
                                                (OGRPolygon*)poSubGeom);
                }
            }
        }
    }
    if (poGeom != NULL)
        delete poGeom;
}

/************************************************************************/
/*                         ParseAPTBoundary()                           */
/************************************************************************/

/* This function will eat records until the end of the boundary definition */
/* Return TRUE if the main parser must re-scan the current record */

void OGRXPlaneAptReader::ParseAPTBoundary()
{
    CPLString osBoundaryName;

    RET_IF_FAIL(assertMinCol(2));

    osBoundaryName = readStringUntilEnd(2);

    CSLDestroy(papszTokens);
    papszTokens = NULL;

    OGRGeometry* poGeom;
    bResumeLine = ParsePolygonalGeometry(&poGeom);
    if (poGeom != NULL && poAPTBoundaryLayer)
    {
        if (poGeom->getGeometryType() == wkbPolygon)
        {
             poAPTBoundaryLayer->AddFeature(osAptICAO, osBoundaryName,
                                        (OGRPolygon*)poGeom);
        }
        else
        {
            OGRGeometryCollection* poGeomCollection = (OGRGeometryCollection*)poGeom;
            for(int i=0;i<poGeomCollection->getNumGeometries();i++)
            {
                OGRGeometry* poSubGeom = poGeomCollection->getGeometryRef(i);
                if (poSubGeom->getGeometryType() == wkbPolygon &&
                    ((OGRPolygon*)poSubGeom)->getExteriorRing()->getNumPoints() >= 4)
                {
                    poAPTBoundaryLayer->AddFeature(osAptICAO, osBoundaryName,
                                            (OGRPolygon*)poSubGeom);
                }
            }
        }
    }
    if (poGeom != NULL)
        delete poGeom;
}


/************************************************************************/
/*                             ParseLinearGeometry()                    */
/************************************************************************/

/* This function will eat records until the end of the multilinestring */
/* Return TRUE if the main parser must re-scan the current record */

int OGRXPlaneAptReader::ParseLinearGeometry(OGRMultiLineString& multilinestring, int* pbIsValid)
{
    double dfLat, dfLon;
    double dfFirstLat = 0., dfFirstLon = 0.;
    double dfLastLat = 0., dfLastLon = 0.;
    double dfLatBezier, dfLonBezier;
    double dfFirstLatBezier = 0., dfFirstLonBezier = 0.;
    double dfLastLatBezier = 0., dfLastLonBezier = 0.;
    int bIsFirst = TRUE;
    int bFirstIsBezier = TRUE;
    int bLastIsValid = FALSE;
    int bLastIsBezier = FALSE;
    int bLastPartIsClosedOrEnded = FALSE;
    const char* pszLine;

    OGRLineString lineString;

    while((pszLine = CPLReadLine(fp)) != NULL)
    {
        int nType = -1;
        papszTokens = CSLTokenizeString(pszLine);
        nTokens = CSLCount(papszTokens);

        nLineNumber ++;

        if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
        {
            if (bLastPartIsClosedOrEnded == FALSE)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a linear feature : %d",
                        nLineNumber, nType);
            }
            else if (multilinestring.getNumGeometries() == 0)
            {
                CPLDebug("XPlane", "Line %d : Linear geometry is invalid or empty",
                         nLineNumber);
            }
            else
            {
                *pbIsValid = TRUE;
            }
            return TRUE;
        }
        if (nTokens == 0 || assertMinCol(2) == FALSE)
        {
            CSLDestroy(papszTokens);
            continue;
        }

        nType = atoi(papszTokens[0]);
        if (nType == APT_NODE)
        {
            RET_FALSE_IF_FAIL(assertMinCol(3));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));

            if (bLastIsBezier && !bIsFirst &&
                !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                AddBezierCurve(lineString,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLat, dfLon);
            }
            else
                lineString.addPoint(dfLon, dfLat);

            bLastPartIsClosedOrEnded = FALSE;
            bLastIsBezier = FALSE;
        }
        else if (nType == APT_NODE_WITH_BEZIER)
        {
            RET_FALSE_IF_FAIL(assertMinCol(5));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
            RET_FALSE_IF_FAIL(readLatLon(&dfLatBezier, &dfLonBezier, 3));

            if (bLastIsBezier)
            {
                AddBezierCurve(lineString,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLatBezier, dfLonBezier,
                               dfLat, dfLon);
            }
            else if (!bIsFirst && !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                double dfCtrLatBezier = dfLat - (dfLatBezier - dfLat);
                double dfCtrLonBezier = dfLon - (dfLonBezier - dfLon);
                AddBezierCurve(lineString,
                               dfLastLat, dfLastLon,
                               dfCtrLatBezier, dfCtrLonBezier,
                               dfLat, dfLon);
            }

            bLastPartIsClosedOrEnded = FALSE;
            bLastIsBezier = TRUE;
            dfLastLatBezier = dfLatBezier;
            dfLastLonBezier = dfLonBezier;
        }
        else if (nType == APT_NODE_CLOSE || nType == APT_NODE_END)
        {
            RET_FALSE_IF_FAIL(assertMinCol(3));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
            if (bIsFirst)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a linear feature : %d",
                        nLineNumber, nType);
                return TRUE;
            }

            if (bLastIsBezier && !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                AddBezierCurve(lineString,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLat, dfLon);
            }
            else
                lineString.addPoint(dfLon, dfLat);

            if (nType == APT_NODE_CLOSE )
                lineString.closeRings();

            if (lineString.getNumPoints() < 2)
            {
                CPLDebug("XPlane", "Line %d : A linestring has less than 2 points",
                         nLineNumber);
            }
            else
            {
                multilinestring.addGeometry(&lineString);
            }
            lineString.empty();

            bLastPartIsClosedOrEnded = TRUE;
            bLastIsBezier = FALSE;
        }
        else if (nType == APT_NODE_CLOSE_WITH_BEZIER || nType == APT_NODE_END_WITH_BEZIER)
        {
            RET_FALSE_IF_FAIL(assertMinCol(5));
            RET_FALSE_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
            RET_FALSE_IF_FAIL(readLatLon(&dfLatBezier, &dfLonBezier, 3));
            if (bIsFirst)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a linear feature : %d",
                        nLineNumber, nType);
                return TRUE;
            }

            if (bLastIsBezier)
            {
                AddBezierCurve(lineString,
                               dfLastLat, dfLastLon,
                               dfLastLatBezier, dfLastLonBezier,
                               dfLatBezier, dfLonBezier,
                               dfLat, dfLon);
            }
            else if (!bIsFirst && !(dfLastLat == dfLat && dfLastLon == dfLon))
            {
                double dfCtrLatBezier = dfLat - (dfLatBezier - dfLat);
                double dfCtrLonBezier = dfLon - (dfLonBezier - dfLon);
                AddBezierCurve(lineString,
                               dfLastLat, dfLastLon,
                               dfCtrLatBezier, dfCtrLonBezier,
                               dfLat, dfLon);
            }
            else
            {
                lineString.addPoint(dfLon, dfLat);
            }

            if (nType == APT_NODE_CLOSE_WITH_BEZIER)
            {
                if (bFirstIsBezier)
                {
                    AddBezierCurve(lineString,
                                dfLat, dfLon,
                                dfLatBezier, dfLonBezier,
                                dfFirstLatBezier, dfFirstLonBezier,
                                dfFirstLat, dfFirstLon);
                }
                else
                {
                    lineString.closeRings();
                }
            }

            if (lineString.getNumPoints() < 2)
            {
                CPLDebug("XPlane", "Line %d : A linestring has less than 2 points",
                         nLineNumber);
            }
            else
            {
                multilinestring.addGeometry(&lineString);
            }
            lineString.empty();

            bLastPartIsClosedOrEnded = TRUE;
            bLastIsBezier = FALSE; /* we don't want to draw an arc between two parts */
        }
        else
        {
            if (bLastPartIsClosedOrEnded == FALSE)
            {
                CPLDebug("XPlane", "Line %d : Unexpected token when reading a linear feature : %d",
                        nLineNumber, nType);
            }
            else if (multilinestring.getNumGeometries() == 0)
            {
                CPLDebug("XPlane", "Line %d : Linear geometry is invalid or empty",
                         nLineNumber);
            }
            else
            {
                *pbIsValid = TRUE;
            }
            return TRUE;
        }

        if (bIsFirst)
        {
            dfFirstLat = dfLat;
            dfFirstLon = dfLon;
            dfFirstLatBezier = dfLatBezier;
            dfFirstLonBezier = dfLonBezier;
            bFirstIsBezier = bLastIsBezier;
        }
        bIsFirst = bLastPartIsClosedOrEnded;

        dfLastLat = dfLat;
        dfLastLon = dfLon;
        bLastIsValid = TRUE;

        CSLDestroy(papszTokens);
    }

    CPLAssert(0);

    papszTokens = NULL;

    return FALSE;
}

/************************************************************************/
/*                      ParseAPTLinearFeature()                         */
/************************************************************************/

/* This function will eat records until the end of the linear feature definition */
/* Return TRUE if the main parser must re-scan the current record */

void OGRXPlaneAptReader::ParseAPTLinearFeature()
{
    CPLString osLinearFeatureName;

    RET_IF_FAIL(assertMinCol(2));

    osLinearFeatureName = readStringUntilEnd(2);

    CSLDestroy(papszTokens);
    papszTokens = NULL;

    OGRMultiLineString multilinestring;
    int bIsValid = FALSE;
    bResumeLine = ParseLinearGeometry(multilinestring, &bIsValid);
    if (bIsValid && poAPTLinearFeatureLayer)
    {
        poAPTLinearFeatureLayer->AddFeature(osAptICAO, osLinearFeatureName,
                                            &multilinestring);
    }
}

/************************************************************************/
/*                         ParseTowerRecord()                           */
/************************************************************************/

void OGRXPlaneAptReader::ParseTowerRecord()
{
    RET_IF_FAIL(assertMinCol(6));

    RET_IF_FAIL(readLatLon(&dfLatTower, &dfLonTower, 1));

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

    if (poATCFreqLayer)
    {
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
}


/************************************************************************/
/*                      ParseStartupLocationRecord()                    */
/************************************************************************/

void OGRXPlaneAptReader::ParseStartupLocationRecord()
{
    double dfLat, dfLon, dfTrueHeading;
    CPLString osName;

    RET_IF_FAIL(assertMinCol(4));

    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));

    RET_IF_FAIL(readTrueHeading(&dfTrueHeading, 3));

    osName = readStringUntilEnd(4);

    if (poStartupLocationLayer)
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
    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
    eColor = atoi(papszTokens[3]);
    osName = readStringUntilEnd(4);

    if (poAPTLightBeaconLayer)
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

    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
    bIsIllumnited = atoi(papszTokens[3]);
    osName = readStringUntilEnd(4);

    if (poAPTWindsockLayer)
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
    CPLString osText;

    RET_IF_FAIL(assertMinCol(7));

    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
    RET_IF_FAIL(readTrueHeading(&dfTrueHeading, 3, "heading"));
    /* papszTokens[4] : ignored. Taxiway sign style */
    nSize = atoi(papszTokens[5]);
    osText = readStringUntilEnd(6);

    if (poTaxiwaySignLayer)
        poTaxiwaySignLayer->AddFeature(osAptICAO, osText, dfLat, dfLon,
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

    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));
    eType = atoi(papszTokens[3]);
    RET_IF_FAIL(readTrueHeading(&dfTrueHeading, 4, "heading"));
    RET_IF_FAIL(readDoubleWithBounds(&dfVisualGlidePathAngle, 5, "visual glidepath angle", 0, 90));
    pszRwyNum = papszTokens[6];
    /* papszTokens[7] : ignored. Type of lighting object represented */

    if (poVASI_PAPI_WIGWAG_Layer)
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

    OGRFieldDefn oType("type", OFTInteger );
    oType.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oType );
    
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
                                   int nAPTType,
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
    poFeature->SetField( nCount++, (nAPTType == APT_AIRPORT_HEADER)    ? 0 :
                                   (nAPTType == APT_SEAPLANE_HEADER)   ? 1 :
                                 /*(nAPTType == APT_HELIPORT_HEADER)*/   2 );
    poFeature->SetField( nCount++, dfElevation );
    poFeature->SetField( nCount++, bHasTower );
    if (bHasCoordinates)
    {
        poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    }
    else
    {
        CPLDebug("XPlane", "Airport %s/%s has no coordinates", pszAptICAO, pszAptName);
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

    OGRFieldDefn oFieldEdgeLigthing("edge_lighting", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldEdgeLigthing );

    OGRFieldDefn oFieldDistanceRemainingSigns("distance_remaining_signs", OFTInteger );
    oFieldDistanceRemainingSigns.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldDistanceRemainingSigns );

    OGRFieldDefn oFieldDisplacedThreshold("displaced_threshold_m", OFTReal );
    oFieldDisplacedThreshold.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldDisplacedThreshold );

    OGRFieldDefn oFieldIsDisplaced("is_displaced", OFTInteger );
    oFieldIsDisplaced.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldIsDisplaced );

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
                                        const char* pszEdgeLighting,
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
    poFeature->SetField( nCount++, pszEdgeLighting );
    poFeature->SetField( nCount++, bHasDistanceRemainingSigns );
    poFeature->SetField( nCount++, dfDisplacedThresholdLength );
    poFeature->SetField( nCount++, FALSE);
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
    int nCount = 16;
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfHeading );
}

/************************************************************************/
/*             AddFeatureFromNonDisplacedThreshold()                    */
/************************************************************************/

OGRFeature* OGRXPlaneRunwayThresholdLayer::
        AddFeatureFromNonDisplacedThreshold(OGRFeature* poNonDisplacedThresholdFeature)
{
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    poFeature->SetFrom(poNonDisplacedThresholdFeature, FALSE);

    double dfDisplacedThresholdLength = poFeature->GetFieldAsDouble("displaced_threshold_m");
    double dfTrueHeading = poFeature->GetFieldAsDouble("true_heading_deg");
    poFeature->SetField("is_displaced", TRUE);
    OGRPoint* poPoint = (OGRPoint*)poFeature->GetGeometryRef();
    double dfLatDisplaced, dfLonDisplaced;
    OGRXPlane_ExtendPosition(poPoint->getY(), poPoint->getX(),
                             dfDisplacedThresholdLength, dfTrueHeading,
                             &dfLatDisplaced, &dfLonDisplaced);
    poPoint->setX(dfLonDisplaced);
    poPoint->setY(dfLatDisplaced);

    RegisterFeature(poFeature);

    return poFeature;
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

    OGRFieldDefn oFieldEdgeLigthing("edge_lighting", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldEdgeLigthing );

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
                                        const char* pszEdgeLighting,
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
    poFeature->SetField( nCount++, pszEdgeLighting );
    poFeature->SetField( nCount++, bHasDistanceRemainingSigns );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfTrack12 );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                      OGRXPlaneStopwayLayer()                         */
/************************************************************************/



OGRXPlaneStopwayLayer::OGRXPlaneStopwayLayer() : OGRXPlaneLayer("Stopway")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum1("rwy_num", OFTString );
    oFieldRwyNum1.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum1 );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/


OGRFeature*
     OGRXPlaneStopwayLayer::AddFeature(const char* pszAptICAO,
                                       const char* pszRwyNum,
                                       double dfLatThreshold,
                                       double dfLonThreshold,
                                       double dfRunwayHeading,
                                       double dfWidth,
                                       double dfStopwayLength)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfLat2, dfLon2;
    double adfLat[4], adfLon[4];
    
    OGRXPlane_ExtendPosition(dfLatThreshold, dfLonThreshold, dfStopwayLength, 180 + dfRunwayHeading, &dfLat2, &dfLon2);
    
    OGRXPlane_ExtendPosition(dfLatThreshold, dfLonThreshold, dfWidth / 2, dfRunwayHeading - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfRunwayHeading - 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfRunwayHeading + 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfLatThreshold, dfLonThreshold, dfWidth / 2, dfRunwayHeading + 90, &adfLat[3], &adfLon[3]);
    
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
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, dfStopwayLength );

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

    OGRFieldDefn oFieldEdgeLighting("edge_lighting", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldEdgeLighting );

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
                                        const char* pszEdgeLighting)
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
    poFeature->SetField( nCount++, pszEdgeLighting );

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

    OGRFieldDefn oFieldEdgeLighting("edge_lighting", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldEdgeLighting );

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
                                               const char* pszEdgeLighting)
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
    poFeature->SetField( nCount++, pszEdgeLighting );

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
/*                      OGRXPlanePavementLayer()                        */
/************************************************************************/


OGRXPlanePavementLayer::OGRXPlanePavementLayer() : OGRXPlaneLayer("Pavement")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldTextureHeading("texture_heading", OFTReal );
    oFieldTextureHeading.SetWidth( 6 );
    oFieldTextureHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTextureHeading );

}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlanePavementLayer::AddFeature(const char* pszAptICAO,
                                        const char* pszPavementName,
                                        const char* pszSurfaceType,
                                        double dfSmoothness,
                                        double dfTextureHeading,
                                        OGRPolygon* poPolygon)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    poFeature->SetGeometry( poPolygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszPavementName );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, dfTextureHeading );

    RegisterFeature(poFeature);

    return poFeature;
}



/************************************************************************/
/*                   OGRXPlaneAPTBoundaryLayer()                        */
/************************************************************************/


OGRXPlaneAPTBoundaryLayer::OGRXPlaneAPTBoundaryLayer() : OGRXPlaneLayer("APTBoundary")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneAPTBoundaryLayer::AddFeature(const char* pszAptICAO,
                                           const char* pszBoundaryName,
                                           OGRPolygon* poPolygon)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    poFeature->SetGeometry( poPolygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszBoundaryName );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*               OGRXPlaneAPTLinearFeatureLayer()                       */
/************************************************************************/


OGRXPlaneAPTLinearFeatureLayer::OGRXPlaneAPTLinearFeatureLayer() : OGRXPlaneLayer("APTLinearFeature")
{
    poFeatureDefn->SetGeomType( wkbMultiLineString );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneAPTLinearFeatureLayer::AddFeature(const char* pszAptICAO,
                                                const char* pszLinearFeatureName,
                                                OGRMultiLineString* poMultilineString)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    poFeature->SetGeometry( poMultilineString );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszLinearFeatureName );

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
