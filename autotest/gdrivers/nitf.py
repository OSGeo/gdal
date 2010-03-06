#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for NITF driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
import gdal
import array
import string
import struct
import shutil

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

def nitf_create(creation_options):

    drv = gdal.GetDriverByName( 'NITF' )

    try:
        os.remove( 'tmp/test_create.ntf' )
    except:
        pass

    ds = drv.Create( 'tmp/test_create.ntf', 200, 100, 3, gdal.GDT_Byte,
                     creation_options )
    ds.SetGeoTransform( (100, 0.1, 0.0, 30.0, 0.0, -0.1 ) )

    my_list = list(range(200)) + list(range(20,220)) + list(range(30,230))
    raw_data = array.array('h',my_list).tostring()

    for line in range(100):
        ds.WriteRaster( 0, line, 200, 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1,2,3] )

    ds.GetRasterBand( 1 ).SetRasterColorInterpretation( gdal.GCI_BlueBand )
    ds.GetRasterBand( 2 ).SetRasterColorInterpretation( gdal.GCI_GreenBand )
    ds.GetRasterBand( 3 ).SetRasterColorInterpretation( gdal.GCI_RedBand )

    ds = None

    return 'success'

###############################################################################
# Test direction creation of an non-compressed NITF file.

def nitf_4():

    return nitf_create([ 'ICORDS=G' ])


###############################################################################
# Verify created file

def nitf_check_created_file(checksum1, checksum2, checksum3):
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
    
    if abs(exp_mean-mean) > 0.01 or abs(exp_stddev-stddev) > 0.01:
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

    tst = gdaltest.GDALTest( 'NITF', '../tmp/nitf9.ntf', 2, 22296 )
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
    try:
        jp2ecw_drv = gdal.GetDriverByName( 'JP2ECW' )
    except:
        jp2ecw_drv = None

    if jp2ecw_drv is None:
        return 'skip'

    # Deregister other potential conflicting JPEG2000 drivers
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2ECW')

    if nitf_create([ 'ICORDS=G', 'IC=C8' ]) == 'success':
        ret = nitf_check_created_file(32398, 42502, 38882)
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

    ret = nitf_check_created_file(32398, 42502, 38882)

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

    ret = nitf_check_created_file(32398, 42502, 38882)

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

def nitf_36():

    src_ds = gdal.Open( 'data/rgbsmall.tif' )
    ds = gdal.GetDriverByName('NITF').CreateCopy( 'tmp/nitf36.ntf', src_ds,
                                                  options = ['IC=C3', 'BLOCKSIZE=32', 'QUALITY=100'] )
    src_ds = None
    ds = None

    ds = gdal.Open( 'tmp/nitf36.ntf' )
    
    (exp_mean, exp_stddev) = (65.4208, 47.254550335)
    (mean, stddev) = ds.GetRasterBand(1).ComputeBandStats()
    
    if abs(exp_mean-mean) > 0.01 or abs(exp_stddev-stddev) > 0.01:
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
    
    if abs(exp_mean-mean) > 0.01 or abs(exp_stddev-stddev) > 0.01:
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
    nBlockx = ((width+blockWidth-1)/blockWidth)
    iBlockx = x / blockWidth
    iBlocky = y / blockHeight
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

    ds = gdal.Open('data/U_4017A.NTF')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    if stats[2] < 2385 or stats[2] > 2386:
        print(stats)
        return 'fail'
    ds = None

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
    return nitf_43('JP2ECW', ['IC=C8', 'TARGET=0'])

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

def nitf_online_15(driver_to_test):
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
    if ds.GetRasterBand(1).Checksum() == 1054:
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
        gdal.GetDriverByName('NITF').Delete( 'tmp/nitf46.ntf' )
        os.unlink( 'tmp/nitf46.ntf_0.ovr' )
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
    nitf_44,
    nitf_45,
    #nitf_46_jp2ecw,
    #nitf_46_jp2mrsid,
    #nitf_46_jp2kak,
    nitf_46_jasper,
    nitf_47,
    nitf_48,
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
    nitf_online_16_jp2ecw,
    nitf_online_16_jp2mrsid,
    nitf_online_16_jp2kak,
    nitf_online_16_jasper,
    nitf_online_17_jp2ecw,
    nitf_online_17_jp2mrsid,
    nitf_online_17_jp2kak,
    nitf_online_17_jasper,
    nitf_online_18,
    nitf_online_19,
    nitf_online_20,
    nitf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'nitf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

