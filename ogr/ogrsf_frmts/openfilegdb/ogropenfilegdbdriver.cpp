/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_openfilegdb.h"
#include "ogropenfilegdbdrivercore.h"

#include <cstddef>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"

// g++ -O2 -Wall -Wextra -g -shared -fPIC ogr/ogrsf_frmts/openfilegdb/*.cpp
// -o ogr_OpenFileGDB.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts
// -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/openfilegdb -L. -lgdal

extern "C" void RegisterOGROpenFileGDB();

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROpenFileGDBDriverOpen(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
#ifdef FOR_FUSIL
    CPLString osOrigFilename(pszFilename);
#endif
    if (OGROpenFileGDBDriverIdentify(poOpenInfo, pszFilename) ==
        GDAL_IDENTIFY_FALSE)
        return nullptr;

#ifdef FOR_FUSIL
    const char *pszSrcDir = CPLGetConfigOption("FUSIL_SRC_DIR", NULL);
    if (pszSrcDir != NULL && VSIStatL(osOrigFilename, &stat) == 0 &&
        VSI_ISREG(stat.st_mode))
    {
        /* Copy all files from FUSIL_SRC_DIR to directory of pszFilename */
        /* except pszFilename itself */
        CPLString osSave(pszFilename);
        char **papszFiles = VSIReadDir(pszSrcDir);
        for (int i = 0; papszFiles[i] != NULL; i++)
        {
            if (strcmp(papszFiles[i], CPLGetFilename(osOrigFilename)) != 0)
            {
                CPLCopyFile(CPLFormFilename(CPLGetPath(osOrigFilename),
                                            papszFiles[i], NULL),
                            CPLFormFilename(pszSrcDir, papszFiles[i], NULL));
            }
        }
        CSLDestroy(papszFiles);
        pszFilename = CPLFormFilename("", osSave.c_str(), NULL);
    }
#endif

#ifdef DEBUG
    /* For AFL, so that .cur_input is detected as the archive filename */
    if (poOpenInfo->fpL != nullptr &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input"))
    {
        GDALOpenInfo oOpenInfo(
            (CPLString("/vsitar/") + poOpenInfo->pszFilename).c_str(),
            poOpenInfo->nOpenFlags);
        oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
        return OGROpenFileGDBDriverOpen(&oOpenInfo);
    }
#endif

    auto poDS = std::make_unique<OGROpenFileGDBDataSource>();
    bool bRetryFileGDB = false;
    if (poDS->Open(poOpenInfo, bRetryFileGDB))
    {
        if (poDS->GetSubdatasets().size() == 2)
        {
            // If there is a single raster dataset, open it right away.
            GDALOpenInfo oOpenInfo(
                poDS->GetSubdatasets().FetchNameValue("SUBDATASET_1_NAME"),
                poOpenInfo->nOpenFlags);
            poDS = std::make_unique<OGROpenFileGDBDataSource>();
            if (poDS->Open(&oOpenInfo, bRetryFileGDB))
            {
                poDS->SetDescription(poOpenInfo->pszFilename);
            }
            else
            {
                poDS.reset();
            }
        }
        return poDS.release();
    }
    else if (bRetryFileGDB)
    {
        auto poDriver = GetGDALDriverManager()->GetDriverByName("FileGDB");
        if (poDriver)
        {
            GDALOpenInfo oOpenInfo(pszFilename, poOpenInfo->nOpenFlags);
            CPLStringList aosOpenOptions;
            aosOpenOptions.SetNameValue("@MAY_USE_OPENFILEGDB", "NO");
            oOpenInfo.papszOpenOptions = aosOpenOptions.List();
            return poDriver->Open(&oOpenInfo, false);
        }
    }

    return nullptr;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGROpenFileGDBDriverCreate(const char *pszName, int nXSize,
                                               int nYSize, int nBands,
                                               GDALDataType eType,
                                               char ** /* papszOptions*/)

{
    if (!(nXSize == 0 && nYSize == 0 && nBands == 0 && eType == GDT_Unknown))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OpenFileGDB::Create(): only vector datasets supported");
        return nullptr;
    }

    auto poDS = std::make_unique<OGROpenFileGDBDataSource>();
    if (!poDS->Create(pszName))
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                     OGROpenFileGDBDriverDelete()                     */
/************************************************************************/

static CPLErr OGROpenFileGDBDriverDelete(const char *pszFilename)
{
    CPLStringList aosFiles(VSIReadDir(pszFilename));
    if (aosFiles.empty())
        return CE_Failure;

    for (int i = 0; i < aosFiles.size(); ++i)
    {
        if (strcmp(aosFiles[i], ".") != 0 && strcmp(aosFiles[i], "..") != 0)
        {
            const std::string osFilename(
                CPLFormFilename(pszFilename, aosFiles[i], nullptr));
            if (VSIUnlink(osFilename.c_str()) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot delete %s",
                         osFilename.c_str());
                return CE_Failure;
            }
        }
    }
    if (VSIRmdir(pszFilename) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot delete %s", pszFilename);
        return CE_Failure;
    }

    return CE_None;
}

/***********************************************************************/
/*                       RegisterOGROpenFileGDB()                      */
/***********************************************************************/

void RegisterOGROpenFileGDB()

{
    if (!GDAL_CHECK_VERSION("OGR OpenFileGDB"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGROpenFileGDBDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGROpenFileGDBDriverOpen;
    poDriver->pfnCreate = OGROpenFileGDBDriverCreate;
    poDriver->pfnDelete = OGROpenFileGDBDriverDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
