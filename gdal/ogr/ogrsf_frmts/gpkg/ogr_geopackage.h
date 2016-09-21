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
#include "ogrgeopackageutility.h"
#include "gpkgmbtilescommon.h"

#define UNKNOWN_SRID   -2
#define DEFAULT_SRID    0

/************************************************************************/
/*                          GDALGeoPackageDataset                       */
/************************************************************************/

class OGRGeoPackageTableLayer;

class GDALGeoPackageDataset CPL_FINAL : public OGRSQLiteBaseDataSource, public GDALGPKGMBTilesLikePseudoDataset
{
    friend class GDALGeoPackageRasterBand;
    friend class OGRGeoPackageTableLayer;

    OGRGeoPackageTableLayer** m_papoLayers;
    int                 m_nLayers;
    bool                m_bUtf8;
    void                CheckUnknownExtensions(bool bCheckRasterTable = false);

    CPLString           m_osIdentifier;
    bool                m_bIdentifierAsCO;
    CPLString           m_osDescription;
    bool                m_bDescriptionAsCO;
    bool                m_bHasReadMetadataFromStorage;
    bool                m_bMetadataDirty;
    char              **m_papszSubDatasets;
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

    CPLString           m_osTilingScheme;

        void            ComputeTileAndPixelShifts();
        int             InitRaster ( GDALGeoPackageDataset* poParentDS,
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
        int             InitRaster ( GDALGeoPackageDataset* poParentDS,
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

        int     OpenRaster( const char* pszTableName,
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
                            char** papszOptions );
        CPLErr   FinalizeRasterRegistration();

        int                     RegisterWebPExtension();
        int                     RegisterZoomOtherExtension();
        void                    ParseCompressionOptions(char** papszOptions);

        int                     HasMetadataTables();
        int                     CreateMetadataTables();
        const char*             CheckMetadataDomain( const char* pszDomain );
        void                    WriteMetadata(CPLXMLNode* psXMLNode, /* will be destroyed by the method */
                                              const char* pszTableName);
        CPLErr                  FlushMetadata();

    public:
                            GDALGeoPackageDataset();
                            ~GDALGeoPackageDataset();

        virtual char **     GetMetadata( const char *pszDomain = NULL );
        virtual const char *GetMetadataItem( const char * pszName,
                                             const char * pszDomain = "" );
        virtual char **     GetMetadataDomainList();
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                         const char * pszDomain = "" );
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                             const char * pszValue,
                                             const char * pszDomain = "" );

        virtual const char* GetProjectionRef();
        virtual CPLErr      SetProjection( const char* pszProjection );

        virtual CPLErr      GetGeoTransform( double* padfGeoTransform );
        virtual CPLErr      SetGeoTransform( double* padfGeoTransform );

        virtual void        FlushCache();
        virtual CPLErr      IBuildOverviews( const char *, int, int *,
                                             int, int *, GDALProgressFunc, void * );

        virtual int         GetLayerCount() { return m_nLayers; }
        int                 Open( GDALOpenInfo* poOpenInfo );
        int                 Create( const char * pszFilename,
                                    int nXSize,
                                    int nYSize,
                                    int nBands,
                                    GDALDataType eDT,
                                    char **papszOptions );
        OGRLayer*           GetLayer( int iLayer );
        OGRErr              DeleteLayer( int iLayer );
        OGRLayer*           ICreateLayer( const char * pszLayerName,
                                         OGRSpatialReference * poSpatialRef,
                                         OGRwkbGeometryType eGType,
                                         char **papszOptions );
        int                 TestCapability( const char * );

        virtual std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetLayerWithGetSpatialWhereByName( const char* pszName );

        virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect );
        virtual void        ReleaseResultSet( OGRLayer * poLayer );

        virtual OGRErr      CommitTransaction();
        virtual OGRErr      RollbackTransaction();

        int                 GetSrsId( const OGRSpatialReference * poSRS );
        const char*         GetSrsName( const OGRSpatialReference * poSRS );
        OGRSpatialReference* GetSpatialRef( int iSrsId );
        bool                GetUTF8() { return m_bUtf8; }
        OGRErr              CreateExtensionsTableIfNecessary();
        bool                HasExtensionsTable();
        OGRErr              CreateGDALAspatialExtension();
        void                SetMetadataDirty() { m_bMetadataDirty = true; }

        const char*         GetGeometryTypeString(OGRwkbGeometryType eType);

        static GDALDataset* CreateCopy( const char *pszFilename,
                                                   GDALDataset *poSrcDS,
                                                   int bStrict,
                                                   char ** papszOptions,
                                                   GDALProgressFunc pfnProgress,
                                                   void * pProgressData );

    protected:
        // Coming from GDALGPKGMBTilesLikePseudoDataset

        virtual CPLErr                  IFlushCacheWithErrCode();
        virtual int                     IGetRasterCount() { return nBands; }
        virtual GDALRasterBand*         IGetRasterBand(int nBand) { return GetRasterBand(nBand); }
        virtual sqlite3                *IGetDB() { return GetDB(); }
        virtual bool                    IGetUpdate() { return bUpdate != FALSE; }
        virtual bool                    ICanIWriteBlock();
        virtual void                    IStartTransaction() { SoftStartTransaction(); }
        virtual void                    ICommitTransaction() { SoftCommitTransaction(); }
        virtual const char             *IGetFilename() { return m_pszFilename; }
        virtual int                     GetRowFromIntoTopConvention(int nRow) { return nRow; }

    private:

        OGRErr              PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected);
        OGRErr              SetApplicationId();
        int                 OpenOrCreateDB(int flags);
        bool                HasGDALAspatialExtension();
};

/************************************************************************/
/*                        GDALGeoPackageRasterBand                      */
/************************************************************************/

class GDALGeoPackageRasterBand CPL_FINAL: public GDALGPKGMBTilesLikeRasterBand
{
    public:
                                GDALGeoPackageRasterBand(GDALGeoPackageDataset* poDS,
                                                         int nTileWidth, int nTileHeight);

        virtual int             GetOverviewCount();
        virtual GDALRasterBand* GetOverview(int nIdx);
};

/************************************************************************/
/*                           OGRGeoPackageLayer                         */
/************************************************************************/

class OGRGeoPackageLayer : public OGRLayer, public IOGRSQLiteGetSpatialWhere
{
  protected:
    GDALGeoPackageDataset *m_poDS;

    OGRFeatureDefn*      m_poFeatureDefn;
    int                  iNextShapeId;

    sqlite3_stmt        *m_poQueryStatement;
    int                  bDoStep;

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

                        OGRGeoPackageLayer(GDALGeoPackageDataset* poDS);
                        ~OGRGeoPackageLayer();
    /************************************************************************/
    /* OGR API methods */

    OGRFeature*         GetNextFeature();
    const char*         GetFIDColumn();
    void                ResetReading();
    int                 TestCapability( const char * );
    OGRFeatureDefn*     GetLayerDefn() { return m_poFeatureDefn; }

    virtual int          HasFastSpatialFilter(int /*iGeomCol*/) { return FALSE; }
    virtual CPLString    GetSpatialWhere(int /*iGeomCol*/,
                                         OGRGeometry* /*poFilterGeom*/) { return ""; }

};

/************************************************************************/
/*                        OGRGeoPackageTableLayer                       */
/************************************************************************/

class OGRGeoPackageTableLayer CPL_FINAL : public OGRGeoPackageLayer
{
    char*                       m_pszTableName;
    int                         m_iSrs;
    OGREnvelope*                m_poExtent;
    CPLString                   m_soColumns;
    CPLString                   m_soFilter;
    CPLString                   osQuery;
    bool                        m_bExtentChanged;
    sqlite3_stmt*               m_poUpdateStatement;
    bool                        m_bInsertStatementWithFID;
    sqlite3_stmt*               m_poInsertStatement;
    bool                        m_bDeferredSpatialIndexCreation;
    // m_bHasSpatialIndex cannot be bool.  -1 is unset.
    int                         m_bHasSpatialIndex;
    bool                        m_bDropRTreeTable;
    bool                        m_abHasGeometryExtension[wkbTIN+1];
    bool                        m_bPreservePrecision;
    bool                        m_bTruncateFields;
    bool                        m_bDeferredCreation;
    int                         m_iFIDAsRegularColumnIndex;

    CPLString                   m_osIdentifierLCO;
    CPLString                   m_osDescriptionLCO;
    bool                        m_bHasReadMetadataFromStorage;

    virtual OGRErr      ResetStatement();

    void                BuildWhere(void);
    OGRErr              RegisterGeometryColumn();

    public:
                        OGRGeoPackageTableLayer( GDALGeoPackageDataset *poDS,
                                            const char * pszTableName );
                        ~OGRGeoPackageTableLayer();

    /************************************************************************/
    /* OGR API methods */

    int                 TestCapability( const char * );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );
    OGRErr              CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                         int bApproxOK = TRUE );
    void                ResetReading();
	OGRErr              ICreateFeature( OGRFeature *poFeater );
    OGRErr              ISetFeature( OGRFeature *poFeature );
    OGRErr              DeleteFeature(GIntBig nFID);
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
                { OGRGeoPackageLayer::SetSpatialFilter(iGeomField, poGeom); }

    OGRErr              SetAttributeFilter( const char *pszQuery );
    OGRErr              SyncToDisk();
    OGRFeature*         GetNextFeature();
    OGRFeature*         GetFeature(GIntBig nFID);
    OGRErr              StartTransaction();
    OGRErr              CommitTransaction();
    OGRErr              RollbackTransaction();
    GIntBig             GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
                { return OGRGeoPackageLayer::GetExtent(iGeomField, psExtent, bForce); }

    OGRErr              ReadTableDefinition(int bIsSpatial);
    void                SetCreationParameters( OGRwkbGeometryType eGType,
                                               const char* pszGeomColumnName,
                                               int bGeomNullable,
                                               OGRSpatialReference* poSRS,
                                               const char* pszFIDColumnName,
                                               const char* pszIdentifier,
                                               const char* pszDescription );
    void                SetDeferredSpatialIndexCreation( bool bFlag )
                                { m_bDeferredSpatialIndexCreation = bFlag; }

    void                CreateSpatialIndexIfNecessary();
    bool                CreateSpatialIndex();
    bool                DropSpatialIndex(bool bCalledFromSQLFunction = false);

    virtual char **     GetMetadata( const char *pszDomain = NULL );
    virtual const char *GetMetadataItem( const char * pszName,
                                             const char * pszDomain = "" );
    virtual char **     GetMetadataDomainList();

    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                        const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain = "" );

    void                RenameTo(const char* pszDstTableName);

    virtual int          HasFastSpatialFilter(int iGeomCol);
    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom);

    bool                HasSpatialIndex();
    void                SetPrecisionFlag( int bFlag )
                                { m_bPreservePrecision = CPL_TO_BOOL( bFlag ); }
    void                SetTruncateFieldsFlag( int bFlag )
                                { m_bTruncateFields = CPL_TO_BOOL( bFlag ); }
    OGRErr              RunDeferredCreationIfNecessary();

    /************************************************************************/
    /* GPKG methods */

  private:
    OGRErr              UpdateExtent( const OGREnvelope *poExtent );
    OGRErr              SaveExtent();
    OGRErr              BuildColumns();
    OGRBoolean          IsGeomFieldSet( OGRFeature *poFeature );
    CPLString           FeatureGenerateUpdateSQL( OGRFeature *poFeature );
    CPLString           FeatureGenerateInsertSQL( OGRFeature *poFeature, bool bAddFID, bool bBindNullFields );
    OGRErr              FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, bool bAddFID, bool bBindNullFields );
    OGRErr              FeatureBindParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount, bool bAddFID, bool bBindNullFields );

    void                CheckUnknownExtensions();
    bool                CreateGeometryExtensionIfNecessary(OGRwkbGeometryType eGType);
};

/************************************************************************/
/*                         OGRGeoPackageSelectLayer                     */
/************************************************************************/

class OGRGeoPackageSelectLayer CPL_FINAL : public OGRGeoPackageLayer, public IOGRSQLiteSelectLayer
{
    OGRSQLiteSelectLayerCommonBehaviour* poBehaviour;

    virtual OGRErr      ResetStatement();

  public:
                        OGRGeoPackageSelectLayer( GDALGeoPackageDataset *,
                                              CPLString osSQL,
                                              sqlite3_stmt *,
                                              int bUseStatementForGetNextFeature,
                                              int bEmptyLayer );
                       ~OGRGeoPackageSelectLayer();

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();
    virtual GIntBig     GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);

    virtual OGRFeatureDefn *     GetLayerDefn() { return OGRGeoPackageLayer::GetLayerDefn(); }
    virtual char*&               GetAttrQueryString() { return m_pszAttrQueryString; }
    virtual OGRFeatureQuery*&    GetFeatureQuery() { return m_poAttrQuery; }
    virtual OGRGeometry*&        GetFilterGeom() { return m_poFilterGeom; }
    virtual int&                 GetIGeomFieldFilter() { return m_iGeomFieldFilter; }
    virtual OGRSpatialReference* GetSpatialRef() { return OGRGeoPackageLayer::GetSpatialRef(); }
    virtual int                  InstallFilter( OGRGeometry * poGeomIn ) { return OGRGeoPackageLayer::InstallFilter(poGeomIn); }
    virtual int                  HasReadFeature() { return iNextShapeId > 0; }
    virtual void                 BaseResetReading() { OGRGeoPackageLayer::ResetReading(); }
    virtual OGRFeature          *BaseGetNextFeature() { return OGRGeoPackageLayer::GetNextFeature(); }
    virtual OGRErr               BaseSetAttributeFilter(const char* pszQuery) { return OGRGeoPackageLayer::SetAttributeFilter(pszQuery); }
    virtual GIntBig              BaseGetFeatureCount(int bForce) { return OGRGeoPackageLayer::GetFeatureCount(bForce); }
    virtual int                  BaseTestCapability( const char *pszCap ) { return OGRGeoPackageLayer::TestCapability(pszCap); }
    virtual OGRErr               BaseGetExtent(OGREnvelope *psExtent, int bForce) { return OGRGeoPackageLayer::GetExtent(psExtent, bForce); }
    virtual OGRErr               BaseGetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) { return OGRGeoPackageLayer::GetExtent(iGeomField, psExtent, bForce); }
};


#endif /* OGR_GEOPACKAGE_H_INCLUDED */
