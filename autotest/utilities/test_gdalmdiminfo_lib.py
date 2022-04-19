#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalmdiminfo
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

import json
import os
import pytest
import struct

from osgeo import gdal
from osgeo import osr

###############################################################################
# Validate against schema

def _validate(res):
    try:
        import jsonschema
    except ImportError:
        return
    if int(jsonschema.__version__.split('.')[0]) < 3:
        return

    if isinstance(res, str):
        res = json.loads(res)

    schema_filename = '../../gdal/data/gdalmdiminfo_output.schema.json'
    if not os.path.exists(schema_filename):
        return

    jsonschema.validate(res, json.loads(open(schema_filename, 'rt').read()))


###############################################################################
# Test with non multidim dataset


def test_gdalmdiminfo_lib_non_multidim_dataset():

    ds = gdal.Open('../gcore/data/byte.tif')

    with pytest.raises(TypeError):
        gdal.MultiDimInfo(ds)

    with pytest.raises(TypeError):
        gdal.MultiDimInfo('../gcore/data/byte.tif')

###############################################################################
# Test with a empty MEM dataset


def test_gdalmdiminfo_lib_empty_mem_dataset():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('')
    ret = gdal.MultiDimInfo(ds)
    _validate(ret)

    assert ret == {'type': 'group', "driver": "MEM", 'name': '/'}


###############################################################################
# Test with a MEM dataset


def test_gdalmdiminfo_lib_mem_dataset():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('')
    rg = ds.GetRootGroup()
    subg = rg.CreateGroup('subgroup')
    subg.CreateGroup('subsubgroup')

    dim0 = rg.CreateDimension("dim0", "my_type", "my_direction", 2)
    comp0 = gdal.EDTComponent.Create('x', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    comp1 = gdal.EDTComponent.Create('y', 4, gdal.ExtendedDataType.Create(gdal.GDT_Int32))
    dt = gdal.ExtendedDataType.CreateCompound("mytype", 8, [comp0, comp1])
    ar = rg.CreateMDArray("ar_compound", [ dim0 ], dt)
    assert ar.Write(struct.pack('hi' * 2, 32767, 1000000, -32768, -1000000)) == gdal.CE_None
    assert ar.SetNoDataValueRaw(struct.pack('hi', 32767, 1000000)) == gdal.CE_None

    dim1 = rg.CreateDimension("dim1", None, None, 3)
    ar = rg.CreateMDArray("ar_2d", [ dim0, dim1 ], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    ar.SetOffset(1)
    ar.SetScale(2)
    ar.SetUnit('foo')
    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=utm +zone=31 +datum=WGS84")
    srs.SetDataAxisToSRSAxisMapping([2,1])
    ar.SetSpatialRef(srs)
    attr = ar.CreateAttribute('myattr', [], gdal.ExtendedDataType.CreateString())
    attr.WriteString('bar')

    ret = gdal.MultiDimInfo(ds, detailed = True, as_text = True)
    _validate(ret)

    expected = """{
  "type": "group",
  "driver": "MEM",
  "name": "/",
  "dimensions": [
    {
      "name": "dim0",
      "full_name": "/dim0",
      "size": 2,
      "type": "my_type",
      "direction": "my_direction"
    },
    {
      "name": "dim1",
      "full_name": "/dim1",
      "size": 3
    }
  ],
  "arrays": {
    "ar_2d": {
      "datatype": "Byte",
      "dimensions": [
        "/dim0",
        "/dim1"
      ],
      "dimension_size": [
        2,
        3
      ],
      "attributes": {
        "myattr": {
          "datatype": "String",
          "value": "bar"
        }
      },
      "unit": "foo",
      "offset": 1,
      "scale": 2,
      "srs": {
        "wkt": "PROJCRS[\\"unknown\\",BASEGEOGCRS[\\"unknown\\",DATUM[\\"World Geodetic System 1984\\",ELLIPSOID[\\"WGS 84\\",6378137,298.257223563,LENGTHUNIT[\\"metre\\",1]],ID[\\"EPSG\\",6326]],PRIMEM[\\"Greenwich\\",0,ANGLEUNIT[\\"degree\\",0.0174532925199433],ID[\\"EPSG\\",8901]]],CONVERSION[\\"UTM zone 31N\\",METHOD[\\"Transverse Mercator\\",ID[\\"EPSG\\",9807]],PARAMETER[\\"Latitude of natural origin\\",0,ANGLEUNIT[\\"degree\\",0.0174532925199433],ID[\\"EPSG\\",8801]],PARAMETER[\\"Longitude of natural origin\\",3,ANGLEUNIT[\\"degree\\",0.0174532925199433],ID[\\"EPSG\\",8802]],PARAMETER[\\"Scale factor at natural origin\\",0.9996,SCALEUNIT[\\"unity\\",1],ID[\\"EPSG\\",8805]],PARAMETER[\\"False easting\\",500000,LENGTHUNIT[\\"metre\\",1],ID[\\"EPSG\\",8806]],PARAMETER[\\"False northing\\",0,LENGTHUNIT[\\"metre\\",1],ID[\\"EPSG\\",8807]],ID[\\"EPSG\\",16031]],CS[Cartesian,2],AXIS[\\"(E)\\",east,ORDER[1],LENGTHUNIT[\\"metre\\",1,ID[\\"EPSG\\",9001]]],AXIS[\\"(N)\\",north,ORDER[2],LENGTHUNIT[\\"metre\\",1,ID[\\"EPSG\\",9001]]]]",
        "data_axis_to_srs_axis_mapping": [2, 1]
      },
      "values": [
        [0, 0, 0],
        [0, 0, 0]
      ]
    },
    "ar_compound": {
      "datatype": {
        "name": "mytype",
        "size": 8,
        "components": [
          {
            "name": "x",
            "offset": 0,
            "type": "Int16"
          },
          {
            "name": "y",
            "offset": 4,
            "type": "Int32"
          }
        ]
      },
      "dimensions": [
        "/dim0"
      ],
      "dimension_size": [
        2
      ],
      "nodata_value": {
        "x": 32767,
        "y": 1000000
      },
      "values": [{"x": 32767, "y": 1000000}, {"x": -32768, "y": -1000000}]
    }
  },
  "groups": {
    "subgroup": {
      "groups": {
        "subsubgroup": {}
      }
    }
  }
}"""
    try:
        expected = expected.decode('UTF-8')
    except:
        pass
    if ret != expected:
        print(ret)
    assert ret == expected

    ret = gdal.MultiDimInfo(ds, array = 'ar_compound', detailed = True, as_text = True)
    _validate(ret)

    expected = """{
  "type": "array",
  "name": "ar_compound",
  "datatype": {
    "name": "mytype",
    "size": 8,
    "components": [
      {
        "name": "x",
        "offset": 0,
        "type": "Int16"
      },
      {
        "name": "y",
        "offset": 4,
        "type": "Int32"
      }
    ]
  },
  "dimensions": [
    {
      "name": "dim0",
      "full_name": "/dim0",
      "size": 2,
      "type": "my_type",
      "direction": "my_direction"
    }
  ],
  "dimension_size": [
    2
  ],
  "nodata_value": {
    "x": 32767,
    "y": 1000000
  },
  "values": [{"x": 32767, "y": 1000000}, {"x": -32768, "y": -1000000}]
}"""
    if ret != expected:
        print(ret)
    assert ret == expected


###############################################################################
# Test arrayoption


def test_gdalmdiminfo_lib_arrayoption():

    if gdal.GetDriverByName('netCDF') is None:
        pytest.skip('netCDF driver not enabled')

    ret = gdal.MultiDimInfo('../gdrivers/data/netcdf/with_bounds.nc')
    assert len(ret['arrays']) == 2

    ret = gdal.MultiDimInfo('../gdrivers/data/netcdf/with_bounds.nc',
                            arrayoptions = ['SHOW_BOUNDS=NO'])
    assert len(ret['arrays']) == 1


###############################################################################

def test_gdalmdiminfo_lib_int64():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", "my_type", "my_direction", 1)
    ar = rg.CreateMDArray("ar", [ dim0 ], gdal.ExtendedDataType.Create(gdal.GDT_Int64))
    assert ar.Write(struct.pack('q', -10000000000)) == gdal.CE_None

    ret = gdal.MultiDimInfo(ds, detailed = True)
    assert ret['arrays']['ar']['values'] == [-10000000000]


###############################################################################

def test_gdalmdiminfo_lib_uint64():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.CreateMultiDimensional('')
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", "my_type", "my_direction", 1)
    ar = rg.CreateMDArray("ar", [ dim0 ], gdal.ExtendedDataType.Create(gdal.GDT_UInt64))
    assert ar.Write(struct.pack('Q', 10000000000)) == gdal.CE_None

    ret = gdal.MultiDimInfo(ds, detailed = True)
    assert ret['arrays']['ar']['values'] == [10000000000]
