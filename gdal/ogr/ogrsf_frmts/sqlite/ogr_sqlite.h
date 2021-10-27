/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/SQLite driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_SQLITE_H_INCLUDED
#define OGR_SQLITE_H_INCLUDED

#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "cpl_error.h"
#include "gdal_pam.h"
#include "ogrsf_frmts.h"
#include "ogrsf_frmts.h"
#include "rasterlite2_header.h"

#ifdef SPATIALITE_AMALGAMATION
/*
/ using an AMALGAMATED version of SpatiaLite
/ a private internal copy of SQLite is included:
/ so we are required including the SpatiaLite's
/ own header
/
/ IMPORTANT NOTICE: using AMALAGATION is only
/ useful on Windows (to skip DLL hell related oddities)
/
/ You MUST NOT use AMALGAMATION on Linux or any
/ other "sane" operating system !!!!
*/
#include <spatialite/sqlite3.h>
#else
#include <sqlite3.h>
#endif

#define UNINITIALIZED_SRID  -2

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) || defined(ALLOW_FORMAT_DUMPS)
// Enable accepting a SQL dump (starting with a "-- SQL SQLITE" or
// "-- SQL RASTERLITE" or "--SQL MBTILES" line) as a valid
// file. This makes fuzzer life easier
#define ENABLE_SQL_SQLITE_FORMAT
#endif

#include "ogrsqlitebase.h"

/************************************************************************/
/*      SpatiaLite's own Geometry type IDs.                             */
/************************************************************************/

enum OGRSpatialiteGeomType
{
// 2D [XY]
    OGRSplitePointXY                     = 1,
    OGRSpliteLineStringXY                = 2,
    OGRSplitePolygonXY                   = 3,
    OGRSpliteMultiPointXY                = 4,
    OGRSpliteMultiLineStringXY           = 5,
    OGRSpliteMultiPolygonXY              = 6,
    OGRSpliteGeometryCollectionXY        = 7,
// 3D [XYZ]
    OGRSplitePointXYZ                    = 1001,
    OGRSpliteLineStringXYZ               = 1002,
    OGRSplitePolygonXYZ                  = 1003,
    OGRSpliteMultiPointXYZ               = 1004,
    OGRSpliteMultiLineStringXYZ          = 1005,
    OGRSpliteMultiPolygonXYZ             = 1006,
    OGRSpliteGeometryCollectionXYZ       = 1007,
// 2D with Measure [XYM]
    OGRSplitePointXYM                    = 2001,
    OGRSpliteLineStringXYM               = 2002,
    OGRSplitePolygonXYM                  = 2003,
    OGRSpliteMultiPointXYM               = 2004,
    OGRSpliteMultiLineStringXYM          = 2005,
    OGRSpliteMultiPolygonXYM             = 2006,
    OGRSpliteGeometryCollectionXYM       = 2007,
// 3D with Measure [XYZM]
    OGRSplitePointXYZM                   = 3001,
    OGRSpliteLineStringXYZM              = 3002,
    OGRSplitePolygonXYZM                 = 3003,
    OGRSpliteMultiPointXYZM              = 3004,
    OGRSpliteMultiLineStringXYZM         = 3005,
    OGRSpliteMultiPolygonXYZM            = 3006,
    OGRSpliteGeometryCollectionXYZM      = 3007,
// COMPRESSED: 2D [XY]
    OGRSpliteComprLineStringXY           = 1000002,
    OGRSpliteComprPolygonXY              = 1000003,
    OGRSpliteComprMultiPointXY           = 1000004,
    OGRSpliteComprMultiLineStringXY      = 1000005,
    OGRSpliteComprMultiPolygonXY         = 1000006,
    OGRSpliteComprGeometryCollectionXY   = 1000007,
// COMPRESSED: 3D [XYZ]
    OGRSpliteComprLineStringXYZ          = 1001002,
    OGRSpliteComprPolygonXYZ             = 1001003,
    OGRSpliteComprMultiPointXYZ          = 1001004,
    OGRSpliteComprMultiLineStringXYZ     = 1001005,
    OGRSpliteComprMultiPolygonXYZ        = 1001006,
    OGRSpliteComprGeometryCollectionXYZ  = 1001007,
// COMPRESSED: 2D with Measure [XYM]
    OGRSpliteComprLineStringXYM          = 1002002,
    OGRSpliteComprPolygonXYM             = 1002003,
    OGRSpliteComprMultiPointXYM          = 1002004,
    OGRSpliteComprMultiLineStringXYM     = 1002005,
    OGRSpliteComprMultiPolygonXYM        = 1002006,
    OGRSpliteComprGeometryCollectionXYM  = 1002007,
// COMPRESSED: 3D with Measure [XYZM]
    OGRSpliteComprLineStringXYZM         = 1003002,
    OGRSpliteComprPolygonXYZM            = 1003003,
    OGRSpliteComprMultiPointXYZM         = 1003004,
    OGRSpliteComprMultiLineStringXYZM    = 1003005,
    OGRSpliteComprMultiPolygonXYZM       = 1003006,
    OGRSpliteComprGeometryCollectionXYZM = 1003007
};

/************************************************************************/
/*                            OGRSQLiteLayer                            */
/************************************************************************/

class OGRSQLiteDataSource;

class OGRSQLiteLayer CPL_NON_FINAL: public OGRLayer, public IOGRSQLiteGetSpatialWhere
{
  private:
    static OGRErr       createFromSpatialiteInternal(const GByte *pabyData,
                                                     OGRGeometry **ppoReturn,
                                                     int nBytes,
                                                     OGRwkbByteOrder eByteOrder,
                                                     int* pnBytesConsumed,
                                                     int nRecLevel);

    static int          CanBeCompressedSpatialiteGeometry(const OGRGeometry *poGeometry);

    static int          ComputeSpatiaLiteGeometrySize(const OGRGeometry *poGeometry,
                                                      bool bSpatialite2D,
                                                      bool bUseComprGeom );

    static int          GetSpatialiteGeometryCode(const OGRGeometry *poGeometry,
                                                  bool bSpatialite2D,
                                                  bool bUseComprGeom,
                                                  bool bAcceptMultiGeom);

    static int          ExportSpatiaLiteGeometryInternal(const OGRGeometry *poGeometry,
                                                        OGRwkbByteOrder eByteOrder,
                                                        bool bSpatialite2D,
                                                        bool bUseComprGeom,
                                                        GByte* pabyData );

  protected:
    OGRSQLiteFeatureDefn *m_poFeatureDefn = nullptr;

    GIntBig              m_iNextShapeId = 0;

    sqlite3_stmt        *m_hStmt = nullptr;
    bool                 m_bDoStep = true;
    bool                 m_bEOF = false;

    OGRSQLiteDataSource *m_poDS = nullptr;

    char                *m_pszFIDColumn = nullptr;

    int                *m_panFieldOrdinals = nullptr;
    int                 m_iFIDCol = -1;
    int                 m_iOGRNativeDataCol = -1;
    int                 m_iOGRNativeMediaTypeCol = -1;

    bool                m_bIsVirtualShape = false;

    void                BuildFeatureDefn( const char *pszLayerName,
                                          bool bIsSelect,
                                          sqlite3_stmt *hStmt,
                                          const std::set<CPLString>* paosGeomCols,
                                          const std::set<CPLString>& aosIgnoredCols);

    void                ClearStatement();
    virtual OGRErr      ResetStatement() = 0;

    bool                m_bUseComprGeom = false;

    char              **m_papszCompressedColumns = nullptr;

    bool                m_bAllowMultipleGeomFields = false;

    static
    CPLString           FormatSpatialFilterFromRTree(OGRGeometry* poFilterGeom,
                                                       const char* pszRowIDName,
                                                       const char* pszEscapedTable,
                                                       const char* pszEscapedGeomCol);

    static
    CPLString           FormatSpatialFilterFromMBR(OGRGeometry* poFilterGeom,
                                                   const char* pszEscapedGeomColName);

    explicit            OGRSQLiteLayer(OGRSQLiteDataSource* poDSIn);

  public:
    virtual             ~OGRSQLiteLayer();

    void                Finalize();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override { return m_poFeatureDefn; }
    virtual OGRSQLiteFeatureDefn *myGetLayerDefn() { return m_poFeatureDefn; }

    virtual const char *GetFIDColumn() override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr       StartTransaction() override;
    virtual OGRErr       CommitTransaction() override;
    virtual OGRErr       RollbackTransaction() override;

    virtual void        InvalidateCachedFeatureCountAndExtent() { }

    virtual bool          IsTableLayer() { return false; }

    virtual bool          HasSpatialIndex(CPL_UNUSED int iGeomField) { return false; }

    virtual bool          HasFastSpatialFilter(CPL_UNUSED int iGeomCol) override { return false; }
    virtual CPLString     GetSpatialWhere(CPL_UNUSED int iGeomCol,
                                          CPL_UNUSED OGRGeometry* poFilterGeom) override { return ""; }

    static OGRErr       GetSpatialiteGeometryHeader( const GByte *pabyData,
                                                    int nBytes,
                                                    int* pnSRID,
                                                    OGRwkbGeometryType* peType,
                                                    bool* pbIsEmpty,
                                                    double* pdfMinX,
                                                    double* pdfMinY,
                                                    double* pdfMaxX,
                                                    double* pdfMaxY );
    static OGRErr       ImportSpatiaLiteGeometry( const GByte *, int,
                                                  OGRGeometry ** );
    static OGRErr       ImportSpatiaLiteGeometry( const GByte *, int,
                                                  OGRGeometry **, int *pnSRID );
    static OGRErr       ExportSpatiaLiteGeometry( const OGRGeometry *,
                                                  GInt32, OGRwkbByteOrder,
                                                  bool, bool bUseComprGeom, GByte **, int * );
};

/************************************************************************/
/*                         OGRSQLiteTableLayer                          */
/************************************************************************/

class OGRSQLiteTableLayer final: public OGRSQLiteLayer
{
    bool                m_bIsTable = true;

    bool                m_bLaunderColumnNames = true;
    bool                m_bSpatialite2D = false;

    CPLString           m_osWHERE{};
    CPLString           m_osQuery{};
    bool                m_bDeferredSpatialIndexCreation = false;

    char               *m_pszTableName = nullptr;
    char               *m_pszEscapedTableName = nullptr;

    bool                m_bLayerDefnError = false;

    sqlite3_stmt       *m_hInsertStmt = nullptr;
    CPLString           m_osLastInsertStmt{};

    bool                m_bHasCheckedTriggers = false;
    bool                m_bHasTriedDetectingFID64 = false;

    bool                m_bStatisticsNeedsToBeFlushed = false;
    GIntBig             m_nFeatureCount = -1; /* if -1, means not up-to-date */

    int                 m_bDeferredCreation = false;

    char               *m_pszCreationGeomFormat = nullptr;
    int                 m_iFIDAsRegularColumnIndex = -1;

    void                ClearInsertStmt();

    void                BuildWhere();

    virtual OGRErr      ResetStatement() override;

    OGRErr              RecomputeOrdinals();

    void                AddColumnDef(char* pszNewFieldList, size_t nBufLen,
                                     OGRFieldDefn* poFldDefn);

    void                InitFieldListForRecrerate(char* & pszNewFieldList,
                                                  char* & pszFieldListForSelect,
                                                  size_t& nBufLenOut,
                                                  int nExtraSpace = 0);
    OGRErr              RecreateTable(const char* pszFieldListForSelect,
                                      const char* pszNewFieldList,
                                      const char* pszGenericErrorMessage);
    OGRErr              BindValues( OGRFeature *poFeature,
                                    sqlite3_stmt* hStmt,
                                    bool bBindUnsetAsNull );

    bool                CheckSpatialIndexTable(int iGeomCol);

    CPLErr              EstablishFeatureDefn(const char* pszGeomCol);

    void                LoadStatistics();
    void                LoadStatisticsSpatialite4DB();

    CPLString           FieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn );

    OGRErr              RunAddGeometryColumn( const OGRSQLiteGeomFieldDefn *poGeomField,
                                              bool bAddColumnsForNonSpatialite );

  public:
    explicit            OGRSQLiteTableLayer( OGRSQLiteDataSource * );
                        virtual ~OGRSQLiteTableLayer();

    CPLErr              Initialize( const char *pszTableName,
                                    bool bIsTable,
                                    bool bIsVirtualShapeIn,
                                    bool bDeferredCreation);
    void                SetCreationParameters( const char *pszFIDColumnName,
                                               OGRwkbGeometryType eGeomType,
                                               const char *pszGeomFormat,
                                               const char *pszGeometryName,
                                               OGRSpatialReference *poSRS,
                                               int nSRSId );
    virtual const char* GetName() override;

    virtual GIntBig     GetFeatureCount( int ) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;
    bool                HasLayerDefnError() { GetLayerDefn(); return m_bLayerDefnError; }

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;
    virtual OGRErr      SetAttributeFilter( const char * ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                         int bApproxOK = TRUE ) override;
    virtual OGRErr      DeleteField( int iField ) override;
    virtual OGRErr      ReorderFields( int* panMap ) override;
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags ) override;

    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;

    virtual char      **GetMetadata( const char * pszDomain = "" ) override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;

    // follow methods are not base class overrides
    void                SetLaunderFlag( bool bFlag )
                                { m_bLaunderColumnNames = bFlag; }
    void                SetUseCompressGeom( bool bFlag )
                                { m_bUseComprGeom = bFlag; }
    void                SetDeferredSpatialIndexCreation( bool bFlag )
                                { m_bDeferredSpatialIndexCreation = bFlag; }
    void                SetCompressedColumns( const char* pszCompressedColumns );

    int                 CreateSpatialIndex(int iGeomCol);

    void                CreateSpatialIndexIfNecessary();

    void                InitFeatureCount();
    bool                DoStatisticsNeedToBeFlushed();
    void                ForceStatisticsToBeFlushed();
    bool                AreStatisticsValid();
    int                 SaveStatistics();

    virtual void        InvalidateCachedFeatureCountAndExtent() override;

    virtual bool         IsTableLayer() override { return true; }

    virtual bool         HasSpatialIndex(int iGeomField) override;
    virtual bool         HasFastSpatialFilter(int iGeomCol) override;
    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom) override;

    OGRErr               RunDeferredCreationIfNecessary();
};

/************************************************************************/
/*                         OGRSQLiteViewLayer                           */
/************************************************************************/

class OGRSQLiteViewLayer final: public OGRSQLiteLayer
{
    CPLString           m_osWHERE{};
    CPLString           m_osQuery{};
    bool                m_bHasCheckedSpatialIndexTable = false;

    OGRSQLiteGeomFormat m_eGeomFormat = OSGF_None;
    CPLString           m_osGeomColumn{};
    bool                m_bHasSpatialIndex = false;

    char               *m_pszViewName = nullptr;
    char               *m_pszEscapedTableName = nullptr;
    char               *m_pszEscapedUnderlyingTableName = nullptr;

    bool                m_bLayerDefnError = false;

    CPLString           m_osUnderlyingTableName{};
    CPLString           m_osUnderlyingGeometryColumn{};

    OGRSQLiteLayer     *m_poUnderlyingLayer = nullptr;

    OGRSQLiteLayer     *GetUnderlyingLayer();

    void                BuildWhere();

    virtual OGRErr      ResetStatement() override;

    CPLErr              EstablishFeatureDefn();

  public:
    explicit            OGRSQLiteViewLayer( OGRSQLiteDataSource * );
                        virtual ~OGRSQLiteViewLayer();

    virtual const char* GetName() override { return m_pszViewName; }
    virtual OGRwkbGeometryType GetGeomType() override;

    CPLErr              Initialize( const char *pszViewName,
                                    const char *pszViewGeometry,
                                    const char *pszViewRowid,
                                    const char *pszTableName,
                                    const char *pszGeometryColumn);

    virtual OGRFeatureDefn *GetLayerDefn() override;
    bool                HasLayerDefnError() { GetLayerDefn(); return m_bLayerDefnError; }

    virtual OGRFeature *GetNextFeature() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRSQLiteLayer::SetSpatialFilter(iGeomField, poGeom); }
    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;

    virtual bool         HasSpatialIndex(CPL_UNUSED int iGeomField) override { return m_bHasSpatialIndex; }
    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom) override;
};

/************************************************************************/
/*                         OGRSQLiteSelectLayer                         */
/************************************************************************/

class OGRSQLiteSelectLayer CPL_NON_FINAL: public OGRSQLiteLayer, public IOGRSQLiteSelectLayer
{
    OGRSQLiteSelectLayerCommonBehaviour* m_poBehavior = nullptr;

    virtual OGRErr      ResetStatement() override;

    CPL_DISALLOW_COPY_ASSIGN(OGRSQLiteSelectLayer)

  public:
                        OGRSQLiteSelectLayer( OGRSQLiteDataSource *,
                                              const CPLString& osSQL,
                                              sqlite3_stmt *,
                                              bool bUseStatementForGetNextFeature,
                                              bool bEmptyLayer,
                                              bool bAllowMultipleGeomFields );
                       virtual ~OGRSQLiteSelectLayer();

    virtual void        ResetReading() override;

    virtual OGRFeature *GetNextFeature() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual void        SetSpatialFilter( OGRGeometry * poGeom ) override { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;
    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual OGRFeatureDefn *     GetLayerDefn() override { return OGRSQLiteLayer::GetLayerDefn(); }
    virtual char*&               GetAttrQueryString() override { return m_pszAttrQueryString; }
    virtual OGRFeatureQuery*&    GetFeatureQuery() override { return m_poAttrQuery; }
    virtual OGRGeometry*&        GetFilterGeom() override { return m_poFilterGeom; }
    virtual int&                 GetIGeomFieldFilter() override { return m_iGeomFieldFilter; }
    virtual OGRSpatialReference* GetSpatialRef() override { return OGRSQLiteLayer::GetSpatialRef(); }
    virtual int                  InstallFilter( OGRGeometry * poGeomIn ) override { return OGRSQLiteLayer::InstallFilter(poGeomIn); }
    virtual int                  HasReadFeature() override { return m_iNextShapeId > 0; }
    virtual void                 BaseResetReading() override { OGRSQLiteLayer::ResetReading(); }
    virtual OGRFeature          *BaseGetNextFeature() override { return OGRSQLiteLayer::GetNextFeature(); }
    virtual OGRErr               BaseSetAttributeFilter(const char* pszQuery) override { return OGRSQLiteLayer::SetAttributeFilter(pszQuery); }
    virtual GIntBig              BaseGetFeatureCount(int bForce) override { return OGRSQLiteLayer::GetFeatureCount(bForce); }
    virtual int                  BaseTestCapability( const char *pszCap ) override { return OGRSQLiteLayer::TestCapability(pszCap); }
    virtual OGRErr               BaseGetExtent(OGREnvelope *psExtent, int bForce) override { return OGRSQLiteLayer::GetExtent(psExtent, bForce); }
    virtual OGRErr               BaseGetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override { return OGRSQLiteLayer::GetExtent(iGeomField, psExtent, bForce); }
};

/************************************************************************/
/*                         OGRSQLiteDataSource                          */
/************************************************************************/

class OGRSQLiteDataSource final : public OGRSQLiteBaseDataSource
{
    OGRSQLiteLayer    **m_papoLayers = nullptr;
    int                 m_nLayers = 0;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 m_nKnownSRID = 0;
    int                *m_panSRID = nullptr;
    OGRSpatialReference **m_papoSRS = nullptr;

    void                AddSRIDToCache(int nId, OGRSpatialReference * poSRS );

    bool                m_bHaveGeometryColumns = false;
    bool                m_bIsSpatiaLiteDB = false;
    bool                m_bSpatialite4Layout = false;

    int                 m_nUndefinedSRID = -1;

    virtual void        DeleteLayer( const char *pszLayer );

    const char*         GetSRTEXTColName();

    bool                InitWithEPSG();

    bool                OpenVirtualTable(const char* pszName, const char* pszSQL);

    GIntBig             m_nFileTimestamp = 0;
    bool                m_bLastSQLCommandIsUpdateLayerStatistics = false;

    std::map< CPLString, std::set<CPLString> > m_aoMapTableToSetOfGeomCols{};

    void                SaveStatistics();

    std::vector<OGRLayer*> m_apoInvisibleLayers{};

#ifdef HAVE_RASTERLITE2
    void               *m_hRL2Ctxt = nullptr;
    bool                InitRasterLite2();
    void                FinishRasterLite2();

    CPLString           m_osCoverageName{};
    GIntBig             m_nSectionId = -1;
    rl2CoveragePtr      m_pRL2Coverage = nullptr;
    bool                m_bRL2MixedResolutions = false;
#endif
    CPLStringList       m_aosSubDatasets{};
    bool                m_bGeoTransformValid = false;
    double              m_adfGeoTransform[6];
    CPLString           m_osProjection{};
    bool                m_bPromote1BitAs8Bit = false;
    bool                OpenRaster();
    bool                OpenRasterSubDataset(const char* pszConnectionId);
    OGRSQLiteDataSource* m_poParentDS = nullptr;
    std::vector<OGRSQLiteDataSource*> m_apoOverviewDS{};

#ifdef HAVE_RASTERLITE2
    void                ListOverviews();
    void                CreateRL2OverviewDatasetIfNeeded(double dfXRes,
                                                      double dfYRes);
#endif

  public:
                        OGRSQLiteDataSource();
                        virtual ~OGRSQLiteDataSource();

    bool                Open( GDALOpenInfo* poOpenInfo );
    bool                Create( const char *, char **papszOptions );

    bool                OpenTable( const char *pszTableName,
                                   bool IsTable,
                                   bool bIsVirtualShape );
    bool                OpenView( const char *pszViewName,
                                   const char *pszViewGeometry,
                                   const char *pszViewRowid,
                                   const char *pszTableName,
                                   const char *pszGeometryColumn);

    virtual int         GetLayerCount() override { return m_nLayers; }
    virtual OGRLayer   *GetLayer( int ) override;
    virtual OGRLayer   *GetLayerByName( const char* ) override;
    virtual bool        IsLayerPrivate( int ) const override;
    OGRLayer           *GetLayerByNameNotVisible( const char* );
    virtual std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetLayerWithGetSpatialWhereByName( const char* pszName ) override;

    virtual OGRLayer    *ICreateLayer( const char *pszLayerName,
                                      OGRSpatialReference *poSRS,
                                      OGRwkbGeometryType eType,
                                      char **papszOptions ) override;
    virtual OGRErr      DeleteLayer(int) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    virtual void        FlushCache(bool bAtClosing) override;

    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual char**      GetMetadata(const char* pszDomain = "") override;

    virtual CPLErr      GetGeoTransform( double* padfGeoTransform ) override;
    virtual const char* _GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    char               *LaunderName( const char * );
    int                 FetchSRSId( const OGRSpatialReference * poSRS );
    OGRSpatialReference*FetchSRS( int nSRID );

    void                DisableUpdate() { eAccess = GA_ReadOnly; }

    void                SetName(const char* pszNameIn);

    const std::set<CPLString>& GetGeomColsForTable(const char* pszTableName)
            { return m_aoMapTableToSetOfGeomCols[pszTableName]; }

    GIntBig             GetFileTimestamp() const { return m_nFileTimestamp; }

    bool                IsSpatialiteLoaded();
    int                 GetSpatialiteVersionNumber();

    bool                IsSpatialiteDB() const { return m_bIsSpatiaLiteDB; }
    bool                HasSpatialite4Layout() const { return m_bSpatialite4Layout; }

    int                 GetUndefinedSRID() const { return m_nUndefinedSRID; }
    bool                HasGeometryColumns() const { return m_bHaveGeometryColumns; }

    void                ReloadLayers();

#ifdef HAVE_RASTERLITE2
    void*               GetRL2Context() const { return m_hRL2Ctxt; }
    rl2CoveragePtr      GetRL2CoveragePtr() const { return m_pRL2Coverage; }
    GIntBig             GetSectionId() const { return m_nSectionId; }
    const double*       GetGeoTransform() const { return m_adfGeoTransform; }
    bool                IsRL2MixedResolutions() const { return m_bRL2MixedResolutions; }

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * ) override;

#endif
    OGRSQLiteDataSource* GetParentDS() const { return m_poParentDS; }
    const std::vector<OGRSQLiteDataSource*>& GetOverviews() const { return m_apoOverviewDS; }
    bool                 HasPromote1BitAS8Bit() const { return m_bPromote1BitAs8Bit; }
};

#ifdef HAVE_RASTERLITE2
/************************************************************************/
/*                           RL2RasterBand                              */
/************************************************************************/

class RL2RasterBand final: public GDALPamRasterBand
{
    bool            m_bHasNoData;
    double          m_dfNoDataValue;
    GDALColorInterp m_eColorInterp;
    GDALColorTable* m_poCT;

    public:
                            RL2RasterBand( int nBandIn,
                                           int nPixelType,
                                           GDALDataType eDT,
                                           int nBits,
                                           bool bPromote1BitAs8Bit,
                                           bool bSigned,
                                           int nBlockXSizeIn,
                                           int nBlockYSizeIn,
                                           bool bHasNoDataIn,
                                           double dfNoDataValueIn );
        explicit            RL2RasterBand( const RL2RasterBand* poOther );

        virtual            ~RL2RasterBand();

    protected:

        virtual CPLErr      IReadBlock( int, int, void* ) override;
        virtual GDALColorInterp GetColorInterpretation() override
                                                    { return m_eColorInterp; }
        virtual double      GetNoDataValue( int* pbSuccess = nullptr ) override;
        virtual GDALColorTable* GetColorTable() override;
        virtual int         GetOverviewCount() override;
        virtual GDALRasterBand* GetOverview(int) override;
};
#endif // HAVE_RASTERLITE2

CPLString OGRSQLiteFieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn,
                                               bool bSQLiteDialectInternalUse );

void OGRSQLiteRegisterInflateDeflate(sqlite3* hDB);

void OGRSQLiteDriverUnload(GDALDriver*);

#ifdef HAVE_RASTERLITE2
GDALDataset *OGRSQLiteDriverCreateCopy( const char *, GDALDataset *,
                                        int, char **,
                                        GDALProgressFunc pfnProgress,
                                        void * pProgressData );
#endif

#endif /* ndef OGR_SQLITE_H_INCLUDED */
