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
    ogrtest.have_read_gpsbabel = False
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
    else:
        ogrtest.have_read_gpsbabel = True

    ogrtest.have_gpsbabel = True

    return 'success'

###############################################################################
# Test reading with explicit subdriver

def ogr_gpsbabel_1():

    if not ogrtest.have_read_gpsbabel:
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

    if not ogrtest.have_read_gpsbabel:
        return 'skip'

    ds = ogr.Open('data/nmea.txt')
    if ds is None:
        return 'fail'

    if ds.GetLayerCount() != 2:
        return 'fail'

    return 'success'

###############################################################################
# Test writing

def ogr_gpsbabel_3():

    if not ogrtest.have_gpsbabel:
        return 'skip'

    ds = ogr.GetDriverByName('GPSBabel').CreateDataSource('GPSBabel:nmea:tmp/nmea.txt')
    lyr = ds.CreateLayer('track_points', geom_type = ogr.wkbPoint)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('track_fid', 0)
    feat.SetField('track_seg_id', 0)
    feat.SetField('track_name', 'TRACK_NAME')
    feat.SetField('name', 'PT_NAME')
    feat.SetField('hdop', 123)
    feat.SetField('vdop', 456)
    feat.SetField('pdop', 789)
    feat.SetField('sat', 6)
    feat.SetField('time', '2010/06/03 12:34:56')
    feat.SetField('fix', '3d')
    geom = ogr.CreateGeometryFromWkt('POINT(2.50 49.25)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    f = open('tmp/nmea.txt', 'rt')
    res = f.read()
    f.close()

    gdal.Unlink('tmp/nmea.txt')

    if res.find('$GPRMC,123456,A,4915.000,N,00230.000,E,0.00,0.00,030610,,*16') == -1 or \
       res.find('$GPGGA,123456,4915.000,N,00230.000,E,1,06,123.0,0.000,M,0.0,M,,*7B') == -1 or \
       res.find('$GPGSA,A,3,,,,,,,,,,,,,789.0,123.0,456.0*33') == -1:
        gdaltest.post_reason('did not get expected result')
        print(res)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_gpsbabel_init,
    ogr_gpsbabel_1,
    ogr_gpsbabel_2,
    ogr_gpsbabel_3 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gpsbabel' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

