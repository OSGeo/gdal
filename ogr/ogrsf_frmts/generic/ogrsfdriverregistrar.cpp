/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSFDriverRegistrar class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ograpispy.h"

/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

/**
 * \brief Constructor
 *
 * Normally the driver registrar is constructed by the
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

//! @cond Doxygen_Suppress
/************************************************************************/
/*                            GetRegistrar()                            */
/************************************************************************/

OGRSFDriverRegistrar *OGRSFDriverRegistrar::GetRegistrar()
{
    static OGRSFDriverRegistrar oSingleton;
    return &oSingleton;
}

//! @endcond

/************************************************************************/
/*                           OGRCleanupAll()                            */
/************************************************************************/

/**
 * \brief Cleanup all OGR related resources.
 *
 * \see GDALDestroy()
 * \deprecated Use GDALDestroy() instead
 */
void OGRCleanupAll()

{
    GDALDestroyDriverManager();
}

/************************************************************************/
/*                              OGROpen()                               */
/************************************************************************/

/**
  \brief Open a file / data source with one of the registered drivers.

  This function loops through all the drivers registered with the driver
  manager trying each until one succeeds with the given data source.

  If this function fails, CPLGetLastErrorMsg() can be used to check if there
  is an error message explaining why.

  For drivers supporting the VSI virtual file API, it is possible to open
  a file in a .zip archive (see VSIInstallZipFileHandler()), in a .tar/.tar.gz/.tgz archive
  (see VSIInstallTarFileHandler()) or on a HTTP / FTP server (see VSIInstallCurlFileHandler())

  NOTE: It is *NOT* safe to cast the returned handle to
  OGRDataSource*. If a C++ object is needed, the handle should be cast to GDALDataset*.
  Similarly, the returned OGRSFDriverH handle should be cast to GDALDriver*, and
  *NOT* OGRSFDriver*.

  @deprecated Use GDALOpenEx()

  @param pszName the name of the file, or data source to open.
  @param bUpdate FALSE for read-only access (the default) or TRUE for
         read-write access.
  @param pahDriverList if non-NULL, this argument will be updated with a
         pointer to the driver which was used to open the data source.

  @return NULL on error or if the pass name is not supported by this driver,
  otherwise a handle to a GDALDataset.  This GDALDataset should be
  closed by deleting the object when it is no longer needed.

  Example:

  \code{.cpp}
    OGRDataSourceH hDS;
    OGRSFDriverH *pahDriver;

    hDS = OGROpen( "polygon.shp", 0, pahDriver );
    if( hDS == NULL )
    {
        return;
    }

    ... use the data source ...

    OGRReleaseDataSource( hDS );
  \endcode

*/

OGRDataSourceH OGROpen(const char *pszName, int bUpdate,
                       OGRSFDriverH *pahDriverList)

{
    VALIDATE_POINTER1(pszName, "OGROpen", nullptr);

    GDALDatasetH hDS =
        GDALOpenEx(pszName, GDAL_OF_VECTOR | ((bUpdate) ? GDAL_OF_UPDATE : 0),
                   nullptr, nullptr, nullptr);
    if (hDS != nullptr && pahDriverList != nullptr)
        *pahDriverList =
            reinterpret_cast<OGRSFDriverH>(GDALGetDatasetDriver(hDS));

    return reinterpret_cast<OGRDataSourceH>(hDS);
}

/************************************************************************/
/*                           OGROpenShared()                            */
/************************************************************************/

/**
  \brief Open a file / data source with one of the registered drivers if not
  already opened, or increment reference count of already opened data source
  previously opened with OGROpenShared()

  This function loops through all the drivers registered with the driver
  manager trying each until one succeeds with the given data source.

  If this function fails, CPLGetLastErrorMsg() can be used to check if there
  is an error message explaining why.

  NOTE: It is *NOT* safe to cast the returned handle to
  OGRDataSource*. If a C++ object is needed, the handle should be cast to GDALDataset*.
  Similarly, the returned OGRSFDriverH handle should be cast to GDALDriver*, and
  *NOT* OGRSFDriver*.

  @deprecated Use GDALOpenEx()

  @param pszName the name of the file, or data source to open.
  @param bUpdate FALSE for read-only access (the default) or TRUE for
         read-write access.
  @param pahDriverList if non-NULL, this argument will be updated with a
         pointer to the driver which was used to open the data source.

  @return NULL on error or if the pass name is not supported by this driver,
  otherwise a handle to a GDALDataset.  This GDALDataset should be
  closed by deleting the object when it is no longer needed.

  Example:

  \code{.cpp}
    OGRDataSourceH  hDS;
    OGRSFDriverH        *pahDriver;

    hDS = OGROpenShared( "polygon.shp", 0, pahDriver );
    if( hDS == NULL )
    {
        return;
    }

    ... use the data source ...

    OGRReleaseDataSource( hDS );
  \endcode

*/
OGRDataSourceH OGROpenShared(const char *pszName, int bUpdate,
                             OGRSFDriverH *pahDriverList)

{
    VALIDATE_POINTER1(pszName, "OGROpenShared", nullptr);

    GDALDatasetH hDS = GDALOpenEx(
        pszName,
        GDAL_OF_VECTOR | ((bUpdate) ? GDAL_OF_UPDATE : 0) | GDAL_OF_SHARED,
        nullptr, nullptr, nullptr);
    if (hDS != nullptr && pahDriverList != nullptr)
        *pahDriverList =
            reinterpret_cast<OGRSFDriverH>(GDALGetDatasetDriver(hDS));
    return reinterpret_cast<OGRDataSourceH>(hDS);
}

/************************************************************************/
/*                        OGRReleaseDataSource()                        */
/************************************************************************/

/**
\brief Drop a reference to this datasource, and if the reference count drops to zero close (destroy) the datasource.

Internally this actually calls
the OGRSFDriverRegistrar::ReleaseDataSource() method.  This method is
essentially a convenient alias.

@deprecated Use GDALClose()

@param hDS handle to the data source to release

@return OGRERR_NONE on success or an error code.
*/

OGRErr OGRReleaseDataSource(OGRDataSourceH hDS)

{
    VALIDATE_POINTER1(hDS, "OGRReleaseDataSource", OGRERR_INVALID_HANDLE);

    GDALClose(reinterpret_cast<GDALDatasetH>(hDS));

    return OGRERR_NONE;
}

//! @cond Doxygen_Suppress
/************************************************************************/
/*                           GetOpenDSCount()                           */
/************************************************************************/

int OGRSFDriverRegistrar::GetOpenDSCount()
{
    CPLError(CE_Failure, CPLE_AppDefined, "Stub implementation");
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

OGRDataSource *OGRSFDriverRegistrar::GetOpenDS(CPL_UNUSED int iDS)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Stub implementation");
    return nullptr;
}

/************************************************************************/
/*                            OGRGetOpenDS()                            */
/************************************************************************/

OGRDataSourceH OGRGetOpenDS(int iDS)

{
    return reinterpret_cast<OGRDataSourceH>(
        OGRSFDriverRegistrar::GetRegistrar()->GetOpenDS(iDS));
}

/************************************************************************/
/*                         OpenWithDriverArg()                          */
/************************************************************************/

GDALDataset *OGRSFDriverRegistrar::OpenWithDriverArg(GDALDriver *poDriver,
                                                     GDALOpenInfo *poOpenInfo)
{
    OGRDataSource *poDS = reinterpret_cast<OGRDataSource *>(
        reinterpret_cast<OGRSFDriver *>(poDriver)->Open(
            poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update));
    if (poDS != nullptr)
        poDS->SetDescription(poDS->GetName());
    return poDS;
}

/************************************************************************/
/*                          CreateVectorOnly()                          */
/************************************************************************/

GDALDataset *OGRSFDriverRegistrar::CreateVectorOnly(GDALDriver *poDriver,
                                                    const char *pszName,
                                                    CSLConstList papszOptions)
{
    OGRDataSource *poDS = reinterpret_cast<OGRDataSource *>(
        reinterpret_cast<OGRSFDriver *>(poDriver)->CreateDataSource(
            pszName, const_cast<char **>(papszOptions)));
    if (poDS != nullptr && poDS->GetName() != nullptr)
        poDS->SetDescription(poDS->GetName());
    return poDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

CPLErr OGRSFDriverRegistrar::DeleteDataSource(GDALDriver *poDriver,
                                              const char *pszName)
{
    if (reinterpret_cast<OGRSFDriver *>(poDriver)->DeleteDataSource(pszName) ==
        OGRERR_NONE)
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

void OGRSFDriverRegistrar::RegisterDriver(OGRSFDriver *poDriver)

{
    GDALDriver *poGDALDriver =
        GDALDriver::FromHandle(GDALGetDriverByName(poDriver->GetName()));
    if (poGDALDriver == nullptr)
    {
        poDriver->SetDescription(poDriver->GetName());
        poDriver->SetMetadataItem("OGR_DRIVER", "YES");

        if (poDriver->GetMetadataItem(GDAL_DMD_LONGNAME) == nullptr)
            poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, poDriver->GetName());

        poDriver->pfnOpenWithDriverArg = OpenWithDriverArg;

        if (poDriver->TestCapability(ODrCCreateDataSource))
        {
            poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
            poDriver->pfnCreateVectorOnly = CreateVectorOnly;
        }
        if (poDriver->TestCapability(ODrCDeleteDataSource))
        {
            poDriver->pfnDeleteDataSource = DeleteDataSource;
        }

        poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
    else
    {
        if (poGDALDriver->GetMetadataItem("OGR_DRIVER") == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "A non OGR driver is registered with the same name: %s",
                     poDriver->GetName());
        }
        delete poDriver;
    }
}

/************************************************************************/
/*                         OGRRegisterDriver()                          */
/************************************************************************/

void OGRRegisterDriver(OGRSFDriverH hDriver)

{
    VALIDATE_POINTER0(hDriver, "OGRRegisterDriver");

    GetGDALDriverManager()->RegisterDriver(GDALDriver::FromHandle(hDriver));
}

/************************************************************************/
/*                        OGRDeregisterDriver()                         */
/************************************************************************/

void OGRDeregisterDriver(OGRSFDriverH hDriver)

{
    VALIDATE_POINTER0(hDriver, "OGRDeregisterDriver");

    GetGDALDriverManager()->DeregisterDriver(GDALDriver::FromHandle(hDriver));
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

int OGRSFDriverRegistrar::GetDriverCount()

{
    /* We must be careful only to return drivers that are actual OGRSFDriver* */
    GDALDriverManager *poDriverManager = GetGDALDriverManager();
    int nTotal = poDriverManager->GetDriverCount();
    int nOGRDriverCount = 0;
    for (int i = 0; i < nTotal; i++)
    {
        GDALDriver *poDriver = poDriverManager->GetDriver(i);
        if (poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) != nullptr)
            nOGRDriverCount++;
    }
    return nOGRDriverCount;
}

//! @endcond

/************************************************************************/
/*                         OGRGetDriverCount()                          */
/************************************************************************/

/**
  \brief Fetch the number of registered drivers.

  @deprecated Use GDALGetDriverCount()

  @return the drivers count.

*/
int OGRGetDriverCount()

{
    return OGRSFDriverRegistrar::GetRegistrar()->GetDriverCount();
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALDriver *OGRSFDriverRegistrar::GetDriver(int iDriver)

{
    /* We must be careful only to return drivers that are actual OGRSFDriver* */
    GDALDriverManager *poDriverManager = GetGDALDriverManager();
    int nTotal = poDriverManager->GetDriverCount();
    int nOGRDriverCount = 0;
    for (int i = 0; i < nTotal; i++)
    {
        GDALDriver *poDriver = poDriverManager->GetDriver(i);
        if (poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) != nullptr)
        {
            if (nOGRDriverCount == iDriver)
                return poDriver;
            nOGRDriverCount++;
        }
    }
    return nullptr;
}

//! @endcond

/************************************************************************/
/*                            OGRGetDriver()                            */
/************************************************************************/

/**
  \brief Fetch the indicated driver.

  NOTE: It is *NOT* safe to cast the returned handle to
  OGRSFDriver*. If a C++ object is needed, the handle should be cast to GDALDriver*.

  @deprecated Use GDALGetDriver()

  @param iDriver the driver index, from 0 to GetDriverCount()-1.

  @return handle to the driver, or NULL if iDriver is out of range.

*/

OGRSFDriverH OGRGetDriver(int iDriver)

{
    return reinterpret_cast<OGRSFDriverH>(
        OGRSFDriverRegistrar::GetRegistrar()->GetDriver(iDriver));
}

/************************************************************************/
/*                          GetDriverByName()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALDriver *OGRSFDriverRegistrar::GetDriverByName(const char *pszName)

{
    GDALDriverManager *poDriverManager = GetGDALDriverManager();
    GDALDriver *poGDALDriver =
        poDriverManager->GetDriverByName(CPLSPrintf("OGR_%s", pszName));
    if (poGDALDriver == nullptr)
        poGDALDriver = poDriverManager->GetDriverByName(pszName);
    if (poGDALDriver == nullptr ||
        poGDALDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr)
        return nullptr;
    return poGDALDriver;
}

//! @endcond

/************************************************************************/
/*                         OGRGetDriverByName()                         */
/************************************************************************/

/**
  \brief Fetch the indicated driver.

  NOTE: It is *NOT* safe to cast the returned handle to
  OGRSFDriver*. If a C++ object is needed, the handle should be cast to GDALDriver*.

  @deprecated Use GDALGetDriverByName()

  @param pszName the driver name

  @return the driver, or NULL if no driver with that name is found
*/

OGRSFDriverH OGRGetDriverByName(const char *pszName)

{
    VALIDATE_POINTER1(pszName, "OGRGetDriverByName", nullptr);

    return reinterpret_cast<OGRSFDriverH>(
        OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(pszName));
}
