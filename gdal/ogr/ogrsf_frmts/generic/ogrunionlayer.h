/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRUnionLayer class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGRUNIONLAYER_H_INCLUDED
#define _OGRUNIONLAYER_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                      OGRUnionLayerGeomFieldDefn                      */
/************************************************************************/

class OGRUnionLayerGeomFieldDefn: public OGRGeomFieldDefn
{
    public:

    int             bGeomTypeSet;
    int             bSRSSet;
    OGREnvelope     sStaticEnvelope;

            OGRUnionLayerGeomFieldDefn(const char* pszName, OGRwkbGeometryType eType);
            OGRUnionLayerGeomFieldDefn(OGRGeomFieldDefn* poSrc);
            OGRUnionLayerGeomFieldDefn(OGRUnionLayerGeomFieldDefn* poSrc);
           ~OGRUnionLayerGeomFieldDefn();
};

/************************************************************************/
/*                         OGRUnionLayer                                */
/************************************************************************/

typedef enum
{
    FIELD_FROM_FIRST_LAYER,
    FIELD_UNION_ALL_LAYERS,
    FIELD_INTERSECTION_ALL_LAYERS,
    FIELD_SPECIFIED,
} FieldUnionStrategy;

class OGRUnionLayer : public OGRLayer
{
  protected:
    CPLString           osName;
    int                 nSrcLayers;
    OGRLayer          **papoSrcLayers;
    int                 bHasLayerOwnership;

    OGRFeatureDefn     *poFeatureDefn;
    int                 nFields;
    OGRFieldDefn      **papoFields;
    int                 nGeomFields;
    OGRUnionLayerGeomFieldDefn **papoGeomFields;
    FieldUnionStrategy eFieldStrategy;
    CPLString           osSourceLayerFieldName;

    int                 bPreserveSrcFID;

    int                 nFeatureCount;

    int                 iCurLayer;
    char               *pszAttributeFilter;
    int                 nNextFID;
    int                *panMap;
    char              **papszIgnoredFields;
    int                 bAttrFilterPassThroughValue;
    int                *pabModifiedLayers;
    int                *pabCheckIfAutoWrap;
    OGRSpatialReference *poGlobalSRS;

    void                AutoWarpLayerIfNecessary(int iSubLayer);
    OGRFeature         *TranslateFromSrcLayer(OGRFeature* poSrcFeature);
    void                ApplyAttributeFilterToSrcLayer(int iSubLayer);
    int                 GetAttrFilterPassThroughValue();
    void                ConfigureActiveLayer();
    void                SetSpatialFilterToSourceLayer(OGRLayer* poSrcLayer);

  public:
                        OGRUnionLayer( const char* pszName,
                                       int nSrcLayers, /* must be >= 1 */
                                       OGRLayer** papoSrcLayers, /* array itself ownership always transfered, layer ownership depending on bTakeLayerOwnership */
                                       int bTakeLayerOwnership);

    virtual             ~OGRUnionLayer();

    /* All the following non virtual methods must be called just after the constructor */
    /* and before any virtual method */
    void                SetFields(FieldUnionStrategy eFieldStrategy,
                                  int nFields,
                                  OGRFieldDefn** papoFields,  /* duplicated by the method */
                                  int nGeomFields, /* maybe -1 to explicitely disable geometry fields */
                                  OGRUnionLayerGeomFieldDefn** papoGeomFields  /* duplicated by the method */);
    void                SetSourceLayerFieldName(const char* pszSourceLayerFieldName);
    void                SetPreserveSrcFID(int bPreserveSrcFID);
    void                SetFeatureCount(int nFeatureCount);
    virtual const char  *GetName() { return osName.c_str(); }
    virtual OGRwkbGeometryType GetGeomType();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );

    virtual OGRErr      ICreateFeature( OGRFeature* poFeature );

    virtual OGRErr      ISetFeature( OGRFeature* poFeature );

    virtual OGRFeatureDefn *GetLayerDefn();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );

    virtual void        SetSpatialFilter( OGRGeometry * poGeomIn );
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );

    virtual OGRErr      SetIgnoredFields( const char **papszFields );

    virtual OGRErr      SyncToDisk();
};

#endif // _OGRUNIONLAYER_H_INCLUDED
