#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PDS4 format
# Author:   Even Rouault, <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Hobu Inc
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

import contextlib

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest
import pytest

###############################################################################
# Validate XML file against schemas


def validate_xml(filename):

    if ogr.GetDriverByName('GMLAS') is None:
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1D00.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1D00.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1D00_1933.xsd',
                                  'pds.nasa.gov_pds4_cart_v1_PDS4_CART_1D00_1933.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/disp/v1/PDS4_DISP_1B00.xsd',
                                  'pds.nasa.gov_pds4_disp_v1_PDS4_DISP_1B00.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1B00.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1B00.xsd',
                                  force_download=True):
        pytest.skip()


    # Needed by PDS4_CART_1D00_1933
    if not gdaltest.download_file('https://pds.nasa.gov/pds4/geom/v1/PDS4_GEOM_1B10_1700.xsd',
                                  'pds.nasa.gov_pds4_geom_v1_PDS4_GEOM_1B10_1700.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1B10.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1B10.xsd',
                                  force_download=True):
        pytest.skip()

    # Older schemas
    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1800.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1700.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1700.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.xsd',
                                  'pds.nasa.gov_pds4_cart_v1_PDS4_CART_1700.xsd',
                                  force_download=True):
        pytest.skip()

    ds = gdal.OpenEx('GMLAS:' + filename, open_options=[
        'VALIDATE=YES',
        'FAIL_IF_VALIDATION_ERROR=YES',
        'CONFIG_FILE=<Configuration><AllowRemoteSchemaDownload>false</AllowRemoteSchemaDownload><SchemaCache><Directory>tmp/cache</Directory></SchemaCache></Configuration>'])
    return ds is not None


###############################################################################
# hide_substitution_warnings_error_handler()


def hide_substitution_warnings_error_handler_cbk(typ, errno, msg):
    # pylint: disable=unused-argument
    if 'substituted' not in msg and 'VAR_TITLE not defined' not in msg:
        print(msg)


@contextlib.contextmanager
def hide_substitution_warnings_error_handler():
    handler = gdal.PushErrorHandler(hide_substitution_warnings_error_handler_cbk)
    try:
        yield handler
    finally:
        gdal.PopErrorHandler()



def test_ogr_pds4_read_table_character():

    ds = gdal.OpenEx('data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.xml')
    assert ds
    assert ds.GetLayerCount() == 1
    fl = ds.GetFileList()
    assert len(fl) == 2, fl
    assert 'ele_evt_12hr_orbit_2011-2012_truncated.xml' in fl[0]
    assert 'ele_evt_12hr_orbit_2011-2012_truncated.tab' in fl[1]
    assert not ds.GetLayer(-1)
    assert not ds.GetLayer(1)
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'ele_evt_12hr_orbit_2011-2012_truncated'
    assert lyr.GetFeatureCount() == 5

    f = lyr.GetNextFeature()
    assert f.GetFieldCount() == 19
    assert f.GetFID() == 1
    assert f['Event Number'] == 1.0
    assert f.GetGeometryRef().ExportToIsoWkt() == 'POINT Z (224.8604431 28.6008358 408.5436707)'

    f = lyr.GetNextFeature()
    assert f.GetFID() == 2
    while True:
        f = lyr.GetNextFeature()
        if f is None:
            break
        last_fid = f.GetFID()
    assert last_fid == 5
    assert not lyr.GetNextFeature()
    assert not lyr.GetFeature(0)
    assert not lyr.GetFeature(6)
    f = lyr.GetFeature(1)
    assert f.GetFID() == 1
    assert f['Event Number'] == 1.0
    assert f['BP_LOW'] == 102.4400024
    f = lyr.GetFeature(5)
    assert f.GetFID() == 5
    assert f['Event Number'] == 1.0
    assert f['BP_LOW'] == 102.9400024


def test_ogr_pds4_read_table_character_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                               ' -ro data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.xml')
    assert 'INFO' in ret and 'ERROR' not in ret


def test_ogr_pds4_append_and_modify_table_character():

    gdal.FileFromMemBuffer('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml',
                           open('data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.xml', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.tab',
                           open('data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.tab', 'rb').read())

    ds = ogr.Open('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml', update = 1)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCSequentialWrite)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['Event Number'] = 123456
    assert lyr.CreateFeature(f) == 0
    assert f.GetFID() == 6
    assert lyr.GetFeatureCount() == 6
    f = lyr.GetFeature(6)
    assert f['Event Number'] == 123456
    assert not f.IsFieldSet('MET')
    ds = None

    assert validate_xml('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml')

    # Re-open
    ds = ogr.Open('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml', update = 1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 6
    f = lyr.GetFeature(6)
    assert f['Event Number'] == 123456
    assert f.GetGeometryRef() is None
    ds = None

    ogr.GetDriverByName('PDS4').DeleteDataSource('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml')


def test_ogr_pds4_delete_from_table_character():

    gdal.FileFromMemBuffer('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml',
                           open('data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.xml', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.tab',
                           open('data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.tab', 'rb').read())

    ds = ogr.Open('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml', update = 1)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCDeleteFeature)
    assert lyr.DeleteFeature(2) == 0
    assert lyr.GetFeatureCount() == 4
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert '<name>Energetic Electron events, 12 hour orbit, 2011-2012</name>' in data
    assert '<description>Target-centric latitude of the spacecraft' in data
    assert '<description>EE event number. The value is repeated for' in data
    assert '<Special_Constants>' in data

    assert validate_xml('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml')

    # Re-open
    ds = ogr.Open('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml', update = 1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4

    f = lyr.GetNextFeature()
    assert f.GetFieldCount() == 19
    assert f.GetFID() == 1
    assert f['Event Number'] == 1.0
    assert f.GetGeometryRef().ExportToIsoWkt() == 'POINT Z (224.8604431 28.6008358 408.5436707)'

    f = lyr.GetFeature(4)
    assert f['BP_LOW'] == 102.9400024
    ds = None

    ogr.GetDriverByName('PDS4').DeleteDataSource('/vsimem/ele_evt_12hr_orbit_2011-2012_truncated.xml')


def test_ogr_pds4_read_write_table_character_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    open('tmp/ele_evt_12hr_orbit_2011-2012_truncated.xml', 'wb').write(
        open('data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.xml', 'rb').read())
    open('tmp/ele_evt_12hr_orbit_2011-2012_truncated.tab', 'wb').write(
        open('data/pds4/ele_evt_12hr_orbit_2011-2012_truncated.tab', 'rb').read())

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                               ' tmp/ele_evt_12hr_orbit_2011-2012_truncated.xml')

    gdal.Unlink('tmp/ele_evt_12hr_orbit_2011-2012_truncated.xml')
    gdal.Unlink('tmp/ele_evt_12hr_orbit_2011-2012_truncated.tab')
    assert 'INFO' in ret and 'ERROR' not in ret, ret


@pytest.mark.parametrize('line_ending', [None, 'CRLF', 'LF', 'error'])
def test_ogr_pds4_create_table_character(line_ending):

    options = ['VAR_LOGICAL_IDENTIFIER=logical_identifier',
               'VAR_TITLE=title',
               'VAR_INVESTIGATION_AREA_NAME=ian',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=INVESTIGATION_AREA_LID_REFERENCE',
               'VAR_OBSERVING_SYSTEM_NAME=osn',
               'VAR_TARGET=target',
               'VAR_TARGET_TYPE=target']

    ds = ogr.GetDriverByName('PDS4').CreateDataSource('/vsimem/test.xml',
                                                      options=options)

    layer_creation_options = ['TABLE_TYPE=CHARACTER']
    if line_ending:
        layer_creation_options.append('LINE_ENDING=' + line_ending)
    if line_ending == 'error':
        with gdaltest.error_handler():
            lyr = ds.CreateLayer('foo', options=layer_creation_options)
    else:
        lyr = ds.CreateLayer('foo', options=layer_creation_options)
    fld = ogr.FieldDefn('bool', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('int64', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('real', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('datetime', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('date', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('time', ogr.OFTTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['bool'] = 1
    f['int'] = -123456789
    f['int64'] = -1234567890123
    f['real'] = 1.25
    f['str'] = 'foo'
    f['datetime'] = '2019/01/24 12:34:56.789+00'
    f['date'] = '2019-01-24'
    f['time'] = '12:34:56.789'
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/test.xml', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert '_Character' in data
    assert '_Binary' not in data
    if line_ending == 'LF':
        assert '<record_delimiter>Line-Feed</record_delimiter>' in data
    else:
        assert '<record_delimiter>Carriage-Return Line-Feed</record_delimiter>' in data
    assert 'LSB' not in data
    assert 'MSB' not in data

    if line_ending is None:
        # Only do that check in that configuration for faster test execution
        assert validate_xml('/vsimem/test.xml')

    assert gdal.VSIStatL('/vsimem/test/foo.dat')

    f = gdal.VSIFOpenL('/vsimem/test/foo.dat', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    if line_ending == 'LF':
        assert '\n' in data
        assert '\r\n' not in data
    else:
        assert '\r\n' in data

    ds = ogr.Open('/vsimem/test.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 8
    f = lyr.GetNextFeature()
    assert f['bool']
    assert f['int'] == -123456789
    assert f['int64'] == -1234567890123
    assert f['real'] == 1.25
    assert f['str'] == 'foo'
    assert f['datetime'] == '2019/01/24 12:34:56.789+00'
    assert f['date'] == '2019/01/24'
    assert f['time'] == '12:34:56.789'
    ds = None

    if line_ending is None:
        # Only do that part in that configuration for faster test execution

        # Add new layer
        ds = ogr.Open('/vsimem/test.xml', update = 1)
        lyr = ds.CreateLayer('bar', options=['TABLE_TYPE=CHARACTER'])
        lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['int'] = 123
        lyr.CreateFeature(f)
        ds = None

        assert validate_xml('/vsimem/test.xml')

        ds = ogr.Open('/vsimem/test.xml')
        lyr = ds.GetLayerByName('bar')
        f = lyr.GetNextFeature()
        assert f['int'] == 123

        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f['int'] == -123456789

        ds = None

    ogr.GetDriverByName('PDS4').DeleteDataSource('/vsimem/test.xml')
    gdal.Rmdir('/vsimem/test')


def test_ogr_pds4_create_with_srs():

    ds = ogr.GetDriverByName('PDS4').CreateDataSource('/vsimem/test.xml')
    sr = osr.SpatialReference()
    sr.SetFromUserInput('WGS84')
    lyr = ds.CreateLayer('bar', geom_type = ogr.wkbPoint25D, srs = sr,
                         options=['TABLE_TYPE=CHARACTER', 'SAME_DIRECTORY=YES'])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT Z (1 2 3)'))
    lyr.CreateFeature(f)
    ds = None

    assert validate_xml('/vsimem/test.xml')

    assert gdal.VSIStatL('/vsimem/bar.dat')

    f = gdal.VSIFOpenL('/vsimem/test.xml', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    assert '<local_identifier_reference>bar</local_identifier_reference>' in data
    assert '<local_identifier>bar</local_identifier>' in data

    ds = ogr.Open('/vsimem/test.xml')
    lyr = ds.GetLayerByName('bar')
    assert lyr.GetSpatialRef()
    assert lyr.GetSpatialRef().IsGeographic()
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == 'POINT Z (1 2 3)'
    ds = None

    ogr.GetDriverByName('PDS4').DeleteDataSource('/vsimem/test.xml')


def test_ogr_pds4_create_table_binary():

    options = ['VAR_LOGICAL_IDENTIFIER=logical_identifier',
               'VAR_TITLE=title',
               'VAR_INVESTIGATION_AREA_NAME=ian',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=INVESTIGATION_AREA_LID_REFERENCE',
               'VAR_OBSERVING_SYSTEM_NAME=osn',
               'VAR_TARGET=target',
               'VAR_TARGET_TYPE=target']

    for signedness in ['Signed', 'Unsigned']:
        for endianness in ['LSB', 'MSB']:

            ds = ogr.GetDriverByName('PDS4').CreateDataSource('/vsimem/test.xml',
                                                            options=options)

            layername = endianness
            with gdaltest.config_options( {'PDS4_ENDIANNESS': endianness,
                                           'PDS4_SIGNEDNESS': signedness} ):
                lyr = ds.CreateLayer(layername, options = ['TABLE_TYPE=BINARY'])
                fld = ogr.FieldDefn('bool', ogr.OFTInteger)
                fld.SetSubType(ogr.OFSTBoolean)
                lyr.CreateField(fld)

                fld = ogr.FieldDefn('byte', ogr.OFTInteger)
                fld.SetWidth(2)
                lyr.CreateField(fld)

                fld = ogr.FieldDefn('int16', ogr.OFTInteger)
                fld.SetSubType(ogr.OFSTInt16)
                lyr.CreateField(fld)

                lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
                lyr.CreateField(ogr.FieldDefn('int64', ogr.OFTInteger64))

                fld = ogr.FieldDefn('float', ogr.OFTReal)
                fld.SetSubType(ogr.OFSTFloat32)
                lyr.CreateField(fld)

                lyr.CreateField(ogr.FieldDefn('real', ogr.OFTReal))
                lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
                lyr.CreateField(ogr.FieldDefn('datetime', ogr.OFTDateTime))
                lyr.CreateField(ogr.FieldDefn('date', ogr.OFTDate))
                lyr.CreateField(ogr.FieldDefn('time', ogr.OFTTime))

            sign = -1 if signedness == 'Signed' else 1

            f = ogr.Feature(lyr.GetLayerDefn())
            f['bool'] = 1
            f['byte'] = sign * 9
            f['int16'] = sign * 12345
            f['int'] = sign * 123456789
            f['int64'] = sign * 1234567890123
            f['float'] = 1.25
            f['real'] = 1.2567
            f['str'] = 'foo'
            f['datetime'] = '2019/01/24 12:34:56.789+00'
            f['date'] = '2019-01-24'
            f['time'] = '12:34:56.789'
            lyr.CreateFeature(f)

            ds = None

            f = gdal.VSIFOpenL('/vsimem/test.xml', 'rb')
            data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
            gdal.VSIFCloseL(f)

            assert '_Binary' in data
            assert '_Character' not in data
            assert '<record_delimiter>' not in data

            if endianness == 'LSB':
                assert 'LSB' in data, data
                assert 'MSB' not in data, data
            else:
                assert 'MSB' in data, data
                assert 'LSB' not in data, data

            if signedness == 'Signed':
                assert 'Signed' in data, data
                assert 'Unsigned' not in data, data
            else:
                assert 'Unsigned' in data, data
                assert 'Signed' not in data, data

            assert validate_xml('/vsimem/test.xml')

            ds = ogr.Open('/vsimem/test.xml')
            layername = endianness
            lyr = ds.GetLayerByName(layername)
            assert lyr.GetLayerDefn().GetFieldCount() == 11
            f = lyr.GetNextFeature()
            assert f['bool']
            assert f['byte'] == sign * 9
            assert f['int16'] == sign * 12345
            assert f['int'] == sign * 123456789
            assert f['int64'] == sign * 1234567890123
            assert f['float'] == 1.25
            assert f['real'] == 1.2567
            assert f['str'] == 'foo'
            assert f['datetime'] == '2019/01/24 12:34:56.789+00'
            assert f['date'] == '2019/01/24'
            assert f['time'] == '12:34:56.789'

    ds = None

    # Add new layer
    ds = ogr.Open('/vsimem/test.xml', update = 1)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('WGS84')
    lyr = ds.CreateLayer('bar', geom_type = ogr.wkbPoint25D, srs = sr,
                         options = ['TABLE_TYPE=BINARY'])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT Z (1 2 3)'))
    lyr.CreateFeature(f)
    ds = None

    assert validate_xml('/vsimem/test.xml')

    ds = ogr.Open('/vsimem/test.xml')
    lyr = ds.GetLayerByName('bar')
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == 'POINT Z (1 2 3)'
    ds = None

    ogr.GetDriverByName('PDS4').DeleteDataSource('/vsimem/test.xml')
    gdal.Rmdir('/vsimem/test')


@pytest.mark.parametrize('line_ending', [None, 'CRLF', 'LF', 'error'])
def test_ogr_pds4_create_table_delimited(line_ending):

    options = ['VAR_LOGICAL_IDENTIFIER=logical_identifier',
               'VAR_TITLE=title',
               'VAR_INVESTIGATION_AREA_NAME=ian',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=INVESTIGATION_AREA_LID_REFERENCE',
               'VAR_OBSERVING_SYSTEM_NAME=osn',
               'VAR_TARGET=target',
               'VAR_TARGET_TYPE=target']

    ds = ogr.GetDriverByName('PDS4').CreateDataSource('/vsimem/test.xml',
                                                      options=options)

    layer_creation_options = []
    if line_ending:
        layer_creation_options.append('LINE_ENDING=' + line_ending)
    if line_ending == 'error':
        with gdaltest.error_handler():
            lyr = ds.CreateLayer('foo', options=layer_creation_options)
    else:
        lyr = ds.CreateLayer('foo', options=layer_creation_options)

    fld = ogr.FieldDefn('bool', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('int64', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('real', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('datetime', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('date', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('time', ogr.OFTTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['bool'] = 1
    f['int'] = -123456789
    f['int64'] = -1234567890123
    f['real'] = 1.25
    f['str'] = 'foo'
    f['datetime'] = '2019/01/24 12:34:56.789+00'
    f['date'] = '2019-01-24'
    f['time'] = '12:34:56.789'
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (1 2,3 4)'))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/test.xml', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert '_Character' not in data
    assert '_Binary' not in data
    if line_ending == 'LF':
        assert '<record_delimiter>Line-Feed</record_delimiter>' in data
    else:
        assert '<record_delimiter>Carriage-Return Line-Feed</record_delimiter>' in data
    assert 'LSB' not in data
    assert 'MSB' not in data

    if line_ending is None:
        # Only do that check in that configuration for faster test execution
        assert validate_xml('/vsimem/test.xml')

    ds = gdal.OpenEx('/vsimem/test.xml')
    assert ds
    assert ds.GetLayerCount() == 1
    fl = ds.GetFileList()
    assert len(fl) == 3, fl
    assert 'test.xml' in fl[0]
    assert 'foo.csv' in fl[1]
    assert 'foo.vrt' in fl[2]
    ds= None

    f = gdal.VSIFOpenL('/vsimem/test/foo.csv', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    if line_ending == 'LF':
        assert '\n' in data
        assert '\r\n' not in data
    else:
        assert '\r\n' in data

    for filename in [ '/vsimem/test.xml', '/vsimem/test/foo.vrt' ]:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 8, filename
        f = lyr.GetNextFeature()
        assert f['bool']
        assert f['int'] == -123456789
        assert f['int64'] == -1234567890123
        assert f['real'] == 1.25
        assert f['str'] == 'foo'
        assert f['datetime'] == '2019/01/24 12:34:56.789+00'
        assert f['date'] == '2019/01/24'
        assert f['time'] == '12:34:56.789'
        assert f.GetGeometryRef().ExportToIsoWkt() == 'LINESTRING (1 2,3 4)'
        ds = None

    if line_ending is None:
        # Only do that part in that configuration for faster test execution

        # Add new layer
        ds = ogr.Open('/vsimem/test.xml', update = 1)
        lyr = ds.CreateLayer('no_geom', geom_type = ogr.wkbNone, options=['TABLE_TYPE=DELIMITED'])
        lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['int'] = 123
        lyr.CreateFeature(f)
        ds = None

        assert validate_xml('/vsimem/test.xml')

        ds = ogr.Open('/vsimem/test.xml')
        lyr = ds.GetLayerByName('no_geom')
        f = lyr.GetNextFeature()
        assert f['int'] == 123

        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f['int'] == -123456789

        ds = None

    ogr.GetDriverByName('PDS4').DeleteDataSource('/vsimem/test.xml')
    gdal.Rmdir('/vsimem/test')


def test_ogr_pds4_read_table_binary_group_field():

    ds = ogr.Open('data/pds4/xrs2015091_truncated.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 1 + 231
    f = lyr.GetNextFeature()
    assert f['met'] == 70170476
    assert f['solar_mon_spectrum_23_253_1'] == 0
    assert f['solar_mon_spectrum_23_253_4'] == 12437
    assert f['solar_mon_spectrum_23_253_5'] == 31259


def test_ogr_pds4_create_table_delimited_with_srs_no_vrt():


    options = ['VAR_LOGICAL_IDENTIFIER=logical_identifier',
               'VAR_TITLE=title',
               'VAR_INVESTIGATION_AREA_NAME=ian',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=INVESTIGATION_AREA_LID_REFERENCE',
               'VAR_OBSERVING_SYSTEM_NAME=osn',
               'VAR_TARGET=target',
               'VAR_TARGET_TYPE=target']

    ds = ogr.GetDriverByName('PDS4').CreateDataSource('/vsimem/test.xml',
                                                      options=options)
    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=tmerc +datum=WGS84')
    lyr = ds.CreateLayer('foo', srs=srs, options=['CREATE_VRT=NO'])
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None

    assert validate_xml('/vsimem/test.xml')

    ds = ogr.Open('/vsimem/test.xml')
    lyr = ds.GetLayerByName('foo')
    wkt = lyr.GetSpatialRef().ExportToWkt()
    assert wkt.replace('D_WGS_1984', 'WGS_1984') == 'PROJCS["Transverse Mercator target",GEOGCS["GCS_target",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Metre",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'.replace('D_WGS_1984', 'WGS_1984'), wkt

    ds = None

    ogr.GetDriverByName('PDS4').DeleteDataSource('/vsimem/test.xml')
    gdal.Rmdir('/vsimem/test')


def test_ogr_pds4_read_table_delimited_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    open('tmp/poly_delimited.xml', 'wb').write(
        open('data/pds4/poly_delimited.xml', 'rb').read())
    open('tmp/poly_delimited.csv', 'wb').write(
        open('data/pds4/poly_delimited.csv', 'rb').read())

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                               ' tmp/poly_delimited.xml')

    gdal.Unlink('tmp/poly_delimited.xml')
    gdal.Unlink('tmp/poly_delimited.csv')

    assert 'INFO' in ret and 'ERROR' not in ret


def test_ogr_pds4_read_table_delimited_group_field():

    ds = ogr.Open('data/pds4/test_delimited_group.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 5
    f = lyr.GetNextFeature()
    assert f['first_field'] == 1
    assert f['group_first_field_1'] == 2
    assert f['group_second_field_1'] == '3'
    assert f['group_first_field_2'] == 4
    assert f['group_second_field_2'] == '5'


def test_ogr_pds4_read_product_collection():

    ds = ogr.Open('data/pds4/product_collection.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    assert f['Member Status'] == 'P'
    assert f['LIDVID_LID'] == 'urn:nasa:pds:orex.ocams:data_reduced:20160919t162205s722_map_l1pan_v031.fits::1.0'
