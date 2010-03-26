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

/* When used with Spatialite amalgation, there might be no sqlite3 headers */
/* in other places than /include/spatialite/ subdir */
#ifdef HAVE_SPATIALITE
#include <spatialite/sqlite3.h>
#else
#include "sqlite3.h"
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
                                                     int* pnBytesConsumed);

    static int          ComputeSpatiaLiteGeometrySize(const OGRGeometry *poGeometry);
    static int          ExportSpatiaLiteGeometryInternal(const OGRGeometry *poGeometry,
                                                        OGRwkbByteOrder eByteOrder,
                                                        GByte* pabyData);

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

    CPLErr              BuildFeatureDefn( const char *pszLayerName, 
                                          sqlite3_stmt *hStmt );

    virtual void	ClearStatement() = 0;

    static OGRErr       ImportSpatiaLiteGeometry( const GByte *, int,
                                                  OGRGeometry ** );
    static OGRErr       ExportSpatiaLiteGeometry( const OGRGeometry *,
                                                  GInt32, OGRwkbByteOrder,
                                                  GByte **, int * );

  public:
                        OGRSQLiteLayer();
    virtual             ~OGRSQLiteLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual int         TestCapability( const char * );

    virtual sqlite3_stmt        *GetStatement() { return hStmt; }

    virtual OGRErr       StartTransaction();
    virtual OGRErr       CommitTransaction();
    virtual OGRErr       RollbackTransaction();
};

/************************************************************************/
/*                         OGRSQLiteTableLayer                          */
/************************************************************************/

class OGRSQLiteTableLayer : public OGRSQLiteLayer
{
    int                 bUpdateAccess;
    int                 bLaunderColumnNames;

    CPLString           osWHERE;
    CPLString           osQuery;

    virtual void	ClearStatement();
    OGRErr              ResetStatement();
    void                BuildWhere(void);

  public:
                        OGRSQLiteTableLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteTableLayer();

    virtual sqlite3_stmt        *GetStatement();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol,
                                    OGRwkbGeometryType eGeomType,
                                    const char *pszGeomFormat,
                                    OGRSpatialReference *poSRS,
                                    int nSRSId = -1,
                                    int bHasSpatialIndex = FALSE);

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );

    // follow methods are not base class overrides
    void                SetLaunderFlag( int bFlag ) 
                                { bLaunderColumnNames = bFlag; }
};

/************************************************************************/
/*                         OGRSQLiteSelectLayer                         */
/************************************************************************/

class OGRSQLiteSelectLayer : public OGRSQLiteLayer
{
  public:
                        OGRSQLiteSelectLayer( OGRSQLiteDataSource *, 
                                              sqlite3_stmt * );
                        ~OGRSQLiteSelectLayer();

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual int         TestCapability( const char * );

    virtual void	ClearStatement();
};

/************************************************************************/
/*                   OGRSQLiteSingleFeatureLayer                        */
/************************************************************************/

class OGRSQLiteSingleFeatureLayer : public OGRLayer
{
  private:
    int                 nVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGRSQLiteSingleFeatureLayer( const char* pszLayerName,
                                                     int nVal );
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

    int                 nSoftTransactionLevel;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes. 
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;

    int                 bHaveGeometryColumns;
    int                 bIsSpatiaLite;
    
    virtual void        DeleteLayer( const char *pszLayer );

    static OGRwkbGeometryType SpatiaLiteToOGRGeomType( const char * );
    static const char   *OGRToSpatiaLiteGeomType( OGRwkbGeometryType );

  public:
                        OGRSQLiteDataSource();
                        ~OGRSQLiteDataSource();

    int                 Open( const char * );
    int                 OpenTable( const char *pszTableName, 
                                   const char *pszGeomCol = NULL,
                                   OGRwkbGeometryType eGeomType = wkbUnknown,
                                   const char *pszGeomFormat = NULL,
                                   OGRSpatialReference *poSRS = NULL,
                                   int nSRID = -1,
                                   int bHasSpatialIndex = FALSE);

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    
    virtual OGRLayer    *CreateLayer( const char *pszLayerName, 
                                      OGRSpatialReference *poSRS, 
                                      OGRwkbGeometryType eType, 
                                      char **papszOptions );

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
    
    int                 TestCapability( const char * );
};


#endif /* ndef _OGR_SQLITE_H_INCLUDED */


