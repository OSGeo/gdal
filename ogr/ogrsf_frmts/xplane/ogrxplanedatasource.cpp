/******************************************************************************
 * $Id: ogrxplanedatasource.cpp
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Implements OGRXPlaneDataSource class
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

#include <math.h>

#include "ogr_xplane.h"
#include "ogr_xplane_geo_utils.h"

#define FEET_TO_METER       0.30479999798832
#define NM_TO_KM            1.852


/************************************************************************/
/*                          OGRXPlaneDataSource()                          */
/************************************************************************/

OGRXPlaneDataSource::OGRXPlaneDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
}

/************************************************************************/
/*                         ~OGRXPlaneDataSource()                          */
/************************************************************************/

OGRXPlaneDataSource::~OGRXPlaneDataSource()

{
    CPLFree( pszName );
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRXPlaneDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           RegisterLayer()                            */
/************************************************************************/

void OGRXPlaneDataSource::RegisterLayer(OGRXPlaneLayer* poLayer)
{
    papoLayers = (OGRXPlaneLayer**) CPLRealloc(papoLayers,
                                    (nLayers + 1) * sizeof(OGRXPlaneLayer*));
    papoLayers[nLayers++] = poLayer;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRXPlaneDataSource::Open( const char * pszFilename )

{
    const char* pszShortFilename = CPLGetFilename(pszFilename);
    if (EQUAL(pszShortFilename, "nav.dat"))
    {
        return ParseNavDatFile(pszFilename);
    }
    else if (EQUAL(pszShortFilename, "apt.dat"))
    {
        return ParseAptDatFile(pszFilename);
    }
    else
    {
        return FALSE;
    }
}



#define READ_DOUBLE(iToken, pszTokenDesc) \
        CPLStrtod(papszTokens[iToken], &pszNext); \
        if (*pszNext != '\0' ) \
        { \
            CPLDebug("XPlane", "Line %d : invalid " pszTokenDesc " '%s'", \
                     nLineNumber, papszTokens[iToken]); \
            goto next_line; \
        }

#define READ_DOUBLE_WITH_BOUNDS_AND_CONVERSION(iToken, pszTokenDesc, dfFactor, lowerBound, upperBound) \
        dfTmp = dfFactor * READ_DOUBLE(iToken, pszTokenDesc) \
        if (dfTmp < lowerBound || dfTmp > upperBound) \
        { \
            CPLDebug("XPlane", "Line %d : " pszTokenDesc " '%s' out of bounds [%f, %f]", \
                     nLineNumber, papszTokens[iToken], lowerBound / dfFactor, upperBound / dfFactor); \
            goto next_line; \
        }

#define READ_DOUBLE_WITH_BOUNDS(iToken, pszTokenDesc, lowerBound, upperBound) \
        READ_DOUBLE_WITH_BOUNDS_AND_CONVERSION(iToken, pszTokenDesc, 1., lowerBound, upperBound)


#define ASSERT_MIN_COL(nMinColNum) \
            if (nTokens < nMinColNum) \
            { \
                CPLDebug("XPlane", "Line %d : not enough columns : %d. %d is the minimum required", \
                        nLineNumber, nTokens, nMinColNum); \
                goto next_line; \
            }

#define READ_STRING_UNTIL_END(osResult, iFirstIndice) \
            if (nTokens > iFirstIndice) \
            { \
                int _i; \
                int _nIDsToSum = nTokens - iFirstIndice; \
                osResult = papszTokens[iFirstIndice]; \
                for(_i=1;_i<_nIDsToSum;_i++) \
                { \
                    osResult += " "; \
                    osResult += papszTokens[iFirstIndice + _i]; \
                } \
            }

enum
{
    NAVAID_NDB            = 2, 
    NAVAID_VOR            = 3, /* VOR, VORTAC or VOR-DME.*/
    NAVAID_LOC_ILS        = 4, /* Localiser that is part of a full ILS */
    NAVAID_LOC_STANDALONE = 5, /* Stand-alone localiser (LOC), also including a LDA (Landing Directional Aid) or SDF (Simplified Directional Facility) */
    NAVAID_GS             = 6, /* Glideslope */
    NAVAID_OM             = 7, /* Outer marker */
    NAVAID_MM             = 8, /* Middle marker */
    NAVAID_IM             = 9, /* Inner marker */
    NAVAID_DME_COLOC      = 12, /* DME (including the DME element of an ILS, VORTAC or VOR-DME) */
    NAVAID_DME_STANDALONE = 13, /* DME (including the DME element of an NDB-DME) */
};

/************************************************************************/
/*                     ParseNavDatFile()                                */
/************************************************************************/

int OGRXPlaneDataSource::ParseNavDatFile( const char * pszFilename )
{
    FILE* fp = VSIFOpen( pszFilename, "rt" );
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
    if (!pszLine || EQUALN(pszLine, "810 Version", 11) == FALSE &&
                    EQUALN(pszLine, "740 Version", 11) == FALSE)
    {
        VSIFClose(fp);
        return FALSE;
    }

    int nLineNumber = 2;
    CPLDebug("XPlane", "Version/Copyright : %s", pszLine);

    OGRXPlaneILSLayer* poILSLayer = new OGRXPlaneILSLayer();
    OGRXPlaneVORLayer* poVORLayer = new OGRXPlaneVORLayer();
    OGRXPlaneNDBLayer* poNDBLayer = new OGRXPlaneNDBLayer();
    OGRXPlaneGSLayer* poGSLayer = new OGRXPlaneGSLayer();
    OGRXPlaneMarkerLayer* poMarkerLayer = new OGRXPlaneMarkerLayer();
    OGRXPlaneDMELayer* poDMELayer = new OGRXPlaneDMELayer();
    OGRXPlaneDMEILSLayer* poDMEILSLayer = new OGRXPlaneDMEILSLayer();

    RegisterLayer(poILSLayer);
    RegisterLayer(poVORLayer);
    RegisterLayer(poNDBLayer);
    RegisterLayer(poGSLayer);
    RegisterLayer(poMarkerLayer);
    RegisterLayer(poDMELayer);
    RegisterLayer(poDMEILSLayer);

    while((pszLine = CPLReadLine(fp)) != NULL)
    {
        int nType;
        double dfTmp;
        double dfVal, dfLat, dfLon, dfElevation, dfFrequency, dfRange;
        double dfSlavedVariation = 0, dfTrueHeading = 0,
               dfDMEBias = 0, dfSlope = 0;
        char* pszNavaidId;
        char** papszTokens = CSLTokenizeString(pszLine);
        char* pszNext;
        int nTokens = CSLCount(papszTokens);

        nLineNumber ++;

        if (nTokens == 0)
        {
            goto next_line;
        }

        if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
        {
            CSLDestroy(papszTokens);
            break;
        }

        if (nTokens < 9)
        {
            CPLDebug("XPlane", "Line %d : not enough columns : %d",
                     nLineNumber, nTokens);
            goto next_line;
        }

        nType = atoi(papszTokens[0]);
        if (!((nType >= 2 && nType <= 9) || nType == 12 || nType == 13))
        {
            CPLDebug("XPlane", "Line %d : bad feature code '%s'",
                     nLineNumber, papszTokens[0]);
            goto next_line;
        }

        dfLat = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);

        dfLon = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);

        /* feet to meter */
        dfElevation = READ_DOUBLE_WITH_BOUNDS_AND_CONVERSION(3, "elevation", FEET_TO_METER, -1000., 10000.);

        dfFrequency = READ_DOUBLE(4, "frequency");
        /* NDB frequencies are in kHz. Others must be divided by 100 */
        /* to get a frequency in MHz */
        if (nType != NAVAID_NDB)
            dfFrequency /= 100.;

        /* nautical miles to kilometer */
        dfRange = NM_TO_KM * READ_DOUBLE(5, "range");

        pszNavaidId = papszTokens[7];

        if (nType == NAVAID_NDB)
        {
            char* pszSubType = "";
            CPLString osNavaidName;
            if (EQUAL(papszTokens[nTokens-1], "NDB") ||
                EQUAL(papszTokens[nTokens-1], "LOM") ||
                EQUAL(papszTokens[nTokens-1], "NDB-DME"))
            {
                pszSubType = papszTokens[nTokens-1];
                nTokens--;
            }
            else
            {
                CPLDebug("XPlane", "Unexpected NDB subtype : %s", papszTokens[nTokens-1]);
            }

            READ_STRING_UNTIL_END(osNavaidName, 8);

            poNDBLayer->AddFeature(pszNavaidId, osNavaidName, pszSubType,
                                   dfLat, dfLon,
                                   dfElevation, dfFrequency, dfRange);
        }
        else if (nType == NAVAID_VOR)
        {
            char* pszSubType = "";
            CPLString osNavaidName;

            dfSlavedVariation = READ_DOUBLE_WITH_BOUNDS(6, "slaved variation", -180., 180.);

            if (EQUAL(papszTokens[nTokens-1], "VOR") ||
                EQUAL(papszTokens[nTokens-1], "VORTAC") ||
                EQUAL(papszTokens[nTokens-1], "VOR-DME"))
            {
                pszSubType = papszTokens[nTokens-1];
                nTokens--;
            }
            else
            {
                CPLDebug("XPlane", "Unexpected VOR subtype : %s", papszTokens[nTokens-1]);
            }

            READ_STRING_UNTIL_END(osNavaidName, 8);

            poVORLayer->AddFeature(pszNavaidId, osNavaidName, pszSubType,
                                   dfLat, dfLon,
                                   dfElevation, dfFrequency, dfRange, dfSlavedVariation);
        }
        else if (nType == NAVAID_LOC_ILS || nType == NAVAID_LOC_STANDALONE)
        {
            char* pszAptICAO, * pszRwyNum, * pszSubType;

            dfTrueHeading = READ_DOUBLE_WITH_BOUNDS(6, "true heading", 0., 360.);

            ASSERT_MIN_COL(11);

            pszAptICAO = papszTokens[8];
            pszRwyNum = papszTokens[9];
            pszSubType = papszTokens[10];

            if (EQUAL(pszSubType, "ILS-cat-I") ||
                EQUAL(pszSubType, "ILS-cat-II") ||
                EQUAL(pszSubType, "ILS-cat-III") ||
                EQUAL(pszSubType, "LOC") ||
                EQUAL(pszSubType, "LDA") ||
                EQUAL(pszSubType, "SDF") ||
                EQUAL(pszSubType, "IGS") ||
                EQUAL(pszSubType, "LDA-GS"))
            {
                poILSLayer->AddFeature(pszNavaidId, pszAptICAO, pszRwyNum, pszSubType,
                                       dfLat, dfLon,
                                       dfElevation, dfFrequency, dfRange, dfTrueHeading);
            }
            else
            {
                CPLDebug("XPlane", "Line %d : invalid localizer subtype: '%s'",
                        nLineNumber, pszSubType);
                goto next_line;
            }
        }
        else if (nType == NAVAID_GS)
        {
            char* pszAptICAO, * pszRwyNum, * pszSubType;

            dfVal = READ_DOUBLE(6, "slope & heading");
            dfSlope = (int)(dfVal / 1000) / 100.;
            dfTrueHeading = dfVal - dfSlope * 100000;
            if (dfTrueHeading < 0 || dfTrueHeading > 360)
            {
                CPLDebug("XPlane", "Line %d : invalid true heading '%f'",
                     nLineNumber, dfTrueHeading);
                goto next_line;
            }

            ASSERT_MIN_COL(11);

            pszAptICAO = papszTokens[8];
            pszRwyNum = papszTokens[9];
            pszSubType = papszTokens[10];

            if (EQUAL(pszSubType, "GS") )
            {
                poGSLayer->AddFeature(pszNavaidId, pszAptICAO, pszRwyNum,
                                       dfLat, dfLon,
                                       dfElevation, dfFrequency, dfRange, dfTrueHeading, dfSlope);
            }
            else
            {
                CPLDebug("XPlane", "Line %d : invalid glideslope subtype: '%s'",
                        nLineNumber, pszSubType);
                goto next_line;
            }
        }
        else if (nType == NAVAID_OM || nType == NAVAID_MM || nType == NAVAID_IM)
        {
            char* pszAptICAO, * pszRwyNum, * pszSubType;

            dfTrueHeading = READ_DOUBLE_WITH_BOUNDS(6, "true heading", 0., 360.);

            ASSERT_MIN_COL(11);

            pszAptICAO = papszTokens[8];
            pszRwyNum = papszTokens[9];
            pszSubType = papszTokens[10];

            if (EQUAL(pszSubType, "OM") ||
                EQUAL(pszSubType, "MM") ||
                EQUAL(pszSubType, "IM") )
            {
                poMarkerLayer->AddFeature(pszAptICAO, pszRwyNum, pszSubType,
                                          dfLat, dfLon,
                                          dfElevation, dfTrueHeading);
            }
            else
            {
                CPLDebug("XPlane", "Line %d : invalid localizer marker subtype: '%s'",
                        nLineNumber, pszSubType);
                goto next_line;
            }
        }
        else if (nType == NAVAID_DME_COLOC || nType == NAVAID_DME_STANDALONE)
        {
            char* pszSubType = "";
            CPLString osNavaidName;

            dfDMEBias = READ_DOUBLE(6, "DME bias");

            if (EQUAL(papszTokens[nTokens-1], "DME-ILS"))
            {
                char* pszAptICAO, * pszRwyNum, * pszSubType;
                if (nTokens != 11)
                {
                    CPLDebug("XPlane", "Line %d : not enough columns : %d",
                            nLineNumber, nTokens);
                    goto next_line;
                }

                pszAptICAO = papszTokens[8];
                pszRwyNum = papszTokens[9];
                pszSubType = papszTokens[10];

                poDMEILSLayer->AddFeature(pszNavaidId, pszAptICAO, pszRwyNum,
                                          dfLat, dfLon,
                                          dfElevation, dfFrequency, dfRange, dfDMEBias);
            }
            else
            {
                if (EQUAL(papszTokens[nTokens-1], "DME"))
                {
                    nTokens--;
                    if (EQUAL(papszTokens[nTokens-1], "VORTAC") ||
                        EQUAL(papszTokens[nTokens-1], "VOR-DME") ||
                        EQUAL(papszTokens[nTokens-1], "TACAN") ||
                        EQUAL(papszTokens[nTokens-1], "NDB-DME"))
                    {
                        pszSubType = papszTokens[nTokens-1];
                        nTokens--;
                    }
                }
                else
                {
                    CPLDebug("XPlane", "Unexpected DME subtype : %s", papszTokens[nTokens-1]);
                }

                READ_STRING_UNTIL_END(osNavaidName, 8);

                poDMELayer->AddFeature(pszNavaidId, osNavaidName, pszSubType,
                                       dfLat, dfLon,
                                       dfElevation, dfFrequency, dfRange, dfDMEBias);
            }
        }
        else
        {
            CPLAssert(0);
        }

next_line:
        CSLDestroy(papszTokens);
    }

    pszName = CPLStrdup(pszFilename);
    VSIFClose(fp);

    return TRUE;
}


enum
{
    APT_AIRPORT_HEADER         = 1,
    APT_RUNWAY_TAXIWAY_V_810   = 10,
    APT_RUNWAY                 = 100,
    APT_WATER_RUNWAY           = 101,
    APT_HELIPAD                = 102,
    APT_PAVEMENT               = 110,
    APT_NODE                   = 111,
    APT_NODE_WITH_BEZIER       = 112,
    APT_NODE_CLOSE             = 113,
    APT_NODE_CLOSE_WITH_BEZIER = 114,
    APT_NODE_END               = 115,
    APT_NODE_END_WITH_BEZIER   = 116,
    APT_LINEAR_HEADER          = 120,
    APT_BOUNDARY_HEADER        = 130,
    APT_VASI_PAPI_WIGWAG       = 21,
    APT_TAXIWAY_SIGNS          = 20,
    APT_STARTUP_LOCATION       = 15,
    APT_TOWER                  = 14,
    APT_LIGHT_BEACONS          = 18,
    APT_WINDSOCKS              = 19,
    APT_ATC_AWOS_ASOS_ATIS     = 50,
    APT_ATC_CTAF               = 51,
    APT_ATC_CLD                = 52,
    APT_ATC_GND                = 53,
    APT_ATC_TWR                = 54,
    APT_ATC_APP                = 55,
    APT_ATC_DEP                = 56,
};


/************************************************************************/
/*                     ParseAptDatFile()                                */
/************************************************************************/

int OGRXPlaneDataSource::ParseAptDatFile( const char * pszFilename )
{
    FILE* fp = VSIFOpen( pszFilename, "rt" );
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

    int nLineNumber = 2;
    CPLDebug("XPlane", "Version/Copyright : %s", pszLine);

    int bAptHeaderFound = FALSE;
    double dfElevation;
    int bControlTower;
    CPLString osAptICAO;
    CPLString osAptName;

    int bTowerFound = FALSE;
    double dfLatTower = 0, dfLonTower = 0;
    double dfHeightTower = 0;
    CPLString osTowerName;

    int bRunwayFound = FALSE;
    double dfLatFirstRwy = 0, dfLonFirstRwy = 0;

    OGRXPlaneAPTLayer* poAPTLayer = new OGRXPlaneAPTLayer();
    OGRXPlaneRunwayLayer* poRunwayLayer = new OGRXPlaneRunwayLayer();
    OGRXPlaneRunwayThresholdLayer* poRunwayThresholdLayer = new OGRXPlaneRunwayThresholdLayer();
    OGRXPlaneWaterRunwayLayer* poWaterRunwayLayer = new OGRXPlaneWaterRunwayLayer();
    OGRXPlaneWaterRunwayThresholdLayer* poWaterRunwayThresholdLayer = new OGRXPlaneWaterRunwayThresholdLayer();
    OGRXPlaneHelipadLayer* poHelipadLayer = new OGRXPlaneHelipadLayer();
    OGRXPlaneHelipadPolygonLayer* poHelipadPolygonLayer = new OGRXPlaneHelipadPolygonLayer();
    OGRXPlaneATCFreqLayer* poATCFreqLayer = new OGRXPlaneATCFreqLayer();
    OGRXPlaneStartupLocationLayer* poStartupLocationLayer = new OGRXPlaneStartupLocationLayer();
    OGRXPlaneAPTLightBeaconLayer* poAPTLightBeaconLayer = new OGRXPlaneAPTLightBeaconLayer();
    OGRXPlaneAPTWindsockLayer* poAPTWindsockLayer = new OGRXPlaneAPTWindsockLayer();
    OGRXPlaneTaxiwaySignLayer* poTaxiwaySignLayer = new OGRXPlaneTaxiwaySignLayer();
    OGRXPlane_VASI_PAPI_WIGWAG_Layer* poVASI_PAPI_WIGWAG_Layer = new OGRXPlane_VASI_PAPI_WIGWAG_Layer();

    RegisterLayer(poAPTLayer);
    RegisterLayer(poRunwayLayer);
    RegisterLayer(poRunwayThresholdLayer);
    RegisterLayer(poWaterRunwayLayer);
    RegisterLayer(poWaterRunwayThresholdLayer);
    RegisterLayer(poHelipadLayer);
    RegisterLayer(poHelipadPolygonLayer);
    RegisterLayer(poATCFreqLayer);
    RegisterLayer(poStartupLocationLayer);
    RegisterLayer(poAPTLightBeaconLayer);
    RegisterLayer(poAPTWindsockLayer);
    RegisterLayer(poTaxiwaySignLayer);
    RegisterLayer(poVASI_PAPI_WIGWAG_Layer);

    while((pszLine = CPLReadLine(fp)) != NULL)
    {
        double dfTmp;
        char* pszNext;
        int nType;
        char** papszTokens = CSLTokenizeString(pszLine);
        int nTokens = CSLCount(papszTokens);

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
                                       (bTowerFound) ? dfLonTower : dfLatFirstRwy, 
                                       bTowerFound, dfHeightTower, osTowerName);
            }
            CSLDestroy(papszTokens);
            break;
        }

        ASSERT_MIN_COL(2);

        nType = atoi(papszTokens[0]);
        if (nType == APT_AIRPORT_HEADER)
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

            ASSERT_MIN_COL(6);

            /* feet to meter */
            dfElevation = READ_DOUBLE_WITH_BOUNDS_AND_CONVERSION(1, "elevation", FEET_TO_METER, -1000., 10000.);
            bControlTower = atoi(papszTokens[2]);
            // papszTokens[3] ignored
            osAptICAO = papszTokens[4];
            READ_STRING_UNTIL_END(osAptName, 5);

            bAptHeaderFound = TRUE;
        }
        else if (nType == APT_RUNWAY_TAXIWAY_V_810)
        {
            /*
            ASSERT_MIN_COL(15);

            dfLat = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);
            dfLon = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);
            pszRwyNum = papszTokens[3];
            dfTrueHeading = READ_DOUBLE_WITH_BOUNDS(4, "true heading", 0., 360.);
            dfLength = READ_DOUBLE(5, "length");
            dfLength *= FEET_TO_METER;
            dfDisplacedThresholdLength1 = atoi(papszTokens[6]) * FEET_TO_METER;
            if (strchr(papszTokens[6])
                dfDisplacedThresholdLength2 = atoi(strchr(papszTokens[6]) + 1) * FEET_TO_METER;
            dfStopwayLength1 = atoi(papszTokens[7]) * FEET_TO_METER;
            if (strchr(papszTokens[7])
                dfStopwayLength2 = atoi(strchr(papszTokens[7]) + 1) * FEET_TO_METER;
            dfWidth = READ_DOUBLE(8, "width");
            dfWidth *= FEET_TO_METER;
            if (strlen(papszTokens[9] == 6)
            {
                eVisualApproachLighting1 = papszTokens[9][0] - '0';
                // TODO
            }
            */
        }
        else if (nType == APT_RUNWAY)
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

            ASSERT_MIN_COL(8 + 9 + 9);

            dfWidth = READ_DOUBLE(1, "runway width");
            eSurfaceCode = atoi(papszTokens[2]);
            eShoulderCode = atoi(papszTokens[3]);
            dfSmoothness = READ_DOUBLE_WITH_BOUNDS(4, "runway smoothness", 0., 1.);
            bHasCenterLineLights = atoi(papszTokens[5]);
            bHasMIRL = atoi(papszTokens[6]) == 2;
            bHasDistanceRemainingSigns = atoi(papszTokens[7]);

            for( nRwy=0, nCurToken = 8 ; nRwy<=1 ; nRwy++, nCurToken += 9 )
            {
                double dfLat, dfLon, dfDisplacedThresholdLength, dfStopwayLength;
                int eMarkings, eApproachLightingCode, eREIL;
                int bHasTouchdownLights;

                aosRunwayId[nRwy] = papszTokens[nCurToken + 0]; /* for example : 08, 24R, or xxx */
                adfLat[nRwy] = dfLat = READ_DOUBLE_WITH_BOUNDS(nCurToken + 1, "latitude", -90., 90.);
                adfLon[nRwy] = dfLon = READ_DOUBLE_WITH_BOUNDS(nCurToken + 2, "longitude", -180., 180.);
                dfDisplacedThresholdLength = READ_DOUBLE(nCurToken + 3, "displaced threshold length");
                dfStopwayLength = READ_DOUBLE(nCurToken + 4, "stopway/blastpad/over-run length");
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
        else if (nType == APT_WATER_RUNWAY)
        {
            double adfLat[2], adfLon[2];
            OGRFeature* apoWaterRunwayThreshold[2];
            double dfWidth, dfLength;
            int bBuoys;
            CPLString aosRunwayId[2];
            int i;

            ASSERT_MIN_COL(9);

            dfWidth = READ_DOUBLE(1, "runway width");
            bBuoys = atoi(papszTokens[2]);

            for(i=0;i<2;i++)
            {
                aosRunwayId[i] = papszTokens[3 + 3*i];
                adfLat[i] = READ_DOUBLE_WITH_BOUNDS(4 + 3*i, "latitude", -90., 90.);
                adfLon[i] = READ_DOUBLE_WITH_BOUNDS(5 + 3*i, "longitude", -180., 180.);

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
        else if (nType == APT_HELIPAD)
        {
            double dfLat, dfLon, dfTrueHeading, dfLength, dfWidth, dfSmoothness;
            int eSurfaceCode, eMarkings, eShoulderCode;
            int bEdgeLighting;
            const char* pszHelipadName;

            ASSERT_MIN_COL(12);

            pszHelipadName = papszTokens[1];
            dfLat = READ_DOUBLE_WITH_BOUNDS(2, "latitude", -90., 90.);
            dfLon = READ_DOUBLE_WITH_BOUNDS(3, "longitude", -180., 180.);
            dfTrueHeading = READ_DOUBLE_WITH_BOUNDS(4, "true heading", 0., 360.);
            dfLength = READ_DOUBLE(5, "length");
            dfWidth = READ_DOUBLE(6, "width");
            eSurfaceCode = atoi(papszTokens[7]);
            eMarkings = atoi(papszTokens[8]);
            eShoulderCode = atoi(papszTokens[9]);
            dfSmoothness = READ_DOUBLE_WITH_BOUNDS(10, "helipad smoothness", 0., 1.);
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
        else if (nType == APT_TOWER)
        {
            ASSERT_MIN_COL(6);

            dfLatTower = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);
            dfLonTower = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);

            /* feet to meter */
            dfHeightTower = READ_DOUBLE_WITH_BOUNDS_AND_CONVERSION(3, "tower height", FEET_TO_METER, 0., 300.);

            // papszTokens[4] ignored

            READ_STRING_UNTIL_END(osTowerName, 5);

            bTowerFound = TRUE;
        }
        else if (nType >= APT_ATC_AWOS_ASOS_ATIS && nType <= APT_ATC_DEP)
        {
            double dfFrequency;
            CPLString osFreqName;

            ASSERT_MIN_COL(2);

            dfFrequency = READ_DOUBLE(1, "frequency");
            dfFrequency /= 100.;

            READ_STRING_UNTIL_END(osFreqName, 2);

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
        else if (nType == APT_STARTUP_LOCATION)
        {
            double dfLat, dfLon, dfTrueHeading;
            CPLString osName;

            ASSERT_MIN_COL(4);

            dfLat = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);
            dfLon = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);
            dfTrueHeading = READ_DOUBLE_WITH_BOUNDS(3, "true heading", -180., 360.);
            if (dfTrueHeading < 0)
                dfTrueHeading += 360;
            READ_STRING_UNTIL_END(osName, 4);

            poStartupLocationLayer->AddFeature(osAptICAO, osName, dfLat, dfLon, dfTrueHeading);
        }
        else if (nType == APT_LIGHT_BEACONS)
        {
            double dfLat, dfLon;
            int eColor;
            CPLString osName;

            ASSERT_MIN_COL(4);

            dfLat = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);
            dfLon = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);
            eColor = atoi(papszTokens[3]);
            READ_STRING_UNTIL_END(osName, 4);

            poAPTLightBeaconLayer->AddFeature(osAptICAO, osName, dfLat, dfLon,
                                              APTLightBeaconColorEnumeration.GetText(eColor));
        }
        else if (nType == APT_WINDSOCKS)
        {
            double dfLat, dfLon;
            int bIsIllumnited;
            CPLString osName;

            ASSERT_MIN_COL(4);

            dfLat = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);
            dfLon = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);
            bIsIllumnited = atoi(papszTokens[3]);
            READ_STRING_UNTIL_END(osName, 4);

            poAPTWindsockLayer->AddFeature(osAptICAO, osName, dfLat, dfLon,
                                           bIsIllumnited);
        }
        else if (nType == APT_TAXIWAY_SIGNS)
        {
            double dfLat, dfLon;
            double dfTrueHeading;
            int nSize;
            const char* pszText;

            ASSERT_MIN_COL(7);

            dfLat = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);
            dfLon = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);
            dfTrueHeading = READ_DOUBLE_WITH_BOUNDS(3, "heading", 0, 360);
            /* papszTokens[4] : ignored. Taxiway sign style */
            nSize = atoi(papszTokens[5]);
            pszText = papszTokens[6];

            poTaxiwaySignLayer->AddFeature(osAptICAO, pszText, dfLat, dfLon,
                                           dfTrueHeading, nSize);
        }
        else if (nType == APT_VASI_PAPI_WIGWAG)
        {
            double dfLat, dfLon;
            int eType;
            double dfTrueHeading, dfVisualGlidePathAngle;
            const char* pszRwyNum;

            ASSERT_MIN_COL(7);

            dfLat = READ_DOUBLE_WITH_BOUNDS(1, "latitude", -90., 90.);
            dfLon = READ_DOUBLE_WITH_BOUNDS(2, "longitude", -180., 180.);
            eType = atoi(papszTokens[3]);
            dfTrueHeading = READ_DOUBLE_WITH_BOUNDS(4, "heading", 0, 360);
            dfVisualGlidePathAngle = READ_DOUBLE_WITH_BOUNDS(5, "visual glidepath angle", 0, 90);
            pszRwyNum = papszTokens[6];
            /* papszTokens[7] : ignored. Type of lighting object represented */

            poVASI_PAPI_WIGWAG_Layer->AddFeature(osAptICAO, pszRwyNum, VASI_PAPI_WIGWAG_Enumeration.GetText(eType),
                                                 dfLat, dfLon,
                                                 dfTrueHeading, dfVisualGlidePathAngle);
        }

next_line:
        CSLDestroy(papszTokens);
    }

    pszName = CPLStrdup(pszFilename);
    VSIFClose(fp);

    return TRUE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRXPlaneDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}


/************************************************************************/
/*                       OGRXPlaneEnumeration                           */
/***********************************************************************/


OGRXPlaneEnumeration::OGRXPlaneEnumeration(const char *pszEnumerationName,
                            const sEnumerationElement*  osElements,
                            int nElements) :
                                 m_pszEnumerationName(pszEnumerationName),
                                 m_osElements(osElements),
                                 m_nElements(nElements)
{
}

const char* OGRXPlaneEnumeration::GetText(int eValue)
{
    int i;
    for(i=0;i<m_nElements;i++)
    {
        if (m_osElements[i].eValue == eValue)
            return m_osElements[i].pszText;
    }
    CPLDebug("XPlane", "Unknown value (%d) for enumeration %s",
             eValue, m_pszEnumerationName);
    return NULL;
}

int OGRXPlaneEnumeration::GetValue(const char* pszText)
{
    int i;
    if (pszText != NULL)
    {
        for(i=0;i<m_nElements;i++)
        {
            if (strcmp(m_osElements[i].pszText, pszText) == 0)
                return m_osElements[i].eValue;
        }
    }
    CPLDebug("XPlane", "Unknown text (%s) for enumeration %s",
             pszText, m_pszEnumerationName);
    return -1;
}
