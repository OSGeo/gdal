#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JP2Lura driver.
# Author:   Even Rouault  <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault at spatialys.com>
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
import struct
import sys
import shutil
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

sys.path.append( '../pymod' )
sys.path.append('../../gdal/swig/python/samples')

import gdaltest

###############################################################################
# Verify we have the driver.

def jp2lura_1():

    try:
        gdaltest.jp2lura_drv = gdal.GetDriverByName( 'JP2Lura' )
    except:
        gdaltest.jp2lura_drv = None
        return 'skip'

    if gdaltest.jp2lura_drv is not None:
        if gdal.GetConfigOption('LURA_LICENSE_NUM_1') is None or \
           gdal.GetConfigOption('LURA_LICENSE_NUM_2') is None:
            print('Driver JP2Lura is registered, but missing LURA_LICENSE_NUM_1 and LURA_LICENSE_NUM_2')
            gdaltest.jp2lura_drv = None
            return 'skip'

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2Lura')

    ds = gdal.Open('data/byte.jp2')
    if ds is None and gdal.GetLastErrorMsg().find('license') >= 0:
        print('Driver JP2Lura is registered, but issue with license')
        gdaltest.jp2lura_drv = None
        return 'skip'

    return 'success'

###############################################################################
# 

def jp2lura_missing_license_num():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    old_num_1 = gdal.GetConfigOption('LURA_LICENSE_NUM_1')
    old_num_2 = gdal.GetConfigOption('LURA_LICENSE_NUM_2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', '')
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', '')
    with gdaltest.error_handler():
        ds = gdal.Open('data/byte.jp2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', old_num_1)
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', old_num_2)

    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# 

def jp2lura_invalid_license_num():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    old_num_1 = gdal.GetConfigOption('LURA_LICENSE_NUM_1')
    old_num_2 = gdal.GetConfigOption('LURA_LICENSE_NUM_2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', '1')
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', '1')
    with gdaltest.error_handler():
        ds = gdal.Open('data/byte.jp2')
    gdal.SetConfigOption('LURA_LICENSE_NUM_1', old_num_1)
    gdal.SetConfigOption('LURA_LICENSE_NUM_2', old_num_2)

    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
def validate(filename, expected_gmljp2 = True, return_error_count = False, oidoc = None, inspire_tg = True):

    try:
        import validate_jp2
    except:
        print('Cannot run validate_jp2')
        return 'skip'

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET')
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xlink.xsd')
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xml.xsd')
        ogc_schemas_location = 'tmp/cache/SCHEMAS_OPENGIS_NET'
    except:
        ogc_schemas_location = 'disabled'

    if ogc_schemas_location != 'disabled':
        try:
            import xmlvalidate
            xmlvalidate.validate # to make pyflakes happy
        except:
            ogc_schemas_location = 'disabled'

    res = validate_jp2.validate(filename, oidoc, inspire_tg, expected_gmljp2, ogc_schemas_location)
    if return_error_count:
        return (res.error_count, res.warning_count)
    elif res.error_count == 0 and res.warning_count == 0:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Open byte.jp2

def jp2lura_2():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    srs = """PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","26711"]]
"""
    gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    tst = gdaltest.GDALTest( 'JP2Lura', 'byte.jp2', 1, 50054 )
    return tst.testOpen( check_prj = srs, check_gt = gt )

###############################################################################
# Open int16.jp2

def jp2lura_3():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/int16.jp2' )
    ds_ref = gdal.Open( 'data/int16.tif' )

    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Quite a bit of difference...
    if maxdiff > 6:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    ds = ogr.Open( 'data/int16.jp2' )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test copying byte.jp2

def jp2lura_4(out_filename = 'tmp/jp2lura_4.jp2'):

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.jp2')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()

    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/jp2lura_4.vrt', src_ds)
    vrt_ds.SetMetadataItem('TIFFTAG_XRESOLUTION', '300')
    vrt_ds.SetMetadataItem('TIFFTAG_YRESOLUTION', '200')
    vrt_ds.SetMetadataItem('TIFFTAG_RESOLUTIONUNIT', '3 (pixels/cm)')

    gdal.Unlink(out_filename)

    out_ds = gdal.GetDriverByName('JP2Lura').CreateCopy(out_filename, vrt_ds, options = ['REVERSIBLE=YES'])
    del out_ds

    vrt_ds = None
    gdal.Unlink('/vsimem/jp2lura_4.vrt')

    if gdal.VSIStatL(out_filename + '.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    if validate(out_filename, inspire_tg = False) == 'fail':
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open(out_filename)
    cs = ds.GetRasterBand(1).Checksum()
    got_wkt = ds.GetProjectionRef()
    got_gt = ds.GetGeoTransform()
    xres = ds.GetMetadataItem('TIFFTAG_XRESOLUTION')
    yres = ds.GetMetadataItem('TIFFTAG_YRESOLUTION')
    resunit = ds.GetMetadataItem('TIFFTAG_RESOLUTIONUNIT')
    ds = None

    gdal.Unlink(out_filename)

    if xres != '300' or yres != '200' or resunit != '3 (pixels/cm)':
        gdaltest.post_reason('bad resolution')
        print(xres)
        print(yres)
        print(resunit)
        return 'fail'

    sr1 = osr.SpatialReference()
    sr1.SetFromUserInput(got_wkt)
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(src_wkt)

    if sr1.IsSame(sr2) == 0:
        gdaltest.post_reason('bad spatial reference')
        print(got_wkt)
        print(src_wkt)
        return 'fail'

    for i in range(6):
        if abs(got_gt[i] - src_gt[i]) > 1e-8:
            gdaltest.post_reason('bad geotransform')
            print(got_gt)
            print(src_gt)
            return 'fail'

    if cs != 50054:
        gdaltest.post_reason('bad checksum')
        print(cs)
        return 'fail'

    return 'success'


def jp2lura_4_vsimem():
    return jp2lura_4('/vsimem/jp2lura_4.jp2')

###############################################################################
# Test copying int16.jp2

def jp2lura_5():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2Lura', 'int16.jp2', 1, None, options = ['REVERSIBLE=YES', 'CODEC=J2K'] )
    return tst.testCreateCopy()

###############################################################################
# Test reading ll.jp2

def jp2lura_6():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2Lura', 'll.jp2', 1, None )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('data/ll.jp2')
    ds.GetRasterBand(1).Checksum()
    ds = None

    return 'success'

###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)

def jp2lura_7():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2Lura', '/vsigzip/data/byte.jp2.gz', 1, 50054, filename_absolute = 1 )
    return tst.testOpen()

###############################################################################
# Test a JP2Lura with the 3 bands having 13bit depth and the 4th one 1 bit

def jp2lura_8():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open('data/3_13bit_and_1bit.jp2')

    expected_checksums = [ 64570, 57277, 56048 ] # 61292]

    for i in range(len(expected_checksums)):
        if ds.GetRasterBand(i+1).Checksum() != expected_checksums[i]:
            gdaltest.post_reason('unexpected checksum (%d) for band %d' % (expected_checksums[i], i+1))
            return 'fail'

    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('unexpected data type')
        return 'fail'

    return 'success'

###############################################################################
# Check that we can use .j2w world files (#4651)

def jp2lura_9():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/byte_without_geotransform.jp2' )

    geotransform = ds.GetGeoTransform()
    if abs(geotransform[0]-440720) > 0.1 \
        or abs(geotransform[1]-60) > 0.001 \
        or abs(geotransform[2]-0) > 0.001 \
        or abs(geotransform[3]-3751320) > 0.1 \
        or abs(geotransform[4]-0) > 0.001 \
        or abs(geotransform[5]- -60) > 0.001:
        print(geotransform)
        gdaltest.post_reason( 'geotransform differs from expected' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test YCBCR420 creation option

def DISABLED_jp2lura_10():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_10.jp2', src_ds, options = ['YCBCR420=YES', 'RESOLUTIONS=3'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    if out_ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason('fail')
        return 'fail'
    del out_ds
    src_ds = None
    gdal.Unlink('/vsimem/jp2lura_10.jp2')

    # Quite a bit of difference...
    if maxdiff > 12:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit

def DISABLED_jp2lura_11():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open('data/stefan_full_rgba_alpha_1bit.jp2')
    fourth_band = ds.GetRasterBand(4)
    if fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    got_cs = fourth_band.Checksum()
    if got_cs != 8527:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    jp2_bands_data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    jp2_fourth_band_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,int(ds.RasterXSize/16),int(ds.RasterYSize/16))

    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2lura_11.tif', ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    #gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/jp2lura_11.tif')
    if got_cs != 8527:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'

    if jp2_bands_data != gtiff_bands_data:
        gdaltest.post_reason('fail')
        return 'fail'

    if jp2_fourth_band_data != gtiff_fourth_band_data:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/stefan_full_rgba_alpha_1bit.jp2', open_options = ['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    if fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Check that PAM overrides internal georeferencing (#5279)

def jp2lura_12():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # Override projection
    shutil.copy('data/byte.jp2', 'tmp/jp2lura_12.jp2')

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    wkt = ds.GetProjectionRef()
    ds = None

    gdaltest.jp2lura_drv.Delete('tmp/jp2lura_12.jp2')

    if wkt.find('32631') < 0:
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    # Override geotransform
    shutil.copy('data/byte.jp2', 'tmp/jp2lura_12.jp2')

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    ds.SetGeoTransform([1000,1,0,2000,0,-1])
    ds = None

    ds = gdal.Open('tmp/jp2lura_12.jp2')
    gt = ds.GetGeoTransform()
    ds = None

    gdaltest.jp2lura_drv.Delete('tmp/jp2lura_12.jp2')

    if gt != (1000,1,0,2000,0,-1):
        gdaltest.post_reason('fail')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Check that PAM overrides internal GCPs (#5279)

def jp2lura_13():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # Create a dataset with GCPs
    src_ds = gdal.Open('data/rgb_gcp.vrt')
    ds = gdaltest.jp2lura_drv.CreateCopy('tmp/jp2lura_13.jp2', src_ds)
    ds = None
    src_ds = None

    if gdal.VSIStatL('tmp/jp2lura_13.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('tmp/jp2lura_13.jp2')
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    if count != 4:
        gdaltest.post_reason('fail')
        return 'fail'
    if len(gcps) != 4:
        gdaltest.post_reason('fail')
        return 'fail'
    if wkt.find('4326') < 0:
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'
    ds = None

    # Override GCP
    ds = gdal.Open('tmp/jp2lura_13.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    gcps = [ gdal.GCP(0,1,2,3,4) ]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2lura_13.jp2')
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    ds = None

    gdaltest.jp2lura_drv.Delete('tmp/jp2lura_13.jp2')

    if count != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if len(gcps) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if wkt.find('32631') < 0:
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Check that we get GCPs even there's no projection info

def jp2lura_14():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open('data/byte_2gcps.jp2')
    if ds.GetGCPCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test reading PixelIsPoint file (#5437)

def jp2lura_16():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/byte_point.jp2' )
    gt = ds.GetGeoTransform()
    if ds.GetMetadataItem('AREA_OR_POINT') != 'Point':
        gdaltest.post_reason( 'did not get AREA_OR_POINT = Point' )
        return 'fail'
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'TRUE' )

    ds = gdal.Open( 'data/byte_point.jp2' )
    gt = ds.GetGeoTransform()
    ds = None

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', None )

    gt_expected = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform with GTIFF_POINT_GEO_IGNORE TRUE' )
        return 'fail'

    return 'success'

###############################################################################
# Test writing PixelIsPoint file (#5437)

def jp2lura_17():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open( 'data/byte_point.jp2' )
    ds = gdaltest.jp2lura_drv.CreateCopy( '/vsimem/jp2lura_17.jp2', src_ds)
    ds = None
    src_ds = None

    if gdal.VSIStatL('/vsimem/jp2lura_17.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open( '/vsimem/jp2lura_17.jp2' )
    gt = ds.GetGeoTransform()
    if ds.GetMetadataItem('AREA_OR_POINT') != 'Point':
        gdaltest.post_reason( 'did not get AREA_OR_POINT = Point' )
        return 'fail'
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform' )
        return 'fail'

    gdal.Unlink( '/vsimem/jp2lura_17.jp2' )

    return 'success'

###############################################################################
# Test when using the decode_area API when one dimension of the dataset is not a
# multiple of 1024 (#5480)

def jp2lura_18():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('Mem').Create('',2000,2000)
    ds = gdaltest.jp2lura_drv.CreateCopy( '/vsimem/jp2lura_18.jp2', src_ds, options = [ 'TILEXSIZE=2000', 'TILEYSIZE=2000' ])
    ds = None
    src_ds = None

    ds = gdal.Open( '/vsimem/jp2lura_18.jp2' )
    ds.GetRasterBand(1).Checksum()
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink( '/vsimem/jp2lura_18.jp2' )

    return 'success'

###############################################################################
# Test reading file where GMLJP2 has nul character instead of \n (#5760)

def jp2lura_19():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open('data/byte_gmljp2_with_nul_car.jp2')
    if ds.GetProjectionRef() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Validate GMLJP2 content against schema

def jp2lura_20():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    try:
        import xmlvalidate
    except:
        print('Cannot import xmlvalidate')
        import traceback
        traceback.print_exc(file=sys.stdout)
        return 'skip'

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET.zip')
    except:
        try:
            os.stat('../ogr/tmp/cache/SCHEMAS_OPENGIS_NET.zip')
            shutil.copy('../ogr/tmp/cache/SCHEMAS_OPENGIS_NET.zip', 'tmp/cache')
        except:
            url = 'http://schemas.opengis.net/SCHEMAS_OPENGIS_NET.zip'
            if not gdaltest.download_file(url, 'SCHEMAS_OPENGIS_NET.zip', force_download = True, max_download_duration = 20):
                print('Cannot get SCHEMAS_OPENGIS_NET.zip')
                return 'skip'

    try:
        os.mkdir('tmp/cache/SCHEMAS_OPENGIS_NET')
    except:
        pass

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/gml/3.1.1/profiles/gmlJP2Profile/1.0.0/gmlJP2Profile.xsd')
    except:
        gdaltest.unzip( 'tmp/cache/SCHEMAS_OPENGIS_NET', 'tmp/cache/SCHEMAS_OPENGIS_NET.zip')


    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xlink.xsd')
    except:
        xlink_xsd_url = 'http://www.w3.org/1999/xlink.xsd'
        if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xlink.xsd', force_download = True, max_download_duration = 10):
            xlink_xsd_url = 'http://even.rouault.free.fr/xlink.xsd'
            if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xlink.xsd', force_download = True, max_download_duration = 10):
                print('Cannot get xlink.xsd')
                return 'skip'

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/xml.xsd')
    except:
        xlink_xsd_url = 'http://www.w3.org/1999/xml.xsd'
        if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xml.xsd', force_download = True, max_download_duration = 10):
            xlink_xsd_url = 'http://even.rouault.free.fr/xml.xsd'
            if not gdaltest.download_file(xlink_xsd_url, 'SCHEMAS_OPENGIS_NET/xml.xsd', force_download = True, max_download_duration = 10):
                print('Cannot get xml.xsd')
                return 'skip'

    xmlvalidate.transform_abs_links_to_ref_links('tmp/cache/SCHEMAS_OPENGIS_NET')

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_20.jp2', src_ds)
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    ds = None
    gdal.Unlink( '/vsimem/jp2lura_20.jp2' )

    if not xmlvalidate.validate(gmljp2, ogc_schemas_location = 'tmp/cache/SCHEMAS_OPENGIS_NET'):
        return 'fail'

    return 'success'

###############################################################################
# Test RGBA support

def jp2lura_22():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # RGBA
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options = ['REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2lura_22.jp2')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    if validate('/vsimem/jp2lura_22.jp2', expected_gmljp2 = False, inspire_tg = False) == 'fail':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/jp2lura_22.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    if False:
        # RGBA with 1BIT_ALPHA=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options = ['1BIT_ALPHA=YES'])
        del out_ds
        src_ds = None
        if gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.OpenEx('/vsimem/jp2lura_22.jp2', open_options = ['1BIT_ALPHA_PROMOTION=NO'])
        fourth_band = ds.GetRasterBand(4)
        if fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        if ds.GetRasterBand(4).Checksum() != 23120:
            gdaltest.post_reason('fail')
            print(ds.GetRasterBand(4).Checksum())
            return 'fail'
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        # RGBA with YCBCR420=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options = ['YCBCR420=YES'])
        del out_ds
        src_ds = None
        if gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_AlphaBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(1).Checksum() != 11457:
            gdaltest.post_reason('fail')
            print(ds.GetRasterBand(1).Checksum())
            return 'fail'
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        # RGBA with YCC=YES. Will emit a warning for now because of OpenJPEG
        # bug (only fixed in trunk, not released versions at that time)
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options = ['YCC=YES', 'QUALITY=100', 'REVERSIBLE=YES'])
        maxdiff = gdaltest.compare_ds(src_ds, out_ds)
        del out_ds
        src_ds = None
        if gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        if maxdiff > 0:
            gdaltest.post_reason('Image too different from reference')
            return 'fail'

        # RGB,undefined
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES'])
        maxdiff = gdaltest.compare_ds(src_ds, out_ds)
        del out_ds
        src_ds = None
        if gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_Undefined:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        if maxdiff > 0:
            gdaltest.post_reason('Image too different from reference')
            return 'fail'

        # RGB,undefined with ALPHA=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_22.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES', 'ALPHA=YES'])
        maxdiff = gdaltest.compare_ds(src_ds, out_ds)
        del out_ds
        src_ds = None
        if gdal.VSIStatL('/vsimem/jp2lura_22.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2lura_22.jp2')
        if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_AlphaBand:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None
        gdal.Unlink('/vsimem/jp2lura_22.jp2')

        if maxdiff > 0:
            gdaltest.post_reason('Image too different from reference')
            return 'fail'


    return 'success'

###############################################################################
# Test NBITS support

def DISABLED_jp2lura_23():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/uint16.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_23.jp2', src_ds, options = ['NBITS=9', 'QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2lura_23.jp2')
    if ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '9':
        gdaltest.post_reason('failure')
        return 'fail'

    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_23_2.jp2', ds)
    if out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '9':
        gdaltest.post_reason('failure')
        return 'fail'
    del out_ds

    ds = None
    if gdal.VSIStatL('/vsimem/jp2lura_23.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2lura_23.jp2')
    gdal.Unlink('/vsimem/jp2lura_23_2.jp2')


    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test Grey+alpha support

def jp2lura_24():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    #  Grey+alpha
    src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_24.jp2', src_ds, options = ['REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2lura_24.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2lura_24.jp2')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_GrayIndex:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    if validate('/vsimem/jp2lura_24.jp2', expected_gmljp2 = False, inspire_tg = False) == 'fail':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/jp2lura_24.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    if False:
        #  Grey+alpha with 1BIT_ALPHA=YES
        src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
        gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_24.jp2', src_ds, options = ['1BIT_ALPHA=YES'])

        src_ds = None
        if gdal.VSIStatL('/vsimem/jp2lura_24.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.OpenEx('/vsimem/jp2lura_24.jp2', open_options = ['1BIT_ALPHA_PROMOTION=NO'])
        if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_GrayIndex:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_AlphaBand:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None
        ds = gdal.Open('/vsimem/jp2lura_24.jp2')
        if ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is not None:
            gdaltest.post_reason('fail')
            print(ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE'))
            return 'fail'
        if ds.GetRasterBand(2).Checksum() != 23120:
            gdaltest.post_reason('fail')
            print(ds.GetRasterBand(2).Checksum())
            return 'fail'
        ds = None
        gdal.Unlink('/vsimem/jp2lura_24.jp2')

    return 'success'

###############################################################################
# Test multiband support

def jp2lura_25():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 5)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(250)
    src_ds.GetRasterBand(3).Fill(245)
    src_ds.GetRasterBand(4).Fill(240)
    src_ds.GetRasterBand(5).Fill(235)

    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_25.jp2', src_ds, options = ['REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2lura_25.jp2')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    if gdal.VSIStatL('/vsimem/jp2lura_25.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/jp2lura_25.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test CreateCopy() from a JPEG2000 with a 2048x2048 tiling

def jp2lura_27():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # Test optimization in GDALCopyWholeRasterGetSwathSize()
    # Not sure how we can check that except looking at logs with CPL_DEBUG=GDAL
    # for "GDAL: GDALDatasetCopyWholeRaster(): 2048*2048 swaths, bInterleave=1"
    src_ds = gdal.GetDriverByName('MEM').Create('', 2049, 2049, 4)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_27.jp2', src_ds, options = ['LEVELS=1','TILEXSIZE=2048', 'TILEYSIZE=2048'])
    src_ds = None
    #print('End of JP2 decoding')
    out2_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2lura_27.tif', out_ds, options=['TILED=YES'])
    out_ds = None
    del out2_ds
    gdal.Unlink('/vsimem/jp2lura_27.jp2')
    gdal.Unlink('/vsimem/jp2lura_27.tif')

    return 'success'

###############################################################################
# Test CODEBLOCK_WIDTH/_HEIGHT

XML_TYPE_IDX = 0
XML_VALUE_IDX = 1
XML_FIRST_CHILD_IDX = 2

def find_xml_node(ar, element_name, only_attributes = False):
    #type = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if value == element_name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        if only_attributes and child[XML_TYPE_IDX] != gdal.CXT_Attribute:
            continue
        found = find_xml_node(child, element_name)
        if found is not None:
            return found
    return None

def get_attribute_val(ar, attr_name):
    node = find_xml_node(ar, attr_name, True)
    if node is None or node[XML_TYPE_IDX] != gdal.CXT_Attribute:
        return None
    if len(ar) > XML_FIRST_CHILD_IDX and \
        node[XML_FIRST_CHILD_IDX][XML_TYPE_IDX] == gdal.CXT_Text:
        return node[XML_FIRST_CHILD_IDX][XML_VALUE_IDX]
    return None

def find_element_with_name(ar, element_name, name):
    type = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if type == gdal.CXT_Element and value == element_name and get_attribute_val(ar, 'name') == name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        found = find_element_with_name(child, element_name, name)
        if found:
            return found
    return None

def get_element_val(node):
    if node is None:
        return None
    for child_idx in range(XML_FIRST_CHILD_IDX, len(node)):
        child = node[child_idx]
        if child[XML_TYPE_IDX] == gdal.CXT_Text:
            return child[XML_VALUE_IDX]
    return None

def jp2lura_test_codeblock(filename, codeblock_width, codeblock_height):
    node = gdal.GetJPEG2000Structure(filename, ['ALL=YES'])
    xcb = 2**(2+int(get_element_val(find_element_with_name(node, "Field", "SPcod_xcb_minus_2"))))
    ycb = 2**(2+int(get_element_val(find_element_with_name(node, "Field", "SPcod_ycb_minus_2"))))
    if xcb != codeblock_width or ycb != codeblock_height:
        return False
    return True

def jp2lura_28():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)

    tests = [ ( ['CODEBLOCK_WIDTH=2'], 64, 64, True ),
              ( ['CODEBLOCK_WIDTH=2048'], 64, 64, True ),
              ( ['CODEBLOCK_HEIGHT=2'], 64, 64, True ),
              ( ['CODEBLOCK_HEIGHT=2048'], 64, 64, True ),
              ( ['CODEBLOCK_WIDTH=128', 'CODEBLOCK_HEIGHT=128'], 64, 64, True ),
              ( ['CODEBLOCK_WIDTH=63'], 32, 64, True ),
              ( ['CODEBLOCK_WIDTH=32', 'CODEBLOCK_HEIGHT=32'], 32, 32, False ),
            ]

    for (options, expected_cbkw, expected_cbkh, warning_expected) in tests:
        gdal.ErrorReset()
        gdal.PushErrorHandler()
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_28.jp2', src_ds, options = options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('warning expected')
            print(options)
            return 'fail'
        del out_ds
        if not jp2lura_test_codeblock('/vsimem/jp2lura_28.jp2', expected_cbkw, expected_cbkh):
            gdaltest.post_reason('unexpected codeblock size')
            print(options)
            return 'fail'

    gdal.Unlink('/vsimem/jp2lura_28.jp2')

    return 'success'

###############################################################################
# Test color table support

def jp2lura_30():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,255,255,255) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )
    src_ds.GetRasterBand( 1 ).SetRasterColorTable( ct )

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_30.jp2', src_ds)
    gdal.PopErrorHandler()
    if out_ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test unusual band color interpretation order

def DISABLED_jp2lura_31():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 3)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_31.jp2', src_ds)
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2lura_31.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2lura_31.jp2')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_GreenBand:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetColorInterpretation())
        return 'fail'
    if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/jp2lura_31.jp2')


    # With alpha now
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 4)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_31.jp2', src_ds)
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2lura_31.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2lura_31.jp2')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetColorInterpretation())
        return 'fail'
    if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_GreenBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(3).GetColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/jp2lura_31.jp2')

    return 'success'

###############################################################################
# Test crazy tile size

def DISABLED_jp2lura_33():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open("""<VRTDataset rasterXSize="100000" rasterYSize="100000">
  <VRTRasterBand dataType="Byte" band="1">
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_33.jp2', src_ds, options = ['BLOCKXSIZE=100000', 'BLOCKYSIZE=100000'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    gdal.Unlink('/vsimem/jp2lura_33.jp2')

    return 'success'

###############################################################################
# Test opening a file whose dimensions are > 2^31-1

def jp2lura_34():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    ds = gdal.Open('data/dimensions_above_31bit.jp2')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'


###############################################################################
# Test opening a truncated file

def jp2lura_35():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    ds = gdal.Open('data/truncated.jp2')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test we cannot create files with more than 16384 bands

def jp2lura_36():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 16385)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_36.jp2', src_ds)
    gdal.PopErrorHandler()
    if out_ds is not None or gdal.VSIStatL('/vsimem/jp2lura_36.jp2') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test metadata reading & writing

def jp2lura_37():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = ['WRITE_METADATA=YES'])
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    if ds.GetMetadata() != {}:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Simple metadata in main domain
    for options in [ ['WRITE_METADATA=YES'] ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadataItem('FOO', 'BAR')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = options)
        del out_ds
        if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2lura_37.jp2')
        if ds.GetMetadata() != {'FOO': 'BAR'}:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Simple metadata in auxiliary domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadataItem('FOO', 'BAR', 'SOME_DOMAIN')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = ['WRITE_METADATA=YES'])
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    md = ds.GetMetadata('SOME_DOMAIN')
    if md != {'FOO': 'BAR'}:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Simple metadata in auxiliary XML domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadata( [ '<some_arbitrary_xml_box/>' ], 'xml:SOME_DOMAIN')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = ['WRITE_METADATA=YES'])
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    if ds.GetMetadata('xml:SOME_DOMAIN')[0] != '<some_arbitrary_xml_box />\n':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Special xml:BOX_ metadata domain
    for options in [ ['WRITE_METADATA=YES'] ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata( [ '<some_arbitrary_xml_box/>' ], 'xml:BOX_1')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = options)
        del out_ds
        if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2lura_37.jp2')
        if ds.GetMetadata('xml:BOX_0')[0] != '<some_arbitrary_xml_box/>':
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Special xml:XMP metadata domain
    for options in [ ['WRITE_METADATA=YES'] ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata( [ '<fake_xmp_box/>' ], 'xml:XMP')
        out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = options)
        del out_ds
        if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2lura_37.jp2')
        if ds.GetMetadata('xml:XMP')[0] != '<fake_xmp_box/>':
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/jp2lura_37.jp2')

    # Special xml:IPR metadata domain
    #for options in [ ['WRITE_METADATA=YES'] ]:
    #    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    #    src_ds.SetMetadata( [ '<fake_ipr_box/>' ], 'xml:IPR')
    #    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_37.jp2', src_ds, options = options)
    #    del out_ds
    #    if gdal.VSIStatL('/vsimem/jp2lura_37.jp2.aux.xml') is not None:
    #        gdaltest.post_reason('fail')
    #        return 'fail'
    #    ds = gdal.Open('/vsimem/jp2lura_37.jp2')
    #    if ds.GetMetadata('xml:IPR')[0] != '<fake_ipr_box/>':
    #        gdaltest.post_reason('fail')
    #        return 'fail'
    #    ds = None

        gdal.Unlink('/vsimem/jp2lura_37.jp2')

    return 'success'

###############################################################################
# Test non-EPSG SRS (so written with a GML dictionary)

def jp2lura_38():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    wkt = """PROJCS["UTM Zone 31, Northern Hemisphere",GEOGCS["unnamed ellipse",DATUM["unknown",SPHEROID["unnamed",100,1]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]"""
    src_ds.SetProjection(wkt)
    src_ds.SetGeoTransform([0,60,0,0,0,-60])
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_38.jp2', src_ds, options = ['GeoJP2=NO'])
    if out_ds.GetProjectionRef() != wkt:
        gdaltest.post_reason('fail')
        print(out_ds.GetProjectionRef())
        return 'fail'
    crsdictionary = out_ds.GetMetadata_List("xml:CRSDictionary.gml")[0]
    out_ds = None
    gdal.Unlink('/vsimem/jp2lura_38.jp2')

    do_validate = False
    try:
        import xmlvalidate
        do_validate = True
    except:
        print('Cannot import xmlvalidate')
        pass

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET')
    except:
        do_validate = False

    if do_validate:
        if not xmlvalidate.validate(crsdictionary, ogc_schemas_location = 'tmp/cache/SCHEMAS_OPENGIS_NET'):
            print(crsdictionary)
            return 'fail'

    return 'success'

###############################################################################
# Test GMLJP2OVERRIDE configuration option and DGIWG GMLJP2

def jp2lura_39():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    src_ds.SetGeoTransform([0,60,0,0,0,-60])
    gdal.SetConfigOption('GMLJP2OVERRIDE', '/vsimem/override.gml')
    # This GML has srsName only on RectifiedGrid (taken from D.2.2.2 from DGIWG_Profile_of_JPEG2000_for_Georeferenced_Imagery.pdf)
    gdal.FileFromMemBuffer('/vsimem/override.gml', """<?xml version="1.0" encoding="UTF-8"?>
<gml:FeatureCollection xmlns:gml="http://www.opengis.net/gml"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xmlns:gmd="http://www.isotc211.org/2005/gmd"
                       xmlns:gco="http://www.isotc211.org/2005/gco"
                       xsi:schemaLocation="http://www.opengis.net/gml file:///D:/dgiwg/jp2/GML-3.1.1/profiles/DGIWGgmlJP2Profile/1.1.0/DGIWGgmlJP2Profile.xsd">
  <gml:featureMember>
    <gml:FeatureCollection>
      <!-- feature collection for a specific codestream -->
      <gml:featureMember>
        <gml:RectifiedGridCoverage>
          <gml:rectifiedGridDomain>
            <gml:RectifiedGrid dimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
              <gml:limits>
                <gml:GridEnvelope>
                  <!-- Image coordinates -->
                  <gml:low>0 0</gml:low>
                  <gml:high>4999 9999</gml:high>
                </gml:GridEnvelope>
              </gml:limits>
              <gml:axisName>X</gml:axisName>
              <gml:axisName>Y</gml:axisName>
              <!-- The origin location in geo coordinates -->
              <gml:origin>
                <gml:Point>
                  <gml:pos>19.1234567 37.1234567</gml:pos>
                </gml:Point>
              </gml:origin>
              <!--offsetVectors says how much offset each pixel will contribute to, in practice, that is the cell size -->
              <gml:offsetVector>0.0 0.00001234</gml:offsetVector>
              <gml:offsetVector> -0.00001234 0.0</gml:offsetVector>
            </gml:RectifiedGrid>
          </gml:rectifiedGridDomain>
          <!--A RectifiedGridCoverage uses the rangeSet to describe the data below is a description of the range of values described by the grid coverage -->
          <gml:rangeSet>
            <gml:File>
              <gml:rangeParameters/>
              <gml:fileName>gmljp2://codestream/0</gml:fileName>
              <gml:fileStructure>Record Interleaved</gml:fileStructure>
            </gml:File>
          </gml:rangeSet>
        </gml:RectifiedGridCoverage>
      </gml:featureMember>
    </gml:FeatureCollection>
  </gml:featureMember>
</gml:FeatureCollection>""")
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_39.jp2', src_ds, options = ['GeoJP2=NO'])
    gdal.SetConfigOption('GMLJP2OVERRIDE', None)
    gdal.Unlink('/vsimem/override.gml')
    del out_ds
    ds = gdal.Open('/vsimem/jp2lura_39.jp2')
    if ds.GetProjectionRef().find('4326') < 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/jp2lura_39.jp2')

    return 'success'

###############################################################################
# Test we can parse GMLJP2 v2.0

def jp2lura_40():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    src_ds.SetGeoTransform([0,60,0,0,0,-60])
    gdal.SetConfigOption('GMLJP2OVERRIDE', '/vsimem/override.gml')

    gdal.FileFromMemBuffer('/vsimem/override.gml', """<?xml version="1.0" encoding="UTF-8"?>
<?xml version="1.0" encoding="UTF-8"?>
<gmljp2:GMLJP2CoverageCollection gml:id="JPEG2000_0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:gmlcov="http://www.opengis.net/gmlcov/1.0"
    xmlns:gmljp2="http://www.opengis.net/gmljp2/2.0"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:schemaLocation="http://www.opengis.net/gmljp2/2.0 http://schemas.opengis.net/gmljp2/2.0/gmljp2.xsd">
    <gml:gridDomain/>
    <gml:rangeSet>
        <gml:File>
            <gml:rangeParameters/>
            <gml:fileName>gmljp2://codestream</gml:fileName>
            <gml:fileStructure>inapplicable</gml:fileStructure>
        </gml:File>
    </gml:rangeSet>
    <gmlcov:rangeType/>
    <gmljp2:featureMember>
        <gmljp2:GMLJP2RectifiedGridCoverage gml:id="CodeStream">
            <gml:domainSet>
                <gml:RectifiedGrid gml:id="rg0001" dimension="2"
                            srsName="http://www.opengis.net/def/crs/EPSG/0/4326">
                    <gml:limits>
                        <gml:GridEnvelope>
                            <gml:low>0 0</gml:low>
                            <gml:high>19 19</gml:high>
                        </gml:GridEnvelope>
                    </gml:limits>
                    <gml:axisLabels>Lat Long</gml:axisLabels>
                    <gml:origin>
                        <gml:Point gml:id="P0001" srsName="http://www.opengis.net/def/crs/EPSG/0/4326">
                            <gml:pos>48.95 2.05</gml:pos>
                        </gml:Point>
                    </gml:origin>
                    <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/4326">0 0.1</gml:offsetVector>
                    <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/4326">-0.1 0</gml:offsetVector>
                </gml:RectifiedGrid>
            </gml:domainSet>
            <gml:rangeSet>
                <gml:File>
                    <gml:rangeParameters/>
                    <gml:fileName>gmljp2://codestream</gml:fileName>
                    <gml:fileStructure>inapplicable</gml:fileStructure>
                </gml:File>
            </gml:rangeSet>
            <gmlcov:rangeType/>
        </gmljp2:GMLJP2RectifiedGridCoverage>
    </gmljp2:featureMember>
</gmljp2:GMLJP2CoverageCollection>""")
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_40.jp2', src_ds, options = ['GeoJP2=NO'])
    gdal.SetConfigOption('GMLJP2OVERRIDE', None)
    gdal.Unlink('/vsimem/override.gml')
    del out_ds
    ds = gdal.Open('/vsimem/jp2lura_40.jp2')
    if ds.GetProjectionRef().find('4326') < 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (2, 0.1, 0, 49, 0, -0.1)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-5:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    ds = None
    gdal.Unlink('/vsimem/jp2lura_40.jp2')

    return 'success'

###############################################################################
# Test USE_SRC_CODESTREAM=YES

def jp2lura_41():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.jp2')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_41.jp2', src_ds, \
        options = ['USE_SRC_CODESTREAM=YES', '@PROFILE=PROFILE_1', 'GEOJP2=NO', 'GMLJP2=NO'])
    if src_ds.GetRasterBand(1).Checksum() != out_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        return 'fail'
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2lura_41.jp2').size != 9923:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2lura_41.jp2')
    gdal.Unlink('/vsimem/jp2lura_41.jp2.aux.xml')

    # Warning if ignored option
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_41.jp2', src_ds, \
        options = ['USE_SRC_CODESTREAM=YES', 'QUALITY=1'])
    gdal.PopErrorHandler()
    del out_ds
    #if gdal.GetLastErrorMsg() == '':
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    gdal.Unlink('/vsimem/jp2lura_41.jp2')
    gdal.Unlink('/vsimem/jp2lura_41.jp2.aux.xml')

    # Warning if source is not JPEG2000
    src_ds = gdal.Open('data/byte.tif')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_41.jp2', src_ds, \
        options = ['USE_SRC_CODESTREAM=YES'])
    gdal.PopErrorHandler()
    del out_ds
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2lura_41.jp2')

    return 'success'

###############################################################################
# Get structure of a JPEG2000 file

def jp2lura_43():

    ret = gdal.GetJPEG2000StructureAsString('data/byte.jp2', ['ALL=YES'])
    if ret is None:
        return 'fail'

    return 'success'

###############################################################################
# Test GMLJP2v2

def jp2lura_45():

    if gdaltest.jp2lura_drv is None:
        return 'skip'
    if gdal.GetDriverByName('GML') is None:
        return 'skip'
    if gdal.GetDriverByName('KML') is None and gdal.GetDriverByName('LIBKML') is None:
        return 'skip'

    # Test GMLJP2V2_DEF=YES
    src_ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_45.jp2', src_ds, options = ['GMLJP2V2_DEF=YES'])
    if out_ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetLayer(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    del out_ds

    ds = gdal.Open('/vsimem/jp2lura_45.jp2')
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    minimal_instance = """<gmljp2:GMLJP2CoverageCollection gml:id="ID_GMLJP2_0"
     xmlns:gml="http://www.opengis.net/gml/3.2"
     xmlns:gmlcov="http://www.opengis.net/gmlcov/1.0"
     xmlns:gmljp2="http://www.opengis.net/gmljp2/2.0"
     xmlns:swe="http://www.opengis.net/swe/2.0"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://www.opengis.net/gmljp2/2.0 http://schemas.opengis.net/gmljp2/2.0/gmljp2.xsd">
  <gml:domainSet nilReason="inapplicable"/>
  <gml:rangeSet>
    <gml:DataBlock>
       <gml:rangeParameters nilReason="inapplicable"/>
       <gml:doubleOrNilReasonTupleList>inapplicable</gml:doubleOrNilReasonTupleList>
     </gml:DataBlock>
  </gml:rangeSet>
  <gmlcov:rangeType>
    <swe:DataRecord>
      <swe:field name="Collection"> </swe:field>
    </swe:DataRecord>
  </gmlcov:rangeType>
  <gmljp2:featureMember>
   <gmljp2:GMLJP2RectifiedGridCoverage gml:id="RGC_1_ID_GMLJP2_0">
     <gml:boundedBy>
       <gml:Envelope srsDimension="2" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
         <gml:lowerCorner>440720 3750120</gml:lowerCorner>
         <gml:upperCorner>441920 3751320</gml:upperCorner>
       </gml:Envelope>
     </gml:boundedBy>
     <gml:domainSet>
      <gml:RectifiedGrid gml:id="RGC_1_GRID_ID_GMLJP2_0" dimension="2" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
       <gml:limits>
         <gml:GridEnvelope>
           <gml:low>0 0</gml:low>
           <gml:high>19 19</gml:high>
         </gml:GridEnvelope>
       </gml:limits>
       <gml:axisName>x</gml:axisName>
       <gml:axisName>y</gml:axisName>
       <gml:origin>
         <gml:Point gml:id="P0001" srsName="http://www.opengis.net/def/crs/EPSG/0/26711">
           <gml:pos>440750 3751290</gml:pos>
         </gml:Point>
       </gml:origin>
       <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/26711">60 0</gml:offsetVector>
       <gml:offsetVector srsName="http://www.opengis.net/def/crs/EPSG/0/26711">0 -60</gml:offsetVector>
      </gml:RectifiedGrid>
     </gml:domainSet>
     <gml:rangeSet>
      <gml:File>
        <gml:rangeParameters/>
        <gml:fileName>gmljp2://codestream/0</gml:fileName>
        <gml:fileStructure>inapplicable</gml:fileStructure>
      </gml:File>
     </gml:rangeSet>
     <gmlcov:rangeType></gmlcov:rangeType>
   </gmljp2:GMLJP2RectifiedGridCoverage>
  </gmljp2:featureMember>
</gmljp2:GMLJP2CoverageCollection>
"""
    if gmljp2 != minimal_instance:
        gdaltest.post_reason('fail')
        print(gmljp2)
        return 'fail'

    gdal.Unlink('/vsimem/jp2lura_45.jp2')

    return 'success'

###############################################################################
# Test writing & reading RPC in GeoJP2 box

def jp2lura_47():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/byte_rpc.tif')
    out_ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_47.jp2', src_ds)
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2lura_47.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('/vsimem/jp2lura_47.jp2')
    if ds.GetMetadata('RPC') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/jp2lura_47.jp2')

    return 'success'

###############################################################################
# Test reading a dataset whose tile dimensions are larger than dataset ones

def jp2lura_48():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    ds = gdal.Open('data/byte_tile_2048.jp2')
    (blockxsize, blockysize) = ds.GetRasterBand(1).GetBlockSize()
    if (blockxsize, blockysize) != (20,20):
        gdaltest.post_reason('fail')
        print(blockxsize, blockysize)
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 4610:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
def jp2lura_online_1():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        return 'skip'

    # Checksum = 32669 on my PC
    tst = gdaltest.GDALTest( 'JP2Lura', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

    return 'success'

###############################################################################
def jp2lura_online_2():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        return 'skip'

    # Checksum = 15621 on my PC
    tst = gdaltest.GDALTest( 'JP2Lura', 'tmp/cache/gcp.jp2', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/gcp.jp2')
    ds.GetRasterBand(1).Checksum()
    if len(ds.GetGCPs()) != 15:
        gdaltest.post_reason('bad number of GCP')
        return 'fail'

    expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]"""
    if ds.GetGCPProjection() != expected_wkt:
        gdaltest.post_reason('bad GCP projection')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
def jp2lura_online_3():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2Lura', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/Bretagne1.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne1.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    if maxdiff > 17:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
def jp2lura_online_4():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2Lura', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'expected_fail'

    ds = gdal.Open('tmp/cache/Bretagne2.j2k')
    ds_ref = gdal.Open('tmp/cache/Bretagne2.bmp')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, 0, 0, 1024, 1024)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    if maxdiff > 10:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Try reading JP2Lura with color table

def jp2lura_online_5():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9.jp2', 'file9.jp2'):
        return 'skip'

    ds = gdal.Open('tmp/cache/file9.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    if cs1 != 47664:
        gdaltest.post_reason('Did not get expected checksums')
        print(cs1)
        return 'fail'
    if ds.GetRasterBand(1).GetColorTable() is None:
        gdaltest.post_reason('Did not get expected color table')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Try reading YCbCr JP2Lura as RGB

def jp2lura_online_6():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_03/file3.jp2', 'file3.jp2'):
        return 'skip'

    ds = gdal.Open('tmp/cache/file3.jp2')
    #cs1 = ds.GetRasterBand(1).Checksum()
    #cs2 = ds.GetRasterBand(2).Checksum()
    #cs3 = ds.GetRasterBand(3).Checksum()
    #if cs1 != 26140 or cs2 != 32689 or cs3 != 48247:
    #    print(cs1, cs2, cs3)
    #    gdaltest.post_reason('Did not get expected checksums')
    #    return 'fail'
    if ds is not None:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test GDAL_GEOREF_SOURCES

def jp2lura_49():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    tests = [ (None, True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              (None, True, False, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              (None, False, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              (None, False, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('INTERNAL', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('INTERNAL,PAM', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              ('INTERNAL,WORLDFILE', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('INTERNAL,PAM,WORLDFILE', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              ('INTERNAL,WORLDFILE,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM,INTERNAL', False, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('PAM,WORLDFILE,INTERNAL', False, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('PAM', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              ('PAM,WORLDFILE', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              ('WORLDFILE', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,INTERNAL', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM,INTERNAL', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,INTERNAL,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('NONE', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ]

    for (config_option_value, copy_pam, copy_worldfile, expected_srs, expected_gt) in tests:
        gdal.SetConfigOption('GDAL_GEOREF_SOURCES', config_option_value)
        gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.jp2', open('data/byte_nogeoref.jp2', 'rb').read())
        if copy_pam:
            gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.jp2.aux.xml', open('data/byte_nogeoref.jp2.aux.xml', 'rb').read())
        if copy_worldfile:
            gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.j2w', open('data/byte_nogeoref.j2w', 'rb').read())
        ds = gdal.Open('/vsimem/byte_nogeoref.jp2')
        gt = ds.GetGeoTransform()
        srs_wkt = ds.GetProjectionRef()
        ds = None
        gdal.SetConfigOption('GDAL_GEOREF_SOURCES', None)
        gdal.Unlink('/vsimem/byte_nogeoref.jp2')
        gdal.Unlink('/vsimem/byte_nogeoref.jp2.aux.xml')
        gdal.Unlink('/vsimem/byte_nogeoref.j2w')

        if gt != expected_gt:
            gdaltest.post_reason('Did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value,str(copy_pam),str(copy_worldfile)))
            print('Got ' + str(gt))
            print('Expected ' + str(expected_gt))
            return 'fail'

        if (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and srs_wkt.find(expected_srs) < 0):
            gdaltest.post_reason('Did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value,str(copy_pam),str(copy_worldfile)))
            print('Got ' + srs_wkt)
            print('Expected ' + expected_srs)
            return 'fail'

    tests = [ (None, True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              (None, True, False, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              (None, False, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              (None, False, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('INTERNAL', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('INTERNAL,PAM', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('INTERNAL,WORLDFILE', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('INTERNAL,PAM,WORLDFILE', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('INTERNAL,WORLDFILE,PAM', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('WORLDFILE,PAM,INTERNAL', False, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('PAM,WORLDFILE,INTERNAL', False, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('GEOJP2', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('GEOJP2,GMLJP2', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('GMLJP2', True, True, '26712', (439970.0, 60.0, 0.0, 3751030.0, 0.0, -60.0)),
              ('GMLJP2,GEOJP2', True, True, '26712', (439970.0, 60.0, 0.0, 3751030.0, 0.0, -60.0)),
              ('MSIG', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('MSIG,GMLJP2,GEOJP2', True, True, '26712', (439970.0, 60.0, 0.0, 3751030.0, 0.0, -60.0)),
              ('MSIG,GEOJP2,GMLJP2', True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('PAM', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              ('PAM,WORLDFILE', True, True, 'LOCAL_CS["PAM"]', (100.0, 1.0, 0.0, 300.0, 0.0, -1.0)),
              ('WORLDFILE', True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,INTERNAL', True, True, '26711', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM,INTERNAL', True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,INTERNAL,PAM', True, True, '26711', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('NONE', True, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ]

    for (config_option_value, copy_pam, copy_worldfile, expected_srs, expected_gt) in tests:
        gdal.FileFromMemBuffer('/vsimem/inconsitant_geojp2_gmljp2.jp2', open('data/inconsitant_geojp2_gmljp2.jp2', 'rb').read())
        if copy_pam:
            gdal.FileFromMemBuffer('/vsimem/inconsitant_geojp2_gmljp2.jp2.aux.xml', open('data/inconsitant_geojp2_gmljp2.jp2.aux.xml', 'rb').read())
        if copy_worldfile:
            gdal.FileFromMemBuffer('/vsimem/inconsitant_geojp2_gmljp2.j2w', open('data/inconsitant_geojp2_gmljp2.j2w', 'rb').read())
        open_options = []
        if config_option_value is not None:
            open_options += [ 'GEOREF_SOURCES=' + config_option_value ]
        ds = gdal.OpenEx('/vsimem/inconsitant_geojp2_gmljp2.jp2', open_options = open_options)
        gt = ds.GetGeoTransform()
        srs_wkt = ds.GetProjectionRef()
        ds = None
        gdal.Unlink('/vsimem/inconsitant_geojp2_gmljp2.jp2')
        gdal.Unlink('/vsimem/inconsitant_geojp2_gmljp2.jp2.aux.xml')
        gdal.Unlink('/vsimem/inconsitant_geojp2_gmljp2.j2w')

        if gt != expected_gt:
            gdaltest.post_reason('Did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value,str(copy_pam),str(copy_worldfile)))
            print('Got ' + str(gt))
            print('Expected ' + str(expected_gt))
            return 'fail'

        if (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and srs_wkt.find(expected_srs) < 0):
            gdaltest.post_reason('Did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s' % (config_option_value,str(copy_pam),str(copy_worldfile)))
            print('Got ' + srs_wkt)
            print('Expected ' + expected_srs)
            return 'fail'

    ds = gdal.OpenEx('data/inconsitant_geojp2_gmljp2.jp2', open_options = [ 'GEOREF_SOURCES=PAM,WORLDFILE' ] )
    fl = ds.GetFileList()
    if set(fl) != set(['data/inconsitant_geojp2_gmljp2.jp2', 'data/inconsitant_geojp2_gmljp2.jp2.aux.xml']):
        gdaltest.post_reason('Did not get expected filelist')
        print(fl)
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.OpenEx('data/inconsitant_geojp2_gmljp2.jp2', open_options = [ 'GEOREF_SOURCES=unhandled' ] )
        if gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('expected warning')
            return 'fail'

    return 'success'


###############################################################################
# Test reading split IEEE-754 Float32

def jp2lura_50():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2Lura', 'float32_ieee754_split_reversible.jp2', 1, 4672 )
    return tst.testOpen()

    return 'success'

###############################################################################
# Test split IEEE-754 Float32

def jp2lura_51():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # Don't allow it by default
    src_ds = gdal.Open('data/float32.tif')
    with gdaltest.error_handler():
        ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                         options = ['SPLIT_IEEE754=YES'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None

    if validate('/vsimem/jp2lura_51.jp2', inspire_tg = False) == 'fail':
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')

    if maxdiff > 0.01:
        gdaltest.post_reason('fail')
        return 'fail'

    # QUALITY
    with gdaltest.error_handler():
        ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                         options = ['SPLIT_IEEE754=YES', 'QUALITY=100'])
    if ds is not None:
        maxdiff = gdaltest.compare_ds(ds, src_ds)
        ds = None
        if maxdiff > 124:
            gdaltest.post_reason('fail')
            print(maxdiff)
            return 'fail'

        if validate('/vsimem/jp2lura_51.jp2', inspire_tg = False) == 'fail':
            gdaltest.post_reason('fail')
            return 'fail'
    ds = None
    with gdaltest.error_handler():
        gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')
    gdal.Unlink('/vsimem/jp2lura_51.jp2')

    # RATE
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                         options = ['SPLIT_IEEE754=YES', 'RATE=1'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    if maxdiff > 370:
        gdaltest.post_reason('fail')
        print(maxdiff)
        return 'fail'

    if validate('/vsimem/jp2lura_51.jp2', inspire_tg = False) == 'fail':
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')


    # Test reversible
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_51.jp2', src_ds,
                                         options = ['SPLIT_IEEE754=YES', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    if maxdiff != 0.0:
        gdaltest.post_reason('fail')
        print(maxdiff)
        return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_51.jp2')

    return 'success'

###############################################################################
# Test other data types

def jp2lura_52():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    tests =  [ [-32768, gdal.GDT_Int16, 'h'],
               [-1, gdal.GDT_Int16, 'h'],
               [ 32767, gdal.GDT_Int16, 'h'],
               [     0, gdal.GDT_UInt16, 'H'],
               [ 65535, gdal.GDT_UInt16, 'H'],
               [-2 ** 27, gdal.GDT_Int32, 'i'],
               [ 2 ** 27 - 1, gdal.GDT_Int32, 'i'],
               [           0, gdal.GDT_UInt32, 'I'],
               [ 2 ** 28 - 1, gdal.GDT_UInt32, 'I'],
             ]
    for (val, dt, fmt) in tests:

        src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1, dt)
        src_ds.GetRasterBand(1).Fill(val)
        ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_52.jp2', src_ds,
                                             options = ['REVERSIBLE=YES'])
        got_min, got_max = ds.GetRasterBand(1).ComputeRasterMinMax()
        if val != got_min or val != got_max:
            gdaltest.post_reason('fail')
            print(val, dt, fmt, got_min, got_max)
            return 'fail'
        ds = None

        if val >= 0 and validate('/vsimem/jp2lura_52.jp2', expected_gmljp2 = False, inspire_tg = False) == 'fail':
            gdaltest.post_reason('fail')
            print(val, dt, fmt)
            return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_52.jp2')

    return 'success'

###############################################################################
# Test RATE and QUALITY

def jp2lura_53():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')

    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options = [ 'RATE=1' ] )
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    if maxdiff > 8:
        gdaltest.post_reason('fail')
        print(maxdiff)
        return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')


    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options = [ 'QUALITY=100' ] )
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    if maxdiff > 2:
        gdaltest.post_reason('fail')
        print(maxdiff)
        return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')

    # Forcing irreversible due to RATE
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options = [ 'REVERSIBLE=YES', 'RATE=1' ] )
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    if maxdiff > 8:
        gdaltest.post_reason('fail')
        print(maxdiff)
        return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')


    # QUALITY ignored
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_53.jp2', src_ds,
                                         options = [ 'REVERSIBLE=YES', 'QUALITY=100' ] )
    maxdiff = gdaltest.compare_ds(ds, src_ds)
    ds = None
    if maxdiff > 0:
        gdaltest.post_reason('fail')
        print(maxdiff)
        return 'fail'

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_53.jp2')


    return 'success'

###############################################################################
# Test RasterIO edge cases

def jp2lura_54():

    if gdaltest.jp2lura_drv is None:
        return 'skip'

    # Tiled with incomplete boundary tiles
    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 1)
    src_ds.GetRasterBand(1).Fill(100)
    ds = gdaltest.jp2lura_drv.CreateCopy('/vsimem/jp2lura_54.jp2', src_ds,
                options = ['REVERSIBLE=YES', 'TILEXSIZE=64', 'TILEYSIZE=64'])
    # Request with a type that is not the natural type
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 100, 100, 100, 100,
                                          buf_type = gdal.GDT_Int16)
    data = struct.unpack('h'* 100 * 100, data)
    if min(data) != 100 or max(data) != 100:
        gdaltest.post_reason('fail')
        print(min(data))
        print(max(data))
        return 'fail'

    # Request at a resolution that is not a power of two
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 100, 100, 30, 30)
    data = struct.unpack('B'* 30 * 30, data)
    if min(data) != 100 or max(data) != 100:
        gdaltest.post_reason('fail')
        print(min(data))
        print(max(data))
        return 'fail'

    ds = None

    gdaltest.jp2lura_drv.Delete('/vsimem/jp2lura_54.jp2')

    return 'success'


###############################################################################
def jp2lura_cleanup():

    gdaltest.reregister_all_jpeg2000_drivers()

    return 'success'

gdaltest_list = [
    jp2lura_1,
    jp2lura_missing_license_num,
    jp2lura_invalid_license_num,
    jp2lura_2,
    jp2lura_3,
    jp2lura_4,
    jp2lura_4_vsimem,
    jp2lura_5,
    jp2lura_6,
    jp2lura_7,
    jp2lura_8,
    jp2lura_9,
    #jp2lura_10,
    #jp2lura_11,
    jp2lura_12,
    jp2lura_13,
    jp2lura_14,
    jp2lura_16,
    jp2lura_17,
    jp2lura_18,
    jp2lura_19,
    jp2lura_20,
    #jp2lura_21,
    jp2lura_22,
    #jp2lura_23,
    jp2lura_24,
    jp2lura_25,
    #jp2lura_26,
    jp2lura_27,
    jp2lura_28,
    #jp2lura_29,
    jp2lura_30,
    #jp2lura_31,
    #jp2lura_32,
    #jp2lura_33,
    jp2lura_34,
    jp2lura_35,
    jp2lura_36,
    jp2lura_37,
    jp2lura_38,
    jp2lura_39,
    jp2lura_40,
    jp2lura_41,
    #jp2lura_42,
    jp2lura_43,
    #jp2lura_44,
    jp2lura_45,
    #jp2lura_46,
    jp2lura_47,
    jp2lura_48,
    jp2lura_49,
    jp2lura_50,
    jp2lura_51,
    jp2lura_52,
    jp2lura_53,
    jp2lura_54,
    jp2lura_online_1,
    jp2lura_online_2,
    jp2lura_online_3,
    jp2lura_online_4,
    jp2lura_online_5,
    jp2lura_online_6,
    jp2lura_cleanup ]

disabled_gdaltest_list = [
    jp2lura_1,
    jp2lura_53,
    jp2lura_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'jp2lura' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

