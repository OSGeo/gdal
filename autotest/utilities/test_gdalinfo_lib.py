#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalinfo
# Author:   Faza Mahamood <fazamhd at gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pathlib
import shutil

import gdaltest
import pytest

from osgeo import gdal, osr

###############################################################################
# Simple test


def test_gdalinfo_lib_1():

    ds = gdal.Open("../gcore/data/byte.tif")

    ret = gdal.Info(ds)
    assert ret.find("Driver: GTiff/GeoTIFF") != -1, "did not get expected string."


def test_gdalinfo_lib_1_str():

    ret = gdal.Info("../gcore/data/byte.tif")
    assert ret.find("Driver: GTiff/GeoTIFF") != -1, "did not get expected string."


def test_gdalinfo_lib_1_path():

    ret = gdal.Info(pathlib.Path("../gcore/data/byte.tif"))
    assert ret.find("Driver: GTiff/GeoTIFF") != -1, "did not get expected string."


###############################################################################
# Test Json format


def test_gdalinfo_lib_2():

    ds = gdal.Open("../gcore/data/byte.tif")

    ret = gdal.Info(ds, format="json")
    assert ret["driverShortName"] == "GTiff", "wrong value for driverShortName."
    assert ret["geoTransform"] == [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0]
    assert ret["stac"]["proj:transform"] == [60.0, 0.0, 440720.0, 0.0, -60.0, 3751320.0]

    gdaltest.validate_json(ret, "gdalinfo_output.schema.json")


###############################################################################
# Test extraMDDomains()


@pytest.mark.require_driver("NITF")
def test_gdalinfo_lib_3():

    ds = gdal.Open("../gdrivers/data/nitf/fake_nsif.ntf")

    ret = gdal.Info(ds, format="json")
    assert "TRE" not in ret["metadata"], "got unexpected extra MD."

    options = gdal.InfoOptions(format="json", extraMDDomains=["TRE"])
    ret = gdal.Info(ds, options=options)
    assert (
        ret["metadata"]["TRE"]["BLOCKA"].find("010000001000000000") != -1
    ), "did not get extra MD."


###############################################################################
# Test allMetadata


def test_gdalinfo_lib_4():

    ds = gdal.Open("../gdrivers/data/gtiff/byte_with_xmp.tif")

    ret = gdal.Info(ds, allMetadata=True, format="json")
    assert "xml:XMP" in ret["metadata"]


###############################################################################
# Test all options


def test_gdalinfo_lib_5(tmp_path):

    tmp_tif = str(tmp_path / "byte.tif")
    shutil.copy("../gcore/data/byte.tif", tmp_tif)

    ds = gdal.Open(tmp_tif)

    ret = gdal.Info(
        ds,
        format="json",
        deserialize=True,
        computeMinMax=True,
        reportHistograms=True,
        reportProj4=True,
        # stats=True, this is mutually exclusive with approxStats
        approxStats=True,
        computeChecksum=True,
        showGCPs=False,
        showMetadata=False,
        showRAT=False,
        showColorTable=False,
        listMDD=True,
        showFileList=False,
    )
    assert "files" not in ret
    band = ret["bands"][0]
    assert "computedMin" in band
    assert "histogram" in band
    assert "checksum" in band
    assert "stdDev" in band
    assert ret["coordinateSystem"]["dataAxisToSRSAxisMapping"] == [1, 2]

    gdaltest.validate_json(ret, "gdalinfo_output.schema.json")

    ds = None


###############################################################################
# Test command line syntax + dataset as string


def test_gdalinfo_lib_6():

    ret = gdal.Info("../gcore/data/byte.tif", options="-json")
    assert ret["driverShortName"] == "GTiff", "wrong value for driverShortName."
    assert isinstance(ret, dict)


###############################################################################
# Test with unicode strings


def test_gdalinfo_lib_7():

    ret = gdal.Info(
        "../gcore/data/byte.tif".encode("ascii").decode("ascii"),
        options="-json".encode("ascii").decode("ascii"),
    )
    assert ret["driverShortName"] == "GTiff", "wrong value for driverShortName."
    assert isinstance(ret, dict)


###############################################################################
# Test with list of strings


def test_gdalinfo_lib_8():

    ret = gdal.Info("../gcore/data/byte.tif", options=["-json"])
    assert ret["driverShortName"] == "GTiff", "wrong value for driverShortName."
    assert isinstance(ret, dict)


###############################################################################


def test_gdalinfo_lib_nodatavalues():

    ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        options='-of VRT -b 1 -b 1 -b 1 -mo "NODATA_VALUES=0 1 2"',
    )
    ret = gdal.Info(ds)
    assert "PER_DATASET NODATA" in ret, "wrong value for mask flags."


###############################################################################


@pytest.mark.parametrize("epoch", ["2021.0", "2021.3"])
def test_gdalinfo_lib_coordinate_epoch(epoch):

    ds = gdal.Translate(
        "", "../gcore/data/byte.tif", options=f'-of MEM -a_coord_epoch {epoch}"'
    )
    ret = gdal.Info(ds)
    assert f"Coordinate epoch: {epoch}" in ret

    ret = gdal.Info(ds, format="json")
    assert "coordinateEpoch" in ret
    assert ret["coordinateEpoch"] == float(epoch)


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/5794


@pytest.mark.parametrize("datatype", ["Float32", "Float64"])
def test_gdalinfo_lib_nodata_precision(datatype):

    ds = gdal.Translate(
        "",
        "../gcore/data/float32.tif",
        options="-of MEM -a_nodata -1e37 -ot " + datatype,
    )
    ret = gdal.Info(ds)
    assert "e37" in ret.lower() or "e+37" in ret.lower() or "e+037" in ret.lower()

    ret = gdal.Info(ds, format="json", deserialize=False)
    assert "e37" in ret.lower() or "e+37" in ret.lower() or "e+037" in ret.lower()


def test_gdalinfo_lib_nodata_full_precision_float64():

    nodata_str = "-1.1234567890123456e-10"
    ds = gdal.Translate(
        "",
        "../gcore/data/float32.tif",
        options="-of MEM -a_nodata " + nodata_str + " -ot float64",
    )
    ret = gdal.Info(ds)
    pos = ret.find("NoData Value=")
    assert pos > 0
    eol_pos = ret.find("\n", pos)
    assert eol_pos > 0
    got_nodata_str = ret[pos + len("NoData Value=") : eol_pos]
    assert float(got_nodata_str) == float(nodata_str)

    ret = gdal.Info(ds, format="json")
    assert ret["bands"][0]["noDataValue"] == float(nodata_str)
    assert ret["stac"]["raster:bands"][0]["nodata"] == float(nodata_str)


def test_gdalinfo_lib_nodata_int():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(255)
    assert "NoData Value=255\n" in gdal.Info(ds).replace("\r\n", "\n")

    ret = gdal.Info(ds, format="json")
    ndv = ret["bands"][0]["noDataValue"]
    assert ndv == 255
    assert isinstance(ndv, int)


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/8137


def test_gdalinfo_lib_json_projjson_no_epsg():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    srs = osr.SpatialReference()
    srs.SetLocalCS("foo")
    ds.SetSpatialRef(srs)
    ret = gdal.Info(ds, options="-json")
    assert ret["stac"]["proj:epsg"] is None
    assert ret["stac"]["proj:wkt2"] is not None


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/9337


def test_gdalinfo_lib_json_proj_shape():

    width = 2
    height = 1
    ds = gdal.GetDriverByName("MEM").Create("", width, height)
    ret = gdal.Info(ds, options="-json")
    assert ret["stac"]["proj:shape"] == [height, width]


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/9396


def test_gdalinfo_lib_json_engineering_crs():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""ENGCRS["Arbitrary (m)",
    EDATUM["Unknown engineering datum"],
    CS[Cartesian,2],
        AXIS["(E)",east,
            ORDER[1],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]],
        AXIS["(N)",north,
            ORDER[2],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]]]""")
    ds.SetSpatialRef(srs)
    ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    ret = gdal.Info(ds, format="json")
    assert "coordinateSystem" in ret
    assert "cornerCoordinates" in ret
    assert "wgs84Extent" not in ret


###############################################################################
# Test -nonodata


def test_gdalinfo_lib_nonodata(tmp_path):

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(1)

    ret = gdal.Info(ds, format="json")
    assert "noDataValue" in ret["bands"][0]

    ret = gdal.Info(ds, format="json", showNodata=False)
    assert "noDataValue" not in ret["bands"][0]


###############################################################################
# Test -nomask


def test_gdalinfo_lib_nomask(tmp_path):

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)

    ret = gdal.Info(ds, format="json")
    assert "mask" in ret["bands"][0]

    ret = gdal.Info(ds, format="json", showMask=False)
    assert "mask" not in ret["bands"][0]


###############################################################################


def test_gdalinfo_lib_json_stac_common_name():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_PanBand)
    ret = gdal.Info(ds, options="-json")
    assert ret["stac"]["eo:bands"][0]["common_name"] == "pan"


###############################################################################


@pytest.mark.require_driver("HFA")
def test_gdalinfo_lib_json_color_table_and_rat():

    ds = gdal.Open("../gcore/data/rat.img")

    ret = gdal.Info(ds, format="json")
    assert "colorTable" in ret["bands"][0]
    assert "rat" in ret["bands"][0]

    gdaltest.validate_json(ret, "gdalinfo_output.schema.json")


###############################################################################


def test_gdalinfo_lib_no_driver():

    ds = gdal.Open("../gcore/data/byte.tif")
    ds2 = ds.GetRasterBand(1).AsMDArray().AsClassicDataset(0, 1)
    assert ds2.GetDriver() is None
    gdal.Info(ds2)
    gdal.Info(ds2, format="json")


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/13906


@pytest.mark.parametrize(
    "wkt_format,expected",
    [
        ("WKT1", """PROJCS["NAD27 / UTM zone 11N","""),
        ("WKT1_ESRI", """PROJCS["NAD_1927_UTM_Zone_11N","""),
        ("WKT2", """PROJCRS["NAD27 / UTM zone 11N","""),
    ],
)
def test_gdalinfo_lib_wkt_format(wkt_format, expected):

    ds = gdal.Open("../gcore/data/byte.tif")
    ret = gdal.Info(ds, options="-json -wkt_format " + wkt_format)
    assert ret["coordinateSystem"]["wkt"].startswith(expected)
