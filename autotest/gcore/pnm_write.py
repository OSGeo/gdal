#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for portable anymap file format
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

init_list = [ \
    ('byte.tif', 1, 4672, None),
    ('uint16.tif', 1, 4672, None) ]

gdaltest_list = []

for item in init_list:
    ut = gdaltest.GDALTest( 'PNM', item[0], item[1], item[2] )
    if ut is None:
        print( 'PNM tests skipped' )
    gdaltest_list.append( (ut.testCreateCopy, item[0]) )
    gdaltest_list.append( (ut.testCreate, item[0]) )

if __name__ == '__main__':

    gdaltest.setup_run( 'pnm_write' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

