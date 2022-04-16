/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMutexedLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogrmutexedlayer.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$")

OGRMutexedLayer::OGRMutexedLayer( OGRLayer* poDecoratedLayer,
                                  int bTakeOwnership,
                                  CPLMutex* hMutex ) :
    OGRLayerDecorator(poDecoratedLayer, bTakeOwnership), m_hMutex(hMutex)
{
    SetDescription( poDecoratedLayer->GetDescription() );
}

OGRMutexedLayer::~OGRMutexedLayer() {}

OGRGeometry *OGRMutexedLayer::GetSpatialFilter()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetSpatialFilter();
}

void        OGRMutexedLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    OGRLayerDecorator::SetSpatialFilter(poGeom);
}

void        OGRMutexedLayer::SetSpatialFilterRect( double dfMinX, double dfMinY,
                                  double dfMaxX, double dfMaxY )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    OGRLayerDecorator::SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
}

void        OGRMutexedLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    OGRLayerDecorator::SetSpatialFilter(iGeomField, poGeom);
}

void        OGRMutexedLayer::SetSpatialFilterRect( int iGeomField, double dfMinX, double dfMinY,
                                  double dfMaxX, double dfMaxY )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    OGRLayerDecorator::SetSpatialFilterRect(iGeomField, dfMinX, dfMinY, dfMaxX, dfMaxY);
}

OGRErr      OGRMutexedLayer::SetAttributeFilter( const char * poAttrFilter )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetAttributeFilter(poAttrFilter);
}

void        OGRMutexedLayer::ResetReading()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    OGRLayerDecorator::ResetReading();
}

OGRFeature *OGRMutexedLayer::GetNextFeature()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetNextFeature();
}

OGRErr      OGRMutexedLayer::SetNextByIndex( GIntBig nIndex )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetNextByIndex(nIndex);
}

OGRFeature *OGRMutexedLayer::GetFeature( GIntBig nFID )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetFeature(nFID);
}

OGRErr      OGRMutexedLayer::ISetFeature( OGRFeature *poFeature )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::ISetFeature(poFeature);
}

OGRErr      OGRMutexedLayer::ICreateFeature( OGRFeature *poFeature )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::ICreateFeature(poFeature);
}

OGRErr      OGRMutexedLayer::DeleteFeature( GIntBig nFID )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::DeleteFeature(nFID);
}

const char *OGRMutexedLayer::GetName()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetName();
}

OGRwkbGeometryType OGRMutexedLayer::GetGeomType()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetGeomType();
}

OGRFeatureDefn *OGRMutexedLayer::GetLayerDefn()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetLayerDefn();
}

OGRSpatialReference *OGRMutexedLayer::GetSpatialRef()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetSpatialRef();
}

GIntBig         OGRMutexedLayer::GetFeatureCount( int bForce )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetFeatureCount(bForce);
}

OGRErr      OGRMutexedLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetExtent(iGeomField, psExtent, bForce);
}

OGRErr      OGRMutexedLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetExtent(psExtent, bForce);
}

int         OGRMutexedLayer::TestCapability( const char * pszCapability )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::TestCapability(pszCapability);
}

OGRErr      OGRMutexedLayer::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::CreateField(poField, bApproxOK);
}

OGRErr      OGRMutexedLayer::DeleteField( int iField )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::DeleteField(iField);
}

OGRErr      OGRMutexedLayer::ReorderFields( int* panMap )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::ReorderFields(panMap);
}

OGRErr      OGRMutexedLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

OGRErr      OGRMutexedLayer::SyncToDisk()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SyncToDisk();
}

OGRStyleTable *OGRMutexedLayer::GetStyleTable()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetStyleTable();
}

void        OGRMutexedLayer::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetStyleTableDirectly(poStyleTable);
}

void        OGRMutexedLayer::SetStyleTable(OGRStyleTable *poStyleTable)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetStyleTable(poStyleTable);
}

OGRErr      OGRMutexedLayer::StartTransaction()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::StartTransaction();
}

OGRErr      OGRMutexedLayer::CommitTransaction()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::CommitTransaction();
}

OGRErr      OGRMutexedLayer::RollbackTransaction()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::RollbackTransaction();
}

const char *OGRMutexedLayer::GetFIDColumn()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetFIDColumn();
}

const char *OGRMutexedLayer::GetGeometryColumn()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetGeometryColumn();
}

OGRErr      OGRMutexedLayer::SetIgnoredFields( const char **papszFields )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetIgnoredFields(papszFields);
}

char      **OGRMutexedLayer::GetMetadata( const char * pszDomain )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetMetadata(pszDomain);
}

CPLErr      OGRMutexedLayer::SetMetadata( char ** papszMetadata,
                                          const char * pszDomain )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetMetadata(papszMetadata, pszDomain);
}

const char *OGRMutexedLayer::GetMetadataItem( const char * pszName,
                                              const char * pszDomain )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetMetadataItem(pszName, pszDomain);
}

CPLErr      OGRMutexedLayer::SetMetadataItem( const char * pszName,
                                              const char * pszValue,
                                              const char * pszDomain )
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetMetadataItem(pszName, pszValue, pszDomain);
}

OGRErr OGRMutexedLayer::Rename(const char* pszNewName)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::Rename(pszNewName);
}

#if defined(WIN32) && defined(_MSC_VER)
// Horrible hack: for some reason MSVC doesn't export the class
// if it is not referenced from the DLL itself
void OGRRegisterMutexedLayer();
void OGRRegisterMutexedLayer()
{
    CPLAssert(false); // Never call this function: it will segfault
    delete new OGRMutexedLayer(NULL, FALSE, NULL);
}
#endif

#endif /* #ifndef DOXYGEN_SKIP */
