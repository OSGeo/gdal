#!/usr/bin/env python
###############################################################################
# $Id: osr_proj4.py 11065 2007-03-24 09:35:32Z mloskot $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some URL specific translation issues.
# Author:   Howard Butler <hobu.inc@gmail.com>
# 
###############################################################################
# Copyright (c) 2007, Howard Butler <hobu.inc@gmail.com>
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import osr


def osr_url_1():
    
    srs = osr.SpatialReference()
    try:
        srs.ImportFromUrl( 'http://spatialreference.org/ref/epsg/4326/proj4' )
    except:
        return 'fail'

    return 'success'

gdaltest_list = [ 
    osr_url_1,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_url' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

