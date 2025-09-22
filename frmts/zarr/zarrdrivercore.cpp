/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarrdrivercore.h"

#include "vsikerchunk.h"
#include "vsikerchunk_inline.hpp"

/************************************************************************/
/*                    CheckExistenceOfOneZarrFile()                     */
/************************************************************************/

static bool CheckExistenceOfOneZarrFile(const char *pszFilename)
{

    CPLString osMDFilename =
        CPLFormFilenameSafe(pszFilename, ".zarray", nullptr);

    VSIStatBufL sStat;
    if (VSIStatL(osMDFilename, &sStat) == 0)
        return true;

    osMDFilename = CPLFormFilenameSafe(pszFilename, ".zgroup", nullptr);
    if (VSIStatL(osMDFilename, &sStat) == 0)
        return true;

    // Zarr V3
    osMDFilename = CPLFormFilenameSafe(pszFilename, "zarr.json", nullptr);
    if (VSIStatL(osMDFilename, &sStat) == 0)
        return true;

    return false;
}

/************************************************************************/
/*                   ZARRIsLikelyKerchunkJSONRef()                      */
/************************************************************************/

bool ZARRIsLikelyKerchunkJSONRef(const GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->nHeaderBytes > 0 && poOpenInfo->eAccess == GA_ReadOnly &&
        (poOpenInfo->IsExtensionEqualToCI("json") ||
         // e.g. like in https://noaa-nodd-kerchunk-pds.s3.amazonaws.com/nos/cbofs/cbofs.fields.best.nc.zarr
         poOpenInfo->IsExtensionEqualToCI("zarr")))
    {
        const char *pszHeader =
            reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
        if (ZARRIsLikelyStreamableKerchunkJSONRefContent(
                std::string_view(pszHeader, poOpenInfo->nHeaderBytes)))
        {
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                     ZARRDriverIdentify()                             */
/************************************************************************/

int ZARRDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "ZARR:") ||
        STARTS_WITH(poOpenInfo->pszFilename, "ZARR_DUMMY:"))
    {
        return TRUE;
    }

    if (ZARRIsLikelyKerchunkJSONRef(poOpenInfo))
    {
        return TRUE;
    }
    if (STARTS_WITH(poOpenInfo->pszFilename, JSON_REF_FS_PREFIX))
    {
        return -1;
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
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "zarr");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONDATATYPES,
        "Int8 Byte Int16 UInt16 Int32 UInt32 Int64 UInt64 "
        "Float16 Float32 Float64 CFLoat16 CFloat32 CFloat64");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='LIST_ALL_ARRAYS' type='boolean' "
        "description='Whether to list all arrays, and not only those whose "
        "dimension count is 2 or more' default='NO'/>"
        "   <Option name='USE_ZMETADATA' type='boolean' description='Whether "
        "to use consolidated metadata from .zmetadata' default='YES'/>"
        "   <Option name='CACHE_TILE_PRESENCE' type='boolean' "
        "description='Whether to establish an initial listing of present "
        "tiles' default='NO'/>"
        "   <Option name='CACHE_KERCHUNK_JSON' type='boolean' "
        "description='Whether to transform Kerchunk JSON reference files into "
        "Kerchunk Parquet reference files in a local cache' default='NO'/>"
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
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_MULTIDIMENSIONAL, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS,
                              "GeoTransform SRS NoData "
                              "RasterValues "
                              "DatasetMetadata BandMetadata");
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
