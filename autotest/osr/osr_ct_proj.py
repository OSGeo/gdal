#!/usr/bin/env python
###############################################################################
# $Id: osr_ct_proj.py,v 1.8 2006/11/01 04:43:18 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test reprojection of points of many different projections.
# Author:   Frank Warmerdam, warmedam@pobox.com
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
#  $Log: osr_ct_proj.py,v $
#  Revision 1.8  2006/11/01 04:43:18  fwarmerdam
#  Added twopoint equidistant test.
#
#  Revision 1.7  2006/10/27 04:34:35  fwarmerdam
#  corrected licenses
#
#  Revision 1.6  2006/09/28 16:38:39  fwarmerdam
#  Another try...
#
#  Revision 1.5  2006/09/28 16:30:47  fwarmerdam
#  Hopefully this works.
#
#  Revision 1.4  2006/09/28 16:25:14  fwarmerdam
#  Added skip logic if proj not available.
#
#  Revision 1.3  2006/04/21 05:09:25  fwarmerdam
#  added test for Monte Mario (Rome) / Italy Zone 1 problems
#
#  Revision 1.2  2006/04/21 03:25:12  fwarmerdam
#  Added a prime meridian test
#
#  Revision 1.1  2004/11/11 18:27:14  fwarmerdam
#  New
#
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import osr
import gdal
import string

bonne = 'PROJCS["bonne",GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["bonne"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Standard_Parallel_1",60.0],UNIT["Meter",1.0]]'

###############################################################################
# Class to perform the tests.

class ProjTest:
    def __init__( self, src_srs, src_xyz, src_error,
                  dst_srs, dst_xyz, dst_error, options ):
        self.src_srs = src_srs
        self.src_xyz = src_xyz
        self.src_error = src_error
        self.dst_srs = dst_srs
        self.dst_xyz = dst_xyz
        self.dst_error = dst_error
        self.options = options

    def testProj(self):
        src = osr.SpatialReference()
        if src.SetFromUserInput( self.src_srs ) != 0:
            gdaltest.post_reason('SetFromUserInput(%s) failed.' % self.src_srs)
            return 'fail'
        
        dst = osr.SpatialReference()
        if dst.SetFromUserInput( self.dst_srs ) != 0:
            gdaltest.post_reason('SetFromUserInput(%s) failed.' % self.dst_srs)
            return 'fail'

        try:
            gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
            ct = osr.CoordinateTransformation( src, dst )
        except ValueError, (err_msg):
            gdal.PopErrorHandler()
            if string.find(str(err_msg),'Unable to load PROJ.4') != -1:
                gdaltest.post_reason( 'PROJ.4 missing, transforms not available.' )
                return 'skip'
            else:
                gdaltest.post_reason( 'failed to create coordinate transformation')
                return 'fail'
        except:
            gdaltest.post_reason( 'failed to create coordinate transformation')
            return 'fail'

        ######################################################################
        # Tranform source point to destination SRS.
        
        result = ct.TransformPoint( self.src_xyz[0], self.src_xyz[1], self.src_xyz[2] )

        error = abs(result[0] - self.dst_xyz[0]) \
                + abs(result[1] - self.dst_xyz[1]) \
                + abs(result[2] - self.dst_xyz[2])

        if error > self.dst_error:
            gdaltest.post_reason( 'Dest error is %g.' % error )
            return 'fail'

        ######################################################################
        # Now transform back.

        ct = osr.CoordinateTransformation( dst, src )
        
        result = ct.TransformPoint( result[0], result[1], result[2] )

        error = abs(result[0] - self.src_xyz[0]) \
                + abs(result[1] - self.src_xyz[1]) \
                + abs(result[2] - self.src_xyz[2])

        if error > self.src_error:
            gdaltest.post_reason( 'Back to source error is %g.' % error )
            return 'fail'

        return 'success'
        
###############################################################################
# Table of transformations, inputs and expected results (with a threshold)
#
# Each entry in the list should have a tuple with:
#
# - src_srs: any form that SetFromUserInput() will take.
# - (src_x, src_y, src_z): location in src_srs.
# - src_error: threshold for error when srs_x/y is transformed into dst_srs and
#              then back into srs_src.
# - dst_srs: destination srs.
# - (dst_x,dst_y,dst_z): point that src_x/y should transform to.
# - dst_error: acceptable error threshold for comparing to dst_x/y.
# - unit_name: the display name for this unit test.
# - options: eventually we will allow a list of special options here (like one
#   way transformation).  For now just put None. 

transform_list = [ \

    # Simple straight forward reprojection.
    ('+proj=utm +zone=11 +datum=WGS84', (398285.45, 2654587.59, 0.0), 0.02, 
     'WGS84', (-118.0, 24.0, 0.0), 0.00001,
     'UTM_WGS84', None ),

    # Ensure that prime meridian changes are applied.
    ('EPSG:27391', (20000, 40000, 0.0), 0.02, 
     'EPSG:4273', (6.397933,58.358709,0.000000), 0.00001,
     'NGO_Oslo_zone1_NGO', None ),

    # Verify that 26592 "pcs.override" is working well. 
    ('EPSG:26591', (1550000, 10000, 0.0), 0.02, 
     'EPSG:4265', (9.449316,0.090469,0.00), 0.00001,
     'MMRome1_MMGreenwich', None ),

    # Test Bonne projection.
    ('WGS84', (1.0, 65.0, 0.0), 0.00001,
     bonne, (47173.75, 557621.30, 0.0), 0.02,
     'Bonne_WGS84', None),

    # Test Two Point Equidistant
    ('+proj=tpeqd +a=3396000  +b=3396000  +lat_1=36.3201218 +lon_1=-179.1566925 +lat_2=45.8120651 +lon_2=179.3727570 +no_defs', (4983568.76, 2092902.61, 0.0), 0.1,
     '+proj=latlong +a=3396000 +b=3396000', (-140.0, 40.0, 0.0), 0.000001,
     'TPED_Mars', None)
    ]
    
###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

for item in transform_list:
    ut = ProjTest( item[0], item[1], item[2], 
                   item[3], item[4], item[5], 
                   item[7] )
    gdaltest_list.append( (ut.testProj, item[6]) )

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_ct_proj' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

