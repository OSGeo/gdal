#!/usr/bin/env python
###############################################################################
# $Id: nitf.py,v 1.7 2006/10/27 04:27:12 fwarmerdam Exp $
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
# 
#  $Log: nitf.py,v $
#  Revision 1.7  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.6  2005/10/13 01:42:36  fwarmerdam
#  Added vsimem test.
#
#  Revision 1.5  2005/03/21 16:20:21  fwarmerdam
#  added geotransform test from existing IGEOLO.
#
#  Revision 1.4  2005/02/22 08:18:54  fwarmerdam
#  fixed issue with color interp testing
#
#  Revision 1.3  2005/02/18 19:27:15  fwarmerdam
#  Added direct Create test with band interps
#
#  Revision 1.2  2004/02/20 00:30:32  warmerda
#  use .tif files as source data
#
#  Revision 1.1  2003/07/17 16:18:43  warmerda
#  New
#
#

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
# Cleanup.

def nitf_cleanup():
    try:
        os.remove( 'tmp/test_4.ntf' )
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
    nitf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'nitf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

