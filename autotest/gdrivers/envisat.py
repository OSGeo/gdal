#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ENVISAT driver.
# Author:   Antonio Valentino <antonio dot valentino at tiscali dot it>
#
###############################################################################
# Copyright (c) 2011, Antonio Valentino <antonio dot valentino at tiscali dot it>
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
import gzip
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest


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
class TestEnvisat:
    # Just a base class

    def __init__( self, downloadURL, fileName, size, checksum ):
        self.downloadURL = downloadURL
        self.fileName = fileName
        self.size = size
        self.checksum = checksum

    def download_file( self ):
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

    def test_envisat_1( self ):
        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        if (ds.RasterXSize, ds.RasterYSize) != self.size:
            gdaltest.post_reason('Bad size. Expected %s, got %s' % (self.size, (ds.RasterXSize, ds.RasterYSize)))
            return 'failure'

        return 'success'

    def test_envisat_2( self ):
        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        if ds.GetRasterBand(1).Checksum() != self.checksum:
            gdaltest.post_reason('Bad checksum. Expected %d, got %d' % (self.checksum, ds.GetRasterBand(1).Checksum()))
            return 'failure'

        return 'success'

    def test_envisat_3( self ):
        # regrassion test for #3160 and #3709

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        d = {}
        for gcp in ds.GetGCPs():
            lp = (gcp.GCPLine, gcp.GCPPixel)
            if lp in d:
                gdaltest.post_reason('Duplicate GCP coordinates.')
                return 'failure'
            else:
                d[lp] = (gcp.GCPX, gcp.GCPY, gcp.GCPZ)

        return 'success'

    def test_envisat_4( self ):
        # test number of bands

        if not self.download_file():
            return 'skip'

        filename = os.path.join('tmp', 'cache', self.fileName)
        mds_num = _get_mds_num(filename)

        ds = gdal.Open(filename)
        if ds is None:
            return 'fail'

        if ds.RasterCount < mds_num:
            gdaltest.post_reason('Not all bands have been detected')
            return 'failure'

        return 'success'

    def test_envisat_5( self ):
        # test metadata in RECORDS domain

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        product = ds.GetMetadataItem('MPH_PRODUCT')
        record_md = ds.GetMetadata('RECORDS')

        if product[:3] not in ('ASA', 'SAR', 'MER') and record_md:
            gdaltest.post_reason('Unexpected metadata in the "RECORDS" domain.')
            return 'failure'

        return 'success'

###############################################################################
#
class TestEnvisatASAR(TestEnvisat):
    def test_envisat_asar_1( self ):
        # test sensor ID

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        product = ds.GetMetadataItem('MPH_PRODUCT')

        if product[:3] not in ('ASA', 'SAR'):
            gdaltest.post_reason('Wrong sensor ID.')
            return 'failure'

        return 'success'

    def test_envisat_asar_2( self ):
        # test metadata in RECORDS domain

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        product = ds.GetMetadataItem('MPH_PRODUCT')
        record_md = ds.GetMetadata('RECORDS')

        if not record_md:
            gdaltest.post_reason('Unable to read ADS metadata from ASAR.')
            return 'failure'

        record = 'SQ_ADS' # it is present in all ASAR poducts
        if product.startswith('ASA_WV'):
            for field in ('ZERO_DOPPLER_TIME',
                          'INPUT_MEAN',
                          'INPUT_STD_DEV',
                          'PHASE_CROSS_CONF'):
                key0 = '%s_%s' % (record, field)
                key1 = '%s_0_%s' % (record, field)
                if key0 not in record_md and key1 not in record_md:
                    gdaltest.post_reason(
                        'No "%s" or "%s" key in "RECORDS" domain.' %
                                                            (key0, key1))
                    return 'failure'
        else:
            for mds in range(1, ds.RasterCount + 1):
                for field in ('ZERO_DOPPLER_TIME',
                              'INPUT_MEAN',
                              'INPUT_STD_DEV'):
                    key0 = 'MDS%d_%s_%s' % (mds, record, field)
                    key1 = 'MDS%d_%s_0_%s' % (mds, record, field)
                    if key0 not in record_md and key1 not in record_md:
                        gdaltest.post_reason(
                            'No "%s" or "%s" key in "RECORDS" domain.' %
                                                            (key0, key1))
                        return 'failure'

        return 'success'

###############################################################################
#
class TestEnvisatMERIS(TestEnvisat):
    def test_envisat_meris_1( self ):
        # test sensor ID

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        product = ds.GetMetadataItem('MPH_PRODUCT')

        if product[:3] not in ('MER',):
            gdaltest.post_reason('Wrong sensor ID.')
            return 'failure'

        return 'success'

    def test_envisat_meris_2( self ):
        # test metadata in RECORDS domain

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        record_md = ds.GetMetadata('RECORDS')

        if not record_md:
            gdaltest.post_reason('Unable to read ADS metadata from ASAR.')
            return 'failure'

        record = 'Quality_ADS' # it is present in all MER poducts

        for field in ('DSR_TIME', 'ATTACH_FLAG'):
            key0 = '%s_%s' % (record, field)
            key1 = '%s_0_%s' % (record, field)
            if key0 not in record_md and key1 not in record_md:
                gdaltest.post_reason(
                    'No "%s" or "%s" key in "RECORDS" domain.' %
                                                        (key0, key1))
                return 'failure'

        return 'success'

    def test_envisat_meris_3( self ):
        # test Flag bands

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

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
            if not flags_band:
                gdaltest.post_reason('No flag band in MERIS Level 1 product.')
                return 'failure'

            band = ds.GetRasterBand(flags_band)
            if band.DataType != gdal.GDT_Byte:
                gdaltest.post_reason('Incorrect data type of the flag band in '
                                     'MERIS Level 1 product.')
                return 'failure'

            if not detector_index_band:
                gdaltest.post_reason('No "detector index" band in MERIS '
                                     'Level 1 product.')
                return 'failure'

            band = ds.GetRasterBand(detector_index_band)
            if band.DataType != gdal.GDT_Int16:
                gdaltest.post_reason('Incorrect data type of the '
                                     '"detector index" band in MERIS Level 2 '
                                     'product.')
                return 'failure'
        elif level == '2':
            if not flags_band:
                gdaltest.post_reason('No flag band in MERIS Level 2 product.')
                return 'failure'

            band = ds.GetRasterBand(flags_band)
            if band.DataType != gdal.GDT_UInt32:
                gdaltest.post_reason('Incorrect data type of the flag band in '
                                     'MERIS Level 2 product.')
                return 'failure'
        else:
            gdaltest.post_reason('Invalid product level: %s.' % level)
            return 'failure'

        return 'success'


ut1 = TestEnvisatASAR(
    'http://earth.esa.int/services/sample_products/asar/DS1/WS/ASA_WS__BPXPDE20020714_100425_000001202007_00380_01937_0053.N1.gz',
    'ASA_WS__BPXPDE20020714_100425_000001202007_00380_01937_0053.N1',
    (524, 945),
    44998 )
ut2 = TestEnvisatMERIS(
    'http://earth.esa.int/services/sample_products/meris/RRC/L2/MER_RRC_2PTGMV20000620_104318_00000104X000_00000_00000_0001.N1.gz',
    'MER_RRC_2PTGMV20000620_104318_00000104X000_00000_00000_0001.N1',
    (1121, 593),
    55146 )

gdaltest_list = [
    ut1.test_envisat_1,
    ut1.test_envisat_2,
    ut1.test_envisat_3,
    ut1.test_envisat_4,
    ut1.test_envisat_asar_1,
    ut1.test_envisat_asar_2,
    ut2.test_envisat_1,
    ut2.test_envisat_2,
    ut2.test_envisat_3,
    ut2.test_envisat_4,
    ut2.test_envisat_meris_1,
    ut2.test_envisat_meris_2,
    ut2.test_envisat_meris_3,
]


if __name__ == '__main__':

    gdaltest.setup_run( 'envisat' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

