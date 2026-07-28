#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal pipeline external' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_path() is None, reason="gdal binary not available"
)


def test_gdalalg_external_pipeline_simple_raster(tmp_vsimem, tmp_path):
    gdal_path = test_cli_utilities.get_gdal_path()

    gdal.Mkdir(tmp_vsimem / "temp_files", 0o755)
    before = gdal.ReadDir(tmp_vsimem / "temp_files")
    before_tmp_path = gdal.ReadDir(tmp_path)
    with gdal.config_options(
        {"GDAL_ENABLE_EXTERNAL": "YES", "CPL_TMPDIR": str(tmp_path)}
    ):
        with gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} raster reproject --dst-crs=EPSG:4326 <INPUT> <OUTPUT>" ! write {tmp_vsimem}/out.tif'
        ):
            pass

    assert gdal.ReadDir(tmp_vsimem / "temp_files") == before
    assert gdal.ReadDir(tmp_path) == before_tmp_path

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode() == "4326"


def test_gdalalg_external_last_step():
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        with gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} raster reproject --dst-crs=EPSG:4326 <INPUT> <OUTPUT>"'
        ) as alg:
            ds = alg.Output()
            assert ds.GetSpatialRef().GetAuthorityCode() == "4326"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_external_first_step(tmp_vsimem):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        with gdal.alg.pipeline(
            pipeline=f'external "{gdal_path} vsi cp ../gcore/data/byte.tif <OUTPUT>" ! write --format MEM unnamed'
        ) as alg:
            ds = alg.Output()
            assert ds.GetSpatialRef().GetAuthorityCode() == "26711"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_external_first_and_last_step(tmp_vsimem):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        with gdal.alg.pipeline(
            pipeline=f'external "{gdal_path} vsi cp ../gcore/data/byte.tif <OUTPUT>"'
        ) as alg:
            ds = alg.Output()
            assert ds.GetSpatialRef().GetAuthorityCode() == "26711"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_external_with_tee(tmp_vsimem):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        with gdal.alg.pipeline(
            pipeline=f'external "{gdal_path} vsi cp ../gcore/data/byte.tif <OUTPUT>" ! tee [ write {tmp_vsimem}/aux.tif ] ! write --format MEM unnamed'
        ) as alg:
            ds = alg.Output()
            assert ds.GetSpatialRef().GetAuthorityCode() == "26711"

    ds = gdal.Open(tmp_vsimem / "aux.tif")
    assert ds.GetSpatialRef().GetAuthorityCode() == "26711"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_external_pipeline_simple_vector(tmp_vsimem):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        assert gdal.alg.pipeline(
            pipeline=f'read ../ogr/data/poly.shp ! external "{gdal_path} vector reproject --dst-crs=EPSG:4326 <INPUT> <OUTPUT>" ! write {tmp_vsimem}/out.gpkg'
        )

    with gdal.Open(tmp_vsimem / "out.gpkg") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode() == "4326"


def test_gdalalg_external_raster_pipeline_simple(tmp_vsimem):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        assert gdal.alg.raster.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} raster reproject --dst-crs=EPSG:4326 <INPUT> <OUTPUT>" ! write {tmp_vsimem}/out.tif'
        )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode() == "4326"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_external_vector_pipeline_simple(tmp_vsimem):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        assert gdal.alg.vector.pipeline(
            pipeline=f'read ../ogr/data/poly.shp ! external "{gdal_path} vector reproject --dst-crs=EPSG:4326 <INPUT> <OUTPUT>" ! write {tmp_vsimem}/out.gpkg'
        )

    with gdal.Open(tmp_vsimem / "out.gpkg") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode() == "4326"


def test_gdalalg_external_input_output(tmp_vsimem):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        assert gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} raster edit --crs=EPSG:4326 <INPUT-OUTPUT>" ! write {tmp_vsimem}/out.tif'
        )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode() == "4326"


def test_gdalalg_external_pipeline_raster_and_clip(tmp_vsimem, tmp_path):
    gdal_path = test_cli_utilities.get_gdal_path()

    gdal.Mkdir(tmp_vsimem / "temp_files", 0o755)
    before = gdal.ReadDir(tmp_vsimem / "temp_files")
    before_tmp_path = gdal.ReadDir(tmp_path)
    with gdal.config_options(
        {"GDAL_ENABLE_EXTERNAL": "YES", "CPL_TMPDIR": str(tmp_path)}
    ):
        with gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} convert <INPUT> <OUTPUT>" ! clip --like ../gcore/data/byte.tif ! write {tmp_vsimem}/out.tif'
        ):
            pass

    assert gdal.ReadDir(tmp_vsimem / "temp_files") == before
    assert gdal.ReadDir(tmp_path) == before_tmp_path

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.require_geos
def test_gdalalg_external_pipeline_vector_and_clip_raster(tmp_vsimem, tmp_path):
    gdal_path = test_cli_utilities.get_gdal_path()

    poly_tif = tmp_vsimem / "poly.tif"
    gdal.alg.vector.rasterize(
        input="../ogr/data/poly.shp", output=poly_tif, size=[100, 100], burn=255
    )

    gdal.Mkdir(tmp_vsimem / "temp_files", 0o755)
    before = gdal.ReadDir(tmp_vsimem / "temp_files")
    before_tmp_path = gdal.ReadDir(tmp_path)
    with gdal.config_options(
        {"GDAL_ENABLE_EXTERNAL": "YES", "CPL_TMPDIR": str(tmp_path)}
    ):
        with gdal.alg.pipeline(
            pipeline=f'read ../ogr/data/poly.shp ! external "{gdal_path} convert <INPUT> <OUTPUT>" ! clip --input {poly_tif} --like _PIPE_ ! write {tmp_vsimem}/out.tif'
        ):
            pass

    assert gdal.ReadDir(tmp_vsimem / "temp_files") == before
    assert gdal.ReadDir(tmp_path) == before_tmp_path

    with gdal.Open(poly_tif) as src_ds, gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()


@pytest.mark.require_geos
def test_gdalalg_external_pipeline_vector_and_clip_raster_with_tee(
    tmp_vsimem, tmp_path
):
    gdal_path = test_cli_utilities.get_gdal_path()

    poly_tif = tmp_vsimem / "poly.tif"
    gdal.alg.vector.rasterize(
        input="../ogr/data/poly.shp", output=poly_tif, size=[100, 100], burn=255
    )

    gdal.Mkdir(tmp_vsimem / "temp_files", 0o755)
    before = gdal.ReadDir(tmp_vsimem / "temp_files")
    before_tmp_path = gdal.ReadDir(tmp_path)
    with gdal.config_options(
        {"GDAL_ENABLE_EXTERNAL": "YES", "CPL_TMPDIR": str(tmp_path)}
    ):
        with gdal.alg.pipeline(
            pipeline=f'read ../ogr/data/poly.shp ! external "{gdal_path} convert <INPUT> <OUTPUT>" ! tee [ write {tmp_vsimem}/out.shp ] ! clip --input {poly_tif} --like _PIPE_ ! write {tmp_vsimem}/out.tif'
        ):
            pass

    assert gdal.ReadDir(tmp_vsimem / "temp_files") == before
    assert gdal.ReadDir(tmp_path) == before_tmp_path

    with gdal.Open(tmp_vsimem / "out.shp") as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10

    with gdal.Open(poly_tif) as src_ds, gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()


@pytest.mark.require_geos
def test_gdalalg_external_pipeline_vector_and_clip_raster_from_inner_pipeline(
    tmp_vsimem, tmp_path
):
    gdal_path = test_cli_utilities.get_gdal_path()

    poly_tif = tmp_vsimem / "poly.tif"
    gdal.alg.vector.rasterize(
        input="../ogr/data/poly.shp", output=poly_tif, size=[100, 100], burn=255
    )

    gdal.Mkdir(tmp_vsimem / "temp_files", 0o755)
    before = gdal.ReadDir(tmp_vsimem / "temp_files")
    before_tmp_path = gdal.ReadDir(tmp_path)
    with gdal.config_options(
        {"GDAL_ENABLE_EXTERNAL": "YES", "CPL_TMPDIR": str(tmp_path)}
    ):
        with gdal.alg.pipeline(
            pipeline=f'read ../ogr/data/poly.shp ! external "{gdal_path} convert <INPUT> <OUTPUT>" ! clip --input [ read {poly_tif} ] --like _PIPE_ ! write {tmp_vsimem}/out.tif'
        ):
            pass

    assert gdal.ReadDir(tmp_vsimem / "temp_files") == before
    assert gdal.ReadDir(tmp_path) == before_tmp_path

    with gdal.Open(poly_tif) as src_ds, gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()


@pytest.mark.require_geos
def test_gdalalg_external_pipeline_raster_and_clip_vector(tmp_vsimem, tmp_path):
    gdal_path = test_cli_utilities.get_gdal_path()

    byte_shp = tmp_vsimem / "byte.shp"
    gdal.alg.raster.polygonize(input="../gcore/data/byte.tif", output=byte_shp)

    gdal.Mkdir(tmp_vsimem / "temp_files", 0o755)
    before = gdal.ReadDir(tmp_vsimem / "temp_files")
    before_tmp_path = gdal.ReadDir(tmp_path)
    with gdal.config_options(
        {"GDAL_ENABLE_EXTERNAL": "YES", "CPL_TMPDIR": str(tmp_path)}
    ):
        with gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} convert <INPUT> <OUTPUT>" ! clip --input {byte_shp} --like _PIPE_ ! write {tmp_vsimem}/out.shp'
        ):
            pass

    assert gdal.ReadDir(tmp_vsimem / "temp_files") == before
    assert gdal.ReadDir(tmp_path) == before_tmp_path

    with gdal.Open(byte_shp) as src_ds, gdal.Open(tmp_vsimem / "out.shp") as ds:
        assert ds.GetLayer(0).GetFeatureCount() == src_ds.GetLayer(0).GetFeatureCount()


@pytest.mark.require_geos
def test_gdalalg_external_pipeline_raster_and_clip_vector_from_inner_pipeline(
    tmp_vsimem, tmp_path
):
    gdal_path = test_cli_utilities.get_gdal_path()

    byte_shp = tmp_vsimem / "byte.shp"
    gdal.alg.raster.polygonize(input="../gcore/data/byte.tif", output=byte_shp)

    gdal.Mkdir(tmp_vsimem / "temp_files", 0o755)
    before = gdal.ReadDir(tmp_vsimem / "temp_files")
    before_tmp_path = gdal.ReadDir(tmp_path)
    with gdal.config_options(
        {"GDAL_ENABLE_EXTERNAL": "YES", "CPL_TMPDIR": str(tmp_path)}
    ):
        with gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} convert <INPUT> <OUTPUT>" ! clip --input [ read {byte_shp} ] --like _PIPE_ ! write {tmp_vsimem}/out.shp'
        ):
            pass

    assert gdal.ReadDir(tmp_vsimem / "temp_files") == before
    assert gdal.ReadDir(tmp_path) == before_tmp_path

    with gdal.Open(byte_shp) as src_ds, gdal.Open(tmp_vsimem / "out.shp") as ds:
        assert ds.GetLayer(0).GetFeatureCount() == src_ds.GetLayer(0).GetFeatureCount()


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("HFA")
def test_gdalalg_external_pipeline_with_user_specified_drivers(tmp_vsimem, tmp_path):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        assert gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external --if=GPKG --of=HFA "{gdal_path} raster reproject --dst-crs=EPSG:4326 <INPUT> <OUTPUT>" ! write {tmp_vsimem}/out.tif'
        )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode() == "4326"


def test_gdalalg_external_pipeline_no_output_dataset_created_by_external_command(
    tmp_vsimem,
):
    gdal_path = test_cli_utilities.get_gdal_path()

    with gdal.config_option("GDAL_ENABLE_EXTERNAL", "YES"):

        gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} --version" ! write {tmp_vsimem}/out.tif'
        )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.RasterXSize == 20
        assert ds.RasterYSize == 20


def test_gdalalg_external_pipeline_errors(tmp_vsimem, tmp_path):
    gdal_path = test_cli_utilities.get_gdal_path()

    with pytest.raises(
        Exception, match="GDAL_ENABLE_EXTERNAL configuration option is not set to YES"
    ):
        gdal.alg.pipeline(
            pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} raster reproject --dst-crs=EPSG:4326 <INPUT> <OUTPUT>" ! write {tmp_vsimem}/out.tif'
        )

    with gdal.config_option("GDAL_ENABLE_EXTERNAL", "YES"):

        with pytest.raises(
            Exception, match="External command '/i_do/not/exist' failed"
        ):
            gdal.alg.pipeline(
                pipeline=f'read ../gcore/data/byte.tif ! external "/i_do/not/exist" ! write {tmp_vsimem}/out.tif'
            )

        with pytest.raises(Exception, match="failed with error code"):
            gdal.alg.pipeline(
                pipeline=f'read ../gcore/data/byte.tif ! external "{gdal_path} info /i/do/not/exist" ! write {tmp_vsimem}/out.tif'
            )

        with pytest.raises(Exception, match="output_"):
            gdal.alg.pipeline(
                pipeline=f'read ../gcore/data/byte.tif ! external "echo <OUTPUT>" ! write {tmp_vsimem}/out.tif'
            )

        with pytest.raises(
            Exception,
            match="/i_do/not/exist', used for temporary directory, is not a valid directory",
        ):
            with gdal.config_option("CPL_TMPDIR", "/i_do/not/exist"):
                gdal.alg.pipeline(
                    pipeline=f'read ../gcore/data/byte.tif ! external "echo <INPUT>" ! write {tmp_vsimem}/out.tif'
                )

        tmpdirname = str(tmp_path / "subdir;echo eheh")
        gdal.Mkdir(tmpdirname, 0o755)
        with pytest.raises(Exception, match="contains a reserved character"):
            with gdal.config_option("CPL_TMPDIR", tmpdirname):
                gdal.alg.pipeline(
                    pipeline=f'read ../gcore/data/byte.tif ! external "echo <INPUT>" ! write {tmp_vsimem}/out.tif'
                )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_external_first_step_failed_to_produce_dataset(tmp_vsimem):

    with gdal.config_options({"GDAL_ENABLE_EXTERNAL": "YES"}):
        with pytest.raises(Exception, match="output_"):
            gdal.alg.pipeline(
                pipeline='external "echo <OUTPUT>" ! write --format MEM unnamed'
            )
