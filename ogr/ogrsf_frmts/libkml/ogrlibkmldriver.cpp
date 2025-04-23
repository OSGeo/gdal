/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "libkml_headers.h"

#include "ogr_libkml.h"
#include "ogrlibkmldrivercore.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"

using kmldom::KmlFactory;

static CPLMutex *hMutex = nullptr;
static KmlFactory *m_poKmlFactory = nullptr;

/******************************************************************************
 OGRLIBKMLDriverUnload()
******************************************************************************/

static void OGRLIBKMLDriverUnload(GDALDriver * /* poDriver */)
{
    if (hMutex != nullptr)
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
    m_poKmlFactory = nullptr;
}

/******************************************************************************
 Open()
******************************************************************************/

static GDALDataset *OGRLIBKMLDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (OGRLIBKMLDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    {
        CPLMutexHolderD(&hMutex);
        if (m_poKmlFactory == nullptr)
            m_poKmlFactory = KmlFactory::GetFactory();
    }

    OGRLIBKMLDataSource *poDS = new OGRLIBKMLDataSource(m_poKmlFactory);

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;

        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRLIBKMLDriverCreate(const char *pszName, int /* nBands */,
                                          int /* nXSize */, int /* nYSize */,
                                          GDALDataType /* eDT */,
                                          char **papszOptions)
{
    CPLAssert(nullptr != pszName);
    CPLDebug("LIBKML", "Attempt to create: %s", pszName);

    {
        CPLMutexHolderD(&hMutex);
        if (m_poKmlFactory == nullptr)
            m_poKmlFactory = KmlFactory::GetFactory();
    }

    OGRLIBKMLDataSource *poDS = new OGRLIBKMLDataSource(m_poKmlFactory);

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;

        poDS = nullptr;
    }

    return poDS;
}

/******************************************************************************
 DeleteDataSource()

 Note: This method recursively deletes an entire dir if the datasource is a dir
       and all the files are kml or kmz.

******************************************************************************/

static CPLErr OGRLIBKMLDriverDelete(const char *pszName)
{
    /***** dir *****/
    VSIStatBufL sStatBuf;
    if (!VSIStatL(pszName, &sStatBuf) && VSI_ISDIR(sStatBuf.st_mode))
    {
        char **papszDirList = VSIReadDir(pszName);
        for (int iFile = 0;
             papszDirList != nullptr && papszDirList[iFile] != nullptr; iFile++)
        {
            if (CE_Failure == OGRLIBKMLDriverDelete(papszDirList[iFile]))
            {
                CSLDestroy(papszDirList);
                return CE_Failure;
            }
        }
        CSLDestroy(papszDirList);

        if (VSIRmdir(pszName) < 0)
        {
            return CE_Failure;
        }
    }

    /***** kml *****/
    else if (EQUAL(CPLGetExtensionSafe(pszName).c_str(), "kml"))
    {
        if (VSIUnlink(pszName) < 0)
            return CE_Failure;
    }

    /***** kmz *****/
    else if (EQUAL(CPLGetExtensionSafe(pszName).c_str(), "kmz"))
    {
        if (VSIUnlink(pszName) < 0)
            return CE_Failure;
    }

    /***** do not delete other types of files *****/
    else
    {
        return CE_Failure;
    }

    // TODO(schwehr): Isn't this redundant to the else case?
    return CE_None;
}

/******************************************************************************
 RegisterOGRLIBKML()
******************************************************************************/

void RegisterOGRLIBKML()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRLIBKMLDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRLIBKMLDriverOpen;
    poDriver->pfnCreate = OGRLIBKMLDriverCreate;
    poDriver->pfnDelete = OGRLIBKMLDriverDelete;
    poDriver->pfnUnloadDriver = OGRLIBKMLDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
