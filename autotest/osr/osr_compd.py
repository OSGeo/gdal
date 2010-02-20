#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test COMPD_CS support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
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
try:
    from osgeo import osr
except:
    import osr

example_compd_wkt = 'COMPD_CS["OSGB36 / British National Grid + ODN",PROJCS["OSGB 1936 / British National Grid",GEOGCS["OSGB 1936",DATUM["OSGB_1936",SPHEROID["Airy 1830",6377563.396,299.3249646,AUTHORITY["EPSG",7001]],TOWGS84[375,-111,431,0,0,0,0],AUTHORITY["EPSG",6277]],PRIMEM["Greenwich",0,AUTHORITY["EPSG",8901]],UNIT["DMSH",0.0174532925199433,AUTHORITY["EPSG",9108]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG",4277]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",49],PARAMETER["central_meridian",-2],PARAMETER["scale_factor",0.999601272],PARAMETER["false_easting",400000],PARAMETER["false_northing",-100000],UNIT["metre_1",1,AUTHORITY["EPSG",9001]],AXIS["E",EAST],AXIS["N",NORTH],AUTHORITY["EPSG",27700]],VERT_CS["Newlyn",VERT_DATUM["Ordnance Datum Newlyn",2005,AUTHORITY["EPSG",5101]],UNIT["metre_2",1,AUTHORITY["EPSG",9001]],AXIS["Up",UP],AUTHORITY["EPSG",5701]],AUTHORITY["EPSG",7405]]'


###############################################################################
# Test parsing and a few operations on a compound coordinate system.

def osr_compd_1():

    srs = osr.SpatialReference()
    srs.ImportFromWkt( example_compd_wkt )

    if not srs.IsProjected():
        gdaltest.post_reason( 'Projected COMPD_CS not recognised as projected.')
        return 'fail'

    if srs.IsGeographic():
        gdaltest.post_reason( 'projected COMPD_CS misrecognised as geographic.')
        return 'fail'

    if srs.IsLocal():
        gdaltest.post_reason( 'projected COMPD_CS misrecognised as local.')
        return 'fail'

    expected_proj4 = '+proj=tmerc +lat_0=49 +lon_0=-2 +k=0.999601272 +x_0=400000 +y_0=-100000 +ellps=airy +datum=OSGB36 +units=m +no_defs '
    got_proj4 = srs.ExportToProj4()

    if expected_proj4 != got_proj4:
        print( 'Got:      %s' % got_proj4 )
        print( 'Expected: %s' % expected_proj4 )
        gdaltest.post_reason( 'did not get expected proj.4 translation of compd_cs' )
        return 'fail'
    
    if srs.GetLinearUnitsName() != 'metre_1':
        gdaltest.post_reason( 'Did not get expected linear units.' )
        return 'false'

    return 'success'

###############################################################################
# Test SetFromUserInput()

def osr_compd_2():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( example_compd_wkt )

    if not srs.IsProjected():
        gdaltest.post_reason( 'Projected COMPD_CS not recognised as projected.')
        return 'fail'

    return 'success'

gdaltest_list = [ 
    osr_compd_1,
    osr_compd_2,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_compd' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

