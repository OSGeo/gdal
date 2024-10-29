/******************************************************************************
 * $Id$
 *
 * Project:  VDV Translator
 * Purpose:  Implements OGRVDVDriver.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_VDV_H_INCLUDED
#define OGR_VDV_H_INCLUDED

#include "ogrsf_frmts.h"
#include <vector>
#include <map>

class OGRVDVDataSource;

/************************************************************************/
/*                        OGRIDFDataSource                              */
/************************************************************************/

class OGRIDFDataSource final : public GDALDataset
{
    CPLString m_osFilename;
    VSILFILE *m_fpL;
    bool m_bHasParsed;
    GDALDataset *m_poTmpDS;
    bool m_bDestroyTmpDS = false;

    void Parse();

  public:
    explicit OGRIDFDataSource(const char *pszFilename, VSILFILE *fpL);
    virtual ~OGRIDFDataSource();

    virtual int GetLayerCount() override;
    virtual OGRLayer *GetLayer(int) override;
    virtual int TestCapability(const char *pszCap) override;
};

/************************************************************************/
/*                          OGRVDVLayer                                 */
/************************************************************************/

class OGRVDVLayer final : public OGRLayer
{
    GDALDataset *m_poDS = nullptr;
    VSILFILE *m_fpL;
    bool m_bOwnFP;
    bool m_bRecodeFromLatin1;
    vsi_l_offset m_nStartOffset;
    vsi_l_offset m_nCurOffset;
    GIntBig m_nTotalFeatureCount;
    GIntBig m_nFID;
    OGRFeatureDefn *m_poFeatureDefn;
    bool m_bEOF;
    int m_iLongitudeVDV452;
    int m_iLatitudeVDV452;

  public:
    OGRVDVLayer(GDALDataset *poDS, const CPLString &osTableName, VSILFILE *fpL,
                bool bOwnFP, bool bRecodeFromLatin1, vsi_l_offset nStartOffset);
    virtual ~OGRVDVLayer();

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual GIntBig GetFeatureCount(int bForce) override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    virtual int TestCapability(const char *pszCap) override;

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }

    void SetFeatureCount(GIntBig nTotalFeatureCount)
    {
        m_nTotalFeatureCount = nTotalFeatureCount;
    }
};

class OGRVDV452Field
{
  public:
    CPLString osEnglishName;
    CPLString osGermanName;
    CPLString osType;
    int nWidth;

    OGRVDV452Field() : nWidth(0)
    {
    }
};

class OGRVDV452Table
{
  public:
    CPLString osEnglishName;
    CPLString osGermanName;
    std::vector<OGRVDV452Field> aosFields;
};

class OGRVDV452Tables
{
  public:
    std::vector<OGRVDV452Table *> aosTables;
    std::map<CPLString, OGRVDV452Table *> oMapEnglish;
    std::map<CPLString, OGRVDV452Table *> oMapGerman;

    OGRVDV452Tables()
    {
    }

    ~OGRVDV452Tables()
    {
        for (size_t i = 0; i < aosTables.size(); i++)
            delete aosTables[i];
    }
};

/************************************************************************/
/*                          OGRVDVWriterLayer                           */
/************************************************************************/

class OGRVDVWriterLayer final : public OGRLayer
{
    OGRVDVDataSource *m_poDS;
    OGRFeatureDefn *m_poFeatureDefn;
    bool m_bWritePossible;
    VSILFILE *m_fpL;
    bool m_bOwnFP;
    GIntBig m_nFeatureCount;
    OGRVDV452Table *m_poVDV452Table;
    CPLString m_osVDV452Lang;
    bool m_bProfileStrict;
    int m_iLongitudeVDV452;
    int m_iLatitudeVDV452;

    bool WriteSchemaIfNeeded();

  public:
    OGRVDVWriterLayer(OGRVDVDataSource *poDS, const char *pszName,
                      VSILFILE *fpL, bool bOwnFP,
                      OGRVDV452Table *poVDV452Table = nullptr,
                      const CPLString &osVDV452Lang = "",
                      bool bProfileStrict = false);
    virtual ~OGRVDVWriterLayer();

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    virtual int TestCapability(const char *pszCap) override;
    virtual OGRErr CreateField(const OGRFieldDefn *poFieldDefn,
                               int bApproxOK = TRUE) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;

    GDALDataset *GetDataset() override;

    void StopAsCurrentLayer();
};

/************************************************************************/
/*                        OGRVDVDataSource                              */
/************************************************************************/

class OGRVDVDataSource final : public GDALDataset
{
    CPLString m_osFilename;
    VSILFILE *m_fpL;
    bool m_bUpdate;
    bool m_bSingleFile;
    bool m_bNew;
    bool m_bLayersDetected;
    int m_nLayerCount;
    OGRLayer **m_papoLayers;
    OGRVDVWriterLayer *m_poCurrentWriterLayer;
    bool m_bMustWriteEof;
    bool m_bVDV452Loaded;
    OGRVDV452Tables m_oVDV452Tables;

    void DetectLayers();

  public:
    OGRVDVDataSource(const char *pszFilename, VSILFILE *fpL, bool bUpdate,
                     bool bSingleFile, bool bNew);
    virtual ~OGRVDVDataSource();

    virtual int GetLayerCount() override;
    virtual OGRLayer *GetLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    virtual int TestCapability(const char *pszCap) override;

    void SetCurrentWriterLayer(OGRVDVWriterLayer *poLayer);

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Create(const char *pszName, int /*nXSize*/,
                               int /*nYSize*/, int /*nBands*/,
                               GDALDataType /*eType*/, char **papszOptions);
};

#endif /* ndef OGR_VDV_H_INCLUDED */
