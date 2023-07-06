/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Definition of classes for OGR GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
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

#ifndef OGR_GEOPACKAGE_H_INCLUDED
#define OGR_GEOPACKAGE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogrsqlitebase.h"
#include "gpkgmbtilescommon.h"
#include "ogrsqliteutility.h"
#include "cpl_threadsafe_queue.hpp"
#include "ograrrowarrayhelper.h"

#include <condition_variable>
#include <limits>
#include <mutex>
#include <queue>
#include <vector>
#include <set>
#include <thread>

#define UNKNOWN_SRID -2
#define DEFAULT_SRID 0

#define ENABLE_GPKG_OGR_CONTENTS

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) ||     \
    defined(ALLOW_FORMAT_DUMPS)
// Enable accepting a SQL dump (starting with a "-- SQL GPKG" line) as a valid
// file. This makes fuzzer life easier
#define ENABLE_SQL_GPKG_FORMAT
#endif

typedef enum
{
    GPKG_ATTRIBUTES,
    NOT_REGISTERED,
} GPKGASpatialVariant;

// Requirement 2
static const GUInt32 GP10_APPLICATION_ID = 0x47503130U;
static const GUInt32 GP11_APPLICATION_ID = 0x47503131U;
static const GUInt32 GPKG_APPLICATION_ID = 0x47504B47U;
static const GUInt32 GPKG_1_2_VERSION = 10200U;
static const GUInt32 GPKG_1_3_VERSION = 10300U;
static const GUInt32 GPKG_1_4_VERSION = 10400U;

static const size_t knApplicationIdPos = 68;
static const size_t knUserVersionPos = 60;

struct GPKGExtensionDesc
{
    CPLString osExtensionName{};
    CPLString osDefinition{};
    CPLString osScope{};
};

struct GPKGContentsDesc
{
    CPLString osDataType{};
    CPLString osIdentifier{};
    CPLString osDescription{};
    CPLString osMinX{};
    CPLString osMinY{};
    CPLString osMaxX{};
    CPLString osMaxY{};
};

class OGRGeoPackageLayer;

struct OGRGPKGTableLayerFillArrowArray
{
    std::unique_ptr<OGRArrowArrayHelper> psHelper{};
    int nCountRows = 0;
    bool bErrorOccurred = false;
    OGRFeatureDefn *poFeatureDefn = nullptr;
    OGRGeoPackageLayer *poLayer = nullptr;
    struct tm brokenDown
    {
    };
    sqlite3 *hDB = nullptr;
    int nMaxBatchSize = 0;
    bool bAsynchronousMode = false;
    std::mutex oMutex{};
    std::condition_variable oCV{};
    bool bIsFinished = false;
};

/************************************************************************/
/*                          GDALGeoPackageDataset                       */
/************************************************************************/

class OGRGeoPackageTableLayer;

class GDALGeoPackageDataset final : public OGRSQLiteBaseDataSource,
                                    public GDALGPKGMBTilesLikePseudoDataset
{
    friend class GDALGeoPackageRasterBand;
    friend class OGRGeoPackageLayer;
    friend class OGRGeoPackageTableLayer;
    friend void OGRGeoPackageTransform(sqlite3_context *pContext, int argc,
                                       sqlite3_value **argv);

    std::string m_osFilenameInZip{};
    void *m_pSQLFunctionData = nullptr;
    GUInt32 m_nApplicationId = GPKG_APPLICATION_ID;
    GUInt32 m_nUserVersion = GPKG_1_2_VERSION;
    OGRGeoPackageTableLayer **m_papoLayers = nullptr;
    int m_nLayers = 0;
    void CheckUnknownExtensions(bool bCheckRasterTable = false);
#ifdef ENABLE_GPKG_OGR_CONTENTS
    bool m_bHasGPKGOGRContents = false;
#endif
    bool m_bHasGPKGGeometryColumns = false;
    bool m_bHasDefinition12_063 = false;
    bool m_bHasEpochColumn =
        false;  // whether gpkg_spatial_ref_sys has a epoch column
    bool m_bNonSpatialTablesNonRegisteredInGpkgContentsFound = false;

    CPLString m_osIdentifier{};
    bool m_bIdentifierAsCO = false;
    CPLString m_osDescription{};
    bool m_bDescriptionAsCO = false;
    bool m_bGridCellEncodingAsCO = false;
    bool m_bHasReadMetadataFromStorage = false;
    bool m_bMetadataDirty = false;
    CPLStringList m_aosSubDatasets{};
    OGRSpatialReference m_oSRS{};
    bool m_bRecordInsertedInGPKGContent = false;
    bool m_bGeoTransformValid = false;
    double m_adfGeoTransform[6];
    int m_nSRID = -1;  // Unknown Cartesain
    double m_dfTMSMinX = 0.0;
    double m_dfTMSMaxY = 0.0;
    int m_nBandCountFromMetadata = 0;
    std::unique_ptr<GDALColorTable> m_poCTFromMetadata{};
    std::string m_osTFFromMetadata{};
    std::string m_osNodataValueFromMetadata{};

    // Used by OGRGeoPackageTransform
    int m_nLastCachedCTSrcSRId = -1;
    int m_nLastCachedCTDstSRId = -1;
    std::unique_ptr<OGRCoordinateTransformation> m_poLastCachedCT{};

    int m_nOverviewCount = 0;
    GDALGeoPackageDataset **m_papoOverviewDS = nullptr;
    bool m_bZoomOther = false;

    bool m_bInFlushCache = false;

    bool m_bDateTimeWithTZ = true;

    bool m_bRemoveOGREmptyTable = false;

    CPLString m_osTilingScheme = "CUSTOM";

    bool ComputeTileAndPixelShifts();
    bool AllocCachedTiles();
    bool InitRaster(GDALGeoPackageDataset *poParentDS, const char *pszTableName,
                    double dfMinX, double dfMinY, double dfMaxX, double dfMaxY,
                    const char *pszContentsMinX, const char *pszContentsMinY,
                    const char *pszContentsMaxX, const char *pszContentsMaxY,
                    char **papszOpenOptions, const SQLResult &oResult,
                    int nIdxInResult);
    bool InitRaster(GDALGeoPackageDataset *poParentDS, const char *pszTableName,
                    int nZoomLevel, int nBandCount, double dfTMSMinX,
                    double dfTMSMaxY, double dfPixelXSize, double dfPixelYSize,
                    int nTileWidth, int nTileHeight, int nTileMatrixWidth,
                    int nTileMatrixHeight, double dfGDALMinX, double dfGDALMinY,
                    double dfGDALMaxX, double dfGDALMaxY);

    bool OpenRaster(const char *pszTableName, const char *pszIdentifier,
                    const char *pszDescription, int nSRSId, double dfMinX,
                    double dfMinY, double dfMaxX, double dfMaxY,
                    const char *pszContentsMinX, const char *pszContentsMinY,
                    const char *pszContentsMaxX, const char *pszContentsMaxY,
                    bool bIsTiles, char **papszOptions);
    CPLErr FinalizeRasterRegistration();

    bool RegisterWebPExtension();
    bool RegisterZoomOtherExtension();
    void ParseCompressionOptions(char **papszOptions);

    bool HasMetadataTables() const;
    bool CreateMetadataTables();
    const char *CheckMetadataDomain(const char *pszDomain);
    void
    WriteMetadata(CPLXMLNode *psXMLNode, /* will be destroyed by the method */
                  const char *pszTableName);
    void FlushMetadata();

    int FindLayerIndex(const char *pszLayerName);

    bool HasGriddedCoverageAncillaryTable();
    bool CreateTileGriddedTable(char **papszOptions);

    void RemoveOGREmptyTable();

    std::map<CPLString, CPLString> m_oMapNameToType{};
    const std::map<CPLString, CPLString> &GetNameTypeMapFromSQliteMaster();
    void RemoveTableFromSQLiteMasterCache(const char *pszTableName);

    bool m_bMapTableToExtensionsBuilt = false;
    std::map<CPLString, std::vector<GPKGExtensionDesc>>
        m_oMapTableToExtensions{};
    const std::map<CPLString, std::vector<GPKGExtensionDesc>> &
    GetUnknownExtensionsTableSpecific();

    bool m_bMapTableToContentsBuilt = false;
    std::map<CPLString, GPKGContentsDesc> m_oMapTableToContents{};
    const std::map<CPLString, GPKGContentsDesc> &GetContents();

    std::map<int, OGRSpatialReference *> m_oMapSrsIdToSrs{};

    OGRErr DeleteLayerCommon(const char *pszLayerName);
    OGRErr DeleteRasterLayer(const char *pszLayerName);
    bool DeleteVectorOrRasterLayer(const char *pszLayerName);

    bool ConvertGpkgSpatialRefSysToExtensionWkt2();
    void DetectSpatialRefSysColumns();

    std::map<int, bool> m_oSetGPKGLayerWarnings{};

    void FixupWrongRTreeTrigger();
    void FixupWrongMedataReferenceColumnNameUpdate();
    void ClearCachedRelationships();
    void LoadRelationships() const;
    void LoadRelationshipsUsingRelatedTablesExtension() const;
    static std::string
    GenerateNameForRelationship(const char *pszBaseTableName,
                                const char *pszRelatedTableName,
                                const char *pszType);
    bool ValidateRelationship(const GDALRelationship *poRelationship,
                              std::string &failureReason);

    bool m_bIsGeometryTypeAggregateInterrupted = false;
    std::string m_osGeometryTypeAggregateResult{};

    // Used by GDALGeoPackageDataset::GetRasterLayerDataset()
    std::map<std::string, std::unique_ptr<GDALDataset>> m_oCachedRasterDS{};

    bool CloseDB();
    CPLErr Close() override;

    CPL_DISALLOW_COPY_ASSIGN(GDALGeoPackageDataset)

  public:
    GDALGeoPackageDataset();
    virtual ~GDALGeoPackageDataset();

    char **GetFileList(void) override;

    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    virtual char **GetMetadataDomainList() override;
    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr GetGeoTransform(double *padfGeoTransform) override;
    virtual CPLErr SetGeoTransform(double *padfGeoTransform) override;

    virtual CPLErr FlushCache(bool bAtClosing) override;
    virtual CPLErr IBuildOverviews(const char *, int, const int *, int,
                                   const int *, GDALProgressFunc, void *,
                                   CSLConstList papszOptions) override;

    virtual int GetLayerCount() override
    {
        return m_nLayers;
    }
    int Open(GDALOpenInfo *poOpenInfo, const std::string &osFilenameInZip);
    int Create(const char *pszFilename, int nXSize, int nYSize, int nBands,
               GDALDataType eDT, char **papszOptions);
    OGRLayer *GetLayer(int iLayer) override;
    OGRErr DeleteLayer(int iLayer) override;
    OGRLayer *ICreateLayer(const char *pszLayerName,
                           OGRSpatialReference *poSpatialRef,
                           OGRwkbGeometryType eGType,
                           char **papszOptions) override;
    int TestCapability(const char *) override;

    std::vector<std::string>
    GetFieldDomainNames(CSLConstList papszOptions = nullptr) const override;
    const OGRFieldDomain *
    GetFieldDomain(const std::string &name) const override;
    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
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

    virtual std::pair<OGRLayer *, IOGRSQLiteGetSpatialWhere *>
    GetLayerWithGetSpatialWhereByName(const char *pszName) override;

    virtual OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;
    virtual void ReleaseResultSet(OGRLayer *poLayer) override;

    virtual OGRErr CommitTransaction() override;
    virtual OGRErr RollbackTransaction() override;

    inline bool IsInTransaction() const
    {
        return nSoftTransactionLevel > 0;
    }

    int GetSrsId(const OGRSpatialReference &oSRS);
    const char *GetSrsName(const OGRSpatialReference &oSRS);
    OGRSpatialReference *GetSpatialRef(int iSrsId,
                                       bool bFallbackToEPSG = false);
    OGRErr CreateExtensionsTableIfNecessary();
    bool HasExtensionsTable();
    void SetMetadataDirty()
    {
        m_bMetadataDirty = true;
    }

    bool HasDataColumnsTable() const;
    bool HasDataColumnConstraintsTable() const;
    bool HasDataColumnConstraintsTableGPKG_1_0() const;
    bool CreateColumnsTableAndColumnConstraintsTablesIfNecessary();
    bool HasGpkgextRelationsTable() const;
    bool CreateRelationsTableIfNecessary();
    bool HasQGISLayerStyles() const;
    bool HasNonSpatialTablesNonRegisteredInGpkgContents() const
    {
        return m_bNonSpatialTablesNonRegisteredInGpkgContentsFound;
    }

    const char *GetGeometryTypeString(OGRwkbGeometryType eType);

    void ResetReadingAllLayers();
    OGRErr UpdateGpkgContentsLastChange(const char *pszTableName);

    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    static std::string GetCurrentDateEscapedSQL();

    bool IsGeometryTypeAggregateInterrupted() const
    {
        return m_bIsGeometryTypeAggregateInterrupted;
    }
    void SetGeometryTypeAggregateInterrupted(bool b)
    {
        m_bIsGeometryTypeAggregateInterrupted = b;
        if (b)
            sqlite3_interrupt(hDB);
    }
    void SetGeometryTypeAggregateResult(const std::string &s)
    {
        m_osGeometryTypeAggregateResult = s;
    }
    const std::string &GetGeometryTypeAggregateResult() const
    {
        return m_osGeometryTypeAggregateResult;
    }

    GDALDataset *GetRasterLayerDataset(const char *pszLayerName);

  protected:
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, int, int *, GSpacing, GSpacing,
                             GSpacing,
                             GDALRasterIOExtraArg *psExtraArg) override;

    // Coming from GDALGPKGMBTilesLikePseudoDataset

    virtual CPLErr IFlushCacheWithErrCode(bool bAtClosing) override;
    virtual int IGetRasterCount() override
    {
        return nBands;
    }
    virtual GDALRasterBand *IGetRasterBand(int nBand) override
    {
        return GetRasterBand(nBand);
    }
    virtual sqlite3 *IGetDB() override
    {
        return GetDB();
    }
    virtual bool IGetUpdate() override
    {
        return GetUpdate();
    }
    virtual bool ICanIWriteBlock() override;
    virtual OGRErr IStartTransaction() override
    {
        return SoftStartTransaction();
    }
    virtual OGRErr ICommitTransaction() override
    {
        return SoftCommitTransaction();
    }
    virtual const char *IGetFilename() override
    {
        return m_pszFilename;
    }
    virtual int GetRowFromIntoTopConvention(int nRow) override
    {
        return nRow;
    }

  private:
    OGRErr SetApplicationAndUserVersionId();
    bool ReOpenDB();
    bool OpenOrCreateDB(int flags);
    void InstallSQLFunctions();
    bool HasGDALAspatialExtension();
};

/************************************************************************/
/*                        GDALGeoPackageRasterBand                      */
/************************************************************************/

class GDALGeoPackageRasterBand final : public GDALGPKGMBTilesLikeRasterBand
{
    // Whether STATISTICS_MINIMUM and/or STATISTICS_MAXIMUM have been computed
    // from the min, max columns of the gpkg_2d_gridded_tile_ancillary table
    // (only for non-Byte data)
    bool m_bMinMaxComputedFromTileAncillary = false;
    double m_dfStatsMinFromTileAncillary =
        std::numeric_limits<double>::quiet_NaN();
    double m_dfStatsMaxFromTileAncillary =
        std::numeric_limits<double>::quiet_NaN();
    CPLStringList m_aosMD{};

    // Whether gpkg_metadata has been read to set initial metadata
    bool m_bHasReadMetadataFromStorage = false;

    // Whether STATISTICS_* have been set in this "session"
    bool m_bStatsMetadataSetInThisSession = false;

    bool m_bAddImplicitStatistics = true;

    void LoadBandMetadata();

  public:
    GDALGeoPackageRasterBand(GDALGeoPackageDataset *poDS, int nTileWidth,
                             int nTileHeight);

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int nIdx) override;

    virtual CPLErr SetNoDataValue(double dfNoDataValue) override;

    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;

    void AddImplicitStatistics(bool b)
    {
        m_bAddImplicitStatistics = b;
    }
    inline bool HaveStatsMetadataBeenSetInThisSession() const
    {
        return m_bStatsMetadataSetInThisSession;
    }
    void InvalidateStatistics();
};

/************************************************************************/
/*                           OGRGeoPackageLayer                         */
/************************************************************************/

class OGRGeoPackageLayer CPL_NON_FINAL : public OGRLayer,
                                         public IOGRSQLiteGetSpatialWhere
{
  protected:
    friend void OGR_GPKG_FillArrowArray_Step(sqlite3_context *pContext,
                                             int /*argc*/,
                                             sqlite3_value **argv);

    GDALGeoPackageDataset *m_poDS;

    OGRFeatureDefn *m_poFeatureDefn;
    GIntBig iNextShapeId;

    sqlite3_stmt *m_poQueryStatement;
    bool bDoStep;
    bool m_bEOF = false;

    char *m_pszFidColumn;

    int iFIDCol;
    int iGeomCol;
    int *panFieldOrdinals;

    void ClearStatement();
    virtual OGRErr ResetStatement() = 0;

    void BuildFeatureDefn(const char *pszLayerName, sqlite3_stmt *hStmt);

    OGRFeature *TranslateFeature(sqlite3_stmt *hStmt);
    bool ParseDateField(const char *pszTxt, OGRField *psField,
                        const OGRFieldDefn *poFieldDefn, GIntBig nFID);
    bool ParseDateField(sqlite3_stmt *hStmt, int iRawField, int nSqlite3ColType,
                        OGRField *psField, const OGRFieldDefn *poFieldDefn,
                        GIntBig nFID);
    bool ParseDateTimeField(const char *pszTxt, OGRField *psField,
                            const OGRFieldDefn *poFieldDefn, GIntBig nFID);
    bool ParseDateTimeField(sqlite3_stmt *hStmt, int iRawField,
                            int nSqlite3ColType, OGRField *psField,
                            const OGRFieldDefn *poFieldDefn, GIntBig nFID);

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
    virtual int GetNextArrowArray(struct ArrowArrayStream *,
                                  struct ArrowArray *out_array) override;

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoPackageLayer)

  public:
    explicit OGRGeoPackageLayer(GDALGeoPackageDataset *poDS);
    virtual ~OGRGeoPackageLayer();
    /************************************************************************/
    /* OGR API methods */

    OGRFeature *GetNextFeature() override;
    const char *GetFIDColumn() override;
    void ResetReading() override;
    int TestCapability(const char *) override;
    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }
    OGRErr SetIgnoredFields(const char **papszFields) override;

    virtual bool HasFastSpatialFilter(int /*iGeomCol*/) override
    {
        return false;
    }
    virtual CPLString GetSpatialWhere(int /*iGeomCol*/,
                                      OGRGeometry * /*poFilterGeom*/) override
    {
        return "";
    }
};

/************************************************************************/
/*                        OGRGeoPackageTableLayer                       */
/************************************************************************/

struct OGRGPKGTableLayerFillArrowArray;

class OGRGeoPackageTableLayer final : public OGRGeoPackageLayer
{
    char *m_pszTableName = nullptr;
    bool m_bIsTable = true;  // sensible init for creation mode
    bool m_bIsSpatial = false;
    bool m_bIsInGpkgContents = false;
    bool m_bFeatureDefnCompleted = false;
    int m_iSrs = 0;
    int m_nZFlag = 0;
    int m_nMFlag = 0;
    OGREnvelope *m_poExtent = nullptr;
#ifdef ENABLE_GPKG_OGR_CONTENTS
    GIntBig m_nTotalFeatureCount = -1;
    bool m_bOGRFeatureCountTriggersEnabled = false;
    bool m_bAddOGRFeatureCountTriggers = false;
    bool m_bFeatureCountTriggersDeletedInTransaction = false;
#endif
    CPLString m_soColumns{};
    std::vector<bool> m_abGeneratedColumns{};  // .size() ==
        // m_poFeatureDefn->GetFieldDefnCount()
    CPLString m_soFilter{};
    CPLString osQuery{};
    CPLString m_osRTreeName{};
    CPLString m_osFIDForRTree{};
    bool m_bExtentChanged = false;
    bool m_bContentChanged = false;
    sqlite3_stmt *m_poUpdateStatement = nullptr;
    std::string m_osUpdateStatementSQL{};
    bool m_bInsertStatementWithFID = false;
    bool m_bInsertStatementWithUpsert = false;
    std::string m_osInsertStatementUpsertUniqueColumnName{};
    sqlite3_stmt *m_poInsertStatement = nullptr;
    sqlite3_stmt *m_poGetFeatureStatement = nullptr;
    bool m_bDeferredSpatialIndexCreation = false;
    // m_bHasSpatialIndex cannot be bool.  -1 is unset.
    int m_bHasSpatialIndex = -1;
    bool m_bDropRTreeTable = false;
    bool m_abHasGeometryExtension[wkbTriangle + 1];
    bool m_bPreservePrecision = true;
    bool m_bTruncateFields = false;
    bool m_bDeferredCreation = false;
    bool m_bTableCreatedInTransaction = false;
    int m_iFIDAsRegularColumnIndex = -1;
    std::string m_osInsertionBuffer{};  // used by FeatureBindParameters to
                                        // store datetime values

    CPLString m_osIdentifierLCO{};
    CPLString m_osDescriptionLCO{};
    bool m_bHasReadMetadataFromStorage = false;
    bool m_bHasTriedDetectingFID64 = false;
    GPKGASpatialVariant m_eASpatialVariant = GPKG_ATTRIBUTES;
    std::set<OGRwkbGeometryType> m_eSetBadGeomTypeWarned{};

    int m_nIsCompatOfOptimizedGetNextArrowArray = -1;

    int m_nCountInsertInTransactionThreshold = -1;
    GIntBig m_nCountInsertInTransaction = 0;
    std::vector<CPLString> m_aoRTreeTriggersSQL{};
    bool m_bUpdate1TriggerDisabled = false;
    bool m_bHasUpdate6And7Triggers = false;
    std::string m_osUpdate1Trigger{};
    typedef struct
    {
        GIntBig nId;
        float fMinX;
        float fMinY;
        float fMaxX;
        float fMaxY;
    } GPKGRTreeEntry;
    std::vector<GPKGRTreeEntry> m_aoRTreeEntries{};

    // Variables used for background RTree building
    std::string m_osAsyncDBName{};
    std::string m_osAsyncDBAttachName{};
    sqlite3 *m_hAsyncDBHandle = nullptr;
    cpl::ThreadSafeQueue<std::vector<GPKGRTreeEntry>> m_oQueueRTreeEntries{};
    bool m_bAllowedRTreeThread = false;
    bool m_bThreadRTreeStarted = false;
    bool m_bErrorDuringRTreeThread = false;
    size_t m_nRTreeBatchSize =
        10 * 1000;  // maximum size of a std::vector<GPKGRTreeEntry> item in
                    // m_oQueueRTreeEntries
    size_t m_nRTreeBatchesBeforeStart =
        10;  // number of items in m_oQueueRTreeEntries before starting the
             // thread
    std::thread m_oThreadRTree{};

    void StartAsyncRTree();
    void CancelAsyncRTree();
    void RemoveAsyncRTreeTempDB();
    void AsyncRTreeThreadFunction();

    virtual OGRErr ResetStatement() override;

    void BuildWhere();
    OGRErr RegisterGeometryColumn();

    CPLString
    GetColumnsOfCreateTable(const std::vector<OGRFieldDefn *> &apoFields);
    CPLString
    BuildSelectFieldList(const std::vector<OGRFieldDefn *> &apoFields);
    OGRErr RecreateTable(const CPLString &osColumnsForCreate,
                         const CPLString &osFieldListForSelect);
#ifdef ENABLE_GPKG_OGR_CONTENTS
    void CreateFeatureCountTriggers(const char *pszTableName = nullptr);
    void DisableFeatureCountTriggers(bool bNullifyFeatureCount = true);
#endif

    void CheckGeometryType(OGRFeature *poFeature);

    OGRErr ReadTableDefinition();
    void InitView();

    bool DoSpecialProcessingForColumnCreation(OGRFieldDefn *poField);

    bool StartDeferredSpatialIndexUpdate();
    bool FlushPendingSpatialIndexUpdate();
    void WorkaroundUpdate1TriggerIssue();
    void RevertWorkaroundUpdate1TriggerIssue();

    OGRErr RenameFieldInAuxiliaryTables(const char *pszOldName,
                                        const char *pszNewName);

    OGRErr CreateOrUpsertFeature(OGRFeature *poFeature, bool bUpsert);

    GIntBig GetTotalFeatureCount();

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoPackageTableLayer)

    // Used when m_nIsCompatOfOptimizedGetNextArrowArray == TRUE
    struct ArrowArrayPrefetchTask
    {
        std::thread m_oThread{};
        std::condition_variable m_oCV{};
        std::mutex m_oMutex{};
        bool m_bArrayReady = false;
        bool m_bFetchRows = false;
        bool m_bStop = false;
        std::unique_ptr<GDALGeoPackageDataset> m_poDS{};
        OGRGeoPackageTableLayer *m_poLayer{};
        GIntBig m_iStartShapeId = 0;
        std::unique_ptr<struct ArrowArray> m_psArrowArray = nullptr;
    };
    std::queue<std::unique_ptr<ArrowArrayPrefetchTask>>
        m_oQueueArrowArrayPrefetchTasks{};

    // Used when m_nIsCompatOfOptimizedGetNextArrowArray == FALSE
    std::thread m_oThreadNextArrowArray{};
    std::unique_ptr<OGRGPKGTableLayerFillArrowArray> m_poFillArrowArray{};
    std::unique_ptr<GDALGeoPackageDataset> m_poOtherDS{};

    virtual int GetNextArrowArray(struct ArrowArrayStream *,
                                  struct ArrowArray *out_array) override;
    int GetNextArrowArrayInternal(struct ArrowArray *out_array);
    int GetNextArrowArrayAsynchronous(struct ArrowArray *out_array);
    void GetNextArrowArrayAsynchronousWorker();
    void CancelAsyncNextArrowArray();

  public:
    OGRGeoPackageTableLayer(GDALGeoPackageDataset *poDS,
                            const char *pszTableName);
    virtual ~OGRGeoPackageTableLayer();

    /************************************************************************/
    /* OGR API methods */

    const char *GetName() override
    {
        return GetDescription();
    }
    const char *GetFIDColumn() override;
    OGRwkbGeometryType GetGeomType() override;
    const char *GetGeometryColumn() override;
    OGRFeatureDefn *GetLayerDefn() override;
    int TestCapability(const char *) override;
    OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK = TRUE) override;
    OGRErr CreateGeomField(OGRGeomFieldDefn *poGeomFieldIn,
                           int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iFieldToDelete) override;
    virtual OGRErr AlterFieldDefn(int iFieldToAlter,
                                  OGRFieldDefn *poNewFieldDefn,
                                  int nFlagsIn) override;
    virtual OGRErr
    AlterGeomFieldDefn(int iGeomFieldToAlter,
                       const OGRGeomFieldDefn *poNewGeomFieldDefn,
                       int nFlagsIn) override;
    virtual OGRErr ReorderFields(int *panMap) override;
    void ResetReading() override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;
    OGRErr DeleteFeature(GIntBig nFID) override;
    virtual void SetSpatialFilter(OGRGeometry *) override;
    virtual void SetSpatialFilter(int iGeomField, OGRGeometry *poGeom) override
    {
        OGRGeoPackageLayer::SetSpatialFilter(iGeomField, poGeom);
    }

    OGRErr SetAttributeFilter(const char *pszQuery) override;
    OGRErr SyncToDisk() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;
    GIntBig GetFeatureCount(int) override;
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRGeoPackageLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    OGRGeometryTypeCounter *GetGeometryTypes(int iGeomField, int nFlagsGGT,
                                             int &nEntryCountOut,
                                             GDALProgressFunc pfnProgress,
                                             void *pProgressData) override;

    void RecomputeExtent();

    void SetOpeningParameters(const char *pszTableName,
                              const char *pszObjectType, bool bIsInGpkgContents,
                              bool bIsSpatial, const char *pszGeomColName,
                              const char *pszGeomType, bool bHasZ, bool bHasM);
    void SetCreationParameters(OGRwkbGeometryType eGType,
                               const char *pszGeomColumnName, int bGeomNullable,
                               OGRSpatialReference *poSRS,
                               const char *pszFIDColumnName,
                               const char *pszIdentifier,
                               const char *pszDescription);
    void SetDeferredSpatialIndexCreation(bool bFlag);
    void SetASpatialVariant(GPKGASpatialVariant eASpatialVariant)
    {
        m_eASpatialVariant = eASpatialVariant;
        if (eASpatialVariant == GPKG_ATTRIBUTES)
            m_bIsInGpkgContents = true;
    }

    void CreateSpatialIndexIfNecessary();
    void FinishOrDisableThreadedRTree();
    bool CreateSpatialIndex(const char *pszTableName = nullptr);
    bool DropSpatialIndex(bool bCalledFromSQLFunction = false);
    CPLString ReturnSQLCreateSpatialIndexTriggers(const char *pszTableName,
                                                  const char *pszGeomColName);
    CPLString ReturnSQLDropSpatialIndexTriggers();

    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    virtual char **GetMetadataDomainList() override;

    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;

    virtual OGRErr Rename(const char *pszDstTableName) override;

    virtual bool HasFastSpatialFilter(int iGeomCol) override;
    virtual CPLString GetSpatialWhere(int iGeomCol,
                                      OGRGeometry *poFilterGeom) override;

    bool HasSpatialIndex();
    void SetPrecisionFlag(int bFlag)
    {
        m_bPreservePrecision = CPL_TO_BOOL(bFlag);
    }
    void SetTruncateFieldsFlag(int bFlag)
    {
        m_bTruncateFields = CPL_TO_BOOL(bFlag);
    }
    OGRErr RunDeferredCreationIfNecessary();
    bool RunDeferredDropRTreeTableIfNecessary();
    bool DoJobAtTransactionCommit();
    bool DoJobAtTransactionRollback();
    bool RunDeferredSpatialIndexUpdate();

#ifdef ENABLE_GPKG_OGR_CONTENTS
    bool GetAddOGRFeatureCountTriggers() const
    {
        return m_bAddOGRFeatureCountTriggers;
    }
    void SetAddOGRFeatureCountTriggers(bool b)
    {
        m_bAddOGRFeatureCountTriggers = b;
    }
    bool GetOGRFeatureCountTriggersDeletedInTransaction() const
    {
        return m_bFeatureCountTriggersDeletedInTransaction;
    }
    void SetOGRFeatureCountTriggersEnabled(bool b)
    {
        m_bOGRFeatureCountTriggersEnabled = b;
    }

    void DisableFeatureCount();
#endif

    bool CreateGeometryExtensionIfNecessary(OGRwkbGeometryType eGType);

    /************************************************************************/
    /* GPKG methods */

  private:
    bool CheckUpdatableTable(const char *pszOperation);
    OGRErr UpdateExtent(const OGREnvelope *poExtent);
    OGRErr SaveExtent();
    OGRErr SaveTimestamp();
    OGRErr BuildColumns();
    bool IsGeomFieldSet(OGRFeature *poFeature);
    std::string FeatureGenerateUpdateSQL(const OGRFeature *poFeature) const;
    std::string FeatureGenerateUpdateSQL(
        const OGRFeature *poFeature, int nUpdatedFieldsCount,
        const int *panUpdatedFieldsIdx, int nUpdatedGeomFieldsCount,
        const int *panUpdatedGeomFieldsIdx) const;
    CPLString
    FeatureGenerateInsertSQL(OGRFeature *poFeature, bool bAddFID,
                             bool bBindUnsetFields, bool bUpsert,
                             const std::string &osUpsertUniqueColumnName);
    OGRErr FeatureBindUpdateParameters(OGRFeature *poFeature,
                                       sqlite3_stmt *poStmt);
    OGRErr FeatureBindInsertParameters(OGRFeature *poFeature,
                                       sqlite3_stmt *poStmt, bool bAddFID,
                                       bool bBindUnsetFields);
    OGRErr FeatureBindParameters(OGRFeature *poFeature, sqlite3_stmt *poStmt,
                                 int *pnColCount, bool bAddFID,
                                 bool bBindUnsetFields, int nUpdatedFieldsCount,
                                 const int *panUpdatedFieldsIdx,
                                 int nUpdatedGeomFieldsCount,
                                 const int *panUpdatedGeomFieldsIdx);

    void UpdateContentsToNullExtent();

    void CheckUnknownExtensions();
    bool CreateGeometryExtensionIfNecessary(const OGRGeometry *poGeom);
};

/************************************************************************/
/*                         OGRGeoPackageSelectLayer                     */
/************************************************************************/

class OGRGeoPackageSelectLayer final : public OGRGeoPackageLayer,
                                       public IOGRSQLiteSelectLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRGeoPackageSelectLayer)

    OGRSQLiteSelectLayerCommonBehaviour *poBehavior = nullptr;

    virtual OGRErr ResetStatement() override;

  public:
    OGRGeoPackageSelectLayer(GDALGeoPackageDataset *, const CPLString &osSQL,
                             sqlite3_stmt *,
                             bool bUseStatementForGetNextFeature,
                             bool bEmptyLayer);
    virtual ~OGRGeoPackageSelectLayer();

    virtual void ResetReading() override;

    virtual OGRFeature *GetNextFeature() override;
    virtual GIntBig GetFeatureCount(int) override;

    virtual void SetSpatialFilter(OGRGeometry *poGeom) override
    {
        SetSpatialFilter(0, poGeom);
    }
    virtual void SetSpatialFilter(int iGeomField, OGRGeometry *) override;
    virtual OGRErr SetAttributeFilter(const char *) override;

    virtual int TestCapability(const char *) override;

    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override
    {
        return GetExtent(0, psExtent, bForce);
    }
    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce = TRUE) override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return OGRGeoPackageLayer::GetLayerDefn();
    }
    virtual char *&GetAttrQueryString() override
    {
        return m_pszAttrQueryString;
    }
    virtual OGRFeatureQuery *&GetFeatureQuery() override
    {
        return m_poAttrQuery;
    }
    virtual OGRGeometry *&GetFilterGeom() override
    {
        return m_poFilterGeom;
    }
    virtual int &GetIGeomFieldFilter() override
    {
        return m_iGeomFieldFilter;
    }
    virtual OGRSpatialReference *GetSpatialRef() override
    {
        return OGRGeoPackageLayer::GetSpatialRef();
    }
    virtual int InstallFilter(OGRGeometry *poGeomIn) override
    {
        return OGRGeoPackageLayer::InstallFilter(poGeomIn);
    }
    virtual int HasReadFeature() override
    {
        return iNextShapeId > 0;
    }
    virtual void BaseResetReading() override
    {
        OGRGeoPackageLayer::ResetReading();
    }
    virtual OGRFeature *BaseGetNextFeature() override
    {
        return OGRGeoPackageLayer::GetNextFeature();
    }
    virtual OGRErr BaseSetAttributeFilter(const char *pszQuery) override
    {
        return OGRGeoPackageLayer::SetAttributeFilter(pszQuery);
    }
    virtual GIntBig BaseGetFeatureCount(int bForce) override
    {
        return OGRGeoPackageLayer::GetFeatureCount(bForce);
    }
    virtual int BaseTestCapability(const char *pszCap) override
    {
        return OGRGeoPackageLayer::TestCapability(pszCap);
    }
    virtual OGRErr BaseGetExtent(OGREnvelope *psExtent, int bForce) override
    {
        return OGRGeoPackageLayer::GetExtent(psExtent, bForce);
    }
    virtual OGRErr BaseGetExtent(int iGeomField, OGREnvelope *psExtent,
                                 int bForce) override
    {
        return OGRGeoPackageLayer::GetExtent(iGeomField, psExtent, bForce);
    }
};

#endif /* OGR_GEOPACKAGE_H_INCLUDED */
