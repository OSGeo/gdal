/******************************************************************************
 * $I$
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiDriver.
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

#include "cpl_conv.h"
#include "ogr_idrisi.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRIdrisiOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update)
    {
        return nullptr;
    }

    // --------------------------------------------------------------------
    //      Does this appear to be a .vct file?
    // --------------------------------------------------------------------
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "vct"))
        return nullptr;

    auto poDS = std::make_unique<OGRIdrisiDataSource>();

    if (!poDS->Open(poOpenInfo->pszFilename))
    {
        return nullptr;
    }

    return poDS.release();
}

/************************************************************************/
/*                        RegisterOGRIdrisi()                           */
/************************************************************************/

void RegisterOGRIdrisi()

{
    if (GDALGetDriverByName("Idrisi") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("Idrisi");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Idrisi Vector (.vct)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "vct");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->pfnOpen = OGRIdrisiOpen;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}
