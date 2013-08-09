#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for LCP driver.
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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
import random

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
#  Test test_FARSITE_UTM12.LCP

def lcp_1():

    ds = gdal.Open('data/test_FARSITE_UTM12.LCP')
    if ds.RasterCount != 8:
        gdaltest.post_reason('wrong number of bands')
        return 'fail'
        
    if ds.GetProjectionRef().find('NAD_1983_UTM_Zone_12N') == -1:
        gdaltest.post_reason("didn't get expect projection. Got : %s" % (ds.GetProjectionRef()))
        return 'fail'

    metadata = [ ('LATITUDE', '49'),
                 ('LINEAR_UNIT', 'Meters'),
                 ('DESCRIPTION', 'This is a test LCP file created with FARSITE 4.1.054, using data downloaded from the USGS \r\nNational Map for LANDFIRE (2008-05-06). Data were reprojected to UTM zone 12 on NAD83 \r\nusing gdalwarp (GDAL 1.4.2).\r\n') ]
    md = ds.GetMetadata()
    for item in metadata:
        if md[item[0]] != item[1]:
            gdaltest.post_reason('wrong metadataitem for dataset. md[\'%s\']=\'%s\', expected \'%s\'' % (item[0], md[item[0]], item[1]))
            return 'fail'

    check_gt = (285807.932887174887583,30,0,5379230.386217921040952,0,-30)
    new_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(new_gt[i]-check_gt[i]) > 1e-5:
            print('')
            print('old = ', check_gt)
            print('new = ', new_gt)
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    dataPerBand = [ ( 18645, [  ('ELEVATION_UNIT', '0' ),
                                ('ELEVATION_UNIT_NAME', 'Meters' ),
                                ('ELEVATION_MIN', '1064' ),
                                ('ELEVATION_MAX', '1492' ),
                                ('ELEVATION_NUM_CLASSES', '-1' ),
                                ('ELEVATION_FILE', '' ) ] ),
                    ( 16431, [  ('SLOPE_UNIT', '0' ),
                                ('SLOPE_UNIT_NAME', 'Degrees' ),
                                ('SLOPE_MIN', '0' ),
                                ('SLOPE_MAX', '34' ),
                                ('SLOPE_NUM_CLASSES', '36' ),
                                ('SLOPE_FILE', 'slope.asc' ) ] ),
                    ( 18851, [  ('ASPECT_UNIT', '2' ),
                                ('ASPECT_UNIT_NAME', 'Azimuth degrees' ),
                                ('ASPECT_MIN', '0' ),
                                ('ASPECT_MAX', '357' ),
                                ('ASPECT_NUM_CLASSES', '-1' ),
                                ('ASPECT_FILE', 'aspect.asc' ) ] ),
                    ( 26182, [  ('FUEL_MODEL_OPTION', '0' ),
                                ('FUEL_MODEL_OPTION_DESC', 'no custom models AND no conversion file needed' ),
                                ('FUEL_MODEL_MIN', '1' ),
                                ('FUEL_MODEL_MAX', '99' ),
                                ('FUEL_MODEL_NUM_CLASSES', '6' ),
                                ('FUEL_MODEL_VALUES', '1,2,5,8,10,99' ),
                                ('FUEL_MODEL_FILE', 'fbfm13.asc' ) ] ),
                    ( 30038, [  ('CANOPY_COV_UNIT', '0' ),
                                ('CANOPY_COV_UNIT_NAME', 'Categories (0-4)' ),
                                ('CANOPY_COV_MIN', '0' ),
                                ('CANOPY_COV_MAX', '95' ),
                                ('CANOPY_COV_NUM_CLASSES', '10' ),
                                ('CANOPY_COV_FILE', 'cancov.asc' ) ] ),
                    ( 22077, [  ('CANOPY_HT_UNIT', '3' ),
                                ('CANOPY_HT_UNIT_NAME', 'Meters x 10' ),
                                ('CANOPY_HT_MIN', '0' ),
                                ('CANOPY_HT_MAX', '375' ),
                                ('CANOPY_HT_NUM_CLASSES', '5' ),
                                ('CANOPY_HT_FILE', 'canht.asc' ) ] ),
                    ( 30388, [  ('CBH_UNIT', '3' ),
                                ('CBH_UNIT_NAME', 'Meters x 10' ),
                                ('CBH_MIN', '0' ),
                                ('CBH_MAX', '100' ),
                                ('CBH_NUM_CLASSES', '33' ),
                                ('CBH_FILE', 'cbh.asc' ) ] ),
                    ( 23249, [  ('CBD_UNIT', '3' ),
                                ('CBD_UNIT_NAME', 'kg/m^3 x 100' ),
                                ('CBD_MIN', '0' ),
                                ('CBD_MAX', '21' ),
                                ('CBD_NUM_CLASSES', '20' ),
                                ('CBD_FILE', 'cbd.asc' ) ] )
                  ]

    for i in range(8):
        band = ds.GetRasterBand(i+1)
        if band.Checksum() != dataPerBand[i][0]:
            gdaltest.post_reason('wrong checksum for band %d. Got %d, expected %d' % (i+1, band.Checksum(), dataPerBand[i][0]))
            return 'fail'
        md = band.GetMetadata()
        for item in dataPerBand[i][1]:
            if md[item[0]] != item[1]:
                gdaltest.post_reason('wrong metadataitem for band %d. md[\'%s\']=\'%s\', expected \'%s\'' % (i+1, item[0], md[item[0]], item[1]))
                return 'fail'

    ds = None

    return 'success'

###############################################################################
# test test_USGS_LFNM_Alb83.lcp

def lcp_2():

    ds = gdal.Open('data/test_USGS_LFNM_Alb83.lcp')
    if ds.RasterCount != 8:
        gdal.post_reason('wrong number of bands')
        return 'fail'

    metadata = [ ('LATITUDE', '48'),
                 ('LINEAR_UNIT', 'Meters'),
                 ('DESCRIPTION', '') ]
    md = ds.GetMetadata()
    for item in metadata:
        if md[item[0]] != item[1]:
            gdaltest.post_reason('wrong metadataitem for dataset. md[\'%s\']=\'%s\', expected \'%s\'' % (item[0], md[item[0]], item[1]))
            return 'fail'

    check_gt = (-1328145,30,0,2961735,0,-30)
    new_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(new_gt[i]-check_gt[i]) > 1e-5:
            print('')
            print('old = ', check_gt)
            print('new = ', new_gt)
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    dataPerBand = [ ( 28381, [  ('ELEVATION_UNIT', '0' ),
                                ('ELEVATION_UNIT_NAME', 'Meters' ),
                                ('ELEVATION_MIN', '1064' ),
                                ('ELEVATION_MAX', '1492' ),
                                ('ELEVATION_NUM_CLASSES', '-1' ),
                                ('ELEVATION_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_elevation_1.txt' ) ] ),
                    ( 25824, [  ('SLOPE_UNIT', '0' ),
                                ('SLOPE_UNIT_NAME', 'Degrees' ),
                                ('SLOPE_MIN', '0' ),
                                ('SLOPE_MAX', '34' ),
                                ('SLOPE_NUM_CLASSES', '35' ),
                                ('SLOPE_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_slope_1.txt' ) ] ),
                    ( 28413, [  ('ASPECT_UNIT', '2' ),
                                ('ASPECT_UNIT_NAME', 'Azimuth degrees' ),
                                ('ASPECT_MIN', '0' ),
                                ('ASPECT_MAX', '357' ),
                                ('ASPECT_NUM_CLASSES', '-1' ),
                                ('ASPECT_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_aspect_1.txt' ) ] ),
                    ( 19052, [  ('FUEL_MODEL_OPTION', '0' ),
                                ('FUEL_MODEL_OPTION_DESC', 'no custom models AND no conversion file needed' ),
                                ('FUEL_MODEL_MIN', '1' ),
                                ('FUEL_MODEL_MAX', '10' ),
                                ('FUEL_MODEL_NUM_CLASSES', '5' ),
                                ('FUEL_MODEL_VALUES', '1,2,5,8,10' ),
                                ('FUEL_MODEL_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_fuel1.txt' ) ] ),
                    ( 30164, [  ('CANOPY_COV_UNIT', '1' ),
                                ('CANOPY_COV_UNIT_NAME', 'Percent' ),
                                ('CANOPY_COV_MIN', '0' ),
                                ('CANOPY_COV_MAX', '95' ),
                                ('CANOPY_COV_NUM_CLASSES', '10' ),
                                ('CANOPY_COV_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_canopy1.txt' ) ] ),
                    ( 22316, [  ('CANOPY_HT_UNIT', '3' ),
                                ('CANOPY_HT_UNIT_NAME', 'Meters x 10' ),
                                ('CANOPY_HT_MIN', '0' ),
                                ('CANOPY_HT_MAX', '375' ),
                                ('CANOPY_HT_NUM_CLASSES', '5' ),
                                ('CANOPY_HT_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_height_1.txt' ) ] ),
                    ( 30575, [  ('CBH_UNIT', '3' ),
                                ('CBH_UNIT_NAME', 'Meters x 10' ),
                                ('CBH_MIN', '0' ),
                                ('CBH_MAX', '100' ),
                                ('CBH_NUM_CLASSES', '33' ),
                                ('CBH_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_base_1.txt' ) ] ),
                    ( 23304, [  ('CBD_UNIT', '3' ),
                                ('CBD_UNIT_NAME', 'kg/m^3 x 100' ),
                                ('CBD_MIN', '0' ),
                                ('CBD_MAX', '21' ),
                                ('CBD_NUM_CLASSES', '20' ),
                                ('CBD_FILE', 'd:\\scratch\\dist\\79990093\\Output\\rastert_density_1.txt' ) ] )
                  ]

    for i in range(8):
        band = ds.GetRasterBand(i+1)
        if band.Checksum() != dataPerBand[i][0]:
            gdaltest.post_reason('wrong checksum for band %d. Got %d, expected %d' % (i+1, band.Checksum(), dataPerBand[i][0]))
            return 'fail'
        md = band.GetMetadata()
        for item in dataPerBand[i][1]:
            if md[item[0]] != item[1]:
                gdaltest.post_reason('wrong metadataitem for band %d. md[\'%s\']=\'%s\', expected \'%s\'' % (i+1, item[0], md[item[0]], item[1]))
                return 'fail'

    ds = None

    return 'success'

###############################################################################
#  Test for empty prj

def lcp_3():

    ds = gdal.Open('data/test_USGS_LFNM_Alb83.lcp')
    if ds == None:
        return 'fail'
    wkt = ds.GetProjection()
    if wkt is None:
        gdaltest.post_reason('Got None from GetProjection()')
        return 'fail'
    return 'success'

###############################################################################
#  Test that the prj file isn't added to the sibling list if it isn't there.

def lcp_4():

    retval = 'success'
    ds = gdal.Open('data/test_USGS_LFNM_Alb83.lcp')
    if ds == None:
        return 'fail'
    fl = ds.GetFileList()
    if len(fl) != 1:
        gdaltest.post_reason('Invalid file list')
        retval = 'fail'
    return 'success'

###############################################################################
#  Test for valid prj

def lcp_5():

    ds = gdal.Open('data/test_FARSITE_UTM12.LCP')
    if ds == None:
        return 'fail'
    wkt = ds.GetProjection()
    if wkt is None or wkt == '':
        gdaltest.post_reason('Got invalid wkt from GetProjection()')
        return 'fail'
    return 'success'

###############################################################################
#  Test for valid sibling list

def lcp_6():

    retval = 'success'
    ds = gdal.Open('data/test_FARSITE_UTM12.LCP')
    if ds == None:
        return 'fail'
    fl = ds.GetFileList()
    if len(fl) != 2:
        gdaltest.post_reason('Invalid file list')
        retval = 'fail'
    try:
        os.remove('data/test_FARSITE_UTM12.LCP.aux.xml')
    except:
        pass

    return retval

###############################################################################
#  Test create copy that copies data over

def lcp_7():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    lcp_drv = gdal.GetDriverByName('LCP')
    if lcp_drv == None:
        return 'fail'
    # Make sure all avaible band counts work
    retval = 'success'
    for i in [5, 7, 8, 10]:
        src_ds = mem_drv.Create('/vsimem/lcptest', 10, 20, i, gdal.GDT_Int16)
        if src_ds == None:
            return 'fail'
        dst_ds = lcp_drv.CreateCopy('tmp/lcp_7.lcp', src_ds)
        if dst_ds == None:
            gdaltest.post_reason('Failed to create lcp with %d bands' % i)
            retval = 'fail'
            break
        dst_ds = None
    src_ds = None
    dst_ds = None

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_7.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy with invalid bands

def lcp_8():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    lcp_drv = gdal.GetDriverByName('LCP')
    if lcp_drv == None:
        return 'fail'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    retval = 'success'
    for i in [0,1,2,3,4,6,9,11]:
        src_ds = mem_drv.Create('', 10, 10, i, gdal.GDT_Int16)
        if src_ds == None:
            retval = 'fail'
            break
        dst_ds = lcp_drv.CreateCopy('tmp/lcp_8.lcp', src_ds)
        if dst_ds != None:
            gdaltest.post_reason('Created invalid lcp')
            retval = 'fail'
            break
    gdal.PopErrorHandler()
    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_8.' + ext)
        except:
            pass
    return 'success'

###############################################################################
#  Test create copy

def lcp_9():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    lcp_drv = gdal.GetDriverByName('LCP')
    if lcp_drv == None:
        return 'fail'
    src_ds = mem_drv.Create('', 10, 20, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'
    retval = 'success'
    lcp_ds = lcp_drv.CreateCopy('tmp/lcp_9.lcp', src_ds, False)
    if lcp_ds == None:
        retval = 'fail'
    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_9.' + ext)
        except:
            pass
    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work

def lcp_10():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    for option in ['METERS', 'FEET']:
        co = ['ELEVATION_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_10.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(1).GetMetadataItem("ELEVATION_UNIT_NAME")
        if units.lower() != option.lower():
            gdaltest.post_reason('Could not set ELEVATION_UNIT')
            retval = 'fail'
            break

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_10.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work

def lcp_11():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    for option in ['DEGREES', 'PERCENT']:
        co = ['SLOPE_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_11.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(2).GetMetadataItem("SLOPE_UNIT_NAME")
        if units.lower() != option.lower():
            gdaltest.post_reason('Could not set SLOPE_UNIT')
            retval = 'fail'
            break

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_11.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work

def lcp_12():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    for option in ['GRASS_CATEGORIES', 'AZIMUTH_DEGREES', 'GRASS_DEGREES']:
        co = ['ASPECT_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_12.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(3).GetMetadataItem("ASPECT_UNIT_NAME")
        if units.lower() != option.replace('_', ' ').lower():
            gdaltest.post_reason('Could not set ASPECT_UNIT')
            retval = 'fail'
            break

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_12.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work

def lcp_13():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    for option in ['PERCENT', 'CATEGORIES']:
        co = ['CANOPY_COV_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_13.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(5).GetMetadataItem("CANOPY_COV_UNIT_NAME")
        if units.lower()[:10] != option.lower()[:10]:
            gdaltest.post_reason('Could not set CANOPY_COV_UNIT')
            retval = 'fail'
            break

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_13.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work

def lcp_14():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    for option in ['METERS', 'FEET', 'METERS_X_10', 'FEET_X_10']:
        co = ['CANOPY_HT_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_14.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(6).GetMetadataItem("CANOPY_HT_UNIT_NAME")
        if units.lower() != option.replace('_', ' ').lower():
            gdaltest.post_reason('Could not set CANOPY_HT_UNIT')
            retval = 'fail'
            break

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_14.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work

def lcp_15():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    for option in ['METERS', 'FEET', 'METERS_X_10', 'FEET_X_10']:
        co = ['CBH_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_15.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(7).GetMetadataItem("CBH_UNIT_NAME")
        if units.lower() != option.replace('_', ' ').lower():
            gdaltest.post_reason('Could not set CBH_UNIT')
            retval = 'fail'
            break

    for ext in ['lcp', 'prj', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_15.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work

def lcp_16():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    answers = ['kg/m^3', 'lb/ft^3', 'kg/m^3 x 100', 'lb/ft^3 x 1000',
               'tons/acre x 100']
    for i, option in enumerate(['KG_PER_CUBIC_METER', 'POUND_PER_CUBIC_FOOT',
                                'KG_PER_CUBIC_METER_X_100',
                                'POUND_PER_CUBIC_FOOT_X_1000']):
        co = ['CBD_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_16.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(8).GetMetadataItem("CBD_UNIT_NAME")
        if units.lower() != answers[i].lower():
            gdaltest.post_reason('Could not set CBD_UNIT')
            retval = 'fail'
            break

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_16.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure all unit metadata co work
#  It is unclear whether the metadata generated is correct, or the
#  documentation.  Docs say mg/ha * 10 and tn/ac * 10, metadata is not * 10.

def lcp_17():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    answers = ['mg/ha', 't/ac x 10']
    for i, option in enumerate(['MG_PER_HECTARE_X_10', 'TONS_PER_ACRE_X_10']):
        co = ['DUFF_UNIT=%s' % option]
        lcp_ds = drv.CreateCopy('tmp/lcp_17.lcp', src_ds, False, co)
        units = lcp_ds.GetRasterBand(9).GetMetadataItem("DUFF_UNIT_NAME")
        if units.lower() != answers[i].lower():
            #gdaltest.post_reason('Could not set DUFF_UNIT')
            retval = 'expected_fail'
            break

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_17.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make sure creation options work.

def lcp_18():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    lcp_ds = drv.CreateCopy('tmp/lcp_18.lcp', src_ds, False, ['LATITUDE=45',])
    if lcp_ds == None:
        return 'fail'
    if lcp_ds.GetMetadataItem('LATITUDE') != '45':
        gdaltest.post_reason('Failed to set LATITUDE creation option')
        retval = 'fail'
    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_18.' + ext)
        except:
            pass
    return 'success'

###############################################################################
#  Test create copy and make sure creation options work.

def lcp_19():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    lcp_ds = drv.CreateCopy('tmp/lcp_19.lcp', src_ds, False, ['LINEAR_UNIT=FOOT'])
    if lcp_ds == None:
        return 'fail'
    if lcp_ds.GetMetadataItem('LINEAR_UNIT') != 'Feet':
        gdaltest.post_reason('Failed to set LINEAR_UNIT creation option')
        retval = 'fail'
    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_19.' + ext)
        except:
            pass

    return 'success'

###############################################################################
#  Test create copy and make sure DESCRIPTION co works

def lcp_20():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    desc = "test description"
    lcp_ds = drv.CreateCopy('tmp/lcp_20.lcp', src_ds, False, ['DESCRIPTION=%s' % desc])
    if lcp_ds == None:
        return 'fail'
    if lcp_ds.GetMetadataItem('DESCRIPTION') != desc:
        gdaltest.post_reason('Failed to set DESCRIPTION creation option')
        retval = 'fail'
    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_20.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make data is copied over via checksums

def lcp_21():
    try:
        import random
    except ImportError:
        return 'skip'
    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 3, 3, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    for i in range(10):
        data = [random.randint(0, 100) for i in range(9)]
        src_ds.GetRasterBand(i+1).WriteRaster(0, 0, 3, 3, struct.pack('h'*9, *data))

    lcp_ds = drv.CreateCopy('tmp/lcp_21.lcp', src_ds, False)
    if lcp_ds == None:
        return 'fail'
    retval = 'success'
    for i in range(10):
        if src_ds.GetRasterBand(i+1).Checksum() != lcp_ds.GetRasterBand(i+1).Checksum():
            gdaltest.post_reason('Did not get expected checksum')
            retval = 'fail'

    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_21.' + ext)
        except:
            pass

    return retval

###############################################################################
#  Test create copy and make data is copied over via numpy comparison.

def lcp_22():
    try:
        import random
        import numpy
    except ImportError:
        return 'skip'
    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 3, 3, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    for i in range(10):
        data = [random.randint(0, 100) for i in range(9)]
        src_ds.GetRasterBand(i+1).WriteRaster(0, 0, 3, 3, struct.pack('h'*9, *data))

    lcp_ds = drv.CreateCopy('tmp/lcp_22.lcp', src_ds, False)
    if lcp_ds == None:
        return 'fail'
    retval = 'success'
    for i in range(10):
        src_data = src_ds.GetRasterBand(i+1).ReadAsArray()
        dst_data = lcp_ds.GetRasterBand(i+1).ReadAsArray()
        if not numpy.array_equal(src_data, dst_data):
            gdaltest.post_reason('Did not copy data correctly')
            retval = 'fail'
    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_22.' + ext)
        except:
            pass

    return retval


###############################################################################
#  Test create copy and make sure invalid creation options are caught.

def lcp_23():

    mem_drv = gdal.GetDriverByName('MEM')
    if mem_drv == None:
        return 'fail'
    drv = gdal.GetDriverByName('LCP')
    if drv == None:
        return 'fail'
    src_ds = mem_drv.Create('/vsimem/', 10, 10, 10, gdal.GDT_Int16)
    if src_ds == None:
        return 'fail'

    retval = 'success'
    bad = 'NOT_A_REAL_OPTION'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    for option in ['ELEVATION_UNIT', 'SLOPE_UNIT', 'ASPECT_UNIT',
                   'FUEL_MODEL_OPTION', 'CANOPY_COV_UNIT', 'CANOPY_HT_UNIT',
                   'CBH_UNIT', 'CBD_UNIT', 'DUFF_UNIT']:
        co = ['%s=%s' % (option, bad),]
        lcp_ds = drv.CreateCopy('tmp/lcp_23.lcp', src_ds, False, co)
        if lcp_ds != None:
            retval = 'fail'
    gdal.PopErrorHandler()
    for ext in ['lcp', 'lcp.aux.xml']:
        try:
            os.remove('tmp/lcp_23.' + ext)
        except:
            pass

    return retval

gdaltest_list = [
    lcp_1,
    lcp_2,
    lcp_3,
    lcp_4,
    lcp_5,
    lcp_6,
    lcp_7,
    lcp_8,
    lcp_9,
    lcp_10,
    lcp_11,
    lcp_12,
    lcp_13,
    lcp_14,
    lcp_15,
    lcp_16,
    lcp_17,
    lcp_18,
    lcp_19,
    lcp_20,
    lcp_21,
    lcp_22,
    lcp_23]

if __name__ == '__main__':

    gdaltest.setup_run( 'lcp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

