#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JP2 metadata support.
# Author:   Even Rouault < even dot rouault @ spatialys.com >
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

import pytest


###############################################################################
# Test bugfix for #5249 (Irrelevant ERDAS GeoTIFF JP2Box read)

def test_jp2metadata_1():

    ds = gdal.Open('data/jpeg2000/erdas_foo.jp2')
    if ds is None:
        pytest.skip()

    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    assert wkt.startswith('PROJCS["ETRS89')
    expected_gt = (356000.0, 0.5, 0.0, 7596000.0, 0.0, -0.5)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-5)
    
###############################################################################
# Test Pleiades imagery metadata


def test_jp2metadata_2():

    # Pleiades product description http://www.cscrs.itu.edu.tr/assets/downloads/PleiadesUserGuide.pdf
    try:
        os.remove('data/jpeg2000/IMG_md_ple_R1C1.jp2.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/jpeg2000/IMG_md_ple_R1C1.jp2', gdal.GA_ReadOnly)
    if ds is None:
        pytest.skip()

    filelist = ds.GetFileList()

    assert len(filelist) == 3, filelist

    mddlist = ds.GetMetadataDomainList()
    assert 'IMD' in mddlist and 'RPC' in mddlist and 'IMAGERY' in mddlist, \
        'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    ds = None

    assert not os.path.exists('data/jpeg2000/IMG_md_ple_R1C1.jp2.aux.xml')

    
###############################################################################
# Test reading GMLJP2 file with srsName only on the Envelope, and lots of other
# metadata junk.  This file is also handled currently with axis reordering
# disabled.


def test_jp2metadata_3():

    gdal.SetConfigOption('GDAL_IGNORE_AXIS_ORIENTATION', 'YES')

    exp_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'

    ds = gdal.Open('data/jpeg2000/ll.jp2')
    if ds is None:
        gdal.SetConfigOption('GDAL_IGNORE_AXIS_ORIENTATION', 'NO')
        pytest.skip()
    wkt = ds.GetProjection()

    if wkt != exp_wkt:
        print('got: ', wkt)
        print('exp: ', exp_wkt)
        pytest.fail('did not get expected WKT, should be WGS84')

    gt = ds.GetGeoTransform()
    if gt[0] != pytest.approx(8, abs=0.0000001) or gt[3] != pytest.approx(50, abs=0.000001) \
       or gt[1] != pytest.approx(0.000761397164, abs=0.000000000005) \
       or gt[2] != pytest.approx(0.0, abs=0.000000000005) \
       or gt[4] != pytest.approx(0.0, abs=0.000000000005) \
       or gt[5] != pytest.approx(-0.000761397164, abs=0.000000000005):
        print('got: ', gt)
        pytest.fail('did not get expected geotransform')

    ds = None

    gdal.SetConfigOption('GDAL_IGNORE_AXIS_ORIENTATION', 'NO')

###############################################################################
# Test reading a file with axis orientation set properly for an alternate
# axis order coordinate system (urn:...:EPSG::4326).


def test_jp2metadata_4():

    exp_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'

    ds = gdal.Open('data/jpeg2000/gmljp2_dtedsm_epsg_4326_axes.jp2')
    if ds is None:
        pytest.skip()
    wkt = ds.GetProjection()

    if wkt != exp_wkt:
        print('got: ', wkt)
        print('exp: ', exp_wkt)
        pytest.fail('did not get expected WKT, should be WGS84')

    gt = ds.GetGeoTransform()
    gte = (42.999583333333369, 0.008271349862259, 0,
           34.000416666666631, 0, -0.008271349862259)

    if gt[0] != pytest.approx(gte[0], abs=0.0000001) or gt[3] != pytest.approx(gte[3], abs=0.000001) \
       or gt[1] != pytest.approx(gte[1], abs=0.000000000005) \
       or gt[2] != pytest.approx(gte[2], abs=0.000000000005) \
       or gt[4] != pytest.approx(gte[4], abs=0.000000000005) \
       or gt[5] != pytest.approx(gte[5], abs=0.000000000005):
        print('got: ', gt)
        pytest.fail('did not get expected geotransform')

    ds = None

###############################################################################
# Test reading a file with EPSG axis orientation being northing, easting,
# but with explicit axisName being easting, northing (#5960)


def test_jp2metadata_5():

    ds = gdal.Open('data/jpeg2000/gmljp2_epsg3035_easting_northing.jp2')
    if ds is None:
        pytest.skip()

    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == '3035'

    gt = ds.GetGeoTransform()
    gte = (4895766.000000001, 2.0, 0.0, 2296946.0, 0.0, -2.0)

    if gt[0] != pytest.approx(gte[0], abs=0.0000001) or gt[3] != pytest.approx(gte[3], abs=0.000001) \
       or gt[1] != pytest.approx(gte[1], abs=0.000000000005) \
       or gt[2] != pytest.approx(gte[2], abs=0.000000000005) \
       or gt[4] != pytest.approx(gte[4], abs=0.000000000005) \
       or gt[5] != pytest.approx(gte[5], abs=0.000000000005):
        print('got: ', gt)
        pytest.fail('did not get expected geotransform')

    ds = None




