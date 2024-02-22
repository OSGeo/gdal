/******************************************************************************
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster driver support functions.
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
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

#include "pcrasterdrivercore.h"

/* value for first 27 bytes of MAIN_HEADER.signature */
#ifndef CSF_SIG
#define CSF_SIG "RUU CROSS SYSTEM MAP FORMAT"
#define CSF_SIZE_SIG (sizeof(CSF_SIG) - 1)
#endif

/************************************************************************/
/*                     PCRasterDriverIdentify()                         */
/************************************************************************/

int PCRasterDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return (poOpenInfo->fpL &&
            poOpenInfo->nHeaderBytes >= static_cast<int>(CSF_SIZE_SIG) &&
            strncmp(reinterpret_cast<char *>(poOpenInfo->pabyHeader), CSF_SIG,
                    CSF_SIZE_SIG) == 0);
}

#undef CSF_SIG
#undef CSF_SIZE_SIG

/************************************************************************/
/*                    PCRasterDriverSetCommonMetadata()                 */
/************************************************************************/

void PCRasterDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "PCRaster Raster File");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int32 Float32");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/pcraster.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "map");

    poDriver->pfnIdentify = PCRasterDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                  DeclareDeferredPCRasterPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredPCRasterPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    PCRasterDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
