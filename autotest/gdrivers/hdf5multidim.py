#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in HDF5 driver
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

import gdaltest
import pytest
import struct

from osgeo import gdal

pytestmark = pytest.mark.require_driver('HDF5')


def test_hdf5_multidim_basic():

    ds = gdal.OpenEx('data/hdf5/u8be.h5', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert not rg.GetGroupNames()
    assert not rg.OpenGroup('non_existing')
    assert rg.GetMDArrayNames() == [ 'TestArray' ]
    assert not rg.OpenMDArray('non_existing')
    ar = rg.OpenMDArray('TestArray')
    assert ar
    assert not ar.GetAttribute('non_existing')
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetSize() == 6
    assert dims[1].GetSize() == 5
    assert ar.GetDataType().GetNumericDataType() == gdal.GDT_Byte

    got_data = ar.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert len(got_data) == 30 * 2
    assert struct.unpack('H' * 30, got_data) == (0, 1, 2, 3, 4,
                                                 1, 2, 3, 4, 5,
                                                 2, 3, 4, 5, 6,
                                                 3, 4, 5, 6, 7,
                                                 4, 5, 6, 7, 8,
                                                 5, 6, 7, 8, 9)

    got_data = ar.Read( array_start_idx = [2, 1],
                        count = [3, 2],
                        buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert len(got_data) == 6 * 2
    assert struct.unpack('H' * 6, got_data) == (3, 4,
                                                4, 5,
                                                5, 6)


    got_data = ar.Read( array_start_idx = [2, 1],
                        count = [3, 2],
                        array_step = [-1, -1],
                        buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert len(got_data) == 6 * 2
    assert struct.unpack('H' * 6, got_data) == (3, 2,
                                                2, 1,
                                                1, 0)

    with gdaltest.config_option('GDAL_HDF5_TEMP_ARRAY_ALLOC_SIZE', '0'):
        got_data = ar.Read( array_start_idx = [2, 1],
                            count = [3, 2],
                            array_step = [-1, -1],
                            buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
        assert len(got_data) == 6 * 2
        assert struct.unpack('H' * 6, got_data) == (3, 2,
                                                    2, 1,
                                                    1, 0)


def test_hdf5_multidim_var_alldatatypes():

    ds = gdal.OpenEx('HDF5:data/netcdf/alldatatypes.nc', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg

    expected_vars = [ #('char_var', gdal.GDT_Byte, (ord('x'),ord('y'))),
                      ('ubyte_var', gdal.GDT_Byte, (255, 254)),
                      ('byte_var', gdal.GDT_Int16, (-128,-127)),
                      ('byte_unsigned_false_var', gdal.GDT_Int16, (-128,-127)),
                      #('byte_unsigned_true_var', gdal.GDT_Byte, (128, 129)),
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
        assert var.GetDataType().GetClass() == gdal.GEDTC_NUMERIC, var_name
        assert var.GetDataType().GetNumericDataType() == dt, var_name
        if dt == gdal.GDT_Byte:
            assert struct.unpack('B' * len(val), var.Read()) == val
        if dt == gdal.GDT_UInt16:
            assert struct.unpack('H' * len(val), var.Read()) == val
        if dt == gdal.GDT_Int16:
            assert struct.unpack('h' * len(val), var.Read()) == val
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

    assert struct.unpack('i' * 2, var['x'].Read()) == (1, 3)
    assert struct.unpack('h' * 2, var['y'].Read()) == (2, 4)

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


def test_hdf5_multidim_read_array():

    ds = gdal.OpenEx('HDF5:data/netcdf/alldatatypes.nc', gdal.OF_MULTIDIM_RASTER)
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

    # Test reading from slice (slow path)
    data = var.Read(array_start_idx = [3, 0 + (2-1) * 2, 0 + (3-1) * 1], count = [1, 2, 3], array_step = [1, -2, -1])
    data_from_slice = var[3].Read(array_start_idx = [0 + (2-1) * 2, 0 + (3-1) * 1], count = [2, 3], array_step = [-2, -1])
    assert data_from_slice == data

    # 4D
    var = rg.OpenMDArray('ubyte_t2_z2_y2_x2_var')
    data = var.Read(count = [2, 3, 2, 3], array_step = [1, 1, 2, 1])
    got_data_ref = (1, 2, 3, 9, 10, 11, 2, 3, 4, 10, 11, 12, 3, 4, 5, 11, 12, 1, 2, 3, 4, 10, 11, 12, 3, 4, 5, 11, 12, 1, 4, 5, 6, 12, 1, 2)
    assert struct.unpack('B' * len(data), data) == got_data_ref

    data = var.Read(count = [2, 3, 2, 3], array_step = [1, 1, 2, 1], buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert struct.unpack('H' * (len(data) // 2), data) ==  got_data_ref


def test_hdf5_multidim_attr_alldatatypes():

    ds = gdal.OpenEx('HDF5:data/netcdf/alldatatypes.nc', gdal.OF_MULTIDIM_RASTER)
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
    assert struct.unpack('H'* 2, map_attrs['attr_complex_int16'].ReadAsRaw()) == (1, 2)
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


def test_hdf5_multidim_nodata_unit():

    ds = gdal.OpenEx('HDF5:data/netcdf/trmm-nc4.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('pcp')
    assert struct.unpack('f', ar.GetNoDataValueAsRaw())[0] == struct.unpack('f', struct.pack('f', -9999.9))[0]
    assert ar.GetUnit() == ''

    ar = rg.OpenMDArray('longitude')
    assert ar.GetNoDataValueAsRaw() is None
    assert ar.GetUnit() == 'degrees_east'


def test_hdf5_multidim_recursive_groups():

    # File generated with
    # import h5py
    # f = h5py.File('hdf5/recursive_groups.h5','w')
    # group = f.create_group("subgroup")
    # group['link_to_root'] = f
    # group['link_to_self'] = group
    # group['soft_link_to_root'] = h5py.SoftLink('/')
    # group['soft_link_to_self'] = h5py.SoftLink('/subgroup')
    # group['soft_link_to_not_existing'] = h5py.SoftLink('/not_existing')
    # group['hard_link_to_root'] = h5py.HardLink('/')
    # group['ext_link_to_self_root'] = h5py.ExternalLink("hdf5/recursive_groups.h5", "/")
    # f.close()

    ds = gdal.OpenEx('data/hdf5/recursive_groups.h5', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ['subgroup']


def test_hdf5_netcdf_dimensions():

    ds = gdal.OpenEx('HDF5:data/netcdf/trmm-nc4.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()

    assert rg.GetAttribute('CDI')

    dims = rg.GetDimensions()
    assert len(dims) == 3

    assert dims[0].GetFullName() == '/latitude'
    assert dims[0].GetName() == 'latitude'
    assert dims[0].GetSize() == 40
    var = dims[0].GetIndexingVariable()
    assert var
    assert var.GetName() == 'latitude'

    assert dims[1].GetFullName() == '/longitude'
    assert dims[1].GetName() == 'longitude'
    assert dims[1].GetSize() == 40
    var = dims[1].GetIndexingVariable()
    assert var

    assert dims[2].GetFullName() == '/time'
    assert dims[2].GetName() == 'time'
    assert dims[2].GetSize() == 1
    var = dims[2].GetIndexingVariable()
    assert var

    ar = rg.OpenMDArray('latitude')
    dims = ar.GetDimensions()
    assert len(dims) == 1
    assert dims[0].GetName() == 'latitude'
    assert dims[0].GetIndexingVariable()
    assert dims[0].GetIndexingVariable().GetName() == 'latitude'

    ar = rg.OpenMDArray('pcp')
    dims = ar.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetName() == 'time'
    assert dims[0].GetIndexingVariable()
    assert dims[0].GetIndexingVariable().GetName() == 'time'


def test_hdf5_multidim_netcdf_dimensions_complex_case():

    ds = gdal.OpenEx('HDF5:data/netcdf/alldatatypes.nc', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    dims = rg.GetDimensions()
    assert len(dims) == 6

    dim = next((x for x in dims if x.GetName() == 'X'), None)
    assert dim
    assert dim.GetFullName() == '/X'
    assert dim.GetSize() == 2
    assert dim.GetIndexingVariable()
    assert dim.GetIndexingVariable().GetFullName() == '/X'

    dim = next((x for x in dims if x.GetName() == 'Y'), None)
    assert dim
    assert dim.GetFullName() == '/Y'
    assert dim.GetSize() == 1
    assert dim.GetIndexingVariable()
    assert dim.GetIndexingVariable().GetFullName() == '/Y'

    dim = next((x for x in dims if x.GetName() == 'X2'), None)
    assert dim
    assert dim.GetFullName() == '/X2'
    assert dim.GetSize() == 4
    assert not dim.GetIndexingVariable()

    assert 'X' in rg.GetMDArrayNames()
    assert 'X2' not in rg.GetMDArrayNames()

    subgroup = rg.OpenGroup('group')
    assert subgroup
    dims = subgroup.GetDimensions()
    assert len(dims) == 2

    dim = next((x for x in dims if x.GetName() == 'X'), None)
    assert dim
    assert dim.GetFullName() == '/group/X'
    assert dim.GetSize() == 3
    assert not dim.GetIndexingVariable()

    dim = next((x for x in dims if x.GetName() == 'Y'), None)
    assert dim
    assert dim.GetFullName() == '/group/Y'
    assert dim.GetSize() == 2
    assert not dim.GetIndexingVariable()

    ar = subgroup.OpenMDArray('char_var')
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetFullName() == '/Y'
    assert dims[0].GetSize() == 1
    assert dims[0].GetIndexingVariable()
    assert dims[1].GetFullName() == '/group/Y'
    assert dims[1].GetSize() == 2
    assert not dims[1].GetIndexingVariable()
    assert dims[2].GetFullName() == '/group/X'
    assert dims[2].GetSize() == 3
    assert not dims[2].GetIndexingVariable()

    ar = rg.OpenMDArray('ubyte_z2_y2_x2_var')
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetFullName() == '/Z2'
    assert not dims[0].GetIndexingVariable()


def test_hdf5_multidim_dimension_labels_with_null():

    ds = gdal.OpenEx('data/hdf5/dimension_labels_with_null.h5', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArray('data')
    attr = ar.GetAttribute('DIMENSION_LABELS')
    assert attr.ReadAsStringArray() == ['', '', 'x']

    ds = gdal.OpenEx('data/hdf5/dimension_labels_with_null.h5', gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray('data')
    dims = ar.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetName() == 'dim0'
    assert dims[1].GetName() == 'dim1'
    assert dims[2].GetName() == 'x'


def test_hdf5_multidim_family_driver():

    assert gdal.OpenEx('data/hdf5/test_family_0.h5', gdal.OF_MULTIDIM_RASTER)
