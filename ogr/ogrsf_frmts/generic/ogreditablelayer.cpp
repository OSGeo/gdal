/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGREditableLayer class
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#include "ogreditablelayer.h"
#include "../mem/ogr_mem.h"

#include <map>

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  ~IOGREditableLayerSynchronizer()                    */
/************************************************************************/

IOGREditableLayerSynchronizer::~IOGREditableLayerSynchronizer() {}

/************************************************************************/
/*                          OGREditableLayer()                          */
/************************************************************************/

OGREditableLayer::OGREditableLayer(
    OGRLayer* poDecoratedLayer,
    bool bTakeOwnershipDecoratedLayer,
    IOGREditableLayerSynchronizer* poSynchronizer,
    bool bTakeOwnershipSynchronizer) :
    OGRLayerDecorator(poDecoratedLayer,
                      bTakeOwnershipDecoratedLayer),
    m_poSynchronizer(poSynchronizer),
    m_bTakeOwnershipSynchronizer(bTakeOwnershipSynchronizer),
    m_poEditableFeatureDefn(poDecoratedLayer->GetLayerDefn()->Clone()),
    m_nNextFID(0),
    m_poMemLayer(new OGRMemLayer( "", nullptr, wkbNone )),
    m_bStructureModified(false),
    m_bSupportsCreateGeomField(false),
    m_bSupportsCurveGeometries(false)
{
    m_poEditableFeatureDefn->Reference();

    for( int i = 0; i < m_poEditableFeatureDefn->GetFieldCount(); i++ )
        m_poMemLayer->CreateField(m_poEditableFeatureDefn->GetFieldDefn(i));

    for( int i = 0; i < m_poEditableFeatureDefn->GetGeomFieldCount(); i++ )
        m_poMemLayer->
            CreateGeomField(m_poEditableFeatureDefn->GetGeomFieldDefn(i));

    m_oIter = m_oSetCreated.begin();
}

/************************************************************************/
/*                         ~OGREditableLayer()                          */
/************************************************************************/

OGREditableLayer::~OGREditableLayer()
{
    OGREditableLayer::SyncToDisk();

    m_poEditableFeatureDefn->Release();
    delete m_poMemLayer;
    if( m_bTakeOwnershipSynchronizer )
        delete m_poSynchronizer;
}

/************************************************************************/
/*                           SetNextFID()                               */
/************************************************************************/

void OGREditableLayer::SetNextFID(GIntBig nNextFID)
{
    m_nNextFID = nNextFID;
}

/************************************************************************/
/*                       SetSupportsCurveGeometries()                   */
/************************************************************************/

void OGREditableLayer::SetSupportsCurveGeometries(bool bSupportsCurveGeometries)
{
    m_bSupportsCurveGeometries = bSupportsCurveGeometries;
}

/************************************************************************/
/*                       SetSupportsCreateGeomField()                   */
/************************************************************************/

void OGREditableLayer::SetSupportsCreateGeomField(bool bSupportsCreateGeomField)
{
    m_bSupportsCreateGeomField = bSupportsCreateGeomField;
}

/************************************************************************/
/*                           DetectNextFID()                            */
/************************************************************************/

void OGREditableLayer::DetectNextFID()
{
    if( m_nNextFID > 0 )
        return;
    m_nNextFID = 0;
    m_poDecoratedLayer->ResetReading();
    OGRFeature* poFeat = nullptr;
    while( (poFeat = m_poDecoratedLayer->GetNextFeature()) != nullptr )
    {
        if( poFeat->GetFID() > m_nNextFID )
            m_nNextFID = poFeat->GetFID();
        delete poFeat;
    }
    m_nNextFID++;
}

/************************************************************************/
/*                         GetSrcGeomFieldIndex()                       */
/************************************************************************/

int OGREditableLayer::GetSrcGeomFieldIndex(int iGeomField)
{
    if( m_poDecoratedLayer == nullptr ||
        iGeomField < 0 ||
        iGeomField >= m_poEditableFeatureDefn->GetGeomFieldCount() )
    {
        return -1;
    }
    OGRGeomFieldDefn* poGeomFieldDefn =
                m_poEditableFeatureDefn->GetGeomFieldDefn(iGeomField);
    return m_poDecoratedLayer->GetLayerDefn()->GetGeomFieldIndex(
                                                poGeomFieldDefn->GetNameRef());
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void        OGREditableLayer::ResetReading()
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->ResetReading();
    m_oIter = m_oSetCreated.begin();
}

/************************************************************************/
/*                             Translate()                              */
/************************************************************************/

OGRFeature* OGREditableLayer::Translate(OGRFeatureDefn* poTargetDefn,
                                        OGRFeature* poSrcFeature,
                                        bool bCanStealSrcFeature,
                                        bool bHideDeletedFields)
{
    if( poSrcFeature == nullptr )
        return nullptr;
    OGRFeature* poRet = new OGRFeature(poTargetDefn);

    std::map<CPLString, int> oMapTargetFieldNameToIdx;
    std::map<CPLString, int>* poMap = &oMapTargetFieldNameToIdx;
    if( poTargetDefn == m_poEditableFeatureDefn &&
        !m_oMapEditableFDefnFieldNameToIdx.empty() )
    {
        poMap = &m_oMapEditableFDefnFieldNameToIdx;
    }
    else
    {
        for( int iField = 0; iField < poTargetDefn->GetFieldCount(); iField++ )
        {
            oMapTargetFieldNameToIdx[
                poTargetDefn->GetFieldDefn(iField)->GetNameRef()] = iField;
        }
        if( poTargetDefn == m_poEditableFeatureDefn )
            m_oMapEditableFDefnFieldNameToIdx = oMapTargetFieldNameToIdx;
    }

    int* panMap = static_cast<int *>(CPLMalloc( sizeof(int) * poSrcFeature->GetFieldCount() ));
    for( int iField = 0; iField < poSrcFeature->GetFieldCount(); iField++ )
    {
        const char* pszFieldName = poSrcFeature->GetFieldDefnRef(iField)->GetNameRef();
        if( bHideDeletedFields &&
            m_oSetDeletedFields.find(pszFieldName) != m_oSetDeletedFields.end() )
        {
            panMap[iField] = -1;
        }
        else
        {
            auto oIter = poMap->find(pszFieldName);
            panMap[iField] =
                (oIter == poMap->end()) ? -1 : oIter->second;
        }
    }
    poRet->SetFieldsFrom( poSrcFeature, panMap, TRUE );
    CPLFree(panMap);

    for( int i=0; i < poTargetDefn->GetGeomFieldCount(); i++ )
    {
        OGRGeomFieldDefn* poGeomField = poTargetDefn->GetGeomFieldDefn(i);
        int iSrcGeomFieldIdx = poTargetDefn->GetGeomFieldIndex(
                                                poGeomField->GetNameRef());
        if( iSrcGeomFieldIdx >= 0 )
        {
            if( bCanStealSrcFeature )
            {
                poRet->SetGeomFieldDirectly( i,
                         poSrcFeature->StealGeometry(iSrcGeomFieldIdx) );
            }
            else
            {
                poRet->SetGeomField( i,
                         poSrcFeature->GetGeomFieldRef(iSrcGeomFieldIdx) );
            }
            OGRGeometry* poGeom = poRet->GetGeomFieldRef(i);
            if( poGeom != nullptr )
                poGeom->assignSpatialReference( poGeomField->GetSpatialRef() );
        }
    }
    poRet->SetStyleString( poSrcFeature->GetStyleString() );
    poRet->SetNativeData( poSrcFeature->GetNativeData() );
    poRet->SetNativeMediaType( poSrcFeature->GetNativeMediaType() );
    poRet->SetFID(poSrcFeature->GetFID());

    return poRet;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGREditableLayer::GetNextFeature()
{
    if( !m_poDecoratedLayer ) return nullptr;
    while( true )
    {
        OGRFeature* poSrcFeature = m_poDecoratedLayer->GetNextFeature();
        bool bHideDeletedFields = true;
        if( poSrcFeature != nullptr )
        {
            const GIntBig nFID = poSrcFeature->GetFID();
            if( m_oSetDeleted.find(nFID) != m_oSetDeleted.end() )
            {
                delete poSrcFeature;
                continue;
            }
            else if( m_oSetCreated.find(nFID) != m_oSetCreated.end() ||
                     m_oSetEdited.find(nFID) != m_oSetEdited.end() )
            {
                delete poSrcFeature;
                poSrcFeature = m_poMemLayer->GetFeature(nFID);
                bHideDeletedFields = false;
            }
        }
        else
        {
            if( m_oIter != m_oSetCreated.end() )
            {
                poSrcFeature = m_poMemLayer->GetFeature(*m_oIter);
                bHideDeletedFields = false;
                ++ m_oIter;
            }
            else
            {
                return nullptr;
            }
        }
        OGRFeature* poRet = Translate(m_poEditableFeatureDefn, poSrcFeature,
                                      true, bHideDeletedFields);
        delete poSrcFeature;

        if( (m_poFilterGeom == nullptr
             || FilterGeometry( poRet->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poRet ) ) )
        {
            return poRet;
        }
        delete poRet;
    }
}

/************************************************************************/
/*                          SetNextByIndex()                            */
/************************************************************************/

OGRErr      OGREditableLayer::SetNextByIndex( GIntBig nIndex )
{
    if( m_poDecoratedLayer != nullptr &&
        m_oSetCreated.empty() &&
        m_oSetDeleted.empty() &&
        m_oSetEdited.empty() )
    {
        return m_poDecoratedLayer->SetNextByIndex(nIndex);
    }

    return OGRLayer::SetNextByIndex(nIndex);
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature *OGREditableLayer::GetFeature( GIntBig nFID )
{
    if( !m_poDecoratedLayer ) return nullptr;

    OGRFeature* poSrcFeature = nullptr;
    bool bHideDeletedFields = true;
    if( m_oSetCreated.find(nFID) != m_oSetCreated.end() ||
        m_oSetEdited.find(nFID) != m_oSetEdited.end() )
    {
        poSrcFeature = m_poMemLayer->GetFeature(nFID);
        bHideDeletedFields = false;
    }
    else if( m_oSetDeleted.find(nFID) != m_oSetDeleted.end() )
    {
        poSrcFeature = nullptr;
    }
    else
    {
        poSrcFeature = m_poDecoratedLayer->GetFeature(nFID);
    }
    OGRFeature* poRet = Translate(m_poEditableFeatureDefn, poSrcFeature,
                                  true, bHideDeletedFields);
    delete poSrcFeature;
    return poRet;
}

/************************************************************************/
/*                            ISetFeature()                             */
/************************************************************************/

OGRErr      OGREditableLayer::ISetFeature( OGRFeature *poFeature )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    if( !m_bStructureModified &&
        m_oSetDeleted.empty() &&
        m_oSetEdited.empty() &&
        m_oSetCreated.empty() &&
        m_poDecoratedLayer->TestCapability(OLCRandomWrite) )
    {
        OGRFeature* poTargetFeature = Translate(m_poDecoratedLayer->GetLayerDefn(),
                                                poFeature, false, false);
        OGRErr eErr = m_poDecoratedLayer->SetFeature(poTargetFeature);
        delete poTargetFeature;
        return eErr;
    }

    OGRFeature* poMemFeature = Translate(m_poMemLayer->GetLayerDefn(),
                                         poFeature, false, false);
    OGRErr eErr = m_poMemLayer->SetFeature(poMemFeature);
    if( eErr == OGRERR_NONE )
    {
        const GIntBig nFID = poMemFeature->GetFID();
        m_oSetDeleted.erase(nFID);
        // If the feature isn't in the created list, insert it in the edited list
        if( m_oSetCreated.find(nFID) == m_oSetCreated.end() )
        {
            m_oSetEdited.insert(nFID);
        }
        poFeature->SetFID(nFID);
    }
    delete poMemFeature;

    return eErr;
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr      OGREditableLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    if( !m_bStructureModified &&
        m_oSetDeleted.empty() &&
        m_oSetCreated.empty() &&
        m_poDecoratedLayer->TestCapability(OLCSequentialWrite) )
    {
        OGRFeature* poTargetFeature = Translate(m_poDecoratedLayer->GetLayerDefn(),
                                                poFeature, false, false);
        OGRErr eErr = m_poDecoratedLayer->CreateFeature(poTargetFeature);
        if( poFeature->GetFID() < 0 )
            poFeature->SetFID(poTargetFeature->GetFID());
        delete poTargetFeature;
        return eErr;
    }

    OGRFeature* poMemFeature = Translate(m_poMemLayer->GetLayerDefn(),
                                         poFeature, false, false);
    DetectNextFID();
    if( poMemFeature->GetFID() < 0 )
        poMemFeature->SetFID( m_nNextFID ++ );
    OGRErr eErr = m_poMemLayer->CreateFeature(poMemFeature);
    if( eErr == OGRERR_NONE )
    {
        const GIntBig nFID = poMemFeature->GetFID();
        m_oSetDeleted.erase(nFID);
        m_oSetEdited.erase(nFID);
        m_oSetCreated.insert(nFID);
        poFeature->SetFID(nFID);
    }
    delete poMemFeature;

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr      OGREditableLayer::DeleteFeature( GIntBig nFID )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    OGRErr eErr;
    if( m_oSetDeleted.find(nFID) != m_oSetDeleted.end() )
    {
        eErr = OGRERR_NON_EXISTING_FEATURE;
    }
    // cppcheck-suppress redundantIfRemove
    else if( m_oSetCreated.find(nFID) != m_oSetCreated.end() )
    {
        m_oSetCreated.erase(nFID);
        eErr = m_poMemLayer->DeleteFeature(nFID);
    }
    // cppcheck-suppress redundantIfRemove
    else if( m_oSetEdited.find(nFID) != m_oSetEdited.end() )
    {
        m_oSetEdited.erase(nFID);
        m_oSetDeleted.insert(nFID);
        eErr = m_poMemLayer->DeleteFeature(nFID);
    }
    else
    {
        OGRFeature* poFeature = m_poDecoratedLayer->GetFeature(nFID);
        if( poFeature != nullptr )
        {
            m_oSetDeleted.insert(nFID);
            eErr = OGRERR_NONE;
            delete poFeature;
        }
        else
        {
            eErr = OGRERR_NON_EXISTING_FEATURE;
        }
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                             GetGeomType()                            */
/************************************************************************/

OGRwkbGeometryType OGREditableLayer::GetGeomType()
{
    return OGRLayer::GetGeomType();
}

/************************************************************************/
/*                             GetLayerDefn()                           */
/************************************************************************/

OGRFeatureDefn *OGREditableLayer::GetLayerDefn()
{
    return m_poEditableFeatureDefn;
}

/************************************************************************/
/*                             GetSpatialRef()                          */
/************************************************************************/

OGRSpatialReference *OGREditableLayer::GetSpatialRef()
{
    return OGRLayer::GetSpatialRef();
}

/************************************************************************/
/*                           GetSpatialFilter()                         */
/************************************************************************/

OGRGeometry *OGREditableLayer::GetSpatialFilter()
{
    return OGRLayer::GetSpatialFilter();
}

/************************************************************************/
/*                           SetAttributeFilter()                       */
/************************************************************************/

OGRErr      OGREditableLayer::SetAttributeFilter( const char * poAttrFilter )
{
    return OGRLayer::SetAttributeFilter(poAttrFilter);
}

/************************************************************************/
/*                           SetSpatialFilter()                         */
/************************************************************************/

void        OGREditableLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    SetSpatialFilter(0, poGeom);
}

/************************************************************************/
/*                           SetSpatialFilter()                         */
/************************************************************************/

void        OGREditableLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    if( iGeomField < 0 ||
        (iGeomField != 0 && iGeomField >= GetLayerDefn()->GetGeomFieldCount()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid geometry field index : %d", iGeomField);
        return;
    }

    m_iGeomFieldFilter = iGeomField;
    if( InstallFilter( poGeom ) )
        ResetReading();

    int iSrcGeomFieldIdx = GetSrcGeomFieldIndex(iGeomField);
    if( iSrcGeomFieldIdx >= 0 )
    {
        m_poDecoratedLayer->SetSpatialFilter(iSrcGeomFieldIdx, poGeom);
    }
    m_poMemLayer->SetSpatialFilter(iGeomField, poGeom);
}

/************************************************************************/
/*                         SetSpatialFilterRect()                       */
/************************************************************************/

void OGREditableLayer::SetSpatialFilterRect( double dfMinX, double dfMinY,
                                             double dfMaxX, double dfMaxY )
{
   return OGRLayer::SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
}

/************************************************************************/
/*                         SetSpatialFilterRect()                       */
/************************************************************************/

void OGREditableLayer::SetSpatialFilterRect(
    int iGeomField, double dfMinX, double dfMinY,
    double dfMaxX, double dfMaxY )
{
    return
      OGRLayer::SetSpatialFilterRect(iGeomField,
                                     dfMinX, dfMinY, dfMaxX, dfMaxY);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGREditableLayer::GetFeatureCount( int bForce )
{
    if( !m_poDecoratedLayer ) return 0;
    if( m_poAttrQuery == nullptr && m_poFilterGeom == nullptr &&
        m_oSetDeleted.empty() &&
        m_oSetEdited.empty() )
    {
        GIntBig nFC = m_poDecoratedLayer->GetFeatureCount(bForce);
        if( nFC >= 0 )
        {
            nFC += m_oSetCreated.size();
        }
        return nFC;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr      OGREditableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return GetExtent(0, psExtent, bForce);
}

/************************************************************************/
/*                               GetExtent()                            */
/************************************************************************/

OGRErr      OGREditableLayer::GetExtent(int iGeomField, OGREnvelope *psExtent,
                                        int bForce)
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    int iSrcGeomFieldIdx = GetSrcGeomFieldIndex(iGeomField);
    if( iSrcGeomFieldIdx >= 0 && m_oSetEdited.empty() &&
        m_oSetDeleted.empty() )
    {
        OGRErr eErr = m_poDecoratedLayer->GetExtent(iSrcGeomFieldIdx, psExtent,
                                                    bForce);
        if( eErr == OGRERR_NONE )
        {
            OGREnvelope sExtentMemLayer;
            if( m_poMemLayer->GetExtent(iGeomField,
                                    &sExtentMemLayer, bForce) == OGRERR_NONE )
            {
                psExtent->Merge(sExtentMemLayer);
            }
        }
        return eErr;
    }
    return GetExtentInternal(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int         OGREditableLayer::TestCapability( const char * pszCap )
{
    if( !m_poDecoratedLayer ) return FALSE;
    if( EQUAL(pszCap, OLCSequentialWrite) ||
        EQUAL(pszCap, OLCRandomWrite) ||
        EQUAL(pszCap, OLCCreateField) ||
        EQUAL(pszCap, OLCDeleteField) ||
        EQUAL(pszCap, OLCReorderFields) ||
        EQUAL(pszCap, OLCAlterFieldDefn) ||
        EQUAL(pszCap, OLCAlterGeomFieldDefn) ||
        EQUAL(pszCap, OLCDeleteFeature) )
    {
        return m_poDecoratedLayer->TestCapability(OLCCreateField) == TRUE ||
               m_poDecoratedLayer->TestCapability(OLCSequentialWrite) == TRUE;
    }
    if( EQUAL(pszCap, OLCCreateGeomField) )
        return m_bSupportsCreateGeomField;
    if( EQUAL(pszCap, OLCCurveGeometries) )
        return m_bSupportsCurveGeometries;
    if( EQUAL(pszCap, OLCTransactions) )
        return FALSE;

    return m_poDecoratedLayer->TestCapability(pszCap);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr      OGREditableLayer::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    m_oMapEditableFDefnFieldNameToIdx.clear();

    if( !m_bStructureModified &&
        m_poDecoratedLayer->TestCapability(OLCCreateField) )
    {
        OGRErr eErr = m_poDecoratedLayer->CreateField(poField, bApproxOK);
        if( eErr == OGRERR_NONE )
        {
            eErr = m_poMemLayer->CreateField(poField, bApproxOK);
            if( eErr == OGRERR_NONE )
            {
                m_poEditableFeatureDefn->AddFieldDefn(poField);
            }
        }
        return eErr;
    }

    OGRErr eErr = m_poMemLayer->CreateField(poField, bApproxOK);
    if( eErr == OGRERR_NONE )
    {
        m_poEditableFeatureDefn->AddFieldDefn(poField);
        m_bStructureModified = true;
    }
    return eErr;
}

/************************************************************************/
/*                             DeleteField()                            */
/************************************************************************/

OGRErr      OGREditableLayer::DeleteField( int iField )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    m_oMapEditableFDefnFieldNameToIdx.clear();

    CPLString osDeletedField;
    if( iField >= 0 && iField < m_poEditableFeatureDefn->GetFieldCount() )
    {
        osDeletedField = m_poEditableFeatureDefn->GetFieldDefn(iField)->GetNameRef();
    }

    OGRErr eErr = m_poMemLayer->DeleteField(iField);
    if( eErr == OGRERR_NONE )
    {
        m_poEditableFeatureDefn->DeleteFieldDefn(iField);
        m_bStructureModified = true;
        m_oSetDeletedFields.insert(osDeletedField);
    }
    return eErr;
}

/************************************************************************/
/*                             ReorderFields()                          */
/************************************************************************/

OGRErr      OGREditableLayer::ReorderFields( int* panMap )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    m_oMapEditableFDefnFieldNameToIdx.clear();

    OGRErr eErr = m_poMemLayer->ReorderFields(panMap);
    if( eErr == OGRERR_NONE )
    {
        m_poEditableFeatureDefn->ReorderFieldDefns(panMap);
        m_bStructureModified = true;
    }
    return eErr;
}

/************************************************************************/
/*                            AlterFieldDefn()                          */
/************************************************************************/

OGRErr      OGREditableLayer::AlterFieldDefn( int iField,
                                              OGRFieldDefn* poNewFieldDefn,
                                              int nFlagsIn )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    m_oMapEditableFDefnFieldNameToIdx.clear();

    OGRErr eErr = m_poMemLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
    if( eErr == OGRERR_NONE )
    {
        OGRFieldDefn* poFieldDefn = m_poEditableFeatureDefn->GetFieldDefn(iField);
        OGRFieldDefn* poMemFieldDefn = m_poMemLayer->GetLayerDefn()->GetFieldDefn(iField);
        poFieldDefn->SetName(poMemFieldDefn->GetNameRef());
        poFieldDefn->SetType(poMemFieldDefn->GetType());
        poFieldDefn->SetSubType(poMemFieldDefn->GetSubType());
        poFieldDefn->SetWidth(poMemFieldDefn->GetWidth());
        poFieldDefn->SetPrecision(poMemFieldDefn->GetPrecision());
        poFieldDefn->SetDefault(poMemFieldDefn->GetDefault());
        poFieldDefn->SetNullable(poMemFieldDefn->IsNullable());
        poFieldDefn->SetUnique(poMemFieldDefn->IsUnique());
        poFieldDefn->SetDomainName(poMemFieldDefn->GetDomainName());
        m_bStructureModified = true;
    }
    return eErr;
}

/************************************************************************/
/*                         AlterGeomFieldDefn()                         */
/************************************************************************/

OGRErr      OGREditableLayer::AlterGeomFieldDefn( int iGeomField,
                                                  const OGRGeomFieldDefn* poNewGeomFieldDefn,
                                                  int nFlagsIn )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;

    OGRErr eErr = m_poMemLayer->AlterGeomFieldDefn(iGeomField, poNewGeomFieldDefn, nFlagsIn);
    if( eErr == OGRERR_NONE )
    {
        OGRGeomFieldDefn* poFieldDefn = m_poEditableFeatureDefn->GetGeomFieldDefn(iGeomField);
        OGRGeomFieldDefn* poMemFieldDefn = m_poMemLayer->GetLayerDefn()->GetGeomFieldDefn(iGeomField);
        poFieldDefn->SetName(poMemFieldDefn->GetNameRef());
        poFieldDefn->SetType(poMemFieldDefn->GetType());
        poFieldDefn->SetNullable(poMemFieldDefn->IsNullable());
        poFieldDefn->SetSpatialRef(poMemFieldDefn->GetSpatialRef());
        m_bStructureModified = true;
    }
    return eErr;
}
/************************************************************************/
/*                          CreateGeomField()                          */
/************************************************************************/

OGRErr      OGREditableLayer::CreateGeomField( OGRGeomFieldDefn *poField,
                                            int bApproxOK )
{
    if( !m_poDecoratedLayer || !m_bSupportsCreateGeomField ) return OGRERR_FAILURE;

    if( !m_bStructureModified && m_poDecoratedLayer->TestCapability(OLCCreateGeomField) )
    {
        OGRErr eErr = m_poDecoratedLayer->CreateGeomField(poField, bApproxOK);
        if( eErr == OGRERR_NONE )
        {
            eErr = m_poMemLayer->CreateGeomField(poField, bApproxOK);
            if( eErr == OGRERR_NONE )
            {
                m_poEditableFeatureDefn->AddGeomFieldDefn(poField);
            }
        }
        return eErr;
    }

    OGRErr eErr = m_poMemLayer->CreateGeomField(poField, bApproxOK);
    if( eErr == OGRERR_NONE )
    {
        m_poEditableFeatureDefn->AddGeomFieldDefn(poField);
        m_bStructureModified = true;
    }
    return eErr;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr      OGREditableLayer::SyncToDisk()
{
    if( !m_poDecoratedLayer || m_poSynchronizer == nullptr ) return OGRERR_FAILURE;
    OGRErr eErr = m_poDecoratedLayer->SyncToDisk();
    if( eErr == OGRERR_NONE )
    {
        if( m_oSetCreated.empty() && m_oSetEdited.empty() &&
            m_oSetDeleted.empty() && !m_bStructureModified )
        {
            return OGRERR_NONE;
        }
        eErr = m_poSynchronizer->EditableSyncToDisk(this, &m_poDecoratedLayer);
    }
    m_oSetCreated.clear();
    m_oSetEdited.clear();
    m_oSetDeleted.clear();
    m_oSetDeletedFields.clear();
    m_bStructureModified = false;
    return eErr;
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGREditableLayer::StartTransaction()

{
    return OGRLayer::StartTransaction();
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGREditableLayer::CommitTransaction()

{
    return OGRLayer::CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGREditableLayer::RollbackTransaction()

{
    return OGRLayer::RollbackTransaction();
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGREditableLayer::GetGeometryColumn()

{
    return OGRLayer::GetGeometryColumn();
}

//! @endcond
