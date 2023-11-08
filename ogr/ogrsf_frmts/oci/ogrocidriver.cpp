/******************************************************************************
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCIDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_oci.h"
#include "ogrocidrivercore.h"

/************************************************************************/
/*                          OGROCIDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGROCIDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGROCIDriverIdentify(poOpenInfo))
        return nullptr;

    OGROCIDataSource *poDS;

    poDS = new OGROCIDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                    poOpenInfo->eAccess == GA_Update, TRUE))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                         OGROCIDriverCreate()                         */
/************************************************************************/

static GDALDataset *
OGROCIDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                   CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                   CPL_UNUSED GDALDataType eDT, CPL_UNUSED char **papszOptions)

{
    OGROCIDataSource *poDS;

    poDS = new OGROCIDataSource();

    if (!poDS->Open(pszName, nullptr, TRUE, TRUE))
    {
        delete poDS;
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Oracle driver doesn't currently support database creation.\n"
            "Please create database with Oracle tools before loading tables.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGROCI()                            */
/************************************************************************/

void RegisterOGROCI()

{
    if (!GDAL_CHECK_VERSION("OCI driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGROCIDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGROCIDriverOpen;
    poDriver->pfnCreate = OGROCIDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
