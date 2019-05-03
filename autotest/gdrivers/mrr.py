#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MRR format driver.
# Author:   Upendra Patel <upendra.patel1@pb.com>
#
###############################################################################
# Copyright (c) 2007, Upendra Patel <upendra.patel1@pb.com>
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
from osgeo import osr


import gdaltest
import pytest

###############################################################################
# Perform simple read test.
def test_mrr_1():
    tst = gdaltest.GDALTest('MRR', 'MRR/10X10X200_RealDouble.mrr', 1, 37769)
    tst.testOpen()
    dataset = gdal.Open('data/MRR/10X10X200_RealDouble.mrr')
    band = dataset.GetRasterBand(1)    
    # Get statistics
    stats = band.GetStatistics( True, True )
    print("[ STATS ] =  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f" % (stats[0], stats[1], stats[2], stats[3] )) 
    assert None != stats , 'Getstatistics returns None.'
    assert 1.00 == round(stats[0],2), 'Statistics:[Minumum] mismatched.'
    assert 100.00 == round(stats[1],2), 'Statistics:[Maximum] mismatched.'
    assert 50.50 == round(stats[2],2), 'Statistics:[Mean] mismatched.'
    assert 29.01 == round(stats[3],2), 'Statistics:[StdDev] mismatched.'

###############################################################################
# Confirm we can recognised RealSingle data.
def test_mrr_2_DataType():
    ds = gdal.Open('data/MRR/10X10X200_RealSingle.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Float32, 'Failed to detect RealSingle data type'
    ds = None

###############################################################################
# Confirm we can recognised RealDouble data.
def test_mrr_3_DataType():
    ds = gdal.Open('data/MRR/10X10X200_RealDouble.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Float64, 'Failed to detect RealDouble data type'
    ds = None

###############################################################################
# Confirm we can recognised SignedInt32 data.
def test_mrr_4_DataType():
    ds = gdal.Open('data/MRR/10X10X200_SignedInt32.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Int32, 'Failed to detect SignedInt32 data type'
    ds = None

###############################################################################
# Confirm we can recognised UnsignedInt32 data.
def test_mrr_5_DataType():
    ds = gdal.Open('data/MRR/10X10X200_UnsignedInt32.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_UInt32, 'Failed to detect UnsignedInt32 data type'
    ds = None

###############################################################################
# Confirm we can recognised SignedInt16 data.
def test_mrr_6_DataType():
    ds = gdal.Open('data/MRR/10X10X200_SignedInt16.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Int16, 'Failed to detect SignedInt16 data type'
    ds = None

###############################################################################
# Confirm we can recognised UnsignedInt16 data.
def test_mrr_7_DataType():
    ds = gdal.Open('data/MRR/10X10X200_UnsignedInt16.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_UInt16, 'Failed to detect UnsignedInt16 data type'
    ds = None

###############################################################################
# Confirm we can recognised Bit4 data.
def test_mrr_8_DataType():
    ds = gdal.Open('data/MRR/10X10X200_Bit4.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Byte, 'Failed to detect Bit4 data type'
    ds = None

###############################################################################
# Confirm we can recognised signedInt8 and UnsignedInt8 data is mapping to SignedInt16 and UnsignedInt16.
def test_mrr_9_DataType():
    ds = gdal.Open('data/MRR/10X10X200_SignedInt8.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Int16, 'Failed to map to  SignedInt16 data type'
    ds = None
    ds = gdal.Open('data/MRR/10X10X200_UnsignedInt8.mrr')
    dataType = ds.GetRasterBand(1).DataType
    assert dataType == gdal.GDT_Byte, 'Failed to map to  UnsignedInt16 data type'
    ds = None

###############################################################################
# Verify MRR driver info
def test_mrr_10_DriverInfo():    
	dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
	assert 'MRR' == dataset.GetDriver().ShortName, 'Driver short name mismatched.'
	assert 'MapInfo Multi Resolution Raster' == dataset.GetDriver().LongName, 'Driver full name mismatched.'
	dataset = None

###############################################################################
# Verify MRR width and Height
def test_mrr_11_RasterSize():
	dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
	assert 1024 == dataset.RasterXSize, 'Raster width mismatched.'
	assert 1024 == dataset.RasterYSize, 'Raster height mismatched.'
	dataset = None

###############################################################################
# Verify Imagery MRR's band count
def test_mrr_12_ImageBandCount():
	dataset = gdal.Open('data/MRR/RGBA_Imagery.mrr', gdal.GA_ReadOnly)
	assert 4 == dataset.RasterCount, 'Raster band count mismatched.'	
	dataset = None

###############################################################################
# Verify Classified MRR's band count
def test_mrr_13_ClassifedBandCount():
	dataset = gdal.Open('data/MRR/Having65540Classes_English.mrr', gdal.GA_ReadOnly)
	assert 1 == dataset.RasterCount, 'Raster band count mismatched.'	
	dataset = None

###############################################################################
# Verify MRR projection info
def test_mrr_14_Projection():
	dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
	str1 = 'PROJCS["unnamed",GEOGCS["unnamed",DATUM["North_American_Datum_1983",SPHEROID["GRS 80",6378137,298.257222101],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0],'
	str2 = 'UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-123],PARAMETER["scale_factor",0.9996],'
	str3 = 'PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
	expProj = str1 + str2 + str3
	assert expProj == dataset.GetProjection(), 'Raster projection mismatched.'	
	dataset = None

###############################################################################
# Verify MRR Origin
def test_mrr_15_Origin():
	dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
	geotransform = dataset.GetGeoTransform()
	assert 444500.0 == geotransform[0], 'Raster OriginX mismatched.'
	assert 5350500.0 == geotransform[3], 'Raster OriginY mismatched.'
	dataset = None

###############################################################################
# Verify MRR cellsize
def test_mrr_16_CellSize():
	dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
	geotransform = dataset.GetGeoTransform()
	assert 200 == geotransform[1], 'Raster pixelSizeX mismatched.'
	assert -200 == geotransform[5], 'Raster pixelSizeY mismatched.'
	dataset = None

###############################################################################
# Verify MRR Statistics	
def test_mrr_17_Statistics():
	dataset = gdal.Open('data/MRR/SeattleElevation.mrr', gdal.GA_ReadOnly)
	band = dataset.GetRasterBand(1)
	stats = band.GetStatistics( True, True )
	print("[ STATS ] =  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f" % (stats[0], stats[1], stats[2], stats[3] )) 
	assert None != band.GetStatistics( True, True) , 'Getstatistics returns None'
	assert 0.005 == round(stats[0],3), 'Statistics:[Minumum] mismatched.'
	assert 4370.262 == round(stats[1],3), 'Statistics:[Maximum] mismatched.'
	assert 483.009 == round(stats[2],3), 'Statistics:[Mean] mismatched.'
	assert 517.523 == round(stats[3],3), 'Statistics:[StdDev] mismatched.'	  
	dataset = None

###############################################################################
# Cleanup

def test_mrr_cleanup():
    gdaltest.clean_tmp()




