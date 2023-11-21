/******************************************************************************
 *
 * Project:  XLS Translator
 * Purpose:  Implements OGRXLSDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_xls.h"
#include "ogrxlsdrivercore.h"
#include "cpl_conv.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRXLSDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if ((poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0)
    {
        return nullptr;
    }

    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "XLS"))
    {
        return nullptr;
    }

    OGRXLSDataSource *poDS = new OGRXLSDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, false))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRXLS()                           */
/************************************************************************/

void RegisterOGRXLS()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRXLSDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRXLSDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
