#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JP2OpenJPEG driver.
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
import string
import array
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def jp2openjpeg_1():

    try:
        gdaltest.jp2openjpeg_drv = gdal.GetDriverByName( 'JP2OpenJPEG' )
    except:
        gdaltest.jp2openjpeg_drv = None
        return 'skip'

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

    return 'success'
	
###############################################################################
# Open byte.jp2

def jp2openjpeg_2():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

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

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'byte.jp2', 1, 50054 )
    return tst.testOpen( check_prj = srs, check_gt = gt )

###############################################################################
# Open int16.jp2

def jp2openjpeg_3():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/int16.jp2' )
    ds_ref = gdal.Open( 'data/int16.tif' )

    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Quite a bit of difference...
    if maxdiff > 6:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test copying byte.jp2

def jp2openjpeg_4(out_filename = 'tmp/jp2openjpeg_4.jp2'):

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.jp2')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()

    gdal.Unlink(out_filename)

    out_ds = gdal.GetDriverByName('JP2OpenJPEG').CreateCopy(out_filename, src_ds, options = ['REVERSIBLE=YES', 'QUALITY=100'])
    out_ds = None

    gdal.Unlink(out_filename + '.aux.xml')

    ds = gdal.Open(out_filename)
    cs = ds.GetRasterBand(1).Checksum()
    got_wkt = ds.GetProjectionRef()
    got_gt = ds.GetGeoTransform()
    ds = None

    gdal.Unlink(out_filename)

    sr1 = osr.SpatialReference()
    sr1.SetFromUserInput(got_wkt)
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(src_wkt)

    if sr1.IsSame(sr2) == 0:
        gdaltest.post_reason('bad spatial reference')
        print(got_wkt)
        print(src_wkt)
        return 'fail'

    for i in range(6):
        if abs(got_gt[i] - src_gt[i]) > 1e-8:
            gdaltest.post_reason('bad geotransform')
            print(got_gt)
            print(src_gt)
            return 'fail'

    if cs != 50054:
        gdaltest.post_reason('bad checksum')
        print(cs)
        return 'fail'

    return 'success'


def jp2openjpeg_4_vsimem():
    return jp2openjpeg_4('/vsimem/jp2openjpeg_4.jp2')

###############################################################################
# Test copying int16.jp2

def jp2openjpeg_5():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'int16.jp2', 1, None, options = ['RESOLUTIONS=1','REVERSIBLE=YES', 'QUALITY=100', 'CODEC=J2K'] )
    return tst.testCreateCopy()

###############################################################################
# Test reading ll.jp2

def jp2openjpeg_6():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'll.jp2', 1, None )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('data/ll.jp2')
    ds.GetRasterBand(1).Checksum()
    ds = None

    return 'success'
    
###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)

def jp2openjpeg_7():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'JP2OpenJPEG', '/vsigzip/data/byte.jp2.gz', 1, 50054, filename_absolute = 1 )
    return tst.testOpen()
    
###############################################################################
# Test a JP2OpenJPEG with the 3 bands having 13bit depth and the 4th one 1 bit

def jp2openjpeg_8():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'
    
    # This test will cause a crash with an unpatched version of Jasper, such as the one of Ubuntu 8.04 LTS
    # --> "jpc_dec.c:1072: jpc_dec_tiledecode: Assertion `dec->numcomps == 3' failed."
    # Recent Debian/Ubuntu have the appropriate patch.
    # So we try to run in a subprocess first
    import test_cli_utilities
    if test_cli_utilities.get_gdalinfo_path() is not None:
        ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --config GDAL_SKIP "JP2ECW JP2MRSID JP2KAK" data/3_13bit_and_1bit.jp2')
        if ret.find('Band 1') == -1:
            gdaltest.post_reason('Jasper library would need patches')
            return 'fail'
    
    ds = gdal.Open('data/3_13bit_and_1bit.jp2')
    
    expected_checksums = [ 64570, 57277, 56048, 61292]
    
    for i in range(4):
        if ds.GetRasterBand(i+1).Checksum() != expected_checksums[i]:
            gdaltest.post_reason('unexpected checksum (%d) for band %d' % (expected_checksums[i], i+1))
            return 'fail'

    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('unexpected data type')
        return 'fail'
            
    return 'success'
    
###############################################################################
def jp2openjpeg_online_1():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        return 'skip'

    # Checksum = 32669 on my PC
    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

    return 'success'

###############################################################################
def jp2openjpeg_online_2():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        return 'skip'

    # Checksum = 15621 on my PC
    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/gcp.jp2', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/gcp.jp2')
    ds.GetRasterBand(1).Checksum()
    if len(ds.GetGCPs()) != 15:
        gdaltest.post_reason('bad number of GCP')
        return 'fail'

    expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]"""
    if ds.GetGCPProjection() != expected_wkt:
        gdaltest.post_reason('bad GCP projection')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
def jp2openjpeg_online_3():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/Bretagne1.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne1.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    if maxdiff > 17:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
def jp2openjpeg_online_4():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'expected_fail'

    ds = gdal.Open('tmp/cache/Bretagne2.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne2.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, 0, 0, 1024, 1024)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    if maxdiff > 10:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Try reading JP2OpenJPEG with color table

def jp2openjpeg_online_5():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9.jp2', 'file9.jp2'):
        return 'skip'

    ds = gdal.Open('tmp/cache/file9.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    if cs1 != 48954 or cs2 != 4939 or cs3 != 17734:
        print(cs1, cs2, cs3)
        gdaltest.post_reason('Did not get expected checksums')
        return 'fail'
        
    ds = None

    return 'success'

###############################################################################
# Try reading YCbCr JP2OpenJPEG as RGB

def jp2openjpeg_online_6():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_03/file3.jp2', 'file3.jp2'):
        return 'skip'

    ds = gdal.Open('tmp/cache/file3.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    if cs1 != 26140 or cs2 != 32689 or cs3 != 48247:
        print(cs1, cs2, cs3)
        gdaltest.post_reason('Did not get expected checksums')
        return 'fail'
        
    ds = None

    return 'success'

###############################################################################
def jp2openjpeg_cleanup():

    gdaltest.reregister_all_jpeg2000_drivers()

    return 'success'

gdaltest_list = [
    jp2openjpeg_1,
    jp2openjpeg_2,
    jp2openjpeg_3,
    jp2openjpeg_4,
    jp2openjpeg_4_vsimem,
    jp2openjpeg_5,
    jp2openjpeg_6,
    jp2openjpeg_7,
    #jp2openjpeg_8,
    jp2openjpeg_online_1,
    jp2openjpeg_online_2,
    jp2openjpeg_online_3,
    jp2openjpeg_online_4,
    #jp2openjpeg_online_5,
    jp2openjpeg_online_6,
    jp2openjpeg_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'jp2openjpeg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

