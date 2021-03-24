#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in MEM driver
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

import array
import gdaltest
import math
import pytest
import struct


def test_mem_md_basic():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    assert ds
    assert ds.GetDescription() == 'myds'

    rg = ds.GetRootGroup()
    assert rg
    assert rg.GetName() == '/'
    assert rg.GetFullName() == '/'
    assert not rg.GetMDArrayNames()
    assert not rg.GetGroupNames()
    assert not rg.GetAttributes()
    assert not rg.GetDimensions()
    assert not rg.OpenMDArray("not existing")
    assert not rg.OpenMDArrayFromFullname("not existing")
    assert not rg.OpenGroup("not existing")
    assert not rg.OpenGroupFromFullname("not existing")
    assert not rg.GetAttribute("not existing")


def test_mem_md_subgroup():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()

    with gdaltest.error_handler():
        assert not rg.CreateGroup('') # unnamed group not supported
    with pytest.raises(ValueError):
        assert not rg.CreateGroup(None)

    subg = rg.CreateGroup('subgroup')
    assert subg
    assert subg.GetName() == 'subgroup'
    assert subg.GetFullName() == '/subgroup'
    assert rg.GetGroupNames() == [ 'subgroup' ]
    assert rg.OpenGroup('subgroup').GetName() == 'subgroup'

    subsubg = subg.CreateGroup('subsubgroup')
    assert subsubg.GetFullName() == '/subgroup/subsubgroup'

    subsubg = rg.OpenGroupFromFullname('/subgroup/subsubgroup')
    assert subsubg is not None
    assert subsubg.GetFullName() == '/subgroup/subsubgroup'

    subg.CreateMDArray("myarray", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    array = rg.OpenMDArrayFromFullname('/subgroup/myarray')
    assert array is not None
    assert array.GetFullName() == '/subgroup/myarray'

    copy_ds = drv.CreateCopy('', ds)
    assert copy_ds
    copy_rg = copy_ds.GetRootGroup()
    assert copy_rg
    assert copy_rg.GetGroupNames() == [ 'subgroup' ]


def test_mem_md_array_unnamed_array():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    edt =  gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    with gdaltest.error_handler():
        assert not rg.CreateMDArray("", [], edt)


def test_mem_md_array_duplicated_array_name():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    assert rg.CreateMDArray("same_name", [],
                            gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    with gdaltest.error_handler():
        assert not rg.CreateMDArray("same_name", [],
                                    gdal.ExtendedDataType.Create(gdal.GDT_Byte))


def test_mem_md_array_nodim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    myarray = rg.CreateMDArray("myarray", [],
                               gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    assert myarray
    assert myarray.GetName() == 'myarray'
    assert myarray.GetFullName() == '/myarray'
    assert rg.GetMDArrayNames() == ['myarray']
    assert rg.OpenMDArray('myarray')
    assert myarray.GetDimensionCount() == 0
    assert myarray.GetTotalElementsCount() == 1
    assert not myarray.GetDimensions()
    assert myarray.shape is None
    assert myarray.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert myarray.GetDataType().GetNumericDataType() == gdal.GDT_UInt16
    got_data = myarray.Read()
    assert len(got_data) == 2
    assert struct.unpack('H', got_data) == (0, )
    assert myarray.Write(struct.pack('H', 65535)) == gdal.CE_None
    got_data = myarray.Read()
    assert len(got_data) == 2
    assert struct.unpack('H', got_data) == (65535, )

    assert myarray.AdviseRead() == gdal.CE_None

    copy_ds = drv.CreateCopy('', ds)
    assert copy_ds
    copy_rg = copy_ds.GetRootGroup()
    assert copy_rg
    copy_myarray = copy_rg.OpenMDArray('myarray')
    assert copy_myarray
    assert copy_myarray.Read() == got_data


def test_mem_md_array_single_dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
    dims = rg.GetDimensions()
    assert len(dims) == 1
    myarray = rg.CreateMDArray("myarray", [ dim ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray
    assert myarray.GetName() == 'myarray'
    assert rg.GetMDArrayNames() == ['myarray']
    assert rg.OpenMDArray('myarray')
    assert myarray.GetDimensionCount() == 1
    assert myarray.GetTotalElementsCount() == 2
    got_dims = myarray.GetDimensions()
    assert len(got_dims) == 1
    assert myarray.shape == (2, )
    assert got_dims[0].GetName() == 'dim0'
    assert got_dims[0].GetType() == 'unspecified type'
    assert got_dims[0].GetDirection() == 'unspecified direction'
    assert got_dims[0].GetSize() == 2
    got_data = myarray.Read()
    assert len(got_data) == 2
    assert struct.unpack('B' * 2, got_data) == (0, 0)

    assert myarray.AdviseRead() == gdal.CE_None

    attr = myarray.CreateAttribute('attr', [],
                                   gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert attr
    assert attr.GetFullName() == '/myarray/attr'

    assert myarray.GetNoDataValueAsDouble() is None
    assert myarray.SetNoDataValueDouble(1) == gdal.CE_None
    assert myarray.GetNoDataValueAsDouble() == 1
    assert myarray.SetNoDataValueRaw(struct.pack('B', 127)) == gdal.CE_None
    with gdaltest.error_handler():
        assert myarray.SetNoDataValueRaw(struct.pack('h', 127)) != gdal.CE_None
    assert struct.unpack('B', myarray.GetNoDataValueAsRaw()) == (127,)

    assert myarray.GetScale() is None
    assert myarray.GetOffset() is None
    assert myarray.GetScaleStorageType() == gdal.GDT_Unknown
    assert myarray.GetOffsetStorageType() == gdal.GDT_Unknown

    assert myarray.SetScale(2.5) == gdal.CE_None
    assert myarray.GetScale() == 2.5
    assert myarray.GetScaleStorageType() == gdal.GDT_Unknown
    assert myarray.SetScale(2.5, storageType = gdal.GDT_Float32) == gdal.CE_None
    assert myarray.GetScaleStorageType() == gdal.GDT_Float32

    assert myarray.SetOffset(1.5) == gdal.CE_None
    assert myarray.GetOffset() == 1.5
    assert myarray.GetOffsetStorageType() == gdal.GDT_Unknown
    assert myarray.SetOffset(1.5, storageType = gdal.GDT_Float32) == gdal.CE_None
    assert myarray.GetOffsetStorageType() == gdal.GDT_Float32

    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    tab = [ 0 ]
    copy_ds = drv.CreateCopy('', ds, callback = my_cbk,
                             callback_data = tab)
    assert tab[0] == 1
    assert copy_ds
    copy_rg = copy_ds.GetRootGroup()
    assert copy_rg
    copy_myarray = copy_rg.OpenMDArray('myarray')
    assert copy_myarray
    assert copy_myarray.Read() == got_data
    assert copy_myarray.GetNoDataValueAsRaw() == myarray.GetNoDataValueAsRaw()

    assert myarray.DeleteNoDataValue() == gdal.CE_None
    assert myarray.GetNoDataValueAsDouble() is None

    assert myarray.SetUnit('foo') == gdal.CE_None
    assert myarray.GetUnit() == 'foo'



def test_mem_md_array_string():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
    var = rg.CreateMDArray('var', [dim], gdal.ExtendedDataType.CreateString())
    assert var
    assert var.Write(['', '0123456789']) == gdal.CE_None
    var = rg.OpenMDArray('var')
    assert var
    assert var.Read() == ['', '0123456789']


def test_mem_md_datatypes():

    dt_byte = gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    assert dt_byte.GetClass() == gdal.GEDTC_NUMERIC
    assert dt_byte.GetName() == ''
    assert dt_byte.GetNumericDataType() == gdal.GDT_Byte
    assert dt_byte.GetSize() == 1
    assert dt_byte.CanConvertTo(dt_byte)
    with pytest.raises(ValueError):
        assert dt_byte.CanConvertTo(None)
    assert dt_byte == gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    assert not dt_byte != gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    assert dt_byte.Equals(dt_byte)
    with pytest.raises(ValueError):
        assert dt_byte.Equals(None)
    assert not dt_byte.GetComponents()

    dt_cint32 = gdal.ExtendedDataType.Create(gdal.GDT_CInt32)
    assert dt_cint32.GetClass() == gdal.GEDTC_NUMERIC
    assert dt_cint32.GetName() == ''
    assert dt_cint32.GetNumericDataType() == gdal.GDT_CInt32
    assert dt_cint32.GetSize() == 2 * 4
    assert dt_cint32.CanConvertTo(dt_cint32)
    assert dt_cint32.Equals(dt_cint32)
    assert dt_cint32.CanConvertTo(dt_byte)
    assert not dt_cint32.Equals(dt_byte)
    assert not dt_cint32.GetComponents()

    dt_string = gdal.ExtendedDataType.CreateString()
    assert dt_string.GetClass() == gdal.GEDTC_STRING
    assert dt_string.GetName() == ''
    assert dt_string.GetNumericDataType() == gdal.GDT_Unknown
    assert dt_string.GetSize() in (4, 8) # depends on 32 vs 64 build
    assert dt_string.GetMaxStringLength() == 0
    assert dt_string.CanConvertTo(dt_string)
    assert dt_string.Equals(dt_string)
    assert not dt_string.Equals(dt_byte)
    assert not dt_byte.Equals(dt_string)
    assert not dt_string.GetComponents()

    dt_string_limited_size = gdal.ExtendedDataType.CreateString(10)
    assert dt_string_limited_size.GetMaxStringLength() == 10

    comp0 = gdal.EDTComponent.Create('x', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    comp1 = gdal.EDTComponent.Create('y', 4, gdal.ExtendedDataType.Create(gdal.GDT_Int32))

    with gdaltest.error_handler():
        assert gdal.ExtendedDataType.CreateCompound("mytype", 8, []) is None
        assert gdal.ExtendedDataType.CreateCompound("mytype", 2000 * 1000 * 1000, [comp0]) is None

    compound_dt = gdal.ExtendedDataType.CreateCompound("mytype", 8, [comp0, comp1])
    assert compound_dt.GetClass() == gdal.GEDTC_COMPOUND
    assert compound_dt.GetName() == 'mytype'
    assert compound_dt.GetNumericDataType() == gdal.GDT_Unknown
    assert compound_dt.GetSize() == 8
    comps = compound_dt.GetComponents()
    assert len(comps) == 2
    assert comps[0].GetName() == 'x'
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Int16
    assert comps[1].GetName() == 'y'
    assert comps[1].GetOffset() == 4
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_Int32
    assert compound_dt.CanConvertTo(compound_dt)
    assert compound_dt.Equals(compound_dt)
    assert not compound_dt.Equals(dt_byte)
    assert not dt_byte.Equals(compound_dt)

    with gdaltest.error_handler():
        # Too short size
        assert not gdal.ExtendedDataType.CreateCompound("mytype", 7, [comp0, comp1])

        # Too big size
        assert not gdal.ExtendedDataType.CreateCompound("mytype", 1 << 30, [comp0, comp1])

        # Wrongly ordered
        assert not gdal.ExtendedDataType.CreateCompound("mytype", 8, [comp1, comp0])

        # Empty
        assert not gdal.ExtendedDataType.CreateCompound("mytype", 0, [])

    other_compound_dt = gdal.ExtendedDataType.CreateCompound("mytype", 8, [comp0])
    assert not compound_dt.Equals(other_compound_dt)
    assert not other_compound_dt.Equals(compound_dt)
    assert compound_dt.CanConvertTo(other_compound_dt)
    assert not other_compound_dt.CanConvertTo(compound_dt)


def test_mem_md_array_compoundtype():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim = rg.CreateDimension("dim0", None, None, 2)
    comp0 = gdal.EDTComponent.Create('x', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    comp1 = gdal.EDTComponent.Create('y', 4, gdal.ExtendedDataType.Create(gdal.GDT_Int32))
    dt = gdal.ExtendedDataType.CreateCompound("mytype", 8, [comp0, comp1])
    myarray = rg.CreateMDArray("myarray", [ dim ], dt)
    assert myarray
    dt = myarray.GetDataType()
    assert dt.GetName() == 'mytype'
    assert dt.GetClass() == gdal.GEDTC_COMPOUND
    assert dt.GetSize() == 8
    comps = dt.GetComponents()
    assert len(comps) == 2
    assert comps[0].GetName() == 'x'
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Int16
    assert comps[1].GetName() == 'y'
    assert comps[1].GetOffset() == 4
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_Int32

    assert myarray.Write(struct.pack('hi' * 2, 32767, 1000000, -32768, -1000000)) == gdal.CE_None
    assert struct.unpack('hi' * 2, myarray.Read()) == (32767, 1000000, -32768, -1000000)

    extract_compound_dt = gdal.ExtendedDataType.CreateCompound("mytype", 4,
        [gdal.EDTComponent.Create('y', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int32))])
    got_data = myarray.Read(buffer_datatype = extract_compound_dt)
    assert len(got_data) == 8
    assert struct.unpack('i' * 2, got_data) == (1000000, -1000000)

    with gdaltest.error_handler():
        assert not myarray.GetView('["z')
        assert not myarray.GetView('["z"')
        assert not myarray.GetView('["z"]')
    y_ar = myarray["y"]
    assert y_ar
    assert y_ar.GetDimensionCount() == 1
    assert y_ar.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert y_ar.GetDataType().GetNumericDataType() == gdal.GDT_Int32
    assert y_ar.GetBlockSize() == myarray.GetBlockSize()

    assert y_ar.GetSpatialRef() == myarray.GetSpatialRef()
    assert y_ar.GetNoDataValueAsRaw() == myarray.GetNoDataValueAsRaw()
    assert y_ar.GetUnit() == myarray.GetUnit()
    assert y_ar.GetScale() == myarray.GetScale()
    assert y_ar.GetOffset() == myarray.GetOffset()

    assert myarray.SetUnit("foo") == gdal.CE_None
    assert myarray.SetScale(1) == gdal.CE_None
    assert myarray.SetOffset(2) == gdal.CE_None
    assert myarray.SetNoDataValueRaw(struct.pack('hi', 32767, 1000000)) == gdal.CE_None
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    assert myarray.SetSpatialRef(sr) == gdal.CE_None

    assert y_ar.GetUnit() == myarray.GetUnit()
    assert y_ar.GetScale() == myarray.GetScale()
    assert y_ar.GetOffset() == myarray.GetOffset()
    assert y_ar.GetNoDataValueAsRaw() == struct.pack('i', 1000000)
    assert y_ar.GetSpatialRef().IsSame(myarray.GetSpatialRef())

    got_data = y_ar.Read()
    assert len(got_data) == 8
    assert struct.unpack('i' * 2, got_data) == (1000000, -1000000)

    with gdaltest.error_handler():
        assert not y_ar.GetView('["y"]')

    assert y_ar.AdviseRead() == gdal.CE_None

    y_ar = myarray["y"][1]
    got_data = y_ar.Read()
    assert len(got_data) == 4
    assert struct.unpack('i' * 1, got_data) == (-1000000,)

    y_ar = myarray[1]["y"]
    got_data = y_ar.Read()
    assert len(got_data) == 4
    assert struct.unpack('i' * 1, got_data) == (-1000000,)

def test_mem_md_array_3_dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 2)
    dim1 = rg.CreateDimension("dim1", None, None, 3)
    dim2 = rg.CreateDimension("dim2", None, None, 4)
    myarray = rg.CreateMDArray("myarray", [ dim0, dim1, dim2 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray
    assert myarray.GetName() == 'myarray'
    assert rg.GetMDArrayNames() == ['myarray']
    assert rg.OpenMDArray('myarray')
    assert myarray.GetDimensionCount() == 3
    assert myarray.GetTotalElementsCount() == 24
    assert myarray.shape == (2, 3, 4)

    assert not myarray.GetSpatialRef()
    assert myarray.SetSpatialRef(None) == gdal.CE_None
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    assert myarray.SetSpatialRef(sr) == gdal.CE_None
    assert myarray.GetSpatialRef() is not None

    data = array.array('B', list(range(24))).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    got_data = myarray.Read()
    assert got_data == data

    got_data = myarray.Read([0,0,0],[2,3,4],[1,1,1],[12,4,1],
                            gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert got_data == data

    data = myarray.Read(array_start_idx = [1, 0, 2], count = [1, 2, 2], array_step = [1, 2, 1])
    assert struct.unpack('B' * len(data), data) == (14, 15, 22, 23)

    data = myarray.Read(array_start_idx = [1, 0, 3], count = [1, 2, 2], array_step = [1, 2, -1])
    assert struct.unpack('B' * len(data), data) == (15, 14, 23, 22)

    data = struct.pack('d' * 1, 25.0)
    float64dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    assert myarray.Write(data, [1,2,3],[1,1,1], buffer_datatype = float64dt) == gdal.CE_None

    got_data = myarray.Read([1,2,3],[1,1,1], buffer_datatype = float64dt)
    got_data = struct.unpack('d' * 1, got_data)[0]
    assert got_data == 25.0


def test_mem_md_array_4_dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 2)
    dim1 = rg.CreateDimension("dim1", None, None, 3)
    dim2 = rg.CreateDimension("dim2", None, None, 4)
    dim3 = rg.CreateDimension("dim3", None, None, 5)
    myarray = rg.CreateMDArray("myarray", [ dim0, dim1, dim2, dim3 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    assert myarray
    assert myarray.GetName() == 'myarray'
    assert rg.GetMDArrayNames() == ['myarray']
    assert rg.OpenMDArray('myarray')
    assert myarray.GetDimensionCount() == 4
    assert myarray.GetTotalElementsCount() == 2 * 3 * 4 * 5

    data = array.array('h', [-i for i in range(2 * 3 * 4 * 5)]).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    got_data = myarray.Read()
    assert got_data == data

    data = myarray.Read(array_start_idx = [1, 0, 2, 3], count = [1, 2, 2, 2], array_step = [1, 2, 1, 1])
    assert struct.unpack('h' * (len(data)//2), data) == (-73, -74, -78, -79, -113, -114, -118, -119)


def test_mem_md_copy_array():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 20)
    dim1 = rg.CreateDimension("dim1", None, None, 30)
    dim2 = rg.CreateDimension("dim2", None, None, 10)
    dim3 = rg.CreateDimension("dim3", None, None, 51)
    myarray = rg.CreateMDArray("myarray", [ dim0, dim1, dim2, dim3 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_UInt32))

    data = array.array('I',
                       list(range(myarray.GetTotalElementsCount()))).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    def my_cbk(pct, _, arg):
        assert pct > tab[0]
        tab[0] = pct
        return 1

    tab = [ 0 ]
    with gdaltest.config_option('GDAL_SWATH_SIZE', str(100* 1000)):
        copy_ds = drv.CreateCopy('', ds, callback = my_cbk,
                                callback_data = tab)
    assert tab[0] == 1
    assert copy_ds
    copy_rg = copy_ds.GetRootGroup()
    assert copy_rg
    copy_myarray = copy_rg.OpenMDArray('myarray')
    assert copy_myarray
    assert copy_myarray.Read() == data


def test_mem_md_array_read_write_errors():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 2)
    dim1 = rg.CreateDimension("dim1", None, None, 3)
    dim2 = rg.CreateDimension("dim2", None, None, 4)
    myarray = rg.CreateMDArray("myarray", [ dim0, dim1, dim2 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray

    assert myarray.Read([0,0,0],[1,1,1],[1,1,1],[0,0,0])
    with gdaltest.error_handler():

        # Invalid number of values in array_idx array
        assert not myarray.Read([0,0],[1,1,1],[1,1,1],[0,0,0])

        # Invalid number of values in count array
        assert not myarray.Read([0,0,0],[1,1],[1,1,1],[0,0,0])

        # Invalid number of values in step array
        assert not myarray.Read([0,0,0],[1,1,1],[1,1],[0,0,0])

        # Invalid number of values in buffer_stride array
        assert not myarray.Read([0,0,0],[1,1,1],[1,1,1],[0,0])

        # Invalid start_index[0]
        assert not myarray.Read([-1,0,0],[1,1,1],[1,1,1],[0,0,0])

        # Invalid start_index[0]
        assert not myarray.Read([2,0,0],[1,1,1],[1,1,1],[0,0,0])

        # Invalid count[0]
        assert not myarray.Read([0,0,0],[-1,1,1],[1,1,1],[0,0,0])

        # Invalid count[0]
        assert not myarray.Read([0,0,0],[0,1,1],[1,1,1],[0,0,0])

        # Invalid count[0]
        assert not myarray.Read([0,0,0],[3,1,1],[1,1,1],[0,0,0])

        # start_idx[0] + (count[0]-1) * step[0] >= dim[0].size
        assert not myarray.Read([0,0,0],[2,1,1],[2,1,1],[0,0,0])
        assert not myarray.Read([0,0,0],[2<<50,1,1],[2<<50,1,1],[0,0,0])

        # Overflow with step[0]
        assert not myarray.Read([0,0,0],[2,1,1],[-(1<<63),1,1],[0,0,0])

        # start_idx[0] + (count[0]-1) * step[0] < 0
        assert not myarray.Read([0,0,0],[2,1,1],[-1,1,1],[0,0,0])
        assert not myarray.Read([0,0,0],[2,1,1],[-2<<50,1,1],[0,0,0])

        # Too big stride
        assert not myarray.Read([1,0,0],[2,1,1],[1,1,1],[(1 << 63)-1,0,0])

        # Negative stride not supported in SWIG bindings
        assert not myarray.Read([0,0,0],[1,1,1],[1,1,1],[-1,0,0])

    data = struct.pack('d' * 1, 25.0)
    float64dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    with gdaltest.error_handler():
        assert myarray.Write('', [1,2,3],[1,1,1]) == gdal.CE_Failure
        assert myarray.Write(data[0:7], [1,2,3],[1,1,1], buffer_datatype = float64dt) == gdal.CE_Failure


def test_mem_md_invalid_dims():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    assert rg.CreateDimension("dim1", None, None, 1)
    with gdaltest.error_handler():
        # empty name
        assert not rg.CreateDimension("", None, None, 1)
        # existing dim
        assert not rg.CreateDimension("dim1", None, None, 1)


def test_mem_md_array_invalid_args():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    edt = gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    dim = rg.CreateDimension("dim0", None, None, 1)
    with pytest.raises(TypeError):
        rg.CreateMDArray("myarray", None, edt)
    with pytest.raises((TypeError, SystemError)):
        rg.CreateMDArray("myarray", [None], edt)
    with pytest.raises((TypeError, SystemError)):
        rg.CreateMDArray("myarray", [1], edt)
    with pytest.raises(ValueError):
        rg.CreateMDArray("myarray", [dim], None)
    with pytest.raises(ValueError):
        rg.CreateMDArray(None, [dim], edt)


def test_mem_md_array_too_large():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim = rg.CreateDimension("dim0", None, None, (1 << 64)-1)
    with gdaltest.error_handler():
        assert not rg.CreateMDArray("myarray", [dim],
                                     gdal.ExtendedDataType.Create(gdal.GDT_Byte))


def test_mem_md_array_too_large_overflow_dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 1 << 25)
    dim1 = rg.CreateDimension("dim1", None, None, 1 << 25)
    dim2 = rg.CreateDimension("dim2", None, None, 1 << 25)
    with gdaltest.error_handler():
        assert not rg.CreateMDArray("myarray", [dim0, dim1, dim2],
                                     gdal.ExtendedDataType.Create(gdal.GDT_Byte))


def test_mem_md_array_30dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dims = []
    for i in range(30):
        dim = rg.CreateDimension("dim%d" % i, None, None, 1 + (i % 2))
        dims.append(dim)
    myarray = rg.CreateMDArray("myarray", dims,
                                     gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray
    assert myarray.GetDimensionCount() == 30
    assert myarray.GetTotalElementsCount() == 2**15
    got_data = myarray.Read()
    assert len(got_data) == 2**15


def test_mem_md_array_32dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dims = []
    for i in range(32):
        dim = rg.CreateDimension("dim%d" % i, None, None, 1 + (i % 2))
        dims.append(dim)
    myarray = rg.CreateMDArray("myarray", dims,
                                     gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert len(myarray.Read()) == 2**16


def test_mem_md_group_attribute_single_numeric():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()

    float64dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    with gdaltest.error_handler():
        assert not rg.CreateAttribute('', [1], float64dt) # unnamed attr not supported
    with pytest.raises(ValueError):
        assert not rg.CreateAttribute(None, [1], float64dt)

    attr = rg.CreateAttribute('attr', [1], float64dt)
    assert attr
    assert attr.GetName() == 'attr'
    assert rg.GetAttribute('attr').GetName() == 'attr'
    assert rg.GetAttributes()[0].GetName() == 'attr'
    assert attr.GetDimensionsSize() == [1]
    assert attr.GetTotalElementsCount() == 1
    assert attr.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert attr.Read() == 0.0
    assert attr.ReadAsString() == '0'
    assert attr.Write(1) == gdal.CE_None
    assert attr.Read() == 1
    assert attr.Write(1.25) == gdal.CE_None
    assert attr.Read() == 1.25
    assert attr.ReadAsDouble() == 1.25
    assert attr.ReadAsDoubleArray() == (1.25,)
    assert attr.ReadAsString() == '1.25'
    assert attr.ReadAsStringArray() == ['1.25']
    assert attr.Write([2]) == gdal.CE_None
    assert attr.Read() == 2
    assert attr.Write([2.25]) == gdal.CE_None
    assert attr.Read() == 2.25
    with gdaltest.error_handler():
        assert attr.Write([]) != gdal.CE_None
        assert attr.Write([1, 2]) != gdal.CE_None


def test_mem_md_group_attribute_multiple_numeric():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()

    float64dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    attr = rg.CreateAttribute('attr', [2,3], float64dt)
    assert attr
    assert attr.GetFullName() == '/_GLOBAL_/attr'
    assert attr.GetDimensionsSize() == [2,3]
    assert attr.GetTotalElementsCount() == 6
    assert attr.Read() == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    assert attr.Write([1.1,2,3,4,5,6]) == gdal.CE_None
    assert attr.Read() == (1.1,2,3,4,5,6)
    assert attr.ReadAsIntArray() == (1,2,3,4,5,6)
    assert attr.ReadAsInt() == 1
    assert attr.ReadAsDouble() == 1.1

    subg = rg.CreateGroup('subgroup')
    attr = subg.CreateAttribute('attr', [2,3], float64dt)
    assert attr
    assert attr.GetFullName() == '/subgroup/_GLOBAL_/attr'


def test_mem_md_group_attribute_single_string():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()

    attr = rg.CreateAttribute('attr', [1], gdal.ExtendedDataType.CreateString())
    assert attr
    assert attr.Read() is None
    assert attr.ReadAsStringArray() == ['']
    assert attr.Write('foo') == gdal.CE_None
    assert attr.Read() == 'foo'
    assert attr.ReadAsStringArray() == ['foo']
    assert attr.Write(['bar']) == gdal.CE_None
    assert attr.Read() == 'bar'


def test_mem_md_group_attribute_multiple_string():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()

    attr = rg.CreateAttribute('attr', [2,3], gdal.ExtendedDataType.CreateString())
    assert attr
    assert attr.GetDimensionsSize() == [2,3]
    assert attr.GetTotalElementsCount() == 6
    assert attr.Read() == ['', '', '', '', '', '']
    assert attr.Write(['foo','bar','baz','FOO','BAR','BAZ']) == gdal.CE_None
    assert attr.Read() == ['foo','bar','baz','FOO','BAR','BAZ']


def test_mem_md_array_attribute():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    myarray = rg.CreateMDArray("myarray", [],
                                     gdal.ExtendedDataType.Create(gdal.GDT_Byte))

    float64dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    with gdaltest.error_handler():
        assert not myarray.CreateAttribute('', [1], float64dt) # unnamed attr not supported
    with pytest.raises(ValueError):
        assert not myarray.CreateAttribute(None, [1], float64dt)

    attr = myarray.CreateAttribute('attr', [1], float64dt)
    assert attr
    assert attr.GetName() == 'attr'
    assert myarray.GetAttribute('attr').GetName() == 'attr'
    assert myarray.GetAttributes()[0].GetName() == 'attr'
    assert not myarray.GetAttribute("not existing")


def test_mem_md_array_slice():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim_2 = rg.CreateDimension("dim_2", None, None, 2)
    dim_3 = rg.CreateDimension("dim_3", None, None, 3)
    dim_4 = rg.CreateDimension("dim_4", None, None, 4)

    ar = rg.CreateMDArray("nodim", [],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert ar.Write(struct.pack('B', 1)) == gdal.CE_None
    with gdaltest.error_handler():
        assert not ar[:]

    ar = rg.CreateMDArray("array", [dim_2, dim_3, dim_4],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    data = array.array('B', list(range(24))).tobytes()
    assert ar.Write(data) == gdal.CE_None

    with pytest.raises(Exception):
        ar.GetView(None)

    attr = ar.CreateAttribute('attr', [],
                              gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    assert attr.Write(1) == gdal.CE_None

    with gdaltest.error_handler():
        assert not ar.GetView("")
        assert not ar.GetView("x")
        assert not ar.GetView("[")
        assert not ar.GetView("[]")
        assert not ar.GetView("[foo]")
        assert not ar.GetView("[1,2,3,4]")
        assert not ar.GetView("[1,2,3,:]")
        assert not ar.GetView("[1:2:3:4]")
        assert not ar.GetView("[...,...]")
        assert not ar.GetView("[...,0,...]")
        assert not ar.GetView("[-3]")
        assert not ar.GetView("[2]")
        assert not ar.GetView('[::0]')
        assert not ar.GetView('[0:0:1]')
        assert not ar.GetView('[1:1:1]')
        assert not ar.GetView('[1:1:-1]')
        assert not ar.GetView('[0,0,0][:]')

    sliced_ar = ar[1]
    assert sliced_ar
    assert sliced_ar.GetAttribute('attr') is not None
    assert len(sliced_ar.GetAttributes()) == 1
    assert sliced_ar.GetBlockSize() == ar.GetBlockSize()[1:]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount() - 1
    assert sliced_ar.GetDataType() == ar.GetDataType()
    dims = sliced_ar.GetDimensions()
    assert dims[0].GetName() == 'dim_3'
    assert dims[1].GetName() == 'dim_4'
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 0, 0], count = [1, 3, 4])

    sliced_ar = ar[1,2,3]
    assert sliced_ar.GetDimensionCount() == 0
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 2, 3], count = [1, 1, 1])

    sliced_ar = ar[1,'newaxis',2,3]
    assert sliced_ar.GetDimensionCount() == 1
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 2, 3], count = [1, 1, 1])

    try:
        import numpy as np
        sliced_ar = ar[1,np.newaxis,2,3]
        assert sliced_ar.GetDimensionCount() == 1
        assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 2, 3], count = [1, 1, 1])
    except ImportError:
        pass

    sliced_ar = ar[1][2][3]
    assert sliced_ar.GetDimensionCount() == 0
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 2, 3], count = [1, 1, 1])

    sliced_ar = ar.GetView('[1][2][3]')
    assert sliced_ar.GetDimensionCount() == 0
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 2, 3], count = [1, 1, 1])

    orig_data = sliced_ar.Read()
    assert sliced_ar.Write(struct.pack('B', 123)) == gdal.CE_None
    assert struct.unpack('B', sliced_ar.Read()) == (123, )
    assert sliced_ar.Write(orig_data) == gdal.CE_None

    sliced_ar = ar[...]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount()
    assert sliced_ar.Read() == ar.Read()
    assert sliced_ar.Read(array_start_idx = [1, 0, 0],
                          count = [1, 3, 2],
                          array_step = [1, 1, 2]) == \
            ar.Read(array_start_idx = [1, 0, 0],
                    count = [1, 3, 2],
                    array_step = [1, 1, 2])

    assert sliced_ar.GetUnit() == ar.GetUnit()
    assert sliced_ar.GetScale() == ar.GetScale()
    assert sliced_ar.GetOffset() == ar.GetOffset()
    assert sliced_ar.GetNoDataValueAsRaw() == ar.GetNoDataValueAsRaw()
    assert sliced_ar.GetSpatialRef() is None

    assert ar.SetUnit("foo") == gdal.CE_None
    assert ar.SetScale(1) == gdal.CE_None
    assert ar.SetOffset(2) == gdal.CE_None
    assert ar.SetNoDataValueDouble(1) == gdal.CE_None
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    assert ar.SetSpatialRef(sr) == gdal.CE_None
    assert ar.GetSpatialRef()

    assert sliced_ar.GetUnit() == ar.GetUnit()
    assert sliced_ar.GetScale() == ar.GetScale()
    assert sliced_ar.GetOffset() == ar.GetOffset()
    assert sliced_ar.GetNoDataValueAsRaw() == ar.GetNoDataValueAsRaw()
    assert sliced_ar.GetSpatialRef().IsSame(ar.GetSpatialRef())

    sliced_ar = ar[1,...]
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 0, 0], count = [1, 3, 4])

    sliced_ar = ar[...,1]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount() - 1
    assert sliced_ar.Read() == ar.Read(array_start_idx = [0, 0, 1], count = [2, 3, 1])

    sliced_ar = ar[1,...,2]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount() - 2
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 0, 2], count = [1, 3, 1])

    sliced_ar = ar.GetView('[:]')
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount()
    assert sliced_ar.Read() == ar.Read()

    sliced_ar = ar.GetView('[:,:,1]')
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount() - 1
    assert sliced_ar.Read() == ar.Read(array_start_idx = [0, 0, 1], count = [2, 3, 1])

    sliced_ar = ar[::]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount()
    assert sliced_ar.Read() == ar.Read()

    sliced_ar = ar[0::]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount()
    assert sliced_ar.Read() == ar.Read()

    sliced_ar = ar[0:2:1]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount()
    assert sliced_ar.Read() == ar.Read()

    sliced_ar = ar[0:-1:1]
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount()
    assert sliced_ar.Read() == ar.Read(array_start_idx = [0, 0, 0], count = [1, 3, 4])

    sliced_ar = ar[1:2:1]
    assert sliced_ar
    assert sliced_ar.GetDimensionCount() == ar.GetDimensionCount()
    assert sliced_ar.Read() == ar.Read(array_start_idx = [1, 0, 0], count = [1, 3, 4])

    sliced_ar = ar[::-1,0,0]
    assert sliced_ar
    assert sliced_ar.GetDimensionCount() == 1
    assert sliced_ar.GetDimensions()[0].GetSize() == 2
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 12)[::-1]

    sliced_ar = ar[0,0,::-1]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[::-1]

    sliced_ar = ar[0,0,2::-1]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[2::-1]

    sliced_ar = ar[0,0,2:0:-1]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[2:0:-1]

    sliced_ar = ar[0,0,0:4:1]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[0:4:1]

    sliced_ar = ar[0,0,0:4:2]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[0:4:2]

    sliced_ar = ar[0,0,0:3:2]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[0:3:2]

    sliced_ar = ar[0,0,3:0:-2]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[3:0:-2]

    sliced_ar = ar[0,0,3:1:-2]
    assert sliced_ar
    read = sliced_ar.Read()
    assert struct.unpack('B' * len(read), read) == (0, 1, 2, 3)[3:1:-2]


def test_mem_md_band_as_mdarray():

    drv = gdal.GetDriverByName('MEM')

    def get_array():
        ds = drv.Create('my_ds', 10, 5, 2, gdal.GDT_UInt16)
        ds.SetGeoTransform([2,0.1,0,49,0,-0.1])
        sr = osr.SpatialReference()
        sr.ImportFromEPSG(32631)
        ds.SetSpatialRef(sr)
        band = ds.GetRasterBand(1)
        band.SetUnitType('foo')
        band.SetNoDataValue(2)
        band.SetOffset(1.5)
        band.SetScale(2.5)
        band.SetMetadataItem('FOO', 'BAR')
        band.WriteRaster(0, 0, 10, 5, struct.pack('H', 1), 1, 1)
        band.WriteRaster(0, 1, 1, 1, struct.pack('H', 2))
        band.WriteRaster(1, 0, 1, 1, struct.pack('H', 3))
        return (band.AsMDArray(), band.ReadRaster())

    ar, expected_data = get_array()
    assert ar
    assert ar.GetView('[...]')
    assert ar.GetBlockSize() == [ 1, 10 ]
    assert ar.GetDimensionCount() == 2
    dims = ar.GetDimensions()
    assert dims[0].GetName() == 'Y'
    assert dims[0].GetSize() == 5
    var_y = dims[0].GetIndexingVariable()
    assert var_y
    assert var_y.GetDimensions()[0].GetSize() == 5
    assert struct.unpack('d' * 5, var_y.Read()) == (48.95, 48.85, 48.75, 48.65, 48.55)
    assert dims[1].GetName() == 'X'
    assert dims[1].GetSize() == 10
    var_x = dims[1].GetIndexingVariable()
    assert var_x
    assert var_x.GetDimensions()[0].GetSize() == 10
    assert struct.unpack('d' * 10, var_x.Read()) == (2.05, 2.15, 2.25, 2.35, 2.45, 2.55, 2.65, 2.75, 2.85, 2.95)
    assert ar.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert ar.GetDataType().GetNumericDataType() == gdal.GDT_UInt16
    assert ar.Read() == expected_data
    assert struct.unpack('H' * 4, ar.Read(array_start_idx = [1,1], count = [2, 2], array_step = [-1, -1])) == (1,2,3,1)
    assert struct.unpack('H' * 4, ar[1::-1,1::-1].Read(array_start_idx = [0,0], count = [2, 2], array_step = [1, 1])) == (1,2,3,1)
    assert struct.unpack('H' * 4, ar.Read(array_start_idx = [0,0], count = [2, 2], array_step = [1, 1])) == (1,3,2,1)
    assert struct.unpack('H' * 4, ar[1::-1,1::-1].Read(array_start_idx = [1,1], count = [2, 2], array_step = [-1, -1])) == (1,3,2,1)
    assert ar.Write(expected_data) == gdal.CE_None
    assert ar.Read() == expected_data
    assert ar.GetUnit() == 'foo'
    assert ar.GetNoDataValueAsDouble() == 2
    assert ar.GetOffset() == 1.5
    assert ar.GetScale() == 2.5
    assert ar.GetSpatialRef() is not None
    assert len(ar.GetAttributes()) == 1
    attr = ar.GetAttribute('FOO')
    assert attr
    assert attr.Read() == 'BAR'

    def get_array():
        ds = drv.Create('my_ds', 10, 5, 2)
        band = ds.GetRasterBand(1)
        return (band.AsMDArray(), band.ReadRaster())

    ar, expected_data = get_array()
    assert ar.GetSpatialRef() is None
    assert ar.GetUnit() == ''
    assert ar.GetNoDataValueAsRaw() is None


def test_mem_md_array_as_classic_dataset():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim_y = rg.CreateDimension("dim_y", None, None, 2)
    dim_x = rg.CreateDimension("dim_x", None, None, 3)

    dim_x_var = rg.CreateMDArray("dim_x", [dim_x],
                                     gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    dim_x_var.Write( struct.pack('d' * 3, 1.25, 2.25, 3.25) )
    dim_x.SetIndexingVariable(dim_x_var)

    dim_y_var = rg.CreateMDArray("dim_y", [dim_y],
                                     gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    dim_y_var.Write( struct.pack('d' * 2, 10, 8) )
    dim_y.SetIndexingVariable(dim_y_var)

    ar = rg.CreateMDArray("nodim", [],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    with gdaltest.error_handler():
        assert not ar.AsClassicDataset(0, 0)

    ar = rg.CreateMDArray("1d", [ dim_x ],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    with gdaltest.error_handler():
        assert not ar.AsClassicDataset(1, 0)
    ds = ar.AsClassicDataset(0, 0)
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 1
    assert ds.RasterCount == 1
    assert not ds.GetSpatialRef()
    data = struct.pack('B' * 3, 0, 1, 2)
    assert ar.Write(data) == gdal.CE_None
    band = ds.GetRasterBand(1)
    assert len(band.ReadRaster()) == len(data)
    assert band.ReadRaster() == data
    assert band.WriteRaster(0, 0, 3, 1, data) == gdal.CE_None
    assert band.ReadRaster() == data

    ar = rg.CreateMDArray("2d_string", [ dim_y, dim_x ],
                          gdal.ExtendedDataType.CreateString())
    with gdaltest.error_handler():
        assert not ar.AsClassicDataset(0, 1)

    # 2D
    ar = rg.CreateMDArray("2d", [ dim_y, dim_x ],
                          gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    attr = ar.CreateAttribute('attr_float64', [1],
                              gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    attr.Write(1.25)
    attr = ar.CreateAttribute('attr_strings', [2],
                              gdal.ExtendedDataType.CreateString())
    attr.Write(['foo', 'bar'])
    with gdaltest.error_handler():
        assert not ar.AsClassicDataset(0, 0)
        assert not ar.AsClassicDataset(0, 2)
        assert not ar.AsClassicDataset(2, 0)
    ds = ar.AsClassicDataset(1, 0)
    assert ds
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 2
    assert ds.RasterCount == 1
    assert ds.GetGeoTransform() == (0.75, 1.0, 0.0, 11.0, 0.0, -2.0)
    assert ds.GetMetadata() == {'attr_float64': '1.25', 'attr_strings': '{foo,bar}'}
    band = ds.GetRasterBand(1)
    assert band.DataType == gdal.GDT_UInt16
    assert band.GetBlockSize() == [3, 1]
    data = struct.pack('H' * 6, 0, 1, 2, 3, 4, 5)
    assert ar.Write(data) == gdal.CE_None
    assert len(band.ReadRaster()) == len(data)
    assert band.ReadRaster() == data
    assert band.WriteRaster(0, 0, 3, 2, data) == gdal.CE_None
    assert band.ReadRaster() == data
    assert band.ReadBlock(0, 0) == struct.pack('H' * 3, 0, 1, 2)
    assert band.ReadBlock(0, 1) == struct.pack('H' * 3, 3, 4, 5)
    assert not band.GetNoDataValue()
    assert not band.GetScale()
    assert not band.GetOffset()
    assert band.GetUnitType() == ''

    assert ds.GetSpatialRef() is None
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    sr.SetDataAxisToSRSAxisMapping([2, 1])
    ar.SetSpatialRef(sr)
    got_sr = ds.GetSpatialRef()
    assert got_sr is not None
    assert got_sr.GetDataAxisToSRSAxisMapping() == [1, 2]
    got_sr.SetDataAxisToSRSAxisMapping([2,1])
    assert got_sr.IsSame(sr)
    ar.SetNoDataValueDouble(2)
    ar.SetScale(1.5)
    ar.SetOffset(2.5)
    ar.SetUnit('foo')
    assert band.GetNoDataValue() == 2
    assert band.GetScale() == 1.5
    assert band.GetOffset() == 2.5
    assert band.GetUnitType() == 'foo'

    # 3D with band dimension first
    dim_bands = rg.CreateDimension("dim_bands", None, None, 2)
    dim_bands_var = rg.CreateMDArray("dim_bands", [dim_bands],
                                     gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    dim_bands_var.Write( struct.pack('d' * 2, 1.25, 2.25) )
    dim_bands_var.SetUnit('my_unit')
    dim_bands.SetIndexingVariable(dim_bands_var)

    ar = rg.CreateMDArray("3d_band_first", [ dim_bands, dim_y, dim_x ],
                          gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    ds = ar.AsClassicDataset(2, 1)
    assert ds
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 2
    assert ds.RasterCount == 2
    band = ds.GetRasterBand(2)
    assert band.GetMetadata() == {'DIM_dim_bands_INDEX': '1',
                                  'DIM_dim_bands_UNIT': 'my_unit',
                                  'DIM_dim_bands_VALUE': '2.25'}
    assert band.GetBlockSize() == [3, 1]
    data = struct.pack('H' * 6, 0, 1, 2, 3, 4, 5)
    assert ar.Write(data, array_start_idx = [1, 0, 0], count = [1, 2, 3]) == gdal.CE_None
    assert band.ReadRaster() == data
    assert band.WriteRaster(0, 0, 3, 2, data) == gdal.CE_None
    assert band.ReadRaster() == data


    # 3D with band dimension last
    ar = rg.CreateMDArray("3d_band_last", [ dim_y, dim_x, dim_bands ],
                          gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    ds = ar.AsClassicDataset(1, 0)
    assert ds
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 2
    assert ds.RasterCount == 2
    band = ds.GetRasterBand(2)
    assert band.GetMetadata() == {'DIM_dim_bands_INDEX': '1',
                                  'DIM_dim_bands_UNIT': 'my_unit',
                                  'DIM_dim_bands_VALUE': '2.25'}
    assert band.GetBlockSize() == [3, 1]
    data = struct.pack('H' * 6, 0, 1, 2, 3, 4, 5)
    assert ar.Write(data, array_start_idx = [0, 0, 1], count = [2, 3, 1]) == gdal.CE_None
    assert band.ReadRaster() == data
    assert band.WriteRaster(0, 0, 3, 2, data) == gdal.CE_None
    assert band.ReadRaster() == data

    # 4D
    dim_time_subset = rg.CreateDimension("subset_time_1_2_3", None, None, 3)
    dim_time_subset_var = rg.CreateMDArray("subset_time_1_2_3", [dim_time_subset],
                                     gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    dim_time_subset_var.Write( struct.pack('d' * 3, 1.5, 2.5, 3.5) )
    dim_time_subset.SetIndexingVariable(dim_time_subset_var)
    ar = rg.CreateMDArray("4d", [ dim_time_subset, dim_y, dim_x, dim_bands ],
                          gdal.ExtendedDataType.Create(gdal.GDT_UInt16))
    ds = ar.AsClassicDataset(2, 1)
    band = ds.GetRasterBand(5)
    assert band.GetMetadata() == {'DIM_dim_bands_INDEX': '0',
                                  'DIM_dim_bands_UNIT': 'my_unit',
                                  'DIM_dim_bands_VALUE': '1.25',
                                  'DIM_time_INDEX': '5',
                                  'DIM_time_VALUE': '3.5'}


def test_mem_md_array_transpose():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim_z = rg.CreateDimension("dim_z", None, None, 4)
    dim_y = rg.CreateDimension("dim_y", None, None, 2)
    dim_x = rg.CreateDimension("dim_x", None, None, 3)
    ar = rg.CreateMDArray("ar", [ dim_z, dim_y, dim_x ],
                          gdal.ExtendedDataType.Create(gdal.GDT_UInt16))

    attr = ar.CreateAttribute('attr', [],
                              gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    assert attr.Write(1) == gdal.CE_None

    data = array.array('H', list(range(24))).tobytes()
    assert ar.Write(data) == gdal.CE_None

    with gdaltest.error_handler():
        assert not ar.Transpose([]) # 0 axis
        assert not ar.Transpose([0, 1]) # missing axis
        assert not ar.Transpose([0, 1, 2, 3]) # too many axis
        assert not ar.Transpose([0, 1, 3]) # invalid axis number
        assert not ar.Transpose([0, 1, -2]) # invalid axis number
        assert not ar.Transpose([0, 1, -1]) # missing axis
        assert not ar.Transpose([0, 1, 1]) # repeated axis

    # Idendity
    transposed = ar.Transpose([0, 1, 2])
    assert transposed.GetDimensionCount() == 3
    dims = transposed.GetDimensions()
    assert dims[0].GetName() == 'dim_z'
    assert dims[1].GetName() == 'dim_y'
    assert dims[2].GetName() == 'dim_x'
    assert transposed.Read() == data

    assert transposed.GetUnit() == ar.GetUnit()
    assert transposed.GetScale() == ar.GetScale()
    assert transposed.GetOffset() == ar.GetOffset()
    assert transposed.GetNoDataValueAsRaw() == ar.GetNoDataValueAsRaw()
    assert transposed.GetSpatialRef() is None
    assert transposed.GetAttribute('attr') is not None
    assert len(transposed.GetAttributes()) == 1

    assert ar.SetUnit("foo") == gdal.CE_None
    assert ar.SetScale(1) == gdal.CE_None
    assert ar.SetOffset(2) == gdal.CE_None
    assert ar.SetNoDataValueDouble(1) == gdal.CE_None
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    sr.SetDataAxisToSRSAxisMapping([3, 2])
    assert ar.SetSpatialRef(sr) == gdal.CE_None
    assert ar.GetSpatialRef()

    assert transposed.GetUnit() == ar.GetUnit()
    assert transposed.GetScale() == ar.GetScale()
    assert transposed.GetOffset() == ar.GetOffset()
    assert transposed.GetNoDataValueAsRaw() == ar.GetNoDataValueAsRaw()
    assert transposed.GetSpatialRef().IsSame(ar.GetSpatialRef())

    # Idendity with one extra axis
    transposed = ar.Transpose([0, 1, -1, 2])
    assert transposed.GetDimensionCount() == 4
    dims = transposed.GetDimensions()
    assert dims[0].GetName() == 'dim_z'
    assert dims[1].GetName() == 'dim_y'
    assert dims[2].GetName() == 'newaxis'
    assert dims[3].GetName() == 'dim_x'
    assert transposed.Read() == data
    assert transposed.GetBlockSize() == [0, 0, 0, 0]

    got_sr = transposed.GetSpatialRef()
    assert got_sr
    assert got_sr.GetDataAxisToSRSAxisMapping() == [4, 2]

    # Full transpose
    transposed = ar.Transpose([2, 1, 0])
    assert transposed.GetDimensionCount() == 3
    dims = transposed.GetDimensions()
    assert dims[0].GetName() == 'dim_x'
    assert dims[1].GetName() == 'dim_y'
    assert dims[2].GetName() == 'dim_z'
    transposed_data = transposed.Read()
    assert struct.unpack('H' * 24, transposed_data) == (0, 6, 12, 18, 3, 9, 15, 21, 1, 7, 13, 19, 4, 10, 16, 22, 2, 8, 14, 20, 5, 11, 17, 23)
    assert transposed.Read(buffer_stride = [1, 3, 6]) == data
    assert transposed.Write(struct.pack('H', 0) * 24) == gdal.CE_None
    assert ar.Read() == struct.pack('H', 0) * 24
    assert transposed.Write(transposed_data) == gdal.CE_None
    assert ar.Read() == data

    # Rotation of axis
    transposed = ar.Transpose([1, 2, 0])
    assert transposed.Read(buffer_stride = [3, 1, 6]) == data


def test_mem_md_array_single_dim_non_contiguous_copy():

    drv = gdal.GetDriverByName('MEM')
    nvalues = 30
    spacing = 63
    data = array.array('B', list(range(nvalues))).tobytes()
    for t in (gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_Int32, gdal.GDT_Float64, gdal.GDT_CFloat64):
        ds = drv.CreateMultiDimensional('myds')
        rg = ds.GetRootGroup()
        dim = rg.CreateDimension("dim0", "", "", nvalues)
        ar = rg.CreateMDArray("ar", [ dim ],
                                gdal.ExtendedDataType.Create(t))
        assert ar
        assert ar.Write(data, buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Byte)) == gdal.CE_None
        got_data = ar.Read( buffer_stride = [spacing], buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Byte) )
        assert len(got_data) == (nvalues - 1) * spacing + 1
        got_data = struct.unpack('B' * len(got_data), got_data)
        for i in range(nvalues):
            assert got_data[i * spacing] == i


def test_mem_md_array_get_unscaled_0dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    myarray = rg.CreateMDArray("myarray", [],
                               gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray

    data = array.array('B', [1]).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    myarray.SetOffset(1.5)
    myarray.SetScale(200.5)

    unscaled = myarray.GetUnscaled()
    assert unscaled.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert struct.unpack('d' * 1, unscaled.Read())[0] == 1 * 200.5 + 1.5

    float32dt = gdal.ExtendedDataType.Create(gdal.GDT_Float32)
    assert struct.unpack('f' * 1, unscaled.Read(buffer_datatype = float32dt))[0] == 1 * 200.5 + 1.5

    assert unscaled.Write(struct.pack('d' * 1, 2 * 200.5 + 1.5)) == gdal.CE_None
    assert struct.unpack('B' * 1, myarray.Read())[0] == 2

    assert unscaled.Write(struct.pack('d' * 1, 2.1 * 200.5 + 1.5)) == gdal.CE_None
    assert struct.unpack('B' * 1, myarray.Read())[0] == 2

    assert unscaled.Write(struct.pack('d' * 1, 1.9 * 200.5 + 1.5)) == gdal.CE_None
    assert struct.unpack('B' * 1, myarray.Read())[0] == 2

    assert unscaled.Write(struct.pack('f' * 1, 3 * 200.5 + 1.5), buffer_datatype = float32dt) == gdal.CE_None
    assert struct.unpack('B' * 1, myarray.Read())[0] == 3


def test_mem_md_array_get_unscaled_0dim_complex():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    myarray = rg.CreateMDArray("myarray", [],
                               gdal.ExtendedDataType.Create(gdal.GDT_CInt16))
    assert myarray

    data = array.array('H', [1, 2]).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    myarray.SetOffset(1.5)
    myarray.SetScale(200.5)

    unscaled = myarray.GetUnscaled()
    assert unscaled.GetDataType().GetNumericDataType() == gdal.GDT_CFloat64
    assert struct.unpack('d' * 2, unscaled.Read()) == (1 * 200.5 + 1.5,  2 * 200.5 + 1.5)

    cfloat32dt = gdal.ExtendedDataType.Create(gdal.GDT_CFloat32)
    assert struct.unpack('f' * 2, unscaled.Read(buffer_datatype = cfloat32dt)) == (1 * 200.5 + 1.5,  2 * 200.5 + 1.5)

    assert unscaled.Write(struct.pack('d' * 2, 3 * 200.5 + 1.5, 4 * 200.5 + 1.5)) == gdal.CE_None
    assert struct.unpack('H' * 2, myarray.Read()) == (3, 4)

    assert unscaled.Write(struct.pack('f' * 2, 5 * 200.5 + 1.5, 6 * 200.5 + 1.5), buffer_datatype = cfloat32dt) == gdal.CE_None
    assert struct.unpack('H' * 2, myarray.Read()) == (5, 6)


def test_mem_md_array_get_unscaled_0dim_non_matching_nodata():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    myarray = rg.CreateMDArray("myarray", [],
                               gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray

    data = array.array('B', [1]).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    myarray.SetOffset(1.5)
    myarray.SetScale(200.5)
    myarray.SetNoDataValueDouble(3)

    unscaled = myarray.GetUnscaled()
    assert unscaled.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    nodata = unscaled.GetNoDataValueAsDouble()
    assert math.isnan(nodata)
    assert struct.unpack('d' * 1, unscaled.Read())[0] == 1 * 200.5 + 1.5
    assert struct.unpack('f' * 1, unscaled.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float32)))[0] == 1 * 200.5 + 1.5

    assert unscaled.Write(struct.pack('d' * 1, 2 * 200.5 + 1.5)) == gdal.CE_None
    assert struct.unpack('B' * 1, myarray.Read())[0] == 2


def test_mem_md_array_get_unscaled_0dim_matching_nodata():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    myarray = rg.CreateMDArray("myarray", [],
                               gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray

    data = array.array('B', [1]).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    myarray.SetOffset(1.5)
    myarray.SetScale(200.5)
    myarray.SetNoDataValueDouble(1)

    unscaled = myarray.GetUnscaled()
    assert unscaled.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    nodata = unscaled.GetNoDataValueAsDouble()
    assert math.isnan(nodata)
    assert math.isnan(struct.unpack('d' * 1, unscaled.Read())[0])

    assert unscaled.Write(struct.pack('d' * 1, 2 * 200.5 + 1.5)) == gdal.CE_None
    assert struct.unpack('B' * 1, myarray.Read())[0] == 2

    assert unscaled.Write(struct.pack('d' * 1, nodata)) == gdal.CE_None
    assert struct.unpack('B' * 1, myarray.Read())[0] == 1


def test_mem_md_array_get_unscaled_0dim_matching_nodata_complex():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    myarray = rg.CreateMDArray("myarray", [],
                               gdal.ExtendedDataType.Create(gdal.GDT_CInt16))
    assert myarray

    data = array.array('H', [1, 2]).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    myarray.SetOffset(1.5)
    myarray.SetScale(200.5)
    myarray.SetNoDataValueRaw(struct.pack('H' * 2, 1, 2))

    unscaled = myarray.GetUnscaled()
    assert unscaled.GetDataType().GetNumericDataType() == gdal.GDT_CFloat64
    nodata = unscaled.GetNoDataValueAsDouble()
    assert math.isnan(nodata)
    assert math.isnan(struct.unpack('d' * 2, unscaled.Read())[0])
    assert math.isnan(struct.unpack('d' * 2, unscaled.Read())[1])

    assert unscaled.Write(struct.pack('d' * 2, nodata, nodata)) == gdal.CE_None
    assert struct.unpack('H' * 2, myarray.Read()) == (1, 2)


def test_mem_md_array_get_unscaled_3dim():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 2)
    dim1 = rg.CreateDimension("dim1", None, None, 3)
    dim2 = rg.CreateDimension("dim2", None, None, 4)
    myarray = rg.CreateMDArray("myarray", [ dim0, dim1, dim2 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    assert myarray

    data = array.array('B', list(range(24))).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    assert myarray.GetUnscaled().Read() == myarray.Read()

    myarray.SetOffset(1.5)
    myarray.SetScale(200.5)

    unscaled = myarray.GetUnscaled()
    assert unscaled.GetOffset() is None
    assert unscaled.GetScale() is None
    assert unscaled.GetNoDataValueAsRaw() is None
    assert unscaled.GetSpatialRef() is None
    assert unscaled.GetUnit() == myarray.GetUnit()
    assert unscaled.GetBlockSize() == myarray.GetBlockSize()
    assert [x.GetSize() for x in unscaled.GetDimensions()] == [x.GetSize() for x in myarray.GetDimensions() ]
    assert unscaled.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    expected_data = [x * 200.5 + 1.5 for x in range(24)]
    unscaled_data = unscaled.Read()
    assert [x for x in struct.unpack('d' * 24, unscaled_data)] == expected_data
    float32_dt = gdal.ExtendedDataType.Create(gdal.GDT_Float32)
    unscaled_data_float32 = unscaled.Read(buffer_datatype = float32_dt)
    assert [x for x in struct.unpack('f' * 24, unscaled_data_float32)] == expected_data

    assert myarray.Write(b'\x00' * 24) == gdal.CE_None
    assert myarray.Read() != data

    assert unscaled.Write(unscaled_data) == gdal.CE_None
    assert myarray.Read() == data

    assert myarray.Write(b'\x00' * 24) == gdal.CE_None
    assert myarray.Read() != data

    assert unscaled.Write(unscaled_data_float32, buffer_datatype = float32_dt) == gdal.CE_None
    assert myarray.Read() == data

    myarray.SetNoDataValueDouble(1)
    unscaled = myarray.GetUnscaled()
    assert math.isnan(unscaled.GetNoDataValueAsDouble())
    unscaled_data_with_nan = unscaled.Read()
    got_data = [x for x in struct.unpack('d' * 24, unscaled_data_with_nan)]
    expected_data = [float('nan') if x == 1 else x * 200.5 + 1.5 for x in struct.unpack('B' * 24, myarray.Read())]
    for i in range(24):
        if math.isnan(expected_data[i]):
            assert math.isnan(got_data[i])
        else:
            assert got_data[i] == expected_data[i]

    assert myarray.Write(b'\x00' * 24) == gdal.CE_None
    assert myarray.Read() != data

    assert unscaled.Write(unscaled_data_with_nan) == gdal.CE_None
    assert myarray.Read() == data


def test_mem_md_array_get_unscaled_1dim_complex():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 2)
    myarray = rg.CreateMDArray("myarray", [ dim0 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_CInt16))
    assert myarray

    data = array.array('H', [1, 2, 3, 4]).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    assert myarray.GetUnscaled().Read() == myarray.Read()

    assert myarray.GetUnscaled().AdviseRead() == gdal.CE_None

    myarray.SetOffset(1.5)
    myarray.SetScale(200.5)
    myarray.SetNoDataValueRaw(struct.pack('H' * 2, 1, 2))

    unscaled = myarray.GetUnscaled()
    assert unscaled.GetDataType().GetNumericDataType() == gdal.GDT_CFloat64
    unscaled_data_with_nan = unscaled.Read()
    got_data = [x for x in struct.unpack('d' * 4, unscaled_data_with_nan)]
    assert math.isnan(got_data[0])
    assert math.isnan(got_data[1])
    assert got_data[2] == 3 * 200.5 + 1.5
    assert got_data[3] == 4 * 200.5 + 1.5

    assert unscaled.Write(unscaled_data_with_nan) == gdal.CE_None
    assert myarray.Read() == data


def test_mem_md_array_get_mask():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()

    myarray = rg.CreateMDArray("myarray_emptydim", [],
                               gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    mask = myarray.GetMask()
    assert mask is not None
    assert struct.unpack('B', mask.Read())[0] == 1
    assert struct.unpack('H', mask.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int16)))[0] == 1

    myarray.SetNoDataValueDouble(0)
    assert struct.unpack('B', mask.Read())[0] == 0
    assert struct.unpack('H', mask.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int16)))[0] == 0

    assert myarray.GetMask().AdviseRead() == gdal.CE_None

    dim0 = rg.CreateDimension("dim0", None, None, 2)
    dim1 = rg.CreateDimension("dim1", None, None, 3)
    dim2 = rg.CreateDimension("dim2", None, None, 4)
    myarray = rg.CreateMDArray("myarray_string", [dim0],
                               gdal.ExtendedDataType.CreateString())
    # Non-numeric array unsupported
    with gdaltest.error_handler():
        assert not myarray.GetMask()

    myarray = rg.CreateMDArray("myarray", [ dim0, dim1, dim2 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Int32))
    data = array.array('I', list(range(24))).tobytes()
    assert myarray.Write(data) == gdal.CE_None

    mask = myarray.GetMask()
    assert mask.GetOffset() is None
    assert mask.GetScale() is None
    with gdaltest.error_handler():
        assert mask.Write(mask.Read()) == gdal.CE_Failure
    assert mask.GetNoDataValueAsRaw() is None
    assert mask.GetSpatialRef() is None
    assert mask.GetUnit() == myarray.GetUnit()
    assert mask.GetBlockSize() == myarray.GetBlockSize()
    assert [x.GetSize() for x in mask.GetDimensions()] == [x.GetSize() for x in myarray.GetDimensions() ]
    assert mask.GetDataType().GetNumericDataType() == gdal.GDT_Byte
    # Case when we don't need to read the underlying array at all: the mask is always valid
    assert [x for x in struct.unpack('B' * 24, mask.Read())] == [ 1 ] * 24
    assert [x for x in struct.unpack('B' * 24, mask.Read(buffer_stride = [1, 2, 6]))] == [ 1 ] * 24
    assert [x for x in struct.unpack('H' * 24, mask.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int16)))] == [ 1 ] * 24
    assert [x for x in struct.unpack('H' * 24, mask.Read(
        buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int16), buffer_stride = [1, 2, 6]))] == [ 1 ] * 24

    # Test no data value
    myarray.SetNoDataValueDouble(10)
    expected_data = [ 1 ] * 24
    expected_data[10] = 0
    assert [x for x in struct.unpack('B' * 24, mask.Read())] == expected_data
    assert [x for x in struct.unpack('B' * 24, mask.Read(buffer_stride = [1, 2, 6]))] == [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1]
    assert [x for x in struct.unpack('H' * 24, mask.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Int16)))] == expected_data

    # Test missing_value, _FillValue, valid_min, valid_max
    bytedt = gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    attr = myarray.CreateAttribute('missing_value', [1], bytedt)
    assert attr.Write(8) == gdal.CE_None
    attr = myarray.CreateAttribute('_FillValue', [1], bytedt)
    assert attr.Write(9) == gdal.CE_None
    attr = myarray.CreateAttribute('valid_min', [1], bytedt)
    assert attr.Write(2) == gdal.CE_None
    attr = myarray.CreateAttribute('valid_max', [1], bytedt)
    assert attr.Write(22) == gdal.CE_None
    expected_data = [ 1 ] * 24
    expected_data[0] = 0
    expected_data[1] = 0
    expected_data[8] = 0
    expected_data[9] = 0
    expected_data[10] = 0
    expected_data[23] = 0
    assert [x for x in struct.unpack('B' * 24, mask.Read())] == expected_data

    # Test valid_range
    myarray = rg.CreateMDArray("myarray_valid_range", [ dim0, dim1, dim2 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    data = array.array('H', list(range(24))).tobytes()
    assert myarray.Write(data) == gdal.CE_None
    attr = myarray.CreateAttribute('valid_range', [2], bytedt)
    assert attr.Write([1,22]) == gdal.CE_None
    mask = myarray.GetMask()
    expected_data = [ 1 ] * 24
    expected_data[0] = 0
    expected_data[23] = 0
    assert [x for x in struct.unpack('B' * 24, mask.Read())] == expected_data

    try:
        import numpy
        has_numpy = True
    except ImportError:
        has_numpy = False

    if has_numpy:
        ma = myarray.ReadAsMaskedArray()
        assert ma[0,0,0] is numpy.ma.masked
        assert ma[0,0,1] is not numpy.ma.masked

    # Test array with nan
    myarray = rg.CreateMDArray("myarray_with_nan", [ dim0 ],
                               gdal.ExtendedDataType.Create(gdal.GDT_Float32))
    assert myarray.Write(struct.pack('f' * 2, 0, float('nan'))) == gdal.CE_None

    mask = myarray.GetMask()
    assert [x for x in struct.unpack('B' * 2, mask.Read())] == [1, 0]

    # Test all data types
    for dt, v, nv, expected in [ (gdal.GDT_Byte, 1, 1,[1, 0]),
                                 (gdal.GDT_Byte, 1, 1.5, [1, 1]),
                                 (gdal.GDT_Int16, 1, 1, [1, 0]),
                                 (gdal.GDT_UInt16, 1, 1, [1, 0]),
                                 (gdal.GDT_Int32, 1, 1, [1, 0]),
                                 (gdal.GDT_UInt32, 1, 1, [1, 0]),
                                 (gdal.GDT_Float32, 1, 1, [1, 0]),
                                 (gdal.GDT_Float32, 1.5, 1.5, [1, 0]),
                                 (gdal.GDT_Float64, 1, 1, [1, 0]),
                                 (gdal.GDT_Float64, 1.5, 1.5, [1, 0]),
                                 (gdal.GDT_CInt16, 1, 1, [1, 0]) ]:
        myarray = rg.CreateMDArray("array_dt_" + gdal.GetDataTypeName(dt) + '_' + str(v) + '_' + str(nv), [ dim0 ],
                               gdal.ExtendedDataType.Create(dt))
        assert myarray.Write(struct.pack('d' * 2, 0, v), buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == gdal.CE_None
        myarray.SetNoDataValueDouble(nv)
        mask = myarray.GetMask()
        assert [x for x in struct.unpack('B' * 2, mask.Read())] == expected, myarray.GetName()


def test_mem_md_array_resolvemdarray():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()

    a = rg.CreateGroup("a")
    aa = a.CreateGroup("aa")
    a.CreateGroup("ab")
    b = rg.CreateGroup("b")

    a.CreateMDArray("var_a", [], gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    a.CreateMDArray("var_c", [], gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    aa.CreateMDArray("var_aa", [], gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    b.CreateMDArray("var_b", [], gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    b.CreateMDArray("var_c", [], gdal.ExtendedDataType.Create(gdal.GDT_Int16))

    assert rg.ResolveMDArray("x", "/") is None

    assert rg.ResolveMDArray("/a/var_a", "/").GetFullName() == "/a/var_a"
    assert rg.ResolveMDArray("var_a", "/").GetFullName() == "/a/var_a"
    assert rg.ResolveMDArray("var_a", "").GetFullName() == "/a/var_a"
    assert rg.ResolveMDArray("var_a", "/a").GetFullName() == "/a/var_a"
    assert rg.ResolveMDArray("var_a", "/a/aa").GetFullName() == "/a/var_a"
    assert rg.ResolveMDArray("var_a", "/a/ab").GetFullName() == "/a/var_a"
    assert rg.ResolveMDArray("var_a", "/b").GetFullName() == "/a/var_a"

    assert rg.ResolveMDArray("var_aa", "/").GetFullName() == "/a/aa/var_aa"
    assert a.ResolveMDArray("var_aa", "/").GetFullName() == "/a/aa/var_aa"
    assert a.ResolveMDArray("var_aa", "/aa").GetFullName() == "/a/aa/var_aa"
    assert aa.ResolveMDArray("var_aa", "/").GetFullName() == "/a/aa/var_aa"

    assert rg.ResolveMDArray("var_b", "").GetFullName() == "/b/var_b"

    assert rg.ResolveMDArray("var_c", "/a").GetFullName() == "/a/var_c"
    assert rg.ResolveMDArray("var_c", "/a/aa").GetFullName() == "/a/var_c"
    assert rg.ResolveMDArray("var_c", "/b").GetFullName() == "/b/var_c"
    assert a.ResolveMDArray("var_c", "").GetFullName() == "/a/var_c"
    assert b.ResolveMDArray("var_c", "").GetFullName() == "/b/var_c"


def test_mem_md_array_statistics():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
    dim1 = rg.CreateDimension("dim1", "unspecified type", "unspecified direction", 3)
    float64dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    ar = rg.CreateMDArray("myarray", [dim0, dim1], float64dt)
    ar.SetNoDataValueDouble(6)
    data = struct.pack('d' * 6, 1, 2, 3, 4, 5, 6)
    ar.Write(data)

    stats = ar.ComputeStatistics(None, False)
    assert stats.min == 1.0
    assert stats.max == 5.0
    assert stats.mean == 3.0
    assert stats.std_dev == pytest.approx(1.4142135623730951)
    assert stats.valid_count == 5

    stats = ar.GetStatistics(None, False, False)
    assert stats is None

    stats = ar.GetStatistics(None, False, True)
    assert stats is not None
    assert stats.min == 1.0
    assert stats.max == 5.0
    assert stats.mean == 3.0
    assert stats.std_dev == pytest.approx(1.4142135623730951)
    assert stats.valid_count == 5


def test_mem_md_array_statistics_float32():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
    dim1 = rg.CreateDimension("dim1", "unspecified type", "unspecified direction", 3)
    float32dt = gdal.ExtendedDataType.Create(gdal.GDT_Float32)
    ar = rg.CreateMDArray("myarray", [dim0, dim1], float32dt)
    ar.SetNoDataValueDouble(6)
    data = struct.pack('f' * 6, 1, 2, 3, 4, 5, 6)
    ar.Write(data)

    stats = ar.ComputeStatistics(None, False)
    assert stats.min == 1.0
    assert stats.max == 5.0
    assert stats.mean == 3.0
    assert stats.std_dev == pytest.approx(1.4142135623730951)
    assert stats.valid_count == 5


def test_mem_md_array_copy_autoscale():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
    dim1 = rg.CreateDimension("dim1", "unspecified type", "unspecified direction", 3)
    float32dt = gdal.ExtendedDataType.Create(gdal.GDT_Float32)
    ar = rg.CreateMDArray("myarray", [dim0, dim1], float32dt)
    data = struct.pack('f' * 6, 1.5, 2, 3, 4, 5, 6.5)
    ar.Write(data)
    attr = ar.CreateAttribute('attr_float64', [1],
                              gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    attr.Write(1.25)

    attr = ar.CreateAttribute('valid_min', [1],
                              gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    attr.Write(1.25)

    out_ds = drv.CreateCopy('', ds, options = ['ARRAY:AUTOSCALE=YES'])
    out_rg = out_ds.GetRootGroup()
    out_ar = out_rg.OpenMDArray('myarray')
    assert out_ar.GetAttribute('attr_float64') is not None
    assert out_ar.GetAttribute('valid_min') is None
    assert out_ar.GetDataType() == gdal.ExtendedDataType.Create(gdal.GDT_UInt16)
    assert out_ar.GetOffset() == 1.5
    assert out_ar.GetScale() == (6.5 - 1.5) / 65535.
    assert struct.unpack('H' * 6, out_ar.Read()) == (0, 6554, 19661, 32768, 45875, 65535)
    assert struct.unpack('d' * 6, out_ar.GetUnscaled().Read()) == pytest.approx( (1.5, 2, 3, 4, 5, 6.5), abs = out_ar.GetScale() / 2 )


def test_mem_md_array_copy_autoscale_with_explicit_data_type_and_nodata():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('myds')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", "unspecified type", "unspecified direction", 2)
    dim1 = rg.CreateDimension("dim1", "unspecified type", "unspecified direction", 3)
    float32dt = gdal.ExtendedDataType.Create(gdal.GDT_Float32)
    ar = rg.CreateMDArray("myarray", [dim0, dim1], float32dt)
    ar.SetNoDataValueDouble(5)
    data = struct.pack('f' * 6, 1.5, 2, 3, 4, 6.5, 5)
    ar.Write(data)

    out_ds = drv.CreateCopy('', ds, options = ['ARRAY:AUTOSCALE=YES',
                                               'ARRAY:AUTOSCALE_DATA_TYPE=Int16'])
    out_rg = out_ds.GetRootGroup()
    out_ar = out_rg.OpenMDArray('myarray')
    assert out_ar.GetDataType() == gdal.ExtendedDataType.Create(gdal.GDT_Int16)
    assert out_ar.GetScale() == (6.5 - 1.5) / (65535. - 1)
    assert out_ar.GetOffset() == 1.5 - (-32768) * out_ar.GetScale()
    assert out_ar.GetNoDataValueAsDouble() == 32767.
    assert struct.unpack('h' * 6, out_ar.Read()) == (-32768, -26215, -13108, -1, 32766, 32767)
    unscaled = struct.unpack('d' * 6, out_ar.GetUnscaled().Read())
    assert unscaled[0:5] == pytest.approx( (1.5, 2, 3, 4, 6.5), abs = out_ar.GetScale() / 2 )
    assert math.isnan(unscaled[5])


def XX_test_all_forever():
    while True:
        test_mem_md_basic()
        test_mem_md_subgroup()
        test_mem_md_array_unnamed_array()
        test_mem_md_array_duplicated_array_name()
        test_mem_md_array_nodim()
        test_mem_md_array_single_dim()
        test_mem_md_array_string()
        test_mem_md_datatypes()
        test_mem_md_array_compoundtype()
        test_mem_md_array_3_dim()
        test_mem_md_array_4_dim()
        test_mem_md_copy_array()
        test_mem_md_array_read_write_errors()
        test_mem_md_invalid_dims()
        test_mem_md_array_invalid_args()
        test_mem_md_array_too_large()
        test_mem_md_array_too_large_overflow_dim()
        test_mem_md_array_30dim()
        test_mem_md_array_32dim()
        test_mem_md_group_attribute_single_numeric()
        test_mem_md_group_attribute_multiple_numeric()
        test_mem_md_group_attribute_single_string()
        test_mem_md_group_attribute_multiple_string()
        test_mem_md_array_attribute()
        test_mem_md_array_slice()
        test_mem_md_band_as_mdarray()
        test_mem_md_array_as_classic_dataset()
        test_mem_md_array_transpose()
        test_mem_md_array_single_dim_non_contiguous_copy()
