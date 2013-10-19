#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic support for ICC profile in TIFF file.
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
# Test writing and reading of ICC profile in Create() options

def tiff_write_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = [ 'SOURCE_ICC_PROFILE=' + icc ]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)

    # Check with dataset from Create()
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None
    
    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'
    
    # Check again with dataset from Open()
    ds = gdal.Open('tmp/icc_test.tiff')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None
    
    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    driver.Delete('tmp/icc_test.tiff')
        
    return 'success'

###############################################################################
# Test writing and reading of ICC profile in CreateCopy()

def tiff_copy_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = [ 'SOURCE_ICC_PROFILE=' + icc ]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds)
    
    # Check with dataset from CreateCopy()
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test2.tiff')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')

    return 'success'

###############################################################################
# Test writing and reading of ICC profile in CreateCopy() options

def tiff_copy_options_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = [ 'SOURCE_ICC_PROFILE=' + icc ]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds, options = options)

    # Check with dataset from CreateCopy()
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test2.tiff')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')

    return 'success'
    
def cvtTuple2String(a):
    s = '';
    for i in range(0, len(a)):
        if (s != ''):
            s = s + ', '
        s = s + str(a[i])

    return s

###############################################################################
# Test writing and reading of ICC colorimetric data from options

def tiff_copy_options_colorimetric_data():
    # sRGB values
    source_primaries = [(0.64, 0.33, 1.0), (0.3, 0.6, 1.0), (0.15, 0.06, 1.0)]
    source_whitepoint = (0.31271, 0.32902, 1.0)
    tifftag_transferfunction = (list(range(1, 256*4, 4)), list(range(2, 256*4+1, 4)), list(range(3, 256*4+2, 4)))
    
    options = [ 'SOURCE_PRIMARIES_RED=' + cvtTuple2String(source_primaries[0]),
        'SOURCE_PRIMARIES_GREEN=' + cvtTuple2String(source_primaries[1]),
        'SOURCE_PRIMARIES_BLUE=' + cvtTuple2String(source_primaries[2]),
        'SOURCE_WHITEPOINT=' + cvtTuple2String(source_whitepoint),
        'TIFFTAG_TRANSFERFUNCTION_RED=' + cvtTuple2String(tifftag_transferfunction[0]),
        'TIFFTAG_TRANSFERFUNCTION_GREEN=' + cvtTuple2String(tifftag_transferfunction[1]),
        'TIFFTAG_TRANSFERFUNCTION_BLUE=' + cvtTuple2String(tifftag_transferfunction[2])
        ]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)
    
    # Check with dataset from CreateCopy()	
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds, options = options)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')
    
    for i in range(0, 3):
        if abs(source_whitepoint2[i] - source_whitepoint[i]) > 0.0001:
            return 'fail'

    source_primaries2 = [ 
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')') ]

    for j in range(0, 3):
        for i in range(0, 3):
            if abs(source_primaries2[j][i] - source_primaries[j][i]) > 0.0001:
                gdaltest.post_reason('fail')
                return 'fail'
                
    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']') )

    if tifftag_transferfunction2 != tifftag_transferfunction:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check again with dataset from Open()	
    ds2 = gdal.Open('tmp/icc_test2.tiff')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')
    
    for i in range(0, 3):
        if abs(source_whitepoint2[i] - source_whitepoint[i]) > 0.0001:
            gdaltest.post_reason('fail')
            return 'fail'

    source_primaries2 = [ 
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')') ]

    for j in range(0, 3):
        for i in range(0, 3):
            if abs(source_primaries2[j][i] - source_primaries[j][i]) > 0.0001:
                gdaltest.post_reason('fail')
                return 'fail'

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']') )

    if tifftag_transferfunction2 != tifftag_transferfunction:
        gdaltest.post_reason('fail')
        return 'fail'

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')

    return 'success'

###############################################################################
# Test writing and reading of ICC colorimetric data in the file

def tiff_copy_colorimetric_data():
    # sRGB values
    source_primaries = [(0.64, 0.33, 1.0), (0.3, 0.6, 1.0), (0.15, 0.06, 1.0)]
    source_whitepoint = (0.31271, 0.32902, 1.0)
    tifftag_transferfunction = (list(range(1, 256*4, 4)), list(range(2, 256*4+1, 4)), list(range(3, 256*4+2, 4)))
    
    options = [ 'SOURCE_PRIMARIES_RED=' + cvtTuple2String(source_primaries[0]),
        'SOURCE_PRIMARIES_GREEN=' + cvtTuple2String(source_primaries[1]),
        'SOURCE_PRIMARIES_BLUE=' + cvtTuple2String(source_primaries[2]),
        'SOURCE_WHITEPOINT=' + cvtTuple2String(source_whitepoint),
        'TIFFTAG_TRANSFERFUNCTION_RED=' + cvtTuple2String(tifftag_transferfunction[0]),
        'TIFFTAG_TRANSFERFUNCTION_GREEN=' + cvtTuple2String(tifftag_transferfunction[1]),
        'TIFFTAG_TRANSFERFUNCTION_BLUE=' + cvtTuple2String(tifftag_transferfunction[2]) 
        ]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)
    ds = None
    ds = gdal.Open('tmp/icc_test.tiff')
    
    # Check with dataset from CreateCopy()	
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')
    
    for i in range(0, 3):
        if abs(source_whitepoint2[i] - source_whitepoint[i]) > 0.0001:
            gdaltest.post_reason('fail')
            return 'fail'

    source_primaries2 = [ 
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')') ]

    for j in range(0, 3):
        for i in range(0, 3):
            if abs(source_primaries2[j][i] - source_primaries[j][i]) > 0.0001:
                gdaltest.post_reason('fail')
                return 'fail'
                
    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']') )

    if tifftag_transferfunction2 != tifftag_transferfunction:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check again with dataset from Open()	
    ds2 = gdal.Open('tmp/icc_test2.tiff')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')
    
    for i in range(0, 3):
        if abs(source_whitepoint2[i] - source_whitepoint[i]) > 0.0001:
            gdaltest.post_reason('fail')
            return 'fail'

    source_primaries2 = [ 
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')') ]

    for j in range(0, 3):
        for i in range(0, 3):
            if abs(source_primaries2[j][i] - source_primaries[j][i]) > 0.0001:
                gdaltest.post_reason('fail')
                return 'fail'

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']') )

    if tifftag_transferfunction2 != tifftag_transferfunction:
        gdaltest.post_reason('fail')
        return 'fail'

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')

    return 'success'
    
###############################################################################
# Test updating ICC profile

def tiff_update_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data)
    f.close()

    # Create dummy file
    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)
    ds = None

    ds = gdal.Open('tmp/icc_test.tiff', gdal.GA_Update)

    if sys.version_info >= (3,0,0):
        icc = icc.decode('ascii')
    ds.SetMetadataItem('SOURCE_ICC_PROFILE', icc, 'COLOR_PROFILE')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None
    
    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'
        
    # Reopen the file to verify it was written.
    ds = gdal.Open('tmp/icc_test.tiff')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None
    
    if md['SOURCE_ICC_PROFILE'] != icc:
        gdaltest.post_reason('fail')
        return 'fail'

    driver.Delete('tmp/icc_test.tiff')
        
    return 'success'

###############################################################################
# Test updating colorimetric options

def tiff_update_colorimetric():
    source_primaries = [(0.234, 0.555, 1.0), (0.2,0,1), (2,3.5,1)]
    source_whitepoint = (0.31271, 0.32902, 1.0)
    tifftag_transferfunction = (list(range(1, 256*4, 4)), list(range(2, 256*4+1, 4)), list(range(3, 256*4+2, 4)))
    
    # Create dummy file
    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)
    ds = None

    ds = gdal.Open('tmp/icc_test.tiff', gdal.GA_Update)
    
    ds.SetMetadataItem('SOURCE_PRIMARIES_RED', cvtTuple2String(source_primaries[0]), 'COLOR_PROFILE')
    ds.SetMetadataItem('SOURCE_PRIMARIES_GREEN', cvtTuple2String(source_primaries[1]), 'COLOR_PROFILE')
    ds.SetMetadataItem('SOURCE_PRIMARIES_BLUE', cvtTuple2String(source_primaries[2]), 'COLOR_PROFILE')
    ds.SetMetadataItem('SOURCE_WHITEPOINT', cvtTuple2String(source_whitepoint), 'COLOR_PROFILE')
    ds.SetMetadataItem('TIFFTAG_TRANSFERFUNCTION_RED', cvtTuple2String(tifftag_transferfunction[0]), 'COLOR_PROFILE')
    ds.SetMetadataItem('TIFFTAG_TRANSFERFUNCTION_GREEN', cvtTuple2String(tifftag_transferfunction[1]), 'COLOR_PROFILE')
    ds.SetMetadataItem('TIFFTAG_TRANSFERFUNCTION_BLUE', cvtTuple2String(tifftag_transferfunction[2]), 'COLOR_PROFILE')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')
    
    for i in range(0, 3):
        if abs(source_whitepoint2[i] - source_whitepoint[i]) > 0.0001:
            gdaltest.post_reason('fail')
            return 'fail'

    source_primaries2 = [ 
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')') ]

    for j in range(0, 3):
        for i in range(0, 3):
            if abs(source_primaries2[j][i] - source_primaries[j][i]) > 0.0001:
                gdaltest.post_reason('fail')
                return 'fail'
                
    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']') )

    if tifftag_transferfunction2 != tifftag_transferfunction:
        gdaltest.post_reason('fail')
        return 'fail'
        
    # Reopen the file to verify it was written.
    ds = gdal.Open('tmp/icc_test.tiff')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None
    
    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')
    
    for i in range(0, 3):
        if abs(source_whitepoint2[i] - source_whitepoint[i]) > 0.0001:
            gdaltest.post_reason('fail')
            return 'fail'

    source_primaries2 = [ 
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')') ]

    for j in range(0, 3):
        for i in range(0, 3):
            if abs(source_primaries2[j][i] - source_primaries[j][i]) > 0.0001:
                gdaltest.post_reason('fail')
                return 'fail'
                
    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']') )

    if tifftag_transferfunction2 != tifftag_transferfunction:
        gdaltest.post_reason('fail')
        return 'fail'

    driver.Delete('tmp/icc_test.tiff')
        
    return 'success'
    
###############################################################################################

gdaltest_list.append( (tiff_write_icc) )
gdaltest_list.append( (tiff_copy_icc) )
gdaltest_list.append( (tiff_copy_options_icc) )
gdaltest_list.append( (tiff_copy_options_colorimetric_data) )
gdaltest_list.append( (tiff_copy_colorimetric_data) )
gdaltest_list.append( (tiff_update_icc) )
gdaltest_list.append( (tiff_update_colorimetric) )

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_profile' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

