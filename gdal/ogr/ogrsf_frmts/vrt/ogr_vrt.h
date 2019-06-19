/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/VRT driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_VRT_H_INCLUDED
#define OGR_VRT_H_INCLUDED

#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogrlayerpool.h"
#include "ogrsf_frmts.h"

#include <set>
#include <string>
#include <vector>

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
        CPLString           osName;  // Name of the VRT geometry field */
        OGRwkbGeometryType  eGeomType;
        OGRSpatialReference *poSRS;

        bool                bSrcClip;
        OGRGeometry         *poSrcRegion;

        // Geometry interpretation related.
        OGRVRTGeometryStyle eGeometryStyle;

        // Points to a OGRField for VGS_WKT, VGS_WKB, VGS_Shape and OGRGeomField
        // for VGS_Direct.
        int                 iGeomField;

        // VGS_PointFromColumn
        int                 iGeomXField;
        int                 iGeomYField;
        int                 iGeomZField;
        int                 iGeomMField;
        bool                bReportSrcColumn;
        bool                bUseSpatialSubquery;
        bool                bNullable;

        OGREnvelope         sStaticEnvelope;

                        OGRVRTGeomFieldProps();
                       ~OGRVRTGeomFieldProps();
};

/************************************************************************/
/*                            OGRVRTLayer                                */
/************************************************************************/

class OGRVRTDataSource;

class OGRVRTLayer final: public OGRLayer
{
  protected:
    OGRVRTDataSource*   poDS;
    std::vector<OGRVRTGeomFieldProps*> apoGeomFieldProps;

    bool                bHasFullInitialized;
    CPLString           osName;
    CPLXMLNode         *psLTree;
    CPLString           osVRTDirectory;

    OGRFeatureDefn      *poFeatureDefn;

    GDALDataset         *poSrcDS;
    OGRLayer            *poSrcLayer;
    OGRFeatureDefn      *poSrcFeatureDefn;
    bool                bNeedReset;
    bool                bSrcLayerFromSQL;
    bool                bSrcDSShared;
    bool                bAttrFilterPassThrough;

    char                *pszAttrFilter;

    int                 iFIDField;  // -1 means pass through.
    CPLString           osFIDFieldName;
    int                 iStyleField;  // -1 means pass through.

    // Attribute mapping.
    std::vector<int>    anSrcField;
    std::vector<int>    abDirectCopy;

    bool                bUpdate;

    OGRFeature         *TranslateFeature( OGRFeature*& , int bUseSrcRegion );
    OGRFeature         *TranslateVRTFeatureToSrcFeature( OGRFeature* poVRTFeature);

    bool                ResetSourceReading();

    bool                FullInitialize();

    OGRFeatureDefn     *GetSrcLayerDefn();
    void                ClipAndAssignSRS(OGRFeature* poFeature);

    GIntBig             nFeatureCount;

    bool                bError;

    bool                ParseGeometryField( CPLXMLNode* psNode,
                                            CPLXMLNode* psNodeParent,
                                            OGRVRTGeomFieldProps* poProps );

  public:
    explicit             OGRVRTLayer( OGRVRTDataSource* poDSIn );
    virtual             ~OGRVRTLayer();

    bool               FastInitialize( CPLXMLNode *psLTree,
                                       const char *pszVRTDirectory,
                                       int bUpdate );

    virtual const char  *GetName() override { return osName.c_str(); }
    virtual OGRwkbGeometryType GetGeomType() override;

/* -------------------------------------------------------------------- */
/*      Caution : all the below methods should care of calling          */
/*      FullInitialize() if not already done                            */
/* -------------------------------------------------------------------- */

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce = TRUE ) override;
    virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent,
                                   int bForce = TRUE ) override;

    virtual void        SetSpatialFilter( OGRGeometry *poGeomIn ) override;
    virtual void        SetSpatialFilter( int iGeomField,
                                          OGRGeometry *poGeomIn ) override;

    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;

    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;

    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;

    virtual OGRErr      SyncToDisk() override;

    virtual const char *GetFIDColumn() override;

    virtual OGRErr      StartTransaction() override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual OGRErr      SetIgnoredFields( const char **papszFields ) override;

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

class OGRVRTDataSource final: public OGRDataSource
{
    OGRLayer          **papoLayers;
    OGRLayerType       *paeLayerType;
    int                 nLayers;

    char               *pszName;

    CPLXMLNode         *psTree;

    int                 nCallLevel;

    std::set<std::string> aosOtherDSNameSet;

    OGRLayer*           InstantiateWarpedLayer( CPLXMLNode *psLTree,
                                                const char *pszVRTDirectory,
                                                int bUpdate,
                                                int nRecLevel );
    OGRLayer*           InstantiateUnionLayer( CPLXMLNode *psLTree,
                                               const char *pszVRTDirectory,
                                               int bUpdate,
                                               int nRecLevel );

    OGRLayerPool*       poLayerPool;

    OGRVRTDataSource   *poParentDS;
    bool                bRecursionDetected;

  public:
    explicit            OGRVRTDataSource( GDALDriver *poDriver );
                        virtual ~OGRVRTDataSource();

    virtual int         CloseDependentDatasets() override;

    OGRLayer*           InstantiateLayer( CPLXMLNode *psLTree,
                                          const char *pszVRTDirectory,
                                          int bUpdate,
                                          int nRecLevel = 0 );

    OGRLayer*           InstantiateLayerInternal( CPLXMLNode *psLTree,
                                                  const char *pszVRTDirectory,
                                                  int bUpdate,
                                                  int nRecLevel );

    bool                Initialize( CPLXMLNode *psXML, const char *pszName,
                                    int bUpdate );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;

    virtual char      **GetFileList() override;

    // Anti-recursion mechanism for standard Open.
    void                SetCallLevel(int nCallLevelIn)
        { nCallLevel = nCallLevelIn; }
    int                 GetCallLevel() { return nCallLevel; }

    void                SetParentDS( OGRVRTDataSource* poParentDSIn )
        { poParentDS = poParentDSIn; }
    OGRVRTDataSource*   GetParentDS() { return poParentDS; }

    void                SetRecursionDetected() { bRecursionDetected = true; }
    bool                GetRecursionDetected() const
        { return bRecursionDetected; }

    // Anti-recursion mechanism for shared Open.
    void                AddForbiddenNames( const char* pszOtherDSName );
    bool                IsInForbiddenNames( const char* pszOtherDSName ) const;
};

OGRwkbGeometryType OGRVRTGetGeometryType(const char* pszGType, int* pbError);
CPLString OGRVRTGetSerializedGeometryType(OGRwkbGeometryType eGeomType);

#endif  // ndef OGR_VRT_H_INCLUDED
