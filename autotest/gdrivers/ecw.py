#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ECW driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
import os.path
import sys
import string
import array
import shutil
from osgeo import gdal
from osgeo import osr
from sys import version_info

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
def has_write_support():
    if hasattr(gdaltest, 'b_ecw_has_write_support'):
        return gdaltest.b_ecw_has_write_support
    gdaltest.b_ecw_has_write_support = False
    if ecw_1() != 'success':
        return False
    if ecw_3() == 'success':
        gdaltest.b_ecw_has_write_support = True
    try:
        os.remove('tmp/jrc_out.ecw')
    except:
        pass
    return gdaltest.b_ecw_has_write_support

###############################################################################
#

def ecw_init():

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2ECW')
    return 'success'

###############################################################################
# Verify we have the driver.

def ecw_1():

    try:
        gdaltest.ecw_drv = gdal.GetDriverByName( 'ECW' )
        gdaltest.jp2ecw_drv = gdal.GetDriverByName( 'JP2ECW' )
    except:
        gdaltest.ecw_drv = None
        gdaltest.jp2ecw_drv = None

    gdaltest.ecw_write = 0

    if gdaltest.ecw_drv is not None:
        if gdaltest.ecw_drv.GetMetadataItem('DMD_CREATIONDATATYPES') != None:
            gdaltest.ecw_write = 1

        longname = gdaltest.ecw_drv.GetMetadataItem('DMD_LONGNAME')

        sdk_off = longname.find('SDK ')
        if sdk_off != -1:
            gdaltest.ecw_drv.major_version = int(longname[sdk_off+4])
        else:
            gdaltest.ecw_drv.major_version = 3
    # we set ECW to not resolve projection and datum strings to get 3.x behavior.     
    gdal.SetConfigOption("ECW_DO_NOT_RESOLVE_DATUM_PROJECTION", "YES")
    return 'success'

###############################################################################
# Verify various information about our test image. 

def ecw_2():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (141.172, 67.3636)
    else:
        if gdaltest.ecw_drv.major_version == 5:
            (exp_mean, exp_stddev) = (141.606,67.2919)
        else:
            (exp_mean, exp_stddev) = (140.332, 67.611)

    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    if abs(mean-exp_mean) > 0.5 or abs(stddev-exp_stddev) > 0.5:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-467498.5) > 0.1 \
        or abs(geotransform[1]-16.5475) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-5077883.2825) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -16.5475) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    return 'success'

###############################################################################
# Verify that an write the imagery out to a new file.

def ecw_3():
    if gdaltest.ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    out_ds = gdaltest.ecw_drv.CreateCopy( 'tmp/jrc_out.ecw', ds, options = ['TARGET=75'] )
    if out_ds is not None:
        version = out_ds.GetMetadataItem('VERSION')
        if version != '2':
            gdaltest.post_reason('bad VERSION')
            return 'fail'

    ds = None
    
    if out_ds is None:
        if gdal.GetLastErrorMsg().find('ECW_ENCODE_KEY') >= 0:
            gdaltest.ecw_write = 0
            return 'skip'

    gdaltest.b_ecw_has_write_support = True

    return 'success' 

###############################################################################
# Verify various information about our generated image. 

def ecw_4():

    if gdaltest.ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    try:
        os.unlink('tmp/jrc_out.ecw.aux.xml')
    except:
        pass

    ds = gdal.Open( 'tmp/jrc_out.ecw' )
    version = ds.GetMetadataItem('VERSION')
    if version != '2':
        gdaltest.post_reason('bad VERSION')
        return 'fail'

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (140.290, 66.6303)
    else:
        if gdaltest.ecw_drv.major_version == 5:
            (exp_mean, exp_stddev) = (141.517,67.1285)
        else: 
            (exp_mean, exp_stddev) = (138.971, 67.716)

    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    if abs(mean-exp_mean) > 1.5 or abs(stddev-exp_stddev) > 0.5:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-467498.5) > 0.1 \
        or abs(geotransform[1]-16.5475) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-5077883.2825) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -16.5475) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Now try writing a JPEG2000 compressed version of the same with the ECW driver

def ecw_5():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'data/small.vrt' )
    ds_out = gdaltest.jp2ecw_drv.CreateCopy( 'tmp/ecw_5.jp2', ds, options = ['TARGET=75'] )
    if ds_out.GetDriver().ShortName != "JP2ECW":
        return 'fail'
    version = ds_out.GetMetadataItem('VERSION')
    if version != '1':
        gdaltest.post_reason('bad VERSION')
        return 'fail'
    ds = None

    return 'success' 

###############################################################################
# Verify various information about our generated image. 

def ecw_6():

    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'tmp/ecw_5.jp2' )

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (144.422, 44.9075)
    else:
        (exp_mean, exp_stddev) = (143.375, 44.8539)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    # The difference in the stddev is outragously large between win32 and
    # linux, but I don't know why. 
    if abs(mean-exp_mean) > 1.5 or abs(stddev-exp_stddev) > 6:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    (mean, stddev) = ds.GetRasterBand(2).ComputeBandStats()

    # The difference in the stddev is outragously large between win32 and
    # linux, but I don't know why. 
    if abs(mean-exp_mean) > 1.0 or abs(stddev-exp_stddev) > 6:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-440720) > 0.1 \
        or abs(geotransform[1]-60) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-3751320) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -60) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    prj = ds.GetProjectionRef()
    if prj.find('UTM') == -1 or prj.find('NAD27') == -1 \
       or prj.find('one 11') == -1:
        print(prj)
        gdaltest.post_reason( 'Coordinate system not UTM 11, NAD27?' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Write the same image to NITF.

def ecw_7():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'data/small.vrt' )
    drv = gdal.GetDriverByName( 'NITF' )
    drv.CreateCopy( 'tmp/ecw_7.ntf', ds, options = ['IC=C8', 'TARGET=75'], strict = 0 )
    ds = None

    return 'success' 

###############################################################################
# Verify various information about our generated image. 

def ecw_8():

    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'tmp/ecw_7.ntf' )
    
    (exp_mean, exp_stddev) = (145.57, 43.1712)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    if abs(mean-exp_mean) > 1.0 or abs(stddev-exp_stddev) > 1.0:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-440720) > 0.1 \
        or abs(geotransform[1]-60) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-3751320) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -60) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    prj = ds.GetProjectionRef()
    if prj.find('UTM Zone 11') == -1 or prj.find('WGS84') == -1:
        gdaltest.post_reason( 'Coordinate system not UTM 11, WGS84?' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Try writing 16bit JP2 file directly using Create().

def ecw_9():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    # This always crashe on Frank's machine - some bug in old sdk.
    try:
        if os.environ['USER'] == 'warmerda' and gdaltest.ecw_drv.major_version == 3:
            return 'skip'
    except:
        pass

    ds = gdaltest.jp2ecw_drv.Create( 'tmp/ecw9.jp2', 200, 100, 1,
                                     gdal.GDT_Int16, options = ['TARGET=75'] )
    ds.SetGeoTransform( (100, 0.1, 0.0, 30.0, 0.0, -0.1 ) )

    ds.SetProjection( 'GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]' )

    raw_data = array.array('h',list(range(200))).tostring()

    for line in range(100):
        ds.WriteRaster( 0, line, 200, 1, raw_data,
                        buf_type = gdal.GDT_Int16 )
    ds = None

    return 'success'

###############################################################################
# Verify previous 16bit file.

def ecw_10():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    # This always crashe on Frank's machine - some bug in old sdk.
    try:
        if os.environ['USER'] == 'warmerda' and gdaltest.ecw_drv.major_version == 3:
            return 'skip'
    except:
        pass

    ds = gdal.Open( 'tmp/ecw9.jp2' )
    
    (exp_mean, exp_stddev) = (98.49, 57.7129)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    if abs(mean-exp_mean) > 1.1 or abs(stddev-exp_stddev) > 0.1:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-100) > 0.1 \
        or abs(geotransform[1]-0.1) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-30) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -0.1) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    # should check the projection, but I'm too lazy just now.

    return 'success' 

###############################################################################
# Test direct creation of an NITF/JPEG2000 file.

def ecw_11():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    drv = gdal.GetDriverByName( 'NITF' )
    ds = drv.Create( 'tmp/test_11.ntf', 200, 100, 3, gdal.GDT_Byte,
                     [ 'ICORDS=G' ] )
    ds.SetGeoTransform( (100, 0.1, 0.0, 30.0, 0.0, -0.1 ) )

    my_list = list(range(200)) + list(range(20,220)) + list(range(30,230))
    raw_data = array.array('h',my_list).tostring()

    for line in range(100):
        ds.WriteRaster( 0, line, 200, 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1,2,3] )

    ds.GetRasterBand( 1 ).SetRasterColorInterpretation( gdal.GCI_BlueBand )
    ds.GetRasterBand( 2 ).SetRasterColorInterpretation( gdal.GCI_GreenBand )
    ds.GetRasterBand( 3 ).SetRasterColorInterpretation( gdal.GCI_RedBand )

    ds = None

    return 'success'

###############################################################################
# Verify previous file

def ecw_12():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'tmp/test_11.ntf' )
    
    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-100) > 0.1 \
        or abs(geotransform[1]-0.1) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-30.0) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -0.1) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason( 'Got wrong color interpretation.' )
        return 'fail'

    if ds.GetRasterBand(2).GetRasterColorInterpretation() !=gdal.GCI_GreenBand:
        gdaltest.post_reason( 'Got wrong color interpretation.' )
        return 'fail'

    if ds.GetRasterBand(3).GetRasterColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason( 'Got wrong color interpretation.' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# This is intended to verify that the ECWDataset::RasterIO() special case
# works properly.  It is used to copy subwindow into a memory dataset
# which we then checksum.  To stress the RasterIO(), we also change data
# type and select an altered band list. 

def ecw_13():
    if gdaltest.jp2ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/rgb16_ecwsdk.jp2' )

    wrktype = gdal.GDT_Float32
    raw_data = ds.ReadRaster( 10, 10, 40, 40, buf_type = wrktype,
                              band_list = [3,2,1] )
    ds = None

    drv = gdal.GetDriverByName( 'MEM' )
    ds = drv.Create( 'workdata', 40, 40, 3, wrktype )
    ds.WriteRaster( 0, 0, 40, 40, raw_data, buf_type = wrktype )
    
    checksums = ( ds.GetRasterBand(1).Checksum(),
                  ds.GetRasterBand(2).Checksum(),
                  ds.GetRasterBand(3).Checksum() )
    ds = None

    if checksums != ( 19253, 17848, 19127 ):
        gdaltest.post_reason( 'Expected checksums do match expected checksums')
        print(checksums)
        return 'fail'

    return 'success'

###############################################################################
# Write out image with GCPs.

def ecw_14():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'data/rgb_gcp.vrt' )
    gdaltest.jp2ecw_drv.CreateCopy( 'tmp/rgb_gcp.jp2', ds )
    ds = None

    return 'success' 
###############################################################################
# Verify various information about our generated image. 

def ecw_15():

    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'tmp/rgb_gcp.jp2' )

    gcp_srs = ds.GetGCPProjection()
    if gcp_srs[:6] != 'GEOGCS' \
       or gcp_srs.find('WGS') == -1 \
       or gcp_srs.find('84') == -1:
        gdaltest.post_reason('GCP Projection not retained.')
        print(gcp_srs)
        return 'fail'

    gcps = ds.GetGCPs()
    if len(gcps) != 4 \
       or gcps[1].GCPPixel != 0 \
       or gcps[1].GCPLine  != 50 \
       or gcps[1].GCPX     != 0 \
       or gcps[1].GCPY     != 50 \
       or gcps[1].GCPZ     != 0:
        gdaltest.post_reason( 'GCPs wrong.' )
        print(gcps)
        return 'fail'
    
    ds = None

    return 'success'

###############################################################################
# Open byte.jp2

def ecw_16():

    if gdaltest.jp2ecw_drv is None:
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
    
    tst = gdaltest.GDALTest( 'JP2ECW', 'byte.jp2', 1, 50054 )
    return tst.testOpen( check_prj = srs, check_gt = gt )

###############################################################################
# Open int16.jp2

def ecw_17():

    if gdaltest.jp2ecw_drv is None:
        return 'skip'

    if gdaltest.ecw_drv.major_version == 4:
        gdaltest.post_reason( '4.x SDK gets unreliable results for jp2')
        return 'skip'

    ds = gdal.Open( 'data/int16.jp2' )
    ds_ref = gdal.Open( 'data/int16.tif' )
    
    maxdiff = gdaltest.compare_ds(ds, ds_ref)

    ds = None
    ds_ref = None
    
    # Quite a bit of difference...
    if maxdiff > 6:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)

def ecw_18():

    if gdaltest.jp2ecw_drv is None:
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
    
    tst = gdaltest.GDALTest( 'JP2ECW', '/vsigzip/data/byte.jp2.gz', 1, 50054, filename_absolute = 1 )
    return tst.testOpen( check_prj = srs, check_gt = gt )
    
###############################################################################
# Test a JPEG2000 with the 3 bands having 13bit depth and the 4th one 1 bit

def ecw_19():

    if gdaltest.jp2ecw_drv is None:
        return 'skip'
    
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
# Confirm that we have an overview for this image and that the statistics 
# are as expected.

def ecw_20():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )

    band = ds.GetRasterBand(1)
    if band.GetOverviewCount() != 1:
        gdaltest.post_reason( 'did not get expected number of overview')
        return 'fail'

    # Both requests should go *exactly* to the same code path
    data_subsampled = band.ReadRaster(0, 0, 400, 400, 200, 200)
    data_overview = band.GetOverview(0).ReadRaster(0, 0, 200, 200)
    if data_subsampled != data_overview:
        gdaltest.post_reason('inconsistant overview behaviour')
        return 'fail'

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (141.644, 67.2186)
    else:
        if gdaltest.ecw_drv.major_version == 5: 
            (exp_mean, exp_stddev) = (142.189, 62.4223)
        else: 
            (exp_mean, exp_stddev) = (140.889, 62.742)
    (mean, stddev) = band.GetOverview(0).ComputeBandStats()

    if abs(mean-exp_mean) > 0.5 or abs(stddev-exp_stddev) > 0.5:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    return 'success'

###############################################################################
# This test is intended to go through an optimized data path (likely
# one big interleaved read) in the CreateCopy() instead of the line by 
# line access typical of ComputeBandStats.  Make sure we get the same as
# line by line.

def ecw_21():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    mem_ds = gdal.GetDriverByName('MEM').CreateCopy('xxxyyy',ds,options=['INTERLEAVE=PIXEL'])
    ds = None

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (141.172, 67.3636)
    else:
        if gdaltest.ecw_drv.major_version == 5:
            (exp_mean, exp_stddev) = (141.606, 67.2919)
        else:
            (exp_mean, exp_stddev) = (140.332, 67.611)

    (mean, stddev) = mem_ds.GetRasterBand(1).ComputeBandStats()

    if abs(mean-exp_mean) > 0.5 or abs(stddev-exp_stddev) > 0.5:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    return 'success'

###############################################################################
# This tests reading of georeferencing and coordinate system from within an
# ECW file.

def ecw_22():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/spif83.ecw' )

    expected_wkt = """PROJCS["L2CAL6M",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4269"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",32.78333330780953],PARAMETER["standard_parallel_2",33.88333332087654],PARAMETER["latitude_of_origin",32.16666668243202],PARAMETER["central_meridian",-116.2499999745946],PARAMETER["false_easting",2000000],PARAMETER["false_northing",500000],UNIT["Meter",1]]"""
    wkt = ds.GetProjectionRef()

    if wkt != expected_wkt:
        print(wkt)
        gdaltest.post_reason( 'did not get expected SRS.' )
        return 'fail'
    
    return 'success'

###############################################################################
# This tests overriding the coordinate system from an .aux.xml file, while
# preserving the ecw derived georeferencing.

def ecw_23():

    if gdaltest.ecw_drv is None:
        return 'skip'

    shutil.copyfile( 'data/spif83.ecw', 'tmp/spif83.ecw' )
    shutil.copyfile( 'data/spif83_hidden.ecw.aux.xml', 'tmp/spif83.ecw.aux.xml')

    ds = gdal.Open( 'tmp/spif83.ecw' )

    expected_wkt = """PROJCS["OSGB 1936 / British National Grid",GEOGCS["OSGB 1936",DATUM["OSGB_1936",SPHEROID["Airy 1830",6377563.396,299.3249646,AUTHORITY["EPSG","7001"]],TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489],AUTHORITY["EPSG","6277"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4277"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",49],PARAMETER["central_meridian",-2],PARAMETER["scale_factor",0.9996012717],PARAMETER["false_easting",400000],PARAMETER["false_northing",-100000],AUTHORITY["EPSG","27700"],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"""
    wkt = ds.GetProjectionRef()

    if wkt != expected_wkt:
        print(wkt)
        gdaltest.post_reason( 'did not get expected SRS.' )
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (6138559.5576418638, 195.5116973254697, 0.0, 2274798.7836679211, 0.0, -198.32414964918371)
    if gt != expected_gt:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform.' )
        return 'fail'

    try:
        os.remove( 'tmp/spif83.ecw' )
        os.remove( 'tmp/spif83.ecw.aux.xml' )
    except:
        pass
    
    return 'success'

###############################################################################
# Test that we can alter geotransform on existing ECW

def ecw_24():

    if gdaltest.ecw_drv is None:
        return 'skip'

    shutil.copyfile( 'data/spif83.ecw', 'tmp/spif83.ecw' )
    try:
        os.remove( 'tmp/spif83.ecw.aux.xml' )
    except:
        pass

    ds = gdal.Open( 'tmp/spif83.ecw', gdal.GA_Update )
    gt = [1,2,0,3,0,-4]
    ds.SetGeoTransform(gt)
    ds = None

    try:
        os.stat( 'tmp/spif83.ecw.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    ds = gdal.Open( 'tmp/spif83.ecw')
    got_gt = ds.GetGeoTransform()
    ds = None

    for i in range(6):
        if abs(gt[i] - got_gt[i]) > 1e-5:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'

    try:
        os.remove( 'tmp/spif83.ecw' )
    except:
        pass

    return 'success'

###############################################################################
# Test that we can alter projection info on existing ECW (through SetProjection())

def ecw_25():

    if gdaltest.ecw_drv is None:
        return 'skip'

    shutil.copyfile( 'data/spif83.ecw', 'tmp/spif83.ecw' )
    try:
        os.remove( 'tmp/spif83.ecw.aux.xml' )
    except:
        pass

    proj = 'NUTM31'
    datum = 'WGS84'
    units = 'FEET'

    ds = gdal.Open( 'tmp/spif83.ecw', gdal.GA_Update )
    sr = osr.SpatialReference()
    sr.ImportFromERM(proj, datum, units)
    wkt = sr.ExportToWkt()
    ds.SetProjection(wkt)
    ds = None

    try:
        os.stat( 'tmp/spif83.ecw.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    ds = gdal.Open( 'tmp/spif83.ecw')
    got_proj = ds.GetMetadataItem("PROJ", "ECW")
    got_datum = ds.GetMetadataItem("DATUM", "ECW")
    got_units = ds.GetMetadataItem("UNITS", "ECW")
    got_wkt = ds.GetProjectionRef()
    ds = None

    if got_proj != proj:
        gdaltest.post_reason('fail')
        print(proj)
        return 'fail'
    if got_datum != datum:
        gdaltest.post_reason('fail')
        print(datum)
        return 'fail'
    if got_units != units:
        gdaltest.post_reason('fail')
        print(units)
        return 'fail'

    if wkt != got_wkt:
        gdaltest.post_reason('fail')
        print(got_wkt)
        return 'fail'

    try:
        os.remove( 'tmp/spif83.ecw' )
    except:
        pass

    return 'success'

###############################################################################
# Test that we can alter projection info on existing ECW (through SetMetadataItem())

def ecw_26():

    if gdaltest.ecw_drv is None:
        return 'skip'

    shutil.copyfile( 'data/spif83.ecw', 'tmp/spif83.ecw' )
    try:
        os.remove( 'tmp/spif83.ecw.aux.xml' )
    except:
        pass

    proj = 'NUTM31'
    datum = 'WGS84'
    units = 'FEET'

    ds = gdal.Open( 'tmp/spif83.ecw', gdal.GA_Update )
    ds.SetMetadataItem("PROJ", proj, "ECW")
    ds.SetMetadataItem("DATUM", datum, "ECW")
    ds.SetMetadataItem("UNITS", units, "ECW")
    ds = None

    try:
        os.stat( 'tmp/spif83.ecw.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    ds = gdal.Open( 'tmp/spif83.ecw')
    got_proj = ds.GetMetadataItem("PROJ", "ECW")
    got_datum = ds.GetMetadataItem("DATUM", "ECW")
    got_units = ds.GetMetadataItem("UNITS", "ECW")
    got_wkt = ds.GetProjectionRef()
    ds = None

    if got_proj != proj:
        gdaltest.post_reason('fail')
        print(proj)
        return 'fail'
    if got_datum != datum:
        gdaltest.post_reason('fail')
        print(datum)
        return 'fail'
    if got_units != units:
        gdaltest.post_reason('fail')
        print(units)
        return 'fail'

    sr = osr.SpatialReference()
    sr.ImportFromERM(proj, datum, units)
    wkt = sr.ExportToWkt()

    if wkt != got_wkt:
        gdaltest.post_reason('fail')
        print(got_wkt)
        return 'fail'

    try:
        os.remove( 'tmp/spif83.ecw' )
    except:
        pass

    return 'success'

###############################################################################
# Check that we can use .j2w world files (#4651)

def ecw_27():

    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'data/byte_without_geotransform.jp2' )

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-440720) > 0.1 \
        or abs(geotransform[1]-60) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-3751320) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -60) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Check picking use case

def ecw_28():

    if gdaltest.ecw_drv is None:
        return 'skip'

    x = y = 50

    ds = gdal.Open( 'data/jrc.ecw' )
    multiband_data = ds.ReadRaster(x,y,1,1)
    ds = None

    ds = gdal.Open( 'data/jrc.ecw' )
    data1 = ds.GetRasterBand(1).ReadRaster(x,y,1,1)
    data2 = ds.GetRasterBand(2).ReadRaster(x,y,1,1)
    data3 = ds.GetRasterBand(3).ReadRaster(x,y,1,1)
    ds = None

    import struct
    tab1 = struct.unpack('B' * 3, multiband_data)
    tab2 = struct.unpack('B' * 3, data1 + data2 + data3)

    # Due to the nature of ECW, reading one band or several bands does not give
    # the same results.

    return 'success'

###############################################################################
# Test supersampling

def ecw_29():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    data_b1 = ds.GetRasterBand(1).ReadRaster(0,0,400,400)
    ds = None

    ds = gdal.Open( 'data/jrc.ecw' )
    data_ecw_supersampled_b1 = ds.GetRasterBand(1).ReadRaster(0,0,400,400,800,800)
    ds = None

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/ecw_29_0.tif', 400 ,400, 1)
    ds.WriteRaster(0, 0, 400, 400, data_b1)
    data_tiff_supersampled_b1 = ds.GetRasterBand(1).ReadRaster(0,0,400,400,800,800)
    ds = None

    ds1 = gdal.GetDriverByName('GTiff').Create('/vsimem/ecw_29_1.tif', 800 ,800, 1)
    ds1.WriteRaster(0, 0, 800, 800, data_ecw_supersampled_b1)

    ds2 = gdal.GetDriverByName('GTiff').Create('/vsimem/ecw_29_2.tif', 800 ,800, 1)
    ds2.WriteRaster(0, 0, 800, 800, data_tiff_supersampled_b1)

    ret = 'success'
    if gdaltest.ecw_drv.major_version < 5:
        maxdiff = gdaltest.compare_ds(ds1, ds2)
        if maxdiff != 0:
            print(maxdiff)
            ret = 'fail'
    else:
        # Compare the images by comparing their statistics on subwindows
        nvals = 0
        sum_abs_diff_mean = 0
        sum_abs_diff_stddev = 0
        tile = 32
        for j in range(2 * int((ds1.RasterYSize - tile/2) / tile)):
            for i in range(2 * int((ds1.RasterXSize - tile/2) / tile)):
                tmp_ds1 = gdal.GetDriverByName('MEM').Create('', tile ,tile, 1)
                tmp_ds2 = gdal.GetDriverByName('MEM').Create('', tile ,tile, 1)
                data1 = ds1.ReadRaster(i * (tile/2), j * (tile/2), tile, tile)
                data2 = ds2.ReadRaster(i * (tile/2), j * (tile/2), tile, tile)
                tmp_ds1.WriteRaster(0,0,tile,tile,data1)
                tmp_ds2.WriteRaster(0,0,tile,tile,data2)
                (ignored, ignored, mean1, stddev1) = tmp_ds1.GetRasterBand(1).GetStatistics(1,1)
                (ignored, ignored, mean2, stddev2) = tmp_ds2.GetRasterBand(1).GetStatistics(1,1)
                nvals = nvals + 1
                sum_abs_diff_mean = sum_abs_diff_mean + abs(mean1-mean2)
                sum_abs_diff_stddev = sum_abs_diff_stddev + abs(stddev1 - stddev2)
                if abs(mean1-mean2) > (stddev1 + stddev2) / 2 or abs(stddev1 - stddev2) > 30:
                    print("%d, %d, %f, %f" % (j, i ,abs(mean1-mean2), abs(stddev1 - stddev2)))
                    ret = 'fail'

        if sum_abs_diff_mean / nvals > 4 or sum_abs_diff_stddev / nvals > 3:
            print(sum_abs_diff_mean / nvals)
            print(sum_abs_diff_stddev / nvals)
            ret = 'fail'

    ds1 = None
    ds2 = None

    gdal.Unlink('/vsimem/ecw_29_0.tif')
    gdal.Unlink('/vsimem/ecw_29_1.tif')
    gdal.Unlink('/vsimem/ecw_29_2.tif')

    return ret

###############################################################################
# Test IReadBlock()

def ecw_30():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    (blockxsize, blockysize) = ds.GetRasterBand(1).GetBlockSize()
    data_readraster = ds.GetRasterBand(1).ReadRaster(0,0,blockxsize,blockysize)
    data_readblock = ds.GetRasterBand(1).ReadBlock(0,0)
    ds = None

    if data_readraster != data_readraster:
        return 'fail'

    return 'success'

###############################################################################
# Test async reader interface ( SDK >= 4.x )

def ecw_31():

    if gdaltest.ecw_drv is None:
        return 'skip'

    if gdaltest.ecw_drv.major_version < 4:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    ref_buf = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    ds = None

    ds = gdal.Open( 'data/jrc.ecw' )

    asyncreader = ds.BeginAsyncReader(0,0,ds.RasterXSize,ds.RasterYSize)
    while True:
        result = asyncreader.GetNextUpdatedRegion(0.05)
        if result[0] == gdal.GARIO_COMPLETE:
            break
        elif result[0] != gdal.GARIO_ERROR:
            continue
        else:
            gdaltest.post_reason('error occured')
            ds.EndAsyncReader(asyncreader)
            return 'fail'

    if result != [gdal.GARIO_COMPLETE, 0, 0, ds.RasterXSize,ds.RasterYSize]:
        gdaltest.post_reason('wrong return values for GetNextUpdatedRegion()')
        print(result)
        ds.EndAsyncReader(asyncreader)
        return 'fail'

    async_buf = asyncreader.GetBuffer()

    ds.EndAsyncReader(asyncreader)
    asyncreader = None
    ds = None

    if async_buf != ref_buf:
        gdaltest.post_reason('async_buf != ref_buf')
        return 'fail'

    return 'success'

###############################################################################
# ECW SDK 3.3 has a bug with the ECW format when we query the
# number of bands of the dataset, but not in the "natural order".
# It ignores the content of panBandMap. (#4234)

def ecw_32():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open('data/jrc.ecw')
    data_123 = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize, band_list = [1,2,3])
    data_321 = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize, band_list = [3,2,1])
    if data_123 == data_321:
        gdaltest.post_reason('failure')
        return 'fail'

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="400" rasterYSize="400">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/jrc.ecw</SourceFilename>
        <SourceBand>3</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2">
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/jrc.ecw</SourceFilename>
        <SourceBand>2</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3">
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/jrc.ecw</SourceFilename>
        <SourceBand>1</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""")
    data_vrt = vrt_ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize, band_list = [1,2,3])

    if data_321 != data_vrt:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test heuristics that detect successive band reading pattern

def ecw_33():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    multiband_data = ds.ReadRaster(100,100,50,50)
    ds = None

    ds = gdal.Open( 'data/jrc.ecw' )

    # To feed the heuristics
    ds.GetRasterBand(1).ReadRaster(10,10,50,50)
    ds.GetRasterBand(2).ReadRaster(10,10,50,50)
    ds.GetRasterBand(3).ReadRaster(10,10,50,50)

    # Now the heuristics should be set to ON
    data1_1 = ds.GetRasterBand(1).ReadRaster(100,100,50,50)
    data2_1 = ds.GetRasterBand(2).ReadRaster(100,100,50,50)
    data3_1 = ds.GetRasterBand(3).ReadRaster(100,100,50,50)

    # Break heuristics
    ds.GetRasterBand(2).ReadRaster(100,100,50,50)
    ds.GetRasterBand(1).ReadRaster(100,100,50,50)

    # To feed the heuristics again
    ds.GetRasterBand(1).ReadRaster(10,10,50,50)
    ds.GetRasterBand(2).ReadRaster(10,10,50,50)
    ds.GetRasterBand(3).ReadRaster(10,10,50,50)

    # Now the heuristics should be set to ON
    data1_2 = ds.GetRasterBand(1).ReadRaster(100,100,50,50)
    data2_2 = ds.GetRasterBand(2).ReadRaster(100,100,50,50)
    data3_2 = ds.GetRasterBand(3).ReadRaster(100,100,50,50)

    ds = None

    if data1_1 != data1_2 or data2_1 != data2_2 or data3_1 != data3_2:
        gdaltest.post_reason('fail')
        return 'fail'

    # When heuristics is ON, returned values should be the same as
    # 3-band at a time reading
    import struct
    tab1 = struct.unpack('B' * 3 * 50 * 50, multiband_data)
    tab2 = struct.unpack('B' * 3 * 50 * 50, data1_1 + data2_1 + data3_2)
    if tab1 != tab2 :
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Check bugfix for #5262

def ecw_33_bis():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    data_ref = ds.ReadRaster(0,0,50,50)

    ds = gdal.Open( 'data/jrc.ecw' )

    # To feed the heuristics
    ds.GetRasterBand(1).ReadRaster(0,0,50,50,buf_pixel_space=4)
    ds.GetRasterBand(2).ReadRaster(0,0,50,50,buf_pixel_space=4)
    ds.GetRasterBand(3).ReadRaster(0,0,50,50,buf_pixel_space=4)

    # Now the heuristics should be set to ON
    data1 = ds.GetRasterBand(1).ReadRaster(0,0,50,50,buf_pixel_space=4)
    data2 = ds.GetRasterBand(2).ReadRaster(0,0,50,50,buf_pixel_space=4)
    data3 = ds.GetRasterBand(3).ReadRaster(0,0,50,50,buf_pixel_space=4)

    # Note: we must compare with the dataset RasterIO() buffer since
    # with SDK 3.3, the results of band RasterIO() and dataset RasterIO() are
    # not consistant. (which seems to be no longer the case with more recent
    # SDK such as 5.0)
    for i in range(50*50):
        if data1[i*4] != data_ref[i]: 
            gdaltest.post_reason('fail')
            return 'fail'
        if data2[i*4] != data_ref[50*50+i]: 
            gdaltest.post_reason('fail')
            return 'fail'
        if data3[i*4] != data_ref[2*50*50+i]: 
            gdaltest.post_reason('fail')
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Verify that an write the imagery out to a new ecw file. Source file is 16 bit.

def ecw_34():
    
    if gdaltest.ecw_drv is None or gdaltest.ecw_write == 0 :
        return 'skip'
    if gdaltest.ecw_drv.major_version <5:
        return 'skip'

    ds = gdal.GetDriverByName('MEM').Create('MEM:::', 128, 128, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(65535)
    ref_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type = gdal.GDT_UInt16)
    out_ds = gdaltest.ecw_drv.CreateCopy( 'tmp/UInt16_big_out.ecw', ds, options = ['ECW_FORMAT_VERSION=3','TARGET=1'] )
    out_ds = None
    ds = None

    ds = gdal.Open( 'tmp/UInt16_big_out.ecw' )
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type = gdal.GDT_UInt16)
    version = ds.GetMetadataItem('VERSION')
    ds = None

    if got_data != ref_data:
        return 'fail'
    if version != '3':
        gdaltest.post_reason('bad VERSION')
        print(version)
        return 'fail'

    return 'success'

###############################################################################
# Verify that an write the imagery out to a new JP2 file. Source file is 16 bit.

def ecw_35():
    if gdaltest.jp2ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.GetDriverByName('MEM').Create('MEM:::', 128, 128, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(65535)
    ref_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type = gdal.GDT_UInt16)
    out_ds = gdaltest.jp2ecw_drv.CreateCopy( 'tmp/UInt16_big_out.jp2', ds, options = ['TARGET=1'] )
    out_ds = None
    ds = None

    ds = gdal.Open( 'tmp/UInt16_big_out.jp2' )
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type = gdal.GDT_UInt16)
    ds = None

    if got_data != ref_data:
        return 'fail'
    return 'success'

###############################################################################
# Make sure that band descriptions are preserved for version 3 ECW files. 
def ecw_36():

    if gdaltest.ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'
    if gdaltest.ecw_drv.major_version <5:
        return 'skip'    

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="400" rasterYSize="400">
    <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Blue</ColorInterp>
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/jrc.ecw</SourceFilename>
        <SourceBand>3</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2">
        <ColorInterp>Red</ColorInterp>
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/jrc.ecw</SourceFilename>
        <SourceBand>1</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3">
        <ColorInterp>Green</ColorInterp>
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/jrc.ecw</SourceFilename>
        <SourceBand>2</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""")
    
    dswr = gdaltest.ecw_drv.CreateCopy( 'tmp/jrc312.ecw', vrt_ds, options = ['ECW_FORMAT_VERSION=3','TARGET=75'] )

    if dswr.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_BlueBand : 
        print ('Band 1 color interpretation should be Blue  but is : '+ gdal.GetColorInterpretationName(dswr.GetRasterBand(1).GetColorInterpretation()))
        return 'fail'
    if dswr.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_RedBand: 
        print ('Band 2 color interpretation should be Red but is : '+ gdal.GetColorInterpretationName(dswr.GetRasterBand(2).GetColorInterpretation()))
        return 'fail'
    if dswr.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_GreenBand: 
        print ('Band 3 color interpretation should be Green but is : '+ gdal.GetColorInterpretationName(dswr.GetRasterBand(3).GetColorInterpretation()))
        return 'fail'

    dswr = None

    dsr = gdal.Open( 'tmp/jrc312.ecw' )

    if dsr.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_BlueBand : 
        print ('Band 1 color interpretation should be Blue  but is : '+ gdal.GetColorInterpretationName(dsr.GetRasterBand(1).GetColorInterpretation()))
        return 'fail'
    if dsr.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_RedBand: 
        print ('Band 2 color interpretation should be Red but is : '+ gdal.GetColorInterpretationName(dsr.GetRasterBand(2).GetColorInterpretation()))
        return 'fail'
    if dsr.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_GreenBand: 
        print ('Band 3 color interpretation should be Green but is : '+ gdal.GetColorInterpretationName(dsr.GetRasterBand(3).GetColorInterpretation()))
        return 'fail'

    dsr = None 

    return 'success'
###############################################################################
# Make sure that band descriptions are preserved for version 2 ECW files when 
# color space set implicitly to sRGB.
 
def ecw_37():

    if gdaltest.ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'
    if gdaltest.ecw_drv.major_version <5:
        return 'skip'    

    ds = gdal.Open("data/jrc.ecw")    

    dswr = gdaltest.ecw_drv.CreateCopy( 'tmp/jrc123.ecw', ds, options = ['ECW_FORMAT_VERSION=3','TARGET=75'] )

    if dswr.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand : 
        print ('Band 1 color interpretation should be Red but is : '+ gdal.GetColorInterpretationName(dswr.GetRasterBand(1).GetColorInterpretation()))
        return 'fail'
    if dswr.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand: 
        print ('Band 2 color interpretation should be Green but is : '+ gdal.GetColorInterpretationName(dswr.GetRasterBand(2).GetColorInterpretation()))
        return 'fail'
    if dswr.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand: 
        print ('Band 3 color interpretation should be Blue but is : '+ gdal.GetColorInterpretationName(dswr.GetRasterBand(3).GetColorInterpretation()))
        return 'fail'

    dswr = None

    dsr = gdal.Open( 'tmp/jrc123.ecw' )

    if dsr.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand : 
        print ('Band 1 color interpretation should be Red  but is : '+ gdal.GetColorInterpretationName(dsr.GetRasterBand(1).GetColorInterpretation()))
        return 'fail'
    if dsr.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand: 
        print ('Band 2 color interpretation should be Green but is : '+ gdal.GetColorInterpretationName(dsr.GetRasterBand(2).GetColorInterpretation()))
        return 'fail'
    if dsr.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand: 
        print ('Band 3 color interpretation should be Blue but is : '+ gdal.GetColorInterpretationName(dsr.GetRasterBand(3).GetColorInterpretation()))
        return 'fail'

    dsr = None 

    return 'success'

###############################################################################
# Check openning unicode files

def ecw_38():

    if gdaltest.ecw_drv is None:
        return 'skip'

    gdaltest.ecw_38_fname = ''
    if version_info >= (3,0,0):
        exec("""gdaltest.ecw_38_fname = 'tmp/za\u017C\u00F3\u0142\u0107g\u0119\u015Bl\u0105ja\u017A\u0144.ecw'""")
    else:
        exec("""gdaltest.ecw_38_fname = u'tmp/za\u017C\u00F3\u0142\u0107g\u0119\u015Bl\u0105ja\u017A\u0144.ecw'""")
    fname = gdaltest.ecw_38_fname

    if gdaltest.ecw_drv.major_version <4:
        return 'skip'

    shutil.copyfile( 'data/jrc.ecw', fname )
    
    ds = gdal.Open( 'data/jrc.ecw' )
 
    ds_ref = gdal.Open( fname )
    
    maxdiff = gdaltest.compare_ds(ds, ds_ref)

    ds = None
    ds_ref = None
    
    # Quite a bit of difference...
    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'


###############################################################################
# Check writing histograms. 

def ecw_39():

    if gdaltest.ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'
    if gdaltest.ecw_drv.major_version <5:
        return 'skip'    

    ds = gdal.Open( 'data/jrc.ecw' )
  
    dswr = gdaltest.ecw_drv.CreateCopy( 'tmp/jrcstats.ecw', ds, options = ['ECW_FORMAT_VERSION=3','TARGET=75'] )
    ds = None;
    hist = (0, 255, 2, [3, 4])

    dswr.GetRasterBand(1).SetDefaultHistogram( 0, 255, [3, 4] )
    dswr = None
    
    ds = gdal.Open( 'tmp/jrcstats.ecw');
    
    result = (hist == ds.GetRasterBand(1).GetDefaultHistogram(force=0))

    ds = None    
    if not result:
        gdaltest.post_reason('Default histogram written incorrectly')
        return 'fail'

    return 'success'

###############################################################################
# Check reading a ECW v3 file

def ecw_40():

    if gdaltest.ecw_drv is None:
        return 'skip'

    ds = gdal.Open('data/stefan_full_rgba_ecwv3_meta.ecw')
    if ds is None:
        if gdaltest.ecw_drv.major_version < 5:
            if gdal.GetLastErrorMsg().find('requires ECW SDK 5.0') >= 0:
                return 'skip'
            else:
                gdaltest.post_reason('explicit error message expected')
                return 'fail'
        else:
            gdaltest.post_reason('fail')
            return 'fail'

    expected_md = [
  ('CLOCKWISE_ROTATION_DEG','0.000000'),
  ('COLORSPACE','RGB'),
  ('COMPRESSION_DATE','2013-04-04T09:20:03Z'),
  ('COMPRESSION_RATE_ACTUAL','3.165093'),
  ('COMPRESSION_RATE_TARGET','20'),
  ('FILE_METADATA_COMPRESSION_SOFTWARE','python2.7/GDAL v1.10.0.0/ECWJP2 SDK v5.0.0.0'),
  ('FILE_METADATA_ACQUISITION_DATE','2012-09-12'),
  ('FILE_METADATA_ACQUISITION_SENSOR_NAME','Leica ADS-80'),
  ('FILE_METADATA_ADDRESS','2 Abbotsford Street, West Leederville WA 6007 Australia'),
  ('FILE_METADATA_AUTHOR','Unknown'),
  ('FILE_METADATA_CLASSIFICATION','test gdal image'),
  ('FILE_METADATA_COMPANY','ERDAS-QA'),
  ('FILE_METADATA_COMPRESSION_SOFTWARE','python2.7/GDAL v1.10.0.0/ECWJP2 SDK v5.0.0.0'),
  ('FILE_METADATA_COPYRIGHT','Intergraph 2013'),
  ('FILE_METADATA_EMAIL','support@intergraph.com'),
  ('FILE_METADATA_TELEPHONE','+61 8 9388 2900'),
  ('VERSION','3') ]

    got_md = ds.GetMetadata()
    for (key,value) in expected_md:
        if key not in got_md or got_md[key] != value:
            gdaltest.post_reason('fail')
            print(key)
            print(got_md[key])
            return 'fail'

    expected_cs_list = [ 28760, 59071, 54087, 22499 ]
    for i in range(4):
        got_cs = ds.GetRasterBand(i + 1).Checksum()
        if got_cs != expected_cs_list[i]:
            gdaltest.post_reason('fail')
            print(expected_cs_list[i])
            print(got_cs)
            return 'fail'

    return 'success'

###############################################################################
# Check generating statistics & histogram for a ECW v3 file

def ecw_41():

    if gdaltest.ecw_drv is None or gdaltest.ecw_drv.major_version < 5:
        return 'skip'

    shutil.copy('data/stefan_full_rgba_ecwv3_meta.ecw', 'tmp/stefan_full_rgba_ecwv3_meta.ecw')
    try:
        os.remove('tmp/stefan_full_rgba_ecwv3_meta.ecw.aux.xml')
    except:
        pass

    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw')

    # Check that no statistics is already included in the file
    if ds.GetRasterBand(1).GetMinimum() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMaximum() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetStatistics(1,0) != [0.0, 0.0, 0.0, -1.0]:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetDefaultHistogram(force = 0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Now compute the stats
    stats = ds.GetRasterBand(1).GetStatistics(0,1)
    expected_stats = [0.0, 255.0, 21.662427983539093, 51.789457392268119]
    for i in range(4):
        if abs(stats[i]-expected_stats[i]) > 1:
            gdaltest.post_reason('fail')
            print(stats)
            print(expected_stats)
            return 'fail'

    ds = None

    # Check that there's no .aux.xml file
    try:
        os.stat('tmp/stefan_full_rgba_ecwv3_meta.ecw.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw')
    if ds.GetRasterBand(1).GetMinimum() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMinimum())
        return 'fail'
    if ds.GetRasterBand(1).GetMaximum() != 255:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMaximum())
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics(0,0)
    expected_stats = [0.0, 255.0, 21.662427983539093, 51.789457392268119]
    for i in range(4):
        if abs(stats[i]-expected_stats[i]) > 1:
            gdaltest.post_reason('fail')
            print(stats[i])
            print(expected_stats[i])
            return 'fail'
        pass
    ds = None

    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw')
    # And compute the histogram
    got_hist = ds.GetRasterBand(1).GetDefaultHistogram()
    expected_hist = (-0.5, 255.5, 256, [1006, 16106, 548, 99, 13, 24, 62, 118, 58, 125, 162, 180, 133, 146, 70, 81, 84, 97, 90, 60, 79, 70, 85, 77, 73, 63, 60, 64, 56, 69, 63, 73, 70, 72, 61, 66, 40, 52, 65, 44, 62, 54, 56, 55, 63, 51, 47, 39, 58, 44, 36, 43, 47, 45, 54, 28, 40, 41, 37, 36, 33, 31, 28, 34, 19, 32, 19, 23, 23, 33, 16, 34, 32, 54, 29, 33, 40, 37, 27, 34, 24, 29, 26, 21, 22, 24, 25, 19, 29, 22, 24, 14, 20, 20, 29, 28, 13, 19, 21, 19, 19, 21, 13, 19, 13, 14, 22, 15, 13, 26, 10, 13, 13, 14, 10, 17, 15, 19, 11, 18, 11, 14, 8, 12, 20, 12, 17, 10, 15, 15, 16, 14, 11, 7, 7, 10, 8, 12, 7, 8, 14, 7, 9, 12, 4, 6, 12, 5, 5, 4, 11, 8, 4, 8, 7, 10, 11, 6, 7, 5, 6, 8, 10, 10, 7, 5, 3, 5, 5, 6, 4, 10, 7, 6, 8, 4, 6, 6, 4, 6, 6, 7, 10, 4, 5, 2, 5, 6, 1, 1, 2, 6, 2, 1, 7, 4, 1, 3, 3, 2, 6, 2, 3, 3, 3, 3, 5, 5, 4, 2, 3, 2, 1, 3, 5, 5, 4, 1, 1, 2, 5, 10, 5, 9, 3, 5, 3, 5, 4, 5, 4, 4, 6, 7, 9, 17, 13, 15, 14, 13, 20, 18, 16, 27, 35, 53, 60, 51, 46, 40, 38, 50, 66, 36, 45, 13])
    if got_hist != expected_hist:
        gdaltest.post_reason('fail')
        print(got_hist)
        print(expected_hist)
        return 'fail'
    ds = None

    # Remove the .aux.xml file
    try:
        os.remove('tmp/stefan_full_rgba_ecwv3_meta.ecw.aux.xml')
    except:
        pass

    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw')
    if ds.GetRasterBand(1).GetMinimum() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMinimum())
        return 'fail'
    if ds.GetRasterBand(1).GetMaximum() != 255:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMaximum())
        return 'fail'
    got_hist = ds.GetRasterBand(1).GetDefaultHistogram(force = 0)
    if got_hist != expected_hist:
        gdaltest.post_reason('fail')
        print(got_hist)
        print(expected_hist)
        return 'fail'
    ds = None

    # Check that there's no .aux.xml file
    try:
        os.stat('tmp/stefan_full_rgba_ecwv3_meta.ecw.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test setting/unsetting file metadata of a ECW v3 file

def ecw_42():

    if gdaltest.ecw_drv is None or gdaltest.ecw_drv.major_version < 5:
        return 'skip'

    shutil.copy('data/stefan_full_rgba_ecwv3_meta.ecw', 'tmp/stefan_full_rgba_ecwv3_meta.ecw')
    try:
        os.remove('tmp/stefan_full_rgba_ecwv3_meta.ecw.aux.xml')
    except:
        pass

    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw', gdal.GA_Update)
    md = {}
    md['FILE_METADATA_CLASSIFICATION'] = 'FILE_METADATA_CLASSIFICATION'
    md['FILE_METADATA_ACQUISITION_DATE'] = '2013-04-04'
    md['FILE_METADATA_ACQUISITION_SENSOR_NAME'] = 'FILE_METADATA_ACQUISITION_SENSOR_NAME'
    md['FILE_METADATA_COMPRESSION_SOFTWARE'] = 'FILE_METADATA_COMPRESSION_SOFTWARE'
    md['FILE_METADATA_AUTHOR'] = 'FILE_METADATA_AUTHOR'
    md['FILE_METADATA_COPYRIGHT'] = 'FILE_METADATA_COPYRIGHT'
    md['FILE_METADATA_COMPANY'] = 'FILE_METADATA_COMPANY'
    md['FILE_METADATA_EMAIL'] = 'FILE_METADATA_EMAIL'
    md['FILE_METADATA_ADDRESS'] = 'FILE_METADATA_ADDRESS'
    md['FILE_METADATA_TELEPHONE'] = 'FILE_METADATA_TELEPHONE'
    ds.SetMetadata(md)
    ds = None

    # Check that there's no .aux.xml file
    try:
        os.stat('tmp/stefan_full_rgba_ecwv3_meta.ecw.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    # Check item values
    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw')
    got_md = ds.GetMetadata()
    for item in md:
        if got_md[item] != md[item]:
            gdaltest.post_reason('fail')
            print(got_md[item])
            print(md[item])
            return 'fail'
    ds = None

    # Test unsetting all the stuff
    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw', gdal.GA_Update)
    md = {}
    md['FILE_METADATA_CLASSIFICATION'] = ''
    md['FILE_METADATA_ACQUISITION_DATE'] = '1970-01-01'
    md['FILE_METADATA_ACQUISITION_SENSOR_NAME'] = ''
    md['FILE_METADATA_COMPRESSION_SOFTWARE'] = ''
    md['FILE_METADATA_AUTHOR'] = ''
    md['FILE_METADATA_COPYRIGHT'] = ''
    md['FILE_METADATA_COMPANY'] = ''
    md['FILE_METADATA_EMAIL'] = ''
    md['FILE_METADATA_ADDRESS'] = ''
    md['FILE_METADATA_TELEPHONE'] = ''
    ds.SetMetadata(md)
    ds = None

    # Check that there's no .aux.xml file
    try:
        os.stat('tmp/stefan_full_rgba_ecwv3_meta.ecw.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    # Check item values
    ds = gdal.Open('tmp/stefan_full_rgba_ecwv3_meta.ecw')
    got_md = ds.GetMetadata()
    for item in md:
        if item in got_md and item != 'FILE_METADATA_ACQUISITION_DATE':
            gdaltest.post_reason('fail')
            print(got_md[item])
            print(md[item])
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit

def ecw_43():

    if gdaltest.jp2ecw_drv is None:
        return 'skip'

    ds = gdal.Open('data/stefan_full_rgba_alpha_1bit.jp2')
    fourth_band = ds.GetRasterBand(4)
    if fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is not None:
        return 'fail'
    got_cs = fourth_band.Checksum()
    if got_cs != 8527:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    jp2_bands_data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    jp2_fourth_band_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    jp2_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)

    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/ecw_43.tif', ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    #gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/ecw_43.tif')
    if got_cs != 8527:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'

    if jp2_bands_data != gtiff_bands_data:
        gdaltest.post_reason('fail')
        return 'fail'

    if jp2_fourth_band_data != gtiff_fourth_band_data:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
def ecw_online_1():
    if gdaltest.jp2ecw_drv is None:
        return 'skip'
    
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        return 'skip'

    # checksum = 32316 on my PC
    tst = gdaltest.GDALTest( 'JP2ECW', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

    return 'success'

###############################################################################
def ecw_online_2():
    if gdaltest.jp2ecw_drv is None:
        return 'skip'
    
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        return 'skip'

    # checksum = 1292 on my PC
    tst = gdaltest.GDALTest( 'JP2ECW', 'tmp/cache/gcp.jp2', 1, None, filename_absolute = 1 )

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
def ecw_online_3():
    if gdaltest.jp2ecw_drv is None:
        return 'skip'
    if gdaltest.ecw_drv.major_version ==4:
        gdaltest.post_reason( '4.x SDK gets unreliable results for jp2')
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        return 'skip'

    # checksum = 16481 on my PC
    tst = gdaltest.GDALTest( 'JP2ECW', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute = 1 )

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
    if maxdiff > 16:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
def ecw_online_4():

    if gdaltest.jp2ecw_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        return 'skip'

    # Checksum = 53054 on my PC
    tst = gdaltest.GDALTest( 'JP2ECW', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/Bretagne2.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne2.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, width = 256, height = 256)
#    print(ds.GetRasterBand(1).Checksum())
#    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
def ecw_online_5():

    if gdaltest.ecw_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/ecw/red_flower.ecw', 'red_flower.ecw'):
        return 'skip'

    ds = gdal.Open('tmp/cache/red_flower.ecw')

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (112.801,52.0431)
        # on Tamas slavebots, (mean,stddev)  = (113.301,52.0434)
        mean_tolerance = 1
    else:
        mean_tolerance = 0.5
        if gdaltest.ecw_drv.major_version == 5: 
            (exp_mean, exp_stddev) = (113.345,52.1259)			
        else: 
            (exp_mean, exp_stddev) = (114.337,52.1751)

    (mean, stddev) = ds.GetRasterBand(2).ComputeBandStats()

    if abs(mean-exp_mean) > mean_tolerance or abs(stddev-exp_stddev) > 0.5:
        gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
        return 'fail'

    return 'success'

###############################################################################
# This tests the HTTP driver in fact. To ensure if keeps the original filename,
# and in particular the .ecw extension, to make the ECW driver happy

def ecw_online_6():

    if gdaltest.ecw_drv is None:
        return 'skip'

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    try:
        dods_drv = gdal.GetDriverByName( 'DODS' )
        if dods_drv is not None:
            dods_drv.Deregister()
    except:
        dods_drv = None

    ds = gdal.Open('http://download.osgeo.org/gdal/data/ecw/spif83.ecw')

    if dods_drv is not None:
        dods_drv.Register()

    if ds is None:
        # The ECW driver doesn't manage to open in /vsimem, thus fallbacks
        # to writing to /tmp, which doesn't work on Windows
        if sys.platform == 'win32':    
            return 'skip'
        return 'fail'
    ds = None

    return 'success'

###############################################################################
def ecw_cleanup():

    #gdaltest.clean_tmp()

    try:
        os.remove( 'tmp/jrc_out.ecw' )
    except:
        pass
    try:
        os.remove( 'tmp/jrc_out.ecw.aux.xml' )
    except:
        pass
    try:
        os.remove( 'tmp/ecw_5.jp2' )
    except:
        pass
    try:
        os.remove( 'tmp/ecw_5.jp2.aux.xml' )
    except:
        pass
    try:
        os.remove( 'tmp/ecw_7.ntf' )
    except:
        pass
    try:
        os.remove( 'tmp/ecw9.jp2' )
    except:
        pass
    try:
        os.remove( 'tmp/test_11.ntf' )
    except:
        pass
    try:
        os.remove( 'tmp/rgb_gcp.jp2' )
    except:
        pass
    try:
        os.remove( 'tmp/spif83.ecw' )
    except:
        pass
    try:
        os.remove( 'tmp/spif83.ecw.aux.xml' )
    except:
        pass
    try:
        os.remove( 'tmp/UInt16_big_out.ecw' )
    except:
        pass
    try:
        os.remove( 'tmp/UInt16_big_out.jp2' )
    except:
        pass
    try:
        os.remove( 'tmp/UInt16_big_out.jp2.aux.xml' )
    except:
        pass
    try:
        os.remove( 'tmp/UInt16_big_out.ecw.aux.xml' )
    except:
        pass
    try:
        os.remove( 'tmp/jrc312.ecw' )
    except:
        pass
    try:
        os.remove( 'tmp/jrc123.ecw' )
    except:
        pass
    try:
        os.remove( 'tmp/jrcstats.ecw' )
    except:
        pass

    try:
        fname = gdaltest.ecw_38_fname
        os.remove( fname )
    except:
        pass
    try:
        fname = gdaltest.ecw_38_fname
        os.remove( fname + '.aux.xml' )
    except:
        pass
    try:
        os.remove( 'tmp/stefan_full_rgba_ecwv3_meta.ecw' )
    except:
        pass
    gdaltest.reregister_all_jpeg2000_drivers()
    
    return 'success'

gdaltest_list = [
    ecw_init,
    ecw_1,
    ecw_2,
    ecw_3,
    ecw_4,
    ecw_5,
    ecw_6,
    ecw_7,
    ecw_8,
    ecw_9,
    ecw_10,
    ecw_11,
    ecw_12,
    ecw_13,
    ecw_14,
    ecw_15,
    ecw_16,
    ecw_17,
    ecw_18,
    ecw_19,
    ecw_20,
    ecw_21,
    ecw_22,
    ecw_23,
    ecw_24,
    ecw_25,
    ecw_26,
    ecw_27,
    ecw_28,
    ecw_29,
    ecw_30,
    ecw_31,
    ecw_32,
    ecw_33,
    ecw_33_bis,
    ecw_34,
    ecw_35,
    ecw_36,
    ecw_37,
    ecw_38,
    ecw_39,
    ecw_40,
    ecw_41,
    ecw_42,
    ecw_43,
    ecw_online_1,
    ecw_online_2,
    #JTO this test does not make sense. It tests difference between two files pixel by pixel but compression is lossy# ecw_online_3, 
    ecw_online_4,
    ecw_online_5,
    ecw_online_6,
    ecw_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ecw' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

