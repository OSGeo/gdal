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
 ****************************************************************************/

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

    OGRFeatureDefn     *ReadTableDefinition(const char * pszTableName,
                                            const char * pszSchemaName);

    void                BuildWhere(void);
    char               *BuildFields(void);
    void                BuildFullQueryStatement(void);

    char               *pszTableName;
    char               *pszSchemaName;
    char               *pszSqlTableName;

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
                                         const char * pszTableName,
                                         const char * pszSchemaName,
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

    const char*         GetTableName() { return pszTableName; }
    const char*         GetSchemaName() { return pszSchemaName; }

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

    OGRPGTableLayer   **papoLayers;
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
    int                 OpenTable( const char * pszTableName, const char * pszSchemaName, int bUpdate, int bTestOpen );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    OGRLayer            *GetLayerByName(const char * pszName);

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


