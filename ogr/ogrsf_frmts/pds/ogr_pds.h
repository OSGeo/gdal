/******************************************************************************
 * $Id$
 *
 * Project:  PDS Translator
 * Purpose:  Definition of classes for OGR .pdstable driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_PDS_H_INCLUDED
#define OGR_PDS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "nasakeywordhandler.h"

namespace OGRPDS {

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

class OGRPDSLayer final: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRPDSLayer>
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

    CPL_DISALLOW_COPY_ASSIGN(OGRPDSLayer)

  public:
                        OGRPDSLayer(CPLString osTableID,
                                         const char* pszLayerName, VSILFILE* fp,
                                         CPLString osLabelFilename,
                                         CPLString osStructureFilename,
                                         int nRecords,
                                         int nStartBytes, int nRecordSize,
                                         GByte* pabyRecord, bool bIsASCII);
                        virtual ~OGRPDSLayer();

    virtual void                ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRPDSLayer)

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override;

    virtual GIntBig             GetFeatureCount(int bForce = TRUE ) override;

    virtual OGRFeature         *GetFeature( GIntBig nFID ) override;

    virtual OGRErr              SetNextByIndex( GIntBig nIndex ) override;
};

} /* end of OGRPDS namespace */

/************************************************************************/
/*                           OGRPDSDataSource                           */
/************************************************************************/

class OGRPDSDataSource final: public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    NASAKeywordHandler  oKeywords;

    CPLString           osTempResult;
    const char         *GetKeywordSub( const char *pszPath,
                                       int iSubscript,
                                       const char *pszDefault );

    bool                LoadTable( const char* pszFilename,
                                   int nRecordSize,
                                   CPLString osTableID );

  public:
                        OGRPDSDataSource();
                        virtual ~OGRPDSDataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;

    static void         CleanString( CPLString &osInput );
};

#endif /* ndef OGR_PDS_H_INCLUDED */
