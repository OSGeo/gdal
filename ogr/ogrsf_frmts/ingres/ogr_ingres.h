/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Declarations for Ingres OGR Driver Classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef OGR_INGRES_H_INCLUDED
#define OGR_INGRES_H_INCLUDED

#include <iiapi.h>
#include "ogrsf_frmts.h"

class OGRIngresDataSource;

/************************************************************************/
/*                          OGRIngresStatement                          */
/************************************************************************/

class OGRIngresStatement
{
public:
    II_PTR            hConn = nullptr;
    II_PTR            hStmt = nullptr;
    II_PTR            hTransaction = nullptr;

    IIAPI_GETDESCRPARM  getDescrParm;
    IIAPI_GETCOLPARM    getColParm;
    IIAPI_DATAVALUE     *pasDataBuffer = nullptr;
    IIAPI_GETQINFOPARM  queryInfo;

    GByte             *pabyWrkBuffer = nullptr;
    char              **papszFields = nullptr;

    int               bDebug = TRUE;

    int               bHaveParm = FALSE;
    IIAPI_DT_ID       eParmType;
    int               nParmLen = 0;
    GByte            *pabyParmData = nullptr;

    explicit OGRIngresStatement( II_PTR hConn );
    ~OGRIngresStatement();

    void addInputParameter( IIAPI_DT_ID eDType, int nLength, GByte *pabyData );

    int ExecuteSQL( const char * );

    char **GetRow();
    void   DumpRow( FILE * );
    static void         ReportError( IIAPI_GENPARM *, const char * = NULL );

    int    IsColumnLong(int iCol);
    void   ClearDynamicColumns();
    void   Close();
    int    SendParms();
};

/************************************************************************/
/*                            OGRIngresLayer                             */
/************************************************************************/

class OGRIngresLayer CPL_NON_FINAL: public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    int                 iNextShapeId;

    OGRIngresDataSource    *poDS;

    CPLString           osQueryStatement;

    int                 nResultOffset;

    CPLString           osGeomColumn;
    CPLString           osIngresGeomType;

    CPLString           osFIDColumn;

    OGRIngresStatement *poResultSet; /* stmt */

    int                 FetchSRSId(OGRFeatureDefn *poDefn);
    OGRGeometry        *TranslateGeometry( const char * );

  public:
                        OGRIngresLayer();
    virtual             ~OGRIngresLayer();

    virtual void        ResetReading() override;

    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int         TestCapability( const char * ) override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    /* custom methods */
    virtual OGRFeature *RecordToFeature( char **papszRow );
    virtual OGRFeature *GetNextRawFeature();
};

/************************************************************************/
/*                          OGRIngresTableLayer                          */
/************************************************************************/

class OGRIngresTableLayer final: public OGRIngresLayer
{
    int                 bUpdateAccess;

    OGRFeatureDefn     *ReadTableDefinition(const char *);

    void                BuildWhere();
    char               *BuildFields();
    void                BuildFullQueryStatement();

    CPLString           osQuery;
    CPLString           osWHERE;

    int                 bLaunderColumnNames;
    int                 bPreservePrecision;

    OGRErr              PrepareOldStyleGeometry( OGRGeometry*, CPLString& );
    OGRErr              PrepareNewStyleGeometry( OGRGeometry*, CPLString& );

  public:
                        OGRIngresTableLayer( OGRIngresDataSource *,
                                         const char * pszName,
                                         int bUpdate, int nSRSId = -2 );
                        virtual ~OGRIngresTableLayer();

    OGRErr              Initialize(const char* pszTableName);

//    virtual OGRFeature *GetFeature( GIntBig nFeatureId );
    virtual void        ResetReading() override;
//    virtual GIntBig     GetFeatureCount( int );

    void                SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                         OGRIngresResultLayer                          */
/************************************************************************/

class OGRIngresResultLayer final: public OGRIngresLayer
{
    void                BuildFullQueryStatement();

    char                *pszRawStatement;

    int                 nFeatureCount;

  public:
                        OGRIngresResultLayer( OGRIngresDataSource *,
                                              const char * pszRawStatement,
                                              OGRIngresStatement *hStmt );
    virtual             ~OGRIngresResultLayer();

    OGRFeatureDefn     *ReadResultDefinition();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;
};

/************************************************************************/
/*                          OGRIngresDataSource                          */
/************************************************************************/

class OGRIngresDataSource final: public OGRDataSource
{
    OGRIngresLayer    **papoLayers = nullptr;
    int                 nLayers = 0;

    char               *pszName = nullptr;

    int                 bDSUpdate = FALSE;

    II_PTR              hConn = nullptr;

    int                 DeleteLayer( int iLayer ) override;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 nKnownSRID = 0;
    int                *panSRID = nullptr;
    OGRSpatialReference **papoSRS = nullptr;

    OGRIngresLayer     *poActiveLayer = nullptr; /* this layer has active transaction */

    int                 bNewIngres = FALSE; /* TRUE if new spatial library */

  public:
                        OGRIngresDataSource();
                        virtual ~OGRIngresDataSource();

    II_PTR              GetConn() { return hConn; }

    int                 FetchSRSId( OGRSpatialReference * poSRS );

    OGRSpatialReference *FetchSRS( int nSRSId );

    static OGRErr              InitializeMetadataTables();

    int                 Open( const char *pszFullName,
                              char **papszOptions, int bUpdate );
    int                 OpenTable( const char *, int bUpdate );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    virtual OGRLayer    *ICreateLayer( const char *,
                                       OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL ) override;

    int                 TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    // nonstandard

    char               *LaunderName( const char * );

    void                EstablishActiveLayer( OGRIngresLayer * );
    int                 IsNewIngres();
};

/************************************************************************/
/*                            OGRIngresDriver                            */
/************************************************************************/

class OGRIngresDriver final: public OGRSFDriver
{
    char         **ParseWrappedName( const char * );

  public:
    virtual ~OGRIngresDriver();

    const char *GetName() override;
    OGRDataSource *Open( const char *, int ) override;
    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL ) override;
    int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_PG_H_INCLUDED */
