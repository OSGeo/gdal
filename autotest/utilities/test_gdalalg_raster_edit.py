#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster edit' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_edit_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["edit"]


def test_gdalalg_raster_edit_read_only(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    alg["dataset"] = gdal.OpenEx(tmp_filename)
    with pytest.raises(
        Exception, match="edit: Dataset should be opened in update mode"
    ):
        alg.Run()


def test_gdalalg_raster_edit_crs(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--crs=EPSG:32611",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "32611"


def test_gdalalg_raster_edit_crs_none(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--crs=none",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetSpatialRef() is None


def test_gdalalg_raster_edit_bbox(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--bbox=1,2,10,200",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetGeoTransform() == pytest.approx((1.0, 0.45, 0.0, 200.0, 0.0, -9.9))


def test_gdalalg_raster_edit_bbox_invalid(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    with pytest.raises(
        Exception,
        match="Value of 'bbox' should be xmin,ymin,xmax,ymax with xmin <= xmax and ymin <= ymax",
    ):
        alg.ParseRunAndFinalize(
            [
                "--bbox=1,200,10,2",
                tmp_filename,
            ]
        )


def test_gdalalg_raster_edit_nodata(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--nodata=100",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetNoDataValue() == 100

    alg = get_edit_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--nodata=none",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetNoDataValue() is None


def test_gdalalg_raster_edit_nodata_invalid(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    with pytest.raises(
        Exception,
        match="Value of 'nodata' should be 'none', a numeric value, 'nan', 'inf' or '-inf'",
    ):
        alg.ParseRunAndFinalize(
            [
                "--nodata=invalid",
                tmp_filename,
            ]
        )


def test_gdalalg_raster_edit_metadata(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    alg = get_edit_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--metadata",
            "foo=bar",
            "--metadata",
            "bar=baz",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetMetadata() == {"AREA_OR_POINT": "Area", "foo": "bar", "bar": "baz"}

    alg = get_edit_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--unset-metadata",
            "foo",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetMetadata() == {"AREA_OR_POINT": "Area", "bar": "baz"}


def test_gdalalg_raster_edit_stats():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")

    alg = get_edit_alg()
    alg["dataset"] = src_ds
    alg["stats"] = True
    assert alg.Run()

    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") == "1"
    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MAXIMUM") == "2"
    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MEAN") == "1.5"
    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_STDDEV") == "0.5"


def test_gdalalg_raster_edit_approx_stats():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")

    alg = get_edit_alg()
    alg["dataset"] = src_ds
    alg["approx-stats"] = True
    assert alg.Run()

    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") == "1"
    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MAXIMUM") == "2"
    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MEAN") == "1.5"
    assert src_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_STDDEV") == "0.5"


def test_gdalalg_raster_edit_hist(tmp_vsimem):

    tmp_filename = tmp_vsimem / "out.tif"
    with gdal.GetDriverByName("GTiff").Create(tmp_filename, 2, 1) as ds:
        ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")

    with gdal.Open(tmp_filename) as ds:
        alg = get_edit_alg()
        alg["dataset"] = ds
        alg["hist"] = True
        alg["auxiliary"] = True
        assert alg.Run()
        assert alg.Finalize()

    assert gdal.VSIStatL(str(tmp_filename) + ".aux.xml") is not None
    with gdal.VSIFile(str(tmp_filename) + ".aux.xml", "rb") as f:
        assert b"<Histograms>" in f.read()


def get_pipeline_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("pipeline")


def test_gdalalg_raster_pipeline_edit_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "edit",
            "--crs=EPSG:32611",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "32611"
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_edit_crs_none(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "edit",
            "--crs=none",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetSpatialRef() is None


def test_gdalalg_raster_pipeline_edit_bbox(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "edit",
            "--bbox=1,2,10,200",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetGeoTransform() == pytest.approx((1.0, 0.45, 0.0, 200.0, 0.0, -9.9))


def test_gdalalg_raster_pipeline_edit_nodata(tmp_vsimem):

    out_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(out_filename, open("../gcore/data/byte.tif", "rb").read())

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "edit",
            "--nodata=100",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).GetNoDataValue() == 100

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "edit",
            "--nodata=none",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).GetNoDataValue() is None


def test_gdalalg_raster_pipeline_edit_metadata(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "edit",
            "--metadata=foo=bar,bar=baz",
            "!",
            "edit",
            "--unset-metadata=foo",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetMetadata() == {"AREA_OR_POINT": "Area", "bar": "baz"}
