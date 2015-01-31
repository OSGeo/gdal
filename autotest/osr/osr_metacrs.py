#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test with MetaCRS TestSuite
# Author:   Frank Warmerdam, warmedam@pobox.com
# 
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

import sys
import csv

sys.path.append( '../pymod' )

import gdaltest

from osgeo import osr, gdal

###############################################################################
# Class to perform the tests.

class MetaCRSTest:
    def __init__( self, test_line ):
        self.test_line = test_line

    def parse_line( self ):
        test_line = self.test_line

        self.src_srs = self.build_srs(test_line['srcCrsAuth'],
                                      test_line['srcCrs'])
        try:
            self.dst_srs = self.build_srs(test_line['tgtCrsAuth'],
                                          test_line['tgtCrs'])
        except:
            # Old style
            self.dst_srs = self.build_srs(test_line['tgtCrsType'],
                                          test_line['tgtCrs'])

        if self.src_srs is None or self.dst_srs is None:
            return 'fail'

        try:
            self.src_xyz = ( float(test_line['srcOrd1']),
                             float(test_line['srcOrd2']),
                             float(test_line['srcOrd3']) )
        except:
            self.src_xyz = ( float(test_line['srcOrd1']),
                             float(test_line['srcOrd2']),
                             0.0 )
        try:
            self.dst_xyz = ( float(test_line['tgtOrd1']),
                             float(test_line['tgtOrd2']),
                             float(test_line['tgtOrd3']) )
        except:
            self.dst_xyz = ( float(test_line['tgtOrd1']),
                             float(test_line['tgtOrd2']),
                             0.0 )
        try:
            self.dst_error = max(float(test_line['tolOrd1']),
                                 float(test_line['tolOrd2']),
                                 float(test_line['tolOrd3']))
        except:
            self.dst_error = max(float(test_line['tolOrd1']),
                                 float(test_line['tolOrd2']))

        return 'success'
        
    def build_srs(self,type,crstext):
        if type == 'EPSG':
            srs = osr.SpatialReference()
            if srs.ImportFromEPSGA( int(crstext) ) == 0:
                return srs
            else:
                gdaltest.post_reason( 'failed to translate EPSG:' + crstext )
                return None
        else:
            gdaltest.post_reason( 'unsupported srs type: ' + type )
            return None
            
    def testMetaCRS(self):
        result = self.parse_line()
        if result != 'success':
            return result
        
        try:
            gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
            ct = osr.CoordinateTransformation( self.src_srs, self.dst_srs )
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
        # Tranform source point to destination SRS, swapping EPSG GEOGCS
        # axes if needed.

        if self.src_srs.EPSGTreatsAsLatLong():
            self.src_xyz = (self.src_xyz[1],self.src_xyz[0],self.src_xyz[2])
        
        result = ct.TransformPoint( self.src_xyz[0], self.src_xyz[1], self.src_xyz[2] )

        if self.src_srs.EPSGTreatsAsLatLong():
            result = (result[1],result[0],result[2])
            
        ######################################################################
        # Check results.
        error = abs(result[0] - self.dst_xyz[0]) \
                + abs(result[1] - self.dst_xyz[1]) \
                + abs(result[2] - self.dst_xyz[2])

        if error > self.dst_error:
            err_msg = 'Dest error is %g, src=%g,%g,%g, dst=%g,%g,%g, exp=%g,%g,%g' \
                      % (error,
                         self.src_xyz[0],self.src_xyz[1],self.src_xyz[2],
                         result[0], result[1], result[2],
                         self.dst_xyz[0],self.dst_xyz[1],self.dst_xyz[2])

            gdaltest.post_reason( err_msg )

            gdal.Debug( 'OSR', 'Src SRS:\n%s\n\nDst SRS:\n%s\n' \
                        % (self.src_srs.ExportToPrettyWkt(),
                           self.dst_srs.ExportToPrettyWkt()) )
                        
            return 'fail'

        return 'success'
        
###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

csv_reader = csv.DictReader(open('data/Test_Data_File.csv','rt'))

for test in csv_reader:
    ut = MetaCRSTest( test )
    gdaltest_list.append( (ut.testMetaCRS, test['testName']) )

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_metacrs' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

