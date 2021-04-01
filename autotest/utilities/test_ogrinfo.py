#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrinfo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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


from osgeo import gdal
import gdaltest
import ogrtest
import test_cli_utilities
import pytest

###############################################################################
# Simple test


def test_ogrinfo_1():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp')
    assert (err is None or err == ''), 'got error/warning'
    assert ret.find('ESRI Shapefile') != -1

###############################################################################
# Test -ro option


def test_ogrinfo_2():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro ../ogr/data/poly.shp')
    assert ret.find('ESRI Shapefile') != -1

###############################################################################
# Test -al option


def test_ogrinfo_3():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -al ../ogr/data/poly.shp')
    assert ret.find('Layer name: poly') != -1
    assert ret.find('Geometry: Polygon') != -1
    assert ret.find('Feature Count: 10') != -1
    assert ret.find('Extent: (478315') != -1
    assert ret.find('PROJCRS["OSGB') != -1
    assert ret.find('AREA: Real (') != -1

###############################################################################
# Test layer name


def test_ogrinfo_4():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly')
    assert ret.find('Feature Count: 10') != -1

###############################################################################
# Test -sql option


def test_ogrinfo_5():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp -sql "select * from poly"')
    assert ret.find('Feature Count: 10') != -1

###############################################################################
# Test -geom=NO option


def test_ogrinfo_6():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=no')
    assert ret.find('Feature Count: 10') != -1
    assert ret.find('POLYGON') == -1

###############################################################################
# Test -geom=SUMMARY option


def test_ogrinfo_7():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=summary')
    assert ret.find('Feature Count: 10') != -1
    assert ret.find('POLYGON (') == -1
    assert ret.find('POLYGON :') != -1

###############################################################################
# Test -spat option


def test_ogrinfo_8():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -spat 479609 4764629 479764 4764817')
    if ogrtest.have_geos():
        assert ret.find('Feature Count: 4') != -1
        return
    else:
        assert ret.find('Feature Count: 5') != -1
        return

###############################################################################
# Test -where option


def test_ogrinfo_9():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -where "EAS_ID=171"')
    assert ret.find('Feature Count: 1') != -1

###############################################################################
# Test -fid option


def test_ogrinfo_10():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -fid 9')
    assert ret.find('OGRFeature(poly):9') != -1

###############################################################################
# Test -fields=no option


def test_ogrinfo_11():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -fields=no')
    assert ret.find('AREA (Real') == -1
    assert ret.find('POLYGON (') != -1

###############################################################################
# Test ogrinfo --version


def test_ogrinfo_12():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --version', check_memleak=False)
    assert ret.startswith(gdal.VersionInfo('--version'))

###############################################################################
# Test erroneous use of --config


def test_ogrinfo_13():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --config', check_memleak=False)
    assert '--config option given without a key and value argument' in err

###############################################################################
# Test erroneous use of --mempreload.


def test_ogrinfo_14():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --mempreload', check_memleak=False)
    assert '--mempreload option given without directory path' in err

###############################################################################
# Test --mempreload


def test_ogrinfo_15():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (ret, _) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --debug on --mempreload ../ogr/data /vsimem/poly.shp', check_memleak=False)
    assert "ESRI Shapefile" in ret

###############################################################################
# Test erroneous use of --debug.


def test_ogrinfo_16():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --debug', check_memleak=False)
    assert '--debug option given without debug level' in err

###############################################################################
# Test erroneous use of --optfile.


def test_ogrinfo_17():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --optfile', check_memleak=False)
    assert '--optfile option given without filename' in err

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --optfile /foo/bar', check_memleak=False)
    assert 'Unable to open optfile' in err

    f = open('tmp/optfile.txt', 'wt')
    f.write('--config foo\n')
    f.close()
    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' --optfile tmp/optfile.txt', check_memleak=False)
    os.unlink('tmp/optfile.txt')
    assert '--config option given without a key and value argument' in err

###############################################################################
# Test --optfile


def test_ogrinfo_18():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    f = open('tmp/optfile.txt', 'wt')
    f.write('# comment\n')
    f.write('../ogr/data/poly.shp\n')
    f.close()
    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --optfile tmp/optfile.txt', check_memleak=False)
    os.unlink('tmp/optfile.txt')
    assert "ESRI Shapefile" in ret

###############################################################################
# Test --formats


def test_ogrinfo_19():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --formats', check_memleak=False)
    assert 'ESRI Shapefile -vector- (rw+v): ESRI Shapefile' in ret

###############################################################################
# Test --help-general


def test_ogrinfo_20():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --help-general', check_memleak=False)
    assert 'Generic GDAL utility command options' in ret

###############################################################################
# Test --locale


def test_ogrinfo_21():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' --locale C ../ogr/data/poly.shp', check_memleak=False)
    assert "ESRI Shapefile" in ret

###############################################################################
# Test RFC 41 support


def test_ogrinfo_22():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    f = open('tmp/test_ogrinfo_22.csv', 'wt')
    f.write('_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n')
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.close()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' tmp/test_ogrinfo_22.csv', check_memleak=False)
    assert '1: test_ogrinfo_22 (Unknown (any), Unknown (any))' in ret

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -al -wkt_format wkt1 tmp/test_ogrinfo_22.csv', check_memleak=False)
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
Data axis to CRS axis mapping: 2,1
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
Data axis to CRS axis mapping: 1,2
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


def test_ogrinfo_23():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    f = open('tmp/test_ogrinfo_23.csv', 'wt')
    f.write('_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n')
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.write('"POINT(3 4)","POINT(1 2)"\n')
    f.close()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -al tmp/test_ogrinfo_23.csv -wkt_format wkt1 -spat 1 2 1 2 -geomfield geom__WKTgeom2_EPSG_32631', check_memleak=False)
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
Data axis to CRS axis mapping: 2,1
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
Data axis to CRS axis mapping: 1,2
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

###############################################################################
# Test metadata


def test_ogrinfo_24():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    f = open('tmp/test_ogrinfo_24.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
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
</OGRVRTDataSource>""")
    f.close()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al tmp/test_ogrinfo_24.vrt -so', check_memleak=False)
    assert 'foo=bar' in ret
    assert 'bar=baz' in ret

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al tmp/test_ogrinfo_24.vrt -so -mdd all', check_memleak=False)
    assert 'foo=bar' in ret
    assert 'baz=foo' in ret
    assert 'bar=baz' in ret

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al tmp/test_ogrinfo_24.vrt -so -nomd', check_memleak=False)
    assert 'Metadata' not in ret

    os.unlink('tmp/test_ogrinfo_24.vrt')

###############################################################################
# Test -rl


def test_ogrinfo_25():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -rl -q ../ogr/data/poly.shp')
    assert (err is None or err == ''), 'got error/warning'
    assert 'OGRFeature(poly):0' in ret and 'OGRFeature(poly):9' in ret, \
        'wrong output'


###############################################################################
# Test SQLStatement with -sql @filename syntax


def test_ogrinfo_sql_filename():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    open('tmp/my.sql', 'wt').write("""-- initial comment\nselect\n'--''--',* from --comment\npoly\n-- trailing comment""")
    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -q ../ogr/data/poly.shp -sql @tmp/my.sql')
    os.unlink('tmp/my.sql')
    assert (err is None or err == ''), 'got error/warning'
    assert 'OGRFeature(poly):0' in ret and 'OGRFeature(poly):9' in ret, \
        'wrong output'

###############################################################################
# Test -nogeomtype


def test_ogrinfo_nogeomtype():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -nogeomtype ../ogr/data/poly.shp')
    assert (err is None or err == ''), 'got error/warning'
    expected_ret = """INFO: Open of `../ogr/data/poly.shp'
      using driver `ESRI Shapefile' successful.
1: poly
"""
    expected_lines = expected_ret.splitlines()
    lines = ret.splitlines()
    for i, exp_line in enumerate(expected_lines):
        if exp_line != lines[i]:
            if gdaltest.is_travis_branch('mingw'):
                return 'expected_fail'
            pytest.fail(ret)

###############################################################################
# Test field domains


def test_ogrinfo_fielddomains():
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()
    if gdal.GetDriverByName('GPKG') is None:
        pytest.skip('GPKG driver missing')

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -al ../ogr/data/gpkg/domains.gpkg')
    assert (err is None or err == ''), 'got error/warning'
    assert 'with_range_domain_int: Integer (0.0), domain name=range_domain_int' in ret

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -fielddomain range_domain_int ../ogr/data/gpkg/domains.gpkg')
    assert (err is None or err == ''), 'got error/warning'
    assert 'Type: range' in ret
    assert 'Field type: Integer' in ret
    assert 'Split policy: default value' in ret
    assert 'Merge policy: default value' in ret
    assert 'Minimum value: 1' in ret
    assert 'Maximum value: 2 (excluded)' in ret

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -fielddomain range_domain_int64 ../ogr/data/gpkg/domains.gpkg')
    assert (err is None or err == ''), 'got error/warning'
    assert 'Type: range' in ret
    assert 'Field type: Integer64' in ret
    assert 'Split policy: default value' in ret
    assert 'Merge policy: default value' in ret
    assert 'Minimum value: -1234567890123 (excluded)' in ret
    assert 'Maximum value: 1234567890123' in ret

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -fielddomain range_domain_real ../ogr/data/gpkg/domains.gpkg')
    assert (err is None or err == ''), 'got error/warning'
    assert 'Type: range' in ret
    assert 'Field type: Real' in ret
    assert 'Split policy: default value' in ret
    assert 'Merge policy: default value' in ret
    assert 'Minimum value: 1.5' in ret
    assert 'Maximum value: 2.5' in ret

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -fielddomain range_domain_real_inf ../ogr/data/gpkg/domains.gpkg')
    assert (err is None or err == ''), 'got error/warning'
    assert 'Type: range' in ret
    assert 'Field type: Real' in ret
    assert 'Split policy: default value' in ret
    assert 'Merge policy: default value' in ret
    assert 'Minimum value' not in ret
    assert 'Maximum value' not in ret

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -fielddomain enum_domain ../ogr/data/gpkg/domains.gpkg')
    assert (err is None or err == ''), 'got error/warning'
    assert 'Type: coded' in ret
    assert 'Field type: Integer' in ret
    assert 'Split policy: default value' in ret
    assert 'Merge policy: default value' in ret
    assert '1: one' in ret
    assert '2' in ret
    assert '2:' not in ret

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrinfo_path() + ' -fielddomain glob_domain ../ogr/data/gpkg/domains.gpkg')
    assert (err is None or err == ''), 'got error/warning'
    assert 'Type: glob' in ret
    assert 'Field type: String' in ret
    assert 'Split policy: default value' in ret
    assert 'Merge policy: default value' in ret
    assert 'Glob: *' in ret

