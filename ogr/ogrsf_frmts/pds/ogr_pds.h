/******************************************************************************
 *
 * Project:  PDS Translator
 * Purpose:  Definition of classes for OGR .pdstable driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_PDS_H_INCLUDED
#define OGR_PDS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "nasakeywordhandler.h"

namespace OGRPDS
{

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

class OGRPDSLayer final : public OGRLayer,
                          public OGRGetNextFeatureThroughRaw<OGRPDSLayer>
{
    OGRFeatureDefn *poFeatureDefn;

    std::string osTableID;
    VSILFILE *fpPDS;
    int nRecords;
    int nStartBytes;
    int nRecordSize;
    GByte *pabyRecord;
    int nNextFID;
    int nLongitudeIndex;
    int nLatitudeIndex;

    FieldDesc *pasFieldDesc;

    void ReadStructure(const std::string &osStructureFilename);
    OGRFeature *GetNextRawFeature();

    CPL_DISALLOW_COPY_ASSIGN(OGRPDSLayer)

  public:
    OGRPDSLayer(const std::string &osTableID, const char *pszLayerName,
                VSILFILE *fp, const std::string &osLabelFilename,
                const std::string &osStructureFilename, int nRecords,
                int nStartBytes, int nRecordSize, GByte *pabyRecord,
                bool bIsASCII);
    virtual ~OGRPDSLayer();

    virtual void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRPDSLayer)

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;

    virtual OGRFeature *GetFeature(GIntBig nFID) override;

    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;
};

}  // namespace OGRPDS

/************************************************************************/
/*                           OGRPDSDataSource                           */
/************************************************************************/

class OGRPDSDataSource final : public GDALDataset
{
    OGRLayer **papoLayers;
    int nLayers;

    NASAKeywordHandler oKeywords;

    CPLString osTempResult;
    const char *GetKeywordSub(const char *pszPath, int iSubscript,
                              const char *pszDefault);

    bool LoadTable(const char *pszFilename, int nRecordSize,
                   CPLString osTableID);

  public:
    OGRPDSDataSource();
    virtual ~OGRPDSDataSource();

    int Open(const char *pszFilename);

    virtual int GetLayerCount() override
    {
        return nLayers;
    }

    virtual OGRLayer *GetLayer(int) override;

    static void CleanString(CPLString &osInput);
};

#endif /* ndef OGR_PDS_H_INCLUDED */
