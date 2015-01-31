#!/usr/bin/env python
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

sys.path.append( '../pymod' )
sys.path.append( '../osr' )

import gdaltest
import ogrtest

###############################################################################
# Test driver presence

def pdf_init():

    try:
        gdaltest.pdf_drv = gdal.GetDriverByName('PDF')
    except:
        gdaltest.pdf_drv = None

    if gdaltest.pdf_drv is None:
        return 'skip'

    md = gdaltest.pdf_drv.GetMetadata()
    if not 'HAVE_POPPLER' in md and not 'HAVE_PODOFO' in md:
        gdaltest.pdf_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Returns True if we run with poppler

def pdf_is_poppler():

    md = gdaltest.pdf_drv.GetMetadata()
    val = gdal.GetConfigOption("GDAL_PDF_LIB", "POPPLER")
    if val == 'POPPLER' and 'HAVE_POPPLER' in md:
        return True
    else:
        return False

###############################################################################
# Returns True if we can compute the checksum

def pdf_checksum_available():

    try:
        ret = gdaltest.pdf_is_checksum_available
        if ret is True or ret is False:
            return ret
    except:
        pass

    if pdf_is_poppler():
        gdaltest.pdf_is_checksum_available = True
        return gdaltest.pdf_is_checksum_available

    (out, err) = gdaltest.runexternal_out_and_err("pdftoppm -v")
    if err.find('pdftoppm version') == 0:
        gdaltest.pdf_is_checksum_available = True
        return gdaltest.pdf_is_checksum_available
    else:
        print('Cannot compute to checksum due to missing pdftoppm')
        print(err)
        gdaltest.pdf_is_checksum_available = False
        return gdaltest.pdf_is_checksum_available

###############################################################################
# Test OGC best practice geospatial PDF

def pdf_online_1():

    if gdaltest.pdf_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.agc.army.mil/GeoPDFgallery/Imagery/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf', 'Cherrydale_eDOQQ_1m_0_033_R1C1.pdf'):
        return 'skip'

    try:
        os.stat('tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    except:
        return 'skip'

    ds = gdal.Open('tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    if ds is None:
        return 'fail'

    if ds.RasterXSize != 1241:
        gdaltest.post_reason('bad dimensions')
        print(ds.RasterXSize)
        return 'fail'

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    expected_gt = (-77.112328333299999, 9.1666559999999995e-06, 0.0, 38.897842488372, -0.0, -9.1666559999999995e-06)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            gdaltest.post_reason('bad geotransform')
            print(gt)
            print(expected_gt)
            return 'fail'

    expected_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]'
    if wkt != expected_wkt:
        gdaltest.post_reason('bad WKT')
        print(wkt)
        return 'fail'

    if pdf_checksum_available():
        cs = ds.GetRasterBand(1).Checksum()
        if cs == 0:
            gdaltest.post_reason('bad checksum')
            return 'fail'

    return 'success'

###############################################################################

def pdf_online_2():

    if gdaltest.pdf_drv is None:
        return 'skip'

    try:
        os.stat('tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    except:
        return 'skip'

    ds = gdal.Open('PDF:1:tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf')
    if ds is None:
        return 'fail'

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    expected_gt = (-77.112328333299999, 9.1666559999999995e-06, 0.0, 38.897842488372, -0.0, -9.1666559999999995e-06)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            print(gt)
            print(expected_gt)
            return 'fail'

    expected_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]'
    if wkt != expected_wkt:
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test Adobe style geospatial pdf

def pdf_1():

    if gdaltest.pdf_drv is None:
        return 'skip'

    gdal.SetConfigOption('GDAL_PDF_DPI', '200')
    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    gdal.SetConfigOption('GDAL_PDF_DPI', None)
    if ds is None:
        return 'fail'

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    expected_gt = (333274.61654367246, 31.764802242655662, 0.0, 4940391.7593506984, 0.0, -31.794745501708238)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-7:
            gdaltest.post_reason('bad geotransform')
            print(gt)
            print(expected_gt)
            return 'fail'

    expected_wkt = 'PROJCS["WGS_1984_UTM_Zone_20N",GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_84",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["False_Easting",500000.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-63.0],PARAMETER["Scale_Factor",0.9996],PARAMETER["Latitude_Of_Origin",0.0],UNIT["Meter",1.0]]'
    if wkt != expected_wkt:
        gdaltest.post_reason('bad WKT')
        print(wkt)
        return 'fail'

    if pdf_checksum_available():
        cs = ds.GetRasterBand(1).Checksum()
        #if cs != 17740 and cs != 19346:
        if cs == 0:
            gdaltest.post_reason('bad checksum')
            print(cs)
            return 'fail'

    neatline = ds.GetMetadataItem('NEATLINE')
    got_geom = ogr.CreateGeometryFromWkt(neatline)
    expected_geom = ogr.CreateGeometryFromWkt('POLYGON ((338304.150125828920864 4896673.639421294443309,338304.177293475600891 4933414.799376524984837,382774.271384406310972 4933414.546264361590147,382774.767329963855445 4896674.273581005632877,338304.150125828920864 4896673.639421294443309))')

    if ogrtest.check_feature_geometry(got_geom, expected_geom) != 0:
        gdaltest.post_reason('bad neatline')
        print(neatline)
        return 'fail'

    return 'success'

###############################################################################
# Test write support with ISO32000 geo encoding

def pdf_iso32000():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'byte.tif', 1, None )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 1, check_srs = True, check_checksum_not_null = pdf_checksum_available())

    return ret

###############################################################################
# Test write support with ISO32000 geo encoding, with DPI=300

def pdf_iso32000_dpi_300():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'byte.tif', 1, None, options = ['DPI=300'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 1, check_srs = True, check_checksum_not_null = pdf_checksum_available())

    return ret

###############################################################################
# Test write support with OGC_BP geo encoding

def pdf_ogcbp():

    if gdaltest.pdf_drv is None:
        return 'skip'

    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    tst = gdaltest.GDALTest( 'PDF', 'byte.tif', 1, None, options = ['GEO_ENCODING=OGC_BP'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 1, check_srs = True, check_checksum_not_null = pdf_checksum_available())
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)

    return ret

###############################################################################
# Test write support with OGC_BP geo encoding, with DPI=300

def pdf_ogcbp_dpi_300():

    if gdaltest.pdf_drv is None:
        return 'skip'

    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    tst = gdaltest.GDALTest( 'PDF', 'byte.tif', 1, None, options = ['GEO_ENCODING=OGC_BP', 'DPI=300'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 1, check_srs = True, check_checksum_not_null = pdf_checksum_available())
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)

    return ret
    
def pdf_ogcbp_lcc():

    if gdaltest.pdf_drv is None:
        return 'skip'

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
    src_ds.SetGeoTransform([500000,1,0,1000000,0,-1])

    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_ogcbp_lcc.pdf', src_ds)
    out_wkt = out_ds.GetProjectionRef()
    out_ds = None
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)

    src_ds = None

    gdal.Unlink('tmp/temp.tif')
    gdal.Unlink('tmp/pdf_ogcbp_lcc.pdf')

    sr1 = osr.SpatialReference(wkt)
    sr2 = osr.SpatialReference(out_wkt)
    if sr1.IsSame(sr2) == 0:
        gdaltest.post_reason('wrong wkt')
        print(sr2.ExportToPrettyWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test no compression

def pdf_no_compression():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'byte.tif', 1, None, options = ['COMPRESS=NONE'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

###############################################################################
# Test compression methods

def pdf_jpeg_compression(filename = 'byte.tif'):

    if gdaltest.pdf_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', filename, 1, None, options = ['COMPRESS=JPEG'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

def pdf_get_J2KDriver(drv_name):
    drv = gdal.GetDriverByName(drv_name)
    if drv is None:
        return None
    if drv_name == 'JP2ECW':
        import ecw
        if not ecw.has_write_support():
            return None
    return drv

def pdf_jpx_compression(filename, drv_name = None):

    if gdaltest.pdf_drv is None:
        return 'skip'

    if drv_name is None:
        if pdf_get_J2KDriver('JP2KAK') is None and \
           pdf_get_J2KDriver('JP2ECW') is None and \
           pdf_get_J2KDriver('JP2OpenJpeg') is None and \
           pdf_get_J2KDriver('JPEG2000') is None :
            return 'skip'
    elif pdf_get_J2KDriver(drv_name) is None:
        return 'skip'

    if drv_name is None:
        options = ['COMPRESS=JPEG2000']
    else:
        options = ['COMPRESS=JPEG2000', 'JPEG2000_DRIVER=%s' % drv_name]

    tst = gdaltest.GDALTest( 'PDF', filename, 1, None, options = options )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

def pdf_jp2_auto_compression():
    return pdf_jpx_compression('utm.tif')

def pdf_jp2kak_compression():
    return pdf_jpx_compression('utm.tif', 'JP2KAK')

def pdf_jp2ecw_compression():
    return pdf_jpx_compression('utm.tif', 'JP2ECW')

def pdf_jp2openjpeg_compression():
    return pdf_jpx_compression('utm.tif', 'JP2OpenJpeg')

def pdf_jpeg2000_compression():
    return pdf_jpx_compression('utm.tif', 'JPEG2000')

def pdf_jp2ecw_compression_rgb():
    return pdf_jpx_compression('rgbsmall.tif', 'JP2ECW')

def pdf_jpeg_compression_rgb():
    return pdf_jpeg_compression('rgbsmall.tif')

###############################################################################
# Test RGBA

def pdf_rgba_default_compression(options_param = []):

    if gdaltest.pdf_drv is None:
        return 'skip'

    if not pdf_checksum_available():
        return 'skip'

    src_ds = gdal.Open( '../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/rgba.pdf', src_ds, options = options_param)
    out_ds = None

    #gdal.SetConfigOption('GDAL_PDF_BANDS', '4')
    out_ds = gdal.Open('tmp/rgba.pdf')
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
    #gdal.SetConfigOption('GDAL_PDF_BANDS', None)

    gdal.Unlink('tmp/rgba.pdf')

    if cs4 < 0:
        return 'skip'

    if cs4 == 0:
        gdaltest.post_reason('wrong checksum')
        print(cs4)
        return 'fail'

    if cs1 != src_cs1 or cs2 != src_cs2 or cs3 != src_cs3 or cs4 != src_cs4:
        print(cs1)
        print(cs2)
        print(cs3)
        print(cs4)
        print(src_cs1)
        print(src_cs2)
        print(src_cs3)
        print(src_cs4)

    return 'success'

def pdf_rgba_default_compression_tiled():
    return pdf_rgba_default_compression(['BLOCKXSIZE=32', 'BLOCKYSIZE=32'])

def pdf_jpeg_compression_rgba():
    return pdf_jpeg_compression('../../gcore/data/stefan_full_rgba.tif')

###############################################################################
# Test PREDICTOR=2

def pdf_predictor_2():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'utm.tif', 1, None, options = ['PREDICTOR=2'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

def pdf_predictor_2_rgb():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'rgbsmall.tif', 1, None, options = ['PREDICTOR=2'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

###############################################################################
# Test tiling

def pdf_tiled():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'utm.tif', 1, None, options = ['COMPRES=DEFLATE', 'TILED=YES'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

def pdf_tiled_128():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'utm.tif', 1, None, options = ['BLOCKXSIZE=128', 'BLOCKYSIZE=128'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

###############################################################################
# Test raster with color table

def pdf_color_table():

    if gdaltest.pdf_drv is None:
        return 'skip'

    if gdal.GetDriverByName('GIF') is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'bug407.gif', 1, None )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = pdf_checksum_available())

    return ret

###############################################################################
# Test XMP support

def pdf_xmp():

    if gdaltest.pdf_drv is None:
        return 'skip'

    src_ds = gdal.Open( 'data/adobe_style_geospatial_with_xmp.pdf')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_xmp.pdf', src_ds, options = ['WRITE_INFO=NO'])
    ref_md = src_ds.GetMetadata('xml:XMP')
    got_md = out_ds.GetMetadata('xml:XMP')
    base_md = out_ds.GetMetadata()
    out_ds = None
    src_ds = None

    gdal.Unlink('tmp/pdf_xmp.pdf')

    if ref_md[0] != got_md[0]:
        gdaltest.post_reason('fail')
        print(got_md[0])
        return 'fail'

    if len(base_md) != 2: # NEATLINE and DPI
        gdaltest.post_reason('fail')
        print(base_md)
        return 'fail'

    return 'success'

###############################################################################
# Test Info

def pdf_info():

    if gdaltest.pdf_drv is None:
        return 'skip'

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

    src_ds = gdal.Open( 'data/byte.tif')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_info.pdf', src_ds, options = options)
    out_ds2 = gdaltest.pdf_drv.CreateCopy('tmp/pdf_info_2.pdf', out_ds)
    md = out_ds2.GetMetadata()
    out_ds2 = None
    out_ds = None
    src_ds = None

    gdal.Unlink('tmp/pdf_info.pdf')
    gdal.Unlink('tmp/pdf_info_2.pdf')

    if md['AUTHOR'] != val or \
       md['CREATOR'] != 'creator' or \
       md['KEYWORDS'] != 'keywords' or \
       md['PRODUCER'] != 'producer' or \
       md['SUBJECT'] != 'subject' or \
       md['TITLE'] != 'title':
        gdaltest.post_reason("metadata doesn't match")
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Switch from poppler to podofo if both are available

def pdf_switch_underlying_lib():

    if gdaltest.pdf_drv is None:
        return 'skip'

    md = gdaltest.pdf_drv.GetMetadata()
    if 'HAVE_POPPLER' in md and 'HAVE_PODOFO' in md:
        gdal.SetConfigOption("GDAL_PDF_LIB", "PODOFO")
        print('Using podofo now')
        gdaltest.pdf_is_checksum_available = None
    else:
        gdaltest.pdf_drv = None

    return 'success'

###############################################################################
# Check SetGeoTransform() / SetProjection()

def pdf_update_gt():

    if gdaltest.pdf_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_update_gt.pdf', src_ds)
    ds = None
    src_ds = None

    # Alter geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf', gdal.GA_Update)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2,1,0,49,0,-1])
    ds = None

    # Check geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf')
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [2,1,0,49,0,-1]
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('did not get expected gt')
            print(gt)
            return 'fail'

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
        if abs(gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('did not get expected gt')
            print(gt)
            return 'fail'

    # Set geotransform again
    ds = gdal.Open('tmp/pdf_update_gt.pdf', gdal.GA_Update)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([3,1,0,50,0,-1])
    ds = None

    # Check geotransform
    ds = gdal.Open('tmp/pdf_update_gt.pdf')
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [3,1,0,50,0,-1]
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('did not get expected gt')
            print(gt)
            return 'fail'

    gdaltest.pdf_drv.Delete('tmp/pdf_update_gt.pdf')

    return 'success'

###############################################################################
# Check SetMetadataItem() for Info

def pdf_update_info():

    if gdaltest.pdf_drv is None:
        return 'skip'

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

    if author != 'author':
        gdaltest.post_reason('did not get expected metadata')
        print(author)
        return 'fail'

    # Update info
    ds = gdal.Open('tmp/pdf_update_info.pdf', gdal.GA_Update)
    ds.SetMetadataItem('AUTHOR', 'author2')
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_info.pdf')
    author = ds.GetMetadataItem('AUTHOR')
    ds = None

    if author != 'author2':
        gdaltest.post_reason('did not get expected metadata')
        print(author)
        return 'fail'

    # Clear info
    ds = gdal.Open('tmp/pdf_update_info.pdf', gdal.GA_Update)
    ds.SetMetadataItem('AUTHOR', None)
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_info.pdf')
    author = ds.GetMetadataItem('AUTHOR')
    ds = None

    if author != None:
        gdaltest.post_reason('did not get expected metadata')
        print(author)
        return 'fail'

    gdaltest.pdf_drv.Delete('tmp/pdf_update_info.pdf')

    return 'success'

###############################################################################
# Check SetMetadataItem() for xml:XMP

def pdf_update_xmp():

    if gdaltest.pdf_drv is None:
        return 'skip'

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

    if xmp != "<?xpacket begin='a'/><a/>":
        gdaltest.post_reason('did not get expected metadata')
        print(xmp)
        return 'fail'

    # Update info
    ds = gdal.Open('tmp/pdf_update_xmp.pdf', gdal.GA_Update)
    ds.SetMetadata(["<?xpacket begin='a'/><a_updated/>"], "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_xmp.pdf')
    xmp = ds.GetMetadata('xml:XMP')[0]
    ds = None

    if xmp != "<?xpacket begin='a'/><a_updated/>":
        gdaltest.post_reason('did not get expected metadata')
        print(xmp)
        return 'fail'

    # Clear info
    ds = gdal.Open('tmp/pdf_update_xmp.pdf', gdal.GA_Update)
    ds.SetMetadata(None, "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open('tmp/pdf_update_xmp.pdf')
    xmp = ds.GetMetadata('xml:XMP')
    ds = None

    if xmp != None:
        gdaltest.post_reason('did not get expected metadata')
        print(xmp)
        return 'fail'

    gdaltest.pdf_drv.Delete('tmp/pdf_update_xmp.pdf')

    return 'success'

###############################################################################
# Check SetGCPs() but with GCPs that resolve to a geotransform

def pdf_update_gcps(dpi = 300):

    if gdaltest.pdf_drv is None:
        return 'skip'
        
    out_filename = 'tmp/pdf_update_gcps.pdf'

    src_ds = gdal.Open('data/byte.tif')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()
    ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options = ['GEO_ENCODING=NONE', 'DPI=%d' % dpi])
    ds = None
    src_ds = None

    gcp = [ [ 2., 8., 0, 0 ],
            [ 2., 18., 0, 0 ],
            [ 16., 18., 0, 0 ],
            [ 16., 8., 0, 0 ] ]
             
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
</VRTDataset>""" % (  gcp[0][0], gcp[0][1], gcp[0][2], gcp[0][3],
                      gcp[1][0], gcp[1][1], gcp[1][2], gcp[1][3],
                      gcp[2][0], gcp[2][1], gcp[2][2], gcp[2][3],
                      gcp[3][0], gcp[3][1], gcp[3][2], gcp[3][3] )
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
    ds = None
    
    if got_wkt == '':
        gdaltest.post_reason('did not expect null GetProjectionRef')
        print(got_wkt)
        return 'fail'
    
    if got_gcp_wkt != '':
        gdaltest.post_reason('did not expect non null GetGCPProjection')
        print(got_gcp_wkt)
        return 'fail'

    for i in range(6):
        if abs(got_gt[i] - src_gt[i]) > 1e-8:
            gdaltest.post_reason('did not get expected gt')
            print(got_gt)
            return 'fail'
            
    if got_gcp_count != 0:
        gdaltest.post_reason('did not expect GCPs')
        print(got_gcp_count)
        return 'fail'

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_lr = ogr.Geometry(ogr.wkbLinearRing)
    for i in range(4):
        expected_lr.AddPoint_2D(gcp[i][2], gcp[i][3])
    expected_lr.AddPoint_2D(gcp[0][2], gcp[0][3])
    expected_geom = ogr.Geometry(ogr.wkbPolygon)
    expected_geom.AddGeometry(expected_lr)

    if ogrtest.check_feature_geometry(got_geom, expected_geom) != 0:
        gdaltest.post_reason('bad neatline')
        print('got : %s' % got_neatline)
        return 'fail'

    gdaltest.pdf_drv.Delete(out_filename)

    return 'success'

def pdf_update_gcps_iso32000():
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', None)
    ret = pdf_update_gcps()
    return ret
    
def pdf_update_gcps_ogc_bp():
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', 'OGC_BP')
    ret = pdf_update_gcps()
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', None)
    return ret

###############################################################################
# Check SetGCPs() but with GCPs that do *not* resolve to a geotransform

def pdf_set_5_gcps_ogc_bp(dpi = 300):

    if gdaltest.pdf_drv is None:
        return 'skip'
        
    out_filename = 'tmp/pdf_set_5_gcps_ogc_bp.pdf'

    src_ds = gdal.Open('data/byte.tif')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()
    src_ds = None

    gcp = [ [ 2., 8., 0, 0 ],
            [ 2., 10., 0, 0 ],
            [ 2., 18., 0, 0 ],
            [ 16., 18., 0, 0 ],
            [ 16., 8., 0, 0 ] ]
             
    for i in range(len(gcp)):
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
</VRTDataset>""" % (  src_wkt,
                      gcp[0][0], gcp[0][1], gcp[0][2], gcp[0][3],
                      gcp[1][0], gcp[1][1], gcp[1][2], gcp[1][3],
                      gcp[2][0], gcp[2][1], gcp[2][2], gcp[2][3],
                      gcp[3][0], gcp[3][1], gcp[3][2], gcp[3][3],
                      gcp[4][0], gcp[4][1], gcp[4][2], gcp[4][3])
    vrt_ds = gdal.Open(vrt_txt)
    vrt_gcps = vrt_ds.GetGCPs()

    # Create PDF
    ds = gdaltest.pdf_drv.CreateCopy(out_filename, vrt_ds, options = ['GEO_ENCODING=OGC_BP', 'DPI=%d' % dpi])
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
    
    if got_wkt  != '':
        gdaltest.post_reason('did not expect non null GetProjectionRef')
        print(got_wkt)
        return 'fail'
    
    if got_gcp_wkt == '':
        gdaltest.post_reason('did not expect null GetGCPProjection')
        print(got_gcp_wkt)
        return 'fail'

    expected_gt = [0, 1, 0, 0, 0, 1]
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('did not get expected gt')
            print(got_gt)
            return 'fail'
            
    if got_gcp_count != len(gcp):
        gdaltest.post_reason('did not get expected GCP count')
        print(got_gcp_count)
        return 'fail'
        
    for i in range(got_gcp_count):
        if abs(got_gcps[i].GCPX - vrt_gcps[i].GCPX) > 1e-5 or \
           abs(got_gcps[i].GCPY - vrt_gcps[i].GCPY) > 1e-5 or \
           abs(got_gcps[i].GCPPixel - vrt_gcps[i].GCPPixel) > 1e-5 or \
           abs(got_gcps[i].GCPLine - vrt_gcps[i].GCPLine) > 1e-5:
           gdaltest.post_reason('did not get expected GCP (%d)' % i)
           print(got_gcps[i])
           print(vrt_gcps[i])
           return 'fail'

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    # Not sure this is really what we want, but without any geotransform, we cannot
    # find projected coordinates
    expected_geom = ogr.CreateGeometryFromWkt('POLYGON ((2 8,2 10,2 18,16 18,16 8,2 8))')

    if ogrtest.check_feature_geometry(got_geom, expected_geom) != 0:
        gdaltest.post_reason('bad neatline')
        print('got : %s' % got_neatline)
        print('expected : %s' % expected_geom.ExportToWkt())
        return 'fail'

    gdaltest.pdf_drv.Delete(out_filename)

    return 'success'


###############################################################################
# Check NEATLINE support

def pdf_set_neatline(geo_encoding, dpi = 300):

    if gdaltest.pdf_drv is None:
        return 'skip'

    out_filename = 'tmp/pdf_set_neatline.pdf'

    if geo_encoding == 'ISO32000':
        neatline = 'POLYGON ((441720 3751320,441720 3750120,441920 3750120,441920 3751320,441720 3751320))'
    else:  # For OGC_BP, we can use more than 4 points
        neatline = 'POLYGON ((441720 3751320,441720 3751000,441720 3750120,441920 3750120,441920 3751320,441720 3751320))'

    # Test CreateCopy() with NEATLINE
    src_ds = gdal.Open('data/byte.tif')
    expected_gt = src_ds.GetGeoTransform()
    ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options = ['NEATLINE=%s' % neatline, 'GEO_ENCODING=%s' % geo_encoding, 'DPI=%d' % dpi])
    ds = None
    src_ds = None

    # Check
    ds = gdal.Open(out_filename)
    got_gt = ds.GetGeoTransform()
    got_neatline = ds.GetMetadataItem('NEATLINE')
    ds = None

    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-7:
            gdaltest.post_reason('did not get expected gt')
            print(got_gt)
            return 'fail'

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_geom = ogr.CreateGeometryFromWkt(neatline)

    if ogrtest.check_feature_geometry(got_geom, expected_geom) != 0:
        gdaltest.post_reason('bad neatline')
        print('got : %s' % got_neatline)
        print('expected : %s' % expected_geom.ExportToWkt())
        return 'fail'

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
    ds = None
    gdal.SetConfigOption('GDAL_PDF_GEO_ENCODING', None)

    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-7:
            gdaltest.post_reason('did not get expected gt')
            print(got_gt)
            return 'fail'

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_geom = ogr.CreateGeometryFromWkt(neatline)

    if ogrtest.check_feature_geometry(got_geom, expected_geom) != 0:
        gdaltest.post_reason('bad neatline')
        print('got : %s' % got_neatline)
        print('expected : %s' % expected_geom.ExportToWkt())
        return 'fail'

    gdaltest.pdf_drv.Delete(out_filename)

    return 'success'

def pdf_set_neatline_iso32000():
    return pdf_set_neatline('ISO32000')

def pdf_set_neatline_ogc_bp():
    return pdf_set_neatline('OGC_BP')

###############################################################################
# Check that we can generate identical file

def pdf_check_identity_iso32000():

    if gdaltest.pdf_drv is None:
        return 'skip'

    out_filename = 'tmp/pdf_check_identity_iso32000.pdf'

    src_ds = gdal.Open('data/test_pdf.vrt')
    out_ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options = ['STREAM_COMPRESS=NONE'])
    del out_ds
    src_ds = None

    f = open('data/test_iso32000.pdf', 'rb')
    data_ref = f.read()
    f.close()

    f = open(out_filename, 'rb')
    data_got = f.read()
    f.close()

    gdaltest.pdf_drv.Delete(out_filename)

    if data_ref != data_got:
        gdaltest.post_reason('content does not match reference content')
        return 'fail'

    return 'success'

###############################################################################
# Check that we can generate identical file

def pdf_check_identity_ogc_bp():

    if gdaltest.pdf_drv is None:
        return 'skip'

    out_filename = 'tmp/pdf_check_identity_ogc_bp.pdf'

    src_ds = gdal.Open('data/test_pdf.vrt')
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'NO')
    out_ds = gdaltest.pdf_drv.CreateCopy(out_filename, src_ds, options = ['GEO_ENCODING=OGC_BP', 'STREAM_COMPRESS=NONE'])
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

    if data_ref != data_got:
        gdaltest.post_reason('content does not match reference content')
        return 'fail'

    return 'success'

###############################################################################
# Check layers support

def pdf_layers():

    if gdaltest.pdf_drv is None:
        return 'skip'

    if gdaltest.pdf_drv.GetMetadataItem('HAVE_POPPLER') == None:
        return 'skip'

    pdf_lib = gdal.GetConfigOption("GDAL_PDF_LIB")
    if pdf_lib is not None and pdf_lib == 'PODOFO':
        return 'skip'

    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    layers = ds.GetMetadata_List('LAYERS')
    cs1 = ds.GetRasterBand(1).Checksum()
    ds = None

    #if layers != ['LAYER_00_INIT_STATE=ON', 'LAYER_00_NAME=New_Data_Frame', 'LAYER_01_INIT_STATE=ON', 'LAYER_01_NAME=New_Data_Frame.Graticule', 'LAYER_02_INIT_STATE=ON', 'LAYER_02_NAME=Layers', 'LAYER_03_INIT_STATE=ON', 'LAYER_03_NAME=Layers.Measured_Grid', 'LAYER_04_INIT_STATE=ON', 'LAYER_04_NAME=Layers.Graticule']:
    if layers != ['LAYER_00_NAME=New_Data_Frame', 'LAYER_01_NAME=New_Data_Frame.Graticule', 'LAYER_02_NAME=Layers', 'LAYER_03_NAME=Layers.Measured_Grid', 'LAYER_04_NAME=Layers.Graticule']:
        gdaltest.post_reason('did not get expected layers')
        print(layers)
        return 'fail'

    if not pdf_checksum_available():
        return 'skip'

    # Turn a layer off
    gdal.SetConfigOption('GDAL_PDF_LAYERS_OFF', 'New_Data_Frame')
    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    cs2 = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_PDF_LAYERS_OFF', None)

    if cs2 == cs1:
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    # Turn the other layer on
    gdal.SetConfigOption('GDAL_PDF_LAYERS', 'Layers')
    ds = gdal.Open('data/adobe_style_geospatial.pdf')
    cs3 = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_PDF_LAYERS', None)

    # So the end result must be identical
    if cs3 != cs2:
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    # Turn another sublayer on
    ds = gdal.OpenEx('data/adobe_style_geospatial.pdf', open_options=['LAYERS=Layers.Measured_Grid'])
    cs4 = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs4 == cs1 or cs4 == cs2:
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test MARGIN, EXTRA_STREAM, EXTRA_LAYER_NAME and EXTRA_IMAGES options

def pdf_custom_layout():

    if gdaltest.pdf_drv is None:
        return 'skip'

    js = """button = app.alert({cMsg: 'This file was generated by GDAL. Do you want to visit its website ?', cTitle: 'Question', nIcon:2, nType:2});
if (button == 4) app.launchURL('http://gdal.org/');"""

    options = [ 'LEFT_MARGIN=1',
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
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_custom_layout.pdf', src_ds, options = options)
    ds = None
    src_ds = None

    if pdf_is_poppler():
        ds = gdal.Open('tmp/pdf_custom_layout.pdf')
        ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List('LAYERS')
        ds = None

    gdal.Unlink('tmp/pdf_custom_layout.pdf')

    if pdf_is_poppler():
        if layers != ['LAYER_00_NAME=byte_tif', 'LAYER_01_NAME=Footpage_and_logo']:
            gdaltest.post_reason('did not get expected layers')
            print(layers)
            return 'fail'

    return 'success'

###############################################################################
# Test CLIPPING_EXTENT, EXTRA_RASTERS, EXTRA_RASTERS_LAYER_NAME, OFF_LAYERS, EXCLUSIVE_LAYERS options

def pdf_extra_rasters():

    if gdaltest.pdf_drv is None:
        return 'skip'

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

    options = [ 'MARGIN=1',
                'DPI=300',
                'CLIPPING_EXTENT=440780,3750180,441860,3751260',
                'LAYER_NAME=byte_tif',
                'EXTRA_RASTERS=tmp/subbyte.vrt',
                'EXTRA_RASTERS_LAYER_NAME=subbyte',
                'OFF_LAYERS=byte_tif',
                'EXCLUSIVE_LAYERS=byte_tif,subbyte']

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_extra_rasters.pdf', src_ds, options = options)
    ds = None
    src_ds = None

    if pdf_is_poppler():
        ds = gdal.Open('tmp/pdf_extra_rasters.pdf')
        cs = ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List('LAYERS')
        ds = None

    gdal.Unlink('tmp/pdf_extra_rasters.pdf')
    os.unlink('tmp/subbyte.vrt')

    if pdf_is_poppler():
        if layers != ['LAYER_00_NAME=byte_tif', 'LAYER_01_NAME=subbyte']:
            gdaltest.post_reason('did not get expected layers')
            print(layers)
            return 'fail'
        if cs != 7926 and cs != 8177 and cs != 8174:
            gdaltest.post_reason('bad checksum')
            print(cs)
            return 'fail'

    return 'success'

###############################################################################
# Test adding a OGR datasource

def pdf_write_ogr():

    if gdaltest.pdf_drv is None:
        return 'skip'

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

    options = [ 'OGR_DATASOURCE=tmp/test.vrt', 'OGR_DISPLAY_LAYER_NAMES=A_Layer', 'OGR_DISPLAY_FIELD=foo' ]

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_write_ogr.pdf', src_ds, options = options)
    ds = None
    src_ds = None

    if pdf_is_poppler():
        ds = gdal.Open('tmp/pdf_write_ogr.pdf')
        cs_ref = ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List('LAYERS')
        ds = None

    ogr_ds = ogr.Open('tmp/pdf_write_ogr.pdf')
    ogr_lyr = ogr_ds.GetLayer(0)
    feature_count = ogr_lyr.GetFeatureCount()
    ogr_ds = None

    if pdf_is_poppler():
        gdal.SetConfigOption('GDAL_PDF_RENDERING_OPTIONS', 'RASTER')
        ds = gdal.Open('tmp/pdf_write_ogr.pdf')
        cs_raster = ds.GetRasterBand(1).Checksum()
        ds = None
        ds = gdal.OpenEx('tmp/pdf_write_ogr.pdf', open_options = ['RENDERING_OPTIONS=VECTOR'])
        cs_vector = ds.GetRasterBand(1).Checksum()
        ds = None
        gdal.SetConfigOption('GDAL_PDF_RENDERING_OPTIONS', 'RASTER,VECTOR,TEXT')
        ds = gdal.Open('tmp/pdf_write_ogr.pdf')
        cs_raster_vector_text = ds.GetRasterBand(1).Checksum()
        ds = None
        gdal.SetConfigOption('GDAL_PDF_RENDERING_OPTIONS', None)

        if cs_raster == cs_ref:
            gdaltest.post_reason('fail')
            print(cs_ref)
            print(cs_raster)
            return 'fail'

        if cs_vector == cs_ref:
            gdaltest.post_reason('fail')
            print(cs_ref)
            print(cs_vector)
            return 'fail'

        if cs_raster_vector_text != cs_ref:
            gdaltest.post_reason('fail')
            print(cs_ref)
            print(cs_raster_vector_text)
            return 'fail'

    gdal.Unlink('tmp/pdf_write_ogr.pdf')

    gdal.Unlink('tmp/test.csv')
    gdal.Unlink('tmp/test.vrt')

    if pdf_is_poppler():
        if layers != ['LAYER_00_NAME=A_Layer', 'LAYER_01_NAME=A_Layer.Text']:
            gdaltest.post_reason('did not get expected layers')
            print(layers)
            return 'fail'

    # Should have filtered out id = 4
    if feature_count != 3:
        gdaltest.post_reason('did not get expected feature count')
        print(feature_count)
        return 'fail'

    return 'success'

###############################################################################
# Test adding a OGR datasource with reprojection of OGR SRS to GDAL SRS

def pdf_write_ogr_with_reprojection():

    if gdaltest.pdf_drv is None:
        return 'skip'

    import osr_ct
    osr_ct.osr_ct_1()
    if gdaltest.have_proj4 == 0:
        return 'skip'

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

    options = [ 'OGR_DATASOURCE=tmp/test.vrt', 'OGR_DISPLAY_LAYER_NAMES=A_Layer', 'OGR_DISPLAY_FIELD=foo' ]

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_write_ogr.pdf', src_ds, options = options)
    del ds
    src_ds = None

    ogr_ds = ogr.Open('tmp/pdf_write_ogr.pdf')
    ogr_lyr = ogr_ds.GetLayer(0)
    feature_count = ogr_lyr.GetFeatureCount()
    ogr_ds = None

    gdal.Unlink('tmp/pdf_write_ogr.pdf')

    gdal.Unlink('tmp/test.csv')
    gdal.Unlink('tmp/test.vrt')

    # Should have filtered out id = 6
    if feature_count != 5:
        gdaltest.post_reason('did not get expected feature count')
        print(feature_count)
        return 'fail'

    return 'success'

###############################################################################
# Test direct copy of source JPEG file

def pdf_jpeg_direct_copy():

    if gdaltest.pdf_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    src_ds = gdal.Open('data/byte_with_xmp.jpg')
    ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_jpeg_direct_copy.pdf', src_ds, options = ['XMP=NO'])
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/pdf_jpeg_direct_copy.pdf')
    # No XMP at PDF level
    if ds.GetMetadata('xml:XMP') is not None:
        gdaltest.post_reason('failed')
        return 'fail'
    if ds.RasterXSize != 20:
        gdaltest.post_reason('failed')
        return 'fail'
    if pdf_checksum_available() and ds.GetRasterBand(1).Checksum() == 0:
        gdaltest.post_reason('failed')
        return 'fail'
    ds = None

    # But we can find the original XMP from the JPEG file !
    f = open('tmp/pdf_jpeg_direct_copy.pdf', 'rb')
    data = f.read().decode('ISO-8859-1')
    f.close()
    offset = data.find('ns.adobe.com')

    gdal.Unlink('tmp/pdf_jpeg_direct_copy.pdf')

    if offset == -1:
        gdaltest.post_reason('failed')
        return 'fail'

    return 'success'

###############################################################################
# Test direct copy of source JPEG file within VRT file

def pdf_jpeg_in_vrt_direct_copy():

    if gdaltest.pdf_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

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
    if ds.GetMetadata('xml:XMP') is not None:
        gdaltest.post_reason('failed')
        return 'fail'
    if ds.RasterXSize != 20:
        gdaltest.post_reason('failed')
        return 'fail'
    if pdf_checksum_available() and ds.GetRasterBand(1).Checksum() == 0:
        gdaltest.post_reason('failed')
        return 'fail'
    ds = None

    # But we can find the original XMP from the JPEG file !
    f = open('tmp/pdf_jpeg_in_vrt_direct_copy.pdf', 'rb')
    data = f.read().decode('ISO-8859-1')
    f.close()
    offset = data.find('ns.adobe.com')

    gdal.Unlink('tmp/pdf_jpeg_in_vrt_direct_copy.pdf')

    if offset == -1:
        gdaltest.post_reason('failed')
        return 'fail'

    return 'success'

###############################################################################
# Test reading georeferencing attached to an image, and not to the page (#4695)

def pdf_georef_on_image(src_filename = 'data/byte.tif'):

    if gdaltest.pdf_drv is None:
        return 'skip'

    src_ds = gdal.Open(src_filename)
    gdal.SetConfigOption('GDAL_PDF_WRITE_GEOREF_ON_IMAGE', 'YES')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_georef_on_image.pdf', src_ds, options = ['MARGIN=10', 'GEO_ENCODING=NONE'])
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

    gdal.Unlink('tmp/pdf_georef_on_image.pdf')

    if got_wkt == '':
        gdaltest.post_reason('did not get projection')
        return 'fail'

    if pdf_checksum_available() and src_cs != got_cs:
        gdaltest.post_reason('did not get same checksum')
        print(src_cs)
        print(got_cs)
        return 'fail'

    return 'success'

def pdf_georef_on_image_rgb():
    return pdf_georef_on_image('data/rgbsmall.tif')

###############################################################################
# Test writing a PDF that hits Acrobat limits in term of page dimensions (#5412)

def pdf_write_huge():

    if gdaltest.pdf_drv is None:
        return 'skip'

    for (xsize, ysize) in [ (19200,1), (1,19200) ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', xsize, ysize, 1)
        ds = gdaltest.pdf_drv.CreateCopy('/vsimem/pdf_write_huge.pdf', src_ds)
        ds = None
        ds = gdal.Open('/vsimem/pdf_write_huge.pdf')
        if int(ds.GetMetadataItem('DPI')) != 96:
            gdaltest.post_reason('failure')
            print(ds.GetMetadataItem('DPI'))
            return 'fail'
        if ds.RasterXSize != src_ds.RasterXSize or \
        ds.RasterYSize != src_ds.RasterYSize:
            gdaltest.post_reason('failure')
            print(ds.RasterXSize)
            print(ds.RasterYSize)
            return 'fail'
        ds = None

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds = gdaltest.pdf_drv.CreateCopy('/vsimem/pdf_write_huge.pdf', src_ds, options = ['DPI=72'])
        gdal.PopErrorHandler()
        msg = gdal.GetLastErrorMsg()
        if msg == '':
            gdaltest.post_reason('failure')
            return 'fail'
        ds = None
        ds = gdal.Open('/vsimem/pdf_write_huge.pdf')
        if int(ds.GetMetadataItem('DPI')) != 72:
            gdaltest.post_reason('failure')
            print(ds.GetMetadataItem('DPI'))
            return 'fail'
        ds = None

        src_ds = None

    for option in [ 'LEFT_MARGIN=14400', 'TOP_MARGIN=14400' ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds = gdaltest.pdf_drv.CreateCopy('/vsimem/pdf_write_huge.pdf', src_ds, options = [ option ])
        gdal.PopErrorHandler()
        msg = gdal.GetLastErrorMsg()
        if msg == '':
            gdaltest.post_reason('failure')
            return 'fail'
        ds = None
        ds = gdal.Open('/vsimem/pdf_write_huge.pdf')
        if int(ds.GetMetadataItem('DPI')) != 72:
            gdaltest.post_reason('failure')
            print(ds.GetMetadataItem('DPI'))
            return 'fail'
        ds = None

        src_ds = None

    gdal.Unlink('/vsimem/pdf_write_huge.pdf')

    return 'success'

gdaltest_list = [
    pdf_init,
    pdf_online_1,
    pdf_online_2,
    pdf_1,
    pdf_iso32000,
    pdf_iso32000_dpi_300,
    pdf_ogcbp,
    pdf_ogcbp_dpi_300,
    pdf_ogcbp_lcc,
    pdf_no_compression,
    pdf_jpeg_compression,
    pdf_jp2_auto_compression,
    pdf_jp2kak_compression,
    pdf_jp2ecw_compression,
    pdf_jp2openjpeg_compression,
    pdf_jpeg2000_compression,
    pdf_jp2ecw_compression_rgb,
    pdf_jpeg_compression_rgb,
    pdf_rgba_default_compression,
    pdf_rgba_default_compression_tiled,
    pdf_jpeg_compression_rgba,
    pdf_predictor_2,
    pdf_predictor_2_rgb,
    pdf_tiled,
    pdf_tiled_128,
    pdf_color_table,
    pdf_xmp,
    pdf_info,
    pdf_update_gt,
    pdf_update_info,
    pdf_update_xmp,
    pdf_update_gcps_iso32000,
    pdf_update_gcps_ogc_bp,
    pdf_set_5_gcps_ogc_bp,
    pdf_set_neatline_iso32000,
    pdf_set_neatline_ogc_bp,
    pdf_check_identity_iso32000,
    pdf_check_identity_ogc_bp,
    pdf_layers,
    pdf_custom_layout,
    pdf_extra_rasters,
    pdf_write_ogr,
    pdf_write_ogr_with_reprojection,
    pdf_jpeg_direct_copy,
    pdf_jpeg_in_vrt_direct_copy,
    pdf_georef_on_image,
    pdf_georef_on_image_rgb,
    pdf_write_huge,

    pdf_switch_underlying_lib,

    pdf_iso32000,
    pdf_ogcbp,
    pdf_update_gt,
    pdf_update_info,
    pdf_update_xmp,
    pdf_rgba_default_compression_tiled,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'PDF' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

