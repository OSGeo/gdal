#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a BMP file.
# Author:   Andrey Kiselev, dron@remotesensing.org
# 
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# 
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

init_list = [ \
    ('1bit.bmp', 1, 200, None),
    ('4bit_pal.bmp', 1, 2587, None),
    ('8bit_pal.bmp', 1, 4672, None)]


def bmp_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/bmp/8bit_pal_rle.bmp', '8bit_pal_rle.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'BMP', 'tmp/cache/8bit_pal_rle.bmp', 1, 17270, filename_absolute = 1 )

    return tst.testOpen()

def bmp_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/bmp/24bit.bmp', '24bit.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'BMP', 'tmp/cache/24bit.bmp', 1, 7158, filename_absolute = 1 )
    if tst == 'success':
        tst = gdaltest.GDALTest( 'BMP', 'tmp/cache/24bit.bmp', 3, 27670, filename_absolute = 1 )

    return tst.testOpen()

for item in init_list:
    ut = gdaltest.GDALTest( 'BMP', item[0], item[1], item[2] )
    if ut is None:
        print( 'BMP tests skipped' )
        sys.exit()
    gdaltest_list.append( (ut.testOpen, item[0]) )

gdaltest_list.append( bmp_online_1 )
gdaltest_list.append( bmp_online_2 )

if __name__ == '__main__':

    gdaltest.setup_run( 'bmp_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

