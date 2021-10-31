#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PDS4 format
# Author:   Even Rouault, <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2017, Hobu Inc
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
import os
import struct


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

    # for GDAL 3.4 / PDS4_PDS_1G00

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1G00.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1G00.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1G00_1950.xsd',
                                  'pds.nasa.gov_pds4_cart_v1_PDS4_CART_1G00_1950.xsd',
                                  force_download=True):
        pytest.skip()

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/disp/v1/PDS4_DISP_1G00_1500.xsd',
                                  'pds.nasa.gov_pds4_disp_v1_PDS4_DISP_1G00_1500.xsd',
                                  force_download=True):
        pytest.skip()

    # Used by PDS4_CART_1G00_1950.xsd
    if not gdaltest.download_file('https://pds.nasa.gov/pds4/geom/v1/PDS4_GEOM_1G00_1920.xsd',
                                  'pds.nasa.gov_pds4_geom_v1_PDS4_GEOM_1G00_1920.xsd',
                                  force_download=True):
        pytest.skip()

    # GDAL 3.3

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
# Perform simple read test on PDS4 dataset.

@pytest.mark.parametrize("filename", ['pds4/byte_pds4_cart_1700.xml',
                                      'pds4/byte_pds4_cart_1b00.xml',
                                      'pds4/byte_pds4_cart_1d00_1933.xml',
                                      'pds4/byte_pds4_cart_1g00_1950.xml'])
def test_pds4_read_cart_versions(filename):
    srs = """PROJCS["Transverse Mercator Earth",
    GEOGCS["GCS_Earth",
        DATUM["D_North_American_Datum_1927",
            SPHEROID["North_American_Datum_1927",6378206.4,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],UNIT["meter",1]]
"""
    gt = (-59280.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    tst = gdaltest.GDALTest('PDS4', filename, 1, 4672)
    return tst.testOpen(check_prj=srs, check_gt=gt)

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

###############################################################################
# Test CreateCopy() with defaults


def test_pds4_2():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053)
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret


###############################################################################
def test_pds4_write_utm():

    src_ds = gdal.Open('data/byte.tif')
    with gdaltest.error_handler():
        gdal.GetDriverByName('PDS4').CreateCopy('/vsimem/temp.xml', src_ds)
    ds = gdal.Open('/vsimem/temp.xml')
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    f = gdal.VSIFOpenL('/vsimem/temp.xml', 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert '<cart:west_bounding_coordinate unit="deg">-117.6411686' in data, data
    assert '<cart:east_bounding_coordinate unit="deg">-117.6281108' in data, data
    assert '<cart:north_bounding_coordinate unit="deg">33.90241956' in data, data
    assert '<cart:south_bounding_coordinate unit="deg">33.891530168' in data, data
    gdal.GetDriverByName('PDS4').Delete('/vsimem/temp.xml')


###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BSQ


def test_pds4_3():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BSQ'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BIP


def test_pds4_4():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BIP'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BIL


def test_pds4_5():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BIL'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BSQ and IMAGE_FORMAT=GEOTIFF


def test_pds4_6():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BSQ', 'IMAGE_FORMAT=GEOTIFF'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BIP and IMAGE_FORMAT=GEOTIFF


def test_pds4_7():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BIP', 'IMAGE_FORMAT=GEOTIFF'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test SRS support

@pytest.mark.parametrize('proj4str', ['+proj=eqc +lat_ts=43.75 +lat_0=10 +lon_0=-112.5 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                      '+proj=lcc +lat_1=10 +lat_0=10 +lon_0=-112.5 +k_0=0.9 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',  # LCC_1SP
                                      '+proj=lcc +lat_0=10 +lon_0=-112.5 +lat_1=9 +lat_2=11 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',  # LCC_2SP
                                      '+proj=omerc +lat_0=10 +lonc=11 +alpha=12 +gamma=12 +k=1 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',  # Oblique Mercator Azimuth Center
                                      '+proj=omerc +lat_0=10 +lat_1=12 +lon_1=11 +lat_2=14 +lon_2=13 +k=1 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',  # Oblique Mercator 2 points
                                      '+proj=stere +lat_0=90 +lon_0=10 +k=0.9 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',  # Polar Stereographic
                                      '+proj=poly +lat_0=9 +lon_0=10 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                      '+proj=sinu +lon_0=10 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                      '+proj=tmerc +lat_0=11 +lon_0=10 +k=0.9 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                      '+proj=merc +lat_ts=2 +lon_0=3 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                      '+proj=merc +lon_0=3 +k=0.9 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                      '+proj=ortho +lat_0=1 +lon_0=2 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                      '+proj=laea +lat_0=1 +lon_0=2 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs',
                                     ])
def test_pds4_projected_srs(proj4str):

    options = ['VAR_LOGICAL_IDENTIFIER=urn:foo:bar:baz:logical_identifier',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=urn:foo:bar:baz:ialr',
               'VAR_TARGET_TYPE=planet']

    filename = '/vsimem/out.xml'

    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, options=options)
    sr = osr.SpatialReference()
    sr.ImportFromProj4(proj4str)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    with gdaltest.error_handler():
        ds = None

    ret = validate_xml(filename)
    assert ret, ('validation of file for %s failed' % proj4str)

    ds = gdal.Open(filename)
    wkt = ds.GetProjectionRef()
    sr = osr.SpatialReference()
    sr.SetFromUserInput(wkt)
    got_proj4 = sr.ExportToProj4().strip()
    assert got_proj4 == proj4str, ''

    gdal.GetDriverByName('PDS4').Delete(filename)


def test_pds4_longlat_srs():

    options = ['VAR_LOGICAL_IDENTIFIER=urn:foo:bar:baz:logical_identifier',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=urn:foo:bar:baz:ialr',
               'VAR_TARGET_TYPE=planet']

    filename = '/vsimem/out.xml'

    # longlat doesn't roundtrip as such
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, options=options)
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +R=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    with gdaltest.error_handler():
        ds = None
    ds = gdal.Open(filename)
    wkt = ds.GetProjectionRef()
    sr = osr.SpatialReference()
    sr.SetFromUserInput(wkt)
    got_proj4 = sr.ExportToProj4().strip()
    proj4 = '+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +R=2439400 +units=m +no_defs'
    assert got_proj4 == proj4, ''

    got_gt = ds.GetGeoTransform()
    expected_gt = (85151.12354629935, 42575.561773149675, 0.0, 2086202.5268843342, 0.0, -85151.12354629935)
    assert max([abs(got_gt[i] - expected_gt[i]) for i in range(6)]) <= 1, ''
    ds = None

    gdal.GetDriverByName('PDS4').Delete(filename)

###############################################################################
# Test nodata / mask


def test_pds4_9():

    ds = gdal.Open('data/pds4/byte_pds4_cart_1700.xml')
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    assert ndv == 74

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 4800
    ds = None

    filename = '/vsimem/out.xml'
    # Test copy of all specialConstants

    options = ['VAR_LOGICAL_IDENTIFIER=urn:foo:bar:baz:logical_identifier',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=urn:foo:bar:baz:ialr',
               'VAR_TARGET_TYPE=planet']

    with hide_substitution_warnings_error_handler():
        gdal.Translate(filename, 'data/pds4/byte_pds4_cart_1700.xml',
                       format='PDS4', creationOptions=options)

    ret = validate_xml(filename)
    assert ret, 'validation failed'

    ds = gdal.Open(filename)
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    assert ndv == 74

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 4800

    ds = None

    filename = '/vsimem/out.xml'
    # Test copy of all specialConstants and override noData
    for frmt in ['RAW', 'GEOTIFF']:
        with hide_substitution_warnings_error_handler():
            gdal.Translate(filename, 'data/pds4/byte_pds4_cart_1700.xml', format='PDS4',
                           noData=75,
                           creationOptions=['IMAGE_FORMAT=' + frmt] + options)

        ret = validate_xml(filename)
        assert ret, 'validation failed'

        ds = gdal.Open(filename)
        ndv = ds.GetRasterBand(1).GetNoDataValue()
        assert ndv == 75

        flag = ds.GetRasterBand(1).GetMaskFlags()
        assert flag == 0

        cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
        assert cs == 4833

        ds = None

    # Test just setting noData
    for frmt in ['RAW', 'GEOTIFF']:
        with hide_substitution_warnings_error_handler():
            gdal.Translate(filename, 'data/pds4/byte_pds4_cart_1700.xml', format='PDS4',
                           creationOptions=['USE_SRC_LABEL=NO',
                                            'IMAGE_FORMAT=' + frmt] + options)

        ret = validate_xml(filename)
        assert ret, 'validation failed'

        ds = gdal.Open(filename)
        ndv = ds.GetRasterBand(1).GetNoDataValue()
        assert ndv == 74, frmt

        ds = None

        # Test filling with nodata
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                                 options=['IMAGE_FORMAT=' + frmt] + options)
        ds.GetRasterBand(1).SetNoDataValue(1)
        with hide_substitution_warnings_error_handler():
            ds = None

        ds = gdal.Open(filename)
        cs = ds.GetRasterBand(1).Checksum()
        assert cs == 1, frmt
        ds = None

        # Test setting nodata and then explicit Fill()
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                                 options=['IMAGE_FORMAT=' + frmt] + options)
        ds.GetRasterBand(1).SetNoDataValue(10)
        ds.GetRasterBand(1).Fill(1)
        with hide_substitution_warnings_error_handler():
            ds = None

        ds = gdal.Open(filename)
        cs = ds.GetRasterBand(1).Checksum()
        assert cs == 1, frmt
        ds = None

    template = '/vsimem/template.xml'

    # Empty Special_Constants + optional Reference_List
    gdal.FileFromMemBuffer(template, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xsi:schemaLocation="http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd">
    <Identification_Area>
        <logical_identifier>${LOGICAL_IDENTIFIER}</logical_identifier>
        <version_id>1.0</version_id>
        <title>${TITLE}</title>
        <information_model_version>1.8.0.0</information_model_version>
        <product_class>Product_Observational</product_class>
    </Identification_Area>
    <Observation_Area>
        <Time_Coordinates>
        <start_date_time xsi:nil="true" />
        <stop_date_time xsi:nil="true" />
        </Time_Coordinates>
        <Investigation_Area>
        <name>${INVESTIGATION_AREA_NAME}</name>
        <type>Mission</type>
        <Internal_Reference>
            <lid_reference>${INVESTIGATION_AREA_LID_REFERENCE}</lid_reference>
            <reference_type>data_to_investigation</reference_type>
        </Internal_Reference>
        </Investigation_Area>
        <Observing_System>
        <Observing_System_Component>
            <name>${OBSERVING_SYSTEM_NAME}</name>
            <type>Spacecraft</type>
        </Observing_System_Component>
        </Observing_System>
        <Target_Identification>
        <name>Earth</name>
        <type>Planet</type>
        <Internal_Reference>
            <lid_reference>urn:nasa:pds:context:target:planet.earth</lid_reference>
            <reference_type>data_to_target</reference_type>
        </Internal_Reference>
        </Target_Identification>
    </Observation_Area>
    <!-- some comments -->
    <Reference_List>
        <External_Reference>
          <doi>doi</doi>
          <reference_text>ref_text</reference_text>
          <description>instrument overview</description>
        </External_Reference>
    </Reference_List>
    <!-- other comments -->
    <File_Area_Observational>
        <Array_2D>
            <Special_Constants />
        </Array_2D>
    </File_Area_Observational>
</Product_Observational>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template] + options)
    ds.GetRasterBand(1).SetNoDataValue(10)
    with hide_substitution_warnings_error_handler():
        ds = None

    ds = gdal.Open(filename)
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    assert ndv == 10
    ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert 'some comments' in data
    assert '<Reference_List>' in data
    assert 'other comments' in data

    ret = validate_xml(filename)
    assert ret, 'validation failed'

    # Special_Constants with just saturated_constant
    gdal.FileFromMemBuffer(template, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xsi:schemaLocation="http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd">
    <Identification_Area>
        <logical_identifier>${LOGICAL_IDENTIFIER}</logical_identifier>
        <version_id>1.0</version_id>
        <title>${TITLE}</title>
        <information_model_version>1.8.0.0</information_model_version>
        <product_class>Product_Observational</product_class>
    </Identification_Area>
    <Observation_Area>
        <Time_Coordinates>
        <start_date_time xsi:nil="true" />
        <stop_date_time xsi:nil="true" />
        </Time_Coordinates>
        <Investigation_Area>
        <name>${INVESTIGATION_AREA_NAME}</name>
        <type>Mission</type>
        <Internal_Reference>
            <lid_reference>${INVESTIGATION_AREA_LID_REFERENCE}</lid_reference>
            <reference_type>data_to_investigation</reference_type>
        </Internal_Reference>
        </Investigation_Area>
        <Observing_System>
        <Observing_System_Component>
            <name>${OBSERVING_SYSTEM_NAME}</name>
            <type>Spacecraft</type>
        </Observing_System_Component>
        </Observing_System>
        <Target_Identification>
        <name>Earth</name>
        <type>Planet</type>
        <Internal_Reference>
            <lid_reference>urn:nasa:pds:context:target:planet.earth</lid_reference>
            <reference_type>data_to_target</reference_type>
        </Internal_Reference>
        </Target_Identification>
    </Observation_Area>
    <File_Area_Observational>
        <Array_2D>
            <Special_Constants>
                <saturated_constant>255</saturated_constant>
            </Special_Constants>
        </Array_2D>
    </File_Area_Observational>
</Product_Observational>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template] + options)
    ds.GetRasterBand(1).SetNoDataValue(10)
    with hide_substitution_warnings_error_handler():
        ds = None

    ds = gdal.Open(filename)
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    assert ndv == 10
    ds = None

    ret = validate_xml(filename)
    assert ret, 'validation failed'

    gdal.GetDriverByName('PDS4').Delete(filename)
    gdal.Unlink(template)

###############################################################################
# Test scale / offset


def test_pds4_10():

    filename = '/vsimem/out.xml'
    filename2 = '/vsimem/out2.xml'
    for frmt in ['RAW', 'GEOTIFF']:
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                                 options=['IMAGE_FORMAT=' + frmt])
        ds.GetRasterBand(1).SetScale(2)
        ds.GetRasterBand(1).SetOffset(3)
        with hide_substitution_warnings_error_handler():
            ds = None
            gdal.Translate(filename2, filename, format='PDS4')

        ds = gdal.Open(filename2)
        scale = ds.GetRasterBand(1).GetScale()
        assert scale == 2
        offset = ds.GetRasterBand(1).GetOffset()
        assert offset == 3
        ds = None

        gdal.GetDriverByName('PDS4').Delete(filename)
        gdal.GetDriverByName('PDS4').Delete(filename2)


###############################################################################
# Test various data types


def test_pds4_11():

    filename = '/vsimem/out.xml'
    for (dt, data) in [(gdal.GDT_Byte, struct.pack('B', 255)),
                       (gdal.GDT_UInt16, struct.pack('H', 65535)),
                       (gdal.GDT_Int16, struct.pack('h', -32768)),
                       (gdal.GDT_UInt32, struct.pack('I', 4000000000)),
                       (gdal.GDT_Int32, struct.pack('i', -2000000000)),
                       (gdal.GDT_Float32, struct.pack('f', 1.25)),
                       (gdal.GDT_Float64, struct.pack('d', 1.25)),
                       (gdal.GDT_CFloat32, struct.pack('ff', 1.25, 2.5)),
                       (gdal.GDT_CFloat64, struct.pack('dd', 1.25, 2.5))]:
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1, dt)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, data)
        with hide_substitution_warnings_error_handler():
            ds = None

        with gdaltest.config_option('PDS4_FORCE_MASK', 'YES'):
            ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).DataType == dt
        got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
        assert got_data == data, dt
        cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
        assert cs == 3, dt
        ds = None

    gdal.GetDriverByName('PDS4').Delete(filename)

###############################################################################
# Test various creation options


def test_pds4_12():

    filename = '/vsimem/out.xml'
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['VAR_LOGICAL_IDENTIFIER=logical_identifier',
                                                      'VAR_TITLE=title',
                                                      'VAR_INVESTIGATION_AREA_NAME=ian',
                                                      'VAR_INVESTIGATION_AREA_LID_REFERENCE=ialr',
                                                      'VAR_OBSERVING_SYSTEM_NAME=osn',
                                                      'VAR_UNUSED=foo',
                                                      'TEMPLATE=data/pds4/byte_pds4_cart_1700.xml',
                                                      'BOUNDING_DEGREES=1,2,3,4',
                                                      'LATITUDE_TYPE=Planetographic',
                                                      'LONGITUDE_DIRECTION=Positive West',
                                                      'IMAGE_FILENAME=/vsimem/myimage.raw'])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +R=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert '<logical_identifier>logical_identifier</logical_identifier>' in data
    assert '<cart:west_bounding_coordinate unit="deg">1</cart:west_bounding_coordinate>' in data
    assert '<cart:east_bounding_coordinate unit="deg">3</cart:east_bounding_coordinate>' in data
    assert '<cart:north_bounding_coordinate unit="deg">4</cart:north_bounding_coordinate>' in data
    assert '<cart:south_bounding_coordinate unit="deg">2</cart:south_bounding_coordinate>' in data
    assert '<cart:latitude_type>Planetographic</cart:latitude_type>' in data
    assert '<cart:longitude_direction>Positive West</cart:longitude_direction>' in data
    assert '<file_name>myimage.raw</file_name>' in data

    gdal.GetDriverByName('PDS4').Delete(filename)

###############################################################################
# Test subdatasets


def test_pds4_13():

    ds = gdal.Open('data/pds4/byte_pds4_cart_1700_multi_sds.xml')
    subds = ds.GetSubDatasets()
    expected_subds = [('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:1:1',
                       'Image file byte_pds4_cart_1700.img, array first_sds'),
                      ('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:1:2',
                       'Image file byte_pds4_cart_1700.img, array second_sds'),
                      ('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:2:1',
                       'Image file byte_pds4_cart_1700.img, array third_sds')]
    assert subds == expected_subds

    ds = gdal.Open('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:1:1')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 2315

    ds = gdal.Open('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:1:2')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 2302

    ds = gdal.Open('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:2:1')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 3496

    ds = gdal.Open(os.path.join(os.getcwd(), 'data', 'pds4', 'byte_pds4_cart_1700_multi_sds.xml'))
    subds_name = ds.GetSubDatasets()[0][0]
    ds = gdal.Open(subds_name)
    assert ds is not None

    with gdaltest.error_handler():
        ds = gdal.Open(r'PDS4:c:\do_not\exist.xml:1:1')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:i_do_not_exist.xml')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:i_do_not_exist.xml:1:1')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:3:1')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:data/pds4/byte_pds4_cart_1700_multi_sds.xml:1:3')
    assert ds is None

###############################################################################
# Test error cases


def test_pds4_14():

    filename = '/vsimem/test.xml'

    gdal.FileFromMemBuffer(filename, "Product_Observational http://pds.nasa.gov/pds4/pds/v1")
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds is None

    gdal.FileFromMemBuffer(filename, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1">
    <File_Area_Observational/>
    <File_Area_Observational>
        <File/>
    </File_Area_Observational>
    <File_Area_Observational>
        <File>
            <file_name>i_do_not_exist.img</file_name>
        </File>
        <Array>
            <axes>3</axes>
        </Array>
    </File_Area_Observational>
    <File_Area_Observational>
        <File>
            <file_name>i_do_not_exist.img</file_name>
        </File>
        <Array>
            <axes>3</axes>
            <axis_index_order>Last Index Fastest</axis_index_order>
        </Array>
    </File_Area_Observational>
    <File_Area_Observational>
        <File>
            <file_name>i_do_not_exist.img</file_name>
        </File>
        <Array>
            <axes>3</axes>
            <axis_index_order>Last Index Fastest</axis_index_order>
            <Element_Array>
                <data_type>SignedByte</data_type>
            </Element_Array>
            <Axis_Array>
            </Axis_Array>
            <Axis_Array>
                <axis_name>x</axis_name>
                <elements>1</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Band</axis_name>
                <elements>0</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Band</axis_name>
                <elements>1</elements>
                <sequence_number>0</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Band</axis_name>
                <elements>1</elements>
                <sequence_number>4</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Band</axis_name>
                <elements>1</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Band</axis_name>
                <elements>1</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
        </Array>
    </File_Area_Observational>
</Product_Observational>""")
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds is None

    gdal.FileFromMemBuffer(filename, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1">
    <File_Area_Observational>
        <File>
            <file_name>i_do_not_exist.img</file_name>
        </File>
        <Array_3D>
            <axes>3</axes>
            <axis_index_order>Last Index Fastest</axis_index_order>
            <Element_Array>
                <data_type>UnsignedByte</data_type>
            </Element_Array>
            <Axis_Array>
                <axis_name>Band</axis_name>
                <elements>65537</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Line</axis_name>
                <elements>1</elements>
                <sequence_number>2</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Sample</axis_name>
                <elements>1</elements>
                <sequence_number>3</sequence_number>
            </Axis_Array>
        </Array_3D>
    </File_Area_Observational>
</Product_Observational>""")
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds is None

    gdal.FileFromMemBuffer(filename, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1">
    <File_Area_Observational>
        <File>
            <file_name>i_do_not_exist.img</file_name>
        </File>
        <Array_2D>
            <axes>2</axes>
            <axis_index_order>Last Index Fastest</axis_index_order>
            <Element_Array>
                <data_type>SignedByte</data_type>
            </Element_Array>
            <Axis_Array>
                <axis_name>Line</axis_name>
                <elements>1</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Sample</axis_name>
                <elements>1</elements>
                <sequence_number>2</sequence_number>
            </Axis_Array>
        </Array_2D>
    </File_Area_Observational>
</Product_Observational>""")
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds is None

    gdal.FileFromMemBuffer(filename, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1">
    <File_Area_Observational>
        <File>
            <file_name>i_do_not_exist.img</file_name>
        </File>
        <Array_2D>
            <axes>2</axes>
            <axis_index_order>Last Index Fastest</axis_index_order>
            <Element_Array>
                <data_type>ComplexMSB16</data_type>
            </Element_Array>
            <Axis_Array>
                <axis_name>Line</axis_name>
                <elements>1</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Sample</axis_name>
                <elements>2000000000</elements>
                <sequence_number>2</sequence_number>
            </Axis_Array>
        </Array_2D>
    </File_Area_Observational>
</Product_Observational>""")
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds is None

    gdal.FileFromMemBuffer(filename, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1">
    <File_Area_Observational>
        <File>
            <file_name>i_do_not_exist.img</file_name>
        </File>
        <Array_2D>
            <axes>2</axes>
            <axis_index_order>Last Index Fastest</axis_index_order>
            <Element_Array>
                <data_type>ComplexMSB16</data_type>
            </Element_Array>
            <Axis_Array>
                <axis_name>Sample</axis_name>
                <elements>1</elements>
                <sequence_number>1</sequence_number>
            </Axis_Array>
            <Axis_Array>
                <axis_name>Line</axis_name>
                <elements>2000000000</elements>
                <sequence_number>2</sequence_number>
            </Axis_Array>
        </Array_2D>
    </File_Area_Observational>
</Product_Observational>""")
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds is None

    gdal.Unlink(filename)

    # Invalid value for INTERLEAVE
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create('/vsimem/out.xml', 1, 1,
                                                 options=['INTERLEAVE=INVALID'])
    assert ds is None

    # INTERLEAVE=BIL not supported for GeoTIFF in PDS4
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create('/vsimem/out.xml', 1, 1,
                                                 options=['INTERLEAVE=BIL',
                                                          'IMAGE_FORMAT=GEOTIFF'])
    assert ds is None

    # Cannot create GeoTIFF file
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create('/i/do_not/exist.xml', 1, 1,
                                                 options=['IMAGE_FORMAT=GEOTIFF'])
    assert ds is None

    gdal.Translate('/vsimem/test.tif', 'data/byte.tif')
    # Output file has same name as input file
    with gdaltest.error_handler():
        ds = gdal.Translate('/vsimem/test.xml', '/vsimem/test.tif',
                            format='PDS4', creationOptions=['IMAGE_FORMAT=GEOTIFF'])
    assert ds is None
    gdal.Unlink('/vsimem/test.tif')

    template = '/vsimem/template.xml'

    # Missing Product_Observational root
    gdal.FileFromMemBuffer(template, """<foo/>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    assert gdal.GetLastErrorMsg() == 'Cannot find Product_Observational element in template'

    # Missing Target_Identification
    gdal.FileFromMemBuffer(template, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xsi:schemaLocation="http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd">
</Product_Observational>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +R=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    assert gdal.GetLastErrorMsg() == 'Cannot find Target_Identification element in template'

    # Missing Observation_Area
    gdal.FileFromMemBuffer(template, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xsi:schemaLocation="http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd">
</Product_Observational>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    assert gdal.GetLastErrorMsg() == 'Cannot find Observation_Area in template'

    # Unexpected content found after Observation_Area in template
    gdal.FileFromMemBuffer(template, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xsi:schemaLocation="http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd">
    <Observation_Area/>
    <!-- -->
    <foo/>
</Product_Observational>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    assert gdal.GetLastErrorMsg() == 'Unexpected content found after Observation_Area in template'

    gdal.Unlink(template)
    gdal.Unlink(filename)
    gdal.Unlink('/vsimem/test.img')

###############################################################################
# Test Create() without geospatial info but from a geospatial enabled template


def test_pds4_15():

    filename = '/vsimem/out.xml'
    with hide_substitution_warnings_error_handler():
        gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                            options=['TEMPLATE=data/pds4/byte_pds4_cart_1700.xml'])

    ret = validate_xml(filename)
    assert ret, 'validation failed'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert '<cart:Cartography>' not in data

    gdal.GetDriverByName('PDS4').Delete(filename)

###############################################################################
# Test Create() with geospatial info but from a template without Discipline_Area


def test_pds4_16():

    template = '/vsimem/template.xml'
    filename = '/vsimem/out.xml'

    gdal.FileFromMemBuffer(template,
                           """<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1"
                          xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="
    http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd">
  <Identification_Area>
    <logical_identifier>${LOGICAL_IDENTIFIER}</logical_identifier>
    <version_id>1.0</version_id>
    <title>${TITLE}</title>
    <information_model_version>1.8.0.0</information_model_version>
    <product_class>Product_Observational</product_class>
  </Identification_Area>
  <Observation_Area>
    <Time_Coordinates>
      <start_date_time xsi:nil="true" />
      <stop_date_time xsi:nil="true" />
    </Time_Coordinates>
    <Investigation_Area>
      <name>${INVESTIGATION_AREA_NAME}</name>
      <type>Mission</type>
      <Internal_Reference>
        <lid_reference>${INVESTIGATION_AREA_LID_REFERENCE}</lid_reference>
        <reference_type>data_to_investigation</reference_type>
      </Internal_Reference>
    </Investigation_Area>
    <Observing_System>
      <Observing_System_Component>
        <name>${OBSERVING_SYSTEM_NAME}</name>
        <type>Spacecraft</type>
      </Observing_System_Component>
    </Observing_System>
    <Target_Identification>
      <name>Earth</name>
      <type>Planet</type>
      <Internal_Reference>
        <lid_reference>urn:nasa:pds:context:target:planet.earth</lid_reference>
        <reference_type>data_to_target</reference_type>
      </Internal_Reference>
    </Target_Identification>
  </Observation_Area>
  <File_Area_Observational>
  </File_Area_Observational>
</Product_Observational>""")

    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +R=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    with hide_substitution_warnings_error_handler():
        ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)

    ret = validate_xml(filename)
    assert ret, ('validation failed: %s' % data)

    assert 'http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd http://pds.nasa.gov/pds4/cart/v1 https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.xsd"' in data
    assert 'xmlns:cart="http://pds.nasa.gov/pds4/cart/v1"' in data
    assert '<cart:Cartography>' in data

    gdal.GetDriverByName('PDS4').Delete(filename)
    gdal.Unlink(template)

###############################################################################
# Test ARRAY_TYPE creation option


def test_pds4_17():

    options = ['VAR_LOGICAL_IDENTIFIER=urn:foo:bar:baz:logical_identifier',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=urn:foo:bar:baz:ialr',
               'VAR_TARGET_TYPE=planet',
               'VAR_TARGET=planet']

    filename = '/vsimem/out.xml'

    with gdaltest.error_handler():
        gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1, options=['ARRAY_TYPE=Array_2D'] + options)

    ret = validate_xml(filename)
    assert ret, 'validation failed'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert ('<Array_2D>' in data and '<axes>2</axes>' in data and \
       '<axis_name>Band</axis_name>' not in data and \
       '<sequence_number>3</sequence_number>' not in data)

    gdal.GetDriverByName('PDS4').Delete(filename)

    # Test multi-band creation with Array_2D
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 2, options=['ARRAY_TYPE=Array_2D'] + options)
    assert ds is None, 'expected failure'

    # Test multi-band creation with Array_3D_Spectrum
    with gdaltest.error_handler():
        gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 2, options=['ARRAY_TYPE=Array_3D_Spectrum'] + options)

    ret = validate_xml(filename)
    assert ret, 'validation failed'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert '<Array_3D_Spectrum>' in data and '<axes>3</axes>' in data

    gdal.GetDriverByName('PDS4').Delete(filename)

###############################################################################
# Test RADII creation option


def test_pds4_18():

    filename = '/vsimem/out.xml'

    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1, options=['RADII=1,2'])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +R=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    with gdaltest.error_handler():
        ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert ('<cart:a_axis_radius unit="m">1</cart:a_axis_radius>' in data and \
       '<cart:b_axis_radius unit="m">1</cart:b_axis_radius>' in data and \
       '<cart:c_axis_radius unit="m">2</cart:c_axis_radius>' in data)

    gdal.GetDriverByName('PDS4').Delete(filename)

###############################################################################
# Test APPEND_SUBDATASET=YES


@pytest.mark.parametrize('options', [['IMAGE_FORMAT=RAW'], ['IMAGE_FORMAT=GEOTIFF']])
def test_pds4_append_subdataset(options):

    filename = '/vsimem/out.xml'

    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1, options=options)
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=tmerc +R=6378137 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    ds.GetRasterBand(1).Fill(1)
    ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    src_ds.GetRasterBand(1).Fill(2)

    assert gdal.GetDriverByName('PDS4').CreateCopy(filename, src_ds, options=['APPEND_SUBDATASET=YES'])
    ds = gdal.Open(filename)
    subds = ds.GetSubDatasets()
    assert len(subds) == 2, subds

    ds = gdal.Open('PDS4:/vsimem/out.xml:1:1')
    assert ds.GetRasterBand(1).Checksum() == 1

    ds = gdal.Open('PDS4:/vsimem/out.xml:1:2')
    assert ds.GetRasterBand(1).Checksum() == 2

    src_ds.GetRasterBand(1).Fill(3)

    assert gdal.GetDriverByName('PDS4').CreateCopy(filename, src_ds, options=['APPEND_SUBDATASET=YES'])

    ds = gdal.Open('PDS4:/vsimem/out.xml:1:3')
    assert ds.GetRasterBand(1).Checksum() == 3

    gdal.GetDriverByName('PDS4').Delete(filename)


###############################################################################
# Test APPEND_SUBDATASET=YES error case


def test_pds4_append_subdataset_not_same_gt():

    filename = '/vsimem/out.xml'

    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=tmerc +R=6378137 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    ds.GetRasterBand(1).Fill(1)
    ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([2.1, 1, 0, 49, 0, -2])
    src_ds.GetRasterBand(1).Fill(2)

    assert not gdal.GetDriverByName('PDS4').CreateCopy(filename, src_ds, options=['APPEND_SUBDATASET=YES'])

    gdal.GetDriverByName('PDS4').Delete(filename)


###############################################################################
# Test APPEND_SUBDATASET=YES error case


def test_pds4_append_subdataset_not_same_srs():

    filename = '/vsimem/out.xml'

    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=tmerc +R=6378137 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    ds.GetRasterBand(1).Fill(1)
    ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=tmerc +R=1 +no_defs')
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    src_ds.GetRasterBand(1).Fill(2)

    assert not gdal.GetDriverByName('PDS4').CreateCopy(filename, src_ds, options=['APPEND_SUBDATASET=YES'])

    gdal.GetDriverByName('PDS4').Delete(filename)

###############################################################################


def _test_createlabelonly(src_ds,
                          expected_content = None,
                          filename = '/vsimem/out.xml',
                          validate = False,
                          creation_options = []):

    src_ds_name = src_ds.GetDescription()
    src_driver_name = src_ds.GetDriver().GetDescription()

    with gdaltest.error_handler():
        assert gdal.GetDriverByName('PDS4').CreateCopy(filename, src_ds, options=['CREATE_LABEL_ONLY=YES'] + creation_options)
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds
    found_src_ds_name = False
    for fname in ds.GetFileList():
        if fname.replace('\\', '/') == src_ds_name.replace('\\', '/'):
            found_src_ds_name = True
    assert found_src_ds_name, ds.GetFileList()
    assert ds.RasterCount == src_ds.RasterCount
    assert ds.RasterXSize == src_ds.RasterXSize
    assert ds.RasterYSize == src_ds.RasterYSize
    with gdaltest.error_handler():
        for i in range(ds.RasterCount):
            assert ds.GetRasterBand(i+1).Checksum() == src_ds.GetRasterBand(i+1).Checksum()
    ds = None
    src_ds = None

    if validate:
        ret = validate_xml(filename)
        assert ret, 'validation failed'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert 'Binary file pre-existing PDS4 label' in data, data
    if expected_content:
        if isinstance(expected_content, list):
            for expected_content_item in expected_content:
                assert expected_content_item in data, data
        else:
            assert expected_content in data, data

    gdal.GetDriverByName('PDS4').Delete(filename)
    assert gdal.VSIStatL(src_ds_name)
    gdal.GetDriverByName(src_driver_name).Delete(src_ds_name)

###############################################################################
# Test CREATE_LABEL_ONLY=YES with ENVI


def test_pds4_createlabelonly_envi():

    gdal.FileFromMemBuffer('/vsimem/envi_rgbsmall_bip.img', open('data/envi/envi_rgbsmall_bip.img', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/envi_rgbsmall_bip.hdr', open('data/envi/envi_rgbsmall_bip.hdr', 'rb').read())

    src_ds = gdal.Open('/vsimem/envi_rgbsmall_bip.img')
    return _test_createlabelonly(src_ds)


###############################################################################
# Test CREATE_LABEL_ONLY=YES with GTiff


def test_pds4_createlabelonly_gtiff():

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/byte.tif', gdal.Open('data/byte.tif'))

    options = ['VAR_LOGICAL_IDENTIFIER=urn:foo:bar:baz:logical_identifier',
               'VAR_INVESTIGATION_AREA_LID_REFERENCE=urn:foo:bar:baz:ialr']

    src_ds = gdal.Open('/vsimem/byte.tif')
    return _test_createlabelonly(src_ds,
                                 expected_content = '<parsing_standard_id>TIFF 6.0</parsing_standard_id>',
                                 validate = True,
                                 creation_options = options)


###############################################################################
# Test CREATE_LABEL_ONLY=YES with a tiled GTiff (incompatible of raw binary layout)


def test_pds4_createlabelonly_gtiff_error():

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/byte.tif', gdal.Open('data/byte.tif'), options=['TILED=YES'])
    src_ds = gdal.Open('/vsimem/byte.tif')
    with gdaltest.error_handler():
        assert not gdal.GetDriverByName('PDS4').CreateCopy('/vsimem/out.xml', src_ds, options=['CREATE_LABEL_ONLY=YES'])
    src_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/byte.tif')


###############################################################################
# Test CREATE_LABEL_ONLY=YES with BigTIFF


def test_pds4_createlabelonly_bigtiff():

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/byte.tif', gdal.Open('data/byte.tif'), options=['BIGTIFF=YES'])

    src_ds = gdal.Open('/vsimem/byte.tif')
    return _test_createlabelonly(src_ds, expected_content = '<parsing_standard_id>TIFF 6.0</parsing_standard_id>')


###############################################################################
# Test CREATE_LABEL_ONLY=YES with ISIS3


def test_pds4_createlabelonly_isis3():

    gdal.GetDriverByName('ISIS3').CreateCopy('/vsimem/input.cub', gdal.Open('../gcore/data/uint16.tif'))

    src_ds = gdal.Open('/vsimem/input.cub')
    return _test_createlabelonly(src_ds, expected_content = '<parsing_standard_id>ISIS3</parsing_standard_id>')


###############################################################################
# Test CREATE_LABEL_ONLY=YES with VICAR


def test_pds4_createlabelonly_vicar():

    gdal.FileFromMemBuffer('/vsimem/test_vicar_truncated.bin', open('data/vicar/test_vicar_truncated.bin', 'rb').read())

    src_ds = gdal.Open('/vsimem/test_vicar_truncated.bin')
    return _test_createlabelonly(src_ds, expected_content = '<parsing_standard_id>VICAR2</parsing_standard_id>')


###############################################################################
# Test CREATE_LABEL_ONLY=YES with FITS


def test_pds4_createlabelonly_fits():

    fits_drv = gdal.GetDriverByName('FITS')
    if not fits_drv:
        pytest.skip()

    fits_drv.CreateCopy('tmp/input.fits', gdal.Open('../gcore/data/int16.tif'))

    src_ds = gdal.Open('tmp/input.fits')
    return _test_createlabelonly(src_ds,
                                 expected_content = ['<parsing_standard_id>FITS 3.0</parsing_standard_id>',
                                                     '<disp:vertical_display_direction>Bottom to Top</disp:vertical_display_direction>'],
                                 filename = 'tmp/out.xml')


###############################################################################
# Test CREATE_LABEL_ONLY=YES with PDS3


def test_pds4_createlabelonly_pds3():

    gdal.FileFromMemBuffer('/vsimem/mc02_truncated.img', open('data/pds/mc02_truncated.img', 'rb').read())

    src_ds = gdal.Open('/vsimem/mc02_truncated.img')
    return _test_createlabelonly(src_ds, expected_content = '<parsing_standard_id>PDS3</parsing_standard_id>')



###############################################################################
# Test CreateCopy() with a template that has sp:Spectral_Characteristics


def test_pds4_spectral_characteristics():

    # Needed by template_with_sp.xml
    if not gdaltest.download_file('http://pds.nasa.gov/pds4/sp/v1/PDS4_SP_1100.xsd',
                                  'pds.nasa.gov_pds4_sp_v1_PDS4_SP_1100.xsd',
                                  force_download=True):
        pytest.skip()

    # Needed by PDS4_SP_1100.xsd
    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1100.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1100.xsd',
                                  force_download=True):
        pytest.skip()

    # Needed by template_with_sp.xml
    if not gdaltest.download_file('http://pds.nasa.gov/pds4/disp/v1/PDS4_DISP_1600.xsd',
                                  'pds.nasa.gov_pds4_disp_v1_PDS4_DISP_1600.xsd',
                                  force_download=True):
        pytest.skip()

    # Needed by PDS4_DISP_1600.xsd
    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1600.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1600.xsd',
                                  force_download=True):
        pytest.skip()

    filename = '/vsimem/out.xml'
    with hide_substitution_warnings_error_handler():
        gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                            options=['TEMPLATE=data/pds4/template_with_sp.xml'])

    ret = validate_xml(filename)
    assert ret, 'validation failed'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert '<Array_3D_Spectrum>' in data
    assert '<local_identifier>Spectral_Qube_Object</local_identifier>' in data

    gdal.GetDriverByName('PDS4').Delete(filename)



###############################################################################
# Test Oblique Cylindrical


def check_pds4_oblique_cylindrical(filename):
    ds = gdal.Open(filename)
    assert ds.GetSpatialRef().ExportToProj4().startswith('+proj=ob_tran +R=2575000 +o_proj=eqc +o_lon_p=-158.352054 +o_lat_p=191.769776 +lon_0=-163.331591 ')
    assert ds.GetGeoTransform() == pytest.approx((-3190898.22208, 0, 351.11116, -764017.88416, 351.11116, 0), rel=1e-8)

def test_pds4_oblique_cylindrical_read():
    check_pds4_oblique_cylindrical('data/pds4/oblique_cylindrical.xml')


def test_pds4_oblique_cylindrical_write():
    src_ds = gdal.Open('data/pds4/oblique_cylindrical.xml')
    filename = '/vsimem/out.xml'

    gdal.GetDriverByName('PDS4').CreateCopy(filename, src_ds)
    check_pds4_oblique_cylindrical(filename)

    gdal.GetDriverByName('PDS4').Delete(filename)

