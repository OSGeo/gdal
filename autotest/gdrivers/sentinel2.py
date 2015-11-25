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

def sentinel2_1():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml'
    gdal.ErrorReset()
    ds = gdal.Open(filename_xml)
    if ds is None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    expected_md = {'CLOUD_COVERAGE_ASSESSMENT': '0.0',
 'DATATAKE_1_DATATAKE_SENSING_START': 'YYYY-MM-DDTHH:MM:SS.sssZ',
 'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
 'DATATAKE_1_ID': 'GS2A_YYYYMMDDTHHMMSS_000XYZ_N01.03',
 'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
 'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
 'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
 'DEGRADED_ANC_DATA_PERCENTAGE': '0',
 'DEGRADED_MSI_DATA_PERCENTAGE': '0',
 'FOOTPRINT': 'POLYGON((11 46, 11 45, 13 45, 13 46, 11 46))',
 'FORMAT_CORRECTNESS_FLAG': 'PASSED',
 'GENERAL_QUALITY_FLAG': 'PASSED',
 'GENERATION_TIME': 'YYYY-MM-DDTHH:MM:SS.sssZ',
 'GEOMETRIC_QUALITY_FLAG': 'PASSED',
 'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
 'PREVIEW_IMAGE_URL': 'http://example.com',
 'PROCESSING_BASELINE': '01.03',
 'PROCESSING_LEVEL': 'Level-1C',
 'PRODUCT_START_TIME': 'YYYY-MM-DDTHH:MM:SS.sssZ',
 'PRODUCT_STOP_TIME': 'YYYY-MM-DDTHH:MM:SS.sssZ',
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
 'SUBDATASET_1_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml:10m:EPSG_32632',
 'SUBDATASET_2_DESC': 'Bands B5, B6, B7, B8A, B11, B12 with 20m resolution, UTM 32N',
 'SUBDATASET_2_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml:20m:EPSG_32632',
 'SUBDATASET_3_DESC': 'Bands B1, B9, B10 with 60m resolution, UTM 32N',
 'SUBDATASET_3_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml:60m:EPSG_32632',
 'SUBDATASET_4_DESC': 'RGB preview, UTM 32N',
 'SUBDATASET_4_NAME': 'SENTINEL2_L1C:data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml:PREVIEW:EPSG_32632'}
    got_md = ds.GetMetadata('SUBDATASETS')
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        import pprint
        pprint.pprint(got_md)
        return 'fail'

    # Try opening a zip file as distributed from https://scihub.esa.int/
    if not sys.platform.startswith('win'):
        os.system('sh -c "cd data/fake_sentinel2_l1c && zip -r ../../tmp/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.zip S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE >/dev/null" && cd ../..')
        if os.path.exists('tmp/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.zip'):
            ds = gdal.Open('tmp/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.zip')
            if ds is None:
                gdaltest.post_reason('fail')
                return 'fail'
            os.unlink('tmp/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.zip')

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

def sentinel2_2():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml'
    ds = gdal.Open('SENTINEL2_L1C:%s:10m:EPSG_32632' % filename_xml)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    expected_md = {'CLOUD_COVERAGE_ASSESSMENT': '0.0',
 'DATATAKE_1_DATATAKE_SENSING_START': 'YYYY-MM-DDTHH:MM:SS.sssZ',
 'DATATAKE_1_DATATAKE_TYPE': 'INS-NOBS',
 'DATATAKE_1_ID': 'GS2A_YYYYMMDDTHHMMSS_000XYZ_N01.03',
 'DATATAKE_1_SENSING_ORBIT_DIRECTION': 'DESCENDING',
 'DATATAKE_1_SENSING_ORBIT_NUMBER': '22',
 'DATATAKE_1_SPACECRAFT_NAME': 'Sentinel-2A',
 'DEGRADED_ANC_DATA_PERCENTAGE': '0',
 'DEGRADED_MSI_DATA_PERCENTAGE': '0',
 'FORMAT_CORRECTNESS_FLAG': 'PASSED',
 'GENERAL_QUALITY_FLAG': 'PASSED',
 'GENERATION_TIME': 'YYYY-MM-DDTHH:MM:SS.sssZ',
 'GEOMETRIC_QUALITY_FLAG': 'PASSED',
 'PREVIEW_GEO_INFO': 'BrowseImageFootprint',
 'PREVIEW_IMAGE_URL': 'http://example.com',
 'PROCESSING_BASELINE': '01.03',
 'PROCESSING_LEVEL': 'Level-1C',
 'PRODUCT_START_TIME': 'YYYY-MM-DDTHH:MM:SS.sssZ',
 'PRODUCT_STOP_TIME': 'YYYY-MM-DDTHH:MM:SS.sssZ',
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
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/GRANULE/S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B08.jp2</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="10980" RasterYSize="10980" DataType="UInt16" BlockXSize="128" BlockYSize="128" />
      <SrcRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
      <DstRect xOff="0" yOff="0" xSize="10980" ySize="10980" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/GRANULE/S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B08.jp2</SourceFilename>
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

def sentinel2_3():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml'
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

def sentinel2_4():

    filename_xml = 'data/fake_sentinel2_l1c/S2A_OPER_PRD_MSIL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.SAFE/S2A_OPER_MTD_SAFL1C_PDMC_YYYMMDDTHHMMSS_RXYZ_VYYYYMMDDTYYMMSS_YYYYMMDDTHHMMSS.xml'
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

def sentinel2_5():

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
                    <Granules datastripIdentifier="S2A_OPER_MSI_L1C_DS_MTI__YYYYMMDDTHHMMSS_SYYYYMMDDTHHMMSS_N01.03" granuleIdentifier="S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_N01.03" imageFormat="JPEG2000">
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B01</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B06</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B10</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B08</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B07</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B09</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B05</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B12</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B11</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B04</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B03</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B02</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TQR_B8A</IMAGE_ID>
                    </Granules>
                </Granule_List>
                <Granule_List>
                    <Granules datastripIdentifier="S2A_OPER_MSI_L1C_DS_MTI__YYYYMMDDTHHMMSS_SYYYYMMDDTHHMMSS_N01.03" granuleIdentifier="S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_N01.03" imageFormat="JPEG2000">
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B01</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B06</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B10</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B08</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B07</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B09</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B05</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B12</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B11</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B04</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B03</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B02</IMAGE_ID>
                        <IMAGE_ID>S2A_OPER_MSI_L1C_TL_MTI__YYYYMMDDTHHMMSS_A000XYZ_T32TRQ_B8A</IMAGE_ID>
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


gdaltest_list = [
    sentinel2_1,
    sentinel2_2,
    sentinel2_3,
    sentinel2_4,
    sentinel2_5
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'sentinel2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
