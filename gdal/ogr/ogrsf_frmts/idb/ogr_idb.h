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

#ifndef OGR_IDB_H_INCLUDED_
#define OGR_IDB_H_INCLUDED_

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include "idb_headers.h"

/************************************************************************/
/*                            OGRIDBLayer                              */
/************************************************************************/

class OGRIDBDataSource;

class OGRIDBLayer CPL_NON_FINAL: public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    ITCursor   *m_poCurr;

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

    virtual ITCursor *  GetQuery() { return m_poCurr; }

  public:
                        OGRIDBLayer();
    virtual             ~OGRIDBLayer();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRIDBTableLayer                          */
/************************************************************************/

class OGRIDBTableLayer final: public OGRIDBLayer
{
    int                 bUpdateAccess;

    char                *pszQuery;

    int                 bHaveSpatialExtents;

    void                ClearQuery();
    OGRErr              ResetQuery();

    virtual ITCursor *  GetQuery() override;

  public:
    explicit            OGRIDBTableLayer( OGRIDBDataSource * );
                        virtual ~OGRIDBTableLayer();

    CPLErr              Initialize( const char *pszTableName,
                                    const char *pszGeomCol,
                                    int bUpdate
                                  );

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;

#if 0
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
#endif
    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                          OGRIDBSelectLayer                          */
/************************************************************************/

class OGRIDBSelectLayer final: public OGRIDBLayer
{
    char                *pszBaseQuery;

    void                ClearQuery();
    OGRErr              ResetQuery();

    virtual ITCursor *  GetQuery() override;

  public:
                        OGRIDBSelectLayer( OGRIDBDataSource *,
                                           ITCursor * );
                        virtual ~OGRIDBSelectLayer();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRIDBDataSource                          */
/************************************************************************/

class OGRIDBDataSource final: public OGRDataSource
{
    OGRIDBLayer        **papoLayers;
    int                 nLayers;

    char               *pszName;

    int                 bDSUpdate;
    ITConnection       *poConn;

  public:
                        OGRIDBDataSource();
                        virtual ~OGRIDBDataSource();

    int                 Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *pszTableName,
                                   const char *pszGeomCol,
                                   int bUpdate );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    // Internal use
    ITConnection *      GetConnection() { return poConn; }
};

/************************************************************************/
/*                             OGRIDBDriver                            */
/************************************************************************/

class OGRIDBDriver final: public OGRSFDriver
{
    public:
                       ~OGRIDBDriver();
        const char *    GetName() override;
        OGRDataSource * Open( const char *, int ) override;

        OGRDataSource * CreateDataSource( const char *pszName,
                                          char ** = NULL ) override;

        int             TestCapability( const char * ) override;
};

ITCallbackResult
IDBErrorHandler( const ITErrorManager &err, void * userdata, long errorlevel );

#endif /* ndef _OGR_idb_H_INCLUDED_ */
