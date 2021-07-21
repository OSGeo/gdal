#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Sentinel2 support.
# Author:   Even Rouault, <even.rouault at spatialys.com>
# Funded by: Centre National d'Etudes Spatiales (CNES)
#
###############################################################################
# Copyright (c) 2015, Even Rouault, <even.rouault at spatialys.com>
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
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Test opening a L1C product


def test_sentinel2_l1c_1():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUD_COVERAGE_ASSESSMENT': '0.0',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'FOOTPRINT': 'POLYGON((11 46, 11 45, 13 45, 13 46, 11 46))',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1C',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1C',
                   'QUANTIFICATION_VALUE': '1000',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution, UTM 32N',
                   'SUBDATASET_1_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:10m:EPSG_32632',
                   'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution, UTM 32N',
                   'SUBDATASET_2_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:20m:EPSG_32632',
                   'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution, UTM 32N',
                   'SUBDATASET_3_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:60m:EPSG_32632',
                   'SUBDATASET_4_DESC': 'RGB preview, UTM 32N',
                   'SUBDATASET_4_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:PREVIEW:EPSG_32632'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    # Try opening a zip file as distributed from https://scihub.esa.int/
    if not sys.platform.startswith('win'):
        os.system('sh -c "cd data/sentinel2/fake_l1c && zip -r ../../tmp/S2A_OPER_PRD_MSIL1C.zip S2A_OPER_PRD_MSIL1C.SAFE >/dev/null" && cd ../..')
        if os.path.exists('tmp/S2A_OPER_PRD_MSIL1C.zip'):
            ds = gdal.Open('tmp/S2A_OPER_PRD_MSIL1C.zip')
            assert ds is not None
            os.unlink('tmp/S2A_OPER_PRD_MSIL1C.zip')

    # Try opening the 4 subdatasets
    for i in range(4):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i + 1)])
        assert ds is not None and gdal.GetLastErrorMsg() == '', \
            got_md['SUBDATASET_%d_NAME' % (i + 1)]

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1C:',
                 'SENTINEL2_L1C:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L1C:%s' % filename_xml,
                 'SENTINEL2_L1C:%s:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L1C:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_32633' % filename_xml]:
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        assert ds is None, name

###############################################################################
# Test opening a L1C subdataset on the 10m bands


def test_sentinel2_l1c_2():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C:%s:10m:EPSG_32632' % filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUD_COVERAGE_ASSESSMENT': '0.0',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1C',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1C',
                   'QUANTIFICATION_VALUE': '1000',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 20984 and ds.RasterYSize == 20980

    assert ds.GetProjectionRef().find('32632') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0)

    assert ds.RasterCount == 4

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TQR_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TRQ_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TRQ_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="10004" yOff="10000" xSize="10980" ySize="10980" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B4',
                   'BANDWIDTH': '30',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1500',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '665',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.GetColorInterpretation() == gdal.GCI_RedBand

    assert band.DataType == gdal.GDT_UInt16

    assert band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '12'

    band = ds.GetRasterBand(4)

    assert band.GetColorInterpretation() == gdal.GCI_Undefined

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B8',
                   'BANDWIDTH': '115',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1000',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '842',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()


###############################################################################
# Test opening a L1C subdataset on the 60m bands and enabling alpha band


def test_sentinel2_l1c_3():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    ds = gdal.OpenEx('SENTINEL2_L1C:%s:60m:EPSG_32632' % filename_xml, open_options=['ALPHA=YES'])
    assert ds is not None

    assert ds.RasterCount == 4

    band = ds.GetRasterBand(4)

    assert band.GetColorInterpretation() == gdal.GCI_AlphaBand

    gdal.ErrorReset()
    cs = band.Checksum()
    assert cs == 0 and gdal.GetLastErrorMsg() == ''

    band.ReadRaster()

###############################################################################
# Test opening a L1C subdataset on the PREVIEW bands


def test_sentinel2_l1c_4():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    ds = gdal.OpenEx('SENTINEL2_L1C:%s:PREVIEW:EPSG_32632' % filename_xml)
    assert ds is not None

    assert ds.RasterCount == 3

    fl = ds.GetFileList()
    # main XML + 2 granule XML + 2 jp2
    if len(fl) != 1 + 2 + 2:
        import pprint
        pprint.pprint(fl)
        pytest.fail()

    band = ds.GetRasterBand(1)
    assert band.GetColorInterpretation() == gdal.GCI_RedBand

    assert band.DataType == gdal.GDT_Byte

###############################################################################
# Test opening invalid XML files


def test_sentinel2_l1c_5():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    assert ds is None

    # File is OK, but granule MTD are missing
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
    <n1:General_Info>
        <Product_Info>
<Query_Options>
<Band_List>
<BAND_NAME>B1</BAND_NAME>
<BAND_NAME>B2</BAND_NAME>
<BAND_NAME>B3</BAND_NAME>
<BAND_NAME>B4</BAND_NAME>
<BAND_NAME>B5</BAND_NAME>
<BAND_NAME>B6</BAND_NAME>
<BAND_NAME>B7</BAND_NAME>
<BAND_NAME>B8</BAND_NAME>
<BAND_NAME>B9</BAND_NAME>
<BAND_NAME>B10</BAND_NAME>
<BAND_NAME>B11</BAND_NAME>
<BAND_NAME>B12</BAND_NAME>
<BAND_NAME>B8A</BAND_NAME>
</Band_List>
</Query_Options>
<Product_Organisation>
                <Granule_List>
                    <Granules datastripIdentifier="S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_SY20151231T235959_N01.03" granuleIdentifier="S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_N01.03" imageFormat="JPEG2000">
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B01</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B06</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B10</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_T32TQR_B08</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B07</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B09</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B05</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B12</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B11</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B04</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B03</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B02</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_B8A</IMAGE_ID>
                    </Granules>
                </Granule_List>
                <Granule_List>
                    <Granules datastripIdentifier="S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_SY20151231T235959_N01.03" granuleIdentifier="S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_N01.03" imageFormat="JPEG2000">
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B01</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B06</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B10</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B08</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B07</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B09</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B05</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B12</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B11</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B04</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B03</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B02</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TRQ_B8A</IMAGE_ID>
                    </Granules>
                </Granule_List>
                <Granule_List>
                    <Granules/> <!-- missing granuleIdentifier -->
                </Granule_List>
                <Granule_List>
                    <Granules granuleIdentifier="foo"/> <!-- invalid id -->
                </Granule_List>
</Product_Organisation>
        </Product_Info>
    </n1:General_Info>
</n1:Level-1C_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.Open('/vsimem/test.xml')
    assert gdal.GetLastErrorMsg() != ''

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    assert ds is None

    # No Product_Info
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
    <n1:General_Info>
    </n1:General_Info>
</n1:Level-1C_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    assert ds is None

    # No Product_Organisation
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
    <n1:General_Info>
        <Product_Info/>
    </n1:General_Info>
</n1:Level-1C_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    assert ds is None

    # No Band_List
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
    <n1:General_Info>
        <Product_Info>
<Product_Organisation>
</Product_Organisation>
        </Product_Info>
    </n1:General_Info>
</n1:Level-1C_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    # No valid bands
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
    <n1:General_Info>
        <Product_Info>
<Query_Options>
<Band_List>
<BAND_NAME>Bxx</BAND_NAME>
</Band_List>
</Query_Options>
<Product_Organisation>
</Product_Organisation>
        </Product_Info>
    </n1:General_Info>
</n1:Level-1C_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    gdal.Unlink('/vsimem/test.xml')

###############################################################################
# Windows specific test to test support for long filenames


@pytest.mark.skipif(sys.platform != 'win32', reason='Incorrect platform')
def test_sentinel2_l1c_6():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    filename_xml = filename_xml.replace('/', '\\')
    filename_xml = '\\\\?\\' + os.getcwd() + '\\' + filename_xml
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    subds_name = ds.GetMetadataItem('SUBDATASET_1_NAME', 'SUBDATASETS')
    gdal.ErrorReset()
    ds = gdal.Open(subds_name)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

###############################################################################
# Test with a real JP2 tile


def test_sentinel2_l1c_7():

    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
    <n1:General_Info>
        <Product_Info>
            <Query_Options>
            <Band_List>
                <BAND_NAME>B1</BAND_NAME>
            </Band_List>
            </Query_Options>
            <Product_Organisation>
                <Granule_List>
                    <Granules datastripIdentifier="S2A_OPER_MSI_L1C_bla_N01.03" granuleIdentifier="S2A_OPER_MSI_L1C_bla_N01.03" imageFormat="JPEG2000">
                        <IMAGE_ID>S2A_OPER_MSI_L1C_bla_T32TQR_B01</IMAGE_ID>
                    </Granules>
                </Granule_List>
            </Product_Organisation>
        </Product_Info>
    </n1:General_Info>
</n1:Level-1C_User_Product>""")

    gdal.FileFromMemBuffer('/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/S2A_OPER_MTD_L1C_bla.xml',
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<n1:Level-1C_Tile_ID xmlns:n1="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_TILE_L1C/02.09.07/scripts/../../../schemas/02.11.06/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd">
<n1:General_Info>
<TILE_ID metadataLevel="Brief">S2A_OPER_MSI_L1C_bla_N01.03</TILE_ID>
</n1:General_Info>
<n1:Geometric_Info>
<Tile_Geocoding metadataLevel="Brief">
<HORIZONTAL_CS_NAME>WGS84 / UTM zone 53S</HORIZONTAL_CS_NAME>
<HORIZONTAL_CS_CODE>EPSG:32753</HORIZONTAL_CS_CODE>
<Size resolution="60">
<NROWS>1830</NROWS>
<NCOLS>1830</NCOLS>
</Size>
<Geoposition resolution="60">
<ULX>499980</ULX>
<ULY>7200040</ULY>
<XDIM>60</XDIM>
<YDIM>-60</YDIM>
</Geoposition>
</Tile_Geocoding>
</n1:Geometric_Info>
</n1:Level-1C_Tile_ID>
""")

    # Open with missing tile
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:60m:EPSG_32753')
    ds = None

    f = open('data/jpeg2000/gtsmall_10_uint16.jp2', 'rb')
    f2 = gdal.VSIFOpenL('/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_bla_B01.jp2', 'wb')
    data = f.read()
    gdal.VSIFWriteL(data, 1, len(data), f2)
    gdal.VSIFCloseL(f2)
    f.close()

    ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:60m:EPSG_32753')
    nbits = ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE')
    assert nbits == '10'

    gdal.Unlink('/vsimem/test.xml')
    gdal.Unlink('/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_bla_B01.jp2')

###############################################################################
# Test opening a L1C tile


def test_sentinel2_l1c_tile_1():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
                   'DATASTRIP_ID': 'S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_S20151231T235959_N01.03',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'DOWNLINK_PRIORITY': 'NOMINAL',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1C',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1C',
                   'QUANTIFICATION_VALUE': '1000',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SENSING_TIME': '2015-12-31T23:59:59.999Z',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0',
                   'TILE_ID': 'S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_N01.03'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution',
                   'SUBDATASET_1_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:10m',
                   'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution',
                   'SUBDATASET_2_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:20m',
                   'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution',
                   'SUBDATASET_3_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:60m',
                   'SUBDATASET_4_DESC': 'RGB preview',
                   'SUBDATASET_4_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:PREVIEW'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    # Try opening the 4 subdatasets
    for i in range(4):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i + 1)])
        assert ds is not None and gdal.GetLastErrorMsg() == '', \
            got_md['SUBDATASET_%d_NAME' % (i + 1)]

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1C_TILE:',
                 'SENTINEL2_L1C_TILE:foo.xml:10m',
                 'SENTINEL2_L1C_TILE:%s' % filename_xml,
                 'SENTINEL2_L1C_TILE:%s:' % filename_xml]:
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        assert ds is None, name


###############################################################################
# Test opening a L1C tile without main MTD file


def test_sentinel2_l1c_tile_2():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', 'NO')  # Simulate absence of main MTD file
    ds = gdal.Open(filename_xml)
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', None)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
                   'DATASTRIP_ID': 'S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_S20151231T235959_N01.03',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'DOWNLINK_PRIORITY': 'NOMINAL',
                   'SENSING_TIME': '2015-12-31T23:59:59.999Z',
                   'TILE_ID': 'S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_N01.03'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution',
                   'SUBDATASET_1_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:10m',
                   'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution',
                   'SUBDATASET_2_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:20m',
                   'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution',
                   'SUBDATASET_3_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:60m',
                   'SUBDATASET_4_DESC': 'RGB preview',
                   'SUBDATASET_4_NAME': 'SENTINEL2_L1C_TILE:data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:PREVIEW'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()


###############################################################################
# Test opening a L1C tile subdataset on the 10m bands


def test_sentinel2_l1c_tile_3():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C_TILE:%s:10m' % filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
                   'DATASTRIP_ID': 'S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_S20151231T235959_N01.03',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'DOWNLINK_PRIORITY': 'NOMINAL',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1C',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1C',
                   'QUANTIFICATION_VALUE': '1000',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SENSING_TIME': '2015-12-31T23:59:59.999Z',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0',
                   'TILE_ID': 'S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_N01.03'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 10980 and ds.RasterYSize == 10980

    assert ds.GetProjectionRef().find('32632') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0)

    assert ds.RasterCount == 4

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TQR_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B4',
                   'BANDWIDTH': '30',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1500',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '665',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.GetColorInterpretation() == gdal.GCI_RedBand

    assert band.DataType == gdal.GDT_UInt16

    assert band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '12'

    band = ds.GetRasterBand(4)

    assert band.GetColorInterpretation() == gdal.GCI_Undefined

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B8',
                   'BANDWIDTH': '115',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1000',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '842',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()


###############################################################################
# Test opening a L1C tile subdataset on the 10m bands without main MTD file


def test_sentinel2_l1c_tile_4():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', 'NO')  # Simulate absence of main MTD file
    ds = gdal.OpenEx('SENTINEL2_L1C_TILE:%s:10m' % filename_xml, open_options=['ALPHA=YES'])
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', None)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
                   'DATASTRIP_ID': 'S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_S20151231T235959_N01.03',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'DOWNLINK_PRIORITY': 'NOMINAL',
                   'SENSING_TIME': '2015-12-31T23:59:59.999Z',
                   'TILE_ID': 'S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_N01.03'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 10980 and ds.RasterYSize == 10980

    assert ds.GetProjectionRef().find('32632') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0)

    assert ds.RasterCount == 5

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TQR_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B4',
                   'BANDWIDTH': '30',
                   'BANDWIDTH_UNIT': 'nm',
                   'WAVELENGTH': '665',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.GetColorInterpretation() == gdal.GCI_RedBand

    assert band.DataType == gdal.GDT_UInt16

    assert band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '12'

    band = ds.GetRasterBand(5)

    assert band.GetColorInterpretation() == gdal.GCI_AlphaBand

###############################################################################
# Test opening a L1C tile subdataset on the preview bands


def test_sentinel2_l1c_tile_5():

    filename_xml = 'data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C_TILE:%s:PREVIEW' % filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    assert ds.RasterXSize == 343 and ds.RasterYSize == 343

    assert ds.GetProjectionRef().find('32632') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (699960.0, 320.0, 0.0, 5100060.0, 0.0, -320.0)

    assert ds.RasterCount == 3

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/QI_DATA/S2A_OPER_PVI_L1C_T32TQR.jp2</SourceFilename>
      <SourceBand>3</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="343" ySize="343" />
      <DstRect xOff="0" yOff="0" xSize="343" ySize="343" />
    </SimpleSource>"""
    assert placement_vrt in vrt

###############################################################################
# Test opening invalid XML files


def test_sentinel2_l1c_tile_6():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1C_Tile_ID xmlns:n1="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_TILE_L1C/02.09.07/scripts/../../../schemas/02.11.06/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C_TILE:/vsimem/test.xml:10m')
    assert ds is None

    gdal.FileFromMemBuffer('/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/S2A_OPER_MTD_L1C_bla.xml',
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<n1:Level-1C_Tile_ID xmlns:n1="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_TILE_L1C/02.09.07/scripts/../../../schemas/02.11.06/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd">
<n1:General_Info>
<TILE_ID metadataLevel="Brief">S2A_OPER_MSI_L1C_bla_N01.03</TILE_ID>
</n1:General_Info>
<n1:Geometric_Info>
<Tile_Geocoding metadataLevel="Brief">
<HORIZONTAL_CS_NAME>WGS84 / UTM zone 53S</HORIZONTAL_CS_NAME>
<HORIZONTAL_CS_CODE>EPSG:32753</HORIZONTAL_CS_CODE>
<Size resolution="60">
<NROWS>1830</NROWS>
<NCOLS>1830</NCOLS>
</Size>
<Geoposition resolution="60">
<ULX>499980</ULX>
<ULY>7200040</ULY>
<XDIM>60</XDIM>
<YDIM>-60</YDIM>
</Geoposition>
</Tile_Geocoding>
</n1:Geometric_Info>
</n1:Level-1C_Tile_ID>
""")

    # Just tell it doesn't crash without any tile
    gdal.Open('/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/S2A_OPER_MTD_L1C_bla.xml')

    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C_TILE:/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/S2A_OPER_MTD_L1C_bla.xml:10m')
    assert ds is None

    gdal.Unlink('/vsimem/test.xml')
    gdal.Unlink('/vsimem/S2A_OPER_MSI_L1C_bla_N01.03/S2A_OPER_MTD_L1C_bla.xml')

###############################################################################
# Test opening a L1B product


def test_sentinel2_l1b_1():

    filename_xml = 'data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/S2B_OPER_MTD_SAFL1B.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUD_COVERAGE_ASSESSMENT': '0.0',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2B_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2B',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'FOOTPRINT': 'POLYGON((11 46, 11 45, 13 45, 13 46, 11 46))',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1B',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1B',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 of granule S2B_OPER_MTD_L1B.xml with 10m resolution',
                   'SUBDATASET_1_NAME': 'SENTINEL2_L1B:data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:10m',
                   'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 of granule S2B_OPER_MTD_L1B.xml with 20m resolution',
                   'SUBDATASET_2_NAME': 'SENTINEL2_L1B:data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:20m',
                   'SUBDATASET_3_DESC': 'Bands B1, B9, B10 of granule S2B_OPER_MTD_L1B.xml with 60m resolution',
                   'SUBDATASET_3_NAME': 'SENTINEL2_L1B:data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    # Try opening the 3 subdatasets
    for i in range(3):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i + 1)])
        assert ds is not None and gdal.GetLastErrorMsg() == '', \
            got_md['SUBDATASET_%d_NAME' % (i + 1)]

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1B:',
                 'SENTINEL2_L1B:foo.xml:10m',
                 'SENTINEL2_L1B:%s' % filename_xml,
                 'SENTINEL2_L1B:%s:' % filename_xml,
                 'SENTINEL2_L1B:%s:30m' % filename_xml]:
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        assert ds is None, name


###############################################################################
# Test opening a L1B granule


def test_sentinel2_l1b_2():

    filename_xml = 'data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
                   'DATASTRIP_ID': 'S2B_OPER_MSI_L1B_DS_MTI__20151231T235959_S20151231T235959_N01.03',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2B_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2B',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'DETECTOR_ID': '02',
                   'DOWNLINK_PRIORITY': 'NOMINAL',
                   'FOOTPRINT': 'POLYGON((11 46 1, 11 45 2, 13 45 3, 13 46 4, 11 46 1))',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'GRANULE_ID': 'S2B_OPER_MSI_L1B_GR_MTI__20151231T235959_S20151231T235959_D02_N01.03',
                   'INCIDENCE_AZIMUTH_ANGLE': '96',
                   'INCIDENCE_ZENITH_ANGLE': '8',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1B',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1B',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'SENSING_TIME': '2015-12-31T23:59:59.999Z',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SOLAR_AZIMUTH_ANGLE': '158',
                   'SOLAR_ZENITH_ANGLE': '43',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    subdatasets_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution',
                      'SUBDATASET_1_NAME': 'SENTINEL2_L1B:data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:10m',
                      'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution',
                      'SUBDATASET_2_NAME': 'SENTINEL2_L1B:data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:20m',
                      'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution',
                      'SUBDATASET_3_NAME': 'SENTINEL2_L1B:data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != subdatasets_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    cwd = os.getcwd()
    gdal.ErrorReset()
    try:
        os.chdir('data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03')
        ds = gdal.Open('S2B_OPER_MTD_L1B.xml')
    finally:
        os.chdir(cwd)
    assert ds is not None and gdal.GetLastErrorMsg() == ''
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()


###############################################################################
# Test opening a L1B subdataset


def test_sentinel2_l1b_3():

    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1B:data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m')
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
                   'DATASTRIP_ID': 'S2B_OPER_MSI_L1B_DS_MTI__20151231T235959_S20151231T235959_N01.03',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2B_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2B',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'DETECTOR_ID': '02',
                   'DOWNLINK_PRIORITY': 'NOMINAL',
                   'FOOTPRINT': 'POLYGON((11 46 1, 11 45 2, 13 45 3, 13 46 4, 11 46 1))',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'GRANULE_ID': 'S2B_OPER_MSI_L1B_GR_MTI__20151231T235959_S20151231T235959_D02_N01.03',
                   'INCIDENCE_AZIMUTH_ANGLE': '96',
                   'INCIDENCE_ZENITH_ANGLE': '8',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1B',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1B',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'SENSING_TIME': '2015-12-31T23:59:59.999Z',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SOLAR_AZIMUTH_ANGLE': '158',
                   'SOLAR_ZENITH_ANGLE': '43',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 1276 and ds.RasterYSize == 384

    assert ds.GetGCPProjection().find('4326') >= 0

    gcps = ds.GetGCPs()
    assert len(gcps) == 5

    assert (gcps[0].GCPPixel == 0 and \
       gcps[0].GCPLine == 0 and \
       gcps[0].GCPX == 11 and \
       gcps[0].GCPY == 46 and \
       gcps[0].GCPZ == 1)

    assert (gcps[1].GCPPixel == 0 and \
       gcps[1].GCPLine == 384 and \
       gcps[1].GCPX == 11 and \
       gcps[1].GCPY == 45 and \
       gcps[1].GCPZ == 2)

    assert (gcps[2].GCPPixel == 1276 and \
       gcps[2].GCPLine == 384 and \
       gcps[2].GCPX == 13 and \
       gcps[2].GCPY == 45 and \
       gcps[2].GCPZ == 3)

    assert (gcps[3].GCPPixel == 1276 and \
       gcps[3].GCPLine == 0 and \
       gcps[3].GCPX == 13 and \
       gcps[3].GCPY == 46 and \
       gcps[3].GCPZ == 4)

    assert (gcps[4].GCPPixel == 1276. / 2 and \
       gcps[4].GCPLine == 384. / 2 and \
       gcps[4].GCPX == 12 and \
       gcps[4].GCPY == 45.5 and \
       gcps[4].GCPZ == 2.5)

    assert ds.RasterCount == 3

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/IMG_DATA/S2B_OPER_MSI_L1B_B01.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="1276" ySize="384" />
      <DstRect xOff="0" yOff="0" xSize="1276" ySize="384" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B1',
                   'BANDWIDTH': '20',
                   'BANDWIDTH_UNIT': 'nm',
                   'WAVELENGTH': '443',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.DataType == gdal.GDT_UInt16

    assert band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '12'

###############################################################################
# Test opening a L1B granule (with missing tile, without any ../../main_mtd.xml)


def test_sentinel2_l1b_4():

    gdal.FileFromMemBuffer('/vsimem/foo/S2B_PROD_MTD_foo.xml',
                           """<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
    <n1:General_Info>
        <Product_Info>
<Query_Options>
<Band_List>
<BAND_NAME>B1</BAND_NAME>
</Band_List>
</Query_Options>
<Product_Organisation>
</Product_Organisation>
        </Product_Info>
    </n1:General_Info>
</n1:Level-1B_User_Product>""")

    gdal.FileFromMemBuffer('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml',
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
<n1:General_Info>
<TILE_ID metadataLevel="Brief">S2A_OPER_MSI_L1C_bla_N01.03</TILE_ID>
</n1:General_Info>
<n1:Geometric_Info>
<Granule_Dimensions metadataLevel="Brief">
<Size resolution="60">
<NROWS>1830</NROWS>
<NCOLS>1830</NCOLS>
</Size>
</Granule_Dimensions>
</n1:Geometric_Info>
</n1:Level-1B_Granule_ID>
""")

    # Open with missing tile
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m')
    ds = None

    # Now open with missing main MTD
    gdal.Unlink('/vsimem/foo/S2B_PROD_MTD_foo.xml')

    f = open('data/jpeg2000/gtsmall_10_uint16.jp2', 'rb')
    f2 = gdal.VSIFOpenL('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/IMG_DATA/S2B_OPER_MSI_L1B_B01.jp2', 'wb')
    data = f.read()
    gdal.VSIFWriteL(data, 1, len(data), f2)
    gdal.VSIFCloseL(f2)
    f.close()

    # With brief granule metadata (no Granule_Dimensions)
    gdal.FileFromMemBuffer('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml',
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
<n1:General_Info>
<TILE_ID metadataLevel="Brief">S2A_OPER_MSI_L1C_bla_N01.03</TILE_ID>
</n1:General_Info>
<n1:Geometric_Info>
</n1:Geometric_Info>
</n1:Level-1B_Granule_ID>
""")
    ds = gdal.Open('SENTINEL2_L1B:/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m')
    assert ds.RasterXSize == 500

    # With standard granule metadata (with Granule_Dimensions)
    gdal.FileFromMemBuffer('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml',
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
<n1:General_Info>
<TILE_ID metadataLevel="Brief">S2A_OPER_MSI_L1C_bla_N01.03</TILE_ID>
</n1:General_Info>
<n1:Geometric_Info>
<Granule_Dimensions metadataLevel="Brief">
<Size resolution="60">
<NROWS>1830</NROWS>
<NCOLS>1830</NCOLS>
</Size>
</Granule_Dimensions>
</n1:Geometric_Info>
</n1:Level-1B_Granule_ID>
""")

    ds = gdal.Open('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml')
    expected_md = {'SUBDATASET_1_DESC': 'Bands B1 with 60m resolution',
                   'SUBDATASET_1_NAME': 'SENTINEL2_L1B:/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()
    ds = None

    ds = gdal.OpenEx('SENTINEL2_L1B:/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m', open_options=['ALPHA=YES'])
    assert ds is not None
    assert ds.RasterCount == 2
    ds = None

    gdal.Unlink('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml')
    gdal.Unlink('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/IMG_DATA/S2B_OPER_MSI_L1B_B01.jp2')

###############################################################################
# Test opening invalid XML files


def test_sentinel2_l1b_5():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    # No Product_Info
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
    <n1:General_Info>
    </n1:General_Info>
</n1:Level-1B_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    # No Product_Organisation
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
    <n1:General_Info>
        <Product_Info/>
    </n1:General_Info>
</n1:Level-1B_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    assert ds is None

    # No Band_List
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
    <n1:General_Info>
        <Product_Info>
<Product_Organisation>
</Product_Organisation>
        </Product_Info>
    </n1:General_Info>
</n1:Level-1B_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    # No valid bands
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
    <n1:General_Info>
        <Product_Info>
<Query_Options>
<Band_List>
<BAND_NAME>Bxx</BAND_NAME>
</Band_List>
</Query_Options>
<Product_Organisation>
</Product_Organisation>
        </Product_Info>
    </n1:General_Info>
</n1:Level-1B_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    assert ds is None

    # No Granule_Dimensions
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
  <n1:General_Info>
  </n1:General_Info>
  <n1:Geometric_Info>
  </n1:Geometric_Info>
</n1:Level-1B_Granule_ID>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    assert ds is None

    # No ROWS
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
  <n1:General_Info>
  </n1:General_Info>
  <n1:Geometric_Info>
<Granule_Dimensions metadataLevel="Standard">
<Size resolution="10">
<xNROWS>2304</xNROWS>
<NCOLS>2552</NCOLS>
</Size>
</Granule_Dimensions>
  </n1:Geometric_Info>
</n1:Level-1B_Granule_ID>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    assert ds is None

    # No NCOLS
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
  <n1:General_Info>
  </n1:General_Info>
  <n1:Geometric_Info>
<Granule_Dimensions metadataLevel="Standard">
<Size resolution="10">
<NROWS>2304</NROWS>
<xNCOLS>2552</xNCOLS>
</Size>
</Granule_Dimensions>
  </n1:Geometric_Info>
</n1:Level-1B_Granule_ID>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    assert ds is None

    # Not the desired resolution
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
  <n1:General_Info>
  </n1:General_Info>
  <n1:Geometric_Info>
<Granule_Dimensions metadataLevel="Standard">
</Granule_Dimensions>
  </n1:Geometric_Info>
</n1:Level-1B_Granule_ID>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    assert ds is None
    gdal.Unlink('/vsimem/test.xml')

    gdal.Unlink('/vsimem/test.xml')

###############################################################################
# Test opening a L2A product


def test_sentinel2_l2a_1():

    filename_xml = 'data/sentinel2/fake_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'AOT_RETRIEVAL_ACCURACY': '0',
                   'BARE_SOILS_PERCENTAGE': '0',
                   'CLOUD_COVERAGE_ASSESSMENT': '0.0',
                   'CLOUD_SHADOW_PERCENTAGE': '0',
                   'DARK_FEATURES_PERCENTAGE': '0',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'FOOTPRINT': 'POLYGON((11 46, 11 45, 13 45, 13 46, 11 46))',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'HIGH_PROBA_CLOUDS_PERCENTAGE': '0',
                   'L1C_TOA_QUANTIFICATION_VALUE': '1000',
                   'L1C_TOA_QUANTIFICATION_VALUE_UNIT': 'none',
                   'L2A_AOT_QUANTIFICATION_VALUE': '1000.0',
                   'L2A_AOT_QUANTIFICATION_VALUE_UNIT': 'none',
                   'L2A_BOA_QUANTIFICATION_VALUE': '1000',
                   'L2A_BOA_QUANTIFICATION_VALUE_UNIT': 'none',
                   'L2A_WVP_QUANTIFICATION_VALUE': '1000.0',
                   'L2A_WVP_QUANTIFICATION_VALUE_UNIT': 'cm',
                   'LOW_PROBA_CLOUDS_PERCENTAGE': '0',
                   'MEDIUM_PROBA_CLOUDS_PERCENTAGE': '0',
                   'NODATA_PIXEL_PERCENTAGE': '0',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-2Ap',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI2Ap',
                   'RADIATIVE_TRANSFER_ACCURAY': '0',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SATURATED_DEFECTIVE_PIXEL_PERCENTAGE': '0',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SNOW_ICE_PERCENTAGE': '0',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0',
                   'THIN_CIRRUS_PERCENTAGE': '0',
                   'VEGETATION_PERCENTAGE': '0',
                   'WATER_PERCENTAGE': '0',
                   'WATER_VAPOUR_RETRIEVAL_ACCURACY': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {'SUBDATASET_1_DESC': 'Bands B1, B2, B3, B4, B5, B6, B7, B9, B10, B11, B12, B8A, AOT, CLD, SCL, SNW, WVP with 60m resolution, UTM 32N',
                   'SUBDATASET_1_NAME': 'SENTINEL2_L2A:data/sentinel2/fake_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml:60m:EPSG_32632',
                   'SUBDATASET_2_DESC': 'RGB preview, UTM 32N',
                   'SUBDATASET_2_NAME': 'SENTINEL2_L2A:data/sentinel2/fake_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml:PREVIEW:EPSG_32632'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    # Try opening the 4 subdatasets
    for i in range(2):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i + 1)])
        assert ds is not None and gdal.GetLastErrorMsg() == '', \
            got_md['SUBDATASET_%d_NAME' % (i + 1)]

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L2A:',
                 'SENTINEL2_L2A:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L2A:%s' % filename_xml,
                 'SENTINEL2_L2A:%s:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L2A:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_32633' % filename_xml]:
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        assert ds is None, name


###############################################################################
# Test opening a L21 subdataset on the 60m bands


def test_sentinel2_l2a_2():

    filename_xml = 'data/sentinel2/fake_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L2A:%s:60m:EPSG_32632' % filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'AOT_RETRIEVAL_ACCURACY': '0',
                   'BARE_SOILS_PERCENTAGE': '0',
                   'CLOUD_COVERAGE_ASSESSMENT': '0.0',
                   'CLOUD_SHADOW_PERCENTAGE': '0',
                   'DARK_FEATURES_PERCENTAGE': '0',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'HIGH_PROBA_CLOUDS_PERCENTAGE': '0',
                   'L1C_TOA_QUANTIFICATION_VALUE': '1000',
                   'L1C_TOA_QUANTIFICATION_VALUE_UNIT': 'none',
                   'L2A_AOT_QUANTIFICATION_VALUE': '1000.0',
                   'L2A_AOT_QUANTIFICATION_VALUE_UNIT': 'none',
                   'L2A_BOA_QUANTIFICATION_VALUE': '1000',
                   'L2A_BOA_QUANTIFICATION_VALUE_UNIT': 'none',
                   'L2A_WVP_QUANTIFICATION_VALUE': '1000.0',
                   'L2A_WVP_QUANTIFICATION_VALUE_UNIT': 'cm',
                   'LOW_PROBA_CLOUDS_PERCENTAGE': '0',
                   'MEDIUM_PROBA_CLOUDS_PERCENTAGE': '0',
                   'NODATA_PIXEL_PERCENTAGE': '0',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-2Ap',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI2Ap',
                   'RADIATIVE_TRANSFER_ACCURAY': '0',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SATURATED_DEFECTIVE_PIXEL_PERCENTAGE': '0',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SNOW_ICE_PERCENTAGE': '0',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0',
                   'THIN_CIRRUS_PERCENTAGE': '0',
                   'VEGETATION_PERCENTAGE': '0',
                   'WATER_PERCENTAGE': '0',
                   'WATER_VAPOUR_RETRIEVAL_ACCURACY': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 1830 and ds.RasterYSize == 1830

    assert ds.GetProjectionRef().find('32632') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (699960.0, 60.0, 0.0, 5100060.0, 0.0, -60.0)

    assert ds.RasterCount == 17

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l2a/S2A_USER_PRD_MSIL2A.SAFE/GRANULE/S2A_USER_MSI_L2A_T32TQR_N01.03/IMG_DATA/R60m/S2A_USER_MSI_L2A_T32TQR_B01_60m.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
      <DstRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B1',
                   'BANDWIDTH': '20',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1900',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '443',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.DataType == gdal.GDT_UInt16

    band = ds.GetRasterBand(13)

    assert band.GetColorInterpretation() == gdal.GCI_Undefined

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'AOT'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    scl_band = 0
    for i in range(ds.RasterCount):
        if ds.GetRasterBand(i + 1).GetMetadataItem('BANDNAME') == 'SCL':
            scl_band = i + 1
    assert scl_band != 0
    band = ds.GetRasterBand(scl_band)
    expected_categories = ['NODATA',
                           'SATURATED_DEFECTIVE',
                           'DARK_FEATURE_SHADOW',
                           'CLOUD_SHADOW',
                           'VEGETATION',
                           'BARE_SOIL_DESERT',
                           'WATER',
                           'CLOUD_LOW_PROBA',
                           'CLOUD_MEDIUM_PROBA',
                           'CLOUD_HIGH_PROBA',
                           'THIN_CIRRUS',
                           'SNOW_ICE']
    got_categories = band.GetCategoryNames()
    if got_categories != expected_categories:
        import pprint
        pprint.pprint(got_categories)
        pytest.fail()


###############################################################################
# Test opening invalid XML files


def test_sentinel2_l2a_3():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-2A_User_Product xmlns:n1="https://psd-12.sentinel2.eo.esa.int/PSD/User_Product_Level-2A.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-2A.xsd S2_User_Product_Level-2A_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L2A:/vsimem/test.xml:10m:EPSG_32632')
    assert ds is None

    # File is OK, but granule MTD are missing
    gdal.FileFromMemBuffer('/vsimem/test.xml',
                           """<n1:Level-2A_User_Product xmlns:n1="https://psd-12.sentinel2.eo.esa.int/PSD/User_Product_Level-2A.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-2A.xsd S2_User_Product_Level-2A_Metadata.xsd">
    <n1:General_Info>
        <L2A_Product_Info>
<Query_Options>
<Band_List>
<BAND_NAME>B1</BAND_NAME>
<BAND_NAME>B2</BAND_NAME>
<BAND_NAME>B3</BAND_NAME>
<BAND_NAME>B4</BAND_NAME>
<BAND_NAME>B5</BAND_NAME>
<BAND_NAME>B6</BAND_NAME>
<BAND_NAME>B7</BAND_NAME>
<BAND_NAME>B8</BAND_NAME>
<BAND_NAME>B9</BAND_NAME>
<BAND_NAME>B10</BAND_NAME>
<BAND_NAME>B11</BAND_NAME>
<BAND_NAME>B12</BAND_NAME>
<BAND_NAME>B8A</BAND_NAME>
</Band_List>
</Query_Options>
<L2A_Product_Organisation>
                <Granule_List>
                    <Granules datastripIdentifier="S2A_OPER_MSI_L2A_DS_MTI__20151231T235959_SY20151231T235959_N01.03" granuleIdentifier="S2A_OPER_MSI_L2A_TL_MTI__20151231T235959_A000123_T32TQR_N01.03" imageFormat="JPEG2000">
                        <IMAGE_ID_2A>S2A_OPER_MSI_L2A_TL_MTI__20151231T235959_A000123_T32TQR_B01_60m</IMAGE_ID_2A>
                    </Granules>
                </Granule_List>
                <Granule_List>
                    <Granules/> <!-- missing granuleIdentifier -->
                </Granule_List>
                <Granule_List>
                    <Granules granuleIdentifier="foo"/> <!-- invalid id -->
                </Granule_List>
</L2A_Product_Organisation>
        </L2A_Product_Info>
    </n1:General_Info>
</n1:Level-2A_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.Open('/vsimem/test.xml')
    assert gdal.GetLastErrorMsg() != ''

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L2A:/vsimem/test.xml:10m:EPSG_32632')
    assert ds is None

    gdal.Unlink('/vsimem/test.xml')

###############################################################################
# Test opening a L2A MSIL2A product


def test_sentinel2_l2a_4():

    filename_xml = 'data/sentinel2/fake_l2a_MSIL2A/S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE/MTD_MSIL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {
                    'AOT_QUANTIFICATION_VALUE': '1000.0',
                    'AOT_QUANTIFICATION_VALUE_UNIT': 'none',
                    'AOT_RETRIEVAL_ACCURACY': '0.0',
                    'BOA_QUANTIFICATION_VALUE': '10000',
                    'BOA_QUANTIFICATION_VALUE_UNIT': 'none',
                    'CLOUD_COVERAGE_ASSESSMENT': '54.4',
                    'CLOUD_SHADOW_PERCENTAGE': '1.5',
                    'DARK_FEATURES_PERCENTAGE': '1.5',
                    'DATATAKE_1_DATATAKE_SENSING_START': '2018-08-18T09:40:31.024Z',
                    'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                    'DATATAKE_1_ID': 'GS2A_20180818T094031_016478_N02.08',
                    'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                    'DATATAKE_1_SENSING_ORBIT_NUMBER': '36',
                    'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                    'DEGRADED_ANC_DATA_PERCENTAGE': '0.0',
                    'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                    'FOOTPRINT': 'POLYGON((22.6 57.7, 24.5 57.6, 24.4 56.7, 22.6 56.7, 22.6 57.7))',
                    'FORMAT_CORRECTNESS': 'PASSED',
                    'GENERAL_QUALITY': 'PASSED',
                    'GENERATION_TIME': '2018-08-18T12:03:45.000000Z',
                    'GEOMETRIC_QUALITY': 'PASSED',
                    'HIGH_PROBA_CLOUDS_PERCENTAGE': '15.3',
                    'MEDIUM_PROBA_CLOUDS_PERCENTAGE': '24.1',
                    'NODATA_PIXEL_PERCENTAGE': '0.0',
                    'NOT_VEGETATED_PERCENTAGE': '3.5',
                    'PREVIEW_GEO_INFO': 'Not applicable',
                    'PREVIEW_IMAGE_URL': 'Not applicable',
                    'PROCESSING_BASELINE': '02.08',
                    'PROCESSING_LEVEL': 'Level-2A',
                    'PRODUCT_START_TIME': '2018-08-18T09:40:31.024Z',
                    'PRODUCT_STOP_TIME': '2018-08-18T09:40:31.024Z',
                    'PRODUCT_TYPE': 'S2MSI2A',
                    'PRODUCT_URI': 'S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE',
                    'RADIATIVE_TRANSFER_ACCURACY': '0.0',
                    'RADIOMETRIC_QUALITY': 'PASSED',
                    'REFLECTANCE_CONVERSION_U': '0.97',
                    'SATURATED_DEFECTIVE_PIXEL_PERCENTAGE': '0.0',
                    'SENSOR_QUALITY': 'PASSED',
                    'SNOW_ICE_PERCENTAGE': '0.4',
                    'SPECIAL_VALUE_NODATA': '0',
                    'SPECIAL_VALUE_SATURATED': '6',
                    'THIN_CIRRUS_PERCENTAGE': '14.9',
                    'UNCLASSIFIED_PERCENTAGE': '5.7',
                    'VEGETATION_PERCENTAGE': '14.0',
                    'WATER_PERCENTAGE': '18.7',
                    'WATER_VAPOUR_RETRIEVAL_ACCURACY': '0.0',
                    'WVP_QUANTIFICATION_VALUE': '1000.0',
                    'WVP_QUANTIFICATION_VALUE_UNIT': 'cm'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {
                    'SUBDATASET_1_DESC':
                    'Bands B2, B3, B4, B8 with 10m resolution, UTM 34N',
                    'SUBDATASET_1_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2A/S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634',
                    'SUBDATASET_2_DESC':
                    'Bands B5, B6, B7, B8A, B11, B12, AOT, CLD, SCL, SNW, WVP with 20m resolution, UTM 34N',
                    'SUBDATASET_2_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2A/S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE/MTD_MSIL2A.xml:20m:EPSG_32634',
                    'SUBDATASET_3_DESC':
                    'Bands B1, B9, AOT, CLD, SCL, SNW, WVP with 60m resolution, UTM 34N',
                    'SUBDATASET_3_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2A/S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE/MTD_MSIL2A.xml:60m:EPSG_32634',
                    'SUBDATASET_4_DESC':
                    'True color image, UTM 34N',
                    'SUBDATASET_4_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2A/S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE/MTD_MSIL2A.xml:TCI:EPSG_32634'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    # Try opening the 4 subdatasets
    for i in range(2):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i + 1)])
        assert ds is not None and gdal.GetLastErrorMsg() == '', \
            got_md['SUBDATASET_%d_NAME' % (i + 1)]

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L2A:',
                 'SENTINEL2_L2A:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L2A:%s' % filename_xml,
                 'SENTINEL2_L2A:%s:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L2A:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_32633' % filename_xml]:
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        assert ds is None, name



###############################################################################
# Test opening a L2A MSIL2A subdataset on the 60m bands


def test_sentinel2_l2a_5():

    filename_xml = 'data/sentinel2/fake_l2a_MSIL2A/S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE/MTD_MSIL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L2A:%s:60m:EPSG_32634' % filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {
                    'AOT_QUANTIFICATION_VALUE': '1000.0',
                    'AOT_QUANTIFICATION_VALUE_UNIT': 'none',
                    'AOT_RETRIEVAL_ACCURACY': '0.0',
                    'BOA_QUANTIFICATION_VALUE': '10000',
                    'BOA_QUANTIFICATION_VALUE_UNIT': 'none',
                    'CLOUD_COVERAGE_ASSESSMENT': '54.4',
                    'CLOUD_SHADOW_PERCENTAGE': '1.5',
                    'DARK_FEATURES_PERCENTAGE': '1.5',
                    'DATATAKE_1_DATATAKE_SENSING_START': '2018-08-18T09:40:31.024Z',
                    'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                    'DATATAKE_1_ID': 'GS2A_20180818T094031_016478_N02.08',
                    'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                    'DATATAKE_1_SENSING_ORBIT_NUMBER': '36',
                    'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                    'DEGRADED_ANC_DATA_PERCENTAGE': '0.0',
                    'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                    'FORMAT_CORRECTNESS': 'PASSED',
                    'GENERAL_QUALITY': 'PASSED',
                    'GENERATION_TIME': '2018-08-18T12:03:45.000000Z',
                    'GEOMETRIC_QUALITY': 'PASSED',
                    'HIGH_PROBA_CLOUDS_PERCENTAGE': '15.3',
                    'MEDIUM_PROBA_CLOUDS_PERCENTAGE': '24.1',
                    'NODATA_PIXEL_PERCENTAGE': '0.0',
                    'NOT_VEGETATED_PERCENTAGE': '3.5',
                    'PREVIEW_GEO_INFO': 'Not applicable',
                    'PREVIEW_IMAGE_URL': 'Not applicable',
                    'PROCESSING_BASELINE': '02.08',
                    'PROCESSING_LEVEL': 'Level-2A',
                    'PRODUCT_START_TIME': '2018-08-18T09:40:31.024Z',
                    'PRODUCT_STOP_TIME': '2018-08-18T09:40:31.024Z',
                    'PRODUCT_TYPE': 'S2MSI2A',
                    'PRODUCT_URI': 'S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE',
                    'RADIATIVE_TRANSFER_ACCURACY': '0.0',
                    'RADIOMETRIC_QUALITY': 'PASSED',
                    'REFLECTANCE_CONVERSION_U': '0.97',
                    'SATURATED_DEFECTIVE_PIXEL_PERCENTAGE': '0.0',
                    'SENSOR_QUALITY': 'PASSED',
                    'SNOW_ICE_PERCENTAGE': '0.4',
                    'SPECIAL_VALUE_NODATA': '0',
                    'SPECIAL_VALUE_SATURATED': '6',
                    'THIN_CIRRUS_PERCENTAGE': '14.9',
                    'UNCLASSIFIED_PERCENTAGE': '5.7',
                    'VEGETATION_PERCENTAGE': '14.0',
                    'WATER_PERCENTAGE': '18.7',
                    'WATER_VAPOUR_RETRIEVAL_ACCURACY': '0.0',
                    'WVP_QUANTIFICATION_VALUE': '1000.0',
                    'WVP_QUANTIFICATION_VALUE_UNIT': 'cm'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 1830 and ds.RasterYSize == 1830

    assert ds.GetProjectionRef().find('32634') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (600000.0, 60.0, 0.0, 6400020.0, 0.0, -60.0)

    assert ds.RasterCount == 7

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l2a_MSIL2A/S2A_MSIL2A_20180818T094031_N0208_R036_T34VFJ_20180818T120345.SAFE/GRANULE/L2A_T34VFJ_A016478_20180818T094030/IMG_DATA/R60m/T34VFJ_20180818T094031_B01_60m.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
      <DstRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B1',
                   'BANDWIDTH': '20',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1884.69',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '443',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.DataType == gdal.GDT_UInt16

###############################################################################
# Test opening a L2A MSIL2Ap product


def test_sentinel2_l2a_6():

    filename_xml = 'data/sentinel2/fake_l2a_MSIL2Ap/S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE/MTD_MSIL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {
                    'AOT_RETRIEVAL_ACCURACY': '0.0',
                    'BARE_SOILS_PERCENTAGE': '0.4',
                    'CLOUD_COVERAGE_ASSESSMENT': '86.3',
                    'CLOUD_COVERAGE_PERCENTAGE': '84.4',
                    'CLOUD_SHADOW_PERCENTAGE': '4.1',
                    'DARK_FEATURES_PERCENTAGE': '1.0',
                    'DATATAKE_1_DATATAKE_SENSING_START': '2017-08-23T09:40:31.026Z',
                    'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                    'DATATAKE_1_ID': 'GS2A_20170823T094031_011330_N02.05',
                    'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                    'DATATAKE_1_SENSING_ORBIT_NUMBER': '36',
                    'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                    'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                    'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                    'FOOTPRINT': 'POLYGON((22.6 57.7, 24.5 57.6, 24.4 56.7, 22.6 56.7, 22.6 57.7))',
                    'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                    'GENERAL_QUALITY_FLAG': 'PASSED',
                    'GENERATION_TIME': '2017-08-25T08:50:10Z',
                    'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                    'HIGH_PROBA_CLOUDS_PERCENTAGE': '36.1',
                    'MEDIUM_PROBA_CLOUDS_PERCENTAGE': '28.9',
                    'L1C_TOA_QUANTIFICATION_VALUE': '10000',
                    'L1C_TOA_QUANTIFICATION_VALUE_UNIT': 'none',
                    'L2A_AOT_QUANTIFICATION_VALUE': '1000.0',
                    'L2A_AOT_QUANTIFICATION_VALUE_UNIT': 'none',
                    'L2A_BOA_QUANTIFICATION_VALUE': '10000',
                    'L2A_BOA_QUANTIFICATION_VALUE_UNIT': 'none',
                    'L2A_WVP_QUANTIFICATION_VALUE': '1000.0',
                    'L2A_WVP_QUANTIFICATION_VALUE_UNIT': 'cm',
                    'LOW_PROBA_CLOUDS_PERCENTAGE': '1.6',
                    'NODATA_PIXEL_PERCENTAGE': '0.0',
                    'PREVIEW_GEO_INFO': 'Not applicable',
                    'PREVIEW_IMAGE_URL': 'Not applicable',
                    'PROCESSING_BASELINE': '02.05',
                    'PROCESSING_LEVEL': 'Level-2Ap',
                    'PRODUCT_START_TIME': '2017-08-23T09:40:31.026Z',
                    'PRODUCT_STOP_TIME': '2017-08-23T09:40:31.026Z',
                    'PRODUCT_TYPE': 'S2MSI2Ap',
                    'PRODUCT_URI_1C': 'S2A_MSIL1C_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE',
                    'PRODUCT_URI_2A': 'S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE',
                    'RADIATIVE_TRANSFER_ACCURAY': '0.0',
                    'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                    'REFERENCE_BAND': 'B1',
                    'REFLECTANCE_CONVERSION_U': '0.97',
                    'SATURATED_DEFECTIVE_PIXEL_PERCENTAGE': '0.0',
                    'SENSOR_QUALITY_FLAG': 'PASSED',
                    'SNOW_ICE_PERCENTAGE': '0.2',
                    'SPECIAL_VALUE_NODATA': '0',
                    'SPECIAL_VALUE_SATURATED': '6',
                    'THIN_CIRRUS_PERCENTAGE': '19.3',
                    'VEGETATION_PERCENTAGE': '5.0',
                    'WATER_PERCENTAGE': '2.9',
                    'WATER_VAPOUR_RETRIEVAL_ACCURACY': '0.0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {
                    'SUBDATASET_1_DESC':
                    'Bands B2, B3, B4, B8 with 10m resolution, UTM 34N',
                    'SUBDATASET_1_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2Ap/S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634',
                    'SUBDATASET_2_DESC':
                    'Bands B5, B6, B7, B8A, B11, B12, AOT, CLD, SCL, SNW, WVP with 20m resolution, UTM 34N',
                    'SUBDATASET_2_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2Ap/S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE/MTD_MSIL2A.xml:20m:EPSG_32634',
                    'SUBDATASET_3_DESC':
                    'Bands B1, B9, AOT, CLD, SCL, SNW, WVP with 60m resolution, UTM 34N',
                    'SUBDATASET_3_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2Ap/S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE/MTD_MSIL2A.xml:60m:EPSG_32634',
                    'SUBDATASET_4_DESC':
                    'True color image, UTM 34N',
                    'SUBDATASET_4_NAME':
                    'SENTINEL2_L2A:data/sentinel2/fake_l2a_MSIL2Ap/S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE/MTD_MSIL2A.xml:TCI:EPSG_32634'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    # Try opening the 4 subdatasets
    for i in range(2):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i + 1)])
        assert ds is not None and gdal.GetLastErrorMsg() == '', \
            got_md['SUBDATASET_%d_NAME' % (i + 1)]

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L2A:',
                 'SENTINEL2_L2A:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L2A:%s' % filename_xml,
                 'SENTINEL2_L2A:%s:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L2A:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_32633' % filename_xml]:
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        assert ds is None, name



###############################################################################
# Test opening a L2A MSIL2Ap subdataset on the 60m bands


def test_sentinel2_l2a_7():

    filename_xml = 'data/sentinel2/fake_l2a_MSIL2Ap/S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE/MTD_MSIL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L2A:%s:60m:EPSG_32634' % filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {
                    'AOT_RETRIEVAL_ACCURACY': '0.0',
                    'BARE_SOILS_PERCENTAGE': '0.4',
                    'CLOUD_COVERAGE_ASSESSMENT': '86.3',
                    'CLOUD_COVERAGE_PERCENTAGE': '84.4',
                    'CLOUD_SHADOW_PERCENTAGE': '4.1',
                    'DARK_FEATURES_PERCENTAGE': '1.0',
                    'DATATAKE_1_DATATAKE_SENSING_START': '2017-08-23T09:40:31.026Z',
                    'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                    'DATATAKE_1_ID': 'GS2A_20170823T094031_011330_N02.05',
                    'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                    'DATATAKE_1_SENSING_ORBIT_NUMBER': '36',
                    'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                    'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                    'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                    'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                    'GENERAL_QUALITY_FLAG': 'PASSED',
                    'GENERATION_TIME': '2017-08-25T08:50:10Z',
                    'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                    'HIGH_PROBA_CLOUDS_PERCENTAGE': '36.1',
                    'MEDIUM_PROBA_CLOUDS_PERCENTAGE': '28.9',
                    'L1C_TOA_QUANTIFICATION_VALUE': '10000',
                    'L1C_TOA_QUANTIFICATION_VALUE_UNIT': 'none',
                    'L2A_AOT_QUANTIFICATION_VALUE': '1000.0',
                    'L2A_AOT_QUANTIFICATION_VALUE_UNIT': 'none',
                    'L2A_BOA_QUANTIFICATION_VALUE': '10000',
                    'L2A_BOA_QUANTIFICATION_VALUE_UNIT': 'none',
                    'L2A_WVP_QUANTIFICATION_VALUE': '1000.0',
                    'L2A_WVP_QUANTIFICATION_VALUE_UNIT': 'cm',
                    'LOW_PROBA_CLOUDS_PERCENTAGE': '1.6',
                    'NODATA_PIXEL_PERCENTAGE': '0.0',
                    'PREVIEW_GEO_INFO': 'Not applicable',
                    'PREVIEW_IMAGE_URL': 'Not applicable',
                    'PROCESSING_BASELINE': '02.05',
                    'PROCESSING_LEVEL': 'Level-2Ap',
                    'PRODUCT_START_TIME': '2017-08-23T09:40:31.026Z',
                    'PRODUCT_STOP_TIME': '2017-08-23T09:40:31.026Z',
                    'PRODUCT_TYPE': 'S2MSI2Ap',
                    'PRODUCT_URI_1C': 'S2A_MSIL1C_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE',
                    'PRODUCT_URI_2A': 'S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE',
                    'RADIATIVE_TRANSFER_ACCURAY': '0.0',
                    'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                    'REFERENCE_BAND': 'B1',
                    'REFLECTANCE_CONVERSION_U': '0.97',
                    'SATURATED_DEFECTIVE_PIXEL_PERCENTAGE': '0.0',
                    'SENSOR_QUALITY_FLAG': 'PASSED',
                    'SNOW_ICE_PERCENTAGE': '0.2',
                    'SPECIAL_VALUE_NODATA': '0',
                    'SPECIAL_VALUE_SATURATED': '6',
                    'THIN_CIRRUS_PERCENTAGE': '19.3',
                    'VEGETATION_PERCENTAGE': '5.0',
                    'WATER_PERCENTAGE': '2.9',
                    'WATER_VAPOUR_RETRIEVAL_ACCURACY': '0.0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 1830 and ds.RasterYSize == 1830

    assert ds.GetProjectionRef().find('32634') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (600000.0, 60.0, 0.0, 6400020.0, 0.0, -60.0)

    assert ds.RasterCount == 7

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l2a_MSIL2Ap/S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE/GRANULE/L2A_T34VFJ_A011330_20170823T094252/IMG_DATA/R60m/L2A_T34VFJ_20170823T094031_B01_60m.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
      <DstRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B1',
                   'BANDWIDTH': '20',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1913.57',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '443',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.DataType == gdal.GDT_UInt16


###############################################################################
# Test opening a L1C Safe Compact product


def test_sentinel2_l1c_safe_compact_1():

    filename_xml = 'data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/MTD_MSIL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUD_COVERAGE_ASSESSMENT': '0.0',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'FOOTPRINT': 'POLYGON((11 46, 11 45, 13 45, 13 46, 11 46))',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1C',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1C',
                   'QUANTIFICATION_VALUE': '1000',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution, UTM 32N',
                   'SUBDATASET_1_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/MTD_MSIL1C.xml:10m:EPSG_32632',
                   'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution, UTM 32N',
                   'SUBDATASET_2_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/MTD_MSIL1C.xml:20m:EPSG_32632',
                   'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution, UTM 32N',
                   'SUBDATASET_3_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/MTD_MSIL1C.xml:60m:EPSG_32632',
                   'SUBDATASET_4_DESC': 'True color image, UTM 32N',
                   'SUBDATASET_4_NAME': 'SENTINEL2_L1C:data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/MTD_MSIL1C.xml:TCI:EPSG_32632'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    # Try opening the 4 subdatasets
    for i in range(4):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i + 1)])
        assert ds is not None and gdal.GetLastErrorMsg() == '', \
            got_md['SUBDATASET_%d_NAME' % (i + 1)]

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1C:',
                 'SENTINEL2_L1C:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L1C:%s' % filename_xml,
                 'SENTINEL2_L1C:%s:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L1C:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_32633' % filename_xml]:
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        assert ds is None, name

    # Try opening a zip file as distributed from https://scihub.esa.int/
    if not sys.platform.startswith('win'):
        os.system('sh -c "cd data/sentinel2/fake_l1c_safecompact && zip -r ../../tmp/S2A_MSIL1C_test.zip S2A_OPER_PRD_MSIL1C.SAFE >/dev/null" && cd ../..')
        if os.path.exists('tmp/S2A_MSIL1C_test.zip'):
            ds = gdal.Open('tmp/S2A_MSIL1C_test.zip')
            assert ds is not None
            os.unlink('tmp/S2A_MSIL1C_test.zip')


###############################################################################
# Test opening a L1C Safe Compact subdataset on the 10m bands


def test_sentinel2_l1c_safe_compact_2():

    filename_xml = 'data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/MTD_MSIL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C:%s:10m:EPSG_32632' % filename_xml)
    assert ds is not None and gdal.GetLastErrorMsg() == ''

    expected_md = {'CLOUD_COVERAGE_ASSESSMENT': '0.0',
                   'DATATAKE_1_DATATAKE_SENSING_START': '2015-12-31T23:59:59.999Z',
                   'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
                   'DATATAKE_1_ID': 'GS2A_20151231T235959_000123_N01.03',
                   'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
                   'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
                   'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
                   'DEGRADED_ANC_DATA_PERCENTAGE': '0',
                   'DEGRADED_MSI_DATA_PERCENTAGE': '0',
                   'FORMAT_CORRECTNESS_FLAG': 'PASSED',
                   'GENERAL_QUALITY_FLAG': 'PASSED',
                   'GENERATION_TIME': '2015-12-31T23:59:59.999Z',
                   'GEOMETRIC_QUALITY_FLAG': 'PASSED',
                   'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
                   'PREVIEW_IMAGE_URL': 'http://example.com',
                   'PROCESSING_BASELINE': '01.03',
                   'PROCESSING_LEVEL': 'Level-1C',
                   'PRODUCT_START_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_STOP_TIME': '2015-12-31T23:59:59.999Z',
                   'PRODUCT_TYPE': 'S2MSI1C',
                   'QUANTIFICATION_VALUE': '1000',
                   'RADIOMETRIC_QUALITY_FLAG': 'PASSED',
                   'REFERENCE_BAND': 'B1',
                   'REFLECTANCE_CONVERSION_U': '0.97',
                   'SENSOR_QUALITY_FLAG': 'PASSED',
                   'SPECIAL_VALUE_NODATA': '1',
                   'SPECIAL_VALUE_SATURATED': '0'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert ds.RasterXSize == 10980 and ds.RasterYSize == 10980

    assert ds.GetProjectionRef().find('32632') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0)

    assert ds.RasterCount == 4

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/GRANULE/FOO/IMG_DATA/BAR_B04.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>
"""
    assert placement_vrt in vrt

    assert ds.GetMetadata('xml:SENTINEL2') is not None

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B4',
                   'BANDWIDTH': '30',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1500',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '665',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()

    assert band.GetColorInterpretation() == gdal.GCI_RedBand

    assert band.DataType == gdal.GDT_UInt16

    assert band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '12'

    band = ds.GetRasterBand(4)

    assert band.GetColorInterpretation() == gdal.GCI_Undefined

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B8',
                   'BANDWIDTH': '115',
                   'BANDWIDTH_UNIT': 'nm',
                   'SOLAR_IRRADIANCE': '1000',
                   'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
                   'WAVELENGTH': '842',
                   'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        import pprint
        pprint.pprint(got_md)
        pytest.fail()


###############################################################################
# Test opening a L1C subdataset on the TCI bands


def test_sentinel2_l1c_safe_compact_3():

    filename_xml = 'data/sentinel2/fake_l1c_safecompact/S2A_MSIL1C_test.SAFE/MTD_MSIL1C.xml'
    ds = gdal.OpenEx('SENTINEL2_L1C:%s:TCI:EPSG_32632' % filename_xml)
    assert ds is not None

    assert ds.RasterCount == 3

    fl = ds.GetFileList()
    # main XML + 1 granule XML + 1 jp2
    if len(fl) != 1 + 1 + 1:
        import pprint
        pprint.pprint(fl)
        pytest.fail()

    band = ds.GetRasterBand(1)
    assert band.GetColorInterpretation() == gdal.GCI_RedBand

    assert band.DataType == gdal.GDT_Byte



