#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR GPSBabel driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal

###############################################################################
# Check that dependencies are met

def ogr_gpsbabel_init():

    # Test if the gpsbabel is accessible
    ogrtest.have_gpsbabel = False
    try:
        ret = gdaltest.runexternal('gpsbabel -V')
    except:
        ret = ''
    if ret.find('GPSBabel') == -1:
        print('Cannot access GPSBabel utility')
        return 'skip'

    try:
        ds = ogr.Open( 'data/test.gpx' )
    except:
        ds = None

    if ds is None:
        print('GPX driver not configured for read support')
        return 'skip'

    ogrtest.have_gpsbabel = True

    return 'success'

###############################################################################
# Test reading with explicit subdriver

def ogr_gpsbabel_1():

    if not ogrtest.have_gpsbabel:
        return 'skip'

    ds = ogr.Open('GPSBabel:nmea:data/nmea.txt')
    if ds is None:
        return 'fail'

    if ds.GetLayerCount() != 2:
        return 'fail'

    return 'success'

###############################################################################
# Test reading with implicit subdriver

def ogr_gpsbabel_2():

    if not ogrtest.have_gpsbabel:
        return 'skip'

    ds = ogr.Open('data/nmea.txt')
    if ds is None:
        return 'fail'

    if ds.GetLayerCount() != 2:
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_gpsbabel_init,
    ogr_gpsbabel_1,
    ogr_gpsbabel_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gpsbabel' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

