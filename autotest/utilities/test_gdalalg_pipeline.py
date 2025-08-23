#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal pipeline' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr


def get_pipeline_alg():
    return gdal.GetGlobalAlgorithmRegistry()["pipeline"]


def test_gdalalg_pipeline_read_and_write_vector(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    pipeline = get_pipeline_alg()
    with gdaltest.error_raised(gdal.CE_Warning):
        assert pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "edit",
                "--layer-metadata",
                "FOO=BAR",
                "!",
                "write",
                out_filename,
            ],
            my_progress,
        )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_pipeline_read_and_write_raster(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    pipeline = get_pipeline_alg()
    with gdaltest.error_raised(gdal.CE_Warning):
        assert pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "edit",
                "--gcp=1,2,3,4",
                "!",
                "write",
                out_filename,
            ],
            my_progress,
        )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_pipeline_read_and_write_vector_from_object():

    src_ds = gdal.OpenEx("../ogr/data/poly.shp")
    with gdal.Run(
        "pipeline",
        input=src_ds,
        output_format="MEM",
        output="",
        pipeline="read ! write",
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_pipeline_read_and_write_raster_from_object():

    src_ds = gdal.Open("../gcore/data/byte.tif")
    with gdal.Run(
        "pipeline",
        input=src_ds,
        output_format="MEM",
        output="",
        pipeline="read ! write",
    ) as alg:
        assert alg.Output().GetRasterBand(1).Checksum() == 4672


def test_gdalalg_pipeline_read_vector_write_raster():

    with gdal.Run(
        "pipeline",
        pipeline="read ../ogr/data/poly.shp ! rasterize --size 256,256 ! write --output-format stream streamed_dataset",
    ) as alg, gdal.Run(
        "vector",
        "rasterize",
        input="../ogr/data/poly.shp",
        size=[256, 256],
        output_format="MEM",
        output="",
    ) as alg2:
        assert (
            alg.Output().GetRasterBand(1).Checksum()
            == alg2.Output().GetRasterBand(1).Checksum()
        )


def test_gdalalg_pipeline_errors():

    alg = get_pipeline_alg()
    alg.ParseCommandLineArguments(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "write",
            "--output-format=stream",
            "streamed_dataset",
        ]
    )
    with pytest.raises(Exception, match="can only be called once per instance"):
        alg.ParseCommandLineArguments(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "write",
                "--output-format=stream",
                "streamed_dataset",
            ]
        )

    with pytest.raises(Exception, match="pipeline: unknown step name: foo"):
        gdal.Run("pipeline", pipeline="foo")

    with pytest.raises(Exception, match="pipeline: At least 2 steps must be provided"):
        gdal.Run("pipeline", pipeline="read ../gcore/data/byte.tif")

    with pytest.raises(Exception, match="pipeline: Last step should be 'write'"):
        gdal.Run("pipeline", pipeline="read ../gcore/data/byte.tif ! reproject")

    with pytest.raises(Exception, match="read: Option '--bar' is unknown"):
        gdal.Run("pipeline", pipeline="read ../gcore/data/byte.tif --bar ! write")

    with pytest.raises(Exception, match="pipeline: Only first step can be 'read'"):
        gdal.Run("pipeline", pipeline="read ../gcore/data/byte.tif ! read ! write")

    with pytest.raises(Exception, match="pipeline: Only last step can be 'write'"):
        gdal.Run("pipeline", pipeline="read ../gcore/data/byte.tif ! write ! write")

    with pytest.raises(Exception, match="pipeline: First step should be 'read',"):
        gdal.Run("pipeline", pipeline="write ! reproject")

    with pytest.raises(
        Exception,
        match="pipeline: Step 'buffer' expects a vector input dataset, but previous step 'read' generates a raster output dataset",
    ):
        gdal.Run(
            "pipeline",
            pipeline="read ../gcore/data/byte.tif ! buffer --distance=1 ! write --output-format=stream streamed_dataset",
        )

    with pytest.raises(
        Exception,
        match="pipeline: Step 'hillshade' expects a raster input dataset, but previous step 'read' generates a vector output dataset",
    ):
        gdal.Run(
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! hillshade ! write --output-format=stream streamed_dataset",
        )

    with pytest.raises(
        Exception,
        match="buffer: Positional arguments starting at 'DISTANCE' have not been specified",
    ):
        gdal.Run(
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! buffer ! write --output-format=stream streamed_dataset",
        )

    with pytest.raises(Exception, match="edit: Option '--gcp' is unknown"):
        gdal.Run(
            "pipeline",
            pipeline="read ../ogr/data/poly.shp ! edit --gcp=1,2,3,4 ! write --output-format=stream streamed_dataset",
        )

    with pytest.raises(
        Exception, match="clip: Argument 'window' is mutually exclusive with 'bbox'"
    ):
        gdal.Run(
            "pipeline",
            input="../gcore/data/byte.tif",
            output_format="MEM",
            output="",
            pipeline="read ! clip --bbox=1,2,3,4 --window=1,2,3,4 ! write",
        )


@pytest.fixture()
def gdal_path():
    return test_cli_utilities.get_gdal_path()


def test_gdalalg_pipeline_command_line(gdal_path, tmp_path):

    out = gdaltest.runexternal(
        f"{gdal_path} pipeline --progress read ../gcore/data/byte.tif ! write {tmp_path}/out.tif"
    )
    assert "0...10...20...30...40...50...60...70...80...90...100 - done" in out

    out = gdaltest.runexternal(
        f'{gdal_path} pipeline --progress "read ../gcore/data/byte.tif ! write {tmp_path}/out2.tif"'
    )
    assert "0...10...20...30...40...50...60...70...80...90...100 - done" in out

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline read ../gcore/data/byte.tif ! reproject --resampling=?"
    )
    assert (
        "Potential values for argument 'resampling' are:\n- nearest\n- bilinear"
        in err.replace("\r\n", "\n")
    )


def test_gdalalg_pipeline_help(gdal_path):

    out = gdaltest.runexternal(f"{gdal_path} pipeline --help")
    assert out.startswith("Usage: gdal pipeline [OPTIONS] <PIPELINE>")
    assert "read" in out
    assert "concat" in out
    assert "rasterize" in out


def test_gdalalg_pipeline_help_doc(gdal_path):

    out = gdaltest.runexternal(f"{gdal_path} pipeline --help-doc=main")

    assert "Usage: gdal pipeline [OPTIONS] <PIPELINE>" in out
    assert "<PIPELINE> is of the form:" in out

    out = gdaltest.runexternal(f"{gdal_path} pipeline --help-doc=edit-raster")

    assert "* edit [OPTIONS]" in out

    out, _ = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline --help-doc=unknown"
    )

    assert "ERROR: unknown pipeline step 'unknown'" in out


def test_gdal_pipeline_raster_output_to_gdalg(tmp_path, gdal_path):

    src_filename = os.path.join(os.getcwd(), "../gcore/data/byte.tif").replace(
        "\\", "/"
    )
    out_filename = str(tmp_path / "out.gdalg.json")
    gdaltest.runexternal(
        f"{gdal_path} pipeline read {src_filename} ! write {out_filename}"
    )
    # Test that configuration option is not serialized
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": f"gdal pipeline read --input {src_filename}",
        "type": "gdal_streamed_alg",
    }

    if gdal.GetDriverByName("GDALG"):
        ds = gdal.Open(out_filename)
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdal_pipeline_vector_output_to_gdalg(tmp_path, gdal_path):

    src_filename = os.path.join(os.getcwd(), "../ogr/data/poly.shp").replace("\\", "/")
    out_filename = str(tmp_path / "out.gdalg.json")
    gdaltest.runexternal(
        f"{gdal_path} pipeline read {src_filename} ! write {out_filename}"
    )
    # Test that configuration option is not serialized
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": f"gdal pipeline read --input {src_filename}",
        "type": "gdal_streamed_alg",
    }

    if gdal.GetDriverByName("GDALG"):
        ds = gdal.OpenEx(out_filename)
        assert ds.GetLayer(0).GetFeatureCount() == 10


def get_src_ds(geom3D):
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("z", ogr.OFTReal))
    for x, y, z in [
        (100, 1000, 500),
        (110, 1000, 400),
        (100, 1010, 300),
        (110, 1010, 600),
        (109, 1009, 610),
    ]:
        f = ogr.Feature(src_lyr.GetLayerDefn())
        if geom3D:
            f.SetGeometry(ogr.CreateGeometryFromWkt(f"POINT Z({x} {y} {z})"))
        else:
            f["z"] = z
            f.SetGeometry(ogr.CreateGeometryFromWkt(f"POINT({x} {y})"))
        src_lyr.CreateFeature(f)
    return src_ds


def test_gdalalg_pipeline_grid_average():

    src_ds = get_src_ds(True)
    with gdal.Run(
        "pipeline",
        input=src_ds,
        output_format="MEM",
        output="",
        pipeline="read ! grid average ! write",
    ) as alg:
        assert alg.Output().GetRasterBand(1).Checksum() == 35730


def test_gdalalg_pipeline_footprint():

    with gdal.Run(
        "pipeline",
        input="../gcore/data/byte.tif",
        output_format="MEM",
        output="",
        pipeline="read ! footprint ! write",
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 1


def test_gdalalg_pipeline_polygonize():

    with gdal.Run(
        "pipeline",
        input="../gcore/data/byte.tif",
        output_format="MEM",
        output="",
        pipeline="read ! polygonize ! write",
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 281


def test_gdalalg_pipeline_contour():

    with gdal.Run(
        "pipeline",
        input="../gcore/data/byte.tif",
        output_format="MEM",
        output="",
        pipeline="read ! contour --interval 10 ! write",
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 218


def test_gdalalg_pipeline_calc():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("muparser not available")

    with gdal.Run(
        "pipeline",
        output_format="MEM",
        output="",
        pipeline="calc ../gcore/data/byte.tif --calc 255-X ! write",
    ) as alg:
        assert alg.Output().GetRasterBand(1).Checksum() == 4563


def test_gdalalg_pipeline_raster_info():

    with gdal.Run(
        "pipeline",
        pipeline="read ../gcore/data/byte.tif ! info",
    ) as alg:
        assert "bands" in alg.Output()


def test_gdalalg_pipeline_vector_info():

    with gdal.Run(
        "pipeline",
        pipeline="read ../ogr/data/poly.shp ! info",
    ) as alg:
        assert "layers" in alg.Output()


def test_gdalalg_pipeline_raster_info_executable():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f'{gdal_path} pipeline --pipeline "read ../gcore/data/byte.tif ! info"'
    )
    assert out.startswith("Driver: GTiff/GeoTIFF")


def test_gdalalg_pipeline_vector_info_executable():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f'{gdal_path} pipeline "--pipeline=read ../ogr/data/poly.shp ! info"'
    )
    assert out.startswith("INFO: Open of `../ogr/data/poly.shp'")


def test_gdalalg_pipeline_run_existing(tmp_path):

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    pipeline_filename = tmp_path / "pipeline.gdalg.json"

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read ../gcore/data/byte.tif ! edit --nodata=5 --metadata=KEY=VALUE",
            }
        )
        f.write(j.encode("UTF-8"))

    with gdal.Run(
        "pipeline",
        pipeline=pipeline_filename,
        output="",
        output_format="MEM",
        arguments={"edit.metadata": "FOO=BAR"},
    ) as alg:
        ds = alg.Output()
        assert ds.GetDriver().ShortName == "MEM"
        assert ds.GetRasterBand(1).Checksum() == 4672
        assert ds.GetRasterBand(1).GetNoDataValue() == 5
        assert ds.GetMetadataItem("FOO") == "BAR"

    output_filename = tmp_path / "out.tif"

    out = gdaltest.runexternal(
        f"{gdal_path} pipeline {pipeline_filename} --write.output {output_filename} --quiet"
    )
    assert out == ""

    with gdal.Open(output_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672
        assert ds.GetMetadataItem("KEY") == "VALUE"

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename} --output {output_filename} --quiet"
    )
    assert "--overwrite" in err

    gdal.Unlink(output_filename)

    out = gdaltest.runexternal(
        f"{gdal_path} pipeline --pipeline={pipeline_filename} --output {output_filename} --quiet"
    )
    assert out == ""
    with gdal.Open(output_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672
        assert ds.GetMetadataItem("KEY") == "VALUE"
    gdal.Unlink(output_filename)

    out = gdaltest.runexternal(
        f"{gdal_path} pipeline --pipeline {pipeline_filename} --input ../gcore/data/rgbsmall.tif --output {output_filename} --metadata FOO=BAR --quiet"
    )
    assert out == ""
    with gdal.Open(output_filename) as ds:
        assert ds.RasterCount == 3
        assert ds.GetMetadataItem("FOO") == "BAR"
    gdal.Unlink(output_filename)

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read ../gcore/data/byte.tif ! edit --metadata KEY=VALUE",
            }
        )
        f.write(j.encode("UTF-8"))

    out = gdaltest.runexternal(
        f"{gdal_path} pipeline --pipeline {pipeline_filename} --input ../gcore/data/rgbsmall.tif --output {output_filename} --metadata=FOO=BAR --quiet"
    )
    assert out == ""
    with gdal.Open(output_filename) as ds:
        assert ds.RasterCount == 3
        assert ds.GetMetadataItem("FOO") == "BAR"
    gdal.Unlink(output_filename)

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}"
    )
    assert (
        "write: Positional arguments starting at 'OUTPUT' have not been specified"
        in err
    )

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --foo.bar=baz"
    )
    assert (
        "Argument 'foo.bar' refers to a non-existing 'foo' step in the pipeline" in err
    )

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --read.bar=baz"
    )
    assert "read: Option '--bar' is unknown" in err

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --foo=bar"
    )
    assert "pipeline: No step in the pipeline has an argument named 'foo'" in err

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --foo.bar.baz=baw"
    )
    assert (
        "Invalid argument name 'foo.bar.baz'. It should of the form <algorithm-name>.<argument-name>"
        in err
    )
    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read ../gcore/data/byte.tif ! edit --metadata=KEY=VALUE ! edit ! write --output-format=stream streamed_dataset",
            }
        )
        f.write(j.encode("UTF-8"))

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --metadata=foo=bar"
    )
    assert (
        "Ambiguous argument name 'metadata', because it is valid for several steps in the pipeline. It should be specified with the form <algorithm-name>.<argument-name>"
        in err
    )

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --edit.metadata=foo=bar"
    )
    assert (
        "pipeline: Argument 'edit.metadata' is ambiguous as there are several 'edit' steps in the pipeline"
        in err
    )

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --edit[2].metadata=foo=bar"
    )
    assert (
        "pipeline: Argument 'edit[2].metadata' refers to a non-existing 'edit[2]' step in the pipeline"
        in err
    )

    out = gdaltest.runexternal(
        f"{gdal_path} pipeline {pipeline_filename} --output {output_filename} --edit[1].metadata=foo=bar --quiet"
    )
    assert out == ""
    with gdal.Open(output_filename) as ds:
        assert ds.GetMetadataItem("KEY") == "VALUE"
        assert ds.GetMetadataItem("foo") == "bar"
    gdal.Unlink(output_filename)

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read ../ogr/data/poly.shp ! edit --geometry-type MULTIPOLYGON --metadata=KEY=VALUE",
            }
        )
        f.write(j.encode("UTF-8"))

    with gdal.Run(
        "pipeline",
        pipeline=pipeline_filename,
        output="",
        output_format="MEM",
        arguments={"edit.metadata": "FOO=BAR"},
    ) as alg:
        ds = alg.Output()
        assert ds.GetDriver().ShortName == "MEM"
        assert ds.GetLayerCount() == 1
        assert ds.GetLayer(0).GetFeatureCount() == 10
        assert ds.GetLayer(0).GetGeomType() == ogr.wkbMultiPolygon
        assert ds.GetMetadataItem("FOO") == "BAR"

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        f.write(b"""corrupted""")

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --edit[2].metadata=foo=bar"
    )
    assert "JSON parsing error" in err

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
            }
        )
        f.write(j.encode("UTF-8"))

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename}  --output {output_filename} --edit[2].metadata=foo=bar"
    )
    assert "pipeline: command_line missing in" in err

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read a b c ! edit --metadata=KEY=VALUE ! edit ! write --output-format=stream streamed_dataset",
            }
        )
        f.write(j.encode("UTF-8"))

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename} --output {output_filename} --input=/i/do_not/exist"
    )
    assert "/i/do_not/exist" in err

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read a b c ! edit --metadata=KEY=VALUE ! edit ! write --output-format stream --output streamed_dataset",
            }
        )
        f.write(j.encode("UTF-8"))

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} pipeline {pipeline_filename} --output {output_filename} --input=/i/do_not/exist"
    )
    assert "/i/do_not/exist" in err


def test_gdalalg_pipeline_existing_completion(tmp_path):

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    pipeline_filename = tmp_path / "pipeline.gdalg.json"

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read ../gcore/data/byte.tif ! edit --nodata=5 --metadata=KEY=VALUE",
            }
        )
        f.write(j.encode("UTF-8"))

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline {pipeline_filename} -"
    ).split(" ")
    expected_out = [
        "--input-format=",
        "--open-option=",
        "--input=",
        "--edit.crs=",
        "--edit.bbox=",
        "--edit.nodata=",
        "--edit.metadata=",
        "--edit.unset-metadata=",
        "--edit.gcp=",
        "--output-format=",
        "--output=",
        "--creation-option=",
        "--overwrite",
    ]
    for x in expected_out:
        assert x in out
    assert "--input-layer=" not in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline {pipeline_filename} --input-format="
    ).split(" ")
    assert "MEM" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline {pipeline_filename} --output=/vsimem/out.tif --creation-option="
    ).split(" ")
    assert "COMPRESS=" in out

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read ../ogr/data/poly.shp ! edit",
            }
        )
        f.write(j.encode("UTF-8"))

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline {pipeline_filename} -"
    ).split(" ")
    expected_out = [
        "--input-format=",
        "--open-option=",
        "--input=",
        "--input-layer=",
        "--edit.active-layer=",
        "--edit.geometry-type=",
        "--edit.crs=",
        "--edit.metadata=",
        "--edit.unset-metadata=",
        "--edit.layer-metadata=",
        "--edit.unset-layer-metadata=",
        "--output-format=",
        "--output=",
        "--creation-option=",
        "--layer-creation-option=",
        "--overwrite",
        "--update",
        "--overwrite-layer",
        "--append",
        "--output-layer=",
        "--write.skip-errors",
    ]
    for x in expected_out:
        assert x in out
    assert "--edit.nodata=" not in out

    with gdal.VSIFile(pipeline_filename, "wb") as f:
        j = json.dumps(
            {
                "type": "gdal_streamed_alg",
                "relative_paths_relative_to_this_file": False,
                "command_line": "gdal pipeline read ../ogr/data/poly.shp ! edit ! edit",
            }
        )
        f.write(j.encode("UTF-8"))

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline {pipeline_filename} -"
    ).split(" ")
    assert "--edit[0].geometry-type=" in out
    assert "--edit[1].geometry-type=" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal pipeline {pipeline_filename} --edit[0].geometry-type="
    ).split(" ")
    assert "GEOMETRY" in out
