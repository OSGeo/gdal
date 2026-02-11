#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalinfo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import shutil
import stat

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdalinfo_path() is None, reason="gdalinfo not available"
)


@pytest.fixture()
def gdalinfo_path():
    return test_cli_utilities.get_gdalinfo_path()


###############################################################################
# Simple test


def test_gdalinfo_1(gdalinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " ../gcore/data/byte.tif",
        encoding="UTF-8",
    )
    assert err is None or err == "", f"got error/warning {err}"
    assert ret.find("Driver: GTiff/GeoTIFF") != -1


###############################################################################
# Test -checksum option


def test_gdalinfo_2(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " -checksum ../gcore/data/byte.tif")
    assert ret.find("Checksum=4672") != -1


###############################################################################
# Test -nomd option


def test_gdalinfo_3(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " ../gcore/data/byte.tif")
    assert ret.find("Metadata") != -1

    ret = gdaltest.runexternal(gdalinfo_path + " -nomd ../gcore/data/byte.tif")
    assert ret.find("Metadata") == -1


###############################################################################
# Test -noct option


@pytest.mark.require_driver("GIF")
def test_gdalinfo_4(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " ../gdrivers/data/gif/bug407.gif")
    assert ret.find("0: 255,255,255,255") != -1

    ret = gdaltest.runexternal(gdalinfo_path + " -noct ../gdrivers/data/gif/bug407.gif")
    assert ret.find("0: 255,255,255,255") == -1


###############################################################################
# Test -stats option


def test_gdalinfo_5(gdalinfo_path, tmp_path):

    tmpfilename = str(tmp_path / "test_gdalinfo_5.tif")
    shutil.copy("../gcore/data/byte.tif", tmpfilename)

    ret = gdaltest.runexternal(gdalinfo_path + " " + tmpfilename)
    assert ret.find("STATISTICS_MINIMUM=74") == -1, "got wrong minimum."

    ret = gdaltest.runexternal(gdalinfo_path + " -stats " + tmpfilename)
    assert ret.find("STATISTICS_MINIMUM=74") != -1, "got wrong minimum (2)."

    # We will blow an exception if the file does not exist now!
    assert os.path.exists(tmpfilename + ".aux.xml")


###############################################################################
# Test a dataset with overviews and RAT


@pytest.mark.require_driver("HFA")
def test_gdalinfo_6(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " ../gdrivers/data/hfa/int.img")
    assert ret.find("Overviews") != -1
    assert ret.find("GDALRasterAttributeTable") != -1


###############################################################################
# Test a dataset with GCPs


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalinfo_7(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " -wkt_format WKT1 ../gcore/data/gcps.vrt"
    )
    assert ret.find("GCP Projection =") != -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') != -1
    assert ret.find("(100,100) -> (446720,3745320,0)") != -1

    # Same but with -nogcps
    ret = gdaltest.runexternal(
        gdalinfo_path + " -wkt_format WKT1 -nogcp ../gcore/data/gcps.vrt"
    )
    assert ret.find("GCP Projection =") == -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') == -1
    assert ret.find("(100,100) -> (446720,3745320,0)") == -1


###############################################################################
# Test -hist option


def test_gdalinfo_8(gdalinfo_path, tmp_path):

    tmpfilename = str(tmp_path / "test_gdalinfo_8.tif")
    shutil.copy("../gcore/data/byte.tif", tmpfilename)

    ret = gdaltest.runexternal(gdalinfo_path + " " + tmpfilename)
    assert (
        ret.find(
            "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1"
        )
        == -1
    ), "did not expect histogram."

    ret = gdaltest.runexternal(gdalinfo_path + " -hist " + tmpfilename)
    assert (
        ret.find(
            "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1"
        )
        != -1
    ), "did not get expected histogram."

    # We will blow an exception if the file does not exist now!
    assert os.path.exists(tmpfilename + ".aux.xml")


###############################################################################
# Test -mdd option


@pytest.mark.require_driver("NITF")
def test_gdalinfo_9(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " ../gdrivers/data/nitf/fake_nsif.ntf")
    assert ret.find("BLOCKA=010000001000000000") == -1, "got unexpected extra MD."

    ret = gdaltest.runexternal(
        gdalinfo_path + " -mdd TRE ../gdrivers/data/nitf/fake_nsif.ntf"
    )
    assert ret.find("BLOCKA=010000001000000000") != -1, "did not get extra MD."


###############################################################################
# Test -mm option


def test_gdalinfo_10(gdalinfo_path):
    ret = gdaltest.runexternal(gdalinfo_path + " ../gcore/data/byte.tif")
    assert ret.find("Computed Min/Max=74.000,255.000") == -1

    ret = gdaltest.runexternal(gdalinfo_path + " -mm ../gcore/data/byte.tif")
    assert ret.find("Computed Min/Max=74.000,255.000") != -1


###############################################################################
# Test gdalinfo --version


def test_gdalinfo_11(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " --version", check_memleak=False)
    assert ret.startswith(gdal.VersionInfo("--version"))


###############################################################################
# Test gdalinfo --build


def test_gdalinfo_12(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " --build", check_memleak=False)
    ret = ret.replace("\r\n", "\n")
    assert ret.startswith(gdal.VersionInfo("BUILD_INFO"))


###############################################################################
# Test gdalinfo --license


def test_gdalinfo_13(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " --license", check_memleak=False)
    ret = ret.replace("\r\n", "\n")
    if not ret.startswith(gdal.VersionInfo("LICENSE")):
        print(gdal.VersionInfo("LICENSE"))
        if gdaltest.is_travis_branch("mingw"):
            return "expected_fail"
        pytest.fail(ret)


###############################################################################
# Test erroneous use of --config.


def test_gdalinfo_14(gdalinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --config", check_memleak=False
    )
    assert "--config option given without" in err


###############################################################################
# Test erroneous use of --mempreload.


def test_gdalinfo_15(gdalinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --mempreload", check_memleak=False
    )
    assert "--mempreload option given without directory path" in err


###############################################################################
# Test --mempreload


def test_gdalinfo_16(gdalinfo_path):

    ret, _ = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --debug on --mempreload ../gcore/data /vsimem/byte.tif",
        check_memleak=False,
        encoding="UTF-8",
    )
    assert ret.startswith("Driver: GTiff/GeoTIFF")


###############################################################################
# Test erroneous use of --debug.


def test_gdalinfo_17(gdalinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --debug", check_memleak=False
    )
    assert "--debug option given without debug level" in err


###############################################################################
# Test erroneous use of --optfile.


def test_gdalinfo_18(gdalinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --optfile", check_memleak=False
    )
    assert "--optfile option given without filename" in err

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --optfile /foo/bar",
        check_memleak=False,
    )
    assert "Unable to open optfile" in err


###############################################################################
# Test --optfile


def test_gdalinfo_19(gdalinfo_path, tmp_path):

    optfile_txt = str(tmp_path / "optfile.txt")

    f = open(optfile_txt, "wt")
    f.write("# comment\n")
    f.write("../gcore/data/byte.tif\n")
    f.close()
    ret = gdaltest.runexternal(
        gdalinfo_path + f" --optfile {optfile_txt}",
        check_memleak=False,
    )
    assert ret.startswith("Driver: GTiff/GeoTIFF")


###############################################################################
# Test --formats


def test_gdalinfo_20(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " --formats", check_memleak=False)
    assert "GTiff -raster- (rw+uvs): GeoTIFF" in ret


###############################################################################
# Test --formats -json


@pytest.mark.require_driver("VRT")
def test_gdalinfo_formats_json(gdalinfo_path):

    ret = json.loads(
        gdaltest.runexternal(gdalinfo_path + " --formats -json", check_memleak=False)
    )
    assert {
        "short_name": "VRT",
        "long_name": "Virtual Raster",
        "scopes": ["raster", "multidimensional_raster"],
        "capabilities": ["open", "create", "create_copy", "update", "virtual_io"],
        "file_extensions": ["vrt"],
    } in ret


###############################################################################
# Test erroneous use of --format.


def test_gdalinfo_21(gdalinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --format", check_memleak=False
    )
    assert "--format option given without a format code" in err

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " --format foo_bar",
        check_memleak=False,
    )
    assert "--format option given with format" in err


###############################################################################
# Test --format


def test_gdalinfo_22(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " --format GTiff", check_memleak=False)

    expected_strings = [
        "Short Name:",
        "Long Name:",
        "Extensions:",
        "Mime Type:",
        "Help Topic:",
        "Supports: Create()",
        "Supports: CreateCopy()",
        "Supports: Virtual IO",
        "Creation Datatypes",
        "<CreationOptionList>",
    ]
    for expected_string in expected_strings:
        assert expected_string in ret, "did not find %s" % expected_string


###############################################################################
# Test --help-general


def test_gdalinfo_23(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " --help-general", check_memleak=False)
    assert "Generic GDAL utility command options" in ret


###############################################################################
# Test --locale


def test_gdalinfo_24(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " --locale C ../gcore/data/byte.tif",
        check_memleak=False,
    )
    assert ret.startswith("Driver: GTiff/GeoTIFF")


###############################################################################
# Test -listmdd


def test_gdalinfo_25(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " ../gdrivers/data/gtiff/byte_with_xmp.tif -listmdd",
        check_memleak=False,
    )
    assert "Metadata domains:" in ret
    assert "  xml:XMP" in ret


###############################################################################
# Test -mdd all


def test_gdalinfo_26(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " ../gdrivers/data/gtiff/byte_with_xmp.tif -mdd all",
        check_memleak=False,
    )
    assert "Metadata (xml:XMP)" in ret


###############################################################################
# Test -oo


@pytest.mark.require_driver("AAIGRID")
def test_gdalinfo_27(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " ../gdrivers/data/aaigrid/float64.asc -oo datatype=float64",
        check_memleak=False,
    )
    assert "Type=Float64" in ret


###############################################################################
# Simple -json test


def test_gdalinfo_28(gdalinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " -json ../gcore/data/byte.tif",
        encoding="UTF-8",
    )
    ret = json.loads(ret)
    assert err is None or err == "", f"got error/warning {err}"
    assert ret["driverShortName"] == "GTiff"


###############################################################################
# Test -json -checksum option


def test_gdalinfo_29(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " -json -checksum ../gcore/data/byte.tif"
    )
    ret = json.loads(ret)
    assert ret["bands"][0]["checksum"] == 4672


###############################################################################
# Test -json -nomd option


def test_gdalinfo_30(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " -json ../gcore/data/byte.tif")
    ret = json.loads(ret)
    assert "metadata" in ret

    ret = gdaltest.runexternal(gdalinfo_path + " -json -nomd ../gcore/data/byte.tif")
    ret = json.loads(ret)
    assert "metadata" not in ret


###############################################################################
# Test -json -noct option


@pytest.mark.require_driver("GIF")
def test_gdalinfo_31(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " -json ../gdrivers/data/gif/bug407.gif")
    ret = json.loads(ret)
    assert ret["bands"][0]["colorTable"]["entries"][0] == [255, 255, 255, 255]

    ret = gdaltest.runexternal(
        gdalinfo_path + " -json -noct ../gdrivers/data/gif/bug407.gif"
    )
    ret = json.loads(ret)
    assert "colorTable" not in ret["bands"][0]


###############################################################################
# Test -stats option


def test_gdalinfo_stats(gdalinfo_path, tmp_path):

    filename = str(tmp_path / "test.tif")
    shutil.copy("../gcore/data/byte.tif", filename)

    ret = gdaltest.runexternal(gdalinfo_path + f" -json {filename}")
    ret = json.loads(ret)
    assert "" not in ret["bands"][0]["metadata"], "got wrong minimum."

    ret = gdaltest.runexternal(gdalinfo_path + f" -json -stats {filename}")
    ret = json.loads(ret)
    assert (
        ret["bands"][0]["metadata"][""]["STATISTICS_MINIMUM"] == "74"
    ), "got wrong minimum (2)."

    # We will blow an exception if the file does not exist now!
    os.remove(f"{filename}.aux.xml")


###############################################################################
# Test a dataset with overviews and RAT


@pytest.mark.require_driver("HFA")
def test_gdalinfo_33(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " -json ../gdrivers/data/hfa/int.img")
    ret = json.loads(ret)
    assert "overviews" in ret["bands"][0]
    assert "rat" in ret["bands"][0]


###############################################################################
# Test a dataset with GCPs


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalinfo_34(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " -json ../gcore/data/gcps.vrt")
    ret = json.loads(ret)
    assert "wkt" in ret["gcps"]["coordinateSystem"]
    assert (
        ret["gcps"]["coordinateSystem"]["wkt"].find('PROJCRS["NAD27 / UTM zone 11N"')
        != -1
    )
    assert ret["gcps"]["gcpList"][0]["x"] == 440720.0

    ret = gdaltest.runexternal(gdalinfo_path + " -json -nogcp ../gcore/data/gcps.vrt")
    ret = json.loads(ret)
    assert "gcps" not in ret


###############################################################################
# Test -hist option


def test_gdalinfo_35(gdalinfo_path, tmp_path):

    tmp_tif = str(tmp_path / "byte.tif")
    shutil.copy("../gcore/data/byte.tif", tmp_tif)

    ret = gdaltest.runexternal(gdalinfo_path + f" -json {tmp_tif}")
    ret = json.loads(ret)
    assert "histogram" not in ret["bands"][0], "did not expect histogram."

    ret = gdaltest.runexternal(gdalinfo_path + f" -json -hist {tmp_tif}")
    ret = json.loads(ret)
    assert ret["bands"][0]["histogram"]["buckets"] == [
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        6,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        37,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        57,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        62,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        66,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        72,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        31,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        24,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        7,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        12,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        5,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        3,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        2,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
    ], "did not get expected histogram."

    assert os.path.exists(tmp_tif + ".aux.xml")


###############################################################################
# Test -mdd option


@pytest.mark.require_driver("NITF")
def test_gdalinfo_36(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " -json ../gdrivers/data/nitf/fake_nsif.ntf"
    )
    ret = json.loads(ret)
    assert "TRE" not in ret["metadata"], "got unexpected extra MD."

    ret = gdaltest.runexternal(
        gdalinfo_path + " -json -mdd TRE ../gdrivers/data/nitf/fake_nsif.ntf"
    )
    ret = json.loads(ret)
    assert (
        ret["metadata"]["TRE"]["BLOCKA"].find("010000001000000000") != -1
    ), "did not get extra MD."


###############################################################################
# Test -mm option


def test_gdalinfo_37(gdalinfo_path):

    ret = gdaltest.runexternal(gdalinfo_path + " -json ../gcore/data/byte.tif")
    ret = json.loads(ret)
    assert "computedMin" not in ret["bands"][0]

    ret = gdaltest.runexternal(gdalinfo_path + " -json -mm ../gcore/data/byte.tif")
    ret = json.loads(ret)
    assert ret["bands"][0]["computedMin"] == 74.000


###############################################################################
# Test -listmdd


def test_gdalinfo_38(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " -json ../gdrivers/data/gtiff/byte_with_xmp.tif -listmdd",
        check_memleak=False,
    )
    ret = json.loads(ret)
    assert "metadataDomains" in ret["metadata"]
    assert ret["metadata"]["metadataDomains"][0] == "xml:XMP"


###############################################################################
# Test -mdd all


def test_gdalinfo_39(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " -json ../gdrivers/data/gtiff/byte_with_xmp.tif -mdd all",
        check_memleak=False,
    )
    ret = json.loads(ret)
    assert "xml:XMP" in ret["metadata"]


###############################################################################
# Test -json wgs84Extent


def test_gdalinfo_40(gdalinfo_path):

    ret = gdaltest.runexternal(
        gdalinfo_path + " -json ../gdrivers/data/small_world.tif"
    )
    ret = json.loads(ret)
    assert "wgs84Extent" in ret
    assert "type" in ret["wgs84Extent"]
    assert ret["wgs84Extent"]["type"] == "Polygon"
    assert "coordinates" in ret["wgs84Extent"]
    assert ret["wgs84Extent"]["coordinates"] == [
        [[-180.0, 90.0], [-180.0, -90.0], [180.0, -90.0], [180.0, 90.0], [-180.0, 90.0]]
    ]


###############################################################################
# Test -if option


def test_gdalinfo_if_option(gdalinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " -if GTiff ../gcore/data/byte.tif",
        encoding="UTF-8",
    )
    assert err is None or err == "", "got error/warning"
    assert ret.find("Driver: GTiff/GeoTIFF") != -1

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " -if invalid_driver_name ../gcore/data/byte.tif",
        encoding="UTF-8",
    )
    assert err is not None
    assert "invalid_driver_name" in err

    _, err = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " -if HFA ../gcore/data/byte.tif",
        encoding="UTF-8",
    )
    assert err is not None


###############################################################################
# Test STAC JSON output


def test_gdalinfo_stac_json(gdalinfo_path, tmp_path):

    tmpfilename = str(tmp_path / "test_gdalinfo_stac_json.tif")
    shutil.copy("../gcore/data/byte.tif", tmpfilename)
    ret, _ = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " -json -proj4 -stats -hist " + tmpfilename,
        encoding="UTF-8",
    )
    data = json.loads(ret)

    assert "stac" in data
    stac = data["stac"]

    assert stac["proj:shape"] == [20, 20]

    assert stac["proj:wkt2"].startswith("PROJCRS")
    assert stac["proj:epsg"] == 26711
    from osgeo import osr

    if osr.GetPROJVersionMajor() >= 7:
        assert isinstance(stac["proj:projjson"], dict)
    assert stac["proj:transform"] == [60.0, 0.0, 440720.0, 0.0, -60.0, 3751320.0]

    assert len(stac["raster:bands"]) == 1
    raster_band = stac["raster:bands"][0]
    assert raster_band["data_type"] == "uint8"
    assert raster_band["stats"] == {
        "minimum": 74.0,
        "maximum": 255.0,
        "mean": 126.765,
        "stddev": 22.928,
    }
    assert "histogram" in raster_band

    assert len(stac["eo:bands"]) == 1
    band = stac["eo:bands"][0]
    assert band["name"] == "b1"
    assert band["description"] == "Gray"


def test_gdalinfo_stac_eo_bands(gdalinfo_path, tmp_path):
    # Test eo:bands cloud_cover
    # https://github.com/OSGeo/gdal/pull/6265#issuecomment-1232229669
    tmpfilename = str(tmp_path / "test_gdalinfo_stac_json_cloud_cover.tif")
    shutil.copy("../gcore/data/md_dg.tif", tmpfilename)
    shutil.copy(
        "../gcore/data/md_dg.IMD", f"{tmp_path}/test_gdalinfo_stac_json_cloud_cover.IMD"
    )
    shutil.copy(
        "../gcore/data/md_dg.RPB", f"{tmp_path}/test_gdalinfo_stac_json_cloud_cover.RPB"
    )
    ret, _ = gdaltest.runexternal_out_and_err(
        gdalinfo_path + " -json " + tmpfilename,
        encoding="UTF-8",
    )
    data = json.loads(ret)

    assert data["stac"]["eo:cloud_cover"] == 2


def test_gdalinfo_access_to_file_without_permission(gdalinfo_path, tmp_path):

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
        gdalinfo_path + " " + tmpfilename,
        encoding="UTF-8",
    )
    lines = list(filter(lambda x: len(x) > 0, err.split("\n")))
    assert (len(lines)) == 3

    os.chmod(tmpfilename, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)


###############################################################################
# Test error messages when file cannot be opened


def test_gdalinfo_file_does_not_exist(gdalinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalinfo_path + " does_not_exist.tif")

    assert "No such file or directory" in err
    assert "ogrinfo" not in err


def test_gdalinfo_open_vector(gdalinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalinfo_path + " ../ogr/data/poly.shp")

    assert "Did you intend to call ogrinfo" in err
