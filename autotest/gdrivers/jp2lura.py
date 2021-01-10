#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JP2Lura driver.
# Author:   Even Rouault  <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault at spatialys.com>
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
import struct
import sys
import shutil
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

import gdaltest
from test_py_scripts import samples_path

def jp2lura_available():

    if gdal.GetConfigOption('LURA_LICENSE_NUM_1') is None or \
        gdal.GetConfigOption('LURA_LICENSE_NUM_2') is None:
        return False

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2Lura')
    ds = gdal.Open( os.path.join(os.path.dirname(__file__), 'data/jpeg2000/byte.jp2') )
    gdaltest.reregister_all_jpeg2000_drivers()
    return ds is not None


pytestmark = [ pytest.mark.require_driver('JP2Lura'),
               pytest.mark.skipif(not jp2lura_available(), reason='JP2Lura driver not available or missing license')
             ]

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.jp2lura_drv = gdal.GetDriverByName('JP2Lura')
    assert gdaltest.jp2lura_drv is not None

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2Lura')

    yield

    gdaltest.reregister_all_jpeg2000_drivers()


###############################################################################
#


def test_jp2lura_missing_license_num():


    old_num_1 = gdal.GetConfigOption('LURA_LICENSE_NUM_1')
    old_num_2 = gdal.GetConfigOption('LURA_LICENSE_NUM_2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', '')
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', '')
    with gdaltest.error_handler():
        ds = gdal.Open('data/jpeg2000/byte.jp2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', old_num_1)
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', old_num_2)

    assert ds is None

###############################################################################
#


def test_jp2lura_invalid_license_num():


    old_num_1 = gdal.GetConfigOption('LURA_LICENSE_NUM_1')
    old_num_2 = gdal.GetConfigOption('LURA_LICENSE_NUM_2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', '1')
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', '1')
    with gdaltest.error_handler():
        ds = gdal.Open('data/jpeg2000/byte.jp2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', old_num_1)
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', old_num_2)

    assert ds is None

###############################################################################


def validate(filename, expected_gmljp2=True, return_error_count=False, oidoc=None, inspire_tg=True):

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    try:
        import validate_jp2
    except ImportError:
        pytest.skip('Cannot run validate_jp2')

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
# Open byte.jp2


def test_jp2lura_2():

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

    tst = gdaltest.GDALTest('JP2Lura', 'jpeg2000/byte.jp2', 1, 50054)
    return tst.testOpen(check_prj=srs, check_gt=gt)

###############################################################################
# Open int16.jp2


def test_jp2lura_3():

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


def test_jp2lura_4(out_filename='tmp/jp2lura_4.jp2'):

    src_ds = gdal.Open('data/jpeg2000/byte.jp2')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()

    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/jp2lura_4.vrt', src_ds)
    vrt_ds.SetMetadataItem('TIFFTAG_XRESOLUTION', '300')
    vrt_ds.SetMetadataItem('TIFFTAG_YRESOLUTION', '200')
    vrt_ds.SetMetadataItem('TIFFTAG_RESOLUTIONUNIT', '3 (pixels/cm)')

    gdal.Unlink(out_filename)

    out_ds = gdal.GetDriverByName('JP2Lura').CreateCopy(out_filename, vrt_ds, options=['REVERSIBLE=YES'])
    del out_ds

    vrt_ds = None
    gdal.Unlink('/vsimem/jp2lura_4.vrt')

    assert gdal.VSIStatL(out_filename + '.aux.xml') is None

    assert validate(out_filename, inspire_tg=False) != 'fail'

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


def test_jp2lura_4_vsimem():
    return test_jp2lura_4('/vsimem/jp2lura_4.jp2')

###############################################################################
# Test copying int16.jp2


def test_jp2lura_5():

    tst = gdaltest.GDALTest('JP2Lura', 'jpeg2000/int16.jp2', 1, None, options=['REVERSIBLE=YES', 'CODEC=J2K'])
    return tst.testCreateCopy()

###############################################################################
# Test reading ll.jp2


def test_jp2lura_6():

    tst = gdaltest.GDALTest('JP2Lura', 'jpeg2000/ll.jp2', 1, None)

    tst.testOpen()

    ds = gdal.Open('data/jpeg2000/ll.jp2')
    ds.GetRasterBand(1).Checksum()
    ds = None

###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)


def test_jp2lura_7():

    tst = gdaltest.GDALTest('JP2Lura', '/vsigzip/data/jpeg2000/byte.jp2.gz', 1, 50054, filename_absolute=1)
    ret = tst.testOpen()
    gdal.Unlink('data/jpeg2000/byte.jp2.gz.properties')
    return ret

###############################################################################
# Test a JP2Lura with the 3 bands having 13bit depth and the 4th one 1 bit


def test_jp2lura_8():

    ds = gdal.Open('data/jpeg2000/3_13bit_and_1bit.jp2')

    expected_checksums = [64570, 57277, 56048]  # 61292]

    for i, csum in enumerate(expected_checksums):
        assert ds.GetRasterBand(i + 1).Checksum() == csum, \
            ('unexpected checksum (%d) for band %d' % (csum, i + 1))

    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16, 'unexpected data type'

###############################################################################
# Check that we can use .j2w world files (#4651)


def test_jp2lura_9():

    ds = gdal.Open('data/jpeg2000/byte_without_geotransform.jp2')

    geotransform = ds.GetGeoTransform()
    assert geotransform[0] == pytest.approx(440720, abs=0.1) and geotransform[1] == pytest.approx(60, abs=0.001) and geotransform[2] == pytest.approx(0, abs=0.001) and geotransform[3] == pytest.approx(3751320, abs=0.1) and geotransform[4] == pytest.approx(0, abs=0.001) and geotransform[5] == pytest.approx(-60, abs=0.001), \
        'geotransform differs from expected'

    ds = None

###############################################################################
# Test YCBCR420 creation option


def DISABLED_jp2lura_10():

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_10.jp2', src_ds, options=['YCBCR420=YES', 'RESOLUTIONS=3'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    assert out_ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert out_ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert out_ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    del out_ds
    src_ds = None
    gdal.Unlink('/vsimem/jp2lura_10.jp2')

    # Quite a bit of difference...
    assert maxdiff <= 12, 'Image too different from reference'

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit


def DISABLED_jp2lura_11():

    ds = gdal.Open('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2')
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is None
    got_cs = fourth_band.Checksum()
    assert got_cs == 8527
    jp2_bands_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    jp2_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, int(ds.RasterXSize / 16), int(ds.RasterYSize / 16))

    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2lura_11.tif', ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    # gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/jp2lura_11.tif')
    assert got_cs == 8527

    assert jp2_bands_data == gtiff_bands_data

    assert jp2_fourth_band_data == gtiff_fourth_band_data

    ds = gdal.OpenEx('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'

###############################################################################
# Check that PAM overrides internal georeferencing (#5279)


def test_jp2lura_12():


    # Override projection
    shutil.copy('data/jpeg2000/byte.jp2', 'tmp/jp2lura_12.jp2')

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    wkt = ds.GetProjectionRef()
    ds = None

    gdaltest.jp2lura_drv.Delete('tmp/jp2lura_12.jp2')

    assert '32631' in wkt

    # Override geotransform
    shutil.copy('data/jpeg2000/byte.jp2', 'tmp/jp2lura_12.jp2')

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    ds.SetGeoTransform([1000, 1, 0, 2000, 0, -1])
    ds = None

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    gt = ds.GetGeoTransform()
    ds = None

    gdaltest.jp2lura_drv.Delete('tmp/jp2lura_12.jp2')

    assert gt == (1000, 1, 0, 2000, 0, -1)

###############################################################################
# Check that PAM overrides internal GCPs (#5279)


def test_jp2lura_13():

    # Create a dataset with GCPs
    src_ds = gdal.Open('data/rgb_gcp.vrt')
    ds = gdaltest.jp2lura_drv.CreateCopy('tmp/jp2lura_13.jp2', src_ds)
    ds = None
    src_ds = None

    assert gdal.VSIStatL('tmp/jp2lura_13.jp2.aux.xml') is None

    ds = gdal.Open('tmp/jp2lura_13.jp2')
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    assert count == 4
    assert len(gcps) == 4
    assert '4326' in wkt
    ds = None

    # Override GCP
    ds = gdal.Open('tmp/jp2lura_13.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2lura_13.jp2')
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    ds = None

    gdaltest.jp2lura_drv.Delete('tmp/jp2lura_13.jp2')

    assert count == 1
    assert len(gcps) == 1
    assert '32631' in wkt

###############################################################################
# Check that we get GCPs even there's no projection info


def test_jp2lura_14():

    ds = gdal.Open('data/jpeg2000/byte_2gcps.jp2')
    assert ds.GetGCPCount() == 2

###############################################################################
# Test reading PixelIsPoint file (#5437)


def test_jp2lura_16():

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


def test_jp2lura_17():

    src_ds = gdal.Open('data/jpeg2000/byte_point.jp2')
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_17.jp2', src_ds)
    ds = None
    src_ds = None

    assert gdal.VSIStatL('/vsimem/jp2lura_17.jp2.aux.xml') is None

    ds = gdal.Open('/vsimem/jp2lura_17.jp2')
    gt = ds.GetGeoTransform()
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Point', \
        'did not get AREA_OR_POINT = Point'
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, 'did not get expected geotransform'

    gdal.Unlink('/vsimem/jp2lura_17.jp2')

###############################################################################
# Test when using the decode_area API when one dimension of the dataset is not a
# multiple of 1024 (#5480)


def test_jp2lura_18():

    src_ds = gdal.GetDriverByName('Mem').Create('', 2000, 2000)
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_18.jp2', src_ds, options=['TILEXSIZE=2000', 'TILEYSIZE=2000'])
    ds = None
    src_ds = None

    ds = gdal.Open('/vsimem/jp2lura_18.jp2')
    ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    gdal.Unlink('/vsimem/jp2lura_18.jp2')

###############################################################################
# Test reading file where GMLJP2 has nul character instead of \n (#5760)


def test_jp2lura_19():

    ds = gdal.Open('data/jpeg2000/byte_gmljp2_with_nul_car.jp2')
    assert ds.GetProjectionRef() != ''
    ds = None

###############################################################################
# Validate GMLJP2 content against schema


def test_jp2lura_20():

    try:
        import xmlvalidate
    except ImportError:
        import traceback
        traceback.print_exc(file=sys.stdout)
        pytest.skip('Cannot import xmlvalidate')

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
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_20.jp2', src_ds)
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    ds = None
    gdal.Unlink('/vsimem/jp2lura_20.jp2')

    assert xmlvalidate.validate(gmljp2, ogc_schemas_location='tmp/cache/SCHEMAS_OPENGIS_NET')

###############################################################################
# Test RGBA support


def test_jp2lura_22():

    # RGBA
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options=['REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2lura_22.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    assert validate('/vsimem/jp2lura_22.jp2', expected_gmljp2=False, inspire_tg=False) != 'fail'

    gdal.Unlink('/vsimem/jp2lura_22.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

    if False:  # pylint: disable=using-constant-test
        # RGBA with 1BIT_ALPHA=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options=['1BIT_ALPHA=YES'])
        del out_ds
        src_ds = None
        assert gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is None
        ds = gdal.OpenEx('/vsimem/jp2lura_22.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
        fourth_band = ds.GetRasterBand(4)
        assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'
        ds = None
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        assert ds.GetRasterBand(4).Checksum() == 23120
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        # RGBA with YCBCR420=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options=['YCBCR420=YES'])
        del out_ds
        src_ds = None
        assert gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
        assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
        assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
        assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
        assert ds.GetRasterBand(1).Checksum() == 11457
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        # RGBA with YCC=YES. Will emit a warning for now because of OpenJPEG
        # bug (only fixed in trunk, not released versions at that time)
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options=['YCC=YES', 'QUALITY=100', 'REVERSIBLE=YES'])
        maxdiff = gdaltest.compare_ds(src_ds, out_ds)
        del out_ds
        src_ds = None
        assert gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        assert maxdiff <= 0, 'Image too different from reference'

        # RGB,undefined
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES'])
        maxdiff = gdaltest.compare_ds(src_ds, out_ds)
        del out_ds
        src_ds = None
        assert gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
        assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
        assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
        assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        assert maxdiff <= 0, 'Image too different from reference'

        # RGB,undefined with ALPHA=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options=['QUALITY=100', 'REVERSIBLE=YES', 'ALPHA=YES'])
        maxdiff = gdaltest.compare_ds(src_ds, out_ds)
        del out_ds
        src_ds = None
        assert gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
        assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
        assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
        assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        assert maxdiff <= 0, 'Image too different from reference'


###############################################################################
# Test NBITS support


def DISABLED_jp2lura_23():

    src_ds = gdal.Open('../gcore/data/uint16.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_23.jp2', src_ds, options=['NBITS=9', 'QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2lura_23.jp2')
    assert ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '9'

    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_23_2.jp2', ds)
    assert out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '9'
    del out_ds

    ds = None
    assert gdal.VSIStatL('/vsimem/jp2lura_23.jp2.aux.xml') is None
    gdal.Unlink('/vsimem/jp2lura_23.jp2')
    gdal.Unlink('/vsimem/jp2lura_23_2.jp2')

    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# Test Grey+alpha support


def test_jp2lura_24():

    #  Grey+alpha
    src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_24.jp2', src_ds, options=['REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL('/vsimem/jp2lura_24.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2lura_24.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    assert validate('/vsimem/jp2lura_24.jp2', expected_gmljp2=False, inspire_tg=False) != 'fail'

    gdal.Unlink('/vsimem/jp2lura_24.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

    if False:  # pylint: disable=using-constant-test
        #  Grey+alpha with 1BIT_ALPHA=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
        gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_24.jp2', src_ds, options=['1BIT_ALPHA=YES'])

        src_ds = None
        assert gdal.VSIStatL('/vsimem/jp2lura_24.jp2.aux.xml') is None
        ds = gdal.OpenEx('/vsimem/jp2lura_24.jp2', open_options=['1BIT_ALPHA_PROMOTION=NO'])
        assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
        assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
        assert ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'
        ds = None
        ds = gdal.Open('/vsimem/jp2lura_24.jp2')
        assert ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is None
        assert ds.GetRasterBand(2).Checksum() == 23120
        ds = None
        gdal.Unlink('/vsimem/jp2lura_24.jp2')


###############################################################################
# Test multiband support


def test_jp2lura_25():

    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 5)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(250)
    src_ds.GetRasterBand(3).Fill(245)
    src_ds.GetRasterBand(4).Fill(240)
    src_ds.GetRasterBand(5).Fill(235)

    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_25.jp2', src_ds, options=['REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2lura_25.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_Undefined
    ds = None
    assert gdal.VSIStatL('/vsimem/jp2lura_25.jp2.aux.xml') is None

    gdal.Unlink('/vsimem/jp2lura_25.jp2')

    assert maxdiff <= 0, 'Image too different from reference'

###############################################################################
# Test CreateCopy() from a JPEG2000 with a 2048x2048 tiling


def test_jp2lura_27():

    # Test optimization in GDALCopyWholeRasterGetSwathSize()
    # Not sure how we can check that except looking at logs with CPL_DEBUG=GDAL
    # for "GDAL: GDALDatasetCopyWholeRaster(): 2048*2048 swaths, bInterleave=1"
    src_ds = gdal.GetDriverByName('MEM').Create('', 2049, 2049, 4)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_27.jp2', src_ds, options=['LEVELS=1', 'TILEXSIZE=2048', 'TILEYSIZE=2048'])
    src_ds = None
    # print('End of JP2 decoding')
    out2_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2lura_27.tif', out_ds, options=['TILED=YES'])
    out_ds = None
    del out2_ds
    gdal.Unlink('/vsimem/jp2lura_27.jp2')
    gdal.Unlink('/vsimem/jp2lura_27.tif')

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


def jp2lura_test_codeblock(filename, codeblock_width, codeblock_height):
    node = gdal.GetJPEG2000Structure(filename, ['ALL=YES'])
    xcb = 2**(2 + int(get_element_val(find_element_with_name(node, "Field", "SPcod_xcb_minus_2"))))
    ycb = 2**(2 + int(get_element_val(find_element_with_name(node, "Field", "SPcod_ycb_minus_2"))))
    if xcb != codeblock_width or ycb != codeblock_height:
        return False
    return True


def test_jp2lura_28():

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
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_28.jp2', src_ds, options=options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            print(options)
            pytest.fail('warning expected')
        del out_ds
        if not jp2lura_test_codeblock('/vsimem/jp2lura_28.jp2', expected_cbkw, expected_cbkh):
            print(options)
            pytest.fail('unexpected codeblock size')

    gdal.Unlink('/vsimem/jp2lura_28.jp2')

###############################################################################
# Test color table support


def test_jp2lura_30():

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))
    src_ds.GetRasterBand(1).SetRasterColorTable(ct)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_30.jp2', src_ds)
    gdal.PopErrorHandler()
    assert out_ds is None

###############################################################################
# Test unusual band color interpretation order


def DISABLED_jp2lura_31():

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 3)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_31.jp2', src_ds)
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2lura_31.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2lura_31.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None
    gdal.Unlink('/vsimem/jp2lura_31.jp2')

    # With alpha now
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 4)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_31.jp2', src_ds)
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2lura_31.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2lura_31.jp2')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None
    gdal.Unlink('/vsimem/jp2lura_31.jp2')

###############################################################################
# Test crazy tile size


def DISABLED_jp2lura_33():

    src_ds = gdal.Open("""<VRTDataset rasterXSize="100000" rasterYSize="100000">
  <VRTRasterBand dataType="Byte" band="1">
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_33.jp2', src_ds, options=['BLOCKXSIZE=100000', 'BLOCKYSIZE=100000'])
    gdal.PopErrorHandler()
    assert out_ds is None
    out_ds = None
    gdal.Unlink('/vsimem/jp2lura_33.jp2')

###############################################################################
# Test opening a file whose dimensions are > 2^31-1


def test_jp2lura_34():

    gdal.PushErrorHandler()
    ds = gdal.Open('data/jpeg2000/dimensions_above_31bit.jp2')
    gdal.PopErrorHandler()
    assert ds is None


###############################################################################
# Test opening a truncated file

def test_jp2lura_35():

    gdal.PushErrorHandler()
    ds = gdal.Open('data/jpeg2000/truncated.jp2')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Test we cannot create files with more than 16384 bands


def test_jp2lura_36():

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 16385)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_36.jp2', src_ds)
    gdal.PopErrorHandler()
    assert out_ds is None and gdal.VSIStatL('/vsimem/jp2lura_36.jp2') is None

###############################################################################
# Test metadata reading & writing


def test_jp2lura_37():

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options=['WRITE_METADATA=YES'])
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    assert ds.GetMetadata() == {}
    gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Simple metadata in main domain
    for options in [['WRITE_METADATA=YES']]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadataItem('FOO', 'BAR')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options=options)
        del out_ds
        assert gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2lura_37.jp2')
        assert ds.GetMetadata() == {'FOO': 'BAR'}
        ds = None

        gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Simple metadata in auxiliary domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadataItem('FOO', 'BAR', 'SOME_DOMAIN')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options=['WRITE_METADATA=YES'])
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    md = ds.GetMetadata('SOME_DOMAIN')
    assert md == {'FOO': 'BAR'}
    gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Simple metadata in auxiliary XML domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadata(['<some_arbitrary_xml_box/>'], 'xml:SOME_DOMAIN')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options=['WRITE_METADATA=YES'])
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is None
    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    assert ds.GetMetadata('xml:SOME_DOMAIN')[0] == '<some_arbitrary_xml_box />\n'
    gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Special xml:BOX_ metadata domain
    for options in [['WRITE_METADATA=YES']]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata(['<some_arbitrary_xml_box/>'], 'xml:BOX_1')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options=options)
        del out_ds
        assert gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2lura_37.jp2')
        assert ds.GetMetadata('xml:BOX_0')[0] == '<some_arbitrary_xml_box/>'
        gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Special xml:XMP metadata domain
    for options in [['WRITE_METADATA=YES']]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata(['<fake_xmp_box/>'], 'xml:XMP')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options=options)
        del out_ds
        assert gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is None
        ds = gdal.Open('/vsimem/jp2lura_37.jp2')
        assert ds.GetMetadata('xml:XMP')[0] == '<fake_xmp_box/>'
        ds = None

        gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Special xml:IPR metadata domain
    # for options in [ ['WRITE_METADATA=YES'] ]:
    #    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    #    src_ds.SetMetadata( [ '<fake_ipr_box/>' ], 'xml:IPR')
    #    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = options)
    #    del out_ds
    #    if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
    #        gdaltest.post_reason('fail')
    #        return 'fail'
    #    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    #    if ds.GetMetadata('xml:IPR')[0] != '<fake_ipr_box/>':
    #        gdaltest.post_reason('fail')
    #        return 'fail'
    #    ds = None

        gdal.Unlink('/vsimem/jp2lura_37.jp2')


###############################################################################
# Test non-EPSG SRS (so written with a GML dictionary)


def test_jp2lura_38():

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    wkt = """PROJCS["UTM Zone 31, Northern Hemisphere",GEOGCS["unnamed ellipse",DATUM["unknown",SPHEROID["unnamed",100,1]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"""
    src_ds.SetProjection(wkt)
    src_ds.SetGeoTransform([0, 60, 0, 0, 0, -60])
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_38.jp2', src_ds, options=['GeoJP2=NO'])
    assert out_ds.GetProjectionRef() == wkt
    crsdictionary = out_ds.GetMetadata_List("xml:CRSDictionary.gml")[0]
    out_ds = None
    gdal.Unlink('/vsimem/jp2lura_38.jp2')

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


def test_jp2lura_39():

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
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_39.jp2', src_ds, options=['GeoJP2=NO'])
    gdal.SetConfigOption('GMLJP2OVERRIDE', None)
    gdal.Unlink('/vsimem/override.gml')
    del out_ds
    ds = gdal.Open('/vsimem/jp2lura_39.jp2')
    assert ds.GetProjectionRef().find('4326') >= 0
    ds = None
    gdal.Unlink('/vsimem/jp2lura_39.jp2')

###############################################################################
# Test we can parse GMLJP2 v2.0


def test_jp2lura_40():

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
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_40.jp2', src_ds, options=['GeoJP2=NO'])
    gdal.SetConfigOption('GMLJP2OVERRIDE', None)
    gdal.Unlink('/vsimem/override.gml')
    del out_ds
    ds = gdal.Open('/vsimem/jp2lura_40.jp2')
    assert ds.GetProjectionRef().find('4326') >= 0
    got_gt = ds.GetGeoTransform()
    expected_gt = (2, 0.1, 0, 49, 0, -0.1)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-5)
    ds = None
    gdal.Unlink('/vsimem/jp2lura_40.jp2')

###############################################################################
# Test USE_SRC_CODESTREAM=YES


def test_jp2lura_41():

    src_ds = gdal.Open('data/jpeg2000/byte.jp2')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_41.jp2', src_ds,
                                             options=['USE_SRC_CODESTREAM=YES', '@PROFILE=PROFILE_1', 'GEOJP2=NO', 'GMLJP2=NO'])
    assert src_ds.GetRasterBand(1).Checksum() == out_ds.GetRasterBand(1).Checksum()
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2lura_41.jp2').size == 9923
    gdal.Unlink('/vsimem/jp2lura_41.jp2')
    gdal.Unlink('/vsimem/jp2lura_41.jp2.aux.xml')

    # Warning if ignored option
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_41.jp2', src_ds,
                                             options=['USE_SRC_CODESTREAM=YES', 'QUALITY=1'])
    gdal.PopErrorHandler()
    del out_ds
    # if gdal.GetLastErrorMsg() == '':
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    gdal.Unlink('/vsimem/jp2lura_41.jp2')
    gdal.Unlink('/vsimem/jp2lura_41.jp2.aux.xml')

    # Warning if source is not JPEG2000
    src_ds = gdal.Open('data/byte.tif')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_41.jp2', src_ds,
                                             options=['USE_SRC_CODESTREAM=YES'])
    gdal.PopErrorHandler()
    del out_ds
    assert gdal.GetLastErrorMsg() != ''
    gdal.Unlink('/vsimem/jp2lura_41.jp2')

###############################################################################
# Get structure of a JPEG2000 file


def test_jp2lura_43():

    ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte.jp2', ['ALL=YES'])
    assert ret is not None

###############################################################################
# Test GMLJP2v2


def test_jp2lura_45():

    if gdal.GetDriverByName('GML') is None:
        pytest.skip()
    if gdal.GetDriverByName('KML') is None and gdal.GetDriverByName('LIBKML') is None:
        pytest.skip()

    # Test GMLJP2V2_DEF=YES
    src_ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_45.jp2', src_ds, options=['GMLJP2V2_DEF=YES'])
    assert out_ds.GetLayerCount() == 0
    assert out_ds.GetLayer(0) is None
    del out_ds

    ds = gdal.Open('/vsimem/jp2lura_45.jp2')
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

    gdal.Unlink('/vsimem/jp2lura_45.jp2')

###############################################################################
# Test writing & reading RPC in GeoJP2 box


def test_jp2lura_47():

    src_ds = gdal.Open('../gcore/data/byte_rpc.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_47.jp2', src_ds)
    del out_ds
    assert gdal.VSIStatL('/vsimem/jp2lura_47.jp2.aux.xml') is None

    ds = gdal.Open('/vsimem/jp2lura_47.jp2')
    assert ds.GetMetadata('RPC') is not None
    ds = None

    gdal.Unlink('/vsimem/jp2lura_47.jp2')

###############################################################################
# Test reading a dataset whose tile dimensions are larger than dataset ones


def test_jp2lura_48():

    ds = gdal.Open('data/jpeg2000/byte_tile_2048.jp2')
    (blockxsize, blockysize) = ds.GetRasterBand(1).GetBlockSize()
    assert (blockxsize, blockysize) == (20, 20)
    assert ds.GetRasterBand(1).Checksum() == 4610
    ds = None

###############################################################################


def test_jp2lura_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        pytest.skip()

    # Checksum = 32669 on my PC
    tst = gdaltest.GDALTest('JP2Lura', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

###############################################################################


def test_jp2lura_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        pytest.skip()

    # Checksum = 15621 on my PC
    tst = gdaltest.GDALTest('JP2Lura', 'tmp/cache/gcp.jp2', 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/gcp.jp2')
    ds.GetRasterBand(1).Checksum()
    assert len(ds.GetGCPs()) == 15, 'bad number of GCP'

    expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]"""
    assert ds.GetGCPProjection() == expected_wkt, 'bad GCP projection'

    ds = None

###############################################################################


def test_jp2lura_online_3():

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        pytest.skip()

    tst = gdaltest.GDALTest('JP2Lura', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute=1)

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


def test_jp2lura_online_4():

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        pytest.skip()
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        pytest.skip()

    tst = gdaltest.GDALTest('JP2Lura', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute=1)

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
# Try reading JP2Lura with color table


def test_jp2lura_online_5():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9.jp2', 'file9.jp2'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/file9.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    assert cs1 == 47664, 'Did not get expected checksums'
    assert ds.GetRasterBand(1).GetColorTable() is not None, \
        'Did not get expected color table'
    ds = None

###############################################################################
# Try reading YCbCr JP2Lura as RGB


def test_jp2lura_online_6():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_03/file3.jp2', 'file3.jp2'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/file3.jp2')
    # cs1 = ds.GetRasterBand(1).Checksum()
    # cs2 = ds.GetRasterBand(2).Checksum()
    # cs3 = ds.GetRasterBand(3).Checksum()
    # if cs1 != 26140 or cs2 != 32689 or cs3 != 48247:
    #    print(cs1, cs2, cs3)
    #    gdaltest.post_reason('Did not get expected checksums')
    #    return 'fail'
    assert ds is None
    ds = None

###############################################################################
# Test GDAL_GEOREF_SOURCES


def test_jp2lura_49():

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
# Test reading split IEEE-754 Float32

def test_jp2lura_50():

    tst = gdaltest.GDALTest('JP2Lura', 'jpeg2000/float32_ieee754_split_reversible.jp2', 1, 4672)
    return tst.testOpen()

###############################################################################
# Test split IEEE-754 Float32


def test_jp2lura_51():

    # Don't allow it by default
    src_ds = gdal.Open('data/float32.tif')
    with gdaltest.error_handler():
        ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds)
    assert ds is None

    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                         options=['SPLIT_IEEE754=YES'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None

    assert validate('/vsimem/jp2lura_51.jp2', inspire_tg=False) != 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')

    assert maxdiff <= 0.01

    # QUALITY
    with gdaltest.error_handler():
        ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                             options=['SPLIT_IEEE754=YES', 'QUALITY=100'])
    if ds is not None:
        maxdiff = gdaltest.compare_ds(ds, src_ds)
        ds = None
        assert maxdiff <= 124

        assert validate('/vsimem/jp2lura_51.jp2', inspire_tg=False) != 'fail'
    ds = None
    with gdaltest.error_handler():
        gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')
    gdal.Unlink('/vsimem/jp2lura_51.jp2')

    # RATE
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                         options=['SPLIT_IEEE754=YES', 'RATE=1'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    assert maxdiff <= 370

    assert validate('/vsimem/jp2lura_51.jp2', inspire_tg=False) != 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')

    # Test reversible
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                         options=['SPLIT_IEEE754=YES', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    assert maxdiff == 0.0

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')

###############################################################################
# Test other data types


def test_jp2lura_52():

    tests = [[-32768, gdal.GDT_Int16, 'h'],
             [-1, gdal.GDT_Int16, 'h'],
             [32767, gdal.GDT_Int16, 'h'],
             [0, gdal.GDT_UInt16, 'H'],
             [65535, gdal.GDT_UInt16, 'H'],
             [-2 ** 27, gdal.GDT_Int32, 'i'],
             [2 ** 27 - 1, gdal.GDT_Int32, 'i'],
             [0, gdal.GDT_UInt32, 'I'],
             [2 ** 28 - 1, gdal.GDT_UInt32, 'I'],
            ]
    for (val, dt, fmt) in tests:

        src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1, dt)
        src_ds.GetRasterBand(1).Fill(val)
        ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_52.jp2', src_ds,
                                             options=['REVERSIBLE=YES'])
        got_min, got_max = ds.GetRasterBand(1).ComputeRasterMinMax()
        assert val == got_min and val == got_max, (val, dt, fmt, got_min, got_max)
        ds = None

        assert not (val >= 0 and validate('/vsimem/jp2lura_52.jp2', expected_gmljp2=False, inspire_tg=False) == 'fail'), \
            (val, dt, fmt)

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_52.jp2')

###############################################################################
# Test RATE and QUALITY


def test_jp2lura_53():

    src_ds = gdal.Open('data/byte.tif')

    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options=['RATE=1'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    assert maxdiff <= 8

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')

    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options=['QUALITY=100'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    assert maxdiff <= 2

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')

    # Forcing irreversible due to RATE
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options=['REVERSIBLE=YES', 'RATE=1'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    assert maxdiff <= 8

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')

    # QUALITY ignored
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options=['REVERSIBLE=YES', 'QUALITY=100'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    assert maxdiff <= 0

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')

###############################################################################
# Test RasterIO edge cases


def test_jp2lura_54():

    # Tiled with incomplete boundary tiles
    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 1)
    src_ds.GetRasterBand(1).Fill(100)
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_54.jp2', src_ds,
                                         options=['REVERSIBLE=YES', 'TILEXSIZE=64', 'TILEYSIZE=64'])
    # Request with a type that is not the natural type
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 100, 100, 100, 100,
                                          buf_type=gdal.GDT_Int16)
    data = struct.unpack('h' * 100 * 100, data)
    assert min(data) == 100 and max(data) == 100

    # Request at a resolution that is not a power of two
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 100, 100, 30, 30)
    data = struct.unpack('B' * 30 * 30, data)
    assert min(data) == 100 and max(data) == 100

    ds = None

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_54.jp2')
