#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the image reprojection functions. Try to test as many
#           resamplers as possible (we have optimized resamplers for some
#           data types, test them too).
# Author:   Andrey Kiselev, dron16@ak4719.spb.edu
# 
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron16@ak4719.spb.edu>
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

sys.path.append( '../pymod' )

import ogrtest
import gdaltest

###############################################################################

def cutline_1():

    tst = gdaltest.GDALTest( 'VRT', 'cutline_noblend.vrt', 1, 11409 )
    return tst.testOpen()

###############################################################################

def cutline_2():

    if not ogrtest.have_geos():
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'cutline_blend.vrt', 1, 21395 )
    return tst.testOpen()

###############################################################################

def cutline_3():

    if not ogrtest.have_geos():
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'cutline_multipolygon.vrt', 1, 20827 )
    return tst.testOpen()

###############################################################################

gdaltest_list = [
    cutline_1,
    cutline_2,
    cutline_3
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'cutline' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

