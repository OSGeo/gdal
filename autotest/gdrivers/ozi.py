#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OZI Testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Test reading OZF2 file


def test_ozi_online_1():

    if not gdaltest.download_file('http://www.oziexplorer2.com/maps/Europe2001_setup.exe', 'Europe2001_setup.exe'):
        pytest.skip()

    try:
        os.stat('tmp/cache/Europe 2001_OZF.map')
    except OSError:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/Europe2001_setup.exe')
            try:
                os.stat('tmp/cache/Europe 2001_OZF.map')
            except OSError:
                pytest.skip()
        except:
            pytest.skip()

    ds = gdal.Open('tmp/cache/Europe 2001_OZF.map')
    assert ds is not None

    if False:  # pylint: disable=using-constant-test
        gt = ds.GetGeoTransform()
        wkt = ds.GetProjectionRef()

        expected_gt = (-1841870.2731215316, 3310.9550245520159, -13.025246304875619, 8375316.4662204208, -16.912440131236657, -3264.1162527118681)
        for i in range(6):
            assert gt[i] == pytest.approx(expected_gt[i], abs=1e-7), 'bad geotransform'

    else:
        gcps = ds.GetGCPs()

        assert len(gcps) == 4, 'did not get expected gcp count.'

        gcp0 = gcps[0]
        assert gcp0.GCPPixel == 61 and gcp0.GCPLine == 436 and gcp0.GCPX == pytest.approx(-1653990.4525324, abs=0.001) and gcp0.GCPY == pytest.approx(6950885.0402214, abs=0.001), \
            'did not get expected gcp.'

        wkt = ds.GetGCPProjection()

    expected_wkt = 'PROJCS["unnamed",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["latitude_of_origin",4],PARAMETER["central_meridian",10],PARAMETER["standard_parallel_1",40],PARAMETER["standard_parallel_2",56],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    assert wkt == expected_wkt, wkt

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 16025, 'bad checksum'



