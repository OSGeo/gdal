#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PDF Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

    return 'success'

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

    if ds.RasterXSize != 620:
        gdaltest.post_reason('bad dimensions')
        return 'fail'

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    expected_gt = (-77.112328333299999, 1.8333311999999999e-05, 0.0, 38.897842488372, -0.0, -1.8333311999999999e-05)
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

    expected_gt = (-77.112328333299999, 1.8333311999999999e-05, 0.0, 38.897842488372, -0.0, -1.8333311999999999e-05)
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
        if abs(gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('bad geotransform')
            print(gt)
            print(expected_gt)
            return 'fail'

    expected_wkt = 'PROJCS["WGS_1984_UTM_Zone_20N",GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_84",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["False_Easting",500000.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-63.0],PARAMETER["Scale_Factor",0.9996],PARAMETER["Latitude_Of_Origin",0.0],UNIT["Meter",1.0]]'
    if wkt != expected_wkt:
        gdaltest.post_reason('bad WKT')
        print(wkt)
        return 'fail'

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

    gdal.SetConfigOption('GDAL_PDF_DPI', '72')
    tst = gdaltest.GDALTest( 'PDF', 'byte.tif', 1, None )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 1, check_srs = True, check_checksum_not_null = True)
    gdal.SetConfigOption('GDAL_PDF_DPI', None)

    return ret

###############################################################################
# Test write support with OGC_BP geo encoding

def pdf_ogcbp():

    if gdaltest.pdf_drv is None:
        return 'skip'

    gdal.SetConfigOption('GDAL_PDF_DPI', '72')
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    tst = gdaltest.GDALTest( 'PDF', 'byte.tif', 1, None, options = ['GEO_ENCODING=OGC_BP'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 1, check_srs = True, check_checksum_not_null = True)
    gdal.SetConfigOption('GDAL_PDF_DPI', None)
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

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/temp.tif', 1, 1)
    src_ds.SetProjection(wkt)
    src_ds.SetGeoTransform([500000,1,0,1000000,0,-1])

    gdal.SetConfigOption('GDAL_PDF_DPI', '72')
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', 'FALSE')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/pdf_ogcbp_lcc.pdf', src_ds)
    out_wkt = out_ds.GetProjectionRef()
    out_ds = None
    gdal.SetConfigOption('GDAL_PDF_DPI', None)
    gdal.SetConfigOption('GDAL_PDF_OGC_BP_WRITE_WKT', None)

    src_ds = None

    gdal.Unlink('/vsimem/temp.tif')
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
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = True)

    return ret

###############################################################################
# Test compression methods

def pdf_jpeg_compression(filename = 'byte.tif'):

    if gdaltest.pdf_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', filename, 1, None, options = ['COMPRESS=JPEG'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = True)

    return ret

def pdf_jpx_compression(filename, drv_name = None):

    if gdaltest.pdf_drv is None:
        return 'skip'

    if drv_name is None:
        if gdal.GetDriverByName('JP2KAK') is None and \
           gdal.GetDriverByName('JP2ECW') is None and \
           gdal.GetDriverByName('JP2OpenJpeg') is None and \
           gdal.GetDriverByName('JPEG2000') is None :
            return 'skip'
    elif gdal.GetDriverByName(drv_name) is None:
        return 'skip'

    if drv_name is None:
        options = ['COMPRESS=JPEG2000']
    else:
        options = ['COMPRESS=JPEG2000', 'JPEG2000_DRIVER=%s' % drv_name]

    tst = gdaltest.GDALTest( 'PDF', filename, 1, None, options = options )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = True)

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

def pdf_rgba_default_compression():

    if gdaltest.pdf_drv is None:
        return 'skip'

    src_ds = gdal.Open( '../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.pdf_drv.CreateCopy('tmp/rgba.pdf', src_ds)
    out_ds = None

    gdal.SetConfigOption('GDAL_PDF_BANDS', '4')
    gdal.SetConfigOption('GDAL_PDF_DPI', '72')
    out_ds = gdal.Open('tmp/rgba.pdf')
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs2 = out_ds.GetRasterBand(2).Checksum()
    cs3 = out_ds.GetRasterBand(3).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()

    src_cs1 = src_ds.GetRasterBand(1).Checksum()
    src_cs2 = src_ds.GetRasterBand(2).Checksum()
    src_cs3 = src_ds.GetRasterBand(3).Checksum()
    src_cs4 = src_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.SetConfigOption('GDAL_PDF_BANDS', None)
    gdal.SetConfigOption('GDAL_PDF_DPI', None)

    gdal.Unlink('tmp/rgba.pdf')

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

def pdf_jpeg_compression_rgba():
    return pdf_jpeg_compression('../../gcore/data/stefan_full_rgba.tif')


def pdf_tiled():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'utm.tif', 1, None, options = ['COMPRES=DEFLATE', 'TILED=YES'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = True)

    return ret

def pdf_tiled_128():

    if gdaltest.pdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PDF', 'utm.tif', 1, None, options = ['BLOCKXSIZE=128', 'BLOCKYSIZE=128'] )
    ret = tst.testCreateCopy(check_minmax = 0, check_gt = 0, check_srs = None, check_checksum_not_null = True)

    return ret
    
gdaltest_list = [
    pdf_init,
    pdf_online_1,
    pdf_online_2,
    pdf_1,
    pdf_iso32000,
    pdf_ogcbp,
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
    pdf_jpeg_compression_rgba,
    pdf_tiled,
    pdf_tiled_128
]

if __name__ == '__main__':

    gdaltest.setup_run( 'PDF' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

