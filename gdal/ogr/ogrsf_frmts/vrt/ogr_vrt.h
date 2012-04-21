/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/VRT driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_VRT_H_INCLUDED
#define _OGR_VRT_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include "cpl_minixml.h"

#include <vector>
#include <string>
#include <set>

typedef enum { 
    VGS_None,
    VGS_Direct,
    VGS_PointFromColumns, 
    VGS_WKT, 
    VGS_WKB,
    VGS_Shape
} OGRVRTGeometryStyle;

/************************************************************************/
/*                            OGRVRTLayer                                */
/************************************************************************/

class OGRVRTDataSource;

class OGRVRTLayer : public OGRLayer
{
  protected:
    OGRVRTDataSource*   poDS;

    int                 bHasFullInitialized;
    CPLString           osName;
    OGRwkbGeometryType  eGeomType;
    CPLXMLNode         *psLTree;
    CPLString           osVRTDirectory;

    OGRFeatureDefn      *poFeatureDefn;

    OGRDataSource       *poSrcDS;
    OGRLayer            *poSrcLayer;
    OGRFeatureDefn      *poSrcFeatureDefn;
    int                 bNeedReset;
    int                 bSrcLayerFromSQL;
    int                 bSrcDSShared;
    int                 bAttrFilterPassThrough;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;

    char                *pszAttrFilter;

    int                 bSrcClip;
    OGRGeometry         *poSrcRegion;

    int                 iFIDField; // -1 means pass through. 
    int                 iStyleField; // -1 means pass through.

    // Geometry interpretation related.
    OGRVRTGeometryStyle eGeometryStyle;
    
    int                 iGeomField; 

                        // VGS_PointFromColumn
    int                 iGeomXField, iGeomYField, iGeomZField;

    int                 bUseSpatialSubquery;

    // Attribute Mapping
    std::vector<int>    anSrcField;
    std::vector<int>    abDirectCopy;

    int                 bUpdate;

    OGRFeature         *TranslateFeature( OGRFeature*& , int bUseSrcRegion );
    OGRErr              createFromShapeBin( GByte *, OGRGeometry **, int );
    
    OGRFeature         *TranslateVRTFeatureToSrcFeature( OGRFeature* poVRTFeature);

    int                 ResetSourceReading();

    int                 FullInitialize();

    OGRFeatureDefn     *GetSrcLayerDefn();
    void                ClipAndAssignSRS(OGRFeature* poFeature);

  public:
                        OGRVRTLayer(OGRVRTDataSource* poDSIn);
    virtual             ~OGRVRTLayer();

    int                FastInitialize( CPLXMLNode *psLTree,
                                    const char *pszVRTDirectory,
                                    int bUpdate);

    virtual const char  *GetName() { return osName.c_str(); }
    virtual OGRwkbGeometryType GetGeomType();

/* -------------------------------------------------------------------- */
/*      Caution : all the below methods should care of calling          */
/*      FullInitialize() if not already done                            */
/* -------------------------------------------------------------------- */
    
    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual OGRErr      SetNextByIndex( long nIndex );

    virtual OGRFeatureDefn *GetLayerDefn();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );

    virtual void        SetSpatialFilter( OGRGeometry * poGeomIn );

    virtual OGRErr      CreateFeature( OGRFeature* poFeature );

    virtual OGRErr      SetFeature( OGRFeature* poFeature );

    virtual OGRErr      DeleteFeature( long nFID );

    virtual OGRErr      SyncToDisk();
};

/************************************************************************/
/*                           OGRVRTDataSource                            */
/************************************************************************/

class OGRVRTDataSource : public OGRDataSource
{
    OGRVRTLayer        **papoLayers;
    int                 nLayers;
    
    char               *pszName;

    CPLXMLNode         *psTree;

    int                 nCallLevel;

    std::set<std::string> aosOtherDSNameSet;

  public:
                        OGRVRTDataSource();
                        ~OGRVRTDataSource();

    int                 Initialize( CPLXMLNode *psXML, const char *pszName,
                                    int bUpdate );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    /* Anti-recursion mechanism for standard Open */
    void                SetCallLevel(int nCallLevelIn) { nCallLevel = nCallLevelIn; }
    int                 GetCallLevel() { return nCallLevel; }

    /* Anti-recursion mechanism for shared Open */
    void                AddForbiddenNames(const char* pszOtherDSName);
    int                 IsInForbiddenNames(const char* pszOtherDSName);
};

/************************************************************************/
/*                             OGRVRTDriver                             */
/************************************************************************/

class OGRVRTDriver : public OGRSFDriver
{
  public:
                ~OGRVRTDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int         TestCapability( const char * );
};


#endif /* ndef _OGR_VRT_H_INCLUDED */


