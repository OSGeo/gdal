/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
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

#include "stdinc.h"

GDALDataset *GDALWMSDatasetOpen(GDALOpenInfo *poOpenInfo) {
    CPLXMLNode *config = NULL;
    CPLErr ret = CE_None;

    if ((poOpenInfo->nHeaderBytes == 0) && EQUALN((const char *) poOpenInfo->pszFilename, "<GDAL_WMS>", 10)) {
        config = CPLParseXMLString(poOpenInfo->pszFilename);
    } else if ((poOpenInfo->nHeaderBytes >= 10) && EQUALN((const char *) poOpenInfo->pabyHeader, "<GDAL_WMS>", 10)) {
        config = CPLParseXMLFile(poOpenInfo->pszFilename);
    } else return NULL;
    if (config == NULL) return NULL;
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLDestroyXMLNode(config);
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The WMS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

    GDALWMSDataset *ds = new GDALWMSDataset();
    ret = ds->Initialize(config);
    if (ret != CE_None) {
        delete ds;
        ds = 0;
    }
    CPLDestroyXMLNode(config);

    return ds;
}

/************************************************************************/
/*                         GDALDeregister_WMS()                         */
/************************************************************************/

static void GDALDeregister_WMS( GDALDriver * )

{
    DestroyWMSMiniDriverManager();
}

/************************************************************************/
/*                          GDALRegister_WMS()                          */
/************************************************************************/

void GDALRegister_WMS() {
    GDALDriver *driver;
    if (GDALGetDriverByName("WMS") == NULL) {
        driver = new GDALDriver();
        driver->SetDescription("WMS");
        driver->SetMetadataItem(GDAL_DMD_LONGNAME, "OGC Web Map Service");
        driver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_wms.html");
        driver->pfnOpen = GDALWMSDatasetOpen;
        driver->pfnUnloadDriver = GDALDeregister_WMS;
        GetGDALDriverManager()->RegisterDriver(driver);

        GDALWMSMiniDriverManager *const mdm = GetGDALWMSMiniDriverManager();
        mdm->Register(new GDALWMSMiniDriverFactory_WMS());
        mdm->Register(new GDALWMSMiniDriverFactory_TileService());
        mdm->Register(new GDALWMSMiniDriverFactory_WorldWind());
        mdm->Register(new GDALWMSMiniDriverFactory_TMS());
    }
}
