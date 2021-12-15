#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalinfo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
import json
import shutil


from osgeo import gdal
import gdaltest
import test_cli_utilities
import pytest

###############################################################################
# Simple test


def test_gdalinfo_1():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif', encoding = 'UTF-8')
    assert (err is None or err == ''), 'got error/warning'
    assert ret.find('Driver: GTiff/GeoTIFF') != -1

###############################################################################
# Test -checksum option


def test_gdalinfo_2():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -checksum ../gcore/data/byte.tif')
    assert ret.find('Checksum=4672') != -1

###############################################################################
# Test -nomd option


def test_gdalinfo_3():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif')
    assert ret.find('Metadata') != -1

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -nomd ../gcore/data/byte.tif')
    assert ret.find('Metadata') == -1

###############################################################################
# Test -noct option


def test_gdalinfo_4():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/gif/bug407.gif')
    assert ret.find('0: 255,255,255,255') != -1

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -noct ../gdrivers/data/gif/bug407.gif')
    assert ret.find('0: 255,255,255,255') == -1

###############################################################################
# Test -stats option


def test_gdalinfo_5():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    tmpfilename = 'tmp/test_gdalinfo_5.tif'
    if os.path.exists(tmpfilename + '.aux.xml'):
        os.remove(tmpfilename + '.aux.xml')
    shutil.copy('../gcore/data/byte.tif', tmpfilename)

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ' + tmpfilename)
    assert ret.find('STATISTICS_MINIMUM=74') == -1, 'got wrong minimum.'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -stats ' + tmpfilename)
    assert ret.find('STATISTICS_MINIMUM=74') != -1, 'got wrong minimum (2).'

    # We will blow an exception if the file does not exist now!
    os.remove(tmpfilename + '.aux.xml')
    os.remove(tmpfilename)

###############################################################################
# Test a dataset with overviews and RAT


def test_gdalinfo_6():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/hfa/int.img')
    assert ret.find('Overviews') != -1
    assert ret.find('GDALRasterAttributeTable') != -1

###############################################################################
# Test a dataset with GCPs


def test_gdalinfo_7():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -wkt_format WKT1 ../gcore/data/gcps.vrt')
    assert ret.find('GCP Projection =') != -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') != -1
    assert ret.find('(100,100) -> (446720,3745320,0)') != -1

    # Same but with -nogcps
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -wkt_format WKT1 -nogcp ../gcore/data/gcps.vrt')
    assert ret.find('GCP Projection =') == -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') == -1
    assert ret.find('(100,100) -> (446720,3745320,0)') == -1

###############################################################################
# Test -hist option


def test_gdalinfo_8():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    tmpfilename = 'tmp/test_gdalinfo_8.tif'
    if os.path.exists(tmpfilename + '.aux.xml'):
        os.remove(tmpfilename + '.aux.xml')
    shutil.copy('../gcore/data/byte.tif', tmpfilename)

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ' + tmpfilename)
    assert ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') == -1, \
        'did not expect histogram.'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -hist ' + tmpfilename)
    assert ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') != -1, \
        'did not get expected histogram.'

    # We will blow an exception if the file does not exist now!
    os.remove(tmpfilename + '.aux.xml')
    os.remove(tmpfilename)

###############################################################################
# Test -mdd option


def test_gdalinfo_9():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/nitf/fake_nsif.ntf')
    assert ret.find('BLOCKA=010000001000000000') == -1, 'got unexpected extra MD.'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -mdd TRE ../gdrivers/data/nitf/fake_nsif.ntf')
    assert ret.find('BLOCKA=010000001000000000') != -1, 'did not get extra MD.'

###############################################################################
# Test -mm option


def test_gdalinfo_10():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif')
    assert ret.find('Computed Min/Max=74.000,255.000') == -1

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -mm ../gcore/data/byte.tif')
    assert ret.find('Computed Min/Max=74.000,255.000') != -1

###############################################################################
# Test gdalinfo --version


def test_gdalinfo_11():

    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --version', check_memleak=False)
    assert ret.startswith(gdal.VersionInfo('--version'))

###############################################################################
# Test gdalinfo --build


def test_gdalinfo_12():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --build', check_memleak=False)
    ret = ret.replace('\r\n', '\n')
    assert ret.startswith(gdal.VersionInfo('BUILD_INFO'))

###############################################################################
# Test gdalinfo --license


def test_gdalinfo_13():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --license', check_memleak=False)
    ret = ret.replace('\r\n', '\n')
    if not ret.startswith(gdal.VersionInfo('LICENSE')):
        print(gdal.VersionInfo('LICENSE'))
        if gdaltest.is_travis_branch('mingw'):
            return 'expected_fail'
        pytest.fail(ret)


###############################################################################
# Test erroneous use of --config.


def test_gdalinfo_14():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --config', check_memleak=False)
    assert '--config option given without a key and value argument' in err

###############################################################################
# Test erroneous use of --mempreload.


def test_gdalinfo_15():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --mempreload', check_memleak=False)
    assert '--mempreload option given without directory path' in err

###############################################################################
# Test --mempreload


def test_gdalinfo_16():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (ret, _) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --debug on --mempreload ../gcore/data /vsimem/byte.tif', check_memleak=False, encoding = 'UTF-8')
    assert ret.startswith('Driver: GTiff/GeoTIFF')

###############################################################################
# Test erroneous use of --debug.


def test_gdalinfo_17():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --debug', check_memleak=False)
    assert '--debug option given without debug level' in err

###############################################################################
# Test erroneous use of --optfile.


def test_gdalinfo_18():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --optfile', check_memleak=False)
    assert '--optfile option given without filename' in err

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --optfile /foo/bar', check_memleak=False)
    assert 'Unable to open optfile' in err

###############################################################################
# Test --optfile


def test_gdalinfo_19():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    f = open('tmp/optfile.txt', 'wt')
    f.write('# comment\n')
    f.write('../gcore/data/byte.tif\n')
    f.close()
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --optfile tmp/optfile.txt', check_memleak=False)
    os.unlink('tmp/optfile.txt')
    assert ret.startswith('Driver: GTiff/GeoTIFF')

###############################################################################
# Test --formats


def test_gdalinfo_20():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --formats', check_memleak=False)
    assert 'GTiff -raster- (rw+vs): GeoTIFF' in ret

###############################################################################
# Test erroneous use of --format.


def test_gdalinfo_21():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --format', check_memleak=False)
    assert '--format option given without a format code' in err

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --format foo_bar', check_memleak=False)
    assert '--format option given with format' in err

###############################################################################
# Test --format


def test_gdalinfo_22():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --format GTiff', check_memleak=False)

    expected_strings = [
        'Short Name:',
        'Long Name:',
        'Extensions:',
        'Mime Type:',
        'Help Topic:',
        'Supports: Create()',
        'Supports: CreateCopy()',
        'Supports: Virtual IO',
        'Creation Datatypes',
        '<CreationOptionList>']
    for expected_string in expected_strings:
        assert expected_string in ret, ('did not find %s' % expected_string)


###############################################################################
# Test --help-general


def test_gdalinfo_23():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --help-general', check_memleak=False)
    assert 'Generic GDAL utility command options' in ret

###############################################################################
# Test --locale


def test_gdalinfo_24():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --locale C ../gcore/data/byte.tif', check_memleak=False)
    assert ret.startswith('Driver: GTiff/GeoTIFF')

###############################################################################
# Test -listmdd


def test_gdalinfo_25():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/gtiff/byte_with_xmp.tif -listmdd', check_memleak=False)
    assert 'Metadata domains:' in ret
    assert '  xml:XMP' in ret

###############################################################################
# Test -mdd all


def test_gdalinfo_26():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/gtiff/byte_with_xmp.tif -mdd all', check_memleak=False)
    assert 'Metadata (xml:XMP)' in ret

###############################################################################
# Test -oo


def test_gdalinfo_27():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/aaigrid/float64.asc -oo datatype=float64', check_memleak=False)
    assert 'Type=Float64' in ret

###############################################################################
# Simple -json test


def test_gdalinfo_28():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -json ../gcore/data/byte.tif', encoding = 'UTF-8')
    ret = json.loads(ret)
    assert (err is None or err == ''), 'got error/warning'
    assert ret['driverShortName'] == 'GTiff'

###############################################################################
# Test -json -checksum option


def test_gdalinfo_29():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -checksum ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert ret['bands'][0]['checksum'] == 4672

###############################################################################
# Test -json -nomd option


def test_gdalinfo_30():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert 'metadata' in ret

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -nomd ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert 'metadata' not in ret

###############################################################################
# Test -json -noct option


def test_gdalinfo_31():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gdrivers/data/gif/bug407.gif')
    ret = json.loads(ret)
    assert ret['bands'][0]['colorTable']['entries'][0] == [255, 255, 255, 255]

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -noct ../gdrivers/data/gif/bug407.gif')
    ret = json.loads(ret)
    assert 'colorTable' not in ret['bands'][0]

###############################################################################
# Test -stats option


def test_gdalinfo_32():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    try:
        os.remove('../gcore/data/byte.tif.aux.xml')
    except OSError:
        pass

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert '' not in ret['bands'][0]['metadata'], 'got wrong minimum.'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -stats ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert ret['bands'][0]['metadata']['']['STATISTICS_MINIMUM'] == '74', \
        'got wrong minimum (2).'

    # We will blow an exception if the file does not exist now!
    os.remove('../gcore/data/byte.tif.aux.xml')

###############################################################################
# Test a dataset with overviews and RAT


def test_gdalinfo_33():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gdrivers/data/hfa/int.img')
    ret = json.loads(ret)
    assert 'overviews' in ret['bands'][0]
    assert 'rat' in ret

###############################################################################
# Test a dataset with GCPs


def test_gdalinfo_34():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gcore/data/gcps.vrt')
    ret = json.loads(ret)
    assert 'wkt' in ret['gcps']['coordinateSystem']
    assert ret['gcps']['coordinateSystem']['wkt'].find('PROJCRS["NAD27 / UTM zone 11N"') != -1
    assert ret['gcps']['gcpList'][0]['x'] == 440720.0

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -nogcp ../gcore/data/gcps.vrt')
    ret = json.loads(ret)
    assert 'gcps' not in ret

###############################################################################
# Test -hist option


def test_gdalinfo_35():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    try:
        os.remove('../gcore/data/byte.tif.aux.xml')
    except OSError:
        pass

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert 'histogram' not in ret['bands'][0], 'did not expect histogram.'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -hist ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert ret['bands'][0]['histogram']['buckets'] == [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 37, 0, 0, 0, 0, 0, 0, 0, 57, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 0, 0, 0, 0, 0, 0, 72, 0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 0, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1], \
        'did not get expected histogram.'

    # We will blow an exception if the file does not exist now!
    os.remove('../gcore/data/byte.tif.aux.xml')

###############################################################################
# Test -mdd option


def test_gdalinfo_36():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gdrivers/data/nitf/fake_nsif.ntf')
    ret = json.loads(ret)
    assert 'TRE' not in ret['metadata'], 'got unexpected extra MD.'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -mdd TRE ../gdrivers/data/nitf/fake_nsif.ntf')
    ret = json.loads(ret)
    assert ret['metadata']['TRE']['BLOCKA'].find('010000001000000000') != -1, \
        'did not get extra MD.'

###############################################################################
# Test -mm option


def test_gdalinfo_37():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert 'computedMin' not in ret['bands'][0]

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json -mm ../gcore/data/byte.tif')
    ret = json.loads(ret)
    assert ret['bands'][0]['computedMin'] == 74.000

###############################################################################
# Test -listmdd


def test_gdalinfo_38():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gdrivers/data/gtiff/byte_with_xmp.tif -listmdd', check_memleak=False)
    ret = json.loads(ret)
    assert 'metadataDomains' in ret['metadata']
    assert ret['metadata']['metadataDomains'][0] == 'xml:XMP'

###############################################################################
# Test -mdd all


def test_gdalinfo_39():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gdrivers/data/gtiff/byte_with_xmp.tif -mdd all', check_memleak=False)
    ret = json.loads(ret)
    assert 'xml:XMP' in ret['metadata']

###############################################################################
# Test -json wgs84Extent


def test_gdalinfo_40():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -json ../gdrivers/data/small_world.tif')
    ret = json.loads(ret)
    assert 'wgs84Extent' in ret
    assert 'type' in ret['wgs84Extent']
    assert ret['wgs84Extent']['type'] == 'Polygon'
    assert 'coordinates' in ret['wgs84Extent']
    assert ret['wgs84Extent']['coordinates'] == [[[-180.0, 90.0], [-180.0, -90.0], [180.0, -90.0], [180.0, 90.0], [-180.0, 90.0]]]


###############################################################################
# Test -if option


def test_gdalinfo_if_option():
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -if GTiff ../gcore/data/byte.tif', encoding = 'UTF-8')
    assert (err is None or err == ''), 'got error/warning'
    assert ret.find('Driver: GTiff/GeoTIFF') != -1

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -if invalid_driver_name ../gcore/data/byte.tif', encoding = 'UTF-8')
    assert err is not None
    assert 'invalid_driver_name' in err

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -if HFA ../gcore/data/byte.tif', encoding = 'UTF-8')
    assert err is not None
