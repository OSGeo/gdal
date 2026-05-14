#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test STACTA driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import copy
import json
import math
import struct
from http.server import BaseHTTPRequestHandler

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_driver("STACTA")


def test_stacta_basic():

    ds = gdal.Open("data/stacta/test.json")
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 2048
    assert ds.RasterYSize == 1024
    assert ds.GetSpatialRef().GetName() == "WGS 84"
    assert ds.GetGeoTransform() == pytest.approx(
        [-180.0, 0.17578125, 0.0, 90.0, 0.0, -0.17578125], rel=1e-8
    )
    assert ds.GetRasterBand(1).GetNoDataValue() == 0.0
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert len(ds.GetSubDatasets()) == 0

    # Create a reference dataset, that is externally the same as the STACTA one
    vrt_ds = gdal.BuildVRT(
        "",
        [
            "data/stacta/WorldCRS84Quad/2/0/0.tif",
            "data/stacta/WorldCRS84Quad/2/0/1.tif",
        ],
    )
    ref_ds = gdal.Translate("", vrt_ds, format="MEM")
    ref_ds.BuildOverviews("NEAR", [2, 4])

    # Whole dataset reading
    assert ds.ReadRaster() == ref_ds.ReadRaster()

    # Whole band reading
    assert ds.GetRasterBand(2).ReadRaster() == ref_ds.GetRasterBand(2).ReadRaster()

    # Subwindow intersecting 2 tiles
    assert ds.ReadRaster(1000, 500, 50, 100) == ref_ds.ReadRaster(1000, 500, 50, 100)

    # Subwindow intersecting 2 tiles with downsampling, but at the same zoom level
    assert ds.ReadRaster(1000, 500, 50, 100, 30, 60) == ref_ds.ReadRaster(
        1000, 500, 50, 100, 30, 60
    )

    # Subwindow intersecting 2 tiles with downsampling, but at another zoom level
    assert ds.ReadRaster(1000, 500, 50, 100, 10, 20) == ref_ds.ReadRaster(
        1000, 500, 50, 100, 10, 20
    )

    # Subwindow intersecting 2 tiles with downsampling, but at another zoom level
    assert ds.GetRasterBand(1).ReadRaster(
        1000, 500, 50, 100, 10, 20
    ) == ref_ds.GetRasterBand(1).ReadRaster(1000, 500, 50, 100, 10, 20)

    # Same as above but with bilinear resampling
    assert ds.ReadRaster(
        1000, 500, 50, 100, 30, 60, resample_alg=gdal.GRIORA_Bilinear
    ) == ref_ds.ReadRaster(
        1000, 500, 50, 100, 30, 60, resample_alg=gdal.GRIORA_Bilinear
    )

    # Downsampling with floating point coordinates, intersecting one tile
    assert ds.ReadRaster(
        0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear
    ) == ref_ds.ReadRaster(
        0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear
    )

    # Downsampling with floating point coordinates, intersecting two tiles
    assert ds.ReadRaster(
        0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear
    ) == ref_ds.ReadRaster(
        0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear
    )


def test_stacta_east_hemisphere():

    # Test a json file with min_tile_col = 1 at zoom level 2
    ds = gdal.OpenEx(
        "data/stacta/test_east_hemisphere.json", open_options=["WHOLE_METATILE=YES"]
    )
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024
    assert ds.GetSpatialRef().GetName() == "WGS 84"
    assert ds.GetGeoTransform() == pytest.approx(
        [0.0, 0.17578125, 0.0, 90.0, 0.0, -0.17578125], rel=1e-8
    )
    assert ds.GetRasterBand(1).GetOverviewCount() == 2

    # Create a reference dataset, that is externally the same as the STACTA one
    vrt_ds = gdal.BuildVRT("", ["data/stacta/WorldCRS84Quad/2/0/1.tif"])
    ref_ds = gdal.Translate("", vrt_ds, format="MEM")
    ref_ds.BuildOverviews("NEAR", [2, 4])

    assert ds.ReadRaster(600, 500, 50, 100, 10, 20) == ref_ds.ReadRaster(
        600, 500, 50, 100, 10, 20
    )


def test_stacta_subdatasets():

    ds = gdal.Open("data/stacta/test_multiple_asset_templates.json")
    assert len(ds.GetSubDatasets()) == 2
    subds1 = gdal.Open(ds.GetSubDatasets()[0][0])
    assert subds1 is not None
    subds2 = gdal.Open(ds.GetSubDatasets()[1][0])
    assert subds2 is not None
    assert subds1.GetRasterBand(1).ReadRaster() != subds2.GetRasterBand(1).ReadRaster()

    ds = gdal.Open("data/stacta/test_multiple_tms.json")
    assert len(ds.GetSubDatasets()) == 2
    subds1 = gdal.Open(ds.GetSubDatasets()[0][0])
    assert subds1 is not None


def test_stacta_missing_metatile():

    gdal.FileFromMemBuffer(
        "/vsimem/stacta/test.json", open("data/stacta/test.json", "rb").read()
    )
    gdal.FileFromMemBuffer(
        "/vsimem/stacta/WorldCRS84Quad/0/0/0.tif",
        open("data/stacta/WorldCRS84Quad/0/0/0.tif", "rb").read(),
    )
    gdal.FileFromMemBuffer(
        "/vsimem/stacta/WorldCRS84Quad/1/0/0.tif",
        open("data/stacta/WorldCRS84Quad/1/0/0.tif", "rb").read(),
    )
    gdal.FileFromMemBuffer(
        "/vsimem/stacta/WorldCRS84Quad/2/0/0.tif",
        open("data/stacta/WorldCRS84Quad/2/0/0.tif", "rb").read(),
    )

    ds = gdal.Open("/vsimem/stacta/test.json")
    with pytest.raises(Exception):
        ds.ReadRaster()

    # Missing right tile
    with gdaltest.config_option("GDAL_STACTA_SKIP_MISSING_METATILE", "YES"):
        ds = gdal.Open("/vsimem/stacta/test.json")
        got_data = ds.ReadRaster()
        assert got_data is not None
        got_data = struct.unpack("B" * len(got_data), got_data)
        for i in range(3):
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 500] != 0
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 1500] == 0

    gdal.Unlink("/vsimem/stacta/WorldCRS84Quad/1/0/0.tif")
    gdal.Unlink("/vsimem/stacta/WorldCRS84Quad/2/0/0.tif")
    gdal.FileFromMemBuffer(
        "/vsimem/stacta/WorldCRS84Quad/2/0/1.tif",
        open("data/stacta/WorldCRS84Quad/2/0/1.tif", "rb").read(),
    )

    # Missing left tile
    with gdaltest.config_option("GDAL_STACTA_SKIP_MISSING_METATILE", "YES"):
        ds = gdal.Open("/vsimem/stacta/test.json")
        got_data = ds.ReadRaster()
        assert got_data is not None
        got_data = struct.unpack("B" * len(got_data), got_data)
        for i in range(3):
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 500] == 0
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 1500] != 0

    gdal.Unlink("/vsimem/stacta/test.json")
    assert gdal.VSIStatL("/vsimem/stacta/WorldCRS84Quad/1/0/0.tif") is None
    gdal.Unlink("/vsimem/stacta/WorldCRS84Quad/2/0/1.tif")


###############################################################################
do_log = False


class STACTAHandler(BaseHTTPRequestHandler):
    def log_request(self, code="-", size="-"):
        pass

    def do_HEAD(self):
        self.send_response(200)
        self.end_headers()

    def do_GET(self):

        try:
            if do_log:
                f = open("/tmp/log.txt", "a")
                f.write("GET %s\n" % self.path)
                f.close()

            assert self.path.startswith("/WorldCRS84Quad/")

            if self.path.endswith(".tif"):
                f = open("data/stacta" + self.path, "rb")
                content = f.read()
                f.close()
                self.send_response(200)
                self.send_header("Content-type", "image/tiff")
                self.send_header("Content-Length", len(content))
                self.end_headers()
                self.wfile.write(content)
                return

            assert False

        except IOError:
            pass

        self.send_error(404, "File Not Found: %s" % self.path)


@pytest.mark.require_curl
def test_stacta_network():

    process, port = webserver.launch(handler=STACTAHandler)
    if port == 0:
        pytest.skip()

    try:
        stacta_def = json.loads(open("data/stacta/test.json", "rb").read())
        stacta_def["asset_templates"]["bands"]["href"] = (
            "http://localhost:%d/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.tif"
            % port
        )
        with gdaltest.tempfile("/vsimem/test.json", json.dumps(stacta_def)):
            ds = gdal.Open("/vsimem/test.json")
            assert ds.RasterCount == 3
            assert ds.RasterXSize == 2048
            assert ds.RasterYSize == 1024
            assert ds.GetSpatialRef().GetName() == "WGS 84"
            assert ds.GetGeoTransform() == pytest.approx(
                [-180.0, 0.17578125, 0.0, 90.0, 0.0, -0.17578125], rel=1e-8
            )
            assert ds.GetRasterBand(1).GetNoDataValue() == 0.0
            assert ds.GetRasterBand(1).GetOverviewCount() == 2
            assert len(ds.GetSubDatasets()) == 0

            # Create a reference dataset, that is externally the same as the STACTA one
            vrt_ds = gdal.BuildVRT(
                "",
                [
                    "data/stacta/WorldCRS84Quad/2/0/0.tif",
                    "data/stacta/WorldCRS84Quad/2/0/1.tif",
                ],
            )
            ref_ds = gdal.Translate("", vrt_ds, format="MEM")
            ref_ds.BuildOverviews("NEAR", [2, 4])

            # Whole dataset reading
            assert ds.ReadRaster() == ref_ds.ReadRaster()

    finally:
        webserver.server_stop(process, port)


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/stacta/test_with_raster_extension.json",
        "data/stacta/test_with_raster_extension_no_eo_bands.json",
        "data/stacta/test_stac_1_1.json",
    ],
)
def test_stacta_with_raster_extension_nominal(filename):

    ds = gdal.Open(filename)
    assert ds.RasterCount == 6
    band = ds.GetRasterBand(1)
    if (
        filename == "data/stacta/test_with_raster_extension.json"
        or filename == "data/stacta/test_stac_1_1.json"
    ):
        assert band.GetMetadataItem("name") == "B1"
    if filename == "data/stacta/test_stac_1_1.json":
        assert band.GetMetadata_Dict() == {"common_name": "nir", "name": "B1"}
    assert band.DataType == gdal.GDT_UInt8
    assert band.GetNoDataValue() == 1
    assert band.GetOffset() == 1.2
    assert band.GetScale() == 10
    assert band.GetUnitType() == "dn"
    assert band.GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "7"

    band = ds.GetRasterBand(2)
    assert band.DataType == gdal.GDT_Float32
    assert band.GetNoDataValue() == 1.5
    assert band.GetOffset() is None
    assert band.GetScale() is None
    assert band.GetUnitType() == ""

    band = ds.GetRasterBand(3)
    assert band.GetNoDataValue() == float("inf")

    band = ds.GetRasterBand(4)
    assert band.GetNoDataValue() == float("-inf")

    band = ds.GetRasterBand(5)
    assert math.isnan(band.GetNoDataValue())

    band = ds.GetRasterBand(6)
    assert band.GetNoDataValue() is None


###############################################################################


def test_stacta_with_raster_extension_errors():

    j_ori = json.loads(open("data/stacta/test_with_raster_extension.json", "rb").read())

    j = copy.deepcopy(j_ori)
    del j["asset_templates"]["bands"]["raster:bands"]
    with gdaltest.tempfile("/vsimem/test.json", json.dumps(j)):
        with pytest.raises(
            Exception, match="Cannot open /vsimem/non_existing/WorldCRS84Quad/0/0/0.tif"
        ):
            gdal.Open("/vsimem/test.json")

    j = copy.deepcopy(j_ori)
    del j["asset_templates"]["bands"]["raster:bands"][4]
    with gdaltest.tempfile("/vsimem/test.json", json.dumps(j)):
        with pytest.raises(
            Exception, match="Cannot open /vsimem/non_existing/WorldCRS84Quad/0/0/0.tif"
        ):
            with gdal.quiet_errors():
                gdal.Open("/vsimem/test.json")

    j = copy.deepcopy(j_ori)
    j["asset_templates"]["bands"]["raster:bands"][0] = "invalid"
    with gdaltest.tempfile("/vsimem/test.json", json.dumps(j)):
        with pytest.raises(Exception, match=r"Wrong raster:bands\[0\]"):
            with gdal.quiet_errors():
                gdal.Open("/vsimem/test.json")

    j = copy.deepcopy(j_ori)
    del j["asset_templates"]["bands"]["raster:bands"][0]["data_type"]
    with gdaltest.tempfile("/vsimem/test.json", json.dumps(j)):
        with pytest.raises(Exception, match=r"Wrong raster:bands\[0\].data_type"):
            gdal.Open("/vsimem/test.json")

    j = copy.deepcopy(j_ori)
    j["asset_templates"]["bands"]["raster:bands"][0]["data_type"] = "invalid"
    with gdaltest.tempfile("/vsimem/test.json", json.dumps(j)):
        with pytest.raises(Exception, match=r"Wrong raster:bands\[0\].data_type"):
            gdal.Open("/vsimem/test.json")

    j = copy.deepcopy(j_ori)
    j["asset_templates"]["bands"]["raster:bands"][0]["nodata"] = "invalid"
    with gdaltest.tempfile("/vsimem/test.json", json.dumps(j)):
        with gdal.quiet_errors():
            assert gdal.Open("/vsimem/test.json") is not None

    j = copy.deepcopy(j_ori)
    j["asset_templates"]["bands"]["raster:bands"][0]["nodata"] = ["invalid json object"]
    with gdaltest.tempfile("/vsimem/test.json", json.dumps(j)):
        with gdal.quiet_errors():
            assert gdal.Open("/vsimem/test.json") is not None


###############################################################################
# Test force opening a STACTA file


def test_stacta_force_opening(tmp_vsimem):

    filename = str(tmp_vsimem / "test.foo")

    with open("data/stacta/test.json", "rb") as fsrc:
        with gdaltest.vsi_open(filename, "wb") as fdest:
            fdest.write(fsrc.read(1))
            fdest.write(b" " * (1000 * 1000))
            fdest.write(fsrc.read())

    with pytest.raises(Exception):
        gdal.OpenEx(filename)

    with gdaltest.vsi_open(tmp_vsimem / "WorldCRS84Quad/0/0/0.tif", "wb") as fdest:
        fdest.write(open("data/stacta/WorldCRS84Quad/0/0/0.tif", "rb").read())

    ds = gdal.OpenEx(filename, allowed_drivers=["STACTA"])
    assert ds.GetDriver().GetDescription() == "STACTA"


###############################################################################
# Test force opening a URL as STACTA


def test_stacta_force_opening_url():

    drv = gdal.IdentifyDriverEx("http://example.com", allowed_drivers=["STACTA"])
    assert drv.GetDescription() == "STACTA"


###############################################################################
# Test force opening, but provided file is still not recognized (for good reasons)


def test_stacta_force_opening_no_match():

    drv = gdal.IdentifyDriverEx("data/byte.tif", allowed_drivers=["STACTA"])
    assert drv is None
