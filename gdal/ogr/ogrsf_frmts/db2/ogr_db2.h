/*****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Definition of classes for OGR DB2 Spatial driver.
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 *****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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

#ifndef _OGR_DB2_H_INCLUDED
#define _OGR_DB2_H_INCLUDED
//***#include <sqlcli1.h>   // needed for CLI support
#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"

class OGRDB2DataSource;

#define DB2ODBC_PREFIX "DB2ODBC:"

/************************************************************************/
/*                         OGRDB2AppendEscaped( )                       */
/************************************************************************/

void OGRDB2AppendEscaped( CPLODBCStatement* poStatement,
                          const char* pszStrValue);


/************************************************************************/
/*                             OGRDB2Layer                              */
/************************************************************************/

class OGRDB2Layer : public OGRLayer
{
protected:
    OGRFeatureDefn     *poFeatureDefn;

    CPLODBCStatement   *m_poStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    GIntBig            iNextShapeId;

    OGRDB2DataSource   *poDS;

    char               *pszGeomColumn;
    char               *pszFIDColumn;

    int                bIsIdentityFid;
    char               cGenerated;// 'A' always generated, 'D' default,' ' not
    int                nLayerStatus;
    int                *panFieldOrdinals;

    CPLErr             BuildFeatureDefn( const char *pszLayerName,
                                         CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() {
        return m_poStmt;
    }

public:
    OGRDB2Layer();
    virtual             ~OGRDB2Layer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( GIntBig nFeatureId );

    virtual OGRFeatureDefn *GetLayerDefn() {
        return poFeatureDefn;
    }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual OGRErr     StartTransaction();
    virtual OGRErr     CommitTransaction();
    virtual OGRErr     RollbackTransaction();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual int         TestCapability( const char * );
    char*               GByteArrayToHexString( const GByte* pabyData,
            int nLen);

    void               SetLayerStatus( int nStatus ) {
        nLayerStatus = nStatus;
    }
    int                GetLayerStatus() {
        return nLayerStatus;
    }
    int                GetSRSId() {
        return nSRSId;
    }
};

/************************************************************************/
/*                       OGRDB2TableLayer                               */
/************************************************************************/

class OGRDB2TableLayer : public OGRDB2Layer
{
    int                 bUpdateAccess;
    int                 bLaunderColumnNames;
    int                 bPreservePrecision;
    int                 bNeedSpatialIndex;

    int                 nUploadGeometryFormat;
    char                *m_pszQuery;

    void                ClearStatement();
    CPLODBCStatement* BuildStatement(const char* pszColumns);

    CPLString BuildFields();

    virtual CPLODBCStatement *  GetStatement();

    char               *pszTableName;
    char               *m_pszLayerName;
    char               *pszSchemaName;

    OGRwkbGeometryType eGeomType;

public:
    OGRDB2TableLayer( OGRDB2DataSource * );
    ~OGRDB2TableLayer();

    CPLErr              Initialize( const char *pszSchema,
                                    const char *pszTableName,
                                    const char *pszGeomCol,
                                    int nCoordDimension,
                                    int nSRId,
                                    const char *pszSRText,
                                    OGRwkbGeometryType eType);

    OGRErr              CreateSpatialIndex();
    void                DropSpatialIndex();

    virtual void        ResetReading();
    virtual GIntBig         GetFeatureCount( int );

    virtual OGRFeatureDefn *GetLayerDefn();

    virtual const char* GetName();

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( GIntBig nFID );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );

    const char*         GetTableName() {
        return pszTableName;
    }
    const char*         GetLayerName() {
        return m_pszLayerName;
    }
    const char*         GetSchemaName() {
        return pszSchemaName;
    }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRFeature *GetFeature( GIntBig nFeatureId );

    virtual int         TestCapability( const char * );

    void                SetLaunderFlag( int bFlag )
    {
        bLaunderColumnNames = bFlag;
    }
    void                SetPrecisionFlag( int bFlag )
    {
        bPreservePrecision = bFlag;
    }
    void                SetSpatialIndexFlag( int bFlag )
    {
        bNeedSpatialIndex = bFlag;
    }
    void                SetUploadGeometryFormat( int nGeometryFormat )
    {
        nUploadGeometryFormat = nGeometryFormat;
    }
    void                AppendFieldValue(CPLODBCStatement *poStatement,
                                         OGRFeature* poFeature, int i,
                                         int *bind_num, void **bind_buffer);
    int                 FetchSRSId();
};

/************************************************************************/
/*                      OGRDB2SelectLayer                      */
/************************************************************************/

class OGRDB2SelectLayer : public OGRDB2Layer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement();

public:
    OGRDB2SelectLayer( OGRDB2DataSource *,
                       CPLODBCStatement * );
    ~OGRDB2SelectLayer();

    virtual void        ResetReading();
    virtual GIntBig     GetFeatureCount( int );

    virtual OGRFeature *GetFeature( GIntBig nFeatureId );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                           OGRDB2DataSource                           */
/************************************************************************/

class OGRDB2DataSource : public GDALDataset
{
    OGRDB2TableLayer    **papoLayers;
    int                 nLayers;

    char               *pszName;

    char               *pszCatalog;

    int                 bDSUpdate;
    CPLODBCSession      oSession;

    int                 nGeometryFormat;

    int                 bUseGeometryColumns;

    int                 bListAllTables;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;

public:
    OGRDB2DataSource();
    ~OGRDB2DataSource();
    int                 DeleteLayer( OGRDB2TableLayer * poLayer );
    const char          *GetCatalog() {
        return pszCatalog;
    }

    int                 ParseValue(char** pszValue, char* pszSource,
                                   const char* pszKey,
                                   int nStart, int nNext, int nTerm,
                                   int bRemove);

    int                 Open(GDALOpenInfo* poOpenInfo);
    int                 Create( const char * pszFilename,
                                int nXSize,
                                int nYSize,
                                int nBands,
                                GDALDataType eDT,
                                char **papszOptions );
    int                 Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *pszSchemaName,
                                   const char *pszTableName,
                                   const char *pszGeomCol,
                                   int nCoordDimension,
                                   int nSRID, const char *pszSRText,
                                   OGRwkbGeometryType eType, int bUpdate );

    const char          *GetName() {
        return pszName;
    }
    int                 GetLayerCount();
    OGRLayer            *GetLayer( int );
    OGRLayer            *GetLayerByName( const char* pszLayerName );

    int                 GetGeometryFormat() {
        return nGeometryFormat;
    }
    int                 UseGeometryColumns() {
        return bUseGeometryColumns;
    }

    virtual int         DeleteLayer( int iLayer );
    virtual OGRLayer    *ICreateLayer( const char *,
                                       OGRSpatialReference * = NULL,
                                       OGRwkbGeometryType = wkbUnknown,
                                       char ** = NULL );

    int                 TestCapability( const char * );

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    char                *LaunderName( const char *pszSrcName );
    char                *ToUpper( const char *pszSrcName );
    OGRErr              InitializeMetadataTables();

    OGRSpatialReference* FetchSRS( int nId );
    int                 FetchSRSId( OGRSpatialReference * poSRS );

    OGRErr              StartTransaction(CPL_UNUSED int bForce);
    OGRErr              CommitTransaction();
    OGRErr              RollbackTransaction();
    // Internal use
    CPLODBCSession     *GetSession() {
        return &oSession;
    }

};

/************************************************************************/
/*                             OGRDB2Driver                             */
/************************************************************************/

class OGRDB2Driver : public GDALDriver
{
public:
    ~OGRDB2Driver();
};

#endif /* ifndef _OGR_DB2_H_INCLUDED */
