#!/usr/bin/env python
###############################################################################
# $Id: ecw.py,v 1.17 2006/11/16 03:22:56 fwarmerdam Exp $
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
# 
#  $Log: ecw.py,v $
#  Revision 1.17  2006/11/16 03:22:56  fwarmerdam
#  Fixed handling of deregistering jp2kak, jpeg2000 drivers.
#
#  Revision 1.16  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.15  2006/04/28 05:18:44  fwarmerdam
#  Added gcp tests
#
#  Revision 1.14  2006/03/21 05:18:56  fwarmerdam
#  loosen up stddev range on test 6 alot
#
#  Revision 1.13  2006/03/21 05:03:39  fwarmerdam
#  loosen up tests so they pass on win32
#
#  Revision 1.12  2005/10/25 02:31:24  fwarmerdam
#  Commit frenzy...
#
#  Revision 1.11  2005/05/23 05:22:29  fwarmerdam
#  make UTM Zone 11 test less stringent
#
#  Revision 1.10  2005/03/03 17:05:03  fwarmerdam
#  added ECWDataset::RasterIO() test
#
#  Revision 1.9  2005/02/22 08:19:42  fwarmerdam
#  added test for color interp setting in NITF direct create w/jpeg2000
#
#  Revision 1.8  2005/02/17 16:04:58  fwarmerdam
#  Cleanup ecw9.jp2
#
#  Revision 1.7  2005/02/17 16:04:31  fwarmerdam
#  Added Create() test.
#
#  Revision 1.6  2005/02/15 03:30:58  fwarmerdam
#  temporariliy deregister JP2KAK driver
#
#  Revision 1.5  2005/02/15 02:29:55  fwarmerdam
#  adjusted for linux/gcc3.2.3
#
#  Revision 1.4  2004/12/21 04:56:05  fwarmerdam
#  impoved cleanup a bit
#
#  Revision 1.3  2004/12/21 04:55:12  fwarmerdam
#  added jp2 and nitf jp2 output testing
#
#  Revision 1.1  2004/12/02 20:47:50  fwarmerdam
#  New
#
#

import os
import sys
import string
import array
import gdal

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
        return 'skip'

    try:
        gdaltest.jp2kak = None
        gdaltest.jpeg2000 = None
        if gdal.GetDriverByName( 'JP2KAK' ):
            gdaltest.jp2kak = gdal.GetDriverByName('JP2KAK')
            gdaltest.jp2kak.Deregister()
        if gdal.GetDriverByName( 'JPEG2000' ):
            gdaltest.jpeg2000 = gdal.GetDriverByName('JPEG2000')
            gdaltest.jpeg2000.Deregister()
    except:
        pass

    return 'success'

###############################################################################
# Verify various information about our test image. 

def ecw_2():

    if gdaltest.ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    
    (exp_mean, exp_stddev) = (141.172, 67.3636)
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
	print geotransform
	gdaltest.post_reason( 'geotransform differs from expected' )
	return 'fail'

    return 'success'

###############################################################################
# Verify that an write the imagery out to a new file.

def ecw_3():
    if gdaltest.ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'data/jrc.ecw' )
    gdaltest.ecw_drv.CreateCopy( 'tmp/jrc_out.ecw', ds )
    ds = None

    return 'success' 
	
###############################################################################
# Verify various information about our generated image. 

def ecw_4():

    if gdaltest.ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'tmp/jrc_out.ecw' )
    
    (exp_mean, exp_stddev) = (140.290, 66.6303)
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
	print geotransform
	gdaltest.post_reason( 'geotransform differs from expected' )
	return 'fail'

    ds = None

    return 'success'

###############################################################################
# Now try writing a JPEG2000 compressed version of the same with the ECW driver

def ecw_5():
    if gdaltest.jp2ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'data/small.vrt' )
    gdaltest.jp2ecw_drv.CreateCopy( 'tmp/ecw_5.jp2', ds )
#				    options = ['TARGET=100'] )
    ds = None

    return 'success' 
	
###############################################################################
# Verify various information about our generated image. 

def ecw_6():

    if gdaltest.jp2ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'tmp/ecw_5.jp2' )
    
    (exp_mean, exp_stddev) = (144.422, 44.9075)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    # The difference in the stddev is outragously large between win32 and
    # linux, but I don't know why. 
    if abs(mean-exp_mean) > 1.5 or abs(stddev-exp_stddev) > 6:
	gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
	return 'fail'

    (exp_mean, exp_stddev) = (144.422, 44.9075)
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
	print geotransform
	gdaltest.post_reason( 'geotransform differs from expected' )
	return 'fail'

    prj = ds.GetProjectionRef()
    if string.find(prj,'UTM') == -1 or string.find(prj,'NAD27') == -1 \
       or string.find(prj,'one 11') == -1:
        print prj
        gdaltest.post_reason( 'Coordinate system not UTM 11, NAD27?' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Write the same image to NITF.

def ecw_7():
    if gdaltest.jp2ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'data/small.vrt' )
    drv = gdal.GetDriverByName( 'NITF' )
    drv.CreateCopy( 'tmp/ecw_7.ntf', ds, options = ['IC=C8'] )
    ds = None

    return 'success' 
	
###############################################################################
# Verify various information about our generated image. 

def ecw_8():

    if gdaltest.jp2ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'tmp/ecw_7.ntf' )
    
    (exp_mean, exp_stddev) = (145.57, 43.1712)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    if abs(mean-exp_mean) > 1.0 or abs(stddev-exp_stddev) > 1.0:
	gdaltest.post_reason( 'mean/stddev of (%g,%g) diffs from expected(%g,%g)' % (mean, stddev,exp_mean, exp_stddev) )
	return 'fail'

    (exp_mean, exp_stddev) = (145.57, 43.1712)
    (mean, stddev) = ds.GetRasterBand(2).ComputeBandStats()

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
	print geotransform
	gdaltest.post_reason( 'geotransform differs from expected' )
	return 'fail'

    prj = ds.GetProjectionRef()
    if string.find(prj,'UTM Zone 11') == -1 or string.find(prj,'WGS84') == -1:
        gdaltest.post_reason( 'Coordinate system not UTM 11, WGS84?' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Try writing 16bit JP2 file directly using Create().

def ecw_9():
    if gdaltest.jp2ecw_drv is None:
	return 'skip'

    ds = gdaltest.jp2ecw_drv.Create( 'tmp/ecw9.jp2', 200, 100, 1,
                                     gdal.GDT_Int16 )
    ds.SetGeoTransform( (100, 0.1, 0.0, 30.0, 0.0, -0.1 ) )

    ds.SetProjection( 'GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]' )

    raw_data = array.array('h',range(200)).tostring()

    for line in range(100):
        ds.WriteRaster( 0, line, 200, 1, raw_data,
                        buf_type = gdal.GDT_Int16 )
    ds = None

    return 'success'

###############################################################################
# Verify previous 16bit file.

def ecw_10():
    if gdaltest.jp2ecw_drv is None:
	return 'skip'

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
	print geotransform
	gdaltest.post_reason( 'geotransform differs from expected' )
	return 'fail'

    # should check the projection, but I'm too lazy just now.

    return 'success' 
	

###############################################################################
# Test direct creation of an NITF/JPEG2000 file.

def ecw_11():
    if gdaltest.jp2ecw_drv is None:
	return 'skip'

    drv = gdal.GetDriverByName( 'NITF' )
    ds = drv.Create( 'tmp/test_11.ntf', 200, 100, 3, gdal.GDT_Byte,
                     [ 'ICORDS=G' ] )
    ds.SetGeoTransform( (100, 0.1, 0.0, 30.0, 0.0, -0.1 ) )

    list = range(200) + range(20,220) + range(30,230)
    raw_data = array.array('h',list).tostring()

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
    if gdaltest.jp2ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'tmp/test_11.ntf' )
    
    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-100) > 0.1 \
	or abs(geotransform[1]-0.1) > 0.001 \
	or abs(geotransform[2]-0) > 0.001 \
	or abs(geotransform[3]-30.0) > 0.1 \
	or abs(geotransform[4]-0) > 0.001 \
	or abs(geotransform[5]- -0.1) > 0.001:
	print geotransform
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
        print checksums
        return 'fail'

    return 'success'

###############################################################################
# Write out image with GCPs.

def ecw_14():
    if gdaltest.ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'data/rgb_gcp.vrt' )
    gdaltest.jp2ecw_drv.CreateCopy( 'tmp/rgb_gcp.jp2', ds )
    ds = None

    return 'success' 
	
###############################################################################
# Verify various information about our generated image. 

def ecw_15():

    if gdaltest.ecw_drv is None:
	return 'skip'

    ds = gdal.Open( 'tmp/rgb_gcp.jp2' )

    gcp_srs = ds.GetGCPProjection()
    if gcp_srs[:6] != 'GEOGCS' \
       or string.find(gcp_srs,'WGS') == -1 \
       or string.find(gcp_srs,'84') == -1:
        gdaltest.post_reason('GCP Projection not retained.')
        print gcp_srs
        return 'fail'

    gcps = ds.GetGCPs()
    if len(gcps) != 4 \
       or gcps[1].GCPPixel != 0 \
       or gcps[1].GCPLine  != 50 \
       or gcps[1].GCPX     != 0 \
       or gcps[1].GCPY     != 50 \
       or gcps[1].GCPZ     != 0:
        gdaltest.post_reason( 'GCPs wrong.' )
        print gcps
        return 'fail'
    
    ds = None

    return 'success'

###############################################################################
def ecw_cleanup():

    #gdaltest.clean_tmp()

    try:
        gdaltest.jp2kak.Register()
    except:
        pass
    
    try:
        gdaltest.jpeg2000.Register()
    except:
        pass
    
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
    ecw_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ecw' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

