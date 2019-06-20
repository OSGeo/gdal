/******************************************************************************
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

#ifndef DOXYGEN_SKIP

#include "ogrlayerpool.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGRAbstractProxiedLayer()                       */
/************************************************************************/

OGRAbstractProxiedLayer::OGRAbstractProxiedLayer( OGRLayerPool* poPoolIn ) :
    poPrevLayer(nullptr),
    poNextLayer(nullptr),
    poPool(poPoolIn)
{
    CPLAssert(poPoolIn != nullptr);
}

/************************************************************************/
/*                     ~OGRAbstractProxiedLayer()                       */
/************************************************************************/

OGRAbstractProxiedLayer::~OGRAbstractProxiedLayer()
{
    /* Remove us from the list of LRU layers if necessary */
    poPool->UnchainLayer(this);
}

/************************************************************************/
/*                            OGRLayerPool()                            */
/************************************************************************/

OGRLayerPool::OGRLayerPool(int nMaxSimultaneouslyOpenedIn) :
    poMRULayer(nullptr),
    poLRULayer(nullptr),
    nMRUListSize(0),
    nMaxSimultaneouslyOpened(nMaxSimultaneouslyOpenedIn)
{}

/************************************************************************/
/*                           ~OGRLayerPool()                            */
/************************************************************************/

OGRLayerPool::~OGRLayerPool()
{
    CPLAssert( poMRULayer == nullptr );
    CPLAssert( poLRULayer == nullptr );
    CPLAssert( nMRUListSize == 0 );
}

/************************************************************************/
/*                          SetLastUsedLayer()                          */
/************************************************************************/

void OGRLayerPool::SetLastUsedLayer(OGRAbstractProxiedLayer* poLayer)
{
    /* If we are already the MRU layer, nothing to do */
    if (poLayer == poMRULayer)
        return;

    //CPLDebug("OGR", "SetLastUsedLayer(%s)", poLayer->GetName());

    if (poLayer->poPrevLayer != nullptr || poLayer->poNextLayer != nullptr)
    {
        /* Remove current layer from its current place in the list */
        UnchainLayer(poLayer);
    }
    else if (nMRUListSize == nMaxSimultaneouslyOpened)
    {
        /* If we have reached the maximum allowed number of layers */
        /* simultaneously opened, then close the LRU one that */
        /* was still active until now */
        CPLAssert(poLRULayer != nullptr);

        poLRULayer->CloseUnderlyingLayer();
        UnchainLayer(poLRULayer);
    }

    /* Put current layer on top of MRU list */
    CPLAssert(poLayer->poPrevLayer == nullptr);
    CPLAssert(poLayer->poNextLayer == nullptr);
    poLayer->poNextLayer = poMRULayer;
    if (poMRULayer != nullptr)
    {
        CPLAssert(poMRULayer->poPrevLayer == nullptr);
        poMRULayer->poPrevLayer = poLayer;
    }
    poMRULayer = poLayer;
    if (poLRULayer == nullptr)
        poLRULayer = poLayer;
    nMRUListSize ++;
}

/************************************************************************/
/*                           UnchainLayer()                             */
/************************************************************************/

void OGRLayerPool::UnchainLayer(OGRAbstractProxiedLayer* poLayer)
{
    OGRAbstractProxiedLayer* poPrevLayer = poLayer->poPrevLayer;
    OGRAbstractProxiedLayer* poNextLayer = poLayer->poNextLayer;

    CPLAssert(poPrevLayer == nullptr || poPrevLayer->poNextLayer == poLayer);
    CPLAssert(poNextLayer == nullptr || poNextLayer->poPrevLayer == poLayer);

    if (poPrevLayer != nullptr || poNextLayer != nullptr || poLayer == poMRULayer)
        nMRUListSize --;

    if (poLayer == poMRULayer)
        poMRULayer = poNextLayer;
    if (poLayer == poLRULayer)
        poLRULayer = poPrevLayer;
    if (poPrevLayer != nullptr)
        poPrevLayer->poNextLayer = poNextLayer;
    if (poNextLayer != nullptr)
        poNextLayer->poPrevLayer = poPrevLayer;
    poLayer->poPrevLayer = nullptr;
    poLayer->poNextLayer = nullptr;
}

/************************************************************************/
/*                          OGRProxiedLayer()                           */
/************************************************************************/

OGRProxiedLayer::OGRProxiedLayer( OGRLayerPool* poPoolIn,
                                  OpenLayerFunc pfnOpenLayerIn,
                                  FreeUserDataFunc pfnFreeUserDataIn,
                                  void* pUserDataIn ) :
    OGRAbstractProxiedLayer(poPoolIn),
    pfnOpenLayer(pfnOpenLayerIn),
    pfnFreeUserData(pfnFreeUserDataIn),
    pUserData(pUserDataIn),
    poUnderlyingLayer(nullptr),
    poFeatureDefn(nullptr),
    poSRS(nullptr)
{
    CPLAssert(pfnOpenLayerIn != nullptr);
}

/************************************************************************/
/*                         ~OGRProxiedLayer()                           */
/************************************************************************/

OGRProxiedLayer::~OGRProxiedLayer()
{
    delete poUnderlyingLayer;

    if( poSRS )
        poSRS->Release();

    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( pfnFreeUserData != nullptr )
        pfnFreeUserData(pUserData);
}

/************************************************************************/
/*                       OpenUnderlyingLayer()                          */
/************************************************************************/

int OGRProxiedLayer::OpenUnderlyingLayer()
{
    CPLDebug("OGR", "OpenUnderlyingLayer(%p)", this);
    CPLAssert(poUnderlyingLayer == nullptr);
    poPool->SetLastUsedLayer(this);
    poUnderlyingLayer = pfnOpenLayer(pUserData);
    if( poUnderlyingLayer == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot open underlying layer");
    }
    return poUnderlyingLayer != nullptr;
}

/************************************************************************/
/*                         CloseUnderlyingLayer()                       */
/************************************************************************/

void OGRProxiedLayer::CloseUnderlyingLayer()
{
    CPLDebug("OGR", "CloseUnderlyingLayer(%p)", this);
    delete poUnderlyingLayer;
    poUnderlyingLayer = nullptr;
}

/************************************************************************/
/*                          GetUnderlyingLayer()                        */
/************************************************************************/

OGRLayer* OGRProxiedLayer::GetUnderlyingLayer()
{
    if( poUnderlyingLayer == nullptr )
    {
        //  If the open fails, poUnderlyingLayer will still be a nullptr
        // and the user will be warned by the open call.
        // coverity[check_return]
        OpenUnderlyingLayer();
    }
    return poUnderlyingLayer;
}

/************************************************************************/
/*                          GetSpatialFilter()                          */
/************************************************************************/

OGRGeometry *OGRProxiedLayer::GetSpatialFilter()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return nullptr;
    return poUnderlyingLayer->GetSpatialFilter();
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void        OGRProxiedLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return;
    poUnderlyingLayer->SetSpatialFilter(poGeom);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void        OGRProxiedLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return;
    poUnderlyingLayer->SetSpatialFilter(iGeomField, poGeom);
}
/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr      OGRProxiedLayer::SetAttributeFilter( const char * poAttrFilter )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetAttributeFilter(poAttrFilter);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void        OGRProxiedLayer::ResetReading()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return;
    poUnderlyingLayer->ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRProxiedLayer::GetNextFeature()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return nullptr;
    return poUnderlyingLayer->GetNextFeature();
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::SetNextByIndex( GIntBig nIndex )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetNextByIndex(nIndex);
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRProxiedLayer::GetFeature( GIntBig nFID )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return nullptr;
    return poUnderlyingLayer->GetFeature(nFID);
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr      OGRProxiedLayer::ISetFeature( OGRFeature *poFeature )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetFeature(poFeature);
}

/************************************************************************/
/*                            ICreateFeature()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->CreateFeature(poFeature);
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr      OGRProxiedLayer::DeleteFeature( GIntBig nFID )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->DeleteFeature(nFID);
}

/************************************************************************/
/*                             GetName()                                */
/************************************************************************/

const char *OGRProxiedLayer::GetName()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return "";
    return poUnderlyingLayer->GetName();
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRProxiedLayer::GetGeomType()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return wkbUnknown;
    return poUnderlyingLayer->GetGeomType();
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRProxiedLayer::GetLayerDefn()
{
    if( poFeatureDefn != nullptr )
        return poFeatureDefn;

    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() )
    {
        poFeatureDefn = new OGRFeatureDefn("");
    }
    else
    {
        poFeatureDefn = poUnderlyingLayer->GetLayerDefn();
    }

    poFeatureDefn->Reference();

    return poFeatureDefn;
}

/************************************************************************/
/*                            GetSpatialRef()                           */
/************************************************************************/

OGRSpatialReference *OGRProxiedLayer::GetSpatialRef()
{
    if( poSRS != nullptr )
        return poSRS;
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return nullptr;
    OGRSpatialReference* poRet = poUnderlyingLayer->GetSpatialRef();
    if( poRet != nullptr )
    {
        poSRS = poRet;
        poSRS->Reference();
    }
    return poRet;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig         OGRProxiedLayer::GetFeatureCount( int bForce )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return 0;
    return poUnderlyingLayer->GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr      OGRProxiedLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->GetExtent(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr      OGRProxiedLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int         OGRProxiedLayer::TestCapability( const char * pszCapability )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return FALSE;
    return poUnderlyingLayer->TestCapability(pszCapability);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr      OGRProxiedLayer::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->CreateField(poField, bApproxOK);
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr      OGRProxiedLayer::DeleteField( int iField )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->DeleteField(iField);
}

/************************************************************************/
/*                            ReorderFields()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::ReorderFields( int* panMap )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->ReorderFields(panMap);
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

/************************************************************************/
/*                            SyncToDisk()                              */
/************************************************************************/

OGRErr      OGRProxiedLayer::SyncToDisk()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SyncToDisk();
}

/************************************************************************/
/*                           GetStyleTable()                            */
/************************************************************************/

OGRStyleTable *OGRProxiedLayer::GetStyleTable()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return nullptr;
    return poUnderlyingLayer->GetStyleTable();
}

/************************************************************************/
/*                       SetStyleTableDirectly()                        */
/************************************************************************/

void        OGRProxiedLayer::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return;
    return poUnderlyingLayer->SetStyleTableDirectly(poStyleTable);
}

/************************************************************************/
/*                           SetStyleTable()                            */
/************************************************************************/

void        OGRProxiedLayer::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return;
    return poUnderlyingLayer->SetStyleTable(poStyleTable);
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr      OGRProxiedLayer::StartTransaction()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->StartTransaction();
}

/************************************************************************/
/*                          CommitTransaction()                         */
/************************************************************************/

OGRErr      OGRProxiedLayer::CommitTransaction()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr      OGRProxiedLayer::RollbackTransaction()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->RollbackTransaction();
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRProxiedLayer::GetFIDColumn()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return "";
    return poUnderlyingLayer->GetFIDColumn();
}

/************************************************************************/
/*                          GetGeometryColumn()                         */
/************************************************************************/

const char *OGRProxiedLayer::GetGeometryColumn()
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return "";
    return poUnderlyingLayer->GetGeometryColumn();
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr      OGRProxiedLayer::SetIgnoredFields( const char **papszFields )
{
    if( poUnderlyingLayer == nullptr && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetIgnoredFields(papszFields);
}

#endif /* #ifndef DOXYGEN_SKIP */
