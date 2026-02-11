#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in VRT driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import math
import struct

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


def test_vrtmultidim_dimension():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="X" size="2" type="foo" direction="bar" indexingVariable="X"/>
        <Dimension name="Y" size="1234567890123"/>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert rg.GetName() == "/"
    dims = rg.GetDimensions()
    assert len(dims) == 2
    dim_0 = dims[0]
    assert dim_0.GetName() == "X"
    assert dim_0.GetSize() == 2
    assert dim_0.GetType() == "foo"
    assert dim_0.GetDirection() == "bar"
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert not dim_0.GetIndexingVariable()
        assert gdal.GetLastErrorMsg() == "Cannot find variable X"
    dim_1 = dims[1]
    assert dim_1.GetName() == "Y"
    assert dim_1.GetSize() == 1234567890123

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group MISSING_name="/">
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="INVALID">
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension MISSING_name="X" size="1"/>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="X" MISSING_size="1"/>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds


def test_vrtmultidim_attribute():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Attribute name="foo">
            <DataType>String</DataType>
            <Value>bar</Value>
            <Value>baz</Value>
        </Attribute>
        <Attribute name="bar">
            <DataType>Float64</DataType>
            <Value>1.25125</Value>
        </Attribute>
        <Attribute name="empty">
            <DataType>Float64</DataType>
        </Attribute>
        <Array name="ar">
            <DataType>Float32</DataType>
            <Attribute name="foo">
                <DataType>String</DataType>
                <Value>bar</Value>
            </Attribute>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    attrs = rg.GetAttributes()
    assert len(attrs) == 3

    foo = next((x for x in attrs if x.GetName() == "foo"), None)
    assert foo
    assert foo.GetDataType().GetClass() == gdal.GEDTC_STRING
    assert foo.Read() == ["bar", "baz"]

    bar = next((x for x in attrs if x.GetName() == "bar"), None)
    assert bar
    assert bar.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert bar.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert bar.Read() == 1.25125

    empty = next((x for x in attrs if x.GetName() == "empty"), None)
    assert empty
    assert empty.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert empty.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert empty.Read() == 0.0

    ar = rg.OpenMDArray("ar")
    assert ar
    attrs = ar.GetAttributes()
    assert len(attrs) == 1

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Attribute MISSING_name="foo">
                <DataType>String</DataType>
                <Value>bar</Value>
        </Attribute>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Attribute name="foo">
                <MISSING_DataType>String</MISSING_DataType>
                <Value>bar</Value>
        </Attribute>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Attribute name="foo">
                <DataType>INVALID</DataType>
                <Value>bar</Value>
        </Attribute>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds


def test_vrtmultidim_subgroup_and_cross_references():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="X" size="20" indexingVariable="X"/>
        <Dimension name="Y" size="30" indexingVariable="/Y"/>
        <Array name="Y">
            <DataType>Float32</DataType>
            <DimensionRef ref="Y"/>
        </Array>
        <Array name="X">
            <DataType>Float32</DataType>
            <DimensionRef ref="/X"/>
        </Array>
        <Group name="subgroup">
            <Dimension name="X" size="2" indexingVariable="X"/>
            <Dimension name="Y" size="3" indexingVariable="/subgroup/Y"/>
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="/subgroup/X"/>
            </Array>
            <Array name="Y">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
            </Array>
        </Group>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["subgroup"]
    subgroup = rg.OpenGroup("subgroup")
    assert subgroup
    dims = subgroup.GetDimensions()
    assert len(dims) == 2

    dim_0 = dims[0]
    assert dim_0.GetName() == "X"
    assert dim_0.GetSize() == 2
    indexing_var = dim_0.GetIndexingVariable()
    assert indexing_var
    assert indexing_var.GetName() == "X"
    assert indexing_var.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert indexing_var.GetDimensionCount() == 1
    assert indexing_var.GetDimensions()[0].GetSize() == 2

    dim_1 = dims[1]
    assert dim_1.GetName() == "Y"
    assert dim_1.GetSize() == 3
    indexing_var = dim_1.GetIndexingVariable()
    assert indexing_var
    assert indexing_var.GetName() == "Y"
    assert indexing_var.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert indexing_var.GetDimensionCount() == 1
    assert indexing_var.GetDimensions()[0].GetSize() == 3

    assert rg.GetMDArrayNames() == ["Y", "X"]
    X = rg.OpenMDArray("X")
    assert X
    assert X.GetDataType().GetNumericDataType() == gdal.GDT_Float32
    assert X.GetDimensionCount() == 1
    assert X.GetDimensions()[0].GetSize() == 20
    Y = rg.OpenMDArray("Y")
    assert Y
    assert Y.GetDataType().GetNumericDataType() == gdal.GDT_Float32
    assert Y.GetDimensionCount() == 1
    assert Y.GetDimensions()[0].GetSize() == 30

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Group MISSING_name="subgroup"/>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array MISSING_name="X">
                <DataType>Float64</DataType>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array name="X">
                <MISSING_DataType>Float64</MISSING_DataType>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>invalid</DataType>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef MISSING_ref="X"/>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="INVALID"/>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="/INVALID"/>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="/INVALID_GROUP/INVALID"/>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds


def test_vrtmultidim_srs():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="X" size="4"/>
        <Array name="X">
            <DataType>Float64</DataType>
            <SRS>EPSG:32632</SRS>
        </Array>
        <Array name="Y">
            <DataType>Float64</DataType>
            <SRS dataAxisToSRSAxisMapping="2,1">EPSG:32632</SRS>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()

    X = rg.OpenMDArray("X")
    srs = X.GetSpatialRef()
    assert srs

    Y = rg.OpenMDArray("Y")
    srs = Y.GetSpatialRef()
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


def test_vrtmultidim_nodata_unit_offset_scale():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Array name="ar1">
            <DataType>Float64</DataType>
            <Unit>foo</Unit>
            <NoDataValue>1.25125</NoDataValue>
            <Offset>1.5</Offset>
            <Scale>2.5</Scale>
        </Array>
        <Array name="ar2">
            <DataType>Float64</DataType>
            <NoDataValue>nan</NoDataValue>
        </Array>
        <Array name="ar3">
            <DataType>Float64</DataType>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray("ar1")
    assert ar.GetNoDataValueAsDouble() == 1.25125
    assert struct.unpack("d", ar.Read()) == (1.25125,)
    assert ar.GetUnit() == "foo"
    assert ar.GetOffset() == 1.5
    assert ar.GetScale() == 2.5

    ar = rg.OpenMDArray("ar2")
    assert math.isnan(ar.GetNoDataValueAsDouble())
    assert math.isnan(struct.unpack("d", ar.Read())[0])
    assert ar.GetOffset() is None
    assert ar.GetScale() is None

    ar = rg.OpenMDArray("ar3")
    assert ar.GetNoDataValueAsDouble() is None
    assert struct.unpack("d", ar.Read()) == (0,)


def test_vrtmultidim_RegularlySpacedValues():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="X" size="4"/>
        <Array name="X">
            <DataType>Float64</DataType>
            <DimensionRef ref="X"/>
            <RegularlySpacedValues start="0.5" increment="10.5"/>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    X = rg.OpenMDArray("X")
    assert struct.unpack("d" * 4, X.Read()) == (0.5, 11.0, 21.5, 32.0)
    assert struct.unpack(
        "d" * 2, X.Read(array_start_idx=[1], count=[2], array_step=[2])
    ) == (11.0, 32.0)

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="X" size="4"/>
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="X"/>
                <RegularlySpacedValues MISSING_start="0.5" increment="10.5"/>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="X" size="4"/>
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="X"/>
                <RegularlySpacedValues start="0.5" MISSING_increment="10.5"/>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds


def test_vrtmultidim_ConstantValue():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4"/>
        <Dimension name="X" size="3"/>
        <Array name="ar">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <ConstantValue>5</ConstantValue>
            <ConstantValue offset="1,1">10</ConstantValue>
            <ConstantValue offset="1,2" count="2,1">100</ConstantValue>
        </Array>
        <Array name="no_dim">
            <DataType>Float64</DataType>
            <ConstantValue>50</ConstantValue>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("ar")
    got = struct.unpack("d" * 12, ar.Read())
    assert got == (5, 5, 5, 5, 10, 100, 5, 10, 100, 5, 10, 10)
    assert struct.unpack("d" * 4, ar.Read(array_start_idx=[2, 1], count=[2, 2])) == (
        10,
        100,
        10,
        10,
    )

    ar = rg.OpenMDArray("no_dim")
    assert struct.unpack("d", ar.Read()) == (50,)

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="Y" size="4"/>
            <Dimension name="X" size="3"/>
            <Array name="ar">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="X"/>
                <!-- not enough values in offset -->
                <ConstantValue offset="1" count="2,1">10</ConstantValue>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="Y" size="4"/>
            <Dimension name="X" size="3"/>
            <Array name="ar">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="X"/>
                <!-- invalid values in offset -->
                <ConstantValue offset="1,-1" count="2,1">10</ConstantValue>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="Y" size="4"/>
            <Dimension name="X" size="3"/>
            <Array name="ar">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="X"/>
                <!-- not enough values in count -->
                <ConstantValue offset="1,2" count="2">10</ConstantValue>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="Y" size="4"/>
            <Dimension name="X" size="3"/>
            <Array name="ar">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="X"/>
                <!-- invalid values in count -->
                <ConstantValue offset="1,2" count="2,0">10</ConstantValue>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="Y" size="4"/>
            <Dimension name="X" size="3"/>
            <Array name="ar">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="X"/>
                <!-- invalid values in count -->
                <ConstantValue offset="1,2" count="2,-1">10</ConstantValue>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds


def test_vrtmultidim_InlineValues():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4"/>
        <Dimension name="X" size="3"/>
        <Array name="ar">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <InlineValues>
                0 1 2
                3 4 5
                6 7 8
                9 10 11
            </InlineValues>
            <InlineValues offset="1,1">-4 -5
                                       -7 -8
                                       -10 -11
            </InlineValues>
            <InlineValues offset="1,2" count="2,1">100 101</InlineValues>
        </Array>
        <Array name="no_dim">
            <DataType>Float64</DataType>
            <InlineValues>50</InlineValues>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray("ar")
    got = struct.unpack("d" * 12, ar.Read())
    assert got == (0.0, 1.0, 2.0, 3.0, -4.0, 100.0, 6.0, -7.0, 101.0, 9.0, -10.0, -11.0)
    assert struct.unpack("d" * 4, ar.Read(array_start_idx=[2, 1], count=[2, 2])) == (
        -7,
        101,
        -10,
        -11,
    )
    assert struct.unpack(
        "d" * 4, ar.Read(array_start_idx=[2, 1], count=[2, 2], array_step=[-1, -1])
    ) == (-7, 6, -4, 3)

    ar = rg.OpenMDArray("no_dim")
    assert struct.unpack("d", ar.Read()) == (50,)

    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Dimension name="Y" size="4000000"/>
            <Dimension name="X" size="3000000"/>
            <Array name="ar">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="X"/>
                <!-- check that no denial of service happens -->
                <InlineValues>10</InlineValues>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert not ds


def test_vrtmultidim_Source():
    def f():

        gdal.FileFromMemBuffer(
            "/vsimem/src.vrt",
            """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4"/>
        <Dimension name="X" size="3"/>
        <Array name="ar_source">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <InlineValues>
                0 1 2
                3 4 5
                6 7 8
                9 10 11
            </InlineValues>
        </Array>
    </Group>
</VRTDataset>""",
        )

        ds = gdal.OpenEx(
            """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4"/>
        <Dimension name="X" size="3"/>

        <Array name="ar">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
            </Source>
        </Array>

        <Array name="ar_with_offset">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceSlab offset="1,1" count="2,2" step="2,1"/>
                <DestSlab offset="2,1"/>
            </Source>
        </Array>

        <Array name="ar_transposed">
            <DataType>Float64</DataType>
            <DimensionRef ref="X"/>
            <DimensionRef ref="Y"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceTranspose>1,0</SourceTranspose>
            </Source>
        </Array>

        <Array name="ar_view">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceView>[::-1,...]</SourceView>
            </Source>
        </Array>

        <Array name="ar_non_existing_source_with_offset">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/non_existing.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceSlab offset="1,1" count="2,2" step="2,1"/>
                <DestSlab offset="2,1"/>
            </Source>
        </Array>

        <Array name="ar_non_existing_source">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/non_existing.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
            </Source>
        </Array>

        <Array name="ar_invalid_source_slab_offset">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceSlab offset="4,1" count="2,2" step="2,1"/>
                <DestSlab offset="2,1"/>
            </Source>
        </Array>

        <Array name="ar_invalid_number_of_dimensions">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceView>[0]</SourceView>
            </Source>
        </Array>

        <Array name="ar_non_existing_array_source">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>non_existing_array_source</SourceArray>
            </Source>
        </Array>

        <Array name="ar_view_invalid">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceView>[100,...]</SourceView>
            </Source>
        </Array>

        <Array name="ar_transposed_invalid">
            <DataType>Float64</DataType>
            <DimensionRef ref="X"/>
            <DimensionRef ref="Y"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
                <SourceTranspose>1</SourceTranspose>
            </Source>
        </Array>

    </Group>
</VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert ds
        rg = ds.GetRootGroup()

        ar = rg.OpenMDArray("ar")
        assert ar
        got = struct.unpack("d" * 12, ar.Read())
        assert got == (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)

        ar_with_offset = rg.OpenMDArray("ar_with_offset")
        assert ar_with_offset
        got = struct.unpack("d" * 12, ar_with_offset.Read())
        assert got == (0, 0, 0, 0, 0, 0, 0, 4, 5, 0, 10, 11)
        assert struct.unpack(
            "d" * 2, ar_with_offset.Read(array_start_idx=[2, 0], count=[1, 2])
        ) == (0, 4)
        assert struct.unpack(
            "d" * 2, ar_with_offset.Read(array_start_idx=[3, 1], count=[1, 2])
        ) == (10, 11)
        assert struct.unpack(
            "d" * 4,
            ar_with_offset.Read(
                array_start_idx=[2, 1], count=[2, 2], array_step=[-1, -1]
            ),
        ) == (4, 0, 0, 0)

        ar_transposed = rg.OpenMDArray("ar_transposed")
        assert ar_transposed
        got = struct.unpack("d" * 12, ar_transposed.Read())
        assert got == (0, 3, 6, 9, 1, 4, 7, 10, 2, 5, 8, 11)

        ar_view = rg.OpenMDArray("ar_view")
        assert ar_view
        got = struct.unpack("d" * 12, ar_view.Read())
        assert got == (9, 10, 11, 6, 7, 8, 3, 4, 5, 0, 1, 2)

        # Source does not exist, but we don't request an area where it is active
        # so we should not try to open it
        ar = rg.OpenMDArray("ar_non_existing_source_with_offset")
        assert ar
        assert struct.unpack(
            "d" * 2, ar.Read(array_start_idx=[0, 0], count=[1, 2])
        ) == (0, 0)

        ar = rg.OpenMDArray("ar_non_existing_source")
        assert ar
        with gdal.quiet_errors():
            assert not ar.Read()

        ar = rg.OpenMDArray("ar_invalid_source_slab_offset")
        assert ar
        with gdal.quiet_errors():
            assert not ar.Read()

        ar = rg.OpenMDArray("ar_invalid_number_of_dimensions")
        assert ar
        with gdal.quiet_errors():
            assert not ar.Read()

        ar = rg.OpenMDArray("ar_non_existing_array_source")
        assert ar
        with gdal.quiet_errors():
            assert not ar.Read()

        ar = rg.OpenMDArray("ar_view_invalid")
        assert ar
        with gdal.quiet_errors():
            assert not ar.Read()

        ar = rg.OpenMDArray("ar_transposed_invalid")
        assert ar
        with gdal.quiet_errors():
            assert not ar.Read()

        gdal.Unlink("/vsimem/src.vrt")

        # Check that the cache is correctly working by opening a second
        # dataset after having remove the source
        ds2 = gdal.OpenEx(
            """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4"/>
        <Dimension name="X" size="3"/>

        <Array name="ar">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
            </Source>
        </Array>
    </Group>
</VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
        assert ds2
        rg2 = ds2.GetRootGroup()
        ar2 = rg2.OpenMDArray("ar")
        got = struct.unpack("d" * 12, ar2.Read())
        assert got == (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)

    f()

    # Check that the cache is correctly working: we should get an error
    # now that all referencing arrays have been cleaned up
    ds2 = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4"/>
        <Dimension name="X" size="3"/>

        <Array name="ar">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>/vsimem/src.vrt</SourceFilename>
                <SourceArray>ar_source</SourceArray>
            </Source>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds2
    rg2 = ds2.GetRootGroup()
    ar2 = rg2.OpenMDArray("ar")
    with gdal.quiet_errors():
        assert not ar2.Read()


def test_vrtmultidim_Source_classic_dataset():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4"/>
        <Dimension name="X" size="3"/>

        <Array name="ar">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>data/byte.tif</SourceFilename>
                <SourceBand>1</SourceBand>
                <SourceSlab offset="1,1" count="2,2" step="2,1"/>
                <DestSlab offset="2,1"/>
            </Source>
        </Array>

        <Array name="ar_wrong_band">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <DimensionRef ref="X"/>
            <Source>
                <SourceFilename>data/byte.tif</SourceFilename>
                <SourceBand>2</SourceBand>
            </Source>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray("ar")
    assert ar
    got = struct.unpack("d" * 12, ar.Read())
    assert got == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 132.0, 140.0, 0.0, 156.0, 132.0)

    ar_wrong_band = rg.OpenMDArray("ar_wrong_band")
    assert ar_wrong_band
    with gdal.quiet_errors():
        assert not ar_wrong_band.Read()


def _validate(content):

    try:
        from lxml import etree
    except ImportError:
        return

    import os

    gdal_data = gdal.GetConfigOption("GDAL_DATA")
    if gdal_data is None:
        print("GDAL_DATA not defined")
        return

    doc = etree.XML(content)
    try:
        schema_content = open(os.path.join(gdal_data, "gdalvrt.xsd"), "rb").read()
    except IOError:
        print("Cannot read gdalvrt.xsd schema")
        return
    schema = etree.XMLSchema(etree.XML(schema_content))
    schema.assertValid(doc)


def test_vrtmultidim_serialize():

    tmpfile = "/vsimem/test.vrt"
    gdal.FileFromMemBuffer(
        tmpfile,
        """<VRTDataset>
    <Group name="/">
        <Dimension name="Y" size="4" indexingVariable="Y"/>
        <Dimension name="X" size="3"/>
        <Array name="Y">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <RegularlySpacedValues start="0.5" increment="10.5"/>
        </Array>
        <Array name="ar">
            <DataType>Float64</DataType>
            <DimensionRef ref="Y"/>
            <Dimension name="myX" size="3"/>
            <Source>
                <SourceFilename>data/byte.tif</SourceFilename>
                <SourceBand>1</SourceBand>
                <SourceSlab offset="1,1" count="2,2" step="2,1"/>
                <DestSlab offset="2,1"/>
            </Source>
            <Source>
                <SourceFilename>foo</SourceFilename>
                <SourceArray>the_array</SourceArray>
                <SourceTranspose>1,0</SourceTranspose>
                <SourceView>[...]</SourceView>
            </Source>
            <ConstantValue>15</ConstantValue>
            <InlineValues>
                0 1 2
                3 4 5
                6 7 8
                9 10 11
            </InlineValues>
            <Attribute name="bar">
                <DataType>Int32</DataType>
                <Value>1</Value>
            </Attribute>
        </Array>
        <Array name="ar_string_no_dim">
            <DataType>String</DataType>
            <InlineValuesWithValueElement>
                <Value>foo</Value>
            </InlineValuesWithValueElement>
        </Array>
        <Array name="ar_string_with_dim">
            <DataType>String</DataType>
             <DimensionRef ref="X"/>
            <InlineValuesWithValueElement>
                <Value>foo</Value>
                <Value>bar</Value>
                <Value>baz</Value>
            </InlineValuesWithValueElement>
        </Array>
        <Group name="subgroup">
            <Dimension name="Y" size="5"/>
            <Array name="ar">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="/Y"/>
            </Array>
        </Group>
    </Group>
</VRTDataset>""",
    )
    ds = gdal.OpenEx(tmpfile, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
    rg = ds.GetRootGroup()
    ds = None
    attr = rg.CreateAttribute("foo", [], gdal.ExtendedDataType.CreateString())
    attr.Write("bar")
    attr = None
    rg = None

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="X" size="3" />
    <Dimension name="Y" size="4" indexingVariable="Y" />
    <Attribute name="foo">
      <DataType>String</DataType>
      <Value>bar</Value>
    </Attribute>
    <Array name="Y">
      <DataType>Float64</DataType>
      <DimensionRef ref="Y" />
      <RegularlySpacedValues start="0.5" increment="10.5" />
    </Array>
    <Array name="ar">
      <DataType>Float64</DataType>
      <DimensionRef ref="Y" />
      <Dimension name="myX" size="3" />
      <Source>
        <SourceFilename>data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceSlab offset="1,1" count="2,2" step="2,1" />
        <DestSlab offset="2,1" />
      </Source>
      <Source>
        <SourceFilename>foo</SourceFilename>
        <SourceArray>the_array</SourceArray>
        <SourceTranspose>1,0</SourceTranspose>
        <SourceView>[...]</SourceView>
        <SourceSlab offset="0,0" count="0,0" step="1,1" />
        <DestSlab offset="0,0" />
      </Source>
      <ConstantValue offset="0,0" count="4,3">15</ConstantValue>
      <InlineValues offset="0,0" count="4,3">0 1 2 3 4 5 6 7 8 9 10 11</InlineValues>
      <Attribute name="bar">
        <DataType>Int32</DataType>
        <Value>1</Value>
      </Attribute>
    </Array>
    <Array name="ar_string_no_dim">
      <DataType>String</DataType>
      <InlineValuesWithValueElement>
        <Value>foo</Value>
      </InlineValuesWithValueElement>
    </Array>
    <Array name="ar_string_with_dim">
      <DataType>String</DataType>
      <DimensionRef ref="X" />
      <InlineValuesWithValueElement offset="0" count="3">
        <Value>foo</Value>
        <Value>bar</Value>
        <Value>baz</Value>
      </InlineValuesWithValueElement>
    </Array>
    <Group name="subgroup">
      <Dimension name="Y" size="5" />
      <Array name="ar">
        <DataType>Float64</DataType>
        <DimensionRef ref="Y" />
        <DimensionRef ref="/Y" />
      </Array>
    </Group>
  </Group>
</VRTDataset>
"""

    _validate(got_data)

    gdal.Unlink(tmpfile)


def test_vrtmultidim_createcopy():

    src_ds = gdal.GetDriverByName("MEM").CreateMultiDimensional("myds")
    src_rg = src_ds.GetRootGroup()
    src_dim = src_rg.CreateDimension("dim", "", "", 3)
    src_ar = src_rg.CreateMDArray(
        "array", [src_dim], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    src_ar.Write(struct.pack("d" * 3, 1.5, 2.5, 3.5))
    src_ar.SetNoDataValueDouble(1.5)
    src_ar.SetUnit("foo")
    src_ar.SetOffset(2.5)
    src_ar.SetScale(3.5)
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        'ENGCRS["FOO",EDATUM["BAR"],CS[vertical,1],AXIS["foo",up,LENGTHUNIT["m",1]]]'
    )
    sr.SetDataAxisToSRSAxisMapping([2, 1])
    src_ar.SetSpatialRef(sr)
    attr = src_ar.CreateAttribute("foo", [], gdal.ExtendedDataType.CreateString())
    attr.Write("bar")
    attr = None

    with gdal.quiet_errors():
        gdal.GetDriverByName("VRT").CreateCopy("/i_do/not_exist", src_ds)

    tmpfile = "/vsimem/test.vrt"
    assert gdal.GetDriverByName("VRT").CreateCopy(tmpfile, src_ds)

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="dim" size="3" />
    <Array name="array">
      <DataType>Float64</DataType>
      <DimensionRef ref="dim" />
      <SRS dataAxisToSRSAxisMapping="2,1">ENGCRS["FOO",EDATUM["BAR"],CS[vertical,1],AXIS["foo",up,LENGTHUNIT["m",1]]]</SRS>
      <Unit>foo</Unit>
      <NoDataValue>1.5</NoDataValue>
      <Offset>2.5</Offset>
      <Scale>3.5</Scale>
      <RegularlySpacedValues start="1.5" increment="1" />
      <Attribute name="foo">
        <DataType>String</DataType>
        <Value>bar</Value>
      </Attribute>
    </Array>
  </Group>
</VRTDataset>
"""

    _validate(got_data)

    gdal.Unlink(tmpfile)


def test_vrtmultidim_createmultidimensional():

    tmpfile = "/vsimem/test.vrt"
    ds = gdal.GetDriverByName("VRT").CreateMultiDimensional(tmpfile)
    rg = ds.GetRootGroup()

    ds_other = gdal.GetDriverByName("VRT").CreateMultiDimensional("")
    dim_other = ds_other.GetRootGroup().CreateDimension("dim", "", "", 4)

    dim = rg.CreateDimension("dim", "", "", 3)
    assert dim
    with gdal.quiet_errors():
        assert not rg.CreateDimension("", "", "", 1)
        assert not rg.CreateDimension("dim", "", "", 1)

    assert rg.CreateAttribute("attr", [1], gdal.ExtendedDataType.CreateString())
    with gdal.quiet_errors():
        assert not rg.CreateAttribute("", [1], gdal.ExtendedDataType.CreateString())
        assert not rg.CreateAttribute(
            "attr_2dim", [1, 2], gdal.ExtendedDataType.CreateString()
        )
        assert not rg.CreateAttribute("attr", [1], gdal.ExtendedDataType.CreateString())
        assert not rg.CreateAttribute(
            "attr_too_big", [4000 * 1000 * 1000], gdal.ExtendedDataType.CreateString()
        )

    ar = rg.CreateMDArray(
        "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float32), ["BLOCKSIZE=2"]
    )
    assert ar[0]
    with gdal.quiet_errors():
        assert not rg.CreateMDArray(
            "", [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float32)
        )
        assert not rg.CreateMDArray(
            "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float32)
        )
        assert not rg.CreateMDArray(
            "ar2", [dim_other], gdal.ExtendedDataType.Create(gdal.GDT_Float32)
        )

    assert ar.CreateAttribute("attr", [1], gdal.ExtendedDataType.CreateString())
    with gdal.quiet_errors():
        assert not ar.CreateAttribute("", [1], gdal.ExtendedDataType.CreateString())
        assert not ar.CreateAttribute("attr", [1], gdal.ExtendedDataType.CreateString())

    subg = rg.CreateGroup("subgroup")
    assert subg
    with gdal.quiet_errors():
        assert not rg.CreateGroup("subgroup")
        assert not rg.CreateGroup("")

    ds.FlushCache()

    f = gdal.VSIFOpenL(tmpfile, "rb")
    got_data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)
    # print(got_data)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="dim" size="3" />
    <Attribute name="attr">
      <DataType>String</DataType>
    </Attribute>
    <Array name="ar">
      <DataType>Float32</DataType>
      <DimensionRef ref="dim" />
      <BlockSize>2</BlockSize>
      <Attribute name="attr">
        <DataType>String</DataType>
      </Attribute>
    </Array>
    <Group name="subgroup" />
  </Group>
</VRTDataset>
"""
    _validate(got_data)

    with gdal.OpenEx(tmpfile, gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        assert ar.GetBlockSize() == [2]

    gdal.Unlink(tmpfile)


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_singlesourcearray():

    ds = gdal.Open("data/vrt/arraysource_singlesourcearray.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4855


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_statistics_and_serialization(tmp_vsimem):

    netcdf_tmp_file = str(tmp_vsimem / "tmp.nc")
    gdal.FileFromMemBuffer(netcdf_tmp_file, open("data/netcdf/byte.nc", "rb").read())
    tmp_file = str(tmp_vsimem / "tmp.vrt")
    data = open("data/vrt/arraysource_singlesourcearray.vrt", "r").read()
    data = data.replace(
        """<SourceFilename relativeToVRT="1">../netcdf/byte_no_cf.nc</SourceFilename>""",
        f"""<SourceFilename>{netcdf_tmp_file}</SourceFilename>""",
    )
    gdal.FileFromMemBuffer(tmp_file, data)
    ds = gdal.Open(tmp_file, gdal.GA_Update)
    assert ds.GetRasterBand(1).GetMinimum() is None
    assert ds.GetRasterBand(1).GetMaximum() is None
    ds.GetRasterBand(1).ComputeStatistics(0)
    src_ds = gdal.Open(netcdf_tmp_file)
    assert (
        ds.GetRasterBand(1).GetDefaultHistogram()
        == src_ds.GetRasterBand(1).GetDefaultHistogram()
    )
    ds = None
    ds = gdal.Open(tmp_file)
    assert ds.GetRasterBand(1).Checksum() == 4855
    assert ds.GetRasterBand(1).GetMinimum() == 74
    assert ds.GetRasterBand(1).GetMaximum() == 255


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_no_step():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_no_step.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4855


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_array():

    ds = gdal.Open("data/vrt/arraysource_array.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4855


def test_vrtmultidim_arraysource_array_constant():

    ds = gdal.Open("data/vrt/arraysource_array_constant.vrt")
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (10, 10)


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_view():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_view.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_srcrect_dstrect():

    ds = gdal.Open("data/vrt/arraysource_srcrect_dstrect.vrt")
    assert ds.GetRasterBand(1).Checksum() == 1136


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_transpose():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_transpose.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4567


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_resample():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_resample.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_resample_options():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_resample_options.vrt")
    assert ds.GetRasterBand(1).Checksum() == 19827


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_grid():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_grid.vrt")
    assert ds.GetRasterBand(1).Checksum() == 21


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_getunscaled():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_getunscaled.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4855


@pytest.mark.require_driver("netCDF")
def test_vrtmultidim_arraysource_derivedarray_getmask():

    ds = gdal.Open("data/vrt/arraysource_derivedarray_getmask.vrt")
    assert ds.GetRasterBand(1).Checksum() == 400


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_no_array_in_array_source():

    with pytest.raises(
        Exception,
        match="Cannot find a <SimpleSourceArray>, <Array> or <DerivedArray> in <ArraySource>",
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_no_SourceFilename():

    with pytest.raises(
        Exception, match="Cannot find <SourceFilename> in <SingleSourceArray>"
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
          <SingleSourceArray>
            <!--<SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>-->
            <SourceArray>/Band1</SourceArray>
          </SingleSourceArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_no_SourceArray():

    with pytest.raises(
        Exception, match="Cannot find <SourceArray> in <SingleSourceArray>"
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
          <SingleSourceArray>
            <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
            <!--<SourceArray>/Band1</SourceArray>-->
          </SingleSourceArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_wrong_SourceFilename():

    with pytest.raises(Exception, match="i/do/not/exist.nc"):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
          <SingleSourceArray>
            <SourceFilename>i/do/not/exist.nc</SourceFilename>
            <SourceArray>/Band1</SourceArray>
          </SingleSourceArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_wrong_SourceArray():

    with pytest.raises(Exception, match="Cannot find array"):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
          <SingleSourceArray>
            <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
            <SourceArray>/i/do/not/exist</SourceArray>
          </SingleSourceArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_not_a_2D_array():

    with pytest.raises(
        Exception,
        match="Array referenced in <ArraySource> should be a two-dimensional array",
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
          <SingleSourceArray>
            <SourceFilename>data/netcdf/byte.nc</SourceFilename>
            <SourceArray>/x</SourceArray>
          </SingleSourceArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_no_source_array_in_DerivedArray():

    with pytest.raises(
        Exception,
        match="Cannot find a <SimpleSourceArray>, <Array> or <DerivedArray> in <DerivedArray>",
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray/>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_unknown_step():

    with pytest.raises(Exception, match="Unknown <Step>.<wrong> element"):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step><wrong/></Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_view_missing_expr():

    with pytest.raises(
        Exception, match="Cannot find 'expr' attribute in <View> element"
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step><View/></Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_transpose_missing_order():

    with pytest.raises(
        Exception, match="Cannot find 'newOrder' attribute in <Transpose> element"
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step><Transpose/></Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_resample_wrong_dimension():

    with pytest.raises(Exception, match="Missing name attribute on Dimension"):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <Resample>
                      <Dimension/>
                  </Resample>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_resample_wrong_srs():

    with pytest.raises(Exception, match="Invalid value for <SRS>"):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <Resample>
                      <SRS>invalid</SRS>
                  </Resample>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_error_resample_wrong_option():

    with pytest.raises(
        Exception, match="Cannot find 'name' attribute in <Option> element"
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <Resample>
                      <Option/>
                  </Resample>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_grid_missing_gridoptions():

    with pytest.raises(Exception, match="Cannot find <GridOptions> in <Grid> element"):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <Grid>
                  </Grid>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_grid_invalid_XArray():

    with pytest.raises(
        Exception,
        match="Cannot find a <SimpleSourceArray>, <Array> or <DerivedArray> in <XArray>",
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <Grid>
                      <GridOptions>invdist</GridOptions>
                      <XArray/>
                  </Grid>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_grid_invalid_YArray():

    with pytest.raises(
        Exception,
        match="Cannot find a <SimpleSourceArray>, <Array> or <DerivedArray> in <YArray>",
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <Grid>
                      <GridOptions>invdist</GridOptions>
                      <YArray/>
                  </Grid>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_grid_error_wrong_option():

    with pytest.raises(
        Exception, match="Cannot find 'name' attribute in <Option> element"
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <Grid>
                      <GridOptions>invdist</GridOptions>
                      <Option/>
                  </Grid>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_arraysource_getmask_error_wrong_option():

    with pytest.raises(
        Exception, match="Cannot find 'name' attribute in <Option> element"
    ):
        gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Gray</ColorInterp>
        <ArraySource>
            <DerivedArray>
              <SingleSourceArray>
                <SourceFilename>data/netcdf/byte_no_cf.nc</SourceFilename>
                <SourceArray>/Band1</SourceArray>
              </SingleSourceArray>
              <Step>
                  <GetMask>
                      <Option/>
                  </GetMask>
              </Step>
            </DerivedArray>
        </ArraySource>
      </VRTRasterBand>
    </VRTDataset>""")


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "source_slab,dest_slab,view_expr,expected",
    [
        ("", "", "[::1,:]", [0, 1, 2, 3, 4, 5]),
        ("", "", "[::-1,:]", [4, 5, 2, 3, 0, 1]),
        ('<SourceSlab offset="1,0" />', "", "[:,:]", [2, 3, 4, 5, 0, 0]),
        ('<SourceSlab offset="1,0" />', "", "[::-1,:]", [4, 5, 2, 3, 0, 0]),
        ("", '<DestSlab offset="1,0" />', "[:,:]", [0, 0, 0, 1, 2, 3]),
        ("", '<DestSlab offset="1,0" />', "[::-1,:]", [2, 3, 0, 1, 0, 0]),
        (
            '<SourceSlab offset="1,0" />',
            '<DestSlab offset="1,0" />',
            "[:,:]",
            [0, 0, 2, 3, 4, 5],
        ),
        (
            '<SourceSlab offset="1,0" />',
            '<DestSlab offset="1,0" />',
            "[::-1,:]",
            [4, 5, 2, 3, 0, 0],
        ),
    ],
)
def test_vrtmultidim_arraysource_view(
    tmp_vsimem, source_slab, dest_slab, view_expr, expected
):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", 2, 3) as ds:
        ds.GetRasterBand(1).WriteRaster(
            0, 0, 2, 3, array.array("B", [0, 1, 2, 3, 4, 5])
        )

    ds = gdal.Open(f"""<VRTDataset rasterXSize="2" rasterYSize="3">
  <VRTRasterBand dataType="Byte" band="1">
    <ArraySource>
      <DerivedArray>
        <Array name="data">
          <DataType>Byte</DataType>
          <Dimension name="y" size="3"/>
          <Dimension name="x" size="2"/>
          <Source>
            <SourceFilename relativeToVRT="0">{tmp_vsimem}/src.tif</SourceFilename>
            <SourceBand>1</SourceBand>
            {source_slab}
            {dest_slab}
          </Source>
        </Array>
        <Step>
          <View expr="{view_expr}"/>
        </Step>
      </DerivedArray>
    </ArraySource>
  </VRTRasterBand>
</VRTDataset>""")

    assert array.array("B", ds.GetRasterBand(1).ReadRaster()) == array.array(
        "B", expected
    )


@pytest.mark.require_driver("HDF5")
@gdaltest.enable_exceptions()
def test_vrtmultidim_GetRawBlockInfo_single_source(tmp_vsimem):

    if not gdal.GetDriverByName("HDF5").GetMetadataItem("HAVE_H5Dget_chunk_info"):
        pytest.skip("libhdf5 < 1.10.5")

    gdal.Run(
        "mdim convert", input="data/hdf5/deflate.h5", output=tmp_vsimem / "out.vrt"
    )
    with gdal.OpenEx(tmp_vsimem / "out.vrt", gdal.OF_MULTIDIM_RASTER) as ds:
        array = ds.GetRootGroup().OpenMDArrayFromFullname("/Band1")

        info = array.GetRawBlockInfo([0, 0])
        assert info.GetFilename() == "data/hdf5/deflate.h5"
        assert info.GetOffset() == 13908
        assert info.GetSize() == 10
        assert info.GetInfo() == ["COMPRESSION=DEFLATE", "FILTER=SHUFFLE"]
        assert info.GetInlineData() is None

        info = array.GetRawBlockInfo([19, 9])
        assert info.GetFilename() == "data/hdf5/deflate.h5"
        assert info.GetOffset() == 15898
        assert info.GetSize() == 10
        assert info.GetInfo() == ["COMPRESSION=DEFLATE", "FILTER=SHUFFLE"]
        assert info.GetInlineData() is None

        with pytest.raises(Exception, match="invalid block coordinate"):
            array.GetRawBlockInfo([20, 0])


@pytest.mark.require_driver("netCDF")
@gdaltest.enable_exceptions()
def test_vrtmultidim_GetRawBlockInfo_unblocked(tmp_vsimem):

    gdal.Run("mdim convert", input="data/netcdf/byte.nc", output=tmp_vsimem / "out.vrt")
    with gdal.OpenEx(tmp_vsimem / "out.vrt", gdal.OF_MULTIDIM_RASTER) as ds:
        array = ds.GetRootGroup().OpenMDArrayFromFullname("/Band1")

        with pytest.raises(Exception, match="block size for dimension 0 is unknown"):
            array.GetRawBlockInfo([0, 0])


@pytest.mark.require_driver("netCDF")
@pytest.mark.require_driver("HDF5")
@gdaltest.enable_exceptions()
def test_vrtmultidim_GetRawBlockInfo_two_sources(tmp_path):

    if not gdal.GetDriverByName("HDF5").GetMetadataItem("HAVE_H5Dget_chunk_info"):
        pytest.skip("libhdf5 < 1.10.5")

    gdal.Run(
        "raster clip",
        input="data/byte.tif",
        output=tmp_path / "out_top.nc",
        window=[0, 0, 20, 10],
        creation_option={"FORMAT": "NC4", "COMPRESS": "DEFLATE"},
    )
    gdal.Run(
        "raster clip",
        input="data/byte.tif",
        output=tmp_path / "out_bottom.nc",
        window=[0, 10, 20, 10],
        creation_option={"FORMAT": "NC4", "COMPRESS": "DEFLATE"},
    )
    gdal.Run(
        "mdim mosaic",
        input=[tmp_path / "out_top.nc", tmp_path / "out_bottom.nc"],
        output=tmp_path / "out.vrt",
    )

    with gdal.OpenEx(tmp_path / "out.vrt", gdal.OF_MULTIDIM_RASTER) as ds:
        array = ds.GetRootGroup().OpenMDArrayFromFullname("/Band1")

        for y in range(10):
            info = array.GetRawBlockInfo([y, 0])
            assert "out_bottom.nc" in info.GetFilename()

        for y in range(10, 20):
            info = array.GetRawBlockInfo([y, 0])
            assert "out_top.nc" in info.GetFilename()


@gdaltest.enable_exceptions()
def test_vrtmultidim_overview_by_ref():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Array name="ar">
            <DataType>Float32</DataType>
            <Overviews>
                <ArrayFullName>/ar2</ArrayFullName>
            </Overviews>
        </Array>
        <Array name="ar2">
            <DataType>Float32</DataType>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("ar")
    assert ar.GetOverviewCount() == 1
    assert ar.GetOverview(-1) is None
    assert ar.GetOverview(1) is None
    assert ar.GetOverview(0).GetFullName() == "/ar2"


@gdaltest.enable_exceptions()
def test_vrtmultidim_overview_by_ref_wrong():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Array name="ar">
            <DataType>Float32</DataType>
            <Overviews>
                <ArrayFullName>/wrong</ArrayFullName>
            </Overviews>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("ar")
    with pytest.raises(
        Exception, match="Cannot resolve overview full name '/wrong' to an actual array"
    ):
        ar.GetOverview(0)


@gdaltest.enable_exceptions()
def test_vrtmultidim_overview_inline():

    ds = gdal.OpenEx(
        """<VRTDataset>
    <Group name="/">
        <Array name="ar">
            <Dimension name="Y" size="40"/>
            <Dimension name="X" size="20"/>
            <DataType>Float32</DataType>
            <Overviews>
                <Array name="ar2">
                    <Dimension name="Y_reduced" size="20"/>
                    <Dimension name="X_reduced" size="10"/>
                    <DataType>Float32</DataType>
                </Array>
            </Overviews>
        </Array>
    </Group>
</VRTDataset>""",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("ar")
    assert ar.GetOverviewCount() == 1
    assert ar.GetOverview(0).GetName() == "ar2"

    classic_ds = ar.AsClassicDataset(1, 0)
    assert classic_ds.RasterYSize == 40
    assert classic_ds.RasterXSize == 20
    band = classic_ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1
    assert band.GetOverview(-1) is None
    assert band.GetOverview(1) is None
    assert band.GetOverview(0).YSize == 20
    assert band.GetOverview(0).XSize == 10

    ar_from_classic_ds = classic_ds.AsMDArray()
    assert ar_from_classic_ds.GetOverviewCount() == 1
    assert ar_from_classic_ds.GetOverview(-1) is None
    assert ar_from_classic_ds.GetOverview(1) is None
    assert ar_from_classic_ds.GetOverview(0).GetDimensions()[0].GetSize() == 1
    assert ar_from_classic_ds.GetOverview(0).GetDimensions()[1].GetSize() == 20
    assert ar_from_classic_ds.GetOverview(0).GetDimensions()[2].GetSize() == 10

    ar_from_band = band.AsMDArray()
    assert ar_from_band.GetOverviewCount() == 1
    assert ar_from_band.GetOverview(-1) is None
    assert ar_from_band.GetOverview(1) is None
    assert ar_from_band.GetOverview(0).GetDimensions()[0].GetSize() == 20
    assert ar_from_band.GetOverview(0).GetDimensions()[1].GetSize() == 10


@gdaltest.enable_exceptions()
def test_vrtmultidim_overview_inline_wrong():

    with pytest.raises(Exception, match="Missing name attribute on Array"):
        gdal.OpenEx(
            """<VRTDataset>
        <Group name="/">
            <Array name="ar">
                <DataType>Float32</DataType>
                <Overviews>
                    <Array/>
                </Overviews>
            </Array>
        </Group>
    </VRTDataset>""",
            gdal.OF_MULTIDIM_RASTER,
        )
