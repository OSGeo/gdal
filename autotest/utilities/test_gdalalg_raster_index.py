#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster index' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os

import gdaltest
import pytest

from osgeo import gdal, ogr, osr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["index"]


def test_gdalalg_raster_index_layer_must_be_specified():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    with pytest.raises(Exception, match="Argument 'layer' must be specified"):
        alg.Run()


def test_gdalalg_raster_index():

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("my_layer")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "location"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert f["location"] == "../gcore/data/byte.tif"
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((440720 3751320,441920 3751320,441920 3750120,440720 3750120,440720 3751320))"
    )


def test_gdalalg_raster_index_source_by_ref():

    alg = get_alg()
    alg["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    with pytest.raises(
        Exception, match="Input datasets must be provided by name, not as object"
    ):
        alg.Run()


def test_gdalalg_raster_index_overwrite(tmp_vsimem):

    out_filename = tmp_vsimem / "out.shp"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    alg["update"] = True
    with pytest.raises(
        Exception,
        match="Layer 'out' already exists. Specify the --overwrite-layer option to overwrite it, or --append to append to it.",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    alg["append"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 2
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["overwrite-layer"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/uint16.tif"
    alg["output"] = out_filename
    alg["append"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 2
    assert alg.Finalize()
    ds.Close()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["overwrite"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    assert alg.Finalize()
    ds.Close()


def test_gdalalg_raster_index_recursive_filter_absolute_path_location_name():

    alg = get_alg()
    alg["input"] = "../gcore/data"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["recursive"] = True
    alg["filename-filter"] = "byt?.tif"
    alg["absolute-path"] = True
    alg["location-name"] = "path"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "path"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert "byte.tif" in f["path"]
    assert not f["path"].startswith("../gcore")
    assert os.path.exists(f["path"])


def test_gdalalg_raster_index_metadata():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["metadata"] = {"foo": "bar"}
    alg["filename-filter"] = "byte.tif"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadataItem("foo") == "bar"


@pytest.mark.parametrize("min_pixel_size,expected_count", [(61, 0), (59, 1)])
def test_gdalalg_raster_index_min_pixel_size(min_pixel_size, expected_count):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["min-pixel-size"] = min_pixel_size
    with gdaltest.error_raised(
        gdal.CE_Warning if min_pixel_size == 61 else gdal.CE_None
    ):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == expected_count


def test_gdalalg_raster_index_crs():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["dst-crs"] = "EPSG:4267"
    alg["source-crs-field-name"] = "source_crs"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4267"
    f = lyr.GetNextFeature()
    assert f["source_crs"] == "EPSG:26711"
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((-117.641168620797 33.9023526904272,-117.628190189534 33.9024195619211,-117.628110837847 33.8915970129623,-117.641087629972 33.8915301685907,-117.641168620797 33.9023526904272))"
    )


def test_gdalalg_raster_index_error():

    alg = get_alg()
    alg["input"] = "/i/do/not/exist"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["dst-crs"] = "EPSG:4267"
    with pytest.raises(Exception, match="Unable to open /i/do/not/exist"):
        alg.Run()


def test_gdalalg_raster_index_skip_errors_with_crs():

    alg = get_alg()
    alg["input"] = "/i/do/not/exist"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["dst-crs"] = "EPSG:4267"
    alg["skip-errors"] = True
    with gdaltest.error_raised(gdal.CE_Warning):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0


def test_gdalalg_raster_index_skip_errors_without_crs():

    alg = get_alg()
    alg["input"] = "/i/do/not/exist"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "out"
    alg["skip-errors"] = True
    with gdaltest.error_raised(gdal.CE_Warning):
        assert alg.Run()
    ds = alg["output"].GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0


@pytest.mark.require_driver("PARQUET")
@pytest.mark.require_driver("GTI")
def test_gdalalg_raster_index_stac_geoparquet(tmp_vsimem):

    gdal.alg.raster.index(
        input="../gdrivers/data/small_world.tif",
        output=tmp_vsimem / "out.parquet",
        profile="STAC-GeoParquet",
    )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["id"] == "small_world.tif"
        assert f["stac_extensions"] == [
            "https://stac-extensions.github.io/projection/v2.0.0/schema.json",
            "https://stac-extensions.github.io/eo/v2.0.0/schema.json",
        ]
        assert json.loads(f["links"]) == []
        assert f["assets.image.href"] == "../gdrivers/data/small_world.tif"
        assert f["assets.image.roles"] == ["data"]
        assert f["assets.image.title"] is None
        assert f["assets.image.type"] == "image/tiff; application=geotiff"
        assert json.loads(f["bands"]) == [
            {
                "name": "Band 1",
                "eo:common_name": "red",
                "eo:center_wavelength": None,
                "eo:full_width_half_max": None,
                "nodata": None,
                "data_type": "uint8",
                "unit": None,
            },
            {
                "name": "Band 2",
                "eo:common_name": "green",
                "eo:center_wavelength": None,
                "eo:full_width_half_max": None,
                "nodata": None,
                "data_type": "uint8",
                "unit": None,
            },
            {
                "name": "Band 3",
                "eo:common_name": "blue",
                "eo:center_wavelength": None,
                "eo:full_width_half_max": None,
                "nodata": None,
                "data_type": "uint8",
                "unit": None,
            },
        ]
        assert f["proj:code"] == "EPSG:4326"
        assert f["proj:wkt2"] is None
        assert f["proj:projjson"] is None
        assert f["proj:bbox"] == [-180, -90, 180, 90]
        assert f["proj:shape"] == [200, 400]
        assert f["proj:transform"] == [0.9, 0, -180, 0, -0.9, 90, 0, 0, 1]
        assert (
            f.GetGeometryRef().ExportToWkt()
            == "POLYGON ((-180 90,-180 -90,180 -90,180 90,-180 90))"
        )

    with gdal.Open("GTI:" + str(tmp_vsimem / "out.parquet")) as ds:
        assert ds.GetRasterBand(1).GetDescription() == "Band 1"


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_band_metadata(tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in.tif", 1, 1) as ds:
        ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
        b = ds.GetRasterBand(1)
        b.SetDescription("my band")
        b.SetUnitType("meter")
        b.SetNoDataValue(1)
        b.SetMetadataItem("CENTRAL_WAVELENGTH_UM", "1.5", "IMAGERY")
        b.SetMetadataItem("FWHM_UM", "0.5", "IMAGERY")

    gdal.alg.raster.index(
        input=tmp_vsimem / "in.tif",
        output=tmp_vsimem / "out.parquet",
        profile="STAC-GeoParquet",
    )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert json.loads(f["bands"]) == [
            {
                "data_type": "uint8",
                "eo:center_wavelength": 1.5,
                "eo:common_name": None,
                "eo:full_width_half_max": 0.5,
                "name": "my band",
                "nodata": "1",
                "unit": "meter",
            },
        ]

    if gdal.GetDriverByName("GTI"):
        with gdal.Open("GTI:" + str(tmp_vsimem / "out.parquet")) as ds:
            assert ds.GetRasterBand(1).GetMetadata_Dict("IMAGERY") == {
                "CENTRAL_WAVELENGTH_UM": "1.5",
                "FWHM_UM": "0.5",
            }


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_md5(tmp_vsimem):

    gdal.alg.raster.index(
        input="../gcore/data/byte.tif",
        output=tmp_vsimem / "out.parquet",
        profile="STAC-GeoParquet",
        id_method="md5",
    )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["id"] == "5B76BD92A47DE21AB2263A317E0B139C-byte.tif"


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_id_metadata_item(tmp_vsimem):

    gdal.alg.raster.index(
        input="../gcore/data/byte.tif",
        output=tmp_vsimem / "out.parquet",
        profile="STAC-GeoParquet",
        id_metadata_item="AREA_OR_POINT",
    )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["id"] == "Area"


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_base_url(tmp_vsimem):

    gdal.alg.raster.index(
        input="../gcore/data/byte.tif",
        output=tmp_vsimem / "out.parquet",
        profile="STAC-GeoParquet",
        base_url="http://example.com",
    )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
        f = lyr.GetNextFeature()
        assert f["assets.image.href"] == "http://example.com/byte.tif"


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_batch_size(tmp_vsimem):

    with gdal.config_option("GDAL_RASTER_INDEX_BATCH_SIZE", "1"):
        gdal.alg.raster.index(
            input=["../gcore/data/byte.tif", "../gcore/data/uint16.tif"],
            output=tmp_vsimem / "out.parquet",
            profile="STAC-GeoParquet",
            absolute_path=True,
        )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["assets.image.href"].startswith("file://")
        assert f["assets.image.href"].endswith("byte.tif")
        f = lyr.GetNextFeature()
        assert f["assets.image.href"].startswith("file://")
        assert f["assets.image.href"].endswith("uint16.tif")


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_no_proj_code(tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in.tif", 1, 1) as ds:
        ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
        srs = osr.SpatialReference()
        srs.SetFromUserInput("+proj=longlat +ellps=GRS80")
        ds.SetSpatialRef(srs)

    gdal.alg.raster.index(
        input=tmp_vsimem / "in.tif",
        output=tmp_vsimem / "out.parquet",
        profile="STAC-GeoParquet",
    )

    with ogr.Open(tmp_vsimem / "out.parquet") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["proj:code"] is None
        assert f["proj:wkt2"].startswith("GEOGCRS[")
        assert json.loads(f["proj:projjson"])["type"] == "GeographicCRS"


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_error_not_parquet(tmp_vsimem):

    with pytest.raises(
        Exception,
        match="STAC-GeoParquet profile is only compatible with Parquet output format",
    ):
        gdal.alg.raster.index(
            input="../gcore/data/byte.tif",
            output=tmp_vsimem / "out.gpkg",
            profile="STAC-GeoParquet",
        )


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_error_not_parquet_bis(tmp_vsimem):

    with pytest.raises(
        Exception,
        match="STAC-GeoParquet profile is only compatible with Parquet output format",
    ):
        gdal.alg.raster.index(
            input="../gcore/data/byte.tif",
            output=tmp_vsimem / "out.parquet",
            profile="STAC-GeoParquet",
            output_format="GPKG",
        )


@pytest.mark.require_driver("PARQUET")
def test_gdalalg_raster_index_stac_geoparquet_error_not_epsg_4326(tmp_vsimem):

    with pytest.raises(
        Exception,
        match="STAC-GeoParquet profile is only compatible with --dst-crs=EPSG:4326",
    ):
        gdal.alg.raster.index(
            input="../gcore/data/byte.tif",
            output=tmp_vsimem / "out.parquet",
            profile="STAC-GeoParquet",
            dst_crs="EPSG:3857",
        )
