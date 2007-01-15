#!/usr/bin/env python
###############################################################################
# $Id: hdf4_read.py,v 1.1 2003/04/01 09:55:23 dron Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a HDF file.
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
# 
#  $Log: hdf4_read.py,v $
#  Revision 1.1  2003/04/01 09:55:23  dron
#  New.
#
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

init_list = [ \
    ('byte_3.hdf', 1, 4672, None),
    ('int16_3.hdf', 1, 4672, None),
    ('uint16_3.hdf', 1, 4672, None),
    ('int32_3.hdf', 1, 4672, None),
    ('uint32_3.hdf', 1, 4672, None),
    ('float32_3.hdf', 1, 4672, None),
    ('float64_3.hdf', 1, 4672, None),
    ('utmsmall_3.hdf', 1, 50054, None),
    ('byte_2.hdf', 1, 4672, None),
    ('int16_2.hdf', 1, 4672, None),
    ('uint16_2.hdf', 1, 4672, None),
    ('int32_2.hdf', 1, 4672, None),
    ('uint32_2.hdf', 1, 4672, None),
    ('float32_2.hdf', 1, 4672, None),
    ('float64_2.hdf', 1, 4672, None),
    ('utmsmall_2.hdf', 1, 50054, None)]

for item in init_list:
    ut = gdaltest.GDALTest( 'HDF4', item[0], item[1], item[2] )
    if ut is None:
	print( 'HDF4 tests skipped' )
	sys.exit()
    gdaltest_list.append( (ut.testOpen, item[0]) )

if __name__ == '__main__':

    gdaltest.setup_run( 'hdf4_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

