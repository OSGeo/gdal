#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_ls.py testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault @ mines-paris dot org>
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

from osgeo import gdal
import sys
import os

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import test_py_scripts

###############################################################################
def run_gdal_ls(argv):
    script_path = test_py_scripts.get_py_script('gdal_ls')
    if script_path is None:
        return ('skip', None)

    saved_syspath = sys.path
    sys.path.append(script_path)
    try:
        import gdal_ls
    except:
        sys.path = saved_syspath
        return ('fail', None)

    sys.path = saved_syspath

    from sys import version_info
    if version_info >= (3,0,0):
        import io
        outstr = io.StringIO()
    else:
        import StringIO
        outstr = StringIO.StringIO()
    ret = gdal_ls.gdal_ls(argv, outstr)
    retstr = outstr.getvalue()
    outstr.close()

    if ret != 0:
        gdaltest.post_reason('got error code : %d' % ret)
        return ('fail', 'None')

    return ('success', retstr)

###############################################################################
# List one file

def test_gdal_ls_py_1():
    (ret, ret_str) = run_gdal_ls(['', '-l', '../ogr/data/poly.shp'])

    if ret != 'success':
        return ret

    if ret_str.find('poly.shp') == -1:
        print(ret_str)
        return 'fail'

    return 'success'

###############################################################################
# List one dir

def test_gdal_ls_py_2():
    (ret, ret_str) = run_gdal_ls(['', '-l', '../ogr/data'])

    if ret != 'success':
        return ret

    if ret_str.find('poly.shp') == -1:
        print(ret_str)
        return 'fail'

    return 'success'

###############################################################################
# List recursively

def test_gdal_ls_py_3():
    (ret, ret_str) = run_gdal_ls(['', '-R', '../ogr/data'])

    if ret != 'success':
        return ret

    if ret_str.find('PROJ_UNITS') == -1:
        print(ret_str)
        return 'fail'

    return 'success'

###############################################################################
# List in a .zip

def test_gdal_ls_py_4():
    (ret, ret_str) = run_gdal_ls(['', '-l', '/vsizip/../ogr/data/poly.zip'])

    if ret != 'success':
        return ret

    if ret_str.find('-r--r--r--  1 unknown unknown          415 2008-02-11 21:35 /vsizip/../ogr/data/poly.zip/poly.PRJ') == -1:
        print(ret_str)
        if gdaltest.skip_on_travis():
            # FIXME
            # Fails on Travis with dates at 1970-01-01 00:00
            # Looks like a 32/64bit issue with Python bindings of VSIStatL()
            return 'skip'
        return 'fail'

    return 'success'

###############################################################################
# List dir in /vsicurl/

def test_gdal_ls_py_5():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would stall for a long time')
        return 'skip'

    f = gdal.VSIFOpenL('/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip', 'rb')
    if f is None:
        return 'skip'
    d = gdal.VSIFReadL(1,1,f)
    gdal.VSIFCloseL(f)
    if len(d) == 0:
        return 'skip'

    (ret, ret_str) = run_gdal_ls(['', '-R', 'http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/'])

    if ret != 'success':
        return ret

    if ret_str.find('/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/wkb_wkt/3d_broken_line.wkb') == -1:
        print(ret_str)
        return 'fail'

    return 'success'

###############################################################################
# List in a .zip in /vsicurl/

def test_gdal_ls_py_6():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    f = gdal.VSIFOpenL('/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip', 'rb')
    if f is None:
        return 'skip'
    d = gdal.VSIFReadL(1,1,f)
    gdal.VSIFCloseL(f)
    if len(d) == 0:
        return 'skip'

    (ret, ret_str) = run_gdal_ls(['', '-l', '/vsizip/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip'])

    if ret != 'success':
        return ret

    if ret_str.find('-r--r--r--  1 unknown unknown          415 2008-02-11 21:35 /vsizip/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip/poly.PRJ') == -1:
        print(ret_str)
        if gdaltest.skip_on_travis():
            # FIXME
            # Fails on Travis with dates at 1970-01-01 00:00
            # Looks like a 32/64bit issue with Python bindings of VSIStatL()
            return 'skip'
        return 'fail'

    return 'success'

###############################################################################
# List dir in /vsicurl/ and recurse in zip

def test_gdal_ls_py_7():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would stall for a long time')
        return 'skip'

    f = gdal.VSIFOpenL('/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip', 'rb')
    if f is None:
        return 'skip'
    d = gdal.VSIFReadL(1,1,f)
    gdal.VSIFCloseL(f)
    if len(d) == 0:
        return 'skip'

    (ret, ret_str) = run_gdal_ls(['', '-R', '-Rzip', 'http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/'])

    if ret != 'success':
        return ret

    if ret_str.find('/vsizip//vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip/poly.PRJ') == -1:
        print(ret_str)
        return 'fail'

    return 'success'

###############################################################################
# List FTP dir in /vsicurl/

def test_gdal_ls_py_8():
    if not gdaltest.run_slow_tests():
        return 'skip'

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would stall for a long time')
        return 'skip'

    f = gdal.VSIFOpenL('/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip', 'rb')
    if f is None:
        return 'skip'
    d = gdal.VSIFReadL(1,1,f)
    gdal.VSIFCloseL(f)
    if len(d) == 0:
        return 'skip'

    (ret, ret_str) = run_gdal_ls(['', '-l', '-R', '-Rzip', 'ftp://ftp.remotesensing.org/gdal/data/aig'])

    if ret != 'success':
        return ret

    if ret_str.find('-r--r--r--  1 unknown unknown        24576 2007-03-29 00:00 /vsicurl/ftp://ftp.remotesensing.org/gdal/data/aig/nzdem/info/arc0002r.001') == -1:
        print(ret_str)
        return 'fail'

    if ret_str.find('-r--r--r--  1 unknown unknown        24576 2007-03-29 12:20 /vsizip//vsicurl/ftp://ftp.remotesensing.org/gdal/data/aig/nzdem.zip/nzdem/info/arc0002r.001') == -1:
        print(ret_str)
        return 'fail'

    return 'success'

gdaltest_list = [
    test_gdal_ls_py_1,
    test_gdal_ls_py_2,
    test_gdal_ls_py_3,
    test_gdal_ls_py_4,
    test_gdal_ls_py_5,
    test_gdal_ls_py_6,
    test_gdal_ls_py_7,
    test_gdal_ls_py_8,
    ]

if __name__ == '__main__':

    gdal.SetConfigOption('GDAL_RUN_SLOW_TESTS', 'YES')

    gdaltest.setup_run( 'test_gdal_ls_py' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
