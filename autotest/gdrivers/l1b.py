#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for L1B driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>
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
import array
import string

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# 
class TestL1B:
    def __init__( self, downloadURL, fileName, checksum, download_size, gcpNumber ):
        self.downloadURL = downloadURL
        self.fileName = fileName
        self.checksum = checksum
        self.download_size = download_size
        self.gcpNumber = gcpNumber

    def test( self ):
        if not gdaltest.download_file(self.downloadURL + '/' + self.fileName, self.fileName, self.download_size):
            return 'skip'

        ds = gdal.Open('tmp/cache/' + self.fileName)

        if ds.GetRasterBand(1).Checksum() != self.checksum:
            gdaltest.post_reason('Bad checksum. Expected %d, got %d' % (self.checksum, ds.GetRasterBand(1).Checksum()))
            return 'failure'

        if len(ds.GetGCPs()) != self.gcpNumber:
            gdaltest.post_reason('Bad GCP number. Expected %d, got %d' % (self.gcpNumber, len(ds.GetGCPs())))
            return 'failure'

        return 'success'

###############################################################################
# 
def l1b_geoloc():
    try:
        os.stat('tmp/cache/n12gac8bit.l1b')
    except:
        return 'skip'

    ds = gdal.Open('tmp/cache/n12gac8bit.l1b')
    md = ds.GetMetadata('GEOLOCATION')
    expected_md = {
      'LINE_OFFSET' : '0',
      'LINE_STEP' : '1',
      'PIXEL_OFFSET' : '0',
      'PIXEL_STEP' : '1',
      'X_BAND' : '1',
      'X_DATASET' : 'L1BGCPS_INTERPOL:"tmp/cache/n12gac8bit.l1b"',
      'Y_BAND' : '2',
      'Y_DATASET' : 'L1BGCPS_INTERPOL:"tmp/cache/n12gac8bit.l1b"' }
    for key in expected_md:
        if md[key] != expected_md[key]:
            print(md)
            return 'fail'
    ds = None

    ds = gdal.Open('L1BGCPS_INTERPOL:"tmp/cache/n12gac8bit.l1b"')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 62397:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    cs = ds.GetRasterBand(2).Checksum()
    if cs != 52616:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# 
def l1b_solar_zenith_angles_before_noaa_15():
    try:
        os.stat('tmp/cache/n12gac10bit.l1b')
    except:
        return 'skip'

    ds = gdal.Open('tmp/cache/n12gac10bit.l1b')
    md = ds.GetMetadata('SUBDATASETS')
    expected_md = {
      'SUBDATASET_1_NAME' : 'L1B_SOLAR_ZENITH_ANGLES:"tmp/cache/n12gac10bit.l1b"',
      'SUBDATASET_1_DESC' : 'Solar zenith angles'
    }
    for key in expected_md:
        if md[key] != expected_md[key]:
            print(md)
            return 'fail'
    ds = None

    ds = gdal.Open('L1B_SOLAR_ZENITH_ANGLES:"tmp/cache/n12gac10bit.l1b"')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 22924:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# 
def l1b_metadata_before_noaa_15():
    try:
        os.stat('tmp/cache/n12gac10bit.l1b')
    except:
        return 'skip'

    gdal.SetConfigOption('L1B_FETCH_METADATA', 'YES')
    gdal.SetConfigOption('L1B_METADATA_DIRECTORY', 'tmp')
    ds = gdal.Open('tmp/cache/n12gac10bit.l1b')
    gdal.SetConfigOption('L1B_FETCH_METADATA', None)
    gdal.SetConfigOption('L1B_METADATA_DIRECTORY', None)

    f = open('tmp/n12gac10bit.l1b_metadata.csv', 'rb')
    l = f.readline()
    if l != 'SCANLINE,NBLOCKYOFF,YEAR,DAY,MS_IN_DAY,FATAL_FLAG,TIME_ERROR,DATA_GAP,DATA_JITTER,INSUFFICIENT_DATA_FOR_CAL,NO_EARTH_LOCATION,DESCEND,P_N_STATUS,BIT_SYNC_STATUS,SYNC_ERROR,FRAME_SYNC_ERROR,FLYWHEELING,BIT_SLIPPAGE,C3_SBBC,C4_SBBC,C5_SBBC,TIP_PARITY_FRAME_1,TIP_PARITY_FRAME_2,TIP_PARITY_FRAME_3,TIP_PARITY_FRAME_4,TIP_PARITY_FRAME_5,SYNC_ERRORS,CAL_SLOPE_C1,CAL_INTERCEPT_C1,CAL_SLOPE_C2,CAL_INTERCEPT_C2,CAL_SLOPE_C3,CAL_INTERCEPT_C3,CAL_SLOPE_C4,CAL_INTERCEPT_C4,CAL_SLOPE_C5,CAL_INTERCEPT_C5,NUM_SOLZENANGLES_EARTHLOCPNTS\n':
        print(l)
        return 'fail'
    l = f.readline()
    if l != '3387,0,1998,84,16966146,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0.102000,-4.130000,0.103000,-4.210000,-0.001677,1.667438,-0.157728,156.939636,-0.179833,179.775742,51\n':
        print(l)
        return 'fail'
    f.close()

    os.unlink('tmp/n12gac10bit.l1b_metadata.csv')

    return 'success'

###############################################################################
# 
def l1b_angles_after_noaa_15():
    try:
        os.stat('tmp/cache/n16gac10bit.l1b')
    except:
        return 'skip'

    ds = gdal.Open('tmp/cache/n16gac10bit.l1b')
    md = ds.GetMetadata('SUBDATASETS')
    expected_md = {
      'SUBDATASET_1_NAME' : 'L1B_ANGLES:"tmp/cache/n16gac10bit.l1b"',
      'SUBDATASET_1_DESC' : 'Solar zenith angles, satellite zenith angles and relative azimuth angles'
    }
    for key in expected_md:
        if md[key] != expected_md[key]:
            print(md)
            return 'fail'
    ds = None

    ds = gdal.Open('L1B_ANGLES:"tmp/cache/n16gac10bit.l1b"')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 31487:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    cs = ds.GetRasterBand(2).Checksum()
    if cs != 23380:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    cs = ds.GetRasterBand(3).Checksum()
    if cs != 64989:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# 
def l1b_clouds_after_noaa_15():
    try:
        os.stat('tmp/cache/n16gac10bit.l1b')
    except:
        return 'skip'

    ds = gdal.Open('tmp/cache/n16gac10bit.l1b')
    md = ds.GetMetadata('SUBDATASETS')
    expected_md = {
      'SUBDATASET_2_NAME' : 'L1B_CLOUDS:"tmp/cache/n16gac10bit.l1b"',
      'SUBDATASET_2_DESC' : 'Clouds from AVHRR (CLAVR)'
    }
    for key in expected_md:
        if md[key] != expected_md[key]:
            print(md)
            return 'fail'
    ds = None

    ds = gdal.Open('L1B_CLOUDS:"tmp/cache/n16gac10bit.l1b"')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# 
def l1b_metadata_after_noaa_15():
    try:
        os.stat('tmp/cache/n16gac10bit.l1b')
    except:
        return 'skip'

    gdal.SetConfigOption('L1B_FETCH_METADATA', 'YES')
    gdal.SetConfigOption('L1B_METADATA_DIRECTORY', 'tmp')
    ds = gdal.Open('tmp/cache/n16gac10bit.l1b')
    gdal.SetConfigOption('L1B_FETCH_METADATA', None)
    gdal.SetConfigOption('L1B_METADATA_DIRECTORY', None)

    f = open('tmp/n16gac10bit.l1b_metadata.csv', 'rb')
    l = f.readline()
    if l != 'SCANLINE,NBLOCKYOFF,YEAR,DAY,MS_IN_DAY,SAT_CLOCK_DRIF_DELTA,SOUTHBOUND,SCANTIME_CORRECTED,C3_SELECT,FATAL_FLAG,TIME_ERROR,DATA_GAP,INSUFFICIENT_DATA_FOR_CAL,NO_EARTH_LOCATION,FIRST_GOOD_TIME_AFTER_CLOCK_UPDATE,INSTRUMENT_STATUS_CHANGED,SYNC_LOCK_DROPPED,FRAME_SYNC_ERROR,FRAME_SYNC_DROPPED_LOCK,FLYWHEELING,BIT_SLIPPAGE,TIP_PARITY_ERROR,REFLECTED_SUNLIGHT_C3B,REFLECTED_SUNLIGHT_C4,REFLECTED_SUNLIGHT_C5,RESYNC,P_N_STATUS,BAD_TIME_CAN_BE_INFERRED,BAD_TIME_CANNOT_BE_INFERRED,TIME_DISCONTINUITY,REPEAT_SCAN_TIME,UNCALIBRATED_BAD_TIME,CALIBRATED_FEWER_SCANLINES,UNCALIBRATED_BAD_PRT,CALIBRATED_MARGINAL_PRT,UNCALIBRATED_CHANNELS,NO_EARTH_LOC_BAD_TIME,EARTH_LOC_QUESTIONABLE_TIME,EARTH_LOC_QUESTIONABLE,EARTH_LOC_VERY_QUESTIONABLE,C3B_UNCALIBRATED,C3B_QUESTIONABLE,C3B_ALL_BLACKBODY,C3B_ALL_SPACEVIEW,C3B_MARGINAL_BLACKBODY,C3B_MARGINAL_SPACEVIEW,C4_UNCALIBRATED,C4_QUESTIONABLE,C4_ALL_BLACKBODY,C4_ALL_SPACEVIEW,C4_MARGINAL_BLACKBODY,C4_MARGINAL_SPACEVIEW,C5_UNCALIBRATED,C5_QUESTIONABLE,C5_ALL_BLACKBODY,C5_ALL_SPACEVIEW,C5_MARGINAL_BLACKBODY,C5_MARGINAL_SPACEVIEW,BIT_ERRORS,VIS_OP_CAL_C1_SLOPE_1,VIS_OP_CAL_C1_INTERCEPT_1,VIS_OP_CAL_C1_SLOPE_2,VIS_OP_CAL_C1_INTERCEPT_2,VIS_OP_CAL_C1_INTERSECTION,VIS_TEST_CAL_C1_SLOPE_1,VIS_TEST_CAL_C1_INTERCEPT_1,VIS_TEST_CAL_C1_SLOPE_2,VIS_TEST_CAL_C1_INTERCEPT_2,VIS_TEST_CAL_C1_INTERSECTION,VIS_PRELAUNCH_CAL_C1_SLOPE_1,VIS_PRELAUNCH_CAL_C1_INTERCEPT_1,VIS_PRELAUNCH_CAL_C1_SLOPE_2,VIS_PRELAUNCH_CAL_C1_INTERCEPT_2,VIS_PRELAUNCH_CAL_C1_INTERSECTION,VIS_OP_CAL_C2_SLOPE_1,VIS_OP_CAL_C2_INTERCEPT_1,VIS_OP_CAL_C2_SLOPE_2,VIS_OP_CAL_C2_INTERCEPT_2,VIS_OP_CAL_C2_INTERSECTION,VIS_TEST_CAL_C2_SLOPE_1,VIS_TEST_CAL_C2_INTERCEPT_1,VIS_TEST_CAL_C2_SLOPE_2,VIS_TEST_CAL_C2_INTERCEPT_2,VIS_TEST_CAL_C2_INTERSECTION,VIS_PRELAUNCH_CAL_C2_SLOPE_1,VIS_PRELAUNCH_CAL_C2_INTERCEPT_1,VIS_PRELAUNCH_CAL_C2_SLOPE_2,VIS_PRELAUNCH_CAL_C2_INTERCEPT_2,VIS_PRELAUNCH_CAL_C2_INTERSECTION,VIS_OP_CAL_C3A_SLOPE_1,VIS_OP_CAL_C3A_INTERCEPT_1,VIS_OP_CAL_C3A_SLOPE_2,VIS_OP_CAL_C3A_INTERCEPT_2,VIS_OP_CAL_C3A_INTERSECTION,VIS_TEST_CAL_C3A_SLOPE_1,VIS_TEST_CAL_C3A_INTERCEPT_1,VIS_TEST_CAL_C3A_SLOPE_2,VIS_TEST_CAL_C3A_INTERCEPT_2,VIS_TEST_CAL_C3A_INTERSECTION,VIS_PRELAUNCH_CAL_C3A_SLOPE_1,VIS_PRELAUNCH_CAL_C3A_INTERCEPT_1,VIS_PRELAUNCH_CAL_C3A_SLOPE_2,VIS_PRELAUNCH_CAL_C3A_INTERCEPT_2,VIS_PRELAUNCH_CAL_C3A_INTERSECTION,IR_OP_CAL_C3B_COEFF_1,IR_OP_CAL_C3B_COEFF_2,IR_OP_CAL_C3B_COEFF_3,IR_TEST_CAL_C3B_COEFF_1,IR_TEST_CAL_C3B_COEFF_2,IR_TEST_CAL_C3B_COEFF_3,IR_OP_CAL_C4_COEFF_1,IR_OP_CAL_C4_COEFF_2,IR_OP_CAL_C4_COEFF_3,IR_TEST_CAL_C4_COEFF_1,IR_TEST_CAL_C4_COEFF_2,IR_TEST_CAL_C4_COEFF_3,IR_OP_CAL_C5_COEFF_1,IR_OP_CAL_C5_COEFF_2,IR_OP_CAL_C5_COEFF_3,IR_TEST_CAL_C5_COEFF_1,IR_TEST_CAL_C5_COEFF_2,IR_TEST_CAL_C5_COEFF_3,EARTH_LOC_CORR_TIP_EULER,EARTH_LOC_IND,SPACECRAFT_ATT_CTRL,ATT_SMODE,ATT_PASSIVE_WHEEL_TEST,TIME_TIP_EULER,TIP_EULER_ROLL,TIP_EULER_PITCH,TIP_EULER_YAW,SPACECRAFT_ALT\n':
        print(l)
        return 'fail'
    l = f.readline()
    if l != '3406,0,2003,85,3275054,79,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0.052300,-2.015999,0.152800,-51.910000,499,0.052300,-2.015999,0.152800,-51.910000,498,0.052300,-2.015999,0.152800,-51.910000,498,0.051300,-1.942999,0.151000,-51.770000,500,0.051300,-1.942999,0.151000,-51.770000,500,0.051300,-1.942999,0.151000,-51.770000,500,0.000000,0.000000,0.000000,0.000000,0,0.000000,0.000000,0.000000,0.000000,0,0.000000,0.000000,0.000000,0.000000,0,2.488212,-0.002511,0.000000,2.488212,-0.002511,0.000000,179.546496,-0.188553,0.000008,179.546496,-0.188553,0.000008,195.236384,-0.201709,0.000006,195.236384,-0.201709,0.000006,0,0,0,0,0,608093,-0.021000,-0.007000,0.000000,862.000000\n':
        print(l)
        return 'fail'
    f.close()

    os.unlink('tmp/n16gac10bit.l1b_metadata.csv')

    return 'success'

gdaltest_list = []

l1b_list = [ ('http://download.osgeo.org/gdal/data/l1b', 'n12gac8bit.l1b', 51754, -1, 1938),
             ('http://download.osgeo.org/gdal/data/l1b', 'n12gac10bit.l1b', 46039, -1, 1887),
             ('http://download.osgeo.org/gdal/data/l1b', 'n12gac10bit_ebcdic.l1b', 46039, -1, 1887), # 2848
             ('http://download.osgeo.org/gdal/data/l1b', 'n14gac16bit.l1b', 42286, -1, 2142),
             ('http://download.osgeo.org/gdal/data/l1b', 'n15gac8bit.l1b', 55772, -1, 2091),
             ('http://download.osgeo.org/gdal/data/l1b', 'n16gac10bit.l1b', 6749, -1, 2142),
             ('http://download.osgeo.org/gdal/data/l1b', 'n17gac16bit.l1b', 61561, -1, 2040),
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'frang.1b', 33700, 30000, 357),  # 10 bit guess
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'franh.1b', 56702, 100000, 255), # 10 bit guess
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'calfirel.1b', 55071, 30000, 255), # 16 bit guess
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'rapnzg.1b', 58084, 30000, 612), # 16 bit guess
             ('ftp://ftp.sat.dundee.ac.uk/misc/testdata/new_noaa/new_klm_format', 'noaa18.n1b', 50229, 50000, 102),
             ('ftp://ftp.sat.dundee.ac.uk/misc/testdata/metop', 'noaa1b', 62411, 150000, 408)
           ]

for item in l1b_list:
    ut = TestL1B( item[0], item[1], item[2], item[3], item[4] )
    gdaltest_list.append( (ut.test, item[1]) )

gdaltest_list.append(l1b_geoloc)
gdaltest_list.append(l1b_solar_zenith_angles_before_noaa_15)
gdaltest_list.append(l1b_metadata_before_noaa_15)
gdaltest_list.append(l1b_angles_after_noaa_15)
gdaltest_list.append(l1b_clouds_after_noaa_15)
gdaltest_list.append(l1b_metadata_after_noaa_15)
if __name__ == '__main__':

    gdaltest.setup_run( 'l1b' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

