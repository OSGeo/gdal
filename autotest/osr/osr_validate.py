#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test (error cases of) OSRValidate
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import osr

###############################################################################
# No root pointer

def osr_validate_1():

    empty_srs = osr.SpatialReference()
    if empty_srs.Validate() == 0:
        return 'fail'

    return 'success'

###############################################################################
# Unrecognised root node

def osr_validate_2():

    srs = osr.SpatialReference()
    srs.ImportFromWkt("FOO[]")
    if srs.Validate() == 0:
        return 'fail'

    return 'success'

###############################################################################
# COMPD_CS errors

def osr_validate_3():

    # No DATUM child in GEOGCS
    srs = osr.SpatialReference()
    
    srs.ImportFromWkt("""COMPD_CS[]""")
    print srs.Validate()
    
    srs.ImportFromWkt("""COMPD_CS["MYNAME",GEOGCS[]]""")
    if srs.Validate() == 0:
        return 'fail'

    # AUTHORITY has wrong number of children (1), not 2.
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""COMPD_CS["MYNAME",AUTHORITY[]]""")
    if srs.Validate() == 0:
        return 'fail'

    # Unexpected child for COMPD_CS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""COMPD_CS["MYNAME",FOO[]]""")
    if srs.Validate() == 0:
        return 'fail'

    return 'success'

###############################################################################
# VERT_CS errors

def osr_validate_4():

    # Invalid number of children : 1
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",VERT_DATUM[]]""")
    if srs.Validate() == 0:
        return 'fail'

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",UNIT[]]""")
    if srs.Validate() == 0:
        return 'fail'

    # AXIS has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",AXIS[]]""")
    if srs.Validate() == 0:
        return 'fail'

    # AUTHORITY has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",AUTHORITY[]]""")
    if srs.Validate() == 0:
        return 'fail'

    # Unexpected child for VERT_CS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",FOO[]]""")
    if srs.Validate() == 0:
        return 'fail'

    # No VERT_DATUM child in VERT_CS
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME"]""")
    if srs.Validate() == 0:
        return 'fail'

    # No UNIT child in VERT_CS
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",VERT_DATUM["MYNAME",2005,AUTHORITY["EPSG","0"]]]""")
    if srs.Validate() == 0:
        return 'fail'

    # Too many AXIS children in VERT_CS
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",VERT_DATUM["MYNAME",2005,AUTHORITY["EPSG","0"]],UNIT["metre",1],AXIS["foo",foo],AXIS["bar",bar]]""")
    if srs.Validate() == 0:
        return 'fail'

    return 'success'

###############################################################################
# GEOCCS errors

def osr_validate_5():

    #srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]')

    # PRIMEM has wrong number of children (1),not 2 or 3 as expected
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM[],UNIT["meter",1]]')
    if srs.Validate() == 0:
        return 'fail'

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT[]]')
    if srs.Validate() == 0:
        return 'fail'

    # AXIS has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],AXIS[],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]')
    if srs.Validate() == 0:
        return 'fail'

    # AUTHORITY has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1],AUTHORITY[]]')
    if srs.Validate() == 0:
        return 'fail'

    # Unexpected child for GEOCCS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",FOO[]]')
    if srs.Validate() == 0:
        return 'fail'

    # No DATUM child in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric"]')
    if srs.Validate() == 0:
        return 'fail'

    # No PRIMEM child in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]]]')
    if srs.Validate() == 0:
        return 'fail'

    # No UNIT child in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]],PRIMEM["Greenwich",0]]')
    if srs.Validate() == 0:
        return 'fail'

    # Wrong number of AXIS children in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],AXIS["foo",foo],UNIT["meter",1]]')
    if srs.Validate() == 0:
        return 'fail'

    return 'success'

###############################################################################
# PROJCS errors

def osr_validate_6():

    #srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32631"]]')

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",UNIT[]]')
    if srs.Validate() == 0:
        return 'fail'

    # PARAMETER has wrong number of children (1),not 2 as expected
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PARAMETER[]]')
    if srs.Validate() == 0:
        return 'fail'

    # Unrecognised PARAMETER `foo'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PARAMETER["foo",0]]')
    if srs.Validate() == 0:
        return 'fail'

    # PROJECTION has wrong number of children (0),not 1 or 2 as expected
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION]')
    if srs.Validate() == 0:
        return 'fail'

    # Unrecognised PROJECTION `foo'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["foo"]]')
    if srs.Validate() == 0:
        return 'fail'

    # Unsupported, but recognised PROJECTION `Tunisia_Mining_Grid'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Tunisia_Mining_Grid"]]')
    if srs.Validate() == 0:
        return 'fail'

    # Unexpected child for PROJECTION `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Transverse_Mercator", FOO]]')
    if srs.Validate() == 0:
        return 'fail'

    # AUTHORITY has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Transverse_Mercator", AUTHORITY]]')
    if srs.Validate() == 0:
        return 'fail'

    # AUTHORITY has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",AUTHORITY]]')
    if srs.Validate() == 0:
        return 'fail'

    # AXIS has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",AXIS]]')
    if srs.Validate() == 0:
        return 'fail'

    # Unexpected child for PROJCS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",FOO]]')
    if srs.Validate() == 0:
        return 'fail'

    # PROJCS does not have PROJECTION subnode.
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N"]')
    if srs.Validate() == 0:
        return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [ 
    osr_validate_1,
    osr_validate_2,
    osr_validate_3,
    osr_validate_4,
    osr_validate_5,
    osr_validate_6 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_validate' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

