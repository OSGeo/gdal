/******************************************************************************
 *
 * Project:  SEG-Y Translator
 * Purpose:  Implements OGRSEGYLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMSEGYS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_segy.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

// #define SEGY_EXTENSIONS

static const int DT_IBM_4BYTES_FP       = 1;
static const int DT_4BYTES_INT          = 2;
static const int DT_2BYTES_INT          = 3;
static const int DT_4BYTES_FP_WITH_GAIN = 4;
static const int DT_IEEE_4BYTES_FP      = 5;
static const int DT_1BYTE_INT           = 8;

typedef struct
{
    const char*     pszName;
    OGRFieldType    eType;
} FieldDesc;

static const FieldDesc SEGYFields[] =
{
    { "TRACE_NUMBER_WITHIN_LINE", OFTInteger },
    { "TRACE_NUMBER_WITHIN_FILE", OFTInteger },
    { "ORIGINAL_FIELD_RECORD_NUMBER", OFTInteger },
    { "TRACE_NUMBER_WITHIN_ORIGINAL_FIELD_RECORD", OFTInteger },
    { "TRACE_IDENTIFICATION_CODE", OFTInteger },
    { "ENSEMBLE_NUMBER", OFTInteger },
    { "TRACE_NUMBER_WITHIN_ENSEMBLE", OFTInteger },
    { "NUMBER_VERTICAL_SUMMED_TRACES", OFTInteger },
    { "NUMBER_HORIZONTAL_STACKED_TRACES", OFTInteger },
    { "DATA_USE", OFTInteger },
    { "DISTANCE_SOURCE_GROUP", OFTInteger },
    { "RECEIVER_GROUP_ELEVATION", OFTInteger },
    { "SURFACE_ELEVATION_AT_SOURCE", OFTInteger },
    { "SOURCE_DEPTH_BELOW_SURFACE", OFTInteger },
    { "DATUM_ELEVATION_AT_RECEIVER_GROUP", OFTInteger },
    { "DATUM_ELEVATION_AT_SOURCE", OFTInteger },
    { "WATER_DEPTH_AT_SOURCE", OFTInteger },
    { "WATER_DEPTH_AT_GROUP", OFTInteger },
    { "VERTICAL_SCALAR", OFTInteger },
    { "HORIZONTAL_SCALAR", OFTInteger },
    { "SOURCE_X", OFTInteger },
    { "SOURCE_Y", OFTInteger },
    { "GROUP_X", OFTInteger },
    { "GROUP_Y", OFTInteger },
    { "COORDINATE_UNITS", OFTInteger },
    { "WEATHERING_VELOCITY", OFTInteger },
    { "SUB_WEATHERING_VELOCITY", OFTInteger },
    { "UPHOLE_TIME_AT_SOURCE", OFTInteger },
    { "UPHOLE_TIME_AT_GROUP", OFTInteger },
    { "SOURCE_STATIC_CORRECTION", OFTInteger },
    { "GROUP_STATIC_CORRECTION", OFTInteger },
    { "TOTAL_STATIC_CORRECTION", OFTInteger },
    { "LAG_TIME_A", OFTInteger },
    { "LAG_TIME_B", OFTInteger },
    { "DELAY_RECORDING_TIME", OFTInteger },
    { "MUTE_TIME_START", OFTInteger },
    { "MUTE_TIME_END", OFTInteger },
    { "SAMPLES", OFTInteger },
    { "SAMPLE_INTERVAL", OFTInteger },
    { "GAIN_TYPE", OFTInteger },
    { "INSTRUMENT_GAIN_CONSTANT", OFTInteger },
    { "INSTRUMENT_INITIAL_GAIN", OFTInteger },
    { "CORRELATED", OFTInteger },
    { "SWEEP_FREQUENCY_AT_START", OFTInteger },
    { "SWEEP_FREQUENCY_AT_END", OFTInteger },
    { "SWEEP_LENGTH", OFTInteger },
    { "SWEEP_TYPE", OFTInteger },
    { "SWEEP_TRACE_TAPER_LENGTH_AT_START", OFTInteger },
    { "SWEEP_TRACE_TAPER_LENGTH_AT_END", OFTInteger },
    { "TAPER_TYPE", OFTInteger },
    { "ALIAS_FILTER_FREQUENCY", OFTInteger },
    { "ALIAS_FILTER_SLOPE", OFTInteger },
    { "NOTCH_FILTER_FREQUENCY", OFTInteger },
    { "NOTCH_FILTER_SLOPE", OFTInteger },
    { "LOW_CUT_FREQUENCY", OFTInteger },
    { "HIGH_CUT_FREQUENCY", OFTInteger },
    { "LOW_CUT_SLOPE", OFTInteger },
    { "HIGH_CUT_SLOPE", OFTInteger },
    { "YEAR", OFTInteger },
    { "DAY_OF_YEAR", OFTInteger },
    { "HOUR", OFTInteger },
    { "MINUTE", OFTInteger },
    { "SECOND", OFTInteger },
    { "TIME_BASIC_CODE", OFTInteger },
    { "TRACE_WEIGHTING_FACTOR", OFTInteger },
    { "GEOPHONE_GROUP_NUMBER_OF_ROLL_SWITH", OFTInteger },
    { "GEOPHONE_GROUP_NUMBER_OF_TRACE_NUMBER_ONE", OFTInteger },
    { "GEOPHONE_GROUP_NUMBER_OF_LAST_TRACE", OFTInteger },
    { "GAP_SIZE", OFTInteger },
    { "OVER_TRAVEL", OFTInteger },
};

/* SEGY >= 1.0 */
static const FieldDesc SEGYFields10[] =
{
    { "INLINE_NUMBER", OFTInteger },
    { "CROSSLINE_NUMBER", OFTInteger },
    { "SHOTPOINT_NUMBER", OFTInteger },
    { "SHOTPOINT_SCALAR", OFTInteger },
};

static const int TRACE_NUMBER_WITHIN_LINE = 0;
static const int TRACE_NUMBER_WITHIN_FILE = 1;
static const int ORIGINAL_FIELD_RECORD_NUMBER = 2;
static const int TRACE_NUMBER_WITHIN_ORIGINAL_FIELD_RECORD = 3;
static const int TRACE_IDENTIFICATION_CODE = 4;
static const int ENSEMBLE_NUMBER = 5;
static const int TRACE_NUMBER_WITHIN_ENSEMBLE = 6;
static const int NUMBER_VERTICAL_SUMMED_TRACES = 7;
static const int NUMBER_HORIZONTAL_STACKED_TRACES = 8;
static const int DATA_USE = 9;
static const int DISTANCE_SOURCE_GROUP = 10;
static const int RECEIVER_GROUP_ELEVATION = 11;
static const int SURFACE_ELEVATION_AT_SOURCE = 12;
static const int SOURCE_DEPTH_BELOW_SURFACE = 13;
static const int DATUM_ELEVATION_AT_RECEIVER_GROUP = 14;
static const int DATUM_ELEVATION_AT_SOURCE = 15;
static const int WATER_DEPTH_AT_SOURCE = 16;
static const int WATER_DEPTH_AT_GROUP = 17;
static const int VERTICAL_SCALAR = 18;
static const int HORIZONTAL_SCALAR = 19;
static const int SOURCE_X = 20;
static const int SOURCE_Y = 21;
static const int GROUP_X = 22;
static const int GROUP_Y = 23;
static const int COORDINATE_UNITS = 24;
static const int WEATHERING_VELOCITY = 25;
static const int SUB_WEATHERING_VELOCITY = 26;
static const int UPHOLE_TIME_AT_SOURCE = 27;
static const int UPHOLE_TIME_AT_GROUP = 28;
static const int SOURCE_STATIC_CORRECTION = 29;
static const int GROUP_STATIC_CORRECTION = 30;
static const int TOTAL_STATIC_CORRECTION = 31;
static const int LAG_TIME_A = 32;
static const int LAG_TIME_B = 33;
static const int DELAY_RECORDING_TIME = 34;
static const int MUTE_TIME_START = 35;
static const int MUTE_TIME_END = 36;
static const int SAMPLES = 37;
static const int SAMPLE_INTERVAL = 38;
static const int GAIN_TYPE = 39;
static const int INSTRUMENT_GAIN_CONSTANT = 40;
static const int INSTRUMENT_INITIAL_GAIN = 41;
static const int CORRELATED = 42;
static const int SWEEP_FREQUENCY_AT_START = 43;
static const int SWEEP_FREQUENCY_AT_END = 44;
static const int SWEEP_LENGTH = 45;
static const int SWEEP_TYPE = 46;
static const int SWEEP_TRACE_TAPER_LENGTH_AT_START = 47;
static const int SWEEP_TRACE_TAPER_LENGTH_AT_END = 48;
static const int TAPER_TYPE = 49;
static const int ALIAS_FILTER_FREQUENCY = 50;
static const int ALIAS_FILTER_SLOPE = 51;
static const int NOTCH_FILTER_FREQUENCY = 52;
static const int NOTCH_FILTER_SLOPE = 53;
static const int LOW_CUT_FREQUENCY = 54;
static const int HIGH_CUT_FREQUENCY = 55;
static const int LOW_CUT_SLOPE = 56;
static const int HIGH_CUT_SLOPE = 57;
static const int YEAR = 58;
static const int DAY_OF_YEAR = 59;
static const int HOUR = 60;
static const int MINUTE = 61;
static const int SECOND = 62;
static const int TIME_BASIC_CODE = 63;
static const int TRACE_WEIGHTING_FACTOR = 64;
static const int GEOPHONE_GROUP_NUMBER_OF_ROLL_SWITH = 65;
static const int GEOPHONE_GROUP_NUMBER_OF_TRACE_NUMBER_ONE = 66;
static const int GEOPHONE_GROUP_NUMBER_OF_LAST_TRACE = 67;
static const int GAP_SIZE = 68;
static const int OVER_TRAVEL = 69;
static const int INLINE_NUMBER = 70;
static const int CROSSLINE_NUMBER = 71;
static const int SHOTPOINT_NUMBER = 72;
static const int SHOTPOINT_SCALAR = 73;

/************************************************************************/
/*                       SEGYReadMSBFloat32()                           */
/************************************************************************/

#ifdef SEGY_EXTENSIONS
static float SEGYReadMSBFloat32(const GByte* pabyVal)
{
    float fVal = 0.0f;
    memcpy(&fVal, pabyVal, 4);
    CPL_MSBPTR32(&fVal);
    return fVal;
}
#endif

/************************************************************************/
/*                           OGRSEGYLayer()                            */
/************************************************************************/

OGRSEGYLayer::OGRSEGYLayer( const char* pszFilename,
                            VSILFILE* fpIn,
                            SEGYBinaryFileHeader* psBFH ) :
    poFeatureDefn(new OGRFeatureDefn(CPLGetBasename(pszFilename))),
    bEOF(false),
    nNextFID(0),
    fp(fpIn),
    nDataSize(0)
{
    memcpy(&sBFH, psBFH, sizeof(sBFH));

    switch( sBFH.nDataSampleType )
    {
        case DT_IBM_4BYTES_FP: nDataSize = 4; break;
        case DT_4BYTES_INT: nDataSize = 4; break;
        case DT_2BYTES_INT: nDataSize = 2; break;
        case DT_4BYTES_FP_WITH_GAIN: nDataSize = 4; break;
        case DT_IEEE_4BYTES_FP: nDataSize = 4; break;
        case DT_1BYTE_INT: nDataSize = 1; break;
        default: break;
    }

    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    for( int i = 0; i < static_cast<int>(sizeof(SEGYFields) /
                                         sizeof(SEGYFields[0]));
         i++ )
    {
        OGRFieldDefn oField( SEGYFields[i].pszName,
                             SEGYFields[i].eType );
        poFeatureDefn->AddFieldDefn( &oField );
    }

    if( sBFH.dfSEGYRevisionNumber >= 1.0 )
    {
        for( int i = 0;
             i < static_cast<int>(sizeof(SEGYFields10) /
                                  sizeof(SEGYFields10[0]));
             i++ )
        {
            OGRFieldDefn oField( SEGYFields10[i].pszName,
                                 SEGYFields10[i].eType );
            poFeatureDefn->AddFieldDefn( &oField );
        }
    }

    OGRFieldDefn oField( "SAMPLE_ARRAY", OFTRealList );
    poFeatureDefn->AddFieldDefn(&oField);

    ResetReading();
}

/************************************************************************/
/*                            ~OGRSEGYLayer()                          */
/************************************************************************/

OGRSEGYLayer::~OGRSEGYLayer()

{
    poFeatureDefn->Release();

    VSIFCloseL( fp );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSEGYLayer::ResetReading()

{
    nNextFID = 0;
    bEOF = false;

    VSIFSeekL( fp, 3200 + 400 + 3200 * sBFH.nNumberOfExtendedTextualFileHeader,
               SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSEGYLayer::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
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
/*                             GetIBMFloat()                            */
/************************************************************************/

static float GetIBMFloat(const GByte* pabyData)
{
    int nVal = 0;
    memcpy(&nVal, pabyData, 4);
    CPL_MSBPTR32(&nVal);
    const int nSign = 1 - 2 * ((nVal >> 31) & 0x01);
    const int nExp = (nVal >> 24) & 0x7f;
    const int nMant = nVal & 0xffffff;

    if( nExp == 0x7f )
    {
        nVal = (nVal & 0x80000000) | (0xff << 23) | (nMant >> 1);
        float fVal = 0;
        memcpy(&fVal, &nVal, 4);
        return fVal;
    }

    return
        static_cast<float>(
            static_cast<double>(nSign) *
            nMant *
            pow(2.0, 4 * (nExp - 64) - 24));
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSEGYLayer::GetNextRawFeature()
{
    if( bEOF )
        return NULL;

    GByte abyTraceHeader[240];

    if( (int)VSIFReadL(abyTraceHeader, 1, 240, fp) != 240 )
    {
        bEOF = true;
        return NULL;
    }

    const int nTraceNumberWithinLine = SEGYReadMSBInt32(abyTraceHeader + 0);
    const int nTraceNumberWithinFile = SEGYReadMSBInt32(abyTraceHeader + 4);
    const int nOriginalFieldRecordNumber = SEGYReadMSBInt32(abyTraceHeader + 8);
    const int nTraceNumberWithinOriginalFieldRecord = SEGYReadMSBInt32(abyTraceHeader + 12);
    const int nEnsembleNumber = SEGYReadMSBInt32(abyTraceHeader + 20);
    const int nTraceNumberWithinEnsemble = SEGYReadMSBInt32(abyTraceHeader + 24);
    const int nTraceIdentificationCode = SEGYReadMSBInt16(abyTraceHeader + 28);
    const int nNumberVerticalSummedTraces = SEGYReadMSBInt16(abyTraceHeader + 30);
    const int nNumberHorizontalStackedTraces = SEGYReadMSBInt16(abyTraceHeader + 32);
    const int nDataUse = SEGYReadMSBInt16(abyTraceHeader + 34);
    const int nDistanceSourceGroup = SEGYReadMSBInt32(abyTraceHeader + 36);
    const int nReceiverGroupElevation = SEGYReadMSBInt32(abyTraceHeader + 40);
    const int nSurfaceElevationAtSource = SEGYReadMSBInt32(abyTraceHeader + 44);
    const int nSourceDepthBelowSurface = SEGYReadMSBInt32(abyTraceHeader + 48);
    const int nDatumElevationAtReceiverGroup = SEGYReadMSBInt32(abyTraceHeader + 52);
    const int nDatumElevationAtSource = SEGYReadMSBInt32(abyTraceHeader + 56);
    const int nWaterDepthAtSource = SEGYReadMSBInt32(abyTraceHeader + 60);
    const int nWaterDepthAtGroup = SEGYReadMSBInt32(abyTraceHeader + 64);
    const int nVerticalScalar = SEGYReadMSBInt16(abyTraceHeader + 68);
    const int nHorizontalScalar = SEGYReadMSBInt16(abyTraceHeader + 70);
    const int nSourceX = SEGYReadMSBInt32(abyTraceHeader + 72);
    const int nSourceY = SEGYReadMSBInt32(abyTraceHeader + 76);
    const int nGroupX = SEGYReadMSBInt32(abyTraceHeader + 80);
    const int nGroupY = SEGYReadMSBInt32(abyTraceHeader + 84);
    const int nCoordinateUnits = SEGYReadMSBInt16(abyTraceHeader + 88);
    const int nWeatheringVelocity = SEGYReadMSBInt16(abyTraceHeader + 90);
    const int nSubWeatheringVelocity = SEGYReadMSBInt16(abyTraceHeader + 92);

    const int nUpholeTimeAtSource = SEGYReadMSBInt16(abyTraceHeader + 94);
    const int nUpholeTimeAtGroup = SEGYReadMSBInt16(abyTraceHeader + 96);
    const int nSourceStaticCorrection = SEGYReadMSBInt16(abyTraceHeader + 98);
    const int nGroupStaticCorrection = SEGYReadMSBInt16(abyTraceHeader + 100);
    const int nTotalStaticCorrection = SEGYReadMSBInt16(abyTraceHeader + 102);
    const int nLagTimeA = SEGYReadMSBInt16(abyTraceHeader + 104);
    const int nLagTimeB = SEGYReadMSBInt16(abyTraceHeader + 106);
    const int nDelayRecordingTime = SEGYReadMSBInt16(abyTraceHeader + 108);
    const int nMuteTimeStart = SEGYReadMSBInt16(abyTraceHeader + 110);
    const int nMuteTimeEnd = SEGYReadMSBInt16(abyTraceHeader + 112);

    int nSamples = SEGYReadMSBInt16(abyTraceHeader + 114);
    // Happens with
    // ftp://software.seg.org/pub/datasets/2D/Hess_VTI/timodel_c11.segy.gz
    if( nSamples == 0 )
        nSamples = sBFH.nSamplesPerDataTrace;

    if( nSamples < 0 )
    {
        bEOF = true;
        return NULL;
    }
    const int nSampleInterval = SEGYReadMSBInt16(abyTraceHeader + 116);

    const int nGainType = SEGYReadMSBInt16(abyTraceHeader + 118);
    const int nInstrumentGainConstant = SEGYReadMSBInt16(abyTraceHeader + 120);
    const int nInstrumentInitialGain = SEGYReadMSBInt16(abyTraceHeader + 122);
    const int nCorrelated = SEGYReadMSBInt16(abyTraceHeader + 124);
    const int nSweepFrequencyAtStart = SEGYReadMSBInt16(abyTraceHeader + 126);
    const int nSweepFrequencyAtEnd = SEGYReadMSBInt16(abyTraceHeader + 128);
    const int nSweepLength = SEGYReadMSBInt16(abyTraceHeader + 130);
    const int nSweepType = SEGYReadMSBInt16(abyTraceHeader + 132);
    const int nSweepTraceTaperLengthAtStart = SEGYReadMSBInt16(abyTraceHeader + 134);
    const int nSweepTraceTaperLengthAtEnd = SEGYReadMSBInt16(abyTraceHeader + 136);
    const int nTaperType = SEGYReadMSBInt16(abyTraceHeader + 138);
    const int nAliasFilterFrequency = SEGYReadMSBInt16(abyTraceHeader + 140);
    const int nAliasFilterSlope = SEGYReadMSBInt16(abyTraceHeader + 142);
    const int nNotchFilterFrequency = SEGYReadMSBInt16(abyTraceHeader + 144);
    const int nNotchFilterSlope = SEGYReadMSBInt16(abyTraceHeader + 146);
    const int nLowCutFrequency = SEGYReadMSBInt16(abyTraceHeader + 148);
    const int nHighCutFrequency = SEGYReadMSBInt16(abyTraceHeader + 150);
    const int nLowCutSlope = SEGYReadMSBInt16(abyTraceHeader + 152);
    const int nHighCutSlope = SEGYReadMSBInt16(abyTraceHeader + 154);

    const int nYear = SEGYReadMSBInt16(abyTraceHeader + 156);
    const int nDayOfYear = SEGYReadMSBInt16(abyTraceHeader + 158);
    const int nHour = SEGYReadMSBInt16(abyTraceHeader + 160);
    const int nMinute = SEGYReadMSBInt16(abyTraceHeader + 162);
    const int nSecond = SEGYReadMSBInt16(abyTraceHeader + 164);
    const int nTimeBasicCode = SEGYReadMSBInt16(abyTraceHeader + 166);

    const int nTraceWeightingFactor = SEGYReadMSBInt16(abyTraceHeader + 168);
    const int nGeophoneGroupNumberOfRollSwith = SEGYReadMSBInt16(abyTraceHeader + 170);
    const int nGeophoneGroupNumberOfTraceNumberOne = SEGYReadMSBInt16(abyTraceHeader + 172);
    const int nGeophoneGroupNumberOfLastTrace = SEGYReadMSBInt16(abyTraceHeader + 174);
    const int nGapSize = SEGYReadMSBInt16(abyTraceHeader + 176);
    const int nOverTravel = SEGYReadMSBInt16(abyTraceHeader + 178);

    const int nInlineNumber = SEGYReadMSBInt32(abyTraceHeader + 188);
    const int nCrosslineNumber = SEGYReadMSBInt32(abyTraceHeader + 192);
    const int nShotpointNumber = SEGYReadMSBInt32(abyTraceHeader + 196);
    const int nShotpointScalar = SEGYReadMSBInt16(abyTraceHeader + 200);

#ifdef SEGY_EXTENSIONS
#if DEBUG_VERBOSE
    // Extensions of http://sioseis.ucsd.edu/segy.header.html
    const float fDeepWaterDelay = SEGYReadMSBFloat32(abyTraceHeader + 180);
    const float fStartMuteTime  = SEGYReadMSBFloat32(abyTraceHeader + 184);
    const float fEndMuteTime  = SEGYReadMSBFloat32(abyTraceHeader + 188);
    const float fSampleInterval  = SEGYReadMSBFloat32(abyTraceHeader + 192);
    const float fWaterBottomTime  = SEGYReadMSBFloat32(abyTraceHeader + 196);
    const int nEndOfRp = SEGYReadMSBInt16(abyTraceHeader + 200);
    // TODO(schwehr): Use the extension vars and move DEBUG_VERBOSE here.
    CPLDebug("SEGY", "fDeepWaterDelay = %f", fDeepWaterDelay);
    CPLDebug("SEGY", "fStartMuteTime = %f", fStartMuteTime);
    CPLDebug("SEGY", "fEndMuteTime = %f", fEndMuteTime);
    CPLDebug("SEGY", "fSampleInterval = %f", fSampleInterval);
    CPLDebug("SEGY", "fWaterBottomTime = %f", fWaterBottomTime);
    CPLDebug("SEGY", "nEndOfRp = %d", nEndOfRp);
#endif  // DEBUG_VERBOSE
#endif  // SEGY_EXTENSIONS

    double dfHorizontalScale =
        (nHorizontalScalar > 0) ? nHorizontalScalar :
        (nHorizontalScalar < 0) ? 1.0 / -nHorizontalScalar : 1.0;
    if( nCoordinateUnits == 2 )
        dfHorizontalScale /= 3600;

    const double dfGroupX = nGroupX * dfHorizontalScale;
    const double dfGroupY = nGroupY * dfHorizontalScale;

#if DEBUG_VERBOSE
    const double dfSourceX = nSourceX * dfHorizontalScale;
    const double dfSourceY = nSourceY * dfHorizontalScale;

    CPLDebug("SEGY", "nTraceNumberWithinLine = %d", nTraceNumberWithinLine);
    CPLDebug("SEGY", "nTraceNumberWithinFile = %d", nTraceNumberWithinFile);
    CPLDebug("SEGY", "nOriginalFieldRecordNumber = %d", nOriginalFieldRecordNumber);
    CPLDebug("SEGY", "nTraceNumberWithinOriginalFieldRecord = %d", nTraceNumberWithinOriginalFieldRecord);
    CPLDebug("SEGY", "nTraceIdentificationCode = %d", nTraceIdentificationCode);
    CPLDebug("SEGY", "nEnsembleNumber = %d", nEnsembleNumber);
    CPLDebug("SEGY", "nTraceNumberWithinEnsemble = %d", nTraceNumberWithinEnsemble);
    CPLDebug("SEGY", "nNumberVerticalSummedTraces = %d", nNumberVerticalSummedTraces);
    CPLDebug("SEGY", "nNumberHorizontalStackedTraces = %d", nNumberHorizontalStackedTraces);
    CPLDebug("SEGY", "nDataUse = %d", nDataUse);
    CPLDebug("SEGY", "nDistanceSourceGroup = %d", nDistanceSourceGroup);
    CPLDebug("SEGY", "nReceiverGroupElevation = %d", nReceiverGroupElevation);
    CPLDebug("SEGY", "nSurfaceElevationAtSource = %d", nSurfaceElevationAtSource);
    CPLDebug("SEGY", "nSourceDepthBelowSurface = %d", nSourceDepthBelowSurface);
    CPLDebug("SEGY", "nDatumElevationAtReceiverGroup = %d", nDatumElevationAtReceiverGroup);
    CPLDebug("SEGY", "nDatumElevationAtSource = %d", nDatumElevationAtSource);
    CPLDebug("SEGY", "nWaterDepthAtSource = %d", nWaterDepthAtSource);
    CPLDebug("SEGY", "nWaterDepthAtGroup = %d", nWaterDepthAtGroup);
    CPLDebug("SEGY", "nVerticalScalar = %d", nVerticalScalar);
    CPLDebug("SEGY", "nHorizontalScalar = %d", nHorizontalScalar);
    CPLDebug("SEGY", "nSourceX = %d", nSourceX);
    CPLDebug("SEGY", "nSourceY = %d", nSourceY);
    CPLDebug("SEGY", "dfSourceX = %f", dfSourceX);
    CPLDebug("SEGY", "dfSourceY = %f", dfSourceY);
    CPLDebug("SEGY", "nGroupX = %d", nGroupX);
    CPLDebug("SEGY", "nGroupY = %d", nGroupY);
    CPLDebug("SEGY", "dfGroupX = %f", dfGroupX);
    CPLDebug("SEGY", "dfGroupY = %f", dfGroupY);
    CPLDebug("SEGY", "nCoordinateUnits = %d", nCoordinateUnits);

    CPLDebug("SEGY", "nWeatheringVelocity = %d", nWeatheringVelocity);
    CPLDebug("SEGY", "nSubWeatheringVelocity = %d", nSubWeatheringVelocity);
    CPLDebug("SEGY", "nUpholeTimeAtSource = %d", nUpholeTimeAtSource);
    CPLDebug("SEGY", "nUpholeTimeAtGroup = %d", nUpholeTimeAtGroup);
    CPLDebug("SEGY", "nSourceStaticCorrection = %d", nSourceStaticCorrection);
    CPLDebug("SEGY", "nGroupStaticCorrection = %d", nGroupStaticCorrection);
    CPLDebug("SEGY", "nTotalStaticCorrection = %d", nTotalStaticCorrection);
    CPLDebug("SEGY", "nLagTimeA = %d", nLagTimeA);
    CPLDebug("SEGY", "nLagTimeB = %d", nLagTimeB);
    CPLDebug("SEGY", "nDelayRecordingTime = %d", nDelayRecordingTime);
    CPLDebug("SEGY", "nMuteTimeStart = %d", nMuteTimeStart);
    CPLDebug("SEGY", "nMuteTimeEnd = %d", nMuteTimeEnd);

    CPLDebug("SEGY", "nSamples = %d", nSamples);
    CPLDebug("SEGY", "nSampleInterval = %d", nSampleInterval);

    CPLDebug("SEGY", "nGainType = %d", nGainType);
    CPLDebug("SEGY", "nInstrumentGainConstant = %d", nInstrumentGainConstant);
    CPLDebug("SEGY", "nInstrumentInitialGain = %d", nInstrumentInitialGain);
    CPLDebug("SEGY", "nCorrelated = %d", nCorrelated);
    CPLDebug("SEGY", "nSweepFrequencyAtStart = %d", nSweepFrequencyAtStart);
    CPLDebug("SEGY", "nSweepFrequencyAtEnd = %d", nSweepFrequencyAtEnd);
    CPLDebug("SEGY", "nSweepLength = %d", nSweepLength);
    CPLDebug("SEGY", "nSweepType = %d", nSweepType);
    CPLDebug("SEGY", "nSweepTraceTaperLengthAtStart = %d", nSweepTraceTaperLengthAtStart);
    CPLDebug("SEGY", "nSweepTraceTaperLengthAtEnd = %d", nSweepTraceTaperLengthAtEnd);
    CPLDebug("SEGY", "nTaperType = %d", nTaperType);
    CPLDebug("SEGY", "nAliasFilterFrequency = %d", nAliasFilterFrequency);
    CPLDebug("SEGY", "nAliasFilterSlope = %d", nAliasFilterSlope);
    CPLDebug("SEGY", "nNotchFilterFrequency = %d", nNotchFilterFrequency);
    CPLDebug("SEGY", "nNotchFilterSlope = %d", nNotchFilterSlope);
    CPLDebug("SEGY", "nLowCutFrequency = %d", nLowCutFrequency);
    CPLDebug("SEGY", "nHighCutFrequency = %d", nHighCutFrequency);
    CPLDebug("SEGY", "nLowCutSlope = %d", nLowCutSlope);
    CPLDebug("SEGY", "nHighCutSlope = %d", nHighCutSlope);
    CPLDebug("SEGY", "nYear = %d", nYear);
    CPLDebug("SEGY", "nDayOfYear = %d", nDayOfYear);
    CPLDebug("SEGY", "nHour = %d", nHour);
    CPLDebug("SEGY", "nMinute = %d", nMinute);
    CPLDebug("SEGY", "nSecond = %d", nSecond);
    CPLDebug("SEGY", "nTimeBasicCode = %d", nTimeBasicCode);
    CPLDebug("SEGY", "nTraceWeightingFactor = %d", nTraceWeightingFactor);
    CPLDebug("SEGY", "nGeophoneGroupNumberOfRollSwith = %d", nGeophoneGroupNumberOfRollSwith);
    CPLDebug("SEGY", "nGeophoneGroupNumberOfTraceNumberOne = %d", nGeophoneGroupNumberOfTraceNumberOne);
    CPLDebug("SEGY", "nGeophoneGroupNumberOfLastTrace = %d", nGeophoneGroupNumberOfLastTrace);
    CPLDebug("SEGY", "nGapSize = %d", nGapSize);
    CPLDebug("SEGY", "nOverTravel = %d", nOverTravel);

    if( sBFH.dfSEGYRevisionNumber >= 1.0 )
    {
        CPLDebug("SEGY", "nInlineNumber = %d", nInlineNumber);
        CPLDebug("SEGY", "nCrosslineNumber = %d", nCrosslineNumber);
        CPLDebug("SEGY", "nShotpointNumber = %d", nShotpointNumber);
        CPLDebug("SEGY", "nShotpointScalar = %d", nShotpointScalar);
    }
#endif

    GByte* pabyData = static_cast<GByte *>(
        VSI_MALLOC_VERBOSE(nDataSize * nSamples));
    double* padfValues = static_cast<double *>(
        VSI_CALLOC_VERBOSE(nSamples, sizeof(double)));
    if( pabyData == NULL || padfValues == NULL )
    {
        VSIFSeekL( fp, nDataSize * nSamples, SEEK_CUR );
    }
    else
    {
        if( static_cast<int>(VSIFReadL(pabyData, nDataSize,
                                       nSamples, fp)) != nSamples )
        {
            bEOF = true;
        }
        for( int i = 0; i < nSamples; i++ )
        {
            switch( sBFH.nDataSampleType )
            {
                case DT_IBM_4BYTES_FP:
                {
                    padfValues[i] = GetIBMFloat(pabyData + i * 4);
                    break;
                }

                case DT_4BYTES_INT:
                {
                    int nVal = 0;
                    memcpy(&nVal, pabyData + i * 4, 4);
                    CPL_MSBPTR32(&nVal);
                    padfValues[i] = nVal;
                    break;
                }

                case DT_2BYTES_INT:
                {
                    GInt16 nVal = 0;
                    memcpy(&nVal, pabyData + i * 2, 2);
                    CPL_MSBPTR16(&nVal);
                    padfValues[i] = nVal;
                    break;
                }

                case DT_IEEE_4BYTES_FP:
                {
                    float fVal = 0.0f;
                    memcpy(&fVal, pabyData + i * 4, 4);
                    CPL_MSBPTR32(&fVal);
                    padfValues[i] = fVal;
                    break;
                }

                case DT_1BYTE_INT:
                {
                    padfValues[i] = ((char*)pabyData)[i];
                    break;
                }

                default:
                    break;
            }
        }
    }
    CPLFree(pabyData);

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetFID(nNextFID ++);
    if( dfGroupX != 0.0 || dfGroupY != 0.0 )
        poFeature->SetGeometryDirectly(new OGRPoint(dfGroupX, dfGroupY));

    poFeature->SetField(TRACE_NUMBER_WITHIN_LINE, nTraceNumberWithinLine);
    poFeature->SetField(TRACE_NUMBER_WITHIN_FILE, nTraceNumberWithinFile);
    poFeature->SetField(ORIGINAL_FIELD_RECORD_NUMBER, nOriginalFieldRecordNumber);
    poFeature->SetField(TRACE_NUMBER_WITHIN_ORIGINAL_FIELD_RECORD, nTraceNumberWithinOriginalFieldRecord);
    poFeature->SetField(TRACE_IDENTIFICATION_CODE, nTraceIdentificationCode);
    poFeature->SetField(ENSEMBLE_NUMBER, nEnsembleNumber);
    poFeature->SetField(TRACE_NUMBER_WITHIN_ENSEMBLE, nTraceNumberWithinEnsemble);
    poFeature->SetField(NUMBER_VERTICAL_SUMMED_TRACES, nNumberVerticalSummedTraces);
    poFeature->SetField(NUMBER_HORIZONTAL_STACKED_TRACES, nNumberHorizontalStackedTraces);
    poFeature->SetField(DATA_USE, nDataUse);
    poFeature->SetField(DISTANCE_SOURCE_GROUP, nDistanceSourceGroup);
    poFeature->SetField(RECEIVER_GROUP_ELEVATION, nReceiverGroupElevation);
    poFeature->SetField(SURFACE_ELEVATION_AT_SOURCE, nSurfaceElevationAtSource);
    poFeature->SetField(SOURCE_DEPTH_BELOW_SURFACE, nSourceDepthBelowSurface);
    poFeature->SetField(DATUM_ELEVATION_AT_RECEIVER_GROUP, nDatumElevationAtReceiverGroup);
    poFeature->SetField(DATUM_ELEVATION_AT_SOURCE, nDatumElevationAtSource);
    poFeature->SetField(WATER_DEPTH_AT_SOURCE, nWaterDepthAtSource);
    poFeature->SetField(WATER_DEPTH_AT_GROUP, nWaterDepthAtGroup);
    poFeature->SetField(VERTICAL_SCALAR, nVerticalScalar);
    poFeature->SetField(HORIZONTAL_SCALAR, nHorizontalScalar);
    poFeature->SetField(SOURCE_X, nSourceX);
    poFeature->SetField(SOURCE_Y, nSourceY);
    poFeature->SetField(GROUP_X, nGroupX);
    poFeature->SetField(GROUP_Y, nGroupY);
    poFeature->SetField(COORDINATE_UNITS, nCoordinateUnits);
    poFeature->SetField(WEATHERING_VELOCITY, nWeatheringVelocity);
    poFeature->SetField(SUB_WEATHERING_VELOCITY, nSubWeatheringVelocity);
    poFeature->SetField(UPHOLE_TIME_AT_SOURCE, nUpholeTimeAtSource);
    poFeature->SetField(UPHOLE_TIME_AT_GROUP, nUpholeTimeAtGroup);
    poFeature->SetField(SOURCE_STATIC_CORRECTION, nSourceStaticCorrection);
    poFeature->SetField(GROUP_STATIC_CORRECTION, nGroupStaticCorrection);
    poFeature->SetField(TOTAL_STATIC_CORRECTION, nTotalStaticCorrection);
    poFeature->SetField(LAG_TIME_A, nLagTimeA);
    poFeature->SetField(LAG_TIME_B, nLagTimeB);
    poFeature->SetField(DELAY_RECORDING_TIME, nDelayRecordingTime);
    poFeature->SetField(MUTE_TIME_START, nMuteTimeStart);
    poFeature->SetField(MUTE_TIME_END, nMuteTimeEnd);
    poFeature->SetField(SAMPLES, nSamples);
    poFeature->SetField(SAMPLE_INTERVAL, nSampleInterval);
    poFeature->SetField(GAIN_TYPE, nGainType);
    poFeature->SetField(INSTRUMENT_GAIN_CONSTANT, nInstrumentGainConstant);
    poFeature->SetField(INSTRUMENT_INITIAL_GAIN, nInstrumentInitialGain);
    poFeature->SetField(CORRELATED, nCorrelated);
    poFeature->SetField(SWEEP_FREQUENCY_AT_START, nSweepFrequencyAtStart);
    poFeature->SetField(SWEEP_FREQUENCY_AT_END, nSweepFrequencyAtEnd);
    poFeature->SetField(SWEEP_LENGTH, nSweepLength);
    poFeature->SetField(SWEEP_TYPE, nSweepType);
    poFeature->SetField(SWEEP_TRACE_TAPER_LENGTH_AT_START, nSweepTraceTaperLengthAtStart);
    poFeature->SetField(SWEEP_TRACE_TAPER_LENGTH_AT_END, nSweepTraceTaperLengthAtEnd);
    poFeature->SetField(TAPER_TYPE, nTaperType);
    poFeature->SetField(ALIAS_FILTER_FREQUENCY, nAliasFilterFrequency);
    poFeature->SetField(ALIAS_FILTER_SLOPE, nAliasFilterSlope);
    poFeature->SetField(NOTCH_FILTER_FREQUENCY, nNotchFilterFrequency);
    poFeature->SetField(NOTCH_FILTER_SLOPE, nNotchFilterSlope);
    poFeature->SetField(LOW_CUT_FREQUENCY, nLowCutFrequency);
    poFeature->SetField(HIGH_CUT_FREQUENCY, nHighCutFrequency);
    poFeature->SetField(LOW_CUT_SLOPE, nLowCutSlope);
    poFeature->SetField(HIGH_CUT_SLOPE, nHighCutSlope);
    poFeature->SetField(YEAR, nYear);
    poFeature->SetField(DAY_OF_YEAR, nDayOfYear);
    poFeature->SetField(HOUR, nHour);
    poFeature->SetField(MINUTE, nMinute);
    poFeature->SetField(SECOND, nSecond);
    poFeature->SetField(TIME_BASIC_CODE, nTimeBasicCode);
    poFeature->SetField(TRACE_WEIGHTING_FACTOR, nTraceWeightingFactor);
    poFeature->SetField(GEOPHONE_GROUP_NUMBER_OF_ROLL_SWITH, nGeophoneGroupNumberOfRollSwith);
    poFeature->SetField(GEOPHONE_GROUP_NUMBER_OF_TRACE_NUMBER_ONE, nGeophoneGroupNumberOfTraceNumberOne);
    poFeature->SetField(GEOPHONE_GROUP_NUMBER_OF_LAST_TRACE, nGeophoneGroupNumberOfLastTrace);
    poFeature->SetField(GAP_SIZE, nGapSize);
    poFeature->SetField(OVER_TRAVEL, nOverTravel);

    if( sBFH.dfSEGYRevisionNumber >= 1.0 )
    {
        poFeature->SetField(INLINE_NUMBER, nInlineNumber);
        poFeature->SetField(CROSSLINE_NUMBER, nCrosslineNumber);
        poFeature->SetField(SHOTPOINT_NUMBER, nShotpointNumber);
        poFeature->SetField(SHOTPOINT_SCALAR, nShotpointScalar);
    }

    if( nSamples > 0 && padfValues != NULL )
        poFeature->SetField(poFeature->GetFieldCount() - 1,
                            nSamples, padfValues);

    CPLFree(padfValues);
    return poFeature;
}

static const FieldDesc SEGYHeaderFields[] =
{
    { "TEXT_HEADER", OFTString },
    { "JOB_ID_NUMBER", OFTInteger },
    { "LINE_NUMBER", OFTInteger },
    { "REEL_NUMBER", OFTInteger },
    { "DATA_TRACES_PER_ENSEMBLE", OFTInteger },
    { "AUX_TRACES_PER_ENSEMBLE", OFTInteger },
    { "SAMPLE_INTERVAL", OFTInteger },
    { "SAMPLE_INTERVAL_ORIGINAL", OFTInteger },
    { "SAMPLES_PER_DATA_TRACE", OFTInteger },
    { "SAMPLES_PER_DATA_TRACE_ORIGINAL", OFTInteger },
    { "DATA_SAMPLE_TYPE", OFTInteger },
    { "ENSEMBLE_FOLD", OFTInteger },
    { "TRACE_SORTING_CODE", OFTInteger },
    { "VERTICAL_SUM_CODE", OFTInteger },
    { "SWEEP_FREQUENCY_AT_START", OFTInteger },
    { "SWEEP_FREQUENCY_AT_END", OFTInteger },
    { "SWEEP_LENGTH", OFTInteger },
    { "SWEEP_TYPE", OFTInteger },
    { "TRACE_NUMBER_OF_SWEEP_CHANNEL", OFTInteger },
    { "SWEEP_TRACE_TAPER_LENGTH_AT_START", OFTInteger },
    { "SWEEP_TRACE_TAPER_LENGTH_AT_END", OFTInteger },
    { "TAPER_TYPE", OFTInteger },
    { "CORRELATED", OFTInteger },
    { "BINARY_GAIN_RECOVERED", OFTInteger },
    { "AMPLITUDE_RECOVERY_METHOD", OFTInteger },
    { "MEASUREMENT_SYSTEM", OFTInteger },
    { "IMPULSE_SIGNAL_POLARITY", OFTInteger },
    { "VIBRATORY_POLARY_CODE", OFTInteger },
    { "SEGY_REVISION_NUMBER", OFTInteger },
    { "SEGY_FLOAT_REVISION_NUMBER", OFTReal },
    { "FIXED_LENGTH_TRACE_FLAG", OFTInteger },
    { "NUMBER_OF_EXTENDED_TEXTUAL_FILE_HEADER", OFTInteger },
};

static const int HEADER_TEXT_HEADER = 0;
static const int HEADER_JOB_ID_NUMBER = 1;
static const int HEADER_LINE_NUMBER = 2;
static const int HEADER_REEL_NUMBER = 3;
static const int HEADER_DATA_TRACES_PER_ENSEMBLE = 4;
static const int HEADER_AUX_TRACES_PER_ENSEMBLE = 5;
static const int HEADER_SAMPLE_INTERVAL = 6;
static const int HEADER_SAMPLE_INTERVAL_ORIGINAL = 7;
static const int HEADER_SAMPLES_PER_DATA_TRACE = 8;
static const int HEADER_SAMPLES_PER_DATA_TRACE_ORIGINAL = 9;
static const int HEADER_DATA_SAMPLE_TYPE = 10;
static const int HEADER_ENSEMBLE_FOLD = 11;
static const int HEADER_TRACE_SORTING_CODE = 12;
static const int HEADER_VERTICAL_SUM_CODE = 13;
static const int HEADER_SWEEP_FREQUENCY_AT_START = 14;
static const int HEADER_SWEEP_FREQUENCY_AT_END = 15;
static const int HEADER_SWEEP_LENGTH = 16;
static const int HEADER_SWEEP_TYPE = 17;
static const int HEADER_TRACE_NUMBER_OF_SWEEP_CHANNEL = 18;
static const int HEADER_SWEEP_TRACE_TAPER_LENGTH_AT_START = 19;
static const int HEADER_SWEEP_TRACE_TAPER_LENGTH_AT_END = 20;
static const int HEADER_TAPER_TYPE = 21;
static const int HEADER_CORRELATED = 22;
static const int HEADER_BINARY_GAIN_RECOVERED = 23;
static const int HEADER_AMPLITUDE_RECOVERY_METHOD = 24;
static const int HEADER_MEASUREMENT_SYSTEM = 25;
static const int HEADER_IMPULSE_SIGNAL_POLARITY = 26;
static const int HEADER_VIBRATORY_POLARY_CODE = 27;
static const int HEADER_SEGY_REVISION_NUMBER = 28;
static const int HEADER_FLOAT_SEGY_REVISION_NUMBER = 29;
static const int HEADER_FIXED_LENGTH_TRACE_FLAG = 30;
static const int HEADER_NUMBER_OF_EXTENDED_TEXTUAL_FILE_HEADER = 31;

/************************************************************************/
/*                         OGRSEGYHeaderLayer()                         */
/************************************************************************/

OGRSEGYHeaderLayer::OGRSEGYHeaderLayer( const char* pszLayerName,
                                        SEGYBinaryFileHeader* psBFH,
                                        const char* pszHeaderTextIn ) :
    poFeatureDefn(new OGRFeatureDefn(pszLayerName)),
    bEOF(false),
    pszHeaderText(CPLStrdup(pszHeaderTextIn))
{
    memcpy(&sBFH, psBFH, sizeof(sBFH));

    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    for( int i = 0;
         i < static_cast<int>(sizeof(SEGYHeaderFields)/
                              sizeof(SEGYHeaderFields[0]));
         i++ )
    {
        OGRFieldDefn oField( SEGYHeaderFields[i].pszName,
                             SEGYHeaderFields[i].eType );
        poFeatureDefn->AddFieldDefn( &oField );
    }

    ResetReading();
}

/************************************************************************/
/*                            ~OGRSEGYLayer()                          */
/************************************************************************/

OGRSEGYHeaderLayer::~OGRSEGYHeaderLayer()

{
    poFeatureDefn->Release();
    CPLFree(pszHeaderText);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSEGYHeaderLayer::ResetReading()

{
    bEOF = false;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSEGYHeaderLayer::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
        {
            delete poFeature;
        }
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSEGYHeaderLayer::GetNextRawFeature()
{
    if( bEOF )
        return NULL;

    bEOF = true;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetFID(0);

    poFeature->SetField(HEADER_TEXT_HEADER, pszHeaderText);
    poFeature->SetField(HEADER_JOB_ID_NUMBER, sBFH.nJobIdNumber);
    poFeature->SetField(HEADER_LINE_NUMBER, sBFH.nLineNumber);
    poFeature->SetField(HEADER_REEL_NUMBER, sBFH.nReelNumber);
    poFeature->SetField(HEADER_DATA_TRACES_PER_ENSEMBLE, sBFH.nDataTracesPerEnsemble);
    poFeature->SetField(HEADER_AUX_TRACES_PER_ENSEMBLE, sBFH.nAuxTracesPerEnsemble);
    poFeature->SetField(HEADER_SAMPLE_INTERVAL, sBFH.nSampleInterval);
    poFeature->SetField(HEADER_SAMPLE_INTERVAL_ORIGINAL, sBFH.nSampleIntervalOriginal);
    poFeature->SetField(HEADER_SAMPLES_PER_DATA_TRACE, sBFH.nSamplesPerDataTrace);
    poFeature->SetField(HEADER_SAMPLES_PER_DATA_TRACE_ORIGINAL, sBFH.nSamplesPerDataTraceOriginal);
    poFeature->SetField(HEADER_DATA_SAMPLE_TYPE, sBFH.nDataSampleType);
    poFeature->SetField(HEADER_ENSEMBLE_FOLD, sBFH.nEnsembleFold);
    poFeature->SetField(HEADER_TRACE_SORTING_CODE, sBFH.nTraceSortingCode);
    poFeature->SetField(HEADER_VERTICAL_SUM_CODE, sBFH.nVerticalSumCode);
    poFeature->SetField(HEADER_SWEEP_FREQUENCY_AT_START, sBFH.nSweepFrequencyAtStart);
    poFeature->SetField(HEADER_SWEEP_FREQUENCY_AT_END, sBFH.nSweepFrequencyAtEnd);
    poFeature->SetField(HEADER_SWEEP_LENGTH, sBFH.nSweepLength);
    poFeature->SetField(HEADER_SWEEP_TYPE, sBFH.nSweepType);
    poFeature->SetField(HEADER_TRACE_NUMBER_OF_SWEEP_CHANNEL, sBFH.nTraceNumberOfSweepChannel);
    poFeature->SetField(HEADER_SWEEP_TRACE_TAPER_LENGTH_AT_START, sBFH.nSweepTraceTaperLengthAtStart);
    poFeature->SetField(HEADER_SWEEP_TRACE_TAPER_LENGTH_AT_END, sBFH.nSweepTraceTaperLengthAtEnd);
    poFeature->SetField(HEADER_TAPER_TYPE, sBFH.nTaperType);
    poFeature->SetField(HEADER_CORRELATED, sBFH.nCorrelated);
    poFeature->SetField(HEADER_BINARY_GAIN_RECOVERED, sBFH.nBinaryGainRecovered);
    poFeature->SetField(HEADER_AMPLITUDE_RECOVERY_METHOD, sBFH.nAmplitudeRecoveryMethod);
    poFeature->SetField(HEADER_MEASUREMENT_SYSTEM, sBFH.nMeasurementSystem);
    poFeature->SetField(HEADER_IMPULSE_SIGNAL_POLARITY, sBFH.nImpulseSignalPolarity);
    poFeature->SetField(HEADER_VIBRATORY_POLARY_CODE, sBFH.nVibratoryPolaryCode);
    poFeature->SetField(HEADER_SEGY_REVISION_NUMBER, sBFH.nSEGYRevisionNumber);
    poFeature->SetField(HEADER_FLOAT_SEGY_REVISION_NUMBER, sBFH.dfSEGYRevisionNumber);
    poFeature->SetField(HEADER_FIXED_LENGTH_TRACE_FLAG, sBFH.nFixedLengthTraceFlag);
    poFeature->SetField(HEADER_NUMBER_OF_EXTENDED_TEXTUAL_FILE_HEADER, sBFH.nNumberOfExtendedTextualFileHeader);

    return poFeature;
}
