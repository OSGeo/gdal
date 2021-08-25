#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  osgeo_utils.auxiliary (gdal-utils) testing
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2021, Idan Miara <idan@miara.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################
import array
import os
from numbers import Real
from pathlib import Path
from typing import Optional

from osgeo import gdal

import pytest

# test that osgeo_utils is available, if not skip all tests
import test_py_scripts

pytest.importorskip('osgeo_utils')

from osgeo_utils.auxiliary import util, raster_creation, base, array_util, color_table
from osgeo_utils.auxiliary.color_palette import ColorPalette
from osgeo_utils.auxiliary.color_table import get_color_table
from osgeo_utils.auxiliary.extent_util import Extent
import gdaltest

temp_files = []


def test_utils_py_0():
    for b in (False, 'False', 'OfF', 'no'):
        assert not base.is_true(b)
    for b in (True, 'TruE', 'ON', 'yes'):
        assert base.is_true(b)

    assert base.enum_to_str(Extent.UNION) == 'UNION'
    assert base.enum_to_str('UNION') == 'UNION'

    filename = Path('abc') / Path('def') / Path('a.txt')
    assert base.is_path_like(Path(filename))
    assert base.is_path_like(str(filename))
    assert not base.is_path_like(None)
    assert not base.is_path_like([filename])

    assert base.get_suffix(filename) == '.txt'
    assert base.get_extension(filename) == 'txt'

    for idx, b in enumerate((0x23, 0xc1, 0xab, 0x00)):
        byte = base.get_byte(0xab_c1_23, idx)
        assert byte == b

    assert base.path_join(filename, 'a', 'b') == str(filename / 'a' / 'b')

    assert base.num(42) == 42
    assert base.num('42') == 42
    assert isinstance(base.num('42'), int)

    assert base.num(42.0) == 42.0
    assert base.num('42.0') == 42.0
    assert isinstance(base.num('42.0'), float)
    assert base.num('42.') == 42.0
    assert isinstance(base.num('42.'), float)
    assert base.num(42.5) == 42.5
    assert base.num('42.5') == 42.5

    assert base.num_or_none('') is None
    assert base.num_or_none(None) is None
    assert base.num_or_none('1a') is None
    assert base.num_or_none('42') == 42
    assert base.num_or_none('42.0') == 42.0


def test_utils_py_1():
    """ test get_ovr_idx, create_flat_raster """
    filename = 'tmp/raster.tif'
    temp_files.append(filename)
    overview_list = [2, 4]
    raster_creation.create_flat_raster(filename, overview_list=overview_list)
    ds = util.open_ds(filename)
    compression = util.get_image_structure_metadata(filename, 'COMPRESSION')
    assert compression == 'DEFLATE'
    ovr_count = util.get_ovr_count(ds) + 1
    assert ovr_count == 3
    pixel_size = util.get_pixel_size(ds)
    assert pixel_size == (10, -10)

    for i in range(-ovr_count, ovr_count):
        assert util.get_ovr_idx(filename, ovr_idx=i) == (i if i >= 0 else ovr_count + i)

    ovr_factor = [1] + overview_list
    ras_size = ds.RasterXSize, ds.RasterYSize
    for res, ovr_idx in [(5, 0), (10, 0), (11, 0), (19.99, 0), (20, 1), (20.1, 1), (39, 1), (40, 2), (41, 2), (400, 2)]:
        assert util.get_ovr_idx(ds, ovr_res=res) == ovr_idx
        assert util.get_ovr_idx(ds, float(res)) == ovr_idx  # noqa secret functionality

        bands = util.get_bands(ds, ovr_idx=ovr_idx)
        assert len(bands) == 1
        f = ovr_factor[ovr_idx]
        assert (bands[0].XSize, bands[0].XSize) == (ras_size[0] // f, ras_size[1] // f)

    # test open_ds with multiple different inputs
    filename2 = 'tmp/raster2.tif'
    temp_files.append(filename2)
    raster_creation.create_flat_raster(filename2)
    ds_list = util.open_ds([ds, filename2])
    assert tuple(util.get_ovr_count(ds) for ds in ds_list) == (2, 0)
    ds_list = None


@pytest.mark.parametrize("data,name,min,max,approx_ok",
                         [('gcore', 'byte.tif', 74, 255, False)])
def test_min_max(data, name, min, max, approx_ok):
    ds = util.open_ds(test_py_scripts.get_data_path(data) + name)
    min_max = util.get_raster_min_max(ds, approx_ok=approx_ok)
    assert min_max == (min, max)


def test_utils_arrays():
    scalars = [7, 5.2]

    for scalar in scalars:
        assert isinstance(scalar, array_util.ScalarLike.__args__)
        assert isinstance(scalar, array_util.ArrayOrScalarLike.__args__)

    for vec in (scalars, tuple(scalars), array.array('d', scalars), array.array('i', [2, 3])):
        assert isinstance(vec, array_util.ArrayLike.__args__)
        assert isinstance(vec, array_util.ArrayOrScalarLike.__args__)

    for not_vec in (None, {1: 2}):
        assert not isinstance(not_vec, array_util.ArrayLike.__args__)
        assert not isinstance(not_vec, array_util.ArrayOrScalarLike.__args__)


def test_utils_np_arrays():
    np = pytest.importorskip('numpy')
    vec_2d = [[1, 2, 3], [4, 5, 6]]

    for dtype in (np.int8, np.int32, np.float64):
        for vec in (vec_2d[0], vec_2d):
            arr = np.array(vec, dtype=dtype)
            assert isinstance(arr, array_util.ArrayLike.__args__)


@pytest.mark.parametrize("name,count,pal",
                         [['color_paletted_red_green_0-255.qml', 256, {0: 0x00ffffff, 1: 0xFF808080}],
                          ['color_pseudocolor_spectral_0-100.qml', 5, {0: 0xFFD7191C, 25: 0xFFFFFFBF}]])
def test_utils_color_files(name: str, count: int, pal: dict):
    """ test color palettes: read QML and TXT files """
    root = Path(test_py_scripts.get_data_path('utilities'))
    path = root / name
    path2 = path.with_suffix('.txt')
    cp1 = ColorPalette()
    cp2 = ColorPalette()
    cp1.read_file(path)
    # cp1.write_file(path2)
    cp2.read_file(path2)
    assert cp1 == cp2
    assert len(cp1.pal) == count
    for k, v in pal.items():
        # compare the first values against the hard-coded test sample
        assert cp1.pal[k] == v


@pytest.mark.parametrize("name,ndv",
                         [['color_paletted_red_green_0-255.txt', None],
                          ['color_paletted_red_green_0-1-nv.txt', 0]])
def test_utils_color_files_nv(name: str, ndv: Optional[Real]):
    """ test color palettes with and without nv """
    root = Path(test_py_scripts.get_data_path('utilities'))

    path = root / name
    cp1 = ColorPalette()
    cp1.read_file(path)
    assert cp1.ndv == ndv

    tmp_filename = Path('tmp') / name
    temp_files.append(tmp_filename)
    cp1.write_file(tmp_filename)
    cp2 = ColorPalette()
    cp2.read_file(tmp_filename)
    assert cp1 == cp2


def test_utils_color_table_and_palette():
    pal = ColorPalette()
    color_entries = {1: (255, 0, 0, 255), 2: (0, 255, 0, 255), 4: (1, 2, 3, 4)}
    for k, v in color_entries.items():
        pal.pal[k] = ColorPalette.color_entry_to_color(*v)

    assert pal.pal[4] == 0x04010203, 'color entry to int'
    ct4 = gdal.ColorTable()
    ct256 = gdal.ColorTable()

    color_table.color_table_from_color_palette(pal, ct4, fill_missing_colors=False)
    assert ct4.GetCount() == 5, 'color table without filling'
    color_table.color_table_from_color_palette(pal, ct256, fill_missing_colors=True)
    assert ct256.GetCount() == 256, 'color table with filling'

    assert (0, 0, 0, 0) == ct4.GetColorEntry(0), 'empty value'
    assert (0, 0, 0, 0) == ct4.GetColorEntry(3), 'empty value'

    assert color_entries[1] == ct256.GetColorEntry(0), 'filled value'
    assert color_entries[2] == ct256.GetColorEntry(3), 'filled value'

    for k, v in color_entries.items():
        assert pal.pal[k] == ColorPalette.color_entry_to_color(*v), 'color in palette'
        assert v == ct4.GetColorEntry(k) == ct256.GetColorEntry(k), 'color in table'

    max_k = max(color_entries.keys())
    for i in range(max_k, 256):
        assert color_entries[max_k] == ct256.GetColorEntry(i), 'fill remaining entries'


def test_read_write_color_table_from_raster():
    """ test color palettes with and without nv """
    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    ds = gdaltest.tiff_drv.Create('tmp/ct8.tif', 1, 1, 1, gdal.GDT_Byte)
    ct = get_color_table(ds)
    assert ct is None

    name = 'color_paletted_red_green_0-255.txt'
    root = Path(test_py_scripts.get_data_path('utilities'))
    path = root / name
    cp1 = ColorPalette()
    cp1.read_file(path)
    ct = get_color_table(cp1)
    assert ct is not None
    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ct2 = get_color_table(ds)
    assert ct2 is not None
    assert ct.GetCount() == ct2.GetCount()
    for k,v in cp1.pal.items():
        assert ColorPalette.color_to_color_entry(v, with_alpha=True) == \
               ct.GetColorEntry(k) == ct2.GetColorEntry(k)
    # assert ct.GetColorEntry(0) == ct2.GetColorEntry(0)

    ct = None
    ct2 = None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/ct8.tif')


def test_utils_py_cleanup():
    for filename in temp_files:
        try:
            os.remove(filename)
        except OSError:
            pass
