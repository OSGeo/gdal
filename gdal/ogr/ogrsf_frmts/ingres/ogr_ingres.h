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
    II_PTR            hConn;
    II_PTR            hStmt;
    II_PTR            hTransaction;

    IIAPI_GETDESCRPARM	getDescrParm;
    IIAPI_GETCOLPARM	getColParm;
    IIAPI_DATAVALUE	*pasDataBuffer;
    IIAPI_GETQINFOPARM  queryInfo;
    
    GByte             *pabyWrkBuffer;
    char              **papszFields;

    int               bDebug;

    int               bHaveParm;
    IIAPI_DT_ID       eParmType;
    int               nParmLen;
    GByte            *pabyParmData;

    OGRIngresStatement( II_PTR hConn );
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

class OGRIngresLayer : public OGRLayer
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

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    /* custom methods */
    virtual OGRFeature *RecordToFeature( char **papszRow );
    virtual OGRFeature *GetNextRawFeature();
};

/************************************************************************/
/*                          OGRIngresTableLayer                          */
/************************************************************************/

class OGRIngresTableLayer : public OGRIngresLayer
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

    OGRErr              PrepareOldStyleGeometry( OGRGeometry*, CPLString& );
    OGRErr              PrepareNewStyleGeometry( OGRGeometry*, CPLString& );
    
  public:
                        OGRIngresTableLayer( OGRIngresDataSource *,
                                         const char * pszName,
                                         int bUpdate, int nSRSId = -2 );
                        ~OGRIngresTableLayer();

    OGRErr              Initialize(const char* pszTableName);
    
//    virtual OGRFeature *GetFeature( long nFeatureId );
    virtual void        ResetReading();
//    virtual int         GetFeatureCount( int );

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
//    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
};

/************************************************************************/
/*                         OGRIngresResultLayer                          */
/************************************************************************/

class OGRIngresResultLayer : public OGRIngresLayer
{
    void                BuildFullQueryStatement(void);

    char                *pszRawStatement;
    
    // Layer srid.
    int                 nSRSId;
    
    int                 nFeatureCount;

  public:
                        OGRIngresResultLayer( OGRIngresDataSource *,
                                              const char * pszRawStatement,
                                              OGRIngresStatement *hStmt );
    virtual             ~OGRIngresResultLayer();

    OGRFeatureDefn     *ReadResultDefinition();


    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );
};

/************************************************************************/
/*                          OGRIngresDataSource                          */
/************************************************************************/

class OGRIngresDataSource : public OGRDataSource
{
    OGRIngresLayer    **papoLayers;
    int                 nLayers;
    
    char               *pszName;

    int                 bDSUpdate;

    II_PTR              hConn;

    int                 DeleteLayer( int iLayer );

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes. 
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;

    OGRIngresLayer     *poActiveLayer; /* this layer has active transaction */

    int					bNewIngres; /* TRUE if new spatial library */

  public:
                        OGRIngresDataSource();
                        ~OGRIngresDataSource();

    II_PTR              GetConn() { return hConn; }


    int                 FetchSRSId( OGRSpatialReference * poSRS );

    OGRSpatialReference *FetchSRS( int nSRSId );

    OGRErr              InitializeMetadataTables();

    int                 Open( const char *pszFullName, 
                              char **papszOptions, int bUpdate );
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

    char               *LaunderName( const char * );

    void                EstablishActiveLayer( OGRIngresLayer * );
    int					IsNewIngres();
};

/************************************************************************/
/*                            OGRIngresDriver                            */
/************************************************************************/

class OGRIngresDriver : public OGRSFDriver
{
    char         **ParseWrappedName( const char * );

  public:
                ~OGRIngresDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    int                 TestCapability( const char * );
};


#endif /* ndef OGR_PG_H_INCLUDED */


