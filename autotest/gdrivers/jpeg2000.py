#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for Jasper/JP2ECW driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
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


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('JPEG2000')

gdaltest.buggy_jasper = None


def is_buggy_jasper():
    if gdaltest.buggy_jasper is not None:
        return gdaltest.buggy_jasper

    gdaltest.buggy_jasper = False
    if gdal.GetDriverByName('JPEG2000') is None:
        return False

    # This test will cause a crash with an unpatched version of Jasper, such as the one of Ubuntu 8.04 LTS
    # --> "jpc_dec.c:1072: jpc_dec_tiledecode: Assertion `dec->numcomps == 3' failed."
    # Recent Debian/Ubuntu have the appropriate patch.
    # So we try to run in a subprocess first
    import test_cli_utilities
    if test_cli_utilities.get_gdalinfo_path() is not None:
        ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --config GDAL_SKIP "JP2ECW JP2MRSID JP2KAK JP2LURA JP2OpenJPEG" data/jpeg2000/3_13bit_and_1bit.jp2')
        if ret.find('Band 1') == -1:
            gdaltest.post_reason('Jasper library would need patches')
            gdaltest.buggy_jasper = True
            return True

    return False

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.jpeg2000_drv = gdal.GetDriverByName('JPEG2000')
    assert gdaltest.jpeg2000_drv is not None

    gdaltest.deregister_all_jpeg2000_drivers_but('JPEG2000')

    yield

    gdaltest.reregister_all_jpeg2000_drivers()


###############################################################################
# Open byte.jp2


def test_jpeg2000_2():

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

    tst = gdaltest.GDALTest('JPEG2000', 'jpeg2000/byte.jp2', 1, 50054)
    return tst.testOpen(check_prj=srs, check_gt=gt)

###############################################################################
# Open int16.jp2


def test_jpeg2000_3():

    ds = gdal.Open('data/jpeg2000/int16.jp2')
    ds_ref = gdal.Open('data/int16.tif')

    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Quite a bit of difference...
    assert maxdiff <= 6, 'Image too different from reference'

###############################################################################
# Test copying byte.jp2


def test_jpeg2000_4():

    tst = gdaltest.GDALTest('JPEG2000', 'jpeg2000/byte.jp2', 1, 50054)
    tst.testCreateCopy()

    # This may fail for a good reason
    if tst.testCreateCopy(check_gt=1, check_srs=1) != 'success':
        gdaltest.post_reason('This is an expected failure if Jasper has not the jp2_encode_uuid function')
        return 'expected_fail'

    
###############################################################################
# Test copying int16.jp2


def test_jpeg2000_5():

    tst = gdaltest.GDALTest('JPEG2000', 'jpeg2000/int16.jp2', 1, None)
    return tst.testCreateCopy()

###############################################################################
# Test reading ll.jp2


def test_jpeg2000_6():

    tst = gdaltest.GDALTest('JPEG2000', 'jpeg2000/ll.jp2', 1, None)

    tst.testOpen()

    ds = gdal.Open('data/jpeg2000/ll.jp2')
    ds.GetRasterBand(1).Checksum()
    ds = None

###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)


def test_jpeg2000_7():

    tst = gdaltest.GDALTest('JPEG2000', '/vsigzip/data/jpeg2000/byte.jp2.gz', 1, 50054, filename_absolute=1)
    ret = tst.testOpen()
    gdal.Unlink('data/jpeg2000/byte.jp2.gz.properties')
    return ret

###############################################################################
# Test a JPEG2000 with the 3 bands having 13bit depth and the 4th one 1 bit


def test_jpeg2000_8():

    if is_buggy_jasper():
        pytest.skip()

    ds = gdal.Open('data/jpeg2000/3_13bit_and_1bit.jp2')

    expected_checksums = [64570, 57277, 56048, 61292]

    for i in range(4):
        assert ds.GetRasterBand(i + 1).Checksum() == expected_checksums[i], \
            ('unexpected checksum (%d) for band %d' % (expected_checksums[i], i + 1))

    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16, 'unexpected data type'

###############################################################################
# Check that we can use .j2w world files (#4651)


def test_jpeg2000_9():

    ds = gdal.Open('data/jpeg2000/byte_without_geotransform.jp2')

    geotransform = ds.GetGeoTransform()
    assert geotransform[0] == pytest.approx(440720, abs=0.1) and geotransform[1] == pytest.approx(60, abs=0.001) and geotransform[2] == pytest.approx(0, abs=0.001) and geotransform[3] == pytest.approx(3751320, abs=0.1) and geotransform[4] == pytest.approx(0, abs=0.001) and geotransform[5] == pytest.approx(-60, abs=0.001), \
        'geotransform differs from expected'

    ds = None

###############################################################################
# Check writing a file with more than 4 bands (#4686)


def test_jpeg2000_10():

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/jpeg2000_10_src.tif', 128, 128, 5)
    for i in range(src_ds.RasterCount):
        src_ds.GetRasterBand(i + 1).Fill(10 * i + 1)

    ds = gdaltest.jpeg2000_drv.CreateCopy('/vsimem/jpeg2000_10_dst.tif', src_ds)
    ds = None

    ds = gdal.Open('/vsimem/jpeg2000_10_dst.tif')
    assert ds is not None
    for i in range(src_ds.RasterCount):
        assert ds.GetRasterBand(i + 1).Checksum() == src_ds.GetRasterBand(i + 1).Checksum(), \
            ('bad checksum for band %d' % (i + 1))
    ds = None
    src_ds = None

    gdal.Unlink('/vsimem/jpeg2000_10_src.tif')
    gdal.Unlink('/vsimem/jpeg2000_10_dst.tif')

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit


def test_jpeg2000_11():

    if is_buggy_jasper():
        pytest.skip()

    ds = gdal.Open('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2')
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is None
    got_cs = fourth_band.Checksum()
    assert got_cs == 8527
    jp2_bands_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    jp2_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, int(ds.RasterXSize / 16), int(ds.RasterYSize / 16))

    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jpeg2000_11.tif', ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    # gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/jpeg2000_11.tif')
    assert got_cs == 8527

    assert jp2_bands_data == gtiff_bands_data

    assert jp2_fourth_band_data == gtiff_fourth_band_data

    ds = gdal.OpenEx('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'

###############################################################################


def test_jpeg2000_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        pytest.skip()

    # Checksum = 32669 on my PC
    tst = gdaltest.GDALTest('JPEG2000', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

###############################################################################


def test_jpeg2000_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        pytest.skip()

    # Checksum = 15621 on my PC
    tst = gdaltest.GDALTest('JPEG2000', 'tmp/cache/gcp.jp2', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/gcp.jp2')
    ds.GetRasterBand(1).Checksum()
    assert len(ds.GetGCPs()) == 15, 'bad number of GCP'

    expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]"""
    assert ds.GetGCPProjection() == expected_wkt, 'bad GCP projection'

    ds = None

###############################################################################


def test_jpeg2000_online_3():

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        pytest.skip()

    # Checksum = 14443 on my PC
    tst = gdaltest.GDALTest('JPEG2000', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/Bretagne1.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne1.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    assert maxdiff <= 17, 'Image too different from reference'

###############################################################################


def test_jpeg2000_online_4():

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        pytest.skip()

    tst = gdaltest.GDALTest('JPEG2000', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute=1)

    # Jasper cannot handle this image
    # Actually, a patched Jasper can ;-)
    if tst.testOpen() != 'success':
        gdaltest.post_reason('Expected failure: Jasper cannot handle this image yet')
        return 'expected_fail'

    ds = gdal.Open('tmp/cache/Bretagne2.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne2.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    assert maxdiff <= 17, 'Image too different from reference'

###############################################################################
# Try reading JPEG2000 with color table


def test_jpeg2000_online_5():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9.jp2', 'file9.jp2'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/file9.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    assert cs1 == 48954 and cs2 == 4939 and cs3 == 17734, \
        'Did not get expected checksums'

    ds = None

###############################################################################
# Try reading YCbCr JPEG2000 as RGB


def test_jpeg2000_online_6():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_03/file3.jp2', 'file3.jp2'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/file3.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    assert cs1 == 25337 and cs2 == 28262 and cs3 == 59580, \
        'Did not get expected checksums'

    ds = None
