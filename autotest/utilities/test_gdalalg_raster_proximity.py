#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster proximity' testing
# Author:   Alessandro Pasotti <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

gdaltest.importorskip_gdal_array()
np = pytest.importorskip("numpy")


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["proximity"]


# Helper function to create a GTiff raster from a numpy array
def create_gtiff_from_array(
    filename,
    data_array,
    gt=(0, 1, 0, 0, 0, -1),
    srs_wkt=None,
    nodata_val=None,
    dtype=gdal.GDT_Byte,
):
    driver = gdal.GetDriverByName("GTiff")
    rows, cols = data_array.shape
    dataset = driver.Create(str(filename), cols, rows, 2, dtype)
    dataset.SetGeoTransform(gt)
    if srs_wkt:
        dataset.SetProjection(srs_wkt)

    band = dataset.GetRasterBand(1)
    if nodata_val is not None:
        band.SetNoDataValue(nodata_val)
    band.WriteArray(data_array)
    band2 = dataset.GetRasterBand(2)
    band2.WriteArray(np.fliplr(data_array))
    if nodata_val is not None:
        band2.SetNoDataValue(nodata_val)

    dataset.Close()


@pytest.mark.parametrize(
    "options,expected_output_data",
    (
        (
            {
                "datatype": "Byte",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "nodata": 0,
            },
            np.array([[0, 0, 2], [0, 1, 1], [2, 1, 0]], dtype=np.uint8),
        ),
        (
            {
                "datatype": "Float32",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "nodata": 0,
            },
            np.array(
                [[0.0, 0.0, 2.0], [0.0, 1.4142135, 1.0], [2.0, 1.0, 0.0]],
                dtype=np.float32,
            ),
        ),
        # Test with target-values 1 and 3
        (
            {
                "datatype": "Byte",
                "target-values": [1, 3],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "nodata": 0,
            },
            np.array([[0, 1, 2], [1, 1, 1], [2, 1, 0]], dtype=np.uint8),
        ),
        # Test nodata 255
        (
            {
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "nodata": 255,
            },
            np.array([[255, 255, 2], [255, 1.4142135, 1], [2, 1, 0]], dtype=np.float32),
        ),
        # Test DISTANCE_UNITS=GEOGRAPHIC
        (
            {
                "datatype": "Float32",
                "target-values": [1],
                "distance-units": "GEO",
                "max-distance": 2,
                "nodata": 0,
            },
            np.array([[0, 0, 2], [0, 1.4142135, 1], [2, 1, 0]], dtype=np.float32),
        ),
        # Test TILED creation option
        (
            {
                "datatype": "Byte",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "nodata": 0,
                "creation-option": {"TILED": "YES"},
            },
            np.array([[0, 0, 2], [0, 1, 1], [2, 1, 0]], dtype=np.uint8),
        ),
        # Test fixed buffer value
        (
            {
                "datatype": "Byte",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "nodata": 255,
                "fixed-value": 128,
            },
            np.array([[255, 255, 128], [255, 128, 128], [128, 128, 0]], dtype=np.uint8),
        ),
        # Test fixed buffer value without nodata and Byte type
        (
            {
                "datatype": "Byte",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "fixed-value": 128,
            },
            np.array([[255, 255, 128], [255, 128, 128], [128, 128, 0]], dtype=np.uint8),
        ),
        # Test fixed buffer value without nodata and Int8 type
        (
            {
                "datatype": "Int8",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "fixed-value": 1,
            },
            np.array([[127, 127, 1], [127, 1, 1], [1, 1, 0]], dtype=np.int8),
        ),
        # Test fixed buffer value without nodata and Float32 type
        (
            {
                "datatype": "Float32",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "fixed-value": 128,
            },
            np.array(
                [[65535, 65535, 128], [65535, 128, 128], [128, 128, 0]],
                dtype=np.float32,
            ),
        ),
        # Test using band 2
        (
            {
                "band": 2,
                "datatype": "Byte",
                "target-values": [1],
                "distance-units": "PIXEL",
                "max-distance": 2,
                "nodata": 0,
            },
            np.array(
                [
                    [
                        2,
                        0,
                        0,
                    ],
                    [1, 1, 0],
                    [0, 1, 2],
                ],
                dtype=np.uint8,
            ),
        ),
    ),
)
@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_proximity_options(tmp_vsimem, options, expected_output_data):
    """Test proximity calculation with several options."""
    input_data = np.array([[3, 0, 0], [0, 0, 0], [0, 0, 1]], dtype=np.uint8)
    src_filename = tmp_vsimem / "prox_in.tif"
    dst_filename_file = tmp_vsimem / "prox_out.tif"

    create_gtiff_from_array(
        src_filename, input_data, gt=(10, 1, 0, 45, 0, -1), srs_wkt="EPSG:4326"
    )
    src_ds_opened = gdal.Open(str(src_filename))

    # Test with file output
    alg = get_alg()
    alg["input"] = str(src_filename)
    alg["output"] = str(dst_filename_file)

    for k, v in options.items():
        alg[k] = v

    assert alg.Run()
    assert alg.Finalize()

    try:
        data_type = gdal.GetDataTypeByName(options["datatype"])
    except KeyError:
        data_type = gdal.GDT_Float32

    out_ds = gdal.Open(str(dst_filename_file))
    band_out = out_ds.GetRasterBand(1)
    assert band_out.DataType == data_type
    assert out_ds is not None
    output_data_file = out_ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(output_data_file, expected_output_data, atol=1e-6)
    assert out_ds.GetGeoTransform() == src_ds_opened.GetGeoTransform()
    assert out_ds.GetProjection() == src_ds_opened.GetProjection()
    if (
        "creation-option" in options
        and "TILED" in options["creation-option"]
        and options["creation-option"]["TILED"] == "YES"
    ):
        assert band_out.GetBlockSize() == [256, 256]
    del out_ds


@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_proximity_overwrite(tmp_vsimem):
    """Test the overwrite flag."""
    input_data = np.array([[0, 1, 0]], dtype=np.uint8)
    src_filename = tmp_vsimem / "prox_in_ow.tif"
    dst_filename = tmp_vsimem / "prox_out_ow.tif"
    create_gtiff_from_array(src_filename, input_data)

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = str(src_filename)
    alg["output"] = str(dst_filename)
    alg["target-values"] = [1]
    assert alg.Run(my_progress)
    assert alg.Finalize()

    assert tab_pct[0] == 1.0

    alg2 = get_alg()
    alg2["input"] = str(src_filename)
    alg2["output"] = str(dst_filename)
    alg2["target-values"] = [1]
    with pytest.raises(Exception, match="already exists"):
        alg2.Run()

    alg3 = get_alg()
    alg3["input"] = str(src_filename)
    alg3["output"] = str(dst_filename)
    alg3["target-values"] = [1]
    alg3["overwrite"] = True
    assert alg3.Run()
    assert alg3.Finalize()


@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_proximity_respect_input_nodata(tmp_vsimem):
    """Test the nodata value in the input dataset are not included in the proximity."""

    input_data = np.array([[3, 0, 255], [0, 0, 0], [0, 0, 1]], dtype=np.uint8)
    src_filename = tmp_vsimem / "prox_in_nodata.tif"
    dst_filename = tmp_vsimem / "prox_out_nodata.tif"
    create_gtiff_from_array(src_filename, input_data, nodata_val=255)

    expected_output_data = np.array(
        [[128, 128, 128], [128, 1, 1], [2, 1, 0]], dtype=np.uint8
    )

    alg = get_alg()
    alg["input"] = str(src_filename)
    alg["output"] = str(dst_filename)
    alg["target-values"] = [1]
    alg["distance-units"] = "PIXEL"
    alg["max-distance"] = 2
    alg["datatype"] = "Byte"
    alg["nodata"] = 128
    assert alg.Run()
    assert alg.Finalize()

    out_ds = gdal.Open(str(dst_filename))
    output_data = out_ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(output_data, expected_output_data, atol=1e-6)
    out_ds = None


@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_proximity_in_pipeline_invalid_band():

    with pytest.raises(
        Exception,
        match="proximity: Value of 'band' should be greater or equal than 1 and less or equal than 1",
    ):
        gdal.Run(
            "raster",
            "pipeline",
            pipeline="read ../gcore/data/byte.tif ! proximity --band 2 ! write --of=stream streamed_dataset",
        )


@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_proximity_cannot_create_temp_file(tmp_path, tmp_vsimem):

    input_data = np.array([[3, 0, 255], [0, 0, 0], [0, 0, 1]], dtype=np.uint8)
    src_filename = tmp_vsimem / "prox_in_nodata.tif"
    dst_filename = tmp_vsimem / "prox_out_nodata.tif"
    create_gtiff_from_array(src_filename, input_data, nodata_val=255)

    alg = get_alg()
    alg["input"] = str(src_filename)
    alg["output"] = str(dst_filename)
    alg["target-values"] = [1]
    alg["distance-units"] = "PIXEL"
    alg["max-distance"] = 2
    alg["datatype"] = "Byte"
    alg["nodata"] = 128
    with gdaltest.config_options(
        {
            "GDAL_RASTER_PIPELINE_USE_GTIFF_FOR_TEMP_DATASET": "YES",
            "CPL_TMPDIR": "/i_do/not/exist",
        }
    ):
        with pytest.raises(Exception):
            alg.Run()
