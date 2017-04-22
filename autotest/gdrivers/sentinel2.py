#!/usr/bin/env python
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test opening a L1C product

def sentinel2_l1c_1():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution, UTM 32N',
 'SUBDATASET_1_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:10m:EPSG_32632',
 'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution, UTM 32N',
 'SUBDATASET_2_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:20m:EPSG_32632',
 'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution, UTM 32N',
 'SUBDATASET_3_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:60m:EPSG_32632',
 'SUBDATASET_4_DESC': 'RGB preview, UTM 32N',
 'SUBDATASET_4_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml:PREVIEW:EPSG_32632'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    # Try opening a zip file as distributed from https://scihub.esa.int/
    if not sys.platform.startswith('win'):
        os.system('sh -c "cd data/fake_sentinel2_l1c && zip -r ../../tmp/S2A_OPER_PRD_MSIL1C.zip S2A_OPER_PRD_MSIL1C.SAFE >/dev/null" && cd ../..')
        if os.path.exists('tmp/S2A_OPER_PRD_MSIL1C.zip'):
            ds = gdal.Open('tmp/S2A_OPER_PRD_MSIL1C.zip')
            if ds is None:
                gdaltest.post_reason('fail')
                return 'fail'
            os.unlink('tmp/S2A_OPER_PRD_MSIL1C.zip')

    # Try opening the 4 subdatasets
    for i in range(4):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i+1)])
        if ds is None or gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            print(got_md['SUBDATASET_%d_NAME' % (i+1)])
            return 'fail'

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1C:',
                 'SENTINEL2_L1C:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L1C:%s' % filename_xml,
                 'SENTINEL2_L1C:%s:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L1C:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_32633' % filename_xml] :
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        if ds is not None:
            gdaltest.post_reason('fail')
            print(name)
            return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C subdataset on the 10m bands

def sentinel2_l1c_2():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C:%s:10m:EPSG_32632' % filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if ds.RasterXSize != 20984 or ds.RasterYSize != 20980:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetProjectionRef().find('32632') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_gt = ds.GetGeoTransform()
    if got_gt != (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0):
        gdaltest.post_reason('fail')
        print(got_gt)
        return 'fail'

    if ds.RasterCount != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TQR_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="10980" RasterYSize="10980" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TRQ_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TRQ_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="10980" RasterYSize="10980" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="10004" yOff="10000" xSize="10980" ySize="10980" />
    </SimpleSource>"""
    if vrt.find(placement_vrt) < 0:
        gdaltest.post_reason('fail')
        print(vrt)
        return 'fail'

    if ds.GetMetadata('xml:SENTINEL2') is None:
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if band.GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '12':
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(4)

    if band.GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason('fail')
        return 'fail'

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B8',
 'BANDWIDTH': '115',
 'BANDWIDTH_UNIT': 'nm',
 'SOLAR_IRRADIANCE': '1000',
 'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
 'WAVELENGTH': '842',
 'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C subdataset on the 60m bands and enabling alpha band

def sentinel2_l1c_3():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    ds = gdal.OpenEx('SENTINEL2_L1C:%s:60m:EPSG_32632' % filename_xml, open_options = ['ALPHA=YES'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.RasterCount != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(4)

    if band.GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    cs = band.Checksum()
    if cs != 0 or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    band.ReadRaster()

    return 'success'

###############################################################################
# Test opening a L1C subdataset on the PREVIEW bands

def sentinel2_l1c_4():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    ds = gdal.OpenEx('SENTINEL2_L1C:%s:PREVIEW:EPSG_32632' % filename_xml)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.RasterCount != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    fl = ds.GetFileList()
    # main XML + 2 granule XML + 2 jp2
    if len(fl) != 1 + 2 + 2:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(fl)
        return 'fail'

    band = ds.GetRasterBand(1)
    if band.GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.DataType != gdal.GDT_Byte:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test opening invalid XML files

def sentinel2_l1c_5():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
"""<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # No Product_Info
    gdal.FileFromMemBuffer('/vsimem/test.xml',
"""<n1:Level-1C_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1C.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd S2_User_Product_Level-1C_Metadata.xsd">
    <n1:General_Info>
    </n1:General_Info>
</n1:Level-1C_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:10m:EPSG_32632')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'


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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'


    gdal.Unlink('/vsimem/test.xml')

    return 'success'

###############################################################################
# Windows specific test to test support for long filenames

def sentinel2_l1c_6():

    if sys.platform != 'win32':
        return 'skip'

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/S2A_OPER_MTD_SAFL1C.xml'
    filename_xml = filename_xml.replace('/', '\\')
    filename_xml = '\\\\?\\' + os.getcwd() + '\\' + filename_xml
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    subds_name = ds.GetMetadataItem('SUBDATASET_1_NAME', 'SUBDATASETS')
    gdal.ErrorReset()
    ds = gdal.Open(subds_name)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test with a real JP2 tile

def sentinel2_l1c_7():

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

    f = open('data/gtsmall_10_uint16.jp2', 'rb')
    f2 = gdal.VSIFOpenL('/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_bla_B01.jp2', 'wb')
    data = f.read()
    gdal.VSIFWriteL(data, 1, len(data), f2)
    gdal.VSIFCloseL(f2)
    f.close()

    ds = gdal.Open('SENTINEL2_L1C:/vsimem/test.xml:60m:EPSG_32753')
    nbits = ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE')
    if nbits != '10':
        gdaltest.post_reason('fail')
        print(nbits)
        return 'fail'

    gdal.Unlink('/vsimem/test.xml')
    gdal.Unlink('/vsimem/GRANULE/S2A_OPER_MSI_L1C_bla_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_bla_B01.jp2')

    return 'success'

###############################################################################
# Test opening a L1C tile

def sentinel2_l1c_tile_1():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution',
 'SUBDATASET_1_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:10m',
 'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution',
 'SUBDATASET_2_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:20m',
 'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution',
 'SUBDATASET_3_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:60m',
 'SUBDATASET_4_DESC': 'RGB preview',
 'SUBDATASET_4_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:PREVIEW'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    # Try opening the 4 subdatasets
    for i in range(4):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i+1)])
        if ds is None or gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            print(got_md['SUBDATASET_%d_NAME' % (i+1)])
            return 'fail'

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1C_TILE:',
                 'SENTINEL2_L1C_TILE:foo.xml:10m',
                 'SENTINEL2_L1C_TILE:%s' % filename_xml,
                 'SENTINEL2_L1C_TILE:%s:' % filename_xml] :
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        if ds is not None:
            gdaltest.post_reason('fail')
            print(name)
            return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C tile without main MTD file

def sentinel2_l1c_tile_2():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', 'NO') # Simulate absence of main MTD file
    ds = gdal.Open(filename_xml)
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', None)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
 'DATASTRIP_ID': 'S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_S20151231T235959_N01.03',
 'DEGRADED_MSI_DATA_PERCENTAGE': '0',
 'DOWNLINK_PRIORITY': 'NOMINAL',
 'SENSING_TIME': '2015-12-31T23:59:59.999Z',
 'TILE_ID': 'S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_N01.03'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution',
 'SUBDATASET_1_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:10m',
 'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution',
 'SUBDATASET_2_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:20m',
 'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution',
 'SUBDATASET_3_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:60m',
 'SUBDATASET_4_DESC': 'RGB preview',
 'SUBDATASET_4_NAME': 'SENTINEL2_L1C_TILE:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml:PREVIEW'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C tile subdataset on the 10m bands

def sentinel2_l1c_tile_3():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C_TILE:%s:10m' % filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if ds.RasterXSize != 10980 or ds.RasterYSize != 10980:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetProjectionRef().find('32632') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_gt = ds.GetGeoTransform()
    if got_gt != (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0):
        gdaltest.post_reason('fail')
        print(got_gt)
        return 'fail'

    if ds.RasterCount != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TQR_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="10980" RasterYSize="10980" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>"""
    if vrt.find(placement_vrt) < 0:
        gdaltest.post_reason('fail')
        print(vrt)
        return 'fail'

    if ds.GetMetadata('xml:SENTINEL2') is None:
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if band.GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '12':
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(4)

    if band.GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason('fail')
        return 'fail'

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B8',
 'BANDWIDTH': '115',
 'BANDWIDTH_UNIT': 'nm',
 'SOLAR_IRRADIANCE': '1000',
 'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
 'WAVELENGTH': '842',
 'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C tile subdataset on the 10m bands without main MTD file

def sentinel2_l1c_tile_4():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', 'NO') # Simulate absence of main MTD file
    ds = gdal.OpenEx('SENTINEL2_L1C_TILE:%s:10m' % filename_xml, open_options = ['ALPHA=YES'])
    gdal.SetConfigOption('SENTINEL2_USE_MAIN_MTD', None)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    expected_md = {'CLOUDY_PIXEL_PERCENTAGE': '0',
 'DATASTRIP_ID': 'S2A_OPER_MSI_L1C_DS_MTI__20151231T235959_S20151231T235959_N01.03',
 'DEGRADED_MSI_DATA_PERCENTAGE': '0',
 'DOWNLINK_PRIORITY': 'NOMINAL',
 'SENSING_TIME': '2015-12-31T23:59:59.999Z',
 'TILE_ID': 'S2A_OPER_MSI_L1C_TL_MTI__20151231T235959_A000123_T32TQR_N01.03'}
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if ds.RasterXSize != 10980 or ds.RasterYSize != 10980:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetProjectionRef().find('32632') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_gt = ds.GetGeoTransform()
    if got_gt != (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0):
        gdaltest.post_reason('fail')
        print(got_gt)
        return 'fail'

    if ds.RasterCount != 5:
        gdaltest.post_reason('fail')
        return 'fail'

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_T32TQR_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="10980" RasterYSize="10980" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>"""
    if vrt.find(placement_vrt) < 0:
        gdaltest.post_reason('fail')
        print(vrt)
        return 'fail'

    if ds.GetMetadata('xml:SENTINEL2') is None:
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B4',
 'BANDWIDTH': '30',
 'BANDWIDTH_UNIT': 'nm',
 'WAVELENGTH': '665',
 'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if band.GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '12':
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(5)

    if band.GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C tile subdataset on the preview bands

def sentinel2_l1c_tile_5():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/S2A_OPER_MTD_L1C_T32TQR.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C_TILE:%s:PREVIEW' % filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.RasterXSize != 343 or ds.RasterYSize != 343:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetProjectionRef().find('32632') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_gt = ds.GetGeoTransform()
    if got_gt != (699960.0, 320.0, 0.0, 5100060.0, 0.0, -320.0):
        gdaltest.post_reason('fail')
        print(got_gt)
        return 'fail'

    if ds.RasterCount != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/S2A_OPER_MSI_L1C_T32TQR_N01.03/QI_DATA/S2A_OPER_PVI_L1C_T32TQR.jp2</SourceFilename>
      <SourceBand>3</SourceBand>
      <SourceProperties RasterXSize="343" RasterYSize="343" DataType="Byte" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="343" ySize="343" />
      <DstRect xOff="0" yOff="0" xSize="343" ySize="343" />
    </SimpleSource>"""
    if vrt.find(placement_vrt) < 0:
        gdaltest.post_reason('fail')
        print(vrt)
        return 'fail'

    return 'success'

###############################################################################
# Test opening invalid XML files

def sentinel2_l1c_tile_6():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
"""<n1:Level-1C_Tile_ID xmlns:n1="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_TILE_L1C/02.09.07/scripts/../../../schemas/02.11.06/PSD/S2_PDI_Level-1C_Tile_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1C_TILE:/vsimem/test.xml:10m')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/test.xml')
    gdal.Unlink('/vsimem/S2A_OPER_MSI_L1C_bla_N01.03/S2A_OPER_MTD_L1C_bla.xml')

    return 'success'

###############################################################################
# Test opening a L1B product

def sentinel2_l1b_1():

    filename_xml = 'data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/S2B_OPER_MTD_SAFL1B.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 of granule S2B_OPER_MTD_L1B.xml with 10m resolution',
 'SUBDATASET_1_NAME': 'SENTINEL2_L1B:data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:10m',
 'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 of granule S2B_OPER_MTD_L1B.xml with 20m resolution',
 'SUBDATASET_2_NAME': 'SENTINEL2_L1B:data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:20m',
 'SUBDATASET_3_DESC': 'Bands B1, B9, B10 of granule S2B_OPER_MTD_L1B.xml with 60m resolution',
 'SUBDATASET_3_NAME': 'SENTINEL2_L1B:data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    # Try opening the 3 subdatasets
    for i in range(3):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i+1)])
        if ds is None or gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            print(got_md['SUBDATASET_%d_NAME' % (i+1)])
            return 'fail'

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1B:',
                 'SENTINEL2_L1B:foo.xml:10m',
                 'SENTINEL2_L1B:%s' % filename_xml,
                 'SENTINEL2_L1B:%s:' % filename_xml,
                 'SENTINEL2_L1B:%s:30m' % filename_xml] :
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        if ds is not None:
            gdaltest.post_reason('fail')
            print(name)
            return 'fail'

    return 'success'

###############################################################################
# Test opening a L1B granule

def sentinel2_l1b_2():

    filename_xml = 'data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    subdatasets_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution',
 'SUBDATASET_1_NAME': 'SENTINEL2_L1B:data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:10m',
 'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution',
 'SUBDATASET_2_NAME': 'SENTINEL2_L1B:data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:20m',
 'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution',
 'SUBDATASET_3_NAME': 'SENTINEL2_L1B:data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != subdatasets_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    cwd = os.getcwd()
    gdal.ErrorReset()
    try:
        os.chdir('data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03')
        ds = gdal.Open('S2B_OPER_MTD_L1B.xml')
    finally:
        os.chdir(cwd)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    got_md = ds.GetMetadata()
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a L1B subdataset

def sentinel2_l1b_3():

    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1B:data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m')
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if ds.RasterXSize != 1276 or ds.RasterYSize != 384:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetGCPProjection().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gcps = ds.GetGCPs()
    if len(gcps) != 5:
        gdaltest.post_reason('fail')
        print(len(gcps))
        return 'fail'

    if gcps[0].GCPPixel != 0 or \
       gcps[0].GCPLine != 0 or \
       gcps[0].GCPX != 11 or \
       gcps[0].GCPY != 46 or \
       gcps[0].GCPZ != 1:
        gdaltest.post_reason('fail')
        print(gcps[0])
        return 'fail'

    if gcps[1].GCPPixel != 0 or \
       gcps[1].GCPLine != 384 or \
       gcps[1].GCPX != 11 or \
       gcps[1].GCPY != 45 or \
       gcps[1].GCPZ != 2:
        gdaltest.post_reason('fail')
        print(gcps[1])
        return 'fail'

    if gcps[2].GCPPixel != 1276 or \
       gcps[2].GCPLine != 384 or \
       gcps[2].GCPX != 13 or \
       gcps[2].GCPY != 45 or \
       gcps[2].GCPZ != 3:
        gdaltest.post_reason('fail')
        print(gcps[2])
        return 'fail'

    if gcps[3].GCPPixel != 1276 or \
       gcps[3].GCPLine != 0 or \
       gcps[3].GCPX != 13 or \
       gcps[3].GCPY != 46 or \
       gcps[3].GCPZ != 4:
        gdaltest.post_reason('fail')
        print(gcps[3])
        return 'fail'

    if gcps[4].GCPPixel != 1276./2 or \
       gcps[4].GCPLine != 384./2 or \
       gcps[4].GCPX != 12 or \
       gcps[4].GCPY != 45.5 or \
       gcps[4].GCPZ != 2.5:
        gdaltest.post_reason('fail')
        print(gcps[4])
        return 'fail'

    if ds.RasterCount != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1b/S2B_OPER_PRD_MSIL1B.SAFE/GRANULE/S2B_OPER_MSI_L1B_N01.03/IMG_DATA/S2B_OPER_MSI_L1B_B01.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="1276" RasterYSize="384" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="1276" ySize="384" />
      <DstRect xOff="0" yOff="0" xSize="1276" ySize="384" />
    </SimpleSource>"""
    if vrt.find(placement_vrt) < 0:
        gdaltest.post_reason('fail')
        print(vrt)
        return 'fail'

    if ds.GetMetadata('xml:SENTINEL2') is None:
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(1)
    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B1',
 'BANDWIDTH': '20',
 'BANDWIDTH_UNIT': 'nm',
 'WAVELENGTH': '443',
 'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if band.DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '12':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test opening a L1B granule (with missing tile, without any ../../main_mtd.xml)

def sentinel2_l1b_4():

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

    f = open('data/gtsmall_10_uint16.jp2', 'rb')
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
    if ds.RasterXSize != 500:
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'
    ds = None

    ds = gdal.OpenEx('SENTINEL2_L1B:/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml:60m', open_options = ['ALPHA=YES'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterCount != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/S2B_OPER_MTD_L1B.xml')
    gdal.Unlink('/vsimem/foo/GRANULE/S2B_OPER_MTD_L1B_N01.03/IMG_DATA/S2B_OPER_MSI_L1B_B01.jp2')

    return 'success'

###############################################################################
# Test opening invalid XML files

def sentinel2_l1b_5():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
"""<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # No Product_Info
    gdal.FileFromMemBuffer('/vsimem/test.xml',
"""<n1:Level-1B_User_Product xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/User_Product_Level-1B.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-1B.xsd S2_User_Product_Level-1B_Metadata.xsd">
    <n1:General_Info>
    </n1:General_Info>
</n1:Level-1B_User_Product>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'


    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
"""<n1:Level-1B_Granule_ID xmlns:n1="https://psd-13.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="https://psd-12.sentinel2.eo.esa.int/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd /dpc/app/s2ipf/FORMAT_METADATA_GR_L1B/02.09.06/scripts/../../../schemas/02.11.07/PSD/S2_PDI_Level-1B_Granule_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L1B:/vsimem/test.xml:10m')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/test.xml')


    gdal.Unlink('/vsimem/test.xml')

    return 'success'

###############################################################################
# Test opening a L2A product

def sentinel2_l2a_1():

    filename_xml = 'data/fake_sentinel2_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    expected_md = {'SUBDATASET_1_DESC': 'Bands B1, B2, B3, B4, B5, B6, B7, B9, B10, B11, B12, B8A, AOT, CLD, SCL, SNW, WVP with 60m resolution, UTM 32N',
 'SUBDATASET_1_NAME': 'SENTINEL2_L2A:data/fake_sentinel2_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml:60m:EPSG_32632',
 'SUBDATASET_2_DESC': 'RGB preview, UTM 32N',
 'SUBDATASET_2_NAME': 'SENTINEL2_L2A:data/fake_sentinel2_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml:PREVIEW:EPSG_32632'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    # Try opening the 4 subdatasets
    for i in range(2):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i+1)])
        if ds is None or gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            print(got_md['SUBDATASET_%d_NAME' % (i+1)])
            return 'fail'

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L2A:',
                 'SENTINEL2_L2A:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L2A:%s' % filename_xml,
                 'SENTINEL2_L2A:%s:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L2A:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L2A:%s:10m:EPSG_32633' % filename_xml] :
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        if ds is not None:
            gdaltest.post_reason('fail')
            print(name)
            return 'fail'

    return 'success'

###############################################################################
# Test opening a L21 subdataset on the 60m bands

def sentinel2_l2a_2():

    filename_xml = 'data/fake_sentinel2_l2a/S2A_USER_PRD_MSIL2A.SAFE/S2A_USER_MTD_SAFL2A.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L2A:%s:60m:EPSG_32632' % filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if ds.RasterXSize != 1830 or ds.RasterYSize != 1830:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetProjectionRef().find('32632') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_gt = ds.GetGeoTransform()
    if got_gt != (699960.0, 60.0, 0.0, 5100060.0, 0.0, -60.0):
        gdaltest.post_reason('fail')
        print(got_gt)
        return 'fail'

    if ds.RasterCount != 17:
        gdaltest.post_reason('fail')
        print(ds.RasterCount)
        return 'fail'

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l2a/S2A_USER_PRD_MSIL2A.SAFE/GRANULE/S2A_USER_MSI_L2A_T32TQR_N01.03/IMG_DATA/R60m/S2A_USER_MSI_L2A_T32TQR_B01_60m.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="1830" RasterYSize="1830" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
      <DstRect xOff="0" yOff="0" xSize="1830" ySize="1830" />
    </SimpleSource>"""
    if vrt.find(placement_vrt) < 0:
        gdaltest.post_reason('fail')
        print(vrt)
        return 'fail'

    if ds.GetMetadata('xml:SENTINEL2') is None:
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if band.DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(13)

    if band.GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason('fail')
        return 'fail'

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'AOT'}
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    scl_band = 0
    for i in range(ds.RasterCount):
        if ds.GetRasterBand(i+1).GetMetadataItem('BANDNAME') == 'SCL':
            scl_band = i +1
    if scl_band == 0:
        gdaltest.post_reason('fail')
        return 'fail'
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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_categories)
        return 'fail'

    return 'success'

###############################################################################
# Test opening invalid XML files

def sentinel2_l2a_3():

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/test.xml',
"""<n1:Level-2A_User_Product xmlns:n1="https://psd-12.sentinel2.eo.esa.int/PSD/User_Product_Level-2A.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pdgs.s2.esa.int/PSD/User_Product_Level-2A.xsd S2_User_Product_Level-2A_Metadata.xsd">
""")

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L2A:/vsimem/test.xml:10m:EPSG_32632')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
        ds = gdal.Open('/vsimem/test.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('SENTINEL2_L2A:/vsimem/test.xml:10m:EPSG_32632')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/test.xml')

    return 'success'

###############################################################################
# Test opening a L1C Safe Compact product

def sentinel2_l1c_safe_compact_1():

    filename_xml = 'data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/MTD_MSIL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    expected_md = {'SUBDATASET_1_DESC': 'Bands B2, B3, B4, B8 with 10m resolution, UTM 32N',
 'SUBDATASET_1_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/MTD_MSIL1C.xml:10m:EPSG_32632',
 'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution, UTM 32N',
 'SUBDATASET_2_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/MTD_MSIL1C.xml:20m:EPSG_32632',
 'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution, UTM 32N',
 'SUBDATASET_3_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/MTD_MSIL1C.xml:60m:EPSG_32632',
 'SUBDATASET_4_DESC': 'True color image, UTM 32N',
 'SUBDATASET_4_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/MTD_MSIL1C.xml:TCI:EPSG_32632'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    # Try opening the 4 subdatasets
    for i in range(4):
        gdal.ErrorReset()
        ds = gdal.Open(got_md['SUBDATASET_%d_NAME' % (i+1)])
        if ds is None or gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            print(got_md['SUBDATASET_%d_NAME' % (i+1)])
            return 'fail'

    # Try various invalid subdataset names
    for name in ['SENTINEL2_L1C:',
                 'SENTINEL2_L1C:foo.xml:10m:EPSG_32632',
                 'SENTINEL2_L1C:%s' % filename_xml,
                 'SENTINEL2_L1C:%s:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_' % filename_xml,
                 'SENTINEL2_L1C:%s:50m:EPSG_32632' % filename_xml,
                 'SENTINEL2_L1C:%s:10m:EPSG_32633' % filename_xml] :
        with gdaltest.error_handler():
            ds = gdal.Open(name)
        if ds is not None:
            gdaltest.post_reason('fail')
            print(name)
            return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C Safe Compact subdataset on the 10m bands

def sentinel2_l1c_safe_compact_2():

    filename_xml = 'data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/MTD_MSIL1C.xml'
    gdal.ErrorReset()
    ds = gdal.Open('SENTINEL2_L1C:%s:10m:EPSG_32632' % filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if ds.RasterXSize != 10980 or ds.RasterYSize != 10980:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetProjectionRef().find('32632') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_gt = ds.GetGeoTransform()
    if got_gt != (699960.0, 10.0, 0.0, 5100060.0, 0.0, -10.0):
        gdaltest.post_reason('fail')
        print(got_gt)
        return 'fail'

    if ds.RasterCount != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/GRANULE/FOO/IMG_DATA/BAR_B04.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="10980" RasterYSize="10980" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>
"""
    if vrt.find(placement_vrt) < 0:
        gdaltest.post_reason('fail')
        print(vrt)
        return 'fail'

    if ds.GetMetadata('xml:SENTINEL2') is None:
        gdaltest.post_reason('fail')
        return 'fail'

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
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    if band.GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '12':
        gdaltest.post_reason('fail')
        return 'fail'

    band = ds.GetRasterBand(4)

    if band.GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason('fail')
        return 'fail'

    got_md = band.GetMetadata()
    expected_md = {'BANDNAME': 'B8',
 'BANDWIDTH': '115',
 'BANDWIDTH_UNIT': 'nm',
 'SOLAR_IRRADIANCE': '1000',
 'SOLAR_IRRADIANCE_UNIT': 'W/m2/um',
 'WAVELENGTH': '842',
 'WAVELENGTH_UNIT': 'nm'}
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a L1C subdataset on the TCI bands

def sentinel2_l1c_safe_compact_3():

    filename_xml = 'data/fake_sentinel2_l1c_safecompact/S2A_OPER_PRD_MSIL1C.SAFE/MTD_MSIL1C.xml'
    ds = gdal.OpenEx('SENTINEL2_L1C:%s:TCI:EPSG_32632' % filename_xml)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.RasterCount != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    fl = ds.GetFileList()
    # main XML + 1 granule XML + 1 jp2
    if len(fl) != 1 + 1 + 1:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(fl)
        return 'fail'

    band = ds.GetRasterBand(1)
    if band.GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    if band.DataType != gdal.GDT_Byte:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

gdaltest_list = [
    sentinel2_l1c_1,
    sentinel2_l1c_2,
    sentinel2_l1c_3,
    sentinel2_l1c_4,
    sentinel2_l1c_5,
    sentinel2_l1c_6,
    sentinel2_l1c_7,
    sentinel2_l1c_tile_1,
    sentinel2_l1c_tile_2,
    sentinel2_l1c_tile_3,
    sentinel2_l1c_tile_4,
    sentinel2_l1c_tile_5,
    sentinel2_l1c_tile_6,
    sentinel2_l1b_1,
    sentinel2_l1b_2,
    sentinel2_l1b_3,
    sentinel2_l1b_4,
    sentinel2_l1b_5,
    sentinel2_l2a_1,
    sentinel2_l2a_2,
    sentinel2_l2a_3,
    sentinel2_l1c_safe_compact_1,
    sentinel2_l1c_safe_compact_2,
    sentinel2_l1c_safe_compact_3
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'sentinel2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
