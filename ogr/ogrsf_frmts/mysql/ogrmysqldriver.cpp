/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_mysql.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

#include "ogrmysqldrivercore.h"

static CPLMutex *hMutex = nullptr;
static int bInitialized = FALSE;

/************************************************************************/
/*                        OGRMySQLDriverUnload()                        */
/************************************************************************/

static void OGRMySQLDriverUnload(CPL_UNUSED GDALDriver *poDriver)
{
    if (bInitialized)
    {
        mysql_library_end();
        bInitialized = FALSE;
    }
    if (hMutex != nullptr)
    {
        CPLDestroyMutex(hMutex);
        hMutex = nullptr;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRMySQLDriverOpen(GDALOpenInfo *poOpenInfo)

{
    OGRMySQLDataSource *poDS;

    if (!OGRMySQLDriverIdentify(poOpenInfo))
        return nullptr;

    {
        CPLMutexHolderD(&hMutex);
        if (!bInitialized)
        {
            if (mysql_library_init(0, nullptr, nullptr))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not initialize MySQL library");
                return nullptr;
            }
            bInitialized = TRUE;
        }
    }

    poDS = new OGRMySQLDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                    poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRMySQLDriverCreate(const char *pszName,
                                         CPL_UNUSED int nBands,
                                         CPL_UNUSED int nXSize,
                                         CPL_UNUSED int nYSize,
                                         CPL_UNUSED GDALDataType eDT,
                                         CPL_UNUSED char **papszOptions)
{
    OGRMySQLDataSource *poDS;

    poDS = new OGRMySQLDataSource();

    if (!poDS->Open(pszName, nullptr, TRUE))
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MySQL driver doesn't currently support database creation.\n"
                 "Please create database before using.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                          RegisterOGRMySQL()                          */
/************************************************************************/

void RegisterOGRMySQL()

{
    if (!GDAL_CHECK_VERSION("MySQL driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRMySQLDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRMySQLDriverOpen;
    poDriver->pfnCreate = OGRMySQLDriverCreate;
    poDriver->pfnUnloadDriver = OGRMySQLDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
