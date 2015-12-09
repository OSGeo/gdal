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
#include "gdal_pam.h"
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
/*      Sqlite3 Database Container Types supported                                    */
/*      - OGRSQLiteDataSource::Open:                                                                */
/*      - should determin which type (and possible version)                                  */
/*      - store this value in  OGRSQLiteEditableLayer                                          */
/*      - with switch on this value,  OGRSQLiteEditableLayer should deal with   */
/*         Layer-Type and verson specific tasks                                                     */
/*  Note: Rasterlite2_Tables and SpatialTopology_Tables:                        */
/* - are treated as SpatialTable_4                                            */
/* -- but their Administration TABLES/VIEWS will not be listed              */
/* -- as Geometry-Tables in  OGRSQLiteDataSource::Open             */
/************************************************************************/
enum OGRSQLiteDatabaseType
{
    OSDBT_Unknown = 0,
    OSDBT_OGRSpatialTable = 1,
    OSDBT_SpatialTable_2 = 11,
    OSDBT_SpatialTable_3 = 12,
    OSDBT_SpatialTable_4 = 13,
    OSDBT_Rasterlite1_Tables = 91,
     /* admin tables for RasterLite2-Rasters, otherwise OSVLT_SpatialTable_4 */
    OSDBT_Rasterlite2_Tables = 101,
    OSDBT_SpatialTopology_Tables = 201,
    OSDBT_SpatialTopology_Networks = 203,
    OSDBT_GeoPackage_Tables = 301,
    /* if one day directly supported, View/Table logic is different*/
    OSDBT_MBTiles_Views = 400,
    OSDBT_MBTiles_Tables = 401,
};
/************************************************************************/
/*      OGR and SpatiaLite Layer Types supported                                    */
/*      - OGRSQLiteDataSource::Open:                                                                */
/*      - should determin which type (and possible version)                                  */
/*      - store this value in  OGRSQLiteEditableLayer                                          */
/*      - with switch on this value,  OGRSQLiteEditableLayer should deal with   */
/*         Layer-Type and verson specific tasks                                                     */
/*  Note: Rasterlite2_Tables and SpatialTopology_Tables/Views:                */
/* - will be delt with in extra Administration classes                        */
/************************************************************************/
enum OGRSQLiteLayerType
{
    OSLLT_Unknown = 0,
    OSLLT_OGRSpatialTable = 1,
    OSLLT_OGRSpatialView = 2,
    OSLLT_OGRVirtualTable = 3,    
    OSLLT_SpatialTable_2 = 11,
    OSLLT_SpatialTable_3 = 12,
    OSLLT_SpatialTable_4 = 13,
    OSLLT_SpatialView_2 = 14,
    OSLLT_SpatialView_3 = 15,
    OSLLT_SpatialView_4 =16,
    OSLLT_SpatialVirtualShape_2=20,
    OSLLT_SpatialVirtualShape_3=21,
    OSLLT_SpatialVirtualShape_4=22,
    OSLLT_SpatialVirtualXL_3=30,
    OSLLT_SpatialVirtualXL_4=31,
    OSLLT_Rasterlite1_Raster = 90,
    /* admin tables for RasterLite1-Rasters, otherwise treated as OSLLT_SpatialTable_3 */
    OSLLT_Rasterlite1_Table = 91,
    OSLLT_Rasterlite2_Raster = 100,
    /* admin tables for RasterLite2-Rasters, otherwise  treated as OSLLT_SpatialTable_4 */
    OSLLT_Rasterlite2_Table = 101,
    /* admin tables for Spatialite-Topologies, otherwise  treated as OSLLT_SpatialTable_4 */
    OSLLT_SpatialTopology_Table= 201,
    /* admin tables for Spatialite-Topologies, otherwise  treated as OSLLT_SpatialView_4 */
    OSLLT_SpatialTopology_View = 202,
    /* admin tables for Spatialite-Topologies, otherwise  treated as OSLLT_SpatialTable_4 */
    OSLLT_SpatialTopology_Network = 203,
};

/************************************************************************/
/*                        OGRSQLiteGeomFieldDefn                        */
/************************************************************************/

class OGRSQLiteGeomFieldDefn : public OGRGeomFieldDefn
{
    public:
        OGRSQLiteGeomFieldDefn( const char* pszName, int iGeomColIn ) :
            OGRGeomFieldDefn(pszName, wkbUnknown), nSRSId(-1),
            iCol(iGeomColIn), bTriedAsSpatiaLite(FALSE), eGeomFormat(OSGF_None),
            bHasM(FALSE), bCachedExtentIsValid(FALSE), bHasSpatialIndex(FALSE),
            bHasCheckedSpatialIndexTable(FALSE)
            {
            }

        int nSRSId;
        int iCol; /* ordinal of geometry field in SQL statement */
        int bTriedAsSpatiaLite;
        OGRSQLiteGeomFormat eGeomFormat;
        int bHasM;
        OGREnvelope         oCachedExtent;
        int                 bCachedExtentIsValid;
        int                 bHasSpatialIndex;
        int                 bHasCheckedSpatialIndexTable;
        std::vector< std::pair<CPLString,CPLString> > aosDisabledTriggers;
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

class IOGRSQLiteGetSpatialWhere
{
  public:
    virtual              ~IOGRSQLiteGetSpatialWhere() {}

    virtual int           HasFastSpatialFilter(int iGeomCol) = 0;
    virtual CPLString     GetSpatialWhere(int iGeomCol,
                                          OGRGeometry* poFilterGeom) = 0;
};

class OGRSQLiteLayer : public OGRLayer, public IOGRSQLiteGetSpatialWhere
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

    GIntBig             iNextShapeId;

    sqlite3_stmt        *hStmt;
    int                  bDoStep;

    OGRSQLiteDataSource *poDS;

    char                *pszFIDColumn;

    int                *panFieldOrdinals;
    int                 iFIDCol;
    int                 iOGRNativeDataCol;
    int                 iOGRNativeMediaTypeCol;

    int                 bIsVirtualShape;

    void                BuildFeatureDefn( const char *pszLayerName,
                                          sqlite3_stmt *hStmt,
                                          const std::set<CPLString>& aosGeomCols,
                                          const std::set<CPLString>& aosIgnoredCols);

    void                ClearStatement();
    virtual OGRErr      ResetStatement() = 0;

    int                 bUseComprGeom;

    char              **papszCompressedColumns;

    int                 bAllowMultipleGeomFields;

    CPLString           FormatSpatialFilterFromRTree(OGRGeometry* poFilterGeom,
                                                       const char* pszRowIDName,
                                                       const char* pszEscapedTable,
                                                       const char* pszEscapedGeomCol);
    CPLString           FormatSpatialFilterFromMBR(OGRGeometry* poFilterGeom,
                                                   const char* pszEscapedGeomColName);


  public:
                        OGRSQLiteLayer();
    virtual             ~OGRSQLiteLayer();
    static OGRSQLiteDatabaseType GetSQLiteDatabaseType(OGRSQLiteDataSource *poDS);
    virtual void        Finalize();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( GIntBig nFeatureId );
    
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
    virtual OGRSQLiteFeatureDefn *myGetLayerDefn() { return poFeatureDefn; }

    virtual const char *GetFIDColumn();

    virtual int         TestCapability( const char * );

    virtual OGRErr       StartTransaction();
    virtual OGRErr       CommitTransaction();
    virtual OGRErr       RollbackTransaction();

    virtual void        InvalidateCachedFeatureCountAndExtent() { }

    virtual int          IsTableLayer() { return FALSE; }
    virtual int          IsViewLayer() { return FALSE; }

    virtual int          HasSpatialIndex(CPL_UNUSED int iGeomField) { return FALSE; }

    virtual int          HasFastSpatialFilter(CPL_UNUSED int iGeomCol) { return FALSE; }
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
/*                         OGRSQLiteEditableLayer                          */
/************************************************************************/
class OGRSQLiteEditableLayer : public OGRSQLiteLayer
{
  protected:
    OGRSQLiteLayerType eSQLiteLayerType;
    int                 bLaunderColumnNames;
    int                 bSpatialite2D;

    CPLString           osWHERE;
    CPLString           osQuery;
    int                 bDeferredSpatialIndexCreation;

    char               *pszTableName;
    char               *pszEscapedTableName;
    char               *pszEscapedUnderlyingTableName;
    virtual const char     *GetGeometryTable(){return pszEscapedTableName;};
    // for normal tables '_rowid_' can be used
    // - for SpatialViews: the primary key of the view - as defined in  views_geometry_columns must be used
    virtual const char     *GetEscapedRowId() {return "_rowid_";};
    CPLString           osUnderlyingTableName;
    CPLString           osUnderlyingGeometryColumn;
    int                 bHasCheckedSpatialIndexTable;

    OGRSQLiteGeomFormat eGeomFormat;
    CPLString           osGeomColumn;
    int                 bHasSpatialIndex;

    int                 bLayerDefnError;

    sqlite3_stmt       *hInsertStmt;
    CPLString           osLastInsertStmt;
    int                 bHasDefaultValue;

    int                 bHasCheckedTriggers;

    void                ClearInsertStmt();

    void                BuildWhere(void);

    virtual OGRErr      ResetStatement();
    // added for SpatialView - writable
    int                 bTriggerInsert;
    int                 bTriggerUpdate;
    int                 bTriggerDelete;

    OGRErr              RecomputeOrdinals();

    OGRErr              AddColumnAncientMethod( OGRFieldDefn& oField);
    void                AddColumnDef(char* pszNewFieldList,
                                     OGRFieldDefn* poFldDefn);

    void                InitFieldListForRecrerate(char* & pszNewFieldList,
                                                  char* & pszFieldListForSelect,
                                                  int nExtraSpace = 0);
    OGRErr              RecreateTable(const char* pszFieldListForSelect,
                                      const char* pszNewFieldList,
                                      const char* pszGenericErrorMessage);
    OGRErr              BindValues( OGRFeature *poFeature,
                                        sqlite3_stmt* hStmt,
                                        int bBindNullValues );

    int                 CheckSpatialIndexTable(int iGeomCol);

    CPLErr              EstablishFeatureDefn(const char* pszGeomCol);

    int                 bStatisticsNeedsToBeFlushed;
    GIntBig             nFeatureCount; /* if -1, means not up-to-date */

    void                LoadStatistics();
    void                LoadStatisticsSpatialite4DB();

    CPLString           FieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn );
    
    int                 bDeferredCreation;
    OGRErr              RunAddGeometryColumn( OGRSQLiteGeomFieldDefn *poGeomField,
                                              int bAddColumnsForNonSpatialite );

    char               *pszCreationGeomFormat;    
    int                 iFIDAsRegularColumnIndex; 

  public:
                        OGRSQLiteEditableLayer();
                        ~OGRSQLiteEditableLayer();

    CPLErr              Initialize( const char *pszTableName, 
                                    OGRSQLiteLayerType eSQLiteLayerType,
                                    int bDeferredCreation = FALSE );
    static OGRSQLiteLayerType GetSpatialiteLayerType(OGRSQLiteDataSource *poDS,
                                        const char *pszTableName=NULL, 
                                        OGRSQLiteDatabaseType eDatabaseType=OSDBT_Unknown);
    void                SetCreationParameters( const char *pszFIDColumnName,
                                               OGRwkbGeometryType eGeomType,
                                               const char *pszGeomFormat,
                                               const char *pszGeometryName,
                                               OGRSpatialReference *poSRS,
                                               int nSRSId );
    virtual const char* GetName();

    virtual GIntBig     GetFeatureCount( int );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce);
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce);

    virtual OGRFeatureDefn *GetLayerDefn();
    int                 HasLayerDefnError() { GetLayerDefn(); return bLayerDefnError; }

    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( GIntBig nFID );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                         int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual OGRFeature *GetNextFeature();
    virtual OGRFeature *GetFeature( GIntBig nFeatureId );

    virtual int         TestCapability( const char * );

    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );

    // follow methods are not base class overrides
    void                SetLaunderFlag( int bFlag ) 
                                { bLaunderColumnNames = bFlag; }
    void                SetUseCompressGeom( int bFlag )
                                { bUseComprGeom = bFlag; }
    void                SetDeferredSpatialIndexCreation( int bFlag )
                                { bDeferredSpatialIndexCreation = bFlag; }
    void                SetCompressedColumns( const char* pszCompressedColumns );

    void                InitFeatureCount();
    int                 DoStatisticsNeedToBeFlushed();
    void                ForceStatisticsToBeFlushed();
    int                 AreStatisticsValid();
    int                 SaveStatistics();

    virtual void        InvalidateCachedFeatureCountAndExtent();

    virtual int          HasSpatialIndex(int iGeomField);
    virtual int          HasFastSpatialFilter(int iGeomCol);
    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom);
    // Table specific functions
    virtual void        CreateSpatialIndexIfNecessary() {return;};
    virtual int         CreateSpatialIndex(CPL_UNUSED int iGeomCol) {return OGRERR_NONE;};
    virtual OGRErr      RunDeferredCreationIfNecessary() {return OGRERR_NONE;};
};

/************************************************************************/
/*                         OGRSQLiteTableLayer                          */
/************************************************************************/

class OGRSQLiteTableLayer : public OGRSQLiteEditableLayer
{
  public:
                        OGRSQLiteTableLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteTableLayer();
                        
    CPLErr              Initialize( const char *pszTableName, 
                                    OGRSQLiteLayerType eSQLiteLayerType,
                                    int bDeferredCreation = FALSE );      
    virtual int          IsTableLayer() { return TRUE; }   
    virtual void        CreateSpatialIndexIfNecessary();
    virtual int         CreateSpatialIndex(int iGeomCol);
    virtual OGRErr      RunDeferredCreationIfNecessary();                    
};

/************************************************************************/
/*                         OGRSpatialViewLayer                           */
/************************************************************************/
class OGRSQLiteViewLayer : public OGRSQLiteEditableLayer
{
    virtual const char     *GetGeometryTable();
    virtual const char     *GetEscapedRowId();
    virtual int            IsViewLayer() { return TRUE; };
  public:
                        OGRSQLiteViewLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteViewLayer();
  
    CPLErr              Initialize( const char *pszViewName, 
                                    OGRSQLiteLayerType eSQLiteLayerType,
                                    int bDeferredCreation = FALSE );
};

/************************************************************************/
/*                         IOGRSQLiteSelectLayer                        */
/************************************************************************/

class IOGRSQLiteSelectLayer
{
    public:
        virtual                     ~IOGRSQLiteSelectLayer() {}

        virtual char*&               GetAttrQueryString() = 0;
        virtual OGRFeatureQuery*&    GetFeatureQuery() = 0;
        virtual OGRGeometry*&        GetFilterGeom() = 0;
        virtual int&                 GetIGeomFieldFilter() = 0;
        virtual OGRSpatialReference* GetSpatialRef() = 0;
        virtual OGRFeatureDefn      *GetLayerDefn() = 0;
        virtual int                  InstallFilter( OGRGeometry * ) = 0;
        virtual int                  HasReadFeature() = 0;
        virtual void                 BaseResetReading() = 0;
        virtual OGRFeature          *BaseGetNextFeature() = 0;
        virtual OGRErr               BaseSetAttributeFilter(const char* pszQuery) = 0;
        virtual GIntBig              BaseGetFeatureCount(int bForce) = 0;
        virtual int                  BaseTestCapability( const char * ) = 0;
        virtual OGRErr               BaseGetExtent(OGREnvelope *psExtent, int bForce) = 0;
        virtual OGRErr               BaseGetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) = 0;
};

/************************************************************************/
/*                   OGRSQLiteSelectLayerCommonBehaviour                */
/************************************************************************/

class OGRSQLiteBaseDataSource;
class OGRSQLiteSelectLayerCommonBehaviour
{
    OGRSQLiteBaseDataSource *poDS;
    IOGRSQLiteSelectLayer   *poLayer;

    CPLString           osSQLBase;

    int                 bEmptyLayer;
    int                 bAllowResetReadingEvenIfIndexAtZero;
    int                 bSpatialFilterInSQL;

    std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetBaseLayer(size_t& i);
    int                 BuildSQL();

  public:

    CPLString           osSQLCurrent;

        OGRSQLiteSelectLayerCommonBehaviour(OGRSQLiteBaseDataSource* poDS,
                                            IOGRSQLiteSelectLayer* poBaseLayer,
                                            CPLString osSQL,
                                            int bEmptyLayer);

    void        ResetReading();
    OGRFeature *GetNextFeature();
    GIntBig     GetFeatureCount( int );
    void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    OGRErr      SetAttributeFilter( const char * );
    int         TestCapability( const char * );
    OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce);
};

/************************************************************************/
/*                         OGRSQLiteSelectLayer                         */
/************************************************************************/

class OGRSQLiteSelectLayer : public OGRSQLiteLayer, public IOGRSQLiteSelectLayer
{
    OGRSQLiteSelectLayerCommonBehaviour* poBehaviour;

    virtual OGRErr      ResetStatement();
 
  public:
                        OGRSQLiteSelectLayer( OGRSQLiteDataSource *, 
                                              CPLString osSQL,
                                              sqlite3_stmt *,
                                              int bUseStatementForGetNextFeature,
                                              int bEmptyLayer,
                                              int bAllowMultipleGeomFields );
                       ~OGRSQLiteSelectLayer();

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();
    virtual GIntBig     GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);

    virtual OGRFeatureDefn *     GetLayerDefn() { return OGRSQLiteLayer::GetLayerDefn(); }
    virtual char*&               GetAttrQueryString() { return m_pszAttrQueryString; }
    virtual OGRFeatureQuery*&    GetFeatureQuery() { return m_poAttrQuery; }
    virtual OGRGeometry*&        GetFilterGeom() { return m_poFilterGeom; }
    virtual int&                 GetIGeomFieldFilter() { return m_iGeomFieldFilter; }
    virtual OGRSpatialReference* GetSpatialRef() { return OGRSQLiteLayer::GetSpatialRef(); }
    virtual int                  InstallFilter( OGRGeometry * poGeomIn ) { return OGRSQLiteLayer::InstallFilter(poGeomIn); }
    virtual int                  HasReadFeature() { return iNextShapeId > 0; }
    virtual void                 BaseResetReading() { OGRSQLiteLayer::ResetReading(); }
    virtual OGRFeature          *BaseGetNextFeature() { return OGRSQLiteLayer::GetNextFeature(); }
    virtual OGRErr               BaseSetAttributeFilter(const char* pszQuery) { return OGRSQLiteLayer::SetAttributeFilter(pszQuery); }
    virtual GIntBig              BaseGetFeatureCount(int bForce) { return OGRSQLiteLayer::GetFeatureCount(bForce); }
    virtual int                  BaseTestCapability( const char *pszCap ) { return OGRSQLiteLayer::TestCapability(pszCap); }
    virtual OGRErr               BaseGetExtent(OGREnvelope *psExtent, int bForce) { return OGRSQLiteLayer::GetExtent(psExtent, bForce); }
    virtual OGRErr               BaseGetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) { return OGRSQLiteLayer::GetExtent(iGeomField, psExtent, bForce); }
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
/*                       OGRSQLiteBaseDataSource                        */
/************************************************************************/

/* Used by both OGRSQLiteDataSource and OGRGeoPackageDataSource */
class OGRSQLiteBaseDataSource : public GDALPamDataset
{
  protected:
    char               *m_pszFilename;

    sqlite3             *hDB;
    int                 bUpdate;

#ifdef HAVE_SQLITE_VFS
    sqlite3_vfs*        pMyVFS;
#endif

    VSILFILE*           fpMainFile; /* Set by the VFS layer when it opens the DB */
                                    /* Must *NOT* be closed by the datasource explicitly. */

    int                 OpenOrCreateDB(int flags, int bRegisterOGR2SQLiteExtensions);
    int                 SetSynchronous();
    int                 SetCacheSize();

    void                CloseDB();

    std::map<CPLString, OGREnvelope> oMapSQLEnvelope;

#ifdef SPATIALITE_412_OR_LATER
    void               *hSpatialiteCtxt;
    int                 InitNewSpatialite();
    void                FinishNewSpatialite();
#endif

    int                 bUserTransactionActive;
    int                 nSoftTransactionLevel;
    
    OGRErr              DoTransactionCommand(const char* pszCommand);

  public:
                        OGRSQLiteBaseDataSource();
                        ~OGRSQLiteBaseDataSource();

    sqlite3            *GetDB() { return hDB; }
    int                 GetUpdate() const { return bUpdate; }

    void                NotifyFileOpened (const char* pszFilename,
                                          VSILFILE* fp);

    const OGREnvelope*  GetEnvelopeFromSQL(const CPLString& osSQL);
    void                SetEnvelopeForSQL(const CPLString& osSQL, const OGREnvelope& oEnvelope);

    virtual std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetLayerWithGetSpatialWhereByName( const char* pszName ) = 0;

    virtual OGRErr      StartTransaction(int bForce = FALSE);
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();
    
    virtual int         TestCapability( const char * );

    OGRErr              SoftStartTransaction();
    OGRErr              SoftCommitTransaction();
    OGRErr              SoftRollbackTransaction();
};

/************************************************************************/
/*                         OGRSQLiteDataSource                          */
/************************************************************************/

class OGRSQLiteDataSource : public OGRSQLiteBaseDataSource
{
    OGRSQLiteLayer    **papoLayers;
    int                 nLayers;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes. 
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;

    char              **papszOpenOptions;

    void                AddSRIDToCache(int nId, OGRSpatialReference * poSRS );

    int                 bHaveGeometryColumns;
    int                 bIsSpatiaLiteDB;
    int                 bSpatialite4Layout;

    int                 nUndefinedSRID;

    virtual void        DeleteLayer( const char *pszLayer );

    const char*         GetSRTEXTColName();

    int                 InitWithEPSG();

    int                 OpenVirtualTable(const char* pszName,OGRSQLiteLayerType eSQLiteLayerTypeTable, const char* pszSQL);

    GIntBig             nFileTimestamp;
    int                 bLastSQLCommandIsUpdateLayerStatistics;

    std::map< CPLString, std::set<CPLString> > aoMapTableToSetOfGeomCols;

    void                SaveStatistics();
    OGRSQLiteDatabaseType eDatabaseType;

  public:
                        OGRSQLiteDataSource();
                        ~OGRSQLiteDataSource();

    int                 Open( const char *, int bUpdateIn, char** papszOpenOptions );
    int                 Create( const char *, char **papszOptions );
    
    int                 OpenTable( const char *pszTableName, 
                                   OGRSQLiteLayerType eSQLiteLayerTypeTable = OSLLT_OGRSpatialTable);
    int                 OpenView( const char *pszViewName, 
                                   OGRSQLiteLayerType eSQLiteLayerTypeView = OSLLT_OGRSpatialView);    
                                  
   OGRSQLiteDatabaseType  GetDatabaseType() const { return eDatabaseType; }

    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer   *GetLayer( int );
    virtual OGRLayer   *GetLayerByName( const char* );
    virtual std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetLayerWithGetSpatialWhereByName( const char* pszName );

    virtual OGRLayer    *ICreateLayer( const char *pszLayerName, 
                                      OGRSpatialReference *poSRS, 
                                      OGRwkbGeometryType eType, 
                                      char **papszOptions );
    virtual OGRErr      DeleteLayer(int);

    virtual int         TestCapability( const char * );

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    virtual void        FlushCache();
    
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    char               *LaunderName( const char * );
    int                 FetchSRSId( OGRSpatialReference * poSRS );
    OGRSpatialReference*FetchSRS( int nSRID );

    void                SetUpdate(int bUpdateIn) { bUpdate = bUpdateIn; }

    void                SetName(const char* pszNameIn);

    const std::set<CPLString>& GetGeomColsForTable(const char* pszTableName)
            { return aoMapTableToSetOfGeomCols[pszTableName]; }

    GIntBig             GetFileTimestamp() const { return nFileTimestamp; }

    int                 IsSpatialiteLoaded();
    int                 GetSpatialiteVersionNumber();

    int                 IsSpatialiteDB() const { return bIsSpatiaLiteDB; }
    int                 HasSpatialite4Layout() const { return bSpatialite4Layout; }

    int                 GetUndefinedSRID() const { return nUndefinedSRID; }
    int                 HasGeometryColumns() const { return bHaveGeometryColumns; }

    void                ReloadLayers();
};

/* To escape literals. The returned string doesn't contain the surrounding single quotes */
CPLString OGRSQLiteEscape( const char *pszLiteral );

/* To escape table or field names. The returned string doesn't contain the surrounding double quotes */
CPLString OGRSQLiteEscapeName( const char* pszName );

CPLString OGRSQLiteParamsUnquote(const char* pszVal);

CPLString OGRSQLiteFieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn,
                                               int bSQLiteDialectInternalUse );

int OGRSQLITEStringToDateTimeField( OGRFeature* poFeature, int iField,
                                    const char* pszValue );

#ifdef HAVE_SQLITE_VFS
typedef void (*pfnNotifyFileOpenedType)(void* pfnUserData, const char* pszFilename, VSILFILE* fp);
sqlite3_vfs* OGRSQLiteCreateVFS(pfnNotifyFileOpenedType pfn, void* pfnUserData);
#endif

void OGRSQLiteRegisterInflateDeflate(sqlite3* hDB);

#endif /* ndef _OGR_SQLITE_H_INCLUDED */
