#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for MrSID driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

import os
import shutil
from osgeo import gdal


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('MrSID')

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.mrsid_drv = gdal.GetDriverByName('MrSID')
    gdaltest.jp2mrsid_drv = gdal.GetDriverByName('JP2MrSID')
    if gdaltest.jp2mrsid_drv:
        gdaltest.deregister_all_jpeg2000_drivers_but('JP2MrSID')

    yield

    gdaltest.reregister_all_jpeg2000_drivers()

    try:
        os.remove('data/sid/mercator.sid.aux.xml')
        os.remove('data/sid/mercator_new.sid.aux.xml')
    except OSError:
        pass

###############################################################################
# Read a simple byte file, checking projections and geotransform.


def test_mrsid_1():

    tst = gdaltest.GDALTest('MrSID', 'sid/mercator.sid', 1, None)

    gt = (-15436.385771224039, 60.0, 0.0, 3321987.8617962394, 0.0, -60.0)
    #
    # Old, internally generated.
    #
    prj = """PROJCS["MER         E000|",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""
    #
    # MrSID SDK getWKT() method.
    #
    prj = """PROJCS["MER         E000|",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982139006,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",1],
    PARAMETER["central_meridian",1],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",1],
    PARAMETER["false_northing",1],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""

    #
    # MrSID SDK getWKT() method - DSDK 8 and newer?
    #
    prj = """PROJCS["MER         E000|",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982139006,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""

    tst.testOpen(check_gt=gt,
                       check_stat=(0.0, 255.0, 103.319, 55.153),
                       check_approx_stat=(2.0, 243.0, 103.131, 43.978))

    ds = gdal.Open('data/sid/mercator.sid')
    got_prj = ds.GetProjectionRef()
    ds = None

    if prj.find('North_American_Datum_1927') == -1 or \
       prj.find('Mercator_1SP') == -1:
        print(got_prj)
        pytest.fail('did not get expected projection')

    if got_prj != prj:
        print('Warning: did not get exactly expected projection. Got %s' % got_prj)


###############################################################################
# Do a direct IO to read the image at a resolution for which there is no
# builtin overview.  Checks for the bug Steve L found in the optimized
# RasterIO implementation.


def test_mrsid_2():

    ds = gdal.Open('data/sid/mercator.sid')

    try:
        data = ds.ReadRaster(0, 0, 515, 515, buf_xsize=10, buf_ysize=10)
    except:
        pytest.fail('Small overview read failed: ' + gdal.GetLastErrorMsg())

    ds = None

    total = sum(data)
    mean = float(total) / len(data)

    assert mean >= 95 and mean <= 105, 'image mean out of range.'

###############################################################################
# Test overview reading.


def test_mrsid_3():

    ds = gdal.Open('data/sid/mercator.sid')

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 4, 'did not get expected overview count'

    new_stat = band.GetOverview(3).GetStatistics(0, 1)

    check_stat = (11.0, 230.0, 103.42607897153351, 39.952592422557757)

    stat_epsilon = 0.0001
    for i in range(4):
        if new_stat[i] != pytest.approx(check_stat[i], abs=stat_epsilon):
            print('')
            print('old = ', check_stat)
            print('new = ', new_stat)
            pytest.fail('Statistics differ.')


###############################################################################
# Check a new (V3) file which uses a different form for coordinate sys.


def test_mrsid_4():

    try:
        os.remove('data/sid/mercator_new.sid.aux.xml')
    except OSError:
        pass

    tst = gdaltest.GDALTest('MrSID', 'sid/mercator_new.sid', 1, None)

    gt = (-15436.385771224039, 60.0, 0.0, 3321987.8617962394, 0.0, -60.0)
    prj = """PROJCS["MER         E000",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",33.76446202777777],
    PARAMETER["central_meridian",-117.4745428888889],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""

    ret = tst.testOpen(check_gt=gt, check_prj=prj,
                       check_stat=(0.0, 255.0, 103.112, 52.477),
                       check_approx_stat=(0.0, 255.0, 102.684, 51.614))

    try:
        os.remove('data/sid/mercator_new.sid.aux.xml')
    except OSError:
        pass

    return ret

###############################################################################
# Open byte.jp2


def test_mrsid_6():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    srs = """PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","26711"]]
"""
    gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    tst = gdaltest.GDALTest('JP2MrSID', 'jpeg2000/byte.jp2', 1, 50054)
    return tst.testOpen(check_prj=srs, check_gt=gt)


###############################################################################
# Open int16.jp2

def test_mrsid_7():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    ds = gdal.Open('data/jpeg2000/int16.jp2')
    ds_ref = gdal.Open('data/int16.tif')

    maxdiff = gdaltest.compare_ds(ds, ds_ref)

    if maxdiff > 5:
        print(ds.GetRasterBand(1).Checksum())
        print(ds_ref.GetRasterBand(1).Checksum())

        ds = None
        ds_ref = None
        pytest.fail('Image too different from reference')

    ds = None
    ds_ref = None

###############################################################################
# Test PAM override for nodata, coordsys, and geotransform.


def test_mrsid_8():

    new_gt = (10000, 50, 0, 20000, 0, -50)
    new_srs = """PROJCS["OSGB 1936 / British National Grid",GEOGCS["OSGB 1936",DATUM["OSGB_1936",SPHEROID["Airy 1830",6377563.396,299.3249646,AUTHORITY["EPSG","7001"]],AUTHORITY["EPSG","6277"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4277"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",49],PARAMETER["central_meridian",-2],PARAMETER["scale_factor",0.9996012717],PARAMETER["false_easting",400000],PARAMETER["false_northing",-100000],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","27700"]]"""

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdal.GetDriverByName('MrSID').Delete('tmp/mercator.sid')
    gdal.PopErrorHandler()

    shutil.copyfile('data/sid/mercator.sid', 'tmp/mercator.sid')

    ds = gdal.Open('tmp/mercator.sid')

    ds.SetGeoTransform(new_gt)
    ds.SetProjection(new_srs)
    ds.GetRasterBand(1).SetNoDataValue(255)
    ds = None

    ds = gdal.Open('tmp/mercator.sid')

    assert new_srs == ds.GetProjectionRef(), 'SRS Override failed.'

    assert new_gt == ds.GetGeoTransform(), 'Geotransform Override failed.'

    assert ds.GetRasterBand(1).GetNoDataValue() == 255, 'Nodata override failed.'

    ds = None

    gdal.GetDriverByName('MrSID').Delete('tmp/mercator.sid')

###############################################################################
# Test VSI*L IO with .sid


def test_mrsid_9():

    f = open('data/sid/mercator.sid', 'rb')
    data = f.read()
    f.close()

    f = gdal.VSIFOpenL('/vsimem/mrsid_9.sid', 'wb')
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('/vsimem/mrsid_9.sid')
    assert ds is not None
    ds = None

    gdal.Unlink('/vsimem/mrsid_9.sid')

###############################################################################
# Test VSI*L IO with .jp2


def test_mrsid_10():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    f = open('data/jpeg2000/int16.jp2', 'rb')
    data = f.read()
    f.close()

    f = gdal.VSIFOpenL('/vsimem/mrsid_10.jp2', 'wb')
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('/vsimem/mrsid_10.jp2')
    assert ds is not None
    ds = None

    gdal.Unlink('/vsimem/mrsid_10.jp2')

###############################################################################
# Check that we can use .j2w world files (#4651)


def test_mrsid_11():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    ds = gdal.Open('data/jpeg2000/byte_without_geotransform.jp2')

    geotransform = ds.GetGeoTransform()
    assert geotransform[0] == pytest.approx(440720, abs=0.1) and geotransform[1] == pytest.approx(60, abs=0.001) and geotransform[2] == pytest.approx(0, abs=0.001) and geotransform[3] == pytest.approx(3751320, abs=0.1) and geotransform[4] == pytest.approx(0, abs=0.001) and geotransform[5] == pytest.approx(-60, abs=0.001), \
        'geotransform differs from expected'

    ds = None

###############################################################################


def test_mrsid_online_1():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        pytest.skip()

    # Checksum = 29473 on my PC
    tst = gdaltest.GDALTest('JP2MrSID', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

###############################################################################


def test_mrsid_online_2():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        pytest.skip()

    # Checksum = 209 on my PC
    tst = gdaltest.GDALTest('JP2MrSID', 'tmp/cache/gcp.jp2', 1, None, filename_absolute=1)

    tst.testOpen()

    # The JP2MrSID driver doesn't handle GCPs
    ds = gdal.Open('tmp/cache/gcp.jp2')
    ds.GetRasterBand(1).Checksum()
    # if len(ds.GetGCPs()) != 15:
    #    gdaltest.post_reason('bad number of GCP')
    #    return 'fail'
    #
    # expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]"""
    # if ds.GetGCPProjection() != expected_wkt:
    #    gdaltest.post_reason('bad GCP projection')
    #    return 'fail'

    ds = None

###############################################################################


def test_mrsid_online_3():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        pytest.skip()

    # checksum = 14443 on my PC
    tst = gdaltest.GDALTest('JP2MrSID', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/Bretagne1.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne1.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    if maxdiff > 17:
        print(ds.GetRasterBand(1).Checksum())
        print(ds_ref.GetRasterBand(1).Checksum())

        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from reference')


###############################################################################


def test_mrsid_online_4():

    if gdaltest.jp2mrsid_drv is None:
        pytest.skip()

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        pytest.skip()

    # Checksum = 53186 on my PC
    tst = gdaltest.GDALTest('JP2MrSID', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/Bretagne2.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne2.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, width=256, height=256)

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    if maxdiff > 1:
        print(ds.GetRasterBand(1).Checksum())
        print(ds_ref.GetRasterBand(1).Checksum())
        pytest.fail('Image too different from reference')
