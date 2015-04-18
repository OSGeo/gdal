#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrinfo testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import os

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
import ogrtest
import test_cli_utilities

###############################################################################
# Simple test

def test_ogrinfo_1():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'
    if ret.find('ESRI Shapefile') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -ro option

def test_ogrinfo_2():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro ../ogr/data/poly.shp')
    if ret.find('ESRI Shapefile') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -al option

def test_ogrinfo_3():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -al ../ogr/data/poly.shp')
    if ret.find('Layer name: poly') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ret.find('Geometry: Polygon') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ret.find('Feature Count: 10') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ret.find('Extent: (478315') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ret.find('PROJCS["OSGB') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ret.find('AREA: Real (') == -1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test layer name

def test_ogrinfo_4():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -sql option

def test_ogrinfo_5():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp -sql "select * from poly"')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -geom=NO option

def test_ogrinfo_6():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=no')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'
    if ret.find('POLYGON') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -geom=SUMMARY option

def test_ogrinfo_7():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=summary')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'
    if ret.find('POLYGON (') != -1:
        return 'fail'
    if ret.find('POLYGON :') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -spat option

def test_ogrinfo_8():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -spat 479609 4764629 479764 4764817')
    if ogrtest.have_geos():
        if ret.find('Feature Count: 4') == -1:
            return 'fail'
        return 'success'
    else:
        if ret.find('Feature Count: 5') == -1:
            return 'fail'
        return 'success'

###############################################################################
# Test -where option

def test_ogrinfo_9():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -where "EAS_ID=171"')
    if ret.find('Feature Count: 1') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -fid option

def test_ogrinfo_10():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -fid 9')
    if ret.find('OGRFeature(poly):9') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -fields=no option

def test_ogrinfo_11():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -fields=no')
    if ret.find('AREA (Real') != -1:
        return 'fail'
    if ret.find('POLYGON (') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test ogrinfo --version

def test_ogrinfo_12():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --version', check_memleak = False )
    if ret.find(gdal.VersionInfo('--version')) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --config

def test_ogrinfo_13():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --config', check_memleak = False )
    if err.find('--config option given without a key and value argument') < 0:
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --mempreload

def test_ogrinfo_14():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --mempreload', check_memleak = False )
    if err.find('--mempreload option given without directory path') < 0:
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test --mempreload

def test_ogrinfo_15():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --debug on --mempreload ../ogr/data /vsimem/poly.shp', check_memleak = False )
    if ret.find("ESRI Shapefile") < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --debug

def test_ogrinfo_16():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --debug', check_memleak = False )
    if err.find('--debug option given without debug level') < 0:
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --optfile

def test_ogrinfo_17():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --optfile', check_memleak = False )
    if err.find('--optfile option given without filename') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --optfile /foo/bar', check_memleak = False )
    if err.find('Unable to open optfile') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test --optfile

def test_ogrinfo_18():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    f = open('tmp/optfile.txt', 'wt')
    f.write('# comment\n')
    f.write('../ogr/data/poly.shp\n')
    f.close()
    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --optfile tmp/optfile.txt', check_memleak = False )
    os.unlink('tmp/optfile.txt')
    if ret.find("ESRI Shapefile") < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test --formats

def test_ogrinfo_19():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --formats', check_memleak = False )
    if ret.find('ESRI Shapefile -vector- (rw+v): ESRI Shapefile') < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test --help-general

def test_ogrinfo_20():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --help-general', check_memleak = False )
    if ret.find('Generic GDAL utility command options') < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test --locale

def test_ogrinfo_21():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --locale C ../ogr/data/poly.shp', check_memleak = False )
    if ret.find("ESRI Shapefile") < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test RFC 41 support

def test_ogrinfo_22():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    f = open('tmp/test_ogrinfo_22.csv', 'wt')
    f.write('_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n')
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.close()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' tmp/test_ogrinfo_22.csv', check_memleak = False )
    if ret.find('1: test_ogrinfo_22 (Unknown (any), Unknown (any))') < 0:
        print(ret)
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -al tmp/test_ogrinfo_22.csv', check_memleak = False )
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
    for i in range(len(expected_lines)):
        if expected_lines[i] != lines[i]:
            print(ret)
            return 'fail'

    os.unlink('tmp/test_ogrinfo_22.csv')

    return 'success'

###############################################################################
# Test -geomfield (RFC 41) support

def test_ogrinfo_23():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    f = open('tmp/test_ogrinfo_23.csv', 'wt')
    f.write('_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n')
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.write('"POINT(3 4)","POINT(1 2)"\n')
    f.close()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -al tmp/test_ogrinfo_23.csv -spat 1 2 1 2 -geomfield geom__WKTgeom2_EPSG_32631', check_memleak = False )
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
    for i in range(len(expected_lines)):
        if expected_lines[i] != lines[i]:
            print(ret)
            return 'fail'

    os.unlink('tmp/test_ogrinfo_23.csv')

    return 'success'

###############################################################################
# Test metadata

def test_ogrinfo_24():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    f = open('tmp/test_ogrinfo_24.vrt', 'wt')
    f.write( """<OGRVRTDataSource>
    <Metadata>
        <MDI key="foo">bar</MDI>
    </Metadata>
    <Metadata domain="other_domain">
        <MDI key="baz">foo</MDI>
    </Metadata>
    <OGRVRTLayer name="poly">
        <Metadata>
            <MDI key="bar">baz</MDI>
        </Metadata>
        <SrcDataSource relativeToVRT="1" shared="1">../../ogr/data/poly.shp</SrcDataSource>
        <SrcLayer>poly</SrcLayer>
  </OGRVRTLayer>
</OGRVRTDataSource>""" )
    f.close()
   

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al tmp/test_ogrinfo_24.vrt -so', check_memleak = False )
    expected_ret = """INFO: Open of `tmp/test_ogrinfo_24.vrt'
      using driver `OGR_VRT' successful.
Metadata:
  foo=bar

Layer name: poly
Metadata:
  bar=baz
Geometry: Polygon
Feature Count: 10
Extent: (478315.531250, 4762880.500000) - (481645.312500, 4765610.500000)
Layer SRS WKT:
PROJCS["OSGB 1936 / British National Grid",
    GEOGCS["OSGB 1936",
        DATUM["OSGB_1936",
            SPHEROID["Airy_1830",6377563.396,299.3249646]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",49],
    PARAMETER["central_meridian",-2],
    PARAMETER["scale_factor",0.9996012717],
    PARAMETER["false_easting",400000],
    PARAMETER["false_northing",-100000],
    UNIT["Meter",1]]
AREA: Real (12.3)
EAS_ID: Integer64 (11.0)
PRFEDEA: String (16.0)
"""
    expected_lines = expected_ret.splitlines()
    lines = ret.splitlines()
    for i in range(len(expected_lines)):
        if expected_lines[i] != lines[i]:
            print(ret)
            return 'fail'
    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al tmp/test_ogrinfo_24.vrt -so -mdd all', check_memleak = False )
    expected_ret = """INFO: Open of `tmp/test_ogrinfo_24.vrt'
      using driver `OGR_VRT' successful.
Metadata:
  foo=bar
Metadata (other_domain):
  baz=foo

Layer name: poly
Metadata:
  bar=baz
Geometry: Polygon
Feature Count: 10
Extent: (478315.531250, 4762880.500000) - (481645.312500, 4765610.500000)
Layer SRS WKT:
PROJCS["OSGB 1936 / British National Grid",
    GEOGCS["OSGB 1936",
        DATUM["OSGB_1936",
            SPHEROID["Airy_1830",6377563.396,299.3249646]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",49],
    PARAMETER["central_meridian",-2],
    PARAMETER["scale_factor",0.9996012717],
    PARAMETER["false_easting",400000],
    PARAMETER["false_northing",-100000],
    UNIT["Meter",1]]
AREA: Real (12.3)
EAS_ID: Integer64 (11.0)
PRFEDEA: String (16.0)
"""
    expected_lines = expected_ret.splitlines()
    lines = ret.splitlines()
    for i in range(len(expected_lines)):
        if expected_lines[i] != lines[i]:
            print(ret)
            return 'fail'
            
    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al tmp/test_ogrinfo_24.vrt -so -nomd', check_memleak = False )
    expected_ret = """INFO: Open of `tmp/test_ogrinfo_24.vrt'
      using driver `OGR_VRT' successful.

Layer name: poly
Geometry: Polygon
Feature Count: 10
Extent: (478315.531250, 4762880.500000) - (481645.312500, 4765610.500000)
Layer SRS WKT:
PROJCS["OSGB 1936 / British National Grid",
    GEOGCS["OSGB 1936",
        DATUM["OSGB_1936",
            SPHEROID["Airy_1830",6377563.396,299.3249646]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",49],
    PARAMETER["central_meridian",-2],
    PARAMETER["scale_factor",0.9996012717],
    PARAMETER["false_easting",400000],
    PARAMETER["false_northing",-100000],
    UNIT["Meter",1]]
AREA: Real (12.3)
EAS_ID: Integer64 (11.0)
PRFEDEA: String (16.0)
"""
    expected_lines = expected_ret.splitlines()
    lines = ret.splitlines()
    for i in range(len(expected_lines)):
        if expected_lines[i] != lines[i]:
            print(ret)
            return 'fail'

    os.unlink('tmp/test_ogrinfo_24.vrt')

    return 'success'

gdaltest_list = [
    test_ogrinfo_1,
    test_ogrinfo_2,
    test_ogrinfo_3,
    test_ogrinfo_4,
    test_ogrinfo_5,
    test_ogrinfo_6,
    test_ogrinfo_7,
    test_ogrinfo_8,
    test_ogrinfo_9,
    test_ogrinfo_10,
    test_ogrinfo_11,
    test_ogrinfo_12,
    test_ogrinfo_13,
    test_ogrinfo_14,
    test_ogrinfo_15,
    test_ogrinfo_16,
    test_ogrinfo_17,
    test_ogrinfo_18,
    test_ogrinfo_19,
    test_ogrinfo_20,
    test_ogrinfo_21,
    test_ogrinfo_22,
    test_ogrinfo_23,
    test_ogrinfo_24,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_ogrinfo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





