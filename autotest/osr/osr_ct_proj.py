#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test reprojection of points of many different projections.
# Author:   Frank Warmerdam, warmedam@pobox.com
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import osr, gdal

bonne = 'PROJCS["bonne",GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["bonne"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Standard_Parallel_1",60.0],UNIT["Meter",1.0]]'

###############################################################################
# Class to perform the tests.

class ProjTest:
    def __init__( self, src_srs, src_xyz, src_error,
                  dst_srs, dst_xyz, dst_error, options, requirements ):
        self.src_srs = src_srs
        self.src_xyz = src_xyz
        self.src_error = src_error
        self.dst_srs = dst_srs
        self.dst_xyz = dst_xyz
        self.dst_error = dst_error
        self.options = options
        self.requirements = requirements

    def testProj(self):

        import osr_ct
        osr_ct.osr_ct_1()
        if gdaltest.have_proj4 == 0:
            return 'skip'

        if self.requirements is not None and self.requirements[:5] == 'GRID:':
            try:
                proj_lib = os.environ['PROJ_LIB']
            except:
                #print( 'PROJ_LIB unset, skipping test.' )
                return 'skip'

            try:
                open( proj_lib + '/' + self.requirements[5:] )
            except:
                #print( 'Did not find GRID:%s' % self.requirements[5:] )
                return 'skip'
        
        src = osr.SpatialReference()
        if src.SetFromUserInput( self.src_srs ) != 0:
            gdaltest.post_reason('SetFromUserInput(%s) failed.' % self.src_srs)
            return 'fail'
        
        dst = osr.SpatialReference()
        if dst.SetFromUserInput( self.dst_srs ) != 0:
            gdaltest.post_reason('SetFromUserInput(%s) failed.' % self.dst_srs)
            return 'fail'

        if self.requirements is not None and self.requirements[0] != 'G':
            additionnal_error_str = ' Check that proj version is >= %s ' % self.requirements
        else:
            additionnal_error_str = ''

        try:
            gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
            ct = osr.CoordinateTransformation( src, dst )
            gdal.PopErrorHandler()
            if gdal.GetLastErrorMsg().find('Unable to load PROJ.4') != -1:
                gdaltest.post_reason( 'PROJ.4 missing, transforms not available.' )
                return 'skip'
        except ValueError:
            gdal.PopErrorHandler()
            if gdal.GetLastErrorMsg().find('Unable to load PROJ.4') != -1:
                gdaltest.post_reason( 'PROJ.4 missing, transforms not available.' )
                return 'skip'
            else:
                gdaltest.post_reason( 'failed to create coordinate transformation. %s' % gdal.GetLastErrorMsg())
                return 'fail'
        except:
            gdal.PopErrorHandler()
            gdaltest.post_reason( 'failed to create coordinate transformation. %s' % gdal.GetLastErrorMsg())
            return 'fail'

        ######################################################################
        # Tranform source point to destination SRS.
        
        result = ct.TransformPoint( self.src_xyz[0], self.src_xyz[1], self.src_xyz[2] )

        error = abs(result[0] - self.dst_xyz[0]) \
                + abs(result[1] - self.dst_xyz[1]) \
                + abs(result[2] - self.dst_xyz[2])

        if error > self.dst_error:
            gdaltest.post_reason( 'Dest error is %g, got (%.15g,%.15g,%.15g)%s' \
                                  % (error, result[0], result[1], result[2], additionnal_error_str) )
            return 'fail'

        ######################################################################
        # Now transform back.

        ct = osr.CoordinateTransformation( dst, src )
        
        result = ct.TransformPoint( result[0], result[1], result[2] )

        error = abs(result[0] - self.src_xyz[0]) \
                + abs(result[1] - self.src_xyz[1]) \
                + abs(result[2] - self.src_xyz[2])

        if error > self.src_error:
            gdaltest.post_reason( 'Back to source error is %g.%s' % (error, additionnal_error_str) )
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
# - requirements: string with minimum proj version required, GRID:<gridname>
#                 or None depend on requirements for the test.

transform_list = [ \

    # Simple straight forward reprojection.
    ('+proj=utm +zone=11 +datum=WGS84', (398285.45, 2654587.59, 0.0), 0.02, 
     'WGS84', (-118.0, 24.0, 0.0), 0.00001,
     'UTM_WGS84', None, None ),

    # Ensure that prime meridian *and* axis orientation changes are applied.
    # Note that this test will fail with PROJ.4 4.7 or earlier, it requires
    # axis support in PROJ 4.8.0. 
#    ('EPSG:27391', (40000, 20000, 0.0), 0.02, 
#     'EPSG:4273', (6.397933,58.358709,0.000000), 0.00001,
#     'NGO_Oslo_zone1_NGO', None, '4.8.0' ),

    # Verify that 26592 "pcs.override" is working well. 
    ('EPSG:26591', (1550000, 10000, 0.0), 0.02, 
     'EPSG:4265', (9.449316,0.090469,0.00), 0.00001,
     'MMRome1_MMGreenwich', None, None ),

    # Test Bonne projection.
    ('WGS84', (1.0, 65.0, 0.0), 0.00001,
     bonne, (47173.75, 557621.30, 0.0), 0.02,
     'Bonne_WGS84', None, None),

    # Test Two Point Equidistant
    ('+proj=tpeqd +a=3396000  +b=3396000  +lat_1=36.3201218 +lon_1=-179.1566925 +lat_2=45.8120651 +lon_2=179.3727570 +no_defs', (4983568.76, 2092902.61, 0.0), 0.1,
     '+proj=latlong +a=3396000 +b=3396000', (-140.0, 40.0, 0.0), 0.000001,
     'TPED_Mars', None, None),

    # test scale factor precision (per #1970)
    ('data/wkt_rt90.def', (1572570.342,6728429.67,0.0), 0.001,
     ' +proj=utm +zone=33 +ellps=GRS80 +units=m +no_defs',(616531.1155,6727527.5682,0.0), 0.001,
     'ScalePrecision(#1970)', None, None),

    # Test Google Mercator (EPSG:3785)
    ('EPSG:3785', (1572570.342,6728429.67,0.0), 0.001,
     'WGS84',(14.126639735716626, 51.601722482149995, 0.0), 0.0000001,
     'GoogleMercator(#3136)', None, None),

    # Test Equirectangular with all parameters
    ('+proj=eqc +ellps=sphere  +lat_0=-2 +lat_ts=1 +lon_0=-10', (-14453132.04, 4670184.72,0.0), 0.1,
     '+proj=latlong +ellps=sphere', (-140.0, 40.0, 0.0), 0.000001,
     'Equirectangular(#2706)', None, "4.6.1"),

    # Test Geocentric
    ('+proj=latlong +datum=WGS84', (-140.0, 40.0, 0.0), 0.000001,
     'EPSG:4328', (-3748031.46884168,-3144971.82314589,4077985.57220038), 0.1,
     'Geocentric', None, None),

    # Test Vertical Datum Shift with a change of horizontal and vert units.
    ('+proj=utm +zone=11 +datum=WGS84', (100000.0,3500000.0,0.0), 0.1,
     '+proj=utm +zone=11 +datum=WGS84 +geoidgrids=egm96_15.gtx +units=us-ft', (328083.333225467,11482916.6665952,135.881817690812), 0.01,
     'EGM 96 Conversion', None, "GRID:egm96_15.gtx" ),

    # Test optimization in case of identical projections (projected)
    ('+proj=utm +zone=11 +datum=NAD27 +units=m', (440720.0,3751260.0,0.0), 0,
     '+proj=utm +zone=11 +datum=NAD27 +units=m', (440720.0,3751260.0,0.0), 0,
     'No-op Optimization (projected)', None, None),

    # Test optimization in case of identical projections (geodetic)
    ('+proj=longlat +datum=WGS84', (2,49,0.0), 0,
     '+proj=longlat +datum=WGS84', (2,49,0.0), 0,
     'No-op Optimization (geodetic)', None, None)

    ]
    
###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

for item in transform_list:
    ut = ProjTest( item[0], item[1], item[2], 
                   item[3], item[4], item[5], 
                   item[7], item[8] )
    gdaltest_list.append( (ut.testProj, item[6]) )

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_ct_proj' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

