/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMutexedDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogrmutexeddatasource.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

OGRMutexedDataSource::OGRMutexedDataSource(OGRDataSource* poBaseDataSource,
                                           int bTakeOwnership,
                                           void* hMutexIn) :
            m_poBaseDataSource(poBaseDataSource),
            m_bHasOwnership(bTakeOwnership),
            m_hGlobalMutex(hMutexIn)
{
}

OGRMutexedDataSource::~OGRMutexedDataSource()
{
    if( m_bHasOwnership )
        delete m_poBaseDataSource;
}

const char  *OGRMutexedDataSource::GetName()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetName();
}

int         OGRMutexedDataSource::GetLayerCount()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetLayerCount();
}

OGRLayer    *OGRMutexedDataSource::GetLayer(int iIndex)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetLayer(iIndex);
}

OGRLayer    *OGRMutexedDataSource::GetLayerByName(const char *pszName)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetLayerByName(pszName);
}

OGRErr      OGRMutexedDataSource::DeleteLayer(int iIndex)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->DeleteLayer(iIndex);
}

int         OGRMutexedDataSource::TestCapability( const char * pszCap )
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->TestCapability(pszCap);
}

OGRLayer   *OGRMutexedDataSource::CreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef,
                                     OGRwkbGeometryType eGType,
                                     char ** papszOptions)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->CreateLayer(pszName, poSpatialRef, eGType, papszOptions);
}

OGRLayer   *OGRMutexedDataSource::CopyLayer( OGRLayer *poSrcLayer, 
                                   const char *pszNewName, 
                                   char **papszOptions )
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->CopyLayer(poSrcLayer, pszNewName, papszOptions );
}

OGRStyleTable *OGRMutexedDataSource::GetStyleTable()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetStyleTable();
}

void        OGRMutexedDataSource::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    m_poBaseDataSource->SetStyleTableDirectly(poStyleTable);
}

void        OGRMutexedDataSource::SetStyleTable(OGRStyleTable *poStyleTable)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    m_poBaseDataSource->SetStyleTable(poStyleTable);
}

OGRLayer *  OGRMutexedDataSource::ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect )
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->ExecuteSQL(pszStatement, poSpatialFilter,
                                          pszDialect);
}

void        OGRMutexedDataSource::ReleaseResultSet( OGRLayer * poResultsSet )
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    m_poBaseDataSource->ReleaseResultSet(poResultsSet);
}

OGRErr      OGRMutexedDataSource::SyncToDisk()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->SyncToDisk();
}
