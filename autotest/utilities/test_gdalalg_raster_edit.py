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

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr


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


def test_gdalalg_raster_edit_unset_metadata_domain(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadata(['{ "key": "value" }'], "json:ISIS3")

    gdal.Translate(tmp_vsimem / "my.tif", src_ds)

    with gdal.Open(tmp_vsimem / "my.tif") as ds:
        assert ds.GetMetadata("json:ISIS3")[0] == '{ "key": "value" }'

    gdal.Run(
        "raster edit", dataset=tmp_vsimem / "my.tif", unset_metadata_domain="json:ISIS3"
    )

    with gdal.Open(tmp_vsimem / "my.tif") as ds:
        assert ds.GetMetadata("json:ISIS3") is None


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


def test_gdalalg_raster_edit_gcp_from_list_of_values():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    gdal.Run(
        "raster",
        "edit",
        dataset=mem_ds,
        crs="EPSG:4326",
        gcp=[[1.5, 2.5, 3.5, 4.5], [5.5, 6.5, 7.5, 8.5, 9.5]],
    )

    assert mem_ds.GetGCPCount() == 2
    assert mem_ds.GetGCPSpatialRef().GetAuthorityCode(None) == "4326"

    assert mem_ds.GetGCPs()[0].GCPPixel == 1.5
    assert mem_ds.GetGCPs()[0].GCPLine == 2.5
    assert mem_ds.GetGCPs()[0].GCPX == 3.5
    assert mem_ds.GetGCPs()[0].GCPY == 4.5

    assert mem_ds.GetGCPs()[1].GCPPixel == 5.5
    assert mem_ds.GetGCPs()[1].GCPLine == 6.5
    assert mem_ds.GetGCPs()[1].GCPX == 7.5
    assert mem_ds.GetGCPs()[1].GCPY == 8.5
    assert mem_ds.GetGCPs()[1].GCPZ == 9.5


def test_gdalalg_raster_edit_gcp_from_list_of_gdal_GCP():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    gdal.Run(
        "raster",
        "edit",
        dataset=mem_ds,
        gcp=[gdal.GCP(3.5, 4.5, 0, 1.5, 2.5), gdal.GCP(7.5, 8.5, 9.5, 5.5, 6.5)],
    )

    assert mem_ds.GetGCPCount() == 2
    assert mem_ds.GetGCPSpatialRef() is None

    assert mem_ds.GetGCPs()[0].GCPPixel == 1.5
    assert mem_ds.GetGCPs()[0].GCPLine == 2.5
    assert mem_ds.GetGCPs()[0].GCPX == 3.5
    assert mem_ds.GetGCPs()[0].GCPY == 4.5

    assert mem_ds.GetGCPs()[1].GCPPixel == 5.5
    assert mem_ds.GetGCPs()[1].GCPLine == 6.5
    assert mem_ds.GetGCPs()[1].GCPX == 7.5
    assert mem_ds.GetGCPs()[1].GCPY == 8.5
    assert mem_ds.GetGCPs()[1].GCPZ == 9.5


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_edit_gcp_from_vector_dataset(tmp_vsimem):

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    with gdal.GetDriverByName("CSV").CreateVector(tmp_vsimem / "gcps.csv") as gcp_ds:
        lyr = gcp_ds.CreateLayer("gcps")
        lyr.CreateField(ogr.FieldDefn("id"))
        lyr.CreateField(ogr.FieldDefn("info"))
        lyr.CreateField(ogr.FieldDefn("column"))
        lyr.CreateField(ogr.FieldDefn("line"))
        lyr.CreateField(ogr.FieldDefn("x"))
        lyr.CreateField(ogr.FieldDefn("y"))
        lyr.CreateField(ogr.FieldDefn("z"))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["id"] = "my_id"
        f["info"] = "my_info"
        f["column"] = 1.5
        f["line"] = 2.5
        f["x"] = 3.5
        f["y"] = 4.5
        f["z"] = 5.5
        lyr.CreateFeature(f)

    gdal.Run("raster", "edit", dataset=mem_ds, gcp=f"@{tmp_vsimem}/gcps.csv")

    assert mem_ds.GetGCPCount() == 1
    assert mem_ds.GetGCPSpatialRef() is None

    assert mem_ds.GetGCPs()[0].Id == "my_id"
    assert mem_ds.GetGCPs()[0].Info == "my_info"
    assert mem_ds.GetGCPs()[0].GCPPixel == 1.5
    assert mem_ds.GetGCPs()[0].GCPLine == 2.5
    assert mem_ds.GetGCPs()[0].GCPX == 3.5
    assert mem_ds.GetGCPs()[0].GCPY == 4.5
    assert mem_ds.GetGCPs()[0].GCPZ == 5.5


def test_gdalalg_raster_edit_gcp_bad_format():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    with pytest.raises(Exception, match=" Bad format for 1,2,3"):
        gdal.Run("raster", "edit", dataset=mem_ds, gcp="1,2,3")

    with pytest.raises(Exception, match=" Bad format for 1,2,3,foo"):
        gdal.Run("raster", "edit", dataset=mem_ds, gcp="1,2,3,foo")


def test_gdalalg_raster_edit_gcp_from_vector_dataset_cannot_be_opened():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(Exception, match="/i_do/not/exist.csv"):
        gdal.Run("raster", "edit", dataset=mem_ds, gcp="@/i_do/not/exist.csv")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_edit_gcp_from_vector_dataset_two_layers(tmp_vsimem):

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    with gdal.GetDriverByName("GPKG").CreateVector(tmp_vsimem / "gcps.gpkg") as gcp_ds:
        gcp_ds.CreateLayer("a")
        gcp_ds.CreateLayer("b")

    with pytest.raises(
        Exception, match="GCPs can only be specified for single-layer datasets"
    ):
        gdal.Run("raster", "edit", dataset=mem_ds, gcp=f"@{tmp_vsimem}/gcps.gpkg")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_edit_gcp_from_vector_dataset_missing_column_field(tmp_vsimem):

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    with gdal.GetDriverByName("GPKG").CreateVector(tmp_vsimem / "gcps.gpkg") as gcp_ds:
        lyr = gcp_ds.CreateLayer("gcps")
        lyr.CreateField(ogr.FieldDefn("line"))
        lyr.CreateField(ogr.FieldDefn("x"))
        lyr.CreateField(ogr.FieldDefn("y"))

    with pytest.raises(Exception, match="Field 'column' cannot be found in"):
        gdal.Run("raster", "edit", dataset=mem_ds, gcp=f"@{tmp_vsimem}/gcps.gpkg")


@pytest.mark.require_driver("Zarr")
def test_gdalalg_raster_edit_gcp_output_fromat_does_not_support(tmp_vsimem):

    ds = gdal.GetDriverByName("ZARR").Create(tmp_vsimem / "test.zarr", 1, 1)

    with pytest.raises(Exception, match="Setting GCPs failed"):
        gdal.Run("raster", "edit", dataset=ds, gcp=[[1.5, 2.5, 3.5, 4.5]])


@pytest.mark.require_driver("COG")
def test_gdalalg_raster_edit_cog(tmp_vsimem):

    gdal.alg.raster.convert(
        input="../gcore/data/byte.tif",
        output=tmp_vsimem / "out.tif",
        output_format="COG",
    )
    with pytest.raises(
        Exception, match=r"has C\(loud\) O\(ptimized\) G\(eoTIFF\) layout"
    ):
        gdal.alg.raster.edit(dataset=tmp_vsimem / "out.tif", crs="EPSG:32611")
    with gdal.quiet_errors():
        gdal.alg.raster.edit(
            dataset=tmp_vsimem / "out.tif",
            crs="EPSG:32611",
            open_option={"IGNORE_COG_LAYOUT_BREAK": "YES"},
        )


def test_gdalalg_raster_edit_color_interpretation_single_band():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    gdal.alg.raster.edit(dataset=ds, color_interpretation="Red")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand

    gdal.alg.raster.edit(dataset=ds, color_interpretation="all=Green")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand

    gdal.alg.raster.edit(dataset=ds, color_interpretation="1=Blue")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand

    with pytest.raises(Exception, match="Unsupported color interpretation: invalid"):
        gdal.alg.raster.edit(dataset=ds, color_interpretation="invalid")

    with pytest.raises(Exception, match="Unsupported color interpretation: invalid"):
        gdal.alg.raster.edit(dataset=ds, color_interpretation="all=invalid")

    with pytest.raises(Exception, match="Unsupported color interpretation: invalid"):
        gdal.alg.raster.edit(dataset=ds, color_interpretation="1=invalid")

    with pytest.raises(Exception, match="Invalid band number '0' in '0=Red'"):
        gdal.alg.raster.edit(dataset=ds, color_interpretation="0=Red")

    with pytest.raises(Exception, match="Invalid band number '2' in '2=Red'"):
        gdal.alg.raster.edit(dataset=ds, color_interpretation="2=Red")

    with pytest.raises(
        Exception,
        match="More color interpretation values specified than bands in the dataset",
    ):
        gdal.alg.raster.edit(dataset=ds, color_interpretation=["Red", "Green"])


def test_gdalalg_raster_edit_color_interpretation_multi_band():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)

    gdal.alg.raster.edit(dataset=ds, color_interpretation="all=Green")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_GreenBand

    gdal.alg.raster.edit(dataset=ds, color_interpretation=["Red", "Green", "Blue"])
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand

    gdal.alg.raster.edit(
        dataset=ds, color_interpretation=["3=Red", "1=Green", "2=Blue"]
    )
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_RedBand

    with pytest.raises(
        Exception,
        match="With several bands, specify as many color interpretation as bands, one or many values of the form <band_number>=<color> or a single value all=<color>",
    ):
        gdal.alg.raster.edit(dataset=ds, color_interpretation="Red")

    with pytest.raises(
        Exception,
        match="More color interpretation values specified than bands in the dataset",
    ):
        gdal.alg.raster.edit(
            dataset=ds, color_interpretation=["Red", "Green", "Blue", "Alpha"]
        )

    with pytest.raises(
        Exception,
        match="Less color interpretation values specified than bands in the dataset",
    ):
        gdal.alg.raster.edit(dataset=ds, color_interpretation=["Red", "Green"])

    with pytest.raises(
        Exception, match="Mix of different syntaxes to specify color interpretation"
    ):
        gdal.alg.raster.edit(dataset=ds, color_interpretation=["Red", "all=Green"])

    with pytest.raises(
        Exception, match="Mix of different syntaxes to specify color interpretation"
    ):
        gdal.alg.raster.edit(dataset=ds, color_interpretation=["2=Red", "Green"])

    with pytest.raises(
        Exception, match="Mix of different syntaxes to specify color interpretation"
    ):
        gdal.alg.raster.edit(dataset=ds, color_interpretation=["Red", "2=Green"])


def test_gdalalg_raster_edit_color_interpretation_autocomplete():

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster edit --color-interpretation last_word_is_complete=true"
    ).split(" ")
    assert "all=" in out
    assert "Red" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster edit ../gcore/data/byte.tif --color-interpretation last_word_is_complete=true"
    ).split(" ")
    assert "all=" in out
    assert "1=" in out
    assert "Red" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster edit --color-interpretation all= last_word_is_complete=false"
    ).split(" ")
    assert "all=" not in out
    assert "Red" in out


def test_gdalalg_raster_edit_scale():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)

    gdal.alg.raster.edit(dataset=ds, scale=2.5)
    assert ds.GetRasterBand(1).GetScale() == 2.5
    assert ds.GetRasterBand(2).GetScale() == 2.5
    assert ds.GetRasterBand(3).GetScale() == 2.5

    gdal.alg.raster.edit(dataset=ds, scale=[3.5, 2, 1.5])
    assert ds.GetRasterBand(1).GetScale() == 3.5
    assert ds.GetRasterBand(2).GetScale() == 2
    assert ds.GetRasterBand(3).GetScale() == 1.5

    gdal.alg.raster.edit(dataset=ds, scale="2=2.5")
    assert ds.GetRasterBand(1).GetScale() == 3.5
    assert ds.GetRasterBand(2).GetScale() == 2.5
    assert ds.GetRasterBand(3).GetScale() == 1.5

    with pytest.raises(
        Exception,
        match="Invalid value 'foo' for 'scale'",
    ):
        gdal.alg.raster.edit(dataset=ds, scale="foo")

    with pytest.raises(
        Exception,
        match="Invalid value 'foo=1' for 'scale'",
    ):
        gdal.alg.raster.edit(dataset=ds, scale="foo=1")

    with pytest.raises(
        Exception,
        match="Invalid value '1=foo' for 'scale'",
    ):
        gdal.alg.raster.edit(dataset=ds, scale="1=foo")

    with pytest.raises(
        Exception,
        match="Less scale values specified than bands in the dataset",
    ):
        gdal.alg.raster.edit(dataset=ds, scale=[1, 2])

    with pytest.raises(
        Exception,
        match="More scale values specified than bands in the dataset",
    ):
        gdal.alg.raster.edit(dataset=ds, scale=[1, 2, 3, 4])

    with pytest.raises(
        Exception,
        match="Mix of different syntaxes to specify scale",
    ):
        gdal.alg.raster.edit(dataset=ds, scale=[1, "2=3"])

    with pytest.raises(
        Exception,
        match="Invalid band number '0' in '0=3'",
    ):
        gdal.alg.raster.edit(dataset=ds, scale=[1, "0=3"])

    with pytest.raises(
        Exception,
        match="Invalid band number '4' in '4=3'",
    ):
        gdal.alg.raster.edit(dataset=ds, scale=[1, "4=3"])


def test_gdalalg_raster_edit_offset():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)

    gdal.alg.raster.edit(dataset=ds, offset=2.5)
    assert ds.GetRasterBand(1).GetOffset() == 2.5
    assert ds.GetRasterBand(2).GetOffset() == 2.5
    assert ds.GetRasterBand(3).GetOffset() == 2.5

    gdal.alg.raster.edit(dataset=ds, offset=[3.5, 2, 1.5])
    assert ds.GetRasterBand(1).GetOffset() == 3.5
    assert ds.GetRasterBand(2).GetOffset() == 2
    assert ds.GetRasterBand(3).GetOffset() == 1.5

    gdal.alg.raster.edit(dataset=ds, offset="2=2.5")
    assert ds.GetRasterBand(1).GetOffset() == 3.5
    assert ds.GetRasterBand(2).GetOffset() == 2.5
    assert ds.GetRasterBand(3).GetOffset() == 1.5

    with pytest.raises(
        Exception,
        match="Invalid value 'foo' for 'offset'",
    ):
        gdal.alg.raster.edit(dataset=ds, offset="foo")

    with pytest.raises(
        Exception,
        match="Invalid value 'foo=1' for 'offset'",
    ):
        gdal.alg.raster.edit(dataset=ds, offset="foo=1")

    with pytest.raises(
        Exception,
        match="Invalid value '1=foo' for 'offset'",
    ):
        gdal.alg.raster.edit(dataset=ds, offset="1=foo")

    with pytest.raises(
        Exception,
        match="Less offset values specified than bands in the dataset",
    ):
        gdal.alg.raster.edit(dataset=ds, offset=[1, 2])

    with pytest.raises(
        Exception,
        match="More offset values specified than bands in the dataset",
    ):
        gdal.alg.raster.edit(dataset=ds, offset=[1, 2, 3, 4])

    with pytest.raises(
        Exception,
        match="Mix of different syntaxes to specify offset",
    ):
        gdal.alg.raster.edit(dataset=ds, offset=[1, "2=3"])

    with pytest.raises(
        Exception,
        match="Invalid band number '0' in '0=3'",
    ):
        gdal.alg.raster.edit(dataset=ds, offset=[1, "0=3"])

    with pytest.raises(
        Exception,
        match="Invalid band number '4' in '4=3'",
    ):
        gdal.alg.raster.edit(dataset=ds, offset=[1, "4=3"])
