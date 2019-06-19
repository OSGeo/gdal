#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for SDTS driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
from osgeo import osr


import gdaltest

###############################################################################
# Test a truncated version of an SDTS DEM downloaded at
# http://thor-f5.er.usgs.gov/sdts/datasets/raster/dem/dem_oct_2001/1107834.dem.sdts.tar.gz


def test_sdts_1():

    tst = gdaltest.GDALTest('SDTS', 'STDS_1107834_truncated/1107CATD.DDF', 1, 61672)
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    srs.SetUTM(16)
    tst.testOpen(check_prj=srs.ExportToWkt(),
                    check_gt=(666015, 30, 0, 5040735, 0, -30),
                    check_filelist=False)

    ds = gdal.Open('data/STDS_1107834_truncated/1107CATD.DDF')
    md = ds.GetMetadata()

    assert md['TITLE'] == 'ALANSON, MI-24000'



