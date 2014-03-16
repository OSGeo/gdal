#!/usr/bin/env python
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
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

import os
import sys
from osgeo import gdal
from osgeo import osr
import array
import string
import struct
import shutil
from sys import version_info

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Write/Read test of simple byte reference data.

def nitf_1():

    tst = gdaltest.GDALTest( 'NITF', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple 16bit reference data. 

def nitf_2():

    tst = gdaltest.GDALTest( 'NITF', 'int16.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read RGB image with lat/long georeferencing, and verify.

def nitf_3():

    tst = gdaltest.GDALTest( 'NITF', 'rgbsmall.tif', 3, 21349 )
    return tst.testCreateCopy()


###############################################################################
# Test direction creation of an NITF file.

def nitf_create(creation_options, set_inverted_color_interp = True):

    drv = gdal.GetDriverByName( 'NITF' )

    try:
        os.remove( 'tmp/test_create.ntf' )
    except:
        pass

    ds = drv.Create( 'tmp/test_create.ntf', 200, 100, 3, gdal.GDT_Byte,
                     creation_options )
    ds.SetGeoTransform( (100, 0.1, 0.0, 30.0, 0.0, -0.1 ) )

    if set_inverted_color_interp:
        ds.GetRasterBand( 1 ).SetRasterColorInterpretation( gdal.GCI_BlueBand )
        ds.GetRasterBand( 2 ).SetRasterColorInterpretation( gdal.GCI_GreenBand )
        ds.GetRasterBand( 3 ).SetRasterColorInterpretation( gdal.GCI_RedBand )
    else:
        ds.GetRasterBand( 1 ).SetRasterColorInterpretation( gdal.GCI_RedBand )
        ds.GetRasterBand( 2 ).SetRasterColorInterpretation( gdal.GCI_GreenBand )
        ds.GetRasterBand( 3 ).SetRasterColorInterpretation( gdal.GCI_BlueBand )

    my_list = list(range(200)) + list(range(20,220)) + list(range(30,230))
    raw_data = array.array('h',my_list).tostring()

    for line in range(100):
        ds.WriteRaster( 0, line, 200, 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1,2,3] )

    ds = None

    return 'success'

###############################################################################
# Test direction creation of an non-compressed NITF file.

def nitf_4():

    return nitf_create([ 'ICORDS=G' ])


###############################################################################
# Verify created file

def nitf_check_created_file(checksum1, checksum2, checksum3, set_inverted_color_interp = True):
    ds = gdal.Open( 'tmp/test_create.ntf' )
    
    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = checksum1
    if chksum != chksum_expect:
        gdaltest.post_reason( 'Did not get expected chksum for band 1' )
        print(chksum, chksum_expect)
        return 'fail'

    chksum = ds.GetRasterBand(2).Checksum()
    chksum_expect = checksum2
    if chksum != chksum_expect:
        gdaltest.post_reason( 'Did not get expected chksum for band 2' )
        print(chksum, chksum_expect)
        return 'fail'

    chksum = ds.GetRasterBand(3).Checksum()
    chksum_expect = checksum3
    if chksum != chksum_expect:
        gdaltest.post_reason( 'Did not get expected chksum for band 3' )
        print(chksum, chksum_expect)
        return 'fail'

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-100) > 0.1 \
        or abs(geotransform[1]-0.1) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-30.0) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -0.1) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    if set_inverted_color_interp:
        if ds.GetRasterBand(1).GetRasterColorInterpretation() != gdal.GCI_BlueBand:
            gdaltest.post_reason( 'Got wrong color interpretation.' )
            return 'fail'

        if ds.GetRasterBand(2).GetRasterColorInterpretation() !=gdal.GCI_GreenBand:
            gdaltest.post_reason( 'Got wrong color interpretation.' )
            return 'fail'

        if ds.GetRasterBand(3).GetRasterColorInterpretation() != gdal.GCI_RedBand:
            gdaltest.post_reason( 'Got wrong color interpretation.' )
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Verify file created by nitf_4()

def nitf_5():

    return nitf_check_created_file(32498, 42602, 38982)
	
###############################################################################
# Read existing NITF file.  Verifies the new adjusted IGEOLO interp.

def nitf_6():

    tst = gdaltest.GDALTest( 'NITF', 'rgb.ntf', 3, 21349 )
    return tst.testOpen( check_prj = 'WGS84',
                         check_gt = (-44.842029478458, 0.003503401360, 0,
                                     -22.930748299319, 0, -0.003503401360) )

###############################################################################
# NITF in-memory.

def nitf_7():

    tst = gdaltest.GDALTest( 'NITF', 'rgbsmall.tif', 3, 21349 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Verify we can open an NSIF file, and get metadata including BLOCKA.

def nitf_8():

    ds = gdal.Open( 'data/fake_nsif.ntf' )
    
    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 12033
    if chksum != chksum_expect:
        gdaltest.post_reason( 'Did not get expected chksum for band 1' )
        print(chksum, chksum_expect)
        return 'fail'

    md = ds.GetMetadata()
    if md['NITF_FHDR'] != 'NSIF01.00':
        gdaltest.post_reason( 'Got wrong FHDR value' )
        return 'fail'

    if md['NITF_BLOCKA_BLOCK_INSTANCE_01'] != '01' \
       or md['NITF_BLOCKA_BLOCK_COUNT'] != '01' \
       or md['NITF_BLOCKA_N_GRAY_01'] != '00000' \
       or md['NITF_BLOCKA_L_LINES_01'] != '01000' \
       or md['NITF_BLOCKA_LAYOVER_ANGLE_01'] != '000' \
       or md['NITF_BLOCKA_SHADOW_ANGLE_01'] != '000' \
       or md['NITF_BLOCKA_FRLC_LOC_01'] != '+41.319331+020.078400' \
       or md['NITF_BLOCKA_LRLC_LOC_01'] != '+41.317083+020.126072' \
       or md['NITF_BLOCKA_LRFC_LOC_01'] != '+41.281634+020.122570' \
       or md['NITF_BLOCKA_FRFC_LOC_01'] != '+41.283881+020.074924':
        gdaltest.post_reason( 'BLOCKA metadata has unexpected value.' )
        return 'fail'
    
    return 'success'

###############################################################################
# Create and read a JPEG encoded NITF file.

def nitf_9():

    src_ds = gdal.Open( 'data/rgbsmall.tif' )
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf9.ntf', src_ds,
                                                  options = ['IC=C3'] )
    src_ds = None
    ds = None

    ds = gdal.Open( 'tmp/nitf9.ntf' )
    
    (exp_mean, exp_stddev) = (65.9532, 46.9026375565)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()
    
    if abs(exp_mean-mean) > 0.1 or abs(exp_stddev-stddev) > 0.1:
        print(mean, stddev)
        gdaltest.post_reason( 'did not get expected mean or standard dev.' )
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['COMPRESSION'] != 'JPEG':
        gdaltest.post_reason( 'Did not get expected compression value.' )
        return 'fail'
    
    return 'success'

###############################################################################
# For esoteric reasons, createcopy from jpeg compressed nitf files can be
# tricky.  Verify this is working. 

def nitf_10():
    
    src_ds = gdal.Open('tmp/nitf9.ntf')
    expected_cs = src_ds.GetRasterBand(2).Checksum()
    src_ds = None
    if expected_cs != 22296 and expected_cs != 22259:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    tst = gdaltest.GDALTest( 'NITF', '../tmp/nitf9.ntf', 2, expected_cs )
    return tst.testCreateCopy()

###############################################################################
# Test 1bit file ... conveniently very small and easy to include! (#1854)

def nitf_11():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/i_3034c.ntf
    tst = gdaltest.GDALTest( 'NITF', 'i_3034c.ntf', 1, 170 )
    return tst.testOpen()

###############################################################################
# Verify that TRE and CGM access via the metadata domain works.

def nitf_12():

    ds = gdal.Open( 'data/fake_nsif.ntf' )

    mdTRE = ds.GetMetadata( 'TRE' )

    try: # NG bindings
        blockA = ds.GetMetadataItem( 'BLOCKA', 'TRE' )
    except:
        blockA = mdTRE['BLOCKA']

    mdCGM = ds.GetMetadata( 'CGM' )

    try: # NG bindings
        segmentCount = ds.GetMetadataItem( 'SEGMENT_COUNT', 'CGM' )
    except:
        segmentCount = mdCGM['SEGMENT_COUNT']

    ds = None

    expectedBlockA = '010000001000000000                +41.319331+020.078400+41.317083+020.126072+41.281634+020.122570+41.283881+020.074924     '

    if mdTRE['BLOCKA'] != expectedBlockA:
        gdaltest.post_reason( 'did not find expected BLOCKA from metadata.' )
        return 'fail'

    if blockA != expectedBlockA:
        gdaltest.post_reason( 'did not find expected BLOCKA from metadata item.' )
        return 'fail'

    if mdCGM['SEGMENT_COUNT'] != '0':
        gdaltest.post_reason( 'did not find expected SEGMENT_COUNT from metadata.' )
        return 'fail'

    if segmentCount != '0':
        gdaltest.post_reason( 'did not find expected SEGMENT_COUNT from metadata item.' )
        return 'fail'

    return 'success'


###############################################################################
# Test creation of an NITF file in UTM Zone 11, Southern Hemisphere.

def nitf_13():
    drv = gdal.GetDriverByName( 'NITF' )
    ds = drv.Create( 'tmp/test_13.ntf', 200, 100, 1, gdal.GDT_Byte,
                     [ 'ICORDS=S' ] )
    ds.SetGeoTransform( (400000, 10, 0.0, 6000000, 0.0, -10 ) )
    ds.SetProjection('PROJCS["UTM Zone 11, Southern Hemisphere",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000],UNIT["Meter",1]]')

    my_list = list(range(200))
    raw_data = array.array('f',my_list).tostring()

    for line in range(100):
        ds.WriteRaster( 0, line, 200, 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1] )

    ds = None

    return 'success'

###############################################################################
# Verify previous file

def nitf_14():
    ds = gdal.Open( 'tmp/test_13.ntf' )

    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 55964
    if chksum != chksum_expect:
        gdaltest.post_reason( 'Did not get expected chksum for band 1' )
        print(chksum, chksum_expect)
        return 'fail'

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-400000) > .1 \
    or abs(geotransform[1]-10) > 0.001 \
    or abs(geotransform[2]-0) > 0.001 \
    or abs(geotransform[3]-6000000) > .1 \
    or abs(geotransform[4]-0) > 0.001 \
    or abs(geotransform[5]- -10) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    prj = ds.GetProjectionRef()
    if prj.find('UTM Zone 11, Southern Hemisphere') == -1:
        print(prj)
        gdaltest.post_reason( 'Coordinate system not UTM Zone 11, Southern Hemisphere' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test creating an in memory copy.

def nitf_15():

    tst = gdaltest.GDALTest( 'NITF', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Checks a 1-bit mono with mask table having (0x00) black as transparent with white arrow.

def nitf_16():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3034d.nsf
    tst = gdaltest.GDALTest( 'NITF', 'ns3034d.nsf', 1, 170 )
    return tst.testOpen()


###############################################################################
# Checks a 1-bit RGB/LUT (green arrow) with a mask table (pad pixels having value of 0x00)
# and a transparent pixel value of 1 being mapped to green by the LUT

def nitf_17():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/i_3034f.ntf
    tst = gdaltest.GDALTest( 'NITF', 'i_3034f.ntf', 1, 170 )
    return tst.testOpen()

###############################################################################
# Test NITF file without image segment

def nitf_18():

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv1_1/U_0006A.NTF
    ds = gdal.Open("data/U_0006A.NTF")
    gdal.PopErrorHandler()

    if ds.RasterCount != 0:
        return 'fail'

    return 'success'

###############################################################################
# Test BILEVEL (C1) decompression

def nitf_19():

    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_0/U_1050A.NTF
    tst = gdaltest.GDALTest( 'NITF', 'U_1050A.NTF', 1, 65024 )

    return tst.testOpen()


###############################################################################
# Test NITF file consiting only of an header

def nitf_20():

    # Shut up the warning about file either corrupt or empty
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    # From http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv1_1/U_0002A.NTF
    ds = gdal.Open("data/U_0002A.NTF")
    gdal.PopErrorHandler()

    if ds is not None:
        return 'fail'

    return 'success'


###############################################################################
# Verify that TEXT access via the metadata domain works.
#
# See also nitf_35 for writing TEXT segments.

def nitf_21():

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open( 'data/ns3114a.nsf' )
    gdal.PopErrorHandler()

    mdTEXT = ds.GetMetadata( 'TEXT' )

    try: # NG bindings
        data0 = ds.GetMetadataItem( 'DATA_0', 'TEXT' )
    except:
        data0 = mdTEXT['DATA_0']

    ds = None

    if mdTEXT['DATA_0'] != 'A':
        gdaltest.post_reason( 'did not find expected DATA_0 from metadata.' )
        return 'fail'

    if data0 != 'A':
        gdaltest.post_reason( 'did not find expected DATA_0 from metadata item.' )
        return 'fail'

    return 'success'


###############################################################################
# Write/Read test of simple int32 reference data. 

def nitf_22():

    tst = gdaltest.GDALTest( 'NITF', '../../gcore/data/int32.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple float32 reference data. 

def nitf_23():

    tst = gdaltest.GDALTest( 'NITF', '../../gcore/data/float32.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple float64 reference data. 

def nitf_24():

    tst = gdaltest.GDALTest( 'NITF', '../../gcore/data/float64.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple uint16 reference data. 

def nitf_25():

    tst = gdaltest.GDALTest( 'NITF', '../../gcore/data/uint16.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Write/Read test of simple uint32 reference data. 

def nitf_26():

    tst = gdaltest.GDALTest( 'NITF', '../../gcore/data/uint32.tif', 1, 4672 )
    return tst.testCreateCopy()

###############################################################################
# Test Create() with IC=NC compression, and multi-blocks

def nitf_27():

    if nitf_create([ 'ICORDS=G', 'IC=NC', 'BLOCKXSIZE=10', 'BLOCKYSIZE=10' ]) != 'success':
        return 'fail'

    return nitf_check_created_file(32498, 42602, 38982)


###############################################################################
# Test Create() with IC=C8 compression with the JP2ECW driver

def nitf_28_jp2ecw():
    gdaltest.nitf_28_jp2ecw_is_ok = False
    import ecw
    if not ecw.has_write_support():
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2ECW')

    if nitf_create([ 'ICORDS=G', 'IC=C8', 'TARGET=75' ], set_inverted_color_interp = False) == 'success':
        ret = nitf_check_created_file(32398, 42502, 38882, set_inverted_color_interp = False)
        if ret == 'success':
            gdaltest.nitf_28_jp2ecw_is_ok = True
    else:
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

###############################################################################
# Test reading the previously create file with the JP2MrSID driver
# (The NITF driver only looks for the JP2ECW driver when creating IC=C8 NITF files,
#  but allows any GDAL driver to open the JP2 stream inside it)

def nitf_28_jp2mrsid():
    if not gdaltest.nitf_28_jp2ecw_is_ok:
        return 'skip'

    try:
        jp2mrsid_drv = gdal.GetDriverByName( 'JP2MrSID' )
    except:
        jp2mrsid_drv = None

    if jp2mrsid_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2MrSID')

    ret = nitf_check_created_file(32398, 42502, 38882, set_inverted_color_interp = False)

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret


###############################################################################
# Test reading the previously create file with the JP2KAK driver
# (The NITF driver only looks for the JP2ECW driver when creating IC=C8 NITF files,
#  but allows any GDAL driver to open the JP2 stream inside it)
#
# Note: I (E. Rouault) haven't been able to check that this test actually works.

def nitf_28_jp2kak():
    if not gdaltest.nitf_28_jp2ecw_is_ok:
        return 'skip'

    try:
        jp2kak_drv = gdal.GetDriverByName( 'JP2KAK' )
    except:
        jp2kak_drv = None

    if jp2kak_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2KAK')

    ret = nitf_check_created_file(32398, 42502, 38882, set_inverted_color_interp = False)

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

###############################################################################
# Test Create() with a LUT

def nitf_29():

    drv = gdal.GetDriverByName( 'NITF' )

    ds = drv.Create( 'tmp/test_29.ntf', 1, 1, 1, gdal.GDT_Byte,
                     [ 'IREP=RGB/LUT', 'LUT_SIZE=128' ] )

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,255,255,255) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )

    ds.GetRasterBand( 1 ).SetRasterColorTable( ct )

    ds = None

    ds = gdal.Open( 'tmp/test_29.ntf' )

    ct = ds.GetRasterBand( 1 ).GetRasterColorTable()
    if ct.GetCount() != 129 or \
       ct.GetColorEntry(0) != (255,255,255,255) or \
       ct.GetColorEntry(1) != (255,255,0,255) or \
       ct.GetColorEntry(2) != (255,0,255,255) or \
       ct.GetColorEntry(3) != (0,255,255,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'

    new_ds = drv.CreateCopy( 'tmp/test_29_copy.ntf', ds )
    new_ds = None
    ds = None

    ds = gdal.Open( 'tmp/test_29_copy.ntf' )

    ct = ds.GetRasterBand( 1 ).GetRasterColorTable()
    if ct.GetCount() != 130 or \
       ct.GetColorEntry(0) != (255,255,255,255) or \
       ct.GetColorEntry(1) != (255,255,0,255) or \
       ct.GetColorEntry(2) != (255,0,255,255) or \
       ct.GetColorEntry(3) != (0,255,255,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Verify we can write a file with BLOCKA TRE and read it back properly.

def nitf_30():

    src_ds = gdal.Open( 'data/fake_nsif.ntf' )
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf30.ntf', src_ds )
    src_ds = None
    
    chksum = ds.GetRasterBand(1).Checksum()
    chksum_expect = 12033
    if chksum != chksum_expect:
        gdaltest.post_reason( 'Did not get expected chksum for band 1' )
        print(chksum, chksum_expect)
        return 'fail'

    md = ds.GetMetadata()
    if md['NITF_FHDR'] != 'NSIF01.00':
        gdaltest.post_reason( 'Got wrong FHDR value' )
        return 'fail'

    if md['NITF_BLOCKA_BLOCK_INSTANCE_01'] != '01' \
       or md['NITF_BLOCKA_BLOCK_COUNT'] != '01' \
       or md['NITF_BLOCKA_N_GRAY_01'] != '00000' \
       or md['NITF_BLOCKA_L_LINES_01'] != '01000' \
       or md['NITF_BLOCKA_LAYOVER_ANGLE_01'] != '000' \
       or md['NITF_BLOCKA_SHADOW_ANGLE_01'] != '000' \
       or md['NITF_BLOCKA_FRLC_LOC_01'] != '+41.319331+020.078400' \
       or md['NITF_BLOCKA_LRLC_LOC_01'] != '+41.317083+020.126072' \
       or md['NITF_BLOCKA_LRFC_LOC_01'] != '+41.281634+020.122570' \
       or md['NITF_BLOCKA_FRFC_LOC_01'] != '+41.283881+020.074924':
        gdaltest.post_reason( 'BLOCKA metadata has unexpected value.' )
        return 'fail'

    ds = None
    
    gdal.GetDriverByName('NITF').Delete( 'tmp/nitf30.ntf' )

    return 'success'

###############################################################################
# Verify we can write a file with a custom TRE and read it back properly.

def nitf_31():

    if nitf_create( [ 'TRE=CUSTOM= Test TRE1\\0MORE',
                      'TRE=TOTEST=SecondTRE',
                      'ICORDS=G' ] ) != 'success':
        return 'fail'

    ds = gdal.Open( 'tmp/test_create.ntf' )

    md = ds.GetMetadata( 'TRE' )
    if len(md) != 2:
        gdaltest.post_reason( 'Did not get expected TRE count' )
        print(md)
        return 'fail'

    # Check that the leading space in the CUSTOM metadata item is preserved (#3088, #3204)
    try:
        if ds.GetMetadataItem( 'CUSTOM', 'TRE') != ' Test TRE1\\0MORE':
            gdaltest.post_reason( 'Did not get expected TRE contents' )
            print(ds.GetMetadataItem( 'CUSTOM', 'TRE'))
            return 'fail'
    except:
        pass
        
    if md['CUSTOM'] != ' Test TRE1\\0MORE' \
       or md['TOTEST'] != 'SecondTRE':
        gdaltest.post_reason( 'Did not get expected TRE contents' )
        print(md)
        return 'fail'

    ds = None
    return nitf_check_created_file( 32498, 42602, 38982 )


###############################################################################
# Test Create() with ICORDS=D

def nitf_32():

    if nitf_create([ 'ICORDS=D' ]) != 'success':
        return 'fail'

    return nitf_check_created_file(32498, 42602, 38982)


###############################################################################
# Test Create() with ICORDS=D and a consistant BLOCKA

def nitf_33():

    if nitf_create([ 'ICORDS=D',
        'BLOCKA_BLOCK_COUNT=01',
        'BLOCKA_BLOCK_INSTANCE_01=01',
        'BLOCKA_L_LINES_01=100',
        'BLOCKA_FRLC_LOC_01=+29.950000+119.950000',
        'BLOCKA_LRLC_LOC_01=+20.050000+119.950000',
        'BLOCKA_LRFC_LOC_01=+20.050000+100.050000',
        'BLOCKA_FRFC_LOC_01=+29.950000+100.050000' ]) != 'success':
        return 'fail'

    return nitf_check_created_file(32498, 42602, 38982)


###############################################################################
# Test CreateCopy() of a 16bit image with tiling

def nitf_34():

    tst = gdaltest.GDALTest( 'NITF', 'n43.dt0', 1, 49187, options = [ 'BLOCKSIZE=64' ] )
    return tst.testCreateCopy( )

###############################################################################
# Test CreateCopy() writing file with a text segment.

def nitf_35():

    src_ds = gdal.Open( 'data/text_md.vrt' )
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf_35.ntf', src_ds )
    src_ds = None
    ds = None

    ds = gdal.Open( 'tmp/nitf_35.ntf' )

    exp_text = """This is text data
with a newline."""
    
    md = ds.GetMetadata('TEXT')
    if md['DATA_0'] != exp_text:
        gdaltest.post_reason( 'Did not get expected TEXT metadata.' )
        print(md)
        return 'fail'

    exp_text = """Also, a second text segment is created."""
    
    md = ds.GetMetadata('TEXT')
    if md['DATA_1'] != exp_text:
        gdaltest.post_reason( 'Did not get expected TEXT metadata.' )
        print(md)
        return 'fail'

    ds = None

    gdal.GetDriverByName('NITF').Delete( 'tmp/nitf_35.ntf' )
    return 'success'

###############################################################################
# Create and read a JPEG encoded NITF file (C3) with several blocks
# Check that statistics are persisted (#3985)

def nitf_36():

    src_ds = gdal.Open( 'data/rgbsmall.tif' )
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf36.ntf', src_ds,
                                                  options = ['IC=C3', 'BLOCKSIZE=32', 'QUALITY=100'] )
    src_ds = None
    ds = None

    ds = gdal.Open( 'tmp/nitf36.ntf' )

    if ds.GetRasterBand(1).GetMinimum() is not None:
        gdaltest.post_reason( 'Did not expect to have minimum value at that point.' )
        return 'fail'

    (minval, maxval, mean, stddev) = ds.GetRasterBand(1).GetStatistics(False, False)
    if stddev >= 0:
        gdaltest.post_reason( 'Did not expect to have statistics at that point.' )
        return 'fail'

    (exp_mean, exp_stddev) = (65.4208, 47.254550335)
    (minval, maxval, mean, stddev) = ds.GetRasterBand(1).GetStatistics(False, True)

    if abs(exp_mean-mean) > 0.1 or abs(exp_stddev-stddev) > 0.1:
        print(mean, stddev)
        gdaltest.post_reason( 'did not get expected mean or standard dev.' )
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['COMPRESSION'] != 'JPEG':
        gdaltest.post_reason( 'Did not get expected compression value.' )
        return 'fail'

    ds = None

    # Check that statistics are persisted (#3985)
    ds = gdal.Open( 'tmp/nitf36.ntf' )

    if ds.GetRasterBand(1).GetMinimum() is None:
        gdaltest.post_reason( 'Should have minimum value at that point.' )
        return 'fail'

    (minval, maxval, mean, stddev) = ds.GetRasterBand(1).GetStatistics(False, False)
    if abs(exp_mean-mean) > 0.1 or abs(exp_stddev-stddev) > 0.1:
        print(mean, stddev)
        gdaltest.post_reason( 'Should have statistics at that point.' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Create and read a NITF file with 69999 bands

def nitf_37():
    try:
        if int(gdal.VersionInfo('VERSION_NUM')) < 1700:
            return 'skip'
    except:
    # OG-python bindings don't have gdal.VersionInfo. Too bad, but let's hope that GDAL's version isn't too old !
        pass 

    ds = gdal.GetDriverByName('NITF').Create( 'tmp/nitf37.ntf', 1, 1, 69999)
    ds = None

    ds = gdal.Open( 'tmp/nitf37.ntf' )
    if ds.RasterCount != 69999:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Create and read a NITF file with 999 images

def nitf_38():

    ds = gdal.Open('data/byte.tif')
    nXSize = ds.RasterXSize
    nYSize = ds.RasterYSize
    data =  ds.GetRasterBand(1).ReadRaster(0, 0, nXSize, nYSize)
    expected_cs = ds.GetRasterBand(1).Checksum()

    ds = gdal.GetDriverByName('NITF').Create( 'tmp/nitf38.ntf', nXSize, nYSize, 1, options = [ 'NUMI=999' ])
    ds = None

    ds = gdal.Open('NITF_IM:998:tmp/nitf38.ntf', gdal.GA_Update)
    ds.GetRasterBand(1).WriteRaster(0, 0, nXSize, nYSize, data)
    
    # Create overviews
    ds.BuildOverviews( overviewlist = [2] )
    
    ds = None

    ds = gdal.Open( 'NITF_IM:0:tmp/nitf38.ntf' )
    if ds.GetRasterBand(1).Checksum() != 0:
        return 'fail'
    ds = None

    ds = gdal.Open( 'NITF_IM:998:tmp/nitf38.ntf' )
    cs = ds.GetRasterBand(1).Checksum();
    if cs != expected_cs:
        print(cs)
        gdaltest.post_reason( 'bad checksum for image of 998th subdataset' )
        return 'fail'
        
    # Check the overview
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 1087:
        print(cs)
        gdaltest.post_reason( 'bad checksum for overview of image of 998th subdataset' )
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Create and read a JPEG encoded NITF file (M3) with several blocks

def nitf_39():

    src_ds = gdal.Open( 'data/rgbsmall.tif' )
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf39.ntf', src_ds,
                                                  options = ['IC=M3', 'BLOCKSIZE=32', 'QUALITY=100'] )
    src_ds = None
    ds = None

    ds = gdal.Open( 'tmp/nitf39.ntf' )
    
    (exp_mean, exp_stddev) = (65.4208, 47.254550335)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()
    
    if abs(exp_mean-mean) > 0.1 or abs(exp_stddev-stddev) > 0.1:
        print(mean, stddev)
        gdaltest.post_reason( 'did not get expected mean or standard dev.' )
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['COMPRESSION'] != 'JPEG':
        gdaltest.post_reason( 'Did not get expected compression value.' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Create a 10 GB NITF file

def nitf_40():

    # Determine if the filesystem supports sparse files (we don't want to create a real 10 GB
    # file !
    if (gdaltest.filesystem_supports_sparse_files('tmp') == False):
        return 'skip'

    width = 99000
    height = 99000
    x = width - 1
    y = height - 1

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf40.ntf', width, height, options = ['BLOCKSIZE=256'])
    data = struct.pack('B' * 1, 123)

    # Write a non NULL byte at the bottom right corner of the image (around 10 GB offset)
    ds.GetRasterBand(1).WriteRaster(x, y, 1, 1, data)
    ds = None

    # Check that we can fetch it at the right value
    ds = gdal.Open('tmp/nitf40.ntf')
    if ds.GetRasterBand(1).ReadRaster(x, y, 1, 1) != data:
        return 'fail'
    ds = None

    # Check that it is indeed at a very far offset, and that the NITF driver hasn't
    # put it somewhere else due to unvoluntary cast to 32bit integer...
    blockWidth = 256
    blockHeight = 256
    nBlockx = int((width+blockWidth-1)/blockWidth)
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
    if val != 123:
        gdaltest.post_reason('Bad value at offset %d : %d' % (offset, val))
        return 'fail'

    return 'success'


###############################################################################
# Check reading a 12-bit JPEG compressed NITF

def nitf_41():

    # Check if JPEG driver supports 12bit JPEG reading/writing
    jpg_drv = gdal.GetDriverByName('JPEG')
    md = jpg_drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    try:
        os.remove('data/U_4017A.NTF.aux.xml')
    except:
        pass

    ds = gdal.Open('data/U_4017A.NTF')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    if stats[2] < 2385 or stats[2] > 2386:
        print(stats)
        return 'fail'
    ds = None

    try:
        os.remove('data/U_4017A.NTF.aux.xml')
    except:
        pass

    return 'success'

###############################################################################
# Check creating a 12-bit JPEG compressed NITF

def nitf_42():

    # Check if JPEG driver supports 12bit JPEG reading/writing
    jpg_drv = gdal.GetDriverByName('JPEG')
    md = jpg_drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    ds = gdal.Open('data/U_4017A.NTF')
    out_ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf42.ntf', ds, options = ['IC=C3', 'FHDR=NITF02.10'])
    out_ds = None

    ds = gdal.Open('tmp/nitf42.ntf')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    if stats[2] < 2385 or stats[2] > 2386:
        print(stats)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test CreateCopy() in IC=C8 with various JPEG2000 drivers

def nitf_43(driver_to_test, options):

    try:
        jp2_drv = gdal.GetDriverByName( driver_to_test )
        if driver_to_test == 'JP2ECW' and jp2_drv is not None:
            if 'DMD_CREATIONOPTIONLIST' not in jp2_drv.GetMetadata():
                jp2_drv = None
    except:
        jp2_drv = None

    if jp2_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('data/byte.tif')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds = gdal.GetDriverByName('NITF').CreateCopy('tmp/nitf_43.ntf', ds, options = options, strict=0)
    gdal.PopErrorHandler()
    out_ds = None
    out_ds = gdal.Open('tmp/nitf_43.ntf')
    if out_ds.GetRasterBand(1).Checksum() == 4672:
        ret = 'success'
    else:
        ret = 'fail'
    out_ds = None
    gdal.GetDriverByName('NITF').Delete('tmp/nitf_43.ntf')

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret
    
def nitf_43_jasper():
    return nitf_43('JPEG2000', ['IC=C8'])

def nitf_43_jp2ecw():
    import ecw
    if not ecw.has_write_support():
        return 'skip'
    return nitf_43('JP2ECW', ['IC=C8', 'TARGET=0'])

def nitf_43_jp2kak():
    return nitf_43('JP2KAK', ['IC=C8', 'QUALITY=100'])

###############################################################################
# Check creating a monoblock 10000x1 image (ticket #3263)

def nitf_44():

    out_ds = gdal.GetDriverByName('NITF').Create('tmp/nitf44.ntf', 10000, 1)
    out_ds.GetRasterBand(1).Fill(255)
    out_ds = None

    ds = gdal.Open('tmp/nitf44.ntf')
    
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ds.GetRasterBand(1).GetBlockSize()
        if blockx != 10000:
            return 'fail'
    
    if ds.GetRasterBand(1).Checksum() != 57182:
        return 'fail'
    ds = None

    return 'success'
    
###############################################################################
# Check overviews on a JPEG compressed subdataset

def nitf_45():

    try:
        os.remove('tmp/nitf45.ntf.aux.xml')
    except:
        pass

    shutil.copyfile( 'data/two_images_jpeg.ntf', 'tmp/nitf45.ntf' )
    
    ds = gdal.Open( 'NITF_IM:1:tmp/nitf45.ntf', gdal.GA_Update )
    ds.BuildOverviews( overviewlist = [2] )
    # FIXME ? ds.GetRasterBand(1).GetOverview(0) is None until we reopen
    ds = None
    
    ds = gdal.Open( 'NITF_IM:1:tmp/nitf45.ntf' )
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 1086:
        print(cs)
        gdaltest.post_reason('did not get expected checksum for overview of subdataset')
        return 'fail'
    
    ds = None
    
    return 'success'
    
###############################################################################
# Check overviews on a JPEG2000 compressed subdataset

def nitf_46(driver_to_test):

    try:
        jp2_drv = gdal.GetDriverByName( driver_to_test )
    except:
        jp2_drv = None

    if jp2_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)
    
    try:
        os.remove('tmp/nitf46.ntf.aux.xml')
    except:
        pass
    
    try:
        os.remove('tmp/nitf46.ntf_0.ovr')
    except:
        pass
        
    shutil.copyfile( 'data/two_images_jp2.ntf', 'tmp/nitf46.ntf' )
    
    ds = gdal.Open( 'NITF_IM:1:tmp/nitf46.ntf', gdal.GA_Update )
    ds.BuildOverviews( overviewlist = [2] )
    # FIXME ? ds.GetRasterBand(1).GetOverview(0) is None until we reopen
    ds = None
    
    ds = gdal.Open( 'NITF_IM:1:tmp/nitf46.ntf' )
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

# untested yet
def nitf_46_jp2kak():
    return nitf_46('JP2KAK')

def nitf_46_jasper():
    return nitf_46('JPEG2000')

def nitf_46_openjpeg():
    return nitf_46('JP2OpenJPEG')

###############################################################################
# Check reading of rsets.

def nitf_47():

    ds = gdal.Open( 'data/rset.ntf.r0' )

    band = ds.GetRasterBand(2)
    if band.GetOverviewCount() != 2:
        gdaltest.post_reason( 'did not get the expected number of rset overviews.' )
        return 'fail'

    cs = band.GetOverview(1).Checksum()
    if cs != 1297:
        print(cs)
        gdaltest.post_reason('did not get expected checksum for overview of subdataset')
        return 'fail'
    
    ds = None
    
    return 'success'
    
###############################################################################
# Check building of standard overviews in place of rset overviews.

def nitf_48():

    try:
        os.remove('tmp/rset.ntf.r0')
        os.remove('tmp/rset.ntf.r1')
        os.remove('tmp/rset.ntf.r2')
        os.remove('tmp/rset.ntf.r0.ovr')
    except:
        pass

    shutil.copyfile( 'data/rset.ntf.r0', 'tmp/rset.ntf.r0' )
    shutil.copyfile( 'data/rset.ntf.r1', 'tmp/rset.ntf.r1' )
    shutil.copyfile( 'data/rset.ntf.r2', 'tmp/rset.ntf.r2' )
    
    ds = gdal.Open( 'tmp/rset.ntf.r0', gdal.GA_Update )
    ds.BuildOverviews( overviewlist = [3] )
    ds = None
    
    ds = gdal.Open( 'tmp/rset.ntf.r0' )
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason( 'did not get the expected number of rset overviews.' )
        return 'fail'
    
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 2328:
        print(cs)
        gdaltest.post_reason('did not get expected checksum for overview of subdataset')
        return 'fail'
    
    ds = None
    
    try:
        os.remove('tmp/rset.ntf.r0')
        os.remove('tmp/rset.ntf.r1')
        os.remove('tmp/rset.ntf.r2')
        os.remove('tmp/rset.ntf.r0.ovr')
    except:
        pass

    return 'success'

###############################################################################
# Test TEXT and CGM creation options with CreateCopy() (#3376)

def nitf_49():

    options = [ "TEXT=DATA_0=COUCOU",
                "TEXT=HEADER_0=ABC", # This content is invalid but who cares here
                "CGM=SEGMENT_COUNT=1",
                "CGM=SEGMENT_0_SLOC_ROW=25",
                "CGM=SEGMENT_0_SLOC_COL=25",
                "CGM=SEGMENT_0_SDLVL=2",
                "CGM=SEGMENT_0_SALVL=1",
                "CGM=SEGMENT_0_DATA=XYZ" ]

    src_ds = gdal.Open('data/text_md.vrt')

    # This will check that the creation option overrides the TEXT metadata domain from the source
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf49.ntf', src_ds,
                                                  options = options )

    # Test copy from source TEXT and CGM metadata domains
    ds2 = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf49_2.ntf', ds )

    md = ds2.GetMetadata('TEXT')
    if 'DATA_0' not in md or md['DATA_0'] != 'COUCOU' or \
       'HEADER_0' not in md or md['HEADER_0'].find('ABC  ') == -1:
        gdaltest.post_reason('did not get expected TEXT metadata')
        print(md)
        return 'success'

    md = ds2.GetMetadata('CGM')
    if 'SEGMENT_COUNT' not in md or md['SEGMENT_COUNT'] != '1' or \
       'SEGMENT_0_DATA' not in md or md['SEGMENT_0_DATA'] != 'XYZ' :
        gdaltest.post_reason('did not get expected CGM metadata')
        print(md)
        return 'success'

    src_ds = None
    ds = None
    ds2 = None

    return 'success'

###############################################################################
# Test TEXT and CGM creation options with Create() (#3376)

def nitf_50():

    options = [ #"IC=C8",
                "TEXT=DATA_0=COUCOU",
                "TEXT=HEADER_0=ABC", # This content is invalid but who cares here
                "CGM=SEGMENT_COUNT=1",
                "CGM=SEGMENT_0_SLOC_ROW=25",
                "CGM=SEGMENT_0_SLOC_COL=25",
                "CGM=SEGMENT_0_SDLVL=2",
                "CGM=SEGMENT_0_SALVL=1",
                "CGM=SEGMENT_0_DATA=XYZ" ]

    try:
        os.remove('tmp/nitf50.ntf')
    except:
        pass

    # This will check that the creation option overrides the TEXT metadata domain from the source
    ds = gdal.GetDriverByName('NITF').Create( 'tmp/nitf50.ntf', 100, 100, 3, options = options )

    ds.WriteRaster( 0, 0, 100, 100, '   ', 1, 1,
                    buf_type = gdal.GDT_Byte,
                    band_list = [1,2,3] )

    ds.GetRasterBand( 1 ).SetRasterColorInterpretation( gdal.GCI_BlueBand )
    ds.GetRasterBand( 2 ).SetRasterColorInterpretation( gdal.GCI_GreenBand )
    ds.GetRasterBand( 3 ).SetRasterColorInterpretation( gdal.GCI_RedBand )

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
        return 'success'

    md = ds.GetMetadata('CGM')
    if 'SEGMENT_COUNT' not in md or md['SEGMENT_COUNT'] != '1' or \
       'SEGMENT_0_DATA' not in md or md['SEGMENT_0_DATA'] != 'XYZ' :
        gdaltest.post_reason('did not get expected CGM metadata')
        print(md)
        return 'success'

    ds = None

    return 'success'

###############################################################################
# Test reading very small images with NBPP < 8 or NBPP == 12

def nitf_51():
    import struct

    for xsize in range(1,9):
        for nbpp in [1,2,3,4,5,6,7,12]:
            ds = gdal.GetDriverByName('NITF').Create( 'tmp/nitf51.ntf', xsize, 1 )
            ds = None

            f = open('tmp/nitf51.ntf', 'rb+')
            # Patch NBPP value at offset 811
            f.seek(811)
            f.write(struct.pack('B' * 2, 48 + int(nbpp/10), 48 + nbpp % 10))

            # Write image data
            f.seek(843)
            n = int((xsize * nbpp+7) / 8)
            for i in range(n):
                f.write(struct.pack('B' * 1, 255))

            f.close()

            ds = gdal.Open('tmp/nitf51.ntf')
            if nbpp == 12:
                data = ds.GetRasterBand(1).ReadRaster(0, 0, xsize, 1, buf_type = gdal.GDT_UInt16)
                arr = struct.unpack('H' * xsize, data)
            else:
                data = ds.GetRasterBand(1).ReadRaster(0, 0, xsize, 1)
                arr = struct.unpack('B' * xsize, data)

            ds = None

            for i in range(xsize):
                if arr[i] != (1 << nbpp) - 1:
                    gdaltest.post_reason('did not get expected data')
                    print('xsize = %d, nbpp = %d' % (xsize, nbpp))
                    print(arr)
                    return 'fail'

    return 'success'

###############################################################################
# Test reading GeoSDE TREs

def nitf_52():

    # Create a fake NITF file with GeoSDE TREs (probably not conformant, but enough to test GDAL code)
    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf52.ntf', 1, 1, options = \
         ['FILE_TRE=GEOPSB=01234567890123456789012345678901234567890123456789012345678901234567890123456789012345EURM                                                                                                                                                                                                                                                                                                                                                                 ', \
          'FILE_TRE=PRJPSB=01234567890123456789012345678901234567890123456789012345678901234567890123456789AC0000000000000000000000000000000', \
          'TRE=MAPLOB=M  0001000010000000000100000000000005000000'])
    ds = None

    ds = gdal.Open('tmp/nitf52.ntf')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    if wkt != """PROJCS["unnamed",GEOGCS["EUROPEAN 1950, Mean (3 Param)",DATUM["EUROPEAN 1950, Mean (3 Param)",SPHEROID["International 1924            ",6378388,297],TOWGS84[-87,-98,-121,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Albers_Conic_Equal_Area"],PARAMETER["standard_parallel_1",0],PARAMETER["standard_parallel_2",0],PARAMETER["latitude_of_center",0],PARAMETER["longitude_of_center",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]""":
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    if gt != (100000.0, 10.0, 0.0, 5000000.0, 0.0, -10.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading UTM MGRS

def nitf_53():

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf53.ntf', 2, 2, options = ['ICORDS=N'])
    ds = None

    f = open('tmp/nitf53.ntf', 'rb+')

    # Patch ICORDS and IGEOLO
    f.seek(775)
    if version_info >= (3,0,0):
        exec("f.write(b'U')")
        exec("f.write(b'31UBQ1000040000')")
        exec("f.write(b'31UBQ2000040000')")
        exec("f.write(b'31UBQ2000030000')")
        exec("f.write(b'31UBQ1000030000')")
    else:
        f.write('U')
        f.write('31UBQ1000040000')
        f.write('31UBQ2000040000')
        f.write('31UBQ2000030000')
        f.write('31UBQ1000030000')

    f.close()

    ds = gdal.Open('tmp/nitf53.ntf')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    if wkt != """PROJCS["UTM Zone 31, Northern Hemisphere",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]""":
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    if gt != (205000.0, 10000.0, 0.0, 5445000.0, 0.0, -10000.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading RPC00B

def nitf_54():

    # Create a fake NITF file with RPC00B TRE (probably not conformant, but enough to test GDAL code)
    RPC00B='100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf54.ntf', 1, 1, options = ['TRE=RPC00B=' + RPC00B])
    ds = None

    ds = gdal.Open('tmp/nitf54.ntf')
    md = ds.GetMetadata('RPC')
    ds = None

    if md is None or 'HEIGHT_OFF' not in md:
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test reading ICHIPB

def nitf_55():

    # Create a fake NITF file with ICHIPB TRE (probably not conformant, but enough to test GDAL code)
    ICHIPB='00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf55.ntf', 1, 1, options = ['TRE=ICHIPB=' + ICHIPB])
    ds = None

    ds = gdal.Open('tmp/nitf55.ntf')
    md = ds.GetMetadata()
    ds = None

    if md is None or 'ICHIP_SCALE_FACTOR' not in md:
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test reading USE00A

def nitf_56():

    # Create a fake NITF file with USE00A TRE (probably not conformant, but enough to test GDAL code)
    USE00A='00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf56.ntf', 1, 1, options = ['TRE=USE00A=' + USE00A])
    ds = None

    ds = gdal.Open('tmp/nitf56.ntf')
    md = ds.GetMetadata()
    ds = None

    if md is None or 'NITF_USE00A_ANGLE_TO_NORTH' not in md:
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GEOLOB

def nitf_57():

    # Create a fake NITF file with GEOLOB TRE
    GEOLOB='000000360000000360-180.000000000090.000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf57.ntf', 1, 1, options = ['TRE=GEOLOB=' + GEOLOB])
    ds = None

    ds = gdal.Open('tmp/nitf57.ntf')
    gt = ds.GetGeoTransform()
    ds = None

    if gt != (-180.0, 1.0, 0.0, 90.0, 0.0, -1.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return 'success'

    return 'success'

###############################################################################
# Test reading STDIDC

def nitf_58():

    # Create a fake NITF file with STDIDC TRE (probably not conformant, but enough to test GDAL code)
    STDIDC='00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf58.ntf', 1, 1, options = ['TRE=STDIDC=' + STDIDC])
    ds = None

    ds = gdal.Open('tmp/nitf58.ntf')
    md = ds.GetMetadata()
    ds = None

    if md is None or 'NITF_STDIDC_ACQUISITION_DATE' not in md:
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test georeferencing through .nfw and .hdr files

def nitf_59():

    shutil.copyfile('data/nitf59.nfw', 'tmp/nitf59.nfw')
    shutil.copyfile('data/nitf59.hdr', 'tmp/nitf59.hdr')
    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf59.ntf', 1, 1, options = ['ICORDS=N'])
    ds = None

    ds = gdal.Open('tmp/nitf59.ntf')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    if wkt != """PROJCS["UTM Zone 31, Northern Hemisphere",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]""":
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    if gt != (149999.5, 1.0, 0.0, 4500000.5, 0.0, -1.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading CADRG polar tile georeferencing (#2940)

def nitf_60():

    # Shut down errors because the file is truncated
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/testtest.on9')
    gdal.PopErrorHandler()
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    if wkt != """PROJCS["unnamed",GEOGCS["unnamed ellipse",DATUM["unknown",SPHEROID["unnamed",6378137,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Azimuthal_Equidistant"],PARAMETER["latitude_of_center",90],PARAMETER["longitude_of_center",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]""":
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    ref_gt = [1036422.8453166834, 149.94543479697344, 0.0, 345474.28177222813, 0.0, -149.94543479697404]
    for i in range(6):
        if abs(gt[i]-ref_gt[i]) > 1e-6:
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)
            return 'fail'

    return 'success'

###############################################################################
# Test reading TRE from DE segment

def nitf_61():

    # Derived from http://www.gwg.nga.mil/ntb/baseline/software/testfile/rsm/SampleFiles/FrameSet1/NITF_Files/i_6130a.zip
    # but hand edited to have just 1x1 imagery
    ds = gdal.Open('data/i_6130a_truncated.ntf')
    md = ds.GetMetadata('TRE')
    xml_tre = ds.GetMetadata('xml:TRE')[0]
    ds = None

    if md is None or 'RSMDCA' not in md or 'RSMECA' not in md or 'RSMPCA' not in md or 'RSMIDA' not in md:
        print(md)
        return 'fail'

    if xml_tre.find('<tre name="RSMDCA"') == -1:
        gdaltest.post_reason('did not get expected xml:TRE')
        print(xml_tre[0])
        return 'fail'

    return 'success'

###############################################################################
# Test creating & reading image comments

def nitf_62():

    # 80+1 characters
    comments = '012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678ZA'

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf62.ntf', 1, 1, options = ['ICOM=' + comments])
    ds = None

    ds = gdal.Open('tmp/nitf62.ntf')
    md = ds.GetMetadata()
    ds = None

    got_comments = md['NITF_IMAGE_COMMENTS']
    if len(got_comments) != 160 or got_comments.find(comments) == -1:
        gdaltest.post_reason('did not get expected comments')
        print("'%s'" % got_comments)
        return 'fail'

    return 'success'

###############################################################################
# Test NITFReadImageLine() and NITFWriteImageLine() when nCols < nBlockWidth (#3551)

def nitf_63():

    ds = gdal.GetDriverByName('NITF').Create('tmp/nitf63.ntf', 50, 25, 3, gdal.GDT_Int16, options = ['BLOCKXSIZE=256'])
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
    if md['NITF_IMODE'] != 'P':
        gdaltest.post_reason('wrong IMODE')
        return 'fail'
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(127)
    ds.GetRasterBand(3).Fill(255)
    ds = None

    ds = gdal.Open('tmp/nitf63.ntf')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    ds = None

    if cs1 != 0 or cs2 != 14186 or cs3 != 15301:
        gdaltest.post_reason('did not get expected checksums : (%d, %d, %d) instead of (0, 14186, 15301)' % (cs1, cs2, cs3))
        return 'fail'


    return 'success'

###############################################################################
# Test SDE_TRE creation option

def nitf_64():

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/nitf_64.tif', 256, 256, 1)
    src_ds.SetGeoTransform([2.123456789, 0.123456789, 0, 49.123456789, 0, -0.123456789])
    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('WGS84')
    src_ds.SetProjection(sr.ExportToWkt())

    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_64.ntf', src_ds, options = ['ICORDS=D'])
    ds = None


    ds = gdal.Open('/vsimem/nitf_64.ntf')
    # One can notice that the topleft location is only precise to the 3th decimal !
    expected_gt = (2.123270588235294, 0.12345882352941177, 0.0, 49.123729411764707, 0.0, -0.12345882352941176)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(expected_gt[i] - got_gt[i]) > 1e-10:
            gdaltest.post_reason('did not get expected GT in ICORDS=D mode')
            print(got_gt)
            return 'fail'
    ds = None



    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_64.ntf', src_ds, options = ['ICORDS=G'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_64.ntf')
    # One can notice that the topleft location is only precise to the 3th decimal !
    expected_gt = (2.1235495642701521, 0.12345642701525053, 0.0, 49.123394880174288, 0.0, -0.12345642701525052)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(expected_gt[i] - got_gt[i]) > 1e-10:
            gdaltest.post_reason('did not get expected GT in ICORDS=G mode')
            print(got_gt)
            return 'fail'
    ds = None
    

    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_64.ntf', src_ds, options = ['SDE_TRE=YES'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_64.ntf')
    # One can notice that the topleft location is precise up to the 9th decimal
    expected_gt = (2.123456789, 0.1234567901234568, 0.0, 49.123456789000002, 0.0, -0.12345679012345678)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(expected_gt[i] - got_gt[i]) > 1e-10:
            gdaltest.post_reason('did not get expected GT in SDE_TRE mode')
            print(got_gt)
            return 'fail'
    ds = None

    src_ds = None
    gdal.Unlink('/vsimem/nitf_64.tif')
    gdal.Unlink('/vsimem/nitf_64.ntf')

    return 'success'

###############################################################################
# Test creating an image with block_width = image_width > 8192 (#3922)

def nitf_65():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_65.ntf', 10000, 100, options = ['BLOCKXSIZE=10000'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_65.ntf')
    (block_xsize, block_ysize) = ds.GetRasterBand(1).GetBlockSize()
    ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/nitf_65.ntf')

    if block_xsize != 10000:
        print(block_xsize)
        return 'fail'

    return 'success'

###############################################################################
# Test creating an image with block_height = image_height > 8192 (#3922)

def nitf_66():

    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_66.ntf', 100, 10000, options = ['BLOCKYSIZE=10000', 'BLOCKXSIZE=50'])
    ds = None

    ds = gdal.Open('/vsimem/nitf_66.ntf')
    (block_xsize, block_ysize) = ds.GetRasterBand(1).GetBlockSize()
    ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/nitf_66.ntf')

    if block_ysize != 10000:
        print(block_ysize)
        return 'fail'

    return 'success'

###############################################################################
# Test that we don't use scanline access in illegal cases (#3926)

def nitf_67():

    src_ds = gdal.Open('data/byte.tif')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.GetDriverByName('NITF').CreateCopy('/vsimem/nitf_67.ntf', src_ds, options = ['BLOCKYSIZE=1', 'BLOCKXSIZE=10'], strict=0)
    gdal.PopErrorHandler()
    ds = None
    src_ds = None

    ds = gdal.Open('/vsimem/nitf_67.ntf')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/nitf_67.ntf')

    if cs != 4672:
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test reading NITF_METADATA domain

def nitf_68():

    ds = gdal.Open('data/rgb.ntf')
    if len(ds.GetMetadata('NITF_METADATA')) != 2:
        print(ds.GetMetadata('NITF_METADATA'))
        return 'fail'
    ds = None

    ds = gdal.Open('data/rgb.ntf')
    if len(ds.GetMetadataItem('NITFFileHeader','NITF_METADATA')) == 0:
        print(ds.GetMetadataItem('NITFFileHeader','NITF_METADATA'))
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test SetGCPs() support

def nitf_69():

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
    ds = gdal.GetDriverByName('NITF').Create('/vsimem/nitf_69_dest.ntf', 20, 20, 1, options = ['ICORDS=G'])
    ds.SetGCPs(src_ds.GetGCPs(), src_ds.GetGCPProjection())
    ds.SetGCPs(src_ds.GetGCPs(), src_ds.GetGCPProjection()) # To check we can call it several times without error
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
    if abs(got_gcps[0].GCPPixel - 0.5) > 1e-5 or abs(got_gcps[0].GCPLine - 0.5) > 1e-5 or \
       abs(got_gcps[0].GCPX - 2) > 1e-5 or abs(got_gcps[0].GCPY - 49) > 1e-5:
        gdaltest.post_reason('wrong gcp')
        print(got_gcps[0])
        return 'fail'

    # Upper-right
    if abs(got_gcps[1].GCPPixel - 19.5) > 1e-5 or abs(got_gcps[1].GCPLine - 0.5) > 1e-5 or \
       abs(got_gcps[1].GCPX - 3) > 1e-5 or abs(got_gcps[1].GCPY - 49.5) > 1e-5:
        gdaltest.post_reason('wrong gcp')
        print(got_gcps[1])
        return 'fail'

    # Lower-right
    if abs(got_gcps[2].GCPPixel - 19.5) > 1e-5 or abs(got_gcps[2].GCPLine - 19.5) > 1e-5 or \
       abs(got_gcps[2].GCPX - 3) > 1e-5 or abs(got_gcps[2].GCPY - 48) > 1e-5:
        gdaltest.post_reason('wrong gcp')
        print(got_gcps[2])
        return 'fail'

    # Lower-left
    if abs(got_gcps[3].GCPPixel - 0.5) > 1e-5 or abs(got_gcps[3].GCPLine - 19.5) > 1e-5 or \
       abs(got_gcps[3].GCPX - 2) > 1e-5 or abs(got_gcps[3].GCPY - 48) > 1e-5:
        gdaltest.post_reason('wrong gcp')
        print(got_gcps[3])
        return 'fail'

    return 'success'

###############################################################################
# Create and read a JPEG encoded NITF file with NITF dimensions != JPEG dimensions

def nitf_70():

    src_ds = gdal.Open( 'data/rgbsmall.tif' )

    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf_70.ntf', src_ds,
                                                  options = ['IC=C3', 'BLOCKXSIZE=64', 'BLOCKYSIZE=64'] )
    ds = None

    # For comparison
    ds = gdal.GetDriverByName('GTiff').CreateCopy( 'tmp/nitf_70.tif', src_ds,
                                                  options = ['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'TILED=YES', 'BLOCKXSIZE=64', 'BLOCKYSIZE=64'] )
    ds = None
    src_ds = None

    ds = gdal.Open( 'tmp/nitf_70.ntf' )
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    
    ds = gdal.Open( 'tmp/nitf_70.tif' )
    cs_ref = ds.GetRasterBand(1).Checksum()
    ds = None
    
    gdal.GetDriverByName('NITF').Delete( 'tmp/nitf_70.ntf' )
    gdal.GetDriverByName('GTiff').Delete( 'tmp/nitf_70.tif' )

    if cs != cs_ref:
        print(cs)
        print(cs_ref)
        return 'fail'

    return 'success'

###############################################################################
# Test NITF21_CGM_ANNO_Uncompressed_unmasked.ntf for bug #1313 and #1714

def nitf_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/bugs/NITF21_CGM_ANNO_Uncompressed_unmasked.ntf', 'NITF21_CGM_ANNO_Uncompressed_unmasked.ntf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/NITF21_CGM_ANNO_Uncompressed_unmasked.ntf', 1, 13123, filename_absolute = 1 )

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ret = tst.testOpen()
    gdal.PopErrorHandler()

    return ret

###############################################################################
# Test NITF file with multiple images

def nitf_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf1.1/U_0001a.ntf', 'U_0001a.ntf'):
        return 'skip'

    ds = gdal.Open( 'tmp/cache/U_0001a.ntf' )

    md = ds.GetMetadata('SUBDATASETS')
    if 'SUBDATASET_1_NAME' not in md:
        gdaltest.post_reason( 'missing SUBDATASET_1_NAME metadata' )
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test ARIDPCM (C2) image

def nitf_online_3():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf1.1/U_0001a.ntf', 'U_0001a.ntf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'NITF_IM:3:tmp/cache/U_0001a.ntf', 1, 23463, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# Test Vector Quantization (VQ) (C4) file

def nitf_online_4():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/cadrg/001zc013.on1', '001zc013.on1'):
        return 'skip'

    # check that the RPF attribute metadata was carried through.
    ds = gdal.Open( 'tmp/cache/001zc013.on1' )
    md = ds.GetMetadata()
    if md['NITF_RPF_CurrencyDate'] != '19950720' \
       or md['NITF_RPF_ProductionDate'] != '19950720' \
       or md['NITF_RPF_SignificantDate'] != '19890629':
        gdaltest.post_reason( 'RPF attribute metadata not captured (#3413)')
        return 'fail'

    ds = None

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/001zc013.on1', 1, 53960, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# Test Vector Quantization (VQ) (M4) file

def nitf_online_5():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/cadrg/overview.ovr', 'overview.ovr'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/overview.ovr', 1, 60699, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# Test a JPEG compressed, single blocked 2048x2048 mono image

def nitf_online_6():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_4001b.ntf', 'U_4001b.ntf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/U_4001b.ntf', 1, 60030, filename_absolute = 1 )

    return tst.testOpen()


###############################################################################
# Test all combinations of IMODE (S,P,B,R) for an image with 6 bands whose 3 are RGB

def nitf_online_7():

    files = [ 'ns3228b.nsf', 'i_3228c.ntf', 'ns3228d.nsf', 'i_3228e.ntf' ]
    for file in files:
        if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/' + file, file):
            return 'skip'

        ds = gdal.Open('tmp/cache/' + file)
        if ds.RasterCount != 6:
            return 'fail'

        checksums = [ 48385, 48385, 40551, 54223, 48385, 33094 ]
        colorInterpretations = [ gdal.GCI_Undefined, gdal.GCI_Undefined, gdal.GCI_RedBand, gdal.GCI_BlueBand, gdal.GCI_Undefined, gdal.GCI_GreenBand ]

        for i in range(6):
            cs = ds.GetRasterBand(i+1).Checksum()
            if cs != checksums[i]:
                gdaltest.post_reason( 'got checksum %d for image %s' \
                                      % (cs, file) )
                return 'fail'
            
            if ds.GetRasterBand(i+1).GetRasterColorInterpretation() != colorInterpretations[i]:
                gdaltest.post_reason( 'got wrong color interp for image %s' \
                                      % file )
                return 'fail'
        ds = None
        
        #shutil.copyfile('tmp/cache/' + file, 'tmp/' + file)
        #ds = gdal.Open('tmp/' + file, gdal.GA_Update)
        #data = ds.GetRasterBand(1).ReadRaster(0, 0, 1024, 1024)
        #ds.GetRasterBand(1).Fill(0)
        #ds = None
        
        #ds = gdal.Open('tmp/' + file, gdal.GA_Update)
        #ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        #ds = None
        
        #ds = gdal.Open('tmp/' + file)
        #print(ds.GetRasterBand(1).Checksum())
        #ds = None
        
        #os.remove('tmp/' + file)

    return 'success'

###############################################################################
# Test JPEG-compressed multi-block mono-band image with a data mask subheader (IC=M3, IMODE=B)

def nitf_online_8():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3301j.nsf', 'ns3301j.nsf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/ns3301j.nsf', 1, 56861, filename_absolute = 1 )

    return tst.testOpen()


###############################################################################
# Test JPEG-compressed multi-block mono-band image without a data mask subheader (IC=C3, IMODE=B)

def nitf_online_9():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3304a.nsf', 'ns3304a.nsf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/ns3304a.nsf', 1, 32419, filename_absolute = 1 )

    return tst.testOpen()


###############################################################################
# Verify that CGM access on a file with 8 CGM segments

def nitf_online_10():


    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3119b.nsf', 'ns3119b.nsf'):
        return 'skip'

    # Shut up the warning about missing image segment
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open( 'tmp/cache/ns3119b.nsf' )
    gdal.PopErrorHandler()

    mdCGM = ds.GetMetadata( 'CGM' )

    ds = None

    if mdCGM['SEGMENT_COUNT'] != '8':
        gdaltest.post_reason( 'wrong SEGMENT_COUNT.' )
        return 'fail'

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
        if mdCGM[item[0]] != item[1]:
            gdaltest.post_reason( 'wrong value for %s.' % item[0] )
            return 'fail'

    return 'success'

###############################################################################
# 5 text files

def nitf_online_11():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_1122a.ntf', 'U_1122a.ntf'):
        return 'skip'

    ds = gdal.Open( 'tmp/cache/U_1122a.ntf' )

    mdTEXT = ds.GetMetadata( 'TEXT' )

    ds = None

    if mdTEXT['DATA_0'] != 'This is test text file 01.\r\n':
        gdaltest.post_reason( 'did not find expected DATA_0 from metadata.' )
        return 'fail'
    if mdTEXT['DATA_1'] != 'This is test text file 02.\r\n':
        gdaltest.post_reason( 'did not find expected DATA_1 from metadata.' )
        return 'fail'
    if mdTEXT['DATA_2'] != 'This is test text file 03.\r\n':
        gdaltest.post_reason( 'did not find expected DATA_2 from metadata.' )
        return 'fail'
    if mdTEXT['DATA_3'] != 'This is test text file 04.\r\n':
        gdaltest.post_reason( 'did not find expected DATA_3 from metadata.' )
        return 'fail'
    if mdTEXT['DATA_4'] != 'This is test text file 05.\r\n':
        gdaltest.post_reason( 'did not find expected DATA_4 from metadata.' )
        return 'fail'

    return 'success'


###############################################################################
# Test 12 bit uncompressed image.

def nitf_online_12():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/bugs/i_3430a.ntf', 'i_3430a.ntf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/i_3430a.ntf', 1, 38647,
                             filename_absolute = 1 )

    return tst.testOpen()


###############################################################################
# Test complex relative graphic/image attachment.

def nitf_online_13():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/u_3054a.ntf', 'u_3054a.ntf'):
        return 'skip'

    # Shut up the warning about missing image segment
    ds = gdal.Open( 'NITF_IM:2:tmp/cache/u_3054a.ntf' )

    mdCGM = ds.GetMetadata( 'CGM' )
    md = ds.GetMetadata()

    ds = None

    if mdCGM['SEGMENT_COUNT'] != '3':
        gdaltest.post_reason( 'wrong SEGMENT_COUNT.' )
        return 'fail'

    tab = [
        ('SEGMENT_2_SLOC_ROW', '0'),
        ('SEGMENT_2_SLOC_COL', '0'),
        ('SEGMENT_2_CCS_COL', '1100'),
        ('SEGMENT_2_CCS_COL', '1100'),
        ('SEGMENT_2_SDLVL', '6'),
        ('SEGMENT_2_SALVL', '3')
        ]

    for item in tab:
        if mdCGM[item[0]] != item[1]:
            gdaltest.post_reason( 'wrong value for %s.' % item[0] )
            return 'fail'

    tab = [
        ('NITF_IDLVL','3'),
        ('NITF_IALVL','1'),
        ('NITF_ILOC_ROW','1100'),
        ('NITF_ILOC_COLUMN','1100'),
        ('NITF_CCS_ROW','1100'),
        ('NITF_CCS_COLUMN','1100'),
        ]

    for item in tab:
        if md[item[0]] != item[1]:
            gdaltest.post_reason( 'wrong value for %s, got %s instead of %s.'
                                  % (item[0], md[item[0]], item[1]) )
            return 'fail'

    return 'success'


###############################################################################
# Check reading a 12-bit JPEG compressed NITF (multi-block)

def nitf_online_14():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_4020h.ntf', 'U_4020h.ntf'):
        return 'skip'

    try:
        os.remove('tmp/cache/U_4020h.ntf.aux.xml')
    except:
        pass

    # Check if JPEG driver supports 12bit JPEG reading/writing
    jpg_drv = gdal.GetDriverByName('JPEG')
    md = jpg_drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    ds = gdal.Open('tmp/cache/U_4020h.ntf')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    if stats[2] < 2607 or stats[2] > 2608:
        print(stats)
        return 'fail'
    ds = None

    try:
        os.remove('tmp/cache/U_4020h.ntf.aux.xml')
    except:
        pass

    return 'success'
    
###############################################################################
# Test opening a IC=C8 NITF file with the various JPEG2000 drivers

def nitf_online_15(driver_to_test, expected_cs = 1054):
    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/p0_01/p0_01a.ntf', 'p0_01a.ntf'):
        return 'skip'

    try:
        jp2_drv = gdal.GetDriverByName( driver_to_test )
    except:
        jp2_drv = None

    if jp2_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('tmp/cache/p0_01a.ntf')
    if ds.GetRasterBand(1).Checksum() == expected_cs:
        ret = 'success'
    else:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason( 'Did not get expected checksums' );
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

def nitf_online_15_jp2ecw():
    return nitf_online_15('JP2ECW')

def nitf_online_15_jp2mrsid():
    return nitf_online_15('JP2MrSID')

# untested yet
def nitf_online_15_jp2kak():
    return nitf_online_15('JP2KAK')

def nitf_online_15_jasper():
    return nitf_online_15('JPEG2000')

def nitf_online_15_openjpeg():
    return nitf_online_15('JP2OpenJPEG')

###############################################################################
# Test opening a IC=C8 NITF file which has 256-entry palette/LUT in both JP2 Header and image Subheader
# We expect RGB expansion from some JPEG2000 driver

def nitf_online_16(driver_to_test):
    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9_jp2_2places.ntf', 'file9_jp2_2places.ntf'):
        return 'skip'

    try:
        jp2_drv = gdal.GetDriverByName( driver_to_test )
    except:
        jp2_drv = None

    if jp2_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('tmp/cache/file9_jp2_2places.ntf')
    # JPEG2000 driver
    if ds.RasterCount == 3 and \
       ds.GetRasterBand(1).Checksum() == 48954 and \
       ds.GetRasterBand(2).Checksum() == 4939 and \
       ds.GetRasterBand(3).Checksum() == 17734 :
        ret = 'success'
        
    elif ds.RasterCount == 1 and \
       ds.GetRasterBand(1).Checksum() == 47664 and \
       ds.GetRasterBand(1).GetRasterColorTable() != None:
        print('strange, this driver does not do table color expansion... thats ok though')
        ret = 'success'
    else:
        print(ds.RasterCount)
        for i in range(ds.RasterCount):
            print(ds.GetRasterBand(i+1).Checksum())
        print(ds.GetRasterBand(1).GetRasterColorTable())
        gdaltest.post_reason( 'Did not get expected checksums' );
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

def nitf_online_16_jp2ecw():
    return nitf_online_16('JP2ECW')

def nitf_online_16_jp2mrsid():
    return nitf_online_16('JP2MrSID')

# untested yet
def nitf_online_16_jp2kak():
    return nitf_online_16('JP2KAK')

def nitf_online_16_jasper():
    return nitf_online_16('JPEG2000')

# color table unsupported by OpenJPEG
def nitf_online_16_openjpeg():
    return nitf_online_16('JP2OpenJPEG')

###############################################################################
# Test opening a IC=C8 NITF file which has 256-entry/LUT in Image Subheader, JP2 header completely removed
# We don't expect RGB expansion from the JPEG2000 driver

def nitf_online_17(driver_to_test):
    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9_j2c.ntf', 'file9_j2c.ntf'):
        return 'skip'

    try:
        jp2_drv = gdal.GetDriverByName( driver_to_test )
    except:
        jp2_drv = None

    if jp2_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but(driver_to_test)

    ds = gdal.Open('tmp/cache/file9_j2c.ntf')
    if ds.RasterCount == 1 and \
       ds.GetRasterBand(1).Checksum() == 47664 and \
       ds.GetRasterBand(1).GetRasterColorTable() != None:
        ret = 'success'
    else:
        print(ds.RasterCount)
        for i in range(ds.RasterCount):
            print(ds.GetRasterBand(i+1).Checksum())
        print(ds.GetRasterBand(1).GetRasterColorTable())
        gdaltest.post_reason( 'Did not get expected checksums' );
        ret = 'fail'

    gdaltest.reregister_all_jpeg2000_drivers()

    return ret

def nitf_online_17_jp2ecw():
    return nitf_online_17('JP2ECW')

def nitf_online_17_jp2mrsid():
    return nitf_online_17('JP2MrSID')

# untested yet
def nitf_online_17_jp2kak():
    return nitf_online_17('JP2KAK')

def nitf_online_17_jasper():
    return nitf_online_17('JPEG2000')

# color table unsupported by OpenJPEG
def nitf_online_17_openjpeg():
    return nitf_online_17('JP2OpenJPEG')

###############################################################################
# Test polar stereographic CADRG tile.  
def nitf_online_18():
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/bugs/bug3337.ntf', 'bug3337.ntf'):
        return 'skip'

    ds = gdal.Open('tmp/cache/bug3337.ntf')

    gt = ds.GetGeoTransform()
    prj = ds.GetProjection()

    # If we have functioning coordinate transformer.
    if prj[:6] == 'PROJCS':
        if prj.find('Azimuthal_Equidistant') == -1:
            gdaltest.post_reason( 'wrong projection?' )
            return 'fail'
        expected_gt=(-1669792.3618991028, 724.73626818537502, 0.0, -556597.45396636717, 0.0, -724.73626818537434)
        if not gdaltest.geotransform_equals( gt, expected_gt, 1.0 ):
            gdaltest.post_reason( 'did not get expected geotransform.' )
            return 'fail'

    # If we do not have a functioning coordinate transformer.
    else:
        if prj != '' \
             or not gdaltest.geotransform_equals(gt,(0,1,0,0,0,1),0.00000001):
            print(gt)
            print(prj)
            gdaltest.post_reason( 'did not get expected empty gt/projection' )
            return 'fail'

        prj = ds.GetGCPProjection()
        if prj[:6] != 'GEOGCS':
            gdaltest.post_reason( 'did not get expected geographic srs' )
            return 'fail'

        gcps = ds.GetGCPs()
        gcp3 = gcps[3]
        if gcp3.GCPPixel != 0 or gcp3.GCPLine != 1536 \
                or abs(gcp3.GCPX+45) > 0.0000000001 \
                or abs(gcp3.GCPY-68.78679656) > 0.00000001:
            gdaltest.post_reason( 'did not get expected gcp.')
            return 'fail'

    ds = None

    return 'success'
    
###############################################################################
# Test CADRG tile crossing dateline (#3383)

def nitf_online_19():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/0000M033.GN3', '0000M033.GN3'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/0000M033.GN3', 1, 38928,
                             filename_absolute = 1 )

    return tst.testOpen( check_gt = (174.375000000000000,0.010986328125000,0,
                                     51.923076923076927,0,-0.006760817307692) )
    
###############################################################################
# Check that the RPF attribute metadata was carried through.
# Special case where the reported size of the attribute subsection is
# smaller than really available

def nitf_online_20():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/0000M033.GN3', '0000M033.GN3'):
        return 'skip'

    # check that the RPF attribute metadata was carried through.
    # Special case where the reported size of the attribute subsection is
    # smaller than really available
    ds = gdal.Open( 'tmp/cache/0000M033.GN3' )
    md = ds.GetMetadata()
    if md['NITF_RPF_CurrencyDate'] != '19941201' \
       or md['NITF_RPF_ProductionDate'] != '19980511' \
       or md['NITF_RPF_SignificantDate'] != '19850305':
        gdaltest.post_reason( 'RPF attribute metadata not captured (#3413)')
        return 'fail'

    return 'success'

###############################################################################
# Check that we can read NITF header located in STREAMING_FILE_HEADER DE 
# segment when header at beginning of file is incomplete

def nitf_online_21():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv2_1/ns3321a.nsf', 'ns3321a.nsf'):
        return 'skip'

    ds = gdal.Open( 'tmp/cache/ns3321a.nsf' )
    md = ds.GetMetadata()
    ds = None

    # If we get NS3321A, it means we are not exploiting the header from the STREAMING_FILE_HEADER DE segment
    if md['NITF_OSTAID'] != 'I_3321A':
        gdaltest.post_reason('did not get expected OSTAID value')
        print(md['NITF_OSTAID'])
        return 'fail'

    return 'success'

###############################################################################
# Test fix for #3002 (reconcilement of NITF file with LA segments)
#

def nitf_online_22():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Nitfv1_1/U_0001C.NTF', 'U_0001C.NTF'):
        return 'skip'

    ds = gdal.Open( 'NITF_IM:1:tmp/cache/U_0001C.NTF' )
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL','6'),
        ('NITF_IALVL','1'),
        ('NITF_ILOC_ROW','360'),
        ('NITF_ILOC_COLUMN','380'),
        ('NITF_CCS_ROW','425'),
        ('NITF_CCS_COLUMN','410'),
        ]

    for item in tab:
        if md[item[0]] != item[1]:
            gdaltest.post_reason( '(1) wrong value for %s, got %s instead of %s.'
                                  % (item[0], md[item[0]], item[1]) )
            return 'fail'

    ds = gdal.Open( 'NITF_IM:2:tmp/cache/U_0001C.NTF' )
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL','11'),
        ('NITF_IALVL','2'),
        ('NITF_ILOC_ROW','360'),
        ('NITF_ILOC_COLUMN','40'),
        ('NITF_CCS_ROW','422'),
        ('NITF_CCS_COLUMN','210'),
        ]

    for item in tab:
        if md[item[0]] != item[1]:
            gdaltest.post_reason( '(2) wrong value for %s, got %s instead of %s.'
                                  % (item[0], md[item[0]], item[1]) )
            return 'fail'

    ds = gdal.Open( 'NITF_IM:3:tmp/cache/U_0001C.NTF' )
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL','5'),
        ('NITF_IALVL','3'),
        ('NITF_ILOC_ROW','40'),
        ('NITF_ILOC_COLUMN','240'),
        ('NITF_CCS_ROW','-1'),
        ('NITF_CCS_COLUMN','-1'),
        ]

    for item in tab:
        if md[item[0]] != item[1]:
            gdaltest.post_reason( '(3) wrong value for %s, got %s instead of %s.'
                                  % (item[0], md[item[0]], item[1]) )
            return 'fail'

    ds = gdal.Open( 'NITF_IM:4:tmp/cache/U_0001C.NTF' )
    md = ds.GetMetadata()
    ds = None

    tab = [
        ('NITF_IDLVL','1'),
        ('NITF_IALVL','0'),
        ('NITF_ILOC_ROW','65'),
        ('NITF_ILOC_COLUMN','30'),
        ('NITF_CCS_ROW','65'),
        ('NITF_CCS_COLUMN','30'),
        ]

    for item in tab:
        if md[item[0]] != item[1]:
            gdaltest.post_reason( '(4) wrong value for %s, got %s instead of %s.'
                                  % (item[0], md[item[0]], item[1]) )
            return 'fail'

    return 'success'

###############################################################################
# Test reading a M4 compressed file (fixed for #3848)

def nitf_online_23():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/nitf/nitf2.0/U_3058b.ntf', 'U_3058b.ntf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/U_3058b.ntf', 1, 44748, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# Test reading ECRG frames

def nitf_online_24():

    if not gdaltest.download_file('http://www.falconview.org/trac/FalconView/downloads/17', 'ECRG_Sample.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/ECRG_Sample.zip')
    except:
        return 'skip'

    oldval = gdal.GetConfigOption('NITF_OPEN_UNDERLYING_DS')
    gdal.SetConfigOption('NITF_OPEN_UNDERLYING_DS', 'NO')
    ds = gdal.Open('/vsizip/tmp/cache/ECRG_Sample.zip/ECRG_Sample/EPF/clfc/2/000000009s0013.lf2')
    gdal.SetConfigOption('NITF_OPEN_UNDERLYING_DS', oldval)
    if ds is None:
        return 'fail'
    xml_tre = ds.GetMetadata('xml:TRE')[0]
    ds = None

    if xml_tre.find('<tre name="GEOPSB"') == -1 or \
       xml_tre.find('<tre name="J2KLRA"') == -1 or \
       xml_tre.find('<tre name="GEOLOB"') == -1 or \
       xml_tre.find('<tre name="BNDPLB"') == -1 or \
       xml_tre.find('<tre name="ACCPOB"') == -1 or \
       xml_tre.find('<tre name="SOURCB"') == -1:
           gdaltest.post_reason('did not get expected xml:TRE')
           print(xml_tre)
           return 'fail'

    return 'success'

###############################################################################
# Test reading a HRE file

def nitf_online_25():

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/docs/HRE_spec/Case1_HRE10G324642N1170747W_Uxx.hr5', 'Case1_HRE10G324642N1170747W_Uxx.hr5'):
        return 'skip'

    tst = gdaltest.GDALTest( 'NITF', 'tmp/cache/Case1_HRE10G324642N1170747W_Uxx.hr5', 1, 7099, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('tmp/cache/Case1_HRE10G324642N1170747W_Uxx.hr5')
    xml_tre = ds.GetMetadata('xml:TRE')[0]
    ds = None

    if xml_tre.find('<tre name="PIAPRD"') == -1:
           gdaltest.post_reason('did not get expected xml:TRE')
           print(xml_tre)
           return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def nitf_cleanup():
    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/test_create.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf9.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/test_13.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/test_29.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/test_29_copy.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf36.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf37.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf38.ntf' )
        os.unlink( 'tmp/nitf38.ntf_0.ovr' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf39.ntf' )
    except:
        pass

    try:
        os.stat( 'tmp/nitf40.ntf' )
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf40.ntf' )
    except:
        pass

    try:
        os.stat( 'tmp/nitf42.ntf' )
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf42.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf44.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf45.ntf' )
        os.unlink( 'tmp/nitf45.ntf_0.ovr' )
    except:
        pass

    try:
        os.stat( 'tmp/nitf46.ntf' )
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf46.ntf' )
        os.unlink( 'tmp/nitf46.ntf_0.ovr' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf49.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf49_2.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf50.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf51.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf52.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf53.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf54.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf55.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf56.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf57.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf58.ntf' )
    except:
        pass

    try:
        os.remove('tmp/nitf59.hdr')
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf59.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf62.ntf' )
    except:
        pass

    try:
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf63.ntf' )
    except:
        pass

    return 'success'

gdaltest_list = [
    nitf_1,
    nitf_2,
    nitf_3,
    nitf_4,
    nitf_5,
    nitf_6,
    nitf_7,
    nitf_8,
    nitf_9,
    nitf_10,
    nitf_11,
    nitf_12,
    nitf_13,
    nitf_14,
    nitf_15,
    nitf_16,
    nitf_17,
    nitf_18,
    nitf_19,
    nitf_20,
    nitf_21,
    nitf_22,
    nitf_23,
    nitf_24,
    nitf_25,
    nitf_26,
    nitf_27,
    nitf_28_jp2ecw,
    nitf_28_jp2mrsid,
    nitf_28_jp2kak,
    nitf_29,
    nitf_30,
    nitf_31,
    nitf_32,
    nitf_33,
    nitf_34,
    nitf_35,
    nitf_36,
    nitf_37,
    nitf_38,
    nitf_39,
    nitf_40,
    nitf_41,
    nitf_42,
    nitf_43_jasper,
    nitf_43_jp2ecw,
    nitf_43_jp2kak,
    nitf_44,
    nitf_45,
    #nitf_46_jp2ecw,
    #nitf_46_jp2mrsid,
    #nitf_46_jp2kak,
    nitf_46_jasper,
    #nitf_46_openjpeg,
    nitf_47,
    nitf_48,
    nitf_49,
    nitf_50,
    nitf_51,
    nitf_52,
    nitf_53,
    nitf_54,
    nitf_55,
    nitf_56,
    nitf_57,
    nitf_58,
    nitf_59,
    nitf_60,
    nitf_61,
    nitf_62,
    nitf_63,
    nitf_64,
    nitf_65,
    nitf_66,
    nitf_67,
    nitf_68,
    nitf_69,
    nitf_70,
    nitf_online_1,
    nitf_online_2,
    nitf_online_3,
    nitf_online_4,
    nitf_online_5,
    nitf_online_6,
    nitf_online_7,
    nitf_online_8,
    nitf_online_9,
    nitf_online_10,
    nitf_online_11,
    nitf_online_12,
    nitf_online_13,
    nitf_online_14,
    nitf_online_15_jp2ecw,
    nitf_online_15_jp2mrsid,
    nitf_online_15_jp2kak,
    nitf_online_15_jasper,
    nitf_online_15_openjpeg,
    nitf_online_16_jp2ecw,
    nitf_online_16_jp2mrsid,
    nitf_online_16_jp2kak,
    nitf_online_16_jasper,
    #nitf_online_16_openjpeg,
    nitf_online_17_jp2ecw,
    nitf_online_17_jp2mrsid,
    nitf_online_17_jp2kak,
    nitf_online_17_jasper,
    #nitf_online_17_openjpeg,
    nitf_online_18,
    nitf_online_19,
    nitf_online_20,
    nitf_online_21,
    nitf_online_22,
    nitf_online_23,
    nitf_online_24,
    nitf_online_25,
    nitf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'nitf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

