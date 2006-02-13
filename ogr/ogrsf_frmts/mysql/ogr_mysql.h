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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.20  2006/02/13 04:15:04  hobu
 * major formatting cleanup
 * Added myself as an author
 *
 * Revision 1.19  2006/02/13 01:44:25  hobu
 * Define an Initialize() method for TableLayer and
 * make sure to use it when we after we construct
 * a new one.  This is to prevent ResetReading from
 * causing segfault in the case where we were given
 * an invalid table name
 *
 * Revision 1.17  2006/02/12 06:17:25  hobu
 * Implement DeleteFeature
 *
 * Revision 1.16  2006/02/12 00:40:56  hobu
 * Implement a somewhat working CreateFeature
 *
 * Revision 1.15  2006/02/11 18:07:26  hobu
 * Moved FetchSRS to happen via the Datasource like PG
 * rather than in the TableLayer and ResultLayer as before
 * Implemented CreateField and CreateLayer
 *
 * Revision 1.14  2006/02/10 04:58:37  hobu
 * InitializeMetadataTables implementation
 *
 * Revision 1.13  2006/02/09 05:54:52  hobu
 * start on CreateLayer
 *
 * Revision 1.12  2006/02/06 23:06:45  fwarmerdam
 * undef mysql's bool
 *
 * Revision 1.11  2006/02/01 01:40:09  hobu
 * separate fetching of SRID
 *
 * Revision 1.10  2006/01/31 06:16:57  hobu
 * move SRS fetching to mysqllayer.cpp
 *
 * Revision 1.9  2006/01/31 05:35:09  hobu
 * move SRS fetching to mysqllayer.cpp
 *
 * Revision 1.8  2006/01/30 03:51:18  hobu
 * some god-awful hackery, but we can do a good job of reading 
 * field definitions from a select query as well as get a spatial reference.
 *
 * Revision 1.7  2006/01/27 01:27:48  fwarmerdam
 * added GetFIDColumn and GetGeometryColumn support
 *
 * Revision 1.6  2006/01/16 16:06:48  hobu
 * Handle geometry column
 *
 * Revision 1.5  2005/08/30 23:53:16  fwarmerdam
 * implement binary field support
 *
 * Revision 1.4  2005/02/22 12:54:27  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.3  2004/10/12 16:59:31  fwarmerdam
 * rearrange include files for win32
 *
 * Revision 1.2  2004/10/08 20:49:01  fwarmerdam
 * enable ExecuteSQL
 *
 * Revision 1.1  2004/10/07 20:57:50  fwarmerdam
 * New
 *
 */

#ifndef _OGR_MYSQL_H_INCLUDED
#define _OGR_MYSQL_H_INLLUDED

#include <my_global.h>
#include <mysql.h>

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

    int                 iNextShapeId;

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

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );

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
    
    virtual OGRFeature *GetFeature( long nFeatureId );
    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    void                SetSpatialFilter( OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( long nFID );
    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }    

    virtual int         TestCapability( const char * );
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
    
    int                 nFeatureCount;

  public:
                        OGRMySQLResultLayer( OGRMySQLDataSource *,
                                             const char * pszRawStatement,
                                             MYSQL_RES *hResultSetIn );
    virtual             ~OGRMySQLResultLayer();

    OGRFeatureDefn     *ReadResultDefinition();


    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );
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
    int                 bHavePostGIS;
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

    int                 Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *, int bUpdate, int bTestOpen );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *, 
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

/************************************************************************/
/*                            OGRMySQLDriver                            */
/************************************************************************/

class OGRMySQLDriver : public OGRSFDriver
{
  public:
                ~OGRMySQLDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
#ifdef notdef
    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
#endif    
    int                 TestCapability( const char * );
};


#endif /* ndef _OGR_PG_H_INCLUDED */


