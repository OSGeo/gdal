#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Northwood GRC driver
# Author:   Chaitanya kumar CH, <chaitanya at osgeo dot in>
# 
###############################################################################
# Copyright (c) 2009, Chaitanya kumar CH, <chaitanya at osgeo dot in>
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

import gdaltest

###############################################################################
# Test a GRC dataset

def nwt_grc_1():

    tst = gdaltest.GDALTest( 'NWT_GRC', 'nwt_grc.grc', 1, 46760 )
    return tst.testOpen()

gdaltest_list = [
    nwt_grc_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'nwt_grc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

