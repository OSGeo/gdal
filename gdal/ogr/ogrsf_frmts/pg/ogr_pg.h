/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/PostgreSQL driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_PG_H_INCLUDED
#define OGR_PG_H_INCLUDED

#include "ogrsf_frmts.h"
#include "libpq-fe.h"
#include "cpl_string.h"

#include "ogrpgutility.h"
#include "ogr_pgdump.h"

/* These are the OIDs for some builtin types, as returned by PQftype(). */
/* They were copied from pg_type.h in src/include/catalog/pg_type.h */

#define BOOLOID                 16
#define BYTEAOID                17
#define CHAROID                 18
#define NAMEOID                 19
#define INT8OID                 20
#define INT2OID                 21
#define INT2VECTOROID           22
#define INT4OID                 23
#define REGPROCOID              24
#define TEXTOID                 25
#define OIDOID                  26
#define TIDOID                  27
#define XIDOID                  28
#define CIDOID                  29
#define OIDVECTOROID            30
#define FLOAT4OID               700
#define FLOAT8OID               701
#define BOOLARRAYOID            1000
#define INT2ARRAYOID            1005
#define INT4ARRAYOID            1007
#define TEXTARRAYOID            1009
#define BPCHARARRAYOID          1014
#define VARCHARARRAYOID         1015
#define INT8ARRAYOID            1016
#define FLOAT4ARRAYOID          1021
#define FLOAT8ARRAYOID          1022
#define BPCHAROID               1042
#define VARCHAROID              1043
#define DATEOID                 1082
#define TIMEOID                 1083
#define TIMESTAMPOID            1114
#define TIMESTAMPTZOID          1184
#define NUMERICOID              1700
#define NUMERICARRAYOID         1231

CPLString OGRPGEscapeString(PGconn *hPGConn,
                            const char* pszStrValue, int nMaxLength = -1,
                            const char* pszTableName = "",
                            const char* pszFieldName = "");
CPLString OGRPGEscapeColumnName(const char* pszColumnName);

#define UNDETERMINED_SRID       -2 /* Special value when we haven't yet looked for SRID */

class OGRPGDataSource;
class OGRPGLayer;

typedef enum
{
    GEOM_TYPE_UNKNOWN = 0,
    GEOM_TYPE_GEOMETRY = 1,
    GEOM_TYPE_GEOGRAPHY = 2,
    GEOM_TYPE_WKB = 3
} PostgisType;

typedef struct
{
    char* pszName;
    char* pszGeomType;
    int   GeometryTypeFlags;
    int   nSRID;
    PostgisType   ePostgisType;
    int   bNullable;
} PGGeomColumnDesc;

/************************************************************************/
/*                         OGRPGGeomFieldDefn                           */
/************************************************************************/

class OGRPGGeomFieldDefn : public OGRGeomFieldDefn
{
    protected:
        OGRPGLayer* poLayer;

    public:
        OGRPGGeomFieldDefn( OGRPGLayer* poLayerIn,
                                const char* pszFieldName ) :
            OGRGeomFieldDefn(pszFieldName, wkbUnknown), poLayer(poLayerIn),
            nSRSId(UNDETERMINED_SRID), GeometryTypeFlags(0), ePostgisType(GEOM_TYPE_UNKNOWN)
            {
            }

        virtual OGRSpatialReference* GetSpatialRef();

        void UnsetLayer() { poLayer = NULL; }

        int nSRSId;
        int GeometryTypeFlags;
        PostgisType   ePostgisType;
};

/************************************************************************/
/*                          OGRPGFeatureDefn                            */
/************************************************************************/

class OGRPGFeatureDefn : public OGRFeatureDefn
{
    public:
        OGRPGFeatureDefn( const char * pszName = NULL ) :
            OGRFeatureDefn(pszName)
        {
            SetGeomType(wkbNone);
        }

        virtual void UnsetLayer()
        {
            for(int i=0;i<nGeomFieldCount;i++)
                ((OGRPGGeomFieldDefn*) papoGeomFieldDefn[i])->UnsetLayer();
        }

        OGRPGGeomFieldDefn* myGetGeomFieldDefn(int i)
        {
            return (OGRPGGeomFieldDefn*) GetGeomFieldDefn(i);
        }
};

/************************************************************************/
/*                            OGRPGLayer                                */
/************************************************************************/

class OGRPGLayer : public OGRLayer
{
  protected:
    OGRPGFeatureDefn   *poFeatureDefn;

    int                 nCursorPage;
    GIntBig             iNextShapeId;

    static char        *GByteArrayToBYTEA( const GByte* pabyData, int nLen);
    static char        *GeometryToBYTEA( OGRGeometry *, int nPostGISMajor, int nPostGISMinor );
    static GByte       *BYTEAToGByteArray( const char *pszBytea, int* pnLength );
    static OGRGeometry *BYTEAToGeometry( const char *, int bIsPostGIS1 );
    Oid                 GeometryToOID( OGRGeometry * );
    OGRGeometry        *OIDToGeometry( Oid );

    OGRPGDataSource    *poDS;

    char               *pszQueryStatement;

    char               *pszCursorName;
    PGresult           *hCursorResult;
    int                 bInvalidated;

    int                 nResultOffset;

    int                 bWkbAsOid;

    char                *pszFIDColumn;

    int                 bCanUseBinaryCursor;
    int                *m_panMapFieldNameToIndex;
    int                *m_panMapFieldNameToGeomIndex;

    int                 ParsePGDate( const char *, OGRField * );

    void                SetInitialQueryCursor();
    void                CloseCursor();

    virtual CPLString   GetFromClauseForGetExtent() = 0;
    OGRErr              RunGetExtentRequest( OGREnvelope *psExtent, int bForce,
                                             CPLString osCommand, int bErrorAsDebug );
    static void         CreateMapFromFieldNameToIndex(PGresult* hResult,
                                                      OGRFeatureDefn* poFeatureDefn,
                                                      int*& panMapFieldNameToIndex,
                                                      int*& panMapFieldNameToGeomIndex);

    int                 ReadResultDefinition(PGresult *hInitialResultIn);

    OGRFeature         *RecordToFeature( PGresult* hResult,
                                         const int* panMapFieldNameToIndex,
                                         const int* panMapFieldNameToGeomIndex,
                                         int iRecord );
    OGRFeature         *GetNextRawFeature();

  public:
                        OGRPGLayer();
    virtual             ~OGRPGLayer();

    virtual void        ResetReading();

    virtual OGRFeatureDefn *    GetLayerDefn();
    virtual OGRPGFeatureDefn *  myGetLayerDefn() { return (OGRPGFeatureDefn*) GetLayerDefn(); }

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce );

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    void                InvalidateCursor();

    virtual const char *GetFIDColumn();

    virtual OGRErr      SetNextByIndex( GIntBig nIndex );

    OGRPGDataSource    *GetDS() { return poDS; }

    virtual void        ResolveSRID(OGRPGGeomFieldDefn* poGFldDefn) = 0;
};

/************************************************************************/
/*                           OGRPGTableLayer                            */
/************************************************************************/

class OGRPGTableLayer : public OGRPGLayer
{
    int                 bUpdateAccess;

    void                BuildWhere(void);
    CPLString           BuildFields(void);
    void                BuildFullQueryStatement(void);

    char               *pszTableName;
    char               *pszSchemaName;
    char               *pszDescription;
    CPLString           osForcedDescription;
    char               *pszSqlTableName;
    int                 bTableDefinitionValid;

    CPLString           osPrimaryKey;

    int                 bGeometryInformationSet;

    /* Name of the parent table with the geometry definition if it is a derived table or NULL */
    char               *pszSqlGeomParentTableName;

    char               *pszGeomColForced;

    CPLString           osQuery;
    CPLString           osWHERE;

    int                 bLaunderColumnNames;
    int                 bPreservePrecision;
    int                 bUseCopy;
    int                 bCopyActive;
    int                 bFIDColumnInCopyFields;
    int                 bFirstInsertion;

    OGRErr              CreateFeatureViaCopy( OGRFeature *poFeature );
    OGRErr              CreateFeatureViaInsert( OGRFeature *poFeature );
    CPLString           BuildCopyFields();

    int                 bHasWarnedIncompatibleGeom;
    void                CheckGeomTypeCompatibility(int iGeomField, OGRGeometry* poGeom);

    int                 bRetrieveFID;
    int                 bHasWarnedAlreadySetFID;

    char              **papszOverrideColumnTypes;
    int                 nForcedSRSId;
    int                 nForcedGeometryTypeFlags;
    int                 bCreateSpatialIndexFlag;
    int                 bInResetReading;

    int                 bAutoFIDOnCreateViaCopy;
    int                 bUseCopyByDefault;

    int                 bDeferredCreation;
    CPLString           osCreateTable;

    int                 iFIDAsRegularColumnIndex;

    CPLString           m_osFirstGeometryFieldName;

    virtual CPLString   GetFromClauseForGetExtent() { return pszSqlTableName; }

    OGRErr              RunAddGeometryColumn( OGRPGGeomFieldDefn *poGeomField );
    OGRErr              RunCreateSpatialIndex( OGRPGGeomFieldDefn *poGeomField );

public:
                        OGRPGTableLayer( OGRPGDataSource *,
                                         CPLString& osCurrentSchema,
                                         const char * pszTableName,
                                         const char * pszSchemaName,
                                         const char * pszDescriptionIn,
                                         const char * pszGeomColForced,
                                         int bUpdate );
                        virtual ~OGRPGTableLayer();

    void                SetGeometryInformation(PGGeomColumnDesc* pasDesc,
                                               int nGeomFieldCount);

    virtual OGRFeature *GetFeature( GIntBig nFeatureId );
    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual GIntBig     GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry *poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( GIntBig nFID );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                         int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce );

    const char*         GetTableName() { return pszTableName; }
    const char*         GetSchemaName() { return pszSchemaName; }

    virtual const char *GetFIDColumn();

    virtual char      **GetMetadataDomainList();
    virtual char      **GetMetadata(const char* pszDomain = "");
    virtual const char *GetMetadataItem(const char* pszName, const char* pszDomain = "");
    virtual CPLErr      SetMetadata(char** papszMD, const char* pszDomain = "");
    virtual CPLErr      SetMetadataItem(const char* pszName, const char* pszValue, const char* pszDomain = "");

    // follow methods are not base class overrides
    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }

    void                SetOverrideColumnTypes( const char* pszOverrideColumnTypes );

    OGRErr              StartCopy();
    OGRErr              EndCopy();

    int                 ReadTableDefinition();
    int                 HasGeometryInformation() { return bGeometryInformationSet; }
    void                SetTableDefinition(const char* pszFIDColumnName,
                                           const char* pszGFldName,
                                           OGRwkbGeometryType eType,
                                           const char* pszGeomType,
                                           int nSRSId,
                                           int GeometryTypeFlags);

    void                SetForcedSRSId( int nForcedSRSIdIn )
                                { nForcedSRSId = nForcedSRSIdIn; }
    void                SetForcedGeometryTypeFlags( int GeometryTypeFlagsIn )
                                { nForcedGeometryTypeFlags = GeometryTypeFlagsIn; }
    void                SetCreateSpatialIndexFlag( int bFlag )
                                { bCreateSpatialIndexFlag = bFlag; }
    void                SetForcedDescription( const char* pszDescriptionIn );
    void                AllowAutoFIDOnCreateViaCopy() { bAutoFIDOnCreateViaCopy = TRUE; }
    void                SetUseCopy() { bUseCopy = TRUE; bUseCopyByDefault = TRUE; }

    void                SetDeferredCreation(int bDeferredCreationIn, CPLString osCreateTable);
    OGRErr              RunDeferredCreationIfNecessary();

    virtual void        ResolveSRID(OGRPGGeomFieldDefn* poGFldDefn);
};

/************************************************************************/
/*                           OGRPGResultLayer                           */
/************************************************************************/

class OGRPGResultLayer : public OGRPGLayer
{
    void                BuildFullQueryStatement(void);

    char                *pszRawStatement;

    char                *pszGeomTableName;
    char                *pszGeomTableSchemaName;

    CPLString           osWHERE;

    virtual CPLString   GetFromClauseForGetExtent()
        { CPLString osStr("(");
          osStr += pszRawStatement; osStr += ")"; return osStr; }

  public:
                        OGRPGResultLayer( OGRPGDataSource *,
                                          const char * pszRawStatement,
                                          PGresult *hInitialResult );
    virtual             ~OGRPGResultLayer();

    virtual void        ResetReading();
    virtual GIntBig     GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry *poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom );

    virtual int         TestCapability( const char * );

    virtual OGRFeature *GetNextFeature();

    virtual void        ResolveSRID(OGRPGGeomFieldDefn* poGFldDefn);
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
    int                 bHaveGeography;

    int                 bUserTransactionActive;
    int                 bSavePointActive;
    int                 nSoftTransactionLevel;

    PGconn              *hPGConn;

    OGRErr              DeleteLayer( int iLayer );

    Oid                 nGeometryOID;
    Oid                 nGeographyOID;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 nKnownSRID;
    int                 *panSRID;
    OGRSpatialReference **papoSRS;

    OGRPGTableLayer     *poLayerInCopyMode;

    void                OGRPGDecodeVersionString(PGver* psVersion, const char* pszVer);

    CPLString           osCurrentSchema;
    CPLString           GetCurrentSchema();

    int                 nUndefinedSRID;

    char               *pszForcedTables;
    char              **papszSchemaList;
    int                 bHasLoadTables;
    CPLString           osActiveSchema;
    int                 bListAllTables;
    void                LoadTables();

    CPLString           osDebugLastTransactionCommand;
    OGRErr              DoTransactionCommand(const char* pszCommand);

    OGRErr              FlushSoftTransaction();

  public:
    PGver               sPostgreSQLVersion;
    PGver               sPostGISVersion;

    int                 bUseBinaryCursor;
    int                 bBinaryTimeFormatIsInt8;
    int                 bUseEscapeStringSyntax;

    bool                m_bHasGeometryColumns;
    bool                m_bHasSpatialRefSys;

    int                GetUndefinedSRID() const { return nUndefinedSRID; }

  public:
                        OGRPGDataSource();
                        virtual ~OGRPGDataSource();

    PGconn              *GetPGConn() { return hPGConn; }

    int                 FetchSRSId( OGRSpatialReference * poSRS );
    OGRSpatialReference *FetchSRS( int nSRSId );
    OGRErr              InitializeMetadataTables();

    int                 Open( const char *, int bUpdate, int bTestOpen,
                              char** papszOpenOptions );
    OGRPGTableLayer*    OpenTable( CPLString& osCurrentSchema,
                                   const char * pszTableName,
                                   const char * pszSchemaName,
                                   const char * pszDescription,
                                   const char * pszGeomColForced,
                                   int bUpdate, int bTestOpen );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount();
    OGRLayer            *GetLayer( int );
    OGRLayer            *GetLayerByName(const char * pszName);

    virtual void        FlushCache(void);

    virtual OGRLayer    *ICreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );

    virtual OGRErr      StartTransaction(int bForce = FALSE);
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    OGRErr              SoftStartTransaction();
    OGRErr              SoftCommitTransaction();
    OGRErr              SoftRollbackTransaction();

    Oid                 GetGeometryOID() { return nGeometryOID; }
    Oid                 GetGeographyOID() { return nGeographyOID; }

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    virtual const char* GetMetadataItem(const char* pszKey,
                                             const char* pszDomain);

    int                 UseCopy();
    void                StartCopy( OGRPGTableLayer *poPGLayer );
    OGRErr              EndCopy( );
};

#endif /* ndef OGR_PG_H_INCLUDED */
