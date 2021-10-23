/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for Geomedia MDB driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GEOMEDIA_H_INCLUDED
#define OGR_GEOMEDIA_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"
#include "ogr_pgeo.h"

/************************************************************************/
/*                          OGRGeomediaLayer                            */
/************************************************************************/

class OGRGeomediaDataSource;

class OGRGeomediaLayer CPL_NON_FINAL: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRGeomediaLayer>
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    CPLODBCStatement   *poStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    GIntBig             iNextShapeId;

    OGRGeomediaDataSource    *poDS;

    char                *pszGeomColumn;
    char                *pszFIDColumn;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }

    void                LookupSRID( int );
    OGRFeature          *GetNextRawFeature();

  public:
                        OGRGeomediaLayer();
    virtual             ~OGRGeomediaLayer();

    virtual void        ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRGeomediaLayer)

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int         TestCapability( const char * ) override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;
};

/************************************************************************/
/*                       OGRGeomediaTableLayer                          */
/************************************************************************/

class OGRGeomediaTableLayer final: public OGRGeomediaLayer
{
    char                *pszQuery;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

  public:
    explicit            OGRGeomediaTableLayer( OGRGeomediaDataSource * );
                        virtual ~OGRGeomediaTableLayer();

    CPLErr              Initialize( const char *pszTableName,
                                    const char *pszGeomCol,
                                    OGRSpatialReference* poSRS );

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;
    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                        OGRGeomediaSelectLayer                        */
/************************************************************************/

class OGRGeomediaSelectLayer final: public OGRGeomediaLayer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *GetStatement() override;

  public:
                        OGRGeomediaSelectLayer( OGRGeomediaDataSource *,
                                                CPLODBCStatement * );
                        virtual ~OGRGeomediaSelectLayer();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                        OGRGeomediaDataSource                         */
/************************************************************************/

class OGRGeomediaDataSource final: public OGRDataSource
{
    OGRGeomediaLayer  **papoLayers;
    int                 nLayers;

    OGRGeomediaLayer  **papoLayersInvisible;
    int                 nLayersWithInvisible;

    char               *pszName;

    CPLODBCSession      oSession;

    CPLString          GetTableNameFromType(const char* pszTableType);
    OGRSpatialReference* GetGeomediaSRS(const char* pszGCoordSystemTable,
                                        const char* pszGCoordSystemGUID);

  public:
                        OGRGeomediaDataSource();
                        virtual ~OGRGeomediaDataSource();

    int                 Open( const char * );
    int                 OpenTable( const char *pszTableName,
                                   const char *pszGeomCol,
                                   int bUpdate );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;
    OGRLayer            *GetLayerByName( const char* pszLayerName ) override;

    int                 TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    // Internal use
    CPLODBCSession     *GetSession() { return &oSession; }
};

#endif /* ndef _OGR_Geomedia_H_INCLUDED */
