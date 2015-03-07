#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JP2OpenJPEG driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def jp2openjpeg_1():

    try:
        gdaltest.jp2openjpeg_drv = gdal.GetDriverByName( 'JP2OpenJPEG' )
    except:
        gdaltest.jp2openjpeg_drv = None
        return 'skip'

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

    return 'success'
	
###############################################################################
# Open byte.jp2

def jp2openjpeg_2():

    if gdaltest.jp2openjpeg_drv is None:
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

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'byte.jp2', 1, 50054 )
    return tst.testOpen( check_prj = srs, check_gt = gt )

###############################################################################
# Open int16.jp2

def jp2openjpeg_3():

    if gdaltest.jp2openjpeg_drv is None:
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

    return 'success'

###############################################################################
# Test copying byte.jp2

def jp2openjpeg_4(out_filename = 'tmp/jp2openjpeg_4.jp2'):

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.jp2')
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()

    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/jp2openjpeg_4.vrt', src_ds)
    vrt_ds.SetMetadataItem('TIFFTAG_XRESOLUTION', '300')
    vrt_ds.SetMetadataItem('TIFFTAG_YRESOLUTION', '200')
    vrt_ds.SetMetadataItem('TIFFTAG_RESOLUTIONUNIT', '3 (pixels/cm)')

    gdal.Unlink(out_filename)

    out_ds = gdal.GetDriverByName('JP2OpenJPEG').CreateCopy(out_filename, vrt_ds, options = ['REVERSIBLE=YES', 'QUALITY=100'])
    del out_ds

    vrt_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_4.vrt')

    if gdal.VSIStatL(out_filename + '.aux.xml') is not None:
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


def jp2openjpeg_4_vsimem():
    return jp2openjpeg_4('/vsimem/jp2openjpeg_4.jp2')

###############################################################################
# Test copying int16.jp2

def jp2openjpeg_5():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'int16.jp2', 1, None, options = ['REVERSIBLE=YES', 'QUALITY=100', 'CODEC=J2K'] )
    return tst.testCreateCopy()

###############################################################################
# Test reading ll.jp2

def jp2openjpeg_6():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'll.jp2', 1, None )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('data/ll.jp2')
    ds.GetRasterBand(1).Checksum()
    ds = None

    return 'success'
    
###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)

def jp2openjpeg_7():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'
    
    tst = gdaltest.GDALTest( 'JP2OpenJPEG', '/vsigzip/data/byte.jp2.gz', 1, 50054, filename_absolute = 1 )
    return tst.testOpen()
    
###############################################################################
# Test a JP2OpenJPEG with the 3 bands having 13bit depth and the 4th one 1 bit

def jp2openjpeg_8():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'
    
    # This test will cause a crash with an unpatched version of Jasper, such as the one of Ubuntu 8.04 LTS
    # --> "jpc_dec.c:1072: jpc_dec_tiledecode: Assertion `dec->numcomps == 3' failed."
    # Recent Debian/Ubuntu have the appropriate patch.
    # So we try to run in a subprocess first
    import test_cli_utilities
    if test_cli_utilities.get_gdalinfo_path() is not None:
        ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --config GDAL_SKIP "JP2ECW JP2MRSID JP2KAK" data/3_13bit_and_1bit.jp2')
        if ret.find('Band 1') == -1:
            gdaltest.post_reason('Jasper library would need patches')
            return 'fail'
    
    ds = gdal.Open('data/3_13bit_and_1bit.jp2')
    
    expected_checksums = [ 64570, 57277, 56048, 61292]
    
    for i in range(4):
        if ds.GetRasterBand(i+1).Checksum() != expected_checksums[i]:
            gdaltest.post_reason('unexpected checksum (%d) for band %d' % (expected_checksums[i], i+1))
            return 'fail'

    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('unexpected data type')
        return 'fail'
            
    return 'success'

###############################################################################
# Check that we can use .j2w world files (#4651)

def jp2openjpeg_9():

    if gdaltest.jp2openjpeg_drv is None:
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

def jp2openjpeg_10():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_10.jp2', src_ds, options = ['YCBCR420=YES', 'RESOLUTIONS=3'])
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
    gdal.Unlink('/vsimem/jp2openjpeg_10.jp2')

    # Quite a bit of difference...
    if maxdiff > 12:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit

def jp2openjpeg_11():

    if gdaltest.jp2openjpeg_drv is None:
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

    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2openjpeg_11.tif', ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    #gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/jp2openjpeg_11.tif')
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

def jp2openjpeg_12():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    # Override projection
    shutil.copy('data/byte.jp2', 'tmp/jp2openjpeg_12.jp2')

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    wkt = ds.GetProjectionRef()
    ds = None

    gdaltest.jp2openjpeg_drv.Delete('tmp/jp2openjpeg_12.jp2')

    if wkt.find('32631') < 0:
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    # Override geotransform
    shutil.copy('data/byte.jp2', 'tmp/jp2openjpeg_12.jp2')

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    ds.SetGeoTransform([1000,1,0,2000,0,-1])
    ds = None

    ds = gdal.Open('tmp/jp2openjpeg_12.jp2')
    gt = ds.GetGeoTransform()
    ds = None

    gdaltest.jp2openjpeg_drv.Delete('tmp/jp2openjpeg_12.jp2')

    if gt != (1000,1,0,2000,0,-1):
        gdaltest.post_reason('fail')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Check that PAM overrides internal GCPs (#5279)

def jp2openjpeg_13():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    # Create a dataset with GCPs
    src_ds = gdal.Open('data/rgb_gcp.vrt')
    ds = gdaltest.jp2openjpeg_drv.CreateCopy('tmp/jp2openjpeg_13.jp2', src_ds)
    ds = None
    src_ds = None

    if gdal.VSIStatL('tmp/jp2openjpeg_13.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('tmp/jp2openjpeg_13.jp2')
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
    ds = gdal.Open('tmp/jp2openjpeg_13.jp2')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    gcps = [ gdal.GCP(0,1,2,3,4) ]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/jp2openjpeg_13.jp2')
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    ds = None

    gdaltest.jp2openjpeg_drv.Delete('tmp/jp2openjpeg_13.jp2')

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

def jp2openjpeg_14():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    ds = gdal.Open('data/byte_2gcps.jp2')
    if ds.GetGCPCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test multi-threading reading

def jp2openjpeg_15():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 256,256)
    src_ds.GetRasterBand(1).Fill(255)
    data = src_ds.ReadRaster()
    gdal.SetConfigOption('GDAL_NUM_THREADS', '2')
    ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_15.jp2', src_ds, options = ['BLOCKXSIZE=32', 'BLOCKYSIZE=32'])
    gdal.SetConfigOption('GDAL_NUM_THREADS', None)
    src_ds = None
    got_data = ds.ReadRaster()
    ds = None
    gdaltest.jp2openjpeg_drv.Delete('/vsimem/jp2openjpeg_15.jp2')
    if got_data != data:
        return 'fail'

    return 'success'

###############################################################################
# Test reading PixelIsPoint file (#5437)

def jp2openjpeg_16():

    if gdaltest.jp2openjpeg_drv is None:
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

def jp2openjpeg_17():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.Open( 'data/byte_point.jp2' )
    ds = gdaltest.jp2openjpeg_drv.CreateCopy( '/vsimem/jp2openjpeg_17.jp2', src_ds)
    ds = None
    src_ds = None

    if gdal.VSIStatL('/vsimem/jp2openjpeg_17.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    
    ds = gdal.Open( '/vsimem/jp2openjpeg_17.jp2' )
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

    gdal.Unlink( '/vsimem/jp2openjpeg_17.jp2' )

    return 'success'

###############################################################################
# Test when using the decode_area API when one dimension of the dataset is not a
# multiple of 1024 (#5480)

def jp2openjpeg_18():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('Mem').Create('',2000,2000)
    ds = gdaltest.jp2openjpeg_drv.CreateCopy( '/vsimem/jp2openjpeg_18.jp2', src_ds, options = [ 'BLOCKXSIZE=2000', 'BLOCKYSIZE=2000' ])
    ds = None
    src_ds = None

    ds = gdal.Open( '/vsimem/jp2openjpeg_18.jp2' )
    ds.GetRasterBand(1).Checksum()
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink( '/vsimem/jp2openjpeg_18.jp2' )

    return 'success'

###############################################################################
# Test reading file where GMLJP2 has nul character instead of \n (#5760)

def jp2openjpeg_19():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    ds = gdal.Open('data/byte_gmljp2_with_nul_car.jp2')
    if ds.GetProjectionRef() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Validate GMLJP2 content against schema

def jp2openjpeg_20():

    if gdaltest.jp2openjpeg_drv is None:
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
    ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_20.jp2', src_ds)
    gmljp2 = ds.GetMetadata_List("xml:gml.root-instance")[0]
    ds = None
    gdal.Unlink( '/vsimem/jp2openjpeg_20.jp2' )

    if not xmlvalidate.validate(gmljp2, ogc_schemas_location = 'tmp/cache/SCHEMAS_OPENGIS_NET'):
        return 'fail'

    return 'success'
    
###############################################################################
# Test YCC=NO creation option

def jp2openjpeg_21():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_21.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES', 'YCC=NO'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_21.jp2')

    # Quite a bit of difference...
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'
    
###############################################################################
# Test RGBA support

def jp2openjpeg_22():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    # RGBA
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
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
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    # RGBA with 1BIT_ALPHA=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options = ['1BIT_ALPHA=YES'])
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.OpenEx('/vsimem/jp2openjpeg_22.jp2', open_options = ['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    if fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
    if ds.GetRasterBand(4).Checksum() != 23120:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(4).Checksum())
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    # RGBA with YCBCR420=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options = ['YCBCR420=YES'])
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
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
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    # RGBA with YCC=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options = ['YCC=YES', 'QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    # RGB,undefined
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
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
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    # RGB,undefined with ALPHA=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba_photometric_rgb.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_22.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES', 'ALPHA=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_22.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_22.jp2')
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
    gdal.Unlink('/vsimem/jp2openjpeg_22.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'


    return 'success'

###############################################################################
# Test NBITS support

def jp2openjpeg_23():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/uint16.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_23.jp2', src_ds, options = ['NBITS=9', 'QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2openjpeg_23.jp2')
    if ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '9':
        gdaltest.post_reason('failure')
        return 'fail'

    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_23_2.jp2', ds)
    if out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '9':
        gdaltest.post_reason('failure')
        return 'fail'
    del out_ds

    ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_23.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_23.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_23_2.jp2')


    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test Grey+alpha support

def jp2openjpeg_24():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    #  Grey+alpha
    src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_24.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_24.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_24.jp2')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_GrayIndex:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(2).GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_24.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    #  Grey+alpha with 1BIT_ALPHA=YES
    src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_24.jp2', src_ds, options = ['1BIT_ALPHA=YES'])
    del out_ds
    src_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_24.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.OpenEx('/vsimem/jp2openjpeg_24.jp2', open_options = ['1BIT_ALPHA_PROMOTION=NO'])
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
    ds = gdal.Open('/vsimem/jp2openjpeg_24.jp2')
    if ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is not None:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(2).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE'))
        return 'fail'
    if ds.GetRasterBand(2).Checksum() != 23120:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(2).Checksum())
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_24.jp2')

    return 'success'

###############################################################################
# Test multiband support

def jp2openjpeg_25():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 5)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(250)
    src_ds.GetRasterBand(3).Fill(245)
    src_ds.GetRasterBand(4).Fill(240)
    src_ds.GetRasterBand(5).Fill(235)

    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_25.jp2', src_ds, options = ['QUALITY=100', 'REVERSIBLE=YES'])
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open('/vsimem/jp2openjpeg_25.jp2')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_25.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/jp2openjpeg_25.jp2')

    if maxdiff > 0:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test INSPIRE_TG support

def jp2openjpeg_26():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 2048, 2048, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000,1,0,5000000,0,-1])

    # Nominal case: tiled
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES'])
    # TODO: add stricter inspire validation
    overview_count = out_ds.GetRasterBand(1).GetOverviewCount()
    # We have 2x2 1024x1024 tiles. Each of them can be reconstructed down to 128x128. 
    # So for full raster the smallest overview is 2*128
    if out_ds.GetRasterBand(1).GetOverview(overview_count-1).XSize != 2*128 or \
       out_ds.GetRasterBand(1).GetOverview(overview_count-1).YSize != 2*128:
        print(out_ds.GetRasterBand(1).GetOverview(overview_count-1).XSize)
        print(out_ds.GetRasterBand(1).GetOverview(overview_count-1).YSize)
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Nominal case: untiled
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'BLOCKXSIZE=2048', 'BLOCKYSIZE=2048'])
    # TODO: add stricter inspire validation
    overview_count = out_ds.GetRasterBand(1).GetOverviewCount()
    if out_ds.GetRasterBand(1).GetOverview(overview_count-1).XSize != 128 or \
       out_ds.GetRasterBand(1).GetOverview(overview_count-1).YSize != 128:
        print(out_ds.GetRasterBand(1).GetOverview(overview_count-1).XSize)
        print(out_ds.GetRasterBand(1).GetOverview(overview_count-1).YSize)
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Nominal case: RGBA
    src_ds = gdal.GetDriverByName('MEM').Create('', 128, 128, 4)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000,1,0,5000000,0,-1])
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'ALPHA=YES'])
    out_ds = None
    ds = gdal.OpenEx('/vsimem/jp2openjpeg_26.jp2', open_options = ['1BIT_ALPHA_PROMOTION=NO'])
    if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(4).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Warning case: disabling JPX
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.j2k', src_ds, options = ['INSPIRE_TG=YES', 'JPX=NO'])
    gdal.PopErrorHandler()
    if out_ds is not None or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Bilevel (1 bit)
    src_ds = gdal.GetDriverByName('MEM').Create('', 128, 128, 1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES'])
    if out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Auto-promotion 12->16 bits
    src_ds = gdal.GetDriverByName('MEM').Create('', 128, 128, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '12', 'IMAGE_STRUCTURE')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES'])
    if out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    if gdal.VSIStatL('/vsimem/jp2openjpeg_26.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    src_ds = gdal.GetDriverByName('MEM').Create('', 2048, 2048, 1)

    # Error case: too big tile
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'BLOCKXSIZE=1536', 'BLOCKYSIZE=1536'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: non square tile
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'BLOCKXSIZE=512', 'BLOCKYSIZE=128'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: incompatible PROFILE
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'PROFILE=UNRESTRICTED'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: valid, but too small number of resolutions regarding PROFILE_1
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'RESOLUTIONS=1'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Too big resolution number. Will fallback to default one
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'RESOLUTIONS=100'])
    gdal.PopErrorHandler()
    if out_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_26.jp2')

    # Error case: unsupported NBITS
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'NBITS=2'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: unsupported CODEC (J2K)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.j2k', src_ds, options = ['INSPIRE_TG=YES'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: invalid CODEBLOCK_WIDTH/HEIGHT
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'CODEBLOCK_WIDTH=128', 'CODEBLOCK_HEIGHT=32'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_26.jp2', src_ds, options = ['INSPIRE_TG=YES', 'CODEBLOCK_WIDTH=32', 'CODEBLOCK_HEIGHT=128'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test CreateCopy() from a JPEG2000 with a 2048x2048 tiling

def jp2openjpeg_27():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    # Test optimization in GDALCopyWholeRasterGetSwathSize()
    # Not sure how we can check that except looking at logs with CPL_DEBUG=GDAL
    # for "GDAL: GDALDatasetCopyWholeRaster(): 2048*2048 swaths, bInterleave=1"

    src_ds = gdal.GetDriverByName('MEM').Create('', 2049, 2049, 4)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_27.jp2', src_ds, options = ['RESOLUTIONS=1','BLOCKXSIZE=2048', 'BLOCKYSIZE=2048'])
    src_ds = None
    #print('End of JP2 decoding')
    out2_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/jp2openjpeg_27.tif', out_ds, options=['TILED=YES'])
    out_ds = None
    del out2_ds
    gdal.Unlink('/vsimem/jp2openjpeg_27.jp2')
    gdal.Unlink('/vsimem/jp2openjpeg_27.tif')

    return 'success'

###############################################################################
# Test CODEBLOCK_WIDTH/_HEIGHT

def jp2openjpeg_test_codeblock(ds, codeblock_width, codeblock_height):
    # TODO...
    return True

def jp2openjpeg_28():

    if gdaltest.jp2openjpeg_drv is None:
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
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_28.jp2', src_ds, options = options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('warning expected')
            print(options)
            return 'fail'
        if not jp2openjpeg_test_codeblock(out_ds, expected_cbkw, expected_cbkh):
            gdaltest.post_reason('unexpected codeblock size')
            print(options)
            return 'fail'
        del out_ds

    gdal.Unlink('/vsimem/jp2openjpeg_28.jp2')

    return 'success'

###############################################################################
# Test TILEPARTS option

def jp2openjpeg_29():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    
    tests = [ ( ['TILEPARTS=DISABLED'], False ),
              ( ['TILEPARTS=RESOLUTIONS'], False ),
              ( ['TILEPARTS=LAYERS'], True ), # warning since there's only one quality layer
              ( ['TILEPARTS=LAYERS', 'QUALITY=1,2'], False ),
              ( ['TILEPARTS=COMPONENTS'], False ),
              ( ['TILEPARTS=ILLEGAL'], True ),
            ]

    for (options, warning_expected) in tests:
        gdal.ErrorReset()
        gdal.PushErrorHandler()
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_29.jp2', src_ds, options = options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('warning expected')
            print(options)
            return 'fail'
        # Not sure if that could be easily checked
        del out_ds

    gdal.Unlink('/vsimem/jp2openjpeg_29.jp2')

    return 'success'

###############################################################################
# Test color table support

def jp2openjpeg_30():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,255,255,255) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )
    src_ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    
    tests = [ ( [], False ),
              ( ['QUALITY=100', 'REVERSIBLE=YES'], False ),
              ( ['QUALITY=50'], True ),
              ( ['REVERSIBLE=NO'], True ),
            ]

    for (options, warning_expected) in tests:
        gdal.ErrorReset()
        gdal.PushErrorHandler()
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds, options = options)
        gdal.PopErrorHandler()
        if warning_expected and gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('warning expected')
            print(options)
            return 'fail'
        ct = out_ds.GetRasterBand( 1 ).GetRasterColorTable()
        if ct.GetCount() != 4 or \
           ct.GetColorEntry(0) != (255,255,255,255) or \
           ct.GetColorEntry(1) != (255,255,0,255) or \
           ct.GetColorEntry(2) != (255,0,255,255) or \
           ct.GetColorEntry(3) != (0,255,255,255):
            gdaltest.post_reason( 'Wrong color table entry.' )
            return 'fail'
        del out_ds

    gdal.Unlink('/vsimem/jp2openjpeg_30.jp2')

    # Test with c4 != 255
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (0,0,0,0) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )
    src_ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds)
    ct = out_ds.GetRasterBand( 1 ).GetRasterColorTable()
    if ct.GetCount() != 4 or \
        ct.GetColorEntry(0) != (0,0,0,0) or \
        ct.GetColorEntry(1) != (255,255,0,255) or \
        ct.GetColorEntry(2) != (255,0,255,255) or \
        ct.GetColorEntry(3) != (0,255,255,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'
    del out_ds
    gdal.Unlink('/vsimem/jp2openjpeg_30.jp2')

    # Same but with CT_COMPONENTS=3
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (0,0,0,0) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )
    src_ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds, options = ['CT_COMPONENTS=3'])
    ct = out_ds.GetRasterBand( 1 ).GetRasterColorTable()
    if ct.GetCount() != 4 or \
        ct.GetColorEntry(0) != (0,0,0,255) or \
        ct.GetColorEntry(1) != (255,255,0,255) or \
        ct.GetColorEntry(2) != (255,0,255,255) or \
        ct.GetColorEntry(3) != (0,255,255,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'
    del out_ds
    gdal.Unlink('/vsimem/jp2openjpeg_30.jp2')
    
    # Not supported: color table on first band, and other bands
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 2)
    ct = gdal.ColorTable()
    src_ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_30.jp2', src_ds)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test unusual band color interpretation order

def jp2openjpeg_31():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 3)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_31.jp2', src_ds)
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2openjpeg_31.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_31.jp2')
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
    gdal.Unlink('/vsimem/jp2openjpeg_31.jp2')


    # With alpha now
    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 4)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_RedBand)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_31.jp2', src_ds)
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2openjpeg_31.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_31.jp2')
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
    gdal.Unlink('/vsimem/jp2openjpeg_31.jp2')

    return 'success'

###############################################################################
# Test creation of "XLBoxes" for JP2C

def jp2openjpeg_32():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_32.jp2', src_ds, options = ['JP2C_XLBOX=YES'])
    gdal.PopErrorHandler()
    if out_ds.GetRasterBand(1).Checksum() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_32.jp2')

    return 'success'

###############################################################################
# Test crazy tile size

def jp2openjpeg_33():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.Open("""<VRTDataset rasterXSize="100000" rasterYSize="100000">
  <VRTRasterBand dataType="Byte" band="1">
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_33.jp2', src_ds, options = ['BLOCKXSIZE=100000', 'BLOCKYSIZE=100000'])
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    gdal.Unlink('/vsimem/jp2openjpeg_33.jp2')

    return 'success'

###############################################################################
# Test opening a file whose dimensions are > 2^31-1

def jp2openjpeg_34():

    if gdaltest.jp2openjpeg_drv is None:
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

def jp2openjpeg_35():

    if gdaltest.jp2openjpeg_drv is None:
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

def jp2openjpeg_36():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 16385)
    gdal.PushErrorHandler()
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_36.jp2', src_ds)
    gdal.PopErrorHandler()
    if out_ds is not None or gdal.VSIStatL('/vsimem/jp2openjpeg_36.jp2') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test metadata reading & writing

def jp2openjpeg_37():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    # No metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options = ['WRITE_METADATA=YES'])
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
    if ds.GetMetadata() != {}:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Simple metadata in main domain
    for options in [ ['WRITE_METADATA=YES'], ['WRITE_METADATA=YES', 'INSPIRE_TG=YES'] ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadataItem('FOO', 'BAR')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options = options)
        del out_ds
        if gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
        if ds.GetMetadata() != {'FOO': 'BAR'}:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Simple metadata in auxiliary domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadataItem('FOO', 'BAR', 'SOME_DOMAIN')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options = ['WRITE_METADATA=YES'])
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
    if ds.GetMetadata('SOME_DOMAIN') != {'FOO': 'BAR'}:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Simple metadata in auxiliary XML domain
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetMetadata( [ '<some_arbitrary_xml_box/>' ], 'xml:SOME_DOMAIN')
    out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options = ['WRITE_METADATA=YES'])
    del out_ds
    if gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
    if ds.GetMetadata('xml:SOME_DOMAIN')[0] != '<some_arbitrary_xml_box />\n':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Special xml:BOX_ metadata domain
    for options in [ ['WRITE_METADATA=YES'], ['WRITE_METADATA=YES', 'INSPIRE_TG=YES'] ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata( [ '<some_arbitrary_xml_box/>' ], 'xml:BOX_1')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options = options)
        del out_ds
        if gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
        if ds.GetMetadata('xml:BOX_0')[0] != '<some_arbitrary_xml_box/>':
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    # Special xml:XMP metadata domain
    for options in [ ['WRITE_METADATA=YES'], ['WRITE_METADATA=YES', 'INSPIRE_TG=YES'] ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds.SetMetadata( [ '<fake_xmp_box/>' ], 'xml:XMP')
        out_ds = gdaltest.jp2openjpeg_drv.CreateCopy('/vsimem/jp2openjpeg_37.jp2', src_ds, options = options)
        del out_ds
        if gdal.VSIStatL('/vsimem/jp2openjpeg_37.jp2.aux.xml') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = gdal.Open('/vsimem/jp2openjpeg_37.jp2')
        if ds.GetMetadata('xml:XMP')[0] != '<fake_xmp_box/>':
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.Unlink('/vsimem/jp2openjpeg_37.jp2')

    return 'success'

###############################################################################
def jp2openjpeg_online_1():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k', '7sisters200.j2k'):
        return 'skip'

    # Checksum = 32669 on my PC
    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/7sisters200.j2k', 1, None, filename_absolute = 1 )

    if tst.testOpen() != 'success':
        return 'fail'

    ds = gdal.Open('tmp/cache/7sisters200.j2k')
    ds.GetRasterBand(1).Checksum()
    ds = None

    return 'success'

###############################################################################
def jp2openjpeg_online_2():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2', 'gcp.jp2'):
        return 'skip'

    # Checksum = 15621 on my PC
    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/gcp.jp2', 1, None, filename_absolute = 1 )

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
def jp2openjpeg_online_3():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.j2k', 'Bretagne1.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne1.bmp', 'Bretagne1.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/Bretagne1.j2k', 1, None, filename_absolute = 1 )

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
def jp2openjpeg_online_4():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.j2k', 'Bretagne2.j2k'):
        return 'skip'
    if not gdaltest.download_file('http://www.openjpeg.org/samples/Bretagne2.bmp', 'Bretagne2.bmp'):
        return 'skip'

    tst = gdaltest.GDALTest( 'JP2OpenJPEG', 'tmp/cache/Bretagne2.j2k', 1, None, filename_absolute = 1 )

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
# Try reading JP2OpenJPEG with color table

def jp2openjpeg_online_5():

    if gdaltest.jp2openjpeg_drv is None:
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
# Try reading YCbCr JP2OpenJPEG as RGB

def jp2openjpeg_online_6():

    if gdaltest.jp2openjpeg_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_03/file3.jp2', 'file3.jp2'):
        return 'skip'

    ds = gdal.Open('tmp/cache/file3.jp2')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    if cs1 != 26140 or cs2 != 32689 or cs3 != 48247:
        print(cs1, cs2, cs3)
        gdaltest.post_reason('Did not get expected checksums')
        return 'fail'
        
    ds = None

    return 'success'

###############################################################################
def jp2openjpeg_cleanup():

    gdaltest.reregister_all_jpeg2000_drivers()

    return 'success'

gdaltest_list = [
    jp2openjpeg_1,
    jp2openjpeg_2,
    jp2openjpeg_3,
    jp2openjpeg_4,
    jp2openjpeg_4_vsimem,
    jp2openjpeg_5,
    jp2openjpeg_6,
    jp2openjpeg_7,
    jp2openjpeg_8,
    jp2openjpeg_9,
    jp2openjpeg_10,
    jp2openjpeg_11,
    jp2openjpeg_12,
    jp2openjpeg_13,
    jp2openjpeg_14,
    jp2openjpeg_15,
    jp2openjpeg_16,
    jp2openjpeg_17,
    jp2openjpeg_18,
    jp2openjpeg_19,
    jp2openjpeg_20,
    jp2openjpeg_21,
    jp2openjpeg_22,
    jp2openjpeg_23,
    jp2openjpeg_24,
    jp2openjpeg_25,
    jp2openjpeg_26,
    jp2openjpeg_27,
    jp2openjpeg_28,
    jp2openjpeg_29,
    jp2openjpeg_30,
    jp2openjpeg_31,
    jp2openjpeg_32,
    jp2openjpeg_33,
    jp2openjpeg_34,
    jp2openjpeg_35,
    jp2openjpeg_36,
    jp2openjpeg_37,
    jp2openjpeg_online_1,
    jp2openjpeg_online_2,
    jp2openjpeg_online_3,
    jp2openjpeg_online_4,
    jp2openjpeg_online_5,
    jp2openjpeg_online_6,
    jp2openjpeg_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'jp2openjpeg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

