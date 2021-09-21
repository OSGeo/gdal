/*****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Definition of classes for OGR DB2 Spatial driver.
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 *****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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

#ifndef OGR_DB2_H_INCLUDED
#define OGR_DB2_H_INCLUDED
#ifdef DB2_CLI
#include <sqlcli1.h>   // needed for CLI support
#endif
#include "ogrsf_frmts.h"
#include "cpl_port.h"
#include "cpl_error.h"
#include "gdal_pam.h"
#include "gdalwarper.h"
#include <time.h>

#ifdef WIN32
#  include <windows.h>
#endif

#ifndef DB2_CLI
#include <sql.h>
#include <sqlext.h>
#endif
#include <odbcinst.h>
#include "cpl_string.h"

// Map ODBC handles to DB2 CLI handles
#ifndef HDBC
#define HDBC SQLHDBC
#define HSTMT SQLHSTMT
#define HENV SQLHENV
#endif

#ifdef PATH_MAX
#  define ODBC_FILENAME_MAX PATH_MAX
#else
#  define ODBC_FILENAME_MAX (255 + 1) /* Max path length */
#endif

class OGRDB2DataSource;

#ifdef DEBUG_SQL
#define DB2_DEBUG_SQL(ogrdb2fn,ogrdb2stmt) \
              CPLDebug(ogrdb2fn, "stmt: '%s'", ogrdb2stmt.GetCommand());
#else
#define DB2_DEBUG_SQL(ogrdb2fn,ogrdb2stmt)
#endif

#ifdef DEBUG_DB2
#define DB2_DEBUG_ENTER(ogrdb2fn) \
            CPLDebug(ogrdb2fn,"Entering");

#define DB2_DEBUG_EXIT(ogrdb2fn) \
            CPLDebug(ogrdb2fn,"Exiting");

#else
#define DB2_DEBUG_ENTER(ogrdb2fn)
#define DB2_DEBUG_EXIT(ogrdb2fn)
#endif

#define DB2ODBC_PREFIX "DB2ODBC:"

#define UNKNOWN_SRID   -2
#define DEFAULT_SRID    0

// LATER - not sure what the maximum blob size should be
#define MAXBLOB 1000000

class GDALDB2RasterBand;
class OGRDB2TableLayer;

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

/* On MSVC SQLULEN is missing in some cases (i.e. VC6)
** but it is always a #define so test this way.   On Unix
** it is a typedef so we can't always do this.
*/
#if defined(_MSC_VER) && !defined(SQLULEN) && !defined(_WIN64)
#  define MISSING_SQLULEN
#endif

#if !defined(MISSING_SQLULEN)
/* ODBC types to support 64 bit compilation */
#  define CPL_SQLULEN SQLULEN
#  define CPL_SQLLEN  SQLLEN
#else
#  define CPL_SQLULEN SQLUINTEGER
#  define CPL_SQLLEN  SQLINTEGER
#endif  /* ifdef SQLULEN */

/**
 * A class representing an ODBC database session.
 *
 * Includes error collection services.
 *
 * Copied from cpl_odbc.h - to resolve issue with needing to include
 * different header files for ODBC and CLI.
 */

/************************************************************************/
/*                             OGRDB2Session                            */
/************************************************************************/

class OGRDB2Session
{
protected:
// From CPLODBCSession
    char      m_szLastError[SQL_MAX_MESSAGE_LENGTH + 1];
    HENV      m_hEnv = nullptr;
    HDBC      m_hDBC = nullptr;
    int       m_bInTransaction = FALSE;
    int       m_bAutoCommit = TRUE;

public:
    OGRDB2Session( );
    virtual ~OGRDB2Session();
// From CPLODBCSession
    int         EstablishSession( const char *pszDSN,
                                  const char *pszUserid,
                                  const char *pszPassword );
    const char  *GetLastError();

    // Transaction handling

    int         ClearTransaction();
    int         BeginTransaction();
    int         CommitTransaction();
    virtual int RollbackTransaction();
    int         IsInTransaction() { return m_bInTransaction; }

    // Essentially internal.

    int         CloseSession();

    int         Failed( int, HSTMT = nullptr );
    HDBC        GetConnection() { return m_hDBC; }
    HENV        GetEnvironment()  { return m_hEnv; }
};

/**
 * Abstraction for statement, and resultset.
 *
 * Includes methods for executing an SQL statement, and for accessing the
 * resultset from that statement.  Also provides for executing other ODBC
 * requests that produce results sets such as SQLColumns() and SQLTables()
 * requests.
 *
 * Copied from cpl_odbc.h - to resolve issue with needing to include
 * different header files for ODBC and CLI
 */

/************************************************************************/
/*                             OGRDB2Statement                          */
/************************************************************************/

class OGRDB2Statement
{
protected:
    int             m_nLastRetCode;
    int             m_bPrepared;
// From CPLODBCStatement
    OGRDB2Session     *m_poSession;
    HSTMT               m_hStmt;

    SQLSMALLINT    m_nColCount;
    char         **m_papszColNames;
    SQLSMALLINT   *m_panColType;
    char         **m_papszColTypeNames;
    CPL_SQLULEN      *m_panColSize;
    SQLSMALLINT   *m_panColPrecision;
    SQLSMALLINT   *m_panColNullable;
    char         **m_papszColColumnDef;

    char         **m_papszColValues;
    CPL_SQLLEN       *m_panColValueLengths;

    char          *m_pszStatement;
    size_t         m_nStatementMax;
    size_t         m_nStatementLen;
public:
    explicit OGRDB2Statement( OGRDB2Session * );
    OGRDB2Statement( );
    ~OGRDB2Statement();
    int             DB2Execute(const char *pszCallingFunction);
    int             DB2Prepare(const char *pszCallingFunction);
    int             GetLastRetCode() {
        return m_nLastRetCode;
    };
    int             DB2BindParameterIn(const char *pszCallingFunction,
                                            int nBindNum,
                                            int nValueType,
                                            int nParameterType,
                                            int nLen,
                                            void * pValuePointer);

// From CPLODBCStatement
    HSTMT          GetStatement() { return m_hStmt; }
    int            Failed( int );
    // Command buffer related.
    void           Clear();
    void           AppendEscaped( const char * );
    void           Append( const char * );
    void           Append( int );
    void           Append( double );
    int            Appendf( const char *, ... ) CPL_PRINT_FUNC_FORMAT (2, 3);
    const char    *GetCommand() { return m_pszStatement; }

    int            ExecuteSQL( const char * = nullptr );

    // Results fetching
    int            Fetch( int nOrientation = SQL_FETCH_NEXT,
                          int nOffset = 0 );
    void           ClearColumnData();

    int            GetColCount();
    const char    *GetColName( int );
    short          GetColType( int );
    const char    *GetColTypeName( int );
    short          GetColSize( int );
    short          GetColPrecision( int );
    short          GetColNullable( int );
    const char    *GetColColumnDef( int );

    int            GetColId( const char * );
    const char    *GetColData( int, const char * = nullptr );
    const char    *GetColData( const char *, const char * = nullptr );
    int            GetColDataLength( int );
    int            GetRowCountAffected();

    // Fetch special metadata.
    int            GetColumns( const char *pszTable,
                               const char *pszCatalog = nullptr,
                               const char *pszSchema = nullptr );
    int            GetPrimaryKeys( const char *pszTable,
                                   const char *pszCatalog = nullptr,
                                   const char *pszSchema = nullptr );

    int            GetTables( const char *pszCatalog = nullptr,
                              const char *pszSchema = nullptr );

    void           DumpResult( FILE *fp, int bShowSchema = FALSE );

    static CPLString GetTypeName( int );
    static SQLSMALLINT GetTypeMapping( SQLSMALLINT );

    int            CollectResultsInfo();
};

/************************************************************************/
/*                         OGRDB2AppendEscaped( )                       */
/************************************************************************/

void OGRDB2AppendEscaped( OGRDB2Statement* poStatement,
                          const char* pszStrValue);

/************************************************************************/
/*                             OGRDB2Layer                              */
/************************************************************************/

class OGRDB2Layer CPL_NON_FINAL: public OGRLayer
{
protected:
    OGRDB2DataSource *m_poDS; // GPKG - where set?
    OGRFeatureDefn     *poFeatureDefn;

    OGRDB2Statement   *m_poStmt;

    OGRDB2Statement   *m_poPrepStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    GIntBig            iNextShapeId;

    OGRDB2DataSource   *poDS;

    char               *pszGeomColumn;
    char               *pszFIDColumn;

    int                bIsIdentityFid;
    char               cGenerated;// 'A' always generated, 'D' default,' ' not
    int                nLayerStatus;
    int                *panFieldOrdinals;

    CPLErr             BuildFeatureDefn( const char *pszLayerName,
                                         OGRDB2Statement *poStmt );

    virtual OGRDB2Statement *  GetStatement() {
        return m_poStmt;
    }

public:
    OGRDB2Layer();
    virtual             ~OGRDB2Layer();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override {
        return poFeatureDefn;
    }

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual OGRErr     StartTransaction() override;
    virtual OGRErr     CommitTransaction() override;
    virtual OGRErr     RollbackTransaction() override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    virtual int         TestCapability( const char * ) override;
    char*               GByteArrayToHexString( const GByte* pabyData,
            int nLen);

    void               SetLayerStatus( int nStatus ) {
        nLayerStatus = nStatus;
    }
    int                GetLayerStatus() {
        return nLayerStatus;
    }
    int                GetSRSId() {
        return nSRSId;
    }
};

/************************************************************************/
/*                       OGRDB2TableLayer                               */
/************************************************************************/

class OGRDB2TableLayer final: public OGRDB2Layer
{
    int                 bUpdateAccess;
    int                 bLaunderColumnNames;
    int                 bPreservePrecision;
    int                 bNeedSpatialIndex;

    //int                 nUploadGeometryFormat;
    char                *m_pszQuery;

    void                ClearStatement();
    OGRDB2Statement* BuildStatement(const char* pszColumns);
    static void                FreeBindBuffer(int nBindNum, void **bind_buffer);
    CPLString BuildFields();

    virtual OGRDB2Statement *  GetStatement() override;

    char               *pszTableName;
    char               *m_pszLayerName;
    char               *pszSchemaName;

    OGRwkbGeometryType eGeomType;

// From GPKG
    //char*                       m_pszTableName;
    int                         m_iSrs;
    //OGREnvelope*                m_poExtent;
    CPLString                   m_soColumns;
    CPLString                   m_soFilter;
    CPLString                   osQuery;
    //OGRBoolean                  m_bExtentChanged;

    //int                         m_bInsertStatementWithFID;

    //int                         bDeferredSpatialIndexCreation;
    //int                         m_bHasSpatialIndex;
    //int                         bDropRTreeTable;
    //int                         m_anHasGeometryExtension[wkbMultiSurface+1];
    //int                         m_bPreservePrecision;
    //int                         m_bTruncateFields;
    //int                         m_bDeferredCreation;
    //int                         m_iFIDAsRegularColumnIndex;

    CPLString                   m_osIdentifierLCO;
    CPLString                   m_osDescriptionLCO;
    //int                         m_bHasReadMetadataFromStorage;
    OGRErr              RegisterGeometryColumn();
    void                BuildWhere();
//    OGRErr              SyncToDisk();

    OGRErr              BindFieldValue(OGRDB2Statement *poStatement,
                                       OGRFeature* poFeature, int i,
                                       int nBindNum, void **papBindBuffer);

public:
    explicit OGRDB2TableLayer( OGRDB2DataSource * );
    virtual ~OGRDB2TableLayer();

    CPLErr              Initialize( const char *pszSchema,
                                    const char *pszTableName,
                                    const char *pszGeomCol,
                                    int nCoordDimension,
                                    int nSRId,
                                    const char *pszSRText,
                                    OGRwkbGeometryType eType);
    static OGRErr              isFieldTypeSupported( OGRFieldType nFieldType );
    OGRErr              CreateSpatialIndex();
    void                DropSpatialIndex();

    virtual void        ResetReading() override;
    virtual GIntBig         GetFeatureCount( int ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual const char* GetName() override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      PrepareFeature( OGRFeature *poFeature, char cType );

    const char*         GetTableName() {
        return pszTableName;
    }
    const char*         GetLayerName() {
        return m_pszLayerName;
    }
    const char*         GetSchemaName() {
        return pszSchemaName;
    }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;

    void                SetLaunderFlag( int bFlag )
    {
        bLaunderColumnNames = bFlag;
    }
    void                SetPrecisionFlag( int bFlag )
    {
        bPreservePrecision = bFlag;
    }
    void                SetSpatialIndexFlag( int bFlag )
    {
        bNeedSpatialIndex = bFlag;
    }

    int                 FetchSRSId();
    //void                CreateSpatialIndexIfNecessary();

    int                 DropSpatialIndex(int bCalledFromSQLFunction = FALSE);
    /*
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

        int                 HasSpatialIndex();
        void                SetTruncateFieldsFlag( int bFlag )
        {
            m_bTruncateFields = bFlag;
        }

        OGRErr              ReadTableDefinition(int bIsSpatial);
        void                SetCreationParameters( OGRwkbGeometryType eGType,
                const char* pszGeomColumnName,
                int bGeomNullable,
                OGRSpatialReference* poSRS,
                const char* pszFIDColumnName,
                const char* pszIdentifier,
                const char* pszDescription );
    */
    // cppcheck-suppress functionStatic
    OGRErr              RunDeferredCreationIfNecessary();
    /************************************************************************/
    /* GPKG methods */

private:

    OGRErr              UpdateExtent( const OGREnvelope *poExtent );
    OGRErr              SaveExtent();
    OGRErr              BuildColumns();
    OGRBoolean          IsGeomFieldSet( OGRFeature *poFeature );
    void                CheckUnknownExtensions();
    int                 CreateGeometryExtensionIfNecessary(
                                OGRwkbGeometryType eGType);
};

/************************************************************************/
/*                      OGRDB2SelectLayer                      */
/************************************************************************/

class OGRDB2SelectLayer final: public OGRDB2Layer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual OGRDB2Statement *  GetStatement() override;

public:
    OGRDB2SelectLayer( OGRDB2DataSource *,
                       OGRDB2Statement * );
    virtual ~OGRDB2SelectLayer();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
     virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
            { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRDB2DataSource                           */
/************************************************************************/

class OGRDB2DataSource final: public GDALPamDataset
{
    friend class GDALDB2RasterBand;
    friend class OGRDB2TableLayer;

// Utility stuff
    double              clock1, clock2;
    time_t              time1, time2;
    double              dtime;
    double              dclock;
    char                stime[256];
    double              getDTime();

    char                **m_papszTableNames;
    char                **m_papszSchemaNames;
    char                **m_papszGeomColumnNames;
    char                **m_papszCoordDimensions;
    char                **m_papszSRIds;
    char                **m_papszSRTexts;
    char                *m_pszFilename;
    int                 m_bIsVector;

    OGRDB2TableLayer    **papoLayers;
    OGRDB2TableLayer    **m_papoLayers;    //DWA

    char               *m_pszName;

    char               *m_pszCatalog;
    int                 m_bIsZ;
    int                 bDSUpdate;
    OGRDB2Session       m_oSession;

    int                 bUseGeometryColumns;

    int                 bListAllTables;
    int                 m_bHasMetadataTables;
    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 m_nKnownSRID;
    int                *m_panSRID;
    OGRSpatialReference **m_papoSRS;
//***************** For raster support
    int                 m_bUpdate;
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

    OGRDB2DataSource* m_poParentDS;
    int                 m_nOverviewCount;
    OGRDB2DataSource** m_papoOverviewDS;
    int                 m_bZoomOther;

    CPLString           m_osWHERE;

    //int                 m_hTempDB;  //LATER - flag that partial_tiles exists
    CPLString           m_osTempDBFilename;

    int                 m_bInFlushCache;

    int                 m_nTileInsertionCount;

    CPLString           m_osTilingScheme;

    void            ComputeTileAndPixelShifts();
    int             InitRaster ( OGRDB2DataSource* poParentDS,
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
                                 OGRDB2Statement* oStmt,
                                 int nIdxInResult );
    int             InitRaster ( OGRDB2DataSource* poParentDS,
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
                                     int* pbIsLossyFormat = nullptr);
    GByte*                  ReadTile(int nRow, int nCol);
    GByte*                  ReadTile(int nRow, int nCol, GByte* pabyData,
                                     int* pbIsLossyFormat = nullptr);

    int                     m_bInWriteTile;
    CPLErr                  WriteTile();

    CPLErr                  WriteTileInternal();
    // cppcheck-suppress functionStatic
    CPLErr                  FlushRemainingShiftedTiles();
    // cppcheck-suppress functionStatic
    CPLErr                  WriteShiftedTile(int nRow, int nCol, int iBand,
            int nDstXOffset, int nDstYOffset,
            int nDstXSize, int nDstYSize);

    int                     RegisterWebPExtension();
    int                     RegisterZoomOtherExtension();
    void                    ParseCompressionOptions(char** papszOptions);

    int                     HasMetadataTables();
    int                     CreateMetadataTables();
    const char*             CheckMetadataDomain( const char* pszDomain );
    void                    WriteMetadata(CPLXMLNode* psXMLNode,
                                          const char* pszTableName);
    CPLErr                  FlushMetadata();

//***************** For raster support

public:
    OGRDB2DataSource();
    virtual ~OGRDB2DataSource();
//***************** For raster support

    virtual char **     GetMetadata( const char *pszDomain = "" ) override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;
    virtual char **     GetMetadataDomainList() override;
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" ) override;
    CPLErr              FlushCacheWithErrCode();

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

    virtual CPLErr      IBuildOverviews( const char *, int, int *,
                                         int, int *,
                                         GDALProgressFunc, void * ) override;

    OGRErr              CreateGDALAspatialExtension();
    void                SetMetadataDirty() {
        m_bMetadataDirty = TRUE;
    }
    /*
    */
    // cppcheck-suppress functionStatic
    OGRErr              CreateExtensionsTableIfNecessary();
    // cppcheck-suppress functionStatic
    int                 HasExtensionsTable();
    virtual void        FlushCache() override;
    static GDALDataset* CreateCopy( const char *pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict,
                                    char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );
//*****************

    int                 DeleteLayer( OGRDB2TableLayer * poLayer );
    const char          *GetCatalog() {
        return m_pszCatalog;
    }

    static int                 ParseValue(char** pszValue, char* pszSource,
                                   const char* pszKey,
                                   int nStart, int nNext, int nTerm,
                                   int bRemove);

    int                 Open(GDALOpenInfo* poOpenInfo);
    int                 Create( const char * pszFilename,
                                int nXSize,
                                int nYSize,
                                int nBands,
                                GDALDataType eDT,
                                char **papszOptions );
    int                 Open( const char *, int bTestOpen );
    int                 OpenTable( const char *pszSchemaName,
                                   const char *pszTableName,
                                   const char *pszGeomCol,
                                   int nCoordDimension,
                                   int nSRID, const char *pszSRText,
                                   OGRwkbGeometryType eType);

    const char          *GetName() {
        return m_pszName;
    }
    int                 GetLayerCount() override;
    OGRLayer            *GetLayer( int ) override;
    OGRLayer            *GetLayerByName( const char* pszLayerName ) override;

    int                 UseGeometryColumns() {
        return bUseGeometryColumns;
    }

    virtual int         DeleteLayer( int iLayer ) override;
    virtual OGRLayer    *ICreateLayer( const char *,
                                       OGRSpatialReference * = nullptr,
                                       OGRwkbGeometryType = wkbUnknown,
                                       char ** = nullptr ) override;

    int                 TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    // cppcheck-suppress functionStatic
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    char                *LaunderName( const char *pszSrcName );
    char                *ToUpper( const char *pszSrcName );
    // cppcheck-suppress functionStatic
    OGRErr              InitializeMetadataTables();

    OGRSpatialReference* FetchSRS( int nId );
    int                 FetchSRSId( OGRSpatialReference * poSRS );

    OGRErr              StartTransaction(CPL_UNUSED int bForce) override;
    OGRErr              CommitTransaction() override;
    OGRErr              RollbackTransaction() override;
    OGRErr              SoftStartTransaction();
    OGRErr              SoftCommitTransaction();
    OGRErr              SoftRollbackTransaction();
    // Internal use
    OGRDB2Session     *GetSession() {
        return &m_oSession;
    }
    int                 InitializeSession( const char * pszNewName,
                                           int bTestOpen );
};

/************************************************************************/
/*                             OGRDB2Driver                             */
/************************************************************************/

class OGRDB2Driver final: public GDALDriver
{
public:
    ~OGRDB2Driver();
};

/************************************************************************/
/*                        GDALDB2RasterBand                             */
/************************************************************************/

class GDALDB2RasterBand final: public GDALPamRasterBand
{
public:

    GDALDB2RasterBand(OGRDB2DataSource* poDS,
                      int nBand,
                      int nTileWidth, int nTileHeight);

    virtual CPLErr          IReadBlock(int nBlockXOff, int nBlockYOff,
                                       void* pData) override;
    virtual CPLErr          IWriteBlock(int nBlockXOff, int nBlockYOff,
                                        void* pData) override;
    virtual CPLErr          FlushCache() override;

    virtual GDALColorTable* GetColorTable() override;
    virtual CPLErr          SetColorTable(GDALColorTable* poCT) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual CPLErr          SetColorInterpretation( GDALColorInterp ) override;

    virtual int             GetOverviewCount() override;
    virtual GDALRasterBand* GetOverview(int nIdx) override;
    char*           GByteArrayToHexString( const GByte* pabyData, int nLen);
};
#endif /* ndef OGR_DB2_H_INCLUDED */
