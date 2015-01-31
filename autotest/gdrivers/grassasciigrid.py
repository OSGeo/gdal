#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GRASS ASCII Grid support.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.

def grassasciigrid_1():

    tst = gdaltest.GDALTest( 'GRASSASCIIGrid', 'grassascii.txt', 1, 212 )
    expected_gt = [-100.0, 62.5, 0.0, 250.0, 0.0, -41.666666666666664]
    return tst.testOpen(check_gt = expected_gt)

###############################################################################

gdaltest_list = [
    grassasciigrid_1
]

if __name__ == '__main__':

    gdaltest.setup_run( 'grassasciigrid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

