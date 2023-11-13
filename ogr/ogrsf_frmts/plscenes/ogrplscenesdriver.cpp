/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  PlanetLabs scene driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015-2016, Planet Labs
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

#include "ogr_plscenes.h"
#include "ogrplscenesdrivercore.h"

/************************************************************************/
/*                            OGRPLScenesOpen()                         */
/************************************************************************/

static GDALDataset *OGRPLScenesOpen(GDALOpenInfo *poOpenInfo)
{
    if (!OGRPLSCENESDriverIdentify(poOpenInfo) ||
        poOpenInfo->eAccess == GA_Update)
        return nullptr;

    char **papszOptions = CSLTokenizeStringComplex(
        poOpenInfo->pszFilename + strlen("PLScenes:"), ",", TRUE, FALSE);
    CPLString osVersion = CSLFetchNameValueDef(
        papszOptions, "version",
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "VERSION", ""));
    CSLDestroy(papszOptions);

    if (EQUAL(osVersion, "v0") || EQUAL(osVersion, "v1"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This API version has been removed or deprecated. "
                 "Please use DATA_V1 API instead");
        return nullptr;
    }
    else if (EQUAL(osVersion, "data_v1") || EQUAL(osVersion, ""))
    {
        return OGRPLScenesDataV1Dataset::Open(poOpenInfo);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unhandled API version: %s",
                 osVersion.c_str());
        return nullptr;
    }
}

/************************************************************************/
/*                        RegisterOGRPLSCENES()                         */
/************************************************************************/

void RegisterOGRPLSCENES()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRPLSCENESDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRPLScenesOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
