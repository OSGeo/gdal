/******************************************************************************
 *
 * Author:   Aaron Boxer, <boxerab at protonmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Grok Image Compression Inc.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "jp2opjlikedataset.h"
#include "jp2opjlikedataset.cpp"

#include "opjdatasetbase.h"
#include "openjpegdrivercore.h"

/************************************************************************/
/*                      GDALRegister_JP2OpenJPEG()                      */
/************************************************************************/

void GDALRegister_JP2OpenJPEG()
{
    if (!GDAL_CHECK_VERSION("JP2OpenJPEG driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OPENJPEGDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen =
        JP2OPJLikeDataset<OPJCodecWrapper, JP2OPJDatasetBase>::Open;
    poDriver->pfnCreateCopy =
        JP2OPJLikeDataset<OPJCodecWrapper, JP2OPJDatasetBase>::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
