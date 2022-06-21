#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GeoTIFF format.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import copy
import math
import os
import sys
import shutil
import struct
from osgeo import gdal
from osgeo import osr
import pytest

import gdaltest
from test_py_scripts import samples_path

###############################################################################


def _check_cog(filename, check_tiled=True, full_check=False):

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    import validate_cloud_optimized_geotiff
    try:
        _, errors, _ = validate_cloud_optimized_geotiff.validate(filename, check_tiled=check_tiled, full_check=full_check)
        assert not errors, 'validate_cloud_optimized_geotiff failed'
    except OSError:
        pytest.fail('validate_cloud_optimized_geotiff failed')

###############################################################################
# Get the GeoTIFF driver, and verify a few things about it.


def test_tiff_write_1():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    assert gdaltest.tiff_drv is not None, 'GTiff driver not found!'

    drv_md = gdaltest.tiff_drv.GetMetadata()
    assert drv_md['DMD_MIMETYPE'] == 'image/tiff', 'mime type is wrong'

###############################################################################
# Create a simple file by copying from an existing one.


def test_tiff_write_2():

    src_ds = gdal.Open('data/cfloat64.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_2.tif', src_ds)

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 5028, 'Didnt get expected checksum on still-open file'

    bnd = None
    new_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open('tmp/test_2.tif')
    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 5028, 'Didnt get expected checksum on reopened file'

    assert bnd.ComputeRasterMinMax() == (74.0, 255.0), \
        'ComputeRasterMinMax() returned wrong value'

    bnd = None
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_2.tif')

###############################################################################
# Create a simple file by copying from an existing one.


def test_tiff_write_3():

    src_ds = gdal.Open('data/utmsmall.tif')

    options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32']

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_3.tif', src_ds,
                                          options=options)

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 50054, 'Didnt get expected checksum on still-open file'

    bnd = None
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_3.tif')

###############################################################################
# Create a tiled file.


def test_tiff_write_4():

    np = pytest.importorskip('numpy')

    options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32']

    new_ds = gdaltest.tiff_drv.Create('tmp/test_4.tif', 40, 50, 3,
                                      gdal.GDT_Byte, options)

    data_red = np.zeros((50, 40), dtype=np.uint8)
    data_green = np.zeros((50, 40), dtype=np.uint8)
    data_blue = np.zeros((50, 40), dtype=np.uint8)

    for y in range(50):
        for x in range(40):
            data_red[y][x] = x
            data_green[y][x] = y
            data_blue[y][x] = x + y

    new_ds.GetRasterBand(1).WriteArray(data_red)
    new_ds.GetRasterBand(2).WriteArray(data_green)
    new_ds.GetRasterBand(3).WriteArray(data_blue)

    gt = (0.0, 1.0, 0.0, 50.0, 0.0, -1.0)
    new_ds.SetGeoTransform(gt)

    assert new_ds.GetRasterBand(1).Checksum() == 21577 and new_ds.GetRasterBand(2).Checksum() == 20950 and new_ds.GetRasterBand(3).Checksum() == 23730, \
        'Wrong checksum.'

    assert gt == new_ds.GetGeoTransform(), 'Wrong geotransform.'

    new_ds.SetMetadata({'TEST_KEY': 'TestValue <>'})

    new_ds = None

    new_ds = gdal.Open('tmp/test_4.tif')

    assert new_ds.GetRasterBand(1).Checksum() == 21577 and new_ds.GetRasterBand(2).Checksum() == 20950 and new_ds.GetRasterBand(3).Checksum() == 23730, \
        'Wrong checksum (2).'

    assert gt == new_ds.GetGeoTransform(), 'Wrong geotransform(2).'

    nd = new_ds.GetRasterBand(1).GetNoDataValue()
    assert nd is None, 'Got unexpected nodata value.'

    md_dict = new_ds.GetMetadata()
    assert md_dict['TEST_KEY'] == 'TestValue <>', 'Missing metadata'

    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_4.tif')

###############################################################################
# Write a file with GCPs.


def test_tiff_write_5():

    src_ds = gdal.Open('data/gcps.vrt')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_5.tif', src_ds)

    assert (new_ds.GetGCPProjection().find(
            'AUTHORITY["EPSG","26711"]') != -1), 'GCP Projection not set properly.'

    gcps = new_ds.GetGCPs()
    assert len(gcps) == 4, 'GCP count wrong.'

    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_5.tif')

    # Test SetGCPs on a new GTiff
    new_ds = gdaltest.tiff_drv.Create('tmp/test_5.tif', 10, 10, 1)
    new_ds.SetGCPs(gcps, src_ds.GetGCPProjection())
    new_ds = None

    new_ds = gdal.Open('tmp/test_5.tif')
    gcps = new_ds.GetGCPs()
    assert len(gcps) == 4, 'GCP count wrong.'
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_5.tif')

###############################################################################
# Test a mixture of reading and writing on a DEFLATE compressed file.

def test_tiff_write_6():

    options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32',
               'COMPRESS=DEFLATE', 'PREDICTOR=2']
    ds = gdaltest.tiff_drv.Create('tmp/test_6.tif', 200, 200, 1,
                                  gdal.GDT_Byte, options)

    # make a 32x32 byte buffer
    buf = b''.join(struct.pack('B', v) for v in range(32)) * 32

    ds.WriteRaster(0, 0, 32, 32, buf, buf_type=gdal.GDT_Byte)
    ds.FlushCache()
    ds.WriteRaster(32, 0, 32, 32, buf, buf_type=gdal.GDT_Byte)
    ds.FlushCache()
    buf_read = ds.ReadRaster(0, 0, 32, 32, buf_type=gdal.GDT_Byte)

    if buf_read != buf:
        gdaltest.tiff_write_6_failed = True
        pytest.fail('did not get back expected data.')

    ds = None

    ds = gdal.Open('tmp/test_6.tif')
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'DEFLATE'
    assert ds.GetMetadataItem('PREDICTOR', 'IMAGE_STRUCTURE') == '2'
    ds = None

    gdaltest.tiff_write_6_failed = False
    gdaltest.tiff_drv.Delete('tmp/test_6.tif')

###############################################################################
# Test a mixture of reading and writing on a LZW compressed file.

def test_tiff_write_7():

    options = ['TILED=YES', 'COMPRESS=LZW', 'PREDICTOR=2']
    ds = gdaltest.tiff_drv.Create('tmp/test_7.tif', 200, 200, 1,
                                  gdal.GDT_Byte, options)

    # make a 32x32 byte buffer
    buf = b''.join(struct.pack('B', v) for v in range(32)) * 32

    ds.WriteRaster(0, 0, 32, 32, buf, buf_type=gdal.GDT_Byte)
    ds.FlushCache()
    ds.WriteRaster(32, 0, 32, 32, buf, buf_type=gdal.GDT_Byte)
    ds.FlushCache()
    buf_read = ds.ReadRaster(0, 0, 32, 32, buf_type=gdal.GDT_Byte)

    assert buf_read == buf, 'did not get back expected data.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/test_7.tif')

###############################################################################
# Test a mixture of reading and writing on a PACKBITS compressed file.


def test_tiff_write_8():

    options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32', 'COMPRESS=PACKBITS']
    ds = gdaltest.tiff_drv.Create('tmp/test_8.tif', 200, 200, 1,
                                  gdal.GDT_Byte, options)

    # make a 32x32 byte buffer
    buf = b''.join(struct.pack('B', v) for v in range(32)) * 32

    ds.WriteRaster(0, 0, 32, 32, buf, buf_type=gdal.GDT_Byte)
    ds.FlushCache()
    ds.WriteRaster(32, 0, 32, 32, buf, buf_type=gdal.GDT_Byte)
    ds.FlushCache()

    buf_read = ds.ReadRaster(0, 0, 32, 32, buf_type=gdal.GDT_Byte)

    assert buf_read == buf, 'did not get back expected data.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/test_8.tif')

###############################################################################
# Create a simple file by copying from an existing one.


def test_tiff_write_9():

    src_ds = gdal.Open('data/byte.tif')
    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_9.tif', src_ds,
                                          options=['NBITS=5'])
    with gdaltest.error_handler():
        new_ds = None

    new_ds = gdal.Open('tmp/test_9.tif')
    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 5287, 'Didnt get expected checksum on reopened file'

    bnd = None
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_9.tif')

###############################################################################
# 1bit file but with band interleaving, and odd size (not multiple of 8) #1957


def test_tiff_write_10():

    ut = gdaltest.GDALTest('GTiff', 'oddsize_1bit2b.tif', 2, 5918,
                           options=['NBITS=1', 'INTERLEAVE=BAND'])
    return ut.testCreate(out_bands=2)

###############################################################################
# Simple 1 bit file, treated through the GTiffBitmapBand class.


def test_tiff_write_11():

    ut = gdaltest.GDALTest('GTiff', 'oddsize1bit.tif', 1, 5918,
                           options=['NBITS=1', 'COMPRESS=CCITTFAX4'])
    return ut.testCreateCopy()

###############################################################################
# Read JPEG Compressed YCbCr subsampled image.


def test_tiff_write_12():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    ds = gdal.Open('data/sasha.tif')
    cs = ds.GetRasterBand(3).Checksum()
    assert cs == 31952 or cs == 30145

###############################################################################
# Write JPEG Compressed YCbCr subsampled image.


def test_tiff_write_13():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open('data/sasha.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/sasha.tif', src_ds, options=['PROFILE=BASELINE',
                                                                        'TILED=YES',
                                                                        'COMPRESS=JPEG',
                                                                        'PHOTOMETRIC=YCBCR',
                                                                        'JPEG_QUALITY=31'])
    ds = None

    ds = gdal.Open('tmp/sasha.tif')
    cs = ds.GetRasterBand(3).Checksum()
    ds = None

    size = os.stat('tmp/sasha.tif').st_size

    gdaltest.tiff_drv.Delete('tmp/sasha.tif')
    assert cs in (17347, 14445,
                  14135, # libjpeg 9e
                 )

    if md['LIBTIFF'] == 'INTERNAL':
        # 22816 with libjpeg-6b or libjpeg-turbo
        # 22828 with libjpeg-9d
        assert size <= 22828, 'fail: bad size'


###############################################################################
# Test creating an in memory copy.


def test_tiff_write_14():

    tst = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672)

    return tst.testCreateCopy(vsimem=1)


###############################################################################
# Test that we can restrict metadata and georeferencing in the output
# file using the PROFILE creation option with CreateCopy()

def test_tiff_write_15():

    ds_in = gdal.Open('data/byte.vrt')

    ds = gdaltest.tiff_drv.CreateCopy('tmp/tw_15.tif', ds_in, options=['PROFILE=BASELINE'])

    ds_in = None
    ds = None

    ds = gdal.Open('tmp/tw_15.tif')

    md = ds.GetMetadata()
    assert 'test' in md, 'Metadata absent from .aux.xml file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' in md, 'Metadata absent from .aux.xml file.'

    ds = None

    gdal.Unlink('tmp/tw_15.tif.aux.xml')

    ds = gdal.Open('tmp/tw_15.tif')

    assert ds.GetGeoTransform() == (0.0, 1.0, 0.0, 0.0, 0.0, 1.0), \
        'Got wrong geotransform, profile ignored?'

    md = ds.GetMetadata()
    assert 'test' not in md, 'Metadata written to BASELINE file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' not in md, 'Metadata written to BASELINE file.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tw_15.tif')

###############################################################################
# Test that we can restrict metadata and georeferencing in the output
# file using the PROFILE creation option with Create()


def test_tiff_write_16():

    ds_in = gdal.Open('data/byte.vrt')

    ds = gdaltest.tiff_drv.Create('tmp/tw_16.tif', 20, 20, gdal.GDT_Byte,
                                  options=['PROFILE=BASELINE'])

    ds.SetMetadata({'test': 'testvalue'})
    ds.GetRasterBand(1).SetMetadata({'testBand': 'testvalueBand'})

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    ds.SetSpatialRef(srs)
    ds.SetGeoTransform((10, 5, 0, 30, 0, -5))

    data = ds_in.ReadRaster(0, 0, 20, 20)
    ds.WriteRaster(0, 0, 20, 20, data)

    ds_in = None
    ds = None

    # Check first from PAM
    assert gdal.VSIStatL('tmp/tw_16.tif.aux.xml') is not None
    ds = gdal.Open('tmp/tw_16.tif')
    assert ds.GetGeoTransform() == (10, 5, 0, 30, 0, -5)
    assert ds.GetSpatialRef() is not None
    assert ds.GetSpatialRef().GetAuthorityCode(None) == '4326'

    md = ds.GetMetadata()
    assert 'test' in md, 'Metadata absent from .aux.xml file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' in md, 'Metadata absent from .aux.xml file.'

    ds = None
    gdal.Unlink('tmp/tw_16.tif.aux.xml')

    ds = gdal.Open('tmp/tw_16.tif')
    assert ds.GetGeoTransform() == (0.0, 1.0, 0.0, 0.0, 0.0, 1.0), \
        'Got wrong geotransform, profile ignored?'
    assert ds.GetSpatialRef() is None

    md = ds.GetMetadata()
    assert 'test' not in md, 'Metadata written to BASELINE file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' not in md, 'Metadata written to BASELINE file.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tw_16.tif')

###############################################################################
# Test writing a TIFF with an RPC tag.


def test_tiff_write_17():

    # Translate RPC controlled data to GeoTIFF.

    ds_in = gdal.Open('data/rpc.vrt')
    rpc_md = ds_in.GetMetadata('RPC')

    tmpfilename = '/vsimem/tiff_write_17.tif'
    ds = gdaltest.tiff_drv.CreateCopy(tmpfilename, ds_in)

    ds_in = None
    ds = None

    # Ensure there is no .aux.xml file which might hold the RPC.
    assert not gdal.VSIStatL(tmpfilename + '.aux.xml'), \
        'unexpectedly found.aux.xml file'

    # confirm there is no .rpb file created by default.
    assert not gdal.VSIStatL(tmpfilename + '.RPB'), 'unexpectedly found .RPB file'

    # confirm there is no _rpc.txt file created by default.
    assert not gdal.VSIStatL(tmpfilename + '_RPC.TXT'), \
        'unexpectedly found _RPC.TXT file'

    # Open the dataset, and confirm the RPC data is still intact.
    ds = gdal.Open(tmpfilename)
    assert gdaltest.rpcs_equal(ds.GetMetadata('RPC'), rpc_md)
    ds = None

    # Modify the RPC
    modified_rpc = copy.copy(rpc_md)
    modified_rpc['LINE_OFF'] = '123456'

    ds = gdal.Open(tmpfilename, gdal.GA_Update)
    ds.SetMetadata(modified_rpc, 'RPC')
    ds = None

    ds = gdal.Open(tmpfilename)
    assert gdaltest.rpcs_equal(ds.GetMetadata('RPC'), modified_rpc)
    ds = None

    # Unset the RPC
    ds = gdal.Open(tmpfilename, gdal.GA_Update)
    ds.SetMetadata(None, 'RPC')
    ds = None

    ds = gdal.Open(tmpfilename)
    assert not ds.GetMetadata('RPC'), 'got RPC, but was not expected'
    ds = None

    gdaltest.tiff_drv.Delete(tmpfilename)

###############################################################################
# Test that above test still work with the optimization in the GDAL_DISABLE_READDIR_ON_OPEN
# case (#3996)


def test_tiff_write_17_disable_readdir():
    oldval = gdal.GetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN')
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'TRUE')
    ret = test_tiff_write_17()
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', oldval)
    return ret

###############################################################################
# Test writing a TIFF with an RPB file and IMD file.


def test_tiff_write_18():

    # Translate RPC controlled data to GeoTIFF.

    ds_in = gdal.Open('data/rpc.vrt')
    rpc_md = ds_in.GetMetadata('RPC')

    gdaltest.tiff_drv.CreateCopy('tmp/tw_18.tif', ds_in,
                                      options=['PROFILE=BASELINE'])

    # Ensure there is no .aux.xml file which might hold the RPC.
    assert not gdal.VSIStatL('tmp/tm_18.tif.aux.xml'), \
        'unexpectedly found tm_18.tif.aux.xml file'

    # confirm there is an .rpb and .imd file.
    assert gdal.VSIStatL('tmp/tw_18.RPB') is not None, 'missing .RPB file.'
    assert gdal.VSIStatL('tmp/tw_18.IMD') is not None, 'missing .IMD file.'

    # confirm there is no _rpc.txt file created by default.
    assert not gdal.VSIStatL('tmp/tw_18_RPC.TXT'), 'unexpectedly found _RPC.TXT file'

    # Open the dataset, and confirm the RPC/IMD data is still intact.
    ds = gdal.Open('tmp/tw_18.tif')

    assert gdaltest.rpcs_equal(ds.GetMetadata('RPC'), rpc_md)

    imd_md = ds.GetMetadata('IMD')
    assert imd_md['version'] == '"R"' and imd_md['numColumns'] == '30324' and imd_md['IMAGE_1.sunEl'] == '39.7', \
        'IMD contents wrong?'

    ds = None

    # Test deferred loading with GetMetadataItem()
    ds = gdal.Open('tmp/tw_18.tif')
    assert ds.GetMetadataItem('LINE_OFF', 'RPC') == '16201', \
        "wrong value for GetMetadataItem('LINE_OFF', 'RPC')"
    assert ds.GetMetadataItem('version', 'IMD') == '"R"', \
        "wrong value for GetMetadataItem('version', 'IMD')"
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tw_18.tif')

    # Confirm IMD and RPC files are cleaned up.  If not likely the
    # file list functionality is not working properly.
    assert not gdal.VSIStatL('tmp/tw_18.RPB'), 'RPB did not get cleaned up.'

    assert not gdal.VSIStatL('tmp/tw_18.IMD'), 'IMD did not get cleaned up.'

    # Remove the RPC
    gdaltest.tiff_drv.CreateCopy('tmp/tw_18.tif', ds_in,
                                      options=['PROFILE=BASELINE'])
    ds = gdal.Open('tmp/tw_18.tif', gdal.GA_Update)
    ds.SetMetadata(None, 'RPC')
    ds = None
    assert not os.path.exists('tmp/tw_18.RPB'), 'RPB did not get removed'

    gdaltest.tiff_drv.Delete('tmp/tw_18.tif')

###############################################################################
# Test writing a IMD files with space in values


def test_tiff_write_imd_with_space_in_values():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1)
    ds.SetMetadataItem('foo.key', 'value with space', 'IMD')
    ds.SetMetadataItem('foo.key2', 'value with " double quote', 'IMD')
    ds.SetMetadataItem('foo.key3', "value with ' single quote", 'IMD')
    ds.SetMetadataItem('foo.key4', """value with " double and ' single quote""", 'IMD')
    ds.SetMetadataItem('foo.key5', 'value_with_;', 'IMD')
    ds.SetMetadataItem('foo.key6', 'regular_value', 'IMD')
    ds = None

    f = gdal.VSIFOpenL('/vsimem/out.IMD', 'rb')
    assert f
    data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)

    gdal.GetDriverByName('GTiff').Delete('/vsimem/out.tif')

    assert data == b'BEGIN_GROUP = foo\n\tkey = "value with space";\n\tkey2 = \'value with " double quote\';\n\tkey3 = "value with \' single quote";\n\tkey4 = "value with \'\' double and \' single quote";\n\tkey5 = "value_with_;";\n\tkey6 = regular_value;\nEND_GROUP = foo\nEND;\n'

###############################################################################
# Test that above test still work with the optimization in the GDAL_DISABLE_READDIR_ON_OPEN
# case (#3996)


def test_tiff_write_18_disable_readdir():
    oldval = gdal.GetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN')
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'TRUE')
    ret = test_tiff_write_18()
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', oldval)
    return ret

###############################################################################
# Test writing a TIFF with an _RPC.TXT


def test_tiff_write_rpc_txt():

    # Translate RPC controlled data to GeoTIFF.

    ds_in = gdal.Open('data/rpc.vrt')

    # Remove IMD before creating the TIFF to avoid creating an .IMD
    # since .IMD + _RPC.TXT is an odd combination
    # If the .IMD is found, we don't try reading _RPC.TXT
    ds_in_without_imd = gdal.GetDriverByName('VRT').CreateCopy('', ds_in)
    ds_in_without_imd.SetMetadata(None, 'IMD')

    rpc_md = ds_in.GetMetadata('RPC')

    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_rpc_txt.tif', ds_in_without_imd,
                                      options=['PROFILE=BASELINE', 'RPCTXT=YES'])

    ds_in = None
    ds = None

    # Ensure there is no .aux.xml file which might hold the RPC.
    try:
        os.remove('tmp/tiff_write_rpc_txt.tif.aux.xml')
    except OSError:
        pass

    # confirm there is no .RPB file created by default.
    assert not os.path.exists('tmp/tiff_write_rpc_txt.RPB')
    assert os.path.exists('tmp/tiff_write_rpc_txt_RPC.TXT')

    # Open the dataset, and confirm the RPC data is still intact.
    ds = gdal.Open('tmp/tiff_write_rpc_txt.tif')

    assert gdaltest.rpcs_equal(ds.GetMetadata('RPC'), rpc_md)

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_rpc_txt.tif')

    # Confirm _RPC.TXT file is cleaned up.  If not likely the
    # file list functionality is not working properly.
    assert not os.path.exists('tmp/tiff_write_rpc_txt_RPC.TXT')


###############################################################################
# Test writing a TIFF with an RPC in .aux.xml


def test_tiff_write_rpc_in_pam():

    ds_in = gdal.Open('data/rpc.vrt')
    rpc_md = ds_in.GetMetadata('RPC')

    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_rpc_in_pam.tif', ds_in,
                                      options=['PROFILE=BASELINE', 'RPB=NO'])

    ds_in = None
    ds = None

    # Ensure there is a .aux.xml file which might hold the RPC.
    try:
        os.stat('tmp/tiff_write_rpc_in_pam.tif.aux.xml')
    except OSError:
        pytest.fail('missing .aux.xml file.')

    # confirm there is no .RPB file created.
    assert not os.path.exists('tmp/tiff_write_rpc_txt.RPB')

    # Open the dataset, and confirm the RPC data is still intact.
    ds = gdal.Open('tmp/tiff_write_rpc_in_pam.tif')

    assert gdaltest.rpcs_equal(ds.GetMetadata('RPC'), rpc_md)

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_rpc_in_pam.tif')
###############################################################################
# Test the write of a pixel-interleaved image with NBITS = 7


def test_tiff_write_19():

    src_ds = gdal.Open('data/contig_strip.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/contig_strip_7.tif', src_ds,
                                          options=['NBITS=7', 'INTERLEAVE=PIXEL'])

    new_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open('tmp/contig_strip_7.tif')
    assert (new_ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum() and \
       new_ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum() and \
       new_ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(3).Checksum()), \
        'Didnt get expected checksum on reopened file'

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete('tmp/contig_strip_7.tif')

###############################################################################
# Test write and read of some TIFF tags
# Also test unsetting those tags (#5619)

def test_tiff_write_20():

    new_ds = gdaltest.tiff_drv.Create('tmp/tags.tif', 1, 1, 1)

    values = [('TIFFTAG_DOCUMENTNAME', 'document_name'),
              ('TIFFTAG_IMAGEDESCRIPTION', 'image_description'),
              ('TIFFTAG_SOFTWARE', 'software'),
              ('TIFFTAG_DATETIME', '2009/01/01 13:01:08'),
              # TODO: artitst?
              ('TIFFTAG_ARTIST', 'artitst'),
              ('TIFFTAG_HOSTCOMPUTER', 'host_computer'),
              ('TIFFTAG_COPYRIGHT', 'copyright'),
              ('TIFFTAG_XRESOLUTION', '100'),
              ('TIFFTAG_YRESOLUTION', '101'),
              ('TIFFTAG_RESOLUTIONUNIT', '2 (pixels/inch)'),
              ('TIFFTAG_MINSAMPLEVALUE', '1'),
              ('TIFFTAG_MAXSAMPLEVALUE', '2'),
             ]

    new_ds.SetMetadata(dict(values))

    new_ds = None

    # hopefully it's closed now!

    assert not os.path.exists('tmp/tags.tif.aux.xml')

    new_ds = gdal.Open('tmp/tags.tif')
    md = new_ds.GetMetadata()
    for item in values:
        assert item[0] in md, ('Could not find tag %s' % (item[0]))

        assert md[item[0]] == item[1], \
            ('For tag %s, got %s, expected %s' % (item[0], md[item[0]], item[1]))

    new_ds = None

    # Test just unsetting once, but leaving other unchanged
    ds = gdal.Open('tmp/tags.tif', gdal.GA_Update)
    ds.SetMetadataItem('TIFFTAG_SOFTWARE', None)
    ds = None

    assert not os.path.exists('tmp/tags.tif.aux.xml')

    ds = gdal.Open('tmp/tags.tif')
    assert ds.GetMetadataItem('TIFFTAG_SOFTWARE') is None, \
        ('expected unset TIFFTAG_SOFTWARE but got %s' % ds.GetMetadataItem('TIFFTAG_SOFTWARE'))
    assert ds.GetMetadataItem('TIFFTAG_DOCUMENTNAME') is not None, \
        'expected set TIFFTAG_DOCUMENTNAME but got None'
    ds = None

    # Test unsetting all the remaining items
    ds = gdal.Open('tmp/tags.tif', gdal.GA_Update)
    ds.SetMetadata({})
    ds = None

    ds = gdal.Open('tmp/tags.tif')
    got_md = ds.GetMetadata()
    ds = None

    assert got_md == {}, 'expected empty metadata list, but got some'

    gdaltest.tiff_drv.Delete('tmp/tags.tif')

###############################################################################
# Test RGBA images with TIFFTAG_EXTRASAMPLES=EXTRASAMPLE_ASSOCALPHA


def test_tiff_write_21():

    src_ds = gdal.Open('data/stefan_full_rgba.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/stefan_full_rgba.tif', src_ds)

    new_ds = None

    new_ds = gdal.Open('tmp/stefan_full_rgba.tif')
    assert new_ds.RasterCount == 4
    for i in range(4):
        assert new_ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == src_ds.GetRasterBand(i + 1).GetRasterColorInterpretation()
        assert new_ds.GetRasterBand(i + 1).Checksum() == src_ds.GetRasterBand(i + 1).Checksum()

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete('tmp/stefan_full_rgba.tif')

###############################################################################
# Test RGBA images with TIFFTAG_EXTRASAMPLES=EXTRASAMPLE_UNSPECIFIED


def test_tiff_write_22():

    src_ds = gdal.Open('data/stefan_full_rgba_photometric_rgb.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/stefan_full_rgba_photometric_rgb.tif', src_ds, options=['PHOTOMETRIC=RGB'])

    new_ds = None

    new_ds = gdal.Open('tmp/stefan_full_rgba_photometric_rgb.tif')
    assert new_ds.RasterCount == 4
    for i in range(4):
        assert new_ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == src_ds.GetRasterBand(i + 1).GetRasterColorInterpretation()
        assert new_ds.GetRasterBand(i + 1).Checksum() == src_ds.GetRasterBand(i + 1).Checksum()

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete('tmp/stefan_full_rgba_photometric_rgb.tif')

###############################################################################
# Test grey+alpha images with ALPHA=YES


def test_tiff_write_23():

    src_ds = gdal.Open('data/stefan_full_greyalpha.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/stefan_full_greyalpha.tif', src_ds, options=['ALPHA=YES'])

    new_ds = None

    new_ds = gdal.Open('tmp/stefan_full_greyalpha.tif')
    assert new_ds.RasterCount == 2
    for i in range(2):
        assert new_ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == src_ds.GetRasterBand(i + 1).GetRasterColorInterpretation()
        assert new_ds.GetRasterBand(i + 1).Checksum() == src_ds.GetRasterBand(i + 1).Checksum()

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete('tmp/stefan_full_greyalpha.tif')

###############################################################################
# Test grey+alpha images without ALPHA=YES


def test_tiff_write_24():

    src_ds = gdal.Open('data/stefan_full_greyalpha.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/stefan_full_greyunspecified.tif', src_ds)

    new_ds = None

    new_ds = gdal.Open('tmp/stefan_full_greyunspecified.tif')
    assert new_ds.RasterCount == 2
    for i in range(2):
        assert new_ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == src_ds.GetRasterBand(i + 1).GetRasterColorInterpretation()
        assert new_ds.GetRasterBand(i + 1).Checksum() == src_ds.GetRasterBand(i + 1).Checksum()

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete('tmp/stefan_full_greyunspecified.tif')

###############################################################################
# Read a CIELAB image to test the RGBA image TIFF interface


def test_tiff_write_25():

    src_ds = gdal.Open('data/cielab.tif')
    assert src_ds.RasterCount == 4
    assert src_ds.GetRasterBand(1).Checksum() == 6
    assert src_ds.GetRasterBand(2).Checksum() == 3
    assert src_ds.GetRasterBand(3).Checksum() == 0
    assert src_ds.GetRasterBand(4).Checksum() == 3
    assert src_ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand
    assert src_ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand
    assert src_ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand
    assert src_ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand
    src_ds = None


###############################################################################
# Test color table in a 8 bit image

def test_tiff_write_26():

    ds = gdaltest.tiff_drv.Create('tmp/ct8.tif', 1, 1, 1, gdal.GDT_Byte)

    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))

    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ct = None
    ds = None

    ds = gdal.Open('tmp/ct8.tif')

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert (ct.GetCount() == 256 and \
       ct.GetColorEntry(0) == (255, 255, 255, 255) and \
       ct.GetColorEntry(1) == (255, 255, 0, 255) and \
       ct.GetColorEntry(2) == (255, 0, 255, 255) and \
       ct.GetColorEntry(3) == (0, 255, 255, 255)), 'Wrong color table entry.'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/ct8.tif')

###############################################################################
# Test color table in a 16 bit image


def test_tiff_write_27():

    ds = gdaltest.tiff_drv.Create('tmp/ct16.tif', 1, 1, 1, gdal.GDT_UInt16)

    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))

    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ct = None
    ds = None

    ds = gdal.Open('tmp/ct16.tif')
    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/ct16_copy.tif', ds)
    del new_ds
    ds = None

    ds = gdal.Open('tmp/ct16_copy.tif')

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert (ct.GetCount() == 65536 and \
       ct.GetColorEntry(0) == (255, 255, 255, 255) and \
       ct.GetColorEntry(1) == (255, 255, 0, 255) and \
       ct.GetColorEntry(2) == (255, 0, 255, 255) and \
       ct.GetColorEntry(3) == (0, 255, 255, 255)), 'Wrong color table entry.'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/ct16.tif')
    gdaltest.tiff_drv.Delete('tmp/ct16_copy.tif')

###############################################################################
# Test SetRasterColorInterpretation on a 2 channel image


def test_tiff_write_28():

    ds = gdaltest.tiff_drv.Create('tmp/greyalpha.tif', 1, 1, 2)

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_Undefined

    ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_AlphaBand)

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_AlphaBand

    ds = None

    ds = gdal.Open('tmp/greyalpha.tif')

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    gdaltest.tiff_drv.Delete('tmp/greyalpha.tif')

###############################################################################
# Test SetRasterColorInterpretation on a 4 channel image


def test_tiff_write_29():

    # When creating a 4 channel image with PHOTOMETRIC=RGB,
    # TIFFTAG_EXTRASAMPLES=EXTRASAMPLE_UNSPECIFIED
    ds = gdaltest.tiff_drv.Create('/vsimem/rgba.tif', 1, 1, 4, options=['PHOTOMETRIC=RGB'])
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '0'
    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_Undefined

    # Now turn on alpha
    ds.GetRasterBand(4).SetRasterColorInterpretation(gdal.GCI_AlphaBand)

    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '2'
    ds = None

    assert gdal.VSIStatL('/vsimem/rgba.tif.aux.xml') is None

    ds = gdal.Open('/vsimem/rgba.tif')
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '2'
    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand

    # Test cancelling alpha
    gdaltest.tiff_drv.CreateCopy('/vsimem/rgb_no_alpha.tif', ds, options=['ALPHA=NO'])
    ds = None

    assert gdal.VSIStatL('/vsimem/rgb_no_alpha.tif.aux.xml') is None

    ds = gdal.Open('/vsimem/rgb_no_alpha.tif')
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '0'
    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_Undefined

    # Test re-adding alpha
    gdaltest.tiff_drv.CreateCopy('/vsimem/rgb_added_alpha.tif', ds, options=['ALPHA=YES'])
    ds = None

    assert gdal.VSIStatL('/vsimem/rgb_added_alpha.tif.aux.xml') is None

    ds = gdal.Open('/vsimem/rgb_added_alpha.tif')
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '2'
    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/rgba.tif')
    gdaltest.tiff_drv.Delete('/vsimem/rgb_no_alpha.tif')
    gdaltest.tiff_drv.Delete('/vsimem/rgb_added_alpha.tif')


###############################################################################
# Create a BigTIFF image with BigTIFF=YES

def test_tiff_write_30():

    ds = gdaltest.tiff_drv.Create('tmp/bigtiff.tif', 1, 1, 1, options=['BigTIFF=YES'])
    ds = None

    ds = gdal.Open('tmp/bigtiff.tif')
    assert ds is not None
    ds = None

    fileobj = open('tmp/bigtiff.tif', mode='rb')
    binvalues = struct.unpack('B' * 4, fileobj.read(4))
    fileobj.close()

    gdaltest.tiff_drv.Delete('tmp/bigtiff.tif')

    # Check BigTIFF signature
    assert (not ((binvalues[2] != 0x2B or binvalues[3] != 0) and
            (binvalues[3] != 0x2B or binvalues[2] != 0)))

###############################################################################
# Create a BigTIFF image implicitly (more than 4Gb).


def test_tiff_write_31():

    ds = gdaltest.tiff_drv.Create('tmp/bigtiff.tif', 100000, 100000, 1,
                                  options=['SPARSE_OK=TRUE'])
    ds = None

    ds = gdal.Open('tmp/bigtiff.tif')
    assert ds is not None
    ds = None

    fileobj = open('tmp/bigtiff.tif', mode='rb')
    binvalues = struct.unpack('B' * 4, fileobj.read(4))
    fileobj.close()

    gdaltest.tiff_drv.Delete('tmp/bigtiff.tif')

    # Check BigTIFF signature
    assert (not ((binvalues[2] != 0x2B or binvalues[3] != 0) and
            (binvalues[3] != 0x2B or binvalues[2] != 0)))

###############################################################################
# Create a rotated image


def test_tiff_write_32():

    ds_in = gdal.Open('data/byte.vrt')

    # Test creation
    ds = gdaltest.tiff_drv.Create('tmp/byte_rotated.tif', 20, 20, gdal.GDT_Byte)

    gt = (10, 3.53553390593, 3.53553390593, 30, 3.53553390593, -3.53553390593)
    ds.SetGeoTransform(gt)

    data = ds_in.ReadRaster(0, 0, 20, 20)
    ds.WriteRaster(0, 0, 20, 20, data)

    ds_in = None

    # Test copy
    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/byte_rotated_copy.tif', ds)
    del new_ds

    # Check copy
    ds = gdal.Open('tmp/byte_rotated_copy.tif')
    new_gt = ds.GetGeoTransform()
    for i in range(6):
        if new_gt[i] != pytest.approx(gt[i], abs=1e-5):
            print('')
            print(('old = ', gt))
            print(('new = ', new_gt))
            pytest.fail('Geotransform differs.')

    ds = None

    gdaltest.tiff_drv.Delete('tmp/byte_rotated.tif')
    gdaltest.tiff_drv.Delete('tmp/byte_rotated_copy.tif')

###############################################################################
# Test that metadata is written in .aux.xml file in GeoTIFF profile with CreateCopy
# (BASELINE is tested by tiff_write_15)


def test_tiff_write_33():

    ds_in = gdal.Open('data/byte.vrt')

    ds = gdaltest.tiff_drv.CreateCopy('tmp/tw_33.tif', ds_in, options=['PROFILE=GeoTIFF'])

    ds_in = None

    ds = None

    ds = gdal.Open('tmp/tw_33.tif')

    md = ds.GetMetadata()
    assert 'test' in md, 'Metadata absent from .aux.xml file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' in md, 'Metadata absent from .aux.xml file.'

    ds = None

    try:
        os.remove('tmp/tw_33.tif.aux.xml')
    except OSError:
        try:
            os.stat('tmp/tw_33.tif.aux.xml')
        except OSError:
            pytest.fail('No .aux.xml file.')

    ds = gdal.Open('tmp/tw_33.tif')

    md = ds.GetMetadata()
    assert 'test' not in md, 'Metadata written to GeoTIFF file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' not in md, 'Metadata written to GeoTIFF file.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tw_33.tif')

###############################################################################
# Test that metadata is written in .aux.xml file in GeoTIFF profile with Create
# (BASELINE is tested by tiff_write_16)


def test_tiff_write_34():

    ds = gdaltest.tiff_drv.Create('tmp/tw_34.tif', 1, 1, gdal.GDT_Byte,
                                  options=['PROFILE=GeoTIFF'])
    ds.SetMetadata({'test': 'testvalue'})
    ds.GetRasterBand(1).SetMetadata({'testBand': 'testvalueBand'})

    ds = None

    ds = gdal.Open('tmp/tw_34.tif')

    md = ds.GetMetadata()
    assert 'test' in md, 'Metadata absent from .aux.xml file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' in md, 'Metadata absent from .aux.xml file.'

    ds = None

    try:
        os.remove('tmp/tw_34.tif.aux.xml')
    except OSError:
        try:
            os.stat('tmp/tw_34.tif.aux.xml')
        except OSError:
            pytest.fail('No .aux.xml file.')

    ds = gdal.Open('tmp/tw_34.tif')

    md = ds.GetMetadata()
    assert 'test' not in md, 'Metadata written to GeoTIFF file.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'testBand' not in md, 'Metadata written to GeoTIFF file.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tw_34.tif')

###############################################################################
# Test big metadata (that was used to consider too big to fit into the GDALGeotiff tag
# before GDAL 3.4.2)


def test_tiff_write_35():

    big_string = 'a' * 12345678
    ds = gdaltest.tiff_drv.Create('tmp/tw_35.tif', 1, 1, gdal.GDT_Byte)

    md = {}
    md['test'] = big_string
    ds.SetMetadata(md)
    ds = None

    assert not os.path.exists('tmp/tw_35.tif.aux.xml')

    ds = gdal.Open('tmp/tw_35.tif')
    assert ds.GetMetadataItem('test') == big_string
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tw_35.tif')

###############################################################################
# Generic functions for the 8 following tests


def tiff_write_big_odd_bits(vrtfilename, tmpfilename, nbits, interleaving):
    ds_in = gdal.Open(vrtfilename)

    ds = gdaltest.tiff_drv.CreateCopy(tmpfilename, ds_in, options=['NBITS=' + str(nbits), 'INTERLEAVE=' + interleaving])

    ds_in = None

    ds = None

    ds = gdal.Open(tmpfilename)
    bnd = ds.GetRasterBand(1)
    cs = bnd.Checksum()
    assert cs == 4672, 'Didnt get expected checksum on band 1'
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    assert md['NBITS'] == str(nbits), 'Didnt get expected NBITS value'

    bnd = ds.GetRasterBand(2)
    assert bnd.Checksum() == 4672, 'Didnt get expected checksum on band 2'
    bnd = ds.GetRasterBand(3)
    assert bnd.Checksum() == 4672, 'Didnt get expected checksum on band 3'
    bnd = None

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['INTERLEAVE'] == interleaving, 'Didnt get expected interleaving'

    ds = None

    gdaltest.tiff_drv.Delete(tmpfilename)


###############################################################################
# Test copy with NBITS=9, INTERLEAVE=PIXEL

def test_tiff_write_36():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_36.tif', 9, 'PIXEL')


###############################################################################
# Test copy with NBITS=9, INTERLEAVE=BAND

def test_tiff_write_37():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_37.tif', 9, 'BAND')

###############################################################################
# Test copy with NBITS=12, INTERLEAVE=PIXEL


def test_tiff_write_38():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_38.tif', 12, 'PIXEL')

###############################################################################
# Test copy with NBITS=12, INTERLEAVE=BAND


def test_tiff_write_39():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_39.tif', 12, 'BAND')

###############################################################################
# Test copy with NBITS=17, INTERLEAVE=PIXEL


def test_tiff_write_40():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_40tif', 17, 'PIXEL')

###############################################################################
# Test copy with NBITS=17, INTERLEAVE=BAND


def test_tiff_write_41():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_41.tif', 17, 'BAND')

###############################################################################
# Test copy with NBITS=24, INTERLEAVE=PIXEL


def test_tiff_write_42():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_42.tif', 24, 'PIXEL')

###############################################################################
# Test copy with NBITS=24, INTERLEAVE=BAND


def test_tiff_write_43():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_43.tif', 24, 'BAND')


###############################################################################
# Test create with NBITS=9 and preservation through CreateCopy of NBITS

def test_tiff_write_44():

    ds = gdaltest.tiff_drv.Create('tmp/tw_44.tif', 1, 1, 1, gdal.GDT_UInt16, options=['NBITS=9'])
    ds = None
    ds = gdal.Open('tmp/tw_44.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    assert md['NBITS'] == '9', 'Didnt get expected NBITS value'

    ds2 = gdaltest.tiff_drv.CreateCopy('tmp/tw_44_copy.tif', ds)
    ds2 = None

    ds2 = gdal.Open('tmp/tw_44_copy.tif')
    bnd = ds2.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    assert md['NBITS'] == '9', 'Didnt get expected NBITS value'

    ds = None
    ds2 = None

    gdaltest.tiff_drv.Delete('tmp/tw_44.tif')
    gdaltest.tiff_drv.Delete('tmp/tw_44_copy.tif')

###############################################################################
# Test create with NBITS=17 and preservation through CreateCopy of NBITS


def test_tiff_write_45():

    ds = gdaltest.tiff_drv.Create('tmp/tw_45.tif', 1, 1, 1, gdal.GDT_UInt32, options=['NBITS=17'])
    ds = None
    ds = gdal.Open('tmp/tw_45.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    assert md['NBITS'] == '17', 'Didnt get expected NBITS value'

    ds2 = gdaltest.tiff_drv.CreateCopy('tmp/tw_45_copy.tif', ds)
    ds2 = None

    ds2 = gdal.Open('tmp/tw_45_copy.tif')
    bnd = ds2.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    assert md['NBITS'] == '17', 'Didnt get expected NBITS value'

    ds = None
    ds2 = None

    gdaltest.tiff_drv.Delete('tmp/tw_45.tif')
    gdaltest.tiff_drv.Delete('tmp/tw_45_copy.tif')


###############################################################################
# Test correct round-tripping of ReadBlock/WriteBlock

def test_tiff_write_46():

    with gdaltest.SetCacheMax(0):

        ds = gdaltest.tiff_drv.Create("tmp/tiff_write_46_1.tif", 10, 10, 1, options=['NBITS=1'])
        ds.GetRasterBand(1).Fill(0)

        ds2 = gdaltest.tiff_drv.Create("tmp/tiff_write_46_2.tif", 10, 10, 1, options=['NBITS=1'])
        ds2.GetRasterBand(1).Fill(1)
        ones = ds2.ReadRaster(0, 0, 10, 1)

        # Load the working block
        data = ds.ReadRaster(0, 0, 10, 1)

        # Write the working bloc
        ds.WriteRaster(0, 0, 10, 1, ones)

        # This will discard the cached block for ds
        ds3 = gdaltest.tiff_drv.Create("tmp/tiff_write_46_3.tif", 10, 10, 1)
        ds3.GetRasterBand(1).Fill(1)

        # Load the working block again
        data = ds.ReadRaster(0, 0, 10, 1)

        # We expect (1, 1, 1, 1, 1, 1, 1, 1, 1, 1)
        got = struct.unpack('B' * 10, data)
        for g in got:
            assert g == 1, got

    ds = None
    ds2 = None
    ds3 = None
    gdaltest.tiff_drv.Delete('tmp/tiff_write_46_1.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_46_2.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_46_3.tif')

###############################################################################
# Test #2457


def test_tiff_write_47():

    with gdaltest.SetCacheMax(0):
        ret = test_tiff_write_3()
    return ret


###############################################################################
# Test #2457 with nYOff of RasterIO not aligned on the block height

def test_tiff_write_48():

    with gdaltest.SetCacheMax(0):

        src_ds = gdal.Open('data/utmsmall.tif')
        new_ds = gdal.GetDriverByName("GTiff").Create('tmp/tiff_write_48.tif', 100, 100, 1, options=['TILED=YES', 'BLOCKXSIZE=96', 'BLOCKYSIZE=96'])
        data = src_ds.ReadRaster(0, 0, 100, 1)
        data2 = src_ds.ReadRaster(0, 1, 100, 99)
        new_ds.WriteRaster(0, 1, 100, 99, data2)
        new_ds.WriteRaster(0, 0, 100, 1, data)
        new_ds = None

    new_ds = None
    new_ds = gdal.Open('tmp/tiff_write_48.tif')
    assert new_ds.GetRasterBand(1).Checksum() == 50054, 'Didnt get expected checksum '

    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_48.tif')


###############################################################################
# Test copying a CMYK TIFF into another CMYK TIFF

def test_tiff_write_49():

    # We open the source as RAW to get the CMYK bands
    src_ds = gdal.Open('GTIFF_RAW:data/rgbsmall_cmyk.tif')

    new_ds = gdal.GetDriverByName("GTiff").CreateCopy('tmp/tiff_write_49.tif', src_ds, options=['PHOTOMETRIC=CMYK'])

    # At this point, for the purpose of the copy, the dataset will have been opened as RAW
    assert new_ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_CyanBand, \
        'Wrong color interpretation.'

    new_ds = None

    new_ds = gdal.Open('GTIFF_RAW:tmp/tiff_write_49.tif')

    for i in range(4):
        assert new_ds.GetRasterBand(i + 1).Checksum() == src_ds.GetRasterBand(i + 1).Checksum(), \
            'Didnt get expected checksum '

    src_ds = None
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_49.tif')


###############################################################################
# Test creating a CMYK TIFF from another CMYK TIFF

def test_tiff_write_50():

    # We open the source as RAW to get the CMYK bands
    src_ds = gdal.Open('GTIFF_RAW:data/rgbsmall_cmyk.tif')

    new_ds = gdal.GetDriverByName("GTiff").Create('tmp/tiff_write_50.tif', src_ds.RasterXSize, src_ds.RasterYSize, 4, options=['PHOTOMETRIC=CMYK'])
    for i in range(4):
        data = src_ds.GetRasterBand(i + 1).ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
        new_ds.GetRasterBand(i + 1).WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data)

    assert new_ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_CyanBand, \
        'Wrong color interpretation.'

    new_ds = None

    new_ds = gdal.Open('GTIFF_RAW:tmp/tiff_write_50.tif')

    for i in range(4):
        assert new_ds.GetRasterBand(i + 1).Checksum() == src_ds.GetRasterBand(i + 1).Checksum(), \
            'Didnt get expected checksum '

    src_ds = None
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_50.tif')


###############################################################################
# Test proper clearing of existing GeoTIFF tags when updating the projection.
# http://trac.osgeo.org/gdal/ticket/2546

def test_tiff_write_51():
    shutil.copyfile('data/utmsmall.tif', 'tmp/tiff_write_51.tif')

    ds = gdal.Open('tmp/tiff_write_51.tif', gdal.GA_Update)

    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/tiff_write_51.tif')
    wkt = ds.GetProjection()
    ds = None

    # Create a new GeoTIFF file with same projection
    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_51_ref.tif', 1, 1, 1)
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    # Read it back as the reference WKT
    ds = gdal.Open('tmp/tiff_write_51_ref.tif')
    expected_wkt = ds.GetProjection()
    ds = None

    assert wkt.find('NAD') == -1 and wkt.find('North Am') == -1, \
        'It appears the NAD27 datum was not properly cleared.'

    assert wkt == expected_wkt and wkt.find('WGS 84 / UTM zone 1N') != -1, \
        'coordinate system does not exactly match.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_51.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_51_ref.tif')

###############################################################################
# Test the ability to update a paletted TIFF files color table.


def test_tiff_write_52():
    shutil.copyfile('data/test_average_palette.tif', 'tmp/tiff_write_52.tif')

    test_ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255, 0)]

    test_ct = gdal.ColorTable()
    for i, data in enumerate(test_ct_data):
        test_ct.SetColorEntry(i, data)

    ds = gdal.Open('tmp/tiff_write_52.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetRasterColorTable(test_ct)
    ds = None

    ds = gdal.Open('tmp/tiff_write_52.tif')
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    assert ct.GetColorEntry(0) == (255, 0, 0, 255), 'Did not get expected color 0.'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_52.tif')

###############################################################################
# Test the ability to create a paletted image and then update later.


def test_tiff_write_53():
    test_ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255, 0)]

    test_ct = gdal.ColorTable()
    for i, data in enumerate(test_ct_data):
        test_ct.SetColorEntry(i, data)

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_53.tif',
                                  30, 50, 1,
                                  options=['PHOTOMETRIC=PALETTE'])
    ds.GetRasterBand(1).Fill(10)
    ds = None

    ds = gdal.Open('tmp/tiff_write_53.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetRasterColorTable(test_ct)
    ds = None

    ds = gdal.Open('tmp/tiff_write_53.tif')
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    assert ct.GetColorEntry(0) == (255, 0, 0, 255), 'Did not get expected color 0.'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_53.tif')


###############################################################################
# Same as before except we create an overview before reopening the file and
# adding the color table

def test_tiff_write_53_bis():
    test_ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255, 0)]

    test_ct = gdal.ColorTable()
    for i, data in enumerate(test_ct_data):
        test_ct.SetColorEntry(i, data)

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_53_bis.tif',
                                  30, 50, 1,
                                  options=['PHOTOMETRIC=PALETTE'])
    ds.GetRasterBand(1).Fill(10)
    ds.BuildOverviews('NONE', overviewlist=[2])
    ds = None

    ds = gdal.Open('tmp/tiff_write_53_bis.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetRasterColorTable(test_ct)
    ds = None

    ds = gdal.Open('tmp/tiff_write_53_bis.tif')
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    assert ct.GetColorEntry(0) == (255, 0, 0, 255), 'Did not get expected color 0.'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_53_bis.tif')

###############################################################################
# Test the ability to create a JPEG compressed TIFF, with PHOTOMETRIC=YCBCR
# and write data into it without closing it and re-opening it (#2645)


def test_tiff_write_54():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_54.tif',
                                  256, 256, 3,
                                  options=['TILED=YES', 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'])
    ds.GetRasterBand(1).Fill(255)
    ds.FlushCache()
    ds = None

    ds = gdal.Open('tmp/tiff_write_54.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_54.tif')

    assert cs != 0, 'did not get expected checksum'


###############################################################################
# Test creating and reading an equirectangular file with all parameters (#2706)

def test_tiff_write_55():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_55.tif',
                                  256, 256, 1)
    srs_expected = 'PROJCS["Equirectangular Mars",GEOGCS["GCS_Mars",DATUM["unknown",SPHEROID["unnamed",3394813.85797594,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",-2],PARAMETER["central_meridian",184.412994384766],PARAMETER["standard_parallel_1",-15],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'

    ds.SetProjection(srs_expected)

    ds.SetGeoTransform((100, 1, 0, 200, 0, -1))
    ds = None

    ds = gdal.Open('tmp/tiff_write_55.tif')
    srs = ds.GetProjectionRef()
    ds = None

    assert srs == srs_expected, \
        'failed to preserve Equirectangular projection as expected, old libgeotiff?'

    gdaltest.tiff_drv.Delete('tmp/tiff_write_55.tif')

###############################################################################
# Test clearing the colormap from an existing paletted TIFF file.


def test_tiff_write_56():

    test_ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255, 0)]

    test_ct = gdal.ColorTable()
    for i, data in enumerate(test_ct_data):
        test_ct.SetColorEntry(i, data)

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_56.tif',
                                  30, 50, 1,
                                  options=['PHOTOMETRIC=PALETTE'])
    ds.GetRasterBand(1).Fill(10)
    ds = None

    test_ct = gdal.ColorTable()

    ds = gdal.Open('tmp/tiff_write_56.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetRasterColorTable(test_ct)
    ds = None

    ds = gdal.Open('tmp/tiff_write_56.tif')
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    assert ct is None, 'color table seemingly not cleared.'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_56.tif')

###############################################################################
# Test replacing normal norm up georef with rotated georef (#2625)


def test_tiff_write_57():

    # copy a file to tmp dir to modify.
    open('tmp/tiff57.tif', 'wb').write(open('data/byte.tif', 'rb').read())

    # open and set a non-northup geotransform.

    ds = gdal.Open('tmp/tiff57.tif', gdal.GA_Update)
    ds.SetGeoTransform([100, 1, 3, 200, 3, 1])
    ds = None

    ds = gdal.Open('tmp/tiff57.tif')
    gt = ds.GetGeoTransform()
    ds = None

    assert gt == (100, 1, 3, 200, 3, 1), \
        'did not get expected geotransform, perhaps unset is not working?'

    gdaltest.tiff_drv.Delete('tmp/tiff57.tif')

###############################################################################
# Test writing partial end strips (#2748)


def test_tiff_write_58():

    md = gdaltest.tiff_drv.GetMetadata()

    for compression in ('NONE', 'JPEG', 'LZW', 'DEFLATE', 'PACKBITS'):

        if md['DMD_CREATIONOPTIONLIST'].find(compression) != -1:
            ds = gdaltest.tiff_drv.Create('tmp/tiff_write_58.tif', 4, 4000, 1, options=['COMPRESS=' + compression])
            ds.GetRasterBand(1).Fill(255)
            ds = None

            ds = gdal.Open('tmp/tiff_write_58.tif')
            assert ds.GetRasterBand(1).Checksum() == 65241, 'wrong checksum'
            ds = None

            gdaltest.tiff_drv.Delete('tmp/tiff_write_58.tif')
        else:
            print(('Skipping compression method %s' % compression))


###############################################################################
# Test fix for #2759


def test_tiff_write_59():

    ret = 'success'

    for nbands in (1, 2):
        for nbits in (1, 8, 9, 12, 16, 17, 24, 32):

            if nbits <= 8:
                gdal_type = gdal.GDT_Byte
                ctype = 'B'
            elif nbits <= 16:
                gdal_type = gdal.GDT_UInt16
                ctype = 'h'
            else:
                gdal_type = gdal.GDT_UInt32
                ctype = 'i'

            ds = gdaltest.tiff_drv.Create("tmp/tiff_write_59.tif", 10, 10, nbands, gdal_type, options=['NBITS=%d' % nbits])
            ds.GetRasterBand(1).Fill(1)

            ds = None
            ds = gdal.Open("tmp/tiff_write_59.tif", gdal.GA_Update)

            data = struct.pack(ctype * 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
            ds.GetRasterBand(1).WriteRaster(0, 0, 10, 1, data)

            ds = None
            ds = gdal.Open("tmp/tiff_write_59.tif")

            data = ds.GetRasterBand(1).ReadRaster(0, 0, 10, 1)

            # We expect zeros
            got = struct.unpack(ctype * 10, data)
            for g in got:
                if g != 0:
                    print(('nbands=%d, NBITS=%d' % (nbands, nbits)))
                    print(got)
                    ret = 'fail'
                    break

            ds = None
            gdaltest.tiff_drv.Delete('tmp/tiff_write_59.tif')

    return ret

###############################################################################
# Test fix for #2760


def test_tiff_write_60():

    tuples = [('TFW=YES', 'tmp/tiff_write_60.tfw'),
              ('WORLDFILE=YES', 'tmp/tiff_write_60.wld')]

    for options_tuple in tuples:
        # Create case
        with gdaltest.error_handler():
            ds = gdaltest.tiff_drv.Create('tmp/tiff_write_60.tif', 10, 10, options=[options_tuple[0], 'PROFILE=BASELINE'])
        gt = (0.0, 1.0, 0.0, 50.0, 0.0, -1.0)
        ds.SetGeoTransform(gt)
        ds = None

        with gdaltest.error_handler():
            ds = gdal.Open('tmp/tiff_write_60.tif')
        assert ds.GetGeoTransform() == gt, ('case1: %s != %s' % (ds.GetGeoTransform(), gt))

        ds = None
        gdaltest.tiff_drv.Delete('tmp/tiff_write_60.tif')

        assert not os.path.exists(options_tuple[1])

        # CreateCopy case
        src_ds = gdal.Open('data/byte.tif')
        with gdaltest.error_handler():
            ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_60.tif', src_ds, options=[options_tuple[0], 'PROFILE=BASELINE'])
        gt = (0.0, 1.0, 0.0, 50.0, 0.0, -1.0)
        ds.SetGeoTransform(gt)
        ds = None
        gdal.Unlink('tmp/tiff_write_60.tif.aux.xml')

        ds = gdal.Open('tmp/tiff_write_60.tif')
        assert ds.GetGeoTransform() == gt, \
            ('case2: %s != %s' % (ds.GetGeoTransform(), gt))

        ds = None
        gdaltest.tiff_drv.Delete('tmp/tiff_write_60.tif')

        assert not os.path.exists(options_tuple[1])


###############################################################################
# Test BigTIFF=IF_NEEDED creation option


def test_tiff_write_61():

    ds = gdaltest.tiff_drv.Create('tmp/bigtiff.tif', 50000, 50000, 1,
                                  options=['BIGTIFF=IF_NEEDED', 'SPARSE_OK=TRUE'])
    ds = None

    ds = gdal.Open('tmp/bigtiff.tif')
    assert ds is not None
    ds = None

    fileobj = open('tmp/bigtiff.tif', mode='rb')
    binvalues = struct.unpack('B' * 4, fileobj.read(4))
    fileobj.close()

    gdaltest.tiff_drv.Delete('tmp/bigtiff.tif')

    # Check classical TIFF signature
    assert (not ((binvalues[2] != 0x2A or binvalues[3] != 0) and
            (binvalues[3] != 0x2A or binvalues[2] != 0)))

###############################################################################
# Test BigTIFF=IF_SAFER creation option


def test_tiff_write_62():

    ds = gdaltest.tiff_drv.Create('tmp/bigtiff.tif', 50000, 50000, 1,
                                  options=['BIGTIFF=IF_SAFER', 'SPARSE_OK=TRUE'])
    ds = None

    ds = gdal.Open('tmp/bigtiff.tif')
    assert ds is not None
    ds = None

    fileobj = open('tmp/bigtiff.tif', mode='rb')
    binvalues = struct.unpack('B' * 4, fileobj.read(4))
    fileobj.close()

    gdaltest.tiff_drv.Delete('tmp/bigtiff.tif')

    # Check BigTIFF signature
    assert (not ((binvalues[2] != 0x2B or binvalues[3] != 0) and
            (binvalues[3] != 0x2B or binvalues[2] != 0)))

###############################################################################
# Test BigTIFF=NO creation option when creating a BigTIFF file would be required


def test_tiff_write_63():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.Create('tmp/bigtiff.tif', 150000, 150000, 1,
                                  options=['BIGTIFF=NO'])
    gdal.PopErrorHandler()

    if ds is None:
        return

    pytest.fail()

###############################################################################
# Test returned projection in WKT format for a WGS84 GeoTIFF (#2787)


def test_tiff_write_64():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_64.tif', 1, 1, 1)
    srs = osr.SpatialReference()
    srs.SetFromUserInput('WGS84')
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/tiff_write_64.tif')
    wkt = ds.GetProjection()
    ds = None

    expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]"""

    assert wkt == expected_wkt, 'coordinate system does not exactly match.'

    gdaltest.tiff_drv.Delete('tmp/tiff_write_64.tif')

###############################################################################
# Verify that we can write XML metadata.


def test_tiff_write_65():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_65.tif', 10, 10)

    doc = '<doc><test xml:attr="abc"/></doc>'
    ds.SetMetadata([doc], 'xml:test')

    ds = None

    ds = gdal.Open('tmp/tiff_write_65.tif')
    md = ds.GetMetadata('xml:test')
    ds = None

    assert len(md) == 1 and md[0] == doc, 'did not get xml back clean'

    gdaltest.tiff_drv.Delete('tmp/tiff_write_65.tif')


###############################################################################
# Verify that we can write and read a band-interleaved GeoTIFF with 65535 bands (#2838)

def test_tiff_write_66():

    if gdal.GetConfigOption('SKIP_MEM_INTENSIVE_TEST') is not None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_66.tif', 1, 1, 65535, options=['INTERLEAVE=BAND'])
    ds = None

    ds = gdal.Open('tmp/tiff_write_66.tif')
    assert ds.RasterCount == 65535

    assert ds.GetRasterBand(1).Checksum() == 0

    assert ds.GetRasterBand(65535).Checksum() == 0

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_66.tif')


###############################################################################
# Verify that we can write and read a pixel-interleaved GeoTIFF with 65535 bands (#2838)

def test_tiff_write_67():

    if gdal.GetConfigOption('SKIP_MEM_INTENSIVE_TEST') is not None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_67.tif', 1, 1, 65535, options=['INTERLEAVE=PIXEL'])
    ds = None

    ds = gdal.Open('tmp/tiff_write_67.tif')
    assert ds.RasterCount == 65535

    assert ds.GetRasterBand(1).Checksum() == 0

    assert ds.GetRasterBand(65535).Checksum() == 0

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_67.tif')

###############################################################################
# Verify that we can set the color table after a Create() (scenario hit by map.tif in #2820)


def test_tiff_write_68():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_68.tif', 151, 161, options=['COMPRESS=LZW'])
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.Open('tmp/tiff_write_68.tif')
    assert ds.GetRasterBand(1).Checksum() != 0
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_68.tif')

###############################################################################
# Verify GTiffRasterBand::NullBlock() when reading empty block without any nodata value set


def test_tiff_write_69():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_69.tif', 32, 32, 1, gdal.GDT_Int16, options=['SPARSE_OK=YES'])
    ds = None

    ds = gdal.Open('tmp/tiff_write_69.tif')
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_69.tif')

###############################################################################
# Verify GTiffRasterBand::NullBlock() when reading empty block with nodata value set


def test_tiff_write_70():

    ref_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_70_ref.tif', 32, 32, 1, gdal.GDT_Int16)
    ref_ds.GetRasterBand(1).Fill(-32768)
    ref_ds = None

    ref_ds = gdal.Open('tmp/tiff_write_70_ref.tif')
    expected_cs = ref_ds.GetRasterBand(1).Checksum()
    ref_ds = None

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_70.tif', 32, 32, 1, gdal.GDT_Int16, options=['SPARSE_OK=YES'])
    ds.GetRasterBand(1).SetNoDataValue(0)
    assert os.stat('tmp/tiff_write_70.tif').st_size <= 8, \
        'directory should not be crystallized'
    ds = None

    ds = gdal.Open('tmp/tiff_write_70.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetNoDataValue(-32768)
    ds = None

    ds = gdal.Open('tmp/tiff_write_70.tif')
    assert ds.GetRasterBand(1).Checksum() == expected_cs, 'wrong checksum'
    ds = None

    ds = gdal.Open('tmp/tiff_write_70.tif', gdal.GA_Update)
    assert ds.GetRasterBand(1).DeleteNoDataValue() == 0
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    ds = None

    with pytest.raises(OSError):
        os.stat('tmp/tiff_write_70.tif.aux.xml')


    ds = gdal.Open('tmp/tiff_write_70.tif')
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_70.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_70_ref.tif')


###############################################################################
# Test reading in a real BigTIFF file (on filesystems supporting sparse files)

def test_tiff_write_71():

    # Determine if the filesystem supports sparse files (we don't want to create a real 10 GB
    # file !
    if not gdaltest.filesystem_supports_sparse_files('tmp'):
        pytest.skip()

    header = open('data/bigtiff_header_extract.tif', 'rb').read()

    f = open('tmp/tiff_write_71.tif', 'wb')
    f.write(header)

    # Write StripByteCounts tag
    # 100,000 in little endian
    for _ in range(100000):
        f.write(b'\xa0\x86\x01\x00\x00\x00\x00\x00')

    # Write StripOffsets tag
    offset = 1600252
    for _ in range(100000):
        f.write(struct.pack('<Q', offset))
        offset = offset + 100000

    # Write 0x78 as value of pixel (99999, 99999)
    f.seek(10001600252 - 1, 0)
    f.write(b'\x78')
    f.close()

    ds = gdal.Open('tmp/tiff_write_71.tif')
    data = ds.GetRasterBand(1).ReadRaster(99999, 99999, 1, 1)
    assert struct.unpack('b', data)[0] == 0x78
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_71.tif')

###############################################################################
# With CreateCopy(), check that TIFF directory is in the first bytes of the file
# and has not been rewritten later (#3021)


def test_tiff_write_72():

    shutil.copyfile('data/byte.tif', 'tmp/byte.tif')
    ds = gdal.Open('tmp/byte.tif', gdal.GA_Update)
    ds.SetMetadata({'TEST_KEY': 'TestValue'})
    ds = None

    for profile in ('GDALGeotiff', 'GEOTIFF', 'BASELINE'):
        src_ds = gdal.Open('tmp/byte.tif')
        out_ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_72.tif', src_ds, options=['ENDIANNESS=LITTLE', 'PROFILE=' + profile])
        del out_ds
        src_ds = None

        fileobj = open('tmp/tiff_write_72.tif', mode='rb')
        fileobj.seek(4)
        binvalues = struct.unpack('B' * 4, fileobj.read(4))
        fileobj.close()

        # Directory should be at offset 8 of the file
        assert (binvalues[0] == 0x08 and binvalues[1] == 0x00 and binvalues[2] == 0x00 and binvalues[3] == 0x00), \
            ('Failed with profile %s' % profile)

    gdaltest.tiff_drv.Delete('tmp/byte.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_72.tif')

###############################################################################
# With Create(), check that TIFF directory is in the first bytes of the file
# and has not been rewritten later (#3021)


def test_tiff_write_73():

    out_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_73.tif', 10, 10, options=['ENDIANNESS=LITTLE'])
    out_ds.SetGeoTransform([1, 0.01, 0, 1, 0, -0.01])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    out_ds.SetProjection(srs.ExportToWkt())
    out_ds.SetMetadata({'TEST_KEY': 'TestValue'})
    out_ds.BuildOverviews('NONE', [2])
    out_ds.GetRasterBand(1).Fill(255)
    out_ds = None

    fileobj = open('tmp/tiff_write_73.tif', mode='rb')
    fileobj.seek(4)
    binvalues = struct.unpack('B' * 4, fileobj.read(4))
    fileobj.close()

    # Directory should be at offset 8 of the file
    assert (binvalues[0] == 0x08 and binvalues[1] == 0x00 and binvalues[2] == 0x00 and binvalues[3] == 0x00)

    # Re-open the file and modify the pixel content
    out_ds = gdal.Open('tmp/tiff_write_73.tif', gdal.GA_Update)
    out_ds.GetRasterBand(1).Fill(0)
    out_ds = None

    fileobj = open('tmp/tiff_write_73.tif', mode='rb')
    fileobj.seek(4)
    binvalues = struct.unpack('B' * 4, fileobj.read(4))
    fileobj.close()

    # Directory should be at offset 8 of the file
    assert (binvalues[0] == 0x08 and binvalues[1] == 0x00 and binvalues[2] == 0x00 and binvalues[3] == 0x00)

    gdaltest.tiff_drv.Delete('tmp/tiff_write_73.tif')

###############################################################################
# Verify we can write 12bit jpeg encoded tiff.


@pytest.mark.skipif('SKIP_TIFF_JPEG12' in os.environ, reason='Crashes on build-windows-msys2-mingw')
def test_tiff_write_74():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    old_accum = gdal.GetConfigOption('CPL_ACCUM_ERROR_MSG', 'OFF')
    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'ON')
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    try:
        ds = gdal.Open('data/mandrilmini_12bitjpeg.tif')
        ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    except:
        ds = None

    gdal.PopErrorHandler()
    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', old_accum)

    if gdal.GetLastErrorMsg().find('Unsupported JPEG data precision 12') != -1:
        pytest.skip('12bit jpeg not available')

    for photometric in ('YCBCR', 'RGB'):

        drv = gdal.GetDriverByName('GTiff')
        dst_ds = drv.CreateCopy('tmp/test_74.tif', ds,
                                options=['COMPRESS=JPEG', 'NBITS=12',
                                         'JPEG_QUALITY=95',
                                         'PHOTOMETRIC=' + photometric])
        dst_ds = None

        dst_ds = gdal.Open('tmp/test_74.tif')
        stats = dst_ds.GetRasterBand(1).GetStatistics(0, 1)

        if stats[2] < 2150 or stats[2] > 2180:
            print(photometric)
            pytest.fail('did not get expected mean for band1.')

        try:
            compression = dst_ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE')
        except:
            md = dst_ds.GetMetadata('IMAGE_STRUCTURE')
            compression = md['COMPRESSION']

        if (photometric == 'YCBCR' and compression != 'YCbCr JPEG') or \
           (photometric == 'RGB' and compression != 'JPEG'):
            print(('COMPRESSION="%s"' % compression))
            pytest.fail('did not get expected COMPRESSION value')

        try:
            nbits = dst_ds.GetRasterBand(3).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE')
        except:
            md = dst_ds.GetRasterBand(3).GetMetadata('IMAGE_STRUCTURE')
            nbits = md['NBITS']

        if nbits != '12':
            print(photometric)
            pytest.fail('did not get expected NBITS value')

        dst_ds = None

        gdaltest.tiff_drv.Delete('tmp/test_74.tif')


###############################################################################
# Verify that FlushCache() alone doesn't cause crash (#3067 )


def test_tiff_write_75():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_75.tif', 1, 1, 1)
    ds.FlushCache()
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_75.tif')

###############################################################################
# Test generating a G4 band to use the TIFFWriteScanline()


def test_tiff_write_76():

    src_ds = gdal.Open('data/slim_g4.tif')
    compression = src_ds.GetMetadata('IMAGE_STRUCTURE')['COMPRESSION']
    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_76.tif', src_ds, options=['BLOCKYSIZE=%d' % src_ds.RasterYSize, 'COMPRESS=' + compression])
    new_ds = None
    new_ds = gdal.Open('tmp/tiff_write_76.tif')

    cs = new_ds.GetRasterBand(1).Checksum()
    assert cs == 3322, 'Got wrong checksum'

    src_ds = None
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_76.tif')

###############################################################################
# Test generating & reading a 8bit all-in-one-strip multiband TIFF (#3904)


def test_tiff_write_77():

    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_77_src.tif', 1, 5000, 3)
    src_ds.GetRasterBand(2).Fill(255)

    for interleaving in ('PIXEL', 'BAND'):
        new_ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_77.tif', src_ds,
                                              options=['BLOCKYSIZE=%d' % src_ds.RasterYSize,
                                                       'COMPRESS=LZW',
                                                       'INTERLEAVE=' + interleaving])

        for attempt in range(2):

            # Test reading a few samples to check that random reading works
            band_lines = [(1, 0), (1, 5), (1, 3), (2, 10), (1, 100), (2, 1000), (2, 500),
                          (1, 500), (2, 500), (2, 4999), (2, 4999), (3, 4999), (1, 4999)]
            for band_line in band_lines:
                cs = new_ds.GetRasterBand(band_line[0]).Checksum(0, band_line[1], 1, 1)
                if band_line[0] == 2:
                    expected_cs = 255 % 7
                else:
                    expected_cs = 0 % 7
                assert cs == expected_cs, 'Got wrong checksum'

            # Test whole bands
            for i in range(3):
                cs = new_ds.GetRasterBand(i + 1).Checksum()
                expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
                assert cs == expected_cs, 'Got wrong checksum'

            if attempt == 0:
                new_ds = None
                new_ds = gdal.Open('tmp/tiff_write_77.tif')

        new_ds = None

        gdaltest.tiff_drv.Delete('tmp/tiff_write_77.tif')

    src_ds = None
    gdaltest.tiff_drv.Delete('tmp/tiff_write_77_src.tif')

###############################################################################
# Test generating & reading a YCbCr JPEG all-in-one-strip multiband TIFF (#3259)


def test_tiff_write_78():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_78_src.tif', 16, 2048, 3)
    src_ds.GetRasterBand(2).Fill(255)

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_78.tif', src_ds,
                                          options=['BLOCKYSIZE=%d' % src_ds.RasterYSize,
                                                   'COMPRESS=JPEG',
                                                   'PHOTOMETRIC=YCBCR'])

    # Make sure the file is flushed so that we re-read from it rather from cached blocks
    new_ds.FlushCache()
    # new_ds = None
    # new_ds = gdal.Open('tmp/tiff_write_78.tif')

    if 'GetBlockSize' in dir(gdal.Band):
        (_, blocky) = new_ds.GetRasterBand(1).GetBlockSize()
        if blocky != 1:
            print('')
            print('using regular band (libtiff <= 3.9.2 or <= 4.0.0beta5, or SplitBand disabled by config option)')

    # Test reading a few samples to check that random reading works
    band_lines = [(1, 0), (1, 5), (1, 3), (2, 10), (1, 100), (2, 1000), (2, 500),
                  (1, 500), (2, 500), (2, 2047), (2, 2047), (3, 2047), (1, 2047)]
    for band_line in band_lines:
        cs = new_ds.GetRasterBand(band_line[0]).Checksum(0, band_line[1], 1, 1)
        if band_line[0] == 1:
            expected_cs = 0 % 7
        elif band_line[0] == 2:
            expected_cs = 255 % 7
        else:
            # We should expect 0, but due to JPEG YCbCr compression & decompression,
            # this ends up being 1
            expected_cs = 1 % 7
        if cs != expected_cs:
            print(band_line)
            pytest.fail('Got wrong checksum')

    # Test whole bands
    for i in range(3):
        cs = new_ds.GetRasterBand(i + 1).Checksum()
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        if i == 2:
            # We should expect 0, but due to JPEG YCbCr compression & decompression,
            # this ends up being 32768
            expected_cs = 32768
        assert cs == expected_cs, 'Got wrong checksum'

    new_ds = None
    gdaltest.tiff_drv.Delete('tmp/tiff_write_78.tif')

    src_ds = None
    gdaltest.tiff_drv.Delete('tmp/tiff_write_78_src.tif')

###############################################################################
# Test reading & updating GDALMD_AREA_OR_POINT (#3522)


def test_tiff_write_79():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_79.tif', 1, 1)
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    for do_projection_ref in [False, True]:
        for check_just_after in [False, True]:

            ds = gdal.Open('tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            mdi = ds.GetMetadataItem('AREA_OR_POINT')
            assert mdi == 'Area', \
                ('(1) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
            ds = None

            # Now update to 'Point'
            ds = gdal.Open('tmp/tiff_write_79.tif', gdal.GA_Update)
            if do_projection_ref:
                ds.GetProjectionRef()
            ds.SetMetadataItem('AREA_OR_POINT', 'Point')
            if check_just_after:
                mdi = ds.GetMetadataItem('AREA_OR_POINT')
                assert mdi == 'Point', \
                    ('(3) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
            ds = None
            assert not os.path.exists('tmp/tiff_write_79.tif.aux.xml')

            # Now should get 'Point'
            ds = gdal.Open('tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            mdi = ds.GetMetadataItem('AREA_OR_POINT')
            assert mdi == 'Point', \
                ('(4) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
            ds = None

            # Now update back to 'Area' through SetMetadata()
            ds = gdal.Open('tmp/tiff_write_79.tif', gdal.GA_Update)
            if do_projection_ref:
                ds.GetProjectionRef()
            md = {}
            md['AREA_OR_POINT'] = 'Area'
            ds.SetMetadata(md)
            if check_just_after:
                mdi = ds.GetMetadataItem('AREA_OR_POINT')
                assert mdi == 'Area', \
                    ('(5) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
            ds = None

            # Now should get 'Area'
            ds = gdal.Open('tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            mdi = ds.GetMetadataItem('AREA_OR_POINT')
            assert mdi == 'Area', '(6) did not get expected value'
            ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_79.tif')

###############################################################################
# Test SetOffset() & SetScale()


def test_tiff_write_80():

    # First part : test storing and retrieving scale & offsets from internal metadata
    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_80.tif', 1, 1)
    ds.GetRasterBand(1).SetScale(100)
    ds.GetRasterBand(1).SetOffset(1000)
    ds = None

    assert not os.path.exists('tmp/tiff_write_80.tif.aux.xml')

    ds = gdal.Open('tmp/tiff_write_80.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    assert scale == 100 and offset == 1000, \
        'did not get expected values in internal case (1)'
    ds = None

    # Test CreateCopy()
    src_ds = gdal.Open('tmp/tiff_write_80.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_80_copy.tif', src_ds)
    src_ds = None
    ds = None
    ds = gdal.Open('tmp/tiff_write_80_copy.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    assert scale == 100 and offset == 1000, 'did not get expected values in copy'
    ds = None
    gdaltest.tiff_drv.Delete('tmp/tiff_write_80_copy.tif')

    # Second part : test unsetting scale & offsets from internal metadata
    ds = gdal.Open('tmp/tiff_write_80.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetScale(1)
    ds.GetRasterBand(1).SetOffset(0)
    ds = None

    ds = gdal.Open('tmp/tiff_write_80.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    assert not scale
    assert not offset
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_80.tif')

    # Third part : test storing and retrieving scale & offsets from PAM metadata
    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_80_bis.tif', 1, 1)
    assert ds.GetRasterBand(1).GetScale() is None and ds.GetRasterBand(1).GetOffset() is None, \
        'expected None values'
    ds = None

    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    ds.GetRasterBand(1).SetScale(-100)
    ds.GetRasterBand(1).SetOffset(-1000)
    ds = None

    try:
        # check that it *goes* to PAM
        os.stat('tmp/tiff_write_80_bis.tif.aux.xml')
    except OSError:
        pytest.fail('did not go to PAM as expected')

    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    assert scale == -100 and offset == -1000, \
        'did not get expected values in PAM case (1)'
    ds = None

    # Fourth part : test unsetting scale & offsets from PAM metadata
    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    ds.GetRasterBand(1).SetScale(1)
    ds.GetRasterBand(1).SetOffset(0)
    ds = None

    assert not os.path.exists('tmp/tiff_write_80_bis.tif.aux.xml')

    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    assert not scale
    assert not offset
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_80_bis.tif')

###############################################################################
# Test retrieving GCP from PAM


def test_tiff_write_81():

    shutil.copyfile('data/byte.tif', 'tmp/tiff_write_81.tif')
    f = open('tmp/tiff_write_81.tif.aux.xml', 'wt')
    f.write("""
<PAMDataset>
  <GCPList Projection="PROJCS[&quot;NAD27 / UTM zone 11N&quot;,GEOGCS[&quot;NAD27&quot;,DATUM[&quot;North_American_Datum_1927&quot;,SPHEROID[&quot;Clarke 1866&quot;,6378206.4,294.9786982139006,AUTHORITY[&quot;EPSG&quot;,&quot;7008&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;6267&quot;]],PRIMEM[&quot;Greenwich&quot;,0],UNIT[&quot;degree&quot;,0.0174532925199433],AUTHORITY[&quot;EPSG&quot;,&quot;4267&quot;]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;latitude_of_origin&quot;,0],PARAMETER[&quot;central_meridian&quot;,-117],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;false_easting&quot;,500000],PARAMETER[&quot;false_northing&quot;,0],UNIT[&quot;metre&quot;,1,AUTHORITY[&quot;EPSG&quot;,&quot;9001&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;26711&quot;]]">
    <GCP Id="" Pixel="0.0000" Line="0.0000" X="4.407200000000E+05" Y="3.751320000000E+06"/>
    <GCP Id="" Pixel="100.0000" Line="0.0000" X="4.467200000000E+05" Y="3.751320000000E+06"/>
    <GCP Id="" Pixel="0.0000" Line="100.0000" X="4.407200000000E+05" Y="3.745320000000E+06"/>
    <GCP Id="" Pixel="100.0000" Line="100.0000" X="4.467200000000E+05" Y="3.745320000000E+06"/>
  </GCPList>
</PAMDataset>""")
    f.close()

    ds = gdal.Open('tmp/tiff_write_81.tif')

    assert (ds.GetGCPProjection().find(
            'AUTHORITY["EPSG","26711"]') != -1), 'GCP Projection not set properly.'

    gcps = ds.GetGCPs()
    assert len(gcps) == 4, 'GCP count wrong.'

    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_81.tif')

###############################################################################
# Test writing & reading a signedbyte 8 bit geotiff


def test_tiff_write_82():

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_82.tif', src_ds, options=['PIXELTYPE=SIGNEDBYTE'])
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/tiff_write_82.tif')
    md = ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE')
    assert md['PIXELTYPE'] == 'SIGNEDBYTE', 'did not get SIGNEDBYTE'
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_82.tif')


###############################################################################
# Test writing & reading an indexed GeoTIFF with an extra transparency band (#3547)

def test_tiff_write_83():

    # Test Create() method
    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_83.tif', 1, 1, 2)
    ct = gdal.ColorTable()
    ct.SetColorEntry(127, (255, 255, 255, 255))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds.GetRasterBand(1).Fill(127)
    ds.GetRasterBand(2).Fill(255)
    ds = None

    # Test CreateCopy() method
    src_ds = gdal.Open('tmp/tiff_write_83.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_83_2.tif', src_ds)
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/tiff_write_83_2.tif')
    ct2 = ds.GetRasterBand(1).GetRasterColorTable()
    assert ct2.GetColorEntry(127) == (255, 255, 255, 255), \
        'did not get expected color table'
    ct2 = None
    cs1 = ds.GetRasterBand(1).Checksum()
    assert cs1 == 127 % 7, 'did not get expected checksum for band 1'
    cs2 = ds.GetRasterBand(2).Checksum()
    assert cs2 == 255 % 7, 'did not get expected checksum for band 2'
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_83.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_83_2.tif')

###############################################################################
# Test propagation of non-standard JPEG quality when the current directory
# changes in the midst of encoding of tiles (#3539)


def test_tiff_write_84():

    md = gdaltest.tiff_drv.GetMetadata()

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    with gdaltest.SetCacheMax(0):
        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_84.tif', 128, 128, 3)
        ds = None

        try:
            os.remove('tmp/tiff_write_84.tif.ovr')
        except OSError:
            pass

        ds = gdal.Open('tmp/tiff_write_84.tif')
        gdal.SetConfigOption('COMPRESS_OVERVIEW', 'JPEG')
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', '90')
        ds.BuildOverviews('NEAREST', overviewlist=[2])
        cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
        ds = None
        gdal.SetConfigOption('COMPRESS_OVERVIEW', None)
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', None)

    gdaltest.tiff_drv.Delete('tmp/tiff_write_84.tif')

    assert cs == 0, 'did not get expected checksum'

###############################################################################
# Test SetUnitType()


def test_tiff_write_85():

    # First part : test storing and retrieving unittype from internal metadata
    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_85.tif', 1, 1)
    ds.GetRasterBand(1).SetUnitType('ft')
    ds = None

    assert not os.path.exists('tmp/tiff_write_85.tif.aux.xml')

    ds = gdal.Open('tmp/tiff_write_85.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'ft', 'did not get expected values in internal case (1)'
    ds = None

    # Test CreateCopy()
    src_ds = gdal.Open('tmp/tiff_write_85.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_85_copy.tif', src_ds)
    src_ds = None
    ds = None
    ds = gdal.Open('tmp/tiff_write_85_copy.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'ft', 'did not get expected values in copy'
    ds = None
    gdaltest.tiff_drv.Delete('tmp/tiff_write_85_copy.tif')

    # Second part : test unsetting unittype from internal metadata
    ds = gdal.Open('tmp/tiff_write_85.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetUnitType(None)
    ds = None

    ds = gdal.Open('tmp/tiff_write_85.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    assert unittype == '', 'did not get expected values in internal case (2)'
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_85.tif')

    # Third part : test storing and retrieving unittype from PAM metadata
    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_85_bis.tif', 1, 1)
    assert not ds.GetRasterBand(1).GetUnitType(), 'expected None values'
    ds = None

    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    ds.GetRasterBand(1).SetUnitType('ft')
    ds = None

    try:
        # check that it *goes* to PAM
        os.stat('tmp/tiff_write_85_bis.tif.aux.xml')
    except OSError:
        pytest.fail('did not go to PAM as expected')

    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'ft', 'did not get expected values in PAM case (1)'
    ds = None

    # Fourth part : test unsetting unittype from PAM metadata
    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    ds.GetRasterBand(1).SetUnitType(None)
    ds = None

    assert not os.path.exists('tmp/tiff_write_85_bis.tif.aux.xml')

    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    assert unittype == '', 'did not get expected values in PAM case (2)'
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_85_bis.tif')

###############################################################################
# Test special handling of xml:ESRI domain.  When the ESRI_XML_PAM config
# option is set we want to write this to PAM, not into the geotiff itself.
# This is a special option so that ArcGIS 10 written geotiffs will still work
# properly with earlier versions of ArcGIS, requested by ESRI.


def test_tiff_write_86():

    gdal.SetConfigOption('ESRI_XML_PAM', 'YES')

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_86.tif', 100, 100,
                                  1, gdal.GDT_Byte)
    ds.SetMetadata(['<abc></abc>'], 'xml:ESRI')
    ds.SetMetadataItem('BaseTest', 'Value')
    ds = None

    # Is the xml:ESRI data available?
    ds = gdal.Open('tmp/tiff_write_86.tif')
    assert ds.GetMetadata('xml:ESRI') == ['<abc />\n'], \
        'did not get expected xml:ESRI metadata.'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value('missing metadata(1)')
        pytest.fail()
    ds = None

    # After removing the pam file is it gone, but the conventional
    # metadata still available?

    os.rename('tmp/tiff_write_86.tif.aux.xml',
              'tmp/tiff_write_86.tif.aux.xml.hidden')

    ds = gdal.Open('tmp/tiff_write_86.tif')
    assert ds.GetMetadata('xml:ESRI') is None, 'unexpectedly got xml:ESRI metadata'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value('missing metadata(2)')
        pytest.fail()

    ds = None

    # now confirm that CreateCopy also preserves things similarly.

    os.rename('tmp/tiff_write_86.tif.aux.xml.hidden',
              'tmp/tiff_write_86.tif.aux.xml')

    ds_src = gdal.Open('tmp/tiff_write_86.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_86_cc.tif', ds_src)
    ds_src = None
    ds = None

    # Is the xml:ESRI data available?
    ds = gdal.Open('tmp/tiff_write_86_cc.tif')
    assert ds.GetMetadata('xml:ESRI') == ['<abc />\n'], \
        'did not get expected xml:ESRI metadata (cc).'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value('missing metadata(1cc)')
        pytest.fail()
    ds = None

    # After removing the pam file is it gone, but the conventional
    # metadata still available?

    os.remove('tmp/tiff_write_86_cc.tif.aux.xml')

    ds = gdal.Open('tmp/tiff_write_86_cc.tif')
    assert ds.GetMetadata('xml:ESRI') is None, 'unexpectedly got xml:ESRI metadata(2)'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value('missing metadata(2cc)')
        pytest.fail()

    ds = None

    # Cleanup

    gdal.SetConfigOption('ESRI_XML_PAM', 'NO')

    gdaltest.tiff_drv.Delete('tmp/tiff_write_86.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_86_cc.tif')


###############################################################################
# Test COPY_SRC_OVERVIEWS creation option

def test_tiff_write_87():

    gdal.Translate('tmp/tiff_write_87_src.tif', 'data/utmsmall.tif', options='-a_nodata 0')

    src_ds = gdal.Open('tmp/tiff_write_87_src.tif', gdal.GA_Update)
    src_ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    expected_cs1 = src_ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs2 = src_ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_87_dst.tif', src_ds, options=['COPY_SRC_OVERVIEWS=YES', 'ENDIANNESS=LITTLE'])
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/tiff_write_87_dst.tif')
    cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    nodata_ovr_0 = ds.GetRasterBand(1).GetOverview(0).GetNoDataValue()
    nodata_ovr_1 = ds.GetRasterBand(1).GetOverview(1).GetNoDataValue()
    ifd_main = int(ds.GetRasterBand(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    ifd_ovr_0 = int(ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    ifd_ovr_1 = int(ds.GetRasterBand(1).GetOverview(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    data_ovr_1 = int(ds.GetRasterBand(1).GetOverview(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
    data_ovr_0 = int(ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
    data_main = int(ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
    size_main = int(ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_0', 'TIFF'))
    with open('tmp/tiff_write_87_dst.tif', 'rb') as f:
        f.seek(data_main - 4)
        size_from_header = struct.unpack('<I', f.read(4))[0]
        assert size_main == size_from_header
        f.seek(data_main + size_main - 4)
        last_bytes = f.read(4)
        last_bytes_repeated = f.read(4)
        assert last_bytes == last_bytes_repeated

    ds = None

    _check_cog('tmp/tiff_write_87_dst.tif', check_tiled=False, full_check=True)

    gdaltest.tiff_drv.Delete('tmp/tiff_write_87_src.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_87_dst.tif')

    # Check checksums
    assert cs1 == expected_cs1 and cs2 == expected_cs2, 'did not get expected checksums'

    assert nodata_ovr_0 == 0 and nodata_ovr_1 == 0, 'did not get expected nodata values'

    assert ifd_main == 8 or(ifd_main < ifd_ovr_0 and ifd_ovr_0 < ifd_ovr_1 and ifd_ovr_1 < data_ovr_1 and data_ovr_1 < data_ovr_0 and data_ovr_0 < data_main)

###############################################################################
# Test that COPY_SRC_OVERVIEWS creation option has an influence
# on BIGTIFF creation


def test_tiff_write_88():

    # The file would be > 4.2 GB without SPARSE_OK
    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_88_src.tif', 60000, 60000, 1,
                                      options=['TILED=YES', 'SPARSE_OK=YES'])
    src_ds.BuildOverviews('NONE', overviewlist=[2, 4])
    # Just write one data block so that we can truncate it
    data = src_ds.GetRasterBand(1).GetOverview(1).ReadRaster(0, 0, 128, 128)
    src_ds.GetRasterBand(1).GetOverview(1).WriteRaster(0, 0, 128, 128, data)
    src_ds = None

    # Truncate the file to cause an I/O error on reading
    # so that the CreateCopy() aborts quickly
    f = open('tmp/tiff_write_88_src.tif', 'rb')
    f.seek(0, 2)
    length = f.tell()
    f.seek(0, 0)
    data = f.read(length - 1)
    f.close()
    f = open('tmp/tiff_write_88_src.tif', 'wb')
    f.write(data)
    f.close()

    src_ds = gdal.Open('tmp/tiff_write_88_src.tif')
    # for testing only. We need to keep the file to check it was a bigtiff
    gdal.SetConfigOption('GTIFF_DELETE_ON_ERROR', 'NO')
    gdal.SetConfigOption('CHECK_DISK_FREE_SPACE', 'NO')  # we don't want free space to be an issue here
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_88_dst.tif', src_ds,
                                      options=['TILED=YES', 'COPY_SRC_OVERVIEWS=YES', 'ENDIANNESS=LITTLE'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('GTIFF_DELETE_ON_ERROR', None)
    gdal.SetConfigOption('CHECK_DISK_FREE_SPACE', None)
    del ds
    src_ds = None

    f = open('tmp/tiff_write_88_dst.tif', 'rb')
    data = f.read(8)
    f.close()

    os.remove('tmp/tiff_write_88_src.tif')
    os.remove('tmp/tiff_write_88_dst.tif')

    ar = struct.unpack('B' * 8, data)
    assert ar[2] == 43, 'not a BIGTIFF file'
    assert ar[4] == 8 and ar[5] == 0 and ar[6] == 0 and ar[7] == 0, \
        'first IFD is not at offset 8'

###############################################################################
# Test JPEG_QUALITY propagation while creating a (default compressed) mask band


def test_tiff_write_89():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    last_size = 0
    for quality in [90, 75, 30]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')

        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_89.tif', 1024, 1024, 3,
                                                  options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality])

        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', None)

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(1).GetMaskBand().Fill(255)

        src_ds = None
        ds = None

        # older versions of python don't have SEEK_END, add if missing.
        try:
            os.SEEK_END
        except AttributeError:
            os.SEEK_END = 2

        f = open('tmp/tiff_write_89.tif', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        # print('quality = %d, size = %d' % (quality, size))

        if quality != 90:
            assert size < last_size, 'did not get decreasing file sizes'

        last_size = size

    gdaltest.tiff_drv.Delete('tmp/tiff_write_89.tif')

###############################################################################
# Test JPEG_QUALITY propagation/override while creating (internal) overviews


def test_tiff_write_90():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    checksums = {}
    qualities = [90,75,75]
    for i, quality in enumerate(qualities):
        src_ds = gdal.Open('../gdrivers/data/utm.tif')
        fname = 'tmp/tiff_write_90_%d' % i

        ds = gdal.GetDriverByName('GTiff').Create(fname, 1024, 1024, 3,
                                                  options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality])

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        if i == 2:
            quality = 30
        with gdaltest.config_option('JPEG_QUALITY_OVERVIEW', '%d'%quality):
            ds.BuildOverviews('AVERAGE', overviewlist=[2, 4])

        src_ds = None
        ds = None

        ds = gdal.Open(fname)
        checksums[i] = [ ds.GetRasterBand(1).Checksum(),
                               ds.GetRasterBand(1).GetOverview(0).Checksum(),
                               ds.GetRasterBand(1).GetOverview(1).Checksum() ]
        ds = None
        gdaltest.tiff_drv.Delete(fname)

    assert checksums[0][0] != checksums[1][0]
    assert checksums[0][1] != checksums[1][1]
    assert checksums[0][2] != checksums[1][2]

    assert checksums[0][0] != checksums[2][0]
    assert checksums[0][1] != checksums[2][1]
    assert checksums[0][2] != checksums[2][2]

    assert checksums[1][0] == checksums[2][0]
    assert checksums[1][1] != checksums[2][1]
    assert checksums[1][2] != checksums[2][2]

###############################################################################
# Test WEBP_LEVEL propagation and overriding while creating overviews
# on a newly created dataset

@pytest.mark.parametrize("external_ovr", [True, False])
def test_tiff_write_90_webp(external_ovr):
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('WEBP') == -1:
        pytest.skip()

    checksums = {}
    qualities = [90,75,75]
    for i, quality in enumerate(qualities):
        src_ds = gdal.Open('../gdrivers/data/utm.tif')
        fname = 'tmp/tiff_write_90_webp_%d' % i

        ds = gdal.GetDriverByName('GTiff').Create(fname, 512, 512, 3,
                                                  options=['COMPRESS=WEBP', 'WEBP_LEVEL=%d' % quality])

        data = src_ds.GetRasterBand(1).ReadRaster()
        ds.GetRasterBand(1).WriteRaster(0, 0, 512, 512, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 512, 512, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 512, 512, data)
        if i == 2:
            quality = 30
        options = {}
        if external_ovr:
            ds = None
            ds = gdal.Open(fname)
            options['COMPRESS_OVERVIEW'] = 'WEBP'
        options['WEBP_LEVEL_OVERVIEW'] = '%d' % quality
        with gdaltest.config_options(options):
            ds.BuildOverviews('AVERAGE', overviewlist=[2, 4])

        src_ds = None
        ds = None

        ds = gdal.Open(fname)
        checksums[i] = [ ds.GetRasterBand(1).Checksum(),
                               ds.GetRasterBand(1).GetOverview(0).Checksum(),
                               ds.GetRasterBand(1).GetOverview(1).Checksum() ]
        ds = None
        gdaltest.tiff_drv.Delete(fname)

    assert checksums[0][0] != checksums[1][0]
    assert checksums[0][1] != checksums[1][1]
    assert checksums[0][2] != checksums[1][2]

    assert checksums[0][0] != checksums[2][0]
    assert checksums[0][1] != checksums[2][1]
    assert checksums[0][2] != checksums[2][2]

    assert checksums[1][0] == checksums[2][0]
    assert checksums[1][1] != checksums[2][1]
    assert checksums[1][2] != checksums[2][2]


###############################################################################
# Test JPEG_QUALITY propagation while creating (internal) overviews after re-opening

def test_tiff_write_91():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    checksums = {}
    for quality in [90, 75, 30]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')

        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_91.tif', 1024, 1024, 3,
                                                  options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality])

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds = None

        ds = gdal.Open('tmp/tiff_write_91.tif', gdal.GA_Update)
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', '%d' % quality)
        ds.BuildOverviews('NEAR', overviewlist=[2, 4])
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', None)

        src_ds = None
        ds = None

        ds = gdal.Open('tmp/tiff_write_91.tif')
        checksums[quality] = [ ds.GetRasterBand(1).Checksum(),
                               ds.GetRasterBand(1).GetOverview(0).Checksum(),
                               ds.GetRasterBand(1).GetOverview(1).Checksum() ]
        ds = None


    gdaltest.tiff_drv.Delete('tmp/tiff_write_91.tif')

    assert checksums[75][0] != checksums[90][0]
    assert checksums[75][1] != checksums[90][1]
    assert checksums[75][2] != checksums[90][2]

    assert checksums[75][0] != checksums[30][0]
    assert checksums[75][1] != checksums[30][1]
    assert checksums[75][2] != checksums[30][2]

    assert checksums[90][0] != checksums[30][0]
    assert checksums[90][1] != checksums[30][1]
    assert checksums[90][2] != checksums[30][2]

###############################################################################
# Test WEBP_LEVEL_OVERVIEW while creating (internal) overviews after re-opening

def test_tiff_write_91_webp():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('WEBP') == -1:
        pytest.skip()

    checksums = {}
    for quality in [90, 75, 30]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')
        fname = 'tmp/tiff_write_91_webp_%d' % quality

        ds = gdal.GetDriverByName('GTiff').Create(fname, 1024, 1024, 3,
                                                  options=['COMPRESS=WEBP'])

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds = None
        src_ds = None

        ds = gdal.Open(fname, gdal.GA_Update)
        with gdaltest.config_option('WEBP_LEVEL_OVERVIEW', '%d'%quality):
            ds.BuildOverviews('AVERAGE', overviewlist=[2, 4])

        ds = None

        ds = gdal.Open(fname)
        checksums[quality] = [ ds.GetRasterBand(1).Checksum(),
                               ds.GetRasterBand(1).GetOverview(0).Checksum(),
                               ds.GetRasterBand(1).GetOverview(1).Checksum() ]

        ds = None
        gdaltest.tiff_drv.Delete(fname)

    assert checksums[75][0] == checksums[90][0]
    assert checksums[75][1] != checksums[90][1]
    assert checksums[75][2] != checksums[90][2]

    assert checksums[75][0] == checksums[30][0]
    assert checksums[75][1] != checksums[30][1]
    assert checksums[75][2] != checksums[30][2]

    assert checksums[90][0] == checksums[30][0]
    assert checksums[90][1] != checksums[30][1]
    assert checksums[90][2] != checksums[30][2]

###############################################################################
# Test the effect of JPEG_QUALITY_OVERVIEW while creating (internal) overviews after re-opening
# This will test that we correctly guess the quality of the main dataset

def test_tiff_write_92():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    last_size = 0
    quality = 30
    for jpeg_quality_overview in [False, 30, 40]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')

        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_92.tif', 1024, 1024, 3,
                                                  options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality])

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds = None

        ds = gdal.Open('tmp/tiff_write_92.tif', gdal.GA_Update)
        assert ds.GetMetadataItem('JPEG_QUALITY', 'IMAGE_STRUCTURE') == str(quality)
        if jpeg_quality_overview is not False:
            gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', '%d' % jpeg_quality_overview)
        ds.BuildOverviews('NEAR', overviewlist=[2, 4])
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', None)

        src_ds = None
        ds = None

        f = open('tmp/tiff_write_92.tif', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        # print('quality = %d, size = %d' % (quality, size))

        if jpeg_quality_overview == 30:
            assert size == last_size, 'did not get equal file sizes'
        elif jpeg_quality_overview == 40:
            assert size > last_size, 'did not get growing file sizes'

        last_size = size

    gdaltest.tiff_drv.Delete('tmp/tiff_write_92.tif')

###############################################################################
# Test JPEG_QUALITY_OVERVIEW propagation while creating external overviews


def test_tiff_write_93():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gdrivers/data/utm.tif')
    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_93.tif', 1024, 1024, 3,
                                              options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'])

    data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
    ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
    ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
    ds = None

    src_ds = None

    last_size = 0
    for quality in [90, 75, 30]:

        try:
            os.remove('tmp/tiff_write_93.tif.ovr')
        except OSError:
            pass

        ds = gdal.Open('tmp/tiff_write_93.tif')
        gdal.SetConfigOption('COMPRESS_OVERVIEW', 'JPEG')
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', '%d' % quality)
        gdal.SetConfigOption('PHOTOMETRIC_OVERVIEW', 'YCBCR')
        ds.BuildOverviews('NEAR', overviewlist=[2, 4])
        gdal.SetConfigOption('COMPRESS_OVERVIEW', None)
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', None)
        gdal.SetConfigOption('PHOTOMETRIC_OVERVIEW', None)
        ds = None

        f = open('tmp/tiff_write_93.tif.ovr', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        # print('quality = %d, size = %d' % (quality, size))

        if quality != 90:
            assert size < last_size, 'did not get decreasing file sizes'

            assert not (quality == 30 and size >= 83000), \
                'file larger than expected. should be about 69100. perhaps jpeg quality is not well propagated'

        last_size = size

    gdaltest.tiff_drv.Delete('tmp/tiff_write_93.tif')


###############################################################################
# Test CreateCopy() of a dataset with a mask into a JPEG compressed dataset
# and check JPEG_QUALITY propagation without warning

def test_tiff_write_94():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_94_src.tif', 1024, 1024, 3)
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', None)
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, '\xff', 1, 1)

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/tiff_write_94_dst.tif', src_ds,
                                                  options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=30'])
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', None)

    src_ds = None
    ds = None

    ds = gdal.Open('tmp/tiff_write_94_dst.tif')
    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_94_src.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_94_dst.tif')

    assert cs == 3, 'wrong checksum'

###############################################################################
# Test that COPY_SRC_OVERVIEWS deal well with rounding issues when computing
# overview levels from the overview size


def test_tiff_write_95():

    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_95_src.tif', 7171, 6083, options=['SPARSE_OK=YES'])
    src_ds.BuildOverviews('NONE', overviewlist=[2, 4, 8, 16, 32, 64])
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_95_dst.tif', src_ds, options=['COPY_SRC_OVERVIEWS=YES'])
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)
    ok = ds is not None
    ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_95_src.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_95_dst.tif')

    assert ok

###############################################################################
# Test that COPY_SRC_OVERVIEWS combined with GDAL_TIFF_INTERNAL_MASK=YES work well


def test_tiff_write_96(other_options = [], nbands = 1, nbits = 8):

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_96_src.tif', 100, 100, nbands, options = ['NBITS=' + str(nbits)])
    src_ds.GetRasterBand(1).Fill(255 if nbits == 8 else 127)
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(
        25, 25, 50, 50, b'\xff', 1, 1)
    src_ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    expected_cs_mask = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    expected_cs_ovr_1 = src_ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs_ovr_mask_1 = src_ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
    expected_cs_ovr_2 = src_ds.GetRasterBand(1).GetOverview(1).Checksum()
    expected_cs_ovr_mask_2 = src_ds.GetRasterBand(1).GetOverview(1).GetMaskBand().Checksum()

    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_96_dst.tif', src_ds, options=['COPY_SRC_OVERVIEWS=YES'] + other_options + ['NBITS=' + str(nbits)])
    ds = None
    src_ds = None
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', None)

    ds = gdal.Open('tmp/tiff_write_96_dst.tif')
    cs = ds.GetRasterBand(1).Checksum()
    cs_mask = ds.GetRasterBand(1).GetMaskBand().Checksum()
    cs_ovr_1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs_ovr_mask_1 = ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
    cs_ovr_2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    cs_ovr_mask_2 = ds.GetRasterBand(1).GetOverview(1).GetMaskBand().Checksum()
    assert ds.GetMetadataItem('HAS_USED_READ_ENCODED_API', '_DEBUG_') == '1'
    ds = None

    assert [expected_cs, expected_cs_mask, expected_cs_ovr_1, expected_cs_ovr_mask_1, expected_cs_ovr_2, expected_cs_ovr_mask_2] == \
       [cs, cs_mask, cs_ovr_1, cs_ovr_mask_1, cs_ovr_2, cs_ovr_mask_2], \
        'did not get expected checksums'

    if check_libtiff_internal_or_at_least(4, 0, 11):
        with gdaltest.config_option('GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE', 'YES'):
            ds = gdal.Open('tmp/tiff_write_96_dst.tif')
            cs = ds.GetRasterBand(1).Checksum()
            cs_mask = ds.GetRasterBand(1).GetMaskBand().Checksum()
            cs_ovr_1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
            cs_ovr_mask_1 = ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
            cs_ovr_2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
            cs_ovr_mask_2 = ds.GetRasterBand(1).GetOverview(1).GetMaskBand().Checksum()

        assert [expected_cs, expected_cs_mask, expected_cs_ovr_1, expected_cs_ovr_mask_1, expected_cs_ovr_2, expected_cs_ovr_mask_2] == \
            [cs, cs_mask, cs_ovr_1, cs_ovr_mask_1, cs_ovr_2, cs_ovr_mask_2], \
            'did not get expected checksums'
        assert ds.GetMetadataItem('HAS_USED_READ_ENCODED_API', '_DEBUG_') == '0'
        ds = None

    _check_cog('tmp/tiff_write_96_dst.tif', check_tiled=False, full_check=True)

    gdaltest.tiff_drv.Delete('tmp/tiff_write_96_src.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_96_dst.tif')


def test_tiff_write_96_tiled_threads_nbits7_nbands1():
    return test_tiff_write_96(['TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=32', 'NUM_THREADS=ALL_CPUS'], nbands = 1, nbits = 7)

def test_tiff_write_96_tiled_threads_nbits7_nbands2():
    return test_tiff_write_96(['BIGTIFF=YES', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=32', 'NUM_THREADS=ALL_CPUS'], nbands = 2, nbits = 7)


###############################################################################
# Test that strile arrays are written after the IFD


def test_tiff_write_ifd_offsets():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100)
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.BuildOverviews('NEAR', overviewlist=[2, 4])

    filename = '/vsimem/test_tiff_write_ifd_offsets.tif'
    with gdaltest.config_option('GDAL_TIFF_INTERNAL_MASK', 'YES'):
        ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options=['COPY_SRC_OVERVIEWS=YES', 'TILED=YES', 'COMPRESS=LZW'])
    val0_ref = int(ds.GetRasterBand(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val1_ref = int(ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val2_ref = int(ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val3_ref = int(ds.GetRasterBand(1).GetOverview(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val4_ref = int(ds.GetRasterBand(1).GetOverview(0).GetMaskBand().GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val5_ref = int(ds.GetRasterBand(1).GetOverview(1).GetMaskBand().GetMetadataItem('IFD_OFFSET', 'TIFF'))
    ds = None

    assert val0_ref < val1_ref
    assert val1_ref < val2_ref
    assert val2_ref < val3_ref
    assert val3_ref < val4_ref
    assert val4_ref < val5_ref
    assert val5_ref < 1100

    # Retry with larger file
    src_ds = gdal.GetDriverByName('MEM').Create('', 4096, 4096)
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.BuildOverviews('NEAR', overviewlist=[2, 4])

    with gdaltest.config_option('GDAL_TIFF_INTERNAL_MASK', 'YES'):
        ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options=['COPY_SRC_OVERVIEWS=YES', 'TILED=YES', 'COMPRESS=LZW'])
    val0 = int(ds.GetRasterBand(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val1 = int(ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val2 = int(ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val3 = int(ds.GetRasterBand(1).GetOverview(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val4 = int(ds.GetRasterBand(1).GetOverview(0).GetMaskBand().GetMetadataItem('IFD_OFFSET', 'TIFF'))
    val5 = int(ds.GetRasterBand(1).GetOverview(1).GetMaskBand().GetMetadataItem('IFD_OFFSET', 'TIFF'))
    ds = None

    # Test rewriting but without changing strile size
    ds = gdal.Open(filename, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None
    assert gdal.GetLastErrorMsg() == ''
    f = gdal.VSIFOpenL(filename, 'rb')
    data = gdal.VSIFReadL(1, 1000, f).decode('LATIN1')
    gdal.VSIFCloseL(f)
    assert 'KNOWN_INCOMPATIBLE_EDITION=NO\n ' in data

    # Test rewriting with changing strile size
    ds = gdal.Open(filename, gdal.GA_Update)
    ds.GetRasterBand(1).WriteRaster(0,0,1,1,'x')
    ds = None
    assert gdal.GetLastErrorMsg() != ''
    f = gdal.VSIFOpenL(filename, 'rb')
    data = gdal.VSIFReadL(1, 1000, f).decode('LATIN1')
    gdal.VSIFCloseL(f)
    assert 'KNOWN_INCOMPATIBLE_EDITION=YES\n' in data

    gdal.GetDriverByName('GTiff').Delete(filename)

    assert (val0_ref, val1_ref, val2_ref, val3_ref, val4_ref, val5_ref) == (val0, val1, val2, val3, val4, val5)

###############################################################################
# Create a simple file by copying from an existing one - PixelIsPoint


def test_tiff_write_97():

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', 'FALSE')

    src_ds = gdal.Open('data/byte_point.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_97.tif', src_ds)

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem('AREA_OR_POINT')
    new_ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, 'did not get expected geotransform'

    assert md == 'Point', 'did not get expected AREA_OR_POINT value'

    gdaltest.tiff_drv.Delete('tmp/test_97.tif')

    # Again, but ignoring PixelIsPoint

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', 'TRUE')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_97_2.tif', src_ds)

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem('AREA_OR_POINT')
    new_ds = None
    src_ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, \
        'did not get expected geotransform when ignoring PixelIsPoint'

    assert md == 'Point', 'did not get expected AREA_OR_POINT value'

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', None)

    # read back this file with pixelispoint behavior enabled.

    new_ds = gdal.Open('tmp/test_97_2.tif')

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem('AREA_OR_POINT')
    new_ds = None

    gt_expected = (440660.0, 60.0, 0.0, 3751380.0, 0.0, -60.0)

    assert gt == gt_expected, \
        'did not get expected geotransform when ignoring PixelIsPoint (2)'

    assert md == 'Point', 'did not get expected AREA_OR_POINT value'

    gdaltest.tiff_drv.Delete('tmp/test_97_2.tif')

###############################################################################
# Create a rotated geotiff file (uses a geomatrix) with - PixelIsPoint


def test_tiff_write_98():

    with gdaltest.config_option('GTIFF_POINT_GEO_IGNORE', 'FALSE'):
        src_ds = gdal.Open('data/geomatrix.tif')

    with gdaltest.config_option('GTIFF_POINT_GEO_IGNORE', 'TRUE'):
        new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_98.tif', src_ds)

        gt = new_ds.GetGeoTransform()
        md = new_ds.GetMetadataItem('AREA_OR_POINT')
        new_ds = None
        src_ds = None

        gt_expected = (1841001.75, 1.5, -5.0, 1144003.25, -5.0, -1.5)

    assert gt == gt_expected, 'did not get expected geotransform'

    assert md == 'Point', 'did not get expected AREA_OR_POINT value'

    with gdaltest.config_option('GTIFF_POINT_GEO_IGNORE', 'FALSE'):

        new_ds = gdal.Open('tmp/test_98.tif')

        gt = new_ds.GetGeoTransform()
        md = new_ds.GetMetadataItem('AREA_OR_POINT')
        new_ds = None
        src_ds = None

    gt_expected = (1841003.5, 1.5, -5.0, 1144006.5, -5.0, -1.5)

    assert gt == gt_expected, 'did not get expected geotransform (2)'

    assert md == 'Point', 'did not get expected AREA_OR_POINT value'

    gdaltest.tiff_drv.Delete('tmp/test_98.tif')

###############################################################################
# Create a rotated geotiff file (uses a geomatrix) with - PixelIsPoint


def test_tiff_write_tiepoints_pixelispoint():

    tmpfilename = '/vsimem/test_tiff_write_tiepoints_pixelispoint.tif'

    gdal.Translate(tmpfilename, 'data/byte_gcp_pixelispoint.tif')
    ds = gdal.Open(tmpfilename)
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Point'
    assert ds.GetGCPCount() == 4
    gcp = ds.GetGCPs()[0]
    assert (gcp.GCPPixel == pytest.approx(0.5, abs=1e-5) and \
       gcp.GCPLine == pytest.approx(0.5, abs=1e-5) and \
       gcp.GCPX == pytest.approx(-180, abs=1e-5) and \
       gcp.GCPY == pytest.approx(90, abs=1e-5) and \
       gcp.GCPZ == pytest.approx(0, abs=1e-5))


    with gdaltest.config_option('GTIFF_POINT_GEO_IGNORE', 'YES'):
        gdal.Translate(tmpfilename, 'data/byte_gcp_pixelispoint.tif')
        ds = gdal.Open(tmpfilename)
        assert ds.GetMetadataItem('AREA_OR_POINT') == 'Point'
        assert ds.GetGCPCount() == 4
        gcp = ds.GetGCPs()[0]
        assert (gcp.GCPPixel == pytest.approx(0, abs=1e-5) and \
        gcp.GCPLine == pytest.approx(0, abs=1e-5) and \
        gcp.GCPX == pytest.approx(-180, abs=1e-5) and \
        gcp.GCPY == pytest.approx(90, abs=1e-5) and \
        gcp.GCPZ == pytest.approx(0, abs=1e-5))

    gdal.Unlink(tmpfilename)

###############################################################################
# Create copy into a RGB JPEG-IN-TIFF (#3887)


def test_tiff_write_99():

    src_ds = gdal.Open('data/rgbsmall.tif')
    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_99.tif', src_ds, options=['COMPRESS=JPEG'])
    del new_ds
    src_ds = None

    ds = gdal.Open('tmp/test_99.tif')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('tmp/test_99.tif')

    assert (cs1, cs2, cs3) == (21629, 21651, 21371), ('%d,%d,%d' % (cs1, cs2, cs3))

###############################################################################
# Create copy into a 2 band JPEG-IN-TIFF (#3887)


def test_tiff_write_100():

    src_ds = gdaltest.tiff_drv.Create('/vsimem/test_100_src.tif', 16, 16, 2)
    src_ds.GetRasterBand(1).Fill(255)
    new_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/test_100_dst.tif', src_ds, options=['COMPRESS=JPEG'])
    del new_ds
    src_ds = None

    ds = gdal.Open('/vsimem/test_100_dst.tif')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/test_100_src.tif')
    gdaltest.tiff_drv.Delete('/vsimem/test_100_dst.tif')

    assert (cs1, cs2) == (3118, 0), ('%d,%d' % (cs1, cs2))

###############################################################################
# Test CHUNKY_STRIP_READ_SUPPORT (#3894)
# We use random data so the compressed files are big enough to need partial
# reloading. tiff_write_78 doesn't produce enough big data to trigger this...


def test_tiff_write_101():

    md = gdaltest.tiff_drv.GetMetadata()
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if sys.platform.startswith('linux'):
        # Much faster to use /dev/urandom than python random generator !
        f = open('/dev/urandom', 'rb')
        rand_array = f.read(10 * 1024 * 1024)
        f.close()
    else:
        import random
        rand_array = b''.join(struct.pack('B', random.randint(0, 255)) for _ in range(10 * 1024 * 1024))

    f = open('tmp/tiff_write_101.bin', 'wb')
    f.write(rand_array)
    f.close()

    f = open('tmp/tiff_write_101.hdr', 'wb')
    f.write("""ENVI
samples = 2500
lines   = 4000
bands   = 1
header offset = 0
file type = ENVI Standard
data type = 1
interleave = bsq
byte order = 0
map info = {UTM, 1, 1, 440720.000000, 3751320.000000, 60.000000, 60.000000, 11, North}
band names = {
Band 1}""".encode('ascii'))
    f.close()

    src_ds = gdal.Open('tmp/tiff_write_101.bin')
    expected_cs = src_ds.GetRasterBand(1).Checksum()

    for compression_method in ['DEFLATE', 'LZW', 'JPEG', 'PACKBITS', 'LZMA']:
        if md['DMD_CREATIONOPTIONLIST'].find(compression_method) == -1:
            continue

        ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_101.tif', src_ds,
                                          options=['COMPRESS=' + compression_method, 'BLOCKXSIZE=2500', 'BLOCKYSIZE=4000'])
        ds = None

        ds = gdal.Open('tmp/tiff_write_101.tif')
        gdal.ErrorReset()
        cs = ds.GetRasterBand(1).Checksum()
        error_msg = gdal.GetLastErrorMsg()
        ds = None

        gdaltest.tiff_drv.Delete('tmp/tiff_write_101.tif')

        if error_msg != '':
            src_ds = None
            gdaltest.tiff_drv.Delete('tmp/tiff_write_101.bin')
            pytest.fail()

        if compression_method != 'JPEG' and cs != expected_cs:
            src_ds = None
            gdaltest.tiff_drv.Delete('tmp/tiff_write_101.bin')
            pytest.fail('for compression method %s, got %d instead of %d' % (compression_method, cs, expected_cs))

    src_ds = None
    gdaltest.tiff_drv.Delete('tmp/tiff_write_101.bin')

###############################################################################
# Test writing and reading back COMPD_CS


def test_tiff_write_102():

    if int(gdal.GetDriverByName('GTiff').GetMetadataItem('LIBGEOTIFF')) < 1600:
        pytest.skip('requires libgeotiff >= 1.6')

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_102.tif', 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(7401)
    name = sr.GetAttrValue('COMPD_CS')
    wkt = sr.ExportToWkt()
    ds.SetProjection(wkt)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_102.tif')
    wkt1 = ds.GetProjectionRef()
    ds = None

    with gdaltest.config_option('GTIFF_REPORT_COMPD_CS', 'NO'):
        ds = gdal.Open('/vsimem/tiff_write_102.tif')
        wkt2 = ds.GetProjectionRef()
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_102.tif')

    assert wkt1.startswith('COMPD_CS'), 'expected COMPD_CS, but got something else'

    assert not wkt2.startswith('COMPD_CS'), 'got COMPD_CS, but did not expected it'

    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt1)
    got_name = sr2.GetAttrValue('COMPD_CS')
    assert got_name == name, wkt2

###############################################################################
# Test -co COPY_SRC_OVERVIEWS=YES on a multiband source with external overviews (#3938)


def test_tiff_write_103():
    import test_cli_utilities
    if test_cli_utilities.get_gdaladdo_path() is None:
        pytest.skip()

    gdal.Translate('tmp/tiff_write_103_src.tif', 'data/rgbsmall.tif', options='-outsize 260 260')
    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' -ro tmp/tiff_write_103_src.tif 2')
    gdal.Translate('tmp/tiff_write_103_dst.tif', 'tmp/tiff_write_103_src.tif', options='-co COPY_SRC_OVERVIEWS=YES')

    src_ds = gdal.Open('tmp/tiff_write_103_src.tif')
    dst_ds = gdal.Open('tmp/tiff_write_103_dst.tif')
    src_cs = src_ds.GetRasterBand(1).GetOverview(0).Checksum()
    dst_cs = dst_ds.GetRasterBand(1).GetOverview(0).Checksum()
    src_ds = None
    dst_ds = None

    gdaltest.tiff_drv.Delete('tmp/tiff_write_103_src.tif')
    gdaltest.tiff_drv.Delete('tmp/tiff_write_103_dst.tif')

    assert src_cs == dst_cs, 'did not get expected checksum'


###############################################################################
# Confirm as best we can that we can write geotiff files with detailed
# projection parameters with the correct linear units set.  (#3901)

def test_tiff_write_104():

    src_ds = gdal.Open('data/spaf27_correct.tif')
    dst_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_104.tif', src_ds)

    src_ds = None
    del dst_ds

    ds = gdal.Open('tmp/test_104.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference(wkt)
    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    assert fe == pytest.approx(2000000.0, abs=0.001), 'did not get expected false easting'

    gdaltest.tiff_drv.Delete('tmp/test_104.tif')

###############################################################################
# Confirm as best we can that we can write geotiff files with detailed
# projection parameters with the correct linear units set.  (#3901)


def test_tiff_write_105():

    shutil.copyfile('data/bug4468.tif', 'tmp/bug4468.tif')

    # Update a pixel and close again.
    ds = gdal.Open('tmp/bug4468.tif', gdal.GA_Update)
    data = ds.ReadRaster(0, 0, 1, 1)
    ds.WriteRaster(0, 0, 1, 1, data)
    ds = None

    # Now check if the image is still intact.
    ds = gdal.Open('tmp/bug4468.tif')
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 2923, ('Did not get expected checksum, got %d.' % cs)

    ds = None

    gdaltest.tiff_drv.Delete('tmp/bug4468.tif')

###############################################################################
# Test the direct copy mechanism of JPEG source


def test_tiff_write_106(filename='../gdrivers/data/jpeg/byte_with_xmp.jpg', options=None, check_cs=True):

    if options is None:
        options = ['COMPRESS=JPEG']

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open(filename)
    nbands = src_ds.RasterCount
    src_cs = []
    for i in range(nbands):
        src_cs.append(src_ds.GetRasterBand(i + 1).Checksum())

    out_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_106.tif', src_ds, options=options)
    out_ds = None

    out_ds = gdal.Open('/vsimem/tiff_write_106.tif')
    cs = []
    for i in range(nbands):
        cs.append(out_ds.GetRasterBand(i + 1).Checksum())
    out_ds = None

    gdal.Unlink('/vsimem/tiff_write_106.tif')

    if check_cs:
        for i in range(nbands):
            assert cs[i] == src_cs[i], 'did not get expected checksum'
    else:
        for i in range(nbands):
            assert cs[i] != 0, 'did not get expected checksum'



def test_tiff_write_107():
    return test_tiff_write_106(options=['COMPRESS=JPEG', 'BLOCKYSIZE=8'])


def test_tiff_write_108():
    return test_tiff_write_106(options=['COMPRESS=JPEG', 'BLOCKYSIZE=20'])


def test_tiff_write_109():
    return test_tiff_write_106(options=['COMPRESS=JPEG', 'TILED=YES', 'BLOCKYSIZE=16', 'BLOCKXSIZE=16'])

# Strip organization of YCbCr does *NOT* give exact pixels w.r.t. original image


def test_tiff_write_110():
    return test_tiff_write_106(filename='../gdrivers/data/jpeg/albania.jpg', check_cs=False)

# Whole copy of YCbCr *DOES* give exact pixels w.r.t. original image


def test_tiff_write_111():
    return test_tiff_write_106(filename='../gdrivers/data/jpeg/albania.jpg', options=['COMPRESS=JPEG', 'BLOCKYSIZE=260'])


def test_tiff_write_111_bis():
    return test_tiff_write_106(filename='../gdrivers/data/jpeg/albania.jpg', options=['COMPRESS=JPEG', 'BLOCKYSIZE=260', 'INTERLEAVE=PIXEL'])


def test_tiff_write_111_ter():
    return test_tiff_write_106(filename='../gdrivers/data/jpeg/albania.jpg', options=['COMPRESS=JPEG', 'BLOCKYSIZE=260', 'INTERLEAVE=BAND'], check_cs=False)

# Tiled organization of YCbCr does *NOT* give exact pixels w.r.t. original image


def test_tiff_write_112():
    return test_tiff_write_106(filename='../gdrivers/data/jpeg/albania.jpg', options=['COMPRESS=JPEG', 'TILED=YES'], check_cs=False)

# The source is a JPEG in RGB colorspace (usually it is YCbCr).


def test_tiff_write_113():
    return test_tiff_write_106(filename='../gdrivers/data/jpeg/rgbsmall_rgb.jpg', options=['COMPRESS=JPEG', 'BLOCKYSIZE=8'])

###############################################################################
# Test CreateCopy() interruption


def test_tiff_write_114():

    tst = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672)

    return tst.testCreateCopy(vsimem=1, interrupt_during_copy=True)

###############################################################################
# Test writing a pixel interleaved RGBA JPEG-compressed TIFF


def test_tiff_write_115():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    tmpfilename = '/vsimem/tiff_write_115.tif'

    src_ds = gdal.Open('data/stefan_full_rgba.tif')
    ds = gdaltest.tiff_drv.CreateCopy(tmpfilename, src_ds, options=['COMPRESS=JPEG'])
    assert ds is not None
    ds = None
    src_ds = None

    f = gdal.VSIFOpenL(tmpfilename + '.aux.xml', 'rb')
    if f is not None:
        gdal.VSIFCloseL(f)
        gdal.Unlink(tmpfilename)
        pytest.fail()

    ds = gdal.Open(tmpfilename)
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['INTERLEAVE'] != 'PIXEL':
        ds = None
        gdal.Unlink(tmpfilename)
        pytest.fail()

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        if cs != expected_cs[i]:
            ds = None
            gdal.Unlink(tmpfilename)
            pytest.fail()

        if ds.GetRasterBand(i + 1).GetRasterColorInterpretation() != gdal.GCI_RedBand + i:
            ds = None
            gdal.Unlink(tmpfilename)
            pytest.fail()

    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################
# Test writing a band interleaved RGBA JPEG-compressed TIFF


def test_tiff_write_116():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    tmpfilename = '/vsimem/tiff_write_116.tif'

    src_ds = gdal.Open('data/stefan_full_rgba.tif')
    ds = gdaltest.tiff_drv.CreateCopy(tmpfilename, src_ds, options=['COMPRESS=JPEG', 'INTERLEAVE=BAND'])
    assert ds is not None
    ds = None
    src_ds = None

    f = gdal.VSIFOpenL(tmpfilename + '.aux.xml', 'rb')
    if f is not None:
        gdal.VSIFCloseL(f)
        gdal.Unlink(tmpfilename)
        pytest.fail()

    ds = gdal.Open(tmpfilename)
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['INTERLEAVE'] != 'BAND':
        ds = None
        gdal.Unlink(tmpfilename)
        pytest.fail()

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        if cs != expected_cs[i]:
            ds = None
            gdal.Unlink(tmpfilename)
            pytest.fail()

        if ds.GetRasterBand(i + 1).GetRasterColorInterpretation() != gdal.GCI_RedBand + i:
            ds = None
            gdal.Unlink(tmpfilename)
            pytest.fail()

    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################
# Test bugfix for ticket #4771 (rewriting of a deflate compressed tile, libtiff bug)


def test_tiff_write_117():
    # This fail with a libtiff 4.x older than 2012-08-13
    md = gdaltest.tiff_drv.GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        pytest.skip()

    import random

    # so that we have always the same random :-)
    random.seed(0)

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_117.tif', 512, 256, 2, options=['COMPRESS=DEFLATE', 'TILED=YES'])

    # Write first tile so that its byte count of that tile is 2048 (a multiple of 1024)
    adjust = 1254
    data = '0' * (65536 - adjust) + ''.join([('%c' % random.randint(0, 255)) for _ in range(adjust)])
    ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)

    # Second tile will be implicitly written at closing, or we could write
    # any content

    ds = None

    ds = gdal.Open('/vsimem/tiff_write_117.tif', gdal.GA_Update)

    # Will adjust tif_rawdatasize to TIFFroundup_64((uint64)size, 1024) = TIFFroundup_64(2048, 1024) = 2048
    ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256)

    # The new bytecount will be greater than 2048
    data = ''.join([('%c' % random.randint(0, 255)) for _ in range(256 * 256)])
    ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)

    # Make sure that data is written now
    ds.FlushCache()

    # Oops, without fix, the second tile will have been overwritten and an error will be emitted
    data = ds.GetRasterBand(1).ReadRaster(256, 0, 256, 256)

    ds = None

    gdal.Unlink('/vsimem/tiff_write_117.tif')

    assert data is not None, \
        'if GDAL is configured with external libtiff 4.x, it can fail if it is older than 4.0.3. With internal libtiff, should not fail'

###############################################################################
# Test bugfix for ticket gh #4538 (rewriting of a deflate compressed tile, libtiff bug)


def test_tiff_write_rewrite_in_place_issue_gh_4538():
    # This fail with libtiff <= 4.3.0
    md = gdaltest.tiff_drv.GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        pytest.skip()

    # Defeats the logic that fixed test_tiff_write_117

    import array
    filename = '/vsimem/tmp.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 144*2, 128, 1,
                                              options = ['TILED=YES',
                                                         'COMPRESS=PACKBITS',
                                                         'BLOCKXSIZE=144',
                                                         'BLOCKYSIZE=128'])
    x = ((144*128)//2) - 645
    ds.GetRasterBand(1).WriteRaster(0, 0, 144, 128,
                                    b'\x00' * x  + array.array('B', [i % 255 for i in range(144*128-x)]))
    block1_data = b'\x00' * (x + 8) + array.array('B', [i % 255 for i in range(144*128-(x+8))])
    ds.GetRasterBand(1).WriteRaster(144, 0, 144, 128, block1_data)
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    ds.GetRasterBand(1).ReadRaster(144, 0, 144, 128)
    block0_data = array.array('B', [i % 255 for i in range(144*128)])
    ds.GetRasterBand(1).WriteRaster(0, 0, 144, 128, block0_data)
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).ReadRaster(0, 0, 144, 128) == block0_data
    assert ds.GetRasterBand(1).ReadRaster(144, 0, 144, 128) == block1_data
    ds = None

    gdal.Unlink(filename)

###############################################################################
# Test bugfix for ticket #4816


def test_tiff_write_118():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_118.tif', 1, 1)
    # Should be rejected in a non-XML domain
    ds.SetMetadata('bla', 'foo')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_118.tif')
    md = ds.GetMetadata('foo')
    ds = None

    gdal.Unlink('/vsimem/tiff_write_118.tif')

    assert not md

###############################################################################
# Test bugfix for ticket #4816


def test_tiff_write_119():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_119.tif', 1, 1)
    ds.SetMetadata('foo=bar', 'foo')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_119.tif')
    md = ds.GetMetadata('foo')
    ds = None

    gdal.Unlink('/vsimem/tiff_write_119.tif')

    assert md['foo'] == 'bar'

###############################################################################
# Test bugfix for ticket #4816


def test_tiff_write_120():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_120.tif', 1, 1)
    ds.SetMetadata('<foo/>', 'xml:foo')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_120.tif')
    md = ds.GetMetadata('xml:foo')
    ds = None

    gdal.Unlink('/vsimem/tiff_write_120.tif')

    assert len(md) == 1
    assert md[0] == '<foo/>'

###############################################################################
# Test error cases of COPY_SRC_OVERVIEWS creation option


def test_tiff_write_121():

    # Test when the overview band is NULL
    src_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="0">non_existing</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_121.tif', src_ds, options=['COPY_SRC_OVERVIEWS=YES'])
    gdal.PopErrorHandler()
    assert ds is None
    src_ds = None

    # Test when the overview count isn't the same on all base bands
    src_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_121.tif', src_ds, options=['COPY_SRC_OVERVIEWS=YES'])
    gdal.PopErrorHandler()
    assert ds is None
    src_ds = None

    # Test when the overview bands of same level have not the same dimensions
    src_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="0">data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_121.tif', src_ds, options=['COPY_SRC_OVERVIEWS=YES'])
    gdal.PopErrorHandler()
    assert ds is None
    src_ds = None

###############################################################################
# Test write and read of some TIFFTAG_RESOLUTIONUNIT tags where '*'/'' is
# specified (gdalwarp conflicts)
# Expected to fail (properly) with older libtiff versions (<=3.8.2 for sure)


def test_tiff_write_122():
    new_ds = gdaltest.tiff_drv.Create('tmp/tags122.tif', 1, 1, 1)

    new_ds.SetMetadata({
        'TIFFTAG_RESOLUTIONUNIT': '*',
    })

    new_ds = None
    # hopefully it's closed now!

    new_ds = gdal.Open('tmp/tags122.tif')
    md = new_ds.GetMetadata()

    if 'TIFFTAG_RESOLUTIONUNIT' not in md:
        pytest.fail('Couldnt find tag TIFFTAG_RESOLUTIONUNIT')

    elif md['TIFFTAG_RESOLUTIONUNIT'] != '1 (unitless)':
        pytest.fail("Got unexpected tag TIFFTAG_RESOLUTIONUNIT='%s' (expected ='1 (unitless)')" % md['TIFFTAG_RESOLUTIONUNIT'])

    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/tags122.tif')

###############################################################################
# Test implicit photometric interpretation


def test_tiff_write_123():

    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_src.tif', 1, 1, 5, gdal.GDT_Int16)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(5).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    src_ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_src.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'
    src_ds = gdal.Open('/vsimem/tiff_write_123_src.tif')
    assert src_ds.GetMetadataItem('TIFFTAG_GDAL_METADATA', '_DEBUG_') is None, \
        'did not expect a TIFFTAG_GDAL_METADATA tag'
    assert src_ds.GetMetadataItem('TIFFTAG_PHOTOMETRIC', '_DEBUG_') == '2'
    assert src_ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert src_ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    assert src_ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert src_ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '0,2'

    new_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_123.tif', src_ds)
    del new_ds
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'
    ds = gdal.Open('/vsimem/tiff_write_123.tif')
    assert ds.GetMetadataItem('TIFFTAG_GDAL_METADATA', '_DEBUG_') is None, \
        'did not expect a TIFFTAG_GDAL_METADATA tag'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert src_ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    assert src_ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '0,2'
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_src.tif')
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123.tif')

    # From implicit RGB to BGR (with Photometric = MinIsBlack)
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_bgr.tif', 1, 1, 3, gdal.GDT_Byte)
    assert ds.GetMetadataItem('TIFFTAG_PHOTOMETRIC', '_DEBUG_') == '2'
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') is None
    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_BlueBand)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_RedBand)
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_bgr.tif.aux.xml',
                            gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect a PAM file'
    ds = gdal.Open('/vsimem/tiff_write_123_bgr.tif')
    assert ds.GetMetadataItem('TIFFTAG_PHOTOMETRIC', '_DEBUG_') == '1'
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '0,0'
    assert ds.GetMetadataItem('TIFFTAG_GDAL_METADATA', '_DEBUG_') is not None, \
        'expected a TIFFTAG_GDAL_METADATA tag'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None

    # Test overriding internal color interpretation with PAM one (read-only mode)
    ds = gdal.Open('/vsimem/tiff_write_123_bgr.tif')
    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_bgr.tif.aux.xml',
                            gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is not None, 'expected a PAM file'
    ds = gdal.Open('/vsimem/tiff_write_123_bgr.tif')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_bgr.tif')

    # Create a BGR with PROFILE=BASELINE --> no TIFFTAG_GDAL_METADATA tag, but .aux.xml instead
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_bgr.tif', 1, 1, 3,
                                  options=['PROFILE=BASELINE'])
    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_BlueBand)
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_RedBand)
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_bgr.tif.aux.xml',
                            gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is not None, 'expected a PAM file'
    ds = gdal.Open('/vsimem/tiff_write_123_bgr.tif')
    assert ds.GetMetadataItem('TIFFTAG_GDAL_METADATA', '_DEBUG_') is None, \
        'did not expect a TIFFTAG_GDAL_METADATA tag'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_bgr.tif')

    # From implicit RGBA to MINISBLACK
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_rgba.tif', 1, 1, 4, gdal.GDT_Byte)
    assert ds.GetMetadataItem('TIFFTAG_PHOTOMETRIC', '_DEBUG_') == '2'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '2'

    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_Undefined)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_Undefined
    assert ds.GetMetadataItem('TIFFTAG_PHOTOMETRIC', '_DEBUG_') == '1'
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') == '0,0,2'
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_rgba.tif')

    # From that implicit RGBA to Gray,Undefined,Undefined,Alpha doesn't
    # produce PAM file
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_guua.tif', 1, 1, 4, gdal.GDT_Byte)
    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GrayIndex)
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_Undefined)
    ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_Undefined)
    ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_guua.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'
    ds = gdal.Open('/vsimem/tiff_write_123_guua.tif')
    assert ds.GetMetadataItem('TIFFTAG_GDAL_METADATA', '_DEBUG_') is None, \
        'did not expect TIFFTAG_GDAL_METADATA tag'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_guua.tif')

    # Test that CreateCopy() from a RGB UInt16 doesn't generate ExtraSamples
    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_rgb_src.tif',
                                      1, 1, 3, gdal.GDT_UInt16, options=['PHOTOMETRIC=RGB'])
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_123_rgb.tif', src_ds)
    src_ds = None
    assert ds.GetMetadataItem('TIFFTAG_PHOTOMETRIC', '_DEBUG_') == '2'
    assert ds.GetMetadataItem('TIFFTAG_EXTRASAMPLES', '_DEBUG_') is None
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_rgb_src.tif')
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_rgb.tif')

    # Test that PHOTOMETRIC=RGB overrides the source color interpretation of the
    # first 3 bands
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 3)
    gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_123_rgb.tif', src_ds,
                                 options=['PHOTOMETRIC=RGB'])
    ds = gdal.Open('/vsimem/tiff_write_123_rgb.tif')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_rgb.tif')

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 5)
    src_ds.GetRasterBand(5).SetColorInterpretation(gdal.GCI_AlphaBand)
    gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_123_rgbua.tif', src_ds,
                                 options=['PHOTOMETRIC=RGB'])
    ds = gdal.Open('/vsimem/tiff_write_123_rgbua.tif')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    assert ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_rgbua.tif')

    # Test updating alpha to undefined
    gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_rgba_to_undefined.tif', 1, 1, 4,
                             options=['PHOTOMETRIC=RGB', 'ALPHA=YES'])
    ds = gdal.Open('/vsimem/tiff_write_123_rgba_to_undefined.tif', gdal.GA_Update)
    ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_Undefined)
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_rgba_to_undefined.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'
    ds = gdal.Open('/vsimem/tiff_write_123_rgba_to_undefined.tif')
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_rgba_to_undefined.tif')

###############################################################################
# Test error cases with palette creation


def test_tiff_write_124():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_124.tif', 1, 1, 3, gdal.GDT_Byte)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() can only be called on band 1"
    ret = ds.GetRasterBand(2).SetColorTable(gdal.ColorTable())
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() not supported for multi-sample TIFF files"
    ret = ds.GetRasterBand(1).SetColorTable(gdal.ColorTable())
    gdal.PopErrorHandler()
    assert ret != 0

    ds = None

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_124.tif', 1, 1, 1, gdal.GDT_UInt32)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() only supported for Byte or UInt16 bands in TIFF format."
    ret = ds.GetRasterBand(1).SetColorTable(gdal.ColorTable())
    gdal.PopErrorHandler()
    assert ret != 0
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() only supported for Byte or UInt16 bands in TIFF format."
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_124.tif', 1, 1, 1, gdal.GDT_UInt32, options=['PHOTOMETRIC=PALETTE'])
    gdal.PopErrorHandler()
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_124.tif')

###############################################################################
# Test out-of-memory conditions with SplitBand and SplitBitmapBand


def test_tiff_write_125():

    if gdal.GetConfigOption('SKIP_MEM_INTENSIVE_TEST') is not None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_125.tif', 2147000000, 5000, 65535, options=['SPARSE_OK=YES', 'BLOCKYSIZE=5000', 'COMPRESS=LZW', 'BIGTIFF=NO'])
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_125.tif')
    # Will not open on 32-bit due to overflow
    if ds is not None:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds.GetRasterBand(1).ReadBlock(0, 0)
        gdal.PopErrorHandler()

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_125.tif', 2147000000, 5000, 1, options=['NBITS=1', 'SPARSE_OK=YES', 'BLOCKYSIZE=5000', 'COMPRESS=LZW', 'BIGTIFF=NO'])
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_125.tif')
    # Will not open on 32-bit due to overflow
    if ds is not None:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds.GetRasterBand(1).ReadBlock(0, 0)
        gdal.PopErrorHandler()

    gdal.Unlink('/vsimem/tiff_write_125.tif')

###############################################################################
# Test implicit JPEG-in-TIFF overviews


def test_tiff_write_126():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gdrivers/data/small_world_400pct.vrt')

    options_list = [(['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'], [48788, 56561, 56462], [61397, 2463, 2454, 2727], [29605, 33654, 34633], [10904, 10453, 10361]),
                    (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEGTABLESMODE=0'], [48788, 56561, 56462], [61397, 2463, 2454, 2727], [29605, 33654, 34633], [10904, 10453, 10361]),
                    (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'TILED=YES'], [48788, 56561, 56462], [61397, 2463, 2454, 2727], [29605, 33654, 34633], [10904, 10453, 10361]),
                    (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'BLOCKYSIZE=800'], [48788, 56561, 56462], [61397, 2463, 2454, 2727], [29605, 33654, 34633], [10904, 10453, 10361]),
                    (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'BLOCKYSIZE=64'], [48788, 56561, 56462], [61397, 2463, 2454, 2727], [29605, 33654, 34633], [10904, 10453, 10361]),
                    (['COMPRESS=JPEG'], [49887, 58937], [59311, 2826], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'INTERLEAVE=BAND'], [49887, 58937], [59311, 2826], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'INTERLEAVE=BAND', 'TILED=YES'], [49887, 58937], [59311, 2826], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'INTERLEAVE=BAND', 'BLOCKYSIZE=800'], [49887, 58937], [59311, 2826], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'INTERLEAVE=BAND', 'BLOCKYSIZE=32'], [49887, 58937], [59311, 2826], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'BLOCKYSIZE=8'], [49887, 58937], [59311, 2826], [30829, 34806], [11664, 58937]),
                   ]

    for (options, cs1, cs2, cs3, cs4) in options_list:
        os.environ['JPEGMEM'] = '500M'
        ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_126.tif', src_ds, options=options)
        ds = None
        del os.environ['JPEGMEM']

        ds = gdal.Open('/vsimem/tiff_write_126.tif')
        # Officially we have 0 public overviews...
        assert ds.GetRasterBand(1).GetOverviewCount() == 0, options
        # But they do exist...
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs in cs1, options
        cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
        assert cs in cs2, options
        cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
        assert cs in cs3, options
        cs = ds.GetRasterBand(1).GetOverview(2).Checksum()
        assert cs in cs4, options
        assert ds.GetRasterBand(1).GetOverview(-1) is None, options
        assert ds.GetRasterBand(1).GetOverview(3) is None, options
        ovr_1_data = ds.GetRasterBand(1).GetOverview(1).GetDataset().ReadRaster(0, 0, 400, 200)
        subsampled_data = ds.ReadRaster(0, 0, 1600, 800, 400, 200)
        assert ovr_1_data == subsampled_data, options
        ds = None

        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    src_ds = gdal.Open('../gdrivers/data/small_world_400pct_1band.vrt')

    options_list = [(['COMPRESS=JPEG'], [49887, 58937], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'TILED=YES'], [49887, 58937], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'BLOCKYSIZE=800'], [49887, 58937], [30829, 34806], [11664, 58937]),
                    (['COMPRESS=JPEG', 'BLOCKYSIZE=32'], [49887, 58937], [30829, 34806], [11664, 58937]),
                   ]

    for (options, cs1, cs3, cs4) in options_list:
        os.environ['JPEGMEM'] = '500M'
        ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_126.tif', src_ds, options=options)
        ds = None
        del os.environ['JPEGMEM']

        ds = gdal.Open('/vsimem/tiff_write_126.tif')
        # Officially we have 0 public overviews...
        assert ds.GetRasterBand(1).GetOverviewCount() == 0, options
        # But they do exist...
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs in cs1, options
        cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
        assert cs in cs3, options
        cs = ds.GetRasterBand(1).GetOverview(2).Checksum()
        assert cs in cs4, options
        ovr_1_data = ds.GetRasterBand(1).GetOverview(1).GetDataset().ReadRaster(0, 0, 400, 200)
        subsampled_data = ds.ReadRaster(0, 0, 1600, 800, 400, 200)
        assert ovr_1_data == subsampled_data, options
        ds = None

        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    # Test single-strip, opened as split band
    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_126_src.tif', 8, 2001)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_126.tif', src_ds, options=['COMPRESS=JPEG', 'BLOCKYSIZE=2001'])
    src_ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126_src.tif')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_126.tif')
    assert ds.GetRasterBand(1).GetBlockSize() == [8, 1]
    ovr_ds = ds.GetRasterBand(1).GetOverview(1).GetDataset()
    ovr_1_data = ovr_ds.ReadRaster(0, 0, ovr_ds.RasterXSize, ovr_ds.RasterYSize, 1, 1)
    subsampled_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 1, 1)
    assert ovr_1_data == subsampled_data
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    # We need libtiff 4.0.4 (unreleased at that time)
    if md['LIBTIFF'] != 'INTERNAL':
        print('skipping tests that will fail without internal libtiff')
        return

    # Test with completely sparse file
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_126.tif', 1024, 1024, options=['COMPRESS=JPEG', 'SPARSE_OK=YES'])
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_126.tif')
    assert ds.GetRasterBand(1).GetOverview(0) is not None
    assert ds.GetRasterBand(1).GetMetadataItem('JPEGTABLES', 'TIFF') is not None
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_0', 'TIFF') is None
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    # Test with partially sparse file
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_126.tif', 1024, 1024, 3, options=['COMPRESS=JPEG', 'SPARSE_OK=YES', 'INTERLEAVE=BAND'])
    # Fill band 3, but let blocks of band 1 unwritten.
    ds.GetRasterBand(3).Fill(0)
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_126.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 0
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

###############################################################################
# Test setting/unsetting metadata in update mode (#5628)


def test_tiff_write_127():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_127.tif', 1, 1)
    ds = None

    for i in range(2):

        ds = gdal.Open('/vsimem/tiff_write_127.tif', gdal.GA_Update)
        obj = ds if i == 0 else ds.GetRasterBand(1)
        obj.SetMetadata({'key': 'value'})
        obj = None
        ds = None

        ds = gdal.Open('/vsimem/tiff_write_127.tif', gdal.GA_Update)
        obj = ds if i == 0 else ds.GetRasterBand(1)
        if obj.GetMetadataItem('key') != 'value':
            print(i)
            pytest.fail(obj.GetMetadata())
        obj.SetMetadata({})
        obj = None
        ds = None

        ds = gdal.Open('/vsimem/tiff_write_127.tif', gdal.GA_Update)
        obj = ds if i == 0 else ds.GetRasterBand(1)
        assert not obj.GetMetadata(), i
        obj.SetMetadataItem('key', 'value')
        obj = None
        ds = None

        ds = gdal.Open('/vsimem/tiff_write_127.tif', gdal.GA_Update)
        obj = ds if i == 0 else ds.GetRasterBand(1)
        assert obj.GetMetadataItem('key') == 'value', i
        obj.SetMetadataItem('key', None)
        obj = None
        ds = None

        ds = gdal.Open('/vsimem/tiff_write_127.tif', gdal.GA_Update)
        obj = ds if i == 0 else ds.GetRasterBand(1)
        assert not obj.GetMetadata(), i
        obj = None
        ds = None

        statBuf = gdal.VSIStatL('/vsimem/tiff_write_127.tif.aux.xml')
        if statBuf is not None:
            print(i)
            pytest.fail('unexpected PAM file')

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_127.tif')

###############################################################################
# Test lossless copying of a CMYK JPEG into JPEG-in-TIFF (#5712)


def test_tiff_write_128():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'NO')
    src_ds = gdal.Open('../gdrivers/data/jpeg/rgb_ntf_cmyk.jpg')
    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', None)

    # Will received implicitly CMYK photometric interpretation.
    old_val = gdal.GetConfigOption('GDAL_PAM_ENABLED')
    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'NO')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_128.tif', src_ds, options=['COMPRESS=JPEG'])
    ds = None
    gdal.SetConfigOption('GDAL_PAM_ENABLED', old_val)

    # We need to reopen in raw to avoig automatic CMYK->RGBA to trigger
    ds = gdal.Open('GTIFF_RAW:/vsimem/tiff_write_128.tif')
    for i in range(4):
        assert src_ds.GetRasterBand(i + 1).GetColorInterpretation() == ds.GetRasterBand(i + 1).GetColorInterpretation()
        assert src_ds.GetRasterBand(i + 1).Checksum() == ds.GetRasterBand(i + 1).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_128.tif')

    # Try with explicit CMYK photometric interpretation
    old_val = gdal.GetConfigOption('GDAL_PAM_ENABLED')
    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'NO')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_128.tif', src_ds, options=['COMPRESS=JPEG', 'PHOTOMETRIC=CMYK'])
    ds = None
    gdal.SetConfigOption('GDAL_PAM_ENABLED', old_val)

    # We need to reopen in raw to avoig automatic CMYK->RGBA to trigger
    ds = gdal.Open('GTIFF_RAW:/vsimem/tiff_write_128.tif')
    for i in range(4):
        assert src_ds.GetRasterBand(i + 1).GetColorInterpretation() == ds.GetRasterBand(i + 1).GetColorInterpretation()
        assert src_ds.GetRasterBand(i + 1).Checksum() == ds.GetRasterBand(i + 1).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_128.tif')

    # Try with more neutral colorspace in the case the source JPEG is not really CMYK (yes that happens !)
    old_val = gdal.GetConfigOption('GDAL_PAM_ENABLED')
    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'NO')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_128.tif', src_ds, options=['COMPRESS=JPEG', 'PHOTOMETRIC=MINISBLACK', 'PROFILE=BASELINE'])
    ds = None
    gdal.SetConfigOption('GDAL_PAM_ENABLED', old_val)

    # Here we can reopen without GTIFF_RAW trick
    ds = gdal.Open('/vsimem/tiff_write_128.tif')
    for i in range(4):
        # The color interpretation will NOT be CMYK
        assert src_ds.GetRasterBand(i + 1).GetColorInterpretation() != ds.GetRasterBand(i + 1).GetColorInterpretation()
        assert src_ds.GetRasterBand(i + 1).Checksum() == ds.GetRasterBand(i + 1).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_128.tif')

###############################################################################
# Check effective guessing of existing JPEG quality


def test_tiff_write_129():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    for jpegtablesmode in ['1', '3']:
        for photometric in ['RGB', 'YCBCR']:
            cs_ref = 0
            for i in range(2):
                ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_129.tif', 64, 32, 3,
                                              options=['COMPRESS=JPEG', 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32', 'JPEG_QUALITY=50', 'PHOTOMETRIC=' + photometric, 'JPEGTABLESMODE=' + jpegtablesmode])
                src_ds = gdal.Open('data/rgbsmall.tif')
                data = src_ds.ReadRaster(0, 0, 32, 32)
                ds.WriteRaster(0, 0, 32, 32, data)

                # In second pass, we re-open the dataset
                if i == 1:
                    ds = None
                    ds = gdal.Open('/vsimem/tiff_write_129.tif', gdal.GA_Update)
                ds.WriteRaster(32, 0, 32, 32, data)
                ds = None

                ds = gdal.Open('/vsimem/tiff_write_129.tif')
                with gdaltest.SetCacheMax(0):
                    cs = ds.GetRasterBand(1).Checksum()
                ds = None
                gdaltest.tiff_drv.Delete('/vsimem/tiff_write_129.tif')

                if i == 0:
                    cs_ref = cs
                elif cs != cs_ref:
                    print(photometric)
                    print(i)
                    pytest.fail(jpegtablesmode)


###############################################################################
# Test cases where JPEG quality will fail


def test_tiff_write_130():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    shutil.copyfile('data/byte_jpg_unusual_jpegtable.tif', 'tmp/byte_jpg_unusual_jpegtable.tif')
    ds = gdal.Open('tmp/byte_jpg_unusual_jpegtable.tif', gdal.GA_Update)
    assert ds.GetRasterBand(1).Checksum() == 4771
    src_ds = gdal.Open('data/byte.tif', gdal.GA_Update)
    ds.WriteRaster(0, 0, 20, 20, src_ds.ReadRaster())
    src_ds = None
    ds = None
    ds = gdal.Open('tmp/byte_jpg_unusual_jpegtable.tif')
    assert ds.GetRasterBand(1).Checksum() == 4743
    ds = None
    os.unlink('tmp/byte_jpg_unusual_jpegtable.tif')

    shutil.copyfile('data/byte_jpg_tablesmodezero.tif', 'tmp/byte_jpg_tablesmodezero.tif')
    ds = gdal.Open('tmp/byte_jpg_tablesmodezero.tif', gdal.GA_Update)
    assert ds.GetRasterBand(1).Checksum() == 4743
    src_ds = gdal.Open('data/byte.tif', gdal.GA_Update)
    ds.WriteRaster(0, 0, 20, 20, src_ds.ReadRaster())
    src_ds = None
    ds = None
    ds = gdal.Open('tmp/byte_jpg_tablesmodezero.tif')
    assert ds.GetRasterBand(1).Checksum() == 4743
    ds = None
    os.unlink('tmp/byte_jpg_tablesmodezero.tif')

###############################################################################
# Test LZMA compression


def test_tiff_write_131(level=1):

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LZMA') == -1:
        pytest.skip()

    filename = '/vsimem/tiff_write_131.tif'
    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy(filename, src_ds,
                                 options=['COMPRESS=LZMA', 'LZMA_PRESET=' + str(level)])
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None

    # LZMA requires an howful amount of memory even on small files
    if gdal.GetLastErrorMsg().find('cannot allocate memory') >= 0:
        gdal.Unlink(filename)
        pytest.skip()

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None

    gdal.Unlink(filename)


def test_tiff_write_131_level_9():
    return test_tiff_write_131(level=9)


###############################################################################
# Test that PAM metadata is cleared when internal metadata is set (#5807)


def test_tiff_write_132():

    for i in range(2):

        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_132.tif', 1, 1)
        ds = None

        # Open in read-only
        ds = gdal.Open('/vsimem/tiff_write_132.tif')
        ds.SetMetadataItem('FOO', 'BAR')
        ds.GetRasterBand(1).SetMetadataItem('FOO', 'BAR')
        ds = None

        # Check that PAM file exists
        assert gdal.VSIStatL('/vsimem/tiff_write_132.tif.aux.xml') is not None

        # Open in read-write
        ds = gdal.Open('/vsimem/tiff_write_132.tif', gdal.GA_Update)
        if i == 0:
            ds.SetMetadataItem('FOO', 'BAZ')
            ds.GetRasterBand(1).SetMetadataItem('FOO', 'BAZ')
        else:
            ds.SetMetadata({'FOO': 'BAZ'})
            ds.GetRasterBand(1).SetMetadata({'FOO': 'BAZ'})
        ds = None

        # Check that PAM file no longer exists
        assert gdal.VSIStatL('/vsimem/tiff_write_132.tif.aux.xml') is None, i

        ds = gdal.Open('/vsimem/tiff_write_132.tif')
        assert ds.GetMetadataItem('FOO') == 'BAZ' and ds.GetRasterBand(1).GetMetadataItem('FOO') == 'BAZ'
        ds = None

        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_132.tif')


###############################################################################
# Test streaming capabilities


def test_tiff_write_133():

    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_133.tif', 1024, 1000, 3, options=['STREAMABLE_OUTPUT=YES'])
    src_ds.SetGeoTransform([1, 2, 0, 3, 0, -2])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    src_ds.SetProjection(srs.ExportToWkt())
    src_ds.SetMetadataItem('FOO', 'BAR')
    src_ds.GetRasterBand(1).SetNoDataValue(127)
    src_ds.GetRasterBand(1).Fill(64)
    src_ds.GetRasterBand(2).Fill(127)
    src_ds.GetRasterBand(3).Fill(184)

    src_ds.FlushCache()
    gdal.PushErrorHandler()
    ret = src_ds.SetProjection(srs.ExportToWkt())
    gdal.PopErrorHandler()
    assert ret != 0
    gdal.PushErrorHandler()
    ret = src_ds.SetGeoTransform([1, 2, 0, 3, 0, -4])
    gdal.PopErrorHandler()
    assert ret != 0
    gdal.PushErrorHandler()
    ret = src_ds.SetMetadataItem('FOO', 'BAZ')
    gdal.PopErrorHandler()
    assert ret != 0
    gdal.PushErrorHandler()
    ret = src_ds.SetMetadata({})
    gdal.PopErrorHandler()
    assert ret != 0
    gdal.PushErrorHandler()
    ret = src_ds.GetRasterBand(1).SetMetadataItem('FOO', 'BAZ')
    gdal.PopErrorHandler()
    assert ret != 0
    gdal.PushErrorHandler()
    ret = src_ds.GetRasterBand(1).SetMetadata({})
    gdal.PopErrorHandler()
    assert ret != 0
    gdal.PushErrorHandler()
    ret = src_ds.GetRasterBand(1).SetNoDataValue(0)
    gdal.PopErrorHandler()
    assert ret != 0

    # Pixel interleaved
    out_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_133_dst.tif', src_ds, options=['STREAMABLE_OUTPUT=YES', 'BLOCKYSIZE=32'])
    out_ds = None

    gdal.SetConfigOption('TIFF_READ_STREAMING', 'YES')
    ds = gdal.Open('/vsimem/tiff_write_133_dst.tif')
    gdal.SetConfigOption('TIFF_READ_STREAMING', None)
    assert ds.GetProjectionRef().find('32601') >= 0
    assert ds.GetGeoTransform() == (1.0, 2.0, 0.0, 3.0, 0.0, -2.0)
    assert ds.GetMetadataItem('FOO') == 'BAR'
    assert ds.GetMetadataItem('UNORDERED_BLOCKS', 'TIFF') is None

    with gdaltest.SetCacheMax(0):
        for y in range(1000):
            got_data = ds.ReadRaster(0, y, 1024, 1)
            assert got_data is not None

    ds.FlushCache()
    for y in range(1000):
        gdal.PushErrorHandler()
        got_data = ds.ReadRaster(0, y, 1024, 1)
        gdal.PopErrorHandler()
        assert got_data is None
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_133_dst.tif')

    # Tiled
    out_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_133_dst.tif', src_ds, options=['STREAMABLE_OUTPUT=YES', 'TILED=YES'])
    out_ds = None

    gdal.SetConfigOption('TIFF_READ_STREAMING', 'YES')
    ds = gdal.Open('/vsimem/tiff_write_133_dst.tif')
    gdal.SetConfigOption('TIFF_READ_STREAMING', None)
    assert ds.GetProjectionRef().find('32601') >= 0
    assert ds.GetGeoTransform() == (1.0, 2.0, 0.0, 3.0, 0.0, -2.0)
    assert ds.GetMetadataItem('FOO') == 'BAR'
    assert ds.GetMetadataItem('UNORDERED_BLOCKS', 'TIFF') is None

    with gdaltest.SetCacheMax(0):
        for yblock in range(int((1000 + 256 - 1) / 256)):
            y = 256 * yblock
            ysize = 256
            if y + ysize > ds.RasterYSize:
                ysize = ds.RasterYSize - y
            for xblock in range(int((1024 + 256 - 1) / 256)):
                x = 256 * xblock
                xsize = 256
                if x + xsize > ds.RasterXSize:
                    xsize = ds.RasterXSize - x
                got_data = ds.ReadRaster(x, y, xsize, ysize)
                assert got_data is not None

    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_133_dst.tif')

    # Band interleaved
    out_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_133_dst.tif', src_ds, options=['STREAMABLE_OUTPUT=YES', 'INTERLEAVE=BAND'])
    out_ds = None

    gdal.SetConfigOption('TIFF_READ_STREAMING', 'YES')
    ds = gdal.Open('/vsimem/tiff_write_133_dst.tif')
    gdal.SetConfigOption('TIFF_READ_STREAMING', None)
    assert ds.GetMetadataItem('UNORDERED_BLOCKS', 'TIFF') is None

    with gdaltest.SetCacheMax(0):
        for band in range(3):
            for y in range(1000):
                got_data = ds.GetRasterBand(band + 1).ReadRaster(0, y, 1024, 1)
                assert got_data is not None
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_133_dst.tif')

    # BIGTIFF
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') >= 0:
        out_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_133_dst.tif', src_ds, options=['STREAMABLE_OUTPUT=YES', 'BIGTIFF=YES'])
        out_ds = None

        gdal.SetConfigOption('TIFF_READ_STREAMING', 'YES')
        ds = gdal.Open('/vsimem/tiff_write_133_dst.tif')
        gdal.SetConfigOption('TIFF_READ_STREAMING', None)
        assert ds.GetMetadataItem('UNORDERED_BLOCKS', 'TIFF') is None

        with gdaltest.SetCacheMax(0):
            for y in range(1000):
                got_data = ds.ReadRaster(0, y, 1024, 1)
                assert got_data is not None

        ds = None
        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_133_dst.tif')

    # Compression not supported
    gdal.PushErrorHandler()
    out_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_133_dst.tif', src_ds, options=['STREAMABLE_OUTPUT=YES', 'COMPRESS=DEFLATE'])
    gdal.PopErrorHandler()
    assert out_ds is None

    # Test writing into a non authorized file
    ds = gdaltest.tiff_drv.Create('/foo/bar', 1024, 1000, 3, options=['STREAMABLE_OUTPUT=YES', 'BLOCKYSIZE=1'])
    assert ds is None

    gdal.PushErrorHandler()
    out_ds = gdaltest.tiff_drv.CreateCopy('/foo/bar', src_ds, options=['STREAMABLE_OUTPUT=YES'])
    gdal.PopErrorHandler()
    assert out_ds is None

    src_ds = None

    # Classical TIFF with IFD not at offset 8
    gdal.SetConfigOption('TIFF_READ_STREAMING', 'YES')
    gdal.PushErrorHandler()
    ds = gdal.Open('data/byte.tif')
    gdal.PopErrorHandler()
    gdal.SetConfigOption('TIFF_READ_STREAMING', None)
    assert ds is None

    # BigTIFF with IFD not at offset 16
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') >= 0:
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_133.tif', 1024, 1000, 3, options=['BIGTIFF=YES'])
        ds.GetRasterBand(1).Fill(0)
        ds.FlushCache()
        ds.SetGeoTransform([1, 2, 0, 3, 0, -2])
        ds = None

        gdal.SetConfigOption('TIFF_READ_STREAMING', 'YES')
        gdal.PushErrorHandler()
        ds = gdal.Open('/vsimem/tiff_write_133.tif')
        gdal.PopErrorHandler()
        gdal.SetConfigOption('TIFF_READ_STREAMING', None)
        assert ds is None

    # Test reading strips in not increasing order
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_133.tif', 1024, 1000, 3, options=['BLOCKYSIZE=1'])
    for y in range(1000):
        ds.WriteRaster(0, 1000 - y - 1, 1024, 1, 'a' * (3 * 1024))
        ds.FlushCache()
    ds = None

    gdal.SetConfigOption('TIFF_READ_STREAMING', 'YES')
    gdal.PushErrorHandler()
    ds = gdal.Open('/vsimem/tiff_write_133.tif')
    gdal.PopErrorHandler()
    gdal.SetConfigOption('TIFF_READ_STREAMING', None)
    assert ds.GetMetadataItem('UNORDERED_BLOCKS', 'TIFF') == 'YES'

    with gdaltest.SetCacheMax(0):
        for y in range(1000):
            got_data = ds.ReadRaster(0, 1000 - y - 1, 1024, 1)
            assert got_data is not None

    # Test writing strips in not increasing order in a streamable output
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_133.tif', 1024, 1000, 3, options=['STREAMABLE_OUTPUT=YES', 'BLOCKYSIZE=1'])
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = ds.WriteRaster(0, 999, 1024, 1, 'a' * (3 * 1024))
    ds.FlushCache()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    ds = None

    # Test writing tiles in not increasing order in a streamable output
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_133.tif', 1024, 1000, 3, options=['STREAMABLE_OUTPUT=YES', 'TILED=YES'])
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = ds.WriteRaster(256, 256, 256, 256, 'a' * (3 * 256 * 256))
    ds.FlushCache()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_133.tif')

###############################################################################
# Test DISCARD_LSB


def test_tiff_write_134():

    for interleave in ['BAND', 'PIXEL']:
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 3, options=['DISCARD_LSB=0,1,3', 'INTERLEAVE='+interleave])
        ds.GetRasterBand(1).Fill(127)
        ds.GetRasterBand(2).Fill(127)
        ds.GetRasterBand(3).Fill(127)
        ds = None
        ds = gdal.Open('/vsimem/tiff_write_134.tif')
        val1 = struct.unpack('B', ds.GetRasterBand(1).ReadRaster())[0]
        val2 = struct.unpack('B', ds.GetRasterBand(2).ReadRaster())[0]
        val3 = struct.unpack('B', ds.GetRasterBand(3).ReadRaster())[0]
        assert val1 == 127 and val2 == 126 and val3 == 128
        ds = None
        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134_src.tif', 1, 1, 3)
    src_ds.GetRasterBand(1).Fill(127)
    src_ds.GetRasterBand(2).Fill(127)
    src_ds.GetRasterBand(3).Fill(255)
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_134.tif', src_ds, options=['DISCARD_LSB=0,1,3'])
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_134.tif')
    val1 = struct.unpack('B', ds.GetRasterBand(1).ReadRaster())[0]
    val2 = struct.unpack('B', ds.GetRasterBand(2).ReadRaster())[0]
    val3 = struct.unpack('B', ds.GetRasterBand(3).ReadRaster())[0]
    assert val1 == 127 and val2 == 126 and val3 == 255
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134_src.tif')
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    for (inval, expected_val) in [(0, 0), (1, 0), (2, 0), (3, 0), (4, 8), (254, 255), (255, 255)]:
        for interleave in ['BAND', 'PIXEL']:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, gdal.GDT_Byte, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).Fill(inval)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('B', ds.GetRasterBand(1).ReadRaster())[0]
            assert val1 == expected_val, (inval, expected_val)
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    for (inval, expected_val) in [(-32768, -32768),
                                  (-32767,-32768),
                                  (-32764,-32768),
                                  (-8, -8),
                                  (-1, -8), # this truncation is questionable
                                  (0, 0),
                                  (1,0),
                                  (3, 0),
                                  (4,8),
                                  (8,8),
                                  (32766,32760),
                                  (32767, 32760)]:
        for interleave in ['BAND', 'PIXEL']:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, gdal.GDT_Int16, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).Fill(inval)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('h', ds.GetRasterBand(1).ReadRaster())[0]
            assert val1 == expected_val, (inval, expected_val)
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    for (inval, expected_val) in [(0,0), (1,0), (3, 0), (4,8), (8,8), (65534,65528), (65535,65528)]:
        for interleave in ['BAND', 'PIXEL']:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, gdal.GDT_UInt16, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).Fill(inval)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('H', ds.GetRasterBand(1).ReadRaster())[0]
            assert val1 == expected_val, (inval, expected_val)
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    for interleave in ['BAND', 'PIXEL']:
        for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64]:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 3, dt, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            if dt == gdal.GDT_Int16:
                ds.GetRasterBand(1).Fill(-127)
            else:
                ds.GetRasterBand(1).Fill(127)
            ds.GetRasterBand(2).Fill(123)
            ds.GetRasterBand(3).Fill(127)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('h', ds.GetRasterBand(1).ReadRaster(0,0,1,1,1,1,gdal.GDT_Int16))[0]
            val2 = struct.unpack('h', ds.GetRasterBand(2).ReadRaster(0,0,1,1,1,1,gdal.GDT_Int16))[0]
            val3 = struct.unpack('h', ds.GetRasterBand(3).ReadRaster(0,0,1,1,1,1,gdal.GDT_Int16))[0]
            if dt in (gdal.GDT_Float32, gdal.GDT_Float64):
                assert val1 == 127 and val2 == 123 and val3 == 127, (interleave, dt, (val1, val2, val3))
            elif dt == gdal.GDT_Int16:
                assert val1 == -128 and val2 == 120 and val3 == 128, (interleave, dt, (val1, val2, val3))
            else:
                assert val1 == 128 and val2 == 120 and val3 == 128, (interleave, dt, (val1, val2, val3))
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    # Test with nodata
    for interleave in ['BAND', 'PIXEL']:
        for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64]:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, dt, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).SetNoDataValue(127)
            ds.GetRasterBand(1).Fill(127)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('B', ds.GetRasterBand(1).ReadRaster(0,0,1,1,1,1,gdal.GDT_Byte))[0]
            assert val1 == 127, (interleave, dt, val1)
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    # Test with nodata and discarding non-nodata value would result to nodata without correction
    for interleave in ['BAND', 'PIXEL']:
        for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64]:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, dt, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).SetNoDataValue(0)
            ds.GetRasterBand(1).Fill(1)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('B', ds.GetRasterBand(1).ReadRaster(0,0,1,1,1,1,gdal.GDT_Byte))[0]
            if dt in (gdal.GDT_Float32, gdal.GDT_Float64):
                assert val1 == 1, (interleave, dt, val1)
            else:
                assert val1 == 8, (interleave, dt, val1)
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    # Test with nodata out of range for integer values
    for interleave in ['BAND', 'PIXEL']:
        for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64]:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, dt, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).SetNoDataValue(127.5)
            ds.GetRasterBand(1).Fill(127)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            assert ds.GetRasterBand(1).GetNoDataValue() == 127.5
            val1 = struct.unpack('B', ds.GetRasterBand(1).ReadRaster(0,0,1,1,1,1,gdal.GDT_Byte))[0]
            if dt in (gdal.GDT_Float32, gdal.GDT_Float64):
                assert val1 == 127, (interleave, dt, val1)
            else:
                assert val1 == 128, (interleave, dt, val1)
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    # Test with some non-integer float value
    for interleave in ['BAND', 'PIXEL']:
        for dt in [gdal.GDT_Float32, gdal.GDT_Float64]:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, dt, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).Fill(-0.3)
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('d', ds.GetRasterBand(1).ReadRaster(0,0,1,1,1,1,gdal.GDT_Float64))[0]
            assert val1 != -0.3 and abs(val1 - -0.3) < 1e-5, dt
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    # Test with nan
    for interleave in ['BAND', 'PIXEL']:
        for dt in [gdal.GDT_Float32, gdal.GDT_Float64]:
            ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1, 2, dt, options=['DISCARD_LSB=3', 'INTERLEAVE='+interleave])
            ds.GetRasterBand(1).Fill(float('nan'))
            ds = None
            ds = gdal.Open('/vsimem/tiff_write_134.tif')
            val1 = struct.unpack('f', ds.GetRasterBand(1).ReadRaster(0,0,1,1,1,1,gdal.GDT_Float32))[0]
            assert math.isnan(val1)
            ds = None
            gdaltest.tiff_drv.Delete('/vsimem/tiff_write_134.tif')

    # Error cases
    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1,
                                 options=['DISCARD_LSB=1', 'PHOTOMETRIC=PALETTE'])
    assert gdal.GetLastErrorMsg() != ''

    gdal.ErrorReset()
    with gdaltest.error_handler():
        # Too many elements
        gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1,
                                 options=['DISCARD_LSB=1,2'])
    assert gdal.GetLastErrorMsg() != ''

    gdal.ErrorReset()
    with gdaltest.error_handler():
        # Too many elements
        gdaltest.tiff_drv.Create('/vsimem/tiff_write_134.tif', 1, 1,
                                 options=['DISCARD_LSB=1', 'NBITS=7'])
    assert gdal.GetLastErrorMsg() != ''

###############################################################################
# Test clearing GCPs (#5945)


def test_tiff_write_135():

    # Simple clear
    src_ds = gdal.Open('data/gcps.vrt')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_135.tif', src_ds)
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_135.tif', gdal.GA_Update)
    ds.SetGCPs([], '')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_135.tif')
    assert not ds.GetGCPs()
    assert ds.GetGCPProjection() == ''
    ds = None

    # Double clear
    src_ds = gdal.Open('data/gcps.vrt')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_135.tif', src_ds)
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_135.tif', gdal.GA_Update)
    ds.SetGCPs([], '')
    ds.SetGCPs([], '')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_135.tif')
    assert not ds.GetGCPs()
    assert ds.GetGCPProjection() == ''
    ds = None

    # Clear + set geotransform and new projection
    src_ds = gdal.Open('data/gcps.vrt')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_135.tif', src_ds)
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_135.tif', gdal.GA_Update)
    ds.SetGCPs([], '')
    ds.SetGeoTransform([1, 2, 3, 4, 5, -6])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_135.tif')
    assert not ds.GetGCPs()
    assert ds.GetGeoTransform() == (1, 2, 3, 4, 5, -6)
    assert ds.GetProjectionRef().find('32601') >= 0
    ds = None

    gdal.Unlink('/vsimem/tiff_write_135.tif')

###############################################################################
# Test writing a single-strip mono-bit dataset


def test_tiff_write_136():

    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_136_src.tif', 8, 2001)
    src_ds.GetRasterBand(1).Fill(1)
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_136.tif', src_ds, options=['NBITS=1', 'COMPRESS=DEFLATE', 'BLOCKYSIZE=2001'])
    src_ds = None
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_136.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs

    gdal.Unlink('/vsimem/tiff_write_136_src.tif')
    gdal.Unlink('/vsimem/tiff_write_136.tif')
    gdal.Unlink('/vsimem/tiff_write_136.tif.aux.xml')

###############################################################################
# Test multi-threaded writing


def test_tiff_write_137():

    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_137_src.tif', 4000, 4000)
    src_ds.GetRasterBand(1).Fill(1)
    data = src_ds.GetRasterBand(1).ReadRaster()
    expected_cs = src_ds.GetRasterBand(1).Checksum()

    # Test NUM_THREADS as creation option
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_137.tif', src_ds,
                                      options=['BLOCKYSIZE=16', 'COMPRESS=DEFLATE', 'NUM_THREADS=ALL_CPUS'])
    src_ds = None
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_137.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == expected_cs

    # Test NUM_THREADS as creation option with Create()
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_137.tif', 4000, 4000, 1,
                                  options=['BLOCKYSIZE=16', 'COMPRESS=DEFLATE', 'NUM_THREADS=ALL_CPUS'])
    ds.GetRasterBand(1).WriteRaster(0, 0, 4000, 4000, data)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_137.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == expected_cs

    # Test NUM_THREADS as open option
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_137.tif', 4000, 4000,
                                  options=['TILED=YES', 'COMPRESS=DEFLATE', 'PREDICTOR=2', 'SPARSE_OK=YES'])
    ds = None
    ds = gdal.OpenEx('/vsimem/tiff_write_137.tif', gdal.OF_UPDATE, open_options=['NUM_THREADS=4'])
    ds.GetRasterBand(1).Fill(1)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_137.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == expected_cs

    # Ask data immediately while the block is compressed
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_137.tif', 4000, 4000,
                                  options=['BLOCKYSIZE=3999', 'COMPRESS=DEFLATE', 'NUM_THREADS=4'])
    ds.WriteRaster(0, 0, 1, 1, 'A')
    ds.FlushCache()
    val = ds.ReadRaster(0, 0, 1, 1).decode('ascii')
    assert val == 'A'
    ds = None

    gdal.Unlink('/vsimem/tiff_write_137_src.tif')
    gdal.Unlink('/vsimem/tiff_write_137.tif')

    # Test NUM_THREADS with raster == tile
    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_137.tif', src_ds,
                                      options=['BLOCKYSIZE=20', 'COMPRESS=DEFLATE', 'NUM_THREADS=ALL_CPUS'])
    src_ds = None
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_137.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == 4672, expected_cs
    gdal.Unlink('/vsimem/tiff_write_137.tif')

###############################################################################
# Test that pixel-interleaved writing generates optimal size


def test_tiff_write_138():

    # Test that consecutive IWriteBlock() calls for the same block but in
    # different bands only generate a single tile write, and not 3 rewrites
    ds = gdal.GetDriverByName('GTiff').Create(
        '/vsimem/tiff_write_138.tif', 10, 1, 3, options=['COMPRESS=DEFLATE'])
    ds.GetRasterBand(1).WriteRaster(0, 0, 10, 1, 'A', buf_xsize=1, buf_ysize=1)
    ds.GetRasterBand(1).FlushCache()
    ds.GetRasterBand(2).WriteRaster(0, 0, 10, 1, 'A', buf_xsize=1, buf_ysize=1)
    ds.GetRasterBand(2).FlushCache()
    ds.GetRasterBand(3).WriteRaster(0, 0, 10, 1, 'A', buf_xsize=1, buf_ysize=1)
    ds.GetRasterBand(3).FlushCache()
    ds = None
    size = gdal.VSIStatL('/vsimem/tiff_write_138.tif').size
    assert size == 181

    # Test fix for #5999

    # Create a file with a huge block that will saturate the block cache.
    with gdaltest.SetCacheMax(1000000):
        tmp_ds = gdal.GetDriverByName('GTiff').Create(
            '/vsimem/tiff_write_138_saturate.tif', gdal.GetCacheMax(), 1)
        tmp_ds = None

        ds = gdal.GetDriverByName('GTiff').Create(
            '/vsimem/tiff_write_138.tif', 10, 1, 3, options=['COMPRESS=DEFLATE'])
        ds.GetRasterBand(1).WriteRaster(0, 0, 10, 1, 'A', buf_xsize=1, buf_ysize=1)
        ds.GetRasterBand(2).WriteRaster(0, 0, 10, 1, 'A', buf_xsize=1, buf_ysize=1)
        ds.GetRasterBand(3).WriteRaster(0, 0, 10, 1, 'A', buf_xsize=1, buf_ysize=1)
        # When internalizing the huge block, check that the 3 above dirty blocks
        # get written as a single tile write.
        tmp_ds = gdal.Open('/vsimem/tiff_write_138_saturate.tif')
        tmp_ds.GetRasterBand(1).Checksum()
        tmp_ds = None
        ds = None
        size = gdal.VSIStatL('/vsimem/tiff_write_138.tif').size
        assert size == 181

    gdal.Unlink('/vsimem/tiff_write_138.tif')
    gdal.Unlink('/vsimem/tiff_write_138_saturate.tif')

###############################################################################
# Test that pixel-interleaved writing generates optimal size


def test_tiff_write_139():

    drv = gdal.GetDriverByName('GTiff')
    # Only post 4.0.5 has the fix for non-byte swabing case
    has_inverted_swab_fix = drv.GetMetadataItem('LIBTIFF') == 'INTERNAL'

    # In the byte case, there are optimizations for the 3 and 4 case. 1 is the general case
    for nbands in (1, 3, 4):

        ds = drv.Create('/vsimem/tiff_write_139.tif', 4, 1, nbands,
                        options=['PREDICTOR=2', 'COMPRESS=DEFLATE'])
        ref_content = struct.pack('B' * 4, 255, 0, 255, 0)
        for i in range(nbands):
            ds.GetRasterBand(i + 1).WriteRaster(0, 0, 4, 1, ref_content)
        ds = None
        ds = gdal.Open('/vsimem/tiff_write_139.tif')
        for i in range(nbands):
            content = ds.GetRasterBand(i + 1).ReadRaster()
            assert ref_content == content
        ds = None

        gdal.Unlink('/vsimem/tiff_write_139.tif')

    # Int16
    for endianness in ['NATIVE', 'INVERTED']:

        if endianness == 'INVERTED' and not has_inverted_swab_fix:
            continue

        ds = drv.Create('/vsimem/tiff_write_139.tif', 6, 1, 1, gdal.GDT_Int16,
                        options=['PREDICTOR=2', 'COMPRESS=DEFLATE', 'ENDIANNESS=%s' % endianness])
        ref_content = struct.pack('h' * 6, -32768, 32767, -32768, 32767, -32768, 32767)
        ds.GetRasterBand(1).WriteRaster(0, 0, 6, 1, ref_content)
        ds = None
        ds = gdal.Open('/vsimem/tiff_write_139.tif')
        content = ds.GetRasterBand(1).ReadRaster()
        if ref_content != content:
            print(endianness)
            pytest.fail(struct.unpack('h' * 6, content))
        ds = None

        gdal.Unlink('/vsimem/tiff_write_139.tif')

    # UInt16 (same code path)
    for endianness in ['NATIVE', 'INVERTED']:

        if endianness == 'INVERTED' and not has_inverted_swab_fix:
            continue

        ds = drv.Create('/vsimem/tiff_write_139.tif', 6, 1, 1, gdal.GDT_UInt16,
                        options=['PREDICTOR=2', 'COMPRESS=DEFLATE', 'ENDIANNESS=%s' % endianness])
        ref_content = struct.pack('H' * 6, 0, 65535, 0, 65535, 0, 65535)
        ds.GetRasterBand(1).WriteRaster(0, 0, 6, 1, ref_content)
        ds = None
        ds = gdal.Open('/vsimem/tiff_write_139.tif')
        content = ds.GetRasterBand(1).ReadRaster()
        if ref_content != content:
            print(endianness)
            pytest.fail(struct.unpack('H' * 6, content))
        ds = None

        gdal.Unlink('/vsimem/tiff_write_139.tif')

    # Int32
    for endianness in ['NATIVE', 'INVERTED']:

        if endianness == 'INVERTED' and not has_inverted_swab_fix:
            continue

        ds = drv.Create('/vsimem/tiff_write_139.tif', 6, 1, 1, gdal.GDT_UInt32,
                        options=['PREDICTOR=2', 'COMPRESS=DEFLATE', 'ENDIANNESS=%s' % endianness])
        ref_content = struct.pack('I' * 6, 0, 2000000000, 0, 2000000000, 0, 2000000000)
        ds.GetRasterBand(1).WriteRaster(0, 0, 6, 1, ref_content)
        ds = None
        ds = gdal.Open('/vsimem/tiff_write_139.tif')
        content = ds.GetRasterBand(1).ReadRaster()
        if ref_content != content:
            print(endianness)
            pytest.fail(struct.unpack('I' * 6, content))
        ds = None

        gdal.Unlink('/vsimem/tiff_write_139.tif')

    # Test floating-point predictor
    # Seems to be broken with ENDIANNESS=INVERTED
    ds = drv.Create('/vsimem/tiff_write_139.tif', 4, 1, 1, gdal.GDT_Float64,
                    options=['PREDICTOR=3', 'COMPRESS=DEFLATE'])
    ref_content = struct.pack('d' * 4, 1, -1e100, 1e10, -1e5)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, ref_content)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_139.tif')
    content = ds.GetRasterBand(1).ReadRaster()
    assert ref_content == content, struct.unpack('d' * 4, content)
    ds = None

    gdal.Unlink('/vsimem/tiff_write_139.tif')

###############################################################################
# Test setting a band to alpha


def test_tiff_write_140():

    # Nominal case: set alpha to last band
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_140.tif', 1, 1, 5)
    ds.GetRasterBand(5).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_140.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'
    ds = gdal.Open('/vsimem/tiff_write_140.tif')
    assert ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    # Strange case: set alpha to a band, but it is already set on another one
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_140.tif', 1, 1, 5)
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    # Should emit a warning
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ret = ds.GetRasterBand(5).SetColorInterpretation(gdal.GCI_AlphaBand)
    assert gdal.GetLastErrorMsg() != ''
    assert ret == 0
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_140.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'
    ds = gdal.Open('/vsimem/tiff_write_140.tif')
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    # Strange case: set alpha to a band, but it is already set on another one (because of ALPHA=YES)
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_140.tif', 1, 1, 5, options=['ALPHA=YES'])
    # Should emit a warning mentioning ALPHA creation option.
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ret = ds.GetRasterBand(5).SetColorInterpretation(gdal.GCI_AlphaBand)
    assert gdal.GetLastErrorMsg().find('ALPHA') >= 0
    assert ret == 0
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_140.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'
    ds = gdal.Open('/vsimem/tiff_write_140.tif')
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_140.tif')

###############################################################################
# Test GEOTIFF_KEYS_FLAVOR=ESRI_PE with EPSG:3857


def test_tiff_write_141():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_141.tif', 1, 1, options=['GEOTIFF_KEYS_FLAVOR=ESRI_PE'])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_141.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    assert wkt.startswith('PROJCS["WGS 84 / Pseudo-Mercator"')

    assert 'EXTENSION["PROJ4"' in wkt

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_141.tif')


###############################################################################
# Test PixelIsPoint without SRS (#6225)

def test_tiff_write_142():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_142.tif', 1, 1)
    ds.SetMetadataItem('AREA_OR_POINT', 'Point')
    ds.SetGeoTransform([10, 1, 0, 100, 0, -1])
    ds = None

    src_ds = gdal.Open('/vsimem/tiff_write_142.tif')
    gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_142_2.tif', src_ds)
    src_ds = None

    ds = gdal.Open('/vsimem/tiff_write_142_2.tif')
    gt = ds.GetGeoTransform()
    md = ds.GetMetadataItem('AREA_OR_POINT')
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_142.tif')
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_142_2.tif')

    gt_expected = (10, 1, 0, 100, 0, -1)
    assert gt == gt_expected, 'did not get expected geotransform'

    assert md == 'Point', 'did not get expected AREA_OR_POINT value'

###############################################################################
# Check that we detect that free space isn't sufficient


def test_tiff_write_143():

    with gdaltest.error_handler():
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_143.tif', 1000000000, 1000000000)
    assert ds is None

###############################################################################
# Test creating a real BigTIFF file > 4 GB with multiple directories (on filesystems supporting sparse files)


def test_tiff_write_144():

    # Determine if the filesystem supports sparse files (we don't want to create a real 10 GB
    # file !
    if not gdaltest.filesystem_supports_sparse_files('tmp'):
        pytest.skip()

    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_144.tif', 20, 20, 1, options=['BIGTIFF=YES'])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    # Extend the file to 4 GB
    f = open('tmp/tiff_write_144.tif', 'rb+')
    f.seek(4294967296, 0)
    f.write(' '.encode('ascii'))
    f.close()

    ds = gdal.Open('tmp/tiff_write_144.tif', gdal.GA_Update)
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open('tmp/tiff_write_144.tif')
    got_cs = ds.GetRasterBand(1).Checksum()
    got_cs_ovr = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    gdal.Unlink('tmp/tiff_write_144.tif')

    assert got_cs == 4873 and got_cs_ovr == 1218

###############################################################################
# Test various warnings / errors of Create()


def test_tiff_write_145():

    options_list = [{'bands': 65536, 'expected_failure': True},
                    {'creation_options': ['INTERLEAVE=foo'], 'expected_failure': True},
                    {'creation_options': ['COMPRESS=foo'], 'expected_failure': False},
                    {'creation_options': ['STREAMABLE_OUTPUT=YES', 'SPARSE_OK=YES'], 'expected_failure': True},
                    {'creation_options': ['STREAMABLE_OUTPUT=YES', 'COPY_SRC_OVERVIEWS=YES'], 'expected_failure': True},
                    {'use_tmp': True, 'xsize': 100000, 'ysize': 100000, 'creation_options': ['BIGTIFF=NO'], 'expected_failure': True},
                    {'creation_options': ['ENDIANNESS=foo'], 'expected_failure': False},
                    {'creation_options': ['NBITS=9'], 'expected_failure': False},
                    {'datatype': gdal.GDT_Float32, 'creation_options': ['NBITS=8'], 'expected_failure': False},
                    {'datatype': gdal.GDT_UInt16, 'creation_options': ['NBITS=8'], 'expected_failure': False},
                    {'datatype': gdal.GDT_UInt16, 'creation_options': ['NBITS=17'], 'expected_failure': False},
                    {'datatype': gdal.GDT_UInt32, 'creation_options': ['NBITS=16'], 'expected_failure': False},
                    {'datatype': gdal.GDT_UInt32, 'creation_options': ['NBITS=33'], 'expected_failure': False},
                    {'bands': 3, 'creation_options': ['PHOTOMETRIC=YCBCR'], 'expected_failure': True},
                    {'bands': 3, 'creation_options': ['PHOTOMETRIC=YCBCR', 'COMPRESS=JPEG', 'INTERLEAVE=BAND'], 'expected_failure': True},
                    {'bands': 1, 'creation_options': ['PHOTOMETRIC=YCBCR', 'COMPRESS=JPEG'], 'expected_failure': True},
                    {'creation_options': ['PHOTOMETRIC=foo'], 'expected_failure': False},
                    {'creation_options': ['PHOTOMETRIC=RGB'], 'expected_failure': False},
                    {'creation_options': ['TILED=YES', 'BLOCKSIZE=1', 'BLOCKYSIZE=1'], 'expected_failure': True},
                   ]

    for options in options_list:
        xsize = options.get('xsize', 1)
        ysize = options.get('ysize', 1)
        bands = options.get('bands', 1)
        datatype = options.get('datatype', gdal.GDT_Byte)
        use_tmp = options.get('use_tmp', False)
        if use_tmp:
            filename = 'tmp/tiff_write_145.tif'
        else:
            filename = '/vsimem/tiff_write_145.tif'
        creation_options = options.get('creation_options', [])
        gdal.Unlink(filename)
        gdal.ErrorReset()
        with gdaltest.error_handler():
            ds = gdaltest.tiff_drv.Create(filename, xsize, ysize, bands, datatype, options=creation_options)
        if ds is not None and options.get('expected_failure', False):
            print(options)
            pytest.fail('expected failure, but did not get it')
        elif ds is None and not options.get('expected_failure', False):
            print(options)
            pytest.fail('got failure, but did not expect it')
        ds = None
        # print(gdal.GetLastErrorMsg())
        if gdal.GetLastErrorMsg() == '':
            print(options)
            pytest.fail('did not get any warning/error')
        gdal.Unlink(filename)


###############################################################################
# Test implicit JPEG-in-TIFF overviews with RGBA (not completely sure this
# is a legal formulation since 4 bands should probably be seen as CMYK)


def test_tiff_write_146():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    tmp_ds = gdal.Translate('', 'data/stefan_full_rgba.tif', format='MEM')
    original_stats = [tmp_ds.GetRasterBand(i + 1).ComputeStatistics(True) for i in range(4)]
    gdal.Translate('/vsimem/tiff_write_146.tif', 'data/stefan_full_rgba.tif', options='-outsize 1000% 1000% -co COMPRESS=JPEG')
    out_ds = gdal.Open('/vsimem/tiff_write_146.tif')
    got_stats = [out_ds.GetRasterBand(i + 1).GetOverview(2).ComputeStatistics(True) for i in range(4)]
    out_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_write_146.tif')

    for i in range(4):
        for j in range(4):
            assert i == 2 or j < 2 or original_stats[i][j] == pytest.approx(got_stats[i][j], abs=5), \
                'did not get expected statistics'


###############################################################################
# Test that we don't use implicit JPEG-in-TIFF overviews with CMYK when converting
# to RGBA


def test_tiff_write_147():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'NO')
    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'NO')
    gdal.Translate('/vsimem/tiff_write_147.tif', '../gdrivers/data/jpeg/rgb_ntf_cmyk.jpg', options='-outsize 1000% 1000% -co COMPRESS=JPEG -co PHOTOMETRIC=CMYK')
    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', None)
    gdal.SetConfigOption('GDAL_PAM_ENABLED', None)
    out_ds = gdal.Open('/vsimem/tiff_write_147.tif')
    assert out_ds.GetRasterBand(1).GetOverview(0) is None, 'did not expected overview'
    out_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_write_147.tif')

###############################################################################
# Test that we can use implicit JPEG-in-TIFF overviews with CMYK in raw mode


def test_tiff_write_148():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'NO')
    tmp_ds = gdal.Translate('', '../gdrivers/data/jpeg/rgb_ntf_cmyk.jpg', format='MEM')
    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', None)
    original_stats = [tmp_ds.GetRasterBand(i + 1).ComputeStatistics(True) for i in range(4)]
    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'NO')
    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'NO')
    gdal.Translate('/vsimem/tiff_write_148.tif', '../gdrivers/data/jpeg/rgb_ntf_cmyk.jpg', options='-outsize 1000% 1000% -co COMPRESS=JPEG -co PHOTOMETRIC=CMYK')
    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', None)
    gdal.SetConfigOption('GDAL_PAM_ENABLED', None)
    out_ds = gdal.Open('GTIFF_RAW:/vsimem/tiff_write_148.tif')
    got_stats = [out_ds.GetRasterBand(i + 1).GetOverview(0).ComputeStatistics(True) for i in range(4)]
    out_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_write_148.tif')

    for i in range(4):
        for j in range(4):
            assert j < 2 or original_stats[i][j] == pytest.approx(got_stats[i][j], abs=5), \
                'did not get expected statistics'


###############################################################################
# Test filling missing blocks with nodata


def test_tiff_write_149():

    # Power-of-two bit depth
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_149.tif', 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(127)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_149.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == 1
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_149.tif')

    # Test implicit blocks
    expected_cs = 13626
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_149.tif', 40, 30, 2, gdal.GDT_UInt16, options=['NBITS=12', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16', 'INTERLEAVE=BAND', 'SPARSE_OK=YES'])
    ds.GetRasterBand(1).SetNoDataValue(127)
    ds.GetRasterBand(2).SetNoDataValue(127)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_149.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == expected_cs
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_149.tif')

    # NBITS=12, SEPARATE. Checksum must be the same as in the implicit blocks case
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_149.tif', 40, 30, 2, gdal.GDT_UInt16, options=['NBITS=12', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16', 'INTERLEAVE=BAND'])
    ds.GetRasterBand(1).SetNoDataValue(127)
    ds.GetRasterBand(2).SetNoDataValue(127)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_149.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == expected_cs
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_149.tif')

    # NBITS=12, CONTIG. Checksum must be the same as in the implicit blocks case
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_149.tif', 40, 30, 2, gdal.GDT_UInt16, options=['NBITS=12', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16', 'INTERLEAVE=PIXEL'])
    ds.GetRasterBand(1).SetNoDataValue(127)
    ds.GetRasterBand(2).SetNoDataValue(127)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_149.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    assert cs == expected_cs
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_149.tif')

###############################################################################
# Test failure when loading block from disk in IWriteBlock()


def test_tiff_write_150():

    shutil.copy('data/tiled_bad_offset.tif', 'tmp/tiled_bad_offset.tif')
    ds = gdal.Open('tmp/tiled_bad_offset.tif', gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds.FlushCache()
    assert gdal.GetLastErrorMsg() != ''
    ds = None
    gdaltest.tiff_drv.Delete('tmp/tiled_bad_offset.tif')

###############################################################################
# Test IWriteBlock() with more than 10 bands


def test_tiff_write_151():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_151.tif', 1, 1, 11)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_151.tif', gdal.GA_Update)
    ds.GetRasterBand(1).Fill(1)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_151.tif', gdal.GA_Update)
    ds.GetRasterBand(1).Checksum()
    ds.GetRasterBand(2).Fill(1)
    ds.GetRasterBand(3).Fill(1)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_151.tif')
    assert ds.GetRasterBand(1).Checksum() == 1
    assert ds.GetRasterBand(2).Checksum() == 1
    assert ds.GetRasterBand(3).Checksum() == 1
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_151.tif')

###############################################################################
# Test flushing of blocks in a contig multi band file with Create()


def test_tiff_write_152():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_152.tif', 1, 1, 2, options=['NBITS=2'])
    ds.GetRasterBand(2).SetNoDataValue(3)
    ds.GetRasterBand(2).Fill(1)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_152.tif')
    assert ds.GetRasterBand(1).Checksum() == 0
    assert ds.GetRasterBand(2).Checksum() == 1
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_152.tif')

###############################################################################
# Test that empty blocks are created in a filesystem sparse way


def test_tiff_write_153():

    target_dir = 'tmp'

    if gdal.VSISupportsSparseFiles(target_dir) == 0:
        pytest.skip()

    gdaltest.tiff_drv.Create(target_dir + '/tiff_write_153.tif', 500, 500)

    f = gdal.VSIFOpenL(target_dir + '/tiff_write_153.tif', 'rb')
    ret = gdal.VSIFGetRangeStatusL(f, 500 * 500, 1)
    gdal.VSIFCloseL(f)

    gdaltest.tiff_drv.Delete(target_dir + '/tiff_write_153.tif')

    assert ret != gdal.VSI_RANGE_STATUS_DATA

###############################################################################
# Test empty block writing skipping and SPARSE_OK in CreateCopy() and Open()


def test_tiff_write_154():

    src_ds = gdal.GetDriverByName('MEM').Create('', 500, 500)

    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=['BLOCKYSIZE=256'])
    ds.FlushCache()
    # At that point empty blocks have not yet been flushed
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 162
    ds = None
    # Now they are and that's done in a filesystem sparse way. TODO: check this
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 256162
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_154.tif')

    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=['BLOCKYSIZE=256', 'COMPRESS=DEFLATE'])
    ds.FlushCache()
    # With compression, empty blocks are written right away
    # 461 is with libdeflate, 462 with zlib
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size in (461, 462)
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size in (461, 462)
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_154.tif')

    # SPARSE_OK in CreateCopy(): blocks are not written
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=['SPARSE_OK=YES', 'BLOCKYSIZE=256'])
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 162
    # SPARSE_OK in Open()/update: blocks are not written
    ds = gdal.OpenEx('/vsimem/tiff_write_154.tif', gdal.OF_UPDATE, open_options=['SPARSE_OK=YES'])
    ds.GetRasterBand(1).Fill(0)
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 162
    ds = None
    # Default behaviour in Open()/update: blocks are written
    ds = gdal.OpenEx('/vsimem/tiff_write_154.tif', gdal.OF_UPDATE)
    ds.GetRasterBand(1).Fill(0)
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 250162
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_154.tif')

    # SPARSE_OK in CreateCopy() in compressed case (strips): blocks are not written
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=['SPARSE_OK=YES', 'BLOCKYSIZE=256', 'COMPRESS=DEFLATE'])
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 174
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_154.tif')

    # SPARSE_OK in CreateCopy() in compressed case (tiling): blocks are not written
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=['SPARSE_OK=YES', 'TILED=YES'])
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 190
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_154.tif')

    # Test detection of 0 blocks for all data types
    for dt in ['signedbyte', gdal.GDT_Int16, gdal.GDT_UInt16,
               gdal.GDT_Int32, gdal.GDT_UInt32,
               gdal.GDT_Float32, gdal.GDT_Float64]:
        # SPARSE_OK in CreateCopy(): blocks are not written
        if dt == 'signedbyte':
            src_ds = gdal.GetDriverByName('MEM').Create('', 500, 500, 1, gdal.GDT_Byte)
            options = ['SPARSE_OK=YES', 'BLOCKYSIZE=256', 'PIXELTYPE=SIGNEDBYTE']
        else:
            src_ds = gdal.GetDriverByName('MEM').Create('', 500, 500, 1, dt)
            options = ['SPARSE_OK=YES', 'BLOCKYSIZE=256']
        gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=options)
        assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 162, dt

    # Test detection of nodata blocks with nodata != 0 for all data types
    for dt in ['signedbyte', gdal.GDT_Int16, gdal.GDT_UInt16,
               gdal.GDT_Int32, gdal.GDT_UInt32,
               gdal.GDT_Float32, gdal.GDT_Float64]:
        # SPARSE_OK in CreateCopy(): blocks are not written
        if dt == 'signedbyte':
            src_ds = gdal.GetDriverByName('MEM').Create('', 500, 500, 1, gdal.GDT_Byte)
            options = ['SPARSE_OK=YES', 'BLOCKYSIZE=256', 'PIXELTYPE=SIGNEDBYTE']
        else:
            src_ds = gdal.GetDriverByName('MEM').Create('', 500, 500, 1, dt)
            options = ['SPARSE_OK=YES', 'BLOCKYSIZE=256']
        src_ds.GetRasterBand(1).Fill(1)
        src_ds.GetRasterBand(1).SetNoDataValue(1)
        ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=options)
        ds = None
        assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 174, dt

    # Test optimized detection when nodata==0, and with the last pixel != 0
    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 1, 1)
    src_ds.GetRasterBand(1).Fill(0)
    src_ds.GetRasterBand(1).WriteRaster(99, 0, 1, 1, struct.pack('B' * 1, 1))
    gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_154.tif', src_ds, options=['SPARSE_OK=YES'])
    assert gdal.VSIStatL('/vsimem/tiff_write_154.tif').size == 246

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_154.tif')

    # Test that setting nodata doesn't prevent blocks to be written (#6706)
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_154.tif', 1, 100, 1)
    ds.GetRasterBand(1).SetNoDataValue(1)
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_154.tif')
    offset = ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF')
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_154.tif')
    assert not (offset is None or int(offset) == 0)

###############################################################################
# Test reading and writing band description


def test_tiff_write_155():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_155.tif', 1, 1)
    ds.GetRasterBand(1).SetDescription('foo')
    ds = None

    assert gdal.VSIStatL('/vsimem/tiff_write_155.tif.aux.xml') is None

    ds = gdal.Open('/vsimem/tiff_write_155.tif')
    assert ds.GetRasterBand(1).GetDescription() == 'foo'
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_155.tif')

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_155.tif', 1, 1, options=['PROFILE=GeoTIFF'])
    ds.GetRasterBand(1).SetDescription('foo')
    ds = None

    assert gdal.VSIStatL('/vsimem/tiff_write_155.tif.aux.xml') is not None

    ds = gdal.Open('/vsimem/tiff_write_155.tif')
    assert ds.GetRasterBand(1).GetDescription() == 'foo'
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_155.tif')

###############################################################################
# Test GetDataCoverageStatus()


def test_tiff_write_156():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_156.tif', 64, 64, options=['SPARSE_OK=YES', 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32'])
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, 'X')

    (flags, pct) = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 32, 32)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    (flags, pct) = ds.GetRasterBand(1).GetDataCoverageStatus(32, 0, 32, 32)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    (flags, pct) = ds.GetRasterBand(1).GetDataCoverageStatus(16, 16, 32, 32)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 25.0

    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_156.tif')

    # Test fix for #6703
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_156.tif', 1, 512, options=['SPARSE_OK=YES', 'BLOCKYSIZE=1'])
    ds.GetRasterBand(1).WriteRaster(0, 100, 1, 1, 'X')
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_156.tif')
    flags, _ = ds.GetRasterBand(1).GetDataCoverageStatus(0, 100, 1, 1)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_156.tif')

###############################################################################
# Test Float16


def test_tiff_write_157():

    # Write controlled values of Float16
    vals = struct.pack('H' * 14,
                       0x0000,  # Positive zero
                       0x8000,  # Negative zero
                       0x7C00,  # Positive infinity
                       0xFC00,  # Negative infinity
                       0x7E00,  # Some positive quiet NaN
                       0xFE00,  # Some negative quiet NaN
                       0x3D00,  # 1.25
                       0xBD00,  # -1.25
                       0x0001,  # Smallest positive denormalized value
                       0x8001,  # Smallest negative denormalized value
                       0x03FF,  # Largest positive denormalized value
                       0x83FF,  # Largest negative denormalized value
                       0x0400,  # Smallest positive normalized value
                       0x8400,  # Smallest negative normalized value
                      )

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_157.tif', 14, 1, 1, gdal.GDT_Float32, options=['NBITS=16'])
    ds = None
    ds = gdal.Open('/vsimem/tiff_write_157.tif')
    offset = int(ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
    ds = None

    f = gdal.VSIFOpenL('/vsimem/tiff_write_157.tif', 'rb+')
    gdal.VSIFSeekL(f, offset, 0)
    gdal.VSIFWriteL(vals, 1, len(vals), f)
    gdal.VSIFCloseL(f)

    # Check that we properly deserialize Float16 values
    ds = gdal.Open('/vsimem/tiff_write_157.tif')
    assert ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '16'
    got = struct.unpack('f' * 14, ds.ReadRaster())
    expected = [0.0, -0.0, gdaltest.posinf(), -gdaltest.posinf(), gdaltest.NaN(), gdaltest.NaN(), 1.25, -1.25, 5.9604644775390625e-08, -5.9604644775390625e-08, 6.0975551605224609e-05, -6.0975551605224609e-05, 6.103515625e-05, -6.103515625e-05]
    for i in range(14):
        if i == 4 or i == 5:
            assert got[i] != got[i]
        elif got[i] != pytest.approx(expected[i], abs=1e-15):
            print(got[i])
            print(expected[i])
            pytest.fail(i)

    # Check that we properly decode&re-encode Float16 values
    gdal.Translate('/vsimem/tiff_write_157_dst.tif', ds)
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_157_dst.tif')
    offset = int(ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
    ds = None

    f = gdal.VSIFOpenL('/vsimem/tiff_write_157_dst.tif', 'rb')
    gdal.VSIFSeekL(f, offset, 0)
    vals_copied = gdal.VSIFReadL(1, 14 * 2, f)
    gdal.VSIFCloseL(f)

    if vals != vals_copied:
        print(struct.unpack('H' * 14, vals))
        pytest.fail(struct.unpack('H' * 14, vals_copied))

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_157.tif')
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_157_dst.tif')

    # Now try Float32 -> Float16 conversion
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_157.tif', 18, 1, 1, gdal.GDT_Float32, options=['NBITS=16'])
    vals = struct.pack('I' * 18,
                       0x00000000,  # Positive zero
                       0x80000000,  # Negative zero
                       0x7f800000,  # Positive infinity
                       0xff800000,  # Negative infinity
                       0x7fc00000,  # Some positive quiet NaN
                       0xffc00000,  # Some negative quiet NaN
                       0x7f800001,  # Some positive signaling NaN with significant that will get lost
                       0xff800001,  # Some negative signaling NaN with significant that will get lost
                       0x3fa00000,  # 1.25
                       0xbfa00000,  # -1.25
                       0x00000001,  # Smallest positive denormalized value
                       0x80000001,  # Smallest negative denormalized value
                       0x007fffff,  # Largest positive denormalized value
                       0x807fffff,  # Largest negative denormalized value
                       0x00800000,  # Smallest positive normalized value
                       0x80800000,  # Smallest negative normalized value
                       0x33800000,  # 5.9604644775390625e-08 = Smallest number that can be converted as a float16 denormalized value
                       0x47800000,  # 65536 --> converted to infinity
                      )
    ds.GetRasterBand(1).WriteRaster(0, 0, 18, 1, vals, buf_type=gdal.GDT_Float32)
    with gdaltest.error_handler():
        ds.FlushCache()
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_157.tif')
    got = struct.unpack('f' * 18, ds.ReadRaster())
    ds = None
    expected = (0.0, -0.0, gdaltest.posinf(), -gdaltest.posinf(),
                gdaltest.NaN(), gdaltest.NaN(), gdaltest.NaN(), gdaltest.NaN(),
                1.25, -1.25, 0.0, -0.0, 0.0, -0.0, 0.0, -0.0, 5.9604644775390625e-08, gdaltest.posinf())

    for i in range(18):
        if i in (4, 5, 6, 7):
            # NaN comparison doesn't work like you'd expect
            assert got[i] != got[i]
        else:
            assert got[i] == pytest.approx(expected[i], abs=1e-15)

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_157.tif')

    # Test pixel interleaved
    gdal.Translate('/vsimem/tiff_write_157.tif', '../gdrivers/data/small_world.tif', options='-co NBITS=16 -ot Float32')
    ds = gdal.Open('/vsimem/tiff_write_157.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 30111
    cs = ds.GetRasterBand(2).Checksum()
    assert cs == 32302
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_157.tif')

###############################################################################
# Test GetActualBlockSize() (perhaps not the best place for that...)


def test_tiff_write_158():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_158.tif', 20, 40, 1, options=['TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=32'])
    (w, h) = ds.GetRasterBand(1).GetActualBlockSize(0, 0)
    assert (w, h) == (16, 32)
    (w, h) = ds.GetRasterBand(1).GetActualBlockSize(1, 1)
    assert (w, h) == (4, 8)
    res = ds.GetRasterBand(1).GetActualBlockSize(2, 0)
    assert res is None
    res = ds.GetRasterBand(1).GetActualBlockSize(0, 2)
    assert res is None
    res = ds.GetRasterBand(1).GetActualBlockSize(-1, 0)
    assert res is None
    res = ds.GetRasterBand(1).GetActualBlockSize(0, -1)
    assert res is None
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_158.tif')

###############################################################################
# Test that COPY_SRC_OVERVIEWS creation option with JPEG compression
# result in a https://trac.osgeo.org/gdal/wiki/CloudOptimizedGeoTIFF


def test_tiff_write_159():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    prev_table = ''
    for options in [[], ['JPEG_QUALITY=50'], ['PHOTOMETRIC=YCBCR']]:

        src_ds = gdal.Translate('', '../gdrivers/data/small_world.tif', format='MEM')
        src_ds.BuildOverviews('NEAR', overviewlist=[2, 4])
        ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_159.tif', src_ds,
                                          options=['COPY_SRC_OVERVIEWS=YES', 'COMPRESS=JPEG'] + options)
        ds = None
        src_ds = None

        ds = gdal.Open('/vsimem/tiff_write_159.tif')
        cs0 = ds.GetRasterBand(1).Checksum()
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
        assert not (cs0 == 0 or cs1 == 0 or cs2 == 0), options
        ifd_main = int(ds.GetRasterBand(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
        ifd_ovr_0 = int(ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('IFD_OFFSET', 'TIFF'))
        ifd_ovr_1 = int(ds.GetRasterBand(1).GetOverview(1).GetMetadataItem('IFD_OFFSET', 'TIFF'))
        data_ovr_1 = int(ds.GetRasterBand(1).GetOverview(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
        data_ovr_0 = int(ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
        data_main = int(ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
        assert (ifd_main < ifd_ovr_0 and ifd_ovr_0 < ifd_ovr_1 and ifd_ovr_1 < data_ovr_1 and data_ovr_1 < data_ovr_0 and data_ovr_0 < data_main), \
            options
        table_main = ds.GetRasterBand(1).GetMetadataItem('JPEGTABLES', 'TIFF')
        table_ovr_0 = ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('JPEGTABLES', 'TIFF')
        table_ovr_1 = ds.GetRasterBand(1).GetOverview(1).GetMetadataItem('JPEGTABLES', 'TIFF')
        assert table_main == table_ovr_0 and table_ovr_0 == table_ovr_1, options
        # Check that the JPEG tables are different in the 3 modes
        assert table_main != prev_table, options
        prev_table = table_main
        ds = None

        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_159.tif')

    for value in range(4):

        src_ds = gdal.Translate('', 'data/byte.tif', format='MEM')
        src_ds.BuildOverviews('NEAR', overviewlist=[2])
        ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_159.tif', src_ds,
                                          options=['COPY_SRC_OVERVIEWS=YES', 'COMPRESS=JPEG', 'JPEGTABLESMODE=%d' % value])
        ds = None
        src_ds = None

        ds = gdal.Open('/vsimem/tiff_write_159.tif')
        cs0 = ds.GetRasterBand(1).Checksum()
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs0 == 4743 and cs1 == 1133, value
        ds = None

        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_159.tif')



###############################################################################
# Test the Create() interface with a BLOCKYSIZE > image height

def test_tiff_write_160():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_160.tif', 10, 10, options=['BLOCKYSIZE=11'])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_160.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 1218
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_160.tif')

###############################################################################
# Test setting GCPs on an image with already a geotransform and vice-versa (#6751)


def test_tiff_write_161():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_161.tif', 1, 1)
    ds.SetGeoTransform([0, 1, 2, 3, 4, 5])
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_161.tif', gdal.GA_Update)
    src_ds = gdal.Open('data/gcps.vrt')
    with gdaltest.error_handler():
        assert ds.SetGCPs(src_ds.GetGCPs(), '') == 0
    assert ds.GetGeoTransform(can_return_null=True) is None
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_161.tif', gdal.GA_Update)
    assert ds.GetGCPs()
    assert ds.GetGeoTransform(can_return_null=True) is None
    with gdaltest.error_handler():
        assert ds.SetGeoTransform([0, 1, 2, 3, 4, 5]) == 0
    assert ds.GetGeoTransform() == (0.0, 1.0, 2.0, 3.0, 4.0, 5.0)
    assert not ds.GetGCPs()
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_161.tif', gdal.GA_Update)
    assert not ds.GetGCPs()
    assert ds.GetGeoTransform() == (0.0, 1.0, 2.0, 3.0, 4.0, 5.0)
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_161.tif')

###############################################################################
# Test creating a JPEG compressed file with big tiles (#6757)


def test_tiff_write_162():

    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 512, 3)

    options = ['TILED=YES', 'BLOCKXSIZE=512', 'BLOCKYSIZE=512', 'COMPRESS=JPEG']

    gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_162.tif', src_ds,
                                 options=options)

    assert gdal.GetLastErrorMsg() == ''

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_162.tif')

###############################################################################
# Test creating a file that would trigger strip chopping (#6924)


def test_tiff_write_163():

    # Was a libtiff 4.0.8 regression
    if gdaltest.tiff_drv.GetMetadataItem('LIBTIFF').find('4.0.8') >= 0:
        pytest.skip('Test broken with libtiff 4.0.8')

    gdal.Translate('/vsimem/tiff_write_163.tif', 'data/byte.tif',
                   options='-outsize 1 20000 -co BLOCKYSIZE=20000 -co PROFILE=BASELINE')
    ds = gdal.Open('/vsimem/tiff_write_163.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 47567
    # Check that IsBlockAvailable() works properly in that mode
    offset_0_2 = ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_2', 'TIFF')
    assert offset_0_2 == str(146 + 2 * 8192)
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_163.tif')

###############################################################################
# Test that we handle [0,1,0,0,0,1] geotransform as a regular geotransform


def test_tiff_write_164():

    ds = gdaltest.tiff_drv.Create('/vsimem/test.tif', 1, 1)
    ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    ds = None

    ds = gdal.Open('/vsimem/test.tif')
    gt = ds.GetGeoTransform(can_return_null=True)
    ds = None

    assert gt == (0, 1, 0, 0, 0, 1)

    # Test [0,1,0,0,0,-1] as well
    ds = gdaltest.tiff_drv.Create('/vsimem/test.tif', 1, 1)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds = None

    ds = gdal.Open('/vsimem/test.tif')
    gt = ds.GetGeoTransform(can_return_null=True)
    ds = None

    assert gt == (0, 1, 0, 0, 0, -1)

    gdal.Unlink('/vsimem/test.tif')

###############################################################################
# Test the current behaviour of per-band nodata vs per-dataset serialization


def test_tiff_write_165():

    ds = gdaltest.tiff_drv.Create('/vsimem/test.tif', 1, 1, 3)
    ret = ds.GetRasterBand(1).SetNoDataValue(100)
    assert ret == 0

    with gdaltest.error_handler():
        ret = ds.GetRasterBand(2).SetNoDataValue(200)
    assert gdal.GetLastErrorMsg() != '', 'warning expected, but not emitted'
    assert ret == 0

    nd = ds.GetRasterBand(1).GetNoDataValue()
    assert nd == 100

    nd = ds.GetRasterBand(2).GetNoDataValue()
    assert nd == 200

    ds = None

    ds = gdal.Open('/vsimem/test.tif')
    nd = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert nd == 200

    gdal.Unlink('/vsimem/test.tif')

###############################################################################
# Test reading & writing Z dimension for ModelTiepointTag and ModelPixelScaleTag (#7093)


def test_tiff_write_166():

    with gdaltest.config_option('GTIFF_REPORT_COMPD_CS', 'YES'):
        ds = gdal.Open('data/tiff_vertcs_scale_offset.tif')
        assert ds.GetRasterBand(1).GetScale() == 2.0
        assert ds.GetRasterBand(1).GetOffset() == 10.0

    # Scale + offset through CreateCopy()
    gdal.Translate('/vsimem/tiff_write_166.tif', 'data/byte.tif',
                   options='-a_srs EPSG:26711+5773 -a_scale 2.0 -a_offset 10 -co PROFILE=GEOTIFF')
    assert gdal.VSIStatL('/vsimem/tiff_write_166.tif.aux.xml') is None

    with gdaltest.config_option('GTIFF_REPORT_COMPD_CS', 'YES'):
        ds = gdal.Open('/vsimem/tiff_write_166.tif')
        assert ds.GetRasterBand(1).GetScale() == 2.0
        assert ds.GetRasterBand(1).GetOffset() == 10.0

    ds = None
    gdal.Unlink('/vsimem/tiff_write_166.tif')

    # Offset only through CreateCopy()
    gdal.Translate('/vsimem/tiff_write_166.tif', 'data/byte.tif',
                   options='-a_srs EPSG:26711+5773 -a_offset 10 -co PROFILE=GEOTIFF')
    assert gdal.VSIStatL('/vsimem/tiff_write_166.tif.aux.xml') is None

    with gdaltest.config_option('GTIFF_REPORT_COMPD_CS', 'YES'):
        ds = gdal.Open('/vsimem/tiff_write_166.tif')
        assert ds.GetRasterBand(1).GetScale() == 1.0
        assert ds.GetRasterBand(1).GetOffset() == 10.0

    ds = None
    gdal.Unlink('/vsimem/tiff_write_166.tif')

    # Scale + offset through Create()
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_166.tif', 1, 1, options=['PROFILE=GEOTIFF'])
    sr = osr.SpatialReference()
    sr.SetFromUserInput('EPSG:26711+5773')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([440720, 60, 0, 3751320, 0, -60])
    ds.GetRasterBand(1).SetScale(2)
    ds.GetRasterBand(1).SetOffset(10)
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_166.tif.aux.xml') is None

    with gdaltest.config_option('GTIFF_REPORT_COMPD_CS', 'YES'):
        ds = gdal.Open('/vsimem/tiff_write_166.tif')
        assert ds.GetRasterBand(1).GetScale() == 2.0
        assert ds.GetRasterBand(1).GetOffset() == 10.0
    ds = None
    gdal.Unlink('/vsimem/tiff_write_166.tif')

    # Scale only through Create()
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_166.tif', 1, 1, options=['PROFILE=GEOTIFF'])
    sr = osr.SpatialReference()
    sr.SetFromUserInput('EPSG:26711+5773')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([440720, 60, 0, 3751320, 0, -60])
    ds.GetRasterBand(1).SetScale(2)
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_166.tif.aux.xml') is None

    with gdaltest.config_option('GTIFF_REPORT_COMPD_CS', 'YES'):
        ds = gdal.Open('/vsimem/tiff_write_166.tif')
        assert ds.GetRasterBand(1).GetScale() == 2.0
        assert ds.GetRasterBand(1).GetOffset() == 0.0
    ds = None
    gdal.Unlink('/vsimem/tiff_write_166.tif')

    # Offset only through through Create()
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_166.tif', 1, 1, options=['PROFILE=GEOTIFF'])
    sr = osr.SpatialReference()
    sr.SetFromUserInput('EPSG:26711+5773')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([440720, 60, 0, 3751320, 0, -60])
    ds.GetRasterBand(1).SetOffset(10)
    ds = None
    assert gdal.VSIStatL('/vsimem/tiff_write_166.tif.aux.xml') is None

    with gdaltest.config_option('GTIFF_REPORT_COMPD_CS', 'YES'):
        ds = gdal.Open('/vsimem/tiff_write_166.tif')
        assert ds.GetRasterBand(1).GetScale() == 1.0
        assert ds.GetRasterBand(1).GetOffset() == 10.0
    ds = None
    gdal.Unlink('/vsimem/tiff_write_166.tif')

###############################################################################


def test_tiff_write_167_deflate_zlevel():

    src_ds = gdal.Open('data/byte.tif')
    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/out.tif', src_ds,
                                             options=['COMPRESS=DEFLATE',
                                                      'ZLEVEL=1'])
    size1 = gdal.VSIStatL('/vsimem/out.tif').size

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/out.tif', src_ds,
                                             options=['COMPRESS=DEFLATE',
                                                      'NUM_THREADS=2',
                                                      'ZLEVEL=9'])
    size2 = gdal.VSIStatL('/vsimem/out.tif').size
    gdal.Unlink('/vsimem/out.tif')

    assert size2 < size1

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 20, 20, 1,
                                              options=['COMPRESS=DEFLATE',
                                                       'ZLEVEL=9'])
    ds.SetProjection(src_ds.GetProjectionRef())
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    ds.WriteRaster(0, 0, 20, 20, src_ds.ReadRaster())
    ds = None

    size2_create = gdal.VSIStatL('/vsimem/out.tif').size
    gdal.Unlink('/vsimem/out.tif')

    assert size2 == size2_create

###############################################################################
# Test CCITTFAX3


def test_tiff_write_168_ccitfax3():

    ut = gdaltest.GDALTest('GTiff', 'oddsize1bit.tif', 1, 5918,
                           options=['NBITS=1', 'COMPRESS=CCITTFAX3'])
    return ut.testCreateCopy()

###############################################################################
# Test CCITTRLE


def test_tiff_write_169_ccitrle():

    ut = gdaltest.GDALTest('GTiff', 'oddsize1bit.tif', 1, 5918,
                           options=['NBITS=1', 'COMPRESS=CCITTRLE'])
    return ut.testCreateCopy()

###############################################################################
# Test invalid compression method


def test_tiff_write_170_invalid_compresion():

    src_ds = gdal.Open('data/byte.tif')
    with gdaltest.error_handler():
        gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/out.tif', src_ds,
                                                 options=['COMPRESS=INVALID'])
    assert gdal.GetLastErrorMsg() != ''
    gdal.Unlink('/vsimem/out.tif')

###############################################################################
# Test ZSTD compression


def test_tiff_write_171_zstd():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('ZSTD') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672,
                           options=['COMPRESS=ZSTD', 'ZSTD_LEVEL=1'])
    return ut.testCreateCopy()

###############################################################################
# Test ZSTD compression with PREDICTOR = 2


def test_tiff_write_171_zstd_predictor():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('ZSTD') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672,
                           options=['COMPRESS=ZSTD', 'ZSTD_LEVEL=1', 'PREDICTOR=2'])
    return ut.testCreateCopy()

###############################################################################
# Test WEBP compression


def test_tiff_write_webp():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('WEBP') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'md_ge_rgb_0010000.tif', 0, None,
                           options=['COMPRESS=WEBP'])
    return ut.testCreateCopy()

###############################################################################
# Test WEBP compression with internal tiling


def test_tiff_write_tiled_webp():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('WEBP') == -1:
        pytest.skip()

    if md['DMD_CREATIONOPTIONLIST'].find('WEBP_LOSSLESS') == -1:
        pytest.skip()

    filename = '/vsimem/tiff_write_tiled_webp.tif'
    src_ds = gdal.Open('data/md_ge_rgb_0010000.tif')
    gdaltest.tiff_drv.CreateCopy(filename, src_ds,
                                 options=['COMPRESS=WEBP',
                                          'WEBP_LOSSLESS=true',
                                          'TILED=true'])
    ds = gdal.Open(filename)
    cs = [ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    assert cs == [21212, 21053, 21349]

    gdaltest.tiff_drv.Delete(filename)
    gdal.Unlink('data/md_ge_rgb_0010000.tif.aux.xml')

###############################################################################
# Test WEBP compression with huge single strip


def test_tiff_write_webp_huge_single_strip():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('WEBP') == -1:
        pytest.skip()

    filename = '/vsimem/tif_webp_huge_single_strip.tif'
    src_ds = gdal.Open('data/tif_webp_huge_single_strip.tif')
    gdaltest.tiff_drv.CreateCopy(filename, src_ds,
                                 options=['COMPRESS=WEBP',
                                          'BLOCKYSIZE=2001'])
    ds = gdal.Open(filename)
    original_stats = [src_ds.GetRasterBand(i + 1).ComputeStatistics(True) for i in range(3)]
    got_stats = [ds.GetRasterBand(i + 1).ComputeStatistics(True) for i in range(3)]
    ds = None
    src_ds = None

    for i in range(3):
        for j in range(4):
            assert original_stats[i][j] == pytest.approx(got_stats[i][j], abs=1e-1 * abs(original_stats[i][j])), \
                'did not get expected statistics'

    gdaltest.tiff_drv.Delete(filename)
    gdal.Unlink('data/tif_webp_huge_single_strip.tif.aux.xml')


###############################################################################
# GeoTIFF DGIWG tags


def test_tiff_write_172_geometadata_tiff_rsid():

    tmpfilename = '/vsimem/tiff_write_172_geometadata_tiff_rsid.tiff'
    ds = gdal.GetDriverByName('GTiff').Create(tmpfilename, 1, 1)
    ds.SetMetadataItem('GEO_METADATA', 'foo')
    ds.SetMetadataItem('TIFF_RSID', 'bar')
    ds = None

    ds = gdal.Open(tmpfilename, gdal.GA_Update)
    assert ds.GetMetadataItem('GEO_METADATA') == 'foo', ds.GetMetadata()
    assert ds.GetMetadataItem('TIFF_RSID') == 'bar', ds.GetMetadata()
    ds.SetMetadata({})
    ds = None

    ds = gdal.Open(tmpfilename)
    assert ds.GetMetadataItem('GEO_METADATA') is None, ds.GetMetadata()
    assert ds.GetMetadataItem('TIFF_RSID') is None, ds.GetMetadata()
    ds = None

    gdal.Unlink(tmpfilename)

###############################################################################
# Test LERC compression


def test_tiff_write_173_lerc():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672,
                           options=['COMPRESS=LERC'])
    return ut.testCreateCopy()

###############################################################################
# Test LERC_DEFLATE compression


def test_tiff_write_174_lerc_deflate():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC_DEFLATE') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672,
                           options=['COMPRESS=LERC_DEFLATE'])
    return ut.testCreateCopy()

###############################################################################
# Test LERC_DEFLATE compression


def test_tiff_write_174_lerc_deflate_with_level():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC_DEFLATE') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672,
                           options=['COMPRESS=LERC_DEFLATE', 'ZLEVEL=1'])
    return ut.testCreateCopy()

###############################################################################
# Test LERC_ZSTD compression


def test_tiff_write_175_lerc_zstd():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC_ZSTD') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672,
                           options=['COMPRESS=LERC_ZSTD'])
    return ut.testCreateCopy()

###############################################################################
# Test LERC_ZSTD compression


def test_tiff_write_175_lerc_zstd_with_level():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC_ZSTD') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672,
                           options=['COMPRESS=LERC_ZSTD', 'ZSTD_LEVEL=1'])
    return ut.testCreateCopy()

###############################################################################
# Test LERC compression with MAX_Z_ERROR


def test_tiff_write_176_lerc_max_z_error():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4529,
                           options=['COMPRESS=LERC', 'MAX_Z_ERROR=1'])
    return ut.testCreateCopy(skip_preclose_test=1)

###############################################################################
# Test LERC compression with several bands and tiling


def test_tiff_write_177_lerc_several_bands_tiling():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    filename = '/vsimem/tiff_write_177_lerc_several_bands_tiling.tif'
    gdal.Translate(filename, '../gdrivers/data/small_world.tif',
                   creationOptions=['COMPRESS=LERC', 'TILED=YES'])
    ds = gdal.Open(filename)
    cs = [ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    ds = None
    gdal.Unlink(filename)
    assert cs == [30111, 32302, 40026]

###############################################################################
# Test LERC compression with alpha band


def test_tiff_write_178_lerc_with_alpha():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    filename = '/vsimem/tiff_write_178_lerc_with_alpha.tif'
    gdal.Translate(filename, 'data/stefan_full_rgba.tif',
                   creationOptions=['COMPRESS=LERC'])
    ds = gdal.Open(filename)
    cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    ds = None
    gdal.Unlink(filename)
    assert cs == [12603, 58561, 36064, 10807]

###############################################################################
# Test LERC compression with alpha band with only 0 and 255


def test_tiff_write_178_lerc_with_alpha_0_and_255():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    filename = '/vsimem/tiff_write_178_lerc_with_alpha_0_and_255.tif'
    gdal.Translate(filename, 'data/rgba_with_alpha_0_and_255.tif',
                   creationOptions=['COMPRESS=LERC'])
    ds = gdal.Open(filename)
    cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    ds = None
    gdal.Unlink(filename)
    assert cs == [13, 13, 13, 13]

###############################################################################
# Test LERC compression with different data types


def test_tiff_write_179_lerc_data_types():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    filename = '/vsimem/tiff_write_179_lerc_data_types.tif'
    for src_filename in ['uint16.tif', 'int16.tif', 'uint32.tif', 'int32.tif',
                         'float32.tif', 'float64.tif']:
        gdal.Translate(filename, 'data/' + src_filename,
                       creationOptions=['COMPRESS=LERC'])
        ds = gdal.Open(filename)
        cs = ds.GetRasterBand(1).Checksum()
        ds = None
        gdal.Unlink(filename)
        assert cs == 4672

    filename_tmp = filename + ".tmp.tif"
    gdal.Translate(filename_tmp, 'data/byte.tif',
                   creationOptions=['PIXELTYPE=SIGNEDBYTE'])
    gdal.Translate(filename, filename_tmp, creationOptions=['COMPRESS=LERC'])
    gdal.Unlink(filename_tmp)
    ds = gdal.Open(filename)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink(filename)
    assert cs == 4672

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.Translate(filename, 'data/cfloat32.tif', creationOptions=['COMPRESS=LERC'])
    assert gdal.GetLastErrorMsg() != ''
    gdal.Unlink(filename)

###############################################################################
# Test LERC compression with several bands and separate


def test_tiff_write_180_lerc_separate():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    filename = '/vsimem/tiff_write_180_lerc_separate.tif'
    gdal.Translate(filename, '../gdrivers/data/small_world.tif',
                   creationOptions=['COMPRESS=LERC', 'INTERLEAVE=BAND'])
    ds = gdal.Open(filename)
    cs = [ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    ds = None
    gdal.Unlink(filename)
    assert cs == [30111, 32302, 40026]


###############################################################################
# Test MAX_Z_ERROR_OVERVIEW effect while creating overviews
# on a newly created dataset

@pytest.mark.parametrize("external_ovr,compression", [(True, 'LERC_ZSTD'),
                                                      (False, 'LERC_ZSTD'),
                                                      (True, 'LERC_DEFLATE'),
                                                      (False, 'LERC_DEFLATE')])
def test_tiff_write_lerc_overview(external_ovr, compression):
    md = gdaltest.tiff_drv.GetMetadata()
    if compression not in md['DMD_CREATIONOPTIONLIST']:
        pytest.skip()

    checksums = {}
    errors = [0,10,10]
    src_ds = gdal.Open('../gdrivers/data/utm.tif')
    for i, error in enumerate(errors):
        fname = '/vsimem/test_tiff_write_lerc_overview_%d' % i

        ds = gdal.GetDriverByName('GTiff').Create(fname, 256, 256, 1,
                                                  options=['COMPRESS=' + compression,
                                                           'MAX_Z_ERROR=%f' % error])
        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 256, 256)
        ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)
        if i == 2:
            error = 30
        options = {}
        if external_ovr:
            ds = None
            ds = gdal.Open(fname)
            options['COMPRESS_OVERVIEW'] = compression
        options['MAX_Z_ERROR_OVERVIEW'] = '%d' % error
        with gdaltest.config_options(options):
            ds.BuildOverviews('AVERAGE', overviewlist=[2, 4])

        ds = None

        ds = gdal.Open(fname)
        assert ds.GetRasterBand(1).GetOverview(0).GetDataset().GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == compression
        checksums[i] = [ ds.GetRasterBand(1).Checksum(),
                               ds.GetRasterBand(1).GetOverview(0).Checksum(),
                               ds.GetRasterBand(1).GetOverview(1).Checksum() ]
        ds = None
        gdaltest.tiff_drv.Delete(fname)

    assert checksums[0][0] != checksums[1][0]
    assert checksums[0][1] != checksums[1][1]
    assert checksums[0][2] != checksums[1][2]

    assert checksums[0][0] != checksums[2][0]
    assert checksums[0][1] != checksums[2][1]
    assert checksums[0][2] != checksums[2][2]

    assert checksums[1][0] == checksums[2][0]
    assert checksums[1][1] != checksums[2][1]
    assert checksums[1][2] != checksums[2][2]

###############################################################################
# Test ZLEVEL_OVERVIEW effect while creating overviews
# on a newly created dataset

@pytest.mark.parametrize("external_ovr", [True, False])
def test_tiff_write_lerc_zlevel(external_ovr):
    md = gdaltest.tiff_drv.GetMetadata()
    if 'LERC_DEFLATE' not in md['DMD_CREATIONOPTIONLIST']:
        pytest.skip()

    filesize = {}
    src_ds = gdal.Open('../gdrivers/data/utm.tif')
    for level in (1,9):
        fname = '/vsimem/test_tiff_write_lerc_zlevel_%d' % level
        ds = gdal.GetDriverByName('GTiff').Create(fname, 256, 256, 1,
                                                  options=['COMPRESS=LERC_DEFLATE'])
        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 256, 256)
        ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)
        options = { 'MAX_Z_ERROR_OVERVIEW' : '10' }
        if external_ovr:
            ds = None
            ds = gdal.Open(fname)
            options['COMPRESS_OVERVIEW'] = 'LERC_DEFLATE'
        options['ZLEVEL_OVERVIEW'] = '%d' % level
        with gdaltest.config_options(options):
            ds.BuildOverviews('AVERAGE', overviewlist=[2, 4])
        ds = None

        if external_ovr:
            filesize[level] = gdal.VSIStatL(fname + '.ovr').size
        else:
            filesize[level] = gdal.VSIStatL(fname).size
        gdaltest.tiff_drv.Delete(fname)

    assert filesize[1] > filesize[9]

###############################################################################
# Test ZSTD_LEVEL_OVERVIEW effect while creating overviews
# on a newly created dataset

@pytest.mark.parametrize("external_ovr", [True, False])
def test_tiff_write_lerc_zstd_level(external_ovr):
    md = gdaltest.tiff_drv.GetMetadata()
    if 'LERC_ZSTD' not in md['DMD_CREATIONOPTIONLIST']:
        pytest.skip()

    filesize = {}
    src_ds = gdal.Open('../gdrivers/data/utm.tif')
    for level in (1,22):
        fname = '/vsimem/test_tiff_write_lerc_zstd_level_%d' % level
        ds = gdal.GetDriverByName('GTiff').Create(fname, 256, 256, 1,
                                                  options=['COMPRESS=LERC_ZSTD'])
        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 256, 256)
        ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)
        options = { 'MAX_Z_ERROR_OVERVIEW' : '10' }
        if external_ovr:
            ds = None
            ds = gdal.Open(fname)
            options['COMPRESS_OVERVIEW'] = 'LERC_ZSTD'
        options['ZSTD_LEVEL_OVERVIEW'] = '%d' % level
        with gdaltest.config_options(options):
            ds.BuildOverviews('AVERAGE', overviewlist=[2, 4])
        ds = None

        if external_ovr:
            filesize[level] = gdal.VSIStatL(fname + '.ovr').size
        else:
            filesize[level] = gdal.VSIStatL(fname).size
        gdaltest.tiff_drv.Delete(fname)

    assert filesize[1] > filesize[22]

###############################################################################
# Test set XMP metadata


def test_tiff_write_181_xmp():

    src_ds = gdal.Open('data/utmsmall.tif')

    new_ds = gdaltest.tiff_drv.CreateCopy('tmp/test_181.tif', src_ds)
    src_ds = None

    xmp_ds = gdal.Open('../gdrivers/data/gtiff/byte_with_xmp.tif')
    xmp = xmp_ds.GetMetadata('xml:XMP')
    xmp_ds = None
    assert 'W5M0MpCehiHzreSzNTczkc9d' in xmp[0], 'Wrong input file without XMP'

    new_ds.SetMetadata(xmp, 'xml:XMP')
    new_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open('tmp/test_181.tif')
    read_xmp = new_ds.GetMetadata('xml:XMP')
    assert read_xmp and 'W5M0MpCehiHzreSzNTczkc9d' in read_xmp[0], \
        'No XMP data written in output file'
    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_181.tif')


def test_tiff_write_181_xmp_copy():

    src_ds = gdal.Open('../gdrivers/data/gtiff/byte_with_xmp.tif')

    filename = 'tmp/test_181_copy.tif'
    new_ds = gdaltest.tiff_drv.CreateCopy(filename, src_ds)
    assert new_ds is not None
    src_ds = None

    new_ds = None
    new_ds = gdal.Open(filename)

    assert int(new_ds.GetRasterBand(1).GetMetadataItem('IFD_OFFSET', 'TIFF')) == 8, 'TIFF directory not at the beginning'

    xmp = new_ds.GetMetadata('xml:XMP')
    new_ds = None
    assert 'W5M0MpCehiHzreSzNTczkc9d' in xmp[0], 'Wrong input file without XMP'

    gdaltest.tiff_drv.Delete(filename)


###############################################################################
# Test delete XMP from a dataset


def test_tiff_write_182_xmp_delete():

    shutil.copyfile('../gdrivers/data/gtiff/byte_with_xmp.tif', 'tmp/test_182.tif')

    chg_ds = gdal.Open('tmp/test_182.tif', gdal.GA_Update)
    read_xmp = chg_ds.GetMetadata('xml:XMP')
    assert read_xmp and 'W5M0MpCehiHzreSzNTczkc9d' in read_xmp[0], \
        'No XMP data written in output file'
    chg_ds.SetMetadata(None, 'xml:XMP')
    chg_ds = None

    again_ds = gdal.Open('tmp/test_182.tif')
    read_xmp = again_ds.GetMetadata('xml:XMP')
    assert not read_xmp, 'XMP data not removed'
    again_ds = None

    gdaltest.tiff_drv.Delete('tmp/test_182.tif')

###############################################################################


def test_tiff_write_183_createcopy_append_subdataset():

    tmpfilename = '/vsimem/test_tiff_write_183_createcopy_append_subdataset.tif'
    gdal.Translate(tmpfilename, 'data/byte.tif')
    gdal.Translate(tmpfilename, 'data/utmsmall.tif',
                   creationOptions=['APPEND_SUBDATASET=YES'])

    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.Open('GTIFF_DIR:2:' + tmpfilename)
    assert ds.GetRasterBand(1).Checksum() == 50054

    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################


def test_tiff_write_184_create_append_subdataset():

    tmpfilename = '/vsimem/test_tiff_write_184_create_append_subdataset.tif'
    gdal.Translate(tmpfilename, 'data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').Create(tmpfilename, 1, 1,
                                              options=['APPEND_SUBDATASET=YES'])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.Open('GTIFF_DIR:2:' + tmpfilename)
    assert ds.GetRasterBand(1).Checksum() == 3

    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################
# Test LERC compression with Create() and BuildOverviews()
# Fixes https://github.com/OSGeo/gdal/issues/1257


def test_tiff_write_185_lerc_create_and_overview():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    filename = '/vsimem/test_tiff_write_185_lerc_create_and_overview.tif'
    ds = gdaltest.tiff_drv.Create(filename, 20, 20, options=['COMPRESS=LERC_DEFLATE'])
    src_ds = gdal.Open('data/byte.tif')
    ds.WriteRaster(0,0,20,20,src_ds.ReadRaster())
    gdal.ErrorReset()
    ds.BuildOverviews('NEAR', [2])
    assert gdal.GetLastErrorMsg() == ''
    ds = None
    ds = gdal.Open(filename)
    cs = ds.GetRasterBand(1).Checksum()
    cs_ovr = ds.GetRasterBand(1).GetOverview(0).Checksum()
    gdal.Unlink(filename)
    assert (cs, cs_ovr) == (4672, 1087)

    filename2 = '/vsimem/test_tiff_write_185_lerc_create_and_overview_copy.tif'
    gdaltest.tiff_drv.CreateCopy(filename2, ds, options=['COMPRESS=LERC_DEFLATE', 'COPY_SRC_OVERVIEWS=YES'])
    assert gdal.GetLastErrorMsg() == ''
    ds = gdal.Open(filename2)
    cs = ds.GetRasterBand(1).Checksum()
    cs_ovr = ds.GetRasterBand(1).GetOverview(0).Checksum()
    gdal.Unlink(filename2)
    assert (cs, cs_ovr) == (4672, 1087)


###############################################################################

def check_libtiff_internal_or_at_least(expected_maj, expected_min, expected_micro):

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] == 'INTERNAL':
        return True
    if md['LIBTIFF'].startswith('LIBTIFF, Version '):
        version = md['LIBTIFF'][len('LIBTIFF, Version '):]
        version = version[0:version.find('\n')]
        got_maj, got_min, got_micro = version.split('.')
        got_maj = int(got_maj)
        got_min = int(got_min)
        got_micro = int(got_micro)
        if got_maj > expected_maj:
            return True
        if got_maj < expected_maj:
            return False
        if got_min > expected_min:
            return True
        if got_min < expected_min:
            return False
        return got_micro >= expected_micro
    return False

###############################################################################
# Test writing a deflate compressed file with a uncompressed strip larger than 4 GB
#

def test_tiff_write_deflate_4GB():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    if not gdaltest.run_slow_tests():
        pytest.skip()

    ref_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    ref_ds.GetRasterBand(1).Fill(127)

    gdal.Translate('/vsimem/out.tif', ref_ds,
                   options = '-co TILED=YES -co COMPRESS=DEFLATE -co BLOCKXSIZE=50000 -co BLOCKYSIZE=86000 -outsize 50000 86000')

    ds = gdal.Open('/vsimem/out.tif')
    data  = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buf_xsize = 20, buf_ysize = 20)
    assert data == ref_ds.ReadRaster()
    ds = None

    gdal.Unlink('/vsimem/out.tif')


###############################################################################
# Test rewriting a LZW strip/tile that is very close to 8 KB with larger data

def test_tiff_write_rewrite_lzw_strip():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    src_data = open('data/bug_gh_1439_to_be_updated_lzw.tif', 'rb').read()
    tmpfilename = '/vsimem/out.tif'
    gdal.FileFromMemBuffer(tmpfilename, src_data)
    ds = gdal.Open(tmpfilename, gdal.GA_Update)
    src_ds = gdal.Open('data/bug_gh_1439_update_lzw.tif')
    ds.WriteRaster(0,0,4096,1,src_ds.ReadRaster())
    ds = None
    ds = gdal.Open(tmpfilename)
    gdal.ErrorReset()
    assert ds.GetRasterBand(1).ReadRaster(0,1,4096,1)
    assert gdal.GetLastErrorMsg() == ''

    gdal.Unlink(tmpfilename)


###############################################################################
# Test COPY_SRC_OVERVIEWS on a configuration with overviews, mask, but no
# overview on the mask

def test_tiff_write_overviews_mask_no_ovr_on_mask():

    tmpfile = '/vsimem/test_tiff_write_overviews_mask_no_ovr_on_mask.tif'
    with gdaltest.config_option('GDAL_TIFF_INTERNAL_MASK', 'YES'):
        ds = gdaltest.tiff_drv.Create(tmpfile, 100, 100)
        ds.GetRasterBand(1).Fill(255)
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)

    ds = gdal.Open(tmpfile)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds.BuildOverviews('NEAR', overviewlist=[2])
    assert 'Building external overviews whereas there is an internal mask is not fully supported. The overviews of the non-mask bands will be created, but not the overviews of the mask band.' in gdal.GetLastErrorMsg()
    # No overview on the mask
    assert ds.GetRasterBand(1).GetOverview(0).GetMaskFlags() == gdal.GMF_ALL_VALID
    ds = None

    tmpfile2 = '/vsimem/test_tiff_write_overviews_mask_no_ovr_on_mask_copy.tif'
    src_ds = gdal.Open(tmpfile)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdaltest.tiff_drv.CreateCopy(tmpfile2, src_ds, options=['COPY_SRC_OVERVIEWS=YES'])
    assert 'Source dataset has a mask band on full resolution, overviews on the regular bands, but lacks overviews on the mask band.' in gdal.GetLastErrorMsg()
    assert ds
    ds = None
    src_ds = None

    ds = gdal.Open(tmpfile)
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    # No overview on the mask
    assert ds.GetRasterBand(1).GetOverview(0).GetMaskFlags() == gdal.GMF_ALL_VALID
    ds = None

    gdaltest.tiff_drv.Delete(tmpfile)
    gdaltest.tiff_drv.Delete(tmpfile2)


###############################################################################
# Test that -co PHOTOMETRIC=YCBCR -co COMPRESS=JPEG does not create a TIFFTAG_GDAL_METADATA

def test_tiff_write_no_gdal_metadata_tag_for_ycbcr_jpeg():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    tmpfile = '/vsimem/test_tiff_write_no_gdal_metadata_tag_for_ycbcr_jpeg.tif'
    assert gdaltest.tiff_drv.Create(tmpfile, 16, 16, 3, gdal.GDT_Byte, options=['PHOTOMETRIC=YCBCR', 'COMPRESS=JPEG'])
    statBuf = gdal.VSIStatL(tmpfile + '.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    assert statBuf is None, 'did not expect PAM file'

    ds = gdal.Open(tmpfile)
    assert ds.GetMetadataItem('TIFFTAG_GDAL_METADATA', '_DEBUG_') is None, \
        'did not expect TIFFTAG_GDAL_METADATA tag'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand

    tmpfile2 = tmpfile + "2"
    assert gdaltest.tiff_drv.CreateCopy(tmpfile2, ds, options=['PHOTOMETRIC=YCBCR', 'COMPRESS=JPEG'])
    ds = None

    ds = gdal.Open(tmpfile2)
    assert ds.GetMetadataItem('TIFFTAG_GDAL_METADATA', '_DEBUG_') is None, \
        'did not expect TIFFTAG_GDAL_METADATA tag'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    ds = None

    gdaltest.tiff_drv.Delete(tmpfile)
    gdaltest.tiff_drv.Delete(tmpfile2)


###############################################################################
# Test that repated flushing after SetGeoTransform() does not grow file size
# indefinitely

def test_tiff_write_setgeotransform_flush():

    tmpfile = '/vsimem/test_tiff_write_setgeotransform_flush.tif'
    gdal.GetDriverByName('GTiff').Create(tmpfile,1,1)
    ds = gdal.Open(tmpfile, gdal.GA_Update)
    ds.SetGeoTransform([2,0,1,49,0,-1])
    for i in range(10):
        ds.FlushCache()
    ds = None

    assert gdal.VSIStatL(tmpfile).size < 1000

    gdaltest.tiff_drv.Delete(tmpfile)

###############################################################################
# Test that compression parameters are taken into account in Create() mode

def test_tiff_write_compression_create_and_createcopy():

    md = gdaltest.tiff_drv.GetMetadata()
    tests = []

    if 'DEFLATE' in md['DMD_CREATIONOPTIONLIST']:
        tests.append((['COMPRESS=DEFLATE', 'ZLEVEL=1'],['COMPRESS=DEFLATE', 'ZLEVEL=9']))

    if 'LZMA' in md['DMD_CREATIONOPTIONLIST']:
        tests.append((['COMPRESS=LZMA', 'LZMA_PRESET=1'],['COMPRESS=LZMA', 'LZMA_PRESET=9']))

    if 'JPEG' in md['DMD_CREATIONOPTIONLIST']:
        tests.append((['COMPRESS=JPEG', 'JPEG_QUALITY=95'],['COMPRESS=JPEG', 'JPEG_QUALITY=50']))

    if 'ZSTD' in md['DMD_CREATIONOPTIONLIST']:
        tests.append((['COMPRESS=ZSTD', 'ZSTD_LEVEL=1'],['COMPRESS=ZSTD', 'ZSTD_LEVEL=9']))

    # FIXME: this test randomly fails, especially on Windows, but also on Linux,
    # for a unknown reason. Nothing suspicious with Valgrind however
    # if 'LERC_DEFLATE' in md['DMD_CREATIONOPTIONLIST']:
    #   tests.append((['COMPRESS=LERC_DEFLATE', 'ZLEVEL=1'],['COMPRESS=LERC_DEFLATE', 'ZLEVEL=9']))

    if 'WEBP' in md['DMD_CREATIONOPTIONLIST']:
        tests.append((['COMPRESS=WEBP', 'WEBP_LEVEL=95'],['COMPRESS=WEBP', 'WEBP_LEVEL=15']))

    if 'JXL' in md['DMD_CREATIONOPTIONLIST']:
        tests.append((['COMPRESS=JXL', 'JXL_LOSSLESS=YES'],['COMPRESS=JXL', 'JXL_LOSSLESS=NO']))
        tests.append((['COMPRESS=JXL', 'JXL_LOSSLESS=YES', 'JXL_EFFORT=3'],['COMPRESS=JXL', 'JXL_LOSSLESS=YES', 'JXL_EFFORT=9']))
        tests.append((['COMPRESS=JXL', 'JXL_LOSSLESS=NO', 'JXL_DISTANCE=0.1'],['COMPRESS=JXL', 'JXL_LOSSLESS=NO', 'JXL_DISTANCE=3']))

    new_tests = []
    for (before, after) in tests:
        new_tests.append((before, after))
        new_tests.append((before + ['COPY_SRC_OVERVIEWS=YES', 'TILED=YES', 'NUM_THREADS=2'],
                          after + ['COPY_SRC_OVERVIEWS=YES', 'TILED=YES', 'NUM_THREADS=2']))
    tests = new_tests

    tmpfile = '/vsimem/test_tiff_write_compression_create.tif'

    src_ds = gdal.Open('data/rgbsmall.tif')
    data = src_ds.ReadRaster()
    for (before, after) in tests:
        ds = gdaltest.tiff_drv.Create(tmpfile, src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, options = before)
        ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data)
        ds = None
        size_before = gdal.VSIStatL(tmpfile).size
        ds = gdaltest.tiff_drv.Create(tmpfile, src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, options = after)
        ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data)
        ds = None
        size_after = gdal.VSIStatL(tmpfile).size
        assert size_after < size_before, (before, after, size_before, size_after)
        print(before, after, size_before, size_after)

        gdaltest.tiff_drv.CreateCopy(tmpfile, src_ds, options = before)
        size_before = gdal.VSIStatL(tmpfile).size
        gdaltest.tiff_drv.CreateCopy(tmpfile, src_ds, options = after)
        size_after = gdal.VSIStatL(tmpfile).size
        assert size_after < size_before, (before, after, size_before, size_after)

    gdaltest.tiff_drv.Delete(tmpfile)

###############################################################################
# Attempt at creating a file with more tile arrays larger than 2 GB


def test_tiff_write_too_many_tiles():

    src_ds = gdal.Open('<VRTDataset rasterXSize="40000000" rasterYSize="40000000"><VRTRasterBand dataType="Byte" band="1"/></VRTDataset>')
    with gdaltest.error_handler():
        assert not gdaltest.tiff_drv.CreateCopy('/vsimem/tmp.tif', src_ds, options = ['TILED=YES'])
    assert 'File too large regarding tile size' in gdal.GetLastErrorMsg()

    with gdaltest.tempfile('/vsimem/test_tiff_write_too_many_tiles.vrt',
                           '<VRTDataset rasterXSize="40000000" rasterYSize="40000000"><VRTRasterBand dataType="Byte" band="1"/></VRTDataset>'):
        src_ds = gdal.Open('/vsimem/test_tiff_write_too_many_tiles.vrt')
        gdal.ErrorReset()
        with gdaltest.config_option('GDAL_TIFF_OVR_BLOCKSIZE', '128'):
            with gdaltest.error_handler():
                src_ds.BuildOverviews('NEAR', [2])
        assert 'File too large regarding tile size' in gdal.GetLastErrorMsg()


###############################################################################
#


def test_tiff_write_jpeg_incompatible_of_paletted():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open('data/test_average_palette.tif')
    with gdaltest.error_handler():
        assert not gdaltest.tiff_drv.CreateCopy('/vsimem/tmp.tif', src_ds, options = ['COMPRESS=JPEG'])
    gdal.Unlink('/vsimem/tmp.tif')


###############################################################################
# Test blocksize overriding while creating (internal) overviews
# on a newly created dataset

@pytest.mark.parametrize("blockSize,numThreads", [[64, None], [256, 8]])
def test_tiff_write_internal_ovr_blocksize(blockSize, numThreads):

    src_ds = gdal.Open('../gdrivers/data/utm.tif')
    fname = 'tmp/tiff_write_internal_ovr_bs%d.tif' % blockSize

    ds = gdal.GetDriverByName('GTiff').Create(fname, 1024, 1024, 1,
                                                options=['TILED=YES','COMPRESS=LZW',
                                                    'BLOCKXSIZE=512', 'BLOCKYSIZE=512'])

    data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
    opts = {'GDAL_TIFF_OVR_BLOCKSIZE':'%d'%blockSize}
    if numThreads:
        opts['GDAL_NUM_THREADS'] = str(numThreads)
    with gdaltest.config_options(opts):
        ds.BuildOverviews('AVERAGE', overviewlist=[2])

    src_ds = None
    ds = None

    ds = gdal.Open(fname)
    (bsx,bsy) = ds.GetRasterBand(1).GetOverview(0).GetBlockSize()
    assert bsx == blockSize
    assert bsy == blockSize
    ds = None
    gdaltest.tiff_drv.Delete(fname)

###############################################################################
# Test blocksize propagation while creating (internal) overviews
# on a newly created dataset

@pytest.mark.parametrize("blockSize,numThreads", [[64, None], [256, 8]])
def test_tiff_write_internal_ovr_default_blocksize(blockSize, numThreads):

    src_ds = gdal.Open('../gdrivers/data/utm.tif')
    fname = 'tmp/tiff_write_internal_ovr_default_bs%d.tif' % blockSize

    ds = gdal.GetDriverByName('GTiff').Create(fname, 1024, 1024, 1,
                                                options=['TILED=YES','COMPRESS=LZW',
                                                    'BLOCKXSIZE=%d'%blockSize,
                                                    'BLOCKYSIZE=%d'%blockSize])

    data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
    opts = {}
    if numThreads:
        opts['GDAL_NUM_THREADS'] = str(numThreads)
    with gdaltest.config_options(opts):
        ds.BuildOverviews('AVERAGE', overviewlist=[2])

    src_ds = None
    ds = None

    ds = gdal.Open(fname)
    (bsx,bsy) = ds.GetRasterBand(1).GetOverview(0).GetBlockSize()
    assert bsx == blockSize
    assert bsy == blockSize
    ds = None
    gdaltest.tiff_drv.Delete(fname)


###############################################################################
# Test LERC compression with Float32/Float64


@pytest.mark.parametrize("gdalDataType,structType", [[gdal.GDT_Float32,'f'],[gdal.GDT_Float64,'d']])
def test_tiff_write_lerc_float(gdalDataType, structType):

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 1, 1, gdalDataType)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack(structType * 2, 0.5, 1.5))
    filename = '/vsimem/test.tif'
    gdaltest.tiff_drv.CreateCopy(filename, src_ds, options = ['COMPRESS=LERC'])
    ds = gdal.Open(filename)
    assert struct.unpack(structType * 2, ds.ReadRaster()) == (0.5, 1.5)
    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test LERC compression withFloat32/Float64 and nan


@pytest.mark.parametrize("gdalDataType,structType", [[gdal.GDT_Float32,'f'],[gdal.GDT_Float64,'d']])
def test_tiff_write_lerc_float_with_nan(gdalDataType, structType):

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 1, 1, gdalDataType)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack(structType * 2, 0.5, float('nan')))
    filename = '/vsimem/test.tif'
    gdaltest.tiff_drv.CreateCopy(filename, src_ds, options = ['COMPRESS=LERC'])
    ds = gdal.Open(filename)
    got_data = struct.unpack(structType * 2, ds.ReadRaster())
    assert got_data[0] == 0.5
    assert math.isnan(got_data[1])
    ds = None
    gdal.Unlink(filename)

###############################################################################
# Test JXL compression


def test_tiff_write_jpegxl_byte_single_band():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JXL') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte.tif', 1, 4672, options=['COMPRESS=JXL'])
    return ut.testCreateCopy()

###############################################################################
# Test JXL compression


def test_tiff_write_jpegxl_byte_three_band():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JXL') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'rgbsmall.tif', 1, 21212, options=['COMPRESS=JXL'])
    return ut.testCreateCopy()

###############################################################################
# Test JXL compression


def test_tiff_write_jpegxl_uint16_single_band():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JXL') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'uint16.tif', 1, 4672, options=['COMPRESS=JXL'])
    return ut.testCreateCopy()

###############################################################################
# Test creating overviews with NaN nodata


def test_tiff_write_overviews_nan_nodata():

    filename = '/vsimem/test_tiff_write_overviews_nan_nodata.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 32, 32, 1, gdal.GDT_Float32, options=['TILED=YES', 'SPARSE_OK=YES'])
    ds.GetRasterBand(1).SetNoDataValue(float('nan'))
    ds.BuildOverviews('NONE', [2, 4])
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test support for coordinate epoch


def test_tiff_write_coordinate_epoch():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/test_tiff_write_coordinate_epoch.tif', 1, 1)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(2021.3)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.SetSpatialRef(srs)
    ds = None

    ds = gdal.Open('/vsimem/test_tiff_write_coordinate_epoch.tif')
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None

    gdal.Unlink('/vsimem/test_tiff_write_coordinate_epoch.tif')


###############################################################################
# Test scenario with multiple IFDs and directory rewriting
# https://github.com/OSGeo/gdal/issues/3746


@pytest.mark.parametrize("reopen", [True, False])
def test_tiff_write_multiple_ifds_directory_rewriting(reopen):

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 32, 32, options=['TILED=YES', 'SPARSE_OK=YES'])
    ds.BuildOverviews('NONE', [2])
    if reopen:
        ds = None
        ds = gdal.Open(filename, gdal.GA_Update)

    ds.GetRasterBand(1).GetOverview(0).Fill(2)

    # Rewrite second IFD
    ds.GetRasterBand(1).GetOverview(0).SetNoDataValue(0)
    # Rewrite first IFD
    ds.GetRasterBand(1).SetNoDataValue(3)

    ds = None

    ds = gdal.Open(filename)
    mm = ds.GetRasterBand(1).GetOverview(0).ComputeRasterMinMax()
    ds = None

    gdal.Unlink(filename)
    assert mm == (2, 2)

###############################################################################
# Test SetSpatialRef() on a read-only dataset


def test_tiff_write_setspatialref_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    assert ds.SetSpatialRef(srs) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    got_srs = ds.GetSpatialRef()
    assert got_srs
    assert got_srs.GetAuthorityCode(None) == '4326'
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetSpatialRef() on a read-only dataset, overriding TIFF tags


def test_tiff_write_setspatialref_read_only_override_tifftags():

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    assert ds.SetSpatialRef(srs) == gdal.CE_None
    ds = None

    ds = gdal.Open(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    assert ds.SetSpatialRef(srs) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    got_srs = ds.GetSpatialRef()
    assert got_srs
    assert got_srs.GetAuthorityCode(None) == '4326'
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32632)
    assert ds.SetSpatialRef(srs) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is None

    ds = gdal.Open(filename)
    got_srs = ds.GetSpatialRef()
    assert got_srs
    assert got_srs.GetAuthorityCode(None) == '32632'
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetGeoTransform() on a read-only dataset


def test_tiff_write_setgeotransform_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    gt = [2,1,0,49,0,-1]
    assert ds.SetGeoTransform(gt) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    got_gt = [x for x in ds.GetGeoTransform()]
    assert got_gt == gt
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetGeoTransform() on a read-only dataset, overriding TIFF tags


def test_tiff_write_setgeotransform_read_only_override_tifftags():

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    assert ds.SetGeoTransform([3,1,0,50,0,-1]) == gdal.CE_None
    ds = None

    ds = gdal.Open(filename)
    gt = [2,1,0,49,0,-1]
    assert ds.SetGeoTransform(gt) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    got_gt = [x for x in ds.GetGeoTransform()]
    assert got_gt == gt
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    gt = [4,1,0,51,0,-1]
    assert ds.SetGeoTransform(gt) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is None

    ds = gdal.Open(filename)
    got_gt = [x for x in ds.GetGeoTransform()]
    assert got_gt == gt
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetGCPs() on a read-only dataset


def test_tiff_write_setgcps_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    assert ds.SetGCPs(gcps, srs) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    got_gcps = ds.GetGCPs()
    assert len(got_gcps) == len(gcps)
    assert got_gcps[0].GCPPixel == gcps[0].GCPPixel
    assert got_gcps[0].GCPLine == gcps[0].GCPLine
    assert got_gcps[0].GCPX == gcps[0].GCPX
    assert got_gcps[0].GCPY == gcps[0].GCPY
    got_srs = ds.GetGCPSpatialRef()
    assert got_srs
    assert got_srs.GetAuthorityCode(None) == '4326'
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetGCPs() on a read-only dataset, overriding TIFF tags


def test_tiff_write_setgcps_read_only_override_tifftags():

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    gcps = [gdal.GCP(5, 6, 7, 8, 9)]
    assert ds.SetGCPs(gcps, None) == gdal.CE_None
    ds = None

    ds = gdal.Open(filename)
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    assert ds.SetGCPs(gcps, srs) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    got_gcps = ds.GetGCPs()
    assert len(got_gcps) == len(gcps)
    assert got_gcps[0].GCPPixel == gcps[0].GCPPixel
    assert got_gcps[0].GCPLine == gcps[0].GCPLine
    assert got_gcps[0].GCPX == gcps[0].GCPX
    assert got_gcps[0].GCPY == gcps[0].GCPY
    got_srs = ds.GetGCPSpatialRef()
    assert got_srs
    assert got_srs.GetAuthorityCode(None) == '4326'
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    gcps = [gdal.GCP(10, 11, 12, 13, 14)]
    assert ds.SetGCPs(gcps, None) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is None

    ds = gdal.Open(filename)
    got_gcps = ds.GetGCPs()
    assert len(got_gcps) == len(gcps)
    assert got_gcps[0].GCPPixel == gcps[0].GCPPixel
    assert got_gcps[0].GCPLine == gcps[0].GCPLine
    assert got_gcps[0].GCPX == gcps[0].GCPX
    assert got_gcps[0].GCPY == gcps[0].GCPY
    assert ds.GetGCPSpatialRef() is None
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetNoDataValue() and DeleteNoDataValue() on a read-only dataset


def test_tiff_write_nodata_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).SetNoDataValue(123) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == 123
    assert ds.GetRasterBand(1).DeleteNoDataValue() == gdal.CE_None
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetNoDataValue() on a read-only dataset, overriding TIFF tags


def test_tiff_write_nodata_read_only_overriding_tifftags():

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    assert ds.GetRasterBand(1).SetNoDataValue(0) == gdal.CE_None
    ds = None


    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).SetNoDataValue(123) == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == 123
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    assert ds.GetRasterBand(1).SetNoDataValue(1) == gdal.CE_None
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == 1
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test Dataset SetMetadataItem() on a read-only dataset


def test_tiff_write_dataset_setmetadataitem_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    assert ds.SetMetadataItem('FOO', 'BAR', 'BAZ') == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('FOO', 'BAZ') == 'BAR'
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test Dataset SetMetadata() on a read-only dataset


def test_tiff_write_dataset_setmetadata_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    assert ds.SetMetadata({'FOO': 'BAR'}, 'BAZ') == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('FOO', 'BAZ') == 'BAR'
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test Band SetMetadataItem() on a read-only dataset


def test_tiff_write_band_setmetadataitem_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).SetMetadataItem('FOO', 'BAR', 'BAZ') == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetMetadataItem('FOO', 'BAZ') == 'BAR'
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test Band SetMetadata() on a read-only dataset


def test_tiff_write_band_setmetadata_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).SetMetadata({'FOO': 'BAR'}, 'BAZ') == gdal.CE_None
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetMetadataItem('FOO', 'BAZ') == 'BAR'
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test SetColorTable() on a read-only dataset


def test_tiff_write_setcolortable_read_only():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 1, 1)

    ds = gdal.Open(filename)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (1, 2, 3, 255))
    assert ds.GetRasterBand(1).SetRasterColorTable(ct) == gdal.CE_None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert ct is not None
    assert ct.GetColorEntry(0) == (1, 2, 3, 255)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)


###############################################################################
# Test SetColorTable() on a read-only dataset, overriding TIFF tags


def test_tiff_write_setcolortable_read_only_overriding_tifftags():

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (1, 2, 3, 255))
    assert ds.GetRasterBand(1).SetRasterColorTable(ct) == gdal.CE_None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ds = None

    ds = gdal.Open(filename)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (4, 5, 6, 255))
    assert ds.GetRasterBand(1).SetRasterColorTable(ct) == gdal.CE_None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is not None

    ds = gdal.Open(filename)
    assert ct is not None
    assert ct.GetColorEntry(0) == (4, 5, 6, 255)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (7, 8, 9, 255))
    assert ds.GetRasterBand(1).SetRasterColorTable(ct) == gdal.CE_None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ds = None

    assert gdal.VSIStatL(filename + '.aux.xml') is None

    ds = gdal.Open(filename)
    assert ct is not None
    assert ct.GetColorEntry(0) == (7, 8, 9, 255)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)


###############################################################################
# Test setting incompatible settings for PREDICTOR


@pytest.mark.parametrize("dt, options",
                         [(gdal.GDT_UInt16, ['PREDICTOR=2', 'NBITS=12']),
                          (gdal.GDT_UInt32, ['PREDICTOR=3']),
                          (gdal.GDT_UInt16, ['PREDICTOR=invalid'])
                          ])
def test_tiff_write_incompatible_predictor(dt, options):

    filename = '/vsimem/out.tif'
    with gdaltest.error_handler():
        assert gdal.GetDriverByName('GTiff').Create(filename, 1, 1, 1, dt, options + ['COMPRESS=LZW']) is None


###############################################################################
# Test PREDICTOR=2 with 64 bit samples

def test_tiff_write_predictor_2_float64():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        pytest.skip('libtiff > 4.3.0 or internal libtiff needed')

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 2, 1, 1, gdal.GDT_Float64, ['COMPRESS=LZW', 'PREDICTOR=2'])
    data = struct.pack('d' * 2, 1, 2)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, data)
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('PREDICTOR', 'IMAGE_STRUCTURE') == '2'
    assert ds.ReadRaster() == data
    ds = None
    gdal.Unlink(filename)

###############################################################################


def test_tiff_write_uint64():

    ut = gdaltest.GDALTest('GTiff', 'gtiff/uint64_full_range.tif', 1, 1)
    return ut.testCreateCopy()


###############################################################################


def test_tiff_write_uint64_nodata():

    filename = '/vsimem/test_tiff_write_uint64_nodata.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1, 1, gdal.GDT_UInt64)
    val = (1 << 64)-1
    assert ds.GetRasterBand(1).SetNoDataValue(val) == gdal.CE_None
    ds = None

    filename_copy = '/vsimem/test_tiff_write_uint64_nodata_filename_copy.tif'
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = gdal.GetDriverByName('GTiff').CreateCopy(filename_copy, ds)
    ds = None

    ds = gdal.Open(filename_copy)
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.GetDriverByName('GTiff').Delete(filename_copy)


###############################################################################


def test_tiff_write_int64():

    ut = gdaltest.GDALTest('GTiff', 'gtiff/int64_full_range.tif', 1, 65535)
    return ut.testCreateCopy()


###############################################################################


def test_tiff_write_int64_nodata():

    filename = '/vsimem/test_tiff_write_int64_nodata.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1, 1, gdal.GDT_Int64)
    val = -(1 << 63)
    assert ds.GetRasterBand(1).SetNoDataValue(val) == gdal.CE_None
    ds = None

    filename_copy = '/vsimem/test_tiff_write_int64_nodata_filename_copy.tif'
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = gdal.GetDriverByName('GTiff').CreateCopy(filename_copy, ds)
    ds = None

    ds = gdal.Open(filename_copy)
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.GetDriverByName('GTiff').Delete(filename_copy)


###############################################################################
# Check IsMaskBand() on an alpha band


def test_tiff_write_alpha_ismaskband():

    filename = '/vsimem/out.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1, 2)
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    assert not ds.GetRasterBand(1).IsMaskBand()
    assert ds.GetRasterBand(2).IsMaskBand()
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)



###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/5580


def test_tiff_write_overview_building_and_approx_stats():

    filename = '/vsimem/out.tif'
    gdal.GetDriverByName('GTiff').Create(filename, 512, 512)
    ds = gdal.Open(filename)
    ds.BuildOverviews("NEAREST", [2, 4, 8])
    ds.GetRasterBand(1).ComputeStatistics(1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete(filename)



def test_tiff_write_cleanup():
    gdaltest.tiff_drv = None
