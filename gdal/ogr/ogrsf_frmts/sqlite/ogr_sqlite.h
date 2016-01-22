/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/SQLite driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_SQLITE_H_INCLUDED
#define _OGR_SQLITE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include <map>
#include <set>

#ifdef HAVE_SPATIALITE
  #ifdef SPATIALITE_AMALGAMATION
    /*
    / using an AMALGAMATED version of SpatiaLite
    / a private internal copy of SQLite is included:
    / so we are required including the SpatiaLite's 
    / own header 
    /
    / IMPORTANT NOTICE: using AMALAGATION is only
    / useful on Windows (to skip DLL hell related oddities)
    */
    #include <spatialite/sqlite3.h>
  #else
    /*
    / You MUST NOT use AMALGAMATION on Linux or any
    / other "sane" operating system !!!!
    */
    #include "sqlite3.h"
  #endif
#else
#include "sqlite3.h"
#endif

#if SQLITE_VERSION_NUMBER >= 3006000
#define HAVE_SQLITE_VFS
#define HAVE_SQLITE3_PREPARE_V2
#endif

#define UNINITIALIZED_SRID  -2

/************************************************************************/
/*      Format used to store geometry data in the database.             */
/************************************************************************/

enum OGRSQLiteGeomFormat
{
    OSGF_None = 0,
    OSGF_WKT = 1,
    OSGF_WKB = 2,
    OSGF_FGF = 3,
    OSGF_SpatiaLite = 4
};

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
/*                        OGRSQLiteGeomFieldDefn                        */
/************************************************************************/

class OGRSQLiteGeomFieldDefn : public OGRGeomFieldDefn
{
    public:
        OGRSQLiteGeomFieldDefn( const char* pszName, int iGeomColIn ) :
            OGRGeomFieldDefn(pszName, wkbUnknown), nSRSId(UNINITIALIZED_SRID),
            iCol(iGeomColIn), bTriedAsSpatiaLite(FALSE), eGeomFormat(OSGF_None)
            {
            }

        int nSRSId;
        int iCol; /* ordinal of geometry field in SQL statement */
        int bTriedAsSpatiaLite;
        OGRSQLiteGeomFormat eGeomFormat;
};

/************************************************************************/
/*                        OGRSQLiteFeatureDefn                          */
/************************************************************************/

class OGRSQLiteFeatureDefn : public OGRFeatureDefn
{
    public:
        OGRSQLiteFeatureDefn( const char * pszName = NULL ) :
            OGRFeatureDefn(pszName)
        {
            SetGeomType(wkbNone);
        }
            
        OGRSQLiteGeomFieldDefn* myGetGeomFieldDefn(int i)
        {
            return (OGRSQLiteGeomFieldDefn*) GetGeomFieldDefn(i);
        }
};

/************************************************************************/
/*                            OGRSQLiteLayer                            */
/************************************************************************/

class OGRSQLiteDataSource;
    
class OGRSQLiteLayer : public OGRLayer
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
                                                      int bHasM, int bSpatialite2D,
                                                      int bUseComprGeom );

    static int          GetSpatialiteGeometryCode(const OGRGeometry *poGeometry,
                                                  int bHasM, int bSpatialite2D,
                                                  int bUseComprGeom,
                                                  int bAcceptMultiGeom);

    static int          ExportSpatiaLiteGeometryInternal(const OGRGeometry *poGeometry,
                                                        OGRwkbByteOrder eByteOrder,
                                                        int bHasM, int bSpatialite2D,
                                                        int bUseComprGeom,
                                                        GByte* pabyData );

  protected:
    OGRSQLiteFeatureDefn *poFeatureDefn;

    int                 iNextShapeId;

    sqlite3_stmt        *hStmt;
    int                  bDoStep;

    OGRSQLiteDataSource *poDS;

    char                *pszFIDColumn;

    int                *panFieldOrdinals;
    int                 iFIDCol;

    int                 bHasSpatialIndex;
    int                 bHasM;

    int                 bIsVirtualShape;

    void                BuildFeatureDefn( const char *pszLayerName,
                                          sqlite3_stmt *hStmt,
                                          const char *pszExpectedGeomCol,
                                          const std::set<CPLString>& aosGeomCols);

    void                ClearStatement();
    virtual OGRErr      ResetStatement() = 0;

    int                 bUseComprGeom;

    char              **papszCompressedColumns;

    int                 bAllowMultipleGeomFields;

  public:
                        OGRSQLiteLayer();
    virtual             ~OGRSQLiteLayer();

    virtual void        Finalize();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
    virtual OGRSQLiteFeatureDefn *myGetLayerDefn() { return poFeatureDefn; }

    virtual const char *GetFIDColumn();

    virtual int         TestCapability( const char * );

    virtual OGRErr       StartTransaction();
    virtual OGRErr       CommitTransaction();
    virtual OGRErr       RollbackTransaction();

    virtual void        InvalidateCachedFeatureCountAndExtent() { }

    virtual int          IsTableLayer() { return FALSE; }

    virtual int          HasSpatialIndex() { return bHasSpatialIndex; }

    virtual CPLString     GetSpatialWhere(CPL_UNUSED int iGeomCol,
                                          CPL_UNUSED OGRGeometry* poFilterGeom) { return ""; }

    static OGRErr       ImportSpatiaLiteGeometry( const GByte *, int,
                                                  OGRGeometry ** );
    static OGRErr       ImportSpatiaLiteGeometry( const GByte *, int,
                                                  OGRGeometry **, int *pnSRID );
    static OGRErr       ExportSpatiaLiteGeometry( const OGRGeometry *,
                                                  GInt32, OGRwkbByteOrder,
                                                  int, int, int bUseComprGeom, GByte **, int * );

};

/************************************************************************/
/*                         OGRSQLiteTableLayer                          */
/************************************************************************/

class OGRSQLiteTableLayer : public OGRSQLiteLayer
{
    int                 bLaunderColumnNames;
    int                 bSpatialite2D;

    CPLString           osWHERE;
    CPLString           osQuery;
    int                 bHasCheckedSpatialIndexTable;
    int                 bDeferedSpatialIndexCreation;

    OGRwkbGeometryType  eGeomType;

    char               *pszTableName;
    char               *pszEscapedTableName;
    CPLString           osLayerName;

    int                 bLayerDefnError;

    sqlite3_stmt       *hInsertStmt;
    CPLString           osLastInsertStmt;

    OGRSQLiteGeomFormat eGeomFormat;
    char                *pszGeomCol;
    int                 nSRSId;
    OGRSpatialReference *poSRS;

    void                ClearInsertStmt();

    void                BuildWhere(void);

    virtual OGRErr      ResetStatement();

    OGRErr              RecomputeOrdinals();

    OGRErr              AddColumnAncientMethod( OGRFieldDefn& oField);

    void                InitFieldListForRecrerate(char* & pszNewFieldList,
                                                  char* & pszFieldListForSelect,
                                                  int nExtraSpace = 0);
    OGRErr              RecreateTable(const char* pszFieldListForSelect,
                                      const char* pszNewFieldList,
                                      const char* pszGenericErrorMessage);
    OGRErr              BindValues( OGRFeature *poFeature,
                                        sqlite3_stmt* hStmt,
                                        int bBindNullValues );

    int                 CheckSpatialIndexTable();

    CPLErr              EstablishFeatureDefn();

    int                 bStatisticsNeedsToBeFlushed;
    OGREnvelope         oCachedExtent;
    int                 bCachedExtentIsValid;
    GIntBig             nFeatureCount; /* if -1, means not up-to-date */

    void                LoadStatistics();
    void                LoadStatisticsSpatialite4DB();

    CPLString           FieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn );

  public:
                        OGRSQLiteTableLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteTableLayer();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol,
                                    int bMustIncludeGeomColName,
                                    OGRwkbGeometryType eGeomType,
                                    const char *pszGeomFormat,
                                    OGRSpatialReference *poSRS,
                                    int nSRSId = UNINITIALIZED_SRID,
                                    int bHasSpatialIndex = FALSE,
                                    int bHasM = FALSE,
                                    int bIsVirtualShapeIn = FALSE);

    virtual const char* GetName();
    virtual OGRwkbGeometryType GetGeomType() { return (eGeomType != wkbUnknown) ? eGeomType : OGRLayer::GetGeomType(); }

    virtual int         GetFeatureCount( int );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce);

    virtual OGRFeatureDefn *GetLayerDefn();
    int                 HasLayerDefnError() { GetLayerDefn(); return bLayerDefnError; }

    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( long nFID );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual OGRFeature *GetNextFeature();
    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual int         TestCapability( const char * );

    // follow methods are not base class overrides
    void                SetLaunderFlag( int bFlag ) 
                                { bLaunderColumnNames = bFlag; }
    void                SetUseCompressGeom( int bFlag )
                                { bUseComprGeom = bFlag; }
    void                SetDeferedSpatialIndexCreation( int bFlag )
                                { bDeferedSpatialIndexCreation = bFlag; }
    void                SetCompressedColumns( const char* pszCompressedColumns );

    int                 CreateSpatialIndex();

    void                CreateSpatialIndexIfNecessary();

    void                InitFeatureCount();
    int                 DoStatisticsNeedToBeFlushed();
    void                ForceStatisticsToBeFlushed();
    int                 AreStatisticsValid();
    int                 SaveStatistics();

    virtual void        InvalidateCachedFeatureCountAndExtent();

    virtual int          IsTableLayer() { return TRUE; }

    virtual int          HasSpatialIndex();

    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom);
};

/************************************************************************/
/*                         OGRSQLiteViewLayer                           */
/************************************************************************/

class OGRSQLiteViewLayer : public OGRSQLiteLayer
{
    CPLString           osWHERE;
    CPLString           osQuery;
    int                 bHasCheckedSpatialIndexTable;

    OGRSQLiteGeomFormat eGeomFormat;
    CPLString           osGeomColumn;
    
    char               *pszViewName;
    char               *pszEscapedTableName;
    char               *pszEscapedUnderlyingTableName;

    int                 bLayerDefnError;

    CPLString           osUnderlyingTableName;
    CPLString           osUnderlyingGeometryColumn;

    OGRSQLiteLayer     *poUnderlyingLayer;
    OGRSQLiteLayer     *GetUnderlyingLayer();

    void                BuildWhere(void);

    virtual OGRErr      ResetStatement();

    CPLErr              EstablishFeatureDefn();

  public:
                        OGRSQLiteViewLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteViewLayer();

    virtual const char* GetName() { return pszViewName; }
    virtual OGRwkbGeometryType GetGeomType();

    CPLErr              Initialize( const char *pszViewName,
                                    const char *pszViewGeometry,
                                    const char *pszViewRowid,
                                    const char *pszTableName,
                                    const char *pszGeometryColumn);

    virtual OGRFeatureDefn *GetLayerDefn();
    int                 HasLayerDefnError() { GetLayerDefn(); return bLayerDefnError; }

    virtual OGRFeature *GetNextFeature();
    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual int         TestCapability( const char * );

    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom);
};

/************************************************************************/
/*                         OGRSQLiteSelectLayer                         */
/************************************************************************/

class OGRSQLiteSelectLayer : public OGRSQLiteLayer
{
    CPLString           osSQLBase;
    CPLString           osSQLCurrent;

    int                 bEmptyLayer;
    int                 bSpatialFilterInSQL;

    virtual OGRErr      ResetStatement();

    OGRSQLiteLayer     *GetBaseLayer(size_t& i);
    int                 BuildSQL();

    int                 bAllowResetReadingEvenIfIndexAtZero;
 
  public:
                        OGRSQLiteSelectLayer( OGRSQLiteDataSource *, 
                                              CPLString osSQL,
                                              sqlite3_stmt *,
                                              int bUseStatementForGetNextFeature,
                                              int bEmptyLayer,
                                              int bAllowMultipleGeomFields );

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();
    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);
};

/************************************************************************/
/*                   OGRSQLiteSingleFeatureLayer                        */
/************************************************************************/

class OGRSQLiteSingleFeatureLayer : public OGRLayer
{
  private:
    int                 nVal;
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGRSQLiteSingleFeatureLayer( const char* pszLayerName,
                                                     int nVal );
                        OGRSQLiteSingleFeatureLayer( const char* pszLayerName,
                                                     const char *pszVal );
                        ~OGRSQLiteSingleFeatureLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual OGRFeatureDefn *GetLayerDefn();
    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                         OGRSQLiteDataSource                          */
/************************************************************************/

class OGRSQLiteDataSource : public OGRDataSource
{
    OGRSQLiteLayer    **papoLayers;
    int                 nLayers;
    
    char               *pszName;

    sqlite3             *hDB;
    int                 bUpdate;

    int                 nSoftTransactionLevel;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes. 
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;
    void                AddSRIDToCache(int nId, OGRSpatialReference * poSRS );

    int                 bHaveGeometryColumns;
    int                 bIsSpatiaLiteDB;
    int                 bSpatialite4Layout;

#ifdef SPATIALITE_412_OR_LATER
    void               *hSpatialiteCtxt;
    int                 InitNewSpatialite();
    void                FinishNewSpatialite();
#endif

    int                 nUndefinedSRID;

    virtual void        DeleteLayer( const char *pszLayer );

    const char*         GetSRTEXTColName();

    int                 OpenOrCreateDB(int flags);
    int                 InitWithEPSG();
    int                 SetSynchronous();
    int                 SetCacheSize();

    int                 OpenVirtualTable(const char* pszName, const char* pszSQL);

#ifdef HAVE_SQLITE_VFS
    sqlite3_vfs*        pMyVFS;
#endif

    VSILFILE*           fpMainFile; /* Set by the VFS layer when it opens the DB */
                                    /* Must *NOT* be closed by the datasource explicitely. */
    GIntBig             nFileTimestamp;
    int                 bLastSQLCommandIsUpdateLayerStatistics;

    std::map<CPLString, OGREnvelope> oMapSQLEnvelope;

    std::map< CPLString, std::set<CPLString> > aoMapTableToSetOfGeomCols;

    void                SaveStatistics();

  public:
                        OGRSQLiteDataSource();
                        ~OGRSQLiteDataSource();

    int                 Open( const char *, int bUpdateIn );
    int                 Create( const char *, char **papszOptions );

    int                 OpenTable( const char *pszTableName, 
                                   const char *pszGeomCol = NULL,
                                   int bMustIncludeGeomColName = FALSE,
                                   OGRwkbGeometryType eGeomType = wkbUnknown,
                                   const char *pszGeomFormat = NULL,
                                   OGRSpatialReference *poSRS = NULL,
                                   int nSRID = UNINITIALIZED_SRID,
                                   int bHasSpatialIndex = FALSE,
                                   int bHasM = FALSE,
                                   int bIsVirtualShapeIn = FALSE );
    int                  OpenView( const char *pszViewName,
                                   const char *pszViewGeometry,
                                   const char *pszViewRowid,
                                   const char *pszTableName,
                                   const char *pszGeometryColumn);

    virtual const char *GetName() { return pszName; }
    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer   *GetLayer( int );
    virtual OGRLayer   *GetLayerByName( const char* );

    virtual OGRLayer    *CreateLayer( const char *pszLayerName, 
                                      OGRSpatialReference *poSRS, 
                                      OGRwkbGeometryType eType, 
                                      char **papszOptions );
    virtual OGRErr      DeleteLayer(int);

    virtual int         TestCapability( const char * );

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    OGRErr              SoftStartTransaction();
    OGRErr              SoftCommit();
    OGRErr              SoftRollback();
    
    OGRErr              FlushSoftTransaction();

    sqlite3            *GetDB() { return hDB; }

    char               *LaunderName( const char * );
    int                 FetchSRSId( OGRSpatialReference * poSRS );
    OGRSpatialReference*FetchSRS( int nSRID );

    int                 GetUpdate() const { return bUpdate; }
    void                SetUpdate(int bUpdateIn) { bUpdate = bUpdateIn; }

    void                SetName(const char* pszNameIn);

    void                NotifyFileOpened (const char* pszFilename,
                                          VSILFILE* fp);

    const OGREnvelope*  GetEnvelopeFromSQL(const CPLString& osSQL);
    void                SetEnvelopeForSQL(const CPLString& osSQL, const OGREnvelope& oEnvelope);

    const std::set<CPLString>& GetGeomColsForTable(const char* pszTableName)
            { return aoMapTableToSetOfGeomCols[pszTableName]; }

    GIntBig             GetFileTimestamp() const { return nFileTimestamp; }

    int                 IsSpatialiteLoaded();
    int                 GetSpatialiteVersionNumber();

    int                 IsSpatialiteDB() const { return bIsSpatiaLiteDB; }
    int                 HasSpatialite4Layout() const { return bSpatialite4Layout; }

    int                 GetUndefinedSRID() const { return nUndefinedSRID; }

    void                ReloadLayers();
};

/************************************************************************/
/*                           OGRSQLiteDriver                            */
/************************************************************************/

class OGRSQLiteDriver : public OGRSFDriver
{
  public:
                ~OGRSQLiteDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    virtual OGRErr      DeleteDataSource( const char *pszName );
    
    int                 TestCapability( const char * );
};

/* To escape literals. The returned string doesn't contain the surrounding single quotes */
CPLString OGRSQLiteEscape( const char *pszLiteral );

/* To escape table or field names. The returned string doesn't contain the surrounding double quotes */
CPLString OGRSQLiteEscapeName( const char* pszName );

CPLString OGRSQLiteParamsUnquote(const char* pszVal);

CPLString OGRSQLiteFieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn );

int OGRSQLITEStringToDateTimeField( OGRFeature* poFeature, int iField,
                                    const char* pszValue );

#ifdef HAVE_SQLITE_VFS
typedef void (*pfnNotifyFileOpenedType)(void* pfnUserData, const char* pszFilename, VSILFILE* fp);
sqlite3_vfs* OGRSQLiteCreateVFS(pfnNotifyFileOpenedType pfn, void* pfnUserData);
#endif

void OGRSQLiteRegisterInflateDeflate(sqlite3* hDB);

#endif /* ndef _OGR_SQLITE_H_INCLUDED */


