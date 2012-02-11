/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/SQLite driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
    OGRFeatureDefn     *poFeatureDefn;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    int                 iNextShapeId;

    sqlite3_stmt        *hStmt;

    OGRSQLiteDataSource *poDS;

    int                 bTriedAsSpatiaLite;
    CPLString           osGeomColumn;
    OGRSQLiteGeomFormat eGeomFormat;

    char                *pszFIDColumn;

    int                *panFieldOrdinals;
    int                 bHasSpatialIndex;
    int                 bHasM;
    int                 bSpatialiteReadOnly;
    int                 bSpatialiteLoaded;
    int                 iSpatialiteVersion;

    int                 bIsVirtualShape;

    void                BuildFeatureDefn( const char *pszLayerName,
                                          sqlite3_stmt *hStmt );

    void                ClearStatement();
    virtual OGRErr      ResetStatement() = 0;

    static OGRErr       ImportSpatiaLiteGeometry( const GByte *, int,
                                                  OGRGeometry ** );
    static OGRErr       ExportSpatiaLiteGeometry( const OGRGeometry *,
                                                  GInt32, OGRwkbByteOrder,
                                                  int, int, int bUseComprGeom, GByte **, int * );

    int                 bUseComprGeom;

  public:
                        OGRSQLiteLayer();
    virtual             ~OGRSQLiteLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual int         TestCapability( const char * );

    virtual OGRErr       StartTransaction();
    virtual OGRErr       CommitTransaction();
    virtual OGRErr       RollbackTransaction();

    virtual int          IsTableLayer() { return FALSE; }

    int                  HasSpatialIndex() const { return bHasSpatialIndex; }

    virtual CPLString     GetSpatialWhere(OGRGeometry* poFilterGeom) { return ""; }
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

    OGRwkbGeometryType  eGeomType;

    char               *pszTableName;
    char               *pszEscapedTableName;

    int                 bLayerDefnError;

    sqlite3_stmt       *hInsertStmt;
    CPLString           osLastInsertStmt;
    void                ClearInsertStmt();

    void                BuildWhere(void);

    OGRErr              ResetStatement();

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

  public:
                        OGRSQLiteTableLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteTableLayer();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol,
                                    OGRwkbGeometryType eGeomType,
                                    const char *pszGeomFormat,
                                    OGRSpatialReference *poSRS,
                                    int nSRSId = -1,
                                    int bHasSpatialIndex = FALSE,
                                    int bHasM = FALSE,
                                    int bSpatialiteReadOnly = FALSE,
                                    int bSpatialiteLoaded = FALSE,
                                    int iSpatialiteVersion = -1,
                                    int bIsVirtualShapeIn = FALSE);

    virtual const char* GetName() { return pszTableName; }
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
    void                SetSpatialite2D( int bFlag ) 
                                { bSpatialite2D = bFlag; }
    void                SetUseCompressGeom( int bFlag )
                                { bUseComprGeom = bFlag; }

    virtual int          IsTableLayer() { return TRUE; }

    virtual CPLString    GetSpatialWhere(OGRGeometry* poFilterGeom);
};

/************************************************************************/
/*                         OGRSQLiteViewLayer                           */
/************************************************************************/

class OGRSQLiteViewLayer : public OGRSQLiteLayer
{
    CPLString           osWHERE;
    CPLString           osQuery;
    int                 bHasCheckedSpatialIndexTable;

    char               *pszEscapedTableName;
    char               *pszEscapedUnderlyingTableName;

    CPLString           osUnderlyingTableName;
    CPLString           osUnderlyingGeometryColumn;

    void                BuildWhere(void);

    OGRErr              ResetStatement();

  public:
                        OGRSQLiteViewLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteViewLayer();

    CPLErr              Initialize( const char *pszViewName,
                                    const char *pszViewGeometry,
                                    const char *pszViewRowid,
                                    const char *pszTableName,
                                    const char *pszGeometryColumn,
                                    int bSpatialiteLoaded);

    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual int         TestCapability( const char * );

    virtual CPLString    GetSpatialWhere(OGRGeometry* poFilterGeom);
};

/************************************************************************/
/*                         OGRSQLiteSelectLayer                         */
/************************************************************************/

class OGRSQLiteSelectLayer : public OGRSQLiteLayer
{
    CPLString           osSQLBase;
    CPLString           osSQLCurrent;

    OGRErr              ResetStatement();

    OGRSQLiteLayer     *GetBaseLayer(size_t& i);
    void                RebuildSQL();

  public:
                        OGRSQLiteSelectLayer( OGRSQLiteDataSource *, 
                                              CPLString osSQL,
                                              sqlite3_stmt * );
                        ~OGRSQLiteSelectLayer();

    virtual void        SetSpatialFilter( OGRGeometry * );

    virtual int         TestCapability( const char * );
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

    int                 bHaveGeometryColumns;
    int                 bIsSpatiaLite;
    
    virtual void        DeleteLayer( const char *pszLayer );

    int                 DetectSRSWktColumn();

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

  public:
                        OGRSQLiteDataSource();
                        ~OGRSQLiteDataSource();

    int                 Open( const char *, int bUpdateIn );
    int                 Create( const char *, char **papszOptions );

    int                 OpenTable( const char *pszTableName, 
                                   const char *pszGeomCol = NULL,
                                   OGRwkbGeometryType eGeomType = wkbUnknown,
                                   const char *pszGeomFormat = NULL,
                                   OGRSpatialReference *poSRS = NULL,
                                   int nSRID = -1,
                                   int bHasSpatialIndex = FALSE,
                                   int bHasM = FALSE,
                                   int bSpatialiteReadOnly = FALSE,
                                   int bSpatialiteLoaded = FALSE,
                                   int iSpatialiteVersion = -1,
                                   int bForce2D = FALSE,
                                   int bIsVirtualShapeIn = FALSE );
    int                  OpenView( const char *pszViewName,
                                   const char *pszViewGeometry,
                                   const char *pszViewRowid,
                                   const char *pszTableName,
                                   const char *pszGeometryColumn,
                                   int bSpatialiteLoaded);

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    
    virtual OGRLayer    *CreateLayer( const char *pszLayerName, 
                                      OGRSpatialReference *poSRS, 
                                      OGRwkbGeometryType eType, 
                                      char **papszOptions );
    virtual OGRErr      DeleteLayer(int);

    int                 TestCapability( const char * );

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

    void                SetName(const char* pszNameIn);

    void                NotifyFileOpened (const char* pszFilename,
                                          VSILFILE* fp);
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

CPLString OGRSQLiteEscape( const char *pszSrcName );
int OGRSQLiteGetSpatialiteVersionNumber();
#ifdef HAVE_SQLITE_VFS
typedef void (*pfnNotifyFileOpenedType)(void* pfnUserData, const char* pszFilename, VSILFILE* fp);
sqlite3_vfs* OGRSQLiteCreateVFS(pfnNotifyFileOpenedType pfn, void* pfnUserData);
#endif

#endif /* ndef _OGR_SQLITE_H_INCLUDED */


