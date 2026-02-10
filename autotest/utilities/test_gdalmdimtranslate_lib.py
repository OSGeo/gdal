#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalmdimtranslate
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import collections
import os
import pathlib
import struct

import gdaltest
import pytest

from osgeo import gdal

###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_no_arg(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    assert gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt")

    assert gdal.MultiDimInfo(tmpfile) == gdal.MultiDimInfo("data/mdim.vrt")


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_multidim_to_mem():

    out_ds = gdal.MultiDimTranslate("", "data/mdim.vrt", format="MEM")
    assert out_ds
    rg = out_ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray("time_increasing")
    assert ar
    assert ar.Read() == ["2010-01-01", "2011-01-01", "2012-01-01", "2013-01-01"]


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_multidim_to_unknown_format():

    with pytest.raises(
        Exception,
        match="Cannot determine output driver for dataset name 'unknown.unknown'",
    ):
        gdal.MultiDimTranslate("unknown.unknown", "data/mdim.vrt")


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_multidim_to_classic(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.tif"

    with pytest.raises(Exception):
        gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt")

    assert gdal.MultiDimTranslate(
        tmpfile,
        pathlib.Path("data/mdim.vrt"),
        arraySpecs=["/my_subgroup/array_in_subgroup"],
    )


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_multidim_1d_to_classic(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.tif"

    assert gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt", arraySpecs=["latitude"])
    ds = gdal.Open(tmpfile)
    band = ds.GetRasterBand(1)
    data = band.ReadRaster()
    assert len(data) == 10 * 4
    assert struct.unpack("f" * 10, data)[0] == 90.0
    ds = None


###############################################################################


def test_gdalmdimtranslate_classic_to_classic(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.tif"

    ds = gdal.MultiDimTranslate(tmpfile, "../gcore/data/byte.tif")
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_classic_to_multidim(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    tmpgtifffile = tmp_vsimem / "tmp.tif"
    if os.path.exists("../gcore/data/byte.tif.aux.xml"):
        os.unlink("../gcore/data/byte.tif.aux.xml")
    ds = gdal.Translate(tmpgtifffile, "../gcore/data/byte.tif")
    ds.SetSpatialRef(None)
    ds = None
    assert gdal.MultiDimTranslate(
        tmpfile, tmpgtifffile, arraySpecs=["band=1,dstname=ar,view=[newaxis,...]"]
    )
    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("utf-8")
    gdal.VSIFCloseL(f)
    # print(got_data)

    gdal.Unlink(tmpfile)
    gdal.Unlink(tmpgtifffile)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="X" size="20" indexingVariable="X" />
    <Dimension name="Y" size="20" indexingVariable="Y" />
    <Dimension name="newaxis" size="1" />
    <Array name="Y">
      <DataType>Float64</DataType>
      <DimensionRef ref="Y" />
      <RegularlySpacedValues start="3751290" increment="-60" />
    </Array>
    <Array name="X">
      <DataType>Float64</DataType>
      <DimensionRef ref="X" />
      <RegularlySpacedValues start="440750" increment="60" />
    </Array>
    <Array name="ar">
      <DataType>Byte</DataType>
      <DimensionRef ref="newaxis" />
      <DimensionRef ref="Y" />
      <DimensionRef ref="X" />
      <Source>
        <SourceFilename relativetoVRT="1">tmp.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceView>[newaxis,...]</SourceView>
        <SourceSlab offset="0,0,0" count="1,20,20" step="1,1,1" />
        <DestSlab offset="0,0,0" />
      </Source>
    </Array>
  </Group>
</VRTDataset>
"""


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_array(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    with pytest.raises(Exception):
        gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt", arraySpecs=["not_existing"])
    with pytest.raises(Exception):
        gdal.MultiDimTranslate(
            tmpfile,
            "data/mdim.vrt",
            arraySpecs=["name=my_variable_with_time_increasing,unknown_opt=foo"],
        )

    assert gdal.MultiDimTranslate(
        tmpfile, "data/mdim.vrt", arraySpecs=["my_variable_with_time_increasing"]
    )

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    gdal.Unlink(tmpfile)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="latitude" type="HORIZONTAL_Y" direction="NORTH" size="10" indexingVariable="latitude" />
    <Dimension name="longitude" type="HORIZONTAL_X" direction="EAST" size="10" indexingVariable="longitude" />
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
        <SourceSlab offset="0" count="10" step="1" />
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
        <SourceSlab offset="0,0,0" count="4,10,10" step="1,1,1" />
        <DestSlab offset="0,0,0" />
      </Source>
    </Array>
  </Group>
</VRTDataset>
"""


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_array_with_transpose_and_view(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    assert gdal.MultiDimTranslate(
        tmpfile,
        "data/mdim.vrt",
        arraySpecs=[
            "name=my_variable_with_time_increasing,dstname=foo,transpose=[1,2,0],view=[::-1,1,...]"
        ],
    )

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    gdal.Unlink(tmpfile)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="subset_latitude_9_-1_10" type="HORIZONTAL_Y" size="10" indexingVariable="subset_latitude_9_-1_10" />
    <Dimension name="time_increasing" type="TEMPORAL" size="4" indexingVariable="time_increasing" />
    <Array name="subset_latitude_9_-1_10">
      <DataType>Float32</DataType>
      <DimensionRef ref="subset_latitude_9_-1_10" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/latitude</SourceArray>
        <SourceView>[::-1]</SourceView>
        <SourceSlab offset="0" count="10" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
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
    <Array name="foo">
      <DataType>Int32</DataType>
      <DimensionRef ref="subset_latitude_9_-1_10" />
      <DimensionRef ref="time_increasing" />
      <Source>
        <SourceFilename>data/mdim.vrt</SourceFilename>
        <SourceArray>/my_variable_with_time_increasing</SourceArray>
        <SourceTranspose>1,2,0</SourceTranspose>
        <SourceView>[::-1,1,...]</SourceView>
        <SourceSlab offset="0,0" count="10,4" step="1,1" />
        <DestSlab offset="0,0" />
      </Source>
      <Attribute name="DIM_longitude_INDEX">
        <DataType>Int32</DataType>
        <Value>1</Value>
      </Attribute>
      <Attribute name="DIM_longitude_VALUE">
        <DataType>Float32</DataType>
        <Value>2.5</Value>
      </Attribute>
    </Array>
  </Group>
</VRTDataset>
"""


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_group(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    with pytest.raises(Exception):
        gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt", groupSpecs=["not_existing"])
    with pytest.raises(Exception):
        gdal.MultiDimTranslate(
            tmpfile, "data/mdim.vrt", groupSpecs=["name=my_subgroup,unknown_opt=foo"]
        )

    assert gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt", groupSpecs=["my_subgroup"])

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


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_two_groups(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    assert gdal.MultiDimTranslate(
        tmpfile,
        "data/mdim.vrt",
        groupSpecs=["my_subgroup", "name=other_subgroup,dstname=renamed"],
    )

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    gdal.Unlink(tmpfile)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Group name="my_subgroup">
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
    <Group name="renamed">
      <Attribute name="foo">
        <DataType>String</DataType>
        <Value>bar</Value>
      </Attribute>
    </Group>
  </Group>
</VRTDataset>
"""


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_subset(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    with pytest.raises(Exception):
        gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt", subsetSpecs=["latitude("])
    with pytest.raises(Exception):
        gdal.MultiDimTranslate(tmpfile, "data/mdim.vrt", subsetSpecs=["latitude(1"])
    with pytest.raises(Exception):
        gdal.MultiDimTranslate(
            tmpfile, "data/mdim.vrt", subsetSpecs=["latitude(1,2,3)"]
        )

    for subset_spec, success, expected_view in [
        # Increasing numeric variable
        ("longitude(-180,-0.01)", False, None),  # All below min
        ("longitude(-180)", False, None),
        ("longitude(22.51,100)", False, None),  # All above max
        ("longitude(22.51)", False, None),
        ("longitude(-0.01,22.51)", True, None),  # Encompassing whole range
        ("longitude(0,22.5)", True, None),  # Exact range
        ("longitude(0)", True, "[0]"),
        ("longitude(2.5)", True, "[1]"),
        ("longitude(20)", True, "[8]"),
        ("longitude(22.5)", True, "[9]"),
        ("longitude(0,0)", True, "[0:1:1]"),
        ("longitude(-0.01,0.01)", True, "[0:1:1]"),
        ("longitude(0,0.01)", True, "[0:1:1]"),
        ("longitude(-0.01,0)", True, "[0:1:1]"),
        ("longitude(-0.01,22.49)", True, "[0:9:1]"),
        ("longitude(0.01,22.49)", True, "[1:9:1]"),
        ("longitude(0.01,22.51)", True, "[1:10:1]"),
        ("longitude(22.5,22.5)", True, "[9:10:1]"),
        ("longitude(22.49,22.5)", True, "[9:10:1]"),
        ("longitude(22.49,22.51)", True, "[9:10:1]"),
        ("longitude(22.5,22.51)", True, "[9:10:1]"),
        # Decreasing numeric variable
        ("latitude(-180,67.49)", False, None),  # All below min
        ("latitude(-180)", False, None),
        ("latitude(90.01,100)", False, None),  # All above max
        ("latitude(90.01)", False, None),
        ("latitude(64.49,90.01)", True, None),  # Encompassing whole range
        ("latitude(67.5,90)", True, None),  # Exact range
        ("latitude(67.5)", True, "[9]"),
        ("latitude(70)", True, "[8]"),
        ("latitude(87.5)", True, "[1]"),
        ("latitude(90)", True, "[0]"),
        ("latitude(70,87.5)", True, "[1:9:1]"),
        ("latitude(90,90)", True, "[0:1:1]"),
        ("latitude(89.99,90)", True, "[0:1:1]"),
        ("latitude(90,90.01)", True, "[0:1:1]"),
        ("latitude(67.5,67.5)", True, "[9:10:1]"),
        ("latitude(67.5,67.51)", True, "[9:10:1]"),
        ("latitude(67.49,67.5)", True, "[9:10:1]"),
        # Increasing string variable
        ('time_increasing("2008-01-01","2009-01-01")', False, None),  # All below min
        ('time_increasing("2008-01-01")', False, None),
        ('time_increasing("2014-01-01","2016-01-01")', False, None),  # All above max
        ('time_increasing("2014-01-01")', False, None),
        (
            'time_increasing("2009-01-01","2014-01-01")',
            True,
            None,
        ),  # Encompassing whole range
        ('time_increasing("2010-01-01","2013-01-01")', True, None),  # Exact range
        ('time_increasing("2010-01-01")', True, "[0]"),
        ('time_increasing("2011-01-01")', True, "[1]"),
        ('time_increasing("2012-01-01")', True, "[2]"),
        ('time_increasing("2013-01-01")', True, "[3]"),
        ('time_increasing("2009-12-31","2010-01-02")', True, "[0:1:1]"),
        ('time_increasing("2009-12-13","2010-01-01")', True, "[0:1:1]"),
        ('time_increasing("2010-01-01","2010-01-01")', True, "[0:1:1]"),
        ('time_increasing("2010-01-01","2010-01-02")', True, "[0:1:1]"),
        ('time_increasing("2011-01-01","2012-01-01")', True, "[1:3:1]"),
        ('time_increasing("2012-12-31","2013-01-02")', True, "[3:4:1]"),
        ('time_increasing("2012-12-13","2013-01-01")', True, "[3:4:1]"),
        ('time_increasing("2013-01-01","2013-01-01")', True, "[3:4:1]"),
        ('time_increasing("2013-01-01","2013-01-02")', True, "[3:4:1]"),
        # Decreasing string variable
        ('time_decreasing("2008-01-01","2009-01-01")', False, None),  # All below min
        ('time_decreasing("2008-01-01")', False, None),
        ('time_decreasing("2014-01-01","2016-01-01")', False, None),  # All above max
        ('time_decreasing("2014-01-01")', False, None),
        (
            'time_decreasing("2009-01-01","2014-01-01")',
            True,
            None,
        ),  # Encompassing whole range
        ('time_decreasing("2010-01-01","2013-01-01")', True, None),  # Exact range
        ('time_decreasing("2010-01-01")', True, "[3]"),
        ('time_decreasing("2011-01-01")', True, "[2]"),
        ('time_decreasing("2012-01-01")', True, "[1]"),
        ('time_decreasing("2013-01-01")', True, "[0]"),
        ('time_decreasing("2009-12-31","2010-01-02")', True, "[3:4:1]"),
        ('time_decreasing("2009-12-13","2010-01-01")', True, "[3:4:1]"),
        ('time_decreasing("2010-01-01","2010-01-01")', True, "[3:4:1]"),
        ('time_decreasing("2010-01-01","2010-01-02")', True, "[3:4:1]"),
        ('time_decreasing("2011-01-01","2012-01-01")', True, "[1:3:1]"),
        ('time_decreasing("2012-12-31","2013-01-02")', True, "[0:1:1]"),
        ('time_decreasing("2012-12-13","2013-01-01")', True, "[0:1:1]"),
        ('time_decreasing("2013-01-01","2013-01-01")', True, "[0:1:1]"),
        ('time_decreasing("2013-01-01","2013-01-02")', True, "[0:1:1]"),
    ]:
        with gdaltest.disable_exceptions(), gdaltest.error_handler():
            res = (
                gdal.MultiDimTranslate(
                    tmpfile,
                    "data/mdim.vrt",
                    arraySpecs=[subset_spec[0 : subset_spec.find("(")]],
                    subsetSpecs=[subset_spec],
                )
                is not None
            )
        assert res == success, subset_spec
        if not success:
            continue

        f = gdal.VSIFOpenL(tmpfile, "rb")
        got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
        gdal.VSIFCloseL(f)
        # print(got_data)/

        gdal.Unlink(tmpfile)
        if expected_view:
            assert expected_view in got_data, subset_spec
        else:
            assert "SourceView" not in got_data, subset_spec

    assert gdal.MultiDimTranslate(
        tmpfile,
        "data/mdim.vrt",
        subsetSpecs=["latitude(70,87.5)", 'time_increasing("2012-01-01")'],
    )

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


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_scaleaxes(tmp_vsimem):

    tmpfile = tmp_vsimem / "out.vrt"
    assert gdal.MultiDimTranslate(
        tmpfile,
        "data/mdim.vrt",
        arraySpecs=["my_variable_with_time_increasing"],
        scaleAxesSpecs=["longitude(2)"],
    )

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


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_dims_with_same_name_different_size(tmp_vsimem):

    srcfile = tmp_vsimem / "in.vrt"
    gdal.FileFromMemBuffer(
        srcfile,
        """<VRTDataset>
    <Group name="/">
        <Array name="X">
            <DataType>Float64</DataType>
            <Dimension name="dim0" size="2"/>
        </Array>
        <Array name="Y">
            <DataType>Float64</DataType>
            <Dimension name="dim0" size="3"/>
        </Array>
    </Group>
</VRTDataset>""",
    )

    tmpfile = tmp_vsimem / "test.vrt"
    gdal.MultiDimTranslate(tmpfile, srcfile, groupSpecs=["/"], format="VRT")

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="dim0" size="2" />
    <Dimension name="dim0_2" size="3" />
    <Array name="X">
      <DataType>Float64</DataType>
      <DimensionRef ref="dim0" />
      <Source>
        <SourceFilename relativetoVRT="1">in.vrt</SourceFilename>
        <SourceArray>/X</SourceArray>
        <SourceSlab offset="0" count="2" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
    <Array name="Y">
      <DataType>Float64</DataType>
      <DimensionRef ref="dim0_2" />
      <Source>
        <SourceFilename relativetoVRT="1">in.vrt</SourceFilename>
        <SourceArray>/Y</SourceArray>
        <SourceSlab offset="0" count="3" step="1" />
        <DestSlab offset="0" />
      </Source>
    </Array>
  </Group>
</VRTDataset>
"""
    gdal.Unlink(tmpfile)
    gdal.Unlink(srcfile)


@pytest.mark.require_driver("netCDF")
def test_gdalmdimtranslate_array_with_view():
    ds = gdal.MultiDimTranslate(
        "",
        "../gdrivers/data/netcdf/byte_no_cf.nc",
        arraySpecs=["name=Band1,view=[::2,::4]"],
        format="MEM",
    )
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("Band1")
    dims = ar.GetDimensions()
    assert dims[0].GetSize() == 10
    assert dims[1].GetSize() == 5


@pytest.mark.require_driver("netCDF")
def test_gdalmdimtranslate_array_resample():
    ds = gdal.MultiDimTranslate(
        "",
        "../gdrivers/data/netcdf/fake_EMIT_L2A.nc",
        arraySpecs=["name=reflectance,resample=true"],
        format="MEM",
    )
    rg = ds.GetRootGroup()
    resampled_ar = rg.OpenMDArray("reflectance")
    dims = resampled_ar.GetDimensions()
    assert dims[0].GetName() == "lat"
    assert dims[0].GetSize() == 3
    assert dims[1].GetName() == "lon"
    assert dims[1].GetSize() == 3
    assert dims[2].GetName() == "bands"
    assert dims[2].GetSize() == 2
    assert resampled_ar.GetDataType() == gdal.ExtendedDataType.Create(gdal.GDT_Float32)
    assert resampled_ar.GetSpatialRef().GetAuthorityCode(None) == "4326"
    assert struct.unpack("f" * (3 * 3 * 2), resampled_ar.Read()) == (
        -9999.0,
        -9999.0,
        -9999.0,
        -9999.0,
        -9999.0,
        -9999.0,
        -9999.0,
        -9999.0,
        30.0,
        -30.0,
        40.0,
        -40.0,
        -9999.0,
        -9999.0,
        10.0,
        -10.0,
        20.0,
        -20.0,
    )

    lat = dims[0].GetIndexingVariable()
    assert lat
    assert struct.unpack("d" * 3, lat.Read()) == (3.5, 2.5, 1.5)

    lon = dims[1].GetIndexingVariable()
    assert lon
    assert struct.unpack("d" * 3, lon.Read()) == (1.5, 2.5, 3.5)


def XXXX_test_all():
    while True:
        test_gdalmdimtranslate_no_arg()
        test_gdalmdimtranslate_multidim_to_classic()
        test_gdalmdimtranslate_classic_to_classic()
        test_gdalmdimtranslate_classic_to_multidim()
        test_gdalmdimtranslate_array()
        test_gdalmdimtranslate_array_with_transpose_and_view()
        test_gdalmdimtranslate_group()
        test_gdalmdimtranslate_two_groups()
        test_gdalmdimtranslate_subset()
        test_gdalmdimtranslate_scaleaxes()


###############################################################################
# Test option argument handling


def test_gdalmdimtranslate_dict_arguments():

    opt = gdal.MultiDimTranslateOptions(
        "__RETURN_OPTION_LIST__",
        creationOptions=collections.OrderedDict(
            (("COMPRESS", "DEFLATE"), ("LEVEL", 4))
        ),
    )

    co_idx = opt.index("-co")

    assert opt[co_idx : co_idx + 4] == ["-co", "COMPRESS=DEFLATE", "-co", "LEVEL=4"]


def test_gdalmdimtranslate_from_gtiff_multiband(tmp_vsimem):
    gdal.MultiDimTranslate(
        tmp_vsimem / "out.vrt",
        "../gdrivers/data/small_world.tif",
        arraySpecs=["foo"],
        format="VRT",
    )

    ds = gdal.OpenEx(tmp_vsimem / "out.vrt", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("foo")
    assert ar.GetDimensionCount() == 3
    assert ar.GetDimensions()[0].GetIndexingVariable().Read() == [
        "Band 1",
        "Band 2",
        "Band 3",
    ]
    assert (
        ar.GetDimensions()[1].GetIndexingVariable().GetDimensions()[0].GetSize() == 200
    )
    assert (
        ar.GetDimensions()[2].GetIndexingVariable().GetDimensions()[0].GetSize() == 400
    )


@pytest.mark.require_driver("netCDF")
def test_gdalmdimtranslate_array_copy_blocksize(tmp_path):

    gdal.MultiDimTranslate(
        tmp_path / "out.nc",
        "../gdrivers/data/netcdf/byte_chunked_not_multiple.nc",
    )
    ds = gdal.Open(tmp_path / "out.nc")
    assert ds.GetRasterBand(1).GetBlockSize() == [15, 6]
