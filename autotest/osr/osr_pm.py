#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some prime meridian related issues with EPSG translation
#           and evaluation by PROJ.4.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
import string

###############################################################################
#	Check that EPSG:27572 lookup has the prime meridian properly set,
# 	and the central meridian.

def osr_pm_1():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 27572 )

    if abs(float(srs.GetAttrValue( 'PRIMEM', 1))-2.33722917) > 0.0000005:
        gdaltest.post_reason( 'Wrong prime meridian.' )
        return 'fail'
    
    if abs(srs.GetProjParm( osr.SRS_PP_CENTRAL_MERIDIAN )-0.0) > 0.0000005:
        gdaltest.post_reason( 'Wrong central meridian.' )
        return 'fail'

    return 'success'

###############################################################################
#	Check that EPSG:27572 lookup has the prime meridian properly set,
# 	and the central meridian in the PROJ.4 string.

def osr_pm_2():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 27572 )
    proj4_srs = srs.ExportToProj4()

    if proj4_srs.find('+pm=paris' ) == -1:
        print(proj4_srs)
        gdaltest.post_reason( 'prime meridian wrong or missing.' )
        return 'fail'
    
    if proj4_srs.find('+lon_0=0' ) == -1:
        print(proj4_srs)
        gdaltest.post_reason( '+lon_0 is wrong.' )
        return 'fail'
    
    return 'success'

###############################################################################
#	Convert PROJ.4 format to WKT and verify that PM and central meridian
#       are properly preserved.

def osr_pm_3():
    
    srs = osr.SpatialReference()
    srs.ImportFromProj4( '+proj=utm +zone=30 +datum=WGS84 +pm=bogota' )

    if abs(float(srs.GetAttrValue( 'PRIMEM', 1))+74.08091666678081) > 0.0000005:
        gdaltest.post_reason( 'Wrong prime meridian.' )
        return 'fail'

    if abs(srs.GetProjParm( osr.SRS_PP_CENTRAL_MERIDIAN )+3.0) > 0.0000005:
        gdaltest.post_reason( 'Wrong central meridian.' )
        return 'fail'

    return 'success'


###############################################################################

gdaltest_list = [ 
    osr_pm_1,
    osr_pm_2,
    osr_pm_3,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_pm' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

