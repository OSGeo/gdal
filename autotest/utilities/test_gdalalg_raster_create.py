#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster create' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math

import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["create"]


def test_gdalalg_raster_create_missing_size():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    with pytest.raises(
        Exception,
        match="create: Argument 'size' should be specified, or 'like' dataset should be specified",
    ):
        alg.Run()


def test_gdalalg_raster_create_cannot_create_file():

    alg = get_alg()
    alg["output"] = "/i_do/not/exist/out.tif"
    alg["size"] = [1, 1]
    with pytest.raises(
        Exception,
        match="Attempt to create new tiff file",
    ):
        alg.Run()


def test_gdalalg_raster_create_cannot_guess_format():

    alg = get_alg()
    alg["output"] = "/i_do/not/exist/out.unk"
    alg["size"] = [1, 1]
    with pytest.raises(
        Exception,
        match="Cannot guess driver for /i_do/not/exist/out.unk",
    ):
        alg.Run()


def test_gdalalg_raster_create_minimal():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["size"] = [2, 3]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 3
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert ds.GetRasterBand(1).GetNoDataValue() is None


def test_gdalalg_raster_create_burn_invalid_count():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["size"] = [1, 1]
    alg["burn"] = [1, 2]
    with pytest.raises(
        Exception,
        match="One value should be provided for argument 'burn', given there is one band",
    ):
        alg.Run()


def test_gdalalg_raster_create_burn_invalid_count_bis():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["size"] = [1, 1]
    alg["band-count"] = 3
    alg["burn"] = [1, 2]
    with pytest.raises(
        Exception,
        match="3 values should be provided for argument 'burn', given there are 3 bands",
    ):
        alg.Run()


def test_gdalalg_raster_create_overwrite(tmp_vsimem):

    out_filename = tmp_vsimem / "test.tif"

    alg = get_alg()
    alg["output"] = out_filename
    alg["size"] = [1, 1]
    assert alg.Run()
    assert alg.Finalize()

    with gdal.Open(out_filename) as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 1
        assert ds.RasterYSize == 1

    alg = get_alg()
    alg["output"] = out_filename
    alg["size"] = [1, 1]
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()

    alg = get_alg()
    alg["output"] = out_filename
    alg["size"] = [2, 3]
    alg["append"] = True
    assert alg.Run()
    assert alg.Finalize()

    with gdal.Open(out_filename) as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 1
        assert ds.RasterYSize == 1
        assert len(ds.GetSubDatasets()) == 2

    alg = get_alg()
    alg["output"] = out_filename
    alg["size"] = [2, 3]
    alg["overwrite"] = True
    assert alg.Run()
    assert alg.Finalize()

    with gdal.Open(out_filename) as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 2
        assert ds.RasterYSize == 3
        assert len(ds.GetSubDatasets()) == 0


@pytest.mark.require_driver("HFA")
def test_gdalalg_raster_create_failed_append(tmp_vsimem):

    out_filename = tmp_vsimem / "test.img"

    alg = get_alg()
    alg["output"] = out_filename
    alg["output-format"] = "HFA"
    alg["size"] = [1, 1]
    assert alg.Run()
    assert alg.Finalize()

    alg = get_alg()
    alg["output"] = out_filename
    alg["output-format"] = "HFA"
    alg["size"] = [1, 1]
    alg["append"] = True
    with pytest.raises(
        Exception,
        match="-append option not supported for driver HFA",
    ):
        alg.Run()


def get_ref_ds():
    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["size"] = [2, 4]
    alg["band-count"] = 2
    alg["nodata"] = 255
    alg["burn"] = [253, 254]
    alg["datatype"] = "UInt16"
    alg["crs"] = "WGS84"
    alg["bbox"] = [2, 49, 3, 50]
    alg["metadata"] = {"key": "value"}
    assert alg.Run()
    ds = alg["output"].GetDataset()
    ds.BuildOverviews("NEAR", [2])
    ds.GetRasterBand(1).SetMetadataItem("foo", "bar")
    return ds


def test_gdalalg_raster_create_full():

    ds = get_ref_ds()
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 4
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetNoDataValue() == 255
    assert (
        ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_UInt8) == b"\xfd"
    )
    assert (
        ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_UInt8) == b"\xfe"
    )
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"
    assert ds.GetGeoTransform() == (2.0, 0.5, 0.0, 50.0, 0.0, -0.25)
    assert ds.GetMetadataItem("key") == "value"


def test_gdalalg_raster_create_copy():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 4
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetNoDataValue() == 255
    assert (
        ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_UInt8) == b"\x00"
    )
    assert (
        ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_UInt8) == b"\x00"
    )
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"
    assert ds.GetGeoTransform() == (2.0, 0.5, 0.0, 50.0, 0.0, -0.25)
    assert ds.GetMetadataItem("key") is None
    assert ds.GetRasterBand(1).GetMetadataItem("foo") is None
    assert ds.GetRasterBand(1).GetOverviewCount() == 0


def test_gdalalg_raster_create_copy_metadata_missing_input():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["copy-metadata"] = True
    alg["size"] = [1, 1]
    with pytest.raises(
        Exception,
        match="Argument 'copy-metadata' can only be set when an input dataset is set",
    ):
        alg.Run()


def test_gdalalg_raster_create_copy_metadata():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["copy-metadata"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetMetadataItem("key") == "value"
    assert ds.GetRasterBand(1).GetMetadataItem("foo") == "bar"


def test_gdalalg_raster_create_copy_overviews_missing_input():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["copy-overviews"] = True
    alg["size"] = [1, 1]
    with pytest.raises(
        Exception,
        match="Argument 'copy-overviews' can only be set when an input dataset is set",
    ):
        alg.Run()


def test_gdalalg_raster_create_copy_overviews_not_same_size():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["copy-overviews"] = True
    alg["size"] = [1, 1]
    with pytest.raises(
        Exception,
        match="Argument 'copy-overviews' can only be set when the input and output datasets have the same dimension",
    ):
        alg.Run()


def test_gdalalg_raster_create_overviews():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["copy-overviews"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).GetOverviewCount() == 1


def test_gdalalg_raster_create_copy_unset_crs():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["crs"] = "none"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetSpatialRef() is None


def test_gdalalg_raster_create_copy_unset_nodata():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["nodata"] = "none"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).GetNoDataValue() is None


def test_gdalalg_raster_create_copy_nodata_nan():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["size"] = [1, 1]
    alg["datatype"] = "Float64"
    alg["nodata"] = "nan"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert math.isnan(ds.GetRasterBand(1).GetNoDataValue())


def test_gdalalg_raster_create_copy_nodata_out_of_range():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["size"] = [1, 1]
    alg["nodata"] = "nan"
    with pytest.raises(
        Exception,
        match="Setting nodata value failed as it cannot be represented on its data type",
    ):
        alg.Run()


def test_gdalalg_raster_create_copy_override_size():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["size"] = [1, 1]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 1
    assert ds.RasterCount == 2


def test_gdalalg_raster_create_copy_override_band_count():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["band-count"] = 1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 4
    assert ds.RasterCount == 1


def test_gdalalg_raster_create_copy_override_datatype():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["datatype"] = "Byte"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8


def test_gdalalg_raster_create_copy_override_crs():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["crs"] = "EPSG:32631"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32631"


def test_gdalalg_raster_create_copy_override_bbox():

    alg = get_alg()
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["input"] = get_ref_ds()
    alg["bbox"] = [1, 47, 2, 48]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetGeoTransform() == (1.0, 0.5, 0.0, 48.0, 0.0, -0.25)


def test_gdalalg_raster_create_creation_option(tmp_vsimem):

    alg = get_alg()
    alg["output"] = tmp_vsimem / "out.tif"
    alg["size"] = [1, 1]
    alg["creation-option"] = {"COMPRESS": "LZW"}
    assert alg.Run()
    assert alg.Finalize()
    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"


def test_gdalalg_raster_create_overwrite_mem_file_with_real_file_same_name(tmp_vsimem):

    out_filename = tmp_vsimem / "out.tif"

    gdal.FileFromMemBuffer(out_filename, "")

    alg = get_alg()
    alg["output"] = out_filename
    alg["output-format"] = "MEM"
    alg["size"] = [1, 1]
    alg["overwrite"] = True
    assert alg.Run()

    assert gdal.VSIStatL(out_filename) is not None


def test_gdalalg_raster_create_driver_not_available():

    alg = get_alg()
    alg["output-format"] = "MEM"
    alg["size"] = [1, 1]

    # Totally non-nominal !
    drv = gdal.GetDriverByName("MEM")
    drv.Deregister()
    try:
        with pytest.raises(Exception, match="Driver 'MEM' does not exist"):
            alg.Run()
    finally:
        drv.Register()


@pytest.mark.require_driver("NITF")
def test_gdalalg_raster_set_crs_failed(tmp_vsimem):

    alg = get_alg()
    alg["output"] = tmp_vsimem / "out.ntf"
    alg["crs"] = "+proj=ortho +datum=WGS84"
    alg["size"] = [1, 1]

    with pytest.raises(
        Exception, match="NITF only supports WGS84 geographic and UTM projections"
    ):
        alg.Run()


def test_gdalalg_raster_set_bbox_failed_because_null_dimension():

    alg = get_alg()
    alg["output-format"] = "MEM"
    alg["bbox"] = [0, 0, 0, 0]
    alg["size"] = [0, 0]

    with pytest.raises(
        Exception,
        match="Cannot set extent because one of dataset height or width is null",
    ):
        alg.Run()


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_set_bbox_failed(tmp_vsimem):

    alg = get_alg()
    alg["output"] = tmp_vsimem / "out.gpkg"
    alg["bbox"] = [0, 0, 0, 0]
    alg["size"] = [1, 1]

    with pytest.raises(Exception, match="Setting extent failed"):
        alg.Run()


def test_gdalalg_raster_create_empty_bbox():

    alg = get_alg()
    alg["output-format"] = "MEM"
    with pytest.raises(
        Exception,
        match="0 value has been specified for argument 'bbox', whereas exactly 4 were expected",
    ):
        alg["bbox"] = []
    alg["size"] = [1, 1]

    with pytest.raises(
        Exception,
        match="0 value has been specified for argument 'bbox', whereas exactly 4 were expected",
    ):
        alg.Run()


def test_gdalalg_raster_create_pipeline_first_step():

    with gdal.Run(
        "raster pipeline",
        output_format="MEM",
        pipeline="create --bbox -10,-10,10,10 --size 2,2 ! write",
    ) as alg:
        assert alg.Output().GetGeoTransform() == (-10, 10, 0, 10, 0, -10)


def test_gdalalg_raster_create_pipeline_middle_step():

    with gdal.Run(
        "raster pipeline",
        output_format="MEM",
        pipeline="read ../gcore/data/byte.tif ! create --burn 7 ! write",
    ) as alg:
        ds = alg.Output()
        assert ds.RasterXSize == 20
        assert ds.RasterYSize == 20
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (7, 7)
