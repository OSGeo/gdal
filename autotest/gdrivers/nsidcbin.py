#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NSIDCbin format driver.
# Author:   Michael D. Sumner, <mdsumner at gmail dot com>
#
###############################################################################
# Copyright (c) 2022, Michael Sumner, <mdsumner at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("NSIDCbin")

###############################################################################
# Perform simple read test.


## test file
## mkdir autotest/gdrivers/data/nsidcbin
## cd autotest/gdrivers/data/nsidcbin
## wget ftp://sidads.colorado.edu/pub/DATASETS/nsidc0081_nrt_nasateam_seaice/south/nt_20220409_f18_nrt_s.bin
## ## /vsicurl/ftp://sidads.colorado.edu/pub/DATASETS/nsidc0081_nrt_nasateam_seaice/south/nt_20220409_f18_nrt_s.bin
def test_nsidcbin_1(filename="data/nsidcbin/nt_20220409_f18_nrt_s.bin"):
    ds = gdal.Open(filename)
    band = ds.GetRasterBand(1)
    assert band.XSize == 316
    assert band.DataType == gdal.GDT_UInt8
    assert int.from_bytes(band.ReadRaster(60, 44, 1, 1), "little") == 27
