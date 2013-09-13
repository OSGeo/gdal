/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for Geomedia MDB driver.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_GEOMEDIA_H_INCLUDED
#define _OGR_GEOMEDIA_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"
#include "ogr_pgeo.h"

/************************************************************************/
/*                          OGRGeomediaLayer                            */
/************************************************************************/

class OGRGeomediaDataSource;
    
class OGRGeomediaLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    CPLODBCStatement   *poStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    int                 iNextShapeId;

    OGRGeomediaDataSource    *poDS;

    char                *pszGeomColumn;
    char                *pszFIDColumn;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }

    void                LookupSRID( int );

  public:
                        OGRGeomediaLayer();
    virtual             ~OGRGeomediaLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
    
    virtual int         TestCapability( const char * );

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();
};

/************************************************************************/
/*                       OGRGeomediaTableLayer                          */
/************************************************************************/

class OGRGeomediaTableLayer : public OGRGeomediaLayer
{
    int                 bUpdateAccess;

    char                *pszQuery;

    void		ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement();

  public:
                        OGRGeomediaTableLayer( OGRGeomediaDataSource * );
                        ~OGRGeomediaTableLayer();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol,
                                    OGRSpatialReference* poSRS );

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                        OGRGeomediaSelectLayer                        */
/************************************************************************/

class OGRGeomediaSelectLayer : public OGRGeomediaLayer
{
    char                *pszBaseStatement;

    void		ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement();

  public:
                        OGRGeomediaSelectLayer( OGRGeomediaDataSource *,
                                           CPLODBCStatement * );
                        ~OGRGeomediaSelectLayer();

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                        OGRGeomediaDataSource                         */
/************************************************************************/

class OGRGeomediaDataSource : public OGRDataSource
{
    OGRGeomediaLayer  **papoLayers;
    int                 nLayers;

    OGRGeomediaLayer  **papoLayersInvisible;
    int                 nLayersWithInvisible;

    char               *pszName;

    int                 bDSUpdate;
    CPLODBCSession      oSession;

    CPLString          GetTableNameFromType(const char* pszTableType);
    OGRSpatialReference* GetGeomediaSRS(const char* pszGCoordSystemTable,
                                        const char* pszGCoordSystemGUID);

  public:
                        OGRGeomediaDataSource();
                        ~OGRGeomediaDataSource();

    int                 Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *pszTableName, 
                                   const char *pszGeomCol,
                                   int bUpdate );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    OGRLayer            *GetLayerByName( const char* pszLayerName );

    int                 TestCapability( const char * );

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    // Internal use
    CPLODBCSession     *GetSession() { return &oSession; }
};

/************************************************************************/
/*                          OGRGeomediaDriver                           */
/************************************************************************/

class OGRGeomediaDriver : public OGRODBCMDBDriver
{
  public:
                ~OGRGeomediaDriver();
                
    const char  *GetName();
    OGRDataSource *Open( const char *, int );

    int          TestCapability( const char * );
};

#endif /* ndef _OGR_Geomedia_H_INCLUDED */
