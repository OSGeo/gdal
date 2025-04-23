/******************************************************************************
 *
 * Project:  Arc/Info Coverage (E00 & Binary) Reader
 * Purpose:  Declarations for OGR wrapper classes for coverage access.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_AVC_H_INCLUDED
#define OGR_AVC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "avc.h"

constexpr int SERIAL_ACCESS_FID = INT_MIN;

class OGRAVCDataSource;

/************************************************************************/
/*                             OGRAVCLayer                              */
/************************************************************************/

class OGRAVCLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    OGRFeatureDefn *poFeatureDefn;

    OGRAVCDataSource *poDS;

    AVCFileType eSectionType;

    bool m_bEOF = false;

    int SetupFeatureDefinition(const char *pszName);
    bool AppendTableDefinition(AVCTableDef *psTableDef);

    bool MatchesSpatialFilter(void *);
    OGRFeature *TranslateFeature(void *);

    bool TranslateTableFields(OGRFeature *poFeature, int nFieldBase,
                              AVCTableDef *psTableDef, AVCField *pasFields);

  public:
    OGRAVCLayer(AVCFileType eSectionType, OGRAVCDataSource *poDS);
    virtual ~OGRAVCLayer();

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override;
};

/************************************************************************/
/*                         OGRAVCDataSource                             */
/************************************************************************/

class OGRAVCDataSource CPL_NON_FINAL : public GDALDataset
{
  protected:
    bool m_bSRSFetched = false;
    OGRSpatialReference *poSRS;
    char *pszCoverageName;

  public:
    OGRAVCDataSource();
    virtual ~OGRAVCDataSource();

    virtual OGRSpatialReference *DSGetSpatialRef();

    const char *GetCoverageName();
};

/* ==================================================================== */
/*      Binary Coverage Classes                                         */
/* ==================================================================== */

class OGRAVCBinDataSource;

/************************************************************************/
/*                            OGRAVCBinLayer                            */
/************************************************************************/

class OGRAVCBinLayer final : public OGRAVCLayer
{
    AVCE00Section *m_psSection;
    AVCBinFile *hFile;

    OGRAVCBinLayer *poArcLayer;
    bool bNeedReset;

    char szTableName[128];
    AVCBinFile *hTable;
    int nTableBaseField;
    int nTableAttrIndex;

    int nNextFID;

    bool FormPolygonGeometry(OGRFeature *poFeature, AVCPal *psPAL);

    bool CheckSetupTable();
    bool AppendTableFields(OGRFeature *poFeature);

  public:
    OGRAVCBinLayer(OGRAVCBinDataSource *poDS, AVCE00Section *psSectionIn);

    ~OGRAVCBinLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFID) override;

    int TestCapability(const char *) override;
};

/************************************************************************/
/*                         OGRAVCBinDataSource                          */
/************************************************************************/

class OGRAVCBinDataSource final : public OGRAVCDataSource
{
    OGRLayer **papoLayers;
    int nLayers;

    AVCE00ReadPtr psAVC;

  public:
    OGRAVCBinDataSource();
    ~OGRAVCBinDataSource();

    int Open(const char *, int bTestOpen);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;

    int TestCapability(const char *) override;

    AVCE00ReadPtr GetInfo()
    {
        return psAVC;
    }
};

/* ==================================================================== */
/*      E00 (ASCII) Coverage Classes                                    */
/* ==================================================================== */

/************************************************************************/
/*                            OGRAVCE00Layer                            */
/************************************************************************/
class OGRAVCE00Layer final : public OGRAVCLayer
{
    AVCE00Section *psSection;
    AVCE00ReadE00Ptr psRead;
    OGRAVCE00Layer *poArcLayer;
    int nFeatureCount;
    bool bNeedReset;
    bool bLastWasSequential = false;
    int nNextFID;

    AVCE00Section *psTableSection;
    AVCE00ReadE00Ptr psTableRead;
    char *pszTableFilename;
    int nTablePos;
    int nTableBaseField;
    int nTableAttrIndex;

    bool FormPolygonGeometry(OGRFeature *poFeature, AVCPal *psPAL);

  public:
    OGRAVCE00Layer(OGRAVCDataSource *poDS, AVCE00Section *psSectionIn);

    ~OGRAVCE00Layer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    GIntBig GetFeatureCount(int bForce) override;
    bool CheckSetupTable(AVCE00Section *psTblSectionIn);
    bool AppendTableFields(OGRFeature *poFeature);
};

/************************************************************************/
/*                         OGRAVCE00DataSource                          */
/************************************************************************/

class OGRAVCE00DataSource final : public OGRAVCDataSource
{
    int nLayers;
    AVCE00ReadE00Ptr psE00;
    OGRAVCE00Layer **papoLayers;

  protected:
    int CheckAddTable(AVCE00Section *psTblSection);

  public:
    OGRAVCE00DataSource();
    virtual ~OGRAVCE00DataSource();

    int Open(const char *, int bTestOpen);

    AVCE00ReadE00Ptr GetInfo()
    {
        return psE00;
    }

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;
    int TestCapability(const char *) override;
    virtual OGRSpatialReference *DSGetSpatialRef() override;
};

#endif /* OGR_AVC_H_INCLUDED */
