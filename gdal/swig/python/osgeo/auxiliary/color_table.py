#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  module for loading gdal.ColorTable from a file using ColorPalette
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************

from osgeo import gdal
from osgeo.auxiliary.color_palette import get_color_palette


def get_color_table(color_palette_or_path_or_strings_or_ds, min_key=0, max_key=255, fill_missing_colors=True):
    # def get_color_table(color_palette_or_path_or_strings: ColorPaletteOrPathOrStrings) -> gdal.ColorTable:
    if color_palette_or_path_or_strings_or_ds is None:
        return None

    ds = gdal.Open(color_palette_or_path_or_strings_or_ds)
    if ds is not None:
        ct = ds.GetRasterBand(1).GetRasterColorTable()
        return ct.Clone()

    pal = get_color_palette(color_palette_or_path_or_strings_or_ds)
    if pal is None:
        return None
    color_table = gdal.ColorTable()
    if fill_missing_colors:
        keys = sorted(list(pal.pal.keys()))
        if min_key is None:
            min_key = keys[0]
        if max_key is None:
            max_key = keys[-1]
        c = pal.color_to_cc(pal.pal[keys[0]])
        for key in range(min_key, max_key+1):
            if key in keys:
                c = pal.color_to_cc(pal.pal[key])
            color_table.SetColorEntry(key, c)
    else:
        for key, col in pal.pal.items():
            color_table.SetColorEntry(key, pal.color_to_cc(col))  # set color for each key
    return color_table
