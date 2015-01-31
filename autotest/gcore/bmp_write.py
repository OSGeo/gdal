#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for Microsoft Bitmap (.bmp)
#           BMP driver.
# Author:   Andrey Kiselev, dron@remotesensing.org
# 
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test creating an in memory copy.

def bmp_vsimem():

    tst = gdaltest.GDALTest( 'BMP', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1 )


###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = [ bmp_vsimem ]

init_list = [ \
    ('byte.tif', 1, 4672, None),
    ('utmsmall.tif', 1, 50054, None),
    ('8bit_pal.bmp', 1, 4672, None),]

for item in init_list:
    ut = gdaltest.GDALTest( 'BMP', item[0], item[1], item[2] )
    if ut is None:
        print( 'BMP tests skipped' )
        sys.exit()
    gdaltest_list.append( (ut.testCreateCopy, item[0]) )
    gdaltest_list.append( (ut.testCreate, item[0]) )

gdaltest_list.append( (gdaltest.clean_tmp, 'bmp_cleanup') )

if __name__ == '__main__':

    gdaltest.setup_run( 'bmp_write' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

