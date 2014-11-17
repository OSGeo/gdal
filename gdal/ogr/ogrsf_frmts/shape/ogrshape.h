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

#ifndef _OGRSHAPE_H_INCLUDED
#define _OGRSHAPE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "shapefil.h"
#include "shp_vsi.h"
#include "ogrlayerpool.h"
#include <vector>

/* Was limited to 255 until OGR 1.10, but 254 seems to be a more */
/* conventionnal limit (http://en.wikipedia.org/wiki/Shapefile, */
/* http://www.clicketyclick.dk/databases/xbase/format/data_types.html, */
/* #5052 ) */
#define OGR_DBF_MAX_FIELD_WIDTH 254

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
/*                         OGRShapeGeomFieldDefn                        */
/************************************************************************/

class OGRShapeGeomFieldDefn: public OGRGeomFieldDefn
{
    char* pszFullName;
    int   bSRSSet;
    CPLString osPrjFile;

    public:
        OGRShapeGeomFieldDefn(const char* pszFullNameIn, OGRwkbGeometryType eType,
                              int bSRSSetIn, OGRSpatialReference *poSRSIn) :
            OGRGeomFieldDefn("", eType),
            pszFullName(CPLStrdup(pszFullNameIn)),
            bSRSSet(bSRSSetIn)
        {
            poSRS = poSRSIn;
        }

        virtual ~OGRShapeGeomFieldDefn() { CPLFree(pszFullName); }

        virtual OGRSpatialReference* GetSpatialRef();

        const CPLString& GetPrjFilename() { return osPrjFile; }
};

/************************************************************************/
/*                            OGRShapeLayer                             */
/************************************************************************/

class OGRShapeDataSource;

class OGRShapeLayer : public OGRAbstractProxiedLayer
{
    OGRShapeDataSource  *poDS;

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
    int                 bSHPNeedsRepack;

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
    
    int                 bCreateSpatialIndexAtClose;

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
                                       OGRwkbGeometryType eReqType,
                                       char ** papszCreateOptions = NULL);
                        ~OGRShapeLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    virtual OGRErr      SetNextByIndex( long nIndex );

    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              ISetFeature( OGRFeature *poFeature );
    OGRErr              DeleteFeature( long nFID );
    OGRErr              ICreateFeature( OGRFeature *poFeature );
    OGRErr              SyncToDisk();
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce);

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual int         TestCapability( const char * );
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );
    
    void                AddToFileList( CPLStringList& oFileList );
    void                CreateSpatialIndexAtClose( int bFlag ) { bCreateSpatialIndexAtClose = bFlag; }
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
    
    int                 b2GBLimit;
    
    char              **papszOpenOptions;

  public:
                        OGRShapeDataSource();
                        ~OGRShapeDataSource();

    OGRLayerPool       *GetPool() { return poPool; }

    int                 Open( GDALOpenInfo* poOpenInfo, int bTestOpen,
                              int bForceSingleFileDataSource = FALSE );
    int                 OpenFile( const char *, int bUpdate, int bTestOpen );

    virtual const char  *GetName() { return pszName; }

    virtual int          GetLayerCount();
    virtual OGRLayer    *GetLayer( int );
    virtual OGRLayer    *GetLayerByName(const char *);

    virtual OGRLayer    *ICreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    virtual OGRLayer    *ExecuteSQL( const char *pszStatement,
                                     OGRGeometry *poSpatialFilter,
                                     const char *pszDialect );

    virtual int          TestCapability( const char * );
    virtual OGRErr       DeleteLayer( int iLayer );

    virtual char      **GetFileList(void);

    void                 SetLastUsedLayer( OGRShapeLayer* poLayer );
    void                 UnchainLayer( OGRShapeLayer* poLayer );

    SHPHandle            DS_SHPOpen( const char * pszShapeFile, const char * pszAccess );
    DBFHandle            DS_DBFOpen( const char * pszDBFFile, const char * pszAccess );
    char               **GetOpenOptions() { return papszOpenOptions; }
};

#endif /* ndef _OGRSHAPE_H_INCLUDED */
