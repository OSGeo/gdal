/******************************************************************************
 * $Id$
 *
 * Project:  SEG-Y Translator
 * Purpose:  Implements OGRSEGYDataSource class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRSEGYDataSource()                       */
/************************************************************************/

OGRSEGYDataSource::OGRSEGYDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

/************************************************************************/
/*                       ~OGRSEGYDataSource()                       */
/************************************************************************/

OGRSEGYDataSource::~OGRSEGYDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSEGYDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSEGYDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                        SEGYReadMSBInt16()                            */
/************************************************************************/

GInt16 SEGYReadMSBInt16(const GByte* pabyVal)
{
    GInt16 nVal;
    memcpy(&nVal, pabyVal, 2);
    CPL_MSBPTR16(&nVal);
    return nVal;
}

/************************************************************************/
/*                        SEGYReadMSBInt32()                            */
/************************************************************************/

GInt32 SEGYReadMSBInt32(const GByte* pabyVal)
{
    GInt32 nVal;
    memcpy(&nVal, pabyVal, 4);
    CPL_MSBPTR32(&nVal);
    return nVal;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSEGYDataSource::Open( const char * pszFilename, const char* pszASCIITextHeader)

{
    pszName = CPLStrdup( pszFilename );

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    VSIFSeekL(fp, 3200, SEEK_SET);

// --------------------------------------------------------------------
//      Read the next 400 bytes, where the Binary File Header is
//      located
// --------------------------------------------------------------------

    GByte abyFileHeader[400];
    if ((int)VSIFReadL(abyFileHeader, 1, 400, fp) != 400)
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    SEGYBinaryFileHeader sBFH;

    sBFH.nJobIdNumber = SEGYReadMSBInt32(abyFileHeader + 0);
    sBFH.nLineNumber = SEGYReadMSBInt32(abyFileHeader + 4);
    sBFH.nReelNumber = SEGYReadMSBInt32(abyFileHeader + 8);
    sBFH.nDataTracesPerEnsemble = SEGYReadMSBInt16(abyFileHeader + 12);
    sBFH.nAuxTracesPerEnsemble = SEGYReadMSBInt16(abyFileHeader + 14);
    sBFH.nSampleInterval = SEGYReadMSBInt16(abyFileHeader + 16);
    sBFH.nSampleIntervalOriginal = SEGYReadMSBInt16(abyFileHeader + 18);
    sBFH.nSamplesPerDataTrace = SEGYReadMSBInt16(abyFileHeader + 20);
    sBFH.nSamplesPerDataTraceOriginal = SEGYReadMSBInt16(abyFileHeader + 22);
    sBFH.nDataSampleType = SEGYReadMSBInt16(abyFileHeader + 24);
    sBFH.nEnsembleFold = SEGYReadMSBInt16(abyFileHeader + 26);
    sBFH.nTraceSortingCode = SEGYReadMSBInt16(abyFileHeader + 28);
    sBFH.nVerticalSumCode = SEGYReadMSBInt16(abyFileHeader + 30);
    sBFH.nSweepFrequencyAtStart = SEGYReadMSBInt16(abyFileHeader + 32);
    sBFH.nSweepFrequencyAtEnd = SEGYReadMSBInt16(abyFileHeader + 34);
    sBFH.nSweepLength = SEGYReadMSBInt16(abyFileHeader + 36);
    sBFH.nSweepType = SEGYReadMSBInt16(abyFileHeader + 38);
    sBFH.nTraceNumberOfSweepChannel = SEGYReadMSBInt16(abyFileHeader + 40);
    sBFH.nSweepTraceTaperLengthAtStart = SEGYReadMSBInt16(abyFileHeader + 42);
    sBFH.nSweepTraceTaperLengthAtEnd = SEGYReadMSBInt16(abyFileHeader + 44);
    sBFH.nTaperType = SEGYReadMSBInt16(abyFileHeader + 46);
    sBFH.nCorrelated = SEGYReadMSBInt16(abyFileHeader + 48);
    sBFH.nBinaryGainRecovered = SEGYReadMSBInt16(abyFileHeader + 50);
    sBFH.nAmplitudeRecoveryMethod = SEGYReadMSBInt16(abyFileHeader + 52);
    sBFH.nMeasurementSystem = SEGYReadMSBInt16(abyFileHeader + 54);
    sBFH.nImpulseSignalPolarity = SEGYReadMSBInt16(abyFileHeader + 56);
    sBFH.nVibratoryPolaryCode = SEGYReadMSBInt16(abyFileHeader + 58);
    sBFH.nSEGYRevisionNumber = SEGYReadMSBInt16(abyFileHeader + 300) & 0xffff;
    sBFH.dfSEGYRevisionNumber = sBFH.nSEGYRevisionNumber / 256.0;
    sBFH.nFixedLengthTraceFlag = SEGYReadMSBInt16(abyFileHeader + 302);
    sBFH.nNumberOfExtendedTextualFileHeader = SEGYReadMSBInt16(abyFileHeader + 304);

#if 0
    CPLDebug("SIGY", "nJobIdNumber = %d", sBFH.nJobIdNumber);
    CPLDebug("SIGY", "nLineNumber = %d", sBFH.nLineNumber);
    CPLDebug("SIGY", "nReelNumber = %d", sBFH.nReelNumber);
    CPLDebug("SIGY", "nDataTracesPerEnsemble = %d", sBFH.nDataTracesPerEnsemble);
    CPLDebug("SIGY", "nAuxTracesPerEnsemble = %d", sBFH.nAuxTracesPerEnsemble);
    CPLDebug("SIGY", "nSampleInterval = %d", sBFH.nSampleInterval);
    CPLDebug("SIGY", "nSampleIntervalOriginal = %d", sBFH.nSampleIntervalOriginal);
    CPLDebug("SIGY", "nSamplesPerDataTrace = %d", sBFH.nSamplesPerDataTrace);
    CPLDebug("SIGY", "nSamplesPerDataTraceOriginal = %d", sBFH.nSamplesPerDataTraceOriginal);
    CPLDebug("SIGY", "nDataSampleType = %d", sBFH.nDataSampleType);
    CPLDebug("SIGY", "nEnsembleFold = %d", sBFH.nEnsembleFold);
    CPLDebug("SIGY", "nTraceSortingCode = %d", sBFH.nTraceSortingCode);
    CPLDebug("SIGY", "nVerticalSumCode = %d", sBFH.nVerticalSumCode);
    CPLDebug("SIGY", "nSweepFrequencyAtStart = %d", sBFH.nSweepFrequencyAtStart);
    CPLDebug("SIGY", "nSweepFrequencyAtEnd = %d", sBFH.nSweepFrequencyAtEnd);
    CPLDebug("SIGY", "nSweepLength = %d", sBFH.nSweepLength);
    CPLDebug("SIGY", "nSweepType = %d", sBFH.nSweepType);
    CPLDebug("SIGY", "nTraceNumberOfSweepChannel = %d", sBFH.nTraceNumberOfSweepChannel);
    CPLDebug("SIGY", "nSweepTraceTaperLengthAtStart = %d", sBFH.nSweepTraceTaperLengthAtStart);
    CPLDebug("SIGY", "nSweepTraceTaperLengthAtEnd = %d", sBFH.nSweepTraceTaperLengthAtEnd);
    CPLDebug("SIGY", "nTaperType = %d", sBFH.nTaperType);
    CPLDebug("SIGY", "nCorrelated = %d", sBFH.nCorrelated);
    CPLDebug("SIGY", "nBinaryGainRecovered = %d", sBFH.nBinaryGainRecovered);
    CPLDebug("SIGY", "nAmplitudeRecoveryMethod = %d", sBFH.nAmplitudeRecoveryMethod);
    CPLDebug("SIGY", "nMeasurementSystem = %d", sBFH.nMeasurementSystem);
    CPLDebug("SIGY", "nImpulseSignalPolarity = %d", sBFH.nImpulseSignalPolarity);
    CPLDebug("SIGY", "nVibratoryPolaryCode = %d", sBFH.nVibratoryPolaryCode);
    CPLDebug("SIGY", "nSEGYRevisionNumber = %d", sBFH.nSEGYRevisionNumber);
    CPLDebug("SIGY", "dfSEGYRevisionNumber = %f", sBFH.dfSEGYRevisionNumber);
    CPLDebug("SIGY", "nFixedLengthTraceFlag = %d", sBFH.nFixedLengthTraceFlag);
    CPLDebug("SIGY", "nNumberOfExtendedTextualFileHeader = %d", sBFH.nNumberOfExtendedTextualFileHeader);
#endif

// --------------------------------------------------------------------
//      Create layer
// --------------------------------------------------------------------

    nLayers = 2;
    papoLayers = (OGRLayer**) CPLMalloc(nLayers * sizeof(OGRLayer*));
    papoLayers[0] = new OGRSEGYLayer(pszName, fp, &sBFH);
    papoLayers[1] = new OGRSEGYHeaderLayer(CPLSPrintf("%s_header", CPLGetBasename(pszName)), &sBFH, pszASCIITextHeader);

    return TRUE;
}
