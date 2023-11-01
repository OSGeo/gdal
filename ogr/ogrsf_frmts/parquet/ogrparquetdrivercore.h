/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#ifndef OGRPARQUETDRIVERCORE_H
#define OGRPARQUETDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "Parquet";
constexpr const char *LONG_NAME = "(Geo)Parquet";
constexpr const char *EXTENSIONS = "parquet";
constexpr const char *OPENOPTIONLIST =
    "<OpenOptionList>"
    "  <Option name='GEOM_POSSIBLE_NAMES' type='string' description='Comma "
    "separated list of possible names for geometry column(s).' "
    "default='geometry,wkb_geometry,wkt_geometry'/>"
    "  <Option name='CRS' type='string' "
    "description='Set/override CRS, typically defined as AUTH:CODE "
    "(e.g EPSG:4326), of geometry column(s)'/>"
    "</OpenOptionList>";

int CPL_DLL OGRParquetDriverIdentify(GDALOpenInfo *poOpenInfo);

#endif
