/******************************************************************************
 * $Id$
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Definition of classes for OGR MSSQL Spatial driver.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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

#ifndef OGR_MSSQLSPATIAL_H_INCLUDED
#define OGR_MSSQLSPATIAL_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"

class OGRMSSQLSpatialDataSource;

/* layer status */
#define MSSQLLAYERSTATUS_ORIGINAL  0
#define MSSQLLAYERSTATUS_INITIAL 1
#define MSSQLLAYERSTATUS_CREATED 2
#define MSSQLLAYERSTATUS_DISABLED 3

/* geometry format to transfer geometry column */
#define MSSQLGEOMETRY_NATIVE 0
#define MSSQLGEOMETRY_WKB 1
#define MSSQLGEOMETRY_WKT 2
#define MSSQLGEOMETRY_WKBZM 3  /* SQL Server 2012 */

/* geometry column types */
#define MSSQLCOLTYPE_GEOMETRY  0
#define MSSQLCOLTYPE_GEOGRAPHY 1
#define MSSQLCOLTYPE_BINARY 2
#define MSSQLCOLTYPE_TEXT 3

/* sqlgeometry constants */

#define SP_NONE 0
#define SP_HASZVALUES 1
#define SP_HASMVALUES 2
#define SP_ISVALID 4
#define SP_ISSINGLEPOINT 8
#define SP_ISSINGLELINESEGMENT 0x10
#define SP_ISWHOLEGLOBE 0x20

#define ST_UNKNOWN 0
#define ST_POINT 1
#define ST_LINESTRING 2
#define ST_POLYGON 3
#define ST_MULTIPOINT 4
#define ST_MULTILINESTRING 5
#define ST_MULTIPOLYGON 6
#define ST_GEOMETRYCOLLECTION 7

/************************************************************************/
/*                         OGRMSSQLAppendEscaped( )                     */
/************************************************************************/

void OGRMSSQLAppendEscaped( CPLODBCStatement* poStatement, const char* pszStrValue);

/************************************************************************/
/*                           OGRMSSQLGeometryParser                     */
/************************************************************************/

class OGRMSSQLGeometryValidator
{
protected:
    int bIsValid;
    OGRGeometry*    poValidGeometry;
    OGRGeometry*    poOriginalGeometry;

public:
    explicit         OGRMSSQLGeometryValidator(OGRGeometry* poGeom);
                    ~OGRMSSQLGeometryValidator();

    // cppcheck-suppress functionStatic
    int             ValidatePoint(OGRPoint * poGeom);
    // cppcheck-suppress functionStatic
    int             ValidateMultiPoint(OGRMultiPoint * poGeom);
    int             ValidateLineString(OGRLineString * poGeom);
    int             ValidateLinearRing(OGRLinearRing * poGeom);
    int             ValidateMultiLineString(OGRMultiLineString * poGeom);
    int             ValidatePolygon(OGRPolygon* poGeom);
    int             ValidateMultiPolygon(OGRMultiPolygon* poGeom);
    int             ValidateGeometryCollection(OGRGeometryCollection* poGeom);
    int             ValidateGeometry(OGRGeometry* poGeom);

    OGRGeometry*    GetValidGeometryRef();
    int             IsValid() { return bIsValid; }
};

/************************************************************************/
/*                           OGRMSSQLGeometryParser                     */
/************************************************************************/

class OGRMSSQLGeometryParser
{
protected:
    unsigned char* pszData;
    /* serialization properties */
    char chProps;
    /* point array */
    int nPointSize;
    int nPointPos;
    int nNumPoints;
    /* figure array */
    int nFigurePos;
    int nNumFigures;
    /* shape array */
    int nShapePos;
    int nNumShapes;
    int nSRSId;
    /* geometry or geography */
    int nColType;

protected:
    OGRPoint*           ReadPoint(int iShape);
    OGRMultiPoint*      ReadMultiPoint(int iShape);
    OGRLineString*      ReadLineString(int iShape);
    OGRMultiLineString* ReadMultiLineString(int iShape);
    OGRPolygon*         ReadPolygon(int iShape);
    OGRMultiPolygon*    ReadMultiPolygon(int iShape);
    OGRGeometryCollection* ReadGeometryCollection(int iShape);

public:
    explicit            OGRMSSQLGeometryParser( int nGeomColumnType );
    OGRErr              ParseSqlGeometry(unsigned char* pszInput, int nLen,
                                                        OGRGeometry **poGeom);
    int                 GetSRSId() { return nSRSId; }
};

/************************************************************************/
/*                           OGRMSSQLGeometryWriter                     */
/************************************************************************/

class OGRMSSQLGeometryWriter
{
protected:
    OGRGeometry *poGeom2;
    unsigned char* pszData;
    int nLen;
    /* serialization propeties */
    char chProps;
    /* point array */
    int nPointSize;
    int nPointPos;
    int nNumPoints;
    int iPoint;
    /* figure array */
    int nFigurePos;
    int nNumFigures;
    int iFigure;
    /* shape array */
    int nShapePos;
    int nNumShapes;
    int iShape;
    int nSRSId;
    /* geometry or geography */
    int nColType;

protected:
    void             WritePoint(OGRPoint* poGeom);
    void             WritePoint(double x, double y);
    void             WritePoint(double x, double y, double z);
    void             WriteLineString(OGRLineString* poGeom);
    void             WritePolygon(OGRPolygon* poGeom);
    void             WriteGeometryCollection(OGRGeometryCollection* poGeom, int iParent);
    void             WriteGeometry(OGRGeometry* poGeom, int iParent);
    void             TrackGeometry(OGRGeometry* poGeom);

public:
                     OGRMSSQLGeometryWriter(OGRGeometry *poGeometry, int nGeomColumnType, int nSRS);
    OGRErr           WriteSqlGeometry(unsigned char* pszBuffer, int nBufLen);
    int              GetDataLen() { return nLen; }
};

/************************************************************************/
/*                             OGRMSSQLSpatialLayer                     */
/************************************************************************/

class OGRMSSQLSpatialLayer : public OGRLayer
{
    protected:
    OGRFeatureDefn     *poFeatureDefn;
    int                 nRawColumns;

    CPLODBCStatement   *poStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    GIntBig             iNextShapeId;

    OGRMSSQLSpatialDataSource    *poDS;

    int                nGeomColumnType;
    char               *pszGeomColumn;
    int                nGeomColumnIndex;
    char               *pszFIDColumn;
    int                nFIDColumnIndex;

    int                bIsIdentityFid;

    int                nLayerStatus;

    int                *panFieldOrdinals;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }

  public:
                        OGRMSSQLSpatialLayer();
    virtual             ~OGRMSSQLSpatialLayer();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual OGRErr     StartTransaction() override;
    virtual OGRErr     CommitTransaction() override;
    virtual OGRErr     RollbackTransaction() override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    virtual int         TestCapability( const char * ) override;
    char*               GByteArrayToHexString( const GByte* pabyData, int nLen);

    void               SetLayerStatus( int nStatus ) { nLayerStatus = nStatus; }
    int                GetLayerStatus() { return nLayerStatus; }
};

/************************************************************************/
/*                       OGRMSSQLSpatialTableLayer                      */
/************************************************************************/

typedef union {
    struct {
        int     iIndicator;
        int     Value;
    } Integer;

    struct {
        int     iIndicator;
        GIntBig     Value;
    } Integer64;

    struct {
        int     iIndicator;
        double  Value;
    } Float;

    struct {
        SQLLEN  nSize;
        char* pData[8000];
    } VarChar;

    struct {
        SQLLEN  nSize;
        GByte*  pData;
    } RawData;

} BCPData;

class OGRMSSQLSpatialTableLayer : public OGRMSSQLSpatialLayer
{
    int                 bUpdateAccess;
    int                 bLaunderColumnNames;
    int                 bPreservePrecision;
    int                 bNeedSpatialIndex;
    int                 bUseCopy;
    int                 nBCPSize;

    int                 nUploadGeometryFormat;

    char                *pszQuery;

    SQLHANDLE           hEnvBCP;
    SQLHANDLE           hDBCBCP;
    int                 nBCPCount;
    BCPData             **papstBindBuffer;

    int                 bIdentityInsert;

    void                ClearStatement();
    CPLODBCStatement* BuildStatement(const char* pszColumns);

    CPLString BuildFields();

    virtual CPLODBCStatement *  GetStatement() override;

    char               *pszTableName;
    char               *pszLayerName;
    char               *pszSchemaName;

    OGRwkbGeometryType eGeomType;

  public:
    explicit            OGRMSSQLSpatialTableLayer( OGRMSSQLSpatialDataSource * );
                        virtual ~OGRMSSQLSpatialTableLayer();

    CPLErr              Initialize( const char *pszSchema,
                                    const char *pszTableName,
                                    const char *pszGeomCol,
                                    int nCoordDimension,
                                    int nSRId,
                                    const char *pszSRText,
                                    OGRwkbGeometryType eType);

    OGRErr              CreateSpatialIndex();
    void                DropSpatialIndex();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual const char* GetName() override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;

    const char*         GetTableName() { return pszTableName; }
    const char*         GetLayerName() { return pszLayerName; }
    const char*         GetSchemaName() { return pszSchemaName; }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual int         TestCapability( const char * ) override;

    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }
    void                SetSpatialIndexFlag( int bFlag )
                                { bNeedSpatialIndex = bFlag; }
    void                SetUploadGeometryFormat( int nGeometryFormat )
                                { nUploadGeometryFormat = nGeometryFormat; }
    void                AppendFieldValue(CPLODBCStatement *poStatement,
                                       OGRFeature* poFeature, int i, int *bind_num, void **bind_buffer);

    int                 FetchSRSId();

    void                SetUseCopy(int bcpSize) { bUseCopy = TRUE; nBCPSize = bcpSize; }
    int                 Failed( int nRetCode );
#ifdef MSSQL_BCP_SUPPORTED
    OGRErr              CreateFeatureBCP( OGRFeature *poFeature );
    int                 Failed2( int nRetCode );
    int                 InitBCP( const char* pszDSN );
    void                CloseBCP();
#endif
};

/************************************************************************/
/*                      OGRMSSQLSpatialSelectLayer                      */
/************************************************************************/

class OGRMSSQLSpatialSelectLayer : public OGRMSSQLSpatialLayer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

  public:
                        OGRMSSQLSpatialSelectLayer( OGRMSSQLSpatialDataSource *,
                                           CPLODBCStatement * );
                        virtual ~OGRMSSQLSpatialSelectLayer();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRODBCDataSource                          */
/************************************************************************/

class OGRMSSQLSpatialDataSource : public OGRDataSource
{
    OGRMSSQLSpatialTableLayer    **papoLayers;
    int                 nLayers;

    char               *pszName;

    char               *pszCatalog;

    int                 bDSUpdate;
    CPLODBCSession      oSession;

    int                 nGeometryFormat;

    int                 bUseGeometryColumns;

    int                 bListAllTables;

    int                 nBCPSize;
    int                 bUseCopy;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;

    char                *pszConnection;

  public:
                        OGRMSSQLSpatialDataSource();
                        virtual ~OGRMSSQLSpatialDataSource();

    const char          *GetCatalog() { return pszCatalog; }

    static int                 ParseValue(char** pszValue, char* pszSource, const char* pszKey,
                                  int nStart, int nNext, int nTerm, int bRemove);

    int                 Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *pszSchemaName, const char *pszTableName,
                                   const char *pszGeomCol,int nCoordDimension,
                                   int nSRID, const char *pszSRText,
                                   OGRwkbGeometryType eType, int bUpdate );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override;
    OGRLayer            *GetLayer( int ) override;
    OGRLayer            *GetLayerByName( const char* pszLayerName ) override;

    int                 GetGeometryFormat() { return nGeometryFormat; }
    int                 UseGeometryColumns() { return bUseGeometryColumns; }

    virtual OGRErr       DeleteLayer( int iLayer ) override;
    virtual OGRLayer    *ICreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL ) override;

    int                 TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    char                *LaunderName( const char *pszSrcName );
    OGRErr              InitializeMetadataTables();

    OGRSpatialReference* FetchSRS( int nId );
    int                 FetchSRSId( OGRSpatialReference * poSRS );

    OGRErr              StartTransaction(CPL_UNUSED int bForce) override;
    OGRErr              CommitTransaction() override;
    OGRErr              RollbackTransaction() override;

    // Internal use
    CPLODBCSession     *GetSession() { return &oSession; }
    const char         *GetConnectionString() { return pszConnection; }
};

/************************************************************************/
/*                             OGRODBCDriver                            */
/************************************************************************/

class OGRMSSQLSpatialDriver : public OGRSFDriver
{
  public:
    virtual ~OGRMSSQLSpatialDriver();

    const char *GetName() override;
    OGRDataSource *Open( const char *, int ) override;

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL ) override;

    int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_MSSQLSPATIAL_H_INCLUDED */
