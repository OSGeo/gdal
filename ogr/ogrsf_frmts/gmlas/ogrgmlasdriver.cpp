/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "ogr_gmlas.h"
#include "ogrgmlasdrivercore.h"

// g++ -I/usr/include/json -DxDEBUG_VERBOSE -DDEBUG   -g -DDEBUG -ftrapv  -Wall
// -Wextra -Winit-self -Wunused-parameter -Wformat -Werror=format-security
// -Wno-format-nonliteral -Wlogical-op -Wshadow -Werror=vla
// -Wmissing-declarations -Wnon-virtual-dtor -Woverloaded-virtual
// -fno-operator-names ogr/ogrsf_frmts/gmlas/*.cpp -fPIC -shared -o ogr_GMLAS.so
// -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem  -L. -lgdal
// -I/home/even/spatialys/eea/inspire_gml/install-xerces-c-3.1.3/include

/************************************************************************/
/*                           OGRGMLASDriverOpen()                       */
/************************************************************************/

static GDALDataset *OGRGMLASDriverOpen(GDALOpenInfo *poOpenInfo)

{
    OGRGMLASDataSource *poDS;

    if (poOpenInfo->eAccess == GA_Update)
        return nullptr;

    if (OGRGMLASDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    poDS = new OGRGMLASDataSource();

    if (!poDS->Open(poOpenInfo))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          RegisterOGRGMLAS()                          */
/************************************************************************/

void RegisterOGRGMLAS()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRGMLASDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRGMLASDriverOpen;
    poDriver->pfnCreateCopy = OGRGMLASDriverCreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
