/******************************************************************************
 * $Id: ogrwalk.h
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

#ifndef _OGRWALK_H_INCLUDED
#define _OGRWALK_H_INCLUDED

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

class OGRWalkLayer : public OGRLayer
{
protected:
    OGRFeatureDefn     *poFeatureDefn;

    CPLODBCStatement   *poStmt;

    // Layer spatial reference system
    OGRSpatialReference *poSRS;

    int                 iNextShapeId;

    OGRWalkDataSource    *poDS;

    int                bGeomColumnWKB;
    char               *pszGeomColumn;
    char               *pszFIDColumn;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }
    void                LookupSpatialRef( const char * );

public:
                        OGRWalkLayer();
                        ~OGRWalkLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    OGRFeature *        GetNextRawFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
    
    int         TestCapability( const char * ) { return FALSE; }

    virtual const char * GetFIDColumn (); 
    virtual const char * GetGeometryColumn ();
};

/************************************************************************/
/*                           OGRWalkTableLayer                          */
/************************************************************************/

class OGRWalkTableLayer : public OGRWalkLayer
{
    char                *pszQuery;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement();

    OGREnvelope         sExtent;

public:
                        OGRWalkTableLayer( OGRWalkDataSource * );
                        ~OGRWalkTableLayer();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol,
                                    double minE,
                                    double maxE,
                                    double minN,
                                    double maxN,
                                    const char *pszMemo );

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
};

/************************************************************************/
/*                          OGRWalkSelectLayer                          */
/************************************************************************/

class OGRWalkSelectLayer : public OGRWalkLayer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement();

  public:
                        OGRWalkSelectLayer( OGRWalkDataSource *, 
                                           CPLODBCStatement * );
                        ~OGRWalkSelectLayer();

    virtual void        ResetReading();
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

};

/************************************************************************/
/*                          OGRWalkDataSource                           */
/************************************************************************/

class OGRWalkDataSource : public OGRDataSource
{
    char               *pszName;
    OGRWalkLayer        **papoLayers;
    int                 nLayers;   

    int                 bDSUpdate;
    CPLODBCSession      oSession;

public:
                        OGRWalkDataSource();
                        ~OGRWalkDataSource();

    int                 Open( const char * , int );

    const char            *GetName() { return pszName; }
    int                    GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                    TestCapability( const char * ) { return FALSE; }

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );

    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    // For Internal Use
    CPLODBCSession     *GetSession() { return &oSession; }

};

/************************************************************************/
/*                            OGRWalkDriver                             */
/************************************************************************/

class OGRWalkDriver : public OGRODBCMDBDriver
{
public:
                ~OGRWalkDriver();
                
    const char    *GetName();
    OGRDataSource *Open( const char *, int );

    OGRDataSource *CreateDataSource( const char *, char ** );
    
    int            TestCapability( const char * );
};

void RegisterOGRWalk();

#endif /* ndef _OGRWALK_H_INCLUDED */
