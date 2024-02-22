/******************************************************************************
 *
 * Project:  OGDI Bridge
 * Purpose:  Implements OGROGDIDriver class.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *           (Based on some code contributed by Frank Warmerdam :)
 *
 ******************************************************************************
 * Copyright (c) 2000, Daniel Morissette
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

#include "ogrogdi.h"
#include "ogrogdidrivercore.h"

#include "cpl_conv.h"

/************************************************************************/
/*                         MyOGDIReportErrorFunction()                  */
/************************************************************************/

#if OGDI_RELEASEDATE >= 20160705
static int MyOGDIReportErrorFunction(int errorcode, const char *error_message)
{
    CPLError(CE_Failure, CPLE_AppDefined, "OGDI error %d: %s", errorcode,
             error_message);
    return FALSE;  // go on
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROGDIDriverOpen(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
    if (!STARTS_WITH_CI(pszFilename, "gltp:"))
        return nullptr;

#if OGDI_RELEASEDATE >= 20160705
    // Available only in post OGDI 3.2.0beta2
    // and only called if env variable OGDI_STOP_ON_ERROR is set to NO
    ecs_SetReportErrorFunction(MyOGDIReportErrorFunction);
#endif

    OGROGDIDataSource *poDS = new OGROGDIDataSource();

    if (!poDS->Open(pszFilename))
    {
        delete poDS;
        poDS = nullptr;
    }

    const bool bUpdate = (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0;
    if (poDS != nullptr && bUpdate)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "OGDI Driver doesn't support update.");
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                          RegisterOGROGDI()                           */
/************************************************************************/

void RegisterOGROGDI()

{
    if (!GDAL_CHECK_VERSION("OGR/OGDI driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGROGDIDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGROGDIDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
