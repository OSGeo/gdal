/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Declarations for MySQL OGR Driver Classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Howard Butler, hobu@hobu.net
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_MYSQL_H_INCLUDED
#define _OGR_MYSQL_H_INCLUDED

#include <my_global.h>
#include <mysql.h>

/* my_global.h from mysql 5.1 declares the min and max macros. */
/* This conflicts with templates in g++-4.3.2 header files. Grrr */
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef bool
#undef bool
#endif

#include "ogrsf_frmts.h"

/************************************************************************/
/*                            OGRMySQLLayer                             */
/************************************************************************/

class OGRMySQLDataSource;
    
class OGRMySQLLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    GIntBig             iNextShapeId;

    OGRMySQLDataSource    *poDS;
 
    char               *pszQueryStatement;

    int                 nResultOffset;

    char                *pszGeomColumn;
    char                *pszGeomColumnTable;
    int                 nGeomType;

    int                 bHasFid;
    char                *pszFIDColumn;

    MYSQL_RES           *hResultSet;

	int                 FetchSRSId();

  public:
                        OGRMySQLLayer();
    virtual             ~OGRMySQLLayer();

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( GIntBig nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    /* custom methods */
    virtual OGRFeature *RecordToFeature( char **papszRow, unsigned long * );
    virtual OGRFeature *GetNextRawFeature();
};

/************************************************************************/
/*                          OGRMySQLTableLayer                          */
/************************************************************************/

class OGRMySQLTableLayer : public OGRMySQLLayer
{
    int                 bUpdateAccess;

    OGRFeatureDefn     *ReadTableDefinition(const char *);

    void                BuildWhere(void);
    char               *BuildFields(void);
    void                BuildFullQueryStatement(void);

    char                *pszQuery;
    char                *pszWHERE;

    int                 bLaunderColumnNames;
    int                 bPreservePrecision;
    
  public:
                        OGRMySQLTableLayer( OGRMySQLDataSource *,
                                         const char * pszName,
                                         int bUpdate, int nSRSId = -2 );
                        ~OGRMySQLTableLayer();

    OGRErr              Initialize(const char* pszTableName);
    
    virtual OGRFeature *GetFeature( GIntBig nFeatureId );
    virtual void        ResetReading();
    virtual GIntBig     GetFeatureCount( int );

    void                SetSpatialFilter( OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( GIntBig nFID );
    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }    

    virtual int         TestCapability( const char * );
	virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
};

/************************************************************************/
/*                         OGRMySQLResultLayer                          */
/************************************************************************/

class OGRMySQLResultLayer : public OGRMySQLLayer
{
    void                BuildFullQueryStatement(void);

    char                *pszRawStatement;
    
    // Layer srid.
    int                 nSRSId;
    
  public:
                        OGRMySQLResultLayer( OGRMySQLDataSource *,
                                             const char * pszRawStatement,
                                             MYSQL_RES *hResultSetIn );
    virtual             ~OGRMySQLResultLayer();

    OGRFeatureDefn     *ReadResultDefinition();


    virtual void        ResetReading();
    virtual GIntBig     GetFeatureCount( int );

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                          OGRMySQLDataSource                          */
/************************************************************************/

class OGRMySQLDataSource : public OGRDataSource
{
    OGRMySQLLayer       **papoLayers;
    int                 nLayers;
    
    char               *pszName;

    int                 bDSUpdate;

    int                 nSoftTransactionLevel;

    MYSQL              *hConn;

    int                DeleteLayer( int iLayer );

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes. 
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;

    OGRMySQLLayer      *poLongResultLayer;
    
  public:
                        OGRMySQLDataSource();
                        ~OGRMySQLDataSource();

    MYSQL              *GetConn() { return hConn; }


    int                 FetchSRSId( OGRSpatialReference * poSRS );

    OGRSpatialReference *FetchSRS( int nSRSId );

    OGRErr              InitializeMetadataTables();

    int                 Open( const char *, int bUpdate );
    int                 OpenTable( const char *, int bUpdate );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *ICreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );


    int                 TestCapability( const char * );

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    // nonstandard

    void                ReportError( const char * = NULL );
    
    char               *LaunderName( const char * );

    void                RequestLongResult( OGRMySQLLayer * );
    void                InterruptLongResult();
};

#endif /* ndef _OGR_MYSQL_H_INCLUDED */


