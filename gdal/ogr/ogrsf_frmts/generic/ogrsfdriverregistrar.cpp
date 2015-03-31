/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSFDriverRegistrar class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

/**
 * \brief Constructor
 *
 * Normally the driver registrar is constucted by the 
 * OGRSFDriverRegistrar::GetRegistrar() accessor which ensures singleton
 * status.  
 */

OGRSFDriverRegistrar::OGRSFDriverRegistrar()

{
}

/************************************************************************/
/*                       ~OGRSFDriverRegistrar()                        */
/************************************************************************/

OGRSFDriverRegistrar::~OGRSFDriverRegistrar()

{
}

/************************************************************************/
/*                           GetRegistrar()                             */
/************************************************************************/


OGRSFDriverRegistrar *OGRSFDriverRegistrar::GetRegistrar()
{
    static      OGRSFDriverRegistrar oSingleton;
    return &oSingleton;
}

/************************************************************************/
/*                           OGRCleanupAll()                            */
/************************************************************************/

#if defined(WIN32) && defined(_MSC_VER)
#include "ogremulatedtransaction.h"
void OGRRegisterMutexedDataSource();
void OGRRegisterMutexedLayer();
int OGRwillNeverBeTrue = FALSE;
#endif

/**
 * \brief Cleanup all OGR related resources. 
 *
 * FIXME
 */
void OGRCleanupAll()

{
    GDALDestroyDriverManager();
#if defined(WIN32) && defined(_MSC_VER)
// Horrible hack: for some reason MSVC doesn't export those classes&symbols
// if they are not referenced from the DLL itself
    if(OGRwillNeverBeTrue)
    {
        OGRRegisterMutexedDataSource();
        OGRRegisterMutexedLayer();
        OGRCreateEmulatedTransactionDataSourceWrapper(NULL,NULL,FALSE,FALSE);
    }
#endif
}

/************************************************************************/
/*                              OGROpen()                               */
/************************************************************************/

OGRDataSourceH OGROpen( const char *pszName, int bUpdate,
                        OGRSFDriverH *pahDriverList )

{
    VALIDATE_POINTER1( pszName, "OGROpen", NULL );

#ifdef OGRAPISPY_ENABLED
    int iSnapshot = OGRAPISpyOpenTakeSnapshot(pszName, bUpdate);
#endif

    GDALDatasetH hDS = GDALOpenEx(pszName, GDAL_OF_VECTOR |
                            ((bUpdate) ? GDAL_OF_UPDATE: 0), NULL, NULL, NULL);
    if( hDS != NULL && pahDriverList != NULL )
        *pahDriverList = (OGRSFDriverH) GDALGetDatasetDriver(hDS);

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyOpen(pszName, bUpdate, iSnapshot, &hDS);
#endif

    return (OGRDataSourceH) hDS;
}

/************************************************************************/
/*                           OGROpenShared()                            */
/************************************************************************/

OGRDataSourceH OGROpenShared( const char *pszName, int bUpdate,
                              OGRSFDriverH *pahDriverList )

{
    VALIDATE_POINTER1( pszName, "OGROpenShared", NULL );

    GDALDatasetH hDS = GDALOpenEx(pszName, GDAL_OF_VECTOR |
            ((bUpdate) ? GDAL_OF_UPDATE: 0) | GDAL_OF_SHARED, NULL, NULL, NULL);
    if( hDS != NULL && pahDriverList != NULL )
        *pahDriverList = (OGRSFDriverH) GDALGetDatasetDriver(hDS);
    return (OGRDataSourceH) hDS;
}

/************************************************************************/
/*                        OGRReleaseDataSource()                        */
/************************************************************************/

OGRErr OGRReleaseDataSource( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGRReleaseDataSource", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpyPreClose(hDS);
#endif
    GDALClose( (GDALDatasetH) hDS );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpyPostClose(hDS);
#endif

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetOpenDSCount()                           */
/************************************************************************/

int OGRSFDriverRegistrar::GetOpenDSCount()
{
    CPLError(CE_Failure, CPLE_AppDefined, "Stub implementation in GDAL 2.0");
    return 0;
}


/************************************************************************/
/*                         OGRGetOpenDSCount()                          */
/************************************************************************/

int OGRGetOpenDSCount()

{
    return OGRSFDriverRegistrar::GetRegistrar()->GetOpenDSCount();
}

/************************************************************************/
/*                             GetOpenDS()                              */
/************************************************************************/

OGRDataSource *OGRSFDriverRegistrar::GetOpenDS( CPL_UNUSED int iDS )
{
    CPLError(CE_Failure, CPLE_AppDefined, "Stub implementation in GDAL 2.0");
    return NULL;
}

/************************************************************************/
/*                            OGRGetOpenDS()                            */
/************************************************************************/

OGRDataSourceH OGRGetOpenDS( int iDS )

{
    return (OGRDataSourceH) OGRSFDriverRegistrar::GetRegistrar()->GetOpenDS( iDS );
}

/************************************************************************/
/*                          OpenWithDriverArg()                         */
/************************************************************************/

GDALDataset* OGRSFDriverRegistrar::OpenWithDriverArg(GDALDriver* poDriver,
                                                 GDALOpenInfo* poOpenInfo)
{
    OGRDataSource* poDS = (OGRDataSource*)
                ((OGRSFDriver*)poDriver)->Open(poOpenInfo->pszFilename,
                                        poOpenInfo->eAccess == GA_Update);
    if( poDS != NULL )
        poDS->SetDescription( poDS->GetName() );
    return poDS;
}

/************************************************************************/
/*                          CreateVectorOnly()                          */
/************************************************************************/

GDALDataset* OGRSFDriverRegistrar::CreateVectorOnly( GDALDriver* poDriver,
                                                     const char * pszName,
                                                     char ** papszOptions )
{
    OGRDataSource* poDS = (OGRDataSource*)
        ((OGRSFDriver*)poDriver)->CreateDataSource(pszName, papszOptions);
    if( poDS != NULL && poDS->GetName() != NULL )
        poDS->SetDescription( poDS->GetName() );
    return poDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

CPLErr OGRSFDriverRegistrar::DeleteDataSource( GDALDriver* poDriver,
                                              const char * pszName )
{
    if( ((OGRSFDriver*)poDriver)->DeleteDataSource(pszName) == OGRERR_NONE )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

void OGRSFDriverRegistrar::RegisterDriver( OGRSFDriver * poDriver )

{
    GDALDriver* poGDALDriver = (GDALDriver*) GDALGetDriverByName( poDriver->GetName() ) ;
    if( poGDALDriver == NULL)
    {
        poDriver->SetDescription( poDriver->GetName() );
        poDriver->SetMetadataItem("OGR_DRIVER", "YES");

        if( poDriver->GetMetadataItem(GDAL_DMD_LONGNAME) == NULL )
            poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, poDriver->GetName() );

        poDriver->pfnOpenWithDriverArg = OpenWithDriverArg;

        if( poDriver->TestCapability(ODrCCreateDataSource) )
        {
            poDriver->SetMetadataItem( GDAL_DCAP_CREATE, "YES" );
            poDriver->pfnCreateVectorOnly = CreateVectorOnly;
        }
        if( poDriver->TestCapability(ODrCDeleteDataSource) )
        {
            poDriver->pfnDeleteDataSource = DeleteDataSource;
        }

        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
    else
    {
        if( poGDALDriver->GetMetadataItem("OGR_DRIVER") == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "A non OGR driver is registered with the same name: %s", poDriver->GetName());
        }
        delete poDriver;
    }
}

/************************************************************************/
/*                         OGRRegisterDriver()                          */
/************************************************************************/

void OGRRegisterDriver( OGRSFDriverH hDriver )

{
    VALIDATE_POINTER0( hDriver, "OGRRegisterDriver" );
    
    GetGDALDriverManager()->RegisterDriver( (GDALDriver*)hDriver );
}

/************************************************************************/
/*                        OGRDeregisterDriver()                         */
/************************************************************************/

void OGRDeregisterDriver( OGRSFDriverH hDriver )

{
    VALIDATE_POINTER0( hDriver, "OGRDeregisterDriver" );
    
    GetGDALDriverManager()->DeregisterDriver( (GDALDriver*)hDriver );
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

int OGRSFDriverRegistrar::GetDriverCount()

{
    /* We must be careful only to return drivers that are actual OGRSFDriver* */
    GDALDriverManager* poDriverManager = GetGDALDriverManager();
    int nTotal = poDriverManager->GetDriverCount();
    int nOGRDriverCount = 0;
    for(int i=0;i<nTotal;i++)
    {
        GDALDriver* poDriver = poDriverManager->GetDriver(i);
        if( poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) != NULL )
            nOGRDriverCount ++;
    }
    return nOGRDriverCount;
}

/************************************************************************/
/*                         OGRGetDriverCount()                          */
/************************************************************************/

int OGRGetDriverCount()

{
    return OGRSFDriverRegistrar::GetRegistrar()->GetDriverCount();
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

GDALDriver *OGRSFDriverRegistrar::GetDriver( int iDriver )

{
    /* We must be careful only to return drivers that are actual OGRSFDriver* */
    GDALDriverManager* poDriverManager = GetGDALDriverManager();
    int nTotal = poDriverManager->GetDriverCount();
    int nOGRDriverCount = 0;
    for(int i=0;i<nTotal;i++)
    {
        GDALDriver* poDriver = poDriverManager->GetDriver(i);
        if( poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) != NULL )
        {
            if( nOGRDriverCount == iDriver )
                return poDriver;
            nOGRDriverCount ++;
        }
    }
    return NULL;
}

/************************************************************************/
/*                            OGRGetDriver()                            */
/************************************************************************/

OGRSFDriverH OGRGetDriver( int iDriver )

{
    return (OGRSFDriverH) OGRSFDriverRegistrar::GetRegistrar()->GetDriver( iDriver );
}

/************************************************************************/
/*                          GetDriverByName()                           */
/************************************************************************/

GDALDriver *OGRSFDriverRegistrar::GetDriverByName( const char * pszName )

{
    GDALDriverManager* poDriverManager = GetGDALDriverManager();
    GDALDriver* poGDALDriver =
        poDriverManager->GetDriverByName(CPLSPrintf("OGR_%s", pszName));
    if( poGDALDriver == NULL )
        poGDALDriver = poDriverManager->GetDriverByName(pszName);
    if( poGDALDriver == NULL ||
        poGDALDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == NULL )
        return NULL;
    return poGDALDriver;
}

/************************************************************************/
/*                         OGRGetDriverByName()                         */
/************************************************************************/

OGRSFDriverH OGRGetDriverByName( const char *pszName )

{
    VALIDATE_POINTER1( pszName, "OGRGetDriverByName", NULL );

    return (OGRSFDriverH) 
        OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName( pszName );
}
