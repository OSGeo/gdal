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

    pipeline = get_edit_alg()
    pipeline["dataset"] = gdal.OpenEx(tmp_filename)
    with pytest.raises(
        Exception, match="edit: Dataset should be opened in update mode"
    ):
        pipeline.Run()


def test_gdalalg_raster_edit_crs(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    pipeline = get_edit_alg()
    assert pipeline.ParseRunAndFinalize(
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

    pipeline = get_edit_alg()
    assert pipeline.ParseRunAndFinalize(
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

    pipeline = get_edit_alg()
    assert pipeline.ParseRunAndFinalize(
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

    pipeline = get_edit_alg()
    with pytest.raises(
        Exception,
        match="Value of 'bbox' should be xmin,ymin,xmax,ymax with xmin <= xmax and ymin <= ymax",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "--bbox=1,200,10,2",
                tmp_filename,
            ]
        )


def test_gdalalg_raster_edit_metadata(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(tmp_filename, open("../gcore/data/byte.tif", "rb").read())

    pipeline = get_edit_alg()
    assert pipeline.ParseRunAndFinalize(
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

    pipeline = get_edit_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "--unset-metadata",
            "foo",
            tmp_filename,
        ]
    )

    with gdal.OpenEx(tmp_filename) as ds:
        assert ds.GetMetadata() == {"AREA_OR_POINT": "Area", "bar": "baz"}


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
