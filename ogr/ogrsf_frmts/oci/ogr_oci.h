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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.8  2003/01/14 15:31:08  warmerda
 * added fallback support if no spatial index available
 *
 * Revision 1.7  2003/01/14 15:09:28  warmerda
 * additions to OGROCITableLayer
 *
 * Revision 1.6  2003/01/10 22:31:19  warmerda
 * various additions
 *
 * Revision 1.5  2003/01/07 22:24:35  warmerda
 * added SRS support
 *
 * Revision 1.4  2003/01/07 21:12:59  warmerda
 * Move GetFeature() to OGROCITableLayer
 *
 * Revision 1.3  2003/01/06 17:57:44  warmerda
 * lots of updates
 *
 * Revision 1.2  2002/12/28 04:38:36  warmerda
 * converted to unix file conventions
 *
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

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

#define ORA_GTYPE_MATCH(a,b)      ( ((a) % 1000) == ((b) % 1000))
#define ORA_GTYPE_UNKNOWN         0
#define ORA_GTYPE_POINT           1
#define ORA_GTYPE_LINESTRING      2
#define ORA_GTYPE_POLYGON         3
#define ORA_GTYPE_COLLECTION      4
#define ORA_GTYPE_MULTIPOINT      5
#define ORA_GTYPE_MULTILINESTRING 6
#define ORA_GTYPE_MULTIPOLYGON    7

/************************************************************************/
/*                            OGROCISession                             */
/************************************************************************/
class CPL_DLL OGROCISession {
  public:
    OCIEnv     *hEnv;
    OCIError   *hError;
    OCISvcCtx  *hSvcCtx;
    OCIDescribe*hDescribe;
    OCIType    *hGeometryTDO;
    OCIType    *hOrdinatesTDO;

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

    OGRGeometry		*poFilterGeom;

    int			iNextShapeId;

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
    OGRGeometry        *TranslateGeometryElement( int nGType, int nDimension,
                                                  int nEType,
                                                  int nInterpretation,
                                                  int nStartOrdinal,
                                                  int nOrdCount);
    int                 GetOrdinalPoint( int iOrdinal, int nDimension,
                                         double *pdfX, double *pdfY,
                                         double *pdfZ );

  public:
    			OGROCILayer();
    virtual             ~OGROCILayer();

    virtual void	ResetReading();
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRGeometry *GetSpatialFilter() { return poFilterGeom; }
    virtual void	SetSpatialFilter( OGRGeometry * );

    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                           OGROCITableLayer                            */
/************************************************************************/

class OGROCITableLayer : public OGROCILayer
{
    int			bUpdateAccess;
    int                 nDimension;
    int                 bNewLayer;
    OGREnvelope         sExtent;
    int                 iNextFIDToWrite;
    int                 bHaveSpatialIndex;

    OGRFeatureDefn     *ReadTableDefinition(const char *);

    void		BuildWhere(void);
    char 	       *BuildFields(void);
    void                BuildFullQueryStatement(void);

    char	        *pszQuery;
    char		*pszWHERE;

    int			bLaunderColumnNames;
    int			bPreservePrecision;
    int                 bValidTable;

    OGRSpatialReference *poSRS;
    int                 nSRID;

    int                 nOrdinalCount;
    int                 nOrdinalMax;
    double             *padfOrdinals;

    OCIArray           *hOrdVARRAY;

    char              **papszOptions;

    void                PushOrdinal( double );

    char               *TranslateToSDOGeometry( OGRGeometry * );
    OGRErr              TranslateElementGroup( OGRGeometry *poGeometry,
                                               OGROCIStringBuf *poElemInfo );

    void                ParseDIMINFO( const char *, double *, double *,
                                      double * );
    void                FinalizeNewLayer();

    void                TestForSpatialIndex( const char * );
    
  public:
    			OGROCITableLayer( OGROCIDataSource *,
                                          const char * pszName,
                                          const char *pszGeomCol, 
                                          int nSRID, int bUpdate, int bNew );
    			~OGROCITableLayer();

    virtual void	ResetReading();
    virtual int         GetFeatureCount( int );

    virtual OGRGeometry *GetSpatialFilter() { return poFilterGeom; }
    virtual void	SetSpatialFilter( OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRFeature *GetNextFeature();
    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRSpatialReference *GetSpatialRef() { return poSRS; }

    virtual int         TestCapability( const char * );

    // following methods are not base class overrides
    void                SetOptions( char ** );

    int                 IsValid() { return bValidTable; }
    void                SetDimension( int );
    void		SetLaunderFlag( int bFlag ) 
				{ bLaunderColumnNames = bFlag; }
    void		SetPrecisionFlag( int bFlag ) 
				{ bPreservePrecision = bFlag; }
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
    int			nLayers;
    
    char	       *pszName;
    char               *pszDBName;

    int			bDSUpdate;

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

    int			Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *pszTableName, 
                                   const char *pszGeomCol,
                                   int nSRID, int bUpdate, int bTestOpen );

    const char	        *GetName() { return pszName; }
    int			GetLayerCount() { return nLayers; }
    OGRLayer		*GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );

    void                DeleteLayer( const char * );
    
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

#endif /* ndef _OGR_OCI_H_INCLUDED */
