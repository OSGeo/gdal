/******************************************************************************
 * $Id: ogrwalk.h$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definition of classes for OGR Walk driver.
 * Author:   Xian Chen, chenxian at walkinfo.com.cn
 *
 ******************************************************************************
 * Copyright (c) 2013,  ZJU Walkinfo Technology Corp., Ltd.
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

#ifndef OGRWALK_H_INCLUDED
#define OGRWALK_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"
#include "ogis_geometry_wkb_struct.h"
#include "ogr_pgeo.h"

/************************************************************************/
/*             Functions for WalkBinary Translation                     */
/************************************************************************/

OGRErr Binary2WkbGeom(unsigned char *p, WKBGeometry* geom, int nBuffer);
OGRErr TranslateWalkGeom(OGRGeometry **ppoGeom, WKBGeometry* geom);
void DeleteWKBGeometry(WKBGeometry &obj);

/************************************************************************/
/*                            OGRWalkLayer                              */
/************************************************************************/

class OGRWalkDataSource;

class OGRWalkLayer CPL_NON_FINAL: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRWalkLayer>
{
protected:
    OGRFeatureDefn     *poFeatureDefn;

    CPLODBCStatement   *poStmt;

    // Layer spatial reference system
    OGRSpatialReference *poSRS;

    GIntBig             iNextShapeId;

    OGRWalkDataSource    *poDS;

    bool               bGeomColumnWKB;
    char               *pszGeomColumn;
    char               *pszFIDColumn;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }
    void                LookupSpatialRef( const char * );

    OGRFeature *        GetNextRawFeature();

public:
                        OGRWalkLayer();
                        virtual ~OGRWalkLayer();

    void                ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRWalkLayer)

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int         TestCapability( const char * ) override { return FALSE; }

    virtual const char * GetFIDColumn () override;
    virtual const char * GetGeometryColumn () override;
};

/************************************************************************/
/*                           OGRWalkTableLayer                          */
/************************************************************************/

class OGRWalkTableLayer final: public OGRWalkLayer
{
    char                *pszQuery;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

    OGREnvelope         sExtent;

public:
    explicit            OGRWalkTableLayer( OGRWalkDataSource * );
                        virtual ~OGRWalkTableLayer();

    CPLErr              Initialize( const char *pszTableName,
                                    const char *pszGeomCol,
                                    double minE,
                                    double maxE,
                                    double minN,
                                    double maxN,
                                    const char *pszMemo );

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
/*                          OGRWalkSelectLayer                          */
/************************************************************************/

class OGRWalkSelectLayer final: public OGRWalkLayer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

  public:
                        OGRWalkSelectLayer( OGRWalkDataSource *,
                                           CPLODBCStatement * );
                        virtual ~OGRWalkSelectLayer();

    virtual void        ResetReading() override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
};

/************************************************************************/
/*                          OGRWalkDataSource                           */
/************************************************************************/

class OGRWalkDataSource final: public OGRDataSource
{
    char               *pszName;
    OGRWalkLayer        **papoLayers;
    int                 nLayers;

    CPLODBCSession      oSession;

public:
                        OGRWalkDataSource();
                        virtual ~OGRWalkDataSource();

    int                 Open( const char * );

    const char            *GetName() override { return pszName; }
    int                    GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                    TestCapability( const char * ) override { return FALSE; }

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;

    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    // For Internal Use
    CPLODBCSession     *GetSession() { return &oSession; }
};

void RegisterOGRWalk();

#endif /* ndef OGRWALK_H_INCLUDED */
