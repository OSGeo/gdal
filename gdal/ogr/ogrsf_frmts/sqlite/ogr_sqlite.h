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
 ****************************************************************************/

#ifndef _OGR_SQLITE_H_INCLUDED
#define _OGR_SQLITE_H_INLLUDED

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include "sqlite3.h"

/************************************************************************/
/*                            OGRSQLiteLayer                            */
/************************************************************************/

class OGRSQLiteDataSource;
    
class OGRSQLiteLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    int                 iNextShapeId;

    sqlite3_stmt        *hStmt;

    OGRSQLiteDataSource *poDS;

    char                *pszGeomColumn;
    char                *pszFIDColumn;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName, 
                                          sqlite3_stmt *hStmt );

    virtual void	ClearStatement() = 0;

  public:
                        OGRSQLiteLayer();
    virtual             ~OGRSQLiteLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

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

    char                *pszQuery;

    virtual void	ClearStatement();
    OGRErr              ResetStatement();

  public:
                        OGRSQLiteTableLayer( OGRSQLiteDataSource * );
                        ~OGRSQLiteTableLayer();

    virtual sqlite3_stmt        *GetStatement();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol );

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

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
    
    virtual void        DeleteLayer( const char *pszLayer );

  public:
                        OGRSQLiteDataSource();
                        ~OGRSQLiteDataSource();

    int                 Open( const char * );
    int                 OpenTable( const char *pszTableName, 
                                   const char *pszGeomCol );

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


