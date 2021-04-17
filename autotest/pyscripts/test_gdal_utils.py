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

import os
from pathlib import Path

import pytest

# test that osgeo_utils is available, if not skip all tests
from osgeo_utils.auxiliary.extent_util import Extent

pytest.importorskip('osgeo_utils')

from osgeo_utils.auxiliary import util, raster_creation, base

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

    assert base.num(42.0) == 42.0
    assert base.num('42.0') == 42.0
    assert base.num('42.') == 42.0

    assert base.num_or_none('') is None
    assert base.num_or_none(None) is None
    assert base.num_or_none('1a') is None
    assert base.num_or_none('42') == 42
    assert base.num_or_none('42.0') == 42.0


def test_utils_py_1():
    """ test get_ovr_idx, create_flat_raster """
    filename = 'tmp/raster.tif'
    temp_files.append(filename)
    raster_creation.create_flat_raster(filename, overview_list=[2, 4])
    ds = util.open_ds(filename)
    compression = util.get_image_structure_metadata(filename, 'COMPRESSION')
    assert compression == 'DEFLATE'
    ovr_count = util.get_ovr_count(ds)+1
    assert ovr_count == 3
    pixel_size = util.get_pixel_size(ds)
    assert pixel_size == (10, -10)

    for i in range(-ovr_count, ovr_count):
        assert util.get_ovr_idx(filename, ovr_idx=i) == (i if i >= 0 else ovr_count+i)

    for res, ovr in [(5, 0), (10, 0), (11, 0), (19.99, 0), (20, 1), (20.1, 1), (39, 1), (40, 2), (41, 2), (400, 2)]:
        assert util.get_ovr_idx(filename, ovr_res=res) == ovr
        assert util.get_ovr_idx(filename, float(res)) == ovr  # noqa secret functionality


def test_utils_py_cleanup():
    for filename in temp_files:
        try:
            os.remove(filename)
        except OSError:
            pass
