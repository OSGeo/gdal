#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MRR format driver.
# Author:   Pitney Bowes Software
#
###############################################################################
# Copyright (c) 2007, Pitney Bowes Software
# Copyright (c) 2011-2013, 
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
from array import array

import gdaltest
import pytest



###############################################################################
# Perform simple read test.

def test_mrr_1():
   
    gdaltest.mrr_drv = gdal.GetDriverByName('MRR')
    if gdaltest.mrr_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('MRR', 'MRR/10X10X200_RealDouble.mrr', 1, 37769)
    tst.testOpen()


###############################################################################
# Confirm we can recognised the following band data types.
# RealSingle = Float
# RealDouble = double
# SignedInt32 = Int32
# UnsignedInt32 = UInt32
# SignedInt16 = Int16
# UnsignedInt16 = UInt16

def test_mrr_2_DataType():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    ds = gdal.Open('data/MRR/10X10X200_RealSingle.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Float32, 'Failed to detect RealSingle data type.'
    ds = gdal.Open('data/MRR/10X10X200_RealDouble.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Float64, 'Failed to detect RealDouble data type.'
    ds = gdal.Open('data/MRR/10X10X200_SignedInt32.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Int32, 'Failed to detect SignedInt32 data type.'
    ds = gdal.Open('data/MRR/10X10X200_UnsignedInt32.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_UInt32, 'Failed to detect UnsignedInt32 data type.'
    ds = gdal.Open('data/MRR/10X10X200_SignedInt16.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Int16, 'Failed to detect SignedInt16 data type.'
    ds = gdal.Open('data/MRR/10X10X200_UnsignedInt16.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_UInt16, 'Failed to detect UnsignedInt16 data type.'
    ds = None


###############################################################################
# Confirm we can recognised following band data types with nearest compatible data types.
# Bit4 = Byte
# signedInt8 = Int16
# UnsignedInt8 = Byte

def test_mrr_3_CompatibleDataType():
    if gdaltest.mrr_drv is None:
        pytest.skip()
    ds = gdal.Open('data/MRR/10X10X200_Bit4.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Byte, 'Failed to detect Bit4 data type'
    ds = gdal.Open('data/MRR/10X10X200_SignedInt8.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Int16, 'Failed to map to  SignedInt16 data type'
    ds = gdal.Open('data/MRR/10X10X200_UnsignedInt8.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Byte, 'Failed to map to  UnsignedInt16 data type'
    ds = None


###############################################################################
# Verify MRR driver info

def test_mrr_4_DriverInfo():

    if gdaltest.mrr_drv is None:
        pytest.skip()
        
    tst = gdaltest.GDALTest('MRR', 'MRR/SeattleElevation.mrr', 1, None)
    tst.testDriver()

    dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
    assert 'MRR' == dataset.GetDriver().ShortName, 'Driver short name mismatched.'
    assert 'MapInfo Multi Resolution Raster' == dataset.GetDriver().LongName, 'Driver full name mismatched.'
    dataset = None


###############################################################################
# Verify MRR width and Height

def test_mrr_5_RasterSize():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
    assert 1024 == dataset.RasterXSize, 'Raster width mismatched.'
    assert 1024 == dataset.RasterYSize, 'Raster height mismatched.'
    dataset = None


###############################################################################
# Verify Imagery MRR's band count and Block size

def test_mrr_6_ImageBandCount():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    dataset = gdal.Open('data/MRR/RGB_Imagery.mrr', gdal.GA_ReadOnly)
    assert 3 == dataset.RasterCount, 'Raster band count mismatched.'
    
    # Ensure that block size is not 0. 
    blockSize = dataset.GetRasterBand(1).GetBlockSize()
    assert 0< blockSize[0]
    assert 0< blockSize[1]

    dataset = None


###############################################################################
# Verify Classified MRR's band count

def test_mrr_7_ClassifedBandCount():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    dataset = gdal.Open('data/MRR/Having65540Classes_English.mrr', gdal.GA_ReadOnly)
    assert 1 == dataset.RasterCount, 'Raster band count mismatched.'	
    dataset = None


###############################################################################
# Verify MRR Origin

def test_mrr_8_Origin():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
    geotransform = dataset.GetGeoTransform()
    assert 444500.0 == geotransform[0], 'Raster OriginX mismatched.'
    assert 5350500.0 == geotransform[3], 'Raster OriginY mismatched.'
    dataset = None

###############################################################################
# Verify MRR cellsize

def test_mrr_9_CellSize():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
    geotransform = dataset.GetGeoTransform()
    assert 200 == geotransform[1], 'Raster pixelSizeX mismatched.'
    assert -200 == geotransform[5], 'Raster pixelSizeY mismatched.'

     # Ensure that block size is not 0. 
    blockSize = dataset.GetRasterBand(1).GetBlockSize()
    assert 0< blockSize[0]
    assert 0< blockSize[1]
    
    dataset = None


###############################################################################
# Verify MRR projection info

def test_mrr_10_Projection():

    if gdaltest.mrr_drv is None:
       pytest.skip()

    tst = gdaltest.GDALTest('MRR', 'MRR/SeattleElevation.mrr', 1, None)

    gt = (444500.0, 200.0, 0.0, 5350500.0, 0.0, -200.0)
    
    prj = """PROJCS["unnamed",
    GEOGCS["unnamed",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 80",6378137,298.257222101],
                TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],            
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","9122"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-123],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]]]"""

    tst.testOpen(check_gt=gt,
                       check_stat=(0.005, 4370.262, 483.008, 517.523),
                       check_approx_stat=(0.00549332052469254, 4370.26220703125, 483.008622016405, 517.523079384345))

    ds = gdal.Open('data/MRR/SeattleElevation.mrr',gdal.GA_ReadOnly)
    got_prj = ds.GetProjectionRef()
    ds = None

    if prj.find('North_American_Datum_1983') == -1 or \
       prj.find('Transverse_Mercator') == -1:
        print(got_prj)
        pytest.fail('did not get expected projection')

    if got_prj != prj:
        print('Warning: did not get exactly expected projection. Got %s' % got_prj)


###############################################################################
# Verify MRR Statistics	

def test_mrr_11_Statistics():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('MRR', 'MRR/SeattleElevation.mrr', 1, None)

    gt = (444500.0, 200.0, 0.0, 5350500.0, 0.0, -200.0)
    
    tst.testOpen(check_gt=gt,
                 check_stat=(0.005, 4370.262, 483.008, 517.523),
                 check_approx_stat=(0.00549332052469254, 4370.26220703125, 483.008622016405, 517.523079384345))


###############################################################################
# Read subwindow.  Tests the tail recursion problem.

def test_mrr_12_BlockRead():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('MRR', 'MRR/SeattleElevation.mrr', 1, 214,
                            5, 5, 5, 5)
    return tst.testOpen()


###############################################################################
# Verify the followings for imagery MRR
# Band Count
# Each band's checksum
# Each band's statistics
# Each band' overview count

def test_mrr_13_ChecksumImagery():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    arrChecksum = array('i', [4955, 9496, 60874, 8232])
    arrStats = [[24.00, 234.00, 129.58, 58.00],
                [1.00, 253.00, 119.44, 78.74],
                [0.00, 254.00, 130.78, 73.45],
                [3.00, 253.00, 150.14, 65.81]]

    overviewCount = 9

    dataset = gdal.Open('data/MRR/RGBA_Imagery.mrr',gdal.GA_ReadOnly)
    bands = dataset.RasterCount
    assert len(arrChecksum) == bands, "Band count mismatched."

    for i in range(bands):
        band = dataset.GetRasterBand(i+1)

        checksum = band.Checksum(xoff=0, yoff=0, xsize=band.XSize, ysize=band.YSize)
        print('Band['+str(i+1)+'] CHECKSUM :',checksum)
        assert arrChecksum[i] == checksum, 'Band['+str(i+1)+'] checksum mismatched.'

        stats = band.GetStatistics( True, True )
        assert None != band.GetStatistics( True, True) , 'Getstatistics returns None'
        print("[ STATS ]: =  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f" % (stats[0], stats[1], stats[2], stats[3] ))        
        assert arrStats[i][0] == round(stats[0],2), 'Statistics:[Minumum] mismatched.'
        assert arrStats[i][1] == round(stats[1],2), 'Statistics:[Maximum] mismatched.'
        assert arrStats[i][2] == round(stats[2],2), 'Statistics:[Mean] mismatched.'
        assert arrStats[i][3] == round(stats[3],2), 'Statistics:[StdDev] mismatched.'
        
        overviews = band.GetOverviewCount()
        print("Band has {} overviews".format(overviews))
        assert overviewCount == overviews, "Overview count mismatched."


###############################################################################
# Verify the followings for Continuous MRR
# Band Count
# Each band's checksum
# Each band's statistics
# Each band' overview count


def test_mrr_14_ChecksumContinious():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    arrChecksum = array('i', [28606, 40719, 38697, 32088, 24241, 19614, 30134, 35043, 36269, 30763, 27037, 27765])
    arrStats = [[9000.00, 26922.00, 12720.07, 2357.53],
                [8254.00, 28338.00, 12254.44, 2579.38],
                [7007.00, 28387.00, 11114.22, 2671.23],
                [6475.00, 30879.00, 11043.36, 3018.66],
                [6070.00, 34429.00, 11602.18, 3573.27],
                [5592.00, 28048.00, 10389.53, 3113.41],
                [5507.00, 25004.00, 9747.69, 2752.26],
                [6813.00, 29738.00, 11081.63, 2799.42],
                [5194.00, 14671.00, 5896.82, 462.21],
                [7597.00, 20317.00, 16433.41, 1454.45],
                [7918.00, 18255.00, 15091.32, 1171.29],
                [20480.00, 61440.00, 54963.20, 12495.90]]
    overviewCount = 10


    dataset = gdal.Open('data/MRR/LanSat8Bands.mrr',gdal.GA_ReadOnly)
    bands = dataset.RasterCount
    assert len(arrChecksum) == bands, "Band count mismatched."

    for i in range(bands):
        band = dataset.GetRasterBand(i+1)

        checksum = band.Checksum(xoff=0, yoff=0, xsize=band.XSize, ysize=band.YSize)
        print('Band['+str(i+1)+'] CHECKSUM :',checksum)
        assert arrChecksum[i] == checksum, 'Band['+str(i+1)+'] checksum mismatched.'

        stats = band.GetStatistics( True, True )
        assert None != band.GetStatistics( True, True) , 'Getstatistics returns None'
        print("[ STATS ]: =  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f" % (stats[0], stats[1], stats[2], stats[3] ))        
        assert arrStats[i][0] == round(stats[0],2), 'Statistics:[Minumum] mismatched.'
        assert arrStats[i][1] == round(stats[1],2), 'Statistics:[Maximum] mismatched.'
        assert arrStats[i][2] == round(stats[2],2), 'Statistics:[Mean] mismatched.'
        assert arrStats[i][3] == round(stats[3],2), 'Statistics:[StdDev] mismatched.'

        overviews = band.GetOverviewCount()
        print("Band has {} overviews".format(overviews))
        assert overviewCount == overviews, "Overview count mismatched."


###############################################################################
# Verify the followings for Classified MRR
# Band Count
# Band checksum
# Band statistics
# Band overview count
# ClassTable Record count

def test_mrr_15_ChecksumClassified():

    if gdaltest.mrr_drv is None:
        pytest.skip()

    expchecksum = 31466
    arrStats = [0.00, 65539.00, 29071.89, 20005.27]
    colorTableRecordCount = 65540
    overviewCount = 9

    dataset = gdal.Open('data/MRR/Having65540Classes_English.mrr',gdal.GA_ReadOnly)
    bands = dataset.RasterCount
    assert 1 == bands, "Band count mismatched."

    band = dataset.GetRasterBand(1)
    checksum = band.Checksum(xoff=0, yoff=0, xsize=band.XSize, ysize=band.YSize)
    print('Band[1] CHECKSUM :',checksum)
    assert expchecksum == checksum, 'Band[1] checksum mismatched.'

    stats = band.GetStatistics( True, True )
    assert None != stats , 'Getstatistics returns None'
    print("[ STATS ]: =  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f" % (stats[0], stats[1], stats[2], stats[3] ))        
    assert arrStats[0] == round(stats[0],2), 'Statistics:[Minumum] mismatched.'
    assert arrStats[1] == round(stats[1],2), 'Statistics:[Maximum] mismatched.'
    assert arrStats[2] == round(stats[2],2), 'Statistics:[Mean] mismatched.'
    assert arrStats[3] == round(stats[3],2), 'Statistics:[StdDev] mismatched.'

    overviews = band.GetOverviewCount()
    print("Band has {} overviews".format(overviews))
    assert overviewCount == overviews, "Overview count mismatched."

    colorTable = band.GetRasterColorTable()
    assert None != colorTable , 'Band do not have color table. GetRasterColorTable returns None'
    print("Band has a color table with {} entries".format(colorTable.GetCount()))
    assert colorTableRecordCount == colorTable.GetCount(), "Band color table record count mismatched."


###############################################################################
# Verify the followings for ImagePalette MRR
# Band Count
# Band checksum
# Band statistics
# Band overview count
# ClassTable Record count

def test_mrr_16_ChecksumImagePalette():

    if gdaltest.mrr_drv is None:
        pytest.skip()
    
    expchecksum = 997
    arrStats = [1.00, 10.00, 5.40, 3.11]
    overviewCount = 8
    colorTableRecordCount = 11

    dataset = gdal.Open('data/MRR/RGBA_PaletteRaster.mrr',gdal.GA_ReadOnly)
    bands = dataset.RasterCount
    assert 1 == bands, "Band count mismatched."

    band = dataset.GetRasterBand(1)
    checksum = band.Checksum(xoff=0, yoff=0, xsize=band.XSize, ysize=band.YSize)
    print('Band[1] CHECKSUM :',checksum)
    assert expchecksum == checksum, 'Band[1] checksum mismatched.'

    stats = band.GetStatistics( True, True )
    assert None != stats , 'Getstatistics returns None'
    print("[ STATS ]: =  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f" % (stats[0], stats[1], stats[2], stats[3] ))        
    assert arrStats[0] == round(stats[0],2), 'Statistics:[Minumum] mismatched.'
    assert arrStats[1] == round(stats[1],2), 'Statistics:[Maximum] mismatched.'
    assert arrStats[2] == round(stats[2],2), 'Statistics:[Mean] mismatched.'
    assert arrStats[3] == round(stats[3],2), 'Statistics:[StdDev] mismatched.'

    overviews = band.GetOverviewCount()
    print("Band has {} overviews".format(overviews))
    assert overviewCount == overviews, "Overview count mismatched."

    colorTable = band.GetRasterColorTable()
    assert None != colorTable , 'Band do not have color table. GetRasterColorTable returns None'
    print("Band has a color table with {} entries".format(colorTable.GetCount()))
    assert colorTableRecordCount == colorTable.GetCount(), "Band color table record count mismatched."