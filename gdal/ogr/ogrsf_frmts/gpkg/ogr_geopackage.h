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

#ifndef _OGR_GEOPACKAGE_H_INCLUDED
#define _OGR_GEOPACKAGE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_sqlite.h"
#include "ogrgeopackageutility.h"

#define UNKNOWN_SRID   -2
#define DEFAULT_SRID    0

/************************************************************************/
/*                          GDALGeoPackageDataset                       */
/************************************************************************/

class OGRGeoPackageTableLayer;

typedef struct
{
    int     nRow;
    int     nCol;
    int     nIdxWithinTileData;
    int     abBandDirty[4];
} CachedTileDesc;

typedef enum
{
    GPKG_TF_PNG_JPEG,
    GPKG_TF_PNG,
    GPKG_TF_PNG8,
    GPKG_TF_JPEG,
    GPKG_TF_WEBP
} GPKGTileFormat;

class GDALGeoPackageDataset : public OGRSQLiteBaseDataSource
{
    friend class GDALGeoPackageRasterBand;

    OGRGeoPackageTableLayer** m_papoLayers;
    int                 m_nLayers;
    int                 m_bUtf8;
    void                CheckUnknownExtensions(int bCheckRasterTable = FALSE);
    
    int                 m_bNew;

    CPLString           m_osRasterTable;
    CPLString           m_osIdentifier;
    int                 m_bIdentifierAsCO;
    CPLString           m_osDescription;
    int                 m_bDescriptionAsCO;
    int                 m_bHasReadMetadataFromStorage;
    int                 m_bMetadataDirty;
    char              **m_papszSubDatasets;
    char               *m_pszProjection;
    int                 m_bRecordInsertedInGPKGContent;
    int                 m_bGeoTransformValid;
    double              m_adfGeoTransform[6];
    int                 m_nSRID;
    double              m_dfTMSMinX;
    double              m_dfTMSMaxY;
    int                 m_nZoomLevel;
    GByte              *m_pabyCachedTiles;
    CachedTileDesc      m_asCachedTilesDesc[4];
    int                 m_nShiftXTiles;
    int                 m_nShiftXPixelsMod;
    int                 m_nShiftYTiles;
    int                 m_nShiftYPixelsMod;
    int                 m_nTileMatrixWidth;
    int                 m_nTileMatrixHeight;

    GPKGTileFormat      m_eTF;
    int                 m_nZLevel;
    int                 m_nQuality;
    int                 m_bDither;

    GDALColorTable*     m_poCT;
    int                 m_bTriedEstablishingCT;
    GByte*              m_pabyHugeColorArray;

    GDALGeoPackageDataset* m_poParentDS;
    int                 m_nOverviewCount;
    GDALGeoPackageDataset** m_papoOverviewDS;
    int                 m_bZoomOther;

    CPLString           m_osWHERE;

    sqlite3            *m_hTempDB;
    CPLString           m_osTempDBFilename;
    
    int                 m_bInFlushCache;
    
    int                 m_nTileInsertionCount;

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

        CPLErr                  ReadTile(const CPLString& osMemFileName,
                                         GByte* pabyTileData,
                                         int* pbIsLossyFormat = NULL);
        GByte*                  ReadTile(int nRow, int nCol);
        GByte*                  ReadTile(int nRow, int nCol, GByte* pabyData,
                                         int* pbIsLossyFormat = NULL);

        int                     m_bInWriteTile;
        CPLErr                  WriteTile();

        CPLErr                  WriteTileInternal(); /* should only be called by WriteTile() */
        CPLErr                  FlushRemainingShiftedTiles();
        CPLErr                  WriteShiftedTile(int nRow, int nCol, int iBand,
                                                 int nDstXOffset, int nDstYOffset,
                                                 int nDstXSize, int nDstYSize);

        int                     RegisterWebPExtension();
        int                     RegisterZoomOtherExtension();
        void                    ParseCompressionOptions(char** papszOptions);

        int                     HasMetadataTables();
        int                     CreateMetadataTables();
        const char*             CheckMetadataDomain( const char* pszDomain );
        void                    WriteMetadata(CPLXMLNode* psXMLNode, /* will be destroyed by the method /*/
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
        CPLErr              FlushCacheWithErrCode();
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
        int                 DeleteLayer( int iLayer );
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

        int                 GetSrsId( const OGRSpatialReference * poSRS );
        const char*         GetSrsName( const OGRSpatialReference * poSRS );
        OGRSpatialReference* GetSpatialRef( int iSrsId );
        virtual int         GetUTF8() { return m_bUtf8; }
        OGRErr              AddColumn( const char * pszTableName, 
                                       const char * pszColumnName, 
                                       const char * pszColumnType );
        OGRErr              CreateExtensionsTableIfNecessary();
        int                 HasExtensionsTable();
        OGRErr              CreateGDALAspatialExtension();

        static GDALDataset* CreateCopy( const char *pszFilename,
                                                   GDALDataset *poSrcDS, 
                                                   int bStrict,
                                                   char ** papszOptions,
                                                   GDALProgressFunc pfnProgress, 
                                                   void * pProgressData );
    private:
    
        OGRErr              PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected);
        OGRErr              SetApplicationId();
        int                 OpenOrCreateDB(int flags);
        int                 HasGDALAspatialExtension();
};

/************************************************************************/
/*                        GDALGeoPackageRasterBand                      */
/************************************************************************/

class GDALGeoPackageRasterBand: public GDALPamRasterBand
{
    public:

                                GDALGeoPackageRasterBand(GDALGeoPackageDataset* poDS,
                                                         int nBand,
                                                         int nTileWidth, int nTileHeight);
        
        virtual CPLErr          IReadBlock(int nBlockXOff, int nBlockYOff,
                                           void* pData);
        virtual CPLErr          IWriteBlock(int nBlockXOff, int nBlockYOff,
                                           void* pData);
        virtual CPLErr          FlushCache();

        virtual GDALColorTable* GetColorTable();
        virtual CPLErr          SetColorTable(GDALColorTable* poCT);

        virtual GDALColorInterp GetColorInterpretation();
        virtual CPLErr          SetColorInterpretation( GDALColorInterp );

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

    virtual int          HasFastSpatialFilter(CPL_UNUSED int iGeomCol) { return FALSE; }
    virtual CPLString    GetSpatialWhere(CPL_UNUSED int iGeomCol,
                                         CPL_UNUSED OGRGeometry* poFilterGeom) { return ""; }

};

/************************************************************************/
/*                        OGRGeoPackageTableLayer                       */
/************************************************************************/

class OGRGeoPackageTableLayer : public OGRGeoPackageLayer
{
    char*                       m_pszTableName;
    int                         m_iSrs;
    OGREnvelope*                m_poExtent;
    CPLString                   m_soColumns;
    CPLString                   m_soFilter;
    CPLString                   osQuery;
    OGRBoolean                  m_bExtentChanged;
    sqlite3_stmt*               m_poUpdateStatement;
    int                         m_bInsertStatementWithFID;
    sqlite3_stmt*               m_poInsertStatement;
    int                         bDeferedSpatialIndexCreation;
    int                         m_bHasSpatialIndex;
    int                         bDropRTreeTable;
    int                         m_anHasGeometryExtension[wkbMultiSurface+1];
    int                         m_bPreservePrecision;
    int                         m_bTruncateFields;
    int                         m_bDeferredCreation;

    virtual OGRErr      ResetStatement();
    
    void                BuildWhere(void);
    
    public:
    
                        OGRGeoPackageTableLayer( GDALGeoPackageDataset *poDS,
                                            const char * pszTableName );
                        ~OGRGeoPackageTableLayer();

    /************************************************************************/
    /* OGR API methods */
                        
    int                 TestCapability( const char * );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );
    void                ResetReading();
	OGRErr              ICreateFeature( OGRFeature *poFeater );
    OGRErr              ISetFeature( OGRFeature *poFeature );
    OGRErr              DeleteFeature(GIntBig nFID);
    virtual void        SetSpatialFilter( OGRGeometry * );
    OGRErr              SetAttributeFilter( const char *pszQuery );
    OGRErr              SyncToDisk();
    OGRFeature*         GetNextFeature();
    OGRFeature*         GetFeature(GIntBig nFID);
    OGRErr              StartTransaction();
    OGRErr              CommitTransaction();
    OGRErr              RollbackTransaction();
    GIntBig             GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    
    // void                SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn );

    OGRErr              ReadTableDefinition(int bIsSpatial);
    void                SetCreationParameters( OGRwkbGeometryType eGType,
                                               const char* pszGeomColumnName,
                                               OGRSpatialReference* poSRS,
                                               const char* pszFIDColumnName );
    void                SetDeferedSpatialIndexCreation( int bFlag )
                                { bDeferedSpatialIndexCreation = bFlag; }

    void                CreateSpatialIndexIfNecessary();
    int                 CreateSpatialIndex();
    int                 DropSpatialIndex(int bCalledFromSQLFunction = FALSE);

    void                RenameTo(const char* pszDstTableName);

    virtual int          HasFastSpatialFilter(int iGeomCol);
    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom);

    int                 HasSpatialIndex();
    void                SetPrecisionFlag( int bFlag )
                                { m_bPreservePrecision = bFlag; }
    void                SetTruncateFieldsFlag( int bFlag )
                                { m_bTruncateFields = bFlag; }
    OGRErr              RunDeferredCreationIfNecessary();

    /************************************************************************/
    /* GPKG methods */
    
    private:
    
    OGRErr              UpdateExtent( const OGREnvelope *poExtent );
    OGRErr              SaveExtent();
    OGRErr              BuildColumns();
    OGRBoolean          IsGeomFieldSet( OGRFeature *poFeature );
    CPLString           FeatureGenerateUpdateSQL( OGRFeature *poFeature );
    CPLString           FeatureGenerateInsertSQL( OGRFeature *poFeature, int bAddFID );
    OGRErr              FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int bAddFID );
    OGRErr              FeatureBindParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount, int bAddFID );

    void                CheckUnknownExtensions();
    int                 CreateGeometryExtensionIfNecessary(OGRwkbGeometryType eGType);
};

/************************************************************************/
/*                         OGRGeoPackageSelectLayer                     */
/************************************************************************/

class OGRGeoPackageSelectLayer : public OGRGeoPackageLayer, public IOGRSQLiteSelectLayer
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


#endif /* _OGR_GEOPACKAGE_H_INCLUDED */
