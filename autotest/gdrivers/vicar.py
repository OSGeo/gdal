#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for PDS driver.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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
from osgeo import ogr
import json
import struct

import gdaltest
import pytest

###############################################################################
# Read truncated VICAR file


def test_vicar_1():

    tst = gdaltest.GDALTest('VICAR', 'vicar/test_vicar_truncated.bin', 1, 0)
    expected_prj = """PROJCS["SINUSOIDAL MARS",
    GEOGCS["GCS_MARS",
        DATUM["D_MARS",
            SPHEROID["MARS",3396000,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Sinusoidal"],
    PARAMETER["longitude_of_center",137],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["meter",1]]]"""
    tst.testOpen(check_prj=expected_prj, skip_checksum=True)

    ds = gdal.Open('data/vicar/test_vicar_truncated.bin')
    expected_gt = (-53985.0, 25.0, 0.0, -200805.0, 0.0, -25.0)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)

    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    assert ds.GetRasterBand(1).GetScale() == pytest.approx(2.34, abs=1e-5)
    assert ds.GetRasterBand(1).GetOffset() == pytest.approx(4.56, abs=1e-5)

    expected_md = {'DLRTO8.REFLECTANCE_OFFSET': '4.56', 'PRODUCT_TYPE': 'IMAGE', 'M94_ORBIT.STOP_TIME': 'stop_time', 'FILE.EVENT_TYPE': 'EVENT_TYPE', 'M94_CAMERAS.MACROPIXEL_SIZE': '1', 'M94_INSTRUMENT.DETECTOR_ID': 'MEX_HRSC_NADIR', 'HRORTHO.SPICE_FILE_NAME': 'SPICE_FILE_NAME', 'DLRTO8.RADIANCE_SCALING_FACTOR': '1.23', 'HRORTHO.GEOMETRIC_CALIB_FILE_NAME': 'calib_file_name', 'HRORTHO.EXTORI_FILE_NAME': "extori'_file_name", 'M94_INSTRUMENT.MISSION_PHASE_NAME': 'MISSION_PHASE_NAME', 'HRCONVER.MISSING_FRAMES': '0', 'DLRTO8.RADIANCE_OFFSET': '1.23', 'HRCONVER.OVERFLOW_FRAMES': '0', 'SPACECRAFT_NAME': 'MARS EXPRESS', 'HRFOOT.BEST_GROUND_SAMPLING_DISTANCE': '1.23', 'M94_ORBIT.START_TIME': 'start_time', 'HRORTHO.DTM_NAME': 'dtm_name', 'DLRTO8.REFLECTANCE_SCALING_FACTOR': '2.34', 'HRCONVER.ERROR_FRAMES': '1'}
    md = ds.GetMetadata()
    if len(md) != len(expected_md):
        print(sorted(md.keys()))
        pytest.fail(sorted(expected_md.keys()))
    for key in expected_md:
        assert md[key] == expected_md[key]

    assert ds.GetMetadataDomainList() == ['', 'json:VICAR']
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    assert lbl['LBLSIZE'] == 9680
    assert lbl['FORMAT'] == 'BYTE'
    assert lbl['PROPERTY']['M94_ORBIT']['ASCENDING_NODE_LONGITUDE'] == 118.46
    assert lbl['PROPERTY']['M94_ORBIT']['SPACECRAFT_ORIENTATION'] == [0.0, -1.0, 0.0]
    assert lbl['TASK']['HRCONVER']['SPICE_FILE_NAME'] == ['foo']
    assert lbl['TASK']['HRORTHO']['EXTORI_FILE_NAME'] == "extori'_file_name"

read_datatypes_lists = [
    ('vicar_byte', gdal.GDT_Byte, 129),
    ('vicar_int16', gdal.GDT_Int16, 129),
    ('vicar_bigendian_int16', gdal.GDT_Int16, 129),
    ('vicar_int32', gdal.GDT_Int32, 129),
    ('vicar_float32_bsq', gdal.GDT_Float32, 123),
    ('vicar_float32_bil', gdal.GDT_Float32, 123),
    ('vicar_float32_bip', gdal.GDT_Float32, 123),
    ('vicar_bigendian_float32', gdal.GDT_Float32, 129),
    ('vicar_float64', gdal.GDT_Float64, 129),
    ('vicar_cfloat32', gdal.GDT_CFloat32, 148),
    ('vicar_vax_float32', gdal.GDT_Float32, 129),
    ('vicar_vax_float64', gdal.GDT_Float64, 129),
    ('vicar_vax_cfloat32', gdal.GDT_CFloat32, 226),
]

@pytest.mark.parametrize(
    'filename,dt,checksum',
    read_datatypes_lists,
    ids=[tup[0] for tup in read_datatypes_lists],
)
def test_vicar_read_datatypes(filename, dt, checksum):

    ds = gdal.Open('data/vicar/%s.vic' % filename)
    assert ds.GetLayerCount() == 0
    assert not ds.GetLayer(0)
    b = ds.GetRasterBand(1)
    assert b.DataType == dt
    assert b.Checksum() == checksum
    b = None
    ds = None

    gdal.FileFromMemBuffer('/vsimem/test.vic', open('data/vicar/%s.vic' % filename, 'rb').read())
    ds = gdal.Open('/vsimem/test.vic', gdal.GA_Update)
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ds.ReadRaster())
    ds = None
    ds = gdal.Open('/vsimem/test.vic')
    assert ds.GetRasterBand(1).Checksum() == checksum
    ds = None
    gdal.Unlink('/vsimem/test.vic')


def test_vicar_read_binary_prefix():

    ds = gdal.OpenEx('data/vicar/vicar_binary_prefix.vic')
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr
    assert not lyr.TestCapability('')
    f = lyr.GetNextFeature()
    assert f
    assert json.loads(f.ExportToJson()) == {"geometry": None, "type": "Feature", "properties": {"short": -32768, "int": -2147483648, "unsigned_char": 255, "float": 1.25, "double": 3.25, "unsigned_int": 4294967295, "unsigned_short": 65535}, "id": 0}
    assert not lyr.GetNextFeature()
    lyr.ResetReading()
    assert lyr.GetNextFeature()
    ds = None

    assert ogr.Open('data/vicar/vicar_binary_prefix.vic')
    assert not ogr.Open('data/vicar/vicar_byte.vic')


def test_vicar_create():

    filename = '/vsimem/test.vic'
    md = {
        'LBLSIZE': 1234,
        'BLTYPE': 'foo',
        'PROPERTY':
        {
            'MYPROP':
            {
                'INT': 1,
                'INT64': 1234567890123,
                'REAL': 1.25,
                'ARRAY_INT': [1,2],
                'ARRAY_STRING': ['a','b'],
                'STRING': "eh'eh"
            },
            'APPROX':
            {
                'NULL': None,
                'BOOL_TRUE': True,
                'BOOL_FALSE': False,
                'OBJ': {"a": "b"}
            }
        },
        'TASK':
        {
            'GEN':
            {
                'USER': 'even',
                'DAT_TIM': 'Thu Sep 24 17:31:50 1992',
                'OTHER_PROP': 'foo'
            }
        }
    }
    src_ds = gdal.Open('data/rgbsmall.tif')
    ds = gdal.GetDriverByName('VICAR').Create(
        filename, src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, gdal.GDT_Byte)
    ds.SetMetadata([json.dumps(md)], "json:VICAR")
    ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_ds.ReadRaster())
    ds = None
    assert not gdal.VSIStatL(filename + '.aux.xml')

    ds = gdal.Open(filename)
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount)] == \
        [src_ds.GetRasterBand(i+1).Checksum() for i in range(src_ds.RasterCount)]
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    assert lbl['LBLSIZE'] == 600
    assert lbl['BLTYPE'] == 'foo'
    assert lbl['PROPERTY']['MYPROP'] == md['PROPERTY']['MYPROP']
    assert lbl['PROPERTY']['APPROX'] == { 'NULL': 'NULL', 'BOOL_TRUE': 1, 'BOOL_FALSE': 0, 'OBJ': '{"a":"b"}' }
    assert lbl['TASK'] == md['TASK']

    filename2 = '/vsimem/test2.vic'
    assert gdal.GetDriverByName('VICAR').CreateCopy(filename2, ds)
    assert not gdal.VSIStatL(filename2 + '.aux.xml')
    ds = None

    ds = gdal.Open(filename2)
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount)] == \
        [src_ds.GetRasterBand(i+1).Checksum() for i in range(src_ds.RasterCount)]
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    assert lbl['LBLSIZE'] == 600
    assert lbl['BLTYPE'] == 'foo'
    assert lbl['PROPERTY']['MYPROP'] == md['PROPERTY']['MYPROP']
    assert lbl['PROPERTY']['APPROX'] == { 'NULL': 'NULL', 'BOOL_TRUE': 1, 'BOOL_FALSE': 0, 'OBJ': '{"a":"b"}' }
    assert lbl['TASK'] == md['TASK']
    ds = None

    gdal.GetDriverByName('VICAR').Delete(filename)
    gdal.GetDriverByName('VICAR').Delete(filename2)

create_datatypes_lists = [
    'vicar_byte',
    'vicar_int16',
    'vicar_bigendian_float32',
    'vicar_float64',
    'vicar_cfloat32',
]

@pytest.mark.parametrize(
    'filename',
    create_datatypes_lists
)
def test_vicar_create_all_data_types(filename):
    dstfilename = '/vsimem/test.vic'
    src_ds = gdal.Open('data/vicar/' + filename + '.vic')
    assert gdal.GetDriverByName('VICAR').CreateCopy(dstfilename, src_ds)
    ds = gdal.Open(dstfilename)
    assert ds.GetRasterBand(1).DataType == src_ds.GetRasterBand(1).DataType
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.GetDriverByName('VICAR').Delete(dstfilename)


def test_vicar_create_label_option_as_inline_value():

    filename = '/vsimem/test.vic'
    assert gdal.GetDriverByName('VICAR').Create(
        filename, 1, 1, 1, gdal.GDT_Byte, options = ['LABEL={"BLTYPE":"foo"}'])
    ds = gdal.Open(filename)
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    assert lbl['BLTYPE'] == 'foo'
    ds = None
    gdal.GetDriverByName('VICAR').Delete(filename)


def test_vicar_create_label_option_as_inline_value_error():

    filename = '/vsimem/test.vic'
    with gdaltest.error_handler():
        assert not gdal.GetDriverByName('VICAR').Create(
            filename, 1, 1, 1, gdal.GDT_Byte, options = ['LABEL={error'])
    gdal.Unlink(filename)


def test_vicar_create_label_option_as_filename():

    filename = '/vsimem/test.vic'
    json_filename = '/vsimem/test.json'
    gdal.FileFromMemBuffer(json_filename, '{"BLTYPE":"foo"}')
    assert gdal.GetDriverByName('VICAR').Create(
        filename, 1, 1, 1, gdal.GDT_Byte, options = ['LABEL=' + json_filename])
    gdal.Unlink(json_filename)
    ds = gdal.Open(filename)
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    assert lbl['BLTYPE'] == 'foo'
    ds = None

    gdal.GetDriverByName('VICAR').Delete(filename)


def test_vicar_create_label_option_as_filename_error():

    filename = '/vsimem/test.vic'
    json_filename = '/vsimem/test.json'
    gdal.FileFromMemBuffer(json_filename, 'error')
    with gdaltest.error_handler():
        assert not gdal.GetDriverByName('VICAR').Create(
            filename, 1, 1, 1, gdal.GDT_Byte, options = ['LABEL=' + json_filename])
    gdal.Unlink(json_filename)
    gdal.Unlink(filename)


@pytest.mark.parametrize("georef_format", ["MIPL", "GEOTIFF"])
def test_vicar_create_georeferencing(georef_format):

    src_ds = gdal.Open('data/vicar/test_vicar_truncated.bin')
    filename = '/vsimem/test.vic'
    ds = gdal.GetDriverByName('VICAR').Create(filename, src_ds.RasterXSize, src_ds.RasterYSize,
                                              options = ['GEOREF_FORMAT=' + georef_format])
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    ds.SetSpatialRef(src_ds.GetSpatialRef())
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    if georef_format == 'MIPL':
        assert 'MAP' in lbl['PROPERTY']
        assert 'GEOTIFF' not in lbl['PROPERTY']
    else:
        assert 'GEOTIFF' in lbl['PROPERTY']
        assert 'MAP' not in lbl['PROPERTY']

    filename2 = '/vsimem/test2.vic'
    assert gdal.GetDriverByName('VICAR').CreateCopy(filename2, ds,
                                                    options = ['GEOREF_FORMAT=' + georef_format])
    ds = None

    ds = gdal.Open(filename2)
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    if georef_format == 'MIPL':
        assert 'MAP' in lbl['PROPERTY']
        assert 'GEOTIFF' not in lbl['PROPERTY']
    else:
        assert 'GEOTIFF' in lbl['PROPERTY']
        assert 'MAP' not in lbl['PROPERTY']
    ds = None

    gdal.GetDriverByName('VICAR').Delete(filename)
    gdal.GetDriverByName('VICAR').Delete(filename2)


compressed_datasets_lists = [
    ('vicar_byte_basic', gdal.GDT_Byte, 4672), # BASIC compression
    ('vicar_byte_basic2', gdal.GDT_Byte, 4672), # BASIC2 compression
    ('vicar_int16_basic2', gdal.GDT_Int16, 4672), # BASIC2 compression
    ('vicar_all_ones_basic2', gdal.GDT_Byte, 34464), # BASIC2 compression
]

@pytest.mark.parametrize(
    'filename,dt,checksum',
    compressed_datasets_lists,
    ids=[tup[0] for tup in compressed_datasets_lists],
)
def test_vicar_read_compressed_datasets(filename, dt, checksum):

    ds = gdal.Open('data/vicar/%s.vic' % filename)
    assert ds.GetLayerCount() == 0
    assert not ds.GetLayer(0)
    b = ds.GetRasterBand(1)
    assert b.DataType == dt
    assert b.Checksum() == checksum


def test_vicar_write_basic():

    src_ds = gdal.Open('data/vicar/vicar_byte_basic.vic')

    filename= '/vsimem/test.vic'
    assert gdal.GetDriverByName('VICAR').CreateCopy(filename, src_ds, options = ['COMPRESS=BASIC'])
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('COMPRESS', 'IMAGE_STRUCTURE') == 'BASIC'
    assert ds.GetRasterBand(1).Checksum() == 4672
    lbl = ds.GetMetadata_List('json:VICAR')[0]
    lbl = json.loads(lbl)
    assert lbl['EOCI1'] == 1040
    assert lbl['EOCI2'] == 0
    ds = None
    gdal.GetDriverByName('VICAR').Delete(filename)


def test_vicar_write_basic2():

    src_ds = gdal.Open('data/vicar/vicar_byte_basic.vic')

    filename= '/vsimem/test.vic'
    assert gdal.GetDriverByName('VICAR').CreateCopy(filename, src_ds, options = ['COMPRESS=BASIC2'])
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('COMPRESS', 'IMAGE_STRUCTURE') == 'BASIC2'
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.GetDriverByName('VICAR').Delete(filename)


def test_vicar_write_basic2_int16():

    src_ds = gdal.Open('data/vicar/vicar_int16_basic2.vic')

    filename= '/vsimem/test.vic'
    assert gdal.GetDriverByName('VICAR').CreateCopy(filename, src_ds, options = ['COMPRESS=BASIC2'])
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('COMPRESS', 'IMAGE_STRUCTURE') == 'BASIC2'
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.GetDriverByName('VICAR').Delete(filename)


def test_vicar_write_basic2_all_ones():

    src_ds = gdal.Open('data/vicar/vicar_all_ones_basic2.vic')

    filename= '/vsimem/test.vic'
    assert gdal.GetDriverByName('VICAR').CreateCopy(filename, src_ds, options = ['COMPRESS=BASIC2'])
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('COMPRESS', 'IMAGE_STRUCTURE') == 'BASIC2'
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.GetDriverByName('VICAR').Delete(filename)


def test_vicar_write_compression_errors():
    filename= '/vsimem/test.vic'
    with gdaltest.error_handler():
        # Only single-band supported
        assert not gdal.GetDriverByName('VICAR').Create(filename, 1, 1, 2, options = ['COMPRESS=BASIC'])
        # Unknown compression method
        assert not gdal.GetDriverByName('VICAR').Create(filename, 1, 1, 1, options = ['COMPRESS=UNKNOWN'])
        # Only integer data types supported
        assert not gdal.GetDriverByName('VICAR').Create(filename, 1, 1, 1, gdal.GDT_Float32, options = ['COMPRESS=BASIC'])
        # Too many lines
        assert not gdal.GetDriverByName('VICAR').Create(filename, 1, 1000*1000*1000, 1, options = ['COMPRESS=BASIC'])
        # Too many columns
        assert not gdal.GetDriverByName('VICAR').Create(filename, 2000*1000*1000, 1, 1, options = ['COMPRESS=BASIC'])

    ds = gdal.GetDriverByName('VICAR').Create(filename, 1, 2, 1, options = ['COMPRESS=BASIC'])
    # Non sequential writing of lines
    ds.WriteRaster(0, 1, 1, 1, 'x')
    with gdaltest.error_handler():
        ds.FlushCache()
    assert gdal.GetLastErrorMsg() != ''
    ds = None
    gdal.Unlink(filename)


def test_vicar_open_from_pds3():

    gdal.FileFromMemBuffer('/vsimem/test',
                           """PDS_VERSION_ID                       = "PDS3"
RECORD_BYTES                         = 1
^IMAGE_HEADER                        = 489
^IMAGE                               = 757
OBJECT                               = IMAGE
    BANDS                            = 1
    BAND_STORAGE_TYPE                = "BAND SEQUENTIAL"
    LINES                            = 1
    LINE_SAMPLES                     = 1
    SAMPLE_BITS                      = 8
END_OBJECT                           = IMAGE
END
LBLSIZE=268             FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=20480  DIM=3  EOL=0  RECSIZE=1  ORG='BSQ'  NL=1  NS=1  NB=1  N1=1  N2=1  N3=1  N4=0  NBB=0  NLB=0  HOST='X86-64-LINX'  INTFMT='LOW'  REALFMT='RIEEE'  BHOST='VAX-VMS'  BINTFMT='LOW'  BREALFMT='VAX'  BLTYPE=''
x
""")

    ds = gdal.Open('/vsimem/test')
    assert ds
    assert ds.GetDriver().ShortName == 'PDS'
    assert struct.unpack('B', ds.GetRasterBand(1).ReadRaster())[0] == ord('x')

    with gdaltest.config_option('GDAL_TRY_PDS3_WITH_VICAR', 'YES'):
        ds = gdal.Open('/vsimem/test')
    assert ds
    assert ds.GetDriver().ShortName == 'VICAR'
    assert struct.unpack('B', ds.GetRasterBand(1).ReadRaster())[0] == ord('x')
