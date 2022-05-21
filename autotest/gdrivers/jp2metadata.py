#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JP2 metadata support.
# Author:   Even Rouault < even dot rouault @ spatialys.com >
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal

import gdaltest
import pytest


###############################################################################
# Test bugfix for #5249 (Irrelevant ERDAS GeoTIFF JP2Box read)

def test_jp2metadata_1():

    ds = gdal.Open('data/jpeg2000/erdas_foo.jp2')
    if ds is None:
        pytest.skip()

    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    assert wkt.startswith('PROJCS["ETRS89')
    expected_gt = (356000.0, 0.5, 0.0, 7596000.0, 0.0, -0.5)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-5)

###############################################################################
# Test Pleiades & Pleiades Neo imagery metadata


def _test_jp2metadata(file_path, rpc_values_to_check=None):
    try:
        os.remove(f'{file_path}.aux.xml')
    except OSError:
        pass

    ds = gdal.Open(file_path, gdal.GA_ReadOnly)
    if ds is None:
        pytest.skip()

    filelist = ds.GetFileList()

    assert len(filelist) == 3, filelist

    mddlist = ds.GetMetadataDomainList()
    assert 'IMD' in mddlist and 'RPC' in mddlist and 'IMAGERY' in mddlist, \
        'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # RPC validity
    md_rpc = ds.GetMetadata('RPC')
    keys_rpc = set(md_rpc.keys())

    mandatory_keys_rpc = {'HEIGHT_OFF', 'HEIGHT_SCALE', 'LAT_OFF', 'LAT_SCALE',
                          'LINE_DEN_COEFF', 'LINE_NUM_COEFF', 'LINE_OFF',
                          'LINE_SCALE', 'LONG_OFF', 'LONG_SCALE',
                          'SAMP_DEN_COEFF', 'SAMP_NUM_COEFF', 'SAMP_OFF',
                          'SAMP_SCALE'}

    diff = mandatory_keys_rpc.difference(keys_rpc)
    diff = [str(d) for d in diff]
    if diff:
        pytest.fail(f'mandatory key.s missing : {", ".join(diff)}')

    empty_keys = []
    for k, v in md_rpc.items():
        if not v:
            empty_keys.append(k)
    if empty_keys:
        pytest.fail(f'empty key.s : {", ".join(empty_keys)}')

    if rpc_values_to_check is not None:
        for k, v in rpc_values_to_check.items():
            if md_rpc[k] != v:
                pytest.fail(f'the value of the RPC key : {k} is not valid')

    ds = None

    assert not os.path.exists(f'{file_path}.aux.xml')


def test_jp2metadata_2():
    # Pleiades product description https://content.satimagingcorp.com/media/pdf/User_Guide_Pleiades.pdf
    file_path = 'data/jpeg2000/IMG_md_ple_R1C1.jp2'
    rpc_values_to_check = {
        'LAT_OFF': '-37.8185709405155',
        'SAMP_OFF': '5187',
        'LAT_SCALE': '0.056013496012568',
        'LONG_SCALE': '0.1152662335048689',
        'LINE_DEN_COEFF': ' 1 0.0001616419179600887 -0.0003138230500963576 -1.394071985006734e-06 -6.696094164696539e-06 -5.345869412075188e-09 1.763447020374064e-08 1.570099327763788e-05 -2.742185667248469e-05 2.311214210508507e-05 -1.355093965247957e-10 2.888456971707225e-08 8.124756826520027e-07 7.468753872581231e-09 5.656409063390933e-07 8.695797176083266e-06 -3.572353935073523e-09 -8.051282577435379e-11 1.691147472316222e-08 -6.436246171675777e-11',
        'SAMP_NUM_COEFF': ' 0.0002609410706716954 1.001026213740965 -0.0003819289116566809 0.0001240788067018346 0.0005862035015589599 5.081629489519709e-05 -1.435215968291365e-05 -0.0002758832786884909 0.0001043228128012142 -1.375374301980545e-08 5.424360099410591e-08 -5.026010178171814e-05 0.0001886885841229406 -6.535315557200323e-05 3.723625930897949e-05 0.000324332729058834 9.492897372587203e-09 -6.383348194827217e-09 -3.519296777850624e-08 -8.099247649030343e-09',
    }
    _test_jp2metadata(file_path, rpc_values_to_check=rpc_values_to_check)


def test_jp2metadata_2b():
    # Pleiades Neo product
    file_path = 'data/jpeg2000/IMG_md_pneo_R1C1.jp2'
    rpc_values_to_check = {
        'LAT_OFF': '12.807914369557892',
        'SAMP_OFF': '5864',
        'LAT_SCALE': '0.06544078829767308',
        'LONG_SCALE': '0.06456467877685057',
        'LINE_DEN_COEFF': ' 1 0.00101120491477 0.00352363792876 1.54038387462e-06 -5.25674523513e-06 -5.82954187807e-08 -4.38661879766e-07 3.65133101845e-06 -1.87290332218e-06 5.73319333615e-06 1.04740906969e-09 -2.80668071974e-07 2.96747022687e-07 1.08930762307e-08 -7.17196535598e-08 3.34275572452e-07 -4.49546103468e-09 -2.39590665536e-08 2.69120818927e-07 1.87360972277e-10',
        'SAMP_NUM_COEFF': ' 0.0225143172142 1.00589741678 -0.00134174726147 0.0237253142511 5.88157266883e-05 0.00269050817565 -0.00136618510734 -0.0116423108131 0.00100142882811 6.72967252237e-05 -1.21136997927e-06 -0.000921827179509 -0.00020078717226 -3.50960323581e-06 -0.000207253349484 6.15117373574e-05 -3.45619374615e-06 -0.000130177039513 -9.67483269543e-06 -3.52598626454e-08',
    }
    _test_jp2metadata(file_path, rpc_values_to_check=rpc_values_to_check)


###############################################################################
# Test reading GMLJP2 file with srsName only on the Envelope, and lots of other
# metadata junk.  This file is also handled currently with axis reordering
# disabled.


def test_jp2metadata_3():

    gdal.SetConfigOption('GDAL_IGNORE_AXIS_ORIENTATION', 'YES')

    exp_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'

    ds = gdal.Open('data/jpeg2000/ll.jp2')
    if ds is None:
        gdal.SetConfigOption('GDAL_IGNORE_AXIS_ORIENTATION', 'NO')
        pytest.skip()
    wkt = ds.GetProjection()

    if wkt != exp_wkt:
        print('got: ', wkt)
        print('exp: ', exp_wkt)
        pytest.fail('did not get expected WKT, should be WGS84')

    gt = ds.GetGeoTransform()
    if gt[0] != pytest.approx(8, abs=0.0000001) or gt[3] != pytest.approx(50, abs=0.000001) \
       or gt[1] != pytest.approx(0.000761397164, abs=0.000000000005) \
       or gt[2] != pytest.approx(0.0, abs=0.000000000005) \
       or gt[4] != pytest.approx(0.0, abs=0.000000000005) \
       or gt[5] != pytest.approx(-0.000761397164, abs=0.000000000005):
        print('got: ', gt)
        pytest.fail('did not get expected geotransform')

    ds = None

    gdal.SetConfigOption('GDAL_IGNORE_AXIS_ORIENTATION', 'NO')

###############################################################################
# Test reading a file with axis orientation set properly for an alternate
# axis order coordinate system (urn:...:EPSG::4326).


def test_jp2metadata_4():

    exp_wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'

    ds = gdal.Open('data/jpeg2000/gmljp2_dtedsm_epsg_4326_axes.jp2')
    if ds is None:
        pytest.skip()
    wkt = ds.GetProjection()

    if wkt != exp_wkt:
        print('got: ', wkt)
        print('exp: ', exp_wkt)
        pytest.fail('did not get expected WKT, should be WGS84')

    gt = ds.GetGeoTransform()
    gte = (42.999583333333369, 0.008271349862259, 0,
           34.000416666666631, 0, -0.008271349862259)

    if gt[0] != pytest.approx(gte[0], abs=0.0000001) or gt[3] != pytest.approx(gte[3], abs=0.000001) \
       or gt[1] != pytest.approx(gte[1], abs=0.000000000005) \
       or gt[2] != pytest.approx(gte[2], abs=0.000000000005) \
       or gt[4] != pytest.approx(gte[4], abs=0.000000000005) \
       or gt[5] != pytest.approx(gte[5], abs=0.000000000005):
        print('got: ', gt)
        pytest.fail('did not get expected geotransform')

    ds = None

###############################################################################
# Test reading a file with EPSG axis orientation being northing, easting,
# but with explicit axisName being easting, northing (#5960)


def test_jp2metadata_5():

    ds = gdal.Open('data/jpeg2000/gmljp2_epsg3035_easting_northing.jp2')
    if ds is None:
        pytest.skip()

    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == '3035'

    gt = ds.GetGeoTransform()
    gte = (4895766.000000001, 2.0, 0.0, 2296946.0, 0.0, -2.0)

    if gt[0] != pytest.approx(gte[0], abs=0.0000001) or gt[3] != pytest.approx(gte[3], abs=0.000001) \
       or gt[1] != pytest.approx(gte[1], abs=0.000000000005) \
       or gt[2] != pytest.approx(gte[2], abs=0.000000000005) \
       or gt[4] != pytest.approx(gte[4], abs=0.000000000005) \
       or gt[5] != pytest.approx(gte[5], abs=0.000000000005):
        print('got: ', gt)
        pytest.fail('did not get expected geotransform')

    ds = None

###############################################################################
# Get structure of a JPEG2000 file


def test_jp2metadata_getjpeg2000structure():

    ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte.jp2', ['ALL=YES'])
    assert ret is not None

    ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte_tlm_plt.jp2', ['ALL=YES'])
    assert ret is not None

    ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte_one_poc.j2k', ['ALL=YES'])
    assert ret is not None

    with gdaltest.config_option('GDAL_JPEG2000_STRUCTURE_MAX_LINES', '15'):
        gdal.ErrorReset()
        with gdaltest.error_handler():
            ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte.jp2', ['ALL=YES'])
        assert ret is not None
        assert gdal.GetLastErrorMsg() != ''

    with gdaltest.config_option('GDAL_JPEG2000_STRUCTURE_MAX_LINES', '150'):
        gdal.ErrorReset()
        with gdaltest.error_handler():
            ret = gdal.GetJPEG2000StructureAsString('data/jpeg2000/byte.jp2', ['ALL=YES'])
        assert ret is not None
        assert gdal.GetLastErrorMsg() != ''
