#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for BAG driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
#                     Frank Warmerdam <warmerdam@pobox.com>
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


import pytest

from osgeo import gdal

import gdaltest


pytestmark = pytest.mark.require_driver('BAG')


@pytest.fixture(autouse=True, scope='module')
def check_no_file_leaks():
    num_files = len(gdaltest.get_opened_files())

    yield

    diff = len(gdaltest.get_opened_files()) - num_files
    assert diff == 0, 'Leak of file handles: %d leaked' % diff


###############################################################################
# Confirm various info on true_n_nominal 1.1 sample file.


def test_bag_2():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.Open('data/true_n_nominal.bag')

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 1072, ('Wrong checksum on band 1, got %d.' % cs)

    cs = ds.GetRasterBand(2).Checksum()
    assert cs == 150, ('Wrong checksum on band 2, got %d.' % cs)

    cs = ds.GetRasterBand(3).Checksum()
    assert cs == 1315, ('Wrong checksum on band 3, got %d.' % cs)

    b1 = ds.GetRasterBand(1)
    assert abs(b1.GetMinimum() - 10) <= 0.01, 'band 1 minimum wrong.'

    assert abs(b1.GetMaximum() - 19.8) <= 0.01, 'band 1 maximum wrong.'

    assert abs(b1.GetNoDataValue() - 1000000.0) <= 0.1, 'band 1 nodata wrong.'

    b2 = ds.GetRasterBand(2)
    assert abs(b2.GetNoDataValue() - 1000000.0) <= 0.1, 'band 2 nodata wrong.'

    b3 = ds.GetRasterBand(3)
    assert abs(b3.GetNoDataValue() - 0.0) <= 0.1, 'band 3 nodata wrong.'

    # It would be nice to test srs and geotransform but they are
    # pretty much worthless on this dataset.

    # Test the xml:BAG metadata domain
    xmlBag = ds.GetMetadata('xml:BAG')[0]
    assert xmlBag.startswith('<?xml'), 'did not get xml:BAG metadata'

    ds = None

    assert not gdaltest.is_file_open('data/true_n_nominal.bag'), 'file still opened.'

###############################################################################
# Test a southern hemisphere falseNorthing sample file.


def test_bag_3():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.Open('data/southern_hemi_false_northing.bag')

    nr = ds.RasterCount
    assert nr == 2, ('Expected 2 bands, got %d.' % nr)

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 21402, ('Wrong checksum on band 1, got %d.' % cs)

    cs = ds.GetRasterBand(2).Checksum()
    assert cs == 33216, ('Wrong checksum on band 2, got %d.' % cs)

    pj = ds.GetProjection()
    assert 'PARAMETER["central_meridian",-105]' in pj, 'PARAMETER["central_meridian",-105] not in projection'
    assert 'PARAMETER["false_northing",10000000]' in pj, \
        'Did not find false_northing of 10000000'

###############################################################################
#


def test_bag_vr_normal():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.Open('data/test_vr.bag')
    assert ds is not None

    got_md = gdal.Info(ds, computeChecksum=True, format='json', wktFormat='WKT1')
    expected_md = {
        'bands': [
            {'band': 1,
             'block': [6, 1],
             'checksum': 65529,
             'colorInterpretation': 'Undefined',
             'description': 'elevation',
             'max': 10.0,
             'metadata': {},
             'min': -10.0,
             'noDataValue': 1000000.0,
             'type': 'Float32'},
            {'band': 2,
             'block': [6, 1],
             'checksum': 60,
             'colorInterpretation': 'Undefined',
             'description': 'uncertainty',
             'max': 5.0,
             'metadata': {},
             'min': 0.0,
             'noDataValue': 1000000.0,
             'type': 'Float32'}],
        'coordinateSystem': { 'dataAxisToSRSAxisMapping': [1, 2], 'wkt': 'PROJCS["NAD83 / UTM zone 10N",\n    GEOGCS["NAD83",\n        DATUM["North_American_Datum_1983",\n            SPHEROID["GRS 1980",6378137,298.257222101004,\n                AUTHORITY["EPSG","7019"]],\n            TOWGS84[0,0,0,0,0,0,0],\n            AUTHORITY["EPSG","6269"]],\n        PRIMEM["Greenwich",0,\n            AUTHORITY["EPSG","8901"]],\n        UNIT["degree",0.0174532925199433,\n            AUTHORITY["EPSG","9122"]],\n        AUTHORITY["EPSG","4269"]],\n    PROJECTION["Transverse_Mercator"],\n    PARAMETER["latitude_of_origin",0],\n    PARAMETER["central_meridian",-123],\n    PARAMETER["scale_factor",0.9996],\n    PARAMETER["false_easting",500000],\n    PARAMETER["false_northing",0],\n    UNIT["metre",1,\n        AUTHORITY["EPSG","9001"]],\n    AXIS["Easting",EAST],\n    AXIS["Northing",NORTH],\n    AUTHORITY["EPSG","26910"]]'},
        'geoTransform': [85.0, 30.0, 0.0, 500112.0, 0.0, -32.0],
        'metadata': {'': {'AREA_OR_POINT': 'Point',
                          'BAG_DATETIME': '2018-08-08T12:34:56',
                          'BagVersion': '1.6.2',
                          'HAS_SUPERGRIDS': 'TRUE',
                          'MAX_RESOLUTION_X': '29.900000',
                          'MAX_RESOLUTION_Y': '31.900000',
                          'MIN_RESOLUTION_X': '4.983333',
                          'MIN_RESOLUTION_Y': '5.316667'}},
        'size': [6, 4]
    }

    for key in expected_md:
        if key not in got_md or got_md[key] != expected_md[key]:
            import pprint
            pp = pprint.PrettyPrinter()
            pp.pprint(got_md)
            pytest.fail(key)

    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=LOW_RES_GRID', 'MINX=0'])
    assert gdal.GetLastErrorMsg() != '', 'warning expected'

    got_md2 = gdal.Info(ds, computeChecksum=True, format='json', wktFormat='WKT1')
    assert got_md2 == got_md

###############################################################################
#


def test_bag_vr_list_supergrids():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('data/test_vr.bag', open_options=['MODE=LIST_SUPERGRIDS'])
    assert ds is not None
    sub_ds = ds.GetSubDatasets()
    assert len(sub_ds) == 24

    assert sub_ds[0][0] == 'BAG:"data/test_vr.bag":supergrid:0:0'

    with gdaltest.error_handler():
        # Bounding box filter ignored since only part of MINX, MINY, MAXX and
        # MAXY has been specified
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=LIST_SUPERGRIDS', 'MINX=200'])
    sub_ds = ds.GetSubDatasets()
    assert len(sub_ds) == 24

    ds = gdal.OpenEx('data/test_vr.bag', open_options=[
                     'MODE=LIST_SUPERGRIDS', 'MINX=100', 'MAXX=220', 'MINY=500000', 'MAXY=500100'])
    sub_ds = ds.GetSubDatasets()
    assert len(sub_ds) == 6
    assert sub_ds[0][0] == 'BAG:"data/test_vr.bag":supergrid:1:1'

    ds = gdal.OpenEx('data/test_vr.bag', open_options=[
                     'MODE=LIST_SUPERGRIDS', 'RES_FILTER_MIN=5', 'RES_FILTER_MAX=10'])
    sub_ds = ds.GetSubDatasets()
    assert len(sub_ds) == 12
    assert sub_ds[0][0] == 'BAG:"data/test_vr.bag":supergrid:0:3'

    ds = gdal.OpenEx('data/test_vr.bag', open_options=[
                     'SUPERGRIDS_INDICES=(2,1),(3,4)'])
    sub_ds = ds.GetSubDatasets()
    assert len(sub_ds) == 2
    assert (sub_ds[0][0] == 'BAG:"data/test_vr.bag":supergrid:2:1' and \
       sub_ds[1][0] == 'BAG:"data/test_vr.bag":supergrid:3:4')

    # One single tuple: open the subdataset directly
    ds = gdal.OpenEx('data/test_vr.bag', open_options=[
                     'SUPERGRIDS_INDICES=(2,1)'])
    ds2 = gdal.Open('BAG:"data/test_vr.bag":supergrid:2:1')
    assert gdal.Info(ds) == gdal.Info(ds2), sub_ds

    # Test invalid values for SUPERGRIDS_INDICES
    for invalid_val in ['', 'x', '(', '(1', '(1,', '(1,)', '(1,2),',
                        '(x,2)', '(2,x)']:
        with gdaltest.error_handler():
            ds = gdal.OpenEx('data/test_vr.bag', open_options=[
                'SUPERGRIDS_INDICES=' + invalid_val])
        assert gdal.GetLastErrorMsg() != '', invalid_val

    
###############################################################################
#


def test_bag_vr_open_supergrids():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.Open('BAG:"data/test_vr.bag":supergrid:0:0')
    assert ds is not None

    got_md = gdal.Info(ds, computeChecksum=True, format='json', wktFormat='WKT1')
    expected_md = {
        'bands': [
            {'band': 1,
             'block': [2, 1],
             'checksum': 65529,
             'colorInterpretation': 'Undefined',
             'description': 'elevation',
             'metadata': {},
             'noDataValue': 1000000.0,
             'type': 'Float32'},
            {'band': 2,
             'block': [2, 1],
             'checksum': 13,
             'colorInterpretation': 'Undefined',
             'description': 'uncertainty',
             'metadata': {},
             'noDataValue': 1000000.0,
             'type': 'Float32'}],
        'coordinateSystem': {'dataAxisToSRSAxisMapping': [1, 2], 'wkt': 'PROJCS["NAD83 / UTM zone 10N",\n    GEOGCS["NAD83",\n        DATUM["North_American_Datum_1983",\n            SPHEROID["GRS 1980",6378137,298.257222101004,\n                AUTHORITY["EPSG","7019"]],\n            TOWGS84[0,0,0,0,0,0,0],\n            AUTHORITY["EPSG","6269"]],\n        PRIMEM["Greenwich",0,\n            AUTHORITY["EPSG","8901"]],\n        UNIT["degree",0.0174532925199433,\n            AUTHORITY["EPSG","9122"]],\n        AUTHORITY["EPSG","4269"]],\n    PROJECTION["Transverse_Mercator"],\n    PARAMETER["latitude_of_origin",0],\n    PARAMETER["central_meridian",-123],\n    PARAMETER["scale_factor",0.9996],\n    PARAMETER["false_easting",500000],\n    PARAMETER["false_northing",0],\n    UNIT["metre",1,\n        AUTHORITY["EPSG","9001"]],\n    AXIS["Easting",EAST],\n    AXIS["Northing",NORTH],\n    AUTHORITY["EPSG","26910"]]'},
        'geoTransform': [70.10000038146973,
                         29.899999618530273,
                         0.0,
                         500031.89999961853,
                         0.0,
                         -31.899999618530273],
        'metadata': {'': {'AREA_OR_POINT': 'Point',
                          'BAG_DATETIME': '2018-08-08T12:34:56',
                          'BagVersion': '1.6.2'},
                     'IMAGE_STRUCTURE': {'INTERLEAVE': 'PIXEL'}},
        'size': [2, 2]
    }

    for key in expected_md:
        if key not in got_md or got_md[key] != expected_md[key]:
            import pprint
            pp = pprint.PrettyPrinter()
            pp.pprint(got_md)
            pytest.fail(key)

    with gdaltest.error_handler():
        ds = gdal.Open('BAG:"/vsimem/unexisting.bag":supergrid:0:0')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('BAG:"data/test_vr.bag":supergrid:4:0')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('BAG:"data/test_vr.bag":supergrid:0:6')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.OpenEx('BAG:"data/test_vr.bag":supergrid:0:0',
                         open_options=['MINX=0'])
    assert gdal.GetLastErrorMsg() != '', 'warning expected'

###############################################################################
#


def test_bag_vr_resampled():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('data/test_vr.bag', open_options=['MODE=RESAMPLED_GRID'])
    assert ds is not None

    got_md = gdal.Info(ds, computeChecksum=True, format='json', wktFormat='WKT1')
    expected_md = {
        'bands': [
            {'band': 1,
             'block': [36, 24],
             'checksum': 4582,
             'colorInterpretation': 'Undefined',
             'description': 'elevation',
             'max': 10.0,
             'metadata': {},
             'min': -10.0,
             'noDataValue': 1000000.0,
             'type': 'Float32'},
            {'band': 2,
             'block': [36, 24],
             'checksum': 6237,
             'colorInterpretation': 'Undefined',
             'description': 'uncertainty',
             'max': 10.0,
             'metadata': {},
             'min': 0.0,
             'noDataValue': 1000000.0,
             'type': 'Float32'}],
        'coordinateSystem': {'dataAxisToSRSAxisMapping': [1, 2], 'wkt': 'PROJCS["NAD83 / UTM zone 10N",\n    GEOGCS["NAD83",\n        DATUM["North_American_Datum_1983",\n            SPHEROID["GRS 1980",6378137,298.257222101004,\n                AUTHORITY["EPSG","7019"]],\n            TOWGS84[0,0,0,0,0,0,0],\n            AUTHORITY["EPSG","6269"]],\n        PRIMEM["Greenwich",0,\n            AUTHORITY["EPSG","8901"]],\n        UNIT["degree",0.0174532925199433,\n            AUTHORITY["EPSG","9122"]],\n        AUTHORITY["EPSG","4269"]],\n    PROJECTION["Transverse_Mercator"],\n    PARAMETER["latitude_of_origin",0],\n    PARAMETER["central_meridian",-123],\n    PARAMETER["scale_factor",0.9996],\n    PARAMETER["false_easting",500000],\n    PARAMETER["false_northing",0],\n    UNIT["metre",1,\n        AUTHORITY["EPSG","9001"]],\n    AXIS["Easting",EAST],\n    AXIS["Northing",NORTH],\n    AUTHORITY["EPSG","26910"]]'},
        'geoTransform': [85.0,
                         4.983333110809326,
                         0.0,
                         500111.5999984741,
                         0.0,
                         -5.316666603088379],
        'metadata': {'': {'AREA_OR_POINT': 'Point',
                          'BAG_DATETIME': '2018-08-08T12:34:56',
                          'BagVersion': '1.6.2'},
                     'IMAGE_STRUCTURE': {'INTERLEAVE': 'PIXEL'}},
        'size': [36, 24]
    }

    for key in expected_md:
        if key not in got_md or got_md[key] != expected_md[key]:
            import pprint
            pp = pprint.PrettyPrinter()
            pp.pprint(got_md)
            pytest.fail(key)

    data_ref = ds.ReadRaster()

    # Test that block size has no influence on the result
    for block_size in (2, 5, 9, 16):
        with gdaltest.config_option('GDAL_BAG_BLOCK_SIZE', str(block_size)):
            ds = gdal.OpenEx('data/test_vr.bag',
                             open_options=['MODE=RESAMPLED_GRID'])
            assert ds.GetRasterBand(1).GetBlockSize() == [block_size, block_size]
            data = ds.ReadRaster()

        assert data == data_ref, block_size

    # Test overviews
    with gdaltest.config_option('GDAL_BAG_MIN_OVR_SIZE', '4'):
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=RESAMPLED_GRID'])
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(2) is None

    ovr = ds.GetRasterBand(1).GetOverview(0)
    cs = ovr.Checksum()
    assert cs == 681
    ovr = ds.GetRasterBand(2).GetOverview(0)
    cs = ovr.Checksum()
    assert cs == 1344
    ds.GetRasterBand(1).GetOverview(0).FlushCache()
    ds.GetRasterBand(1).GetOverview(1).FlushCache()
    cs = ovr.Checksum()
    assert cs == 1344
    ovr = ds.GetRasterBand(1).GetOverview(0)
    cs = ovr.Checksum()
    assert cs == 681

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'MINX=90', 'MAXX=120', 'MAXY=500112'])
    gt = ds.GetGeoTransform()
    got = (gt[0], gt[3], ds.RasterXSize)
    assert got == (90.0, 500112.0, 6)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'MINY=500000'])
    gt = ds.GetGeoTransform()
    got = (gt[3] + gt[5] * ds.RasterYSize, ds.RasterYSize)
    assert got == (500000.0, 21)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RESX=5', 'RESY=6'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (5.0, -6.0)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_STRATEGY=MIN'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (4.983333110809326, -5.316666603088379)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_STRATEGY=MAX'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (29.899999618530273, -31.899999618530273)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_STRATEGY=MEAN'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (12.209166447321573, -13.025833209355673)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_FILTER_MIN=0',
                                   'RES_FILTER_MAX=8'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (8.0, -8.0)
    got = (ds.GetRasterBand(1).Checksum(), ds.GetRasterBand(2).Checksum())
    assert got == (2099, 2747)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_FILTER_MAX=8'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (8.0, -8.0)
    got = (ds.GetRasterBand(1).Checksum(), ds.GetRasterBand(2).Checksum())
    assert got == (2099, 2747)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_FILTER_MIN=8',
                                   'RES_FILTER_MAX=16'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (16.0, -16.0)
    got = (ds.GetRasterBand(1).Checksum(), ds.GetRasterBand(2).Checksum())
    assert got == (796, 864)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_FILTER_MIN=16',
                                   'RES_FILTER_MAX=32'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (32.0, -32.0)
    got = (ds.GetRasterBand(1).Checksum(), ds.GetRasterBand(2).Checksum())
    assert got == (207, 207)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_FILTER_MIN=16'])
    gt = ds.GetGeoTransform()
    got = (gt[1], gt[5])
    assert got == (29.899999618530273, -31.899999618530273)
    got = (ds.GetRasterBand(1).Checksum(), ds.GetRasterBand(2).Checksum())
    assert got == (165, 205)

    # Too big RES_FILTER_MIN
    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=RESAMPLED_GRID',
                                       'RES_FILTER_MIN=32'])
    assert ds is None

    # Too small RES_FILTER_MAX
    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=RESAMPLED_GRID',
                                       'RES_FILTER_MAX=4'])
    assert ds is None

    # RES_FILTER_MIN >= RES_FILTER_MAX
    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=RESAMPLED_GRID',
                                       'RES_FILTER_MIN=4',
                                       'RES_FILTER_MAX=4'])
    assert ds is None

    # Test VALUE_POPULATION
    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_STRATEGY=MEAN',
                                   'VALUE_POPULATION=MAX'])
    m1_max, M1_max, mean1_max, _ = ds.GetRasterBand(1).ComputeStatistics(False)
    m2_max, M2_max, mean2_max, _ = ds.GetRasterBand(2).ComputeStatistics(False)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_STRATEGY=MEAN',
                                   'VALUE_POPULATION=MEAN'])
    m1_mean, M1_mean, mean1_mean, _ = ds.GetRasterBand(
        1).ComputeStatistics(False)
    m2_mean, M2_mean, mean2_mean, _ = ds.GetRasterBand(
        2).ComputeStatistics(False)

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'RES_STRATEGY=MEAN',
                                   'VALUE_POPULATION=MIN'])
    m1_min, M1_min, mean1_min, _ = ds.GetRasterBand(1).ComputeStatistics(False)
    m2_min, M2_min, mean2_min, _ = ds.GetRasterBand(2).ComputeStatistics(False)

    if mean1_min >= mean1_mean or mean1_mean >= mean1_max:
        print(m1_max, M1_max, mean1_max)
        print(m2_max, M2_max, mean2_max)
        print(m1_mean, M1_mean, mean1_mean)
        print(m2_mean, M2_mean, mean2_mean)
        print(m1_min, M1_min, mean1_min)
        pytest.fail(m2_min, M2_min, mean2_min)

    if m2_min >= m2_max or \
       (m2_mean, M2_mean, mean2_mean) != (m2_max, M2_max, mean2_max):
        print(m1_max, M1_max, mean1_max)
        print(m1_mean, M1_mean, mean1_mean)
        print(m1_min, M1_min, mean1_min)
        pytest.fail(m2_min, M2_min, mean2_min)

    
###############################################################################
#


def test_bag_vr_resampled_mask():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'SUPERGRIDS_MASK=YES'])
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4507

###############################################################################
#


def test_bag_vr_resampled_interpolated():

    if gdaltest.bag_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('data/test_vr.bag',
                     open_options=['MODE=RESAMPLED_GRID',
                                   'INTERPOLATION=INVDIST'])
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 2175

    # Test overviews
    with gdaltest.config_option('GDAL_BAG_MIN_OVR_SIZE', '4'):
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=RESAMPLED_GRID'])
    assert ds.GetRasterBand(1).GetOverviewCount() == 2

    # Incompatible options
    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_vr.bag',
                         open_options=['MODE=RESAMPLED_GRID',
                                       'SUPERGRIDS_MASK=YES',
                                       'INTERPOLATION=INVDIST'])
    assert ds is None

###############################################################################
#


def test_bag_write_single_band():

    if gdaltest.bag_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('BAG', 'byte.tif', 1, 4672)
    ret = tst.testCreateCopy(quiet_error_handler=False,
                             new_filename='/vsimem/out.bag')
    return ret

###############################################################################
#


def test_bag_write_two_bands():

    if gdaltest.bag_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('BAG', 'test_vr.bag', 2, 60,
                            options=['BLOCK_SIZE=2',
                                     'VAR_ABSTRACT=foo',
                                     'VAR_XML_IDENTIFICATION_CITATION=<bar/>'])
    tst.testCreateCopy(quiet_error_handler=False,
                             delete_copy=False,
                             new_filename='/vsimem/out.bag')

    ds = gdal.Open('/vsimem/out.bag')
    xml = ds.GetMetadata_List('xml:BAG')[0]
    assert '<bar />' in xml
    assert 'Generated by GDAL ' in xml

    gdal.Unlink('/vsimem/out.bag')

###############################################################################
#


def test_bag_write_south_up():

    if gdaltest.bag_drv is None:
        pytest.skip()

    # Generate a south-up dataset
    src_ds = gdal.Warp('', 'data/byte.tif',
                       format='MEM',
                       outputBounds=[440720, 3751320, 441920, 3750120])

    # Translate it into BAG
    ds = gdal.Translate('/vsimem/out.bag', src_ds)

    # Check that it is presented as north-up
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672

    gt = ds.GetGeoTransform()
    assert gt == (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    ds = None

    gdal.Unlink('/vsimem/out.bag')



