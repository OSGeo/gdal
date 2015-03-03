/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRLayerDecorator class
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

#include "ogrlayerdecorator.h"

CPL_CVSID("$Id$");

OGRLayerDecorator::OGRLayerDecorator(OGRLayer* poDecoratedLayer,
                                     int bTakeOwnership) :
                                        m_poDecoratedLayer(poDecoratedLayer),
                                        m_bHasOwnership(bTakeOwnership)
{
    SetDescription( poDecoratedLayer->GetDescription() );
    CPLAssert(poDecoratedLayer != NULL);
}

OGRLayerDecorator::~OGRLayerDecorator()
{
    if( m_bHasOwnership )
        delete m_poDecoratedLayer;
}


OGRGeometry *OGRLayerDecorator::GetSpatialFilter()
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetSpatialFilter();
}

void        OGRLayerDecorator::SetSpatialFilter( OGRGeometry * poGeom )
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->SetSpatialFilter(poGeom);
}

void        OGRLayerDecorator::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->SetSpatialFilter(iGeomField, poGeom);
}

void        OGRLayerDecorator::SetSpatialFilterRect( double dfMinX, double dfMinY,
                                  double dfMaxX, double dfMaxY )
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
}

void        OGRLayerDecorator::SetSpatialFilterRect( int iGeomField, double dfMinX, double dfMinY,
                                  double dfMaxX, double dfMaxY )
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->SetSpatialFilterRect(iGeomField, dfMinX, dfMinY, dfMaxX, dfMaxY);
}

OGRErr      OGRLayerDecorator::SetAttributeFilter( const char * poAttrFilter )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetAttributeFilter(poAttrFilter);
}

void        OGRLayerDecorator::ResetReading()
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->ResetReading();
}

OGRFeature *OGRLayerDecorator::GetNextFeature()
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetNextFeature();
}

OGRErr      OGRLayerDecorator::SetNextByIndex( GIntBig nIndex )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetNextByIndex(nIndex);
}

OGRFeature *OGRLayerDecorator::GetFeature( GIntBig nFID )
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetFeature(nFID);
}

OGRErr      OGRLayerDecorator::ISetFeature( OGRFeature *poFeature )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetFeature(poFeature);
}

OGRErr      OGRLayerDecorator::ICreateFeature( OGRFeature *poFeature )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->CreateFeature(poFeature);
}

OGRErr      OGRLayerDecorator::DeleteFeature( GIntBig nFID )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->DeleteFeature(nFID);
}

const char* OGRLayerDecorator::GetName()
{
    if( !m_poDecoratedLayer ) return GetDescription();
    return m_poDecoratedLayer->GetName();
}

OGRwkbGeometryType OGRLayerDecorator::GetGeomType()
{
    if( !m_poDecoratedLayer ) return wkbNone;
    return m_poDecoratedLayer->GetGeomType();
}

OGRFeatureDefn *OGRLayerDecorator::GetLayerDefn()
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetLayerDefn();
}

OGRSpatialReference *OGRLayerDecorator::GetSpatialRef()
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetSpatialRef();
}

GIntBig         OGRLayerDecorator::GetFeatureCount( int bForce )
{
    if( !m_poDecoratedLayer ) return 0;
    return m_poDecoratedLayer->GetFeatureCount(bForce);
}

OGRErr      OGRLayerDecorator::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->GetExtent(psExtent, bForce);
}

OGRErr      OGRLayerDecorator::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->GetExtent(iGeomField, psExtent, bForce);
}

int         OGRLayerDecorator::TestCapability( const char * pszCapability )
{
    if( !m_poDecoratedLayer ) return FALSE;
    return m_poDecoratedLayer->TestCapability(pszCapability);
}

OGRErr      OGRLayerDecorator::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->CreateField(poField, bApproxOK);
}

OGRErr      OGRLayerDecorator::DeleteField( int iField )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->DeleteField(iField);
}

OGRErr      OGRLayerDecorator::ReorderFields( int* panMap )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->ReorderFields(panMap);
}

OGRErr      OGRLayerDecorator::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlags);
}

OGRErr      OGRLayerDecorator::SyncToDisk()
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->SyncToDisk();
}

OGRStyleTable *OGRLayerDecorator::GetStyleTable()
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetStyleTable();
}

void        OGRLayerDecorator::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->SetStyleTableDirectly(poStyleTable);
}

void        OGRLayerDecorator::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if( !m_poDecoratedLayer ) return;
    m_poDecoratedLayer->SetStyleTable(poStyleTable);
}

OGRErr      OGRLayerDecorator::StartTransaction()
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->StartTransaction();
}

OGRErr      OGRLayerDecorator::CommitTransaction()
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->CommitTransaction();
}

OGRErr      OGRLayerDecorator::RollbackTransaction()
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->RollbackTransaction();
}

const char *OGRLayerDecorator::GetFIDColumn()
{
    if( !m_poDecoratedLayer ) return "";
    return m_poDecoratedLayer->GetFIDColumn();
}

const char *OGRLayerDecorator::GetGeometryColumn()
{
    if( !m_poDecoratedLayer ) return "";
    return m_poDecoratedLayer->GetGeometryColumn();
}

OGRErr      OGRLayerDecorator::SetIgnoredFields( const char **papszFields )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetIgnoredFields(papszFields);
}

char      **OGRLayerDecorator::GetMetadata( const char * pszDomain )
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetMetadata(pszDomain);
}

CPLErr      OGRLayerDecorator::SetMetadata( char ** papszMetadata,
                                          const char * pszDomain )
{
    if( !m_poDecoratedLayer ) return CE_Failure;
    return m_poDecoratedLayer->SetMetadata(papszMetadata, pszDomain);
}

const char *OGRLayerDecorator::GetMetadataItem( const char * pszName,
                                              const char * pszDomain )
{
    if( !m_poDecoratedLayer ) return NULL;
    return m_poDecoratedLayer->GetMetadataItem(pszName, pszDomain);
}

CPLErr      OGRLayerDecorator::SetMetadataItem( const char * pszName,
                                              const char * pszValue,
                                              const char * pszDomain )
{
    if( !m_poDecoratedLayer ) return CE_Failure;
    return m_poDecoratedLayer->SetMetadataItem(pszName, pszValue, pszDomain);
}
