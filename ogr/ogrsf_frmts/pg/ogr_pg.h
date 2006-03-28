/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/PostgreSQL driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.28  2006/03/28 23:06:57  fwarmerdam
 * fixed contact info
 *
 * Revision 1.27  2006/02/15 04:26:17  fwarmerdam
 * added date support
 *
 * Revision 1.26  2006/02/09 05:04:03  fwarmerdam
 * proper overriding of DeleteLayer() method
 *
 * Revision 1.25  2006/01/27 00:10:32  fwarmerdam
 * added Get{FID,Geometry}Column() support
 *
 * Revision 1.24  2006/01/21 03:48:29  fwarmerdam
 * Use CPLString for some OGRPGTableLayer buffers
 *
 * Revision 1.23  2005/10/24 23:50:50  fwarmerdam
 * moved bUseCopy test into layer on creation of first feature
 *
 * Revision 1.22  2005/10/16 03:39:25  fwarmerdam
 * cleanup COPY support somewhat
 *
 * Revision 1.21  2005/10/16 01:38:34  cfis
 * Updates that add support for using COPY for inserting data to Postgresql.  COPY is less robust than INSERT, but signficantly faster.
 *
 * Revision 1.20  2005/08/06 14:49:27  osemykin
 * Added BINARY CURSOR support
 * Use it with 'PGB:dbname=...' instead 'PG:dbname=...'
 *
 * Revision 1.19  2005/08/03 21:13:51  osemykin
 * Changed PostGIS version representation
 *
 * Revision 1.18  2005/07/20 01:46:04  fwarmerdam
 * postgis 8.0 upgrades
 *
 * Revision 1.17  2005/05/05 20:47:52  dron
 * Override GetExtent() method for PostGIS layers with PostGIS standard function
 * extent() (Oleg Semykin <oleg.semykin@gmail.com>
 *
 * Revision 1.16  2005/02/22 12:54:05  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.15  2004/11/17 17:49:26  fwarmerdam
 * implemented SetFeature and DeleteFeature methods
 *
 * Revision 1.14  2004/05/08 02:14:49  warmerda
 * added GetFeature() on table, generalize FID support a bit
 *
 * Revision 1.13  2004/04/30 17:52:42  warmerda
 * added layer name laundering
 *
 * Revision 1.12  2003/05/21 03:59:42  warmerda
 * expand tabs
 *
 * Revision 1.11  2002/10/09 18:30:10  warmerda
 * substantial upgrade to type handling, and preservations of width/precision
 *
 * Revision 1.10  2002/10/04 14:03:09  warmerda
 * added column name laundering support
 *
 * Revision 1.9  2002/05/09 16:03:19  warmerda
 * major upgrade to support SRS better and add ExecuteSQL
 *
 * Revision 1.8  2002/01/13 16:23:16  warmerda
 * capture dbname= parameter for use in AddGeometryColumn() call
 *
 * Revision 1.7  2001/11/15 21:19:58  warmerda
 * added soft transaction semantics
 *
 * Revision 1.6  2001/09/28 04:03:52  warmerda
 * partially upraded to PostGIS 0.6
 *
 * Revision 1.5  2001/06/26 20:59:13  warmerda
 * implement efficient spatial and attribute query support
 *
 * Revision 1.4  2001/06/19 22:29:12  warmerda
 * upgraded to include PostGIS support
 *
 * Revision 1.3  2001/06/19 15:50:23  warmerda
 * added feature attribute query support
 *
 * Revision 1.2  2000/11/23 06:03:35  warmerda
 * added Oid support
 *
 * Revision 1.1  2000/10/17 17:46:51  warmerda
 * New
 *
 */

#ifndef _OGR_PG_H_INCLUDED
#define _OGR_PG_H_INLLUDED

#include "ogrsf_frmts.h"
#include "libpq-fe.h"
#include "cpl_string.h"

/************************************************************************/
/*                            OGRPGLayer                                */
/************************************************************************/


class OGRPGDataSource;

class OGRPGLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;
    int                 nCoordDimension;

    int                 iNextShapeId;

    char               *GeometryToBYTEA( OGRGeometry * );
    OGRGeometry        *BYTEAToGeometry( const char * );
    OGRGeometry        *HEXToGeometry( const char * );
    char               *GeometryToHex( OGRGeometry * poGeometry, int nSRSId );
    Oid                 GeometryToOID( OGRGeometry * );
    OGRGeometry        *OIDToGeometry( Oid );

    OGRPGDataSource    *poDS;

    char               *pszQueryStatement;

    char               *pszCursorName;
    PGresult           *hCursorResult;
    int                 bCursorActive;

    int                 nResultOffset;

    int                 bHasWkb;
    int                 bWkbAsOid;
    int                 bHasPostGISGeometry;
    char                *pszGeomColumn;

    int                 bHasFid;
    char                *pszFIDColumn;

    int                 ParsePGDate( const char *, OGRField * );

  public:
                        OGRPGLayer();
    virtual             ~OGRPGLayer();

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    /* custom methods */
    virtual OGRFeature *RecordToFeature( int iRecord );
    virtual OGRFeature *GetNextRawFeature();
};

/************************************************************************/
/*                           OGRPGTableLayer                            */
/************************************************************************/


class OGRPGTableLayer : public OGRPGLayer
{
    int                 bUpdateAccess;

    OGRFeatureDefn     *ReadTableDefinition(const char *);

    void                BuildWhere(void);
    char               *BuildFields(void);
    void                BuildFullQueryStatement(void);

    CPLString           osQuery;
    CPLString           osWHERE;

    int                 bLaunderColumnNames;
    int                 bPreservePrecision;
    int                 bUseCopy;
    int                 bCopyActive;

    OGRErr		CreateFeatureViaCopy( OGRFeature *poFeature );
    OGRErr		CreateFeatureViaInsert( OGRFeature *poFeature );
    char                *BuildCopyFields(void);
public:
                        OGRPGTableLayer( OGRPGDataSource *,
                                         const char * pszName,
                                         int bUpdate, int nSRSId = -2 );
                        ~OGRPGTableLayer();

    virtual OGRFeature *GetFeature( long nFeatureId );
    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( long nFID );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );

	virtual OGRErr		GetExtent( OGREnvelope *psExtent, int bForce );

    // follow methods are not base class overrides
    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }

    virtual OGRErr      StartCopy();
    virtual OGRErr      EndCopy();
};

/************************************************************************/
/*                           OGRPGResultLayer                           */
/************************************************************************/

class OGRPGResultLayer : public OGRPGLayer
{
    void                BuildFullQueryStatement(void);

    char                *pszRawStatement;

    PGresult            *hInitialResult;

    int                 nFeatureCount;

  public:
                        OGRPGResultLayer( OGRPGDataSource *,
                                          const char * pszRawStatement,
                                          PGresult *hInitialResult );
    virtual             ~OGRPGResultLayer();

    void                SetSpatialFilter( OGRGeometry * ) {}

    OGRFeatureDefn     *ReadResultDefinition();

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );
};

/************************************************************************/
/*                           OGRPGDataSource                            */
/************************************************************************/
class OGRPGDataSource : public OGRDataSource
{
    typedef struct
    {
        int nMajor;
        int nMinor;
        int nRelease;
    } PGver;

    OGRPGLayer        **papoLayers;
    int                 nLayers;

    char               *pszName;
    char               *pszDBName;

    int                 bDSUpdate;
    int                 bHavePostGIS;

    int                 nSoftTransactionLevel;

    PGconn              *hPGConn;

    int                 DeleteLayer( int iLayer );

    Oid                 nGeometryOID;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 nKnownSRID;
    int                 *panSRID;
    OGRSpatialReference **papoSRS;

    OGRPGTableLayer     *poLayerInCopyMode;

  public:
    PGver               sPostGISVersion;
    
    int                 bUseBinaryCursor;

  public:
                        OGRPGDataSource();
                        ~OGRPGDataSource();

    PGconn              *GetPGConn() { return hPGConn; }

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

    OGRErr              SoftStartTransaction();
    OGRErr              SoftCommit();
    OGRErr              SoftRollback();

    OGRErr              FlushSoftTransaction();

    Oid                 GetGeometryOID() { return nGeometryOID; }

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    char               *LaunderName( const char * );

    int                 UseCopy();
    void                StartCopy( OGRPGTableLayer *poPGLayer );
    OGRErr              EndCopy( );
    int                 CopyInProgress( );
};

/************************************************************************/
/*                             OGRPGDriver                              */
/************************************************************************/

class OGRPGDriver : public OGRSFDriver
{
  public:
                ~OGRPGDriver();

    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );

    int                 TestCapability( const char * );
};


#endif /* ndef _OGR_PG_H_INCLUDED */


