/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRSFDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_p.h"
#include "ograpispy.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

/************************************************************************/
/*                            ~OGRSFDriver()                            */
/************************************************************************/

OGRSFDriver::~OGRSFDriver()

{
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRSFDriver::CreateDataSource( const char *, char ** )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateDataSource() not supported by this driver.\n" );

    return nullptr;
}

/************************************************************************/
/*                      OGR_Dr_CreateDataSource()                       */
/************************************************************************/

OGRDataSourceH OGR_Dr_CreateDataSource( OGRSFDriverH hDriver,
                                        const char *pszName,
                                        char ** papszOptions )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_CreateDataSource", nullptr );

    GDALDriver* poDriver = reinterpret_cast<GDALDriver*>(hDriver);

    /* MapServer had the bad habit of calling with NULL name for a memory datasource */
    if( pszName == nullptr )
        pszName = "";

    OGRDataSourceH hDS = reinterpret_cast<OGRDataSourceH>(
        poDriver->Create( pszName, 0, 0, 0, GDT_Unknown, papszOptions ));

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyCreateDataSource(hDriver, pszName, papszOptions,
                              reinterpret_cast<OGRDataSourceH>(hDS));
#endif

    return hDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr OGRSFDriver::DeleteDataSource( const char *pszDataSource )

{
    (void) pszDataSource;
    CPLError( CE_Failure, CPLE_NotSupported,
              "DeleteDataSource() not supported by this driver." );

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                      OGR_Dr_DeleteDataSource()                       */
/************************************************************************/

OGRErr OGR_Dr_DeleteDataSource( OGRSFDriverH hDriver,
                                const char *pszDataSource )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_DeleteDataSource",
                       OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyDeleteDataSource(hDriver, pszDataSource);
#endif

    CPLErr eErr = reinterpret_cast<GDALDriver *>(hDriver)->Delete( pszDataSource );
    if( eErr == CE_None )
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           OGR_Dr_GetName()                           */
/************************************************************************/

const char *OGR_Dr_GetName( OGRSFDriverH hDriver )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_GetName", nullptr );

    return reinterpret_cast<GDALDriver*>(hDriver)->GetDescription();
}

/************************************************************************/
/*                            OGR_Dr_Open()                             */
/************************************************************************/

OGRDataSourceH OGR_Dr_Open( OGRSFDriverH hDriver, const char *pszName,
                            int bUpdate )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_Open", nullptr );

    const char* const apszDrivers[] = {
        reinterpret_cast<GDALDriver*>(hDriver)->GetDescription(),
        nullptr };

#ifdef OGRAPISPY_ENABLED
    int iSnapshot = OGRAPISpyOpenTakeSnapshot(pszName, bUpdate);
#endif

    GDALDatasetH hDS = GDALOpenEx(pszName,
                                      GDAL_OF_VECTOR |
                                      ((bUpdate) ? GDAL_OF_UPDATE: 0),
                                      apszDrivers, nullptr, nullptr);

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyOpen(pszName, bUpdate, iSnapshot, &hDS);
#endif

    return reinterpret_cast<OGRDataSourceH>(hDS);
}

/************************************************************************/
/*                       OGR_Dr_TestCapability()                        */
/************************************************************************/

int OGR_Dr_TestCapability( OGRSFDriverH hDriver, const char *pszCap )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_TestCapability", 0 );
    VALIDATE_POINTER1( pszCap, "OGR_Dr_TestCapability", 0 );

    GDALDriver* poDriver = reinterpret_cast<GDALDriver *>(hDriver);
    if( EQUAL(pszCap, ODrCCreateDataSource) )
    {
        return poDriver->pfnCreate != nullptr ||
               poDriver->pfnCreateVectorOnly != nullptr;
    }
    else if( EQUAL(pszCap, ODrCDeleteDataSource) )
    {
        return poDriver->pfnDelete != nullptr ||
               poDriver->pfnDeleteDataSource != nullptr;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                       OGR_Dr_CopyDataSource()                        */
/************************************************************************/

OGRDataSourceH OGR_Dr_CopyDataSource( OGRSFDriverH hDriver,
                                      OGRDataSourceH hSrcDS,
                                      const char *pszNewName,
                                      char **papszOptions )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_CopyDataSource", nullptr );
    VALIDATE_POINTER1( hSrcDS, "OGR_Dr_CopyDataSource", nullptr );
    VALIDATE_POINTER1( pszNewName, "OGR_Dr_CopyDataSource", nullptr );

    GDALDriver* poDriver = reinterpret_cast<GDALDriver*>(hDriver);
    if( !poDriver->GetMetadataItem( GDAL_DCAP_CREATE ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "%s driver does not support data source creation.",
                  poDriver->GetDescription() );
        return nullptr;
    }

    GDALDataset *poSrcDS = GDALDataset::FromHandle(hSrcDS);
    GDALDataset *poODS =
        poDriver->Create( pszNewName, 0, 0, 0, GDT_Unknown, papszOptions );
    if( poODS == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < poSrcDS->GetLayerCount(); iLayer++ )
    {
        OGRLayer        *poLayer = poSrcDS->GetLayer(iLayer);

        if( poLayer == nullptr )
            continue;

        poODS->CopyLayer( poLayer, poLayer->GetLayerDefn()->GetName(),
                          papszOptions );
    }

    return reinterpret_cast<OGRDataSourceH>(poODS);
}

//! @endcond
