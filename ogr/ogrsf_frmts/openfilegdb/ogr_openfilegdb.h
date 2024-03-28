/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_OPENFILEGDB_H_INCLUDED
#define OGR_OPENFILEGDB_H_INCLUDED

#include "ogrsf_frmts.h"
#include "filegdbtable.h"
#include "ogr_swq.h"
#include "cpl_mem_cache.h"
#include "cpl_quad_tree.h"

#include "gdal_rat.h"

#include <array>
#include <vector>
#include <map>

using namespace OpenFileGDB;

std::string OFGDBGenerateUUID();

int OGROpenFileGDBIsComparisonOp(int op);

// The FileGeodatabase format does not really allow strings of arbitrary width
// in the XML and .gdbtable header declaration. They must have a non-zero
// maximum width. But if we put it to a huge value (let's say 1 billion), this
// causes crashes in some Esri products (cf #5952, perhaps they allocate
// std::string's to that maximum size?).
// Hence this default of a relative large but not too large
// width when creating a OGR string field width of unspecified width.
// Note that when opening a FileGeodatabase with string fields of that width,
// we do not advertise it in OGRFieldDefn::GetWidth() but we advertise 0 instead,
// to allow round-tripping.
constexpr int DEFAULT_STRING_WIDTH = 65536;

// UUID of object type
constexpr const char *pszFolderTypeUUID =
    "{f3783e6f-65ca-4514-8315-ce3985dad3b1}";
constexpr const char *pszWorkspaceTypeUUID =
    "{c673fe0f-7280-404f-8532-20755dd8fc06}";
constexpr const char *pszFeatureDatasetTypeUUID =
    "{74737149-DCB5-4257-8904-B9724E32A530}";
constexpr const char *pszFeatureClassTypeUUID =
    "{70737809-852c-4a03-9e22-2cecea5b9bfa}";
constexpr const char *pszTableTypeUUID =
    "{cd06bc3b-789d-4c51-aafa-a467912b8965}";
constexpr const char *pszRangeDomainTypeUUID =
    "{c29da988-8c3e-45f7-8b5c-18e51ee7beb4}";
constexpr const char *pszCodedDomainTypeUUID =
    "{8c368b12-a12e-4c7e-9638-c9c64e69e98f}";
constexpr const char *pszRelationshipTypeUUID =
    "{b606a7e1-fa5b-439c-849c-6e9c2481537b}";

// UUID of relationship type
constexpr const char *pszDatasetInFeatureDatasetUUID =
    "{a1633a59-46ba-4448-8706-d8abe2b2b02e}";
constexpr const char *pszDatasetInFolderUUID =
    "{dc78f1ab-34e4-43ac-ba47-1c4eabd0e7c7}";
constexpr const char *pszDomainInDatasetUUID =
    "{17e08adb-2b31-4dcd-8fdd-df529e88f843}";
constexpr const char *pszDatasetsRelatedThroughUUID =
    "{725badab-3452-491b-a795-55f32d67229c}";

/***********************************************************************/
/*                       FETCH_FIELD_IDX()                             */
/***********************************************************************/

#define FETCH_FIELD_IDX_WITH_RET(idxName, varName, type, errorCode)            \
    const int idxName = oTable.GetFieldIdx(varName);                           \
    if (idxName < 0 || oTable.GetField(idxName)->GetType() != type)            \
    {                                                                          \
        CPLError(CE_Failure, CPLE_AppDefined,                                  \
                 "Could not find field %s in table %s", varName,               \
                 oTable.GetFilename().c_str());                                \
        return errorCode;                                                      \
    }

#define FETCH_FIELD_IDX(idxName, varName, type)                                \
    FETCH_FIELD_IDX_WITH_RET(idxName, varName, type, false)

/************************************************************************/
/*                      OGROpenFileGDBLayer                             */
/************************************************************************/

class OGROpenFileGDBDataSource;
class OGROpenFileGDBGeomFieldDefn;
class OGROpenFileGDBFeatureDefn;

typedef enum
{
    SPI_IN_BUILDING,
    SPI_COMPLETED,
    SPI_INVALID,
} SPIState;

class OGROpenFileGDBLayer final : public OGRLayer
{
    friend class OGROpenFileGDBGeomFieldDefn;
    friend class OGROpenFileGDBFeatureDefn;

    OGROpenFileGDBDataSource *m_poDS = nullptr;
    CPLString m_osGDBFilename{};
    CPLString m_osName{};
    std::string m_osPath{};
    std::string m_osThisGUID{};
    bool m_bEditable = false;
    bool m_bRegisteredTable = true;
    CPLStringList m_aosCreationOptions{};
    FileGDBTable *m_poLyrTable = nullptr;
    OGROpenFileGDBFeatureDefn *m_poFeatureDefn = nullptr;
    int m_iGeomFieldIdx = -1;
    int m_iAreaField = -1;    // index of Shape_Area field
    int m_iLengthField = -1;  // index of Shape_Length field
    int m_iCurFeat = 0;
    int m_iFIDAsRegularColumnIndex = -1;
    std::string m_osDefinition{};
    std::string m_osDocumentation{};
    std::string m_osConfigurationKeyword{};
    OGRwkbGeometryType m_eGeomType = wkbNone;
    bool m_bArcGISPro32OrLater = false;
    int m_bValidLayerDefn = -1;
    int m_bEOF = false;
    bool m_bTimeInUTC = false;
    std::string m_osFeatureDatasetGUID{};

    bool m_bWarnedDateNotConvertibleUTC = false;

    bool m_bHasCreatedBackupForTransaction = false;
    std::unique_ptr<OGRFeatureDefn> m_poFeatureDefnBackup{};

    int BuildLayerDefinition();
    int BuildGeometryColumnGDBv10(const std::string &osParentDefinition);
    OGRFeature *GetCurrentFeature();

    std::unique_ptr<FileGDBOGRGeometryConverter> m_poGeomConverter{};

    int m_iFieldToReadAsBinary = -1;

    FileGDBIterator *m_poAttributeIterator = nullptr;
    int m_bIteratorSufficientToEvaluateFilter = FALSE;
    FileGDBIterator *BuildIteratorFromExprNode(swq_expr_node *poNode);

    FileGDBIterator *m_poIterMinMax = nullptr;

    FileGDBSpatialIndexIterator *m_poSpatialIndexIterator = nullptr;
    FileGDBIterator *m_poCombinedIterator = nullptr;

    // Legacy behavior prior to handling of .spx file
    // To remove ultimately.
    SPIState m_eSpatialIndexState = SPI_IN_BUILDING;
    CPLQuadTree *m_pQuadTree = nullptr;
    void **m_pahFilteredFeatures = nullptr;
    int m_nFilteredFeatureCount = -1;
    static void GetBoundsFuncEx(const void *hFeature, CPLRectObj *pBounds,
                                void *pQTUserData);

    void TryToDetectMultiPatchKind();
    void BuildCombinedIterator();
    bool RegisterTable();
    void RefreshXMLDefinitionInMemory();
    bool CreateFeatureDataset(const char *pszFeatureDataset);
    std::string GetLaunderedFieldName(const std::string &osNameOri) const;
    std::string GetLaunderedLayerName(const std::string &osNameOri) const;

    mutable std::vector<std::string> m_aosTempStrings{};
    bool PrepareFileGDBFeature(OGRFeature *poFeature,
                               std::vector<OGRField> &fields,
                               const OGRGeometry *&poGeom, bool bUpdate);

    CPL_DISALLOW_COPY_ASSIGN(OGROpenFileGDBLayer)

  public:
    OGROpenFileGDBLayer(OGROpenFileGDBDataSource *poDS,
                        const char *pszGDBFilename, const char *pszName,
                        const std::string &osDefinition,
                        const std::string &osDocumentation, bool bEditable,
                        OGRwkbGeometryType eGeomType = wkbUnknown,
                        const std::string &osParentDefinition = std::string());

    OGROpenFileGDBLayer(OGROpenFileGDBDataSource *poDS,
                        const char *pszGDBFilename, const char *pszName,
                        OGRwkbGeometryType eType, CSLConstList papszOptions);

    virtual ~OGROpenFileGDBLayer();

    bool Create(const OGRGeomFieldDefn *poSrcGeomFieldDefn);
    void Close();

    const std::string &GetFilename() const
    {
        return m_osGDBFilename;
    }

    const std::string &GetXMLDefinition()
    {
        return m_osDefinition;
    }

    const std::string &GetXMLDocumentation()
    {
        return m_osDocumentation;
    }

    int GetAttrIndexUse()
    {
        return (m_poAttributeIterator == nullptr)        ? 0
               : (m_bIteratorSufficientToEvaluateFilter) ? 2
                                                         : 1;
    }

    const OGRField *GetMinMaxValue(OGRFieldDefn *poFieldDefn, int bIsMin,
                                   int &eOutType);
    int GetMinMaxSumCount(OGRFieldDefn *poFieldDefn, double &dfMin,
                          double &dfMax, double &dfSum, int &nCount);
    int HasIndexForField(const char *pszFieldName);
    FileGDBIterator *BuildIndex(const char *pszFieldName, int bAscending,
                                int op, swq_expr_node *poValue);

    SPIState GetSpatialIndexState() const
    {
        return m_eSpatialIndexState;
    }

    int IsValidLayerDefn()
    {
        return BuildLayerDefinition();
    }

    void CreateSpatialIndex();
    void CreateIndex(const std::string &osIdxName,
                     const std::string &osExpression);
    bool Repack();
    void RecomputeExtent();

    bool CheckFreeListConsistency();

    bool BeginEmulatedTransaction();
    bool CommitEmulatedTransaction();
    bool RollbackEmulatedTransaction();

    GDALDataset *GetDataset() override;

    virtual const char *GetName() override
    {
        return m_osName.c_str();
    }

    virtual OGRwkbGeometryType GetGeomType() override;

    virtual const char *GetFIDColumn() override;

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    OGRErr GetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                       int bForce) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual void SetSpatialFilter(OGRGeometry *) override;

    virtual void SetSpatialFilter(int iGeomField, OGRGeometry *poGeom) override
    {
        OGRLayer::SetSpatialFilter(iGeomField, poGeom);
    }

    virtual OGRErr SetAttributeFilter(const char *pszFilter) override;

    virtual int TestCapability(const char *) override;

    virtual OGRErr Rename(const char *pszNewName) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK) override;
    virtual OGRErr DeleteField(int iFieldToDelete) override;
    virtual OGRErr AlterFieldDefn(int iFieldToAlter,
                                  OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;
    virtual OGRErr
    AlterGeomFieldDefn(int iGeomFieldToAlter,
                       const OGRGeomFieldDefn *poNewGeomFieldDefn,
                       int nFlagsIn) override;

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr DeleteFeature(GIntBig nFID) override;

    virtual OGRErr SyncToDisk() override;
};

/************************************************************************/
/*                      OGROpenFileGDBGeomFieldDefn                     */
/************************************************************************/
class OGROpenFileGDBGeomFieldDefn : public OGRGeomFieldDefn
{
    OGROpenFileGDBLayer *m_poLayer;

    CPL_DISALLOW_COPY_ASSIGN(OGROpenFileGDBGeomFieldDefn)

  public:
    OGROpenFileGDBGeomFieldDefn(OGROpenFileGDBLayer *poLayer,
                                const char *pszNameIn,
                                OGRwkbGeometryType eGeomTypeIn)
        : OGRGeomFieldDefn(pszNameIn, eGeomTypeIn), m_poLayer(poLayer)
    {
    }

    ~OGROpenFileGDBGeomFieldDefn()
    {
    }

    void UnsetLayer()
    {
        m_poLayer = nullptr;
    }

    virtual const OGRSpatialReference *GetSpatialRef() const override
    {
        if (poSRS)
            return poSRS;
        if (m_poLayer != nullptr)
            (void)m_poLayer->BuildLayerDefinition();
        return poSRS;
    }
};

/************************************************************************/
/*                      OGROpenFileGDBFeatureDefn                       */
/************************************************************************/
class OGROpenFileGDBFeatureDefn : public OGRFeatureDefn
{
    OGROpenFileGDBLayer *m_poLayer;
    mutable bool m_bHasBuiltFieldDefn;

    void LazyGeomInit() const
    {
        /* FileGDB v9 case */
        if (!m_bHasBuiltFieldDefn && m_poLayer != nullptr &&
            m_poLayer->m_eGeomType != wkbNone &&
            m_poLayer->m_osDefinition.empty())
        {
            m_bHasBuiltFieldDefn = true;
            (void)m_poLayer->BuildLayerDefinition();
        }
    }

    CPL_DISALLOW_COPY_ASSIGN(OGROpenFileGDBFeatureDefn)

  public:
    OGROpenFileGDBFeatureDefn(OGROpenFileGDBLayer *poLayer, const char *pszName,
                              bool bHasBuiltFieldDefn)
        : OGRFeatureDefn(pszName), m_poLayer(poLayer),
          m_bHasBuiltFieldDefn(bHasBuiltFieldDefn)
    {
    }

    ~OGROpenFileGDBFeatureDefn()
    {
    }

    void UnsetLayer()
    {
        if (!apoGeomFieldDefn.empty())
            cpl::down_cast<OGROpenFileGDBGeomFieldDefn *>(
                apoGeomFieldDefn[0].get())
                ->UnsetLayer();
        m_poLayer = nullptr;
    }

    virtual int GetFieldCount() const override
    {
        if (!m_bHasBuiltFieldDefn && m_poLayer != nullptr)
        {
            m_bHasBuiltFieldDefn = false;
            (void)m_poLayer->BuildLayerDefinition();
        }
        return OGRFeatureDefn::GetFieldCount();
    }

    virtual int GetGeomFieldCount() const override
    {
        LazyGeomInit();
        return OGRFeatureDefn::GetGeomFieldCount();
    }

    virtual OGRGeomFieldDefn *GetGeomFieldDefn(int i) override
    {
        LazyGeomInit();
        return OGRFeatureDefn::GetGeomFieldDefn(i);
    }

    virtual const OGRGeomFieldDefn *GetGeomFieldDefn(int i) const override
    {
        LazyGeomInit();
        return OGRFeatureDefn::GetGeomFieldDefn(i);
    }
};

/************************************************************************/
/*                       OGROpenFileGDBDataSource                       */
/************************************************************************/

class OGROpenFileGDBDataSource final : public OGRDataSource
{
    friend class OGROpenFileGDBLayer;
    friend class GDALOpenFileGDBRasterBand;
    friend class GDALOpenFileGDBRasterAttributeTable;

    CPLString m_osDirName{};
    std::vector<std::unique_ptr<OGROpenFileGDBLayer>> m_apoLayers{};
    std::vector<std::unique_ptr<OGROpenFileGDBLayer>> m_apoHiddenLayers{};
    char **m_papszFiles = nullptr;
    std::map<std::string, int> m_osMapNameToIdx{};
    std::shared_ptr<GDALGroup> m_poRootGroup{};
    CPLStringList m_aosSubdatasets{};

    std::string m_osRasterLayerName{};
    std::map<int, int> m_oMapGDALBandToGDBBandId{};
    bool m_bHasGeoTransform = false;
    std::array<double, 6> m_adfGeoTransform = {{0.0, 1.0, 0, 0.0, 0.0, 1.0}};
    int m_nShiftBlockX =
        0;  // Offset to add to FileGDB col_nbr field to convert from GDAL block numbering to FileGDB one
    int m_nShiftBlockY =
        0;  // Offset to add to FileGDB row_nbr field to convert from GDAL block numbering to FileGDB one
    OGRSpatialReference m_oRasterSRS{};
    std::unique_ptr<OGRLayer> m_poBlkLayer{};
    enum class Compression
    {
        NONE,
        LZ77,
        JPEG,
        JPEG2000,
    };
    Compression m_eRasterCompression = Compression::NONE;

    lru11::Cache<std::string, std::shared_ptr<OGRSpatialReference>>
        m_oCacheWKTToSRS{};

    std::string m_osRootGUID{};
    std::string m_osGDBSystemCatalogFilename{};
    std::string m_osGDBSpatialRefsFilename{};
    std::string m_osGDBItemsFilename{};
    std::string m_osGDBItemRelationshipsFilename{};
    std::map<std::string, std::unique_ptr<GDALRelationship>>
        m_osMapRelationships{};

    // Related to transactions
    bool m_bInTransaction = false;
    bool m_bSystemTablesBackedup = false;
    std::string m_osTransactionBackupDirname{};
    std::set<OGROpenFileGDBLayer *>
        m_oSetLayersCreatedInTransaction{};  // must be vector of raw pointer
    std::set<std::unique_ptr<OGROpenFileGDBLayer>>
        m_oSetLayersDeletedInTransaction{};

    /* For debugging/testing */
    bool bLastSQLUsedOptimizedImplementation;

    bool OpenFileGDBv10(int iGDBItems, int nInterestTable,
                        const GDALOpenInfo *poOpenInfo,
                        const std::string &osRasterLayerName,
                        std::set<int> &oSetIgnoredRasterLayerTableNum,
                        bool &bRetryFileGDBOut);
    int OpenFileGDBv9(int iGDBFeatureClasses, int iGDBObjectClasses,
                      int nInterestTable, const GDALOpenInfo *poOpenInfo,
                      const std::string &osRasterLayerName,
                      std::set<int> &oSetIgnoredRasterLayerTableNum);
    bool OpenRaster(const GDALOpenInfo *poOpenInfo,
                    const std::string &osLayerName,
                    const std::string &osDefinition,
                    const std::string &osDocumentation);
    void GuessJPEGQuality(int nOverviewCount);
    void ReadAuxTable(const std::string &osLayerName);

    int FileExists(const char *pszFilename);
    std::unique_ptr<OGROpenFileGDBLayer>
    BuildLayerFromName(const char *pszName);
    OGRLayer *AddLayer(const CPLString &osName, int nInterestTable,
                       int &nCandidateLayers, int &nLayersSDCOrCDF,
                       const CPLString &osDefinition,
                       const CPLString &osDocumentation,
                       OGRwkbGeometryType eGeomType,
                       const std::string &osParentDefinition);

    static bool IsPrivateLayerName(const CPLString &osName);

    bool CreateGDBSystemCatalog();
    bool CreateGDBDBTune();
    bool CreateGDBSpatialRefs();
    bool CreateGDBItems();
    bool CreateGDBItemTypes();
    bool CreateGDBItemRelationships();
    bool CreateGDBItemRelationshipTypes();

    bool BackupSystemTablesForTransaction();

    CPLErr Close() override;

    CPL_DISALLOW_COPY_ASSIGN(OGROpenFileGDBDataSource)

  public:
    OGROpenFileGDBDataSource();
    virtual ~OGROpenFileGDBDataSource();

    bool Open(const GDALOpenInfo *poOpenInfo, bool &bRetryFileGDBOut);
    bool Create(const char *pszName);

    virtual CPLErr FlushCache(bool bAtClosing = false) override;

    virtual const char *GetName() override
    {
        return m_osDirName.c_str();
    }

    virtual int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    virtual OGRLayer *GetLayer(int) override;
    virtual OGROpenFileGDBLayer *GetLayerByName(const char *pszName) override;
    bool IsLayerPrivate(int) const override;

    virtual OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;
    virtual void ReleaseResultSet(OGRLayer *poResultsSet) override;

    virtual int TestCapability(const char *) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    virtual OGRErr DeleteLayer(int) override;

    virtual char **GetFileList() override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRootGroup;
    }

    virtual OGRErr StartTransaction(int bForce) override;
    virtual OGRErr CommitTransaction() override;
    virtual OGRErr RollbackTransaction() override;

    const CPLStringList &GetSubdatasets() const
    {
        return m_aosSubdatasets;
    }

    CPLErr GetGeoTransform(double *padfGeoTransform) override;
    const OGRSpatialReference *GetSpatialRef() const override;

    char **GetMetadata(const char *pszDomain = "") override;

    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                        std::string &failureReason) override;

    bool DeleteFieldDomain(const std::string &name,
                           std::string &failureReason) override;

    bool UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                           std::string &failureReason) override;

    std::vector<std::string>
    GetRelationshipNames(CSLConstList papszOptions = nullptr) const override;

    const GDALRelationship *
    GetRelationship(const std::string &name) const override;

    bool AddRelationship(std::unique_ptr<GDALRelationship> &&relationship,
                         std::string &failureReason) override;

    bool DeleteRelationship(const std::string &name,
                            std::string &failureReason) override;

    bool UpdateRelationship(std::unique_ptr<GDALRelationship> &&relationship,
                            std::string &failureReason) override;

    bool GetExistingSpatialRef(const std::string &osWKT, double dfXOrigin,
                               double dfYOrigin, double dfXYScale,
                               double dfZOrigin, double dfZScale,
                               double dfMOrigin, double dfMScale,
                               double dfXYTolerance, double dfZTolerance,
                               double dfMTolerance);

    bool AddNewSpatialRef(const std::string &osWKT, double dfXOrigin,
                          double dfYOrigin, double dfXYScale, double dfZOrigin,
                          double dfZScale, double dfMOrigin, double dfMScale,
                          double dfXYTolerance, double dfZTolerance,
                          double dfMTolerance);

    bool RegisterLayerInSystemCatalog(const std::string &osLayerName);

    bool RegisterInItemRelationships(const std::string &osOriginGUID,
                                     const std::string &osDestGUID,
                                     const std::string &osTypeGUID);

    bool RegisterRelationshipInItemRelationships(
        const std::string &osRelationshipGUID, const std::string &osOriginGUID,
        const std::string &osDestGUID);

    bool RemoveRelationshipFromItemRelationships(
        const std::string &osRelationshipGUID);

    bool RegisterFeatureDatasetInItems(const std::string &osFeatureDatasetGUID,
                                       const std::string &osName,
                                       const char *pszXMLDefinition);

    bool FindUUIDFromName(const std::string &osFeatureDatasetName,
                          std::string &osUUIDOut);

    bool RegisterFeatureClassInItems(const std::string &osLayerGUID,
                                     const std::string &osLayerName,
                                     const std::string &osPath,
                                     const FileGDBTable *poLyrTable,
                                     const char *pszXMLDefinition,
                                     const char *pszDocumentation);

    bool RegisterASpatialTableInItems(const std::string &osLayerGUID,
                                      const std::string &osLayerName,
                                      const std::string &osPath,
                                      const char *pszXMLDefinition,
                                      const char *pszDocumentation);

    bool LinkDomainToTable(const std::string &osDomainName,
                           const std::string &osLayerGUID);
    bool UnlinkDomainToTable(const std::string &osDomainName,
                             const std::string &osLayerGUID);

    bool UpdateXMLDefinition(const std::string &osLayerName,
                             const char *pszXMLDefinition);

    bool IsInTransaction() const
    {
        return m_bInTransaction;
    }

    const std::string &GetBackupDirName() const
    {
        return m_osTransactionBackupDirname;
    }

    OGRSpatialReference *BuildSRS(const CPLXMLNode *psInfo);
    OGRSpatialReference *BuildSRS(const char *pszWKT);
};

/************************************************************************/
/*                   OGROpenFileGDBSingleFeatureLayer                   */
/************************************************************************/

class OGROpenFileGDBSingleFeatureLayer final : public OGRLayer
{
  private:
    char *pszVal;
    OGRFeatureDefn *poFeatureDefn;
    int iNextShapeId;

    CPL_DISALLOW_COPY_ASSIGN(OGROpenFileGDBSingleFeatureLayer)

  public:
    OGROpenFileGDBSingleFeatureLayer(const char *pszLayerName,
                                     const char *pszVal);
    virtual ~OGROpenFileGDBSingleFeatureLayer();

    virtual void ResetReading() override
    {
        iNextShapeId = 0;
    }

    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override
    {
        return FALSE;
    }
};

/************************************************************************/
/*                GDALOpenFileGDBRasterAttributeTable                   */
/************************************************************************/

class GDALOpenFileGDBRasterAttributeTable final
    : public GDALRasterAttributeTable
{
    std::unique_ptr<OGROpenFileGDBDataSource> m_poDS{};
    const std::string m_osVATTableName;
    std::unique_ptr<OGRLayer> m_poVATLayer{};
    mutable std::string m_osCachedValue{};

    GDALOpenFileGDBRasterAttributeTable(
        const GDALOpenFileGDBRasterAttributeTable &) = delete;
    GDALOpenFileGDBRasterAttributeTable &
    operator=(const GDALOpenFileGDBRasterAttributeTable &) = delete;

  public:
    GDALOpenFileGDBRasterAttributeTable(
        std::unique_ptr<OGROpenFileGDBDataSource> &&poDS,
        const std::string &osVATTableName,
        std::unique_ptr<OGRLayer> &&poVATLayer)
        : m_poDS(std::move(poDS)), m_osVATTableName(osVATTableName),
          m_poVATLayer(std::move(poVATLayer))
    {
    }

    GDALRasterAttributeTable *Clone() const override
    {
        auto poDS = std::make_unique<OGROpenFileGDBDataSource>();
        GDALOpenInfo oOpenInfo(m_poDS->m_osDirName.c_str(), GA_ReadOnly);
        bool bRetryFileGDBUnused = false;
        if (!poDS->Open(&oOpenInfo, bRetryFileGDBUnused))
            return nullptr;
        auto poVatLayer = poDS->BuildLayerFromName(m_osVATTableName.c_str());
        if (!poVatLayer)
            return nullptr;
        return new GDALOpenFileGDBRasterAttributeTable(
            std::move(poDS), m_osVATTableName, std::move(poVatLayer));
    }

    int GetColumnCount() const override
    {
        return m_poVATLayer->GetLayerDefn()->GetFieldCount();
    }

    int GetRowCount() const override
    {
        return static_cast<int>(m_poVATLayer->GetFeatureCount());
    }

    const char *GetNameOfCol(int iCol) const override
    {
        if (iCol < 0 || iCol >= GetColumnCount())
            return nullptr;
        return m_poVATLayer->GetLayerDefn()->GetFieldDefn(iCol)->GetNameRef();
    }

    GDALRATFieldUsage GetUsageOfCol(int iCol) const override
    {
        const char *pszColName = GetNameOfCol(iCol);
        return pszColName && EQUAL(pszColName, "Value")   ? GFU_MinMax
               : pszColName && EQUAL(pszColName, "Count") ? GFU_PixelCount
                                                          : GFU_Generic;
    }

    int GetColOfUsage(GDALRATFieldUsage eUsage) const override
    {
        if (eUsage == GFU_MinMax)
            return m_poVATLayer->GetLayerDefn()->GetFieldIndex("Value");
        if (eUsage == GFU_PixelCount)
            return m_poVATLayer->GetLayerDefn()->GetFieldIndex("Count");
        return -1;
    }

    GDALRATFieldType GetTypeOfCol(int iCol) const override
    {
        if (iCol < 0 || iCol >= GetColumnCount())
            return GFT_Integer;
        switch (m_poVATLayer->GetLayerDefn()->GetFieldDefn(iCol)->GetType())
        {
            case OFTInteger:
                return GFT_Integer;
            case OFTReal:
                return GFT_Real;
            default:
                break;
        }
        return GFT_String;
    }

    const char *GetValueAsString(int iRow, int iField) const override
    {
        auto poFeat =
            std::unique_ptr<OGRFeature>(m_poVATLayer->GetFeature(iRow + 1));
        if (!poFeat || iField >= poFeat->GetFieldCount())
            return "";
        m_osCachedValue = poFeat->GetFieldAsString(iField);
        return m_osCachedValue.c_str();
    }

    int GetValueAsInt(int iRow, int iField) const override
    {
        auto poFeat =
            std::unique_ptr<OGRFeature>(m_poVATLayer->GetFeature(iRow + 1));
        if (!poFeat || iField >= poFeat->GetFieldCount())
            return 0;
        return poFeat->GetFieldAsInteger(iField);
    }

    double GetValueAsDouble(int iRow, int iField) const override
    {
        auto poFeat =
            std::unique_ptr<OGRFeature>(m_poVATLayer->GetFeature(iRow + 1));
        if (!poFeat || iField >= poFeat->GetFieldCount())
            return 0;
        return poFeat->GetFieldAsDouble(iField);
    }

    void SetValue(int, int, const char *) override
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SetValue() not supported");
    }

    void SetValue(int, int, int) override
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SetValue() not supported");
    }

    void SetValue(int, int, double) override
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SetValue() not supported");
    }

    int ChangesAreWrittenToFile() override
    {
        return false;
    }

    CPLErr SetTableType(const GDALRATTableType) override
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SetTableType() not supported");
        return CE_Failure;
    }

    GDALRATTableType GetTableType() const override
    {
        return GRTT_THEMATIC;
    }

    void RemoveStatistics() override
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "RemoveStatistics() not supported");
    }
};

/************************************************************************/
/*                       GDALOpenFileGDBRasterBand                      */
/************************************************************************/

class GDALOpenFileGDBRasterBand final : public GDALRasterBand
{
    friend class OGROpenFileGDBDataSource;
    std::vector<GByte> m_abyTmpBuffer{};
    int m_nBitWidth = 0;
    int m_nOverviewLevel = 0;
    std::vector<std::unique_ptr<GDALOpenFileGDBRasterBand>>
        m_apoOverviewBands{};
    bool m_bIsMask = false;
    std::unique_ptr<GDALOpenFileGDBRasterBand> m_poMaskBandOwned{};
    GDALOpenFileGDBRasterBand *m_poMainBand = nullptr;
    GDALOpenFileGDBRasterBand *m_poMaskBand = nullptr;
    bool m_bHasNoData = false;
    double m_dfNoData = 0.0;
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

    CPL_DISALLOW_COPY_ASSIGN(GDALOpenFileGDBRasterBand)

  public:
    GDALOpenFileGDBRasterBand(OGROpenFileGDBDataSource *poDSIn, int nBandIn,
                              GDALDataType eDT, int nBitWidth, int nBlockWidth,
                              int nBlockHeight, int nOverviewLevel,
                              bool bIsMask);

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;

    int GetOverviewCount() override
    {
        return static_cast<int>(m_apoOverviewBands.size());
    }

    GDALRasterBand *GetOverview(int i) override
    {
        return (i >= 0 && i < GetOverviewCount()) ? m_apoOverviewBands[i].get()
                                                  : nullptr;
    }

    GDALRasterBand *GetMaskBand() override
    {
        return m_poMaskBand ? m_poMaskBand : GDALRasterBand::GetMaskBand();
    }

    int GetMaskFlags() override
    {
        return m_poMaskBand ? GMF_PER_DATASET : GDALRasterBand::GetMaskFlags();
    }

    double GetNoDataValue(int *pbHasNoData) override
    {
        if (pbHasNoData)
            *pbHasNoData = m_bHasNoData;
        return m_dfNoData;
    }

    GDALRasterAttributeTable *GetDefaultRAT() override;
};

#endif /* ndef OGR_OPENFILEGDB_H_INCLUDED */
