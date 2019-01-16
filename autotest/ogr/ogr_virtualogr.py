#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  VirtualOGR testing
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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


from osgeo import ogr
from osgeo import gdal
import gdaltest
import pytest

# Linter says this isn't used, but it actually is via pytest magic :)
from ogr.ogr_sql_sqlite import require_ogr_sql_sqlite  # noqa

###############################################################################


pytestmark = pytest.mark.usefixtures('require_ogr_sql_sqlite')


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
    sql_lyr = ds.ExecuteSQL(sql_statement, dialect='SQLITE')
    gdal.PopErrorHandler()
    success = gdal.GetLastErrorMsg() == ''
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    return success

###############################################################################
# Basic tests


def test_ogr_virtualogr_1():
    # Invalid syntax
    assert not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR()")

    # Nonexistent dataset
    assert not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('foo')")

    # Dataset with 0 layer
    assert not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('<OGRVRTDataSource></OGRVRTDataSource>')")

    # Dataset with more than 1 layer
    assert not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data')")

    assert ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp')")

    assert ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0)")

    assert ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 1)")

    # Invalid value for update_mode
    assert not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 'foo')")

    # Nonexistent layer
    assert not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'foo')")

    assert ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly')")

    assert ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly', 0)")

    assert ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly', 1)")

    assert ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly', 1, 1)")

    # Too many arguments
    assert not ogr_virtualogr_run_sql("CREATE VIRTUAL TABLE poly USING VirtualOGR('data/poly.shp', 0, 'poly', 1, 1, bla)")

###############################################################################
# Test detection of suspicious use of VirtualOGR


def test_ogr_virtualogr_2():
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_2.db')
    ds.ExecuteSQL("CREATE VIRTUAL TABLE foo USING VirtualOGR('data/poly.shp')")
    ds.ExecuteSQL("CREATE TABLE spy_table (spy_content VARCHAR)")
    ds.ExecuteSQL("CREATE TABLE regular_table (bar VARCHAR)")
    ds = None

    # Check that foo isn't listed
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() != 'foo'
    ds = None

    # Check that it is listed if OGR_SQLITE_LIST_VIRTUAL_OGR=YES
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', 'YES')
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', None)
    found = False
    for i in range(ds.GetLayerCount()):
        if ds.GetLayer(i).GetName() == 'foo':
            found = True
    assert found
    ds = None

    # Add suspicious trigger
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db', update=1)
    ds.ExecuteSQL("CREATE TRIGGER spy_trigger INSERT ON regular_table BEGIN " +
                  "INSERT OR REPLACE INTO spy_table (spy_content) " +
                  "SELECT OGR_STYLE FROM foo; END;")
    ds = None

    gdal.ErrorReset()
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() != 'foo'
    # An error will be triggered at the time the trigger is used
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.ExecuteSQL("INSERT INTO regular_table (bar) VALUES ('bar')")
    gdal.PopErrorHandler()
    did_not_get_error = gdal.GetLastErrorMsg() == ''
    ds = None

    if did_not_get_error:
        gdal.Unlink('/vsimem/ogr_virtualogr_2.db')
        pytest.fail('expected a failure')

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', 'YES')
    ds = ogr.Open('/vsimem/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_LIST_VIRTUAL_OGR', None)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        ds = None
        gdal.Unlink('/vsimem/ogr_virtualogr_2.db')
        pytest.fail('expected an error message')
    did_not_get_error = gdal.GetLastErrorMsg() == ''
    ds = None

    gdal.Unlink('/vsimem/ogr_virtualogr_2.db')

    assert not did_not_get_error, 'expected a failure'

###############################################################################
# Test GDAL as a SQLite3 dynamically loaded extension


def test_ogr_virtualogr_3():
    # Find path of libgdal
    libgdal_name = gdaltest.find_lib('gdal')
    if libgdal_name is None:
        pytest.skip()
    print('Found ' + libgdal_name)

    # Find path of libsqlite3 or libspatialite
    libsqlite_name = gdaltest.find_lib('sqlite3')
    if libsqlite_name is None:
        libsqlite_name = gdaltest.find_lib('spatialite')
    if libsqlite_name is None:
        pytest.skip()
    print('Found ' + libsqlite_name)

    python_exe = sys.executable
    if sys.platform == 'win32':
        python_exe = python_exe.replace('\\', '/')
        libgdal_name = libgdal_name.replace('\\', '/')
        libsqlite_name = libsqlite_name.replace('\\', '/')

    ret = gdaltest.runexternal(python_exe + ' ogr_as_sqlite_extension.py "%s" "%s"' % (libsqlite_name, libgdal_name), check_memleak=False)

    if ret.startswith('skip'):
        pytest.skip()
    assert gdal.VersionInfo('RELEASE_NAME') in ret

###############################################################################
# Test ogr_datasource_load_layers()


def test_ogr_virtualogr_4():
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

    assert ret == 10

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_4.db')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp', 0)")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly')
    ret = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/ogr_virtualogr_4.db')

    assert ret == 10

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_virtualogr_4.db')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_datasource_load_layers('data/poly.shp', 0, 'prefix')")
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM prefix_poly')
    ret = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/ogr_virtualogr_4.db')

    assert ret == 10

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

###############################################################################
# Test failed CREATE VIRTUAL TABLE USING VirtualOGR


def test_ogr_virtualogr_5():

    # Create a CSV with duplicate column name
    fp = gdal.VSIFOpenL('/vsimem/ogr_virtualogr_5.csv', 'wt')
    line = 'foo,foo\n'
    gdal.VSIFWriteL(line, 1, len(line), fp)
    line = 'bar,baz\n'
    gdal.VSIFWriteL(line, 1, len(line), fp)
    gdal.VSIFCloseL(fp)

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("CREATE VIRTUAL TABLE lyr2 USING VirtualOGR('/vsimem/ogr_virtualogr_5.csv')", dialect='SQLITE')
    gdal.PopErrorHandler()
    assert sql_lyr is None
    ds = None

    gdal.Unlink('/vsimem/ogr_virtualogr_5.csv')



