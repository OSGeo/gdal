/******************************************************************************
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSIDriver.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

// Must be included before FYBA headers that mess with min() / max()
#include <mutex>

#include "ogr_sosi.h"
#include "ogrsosidrivercore.h"

static int bFYBAInit = FALSE;
static std::mutex oMutex;

/************************************************************************/
/*                          OGRSOSIInit()                               */
/************************************************************************/

static void OGRSOSIInit()
{
    std::lock_guard<std::mutex> oLock(oMutex);
    if (!bFYBAInit)
    {
        LC_Init(); /* Init FYBA */
        SOSIInitTypes();
        bFYBAInit = TRUE;
    }
}

/************************************************************************/
/*                        OGRSOSIDriverUnload()                         */
/************************************************************************/

static void OGRSOSIDriverUnload(CPL_UNUSED GDALDriver *poDriver)
{

    if (bFYBAInit)
    {
        LC_Close(); /* Close FYBA */
        SOSICleanupTypes();
        bFYBAInit = FALSE;
    }
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

static GDALDataset *OGRSOSIDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (OGRSOSIDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    OGRSOSIInit();

    OGRSOSIDataSource *poDS = new OGRSOSIDataSource();
    if (!poDS->Open(poOpenInfo->pszFilename, 0))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

#ifdef WRITE_SUPPORT
/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *
OGRSOSIDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                    CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                    CPL_UNUSED GDALDataType eDT, CPL_UNUSED char **papszOptions)
{
    OGRSOSIInit();
    OGRSOSIDataSource *poDS = new OGRSOSIDataSource();
    if (!poDS->Create(pszName))
    {
        delete poDS;
        return NULL;
    }
    return poDS;
}
#endif

/************************************************************************/
/*                         RegisterOGRSOSI()                            */
/************************************************************************/

void RegisterOGRSOSI()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRSOSIDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRSOSIDriverOpen;
#ifdef WRITE_SUPPORT
    poDriver->pfnCreate = OGRSOSIDriverCreate;
#endif
    poDriver->pfnUnloadDriver = OGRSOSIDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
