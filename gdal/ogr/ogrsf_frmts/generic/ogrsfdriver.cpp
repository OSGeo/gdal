/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRSFDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

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
              
    return NULL;
}

/************************************************************************/
/*                      OGR_Dr_CreateDataSource()                       */
/************************************************************************/

OGRDataSourceH OGR_Dr_CreateDataSource( OGRSFDriverH hDriver,
                                        const char *pszName, 
                                        char ** papszOptions )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_CreateDataSource", NULL );

    OGRSFDriver* poDriver = (OGRSFDriver *) hDriver;
    CPLAssert( NULL != poDriver );

    OGRDataSource* poDS = NULL;
    poDS = poDriver->CreateDataSource( pszName, papszOptions );

    /* This fix is explained in Ticket #1223 */
    if( NULL != poDS )
    {
        poDS->SetDriver( poDriver );
        CPLAssert( NULL != poDS->GetDriver() );
    }
    else
    {
        CPLDebug( "OGR", "CreateDataSource operation failed. NULL pointer returned." );
    }

    return (OGRDataSourceH) poDS;
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

    return ((OGRSFDriver *) hDriver)->DeleteDataSource( pszDataSource );
}

/************************************************************************/
/*                           OGR_Dr_GetName()                           */
/************************************************************************/

const char *OGR_Dr_GetName( OGRSFDriverH hDriver )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_GetName", NULL );

    return ((OGRSFDriver *) hDriver)->GetName();
}

/************************************************************************/
/*                            OGR_Dr_Open()                             */
/************************************************************************/

OGRDataSourceH OGR_Dr_Open( OGRSFDriverH hDriver, const char *pszName, 
                            int bUpdate )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_Open", NULL );

    OGRDataSource *poDS = ((OGRSFDriver *)hDriver)->Open( pszName, bUpdate );

    if( poDS != NULL && poDS->GetDriver() == NULL )
        poDS->SetDriver( (OGRSFDriver *)hDriver );

    return (OGRDataSourceH) poDS;
}

/************************************************************************/
/*                       OGR_Dr_TestCapability()                        */
/************************************************************************/

int OGR_Dr_TestCapability( OGRSFDriverH hDriver, const char *pszCap )

{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_TestCapability", 0 );
    VALIDATE_POINTER1( pszCap, "OGR_Dr_TestCapability", 0 );

    return ((OGRSFDriver *) hDriver)->TestCapability( pszCap );
}

/************************************************************************/
/*                           CopyDataSource()                           */
/************************************************************************/

OGRDataSource *OGRSFDriver::CopyDataSource( OGRDataSource *poSrcDS, 
                                            const char *pszNewName,
                                            char **papszOptions )

{
    if( !TestCapability( ODrCCreateDataSource ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "%s driver does not support data source creation.",
                  GetName() );
        return NULL;
    }

    OGRDataSource *poODS;

    poODS = CreateDataSource( pszNewName, papszOptions );
    if( poODS == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < poSrcDS->GetLayerCount(); iLayer++ )
    {
        OGRLayer        *poLayer = poSrcDS->GetLayer(iLayer);

        if( poLayer == NULL )
            continue;

        poODS->CopyLayer( poLayer, poLayer->GetLayerDefn()->GetName(), 
                          papszOptions );
    }

    /* Make sure that the driver is attached to the created datasource */
    /* It is also done in OGR_Dr_CopyDataSource() C method, in case */
    /* another C++ implementation forgets to do it. Currently (Nov 2011), */
    /* this implementation is the only one in the OGR source tree */
    if( poODS != NULL && poODS->GetDriver() == NULL )
        poODS->SetDriver( this );

    return poODS;
}

/************************************************************************/
/*                       OGR_Dr_CopyDataSource()                        */
/************************************************************************/

OGRDataSourceH OGR_Dr_CopyDataSource( OGRSFDriverH hDriver, 
                                      OGRDataSourceH hSrcDS, 
                                      const char *pszNewName,
                                      char **papszOptions )
                                      
{
    VALIDATE_POINTER1( hDriver, "OGR_Dr_CopyDataSource", NULL );
    VALIDATE_POINTER1( hSrcDS, "OGR_Dr_CopyDataSource", NULL );
    VALIDATE_POINTER1( pszNewName, "OGR_Dr_CopyDataSource", NULL );

    OGRDataSource* poDS =
        ((OGRSFDriver *) hDriver)->CopyDataSource( 
            (OGRDataSource *) hSrcDS, pszNewName, papszOptions );

    /* Make sure that the driver is attached to the created datasource */
    /* if not already done by the implementation of the CopyDataSource() */
    /* method */
    if( poDS != NULL && poDS->GetDriver() == NULL )
        poDS->SetDriver( (OGRSFDriver *)hDriver );

    return (OGRDataSourceH)poDS;
}

