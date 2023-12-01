/******************************************************************************
 *
 * Project:  FITS Driver
 * Purpose:  Implement FITS raster read/write support
 * Author:   Simon Perkins, s.perkins@lanl.gov
 *
 ******************************************************************************
 * Copyright (c) 2001, Simon Perkins
 * Copyright (c) 2008-2020, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2018, Chiara Marmo <chiara dot marmo at u-psud dot fr>
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

#include "fitsdrivercore.h"

/************************************************************************/
/*                     FITSDriverIdentify()                             */
/************************************************************************/

int FITSDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "FITS:"))
        return true;

    const char *fitsID = "SIMPLE  =                    T";  // Spaces important!
    const size_t fitsIDLen = strlen(fitsID);  // Should be 30 chars long

    if (static_cast<size_t>(poOpenInfo->nHeaderBytes) < fitsIDLen)
        return false;
    if (memcmp(poOpenInfo->pabyHeader, fitsID, fitsIDLen) != 0)
        return false;
    return true;
}

/************************************************************************/
/*                      FITSDriverSetCommonMetadata()                   */
/************************************************************************/

void FITSDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Flexible Image Transport System");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/fits.html");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32 Float64");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "fits");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String IntegerList "
                              "Integer64List RealList");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Int16 Float32");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='REPEAT_*' type='int' description='Repeat value for "
        "fields of type List'/>"
        "  <Option name='COMPUTE_REPEAT' type='string-select' "
        "description='Determine when the repeat value for fields is computed'>"
        "    <Value>AT_FIELD_CREATION</Value>"
        "    <Value>AT_FIRST_FEATURE_CREATION</Value>"
        "  </Option>"
        "</LayerCreationOptionList>");

    poDriver->pfnIdentify = FITSDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                     DeclareDeferredFITSPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredFITSPlugin()
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
    FITSDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
