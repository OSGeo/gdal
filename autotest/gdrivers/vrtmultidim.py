#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in VRT driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault@spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

from osgeo import gdal
from osgeo import osr
import gdaltest
import math
import struct


def test_vrtmultidim_dimension():

    ds = gdal.OpenEx("""<VRTDataset>
    <Group name="/">
        <Dimension name="X" size="2" type="foo" direction="bar" indexingVariable="X"/>
        <Dimension name="Y" size="1234567890123"/>
    </Group>
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert rg.GetName() == '/'
    dims = rg.GetDimensions()
    assert len(dims) == 2
    dim_0 = dims[0]
    assert dim_0.GetName() == 'X'
    assert dim_0.GetSize() == 2
    assert dim_0.GetType() == 'foo'
    assert dim_0.GetDirection() == 'bar'
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert not dim_0.GetIndexingVariable()
        assert gdal.GetLastErrorMsg() == 'Cannot find variable X'
    dim_1 = dims[1]
    assert dim_1.GetName() == 'Y'
    assert dim_1.GetSize() == 1234567890123

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group MISSING_name="/">
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="INVALID">
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Dimension MISSING_name="X" size="1"/>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Dimension name="X" MISSING_size="1"/>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds


def test_vrtmultidim_attribute():

    ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    attrs = rg.GetAttributes()
    assert len(attrs) == 3

    foo = next((x for x in attrs if x.GetName() == 'foo'), None)
    assert foo
    assert foo.GetDataType().GetClass() == gdal.GEDTC_STRING
    assert foo.Read() == ['bar', 'baz']

    bar = next((x for x in attrs if x.GetName() == 'bar'), None)
    assert bar
    assert bar.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert bar.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert bar.Read() == 1.25125

    empty = next((x for x in attrs if x.GetName() == 'empty'), None)
    assert empty
    assert empty.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert empty.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert empty.Read() == 0.0

    ar = rg.OpenMDArray('ar')
    assert ar
    attrs = ar.GetAttributes()
    assert len(attrs) == 1


    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Attribute MISSING_name="foo">
                <DataType>String</DataType>
                <Value>bar</Value>
        </Attribute>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Attribute name="foo">
                <MISSING_DataType>String</MISSING_DataType>
                <Value>bar</Value>
        </Attribute>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Attribute name="foo">
                <DataType>INVALID</DataType>
                <Value>bar</Value>
        </Attribute>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds


def test_vrtmultidim_subgroup_and_cross_references():

    ds = gdal.OpenEx("""<VRTDataset>
    <Group name="/">
        <Dimension name="X" size="20" indexingVariable="X"/>
        <Dimension name="Y" size="30" indexingVariable="/Y"/>
        <Array name="X">
            <DataType>Float32</DataType>
            <DimensionRef ref="/X"/>
        </Array>
        <Array name="Y">
            <DataType>Float32</DataType>
            <DimensionRef ref="Y"/>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == [ 'subgroup' ]
    subgroup = rg.OpenGroup('subgroup')
    assert subgroup
    dims = subgroup.GetDimensions()
    assert len(dims) == 2

    dim_0 = dims[0]
    assert dim_0.GetName() == 'X'
    assert dim_0.GetSize() == 2
    indexing_var = dim_0.GetIndexingVariable()
    assert indexing_var
    assert indexing_var.GetName() == 'X'
    assert indexing_var.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert indexing_var.GetDimensionCount() == 1
    assert indexing_var.GetDimensions()[0].GetSize() == 2

    dim_1 = dims[1]
    assert dim_1.GetName() == 'Y'
    assert dim_1.GetSize() == 3
    indexing_var = dim_1.GetIndexingVariable()
    assert indexing_var
    assert indexing_var.GetName() == 'Y'
    assert indexing_var.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert indexing_var.GetDimensionCount() == 1
    assert indexing_var.GetDimensions()[0].GetSize() == 3

    assert rg.GetMDArrayNames() == ['X', 'Y']
    X = rg.OpenMDArray('X')
    assert X
    assert X.GetDataType().GetNumericDataType() == gdal.GDT_Float32
    assert X.GetDimensionCount() == 1
    assert X.GetDimensions()[0].GetSize() == 20
    Y = rg.OpenMDArray('Y')
    assert Y
    assert Y.GetDataType().GetNumericDataType() == gdal.GDT_Float32
    assert Y.GetDimensionCount() == 1
    assert Y.GetDimensions()[0].GetSize() == 30

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Group MISSING_name="subgroup"/>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Array MISSING_name="X">
                <DataType>Float64</DataType>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Array name="X">
                <MISSING_DataType>Float64</MISSING_DataType>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>invalid</DataType>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef MISSING_ref="X"/>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="INVALID"/>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="/INVALID"/>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="/INVALID_GROUP/INVALID"/>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds


def test_vrtmultidim_srs():

    ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()

    X = rg.OpenMDArray('X')
    srs = X.GetSpatialRef()
    assert srs

    Y = rg.OpenMDArray('Y')
    srs = Y.GetSpatialRef()
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


def test_vrtmultidim_nodata_unit_offset_scale():

    ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('ar1')
    assert ar.GetNoDataValueAsDouble() == 1.25125
    assert struct.unpack('d', ar.Read()) == (1.25125,)
    assert ar.GetUnit() == 'foo'
    assert ar.GetOffset() == 1.5
    assert ar.GetScale() == 2.5

    ar = rg.OpenMDArray('ar2')
    assert math.isnan(ar.GetNoDataValueAsDouble())
    assert math.isnan(struct.unpack('d', ar.Read())[0])
    assert ar.GetOffset() is None
    assert ar.GetScale() is None

    ar = rg.OpenMDArray('ar3')
    assert ar.GetNoDataValueAsDouble() is None
    assert struct.unpack('d', ar.Read()) == (0,)


def test_vrtmultidim_RegularlySpacedValues():

    ds = gdal.OpenEx("""<VRTDataset>
    <Group name="/">
        <Dimension name="X" size="4"/>
        <Array name="X">
            <DataType>Float64</DataType>
            <DimensionRef ref="X"/>
            <RegularlySpacedValues start="0.5" increment="10.5"/>
        </Array>
    </Group>
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    X = rg.OpenMDArray('X')
    assert struct.unpack('d' * 4, X.Read()) == (0.5, 11.0, 21.5, 32.0)
    assert struct.unpack('d' * 2, X.Read(array_start_idx = [1], count = [2], array_step = [2] )) == (11.0, 32.0)

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Dimension name="X" size="4"/>
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="X"/>
                <RegularlySpacedValues MISSING_start="0.5" increment="10.5"/>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
        <Group name="/">
            <Dimension name="X" size="4"/>
            <Array name="X">
                <DataType>Float64</DataType>
                <DimensionRef ref="X"/>
                <RegularlySpacedValues start="0.5" MISSING_increment="10.5"/>
            </Array>
        </Group>
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds


def test_vrtmultidim_ConstantValue():

    ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray('ar')
    got = struct.unpack('d' * 12, ar.Read())
    assert got == (5, 5, 5,
                   5, 10, 100,
                   5, 10, 100,
                   5, 10, 10)
    assert struct.unpack('d' * 4, ar.Read(array_start_idx = [2,1], count = [2,2] )) == (10,100,10,10)

    ar = rg.OpenMDArray('no_dim')
    assert struct.unpack('d', ar.Read()) == (50,)

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
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
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
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
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
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
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
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
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
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
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds


def test_vrtmultidim_InlineValues():

    ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('ar')
    got = struct.unpack('d' * 12, ar.Read())
    assert got == (0.0, 1.0, 2.0,
                   3.0, -4.0, 100.0,
                   6.0, -7.0, 101.0,
                   9.0, -10.0, -11.0)
    assert struct.unpack('d' * 4, ar.Read(array_start_idx = [2,1], count = [2,2] )) == (-7, 101, -10, -11)
    assert struct.unpack('d' * 4, ar.Read(array_start_idx = [2,1], count = [2,2], array_step=[-1,-1] )) == (-7, 6, -4, 3)

    ar = rg.OpenMDArray('no_dim')
    assert struct.unpack('d', ar.Read()) == (50,)

    with gdaltest.error_handler():
        ds = gdal.OpenEx("""<VRTDataset>
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
    </VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
        assert not ds


def test_vrtmultidim_Source():

  def f():

    gdal.FileFromMemBuffer('/vsimem/src.vrt',"""<VRTDataset>
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
</VRTDataset>""")

    ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('ar')
    assert ar
    got = struct.unpack('d' * 12, ar.Read())
    assert got == (0, 1, 2,
                   3, 4, 5,
                   6, 7, 8,
                   9, 10, 11)

    ar_with_offset = rg.OpenMDArray('ar_with_offset')
    assert ar_with_offset
    got = struct.unpack('d' * 12, ar_with_offset.Read())
    assert got == (0, 0, 0,
                   0, 0, 0,
                   0, 4, 5,
                   0, 10, 11)
    assert struct.unpack('d' * 2, ar_with_offset.Read(array_start_idx = [2, 0], count = [1, 2])) == (0, 4)
    assert struct.unpack('d' * 2, ar_with_offset.Read(array_start_idx = [3, 1], count = [1, 2])) == (10, 11)
    assert struct.unpack('d' * 4, ar_with_offset.Read(array_start_idx = [2, 1], count = [2, 2], array_step=[-1,-1] )) == (4, 0, 0, 0)

    ar_transposed = rg.OpenMDArray('ar_transposed')
    assert ar_transposed
    got = struct.unpack('d' * 12, ar_transposed.Read())
    assert got == (0, 3, 6, 9,
                   1, 4, 7, 10,
                   2, 5, 8, 11)

    ar_view = rg.OpenMDArray('ar_view')
    assert ar_view
    got = struct.unpack('d' * 12, ar_view.Read())
    assert got == (9, 10, 11,
                   6, 7, 8,
                   3, 4, 5,
                   0, 1, 2)

    # Source does not exist, but we don't request an area where it is active
    # so we should not try to open it
    ar = rg.OpenMDArray('ar_non_existing_source_with_offset')
    assert ar
    assert struct.unpack('d' * 2, ar.Read(array_start_idx = [0, 0], count = [1, 2])) == (0, 0)

    ar = rg.OpenMDArray('ar_non_existing_source')
    assert ar
    with gdaltest.error_handler():
        assert not ar.Read()

    ar = rg.OpenMDArray('ar_invalid_source_slab_offset')
    assert ar
    with gdaltest.error_handler():
        assert not ar.Read()

    ar = rg.OpenMDArray('ar_invalid_number_of_dimensions')
    assert ar
    with gdaltest.error_handler():
        assert not ar.Read()

    ar = rg.OpenMDArray('ar_non_existing_array_source')
    assert ar
    with gdaltest.error_handler():
        assert not ar.Read()

    ar = rg.OpenMDArray('ar_view_invalid')
    assert ar
    with gdaltest.error_handler():
        assert not ar.Read()

    ar = rg.OpenMDArray('ar_transposed_invalid')
    assert ar
    with gdaltest.error_handler():
        assert not ar.Read()

    gdal.Unlink('/vsimem/src.vrt')

    # Check that the cache is correctly working by opening a second
    # dataset after having remove the source
    ds2 = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds2
    rg2 = ds2.GetRootGroup()
    ar2 = rg2.OpenMDArray('ar')
    got = struct.unpack('d' * 12, ar2.Read())
    assert got == (0, 1, 2,
                   3, 4, 5,
                   6, 7, 8,
                   9, 10, 11)

  f()

  # Check that the cache is correctly working: we should get an error
  # now that all referencing arrays have been cleaned up
  ds2 = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
  assert ds2
  rg2 = ds2.GetRootGroup()
  ar2 = rg2.OpenMDArray('ar')
  with gdaltest.error_handler():
      assert not ar2.Read()


def test_vrtmultidim_Source_classic_dataset():

    ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('ar')
    assert ar
    got = struct.unpack('d' * 12, ar.Read())
    assert got == (0.0, 0.0, 0.0,
                   0.0, 0.0, 0.0,
                   0.0, 132.0, 140.0,
                   0.0, 156.0, 132.0)

    ar_wrong_band = rg.OpenMDArray('ar_wrong_band')
    assert ar_wrong_band
    with gdaltest.error_handler():
        assert not ar_wrong_band.Read()


def _validate(content):

    try:
        from lxml import etree
    except ImportError:
        return

    doc = etree.XML(content)
    try:
        schema_content = open('../../gdal/data/gdalvrt.xsd', 'rb').read()
    except IOError:
        print('Cannot read gdalvrt.xsd schema')
        return
    schema = etree.XMLSchema(etree.XML(schema_content))
    schema.assertValid(doc)


def test_vrtmultidim_serialize():

    tmpfile = '/vsimem/test.vrt'
    gdal.FileFromMemBuffer(tmpfile, """<VRTDataset>
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
</VRTDataset>""")
    ds = gdal.OpenEx(tmpfile, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
    rg = ds.GetRootGroup()
    ds = None
    attr = rg.CreateAttribute('foo', [], gdal.ExtendedDataType.CreateString())
    attr.Write('bar')
    attr = None
    rg = None

    f = gdal.VSIFOpenL(tmpfile, 'rb')
    got_data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    #print(got_data)

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

    src_ds = gdal.GetDriverByName('MEM').CreateMultiDimensional('myds')
    src_rg = src_ds.GetRootGroup()
    src_dim = src_rg.CreateDimension('dim', '', '', 3)
    src_ar = src_rg.CreateMDArray('array', [src_dim], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    src_ar.Write(struct.pack('d' * 3, 1.5, 2.5, 3.5))
    src_ar.SetNoDataValueDouble(1.5)
    src_ar.SetUnit('foo')
    src_ar.SetOffset(2.5)
    src_ar.SetScale(3.5)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('ENGCRS["FOO",EDATUM["BAR"],CS[vertical,1],AXIS["foo",up,LENGTHUNIT["m",1]]]')
    sr.SetDataAxisToSRSAxisMapping([2, 1])
    src_ar.SetSpatialRef(sr)
    attr = src_ar.CreateAttribute('foo', [], gdal.ExtendedDataType.CreateString())
    attr.Write('bar')
    attr = None

    with gdaltest.error_handler():
        gdal.GetDriverByName('VRT').CreateCopy('/i_do/not_exist', src_ds)

    tmpfile = '/vsimem/test.vrt'
    assert gdal.GetDriverByName('VRT').CreateCopy(tmpfile, src_ds)

    f = gdal.VSIFOpenL(tmpfile, 'rb')
    got_data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    #print(got_data)

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

    tmpfile = '/vsimem/test.vrt'
    ds = gdal.GetDriverByName('VRT').CreateMultiDimensional(tmpfile)
    rg = ds.GetRootGroup()

    ds_other = gdal.GetDriverByName('VRT').CreateMultiDimensional('')
    dim_other = ds_other.GetRootGroup().CreateDimension('dim', '', '', 4)

    dim = rg.CreateDimension('dim', '', '', 3)
    assert dim
    with gdaltest.error_handler():
        assert not rg.CreateDimension('', '', '', 1)
        assert not rg.CreateDimension('dim', '', '', 1)

    assert rg.CreateAttribute('attr', [1], gdal.ExtendedDataType.CreateString())
    with gdaltest.error_handler():
        assert not rg.CreateAttribute('', [1], gdal.ExtendedDataType.CreateString())
        assert not rg.CreateAttribute('attr_2dim', [1,2], gdal.ExtendedDataType.CreateString())
        assert not rg.CreateAttribute('attr', [1], gdal.ExtendedDataType.CreateString())
        assert not rg.CreateAttribute('attr_too_big', [4000 * 1000 * 1000], gdal.ExtendedDataType.CreateString())

    ar = rg.CreateMDArray('ar', [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float32))
    assert ar[0]
    with gdaltest.error_handler():
        assert not rg.CreateMDArray('', [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float32))
        assert not rg.CreateMDArray('ar', [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float32))
        assert not rg.CreateMDArray('ar2', [dim_other], gdal.ExtendedDataType.Create(gdal.GDT_Float32))

    assert ar.CreateAttribute('attr', [1], gdal.ExtendedDataType.CreateString())
    with gdaltest.error_handler():
        assert not ar.CreateAttribute('', [1], gdal.ExtendedDataType.CreateString())
        assert not ar.CreateAttribute('attr', [1], gdal.ExtendedDataType.CreateString())

    subg = rg.CreateGroup('subgroup')
    assert subg
    with gdaltest.error_handler():
        assert not rg.CreateGroup('subgroup')
        assert not rg.CreateGroup('')

    ds.FlushCache()

    f = gdal.VSIFOpenL(tmpfile, 'rb')
    got_data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    #print(got_data)

    assert got_data == """<VRTDataset>
  <Group name="/">
    <Dimension name="dim" size="3" />
    <Attribute name="attr">
      <DataType>String</DataType>
    </Attribute>
    <Array name="ar">
      <DataType>Float32</DataType>
      <DimensionRef ref="dim" />
      <Attribute name="attr">
        <DataType>String</DataType>
      </Attribute>
    </Array>
    <Group name="subgroup" />
  </Group>
</VRTDataset>
"""
    _validate(got_data)

    gdal.Unlink(tmpfile)
