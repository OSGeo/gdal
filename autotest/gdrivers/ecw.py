#!/usr/bin/env python
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
import sys
import string
import array
import shutil
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

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

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2ECW')

    if gdaltest.ecw_drv is not None:
        if gdaltest.ecw_drv.GetMetadataItem('DMD_CREATIONDATATYPES') != None:
            gdaltest.ecw_write = 1

        longname = gdaltest.ecw_drv.GetMetadataItem('DMD_LONGNAME')

        sdk_off = longname.find('SDK ')
        if sdk_off != -1:
            gdaltest.ecw_drv.major_version = int(longname[sdk_off+4])
        else:
            gdaltest.ecw_drv.major_version = 3

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
    gdaltest.ecw_drv.CreateCopy( 'tmp/jrc_out.ecw', ds, options = ['TARGET=75'] )
    ds = None

    return 'success' 

###############################################################################
# Verify various information about our generated image. 

def ecw_4():

    if gdaltest.ecw_drv is None or gdaltest.ecw_write == 0:
        return 'skip'

    ds = gdal.Open( 'tmp/jrc_out.ecw' )

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (140.290, 66.6303)
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
        (exp_mean, exp_stddev) = (143.375, 44.853)
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

    if gdaltest.ecw_drv.major_version > 3:
        gdaltest.post_reason( '4.x SDK gets unreliable results for jp2')
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

    if gdaltest.ecw_drv.major_version == 3:    
        (exp_mean, exp_stddev) = (141.644, 67.2186)
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

    if gdaltest.ecw_drv.major_version > 3:
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
        (exp_mean, exp_stddev) = (114.337,52.1751)
        mean_tolerance = 0.5

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

    gdaltest.reregister_all_jpeg2000_drivers()
    
    return 'success'

gdaltest_list = [
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
    ecw_online_1,
    ecw_online_2,
    ecw_online_3,
    ecw_online_4,
    ecw_online_5,
    ecw_online_6,
    ecw_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ecw' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

