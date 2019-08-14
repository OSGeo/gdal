#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ENVISAT driver.
# Author:   Antonio Valentino <antonio dot valentino at tiscali dot it>
#
###############################################################################
# Copyright (c) 2011, Antonio Valentino <antonio dot valentino at tiscali dot it>
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
import gzip
from osgeo import gdal


import gdaltest
import pytest


def _get_mds_num(filename):
    mph_size = 1247

    fd = open(filename, 'rb')
    mph = fd.read(mph_size)
    for line in mph.splitlines():
        line = line.decode('iso8859-1')
        if line.startswith('SPH_SIZE'):
            sph_size = int(line.split('=')[-1][:-7])
            break
    else:
        return

    sph = fd.read(sph_size)
    sph = '\n'.join(line.decode('iso8859-1').rstrip()
                    for line in sph.splitlines())
    count = 0
    for block in sph.split('\n\n'):
        if block.startswith('DS_NAME'):
            ds_type = None
            ds_size = 0
            for line in block.splitlines():
                if line.startswith('DS_TYPE'):
                    ds_type = line.split('=')[-1]
                elif line.startswith('DS_SIZE'):
                    ds_size = int(line.split('=')[-1][:-7])
            if ds_type == 'M' and ds_size > 0:
                count += 1

    return count

###############################################################################
#


class EnvisatTestBase(object):
    # Just a base class

    def download_file(self):
        # download and decompress
        if not gdaltest.download_file(self.downloadURL, os.path.basename(self.downloadURL), -1):
            return False

        filename = os.path.join('tmp', 'cache', self.fileName)
        if os.path.exists(filename):
            return True

        # decompress
        f_in = gzip.open(os.path.join('tmp', 'cache', os.path.basename(self.downloadURL)))
        f_out = open(filename, 'wb')
        f_out.write(f_in.read())
        f_in.close()
        f_out.close()

        return True

    def test_envisat_1(self):
        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        assert (ds.RasterXSize, ds.RasterYSize) == self.size, \
            ('Bad size. Expected %s, got %s' % (self.size, (ds.RasterXSize, ds.RasterYSize)))

    def test_envisat_2(self):
        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        assert ds.GetRasterBand(1).Checksum() == self.checksum, \
            ('Bad checksum. Expected %d, got %d' % (self.checksum, ds.GetRasterBand(1).Checksum()))

    def test_envisat_3(self):
        # Regression test for #3160 and #3709.

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        d = {}
        for gcp in ds.GetGCPs():
            lp = (gcp.GCPLine, gcp.GCPPixel)
            if lp in d:
                pytest.fail('Duplicate GCP coordinates.')
            else:
                d[lp] = (gcp.GCPX, gcp.GCPY, gcp.GCPZ)

        
    def test_envisat_4(self):
        # test number of bands

        if not self.download_file():
            pytest.skip()

        filename = os.path.join('tmp', 'cache', self.fileName)
        mds_num = _get_mds_num(filename)

        ds = gdal.Open(filename)
        assert ds is not None

        assert ds.RasterCount >= mds_num, 'Not all bands have been detected'

    def test_envisat_5(self):
        # test metadata in RECORDS domain

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        product = ds.GetMetadataItem('MPH_PRODUCT')
        record_md = ds.GetMetadata('RECORDS')

        assert product[:3] in ('ASA', 'SAR', 'MER') or not record_md, \
            'Unexpected metadata in the "RECORDS" domain.'

###############################################################################
#


class TestEnvisatASAR(EnvisatTestBase):
    downloadURL = 'http://earth.esa.int/services/sample_products/asar/DS1/WS/ASA_WS__BPXPDE20020714_100425_000001202007_00380_01937_0053.N1.gz'
    fileName = 'ASA_WS__BPXPDE20020714_100425_000001202007_00380_01937_0053.N1'
    size = (524, 945)
    checksum = 44998

    def test_envisat_asar_1(self):
        # test sensor ID

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        product = ds.GetMetadataItem('MPH_PRODUCT')

        assert product[:3] in ('ASA', 'SAR'), 'Wrong sensor ID.'

    def test_envisat_asar_2(self):
        # test metadata in RECORDS domain

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        product = ds.GetMetadataItem('MPH_PRODUCT')
        record_md = ds.GetMetadata('RECORDS')

        assert record_md, 'Unable to read ADS metadata from ASAR.'

        record = 'SQ_ADS'  # it is present in all ASAR poducts
        if product.startswith('ASA_WV'):
            for field in ('ZERO_DOPPLER_TIME',
                          'INPUT_MEAN',
                          'INPUT_STD_DEV',
                          'PHASE_CROSS_CONF'):
                key0 = '%s_%s' % (record, field)
                key1 = '%s_0_%s' % (record, field)
                assert key0 in record_md or key1 in record_md, \
                    ('No "%s" or "%s" key in "RECORDS" domain.' %
                        (key0, key1))
        else:
            for mds in range(1, ds.RasterCount + 1):
                for field in ('ZERO_DOPPLER_TIME',
                              'INPUT_MEAN',
                              'INPUT_STD_DEV'):
                    key0 = 'MDS%d_%s_%s' % (mds, record, field)
                    key1 = 'MDS%d_%s_0_%s' % (mds, record, field)
                    assert key0 in record_md or key1 in record_md, \
                        ('No "%s" or "%s" key in "RECORDS" domain.' %
                            (key0, key1))

        
###############################################################################
#


class TestEnvisatMERIS(EnvisatTestBase):
    downloadURL = 'http://earth.esa.int/services/sample_products/meris/RRC/L2/MER_RRC_2PTGMV20000620_104318_00000104X000_00000_00000_0001.N1.gz'
    fileName = 'MER_RRC_2PTGMV20000620_104318_00000104X000_00000_00000_0001.N1'
    size = (1121, 593)
    checksum = 55146

    def test_envisat_meris_1(self):
        # test sensor ID

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        product = ds.GetMetadataItem('MPH_PRODUCT')

        assert product[:3] in ('MER',), 'Wrong sensor ID.'

    def test_envisat_meris_2(self):
        # test metadata in RECORDS domain

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        record_md = ds.GetMetadata('RECORDS')

        assert record_md, 'Unable to read ADS metadata from ASAR.'

        record = 'Quality_ADS'  # it is present in all MER poducts

        for field in ('DSR_TIME', 'ATTACH_FLAG'):
            key0 = '%s_%s' % (record, field)
            key1 = '%s_0_%s' % (record, field)
            assert key0 in record_md or key1 in record_md, \
                ('No "%s" or "%s" key in "RECORDS" domain.' %
                    (key0, key1))

        
    def test_envisat_meris_3(self):
        # test Flag bands

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        flags_band = None
        detector_index_band = None
        for bandno in range(1, ds.RasterCount + 1):
            band = ds.GetRasterBand(bandno)
            name = band.GetDescription()
            if 'Flags' in name:
                flags_band = bandno
            if name.startswith('Detector index'):
                detector_index_band = bandno

        product = ds.GetMetadataItem('MPH_PRODUCT')
        level = product[8]
        if level == '1':
            assert flags_band, 'No flag band in MERIS Level 1 product.'

            band = ds.GetRasterBand(flags_band)
            assert band.DataType == gdal.GDT_Byte, \
                ('Incorrect data type of the flag band in '
                                     'MERIS Level 1 product.')

            assert detector_index_band, ('No "detector index" band in MERIS '
                                     'Level 1 product.')

            band = ds.GetRasterBand(detector_index_band)
            assert band.DataType == gdal.GDT_Int16, ('Incorrect data type of the '
                                     '"detector index" band in MERIS Level 2 '
                                     'product.')
        elif level == '2':
            assert flags_band, 'No flag band in MERIS Level 2 product.'

            band = ds.GetRasterBand(flags_band)
            assert band.DataType == gdal.GDT_UInt32, \
                ('Incorrect data type of the flag band in '
                                     'MERIS Level 2 product.')
        else:
            pytest.fail('Invalid product level: %s.' % level)

        
    def test_envisat_meris_4(self):
        # test DEM corrections (see #5423)

        if not self.download_file():
            pytest.skip()

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        assert ds is not None

        gcp_values = [
            (gcp.Id, gcp.GCPLine, gcp.GCPPixel, gcp.GCPX, gcp.GCPY, gcp.GCPZ)
            for gcp in ds.GetGCPs()
        ]

        ref = [
            ('1', 0.5, 0.5, 6.484722, 47.191889, 0.0),
            ('2', 0.5, 16.5, 6.279611, 47.245535999999994, 0.0),
            ('3', 0.5, 32.5, 6.074068, 47.298809, 0.0),
            ('4', 0.5, 48.5, 5.868156999999999, 47.351724999999995, 0.0),
            ('5', 0.5, 64.5, 5.661817999999999, 47.404264999999995, 0.0),
            ('6', 0.5, 80.5, 5.455087999999999, 47.456436, 0.0),
            ('7', 0.5, 96.5, 5.247959, 47.508236000000004, 0.0),
            ('8', 0.5, 112.5, 5.04043, 47.559663, 0.0),
            ('9', 0.5, 128.5, 4.8324869999999995, 47.61071, 0.0),
            ('10', 0.5, 144.5, 4.624124, 47.66137499999999, 0.0),
        ]

        for r, v in zip(ref, gcp_values):
            for i, ri in enumerate(r):
                if float(ri) != pytest.approx(float(v[i]), abs=1e-10):
                    print(r)
                    pytest.fail('Wrong GCP coordinates.')

        
