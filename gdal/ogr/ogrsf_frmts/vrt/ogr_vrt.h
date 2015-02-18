/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/VRT driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogrlayerpool.h"

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
/*                         OGRVRTGeomFieldProps                         */
/************************************************************************/

class OGRVRTGeomFieldProps
{
    public:
        CPLString           osName;         /* Name of the VRT geometry field */
        OGRwkbGeometryType  eGeomType;
        OGRSpatialReference *poSRS;

        int                 bSrcClip;
        OGRGeometry         *poSrcRegion;

        // Geometry interpretation related.
        OGRVRTGeometryStyle eGeometryStyle;

        /* points to a OGRField for VGS_WKT, VGS_WKB, VGS_Shape and OGRGeomField for VGS_Direct */
        int                 iGeomField;

                            // VGS_PointFromColumn
        int                 iGeomXField, iGeomYField, iGeomZField;
        int                 bReportSrcColumn;
        int                 bUseSpatialSubquery;

        OGREnvelope         sStaticEnvelope;
        
        int                 bNullable;

                        OGRVRTGeomFieldProps();
                       ~OGRVRTGeomFieldProps();
};

/************************************************************************/
/*                            OGRVRTLayer                                */
/************************************************************************/

class OGRVRTDataSource;

class OGRVRTLayer : public OGRLayer
{
  protected:
    OGRVRTDataSource*   poDS;
    std::vector<OGRVRTGeomFieldProps*> apoGeomFieldProps;

    int                 bHasFullInitialized;
    CPLString           osName;
    CPLXMLNode         *psLTree;
    CPLString           osVRTDirectory;

    OGRFeatureDefn      *poFeatureDefn;

    GDALDataset         *poSrcDS;
    OGRLayer            *poSrcLayer;
    OGRFeatureDefn      *poSrcFeatureDefn;
    int                 bNeedReset;
    int                 bSrcLayerFromSQL;
    int                 bSrcDSShared;
    int                 bAttrFilterPassThrough;

    char                *pszAttrFilter;

    int                 iFIDField; // -1 means pass through. 
    CPLString           osFIDFieldName;
    int                 iStyleField; // -1 means pass through.

    // Attribute Mapping
    std::vector<int>    anSrcField;
    std::vector<int>    abDirectCopy;

    int                 bUpdate;

    OGRFeature         *TranslateFeature( OGRFeature*& , int bUseSrcRegion );
    OGRFeature         *TranslateVRTFeatureToSrcFeature( OGRFeature* poVRTFeature);

    int                 ResetSourceReading();

    int                 FullInitialize();

    OGRFeatureDefn     *GetSrcLayerDefn();
    void                ClipAndAssignSRS(OGRFeature* poFeature);

    GIntBig             nFeatureCount;

    int                 bError;

    int                 ParseGeometryField(CPLXMLNode* psNode,
                                           CPLXMLNode* psNodeParent,
                                           OGRVRTGeomFieldProps* poProps);

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

    virtual OGRFeature *GetFeature( GIntBig nFeatureId );

    virtual OGRErr      SetNextByIndex( GIntBig nIndex );

    virtual OGRFeatureDefn *GetLayerDefn();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual GIntBig     GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent,
                                  int bForce = TRUE);

    virtual void        SetSpatialFilter( OGRGeometry * poGeomIn );
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn );

    virtual OGRErr      ICreateFeature( OGRFeature* poFeature );

    virtual OGRErr      ISetFeature( OGRFeature* poFeature );

    virtual OGRErr      DeleteFeature( GIntBig nFID );

    virtual OGRErr      SyncToDisk();

    virtual const char *GetFIDColumn();

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual OGRErr      SetIgnoredFields( const char **papszFields );

    GDALDataset*        GetSrcDataset();
};

/************************************************************************/
/*                           OGRVRTDataSource                            */
/************************************************************************/

typedef enum
{
    OGR_VRT_PROXIED_LAYER,
    OGR_VRT_LAYER,
    OGR_VRT_OTHER_LAYER,
} OGRLayerType;

class OGRVRTDataSource : public OGRDataSource
{
    OGRLayer          **papoLayers;
    OGRLayerType       *paeLayerType;
    int                 nLayers;
    
    char               *pszName;

    CPLXMLNode         *psTree;

    int                 nCallLevel;

    std::set<std::string> aosOtherDSNameSet;

    OGRLayer*           InstanciateWarpedLayer(CPLXMLNode *psLTree,
                                               const char *pszVRTDirectory,
                                               int bUpdate,
                                               int nRecLevel);
    OGRLayer*           InstanciateUnionLayer(CPLXMLNode *psLTree,
                                               const char *pszVRTDirectory,
                                               int bUpdate,
                                               int nRecLevel);

    OGRLayerPool*       poLayerPool;

    OGRVRTDataSource   *poParentDS;
    int                 bRecursionDetected;

  public:
                        OGRVRTDataSource(GDALDriver* poDriver);
                        ~OGRVRTDataSource();

    OGRLayer*           InstanciateLayer(CPLXMLNode *psLTree,
                                    const char *pszVRTDirectory,
                                    int bUpdate,
                                    int nRecLevel = 0);

    OGRLayer*           InstanciateLayerInternal(CPLXMLNode *psLTree,
                                            const char *pszVRTDirectory,
                                            int bUpdate,
                                            int nRecLevel);

    int                 Initialize( CPLXMLNode *psXML, const char *pszName,
                                    int bUpdate );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    virtual char      **GetFileList();

    /* Anti-recursion mechanism for standard Open */
    void                SetCallLevel(int nCallLevelIn) { nCallLevel = nCallLevelIn; }
    int                 GetCallLevel() { return nCallLevel; }

    void                SetParentDS(OGRVRTDataSource* poParentDSIn) { poParentDS = poParentDSIn; }
    OGRVRTDataSource*   GetParentDS() { return poParentDS; }

    void                SetRecursionDetected() { bRecursionDetected = TRUE; }
    int                 GetRecursionDetected() { return bRecursionDetected; }

    /* Anti-recursion mechanism for shared Open */
    void                AddForbiddenNames(const char* pszOtherDSName);
    int                 IsInForbiddenNames(const char* pszOtherDSName);
};

OGRwkbGeometryType OGRVRTGetGeometryType(const char* pszGType, int* pbError);

#endif /* ndef _OGR_VRT_H_INCLUDED */


