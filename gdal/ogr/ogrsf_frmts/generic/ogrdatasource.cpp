/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the GDALDataset class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ograpispy.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           ~OGRDataSource()                           */
/************************************************************************/

OGRDataSource::OGRDataSource() {}

/************************************************************************/
/*                         DestroyDataSource()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
void OGRDataSource::DestroyDataSource( OGRDataSource *poDS )

{
    delete poDS;
}
//! @endcond

/************************************************************************/
/*                           OGR_DS_Destroy()                           */
/************************************************************************/

void OGR_DS_Destroy( OGRDataSourceH hDS )

{
    if( hDS == nullptr )
        return;
    GDALClose( reinterpret_cast<GDALDatasetH>(hDS) );
    //VALIDATE_POINTER0( hDS, "OGR_DS_Destroy" );
}

/************************************************************************/
/*                          OGR_DS_Reference()                          */
/************************************************************************/

int OGR_DS_Reference( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_Reference", 0 );

    return reinterpret_cast<GDALDataset *>(hDataSource)->Reference();
}

/************************************************************************/
/*                         OGR_DS_Dereference()                         */
/************************************************************************/

int OGR_DS_Dereference( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_Dereference", 0 );

    return reinterpret_cast<GDALDataset *>(hDataSource)->Dereference();
}

/************************************************************************/
/*                         OGR_DS_GetRefCount()                         */
/************************************************************************/

int OGR_DS_GetRefCount( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_GetRefCount", 0 );

    return reinterpret_cast<GDALDataset *>(hDataSource)->GetRefCount();
}

/************************************************************************/
/*                     OGR_DS_GetSummaryRefCount()                      */
/************************************************************************/

int OGR_DS_GetSummaryRefCount( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_GetSummaryRefCount", 0 );

    return reinterpret_cast<GDALDataset *>(hDataSource)->GetSummaryRefCount();
}

/************************************************************************/
/*                         OGR_DS_CreateLayer()                         */
/************************************************************************/

OGRLayerH OGR_DS_CreateLayer( OGRDataSourceH hDS,
                              const char * pszName,
                              OGRSpatialReferenceH hSpatialRef,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_CreateLayer", nullptr );

    if (pszName == nullptr)
    {
        CPLError ( CE_Failure, CPLE_ObjectNull, "Name was NULL in OGR_DS_CreateLayer");
        return nullptr;
    }
    OGRLayerH hLayer = OGRLayer::ToHandle(
        reinterpret_cast<GDALDataset *>(hDS)->CreateLayer(
            pszName, OGRSpatialReference::FromHandle(hSpatialRef), eType, papszOptions ));

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_DS_CreateLayer(hDS, pszName, hSpatialRef, eType, papszOptions, hLayer);
#endif

    return hLayer;
}

/************************************************************************/
/*                          OGR_DS_CopyLayer()                          */
/************************************************************************/

OGRLayerH OGR_DS_CopyLayer( OGRDataSourceH hDS,
                            OGRLayerH hSrcLayer, const char *pszNewName,
                            char **papszOptions )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_CopyLayer", nullptr );
    VALIDATE_POINTER1( hSrcLayer, "OGR_DS_CopyLayer", nullptr );
    VALIDATE_POINTER1( pszNewName, "OGR_DS_CopyLayer", nullptr );

    return OGRLayer::ToHandle(
        reinterpret_cast<GDALDataset *>(hDS)->CopyLayer(
            OGRLayer::FromHandle(hSrcLayer), pszNewName, papszOptions ));
}

/************************************************************************/
/*                         OGR_DS_DeleteLayer()                         */
/************************************************************************/

OGRErr OGR_DS_DeleteLayer( OGRDataSourceH hDS, int iLayer )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_DeleteLayer", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_DS_DeleteLayer(reinterpret_cast<GDALDatasetH>(hDS), iLayer);
#endif

    OGRErr eErr = reinterpret_cast<GDALDataset *>(hDS)->DeleteLayer( iLayer );

    return eErr;
}

/************************************************************************/
/*                       OGR_DS_GetLayerByName()                        */
/************************************************************************/

OGRLayerH OGR_DS_GetLayerByName( OGRDataSourceH hDS, const char *pszLayerName )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetLayerByName", nullptr );

    OGRLayerH hLayer = OGRLayer::ToHandle(
        reinterpret_cast<GDALDataset *>(hDS)->GetLayerByName( pszLayerName ));

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_DS_GetLayerByName(reinterpret_cast<GDALDatasetH>(hDS), pszLayerName, hLayer);
#endif

    return hLayer;
}

/************************************************************************/
/*                         OGR_DS_ExecuteSQL()                          */
/************************************************************************/

OGRLayerH OGR_DS_ExecuteSQL( OGRDataSourceH hDS,
                             const char *pszStatement,
                             OGRGeometryH hSpatialFilter,
                             const char *pszDialect )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_ExecuteSQL", nullptr );

    OGRLayerH hLayer = OGRLayer::ToHandle(
        reinterpret_cast<GDALDataset *>(hDS)->ExecuteSQL( pszStatement,
                OGRGeometry::FromHandle(hSpatialFilter), pszDialect ));

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_DS_ExecuteSQL(reinterpret_cast<GDALDatasetH>(hDS), pszStatement, hSpatialFilter, pszDialect, hLayer);
#endif

    return hLayer;
}


/************************************************************************/
/*                      OGR_DS_ReleaseResultSet()                       */
/************************************************************************/

void OGR_DS_ReleaseResultSet( OGRDataSourceH hDS, OGRLayerH hLayer )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_ReleaseResultSet" );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_DS_ReleaseResultSet(reinterpret_cast<GDALDatasetH>(hDS), hLayer);
#endif

    reinterpret_cast<GDALDataset *>(hDS)->ReleaseResultSet( OGRLayer::FromHandle(hLayer) );
}

/************************************************************************/
/*                       OGR_DS_TestCapability()                        */
/************************************************************************/

int OGR_DS_TestCapability( OGRDataSourceH hDS, const char *pszCapability )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_TestCapability", 0 );
    VALIDATE_POINTER1( pszCapability, "OGR_DS_TestCapability", 0 );

    return reinterpret_cast<GDALDataset *>(hDS)->TestCapability( pszCapability );
}

/************************************************************************/
/*                        OGR_DS_GetLayerCount()                        */
/************************************************************************/

int OGR_DS_GetLayerCount( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetLayerCount", 0 );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_DS_GetLayerCount(reinterpret_cast<GDALDatasetH>(hDS));
#endif

    return reinterpret_cast<GDALDataset *>(hDS)->GetLayerCount();
}

/************************************************************************/
/*                          OGR_DS_GetLayer()                           */
/************************************************************************/

OGRLayerH OGR_DS_GetLayer( OGRDataSourceH hDS, int iLayer )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetLayer", nullptr );

    OGRLayerH hLayer = OGRLayer::ToHandle(
        reinterpret_cast<GDALDataset *>(hDS)->GetLayer( iLayer ));

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_DS_GetLayer(reinterpret_cast<GDALDatasetH>(hDS), iLayer, hLayer);
#endif

    return hLayer;
}

/************************************************************************/
/*                           OGR_DS_GetName()                           */
/************************************************************************/

const char *OGR_DS_GetName( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetName", nullptr );

    return reinterpret_cast<GDALDataset *>(hDS)->GetDescription();
}

/************************************************************************/
/*                         OGR_DS_SyncToDisk()                          */
/************************************************************************/

OGRErr OGR_DS_SyncToDisk( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_SyncToDisk", OGRERR_INVALID_HANDLE );

    reinterpret_cast<GDALDataset *>(hDS)->FlushCache(false);
    if( CPLGetLastErrorType() != 0 )
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                          OGR_DS_GetDriver()                          */
/************************************************************************/

OGRSFDriverH OGR_DS_GetDriver( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetDriver", nullptr );

    return reinterpret_cast<OGRSFDriverH>(
        reinterpret_cast<OGRDataSource *>(hDS)->GetDriver());
}

/************************************************************************/
/*                         OGR_DS_GetStyleTable()                       */
/************************************************************************/

OGRStyleTableH OGR_DS_GetStyleTable( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetStyleTable", nullptr );

    return reinterpret_cast<OGRStyleTableH>(
        reinterpret_cast<GDALDataset *>(hDS)->GetStyleTable( ));
}

/************************************************************************/
/*                         OGR_DS_SetStyleTableDirectly()               */
/************************************************************************/

void OGR_DS_SetStyleTableDirectly( OGRDataSourceH hDS,
                                   OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_SetStyleTableDirectly" );

    reinterpret_cast<GDALDataset *>(hDS)->SetStyleTableDirectly(
        reinterpret_cast<OGRStyleTable *>(hStyleTable) );
}

/************************************************************************/
/*                         OGR_DS_SetStyleTable()                       */
/************************************************************************/

void OGR_DS_SetStyleTable( OGRDataSourceH hDS, OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_SetStyleTable" );
    VALIDATE_POINTER0( hStyleTable, "OGR_DS_SetStyleTable" );

    reinterpret_cast<GDALDataset *>(hDS)->SetStyleTable(
        reinterpret_cast<OGRStyleTable *>(hStyleTable) );
}
