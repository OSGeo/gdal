/****************************************************************************
 *
 * Author:   Aaron Boxer, <aaron.boxer at grokcompression dot com>
 *
 ****************************************************************************
 * Copyright (c) 2026, Grok Image Compression Inc.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/**
 * @file grokdataset.cpp
 * @brief JP2Grok driver registration.
 *
 * This file instantiates the shared JP2OPJLikeDataset template with
 * Grok-specific types (GRKCodecWrapper, JP2GRKDatasetBase) and registers
 * the "JP2Grok" GDAL driver.
 *
 * The shared template in jp2opjlikedataset.cpp provides Open, CreateCopy
 * and Identify.  Grok-specific behaviour is injected via:
 *   - GRKCodecWrapper   (codec operations: compress, decompress, stream I/O)
 *   - JP2GRKDatasetBase (DirectRasterIO, overview handling, async decompress)
 */
#include <string>

#include "jp2opjlikedataset.cpp"
#include "grkdatasetbase.h"

JP2GRKDatasetBase::~JP2GRKDatasetBase()
{
    delete m_codec;
}

/************************************************************************/
/*                        GDALRegister_JP2Grok()                        */
/************************************************************************/

void GDALRegister_JP2Grok()
{
    auto driverName = std::string("JP2Grok");
    auto libraryName = std::string("Grok");

    if (!GDAL_CHECK_VERSION((driverName + " driver").c_str()))
        return;

    if (GDALGetDriverByName(driverName.c_str()) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription(driverName.c_str());
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_LONGNAME,
        ("JPEG-2000 driver based on " + libraryName + " library").c_str());

    poDriver->SetMetadataItem(
        GDAL_DMD_HELPTOPIC,
        ("drivers/raster/jp2" + CPLString(libraryName).tolower() + ".html")
            .c_str());
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "jp2 j2k");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    JP2GRKDatasetBase::setMetaData(poDriver);

    poDriver->pfnIdentify =
        JP2OPJLikeDataset<GRKCodecWrapper, JP2GRKDatasetBase>::Identify;
    poDriver->pfnOpen =
        JP2OPJLikeDataset<GRKCodecWrapper, JP2GRKDatasetBase>::Open;
    poDriver->pfnCreateCopy =
        JP2OPJLikeDataset<GRKCodecWrapper, JP2GRKDatasetBase>::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
