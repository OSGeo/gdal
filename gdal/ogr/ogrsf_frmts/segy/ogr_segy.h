/******************************************************************************
 * $Id$
 *
 * Project:  SEG-Y Translator
 * Purpose:  Definition of classes for OGR SEG-Y driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
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

#ifndef OGR_SEGY_H_INCLUDED
#define OGR_SEGY_H_INCLUDED

#include "ogrsf_frmts.h"

GInt16 SEGYReadMSBInt16(const GByte* pabyVal);
GInt32 SEGYReadMSBInt32(const GByte* pabyVal);

typedef struct
{
    int nJobIdNumber;
    int nLineNumber;
    int nReelNumber;
    int nDataTracesPerEnsemble;
    int nAuxTracesPerEnsemble;
    int nSampleInterval;
    int nSampleIntervalOriginal;
    int nSamplesPerDataTrace;
    int nSamplesPerDataTraceOriginal;
    int nDataSampleType;
    int nEnsembleFold;
    int nTraceSortingCode;
    int nVerticalSumCode;
    int nSweepFrequencyAtStart;
    int nSweepFrequencyAtEnd;
    int nSweepLength;
    int nSweepType;
    int nTraceNumberOfSweepChannel;
    int nSweepTraceTaperLengthAtStart;
    int nSweepTraceTaperLengthAtEnd;
    int nTaperType;
    int nCorrelated;
    int nBinaryGainRecovered;
    int nAmplitudeRecoveryMethod;
    int nMeasurementSystem;
    int nImpulseSignalPolarity;
    int nVibratoryPolaryCode;
    int nSEGYRevisionNumber;
    double dfSEGYRevisionNumber;
    int nFixedLengthTraceFlag;
    int nNumberOfExtendedTextualFileHeader;
} SEGYBinaryFileHeader;

/************************************************************************/
/*                          OGRSEGYLayer                                */
/************************************************************************/

class OGRSEGYLayer: public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    bool               bEOF;
    int                nNextFID;
    VSILFILE*          fp;

    SEGYBinaryFileHeader sBFH;
    int                nDataSize;

    OGRFeature *       GetNextRawFeature();

  public:
                        OGRSEGYLayer( const char* pszFilename,
                                      VSILFILE* fp,
                                      SEGYBinaryFileHeader* psBFH );
                        virtual ~OGRSEGYLayer();

    virtual OGRFeature *        GetNextFeature() override;

    virtual void                ResetReading() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                        OGRSEGYHeaderLayer                            */
/************************************************************************/

class OGRSEGYHeaderLayer: public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    bool               bEOF;

    SEGYBinaryFileHeader sBFH;
    char*                pszHeaderText;

    OGRFeature *       GetNextRawFeature();

  public:
                        OGRSEGYHeaderLayer( const char* pszLayerName,
                                            SEGYBinaryFileHeader* psBFH,
                                            const char* pszHeaderText );
                        virtual ~OGRSEGYHeaderLayer();

    virtual OGRFeature *        GetNextFeature() override;

    virtual void                ResetReading() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                          OGRSEGYDataSource                           */
/************************************************************************/

class OGRSEGYDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

  public:
                        OGRSEGYDataSource();
                        virtual ~OGRSEGYDataSource();

    int                 Open( const char * pszFilename, const char* pszHeaderText );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_SEGY_H_INCLUDED */
