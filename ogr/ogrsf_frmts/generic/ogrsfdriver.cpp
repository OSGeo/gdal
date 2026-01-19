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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ograpispy.h"

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

OGRDataSource *OGRSFDriver::CreateDataSource(const char *, char **)

{
    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateDataSource() not supported by this driver.\n");

    return nullptr;
}

//! @endcond

/************************************************************************/
/*                      OGR_Dr_CreateDataSource()                       */
/************************************************************************/

/**
 \brief This function attempts to create a new data source based on the passed driver.

 The papszOptions argument can be used to control driver specific
 creation options.  These options are normally documented in the format
 specific documentation.

 It is important to call OGR_DS_Destroy() when the datasource is no longer
 used to ensure that all data has been properly flushed to disk.

 @deprecated Use GDALCreate()

 @param hDriver handle to the driver on which data source creation is
based.
 @param pszName the name for the new data source. UTF-8 encoded.
 @param papszOptions a StringList of name=value options.  Options are driver
specific.

 @return NULL is returned on failure, or a new OGRDataSource handle on
success.
*/

OGRDataSourceH OGR_Dr_CreateDataSource(OGRSFDriverH hDriver,
                                       const char *pszName, char **papszOptions)

{
    VALIDATE_POINTER1(hDriver, "OGR_Dr_CreateDataSource", nullptr);

    GDALDriver *poDriver = reinterpret_cast<GDALDriver *>(hDriver);

    /* MapServer had the bad habit of calling with NULL name for a memory
     * datasource */
    if (pszName == nullptr)
        pszName = "";

    OGRDataSourceH hDS = reinterpret_cast<OGRDataSourceH>(
        poDriver->Create(pszName, 0, 0, 0, GDT_Unknown, papszOptions));

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyCreateDataSource(hDriver, pszName, papszOptions,
                              reinterpret_cast<OGRDataSourceH>(hDS));
#endif

    return hDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

//! @cond Doxygen_Suppress

OGRErr OGRSFDriver::DeleteDataSource(const char *pszDataSource)

{
    (void)pszDataSource;
    CPLError(CE_Failure, CPLE_NotSupported,
             "DeleteDataSource() not supported by this driver.");

    return OGRERR_UNSUPPORTED_OPERATION;
}

//! @endcond

/************************************************************************/
/*                      OGR_Dr_DeleteDataSource()                       */
/************************************************************************/

/**
 \brief Delete a datasource.

 Delete (from the disk, in the database, ...) the named datasource.
 Normally it would be safest if the datasource was not open at the time.

 Whether this is a supported operation on this driver case be tested
 using TestCapability() on ODrCDeleteDataSource.

 @deprecated Use GDALDeleteDataset()

 @param hDriver handle to the driver on which data source deletion is
based.

 @param pszDataSource the name of the datasource to delete.

 @return OGRERR_NONE on success, and OGRERR_UNSUPPORTED_OPERATION if this
 is not supported by this driver.
*/

OGRErr OGR_Dr_DeleteDataSource(OGRSFDriverH hDriver, const char *pszDataSource)

{
    VALIDATE_POINTER1(hDriver, "OGR_Dr_DeleteDataSource",
                      OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyDeleteDataSource(hDriver, pszDataSource);
#endif

    CPLErr eErr =
        reinterpret_cast<GDALDriver *>(hDriver)->Delete(pszDataSource);
    if (eErr == CE_None)
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           OGR_Dr_GetName()                           */
/************************************************************************/

/**
  \brief Fetch name of driver (file format).

  This name should be relatively short
  (10-40 characters), and should reflect the underlying file format.  For
  instance "ESRI Shapefile".

  This function is the same as the C++ method OGRSFDriver::GetName().

  @param hDriver handle to the driver to get the name from.
  @return driver name.  This is an internal string and should not be modified
  or freed.
*/

const char *OGR_Dr_GetName(OGRSFDriverH hDriver)

{
    VALIDATE_POINTER1(hDriver, "OGR_Dr_GetName", nullptr);

    return reinterpret_cast<GDALDriver *>(hDriver)->GetDescription();
}

/************************************************************************/
/*                            OGR_Dr_Open()                             */
/************************************************************************/

/**
  \brief Attempt to open file with this driver.

  NOTE: It is *NOT* safe to cast the returned handle to
  OGRDataSource*. If a C++ object is needed, the handle should be cast to GDALDataset*.
  Similarly, the returned OGRSFDriverH handle should be cast to GDALDriver*, and
  *NOT* OGRSFDriver*.

  @deprecated Use GDALOpenEx()

  @param hDriver handle to the driver that is used to open file.
  @param pszName the name of the file, or data source to try and open.
  @param bUpdate TRUE if update access is required, otherwise FALSE (the
  default).

  @return NULL on error or if the pass name is not supported by this driver,
  otherwise a handle to a GDALDataset.  This GDALDataset should be
  closed by deleting the object when it is no longer needed.
*/

OGRDataSourceH OGR_Dr_Open(OGRSFDriverH hDriver, const char *pszName,
                           int bUpdate)

{
    VALIDATE_POINTER1(hDriver, "OGR_Dr_Open", nullptr);

    const char *const apszDrivers[] = {
        reinterpret_cast<GDALDriver *>(hDriver)->GetDescription(), nullptr};

#ifdef OGRAPISPY_ENABLED
    int iSnapshot = OGRAPISpyOpenTakeSnapshot(pszName, bUpdate);
#endif

    GDALDatasetH hDS =
        GDALOpenEx(pszName, GDAL_OF_VECTOR | ((bUpdate) ? GDAL_OF_UPDATE : 0),
                   apszDrivers, nullptr, nullptr);

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyOpen(pszName, bUpdate, iSnapshot, &hDS);
#endif

    return reinterpret_cast<OGRDataSourceH>(hDS);
}

/************************************************************************/
/*                       OGR_Dr_TestCapability()                        */
/************************************************************************/

/**
 \brief Test if capability is available.

 One of the following data source capability names can be passed into this
 function, and a TRUE or FALSE value will be returned indicating whether
 or not the capability is available for this object.

 <ul>
  <li> <b>ODrCCreateDataSource</b>: True if this driver can support creating data sources.<p>
  <li> <b>ODrCDeleteDataSource</b>: True if this driver supports deleting data sources.<p>
 </ul>

 The \#define macro forms of the capability names should be used in preference
 to the strings themselves to avoid misspelling.

 @deprecated Use GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE)

 @param hDriver handle to the driver to test the capability against.
 @param pszCap the capability to test.

 @return TRUE if capability available otherwise FALSE.

*/

int OGR_Dr_TestCapability(OGRSFDriverH hDriver, const char *pszCap)

{
    VALIDATE_POINTER1(hDriver, "OGR_Dr_TestCapability", 0);
    VALIDATE_POINTER1(pszCap, "OGR_Dr_TestCapability", 0);

    GDALDriver *poDriver = reinterpret_cast<GDALDriver *>(hDriver);
    if (EQUAL(pszCap, ODrCCreateDataSource))
    {
        return poDriver->GetMetadataItem(GDAL_DCAP_CREATE) ||
               poDriver->pfnCreate != nullptr ||
               poDriver->pfnCreateVectorOnly != nullptr;
    }
    else if (EQUAL(pszCap, ODrCDeleteDataSource))
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

/**
 \brief This function creates a new datasource by copying all the layers from the source datasource.

 It is important to call OGR_DS_Destroy() when the datasource is no longer
 used to ensure that all data has been properly flushed to disk.

 @deprecated Use GDALCreateCopy()

 @param hDriver handle to the driver on which data source creation is
based.
 @param hSrcDS source datasource
 @param pszNewName the name for the new data source.
 @param papszOptions a StringList of name=value options.  Options are driver
specific.

 @return NULL is returned on failure, or a new OGRDataSource handle on
success.
*/

OGRDataSourceH OGR_Dr_CopyDataSource(OGRSFDriverH hDriver,
                                     OGRDataSourceH hSrcDS,
                                     const char *pszNewName,
                                     char **papszOptions)

{
    VALIDATE_POINTER1(hDriver, "OGR_Dr_CopyDataSource", nullptr);
    VALIDATE_POINTER1(hSrcDS, "OGR_Dr_CopyDataSource", nullptr);
    VALIDATE_POINTER1(pszNewName, "OGR_Dr_CopyDataSource", nullptr);

    GDALDriver *poDriver = reinterpret_cast<GDALDriver *>(hDriver);
    if (!poDriver->GetMetadataItem(GDAL_DCAP_CREATE))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s driver does not support data source creation.",
                 poDriver->GetDescription());
        return nullptr;
    }

    GDALDataset *poSrcDS = GDALDataset::FromHandle(hSrcDS);
    GDALDataset *poODS =
        poDriver->Create(pszNewName, 0, 0, 0, GDT_Unknown, papszOptions);
    if (poODS == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Process each data source layer.                                 */
    /* -------------------------------------------------------------------- */
    for (int iLayer = 0; iLayer < poSrcDS->GetLayerCount(); iLayer++)
    {
        OGRLayer *poLayer = poSrcDS->GetLayer(iLayer);

        if (poLayer == nullptr)
            continue;

        poODS->CopyLayer(poLayer, poLayer->GetLayerDefn()->GetName(),
                         papszOptions);
    }

    return reinterpret_cast<OGRDataSourceH>(poODS);
}
