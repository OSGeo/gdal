#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RRASTER format driver.
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

import pprint
from osgeo import gdal
from osgeo import osr


import gdaltest

###############################################################################
# Perform simple read test.


def test_rraster_1(filename='data/rraster/byte_rraster1.grd', check_prj=None):

    tst = gdaltest.GDALTest('RRASTER', filename, 1, 4672, filename_absolute=True)
    ref_ds = gdal.Open('data/byte.tif')
    if check_prj is None:
        check_prj = ref_ds.GetProjectionRef()
    tst.testOpen(check_prj=check_prj,
                       check_gt=ref_ds.GetGeoTransform(),
                       check_min=74,
                       check_max=255)

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetDescription() == 'byte'

###############################################################################


def test_rraster_1_copy():

    filename = '/vsimem/rraster/byte_rraster.grd'
    gdal.Translate(filename, 'data/rraster/byte_rraster1.grd', format='RRASTER')
    assert not gdal.VSIStatL(filename + '.aux.xml'), 'did not expect .aux.xml'
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(26711)
    test_rraster_1(filename, check_prj=sr.ExportToWkt())
    gdal.GetDriverByName('RRASTER').Delete(filename)

###############################################################################


def _compare_val(got, expected, key_name, to_print):
    if isinstance(got, list) and isinstance(expected, list):
        if len(got) != len(expected):
            print('Unexpected number of elements for %s. Got %d, expected %d' % (key_name, len(got), len(expected)))
            pprint.pprint(to_print)
            return False
        for i, val in enumerate(got):
            if not _compare_val(val, expected[i], '%s[%d]' % (key_name, i), to_print):
                return False
    elif isinstance(got, dict) and isinstance(expected, dict):
        if not _is_dict_included_in_dict(got, expected, key_name, to_print):
            pprint.pprint(to_print)
            return False
    elif got != expected:
        print('Value for %s is different' % key_name)
        pprint.pprint(got)
        return False
    return True

###############################################################################


def _is_dict_included_in_dict(got, expected, key_name='', to_print=None):
    if to_print is None:
        to_print = got
    for k in expected:
        if k not in got:
            print('Missing %s' % k)
            return False
        if not _compare_val(got[k], expected[k], key_name + '/' + k, to_print):
            return False
    return True

###############################################################################


def test_rraster_rgba(filename='data/rraster/rgba_rraster.grd'):

    ds = gdal.Open(filename)
    info = gdal.Info(ds, computeChecksum=True, format='json')
    expected_info = {
        'bands': [{'band': 1,
                   'block': [2, 1],
                   'checksum': 19,
                   'colorInterpretation': 'Red',
                   'description': 'red'},
                  {'band': 2,
                   'block': [2, 1],
                   'checksum': 27,
                   'colorInterpretation': 'Green',
                   'description': 'green'},
                  {'band': 3,
                   'block': [2, 1],
                   'checksum': 22,
                   'colorInterpretation': 'Blue',
                   'description': 'blue'},
                  {'band': 4,
                   'block': [2, 1],
                   'checksum': 7,
                   'colorInterpretation': 'Alpha',
                   'description': 'alpha'}]
    }
    assert _is_dict_included_in_dict(info, expected_info)

###############################################################################


def test_rraster_rgba_copy():

    filename = '/vsimem/rgba_rraster.grd'

    for creationOptions in [[], ['INTERLEAVE=BIP'], ['INTERLEAVE=BIL'], ['INTERLEAVE=BSQ']]:
        gdal.Translate(filename, 'data/rraster/rgba_rraster.grd', format='RRASTER',
                       creationOptions=creationOptions)
        assert not gdal.VSIStatL(filename + '.aux.xml'), 'did not expect .aux.xml'
        print(creationOptions)
        test_rraster_rgba(filename)
        gdal.GetDriverByName('RRASTER').Delete(filename)


###############################################################################


def test_rraster_ct_rgb(filename='data/rraster/byte_rraster_ct_rgb.grd'):

    ds = gdal.Open(filename)
    info = gdal.Info(ds, format='json')
    expected_info = {
        'bands': [{'band': 1,
                   'colorInterpretation': 'Palette',
                   'colorTable': {'count': 2,
                                  'entries': [[10, 20, 30, 255],
                                              [11, 21, 31, 255]],
                                  'palette': 'RGB'},
                   'type': 'Byte'}]
    }
    assert _is_dict_included_in_dict(info, expected_info)

###############################################################################


def test_rraster_ct_rgb_copy():

    filename = '/vsimem/rraster/byte_rraster_ct_rgb.grd'
    gdal.Translate(filename, 'data/rraster/byte_rraster_ct_rgb.grd', format='RRASTER')
    assert not gdal.VSIStatL(filename + '.aux.xml'), 'did not expect .aux.xml'
    test_rraster_ct_rgb(filename)
    gdal.GetDriverByName('RRASTER').Delete(filename)

###############################################################################


def test_rraster_ct_rgba(filename='data/rraster/byte_rraster_ct_rgba.grd'):

    ds = gdal.Open(filename)
    info = gdal.Info(ds, format='json')
    expected_info = {
        'bands': [{'band': 1,
                   'colorInterpretation': 'Palette',
                   'colorTable': {'count': 2,
                                  'entries': [[10, 20, 30, 0],
                                              [11, 21, 31, 255]],
                                  'palette': 'RGB'},
                   'type': 'Byte'}]
    }
    assert _is_dict_included_in_dict(info, expected_info)

###############################################################################


def test_rraster_ct_rgba_copy():

    filename = '/vsimem/rraster/byte_rraster_ct_rgba.grd'
    gdal.Translate(filename, 'data/rraster/byte_rraster_ct_rgba.grd', format='RRASTER')
    assert not gdal.VSIStatL(filename + '.aux.xml'), 'did not expect .aux.xml'
    test_rraster_ct_rgba(filename)
    gdal.GetDriverByName('RRASTER').Delete(filename)

###############################################################################


def test_rraster_rat(filename='data/rraster/byte_rraster_rat.grd'):

    ds = gdal.Open(filename)
    info = gdal.Info(ds, format='json')
    expected_info = {
        'bands': [{'band': 1,
                   'block': [20, 1],
                   'colorInterpretation': 'Undefined',
                   'metadata': {},
                   'type': 'Byte'}],
        'rat': {'fieldDefn': [{'index': 0,
                               'name': 'ID',
                               'type': 0,
                               'usage': 0},
                              {'index': 1,
                               'name': 'int_field',
                               'type': 0,
                               'usage': 0},
                              {'index': 2,
                               'name': 'numeric_field',
                               'type': 1,
                               'usage': 0},
                              {'index': 3,
                               'name': 'string_field',
                               'type': 2,
                               'usage': 0},
                              {'index': 4,
                               'name': 'red',
                               'type': 0,
                               'usage': 6},
                              {'index': 5,
                               'name': 'green',
                               'type': 0,
                               'usage': 7},
                              {'index': 6,
                               'name': 'blue',
                               'type': 0,
                               'usage': 8},
                              {'index': 7,
                               'name': 'alpha',
                               'type': 0,
                               'usage': 9},
                              {'index': 8,
                               'name': 'pixelcount',
                               'type': 0,
                               'usage': 1},
                              {'index': 9,
                               'name': 'name',
                               'type': 2,
                               'usage': 2}],
                'row': [{'f': [0,
                               10,
                               1.2,
                               'foo',
                               0,
                               2,
                               4,
                               6,
                               8,
                               'baz'],
                         'index': 0},
                        {'f': [1,
                               11,
                               2.3,
                               'bar',
                               1,
                               3,
                               5,
                               7,
                               9,
                               'baw'],
                         'index': 1}]}
    }
    assert _is_dict_included_in_dict(info, expected_info)

###############################################################################


def test_rraster_rat_copy():

    filename = '/vsimem/rraster/byte_rraster_rat.grd'
    gdal.Translate(filename, 'data/rraster/byte_rraster_rat.grd', format='RRASTER')
    assert not gdal.VSIStatL(filename + '.aux.xml'), 'did not expect .aux.xml'
    test_rraster_rat(filename)
    gdal.GetDriverByName('RRASTER').Delete(filename)

###############################################################################


def test_rraster_signedbyte():

    filename = '/vsimem/rraster_signedbyte.grd'
    filename2 = '/vsimem/rraster_signedbyte2.grd'
    gdal.Translate(filename, 'data/rraster/byte_rraster.grd', format='RRASTER',
                   creationOptions=['PIXELTYPE=SIGNEDBYTE'])
    gdal.Translate(filename2, filename, format='RRASTER')

    ds = gdal.Open(filename2)
    assert ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') == 'SIGNEDBYTE'
    assert ds.GetRasterBand(1).GetMinimum() == -124
    ds = None

    gdal.GetDriverByName('RRASTER').Delete(filename)
    gdal.GetDriverByName('RRASTER').Delete(filename2)

###############################################################################


def test_rraster_datatypes():

    filename = '/vsimem/temp.grd'

    for srcfilename in ['../gcore/data/uint16.tif',
                        '../gcore/data/int16.tif',
                        '../gcore/data/uint32.tif',
                        '../gcore/data/int32.tif',
                        '../gcore/data/float32.tif',
                        '../gcore/data/float64.tif']:
        src_ds = gdal.Open(srcfilename)
        gdal.Translate(filename, src_ds, format='RRASTER')
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).DataType == src_ds.GetRasterBand(1).DataType, \
            srcfilename
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum(), \
            srcfilename

    gdal.GetDriverByName('RRASTER').Delete(filename)

###############################################################################


def test_rraster_nodata_and_metadata():

    filename = '/vsimem/temp.grd'
    ds = gdal.GetDriverByName('RRASTER').Create(filename, 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(1)
    ds.GetRasterBand(1).SetColorTable(None)
    ds.GetRasterBand(1).SetDefaultRAT(None)
    ds.SetMetadataItem('CREATOR', 'GDAL')
    ds.SetMetadataItem('CREATED', 'Today')
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetMetadata() == {'CREATOR': 'GDAL', 'CREATED': 'Today'}
    assert ds.GetRasterBand(1).GetNoDataValue() == 1
    ds = None

    gdal.GetDriverByName('RRASTER').Delete(filename)

###############################################################################


def test_rraster_update():

    filename = '/vsimem/temp.grd'
    gdal.Translate(filename, 'data/rraster/byte_rraster.grd', format='RRASTER')
    gdal.Open(filename, gdal.GA_Update)
    ds = gdal.Open(filename, gdal.GA_Update)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds.GetRasterBand(1).Fill(0)
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None

    gdal.GetDriverByName('RRASTER').Delete(filename)

###############################################################################


def test_rraster_colorinterpretation():

    filename = '/vsimem/temp.grd'
    ds = gdal.GetDriverByName('RRASTER').Create(filename, 1, 1, 4)
    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None

    gdal.GetDriverByName('RRASTER').Delete(filename)




