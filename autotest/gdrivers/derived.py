#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test cderived driver
# Author:   Julien Michel <julien dot michel at cnes dot fr>
#
###############################################################################
# Copyright (c) 2016, Julien Michel, <julien dot michel at cnes dot fr>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test opening a L1C product


def derived_test():
    filename = "data/cfloat64.tif"
    gdal.ErrorReset()
    ds = gdal.Open(filename)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    got_dsds = ds.GetMetadata('DERIVED_SUBDATASETS')

    expected_dsds = {'DERIVED_SUBDATASET_0_NAME' : 'DERIVED_SUBDATASET:AMPLITUDE:data/cfloat64.tif',
                     'DERIVED_SUBDATASET_0_DESC' : 'Amplitude of input bands from data/cfloat64.tif',
                     'DERIVED_SUBDATASET_1_NAME' : 'DERIVED_SUBDATASET:PHASE:data/cfloat64.tif',
                     'DERIVED_SUBDATASET_1_DESC' : 'Phase of input bands from data/cfloat64.tif',
                     'DERIVED_SUBDATASET_2_NAME' : 'DERIVED_SUBDATASET:REAL:data/cfloat64.tif',
                     'DERIVED_SUBDATASET_2_DESC' : 'Real part of input bands from data/cfloat64.tif',
                     'DERIVED_SUBDATASET_3_NAME' : 'DERIVED_SUBDATASET:IMAG:data/cfloat64.tif',
                     'DERIVED_SUBDATASET_3_DESC' : 'Imaginary part of input bands from data/cfloat64.tif',
                     'DERIVED_SUBDATASET_4_NAME' : 'DERIVED_SUBDATASET:CONJ:data/cfloat64.tif',
                     'DERIVED_SUBDATASET_4_DESC' : 'Conjugate of input bands from data/cfloat64.tif',
                     'DERIVED_SUBDATASET_5_NAME' : 'DERIVED_SUBDATASET:INTENSITY:data/cfloat64.tif',
                     'DERIVED_SUBDATASET_5_DESC' : 'Intensity (squared amplitude) of input bands from data/cfloat64.tif',
                     'DERIVED_SUBDATASET_6_NAME' : 'DERIVED_SUBDATASET:LOGAMPLITUDE:data/cfloat64.tif',
                     'DERIVED_SUBDATASET_6_DESC' : 'log10 of amplitude of input bands from data/cfloat64.tif'}

    if got_dsds != expected_dsds:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_dsds)
        return 'fail'
    

    for (key,val) in expected_dsds.iteritems():
        if key.endswith('_NAME'):
            ds = gdal.Open(val)
            if ds is None or gdal.GetLastErrorMsg() != '':
                gdaltest.post_reason('fail')
                return 'fail'
    
    return 'success'


gdaltest_list = [
    cderived_test
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'derived' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
