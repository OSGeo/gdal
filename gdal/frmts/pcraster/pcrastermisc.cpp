/******************************************************************************
 * $Id$
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

// Library headers.
#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

#ifndef INCLUDED_STRING
#include <string>
#define INCLUDED_STRING
#endif

#ifndef INCLUDED_GDAL_PAM
#include "gdal_pam.h"
#define INCLUDED_GDAL_PAM
#endif

// PCRaster library headers.

// Module headers.
#ifndef INCLUDED_PCRASTERDATASET
#include "pcrasterdataset.h"
#define INCLUDED_PCRASTERDATASET
#endif



CPL_C_START
void GDALRegister_PCRaster(void);
CPL_C_END



void GDALRegister_PCRaster()
{
    if (! GDAL_CHECK_VERSION("PCRaster driver"))
        return;

    if(!GDALGetDriverByName("PCRaster")) {

        GDALDriver* poDriver = new GDALDriver();

        poDriver->SetDescription("PCRaster");
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );

        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "PCRaster Raster File");
        poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int32 Float32");
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_various.html#PCRaster");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "map" );

        poDriver->pfnOpen = PCRasterDataset::open;
        poDriver->pfnCreate = PCRasterDataset::create;
        poDriver->pfnCreateCopy = PCRasterDataset::createCopy;

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}
