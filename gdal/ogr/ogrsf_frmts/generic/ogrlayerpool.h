/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLayerPool and OGRProxiedLayer class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGRLAYERPOOL_H_INCLUDED
#define _OGRLAYERPOOL_H_INCLUDED

#include "ogrsf_frmts.h"

typedef OGRLayer* (*OpenLayerFunc)(void* user_data);
typedef void      (*FreeUserDataFunc)(void* user_data);

class OGRLayerPool;

/************************************************************************/
/*                      OGRAbstractProxiedLayer                         */
/************************************************************************/

class OGRAbstractProxiedLayer : public OGRLayer
{
        friend class OGRLayerPool;

        OGRAbstractProxiedLayer   *poPrevLayer; /* Chain to a layer that was used more recently */
        OGRAbstractProxiedLayer   *poNextLayer; /* Chain to a layer that was used less recently */

    protected:
        OGRLayerPool              *poPool;

        virtual void    CloseUnderlyingLayer() = 0;

    public:
                        OGRAbstractProxiedLayer(OGRLayerPool* poPool);
        virtual        ~OGRAbstractProxiedLayer();
};

/************************************************************************/
/*                             OGRLayerPool                             */
/************************************************************************/

class OGRLayerPool
{
    protected:
        OGRAbstractProxiedLayer *poMRULayer; /* the most recently used layer */
        OGRAbstractProxiedLayer *poLRULayer; /* the least recently used layer (still opened) */
        int                     nMRUListSize; /* the size of the list */
        int                     nMaxSimultaneouslyOpened;

    public:
                                OGRLayerPool(int nMaxSimultaneouslyOpened = 100);
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
    OpenLayerFunc       pfnOpenLayer;
    FreeUserDataFunc    pfnFreeUserData;
    void               *pUserData;
    OGRLayer           *poUnderlyingLayer;
    OGRFeatureDefn     *poFeatureDefn;
    OGRSpatialReference *poSRS;

    int                 OpenUnderlyingLayer();

  protected:

    virtual void        CloseUnderlyingLayer();

  public:

                        OGRProxiedLayer(OGRLayerPool* poPool,
                                        OpenLayerFunc pfnOpenLayer,
                                        FreeUserDataFunc pfnFreeUserData,
                                        void* pUserData);
    virtual            ~OGRProxiedLayer();
    
    OGRLayer           *GetUnderlyingLayer();

    virtual OGRGeometry *GetSpatialFilter();
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual OGRErr      SetNextByIndex( long nIndex );
    virtual OGRFeature *GetFeature( long nFID );
    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( long nFID );

    virtual const char *GetName();
    virtual OGRwkbGeometryType GetGeomType();
    virtual OGRFeatureDefn *GetLayerDefn();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual OGRErr      SyncToDisk();

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );

    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual OGRErr      SetIgnoredFields( const char **papszFields );
};

#endif // _OGRLAYERPOOL_H_INCLUDED
