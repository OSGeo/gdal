#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for NITF driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import copy
import os
import array
import struct
import shutil
import base64
from osgeo import gdal
from osgeo import ogr
from osgeo import osr


import gdaltest
import pytest

@pytest.fixture(scope='module')
def not_jpeg_9b():
    import jpeg
    jpeg.test_jpeg_1()
    if gdaltest.jpeg_version == '9b':
        pytest.skip()

def hex_string(s):
    return "".join(hex(ord(c))[2:] for c in s)

###############################################################################
# Write/Read test of simple byte reference data.

def test_nitf_1():

    tst = gdaltest.GDALTest('NITF', 'byte.tif', 1, 4672)
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple 16bit reference data.


def test_nitf_2():

    tst = gdaltest.GDALTest('NITF', 'int16.tif', 1, 4672)
    return tst.testCreateCopy()

###############################################################################
# Write/Read RGB image with lat/long georeferencing, and verify.


def test_nitf_3():

    tst = gdaltest.GDALTest('NITF', 'rgbsmall.tif', 3, 21349)
    return tst.testCreateCopy()


###############################################################################
# Test direction creation of an NITF file.

def nitf_create(creation_options, set_inverted_color_interp=True, createcopy=False):

    drv = gdal.GetDriverByName('NITF')

    try:
        os.remove('tmp/test_create.ntf')
    except OSError:
        pass

    if createcopy:
        ds = gdal.GetDriverByName('MEM').Create('', 200, 100, 3, gdal.GDT_Byte)
    else:
        ds = drv.Create('tmp/test_create.ntf', 200, 100, 3, gdal.GDT_Byte,
                        creation_options)
    ds.SetGeoTransform((100, 0.1, 0.0, 30.0, 0.0, -0.1))

    if set_inverted_color_interp:
        ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_BlueBand)
        ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
        ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_RedBand)
    else:
        ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
        ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
        ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)

    my_list = list(range(200)) + list(range(20, 220)) + list(range(30, 230))
    try:
        raw_data = array.array('h', my_list).tobytes()
    except:
        # Python 2
        raw_data = array.array('h', my_list).tostring()

    for line in range(100):
        ds.WriteRaster(0, line, 200, 1, raw_data,
                       buf_type=gdal.GDT_Int16,
                       band_list=[1, 2, 3])

    if createcopy:
        ds = drv.CreateCopy('tmp/test_create.ntf', ds,
                            options=creation_options)

    ds = None

###############################################################################
# Test direction creation of an non-compressed NITF file.


def test_nitf_4():

    return nitf_create(['ICORDS=G'])


###############################################################################
# Verify created file

def nitf_check_created_file(checksum1, checksum2, checksum3, set_inverted_color_interp=True):
    ds = gdal.Open('tmp/test_create.ntf')

    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = checksum1
    assert chksum == chksum_expect, 'Did not get expected chksum for band 1'

    chksum = ds.GetRasterBand(2).Checksum()
    chksum_expect = checksum2
    assert chksum == chksum_expect, 'Did not get expected chksum for band 2'

    chksum = ds.GetRasterBand(3).Checksum()
    chksum_expect = checksum3
    assert chksum == chksum_expect, 'Did not get expected chksum for band 3'

    geotransform = ds.GetGeoTransform()
    assert geotransform[0] == pytest.approx(100, abs=0.1) and geotransform[1] == pytest.approx(0.1, abs=0.001) and geotransform[2] == pytest.approx(0, abs=0.001) and geotransform[3] == pytest.approx(30.0, abs=0.1) and geotransform[4] == pytest.approx(0, abs=0.001) and geotransform[5] == pytest.approx(-0.1, abs=0.001), \
        'geotransform differs from expected'

    if set_inverted_color_interp:
        assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_BlueBand, \
            'Got wrong color interpretation.'

        assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand, \
            'Got wrong color interpretation.'

        assert ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_RedBand, \
            'Got wrong color interpretation.'

    ds = None

###############################################################################
# Verify file created by nitf_4()


def test_nitf_5():

    return nitf_check_created_file(32498, 42602, 38982)

###############################################################################
# Read existing NITF file.  Verifies the new adjusted IGEOLO interp.


def test_nitf_6():

    tst = gdaltest.GDALTest('NITF', 'nitf/rgb.ntf', 3, 21349)
    return tst.testOpen(check_prj='WGS84',
                        check_gt=(-44.842029478458, 0.003503401360, 0,
                                  -22.930748299319, 0, -0.003503401360))

###############################################################################
# NITF in-memory.


def test_nitf_7():

    tst = gdaltest.GDALTest('NITF', 'rgbsmall.tif', 3, 21349)
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Verify we can open an NSIF file, and get metadata including BLOCKA.


def test_nitf_8():

    ds = gdal.Open('data/nitf/fake_nsif.ntf')

    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 12033
    assert chksum == chksum_expect, 'Did not get expected chksum for band 1'

    md = ds.GetMetadata()
    assert md['NITF_FHDR'] == 'NSIF01.00', 'Got wrong FHDR value'

    assert md['NITF_BLOCKA_BLOCK_INSTANCE_01'] == '01' and md['NITF_BLOCKA_BLOCK_COUNT'] == '01' and md['NITF_BLOCKA_N_GRAY_01'] == '00000' and md['NITF_BLOCKA_L_LINES_01'] == '01000' and md['NITF_BLOCKA_LAYOVER_ANGLE_01'] == '000' and md['NITF_BLOCKA_SHADOW_ANGLE_01'] == '000' and md['NITF_BLOCKA_FRLC_LOC_01'] == '+41.319331+020.078400' and md['NITF_BLOCKA_LRLC_LOC_01'] == '+41.317083+020.126072' and md['NITF_BLOCKA_LRFC_LOC_01'] == '+41.281634+020.122570' and md['NITF_BLOCKA_FRFC_LOC_01'] == '+41.283881+020.074924', \
        'BLOCKA metadata has unexpected value.'

###############################################################################
# Create and read a JPEG encoded NITF file.


def test_nitf_9():

    src_ds = gdal.Open('data/rgbsmall.tif')
    ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf9.ntf', src_ds,
                                                 options=['IC=C3'])
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/nitf9.ntf')

    (exp_mean, exp_stddev) = (65.9532, 46.9026375565)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    assert exp_mean == pytest.approx(mean, abs=0.1) and exp_stddev == pytest.approx(stddev, abs=0.1), \
        'did not get expected mean or standard dev.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['COMPRESSION'] == 'JPEG', 'Did not get expected compression value.'

###############################################################################
# For esoteric reasons, createcopy from jpeg compressed nitf files can be
# tricky.  Verify this is working.


def test_nitf_10():

    src_ds = gdal.Open('tmp/nitf9.ntf')
    expected_cs = src_ds.GetRasterBand(2).Checksum()
    src_ds = None
    assert expected_cs in (22296,
                           22259,
                           22415, # libjpeg 9e
                          )

    tst = gdaltest.GDALTest('NITF', '../tmp/nitf9.ntf', 2, expected_cs)
    return tst.testCreateCopy()

###############################################################################
# Test 1bit file ... conveniently very small and easy to include! (#1854)


def test_nitf_11():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/i_3034c.ntf
    tst = gdaltest.GDALTest('NITF', 'nitf/i_3034c.ntf', 1, 170)
    return tst.testOpen()

###############################################################################
# Verify that TRE and CGM access via the metadata domain works.


def test_nitf_12():

    ds = gdal.Open('data/nitf/fake_nsif.ntf')

    mdTRE = ds.GetMetadata('TRE')

    try:  # NG bindings
        blockA = ds.GetMetadataItem('BLOCKA', 'TRE')
    except:
        blockA = mdTRE['BLOCKA']

    mdCGM = ds.GetMetadata('CGM')

    try:  # NG bindings
        segmentCount = ds.GetMetadataItem('SEGMENT_COUNT', 'CGM')
    except:
        segmentCount = mdCGM['SEGMENT_COUNT']

    ds = None

    expectedBlockA = '010000001000000000                +41.319331+020.078400+41.317083+020.126072+41.281634+020.122570+41.283881+020.074924     '

    assert mdTRE['BLOCKA'] == expectedBlockA, \
        'did not find expected BLOCKA from metadata.'

    assert blockA == expectedBlockA, 'did not find expected BLOCKA from metadata item.'

    assert mdCGM['SEGMENT_COUNT'] == '0', \
        'did not find expected SEGMENT_COUNT from metadata.'

    assert segmentCount == '0', \
        'did not find expected SEGMENT_COUNT from metadata item.'


###############################################################################
# Test creation of an NITF file in UTM Zone 11, Southern Hemisphere.

def test_nitf_13():
    drv = gdal.GetDriverByName('NITF')
    gdal.ErrorReset()
    ds = drv.Create('tmp/test_13.ntf', 200, 100, 1, gdal.GDT_Byte,
                    ['ICORDS=S'])
    assert gdal.GetLastErrorMsg() == ''
    ds.SetGeoTransform((400000, 10, 0.0, 6000000, 0.0, -10))
    ds.SetProjection('PROJCS["UTM Zone 11, Southern Hemisphere",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000],UNIT["Meter",1]]')

    my_list = list(range(200))
    try:
        raw_data = array.array('f', my_list).tobytes()
    except:
        # Python 2
        raw_data = array.array('f', my_list).tostring()

    for line in range(100):
        ds.WriteRaster(0, line, 200, 1, raw_data,
                       buf_type=gdal.GDT_Int16,
                       band_list=[1])

    ds = None

###############################################################################
# Verify previous file


def test_nitf_14():
    ds = gdal.Open('tmp/test_13.ntf')

    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 55964
    assert chksum == chksum_expect, 'Did not get expected chksum for band 1'

    geotransform = ds.GetGeoTransform()
    assert geotransform[0] == pytest.approx(400000, abs=.1) and geotransform[1] == pytest.approx(10, abs=0.001) and geotransform[2] == pytest.approx(0, abs=0.001) and geotransform[3] == pytest.approx(6000000, abs=.1) and geotransform[4] == pytest.approx(0, abs=0.001) and geotransform[5] == pytest.approx(-10, abs=0.001), \
        'geotransform differs from expected'

    prj = ds.GetProjectionRef()
    assert prj.find('PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000]') != -1, \
        'Coordinate system not UTM Zone 11, Southern Hemisphere'

    ds = None


###############################################################################
# Test automatic setting of ICORDS=N/S for UTM WGS 84 projections


@pytest.mark.parametrize("epsg_code,icords", [(32631, 'N'), (32731, 'S')])
def test_nitf_create_copy_automatic_UTM_ICORDS(epsg_code,icords):
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 20)
    src_ds.SetGeoTransform([-1000, 1000, 0, 2000, 0, -500])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(epsg_code)
    src_ds.SetSpatialRef(srs)

    outfilename = '/vsimem/test_nitf_create_copy_automatic_UTM_ICORDS.ntf'
    gdal.ErrorReset()
    assert gdal.GetDriverByName('NITF').CreateCopy(outfilename, src_ds)
    assert gdal.VSIStatL(outfilename + '.aux.xml') is None
    ds = gdal.Open(outfilename)
    assert ds.GetMetadataItem('NITF_ICORDS') == icords
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
    ds = None
    gdal.GetDriverByName('NITF').Delete(outfilename)


###############################################################################
# Test creation of an NITF file with IGEOLO provided by the user, but not ICORDS
# -> error


def test_nitf_create_copy_user_provided_IGEOLO_without_ICORDS():
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 20)
    src_ds.SetGeoTransform([-1000, 1000, 0, 2000, 0, -500])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_ds.SetSpatialRef(srs)

    outfilename = '/vsimem/test_nitf_create_copy_user_provided_IGEOLO_without_ICORDS.ntf'
    gdal.ErrorReset()
    with gdaltest.error_handler():
        assert gdal.GetDriverByName('NITF').CreateCopy(outfilename, src_ds,
                                                options = ['IGEOLO=' + ('0' * 60)]) is None
    assert gdal.GetLastErrorMsg() != ''
    gdal.GetDriverByName('NITF').Delete(outfilename)


###############################################################################
# Test creation of an NITF file with ICORDS and IGEOLO provided by the user


def test_nitf_create_copy_user_provided_ICORDS_IGEOLO():
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 20)
    src_ds.SetGeoTransform([-1000, 1000, 0, 2000, 0, -500])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_ds.SetSpatialRef(srs)

    outfilename = '/vsimem/test_nitf_create_copy_user_provided_ICORDS_IGEOLO.ntf'
    gdal.ErrorReset()
    gdal.GetDriverByName('NITF').CreateCopy(outfilename, src_ds,
                                            options = ['ICORDS=G', 'IGEOLO=' + ('0' * 60)])
    assert gdal.GetLastErrorMsg() == ''
    assert gdal.VSIStatL(outfilename + '.aux.xml') is None
    ds = gdal.Open(outfilename)
    assert ds.GetMetadataItem('NITF_ICORDS') == 'G'
    assert ds.GetMetadataItem('NITF_IGEOLO') == '0' * 60
    ds = None

    gdal.GetDriverByName('NITF').Delete(outfilename)


###############################################################################
# Test automatic reprojection of corner coordinates to long/lat if the
# source CRS is UTM WGS84 and the user provided ICORDS=G


def test_nitf_create_copy_UTM_corner_reprojection_to_long_lat():
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 20)
    src_ds.SetGeoTransform([-1000, 1000, 0, 2000, 0, -500])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_ds.SetSpatialRef(srs)

    outfilename = '/vsimem/test_nitf_create_copy_UTM_corner_reprojection_to_long_lat.ntf'
    gdal.ErrorReset()
    gdal.GetDriverByName('NITF').CreateCopy(outfilename, src_ds,
                                            options = ['ICORDS=G'])
    assert gdal.GetLastErrorMsg() == ''
    assert gdal.VSIStatL(outfilename + '.aux.xml') is None
    ds = gdal.Open(outfilename)
    assert ds.GetMetadataItem('NITF_ICORDS') == 'G'
    assert ds.GetMetadataItem('NITF_IGEOLO') == '000057N0012936W000057N0012445W000412S0012445W000412S0012936W'
    ds = None

    gdal.GetDriverByName('NITF').Delete(outfilename)

###############################################################################
# Test creating an in memory copy.


def test_nitf_15():

    tst = gdaltest.GDALTest('NITF', 'byte.tif', 1, 4672)

    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Checks a 1-bit mono with mask table having (0x00) black as transparent with white arrow.


def test_nitf_16():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3034d.nsf
    tst = gdaltest.GDALTest('NITF', 'nitf/ns3034d.nsf', 1, 170)
    return tst.testOpen()


###############################################################################
# Checks a 1-bit RGB/LUT (green arrow) with a mask table (pad pixels having value of 0x00)
# and a transparent pixel value of 1 being mapped to green by the LUT

def test_nitf_17():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/i_3034f.ntf
    tst = gdaltest.GDALTest('NITF', 'nitf/i_3034f.ntf', 1, 170)
    return tst.testOpen()

###############################################################################
# Test NITF file without image segment


def test_nitf_18():

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv1_1/U_0006A.NTF
    ds = gdal.Open("data/nitf/U_0006A.NTF")
    gdal.PopErrorHandler()

    assert ds.RasterCount == 0

###############################################################################
# Test BILEVEL (C1) decompression


def test_nitf_19():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_0/U_1050A.NTF
    tst = gdaltest.GDALTest('NITF', 'nitf/U_1050A.NTF', 1, 65024)

    return tst.testOpen()


###############################################################################
# Test NITF file consisting only of an header

def test_nitf_20():

    # Shut up the warning about file either corrupt or empty
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv1_1/U_0002A.NTF
    ds = gdal.Open("data/nitf/U_0002A.NTF")
    gdal.PopErrorHandler()

    assert ds is None


###############################################################################
# Verify that TEXT access via the metadata domain works.
#
# See also nitf_35 for writing TEXT segments.

def test_nitf_21():

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/nitf/ns3114a.nsf')
    gdal.PopErrorHandler()

    mdTEXT = ds.GetMetadata('TEXT')

    try:  # NG bindings
        data0 = ds.GetMetadataItem('DATA_0', 'TEXT')
    except:
        data0 = mdTEXT['DATA_0']

    ds = None

    assert mdTEXT['DATA_0'] == 'A', 'did not find expected DATA_0 from metadata.'

    assert data0 == 'A', 'did not find expected DATA_0 from metadata item.'


###############################################################################
# Write/Read test of simple int32 reference data.

def test_nitf_22():

    tst = gdaltest.GDALTest('NITF', '../../gcore/data/int32.tif', 1, 4672)
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple float32 reference data.


def test_nitf_23():

    tst = gdaltest.GDALTest('NITF', '../../gcore/data/float32.tif', 1, 4672)
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple float64 reference data.


def test_nitf_24():

    tst = gdaltest.GDALTest('NITF', '../../gcore/data/float64.tif', 1, 4672)
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple uint16 reference data.


def test_nitf_25():

    tst = gdaltest.GDALTest('NITF', '../../gcore/data/uint16.tif', 1, 4672)
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple uint32 reference data.


def test_nitf_26():

    tst = gdaltest.GDALTest('NITF', '../../gcore/data/uint32.tif', 1, 4672)
    return tst.testCreateCopy()

###############################################################################
# Test Create() with IC=NC compression, and multi-blocks


def test_nitf_27():

    nitf_create(['ICORDS=G', 'IC=NC', 'BLOCKXSIZE=10', 'BLOCKYSIZE=10'])

    return nitf_check_created_file(32498, 42602, 38982)


###############################################################################
# Test Create() with IC=C8 compression with the JP2ECW driver

def test_nitf_28_jp2ecw():

    gdaltest.nitf_28_jp2ecw_is_ok = False
    if gdal.GetDriverByName('JP2ECW') is None:
        pytest.skip()

    import ecw
    if not ecw.has_write_support():
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2ECW')

    if nitf_create(['ICORDS=G', 'IC=C8', 'TARGET=75'], set_inverted_color_interp=False) == 'success':
        ret = nitf_check_created_file(32398, 42502, 38882, set_inverted_color_interp=False)
        if ret == 'success':
            gdaltest.nitf_28_jp2ecw_is_ok = True
    else:
        ret = 'fail'

    tmpfilename = '/vsimem/nitf_28_jp2ecw.ntf'
    src_ds = gdal.GetDriverByName('MEM').Create('', 1025, 1025)
    gdal.GetDriverByName('NITF').CreateCopy(tmpfilename, src_ds, options=['IC=C8'])
    ds = gdal.Open(tmpfilename)
    blockxsize, blockysize = ds.GetRasterBand(1).GetBlockSize()
    ds = None
    gdal.Unlink(tmpfilename)
    if (blockxsize, blockysize) != (256, 256):  # 256 since this is hardcoded as such in the ECW driver
        gdaltest.post_reason('wrong block size')
        print(blockxsize, blockysize)
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

###############################################################################
# Test reading the previously create file with the JP2MrSID driver


def test_nitf_28_jp2mrsid():
    if not gdaltest.nitf_28_jp2ecw_is_ok:
        pytest.skip()

    jp2mrsid_drv = gdal.GetDriverByName('JP2MrSID')
    if jp2mrsid_drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2MrSID')

    ret = nitf_check_created_file(32398, 42502, 38882, set_inverted_color_interp=False)

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


###############################################################################
# Test reading the previously create file with the JP2KAK driver

def test_nitf_28_jp2kak():
    if not gdaltest.nitf_28_jp2ecw_is_ok:
        pytest.skip()

    jp2kak_drv = gdal.GetDriverByName('JP2KAK')
    if jp2kak_drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2KAK')

    ret = nitf_check_created_file(32398, 42502, 38882, set_inverted_color_interp=False)

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

###############################################################################
# Test reading the previously create file with the JP2KAK driver


def test_nitf_28_jp2openjpeg():
    if not gdaltest.nitf_28_jp2ecw_is_ok:
        pytest.skip()

    drv = gdal.GetDriverByName('JP2OpenJPEG')
    if drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

    ret = nitf_check_created_file(32398, 42502, 38882, set_inverted_color_interp=False)

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

###############################################################################
# Test CreateCopy() with IC=C8 compression with the JP2OpenJPEG driver


def test_nitf_28_jp2openjpeg_bis():
    drv = gdal.GetDriverByName('JP2OpenJPEG')
    if drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

    if nitf_create(['ICORDS=G', 'IC=C8', 'QUALITY=25'], set_inverted_color_interp=False, createcopy=True) == 'success':
        ret = nitf_check_created_file(31604, 42782, 38791, set_inverted_color_interp=False)
    else:
        ret = 'fail'

    tmpfilename = '/vsimem/nitf_28_jp2openjpeg_bis.ntf'
    src_ds = gdal.GetDriverByName('MEM').Create('', 1025, 1025)
    gdal.GetDriverByName('NITF').CreateCopy(tmpfilename, src_ds, options=['IC=C8'])
    ds = gdal.Open(tmpfilename)
    blockxsize, blockysize = ds.GetRasterBand(1).GetBlockSize()
    ds = None
    gdal.Unlink(tmpfilename)
    if (blockxsize, blockysize) != (1024, 1024):
        gdaltest.post_reason('wrong block size')
        print(blockxsize, blockysize)
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


###############################################################################
# Test CreateCopy() with IC=C8 compression and NPJE profiles with the JP2OpenJPEG driver


def test_nitf_jp2openjpeg_npje_numerically_lossless():
    jp2openjpeg_drv = gdal.GetDriverByName('JP2OpenJPEG')
    if jp2openjpeg_drv is None:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/uint16.tif')
    # May throw a warning with openjpeg < 2.5
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').CreateCopy('/vsimem/tmp.ntf',
                                                src_ds,
                                                strict=False,
                                                options=['IC=C8',
                                                         'ABPP=12',
                                                         'JPEG2000_DRIVER=JP2OpenJPEG',
                                                         'PROFILE=NPJE_NUMERICALLY_LOSSLESS'])

    ds = gdal.Open('/vsimem/tmp.ntf')
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetMetadataItem('J2KLRA', 'TRE') == '0050000102000000.03125000100.06250000200.12500000300.25000000400.50000000500.60000000600.70000000700.80000000800.90000000901.00000001001.10000001101.20000001201.30000001301.50000001401.70000001502.00000001602.30000001703.50000001803.90000001912.000000'
    assert ds.GetMetadataItem('COMRAT', 'DEBUG') in ('N141', 'N142', 'N143', 'N169')

    # Get the JPEG2000 code stream subfile
    jpeg2000_ds_name = ds.GetMetadataItem('JPEG2000_DATASET_NAME', 'DEBUG')
    assert jpeg2000_ds_name
    structure = gdal.GetJPEG2000StructureAsString(jpeg2000_ds_name, ['ALL=YES'])
    assert structure is not None

    # Check that the structure of the JPEG2000 codestream is the one expected
    # from the NPJE profile
    # print(structure)
    assert '<Field name="Rsiz" type="uint16" description="Profile 1">2</Field>' in structure
    assert '<Field name="Ssiz0" type="uint8" description="Unsigned 16 bits">15</Field>' in structure
    assert '<Field name="XTsiz" type="uint32">1024</Field>' in structure
    assert '<Field name="YTsiz" type="uint32">1024</Field>' in structure
    assert '<Field name="Scod" type="uint8" description="Standard precincts, No SOP marker segments, No EPH marker segments">0</Field>' in structure
    assert '<Field name="SGcod_NumLayers" type="uint16">20</Field>' in structure
    assert '<Field name="SGcod_MCT" type="uint8">0</Field>' in structure
    assert '<Field name="SPcod_NumDecompositions" type="uint8">5</Field>' in structure
    assert '<Field name="SPcod_xcb_minus_2" type="uint8" description="64">4</Field>' in structure
    assert '<Field name="SPcod_ycb_minus_2" type="uint8" description="64">4</Field>' in structure
    assert '<Field name="SPcod_cbstyle" type="uint8" description="No selective arithmetic coding bypass, No reset of context probabilities on coding pass boundaries, No termination on each coding pass, No vertically causal context, No predictable termination, No segmentation symbols are used">0</Field>' in structure
    assert '<Field name="SPcod_transformation" type="uint8" description="5-3 reversible">1</Field>' in structure

    if 'TLM' in jp2openjpeg_drv.GetMetadataItem('DMD_CREATIONOPTIONLIST'):
        assert '<Marker name="TLM"' in structure
        assert '<Marker name="PLT"' in structure

    gdal.Unlink('/vsimem/tmp.ntf')


###############################################################################
# Test CreateCopy() with IC=C8 compression and NPJE profiles with the JP2OpenJPEG driver


def test_nitf_jp2openjpeg_npje_visually_lossless():
    jp2openjpeg_drv = gdal.GetDriverByName('JP2OpenJPEG')
    if jp2openjpeg_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    # May throw a warning with openjpeg < 2.5
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').CreateCopy('/vsimem/tmp.ntf',
                                                src_ds,
                                                strict=False,
                                                options=['IC=C8',
                                                         'JPEG2000_DRIVER=JP2OpenJPEG',
                                                         'PROFILE=NPJE_VISUALLY_LOSSLESS'])

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

    try:
        ds = gdal.Open('/vsimem/tmp.ntf')
        assert ds.GetRasterBand(1).Checksum() in (4595, 4606, 4633)
        assert ds.GetMetadataItem('J2KLRA', 'TRE') == '0050000101900000.03125000100.06250000200.12500000300.25000000400.50000000500.60000000600.70000000700.80000000800.90000000901.00000001001.10000001101.20000001201.30000001301.50000001401.70000001502.00000001602.30000001703.50000001803.900000'
    finally:
        gdaltest.reregister_all_jpeg2000_drivers()
    assert ds.GetMetadataItem('COMRAT', 'DEBUG').startswith('V')

    # Get the JPEG2000 code stream subfile
    jpeg2000_ds_name = ds.GetMetadataItem('JPEG2000_DATASET_NAME', 'DEBUG')
    assert jpeg2000_ds_name
    structure = gdal.GetJPEG2000StructureAsString(jpeg2000_ds_name, ['ALL=YES'])
    assert structure is not None

    # Check that the structure of the JPEG2000 codestream is the one expected
    # from the NPJE profile
    # print(structure)
    assert '<Field name="Rsiz" type="uint16" description="Profile 1">2</Field>' in structure
    assert '<Field name="Ssiz0" type="uint8" description="Unsigned 8 bits">7</Field>' in structure
    assert '<Field name="XTsiz" type="uint32">1024</Field>' in structure
    assert '<Field name="YTsiz" type="uint32">1024</Field>' in structure
    assert '<Field name="Scod" type="uint8" description="Standard precincts, No SOP marker segments, No EPH marker segments">0</Field>' in structure
    assert '<Field name="SGcod_NumLayers" type="uint16">19</Field>' in structure
    assert '<Field name="SGcod_MCT" type="uint8">0</Field>' in structure
    assert '<Field name="SPcod_NumDecompositions" type="uint8">5</Field>' in structure
    assert '<Field name="SPcod_xcb_minus_2" type="uint8" description="64">4</Field>' in structure
    assert '<Field name="SPcod_ycb_minus_2" type="uint8" description="64">4</Field>' in structure
    assert '<Field name="SPcod_cbstyle" type="uint8" description="No selective arithmetic coding bypass, No reset of context probabilities on coding pass boundaries, No termination on each coding pass, No vertically causal context, No predictable termination, No segmentation symbols are used">0</Field>' in structure
    assert '<Field name="SPcod_transformation" type="uint8" description="9-7 irreversible">0</Field>' in structure

    if 'TLM' in jp2openjpeg_drv.GetMetadataItem('DMD_CREATIONOPTIONLIST'):
        assert '<Marker name="TLM"' in structure
        assert '<Marker name="PLT"' in structure

    gdal.Unlink('/vsimem/tmp.ntf')


###############################################################################
# Test CreateCopy() with IC=C8 compression and NPJE profiles with the JP2OpenJPEG driver


def test_nitf_jp2openjpeg_npje_visually_lossless_with_quality():
    jp2openjpeg_drv = gdal.GetDriverByName('JP2OpenJPEG')
    if jp2openjpeg_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    # May throw a warning with openjpeg < 2.5
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').CreateCopy('/vsimem/tmp.ntf',
                                                src_ds,
                                                strict=False,
                                                options=['IC=C8',
                                                         'JPEG2000_DRIVER=JP2OpenJPEG',
                                                         'PROFILE=NPJE_VISUALLY_LOSSLESS',
                                                         'QUALITY=30'])

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

    try:
        ds = gdal.Open('/vsimem/tmp.ntf')
        assert ds.GetRasterBand(1).Checksum() in (4580, 4582, 4600)
        assert ds.GetMetadataItem('J2KLRA', 'TRE') == '0050000101800000.03125000100.06250000200.12500000300.25000000400.50000000500.60000000600.70000000700.80000000800.90000000901.00000001001.10000001101.20000001201.30000001301.50000001401.70000001502.00000001602.30000001702.400000'
    finally:
        gdaltest.reregister_all_jpeg2000_drivers()

    # Get the JPEG2000 code stream subfile
    jpeg2000_ds_name = ds.GetMetadataItem('JPEG2000_DATASET_NAME', 'DEBUG')
    assert jpeg2000_ds_name
    structure = gdal.GetJPEG2000StructureAsString(jpeg2000_ds_name, ['ALL=YES'])
    assert structure is not None

    # Check that the structure of the JPEG2000 codestream is the one expected
    # from the NPJE profile
    # print(structure)
    assert '<Field name="SGcod_NumLayers" type="uint16">18</Field>' in structure

    gdal.Unlink('/vsimem/tmp.ntf')

###############################################################################
# Test Create() with a LUT


def test_nitf_29():

    drv = gdal.GetDriverByName('NITF')

    ds = drv.Create('tmp/test_29.ntf', 1, 1, 1, gdal.GDT_Byte,
                    ['IREP=RGB/LUT', 'LUT_SIZE=128'])

    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))

    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ds = None

    ds = gdal.Open('tmp/test_29.ntf')

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert (ct.GetCount() == 129 and \
       ct.GetColorEntry(0) == (255, 255, 255, 255) and \
       ct.GetColorEntry(1) == (255, 255, 0, 255) and \
       ct.GetColorEntry(2) == (255, 0, 255, 255) and \
       ct.GetColorEntry(3) == (0, 255, 255, 255)), 'Wrong color table entry.'

    new_ds = drv.CreateCopy('tmp/test_29_copy.ntf', ds)
    del new_ds
    ds = None

    ds = gdal.Open('tmp/test_29_copy.ntf')

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert (ct.GetCount() == 130 and \
       ct.GetColorEntry(0) == (255, 255, 255, 255) and \
       ct.GetColorEntry(1) == (255, 255, 0, 255) and \
       ct.GetColorEntry(2) == (255, 0, 255, 255) and \
       ct.GetColorEntry(3) == (0, 255, 255, 255)), 'Wrong color table entry.'

    ds = None

###############################################################################
# Verify we can write a file with BLOCKA TRE and read it back properly.


def test_nitf_30():

    src_ds = gdal.Open('data/nitf/fake_nsif.ntf')
    ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf30.ntf', src_ds)

    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 12033
    assert chksum == chksum_expect, 'Did not get expected chksum for band 1'

    md = ds.GetMetadata()
    assert md['NITF_FHDR'] == 'NSIF01.00', 'Got wrong FHDR value'

    assert md['NITF_BLOCKA_BLOCK_INSTANCE_01'] == '01' and md['NITF_BLOCKA_BLOCK_COUNT'] == '01' and md['NITF_BLOCKA_N_GRAY_01'] == '00000' and md['NITF_BLOCKA_L_LINES_01'] == '01000' and md['NITF_BLOCKA_LAYOVER_ANGLE_01'] == '000' and md['NITF_BLOCKA_SHADOW_ANGLE_01'] == '000' and md['NITF_BLOCKA_FRLC_LOC_01'] == '+41.319331+020.078400' and md['NITF_BLOCKA_LRLC_LOC_01'] == '+41.317083+020.126072' and md['NITF_BLOCKA_LRFC_LOC_01'] == '+41.281634+020.122570' and md['NITF_BLOCKA_FRFC_LOC_01'] == '+41.283881+020.074924', \
        'BLOCKA metadata has unexpected value.'

    ds = None

    gdal.GetDriverByName('NITF').Delete('tmp/nitf30.ntf')

    # Test overriding src BLOCKA metadata with NITF_BLOCKA creation options
    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf30_override.ntf', src_ds,
                                            options=['BLOCKA_BLOCK_INSTANCE_01=01',
                                                     'BLOCKA_BLOCK_COUNT=01',
                                                     'BLOCKA_N_GRAY_01=00000',
                                                     'BLOCKA_L_LINES_01=01000',
                                                     'BLOCKA_LAYOVER_ANGLE_01=000',
                                                     'BLOCKA_SHADOW_ANGLE_01=000',
                                                     'BLOCKA_FRLC_LOC_01=+42.319331+020.078400',
                                                     'BLOCKA_LRLC_LOC_01=+42.317083+020.126072',
                                                     'BLOCKA_LRFC_LOC_01=+42.281634+020.122570',
                                                     'BLOCKA_FRFC_LOC_01=+42.283881+020.074924'
                                                    ])
    ds = gdal.Open('/vsimem/nitf30_override.ntf')
    md = ds.GetMetadata()
    ds = None
    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf30_override.ntf')

    assert md['NITF_BLOCKA_BLOCK_INSTANCE_01'] == '01' and md['NITF_BLOCKA_BLOCK_COUNT'] == '01' and md['NITF_BLOCKA_N_GRAY_01'] == '00000' and md['NITF_BLOCKA_L_LINES_01'] == '01000' and md['NITF_BLOCKA_LAYOVER_ANGLE_01'] == '000' and md['NITF_BLOCKA_SHADOW_ANGLE_01'] == '000' and md['NITF_BLOCKA_FRLC_LOC_01'] == '+42.319331+020.078400' and md['NITF_BLOCKA_LRLC_LOC_01'] == '+42.317083+020.126072' and md['NITF_BLOCKA_LRFC_LOC_01'] == '+42.281634+020.122570' and md['NITF_BLOCKA_FRFC_LOC_01'] == '+42.283881+020.074924', \
        'BLOCKA metadata has unexpected value.'

    # Test overriding src BLOCKA metadata with TRE=BLOCKA= creation option
    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf30_override.ntf', src_ds,
                                            options=['TRE=BLOCKA=010000001000000000                +42.319331+020.078400+42.317083+020.126072+42.281634+020.122570+42.283881+020.074924xxxxx'
                                                     ])
    ds = gdal.Open('/vsimem/nitf30_override.ntf')
    md = ds.GetMetadata()
    ds = None
    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf30_override.ntf')

    assert md['NITF_BLOCKA_BLOCK_INSTANCE_01'] == '01' and md['NITF_BLOCKA_BLOCK_COUNT'] == '01' and md['NITF_BLOCKA_N_GRAY_01'] == '00000' and md['NITF_BLOCKA_L_LINES_01'] == '01000' and md['NITF_BLOCKA_LAYOVER_ANGLE_01'] == '000' and md['NITF_BLOCKA_SHADOW_ANGLE_01'] == '000' and md['NITF_BLOCKA_FRLC_LOC_01'] == '+42.319331+020.078400' and md['NITF_BLOCKA_LRLC_LOC_01'] == '+42.317083+020.126072' and md['NITF_BLOCKA_LRFC_LOC_01'] == '+42.281634+020.122570' and md['NITF_BLOCKA_FRFC_LOC_01'] == '+42.283881+020.074924', \
        'BLOCKA metadata has unexpected value.'

    # Test that gdal_translate -ullr doesn't propagate BLOCKA
    gdal.Translate('/vsimem/nitf30_no_src_md.ntf', src_ds, format='NITF', outputBounds=[2, 49, 3, 50])
    ds = gdal.Open('/vsimem/nitf30_no_src_md.ntf')
    md = ds.GetMetadata()
    ds = None
    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf30_no_src_md.ntf')
    assert 'NITF_BLOCKA_BLOCK_INSTANCE_01' not in md, \
        'unexpectdly found BLOCKA metadata.'

    # Test USE_SRC_NITF_METADATA=NO
    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf30_no_src_md.ntf', src_ds,
                                            options=['USE_SRC_NITF_METADATA=NO'])
    ds = gdal.Open('/vsimem/nitf30_no_src_md.ntf')
    md = ds.GetMetadata()
    ds = None
    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf30_no_src_md.ntf')
    assert 'NITF_BLOCKA_BLOCK_INSTANCE_01' not in md, \
        'unexpectdly found BLOCKA metadata.'

###############################################################################
# Verify we can write a file with a custom TRE and read it back properly.


def test_nitf_31():

    nitf_create(['TRE=CUSTOM= Test TRE1\\0MORE',
                    'TRE=TOTEST=SecondTRE',
                    'ICORDS=G'])

    ds = gdal.Open('tmp/test_create.ntf')

    md = ds.GetMetadata('TRE')
    assert len(md) == 2, 'Did not get expected TRE count'

    # Check that the leading space in the CUSTOM metadata item is preserved (#3088, #3204)
    try:
        assert ds.GetMetadataItem('CUSTOM', 'TRE') == ' Test TRE1\\0MORE', \
            'Did not get expected TRE contents'
    except:
        pass

    assert md['CUSTOM'] == ' Test TRE1\\0MORE' and md['TOTEST'] == 'SecondTRE', \
        'Did not get expected TRE contents'

    ds = None
    return nitf_check_created_file(32498, 42602, 38982)


###############################################################################
# Test Create() with ICORDS=D

def test_nitf_32():

    nitf_create(['ICORDS=D'])

    return nitf_check_created_file(32498, 42602, 38982)


###############################################################################
# Test Create() with ICORDS=D and a consistent BLOCKA

def test_nitf_33():

    nitf_create(['ICORDS=D',
                    'BLOCKA_BLOCK_COUNT=01',
                    'BLOCKA_BLOCK_INSTANCE_01=01',
                    'BLOCKA_L_LINES_01=100',
                    'BLOCKA_FRLC_LOC_01=+29.950000+119.950000',
                    'BLOCKA_LRLC_LOC_01=+20.050000+119.950000',
                    'BLOCKA_LRFC_LOC_01=+20.050000+100.050000',
                    'BLOCKA_FRFC_LOC_01=+29.950000+100.050000'])

    return nitf_check_created_file(32498, 42602, 38982)


###############################################################################
# Test CreateCopy() of a 16bit image with tiling

def test_nitf_34():

    tst = gdaltest.GDALTest('NITF', 'n43.dt0', 1, 49187, options=['BLOCKSIZE=64'])
    return tst.testCreateCopy()

###############################################################################
# Test CreateCopy() writing file with a text segment.


def test_nitf_35():

    src_ds = gdal.Open('data/nitf/text_md.vrt')
    ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf_35.ntf', src_ds)
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/nitf_35.ntf')

    exp_text = """This is text data
with a newline."""

    md = ds.GetMetadata('TEXT')
    assert md['DATA_0'] == exp_text, 'Did not get expected TEXT metadata.'

    exp_text = """Also, a second text segment is created."""

    md = ds.GetMetadata('TEXT')
    assert md['DATA_1'] == exp_text, 'Did not get expected TEXT metadata.'

    ds = None

    gdal.GetDriverByName('NITF').Delete('tmp/nitf_35.ntf')

###############################################################################
# Create and read a JPEG encoded NITF file (C3) with several blocks
# Check that statistics are persisted (#3985)


def test_nitf_36():

    src_ds = gdal.Open('data/rgbsmall.tif')
    ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf36.ntf', src_ds,
                                                 options=['IC=C3', 'BLOCKSIZE=32', 'QUALITY=100'])
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/nitf36.ntf')

    assert ds.GetRasterBand(1).GetMinimum() is None, \
        'Did not expect to have minimum value at that point.'

    (_, _, mean, stddev) = ds.GetRasterBand(1).GetStatistics(False, False)
    assert stddev < 0, 'Did not expect to have statistics at that point.'

    (exp_mean, exp_stddev) = (65.4208, 47.254550335)
    (_, _, mean, stddev) = ds.GetRasterBand(1).GetStatistics(False, True)

    assert exp_mean == pytest.approx(mean, abs=0.1) and exp_stddev == pytest.approx(stddev, abs=0.1), \
        'did not get expected mean or standard dev.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['COMPRESSION'] == 'JPEG', 'Did not get expected compression value.'

    ds = None

    # Check that statistics are persisted (#3985)
    ds = gdal.Open('tmp/nitf36.ntf')

    assert ds.GetRasterBand(1).GetMinimum() is not None, \
        'Should have minimum value at that point.'

    (_, _, mean, stddev) = ds.GetRasterBand(1).GetStatistics(False, False)
    assert exp_mean == pytest.approx(mean, abs=0.1) and exp_stddev == pytest.approx(stddev, abs=0.1), \
        'Should have statistics at that point.'

    ds = None

###############################################################################
# Create and read a NITF file with 69999 bands


def test_nitf_37():

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf37.ntf', 1, 1, 69999)
    ds = None

    ds = gdal.Open('tmp/nitf37.ntf')
    assert ds.RasterCount == 69999
    ds = None

###############################################################################
# Create and read a NITF file with 999 images


def test_nitf_38():

    ds = gdal.Open('data/byte.tif')
    nXSize = ds.RasterXSize
    nYSize = ds.RasterYSize
    data = ds.GetRasterBand(1).ReadRaster(0, 0, nXSize, nYSize)
    expected_cs = ds.GetRasterBand(1).Checksum()

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf38.ntf', nXSize, nYSize, 1, options=['NUMI=999', 'WRITE_ALL_IMAGES=YES'])
    ds = None

    ds = gdal.Open('NITF_IM:998:tmp/nitf38.ntf', gdal.GA_Update)
    ds.GetRasterBand(1).WriteRaster(0, 0, nXSize, nYSize, data)

    # Create overviews
    ds.BuildOverviews(overviewlist=[2])

    ds = None

    ds = gdal.Open('NITF_IM:0:tmp/nitf38.ntf')
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None

    ds = gdal.Open('NITF_IM:998:tmp/nitf38.ntf')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs, 'bad checksum for image of 998th subdataset'

    # Check the overview
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1087, 'bad checksum for overview of image of 998th subdataset'

    out_ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/nitf38.vrt', ds)
    out_ds = None
    ds = None

    ds = gdal.Open('tmp/nitf38.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/nitf38.vrt')
    assert cs == expected_cs

    ds = gdal.Open('NITF_IM:998:%s/tmp/nitf38.ntf' % os.getcwd())
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('%s/tmp/nitf38.vrt' % os.getcwd(), ds)
    out_ds = None
    ds = None

    ds = gdal.Open('tmp/nitf38.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/nitf38.vrt')
    assert cs == expected_cs

    ds = gdal.Open('NITF_IM:998:%s/tmp/nitf38.ntf' % os.getcwd())
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/nitf38.vrt', ds)
    del out_ds
    ds = None

    ds = gdal.Open('tmp/nitf38.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/nitf38.vrt')
    assert cs == expected_cs

###############################################################################
# Create and read a JPEG encoded NITF file (M3) with several blocks


def test_nitf_39():

    src_ds = gdal.Open('data/rgbsmall.tif')
    ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf39.ntf', src_ds,
                                                 options=['IC=M3', 'BLOCKSIZE=32', 'QUALITY=100'])
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/nitf39.ntf')

    (exp_mean, exp_stddev) = (65.4208, 47.254550335)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    assert exp_mean == pytest.approx(mean, abs=0.1) and exp_stddev == pytest.approx(stddev, abs=0.1), \
        'did not get expected mean or standard dev.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['COMPRESSION'] == 'JPEG', 'Did not get expected compression value.'

    ds = None

###############################################################################
# Create a 10 GB NITF file


def test_nitf_40():

    # Determine if the filesystem supports sparse files (we don't want to create a real 10 GB
    # file !
    if not gdaltest.filesystem_supports_sparse_files('tmp'):
        pytest.skip()

    width = 99000
    height = 99000
    x = width - 1
    y = height - 1

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf40.ntf', width, height, options=['BLOCKSIZE=256'])
    data = struct.pack('B' * 1, 123)

    # Write a non NULL byte at the bottom right corner of the image (around 10 GB offset)
    ds.GetRasterBand(1).WriteRaster(x, y, 1, 1, data)
    ds = None

    # Check that we can fetch it at the right value
    ds = gdal.Open('tmp/nitf40.ntf')
    assert ds.GetRasterBand(1).ReadRaster(x, y, 1, 1) == data
    ds = None

    # Check that it is indeed at a very far offset, and that the NITF driver
    # has not put it somewhere else due to involuntary cast to 32bit integer.
    blockWidth = 256
    blockHeight = 256
    nBlockx = int((width + blockWidth - 1) / blockWidth)
    iBlockx = int(x / blockWidth)
    iBlocky = int(y / blockHeight)
    ix = x % blockWidth
    iy = y % blockHeight
    offset = 843 + (iBlocky * nBlockx + iBlockx) * blockWidth * blockHeight + (iy * blockWidth + ix)

    try:
        os.SEEK_SET
    except AttributeError:
        os.SEEK_SET, os.SEEK_CUR, os.SEEK_END = list(range(3))

    fd = open('tmp/nitf40.ntf', 'rb')
    fd.seek(offset, os.SEEK_SET)
    bytes_read = fd.read(1)
    fd.close()

    val = struct.unpack('B' * 1, bytes_read)[0]
    assert val == 123, ('Bad value at offset %d : %d' % (offset, val))


###############################################################################
# Check reading a 12-bit JPEG compressed NITF

def test_nitf_41(not_jpeg_9b):
    # Check if JPEG driver supports 12bit JPEG reading/writing
    jpg_drv = gdal.GetDriverByName('JPEG')
    md = jpg_drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        pytest.skip('12bit jpeg not available')

    gdal.Unlink('data/nitf/U_4017A.NTF.aux.xml')

    ds = gdal.Open('data/nitf/U_4017A.NTF')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[2] >= 2385 and stats[2] <= 2386
    ds = None

    gdal.Unlink('data/nitf/U_4017A.NTF.aux.xml')


###############################################################################
# Check creating a 12-bit JPEG compressed NITF


def test_nitf_42(not_jpeg_9b):
    # Check if JPEG driver supports 12bit JPEG reading/writing
    jpg_drv = gdal.GetDriverByName('JPEG')
    md = jpg_drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        pytest.skip('12bit jpeg not available')

    ds = gdal.Open('data/nitf/U_4017A.NTF')
    out_ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf42.ntf', ds, options=['IC=C3', 'FHDR=NITF02.10'])
    del out_ds

    ds = gdal.Open('tmp/nitf42.ntf')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[2] >= 2385 and stats[2] <= 2386
    ds = None

###############################################################################
# Test CreateCopy() in IC=C8 with various JPEG2000 drivers


def nitf_43(driver_to_test, options):

    jp2_drv = gdal.GetDriverByName(driver_to_test)
    if driver_to_test == 'JP2ECW' and jp2_drv is not None:
        if 'DMD_CREATIONOPTIONLIST' not in jp2_drv.GetMetadata():
            jp2_drv = None

    if jp2_drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('data/byte.tif')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf_43.ntf', ds, options=options, strict=0)
    gdal.PopErrorHandler()
    out_ds = None
    out_ds = gdal.Open('tmp/nitf_43.ntf')
    if out_ds.GetRasterBand(1).Checksum() == 4672:
        ret = 'success'
    else:
        ret = 'fail'
    out_ds = None

    if open('tmp/nitf_43.ntf', 'rb').read().decode('LATIN1').find('<gml') >= 0:
        print('GMLJP2 detected !')
        ret = 'fail'

    gdal.GetDriverByName('NITF').Delete('tmp/nitf_43.ntf')

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


def test_nitf_43_jasper():
    return nitf_43('JPEG2000', ['IC=C8'])


def test_nitf_43_jp2ecw():
    import ecw
    if not ecw.has_write_support():
        pytest.skip()
    return nitf_43('JP2ECW', ['IC=C8', 'TARGET=0'])


def test_nitf_43_jp2kak():
    return nitf_43('JP2KAK', ['IC=C8', 'QUALITY=100'])

###############################################################################
# Check creating a monoblock 10000x1 image (ticket #3263)


def test_nitf_44():

    out_ds = gdal.GetDriverByName('NITF').Create('tmp/nitf44.ntf', 10000, 1)
    out_ds.GetRasterBand(1).Fill(255)
    out_ds = None

    ds = gdal.Open('tmp/nitf44.ntf')

    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, _) = ds.GetRasterBand(1).GetBlockSize()
        assert blockx == 10000

    assert ds.GetRasterBand(1).Checksum() == 57182
    ds = None

###############################################################################
# Check overviews on a JPEG compressed subdataset


def test_nitf_45():

    try:
        os.remove('tmp/nitf45.ntf.aux.xml')
    except OSError:
        pass

    shutil.copyfile('data/nitf/two_images_jpeg.ntf', 'tmp/nitf45.ntf')

    ds = gdal.Open('NITF_IM:1:tmp/nitf45.ntf', gdal.GA_Update)
    ds.BuildOverviews(overviewlist=[2])
    # FIXME ? ds.GetRasterBand(1).GetOverview(0) is None until we reopen
    ds = None

    ds = gdal.Open('NITF_IM:1:tmp/nitf45.ntf')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1086, 'did not get expected checksum for overview of subdataset'

    ds = None

###############################################################################
# Check overviews on a JPEG2000 compressed subdataset


def nitf_46(driver_to_test):

    jp2_drv = gdal.GetDriverByName(driver_to_test)
    if jp2_drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    try:
        os.remove('tmp/nitf46.ntf.aux.xml')
    except OSError:
        pass

    try:
        os.remove('tmp/nitf46.ntf_0.ovr')
    except OSError:
        pass

    shutil.copyfile('data/nitf/two_images_jp2.ntf', 'tmp/nitf46.ntf')

    ds = gdal.Open('NITF_IM:1:tmp/nitf46.ntf', gdal.GA_Update)
    ds.BuildOverviews(overviewlist=[2])
    # FIXME ? ds.GetRasterBand(1).GetOverview(0) is None until we reopen
    ds = None

    ds = gdal.Open('NITF_IM:1:tmp/nitf46.ntf')
    if ds.GetRasterBand(1).GetOverview(0) is None:
        gdaltest.post_reason('no overview of subdataset')
        ret = 'fail'
    else:
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        if cs != 1086:
            print(cs)
            gdaltest.post_reason('did not get expected checksum for overview of subdataset')
            ret = 'fail'
        else:
            ret = 'success'

    ds = None

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


def nitf_46_jp2ecw():
    return nitf_46('JP2ECW')


def nitf_46_jp2mrsid():
    return nitf_46('JP2MrSID')


def nitf_46_jp2kak():
    return nitf_46('JP2KAK')


def test_nitf_46_jasper():
    return nitf_46('JPEG2000')


def nitf_46_openjpeg():
    return nitf_46('JP2OpenJPEG')

###############################################################################
# Check reading of rsets.


def test_nitf_47():

    ds = gdal.Open('data/nitf/rset.ntf.r0')

    band = ds.GetRasterBand(2)
    assert band.GetOverviewCount() == 2, \
        'did not get the expected number of rset overviews.'

    cs = band.GetOverview(1).Checksum()
    assert cs == 1297, 'did not get expected checksum for overview of subdataset'

    ds = None

###############################################################################
# Check building of standard overviews in place of rset overviews.


def test_nitf_48():

    try:
        os.remove('tmp/rset.ntf.r0')
        os.remove('tmp/rset.ntf.r1')
        os.remove('tmp/rset.ntf.r2')
        os.remove('tmp/rset.ntf.r0.ovr')
    except OSError:
        pass

    shutil.copyfile('data/nitf/rset.ntf.r0', 'tmp/rset.ntf.r0')
    shutil.copyfile('data/nitf/rset.ntf.r1', 'tmp/rset.ntf.r1')
    shutil.copyfile('data/nitf/rset.ntf.r2', 'tmp/rset.ntf.r2')

    ds = gdal.Open('tmp/rset.ntf.r0', gdal.GA_Update)
    ds.BuildOverviews(overviewlist=[3])
    ds = None

    ds = gdal.Open('tmp/rset.ntf.r0')
    assert ds.GetRasterBand(1).GetOverviewCount() == 1, \
        'did not get the expected number of rset overviews.'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 2328, 'did not get expected checksum for overview of subdataset'

    ds = None

    try:
        os.remove('tmp/rset.ntf.r0')
        os.remove('tmp/rset.ntf.r1')
        os.remove('tmp/rset.ntf.r2')
        os.remove('tmp/rset.ntf.r0.ovr')
    except OSError:
        pass


###############################################################################
# Test TEXT and CGM creation options with CreateCopy() (#3376)


def test_nitf_49():

    options = ["TEXT=DATA_0=COUCOU",
               "TEXT=HEADER_0=ABC",  # This content is invalid but who cares here
               "CGM=SEGMENT_COUNT=1",
               "CGM=SEGMENT_0_SLOC_ROW=25",
               "CGM=SEGMENT_0_SLOC_COL=25",
               "CGM=SEGMENT_0_SDLVL=2",
               "CGM=SEGMENT_0_SALVL=1",
               "CGM=SEGMENT_0_DATA=XYZ"]

    src_ds = gdal.Open('data/nitf/text_md.vrt')

    # This will check that the creation option overrides the TEXT metadata domain from the source
    ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf49.ntf', src_ds,
                                                 options=options)

    # Test copy from source TEXT and CGM metadata domains
    ds2 = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf49_2.ntf', ds)

    md = ds2.GetMetadata('TEXT')
    if 'DATA_0' not in md or md['DATA_0'] != 'COUCOU' or \
       'HEADER_0' not in md or md['HEADER_0'].find('ABC  ') == -1:
        gdaltest.post_reason('did not get expected TEXT metadata')
        print(md)
        return

    md = ds2.GetMetadata('CGM')
    if 'SEGMENT_COUNT' not in md or md['SEGMENT_COUNT'] != '1' or \
       'SEGMENT_0_DATA' not in md or md['SEGMENT_0_DATA'] != 'XYZ':
        gdaltest.post_reason('did not get expected CGM metadata')
        print(md)
        return

    src_ds = None
    ds = None
    ds2 = None

###############################################################################
# Test TEXT and CGM creation options with Create() (#3376)


def test_nitf_50():

    options = [  # "IC=C8",
        "TEXT=DATA_0=COUCOU",
        "TEXT=HEADER_0=ABC",  # This content is invalid but who cares here
        "CGM=SEGMENT_COUNT=1",
        "CGM=SEGMENT_0_SLOC_ROW=25",
        "CGM=SEGMENT_0_SLOC_COL=25",
        "CGM=SEGMENT_0_SDLVL=2",
        "CGM=SEGMENT_0_SALVL=1",
        "CGM=SEGMENT_0_DATA=XYZ"]

    try:
        os.remove('tmp/nitf50.ntf')
    except OSError:
        pass

    # This will check that the creation option overrides the TEXT metadata domain from the source
    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf50.ntf', 100, 100, 3, options=options)

    ds.WriteRaster(0, 0, 100, 100, '   ', 1, 1,
                   buf_type=gdal.GDT_Byte,
                   band_list=[1, 2, 3])

    ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_BlueBand)
    ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_RedBand)

    # We need to reopen the dataset, because the TEXT and CGM segments are only written
    # when closing the dataset (for JP2 compressed datastreams, we need to wait for the
    # imagery to be written)
    ds = None
    ds = gdal.Open('tmp/nitf50.ntf')

    md = ds.GetMetadata('TEXT')
    if 'DATA_0' not in md or md['DATA_0'] != 'COUCOU' or \
       'HEADER_0' not in md or md['HEADER_0'].find('ABC  ') == -1:
        gdaltest.post_reason('did not get expected TEXT metadata')
        print(md)
        return

    md = ds.GetMetadata('CGM')
    if 'SEGMENT_COUNT' not in md or md['SEGMENT_COUNT'] != '1' or \
       'SEGMENT_0_DATA' not in md or md['SEGMENT_0_DATA'] != 'XYZ':
        gdaltest.post_reason('did not get expected CGM metadata')
        print(md)
        return

    ds = None

###############################################################################
# Test reading very small images with NBPP < 8 or NBPP == 12


def test_nitf_51():
    for xsize in range(1, 9):
        for nbpp in [1, 2, 3, 4, 5, 6, 7, 12]:
            ds = gdal.GetDriverByName('NITF').Create('tmp/nitf51.ntf', xsize, 1)
            ds = None

            f = open('tmp/nitf51.ntf', 'rb+')
            # Patch NBPP value at offset 811
            f.seek(811)
            f.write(struct.pack('B' * 2, 48 + int(nbpp / 10), 48 + nbpp % 10))

            # Write image data
            f.seek(843)
            n = int((xsize * nbpp + 7) / 8)
            for i in range(n):
                f.write(struct.pack('B' * 1, 255))

            f.close()

            ds = gdal.Open('tmp/nitf51.ntf')
            if nbpp == 12:
                data = ds.GetRasterBand(1).ReadRaster(0, 0, xsize, 1, buf_type=gdal.GDT_UInt16)
                arr = struct.unpack('H' * xsize, data)
            else:
                data = ds.GetRasterBand(1).ReadRaster(0, 0, xsize, 1)
                arr = struct.unpack('B' * xsize, data)

            ds = None

            for i in range(xsize):
                if arr[i] != (1 << nbpp) - 1:
                    print('xsize = %d, nbpp = %d' % (xsize, nbpp))
                    pytest.fail('did not get expected data')


###############################################################################
# Test reading GeoSDE TREs


def test_nitf_52():

    # Create a fake NITF file with GeoSDE TREs (probably not conformant, but enough to test GDAL code)
    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf52.ntf', 1, 1, options=['FILE_TRE=GEOPSB=01234567890123456789012345678901234567890123456789012345678901234567890123456789012345EURM                                                                                                                                                                                                                                                                                                                                                                 ',
                                                                              'FILE_TRE=PRJPSB=01234567890123456789012345678901234567890123456789012345678901234567890123456789AC0000000000000000000000000000000',
                                                                              'TRE=MAPLOB=M  0001000010000000000100000000000005000000'])
    ds = None

    ds = gdal.Open('tmp/nitf52.ntf')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    expected_wkt ="""PROJCS["unnamed",GEOGCS["EUROPEAN 1950, Mean (3 Param)",DATUM["EUROPEAN_1950_Mean_3_Param",SPHEROID["International 1924",6378388,297],TOWGS84[-87,-98,-121,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Albers_Conic_Equal_Area"],PARAMETER["latitude_of_center",0],PARAMETER["longitude_of_center",0],PARAMETER["standard_parallel_1",0],PARAMETER["standard_parallel_2",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"""
    assert wkt in (expected_wkt, expected_wkt.replace('EUROPEAN_1950_Mean_3_Param', 'EUROPEAN 1950, Mean (3 Param)'))

    assert gt == (100000.0, 10.0, 0.0, 5000000.0, 0.0, -10.0), \
        'did not get expected geotransform'

###############################################################################
# Test reading UTM MGRS


def test_nitf_53():

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf53.ntf', 2, 2, options=['ICORDS=N'])
    ds = None

    f = open('tmp/nitf53.ntf', 'rb+')

    # Patch ICORDS and IGEOLO
    f.seek(775)
    f.write(b'U')
    f.write(b'31UBQ1000040000')
    f.write(b'31UBQ2000040000')
    f.write(b'31UBQ2000030000')
    f.write(b'31UBQ1000030000')

    f.close()

    ds = gdal.Open('tmp/nitf53.ntf')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    assert 'PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0]' in wkt, \
        'did not get expected SRS'

    assert gt == (205000.0, 10000.0, 0.0, 5445000.0, 0.0, -10000.0), \
        'did not get expected geotransform'

###############################################################################
# Test reading RPC00B


def test_nitf_54():

    # Create a fake NITF file with RPC00B TRE (probably not conformant, but enough to test GDAL code)
    RPC00B = '100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf54.ntf', 1, 1, options=['TRE=RPC00B=' + RPC00B])
    ds = None

    ds = gdal.Open('tmp/nitf54.ntf')
    md = ds.GetMetadata('RPC')
    ds = None

    assert md is not None and 'HEIGHT_OFF' in md

###############################################################################
# Test reading ICHIPB


def test_nitf_55():

    # Create a fake NITF file with ICHIPB TRE (probably not conformant, but enough to test GDAL code)
    ICHIPB = '00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf55.ntf', 1, 1, options=['TRE=ICHIPB=' + ICHIPB])
    ds = None

    ds = gdal.Open('tmp/nitf55.ntf')
    md = ds.GetMetadata()
    ds = None

    assert md is not None and 'ICHIP_SCALE_FACTOR' in md

###############################################################################
# Test reading USE00A


def test_nitf_56():

    # Create a fake NITF file with USE00A TRE (probably not conformant, but enough to test GDAL code)
    USE00A = '00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf56.ntf', 1, 1, options=['TRE=USE00A=' + USE00A])
    ds = None

    ds = gdal.Open('tmp/nitf56.ntf')
    md = ds.GetMetadata()
    ds = None

    assert md is not None and 'NITF_USE00A_ANGLE_TO_NORTH' in md

###############################################################################
# Test reading GEOLOB


def test_nitf_57():

    # Create a fake NITF file with GEOLOB TRE
    GEOLOB = '000000360000000360-180.000000000090.000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf57.ntf', 1, 1, options=['TRE=GEOLOB=' + GEOLOB])
    ds = None

    ds = gdal.Open('tmp/nitf57.ntf')
    gt = ds.GetGeoTransform()
    ds = None

    if gt != (-180.0, 1.0, 0.0, 90.0, 0.0, -1.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return


###############################################################################
# Test reading STDIDC


def test_nitf_58():

    # Create a fake NITF file with STDIDC TRE (probably not conformant, but enough to test GDAL code)
    STDIDC = '00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf58.ntf', 1, 1, options=['TRE=STDIDC=' + STDIDC])
    ds = None

    ds = gdal.Open('tmp/nitf58.ntf')
    md = ds.GetMetadata()
    ds = None

    assert md is not None and 'NITF_STDIDC_ACQUISITION_DATE' in md

###############################################################################
# Test reading IMRFCA and IMASDA


def test_nitf_read_IMRFCA_IMASDA():

    # Create a fake NITF file with fake IMRFCA and IMASDA TRE
    IMRFCA = '0' * 1760
    IMASDA = '0' * 242

    tmpfile = '/vsimem/nitf_read_IMRFCA_IMASDA.ntf'
    gdal.GetDriverByName('NITF').Create(tmpfile, 1, 1, options=['TRE=IMRFCA=' + IMRFCA, 'TRE=IMASDA=' + IMASDA])
    ds = gdal.Open(tmpfile)
    md = ds.GetMetadata('RPC')
    ds = None
    gdal.Unlink(tmpfile)
    assert not (md is None or md == {})

    # Only IMRFCA
    gdal.GetDriverByName('NITF').Create(tmpfile, 1, 1, options=['TRE=IMRFCA=' + IMRFCA])
    ds = gdal.Open(tmpfile)
    md = ds.GetMetadata('RPC')
    ds = None
    gdal.Unlink(tmpfile)
    assert md == {}

    # Only IMASDA
    gdal.GetDriverByName('NITF').Create(tmpfile, 1, 1, options=['TRE=IMASDA=' + IMASDA])
    ds = gdal.Open(tmpfile)
    md = ds.GetMetadata('RPC')
    ds = None
    gdal.Unlink(tmpfile)
    assert md == {}

    # Too short IMRFCA
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').Create(tmpfile, 1, 1, options=['TRE=IMRFCA=' + IMRFCA[0:-1], 'TRE=IMASDA=' + IMASDA])
        ds = gdal.Open(tmpfile)
    md = ds.GetMetadata('RPC')
    ds = None
    gdal.Unlink(tmpfile)
    assert md == {}

    # Too short IMASDA
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').Create(tmpfile, 1, 1, options=['TRE=IMRFCA=' + IMRFCA, 'TRE=IMASDA=' + IMASDA[0:-1]])
        ds = gdal.Open(tmpfile)
    md = ds.GetMetadata('RPC')
    ds = None
    gdal.Unlink(tmpfile)
    assert md == {}

###############################################################################
# Test georeferencing through .nfw and .hdr files


def test_nitf_59():

    shutil.copyfile('data/nitf/nitf59.nfw', 'tmp/nitf59.nfw')
    shutil.copyfile('data/nitf/nitf59.hdr', 'tmp/nitf59.hdr')
    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf59.ntf', 1, 1, options=['ICORDS=N'])
    ds = None

    ds = gdal.Open('tmp/nitf59.ntf')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    assert """PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0]""" in wkt, \
        'did not get expected SRS'

    assert gt == (149999.5, 1.0, 0.0, 4500000.5, 0.0, -1.0), \
        'did not get expected geotransform'

###############################################################################
# Test reading CADRG polar tile georeferencing (#2940)


def test_nitf_60():

    # Shut down errors because the file is truncated
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/nitf/testtest.on9')
    gdal.PopErrorHandler()
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    assert wkt == """PROJCS["ARC_System_Zone_09",GEOGCS["Unknown datum based upon the Authalic Sphere",DATUM["Not_specified_based_on_Authalic_Sphere",SPHEROID["Sphere",6378137,0],AUTHORITY["EPSG","6035"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Azimuthal_Equidistant"],PARAMETER["latitude_of_center",90],PARAMETER["longitude_of_center",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]""", \
        'did not get expected SRS'

    ref_gt = [1036422.8453166834, 149.94543479697344, 0.0, 345474.28177222813, 0.0, -149.94543479697404]
    for i in range(6):
        assert gt[i] == pytest.approx(ref_gt[i], abs=1e-6), 'did not get expected geotransform'


###############################################################################
# Test reading TRE from DE segment


def test_nitf_61():

    # Derived from http://www.gwg.nga.mil/ntb/baseline/software/testfile/rsm/SampleFiles/FrameSet1/NITF_Files/i_6130a.zip
    # but hand edited to have just 1x1 imagery
    ds = gdal.Open('data/nitf/i_6130a_truncated.ntf')
    md = ds.GetMetadata('TRE')
    xml_tre = ds.GetMetadata('xml:TRE')[0]
    ds = None

    assert md is not None and 'RSMDCA' in md and 'RSMECA' in md and 'RSMPCA' in md and 'RSMIDA' in md

    assert xml_tre.find('<tre name="RSMDCA"') != -1, 'did not get expected xml:TRE'

###############################################################################
# Test creating & reading image comments


def test_nitf_62():

    # 80+1 characters
    comments = '012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678ZA'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf62.ntf', 1, 1, options=['ICOM=' + comments])
    ds = None

    ds = gdal.Open('tmp/nitf62.ntf')
    md = ds.GetMetadata()
    ds = None

    got_comments = md['NITF_IMAGE_COMMENTS']
    if len(got_comments) != 160 or got_comments.find(comments) == -1:
        print("'%s'" % got_comments)
        pytest.fail('did not get expected comments')


###############################################################################
# Test NITFReadImageLine() and NITFWriteImageLine() when nCols < nBlockWidth (#3551)


def test_nitf_63():

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf63.ntf', 50, 25, 3, gdal.GDT_Int16, options=['BLOCKXSIZE=256'])
    ds = None

    try:
        os.SEEK_SET
    except AttributeError:
        os.SEEK_SET, os.SEEK_CUR, os.SEEK_END = list(range(3))

    # Patch IMODE at hand
    f = open('tmp/nitf63.ntf', 'r+')
    f.seek(820, os.SEEK_SET)
    f.write('P')
    f.close()

    ds = gdal.Open('tmp/nitf63.ntf', gdal.GA_Update)
    md = ds.GetMetadata()
    assert md['NITF_IMODE'] == 'P', 'wrong IMODE'
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(127)
    ds.GetRasterBand(3).Fill(255)
    ds = None

    ds = gdal.Open('tmp/nitf63.ntf')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    ds = None

    assert cs1 == 0 and cs2 == 14186 and cs3 == 15301, \
        ('did not get expected checksums : (%d, %d, %d) instead of (0, 14186, 15301)' % (cs1, cs2, cs3))

###############################################################################
# Test SDE_TRE creation option


def test_nitf_64():

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/nitf_64.tif', 256, 256, 1)
    src_ds.SetGeoTransform([2.123456789, 0.123456789, 0, 49.123456789, 0, -0.123456789])
    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('WGS84')
    src_ds.SetProjection(sr.ExportToWkt())

    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_64.ntf', src_ds, options=['ICORDS=D'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_64.ntf')
    # One can notice that the topleft location is only precise to the 3th decimal !
    expected_gt = (2.123270588235294, 0.12345882352941177, 0.0, 49.123729411764707, 0.0, -0.12345882352941176)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        assert expected_gt[i] == pytest.approx(got_gt[i], abs=1e-10), \
            'did not get expected GT in ICORDS=D mode'
    ds = None

    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_64.ntf', src_ds, options=['ICORDS=G'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_64.ntf')
    # One can notice that the topleft location is only precise to the 3th decimal !
    expected_gt = (2.1235495642701521, 0.12345642701525053, 0.0, 49.123394880174288, 0.0, -0.12345642701525052)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        assert expected_gt[i] == pytest.approx(got_gt[i], abs=1e-10), \
            'did not get expected GT in ICORDS=G mode'
    ds = None

    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_64.ntf', src_ds, options=['SDE_TRE=YES'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_64.ntf')
    # One can notice that the topleft location is precise up to the 9th decimal
    expected_gt = (2.123456789, 0.1234567901234568, 0.0, 49.123456789000002, 0.0, -0.12345679012345678)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        assert expected_gt[i] == pytest.approx(got_gt[i], abs=1e-10), \
            'did not get expected GT in SDE_TRE mode'
    ds = None

    src_ds = None
    gdal.Unlink('/vsimem/nitf_64.tif')
    gdal.Unlink('/vsimem/nitf_64.ntf')

###############################################################################
# Test creating an image with block_width = image_width > 8192 (#3922)


def test_nitf_65():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_65.ntf', 10000, 100, options=['BLOCKXSIZE=10000'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_65.ntf')
    (block_xsize, _) = ds.GetRasterBand(1).GetBlockSize()
    ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/nitf_65.ntf')

    assert block_xsize == 10000

###############################################################################
# Test creating an image with block_height = image_height > 8192 (#3922)


def test_nitf_66():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_66.ntf', 100, 10000, options=['BLOCKYSIZE=10000', 'BLOCKXSIZE=50'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_66.ntf')
    (_, block_ysize) = ds.GetRasterBand(1).GetBlockSize()
    ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/nitf_66.ntf')

    assert block_ysize == 10000

###############################################################################
# Test that we don't use scanline access in illegal cases (#3926)


def test_nitf_67():

    src_ds = gdal.Open('data/byte.tif')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_67.ntf', src_ds, options=['BLOCKYSIZE=1', 'BLOCKXSIZE=10'], strict=0)
    gdal.PopErrorHandler()
    ds = None
    src_ds = None

    ds = gdal.Open('/vsimem/nitf_67.ntf')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/nitf_67.ntf')
    gdal.Unlink('/vsimem/nitf_67.ntf.aux.xml')

    assert cs == 4672

###############################################################################
# Test reading NITF_METADATA domain


def test_nitf_68():

    ds = gdal.Open('data/nitf/rgb.ntf')
    assert len(ds.GetMetadata('NITF_METADATA')) == 2
    ds = None

    ds = gdal.Open('data/nitf/rgb.ntf')
    assert ds.GetMetadataItem('NITFFileHeader', 'NITF_METADATA')
    ds = None

###############################################################################
# Test SetGCPs() support


def test_nitf_69():

    vrt_txt = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <GCPList Projection='GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]'>
        <GCP Id="" Pixel="0.5" Line="0.5" X="2" Y="49"/>
        <GCP Id="" Pixel="0.5" Line="19.5" X="2" Y="48"/>
        <GCP Id="" Pixel="19.5" Line="0.5" X="3" Y="49.5"/>
        <GCP Id="" Pixel="19.5" Line="19.5" X="3" Y="48"/>
    </GCPList>
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SourceBand>1</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""

    # Test CreateCopy()
    vrt_ds = gdal.Open(vrt_txt)
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_69_src.ntf', vrt_ds)
    ds = None
    vrt_ds = None

    # Just in case
    gdal.Unlink('/vsimem/nitf_69_src.ntf.aux.xml')

    # Test Create() and SetGCPs()
    src_ds = gdal.Open('/vsimem/nitf_69_src.ntf')
    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_69_dest.ntf', 20, 20, 1, options=['ICORDS=G'])
    ds.SetGCPs(src_ds.GetGCPs(), src_ds.GetGCPProjection())
    ds.SetGCPs(src_ds.GetGCPs(), src_ds.GetGCPProjection())  # To check we can call it several times without error
    ds = None
    src_ds = None

    # Now open again
    ds = gdal.Open('/vsimem/nitf_69_dest.ntf')
    got_gcps = ds.GetGCPs()
    ds = None

    gdal.Unlink('/vsimem/nitf_69_src.ntf')
    gdal.Unlink('/vsimem/nitf_69_dest.ntf')

    # Check

    # Upper-left
    assert (got_gcps[0].GCPPixel == pytest.approx(0.5, abs=1e-5) and got_gcps[0].GCPLine == pytest.approx(0.5, abs=1e-5) and \
       got_gcps[0].GCPX == pytest.approx(2, abs=1e-5) and got_gcps[0].GCPY == pytest.approx(49, abs=1e-5)), \
        'wrong gcp'

    # Upper-right
    assert (got_gcps[1].GCPPixel == pytest.approx(19.5, abs=1e-5) and got_gcps[1].GCPLine == pytest.approx(0.5, abs=1e-5) and \
       got_gcps[1].GCPX == pytest.approx(3, abs=1e-5) and got_gcps[1].GCPY == pytest.approx(49.5, abs=1e-5)), \
        'wrong gcp'

    # Lower-right
    assert (got_gcps[2].GCPPixel == pytest.approx(19.5, abs=1e-5) and got_gcps[2].GCPLine == pytest.approx(19.5, abs=1e-5) and \
       got_gcps[2].GCPX == pytest.approx(3, abs=1e-5) and got_gcps[2].GCPY == pytest.approx(48, abs=1e-5)), \
        'wrong gcp'

    # Lower-left
    assert (got_gcps[3].GCPPixel == pytest.approx(0.5, abs=1e-5) and got_gcps[3].GCPLine == pytest.approx(19.5, abs=1e-5) and \
       got_gcps[3].GCPX == pytest.approx(2, abs=1e-5) and got_gcps[3].GCPY == pytest.approx(48, abs=1e-5)), \
        'wrong gcp'

###############################################################################
# Create and read a JPEG encoded NITF file with NITF dimensions != JPEG dimensions


def test_nitf_70():

    src_ds = gdal.Open('data/rgbsmall.tif')

    ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf_70.ntf', src_ds,
                                                 options=['IC=C3', 'BLOCKXSIZE=64', 'BLOCKYSIZE=64'])
    ds = None

    # For comparison
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/nitf_70.tif', src_ds,
                                                  options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'TILED=YES', 'BLOCKXSIZE=64', 'BLOCKYSIZE=64'])
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/nitf_70.ntf')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    ds = gdal.Open('tmp/nitf_70.tif')
    cs_ref = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.GetDriverByName('NITF').Delete('tmp/nitf_70.ntf')
    gdal.GetDriverByName('GTiff').Delete('tmp/nitf_70.tif')

    # cs == 21821 is what we get with Conda Windows and libjpeg-9e, and cs_ref == 21962 in that case
    # TODO (or maybe not! why in the hell should we care about IJG libjpeg): find out why those values aren't equal...
    assert cs == cs_ref or cs == 21821

###############################################################################
# Test reading ENGRDA TRE (#6285)


def test_nitf_71():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_71.ntf', 1, 1, options=['TRE=ENGRDA=0123456789012345678900210012345678901230123X01200000002XY01X01230123X01200000001X'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_71.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_71.ntf')

    expected_data = """<tres>
  <tre name="ENGRDA" location="image">
    <field name="RESRC" value="01234567890123456789" />
    <field name="RECNT" value="002" />
    <repeated name="RECORDS" number="2">
      <group index="0">
        <field name="ENGLN" value="10" />
        <field name="ENGLBL" value="0123456789" />
        <field name="ENGMTXC" value="0123" />
        <field name="ENGMTXR" value="0123" />
        <field name="ENGTYP" value="X" />
        <field name="ENGDTS" value="0" />
        <field name="ENGDTU" value="12" />
        <field name="ENGDATC" value="00000002" />
        <field name="ENGDATA" value="XY" />
      </group>
      <group index="1">
        <field name="ENGLN" value="01" />
        <field name="ENGLBL" value="X" />
        <field name="ENGMTXC" value="0123" />
        <field name="ENGMTXR" value="0123" />
        <field name="ENGTYP" value="X" />
        <field name="ENGDTS" value="0" />
        <field name="ENGDTU" value="12" />
        <field name="ENGDATC" value="00000001" />
        <field name="ENGDATA" value="X" />
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test writing and reading RPC00B


def compare_rpc(src_md, md):
    # Check that we got data with the expected precision
    for key in src_md:
        if key == 'ERR_BIAS' or key == 'ERR_RAND':
            continue
        assert key in md, ('fail: %s missing' % key)
        if 'COEFF' in key:
            expected = [float(v) for v in src_md[key].strip().split(' ')]
            found = [float(v) for v in md[key].strip().split(' ')]
            if expected != found:
                print(md)
                pytest.fail('fail: %s value is not the one expected' % key)
        elif float(src_md[key]) != float(md[key]):
            print(md)
            pytest.fail('fail: %s value is not the one expected' % key)


def test_nitf_72():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    # Use full precision
    src_md_max_precision = {
        'ERR_BIAS': '1234.56',
        'ERR_RAND': '2345.67',
        'LINE_OFF': '345678',
        'SAMP_OFF': '45678',
        'LAT_OFF': '-89.8765',
        'LONG_OFF': '-179.1234',
        'HEIGHT_OFF': '-9876',
        'LINE_SCALE': '987654',
        'SAMP_SCALE': '67890',
        'LAT_SCALE': '-12.3456',
        'LONG_SCALE': '-123.4567',
        'HEIGHT_SCALE': '-1234',
        'LINE_NUM_COEFF': '0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
        'LINE_DEN_COEFF': '1 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
        'SAMP_NUM_COEFF': '2 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
        'SAMP_DEN_COEFF': '3 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
    }
    src_md = src_md_max_precision
    src_ds.SetMetadata(src_md, 'RPC')

    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds)

    assert gdal.GetLastErrorMsg() == '', 'fail: did not expect warning'

    if gdal.VSIStatL('/vsimem/nitf_72.ntf.aux.xml') is not None:
        f = gdal.VSIFOpenL('/vsimem/nitf_72.ntf.aux.xml', 'rb')
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        print(str(data))
        pytest.fail('fail: PAM file not expected')

    ds = gdal.Open('/vsimem/nitf_72.ntf')
    md = ds.GetMetadata('RPC')
    RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_72.ntf')

    compare_rpc(src_md, md)

    expected_RPC00B_max_precision = '11234.562345.6734567845678-89.8765-179.1234-987698765467890-12.3456-123.4567-1234+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+1.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+2.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+3.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9'
    assert RPC00B == expected_RPC00B_max_precision, 'fail: did not get expected RPC00B'

    # Test without ERR_BIAS and ERR_RAND
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_md = copy.copy(src_md_max_precision)
    del src_md['ERR_BIAS']
    del src_md['ERR_RAND']
    src_ds.SetMetadata(src_md, 'RPC')

    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds)

    assert gdal.GetLastErrorMsg() == '', 'fail: did not expect warning'

    if gdal.VSIStatL('/vsimem/nitf_72.ntf.aux.xml') is not None:
        f = gdal.VSIFOpenL('/vsimem/nitf_72.ntf.aux.xml', 'rb')
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        print(str(data))
        pytest.fail('fail: PAM file not expected')

    ds = gdal.Open('/vsimem/nitf_72.ntf')
    md = ds.GetMetadata('RPC')
    RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
    ds = None

    expected_RPC00B = '10000.000000.0034567845678-89.8765-179.1234-987698765467890-12.3456-123.4567-1234+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+1.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+2.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+3.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9'
    assert RPC00B == expected_RPC00B, 'fail: did not get expected RPC00B'

    # Test that direct RPC00B copy works
    src_nitf_ds = gdal.Open('/vsimem/nitf_72.ntf')
    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72_copy.ntf', src_nitf_ds)
    src_nitf_ds = None

    ds = gdal.Open('/vsimem/nitf_72_copy.ntf')
    md = ds.GetMetadata('RPC')
    RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
    ds = None
    assert RPC00B == expected_RPC00B, 'fail: did not get expected RPC00B'

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_72.ntf')
    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_72_copy.ntf')

    # Test that RPC00B = NO works
    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds, options=['RPC00B=NO'])

    assert gdal.VSIStatL('/vsimem/nitf_72.ntf.aux.xml') is not None, \
        'fail: PAM file was expected'

    ds = gdal.Open('/vsimem/nitf_72.ntf')
    md = ds.GetMetadata('RPC')
    RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_72.ntf')
    assert RPC00B is None, 'fail: did not expect RPC00B'

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    # Test padding
    src_md = {
        'ERR_BIAS': '123',
        'ERR_RAND': '234',
        'LINE_OFF': '3456',
        'SAMP_OFF': '4567',
        'LAT_OFF': '8',
        'LONG_OFF': '17',
        'HEIGHT_OFF': '987',
        'LINE_SCALE': '98765',
        'SAMP_SCALE': '6789',
        'LAT_SCALE': '12',
        'LONG_SCALE': '109',
        'HEIGHT_SCALE': '34',
        'LINE_NUM_COEFF': '0 9.87e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
        'LINE_DEN_COEFF': '1 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
        'SAMP_NUM_COEFF': '2 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
        'SAMP_DEN_COEFF': '3 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9',
    }
    src_ds.SetMetadata(src_md, 'RPC')

    gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds)

    assert gdal.GetLastErrorMsg() == '', 'fail: did not expect warning'

    if gdal.VSIStatL('/vsimem/nitf_72.ntf.aux.xml') is not None:
        f = gdal.VSIFOpenL('/vsimem/nitf_72.ntf.aux.xml', 'rb')
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        print(str(data))
        pytest.fail('fail: PAM file not expected')

    ds = gdal.Open('/vsimem/nitf_72.ntf')
    md = ds.GetMetadata('RPC')
    RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_72.ntf')

    compare_rpc(src_md, md)

    expected_RPC00B = '10123.000234.0000345604567+08.0000+017.0000+098709876506789+12.0000+109.0000+0034+0.000000E+0+9.870000E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+1.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+2.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+3.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9'
    assert RPC00B == expected_RPC00B, 'fail: did not get expected RPC00B'

    # Test loss of precision
    for key in ('LINE_OFF', 'SAMP_OFF', 'LAT_OFF', 'LONG_OFF', 'HEIGHT_OFF', 'LINE_SCALE', 'SAMP_SCALE', 'LAT_SCALE', 'LONG_SCALE', 'HEIGHT_SCALE'):
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        src_md = copy.copy(src_md_max_precision)
        if src_md[key].find('.') < 0:
            src_md[key] += '.1'
        else:
            src_md[key] += '1'

        src_ds.SetMetadata(src_md, 'RPC')

        with gdaltest.error_handler():
            ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds)
        assert ds is not None, 'fail: expected a dataset'
        ds = None

        assert gdal.GetLastErrorMsg() != '', 'fail: expected a warning'

        assert gdal.VSIStatL('/vsimem/nitf_72.ntf.aux.xml') is not None, \
            'fail: PAM file was expected'
        gdal.Unlink('/vsimem/nitf_72.ntf.aux.xml')

        ds = gdal.Open('/vsimem/nitf_72.ntf')
        md = ds.GetMetadata('RPC')
        RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
        ds = None

        gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_72.ntf')

        assert RPC00B == expected_RPC00B_max_precision, \
            'fail: did not get expected RPC00B'

    # Test loss of precision on coefficient lines
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_md = copy.copy(src_md_max_precision)
    src_md['LINE_NUM_COEFF'] = '0 9.876543e-10 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9'
    src_ds.SetMetadata(src_md, 'RPC')

    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds)
    assert ds is not None, 'fail: expected a dataset'
    ds = None

    assert gdal.GetLastErrorMsg() != '', 'fail: expected a warning'

    assert gdal.VSIStatL('/vsimem/nitf_72.ntf.aux.xml') is not None, \
        'fail: PAM file was expected'
    gdal.Unlink('/vsimem/nitf_72.ntf.aux.xml')

    ds = gdal.Open('/vsimem/nitf_72.ntf')
    md = ds.GetMetadata('RPC')
    RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_72.ntf')

    expected_RPC00B = '11234.562345.6734567845678-89.8765-179.1234-987698765467890-12.3456-123.4567-1234+0.000000E+0+0.000000E+0+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+1.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+2.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+3.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9+0.000000E+0+9.876543E+9+9.876543E-9-9.876543E+9-9.876543E-9'
    assert RPC00B == expected_RPC00B, 'fail: did not get expected RPC00B'

    # Test RPCTXT creation option
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds, options=['RPCTXT=YES'])

    assert gdal.VSIStatL('/vsimem/nitf_72_RPC.TXT') is not None, \
        'fail: rpc.txt file was expected'

    ds = gdal.Open('/vsimem/nitf_72.ntf')
    md = ds.GetMetadata('RPC')
    RPC00B = ds.GetMetadataItem('RPC00B', 'TRE')
    fl = ds.GetFileList()
    ds = None

    assert '/vsimem/nitf_72_RPC.TXT' in fl, \
        'fail: _RPC.TXT file not reported in file list'

    # Check that we get full precision from the _RPC.TXT file
    compare_rpc(src_md, md)

    assert RPC00B == expected_RPC00B, 'fail: did not get expected RPC00B'

    # Test out of range
    for key in ('LINE_OFF', 'SAMP_OFF', 'LAT_OFF', 'LONG_OFF', 'HEIGHT_OFF', 'LINE_SCALE', 'SAMP_SCALE', 'LAT_SCALE', 'LONG_SCALE', 'HEIGHT_SCALE'):
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        src_md = copy.copy(src_md_max_precision)
        if src_md[key].find('-') >= 0:
            src_md[key] = '-1' + src_md[key][1:]
        else:
            src_md[key] = '1' + src_md[key]

        src_ds.SetMetadata(src_md, 'RPC')

        with gdaltest.error_handler():
            ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds)
        assert ds is None, ('fail: expected failure for %s' % key)

    # Test out of rangeon coefficient lines
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_md = copy.copy(src_md_max_precision)
    src_md['LINE_NUM_COEFF'] = '0 9.876543e10 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9 0 9.876543e+9 9.876543e-9 -9.876543e+9 -9.876543e-9'
    src_ds.SetMetadata(src_md, 'RPC')

    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_72.ntf', src_ds)
    assert ds is None, 'fail: expected failure'

###############################################################################
# Test case for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1525


def test_nitf_73():

    with gdaltest.error_handler():
        gdal.Open('data/nitf/oss_fuzz_1525.ntf')


###############################################################################
# Test cases for CCLSTA
#  - Simple case


def test_nitf_74():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_74.ntf', 1, 1, options=['FILE_TRE=CCINFA=0012AS 17ge:GENC:3:3-5:AUS00000'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_74.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_74.ntf')

    expected_data = """<tres>
  <tre name="CCINFA" location="file">
    <field name="NUMCODE" value="001" />
    <repeated name="CODES" number="1">
      <group index="0">
        <field name="CODE_LEN" value="2" />
        <field name="CODE" value="AS" />
        <field name="EQTYPE" value="" />
        <field name="ESURN_LEN" value="17" />
        <field name="ESURN" value="ge:GENC:3:3-5:AUS" />
        <field name="DETAIL_LEN" value="00000" />
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

#  - TABLE AG.2 case


def test_nitf_75():

    listing_AG1 = """<?xml version="1.0" encoding="UTF-8"?>
<genc:GeopoliticalEntityEntry
    xmlns:genc="http://api.nsgreg.nga.mil/schema/genc/3.0"
    xmlns:genc-cmn="http://api.nsgreg.nga.mil/schema/genc/3.0/genc-cmn"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:schemaLocation="http://api.nsgreg.nga.mil/schema/genc/3.0 http://api.nsgreg.nga.mil/schema/genc/3.0.0/genc.xsd">
    <genc:encoding>
        <genc-cmn:char3Code>MMR</genc-cmn:char3Code>
        <genc-cmn:char3CodeURISet>
            <genc-cmn:codespaceURL>http://api.nsgreg.nga.mil/geo-political/GENC/3/3-5</genc-cmn:codespaceURL>
            <genc-cmn:codespaceURN>urn:us:gov:dod:nga:def:geo-political:GENC:3:3-5</genc-cmn:codespaceURN>
            <genc-cmn:codespaceURNBased>geo-political:GENC:3:3-5</genc-cmn:codespaceURNBased>
            <genc-cmn:codespaceURNBasedShort>ge:GENC:3:3-5</genc-cmn:codespaceURNBasedShort>
        </genc-cmn:char3CodeURISet>
        <genc-cmn:char2Code>MM</genc-cmn:char2Code>
        <genc-cmn:char2CodeURISet>
            <genc-cmn:codespaceURL>http://api.nsgreg.nga.mil/geo-political/GENC/2/3-5</genc-cmn:codespaceURL>
            <genc-cmn:codespaceURN>urn:us:gov:dod:nga:def:geo-political:GENC:2:3-5</genc-cmn:codespaceURN>
            <genc-cmn:codespaceURNBased>geo-political:GENC:2:3-5</genc-cmn:codespaceURNBased>
            <genc-cmn:codespaceURNBasedShort>ge:GENC:2:3-5</genc-cmn:codespaceURNBasedShort>
        </genc-cmn:char2CodeURISet>
        <genc-cmn:numericCode>104</genc-cmn:numericCode>
        <genc-cmn:numericCodeURISet>
            <genc-cmn:codespaceURL>http://api.nsgreg.nga.mil/geo-political/GENC/n/3-5</genc-cmn:codespaceURL>
            <genc-cmn:codespaceURN>urn:us:gov:dod:nga:def:geo-political:GENC:n:3-5</genc-cmn:codespaceURN>
            <genc-cmn:codespaceURNBased>geo-political:GENC:n:3-5</genc-cmn:codespaceURNBased>
            <genc-cmn:codespaceURNBasedShort>ge:GENC:n:3-5</genc-cmn:codespaceURNBasedShort>
        </genc-cmn:numericCodeURISet>
    </genc:encoding>
    <genc:name><![CDATA[BURMA]]></genc:name>
    <genc:shortName><![CDATA[Burma]]></genc:shortName>
    <genc:fullName><![CDATA[Union of Burma]]></genc:fullName>
    <genc:gencStatus>exception</genc:gencStatus>
    <genc:entryDate>2016-09-30</genc:entryDate>
    <genc:entryType>unchanged</genc:entryType>
    <genc:usRecognition>independent</genc:usRecognition>
    <genc:entryNotesOnNaming><![CDATA[
        The GENC Standard specifies the name "BURMA" where instead ISO 3166-1 specifies "MYANMAR"; GENC specifies the short name "Burma" where instead ISO 3166-1 specifies "Myanmar"; and GENC specifies the full name "Union of Burma" where instead ISO 3166-1 specifies "the Republic of the Union of Myanmar". The GENC Standard specifies the local short name for 'my'/'mya' as "Myanma Naingngandaw" where instead ISO 3166-1 specifies "Myanma".
        ]]></genc:entryNotesOnNaming>
    <genc:division codeSpace="as:GENC:6:3-5">MM-01</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-02</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-03</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-04</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-05</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-06</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-07</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-11</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-12</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-13</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-14</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-15</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-16</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-17</genc:division>
    <genc:division codeSpace="as:GENC:6:3-5">MM-18</genc:division>
    <genc:localShortName>
        <genc:name><![CDATA[Myanma Naingngandaw]]></genc:name>
        <genc:iso6393Char3Code>mya</genc:iso6393Char3Code>
    </genc:localShortName>
</genc:GeopoliticalEntityEntry>"""
    len_listing_AG1 = len(listing_AG1)

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_75.ntf', 1, 1,
        options=['TRE=CCINFA=0062RQ 17ge:GENC:3:3-5:PRI000002RQ 20as:ISO2:6:II-3:US-PR000002BM 17ge:GENC:3:3-5:MMR' +
                 ('%05d' % len_listing_AG1) + ' ' +
                 listing_AG1 +
                 '3MMR 19ge:ISO1:3:VII-7:MMR00000' + '2S1 19ge:GENC:3:3-alt:SCT000002YYC16gg:1059:2:ed9:3E00000'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_75.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_75.ntf')

    expected_data = """<tres>
  <tre name="CCINFA" location="image">
    <field name="NUMCODE" value="006" />
    <repeated name="CODES" number="6">
      <group index="0">
        <field name="CODE_LEN" value="2" />
        <field name="CODE" value="RQ" />
        <field name="EQTYPE" value="" />
        <field name="ESURN_LEN" value="17" />
        <field name="ESURN" value="ge:GENC:3:3-5:PRI" />
        <field name="DETAIL_LEN" value="00000" />
      </group>
      <group index="1">
        <field name="CODE_LEN" value="2" />
        <field name="CODE" value="RQ" />
        <field name="EQTYPE" value="" />
        <field name="ESURN_LEN" value="20" />
        <field name="ESURN" value="as:ISO2:6:II-3:US-PR" />
        <field name="DETAIL_LEN" value="00000" />
      </group>
      <group index="2">
        <field name="CODE_LEN" value="2" />
        <field name="CODE" value="BM" />
        <field name="EQTYPE" value="" />
        <field name="ESURN_LEN" value="17" />
        <field name="ESURN" value="ge:GENC:3:3-5:MMR" />
        <field name="DETAIL_LEN" value="%05d" />
        <field name="DETAIL_CMPR" value="" />
        <field name="DETAIL" value="%s" />
      </group>
      <group index="3">
        <field name="CODE_LEN" value="3" />
        <field name="CODE" value="MMR" />
        <field name="EQTYPE" value="" />
        <field name="ESURN_LEN" value="19" />
        <field name="ESURN" value="ge:ISO1:3:VII-7:MMR" />
        <field name="DETAIL_LEN" value="00000" />
      </group>
      <group index="4">
        <field name="CODE_LEN" value="2" />
        <field name="CODE" value="S1" />
        <field name="EQTYPE" value="" />
        <field name="ESURN_LEN" value="19" />
        <field name="ESURN" value="ge:GENC:3:3-alt:SCT" />
        <field name="DETAIL_LEN" value="00000" />
      </group>
      <group index="5">
        <field name="CODE_LEN" value="2" />
        <field name="CODE" value="YY" />
        <field name="EQTYPE" value="C" />
        <field name="ESURN_LEN" value="16" />
        <field name="ESURN" value="gg:1059:2:ed9:3E" />
        <field name="DETAIL_LEN" value="00000" />
      </group>
    </repeated>
  </tre>
</tres>
""" % (len_listing_AG1, gdal.EscapeString(listing_AG1, gdal.CPLES_XML))

    assert data == expected_data

###############################################################################
# Test parsing MATESA TRE (STDI-0002 App AK)

def test_nitf_76():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_76.ntf', 1, 1, options=['FILE_TRE=MATESA=EO-1_HYPERION                             FTITLE          006307APR2005_Hyperion_331406N0442000E_SWIR172_001_L1R-01B-BIB-GLAS0005RADIOMTRC_CALIB         0001EO-1_HYPERION                             FILENAME        0020HypGain_revC.dat.svfPARENT                  0001EO-1_HYPERION                             FILENAME        0032EO12005097_020D020C_r1_WPS_01.L0PRE_DARKCOLLECT         0001EO-1_HYPERION                             FILENAME        0032EO12005097_020A0209_r1_WPS_01.L0POST_DARKCOLLECT        0001EO-1_HYPERION                             FILENAME        0032EO12005097_020F020E_r1_WPS_01.L0PARENT                  0003EO-1_HYPERION                             FILENAME        0026EO1H1680372005097110PZ.L1REO-1_HYPERION                             FILENAME        0026EO1H1680372005097110PZ.AUXEO-1_HYPERION                             FILENAME        0026EO1H1680372005097110PZ.MET'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_76.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_76.ntf')

    expected_data = """<tres>
  <tre name="MATESA" location="file">
    <field name="CUR_SOURCE" value="EO-1_HYPERION" />
    <field name="CUR_MATE_TYPE" value="FTITLE" />
    <field name="CUR_FILE_ID_LEN" value="0063" />
    <field name="CUR_FILE_ID" value="07APR2005_Hyperion_331406N0442000E_SWIR172_001_L1R-01B-BIB-GLAS" />
    <field name="NUM_GROUPS" value="0005" />
    <repeated name="GROUPS" number="5">
      <group index="0">
        <field name="RELATIONSHIP" value="RADIOMTRC_CALIB" />
        <field name="NUM_MATES" value="0001" />
        <repeated name="MATES" number="1">
          <group index="0">
            <field name="SOURCE" value="EO-1_HYPERION" />
            <field name="MATE_TYPE" value="FILENAME" />
            <field name="MATE_ID_LEN" value="0020" />
            <field name="MATE_ID" value="HypGain_revC.dat.svf" />
          </group>
        </repeated>
      </group>
      <group index="1">
        <field name="RELATIONSHIP" value="PARENT" />
        <field name="NUM_MATES" value="0001" />
        <repeated name="MATES" number="1">
          <group index="0">
            <field name="SOURCE" value="EO-1_HYPERION" />
            <field name="MATE_TYPE" value="FILENAME" />
            <field name="MATE_ID_LEN" value="0032" />
            <field name="MATE_ID" value="EO12005097_020D020C_r1_WPS_01.L0" />
          </group>
        </repeated>
      </group>
      <group index="2">
        <field name="RELATIONSHIP" value="PRE_DARKCOLLECT" />
        <field name="NUM_MATES" value="0001" />
        <repeated name="MATES" number="1">
          <group index="0">
            <field name="SOURCE" value="EO-1_HYPERION" />
            <field name="MATE_TYPE" value="FILENAME" />
            <field name="MATE_ID_LEN" value="0032" />
            <field name="MATE_ID" value="EO12005097_020A0209_r1_WPS_01.L0" />
          </group>
        </repeated>
      </group>
      <group index="3">
        <field name="RELATIONSHIP" value="POST_DARKCOLLECT" />
        <field name="NUM_MATES" value="0001" />
        <repeated name="MATES" number="1">
          <group index="0">
            <field name="SOURCE" value="EO-1_HYPERION" />
            <field name="MATE_TYPE" value="FILENAME" />
            <field name="MATE_ID_LEN" value="0032" />
            <field name="MATE_ID" value="EO12005097_020F020E_r1_WPS_01.L0" />
          </group>
        </repeated>
      </group>
      <group index="4">
        <field name="RELATIONSHIP" value="PARENT" />
        <field name="NUM_MATES" value="0003" />
        <repeated name="MATES" number="3">
          <group index="0">
            <field name="SOURCE" value="EO-1_HYPERION" />
            <field name="MATE_TYPE" value="FILENAME" />
            <field name="MATE_ID_LEN" value="0026" />
            <field name="MATE_ID" value="EO1H1680372005097110PZ.L1R" />
          </group>
          <group index="1">
            <field name="SOURCE" value="EO-1_HYPERION" />
            <field name="MATE_TYPE" value="FILENAME" />
            <field name="MATE_ID_LEN" value="0026" />
            <field name="MATE_ID" value="EO1H1680372005097110PZ.AUX" />
          </group>
          <group index="2">
            <field name="SOURCE" value="EO-1_HYPERION" />
            <field name="MATE_TYPE" value="FILENAME" />
            <field name="MATE_ID_LEN" value="0026" />
            <field name="MATE_ID" value="EO1H1680372005097110PZ.MET" />
          </group>
        </repeated>
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing MATESA TRE (STDI-0002 App AK)

def test_nitf_77():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_77.ntf', 1, 1, options=['TRE=GRDPSB=01+000027.81PIX_LATLON0000000000010000000000010000000000000000000000'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_77.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_77.ntf')

    expected_data = """<tres>
  <tre name="GRDPSB" location="image">
    <field name="NUM_GRDS" value="01" />
    <repeated name="GRDS" number="1">
      <group index="0">
        <field name="ZVL" value="+000027.81" />
        <field name="BAD" value="PIX_LATLON" />
        <field name="LOD" value="000000000001" />
        <field name="LAD" value="000000000001" />
        <field name="LSO" value="00000000000" />
        <field name="PSO" value="00000000000" />
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing BANDSB TRE (STDI-0002 App X)

def test_nitf_78():
    float_data = "40066666" # == struct.pack(">f", 2.1).hex()
    bit_mask = "89800000" # Set bits 31, 27, 24, 23

    tre_data = "TRE=HEX/BANDSB=" + hex_string("00001RADIANCE                S") + float_data*2 + \
                hex_string("0030.00M0030.00M-------M-------M                                                ") + \
                bit_mask + hex_string("DETECTOR                ") + float_data + hex_string("U00.851920.01105")

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_78.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_78.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_78.ntf')

    expected_data = """<tres>
  <tre name="BANDSB" location="image">
    <field name="COUNT" value="00001" />
    <field name="RADIOMETRIC_QUANTITY" value="RADIANCE" />
    <field name="RADIOMETRIC_QUANTITY_UNIT" value="S" />
    <field name="SCALE_FACTOR" value="2.100000" />
    <field name="ADDITIVE_FACTOR" value="2.100000" />
    <field name="ROW_GSD" value="0030.00" />
    <field name="ROW_GSD_UNIT" value="M" />
    <field name="COL_GSD" value="0030.00" />
    <field name="COL_GSD_UNIT" value="M" />
    <field name="SPT_RESP_ROW" value="-------" />
    <field name="SPT_RESP_UNIT_ROW" value="M" />
    <field name="SPT_RESP_COL" value="-------" />
    <field name="SPT_RESP_UNIT_COL" value="M" />
    <field name="DATA_FLD_1" value="" />
    <field name="EXISTENCE_MASK" value="2306867200" />
    <field name="RADIOMETRIC_ADJUSTMENT_SURFACE" value="DETECTOR" />
    <field name="ATMOSPHERIC_ADJUSTMENT_ALTITUDE" value="2.100000" />
    <field name="WAVE_LENGTH_UNIT" value="U" />
    <repeated name="BANDS" number="1">
      <group index="0">
        <field name="BAD_BAND" value="0" />
        <field name="CWAVE" value="0.85192" />
        <field name="FWHM" value="0.01105" />
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing ACCHZB TRE (STDI-0002-1-v5.0 Appendix P)

def test_nitf_79():
    tre_data = "TRE=ACCHZB=01M  00129M  00129004+044.4130499724+33.69234401034+044.4945572008" \
               "+33.67855217830+044.1731373448+32.79106350687+044.2538103407+32.77733592314"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_79.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_79.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_79.ntf')

    expected_data = """<tres>
  <tre name="ACCHZB" location="image">
    <field name="NUM_ACHZ" value="01" />
    <repeated number="1">
      <group index="0">
        <field name="UNIAAH" value="M" />
        <field name="AAH" value="00129" />
        <field name="UNIAPH" value="M" />
        <field name="APH" value="00129" />
        <field name="NUM_PTS" value="004" />
        <repeated number="4">
          <group index="0">
            <field name="LON" value="+044.4130499724" />
            <field name="LAT" value="+33.69234401034" />
          </group>
          <group index="1">
            <field name="LON" value="+044.4945572008" />
            <field name="LAT" value="+33.67855217830" />
          </group>
          <group index="2">
            <field name="LON" value="+044.1731373448" />
            <field name="LAT" value="+32.79106350687" />
          </group>
          <group index="3">
            <field name="LON" value="+044.2538103407" />
            <field name="LAT" value="+32.77733592314" />
          </group>
        </repeated>
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing ACCVTB TRE (STDI-0002-1-v5.0 Appendix P)

def test_nitf_80():
    tre_data = "TRE=ACCVTB=01M  00095M  00095004+044.4130499724+33.69234401034+044.4945572008" \
               "+33.67855217830+044.1731373448+32.79106350687+044.2538103407+32.77733592314"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_80.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_80.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_80.ntf')

    expected_data = """<tres>
  <tre name="ACCVTB" location="image">
    <field name="NUM_ACVT" value="01" />
    <repeated number="1">
      <group index="0">
        <field name="UNIAAV" value="M" />
        <field name="AAV" value="00095" />
        <field name="UNIAPV" value="M" />
        <field name="APV" value="00095" />
        <field name="NUM_PTS" value="004" />
        <repeated number="4">
          <group index="0">
            <field name="LON" value="+044.4130499724" />
            <field name="LAT" value="+33.69234401034" />
          </group>
          <group index="1">
            <field name="LON" value="+044.4945572008" />
            <field name="LAT" value="+33.67855217830" />
          </group>
          <group index="2">
            <field name="LON" value="+044.1731373448" />
            <field name="LAT" value="+32.79106350687" />
          </group>
          <group index="3">
            <field name="LON" value="+044.2538103407" />
            <field name="LAT" value="+32.77733592314" />
          </group>
        </repeated>
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing MSTGTA TRE (STDI-0002-1-v5.0 App E)

def test_nitf_81():
    tre_data = "TRE=MSTGTA=012340123456789AB0123456789ABCDE0120123456789AB0123456789AB000123401234560123450TGT_LOC=             "
    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_81.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_81.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_81.ntf')

    expected_data = """<tres>
  <tre name="MSTGTA" location="image">
    <field name="TGT_NUM" value="01234" />
    <field name="TGT_ID" value="0123456789AB" />
    <field name="TGT_BE" value="0123456789ABCDE" />
    <field name="TGT_PRI" value="012" />
    <field name="TGT_REQ" value="0123456789AB" />
    <field name="TGT_LTIOV" value="0123456789AB" />
    <field name="TGT_TYPE" value="0" />
    <field name="TGT_COLL" value="0" />
    <field name="TGT_CAT" value="01234" />
    <field name="TGT_UTC" value="0123456" />
    <field name="TGT_ELEV" value="012345" />
    <field name="TGT_ELEV_UNIT" value="0" />
    <field name="TGT_LOC" value="TGT_LOC=" />
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing PIATGB TRE (STDI-0002-1-v5.0 App C)

def test_nitf_82():
    tre_data = "TRE=PIATGB=0123456789ABCDE0123456789ABCDE01012340123456789ABCDE012" \
               "TGTNAME=                              012+01.234567-012.345678"
    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_82.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_82.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_82.ntf')

    expected_data = """<tres>
  <tre name="PIATGB" location="image">
    <field name="TGTUTM" value="0123456789ABCDE" />
    <field name="PIATGAID" value="0123456789ABCDE" />
    <field name="PIACTRY" value="01" />
    <field name="PIACAT" value="01234" />
    <field name="TGTGEO" value="0123456789ABCDE" />
    <field name="DATUM" value="012" />
    <field name="TGTNAME" value="TGTNAME=" />
    <field name="PERCOVER" value="012" />
    <field name="TGTLAT" value="+01.234567" />
    <field name="TGTLON" value="-012.345678" />
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing PIXQLA TRE (STDI-0002-1-v5.0 App AA)

def test_nitf_83():
    tre_data = "TRE=PIXQLA=00100200031Dead                                    " \
               "Saturated                               Bad                                     "

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_83.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_83.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_83.ntf')

    expected_data = """<tres>
  <tre name="PIXQLA" location="image">
    <field name="NUMAIS" value="001" />
    <repeated number="1">
      <group index="0">
        <field name="AISDLVL" value="002" />
      </group>
    </repeated>
    <field name="NPIXQUAL" value="0003" />
    <field name="PQ_BIT_VALUE" value="1" />
    <repeated number="3">
      <group index="0">
        <field name="PQ_CONDITION" value="Dead" />
      </group>
      <group index="1">
        <field name="PQ_CONDITION" value="Saturated" />
      </group>
      <group index="2">
        <field name="PQ_CONDITION" value="Bad" />
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing PIXMTA TRE (STDI-0002-1-v5.0 App AJ)

def test_nitf_84():
    tre_data = "TRE=PIXMTA=0010020.00000000E+000.00000000E+001.00000000E+003.35200000E+03F00001P" \
               "BAND_WAVELENGTH                         micron                                  D00000"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_84.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_84.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_84.ntf')

    expected_data = """<tres>
  <tre name="PIXMTA" location="image">
    <field name="NUMAIS" value="001" />
    <repeated number="1">
      <group index="0">
        <field name="AISDLVL" value="002" />
      </group>
    </repeated>
    <field name="ORIGIN_X" value="0.00000000E+00" />
    <field name="ORIGIN_Y" value="0.00000000E+00" />
    <field name="SCALE_X" value="1.00000000E+00" />
    <field name="SCALE_Y" value="3.35200000E+03" />
    <field name="SAMPLE_MODE" value="F" />
    <field name="NUMMETRICS" value="00001" />
    <field name="PERBAND" value="P" />
    <repeated number="1">
      <group index="0">
        <field name="DESCRIPTION" value="BAND_WAVELENGTH" />
        <field name="UNIT" value="micron" />
        <field name="FITTYPE" value="D" />
      </group>
    </repeated>
    <field name="RESERVED_LEN" value="00000" />
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test creating a TRE with a hexadecimal string

def test_nitf_85():
    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_85.ntf', 1, 1, options=["TRE=HEX/TSTTRE=414243"])
    ds = None

    ds = gdal.Open('/vsimem/nitf_85.ntf')
    data = ds.GetMetadata('TRE')['TSTTRE']
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_85.ntf')

    expected_data = "ABC"
    assert data == expected_data

###############################################################################
# Test parsing CSEXRB TRE (STDI-0002-1-v5.0 App AH)

def test_nitf_86():
    tre_data = "TRE=HEX/CSEXRB=" + hex_string("824ecf8e-1041-4cce-9edb-bc92d88624ca0047308e4b1-80e4-4777-b70f-f6e4a6881de9") + \
               hex_string("17261ee9-2175-4ff2-86ad-dddda1f8270ccf306a0b-c47c-44fa-af63-463549f6bf98fd99a346-770e-4048-94d8-5a8b2e832b32") + \
               hex_string("EO-1  HYPERNHYPERNF+03819809.03+03731961.77+03475785.73000000000120201012145900.000000000") + \
               "0100000000000000" + "05" + "0000000100000001" "FFFFFFFFFF" + \
               hex_string("                                    1181.1                                               65535000335200256250.000") + \
               hex_string("             0000132.812+54.861             9991000000")

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_86.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_86.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_86.ntf')

    expected_data = """<tres>
  <tre name="CSEXRB" location="image">
    <field name="IMAGE_UUID" value="824ecf8e-1041-4cce-9edb-bc92d88624ca" />
    <field name="NUM_ASSOC_DES" value="004" />
    <repeated number="4">
      <group index="0">
        <field name="ASSOC_DES_ID" value="7308e4b1-80e4-4777-b70f-f6e4a6881de9" />
      </group>
      <group index="1">
        <field name="ASSOC_DES_ID" value="17261ee9-2175-4ff2-86ad-dddda1f8270c" />
      </group>
      <group index="2">
        <field name="ASSOC_DES_ID" value="cf306a0b-c47c-44fa-af63-463549f6bf98" />
      </group>
      <group index="3">
        <field name="ASSOC_DES_ID" value="fd99a346-770e-4048-94d8-5a8b2e832b32" />
      </group>
    </repeated>
    <field name="PLATFORM_ID" value="EO-1" />
    <field name="PAYLOAD_ID" value="HYPERN" />
    <field name="SENSOR_ID" value="HYPERN" />
    <field name="SENSOR_TYPE" value="F" />
    <field name="GROUND_REF_POINT_X" value="+03819809.03" />
    <field name="GROUND_REF_POINT_Y" value="+03731961.77" />
    <field name="GROUND_REF_POINT_Z" value="+03475785.73" />
    <field name="TIME_STAMP_LOC" value="0" />
    <field name="REFERENCE_FRAME_NUM" value="000000001" />
    <field name="BASE_TIMESTAMP" value="20201012145900.000000000" />
    <field name="DT_MULTIPLIER" value="72057594037927936" />
    <field name="DT_SIZE" value="5" />
    <field name="NUMBER_FRAMES" value="1" />
    <field name="NUMBER_DT" value="1" />
    <repeated number="1">
      <group index="0">
        <field name="DT" value="1099511627775" />
      </group>
    </repeated>
    <field name="MAX_GSD" value="" />
    <field name="ALONG_SCAN_GSD" value="" />
    <field name="CROSS_SCAN_GSD" value="" />
    <field name="GEO_MEAN_GSD" value="1181.1" />
    <field name="A_S_VERT_GSD" value="" />
    <field name="C_S_VERT_GSD" value="" />
    <field name="GEO_MEAN_VERT_GSD" value="" />
    <field name="GSD_BETA_ANGLE" value="" />
    <field name="DYNAMIC_RANGE" value="65535" />
    <field name="NUM_LINES" value="0003352" />
    <field name="NUM_SAMPLES" value="00256" />
    <field name="ANGLE_TO_NORTH" value="250.000" />
    <field name="OBLIQUITY_ANGLE" value="" />
    <field name="AZ_OF_OBLIQUITY" value="" />
    <field name="ATM_REFR_FLAG" value="0" />
    <field name="VEL_ABER_FLAG" value="0" />
    <field name="GRD_COVER" value="0" />
    <field name="SNOW_DEPTH_CATEGORY" value="0" />
    <field name="SUN_AZIMUTH" value="132.812" />
    <field name="SUN_ELEVATION" value="+54.861" />
    <field name="PREDICTED_NIIRS" value="" />
    <field name="CIRCL_ERR" value="" />
    <field name="LINEAR_ERR" value="" />
    <field name="CLOUD_COVER" value="999" />
    <field name="ROLLING_SHUTTER_FLAG" value="1" />
    <field name="UE_TIME_FLAG" value="0" />
    <field name="RESERVED_LEN" value="00000" />
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing ILLUMB TRE (STDI-0002-1-v5.0 App AL)

def test_nitf_87():
    mu = "B5"   # \mu per ISO-8859-1
    bit_mask = "7A0000"
    tre_data = "TRE=HEX/ILLUMB=" + hex_string("0001") + \
        mu + hex_string("m                                      8.5192000000E-01") + \
        hex_string("2.5770800000E+00001NUM_BANDS=1 because ILLUMB has no band-dependent content                        ") + \
        hex_string("World Geodetic System 1984                                                      ") + \
        hex_string("WGE World Geodetic System 1984                                                      ") + \
        hex_string("WE Geodetic                                                                        ") + \
        hex_string("GEOD") + \
        bit_mask + hex_string("00120050407072410+33.234974+044.333405+27.8100000E+0132.8+54.9167.5+52.5") + \
        hex_string("-163.4004099.2+84.0")

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_87.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_87.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_87.ntf')

    expected_data = """<tres>
  <tre name="ILLUMB" location="image">
    <field name="NUM_BANDS" value="0001" />
    <field name="BAND_UNIT" value="µm" />
    <repeated number="1">
      <group index="0">
        <field name="LBOUND" value="8.5192000000E-01" />
        <field name="UBOUND" value="2.5770800000E+00" />
      </group>
    </repeated>
    <field name="NUM_OTHERS" value="00" />
    <field name="NUM_COMS" value="1" />
    <repeated number="1">
      <group index="0">
        <field name="COMMENT" value="NUM_BANDS=1 because ILLUMB has no band-dependent content" />
      </group>
    </repeated>
    <field name="GEO_DATUM" value="World Geodetic System 1984" />
    <field name="GEO_DATUM_CODE" value="WGE" />
    <field name="ELLIPSOID_NAME" value="World Geodetic System 1984" />
    <field name="ELLIPSOID_CODE" value="WE" />
    <field name="VERTICAL_DATUM_REF" value="Geodetic" />
    <field name="VERTICAL_REF_CODE" value="GEOD" />
    <field name="EXISTENCE_MASK" value="7995392" />
    <field name="NUM_ILLUM_SETS" value="001" />
    <repeated number="1">
      <group index="0">
        <field name="DATETIME" value="20050407072410" />
        <field name="TARGET_LAT" value="+33.234974" />
        <field name="TARGET_LON" value="+044.333405" />
        <field name="TARGET_HGT" value="+27.8100000E+0" />
        <field name="SUN_AZIMUTH" value="132.8" />
        <field name="SUN_ELEV" value="+54.9" />
        <field name="MOON_AZIMUTH" value="167.5" />
        <field name="MOON_ELEV" value="+52.5" />
        <field name="MOON_PHASE_ANGLE" value="-163.4" />
        <field name="MOON_ILLUM_PERCENT" value="004" />
        <field name="SENSOR_AZIMUTH" value="099.2" />
        <field name="SENSOR_ELEV" value="+84.0" />
        <repeated number="1">
          <group index="0" />
        </repeated>
      </group>
    </repeated>
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing CSWRPB TRE (STDI-0002-1-v5.0 App AH)

def test_nitf_88():
    tre_data = "TRE=CSWRPB=1F199.9999999900000010000002000000300000040000005000000600000070000008" \
               "1111-9.99999999999999E-99+9.99999999999999E+9900000"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_88.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_88.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_88.ntf')

    expected_data = """<tres>
  <tre name="CSWRPB" location="image">
    <field name="NUM_SETS_WARP_DATA" value="1" />
    <field name="SENSOR_TYPE" value="F" />
    <field name="WRP_INTERP" value="1" />
    <repeated number="1">
      <group index="0">
        <field name="FL_WARP" value="99.99999999" />
        <field name="OFFSET_LINE" value="0000001" />
        <field name="OFFSET_SAMP" value="0000002" />
        <field name="SCALE_LINE" value="0000003" />
        <field name="SCALE_SAMP" value="0000004" />
        <field name="OFFSET_LINE_UNWRP" value="0000005" />
        <field name="OFFSET_SAMP_UNWRP" value="0000006" />
        <field name="SCALE_LINE_UNWRP" value="0000007" />
        <field name="SCALE_SAMP_UNWRP" value="0000008" />
        <field name="LINE_POLY_ORDER_M1" value="1" />
        <field name="LINE_POLY_ORDER_M2" value="1" />
        <field name="SAMP_POLY_ORDER_N1" value="1" />
        <field name="SAMP_POLY_ORDER_N2" value="1" />
        <repeated number="1">
          <group index="0">
            <repeated number="1">
              <group index="0">
                <field name="A" value="-9.99999999999999E-99" />
              </group>
            </repeated>
          </group>
        </repeated>
        <repeated number="1">
          <group index="0">
            <repeated number="1">
              <group index="0">
                <field name="B" value="+9.99999999999999E+99" />
              </group>
            </repeated>
          </group>
        </repeated>
      </group>
    </repeated>
    <field name="RESERVED_LEN" value="00000" />
  </tre>
</tres>
"""
    assert data == expected_data

###############################################################################
# Test parsing CSRLSB TRE (STDI-0002-1-v5.0 App AH)

def test_nitf_89():
    tre_data = "TRE=CSRLSB=0101+11111111.11-22222222.22+33333333.33-44444444.44"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_89.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_89.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_89.ntf')

    expected_data = """<tres>
  <tre name="CSRLSB" location="image">
    <field name="N_RS_ROW_BLOCKS" value="01" />
    <field name="M_RS_COLUMN_BLOCKS" value="01" />
    <repeated number="1">
      <group index="0">
        <repeated number="1">
          <group index="0">
            <field name="RS_DT_1" value="+11111111.11" />
            <field name="RS_DT_2" value="-22222222.22" />
            <field name="RS_DT_3" value="+33333333.33" />
            <field name="RS_DT_4" value="-44444444.44" />
          </group>
        </repeated>
      </group>
    </repeated>
  </tre>
</tres>
"""

    assert data == expected_data

###############################################################################
# Test parsing SECURA TRE (STDI-0002-1-v5.0 App AI)

def test_nitf_90():
    tre_data = "FILE_TRE=SECURA=20201020142500NITF02.10" + " "*207 + "ARH.XML         00068" + \
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?> <arh:Security></arh:Security>"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_90.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_90.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_90.ntf')

    expected_data = """<tres>
  <tre name="SECURA" location="file">
    <field name="FDATTIM" value="20201020142500" />
    <field name="NITFVER" value="NITF02.10" />
    <field name="NFSECFLDS" value="" />
    <field name="SECSTD" value="ARH.XML" />
    <field name="SECCOMP" value="" />
    <field name="SECLEN" value="00068" />
    <field name="SECURITY" value="&lt;?xml version=&quot;1.0&quot; encoding=&quot;UTF-8&quot;?&gt; &lt;arh:Security&gt;&lt;/arh:Security&gt;" />
  </tre>
</tres>
"""

    assert data == expected_data

###############################################################################
# Test parsing SNSPSB TRE (STDI-0002-1-v5.0 App P)

def test_nitf_91():
    tre_data = "TRE=SNSPSB=010001111112222233333M  000001000001000001000001GSL         " + \
               "PLTFM   INS     MOD PRL  SID       ACT               DEG0000001      +111111.11-222222.22" + \
               "         meters 000000000000000000000011111111111111111111112222222222222222222222001" + \
               "API                 Imeters 0123456789"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_91.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_91.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_91.ntf')

    expected_data = """<tres>
  <tre name="SNSPSB" location="image">
    <field name="NUM_SNS" value="01" />
    <repeated number="1">
      <group index="0">
        <field name="NUM_BP" value="00" />
        <field name="NUM_BND" value="01" />
        <repeated number="1">
          <group index="0">
            <field name="BID" value="11111" />
            <field name="WS1" value="22222" />
            <field name="WS2" value="33333" />
          </group>
        </repeated>
        <field name="UNIRES" value="M" />
        <field name="REX" value="000001" />
        <field name="REY" value="000001" />
        <field name="GSX" value="000001" />
        <field name="GSY" value="000001" />
        <field name="GSL" value="GSL" />
        <field name="PLTFM" value="PLTFM" />
        <field name="INS" value="INS" />
        <field name="MOD" value="MOD" />
        <field name="PRL" value="PRL" />
        <field name="SID" value="SID" />
        <field name="ACT" value="ACT" />
        <field name="UNINOA" value="DEG" />
        <field name="NOA" value="0000001" />
        <field name="UNIANG" value="" />
        <field name="UNIALT" value="" />
        <field name="LONSCC" value="+111111.11" />
        <field name="LATSCC" value="-222222.22" />
        <field name="UNISAE" value="" />
        <field name="UNIRPY" value="" />
        <field name="UNIPXT" value="" />
        <field name="UNISPE" value="meters" />
        <field name="ROS" value="0000000000000000000000" />
        <field name="PIS" value="1111111111111111111111" />
        <field name="YAS" value="2222222222222222222222" />
        <field name="NUM_AUX" value="001" />
        <repeated number="1">
          <group index="0">
            <field name="API" value="API" />
            <field name="APF" value="I" />
            <field name="UNIAPX" value="meters" />
            <field name="APN" value="0123456789" />
          </group>
        </repeated>
      </group>
    </repeated>
  </tre>
</tres>
"""

    assert data == expected_data

###############################################################################
# Test parsing RSMAPB TRE (STDI-0002-1-v5.0 App U)

def test_nitf_RSMAPB():

    tre_data = "TRE=RSMAPB=iid                                                                             " + \
        "edition                                 tid                                     01IG+9.99999999999999E+99" + \
        "+9.99999999999999E+99+9.99999999999999E+99+9.99999999999999E+99+9.99999999999999E+99+9.99999999999999E+99" + \
        "Y01011230001+9.99999999999999E+99+9.99999999999999E+99"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_RSMAPB.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_RSMAPB.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_RSMAPB.ntf')

    expected_data = """<tres>
  <tre name="RSMAPB" location="image">
    <field name="IID" value="iid" />
    <field name="EDITION" value="edition" />
    <field name="TID" value="tid" />
    <field name="NPAR" value="01" />
    <field name="APTYP" value="I" />
    <field name="LOCTYP" value="G" />
    <field name="NSFX" value="+9.99999999999999E+99" />
    <field name="NSFY" value="+9.99999999999999E+99" />
    <field name="NSFZ" value="+9.99999999999999E+99" />
    <field name="NOFFX" value="+9.99999999999999E+99" />
    <field name="NOFFY" value="+9.99999999999999E+99" />
    <field name="NOFFZ" value="+9.99999999999999E+99" />
    <field name="APBASE" value="Y" />
    <field name="NISAP" value="01" />
    <field name="NISAPR" value="01" />
    <repeated number="1">
      <group index="0">
        <field name="XPWRR" value="1" />
        <field name="YPWRR" value="2" />
        <field name="ZPWRR" value="3" />
      </group>
    </repeated>
    <field name="NISAPC" value="00" />
    <field name="NBASIS" value="01" />
    <repeated number="1">
      <group index="0">
        <repeated number="1">
          <group index="0">
            <field name="AEL" value="+9.99999999999999E+99" />
          </group>
        </repeated>
      </group>
    </repeated>
    <repeated number="1">
      <group index="0">
        <field name="PARVAL" value="+9.99999999999999E+99" />
      </group>
    </repeated>
  </tre>
</tres>
"""

    assert data == expected_data

###############################################################################
# Test parsing RSMDCB TRE (STDI-0002-1-v5.0 App U)

def test_nitf_RSMDCB():

    tre_data = "TRE=RSMDCB=iid                                                                             " + \
        "edition                                 tid                                     01001iidi" + " "*76 + \
        "01Y01GN" + "+9.99999999999999E+99"*6 + "N01ABCD+9.99999999999999E+99"

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_RSMDCB.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_RSMDCB.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_RSMDCB.ntf')

    expected_data = """<tres>
  <tre name="RSMDCB" location="image">
    <field name="IID" value="iid" />
    <field name="EDITION" value="edition" />
    <field name="TID" value="tid" />
    <field name="NROWCB" value="01" />
    <field name="NIMGE" value="001" />
    <repeated number="1">
      <group index="0">
        <field name="IIDI" value="iidi" />
        <field name="NCOLCB" value="01" />
      </group>
    </repeated>
    <field name="INCAPD" value="Y" />
    <field name="NPAR" value="01" />
    <field name="APTYP" value="G" />
    <field name="LOCTYP" value="N" />
    <field name="NSFX" value="+9.99999999999999E+99" />
    <field name="NSFY" value="+9.99999999999999E+99" />
    <field name="NSFZ" value="+9.99999999999999E+99" />
    <field name="NOFFX" value="+9.99999999999999E+99" />
    <field name="NOFFY" value="+9.99999999999999E+99" />
    <field name="NOFFZ" value="+9.99999999999999E+99" />
    <field name="APBASE" value="N" />
    <field name="NGSAP" value="01" />
    <repeated number="1">
      <group index="0">
        <field name="GSAPID" value="ABCD" />
      </group>
    </repeated>
    <repeated number="1">
      <group index="0">
        <repeated number="1">
          <group index="0">
            <repeated number="1">
              <group index="0">
                <field name="CRSCOV" value="+9.99999999999999E+99" />
              </group>
            </repeated>
          </group>
        </repeated>
      </group>
    </repeated>
  </tre>
</tres>
"""

    assert data == expected_data

###############################################################################
# Test parsing RSMECB TRE (STDI-0002-1-v5.0 App U)

def test_nitf_RSMECB():

    tre_data = "TRE=RSMECB=iid                                                                             " + \
        "edition                                 tid                                     " + \
        "YY01012020110201GN" + "+9.99999999999999E+99"*6 + "N01ABCD02" + "+9.99999999999999E+99"*3 + \
        "1N2" + "+9.99999999999999E+99"*8 + "N2" + "+9.99999999999999E+99"*4 + "2" + "+9.99999999999999E+99"*4

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_RSMECB.ntf', 1, 1, options=[tre_data])
    ds = None

    ds = gdal.Open('/vsimem/nitf_RSMECB.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_RSMECB.ntf')

    expected_data = """<tres>
  <tre name="RSMECB" location="image">
    <field name="IID" value="iid" />
    <field name="EDITION" value="edition" />
    <field name="TID" value="tid" />
    <field name="INCLIC" value="Y" />
    <field name="INCLUC" value="Y" />
    <field name="NPARO" value="01" />
    <field name="IGN" value="01" />
    <field name="CVDATE" value="20201102" />
    <field name="NPAR" value="01" />
    <field name="APTYP" value="G" />
    <field name="LOCTYP" value="N" />
    <field name="NSFX" value="+9.99999999999999E+99" />
    <field name="NSFY" value="+9.99999999999999E+99" />
    <field name="NSFZ" value="+9.99999999999999E+99" />
    <field name="NOFFX" value="+9.99999999999999E+99" />
    <field name="NOFFY" value="+9.99999999999999E+99" />
    <field name="NOFFZ" value="+9.99999999999999E+99" />
    <field name="APBASE" value="N" />
    <field name="NGSAP" value="01" />
    <repeated number="1">
      <group index="0">
        <field name="GSAPID" value="ABCD" />
      </group>
    </repeated>
    <repeated number="1">
      <group index="0">
        <field name="NUMOPG" value="02" />
        <repeated number="3">
          <group index="0">
            <field name="ERRCVG" value="+9.99999999999999E+99" />
          </group>
          <group index="1">
            <field name="ERRCVG" value="+9.99999999999999E+99" />
          </group>
          <group index="2">
            <field name="ERRCVG" value="+9.99999999999999E+99" />
          </group>
        </repeated>
        <field name="TCDF" value="1" />
        <field name="ACSMC" value="N" />
        <field name="NCSEG" value="2" />
        <repeated number="2">
          <group index="0">
            <field name="CORSEG" value="+9.99999999999999E+99" />
            <field name="TAUSEG" value="+9.99999999999999E+99" />
          </group>
          <group index="1">
            <field name="CORSEG" value="+9.99999999999999E+99" />
            <field name="TAUSEG" value="+9.99999999999999E+99" />
          </group>
        </repeated>
      </group>
    </repeated>
    <repeated number="1">
      <group index="0">
        <repeated number="1">
          <group index="0">
            <field name="MAP" value="+9.99999999999999E+99" />
          </group>
        </repeated>
      </group>
    </repeated>
    <field name="URR" value="+9.99999999999999E+99" />
    <field name="URC" value="+9.99999999999999E+99" />
    <field name="UCC" value="+9.99999999999999E+99" />
    <field name="UACSMC" value="N" />
    <field name="UNCSR" value="2" />
    <repeated number="2">
      <group index="0">
        <field name="UCORSR" value="+9.99999999999999E+99" />
        <field name="UTAUSR" value="+9.99999999999999E+99" />
      </group>
      <group index="1">
        <field name="UCORSR" value="+9.99999999999999E+99" />
        <field name="UTAUSR" value="+9.99999999999999E+99" />
      </group>
    </repeated>
    <field name="UNCSC" value="2" />
    <repeated number="2">
      <group index="0">
        <field name="UCORSC" value="+9.99999999999999E+99" />
        <field name="UTAUSC" value="+9.99999999999999E+99" />
      </group>
      <group index="1">
        <field name="UCORSC" value="+9.99999999999999E+99" />
        <field name="UTAUSC" value="+9.99999999999999E+99" />
      </group>
    </repeated>
  </tre>
</tres>
"""

    assert data == expected_data

###############################################################################
# Test creation and reading of Data Extension Segments (DES)

def test_nitf_des():
    des_data = "02U" + " "*166 + r'0004ABCD1234567\0890'

    ds = gdal.GetDriverByName("NITF").Create("/vsimem/nitf_DES.ntf", 1, 1, options=["DES=DES1=" + des_data, "DES=DES2=" + des_data])
    ds = None

    # DESDATA portion will be Base64 encoded on output
    # base64.b64encode(bytes("1234567\x00890", "utf-8")) == b'MTIzNDU2NwA4OTA='
    ds = gdal.Open("/vsimem/nitf_DES.ntf")
    data = ds.GetMetadata("xml:DES")[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_DES.ntf')

    expected_data = """<des_list>
  <des name="DES1">
    <field name="DESVER" value="02" />
    <field name="DECLAS" value="U" />
    <field name="DESCLSY" value="" />
    <field name="DESCODE" value="" />
    <field name="DESCTLH" value="" />
    <field name="DESREL" value="" />
    <field name="DESDCTP" value="" />
    <field name="DESDCDT" value="" />
    <field name="DESDCXM" value="" />
    <field name="DESDG" value="" />
    <field name="DESDGDT" value="" />
    <field name="DESCLTX" value="" />
    <field name="DESCATP" value="" />
    <field name="DESCAUT" value="" />
    <field name="DESCRSN" value="" />
    <field name="DESSRDT" value="" />
    <field name="DESCTLN" value="" />
    <field name="DESSHL" value="0004" />
    <field name="DESSHF" value="ABCD" />
    <field name="DESDATA" value="MTIzNDU2NwA4OTA=" />
  </des>
  <des name="DES2">
    <field name="DESVER" value="02" />
    <field name="DECLAS" value="U" />
    <field name="DESCLSY" value="" />
    <field name="DESCODE" value="" />
    <field name="DESCTLH" value="" />
    <field name="DESREL" value="" />
    <field name="DESDCTP" value="" />
    <field name="DESDCDT" value="" />
    <field name="DESDCXM" value="" />
    <field name="DESDG" value="" />
    <field name="DESDGDT" value="" />
    <field name="DESCLTX" value="" />
    <field name="DESCATP" value="" />
    <field name="DESCAUT" value="" />
    <field name="DESCRSN" value="" />
    <field name="DESSRDT" value="" />
    <field name="DESCTLN" value="" />
    <field name="DESSHL" value="0004" />
    <field name="DESSHF" value="ABCD" />
    <field name="DESDATA" value="MTIzNDU2NwA4OTA=" />
  </des>
</des_list>
"""

    assert data == expected_data

###############################################################################
# Test creation and reading of Data Extension Segments (DES)

def test_nitf_des_CSSHPA():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/tmp.shp')
    lyr = ds.CreateLayer('tmp', geom_type = ogr.wkbPolygon, options = ['DBF_DATE_LAST_UPDATE=2021-01-01'])
    lyr.CreateField(ogr.FieldDefn("ID", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("ID", 1)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((2 49,2 50,3 50,3 49,2 49))'))
    lyr.CreateFeature(f)
    ds = None

    files = {}
    for ext in ('shp', 'shx', 'dbf'):
        f = gdal.VSIFOpenL('/vsimem/tmp.' + ext, 'rb')
        files[ext] = gdal.VSIFReadL(1, 1000000, f)
        gdal.VSIFCloseL(f)
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/tmp.shp')

    shp_offset = 0
    shx_offset = shp_offset + len(files['shp'])
    dbf_offset = shx_offset + len(files['shx'])
    shp_shx_dbf = files['shp'] + files['shx'] + files['dbf']

    des_data = b"02U" + b" "*166 + ('0080CLOUD_SHAPES             POLYGON   SOURCE123456789ABCSHP%06dSHX%06dDBF%06d' % (shp_offset, shx_offset, dbf_offset)).encode('ascii') + shp_shx_dbf

    escaped_data = gdal.EscapeString(des_data, gdal.CPLES_BackslashQuotable)

    ds = gdal.GetDriverByName("NITF").Create("/vsimem/nitf_DES.ntf", 1, 1, options=[b"DES=CSSHPA DES=" + escaped_data])
    ds = None

    ds = gdal.Open("/vsimem/nitf_DES.ntf")
    data = ds.GetMetadata("xml:DES")[0]
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/nitf_DES.ntf')

    expected_data = """<des_list>
  <des name="CSSHPA DES">
    <field name="DESVER" value="02" />
    <field name="DECLAS" value="U" />
    <field name="DESCLSY" value="" />
    <field name="DESCODE" value="" />
    <field name="DESCTLH" value="" />
    <field name="DESREL" value="" />
    <field name="DESDCTP" value="" />
    <field name="DESDCDT" value="" />
    <field name="DESDCXM" value="" />
    <field name="DESDG" value="" />
    <field name="DESDGDT" value="" />
    <field name="DESCLTX" value="" />
    <field name="DESCATP" value="" />
    <field name="DESCAUT" value="" />
    <field name="DESCRSN" value="" />
    <field name="DESSRDT" value="" />
    <field name="DESCTLN" value="" />
    <field name="DESSHL" value="0080" />
    <field name="DESSHF" value="CLOUD_SHAPES             POLYGON   SOURCE123456789ABCSHP000000SHX000236DBF000344">
      <user_defined_fields>
        <field name="SHAPE_USE" value="CLOUD_SHAPES" />
        <field name="SHAPE_CLASS" value="POLYGON" />
        <field name="CC_SOURCE" value="SOURCE123456789ABC" />
        <field name="SHAPE1_NAME" value="SHP" />
        <field name="SHAPE1_START" value="000000" />
        <field name="SHAPE2_NAME" value="SHX" />
        <field name="SHAPE2_START" value="000236" />
        <field name="SHAPE3_NAME" value="DBF" />
        <field name="SHAPE3_START" value="000344" />
      </user_defined_fields>
    </field>
    <field name="DESDATA" value="%s" />
  </des>
</des_list>
""" % base64.b64encode(shp_shx_dbf).decode('ascii')

    assert data == expected_data

###############################################################################
# Test reading/writing headers in ISO-8859-1 encoding
def test_nitf_header_encoding():
    # mu character encoded in UTF-8
    test_char = b'\xc2\xb5'.decode("utf-8")
    ds = gdal.GetDriverByName('NITF').Create('/vsimem/header_encoding.ntf', 1, 1,
        options=["FTITLE=" + test_char, "IID2=" + test_char, "ICOM=" + test_char*80*9])
    ds = None

    ds = gdal.Open('/vsimem/header_encoding.ntf')
    md_binary = ds.GetMetadata("NITF_METADATA")
    md = ds.GetMetadata()

    ds = None
    gdal.GetDriverByName('NITF').Delete('/vsimem/header_encoding.ntf')

    file_header = md_binary["NITFFileHeader"].split()[1]
    file_header = base64.b64decode(file_header)

    # mu character encoded in ISO-8859-1 located at FTITLE position
    assert file_header[39:40] == b'\xb5'

    image_header = md_binary["NITFImageSubheader"].split()[1]
    image_header = base64.b64decode(image_header)

    # mu character encoded in ISO-8859-1 located at IID2 position and ICOM position
    assert image_header[43:44] == b'\xb5'
    assert image_header[373:1093] == b'\xb5'*80*9

    # mu character recoded to UTF-8 in string metadata
    assert md["NITF_FTITLE"] == test_char
    assert md["NITF_IID2"] == test_char
    assert md["NITF_IMAGE_COMMENTS"] == test_char*80*9

###############################################################################
# Test reading C4 compressed file


def test_nitf_read_C4():

    ds = gdal.Open('data/nitf/RPFTOC01.ON2')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 53599

###############################################################################
# Test reading a file with a SENSRB TRE


def test_nitf_SENSRB():

    ds = gdal.Open('data/nitf/SENSRB_TRE.ntf')
    data = ds.GetMetadata('xml:TRE')[0]
    ds = None

    expected_data = """<tres>
  <tre name="SENSRB" location="image">
    <field name="GENERAL_DATA" value="Y" />
    <field name="SENSOR" value="" />
    <field name="SENSOR_URI" value="" />
    <field name="PLATFORM" value="                      UMS" />
    <field name="PLATFORM_URI" value="" />
    <field name="OPERATION_DOMAIN" value="" />
    <field name="CONTENT_LEVEL" value="4" />
    <field name="GEODETIC_SYSTEM" value="" />
    <field name="GEODETIC_TYPE" value="" />
    <field name="ELEVATION_DATUM" value="" />
    <field name="LENGTH_UNIT" value=" m" />
    <field name="ANGULAR_UNIT" value="deg" />
    <field name="START_DATE" value="" />
    <field name="START_TIME" value="00000000000000" />
    <field name="END_DATE" value="20190507" />
    <field name="END_TIME" value="0000084.059869" />
    <field name="GENERATION_COUNT" value="00" />
    <field name="GENERATION_DATE" value="" />
    <field name="GENERATION_TIME" value="" />
    <field name="SENSOR_ARRAY_DATA" value="" />
    <field name="SENSOR_CALIBRATION_DATA" value="" />
    <field name="IMAGE_FORMATION_DATA" value="Y" />
    <field name="METHOD" value="" />
    <field name="MODE" value="" />
    <field name="ROW_COUNT" value="00000000" />
    <field name="COLUMN_COUNT" value="00000000" />
    <field name="ROW_SET" value="00000000" />
    <field name="COLUMN_SET" value="00000000" />
    <field name="ROW_RATE" value="0000000000" />
    <field name="COLUMN_RATE" value="0000000000" />
    <field name="FIRST_PIXEL_ROW" value="00000000" />
    <field name="FIRST_PIXEL_COLUMN" value="00000000" />
    <field name="TRANSFORM_PARAMS" value="3" />
    <repeated name="TRANSFORM_PARAM" number="3">
      <group index="0">
        <field name="TRANSFORM_PARAM" value="         470" />
      </group>
      <group index="1">
        <field name="TRANSFORM_PARAM" value="         471" />
      </group>
      <group index="2">
        <field name="TRANSFORM_PARAM" value="         472" />
      </group>
    </repeated>
    <field name="REFERENCE_TIME" value="" />
    <field name="REFERENCE_ROW" value="" />
    <field name="REFERENCE_COLUMN" value="" />
    <field name="LATITUDE_OR_X" value="   43643267" />
    <field name="LONGITUDE_OR_Y" value="" />
    <field name="ALTITUDE_OR_Z" value="" />
    <field name="SENSOR_X_OFFSET" value="00000000" />
    <field name="SENSOR_Y_OFFSET" value="00000000" />
    <field name="SENSOR_Z_OFFSET" value="00000000" />
    <field name="ATTITUDE_EULER_ANGLES" value="" />
    <field name="ATTITUDE_UNIT_VECTORS" value="" />
    <field name="ATTITUDE_QUATERNION" value="" />
    <field name="SENSOR_VELOCITY_DATA" value="" />
    <field name="POINT_SET_DATA" value="00" />
    <field name="TIME_STAMPED_DATA_SETS" value="02" />
    <repeated name="TIME_STAMPED_SET" number="2">
      <group index="0">
        <field name="TIME_STAMP_TYPE_MM" value="06b" />
        <field name="TIME_STAMP_COUNT_MM" value="0003" />
        <repeated name="TIME_STAMP_COUNTS" number="3">
          <group index="0">
            <field name="TIME_STAMP_TIME_NNNN" value="111111111111" />
            <field name="TIME_STAMP_VALUE_NNNN" value="111100001111" />
          </group>
          <group index="1">
            <field name="TIME_STAMP_TIME_NNNN" value="222222222222" />
            <field name="TIME_STAMP_VALUE_NNNN" value="222200001111" />
          </group>
          <group index="2">
            <field name="TIME_STAMP_TIME_NNNN" value="333333333333" />
            <field name="TIME_STAMP_VALUE_NNNN" value="333300001111" />
          </group>
        </repeated>
      </group>
      <group index="1">
        <field name="TIME_STAMP_TYPE_MM" value="06e" />
        <field name="TIME_STAMP_COUNT_MM" value="0002" />
        <repeated name="TIME_STAMP_COUNTS" number="2">
          <group index="0">
            <field name="TIME_STAMP_TIME_NNNN" value="444444444444" />
            <field name="TIME_STAMP_VALUE_NNNN" value="44440000" />
          </group>
          <group index="1">
            <field name="TIME_STAMP_TIME_NNNN" value="555555555555" />
            <field name="TIME_STAMP_VALUE_NNNN" value="55550000" />
          </group>
        </repeated>
      </group>
    </repeated>
    <field name="PIXEL_REFERENCED_DATA_SETS" value="00" />
    <field name="UNCERTAINTY_DATA" value="000" />
    <field name="ADDITIONAL_PARAMETER_DATA" value="000" />
  </tre>
</tres>
"""

    assert data == expected_data, data

###############################################################################
# Verify we can read UDID metadata

def test_nitf_valid_udid():

    ds = gdal.Open('data/nitf/valid_udid.ntf')

    md = ds.GetMetadata()
    assert md['NITF_CSDIDA_YEAR'] == '2019', \
        'UDID CSDIDA metadata has unexpected value.'

    assert md['NITF_BLOCKA_BLOCK_INSTANCE_01'] == '01', \
        'BLOCKA metadata has unexpected value.'

###############################################################################
# Verify that bad UDID metadata doesn't prevent reading IXSHD metadata

def test_nitf_invalid_udid():

    ds = gdal.Open('data/nitf/invalid_udid.ntf')

    md = ds.GetMetadata()
    assert 'NITF_CSDIDA_YEAR' not in md, \
        'Unexpected parings of UDID CSDIDA metadata.'

    assert md['NITF_BLOCKA_BLOCK_INSTANCE_01'] == '01', \
        'BLOCKA metadata has unexpected value.'

###############################################################################
# Verify ISUBCAT is present when non-empty.

def test_nitf_isubcat_populated():

    # Check a dataset with IQ complex data.
    ds = gdal.Open('data/nitf/sar_sicd.ntf')
    expected = ["I", "Q"]
    for b in range(ds.RasterCount):
        md = ds.GetRasterBand(b + 1).GetMetadata()
        assert md["NITF_ISUBCAT"] == expected[b]

    # Check a dataset with an empty ISUBCAT.
    ds = gdal.Open('data/nitf/rgb.ntf')
    for b in range(ds.RasterCount):
        md = ds.GetRasterBand(b + 1).GetMetadata()
        assert "NITF_ISUBCAT" not in md


###############################################################################
# Test limits on creation


def test_nitf_create_too_large_file():

    # Test 1e10 byte limit for a single image
    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').Create('/vsimem/out.ntf', int(1e5), int(1e5))
    assert gdal.GetLastErrorMsg() != ''

    # Test 1e12 byte limit for while file
    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.GetDriverByName('NITF').Create('/vsimem/out.ntf', int(1e5), int(1e5) // 2,
                                            options = ['NUMI=200', 'WRITE_ALL_IMAGES=YES'])
    assert gdal.GetLastErrorMsg() != ''

    gdal.Unlink('/vsimem/out.ntf')


###############################################################################
# Test creating file with multiple image segments

def test_nitf_create_two_images_final_with_C3_compression():

    gdal.Unlink('/vsimem/out.ntf')

    src_ds = gdal.Open('data/rgbsmall.tif')

    # Write first image segment, reserve space for a second one and a DES
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/out.ntf', src_ds,
                                                 options=['NUMI=2',
                                                          'NUMDES=1'])
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    # Write second and final image segment and DES
    des_data = "02U" + " "*166 + r'0004ABCD1234567\0890'
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/out.ntf', src_ds,
                                                 options=['APPEND_SUBDATASET=YES',
                                                          'IC=C3',
                                                          'IDLVL=2',
                                                          "DES=DES1=" + des_data])
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    ds = gdal.Open('/vsimem/out.ntf')
    assert ds.GetMetadata("xml:DES") is not None
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Open('NITF_IM:1:/vsimem/out.ntf')
    (exp_mean, exp_stddev) = (65.9532, 46.9026375565)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    assert exp_mean == pytest.approx(mean, abs=0.1) and exp_stddev == pytest.approx(stddev, abs=0.1), \
        'did not get expected mean or standard dev.'
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/out.ntf')


###############################################################################
# Test creating file with multiple image segments

def test_nitf_create_three_images_final_uncompressed():

    gdal.Unlink('/vsimem/out.ntf')

    src_ds = gdal.Open('data/rgbsmall.tif')

    # Write first image segment, reserve space for two other ones and a DES
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/out.ntf', src_ds,
                                                 options=['NUMI=3',
                                                          'NUMDES=1'])
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    # Write second image segment
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/out.ntf', src_ds,
                                                 options=['APPEND_SUBDATASET=YES',
                                                          'IC=C3',
                                                          'IDLVL=2'])
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    # Write third and final image segment and DES
    des_data = "02U" + " "*166 + r'0004ABCD1234567\0890'
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/out.ntf', src_ds,
                                                 options=['APPEND_SUBDATASET=YES',
                                                          'IDLVL=3',
                                                          "DES=DES1=" + des_data])
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    ds = gdal.Open('/vsimem/out.ntf')
    assert ds.GetMetadata("xml:DES") is not None
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Open('NITF_IM:1:/vsimem/out.ntf')
    (exp_mean, exp_stddev) = (65.9532, 46.9026375565)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()

    assert exp_mean == pytest.approx(mean, abs=0.1) and exp_stddev == pytest.approx(stddev, abs=0.1), \
        'did not get expected mean or standard dev.'

    ds = gdal.Open('NITF_IM:2:/vsimem/out.ntf')
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.GetDriverByName('NITF').Delete('/vsimem/out.ntf')

###############################################################################
# Test writing/reading PAM metadata


def test_nitf_pam_metadata_single_image():

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_filename = 'tmp/test_nitf_pam_metadata_single_image.ntf'
    gdal.ErrorReset()
    gdal.GetDriverByName('NITF').CreateCopy(out_filename, src_ds)
    assert gdal.GetLastErrorType() == 0
    ds = None

    assert os.path.exists(out_filename + '.aux.xml')
    pam = open(out_filename + '.aux.xml', 'rb').read()
    assert b'<Subdataset' not in pam

    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Area'
    ds = None

    ds = gdal.Open('NITF_IM:0:' + out_filename)
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Area'
    ds = None

    # Try to read the variant of PAM serialization that was used from
    # GDAL 3.4.0 to 3.5.0
    open(out_filename + '.aux.xml', 'wb').write(
b"""<PAMDataset>
  <Subdataset name="0">
    <PAMDataset>
      <Metadata>
        <MDI key="FOO">BAR</MDI>
      </Metadata>
    </PAMDataset>
  </Subdataset>
</PAMDataset>""")

    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem('FOO') == 'BAR'
    ds = None

    ds = gdal.Open('NITF_IM:0:' + out_filename)
    assert ds.GetMetadataItem('FOO') == 'BAR'
    ds = None

    gdal.GetDriverByName('NITF').Delete(out_filename)

###############################################################################
# Test writing/reading PAM metadata


def test_nitf_pam_metadata_several_images():

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_filename = 'tmp/test_nitf_pam_metadata_several_images.ntf'
    gdal.GetDriverByName('NITF').CreateCopy(out_filename, src_ds, options = ['NUMI=2'])
    src_ds2 = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds2.SetGeoTransform(src_ds.GetGeoTransform())
    src_ds2.SetSpatialRef(src_ds.GetSpatialRef())
    gdal.GetDriverByName('NITF').CreateCopy(out_filename, src_ds2, options = ['APPEND_SUBDATASET=YES'])
    ds = None

    assert os.path.exists(out_filename + '.aux.xml')
    pam = open(out_filename + '.aux.xml', 'rb').read()
    assert b'<Subdataset' in pam

    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Area'
    ds = None

    ds = gdal.Open('NITF_IM:0:' + out_filename)
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Area'
    ds = None

    ds = gdal.Open('NITF_IM:1:' + out_filename)
    assert ds.GetMetadataItem('AREA_OR_POINT') is None
    ds = None

    gdal.GetDriverByName('NITF').Delete(out_filename)

###############################################################################
# Test NITF21_CGM_ANNO_Uncompressed_unmasked.ntf for bug #1313 and #1714


def test_nitf_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/bugs/NITF21_CGM_ANNO_Uncompressed_unmasked.ntf', 'NITF21_CGM_ANNO_Uncompressed_unmasked.ntf'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/NITF21_CGM_ANNO_Uncompressed_unmasked.ntf', 1, 13123, filename_absolute=1)

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = tst.testOpen()
    gdal.PopErrorHandler()

    return ret

###############################################################################
# Test NITF file with multiple images


def test_nitf_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf1.1/U_0001a.ntf', 'U_0001a.ntf'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/U_0001a.ntf')

    md = ds.GetMetadata('SUBDATASETS')
    assert 'SUBDATASET_1_NAME' in md, 'missing SUBDATASET_1_NAME metadata'
    ds = None

###############################################################################
# Test ARIDPCM (C2) image


def test_nitf_online_3():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf1.1/U_0001a.ntf', 'U_0001a.ntf'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'NITF_IM:3:tmp/cache/U_0001a.ntf', 1, 23463, filename_absolute=1)

    return tst.testOpen()

###############################################################################
# Test Vector Quantization (VQ) (C4) file


def test_nitf_online_4():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/cadrg/001zc013.on1', '001zc013.on1'):
        pytest.skip()

    # check that the RPF attribute metadata was carried through.
    ds = gdal.Open('tmp/cache/001zc013.on1')
    md = ds.GetMetadata()
    assert md['NITF_RPF_CurrencyDate'] == '19950720' and md['NITF_RPF_ProductionDate'] == '19950720' and md['NITF_RPF_SignificantDate'] == '19890629', \
        'RPF attribute metadata not captured (#3413)'

    ds = None

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/001zc013.on1', 1, 53960, filename_absolute=1)

    return tst.testOpen()

###############################################################################
# Test Vector Quantization (VQ) (M4) file


def test_nitf_online_5():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/cadrg/overview.ovr', 'overview.ovr'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/overview.ovr', 1, 60699, filename_absolute=1)

    return tst.testOpen()

###############################################################################
# Test a JPEG compressed, single blocked 2048x2048 mono image


def test_nitf_online_6():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_4001b.ntf', 'U_4001b.ntf'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/U_4001b.ntf', 1, 60030, filename_absolute=1)

    return tst.testOpen()


###############################################################################
# Test all combinations of IMODE (S,P,B,R) for an image with 6 bands whose 3 are RGB

def test_nitf_online_7():

    for filename in ['ns3228b.nsf', 'i_3228c.ntf', 'ns3228d.nsf', 'i_3228e.ntf']:
        if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/' + filename, filename):
            pytest.skip()

        ds = gdal.Open('tmp/cache/' + filename)
        assert ds.RasterCount == 6

        checksums = [48385, 48385, 40551, 54223, 48385, 33094]
        colorInterpretations = [gdal.GCI_Undefined, gdal.GCI_Undefined, gdal.GCI_RedBand, gdal.GCI_BlueBand, gdal.GCI_Undefined, gdal.GCI_GreenBand]

        for i in range(6):
            cs = ds.GetRasterBand(i + 1).Checksum()
            assert cs == checksums[i], ('got checksum %d for image %s'
                                     % (cs, filename))

            assert ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == colorInterpretations[i], \
                ('got wrong color interp for image %s'
                                     % filename)
        ds = None


###############################################################################
# Test JPEG-compressed multi-block mono-band image with a data mask subheader (IC=M3, IMODE=B)


def test_nitf_online_8():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3301j.nsf', 'ns3301j.nsf'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/ns3301j.nsf', 1, 56861, filename_absolute=1)

    return tst.testOpen()


###############################################################################
# Test JPEG-compressed multi-block mono-band image without a data mask subheader (IC=C3, IMODE=B)

def test_nitf_online_9():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3304a.nsf', 'ns3304a.nsf'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/ns3304a.nsf', 1, 32419, filename_absolute=1)

    return tst.testOpen()


###############################################################################
# Verify that CGM access on a file with 8 CGM segments

def test_nitf_online_10():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3119b.nsf', 'ns3119b.nsf'):
        pytest.skip()

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('tmp/cache/ns3119b.nsf')
    gdal.PopErrorHandler()

    mdCGM = ds.GetMetadata('CGM')

    ds = None

    assert mdCGM['SEGMENT_COUNT'] == '8', 'wrong SEGMENT_COUNT.'

    tab = [
        ('SEGMENT_0_SLOC_ROW', '0'),
        ('SEGMENT_0_SLOC_COL', '0'),
        ('SEGMENT_0_CCS_COL', '0'),
        ('SEGMENT_0_CCS_COL', '0'),
        ('SEGMENT_0_SDLVL', '1'),
        ('SEGMENT_0_SALVL', '0'),
        ('SEGMENT_1_SLOC_ROW', '0'),
        ('SEGMENT_1_SLOC_COL', '684'),
        ('SEGMENT_2_SLOC_ROW', '0'),
        ('SEGMENT_2_SLOC_COL', '1364'),
        ('SEGMENT_3_SLOC_ROW', '270'),
        ('SEGMENT_3_SLOC_COL', '0'),
        ('SEGMENT_4_SLOC_ROW', '270'),
        ('SEGMENT_4_SLOC_COL', '684'),
        ('SEGMENT_5_SLOC_ROW', '270'),
        ('SEGMENT_5_SLOC_COL', '1364'),
        ('SEGMENT_6_SLOC_ROW', '540'),
        ('SEGMENT_6_SLOC_COL', '0'),
        ('SEGMENT_7_SLOC_ROW', '540'),
        ('SEGMENT_7_SLOC_COL', '1364'),
        ('SEGMENT_7_CCS_ROW', '540'),
        ('SEGMENT_7_CCS_COL', '1364'),
        ('SEGMENT_7_SDLVL', '8'),
        ('SEGMENT_7_SALVL', '0'),
    ]

    for item in tab:
        assert mdCGM[item[0]] == item[1], ('wrong value for %s.' % item[0])


###############################################################################
# 5 text files


def test_nitf_online_11():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_1122a.ntf', 'U_1122a.ntf'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/U_1122a.ntf')

    mdTEXT = ds.GetMetadata('TEXT')

    ds = None

    assert mdTEXT['DATA_0'] == 'This is test text file 01.\r\n', \
        'did not find expected DATA_0 from metadata.'
    assert mdTEXT['DATA_1'] == 'This is test text file 02.\r\n', \
        'did not find expected DATA_1 from metadata.'
    assert mdTEXT['DATA_2'] == 'This is test text file 03.\r\n', \
        'did not find expected DATA_2 from metadata.'
    assert mdTEXT['DATA_3'] == 'This is test text file 04.\r\n', \
        'did not find expected DATA_3 from metadata.'
    assert mdTEXT['DATA_4'] == 'This is test text file 05.\r\n', \
        'did not find expected DATA_4 from metadata.'


###############################################################################
# Test 12 bit uncompressed image.

def test_nitf_online_12():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/bugs/i_3430a.ntf', 'i_3430a.ntf'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/i_3430a.ntf', 1, 38647,
                            filename_absolute=1)

    return tst.testOpen()


###############################################################################
# Test complex relative graphic/image attachment.

def test_nitf_online_13():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/u_3054a.ntf', 'u_3054a.ntf'):
        pytest.skip()

    # Shut up the warning about missing image segment
    ds = gdal.Open('NITF_IM:2:tmp/cache/u_3054a.ntf')

    mdCGM = ds.GetMetadata('CGM')
    md = ds.GetMetadata()

    ds = None

    assert mdCGM['SEGMENT_COUNT'] == '3', 'wrong SEGMENT_COUNT.'

    tab = [
        ('SEGMENT_2_SLOC_ROW', '0'),
        ('SEGMENT_2_SLOC_COL', '0'),
        ('SEGMENT_2_CCS_COL', '1100'),
        ('SEGMENT_2_CCS_COL', '1100'),
        ('SEGMENT_2_SDLVL', '6'),
        ('SEGMENT_2_SALVL', '3')
    ]

    for item in tab:
        assert mdCGM[item[0]] == item[1], ('wrong value for %s.' % item[0])

    tab = [
        ('NITF_IDLVL', '3'),
        ('NITF_IALVL', '1'),
        ('NITF_ILOC_ROW', '1100'),
        ('NITF_ILOC_COLUMN', '1100'),
        ('NITF_CCS_ROW', '1100'),
        ('NITF_CCS_COLUMN', '1100'),
    ]

    for item in tab:
        assert md[item[0]] == item[1], ('wrong value for %s, got %s instead of %s.'
                                 % (item[0], md[item[0]], item[1]))



###############################################################################
# Check reading a 12-bit JPEG compressed NITF (multi-block)

def test_nitf_online_14(not_jpeg_9b):

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_4020h.ntf', 'U_4020h.ntf'):
        pytest.skip()

    try:
        os.remove('tmp/cache/U_4020h.ntf.aux.xml')
    except OSError:
        pass

    # Check if JPEG driver supports 12bit JPEG reading/writing
    jpg_drv = gdal.GetDriverByName('JPEG')
    md = jpg_drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        pytest.skip('12bit jpeg not available')

    ds = gdal.Open('tmp/cache/U_4020h.ntf')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[2] >= 2607 and stats[2] <= 2608
    ds = None

    try:
        os.remove('tmp/cache/U_4020h.ntf.aux.xml')
    except OSError:
        pass


###############################################################################
# Test opening a IC=C8 NITF file with the various JPEG2000 drivers


def nitf_online_15(driver_to_test, expected_cs=1054):
    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/p0_01/p0_01a.ntf', 'p0_01a.ntf'):
        pytest.skip()

    jp2_drv = gdal.GetDriverByName(driver_to_test)

    if jp2_drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('tmp/cache/p0_01a.ntf')
    if ds.GetRasterBand(1).Checksum() == expected_cs:
        ret = 'success'
    else:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Did not get expected checksums')
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


def test_nitf_online_15_jp2ecw():
    return nitf_online_15('JP2ECW')


def test_nitf_online_15_jp2mrsid():
    return nitf_online_15('JP2MrSID')


def test_nitf_online_15_jp2kak():
    return nitf_online_15('JP2KAK')


def test_nitf_online_15_jasper():
    return nitf_online_15('JPEG2000')


def test_nitf_online_15_openjpeg():
    return nitf_online_15('JP2OpenJPEG')

###############################################################################
# Test opening a IC=C8 NITF file which has 256-entry palette/LUT in both JP2 Header and image Subheader
# We expect RGB expansion from some JPEG2000 driver


def nitf_online_16(driver_to_test):
    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9_jp2_2places.ntf', 'file9_jp2_2places.ntf'):
        pytest.skip()

    jp2_drv = gdal.GetDriverByName(driver_to_test)

    if jp2_drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('tmp/cache/file9_jp2_2places.ntf')
    # JPEG2000 driver
    if ds.RasterCount == 3 and \
       ds.GetRasterBand(1).Checksum() == 48954 and \
       ds.GetRasterBand(2).Checksum() == 4939 and \
       ds.GetRasterBand(3).Checksum() == 17734:
        ret = 'success'

    elif ds.RasterCount == 1 and \
            ds.GetRasterBand(1).Checksum() == 47664 and \
            ds.GetRasterBand(1).GetRasterColorTable() is not None:
        ret = 'success'
    else:
        print(ds.RasterCount)
        for i in range(ds.RasterCount):
            print(ds.GetRasterBand(i + 1).Checksum())
        print(ds.GetRasterBand(1).GetRasterColorTable())
        gdaltest.post_reason('Did not get expected checksums')
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


def test_nitf_online_16_jp2ecw():
    return nitf_online_16('JP2ECW')


def test_nitf_online_16_jp2mrsid():
    return nitf_online_16('JP2MrSID')


def test_nitf_online_16_jp2kak():
    return nitf_online_16('JP2KAK')


def test_nitf_online_16_jasper():
    return nitf_online_16('JPEG2000')


def test_nitf_online_16_openjpeg():
    return nitf_online_16('JP2OpenJPEG')

###############################################################################
# Test opening a IC=C8 NITF file which has 256-entry/LUT in Image Subheader, JP2 header completely removed
# We don't expect RGB expansion from the JPEG2000 driver


def nitf_online_17(driver_to_test):
    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9_j2c.ntf', 'file9_j2c.ntf'):
        pytest.skip()

    jp2_drv = gdal.GetDriverByName(driver_to_test)

    if jp2_drv is None:
        pytest.skip()

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('tmp/cache/file9_j2c.ntf')
    if ds.RasterCount == 1 and \
       ds.GetRasterBand(1).Checksum() == 47664 and \
       ds.GetRasterBand(1).GetRasterColorTable() is not None:
        ret = 'success'
    else:
        print(ds.RasterCount)
        for i in range(ds.RasterCount):
            print(ds.GetRasterBand(i + 1).Checksum())
        print(ds.GetRasterBand(1).GetRasterColorTable())
        gdaltest.post_reason('Did not get expected checksums')
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


def test_nitf_online_17_jp2ecw():
    return nitf_online_17('JP2ECW')


def test_nitf_online_17_jp2mrsid():
    return nitf_online_17('JP2MrSID')


def test_nitf_online_17_jp2kak():
    return nitf_online_17('JP2KAK')


def test_nitf_online_17_jasper():
    return nitf_online_17('JPEG2000')


def test_nitf_online_17_openjpeg():
    return nitf_online_17('JP2OpenJPEG')

###############################################################################
# Test polar stereographic CADRG tile.


def test_nitf_online_18():
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/bugs/bug3337.ntf', 'bug3337.ntf'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/bug3337.ntf')

    gt = ds.GetGeoTransform()
    prj = ds.GetProjection()

    # If we have functioning coordinate transformer.
    if prj[:6] == 'PROJCS':
        assert prj.find('Azimuthal_Equidistant') != -1, 'wrong projection?'
        expected_gt = (-1669792.3618991028, 724.73626818537502, 0.0, -556597.45396636717, 0.0, -724.73626818537434)
        assert gdaltest.geotransform_equals(gt, expected_gt, 1.0), \
            'did not get expected geotransform.'

    # If we do not have a functioning coordinate transformer.
    else:
        assert prj == '' and gdaltest.geotransform_equals(gt, (0, 1, 0, 0, 0, 1), 0.00000001), \
            'did not get expected empty gt/projection'

        prj = ds.GetGCPProjection()
        assert prj[:6] == 'GEOGCS', 'did not get expected geographic srs'

        gcps = ds.GetGCPs()
        gcp3 = gcps[3]
        assert gcp3.GCPPixel == 0 and gcp3.GCPLine == 1536 and abs(gcp3.GCPX + 45) <= 0.0000000001 and gcp3.GCPY == pytest.approx(68.78679656, abs=0.00000001), \
            'did not get expected gcp.'

    ds = None

###############################################################################
# Test CADRG tile crossing dateline (#3383)


def test_nitf_online_19():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/0000M033.GN3', '0000M033.GN3'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/0000M033.GN3', 1, 38928,
                            filename_absolute=1)

    return tst.testOpen(check_gt=(174.375000000000000, 0.010986328125000, 0,
                                  51.923076923076927, 0, -0.006760817307692))

###############################################################################
# Check that the RPF attribute metadata was carried through.
# Special case where the reported size of the attribute subsection is
# smaller than really available


def test_nitf_online_20():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/0000M033.GN3', '0000M033.GN3'):
        pytest.skip()

    # check that the RPF attribute metadata was carried through.
    # Special case where the reported size of the attribute subsection is
    # smaller than really available
    ds = gdal.Open('tmp/cache/0000M033.GN3')
    md = ds.GetMetadata()
    assert md['NITF_RPF_CurrencyDate'] == '19941201' and md['NITF_RPF_ProductionDate'] == '19980511' and md['NITF_RPF_SignificantDate'] == '19850305', \
        'RPF attribute metadata not captured (#3413)'

###############################################################################
# Check that we can read NITF header located in STREAMING_FILE_HEADER DE
# segment when header at beginning of file is incomplete


def test_nitf_online_21():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3321a.nsf', 'ns3321a.nsf'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/ns3321a.nsf')
    md = ds.GetMetadata()
    ds = None

    # If we get NS3321A, it means we are not exploiting the header from the STREAMING_FILE_HEADER DE segment
    assert md['NITF_OSTAID'] == 'I_3321A', \
        'did not get expected OSTAID value'

###############################################################################
# Test fix for #3002 (reconcile NITF file with LA segments)
#


def test_nitf_online_22():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv1_1/U_0001C.NTF', 'U_0001C.NTF'):
        pytest.skip()

    ds = gdal.Open('NITF_IM:1:tmp/cache/U_0001C.NTF')
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL', '6'),
        ('NITF_IALVL', '1'),
        ('NITF_ILOC_ROW', '360'),
        ('NITF_ILOC_COLUMN', '380'),
        ('NITF_CCS_ROW', '425'),
        ('NITF_CCS_COLUMN', '410'),
    ]

    for item in tab:
        assert md[item[0]] == item[1], ('(1) wrong value for %s, got %s instead of %s.'
                                 % (item[0], md[item[0]], item[1]))

    ds = gdal.Open('NITF_IM:2:tmp/cache/U_0001C.NTF')
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL', '11'),
        ('NITF_IALVL', '2'),
        ('NITF_ILOC_ROW', '360'),
        ('NITF_ILOC_COLUMN', '40'),
        ('NITF_CCS_ROW', '422'),
        ('NITF_CCS_COLUMN', '210'),
    ]

    for item in tab:
        assert md[item[0]] == item[1], ('(2) wrong value for %s, got %s instead of %s.'
                                 % (item[0], md[item[0]], item[1]))

    ds = gdal.Open('NITF_IM:3:tmp/cache/U_0001C.NTF')
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL', '5'),
        ('NITF_IALVL', '3'),
        ('NITF_ILOC_ROW', '40'),
        ('NITF_ILOC_COLUMN', '240'),
        ('NITF_CCS_ROW', '-1'),
        ('NITF_CCS_COLUMN', '-1'),
    ]

    for item in tab:
        assert md[item[0]] == item[1], ('(3) wrong value for %s, got %s instead of %s.'
                                 % (item[0], md[item[0]], item[1]))

    ds = gdal.Open('NITF_IM:4:tmp/cache/U_0001C.NTF')
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL', '1'),
        ('NITF_IALVL', '0'),
        ('NITF_ILOC_ROW', '65'),
        ('NITF_ILOC_COLUMN', '30'),
        ('NITF_CCS_ROW', '65'),
        ('NITF_CCS_COLUMN', '30'),
    ]

    for item in tab:
        assert md[item[0]] == item[1], ('(4) wrong value for %s, got %s instead of %s.'
                                 % (item[0], md[item[0]], item[1]))


###############################################################################
# Test reading a M4 compressed file (fixed for #3848)


def test_nitf_online_23():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_3058b.ntf', 'U_3058b.ntf'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/U_3058b.ntf', 1, 44748, filename_absolute=1)

    return tst.testOpen()

###############################################################################
# Test reading ECRG frames


def test_nitf_online_24():

    if not gdaltest.download_file('http://www.falconview.org/trac/FalconView/downloads/17', 'ECRG_Sample.zip'):
        pytest.skip()

    try:
        os.stat('tmp/cache/ECRG_Sample.zip')
    except OSError:
        pytest.skip()

    oldval = gdal.GetConfigOption('NITF_OPEN_UNDERLYING_DS')
    gdal.SetConfigOption('NITF_OPEN_UNDERLYING_DS', 'NO')
    ds = gdal.Open('/vsizip/tmp/cache/ECRG_Sample.zip/ECRG_Sample/EPF/clfc/2/000000009s0013.lf2')
    gdal.SetConfigOption('NITF_OPEN_UNDERLYING_DS', oldval)
    assert ds is not None
    xml_tre = ds.GetMetadata('xml:TRE')[0]
    ds = None

    assert (not (xml_tre.find('<tre name="GEOPSB"') == -1 or \
       xml_tre.find('<tre name="J2KLRA"') == -1 or \
       xml_tre.find('<tre name="GEOLOB"') == -1 or \
       xml_tre.find('<tre name="BNDPLB"') == -1 or \
       xml_tre.find('<tre name="ACCPOB"') == -1 or \
       xml_tre.find('<tre name="SOURCB"') == -1)), 'did not get expected xml:TRE'

###############################################################################
# Test reading a HRE file


def test_nitf_online_25():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/docs/HRE_spec/Case1_HRE10G324642N1170747W_Uxx.hr5', 'Case1_HRE10G324642N1170747W_Uxx.hr5'):
        pytest.skip()

    tst = gdaltest.GDALTest('NITF', 'tmp/cache/Case1_HRE10G324642N1170747W_Uxx.hr5', 1, 7099, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open('tmp/cache/Case1_HRE10G324642N1170747W_Uxx.hr5')
    xml_tre = ds.GetMetadata('xml:TRE')[0]
    ds = None

    assert xml_tre.find('<tre name="PIAPRD"') != -1, 'did not get expected xml:TRE'

###############################################################################
# Cleanup.


def test_nitf_cleanup():
    try:
        gdal.GetDriverByName('NITF').Delete('tmp/test_create.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf9.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/test_13.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/test_29.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/test_29_copy.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf36.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf37.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf38.ntf')
        os.unlink('tmp/nitf38.ntf_0.ovr')
    except (RuntimeError, OSError):
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf39.ntf')
    except (RuntimeError, OSError):
        pass

    try:
        os.stat('tmp/nitf40.ntf')
        gdal.GetDriverByName('NITF').Delete('tmp/nitf40.ntf')
    except (RuntimeError, OSError):
        pass

    try:
        os.stat('tmp/nitf42.ntf')
        gdal.GetDriverByName('NITF').Delete('tmp/nitf42.ntf')
    except (OSError, RuntimeError):
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf44.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf45.ntf')
        os.unlink('tmp/nitf45.ntf_0.ovr')
    except (RuntimeError, OSError):
        pass

    try:
        os.stat('tmp/nitf46.ntf')
        gdal.GetDriverByName('NITF').Delete('tmp/nitf46.ntf')
        os.unlink('tmp/nitf46.ntf_0.ovr')
    except (RuntimeError, OSError):
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf49.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf49_2.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf50.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf51.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf52.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf53.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf54.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf55.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf56.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf57.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf58.ntf')
    except RuntimeError:
        pass

    try:
        os.remove('tmp/nitf59.hdr')
        gdal.GetDriverByName('NITF').Delete('tmp/nitf59.ntf')
    except (OSError, RuntimeError):
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf62.ntf')
    except RuntimeError:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete('tmp/nitf63.ntf')
    except RuntimeError:
        pass
