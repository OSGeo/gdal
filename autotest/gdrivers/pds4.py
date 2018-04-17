#!/usr/bin/env python
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
import sys

sys.path.append('../pymod')

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest

###############################################################################
# Validate XML file against schemas


def validate_xml(filename):

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1800.xsd',
                                  force_download=True):
        return 'skip'

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/disp/v1/PDS4_DISP_1800.xsd',
                                  'pds.nasa.gov_pds4_disp_v1_PDS4_DISP_1800.xsd',
                                  force_download=True):
        return 'skip'

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1700.xsd',
                                  'pds.nasa.gov_pds4_pds_v1_PDS4_PDS_1700.xsd',
                                  force_download=True):
        return 'skip'

    if not gdaltest.download_file('https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.xsd',
                                  'pds.nasa.gov_pds4_cart_v1_PDS4_CART_1700.xsd',
                                  force_download=True):
        return 'skip'

    ds = gdal.OpenEx('GMLAS:' + filename, open_options=[
        'VALIDATE=YES',
        'FAIL_IF_VALIDATION_ERROR=YES',
        'CONFIG_FILE=<Configuration><AllowRemoteSchemaDownload>false</AllowRemoteSchemaDownload><SchemaCache><Directory>tmp/cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        return 'fail'
    return 'success'

###############################################################################
# Perform simple read test on PDS4 dataset.


def pds4_1():
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
    PARAMETER["false_northing",0]]
"""
    gt = (-59280.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    tst = gdaltest.GDALTest('PDS4', 'byte_pds4.xml', 1, 4672)
    return tst.testOpen(check_prj=srs, check_gt=gt)

###############################################################################
# hide_substitution_warnings_error_handler()


def hide_substitution_warnings_error_handler_cbk(type, errno, msg):
    if msg.find('substituted') < 0 and msg.find('VAR_TITLE not defined') < 0:
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


def pds4_2():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053)
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BSQ


def pds4_3():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BSQ'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BIP


def pds4_4():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BIP'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BIL


def pds4_5():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BIL'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BSQ and IMAGE_FORMAT=GEOTIFF


def pds4_6():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BSQ', 'IMAGE_FORMAT=GEOTIFF'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test CreateCopy() with explicit INTERLEAVE=BIP and IMAGE_FORMAT=GEOTIFF


def pds4_7():

    tst = gdaltest.GDALTest('PDS4', 'rgbsmall.tif', 2, 21053, options=['INTERLEAVE=BIP', 'IMAGE_FORMAT=GEOTIFF'])
    with hide_substitution_warnings_error_handler():
        ret = tst.testCreateCopy(vsimem=1, strict_in=1, quiet_error_handler=False)
    return ret

###############################################################################
# Test SRS support


def pds4_8():

    filename = '/vsimem/out.xml'
    for proj4 in ['+proj=eqc +lat_ts=43.75 +lat_0=10 +lon_0=-112.5 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',
                  '+proj=lcc +lat_1=10 +lat_0=10 +lon_0=-112.5 +k_0=0.9 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',  # LCC_1SP
                  '+proj=lcc +lat_1=9 +lat_2=11 +lat_0=10 +lon_0=-112.5 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',  # LCC_2SP
                  '+proj=omerc +lat_0=10 +lonc=11 +alpha=12 +k=0.9 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',  # Oblique Mercator Azimuth Center
                  '+proj=omerc +lat_0=10 +lon_1=11 +lat_1=12 +lon_2=13 +lat_2=14 +k=0.9 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',  # Oblique Mercator 2 points
                  '+proj=stere +lat_0=90 +lat_ts=90 +lon_0=10 +k=0.9 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',  # Polar Stereographic
                  '+proj=poly +lat_0=9 +lon_0=10 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',
                  '+proj=sinu +lon_0=10 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',
                  '+proj=tmerc +lat_0=11 +lon_0=10 +k=0.9 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs',
                  ]:
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1)
        sr = osr.SpatialReference()
        sr.ImportFromProj4(proj4)
        ds.SetProjection(sr.ExportToWkt())
        ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
        with gdaltest.error_handler():
            ds = None

        ret = validate_xml(filename)
        if ret == 'fail':
            gdaltest.post_reason('validation of file for %s failed' % proj4)
            return 'fail'

        ds = gdal.Open(filename)
        wkt = ds.GetProjectionRef()
        sr = osr.SpatialReference()
        sr.SetFromUserInput(wkt)
        got_proj4 = sr.ExportToProj4().strip()
        if got_proj4 != proj4:
            gdaltest.post_reason('got %s, expected %s' % (got_proj4, proj4))
            print('')
            print(got_proj4)
            print(proj4)
            return 'fail'

    # longlat doesn't roundtrip as such
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +a=2439400 +b=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    with gdaltest.error_handler():
        ds = None
    ds = gdal.Open(filename)
    wkt = ds.GetProjectionRef()
    sr = osr.SpatialReference()
    sr.SetFromUserInput(wkt)
    got_proj4 = sr.ExportToProj4().strip()
    proj4 = '+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +a=2439400 +b=2439400 +units=m +no_defs'
    if got_proj4 != proj4:
        gdaltest.post_reason('got %s, expected %s' % (got_proj4, proj4))
        print('')
        print(got_proj4)
        print(proj4)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (85151.12354629935, 42575.561773149675, 0.0, 2086202.5268843342, 0.0, -85151.12354629935)
    if max([abs(got_gt[i] - expected_gt[i]) for i in range(6)]) > 1:
        gdaltest.post_reason('fail')
        print('')
        print(got_gt)
        print(expected_gt)
        return 'fail'
    ds = None

    gdal.GetDriverByName('PDS4').Delete(filename)
    return 'success'

###############################################################################
# Test nodata / mask


def pds4_9():

    ds = gdal.Open('data/byte_pds4.xml')
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    if ndv != 74:
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 4800:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    ds = None

    filename = '/vsimem/out.xml'
    # Test copy of all specialConstants
    with hide_substitution_warnings_error_handler():
        gdal.Translate(filename, 'data/byte_pds4.xml', format='PDS4')

    ret = validate_xml(filename)
    if ret == 'fail':
        gdaltest.post_reason('validation failed')
        return 'fail'

    ds = gdal.Open(filename)
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    if ndv != 74:
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 4800:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    ds = None

    filename = '/vsimem/out.xml'
    # Test copy of all specialConstants and overide noData
    for format in ['RAW', 'GEOTIFF']:
        with hide_substitution_warnings_error_handler():
            gdal.Translate(filename, 'data/byte_pds4.xml', format='PDS4',
                           noData=75,
                           creationOptions=['IMAGE_FORMAT=' + format])

        ret = validate_xml(filename)
        if ret == 'fail':
            gdaltest.post_reason('validation failed')
            return 'fail'

        ds = gdal.Open(filename)
        ndv = ds.GetRasterBand(1).GetNoDataValue()
        if ndv != 75:
            gdaltest.post_reason('fail')
            print(ndv)
            return 'fail'

        flag = ds.GetRasterBand(1).GetMaskFlags()
        if flag != 0:
            gdaltest.post_reason('fail')
            print(flag)
            return 'fail'

        cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
        if cs != 4833:
            gdaltest.post_reason('fail')
            print(cs)
            return 'fail'

        ds = None

    # Test just setting noData
    for format in ['RAW', 'GEOTIFF']:
        with hide_substitution_warnings_error_handler():
            gdal.Translate(filename, 'data/byte_pds4.xml', format='PDS4',
                           creationOptions=['USE_SRC_LABEL=NO',
                                            'IMAGE_FORMAT=' + format])

        ret = validate_xml(filename)
        if ret == 'fail':
            gdaltest.post_reason('validation failed')
            return 'fail'

        ds = gdal.Open(filename)
        ndv = ds.GetRasterBand(1).GetNoDataValue()
        if ndv != 74:
            gdaltest.post_reason('fail')
            print(format)
            print(ndv)
            return 'fail'

        ds = None

        # Test filling with nodata
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                                 options=['IMAGE_FORMAT=' + format])
        ds.GetRasterBand(1).SetNoDataValue(1)
        with hide_substitution_warnings_error_handler():
            ds = None

        ds = gdal.Open(filename)
        cs = ds.GetRasterBand(1).Checksum()
        if cs != 1:
            gdaltest.post_reason('fail')
            print(format)
            print(cs)
            return 'fail'
        ds = None

        # Test setting nodata and then explicit Fill()
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                                 options=['IMAGE_FORMAT=' + format])
        ds.GetRasterBand(1).SetNoDataValue(10)
        ds.GetRasterBand(1).Fill(1)
        with hide_substitution_warnings_error_handler():
            ds = None

        ds = gdal.Open(filename)
        cs = ds.GetRasterBand(1).Checksum()
        if cs != 1:
            gdaltest.post_reason('fail')
            print(format)
            print(cs)
            return 'fail'
        ds = None

    template = '/vsimem/template.xml'

    # Empty Special_Constants
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
            <Special_Constants />
        </Array_2D>
    </File_Area_Observational>
</Product_Observational>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    ds.GetRasterBand(1).SetNoDataValue(10)
    with hide_substitution_warnings_error_handler():
        ds = None

    ds = gdal.Open(filename)
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    if ndv != 10:
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'
    ds = None

    ret = validate_xml(filename)
    if ret == 'fail':
        gdaltest.post_reason('validation failed')
        return 'fail'

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
                                             options=['TEMPLATE=' + template])
    ds.GetRasterBand(1).SetNoDataValue(10)
    with hide_substitution_warnings_error_handler():
        ds = None

    ds = gdal.Open(filename)
    ndv = ds.GetRasterBand(1).GetNoDataValue()
    if ndv != 10:
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'
    ds = None

    ret = validate_xml(filename)
    if ret == 'fail':
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdal.GetDriverByName('PDS4').Delete(filename)
    gdal.Unlink(template)

    return 'success'

###############################################################################
# Test scale / offset


def pds4_10():

    filename = '/vsimem/out.xml'
    filename2 = '/vsimem/out2.xml'
    for format in ['RAW', 'GEOTIFF']:
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                                 options=['IMAGE_FORMAT=' + format])
        ds.GetRasterBand(1).SetScale(2)
        ds.GetRasterBand(1).SetOffset(3)
        with hide_substitution_warnings_error_handler():
            ds = None
            gdal.Translate(filename2, filename, format='PDS4')

        ds = gdal.Open(filename2)
        scale = ds.GetRasterBand(1).GetScale()
        if scale != 2:
            gdaltest.post_reason('fail')
            print(scale)
            return 'fail'
        offset = ds.GetRasterBand(1).GetOffset()
        if offset != 3:
            gdaltest.post_reason('fail')
            print(offset)
            return 'fail'
        ds = None

        gdal.GetDriverByName('PDS4').Delete(filename)
        gdal.GetDriverByName('PDS4').Delete(filename2)

    return 'success'

###############################################################################
# Test various data types


def pds4_11():

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
        if ds.GetRasterBand(1).DataType != dt:
            gdaltest.post_reason('fail')
            print(ds.GetRasterBand(1).DataType)
            print(dt)
            return 'fail'
        got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
        if got_data != data:
            gdaltest.post_reason('fail')
            print(dt)
            return 'fail'
        cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
        if cs != 3:
            gdaltest.post_reason('fail')
            print(dt)
            print(cs)
            return 'fail'
        ds = None

    gdal.GetDriverByName('PDS4').Delete(filename)

    return 'success'

###############################################################################
# Test various creation options


def pds4_12():

    filename = '/vsimem/out.xml'
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['VAR_LOGICAL_IDENTIFIER=logical_identifier',
                                                      'VAR_TITLE=title',
                                                      'VAR_INVESTIGATION_AREA_NAME=ian',
                                                      'VAR_INVESTIGATION_AREA_LID_REFERENCE=ialr',
                                                      'VAR_OBSERVING_SYSTEM_NAME=osn',
                                                      'VAR_UNUSED=foo',
                                                      'TEMPLATE=data/byte_pds4.xml',
                                                      'BOUNDING_DEGREES=1,2,3,4',
                                                      'LATITUDE_TYPE=planetographic',
                                                      'LONGITUDE_DIRECTION=Positive West',
                                                      'IMAGE_FILENAME=/vsimem/myimage.raw'])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +a=2439400 +b=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    if data.find('<logical_identifier>logical_identifier</logical_identifier>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<cart:west_bounding_coordinate unit="deg">1</cart:west_bounding_coordinate>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<cart:east_bounding_coordinate unit="deg">3</cart:east_bounding_coordinate>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<cart:north_bounding_coordinate unit="deg">4</cart:north_bounding_coordinate>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<cart:south_bounding_coordinate unit="deg">2</cart:south_bounding_coordinate>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<cart:latitude_type>planetographic</cart:latitude_type>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<cart:longitude_direction>Positive West</cart:longitude_direction>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<file_name>myimage.raw</file_name>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.GetDriverByName('PDS4').Delete(filename)

    return 'success'

###############################################################################
# Test subdatasets


def pds4_13():

    ds = gdal.Open('data/byte_pds4_multi_sds.xml')
    subds = ds.GetSubDatasets()
    expected_subds = [('PDS4:data/byte_pds4_multi_sds.xml:1:1',
                       'Image file byte_pds4.img, array first_sds'),
                      ('PDS4:data/byte_pds4_multi_sds.xml:1:2',
                       'Image file byte_pds4.img, array second_sds'),
                      ('PDS4:data/byte_pds4_multi_sds.xml:2:1',
                       'Image file byte_pds4.img, array third_sds')]
    if subds != expected_subds:
        gdaltest.post_reason('fail')
        print(subds)
        return 'fail'

    ds = gdal.Open('PDS4:data/byte_pds4_multi_sds.xml:1:1')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 2315:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    ds = gdal.Open('PDS4:data/byte_pds4_multi_sds.xml:1:2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 2302:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    ds = gdal.Open('PDS4:data/byte_pds4_multi_sds.xml:2:1')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 3496:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    ds = gdal.Open(os.path.join(os.getcwd(), 'data', 'byte_pds4_multi_sds.xml'))
    subds_name = ds.GetSubDatasets()[0][0]
    ds = gdal.Open(subds_name)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:c:\do_not\exist.xml:1:1')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:i_do_not_exist.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:i_do_not_exist.xml:1:1')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:data/byte_pds4_multi_sds.xml:3:1')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('PDS4:data/byte_pds4_multi_sds.xml:1:3')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test error cases


def pds4_14():

    filename = '/vsimem/test.xml'

    gdal.FileFromMemBuffer(filename, "Product_Observational http://pds.nasa.gov/pds4/pds/v1")
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink(filename)

    # Invalid value for INTERLEAVE
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create('/vsimem/out.xml', 1, 1,
                                                 options=['INTERLEAVE=INVALID'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # INTERLEAVE=BIL not supported for GeoTIFF in PDS4
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create('/vsimem/out.xml', 1, 1,
                                                 options=['INTERLEAVE=BIL',
                                                          'IMAGE_FORMAT=GEOTIFF'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Cannot create GeoTIFF file
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create('/i/do_not/exist.xml', 1, 1,
                                                 options=['IMAGE_FORMAT=GEOTIFF'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Translate('/vsimem/test.tif', 'data/byte.tif')
    # Output file has same name as input file
    with gdaltest.error_handler():
        ds = gdal.Translate('/vsimem/test.xml', '/vsimem/test.tif',
                            format='PDS4', creationOptions=['IMAGE_FORMAT=GEOTIFF'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/test.tif')

    template = '/vsimem/template.xml'

    # Missing Product_Observational root
    gdal.FileFromMemBuffer(template, """<foo/>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    if gdal.GetLastErrorMsg() != 'Cannot find Product_Observational element in template':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Missing Target_Identification
    gdal.FileFromMemBuffer(template, """
<Product_Observational xmlns="http://pds.nasa.gov/pds4/pds/v1"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xsi:schemaLocation="http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd">
</Product_Observational>""")
    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                             options=['TEMPLATE=' + template])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +a=2439400 +b=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    if gdal.GetLastErrorMsg() != 'Cannot find Target_Identification element in template':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

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
    if gdal.GetLastErrorMsg() != 'Cannot find Observation_Area in template':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

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
    if gdal.GetLastErrorMsg() != 'Unexpected content found after Observation_Area in template':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.Unlink(template)
    gdal.Unlink(filename)
    gdal.Unlink('/vsimem/test.img')

    return 'success'

###############################################################################
# Test Create() without geospatial info but from a geospatial enabled template


def pds4_15():

    filename = '/vsimem/out.xml'
    with hide_substitution_warnings_error_handler():
        gdal.GetDriverByName('PDS4').Create(filename, 1, 1,
                                            options=['TEMPLATE=data/byte_pds4.xml'])

    ret = validate_xml(filename)
    if ret == 'fail':
        gdaltest.post_reason('validation failed')
        return 'fail'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    if data.find('<cart:Cartography>') >= 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.GetDriverByName('PDS4').Delete(filename)

    return 'success'

###############################################################################
# Test Create() with geospatial info but from a template without Discipline_Area


def pds4_16():

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
    sr.ImportFromProj4('+proj=longlat +a=2439400 +b=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    with hide_substitution_warnings_error_handler():
        ds = None

    ret = validate_xml(filename)
    if ret == 'fail':
        gdaltest.post_reason('validation failed')
        return 'fail'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    if data.find('http://pds.nasa.gov/pds4/pds/v1 https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd http://pds.nasa.gov/pds4/cart/v1 https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.xsd"') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('xmlns:cart="http://pds.nasa.gov/pds4/cart/v1"') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('<cart:Cartography>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.GetDriverByName('PDS4').Delete(filename)
    gdal.Unlink(template)

    return 'success'

###############################################################################
# Test ARRAY_TYPE creation option


def pds4_17():

    filename = '/vsimem/out.xml'

    with gdaltest.error_handler():
        gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1, options=['ARRAY_TYPE=Array_2D'])

    ret = validate_xml(filename)
    if ret == 'fail':
        gdaltest.post_reason('validation failed')
        return 'fail'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    if data.find('<Array_2D>') < 0 or data.find('<axes>2</axes>') < 0 or \
       data.find('<axis_name>Band</axis_name>') >= 0 or \
       data.find('<sequence_number>3</sequence_number>') >= 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.GetDriverByName('PDS4').Delete(filename)

    # Test multi-band creation with Array_2D
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 2, options=['ARRAY_TYPE=Array_2D'])
    if ds is not None:
        gdaltest.post_reason('expected failure')
        return 'fail'

    # Test multi-band creation with Array_3D_Spectrum
    with gdaltest.error_handler():
        gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 2, options=['ARRAY_TYPE=Array_3D_Spectrum'])

    ret = validate_xml(filename)
    if ret == 'fail':
        gdaltest.post_reason('validation failed')
        return 'fail'

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    if data.find('<Array_3D_Spectrum>') < 0 or data.find('<axes>3</axes>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.GetDriverByName('PDS4').Delete(filename)

    return 'success'

###############################################################################
# Test RADII creation option


def pds4_18():

    filename = '/vsimem/out.xml'

    ds = gdal.GetDriverByName('PDS4').Create(filename, 1, 1, 1, options=['RADII=1,2'])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=longlat +a=2439400 +b=2439400 +no_defs')
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -2])
    with gdaltest.error_handler():
        ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    if data.find('<cart:semi_major_radius unit="m">1</cart:semi_major_radius>') < 0 or \
       data.find('<cart:semi_minor_radius unit="m">1</cart:semi_minor_radius>') < 0 or \
       data.find('<cart:polar_radius unit="m">2</cart:polar_radius>') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.GetDriverByName('PDS4').Delete(filename)

    return 'success'


gdaltest_list = [
    pds4_1,
    pds4_2,
    pds4_3,
    pds4_4,
    pds4_5,
    pds4_6,
    pds4_7,
    pds4_8,
    pds4_9,
    pds4_10,
    pds4_11,
    pds4_12,
    pds4_13,
    pds4_14,
    pds4_15,
    pds4_16,
    pds4_17,
    pds4_18]

if __name__ == '__main__':

    gdaltest.setup_run('pds4')

    gdaltest.run_tests(gdaltest_list)

    gdaltest.summarize()
