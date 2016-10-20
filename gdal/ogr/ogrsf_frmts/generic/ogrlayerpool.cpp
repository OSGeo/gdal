/******************************************************************************
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

#ifndef DOXYGEN_SKIP

#include "ogrlayerpool.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGRAbstractProxiedLayer()                       */
/************************************************************************/

OGRAbstractProxiedLayer::OGRAbstractProxiedLayer( OGRLayerPool* poPoolIn ) :
    poPrevLayer(NULL),
    poNextLayer(NULL),
    poPool(poPoolIn)
{
    CPLAssert(poPoolIn != NULL);
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
    poMRULayer(NULL),
    poLRULayer(NULL),
    nMRUListSize(0),
    nMaxSimultaneouslyOpened(nMaxSimultaneouslyOpenedIn)
{}

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

    CPLAssert(poPrevLayer == NULL || poPrevLayer->poNextLayer == poLayer);
    CPLAssert(poNextLayer == NULL || poNextLayer->poPrevLayer == poLayer);

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

OGRProxiedLayer::OGRProxiedLayer( OGRLayerPool* poPoolIn,
                                  OpenLayerFunc pfnOpenLayerIn,
                                  FreeUserDataFunc pfnFreeUserDataIn,
                                  void* pUserDataIn ) :
    OGRAbstractProxiedLayer(poPoolIn),
    pfnOpenLayer(pfnOpenLayerIn),
    pfnFreeUserData(pfnFreeUserDataIn),
    pUserData(pUserDataIn),
    poUnderlyingLayer(NULL),
    poFeatureDefn(NULL),
    poSRS(NULL)
{
    CPLAssert(pfnOpenLayerIn != NULL);
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
/*                          GetUnderlyingLayer()                        */
/************************************************************************/

OGRLayer* OGRProxiedLayer::GetUnderlyingLayer()
{
    if( poUnderlyingLayer == NULL )
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
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return NULL;
    return poUnderlyingLayer->GetSpatialFilter();
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void        OGRProxiedLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return;
    poUnderlyingLayer->SetSpatialFilter(poGeom);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void        OGRProxiedLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return;
    poUnderlyingLayer->SetSpatialFilter(iGeomField, poGeom);
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

OGRErr      OGRProxiedLayer::SetNextByIndex( GIntBig nIndex )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetNextByIndex(nIndex);
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRProxiedLayer::GetFeature( GIntBig nFID )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return NULL;
    return poUnderlyingLayer->GetFeature(nFID);
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr      OGRProxiedLayer::ISetFeature( OGRFeature *poFeature )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->SetFeature(poFeature);
}

/************************************************************************/
/*                            ICreateFeature()                           */
/************************************************************************/

OGRErr      OGRProxiedLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->CreateFeature(poFeature);
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr      OGRProxiedLayer::DeleteFeature( GIntBig nFID )
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

GIntBig         OGRProxiedLayer::GetFeatureCount( int bForce )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return 0;
    return poUnderlyingLayer->GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr      OGRProxiedLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->GetExtent(iGeomField, psExtent, bForce);
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

OGRErr      OGRProxiedLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn )
{
    if( poUnderlyingLayer == NULL && !OpenUnderlyingLayer() ) return OGRERR_FAILURE;
    return poUnderlyingLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
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

#endif /* #ifndef DOXYGEN_SKIP */
