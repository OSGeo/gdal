/******************************************************************************
 *
 * Project:  WFS Translator
 * Purpose:  Definition of classes for OGR WFS driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_WFS_H_INCLUDED
#define OGR_WFS_H_INCLUDED

#include <vector>
#include <set>
#include <map>

#include "cpl_minixml.h"
#include "ogrsf_frmts.h"
#include "gmlfeature.h"
#include "cpl_http.h"
#include "ogr_swq.h"

const CPLXMLNode *WFSFindNode(const CPLXMLNode *psXML, const char *pszRootName);

const char *FindSubStringInsensitive(const char *pszStr, const char *pszSubStr);

CPLString WFS_EscapeURL(const char *pszURL);
CPLString WFS_DecodeURL(const CPLString &osSrc);

class OGRWFSSortDesc
{
  public:
    CPLString osColumn;
    bool bAsc;

    OGRWFSSortDesc(const CPLString &osColumnIn, int bAscIn)
        : osColumn(osColumnIn), bAsc(CPL_TO_BOOL(bAscIn))
    {
    }
};

/************************************************************************/
/*                             OGRWFSLayer                              */
/************************************************************************/

class OGRWFSDataSource;

class OGRWFSLayer final : public OGRLayer
{
    OGRWFSDataSource *poDS = nullptr;

    OGRFeatureDefn *poFeatureDefn = nullptr;
    bool bGotApproximateLayerDefn = false;
    GMLFeatureClass *poGMLFeatureClass = nullptr;

    int bAxisOrderAlreadyInverted = false;
    OGRSpatialReference *m_poSRS = nullptr;
    std::string m_osSRSName{};

    char *pszBaseURL = nullptr;
    char *pszName = nullptr;
    char *pszNS = nullptr;
    char *pszNSVal = nullptr;

    bool bStreamingDS = false;
    GDALDataset *poBaseDS = nullptr;
    OGRLayer *poBaseLayer = nullptr;
    bool bHasFetched = false;
    bool bReloadNeeded = false;

    CPLString osGeometryColumnName{};
    OGRwkbGeometryType eGeomType = wkbUnknown;
    GIntBig nFeatures = -1;
    GIntBig m_nNumberMatched = -1;
    bool m_bHasReadAtLeastOneFeatureInThisPage = false;
    bool bCountFeaturesInGetNextFeature = false;

    int CanRunGetFeatureCountAndGetExtentTogether();

    CPLString MakeGetFeatureURL(int nMaxFeatures, int bRequestHits);
    bool MustRetryIfNonCompliantServer(const char *pszServerAnswer);
    GDALDataset *FetchGetFeature(int nMaxFeatures);
    OGRFeatureDefn *DescribeFeatureType();
    GIntBig ExecuteGetFeatureResultTypeHits();

    OGREnvelope m_oWGS84Extents{};
    OGREnvelope m_oExtents{};

    OGRGeometry *poFetchedFilterGeom = nullptr;

    CPLString osSQLWhere{};
    CPLString osWFSWhere{};

    CPLString osTargetNamespace{};
    CPLString GetDescribeFeatureTypeURL(int bWithNS);

    int nExpectedInserts = 0;
    CPLString osGlobalInsert{};
    std::vector<CPLString> aosFIDList{};

    bool bInTransaction = false;

    CPLString GetPostHeader();

    bool bUseFeatureIdAtLayerLevel = false;

    bool bPagingActive = false;
    int nPagingStartIndex = 0;
    int nFeatureRead = 0;

    OGRFeatureDefn *BuildLayerDefnFromFeatureClass(GMLFeatureClass *poClass);

    char *pszRequiredOutputFormat = nullptr;

    std::vector<OGRWFSSortDesc> aoSortColumns{};

    std::vector<std::string> m_aosSupportedCRSList{};
    OGRLayer::GetSupportedSRSListRetType m_apoSupportedCRSList{};

    std::string m_osTmpDir{};

    void BuildLayerDefn();

    CPL_DISALLOW_COPY_ASSIGN(OGRWFSLayer)

  public:
    OGRWFSLayer(OGRWFSDataSource *poDS, OGRSpatialReference *poSRS,
                int bAxisOrderAlreadyInverted, const char *pszBaseURL,
                const char *pszName, const char *pszNS, const char *pszNSVal);

    ~OGRWFSLayer() override;

    OGRWFSLayer *Clone();

    const char *GetName() const override
    {
        return pszName;
    }

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFID) override;

    const OGRFeatureDefn *GetLayerDefn() const override;

    int TestCapability(const char *) const override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *) override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;

    void SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
    void SetWGS84Extents(double dfMinX, double dfMinY, double dfMaxX,
                         double dfMaxY);
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;

    OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    int HasLayerDefn()
    {
        return poFeatureDefn != nullptr;
    }

    OGRFeatureDefn *ParseSchema(const CPLXMLNode *psSchema);
    OGRFeatureDefn *BuildLayerDefn(OGRFeatureDefn *poSrcFDefn);

    OGRErr DeleteFromFilter(const std::string &osOGCFilter);

    const std::vector<CPLString> &GetLastInsertedFIDList()
    {
        return aosFIDList;
    }

    const char *GetShortName();

    void SetRequiredOutputFormat(const char *pszRequiredOutputFormatIn);

    const char *GetRequiredOutputFormat()
    {
        return pszRequiredOutputFormat;
    }

    void SetOrderBy(const std::vector<OGRWFSSortDesc> &aoSortColumnsIn);

    bool HasGotApproximateLayerDefn()
    {
        GetLayerDefn();
        return bGotApproximateLayerDefn;
    }

    const char *GetNamespacePrefix()
    {
        return pszNS;
    }

    const char *GetNamespaceName()
    {
        return pszNSVal;
    }

    void SetSupportedSRSList(
        std::vector<std::string> &&aosSupportedCRSList,
        OGRLayer::GetSupportedSRSListRetType &&apoSupportedCRSList)
    {
        m_aosSupportedCRSList = std::move(aosSupportedCRSList);
        m_apoSupportedCRSList = std::move(apoSupportedCRSList);
    }

    const OGRLayer::GetSupportedSRSListRetType &
    GetSupportedSRSList(int /*iGeomField*/) override
    {
        return m_apoSupportedCRSList;
    }

    OGRErr SetActiveSRS(int iGeomField,
                        const OGRSpatialReference *poSRS) override;

    const std::string &GetTmpDir() const
    {
        return m_osTmpDir;
    }
};

/************************************************************************/
/*                           OGRWFSJoinLayer                            */
/************************************************************************/

class OGRWFSJoinLayer final : public OGRLayer
{
    OGRWFSDataSource *poDS = nullptr;
    OGRFeatureDefn *poFeatureDefn = nullptr;

    CPLString osGlobalFilter{};
    CPLString osSortBy{};
    bool bDistinct = false;
    std::set<CPLString> aoSetMD5{};

    std::vector<OGRWFSLayer *> apoLayers{};

    GDALDataset *poBaseDS = nullptr;
    OGRLayer *poBaseLayer = nullptr;
    bool bReloadNeeded = false;
    bool bHasFetched = false;

    bool bPagingActive = false;
    int nPagingStartIndex = 0;
    int nFeatureRead = 0;
    int nFeatureCountRequested = 0;

    std::vector<CPLString> aoSrcFieldNames{};
    std::vector<CPLString> aoSrcGeomFieldNames{};

    CPLString osFeatureTypes{};

    std::string m_osTmpDir{};

    OGRWFSJoinLayer(OGRWFSDataSource *poDS, const swq_select *psSelectInfo,
                    const CPLString &osGlobalFilter);
    CPLString MakeGetFeatureURL(int bRequestHits = FALSE);
    GDALDataset *FetchGetFeature();
    GIntBig ExecuteGetFeatureResultTypeHits();

    CPL_DISALLOW_COPY_ASSIGN(OGRWFSJoinLayer)

  public:
    static OGRWFSJoinLayer *Build(OGRWFSDataSource *poDS,
                                  const swq_select *psSelectInfo);
    ~OGRWFSJoinLayer() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    const OGRFeatureDefn *GetLayerDefn() const override;

    int TestCapability(const char *) const override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *) override;
};

/************************************************************************/
/*                           OGRWFSDataSource                           */
/************************************************************************/

class OGRWFSDataSource final : public GDALDataset
{
    static constexpr int DEFAULT_BASE_START_INDEX = 0;
    static constexpr int DEFAULT_PAGE_SIZE = 100;

    bool bRewriteFile = false;
    CPLXMLNode *psFileXML = nullptr;

    OGRWFSLayer **papoLayers = nullptr;
    int nLayers = 0;
    std::map<OGRLayer *, OGRLayer *> oMap{};

    bool bUpdate = false;

    bool bGetFeatureSupportHits = false;
    CPLString osVersion{};
    bool bNeedNAMESPACE = false;
    bool bHasMinOperators = false;
    bool bHasNullCheck = false;
    // Advertized by deegree but not implemented.
    bool bPropertyIsNotEqualToSupported = true;
    // CubeWerx doesn't like GmlObjectId.
    bool bUseFeatureId = false;
    bool bGmlObjectIdNeedsGMLPrefix = false;
    bool bRequiresEnvelopeSpatialFilter = false;
    static bool DetectRequiresEnvelopeSpatialFilter(const CPLXMLNode *psRoot);

    bool bTransactionSupport = false;
    char **papszIdGenMethods = nullptr;
    bool DetectTransactionSupport(const CPLXMLNode *psRoot);

    CPLString osBaseURL{};
    CPLString osPostTransactionURL{};

    CPLXMLNode *LoadFromFile(const char *pszFilename);

    bool bUseHttp10 = false;

    char **papszHttpOptions = nullptr;

    bool bPagingAllowed = false;
    int nPageSize = DEFAULT_PAGE_SIZE;
    int nBaseStartIndex = DEFAULT_BASE_START_INDEX;
    bool DetectSupportPagingWFS2(const CPLXMLNode *psGetCapabilitiesResponse,
                                 const CPLXMLNode *psConfigurationRoot);

    bool bStandardJoinsWFS2 = false;
    bool DetectSupportStandardJoinsWFS2(const CPLXMLNode *psRoot);

    bool bLoadMultipleLayerDefn = false;
    std::set<CPLString> aoSetAlreadyTriedLayers{};

    CPLString osLayerMetadataCSV{};
    CPLString osLayerMetadataTmpFileName{};
    GDALDataset *poLayerMetadataDS = nullptr;
    OGRLayer *poLayerMetadataLayer = nullptr;

    CPLString osGetCapabilities{};
    const char *apszGetCapabilities[2] = {nullptr, nullptr};
    GDALDataset *poLayerGetCapabilitiesDS = nullptr;
    OGRLayer *poLayerGetCapabilitiesLayer = nullptr;

    bool bKeepLayerNamePrefix = false;

    bool bEmptyAsNull = true;

    bool bInvertAxisOrderIfLatLong = true;
    CPLString osConsiderEPSGAsURN{};
    bool bExposeGMLId = true;

    CPLHTTPResult *SendGetCapabilities(const char *pszBaseURL,
                                       CPLString &osTypeName);

    int GetLayerIndex(const char *pszName);

    CPL_DISALLOW_COPY_ASSIGN(OGRWFSDataSource)

  public:
    OGRWFSDataSource();
    ~OGRWFSDataSource() override;

    int Open(const char *pszFilename, int bUpdate,
             CSLConstList papszOpenOptions);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;
    OGRLayer *GetLayerByName(const char *pszLayerName) override;

    OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    void ReleaseResultSet(OGRLayer *poResultsSet) override;

    bool UpdateMode() const
    {
        return bUpdate;
    }

    bool SupportTransactions() const
    {
        return bTransactionSupport;
    }

    void DisableSupportHits()
    {
        bGetFeatureSupportHits = false;
    }

    bool GetFeatureSupportHits() const
    {
        return bGetFeatureSupportHits;
    }

    const char *GetVersion()
    {
        return osVersion.c_str();
    }

    bool IsOldDeegree(const char *pszErrorString);

    bool GetNeedNAMESPACE() const
    {
        return bNeedNAMESPACE;
    }

    bool HasMinOperators() const
    {
        return bHasMinOperators;
    }

    bool HasNullCheck() const
    {
        return bHasNullCheck;
    }

    bool UseFeatureId() const
    {
        return bUseFeatureId;
    }

    bool RequiresEnvelopeSpatialFilter() const
    {
        return bRequiresEnvelopeSpatialFilter;
    }

    void SetGmlObjectIdNeedsGMLPrefix()
    {
        bGmlObjectIdNeedsGMLPrefix = true;
    }

    int DoesGmlObjectIdNeedGMLPrefix() const
    {
        return bGmlObjectIdNeedsGMLPrefix;
    }

    void SetPropertyIsNotEqualToUnSupported()
    {
        bPropertyIsNotEqualToSupported = false;
    }

    bool PropertyIsNotEqualToSupported() const
    {
        return bPropertyIsNotEqualToSupported;
    }

    CPLString GetPostTransactionURL();

    void SaveLayerSchema(const char *pszLayerName, const CPLXMLNode *psSchema);

    CPLHTTPResult *HTTPFetch(const char *pszURL, CSLConstList papszOptions);

    bool IsPagingAllowed() const
    {
        return bPagingAllowed;
    }

    int GetPageSize() const
    {
        return nPageSize;
    }

    int GetBaseStartIndex() const
    {
        return nBaseStartIndex;
    }

    void LoadMultipleLayerDefn(const char *pszLayerName, char *pszNS,
                               char *pszNSVal);

    bool GetKeepLayerNamePrefix() const
    {
        return bKeepLayerNamePrefix;
    }

    const CPLString &GetBaseURL()
    {
        return osBaseURL;
    }

    bool IsEmptyAsNull() const
    {
        return bEmptyAsNull;
    }

    bool InvertAxisOrderIfLatLong() const
    {
        return bInvertAxisOrderIfLatLong;
    }

    const CPLString &GetConsiderEPSGAsURN() const
    {
        return osConsiderEPSGAsURN;
    }

    bool ExposeGMLId() const
    {
        return bExposeGMLId;
    }

    char **GetMetadataDomainList() override;
    CSLConstList GetMetadata(const char *pszDomain = "") override;
};

#endif /* ndef OGR_WFS_H_INCLUDED */
