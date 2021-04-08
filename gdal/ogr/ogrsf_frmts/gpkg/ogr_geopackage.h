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
#include "ogr_sqlite.h"
#include "gpkgmbtilescommon.h"
#include "ogrsqliteutility.h"

#include <vector>
#include <set>

#define UNKNOWN_SRID   -2
#define DEFAULT_SRID    0

#define ENABLE_GPKG_OGR_CONTENTS

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) || defined(ALLOW_FORMAT_DUMPS)
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

static const size_t knApplicationIdPos = 68;
static const size_t knUserVersionPos = 60;

typedef struct
{
    CPLString osExtensionName;
    CPLString osDefinition;
    CPLString osScope;
} GPKGExtensionDesc;

typedef struct
{
    CPLString osDataType;
    CPLString osIdentifier;
    CPLString osDescription;
    CPLString osMinX;
    CPLString osMinY;
    CPLString osMaxX;
    CPLString osMaxY;
} GPKGContentsDesc;

/************************************************************************/
/*                          GDALGeoPackageDataset                       */
/************************************************************************/

class OGRGeoPackageTableLayer;

class GDALGeoPackageDataset final : public OGRSQLiteBaseDataSource, public GDALGPKGMBTilesLikePseudoDataset
{
    friend class GDALGeoPackageRasterBand;
    friend class OGRGeoPackageLayer;
    friend class OGRGeoPackageTableLayer;

    GUInt32             m_nApplicationId;
    GUInt32             m_nUserVersion;
    OGRGeoPackageTableLayer** m_papoLayers;
    int                 m_nLayers;
    void                CheckUnknownExtensions(bool bCheckRasterTable = false);
#ifdef ENABLE_GPKG_OGR_CONTENTS
    bool                m_bHasGPKGOGRContents;
#endif
    bool                m_bHasGPKGGeometryColumns;
    bool                m_bHasDefinition12_063;

    CPLString           m_osIdentifier;
    bool                m_bIdentifierAsCO;
    CPLString           m_osDescription;
    bool                m_bDescriptionAsCO;
    bool                m_bGridCellEncodingAsCO = false;
    bool                m_bHasReadMetadataFromStorage;
    bool                m_bMetadataDirty;
    CPLStringList       m_aosSubDatasets{};
    char               *m_pszProjection;
    bool                m_bRecordInsertedInGPKGContent;
    bool                m_bGeoTransformValid;
    double              m_adfGeoTransform[6];
    int                 m_nSRID;
    double              m_dfTMSMinX;
    double              m_dfTMSMaxY;

    int                 m_nOverviewCount;
    GDALGeoPackageDataset** m_papoOverviewDS;
    bool                m_bZoomOther;

    bool                m_bInFlushCache;

    bool                m_bDateTimeWithTZ = true;

    CPLString           m_osTilingScheme;

        bool            ComputeTileAndPixelShifts();
        bool            AllocCachedTiles();
        bool            InitRaster ( GDALGeoPackageDataset* poParentDS,
                                     const char* pszTableName,
                                     double dfMinX,
                                     double dfMinY,
                                     double dfMaxX,
                                     double dfMaxY,
                                     const char* pszContentsMinX,
                                     const char* pszContentsMinY,
                                     const char* pszContentsMaxX,
                                     const char* pszContentsMaxY,
                                     char** papszOpenOptions,
                                     const SQLResult& oResult,
                                     int nIdxInResult );
        bool            InitRaster ( GDALGeoPackageDataset* poParentDS,
                                     const char* pszTableName,
                                     int nZoomLevel,
                                     int nBandCount,
                                     double dfTMSMinX,
                                     double dfTMSMaxY,
                                     double dfPixelXSize,
                                     double dfPixelYSize,
                                     int nTileWidth,
                                     int nTileHeight,
                                     int nTileMatrixWidth,
                                     int nTileMatrixHeight,
                                     double dfGDALMinX,
                                     double dfGDALMinY,
                                     double dfGDALMaxX,
                                     double dfGDALMaxY );

        bool    OpenRaster( const char* pszTableName,
                            const char* pszIdentifier,
                            const char* pszDescription,
                            int nSRSId,
                            double dfMinX,
                            double dfMinY,
                            double dfMaxX,
                            double dfMaxY,
                            const char* pszContentsMinX,
                            const char* pszContentsMinY,
                            const char* pszContentsMaxX,
                            const char* pszContentsMaxY,
                            bool bIsTiles,
                            char** papszOptions );
        CPLErr   FinalizeRasterRegistration();

        bool                    RegisterWebPExtension();
        bool                    RegisterZoomOtherExtension();
        void                    ParseCompressionOptions(char** papszOptions);

        bool                    HasMetadataTables() const;
        bool                    CreateMetadataTables();
        const char*             CheckMetadataDomain( const char* pszDomain );
        void                    WriteMetadata(CPLXMLNode* psXMLNode, /* will be destroyed by the method */
                                              const char* pszTableName);
        CPLErr                  FlushMetadata();

        int                     FindLayerIndex(const char* pszLayerName);

        bool                    HasGriddedCoverageAncillaryTable();
        bool                    CreateTileGriddedTable(char** papszOptions);

        void                    RemoveOGREmptyTable();

        std::map<CPLString, CPLString> m_oMapNameToType;
        const std::map<CPLString, CPLString>&
                                        GetNameTypeMapFromSQliteMaster();
        void RemoveTableFromSQLiteMasterCache(const char* pszTableName);

        bool                    m_bMapTableToExtensionsBuilt;
        std::map< CPLString, std::vector<GPKGExtensionDesc> > m_oMapTableToExtensions;
        const std::map< CPLString, std::vector<GPKGExtensionDesc> > &
                                        GetUnknownExtensionsTableSpecific();

        bool                    m_bMapTableToContentsBuilt;
        std::map< CPLString, GPKGContentsDesc > m_oMapTableToContents;
        const std::map< CPLString, GPKGContentsDesc > & GetContents();

        std::map<int, OGRSpatialReference*> m_oMapSrsIdToSrs;

        OGRErr              DeleteLayerCommon( const char* pszLayerName );
        OGRErr              DeleteRasterLayer( const char* pszLayerName );
        bool                DeleteVectorOrRasterLayer(
                                                const char* pszLayerName );

        bool                ConvertGpkgSpatialRefSysToExtensionWkt2();

        std::map<int, bool> m_oSetGPKGLayerWarnings{};

        void                FixupWrongRTreeTrigger();

    public:
                            GDALGeoPackageDataset();
                            virtual ~GDALGeoPackageDataset();

        virtual char **     GetMetadata( const char *pszDomain = nullptr ) override;
        virtual const char *GetMetadataItem( const char * pszName,
                                             const char * pszDomain = "" ) override;
        virtual char **     GetMetadataDomainList() override;
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                         const char * pszDomain = "" ) override;
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                             const char * pszValue,
                                             const char * pszDomain = "" ) override;

        virtual const char* _GetProjectionRef() override;
        virtual CPLErr      _SetProjection( const char* pszProjection ) override;
        const OGRSpatialReference* GetSpatialRef() const override {
            return GetSpatialRefFromOldGetProjectionRef();
        }
        CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
            return OldSetProjectionFromSetSpatialRef(poSRS);
        }

        virtual CPLErr      GetGeoTransform( double* padfGeoTransform ) override;
        virtual CPLErr      SetGeoTransform( double* padfGeoTransform ) override;

        virtual void        FlushCache() override;
        virtual CPLErr      IBuildOverviews( const char *, int, int *,
                                             int, int *, GDALProgressFunc, void * ) override;

        virtual int         GetLayerCount() override { return m_nLayers; }
        int                 Open( GDALOpenInfo* poOpenInfo );
        int                 Create( const char * pszFilename,
                                    int nXSize,
                                    int nYSize,
                                    int nBands,
                                    GDALDataType eDT,
                                    char **papszOptions );
        OGRLayer*           GetLayer( int iLayer ) override;
        OGRErr              DeleteLayer( int iLayer ) override;
        OGRLayer*           ICreateLayer( const char * pszLayerName,
                                         OGRSpatialReference * poSpatialRef,
                                         OGRwkbGeometryType eGType,
                                         char **papszOptions ) override;
        int                 TestCapability( const char * ) override;

        const OGRFieldDomain* GetFieldDomain(const std::string& name) const override;
        bool                AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                           std::string& failureReason) override;

        virtual std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetLayerWithGetSpatialWhereByName( const char* pszName ) override;

        virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect ) override;
        virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

        virtual OGRErr      CommitTransaction() override;
        virtual OGRErr      RollbackTransaction() override;

        inline bool         IsInTransaction() const { return nSoftTransactionLevel > 0; }

        int                 GetSrsId( const OGRSpatialReference& oSRS );
        const char*         GetSrsName( const OGRSpatialReference& oSRS );
        OGRSpatialReference* GetSpatialRef( int iSrsId, bool bFallbackToEPSG = false );
        OGRErr              CreateExtensionsTableIfNecessary();
        bool                HasExtensionsTable();
        void                SetMetadataDirty() { m_bMetadataDirty = true; }

        bool                    HasDataColumnsTable() const;
        bool                    HasDataColumnConstraintsTable() const;
        bool                CreateColumnsTableAndColumnConstraintsTablesIfNecessary();

        const char*         GetGeometryTypeString(OGRwkbGeometryType eType);

        void                ResetReadingAllLayers();
        OGRErr              UpdateGpkgContentsLastChange(
                                                const char* pszTableName);

        static GDALDataset* CreateCopy( const char *pszFilename,
                                                   GDALDataset *poSrcDS,
                                                   int bStrict,
                                                   char ** papszOptions,
                                                   GDALProgressFunc pfnProgress,
                                                   void * pProgressData );

        static std::string GetCurrentDateEscapedSQL();

    protected:
        // Coming from GDALGPKGMBTilesLikePseudoDataset

        virtual CPLErr                  IFlushCacheWithErrCode() override;
        virtual int                     IGetRasterCount() override { return nBands; }
        virtual GDALRasterBand*         IGetRasterBand(int nBand) override { return GetRasterBand(nBand); }
        virtual sqlite3                *IGetDB() override { return GetDB(); }
        virtual bool                    IGetUpdate() override { return GetUpdate(); }
        virtual bool                    ICanIWriteBlock() override;
        virtual OGRErr                  IStartTransaction() override { return SoftStartTransaction(); }
        virtual OGRErr                  ICommitTransaction() override { return SoftCommitTransaction(); }
        virtual const char             *IGetFilename() override { return m_pszFilename; }
        virtual int                     GetRowFromIntoTopConvention(int nRow) override { return nRow; }

    private:

        OGRErr              PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected);
        OGRErr              SetApplicationAndUserVersionId();
        bool                ReOpenDB();
        bool                OpenOrCreateDB( int flags );
        void                InstallSQLFunctions();
        bool                HasGDALAspatialExtension();
};

/************************************************************************/
/*                        GDALGeoPackageRasterBand                      */
/************************************************************************/

class GDALGeoPackageRasterBand final: public GDALGPKGMBTilesLikeRasterBand
{
        bool                    m_bStatsComputed;

    public:
                                GDALGeoPackageRasterBand(GDALGeoPackageDataset* poDS,
                                                         int nTileWidth, int nTileHeight);

        virtual int             GetOverviewCount() override;
        virtual GDALRasterBand* GetOverview(int nIdx) override;

        virtual CPLErr          SetNoDataValue( double dfNoDataValue ) override;

        virtual char**          GetMetadata(const char* pszDomain = "") override;
        virtual const char*     GetMetadataItem(const char* pszName,
                                                const char* pszDomain = "") override;
};

/************************************************************************/
/*                           OGRGeoPackageLayer                         */
/************************************************************************/

class OGRGeoPackageLayer CPL_NON_FINAL: public OGRLayer, public IOGRSQLiteGetSpatialWhere
{
  protected:
    GDALGeoPackageDataset *m_poDS;

    OGRFeatureDefn*      m_poFeatureDefn;
    int                  iNextShapeId;

    sqlite3_stmt        *m_poQueryStatement;
    bool                 bDoStep;
    bool                 m_bEOF = false;

    char                *m_pszFidColumn;

    int                 iFIDCol;
    int                 iGeomCol;
    int                *panFieldOrdinals;

    void                ClearStatement();
    virtual OGRErr      ResetStatement() = 0;

    void                BuildFeatureDefn( const char *pszLayerName,
                                           sqlite3_stmt *hStmt );

    OGRFeature*         TranslateFeature(sqlite3_stmt* hStmt);

  public:

    explicit            OGRGeoPackageLayer(GDALGeoPackageDataset* poDS);
                        virtual ~OGRGeoPackageLayer();
    /************************************************************************/
    /* OGR API methods */

    OGRFeature*         GetNextFeature() override;
    const char*         GetFIDColumn() override;
    void                ResetReading() override;
    int                 TestCapability( const char * ) override;
    OGRFeatureDefn*     GetLayerDefn() override { return m_poFeatureDefn; }

    virtual int          HasFastSpatialFilter(int /*iGeomCol*/) override { return FALSE; }
    virtual CPLString    GetSpatialWhere(int /*iGeomCol*/,
                                         OGRGeometry* /*poFilterGeom*/) override { return ""; }
};

/************************************************************************/
/*                        OGRGeoPackageTableLayer                       */
/************************************************************************/

class OGRGeoPackageTableLayer final : public OGRGeoPackageLayer
{
    char*                       m_pszTableName;
    bool                        m_bIsTable;
    bool                        m_bIsSpatial;
    bool                        m_bIsInGpkgContents;
    bool                        m_bFeatureDefnCompleted;
    int                         m_iSrs;
    int                         m_nZFlag = 0;
    int                         m_nMFlag = 0;
    OGREnvelope*                m_poExtent;
#ifdef ENABLE_GPKG_OGR_CONTENTS
    GIntBig                     m_nTotalFeatureCount;
    bool                        m_bOGRFeatureCountTriggersEnabled;
    bool                        m_bAddOGRFeatureCountTriggers;
    bool                        m_bFeatureCountTriggersDeletedInTransaction;
#endif
    CPLString                   m_soColumns;
    CPLString                   m_soFilter;
    CPLString                   osQuery;
    CPLString                   m_osRTreeName;
    CPLString                   m_osFIDForRTree;
    bool                        m_bExtentChanged;
    bool                        m_bContentChanged;
    sqlite3_stmt*               m_poUpdateStatement;
    bool                        m_bInsertStatementWithFID;
    sqlite3_stmt*               m_poInsertStatement;
    bool                        m_bDeferredSpatialIndexCreation;
    // m_bHasSpatialIndex cannot be bool.  -1 is unset.
    int                         m_bHasSpatialIndex;
    bool                        m_bDropRTreeTable;
    bool                        m_abHasGeometryExtension[wkbTriangle+1];
    bool                        m_bPreservePrecision;
    bool                        m_bTruncateFields;
    bool                        m_bDeferredCreation;
    int                         m_iFIDAsRegularColumnIndex;

    CPLString                   m_osIdentifierLCO;
    CPLString                   m_osDescriptionLCO;
    bool                        m_bHasReadMetadataFromStorage;
    bool                        m_bHasTriedDetectingFID64;
    GPKGASpatialVariant         m_eASpatialVariant;
    std::set<OGRwkbGeometryType> m_eSetBadGeomTypeWarned;

    int                         m_nCountInsertInTransactionThreshold = -1;
    GIntBig                     m_nCountInsertInTransaction = 0;
    std::vector<CPLString >     m_aoRTreeTriggersSQL{};
    typedef struct
    {
        GIntBig nId;
        float   fMinX;
        float   fMinY;
        float   fMaxX;
        float   fMaxY;
    } GPKGRTreeEntry;
    std::vector<GPKGRTreeEntry>  m_aoRTreeEntries{};


    virtual OGRErr      ResetStatement() override;

    void                BuildWhere();
    OGRErr              RegisterGeometryColumn();

    CPLString           GetColumnsOfCreateTable(const std::vector<OGRFieldDefn*>& apoFields);
    CPLString           BuildSelectFieldList(const std::vector<OGRFieldDefn*>& apoFields);
    OGRErr              RecreateTable(const CPLString& osColumnsForCreate,
                                      const CPLString& osFieldListForSelect);
#ifdef ENABLE_GPKG_OGR_CONTENTS
    void                CreateTriggers(const char* pszTableName = nullptr);
    void                DisableTriggers(bool bNullifyFeatureCount = true);
#endif

    void                CheckGeometryType( OGRFeature *poFeature );

    OGRErr              ReadTableDefinition();
    void                InitView();

    bool                DoSpecialProcessingForColumnCreation(OGRFieldDefn* poField);

    bool                StartDeferredSpatialIndexUpdate();
    bool                FlushPendingSpatialIndexUpdate();

    public:
                        OGRGeoPackageTableLayer( GDALGeoPackageDataset *poDS,
                                                 const char * pszTableName );
                        virtual ~OGRGeoPackageTableLayer();

    /************************************************************************/
    /* OGR API methods */

    const char*         GetName() override { return GetDescription(); }
    const char*         GetFIDColumn() override;
    OGRwkbGeometryType  GetGeomType() override;
    const char*         GetGeometryColumn() override;
    OGRFeatureDefn*     GetLayerDefn() override;
    int                 TestCapability( const char * ) override;
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE ) override;
    OGRErr              CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                         int bApproxOK = TRUE ) override;
    virtual OGRErr      DeleteField(  int iFieldToDelete ) override;
    virtual OGRErr      AlterFieldDefn( int iFieldToAlter, OGRFieldDefn* poNewFieldDefn, int nFlagsIn ) override;
    virtual OGRErr      ReorderFields( int* panMap ) override;
    void                ResetReading() override;
    OGRErr              ICreateFeature( OGRFeature *poFeater ) override;
    OGRErr              ISetFeature( OGRFeature *poFeature ) override;
    OGRErr              DeleteFeature(GIntBig nFID) override;
    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRGeoPackageLayer::SetSpatialFilter(iGeomField, poGeom); }

    OGRErr              SetAttributeFilter( const char *pszQuery ) override;
    OGRErr              SyncToDisk() override;
    OGRFeature*         GetNextFeature() override;
    OGRFeature*         GetFeature(GIntBig nFID) override;
    OGRErr              StartTransaction() override;
    OGRErr              CommitTransaction() override;
    OGRErr              RollbackTransaction() override;
    GIntBig             GetFeatureCount( int ) override;
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRGeoPackageLayer::GetExtent(iGeomField, psExtent, bForce); }

    void                RecomputeExtent();

    void                SetOpeningParameters(bool bIsInGpkgContents,
                                             bool bIsSpatial,
                                             const char* pszGeomColName,
                                             const char* pszGeomType,
                                             bool bHasZ,
                                             bool bHasM);
    void                SetCreationParameters( OGRwkbGeometryType eGType,
                                               const char* pszGeomColumnName,
                                               int bGeomNullable,
                                               OGRSpatialReference* poSRS,
                                               const char* pszFIDColumnName,
                                               const char* pszIdentifier,
                                               const char* pszDescription );
    void                SetDeferredSpatialIndexCreation( bool bFlag )
                                { m_bDeferredSpatialIndexCreation = bFlag; }
    void                SetASpatialVariant( GPKGASpatialVariant eASpatialVariant )
                                { m_eASpatialVariant = eASpatialVariant; }

    void                CreateSpatialIndexIfNecessary();
    bool                CreateSpatialIndex(const char* pszTableName = nullptr);
    bool                DropSpatialIndex(bool bCalledFromSQLFunction = false);
    CPLString           ReturnSQLCreateSpatialIndexTriggers(const char* pszTableName);
    CPLString           ReturnSQLDropSpatialIndexTriggers();

    virtual char **     GetMetadata( const char *pszDomain = nullptr ) override;
    virtual const char *GetMetadataItem( const char * pszName,
                                             const char * pszDomain = "" ) override;
    virtual char **     GetMetadataDomainList() override;

    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                        const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain = "" ) override;

    void                RenameTo(const char* pszDstTableName);

    virtual int          HasFastSpatialFilter(int iGeomCol) override;
    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom) override;

    bool                HasSpatialIndex();
    void                SetPrecisionFlag( int bFlag )
                                { m_bPreservePrecision = CPL_TO_BOOL( bFlag ); }
    void                SetTruncateFieldsFlag( int bFlag )
                                { m_bTruncateFields = CPL_TO_BOOL( bFlag ); }
    OGRErr              RunDeferredCreationIfNecessary();
    bool                RunDeferredDropRTreeTableIfNecessary();
    bool                DoJobAtTransactionCommit();
    bool                DoJobAtTransactionRollback();
    bool                RunDeferredSpatialIndexUpdate();

#ifdef ENABLE_GPKG_OGR_CONTENTS
    bool                GetAddOGRFeatureCountTriggers() const
                                    { return m_bAddOGRFeatureCountTriggers; }
    void                SetAddOGRFeatureCountTriggers(bool b)
                                    { m_bAddOGRFeatureCountTriggers = b; }
    bool                GetOGRFeatureCountTriggersDeletedInTransaction() const
                        { return m_bFeatureCountTriggersDeletedInTransaction; }
    void                SetOGRFeatureCountTriggersEnabled(bool b)
                                    { m_bOGRFeatureCountTriggersEnabled = b; }

    void                DisableFeatureCount();
#endif

    /************************************************************************/
    /* GPKG methods */

  private:
    bool                CheckUpdatableTable(const char* pszOperation);
    OGRErr              UpdateExtent( const OGREnvelope *poExtent );
    OGRErr              SaveExtent();
    OGRErr              SaveTimestamp();
    OGRErr              BuildColumns();
    bool                IsGeomFieldSet( OGRFeature *poFeature );
    CPLString           FeatureGenerateUpdateSQL( OGRFeature *poFeature );
    CPLString           FeatureGenerateInsertSQL( OGRFeature *poFeature, bool bAddFID, bool bBindUnsetFields );
    OGRErr              FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, bool bAddFID, bool bBindUnsetFields );
    OGRErr              FeatureBindParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount, bool bAddFID, bool bBindUnsetFields );
    void                UpdateContentsToNullExtent();

    void                CheckUnknownExtensions();
    bool                CreateGeometryExtensionIfNecessary(const OGRGeometry* poGeom);
    bool                CreateGeometryExtensionIfNecessary(OGRwkbGeometryType eGType);
};

/************************************************************************/
/*                         OGRGeoPackageSelectLayer                     */
/************************************************************************/

class OGRGeoPackageSelectLayer final : public OGRGeoPackageLayer, public IOGRSQLiteSelectLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRGeoPackageSelectLayer)

    OGRSQLiteSelectLayerCommonBehaviour* poBehavior;

    virtual OGRErr      ResetStatement() override;

  public:
                        OGRGeoPackageSelectLayer( GDALGeoPackageDataset *,
                                              CPLString osSQL,
                                              sqlite3_stmt *,
                                              int bUseStatementForGetNextFeature,
                                              int bEmptyLayer );
                       virtual ~OGRGeoPackageSelectLayer();

    virtual void        ResetReading() override;

    virtual OGRFeature *GetNextFeature() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual void        SetSpatialFilter( OGRGeometry * poGeom ) override { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;
    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual OGRFeatureDefn *     GetLayerDefn() override { return OGRGeoPackageLayer::GetLayerDefn(); }
    virtual char*&               GetAttrQueryString() override { return m_pszAttrQueryString; }
    virtual OGRFeatureQuery*&    GetFeatureQuery() override { return m_poAttrQuery; }
    virtual OGRGeometry*&        GetFilterGeom() override { return m_poFilterGeom; }
    virtual int&                 GetIGeomFieldFilter() override { return m_iGeomFieldFilter; }
    virtual OGRSpatialReference* GetSpatialRef() override { return OGRGeoPackageLayer::GetSpatialRef(); }
    virtual int                  InstallFilter( OGRGeometry * poGeomIn ) override { return OGRGeoPackageLayer::InstallFilter(poGeomIn); }
    virtual int                  HasReadFeature() override { return iNextShapeId > 0; }
    virtual void                 BaseResetReading() override { OGRGeoPackageLayer::ResetReading(); }
    virtual OGRFeature          *BaseGetNextFeature() override { return OGRGeoPackageLayer::GetNextFeature(); }
    virtual OGRErr               BaseSetAttributeFilter(const char* pszQuery) override { return OGRGeoPackageLayer::SetAttributeFilter(pszQuery); }
    virtual GIntBig              BaseGetFeatureCount(int bForce) override { return OGRGeoPackageLayer::GetFeatureCount(bForce); }
    virtual int                  BaseTestCapability( const char *pszCap ) override { return OGRGeoPackageLayer::TestCapability(pszCap); }
    virtual OGRErr               BaseGetExtent(OGREnvelope *psExtent, int bForce) override { return OGRGeoPackageLayer::GetExtent(psExtent, bForce); }
    virtual OGRErr               BaseGetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override { return OGRGeoPackageLayer::GetExtent(iGeomField, psExtent, bForce); }
};

#endif /* OGR_GEOPACKAGE_H_INCLUDED */
