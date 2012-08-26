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

    import sqlite3
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', 'YES')
    conn = sqlite3.connect(':memory:')
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', None)
    success = True
    try:
        conn.execute(sql_statement)
    except:
        success = False
    conn.close()

    return success

###############################################################################
# Basic tests

def ogr_virtualogr_1():

    try:
        import sqlite3
    except:
        return 'skip'

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

    try:
        import sqlite3
    except:
        return 'skip'

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    try:
        os.unlink('tmp/ogr_virtualogr_2.db')
    except:
        pass

    import sqlite3
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', 'YES')
    conn = sqlite3.connect('tmp/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', None)
    conn.execute("PRAGMA SYNCHRONOUS = OFF")
    conn.execute("CREATE VIRTUAL TABLE foo USING VirtualOGR('data/poly.shp')")
    conn.execute("CREATE TABLE spy_table (spy_content VARCHAR)")
    conn.execute("CREATE TABLE regular_table (bar VARCHAR)")
    conn.execute("CREATE TRIGGER spy_trigger INSERT ON regular_table BEGIN " + \
                 "INSERT OR REPLACE INTO spy_table (spy_content) " + \
                 "SELECT OGR_STYLE FROM foo; END;")
    conn.close()

    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', 'YES')
    conn = sqlite3.connect('tmp/ogr_virtualogr_2.db')
    gdal.SetConfigOption('OGR_SQLITE_STATIC_VIRTUAL_OGR', None)
    conn.execute("PRAGMA SYNCHRONOUS = OFF")
    try:
        conn.execute("INSERT INTO regular_table (bar) VALUES ('bar')")
        gdaltest.post_reason('expected a failure')
        return 'fail'
    except:
        pass
    finally:
        conn.close()
        os.unlink('tmp/ogr_virtualogr_2.db')

    return 'success'

gdaltest_list = [
    ogr_virtualogr_1,
    ogr_virtualogr_2,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_virtualogr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

