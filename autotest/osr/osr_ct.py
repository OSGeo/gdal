#!/usr/bin/env python
###############################################################################
# $Id: osr_ct.py,v 1.3 2006/10/27 04:34:35 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test coordinate transformations. 
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
#  $Log: osr_ct.py,v $
#  Revision 1.3  2006/10/27 04:34:35  fwarmerdam
#  corrected licenses
#
#  Revision 1.2  2003/11/10 17:07:58  warmerda
#  improve 3rd test check and comments
#
#  Revision 1.1  2003/11/10 17:01:24  warmerda
#  New
#
#

import os
import sys
import string

sys.path.append( '../pymod' )

import gdal
import gdaltest
import osr
import ogr


###############################################################################
# Verify that we have PROJ.4 available. 

def osr_ct_1():

    
    gdaltest.have_proj4 = 0
    
    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    try:
        ct = osr.CoordinateTransformation( ll_srs, utm_srs )
    except ValueError, (err_msg):
        gdal.PopErrorHandler()
        if string.find(str(err_msg),'Unable to load PROJ.4') != -1:
            gdaltest.post_reason( 'PROJ.4 missing, transforms not available.' )
            return 'skip'
        else:
            gdaltest.post_reason( str(err_msg) )
            return 'fail'

    gdal.PopErrorHandler()
    if ct is None:
        gdaltest.post_reason( 'Unable to create simple CoordinateTransformat.')
        return 'fail'

    gdaltest.have_proj4 = 1

    return 'success'

###############################################################################
# Actually perform a simple LL to UTM conversion. 

def osr_ct_2():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    gdaltest.ct = osr.CoordinateTransformation( ll_srs, utm_srs )

    result = gdaltest.ct.TransformPoint( -117.5, 32.0, 0.0 )
    if abs(result[0] - 452772.06) > 0.01 \
       or abs(result[1] - 3540544.89 ) > 0.01 \
       or abs(result[2] - 0.0) > 0.01:
        gdaltest.post_reason( 'Wrong LL to UTM result' )
        return 'fail'
    else:
        return 'success'

###############################################################################
# Transform an OGR geometry ... this is mostly aimed at ensuring that
# the OGRCoordinateTransformation target SRS isn't deleted till the output
# geometry which also uses it is deleted.

def osr_ct_3():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    ct = osr.CoordinateTransformation( ll_srs, utm_srs )

    pnt = ogr.CreateGeometryFromWkt( 'POINT(-117.5 32.0)', ll_srs )
    result = pnt.Transform( ct )

    ll_srs = None
    ct = None
    utm_srs = None

    out_srs = pnt.GetSpatialReference().ExportToPrettyWkt()
    if out_srs[0:6] != 'PROJCS':
        gdaltest.post_reason( 'output srs corrupt, ref counting issue?' )
        return 'fail'

    pnt = None

    return 'success'
    
    
###############################################################################
# Cleanup

def osr_ct_cleanup():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    gdaltest.ct = None

    return 'success'

gdaltest_list = [ 
    osr_ct_1,
    osr_ct_2,
    osr_ct_3,
    osr_ct_cleanup,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_ct' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

