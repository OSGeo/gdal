#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster clip' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["clip"]


def test_gdalalg_raster_clip_missing_bbox_or_like():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    with pytest.raises(
        Exception, match="clip: --bbox, --geometry or --like must be specified"
    ):
        alg.Run()


def test_gdalalg_raster_clip_input_error():

    alg = get_alg()
    alg["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["bbox"] = [440780, 3750200, 441860, 3751260]
    with pytest.raises(
        Exception, match="Clipping is not supported on a raster without a geotransform"
    ):
        alg.Run()

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([2, 1, -1, 49, -1, -1])

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["bbox"] = [440780, 3750200, 441860, 3751260]
    with pytest.raises(
        Exception,
        match="Clipping is not supported on a raster whose geotransform has rotation terms",
    ):
        alg.Run()


def test_gdalalg_raster_clip_bbox():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["bbox"] = [440780, 3750200, 441860, 3751260]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 18
    assert ds.RasterYSize == 18
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert ds.GetGeoTransform() == pytest.approx(
        (440780, 60, 0, 3751260, 0, -60), rel=1e-8
    )
    assert ds.GetRasterBand(1).Checksum() == 3695


def test_gdalalg_raster_clip_like():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["bbox"] = [440780, 3750200, 441860, 3751260]
    assert alg.Run()
    ds = alg["output"].GetDataset()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["like"] = ds
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 18
    assert ds.RasterYSize == 18
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert ds.GetGeoTransform() == pytest.approx(
        (440780, 60, 0, 3751260, 0, -60), rel=1e-8
    )
    assert ds.GetRasterBand(1).Checksum() == 3695


def test_gdalalg_raster_clip_like_error(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["like"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(Exception, match="has no geotransform matrix"):
        alg.Run()

    like_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    like_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["like"] = like_ds
    with pytest.raises(Exception, match="has no CRS"):
        alg.Run()


@pytest.mark.parametrize("allow_bbox_outside_source", (True, False))
@pytest.mark.parametrize("bbox_pos", ("partially outside", "completely outside"))
def test_gdalalg_raster_clip_bbox_outside_source(bbox_pos, allow_bbox_outside_source):

    alg = get_alg()
    if allow_bbox_outside_source:
        alg["allow-bbox-outside-source"] = True
    if bbox_pos == "partially outside":
        alg["bbox"] = [0, 0, 441920, 3751320]
    elif bbox_pos == "completely outside":
        alg["bbox"] = [0, 100, 0, 100]

    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    if allow_bbox_outside_source:
        with gdaltest.error_raised(gdal.CE_Warning):
            assert alg.Run()
    else:
        with pytest.raises(Exception, match=bbox_pos):
            alg.Run()


def test_gdalalg_raster_clip_bbox_crs():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["bbox"] = [-117.631, 33.89, -117.628, 33.9005]
    alg["bbox-crs"] = "NAD27"
    alg["allow-bbox-outside-source"] = True
    with gdaltest.error_raised(gdal.CE_Warning):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 6
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert ds.GetGeoTransform() == pytest.approx(
        (441620.0, 60.0, 0.0, 3751140.0, 0.0, -60.0), rel=1e-8
    )


def test_gdalalg_raster_clip_geometry_add_alpha():

    src_ds = gdal.Open("../gcore/data/byte.tif")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg[
        "geometry"
    ] = "POLYGON ((440720 3750120,441920 3751320,441920 3750120,440720 3750120))"
    alg["add-alpha"] = True
    with gdaltest.error_raised(gdal.CE_Warning):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert ds.GetGeoTransform() == pytest.approx(src_ds.GetGeoTransform(), rel=1e-8)
    assert ds.GetRasterBand(1).ReadRaster(0, 0, 10, 10) == b"\x00" * 100
    assert ds.GetRasterBand(1).ReadRaster(10, 10, 10, 10) == src_ds.ReadRaster(
        10, 10, 10, 10
    )
    assert ds.GetRasterBand(2).ReadRaster(0, 0, 10, 10) == b"\x00" * 100
    assert ds.GetRasterBand(2).ReadRaster(10, 10, 10, 10) == b"\xFF" * 100


def test_gdalalg_raster_clip_geometry_nodata():

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    src_ds.GetRasterBand(1).SetNoDataValue(255)
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg[
        "geometry"
    ] = "POLYGON ((440720 3750120,441920 3751320,441920 3750120,440720 3750120))"
    with gdaltest.error_raised(gdal.CE_Warning):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetRasterBand(1).ReadRaster(0, 0, 10, 10) == b"\xFF" * 100
    assert ds.GetRasterBand(1).ReadRaster(10, 10, 10, 10) == src_ds.ReadRaster(
        10, 10, 10, 10
    )


def test_gdalalg_raster_clip_wrong_geometry():

    src_ds = gdal.Open("../gcore/data/byte.tif")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["geometry"] = "invalid"
    with pytest.raises(
        Exception, match="Clipping geometry is neither a valid WKT or GeoJSON geometry"
    ):
        alg.Run()


def test_gdalalg_raster_clip_geometry_upside_down():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 2)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    src_ds.WriteRaster(0, 0, 1, 2, b"\x00\xFF")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["bbox"] = [0, 0, 1, 2]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.ReadRaster() == b"\x00\xFF"
    assert ds.GetGeoTransform() == pytest.approx(src_ds.GetGeoTransform(), rel=1e-8)


def test_gdalalg_raster_clip_geometry_only_bbox():

    src_ds = gdal.Open("../gcore/data/byte.tif")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg[
        "geometry"
    ] = "POLYGON ((440720 3750120,441920 3751320,441920 3750120,440720 3750120))"
    alg["only-bbox"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_clip_geometry_srs():

    x1 = -117.641168620797
    y1 = 33.9023526904272
    x2 = -117.628110837847
    y2 = 33.8915970129623
    src_ds = gdal.Open("../gcore/data/byte.tif")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["geometry"] = f"POLYGON (({x1} {y1},{x1} {y2},{x2} {y2},{x2} {y1},{x1} {y1}))"
    alg["geometry-crs"] = "EPSG:4267"
    alg["add-alpha"] = True
    alg["allow-bbox-outside-source"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 22
    assert ds.RasterYSize == 20
    assert ds.GetRasterBand(1).Checksum() == 4851


@pytest.mark.parametrize("allow_bbox_outside_source", [True, False])
def test_gdalalg_raster_clip_geometry_outside_extent(allow_bbox_outside_source):

    src_ds = gdal.Open("../gcore/data/byte.tif")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg[
        "geometry"
    ] = "POLYGON ((440600 3750120,441920 3751320,441920 3750120,440600 3750120))"
    if allow_bbox_outside_source:
        alg["allow-bbox-outside-source"] = True
        with gdaltest.error_raised(gdal.CE_Warning):
            assert alg.Run()
        ds = alg["output"].GetDataset()
        assert ds.RasterXSize == 22
        assert ds.RasterYSize == 20
    else:
        with pytest.raises(
            Exception,
            match="Clipping geometry is partially or totally outside the extent of the raster",
        ):
            alg.Run()
