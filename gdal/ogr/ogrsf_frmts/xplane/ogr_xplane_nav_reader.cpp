/******************************************************************************
 * $Id: ogr_xplane_nav_reader.cpp
 *
 * Project:  X-Plane nav.dat file reader
 * Purpose:  Implements OGRXPlaneNavReader class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_xplane_nav_reader.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                   OGRXPlaneCreateNavFileReader                       */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneCreateNavFileReader( OGRXPlaneDataSource* poDataSource )
{
    OGRXPlaneReader* poReader = new OGRXPlaneNavReader(poDataSource);
    return poReader;
}


/************************************************************************/
/*                         OGRXPlaneNavReader()                         */
/************************************************************************/
OGRXPlaneNavReader::OGRXPlaneNavReader()
{
    poILSLayer = NULL;
    poVORLayer = NULL;
    poNDBLayer = NULL;
    poGSLayer = NULL;
    poMarkerLayer = NULL;
    poDMELayer = NULL;
    poDMEILSLayer = NULL;
}

/************************************************************************/
/*                          OGRXPlaneNavReader()                        */
/************************************************************************/

OGRXPlaneNavReader::OGRXPlaneNavReader( OGRXPlaneDataSource* poDataSource )
{
    poILSLayer = new OGRXPlaneILSLayer();
    poVORLayer = new OGRXPlaneVORLayer();
    poNDBLayer = new OGRXPlaneNDBLayer();
    poGSLayer = new OGRXPlaneGSLayer();
    poMarkerLayer = new OGRXPlaneMarkerLayer();
    poDMELayer = new OGRXPlaneDMELayer();
    poDMEILSLayer = new OGRXPlaneDMEILSLayer();

    poDataSource->RegisterLayer(poILSLayer);
    poDataSource->RegisterLayer(poVORLayer);
    poDataSource->RegisterLayer(poNDBLayer);
    poDataSource->RegisterLayer(poGSLayer);
    poDataSource->RegisterLayer(poMarkerLayer);
    poDataSource->RegisterLayer(poDMELayer);
    poDataSource->RegisterLayer(poDMEILSLayer);
}

/************************************************************************/
/*                        CloneForLayer()                               */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneNavReader::CloneForLayer(OGRXPlaneLayer* poLayer)
{
    OGRXPlaneNavReader* poReader = new OGRXPlaneNavReader();

    poReader->poInterestLayer = poLayer;

    SET_IF_INTEREST_LAYER(poILSLayer);
    SET_IF_INTEREST_LAYER(poVORLayer);
    SET_IF_INTEREST_LAYER(poNDBLayer);
    SET_IF_INTEREST_LAYER(poGSLayer);
    SET_IF_INTEREST_LAYER(poMarkerLayer);
    SET_IF_INTEREST_LAYER(poDMELayer);
    SET_IF_INTEREST_LAYER(poDMEILSLayer);

    if (pszFilename)
    {
        poReader->pszFilename = CPLStrdup(pszFilename);
        poReader->fp = VSIFOpenL( pszFilename, "rb" );
    }

    return poReader;
}

/************************************************************************/
/*                        IsRecognizedVersion()                         */
/************************************************************************/

int OGRXPlaneNavReader::IsRecognizedVersion( const char* pszVersionString)
{
    return EQUALN(pszVersionString, "810 Version", 11) ||
           EQUALN(pszVersionString, "740 Version", 11);
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

void OGRXPlaneNavReader::Read()
{
    const char* pszLine;
    while((pszLine = CPLReadLineL(fp)) != NULL)
    {
        int nType;
        papszTokens = CSLTokenizeString(pszLine);
        nTokens = CSLCount(papszTokens);

        nLineNumber ++;

        if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
        {
            CSLDestroy(papszTokens);
            papszTokens = NULL;
            bEOF = TRUE;
            return;
        }
        else if (nTokens == 0 || assertMinCol(9) == FALSE)
        {
            CSLDestroy(papszTokens);
            papszTokens = NULL;
            continue;
        }

        nType = atoi(papszTokens[0]);
        if (!((nType >= NAVAID_NDB && nType <= NAVAID_IM) ||
               nType == NAVAID_DME_COLOC || nType == NAVAID_DME_STANDALONE))
        {
            CPLDebug("XPlane", "Line %d : bad feature code '%s'",
                     nLineNumber, papszTokens[0]);
            CSLDestroy(papszTokens);
            papszTokens = NULL;
            continue;
        }

        ParseRecord(nType);

        CSLDestroy(papszTokens);
        papszTokens = NULL;

        if (poInterestLayer && poInterestLayer->IsEmpty() == FALSE)
            return;
    }

    papszTokens = NULL;
    bEOF = TRUE;
}

/************************************************************************/
/*                            ParseRecord()                             */
/************************************************************************/

void    OGRXPlaneNavReader::ParseRecord(int nType)
{
    double dfVal, dfLat, dfLon, dfElevation, dfFrequency, dfRange;
    double dfSlavedVariation = 0, dfTrueHeading = 0,
            dfDMEBias = 0, dfSlope = 0;
    char* pszNavaidId;

    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 1));

    /* feet to meter */
    RET_IF_FAIL(readDoubleWithBoundsAndConversion(&dfElevation, 3, "elevation", FEET_TO_METER, -1000., 10000.));

    RET_IF_FAIL(readDouble(&dfFrequency, 4, "frequency"));
    /* NDB frequencies are in kHz. Others must be divided by 100 */
    /* to get a frequency in MHz */
    if (nType != NAVAID_NDB)
        dfFrequency /= 100.;

    /* nautical miles to kilometer */
    RET_IF_FAIL(readDouble(&dfRange, 5, "range"));
    dfRange *= NM_TO_KM;

    pszNavaidId = papszTokens[7];

    if (nType == NAVAID_NDB)
    {
        const char* pszSubType = "";
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

        osNavaidName = readStringUntilEnd(8);

        if (poNDBLayer)
            poNDBLayer->AddFeature(pszNavaidId, osNavaidName, pszSubType,
                                    dfLat, dfLon,
                                    dfElevation, dfFrequency, dfRange);
    }
    else if (nType == NAVAID_VOR)
    {
        const char* pszSubType = "";
        CPLString osNavaidName;

        RET_IF_FAIL(readDoubleWithBounds(&dfSlavedVariation, 6, "slaved variation", -180., 180.));

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

        osNavaidName = readStringUntilEnd(8);

        if (poVORLayer)
            poVORLayer->AddFeature(pszNavaidId, osNavaidName, pszSubType,
                                    dfLat, dfLon,
                                    dfElevation, dfFrequency, dfRange, dfSlavedVariation);
    }
    else if (nType == NAVAID_LOC_ILS || nType == NAVAID_LOC_STANDALONE)
    {
        const char* pszAptICAO, * pszRwyNum, * pszSubType;

        RET_IF_FAIL(readDoubleWithBounds(&dfTrueHeading, 6, "true heading", 0., 360.));

        RET_IF_FAIL(assertMinCol(11));

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
            if (poILSLayer)
                poILSLayer->AddFeature(pszNavaidId, pszAptICAO, pszRwyNum, pszSubType,
                                        dfLat, dfLon,
                                        dfElevation, dfFrequency, dfRange, dfTrueHeading);
        }
        else
        {
            CPLDebug("XPlane", "Line %d : invalid localizer subtype: '%s'",
                    nLineNumber, pszSubType);
            return;
        }
    }
    else if (nType == NAVAID_GS)
    {
        const char* pszAptICAO, * pszRwyNum, * pszSubType;

        RET_IF_FAIL(readDouble(&dfVal, 6, "slope & heading"));
        dfSlope = (int)(dfVal / 1000) / 100.;
        dfTrueHeading = dfVal - dfSlope * 100000;
        if (dfTrueHeading < 0 || dfTrueHeading > 360)
        {
            CPLDebug("XPlane", "Line %d : invalid true heading '%f'",
                    nLineNumber, dfTrueHeading);
            return;
        }

        RET_IF_FAIL(assertMinCol(11));

        pszAptICAO = papszTokens[8];
        pszRwyNum = papszTokens[9];
        pszSubType = papszTokens[10];

        if (EQUAL(pszSubType, "GS") )
        {
            if (poGSLayer)
                poGSLayer->AddFeature(pszNavaidId, pszAptICAO, pszRwyNum,
                                        dfLat, dfLon,
                                        dfElevation, dfFrequency, dfRange, dfTrueHeading, dfSlope);
        }
        else
        {
            CPLDebug("XPlane", "Line %d : invalid glideslope subtype: '%s'",
                    nLineNumber, pszSubType);
            return;
        }
    }
    else if (nType == NAVAID_OM || nType == NAVAID_MM || nType == NAVAID_IM)
    {
        const char* pszAptICAO, * pszRwyNum, * pszSubType;

        RET_IF_FAIL(readDoubleWithBounds(&dfTrueHeading, 6, "true heading", 0., 360.));

        RET_IF_FAIL(assertMinCol(11));

        pszAptICAO = papszTokens[8];
        pszRwyNum = papszTokens[9];
        pszSubType = papszTokens[10];

        if (EQUAL(pszSubType, "OM") ||
            EQUAL(pszSubType, "MM") ||
            EQUAL(pszSubType, "IM") )
        {
            if (poMarkerLayer)
                poMarkerLayer->AddFeature(pszAptICAO, pszRwyNum, pszSubType,
                                            dfLat, dfLon,
                                            dfElevation, dfTrueHeading);
        }
        else
        {
            CPLDebug("XPlane", "Line %d : invalid localizer marker subtype: '%s'",
                    nLineNumber, pszSubType);
            return;
        }
    }
    else if (nType == NAVAID_DME_COLOC || nType == NAVAID_DME_STANDALONE)
    {
        const char* pszSubType = "";
        CPLString osNavaidName;

        RET_IF_FAIL(readDouble(&dfDMEBias, 6, "DME bias"));
        dfDMEBias *= NM_TO_KM;

        if (EQUAL(papszTokens[nTokens-1], "DME-ILS"))
        {
            char* pszAptICAO, * pszRwyNum /* , * pszSubType */;
            if (nTokens != 11)
            {
                CPLDebug("XPlane", "Line %d : not enough columns : %d",
                        nLineNumber, nTokens);
                return;
            }

            pszAptICAO = papszTokens[8];
            pszRwyNum = papszTokens[9];
            /* pszSubType = papszTokens[10]; */

            if (poDMEILSLayer)
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
                    /* pszSubType = papszTokens[nTokens-1]; */
                    nTokens--;
                }
            }
            else
            {
                CPLDebug("XPlane", "Line %d : Unexpected DME subtype : %s",
                            nLineNumber, papszTokens[nTokens-1]);
            }

            osNavaidName = readStringUntilEnd(8);

            if (poDMELayer)
                poDMELayer->AddFeature(pszNavaidId, osNavaidName, pszSubType,
                                        dfLat, dfLon,
                                        dfElevation, dfFrequency, dfRange, dfDMEBias);
        }
    }
    else
    {
        CPLAssert(0);
    }

}


/************************************************************************/
/*                           OGRXPlaneILSLayer()                        */
/************************************************************************/

OGRXPlaneILSLayer::OGRXPlaneILSLayer() : OGRXPlaneLayer("ILS")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneILSLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfTrueHeading)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfTrueHeading );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneVORLayer()                        */
/************************************************************************/


OGRXPlaneVORLayer::OGRXPlaneVORLayer() : OGRXPlaneLayer("VOR")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("navaid_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldSlavedVariation("slaved_variation_deg", OFTReal );
    oFieldSlavedVariation.SetWidth( 6 );
    oFieldSlavedVariation.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSlavedVariation );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneVORLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfSlavedVariation)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszNavaidName );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfSlavedVariation );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneNDBLayer()                        */
/************************************************************************/

OGRXPlaneNDBLayer::OGRXPlaneNDBLayer() : OGRXPlaneLayer("NDB")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("navaid_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_khz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneNDBLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszNavaidName );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneGSLayer                          */
/************************************************************************/

OGRXPlaneGSLayer::OGRXPlaneGSLayer() : OGRXPlaneLayer("GS")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldGlideSlope("glide_slope", OFTReal );
    oFieldGlideSlope.SetWidth( 6 );
    oFieldGlideSlope.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldGlideSlope );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneGSLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfTrueHeading,
                                   double dfSlope)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfSlope );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                         OGRXPlaneMarkerLayer                         */
/************************************************************************/

OGRXPlaneMarkerLayer::OGRXPlaneMarkerLayer() : OGRXPlaneLayer("Marker")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneMarkerLayer::AddFeature(const char* pszAptICAO,
                                      const char* pszRwyNum,
                                      const char* pszSubType,
                                      double dfLat,
                                      double dfLon,
                                      double dfEle,
                                      double dfTrueHeading)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfTrueHeading );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneDMEILSLayer                          */
/************************************************************************/

OGRXPlaneDMEILSLayer::OGRXPlaneDMEILSLayer() : OGRXPlaneLayer("DMEILS")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldBias("bias_km", OFTReal );
    oFieldBias.SetWidth( 6 );
    oFieldBias.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldBias );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
      OGRXPlaneDMEILSLayer::AddFeature(const char* pszNavaidID,
                                      const char* pszAptICAO,
                                      const char* pszRwyNum,
                                      double dfLat,
                                      double dfLon,
                                      double dfEle,
                                      double dfFreq,
                                      double dfRange,
                                      double dfBias)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfBias );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneDMELayer                          */
/************************************************************************/


OGRXPlaneDMELayer::OGRXPlaneDMELayer() : OGRXPlaneLayer("DME")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("navaid_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldBias("bias_km", OFTReal );
    oFieldBias.SetWidth( 6 );
    oFieldBias.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldBias );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneDMELayer::AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfBias)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszNavaidName );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfBias );

    RegisterFeature(poFeature);

    return poFeature;
}
