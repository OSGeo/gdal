#!/usr/bin/env python
###############################################################################
# $Id: osr_esri.py,v 1.5 2006/10/27 04:34:35 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some ESRI specific translation issues.
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
#  $Log: osr_esri.py,v $
#  Revision 1.5  2006/10/27 04:34:35  fwarmerdam
#  corrected licenses
#
#  Revision 1.4  2006/06/26 19:41:34  fwarmerdam
#  added polar stereographic related tests
#
#  Revision 1.3  2005/12/11 21:23:57  fwarmerdam
#  morphtoesri now sets PROJCS name based on PROJECTION
#
#  Revision 1.2  2005/08/03 16:16:17  fwarmerdam
#  Added new tests for pengs requested changes.
#
#  Revision 1.1  2003/06/23 14:47:55  warmerda
#  New
#
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import osr

###############################################################################
# This test verifies that morphToESRI() translates ideosyncratic datum names
# from "EPSG" form to ESRI from when the exception list comes from the
# gdal_datum.csv file. 

def osr_esri_1():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4202 )

    if srs.GetAttrValue( 'DATUM' ) != 'Australian_Geodetic_Datum_1966':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after EPSG import.' %\
                              srs.GetAttrValue( 'DATUM' ) )
        return 'fail'
    
    srs.MorphToESRI()

    if srs.GetAttrValue( 'DATUM' ) != 'D_Australian_1966':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'DATUM' ) )
        return 'fail'
    
    srs.MorphFromESRI()

    if srs.GetAttrValue( 'DATUM' ) != 'Australian_Geodetic_Datum_1966':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after ESRI unmorph.' %\
                              srs.GetAttrValue( 'DATUM' ) )
        return 'fail'
    
    return 'success'

###############################################################################
# Verify that exact correct form of UTM names is established when
# translating certain GEOGCSes to ESRI format.

def osr_esri_2():
    
    srs = osr.SpatialReference()
    srs.SetFromUserInput( '+proj=utm +zone=11 +south +datum=WGS84' )

    srs.MorphToESRI()
    
    if srs.GetAttrValue( 'GEOGCS' ) != 'GCS_WGS_1984':
        gdaltest.post_reason( 'Got wrong GEOGCS name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'GEOGCS' ) )
        return 'fail'
    
    if srs.GetAttrValue( 'PROJCS' ) != 'WGS_1984_UTM_Zone_11S':
        gdaltest.post_reason( 'Got wrong PROJCS name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJCS' ) )
        return 'fail'
    
    return 'success'

###############################################################################
# Verify that Unnamed is changed to Unknown in morphToESRI().

def osr_esri_3():
    
    srs = osr.SpatialReference()
    srs.SetFromUserInput( '+proj=mill +datum=WGS84' )

    srs.MorphToESRI()
    
    if srs.GetAttrValue( 'PROJCS' ) != 'Miller_Cylindrical':
        gdaltest.post_reason( 'Got wrong PROJCS name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJCS' ) )
        return 'fail'
    
    return 'success'

###############################################################################
# Verify Polar Stereographic translations work properly OGR to ESRI.

def osr_esri_4():
    
    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'PROJCS["PS Test",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.2572235629972]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_origin",-80.2333],PARAMETER["central_meridian",171],PARAMETER["scale_factor",0.9999],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]' )

    srs.MorphToESRI()
    
    if srs.GetAttrValue( 'PROJECTION' ) != 'Stereographic_South_Pole':
        gdaltest.post_reason( 'Got wrong PROJECTION name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJECTION' ) )
        return 'fail'
    
    if srs.GetProjParm('standard_parallel_1') != -80.2333:
        gdaltest.post_reason( 'Got wrong parameter value (%g) after ESRI morph.' %\
                              srs.GetProjParm('standard_parallel_1') )
        return 'fail'
    
    return 'success'

###############################################################################
# Verify Polar Stereographic translations work properly ESRI to OGR.

def osr_esri_5():
    
    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'PROJCS["PS Test",GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137,298.2572235629972]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Stereographic_South_Pole"],PARAMETER["standard_parallel_1",-80.2333],PARAMETER["central_meridian",171],PARAMETER["scale_factor",0.9999],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]' )

    srs.MorphFromESRI()
    
    if srs.GetAttrValue( 'PROJECTION' ) != 'Polar_Stereographic':
        gdaltest.post_reason( 'Got wrong PROJECTION name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJECTION' ) )
        return 'fail'
    
    if srs.GetProjParm('latitude_of_origin') != -80.2333:
        gdaltest.post_reason( 'Got wrong parameter value (%g) after ESRI morph.' %\
                              srs.GetProjParm('latitude_of_origin') )
        return 'fail'
    
    return 'success'

gdaltest_list = [ 
    osr_esri_1,
    osr_esri_2,
    osr_esri_3,
    osr_esri_4,
    osr_esri_5,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_esri' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

