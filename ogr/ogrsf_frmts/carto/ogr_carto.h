/******************************************************************************
 *
 * Project:  CARTO Translator
 * Purpose:  Definition of classes for OGR Carto driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_CARTO_H_INCLUDED
#define OGR_CARTO_H_INCLUDED

#include "ogrsf_frmts.h"

#include "cpl_http.h"
#include "cpl_json_header.h"

#include <vector>

json_object *OGRCARTOGetSingleRow(json_object *poObj);
CPLString OGRCARTOEscapeIdentifier(const char *pszStr);
CPLString OGRCARTOEscapeLiteral(const char *pszStr);
CPLString OGRCARTOEscapeLiteralCopy(const char *pszStr);

/************************************************************************/
/*                        OGRCartoGeomFieldDefn                         */
/************************************************************************/

class OGRCartoGeomFieldDefn final : public OGRGeomFieldDefn
{
  public:
    int nSRID;

    OGRCartoGeomFieldDefn(const char *pszNameIn, OGRwkbGeometryType eType)
        : OGRGeomFieldDefn(pszNameIn, eType), nSRID(0)
    {
    }

    ~OGRCartoGeomFieldDefn() override;
};

/************************************************************************/
/*                            OGRCARTOLayer                             */
/************************************************************************/
class OGRCARTODataSource;

class OGRCARTOLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    OGRCARTODataSource *poDS;

    OGRFeatureDefn *poFeatureDefn;
    CPLString osBaseSQL;
    CPLString osFIDColName;

    bool bEOF;
    int nFetchedObjects;
    int iNextInFetchedObjects;
    GIntBig m_nNextFID;
    GIntBig m_nNextOffset;
    json_object *poCachedObj;

    virtual OGRFeature *GetNextRawFeature();
    OGRFeature *BuildFeature(json_object *poRowObj);

    void EstablishLayerDefn(const char *pszLayerName, json_object *poObjIn);
    OGRSpatialReference *GetSRS(const char *pszGeomCol, int *pnSRID);
    virtual CPLString GetSRS_SQL(const char *pszGeomCol) = 0;

  public:
    explicit OGRCARTOLayer(OGRCARTODataSource *poDS);
    ~OGRCARTOLayer() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    const OGRFeatureDefn *GetLayerDefn() const override;
    virtual OGRFeatureDefn *GetLayerDefnInternal(json_object *poObjIn) = 0;
    virtual json_object *FetchNewFeatures();

    const char *GetFIDColumn() const override
    {
        return osFIDColName.c_str();
    }

    int TestCapability(const char *) const override;

    GDALDataset *GetDataset() override;

    static int GetFeaturesToFetch()
    {
        return atoi(CPLGetConfigOption(
            "CARTO_PAGE_SIZE", CPLGetConfigOption("CARTODB_PAGE_SIZE", "500")));
    }
};

typedef enum
{
    INSERT_UNINIT,
    INSERT_SINGLE_FEATURE,
    INSERT_MULTIPLE_FEATURE
} InsertState;

/************************************************************************/
/*                          OGRCARTOTableLayer                          */
/************************************************************************/

class OGRCARTOTableLayer final : public OGRCARTOLayer
{
    CPLString osName;
    CPLString osQuery;
    CPLString osWHERE;
    CPLString osSELECTWithoutWHERE;

    bool bLaunderColumnNames;

    bool bInDeferredInsert;
    bool bCopyMode;
    InsertState eDeferredInsertState;
    CPLString osDeferredBuffer;
    CPLString osCopySQL;
    GIntBig m_nNextFIDWrite;

    bool bDeferredCreation;
    bool bCartodbfy;
    int nMaxChunkSize;

    bool bDropOnCreation;

    void BuildWhere();
    std::vector<bool> m_abFieldSetForInsert;

    CPLString GetSRS_SQL(const char *pszGeomCol) override;

  public:
    OGRCARTOTableLayer(OGRCARTODataSource *poDS, const char *pszName);
    ~OGRCARTOTableLayer() override;

    const char *GetName() const override
    {
        return osName.c_str();
    }

    OGRFeatureDefn *GetLayerDefnInternal(json_object *poObjIn) override;
    json_object *FetchNewFeatures() override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    int TestCapability(const char *) const override;

    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomFieldIn,
                                   int bApproxOK = TRUE) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;

    OGRErr DeleteField(int iField) override;

    OGRFeature *GetNextRawFeature() override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *) override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    void SetLaunderFlag(bool bFlag)
    {
        bLaunderColumnNames = bFlag;
    }

    void SetDeferredCreation(OGRwkbGeometryType eGType,
                             OGRSpatialReference *poSRS, bool bGeomNullable,
                             bool bCartodbfy);
    OGRErr RunDeferredCreationIfNecessary();

    bool GetDeferredCreation() const
    {
        return bDeferredCreation;
    }

    void CancelDeferredCreation()
    {
        bDeferredCreation = false;
        bCartodbfy = false;
    }

    OGRErr FlushDeferredBuffer(bool bReset = true);
    void RunDeferredCartofy();

    OGRErr FlushDeferredInsert(bool bReset = true);
    OGRErr FlushDeferredCopy(bool bReset = true);
    OGRErr ICreateFeatureInsert(OGRFeature *poFeature,
                                bool bHasUserFieldMatchingFID,
                                bool bHasJustGotNextFID);
    OGRErr ICreateFeatureCopy(OGRFeature *poFeature,
                              bool bHasUserFieldMatchingFID,
                              bool bHasJustGotNextFID);
    char *OGRCARTOGetHexGeometry(OGRGeometry *poGeom, int i);

    void SetDropOnCreation(bool bFlag)
    {
        bDropOnCreation = bFlag;
    }

    bool GetDropOnCreation() const
    {
        return bDropOnCreation;
    }
};

/************************************************************************/
/*                         OGRCARTOResultLayer                          */
/************************************************************************/

class OGRCARTOResultLayer final : public OGRCARTOLayer
{
    OGRFeature *poFirstFeature;

    CPLString GetSRS_SQL(const char *pszGeomCol) override;

  public:
    OGRCARTOResultLayer(OGRCARTODataSource *poDS, const char *pszRawStatement);
    ~OGRCARTOResultLayer() override;

    OGRFeatureDefn *GetLayerDefnInternal(json_object *poObjIn) override;
    OGRFeature *GetNextRawFeature() override;

    bool IsOK();
};

/************************************************************************/
/*                          OGRCARTODataSource                          */
/************************************************************************/

class OGRCARTODataSource final : public GDALDataset
{
    char *pszAccount;

    OGRCARTOTableLayer **papoLayers;
    int nLayers;

    bool bReadWrite;
    bool bBatchInsert;
    bool bCopyMode;

    bool bUseHTTPS;

    CPLString osAPIKey;

    bool bMustCleanPersistent;

    CPLString osCurrentSchema;

    int bHasOGRMetadataFunction;

    int nPostGISMajor;
    int nPostGISMinor;

  public:
    OGRCARTODataSource();
    ~OGRCARTODataSource() override;

    int Open(const char *pszFilename, char **papszOpenOptions, int bUpdate);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;

    int TestCapability(const char *) const override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    OGRErr DeleteLayer(int) override;

    OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    void ReleaseResultSet(OGRLayer *poLayer) override;

    const char *GetAPIURL() const;

    bool IsReadWrite() const
    {
        return bReadWrite;
    }

    bool DoBatchInsert() const
    {
        return bBatchInsert;
    }

    bool DoCopyMode() const
    {
        return bCopyMode;
    }

    char **AddHTTPOptions();
    json_object *RunSQL(const char *pszUnescapedSQL);
    json_object *RunCopyFrom(const char *pszSQL, const char *pszCopyFile);

    const CPLString &GetCurrentSchema()
    {
        return osCurrentSchema;
    }

    static int FetchSRSId(const OGRSpatialReference *poSRS);

    int IsAuthenticatedConnection()
    {
        return !osAPIKey.empty();
    }

    int HasOGRMetadataFunction()
    {
        return bHasOGRMetadataFunction;
    }

    void SetOGRMetadataFunction(int bFlag)
    {
        bHasOGRMetadataFunction = bFlag;
    }

    OGRLayer *ExecuteSQLInternal(const char *pszSQLCommand,
                                 OGRGeometry *poSpatialFilter = nullptr,
                                 const char *pszDialect = nullptr,
                                 bool bRunDeferredActions = false);

    int GetPostGISMajor() const
    {
        return nPostGISMajor;
    }

    int GetPostGISMinor() const
    {
        return nPostGISMinor;
    }
};

#endif /* ndef OGR_CARTO_H_INCLUDED */
