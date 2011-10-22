/******************************************************************************
 * $Id$
 *
 * Project:  SEG-Y Translator
 * Purpose:  Implements OGRSEGYDataSource class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
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
/*                           EBCDICToASCII                              */
/************************************************************************/

static const GByte EBCDICToASCII[] =
{
0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F, 0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
0x10, 0x11, 0x12, 0x13, 0x9D, 0x85, 0x08, 0x87, 0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,
0x80, 0x81, 0x82, 0x83, 0x84, 0x0A, 0x17, 0x1B, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,
0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, 0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,
0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0xAC,
0x2D, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x5C, 0x00, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9F,
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSEGYDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;


// --------------------------------------------------------------------
//      Read the first 3200 bytes, where the Textual File Header is
//      located
// --------------------------------------------------------------------

    GByte* pabyTextHeader = (GByte*) CPLMalloc(3200 + 1);
    GByte* pabyASCIITextHeader = (GByte*) CPLMalloc(3200 + 40 + 1);
    if ((int)VSIFReadL(pabyTextHeader, 1, 3200, fp) != 3200)
    {
        VSIFCloseL(fp);
        CPLFree(pabyTextHeader);
        CPLFree(pabyASCIITextHeader);
        return FALSE;
    }

// --------------------------------------------------------------------
//      Try to decode the header encoded as EBCDIC and then ASCII
// --------------------------------------------------------------------

    int i, j, k;
    for( k=0; k<2; k++)
    {
        for( i=0, j=0;i<3200;i++)
        {
            GByte chASCII = (k == 0) ? EBCDICToASCII[pabyTextHeader[i]] :
                                       pabyTextHeader[i];
            if (chASCII < 32 && chASCII != '\t' &&
                chASCII != '\n' && chASCII != '\r')
            {
                break;
            }
            pabyASCIITextHeader[j++] = chASCII;
            if (chASCII != '\n' && ((i + 1) % 80) == 0)
                pabyASCIITextHeader[j++] = '\n';
        }
        pabyASCIITextHeader[j] = '\0';

        if (i == 3200)
            break;
        if (k == 1)
        {
            VSIFCloseL(fp);
            CPLFree(pabyTextHeader);
            CPLFree(pabyASCIITextHeader);
            return FALSE;
        }
    }

    CPLDebug("SIGY", "Header = \n%s", pabyASCIITextHeader);
    CPLFree(pabyTextHeader);


// --------------------------------------------------------------------
//      SEGYRead the next 400 bytes, where the Binary File Header is
//      located
// --------------------------------------------------------------------

    GByte abyFileHeader[400];
    if ((int)VSIFReadL(abyFileHeader, 1, 400, fp) != 400)
    {
        VSIFCloseL(fp);
        CPLFree(pabyASCIITextHeader);
        return FALSE;
    }

// --------------------------------------------------------------------
//      First check that this binary header is not EBCDIC nor ASCII
// --------------------------------------------------------------------
    for( k=0;k<2;k++ )
    {
        for( i=0;i<400;i++)
        {
            GByte chASCII = (k == 0) ? abyFileHeader[i] :
                                       EBCDICToASCII[abyFileHeader[i]];
            /* A translated 0 value, when source value is not 0, means an invalid */
            /* EBCDIC value. Bail out also for control characters */
            if (chASCII < 32 && chASCII != '\t' &&
                chASCII != '\n' && chASCII != '\r')
            {
                break;
            }
        }
        if (i == 400)
        {
            VSIFCloseL(fp);
            CPLFree(pabyASCIITextHeader);
            return FALSE;
        }
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
    papoLayers[1] = new OGRSEGYHeaderLayer(CPLSPrintf("%s_header", CPLGetBasename(pszName)), &sBFH, (char*) pabyASCIITextHeader);

    return TRUE;
}
