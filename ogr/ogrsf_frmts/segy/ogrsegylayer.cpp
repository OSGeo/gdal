/******************************************************************************
 * $Id$
 *
 * Project:  SEG-Y Translator
 * Purpose:  Implements OGRSEGYLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

CPL_CVSID("$Id$");

#define DT_IBM_4BYTES_FP         1
#define DT_4BYTES_INT            2
#define DT_2BYTES_INT            3
#define DT_4BYTES_FP_WITH_GAIN   4
#define DT_IEEE_4BYTES_FP        5
#define DT_1BYTE_INT             8

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

#define TRACE_NUMBER_WITHIN_LINE 0
#define TRACE_NUMBER_WITHIN_FILE 1
#define ORIGINAL_FIELD_RECORD_NUMBER 2
#define TRACE_NUMBER_WITHIN_ORIGINAL_FIELD_RECORD 3
#define TRACE_IDENTIFICATION_CODE 4
#define ENSEMBLE_NUMBER 5
#define TRACE_NUMBER_WITHIN_ENSEMBLE 6
#define NUMBER_VERTICAL_SUMMED_TRACES 7
#define NUMBER_HORIZONTAL_STACKED_TRACES 8
#define DATA_USE 9
#define DISTANCE_SOURCE_GROUP 10
#define RECEIVER_GROUP_ELEVATION 11
#define SURFACE_ELEVATION_AT_SOURCE 12
#define SOURCE_DEPTH_BELOW_SURFACE 13
#define DATUM_ELEVATION_AT_RECEIVER_GROUP 14
#define DATUM_ELEVATION_AT_SOURCE 15
#define WATER_DEPTH_AT_SOURCE 16
#define WATER_DEPTH_AT_GROUP 17
#define VERTICAL_SCALAR 18
#define HORIZONTAL_SCALAR 19
#define SOURCE_X 20
#define SOURCE_Y 21
#define GROUP_X 22
#define GROUP_Y 23
#define COORDINATE_UNITS 24
#define WEATHERING_VELOCITY 25
#define SUB_WEATHERING_VELOCITY 26
#define UPHOLE_TIME_AT_SOURCE 27
#define UPHOLE_TIME_AT_GROUP 28
#define SOURCE_STATIC_CORRECTION 29
#define GROUP_STATIC_CORRECTION 30
#define TOTAL_STATIC_CORRECTION 31
#define LAG_TIME_A 32
#define LAG_TIME_B 33
#define DELAY_RECORDING_TIME 34
#define MUTE_TIME_START 35
#define MUTE_TIME_END 36
#define SAMPLES 37
#define SAMPLE_INTERVAL 38
#define GAIN_TYPE 39
#define INSTRUMENT_GAIN_CONSTANT 40
#define INSTRUMENT_INITIAL_GAIN 41
#define CORRELATED 42
#define SWEEP_FREQUENCY_AT_START 43
#define SWEEP_FREQUENCY_AT_END 44
#define SWEEP_LENGTH 45
#define SWEEP_TYPE 46
#define SWEEP_TRACE_TAPER_LENGTH_AT_START 47
#define SWEEP_TRACE_TAPER_LENGTH_AT_END 48
#define TAPER_TYPE 49
#define ALIAS_FILTER_FREQUENCY 50
#define ALIAS_FILTER_SLOPE 51
#define NOTCH_FILTER_FREQUENCY 52
#define NOTCH_FILTER_SLOPE 53
#define LOW_CUT_FREQUENCY 54
#define HIGH_CUT_FREQUENCY 55
#define LOW_CUT_SLOPE 56
#define HIGH_CUT_SLOPE 57
#define YEAR 58
#define DAY_OF_YEAR 59
#define HOUR 60
#define MINUTE 61
#define SECOND 62
#define TIME_BASIC_CODE 63
#define TRACE_WEIGHTING_FACTOR 64
#define GEOPHONE_GROUP_NUMBER_OF_ROLL_SWITH 65
#define GEOPHONE_GROUP_NUMBER_OF_TRACE_NUMBER_ONE 66
#define GEOPHONE_GROUP_NUMBER_OF_LAST_TRACE 67
#define GAP_SIZE 68
#define OVER_TRAVEL 69
#define INLINE_NUMBER 70
#define CROSSLINE_NUMBER 71
#define SHOTPOINT_NUMBER 72
#define SHOTPOINT_SCALAR 73

#if 0
/************************************************************************/
/*                       SEGYReadMSBFloat32()                           */
/************************************************************************/

static float SEGYReadMSBFloat32(const GByte* pabyVal)
{
    float fVal;
    memcpy(&fVal, pabyVal, 4);
    CPL_MSBPTR32(&fVal);
    return fVal;
}
#endif

/************************************************************************/
/*                           OGRSEGYLayer()                            */
/************************************************************************/


OGRSEGYLayer::OGRSEGYLayer( const char* pszFilename,
                            VSILFILE* fp,
                            SEGYBinaryFileHeader* psBFH )

{
    this->fp = fp;
    nNextFID = 0;
    bEOF = FALSE;
    poSRS = NULL;
    memcpy(&sBFH, psBFH, sizeof(sBFH));

    nDataSize = 0;
    switch (sBFH.nDataSampleType)
    {
        case DT_IBM_4BYTES_FP: nDataSize = 4; break;
        case DT_4BYTES_INT: nDataSize = 4; break;
        case DT_2BYTES_INT: nDataSize = 2; break;
        case DT_4BYTES_FP_WITH_GAIN: nDataSize = 4; break;
        case DT_IEEE_4BYTES_FP: nDataSize = 4; break;
        case DT_1BYTE_INT: nDataSize = 1; break;
        default: break;
    }

    poFeatureDefn = new OGRFeatureDefn( CPLGetBasename(pszFilename) );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    int i;
    for(i=0;i<(int)(sizeof(SEGYFields)/sizeof(SEGYFields[0]));i++)
    {
        OGRFieldDefn    oField( SEGYFields[i].pszName,
                                SEGYFields[i].eType );
        poFeatureDefn->AddFieldDefn( &oField );
    }

    if (sBFH.dfSEGYRevisionNumber >= 1.0)
    {
        for(i=0;i<(int)(sizeof(SEGYFields10)/sizeof(SEGYFields10[0]));i++)
        {
            OGRFieldDefn    oField( SEGYFields10[i].pszName,
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

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSEGYLayer::ResetReading()

{
    nNextFID = 0;
    bEOF = FALSE;

    VSIFSeekL( fp, 3200 + 400 + 3200 * sBFH.nNumberOfExtendedTextualFileHeader,
               SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSEGYLayer::GetNextFeature()
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
/*                             GetIBMFloat()                            */
/************************************************************************/

static float GetIBMFloat(const GByte* pabyData)
{
    int nVal;
    memcpy(&nVal, pabyData, 4);
    CPL_MSBPTR32(&nVal);
    int nSign = 1 - 2 * ((nVal >> 31) & 0x01);
    int nExp = (nVal >> 24) & 0x7f;
    int nMant = nVal & 0xffffff;

    if (nExp == 0x7f)
    {
        nVal = (nVal & 0x80000000) | (0xff << 23) | (nMant >> 1);
        float fVal;
        memcpy(&fVal, &nVal, 4);
        return fVal;
    }

    return (float)((double)nSign * nMant * pow(2.0, 4 * (nExp - 64) - 24));
}
/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSEGYLayer::GetNextRawFeature()
{
    if (bEOF)
        return NULL;

    GByte abyTraceHeader[240];

    if ((int)VSIFReadL(abyTraceHeader, 1, 240, fp) != 240)
    {
        bEOF = TRUE;
        return NULL;
    }

    int nTraceNumberWithinLine = SEGYReadMSBInt32(abyTraceHeader + 0);
    int nTraceNumberWithinFile = SEGYReadMSBInt32(abyTraceHeader + 4);
    int nOriginalFieldRecordNumber = SEGYReadMSBInt32(abyTraceHeader + 8);
    int nTraceNumberWithinOriginalFieldRecord = SEGYReadMSBInt32(abyTraceHeader + 12);
    int nEnsembleNumber = SEGYReadMSBInt32(abyTraceHeader + 20);
    int nTraceNumberWithinEnsemble = SEGYReadMSBInt32(abyTraceHeader + 24);
    int nTraceIdentificationCode = SEGYReadMSBInt16(abyTraceHeader + 28);
    int nNumberVerticalSummedTraces = SEGYReadMSBInt16(abyTraceHeader + 30);
    int nNumberHorizontalStackedTraces = SEGYReadMSBInt16(abyTraceHeader + 32);
    int nDataUse = SEGYReadMSBInt16(abyTraceHeader + 34);
    int nDistanceSourceGroup = SEGYReadMSBInt32(abyTraceHeader + 36);
    int nReceiverGroupElevation = SEGYReadMSBInt32(abyTraceHeader + 40);
    int nSurfaceElevationAtSource = SEGYReadMSBInt32(abyTraceHeader + 44);
    int nSourceDepthBelowSurface = SEGYReadMSBInt32(abyTraceHeader + 48);
    int nDatumElevationAtReceiverGroup = SEGYReadMSBInt32(abyTraceHeader + 52);
    int nDatumElevationAtSource = SEGYReadMSBInt32(abyTraceHeader + 56);
    int nWaterDepthAtSource = SEGYReadMSBInt32(abyTraceHeader + 60);
    int nWaterDepthAtGroup = SEGYReadMSBInt32(abyTraceHeader + 64);
    int nVerticalScalar = SEGYReadMSBInt16(abyTraceHeader + 68);
    int nHorizontalScalar = SEGYReadMSBInt16(abyTraceHeader + 70);
    int nSourceX = SEGYReadMSBInt32(abyTraceHeader + 72);
    int nSourceY = SEGYReadMSBInt32(abyTraceHeader + 76);
    int nGroupX = SEGYReadMSBInt32(abyTraceHeader + 80);
    int nGroupY = SEGYReadMSBInt32(abyTraceHeader + 84);
    int nCoordinateUnits = SEGYReadMSBInt16(abyTraceHeader + 88);
    int nWeatheringVelocity = SEGYReadMSBInt16(abyTraceHeader + 90);
    int nSubWeatheringVelocity = SEGYReadMSBInt16(abyTraceHeader + 92);

    int nUpholeTimeAtSource = SEGYReadMSBInt16(abyTraceHeader + 94);
    int nUpholeTimeAtGroup = SEGYReadMSBInt16(abyTraceHeader + 96);
    int nSourceStaticCorrection = SEGYReadMSBInt16(abyTraceHeader + 98);
    int nGroupStaticCorrection = SEGYReadMSBInt16(abyTraceHeader + 100);
    int nTotalStaticCorrection = SEGYReadMSBInt16(abyTraceHeader + 102);
    int nLagTimeA = SEGYReadMSBInt16(abyTraceHeader + 104);
    int nLagTimeB = SEGYReadMSBInt16(abyTraceHeader + 106);
    int nDelayRecordingTime = SEGYReadMSBInt16(abyTraceHeader + 108);
    int nMuteTimeStart = SEGYReadMSBInt16(abyTraceHeader + 110);
    int nMuteTimeEnd = SEGYReadMSBInt16(abyTraceHeader + 112);

    int nSamples = SEGYReadMSBInt16(abyTraceHeader + 114);
    if (nSamples == 0) /* Happens with ftp://software.seg.org/pub/datasets/2D/Hess_VTI/timodel_c11.segy.gz */
        nSamples = sBFH.nSamplesPerDataTrace;

    if (nSamples < 0)
    {
        bEOF = TRUE;
        return NULL;
    }
    int nSampleInterval = SEGYReadMSBInt16(abyTraceHeader + 116);

    int nGainType = SEGYReadMSBInt16(abyTraceHeader + 118);
    int nInstrumentGainConstant = SEGYReadMSBInt16(abyTraceHeader + 120);
    int nInstrumentInitialGain = SEGYReadMSBInt16(abyTraceHeader + 122);
    int nCorrelated = SEGYReadMSBInt16(abyTraceHeader + 124);
    int nSweepFrequencyAtStart = SEGYReadMSBInt16(abyTraceHeader + 126);
    int nSweepFrequencyAtEnd = SEGYReadMSBInt16(abyTraceHeader + 128);
    int nSweepLength = SEGYReadMSBInt16(abyTraceHeader + 130);
    int nSweepType = SEGYReadMSBInt16(abyTraceHeader + 132);
    int nSweepTraceTaperLengthAtStart = SEGYReadMSBInt16(abyTraceHeader + 134);
    int nSweepTraceTaperLengthAtEnd = SEGYReadMSBInt16(abyTraceHeader + 136);
    int nTaperType = SEGYReadMSBInt16(abyTraceHeader + 138);
    int nAliasFilterFrequency = SEGYReadMSBInt16(abyTraceHeader + 140);
    int nAliasFilterSlope = SEGYReadMSBInt16(abyTraceHeader + 142);
    int nNotchFilterFrequency = SEGYReadMSBInt16(abyTraceHeader + 144);
    int nNotchFilterSlope = SEGYReadMSBInt16(abyTraceHeader + 146);
    int nLowCutFrequency = SEGYReadMSBInt16(abyTraceHeader + 148);
    int nHighCutFrequency = SEGYReadMSBInt16(abyTraceHeader + 150);
    int nLowCutSlope = SEGYReadMSBInt16(abyTraceHeader + 152);
    int nHighCutSlope = SEGYReadMSBInt16(abyTraceHeader + 154);

    int nYear = SEGYReadMSBInt16(abyTraceHeader + 156);
    int nDayOfYear = SEGYReadMSBInt16(abyTraceHeader + 158);
    int nHour = SEGYReadMSBInt16(abyTraceHeader + 160);
    int nMinute = SEGYReadMSBInt16(abyTraceHeader + 162);
    int nSecond = SEGYReadMSBInt16(abyTraceHeader + 164);
    int nTimeBasicCode = SEGYReadMSBInt16(abyTraceHeader + 166);

    int nTraceWeightingFactor = SEGYReadMSBInt16(abyTraceHeader + 168);
    int nGeophoneGroupNumberOfRollSwith = SEGYReadMSBInt16(abyTraceHeader + 170);
    int nGeophoneGroupNumberOfTraceNumberOne = SEGYReadMSBInt16(abyTraceHeader + 172);
    int nGeophoneGroupNumberOfLastTrace = SEGYReadMSBInt16(abyTraceHeader + 174);
    int nGapSize = SEGYReadMSBInt16(abyTraceHeader + 176);
    int nOverTravel = SEGYReadMSBInt16(abyTraceHeader + 178);

    int nInlineNumber = SEGYReadMSBInt32(abyTraceHeader + 188);
    int nCrosslineNumber = SEGYReadMSBInt32(abyTraceHeader + 192);
    int nShotpointNumber = SEGYReadMSBInt32(abyTraceHeader + 196);
    int nShotpointScalar = SEGYReadMSBInt16(abyTraceHeader + 200);

#if 0
    /* Extensions of http://sioseis.ucsd.edu/segy.header.html */
    float fDeepWaterDelay = SEGYReadMSBFloat32(abyTraceHeader + 180);
    float fStartMuteTime  = SEGYReadMSBFloat32(abyTraceHeader + 184);
    float fEndMuteTime  = SEGYReadMSBFloat32(abyTraceHeader + 188);
    float fSampleInterval  = SEGYReadMSBFloat32(abyTraceHeader + 192);
    float fWaterBottomTime  = SEGYReadMSBFloat32(abyTraceHeader + 196);
    int nEndOfRp = SEGYReadMSBInt16(abyTraceHeader + 200);
    CPLDebug("SIGY", "fDeepWaterDelay = %f", fDeepWaterDelay);
    CPLDebug("SIGY", "fStartMuteTime = %f", fStartMuteTime);
    CPLDebug("SIGY", "fEndMuteTime = %f", fEndMuteTime);
    CPLDebug("SIGY", "fSampleInterval = %f", fSampleInterval);
    CPLDebug("SIGY", "fWaterBottomTime = %f", fWaterBottomTime);
    CPLDebug("SIGY", "nEndOfRp = %d", nEndOfRp);
#endif

    double dfHorizontalScale = (nHorizontalScalar > 0) ? nHorizontalScalar :
                               (nHorizontalScalar < 0) ? 1.0 / -nHorizontalScalar : 1.0;
    if (nCoordinateUnits == 2)
        dfHorizontalScale /= 3600;

    double dfGroupX = nGroupX * dfHorizontalScale;
    double dfGroupY = nGroupY * dfHorizontalScale;

#if 0
    double dfSourceX = nSourceX * dfHorizontalScale;
    double dfSourceY = nSourceY * dfHorizontalScale;
#endif

#if 0
    CPLDebug("SIGY", "nTraceNumberWithinLine = %d", nTraceNumberWithinLine);
    CPLDebug("SIGY", "nTraceNumberWithinFile = %d", nTraceNumberWithinFile);
    CPLDebug("SIGY", "nOriginalFieldRecordNumber = %d", nOriginalFieldRecordNumber);
    CPLDebug("SIGY", "nTraceNumberWithinOriginalFieldRecord = %d", nTraceNumberWithinOriginalFieldRecord);
    CPLDebug("SIGY", "nTraceIdentificationCode = %d", nTraceIdentificationCode);
    CPLDebug("SIGY", "nEnsembleNumber = %d", nEnsembleNumber);
    CPLDebug("SIGY", "nTraceNumberWithinEnsemble = %d", nTraceNumberWithinEnsemble);
    CPLDebug("SIGY", "nNumberVerticalSummedTraces = %d", nNumberVerticalSummedTraces);
    CPLDebug("SIGY", "nNumberHorizontalStackedTraces = %d", nNumberHorizontalStackedTraces);
    CPLDebug("SIGY", "nDataUse = %d", nDataUse);
    CPLDebug("SIGY", "nDistanceSourceGroup = %d", nDistanceSourceGroup);
    CPLDebug("SIGY", "nReceiverGroupElevation = %d", nReceiverGroupElevation);
    CPLDebug("SIGY", "nSurfaceElevationAtSource = %d", nSurfaceElevationAtSource);
    CPLDebug("SIGY", "nSourceDepthBelowSurface = %d", nSourceDepthBelowSurface);
    CPLDebug("SIGY", "nDatumElevationAtReceiverGroup = %d", nDatumElevationAtReceiverGroup);
    CPLDebug("SIGY", "nDatumElevationAtSource = %d", nDatumElevationAtSource);
    CPLDebug("SIGY", "nWaterDepthAtSource = %d", nWaterDepthAtSource);
    CPLDebug("SIGY", "nWaterDepthAtGroup = %d", nWaterDepthAtGroup);
    CPLDebug("SIGY", "nVerticalScalar = %d", nVerticalScalar);
    CPLDebug("SIGY", "nHorizontalScalar = %d", nHorizontalScalar);
    CPLDebug("SIGY", "nSourceX = %d", nSourceX);
    CPLDebug("SIGY", "nSourceY = %d", nSourceY);
    CPLDebug("SIGY", "dfSourceX = %f", dfSourceX);
    CPLDebug("SIGY", "dfSourceY = %f", dfSourceY);
    CPLDebug("SIGY", "nGroupX = %d", nGroupX);
    CPLDebug("SIGY", "nGroupY = %d", nGroupY);
    CPLDebug("SIGY", "dfGroupX = %f", dfGroupX);
    CPLDebug("SIGY", "dfGroupY = %f", dfGroupY);
    CPLDebug("SIGY", "nCoordinateUnits = %d", nCoordinateUnits);

    CPLDebug("SIGY", "nWeatheringVelocity = %d", nWeatheringVelocity);
    CPLDebug("SIGY", "nSubWeatheringVelocity = %d", nSubWeatheringVelocity);
    CPLDebug("SIGY", "nUpholeTimeAtSource = %d", nUpholeTimeAtSource);
    CPLDebug("SIGY", "nUpholeTimeAtGroup = %d", nUpholeTimeAtGroup);
    CPLDebug("SIGY", "nSourceStaticCorrection = %d", nSourceStaticCorrection);
    CPLDebug("SIGY", "nGroupStaticCorrection = %d", nGroupStaticCorrection);
    CPLDebug("SIGY", "nTotalStaticCorrection = %d", nTotalStaticCorrection);
    CPLDebug("SIGY", "nLagTimeA = %d", nLagTimeA);
    CPLDebug("SIGY", "nLagTimeB = %d", nLagTimeB);
    CPLDebug("SIGY", "nDelayRecordingTime = %d", nDelayRecordingTime);
    CPLDebug("SIGY", "nMuteTimeStart = %d", nMuteTimeStart);
    CPLDebug("SIGY", "nMuteTimeEnd = %d", nMuteTimeEnd);

    CPLDebug("SIGY", "nSamples = %d", nSamples);
    CPLDebug("SIGY", "nSampleInterval = %d", nSampleInterval);

    CPLDebug("SIGY", "nGainType = %d", nGainType);
    CPLDebug("SIGY", "nInstrumentGainConstant = %d", nInstrumentGainConstant);
    CPLDebug("SIGY", "nInstrumentInitialGain = %d", nInstrumentInitialGain);
    CPLDebug("SIGY", "nCorrelated = %d", nCorrelated);
    CPLDebug("SIGY", "nSweepFrequencyAtStart = %d", nSweepFrequencyAtStart);
    CPLDebug("SIGY", "nSweepFrequencyAtEnd = %d", nSweepFrequencyAtEnd);
    CPLDebug("SIGY", "nSweepLength = %d", nSweepLength);
    CPLDebug("SIGY", "nSweepType = %d", nSweepType);
    CPLDebug("SIGY", "nSweepTraceTaperLengthAtStart = %d", nSweepTraceTaperLengthAtStart);
    CPLDebug("SIGY", "nSweepTraceTaperLengthAtEnd = %d", nSweepTraceTaperLengthAtEnd);
    CPLDebug("SIGY", "nTaperType = %d", nTaperType);
    CPLDebug("SIGY", "nAliasFilterFrequency = %d", nAliasFilterFrequency);
    CPLDebug("SIGY", "nAliasFilterSlope = %d", nAliasFilterSlope);
    CPLDebug("SIGY", "nNotchFilterFrequency = %d", nNotchFilterFrequency);
    CPLDebug("SIGY", "nNotchFilterSlope = %d", nNotchFilterSlope);
    CPLDebug("SIGY", "nLowCutFrequency = %d", nLowCutFrequency);
    CPLDebug("SIGY", "nHighCutFrequency = %d", nHighCutFrequency);
    CPLDebug("SIGY", "nLowCutSlope = %d", nLowCutSlope);
    CPLDebug("SIGY", "nHighCutSlope = %d", nHighCutSlope);
    CPLDebug("SIGY", "nYear = %d", nYear);
    CPLDebug("SIGY", "nDayOfYear = %d", nDayOfYear);
    CPLDebug("SIGY", "nHour = %d", nHour);
    CPLDebug("SIGY", "nMinute = %d", nMinute);
    CPLDebug("SIGY", "nSecond = %d", nSecond);
    CPLDebug("SIGY", "nTimeBasicCode = %d", nTimeBasicCode);
    CPLDebug("SIGY", "nTraceWeightingFactor = %d", nTraceWeightingFactor);
    CPLDebug("SIGY", "nGeophoneGroupNumberOfRollSwith = %d", nGeophoneGroupNumberOfRollSwith);
    CPLDebug("SIGY", "nGeophoneGroupNumberOfTraceNumberOne = %d", nGeophoneGroupNumberOfTraceNumberOne);
    CPLDebug("SIGY", "nGeophoneGroupNumberOfLastTrace = %d", nGeophoneGroupNumberOfLastTrace);
    CPLDebug("SIGY", "nGapSize = %d", nGapSize);
    CPLDebug("SIGY", "nOverTravel = %d", nOverTravel);

    if (sBFH.dfSEGYRevisionNumber >= 1.0)
    {
        CPLDebug("SIGY", "nInlineNumber = %d", nInlineNumber);
        CPLDebug("SIGY", "nCrosslineNumber = %d", nCrosslineNumber);
        CPLDebug("SIGY", "nShotpointNumber = %d", nShotpointNumber);
        CPLDebug("SIGY", "nShotpointScalar = %d", nShotpointScalar);
    }
#endif

    GByte* pabyData = (GByte*) VSIMalloc( nDataSize * nSamples );
    double* padfValues = (double*) VSICalloc( nSamples, sizeof(double) );
    if (pabyData == NULL || padfValues == NULL)
    {
        VSIFSeekL( fp, nDataSize * nSamples, SEEK_CUR );
    }
    else
    {
        if ((int)VSIFReadL(pabyData, nDataSize, nSamples, fp) != nSamples)
        {
            bEOF = TRUE;
        }
        for(int i=0;i<nSamples;i++)
        {
            switch (sBFH.nDataSampleType)
            {
                case DT_IBM_4BYTES_FP:
                {
                    padfValues[i] = GetIBMFloat(pabyData + i * 4);
                    break;
                }

                case DT_4BYTES_INT:
                {
                    int nVal;
                    memcpy(&nVal, pabyData + i * 4, 4);
                    CPL_MSBPTR32(&nVal);
                    padfValues[i] = nVal;
                    break;
                }

                case DT_2BYTES_INT:
                {
                    GInt16 nVal;
                    memcpy(&nVal, pabyData + i * 2, 2);
                    CPL_MSBPTR16(&nVal);
                    padfValues[i] = nVal;
                    break;
                }

                case DT_IEEE_4BYTES_FP:
                {
                    float fVal;
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
    if (dfGroupX != 0.0 || dfGroupY != 0.0)
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

    if (sBFH.dfSEGYRevisionNumber >= 1.0)
    {
        poFeature->SetField(INLINE_NUMBER, nInlineNumber);
        poFeature->SetField(CROSSLINE_NUMBER, nCrosslineNumber);
        poFeature->SetField(SHOTPOINT_NUMBER, nShotpointNumber);
        poFeature->SetField(SHOTPOINT_SCALAR, nShotpointScalar);
    }

    if (nSamples > 0 && padfValues != NULL)
        poFeature->SetField(poFeature->GetFieldCount() - 1, nSamples, padfValues);

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

#define HEADER_TEXT_HEADER 0
#define HEADER_JOB_ID_NUMBER 1
#define HEADER_LINE_NUMBER 2
#define HEADER_REEL_NUMBER 3
#define HEADER_DATA_TRACES_PER_ENSEMBLE 4
#define HEADER_AUX_TRACES_PER_ENSEMBLE 5
#define HEADER_SAMPLE_INTERVAL 6
#define HEADER_SAMPLE_INTERVAL_ORIGINAL 7
#define HEADER_SAMPLES_PER_DATA_TRACE 8
#define HEADER_SAMPLES_PER_DATA_TRACE_ORIGINAL 9
#define HEADER_DATA_SAMPLE_TYPE 10
#define HEADER_ENSEMBLE_FOLD 11
#define HEADER_TRACE_SORTING_CODE 12
#define HEADER_VERTICAL_SUM_CODE 13
#define HEADER_SWEEP_FREQUENCY_AT_START 14
#define HEADER_SWEEP_FREQUENCY_AT_END 15
#define HEADER_SWEEP_LENGTH 16
#define HEADER_SWEEP_TYPE 17
#define HEADER_TRACE_NUMBER_OF_SWEEP_CHANNEL 18
#define HEADER_SWEEP_TRACE_TAPER_LENGTH_AT_START 19
#define HEADER_SWEEP_TRACE_TAPER_LENGTH_AT_END 20
#define HEADER_TAPER_TYPE 21
#define HEADER_CORRELATED 22
#define HEADER_BINARY_GAIN_RECOVERED 23
#define HEADER_AMPLITUDE_RECOVERY_METHOD 24
#define HEADER_MEASUREMENT_SYSTEM 25
#define HEADER_IMPULSE_SIGNAL_POLARITY 26
#define HEADER_VIBRATORY_POLARY_CODE 27
#define HEADER_SEGY_REVISION_NUMBER 28
#define HEADER_FLOAT_SEGY_REVISION_NUMBER 29
#define HEADER_FIXED_LENGTH_TRACE_FLAG 30
#define HEADER_NUMBER_OF_EXTENDED_TEXTUAL_FILE_HEADER 31


/************************************************************************/
/*                         OGRSEGYHeaderLayer()                         */
/************************************************************************/


OGRSEGYHeaderLayer::OGRSEGYHeaderLayer( const char* pszLayerName,
                                        SEGYBinaryFileHeader* psBFH,
                                        char* pszHeaderTextIn )

{
    bEOF = FALSE;
    memcpy(&sBFH, psBFH, sizeof(sBFH));
    pszHeaderText = pszHeaderTextIn;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    int i;
    for(i=0;i<(int)(sizeof(SEGYHeaderFields)/sizeof(SEGYHeaderFields[0]));i++)
    {
        OGRFieldDefn    oField( SEGYHeaderFields[i].pszName,
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
    bEOF = FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSEGYHeaderLayer::GetNextFeature()
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

OGRFeature *OGRSEGYHeaderLayer::GetNextRawFeature()
{
    if (bEOF)
        return NULL;

    bEOF = TRUE;

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
