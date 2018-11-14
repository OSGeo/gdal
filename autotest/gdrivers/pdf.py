#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PDF Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

import gdaltest
import ogrtest


pytestmark = pytest.mark.require_driver('PDF')

###############################################################################
# Test driver presence


@pytest.fixture(
    params=['POPPLER', 'PODOFO', 'PDFIUM']
)
def poppler_or_pdfium_or_podofo(request):
    """
    Runs tests with all three backends.
    """
    backend = request.param
    gdaltest.pdf_drv = gdal.GetDriverByName('PDF')

    md = gdaltest.pdf_drv.GetMetadata()
    if 'HAVE_%s' % backend not in md:
        pytest.skip()

    with gdaltest.config_option('GDAL_PDF_LIB', backend):
        yield backend


@pytest.fixture(
    params=['POPPLER', 'PDFIUM']
)
def poppler_or_pdfium(request):
    """
    Runs tests with poppler or pdfium, but not podofo
    """
    backend = request.param
    gdaltest.pdf_drv = gdal.GetDriverByName('PDF')

    md = gdaltest.pdf_drv.GetMetadata()
    if 'HAVE_%s' % backend not in md:
        pytest.skip()

    with gdaltest.config_option('GDAL_PDF_LIB', backend):
        yield backend

###############################################################################
# Returns True if we run with poppler


def pdf_is_poppler():

    md = gdaltest.pdf_drv.GetMetadata()
    val = gdal.GetConfigOption("GDAL_PDF_LIB", "POPPLER")
    if val == 'POPPLER' and 'HAVE_POPPLER' in md:
        return not pdf_is_pdfium()
    return False

###############################################################################
# Returns True if we run with pdfium


def pdf_is_pdfium():

    md = gdaltest.pdf_drv.GetMetadata()
    val = gdal.GetConfigOption("GDAL_PDF_LIB", "PDFIUM")
    if val == 'PDFIUM' and 'HAVE_PDFIUM' in md:
        return True
    return False

###############################################################################
# Returns True if we can compute the checksum


def pdf_checksum_available():

    try:
        ret = gdaltest.pdf_is_checksum_available
        if ret is True or ret is False:
            return ret
    except AttributeError:
        pass

    if pdf_is_poppler() or pdf_is_pdfium():
        gdaltest.pdf_is_checksum_available = True
        return gdaltest.pdf_is_checksum_available

    (_, err) = gdaltest.runexternal_out_and_err("pdftoppm -v")
    if err.startswith('pdftoppm version'):
        gdaltest.pdf_is_checksum_available = True
        return gdaltest.pdf_is_checksum_available
    print('Cannot compute to checksum due to missing pdftoppm')
    print(err)
    gdaltest.pdf_is_checksum_available = False
    return gdaltest.pdf_is_checksum_available

###############################################################################
# Test OGC best practice geospatial PDF


def test_pdf_online_1(poppler_or_pdfium):
    if not gdaltest.download_file('http://www.agc.army.mil/GeoPDFgallery/Imagery/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf', 'Cherrydale_eDOQQ_1m_0_033_R1C1.pdf'):
        pytest.skip()

    try:
        os.stat('tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    except OSError:
        pytest.skip()

    ds = gdal.Open('tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    assert ds is not None

    assert ds.RasterXSize == 1241, 'bad dimensions'

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    if pdf_is_pdfium():
        expected_gt = (-77.11232757568358, 9.1663393281356228e-06, 0.0, 38.897842406247477, 0.0, -9.1665025563464202e-06)
    elif pdf_is_poppler():
        expected_gt = (-77.112328333299999, 9.1666559999999995e-06, 0.0, 38.897842488372, -0.0, -9.1666559999999995e-06)
    else:
        expected_gt = (-77.112328333299956, 9.1666560000051172e-06, 0.0, 38.897842488371978, 0.0, -9.1666560000046903e-06)

    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            # The remote file has been updated...
            other_expected_gt = (-77.112328333299928, 9.1666560000165691e-06, 0.0, 38.897842488371978, 0.0, -9.1666560000046903e-06)
            for j in range(6):
                assert abs(gt[j] - other_expected_gt[j]) <= 1e-15, 'bad geotransform'

    assert wkt.startswith('GEOGCS["WGS 84"'), 'bad WKT'

    if pdf_checksum_available():
        cs = ds.GetRasterBand(1).Checksum()
        assert cs != 0, 'bad checksum'

    
###############################################################################


def test_pdf_online_2(poppler_or_pdfium):
    try:
        os.stat('tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    except OSError:
        pytest.skip()

    ds = gdal.Open('PDF:1:tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    assert ds is not None

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    if pdf_is_pdfium():
        expected_gt = (-77.11232757568358, 9.1663393281356228e-06, 0.0, 38.897842406247477, 0.0, -9.1665025563464202e-06)
    elif pdf_is_poppler():
        expected_gt = (-77.112328333299999, 9.1666559999999995e-06, 0.0, 38.897842488372, -0.0, -9.1666559999999995e-06)
    else:
        expected_gt = (-77.112328333299956, 9.1666560000051172e-06, 0.0, 38.897842488371978, 0.0, -9.1666560000046903e-06)

    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            # The remote file has been updated...
            other_expected_gt = (-77.112328333299928, 9.1666560000165691e-06, 0.0, 38.897842488371978, 0.0, -9.1666560000046903e-06)
            for j in range(6):
                assert abs(gt[j] - other_expected_gt[j]) <= 1e-15, 'bad geotransform'

    assert wkt.startswith('GEOGCS["WGS 84"')

###############################################################################
# Test Adobe style geospatial pdf


def test_pdf_1(poppler_or_pdfium):
    gdal.SetConfigOption('GDAL_PDF_DPI', '200')
    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    gdal.SetConfigOption('GDAL_PDF_DPI', None)
    assert ds is not None

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    if pdf_is_pdfium():
        expected_gt = (333275.12406585668, 31.764450118407499, 0.0, 4940392.1233656602, 0.0, -31.794983670894396)
    else:
        expected_gt = (333274.61654367246, 31.764802242655662, 0.0, 4940391.7593506984, 0.0, -31.794745501708238)

    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-6, 'bad geotransform'

    expected_wkt = 'PROJCS["WGS 84 / UTM zone 20N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-63],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    assert wkt == expected_wkt, 'bad WKT'

    if pdf_checksum_available():
        cs = ds.GetRasterBand(1).Checksum()
        # if cs != 17740 and cs != 19346:
        assert cs != 0, 'bad checksum'

    neatline = ds.GetMetadataItem('NEATLINE')
    got_geom = ogr.CreateGeometryFromWkt(neatline)
    if pdf_is_pdfium():
        expected_geom = ogr.CreateGeometryFromWkt('POLYGON ((338304.28536533244187 4896674.10591614805162,338304.812550922040828 4933414.853961281478405,382774.246895745047368 4933414.855149634182453,382774.983309225703124 4896673.95723026804626,338304.28536533244187 4896674.10591614805162))')
    else:
        expected_geom = ogr.CreateGeometryFromWkt('POLYGON ((338304.150125828920864 4896673.639421294443309,338304.177293475600891 4933414.799376524984837,382774.271384406310972 4933414.546264361590147,382774.767329963855445 4896674.273581005632877,338304.150125828920864 4896673.639421294443309))')

    if ogrtest.check_feature_geometry(got_geom, expected_geom) != 0:
        print(neatline)
        pytest.fail('bad neatline')

    
###############################################################################
# Test write support with ISO32000 geo encoding


def test_pdf_iso32000(poppler_or_pdfium_or_podofo):
    tst = gdaltest.GDALTest('PDF', 'byte.tif', 1, None)
    ret = tst.testCreateCopy(check_minmax=0, check_gt=1, check_srs=True, check_checksum_not_null=pdf_checksum_available())

    return ret

###############################################################################
# Test write support with ISO32000 geo encoding, with DPI=300


def test_pdf_iso32000_dpi_300(poppler_or_pdfium):
    tst = gdaltest.GDALTest('PDF', 'byte.tif', 1, None, options=['DPI=300'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=1, check_srs=True, check_checksum_not_null=pdf_checksum_available())

    return ret

###############################################################################
# Test write support with OGC_BP geo encoding


def test_pdf_ogcbp(poppler_or_pdfium_or_podofo):
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    tst = gdaltest.GDALTest('PDF', 'byte.tif', 1, None, options=['GEO_ENCODING=OGC_BP'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=1, check_srs=True, check_checksum_not_null=pdf_checksum_available())
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)

    return ret

###############################################################################
# Test write support with OGC_BP geo encoding, with DPI=300


def test_pdf_ogcbp_dpi_300(poppler_or_pdfium):
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    tst = gdaltest.GDALTest('PDF', 'byte.tif', 1, None, options=['GEO_ENCODING=OGC_BP', 'DPI=300'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=1, check_srs=True, check_checksum_not_null=pdf_checksum_available())
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)

    return ret


def test_pdf_ogcbp_lcc(poppler_or_pdfium):
    wkt = """PROJCS["NAD83 / Utah North",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",41.78333333333333],
    PARAMETER["standard_parallel_2",40.71666666666667],
    PARAMETER["latitude_of_origin",40.33333333333334],
    PARAMETER["central_meridian",-111.5],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",1000000],
    UNIT["metre",1]]]"""

    src_ds = gdal.GetDriverByName('GTiff').Create('tmp/temp.tif', 1, 1)
    src_ds.SetProjection(wkt)
    src_ds.SetGeoTransform([500000, 1, 0, 1000000, 0, -1])

    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_ogcbp_lcc.pdf', src_ds)
    out_wkt = out_ds.GetProjectionRef()
    out_ds = None
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)

    src_ds = None

    gdal.Unlink('tmp/temp.tif')
    gdal.GetDriverByName('PDF').Delete('tmp/pdf_ogcbp_lcc.pdf')

    sr1 = osr.SpatialReference(wkt)
    sr2 = osr.SpatialReference(out_wkt)
    if sr1.IsSame(sr2) == 0:
        print(sr2.ExportToPrettyWkt())
        pytest.fail('wrong wkt')

    
###############################################################################
# Test no compression


def test_pdf_no_compression(poppler_or_pdfium):
    tst = gdaltest.GDALTest('PDF', 'byte.tif', 1, None, options=['COMPRESS=NONE'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())

    return ret

###############################################################################
# Test compression methods


def _test_pdf_jpeg_compression(filename):
    if gdal.GetDriverByName('JPEG') is None:
        pytest.skip()

    tst = gdaltest.GDALTest('PDF', filename, 1, None, options=['COMPRESS=JPEG'])
    tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())


def test_pdf_jpeg_compression(poppler_or_pdfium):
    _test_pdf_jpeg_compression('byte.tif')


def pdf_get_J2KDriver(drv_name):
    drv = gdal.GetDriverByName(drv_name)
    if drv is None:
        return None
    if drv_name == 'JP2ECW':
        import ecw
        if not ecw.has_write_support():
            return None
    return drv


def pdf_jpx_compression(filename, drv_name=None):
    if drv_name is None:
        if pdf_get_J2KDriver('JP2KAK') is None and \
           pdf_get_J2KDriver('JP2ECW') is None and \
           pdf_get_J2KDriver('JP2OpenJpeg') is None and \
           pdf_get_J2KDriver('JPEG2000') is None:
            pytest.skip()
    elif pdf_get_J2KDriver(drv_name) is None:
        pytest.skip()

    if drv_name is None:
        options = ['COMPRESS=JPEG2000']
    else:
        options = ['COMPRESS=JPEG2000', 'JPEG2000_DRIVER=%s' % drv_name]

    tst = gdaltest.GDALTest('PDF', filename, 1, None, options=options)
    ret = tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())

    return ret


def test_pdf_jp2_auto_compression(poppler_or_pdfium):
    return pdf_jpx_compression('utm.tif')


def test_pdf_jp2kak_compression(poppler_or_pdfium):
    return pdf_jpx_compression('utm.tif', 'JP2KAK')


def test_pdf_jp2ecw_compression(poppler_or_pdfium):
    return pdf_jpx_compression('utm.tif', 'JP2ECW')


def test_pdf_jp2openjpeg_compression(poppler_or_pdfium):
    return pdf_jpx_compression('utm.tif', 'JP2OpenJpeg')


def test_pdf_jpeg2000_compression(poppler_or_pdfium):
    return pdf_jpx_compression('utm.tif', 'JPEG2000')


def test_pdf_jp2ecw_compression_rgb(poppler_or_pdfium):
    return pdf_jpx_compression('rgbsmall.tif', 'JP2ECW')


def test_pdf_jpeg_compression_rgb(poppler_or_pdfium):
    return _test_pdf_jpeg_compression('rgbsmall.tif')

###############################################################################
# Test RGBA


def pdf_rgba_default_compression(options_param=None):
    options_param = [] if options_param is None else options_param
    if not pdf_checksum_available():
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/rgba.pdf', src_ds, options=options_param)
    out_ds = None

    # gdal.SetConfigOption('GDAL_PDF_BANDS', '4')
    gdal.SetConfigOption('PDF_DUMP_OBJECT', 'tmp/rgba.pdf.txt')
    gdal.SetConfigOption('PDF_DUMP_PARENT', 'YES')
    out_ds = gdal.Open('tmp/rgba.pdf')
    gdal.SetConfigOption('PDF_DUMP_OBJECT', None)
    gdal.SetConfigOption('PDF_DUMP_PARENT', None)
    content = open('tmp/rgba.pdf.txt', 'rt').read()
    os.unlink('tmp/rgba.pdf.txt')
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs2 = out_ds.GetRasterBand(2).Checksum()
    cs3 = out_ds.GetRasterBand(3).Checksum()
    if out_ds.RasterCount == 4:
        cs4 = out_ds.GetRasterBand(4).Checksum()
    else:
        cs4 = -1

    src_cs1 = src_ds.GetRasterBand(1).Checksum()
    src_cs2 = src_ds.GetRasterBand(2).Checksum()
    src_cs3 = src_ds.GetRasterBand(3).Checksum()
    src_cs4 = src_ds.GetRasterBand(4).Checksum()
    out_ds = None
    # gdal.SetConfigOption('GDAL_PDF_BANDS', None)

    gdal.GetDriverByName('PDF').Delete('tmp/rgba.pdf')

    if cs4 < 0:
        pytest.skip()

    assert (content.startswith('Type = dictionary, Num = 3, Gen = 0') and \
       '      Type = dictionary, Num = 3, Gen = 0' in content), \
        'wrong object dump'

    assert cs4 != 0, 'wrong checksum'

    if cs1 != src_cs1 or cs2 != src_cs2 or cs3 != src_cs3 or cs4 != src_cs4:
        print(cs1)
        print(cs2)
        print(cs3)
        print(cs4)
        print(src_cs1)
        print(src_cs2)
        print(src_cs3)
        print(src_cs4)

    

def test_pdf_rgba_default_compression_tiled(poppler_or_pdfium_or_podofo):
    return pdf_rgba_default_compression(['BLOCKXSIZE=32', 'BLOCKYSIZE=32'])


def test_pdf_jpeg_compression_rgba(poppler_or_pdfium):
    return _test_pdf_jpeg_compression('../../gcore/data/stefan_full_rgba.tif')

###############################################################################
# Test PREDICTOR=2


def test_pdf_predictor_2(poppler_or_pdfium):
    tst = gdaltest.GDALTest('PDF', 'utm.tif', 1, None, options=['PREDICTOR=2'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())

    return ret


def test_pdf_predictor_2_rgb(poppler_or_pdfium):
    tst = gdaltest.GDALTest('PDF', 'rgbsmall.tif', 1, None, options=['PREDICTOR=2'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())

    return ret

###############################################################################
# Test tiling


def test_pdf_tiled(poppler_or_pdfium):
    tst = gdaltest.GDALTest('PDF', 'utm.tif', 1, None, options=['COMPRESS=DEFLATE', 'TILED=YES'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())

    return ret


def test_pdf_tiled_128(poppler_or_pdfium):
    tst = gdaltest.GDALTest('PDF', 'utm.tif', 1, None, options=['BLOCKXSIZE=128', 'BLOCKYSIZE=128'])
    ret = tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())

    return ret

###############################################################################
# Test raster with color table


def test_pdf_color_table(poppler_or_pdfium):
    if gdal.GetDriverByName('GIF') is None:
        pytest.skip()

    tst = gdaltest.GDALTest('PDF', 'bug407.gif', 1, None)
    ret = tst.testCreateCopy(check_minmax=0, check_gt=0, check_srs=None, check_checksum_not_null=pdf_checksum_available())

    return ret

###############################################################################
# Test XMP support


def test_pdf_xmp(poppler_or_pdfium):
    src_ds = gdal.Open('data/adobe_style_geospatial_with_xmp.pdf')
    gdaltest.pdf_drv.CreateCopy('tmp/pdf_xmp.pdf', src_ds, options=['WRITE_INFO=NO'])
    out_ds = gdal.Open('tmp/pdf_xmp.pdf')
    if out_ds is None:
        # Some Poppler versions cannot re-open the file
        gdal.GetDriverByName('PDF').Delete('tmp/pdf_xmp.pdf')
        pytest.skip()

    ref_md = src_ds.GetMetadata('xml:XMP')
    got_md = out_ds.GetMetadata('xml:XMP')
    base_md = out_ds.GetMetadata()
    out_ds = None
    src_ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_xmp.pdf')

    assert ref_md[0] == got_md[0]

    assert len(base_md) == 2

###############################################################################
# Test Info


def test_pdf_info(poppler_or_pdfium):
    try:
        val = '\xc3\xa9'.decode('UTF-8')
    except:
        val = '\u00e9'

    options = [
        'AUTHOR=%s' % val,
        'CREATOR=creator',
        'KEYWORDS=keywords',
        'PRODUCER=producer',
        'SUBJECT=subject',
        'TITLE=title'
    ]

    src_ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_info.pdf', src_ds, options=options)
    # print(out_ds.GetMetadata())
    out_ds2 = gdaltest.pdf_drv.CreateCopy('tmp/pdf_info_2.pdf', out_ds)
    md = out_ds2.GetMetadata()
    # print(md)
    out_ds2 = None
    out_ds = None
    src_ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_info.pdf')
    gdal.GetDriverByName('PDF').Delete('tmp/pdf_info_2.pdf')

    assert (md['AUTHOR'] == val and \
       md['CREATOR'] == 'creator' and \
       md['KEYWORDS'] == 'keywords' and \
       md['PRODUCER'] == 'producer' and \
       md['SUBJECT'] == 'subject' and \
       md['TITLE'] == 'title'), "metadata doesn't match"

###############################################################################
# Check SetGeoTransform() / SetProjection()


def test_pdf_update_gt(poppler_or_pdfium_or_podofo):
    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_update_gt.pdf', src_ds)
    ds = None
    src_ds = None

    # Alter geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf', gdal.GA_Update)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds = None

    assert not os.path.exists('tmp/pdf_update_gt.pdf.aux.xml')

    # Check geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf')
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [2, 1, 0, 49, 0, -1]
    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-8, 'did not get expected gt'

    # Clear geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf', gdal.GA_Update)
    ds.SetProjection("")
    ds = None

    # Check geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf')
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-8, 'did not get expected gt'

    # Set geotransform again
    ds = gdal.Open('tmp/pdf_update_gt.pdf', gdal.GA_Update)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([3, 1, 0, 50, 0, -1])
    ds = None

    # Check geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf')
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [3, 1, 0, 50, 0, -1]
    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-8, 'did not get expected gt'

    gdaltest.pdf_drv.Delete('tmp/pdf_update_gt.pdf')

###############################################################################
# Check SetMetadataItem() for Info


def test_pdf_update_info(poppler_or_pdfium_or_podofo):
    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_update_info.pdf', src_ds)
    ds = None
    src_ds = None

    # Add info
    ds = gdal.Open('tmp/pdf_update_info.pdf', gdal.GA_Update)
    ds.SetMetadataItem('AUTHOR', 'author')
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_info.pdf')
    author = ds.GetMetadataItem('AUTHOR')
    ds = None

    assert author == 'author', 'did not get expected metadata'

    # Update info
    ds = gdal.Open('tmp/pdf_update_info.pdf', gdal.GA_Update)
    ds.SetMetadataItem('AUTHOR', 'author2')
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_info.pdf')
    author = ds.GetMetadataItem('AUTHOR')
    ds = None

    assert author == 'author2', 'did not get expected metadata'

    # Clear info
    ds = gdal.Open('tmp/pdf_update_info.pdf', gdal.GA_Update)
    ds.SetMetadataItem('AUTHOR', None)
    ds = None

    # Check PAM doesn't exist
    if os.path.exists('tmp/pdf_update_info.pdf.aux.xml'):
        print(author)
        pytest.fail('did not expected .aux.xml')

    # Check
    ds = gdal.Open('tmp/pdf_update_info.pdf')
    author = ds.GetMetadataItem('AUTHOR')
    ds = None

    assert author is None, 'did not get expected metadata'

    gdaltest.pdf_drv.Delete('tmp/pdf_update_info.pdf')

###############################################################################
# Check SetMetadataItem() for xml:XMP


def test_pdf_update_xmp(poppler_or_pdfium_or_podofo):
    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_update_xmp.pdf', src_ds)
    ds = None
    src_ds = None

    # Add info
    ds = gdal.Open('tmp/pdf_update_xmp.pdf', gdal.GA_Update)
    ds.SetMetadata(["<?xpacket begin='a'/><a/>"], "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_xmp.pdf')
    xmp = ds.GetMetadata('xml:XMP')[0]
    ds = None

    assert xmp == "<?xpacket begin='a'/><a/>", 'did not get expected metadata'

    # Update info
    ds = gdal.Open('tmp/pdf_update_xmp.pdf', gdal.GA_Update)
    ds.SetMetadata(["<?xpacket begin='a'/><a_updated/>"], "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_xmp.pdf')
    xmp = ds.GetMetadata('xml:XMP')[0]
    ds = None

    assert xmp == "<?xpacket begin='a'/><a_updated/>", 'did not get expected metadata'

    # Check PAM doesn't exist
    assert not os.path.exists('tmp/pdf_update_xmp.pdf.aux.xml'), \
        'did not expected .aux.xml'

    # Clear info
    ds = gdal.Open('tmp/pdf_update_xmp.pdf', gdal.GA_Update)
    ds.SetMetadata(None, "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_xmp.pdf')
    xmp = ds.GetMetadata('xml:XMP')
    ds = None

    assert xmp is None, 'did not get expected metadata'

    gdaltest.pdf_drv.Delete('tmp/pdf_update_xmp.pdf')

###############################################################################
# Check SetGCPs() but with GCPs that resolve to a geotransform


def _pdf_update_gcps(poppler_or_pdfium):
    dpi = 300
    out_filename = 'tmp/pdf_update_gcps.pdf'

    src_ds = gdal.Open('data/byte.tif')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()
    ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options=['GEO_ENCODING=NONE', 'DPI=%d' % dpi])
    ds = None
    src_ds = None

    gcp = [[2., 8., 0, 0],
           [2., 18., 0, 0],
           [16., 18., 0, 0],
           [16., 8., 0, 0]]

    for i in range(4):
        gcp[i][2] = src_gt[0] + gcp[i][0] * src_gt[1] + gcp[i][1] * src_gt[2]
        gcp[i][3] = src_gt[3] + gcp[i][0] * src_gt[4] + gcp[i][1] * src_gt[5]

    vrt_txt = """<VRTDataset rasterXSize="20" rasterYSize="20">
<GCPList Projection=''>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
</GCPList>
<VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
    <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
    <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
    <SourceBand>1</SourceBand>
    </SimpleSource>
</VRTRasterBand>
</VRTDataset>""" % (gcp[0][0], gcp[0][1], gcp[0][2], gcp[0][3],
                    gcp[1][0], gcp[1][1], gcp[1][2], gcp[1][3],
                    gcp[2][0], gcp[2][1], gcp[2][2], gcp[2][3],
                    gcp[3][0], gcp[3][1], gcp[3][2], gcp[3][3])
    vrt_ds = gdal.Open(vrt_txt)
    gcps = vrt_ds.GetGCPs()
    vrt_ds = None

    # Set GCPs()
    ds = gdal.Open(out_filename, gdal.GA_Update)
    ds.SetGCPs(gcps, src_wkt)
    ds = None

    # Check
    ds = gdal.Open(out_filename)
    got_gt = ds.GetGeoTransform()
    got_wkt = ds.GetProjectionRef()
    got_gcp_count = ds.GetGCPCount()
    ds.GetGCPs()
    got_gcp_wkt = ds.GetGCPProjection()
    got_neatline = ds.GetMetadataItem('NEATLINE')

    if pdf_is_pdfium():
        max_error = 1
    else:
        max_error = 0.0001

    ds = None

    assert got_wkt != '', 'did not expect null GetProjectionRef'

    assert got_gcp_wkt == '', 'did not expect non null GetGCPProjection'

    for i in range(6):
        assert abs(got_gt[i] - src_gt[i]) <= 1e-8, 'did not get expected gt'

    assert got_gcp_count == 0, 'did not expect GCPs'

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_lr = ogr.Geometry(ogr.wkbLinearRing)
    for i in range(4):
        expected_lr.AddPoint_2D(gcp[i][2], gcp[i][3])
    expected_lr.AddPoint_2D(gcp[0][2], gcp[0][3])
    expected_geom = ogr.Geometry(ogr.wkbPolygon)
    expected_geom.AddGeometry(expected_lr)

    if ogrtest.check_feature_geometry(got_geom, expected_geom, max_error=max_error) != 0:
        print('got : %s' % got_neatline)
        print('expected : %s' % expected_geom.ExportToWkt())
        pytest.fail('bad neatline')

    gdaltest.pdf_drv.Delete(out_filename)


def test_pdf_update_gcps_iso32000(poppler_or_pdfium):
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', None)
    _pdf_update_gcps(poppler_or_pdfium)


def test_pdf_update_gcps_ogc_bp(poppler_or_pdfium):
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', 'OGC_BP')
    _pdf_update_gcps(poppler_or_pdfium)
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', None)

###############################################################################
# Check SetGCPs() but with GCPs that do *not* resolve to a geotransform


def test_pdf_set_5_gcps_ogc_bp(poppler_or_pdfium):
    dpi = 300
    out_filename = 'tmp/pdf_set_5_gcps_ogc_bp.pdf'

    src_ds = gdal.Open('data/byte.tif')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()
    src_ds = None

    gcp = [[2., 8., 0, 0],
           [2., 10., 0, 0],
           [2., 18., 0, 0],
           [16., 18., 0, 0],
           [16., 8., 0, 0]]

    for i, _ in enumerate(gcp):
        gcp[i][2] = src_gt[0] + gcp[i][0] * src_gt[1] + gcp[i][1] * src_gt[2]
        gcp[i][3] = src_gt[3] + gcp[i][0] * src_gt[4] + gcp[i][1] * src_gt[5]

    # That way, GCPs will not resolve to a geotransform
    gcp[1][2] -= 100

    vrt_txt = """<VRTDataset rasterXSize="20" rasterYSize="20">
<GCPList Projection='%s'>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
</GCPList>
<VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
    <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
    <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
    <SourceBand>1</SourceBand>
    </SimpleSource>
</VRTRasterBand>
</VRTDataset>""" % (src_wkt,
                    gcp[0][0], gcp[0][1], gcp[0][2], gcp[0][3],
                    gcp[1][0], gcp[1][1], gcp[1][2], gcp[1][3],
                    gcp[2][0], gcp[2][1], gcp[2][2], gcp[2][3],
                    gcp[3][0], gcp[3][1], gcp[3][2], gcp[3][3],
                    gcp[4][0], gcp[4][1], gcp[4][2], gcp[4][3])
    vrt_ds = gdal.Open(vrt_txt)
    vrt_gcps = vrt_ds.GetGCPs()

    # Create PDF
    ds = gdaltest.pdf_drv.CreateCopy(out_filename, vrt_ds, options=['GEO_ENCODING=OGC_BP', 'DPI=%d' % dpi])
    ds = None

    vrt_ds = None

    # Check
    ds = gdal.Open(out_filename)
    got_gt = ds.GetGeoTransform()
    got_wkt = ds.GetProjectionRef()
    got_gcp_count = ds.GetGCPCount()
    got_gcps = ds.GetGCPs()
    got_gcp_wkt = ds.GetGCPProjection()
    got_neatline = ds.GetMetadataItem('NEATLINE')
    ds = None

    assert got_wkt == '', 'did not expect non null GetProjectionRef'

    assert got_gcp_wkt != '', 'did not expect null GetGCPProjection'

    expected_gt = [0, 1, 0, 0, 0, 1]
    for i in range(6):
        assert abs(got_gt[i] - expected_gt[i]) <= 1e-8, 'did not get expected gt'

    assert got_gcp_count == len(gcp), 'did not get expected GCP count'

    for i in range(got_gcp_count):
        assert (abs(got_gcps[i].GCPX - vrt_gcps[i].GCPX) <= 1e-5 and \
           abs(got_gcps[i].GCPY - vrt_gcps[i].GCPY) <= 1e-5 and \
           abs(got_gcps[i].GCPPixel - vrt_gcps[i].GCPPixel) <= 1e-5 and \
           abs(got_gcps[i].GCPLine - vrt_gcps[i].GCPLine) <= 1e-5), \
            ('did not get expected GCP (%d)' % i)

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    # Not sure this is really what we want, but without any geotransform, we cannot
    # find projected coordinates
    expected_geom = ogr.CreateGeometryFromWkt('POLYGON ((2 8,2 10,2 18,16 18,16 8,2 8))')

    if ogrtest.check_feature_geometry(got_geom, expected_geom) != 0:
        print('got : %s' % got_neatline)
        print('expected : %s' % expected_geom.ExportToWkt())
        pytest.fail('bad neatline')

    gdaltest.pdf_drv.Delete(out_filename)


###############################################################################
# Check NEATLINE support

def _pdf_set_neatline(pdf_backend, geo_encoding, dpi=300):
    out_filename = 'tmp/pdf_set_neatline.pdf'

    if geo_encoding == 'ISO32000':
        neatline = 'POLYGON ((441720 3751320,441720 3750120,441920 3750120,441920 3751320,441720 3751320))'
    else:  # For OGC_BP, we can use more than 4 points
        neatline = 'POLYGON ((441720 3751320,441720 3751000,441720 3750120,441920 3750120,441920 3751320,441720 3751320))'

    # Test CreateCopy() with NEATLINE
    src_ds = gdal.Open('data/byte.tif')
    expected_gt = src_ds.GetGeoTransform()
    ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options=['NEATLINE=%s' % neatline, 'GEO_ENCODING=%s' % geo_encoding, 'DPI=%d' % dpi])
    ds = None
    src_ds = None

    # Check
    ds = gdal.Open(out_filename)
    got_gt = ds.GetGeoTransform()
    got_neatline = ds.GetMetadataItem('NEATLINE')

    if pdf_is_pdfium():
        if geo_encoding == 'ISO32000':
            expected_gt = (440722.21886505646, 59.894520395349709, 0.020450745157516229, 3751318.6133243339, 0.077268565258743135, -60.009312694035692)
        max_error = 1
    else:
        max_error = 0.0001
    ds = None

    for i in range(6):
        assert abs(got_gt[i] - expected_gt[i]) <= 1e-7, 'did not get expected gt'

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_geom = ogr.CreateGeometryFromWkt(neatline)

    if ogrtest.check_feature_geometry(got_geom, expected_geom, max_error=max_error) != 0:
        print('got : %s' % got_neatline)
        print('expected : %s' % expected_geom.ExportToWkt())
        pytest.fail('bad neatline')

    # Test SetMetadataItem()
    ds = gdal.Open(out_filename, gdal.GA_Update)
    neatline = 'POLYGON ((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320))'
    ds.SetMetadataItem('NEATLINE', neatline)
    ds = None

    # Check
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', geo_encoding)
    ds = gdal.Open(out_filename)
    got_gt = ds.GetGeoTransform()
    got_neatline = ds.GetMetadataItem('NEATLINE')

    if pdf_is_pdfium():
        if geo_encoding == 'ISO32000':
            expected_gt = (440722.36151923181, 59.93217744208814, 0.0, 3751318.7819266757, 0.0, -59.941906300000845)

    ds = None
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', None)

    for i in range(6):
        assert (not (expected_gt[i] == 0 and abs(got_gt[i] - expected_gt[i]) > 1e-7) or \
           (expected_gt[i] != 0 and abs((got_gt[i] - expected_gt[i]) / expected_gt[i]) > 1e-7)), \
            'did not get expected gt'

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_geom = ogr.CreateGeometryFromWkt(neatline)

    if ogrtest.check_feature_geometry(got_geom, expected_geom, max_error=max_error) != 0:
        print('got : %s' % got_neatline)
        print('expected : %s' % expected_geom.ExportToWkt())
        pytest.fail('bad neatline')

    gdaltest.pdf_drv.Delete(out_filename)


def test_pdf_set_neatline_iso32000(poppler_or_pdfium):
    return _pdf_set_neatline(poppler_or_pdfium, 'ISO32000')


def test_pdf_set_neatline_ogc_bp(poppler_or_pdfium):
    return _pdf_set_neatline(poppler_or_pdfium, 'OGC_BP')

###############################################################################
# Check that we can generate identical file


def test_pdf_check_identity_iso32000(poppler_or_pdfium):
    out_filename = 'tmp/pdf_check_identity_iso32000.pdf'

    src_ds = gdal.Open('data/test_pdf.vrt')
    out_ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options=['STREAM_COMPRESS=NONE'])
    del out_ds
    src_ds = None

    f = open('data/test_iso32000.pdf', 'rb')
    data_ref = f.read()
    f.close()

    f = open(out_filename, 'rb')
    data_got = f.read()
    f.close()

    gdaltest.pdf_drv.Delete(out_filename)

    assert data_ref == data_got, 'content does not match reference content'

###############################################################################
# Check that we can generate identical file


def test_pdf_check_identity_ogc_bp(poppler_or_pdfium):
    out_filename = 'tmp/pdf_check_identity_ogc_bp.pdf'

    src_ds = gdal.Open('data/test_pdf.vrt')
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'NO')
    out_ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options=['GEO_ENCODING=OGC_BP', 'STREAM_COMPRESS=NONE'])
    del out_ds
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)
    src_ds = None

    f = open('data/test_ogc_bp.pdf', 'rb')
    data_ref = f.read()
    f.close()

    f = open(out_filename, 'rb')
    data_got = f.read()
    f.close()

    gdaltest.pdf_drv.Delete(out_filename)

    assert data_ref == data_got, 'content does not match reference content'

###############################################################################
# Check layers support


def test_pdf_layers(poppler_or_pdfium):
    if not pdf_is_poppler() and not pdf_is_pdfium():
        pytest.skip()

    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    layers = ds.GetMetadata_List('LAYERS')
    cs1 = ds.GetRasterBand(1).Checksum()
    ds = None

    # if layers != ['LAYER_00_INIT_STATE=ON', 'LAYER_00_NAME=New_Data_Frame', 'LAYER_01_INIT_STATE=ON', 'LAYER_01_NAME=New_Data_Frame.Graticule', 'LAYER_02_INIT_STATE=ON', 'LAYER_02_NAME=Layers', 'LAYER_03_INIT_STATE=ON', 'LAYER_03_NAME=Layers.Measured_Grid', 'LAYER_04_INIT_STATE=ON', 'LAYER_04_NAME=Layers.Graticule']:
    assert layers == ['LAYER_00_NAME=New_Data_Frame', 'LAYER_01_NAME=New_Data_Frame.Graticule', 'LAYER_02_NAME=Layers', 'LAYER_03_NAME=Layers.Measured_Grid', 'LAYER_04_NAME=Layers.Graticule'], \
        'did not get expected layers'

    if not pdf_checksum_available():
        pytest.skip()

    # Turn a layer off
    gdal.SetConfigOption('GDAL_PDF_LAYERS_OFF', 'New_Data_Frame')
    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    cs2 = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_PDF_LAYERS_OFF', None)

    assert cs2 != cs1, 'did not get expected checksum'

    # Turn the other layer on
    gdal.SetConfigOption('GDAL_PDF_LAYERS', 'Layers')
    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    cs3 = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_PDF_LAYERS', None)

    # So the end result must be identical
    assert cs3 == cs2, 'did not get expected checksum'

    # Turn another sublayer on
    ds = gdal.OpenEx('data/adobe_style_geospatial.pdf', open_options=['LAYERS=Layers.Measured_Grid'])
    cs4 = ds.GetRasterBand(1).Checksum()
    ds = None

    assert not (cs4 == cs1 or cs4 == cs2), 'did not get expected checksum'

###############################################################################
# Test MARGIN, EXTRA_STREAM, EXTRA_LAYER_NAME and EXTRA_IMAGES options


def test_pdf_custom_layout(poppler_or_pdfium):
    js = """button = app.alert({cMsg: 'This file was generated by GDAL. Do you want to visit its website ?', cTitle: 'Question', nIcon:2, nType:2});
if (button == 4) app.launchURL('http://gdal.org/');"""

    options = ['LEFT_MARGIN=1',
               'TOP_MARGIN=2',
               'RIGHT_MARGIN=3',
               'BOTTOM_MARGIN=4',
               'DPI=300',
               'LAYER_NAME=byte_tif',
               'EXTRA_STREAM=BT 255 0 0 rg /FTimesRoman 1 Tf 1 0 0 1 1 1 Tm (Footpage string) Tj ET',
               'EXTRA_LAYER_NAME=Footpage_and_logo',
               'EXTRA_IMAGES=data/byte.tif,0.5,0.5,0.2,link=http://gdal.org/,data/byte.tif,0.5,1.5,0.2',
               'JAVASCRIPT=%s' % js]

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_custom_layout.pdf', src_ds, options=options)
    ds = None
    src_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():
        ds = gdal.Open('tmp/pdf_custom_layout.pdf')
        ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List('LAYERS')
        ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_custom_layout.pdf')

    if pdf_is_poppler() or pdf_is_pdfium():
        assert layers == ['LAYER_00_NAME=byte_tif', 'LAYER_01_NAME=Footpage_and_logo'], \
            'did not get expected layers'

    
###############################################################################
# Test CLIPPING_EXTENT, EXTRA_RASTERS, EXTRA_RASTERS_LAYER_NAME, OFF_LAYERS, EXCLUSIVE_LAYERS options


def test_pdf_extra_rasters(poppler_or_pdfium):
    subbyte = """<VRTDataset rasterXSize="10" rasterYSize="10">
  <SRS>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982139006,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","26711"]]</SRS>
  <GeoTransform>  4.4102000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7510200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
  <Metadata>
    <MDI key="AREA_OR_POINT">Area</MDI>
  </Metadata>
  <Metadata domain="IMAGE_STRUCTURE">
    <MDI key="INTERLEAVE">BAND</MDI>
  </Metadata>
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata />
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">../data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="5" yOff="5" xSize="10" ySize="10" />
      <DstRect xOff="0" yOff="0" xSize="10" ySize="10" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""

    f = open('tmp/subbyte.vrt', 'wt')
    f.write(subbyte)
    f.close()

    options = ['MARGIN=1',
               'DPI=300',
               'WRITE_USERUNIT=YES',
               'CLIPPING_EXTENT=440780,3750180,441860,3751260',
               'LAYER_NAME=byte_tif',
               'EXTRA_RASTERS=tmp/subbyte.vrt',
               'EXTRA_RASTERS_LAYER_NAME=subbyte',
               'OFF_LAYERS=byte_tif',
               'EXCLUSIVE_LAYERS=byte_tif,subbyte']

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_extra_rasters.pdf', src_ds, options=options)
    ds = None
    src_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():
        ds = gdal.Open('tmp/pdf_extra_rasters.pdf')
        cs = ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List('LAYERS')
        ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_extra_rasters.pdf')
    os.unlink('tmp/subbyte.vrt')

    if pdf_is_poppler() or pdf_is_pdfium():
        assert layers == ['LAYER_00_NAME=byte_tif', 'LAYER_01_NAME=subbyte'], \
            'did not get expected layers'
    assert not (pdf_is_poppler() and (cs != 7926 and cs != 8177 and cs != 8174 and cs != 8165)), \
        'bad checksum'

###############################################################################
# Test adding a OGR datasource


def test_pdf_write_ogr(poppler_or_pdfium):
    f = gdal.VSIFOpenL('tmp/test.csv', 'wb')
    data = """id,foo,WKT,style
1,bar,"MULTIPOLYGON (((440720 3751320,440720 3750120,441020 3750120,441020 3751320,440720 3751320),(440800 3751200,440900 3751200,440900 3751000,440800 3751000,440800 3751200)),((441720 3751320,441720 3750120,441920 3750120,441920 3751320,441720 3751320)))",
2,baz,"LINESTRING(440720 3751320,441920 3750120)","PEN(c:#FF0000,w:5pt,p:""2px 1pt"")"
3,baz2,"POINT(441322.400 3750717.600)","PEN(c:#FF00FF,w:10px);BRUSH(fc:#FFFFFF);LABEL(c:#FF000080, f:""Arial, Helvetica"", a:45, s:12pt, t:""Hello World!"")"
4,baz3,"POINT(0 0)",""
"""
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL('tmp/test.vrt', 'wb')
    data = """<OGRVRTDataSource>
  <OGRVRTLayer name="test">
    <SrcDataSource relativeToVRT="0" shared="1">tmp/test.csv</SrcDataSource>
    <SrcLayer>test</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <LayerSRS>EPSG:26711</LayerSRS>
    <Field name="id" type="Integer" src="id"/>
    <Field name="foo" type="String" src="foo"/>
    <Field name="WKT" type="String" src="WKT"/>
    <Style>style</Style>
  </OGRVRTLayer>
</OGRVRTDataSource>
"""
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    options = ['OGR_DATASOURCE=tmp/test.vrt', 'OGR_DISPLAY_LAYER_NAMES=A_Layer', 'OGR_DISPLAY_FIELD=foo']

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_write_ogr.pdf', src_ds, options=options)
    ds = None
    src_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():
        ds = gdal.Open('tmp/pdf_write_ogr.pdf')
        cs_ref = ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List('LAYERS')
        ds = None

    ogr_ds = ogr.Open('tmp/pdf_write_ogr.pdf')
    ogr_lyr = ogr_ds.GetLayer(0)
    feature_count = ogr_lyr.GetFeatureCount()
    ogr_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():

        cs_tab = []
        rendering_options = ['RASTER', 'VECTOR', 'TEXT', 'RASTER,VECTOR', 'RASTER,TEXT', 'VECTOR,TEXT', 'RASTER,VECTOR,TEXT']
        for opt in rendering_options:
            gdal.ErrorReset()
            ds = gdal.OpenEx('tmp/pdf_write_ogr.pdf', open_options=['RENDERING_OPTIONS=%s' % opt])
            cs = ds.GetRasterBand(1).Checksum()
            # When misconfigured Poppler with fonts, use this to avoid error
            if 'TEXT' in opt and gdal.GetLastErrorMsg().find('font') >= 0:
                cs = -cs
            cs_tab.append(cs)
            ds = None

        # Test that all combinations give a different result
        for i, roi in enumerate(rendering_options):
            # print('Checksum %s: %d' % (rendering_options[i], cs_tab[i]) )
            for j in range(i + 1, len(rendering_options)):
                if cs_tab[i] == cs_tab[j] and cs_tab[i] >= 0 and cs_tab[j] >= 0:
                    print('Checksum %s: %d' % (roi, cs_tab[i]))
                    pytest.fail('Checksum %s: %d' % (rendering_options[j], cs_tab[j]))

        # And test that RASTER,VECTOR,TEXT is the default rendering
        assert abs(cs_tab[len(rendering_options) - 1]) == cs_ref

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_write_ogr.pdf')

    gdal.Unlink('tmp/test.csv')
    gdal.Unlink('tmp/test.vrt')

    if pdf_is_poppler() or pdf_is_pdfium():
        assert layers == ['LAYER_00_NAME=A_Layer', 'LAYER_01_NAME=A_Layer.Text'], \
            'did not get expected layers'

    # Should have filtered out id = 4
    assert feature_count == 3, 'did not get expected feature count'

###############################################################################
# Test adding a OGR datasource with reprojection of OGR SRS to GDAL SRS


def test_pdf_write_ogr_with_reprojection(poppler_or_pdfium):

    f = gdal.VSIFOpenL('tmp/test.csv', 'wb')
    data = """WKT,id
"POINT (-117.641059792392142 33.902263065734573)",1
"POINT (-117.64098016484607 33.891620919037436)",2
"POINT (-117.62829768175105 33.902328822481238)",3
"POINT (-117.628219639160108 33.891686649558416)",4
"POINT (-117.634639319537328 33.896975031776485)",5
"POINT (-121.488694798047447 0.0)",6
"""

    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL('tmp/test.vrt', 'wb')
    data = """<OGRVRTDataSource>
  <OGRVRTLayer name="test">
    <SrcDataSource relativeToVRT="0" shared="1">tmp/test.csv</SrcDataSource>
    <SrcLayer>test</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <LayerSRS>+proj=longlat +datum=NAD27</LayerSRS>
    <Field name="id" type="Integer" src="id"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
"""
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    options = ['OGR_DATASOURCE=tmp/test.vrt', 'OGR_DISPLAY_LAYER_NAMES=A_Layer', 'OGR_DISPLAY_FIELD=foo']

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_write_ogr_with_reprojection.pdf', src_ds, options=options)
    del ds
    src_ds = None

    ogr_ds = ogr.Open('tmp/pdf_write_ogr_with_reprojection.pdf')
    ogr_lyr = ogr_ds.GetLayer(0)
    feature_count = ogr_lyr.GetFeatureCount()
    ogr_ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_write_ogr_with_reprojection.pdf')

    gdal.Unlink('tmp/test.csv')
    gdal.Unlink('tmp/test.vrt')

    # Should have filtered out id = 6
    assert feature_count == 5, 'did not get expected feature count'

###############################################################################
# Test direct copy of source JPEG file


def test_pdf_jpeg_direct_copy(poppler_or_pdfium):
    if gdal.GetDriverByName('JPEG') is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte_with_xmp.jpg')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_jpeg_direct_copy.pdf', src_ds, options=['XMP=NO'])
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/pdf_jpeg_direct_copy.pdf')
    # No XMP at PDF level
    assert ds.GetMetadata('xml:XMP') is None
    assert ds.RasterXSize == 20
    assert not (pdf_checksum_available() and ds.GetRasterBand(1).Checksum() == 0)
    ds = None

    # But we can find the original XMP from the JPEG file !
    f = open('tmp/pdf_jpeg_direct_copy.pdf', 'rb')
    data = f.read().decode('ISO-8859-1')
    f.close()
    offset = data.find('ns.adobe.com')

    gdal.Unlink('tmp/pdf_jpeg_direct_copy.pdf')

    assert offset != -1

###############################################################################
# Test direct copy of source JPEG file within VRT file


def test_pdf_jpeg_in_vrt_direct_copy(poppler_or_pdfium):
    if gdal.GetDriverByName('JPEG') is None:
        pytest.skip()

    src_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <SRS>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982139006,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","26711"]]</SRS>
  <GeoTransform>  4.4072000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7513200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte_with_xmp.jpg</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="1" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_jpeg_in_vrt_direct_copy.pdf', src_ds)
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/pdf_jpeg_in_vrt_direct_copy.pdf')
    # No XMP at PDF level
    assert ds.GetMetadata('xml:XMP') is None
    assert ds.RasterXSize == 20
    assert not (pdf_checksum_available() and ds.GetRasterBand(1).Checksum() == 0)
    ds = None

    # But we can find the original XMP from the JPEG file !
    f = open('tmp/pdf_jpeg_in_vrt_direct_copy.pdf', 'rb')
    data = f.read().decode('ISO-8859-1')
    f.close()
    offset = data.find('ns.adobe.com')

    gdal.Unlink('tmp/pdf_jpeg_in_vrt_direct_copy.pdf')

    assert offset != -1

###############################################################################
# Test reading georeferencing attached to an image, and not to the page (#4695)


@pytest.mark.parametrize(
    'src_filename',
    ['data/byte.tif', 'data/rgbsmall.tif']
)
def pdf_georef_on_image(src_filename, pdf_backend):
    src_ds = gdal.Open(src_filename)
    gdal.SetConfigOption('GDAL_PDF_WRITE_GEOREF_ON_IMAGE', 'YES')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_georef_on_image.pdf', src_ds, options=['MARGIN=10', 'GEO_ENCODING=NONE'])
    del out_ds
    gdal.SetConfigOption('GDAL_PDF_WRITE_GEOREF_ON_IMAGE', None)
    if pdf_checksum_available():
        src_cs = src_ds.GetRasterBand(1).Checksum()
    else:
        src_cs = 0
    src_ds = None

    ds = gdal.Open('tmp/pdf_georef_on_image.pdf')
    subdataset_name = ds.GetMetadataItem('SUBDATASET_1_NAME', 'SUBDATASETS')
    ds = None

    ds = gdal.Open(subdataset_name)
    got_wkt = ds.GetProjectionRef()
    if pdf_checksum_available():
        got_cs = ds.GetRasterBand(1).Checksum()
    else:
        got_cs = 0
    ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_georef_on_image.pdf')

    assert got_wkt != '', 'did not get projection'

    assert not pdf_checksum_available() or src_cs == got_cs, 'did not get same checksum'


###############################################################################
# Test writing a PDF that hits Acrobat limits in term of page dimensions (#5412)


def test_pdf_write_huge(poppler_or_pdfium):
    if pdf_is_poppler() or pdf_is_pdfium():
        tmp_filename = '/vsimem/pdf_write_huge.pdf'
    else:
        tmp_filename = 'tmp/pdf_write_huge.pdf'

    for (xsize, ysize) in [(19200, 1), (1, 19200)]:
        src_ds = gdal.GetDriverByName('MEM').Create('', xsize, ysize, 1)
        ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds)
        ds = None
        ds = gdal.Open(tmp_filename)
        assert int(ds.GetMetadataItem('DPI')) == 96
        assert (ds.RasterXSize == src_ds.RasterXSize and \
                ds.RasterYSize == src_ds.RasterYSize)
        ds = None

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds, options=['DPI=72'])
        gdal.PopErrorHandler()
        msg = gdal.GetLastErrorMsg()
        assert msg != ''
        ds = None
        ds = gdal.Open(tmp_filename)
        assert int(ds.GetMetadataItem('DPI')) == 72
        ds = None

        src_ds = None

    for option in ['LEFT_MARGIN=14400', 'TOP_MARGIN=14400']:
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds, options=[option])
        gdal.PopErrorHandler()
        msg = gdal.GetLastErrorMsg()
        assert msg != ''
        ds = None
        ds = gdal.Open(tmp_filename)
        assert int(ds.GetMetadataItem('DPI')) == 72
        ds = None

        src_ds = None

    gdal.Unlink(tmp_filename)

###############################################################################
# Test creating overviews


def test_pdf_overviews(poppler_or_pdfium):
    if not pdf_is_poppler() and not pdf_is_pdfium():
        pytest.skip()
    tmp_filename = '/vsimem/pdf_overviews.pdf'

    src_ds = gdal.GetDriverByName('MEM').Create('', 1024, 1024, 3)
    for i in range(3):
        src_ds.GetRasterBand(i + 1).Fill(255)
    ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds)
    src_ds = None
    ds = None
    ds = gdal.Open(tmp_filename)
    before = ds.GetRasterBand(1).GetOverviewCount()
    ds.GetRasterBand(1).GetOverview(-1)
    ds.GetRasterBand(1).GetOverview(10)
    if before >= 1:
        assert pdf_is_pdfium(), 'No overview expected at this point!'
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs == 5934
    elif pdf_is_pdfium():
        pytest.fail('Overview expected at this point!')
    ds.BuildOverviews('NONE', [2])
    after = ds.GetRasterBand(1).GetOverviewCount()
    assert after == 1
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 0
    ds = None

    gdaltest.pdf_drv.Delete(tmp_filename)

###############################################################################
# Test password


def test_pdf_password(poppler_or_pdfium_or_podofo):
    # User password of this test file is user_password and owner password is
    # owner_password

    # No password
    with gdaltest.error_handler():
        ds = gdal.Open('data/byte_enc.pdf')
    assert ds is None

    # Wrong password
    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/byte_enc.pdf', open_options=['USER_PWD=wrong_password'])
    assert ds is None

    # Correct password
    ds = gdal.OpenEx('data/byte_enc.pdf', open_options=['USER_PWD=user_password'])
    assert ds is not None

    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    # Test ASK_INTERACTIVE with wrong password
    cmd_line = test_cli_utilities.get_gdal_translate_path() + ' data/byte_enc.pdf /vsimem/out.tif -q -oo USER_PWD=ASK_INTERACTIVE < tmp/password.txt'
    if sys.platform != 'win32':
        cmd_line += ' >/dev/null 2>/dev/null'

    open('tmp/password.txt', 'wb').write('wrong_password'.encode('ASCII'))
    ret = os.system(cmd_line)
    os.unlink('tmp/password.txt')
    assert ret != 0

    # Test ASK_INTERACTIVE with correct password
    open('tmp/password.txt', 'wb').write('user_password'.encode('ASCII'))
    ret = os.system(cmd_line)
    os.unlink('tmp/password.txt')
    assert ret == 0

###############################################################################
# Test multi page support


def test_pdf_multipage(poppler_or_pdfium_or_podofo):
    # byte_and_rgbsmall_2pages.pdf was generated with :
    # 1) gdal_translate gcore/data/byte.tif byte.pdf -of PDF
    # 2) gdal_translate gcore/data/rgbsmall.tif rgbsmall.pdf -of PDF
    # 3)  ~/install-podofo-0.9.3/bin/podofomerge byte.pdf rgbsmall.pdf byte_and_rgbsmall_2pages.pdf

    ds = gdal.Open('data/byte_and_rgbsmall_2pages.pdf')
    subdatasets = ds.GetSubDatasets()
    expected_subdatasets = [('PDF:1:data/byte_and_rgbsmall_2pages.pdf', 'Page 1 of data/byte_and_rgbsmall_2pages.pdf'), ('PDF:2:data/byte_and_rgbsmall_2pages.pdf', 'Page 2 of data/byte_and_rgbsmall_2pages.pdf')]
    assert subdatasets == expected_subdatasets, 'did not get expected subdatasets'
    ds = None

    ds = gdal.Open('PDF:1:data/byte_and_rgbsmall_2pages.pdf')
    assert ds.RasterXSize == 20, 'wrong width'

    ds2 = gdal.Open('PDF:2:data/byte_and_rgbsmall_2pages.pdf')
    assert ds2.RasterXSize == 50, 'wrong width'

    with gdaltest.error_handler():
        ds3 = gdal.Open('PDF:0:data/byte_and_rgbsmall_2pages.pdf')
    assert ds3 is None

    with gdaltest.error_handler():
        ds3 = gdal.Open('PDF:3:data/byte_and_rgbsmall_2pages.pdf')
    assert ds3 is None

    with gdaltest.error_handler():
        ds = gdal.Open('PDF:1:/does/not/exist.pdf')
    assert ds is None

###############################################################################
# Test PAM metadata support


def test_pdf_metadata(poppler_or_pdfium):
    gdal.Translate('tmp/pdf_metadata.pdf', 'data/byte.tif', format='PDF', metadataOptions=['FOO=BAR'])
    ds = gdal.Open('tmp/pdf_metadata.pdf')
    md = ds.GetMetadata()
    assert 'FOO' in md
    ds = None
    ds = gdal.Open('tmp/pdf_metadata.pdf')
    assert ds.GetMetadataItem('FOO') == 'BAR'
    ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_metadata.pdf')

###############################################################################
# Test PAM georef support


def test_pdf_pam_georef(poppler_or_pdfium):
    src_ds = gdal.Open('data/byte.tif')

    # Default behaviour should result in no PAM file
    gdaltest.pdf_drv.CreateCopy('tmp/pdf_pam_georef.pdf', src_ds)
    assert not os.path.exists('tmp/pdf_pam_georef.pdf.aux.xml')

    # Now disable internal georeferencing, so georef should go to PAM
    gdaltest.pdf_drv.CreateCopy('tmp/pdf_pam_georef.pdf', src_ds, options=['GEO_ENCODING=NONE'])
    assert os.path.exists('tmp/pdf_pam_georef.pdf.aux.xml')

    ds = gdal.Open('tmp/pdf_pam_georef.pdf')
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetProjectionRef() == src_ds.GetProjectionRef()
    ds = None

    gdal.GetDriverByName('PDF').Delete('tmp/pdf_pam_georef.pdf')
