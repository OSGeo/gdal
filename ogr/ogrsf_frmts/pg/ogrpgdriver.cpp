/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include "ogr_pg.h"
#include "cpl_conv.h"

#include "ogrpgdrivercore.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRPGDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRPGDriverIdentify(poOpenInfo))
        return nullptr;

    OGRPGDataSource *poDS = new OGRPGDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update,
                    TRUE, poOpenInfo->papszOpenOptions))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

static GDALDataset *
OGRPGDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                  CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                  CPL_UNUSED GDALDataType eDT, char **papszOptions)

{
    OGRPGDataSource *poDS = new OGRPGDataSource();

    if (!poDS->Open(pszName, TRUE, TRUE, papszOptions))
    {
        delete poDS;
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "PostgreSQL driver doesn't currently support database creation.\n"
            "Please create database with the `createdb' command.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRPG()                            */
/************************************************************************/

void RegisterOGRPG()

{
    if (!GDAL_CHECK_VERSION("PG driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRPGDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRPGDriverOpen;
    poDriver->pfnCreate = OGRPGDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
