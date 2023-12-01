/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include "ogr_dgnv8.h"
#include "cpl_conv.h"
#include "ogrteigha.h"
#include "ogrdgnv8drivercore.h"

/************************************************************************/
/*                         OGRDGNV8DriverUnload()                       */
/************************************************************************/

static void OGRDGNV8DriverUnload(GDALDriver *)
{
    CPLDebug("DGNv8", "Driver cleanup");
    OGRTEIGHADeinitialize();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDGNV8DriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRDGNV8DriverIdentify(poOpenInfo))
        return nullptr;

    if (!OGRTEIGHAInitialize())
        return nullptr;

    OGRDGNV8DataSource *poDS = new OGRDGNV8DataSource(OGRDGNV8GetServices());
    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRDGNV8DriverCreate(const char *pszName, int /* nBands */,
                                         int /* nXSize */, int /* nYSize */,
                                         GDALDataType /* eDT */,
                                         char **papszOptions)
{
    if (!OGRTEIGHAInitialize())
        return nullptr;

    OGRDGNV8DataSource *poDS = new OGRDGNV8DataSource(OGRDGNV8GetServices());
    if (!poDS->PreCreate(pszName, papszOptions))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGRDGNV8()                           */
/************************************************************************/

void RegisterOGRDGNV8()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRDGNV8DriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRDGNV8DriverOpen;
    poDriver->pfnCreate = OGRDGNV8DriverCreate;
    poDriver->pfnUnloadDriver = OGRDGNV8DriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
