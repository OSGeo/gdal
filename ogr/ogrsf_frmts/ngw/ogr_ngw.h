/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018-2025, NextGIS
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/
#ifndef OGR_NGW_H_INCLUDED
#define OGR_NGW_H_INCLUDED

// gdal headers
#include "ogrsf_frmts.h"
#include "ogr_swq.h"

#include <array>
#include <map>
#include <set>

namespace NGWAPI
{
std::string GetPermissionsURL(const std::string &osUrl,
                              const std::string &osResourceId);
std::string GetResourceURL(const std::string &osUrl,
                           const std::string &osResourceId);
std::string GetChildrenURL(const std::string &osUrl,
                           const std::string &osResourceId);
std::string GetFeatureURL(const std::string &osUrl,
                          const std::string &osResourceId);
std::string GetTMSURL(const std::string &osUrl,
                      const std::string &osResourceId);
std::string GetFeaturePageURL(const std::string &osUrl,
                              const std::string &osResourceId, GIntBig nStart,
                              int nCount = 0, const std::string &osFields = "",
                              const std::string &osWhere = "",
                              const std::string &osSpatialWhere = "",
                              const std::string &osExtensions = "",
                              bool IsGeometryIgnored = false);
std::string GetRouteURL(const std::string &osUrl);
std::string GetUploadURL(const std::string &osUrl);
std::string GetVersionURL(const std::string &osUrl);
std::string GetCOGURL(const std::string &osUrl,
                      const std::string &osResourceId);
std::string GetSearchURL(const std::string &osUrl, const std::string &osKey,
                         const std::string &osValue);

bool CheckVersion(const std::string &osVersion, int nMajor, int nMinor = 0,
                  int nPatch = 0);
bool CheckRequestResult(bool bResult, const CPLJSONObject &oRoot,
                        const std::string &osErrorMessage);
bool CheckSupportedType(bool bIsRaster, const std::string &osType);

struct Uri
{
    std::string osPrefix;
    std::string osAddress;
    std::string osResourceId;
    std::string osNewResourceName;
};

// C++11 allow defaults
struct Permissions
{
    bool bResourceCanRead = false;
    bool bResourceCanCreate = false;
    bool bResourceCanUpdate = false;
    bool bResourceCanDelete = false;
    bool bDatastructCanRead = false;
    bool bDatastructCanWrite = false;
    bool bDataCanRead = false;
    bool bDataCanWrite = false;
    bool bMetadataCanRead = false;
    bool bMetadataCanWrite = false;
};

Uri ParseUri(const std::string &osUrl);
Permissions CheckPermissions(const std::string &osUrl,
                             const std::string &osResourceId,
                             const CPLStringList &aosHTTPOptions,
                             bool bReadWrite);
bool DeleteResource(const std::string &osUrl, const std::string &osResourceId,
                    const CPLStringList &aosHTTPOptions);
bool RenameResource(const std::string &osUrl, const std::string &osResourceId,
                    const std::string &osNewName,
                    const CPLStringList &aosHTTPOptions);
OGRwkbGeometryType NGWGeomTypeToOGRGeomType(const std::string &osGeomType);
std::string OGRGeomTypeToNGWGeomType(OGRwkbGeometryType eType);
OGRFieldType NGWFieldTypeToOGRFieldType(const std::string &osFieldType);
std::string OGRFieldTypeToNGWFieldType(OGRFieldType eType);
std::string GetFeatureCount(const std::string &osUrl,
                            const std::string &osResourceId);
std::string GetLayerExtent(const std::string &osUrl,
                           const std::string &osResourceId);
bool FlushMetadata(const std::string &osUrl, const std::string &osResourceId,
                   char **papszMetadata, const CPLStringList &aosHTTPOptions);
std::string CreateResource(const std::string &osUrl,
                           const std::string &osPayload,
                           const CPLStringList &aosHTTPOptions);
bool UpdateResource(const std::string &osUrl, const std::string &osResourceId,
                    const std::string &osPayload,
                    const CPLStringList &aosHTTPOptions);
void FillResmeta(const CPLJSONObject &oRoot, char **papszMetadata);
std::string GetResmetaSuffix(CPLJSONObject::Type eType);
bool DeleteFeature(const std::string &osUrl, const std::string &osResourceId,
                   const std::string &osFeatureId,
                   const CPLStringList &aosHTTPOptions);
bool DeleteFeatures(const std::string &osUrl, const std::string &osResourceId,
                    const std::string &osFeaturesIDJson,
                    const CPLStringList &aosHTTPOptions);
GIntBig CreateFeature(const std::string &osUrl, const std::string &osResourceId,
                      const std::string &osFeatureJson,
                      const CPLStringList &aosHTTPOptions);
bool UpdateFeature(const std::string &osUrl, const std::string &osResourceId,
                   const std::string &osFeatureId,
                   const std::string &osFeatureJson,
                   const CPLStringList &aosHTTPOptions);
std::vector<GIntBig> PatchFeatures(const std::string &osUrl,
                                   const std::string &osResourceId,
                                   const std::string &osFeaturesJson,
                                   const CPLStringList &aosHTTPOptions);
bool GetExtent(const std::string &osUrl, const std::string &osResourceId,
               const CPLStringList &aosHTTPOptions, int nEPSG,
               OGREnvelope &stExtent);
CPLJSONObject UploadFile(const std::string &osUrl,
                         const std::string &osFilePath,
                         const CPLStringList &aosHTTPOptions,
                         GDALProgressFunc pfnProgress, void *pProgressData);
}  // namespace NGWAPI

class OGRNGWCodedFieldDomain
{
  public:
    explicit OGRNGWCodedFieldDomain() = default;
    explicit OGRNGWCodedFieldDomain(const CPLJSONObject &oResourceJsonObject);
    virtual ~OGRNGWCodedFieldDomain() = default;
    const OGRFieldDomain *ToFieldDomain(OGRFieldType eFieldType) const;
    GIntBig GetID() const;
    std::string GetDomainsNames() const;
    bool HasDomainName(const std::string &osName) const;

  private:
    GIntBig nResourceID = 0;
    GIntBig nResourceParentID = 0;
    std::string osCreationDate;
    std::string osDisplayName;
    std::string osKeyName;
    std::string osDescription;
    std::array<std::shared_ptr<OGRCodedFieldDomain>, 3> apDomains;
};

class OGRNGWDataset;

class OGRNGWLayer final : public OGRLayer
{
    std::string osResourceId;
    OGRNGWDataset *poDS;
    NGWAPI::Permissions stPermissions;
    bool bFetchedPermissions;
    OGRFeatureDefn *poFeatureDefn;
    GIntBig nFeatureCount;
    OGREnvelope stExtent;
    std::map<GIntBig, OGRFeature *> moFeatures;
    std::map<GIntBig, OGRFeature *>::const_iterator oNextPos;
    GIntBig nPageStart;
    bool bNeedSyncData, bNeedSyncStructure;
    std::set<GIntBig> soChangedIds;
    std::set<GIntBig> soDeletedFieldsIds;
    std::string osFields;
    std::string osWhere;
    std::string osSpatialFilter;
    bool bClientSideAttributeFilter;

    explicit OGRNGWLayer(const std::string &osResourceIdIn,
                         OGRNGWDataset *poDSIn,
                         const NGWAPI::Permissions &stPermissionsIn,
                         OGRFeatureDefn *poFeatureDefnIn,
                         GIntBig nFeatureCountIn,
                         const OGREnvelope &stExtentIn);

  public:
    explicit OGRNGWLayer(OGRNGWDataset *poDSIn,
                         const CPLJSONObject &oResourceJsonObject);
    explicit OGRNGWLayer(OGRNGWDataset *poDSIn, const std::string &osNameIn,
                         OGRSpatialReference *poSpatialRef,
                         OGRwkbGeometryType eGType, const std::string &osKeyIn,
                         const std::string &osDescIn);
    virtual ~OGRNGWLayer();

    bool Delete();
    virtual OGRErr Rename(const char *pszNewName) override;
    std::string GetResourceId() const;

    /* OGRLayer */
    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;
    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;
    virtual OGRFeatureDefn *GetLayerDefn() override;
    virtual int TestCapability(const char *) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iField) override;
    virtual OGRErr ReorderFields(int *panMap) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;

    virtual OGRErr SyncToDisk() override;

    virtual OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr DeleteFeatures(const std::vector<GIntBig> &vFeaturesID);
    bool DeleteAllFeatures();

    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;

    virtual OGRErr SetIgnoredFields(CSLConstList papszFields) override;
    virtual OGRErr SetAttributeFilter(const char *pszQuery) override;
    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *poGeom) override;

    OGRErr SetSelectedFields(const std::set<std::string> &aosFields);
    OGRNGWLayer *Clone() const;

  public:
    static std::string TranslateSQLToFilter(swq_expr_node *poNode);

  protected:
    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;

  private:
    void Fill(const CPLJSONObject &oRootObject);
    void FillMetadata(const CPLJSONObject &oRootObject);
    void FillFields(const CPLJSONArray &oFields,
                    const CPLStringList &soIgnoredFieldNames);
    void FetchPermissions();
    void FreeFeaturesCache(bool bForce = false);
    std::string CreateNGWResourceJson();
    OGRErr SyncFeatures();
    GIntBig GetMaxFeatureCount(bool bForce);
    bool FillFeatures(const std::string &osUrl);
    GIntBig GetNewFeaturesCount() const;
    CPLJSONObject LoadUrl(const std::string &osUrl) const;
};

using OGRNGWLayerPtr = std::shared_ptr<OGRNGWLayer>;

class OGRNGWDataset final : public GDALDataset
{
    friend class OGRNGWLayer;
    int nBatchSize;
    int nPageSize;
    NGWAPI::Permissions stPermissions;
    bool bFetchedPermissions;
    bool bHasFeaturePaging;
    std::string osUserPwd;
    std::string osUrl;
    std::string osResourceId;
    std::string osName;
    bool bExtInNativeData;
    bool bMetadataDerty;
    // http options
    std::string osConnectTimeout;
    std::string osTimeout;
    std::string osRetryCount;
    std::string osRetryDelay;

    // vector
    std::vector<OGRNGWLayerPtr> aoLayers;

    // raster
    GDALDataset *poRasterDS;
    OGREnvelope stPixelExtent;
    int nRasters;
    int nCacheExpires, nCacheMaxSize;

    // json
    std::string osJsonDepth;
    std::string osExtensions;

    // domain
    std::map<GIntBig, OGRNGWCodedFieldDomain> moDomains;

  public:
    OGRNGWDataset();
    virtual ~OGRNGWDataset();

    bool Open(const char *pszFilename, char **papszOpenOptionsIn,
              bool bUpdateIn, int nOpenFlagsIn);
    bool Open(const std::string &osUrlIn, const std::string &osResourceIdIn,
              char **papszOpenOptionsIn, bool bUpdateIn, int nOpenFlagsIn);
    std::string Extensions() const;

    /* GDALDataset */
    virtual int GetLayerCount() override
    {
        return static_cast<int>(aoLayers.size());
    }

    virtual OGRLayer *GetLayer(int) override;
    virtual int TestCapability(const char *) override;
    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions) override;
    virtual OGRErr DeleteLayer(int) override;
    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;
    virtual CPLErr FlushCache(bool bAtClosing) override;
    virtual OGRLayer *ExecuteSQL(const char *pszStatement,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;

    virtual const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(double *padfTransform) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, BANDMAP_TYPE panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;
    std::vector<std::string>
    GetFieldDomainNames(CSLConstList papszOptions = nullptr) const override;
    const OGRFieldDomain *
    GetFieldDomain(const std::string &name) const override;
    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                        std::string &failureReason) override;
    bool DeleteFieldDomain(const std::string &name,
                           std::string &failureReason) override;
    bool UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                           std::string &failureReason) override;

  private:
    CPLStringList GetHeaders(bool bSkipRetry = true) const;

    std::string GetUrl() const
    {
        return osUrl;
    }

    std::string GetResourceId() const
    {
        return osResourceId;
    }

    void FillMetadata(const CPLJSONObject &oRootObject);
    bool FillResources(const CPLStringList &aosHTTPOptions, int nOpenFlagsIn);
    void AddLayer(const CPLJSONObject &oResourceJsonObject,
                  const CPLStringList &aosHTTPOptions, int nOpenFlagsIn);
    void AddRaster(const CPLJSONObject &oResourceJsonObject);
    void SetupRasterDSWrapper(const OGREnvelope &stExtent);
    bool Init(int nOpenFlagsIn);
    bool FlushMetadata(char **papszMetadata);

    inline bool IsUpdateMode() const
    {
        return eAccess == GA_Update;
    }

    bool IsBatchMode() const
    {
        return nBatchSize >= 0;
    }

    bool HasFeaturePaging() const
    {
        return bHasFeaturePaging;
    }

    int GetPageSize() const
    {
        return bHasFeaturePaging ? nPageSize : -1;
    }

    int GetBatchSize() const
    {
        return nBatchSize;
    }

    bool IsExtInNativeData() const
    {
        return bExtInNativeData;
    }

    void FetchPermissions();
    void FillCapabilities(const CPLStringList &aosHTTPOptions);

    OGRNGWCodedFieldDomain GetDomainByID(GIntBig id) const;
    GIntBig GetDomainIdByName(const std::string &osDomainName) const;

  private:
    CPL_DISALLOW_COPY_ASSIGN(OGRNGWDataset)
};

#endif  // OGR_NGW_H_INCLUDED
