#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JP2OpenJPEG driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
import sys
import shutil
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

import gdaltest

pytestmark = pytest.mark.require_driver('JP2OpenJPEG')

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.jp2openjpeg_drv = gdal.GetDriverByName('JP2OpenJPEG')
    assert gdaltest.jp2openjpeg_drv is not None

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

    yield

    gdaltest.reregister_all_jpeg2000_drivers()

###############################################################################
# Open byte.jp2


def test_jp2openjpeg_2():

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

    tst = gdaltest.GDALTest('JP2OpenJPEG', 'jpeg2000/byte.jp2', 1, 50054)
    return tst.testOpen(check_prj=srs, check_gt=gt)

###############################################################################
# Open int16.jp2


def test_jp2openjpeg_3():

    ds = gdal.Open('data/jpeg2000/int16.jp2')
    ds_ref = gdal.Open('data/int16.tif')

    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Quite a bit of difference...
    assert maxdiff <= 6, 'Image too different from reference'

    ds = ogr.Open('data/jpeg2000/int16.jp2')
    assert ds is None

###############################################################################
# Test copying byte.jp2


def test_jp2openjpeg_4(out_filename='tmp/jp2openjpeg_4.jp2'):

    src_ds = gdal.Open('data/jpeg2000/byte.jp2')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()

    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/jp2openjpeg_4.vrt', src_ds)
    vrt_ds.SetMetadataItem('TIFFTAG_XRESOLUTION', '300')
    vrt_ds.SetMetadataItem('TIFFTAG_YRESOLUTION', '200')
    vrt_ds.SetMetadataItem('TIFFTAG_RESOLUTIONUNIT', '3 (pixels/cm)')

    gdal.Unlink(out_filename)

    out_ds = gdal.GetDriverByName('JP2OpenJPEG').CreateCopy(out_filename, vrt_ds, options=['REVERSIBLE=YES', 'QUALITY=100'])
    del out_ds

    vrt_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_4.vrt')

    assert gdal.VSIStatL(out_filename + '.aux.xml') is None

    ds = gdal.Open(out_filename)
    cs = ds.GetRasterBand(1).Checksum()
    got_wkt = ds.GetProjectionRef()
    got_gt = ds.GetGeoTransform()
    xres = ds.GetMetadataItem('TIFFTAG_XRESOLUTION')
    yres = ds.GetMetadataItem('TIFFTAG_YRESOLUTION')
    resunit = ds.GetMetadataItem('TIFFTAG_RESOLUTIONUNIT')
    ds = None

    gdal.Unlink(out_filename)

    assert xres == '300' and yres == '200' and resunit == '3 (pixels/cm)', \
        'bad resolution'

    sr1 = osr.SpatialReference()
    sr1.SetFromUserInput(got_wkt)
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(src_wkt)

    if sr1.IsSame(sr2) == 0:
        print(got_wkt)
        print(src_wkt)
        pytest.fail('bad spatial reference')

    for i in range(6):
        assert got_gt[i] == pytest.approx(src_gt[i], abs=1e-8), 'bad geotransform'

    assert cs == 50054, 'bad checksum'


def test_jp2openjpeg_4_vsimem():
    return test_jp2openjpeg_4('/vsimem/jp2openjpeg_4.jp2')

###############################################################################
# Test copying int16.jp2


def test_jp2openjpeg_5():

    tst = gdaltest.GDALTest('JP2OpenJPEG', 'jpeg2000/int16.jp2', 1, None, options=['REVERSIBLE=YES', 'QUALITY=100', 'CODEC=J2K'])
    return tst.testCreateCopy()

###############################################################################
# Test reading ll.jp2


def test_jp2openjpeg_6():

    tst = gdaltest.GDALTest('JP2OpenJPEG', 'jpeg2000/ll.jp2', 1, None)
    tst.testOpen()

    ds = gdal.Open('data/jpeg2000/ll.jp2')
    ds.GetRasterBand(1).Checksum()
    ds = None

###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)


def test_jp2openjpeg_7():

    tst = gdaltest.GDALTest('JP2OpenJPEG', '/vsigzip/data/jpeg2000/byte.jp2.gz', 1, 50054, filename_absolute=1)
    ret = tst.testOpen()
    gdal.Unlink('data/jpeg2000/byte.jp2.gz.properties')
    return ret

###############################################################################
# Test a JP2OpenJPEG with the 3 bands having 13bit depth and the 4th one 1 bit


def test_jp2openjpeg_8():

    ds = gdal.Open('data/jpeg2000/3_13bit_and_1bit.jp2')

    expected_checksums = [64570, 57277, 56048, 61292]

    for i in range(4):
        assert ds.GetRasterBand(i + 1).Checksum() == expected_checksums[i], \
            ('unexpected checksum (%d) for band %d' % (expected_checksums[i], i + 1))

    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16, 'unexpected data type'

###############################################################################
# Check that we can use .j2w world files (#4651)


def test_jp2openjpeg_9():

    ds = gdal.Open('data/jpeg2000/byte_without_geotransform.jp2')

    geotransform = ds.GetGeoTransform()
    assert geotransform[0] == pytest.approx(440720, abs=0.1) and geotransform[1] == pytest.approx(60, abs=0.001) and geotransform[2] == pytest.approx(0, abs=0.001) and geotransform[3] == pytest.approx(3751320, abs=0.1) and geotransform[4] == pytest.approx(0, abs=0.001) and geotransform[5] == pytest.approx(-60, abs=0.001), \
        'geotransform differs from expected'

    ds = None

###############################################################################
# Test YCBCR420 creation option


def test_jp2openjpeg_10():

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_10.jp2', src_ds, options=['YCBCR420=YES', 'RESOLUTIONS=3'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    assert out_ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert out_ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert out_ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    del out_ds
    src_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_10.jp2')

    # Quite a bit of difference...
    assert maxdiff <= 12, 'Image too different from reference'

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit


def test_jp2openjpeg_11():

    ds = gdal.Open('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2')
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is None
    got_cs = fourth_band.Checksum()
    assert got_cs == 8527
    jp2_bands_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    jp2_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, int(ds.RasterXSize / 16), int(ds.RasterYSize / 16))

    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2openjpeg_11.tif', ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    # gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/jp2openjpeg_11.tif')
    assert got_cs == 8527

    assert jp2_bands_data == gtiff_bands_data

    assert jp2_fourth_band_data == gtiff_fourth_band_data

    ds = gdal.OpenEx('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'

###############################################################################
# Check that PAM overrides internal georeferencing (#5279)


def test_jp2openjpeg_12():

    # Override projection
    shutil.copy('data/jpeg2000/byte.jp2', 'tmp/jp2openjpeg_12.jp2')

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    wkt = ds.GetProjectionRef()
    ds = None

    gdaltest.jp2openjpeg_drv.Delete('tmp/jp2openjpeg_12.jp2')

    assert '32631' in wkt

    # Override geotransform
    shutil.copy('data/jpeg2000/byte.jp2', 'tmp/jp2openjpeg_12.jp2')

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    ds.SetGeoTransform([1000, 1, 0, 2000, 0, -1])
    ds = None

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    gt = ds.GetGeoTransform()
    ds = None

    gdaltest.jp2openjpeg_drv.Delete('tmp/jp2openjpeg_12.jp2')

    assert gt == (1000, 1, 0, 2000, 0, -1)

###############################################################################
# Check that PAM overrides internal GCPs (#5279)


def test_jp2openjpeg_13():

    # Create a dataset with GCPs
    src_ds = gdal.Open('data/rgb_gcp.vrt')
    ds = gdaltest.jp2openjpeg_drv.CreateCopy('tmp/jp2openjpeg_13.jp2', src_ds)
    ds = None
    src_ds = None

    assert gdal.VSIStatL('tmp/jp2openjpeg_13.jp2.aux.xml') is None

    ds = gdal.Open('tmp/jp2openjpeg_13.jp2')
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    assert count == 4
    assert len(gcps) == 4
    assert '4326' in wkt
    ds = None

    # Override GCP
    ds = gdal.Open('tmp/jp2openjpeg_13.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2openjpeg_13.jp2')
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    ds = None

    gdaltest.jp2openjpeg_drv.Delete('tmp/jp2openjpeg_13.jp2')

    assert count == 1
    assert len(gcps) == 1
    assert '32631' in wkt

###############################################################################
# Check that we get GCPs even there's no projection info


def test_jp2openjpeg_14():

    ds = gdal.Open('data/jpeg2000/byte_2gcps.jp2')
    assert ds.GetGCPCount() == 2

###############################################################################
# Test multi-threading reading and (possibly) writing

@pytest.mark.parametrize('JP2OPENJPEG_USE_THREADED_IO', ['YES', 'NO'])
def test_jp2openjpeg_15(JP2OPENJPEG_USE_THREADED_IO):

    src_ds = gdal.GetDriverByName('MEM').Create('', 256, 256)
    src_ds.GetRasterBand(1).Fill(255)
    data = src_ds.ReadRaster()
    # Setting only used for writing
    with gdaltest.config_option('JP2OPENJPEG_USE_THREADED_IO', JP2OPENJPEG_USE_THREADED_IO):
        ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_15.jp2', src_ds, options=['BLOCKXSIZE=33', 'BLOCKYSIZE=34'])
    src_ds = None
    got_data = ds.ReadRaster()
    ds = None
    gdaltest.jp2openjpeg_drv.Delete('/vsimem/jp2openjpeg_15.jp2')
    assert got_data == data

###############################################################################
# Test reading PixelIsPoint file (#5437)


def test_jp2openjpeg_16():

    ds = gdal.Open('data/jpeg2000/byte_point.jp2')
    gt = ds.GetGeoTransform()
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Point', \
        'did not get AREA_OR_POINT = Point'
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, 'did not get expected geotransform'

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', 'TRUE')

    ds = gdal.Open('data/jpeg2000/byte_point.jp2')
    gt = ds.GetGeoTransform()
    ds = None

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', None)

    gt_expected = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    assert gt == gt_expected, \
        'did not get expected geotransform with GTIFF_POINT_GEO_IGNORE TRUE'

###############################################################################
# Test writing PixelIsPoint file (#5437)


def test_jp2openjpeg_17():

    src_ds = gdal.Open('data/jpeg2000/byte_point.jp2')
    ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_17.jp2', src_ds)
    ds = None
    src_ds = None

    assert gdal.VSIStatL('/vsimem/jp2openjpeg_17.jp2.aux.xml') is None

    ds = gdal.Open('/vsimem/jp2openjpeg_17.jp2')
    gt = ds.GetGeoTransform()
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Point', \
        'did not get AREA_OR_POINT = Point'
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, 'did not get expected geotransform'

    gdal.Unlink('/vsimem/jp2openjpeg_17.jp2')

###############################################################################
# Test when using the decode_area API when one dimension of the dataset is not a
# multiple of 1024 (#5480)


def test_jp2openjpeg_18():

    src_ds = gdal.GetDriverByName('Mem').Create('', 2000, 2000)
    ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_18.jp2', src_ds, options=['BLOCKXSIZE=2000', 'BLOCKYSIZE=2000'])
    ds = None
    src_ds = None

    ds = gdal.Open('/vsimem/jp2openjpeg_18.jp2')
    ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    gdal.Unlink('/vsimem/jp2openjpeg_18.jp2')

###############################################################################
# Test reading file where GMLJP2 has nul character instead of \n (#5760)


def test_jp2openjpeg_19():

    ds = gdal.Open('data/jpeg2000/byte_gmljp2_with_nul_car.jp2')
    assert ds.GetProjectionRef() != ''
    ds = None

###############################################################################
# Validate GMLJP2 content against schema


def test_jp2openjpeg_20():

    xmlvalidate = pytest.importorskip('xmlvalidate')

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET.zip')
    except OSError:
        try:
            os.stat('../ogr/tmp/cache/SCHEMAS_OPENGIS_NET.zip')
            shutil.copy('../ogr/tmp/cache/SCHEMAS_OPENGIS_NET.zip', 'tmp/cache')
        except OSError:
            url = 'http://schemas.opengis.net/SCHEMAS_OPENGIS_NET.zip'
            if not gdaltest.download_file(url, 'SCHEMAS_OPENGIS_NET.zip', force_download=True, max_download_duration=20):
                pytest.skip('Cannot get SCHEMAS_OPENGIS_NET.zip')

    try:
        os.mkdir('tmp/cache/SCHEMAS_OPENGIS_NET')
    except OSError:
        pass

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/gml/3.1.1/profiles/gmlJP2Profile/1.0.0/gmlJP2Profile.xsd')
    except OSError:
        gdaltest.unzip('tmp/cache/SCHEMAS_OPENGIS_NET', 'tmp/cache/SCHEMAS_OPENGIS_NET.zip')

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xlink.xsd')
    except OSError:
        xlink_xsd_url = 'http://www.w3.org/1999/xlink.xsd'
        if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xlink.xsd', force_download=True, max_download_duration=10):
            xlink_xsd_url = 'http://even.rouault.free.fr/xlink.xsd'
            if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xlink.xsd', force_download=True, max_download_duration=10):
                pytest.skip('Cannot get xlink.xsd')

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xml.xsd')
    except OSError:
        xlink_xsd_url = 'http://www.w3.org/1999/xml.xsd'
        if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xml.xsd', force_download=True, max_download_duration=10):
            xlink_xsd_url = 'http://even.rouault.free.fr/xml.xsd'
            if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xml.xsd', force_download=True, max_download_duration=10):
                pytest.skip('Cannot get xml.xsd')

    xmlvalidate.transform_abs_links_to_ref_links('tmp/cache/SCHEMAS_OPENGIS_NET')

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_20.jp2', src_ds)
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_20.jp2')

    assert xmlvalidate.validate(gmljp2, ogc_schemas_location='tmp/cache/SCHEMAS_OPENGIS_NET')

###############################################################################
# Test YCC=NO creation option


def test_jp2openjpeg_21():

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_21.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES', 'YCC=NO'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_21.jp2')

    # Quite a bit of difference...
    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# Test RGBA support


def test_jp2openjpeg_22():

    # RGBA
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

    # RGBA with 1BIT_ALPHA=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options=['1BIT_ALPHA=YES'])
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is None
    ds = gdal.OpenEx('/vsimem/jp2openjpeg_22.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'
    ds = None
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
    assert ds.GetRasterBand(4).Checksum() == 23120
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    # RGBA with YCBCR420=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options=['YCBCR420=YES'])
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(1).Checksum() in [11457, 11450, 11498]
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    # RGBA with YCC=YES. Will emit a warning for now because of OpenJPEG
    # bug (only fixed in trunk, not released versions at that time)
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options=['YCC=YES', 'QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is None
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

    # RGB,undefined
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

    # RGB,undefined with ALPHA=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES', 'ALPHA=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

###############################################################################
# Test NBITS support


def test_jp2openjpeg_23():

    src_ds = gdal.Open('../gcore/data/uint16.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_23.jp2', src_ds, options=['NBITS=9', 'QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2openjpeg_23.jp2')
    assert ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '9'

    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_23_2.jp2', ds)
    assert out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '9'
    del out_ds

    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_23.jp2.aux.xml') is None
    gdal.Unlink('/vsimem/jp2openjpeg_23.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_23_2.jp2')

    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# Test Grey+alpha support


def test_jp2openjpeg_24():

    #  Grey+alpha
    src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_24.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_24.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_24.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_24.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

    #  Grey+alpha with 1BIT_ALPHA=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_24.jp2', src_ds, options=['1BIT_ALPHA=YES'])
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_24.jp2.aux.xml') is None
    ds = gdal.OpenEx('/vsimem/jp2openjpeg_24.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'
    ds = None
    ds = gdal.Open('/vsimem/jp2openjpeg_24.jp2')
    assert ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is None
    assert ds.GetRasterBand(2).Checksum() == 23120
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_24.jp2')

###############################################################################
# Test multiband support


def test_jp2openjpeg_25():

    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 5)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(250)
    src_ds.GetRasterBand(3).Fill(245)
    src_ds.GetRasterBand(4).Fill(240)
    src_ds.GetRasterBand(5).Fill(235)

    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_25.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2openjpeg_25.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_Undefined
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_25.jp2.aux.xml') is None

    gdal.Unlink('/vsimem/jp2openjpeg_25.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

###############################################################################


def validate(filename, expected_gmljp2=True, return_error_count=False, oidoc=None, inspire_tg=True):

    for path in ('../ogr', '../../gdal/swig/python/samples'):
        if path not in sys.path:
            sys.path.append(path)

    validate_jp2 = pytest.importorskip('validate_jp2')

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET')
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xlink.xsd')
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xml.xsd')
        ogc_schemas_location = 'tmp/cache/SCHEMAS_OPENGIS_NET'
    except OSError:
        ogc_schemas_location = 'disabled'

    if ogc_schemas_location != 'disabled':
        try:
            import xmlvalidate
            xmlvalidate.validate  # to make pyflakes happy
        except (ImportError, AttributeError):
            ogc_schemas_location = 'disabled'

    res = validate_jp2.validate(filename, oidoc, inspire_tg, expected_gmljp2, ogc_schemas_location)
    if return_error_count:
        return (res.error_count, res.warning_count)
    if res.error_count == 0 and res.warning_count == 0:
        return
    pytest.fail()

###############################################################################
# Test INSPIRE_TG support


def test_jp2openjpeg_26():

    src_ds = gdal.GetDriverByName('MEM').Create('', 2048, 2048, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000, 1, 0, 5000000, 0, -1])

    # Nominal case: tiled
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES'])
    overview_count = out_ds.GetRasterBand(1).GetOverviewCount()
    # We have 2x2 1024x1024 tiles. Each of them can be reconstructed down to 128x128.
    # So for full raster the smallest overview is 2*128
    assert (out_ds.GetRasterBand(1).GetOverview(overview_count - 1).XSize == 2 * 128 and \
       out_ds.GetRasterBand(1).GetOverview(overview_count - 1).YSize == 2 * 128)
    out_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is None
    assert validate('/vsimem/jp2openjpeg_26.jp2') != 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Nominal case: untiled
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'BLOCKXSIZE=2048', 'BLOCKYSIZE=2048'])
    overview_count = out_ds.GetRasterBand(1).GetOverviewCount()
    assert (out_ds.GetRasterBand(1).GetOverview(overview_count - 1).XSize == 128 and \
       out_ds.GetRasterBand(1).GetOverview(overview_count - 1).YSize == 128)
    gdal.ErrorReset()
    out_ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ''
    out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert gdal.GetLastErrorMsg() == ''
    out_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is None
    assert validate('/vsimem/jp2openjpeg_26.jp2') != 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Nominal case: RGBA
    src_ds = gdal.GetDriverByName('MEM').Create('', 128, 128, 4)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000, 1, 0, 5000000, 0, -1])
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'ALPHA=YES'])
    out_ds = None
    ds = gdal.OpenEx('/vsimem/jp2openjpeg_26.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(4).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is None
    assert validate('/vsimem/jp2openjpeg_26.jp2') != 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Warning case: disabling JPX
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'JPX=NO'])
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    out_ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is None
    res = validate('/vsimem/jp2openjpeg_26.jp2', return_error_count=True)
    assert res == 'skip' or res == (2, 0)
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Bilevel (1 bit)
    src_ds = gdal.GetDriverByName('MEM').Create('', 128, 128, 1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES'])
    assert out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is None
    assert validate('/vsimem/jp2openjpeg_26.jp2', expected_gmljp2=False) != 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Auto-promotion 12->16 bits
    src_ds = gdal.GetDriverByName('MEM').Create('', 128, 128, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '12', 'IMAGE_STRUCTURE')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES'])
    assert out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is None
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is None
    assert validate('/vsimem/jp2openjpeg_26.jp2', expected_gmljp2=False) != 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    src_ds = gdal.GetDriverByName('MEM').Create('', 2048, 2048, 1)

    # Error case: too big tile
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'BLOCKXSIZE=1536', 'BLOCKYSIZE=1536'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Error case: non square tile
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'BLOCKXSIZE=512', 'BLOCKYSIZE=128'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Error case: incompatible PROFILE
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'PROFILE=UNRESTRICTED'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Error case: valid, but too small number of resolutions regarding PROFILE_1
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'RESOLUTIONS=1'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Too big resolution number. Will fallback to default one
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'RESOLUTIONS=100'])
    gdal.PopErrorHandler()
    assert out_ds is not None
    out_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Error case: unsupported NBITS
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'NBITS=2'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Error case: unsupported CODEC (J2K)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.j2k', src_ds, options=['INSPIRE_TG=YES'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Error case: invalid CODEBLOCK_WIDTH/HEIGHT
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'CODEBLOCK_WIDTH=128', 'CODEBLOCK_HEIGHT=32'])
    gdal.PopErrorHandler()
    assert out_ds is None
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options=['INSPIRE_TG=YES', 'CODEBLOCK_WIDTH=32', 'CODEBLOCK_HEIGHT=128'])
    gdal.PopErrorHandler()
    assert out_ds is None

###############################################################################
# Test CreateCopy() from a JPEG2000 with a 2048x2048 tiling


def test_jp2openjpeg_27():

    # Test optimization in GDALCopyWholeRasterGetSwathSize()
    # Not sure how we can check that except looking at logs with CPL_DEBUG=GDAL
    # for "GDAL: GDALDatasetCopyWholeRaster(): 2048*2048 swaths, bInterleave=1"
    src_ds = gdal.GetDriverByName('MEM').Create('', 2049, 2049, 4)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_27.jp2', src_ds, options=['RESOLUTIONS=1', 'BLOCKXSIZE=2048', 'BLOCKYSIZE=2048'])
    src_ds = None
    # print('End of JP2 decoding')
    out2_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2openjpeg_27.tif', out_ds, options=['TILED=YES'])
    out_ds = None
    del out2_ds
    gdal.Unlink('/vsimem/jp2openjpeg_27.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_27.tif')

###############################################################################
# Test CODEBLOCK_WIDTH/_HEIGHT


XML_TYPE_IDX = 0
XML_VALUE_IDX = 1
XML_FIRST_CHILD_IDX = 2


def find_xml_node(ar, element_name, only_attributes=False):
    # type = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if value == element_name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        if only_attributes and child[XML_TYPE_IDX] != gdal.CXT_Attribute:
            continue
        found = find_xml_node(child, element_name)
        if found is not None:
            return found
    return None


def get_attribute_val(ar, attr_name):
    node = find_xml_node(ar, attr_name, True)
    if node is None or node[XML_TYPE_IDX] != gdal.CXT_Attribute:
        return None
    if len(ar) > XML_FIRST_CHILD_IDX and \
            node[XML_FIRST_CHILD_IDX][XML_TYPE_IDX] == gdal.CXT_Text:
        return node[XML_FIRST_CHILD_IDX][XML_VALUE_IDX]
    return None


def find_element_with_name(ar, element_name, name):
    typ = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if typ == gdal.CXT_Element and value == element_name and get_attribute_val(ar, 'name') == name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        found = find_element_with_name(child, element_name, name)
        if found:
            return found
    return None


def get_element_val(node):
    if node is None:
        return None
    for child_idx in range(XML_FIRST_CHILD_IDX, len(node)):
        child = node[child_idx]
        if child[XML_TYPE_IDX] == gdal.CXT_Text:
            return child[XML_VALUE_IDX]
    return None


def jp2openjpeg_test_codeblock(filename, codeblock_width, codeblock_height):
    node = gdal.GetJPEG2000Structure(filename, ['ALL=YES'])
    xcb = 2**(2 + int(get_element_val(find_element_with_name(node, "Field", "SPcod_xcb_minus_2"))))
    ycb = 2**(2 + int(get_element_val(find_element_with_name(node, "Field", "SPcod_ycb_minus_2"))))
    if xcb != codeblock_width or ycb != codeblock_height:
        return False
    return True


def test_jp2openjpeg_28():

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)

    tests = [(['CODEBLOCK_WIDTH=2'], 64, 64, True),
             (['CODEBLOCK_WIDTH=2048'], 64, 64, True),
             (['CODEBLOCK_HEIGHT=2'], 64, 64, True),
             (['CODEBLOCK_HEIGHT=2048'], 64, 64, True),
             (['CODEBLOCK_WIDTH=128', 'CODEBLOCK_HEIGHT=128'], 64, 64, True),
             (['CODEBLOCK_WIDTH=63'], 32, 64, True),
             (['CODEBLOCK_WIDTH=32', 'CODEBLOCK_HEIGHT=32'], 32, 32, False),
            ]

    for (options, expected_cbkw, expected_cbkh, warning_expected) in tests:
        gdal.ErrorReset()
        gdal.PushErrorHandler()
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_28.jp2', src_ds, options=options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            print(options)
            pytest.fail('warning expected')
        del out_ds
        if not jp2openjpeg_test_codeblock('/vsimem/jp2openjpeg_28.jp2', expected_cbkw, expected_cbkh):
            print(options)
            pytest.fail('unexpected codeblock size')

    gdal.Unlink('/vsimem/jp2openjpeg_28.jp2')

###############################################################################
# Test TILEPARTS option


def test_jp2openjpeg_29():

    src_ds = gdal.GetDriverByName('MEM').Create('', 128, 128, 1)

    tests = [(['TILEPARTS=DISABLED'], False),
             (['TILEPARTS=RESOLUTIONS'], False),
             (['TILEPARTS=LAYERS'], True),  # warning since there's only one quality layer
             (['TILEPARTS=LAYERS', 'QUALITY=1,2'], False),
             (['TILEPARTS=COMPONENTS'], False),
             (['TILEPARTS=ILLEGAL'], True),
            ]

    for (options, warning_expected) in tests:
        gdal.ErrorReset()
        gdal.PushErrorHandler()
        options.append('BLOCKXSIZE=64')
        options.append('BLOCKYSIZE=64')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_29.jp2', src_ds, options=options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            print(options)
            pytest.fail('warning expected')
        # Not sure if that could be easily checked
        del out_ds
        # print gdal.GetJPEG2000StructureAsString('/vsimem/jp2openjpeg_29.jp2', ['ALL=YES'])

    gdal.Unlink('/vsimem/jp2openjpeg_29.jp2')

###############################################################################
# Test color table support


def test_jp2openjpeg_30():

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))
    src_ds.GetRasterBand(1).SetRasterColorTable(ct)

    tests = [([], False),
             (['QUALITY=100', 'REVERSIBLE=YES'], False),
             (['QUALITY=50'], True),
             (['REVERSIBLE=NO'], True),
            ]

    for (options, warning_expected) in tests:
        gdal.ErrorReset()
        gdal.PushErrorHandler()
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds, options=options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            print(options)
            pytest.fail('warning expected')
        ct = out_ds.GetRasterBand(1).GetRasterColorTable()
        assert (ct.GetCount() == 4 and \
           ct.GetColorEntry(0) == (255, 255, 255, 255) and \
           ct.GetColorEntry(1) == (255, 255, 0, 255) and \
           ct.GetColorEntry(2) == (255, 0, 255, 255) and \
           ct.GetColorEntry(3) == (0, 255, 255, 255)), 'Wrong color table entry.'
        del out_ds

        assert validate('/vsimem/jp2openjpeg_30.jp2', expected_gmljp2=False) != 'fail'

    gdal.Unlink('/vsimem/jp2openjpeg_30.jp2')

    # Test with c4 != 255
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (0, 0, 0, 0))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))
    src_ds.GetRasterBand(1).SetRasterColorTable(ct)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds)
    ct = out_ds.GetRasterBand(1).GetRasterColorTable()
    assert (ct.GetCount() == 4 and \
            ct.GetColorEntry(0) == (0, 0, 0, 0) and \
            ct.GetColorEntry(1) == (255, 255, 0, 255) and \
            ct.GetColorEntry(2) == (255, 0, 255, 255) and \
            ct.GetColorEntry(3) == (0, 255, 255, 255)), 'Wrong color table entry.'
    del out_ds
    gdal.Unlink('/vsimem/jp2openjpeg_30.jp2')

    # Same but with CT_COMPONENTS=3
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (0, 0, 0, 0))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))
    src_ds.GetRasterBand(1).SetRasterColorTable(ct)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds, options=['CT_COMPONENTS=3'])
    ct = out_ds.GetRasterBand(1).GetRasterColorTable()
    assert (ct.GetCount() == 4 and \
            ct.GetColorEntry(0) == (0, 0, 0, 255) and \
            ct.GetColorEntry(1) == (255, 255, 0, 255) and \
            ct.GetColorEntry(2) == (255, 0, 255, 255) and \
            ct.GetColorEntry(3) == (0, 255, 255, 255)), 'Wrong color table entry.'
    del out_ds
    gdal.Unlink('/vsimem/jp2openjpeg_30.jp2')

    # Not supported: color table on first band, and other bands
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 2)
    ct = gdal.ColorTable()
    src_ds.GetRasterBand(1).SetRasterColorTable(ct)
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds)
    gdal.PopErrorHandler()
    assert out_ds is None

###############################################################################
# Test unusual band color interpretation order


def test_jp2openjpeg_31():

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 3)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_31.jp2', src_ds)
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_31.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_31.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_31.jp2')

    # With alpha now
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 4)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_31.jp2', src_ds)
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_31.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_31.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_31.jp2')

###############################################################################
# Test creation of "XLBoxes" for JP2C


def test_jp2openjpeg_32():

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_32.jp2', src_ds, options=['JP2C_XLBOX=YES'])
    gdal.PopErrorHandler()
    assert out_ds.GetRasterBand(1).Checksum() == 0
    out_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_32.jp2')

###############################################################################
# Test crazy tile size


def test_jp2openjpeg_33():

    src_ds = gdal.Open("""<VRTDataset rasterXSize="100000" rasterYSize="100000">
  <VRTRasterBand dataType="Byte" band="1">
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler()
    # Limit number of resolutions, because of
    # https://github.com/uclouvain/openjpeg/issues/493
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_33.jp2', src_ds, options=['BLOCKXSIZE=100000', 'BLOCKYSIZE=100000', 'RESOLUTIONS=5'])
    gdal.PopErrorHandler()
    assert out_ds is None
    out_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_33.jp2')

###############################################################################
# Test opening a file whose dimensions are > 2^31-1


def test_jp2openjpeg_34():

    gdal.PushErrorHandler()
    ds = gdal.Open('data/jpeg2000/dimensions_above_31bit.jp2')
    gdal.PopErrorHandler()
    assert ds is None


###############################################################################
# Test opening a truncated file

def test_jp2openjpeg_35():

    gdal.PushErrorHandler()
    ds = gdal.Open('data/jpeg2000/truncated.jp2')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Test we cannot create files with more than 16384 bands


def test_jp2openjpeg_36():

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 16385)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_36.jp2', src_ds)
    gdal.PopErrorHandler()
    assert out_ds is None and gdal.VSIStatL('/vsimem/jp2openjpeg_36.jp2') is None

###############################################################################
# Test metadata reading & writing


def test_jp2openjpeg_37():

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options=['WRITE_METADATA=YES'])
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
    assert ds.GetMetadata() == {}
    gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Simple metadata in main domain
    for options in [['WRITE_METADATA=YES'], ['WRITE_METADATA=YES', 'INSPIRE_TG=YES']]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadataItem('FOO', 'BAR')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options=options)
        del out_ds
        assert gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
        assert ds.GetMetadata() == {'FOO': 'BAR'}
        ds = None

        assert not ('INSPIRE_TG=YES' in options and validate('/vsimem/jp2openjpeg_37.jp2', expected_gmljp2=False) == 'fail')

        gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Simple metadata in auxiliary domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadataItem('FOO', 'BAR', 'SOME_DOMAIN')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options=['WRITE_METADATA=YES'])
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
    md = ds.GetMetadata('SOME_DOMAIN')
    assert md == {'FOO': 'BAR'}
    gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Simple metadata in auxiliary XML domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadata(['<some_arbitrary_xml_box/>'], 'xml:SOME_DOMAIN')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options=['WRITE_METADATA=YES'])
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
    assert ds.GetMetadata('xml:SOME_DOMAIN')[0] == '<some_arbitrary_xml_box />\n'
    gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Special xml:BOX_ metadata domain
    for options in [['WRITE_METADATA=YES'], ['WRITE_METADATA=YES', 'INSPIRE_TG=YES']]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata(['<some_arbitrary_xml_box/>'], 'xml:BOX_1')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options=options)
        del out_ds
        assert gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
        assert ds.GetMetadata('xml:BOX_0')[0] == '<some_arbitrary_xml_box/>'
        gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Special xml:XMP metadata domain
    for options in [['WRITE_METADATA=YES'], ['WRITE_METADATA=YES', 'INSPIRE_TG=YES']]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata(['<fake_xmp_box/>'], 'xml:XMP')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options=options)
        del out_ds
        assert gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
        assert ds.GetMetadata('xml:XMP')[0] == '<fake_xmp_box/>'
        ds = None

        assert not ('INSPIRE_TG=YES' in options and validate('/vsimem/jp2openjpeg_37.jp2', expected_gmljp2=False) == 'fail')

        gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Special xml:IPR metadata domain
    for options in [['WRITE_METADATA=YES']]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata(['<fake_ipr_box/>'], 'xml:IPR')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options=options)
        del out_ds
        assert gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
        assert ds.GetMetadata('xml:IPR')[0] == '<fake_ipr_box/>'
        ds = None

        assert validate('/vsimem/jp2openjpeg_37.jp2', expected_gmljp2=False) != 'fail'
        gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    
###############################################################################
# Test non-EPSG SRS (so written with a GML dictionary)


def test_jp2openjpeg_38():

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    wkt = """PROJCS["UTM Zone 31, Northern Hemisphere",GEOGCS["unnamed ellipse",DATUM["unknown",SPHEROID["unnamed",100,1]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"""
    src_ds.SetProjection(wkt)
    src_ds.SetGeoTransform([0, 60, 0, 0, 0, -60])
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_38.jp2', src_ds, options=['GeoJP2=NO'])
    assert out_ds.GetProjectionRef() == wkt
    crsdictionary = out_ds.GetMetadata_List("xml:CRSDictionary.gml")[0]
    out_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_38.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_38.jp2.aux.xml')

    do_validate = False
    try:
        import xmlvalidate
        do_validate = True
    except ImportError:
        print('Cannot import xmlvalidate')

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET')
    except OSError:
        do_validate = False

    if do_validate:
        assert xmlvalidate.validate(crsdictionary, ogc_schemas_location='tmp/cache/SCHEMAS_OPENGIS_NET')

    
###############################################################################
# Test GMLJP2OVERRIDE configuration option and DGIWG GMLJP2


def test_jp2openjpeg_39():

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    src_ds.SetGeoTransform([0, 60, 0, 0, 0, -60])
    gdal.SetConfigOption('GMLJP2OVERRIDE', '/vsimem/override.gml')
    # This GML has srsName only on RectifiedGrid (taken from D.2.2.2 from DGIWG_Profile_of_JPEG2000_for_Georeferenced_Imagery.pdf)
    gdal.FileFromMemBuffer('/vsimem/override.gml', """<?xml version="1.0" encoding="UTF-8"?>
<gml:FeatureCollection xmlns:gml="http://www.opengis.net/gml"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xmlns:gmd="http://www.isotc211.org/2005/gmd"
                       xmlns:gco="http://www.isotc211.org/2005/gco"
                       xsi:schemaLocation="http://www.opengis.net/gml file:///D:/dgiwg/jp2/GML-3.1.1/profiles/DGIWGgmlJP2Profile/1.1.0/DGIWGgmlJP2Profile.xsd">
  <gml:featureMember>
    <gml:FeatureCollection>
      <!-- feature collection for a specific codestream -->
      <gml:featureMember>
        <gml:RectifiedGridCoverage>
          <gml:rectifiedGridDomain>
            <gml:RectifiedGrid dimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
              <gml:limits>
                <gml:GridEnvelope>
                  <!-- Image coordinates -->
                  <gml:low>0 0</gml:low>
                  <gml:high>4999 9999</gml:high>
                </gml:GridEnvelope>
              </gml:limits>
              <gml:axisName>X</gml:axisName>
              <gml:axisName>Y</gml:axisName>
              <!-- The origin location in geo coordinates -->
              <gml:origin>
                <gml:Point>
                  <gml:pos>19.1234567 37.1234567</gml:pos>
                </gml:Point>
              </gml:origin>
              <!--offsetVectors says how much offset each pixel will contribute to, in practice, that is the cell size -->
              <gml:offsetVector>0.0 0.00001234</gml:offsetVector>
              <gml:offsetVector> -0.00001234 0.0</gml:offsetVector>
            </gml:RectifiedGrid>
          </gml:rectifiedGridDomain>
          <!--A RectifiedGridCoverage uses the rangeSet to describe the data below is a description of the range of values described by the grid coverage -->
          <gml:rangeSet>
            <gml:File>
              <gml:rangeParameters/>
              <gml:fileName>gmljp2://codestream/0</gml:fileName>
              <gml:fileStructure>Record Interleaved</gml:fileStructure>
            </gml:File>
          </gml:rangeSet>
        </gml:RectifiedGridCoverage>
      </gml:featureMember>
    </gml:FeatureCollection>
  </gml:featureMember>
</gml:FeatureCollection>""")
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_39.jp2', src_ds, options=['GeoJP2=NO'])
    gdal.SetConfigOption('GMLJP2OVERRIDE', None)
    gdal.Unlink('/vsimem/override.gml')
    del out_ds
    ds = gdal.Open('/vsimem/jp2openjpeg_39.jp2')
    assert ds.GetProjectionRef().find('4326') >= 0
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_39.jp2')

###############################################################################
# Test we can parse GMLJP2 v2.0


def test_jp2openjpeg_40():

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    src_ds.SetGeoTransform([0, 60, 0, 0, 0, -60])
    gdal.SetConfigOption('GMLJP2OVERRIDE', '/vsimem/override.gml')

    gdal.FileFromMemBuffer('/vsimem/override.gml', """<?xml version="1.0" encoding="UTF-8"?>
<?xml version="1.0" encoding="UTF-8"?>
<gmljp2:GMLJP2CoverageCollection gml:id="JPEG2000_0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:gmlcov="http://www.opengis.net/gmlcov/1.0"
    xmlns:gmljp2="http://www.opengis.net/gmljp2/2.0"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:schemaLocation="http://www.opengis.net/gmljp2/2.0 http://schemas.opengis.net/gmljp2/2.0/gmljp2.xsd">
    <gml:gridDomain/>
    <gml:rangeSet>
        <gml:File>
            <gml:rangeParameters/>
            <gml:fileName>gmljp2://codestream</gml:fileName>
            <gml:fileStructure>inapplicable</gml:fileStructure>
        </gml:File>
    </gml:rangeSet>
    <gmlcov:rangeType/>
    <gmljp2:featureMember>
        <gmljp2:GMLJP2RectifiedGridCoverage gml:id="CodeStream">
            <gml:domainSet>
                <gml:RectifiedGrid gml:id="rg0001" dimension="2"
                            srsName="http://www.opengis.net/def/crs/EPSG/0/4326">
                    <gml:limits>
                        <gml:GridEnvelope>
                            <gml:low>0 0</gml:low>
                            <gml:high>19 19</gml:high>
                        </gml:GridEnvelope>
                    </gml:limits>
                    <gml:axisLabels>Lat Long</gml:axisLabels>
                    <gml:origin>
                        <gml:Point gml:id="P0001" srsName="http://www.opengis.net/def/crs/EPSG/0/4326">
                            <gml:pos>48.95 2.05</gml:pos>
                        </gml:Point>
                    </gml:origin>
                    <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/4326">0 0.1</gml:offsetVector>
                    <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/4326">-0.1 0</gml:offsetVector>
                </gml:RectifiedGrid>
            </gml:domainSet>
            <gml:rangeSet>
                <gml:File>
                    <gml:rangeParameters/>
                    <gml:fileName>gmljp2://codestream</gml:fileName>
                    <gml:fileStructure>inapplicable</gml:fileStructure>
                </gml:File>
            </gml:rangeSet>
            <gmlcov:rangeType/>
        </gmljp2:GMLJP2RectifiedGridCoverage>
    </gmljp2:featureMember>
</gmljp2:GMLJP2CoverageCollection>""")
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_40.jp2', src_ds, options=['GeoJP2=NO'])
    gdal.SetConfigOption('GMLJP2OVERRIDE', None)
    gdal.Unlink('/vsimem/override.gml')
    del out_ds
    ds = gdal.Open('/vsimem/jp2openjpeg_40.jp2')
    assert ds.GetProjectionRef().find('4326') >= 0
    got_gt = ds.GetGeoTransform()
    expected_gt = (2, 0.1, 0, 49, 0, -0.1)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-5)
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_40.jp2')

###############################################################################
# Test USE_SRC_CODESTREAM=YES


def test_jp2openjpeg_41():

    src_ds = gdal.Open('data/jpeg2000/byte.jp2')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_41.jp2', src_ds,
                                                 options=['USE_SRC_CODESTREAM=YES', 'PROFILE=PROFILE_1', 'GEOJP2=NO', 'GMLJP2=NO'])
    assert src_ds.GetRasterBand(1).Checksum() == out_ds.GetRasterBand(1).Checksum()
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_41.jp2').size == 9923
    gdal.Unlink('/vsimem/jp2openjpeg_41.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_41.jp2.aux.xml')

    # Warning if ignored option
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_41.jp2', src_ds,
                                                 options=['USE_SRC_CODESTREAM=YES', 'QUALITY=1'])
    gdal.PopErrorHandler()
    del out_ds
    assert gdal.GetLastErrorMsg() != ''
    gdal.Unlink('/vsimem/jp2openjpeg_41.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_41.jp2.aux.xml')

    # Warning if source is not JPEG2000
    src_ds = gdal.Open('data/byte.tif')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_41.jp2', src_ds,
                                                 options=['USE_SRC_CODESTREAM=YES'])
    gdal.PopErrorHandler()
    del out_ds
    assert gdal.GetLastErrorMsg() != ''
    gdal.Unlink('/vsimem/jp2openjpeg_41.jp2')

###############################################################################
# Test update of existing file


def test_jp2openjpeg_42():

    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_42.jp2', src_ds, options=['JP2C_LENGTH_ZERO=YES'])
    gdal.PopErrorHandler()
    del out_ds

    # Nothing to rewrite
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    del ds

    # Add metadata: will be written after codestream since there's no other georef or metadata box before codestream
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    ds.SetMetadataItem('FOO', 'BAR')
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Add metadata and GCP
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetMetadata() == {'FOO': 'BAR'}
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check we got metadata and GCP, and there's no GMLJP2 box
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetMetadata() == {'FOO': 'BAR'}
    assert ds.GetGCPCount() == 1
    assert len(ds.GetMetadataDomainList()) == 2
    # Unset metadata and GCP
    ds.SetMetadata(None)
    ds.SetGCPs([], '')
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check we have no longer metadata or GCP
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetMetadata() == {}
    assert ds.GetGCPCount() == 0
    assert ds.GetMetadataDomainList() == ['DERIVED_SUBDATASETS']
    # Add projection and geotransform
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([0, 1, 2, 3, 4, 5])
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check them
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetProjectionRef().find('32631') >= 0
    assert ds.GetGeoTransform() == (0, 1, 2, 3, 4, 5)
    # Check that we have a GMLJP2 box
    assert ds.GetMetadataDomainList() == ['xml:gml.root-instance', 'DERIVED_SUBDATASETS']
    # Remove projection and geotransform
    ds.SetProjection('')
    ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check we have no longer anything
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetProjectionRef() == ''
    assert ds.GetGeoTransform() == (0, 1, 0, 0, 0, 1)
    assert ds.GetMetadataDomainList() == ['DERIVED_SUBDATASETS']
    ds = None

    # Create file with georef boxes before codestream, and disable GMLJP2
    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([0, 1, 2, 3, 4, 5])
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_42.jp2', src_ds, options=['GMLJP2=NO'])
    del out_ds

    # Modify geotransform
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    ds.SetGeoTransform([1, 2, 3, 4, 5, 6])
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check it and that we don't have GMLJP2
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetGeoTransform() == (1, 2, 3, 4, 5, 6)
    assert ds.GetMetadataDomainList() == ['DERIVED_SUBDATASETS']
    ds = None

    # Create file with georef boxes before codestream, and disable GeoJP2
    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([2, 3, 0, 4, 0, -5])
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_42.jp2', src_ds, options=['GeoJP2=NO'])
    del out_ds

    # Modify geotransform
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetGeoTransform() == (2, 3, 0, 4, 0, -5)
    ds.SetGeoTransform([1, 2, 0, 3, 0, -4])
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check it
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetGeoTransform() == (1, 2, 0, 3, 0, -4)
    assert ds.GetMetadataDomainList() == ['xml:gml.root-instance', 'DERIVED_SUBDATASETS']
    # Add GCPs
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check it (a GeoJP2 box has been added and GMLJP2 removed)
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetGCPs()
    assert ds.GetMetadataDomainList() == ['DERIVED_SUBDATASETS']
    # Add IPR box
    ds.SetMetadata(['<fake_ipr_box/>'], 'xml:IPR')
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_42.jp2.aux.xml') is None

    # Check it
    ds = gdal.Open('/vsimem/jp2openjpeg_42.jp2', gdal.GA_Update)
    assert ds.GetMetadata('xml:IPR')[0] == '<fake_ipr_box/>'
    ds = None

    gdal.Unlink('/vsimem/jp2openjpeg_42.jp2')

###############################################################################
# Get structure of a JPEG2000 file


def test_jp2openjpeg_43():

    ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte.jp2', ['ALL=YES'])
    assert ret is not None

    ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte_tlm_plt.jp2', ['ALL=YES'])
    assert ret is not None

    ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte_one_poc.j2k', ['ALL=YES'])
    assert ret is not None

###############################################################################
# Check a file against a OrthoimageryCoverage document


def test_jp2openjpeg_44():

    src_ds = gdal.Open('data/utm.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_44.jp2', src_ds, options=['INSPIRE_TG=YES'])
    del out_ds
    ret = validate('/vsimem/jp2openjpeg_44.jp2', oidoc='data/jpeg2000/utm_inspire_tg_oi.xml')
    gdal.Unlink('/vsimem/jp2openjpeg_44.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_44.jp2.aux.xml')

    return ret

###############################################################################
# Test GMLJP2v2


def test_jp2openjpeg_45():

    with gdaltest.error_handler():
        if ogr.Open('../ogr/data/gml/ionic_wfs.gml') is None:
            pytest.skip('GML read support missing')

    with gdaltest.error_handler():
        if ogr.Open('../ogr/data/kml/empty.kml') is None:
            pytest.skip('KML support missing')

    # Test GMLJP2V2_DEF=YES
    src_ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=YES'])
    assert out_ds.GetLayerCount() == 0
    assert out_ds.GetLayer(0) is None
    del out_ds

    ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    minimal_instance = """<gmljp2:GMLJP2CoverageCollection gml:id="ID_GMLJP2_0"
     xmlns:gml="http://www.opengis.net/gml/3.2"
     xmlns:gmlcov="http://www.opengis.net/gmlcov/1.0"
     xmlns:gmljp2="http://www.opengis.net/gmljp2/2.0"
     xmlns:swe="http://www.opengis.net/swe/2.0"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://www.opengis.net/gmljp2/2.0 http://schemas.opengis.net/gmljp2/2.0/gmljp2.xsd">
  <gml:domainSet nilReason="inapplicable"/>
  <gml:rangeSet>
    <gml:DataBlock>
       <gml:rangeParameters nilReason="inapplicable"/>
       <gml:doubleOrNilReasonTupleList>inapplicable</gml:doubleOrNilReasonTupleList>
     </gml:DataBlock>
  </gml:rangeSet>
  <gmlcov:rangeType>
    <swe:DataRecord>
      <swe:field name="Collection"> </swe:field>
    </swe:DataRecord>
  </gmlcov:rangeType>
  <gmljp2:featureMember>
   <gmljp2:GMLJP2RectifiedGridCoverage gml:id="RGC_1_ID_GMLJP2_0">
     <gml:boundedBy>
       <gml:Envelope srsDimension="2" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
         <gml:lowerCorner>440720 3750120</gml:lowerCorner>
         <gml:upperCorner>441920 3751320</gml:upperCorner>
       </gml:Envelope>
     </gml:boundedBy>
     <gml:domainSet>
      <gml:RectifiedGrid gml:id="RGC_1_GRID_ID_GMLJP2_0" dimension="2" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
       <gml:limits>
         <gml:GridEnvelope>
           <gml:low>0 0</gml:low>
           <gml:high>19 19</gml:high>
         </gml:GridEnvelope>
       </gml:limits>
       <gml:axisName>x</gml:axisName>
       <gml:axisName>y</gml:axisName>
       <gml:origin>
         <gml:Point gml:id="P0001" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
           <gml:pos>440750 3751290</gml:pos>
         </gml:Point>
       </gml:origin>
       <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/26711">60 0</gml:offsetVector>
       <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/26711">0 -60</gml:offsetVector>
      </gml:RectifiedGrid>
     </gml:domainSet>
     <gml:rangeSet>
      <gml:File>
        <gml:rangeParameters/>
        <gml:fileName>gmljp2://codestream/0</gml:fileName>
        <gml:fileStructure>inapplicable</gml:fileStructure>
      </gml:File>
     </gml:rangeSet>
     <gmlcov:rangeType></gmlcov:rangeType>
   </gmljp2:GMLJP2RectifiedGridCoverage>
  </gmljp2:featureMember>
</gmljp2:GMLJP2CoverageCollection>
"""
    assert gmljp2 == minimal_instance

    ret = validate('/vsimem/jp2openjpeg_45.jp2', inspire_tg=False)
    assert ret != 'fail'

    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # GMLJP2V2_DEF={} (inline JSon)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF={}'])
    del out_ds
    ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    assert gmljp2 == minimal_instance
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Invalid JSon
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF={'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Non existing file
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=/vsimem/i_do_not_exist'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Test JSon conf file as a file
    gdal.FileFromMemBuffer("/vsimem/conf.json", '{ "root_instance": { "gml_id": "some_gml_id", "crs_url": false } }')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=/vsimem/conf.json'])
    gdal.Unlink('/vsimem/conf.json')
    del out_ds
    ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    assert 'some_gml_id' in gmljp2
    assert 'urn:ogc:def:crs:EPSG::26711' in gmljp2
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test valid values for grid_coverage_range_type_field_predefined_name
    for predefined in [['Color', 'Color'], ['Elevation_meter', 'Elevation'], ['Panchromatic', 'Panchromatic']]:
        gdal.FileFromMemBuffer("/vsimem/conf.json", '{ "root_instance": { "grid_coverage_range_type_field_predefined_name": "%s" } }' % predefined[0])
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=/vsimem/conf.json'])
        gdal.Unlink('/vsimem/conf.json')
        del out_ds
        ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
        gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
        assert predefined[1] in gmljp2
        ds = None
        gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test invalid value for grid_coverage_range_type_field_predefined_name
    gdal.FileFromMemBuffer("/vsimem/conf.json", '{ "root_instance": { "grid_coverage_range_type_field_predefined_name": "invalid" } }')
    with gdaltest.error_handler():
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=/vsimem/conf.json'])
    gdal.Unlink('/vsimem/conf.json')
    del out_ds
    ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    assert '<gmlcov:rangeType></gmlcov:rangeType>' in gmljp2
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test valid values for grid_coverage_range_type_file
    gdal.FileFromMemBuffer("/vsimem/grid_coverage_range_type_file.xml",
                           """
<swe:DataRecord><swe:field name="custom_datarecord">
    <swe:Quantity definition="http://custom">
        <swe:description>custom</swe:description>
        <swe:uom code="unity"/>
    </swe:Quantity></swe:field></swe:DataRecord>
""")
    gdal.FileFromMemBuffer("/vsimem/conf.json", '{ "root_instance": { "grid_coverage_range_type_file": "/vsimem/grid_coverage_range_type_file.xml" } }')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=/vsimem/conf.json'])
    gdal.Unlink('/vsimem/conf.json')
    gdal.Unlink('/vsimem/grid_coverage_range_type_file.xml')
    del out_ds
    ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    assert "custom_datarecord" in gmljp2, predefined[0]
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test most invalid cases
    import json

    conf = {
        "root_instance": {
            "grid_coverage_file": "/vsimem/i_dont_exist.xml",
        }
    }

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    gdal.PopErrorHandler()
    assert out_ds is None

    conf = {
        "root_instance": {
            "grid_coverage_range_type_file": "/vsimem/i_dont_exist.xml",

            "metadata": [
                "<invalid_root/>",
                "/vsimem/i_dont_exist.xml",
                {
                    "file": "/vsimem/third_metadata.xml",
                    "parent_node": "CoverageCollection"
                },
                {
                    "content": "<invalid_content",
                    "parent_node": "invalid_value"
                }
            ],

            "annotations": [
                "/vsimem/i_dont_exist.shp",
                "/vsimem/i_dont_exist.kml",
                "../gcore/data/byte.tif"
            ],

            "gml_filelist": [
                "/vsimem/i_dont_exist.xml",
                "../gcore/data/byte.tif",
                {
                    "file": "/vsimem/i_dont_exist.shp",
                    "parent_node": "invalid_value",
                    "schema_location": "gmljp2://xml/schema_that_does_not_exist.xsd"
                },
            ],

            "styles": [
                "/vsimem/i_dont_exist.xml",
                "../gcore/data/byte.tif",
                {
                    "file": "/vsimem/i_dont_exist.xml",
                    "parent_node": "invalid_value"
                }
            ],

            "extensions": [
                "/vsimem/i_dont_exist.xml",
                "../gcore/data/byte.tif",
                {
                    "file": "/vsimem/i_dont_exist.xml",
                    "parent_node": "invalid_value"
                }
            ]
        },

        "boxes": [
            "/vsimem/i_dont_exist.xsd",
            {
                "file": "/vsimem/i_dont_exist_too.xsd",
                "label": "i_dont_exist.xsd"
            }
        ]
    }
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    gdal.PopErrorHandler()
    del out_ds
    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test most options: valid case
    gdal.FileFromMemBuffer("/vsimem/second_metadata.xml",
                           """<gmljp2:dcMetadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>Second metadata</dc:title>
</gmljp2:dcMetadata>""")

    gdal.FileFromMemBuffer("/vsimem/third_metadata.xml",
                           """<gmljp2:dcMetadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>Third metadata</dc:title>
</gmljp2:dcMetadata>""")

    gdal.FileFromMemBuffer("/vsimem/feature.xml",
                           """<FeatureCollection gml:id="myFC1" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns="http://www.opengis.net/gml/3.2">
    <featureMember>
        <Observation gml:id="myFC1_Observation">
            <validTime/>
            <resultOf/>
        </Observation>
    </featureMember>
</FeatureCollection>""")

    gdal.FileFromMemBuffer("/vsimem/a_schema.xsd",
                           """<?xml version="1.0" encoding="UTF-8"?>
<xs:schema xmlns:ogr="http://ogr.maptools.org/" targetNamespace="http://ogr.maptools.org/" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:gmlsf="http://www.opengis.net/gmlsf/2.0" elementFormDefault="qualified" version="1.0">
  <xs:annotation>
    <xs:appinfo source="http://schemas.opengis.net/gmlsfProfile/2.0/gmlsfLevels.xsd">
      <gmlsf:ComplianceLevel>0</gmlsf:ComplianceLevel>
    </xs:appinfo>
  </xs:annotation>
  <xs:import namespace="http://www.opengis.net/gml/3.2" schemaLocation="http://schemas.opengis.net/gml/3.2.1/gml.xsd" />
  <xs:import namespace="http://www.opengis.net/gmlsf/2.0" schemaLocation="http://schemas.opengis.net/gmlsfProfile/2.0/gmlsfLevels.xsd" />
  <xs:element name="FeatureCollection" type="ogr:FeatureCollectionType" substitutionGroup="gml:AbstractGML" />
  <xs:complexType name="FeatureCollectionType">
    <xs:complexContent>
      <xs:extension base="gml:AbstractFeatureType">
        <xs:sequence minOccurs="0" maxOccurs="unbounded">
          <xs:element name="featureMember">
            <xs:complexType>
              <xs:complexContent>
                <xs:extension base="gml:AbstractFeatureMemberType">
                  <xs:sequence>
                    <xs:element ref="gml:AbstractFeature" />
                  </xs:sequence>
                </xs:extension>
              </xs:complexContent>
            </xs:complexType>
          </xs:element>
        </xs:sequence>
      </xs:extension>
    </xs:complexContent>
  </xs:complexType>
  <xs:element name="myshape" type="ogr:myshape_Type" substitutionGroup="gml:AbstractFeature" />
  <xs:complexType name="myshape_Type">
    <xs:complexContent>
      <xs:extension base="gml:AbstractFeatureType">
        <xs:sequence>
          <xs:element name="geometryProperty" type="gml:PointPropertyType" nillable="true" minOccurs="0" maxOccurs="1" />
          <xs:element name="foo" nillable="true" minOccurs="0" maxOccurs="1">
            <xs:simpleType>
              <xs:restriction base="xs:string">
                <xs:maxLength value="80" />
              </xs:restriction>
            </xs:simpleType>
          </xs:element>
        </xs:sequence>
      </xs:extension>
    </xs:complexContent>
  </xs:complexType>
</xs:schema>""")

    for name in ['myshape', 'myshape2']:
        ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/' + name + '.shp')
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        lyr = ds.CreateLayer(name, srs=srs)
        lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('foo', 'bar')
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
        lyr.CreateFeature(f)
        ds = None

    gdal.FileFromMemBuffer("/vsimem/feature2.gml",
                           """<FeatureCollection xmlns:ogr="http://ogr.maptools.org/" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://ogr.maptools.org/ http://dummy" gml:id="myFC3">
    <featureMember>
        <Observation gml:id="myFC3_Observation">
            <validTime/>
            <resultOf/>
        </Observation>
    </featureMember>
</FeatureCollection>""")

    gdal.FileFromMemBuffer("/vsimem/feature3.gml",
                           """<FeatureCollection xmlns:ogr="http://ogr.maptools.org/" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd http://ogr.maptools.org/ http://dummy" gml:id="myFC4">
    <featureMember>
        <Observation gml:id="myFC4_Observation">
            <validTime/>
            <resultOf/>
        </Observation>
    </featureMember>
</FeatureCollection>""")

    gdal.FileFromMemBuffer("/vsimem/empty.kml",
                           """<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/kml/2.2 http://schemas.opengis.net/kml/2.2.0/ogckml22.xsd">
    <Document id="empty_doc"/>
</kml>
""")

    gdal.FileFromMemBuffer("/vsimem/style1.xml", '<style1 xmlns="http://dummy" />')
    gdal.FileFromMemBuffer("/vsimem/style2.xml", '<mydummyns:style2 xmlns:mydummyns="http://dummy" />')
    gdal.FileFromMemBuffer("/vsimem/style3.xml", '<style3 />')
    gdal.FileFromMemBuffer("/vsimem/style4.xml", '<style4 />')

    gdal.FileFromMemBuffer("/vsimem/extension1.xml", '<extension1 xmlns="http://dummy" />')
    gdal.FileFromMemBuffer("/vsimem/extension2.xml", '<mydummyns:extension2 xmlns:mydummyns="http://dummy" />')
    gdal.FileFromMemBuffer("/vsimem/extension3.xml", '<extension3 />')
    gdal.FileFromMemBuffer("/vsimem/extension4.xml", '<extension4 />')

    # So that the Python text is real JSon
    false = False

    conf = {
        "root_instance": {
            "grid_coverage_file": "/vsimem/grid_coverage_file.xml",
            "metadata": [
                "<gmljp2:metadata>First metadata</gmljp2:metadata>",
                "/vsimem/second_metadata.xml",
                {
                    "file": "/vsimem/third_metadata.xml",
                    "parent_node": "CoverageCollection"
                },
                {
                    "content":
                    """<?xml version="1.0" encoding="UTF-8"?>
<!-- some comments -->
<gmljp2:eopMetadata>
        <eop:EarthObservation xmlns:eop="http://www.opengis.net/eop/2.0" xmlns:om="http://www.opengis.net/om/2.0" gml:id="EOP1">
                <om:phenomenonTime></om:phenomenonTime>
                <om:resultTime></om:resultTime>
                <om:procedure></om:procedure>
                <om:observedProperty></om:observedProperty>
                <om:featureOfInterest></om:featureOfInterest>
                <om:result></om:result>
                <eop:metaDataProperty>
                        <eop:EarthObservationMetaData>
                                <eop:identifier>Fourth metadata</eop:identifier>
                                <eop:acquisitionType>NOMINAL</eop:acquisitionType>
                                <eop:status>ACQUIRED</eop:status>
                        </eop:EarthObservationMetaData>
                </eop:metaDataProperty>
        </eop:EarthObservation>
</gmljp2:eopMetadata>""",
                    "parent_node": "GridCoverage"
                }
            ],

            "annotations": [
                "/vsimem/myshape.shp",
                "/vsimem/empty.kml"
            ],

            "gml_filelist": [
                "/vsimem/feature.xml",
                {
                    "file": "/vsimem/myshape.shp",
                    "inline": false,
                    "parent_node": "CoverageCollection"
                },
                {
                    "file": "/vsimem/myshape2.shp",
                    "namespace": "http://ogr.maptools.org/",
                    "inline": false,
                    "schema_location": "gmljp2://xml/a_schema.xsd",
                    "parent_node": "GridCoverage"
                },
                {
                    "file": "/vsimem/feature2.gml",
                    "inline": false,
                    "schema_location": "gmljp2://xml/a_schema.xsd"
                },
                {
                    "file": "/vsimem/feature3.gml",
                    "inline": false,
                    "namespace": "http://ogr.maptools.org/",
                    "schema_location": "gmljp2://xml/a_schema.xsd"
                }
            ],

            "styles": [
                "/vsimem/style1.xml",
                {
                    "file": "/vsimem/style2.xml",
                    "parent_node": "GridCoverage"
                },
                {
                    "file": "/vsimem/style3.xml",
                    "parent_node": "CoverageCollection"
                },
                {
                    "file": "/vsimem/style4.xml"
                }
            ],

            "extensions": [
                "/vsimem/extension1.xml",
                {
                    "file": "/vsimem/extension2.xml",
                    "parent_node": "GridCoverage"
                },
                {
                    "file": "/vsimem/extension3.xml",
                    "parent_node": "CoverageCollection"
                },
                {
                    "file": "/vsimem/extension4.xml"
                }
            ]
        },

        "boxes": [
            "/vsimem/a_schema.xsd",
            {
                "file": "/vsimem/a_schema.xsd",
                "label": "duplicated.xsd"
            }
        ]
    }
    gdal.FileFromMemBuffer("/vsimem/grid_coverage_file.xml",
                           """
    <gmljp2:GMLJP2RectifiedGridCoverage gml:id="my_GMLJP2RectifiedGridCoverage">
     <gml:domainSet>
      <gml:RectifiedGrid gml:id="RGC_1_GRID_ID_GMLJP2_0" dimension="2" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
       <gml:limits>
         <gml:GridEnvelope>
           <gml:low>0 0</gml:low>
           <gml:high>19 19</gml:high>
         </gml:GridEnvelope>
       </gml:limits>
       <gml:axisName>x</gml:axisName>
       <gml:axisName>y</gml:axisName>
       <gml:origin>
         <gml:Point gml:id="P0001" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
           <gml:pos>440750 3751290</gml:pos>
         </gml:Point>
       </gml:origin>
       <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/26711">60 0</gml:offsetVector>
       <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/26711">0 -60</gml:offsetVector>
      </gml:RectifiedGrid>
     </gml:domainSet>
     <gml:rangeSet>
      <gml:File>
        <gml:rangeParameters/>
        <gml:fileName>gmljp2://codestream/0</gml:fileName>
        <gml:fileStructure>inapplicable</gml:fileStructure>
      </gml:File>
     </gml:rangeSet>
     <gmlcov:rangeType/>
   </gmljp2:GMLJP2RectifiedGridCoverage>
""")
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    gdal.Unlink("/vsimem/grid_coverage_file.xml")
    gdal.Unlink("/vsimem/second_metadata.xml")
    gdal.Unlink("/vsimem/third_metadata.xml")
    gdal.Unlink("/vsimem/feature.xml")
    for name in ['myshape', 'myshape2']:
        gdal.Unlink("/vsimem/" + name + ".shp")
        gdal.Unlink("/vsimem/" + name + ".shx")
        gdal.Unlink("/vsimem/" + name + ".dbf")
        gdal.Unlink("/vsimem/" + name + ".prj")
    gdal.Unlink("/vsimem/feature2.gml")
    gdal.Unlink("/vsimem/feature3.gml")
    gdal.Unlink("/vsimem/empty.kml")
    gdal.Unlink("/vsimem/a_schema.xsd")
    gdal.Unlink("/vsimem/style1.xml")
    gdal.Unlink("/vsimem/style2.xml")
    gdal.Unlink("/vsimem/style3.xml")
    gdal.Unlink("/vsimem/style4.xml")
    gdal.Unlink("/vsimem/extension1.xml")
    gdal.Unlink("/vsimem/extension2.xml")
    gdal.Unlink("/vsimem/extension3.xml")
    gdal.Unlink("/vsimem/extension4.xml")
    del out_ds

    # Now do the checks
    dircontent = gdal.ReadDir('/vsimem/gmljp2')
    assert dircontent is None

    ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    assert 'my_GMLJP2RectifiedGridCoverage' in gmljp2
    first_metadata_pos = gmljp2.find("First metadata")
    second_metadata_pos = gmljp2.find("Second metadata")
    third_metadata_pos = gmljp2.find("Third metadata")
    GMLJP2RectifiedGridCoverage_pos = gmljp2.find('GMLJP2RectifiedGridCoverage')
    fourth_metadata_pos = gmljp2.find("Fourth metadata")
    feature_pos = gmljp2.find("""<FeatureCollection gml:id="ID_GMLJP2_0_0_myFC1" """)
    myshape_gml_pos = gmljp2.find("""<gmljp2:feature xlink:href="gmljp2://xml/myshape.gml" """)
    myshape2_gml_pos = gmljp2.find("""<gmljp2:feature xlink:href="gmljp2://xml/myshape2.gml" """)
    feature2_pos = gmljp2.find("""<gmljp2:feature xlink:href="gmljp2://xml/feature2.gml" """)
    feature3_pos = gmljp2.find("""<gmljp2:feature xlink:href="gmljp2://xml/feature3.gml" """)
    myshape_kml_pos = gmljp2.find("""<Document id="root_doc_0">""")
    empty_kml_pos = gmljp2.find("""<Document id="empty_doc" />""")
    style1_pos = gmljp2.find("""<style1 xmlns="http://dummy" />""")
    style2_pos = gmljp2.find("""<mydummyns:style2 xmlns:mydummyns="http://dummy" />""")
    style3_pos = gmljp2.find("""<style3 xmlns="http://undefined_namespace" />""")
    style4_pos = gmljp2.find("""<style4 xmlns="http://undefined_namespace" />""")
    extension1_pos = gmljp2.find("""<extension1 xmlns="http://dummy" />""")
    extension2_pos = gmljp2.find("""<mydummyns:extension2 xmlns:mydummyns="http://dummy" />""")
    extension3_pos = gmljp2.find("""<extension3 xmlns="http://undefined_namespace" />""")
    extension4_pos = gmljp2.find("""<extension4 xmlns="http://undefined_namespace" />""")

    assert (first_metadata_pos >= 0 and second_metadata_pos >= 0 and third_metadata_pos >= 0 and \
       GMLJP2RectifiedGridCoverage_pos >= 0 and fourth_metadata_pos >= 0 and \
       feature_pos >= 0 and myshape_gml_pos >= 0 and myshape2_gml_pos >= 0 and \
       feature2_pos >= 0 and myshape_kml_pos >= 0 and empty_kml_pos >= 0 and \
       style1_pos >= 0 and style2_pos >= 0 and style3_pos >= 0 and style4_pos >= 0 and \
       extension1_pos >= 0 and extension2_pos >= 0 and extension3_pos >= 0 and extension4_pos >= 0 and(first_metadata_pos < second_metadata_pos and
            second_metadata_pos < third_metadata_pos and
            third_metadata_pos < GMLJP2RectifiedGridCoverage_pos and
            GMLJP2RectifiedGridCoverage_pos < fourth_metadata_pos and
            fourth_metadata_pos < feature_pos and
            fourth_metadata_pos < feature_pos and
            myshape2_gml_pos < myshape_kml_pos and
            myshape_kml_pos < empty_kml_pos and
            empty_kml_pos < style2_pos and
            style2_pos < extension2_pos and
            extension2_pos < feature_pos and
            feature_pos < myshape_gml_pos and
            myshape_gml_pos < feature2_pos and
            feature2_pos < feature3_pos and
            feature3_pos < style1_pos and
            style1_pos < style3_pos and
            style3_pos < style4_pos and
            style4_pos < extension1_pos and
            extension1_pos < extension3_pos and
            extension3_pos < extension4_pos)), \
        gmljp2
    # print(gmljp2)

    myshape_gml = ds.GetMetadata_List("xml:myshape.gml")[0]
    assert """<ogr1:FeatureCollection gml:id="ID_GMLJP2_0_1_aFeatureCollection" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://ogr.maptools.org/1 gmljp2://xml/myshape.xsd" xmlns:ogr1="http://ogr.maptools.org/1" xmlns:gml="http://www.opengis.net/gml/3.2">""" in myshape_gml
    assert """http://www.opengis.net/def/crs/EPSG/0/4326""" in myshape_gml

    myshape_xsd = ds.GetMetadata_List("xml:myshape.xsd")[0]
    assert """<xs:schema targetNamespace="http://ogr.maptools.org/1" xmlns:ogr1="http://ogr.maptools.org/1" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:gmlsf="http://www.opengis.net/gmlsf/2.0" elementFormDefault="qualified" version="1.0">""" in myshape_xsd

    myshape2_gml = ds.GetMetadata_List("xml:myshape2.gml")[0]
    assert """<ogr2:FeatureCollection gml:id="ID_GMLJP2_0_2_aFeatureCollection" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://ogr.maptools.org/ gmljp2://xml/a_schema.xsd" xmlns:ogr2="http://ogr.maptools.org/" xmlns:gml="http://www.opengis.net/gml/3.2">""" in myshape2_gml

    feature2_gml = ds.GetMetadata_List("xml:feature2.gml")[0]
    assert """<FeatureCollection xmlns:ogr="http://ogr.maptools.org/" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://ogr.maptools.org/ gmljp2://xml/a_schema.xsd" gml:id="ID_GMLJP2_0_3_myFC3">""" in feature2_gml

    feature3_gml = ds.GetMetadata_List("xml:feature3.gml")[0]
    assert """<FeatureCollection xmlns:ogr="http://ogr.maptools.org/" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd http://ogr.maptools.org/ gmljp2://xml/a_schema.xsd" gml:id="ID_GMLJP2_0_4_myFC4">""" in feature3_gml

    myshape2_xsd = ds.GetMetadata_List("xml:a_schema.xsd")[0]
    assert """<xs:schema xmlns:ogr="http://ogr.maptools.org/" """ in myshape2_xsd

    duplicated_xsd = ds.GetMetadata_List("xml:duplicated.xsd")[0]
    assert """<xs:schema xmlns:ogr="http://ogr.maptools.org/" """ in duplicated_xsd, \
        myshape2_xsd

    ds = None

    ds = ogr.Open('/vsimem/jp2openjpeg_45.jp2')
    assert ds.GetLayerCount() == 6, [ ds.GetLayer(j).GetName() for j in range(ds.GetLayerCount()) ]
    expected_layers = ['FC_GridCoverage_1_myshape',
                       'FC_CoverageCollection_1_Observation',
                       'FC_CoverageCollection_2_myshape',
                       'FC_CoverageCollection_3_myshape',
                       'FC_CoverageCollection_4_myshape',
                       'Annotation_1_myshape']
    for j in range(6):
        if ds.GetLayer(j).GetName() != expected_layers[j]:
            for i in range(ds.GetLayerCount()):
                print(ds.GetLayer(i).GetName())
            pytest.fail()
    ds = None

    ret = validate('/vsimem/jp2openjpeg_45.jp2', inspire_tg=False)
    assert ret != 'fail'

    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test reading a feature collection with a schema and within GridCoverage
    conf = {"root_instance": {"gml_filelist": [{"file": "../ogr/data/poly.shp", "parent_node": "GridCoverage"}]}}
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    del out_ds

    dircontent = gdal.ReadDir('/vsimem/gmljp2')
    assert dircontent is None

    ds = ogr.Open('/vsimem/jp2openjpeg_45.jp2')
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0).GetName() == 'FC_GridCoverage_1_poly'
    ds = None

    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test serializing GDAL metadata
    true = True
    conf = {"root_instance": {"metadata": [{"gdal_metadata": true}]}}
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000, 1, 0, 5000000, 0, -1])
    src_ds.SetMetadataItem('FOO', 'BAR')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2openjpeg_45.jp2.aux.xml') is not None:
        fp = gdal.VSIFOpenL('/vsimem/jp2openjpeg_45.jp2.aux.xml', 'rb')
        if fp is not None:
            data = gdal.VSIFReadL(1, 100000, fp).decode('ascii')
            gdal.VSIFCloseL(fp)
            print(data)
        pytest.fail()

    ds = gdal.Open('/vsimem/jp2openjpeg_45.jp2')
    assert ds.GetMetadata() == {'FOO': 'BAR'}
    ds = None

    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    # Test writing&reading a gmljp2:featureMember pointing to a remote resource
    conf = {"root_instance": {"gml_filelist": [{"remote_resource": "https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/expected_gml_gml32.gml"}]}}
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_45.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    del out_ds

    # Nothing should be reported by default
    ds = ogr.Open('/vsimem/jp2openjpeg_45.jp2')
    assert ds is None
    ds = None

    # We have to explicitly allow it.
    ds = gdal.OpenEx('/vsimem/jp2openjpeg_45.jp2', open_options=['OPEN_REMOTE_GML=YES'])
    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2')

    if ds is None:
        if gdaltest.gdalurlopen('https://raw.githubusercontent.com/OSGeo/gdal/release/3.1/autotest/ogr/data/expected_gml_gml32.gml') is None:
            pytest.skip()
        pytest.fail()
    assert ds.GetLayerCount() == 1
    ds = None

    gdal.Unlink('/vsimem/jp2openjpeg_45.jp2.aux.xml')

###############################################################################
# Test GMLJP2v2 metadata generator / XPath


def test_jp2openjpeg_46():

    import json

    conf = {
        "root_instance": {
            "metadata": [
                {
                    "dynamic_metadata":
                    {
                        "template": "/vsimem/template.xml",
                        "source": "/vsimem/source.xml",
                    }
                }
            ]
        }
    }

    gdal.FileFromMemBuffer("/vsimem/template.xml",
                           """<gmljp2:metadata>{{{ XPATH(1) }}} {{{ XPATH('str') }}} {{{ XPATH(true()) }}}X{{{ XPATH(//B) }}} {{{XPATH(if(//B/text() = 'my_value',if(false(),'not_expected',concat('yeah: ',uuid())),'doh!'))}}}</gmljp2:metadata>""")

    gdal.FileFromMemBuffer("/vsimem/source.xml",
                           """<A><B>my_value</B></A>""")

    src_ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_46.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    gdal.Unlink("/vsimem/template.xml")
    gdal.Unlink("/vsimem/source.xml")
    del out_ds
    if gdal.GetLastErrorMsg().find('dynamic_metadata not supported') >= 0:
        gdal.Unlink('/vsimem/jp2openjpeg_46.jp2')
        pytest.skip()
    # Maybe a conflict with FileGDB libxml2
    if gdal.GetLastErrorMsg().find('An error occurred in libxml2') >= 0:
        gdal.Unlink('/vsimem/jp2openjpeg_46.jp2')
        pytest.skip()

    ds = gdal.Open('/vsimem/jp2openjpeg_46.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    if """<gmljp2:metadata>1 str trueX
        <B>my_value</B>
yeah: """ not in gmljp2:
        if """<gmljp2:metadata>1 str true""" in gmljp2:
            pytest.skip()
        pytest.fail(gmljp2)
    ds = None

    gdal.Unlink('/vsimem/jp2openjpeg_46.jp2')

    for invalid_template in [
            '<gmljp2:metadata>{{{</gmljp2:metadata>',
            '<gmljp2:metadata>{{{}}}</gmljp2:metadata>',
            '<gmljp2:metadata>{{{XPATH</gmljp2:metadata>',
            '<gmljp2:metadata>{{{XPATH(1)</gmljp2:metadata>',
            '<gmljp2:metadata>{{{XPATH(}}}</gmljp2:metadata>',
            '<gmljp2:metadata>{{{XPATH()}}}</gmljp2:metadata>',
            '<gmljp2:metadata>{{{XPATH(//node[)}}}</gmljp2:metadata>',
    ]:

        gdal.FileFromMemBuffer("/vsimem/template.xml", invalid_template)
        gdal.FileFromMemBuffer("/vsimem/source.xml", """<A/>""")

        gdal.ErrorReset()
        gdal.PushErrorHandler()
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_46.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
        gdal.PopErrorHandler()
        # print('error : ' + gdal.GetLastErrorMsg())
        gdal.Unlink("/vsimem/template.xml")
        gdal.Unlink("/vsimem/source.xml")
        del out_ds

        ds = gdal.Open('/vsimem/jp2openjpeg_46.jp2')
        gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
        ds = None

        gdal.Unlink('/vsimem/jp2openjpeg_46.jp2')

        assert '<gmljp2:metadata>' not in gmljp2, invalid_template

    # Nonexistent template.
    gdal.FileFromMemBuffer("/vsimem/source.xml", """<A/>""")
    conf = {
        "root_instance": {
            "metadata": [
                {
                    "dynamic_metadata":
                    {
                        "template": "/vsimem/not_existing_template.xml",
                        "source": "/vsimem/source.xml",
                    }
                }
            ]
        }
    }
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_46.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    gdal.PopErrorHandler()
    del out_ds
    gdal.Unlink("/vsimem/source.xml")
    gdal.Unlink('/vsimem/jp2openjpeg_46.jp2')

    # Nonexistent source
    gdal.FileFromMemBuffer(
        "/vsimem/template.xml", """<gmljp2:metadata></gmljp2:metadata>""")
    conf = {
        "root_instance": {
            "metadata": [
                {
                    "dynamic_metadata":
                    {
                        "template": "/vsimem/template.xml",
                        "source": "/vsimem/not_existing_source.xml",
                    }
                }
            ]
        }
    }
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_46.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    gdal.PopErrorHandler()
    del out_ds
    gdal.Unlink("/vsimem/template.xml")
    gdal.Unlink('/vsimem/jp2openjpeg_46.jp2')

    # Invalid source
    gdal.FileFromMemBuffer("/vsimem/template.xml", """<gmljp2:metadata></gmljp2:metadata>""")
    gdal.FileFromMemBuffer("/vsimem/source.xml", """<A""")
    conf = {
        "root_instance": {
            "metadata": [
                {
                    "dynamic_metadata":
                    {
                        "template": "/vsimem/template.xml",
                        "source": "/vsimem/source.xml",
                    }
                }
            ]
        }
    }
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_46.jp2', src_ds, options=['GMLJP2V2_DEF=' + json.dumps(conf)])
    gdal.PopErrorHandler()
    del out_ds
    gdal.Unlink("/vsimem/template.xml")
    gdal.Unlink("/vsimem/source.xml")
    # ds = gdal.Open('/vsimem/jp2openjpeg_46.jp2')
    # gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    # print(gmljp2)
    # ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_46.jp2')

###############################################################################
# Test writing & reading RPC in GeoJP2 box


def test_jp2openjpeg_47():

    src_ds = gdal.Open('../gcore/data/byte_rpc.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_47.jp2', src_ds)
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2openjpeg_47.jp2.aux.xml') is None

    ds = gdal.Open('/vsimem/jp2openjpeg_47.jp2')
    assert ds.GetMetadata('RPC') is not None
    ds = None

    gdal.Unlink('/vsimem/jp2openjpeg_47.jp2')

###############################################################################
# Test reading a dataset whose tile dimensions are larger than dataset ones


def test_jp2openjpeg_48():

    ds = gdal.Open('data/jpeg2000/byte_tile_2048.jp2')
    (blockxsize, blockysize) = ds.GetRasterBand(1).GetBlockSize()
    assert (blockxsize, blockysize) == (20, 20)
    assert ds.GetRasterBand(1).Checksum() == 4610
    ds = None

###############################################################################


def test_jp2openjpeg_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        pytest.skip()

    # Checksum = 32669 on my PC
    tst = gdaltest.GDALTest('JP2OpenJPEG', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

###############################################################################


def test_jp2openjpeg_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        pytest.skip()

    # Checksum = 15621 on my PC
    tst = gdaltest.GDALTest('JP2OpenJPEG', 'tmp/cache/gcp.jp2', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/gcp.jp2')
    ds.GetRasterBand(1).Checksum()
    assert len(ds.GetGCPs()) == 15, 'bad number of GCP'

    expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]"""
    assert ds.GetGCPProjection() == expected_wkt, 'bad GCP projection'

    ds = None

###############################################################################


def test_jp2openjpeg_online_3():

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        pytest.skip()

    tst = gdaltest.GDALTest('JP2OpenJPEG', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute=1)

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


def test_jp2openjpeg_online_4():

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        pytest.skip()

    tst = gdaltest.GDALTest('JP2OpenJPEG', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/Bretagne2.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne2.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, 0, 0, 1024, 1024)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    assert maxdiff <= 10, 'Image too different from reference'

###############################################################################
# Try reading JP2OpenJPEG with color table


def test_jp2openjpeg_online_5():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9.jp2', 'file9.jp2'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/file9.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    assert cs1 == 47664, 'Did not get expected checksums'
    assert ds.GetRasterBand(1).GetColorTable() is not None, \
        'Did not get expected color table'
    ds = None

###############################################################################
# Try reading YCbCr JP2OpenJPEG as RGB


def test_jp2openjpeg_online_6():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_03/file3.jp2', 'file3.jp2'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/file3.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    assert cs1 == 26140 and cs2 == 32689 and cs3 == 48247, \
        'Did not get expected checksums'

    ds = None

###############################################################################
# Test GDAL_GEOREF_SOURCES


def test_jp2openjpeg_49():

    tests = [(None, True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             (None, True, False, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             (None, False, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             (None, False, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
             ('INTERNAL', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
             ('INTERNAL,PAM', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             ('INTERNAL,WORLDFILE', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('INTERNAL,PAM,WORLDFILE', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             ('INTERNAL,WORLDFILE,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,PAM,INTERNAL', False, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
             ('PAM,WORLDFILE,INTERNAL', False, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
             ('PAM', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             ('PAM,WORLDFILE', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             ('WORLDFILE', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,INTERNAL', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,PAM,INTERNAL', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,INTERNAL,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('NONE', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
             ]

    for (config_option_value, copy_pam, copy_worldfile, expected_srs, expected_gt) in tests:
        gdal.SetConfigOption('GDAL_GEOREF_SOURCES', config_option_value)
        gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.jp2', open('data/jpeg2000/byte_nogeoref.jp2', 'rb').read())
        if copy_pam:
            gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.jp2.aux.xml', open('data/jpeg2000/byte_nogeoref.jp2.aux.xml', 'rb').read())
        if copy_worldfile:
            gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.j2w', open('data/jpeg2000/byte_nogeoref.j2w', 'rb').read())
        ds = gdal.Open('/vsimem/byte_nogeoref.jp2')
        gt = ds.GetGeoTransform()
        srs_wkt = ds.GetProjectionRef()
        ds = None
        gdal.SetConfigOption('GDAL_GEOREF_SOURCES', None)
        gdal.Unlink('/vsimem/byte_nogeoref.jp2')
        gdal.Unlink('/vsimem/byte_nogeoref.jp2.aux.xml')
        gdal.Unlink('/vsimem/byte_nogeoref.j2w')

        if gt != expected_gt:
            print('Got ' + str(gt))
            print('Expected ' + str(expected_gt))
            pytest.fail('Did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value, str(copy_pam), str(copy_worldfile)))

        if expected_srs == 'LOCAL_CS["PAM"]' and srs_wkt == 'LOCAL_CS["PAM",UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]':
            pass # ok
        elif (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and expected_srs not in srs_wkt):
            print('Got ' + srs_wkt)
            print('Expected ' + expected_srs)
            pytest.fail('Did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value, str(copy_pam), str(copy_worldfile)))

    tests = [(None, True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             (None, True, False, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             (None, False, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             (None, False, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('INTERNAL', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('INTERNAL,PAM', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('INTERNAL,WORLDFILE', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('INTERNAL,PAM,WORLDFILE', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('INTERNAL,WORLDFILE,PAM', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('WORLDFILE,PAM,INTERNAL', False, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('PAM,WORLDFILE,INTERNAL', False, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('GEOJP2', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('GEOJP2,GMLJP2', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('GMLJP2', True, True, '26712', (439970.0, 60.0, 0.0, 3751030.0, 0.0, -60.0)),
             ('GMLJP2,GEOJP2', True, True, '26712', (439970.0, 60.0, 0.0, 3751030.0, 0.0, -60.0)),
             ('MSIG', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
             ('MSIG,GMLJP2,GEOJP2', True, True, '26712', (439970.0, 60.0, 0.0, 3751030.0, 0.0, -60.0)),
             ('MSIG,GEOJP2,GMLJP2', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
             ('PAM', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             ('PAM,WORLDFILE', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
             ('WORLDFILE', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,INTERNAL', True, True, '26711', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,PAM,INTERNAL', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('WORLDFILE,INTERNAL,PAM', True, True, '26711', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
             ('NONE', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
             ]

    for (config_option_value, copy_pam, copy_worldfile, expected_srs, expected_gt) in tests:
        gdal.FileFromMemBuffer('/vsimem/inconsitant_geojp2_gmljp2.jp2', open('data/jpeg2000/inconsitant_geojp2_gmljp2.jp2', 'rb').read())
        if copy_pam:
            gdal.FileFromMemBuffer('/vsimem/inconsitant_geojp2_gmljp2.jp2.aux.xml', open('data/jpeg2000/inconsitant_geojp2_gmljp2.jp2.aux.xml', 'rb').read())
        if copy_worldfile:
            gdal.FileFromMemBuffer('/vsimem/inconsitant_geojp2_gmljp2.j2w', open('data/jpeg2000/inconsitant_geojp2_gmljp2.j2w', 'rb').read())
        open_options = []
        if config_option_value is not None:
            open_options += ['GEOREF_SOURCES=' + config_option_value]
        ds = gdal.OpenEx('/vsimem/inconsitant_geojp2_gmljp2.jp2', open_options=open_options)
        gt = ds.GetGeoTransform()
        srs_wkt = ds.GetProjectionRef()
        ds = None
        gdal.Unlink('/vsimem/inconsitant_geojp2_gmljp2.jp2')
        gdal.Unlink('/vsimem/inconsitant_geojp2_gmljp2.jp2.aux.xml')
        gdal.Unlink('/vsimem/inconsitant_geojp2_gmljp2.j2w')

        if gt != expected_gt:
            print('Got ' + str(gt))
            print('Expected ' + str(expected_gt))
            pytest.fail('Did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value, str(copy_pam), str(copy_worldfile)))

        if expected_srs == 'LOCAL_CS["PAM"]' and srs_wkt == 'LOCAL_CS["PAM",UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]':
            pass # ok
        elif (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and expected_srs not in srs_wkt):
            print('Got ' + srs_wkt)
            print('Expected ' + expected_srs)
            pytest.fail('Did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value, str(copy_pam), str(copy_worldfile)))

    ds = gdal.OpenEx('data/jpeg2000/inconsitant_geojp2_gmljp2.jp2', open_options=['GEOREF_SOURCES=PAM,WORLDFILE'])
    fl = ds.GetFileList()
    assert set(fl) == set(['data/jpeg2000/inconsitant_geojp2_gmljp2.jp2', 'data/jpeg2000/inconsitant_geojp2_gmljp2.jp2.aux.xml']), \
        'Did not get expected filelist'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.OpenEx('data/jpeg2000/inconsitant_geojp2_gmljp2.jp2', open_options=['GEOREF_SOURCES=unhandled'])
        assert gdal.GetLastErrorMsg() != '', 'expected warning'

    
###############################################################################
# Test opening an image of small dimension with very small tiles (#7012)


def test_jp2openjpeg_50():

    ds = gdal.Open('data/jpeg2000/fake_sent2_preview.jp2')
    blockxsize, blockysize = ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == ds.RasterXSize and blockysize == ds.RasterYSize, \
        'expected warning'
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 2046, 'expected warning'

###############################################################################
# Test CODEBLOCK_STYLE


def test_jp2openjpeg_codeblock_style():

    if gdaltest.jp2openjpeg_drv.GetMetadataItem('DMD_CREATIONOPTIONLIST').find('CODEBLOCK_STYLE') < 0:
        pytest.skip()

    filename = '/vsimem/jp2openjpeg_codeblock_style.jp2'
    for options in [['CODEBLOCK_STYLE=63', 'REVERSIBLE=YES', 'QUALITY=100'],
                    ['CODEBLOCK_STYLE=BYPASS,RESET,TERMALL,VSC,PREDICTABLE,SEGSYM', 'REVERSIBLE=YES', 'QUALITY=100']]:
        gdal.ErrorReset()
        gdaltest.jp2openjpeg_drv.CreateCopy(filename, gdal.Open('data/byte.tif'),
                                            options=options)
        assert gdal.GetLastErrorMsg() == ''
        ds = gdal.Open(filename)
        cs = ds.GetRasterBand(1).Checksum()
        ds = None
        assert cs == 4672

    with gdaltest.error_handler():
        gdaltest.jp2openjpeg_drv.CreateCopy(filename, gdal.Open('data/byte.tif'),
                                            options=['CODEBLOCK_STYLE=64'])
    assert gdal.GetLastErrorMsg() != ''

    with gdaltest.error_handler():
        gdaltest.jp2openjpeg_drv.CreateCopy(filename, gdal.Open('data/byte.tif'),
                                            options=['CODEBLOCK_STYLE=UNSUPPORTED'])
    assert gdal.GetLastErrorMsg() != ''

    gdaltest.jp2openjpeg_drv.Delete(filename)

###############################################################################
# Test external overviews


def test_jp2openjpeg_external_overviews_single_band():

    filename = '/vsimem/jp2openjpeg_external_overviews_single_band.jp2'
    gdaltest.jp2openjpeg_drv.CreateCopy(filename,
                                        gdal.Open('../gcore/data/utmsmall.tif'),
                                        options=['REVERSIBLE=YES', 'QUALITY=100'])
    ds = gdal.Open(filename)
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open(filename)
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    gdaltest.jp2openjpeg_drv.Delete(filename)

    assert cs == 28926

###############################################################################
# Test external overviews


def test_jp2openjpeg_external_overviews_multiple_band():

    filename = '/vsimem/jp2openjpeg_external_overviews_multiple_band.jp2'
    gdaltest.jp2openjpeg_drv.CreateCopy(filename,
                                        gdal.Open('data/small_world.tif'),
                                        options=['REVERSIBLE=YES', 'QUALITY=100'])
    ds = gdal.Open(filename)
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open(filename)
    cs = [ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(3)]
    ds = None

    gdaltest.jp2openjpeg_drv.Delete(filename)

    assert cs == [6233, 7706, 26085]

###############################################################################
# Test accessing overview levels when the dimensions of the full resolution
# image are not a multiple of 2^numresolutions


def test_jp2openjpeg_odd_dimensions():

    ds = gdal.Open('data/jpeg2000/513x513.jp2')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    assert cs == 29642

###############################################################################


def test_jp2openjpeg_odd_dimensions_overviews():

    # Only try the rest with openjpeg >= 2.3 to avoid potential memory issues
    if gdaltest.jp2openjpeg_drv.GetMetadataItem('DMD_CREATIONOPTIONLIST').find('CODEBLOCK_STYLE') < 0:
        pytest.skip()

    # Check that we don't request outside of the full resolution coordinates
    ds = gdal.Open('data/jpeg2000/single_block_32769_16385.jp2')
    assert ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,2049,1025)
    assert gdal.GetLastErrorMsg() == ''
    ds = None

###############################################################################
# Test reading an image whose origin is not (0,0)


def test_jp2openjpeg_image_origin_not_zero():

    ds = gdal.Open('data/jpeg2000/byte_image_origin_not_zero.jp2')
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).ReadRaster(0,0,20,20,10,10) is not None


###############################################################################
# Test reading an image whose tile size is 16 (#2984)


def test_jp2openjpeg_tilesize_16():

    # Generated with gdal_translate byte.tif foo.jp2 -of jp2openjpeg -outsize 256 256 -co blockxsize=16 -co blockysize=16 -co BLOCKSIZE_STRICT=true -co resolutions=3
    ds = gdal.Open('data/jpeg2000/tile_size_16.jp2')
    assert ds.GetRasterBand(1).Checksum() == 44216
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 61711

###############################################################################
# Test generation of PLT marker segments


def test_jp2openjpeg_generate_PLT():

    # Only try the rest with openjpeg > 2.3.1 that supports it
    if gdaltest.jp2openjpeg_drv.GetMetadataItem('DMD_CREATIONOPTIONLIST').find('PLT') < 0:
        pytest.skip()

    filename = '/vsimem/temp.jp2'
    gdaltest.jp2openjpeg_drv.CreateCopy(filename, gdal.Open('data/byte.tif'),
                                        options=['PLT=YES',
                                                 'REVERSIBLE=YES',
                                                 'QUALITY=100'])

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None

    # Check presence of a PLT marker
    ret = gdal.GetJPEG2000StructureAsString(filename, ['ALL=YES'])
    assert '<Marker name="PLT"' in ret

    gdaltest.jp2openjpeg_drv.Delete(filename)

