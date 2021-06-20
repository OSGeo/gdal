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

import os
import tempfile
from typing import Optional, Union

from osgeo import gdal
from osgeo_utils.auxiliary.base import PathLikeOrStr
from osgeo_utils.auxiliary.util import open_ds, PathOrDS
from osgeo_utils.auxiliary.color_palette import get_color_palette, ColorPaletteOrPathOrStrings, ColorPalette

ColorTableLike = Union[gdal.ColorTable, ColorPaletteOrPathOrStrings]


def get_color_table_from_raster(path_or_ds: PathOrDS) -> Optional[gdal.ColorTable]:
    ds = open_ds(path_or_ds, silent_fail=True)
    if ds is not None:
        ct = ds.GetRasterBand(1).GetRasterColorTable()
        return ct.Clone()
    return None


def color_table_from_color_palette(pal: ColorPalette, color_table: gdal.ColorTable,
                                   fill_missing_colors=True, min_key=0, max_key=255) -> bool:
    """ returns None if pal has no values, otherwise returns a gdal.ColorTable from the given ColorPalette"""
    if not pal.pal or not pal.is_numeric():
        # palette has no values or not numeric
        return False
    if fill_missing_colors:
        keys = sorted(list(pal.pal.keys()))
        if min_key is None:
            min_key = keys[0]
        if max_key is None:
            max_key = keys[-1]
        c = pal.color_to_color_entry(pal.pal[keys[0]])
        for key in range(min_key, max_key + 1):
            if key in keys:
                c = pal.color_to_color_entry(pal.pal[key])
            color_table.SetColorEntry(key, c)
    else:
        for key, col in pal.pal.items():
            color_table.SetColorEntry(key, pal.color_to_color_entry(col))  # set color for each key
    return True


def get_color_table(color_palette_or_path_or_strings_or_ds: Optional[ColorTableLike],
                    **kwargs) -> Optional[gdal.ColorTable]:
    if (color_palette_or_path_or_strings_or_ds is None or
       isinstance(color_palette_or_path_or_strings_or_ds, gdal.ColorTable)):
        return color_palette_or_path_or_strings_or_ds

    if isinstance(color_palette_or_path_or_strings_or_ds, gdal.Dataset):
        return get_color_table_from_raster(color_palette_or_path_or_strings_or_ds)

    try:
        pal = get_color_palette(color_palette_or_path_or_strings_or_ds)
        color_table = gdal.ColorTable()
        res = color_table_from_color_palette(pal, color_table, **kwargs)
        return color_table if res else None
    except:
        # the input might be a filename of a raster file
        return get_color_table_from_raster(color_palette_or_path_or_strings_or_ds)


def is_fixed_color_table(color_table: gdal.ColorTable, c=(0, 0, 0, 0)) -> bool:
    for i in range(color_table.GetCount()):
        color_entry: gdal.ColorEntry = color_table.GetColorEntry(i)
        if color_entry != c:
            return False
    return True


def get_fixed_color_table(c=(0, 0, 0, 0), count=1):
    color_table = gdal.ColorTable()
    for i in range(count):
        color_table.SetColorEntry(i, c)
    return color_table


def are_equal_color_table(color_table1: gdal.ColorTable, color_table2: gdal.ColorTable) -> bool:
    if color_table1.GetCount() != color_table2.GetCount():
        return False
    for i in range(color_table1.GetCount()):
        color_entry1: gdal.ColorEntry = color_table1.GetColorEntry(i)
        color_entry2: gdal.ColorEntry = color_table2.GetColorEntry(i)
        if color_entry1 != color_entry2:
            return False
    return True


def write_color_table_to_file(color_table: gdal.ColorTable, color_filename: Optional[PathLikeOrStr]):
    if color_filename is None:
        color_filename = tempfile.mktemp(suffix='.txt')
    os.makedirs(os.path.dirname(color_filename), exist_ok=True)
    with open(color_filename, mode='w') as fp:
        for i in range(color_table.GetCount()):
            color_entry = color_table.GetColorEntry(i)
            color_entry = ' '.join(str(c) for c in color_entry)
            fp.write('{} {}\n'.format(i, color_entry))
    return color_filename
