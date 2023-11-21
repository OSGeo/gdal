/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#include "zarrdrivercore.h"

/************************************************************************/
/*                    CheckExistenceOfOneZarrFile()                     */
/************************************************************************/

static bool CheckExistenceOfOneZarrFile(const char *pszFilename)
{

    CPLString osMDFilename = CPLFormFilename(pszFilename, ".zarray", nullptr);

    VSIStatBufL sStat;
    if (VSIStatL(osMDFilename, &sStat) == 0)
        return true;

    osMDFilename = CPLFormFilename(pszFilename, ".zgroup", nullptr);
    if (VSIStatL(osMDFilename, &sStat) == 0)
        return true;

    // Zarr V3
    osMDFilename = CPLFormFilename(pszFilename, "zarr.json", nullptr);
    if (VSIStatL(osMDFilename, &sStat) == 0)
        return true;

    return false;
}

/************************************************************************/
/*                     ZARRDriverIdentify()                             */
/************************************************************************/

int ZARRDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "ZARR:"))
    {
        return TRUE;
    }

    if (!poOpenInfo->bIsDirectory)
    {
        return FALSE;
    }

    return CheckExistenceOfOneZarrFile(poOpenInfo->pszFilename);
}

/************************************************************************/
/*                     ZARRDriverSetCommonMetadata()                    */
/************************************************************************/

void ZARRDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Zarr");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 Int64 UInt64 "
                              "Float32 Float64 CFloat32 CFloat64");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='USE_ZMETADATA' type='boolean' description='Whether "
        "to use consolidated metadata from .zmetadata' default='YES'/>"
        "   <Option name='CACHE_TILE_PRESENCE' type='boolean' "
        "description='Whether to establish an initial listing of present "
        "tiles' default='NO'/>"
        "   <Option name='MULTIBAND' type='boolean' default='YES' "
        "description='Whether to expose >= 3D arrays as GDAL multiband "
        "datasets "
        "(when using the classic 2D API)'/>"
        "   <Option name='DIM_X' type='string' description="
        "'Name or index of the X dimension (only used when MULTIBAND=YES)'/>"
        "   <Option name='DIM_Y' type='string' description="
        "'Name or index of the Y dimension (only used when MULTIBAND=YES)'/>"
        "   <Option name='LOAD_EXTRA_DIM_METADATA_DELAY' type='string' "
        "description="
        "'Maximum delay in seconds allowed to set the DIM_{dimname}_VALUE band "
        "metadata items'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST,
        "<MultiDimDatasetCreationOptionList>"
        "   <Option name='FORMAT' type='string-select' default='ZARR_V2'>"
        "     <Value>ZARR_V2</Value>"
        "     <Value>ZARR_V3</Value>"
        "   </Option>"
        "   <Option name='CREATE_ZMETADATA' type='boolean' "
        "description='Whether to create consolidated metadata into .zmetadata "
        "(Zarr V2 only)' default='YES'/>"
        "</MultiDimDatasetCreationOptionList>");

    poDriver->pfnIdentify = ZARRDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_MULTIDIMENSIONAL, "YES");
}

/************************************************************************/
/*                    DeclareDeferredZarrPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredZarrPlugin()
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
    ZARRDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
