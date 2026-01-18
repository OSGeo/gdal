#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster overview' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, osr


def get_overview_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["overview"]


def get_overview_add_alg():
    return get_overview_alg()["add"]


def get_overview_delete_alg():
    return get_overview_alg()["delete"]


def test_gdalalg_overview_invalid_arguments():

    add = get_overview_add_alg()
    with pytest.raises(Exception):
        add["levels"] = [1]

    add = get_overview_add_alg()
    with pytest.raises(Exception):
        add["min-size"] = 0


def test_gdalalg_overview_explicit_level():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["levels"] = [2]
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1087


def test_gdalalg_overview_minsize_and_resampling():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["resampling"] = "average"
    add["min-size"] = 10
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152


def test_gdalalg_overview_reuse_resampling_and_levels(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    ds = gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["resampling"] = "average"
    add["min-size"] = 10
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152

    ds.GetRasterBand(1).GetOverview(0).Fill(0)

    add = get_overview_add_alg()
    add["dataset"] = ds
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152


def test_gdalalg_overview_in_place(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    assert add.ParseRunAndFinalize([tmp_filename, "--levels=2", "--co", "BLOCKSIZE=64"])

    assert gdal.VSIStatL(tmp_filename + ".ovr") is None

    with gdal.Open(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1
        assert ds.GetRasterBand(1).GetOverview(0).GetBlockSize() == [64, 64]


def test_gdalalg_overview_external(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    assert add.ParseRunAndFinalize([tmp_filename, "--levels=2", "--external"])

    assert gdal.VSIStatL(tmp_filename + ".ovr") is not None

    with gdal.Open(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1

    delete = get_overview_delete_alg()
    assert delete.ParseRunAndFinalize([tmp_filename, "--external"])

    assert gdal.VSIStatL(tmp_filename + ".ovr") is None


@pytest.mark.require_driver("GPKG")
def test_gdalalg_overview_external_incompatible(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.gpkg")
    gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    with pytest.raises(
        Exception, match="Driver GPKG does not support external overviews"
    ):
        assert add.ParseRunAndFinalize([tmp_filename, "--levels=2", "--external"])


@pytest.mark.require_driver("PNG")
def test_gdalalg_overview_external_other_format(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.png")
    gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    assert add.ParseRunAndFinalize([tmp_filename, "--external", "--levels=2"])

    assert gdal.VSIStatL(tmp_filename + ".ovr") is not None

    with gdal.Open(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1


@pytest.mark.require_driver("HFA")
def test_gdalalg_overview_external_rrd(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    try:
        gdal.Run(
            get_overview_add_alg(),
            input=tmp_filename,
            levels=[2],
            creation_option={"LOCATION": "RRD"},
        )
    except Exception:
        if (
            "This build does not support creating .aux overviews"
            in gdal.GetLastErrorMsg()
        ):
            pytest.skip(gdal.GetLastErrorMsg())

    assert gdal.VSIStatL(tmp_vsimem / "tmp.aux") is not None

    with gdal.Open(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1


def test_gdalalg_overview_delete():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["resampling"] = "average"
    add["min-size"] = 10
    assert add.Run()

    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    delete = get_overview_delete_alg()
    delete["dataset"] = ds
    assert delete.Run()

    assert ds.GetRasterBand(1).GetOverviewCount() == 0


@pytest.mark.require_driver("COG")
def test_gdalalg_overview_cog(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1024, 1024)
    filename = tmp_vsimem / "my_cog.tif"
    gdal.GetDriverByName("COG").CreateCopy(filename, src_ds)

    with gdal.Open(filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() > 0

    delete = get_overview_delete_alg()
    delete["dataset"] = filename
    assert delete.Run()
    assert delete.Finalize()

    with gdal.Open(filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 0

    add = get_overview_add_alg()
    add["dataset"] = filename
    with pytest.raises(
        Exception, match=r"has C\(loud\) O\(ptimized\) G\(eoTIFF\) layout"
    ):
        add.Run()

    add = get_overview_add_alg()
    add["dataset"] = filename
    add["open-option"] = {"IGNORE_COG_LAYOUT_BREAK": "YES"}
    with gdaltest.error_raised(gdal.CE_Warning):
        assert add.Run()
    assert delete.Finalize()

    with gdal.Open(filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() > 0


###############################################################################


@pytest.mark.parametrize("external", [True, False])
@pytest.mark.parametrize("with_progress", [True, False])
def test_gdalalg_overview_add_from_dataset(tmp_vsimem, external, with_progress):

    out_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "out.tif", 10, 10)
    out_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    out_ds.SetSpatialRef(osr.SpatialReference(epsg=4326))
    out_ds = None

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in.tif", 2, 5)
    ds.SetGeoTransform([2, 5, 0, 49, 0, -2])
    ds.SetSpatialRef(osr.SpatialReference(epsg=4326))
    ds.GetRasterBand(1).Fill(1)
    ds = None

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    if with_progress:
        progress = my_progress
    else:
        progress = None

    gdal.Run(
        "raster",
        "overview",
        "add",
        external=external,
        overview_src=tmp_vsimem / "in.tif",
        dataset=tmp_vsimem / "out.tif",
        progress=progress,
    )

    if with_progress:
        assert last_pct[0] == 1.0

    assert (gdal.VSIStatL(tmp_vsimem / "out.tif.ovr") is not None) == external

    ds = gdal.Open(tmp_vsimem / "out.tif", gdal.GA_Update)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ovr = ds.GetRasterBand(1).GetOverview(0)
    assert ovr.XSize == 2
    assert ovr.YSize == 5
    assert ovr.ComputeRasterMinMax(False) == (1, 1)
    ovr.Fill(0)
    ds = None

    last_pct = [0]

    gdal.Run(
        "raster",
        "overview",
        "add",
        external=external,
        overview_src=tmp_vsimem / "in.tif",
        dataset=tmp_vsimem / "out.tif",
        progress=progress,
    )

    if with_progress:
        assert last_pct[0] == 1.0

    assert (gdal.VSIStatL(tmp_vsimem / "out.tif.ovr") is not None) == external

    ds = gdal.Open(tmp_vsimem / "out.tif")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ovr = ds.GetRasterBand(1).GetOverview(0)
    assert ovr.XSize == 2
    assert ovr.YSize == 5
    assert ovr.ComputeRasterMinMax(False) == (1, 1)
    ds = None


###############################################################################


def test_gdalalg_overview_add_from_dataset_not_supported_by_this_format():

    out_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    ovr_ds = gdal.GetDriverByName("MEM").Create("", 6, 5)

    with pytest.raises(
        Exception, match=r"AddOverviews\(\) not supported for this dataset"
    ):
        gdal.Run(
            "raster",
            "overview",
            "add",
            overview_src=ovr_ds,
            dataset=out_ds,
        )


###############################################################################


def test_gdalalg_overview_add_from_dataset_bad_dimension(tmp_vsimem):

    out_ds = gdal.GetDriverByName("GTIFF").Create(tmp_vsimem / "out.tif", 10, 10)
    ovr_ds = gdal.GetDriverByName("MEM").Create("", 11, 5)

    with pytest.raises(
        Exception,
        match=r"AddOverviews\(\): at least one input dataset has dimensions larger than the full resolution dataset",
    ):
        gdal.Run(
            "raster",
            "overview",
            "add",
            overview_src=ovr_ds,
            dataset=out_ds,
        )


###############################################################################


def test_gdalalg_overview_add_from_dataset_zero_dimension(tmp_vsimem):

    out_ds = gdal.GetDriverByName("GTIFF").Create(tmp_vsimem / "out.tif", 10, 10)
    ovr_ds = gdal.GetDriverByName("MEM").Create("", 0, 0)

    with pytest.raises(
        Exception,
        match=r"AddOverviews\(\): at least one input dataset has one of its dimensions equal to 0",
    ):
        gdal.Run(
            "raster",
            "overview",
            "add",
            overview_src=ovr_ds,
            dataset=out_ds,
        )


###############################################################################


def test_gdalalg_overview_add_from_dataset_bad_band_count(tmp_vsimem):

    out_ds = gdal.GetDriverByName("GTIFF").Create(tmp_vsimem / "out.tif", 10, 10)
    ovr_ds = gdal.GetDriverByName("MEM").Create("", 5, 5, 2)

    with pytest.raises(
        Exception,
        match=r"AddOverviews\(\): at least one input dataset not the same number of bands than the full resolution dataset",
    ):
        gdal.Run(
            "raster",
            "overview",
            "add",
            overview_src=ovr_ds,
            dataset=out_ds,
        )


###############################################################################


def test_gdalalg_overview_add_from_dataset_bad_crs(tmp_vsimem):

    out_ds = gdal.GetDriverByName("GTIFF").Create(tmp_vsimem / "out.tif", 10, 10)
    out_ds.SetSpatialRef(osr.SpatialReference(epsg=4326))

    ovr_ds = gdal.GetDriverByName("MEM").Create("", 5, 5)
    ovr_ds.SetSpatialRef(osr.SpatialReference(epsg=32631))

    with pytest.raises(
        Exception,
        match=r"AddOverviews\(\): at least one input dataset has its CRS different from the one of the full resolution dataset",
    ):
        gdal.Run(
            "raster",
            "overview",
            "add",
            overview_src=ovr_ds,
            dataset=out_ds,
        )


###############################################################################


def test_gdalalg_overview_add_from_dataset_bad_gt(tmp_vsimem):

    out_ds = gdal.GetDriverByName("GTIFF").Create(tmp_vsimem / "out.tif", 10, 10)
    out_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])

    ovr_ds = gdal.GetDriverByName("MEM").Create("", 5, 5)
    ovr_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])

    with pytest.raises(
        Exception,
        match=r"AddOverviews\(\): at least one input dataset has its geospatial extent different from the one of the full resolution dataset",
    ):
        gdal.Run(
            "raster",
            "overview",
            "add",
            overview_src=ovr_ds,
            dataset=out_ds,
        )


###############################################################################


def test_gdalalg_overview_add_complete():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster overview add ../gcore/data/byte.tif --co"
    ).split(" ")
    assert "LOCATION=" in out
    assert "COMPRESS=" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster overview add ../gcore/data/byte.tif --co COMPRESS="
    ).split(" ")
    assert "NONE" in out
    assert "LZW" in out


###############################################################################


def test_gdalalg_overview_add_in_pipeline(tmp_vsimem):

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    with gdal.Run(
        "pipeline",
        input=src_ds,
        pipeline="read ! overview add --levels 2 ! write --format stream streamed_dataset",
    ) as alg:
        ds = alg.Output()
        assert ds.GetRasterBand(1).GetOverviewCount() == 1
        assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1192

    assert src_ds.GetRasterBand(1).GetOverviewCount() == 0


###############################################################################


def test_gdalalg_overview_add_warn_conflicting_mask_sources():

    src_ds = gdal.GetDriverByName("MEM").Create("foo", 20, 20, 2)
    src_ds.GetRasterBand(1).SetNoDataValue(255)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Raster band 1 of dataset foo has several conflicting mask sources:\n- nodata value\n- related to a raster band that is an alpha band\nOnly the nodata value will be taken into account",
    ):
        gdal.alg.raster.overview.add(input=src_ds, levels=[2, 4], resampling="average")
