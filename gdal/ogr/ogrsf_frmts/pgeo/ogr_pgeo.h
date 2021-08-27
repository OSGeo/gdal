/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

class OGRPGeoLayer CPL_NON_FINAL: public OGRLayer
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

    bool                m_bEOF = false;

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

class OGRPGeoTableLayer final: public OGRPGeoLayer
{
    char                *pszQuery;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

    OGREnvelope         sExtent;
    std::string         m_osDefinition;
    std::string         m_osDocumentation;

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
                                    int bHasZ,
                                    int nHasM );

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;
    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
    const std::string&  GetXMLDefinition() { return m_osDefinition; }
    const std::string&  GetXMLDocumentation() { return m_osDocumentation; }
};

/************************************************************************/
/*                          OGRPGeoSelectLayer                          */
/************************************************************************/

class OGRPGeoSelectLayer final: public OGRPGeoLayer
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

class OGRPGeoDataSource final: public OGRDataSource
{
    OGRPGeoLayer        **papoLayers;
    int                 nLayers;

    char               *pszName;

    mutable CPLODBCSession oSession;

    bool                m_bHasGdbItemsTable = false;

#ifndef _WIN32
    mutable bool        m_COUNT_STAR_state_known = false;
    mutable bool        m_COUNT_STAR_working = false;
#endif
    static bool         IsPrivateLayerName( const CPLString& osName );
  public:
                        OGRPGeoDataSource();
                        virtual ~OGRPGeoDataSource();

    int                 Open( GDALOpenInfo* poOpenInfo );
    int                 OpenTable( const char *pszTableName,
                                   const char *pszGeomCol,
                                   int bUpdate );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;
    bool                IsLayerPrivate( int ) const override;

    int                 TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    // Internal use
    CPLODBCSession     *GetSession() { return &oSession; }

    bool CountStarWorking() const;

    bool HasGdbItemsTable() const { return m_bHasGdbItemsTable; }
};


#endif /* ndef _OGR_PGeo_H_INCLUDED */
