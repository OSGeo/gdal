#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrinfo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import pathlib
import stat

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_ogrinfo_path() is None, reason="ogrinfo not available"
)


@pytest.fixture()
def ogrinfo_path():
    return test_cli_utilities.get_ogrinfo_path()


###############################################################################
# Simple test


def test_ogrinfo_1(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(ogrinfo_path + " ../ogr/data/poly.shp")
    assert err is None or err == "", "got error/warning"
    assert ret.find("ESRI Shapefile") != -1


###############################################################################
# Missing filename


def test_ogrinfo_missing_filename(ogrinfo_path):

    _, err = gdaltest.runexternal_out_and_err(ogrinfo_path)
    assert "filename: 1 argument(s) expected. 0 provided" in err


###############################################################################
# Test -ro option


def test_ogrinfo_2(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " -ro ../ogr/data/poly.shp")
    assert ret.find("ESRI Shapefile") != -1


###############################################################################
# Test -al option


def test_ogrinfo_3(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " -al ../ogr/data/poly.shp")
    assert ret.find("Layer name: poly") != -1
    assert ret.find("Geometry: Polygon") != -1
    assert ret.find("Feature Count: 10") != -1
    assert ret.find("Extent: (478315") != -1
    assert ret.find('PROJCRS["OSGB') != -1
    assert ret.find("AREA: Real (") != -1


###############################################################################
# Test layer name


def test_ogrinfo_4(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " ../ogr/data/poly.shp poly")
    assert ret.find("Feature Count: 10") != -1


###############################################################################
# Test -sql option


def test_ogrinfo_5(ogrinfo_path):

    ret = gdaltest.runexternal(
        ogrinfo_path + ' ../ogr/data/poly.shp -sql "select * from poly"'
    )
    assert ret.find("Feature Count: 10") != -1


###############################################################################
# Test -geom=NO option


def test_ogrinfo_6(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " ../ogr/data/poly.shp poly -geom=no")
    assert ret.find("Feature Count: 10") != -1
    assert ret.find("POLYGON") == -1


###############################################################################
# Test -geom=SUMMARY option


def test_ogrinfo_7(ogrinfo_path):

    ret = gdaltest.runexternal(
        ogrinfo_path + " ../ogr/data/poly.shp poly -geom=summary"
    )
    assert ret.find("Feature Count: 10") != -1
    assert ret.find("POLYGON (") == -1
    assert ret.find("POLYGON :") != -1


###############################################################################
# Test -spat option


def test_ogrinfo_8(ogrinfo_path):

    ret = gdaltest.runexternal(
        ogrinfo_path + " ../ogr/data/poly.shp poly -spat 479609 4764629 479764 4764817"
    )
    if ogrtest.have_geos():
        assert ret.find("Feature Count: 4") != -1
        return
    else:
        assert ret.find("Feature Count: 5") != -1
        return


###############################################################################
# Test -where option


def test_ogrinfo_9(ogrinfo_path):

    ret = gdaltest.runexternal(
        ogrinfo_path + ' ../ogr/data/poly.shp poly -where "EAS_ID=171"'
    )
    assert ret.find("Feature Count: 1") != -1


###############################################################################
# Test -fid option


def test_ogrinfo_10(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " ../ogr/data/poly.shp poly -fid 9")
    assert ret.find("OGRFeature(poly):9") != -1


###############################################################################
# Test -fields=no option


def test_ogrinfo_11(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " ../ogr/data/poly.shp poly -fields=no")
    assert ret.find("AREA (Real") == -1
    assert ret.find("POLYGON (") != -1


###############################################################################
# Test ogrinfo --version


def test_ogrinfo_12(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " --version", check_memleak=False)
    assert ret.startswith(gdal.VersionInfo("--version"))


###############################################################################
# Test erroneous use of --config


def test_ogrinfo_erroneous_config(ogrinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " --config", check_memleak=False
    )
    assert "--config option given without" in err


###############################################################################
# Test erroneous use of --config


def test_ogrinfo_erroneous_config_2(ogrinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " --config foo", check_memleak=False
    )
    assert "--config option given without" in err


###############################################################################
# Test erroneous use of --mempreload.


def test_ogrinfo_14(ogrinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " --mempreload", check_memleak=False
    )
    assert "--mempreload option given without directory path" in err


###############################################################################
# Test --mempreload


def test_ogrinfo_15(ogrinfo_path):

    ret, _ = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " --debug on --mempreload ../ogr/data /vsimem/poly.shp",
        check_memleak=False,
    )
    assert "ESRI Shapefile" in ret


###############################################################################
# Test erroneous use of --debug.


def test_ogrinfo_16(ogrinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " --debug", check_memleak=False
    )
    assert "--debug option given without debug level" in err


###############################################################################
# Test erroneous use of --optfile.


def test_ogrinfo_17(ogrinfo_path, tmp_path):

    optfile_txt = str(tmp_path / "optfile.txt")

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " --optfile", check_memleak=False
    )
    assert "--optfile option given without filename" in err

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " --optfile /foo/bar",
        check_memleak=False,
    )
    assert "Unable to open optfile" in err

    f = open(optfile_txt, "wt")
    f.write("--config foo\n")
    f.close()
    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + f" --optfile {optfile_txt}",
        check_memleak=False,
    )
    assert "--config option given without a key and value argument" in err


###############################################################################
# Test --optfile


def test_ogrinfo_18(ogrinfo_path, tmp_path):

    optfile_txt = str(tmp_path / "optfile.txt")

    f = open(optfile_txt, "wt")
    f.write("# comment\n")
    f.write("../ogr/data/poly.shp\n")
    f.close()
    ret = gdaltest.runexternal(
        ogrinfo_path + f" --optfile {optfile_txt}",
        check_memleak=False,
    )
    assert "ESRI Shapefile" in ret


###############################################################################
# Test --formats


def test_ogrinfo_19(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " --formats", check_memleak=False)
    assert "ESRI Shapefile -vector- (rw+uv): ESRI Shapefile" in ret


###############################################################################
# Test --formats -json


@pytest.mark.require_driver("ESRI Shapefile")
def test_ogrinfo_formats_json(ogrinfo_path):

    ret = json.loads(
        gdaltest.runexternal(ogrinfo_path + " --formats -json", check_memleak=False)
    )
    assert {
        "short_name": "ESRI Shapefile",
        "long_name": "ESRI Shapefile",
        "scopes": ["vector"],
        "capabilities": ["open", "create", "update", "virtual_io"],
        "file_extensions": ["shp", "dbf", "shz", "shp.zip"],
    } in ret


###############################################################################
# Test --help-general


def test_ogrinfo_20(ogrinfo_path):

    ret = gdaltest.runexternal(ogrinfo_path + " --help-general", check_memleak=False)
    assert "Generic GDAL utility command options" in ret


###############################################################################
# Test --locale


def test_ogrinfo_21(ogrinfo_path):

    ret = gdaltest.runexternal(
        ogrinfo_path + " --locale C ../ogr/data/poly.shp",
        check_memleak=False,
    )
    assert "ESRI Shapefile" in ret


###############################################################################
# Test RFC 41 support


@pytest.mark.require_driver("CSV")
def test_ogrinfo_22(ogrinfo_path, tmp_path):

    csv_fname = str(tmp_path / "test_ogrinfo_22.csv")

    f = open(csv_fname, "wt")
    f.write("_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n")
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.close()

    ret = gdaltest.runexternal(
        ogrinfo_path + f" {csv_fname}",
        check_memleak=False,
    )
    assert "1: test_ogrinfo_22 (Unknown (any), Unknown (any))" in ret

    ret = gdaltest.runexternal(
        ogrinfo_path + f" -al -wkt_format wkt1 {csv_fname}",
        check_memleak=False,
    )
    expected_ret = f"""INFO: Open of `{csv_fname}'
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


###############################################################################
# Test -geomfield (RFC 41) support


@pytest.mark.require_driver("CSV")
def test_ogrinfo_23(ogrinfo_path, tmp_path):

    csv_fname = str(tmp_path / "test_ogrinfo_23.csv")

    f = open(csv_fname, "wt")
    f.write("_WKTgeom1_EPSG_4326,_WKTgeom2_EPSG_32631\n")
    f.write('"POINT(1 2)","POINT(3 4)"\n')
    f.write('"POINT(3 4)","POINT(1 2)"\n')
    f.close()

    ret = gdaltest.runexternal(
        ogrinfo_path
        + f" -al {csv_fname} -wkt_format wkt1 -spat 1 2 1 2 -geomfield geom__WKTgeom2_EPSG_32631",
        check_memleak=False,
    )
    expected_ret = f"""INFO: Open of `{csv_fname}'
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


###############################################################################
# Test metadata


@pytest.mark.require_driver("OGR_VRT")
def test_ogrinfo_24(ogrinfo_path, tmp_path):

    vrt_fname = str(tmp_path / "test_ogrinfo_24.vrt")
    poly_shp = str(pathlib.Path(__file__).parent.parent / "ogr" / "data" / "poly.shp")

    f = open(vrt_fname, "wt")
    f.write(f"""<OGRVRTDataSource>
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
        <SrcDataSource relativeToVRT="0" shared="1">{poly_shp}</SrcDataSource>
        <SrcLayer>poly</SrcLayer>
  </OGRVRTLayer>
</OGRVRTDataSource>""")
    f.close()

    ret = gdaltest.runexternal(
        ogrinfo_path + f" -ro -al {vrt_fname} -so",
        check_memleak=False,
    )
    assert "foo=bar" in ret
    assert "bar=baz" in ret

    ret = gdaltest.runexternal(
        ogrinfo_path + f" -ro -al {vrt_fname} -so -mdd all",
        check_memleak=False,
    )
    assert "foo=bar" in ret
    assert "baz=foo" in ret
    assert "bar=baz" in ret

    ret = gdaltest.runexternal(
        ogrinfo_path + f" -ro -al {vrt_fname} -so -nomd",
        check_memleak=False,
    )
    assert "Metadata" not in ret


###############################################################################
# Test -rl


def test_ogrinfo_25(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -rl -q ../ogr/data/poly.shp"
    )
    assert err is None or err == "", "got error/warning"
    assert "OGRFeature(poly):0" in ret and "OGRFeature(poly):9" in ret, "wrong output"


###############################################################################
# Test SQLStatement with -sql @filename syntax


def test_ogrinfo_sql_filename(ogrinfo_path, tmp_path):

    sql_fname = str(tmp_path / "my.sql")

    open(sql_fname, "wt").write(
        """-- initial comment\nselect\n'--''--',* from --comment\npoly\n-- trailing comment"""
    )
    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + f" -q ../ogr/data/poly.shp -sql @{sql_fname}"
    )
    assert err is None or err == "", "got error/warning"
    assert "OGRFeature(poly):0" in ret and "OGRFeature(poly):9" in ret, "wrong output"


###############################################################################
# Test -nogeomtype


def test_ogrinfo_nogeomtype(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -nogeomtype ../ogr/data/poly.shp"
    )
    assert err is None or err == "", "got error/warning"
    expected_ret = """INFO: Open of `../ogr/data/poly.shp'
      using driver `ESRI Shapefile' successful.
1: poly
"""
    expected_lines = expected_ret.splitlines()
    lines = ret.splitlines()
    for i, exp_line in enumerate(expected_lines):
        if exp_line != lines[i]:
            if gdaltest.is_travis_branch("mingw"):
                return "expected_fail"
            pytest.fail(ret)


###############################################################################
# Test field domains


@pytest.mark.require_driver("GPKG")
def test_ogrinfo_fielddomains(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -al ../ogr/data/gpkg/domains.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert "with_range_domain_int: Integer (0.0), domain name=range_domain_int" in ret

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -fielddomain range_domain_int ../ogr/data/gpkg/domains.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert "Type: range" in ret
    assert "Field type: Integer" in ret
    assert "Split policy: default value" in ret
    assert "Merge policy: default value" in ret
    assert "Minimum value: 1" in ret
    assert "Maximum value: 2 (excluded)" in ret

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -fielddomain range_domain_int64 ../ogr/data/gpkg/domains.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert "Type: range" in ret
    assert "Field type: Integer64" in ret
    assert "Split policy: default value" in ret
    assert "Merge policy: default value" in ret
    assert "Minimum value: -1234567890123 (excluded)" in ret
    assert "Maximum value: 1234567890123" in ret

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -fielddomain range_domain_real ../ogr/data/gpkg/domains.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert "Type: range" in ret
    assert "Field type: Real" in ret
    assert "Split policy: default value" in ret
    assert "Merge policy: default value" in ret
    assert "Minimum value: 1.5" in ret
    assert "Maximum value: 2.5" in ret

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path
        + " -fielddomain range_domain_real_inf ../ogr/data/gpkg/domains.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert "Type: range" in ret
    assert "Field type: Real" in ret
    assert "Split policy: default value" in ret
    assert "Merge policy: default value" in ret
    assert "Minimum value" not in ret
    assert "Maximum value" not in ret

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -fielddomain enum_domain ../ogr/data/gpkg/domains.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert "Type: coded" in ret
    assert "Field type: Integer" in ret
    assert "Split policy: default value" in ret
    assert "Merge policy: default value" in ret
    assert "1: one" in ret
    assert "2" in ret
    assert "2:" not in ret

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -fielddomain glob_domain ../ogr/data/gpkg/domains.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert "Type: glob" in ret
    assert "Field type: String" in ret
    assert "Split policy: default value" in ret
    assert "Merge policy: default value" in ret
    assert "Glob: *" in ret


###############################################################################
# Test hiearchical presentation of layers


@pytest.mark.require_driver("OpenFileGDB")
def test_ogrinfo_hiearchical(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " ../ogr/data/filegdb/featuredataset.gdb"
    )
    assert err is None or err == "", "got error/warning"
    assert "Layer: standalone (Point)" in ret
    assert "Group fd1:" in ret
    assert "  Layer: fd1_lyr1 (Point)" in ret
    assert "  Layer: fd1_lyr2 (Point)" in ret
    assert "Group fd2:" in ret
    assert "  Layer: fd2_lyr (Point)" in ret


###############################################################################
# Test failed -sql


def test_ogrinfo_failed_sql(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + ' ../ogr/data/poly.shp -sql "SELECT bla"'
    )
    assert "ERROR ret code = 1" in err


###############################################################################
# Test opening an empty geopackage


@pytest.mark.require_driver("GPKG")
def test_ogrinfo_empty_gpkg(ogrinfo_path, tmp_path):

    filename = str(tmp_path / "empty.gpkg")
    ogr.GetDriverByName("GPKG").CreateDataSource(filename)

    ret, err = gdaltest.runexternal_out_and_err(ogrinfo_path + f" {filename}")
    assert err is None or err == "", "got error/warning"


###############################################################################
# Test -if


@pytest.mark.require_driver("GPKG")
def test_ogrinfo_if_ok(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -if GPKG ../ogr/data/gpkg/2d_envelope.gpkg"
    )
    assert err is None or err == "", "got error/warning"
    assert ret.find("GPKG") != -1


###############################################################################
# Test -if


@pytest.mark.require_driver("GeoJSON")
def test_ogrinfo_if_ko(ogrinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " -if GeoJSON ../ogr/data/gpkg/2d_envelope.gpkg"
    )
    assert "not recognized as being in a supported file format" in err


###############################################################################
def test_ogrinfo_access_to_file_without_permission(ogrinfo_path, tmp_path):

    tmpfilename = str(tmp_path / "test.bin")
    with open(tmpfilename, "wb") as f:
        f.write(b"\x00" * 1024)
    os.chmod(tmpfilename, 0)

    # Test that file is not accessible
    try:
        f = open(tmpfilename, "rb")
        f.close()
        pytest.skip("could not set non accessible permission")
    except IOError:
        pass

    _, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " " + tmpfilename,
        encoding="UTF-8",
    )
    lines = list(filter(lambda x: len(x) > 0, err.split("\n")))
    assert (len(lines)) == 3

    os.chmod(tmpfilename, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)


###############################################################################
# Test error messages when file cannot be opened


def test_ogrinfo_file_does_not_exist(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(ogrinfo_path + " does_not_exist.shp")

    assert "No such file or directory" in err
    assert "gdalinfo" not in err


def test_ogrinfo_open_raster(ogrinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogrinfo_path + " ../gcore/data/byte.tif"
    )

    assert "Did you intend to call gdalinfo" in err
