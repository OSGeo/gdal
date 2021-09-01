#!/usr/bin/env pytest
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
import shutil
from osgeo import gdal
import pytest



###############################################################################
# Open and verify a the GCPs and metadata.


def test_dimap_1():

    shutil.copy('data/dimap/METADATA.DIM', 'tmp')
    shutil.copy('data/dimap/IMAGERY.TIF', 'tmp')
    shutil.copy('data/rgbsmall.tif', 'tmp')

    ds = gdal.Open('tmp/METADATA.DIM')

    assert ds.RasterCount == 1 and ds.RasterXSize == 6000 and ds.RasterYSize == 6000, \
        'wrong size or bands'

    assert ds.GetRasterBand(1).Checksum(0, 0, 100, 100) == 21586, 'wrong checksum'

    md = ds.GetMetadata()
    assert md['PROCESSING_LEVEL'] == '1A', 'metadata wrong.'

    md = ds.GetMetadata()
    assert md['SPECTRAL_PHYSICAL_BIAS'] == '0.000000', 'metadata wrong.'

    gcp_srs = ds.GetGCPProjection()
    assert (not (gcp_srs[:6] != 'GEOGCS' \
       or gcp_srs.find('WGS') == -1 \
       or gcp_srs.find('84') == -1)), 'GCP Projection not retained.'

    gcps = ds.GetGCPs()
    assert len(gcps) == 4 and gcps[0].GCPPixel == 0.5 and gcps[0].GCPLine == 0.5 and gcps[0].GCPX == pytest.approx(4.3641728, abs=0.0000002) and gcps[0].GCPY == pytest.approx(44.2082255, abs=0.0000002) and gcps[0].GCPZ == pytest.approx(0, abs=0.0000002), \
        'GCPs wrong.'

    ds = None
    os.unlink('tmp/METADATA.DIM')
    os.unlink('tmp/IMAGERY.TIF')
    os.unlink('tmp/rgbsmall.tif')

###############################################################################
# Open DIMAP 2


def test_dimap_2_single_component():

    for name in ['data/dimap2/single_component', 'data/dimap2/single_component/VOL_PHR.XML', 'data/dimap2/single_component/DIM_foo.XML']:
        ds = gdal.Open(name)
        assert ds.RasterCount == 4 and ds.RasterXSize == 20 and ds.RasterYSize == 30, \
            'wrong size or bands'

        md = ds.GetMetadata()
        expected_md = {'GEOMETRIC_ATTITUDES_USED': 'ACCURATE', 'FACILITY_PROCESSING_CENTER': 'PROCESSING_CENTER', 'GEOMETRIC_VERTICAL_DESC': 'REFERENCE3D', 'EPHEMERIS_ACQUISITION_ORBIT_DIRECTION': 'DESCENDING', 'BAND_MODE': 'PX', 'EPHEMERIS_NADIR_LON': 'NADIR_LON', 'EPHEMERIS_ACQUISITION_ORBIT_NUMBER': 'ACQUISITION_ORBIT_NUMBER', 'SPECTRAL_PROCESSING': 'PMS', 'CLOUDCOVER_MEASURE_TYPE': 'AUTOMATIC', 'DATASET_JOB_ID': 'JOB_ID', 'MISSION': 'PHR', 'GEOMETRIC_GROUND_SETTING': 'true', 'GEOMETRIC_VERTICAL_SETTING': 'true', 'DATASET_PRODUCTION_DATE': 'PRODUCTION_DATE', 'DATASET_PRODUCER_CONTACT': 'PRODUCER_CONTACT', 'IMAGING_DATE': '2016-06-17', 'CLOUDCOVER_QUALITY_TABLES': 'PHR', 'DATASET_PRODUCER_NAME': 'PRODUCER_NAME', 'GEOMETRIC_GEOMETRIC_PROCESSING': 'SENSOR', 'GEOMETRIC_EPHEMERIS_USED': 'CORRECTED', 'GEOMETRIC_GROUND_DESC': 'R3D_ORTHO', 'DATASET_DELIVERY_TYPE': 'DELIVERY_TYPE', 'PROCESSING_LEVEL': 'SENSOR', 'DATASET_PRODUCER_ADDRESS': 'PRODUCER_ADDRESS', 'DATASET_PRODUCT_CODE': 'PRODUCT_CODE', 'INSTRUMENT_INDEX': '1A', 'EPHEMERIS_NADIR_LAT': 'NADIR_LAT', 'INSTRUMENT': 'PHR', 'CLOUDCOVER_MEASURE_NAME': 'Cloud_Cotation (CLD)', 'FACILITY_SOFTWARE': 'SOFTWARE', 'IMAGING_TIME': '12:34:56', 'MISSION_INDEX': '1A'}
        assert md == expected_md, 'metadata wrong.'

        rpc = ds.GetMetadata('RPC')
        expected_rpc = {'HEIGHT_OFF': 'HEIGHT_OFF', 'LINE_NUM_COEFF': ' LINE_NUM_COEFF_1 LINE_NUM_COEFF_2 LINE_NUM_COEFF_3 LINE_NUM_COEFF_4 LINE_NUM_COEFF_5 LINE_NUM_COEFF_6 LINE_NUM_COEFF_7 LINE_NUM_COEFF_8 LINE_NUM_COEFF_9 LINE_NUM_COEFF_10 LINE_NUM_COEFF_11 LINE_NUM_COEFF_12 LINE_NUM_COEFF_13 LINE_NUM_COEFF_14 LINE_NUM_COEFF_15 LINE_NUM_COEFF_16 LINE_NUM_COEFF_17 LINE_NUM_COEFF_18 LINE_NUM_COEFF_19 LINE_NUM_COEFF_20', 'LONG_OFF': 'LONG_OFF', 'SAMP_DEN_COEFF': ' SAMP_DEN_COEFF_1 SAMP_DEN_COEFF_2 SAMP_DEN_COEFF_3 SAMP_DEN_COEFF_4 SAMP_DEN_COEFF_5 SAMP_DEN_COEFF_6 SAMP_DEN_COEFF_7 SAMP_DEN_COEFF_8 SAMP_DEN_COEFF_9 SAMP_DEN_COEFF_10 SAMP_DEN_COEFF_11 SAMP_DEN_COEFF_12 SAMP_DEN_COEFF_13 SAMP_DEN_COEFF_14 SAMP_DEN_COEFF_15 SAMP_DEN_COEFF_16 SAMP_DEN_COEFF_17 SAMP_DEN_COEFF_18 SAMP_DEN_COEFF_19 SAMP_DEN_COEFF_20', 'LINE_SCALE': 'LINE_SCALE', 'SAMP_NUM_COEFF': ' SAMP_NUM_COEFF_1 SAMP_NUM_COEFF_2 SAMP_NUM_COEFF_3 SAMP_NUM_COEFF_4 SAMP_NUM_COEFF_5 SAMP_NUM_COEFF_6 SAMP_NUM_COEFF_7 SAMP_NUM_COEFF_8 SAMP_NUM_COEFF_9 SAMP_NUM_COEFF_10 SAMP_NUM_COEFF_11 SAMP_NUM_COEFF_12 SAMP_NUM_COEFF_13 SAMP_NUM_COEFF_14 SAMP_NUM_COEFF_15 SAMP_NUM_COEFF_16 SAMP_NUM_COEFF_17 SAMP_NUM_COEFF_18 SAMP_NUM_COEFF_19 SAMP_NUM_COEFF_20', 'LONG_SCALE': 'LONG_SCALE', 'SAMP_SCALE': 'SAMP_SCALE', 'SAMP_OFF': '4', 'LAT_SCALE': 'LAT_SCALE', 'LAT_OFF': 'LAT_OFF', 'LINE_OFF': '9', 'LINE_DEN_COEFF': ' LINE_DEN_COEFF_1 LINE_DEN_COEFF_2 LINE_DEN_COEFF_3 LINE_DEN_COEFF_4 LINE_DEN_COEFF_5 LINE_DEN_COEFF_6 LINE_DEN_COEFF_7 LINE_DEN_COEFF_8 LINE_DEN_COEFF_9 LINE_DEN_COEFF_10 LINE_DEN_COEFF_11 LINE_DEN_COEFF_12 LINE_DEN_COEFF_13 LINE_DEN_COEFF_14 LINE_DEN_COEFF_15 LINE_DEN_COEFF_16 LINE_DEN_COEFF_17 LINE_DEN_COEFF_18 LINE_DEN_COEFF_19 LINE_DEN_COEFF_20', 'HEIGHT_SCALE': 'HEIGHT_SCALE'}
        assert rpc == expected_rpc, 'RPC wrong.'

        cs = ds.GetRasterBand(1).Checksum()
        assert cs == 7024, 'wrong checksum.'

        nOvr = ds.GetRasterBand(1).GetOverviewCount()
        assert nOvr == 1, 'overviews not correctly exposed'

        ds = None


###############################################################################
# Open DIMAP 2


def test_dimap_2_bundle():

    ds = gdal.Open('data/dimap2/bundle')
    assert ds.RasterCount == 4 and ds.RasterXSize == 20 and ds.RasterYSize == 30
    md = ds.GetMetadata()
    assert md != {}
    assert ds.GetMetadata('RPC') is not None
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 7024
    subds = ds.GetSubDatasets()
    assert len(subds) == 2

    # Open first subdataset
    ds = gdal.Open(subds[0][0])
    assert ds.RasterCount == 4 and ds.RasterXSize == 20 and ds.RasterYSize == 30
    md = ds.GetMetadata()
    assert md != {}
    assert ds.GetMetadata('RPC') is not None
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 7024
    assert len(ds.GetSubDatasets()) == 0

    # Open second subdataset
    ds = gdal.Open(subds[1][0])
    assert ds.RasterCount == 1 and ds.RasterXSize == 20 and ds.RasterYSize == 30
    md = ds.GetMetadata()
    assert md != {}
    assert ds.GetMetadata('RPC') is not None
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 7024


###############################################################################
# Open DIMAP VHR2020 MS-FS


def test_dimap_2_vhr2020_ms_fs():

    ds = gdal.Open('data/dimap2/vhr2020_ms_fs')
    assert ds.RasterCount == 6 and ds.RasterXSize == 1663 and ds.RasterYSize == 1366
    assert [ds.GetRasterBand(i+1).ComputeRasterMinMax()[0] for i in range(6)] == [1,2,3,4,5,6]
    assert [ds.GetRasterBand(i+1).GetDescription() for i in range(6)] == ['Red', 'Green', 'Blue', 'NIR', 'Red Edge', 'Deep Blue']
