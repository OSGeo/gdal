#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SPOT DIMAP driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
import shutil
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Open and verify a the GCPs and metadata.

def dimap_1():

    shutil.copy('data/dimap/METADATA.DIM', 'tmp')
    shutil.copy('data/dimap/IMAGERY.TIF', 'tmp')
    shutil.copy('data/rgbsmall.tif', 'tmp')

    ds = gdal.Open( 'tmp/METADATA.DIM' )

    if ds.RasterCount != 1 \
       or ds.RasterXSize != 6000 \
       or ds.RasterYSize != 6000:
        gdaltest.post_reason ( 'wrong size or bands' )
        return 'fail'

    if ds.GetRasterBand(1).Checksum(0,0,100,100) != 21586:
        gdaltest.post_reason ( 'wrong checksum' )
        return 'fail'

    md = ds.GetMetadata()
    if md['PROCESSING_LEVEL'] != '1A':
        gdaltest.post_reason( 'metadata wrong.' )
        return 'fail'

    md = ds.GetMetadata()
    if md['SPECTRAL_PHYSICAL_BIAS'] != '0.000000':
        gdaltest.post_reason( 'metadata wrong.' )
        return 'fail'

    gcp_srs = ds.GetGCPProjection()
    if gcp_srs[:6] != 'GEOGCS' \
       or gcp_srs.find('WGS') == -1 \
       or gcp_srs.find('84') == -1:
        gdaltest.post_reason('GCP Projection not retained.')
        print(gcp_srs)
        return 'fail'

    gcps = ds.GetGCPs()
    if len(gcps) != 4 \
       or gcps[0].GCPPixel != 0.5 \
       or gcps[0].GCPLine  != 0.5 \
       or abs(gcps[0].GCPX - 4.3641728) > 0.0000002 \
       or abs(gcps[0].GCPY - 44.2082255) > 0.0000002 \
       or abs(gcps[0].GCPZ - 0) > 0.0000002:
        gdaltest.post_reason( 'GCPs wrong.' )
        print(len(gcps))
        print(gcps[0])
        return 'fail'

    ds = None
    os.unlink('tmp/METADATA.DIM')
    os.unlink('tmp/IMAGERY.TIF')
    os.unlink('tmp/rgbsmall.tif')

    return 'success'

###############################################################################
# Open DIMAP 2

def dimap_2():

    for name in [ 'data/dimap2', 'data/dimap2/VOL_PHR.XML', 'data/dimap2/DIM_foo.XML'] :
        ds = gdal.Open(name)
        if ds.RasterCount != 4 \
          or ds.RasterXSize != 20 \
          or ds.RasterYSize != 30:
            gdaltest.post_reason ( 'wrong size or bands' )
            return 'fail'

        md = ds.GetMetadata()
        expected_md = {'GEOMETRIC_ATTITUDES_USED': 'ACCURATE', 'FACILITY_PROCESSING_CENTER': 'PROCESSING_CENTER', 'GEOMETRIC_VERTICAL_DESC': 'REFERENCE3D', 'EPHEMERIS_ACQUISITION_ORBIT_DIRECTION': 'DESCENDING', 'BAND_MODE': 'PX', 'EPHEMERIS_NADIR_LON': 'NADIR_LON', 'EPHEMERIS_ACQUISITION_ORBIT_NUMBER': 'ACQUISITION_ORBIT_NUMBER', 'SPECTRAL_PROCESSING': 'PMS', 'CLOUDCOVER_MEASURE_TYPE': 'AUTOMATIC', 'DATASET_JOB_ID': 'JOB_ID', 'MISSION': 'PHR', 'GEOMETRIC_GROUND_SETTING': 'true', 'GEOMETRIC_VERTICAL_SETTING': 'true', 'DATASET_PRODUCTION_DATE': 'PRODUCTION_DATE', 'DATASET_PRODUCER_CONTACT': 'PRODUCER_CONTACT', 'IMAGING_DATE': 'IMAGING_DATE', 'CLOUDCOVER_QUALITY_TABLES': 'PHR', 'DATASET_PRODUCER_NAME': 'PRODUCER_NAME', 'GEOMETRIC_GEOMETRIC_PROCESSING': 'SENSOR', 'GEOMETRIC_EPHEMERIS_USED': 'CORRECTED', 'GEOMETRIC_GROUND_DESC': 'R3D_ORTHO', 'DATASET_DELIVERY_TYPE': 'DELIVERY_TYPE', 'PROCESSING_LEVEL': 'SENSOR', 'DATASET_PRODUCER_ADDRESS': 'PRODUCER_ADDRESS', 'DATASET_PRODUCT_CODE': 'PRODUCT_CODE', 'INSTRUMENT_INDEX': '1A', 'EPHEMERIS_NADIR_LAT': 'NADIR_LAT', 'INSTRUMENT': 'PHR', 'CLOUDCOVER_MEASURE_NAME': 'Cloud_Cotation (CLD)', 'FACILITY_SOFTWARE': 'SOFTWARE', 'IMAGING_TIME': 'IMAGING_TIME', 'MISSION_INDEX': '1A'}
        if md != expected_md:
            gdaltest.post_reason( 'metadata wrong.' )
            print(md)
            return 'fail'

        rpc = ds.GetMetadata('RPC')
        expected_rpc = {'HEIGHT_OFF': 'HEIGHT_OFF', 'LINE_NUM_COEFF': ' LINE_NUM_COEFF_1 LINE_NUM_COEFF_2 LINE_NUM_COEFF_3 LINE_NUM_COEFF_4 LINE_NUM_COEFF_5 LINE_NUM_COEFF_6 LINE_NUM_COEFF_7 LINE_NUM_COEFF_8 LINE_NUM_COEFF_9 LINE_NUM_COEFF_10 LINE_NUM_COEFF_11 LINE_NUM_COEFF_12 LINE_NUM_COEFF_13 LINE_NUM_COEFF_14 LINE_NUM_COEFF_15 LINE_NUM_COEFF_16 LINE_NUM_COEFF_17 LINE_NUM_COEFF_18 LINE_NUM_COEFF_19 LINE_NUM_COEFF_20', 'LONG_OFF': 'LONG_OFF', 'SAMP_DEN_COEFF': ' SAMP_DEN_COEFF_1 SAMP_DEN_COEFF_2 SAMP_DEN_COEFF_3 SAMP_DEN_COEFF_4 SAMP_DEN_COEFF_5 SAMP_DEN_COEFF_6 SAMP_DEN_COEFF_7 SAMP_DEN_COEFF_8 SAMP_DEN_COEFF_9 SAMP_DEN_COEFF_10 SAMP_DEN_COEFF_11 SAMP_DEN_COEFF_12 SAMP_DEN_COEFF_13 SAMP_DEN_COEFF_14 SAMP_DEN_COEFF_15 SAMP_DEN_COEFF_16 SAMP_DEN_COEFF_17 SAMP_DEN_COEFF_18 SAMP_DEN_COEFF_19 SAMP_DEN_COEFF_20', 'LINE_SCALE': 'LINE_SCALE', 'SAMP_NUM_COEFF': ' SAMP_NUM_COEFF_1 SAMP_NUM_COEFF_2 SAMP_NUM_COEFF_3 SAMP_NUM_COEFF_4 SAMP_NUM_COEFF_5 SAMP_NUM_COEFF_6 SAMP_NUM_COEFF_7 SAMP_NUM_COEFF_8 SAMP_NUM_COEFF_9 SAMP_NUM_COEFF_10 SAMP_NUM_COEFF_11 SAMP_NUM_COEFF_12 SAMP_NUM_COEFF_13 SAMP_NUM_COEFF_14 SAMP_NUM_COEFF_15 SAMP_NUM_COEFF_16 SAMP_NUM_COEFF_17 SAMP_NUM_COEFF_18 SAMP_NUM_COEFF_19 SAMP_NUM_COEFF_20', 'LONG_SCALE': 'LONG_SCALE', 'SAMP_SCALE': 'SAMP_SCALE', 'SAMP_OFF': '4', 'LAT_SCALE': 'LAT_SCALE', 'LAT_OFF': 'LAT_OFF', 'LINE_OFF': '9', 'LINE_DEN_COEFF': ' LINE_DEN_COEFF_1 LINE_DEN_COEFF_2 LINE_DEN_COEFF_3 LINE_DEN_COEFF_4 LINE_DEN_COEFF_5 LINE_DEN_COEFF_6 LINE_DEN_COEFF_7 LINE_DEN_COEFF_8 LINE_DEN_COEFF_9 LINE_DEN_COEFF_10 LINE_DEN_COEFF_11 LINE_DEN_COEFF_12 LINE_DEN_COEFF_13 LINE_DEN_COEFF_14 LINE_DEN_COEFF_15 LINE_DEN_COEFF_16 LINE_DEN_COEFF_17 LINE_DEN_COEFF_18 LINE_DEN_COEFF_19 LINE_DEN_COEFF_20', 'HEIGHT_SCALE': 'HEIGHT_SCALE'}
        if rpc != expected_rpc:
            gdaltest.post_reason( 'RPC wrong.' )
            print(rpc)
            return 'fail'
        ds = None

    # Shouldn't be needed ideally
    gdal.Unlink('data/dimap2/IMG_foo_R1C1.TIF.aux.xml')

    return 'success'


gdaltest_list = [
    dimap_1,
    dimap_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'dimap' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

