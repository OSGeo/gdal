/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_ODBC_H_INCLUDED
#define OGR_ODBC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"

/************************************************************************/
/*                            OGRPGeoLayer                              */
/************************************************************************/

class OGRPGeoDataSource;

class OGRPGeoLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    CPLODBCStatement   *poStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    GIntBig             iNextShapeId;

    OGRPGeoDataSource    *poDS;

    char                *pszGeomColumn;
    char                *pszFIDColumn;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }

    void                LookupSRID( int );

  public:
                        OGRPGeoLayer();
    virtual             ~OGRPGeoLayer();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int         TestCapability( const char * ) override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;
};

/************************************************************************/
/*                           OGRPGeoTableLayer                            */
/************************************************************************/

class OGRPGeoTableLayer : public OGRPGeoLayer
{
    char                *pszQuery;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

    OGREnvelope         sExtent;

  public:
    explicit            OGRPGeoTableLayer( OGRPGeoDataSource * );
                        virtual ~OGRPGeoTableLayer();

    CPLErr              Initialize( const char *pszTableName,
                                    const char *pszGeomCol,
                                    int nShapeType,
                                    double dfExtentLeft,
                                    double dfExtentRight,
                                    double dfExtentBottom,
                                    double dfExtentTop,
                                    int nSRID,
                                    int bHasZ );

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;
    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
};

/************************************************************************/
/*                          OGRPGeoSelectLayer                          */
/************************************************************************/

class OGRPGeoSelectLayer : public OGRPGeoLayer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

  public:
                        OGRPGeoSelectLayer( OGRPGeoDataSource *,
                                           CPLODBCStatement * );
                        virtual ~OGRPGeoSelectLayer();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRPGeoDataSource                            */
/************************************************************************/

class OGRPGeoDataSource : public OGRDataSource
{
    OGRPGeoLayer        **papoLayers;
    int                 nLayers;

    char               *pszName;

    int                 bDSUpdate;
    CPLODBCSession      oSession;

  public:
                        OGRPGeoDataSource();
                        virtual ~OGRPGeoDataSource();

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
    CPLODBCSession     *GetSession() { return &oSession; }
};

/************************************************************************/
/*                           OGRODBCMDBDriver                           */
/************************************************************************/

class OGRODBCMDBDriver : public OGRSFDriver
{
#ifndef WIN32
    CPLString   osDriverFile;
    static bool        LibraryExists( const char* pszLibPath );
    bool        FindDriverLib();
    CPLString   FindDefaultLib(const char* pszLibName);
#endif

protected:
#ifndef WIN32
    bool        InstallMdbDriver();
#endif
};

/************************************************************************/
/*                             OGRPGeoDriver                            */
/************************************************************************/

class OGRPGeoDriver : public OGRODBCMDBDriver
{
  public:
                ~OGRPGeoDriver();

    const char  *GetName() override;
    OGRDataSource *Open( const char *, int ) override;

    int          TestCapability( const char * ) override;
};

#endif /* ndef _OGR_PGeo_H_INCLUDED */
