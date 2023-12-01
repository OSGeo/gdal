/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBDriver class.
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
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

#include "ogr_idb.h"
#include "cpl_conv.h"

#include "ogridbdrivercore.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRIDBDriverOpen(GDALOpenInfo *poOpenInfo)

{
    OGRIDBDataSource *poDS;

    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "IDB:"))
        return nullptr;

    poDS = new OGRIDBDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename,
                    (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0, TRUE))
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

static GDALDataset *OGRIDBDriverCreate(const char *pszName, int /* nBands */,
                                       int /* nXSize */, int /* nYSize */,
                                       GDALDataType /* eDT */,
                                       char ** /*papszOptions*/)

{
    OGRIDBDataSource *poDS;

    if (!STARTS_WITH_CI(pszName, "IDB:"))
        return nullptr;

    poDS = new OGRIDBDataSource();

    if (!poDS->Open(pszName, TRUE, TRUE))
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "IDB driver doesn't currently support database creation.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRIDB()                            */
/************************************************************************/

void RegisterOGRIDB()

{
    if (!GDAL_CHECK_VERSION("IDB driver"))
        return;
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRIDBDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRIDBDriverOpen;
    poDriver->pfnCreate = OGRIDBDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
