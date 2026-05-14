#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim convert' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = [
    pytest.mark.require_driver("VRT"),
    pytest.mark.skipif(
        test_cli_utilities.get_gdal_path() is None, reason="gdal binary not available"
    ),
]


@pytest.fixture()
def gdal_path():
    return test_cli_utilities.get_gdal_path()


def get_mdim_convert_alg():
    return gdal.GetGlobalAlgorithmRegistry()["mdim"]["convert"]


###############################################################################


def test_gdalalg_mdim_convert_basic(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    alg = get_mdim_convert_alg()
    alg["strict"] = True
    alg["array-option"] = ["NOT_A_REAL=OPTION"]
    assert alg.ParseRunAndFinalize(["data/mdim.vrt", tmpfile])

    assert gdal.MultiDimInfo(tmpfile) == gdal.MultiDimInfo("data/mdim.vrt")


###############################################################################


def test_gdalalg_mdim_convert_overwrite(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"

    alg = get_mdim_convert_alg()
    assert alg.ParseRunAndFinalize(["data/mdim.vrt", tmpfile])

    alg = get_mdim_convert_alg()
    with pytest.raises(Exception, match="already exists"):
        alg.ParseRunAndFinalize(["data/mdim.vrt", tmpfile])

    alg = get_mdim_convert_alg()
    alg["overwrite"] = True
    assert alg.ParseRunAndFinalize(["data/mdim.vrt", tmpfile])

    assert gdal.MultiDimInfo(tmpfile) == gdal.MultiDimInfo("data/mdim.vrt")


###############################################################################


def test_gdalalg_mdim_convert_to_mem():

    alg = get_mdim_convert_alg()
    alg["input"] = "data/mdim.vrt"
    alg["output-format"] = "MEM"
    alg["output"] = ""
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds
    rg = out_ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray("time_increasing")
    assert ar
    assert ar.Read() == ["2010-01-01", "2011-01-01", "2012-01-01", "2013-01-01"]


###############################################################################


def test_gdalalg_mdim_convert_multidim_to_classic(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.tif"

    alg = get_mdim_convert_alg()
    alg["input"] = "data/mdim.vrt"
    alg["output"] = tmpfile
    alg["array"] = ["/my_subgroup/array_in_subgroup"]
    assert alg.Run()


###############################################################################


def test_gdalalg_mdim_convert_group(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"

    alg = get_mdim_convert_alg()
    alg["input"] = "data/mdim.vrt"
    alg["output"] = tmpfile
    alg["group"] = ["my_subgroup"]
    assert alg.Run()
    assert alg.Finalize()

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    gdal.Unlink(tmpfile)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="latitude" type="HORIZONTAL_Y" direction="NORTH" size="10" indexingVariable="latitude" />
    <Dimension name="longitude" type="HORIZONTAL_X" direction="EAST" size="10" indexingVariable="longitude" />
    <Array name="latitude">
      <DataType>Float32</DataType>
      <DimensionRef ref="latitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/latitude</SourceArray>
        <SourceSlab offset="0" count="10" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="longitude">
      <DataType>Float32</DataType>
      <DimensionRef ref="longitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/longitude</SourceArray>
        <SourceSlab offset="0" count="10" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="array_in_subgroup">
      <DataType>Int32</DataType>
      <DimensionRef ref="latitude" />
      <DimensionRef ref="longitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/my_subgroup/array_in_subgroup</SourceArray>
        <SourceSlab offset="0,0" count="10,10" step="1,1" />
        <DestSlab offset="0,0" />
      </Source>
    </Array>
  </Group>
</VRTDataset>
"""


###############################################################################


def test_gdalalg_mdim_convert_subset(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"

    alg = get_mdim_convert_alg()
    alg["input"] = "data/mdim.vrt"
    alg["output"] = tmpfile
    alg["subset"] = ["latitude(70,87.5)", 'time_increasing("2012-01-01")']
    assert alg.Run()
    assert alg.Finalize()

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    gdal.Unlink(tmpfile)
    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="latitude" type="HORIZONTAL_Y" direction="NORTH" size="8" indexingVariable="latitude" />
    <Dimension name="longitude" type="HORIZONTAL_X" direction="EAST" size="10" indexingVariable="longitude" />
    <Dimension name="time_decreasing" type="TEMPORAL" size="4" indexingVariable="time_decreasing" />
    <Array name="latitude">
      <DataType>Float32</DataType>
      <DimensionRef ref="latitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/latitude</SourceArray>
        <SourceView>[1:9:1]</SourceView>
        <SourceSlab offset="0" count="8" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="longitude">
      <DataType>Float32</DataType>
      <DimensionRef ref="longitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/longitude</SourceArray>
        <SourceSlab offset="0" count="10" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="my_variable_with_time_increasing">
      <DataType>Int32</DataType>
      <DimensionRef ref="latitude" />
      <DimensionRef ref="longitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/my_variable_with_time_increasing</SourceArray>
        <SourceView>[2,1:9:1,:]</SourceView>
        <SourceSlab offset="0,0" count="8,10" step="1,1" />
        <DestSlab offset="0,0" />
      </Source>
      <Attribute name="DIM_time_increasing_INDEX">
        <DataType>Int32</DataType>
        <Value>2</Value>
      </Attribute>
      <Attribute name="DIM_time_increasing_VALUE">
        <DataType>String</DataType>
        <Value>2012-01-01</Value>
      </Attribute>
    </Array>
    <Array name="time_increasing">
      <DataType>String</DataType>
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/time_increasing</SourceArray>
        <SourceView>[2]</SourceView>
      </Source>
      <Attribute name="DIM_time_increasing_INDEX">
        <DataType>Int32</DataType>
        <Value>2</Value>
      </Attribute>
      <Attribute name="DIM_time_increasing_VALUE">
        <DataType>String</DataType>
        <Value>2012-01-01</Value>
      </Attribute>
    </Array>
    <Array name="my_variable_with_time_decreasing">
      <DataType>Int32</DataType>
      <DimensionRef ref="time_decreasing" />
      <DimensionRef ref="latitude" />
      <DimensionRef ref="longitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/my_variable_with_time_decreasing</SourceArray>
        <SourceView>[:,1:9:1,:]</SourceView>
        <SourceSlab offset="0,0,0" count="4,8,10" step="1,1,1" />
        <DestSlab offset="0,0,0" />
      </Source>
    </Array>
    <Array name="time_decreasing">
      <DataType>String</DataType>
      <DimensionRef ref="time_decreasing" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/time_decreasing</SourceArray>
        <SourceSlab offset="0" count="4" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Group name="my_subgroup">
      <Array name="array_in_subgroup">
        <DataType>Int32</DataType>
        <DimensionRef ref="/latitude" />
        <DimensionRef ref="/longitude" />
        <Source>
          <SourceFilename>data/mdim.vrt</SourceFilename>
          <SourceArray>/my_subgroup/array_in_subgroup</SourceArray>
          <SourceView>[1:9:1,:]</SourceView>
          <SourceSlab offset="0,0" count="8,10" step="1,1" />
          <DestSlab offset="0,0" />
        </Source>
      </Array>
    </Group>
    <Group name="other_subgroup">
      <Attribute name="foo">
        <DataType>String</DataType>
        <Value>bar</Value>
      </Attribute>
    </Group>
  </Group>
</VRTDataset>
"""


###############################################################################


def test_gdalalg_mdim_convert_scaleaxes(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"

    alg = get_mdim_convert_alg()
    alg["input"] = "data/mdim.vrt"
    alg["output"] = tmpfile
    alg["array"] = ["my_variable_with_time_increasing"]
    alg["scale-axes"] = ["longitude(2)"]
    assert alg.Run()
    assert alg.Finalize()

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    gdal.Unlink(tmpfile)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="latitude" type="HORIZONTAL_Y" direction="NORTH" size="10" indexingVariable="latitude" />
    <Dimension name="longitude" type="HORIZONTAL_X" direction="EAST" size="5" indexingVariable="longitude" />
    <Dimension name="time_increasing" type="TEMPORAL" size="4" indexingVariable="time_increasing" />
    <Array name="time_increasing">
      <DataType>String</DataType>
      <DimensionRef ref="time_increasing" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/time_increasing</SourceArray>
        <SourceSlab offset="0" count="4" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="latitude">
      <DataType>Float32</DataType>
      <DimensionRef ref="latitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/latitude</SourceArray>
        <SourceSlab offset="0" count="10" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="longitude">
      <DataType>Float32</DataType>
      <DimensionRef ref="longitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/longitude</SourceArray>
        <SourceView>[0:10:2]</SourceView>
        <SourceSlab offset="0" count="5" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="my_variable_with_time_increasing">
      <DataType>Int32</DataType>
      <DimensionRef ref="time_increasing" />
      <DimensionRef ref="latitude" />
      <DimensionRef ref="longitude" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/my_variable_with_time_increasing</SourceArray>
        <SourceView>[:,:,0:10:2]</SourceView>
        <SourceSlab offset="0,0,0" count="4,10,5" step="1,1,1" />
        <DestSlab offset="0,0,0" />
      </Source>
    </Array>
  </Group>
</VRTDataset>
"""


###############################################################################


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_convert_creation_option(tmp_path):

    tmpfile = tmp_path / "out.nc"

    alg = get_mdim_convert_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmpfile
    alg["creation-option"] = ["COMPRESS=DEFLATE"]
    with gdal.quiet_errors():
        assert alg.Run()
    assert alg.Finalize()

    j = gdal.MultiDimInfo(tmpfile)
    assert j["arrays"]["Band1"]["structural_info"] == {"COMPRESS": "DEFLATE"}


###############################################################################


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_convert_completion_array(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim convert ../gdrivers/data/netcdf/byte.nc --array"
    ).split(" ")
    assert out == ["/x", "/y", "/Band1"]


###############################################################################


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_convert_completion_array_option(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim convert ../gdrivers/data/netcdf/byte.nc --array-option"
    ).split(" ")
    assert "USE_DEFAULT_FILL_AS_NODATA=" in out


###############################################################################
@pytest.mark.require_driver("ZARR")
def test_gdalalg_mdim_convert_valid_transpose_axis(tmp_path):

    tmpfile = tmp_path / "out.zarr"
    alg = get_mdim_convert_alg()
    alg["input"] = "../gdrivers/data/zarr/array_dimensions.zarr"
    alg["output"] = tmpfile
    alg["output-format"] = "ZARR"
    alg["array"] = "name=var,transpose=[a,1]"
    with pytest.raises(
        Exception,
        match=r"Invalid value for axis in transpose: a",
    ):
        alg.Run()
