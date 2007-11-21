#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for NITF driver.
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
import gdal
import array

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Write/Read test of simple byte reference data.

def nitf_1():

    tst = gdaltest.GDALTest( 'NITF', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple 16bit reference data. 

def nitf_2():

    tst = gdaltest.GDALTest( 'NITF', 'int16.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read RGB image with lat/long georeferencing, and verify.

def nitf_3():

    tst = gdaltest.GDALTest( 'NITF', 'rgbsmall.tif', 3, 21349 )
    return tst.testCreateCopy()

###############################################################################
# Test direction creation of an NITF file.

def nitf_4():
    drv = gdal.GetDriverByName( 'NITF' )
    ds = drv.Create( 'tmp/test_4.ntf', 200, 100, 3, gdal.GDT_Byte,
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

def nitf_5():
    ds = gdal.Open( 'tmp/test_4.ntf' )
    
    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 32498
    if chksum != chksum_expect:
	gdaltest.post_reason( 'Did not get expected chksum for band 1' )
	print chksum, chksum_expect
	return 'fail'

    chksum = ds.GetRasterBand(2).Checksum()
    chksum_expect = 42602
    if chksum != chksum_expect:
	gdaltest.post_reason( 'Did not get expected chksum for band 2' )
	print chksum, chksum_expect
	return 'fail'

    chksum = ds.GetRasterBand(3).Checksum()
    chksum_expect = 38982
    if chksum != chksum_expect:
	gdaltest.post_reason( 'Did not get expected chksum for band 3' )
	print chksum, chksum_expect
	return 'fail'

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
# Read existing NITF file.  Verifies the new adjusted IGEOLO interp.

def nitf_6():

    tst = gdaltest.GDALTest( 'NITF', 'rgb.ntf', 3, 21349 )
    return tst.testOpen( check_prj = 'WGS84',
                         check_gt = (-44.842029478458, 0.003503401360, 0,
                                     -22.930748299319, 0, -0.003503401360) )

###############################################################################
# NITF in-memory.

def nitf_7():

    tst = gdaltest.GDALTest( 'NITF', 'rgbsmall.tif', 3, 21349 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Verify we can open an NSIF file, and get metadata including BLOCKA.

def nitf_8():

    ds = gdal.Open( 'data/fake_nsif.ntf' )
    
    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 12033
    if chksum != chksum_expect:
	gdaltest.post_reason( 'Did not get expected chksum for band 1' )
	print chksum, chksum_expect
	return 'fail'

    md = ds.GetMetadata()
    if md['NITF_FHDR'] != 'NSIF01.00':
        gdaltest.post_reason( 'Got wrong FHDR value' )
        return 'fail'

    if md['NITF_BLOCKA_BLOCK_INSTANCE_01'] != '01' \
       or md['NITF_BLOCKA_BLOCK_COUNT'] != '01' \
       or md['NITF_BLOCKA_N_GRAY_01'] != '00000' \
       or md['NITF_BLOCKA_L_LINES_01'] != '01000' \
       or md['NITF_BLOCKA_LAYOVER_ANGLE_01'] != '000' \
       or md['NITF_BLOCKA_SHADOW_ANGLE_01'] != '000' \
       or md['NITF_BLOCKA_FRLC_LOC_01'] != '+41.319331+020.078400' \
       or md['NITF_BLOCKA_LRLC_LOC_01'] != '+41.317083+020.126072' \
       or md['NITF_BLOCKA_LRFC_LOC_01'] != '+41.281634+020.122570' \
       or md['NITF_BLOCKA_FRFC_LOC_01'] != '+41.283881+020.074924':
        gdaltest.post_reason( 'BLOCKA metadata has unexpected value.' )
        return 'fail'
    
    return 'success'

###############################################################################
# Create and read a JPEG encoded NITF file.

def nitf_9():

    src_ds = gdal.Open( 'data/rgbsmall.tif' )
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf9.ntf', src_ds,
                                                  options = ['IC=C3'] )
    src_ds = None
    ds = None

    ds = gdal.Open( 'tmp/nitf9.ntf' )
    
    (exp_mean, exp_stddev) = (65.9532, 46.9026375565)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()
    
    if abs(exp_mean-mean) > 0.01 or abs(exp_stddev-stddev) > 0.01:
        print mean, stddev
        gdaltest.post_reason( 'did not get expected mean or standard dev.' )
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['COMPRESSION'] != 'JPEG':
        gdaltest.post_reason( 'Did not get expected compression value.' )
        return 'fail'
    
    return 'success'

###############################################################################
# For esoteric reasons, createcopy from jpeg compressed nitf files can be
# tricky.  Verify this is working. 

def nitf_10():

    tst = gdaltest.GDALTest( 'NITF', '../tmp/nitf9.ntf', 2, 22296 )
    return tst.testCreateCopy()

###############################################################################
# Test 1bit file ... conveniently very small and easy to include! (#1854)

def nitf_11():

    tst = gdaltest.GDALTest( 'NITF', 'i_3034c.ntf', 1, 170 )
    return tst.testOpen()

###############################################################################
# Cleanup.

def nitf_cleanup():
    try:
        os.remove( 'tmp/test_4.ntf' )
    except:
        pass

    try:
        os.remove( 'tmp/nitf9.ntf' )
    except:
        pass

    return 'success'

gdaltest_list = [
    nitf_1,
    nitf_2,
    nitf_3,
    nitf_4,
    nitf_5,
    nitf_6,
    nitf_7,
    nitf_8,
    nitf_9,
    nitf_10,
    nitf_11,
    nitf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'nitf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

