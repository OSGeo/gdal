#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ENVI format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
# See also: gcore/envi_read.py for a driver focused on raster data types.
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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
import struct

import gdaltest
import pytest

###############################################################################
# Perform simple read test.


def test_envi_1():

    tst = gdaltest.GDALTest('envi', 'envi/aea.dat', 1, 14823)

    prj = """PROJCS["unnamed",
    GEOGCS["Ellipse Based",
        DATUM["Ellipse Based",
            SPHEROID["Unnamed",6378206.4,294.9786982139109]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Albers_Conic_Equal_Area"],
    PARAMETER["standard_parallel_1",29.5],
    PARAMETER["standard_parallel_2",45.5],
    PARAMETER["latitude_of_center",23],
    PARAMETER["longitude_of_center",-96],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["Meter",1]]"""

    return tst.testOpen(check_prj=prj,
                        check_gt=(-936408.178, 28.5, 0.0,
                                  2423902.344, 0.0, -28.5))

###############################################################################
# Verify this can be exported losslessly.


def test_envi_2():

    tst = gdaltest.GDALTest('envi', 'envi/aea.dat', 1, 14823)
    return tst.testCreateCopy(check_gt=1)

###############################################################################
# Try the Create interface with an RGB image.


def test_envi_3():

    tst = gdaltest.GDALTest('envi', 'rgbsmall.tif', 2, 21053)
    return tst.testCreate()

###############################################################################
# Test LCC Projection.


def test_envi_4():

    tst = gdaltest.GDALTest('envi', 'envi/aea.dat', 1, 24)

    prj = """PROJCS["unnamed",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101],
            TOWGS84[0,0,0,0,0,0,0]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",33.90363402775256],
    PARAMETER["standard_parallel_2",33.62529002776137],
    PARAMETER["latitude_of_origin",33.76446202775696],
    PARAMETER["central_meridian",-117.4745428888127],
    PARAMETER["false_easting",20000],
    PARAMETER["false_northing",30000],
    UNIT["Meter",1]]"""

    return tst.testSetProjection(prj=prj)

###############################################################################
# Test TM Projection.


def test_envi_5():

    tst = gdaltest.GDALTest('envi', 'envi/aea.dat', 1, 24)
    prj = """PROJCS["OSGB 1936 / British National Grid",
    GEOGCS["OSGB 1936",
        DATUM["OSGB_1936",
            SPHEROID["Airy 1830",6377563.396,299.3249646,
                AUTHORITY["EPSG","7001"]],
            TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489],
            AUTHORITY["EPSG","6277"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.01745329251994328,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4277"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",49],
    PARAMETER["central_meridian",-2],
    PARAMETER["scale_factor",0.9996012717],
    PARAMETER["false_easting",400000],
    PARAMETER["false_northing",-100000],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","27700"]]"""

    # now it goes through ESRI WKT processing.
    expected_prj = """PROJCS["OSGB_1936_British_National_Grid",
    GEOGCS["GCS_OSGB 1936",
        DATUM["OSGB_1936",
            SPHEROID["Airy_1830",6377563.396,299.3249646]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",49],
    PARAMETER["central_meridian",-2],
    PARAMETER["scale_factor",0.9996012717],
    PARAMETER["false_easting",400000],
    PARAMETER["false_northing",-100000],
    UNIT["Meter",1]]"""

    return tst.testSetProjection(prj=prj, expected_prj=expected_prj)

###############################################################################
# Test LAEA Projection.


def test_envi_6():

    gdaltest.envi_tst = gdaltest.GDALTest('envi', 'envi/aea.dat', 1, 24)

    prj = """PROJCS["unnamed",
    GEOGCS["Unknown datum based upon the Authalic Sphere",
        DATUM["D_Ellipse_Based",
            SPHEROID["Sphere",6370997,0]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.0174532925199433]],
    PROJECTION["Lambert_Azimuthal_Equal_Area"],
    PARAMETER["latitude_of_center",33.764462027757],
    PARAMETER["longitude_of_center",-117.474542888813],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""

    return gdaltest.envi_tst.testSetProjection(prj=prj)

###############################################################################
# Verify VSIF*L capacity


def test_envi_7():

    tst = gdaltest.GDALTest('envi', 'envi/aea.dat', 1, 14823)
    return tst.testCreateCopy(check_gt=1, vsimem=1)

###############################################################################
# Test fix for #3751


def test_envi_8():

    ds = gdal.GetDriverByName('ENVI').Create('/vsimem/foo.bsq', 10, 10, 1)
    set_gt = (50000, 1, 0, 4500000, 0, -1)
    ds.SetGeoTransform(set_gt)
    got_gt = ds.GetGeoTransform()
    assert set_gt == got_gt, 'did not get expected geotransform'
    ds = None

    gdal.GetDriverByName('ENVI').Delete('/vsimem/foo.bsq')

###############################################################################
# Verify reading a compressed file


def test_envi_9():

    tst = gdaltest.GDALTest('envi', 'envi/aea_compressed.dat', 1, 14823)
    return tst.testCreateCopy(check_gt=1)

###############################################################################
# Test RPC reading and writing


def test_envi_10():

    src_ds = gdal.Open('data/envi/envirpc.img')
    out_ds = gdal.GetDriverByName('ENVI').CreateCopy('/vsimem/envirpc.img', src_ds)
    src_ds = None
    del out_ds

    gdal.Unlink('/vsimem/envirpc.img.aux.xml')

    ds = gdal.Open('/vsimem/envirpc.img')
    md = ds.GetMetadata('RPC')
    ds = None

    gdal.GetDriverByName('ENVI').Delete('/vsimem/envirpc.img')

    assert md['HEIGHT_OFF'] == '3355'

###############################################################################
# Check .sta reading


def test_envi_11():

    ds = gdal.Open('data/envi/envistat')
    val = ds.GetRasterBand(1).GetStatistics(0, 0)
    ds = None

    assert val == [1.0, 3.0, 2.0, 0.5], 'bad stats'

###############################################################################
# Test category names reading and writing


def test_envi_12():

    src_ds = gdal.Open('data/envi/testenviclasses')
    out_ds = gdal.GetDriverByName('ENVI').CreateCopy('/vsimem/testenviclasses', src_ds)
    src_ds = None
    del out_ds

    gdal.Unlink('/vsimem/testenviclasses.aux.xml')

    ds = gdal.Open('/vsimem/testenviclasses')
    category = ds.GetRasterBand(1).GetCategoryNames()
    ct = ds.GetRasterBand(1).GetColorTable()

    assert category == ['Black', 'White'], 'bad category names'

    assert ct.GetCount() == 2, 'bad color entry count'

    assert ct.GetColorEntry(0) == (0, 0, 0, 255), 'bad color entry'

    ds = None
    gdal.GetDriverByName('ENVI').Delete('/vsimem/testenviclasses')

###############################################################################
# Test writing of metadata from the ENVI metadata domain and read it back (#4957)


def test_envi_13():

    ds = gdal.GetDriverByName('ENVI').Create('/vsimem/envi_13.dat', 1, 1)
    ds.SetMetadata(['lines=100', 'sensor_type=Landsat TM', 'foo'], 'ENVI')
    ds = None

    gdal.Unlink('/vsimem/envi_13.dat.aux.xml')

    ds = gdal.Open('/vsimem/envi_13.dat')
    lines = ds.RasterYSize
    val = ds.GetMetadataItem('sensor_type', 'ENVI')
    ds = None
    gdal.GetDriverByName('ENVI').Delete('/vsimem/envi_13.dat')

    assert lines == 1

    assert val == 'Landsat TM'

###############################################################################
# Test that the image file is at the expected size on closing (#6662)


def test_envi_14():

    gdal.GetDriverByName('ENVI').Create('/vsimem/envi_14.dat', 3, 4, 5, gdal.GDT_Int16)

    gdal.Unlink('/vsimem/envi_14.dat.aux.xml')

    assert gdal.VSIStatL('/vsimem/envi_14.dat').size == 3 * 4 * 5 * 2

    gdal.GetDriverByName('ENVI').Delete('/vsimem/envi_14.dat')

###############################################################################
# Test reading and writing geotransform matrix with rotation


def test_envi_15():

    src_ds = gdal.Open('data/envi/rotation.img')
    got_gt = src_ds.GetGeoTransform()
    expected_gt = [736600.089, 1.0981889363046606, -2.4665727356350224,
                   4078126.75, -2.4665727356350224, -1.0981889363046606]
    assert max([abs((got_gt[i] - expected_gt[i]) / expected_gt[i]) for i in range(6)]) <= 1e-5, \
        'did not get expected geotransform'

    gdal.GetDriverByName('ENVI').CreateCopy('/vsimem/envi_15.dat', src_ds)

    ds = gdal.Open('/vsimem/envi_15.dat')
    got_gt = ds.GetGeoTransform()
    assert max([abs((got_gt[i] - expected_gt[i]) / expected_gt[i]) for i in range(6)]) <= 1e-5, \
        'did not get expected geotransform'
    ds = None
    gdal.GetDriverByName('ENVI').Delete('/vsimem/envi_15.dat')

###############################################################################
# Test reading a truncated ENVI dataset (see #915)


def test_envi_truncated():

    gdal.GetDriverByName('ENVI').CreateCopy('/vsimem/envi_truncated.dat',
                                            gdal.Open('data/byte.tif'))

    f = gdal.VSIFOpenL('/vsimem/envi_truncated.dat', 'wb+')
    gdal.VSIFTruncateL(f, int(20 * 20 / 2))
    gdal.VSIFCloseL(f)

    with gdaltest.config_option('RAW_CHECK_FILE_SIZE', 'YES'):
        ds = gdal.Open('/vsimem/envi_truncated.dat')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.GetDriverByName('ENVI').Delete('/vsimem/envi_truncated.dat')

    assert cs == 2315

###############################################################################
# Test writing & reading GCPs (#1528)


def test_envi_gcp():

    filename = '/vsimem/test_envi_gcp.dat'
    ds = gdal.GetDriverByName('ENVI').Create(filename, 1, 1)
    gcp = gdal.GCP()
    gcp.GCPPixel = 1
    gcp.GCPLine = 2
    gcp.GCPX = 3
    gcp.GCPY = 4
    ds.SetGCPs([gcp], None)
    ds = None
    gdal.Unlink(filename + ".aux.xml")

    ds = gdal.Open(filename)
    assert ds.GetGCPCount() == 1
    gcps = ds.GetGCPs()
    assert len(gcps) == 1
    gcp = gcps[0]
    ds = None
    assert gcp.GCPPixel == 1
    assert gcp.GCPLine == 2
    assert gcp.GCPX == 3
    assert gcp.GCPY == 4

    gdal.GetDriverByName('ENVI').Delete(filename)

###############################################################################
# Test updating big endian ordered (#1796)


def test_envi_bigendian():

    ds = gdal.Open('data/envi/uint16_envi_bigendian.dat')
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None

    for ext in ('dat', 'hdr'):
        filename = 'uint16_envi_bigendian.' + ext
        gdal.FileFromMemBuffer('/vsimem/' + filename,
                            open('data/envi/' + filename, 'rb').read())

    filename = '/vsimem/uint16_envi_bigendian.dat'
    ds = gdal.Open(filename, gdal.GA_Update)
    ds.SetGeoTransform([0, 2, 0, 0, 0, -2])
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None

    gdal.GetDriverByName('ENVI').Delete(filename)

###############################################################################
# Test different interleaving


def test_envi_interleaving():

    for filename in ('data/envi/envi_rgbsmall_bip.img', 'data/envi/envi_rgbsmall_bil.img', 'data/envi/envi_rgbsmall_bsq.img'):
        ds = gdal.Open(filename)
        assert ds, filename
        assert ds.GetRasterBand(1).Checksum() == 20718, filename
        assert ds.GetRasterBand(2).Checksum() == 20669, filename
        assert ds.GetRasterBand(3).Checksum() == 20895, filename
        ds = None

###############################################################################
# Test nodata


def test_envi_nodata():

    filename = '/vsimem/test_envi_nodata.dat'
    ds = gdal.GetDriverByName('ENVI').Create(filename, 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(1)
    ds = None

    gdal.Unlink(filename + '.aux.xml')

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == 1.0
    ds = None

    gdal.GetDriverByName('ENVI').Delete(filename)


###############################################################################
# Test reading and writing geotransform matrix with rotation = 180


def test_envi_rotation_180():

    filename = '/vsimem/test_envi_rotation_180.dat'
    ds = gdal.GetDriverByName('ENVI').Create(filename, 1, 1)
    ds.SetGeoTransform([0,10,0,0,0,10])
    ds = None

    gdal.Unlink(filename + '.aux.xml')

    ds = gdal.Open(filename)
    got_gt = ds.GetGeoTransform()
    assert got_gt == (0,10,0,0,0,10)
    ds = None

    gdal.GetDriverByName('ENVI').Delete(filename)

###############################################################################
# Test writing different interleaving


@pytest.mark.parametrize('interleaving', ['bip', 'bil', 'bsq'])
def test_envi_writing_interleaving(interleaving):

    srcfilename = 'data/envi/envi_rgbsmall_' + interleaving + '.img'
    dstfilename = '/vsimem/out'
    try:
        gdal.Translate(dstfilename, srcfilename,
                       format = 'ENVI',
                       creationOptions=['INTERLEAVE=' + interleaving])
        ref_data = open(srcfilename, 'rb').read()
        f = gdal.VSIFOpenL(dstfilename, 'rb')
        if f:
            got_data = gdal.VSIFReadL(1, len(ref_data), f)
            gdal.VSIFCloseL(f)
            assert got_data == ref_data
    finally:
        gdal.Unlink(dstfilename)
        gdal.Unlink(dstfilename + '.hdr')

###############################################################################
# Test writing different interleaving (larger file)


@pytest.mark.parametrize('interleaving', ['bip', 'bil', 'bsq'])
def test_envi_writing_interleaving_larger_file(interleaving):

    dstfilename = '/vsimem/out'
    try:
        xsize = 10000
        ysize = 10
        bands = 100
        with gdaltest.SetCacheMax(xsize * (ysize // 2)):
            ds = gdal.GetDriverByName('ENVI').Create(dstfilename, xsize, ysize, bands, options = ['INTERLEAVE=' + interleaving])
            ds.GetRasterBand(1).Fill(1)
            for i in range(bands):
                v = struct.pack('B', i+1)
                ds.GetRasterBand(i+1).WriteRaster(0, 0, xsize, ysize // 2, v * (xsize * (ysize // 2)))
            for i in range(bands):
                v = struct.pack('B', i+1)
                ds.GetRasterBand(i+1).WriteRaster(0, ysize // 2, xsize, ysize // 2, v * (xsize * (ysize // 2)))
            ds = None

        ds = gdal.Open(dstfilename)
        for i in range(bands):
            v = struct.pack('B', i+1)
            assert ds.GetRasterBand(i+1).ReadRaster() == v * (xsize * ysize)
    finally:
        gdal.Unlink(dstfilename)
        gdal.Unlink(dstfilename + '.hdr')
