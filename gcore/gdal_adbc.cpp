/******************************************************************************
 * Name:     gdal_adbc.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core ADBC related declarations.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2024, Dewey Dunnington <dewey@voltrondata.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_adbc.h"

//! ADBC driver initialization function
static GDALAdbcLoadDriverFunc GDALAdbcLoadDriver = nullptr;

/************************************************************************/
/*                      GDALSetAdbcLoadDriverOverride()                 */
/************************************************************************/

/** When set, it is used by the OGR ADBC driver to populate AdbcDriver
 * callbacks. This provides an embedding application the opportunity to
 * locate an up-to-date version of a driver or to bundle a driver not
 * available at the system level.
 *
 * Setting it to NULL resets to the the default behavior of the ADBC driver,
 * which is use AdbcLoadDriver() from arrow-adbc/adbc_driver_manager.h or
 * to error if the OGR ADBC driver was not built against a system driver
 * manager.
 */
void GDALSetAdbcLoadDriverOverride(GDALAdbcLoadDriverFunc init_func)
{
    GDALAdbcLoadDriver = init_func;
}

/************************************************************************/
/*                    GDALGetAdbcLoadDriverOverride()                   */
/************************************************************************/

/** Gets the ADBC driver load function. This will be NULL if an explicit
 * override was not specified.
 */
GDALAdbcLoadDriverFunc GDALGetAdbcLoadDriverOverride()
{
    return GDALAdbcLoadDriver;
}
