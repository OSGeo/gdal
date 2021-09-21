/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018-2020, NextGIS
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#ifndef OGR_NGW_H_INCLUDED
#define OGR_NGW_H_INCLUDED

// gdal headers
#include "ogrsf_frmts.h"
#include "ogr_swq.h"

#include <map>
#include <set>

namespace NGWAPI {
    std::string GetPermissions(const std::string &osUrl, const std::string &osResourceId);
    std::string GetResource(const std::string &osUrl, const std::string &osResourceId);
    std::string GetChildren(const std::string &osUrl, const std::string &osResourceId);
    std::string GetFeature(const std::string &osUrl, const std::string &osResourceId);
    std::string GetTMS(const std::string &osUrl, const std::string &osResourceId);
    std::string GetFeaturePage(const std::string &osUrl, const std::string &osResourceId,
        GIntBig nStart, int nCount = 0, const std::string &osFields = "",
        const std::string &osWhere = "", const std::string &osSpatialWhere = "",
        const std::string &osExtensions = "", bool IsGeometryIgnored = false);
    std::string GetRoute(const std::string &osUrl);
    std::string GetUpload(const std::string &osUrl);
    std::string GetVersion(const std::string &osUrl);
    bool CheckVersion(const std::string &osVersion, int nMajor, int nMinor = 0,
        int nPatch = 0);

    struct Uri {
        std::string osPrefix;
        std::string osAddress;
        std::string osResourceId;
        std::string osNewResourceName;
    };

    // C++11 allow defaults
    struct Permissions {
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
        const std::string &osResourceId, char **papszHTTPOptions, bool bReadWrite);
    bool DeleteResource(const std::string &osUrl, const std::string &osResourceId,
        char **papszHTTPOptions);
    bool RenameResource(const std::string &osUrl, const std::string &osResourceId,
        const std::string &osNewName, char **papszHTTPOptions);
    OGRwkbGeometryType NGWGeomTypeToOGRGeomType(const std::string &osGeomType);
    std::string OGRGeomTypeToNGWGeomType(OGRwkbGeometryType eType);
    OGRFieldType NGWFieldTypeToOGRFieldType(const std::string &osFieldType);
    std::string OGRFieldTypeToNGWFieldType(OGRFieldType eType);
    std::string GetFeatureCount(const std::string &osUrl,
        const std::string &osResourceId);
    std::string GetLayerExtent(const std::string &osUrl,
        const std::string &osResourceId);
    bool FlushMetadata(const std::string &osUrl, const std::string &osResourceId,
        char **papszMetadata, char **papszHTTPOptions );
    std::string CreateResource(const std::string &osUrl, const std::string &osPayload,
        char **papszHTTPOptions);
    bool UpdateResource(const std::string &osUrl, const std::string &osResourceId,
        const std::string &osPayload, char **papszHTTPOptions);
    void FillResmeta(CPLJSONObject &oRoot, char **papszMetadata);
    std::string GetResmetaSuffix(CPLJSONObject::Type eType);
    bool DeleteFeature(const std::string &osUrl, const std::string &osResourceId,
        const std::string &osFeatureId, char **papszHTTPOptions);
    GIntBig CreateFeature(const std::string &osUrl, const std::string &osResourceId,
        const std::string &osFeatureJson, char **papszHTTPOptions);
    bool UpdateFeature(const std::string &osUrl, const std::string &osResourceId,
        const std::string &osFeatureId, const std::string &osFeatureJson,
        char **papszHTTPOptions);
    std::vector<GIntBig> PatchFeatures(const std::string &osUrl, const std::string &osResourceId,
        const std::string &osFeaturesJson, char **papszHTTPOptions);
    bool GetExtent(const std::string &osUrl, const std::string &osResourceId,
        char **papszHTTPOptions, int nEPSG, OGREnvelope &stExtent);
    CPLJSONObject UploadFile(const std::string &osUrl, const std::string &osFilePath,
        char **papszHTTPOptions, GDALProgressFunc pfnProgress, void *pProgressData);
} // namespace NGWAPI

class OGRNGWDataset;

class OGRNGWLayer final: public OGRLayer
{
    std::string osResourceId;
    OGRNGWDataset *poDS;
    NGWAPI::Permissions stPermissions;
    bool bFetchedPermissions;
    OGRFeatureDefn *poFeatureDefn;
    GIntBig nFeatureCount;
    OGREnvelope stExtent;
    std::map<GIntBig, OGRFeature*> moFeatures;
    std::map<GIntBig, OGRFeature*>::const_iterator oNextPos;
    GIntBig nPageStart;
    bool bNeedSyncData, bNeedSyncStructure;
    std::set<GIntBig> soChangedIds;
    std::string osFields;
    std::string osWhere;
    std::string osSpatialFilter;
    bool bClientSideAttributeFilter;

    explicit OGRNGWLayer( const std::string &osResourceIdIn, OGRNGWDataset *poDSIn,
        const NGWAPI::Permissions &stPermissionsIn, OGRFeatureDefn *poFeatureDefnIn,
        GIntBig nFeatureCountIn, const OGREnvelope &stExtentIn );

public:
    explicit OGRNGWLayer( OGRNGWDataset *poDSIn, const CPLJSONObject &oResourceJsonObject );
    explicit OGRNGWLayer( OGRNGWDataset *poDSIn, const std::string &osNameIn,
        OGRSpatialReference *poSpatialRef, OGRwkbGeometryType eGType,
        const std::string &osKeyIn, const std::string &osDescIn );
    virtual ~OGRNGWLayer();

    bool Delete();
    bool Rename( const std::string &osNewName );
    std::string GetResourceId() const;

    /* OGRLayer */
    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex( GIntBig nIndex ) override;
    virtual OGRFeature *GetFeature( GIntBig nFID ) override;
    virtual GIntBig GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
        int bForce = TRUE) override;
    virtual OGRFeatureDefn *GetLayerDefn() override;
    virtual int TestCapability( const char * ) override;

    virtual OGRErr CreateField( OGRFieldDefn *poField,
        int bApproxOK = TRUE ) override;
    virtual OGRErr DeleteField( int iField ) override;
    virtual OGRErr ReorderFields( int *panMap ) override;
    virtual OGRErr AlterFieldDefn( int iField, OGRFieldDefn *poNewFieldDefn,
        int nFlagsIn ) override;

    virtual OGRErr SyncToDisk() override;

    virtual OGRErr DeleteFeature(GIntBig nFID) override;
    bool DeleteAllFeatures();

    virtual CPLErr SetMetadata( char **papszMetadata,
        const char *pszDomain = "" ) override;
    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
        const char *pszDomain = "" ) override;

    virtual OGRErr SetIgnoredFields( const char **papszFields ) override;
    virtual OGRErr SetAttributeFilter( const char *pszQuery ) override;
    virtual void SetSpatialFilter( OGRGeometry *poGeom ) override;
    virtual void SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override;

    OGRErr SetSelectedFields(const std::set<std::string> &aosFields);
    OGRNGWLayer *Clone() const;

public:
    static std::string TranslateSQLToFilter( swq_expr_node *poNode );

protected:
    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;

private:
    void FillMetadata( const CPLJSONObject &oRootObject );
    void FillFields( const CPLJSONArray &oFields );
    void FetchPermissions();
    void FreeFeaturesCache( bool bForce = false );
    std::string CreateNGWResourceJson();
    OGRErr SyncFeatures();
    GIntBig GetMaxFeatureCount( bool bForce );
    bool FillFeatures(const std::string &osUrl);
    GIntBig GetNewFeaturesCount() const;
};

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

    // vector
    OGRNGWLayer **papoLayers;
    int nLayers;

    // raster
    GDALDataset *poRasterDS;
    OGREnvelope stPixelExtent;
    int nRasters;
    int nCacheExpires, nCacheMaxSize;

    // json
    std::string osJsonDepth;
    std::string osExtensions;

public:
    OGRNGWDataset();
    virtual ~OGRNGWDataset();

    bool Open( const char *pszFilename, char **papszOpenOptionsIn,
        bool bUpdateIn, int nOpenFlagsIn );
    bool Open( const std::string &osUrlIn, const std::string &osResourceIdIn,
        char **papszOpenOptionsIn, bool bUpdateIn, int nOpenFlagsIn );
    std::string Extensions() const;

    /* GDALDataset */
    virtual int GetLayerCount() override { return nLayers; }
    virtual OGRLayer *GetLayer( int ) override;
    virtual int TestCapability( const char * ) override;
    virtual OGRLayer  *ICreateLayer( const char *pszName,
        OGRSpatialReference *poSpatialRef = nullptr,
        OGRwkbGeometryType eGType = wkbUnknown,
        char **papszOptions = nullptr ) override;
    virtual OGRErr DeleteLayer( int ) override;
    virtual CPLErr SetMetadata( char **papszMetadata,
        const char *pszDomain = "" ) override;
    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
        const char *pszDomain = "" ) override;
    virtual void FlushCache() override;
    virtual OGRLayer *ExecuteSQL( const char *pszStatement,
        OGRGeometry *poSpatialFilter, const char *pszDialect ) override;

    virtual const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform( double *padfTransform ) override;
    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag, int nXOff, int nYOff,
        int nXSize, int nYSize, void *pData, int nBufXSize, int nBufYSize,
        GDALDataType eBufType, int nBandCount, int *panBandMap,
        GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
        GDALRasterIOExtraArg* psExtraArg ) override;

private:
    char **GetHeaders() const;
    std::string GetUrl() const { return osUrl; }
    std::string GetResourceId() const { return osResourceId; }
    void FillMetadata( const CPLJSONObject &oRootObject );
    bool FillResources( char **papszOptions, int nOpenFlagsIn );
    void AddLayer( const CPLJSONObject &oResourceJsonObject,  char **papszOptions,
        int nOpenFlagsIn );
    void AddRaster( const CPLJSONObject &oResourceJsonObject,  char **papszOptions );
    bool Init(int nOpenFlagsIn);
    bool FlushMetadata( char **papszMetadata );
    inline bool IsUpdateMode() const { return eAccess == GA_Update; }
    bool IsBatchMode() const { return nBatchSize >= 0; }
    bool HasFeaturePaging() const { return bHasFeaturePaging; }
    int GetPageSize() const { return bHasFeaturePaging ? nPageSize : -1; }
    int GetBatchSize() const { return nBatchSize; }
    bool IsExtInNativeData() const { return bExtInNativeData; }
    void FetchPermissions();
    void FillCapabilities( char **papszOptions );
private:
    CPL_DISALLOW_COPY_ASSIGN(OGRNGWDataset)
};

#endif // OGR_NGW_H_INCLUDED
