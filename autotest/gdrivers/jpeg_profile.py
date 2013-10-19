#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic support for ICC profile in JPEG file.
# 
###############################################################################
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

###############################################################################
# This unit test uses a free ICC profile by Marti Maria (littleCMS) 
# <http://www.littlecms.com>
# sRGB.icc uses the zlib license.
# Part of a free package of ICC profile found at:
# http://sourceforge.net/projects/openicc/files/OpenICC-Profiles/

import os
import sys
import string
import shutil
import base64

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal, osr

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []


###############################################################################
# Test writing and reading of ICC profile in CreateCopy()

def jpeg_copy_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = [ 'SOURCE_ICC_PROFILE=' + icc ]

    driver = gdal.GetDriverByName('JPEG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.jpg', ds)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.jpg')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'
    
    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.jpg')

    return 'success'

###############################################################################
# Test writing and reading of ICC profile in CreateCopy() options

def jpeg_copy_options_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = [ 'SOURCE_ICC_PROFILE=' + icc ]

    driver = gdal.GetDriverByName('JPEG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.jpg', ds, options = options)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.jpg')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.jpg')

    return 'success'

###############################################################################
# Test writing and reading of 64K+ ICC profile in CreateCopy()
    
def jpeg_copy_icc_64K():

    # In JPEG, APP2 chunks can only be 64K, so they would be split up.
    # It will still work, but need to test that the segmented ICC profile
    # is put back together correctly.
    # We will simply use the same profile multiple times.
    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    while len(data) < 200000:
        data += data
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = [ 'SOURCE_ICC_PROFILE=' + icc ]

    driver = gdal.GetDriverByName('JPEG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.jpg', ds)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.jpg')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'
    
    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.jpg')

    return 'success'

###############################################################################################

gdaltest_list.append( (jpeg_copy_icc) )
gdaltest_list.append( (jpeg_copy_options_icc) )
gdaltest_list.append( (jpeg_copy_icc_64K) )

if __name__ == '__main__':

    gdaltest.setup_run( 'jpeg_profile' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

