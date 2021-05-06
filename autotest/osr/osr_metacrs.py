#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test with MetaCRS TestSuite
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
import csv
import sys

import pytest

import gdaltest

from osgeo import osr, gdal


###############################################################################
# When imported build a list of units based on the files available.

csv_rows = list(csv.DictReader(open(os.path.join(os.path.dirname(__file__), 'data/Test_Data_File.csv'), 'rt')))


class TestMetaCRS(object):
    @pytest.mark.parametrize(
        'test_line', csv_rows, ids=[row['testName'] for row in csv_rows]
    )
    def test_metacrs(self, test_line):
        self.test_line = test_line
        self.src_xyz = None
        self.dst_xyz = None
        self.src_srs = None
        self.dst_srs = None
        self.dst_error = None

        result = self.parse_line()

        ct = osr.CoordinateTransformation(self.src_srs, self.dst_srs)

        ######################################################################
        # Transform source point to destination SRS

        result = ct.TransformPoint(self.src_xyz[0], self.src_xyz[1], self.src_xyz[2])
 
        # This is odd, but it seems the expected results are switched
        if self.src_srs.EPSGTreatsAsLatLong():
            result = (result[1], result[0], result[2])

        ######################################################################
        # Check results.
        error = abs(result[0] - self.dst_xyz[0]) \
            + abs(result[1] - self.dst_xyz[1]) \
            + abs(result[2] - self.dst_xyz[2])

        if error > self.dst_error:
            err_msg = 'Dest error is %g, src=%g,%g,%g, dst=%.18g,%.18g,%.18g, exp=%.18g,%.18g,%.18g' \
                      % (error,
                         self.src_xyz[0], self.src_xyz[1], self.src_xyz[2],
                         result[0], result[1], result[2],
                         self.dst_xyz[0], self.dst_xyz[1], self.dst_xyz[2])

            gdal.Debug('OSR', 'Src SRS:\n%s\n\nDst SRS:\n%s\n'
                       % (self.src_srs.ExportToPrettyWkt(),
                          self.dst_srs.ExportToPrettyWkt()))

            if test_line['testName'] == 'WGS84 Geogrpahic 2D to CSPC Z3 USFT NAD83' and sys.platform == 'darwin':
                pytest.xfail('Failure ' + err_msg + '. See https://github.com/rouault/gdal/runs/1329425333?check_suite_focus=true')

            pytest.fail(err_msg)

        self.src_srs = None
        self.dst_srs = None

    def parse_line(self):
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

        assert not (self.src_srs is None or self.dst_srs is None)

        try:
            self.src_xyz = (float(test_line['srcOrd1']),
                            float(test_line['srcOrd2']),
                            float(test_line['srcOrd3']))
        except:
            self.src_xyz = (float(test_line['srcOrd1']),
                            float(test_line['srcOrd2']),
                            0.0)
        try:
            self.dst_xyz = (float(test_line['tgtOrd1']),
                            float(test_line['tgtOrd2']),
                            float(test_line['tgtOrd3']))
        except:
            self.dst_xyz = (float(test_line['tgtOrd1']),
                            float(test_line['tgtOrd2']),
                            0.0)
        try:
            self.dst_error = max(float(test_line['tolOrd1']),
                                 float(test_line['tolOrd2']),
                                 float(test_line['tolOrd3']))
        except:
            self.dst_error = max(float(test_line['tolOrd1']),
                                 float(test_line['tolOrd2']))

    def build_srs(self, typ, crstext):
        if typ == 'EPSG':
            srs = osr.SpatialReference()
            if srs.ImportFromEPSG(int(crstext)) == 0:
                return srs
            gdaltest.post_reason('failed to translate EPSG:' + crstext)
            return None
        gdaltest.post_reason('unsupported srs type: ' + typ)
        return None
