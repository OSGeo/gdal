/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLayerPool and OGRProxiedLayer class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogrlayerpool.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGRAbstractProxiedLayer()                       */
/************************************************************************/

OGRAbstractProxiedLayer::OGRAbstractProxiedLayer(OGRLayerPool* poPool)
{
    CPLAssert(poPool != NULL);
    this->poPool = poPool;
    poPrevLayer = NULL;
    poNextLayer = NULL;
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

OGRLayerPool::OGRLayerPool(int nMaxSimultaneouslyOpened)
{
    poMRULayer = NULL;
    poLRULayer = NULL;
    nMRUListSize = 0;
    this->nMaxSimultaneouslyOpened = nMaxSimultaneouslyOpened;
}

/************************************************************************/
/*                           ~OGRLayerPool()                            */
/************************************************************************/

OGRLayerPool::~OGRLayerPool()
{
    CPLAssert( poMRULayer == NULL );
    CPLAssert( poLRULayer == NULL );
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

    if (poLayer->poPrevLayer != NULL || poLayer->poNextLayer != NULL)
    {
        /* Remove current layer from its current place in the list */
        UnchainLayer(poLayer);
    }
    else if (nMRUListSize == nMaxSimultaneouslyOpened)
    {
        /* If we have reached the maximum allowed number of layers */
        /* simultaneously opened, then close the LRU one that */
        /* was still active until now */
        CPLAssert(poLRULayer != NULL);

        poLRULayer->CloseUnderlyingLayer();
        UnchainLayer(poLRULayer);
    }

    /* Put current layer on top of MRU list */
    CPLAssert(poLayer->poPrevLayer == NULL);
    CPLAssert(poLayer->poNextLayer == NULL);
    poLayer->poNextLayer = poMRULayer;
    if (poMRULayer != NULL)
    {
        CPLAssert(poMRULayer->poPrevLayer == NULL);
        poMRULayer->poPrevLayer = poLayer;
    }
    poMRULayer = poLayer;
    if (poLRULayer == NULL)
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

    if (poPrevLayer != NULL)
        CPLAssert(poPrevLayer->poNextLayer == poLayer);
    if (poNextLayer != NULL)
        CPLAssert(poNextLayer->poPrevLayer == poLayer);

    if (poPrevLayer != NULL || poNextLayer != NULL || poLayer == poMRULayer)
        nMRUListSize --;

    if (poLayer == poMRULayer)
        poMRULayer = poNextLayer;
    if (poLayer == poLRULayer)
        poLRULayer = poPrevLayer;
    if (poPrevLayer != NULL)
        poPrevLayer->poNextLayer = poNextLayer;
    if (poNextLayer != NULL)
        poNextLayer->poPrevLayer = poPrevLayer;
    poLayer->poPrevLayer = NULL;
    poLayer->poNextLayer = NULL;
}



/************************************************************************/
/*                          OGRProxiedLayer()                           */
/************************************************************************/

OGRProxiedLayer::OGRProxiedLayer(OGRLayerPool* poPool,
                                 OpenLayerFunc pfnOpenLayer,
                                 FreeUserDataFunc pfnFreeUserData,
                                 void* pUserData) : OGRAbstractProxiedLayer(poPool)
{
    CPLAssert(pfnOpenLayer != NULL);

    this->pfnOpenLayer = pfnOpenLayer;
    this->pfnFreeUserData = pfnFreeUserData;
    this->pUserData = pUserData;
    poUnderlyingLayer = NULL;
    poFeatureDefn = NULL;
    poSRS = NULL;
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

    if( pfnFreeUserData != NULL )
        pfnFreeUserData(pUserData);
}

/************************************************************************/
/*                       OpenUnderlyingLayer()                          */
/************************************************************************/

int OGRProxiedLayer::OpenUnderlyingLayer()
{
    CPLDebug("OGR", "OpenUnderlyingLayer(%p)", this);
    CPLAssert(poUnderlyingLayer == NULL);
    poPool->SetLastUsedLayer(this);
    poUnderlyingLayer = pfnOpenLayer(pUserData);
    if( poUnderlyingLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot open underlying layer");
    }
    return poUnderlyingLayer != NULL;
}

/************************************************************************/
/*                         CloseUnderlyingLayer()                       */
/************************************************************************/

void OGRProxiedLayer::CloseUnderlyingLayer()
{
    CPLDebug("OGR", "CloseUnderlyingLayer(%p)", this);
    delete poUnderlyingLayer;
    poUnderlyingLayer = NULL;
}

/************************************************************************/
/*                          GetSpatialFilter()                          */
/************************************************************************/

OGRGeometry *OGRProxiedLayer::GetSpatialFilter()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return NULL;
    return poUnderlyingLayer->GetSpatialFilter();
}

/************************************************************************/
/*                          GetSpatialFilter()                          */
/************************************************************************/

void        OGRProxiedLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return;
    poUnderlyingLayer->SetSpatialFilter(poGeom);
}

/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr      OGRProxiedLayer::SetAttributeFilter( const char * poAttrFilter )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetAttributeFilter(poAttrFilter);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void        OGRProxiedLayer::ResetReading()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return;
    poUnderlyingLayer->ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRProxiedLayer::GetNextFeature()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return NULL;
    return poUnderlyingLayer->GetNextFeature();
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::SetNextByIndex( long nIndex )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetNextByIndex(nIndex);
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRProxiedLayer::GetFeature( long nFID )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return NULL;
    return poUnderlyingLayer->GetFeature(nFID);
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr      OGRProxiedLayer::SetFeature( OGRFeature *poFeature )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetFeature(poFeature);
}

/************************************************************************/
/*                            CreateFeature()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::CreateFeature( OGRFeature *poFeature )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->CreateFeature(poFeature);
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr      OGRProxiedLayer::DeleteFeature( long nFID )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->DeleteFeature(nFID);
}

/************************************************************************/
/*                             GetName()                                */
/************************************************************************/

const char *OGRProxiedLayer::GetName()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return "";
    return poUnderlyingLayer->GetName();
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRProxiedLayer::GetGeomType()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return wkbUnknown;
    return poUnderlyingLayer->GetGeomType();
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRProxiedLayer::GetLayerDefn()
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() )
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
    if( poSRS != NULL )
        return poSRS;
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return NULL;
    OGRSpatialReference* poRet = poUnderlyingLayer->GetSpatialRef();
    if( poRet != NULL )
    {
        poSRS = poRet;
        poSRS->Reference();
    }
    return poRet;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int         OGRProxiedLayer::GetFeatureCount( int bForce )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return 0;
    return poUnderlyingLayer->GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr      OGRProxiedLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int         OGRProxiedLayer::TestCapability( const char * pszCapability )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return FALSE;
    return poUnderlyingLayer->TestCapability(pszCapability);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr      OGRProxiedLayer::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->CreateField(poField, bApproxOK);
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr      OGRProxiedLayer::DeleteField( int iField )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->DeleteField(iField);
}

/************************************************************************/
/*                            ReorderFields()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::ReorderFields( int* panMap )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->ReorderFields(panMap);
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlags);
}

/************************************************************************/
/*                            SyncToDisk()                              */
/************************************************************************/

OGRErr      OGRProxiedLayer::SyncToDisk()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SyncToDisk();
}

/************************************************************************/
/*                           GetStyleTable()                            */
/************************************************************************/

OGRStyleTable *OGRProxiedLayer::GetStyleTable()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return NULL;
    return poUnderlyingLayer->GetStyleTable();
}

/************************************************************************/
/*                       SetStyleTableDirectly()                        */
/************************************************************************/

void        OGRProxiedLayer::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return;
    return poUnderlyingLayer->SetStyleTableDirectly(poStyleTable);
}

/************************************************************************/
/*                           SetStyleTable()                            */
/************************************************************************/

void        OGRProxiedLayer::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return;
    return poUnderlyingLayer->SetStyleTable(poStyleTable);
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr      OGRProxiedLayer::StartTransaction()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->StartTransaction();
}

/************************************************************************/
/*                          CommitTransaction()                         */
/************************************************************************/

OGRErr      OGRProxiedLayer::CommitTransaction()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr      OGRProxiedLayer::RollbackTransaction()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->RollbackTransaction();
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRProxiedLayer::GetFIDColumn()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return "";
    return poUnderlyingLayer->GetFIDColumn();
}

/************************************************************************/
/*                          GetGeometryColumn()                         */
/************************************************************************/

const char *OGRProxiedLayer::GetGeometryColumn()
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return "";
    return poUnderlyingLayer->GetGeometryColumn();
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr      OGRProxiedLayer::SetIgnoredFields( const char **papszFields )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetIgnoredFields(papszFields);
}

