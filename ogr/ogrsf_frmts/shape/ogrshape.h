/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the Shapefile driver to implement
 *           integration with OGR.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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

#ifndef _OGRSHAPE_H_INCLUDED
#define _OGRSHAPE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "shapefil.h"
#include "ogrlayerpool.h"
#include <vector>

/* ==================================================================== */
/*      Functions from Shape2ogr.cpp.                                   */
/* ==================================================================== */
OGRFeature *SHPReadOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                               OGRFeatureDefn * poDefn, int iShape, 
                               SHPObject *psShape, const char *pszSHPEncoding );
OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape, SHPObject *psShape );
OGRFeatureDefn *SHPReadOGRFeatureDefn( const char * pszName,
                                       SHPHandle hSHP, DBFHandle hDBF,
                                       const char *pszSHPEncoding );
OGRErr SHPWriteOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                           OGRFeatureDefn *poFeatureDefn,
                           OGRFeature *poFeature, const char *pszSHPEncoding,
                           int* pbTruncationWarningEmitted );

/************************************************************************/
/*                            OGRShapeLayer                             */
/************************************************************************/

class OGRShapeDataSource;

class OGRShapeLayer : public OGRAbstractProxiedLayer
{
    OGRShapeDataSource  *poDS;
    OGRSpatialReference *poSRS; /* lazy loaded --> use GetSpatialRef() */
    int                 bSRSSet;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;
    int                 nTotalShapeCount;

    char                *pszFullName;

    SHPHandle           hSHP;
    DBFHandle           hDBF;

    int                 bUpdateAccess;

    OGRwkbGeometryType  eRequestedGeomType;
    int                 ResetGeomType( int nNewType );

    int                 ScanIndices();

    long               *panMatchingFIDs;
    int                 iMatchingFID;
    void                ClearMatchingFIDs();

    OGRGeometry        *m_poFilterGeomLastValid;
    int                 nSpatialFIDCount;
    int                *panSpatialFIDs;
    void                ClearSpatialFIDs();

    int                 bHeaderDirty;

    int                 bCheckedForQIX;
    SHPTreeDiskHandle   hQIX;
    int                 CheckForQIX();

    int                 bCheckedForSBN;
    SBNSearchHandle     hSBN;
    int                 CheckForSBN();

    int                 bSbnSbxDeleted;

    CPLString           ConvertCodePage( const char * );
    CPLString           osEncoding;

    int                 bTruncationWarningEmitted;

    int                 bHSHPWasNonNULL; /* to know if we must try to reopen a .shp */
    int                 bHDBFWasNonNULL; /* to know if we must try to reopen a .dbf */
    int                 eFileDescriptorsState; /* current state of opening of file descriptor to .shp and .dbf */
    int                 TouchLayer();
    int                 ReopenFileDescriptors();

    int                 bResizeAtClose;

    void                TruncateDBF();


  protected:

    virtual void        CloseUnderlyingLayer();

/* WARNING: each of the below public methods should start with a call to */
/* TouchLayer() and test its return value, so as to make sure that */
/* the layer is properly re-opened if necessary */

  public:
    OGRErr              CreateSpatialIndex( int nMaxDepth );
    OGRErr              DropSpatialIndex();
    OGRErr              Repack();
    OGRErr              RecomputeExtent();
    OGRErr              ResizeDBF();

    void                SetResizeAtClose( int bFlag ) { bResizeAtClose = bFlag; }

    const char         *GetFullName() { return pszFullName; }

    OGRFeature *        FetchShape(int iShapeId);
    int                 GetFeatureCountWithSpatialFilterOnly();

  public:
                        OGRShapeLayer( OGRShapeDataSource* poDSIn,
                                       const char * pszName,
                                       SHPHandle hSHP, DBFHandle hDBF,
                                       OGRSpatialReference *poSRS, int bSRSSet,
                                       int bUpdate, 
                                       OGRwkbGeometryType eReqType );
                        ~OGRShapeLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    virtual OGRErr      SetNextByIndex( long nIndex );

    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              DeleteFeature( long nFID );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    OGRErr              SyncToDisk();
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce);

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual OGRSpatialReference *GetSpatialRef();
    
    virtual int         TestCapability( const char * );
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );
};

/************************************************************************/
/*                          OGRShapeDataSource                          */
/************************************************************************/

class OGRShapeDataSource : public OGRDataSource
{
    OGRShapeLayer     **papoLayers;
    int                 nLayers;
    
    char                *pszName;

    int                 bDSUpdate;

    int                 bSingleFileDataSource;

    OGRLayerPool*       poPool;

    void                AddLayer(OGRShapeLayer* poLayer);

    std::vector<CPLString> oVectorLayerName;

  public:
                        OGRShapeDataSource();
                        ~OGRShapeDataSource();

    OGRLayerPool       *GetPool() { return poPool; }

    int                 Open( const char *, int bUpdate, int bTestOpen,
                              int bForceSingleFileDataSource = FALSE );
    int                 OpenFile( const char *, int bUpdate, int bTestOpen );

    virtual const char  *GetName() { return pszName; }

    virtual int          GetLayerCount();
    virtual OGRLayer    *GetLayer( int );
    virtual OGRLayer    *GetLayerByName(const char *);

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    virtual OGRLayer    *ExecuteSQL( const char *pszStatement,
                                     OGRGeometry *poSpatialFilter,
                                     const char *pszDialect );

    virtual int          TestCapability( const char * );
    virtual OGRErr       DeleteLayer( int iLayer );

    void                 SetLastUsedLayer( OGRShapeLayer* poLayer );
    void                 UnchainLayer( OGRShapeLayer* poLayer );
};

/************************************************************************/
/*                            OGRShapeDriver                            */
/************************************************************************/

class OGRShapeDriver : public OGRSFDriver
{
  public:
                ~OGRShapeDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    OGRErr              DeleteDataSource( const char *pszDataSource );
    
    int                 TestCapability( const char * );
};


#endif /* ndef _OGRSHAPE_H_INCLUDED */
