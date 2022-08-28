/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLayerPool and OGRProxiedLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGRLAYERPOOL_H_INCLUDED
#define OGRLAYERPOOL_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"

typedef OGRLayer* (*OpenLayerFunc)(void* user_data);
typedef void      (*FreeUserDataFunc)(void* user_data);

class OGRLayerPool;

/************************************************************************/
/*                      OGRAbstractProxiedLayer                         */
/************************************************************************/

class CPL_DLL OGRAbstractProxiedLayer : public OGRLayer
{
        CPL_DISALLOW_COPY_ASSIGN(OGRAbstractProxiedLayer)

        friend class OGRLayerPool;

        OGRAbstractProxiedLayer   *poPrevLayer; /* Chain to a layer that was used more recently */
        OGRAbstractProxiedLayer   *poNextLayer; /* Chain to a layer that was used less recently */

    protected:
        OGRLayerPool              *poPool;

        virtual void    CloseUnderlyingLayer() = 0;

    public:
        explicit        OGRAbstractProxiedLayer(OGRLayerPool* poPool);
        virtual        ~OGRAbstractProxiedLayer();
};

/************************************************************************/
/*                             OGRLayerPool                             */
/************************************************************************/

class CPL_DLL OGRLayerPool
{
        CPL_DISALLOW_COPY_ASSIGN(OGRLayerPool)

    protected:
        OGRAbstractProxiedLayer *poMRULayer; /* the most recently used layer */
        OGRAbstractProxiedLayer *poLRULayer; /* the least recently used layer (still opened) */
        int                     nMRUListSize; /* the size of the list */
        int                     nMaxSimultaneouslyOpened;

    public:
        explicit                OGRLayerPool(int nMaxSimultaneouslyOpened = 100);
                               ~OGRLayerPool();

        void                    SetLastUsedLayer(OGRAbstractProxiedLayer* poProxiedLayer);
        void                    UnchainLayer(OGRAbstractProxiedLayer* poProxiedLayer);

        int                     GetMaxSimultaneouslyOpened() const { return nMaxSimultaneouslyOpened; }
        int                     GetSize() const { return nMRUListSize; }
};

/************************************************************************/
/*                           OGRProxiedLayer                            */
/************************************************************************/

class OGRProxiedLayer : public OGRAbstractProxiedLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRProxiedLayer)

    OpenLayerFunc       pfnOpenLayer;
    FreeUserDataFunc    pfnFreeUserData;
    void               *pUserData;
    OGRLayer           *poUnderlyingLayer;
    OGRFeatureDefn     *poFeatureDefn;
    OGRSpatialReference *poSRS;

    int                 OpenUnderlyingLayer();

  protected:

    virtual void        CloseUnderlyingLayer() override;

  public:

                        OGRProxiedLayer(OGRLayerPool* poPool,
                                        OpenLayerFunc pfnOpenLayer,
                                        FreeUserDataFunc pfnFreeUserData,
                                        void* pUserData);
    virtual            ~OGRProxiedLayer();

    OGRLayer           *GetUnderlyingLayer();

    virtual OGRGeometry *GetSpatialFilter() override;
    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override;
    virtual OGRFeature *GetFeature( GIntBig nFID ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;

    virtual GDALDataset* GetDataset() override;
    virtual bool         GetArrowStream(struct ArrowArrayStream* out_stream,
                                        CSLConstList papszOptions = nullptr) override;

    virtual const char *GetName() override;
    virtual OGRwkbGeometryType GetGeomType() override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      DeleteField( int iField ) override;
    virtual OGRErr      ReorderFields( int* panMap ) override;
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags ) override;
    virtual OGRErr      AlterGeomFieldDefn( int iGeomField, const OGRGeomFieldDefn* poNewGeomFieldDefn, int nFlags ) override;

    virtual OGRErr      SyncToDisk() override;

    virtual OGRStyleTable *GetStyleTable() override;
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable ) override;

    virtual void        SetStyleTable(OGRStyleTable *poStyleTable) override;

    virtual OGRErr      StartTransaction() override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    virtual OGRErr      SetIgnoredFields( const char **papszFields ) override;

    virtual OGRErr      Rename(const char* pszNewName) override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif // OGRLAYERPOOL_H_INCLUDED
