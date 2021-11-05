#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in netCDF driver
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
import os
import pytest
import shutil
import stat
import struct
import sys

def has_nc4():
    netcdf_drv = gdal.GetDriverByName('NETCDF')
    if netcdf_drv is None:
        return False
    metadata = netcdf_drv.GetMetadata()
    return 'NETCDF_HAS_NC4' in metadata and metadata['NETCDF_HAS_NC4'] == 'YES'


pytestmark = [ pytest.mark.require_driver('netCDF'),
               pytest.mark.skipif(not has_nc4(), reason='netCDF 4 support missing') ]


def test_netcdf_multidim_invalid_file():

    ds = gdal.OpenEx('data/netcdf/byte_truncated.nc', gdal.OF_MULTIDIM_RASTER)
    assert not ds


def test_netcdf_multidim_single_group():

    ds = gdal.OpenEx('data/netcdf/byte_no_cf.nc', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert rg.GetName() == '/'
    assert rg.GetFullName() == '/'
    assert rg.GetGroupNames() is None
    dims = rg.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == 'x'
    assert dims[0].GetFullName() == '/x'
    assert dims[1].GetName() == 'y'
    assert rg.OpenGroup('foo') is None
    assert not rg.GetAttribute("not existing")
    assert rg.GetStructuralInfo() == {'NC_FORMAT': 'CLASSIC'}
    assert rg.GetMDArrayNames() == [ 'Band1' ]
    var = rg.OpenMDArray('Band1')
    assert var
    assert var.GetName() == 'Band1'
    assert not rg.OpenMDArray('foo')
    assert not var.GetAttribute("not existing")
    assert var.GetDimensionCount() == 2
    dims = var.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == 'y'
    assert dims[0].GetSize() == 20
    assert not dims[0].GetIndexingVariable()
    assert dims[1].GetName() == 'x'
    assert dims[1].GetSize() == 20
    assert not dims[1].GetIndexingVariable()
    assert var.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert var.GetDataType().GetNumericDataType() == gdal.GDT_Byte
    assert var.GetAttribute('long_name')
    assert len(var.GetNoDataValueAsRaw()) == 1
    assert var.GetNoDataValueAsDouble() == 0.0
    assert var.GetScale() is None
    assert var.GetOffset() is None
    assert var.GetBlockSize() == [0, 0]
    assert var.GetProcessingChunkSize(0) == [1, 1]

    ref_ds = gdal.Open('data/netcdf/byte_no_cf.nc')
    ref_data = struct.unpack('B' * 400, ref_ds.ReadRaster())
    got_data = struct.unpack('B' * 400, var.Read())
    assert got_data == ref_data

    with gdaltest.error_handler(): # Write to read only
        assert not rg.CreateDimension('X', None, None, 2)
        assert not rg.CreateAttribute('att_text', [], gdal.ExtendedDataType.CreateString())
        assert not var.CreateAttribute('att_text', [], gdal.ExtendedDataType.CreateString())
        assert not rg.CreateGroup('subgroup')
        assert not rg.CreateMDArray('my_var_no_dim', [],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert var.Write(var.Read()) != gdal.CE_None
        att = next((x for x in var.GetAttributes() if x.GetName() == 'long_name'), None)
        assert att.Write('foo') != gdal.CE_None


def test_netcdf_multidim_multi_group():

    ds = gdal.OpenEx('data/netcdf/complex.nc', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert rg.GetName() == '/'
    assert rg.GetGroupNames() == ['group']
    assert rg.GetMDArrayNames() == [ 'Y', 'X', 'Z', 'f32', 'f64' ]
    assert rg.GetAttribute('Conventions')
    assert rg.GetStructuralInfo() == {'NC_FORMAT': 'NETCDF4'}
    subgroup = rg.OpenGroup('group')
    assert subgroup
    assert subgroup.GetName() == 'group'
    assert subgroup.GetFullName() == '/group'
    assert rg.OpenGroup('foo') is None
    assert subgroup.GetGroupNames() is None
    assert subgroup.GetMDArrayNames() == [ 'fmul' ]
    assert subgroup.OpenGroup('foo') is None

    var = rg.OpenMDArray('f32')
    assert var
    assert var.GetName() == 'f32'
    dim0 = var.GetDimensions()[0]
    indexing_var = dim0.GetIndexingVariable()
    assert indexing_var
    assert indexing_var.GetName() == dim0.GetName()
    assert var.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert var.GetDataType().GetNumericDataType() == gdal.GDT_CFloat32
    assert var.GetNoDataValueAsRaw() is None

    var = rg.OpenMDArray('f64')
    assert var
    assert var.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert var.GetDataType().GetNumericDataType() == gdal.GDT_CFloat64

    assert not rg.OpenMDArray('foo')
    var = subgroup.OpenMDArray('fmul')
    assert var
    assert var.GetName() == 'fmul'
    assert var.GetFullName() == '/group/fmul'
    assert not subgroup.OpenMDArray('foo')
    assert var.GetDimensionCount() == 3
    dims = var.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetName() == 'Z'
    assert dims[0].GetSize() == 3
    indexing_var = dims[0].GetIndexingVariable()
    assert indexing_var
    assert indexing_var.GetName() == 'Z'
    assert dims[1].GetName() == 'Y'
    assert dims[1].GetSize() == 5
    assert dims[2].GetName() == 'X'
    assert dims[2].GetSize() == 5


def test_netcdf_multidim_from_ncdump():

    if gdaltest.netcdf_drv.GetMetadataItem("ENABLE_NCDUMP") != 'YES':
        pytest.skip()

    ds = gdal.OpenEx('data/netcdf/byte.nc.txt', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg


def test_netcdf_multidim_var_alldatatypes():

    ds = gdal.OpenEx('data/netcdf/alldatatypes.nc', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg

    expected_vars = [ ('char_var', gdal.GDT_Byte, (ord('x'),ord('y'))),
                      ('ubyte_var', gdal.GDT_Byte, (255, 254)),
                      ('byte_var', gdal.GDT_Int16, (-128,-127)),
                      ('byte_unsigned_false_var', gdal.GDT_Int16, (-128,-127)),
                      ('byte_unsigned_true_var', gdal.GDT_Byte, (128, 129)),
                      ('ushort_var', gdal.GDT_UInt16, (65534, 65533)),
                      ('short_var', gdal.GDT_Int16, (-32768, -32767)),
                      ('uint_var', gdal.GDT_UInt32, (4294967294, 4294967293)),
                      ('int_var', gdal.GDT_Int32, (-2147483648, -2147483647)),
                      ('uint64_var', gdal.GDT_Float64, (1.8446744073709552e+19, 1.8446744073709552e+19)),
                      ('int64_var', gdal.GDT_Float64, (-9.223372036854776e+18, -9.223372036854776e+18)),
                      ('float_var', gdal.GDT_Float32, (1.25, 2.25)),
                      ('double_var', gdal.GDT_Float64, (1.25125, 2.25125)),
                      ('complex_int16_var', gdal.GDT_CInt16, (-32768, -32767, -32766, -32765)),
                      ('complex_int32_var', gdal.GDT_CInt32, (-2147483648, -2147483647, -2147483646, -2147483645)),
                      ('complex64_var', gdal.GDT_CFloat32, (1.25, 2.5, 2.25, 3.5)),
                      ('complex128_var', gdal.GDT_CFloat64, (1.25125, 2.25125, 3.25125, 4.25125)),
                      ]
    for var_name, dt, val in expected_vars:
        var = rg.OpenMDArray(var_name)
        assert var
        assert var.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
        assert var.GetDataType().GetNumericDataType() == dt, var_name
        if dt == gdal.GDT_Byte:
            assert struct.unpack('B' * len(val), var.Read()) == val
        if dt == gdal.GDT_UInt16:
            assert struct.unpack('H' * len(val), var.Read()) == val
        if dt == gdal.GDT_Int16:
            assert struct.unpack('h' * len(val), var.Read()) == val, var_name
        if dt == gdal.GDT_UInt32:
            assert struct.unpack('I' * len(val), var.Read()) == val
        if dt == gdal.GDT_Int32:
            assert struct.unpack('i' * len(val), var.Read()) == val
        if dt == gdal.GDT_Float32:
            assert struct.unpack('f' * len(val), var.Read()) == val
        if dt == gdal.GDT_Float64:
            assert struct.unpack('d' * len(val), var.Read()) == val
        if dt == gdal.GDT_CInt16:
            assert struct.unpack('h' * len(val), var.Read()) == val
        if dt == gdal.GDT_CInt32:
            assert struct.unpack('i' * len(val), var.Read()) == val
        if dt == gdal.GDT_CFloat32:
            assert struct.unpack('f' * len(val), var.Read()) == val
        if dt == gdal.GDT_CFloat64:
            assert struct.unpack('d' * len(val), var.Read()) == val

    # Read byte_var (where nc native type != gdal data type) to other data types
    var = rg.OpenMDArray('byte_var')
    assert struct.unpack('B' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Byte))) == (0, 0)
    assert struct.unpack('i' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int32))) == (-128, -127)

    # Read int_var to other data types
    var = rg.OpenMDArray('int_var')
    assert struct.unpack('h' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int16))) == (-32768, -32768)
    assert struct.unpack('f' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float32))) == (-2147483648.0, -2147483648.0)
    assert struct.unpack('d' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float64))) == (-2147483648.0, -2147483647.0)

    # Read int64_var (where nc native type != gdal data type) to other data types
    var = rg.OpenMDArray('int64_var')
    assert struct.unpack('h' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int16))) == (-32768, -32768)
    assert struct.unpack('f' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float32))) == (-9.223372036854776e+18, -9.223372036854776e+18)
    assert struct.unpack('d' * 2, var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float64))) == (-9.223372036854776e+18, -9.223372036854776e+18)


    var = rg.OpenMDArray('custom_type_2_elts_var')
    dt = var.GetDataType()
    assert dt.GetClass() == gdal.GEDTC_COMPOUND
    assert dt.GetSize() == 8
    assert dt.GetName() == 'custom_type_2_elts'
    comps = dt.GetComponents()
    assert len(comps) == 2
    assert comps[0].GetName() == 'x'
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Int32
    assert comps[1].GetName() == 'y'
    assert comps[1].GetOffset() == 4
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_Int16
    data = var.Read()
    assert len(data) == 2 * 8
    assert struct.unpack('ihihh', data) == (1, 2, 3, 4, 0)

    var = rg.OpenMDArray('custom_type_3_elts_var')
    dt = var.GetDataType()
    assert dt.GetClass() == gdal.GEDTC_COMPOUND
    assert dt.GetSize() == 12
    comps = dt.GetComponents()
    assert len(comps) == 3
    assert comps[0].GetName() == 'x'
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Int32
    assert comps[1].GetName() == 'y'
    assert comps[1].GetOffset() == 4
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_Int16
    assert comps[2].GetName() == 'z'
    assert comps[2].GetOffset() == 8
    assert comps[2].GetType().GetNumericDataType() == gdal.GDT_Float32
    data = var.Read()
    assert len(data) == 2 * 12
    assert struct.unpack('ihf' * 2, data) == (1, 2, 3.5, 4, 5, 6.5)

    var = rg.OpenMDArray('string_var')
    dt = var.GetDataType()
    assert dt.GetClass() == gdal.GEDTC_STRING
    assert var.Read() == ['abcd', 'ef']

    group = rg.OpenGroup('group')
    var = group.OpenMDArray('char_var')
    assert var.GetFullName() == '/group/char_var'
    dims = var.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetFullName() == '/Y'
    assert dims[0].GetSize() == 1
    assert dims[1].GetFullName() == '/group/Y'
    assert dims[1].GetSize() == 2
    assert dims[2].GetFullName() == '/group/X'
    assert dims[2].GetSize() == 3

    dims = rg.GetDimensions()
    assert [dims[i].GetFullName() for i in range(len(dims))] == ['/Y', '/X', '/Y2', '/X2', '/Z2', '/T2']

    dims = group.GetDimensions()
    assert [dims[i].GetFullName() for i in range(len(dims))] == ['/group/Y', '/group/X']


def test_netcdf_multidim_2d_dim_char_variable():

    ds = gdal.OpenEx('data/netcdf/2d_dim_char_variable.nc', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    var = rg.OpenMDArray('TIME')
    assert var.GetDataType().GetClass() == gdal.GEDTC_STRING
    assert var.GetDataType().GetMaxStringLength() == 16
    assert var.GetDimensionCount() == 1
    dims = var.GetDimensions()
    assert len(dims) == 1
    assert dims[0].GetName() == 'TIME'
    assert var.Read() == ['2019-06-29', '2019-06-30']

    indexing_var = rg.GetDimensions()[0].GetIndexingVariable()
    assert indexing_var
    assert indexing_var.GetName() == 'TIME'


def test_netcdf_multidim_read_array():

    ds = gdal.OpenEx('data/netcdf/alldatatypes.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()

    # 0D
    var = rg.OpenMDArray('ubyte_no_dim_var')
    assert var
    assert struct.unpack('B', var.Read()) == (2,)
    assert struct.unpack('H', var.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))) == (2,)

    # 1D
    var = rg.OpenMDArray('ubyte_x2_var')
    data = var.Read(array_start_idx = [1], count = [2], array_step = [2])
    got_data_ref = (2, 4)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    data = var.Read(array_start_idx = [1], count = [2], array_step = [2],
                    buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert struct.unpack('H' * (len(data) // 2), data) == got_data_ref

    data = var.Read(array_start_idx = [2], count = [2], array_step = [-2])
    got_data_ref = (3, 1)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    # 2D
    var = rg.OpenMDArray('ubyte_y2_x2_var')
    data = var.Read(count = [2, 3], array_step = [2, 1])
    got_data_ref = (1, 2, 3, 9, 10, 11)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    data = var.Read(count = [2, 3], array_step = [2, 1], buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert struct.unpack('H' * (len(data) // 2), data) == got_data_ref

    data = var.Read(array_start_idx = [1, 2], count = [2, 2])
    got_data_ref = (7, 8, 11, 12)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    data = var.Read(array_start_idx = [1, 2], count = [2, 2], buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert struct.unpack('H' * (len(data) // 2), data) == got_data_ref

    # 3D
    var = rg.OpenMDArray('ubyte_z2_y2_x2_var')
    data = var.Read(count = [3, 2, 3], array_step = [1, 2, 1])
    got_data_ref = (1, 2, 3, 9, 10, 11, 2, 3, 4, 10, 11, 12, 3, 4, 5, 11, 12, 1)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    data = var.Read(count = [3, 2, 3], array_step = [1, 2, 1], buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert struct.unpack('H' * (len(data) // 2), data) == got_data_ref

    data = var.Read(array_start_idx = [1, 1, 1], count = [3, 2, 2])
    got_data_ref = (7, 8, 11, 12, 8, 9, 12, 1, 9, 10, 1, 2)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    data = var.Read(array_start_idx = [1, 1, 1], count = [3, 2, 2], buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert struct.unpack('H' * (len(data) // 2), data) == got_data_ref

    # Test reading from slice (most optimized path)
    data = var.Read(array_start_idx = [3, 0, 0], count = [1, 2, 3])
    data_from_slice = var[3].Read(count = [2, 3])
    assert data_from_slice == data

    # Test reading from slice (slower path)
    data = var.Read(array_start_idx = [3, 0, 0], count = [1, 2, 3], array_step = [1, 2, 1])
    data_from_slice = var[3].Read(count = [2, 3], array_step = [2, 1])
    assert data_from_slice == data

    # 4D
    var = rg.OpenMDArray('ubyte_t2_z2_y2_x2_var')
    data = var.Read(count = [2, 3, 2, 3], array_step = [1, 1, 2, 1])
    got_data_ref = (1, 2, 3, 9, 10, 11, 2, 3, 4, 10, 11, 12, 3, 4, 5, 11, 12, 1, 2, 3, 4, 10, 11, 12, 3, 4, 5, 11, 12, 1, 4, 5, 6, 12, 1, 2)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    data = var.Read(count = [2, 3, 2, 3], array_step = [1, 1, 2, 1], buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert struct.unpack('H' * (len(data) // 2), data) ==  got_data_ref


def test_netcdf_multidim_attr_alldatatypes():

    ds = gdal.OpenEx('data/netcdf/alldatatypes.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()

    attrs = rg.GetAttributes()
    assert len(attrs) == 1
    assert attrs[0].GetName() == 'global_attr'

    attrs = rg.OpenGroup('group').GetAttributes()
    assert len(attrs) == 1
    assert attrs[0].GetName() == 'group_global_attr'

    var = rg.OpenMDArray('ubyte_var')
    assert var
    attrs = var.GetAttributes()
    assert len(attrs) == 30
    map_attrs = {}
    for attr in attrs:
        map_attrs[attr.GetName()] = attr

    attr = map_attrs['attr_byte']
    assert attr.GetDimensionCount() == 0
    assert len(attr.GetDimensionsSize()) == 0
    assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int16
    assert attr.Read() == -128

    assert map_attrs['attr_ubyte'].Read() == 255

    assert map_attrs['attr_char'].GetDataType().GetClass() == gdal.GEDTC_STRING
    assert map_attrs['attr_char'].GetDimensionCount() == 0
    assert map_attrs['attr_char'].Read() == 'x'
    assert map_attrs['attr_char'].ReadAsStringArray() == ['x']
    with gdaltest.error_handler():
        assert not map_attrs['attr_char'].ReadAsRaw()

    assert map_attrs['attr_string_as_repeated_char'].GetDataType().GetClass() == gdal.GEDTC_STRING
    assert map_attrs['attr_string_as_repeated_char'].GetDimensionCount() == 0
    assert map_attrs['attr_string_as_repeated_char'].Read() == 'xy'
    assert map_attrs['attr_string_as_repeated_char'].ReadAsStringArray() == ['xy']

    assert map_attrs['attr_empty_char'].GetDataType().GetClass() == gdal.GEDTC_STRING
    assert map_attrs['attr_empty_char'].GetDimensionCount() == 0
    assert map_attrs['attr_empty_char'].Read() == ''

    assert map_attrs['attr_two_strings'].GetDataType().GetClass() == gdal.GEDTC_STRING
    assert map_attrs['attr_two_strings'].GetDimensionCount() == 1
    assert map_attrs['attr_two_strings'].GetDimensionsSize()[0] == 2
    assert map_attrs['attr_two_strings'].Read() == ['ab', 'cd']
    assert map_attrs['attr_two_strings'].ReadAsString() == 'ab'

    assert map_attrs['attr_int'].Read() == -2147483647

    assert map_attrs['attr_float'].Read() == 1.25

    assert map_attrs['attr_double'].Read() == 1.25125
    assert map_attrs['attr_double'].ReadAsDoubleArray() == (1.25125,)

    assert map_attrs['attr_int64'].Read() == -9.223372036854776e+18

    assert map_attrs['attr_uint64'].Read() == 1.8446744073709552e+19

    assert map_attrs['attr_complex_int16'].GetDataType().GetNumericDataType() == gdal.GDT_CInt16
    assert map_attrs['attr_complex_int16'].Read() == 1.0
    assert map_attrs['attr_complex_int16'].ReadAsString() == '1+2j'

    assert map_attrs['attr_two_bytes'].Read() == (-128, -127)

    assert map_attrs['attr_two_ints'].Read() == (-2147483648, -2147483647)

    assert map_attrs['attr_two_doubles'].GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert map_attrs['attr_two_doubles'].GetDimensionCount() == 1
    assert map_attrs['attr_two_doubles'].GetDimensionsSize()[0] == 2
    assert map_attrs['attr_two_doubles'].Read() == (1.25125, 2.125125)
    assert map_attrs['attr_two_doubles'].ReadAsDouble() == 1.25125

    assert map_attrs['attr_enum_ubyte'].GetDataType().GetNumericDataType() == gdal.GDT_Byte
    assert map_attrs['attr_enum_ubyte'].Read() == 1

    assert map_attrs['attr_enum_int'].GetDataType().GetNumericDataType() == gdal.GDT_Int32
    assert map_attrs['attr_enum_int'].Read() == 1000000001

    assert len(map_attrs['attr_custom_type_2_elts'].ReadAsRaw()) == 8

    # Compound type contains a string
    with gdaltest.error_handler():
        assert not map_attrs['attr_custom_with_string'].ReadAsRaw()

def test_netcdf_multidim_read_projection():

    ds = gdal.OpenEx('data/netcdf/cf_lcc1sp.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    var = rg.OpenMDArray('Total_cloud_cover')
    srs = var.GetSpatialRef()
    assert srs
    assert srs.GetDataAxisToSRSAxisMapping() == [3, 2]
    lat_origin = srs.GetProjParm('latitude_of_origin')

    assert lat_origin == 25, ('Latitude of origin does not match expected:\n%f'
                             % lat_origin)

    dim_x = next((x for x in var.GetDimensions() if x.GetName() == 'x'), None)
    assert dim_x
    assert dim_x.GetType() == gdal.DIM_TYPE_HORIZONTAL_X

    dim_y = next((x for x in var.GetDimensions() if x.GetName() == 'y'), None)
    assert dim_y
    assert dim_y.GetType() == gdal.DIM_TYPE_HORIZONTAL_Y

###############################################################################
# Test reading a netCDF file whose grid_mapping attribute uses an
# expanded form

def test_netcdf_multidim_expanded_form_of_grid_mapping():

    ds = gdal.OpenEx('data/netcdf/expanded_form_of_grid_mapping.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    var = rg.OpenMDArray('temp')
    sr = var.GetSpatialRef()
    assert sr
    assert 'Transverse_Mercator' in sr.ExportToWkt()


def test_netcdf_multidim_read_netcdf_4d():

    ds = gdal.OpenEx('data/netcdf/netcdf-4d.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    var = rg.OpenMDArray('t')
    assert not var.GetSpatialRef()

    dim_x = next((x for x in var.GetDimensions() if x.GetName() == 'longitude'), None)
    assert dim_x
    assert dim_x.GetType() == gdal.DIM_TYPE_HORIZONTAL_X

    dim_y = next((x for x in var.GetDimensions() if x.GetName() == 'latitude'), None)
    assert dim_y
    assert dim_y.GetType() == gdal.DIM_TYPE_HORIZONTAL_Y

    dim_time = next((x for x in var.GetDimensions() if x.GetName() == 'time'), None)
    assert dim_time
    assert dim_time.GetType() == gdal.DIM_TYPE_TEMPORAL


def test_netcdf_multidim_create_nc3():

    drv = gdal.GetDriverByName('netCDF')
    with gdaltest.error_handler():
        assert not drv.CreateMultiDimensional('/i_do/not_exist.nc')

    def f():
        ds = drv.CreateMultiDimensional('tmp/multidim_nc3.nc', [], ['FORMAT=NC'])
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetStructuralInfo() == {'NC_FORMAT': 'CLASSIC'}
        assert not rg.GetDimensions()

        # not support on NC3
        with gdaltest.error_handler():
            assert not rg.CreateGroup("subgroup")

        dim_x = rg.CreateDimension('X', None, None, 2)
        assert dim_x
        assert dim_x.GetName() == 'X'
        assert dim_x.GetSize() == 2

        dim_y_unlimited = rg.CreateDimension('Y', None, None, 123, ['UNLIMITED=YES'])
        assert dim_y_unlimited
        assert dim_y_unlimited.GetSize() == 123

        with gdaltest.error_handler():
            assert not rg.CreateDimension('unlimited2', None, None, 123, ['UNLIMITED=YES'])

        with gdaltest.error_handler():
            assert not rg.CreateDimension('too_big', None, None, (1 << 31) - 1)

        var = rg.CreateMDArray('my_var_no_dim', [],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert var
        assert var.SetNoDataValueDouble(1) == gdal.CE_None
        assert struct.unpack('d', var.Read()) == (1, )
        assert var.GetNoDataValueAsDouble() == 1
        assert var.DeleteNoDataValue() == gdal.CE_None
        assert var.GetNoDataValueAsRaw() is None
        assert var.Write(struct.pack('d', 1.25125)) == gdal.CE_None
        assert struct.unpack('d', var.Read()) == (1.25125,)
        assert var.SetScale(2.5) == gdal.CE_None
        assert var.GetScale() == 2.5
        assert var.GetScaleStorageType() == gdal.GDT_Float64
        assert var.SetOffset(1.5) == gdal.CE_None
        assert var.GetOffset() == 1.5
        assert var.GetOffsetStorageType() == gdal.GDT_Float64

        var.SetUnit("foo")
        var = rg.OpenMDArray('my_var_no_dim')
        assert var.GetUnit() == "foo"

        var = rg.CreateMDArray('my_var_x', [dim_x],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert var

        var = rg.OpenMDArray('my_var_x')
        assert var
        assert var.GetDimensionCount() == 1
        assert var.GetDimensions()[0].GetSize() == 2
        assert struct.unpack('d' * 2, var.Read()) == (9.969209968386869e+36, 9.969209968386869e+36)

        var = rg.CreateMDArray('my_var_with_unlimited', [dim_y_unlimited, dim_x],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert var

        var = rg.OpenMDArray('my_var_with_unlimited')
        assert var
        assert var.GetDimensionCount() == 2
        assert var.GetDimensions()[0].GetSize() == 0
        assert var.GetDimensions()[1].GetSize() == 2

        att = rg.CreateAttribute('att_text', [], gdal.ExtendedDataType.CreateString())
        assert att
        with gdaltest.error_handler():
            assert not att.Read()
        assert att.Write('f') == gdal.CE_None
        assert att.Write('foo') == gdal.CE_None
        assert att.Read() == 'foo'
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == 'foo'

        # netCDF 3 cannot support NC_STRING. Needs fixed size strings
        with gdaltest.error_handler():
            var = rg.CreateMDArray('my_var_string_array', [dim_x], gdal.ExtendedDataType.CreateString())
        assert not var

        string_type = gdal.ExtendedDataType.CreateString(10)
        var = rg.CreateMDArray('my_var_string_array_fixed_width', [dim_x], string_type)
        assert var
        assert var.GetDimensionCount() == 1
        assert var.Write(['', '0123456789truncated']) == gdal.CE_None
        var = rg.OpenMDArray('my_var_string_array_fixed_width')
        assert var
        assert var.Read() == ['', '0123456789']
        assert var.Write(['foo'], array_start_idx = [1]) == gdal.CE_None
        assert var.Read() == ['', 'foo']

    f()

    def f2():
        ds = gdal.OpenEx('tmp/multidim_nc3.nc', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        att = next((x for x in rg.GetAttributes() if x.GetName() == 'att_text'), None)
        # Test correct switching to define mode due to attribute value being longer
        assert att.Write('rewritten_attribute') == gdal.CE_None
        assert att.Read() == 'rewritten_attribute'

    f2()

    def create_georeferenced():
        ds = gdal.OpenEx('tmp/multidim_nc3.nc', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        dim_y = rg.CreateDimension('my_y', gdal.DIM_TYPE_HORIZONTAL_Y, None, 2)
        dim_x = rg.CreateDimension('my_x', gdal.DIM_TYPE_HORIZONTAL_X, None, 3)
        var_y = rg.CreateMDArray('my_y', [dim_y], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        var_x = rg.CreateMDArray('my_x', [dim_x], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        var = rg.CreateMDArray('my_georeferenced_var', [dim_y, dim_x],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert var
        assert var.GetDimensions()[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        assert var.SetSpatialRef(srs) == gdal.CE_None
        got_srs = var.GetSpatialRef()
        assert got_srs
        assert '32631' in got_srs.ExportToWkt()
        assert var_x.GetAttribute('standard_name').Read() == 'projection_x_coordinate'
        assert var_y.GetAttribute('standard_name').Read() == 'projection_y_coordinate'

    create_georeferenced()

    gdal.Unlink('tmp/multidim_nc3.nc')


def test_netcdf_multidim_create_nc4():

    drv = gdal.GetDriverByName('netCDF')
    def f():
        ds = drv.CreateMultiDimensional('tmp/multidim_nc4.nc')
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetStructuralInfo() == {'NC_FORMAT': 'NETCDF4'}
        subgroup = rg.CreateGroup("subgroup")
        assert subgroup
        assert subgroup.GetName() == 'subgroup'

        with gdaltest.error_handler():
            assert not rg.CreateGroup("")

        with gdaltest.error_handler():
            assert not rg.CreateGroup("subgroup")

        subgroup = rg.OpenGroup("subgroup")
        assert subgroup

        dim1 = subgroup.CreateDimension('unlimited1', None, None, 1, ['UNLIMITED=YES'])
        assert dim1
        dim2 = subgroup.CreateDimension('unlimited2', None, None, 2, ['UNLIMITED=YES'])
        assert dim2

        var = rg.CreateMDArray('my_var_no_dim', [],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64),
                               ['BLOCKSIZE='])
        assert var
        assert var.SetNoDataValueDouble(1) == gdal.CE_None
        assert struct.unpack('d', var.Read()) == (1, )
        assert var.GetNoDataValueAsDouble() == 1
        assert var.DeleteNoDataValue() == gdal.CE_None
        assert var.GetNoDataValueAsRaw() is None
        assert var.Write(struct.pack('d', 1.25125)) == gdal.CE_None
        assert struct.unpack('d', var.Read()) == (1.25125,)
        assert var.SetScale(2.5, gdal.GDT_Float32) == gdal.CE_None
        assert var.GetScale() == 2.5
        assert var.GetScaleStorageType() == gdal.GDT_Float32
        assert var.SetScale(-2.5) == gdal.CE_None
        assert var.GetScale() == -2.5
        assert var.SetOffset(1.5, gdal.GDT_Float32) == gdal.CE_None
        assert var.GetOffset() == 1.5
        assert var.GetOffsetStorageType() == gdal.GDT_Float32
        assert var.SetOffset(-1.5) == gdal.CE_None
        assert var.GetOffset() == -1.5

        dim_x = rg.CreateDimension('X', None, None, 2)
        assert dim_x
        assert dim_x.GetName() == 'X'
        assert dim_x.GetType() == ''
        assert dim_x.GetSize() == 2

        var = rg.CreateMDArray('my_var_x', [dim_x],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64),
                               ['BLOCKSIZE=1', 'COMPRESS=DEFLATE', 'ZLEVEL=6',
                                'CHECKSUM=YES'])
        assert var.Write(struct.pack('f' * 2, 1.25, 2.25),
                buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float32)) == gdal.CE_None
        assert struct.unpack('d' * 2, var.Read()) == (1.25, 2.25)
        assert var.Write(struct.pack('d' * 2, 1.25125, 2.25125)) == gdal.CE_None
        assert var
        assert var.GetBlockSize() == [1]
        assert var.GetStructuralInfo() == { 'COMPRESS': 'DEFLATE' }

        # Try with random filter id. Just to test that FILTER is taken
        # into account
        with gdaltest.error_handler():
            var = rg.CreateMDArray('my_var_x', [dim_x],
                                gdal.ExtendedDataType.Create(gdal.GDT_Float64),
                                ['FILTER=123456789,2'])
        assert var is None

        var = rg.OpenMDArray('my_var_x')
        assert var
        assert var.GetDimensionCount() == 1
        assert var.GetDimensions()[0].GetSize() == 2
        assert struct.unpack('d' * 2, var.Read()) == (1.25125, 2.25125)

        def dims_from_non_netcdf(rg):

            dim_z = rg.CreateDimension('Z', None, None, 4)

            mem_ds = gdal.GetDriverByName('MEM').CreateMultiDimensional('myds')
            mem_rg = mem_ds.GetRootGroup()
            # the netCDF file has already a X variable of size 2
            dim_x_from_mem = mem_rg.CreateDimension('X', None, None, 2)
            # the netCDF file has no Y variable
            dim_y_from_mem = mem_rg.CreateDimension('Y', None, None, 3)
            # the netCDF file has already a Z variable, but of a different size
            dim_z_from_mem = mem_rg.CreateDimension('Z', None, None, dim_z.GetSize() + 1)
            with gdaltest.error_handler():
                var = rg.CreateMDArray('my_var_x_y',
                                       [dim_x_from_mem, dim_y_from_mem, dim_z_from_mem],
                                       gdal.ExtendedDataType.Create(gdal.GDT_Float64))
            assert var
            assert var.GetDimensionCount() == 3
            assert var.GetDimensions()[0].GetSize() == 2
            assert var.GetDimensions()[1].GetSize() == 3
            assert var.GetDimensions()[2].GetSize() == 4

        dims_from_non_netcdf(rg)

        for dt in (gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
                   gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32,
                   gdal.GDT_Float64, gdal.GDT_CInt16, gdal.GDT_CInt32,
                   gdal.GDT_CFloat32, gdal.GDT_CFloat64):

            varname = 'my_var_' + gdal.GetDataTypeName(dt)
            var = rg.CreateMDArray(varname, [dim_x],
                                gdal.ExtendedDataType.Create(dt))
            assert var
            var = rg.OpenMDArray(varname)
            assert var
            assert var.GetDataType().GetNumericDataType() == dt

        # Check good behaviour when complex data type already registered
        dt = gdal.GDT_CFloat32
        varname = 'my_var_' + gdal.GetDataTypeName(dt) + "_bis"
        var = rg.CreateMDArray(varname, [dim_x],
                            gdal.ExtendedDataType.Create(dt))
        assert var
        assert var.GetNoDataValueAsRaw() is None
        assert var.SetNoDataValueRaw(struct.pack('ff', 3.5, 4.5)) == gdal.CE_None
        assert struct.unpack('ff', var.GetNoDataValueAsRaw()) == (3.5, 4.5)
        assert var.Write(struct.pack('ff' * 2, -1.25, 1.25, 2.5, -2.5)) == gdal.CE_None
        assert struct.unpack('ff' * 2, var.Read()) == (-1.25, 1.25, 2.5, -2.5)
        var = rg.OpenMDArray(varname)
        assert var
        assert var.GetDataType().GetNumericDataType() == dt

        var = rg.CreateMDArray('var_as_nc_byte', [],
                gdal.ExtendedDataType.Create(gdal.GDT_Float64), ['NC_TYPE=NC_BYTE'])
        assert var.GetDataType().GetNumericDataType() == gdal.GDT_Int16
        assert var.GetNoDataValueAsRaw() is None
        assert var.SetNoDataValueDouble(-127) == gdal.CE_None
        assert struct.unpack('h', var.GetNoDataValueAsRaw()) == (-127, )
        assert var.GetNoDataValueAsDouble() == -127
        assert var.Write(struct.pack('h', -128)) == gdal.CE_None
        assert struct.unpack('h', var.Read()) == (-128, )

        var = rg.CreateMDArray('var_as_nc_int64', [],
                gdal.ExtendedDataType.Create(gdal.GDT_Float64), ['NC_TYPE=NC_INT64'])
        assert var.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert var.Write(struct.pack('d', -1234567890123)) == gdal.CE_None
        assert struct.unpack('d', var.Read()) == (-1234567890123, )

        var = rg.CreateMDArray('var_as_nc_uint64', [],
                gdal.ExtendedDataType.Create(gdal.GDT_Float64), ['NC_TYPE=NC_UINT64'])
        assert var.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert var.Write(struct.pack('d', 1234567890123)) == gdal.CE_None
        assert struct.unpack('d', var.Read()) == (1234567890123, )

        # Test creation of compound data type
        comp0 = gdal.EDTComponent.Create('x', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int16))
        comp1 = gdal.EDTComponent.Create('y', 4, gdal.ExtendedDataType.Create(gdal.GDT_Int32))
        compound_dt = gdal.ExtendedDataType.CreateCompound("mycompoundtype", 8, [comp0, comp1])

        var = rg.CreateMDArray('var_with_compound_type', [dim_x], compound_dt)
        assert var
        assert var.Write(struct.pack('hi', -128, -12345678), count = [1]) == gdal.CE_None
        assert struct.unpack('hi' * 2, var.Read()) == (-128, -12345678, 0, 0)

        extract_compound_dt = gdal.ExtendedDataType.CreateCompound("mytype", 4,
            [gdal.EDTComponent.Create('y', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int32))])
        got_data = var.Read(buffer_datatype = extract_compound_dt)
        assert len(got_data) == 8
        assert struct.unpack('i' * 2, got_data) == (-12345678, 0)

        var = rg.CreateMDArray('another_var_with_compound_type', [dim_x], compound_dt)
        assert var

        var = rg.OpenMDArray('another_var_with_compound_type')
        assert var.GetDataType().Equals(compound_dt)

        # Attribute on variable
        att = var.CreateAttribute('var_att_text', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('foo_of_var') == gdal.CE_None
        att = next((x for x in var.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == 'foo_of_var'

        with gdaltest.error_handler():
            assert not rg.CreateAttribute('attr_too_many_dimensions', [2, 3], gdal.ExtendedDataType.CreateString())

        att = rg.CreateAttribute('att_text', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('first_write') == gdal.CE_None
        assert att.Read() == 'first_write'
        assert att.Write(123) == gdal.CE_None
        assert att.Read() == '123'
        assert att.Write('foo') == gdal.CE_None
        assert att.Read() == 'foo'
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == 'foo'

        att = rg.CreateAttribute('att_string', [], gdal.ExtendedDataType.CreateString(),
                                 ['NC_TYPE=NC_STRING'])
        assert att
        assert att.Write(123) == gdal.CE_None
        assert att.Read() == '123'
        assert att.Write('bar') == gdal.CE_None
        assert att.Read() == 'bar'
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == 'bar'

        # There is an issue on 32-bit platforms, likely in libnetcdf or libhdf5 itself,
        # with writing more than one string
        if sys.maxsize > 0x7FFFFFFF:
            att = rg.CreateAttribute('att_two_strings', [2], gdal.ExtendedDataType.CreateString())
            assert att
            with gdaltest.error_handler():
                assert att.Write(['not_enough_elements']) != gdal.CE_None
            assert att.Write([1, 2]) == gdal.CE_None
            assert att.Read() == ['1', '2']
            assert att.Write(['foo', 'barbaz']) == gdal.CE_None
            assert att.Read() == ['foo', 'barbaz']
            att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
            assert att.Read() == ['foo', 'barbaz']

        att = rg.CreateAttribute('att_double', [], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert att
        assert att.Write(1.25125) == gdal.CE_None
        assert att.Read() == 1.25125
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == 1.25125

        att = rg.CreateAttribute('att_two_double', [2], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert att
        assert att.Write([1.25125, 2.25125]) == gdal.CE_None
        assert att.Read() == (1.25125, 2.25125)
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == (1.25125, 2.25125)

        att = rg.CreateAttribute('att_byte', [],
                                 gdal.ExtendedDataType.Create(gdal.GDT_Int16),
                                 ['NC_TYPE=NC_BYTE'])
        assert att
        assert att.Write(-128) == gdal.CE_None
        assert att.Read() == -128
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == -128

        att = rg.CreateAttribute('att_two_byte', [2],
                                 gdal.ExtendedDataType.Create(gdal.GDT_Int16),
                                 ['NC_TYPE=NC_BYTE'])
        assert att
        assert att.Write([-128, -127]) == gdal.CE_None
        assert att.Read() == (-128, -127)
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == (-128, -127)

        att = rg.CreateAttribute('att_int64', [],
                                 gdal.ExtendedDataType.Create(gdal.GDT_Float64),
                                 ['NC_TYPE=NC_INT64'])
        assert att
        assert att.Write(-1234567890123) == gdal.CE_None
        assert att.Read() == -1234567890123
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == -1234567890123

        att = rg.CreateAttribute('att_two_int64', [2],
                                 gdal.ExtendedDataType.Create(gdal.GDT_Float64),
                                 ['NC_TYPE=NC_INT64'])
        assert att
        assert att.Write([-1234567890123, -1234567890124]) == gdal.CE_None
        assert att.Read() == (-1234567890123, -1234567890124)
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == (-1234567890123, -1234567890124)

        att = rg.CreateAttribute('att_uint64', [],
                                 gdal.ExtendedDataType.Create(gdal.GDT_Float64),
                                 ['NC_TYPE=NC_UINT64'])
        assert att
        assert att.Write(1234567890123) == gdal.CE_None
        assert att.Read() == 1234567890123
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert att.Read() == 1234567890123

        att = rg.CreateAttribute('att_compound', [], compound_dt)
        assert att
        assert att.Write(struct.pack('hi', -128, -12345678)) == gdal.CE_None
        assert len(att.Read()) == 8
        assert struct.unpack('hi', att.Read()) == (-128, -12345678)
        att = next((x for x in rg.GetAttributes() if x.GetName() == att.GetName()), None)
        assert struct.unpack('hi', att.Read()) == (-128, -12345678)

        # netCDF 4 can support NC_STRING
        var = rg.CreateMDArray('my_var_string_array', [dim_x], gdal.ExtendedDataType.CreateString())
        assert var
        assert var.Write(['', '0123456789']) == gdal.CE_None
        var = rg.OpenMDArray('my_var_string_array')
        assert var
        assert var.Read() == ['', '0123456789']

        string_type = gdal.ExtendedDataType.CreateString(10)
        var = rg.CreateMDArray('my_var_string_array_fixed_width', [dim_x], string_type)
        assert var
        assert var.GetDimensionCount() == 1
        assert var.Write(['', '0123456789truncated']) == gdal.CE_None
        var = rg.OpenMDArray('my_var_string_array_fixed_width')
        assert var
        assert var.Read() == ['', '0123456789']

    f()

    def f2():
        ds = gdal.OpenEx('tmp/multidim_nc4.nc', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        assert rg.CreateGroup('subgroup2')

    f2()

    def f3():
        ds = gdal.OpenEx('tmp/multidim_nc4.nc', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        att = next((x for x in rg.GetAttributes() if x.GetName() == 'att_text'), None)
        assert att.Write('rewritten_attribute') == gdal.CE_None
        assert att.Read() == 'rewritten_attribute'

    f3()

    def create_georeferenced_projected(grp_name, set_dim_type):
        ds = gdal.OpenEx('tmp/multidim_nc4.nc', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        subg = rg.CreateGroup(grp_name)
        dim_y = subg.CreateDimension('my_y', gdal.DIM_TYPE_HORIZONTAL_Y if set_dim_type else None, None, 2)
        dim_x = subg.CreateDimension('my_x', gdal.DIM_TYPE_HORIZONTAL_X if set_dim_type else None, None, 3)
        var_y = subg.CreateMDArray('my_y', [dim_y], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        var_x = subg.CreateMDArray('my_x', [dim_x], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        var = subg.CreateMDArray('my_georeferenced_var', [dim_y, dim_x],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert var
        if set_dim_type:
            assert var.GetDimensions()[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
            assert var.GetDimensions()[1].GetType() == gdal.DIM_TYPE_HORIZONTAL_X
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        assert var.SetSpatialRef(None) == gdal.CE_None
        assert var.SetSpatialRef(srs) == gdal.CE_None
        got_srs = var.GetSpatialRef()
        assert got_srs
        assert '32631' in got_srs.ExportToWkt()
        assert var_x.GetAttribute('standard_name').Read() == 'projection_x_coordinate'
        assert var_x.GetAttribute('long_name').Read() == 'x coordinate of projection'
        assert var_x.GetAttribute('units').Read() == 'm'
        assert var_y.GetAttribute('standard_name').Read() == 'projection_y_coordinate'
        assert var_y.GetAttribute('long_name').Read() == 'y coordinate of projection'
        assert var_y.GetAttribute('units').Read() == 'm'

        var = subg.OpenMDArray(var.GetName())
        assert var
        assert var.GetDimensions()[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
        assert var.GetDimensions()[1].GetType() == gdal.DIM_TYPE_HORIZONTAL_X

    create_georeferenced_projected('georeferenced_projected_with_dim_type', True)
    with gdaltest.error_handler():
        create_georeferenced_projected('georeferenced_projected_without_dim_type', False)

    def create_georeferenced_geographic(grp_name, set_dim_type):
        ds = gdal.OpenEx('tmp/multidim_nc4.nc', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        subg = rg.CreateGroup(grp_name)
        dim_y = subg.CreateDimension('my_y', gdal.DIM_TYPE_HORIZONTAL_Y if set_dim_type else None, None, 2)
        dim_x = subg.CreateDimension('my_x', gdal.DIM_TYPE_HORIZONTAL_X if set_dim_type else None, None, 3)
        var_y = subg.CreateMDArray('my_y', [dim_y], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        var_x = subg.CreateMDArray('my_x', [dim_x], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        var = subg.CreateMDArray('my_georeferenced_var', [dim_y, dim_x],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert var
        if set_dim_type:
            assert var.GetDimensions()[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
            assert var.GetDimensions()[1].GetType() == gdal.DIM_TYPE_HORIZONTAL_X
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        assert var.SetSpatialRef(srs) == gdal.CE_None
        got_srs = var.GetSpatialRef()
        assert got_srs
        assert '4326' in got_srs.ExportToWkt()
        assert var_x.GetAttribute('standard_name').Read() == 'longitude'
        assert var_x.GetAttribute('long_name').Read() == 'longitude'
        assert var_x.GetAttribute('units').Read() == 'degrees_east'
        assert var_y.GetAttribute('standard_name').Read() == 'latitude'
        assert var_y.GetAttribute('long_name').Read() == 'latitude'
        assert var_y.GetAttribute('units').Read() == 'degrees_north'

        var = subg.OpenMDArray(var.GetName())
        assert var
        assert var.GetDimensions()[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
        assert var.GetDimensions()[1].GetType() == gdal.DIM_TYPE_HORIZONTAL_X

    create_georeferenced_geographic('georeferenced_geographic_with_dim_type', True)
    with gdaltest.error_handler():
        create_georeferenced_geographic('georeferenced_geographic_without_dim_type', False)

    gdal.Unlink('tmp/multidim_nc4.nc')


def test_netcdf_multidim_create_several_arrays_with_srs():

    tmpfilename = 'tmp/several_arrays_with_srs.nc'

    def create():
        drv = gdal.GetDriverByName('netCDF')
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()

        lat = rg.CreateDimension('latitude', gdal.DIM_TYPE_HORIZONTAL_X, None, 2)
        rg.CreateMDArray('latitude', [lat], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        lon = rg.CreateDimension('longitude', gdal.DIM_TYPE_HORIZONTAL_Y, None, 2)
        rg.CreateMDArray('longitude', [lon], gdal.ExtendedDataType.Create(gdal.GDT_Float64))

        x = rg.CreateDimension('x', gdal.DIM_TYPE_HORIZONTAL_X, None, 2)
        rg.CreateMDArray('x', [x], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        y = rg.CreateDimension('y', gdal.DIM_TYPE_HORIZONTAL_Y, None, 2)
        rg.CreateMDArray('y', [y], gdal.ExtendedDataType.Create(gdal.GDT_Float64))

        ar = rg.CreateMDArray('ar_longlat_4326_1', [lat, lon], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        ar.SetSpatialRef(srs)

        ar = rg.CreateMDArray('ar_longlat_4326_2', [lat, lon], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        ar.SetSpatialRef(srs)

        ar = rg.CreateMDArray('ar_longlat_4258', [lat, lon], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4258)
        ar.SetSpatialRef(srs)

        ar = rg.CreateMDArray('ar_longlat_32631_1', [y, x], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        ar.SetSpatialRef(srs)

        ar = rg.CreateMDArray('ar_longlat_32631_2', [y, x], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        ar.SetSpatialRef(srs)

        ar = rg.CreateMDArray('ar_longlat_32632', [y, x], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32632)
        ar.SetSpatialRef(srs)

    create()

    def read():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        ar = rg.OpenMDArray('ar_longlat_4326_1')
        assert ar.GetSpatialRef().GetAuthorityCode(None) == '4326'

        ar = rg.OpenMDArray('ar_longlat_4326_2')
        assert ar.GetSpatialRef().GetAuthorityCode(None) == '4326'

        ar = rg.OpenMDArray('ar_longlat_4258')
        assert ar.GetSpatialRef().GetAuthorityCode(None) == '4258'

        ar = rg.OpenMDArray('ar_longlat_32631_1')
        assert ar.GetSpatialRef().GetAuthorityCode(None) == '32631'

        ar = rg.OpenMDArray('ar_longlat_32631_2')
        assert ar.GetSpatialRef().GetAuthorityCode(None) == '32631'

        ar = rg.OpenMDArray('ar_longlat_32632')
        assert ar.GetSpatialRef().GetAuthorityCode(None) == '32632'

    read()

    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_create_dim_zero():

    tmpfilename = 'tmp/test_netcdf_multidim_create_dim_zero_in.nc'

    def create():
        drv = gdal.GetDriverByName('netCDF')
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dim_zero = rg.CreateDimension('dim_zero', None, None, 0)
        assert dim_zero
        ar = rg.CreateMDArray('ar', [dim_zero],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        assert ar

    create()

    tmpfilename2 = 'tmp/test_netcdf_multidim_create_dim_zero_out.nc'

    def copy():
        out_ds = gdal.MultiDimTranslate(tmpfilename2, tmpfilename, format = 'netCDF')
        assert out_ds
        rg = out_ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray('ar')
        assert ar

    copy()

    assert gdal.MultiDimInfo(tmpfilename2, detailed = True)

    gdal.Unlink(tmpfilename)
    gdal.Unlink(tmpfilename2)


def test_netcdf_multidim_dims_with_same_name_different_size():

    src_ds = gdal.OpenEx("""<VRTDataset>
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
</VRTDataset>""", gdal.OF_MULTIDIM_RASTER)

    tmpfilename = 'tmp/test_netcdf_multidim_dims_with_same_name_different_size.nc'
    gdal.GetDriverByName('netCDF').CreateCopy(tmpfilename, src_ds)

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar_x = rg.OpenMDArray('X')
        assert ar_x.GetDimensions()[0].GetSize() == 2
        ar_y = rg.OpenMDArray('Y')
        assert ar_y.GetDimensions()[0].GetSize() == 3

    check()

    gdal.Unlink(tmpfilename)

def test_netcdf_multidim_getmdarraynames_options():

    ds = gdal.OpenEx('data/netcdf/with_bounds.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg.GetMDArrayNames() == [ 'lat', 'lat_bnds' ]
    assert rg.GetMDArrayNames(['SHOW_BOUNDS=NO']) == [ 'lat' ]
    assert rg.GetMDArrayNames(['SHOW_INDEXING=NO']) == [ 'lat_bnds' ]

    ds = gdal.OpenEx('data/netcdf/bug5118.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg.GetMDArrayNames() == [ 'height_above_ground1',
                                     'time',
                                     'v-component_of_wind_height_above_ground',
                                     'x',
                                     'y' ]
    assert rg.GetMDArrayNames(['SHOW_COORDINATES=NO']) == \
                                [ 'v-component_of_wind_height_above_ground' ]


    ds = gdal.OpenEx('data/netcdf/sen3_sral_mwr_fake_standard_measurement.nc',
                     gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert 'time_01' in rg.GetMDArrayNames()
    assert 'time_01' not in rg.GetMDArrayNames(['SHOW_TIME=NO'])

    ds = gdal.OpenEx('data/netcdf/byte_no_cf.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert 'mygridmapping' not in rg.GetMDArrayNames()
    assert 'mygridmapping' in rg.GetMDArrayNames(['SHOW_ZERO_DIM=YES'])


def test_netcdf_multidim_indexing_var_through_coordinates_opposite_order():

    tmpfilename = 'tmp/test_netcdf_multidim_indexing_var_through_coordinates.nc'
    drv = gdal.GetDriverByName('netCDF')

    def create():
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dim_rel_to_lat = rg.CreateDimension('related_to_lat', None, None, 1)
        dim_rel_to_lon = rg.CreateDimension('related_to_lon', None, None, 2)
        var = rg.CreateMDArray('var', [dim_rel_to_lat, dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('coordinates', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('lon lat') == gdal.CE_None

        var = rg.CreateMDArray('lat', [dim_rel_to_lat],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('standard_name', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('latitude') == gdal.CE_None

        var = rg.CreateMDArray('lon', [dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('standard_name', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('longitude') == gdal.CE_None

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        dims = rg.GetDimensions()
        dim_lat = dims[0]
        assert dim_lat.GetName() == 'related_to_lat'
        assert dim_lat.GetIndexingVariable().GetName() == 'lat'
        dim_lon = dims[1]
        assert dim_lon.GetName() == 'related_to_lon'
        assert dim_lon.GetIndexingVariable().GetName() == 'lon'

    create()
    check()
    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_indexing_var_through_coordinates_2D_dims_same_order():

    tmpfilename = 'tmp/test_netcdf_multidim_indexing_var_through_coordinates.nc'
    drv = gdal.GetDriverByName('netCDF')

    def create():
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dimZ = rg.CreateDimension('Z', None, None, 1)
        dim_rel_to_lat = rg.CreateDimension('related_to_lat', None, None, 1)
        dim_rel_to_lon = rg.CreateDimension('related_to_lon', None, None, 2)
        var = rg.CreateMDArray('var', [dimZ, dim_rel_to_lat, dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('coordinates', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('Z lat lon') == gdal.CE_None

        var = rg.CreateMDArray('lat', [dim_rel_to_lat, dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('standard_name', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('latitude') == gdal.CE_None

        var = rg.CreateMDArray('lon', [dim_rel_to_lat, dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('standard_name', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('longitude') == gdal.CE_None

        rg.CreateMDArray('Z', [dimZ],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        dims = rg.GetDimensions()
        dim_lat = dims[1]
        assert dim_lat.GetName() == 'related_to_lat'
        assert dim_lat.GetIndexingVariable().GetName() == 'lat'
        dim_lon = dims[2]
        assert dim_lon.GetName() == 'related_to_lon'
        assert dim_lon.GetIndexingVariable().GetName() == 'lon'

    create()
    check()
    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_indexing_var_through_coordinates_2D_dims_opposite_order():

    tmpfilename = 'tmp/test_netcdf_multidim_indexing_var_through_coordinates.nc'
    drv = gdal.GetDriverByName('netCDF')

    def create():
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dimZ = rg.CreateDimension('Z', None, None, 1)
        dim_rel_to_lat = rg.CreateDimension('related_to_lat', None, None, 1)
        dim_rel_to_lon = rg.CreateDimension('related_to_lon', None, None, 2)
        var = rg.CreateMDArray('var', [dimZ, dim_rel_to_lat, dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('coordinates', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('lon lat Z') == gdal.CE_None

        var = rg.CreateMDArray('lat', [dim_rel_to_lat, dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('standard_name', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('latitude') == gdal.CE_None

        var = rg.CreateMDArray('lon', [dim_rel_to_lat, dim_rel_to_lon],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        att = var.CreateAttribute('standard_name', [], gdal.ExtendedDataType.CreateString())
        assert att
        assert att.Write('longitude') == gdal.CE_None

        rg.CreateMDArray('Z', [dimZ],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        dims = rg.GetDimensions()
        dim_lat = dims[1]
        assert dim_lat.GetName() == 'related_to_lat'
        assert dim_lat.GetIndexingVariable().GetName() == 'lat'
        dim_lon = dims[2]
        assert dim_lon.GetName() == 'related_to_lon'
        assert dim_lon.GetIndexingVariable().GetName() == 'lon'

    create()
    check()
    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_indexing_var_single_dim():

    tmpfilename = 'tmp/test_netcdf_multidim_indexing_var_single_dim.nc'
    drv = gdal.GetDriverByName('netCDF')

    def create():
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dimK = rg.CreateDimension('k', None, None, 1)
        rg.CreateMDArray('time', [dimK],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        dimL = rg.CreateDimension('l', None, None, 1)
        rg.CreateMDArray('unrelated', [dimL],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        dims = rg.GetDimensions()
        dim = dims[0]
        assert dim.GetName() == 'k'
        assert dim.GetIndexingVariable().GetName() == 'time'

    create()
    check()
    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_indexing_var_single_dim_two_candidates():

    tmpfilename = 'tmp/test_netcdf_multidim_indexing_var_single_dim_two_candidates.nc'
    drv = gdal.GetDriverByName('netCDF')

    def create():
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dimK = rg.CreateDimension('k', None, None, 1)
        rg.CreateMDArray('time', [dimK],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))
        rg.CreateMDArray('another_time', [dimK],  gdal.ExtendedDataType.Create(gdal.GDT_Float64))

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        dims = rg.GetDimensions()
        dim = dims[0]
        assert dim.GetIndexingVariable() is None

    create()
    check()
    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_stats():

    tmpfilename = 'tmp/test_netcdf_multidim_stats.nc'
    drv = gdal.GetDriverByName('netCDF')

    def create():
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
        dim1 = rg.CreateDimension("dim1", "unspecified type", "unspecified direction", 3)
        float64dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        ar = rg.CreateMDArray("myarray", [dim0, dim1], float64dt)
        ar.SetNoDataValueDouble(6)
        data = struct.pack('d' * 6, 1, 2, 3, 4, 5, 6)
        ar.Write(data)

    def compute_stats():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray('myarray')

        stats = ar.GetStatistics(False, force=False)
        assert stats is None

        stats = ar.GetStatistics(False, force=True)
        assert stats is not None
        assert stats.min == 1.0
        assert stats.max == 5.0
        assert stats.mean == 3.0
        assert stats.std_dev == pytest.approx(1.4142135623730951)
        assert stats.valid_count == 5

        view = ar.GetView("[0,0]")
        stats = view.GetStatistics(False, force=False)
        assert stats is None

        stats = view.GetStatistics(False, force=True)
        assert stats is not None
        assert stats.min == 1.0
        assert stats.max == 1.0

    # Check that we can read stats from the .aux.xml
    def reopen():
        assert os.path.exists(tmpfilename + '.aux.xml')

        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray('myarray')

        stats = ar.GetStatistics(False, force=False)
        assert stats is not None
        assert stats.min == 1.0
        assert stats.max == 5.0
        assert stats.mean == 3.0
        assert stats.std_dev == pytest.approx(1.4142135623730951)
        assert stats.valid_count == 5

        view = ar.GetView("[0,0]")
        stats = view.GetStatistics(False, force=False)
        assert stats is not None
        assert stats.min == 1.0
        assert stats.max == 1.0

    def clear_stats():
        assert os.path.exists(tmpfilename + '.aux.xml')

        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ds.ClearStatistics()
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray('myarray')
        stats = ar.GetStatistics(False, force=False)
        assert stats is None
        ds = None

        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray('myarray')
        stats = ar.GetStatistics(False, force=False)
        assert stats is None

    try:
        create()
        compute_stats()
        reopen()
        clear_stats()
    finally:
        drv.Delete(tmpfilename)


def test_netcdf_multidim_advise_read():

    ds = gdal.OpenEx('data/netcdf/byte_no_cf.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    var = rg.OpenMDArray('Band1')
    ref_data_whole = struct.unpack('B' * 20 * 20, var.Read())
    ref_data = struct.unpack('B' * 4 * 5, var.Read(array_start_idx = [2, 3], count = [4, 5]))

    sliced = var[1]
    ref_data_line = struct.unpack('B' * 20, sliced.Read())

    transposed = var.Transpose([1, 0])
    ref_data_transposed = struct.unpack('B' * 20 * 20, transposed.Read())

    # AdviseRead on all
    assert var.AdviseRead() == gdal.CE_None

    # Can use AdviseRead
    got_data = struct.unpack('B' * 4 * 5, var.Read(array_start_idx = [2, 3], count = [4, 5]))
    assert got_data == ref_data

    # AdviseRead on portion
    assert var.AdviseRead(array_start_idx = [2, 3], count = [4, 5]) == gdal.CE_None
    got_data = struct.unpack('B' * 4 * 5, var.Read(array_start_idx = [2, 3], count = [4, 5]))
    assert got_data == ref_data

    # Cannot use AdviseRead as we read outside of it
    got_data = struct.unpack('B' * 20 * 20, var.Read())
    assert got_data == ref_data_whole

    # On a slice
    assert sliced.AdviseRead() == gdal.CE_None
    got_data = struct.unpack('B' * 20, sliced.Read())
    assert got_data == ref_data_line

    # On transposed array
    assert transposed.AdviseRead() == gdal.CE_None
    got_data = struct.unpack('B' * 20 * 20, transposed.Read())
    assert got_data == ref_data_transposed

    with gdaltest.error_handler():
        assert var.AdviseRead(array_start_idx = [2, 3], count = [20, 5]) == gdal.CE_Failure



def test_netcdf_multidim_get_mask():

    tmpfilename = 'tmp/test_netcdf_multidim_get_mask.nc'
    drv = gdal.GetDriverByName('netCDF')
    def create():
        ds = drv.CreateMultiDimensional(tmpfilename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
        dim1 = rg.CreateDimension("dim1", "unspecified type", "unspecified direction", 3)
        bytedt = gdal.ExtendedDataType.Create(gdal.GDT_Byte)
        ar = rg.CreateMDArray("myarray", [dim0, dim1], bytedt)
        ar.SetNoDataValueDouble(6)
        data = struct.pack('B' * 6, 1, 2, 3, 4, 5, 6)
        ar.Write(data)
        attr = ar.CreateAttribute('missing_value', [1], bytedt)
        assert attr.Write(1) == gdal.CE_None

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray('myarray')
        maskdata = struct.unpack('B' * 6, ar.GetMask().Read())
        assert maskdata == (0, 1, 1, 1, 1, 0)

    try:
        create()
        check()
    finally:
        drv.Delete(tmpfilename)


def test_netcdf_multidim_createcopy_array_options():

    src_ds = gdal.OpenEx('data/netcdf/byte_no_cf.nc', gdal.OF_MULTIDIM_RASTER)
    tmpfilename = 'tmp/test_netcdf_multidim_createcopy_array_options.nc'
    with gdaltest.error_handler():
        gdal.GetDriverByName('netCDF').CreateCopy(tmpfilename, src_ds,
            options=['ARRAY:IF(DIM=2):BLOCKSIZE=1,2',
                     'ARRAY:IF(NAME=Band1):COMPRESS=DEFLATE',
                     'ARRAY:ZLEVEL=6'])

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray('Band1')
        assert var.GetBlockSize() == [1, 2]
        assert var.GetStructuralInfo() == { 'COMPRESS': 'DEFLATE' }

    check()

    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_createcopy_array_options_if_name_fullname():

    src_ds = gdal.OpenEx('data/netcdf/byte_no_cf.nc', gdal.OF_MULTIDIM_RASTER)
    tmpfilename = 'tmp/test_netcdf_multidim_createcopy_array_options_if_name_fullname.nc'
    with gdaltest.error_handler():
        gdal.GetDriverByName('netCDF').CreateCopy(tmpfilename, src_ds,
            options=['ARRAY:IF(NAME=/Band1):COMPRESS=DEFLATE'])

    def check():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray('Band1')
        assert var.GetStructuralInfo() == { 'COMPRESS': 'DEFLATE' }

    check()

    gdal.Unlink(tmpfilename)


def test_netcdf_multidim_group_by_same_dimension():

    ds = gdal.OpenEx('data/netcdf/sen3_sral_mwr_fake_standard_measurement.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg.GetMDArrayNames(['GROUP_BY=SAME_DIMENSION']) is None
    groups = rg.GetGroupNames(['GROUP_BY=SAME_DIMENSION'])
    assert set(groups) == set(['time_01', 'time_20_c', 'time_20_ku'])
    g = rg.OpenGroup('time_01', ['GROUP_BY=SAME_DIMENSION'])
    assert g is not None
    arrays = g.GetMDArrayNames()
    for arrayName in arrays:
        ar = g.OpenMDArray(arrayName)
        dims = ar.GetDimensions()
        assert len(dims) == 1
        assert dims[0].GetName() == 'time_01'


def test_netcdf_multidim_getcoordinatevariables():

    ds = gdal.OpenEx('data/netcdf/expanded_form_of_grid_mapping.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('temp')
    coordinate_vars = ar.GetCoordinateVariables()
    assert len(coordinate_vars) == 2
    assert coordinate_vars[0].GetName() == 'lat'
    assert coordinate_vars[1].GetName() == 'lon'

    assert len(coordinate_vars[0].GetCoordinateVariables()) == 0


def test_netcdf_multidim_getresampled_with_geoloc():

    ds = gdal.OpenEx('data/netcdf/sentinel5p_fake.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('my_var')
    coordinate_vars = ar.GetCoordinateVariables()
    assert len(coordinate_vars) == 2
    assert coordinate_vars[0].GetName() == 'longitude'
    assert coordinate_vars[1].GetName() == 'latitude'

    resampled_ar = ar.GetResampled([None] * ar.GetDimensionCount(),
                                   gdal.GRIORA_NearestNeighbour, None)
    assert resampled_ar is not None

    # By default, the classic netCDF driver would use bottom-up reordering,
    # which slightly modifies the output of the geolocation interpolation,
    # and would not make it possible to compare exactly with the GetResampled()
    # result
    with gdaltest.config_option('GDAL_NETCDF_BOTTOMUP', 'NO'):
        warped_ds = gdal.Warp('', 'data/netcdf/sentinel5p_fake.nc', format='MEM')
    assert warped_ds.ReadRaster() == resampled_ar.Read()


def test_netcdf_multidim_cache():

    tmpfilename = 'tmp/test.nc'
    shutil.copy('data/netcdf/alldatatypes.nc', tmpfilename)
    gdal.Unlink(tmpfilename + ".gmac")

    def get_transposed_and_cache():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray('ubyte_y2_x2_var')
        assert ar
        transpose = ar.Transpose([1, 0])
        assert transpose.Cache( ['BLOCKSIZE=2,1'] )
        with gdaltest.error_handler():
            # Cannot cache twice the same array
            assert transpose.Cache() is False

        ar2 = ds.GetRootGroup().OpenMDArray('ubyte_z2_y2_x2_var')
        assert ar2
        assert ar2.Cache()

        return transpose.Read()

    transposed_data = get_transposed_and_cache()

    def check_cache_exists():
        cache_ds = gdal.OpenEx(tmpfilename + ".gmac", gdal.OF_MULTIDIM_RASTER)
        assert cache_ds
        rg = cache_ds.GetRootGroup()
        cached_ar = rg.OpenMDArray('Transposed_view_of__ubyte_y2_x2_var_along__1_0_')
        assert cached_ar
        assert cached_ar.GetBlockSize() == [2, 1]
        assert cached_ar.Read() == transposed_data

        assert rg.OpenMDArray('_ubyte_z2_y2_x2_var') is not None

    check_cache_exists()

    def check_cache_working():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray('ubyte_y2_x2_var')
        transpose = ar.Transpose([1, 0])
        assert transpose.Read() == transposed_data
        # Again
        assert transpose.Read() == transposed_data

    check_cache_working()

    # Now alter the cache directly
    def alter_cache():
        cache_ds = gdal.OpenEx(tmpfilename + ".gmac",
                               gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert cache_ds
        rg = cache_ds.GetRootGroup()
        cached_ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        cached_ar.Write(b'\x00' * len(transposed_data))

    alter_cache()

    # And check we get the altered values
    def check_cache_really_working():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray('ubyte_y2_x2_var')
        transpose = ar.Transpose([1, 0])
        assert transpose.Read() == b'\x00' * len(transposed_data)

    check_cache_really_working()

    gdal.Unlink(tmpfilename)
    gdal.Unlink(tmpfilename + ".gmac")


def test_netcdf_multidim_cache_pamproxydb():

    def remove_dir():
        try:
            os.chmod('tmp/tmpdirreadonly', stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
            shutil.rmtree('tmp/tmpdirreadonly')
        except OSError:
            pass
        try:
            shutil.rmtree('tmp/tmppamproxydir')
        except OSError:
            pass

    # Create a read-only directory
    remove_dir()
    os.mkdir('tmp/tmpdirreadonly')
    os.mkdir('tmp/tmppamproxydir')
    shutil.copy('data/netcdf/byte_no_cf.nc', 'tmp/tmpdirreadonly/test.nc')

    # FIXME: how do we create a read-only dir on windows ?
    # The following has no effect
    os.chmod('tmp/tmpdirreadonly', stat.S_IRUSR | stat.S_IXUSR)

    # Test that the directory is really read-only
    try:
        f = open('tmp/tmpdirreadonly/test', 'w')
        if f is not None:
            f.close()
            remove_dir()
            pytest.skip()
    except IOError:
        pass

    try:
        # This must be run as an external process so we can override GDAL_PAM_PROXY_DIR
        # at the beginning of the process
        import test_py_scripts
        ret = test_py_scripts.run_py_script_as_external_script('.', 'netcdf_multidim_pamproxydb', '-test_netcdf_multidim_cache_pamproxydb')
        assert ret.find('success') != -1, ('netcdf_multidim_pamproxydb.py -test_netcdf_multidim_cache_pamproxydb failed %s' % ret)
    finally:
        remove_dir()


###############################################################################
# Test opening a /vsimem/ file


def test_netcdf_multidim_open_vsimem():

    if gdal.GetDriverByName('netCDF').GetMetadataItem('NETCDF_HAS_NETCDF_MEM') is None:
        pytest.skip('NETCDF_HAS_NETCDF_MEM missing')

    gdal.FileFromMemBuffer('/vsimem/test.nc',
                           open('data/netcdf/trmm.nc', 'rb').read())
    ds = gdal.OpenEx('/vsimem/test.nc', gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    gdal.Unlink('/vsimem/test.nc')
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() is not None


###############################################################################
# Test /vsi access through userfaultfd


def test_netcdf_multidim_open_userfaultfd():

    gdal.Unlink('tmp/test_netcdf_open_userfaultfd.zip')

    f = gdal.VSIFOpenL('/vsizip/tmp/test_netcdf_open_userfaultfd.zip/test.nc', 'wb')
    assert f
    data = open('data/netcdf/byte_no_cf.nc', 'rb').read()
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    # Can only work on Linux, with some kernel versions... not in Docker by default
    # so mostly test that we don't crash
    with gdaltest.error_handler():
        ds = gdal.OpenEx("/vsizip/tmp/test_netcdf_open_userfaultfd.zip/test.nc", gdal.OF_MULTIDIM_RASTER)

    success_expected = False
    if 'CI' not in os.environ:
        if sys.platform.startswith('linux'):
            uname = os.uname()
            version = uname.release.split('.')
            major = int(version[0])
            minor = int(version[1])
            if (major, minor) >= (5, 11):
                assert ds
                success_expected = True

    if ds and not success_expected:
        print('/vsi access through userfaultfd succeeded')

    gdal.Unlink('tmp/test_netcdf_open_userfaultfd.zip')


###############################################################################
# Test opening a char 2D variable


def test_netcdf_multidim_open_char_2d():

    ds = gdal.OpenEx('data/netcdf/char_2d.nc', gdal.OF_MULTIDIM_RASTER)
    ar = ds.GetRootGroup().OpenMDArray('f')
    assert ar
    ar.GetNoDataValueAsRaw()
    ar.GetBlockSize()


###############################################################################
# Test opening a char 2D variable with a dimension of sizer zero


def test_netcdf_multidim_open_char_2d_zero_dim():

    ds = gdal.OpenEx('data/netcdf/char_2d_zero_dim.nc', gdal.OF_MULTIDIM_RASTER)
    ar = ds.GetRootGroup().OpenMDArray('f')
    assert ar
    ar.GetNoDataValueAsRaw()
    ar.GetBlockSize()
