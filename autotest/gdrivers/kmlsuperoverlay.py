#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test write functionality for KMLSUPEROVERLAY driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
#
# Permission is hereby granted, free of charge, to any person oxyzaining a
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
import struct
import gdal
import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test CreateCopy() to a KMZ file

def kmlsuperoverlay_1():

    src_ds = gdal.Open('data/small_world.tif')
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('/vsimem/kmlout.kmz', src_ds, options = [ 'FORMAT=PNG'] )
    ds = None

    ds = gdal.Open('/vsizip//vsimem/kmlout.kmz/0/0/0.png')
    diff = gdaltest.compare_ds(src_ds, ds)
    ds = None
    src_ds = None

    gdal.Unlink('/vsimem/kmlout.kmz')

    if diff != 0:
        return 'fail'

    return 'success'

###############################################################################
# Test CreateCopy() to a KML file

def kmlsuperoverlay_2():

    src_ds = gdal.Open('data/utm.tif')
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('tmp/tmp.kml', src_ds)
    ds = None
    src_ds = None

    try:
        os.remove('tmp/0/0/0.jpg')
        os.remove('tmp/0/0/0.kml')

        os.remove('tmp/1/0/0.jpg')
        os.remove('tmp/1/0/0.kml')
        os.remove('tmp/1/0/1.jpg')
        os.remove('tmp/1/0/1.kml')

        os.remove('tmp/1/1/0.jpg')
        os.remove('tmp/1/1/0.kml')
        os.remove('tmp/1/1/1.jpg')
        os.remove('tmp/1/1/1.kml')

        os.rmdir('tmp/0/0')
        os.rmdir('tmp/1/0')
        os.rmdir('tmp/1/1')
        os.rmdir('tmp/0')
        os.rmdir('tmp/1')
        os.remove('tmp/tmp.kml')
    except OSError, e:
        if e.errno == 2:  # no such file or directory
            gdaltest.post_reason("Missing file: %s" % e.filename)
            return 'fail'
        else:
            raise

    return 'success'

###############################################################################
# Cleanup

def  kmlsuperoverlay_cleanup():

    return 'success'


gdaltest_list = [
    kmlsuperoverlay_1,
    kmlsuperoverlay_2,
    kmlsuperoverlay_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( ' kmlsuperoverlay' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

