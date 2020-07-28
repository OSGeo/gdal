/******************************************************************************
 * $Id$
 *
 * Project:  Anatrack Ranges Edge File Translator
 * Purpose:  Implements OGREdgDriver class
 * Author:   Nick Casey, nick@anatrack.com
 *
 ******************************************************************************
 * Copyright (c) 2020, Nick Casey <nick at anatrack.com>
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

#include "ogr_edg.h"

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Identify()                            */
/************************************************************************/

static int OGREdgDriverIdentify(GDALOpenInfo* poOpenInfo)
{
    // Does this appear to be an .edg file?
    return EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "edg");
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGREdgDriverOpen(GDALOpenInfo* poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == nullptr )
        return nullptr;


    if (!OGREdgDriverIdentify(poOpenInfo))
        return nullptr;

    OGREdgDataSource *poDS = new OGREdgDataSource();
    if (!poDS->Open(poOpenInfo->pszFilename))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}


/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGREdgDriverCreate(const char * pszName,
    CPL_UNUSED int nBands,
    CPL_UNUSED int nXSize,
    CPL_UNUSED int nYSize,
    CPL_UNUSED GDALDataType eDT,
    char **papszOptions)
{
    CPLAssert(nullptr != pszName);
    CPLDebug("Anatrack EDG", "Attempt to create: %s", pszName);

    OGREdgDataSource *poDS = new OGREdgDataSource();

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}


/************************************************************************/
/*                           RegisterOGREdg()                           */
/************************************************************************/

void RegisterOGREdg()

{
    if( GDALGetDriverByName("Anatrack EDG") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("Anatrack EDG");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Anatrack Ranges EDG File");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "edg");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drv_edg.html");

    poDriver->pfnOpen = OGREdgDriverOpen;
    poDriver->pfnIdentify = OGREdgDriverIdentify;
    poDriver->pfnCreate = OGREdgDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
