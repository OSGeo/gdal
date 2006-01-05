/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the Shapefile driver to implement
 *           integration with OGR.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.19  2006/01/05 02:15:39  fwarmerdam
 * implement DeleteFeature support
 *
 * Revision 1.18  2005/02/22 12:51:56  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.17  2005/02/02 20:01:02  fwarmerdam
 * added SetNextByIndex support
 *
 * Revision 1.16  2005/01/04 03:43:41  fwarmerdam
 * added support for creating and destroying spatial indexes
 *
 * Revision 1.15  2005/01/03 22:26:21  fwarmerdam
 * updated to use spatial indexing
 *
 * Revision 1.14  2003/05/21 04:03:54  warmerda
 * expand tabs
 *
 * Revision 1.13  2003/04/21 19:03:20  warmerda
 * added SyncToDisk support
 *
 * Revision 1.12  2003/03/04 05:49:05  warmerda
 * added attribute indexing support
 *
 * Revision 1.11  2003/03/03 05:06:46  warmerda
 * implemented DeleteDataSource
 *
 * Revision 1.10  2002/06/15 00:07:23  aubin
 * mods to enable 64bit file i/o
 *
 * Revision 1.9  2002/03/27 21:04:38  warmerda
 * Added support for reading, and creating lone .dbf files for wkbNone geometry
 * layers.  Added support for creating a single .shp file instead of a directory
 * if a path ending in .shp is passed to the data source create method.
 *
 * Revision 1.8  2001/09/04 15:35:14  warmerda
 * add support for deferring geometry type selection till first feature
 *
 * Revision 1.7  2001/03/16 22:16:10  warmerda
 * added support for ESRI .prj files
 *
 * Revision 1.6  2001/03/15 04:21:50  danmo
 * Added GetExtent()
 *
 * Revision 1.5  1999/11/04 21:17:25  warmerda
 * support layer/ds creation, one ds is now many shapefiles
 *
 * Revision 1.4  1999/07/27 00:52:17  warmerda
 * added random access, write and capability methods
 *
 * Revision 1.3  1999/07/26 13:59:25  warmerda
 * added feature writing api
 *
 * Revision 1.2  1999/07/08 20:05:45  warmerda
 * added GetFeatureCount()
 *
 * Revision 1.1  1999/07/05 18:58:07  warmerda
 * New
 *
 */

#ifndef _OGRSHAPE_H_INCLUDED
#define _OGRSHAPE_H_INLLUDED

#include "ogrsf_frmts.h"
#include "shapefil.h"

/* ==================================================================== */
/*      Functions from Shape2ogr.cpp.                                   */
/* ==================================================================== */
OGRFeature *SHPReadOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                               OGRFeatureDefn * poDefn, int iShape );
OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape );
OGRFeatureDefn *SHPReadOGRFeatureDefn( const char * pszName,
                                       SHPHandle hSHP, DBFHandle hDBF );
OGRErr SHPWriteOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                           OGRFeatureDefn *poFeatureDefn,
                           OGRFeature *poFeature );

/************************************************************************/
/*                            OGRShapeLayer                             */
/************************************************************************/

class OGRShapeLayer : public OGRLayer
{
    OGRSpatialReference *poSRS;
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

    int                 bHeaderDirty;

    int                 bCheckedForQIX;
    FILE                *fpQIX;

    int                 CheckForQIX();

  public:
    OGRErr              CreateSpatialIndex( int nMaxDepth );
    OGRErr              DropSpatialIndex();

  public:
                        OGRShapeLayer( const char * pszName,
                                       SHPHandle hSHP, DBFHandle hDBF,
                                       OGRSpatialReference *poSRS,
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

    virtual OGRSpatialReference *GetSpatialRef();
    
    int                 TestCapability( const char * );
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

    int                 bSingleNewFile;

  public:
                        OGRShapeDataSource();
                        ~OGRShapeDataSource();

    int                 Open( const char *, int bUpdate, int bTestOpen,
                              int bSingleNewFile = FALSE );
    int                 OpenFile( const char *, int bUpdate, int bTestOpen );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    virtual OGRLayer    *ExecuteSQL( const char *pszStatement,
                                     OGRGeometry *poSpatialFilter,
                                     const char *pszDialect );

    int                 TestCapability( const char * );
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
