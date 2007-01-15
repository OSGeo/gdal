#!/usr/bin/env python
###############################################################################
# $Id: vrt_read.py,v 1.3 2006/10/27 04:31:51 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a VRT file.
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
#  $Log: vrt_read.py,v $
#  Revision 1.3  2006/10/27 04:31:51  fwarmerdam
#  corrected licenses
#
#  Revision 1.2  2003/03/27 08:20:04  dron
#  Typo fixed.
#
#  Revision 1.1  2003/03/26 19:37:16  dron
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
    ('byte.vrt', 1, 4672, None),
    ('int16.vrt', 1, 4672, None),
    ('uint16.vrt', 1, 4672, None),
    ('int32.vrt', 1, 4672, None),
    ('uint32.vrt', 1, 4672, None),
    ('float32.vrt', 1, 4672, None),
    ('float64.vrt', 1, 4672, None),
    ('cint16.vrt', 1, 5028, None),
    ('cint32.vrt', 1, 5028, None),
    ('cfloat32.vrt', 1, 5028, None),
    ('cfloat64.vrt', 1, 5028, None)]

for item in init_list:
    ut = gdaltest.GDALTest( 'VRT', item[0], item[1], item[2] )
    if ut is None:
	print( 'VRT tests skipped' )
	sys.exit()
    gdaltest_list.append( (ut.testOpen, item[0]) )

if __name__ == '__main__':

    gdaltest.setup_run( 'vrt_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

