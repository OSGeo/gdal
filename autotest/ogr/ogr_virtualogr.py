#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  VirtualOGR testing
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

from osgeo import ogr
from osgeo import gdal
import gdaltest

###############################################################################
def ogr_virtualogr_run_sql(sql_statement):

    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', 'YES')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource(':memory:')
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', None)
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL(sql_statement)
    gdal.PopErrorHandler()
    success = gdal.GetLastErrorMsg() == ''
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    return success

###############################################################################
# Basic tests

def ogr_virtualogr_1():

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    # so that OGR2SQLITE_static_register() is called
    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master", dialect = 'SQLITE')
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    # Invalid syntax
    if ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR()"):
        gdaltest.post_reason('failed')
        return 'fail'

    # Unexisting dataset
    if ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('foo')"):
        gdaltest.post_reason('failed')
        return 'fail'

    # Dataset with 0 layer
    if ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('<OGRVRTDataSource></OGRVRTDataSource>')"):
        gdaltest.post_reason('failed')
        return 'fail'

    # Dataset with more than 1 layer
    if ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data')"):
        gdaltest.post_reason('failed')
        return 'fail'

    if not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp')"):
        gdaltest.post_reason('failed')
        return 'fail'

    if not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0)"):
        gdaltest.post_reason('failed')
        return 'fail'

    if not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 1)"):
        gdaltest.post_reason('failed')
        return 'fail'

    # Invalid value for update_mode
    if ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 'foo')"):
        gdaltest.post_reason('failed')
        return 'fail'

    # Unexisting layer
    if ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'foo')"):
        gdaltest.post_reason('failed')
        return 'fail'

    if not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly')"):
        gdaltest.post_reason('failed')
        return 'fail'

    if not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly', 0)"):
        gdaltest.post_reason('failed')
        return 'fail'

    if not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly', 1)"):
        gdaltest.post_reason('failed')
        return 'fail'

    # Too many arguments
    if ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly', 1, bla)"):
        gdaltest.post_reason('failed')
        return 'fail'

    return 'success'

###############################################################################
# Test detection of suspicious use of VirtualOGR

def ogr_virtualogr_2():

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', 'YES')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', None)
    ds.ExecuteSQL("CREATE VIRTUAL TABLE foo USING VirtualOGR('data/poly.shp')")
    ds.ExecuteSQL("CREATE TABLE spy_table (spy_content VARCHAR)")
    ds.ExecuteSQL("CREATE TABLE regular_table (bar VARCHAR)")
    ds.ExecuteSQL("CREATE TRIGGER spy_trigger INSERT ON regular_table BEGIN " + \
                  "INSERT OR REPLACE INTO spy_table (spy_content) " + \
                  "SELECT OGR_STYLE FROM foo; END;")
    ds = None

    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', 'YES')
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', None)
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.ExecuteSQL("INSERT INTO regular_table (bar) VALUES ('bar')")
    gdal.PopErrorHandler()
    did_not_get_error = gdal.GetLastErrorMsg() == ''
    ds = None
    gdal.Unlink('/vsimem/ogr_virtualogr_2.db')

    if did_not_get_error:
        gdaltest.post_reason('expected a failure')
        return 'fail'

    return 'success'

###############################################################################
# Test GDAL as a SQLite3 dynamically loaded extension

def ogr_virtualogr_3():

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    # Find path of libgdal
    libgdal_name = gdaltest.find_lib('gdal')
    if libgdal_name is None:
        return 'skip'
    print('Found '+ libgdal_name)

    # Find path of libsqlite3 or libspatialite
    libsqlite_name = gdaltest.find_lib('sqlite3')
    if libsqlite_name is None:
        libsqlite_name = gdaltest.find_lib('spatialite')
    if libsqlite_name is None:
        return 'skip'
    print('Found ' + libsqlite_name)

    python_exe = sys.executable
    if sys.platform == 'win32':
        python_exe = python_exe.replace('\\', '/')
        libgdal_name = libgdal_name.replace('\\', '/')
        libsqlite_name = libsqlite_name.replace('\\', '/')

    ret = gdaltest.runexternal(python_exe + ' ogr_as_sqlite_extension.py "%s" "%s"' % (libsqlite_name, libgdal_name), check_memleak = False)

    if ret.find('skip') == 0:
        return 'skip'
    if ret.find(gdal.VersionInfo('RELEASE_NAME')) < 0:
        gdaltest.post_reason('fail : %s' % ret)
        return 'fail'

    return 'success'


gdaltest_list = [
    ogr_virtualogr_1,
    ogr_virtualogr_2,
    ogr_virtualogr_3,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_virtualogr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

