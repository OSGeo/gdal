/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Oracle Spatial OGR Driver Declarations. 
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_OCI_H_INCLUDED
#define _OGR_OCI_H_INCLUDED

#include "ogrsf_frmts.h"
#include "oci.h"
#include "cpl_error.h"

/* -------------------------------------------------------------------- */
/*      Low level Oracle spatial declarations.                          */
/* -------------------------------------------------------------------- */
#define TYPE_OWNER                 "MDSYS"
#define SDO_GEOMETRY               "MDSYS.SDO_GEOMETRY"

typedef struct 
{
   OCINumber x;
   OCINumber y;
   OCINumber z;
} sdo_point_type;

typedef OCIArray sdo_elem_info_array;
typedef OCIArray sdo_ordinate_array;

typedef struct
{
   OCINumber      sdo_gtype;
   OCINumber      sdo_srid;
   sdo_point_type sdo_point;
   OCIArray       *sdo_elem_info;
   OCIArray       *sdo_ordinates;
} SDO_GEOMETRY_TYPE;

typedef struct
{
   OCIInd _atomic;
   OCIInd x;
   OCIInd y;
   OCIInd z;
} sdo_point_type_ind;

typedef struct
{
   OCIInd                    _atomic;
   OCIInd                    sdo_gtype;
   OCIInd                    sdo_srid;
   sdo_point_type_ind        sdo_point;
   OCIInd                    sdo_elem_info;
   OCIInd                    sdo_ordinates;
} SDO_GEOMETRY_ind;

#define ORA_GTYPE_MATCH(a,b)      ( ((a) % 100) == ((b) % 100))
#define ORA_GTYPE_UNKNOWN         0
#define ORA_GTYPE_POINT           1
#define ORA_GTYPE_LINESTRING      2    // or curve
#define ORA_GTYPE_POLYGON         3    // or surface
#define ORA_GTYPE_COLLECTION      4
#define ORA_GTYPE_MULTIPOINT      5 
#define ORA_GTYPE_MULTILINESTRING 6    // or multicurve
#define ORA_GTYPE_MULTIPOLYGON    7    // or multisurface
#define ORA_GTYPE_SOLID           8
#define ORA_GTYPE_MULTISOLID      9


/************************************************************************/
/*                            OGROCISession                             */
/************************************************************************/
class CPL_DLL OGROCISession {
  public:
    OCIEnv     *hEnv;
    OCIError   *hError;
    OCISvcCtx  *hSvcCtx;
    OCIServer  *hServer;
    OCISession *hSession;
    OCIDescribe*hDescribe;
    OCIType    *hGeometryTDO;
    OCIType    *hOrdinatesTDO;
    OCIType    *hElemInfoTDO;

    char       *pszUserid;
    char       *pszPassword;
    char       *pszDatabase;
    
  public:
             OGROCISession();
    virtual ~OGROCISession();

    int      EstablishSession( const char *pszUserid,
                               const char *pszPassword,
                               const char *pszDatabase );

    int      Failed( sword nStatus, const char *pszFunction = NULL );
        
    CPLErr   GetParmInfo( OCIParam *hParmDesc, OGRFieldDefn *poOGRDefn,
                          ub2 *pnOCIType, ub4 *pnOCILen );

    void     CleanName( char * );

    OCIType *PinTDO( const char * );

  private:
    
};

OGROCISession CPL_DLL*
OGRGetOCISession( const char *pszUserid,
                  const char *pszPassword,
                  const char *pszDatabase );

/************************************************************************/
/*                           OGROCIStatement                            */
/************************************************************************/
class CPL_DLL OGROCIStatement {
  public:
                 OGROCIStatement( OGROCISession * );
    virtual     ~OGROCIStatement();

    OCIStmt     *GetStatement() { return hStatement; }
    CPLErr       BindScalar( const char *pszPlaceName, 
                             void *pData, int nDataLen, int nSQLType,
                             sb2 *paeInd = NULL );
    CPLErr       BindObject( const char *pszPlaceName, void *pahObject,
                             OCIType *hTDO, void **papIndicators );

    char        *pszCommandText;

    CPLErr       Prepare( const char * pszStatement );
    CPLErr       Execute( const char * pszStatement,
                          int nMode = -1 );
    void         Clean();
    
    OGRFeatureDefn *GetResultDefn() { return poDefn; }

    char       **SimpleFetchRow();

  private:    
    OGROCISession *poSession;
    OCIStmt       *hStatement;

    OGRFeatureDefn*poDefn;

    char         **papszCurColumn;
    char         **papszCurImage;
    sb2          *panCurColumnInd;

    int           nRawColumnCount;
    int           *panFieldMap;
};

/************************************************************************/
/*                           OGROCIStringBuf                            */
/************************************************************************/
class OGROCIStringBuf 
{
  char *pszString;
  int  nLen;
  int  nBufSize;

  void UpdateEnd();

public:

    OGROCIStringBuf();
    ~OGROCIStringBuf();

    void MakeRoomFor( int );
    void Append( const char * );
    void Appendf( int nMax, const char *pszFormat, ... );
    char *StealString();

    char GetLast();
    char *GetEnd() { UpdateEnd(); return pszString + nLen; }
    char *GetString() { return pszString; }

    void Clear();
};

/************************************************************************/
/*                             OGROCILayer                              */
/************************************************************************/

class OGROCIDataSource;
    
class OGROCILayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    int                 iNextShapeId;

    OGROCIDataSource    *poDS;

    char               *pszQueryStatement;

    int                 nResultOffset;

    OGROCIStatement    *poStatement;

    int                 ExecuteQuery( const char * );

    SDO_GEOMETRY_TYPE  *hLastGeom;
    SDO_GEOMETRY_ind   *hLastGeomInd;

    char               *pszGeomName;
    int                iGeomColumn;

    char               *pszFIDName;
    int                iFIDColumn;

    OGRGeometry        *TranslateGeometry();
    OGRGeometry        *TranslateGeometryElement( int *piElement,
                                                  int nGType, int nDimension,
                                                  int nEType,
                                                  int nInterpretation,
                                                  int nStartOrdinal,
                                                  int nOrdCount);
    int      LoadElementInfo( int iElement, int nElemCount, int nTotalOrdCount,
                              int *pnEType, int *pnInterpretation, 
                              int *pnStartOrdinal, int *pnElemOrdCount );
    int                 GetOrdinalPoint( int iOrdinal, int nDimension,
                                         double *pdfX, double *pdfY,
                                         double *pdfZ );

  public:
                        OGROCILayer();
    virtual             ~OGROCILayer();
    virtual int         FindFieldIndex( const char *pszFieldName, int bExactMatch ) { return OGRLayer::FindFieldIndex( pszFieldName, bExactMatch ); }

    virtual void        ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int         TestCapability( const char * );

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    int                 LookupTableSRID();
};

/************************************************************************/
/*                         OGROCIWritableLayer                          */
/************************************************************************/

class OGROCIWritableLayer : public OGROCILayer
{
protected:
    int                 nDimension;
    int                 nSRID;

    int                 nOrdinalCount;
    int                 nOrdinalMax;
    double             *padfOrdinals;

    int                 nElemInfoCount;
    int                 nElemInfoMax;
    int                *panElemInfo;

    void                PushOrdinal( double );
    void                PushElemInfo( int, int, int );

    OGRErr              TranslateToSDOGeometry( OGRGeometry *,
                                                int *pnGType );
    OGRErr              TranslateElementGroup( OGRGeometry *poGeometry );

    int                 bLaunderColumnNames;
    int                 bPreservePrecision;

    OGRSpatialReference *poSRS;

    char              **papszOptions;

    int                 bTruncationReported;
    void                ReportTruncation( OGRFieldDefn * );

    void                ParseDIMINFO( const char *, double *, double *,
                                      double * );

                        OGROCIWritableLayer();
    virtual            ~OGROCIWritableLayer();
public:

    virtual OGRSpatialReference *GetSpatialRef() { return poSRS; }
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual int         FindFieldIndex( const char *pszFieldName, int bExactMatch );

    // following methods are not base class overrides
    void                SetOptions( char ** );

    void                SetDimension( int );
    void                SetLaunderFlag( int bFlag ) 
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag ) 
                                { bPreservePrecision = bFlag; }
};

/************************************************************************/
/*                          OGROCILoaderLayer                           */
/************************************************************************/

#define LDRM_UNKNOWN  0
#define LDRM_STREAM   1
#define LDRM_VARIABLE 2
#define LDRM_BINARY   3

class OGROCILoaderLayer : public OGROCIWritableLayer
{
    OGREnvelope         sExtent;
    int                 iNextFIDToWrite;

    char                *pszLoaderFilename;
    
    FILE                *fpLoader;
    int                 bHeaderWritten;

    FILE                *fpData;

    int                 nLDRMode;

    void                WriteLoaderHeader();
    void                FinalizeNewLayer();

    OGRErr              WriteFeatureStreamMode( OGRFeature * );
    OGRErr              WriteFeatureVariableMode( OGRFeature * );
    OGRErr              WriteFeatureBinaryMode( OGRFeature * );

  public:
                        OGROCILoaderLayer( OGROCIDataSource *,
                                           const char * pszName,
                                           const char *pszGeomCol, 
                                           int nSRID, 
                                           const char *pszLoaderFile );
                        ~OGROCILoaderLayer();

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * ) {}

    virtual OGRErr      SetAttributeFilter( const char * ) 
                                { return OGRERR_UNSUPPORTED_OPERATION; }

    virtual OGRFeature *GetNextFeature();

    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    
    virtual OGRSpatialReference *GetSpatialRef() { return poSRS; }

    virtual int         TestCapability( const char * );

};

/************************************************************************/
/*                           OGROCITableLayer                            */
/************************************************************************/

class OGROCITableLayer : public OGROCIWritableLayer
{
    int                 bUpdateAccess;
    int                 bNewLayer;
    OGREnvelope         sExtent;
    bool                bExtentUpdated;

    int                 iNextFIDToWrite;
    int                 bHaveSpatialIndex;

    OGRFeatureDefn     *ReadTableDefinition(const char *);

    void                BuildWhere(void);
    char               *BuildFields(void);
    void                BuildFullQueryStatement(void);

    char               *pszQuery;
    char               *pszWHERE;

    int                 bValidTable;

    CPLString           osTableName;
    CPLString           osOwner;

    OCIArray           *hOrdVARRAY;
    OCIArray           *hElemInfoVARRAY;

    void                UpdateLayerExtents();
    void                FinalizeNewLayer();
    void                CreateSpatialIndex();

    void                TestForSpatialIndex( const char * );

    OGROCIStatement   *poBoundStatement; 

    int                 nWriteCacheMax;
    int                 nWriteCacheUsed;

    SDO_GEOMETRY_TYPE  *pasWriteGeoms;
    SDO_GEOMETRY_TYPE **papsWriteGeomMap;
    SDO_GEOMETRY_ind   *pasWriteGeomInd;
    SDO_GEOMETRY_ind  **papsWriteGeomIndMap;
    
    void              **papWriteFields;
    OCIInd            **papaeWriteFieldInd;
    int                *panWriteFIDs;

    int                 AllocAndBindForWrite(int eType);
    OGRErr              FlushPendingFeatures();

    OGRErr              UnboundCreateFeature( OGRFeature *poFeature );
    OGRErr              BoundCreateFeature( OGRFeature *poFeature );

  public:
                        OGROCITableLayer( OGROCIDataSource *,
                                          const char * pszName,
                                          int nSRID, int bUpdate, int bNew );
                        ~OGROCITableLayer();

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRFeature *GetNextFeature();
    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( long nFID );
    
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * );

    virtual OGRErr      SyncToDisk();

    // following methods are not base class overrides
    int                 IsValid() { return bValidTable; }

    int                 GetMaxFID();
};

/************************************************************************/
/*                          OGROCISelectLayer                           */
/************************************************************************/

class OGROCISelectLayer : public OGROCILayer
{
    OGRFeatureDefn     *ReadTableDefinition( OGROCIStatement * poStatement );

  public:
                        OGROCISelectLayer( OGROCIDataSource *,
                                           const char * pszName,
                                           OGROCIStatement *poStatement );
                        ~OGROCISelectLayer();
};

/************************************************************************/
/*                           OGROCIDataSource                           */
/************************************************************************/

class OGROCIDataSource : public OGRDataSource
{
    OGROCILayer       **papoLayers;
    int                 nLayers;
    
    char               *pszName;
    char               *pszDBName;

    int                 bDSUpdate;

    OGROCISession      *poSession;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes. 
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;
    
  public:
                        OGROCIDataSource();
                        ~OGROCIDataSource();

    OGROCISession      *GetSession() { return poSession; }

    int                 Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *pszTableName, 
                                   int nSRID, int bUpdate, int bTestOpen );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    OGRLayer            *GetLayerByName(const char * pszName);

    virtual OGRErr      DeleteLayer(int);
    virtual OGRLayer    *ICreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );

    void                DeleteLayer( const char * );

    void                TruncateLayer( const char * );
    void                ValidateLayer( const char * );
    
    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    int                 FetchSRSId( OGRSpatialReference * poSRS );
    OGRSpatialReference *FetchSRS( int nSRID );
};

/************************************************************************/
/*                             OGROCIDriver                             */
/************************************************************************/

class OGROCIDriver : public OGRSFDriver
{
  public:
                ~OGROCIDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    
    int                 TestCapability( const char * );
};

/* -------------------------------------------------------------------- */
/*      Helper functions.                                               */
/* -------------------------------------------------------------------- */
int 
OGROCIStrokeArcToOGRGeometry_Points( double dfStartX, double dfStartY,
                                     double dfAlongX, double dfAlongY,
                                     double dfEndX, double dfEndY,
                                     double dfMaxAngleStepSizeDegrees,
                                     int bForceWholeCircle,
                                     OGRLineString *poLine );


#endif /* ndef _OGR_OCI_H_INCLUDED */
