/******************************************************************************
 *
 * Author:   Aaron Boxer, <boxerab at protonmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Grok Image Compression Inc.
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
