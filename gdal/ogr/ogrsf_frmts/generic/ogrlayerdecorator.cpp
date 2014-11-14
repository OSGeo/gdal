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
    CPLAssert(poDecoratedLayer != NULL);
}

OGRLayerDecorator::~OGRLayerDecorator()
{
    if( m_bHasOwnership )
        delete m_poDecoratedLayer;
}


OGRGeometry *OGRLayerDecorator::GetSpatialFilter()
{
    return m_poDecoratedLayer->GetSpatialFilter();
}

void        OGRLayerDecorator::SetSpatialFilter( OGRGeometry * poGeom )
{
    m_poDecoratedLayer->SetSpatialFilter(poGeom);
}

void        OGRLayerDecorator::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    m_poDecoratedLayer->SetSpatialFilter(iGeomField, poGeom);
}

void        OGRLayerDecorator::SetSpatialFilterRect( double dfMinX, double dfMinY,
                                  double dfMaxX, double dfMaxY )
{
    m_poDecoratedLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
}

void        OGRLayerDecorator::SetSpatialFilterRect( int iGeomField, double dfMinX, double dfMinY,
                                  double dfMaxX, double dfMaxY )
{
    m_poDecoratedLayer->SetSpatialFilterRect(iGeomField, dfMinX, dfMinY, dfMaxX, dfMaxY);
}

OGRErr      OGRLayerDecorator::SetAttributeFilter( const char * poAttrFilter )
{
    return m_poDecoratedLayer->SetAttributeFilter(poAttrFilter);
}

void        OGRLayerDecorator::ResetReading()
{
    m_poDecoratedLayer->ResetReading();
}

OGRFeature *OGRLayerDecorator::GetNextFeature()
{
    return m_poDecoratedLayer->GetNextFeature();
}

OGRErr      OGRLayerDecorator::SetNextByIndex( long nIndex )
{
    return m_poDecoratedLayer->SetNextByIndex(nIndex);
}

OGRFeature *OGRLayerDecorator::GetFeature( long nFID )
{
    return m_poDecoratedLayer->GetFeature(nFID);
}

OGRErr      OGRLayerDecorator::ISetFeature( OGRFeature *poFeature )
{
    return m_poDecoratedLayer->SetFeature(poFeature);
}

OGRErr      OGRLayerDecorator::ICreateFeature( OGRFeature *poFeature )
{
    return m_poDecoratedLayer->CreateFeature(poFeature);
}

OGRErr      OGRLayerDecorator::DeleteFeature( long nFID )
{
    return m_poDecoratedLayer->DeleteFeature(nFID);
}

const char *OGRLayerDecorator::GetName()
{
    return m_poDecoratedLayer->GetName();
}

OGRwkbGeometryType OGRLayerDecorator::GetGeomType()
{
    return m_poDecoratedLayer->GetGeomType();
}

OGRFeatureDefn *OGRLayerDecorator::GetLayerDefn()
{
    return m_poDecoratedLayer->GetLayerDefn();
}

OGRSpatialReference *OGRLayerDecorator::GetSpatialRef()
{
    return m_poDecoratedLayer->GetSpatialRef();
}

int         OGRLayerDecorator::GetFeatureCount( int bForce )
{
    return m_poDecoratedLayer->GetFeatureCount(bForce);
}

OGRErr      OGRLayerDecorator::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return m_poDecoratedLayer->GetExtent(psExtent, bForce);
}

OGRErr      OGRLayerDecorator::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    return m_poDecoratedLayer->GetExtent(iGeomField, psExtent, bForce);
}

int         OGRLayerDecorator::TestCapability( const char * pszCapability )
{
    return m_poDecoratedLayer->TestCapability(pszCapability);
}

OGRErr      OGRLayerDecorator::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    return m_poDecoratedLayer->CreateField(poField, bApproxOK);
}

OGRErr      OGRLayerDecorator::DeleteField( int iField )
{
    return m_poDecoratedLayer->DeleteField(iField);
}

OGRErr      OGRLayerDecorator::ReorderFields( int* panMap )
{
    return m_poDecoratedLayer->ReorderFields(panMap);
}

OGRErr      OGRLayerDecorator::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    return m_poDecoratedLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlags);
}

OGRErr      OGRLayerDecorator::SyncToDisk()
{
    return m_poDecoratedLayer->SyncToDisk();
}

OGRStyleTable *OGRLayerDecorator::GetStyleTable()
{
    return m_poDecoratedLayer->GetStyleTable();
}

void        OGRLayerDecorator::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    m_poDecoratedLayer->SetStyleTableDirectly(poStyleTable);
}

void        OGRLayerDecorator::SetStyleTable(OGRStyleTable *poStyleTable)
{
    m_poDecoratedLayer->SetStyleTable(poStyleTable);
}

OGRErr      OGRLayerDecorator::StartTransaction()
{
    return m_poDecoratedLayer->StartTransaction();
}

OGRErr      OGRLayerDecorator::CommitTransaction()
{
    return m_poDecoratedLayer->CommitTransaction();
}

OGRErr      OGRLayerDecorator::RollbackTransaction()
{
    return m_poDecoratedLayer->RollbackTransaction();
}

const char *OGRLayerDecorator::GetFIDColumn()
{
    return m_poDecoratedLayer->GetFIDColumn();
}

const char *OGRLayerDecorator::GetGeometryColumn()
{
    return m_poDecoratedLayer->GetGeometryColumn();
}

OGRErr      OGRLayerDecorator::SetIgnoredFields( const char **papszFields )
{
    return m_poDecoratedLayer->SetIgnoredFields(papszFields);
}
