/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBTableLayer class, access to an existing table
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
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

#ifndef _OGR_IDB_H_INCLUDED_
#define _OGR_IDB_H_INCLUDED_

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include <it.h>

/************************************************************************/
/*                            OGRIDBLayer                              */
/************************************************************************/

class OGRIDBDataSource;

class OGRIDBLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    ITCursor   *poCurr;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    int                 iNextShapeId;

    OGRIDBDataSource    *poDS;

    int                 bGeomColumnWKB;
    char                *pszGeomColumn;
    char                *pszFIDColumn;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          ITCursor *poCurr );

    virtual ITCursor *  GetQuery() { return poCurr; }

  public:
                        OGRIDBLayer();
    virtual             ~OGRIDBLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                           OGRIDBTableLayer                          */
/************************************************************************/

class OGRIDBTableLayer : public OGRIDBLayer
{
    int                 bUpdateAccess;

    char                *pszQuery;

    int                 bHaveSpatialExtents;

    void                ClearQuery();
    OGRErr              ResetQuery();

    virtual ITCursor *  GetQuery();

  public:
                        OGRIDBTableLayer( OGRIDBDataSource * );
                        ~OGRIDBTableLayer();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol,
                                    int bUpdate
                                  );

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );

#if 0
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
#endif    
    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                          OGRIDBSelectLayer                          */
/************************************************************************/

class OGRIDBSelectLayer : public OGRIDBLayer
{
    char                *pszBaseQuery;

    void                ClearQuery();
    OGRErr              ResetQuery();

    virtual ITCursor *  GetQuery();

  public:
                        OGRIDBSelectLayer( OGRIDBDataSource *,
                                           ITCursor * );
                        ~OGRIDBSelectLayer();

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                           OGRIDBDataSource                          */
/************************************************************************/

class OGRIDBDataSource : public OGRDataSource
{
    OGRIDBLayer        **papoLayers;
    int                 nLayers;

    char               *pszName;

    int                 bDSUpdate;
    ITConnection       *poConn;

  public:
                        OGRIDBDataSource();
                        ~OGRIDBDataSource();

    int                 Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *pszTableName, 
                                   const char *pszGeomCol,
                                   int bUpdate );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    // Internal use
    ITConnection *      GetConnection() { return poConn; }
};

/************************************************************************/
/*                             OGRIDBDriver                            */
/************************************************************************/

class OGRIDBDriver : public OGRSFDriver
{
    public:
                       ~OGRIDBDriver();
        const char *    GetName();
        OGRDataSource * Open( const char *, int );

        OGRDataSource * CreateDataSource( const char *pszName,
                                          char ** = NULL );

        int             TestCapability( const char * );
};

ITCallbackResult
IDBErrorHandler( const ITErrorManager &err, void * userdata, long errorlevel );

#endif /* ndef _OGR_idb_H_INCLUDED_ */


