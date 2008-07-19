#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  BSB Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>
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
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test Read

def bsb_1():
    try:
        gdaltest.bsb_dr = gdal.GetDriverByName( 'BSB' )
    except:
        return 'skip'

    if gdaltest.bsb_dr is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'BSB', 'rgbsmall.kap', 1, 30321 )

    return tst.testOpen()

###############################################################################
# Test CreateCopy

def bsb_2():

    if gdaltest.bsb_dr is None:
        return 'skip'

    md = gdaltest.bsb_dr.GetMetadata()
    if not md.has_key('DMD_CREATIONDATATYPES'):
        return 'skip'

    tst = gdaltest.GDALTest( 'BSB', 'rgbsmall.kap', 1, 30321 )

    return tst.testCreateCopy()


gdaltest_list = [
    bsb_1,
    bsb_2,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'BSB' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

