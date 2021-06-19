#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalinfo
# Author:   Faza Mahamood <fazamhd at gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
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

###############################################################################
# Simple test


def test_gdalinfo_lib_1():

    ds = gdal.Open('../gcore/data/byte.tif')

    ret = gdal.Info(ds)
    assert ret.find('Driver: GTiff/GeoTIFF') != -1, 'did not get expected string.'

###############################################################################
# Test Json format


def test_gdalinfo_lib_2():

    ds = gdal.Open('../gcore/data/byte.tif')

    ret = gdal.Info(ds, format='json')
    assert ret['driverShortName'] == 'GTiff', 'wrong value for driverShortName.'

###############################################################################
# Test extraMDDomains()


def test_gdalinfo_lib_3():

    ds = gdal.Open('../gdrivers/data/nitf/fake_nsif.ntf')

    ret = gdal.Info(ds, format='json')
    assert 'TRE' not in ret['metadata'], 'got unexpected extra MD.'

    options = gdal.InfoOptions(format='json', extraMDDomains=['TRE'])
    ret = gdal.Info(ds, options=options)
    assert ret['metadata']['TRE']['BLOCKA'].find('010000001000000000') != -1, \
        'did not get extra MD.'

###############################################################################
# Test allMetadata


def test_gdalinfo_lib_4():

    ds = gdal.Open('../gdrivers/data/gtiff/byte_with_xmp.tif')

    ret = gdal.Info(ds, allMetadata=True, format='json')
    assert 'xml:XMP' in ret['metadata']

###############################################################################
# Test all options


def test_gdalinfo_lib_5():

    ds = gdal.Open('../gdrivers/data/byte.tif')

    ret = gdal.Info(ds, format='json', deserialize=True, computeMinMax=True,
                    reportHistograms=True, reportProj4=True,
                    stats=True, approxStats=True, computeChecksum=True,
                    showGCPs=False, showMetadata=False, showRAT=False,
                    showColorTable=False, listMDD=True, showFileList=False)
    assert 'files' not in ret
    band = ret['bands'][0]
    assert 'computedMin' in band
    assert 'histogram' in band
    assert 'checksum' in band
    assert ret['coordinateSystem']['dataAxisToSRSAxisMapping'] == [1, 2]

    ds = None

    gdal.Unlink('../gdrivers/data/byte.tif.aux.xml')

###############################################################################
# Test command line syntax + dataset as string


def test_gdalinfo_lib_6():

    ret = gdal.Info('../gcore/data/byte.tif', options='-json')
    assert ret['driverShortName'] == 'GTiff', 'wrong value for driverShortName.'
    assert type(ret) == dict

###############################################################################
# Test with unicode strings


def test_gdalinfo_lib_7():

    ret = gdal.Info('../gcore/data/byte.tif'.encode('ascii').decode('ascii'), options='-json'.encode('ascii').decode('ascii'))
    assert ret['driverShortName'] == 'GTiff', 'wrong value for driverShortName.'
    assert type(ret) == dict

###############################################################################
# Test with list of strings


def test_gdalinfo_lib_8():

    ret = gdal.Info('../gcore/data/byte.tif', options=['-json'])
    assert ret['driverShortName'] == 'GTiff', 'wrong value for driverShortName.'
    assert type(ret) == dict

###############################################################################


def test_gdalinfo_lib_nodatavalues():

    ds = gdal.Translate('', '../gcore/data/byte.tif', options='-of VRT -b 1 -b 1 -b 1 -mo "NODATA_VALUES=0 1 2"')
    ret = gdal.Info(ds)
    assert 'PER_DATASET NODATA' in ret, 'wrong value for mask flags.'


###############################################################################


def test_gdalinfo_lib_coordinate_epoch():

    ds = gdal.Translate('', '../gcore/data/byte.tif', options='-of MEM -a_coord_epoch 2021.3"')
    ret = gdal.Info(ds)
    assert 'Coordinate epoch: 2021.3' in ret

    ret = gdal.Info(ds, format = 'json')
    assert 'coordinateEpoch' in ret
    assert ret['coordinateEpoch'] == 2021.3
