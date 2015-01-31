#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for RST/Idrisi driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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

###############################################################################
# Read test of byte file.

def idrisi_1():

    tst = gdaltest.GDALTest( 'RST', 'byte.rst', 1, 5044 )
    return tst.testOpen()

###############################################################################
# Read test of byte file.

def idrisi_2():

    tst = gdaltest.GDALTest( 'RST', 'real.rst', 1, 5275 )
    return tst.testOpen()

###############################################################################
# 

def idrisi_3():

    tst = gdaltest.GDALTest( 'RST', 'float32.bil', 1, 27 )

    return tst.testCreate( new_filename = 'tmp/float32.rst', out_bands=1, vsimem = 1 )
    
###############################################################################
# 

def idrisi_4():

    tst = gdaltest.GDALTest( 'RST', 'rgbsmall.tif', 2, 21053 )

    return tst.testCreateCopy( check_gt = 1, check_srs = 1,
                               new_filename = 'tmp/rgbsmall_cc.rst', vsimem = 1 )
    
###############################################################################
# Cleanup.

def idrisi_cleanup():
    gdaltest.clean_tmp()
    try:
        os.unlink('data/rgbsmall.tif.aux.xml')
        print('FIXME?: data/rgbsmall.tif.aux.xml is produced by those tests') 
    except:
        pass
    return 'success'

gdaltest_list = [
    idrisi_1,
    idrisi_2,
    idrisi_3,
    idrisi_4,
    idrisi_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'idrisi' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

