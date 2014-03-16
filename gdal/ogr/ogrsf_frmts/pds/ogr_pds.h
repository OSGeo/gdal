/******************************************************************************
 * $Id$
 *
 * Project:  PDS Translator
 * Purpose:  Definition of classes for OGR .pdstable driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_PDS_H_INCLUDED
#define _OGR_PDS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "nasakeywordhandler.h"

/************************************************************************/
/*                              OGRPDSLayer                             */
/************************************************************************/

typedef enum
{
    ASCII_REAL,
    ASCII_INTEGER,
    CHARACTER,
    MSB_INTEGER,
    MSB_UNSIGNED_INTEGER,
    IEEE_REAL,
} FieldFormat;

typedef struct
{
    int nStartByte;
    int nByteCount;
    FieldFormat eFormat;
    int nItemBytes;
    int nItems;
} FieldDesc;

class OGRPDSLayer : public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;

    CPLString          osTableID;
    VSILFILE*          fpPDS;
    int                nRecords;
    int                nStartBytes;
    int                nRecordSize;
    GByte             *pabyRecord;
    int                nNextFID;
    int                nLongitudeIndex;
    int                nLatitudeIndex;

    FieldDesc*         pasFieldDesc;
    
    void               ReadStructure(CPLString osStructureFilename);
    OGRFeature        *GetNextRawFeature();

  public:
                        OGRPDSLayer(CPLString osTableID,
                                         const char* pszLayerName, VSILFILE* fp,
                                         CPLString osLabelFilename,
                                         CPLString osStructureFilename,
                                         int nRecords,
                                         int nStartBytes, int nRecordSize,
                                         GByte* pabyRecord, int bIsASCII);
                        ~OGRPDSLayer();


    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int                 TestCapability( const char * );

    virtual int                 GetFeatureCount(int bForce = TRUE );

    virtual OGRFeature         *GetFeature( long nFID );

    virtual OGRErr              SetNextByIndex( long nIndex );
};

/************************************************************************/
/*                           OGRPDSDataSource                           */
/************************************************************************/

class OGRPDSDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    NASAKeywordHandler  oKeywords;

    CPLString           osTempResult;
    const char         *GetKeywordSub( const char *pszPath, 
                                       int iSubscript,
                                       const char *pszDefault );

    int                 LoadTable(const char* pszFilename,
                                  int nRecordSize,
                                  CPLString osTableID);

  public:
                        OGRPDSDataSource();
                        ~OGRPDSDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );

    static void         CleanString( CPLString &osInput );
};

/************************************************************************/
/*                              OGRPDSDriver                            */
/************************************************************************/

class OGRPDSDriver : public OGRSFDriver
{
  public:
                ~OGRPDSDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_PDS_H_INCLUDED */
