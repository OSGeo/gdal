#!/usr/bin/env python
###############################################################################
# $Id: hfa.py,v 1.2 2006/10/27 04:27:12 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some functions of HFA driver.  Most testing in ../gcore/hfa_*
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
#  $Log: hfa.py,v $
#  Revision 1.2  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.1  2004/11/11 03:30:59  fwarmerdam
#  New
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we can read the special histogram metadata from a provided image.

def hfa_histread():

    ds = gdal.Open('../gcore/data/utmsmall.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    if md['STATISTICS_MINIMUM'] != '8':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        return 'fail'
    
    if md['STATISTICS_MEDIAN'] != '148':
        gdaltest.post_reason( 'STATISTICS_MEDIAN is wrong.' )
        return 'fail'

    if md['STATISTICS_HISTOMAX'] != '255':
        gdaltest.post_reason( 'STATISTICS_HISTOMAX is wrong.' )
        return 'fail'


    if md['STATISTICS_HISTOBINVALUES'] != '0|0|0|0|0|0|0|0|8|0|0|0|0|0|0|0|23|0|0|0|0|0|0|0|0|29|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|':
        gdaltest.post_reason( 'STATISTICS_HISTOBINVALUES is wrong.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Verify that if we copy this test image to a new Imagine file the histofram
# info is preserved.

def hfa_histwrite():

    drv = gdal.GetDriverByName('HFA')
    ds_src = gdal.Open('../gcore/data/utmsmall.img')
    drv.CreateCopy( 'tmp/work.img', ds_src )
    ds_src = None

    ds = gdal.Open('tmp/work.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None
    
    drv.Delete( 'tmp/work.img' )

    if md['STATISTICS_MINIMUM'] != '8':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        return 'fail'
    
    if md['STATISTICS_MEDIAN'] != '148':
        gdaltest.post_reason( 'STATISTICS_MEDIAN is wrong.' )
        return 'fail'

    if md['STATISTICS_HISTOMAX'] != '255':
        gdaltest.post_reason( 'STATISTICS_HISTOMAX is wrong.' )
        return 'fail'


    if md['STATISTICS_HISTOBINVALUES'] != '0|0|0|0|0|0|0|0|8|0|0|0|0|0|0|0|23|0|0|0|0|0|0|0|0|29|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|':
        gdaltest.post_reason( 'STATISTICS_HISTOBINVALUES is wrong.' )
        return 'fail'

    return 'success'
    
gdaltest_list = [
    hfa_histread,
    hfa_histwrite ]

if __name__ == '__main__':

    gdaltest.setup_run( 'hfa' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

