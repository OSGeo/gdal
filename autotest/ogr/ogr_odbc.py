#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR ODBC driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil
from osgeo import ogr

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
# Basic testing

def ogr_odbc_1():

    ogrtest.odbc_drv = None
    if sys.platform != 'win32':
        return 'skip'

    ogrtest.odbc_drv = ogr.GetDriverByName('ODBC')
    if ogrtest.odbc_drv is None:
        return 'skip'
        
    ds = ogrtest.odbc_drv.Open('data/empty.mdb')
    if ds is None:
        ogrtest.odbc_drv = None
        return 'skip'
        
    ds = None
    
    shutil.copy('data/empty.mdb', 'tmp/odbc.mdb')
    
    # Create and fill tables
    ds = ogrtest.odbc_drv.Open('tmp/odbc.mdb')
    ds.ExecuteSQL("CREATE TABLE test (intfield INT, doublefield DOUBLE, stringfield VARCHAR)")
    ds.ExecuteSQL("INSERT INTO test (intfield, doublefield, stringfield) VALUES (1, 2.34, 'foo')")
    
    ds.ExecuteSQL("CREATE TABLE test_with_pk (OGR_FID INT PRIMARY KEY, intfield INT, doublefield DOUBLE, stringfield VARCHAR)")
    ds.ExecuteSQL("INSERT INTO test_with_pk (OGR_FID, intfield) VALUES (1, 2)")
    ds.ExecuteSQL("INSERT INTO test_with_pk (OGR_FID, intfield) VALUES (2, 3)")
    ds.ExecuteSQL("INSERT INTO test_with_pk (OGR_FID, intfield) VALUES (3, 4)")
    ds.ExecuteSQL("INSERT INTO test_with_pk (OGR_FID, intfield) VALUES (4, 5)")
    ds.ExecuteSQL("INSERT INTO test_with_pk (OGR_FID, intfield) VALUES (5, 6)")
    ds = None
    
    # Test with ODBC:user/pwd@dsn syntax
    ds = ogrtest.odbc_drv.Open('ODBC:user/pwd@DRIVER=Microsoft Access Driver (*.mdb);DBQ=tmp/odbc.mdb')
    if ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None
    
    # Test with ODBC:dsn syntax
    ds = ogrtest.odbc_drv.Open('ODBC:DRIVER=Microsoft Access Driver (*.mdb);DBQ=tmp/odbc.mdb')
    if ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None
    
    # Test with ODBC:dsn,table_list syntax
    ds = ogrtest.odbc_drv.Open('ODBC:DRIVER=Microsoft Access Driver (*.mdb);DBQ=tmp/odbc.mdb,test')
    if ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None
    
    # Reopen and check
    ds = ogrtest.odbc_drv.Open('tmp/odbc.mdb')
    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('failure')
        return 'fail'

    lyr = ds.GetLayerByName('test')
    feat = lyr.GetNextFeature()
    if feat.GetField('intfield') != 1 or feat.GetField('doublefield') != 2.34 or feat.GetField('stringfield') != 'foo':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
        
    lyr = ds.GetLayerByName('test_with_pk')
    # Test GetFeatureCount()
    if lyr.GetFeatureCount() != 5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test GetFeature()
    feat = lyr.GetFeature(4)
    if feat.GetField('intfield') != 5:
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
        
    # Test SetAttributeFilter()
    lyr.SetAttributeFilter('intfield = 6')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 5:
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
        
    # Test ExecuteSQL()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM test")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 1 or feat.GetField('doublefield') != 2.34 or feat.GetField('stringfield') != 'foo':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
        
    ds = None

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_odbc_2():
    if ogrtest.odbc_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/odbc.mdb')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def ogr_odbc_cleanup():
    if ogrtest.odbc_drv is None:
        return 'skip'
        
    try:
        os.unlink('tmp/odbc.mdb')
    except:
        pass
        
    return 'success'

gdaltest_list = [
    ogr_odbc_1, 
    ogr_odbc_2, 
    ogr_odbc_cleanup 
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_odbc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
