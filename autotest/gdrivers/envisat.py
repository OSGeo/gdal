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
import gdal

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
#
class TestEnvisat:
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
        # test metadata in RECORDS domain

        if not self.download_file():
            return 'skip'

        ds = gdal.Open(os.path.join('tmp', 'cache', self.fileName))
        if ds is None:
            return 'fail'

        product = ds.GetMetadataItem('MPH_PRODUCT')
        record_md = ds.GetMetadata('RECORDS')

        if product.startswith('ASA') and not record_md:
            gdaltest.post_reason('Unable to read ADS metadata from ASAR.')
            return 'failure'

        if not product.startswith('ASA') and record_md:
            gdaltest.post_reason('Unexpected metadata in the "RECORDS" domain.')
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
                    gdaltest.post_reason('No "%s" or "%s" key in "RECORDS"'
                                         ' domain.' % (key0, key1))
                    return 'failure'
        else:
            for mds in range(1, ds.RasterCount + 1):
                for field in ('ZERO_DOPPLER_TIME',
                              'INPUT_MEAN',
                              'INPUT_STD_DEV'):
                    key0 = 'MDS%d_%s_%s' % (mds, record, field)
                    key1 = 'MDS%d_%s_0_%s' % (mds, record, field)
                    if key0 not in record_md and key1 not in record_md:
                        gdaltest.post_reason('No "%s" or "%s" key in "RECORDS"'
                                             ' domain.' % (key0, key1))
                        return 'failure'

        return 'success'

ut = TestEnvisat( 'http://earth.esa.int/services/sample_products/asar/DS1/WS/ASA_WS__BPXPDE20020714_100425_000001202007_00380_01937_0053.N1.gz', 'ASA_WS__BPXPDE20020714_100425_000001202007_00380_01937_0053.N1', (524, 945), 44998 )

gdaltest_list = [
    ut.test_envisat_1,
    ut.test_envisat_2,
    ut.test_envisat_3,
    ut.test_envisat_4,
]


if __name__ == '__main__':

    gdaltest.setup_run( 'envisat' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

