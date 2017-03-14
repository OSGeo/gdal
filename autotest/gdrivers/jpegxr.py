#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  JPEGXR Testing.
# Author:   Mateusz Loskot <mateusz at loskot dot net>
#
###############################################################################
# Copyright (c) 2017, Mateusz Loskot <mateusz at loskot dot net>
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

import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
def jpegxr_1():
    try:
        gdaltest.jpegxr_drv = gdal.GetDriverByName('JPEGXR')
    except:
        gdaltest.jpegxr_drv = None
        return 'skip'

    return 'success'

###############################################################################
def jpegxr_2():
    if gdaltest.jpegxr_drv is None:
        return 'skip'

    jxr_test_files = [
        ('lenna-256x256-8bpp-Gray.jxr', (60269, )),
        ('lenna-256x256-24bpp-BGR.jxr', (62731, 63106, 34990)),
        ('mandril-512x512-24bpp-RGB.jxr', (54211, 51131, 12543)),
        ('lenna-256x256-32bpp-BGRA.jxr', (62731, 63106, 34990, 17849)),
        ('lenna-256x256-32bpp-RGBA.jxr', (38199, 61818, 61402, 17849)),
    ]

    for jxr_test in jxr_test_files:
        jxr_file = jxr_test[0]
        for band, checksum in enumerate(jxr_test[1], 1):
            tst = gdaltest.GDALTest('JPEGXR', jxr_file, band, checksum)
            if tst.testOpen() != 'success':
                return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [
    jpegxr_1,
    jpegxr_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'JPEGXR' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

