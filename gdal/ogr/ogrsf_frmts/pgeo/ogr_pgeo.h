/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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
 ****************************************************************************/

#ifndef _OGR_ODBC_H_INCLUDED
#define _OGR_ODBC_H_INLLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"

#define SHPT_NULL	0
#define SHPT_POINT	1
#define SHPT_ARC	3
#define SHPT_POLYGON	5
#define SHPT_MULTIPOINT	8
#define SHPT_POINTZ	11
#define SHPT_ARCZ	13
#define SHPT_POLYGONZ	15
#define SHPT_MULTIPOINTZ 18
#define SHPT_POINTM	21
#define SHPT_ARCM	23
#define SHPT_POLYGONM	25
#define SHPT_MULTIPOINTM 28
#define SHPT_MULTIPATCH 31

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

    int                 iNextShapeId;

    OGRPGeoDataSource    *poDS;

    char                *pszGeomColumn;
    char                *pszFIDColumn;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }
    OGRErr              createFromShapeBin( GByte *pabyShape, 
                                            OGRGeometry **ppoGeom,
                                            int nBytes );

    void                LookupSRID( int );

  public:
                        OGRPGeoLayer();
    virtual             ~OGRPGeoLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                           OGRPGeoTableLayer                            */
/************************************************************************/

class OGRPGeoTableLayer : public OGRPGeoLayer
{
    int                 bUpdateAccess;

    char                *pszQuery;

    void		ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement();

    OGREnvelope         sExtent;

  public:
                        OGRPGeoTableLayer( OGRPGeoDataSource * );
                        ~OGRPGeoTableLayer();

    CPLErr              Initialize( const char *pszTableName, 
                                    const char *pszGeomCol,
                                    int nShapeType,
                                    double dfExtentLeft,
                                    double dfExtentRight,
                                    double dfExtentBottom,
                                    double dfExtentTop,
                                    int nSRID,
                                    int bHasZ );

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
};

/************************************************************************/
/*                          OGRPGeoSelectLayer                          */
/************************************************************************/

class OGRPGeoSelectLayer : public OGRPGeoLayer
{
    char                *pszBaseStatement;

    void		ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement();

  public:
                        OGRPGeoSelectLayer( OGRPGeoDataSource *, 
                                           CPLODBCStatement * );
                        ~OGRPGeoSelectLayer();

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual int         TestCapability( const char * );
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
                        ~OGRPGeoDataSource();

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
    CPLODBCSession     *GetSession() { return &oSession; }
};

/************************************************************************/
/*                             OGRPGeoDriver                            */
/************************************************************************/

class OGRPGeoDriver : public OGRSFDriver
{
    CPLString   osDriverFile;

    bool        InstallMdbDriver();
    bool        LibraryExists( const char* pszLibPath );
    bool        FindDriverLib();
    CPLString   FindDefaultLib(const char* pszLibName);

  public:
                ~OGRPGeoDriver();
                
    const char  *GetName();
    OGRDataSource *Open( const char *, int );

    int          TestCapability( const char * );
};

#endif /* ndef _OGR_PGeo_H_INCLUDED */
