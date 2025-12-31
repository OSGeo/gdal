#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector rasterize' testing
# Author:   Alessandro Pasotti, <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import contextlib

import pytest

from osgeo import gdal


def get_rasterize_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["rasterize"]


# Context manager that creates a temporary file and writes content to it
@contextlib.contextmanager
def temp_cutline(input_csv):

    content = """Counter,HEIGHT,WKT
1,100,"POLYGON((6.25 1.25 100,7.25 1.25 200,7.25 2.25 250,6.25 2.25 254,6.25 1.25 100))"
2,200,"POLYGON((4.25 4.25 220,6.25 4.25 200,6.25 6.25 202,4.25 6.25 200,4.25 4.25 220))"
3,300,"POLYGON((1.001 1.001 200,3.999 3.999 220,3.2 1.6 210,1.001 1.001 200))"
"""
    gdal.FileFromMemBuffer(input_csv, content)
    try:
        yield
    finally:
        gdal.Unlink(input_csv)


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize(
    "create_empty_dataset,options,expected",
    [
        (
            True,
            ["--all-touched", "-b", "3,2,1", "--burn", "200,220,240", "-l", "cutline"],
            "already exists",
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            121,
        ),
        (
            False,
            ["--all-touched", "-b", "3,2,1", "--burn", "200,220,240", "-l", "cutline"],
            "Must specify output resolution (--resolution) or size (--size)",
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            121,
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "--where",
                "Counter='2'",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            46,
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "--sql",
                "SELECT * FROM cutline WHERE Counter='2'",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
            ],
            46,
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "--sql",
                "SELECT * FROM cutline WHERE Counter='2'",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            "Argument 'sql' is mutually exclusive with 'input-layer'.",
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
                "--3d",
            ],
            "Argument '-3d' not allowed with '-burn <value>'",
        ),
        (
            True,
            ["--update", "--all-touched", "-b", "3,2,1", "-l", "cutline", "--3d"],
            101,
        ),
        (
            True,
            [
                "--update",
                "--invert",
                "--all-touched",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            1690,
        ),
        (
            True,
            [
                "--update",
                "--attribute-name",
                "HEIGHT",
                "--all-touched",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            "Argument '-a <attribute_name>' not allowed with '-burn <value>'",
        ),
        (
            True,
            [
                "--update",
                "--attribute-name",
                "__HEIGHT",
                "--all-touched",
                "-b",
                "3,2,1",
                "-l",
                "cutline",
            ],
            "Failed to find field __HEIGHT on layer cutline",
        ),
        (
            True,
            [
                "--update",
                "--attribute-name",
                "HEIGHT",
                "--all-touched",
                "-b",
                "3,2,1",
                "-l",
                "cutline",
            ],
            168,
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "--dialect",
                "SQLITE",
                "--sql",
                "SELECT * FROM cutline WHERE Counter='2'",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
            ],
            46,
        ),
        (
            True,
            [
                "--update",
                "--all-touched",
                "--dialect",
                "XXXXXX",  # No errors: just a warning
                "--sql",
                "SELECT * FROM cutline WHERE Counter='2'",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
            ],
            46,
        ),
        (
            False,
            [
                "--all-touched",
                "--init",
                "100,200,300",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            "Must specify output resolution (--resolution) or size (--size)",
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "10,10",
                "--init",
                "100,200,300",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            "-b option cannot be used when creating a GDAL dataset.",
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "10,10",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            1418,
        ),
        (
            False,
            [
                "--all-touched",
                "--resolution",
                "0.6249,0.5249",
                "--size",
                "10,10",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            "Argument 'size' is mutually exclusive with 'resolution'.",
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "10,10",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            1418,
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "0,10",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            1496,
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "0,10",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "--sql",
                "SELECT * FROM cutline WHERE Counter != 'XXXXX'",
            ],
            1496,
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "0,10",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "--sql",
                "SELECT * FROM cutline WHERE Counter = 'XXXXX'",
            ],
            "Cannot get layer extent",
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "10,0",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            1112,
        ),
        (
            False,
            [
                "--all-touched",
                "--size",
                "10,0",
                "--init",
                "100,200,300",
                "--burn",
                "200,220,240",
                "--sql",
                "SELECT * FROM cutline WHERE Counter != 'XXXXX'",
            ],
            1112,
        ),
        (
            False,
            [
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            500,
        ),
        (
            False,
            [
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--extent",
                "0.64,1.0,7.64,6.50",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            524,
        ),
        (
            False,
            [
                "--tap",
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            497,
        ),
        (
            False,
            [
                "--optimization",
                "XXXXX",
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            "Invalid value 'XXXXX' for string argument 'optimization'. Should be one among 'AUTO', 'RASTER', 'VECTOR'.",
        ),
        (
            False,
            [
                "--optimization",
                "AUTO",
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            500,
        ),
        (
            False,
            [
                "--crs",
                "EPSG:XXXXX",
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            "Invalid value for 'crs' argument",
        ),
        (
            False,
            [
                "--crs",
                "EPSG:4326",
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            500,
        ),
        (
            False,
            [
                "--nodata",
                "-1",
                "--all-touched",
                "--resolution",
                "0.7,0.5",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            431,
        ),
    ],
)
def test_gdalalg_vector_rasterize(tmp_vsimem, create_empty_dataset, options, expected):

    input_csv = str(tmp_vsimem / "cutline.csv")

    options = options.copy()

    with temp_cutline(input_csv):

        output_tif = str(tmp_vsimem / "rasterize_alg_1.tif")
        try:
            gdal.Unlink(output_tif)
        except RuntimeError:
            gdal.ErrorReset()
            pass

        assert gdal.VSIStatL(output_tif) != 0

        if create_empty_dataset:
            # Create a raster to rasterize into.
            target_ds = gdal.GetDriverByName("GTiff").Create(
                output_tif, 12, 12, 3, gdal.GDT_UInt8
            )
            target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))

            # Close TIF file
            target_ds = None

        # Rasterize
        if isinstance(expected, str):
            with pytest.raises(RuntimeError) as error:
                rasterize = get_rasterize_alg()
                rasterize.ParseRunAndFinalize(options + [input_csv, output_tif])
            assert expected in str(error.value)
            return

        else:
            rasterize = get_rasterize_alg()
            assert rasterize.ParseRunAndFinalize(options + [input_csv, output_tif])

        # Check the result
        target_ds = gdal.Open(output_tif)
        checksum = target_ds.GetRasterBand(2).Checksum()
        assert (
            checksum == expected
        ), "Did not get expected image checksum (got %d, expected %s)" % (
            checksum,
            expected,
        )

        if "--crs" in options:
            assert target_ds.GetProjection().startswith('GEOGCS["WGS 84"')
        else:
            assert target_ds.GetProjection() == ""

        if "--nodata" in options:
            assert target_ds.GetRasterBand(2).GetNoDataValue() == -1
        else:
            assert target_ds.GetRasterBand(2).GetNoDataValue() in [None, 0]

        target_ds = None


@pytest.mark.require_driver("CSV")
def test_gdalalg_vector_rasterize_add_option(tmp_vsimem):

    options = [
        "--all-touched",
        "-b",
        "3,2,1",
        "--burn",
        "200,220,240",
    ]

    input_csv = str(tmp_vsimem / "cutline.csv")
    output_tif = str(tmp_vsimem / "rasterize_alg_1.tif")

    # Create a raster to rasterize into.
    target_ds = gdal.GetDriverByName("GTiff").Create(
        output_tif, 12, 12, 3, gdal.GDT_UInt8
    )
    target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))

    # Close TIF file
    target_ds = None

    with temp_cutline(input_csv):

        last_pct = [0]

        def my_progress(pct, msg, user_data):
            last_pct[0] = pct
            return True

        rasterize = get_rasterize_alg()
        assert rasterize.ParseRunAndFinalize(
            options + ["--update", input_csv, output_tif],
            my_progress,
        )
        assert last_pct[0] == 1.0

        with gdal.Open(output_tif) as target_ds:
            checksum = target_ds.GetRasterBand(2).Checksum()

        assert checksum == 121

        rasterize = get_rasterize_alg()
        assert rasterize.ParseRunAndFinalize(options + ["--add", input_csv, output_tif])

        target_ds = gdal.Open(output_tif)
        checksum = target_ds.GetRasterBand(2).Checksum()

        assert checksum == 166


@pytest.mark.require_driver("CSV")
def test_gdalalg_vector_rasterize_dialect_warning(tmp_vsimem):

    expected_warning = "Dialect 'XXXXXX' is unsupported. Only supported dialects are 'OGRSQL', 'SQLITE'. Defaulting to OGRSQL"
    input_csv = str(tmp_vsimem / "cutline.csv")

    with temp_cutline(input_csv):

        options = [
            "--all-touched",
            "--dialect",
            "XXXXXX",  # No error: just a warning
            "--sql",
            "SELECT * FROM cutline WHERE Counter='2'",
            "-b",
            "3,2,1",
            "--burn",
            "200,220,240",
            "--update",
        ]

        output_tif = str(tmp_vsimem / "rasterize_alg_1.tif")

        # Create a raster to rasterize into.
        target_ds = gdal.GetDriverByName("GTiff").Create(
            output_tif, 12, 12, 3, gdal.GDT_UInt8
        )
        target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))

        # Close TIF file
        target_ds = None

        rasterize = get_rasterize_alg()
        assert rasterize.ParseRunAndFinalize(options + [input_csv, output_tif])

        # Check the warning
        assert expected_warning in gdal.GetLastErrorMsg(), gdal.GetLastErrorMsg()


@pytest.mark.require_driver("CSV")
def test_gdalalg_vector_rasterize_overwrite(tmp_vsimem):

    input_csv = str(tmp_vsimem / "cutline.csv")
    with temp_cutline(input_csv):

        output_tif = str(tmp_vsimem / "rasterize_alg_1.tif")

        # Create a raster to rasterize into.
        with gdal.GetDriverByName("GTiff").Create(
            output_tif, 12, 12, 3, gdal.GDT_UInt8
        ) as target_ds:
            target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))

        rasterize = get_rasterize_alg()
        with pytest.raises(
            Exception,
            match="already exists",
        ):
            rasterize.ParseRunAndFinalize(
                ["-b", "1,2,3", "--burn=200,220,240", input_csv, output_tif]
            )

        rasterize = get_rasterize_alg()
        assert rasterize.ParseRunAndFinalize(
            [
                "--co",
                "TILED=YES",
                "--burn=200,220,240",
                input_csv,
                output_tif,
                "--size=10,11",
                "--overwrite",
            ]
        )

        with gdal.Open(output_tif) as ds:
            assert ds.RasterXSize == 10
            assert ds.RasterYSize == 11
            assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]


def test_gdalalg_vector_rasterize_missing_size_and_res():

    rasterize = get_rasterize_alg()
    rasterize["input"] = "../ogr/data/poly.shp"
    rasterize["burn"] = 1
    rasterize["output-format"] = "MEM"

    with pytest.raises(Exception, match="--resolution.*or.*--size"):
        rasterize.Run()


@pytest.mark.require_driver("COG")
def test_gdalalg_vector_rasterize_to_cog(tmp_vsimem):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= last_pct[0]
        last_pct[0] = pct
        return True

    with gdal.alg.vector.rasterize(
        input="../ogr/data/poly.shp",
        output_format="COG",
        output=tmp_vsimem / "out.tif",
        size=[512, 512],
        progress=my_progress,
    ) as alg:
        assert last_pct[0] == 1
        ds = alg.Output()
        assert ds.GetRasterBand(1).Checksum() == 1842
        assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") == "COG"
