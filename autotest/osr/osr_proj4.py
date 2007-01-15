#!/usr/bin/env python
###############################################################################
# $Id: osr_proj4.py,v 1.3 2006/10/27 04:34:35 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some PROJ.4 specific translation issues.
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
# 
#  $Log: osr_proj4.py,v $
#  Revision 1.3  2006/10/27 04:34:35  fwarmerdam
#  corrected licenses
#
#  Revision 1.2  2003/07/18 04:49:36  warmerda
#  added test for exponents
#
#  Revision 1.1  2003/06/27 18:13:37  warmerda
#  New
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import osr

###############################################################################
# Test the the +k_0 flag works as well as +k when consuming PROJ.4 format.
# This is from Bugzilla bug 355.
#

def osr_proj4_1():
    
    srs = osr.SpatialReference()
    srs.ImportFromProj4( '+proj=tmerc +lat_0=53.5000000000 +lon_0=-8.0000000000 +k_0=1.0000350000 +x_0=200000.0000000000 +y_0=250000.0000000000 +a=6377340.189000 +rf=299.324965 +towgs84=482.530,-130.596,564.557,-1.042,-0.214,-0.631,8.15' )

    if abs(srs.GetProjParm( osr.SRS_PP_SCALE_FACTOR )-1.000035) > 0.0000005:
        gdaltest.post_reason( '+k_0 not supported on import from PROJ.4?' )
        return 'fail'

    return 'success'

###############################################################################
# Verify that we can import strings with parameter values that are exponents
# and contain a plus sign.  As per bug 355 in GDAL/OGR's bugzilla. 
#

def osr_proj4_2():
    
    srs = osr.SpatialReference()
    srs.ImportFromProj4( "+proj=lcc +x_0=0.6096012192024384e+06 +y_0=0 +lon_0=90dw +lat_0=42dn +lat_1=44d4'n +lat_2=42d44'n +a=6378206.400000 +rf=294.978698 +nadgrids=conus,ntv1_can.dat" )

    if abs(srs.GetProjParm( osr.SRS_PP_FALSE_EASTING )-609601.219) > 0.0005:
        gdaltest.post_reason( 'Parsing exponents not supported?' )
        return 'fail'

    return 'success'

gdaltest_list = [ 
    osr_proj4_1,
    osr_proj4_2,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_proj4' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

