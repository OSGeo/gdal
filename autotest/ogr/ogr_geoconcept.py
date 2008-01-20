#!/usr/bin/env python
###############################################################################
# $Id: ogr_mem.py 13026 2007-11-25 19:20:48Z warmerdam $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Geoconcept driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import gdal

###############################################################################
# Simple read test of known file.

def ogr_gxt_1():

    gdaltest.gxt_ds = ogr.Open('data/expected_000_GRD.gxt' )

    if gdaltest.gxt_ds is None:
        return 'fail'

    if gdaltest.gxt_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'Got wrong layer count.' )
        return 'fail'

    lyr = gdaltest.gxt_ds.GetLayer(0)
    if lyr.GetName() != '000_GRD.000_GRD':
        gdaltest.post_reason( 'got unexpected layer name.' )
        return 'fail'

    if lyr.GetFeatureCount() != 10:
        gdaltest.post_reason( 'got wrong feature count.' )
        return 'fail'

    expect = [ '000-2007-0050-7130-LAMB93',
               '000-2007-0595-7130-LAMB93',
               '000-2007-0595-6585-LAMB93',
               '000-2007-1145-6250-LAMB93',
               '000-2007-0050-6585-LAMB93',
               '000-2007-0050-7130-LAMB93',
               '000-2007-0595-7130-LAMB93',
               '000-2007-0595-6585-LAMB93',
               '000-2007-1145-6250-LAMB93',
               '000-2007-0050-6585-LAMB93' ]
    
    tr = ogrtest.check_features_against_list( lyr, 'idSel', expect )

    if tr:
        return 'success'
    else:
        return 'fail'

gdaltest_list = [ 
    ogr_gxt_1,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gxt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

