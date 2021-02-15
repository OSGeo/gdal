#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR ODBC driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal


import gdaltest
import ogrtest
import pytest

###############################################################################
# Basic testing


def test_ogr_odbc_1():

    ogrtest.odbc_drv = ogr.GetDriverByName('ODBC')
    if ogrtest.odbc_drv is None:
        pytest.skip()

    if sys.platform != 'win32':
        pytest.skip()

    ds = ogrtest.odbc_drv.Open('data/mdb/empty.mdb')
    if ds is None:
        ogrtest.odbc_drv = None
        pytest.skip()

    ds = None

    shutil.copy('data/mdb/empty.mdb', 'tmp/odbc.mdb')

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
    assert ds is not None
    ds = None

    # Test with ODBC:dsn syntax
    ds = ogrtest.odbc_drv.Open('ODBC:DRIVER=Microsoft Access Driver (*.mdb);DBQ=tmp/odbc.mdb')
    assert ds is not None
    ds = None

    # Test with ODBC:dsn,table_list syntax
    ds = ogrtest.odbc_drv.Open('ODBC:DRIVER=Microsoft Access Driver (*.mdb);DBQ=tmp/odbc.mdb,test')
    assert ds is not None
    assert ds.GetLayerCount() == 1
    ds = None

    # Reopen and check
    ds = ogrtest.odbc_drv.Open('tmp/odbc.mdb')
    assert ds.GetLayerCount() == 2

    lyr = ds.GetLayerByName('test')
    feat = lyr.GetNextFeature()
    if feat.GetField('intfield') != 1 or feat.GetField('doublefield') != 2.34 or feat.GetField('stringfield') != 'foo':
        feat.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('test_with_pk')
    # Test GetFeatureCount()
    assert lyr.GetFeatureCount() == 5

    # Test GetFeature()
    feat = lyr.GetFeature(4)
    if feat.GetField('intfield') != 5:
        feat.DumpReadable()
        pytest.fail()

    # Test SetAttributeFilter()
    lyr.SetAttributeFilter('intfield = 6')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 5:
        feat.DumpReadable()
        pytest.fail()

    # Test ExecuteSQL()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM test")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 1 or feat.GetField('doublefield') != 2.34 or feat.GetField('stringfield') != 'foo':
        feat.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = None

###############################################################################
# Run test_ogrsf


def test_ogr_odbc_2():
    if ogrtest.odbc_drv is None:
        pytest.skip()

    ds = ogrtest.odbc_drv.Open('data/mdb/empty.mdb')
    if ds is None:
        # likely odbc driver for mdb is not installed (or a broken old version of mdbtools is installed!)
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/odbc.mdb')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test that alternative MS Access file extensions can be read


def test_extensions():
    if ogrtest.odbc_drv is None:
        pytest.skip()

    ds = ogrtest.odbc_drv.Open('data/mdb/empty.mdb')
    if ds is None:
        # likely odbc driver for mdb is not installed (or a broken old version of mdbtools is installed!)
        pytest.skip()

    ds = ogrtest.odbc_drv.Open('data/mdb/empty.style')
    assert ds is not None
    lyr = ds.GetLayerByName('Line Symbols')
    assert lyr is not None

    if os.environ.get('GITHUB_WORKFLOW', '') != 'Windows builds':
        # can't run this on Github "Windows builds" workflow, as that has the older
        # 'Microsoft Access Driver (*.mdb)' ODBC driver only, which doesn't support accdb
        # databases
        ds = ogrtest.odbc_drv.Open('data/mdb/empty.accdb')
        assert ds is not None


###############################################################################
# Test reading mdb with null memo fields (https://github.com/OSGeo/gdal/pull/3458)


def test_null_memo():
    if ogrtest.odbc_drv is None:
        pytest.skip()

    ds = ogrtest.odbc_drv.Open('data/mdb/null_memo.mdb')
    if ds is None:
        # likely odbc driver for mdb is not installed (or a broken old version of mdbtools is installed!)
        pytest.skip()

    lyr = ds.GetLayerByName('PROP')
    expected_str = [
        [7400002, '1', 0, 0, '0101', None, 981.156, 900, None, None, None, '0', '0', '2', '0', None, None, None, None,
         '4000', None, None, None, '01', '074', 285310, 4250300, 'Κ', None],
        [7400013, '2', 0, 0, '0101', None, 391.468, 368.15, None, None, None, '0', '0', '1', '0', None, None,
         None, None, '4000', None, None, None, '01', '074', 285273.0, 4250275.0, 'Κ', None],
        [7400014, '3', 0, 0, '0101', None, 1109.932, 850.5, None, None, None, '0', '0', '2', '1', None, None,
         None, None, '4000', None, None, None, '01', '074', 285273.401, 4250229.261, 'Κ', None],
        [7400015.0, '4', 1, 0, '0201', 'Ι', None, None, None, None, 510.0, None, None, '2', None, None, None,
         None, None, None, None, None, None, '01', '074', 285273.401, 4250229.261, 'Κ', None],
        [7400016.0, '5', 1, 1, '0401', '4', None, 111.63, None, None, None, '0', '0', '2', None, 300, 1000,
         500, 1000, '4000', None, None, None, '01', '074', 285273.401, 4250229.261, 'Κ', None],
        [7400017.0, '6', 1, 2, '0401', '2', None, 111.63, None, None, None, '0', '0', '2', None, 300, 1000,
         500, 1000, '4000', None, None, None, '01', '074', 285275.0, 4250227.0, 'Κ', None],
        ]

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        attrs = [feat.GetField(n) for n in range(29)]
        for k in range(29):
            if k in (5, 22, 27):
                # skip some attributes which exhibit cross-platform variations -- they aren't relevant for this test!
                continue

            if attrs[k] != expected_str[i][k]:
                feat.DumpReadable()
                pytest.fail(str(k) + ': ' + str(attrs[k]) + ' <> ' + str(expected_str[i][k]))
        i = i + 1
        feat = lyr.GetNextFeature()


###############################################################################
# Cleanup


def test_ogr_odbc_cleanup():
    if ogrtest.odbc_drv is None:
        pytest.skip()

    gdal.Unlink('tmp/odbc.mdb')
