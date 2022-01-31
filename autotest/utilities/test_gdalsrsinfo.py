#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalsrsinfo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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


import gdaltest
import test_cli_utilities
import pytest

###############################################################################
# Simple test


def test_gdalsrsinfo_1():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalsrsinfo_path() + ' ../gcore/data/byte.tif', encoding='utf-8')
    assert (err is None or err == ''), 'got error/warning'

    assert ret.find('PROJ.4 :') != -1, ret
    assert ret.find('OGC WKT2:2018 :') != -1, ret

###############################################################################
# Test -o proj4 option


def test_gdalsrsinfo_2():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -o proj4 ../gcore/data/byte.tif')

    assert ret.strip() == "+proj=utm +zone=11 +datum=NAD27 +units=m +no_defs"

###############################################################################
# Test -o wkt1 option


def test_gdalsrsinfo_3():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' --single-line -o wkt1 ../gcore/data/byte.tif')

    assert ret.strip() == 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]'

###############################################################################
# Test -o wkt_esri option


def test_gdalsrsinfo_4():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' --single-line -o wkt_esri ../gcore/data/byte.tif')

    assert ret.strip() == 'PROJCS["NAD_1927_UTM_Zone_11N",GEOGCS["GCS_North_American_1927",DATUM["D_North_American_1927",SPHEROID["Clarke_1866",6378206.4,294.978698213898]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["False_Easting",500000.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-117.0],PARAMETER["Scale_Factor",0.9996],PARAMETER["Latitude_Of_Origin",0.0],UNIT["Meter",1.0]]'

###############################################################################
# Test -o wkt_old option


def test_gdalsrsinfo_5():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' --single-line -o wkt_noct ../gcore/data/byte.tif')

    assert ret.strip() == 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1]]'

###############################################################################
# Test -o wkt_simple option


def test_gdalsrsinfo_6():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    if gdaltest.is_travis_branch('mingw'):
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' --single-line -o wkt_simple ../gcore/data/byte.tif')
    ret = ret.replace('\r\n', '\n')

    val = """PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1]]"""

    assert ret.strip() == val

###############################################################################
# Test -o mapinfo option


def test_gdalsrsinfo_7():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -o mapinfo ../gcore/data/byte.tif')

    assert ret.strip() == """'Earth Projection 8, 62, "m", -117, 0, 0.9996, 500000, 0'"""


###############################################################################
# Test nonexistent file.

def test_gdalsrsinfo_9():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    _, err = gdaltest.runexternal_out_and_err(
        test_cli_utilities.get_gdalsrsinfo_path() + ' nonexistent_file')

    assert 'ERROR - failed to load SRS definition from nonexistent_file' in err.strip()


###############################################################################
# Test -V option - valid

def test_gdalsrsinfo_10():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]'
    if sys.platform == 'win32':
        # Win32 shell quoting oddities
        wkt = wkt.replace('"', '\"')
        ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                                   " -V -o proj4 "" + wkt + """)
    else:
        ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                                   " -V -o proj4 '" + wkt + "'")

    ret = ret
    #assert ret.find('Validate Succeeds') != -1

###############################################################################
# Test -V option - invalid


def test_gdalsrsinfo_11():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],BADAUTHORITY["EPSG","4326"]]'
    if sys.platform == 'win32':
        # Win32 shell quoting oddities
        wkt = wkt.replace('"', '\"')
        ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                                   " -V -o proj4 "" + wkt + """)
    else:
        ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                                   " -V -o proj4 '" + wkt + "'")

    if ret.find('Validate Fails') == -1:
        pytest.xfail('validation currently broken. FIXME')

###############################################################################
# Test EPSG:epsg format


def test_gdalsrsinfo_12():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' --single-line -o wkt1 EPSG:4326')

    assert ret.strip() == """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]"""


###############################################################################
# Test proj4 format

def test_gdalsrsinfo_13():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' --single-line -o wkt1 "+proj=longlat +datum=WGS84 +no_defs"')

    assert ret.strip() == """GEOGCS["unknown",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Longitude",EAST],AXIS["Latitude",NORTH]]"""

###############################################################################
# Test VSILFILE format


def test_gdalsrsinfo_14():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -o proj4 /vsizip/../gcore/data/byte.tif.zip')

    assert ret.strip() == "+proj=utm +zone=11 +datum=NAD27 +units=m +no_defs"


###############################################################################
# Test .shp format


def test_gdalsrsinfo_14bis():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -o proj4 ../ogr/data/shp/Stacks.shp')

    assert ret.strip() == "+proj=lcc +lat_0=27.8333333333333 +lon_0=-99 +lat_1=30.2833333333333 +lat_2=28.3833333333333 +x_0=600000 +y_0=3999999.9998984 +datum=NAD83 +units=us-ft +no_defs"

###############################################################################
# Test .prj format


def test_gdalsrsinfo_15():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -o proj4 ../osr/data/lcc_esri.prj')

    assert ret.strip() == "+proj=lcc +lat_0=33.75 +lon_0=-79 +lat_1=34.3333333333333 +lat_2=36.1666666666667 +x_0=609601.22 +y_0=0 +datum=NAD83 +units=m +no_defs"

###############################################################################
# Test DRIVER:file syntax (bug #4493) -  similar test should be done with OGR


def test_gdalsrsinfo_16():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    cmd = test_cli_utilities.get_gdalsrsinfo_path() +\
        ' GTIFF_RAW:../gcore/data/byte.tif'

    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd, encoding = 'UTF-8')
    except:
        pytest.fail('gdalsrsinfo execution failed')

    assert err == ''

###############################################################################
# Test -e


def test_gdalsrsinfo_17():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    # Zero match
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -e "LOCAL_CS[foo]"')

    assert 'EPSG:-1' in ret

    # One match
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -e ../osr/data/lcc_esri.prj')

    assert 'EPSG:32119' in ret

    # Two matches
    open('tmp/test_gdalsrsinfo_17.wkt', 'wt').write(
        'GEOGCS["myLKS94",DATUM["Lithuania_1994_ETRS89",SPHEROID["GRS_1980",6378137,298.257222101],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                              """ -e tmp/test_gdalsrsinfo_17.wkt""")
    assert 'EPSG:4669' in ret

###############################################################################
# Test -o all option


def test_gdalsrsinfo_all():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() +
                               ' -o all ../gcore/data/byte.tif', encoding = 'UTF-8')

    assert 'PROJ.4 :' in ret, ret
    assert 'OGC WKT1 :' in ret, ret
    assert 'OGC WKT2:2015 :' in ret, ret
    assert 'OGC WKT2:2018 :' in ret, ret
    assert 'OGC WKT1 (simple) :' in ret, ret
    assert 'OGC WKT1 (no CT) :' in ret, ret
    assert 'ESRI WKT :' in ret, ret
