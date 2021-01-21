#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrinfo.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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


import ogrtest
import test_py_scripts
import pytest

###############################################################################
# Simple test


def test_ogrinfo_py_1():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp')
    assert ret.find('ESRI Shapefile') != -1

###############################################################################
# Test -ro option


def test_ogrinfo_py_2():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', '-ro '+test_py_scripts.get_data_path('ogr')+'poly.shp')
    assert ret.find('ESRI Shapefile') != -1

###############################################################################
# Test -al option


def test_ogrinfo_py_3():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', '-al '+test_py_scripts.get_data_path('ogr')+'poly.shp')
    assert ret.find('Feature Count: 10') != -1

###############################################################################
# Test layer name


def test_ogrinfo_py_4():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp poly')
    assert ret.find('Feature Count: 10') != -1

###############################################################################
# Test -sql option


def test_ogrinfo_py_5():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp -sql "select * from poly"')
    assert ret.find('Feature Count: 10') != -1

###############################################################################
# Test -geom=NO option


def test_ogrinfo_py_6():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp poly -geom=no')
    assert ret.find('Feature Count: 10') != -1
    assert ret.find('POLYGON') == -1

###############################################################################
# Test -geom=SUMMARY option


def test_ogrinfo_py_7():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp poly -geom=summary')
    assert ret.find('Feature Count: 10') != -1
    assert ret.find('POLYGON (') == -1
    assert ret.find('POLYGON :') != -1

###############################################################################
# Test -spat option


def test_ogrinfo_py_8():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp poly -spat 479609 4764629 479764 4764817')
    if ogrtest.have_geos():
        assert ret.find('Feature Count: 4') != -1
        return
    else:
        assert ret.find('Feature Count: 5') != -1
        return

###############################################################################
# Test -where option


def test_ogrinfo_py_9():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp poly -where "EAS_ID=171"')
    assert ret.find('Feature Count: 1') != -1

###############################################################################
# Test -fid option


def test_ogrinfo_py_10():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp poly -fid 9')
    assert ret.find('OGRFeature(poly):9') != -1

###############################################################################
# Test -fields=no option


def test_ogrinfo_py_11():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', test_py_scripts.get_data_path('ogr') + 'poly.shp poly -fields=no')
    assert ret.find('AREA (Real') == -1
    assert ret.find('POLYGON (') != -1

###############################################################################
# Test RFC 41 support


def test_ogrinfo_py_22():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    f = open('tmp/test_ogrinfo_22.csv', 'wt')
    f.write('_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n')
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.close()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', 'tmp/test_ogrinfo_22.csv')
    assert '1: test_ogrinfo_22 (Unknown (any), Unknown (any))' in ret

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', '-al tmp/test_ogrinfo_22.csv')
    expected_ret = """INFO: Open of `tmp/test_ogrinfo_22.csv'
      using driver `CSV' successful.

Layer name: test_ogrinfo_22
Geometry (geom__WKTgeom1_EPSG_4326): Unknown (any)
Geometry (geom__WKTgeom2_EPSG_32631): Unknown (any)
Feature Count: 1
Extent (geom__WKTgeom1_EPSG_4326): (1.000000, 2.000000) - (1.000000, 2.000000)
Extent (geom__WKTgeom2_EPSG_32631): (3.000000, 4.000000) - (3.000000, 4.000000)
SRS WKT (geom__WKTgeom1_EPSG_4326):
GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AXIS["Latitude",NORTH],
    AXIS["Longitude",EAST],
    AUTHORITY["EPSG","4326"]]
SRS WKT (geom__WKTgeom2_EPSG_32631):
PROJCS["WGS 84 / UTM zone 31N",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",3],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH],
    AUTHORITY["EPSG","32631"]]
Geometry Column 1 = geom__WKTgeom1_EPSG_4326
Geometry Column 2 = geom__WKTgeom2_EPSG_32631
_WKTgeom1_EPSG_4326: String (0.0)
_WKTgeom2_EPSG_32631: String (0.0)
OGRFeature(test_ogrinfo_22):1
  _WKTgeom1_EPSG_4326 (String) = POINT(1 2)
  _WKTgeom2_EPSG_32631 (String) = POINT(3 4)
  geom__WKTgeom1_EPSG_4326 = POINT (1 2)
  geom__WKTgeom2_EPSG_32631 = POINT (3 4)
"""
    expected_lines = expected_ret.splitlines()
    lines = ret.splitlines()
    for i, exp_line in enumerate(expected_lines):
        assert exp_line == lines[i], ret

    os.unlink('tmp/test_ogrinfo_22.csv')

###############################################################################
# Test -geomfield (RFC 41) support


def test_ogrinfo_py_23():
    script_path = test_py_scripts.get_py_script('ogrinfo')
    if script_path is None:
        pytest.skip()

    f = open('tmp/test_ogrinfo_23.csv', 'wt')
    f.write('_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n')
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.write('"POINT(3 4)","POINT(1 2)"\n')
    f.close()

    ret = test_py_scripts.run_py_script(script_path, 'ogrinfo', '-al tmp/test_ogrinfo_23.csv -spat 1 2 1 2 -geomfield geom__WKTgeom2_EPSG_32631')
    expected_ret = """INFO: Open of `tmp/test_ogrinfo_23.csv'
      using driver `CSV' successful.

Layer name: test_ogrinfo_23
Geometry (geom__WKTgeom1_EPSG_4326): Unknown (any)
Geometry (geom__WKTgeom2_EPSG_32631): Unknown (any)
Feature Count: 1
Extent (geom__WKTgeom1_EPSG_4326): (3.000000, 4.000000) - (3.000000, 4.000000)
Extent (geom__WKTgeom2_EPSG_32631): (1.000000, 2.000000) - (1.000000, 2.000000)
SRS WKT (geom__WKTgeom1_EPSG_4326):
GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AXIS["Latitude",NORTH],
    AXIS["Longitude",EAST],
    AUTHORITY["EPSG","4326"]]
SRS WKT (geom__WKTgeom2_EPSG_32631):
PROJCS["WGS 84 / UTM zone 31N",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",3],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH],
    AUTHORITY["EPSG","32631"]]
Geometry Column 1 = geom__WKTgeom1_EPSG_4326
Geometry Column 2 = geom__WKTgeom2_EPSG_32631
_WKTgeom1_EPSG_4326: String (0.0)
_WKTgeom2_EPSG_32631: String (0.0)
OGRFeature(test_ogrinfo_23):2
  _WKTgeom1_EPSG_4326 (String) = POINT(3 4)
  _WKTgeom2_EPSG_32631 (String) = POINT(1 2)
  geom__WKTgeom1_EPSG_4326 = POINT (3 4)
  geom__WKTgeom2_EPSG_32631 = POINT (1 2)
"""
    expected_lines = expected_ret.splitlines()
    lines = ret.splitlines()
    for i, exp_line in enumerate(expected_lines):
        assert exp_line == lines[i], ret

    os.unlink('tmp/test_ogrinfo_23.csv')




