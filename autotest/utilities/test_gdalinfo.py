#!/usr/bin/env python
###############################################################################
# $Id: test_gdalinfo.py $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalinfo testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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

###############################################################################
# 
def test_gdalinfo_init():
    try:
        sys.winver
        gdaltest.devnull = 'NUL'
    except:
        gdaltest.devnull = '/dev/null'

    gdaltest.shelltestskip = True;
    try:
        gdaltest.gdalinfoexe = os.getcwd() + '/../../gdal/apps/gdalinfo'
        ret = os.system(gdaltest.gdalinfoexe + ' --version')
        if ret == 0:
            gdaltest.shelltestskip = False;
            return 'success'
    except:
        pass

    return 'skip'

###############################################################################
# 

def test_gdalinfo_1():
    if gdaltest.shelltestskip:
        return 'skip'

    ret = os.system(gdaltest.gdalinfoexe + ' ../gcore/data/byte.tif >' + gdaltest.devnull)

    if ret == 0:
        return 'success'
    else:
        return 'fail'


gdaltest_list = [
    test_gdalinfo_init,
    test_gdalinfo_1,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalinfo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





