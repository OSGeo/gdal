/******************************************************************************
 *
 * Project:  AMIGOCLOUD Translator
 * Purpose:  Definition of classes for OGR AmigoCloud driver.
 * Author:   Victor Chernetsky, <victor at amigocloud dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_AMIGOCLOUD_H_INCLUDED
#define OGR_AMIGOCLOUD_H_INCLUDED

#include "ogrsf_frmts.h"

#include "cpl_json_header.h"
#include "cpl_hash_set.h"
#include "cpl_http.h"

#include <vector>
#include <string>

#include <cstdlib>

json_object *OGRAMIGOCLOUDGetSingleRow(json_object *poObj);
CPLString OGRAMIGOCLOUDEscapeIdentifier(const char *pszStr);
std::string OGRAMIGOCLOUDJsonEncode(const std::string &value);

/************************************************************************/
/*                      OGRAmigoCloudGeomFieldDefn                      */
/************************************************************************/

class OGRAmigoCloudGeomFieldDefn final : public OGRGeomFieldDefn
{
  public:
    int nSRID;

    OGRAmigoCloudGeomFieldDefn(const char *pszNameIn, OGRwkbGeometryType eType)
        : OGRGeomFieldDefn(pszNameIn, eType), nSRID(0)
    {
    }

    ~OGRAmigoCloudGeomFieldDefn() override;
};

class OGRAmigoCloudFID
{
  public:
    GIntBig iIndex;
    GIntBig iFID;
    std::string osAmigoId;

    OGRAmigoCloudFID(const std::string &amigo_id, GIntBig index)
        : iIndex(index),
          iFID(std::abs((long)CPLHashSetHashStr(amigo_id.c_str()))),
          osAmigoId(amigo_id)
    {
    }

    OGRAmigoCloudFID()
    {
        iIndex = 0;
        iFID = 0;
    }

    OGRAmigoCloudFID(const OGRAmigoCloudFID &fid) = default;
    OGRAmigoCloudFID &operator=(const OGRAmigoCloudFID &fid) = default;
};

/************************************************************************/
/*                          OGRAmigoCloudLayer                          */
/************************************************************************/
class OGRAmigoCloudDataSource;

class OGRAmigoCloudLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    OGRAmigoCloudDataSource *poDS;

    OGRFeatureDefn *poFeatureDefn;
    CPLString osBaseSQL;
    CPLString osFIDColName;

    int bEOF;
    int nFetchedObjects;
    int iNextInFetchedObjects;
    GIntBig iNext;
    json_object *poCachedObj;

    std::map<GIntBig, OGRAmigoCloudFID> mFIDs;

    virtual OGRFeature *GetNextRawFeature();
    OGRFeature *BuildFeature(json_object *poRowObj);

    void EstablishLayerDefn(const char *pszLayerName, json_object *poObjIn);
    OGRSpatialReference *GetSRS(const char *pszGeomCol, int *pnSRID);
    virtual CPLString GetSRS_SQL(const char *pszGeomCol) = 0;

  public:
    explicit OGRAmigoCloudLayer(OGRAmigoCloudDataSource *poDS);
    ~OGRAmigoCloudLayer() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    const OGRFeatureDefn *GetLayerDefn() const override;
    virtual OGRFeatureDefn *GetLayerDefnInternal(json_object *poObjIn) = 0;
    virtual json_object *FetchNewFeatures(GIntBig iNext);

    const char *GetFIDColumn() const override
    {
        return osFIDColName.c_str();
    }

    int TestCapability(const char *) const override;

    GDALDataset *GetDataset() override;

    static int GetFeaturesToFetch()
    {
        return 100;
    }
};

/************************************************************************/
/*                       OGRAmigoCloudTableLayer                        */
/************************************************************************/

class OGRAmigoCloudTableLayer final : public OGRAmigoCloudLayer
{
    CPLString osTableName;
    CPLString osName;
    CPLString osDatasetId;
    CPLString osQuery;
    CPLString osWHERE;
    CPLString osSELECTWithoutWHERE;

    std::vector<std::string> vsDeferredInsertChangesets;
    GIntBig nNextFID;

    int bDeferredCreation;
    int nMaxChunkSize;

    void BuildWhere();

    CPLString GetSRS_SQL(const char *pszGeomCol) override;

  public:
    OGRAmigoCloudTableLayer(OGRAmigoCloudDataSource *poDS, const char *pszName);
    ~OGRAmigoCloudTableLayer() override;

    const char *GetName() const override
    {
        return osName.c_str();
    }

    const char *GetTableName()
    {
        return osTableName.c_str();
    }

    const char *GetDatasetId()
    {
        return osDatasetId.c_str();
    }

    OGRFeatureDefn *GetLayerDefnInternal(json_object *poObjIn) override;
    json_object *FetchNewFeatures(GIntBig iNext) override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    int TestCapability(const char *) const override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;

    OGRFeature *GetNextRawFeature() override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *poGeom) override;
    OGRErr SetAttributeFilter(const char *) override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    void SetDeferredCreation(OGRwkbGeometryType eGType,
                             OGRSpatialReference *poSRS, int bGeomNullable);

    static CPLString GetAmigoCloudType(const OGRFieldDefn &oField);

    OGRErr RunDeferredCreationIfNecessary();

    int GetDeferredCreation() const
    {
        return bDeferredCreation;
    }

    void CancelDeferredCreation()
    {
        bDeferredCreation = FALSE;
    }

    void FlushDeferredInsert();
    bool IsDatasetExists();
};

/************************************************************************/
/*                       OGRAmigoCloudResultLayer                       */
/************************************************************************/

class OGRAmigoCloudResultLayer final : public OGRAmigoCloudLayer
{
    OGRFeature *poFirstFeature;

    CPLString GetSRS_SQL(const char *pszGeomCol) override;

  public:
    OGRAmigoCloudResultLayer(OGRAmigoCloudDataSource *poDS,
                             const char *pszRawStatement);
    ~OGRAmigoCloudResultLayer() override;

    OGRFeatureDefn *GetLayerDefnInternal(json_object *poObjIn) override;
    OGRFeature *GetNextRawFeature() override;

    int IsOK();
};

/************************************************************************/
/*                       OGRAmigoCloudDataSource                        */
/************************************************************************/

class OGRAmigoCloudDataSource final : public GDALDataset
{
    char *pszProjectId;

    OGRAmigoCloudTableLayer **papoLayers;
    int nLayers;
    bool bReadWrite;

    bool bUseHTTPS;

    CPLString osAPIKey;

    bool bMustCleanPersistent;

    CPLString osCurrentSchema;
    // TODO(schwehr): Can bHasOGRMetadataFunction be a bool?
    int bHasOGRMetadataFunction;

  public:
    OGRAmigoCloudDataSource();
    ~OGRAmigoCloudDataSource() override;

    int Open(const char *pszFilename, char **papszOpenOptions, int bUpdate);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;
    OGRLayer *GetLayerByName(const char *) override;

    int TestCapability(const char *) const override;

    virtual OGRLayer *ICreateLayer(const char *pszName,
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

    const char *GetProjectId()
    {
        return pszProjectId;
    }

    char **AddHTTPOptions();
    json_object *
    RunPOST(const char *pszURL, const char *pszPostData,
            const char *pszHeaders = "HEADERS=Content-Type: application/json");
    json_object *RunGET(const char *pszURL);
    bool RunDELETE(const char *pszURL);
    json_object *RunSQL(const char *pszUnescapedSQL);

    const CPLString &GetCurrentSchema()
    {
        return osCurrentSchema;
    }

    static int FetchSRSId(OGRSpatialReference *poSRS);

    static std::string GetUserAgentOption();

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

    bool ListDatasets();
    bool waitForJobToFinish(const char *jobId);
    bool TruncateDataset(const CPLString &tableName);
    void SubmitChangeset(const CPLString &json);
};

#endif /* ndef OGR_AMIGOCLOUD_H_INCLUDED */
