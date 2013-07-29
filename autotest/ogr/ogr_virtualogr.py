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
import ogrtest

###############################################################################
def ogr_virtualogr_run_sql(sql_statement):

    ds = ogr.GetDriverByName('SQLite').CreateDataSource(':memory:')
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL(sql_statement)
    gdal.PopErrorHandler()
    success = gdal.GetLastErrorMsg() == ''
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    if not success:
        return success

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL(sql_statement, dialect = 'SQLITE')
    gdal.PopErrorHandler()
    success = gdal.GetLastErrorMsg() == ''
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    
    return success

###############################################################################
# Basic tests

def ogr_virtualogr_1():

    ogrtest.has_sqlite_dialect = False
    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    sql_lyr = ds.ExecuteSQL( "SELECT * FROM sqlite_master", dialect = 'SQLite' )
    ds.ReleaseResultSet( sql_lyr )
    if sql_lyr is None:
        return 'skip'
    ogrtest.has_sqlite_dialect = True

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

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_2.db')
    ds.ExecuteSQL("CREATE VIRTUAL TABLE foo USING VirtualOGR('data/poly.shp')")
    ds.ExecuteSQL("CREATE TABLE spy_table (spy_content VARCHAR)")
    ds.ExecuteSQL("CREATE TABLE regular_table (bar VARCHAR)")
    ds = None

    # Check that foo isn't listed
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    for i in range(ds.GetLayerCount()):
        if ds.GetLayer(i).GetName() == 'foo':
            gdaltest.post_reason('fail')
            return 'fail'
    ds = None

    # Check that it is listed if OGR_SQLITE_LIST_VIRTUAL_OGR=YES
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', 'YES')
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', None)
    found = False
    for i in range(ds.GetLayerCount()):
        if ds.GetLayer(i).GetName() == 'foo':
            found = True
    if not found:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # Add suspicious trigger
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db', update = 1)
    ds.ExecuteSQL("CREATE TRIGGER spy_trigger INSERT ON regular_table BEGIN " + \
                  "INSERT OR REPLACE INTO spy_table (spy_content) " + \
                  "SELECT OGR_STYLE FROM foo; END;")
    ds = None

    gdal.ErrorReset()
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    for i in range(ds.GetLayerCount()):
        if ds.GetLayer(i).GetName() == 'foo':
            gdaltest.post_reason('fail')
            return 'fail'
    # An error will be triggered at the time the trigger is used
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.ExecuteSQL("INSERT INTO regular_table (bar) VALUES ('bar')")
    gdal.PopErrorHandler()
    did_not_get_error = gdal.GetLastErrorMsg() == ''
    ds = None

    if did_not_get_error:
        gdal.Unlink('/vsimem/ogr_virtualogr_2.db')
        gdaltest.post_reason('expected a failure')
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', 'YES')
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', None)
    gdal.PopErrorHandler()
    if ds is not None:
        ds = None
        gdal.Unlink('/vsimem/ogr_virtualogr_2.db')
        gdaltest.post_reason('expected a failure')
        return 'fail'
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

    if not ogrtest.has_sqlite_dialect:
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

###############################################################################
# Test ogr_datasource_load_layers()

def ogr_virtualogr_4():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_4.db')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp')")
    ds.ReleaseResultSet(sql_lyr)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp')")
    gdal.PopErrorHandler()
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly')
    ret = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/ogr_virtualogr_4.db')

    if ret != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_4.db')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp', 0)")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly')
    ret = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/ogr_virtualogr_4.db')

    if ret != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_4.db')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp', 0, 'prefix')")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM prefix_poly')
    ret = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/ogr_virtualogr_4.db')

    if ret != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    # Various error conditions
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_4.db')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers(0)")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('foo')")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp','a')")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp', 0, 0)")
    ds.ReleaseResultSet(sql_lyr)
    gdal.PopErrorHandler()
    ds = None
    gdal.Unlink('/vsimem/ogr_virtualogr_4.db')

    return 'success'

###############################################################################
# Test failed CREATE VIRTUAL TABLE USING VirtualOGR

def ogr_virtualogr_5():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    # Create a CSV with duplicate column name
    fp = gdal.VSIFOpenL('/vsimem/ogr_virtualogr_5.csv', 'wt')
    line = 'foo,foo\n'
    gdal.VSIFWriteL(line, 1, len(line), fp)
    line = 'bar,baz\n'
    gdal.VSIFWriteL(line, 1, len(line), fp)
    gdal.VSIFCloseL(fp)

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("CREATE VIRTUAL TABLE lyr2 USING VirtualOGR('/vsimem/ogr_virtualogr_5.csv')", dialect = 'SQLITE')
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_virtualogr_5.csv')

    return 'success'

gdaltest_list = [
    ogr_virtualogr_1,
    ogr_virtualogr_2,
    ogr_virtualogr_3,
    ogr_virtualogr_4,
    ogr_virtualogr_5,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_virtualogr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

