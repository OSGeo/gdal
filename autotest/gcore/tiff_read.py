#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a TIFF file.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import sys
import shutil

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal, osr

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

init_list = [
    ('byte.tif', 1, 4672, None),
    ('uint16_sgilog.tif', 1, 4672, None),
    ('next_literalrow.tif', 1, 4, None),
    ('next_literalspan.tif', 1, 4, None),
    ('next_default_case.tif', 1, 4, None),
    ('thunder.tif', 1, 3, None),
    ('int10.tif', 1, 4672, None),
    ('int12.tif', 1, 4672, None),
    ('int16.tif', 1, 4672, None),
    ('uint16.tif', 1, 4672, None),
    ('int24.tif', 1, 4672, None),
    ('int32.tif', 1, 4672, None),
    ('uint32.tif', 1, 4672, None),
    ('float16.tif', 1, 4672, None),
    ('float24.tif', 1, 4672, None),
    ('float32.tif', 1, 4672, None),
    ('float32_minwhite.tif', 1, 1, None),
    ('float64.tif', 1, 4672, None),
    ('cint16.tif', 1, 5028, None),
    ('cint32.tif', 1, 5028, None),
    ('cfloat32.tif', 1, 5028, None),
    ('cfloat64.tif', 1, 5028, None),
# The following four related partial final strip/tiles (#1179)
    ('separate_tiled.tif', 2, 15234, None),
    ('seperate_strip.tif', 2, 15234, None),  # TODO: Spelling.
    ('contig_tiled.tif', 2, 15234, None),
    ('contig_strip.tif', 2, 15234, None),
    ('empty1bit.tif', 1, 0, None)]

###############################################################################
# Test absolute/offset && index directory access

def tiff_read_off():

    # Test absolute/offset directory access.
    ds = gdal.Open('GTIFF_DIR:off:408:data/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'

    # Test index directory access
    ds = gdal.Open('GTIFF_DIR:1:data/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'

    # Check that georeferencing is read properly when accessing
    # "GTIFF_DIR" subdatasets (#3478)
    gt = ds.GetGeoTransform()
    if gt != (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'


###############################################################################
# Confirm we interpret bands as alpha when we should, and not when we
# should not.

def tiff_check_alpha():

    # Grey + alpha

    ds = gdal.Open('data/stefan_full_greyalpha.tif')

    if ds.GetRasterBand(2).GetRasterColorInterpretation()!= gdal.GCI_AlphaBand:
        gdaltest.post_reason( 'Wrong color interpretation (stefan_full_greyalpha).')
        print(ds.GetRasterBand(2).GetRasterColorInterpretation())
        return 'fail'

    ds = None

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/stefan_full_greyalpha.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    gdaltest.supports_force_rgba = False
    if ds.RasterCount == 4:
        gdaltest.supports_force_rgba = True
    if gdaltest.supports_force_rgba:
        got_cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount) ]
        if got_cs != [1970,1970,1970,10807]:
            gdaltest.post_reason( 'fail')
            print(got_cs)
            return 'fail'
        ds = None

    # RGB + alpha

    ds = gdal.Open('data/stefan_full_rgba.tif')

    if ds.GetRasterBand(4).GetRasterColorInterpretation()!= gdal.GCI_AlphaBand:
        gdaltest.post_reason( 'Wrong color interpretation (stefan_full_rgba).')
        print(ds.GetRasterBand(4).GetRasterColorInterpretation())
        return 'fail'

    ds = None

    if gdaltest.supports_force_rgba:
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
        ds = gdal.Open('data/stefan_full_rgba.tif')
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
        got_cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount) ]
        # FIXME? Not the same as without GTIFF_FORCE_RGBA=YES
        if got_cs != [11547, 57792, 35643, 10807]:
            gdaltest.post_reason( 'fail')
            print(got_cs)
            return 'fail'
        ds = None

    # RGB + undefined

    ds = gdal.Open('data/stefan_full_rgba_photometric_rgb.tif')

    if ds.GetRasterBand(4).GetRasterColorInterpretation()!= gdal.GCI_Undefined:
        gdaltest.post_reason( 'Wrong color interpretation (stefan_full_rgba_photometric_rgb).')
        print(ds.GetRasterBand(4).GetRasterColorInterpretation())
        return 'fail'

    ds = None

    if gdaltest.supports_force_rgba:
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
        ds = gdal.Open('data/stefan_full_rgba_photometric_rgb.tif')
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
        got_cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount) ]
        if got_cs != [12603, 58561, 36064, 10807]:
            gdaltest.post_reason( 'fail')
            print(got_cs)
            return 'fail'
        ds = None

    return 'success'


###############################################################################
# Test reading a CMYK tiff as RGBA image

def tiff_read_cmyk_rgba():

    ds = gdal.Open('data/rgbsmall_cmyk.tif')

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if 'SOURCE_COLOR_SPACE' not in md or md['SOURCE_COLOR_SPACE'] != 'CMYK':
        print('bad value for IMAGE_STRUCTURE[SOURCE_COLOR_SPACE]')
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_RedBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(4).GetRasterColorInterpretation()!= gdal.GCI_AlphaBand:
        gdaltest.post_reason( 'Wrong color interpretation (alpha).')
        print(ds.GetRasterBand(4).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 23303:
        print('Expected checksum = %d. Got = %d' % (23303, ds.GetRasterBand(1).Checksum()))
        return 'fail'

    return 'success'

###############################################################################
# Test reading a CMYK tiff as a raw image

def tiff_read_cmyk_raw():

    ds = gdal.Open('GTIFF_RAW:data/rgbsmall_cmyk.tif')

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 29430:
        print('Expected checksum = %d. Got = %d' % (29430, ds.GetRasterBand(1).Checksum()))
        return 'fail'

    return 'success'

###############################################################################
# Test reading a OJPEG image

def tiff_read_ojpeg():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/zackthecat.tif')
    gdal.PopErrorHandler()
    if ds is None:
        if gdal.GetLastErrorMsg().find('Cannot open TIFF file due to missing codec') == 0:
            return 'skip'
        else:
            print(gdal.GetLastErrorMsg())
            return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    got_cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    expected_cs = 61570
    if got_cs != expected_cs:
        print('Expected checksum = %d. Got = %d' % (expected_cs, got_cs))
        return 'fail'

    #
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/zackthecat_corrupted.tif')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if cs != 0:
        print('Should be 0 with internal libtiff')

    return 'success'

###############################################################################
# Read a .tif.gz file

def tiff_read_gzip():

    try:
        os.remove('data/byte.tif.gz.properties')
    except:
        pass

    ds = gdal.Open('/vsigzip/./data/byte.tif.gz')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    try:
        os.stat('data/byte.tif.gz.properties')
        gdaltest.post_reason('did not expect data/byte.tif.gz.properties')
        return 'fail'
    except:
        return 'success'

###############################################################################
# Read a .tif.zip file (with explicit filename)

def tiff_read_zip_1():

    ds = gdal.Open('/vsizip/./data/byte.tif.zip/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tif.zip file (with implicit filename)

def tiff_read_zip_2():

    ds = gdal.Open('/vsizip/./data/byte.tif.zip')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tif.zip file with a single file in a subdirectory (with explicit filename)

def tiff_read_zip_3():

    ds = gdal.Open('/vsizip/./data/onefileinsubdir.zip/onefileinsubdir/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tif.zip file with a single file in a subdirectory(with implicit filename)

def tiff_read_zip_4():

    ds = gdal.Open('/vsizip/./data/onefileinsubdir.zip')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tif.zip file with 2 files in a subdirectory

def tiff_read_zip_5():

    ds = gdal.Open('/vsizip/./data/twofileinsubdir.zip/twofileinsubdir/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tar file (with explicit filename)

def tiff_read_tar_1():

    ds = gdal.Open('/vsitar/./data/byte.tar/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tar file (with implicit filename)

def tiff_read_tar_2():

    ds = gdal.Open('/vsitar/./data/byte.tar')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tgz file (with explicit filename)

def tiff_read_tgz_1():

    ds = gdal.Open('/vsitar/./data/byte.tgz/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tgz file (with implicit filename)

def tiff_read_tgz_2():

    ds = gdal.Open('/vsitar/./data/byte.tgz')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Check handling of non-degree angular units (#601)

def tiff_grads():

    ds = gdal.Open('data/test_gf.tif')
    srs = ds.GetProjectionRef()

    if srs.find('PARAMETER["latitude_of_origin",46.8]') == -1:
        print(srs)
        gdaltest.post_reason( 'Did not get expected latitude of origin.' )
        return 'fail'

    return 'success'

###############################################################################
# Check Erdas Citation Parsing for coordinate system.

def tiff_citation():

    build_info = gdal.VersionInfo('BUILD_INFO')
    if build_info.find('ESRI_BUILD=YES') == -1:
        return 'skip'

    ds = gdal.Open('data/citation_mixedcase.tif')
    wkt = ds.GetProjectionRef()

    expected_wkt = """PROJCS["NAD_1983_HARN_StatePlane_Oregon_North_FIPS_3601_Feet_Intl",GEOGCS["GCS_North_American_1983_HARN",DATUM["NAD83_High_Accuracy_Reference_Network",SPHEROID["GRS_1980",6378137.0,298.257222101]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["False_Easting",8202099.737532808],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-120.5],PARAMETER["Standard_Parallel_1",44.33333333333334],PARAMETER["Standard_Parallel_2",46.0],PARAMETER["Latitude_Of_Origin",43.66666666666666],UNIT["Foot",0.3048]]"""

    if wkt != expected_wkt:
        print('got: ', wkt)
        gdaltest.post_reason( 'Erdas citation processing failing?' )
        return 'fail'

    return 'success'

###############################################################################
# Check that we can read linear projection parameters properly (#3901)

def tiff_linearparmunits():

    # Test the file with the correct formulation.

    ds = gdal.Open('data/spaf27_correct.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference( wkt )

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    if abs(fe-2000000.0) > 0.001:
        gdaltest.post_reason( 'did not get expected false easting (1)' )
        return 'fail'

    # Test the file with the old (broken) GDAL formulation.

    ds = gdal.Open('data/spaf27_brokengdal.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference( wkt )

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    if abs(fe-609601.219202438) > 0.001:
        gdaltest.post_reason( 'did not get expected false easting (2)' )
        return 'fail'

    # Test the file when using an EPSG code.

    ds = gdal.Open('data/spaf27_epsg.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference( wkt )

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    if abs(fe-2000000.0) > 0.001:
        gdaltest.post_reason( 'did not get expected false easting (3)' )
        return 'fail'

    return 'success'

###############################################################################
# Check that the GTIFF_LINEAR_UNITS handling works properly (#3901)

def tiff_linearparmunits2():

    gdal.SetConfigOption( 'GTIFF_LINEAR_UNITS', 'BROKEN' )

    # Test the file with the correct formulation.

    ds = gdal.Open('data/spaf27_correct.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference( wkt )

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    if abs(fe-6561666.66667) > 0.001:
        gdaltest.post_reason( 'did not get expected false easting (1)' )
        return 'fail'

    # Test the file with the correct formulation that is marked as correct.

    ds = gdal.Open('data/spaf27_markedcorrect.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference( wkt )

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    if abs(fe-2000000.0) > 0.001:
        gdaltest.post_reason( 'did not get expected false easting (2)' )
        return 'fail'

    # Test the file with the old (broken) GDAL formulation.

    ds = gdal.Open('data/spaf27_brokengdal.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference( wkt )

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    if abs(fe-2000000.0) > 0.001:
        gdaltest.post_reason( 'did not get expected false easting (3)' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_LINEAR_UNITS', 'DEFAULT' )

    return 'success'

###############################################################################
# Test GTiffSplitBitmapBand to treat one row 1bit files as scanline blocks (#2622)

def tiff_g4_split():

    if not 'GetBlockSize' in dir(gdal.Band):
        return 'skip'

    ds = gdal.Open('data/slim_g4.tif')

    (blockx, blocky) = ds.GetRasterBand(1).GetBlockSize()

    if blocky != 1:
        gdaltest.post_reason( 'Did not get scanline sized blocks.' )
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 3322:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum' )
        return 'fail'

    return 'success'

###############################################################################
# Test reading a tiff with multiple images in it

def tiff_multi_images():

    # Implicitly get the content of the first image (backward compatibility).
    ds = gdal.Open('data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' %
                  (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'

    md = ds.GetMetadata('SUBDATASETS')
    if md['SUBDATASET_1_NAME'] != 'GTIFF_DIR:1:data/twoimages.tif':
        print(md)
        gdaltest.post_reason( 'did not get expected subdatasets metadata.' )
        return 'fail'

    ds = None

    # Explicitly get the content of the first image.
    ds = gdal.Open('GTIFF_DIR:1:data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    # Explicitly get the content of the second image.
    ds = gdal.Open('GTIFF_DIR:2:data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test reading a tiff from a memory buffer (#2931)

def tiff_vsimem():

    try:
        gdal.FileFromMemBuffer
    except:
        return 'skip'

    content = open('data/byte.tif', mode='rb').read()

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/tiffinmem', content)

    ds = gdal.Open('/vsimem/tiffinmem', gdal.GA_Update)
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open('/vsimem/tiffinmem')
    if ds.GetRasterBand(1).Checksum() != 0:
            print('Expected checksum = %d. Got = %d' % (0, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    # Also test with anti-slash
    ds = gdal.Open('/vsimem\\tiffinmem')
    if ds.GetRasterBand(1).Checksum() != 0:
            print('Expected checksum = %d. Got = %d' % (0, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/tiffinmem')

    return 'success'

###############################################################################
# Test reading a tiff from inside a zip in a memory buffer !

def tiff_vsizip_and_mem():

    try:
        gdal.FileFromMemBuffer
    except:
        return 'skip'

    content = open('data/byte.tif.zip', mode='rb').read()

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/tiffinmem.zip', content)

    ds = gdal.Open('/vsizip/vsimem/tiffinmem.zip/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/tiffinmem.zip')

    return 'success'

###############################################################################
# Test reading a GeoTIFF with only ProjectedCSTypeGeoKey defined (ticket #3019)

def tiff_ProjectedCSTypeGeoKey_only():

    ds = gdal.Open('data/ticket3019.tif')
    if ds.GetProjectionRef().find('WGS 84 / UTM zone 31N') == -1:
        print(ds.GetProjectionRef())
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test reading a GeoTIFF with only GTModelTypeGeoKey defined

def tiff_GTModelTypeGeoKey_only():

    ds = gdal.Open('data/GTModelTypeGeoKey_only.tif')
    if ds.GetProjectionRef().find('LOCAL_CS["unnamed",GEOGCS["unknown",DATUM["unknown",SPHEROID["unretrievable - using WGS84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT[,0.0174532925199433]],UNIT["unknown",1]]') != 0:
        print(ds.GetProjectionRef())
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test reading a 12bit jpeg compressed geotiff.

def tiff_12bitjpeg():

    old_accum = gdal.GetConfigOption( 'CPL_ACCUM_ERROR_MSG', 'OFF' )
    gdal.SetConfigOption( 'CPL_ACCUM_ERROR_MSG', 'ON' )
    gdal.ErrorReset()
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )

    try:
        os.unlink('data/mandrilmini_12bitjpeg.tif.aux.xml')
    except:
        pass

    try:
        ds = gdal.Open('data/mandrilmini_12bitjpeg.tif')
        ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    except:
        ds = None

    gdal.PopErrorHandler()
    gdal.SetConfigOption( 'CPL_ACCUM_ERROR_MSG', old_accum )

    if gdal.GetLastErrorMsg().find(
                   'Unsupported JPEG data precision 12') != -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'
    elif ds is None:
        gdaltest.post_reason( 'failed to open 12bit jpeg file with unexpected error' )
        return 'fail'

    try:
        stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    except:
        pass

    if stats[2] < 2150 or stats[2] > 2180 or str(stats[2]) == 'nan':
        gdaltest.post_reason( 'did not get expected mean for band1.')
        print(stats)
        return 'fail'
    ds = None

    os.unlink('data/mandrilmini_12bitjpeg.tif.aux.xml')

    return 'success'

###############################################################################
# Test that statistics for TIFF files are stored and correctly read from .aux.xml

def tiff_read_stats_from_pam():

    try:
        os.remove('data/byte.tif.aux.xml')
    except:
        pass

    ds = gdal.Open('data/byte.tif')
    md = ds.GetRasterBand(1).GetMetadata()
    if 'STATISTICS_MINIMUM' in md:
        gdaltest.post_reason('Unexpected presence of STATISTICS_MINIMUM')
        return 'fail'

    # Force statistics computation
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    if stats[0] != 74.0 or stats[1] != 255.0:
        print(stats)
        return 'fail'

    ds = None
    try:
        os.stat('data/byte.tif.aux.xml')
    except:
        gdaltest.post_reason('Expected generation of data/byte.tif.aux.xml')
        return 'fail'

    ds = gdal.Open('data/byte.tif')
    # Just read statistics (from PAM) without forcing their computation
    stats = ds.GetRasterBand(1).GetStatistics(0, 0)
    if stats[0] != 74.0 or stats[1] != 255.0:
        print(stats)
        return 'fail'
    ds = None

    try:
        os.remove('data/byte.tif.aux.xml')
    except:
        pass

    return 'success'

###############################################################################
# Test extracting georeferencing from a .TAB file

def tiff_read_from_tab():

    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_read_from_tab.tif', 1, 1)
    ds = None

    f = open('tmp/tiff_read_from_tab.tab', 'wt')
    f.write("""!table
!version 300
!charset WindowsLatin1

Definition Table
  File "HP.TIF"
  Type "RASTER"
  (400000,1200000) (0,4000) Label "Pt 1",
  (500000,1200000) (4000,4000) Label "Pt 2",
  (500000,1300000) (4000,0) Label "Pt 3",
  (400000,1300000) (0,0) Label "Pt 4"
  CoordSys Earth Projection 8, 79, "m", -2, 49, 0.9996012717, 400000, -100000
  Units "m"
""")
    f.close()

    ds = gdal.Open('tmp/tiff_read_from_tab.tif')
    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()
    ds = None

    gdal.GetDriverByName('GTiff').Delete('tmp/tiff_read_from_tab.tif')

    try:
        os.stat('tmp/tiff_read_from_tab.tab')
        gdaltest.post_reason('did not expect to find .tab file at that point')
        return 'fail'
    except:
        pass

    if gt != (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return 'fail'

    if wkt.find('OSGB_1936') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading PixelIsPoint file.

def tiff_read_pixelispoint():

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

    ds = gdal.Open( 'data/byte_point.tif' )
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'TRUE' )

    ds = gdal.Open( 'data/byte_point.tif' )
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform with GTIFF_POINT_GEO_IGNORE TRUE' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', None )

    return 'success'

###############################################################################
# Test reading a GeoTIFF file with a geomatrix in PixelIsPoint format.

def tiff_read_geomatrix():

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

    ds = gdal.Open( 'data/geomatrix.tif' )
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (1841001.75, 1.5, -5.0, 1144003.25, -5.0, -1.5)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'TRUE' )

    ds = gdal.Open( 'data/geomatrix.tif' )
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (1841000.0, 1.5, -5.0, 1144000.0, -5.0, -1.5)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform with GTIFF_POINT_GEO_IGNORE TRUE' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', None )

    return 'success'

###############################################################################
# Test that we don't crash when reading a TIFF with corrupted GeoTIFF tags

def tiff_read_corrupted_gtiff():

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('data/corrupted_gtiff_tags.tif')
    gdal.PopErrorHandler()
    del ds

    err_msg = gdal.GetLastErrorMsg()
    if err_msg.find('IO error during') == -1 and \
       err_msg.find('Error fetching data for field') == -1:
        gdaltest.post_reason( 'did not get expected error message' )
        print(err_msg)
        return 'fail'

    return 'success'

###############################################################################
# Test that we don't crash when reading a TIFF with corrupted GeoTIFF tags

def tiff_read_tag_without_null_byte():

    gdal.ErrorReset()
    oldval = gdal.GetConfigOption('CPL_DEBUG')
    gdal.SetConfigOption('CPL_DEBUG', 'OFF')
    ds = gdal.Open('data/tag_without_null_byte.tif')
    gdal.SetConfigOption('CPL_DEBUG', oldval)
    if gdal.GetLastErrorType() != 0:
        gdaltest.post_reason( 'should have not emitted a warning, but only a CPLDebug() message' )
        return 'fail'
    del ds

    return 'success'


###############################################################################
# Test the effect of the GTIFF_IGNORE_READ_ERRORS configuration option (#3994)

def tiff_read_buggy_packbits():

    old_val = gdal.GetConfigOption('GTIFF_IGNORE_READ_ERRORS')
    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', None)
    ds = gdal.Open('data/byte_buggy_packbits.tif')
    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', old_val)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ReadRaster(0,0,20,20)
    gdal.PopErrorHandler()
    if ret is not None:
        gdaltest.post_reason('did not expected a valid result')
        return 'fail'
    ds = None

    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', 'YES')
    ds = gdal.Open('data/byte_buggy_packbits.tif')
    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', old_val)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ReadRaster(0,0,20,20)
    gdal.PopErrorHandler()
    if ret is None:
        gdaltest.post_reason('expected a valid result')
        return 'fail'
    ds = None

    return 'success'


###############################################################################
# Test reading a GeoEye _rpc.txt (#3639)

def tiff_read_rpc_txt():

    shutil.copy('data/byte.tif', 'tmp/test.tif')
    shutil.copy('data/test_rpc.txt', 'tmp/test_rpc.txt')
    ds = gdal.Open('tmp/test.tif')
    rpc_md = ds.GetMetadata('RPC')
    ds = None
    os.remove('tmp/test.tif')
    os.remove('tmp/test_rpc.txt')

    if rpc_md['HEIGHT_OFF'] != '+0300.000 meters':
        gdaltest.post_reason('HEIGHT_OFF wrong:"'+rpc_md['HEIGHT_OFF']+'"')
        return 'fail'

    if rpc_md['LINE_DEN_COEFF'].find(
        '+1.000000000000000E+00 -5.207696939454288E-03') != 0:
        print(rpc_md['LINE_DEN_COEFF'])
        gdaltest.post_reason('LINE_DEN_COEFF wrong')
        return 'fail'

    return 'success'

###############################################################################
# Test reading a TIFF with the RPC tag per
#  http://geotiff.maptools.org/rpc_prop.html

def tiff_read_rpc_tif():

    ds = gdal.Open('data/byte_rpc.tif')
    rpc_md = ds.GetMetadata('RPC')
    ds = None

    if rpc_md['HEIGHT_OFF'] != '300':
        gdaltest.post_reason('HEIGHT_OFF wrong:'+rpc_md['HEIGHT_OFF'])
        return 'fail'

    if rpc_md['LINE_DEN_COEFF'].find('1 -0.00520769693945429') != 0:
        print(rpc_md['LINE_DEN_COEFF'])
        gdaltest.post_reason('LINE_DEN_COEFF wrong')
        return 'fail'

    return 'success'

###############################################################################
# Test a very small TIFF with only 4 tags :
# Magic: 0x4949 <little-endian> Version: 0x2a
# Directory 0: offset 8 (0x8) next 0 (0)
# ImageWidth (256) SHORT (3) 1<1>
# ImageLength (257) SHORT (3) 1<1>
# StripOffsets (273) LONG (4) 1<0>
# StripByteCounts (279) LONG (4) 1<1>

def tiff_small():

    content = '\x49\x49\x2A\x00\x08\x00\x00\x00\x04\x00\x00\x01\x03\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x01\x03\x00\x01\x00\x00\x00\x01\x00\x00\x00\x11\x01\x04\x00\x01\x00\x00\x00\x00\x00\x00\x00\x17\x01\x04\x00\x01\x00\x00\x00\x01\x00\x00\x00'

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/small.tif', content)

    ds = gdal.Open('/vsimem/small.tif')
    if ds.GetRasterBand(1).Checksum() != 0:
            print('Expected checksum = %d. Got = %d' % (0, ds.GetRasterBand(1).Checksum()))
            return 'fail'

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/small.tif')

    return 'success'

###############################################################################
# Test that we can workaround a DoS with

def tiff_dos_strip_chop():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/tiff_dos_strip_chop.tif')
    gdal.PopErrorHandler()
    del ds

    return 'success'

###############################################################################
# Test reading EXIF and GPS metadata

def tiff_read_exif_and_gps():

    ds = gdal.Open('data/exif_and_gps.tif')
    exif_md = ds.GetMetadata('EXIF')
    ds = None

    if exif_md is None or len(exif_md) == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    ds = gdal.Open('data/exif_and_gps.tif')
    EXIF_GPSVersionID = ds.GetMetadataItem('EXIF_GPSVersionID', 'EXIF')
    ds = None

    if EXIF_GPSVersionID is None:
        gdaltest.post_reason('failed')
        return 'fail'

    # We should not get any EXIF metadata with that file
    ds = gdal.Open('data/byte.tif')
    exif_md = ds.GetMetadata('EXIF')
    ds = None

    if not (exif_md is None or len(exif_md) == 0):
        gdaltest.post_reason('failed')
        return 'fail'

    return 'success'

###############################################################################
# Test reading a pixel interleaved RGBA JPEG-compressed TIFF

def tiff_jpeg_rgba_pixel_interleaved():
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    ds = gdal.Open('data/stefan_full_rgba_jpeg_contig.tif')
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['INTERLEAVE'] != 'PIXEL':
        gdaltest.post_reason('failed')
        return 'fail'

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('failed')
            return 'fail'

        if ds.GetRasterBand(i+1).GetRasterColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('failed')
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test reading a band interleaved RGBA JPEG-compressed TIFF

def tiff_jpeg_rgba_band_interleaved():
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    ds = gdal.Open('data/stefan_full_rgba_jpeg_separate.tif')
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['INTERLEAVE'] != 'BAND':
        gdaltest.post_reason('failed')
        return 'fail'

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('failed')
            return 'fail'

        if ds.GetRasterBand(i+1).GetRasterColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('failed')
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test reading a YCbCr JPEG all-in-one-strip multiband TIFF (#3259, #3894)

def tiff_read_online_1():
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    if not gdaltest.download_file('http://trac.osgeo.org/gdal/raw-attachment/ticket/3259/imgpb17.tif', 'imgpb17.tif'):
        return 'skip'

    ds = gdal.Open('tmp/cache/imgpb17.tif')
    gdal.ErrorReset()
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if gdal.GetLastErrorMsg() != '':
        return 'fail'

    if cs != 62628 and cs != 28554:
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Use GTIFF_DIRECT_IO=YES option combined with /vsicurl to test for multi-range
# support

def tiff_read_online_2():

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    if gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/gtiff/utm.tif') is None:
        print('cannot open URL')
        return 'skip'

    old_val = gdal.GetConfigOption('GTIFF_DIRECT_IO')
    gdal.SetConfigOption('GTIFF_DIRECT_IO', 'YES')
    gdal.SetConfigOption('CPL_VSIL_CURL_ALLOWED_EXTENSIONS', '.tif')
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'EMPTY_DIR')
    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/gtiff/utm.tif')
    gdal.SetConfigOption('GTIFF_DIRECT_IO', old_val)
    gdal.SetConfigOption('CPL_VSIL_CURL_ALLOWED_EXTENSIONS', None)
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', None)

    if ds is None:
        gdaltest.post_reason('could not open dataset')
        return 'fail'

    # Read subsampled data
    subsampled_data = ds.ReadRaster(0, 0, 512, 512, 128, 128)
    ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 128,128)
    ds.WriteRaster(0, 0, 128, 128, subsampled_data)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != 54935:
        gdaltest.post_reason('wrong checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test reading a TIFF made of a single-strip that is more than 2GB (#5403)

def tiff_read_huge4GB():

    # Need libtiff 4.X anyway
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if (gdaltest.filesystem_supports_sparse_files('tmp') == False):
        ds = gdal.Open('data/huge4GB.tif')
        if ds is None:
            return 'fail'
    else:
        shutil.copy('data/huge4GB.tif', 'tmp/huge4GB.tif')
        f = open('tmp/huge4GB.tif', 'rb+')
        f.seek(65535 * 65535 + 401)
        f.write(' '.encode('ascii'))
        f.close()
        ds = gdal.Open('tmp/huge4GB.tif')
        if ds is None:
            os.remove('tmp/huge4GB.tif')
            return 'fail'
        ds = None
        os.remove('tmp/huge4GB.tif')

    return 'success'

###############################################################################
# Test reading a (small) BigTIFF. Tests GTiffCacheOffsetOrCount8()

def tiff_read_bigtiff():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdal.Open('data/byte_bigtiff_strip5lines.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != 4672:
        return 'fail'

    return 'success'

###############################################################################
# Test reading in TIFF metadata domain

def tiff_read_tiff_metadata():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    ds = gdal.Open('data/stefan_full_rgba_jpeg_contig.tif')
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') != '254':
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_0', 'TIFF') != '770':
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('JPEGTABLES', 'TIFF').find('FFD8') != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_100_0', 'TIFF') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_100', 'TIFF') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_100_0', 'TIFF') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_100', 'TIFF') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('data/stefan_full_rgba_jpeg_separate.tif')
    if ds.GetRasterBand(4).GetMetadataItem('BLOCK_OFFSET_0_2', 'TIFF') != '11071':
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(4).GetMetadataItem('BLOCK_SIZE_0_2', 'TIFF') != '188':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test reading a JPEG-in-TIFF with tiles of irregular size (corrupted image)

def tiff_read_irregular_tile_size_jpeg_in_tiff():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    ds = gdal.Open('data/irregular_tile_size_jpeg_in_tiff.tif')
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetRasterBand(1).GetOverview(0).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()

    return 'success'

###############################################################################
# Test GTIFF_DIRECT_IO and GTIFF_VIRTUAL_MEM_IO optimizations

def tiff_direct_and_virtual_mem_io():

  # Test with pixel-interleaved and band-interleaved datasets
  for dt in [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_CInt16 ]:

    src_ds = gdal.Open('data/stefan_full_rgba.tif')
    dt_size = 1
    if dt == gdal.GDT_Int16:
        dt_size = 2
        mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, dt)
        data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, buf_type = dt)
        new_vals = []
        for i in range(4*src_ds.RasterXSize*src_ds.RasterYSize):
            if sys.version_info >= (3,0,0):
                new_vals.append(chr(data[2*i]).encode('latin1'))
                new_vals.append(chr(255 - data[2*i]).encode('latin1'))
            else:
                new_vals.append(data[2*i])
                new_vals.append(chr(255 - ord(data[2*i])))
        if sys.version_info >= (3,0,0):
            data = ''.encode('latin1').join(new_vals)
        else:
            data = ''.join(new_vals)
        mem_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data, buf_type = dt)
        src_ds = mem_ds
    elif dt == gdal.GDT_CInt16:
        dt_size = 4
        mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, dt)
        data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, buf_type = dt)
        new_vals = []
        for i in range(4*src_ds.RasterXSize*src_ds.RasterYSize):
            if sys.version_info >= (3,0,0):
                new_vals.append(chr(data[4*i]).encode('latin1'))
                new_vals.append(chr(data[4*i]).encode('latin1'))
                new_vals.append(chr(255 - data[4*i]).encode('latin1'))
                new_vals.append(chr(255 - data[4*i]).encode('latin1'))
            else:
                new_vals.append(data[4*i])
                new_vals.append(data[4*i])
                new_vals.append(chr(255 - ord(data[4*i])))
                new_vals.append(chr(255 - ord(data[4*i])))
        if sys.version_info >= (3,0,0):
            data = ''.encode('latin1').join(new_vals)
        else:
            data = ''.join(new_vals)
        mem_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data, buf_type = dt)
        src_ds = mem_ds

    for truncated in [False, True]:
     if truncated:
         nitermax = 4
         options = [('GTIFF_DIRECT_IO', '/vsimem'), ('GTIFF_VIRTUAL_MEM_IO', '/vsimem')]
     else:
         nitermax = 8
         options = [('GTIFF_DIRECT_IO', '/vsimem'), ('GTIFF_VIRTUAL_MEM_IO', '/vsimem'), ('GTIFF_VIRTUAL_MEM_IO', 'tmp')]
     for (option, prefix) in options:
      if dt == gdal.GDT_CInt16:
          niter = 3
      elif prefix == 'tmp':
          niter = 4
      else:
          niter = nitermax
      for i in range(niter):

        if i == 0:
            filename = '%s/tiff_direct_io_contig.tif' % prefix
            creation_options = []
            if (dt == gdal.GDT_CInt16 or dt == gdal.GDT_Int16):
                creation_options += [ 'ENDIANNESS=INVERTED' ]
            out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options = creation_options)
            out_ds.FlushCache()
            out_ds = None
        elif i == 1:
            filename = '%s/tiff_direct_io_separate.tif' % prefix
            out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options = ['INTERLEAVE=BAND'])
            out_ds.FlushCache()
            out_ds = None
        elif i == 2:
            filename = '%s/tiff_direct_io_tiled_contig.tif' % prefix
            creation_options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16']
            if (dt == gdal.GDT_CInt16 or dt == gdal.GDT_Int16):
                creation_options += [ 'ENDIANNESS=INVERTED' ]
            if option == 'GTIFF_VIRTUAL_MEM_IO' and prefix == '/vsimem':
                gdal.Translate(filename, src_ds, bandList = [1, 2, 3], creationOptions= creation_options)
            else:
                out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options = creation_options)
                out_ds.FlushCache()
                out_ds = None
        elif i == 3:
            filename = '%s/tiff_direct_io_tiled_separate.tif' % prefix
            out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16', 'INTERLEAVE=BAND'])
            out_ds.FlushCache()
            out_ds = None
        elif i == 4:
            filename = '%s/tiff_direct_io_sparse.tif' % prefix
            out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options = ['SPARSE_OK=YES'])
            out_ds.FlushCache()
            out_ds = None
        elif i == 5:
            filename = '%s/tiff_direct_io_sparse_separate.tif' % prefix
            out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options = ['SPARSE_OK=YES', 'INTERLEAVE=BAND'])
            out_ds.FlushCache()
            out_ds = None
        elif i == 6:
            filename = '%s/tiff_direct_io_sparse_tiled.tif' % prefix
            out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options = ['SPARSE_OK=YES', 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16'])
            out_ds.FlushCache()
            out_ds = None
        else:
            filename = '%s/tiff_direct_io_sparse_tiled_separate.tif' % prefix
            out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options = ['SPARSE_OK=YES', 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16', 'INTERLEAVE=BAND'])
            out_ds.FlushCache()
            out_ds = None

        if truncated:
            ds = gdal.Open(filename)
            nbands = ds.RasterCount
            nxsize = ds.RasterXSize
            nysize = ds.RasterYSize
            (nblockxsize, nblockysize) = ds.GetRasterBand(1).GetBlockSize()
            band_interleaved = ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') == 'BAND'
            ds = None

            padding = 0
            if nblockxsize < nxsize:
                if (nysize % nblockysize) != 0:
                    padding = ((nxsize + nblockxsize - 1) / nblockxsize * nblockxsize) * (nblockysize - (nysize % nblockysize))
                if( nxsize % nblockxsize) != 0:
                    padding += nblockxsize - (nxsize % nblockxsize)
                padding *= dt_size
                if not band_interleaved:
                    padding *= nbands
                padding = int(padding)

            to_remove = 1
            if not band_interleaved:
                to_remove += (nbands-1) * dt_size

            f = gdal.VSIFOpenL(filename, 'rb')
            data = gdal.VSIFReadL(1, 1000000, f)
            gdal.VSIFCloseL(f)
            f = gdal.VSIFOpenL(filename, 'wb')
            gdal.VSIFWriteL(data, 1, len(data)-padding-to_remove, f)
            gdal.VSIFCloseL(f)

        ds = gdal.Open(filename)
        xoff = int(ds.RasterXSize/4)
        yoff = int(ds.RasterYSize/4)
        xsize = int(ds.RasterXSize/2)
        ysize = int(ds.RasterXSize/2)
        nbands = ds.RasterCount
        sizeof_float = 4

        if truncated:
            gdal.PushErrorHandler()
        ref_data_native_type = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize)
        ref_data_native_type_whole = ds.GetRasterBand(1).ReadRaster()
        ref_data_native_type_downsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2))
        ref_data_native_type_downsampled_not_nearest = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2), resample_alg = gdal.GRIORA_Bilinear)
        ref_data_native_type_upsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = nbands * xsize, buf_ysize = nbands * ysize)
        ref_data_native_type_custom_spacings = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = nbands * dt_size)
        ref_data_float32 = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_type = gdal.GDT_Float32)
        ref_nbands_data_native_type = ds.ReadRaster(xoff, yoff, xsize, ysize)
        ref_nbands_data_native_type_whole = ds.ReadRaster()
        ref_nbands_data_native_type_downsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2))
        ref_nbands_data_native_type_downsampled_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2), buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        ref_nbands_data_native_type_downsampled_not_nearest = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2), resample_alg = gdal.GRIORA_Bilinear)
        ref_nbands_data_native_type_upsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = 4 * xsize, buf_ysize = 4 * ysize)
        ref_nbands_data_native_type_downsampled_x_upsampled_y = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = 32 * ysize)
        ref_nbands_data_native_type_unordered_list = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list = [ nbands-i for i in range(nbands) ])
        ref_nbands_data_native_type_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        ref_nbands_data_native_type_pixel_interleaved_whole = ds.ReadRaster(buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        ref_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list = [i+1 for i in range(nbands-1)], buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        ref_nbands_data_float32 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type = gdal.GDT_Float32)
        ref_nbands_data_float32_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type = gdal.GDT_Float32, buf_pixel_space = nbands*sizeof_float, buf_band_space = 1*sizeof_float)
        ref_nbands_data_native_type_custom_spacings = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = 2 * nbands * dt_size, buf_band_space = dt_size)
        if nbands == 3:
            ref_nbands_data_native_type_custom_spacings_2 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = 4 * dt_size, buf_band_space = dt_size)
        if truncated:
            gdal.PopErrorHandler()
        ds = None

        if truncated:
            gdal.PushErrorHandler()
        old_val = gdal.GetConfigOption(option)
        gdal.SetConfigOption(option, 'YES')
        ds = gdal.Open(filename)
        band_interleaved = ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') == 'BAND'
        got_data_native_type = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize)
        got_data_native_type_whole = ds.GetRasterBand(1).ReadRaster()
        got_data_native_type_downsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2))
        got_data_native_type_downsampled_not_nearest = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2), resample_alg = gdal.GRIORA_Bilinear)
        got_data_native_type_upsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = nbands * xsize, buf_ysize = nbands * ysize)
        got_data_native_type_custom_spacings = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = nbands * dt_size)
        got_data_float32 = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_type = gdal.GDT_Float32)
        got_nbands_data_native_type = ds.ReadRaster(xoff, yoff, xsize, ysize)
        got_nbands_data_native_type_whole = ds.ReadRaster()
        got_nbands_data_native_type_bottom_right_downsampled = ds.ReadRaster(ds.RasterXSize-2, ds.RasterYSize - 1, 2, 1, buf_xsize = 1, buf_ysize = 1, buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        got_nbands_data_native_type_downsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2))
        got_nbands_data_native_type_downsampled_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2), buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        got_nbands_data_native_type_downsampled_not_nearest = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = int(ysize/2), resample_alg = gdal.GRIORA_Bilinear)
        got_nbands_data_native_type_upsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = 4 * xsize, buf_ysize = 4 * ysize)
        got_nbands_data_native_type_downsampled_x_upsampled_y = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize = int(xsize/2), buf_ysize = 32 * ysize)
        got_nbands_data_native_type_unordered_list = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list = [ nbands-i for i in range(nbands) ])
        got_nbands_data_native_type_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        got_nbands_data_native_type_pixel_interleaved_whole = ds.ReadRaster(buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        got_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list = [i+1 for i in range(nbands-1)], buf_pixel_space = nbands * dt_size, buf_band_space = dt_size)
        got_nbands_data_float32 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type = gdal.GDT_Float32)
        got_nbands_data_float32_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type = gdal.GDT_Float32, buf_pixel_space = nbands*sizeof_float, buf_band_space = 1*sizeof_float)
        got_nbands_data_native_type_custom_spacings = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = 2 * nbands * dt_size, buf_band_space = dt_size)
        if nbands == 3:
            got_nbands_data_native_type_custom_spacings_2 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space = 4 * dt_size, buf_band_space = dt_size)
        ds = None
        gdal.SetConfigOption(option, old_val)
        if truncated:
            gdal.PopErrorHandler()

        gdal.Unlink(filename)

        if ref_data_native_type != got_data_native_type:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if truncated and not band_interleaved:
            if got_data_native_type_whole is not None:
                gdaltest.post_reason('fail')
                print(truncated)
                print(band_interleaved)
                print(option)
                print(i)
                print(gdal.GetDataTypeName(dt))
                return 'fail'
        elif ref_data_native_type_whole != got_data_native_type_whole:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if ref_data_native_type_downsampled != got_data_native_type_downsampled:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if not truncated and ref_data_native_type_downsampled_not_nearest != got_data_native_type_downsampled_not_nearest:
            gdaltest.post_reason('fail')
            print(truncated)
            print(band_interleaved)
            print(option)
            print(i)
            return 'fail'

        if ref_data_native_type_upsampled != got_data_native_type_upsampled:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        for y in range(ysize):
            for x in range(xsize):
              for k in range(dt_size):
                if ref_data_native_type_custom_spacings[(y*xsize+x)*nbands*dt_size+k] != got_data_native_type_custom_spacings[(y*xsize+x)*nbands*dt_size+k]:
                    gdaltest.post_reason('fail')
                    print(gdal.GetDataTypeName(dt))
                    print(option)
                    print(i)
                    return 'fail'
                if not truncated:
                    for band in range(nbands):
                        if ref_nbands_data_native_type_custom_spacings[(y*xsize+x)*2*nbands*dt_size+band*dt_size+k] != got_nbands_data_native_type_custom_spacings[(y*xsize+x)*2*nbands*dt_size+band*dt_size+k]:
                            gdaltest.post_reason('fail')
                            print(gdal.GetDataTypeName(dt))
                            print(option)
                            print(i)
                            print(x,y,k,band)
                            return 'fail'
                    if nbands == 3:
                        for band in range(nbands):
                            if ref_nbands_data_native_type_custom_spacings_2[(y*xsize+x)*4*dt_size+band*dt_size+k] != got_nbands_data_native_type_custom_spacings_2[(y*xsize+x)*4*dt_size+band*dt_size+k]:
                                gdaltest.post_reason('fail')
                                print(gdal.GetDataTypeName(dt))
                                print(option)
                                print(i)
                                print(x,y,k,band)
                                return 'fail'

        if ref_data_float32 != got_data_float32:
            gdaltest.post_reason('fail')
            print(gdal.GetDataTypeName(dt))
            print(option)
            print(i)
            return 'fail'

        if not truncated and ref_nbands_data_native_type != got_nbands_data_native_type:
            gdaltest.post_reason('fail')
            print(truncated)
            print(band_interleaved)
            print(option)
            print(i)
            return 'fail'

        if truncated:
            if got_nbands_data_native_type_whole is not None:
                gdaltest.post_reason('fail')
                print(gdal.GetDataTypeName(dt))
                print(option)
                print(i)
                return 'fail'
        elif ref_nbands_data_native_type_whole != got_nbands_data_native_type_whole:
            gdaltest.post_reason('fail')
            print(gdal.GetDataTypeName(dt))
            print(option)
            print(i)
            return 'fail'

        if truncated:
            if got_nbands_data_native_type_pixel_interleaved_whole is not None:
                gdaltest.post_reason('fail')
                print(option)
                print(i)
                return 'fail'
        elif ref_nbands_data_native_type_pixel_interleaved_whole != got_nbands_data_native_type_pixel_interleaved_whole:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if truncated and got_nbands_data_native_type_bottom_right_downsampled is not None:
            gdaltest.post_reason('fail')
            print(gdal.GetDataTypeName(dt))
            print(option)
            print(i)
            return 'fail'

        if truncated:
            continue

        if ref_nbands_data_native_type_downsampled != got_nbands_data_native_type_downsampled:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if ref_nbands_data_native_type_downsampled_interleaved != got_nbands_data_native_type_downsampled_interleaved:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if ref_nbands_data_native_type_downsampled_not_nearest != got_nbands_data_native_type_downsampled_not_nearest:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if ref_nbands_data_native_type_upsampled != got_nbands_data_native_type_upsampled:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            #import struct
            #f1 = open('out1.txt', 'wb')
            #f2 = open('out2.txt', 'wb')
            #for b in range(nbands):
            #    for y in range(4 * ysize):
            #        f1.write('%s\n' % str(struct.unpack('B' * 4 * xsize, ref_nbands_data_native_type_upsampled[(b * 4 * ysize + y) * 4 * xsize : (b * 4 * ysize + y + 1) * 4 * xsize])))
            #        f2.write('%s\n' % str(struct.unpack('B' * 4 * xsize, got_nbands_data_native_type_upsampled[(b * 4 * ysize + y) * 4 * xsize : (b * 4 * ysize + y + 1) * 4 * xsize])))
            return 'fail'

        if ref_nbands_data_native_type_downsampled_x_upsampled_y != got_nbands_data_native_type_downsampled_x_upsampled_y:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            #import struct
            #f1 = open('out1.txt', 'wb')
            #f2 = open('out2.txt', 'wb')
            #for b in range(nbands):
            #    for y in range(32 * ysize):
            #        f1.write('%s\n' % str(struct.unpack('B' * int(xsize/2), ref_nbands_data_native_type_downsampled_x_upsampled_y[(b * 32 * ysize + y) * int(xsize/2) : (b * 32 * ysize + y + 1) * int(xsize/2)])))
            #        f2.write('%s\n' % str(struct.unpack('B' * int(xsize/2), got_nbands_data_native_type_downsampled_x_upsampled_y[(b * 32 * ysize + y) * int(xsize/2) : (b * 32 * ysize + y + 1) * int(xsize/2)])))
            return 'fail'

        if ref_nbands_data_native_type_unordered_list != got_nbands_data_native_type_unordered_list:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if ref_nbands_data_native_type_pixel_interleaved != got_nbands_data_native_type_pixel_interleaved:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        for y in range(ysize):
            for x in range(xsize):
                for b in range(nbands-1):
                  for k in range(dt_size):
                    if ref_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space[((y*xsize+x)*nbands+b)*dt_size+k] != got_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space[((y*xsize+x)*nbands+b)*dt_size+k]:
                        gdaltest.post_reason('fail')
                        print(option)
                        print(i)
                        print(y)
                        print(x)
                        print(b)
                        return 'fail'

        if ref_nbands_data_float32 != got_nbands_data_float32:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

        if ref_nbands_data_float32_pixel_interleaved != got_nbands_data_float32_pixel_interleaved:
            gdaltest.post_reason('fail')
            print(option)
            print(i)
            return 'fail'

  ds = gdal.Open('data/byte.tif') # any GTiff file will do
  unreached = ds.GetMetadataItem('UNREACHED_VIRTUALMEMIO_CODE_PATH', '_DEBUG_')
  ds = None
  if unreached:
      gdaltest.post_reason('missing code coverage in VirtualMemIO()')
      print('unreached = %s' % unreached)
      return 'fail'

  return 'success'

###############################################################################
# Check read Digital Globe metadata IMD & RPB format

def tiff_read_md1():

    try:
        os.remove('data/md_dg.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_dg.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2010-04-01 12:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_dg.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_dg.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Digital Globe metadata XML format

def tiff_read_md2():

    try:
        os.remove('data/md_dg_2.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_dg_2.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 2:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2011-05-01 13:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_dg_2.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_dg_2.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read GeoEye metadata format

def tiff_read_md3():

    try:
        os.remove('data/md_ge_rgb_0010000.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_ge_rgb_0010000.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2012-06-01 14:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_ge_rgb_0010000.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_ge_rgb_0010000.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read OrbView metadata format

def tiff_read_md4():

    try:
        os.remove('data/md_ov.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_ov.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2013-07-01 15:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_ov.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_ov.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Resurs-DK1 metadata format

def tiff_read_md5():

    try:
        os.remove('data/md_rdk1.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_rdk1.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 2:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 5:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2014-08-01 16:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_rdk1.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_rdk1.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Landsat metadata format

def tiff_read_md6():

    try:
        os.remove('data/md_ls_b1.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_ls_b1.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 2:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 5:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2015-09-01 17:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_ls_b1.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_ls_b1.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Spot metadata format

def tiff_read_md7():

    try:
        os.remove('data/spot/md_spot.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/spot/md_spot.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 2:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 5:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2001-03-01 00:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/spot/md_spot.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/spot/md_spot.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read RapidEye metadata format

def tiff_read_md8():

    try:
        os.remove('data/md_re.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_re.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 2:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 5:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2010-02-01 12:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_re.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_re.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Alos metadata format

def tiff_read_md9():

    try:
        os.remove('data/alos/IMG-md_alos.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/alos/IMG-md_alos.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2010-07-01 00:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/alos/IMG-md_alos.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/alos/IMG-md_alos.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Eros metadata format

def tiff_read_md10():

    try:
        os.remove('data/md_eros.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_eros.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2013-04-01 11:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_eros.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_eros.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Kompsat metadata format

def tiff_read_md11():

    try:
        os.remove('data/md_kompsat.tif.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/md_kompsat.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2007-05-01 07:00:00':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    ds = None

    try:
        os.stat('data/md_kompsat.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_kompsat.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check read Dimap metadata format

def tiff_read_md12():

    ds = gdal.Open( '../gdrivers/data/dimap2/IMG_foo_R2C1.TIF', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    metadata = ds.GetMetadataDomainList()
    if len(metadata) != 6:
        gdaltest.post_reason( 'did not get expected metadata list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    # Test UTC date
    if md['ACQUISITIONDATETIME'] != '2016-06-17 12:34:56':
        print('bad value for IMAGERY[ACQUISITIONDATETIME]')
        return 'fail'

    # Test RPC and that we have a LINE_OFF shift
    rpc = ds.GetMetadata('RPC')
    if rpc['LINE_OFF'] != '-11':
        gdaltest.post_reason( 'RPC wrong.' )
        print(rpc)
        return 'fail'

    ds = None

    try:
        os.stat('data/md_kompsat.tif.aux.xml')
        gdaltest.post_reason('Expected not generation of data/md_kompsat.tif.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test reading a TIFFTAG_GDAL_NODATA with empty text

def tiff_read_empty_nodata_tag():

    ds = gdal.Open('data/empty_nodata.tif')
    if ds.GetRasterBand(1).GetNoDataValue() is not None:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'

    return 'success'

###############################################################################
# Check that no auxiliary files are read with a simple Open(), reading
# imagery and getting IMAGE_STRUCTURE metadata

def tiff_read_strace_check():

    if not sys.platform.startswith('linux'):
        return 'skip'

    python_exe = sys.executable
    cmd = "strace -f %s -c \"from osgeo import gdal; " % python_exe + (
        "gdal.SetConfigOption('CPL_DEBUG', 'OFF');"
        "ds = gdal.Open('../gcore/data/byte.tif');"
        "ds.ReadRaster();"
        "ds.GetMetadata('IMAGE_STRUCTURE');"
        "ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE');"
        " \" " )
    try:
        (out, err) = gdaltest.runexternal_out_and_err(cmd)
    except:
        # strace not available
        return 'skip'

    lines_with_dotdot_gcore = []
    for line in err.split('\n'):
        if line.find('../gcore') >= 0:
            lines_with_dotdot_gcore += [ line ]

    if len(lines_with_dotdot_gcore) != 1:
        gdaltest.post_reason('fail')
        print(lines_with_dotdot_gcore)
        return 'fail'

    return 'success'

###############################################################################
# Test GDAL_READDIR_LIMIT_ON_OPEN

def tiff_read_readdir_limit_on_open():

    gdal.SetConfigOption('GDAL_READDIR_LIMIT_ON_OPEN', '1')

    ds = gdal.Open( 'data/md_kompsat.tif', gdal.GA_ReadOnly )
    filelist = ds.GetFileList()

    gdal.SetConfigOption('GDAL_READDIR_LIMIT_ON_OPEN', None)

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    return 'success'

###############################################################################
#
def tiff_read_minisblack_as_rgba():

    if not gdaltest.supports_force_rgba:
        return 'skip'

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/byte.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount) ]
    if got_cs != [4672,4672,4672,4873]:
        gdaltest.post_reason( 'fail')
        print(got_cs)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
#
def tiff_read_colortable_as_rgba():

    if not gdaltest.supports_force_rgba:
        return 'skip'

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/test_average_palette.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount) ]
    if got_cs != [2433,2433,2433,4873]:
        gdaltest.post_reason( 'fail')
        print(got_cs)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
#
def tiff_read_logl_as_rgba():

    if not gdaltest.supports_force_rgba:
        return 'skip'

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/uint16_sgilog.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount) ]
    # I'm pretty sure this isn't the expected result...
    if got_cs != [0,0,0,4873]:
        gdaltest.post_reason( 'fail')
        print(got_cs)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
#
def tiff_read_scanline_more_than_2GB():

    with gdaltest.error_handler():
        ds = gdal.Open('data/scanline_more_than_2GB.tif')
    if ds is not None:
        return 'fail'
    return 'success'

###############################################################################
# Test that we are at least robust to wrong number of ExtraSamples and warn
# about it

def tiff_read_wrong_number_extrasamples():

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('data/6band_wrong_number_extrasamples.tif')
    if gdal.GetLastErrorMsg().find('Wrong number of ExtraSamples') < 0:
        gdaltest.post_reason( 'fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'
    if ds.GetRasterBand(6).GetRasterColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason( 'fail')
        print(ds.GetRasterBand(6).GetRasterColorInterpretation())
        return 'fail'

    return 'success'

###############################################################################
# Test that we can read a one-trip TIFF without StripByteCounts tag

def tiff_read_one_strip_no_bytecount():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/one_strip_nobytecount.tif')
    gdal.PopErrorHandler()
    if ds.GetRasterBand(1).Checksum() != 1:
        gdaltest.post_reason( 'fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    return 'success'

###############################################################################
# Test GDAL_GEOREF_SOURCES

def tiff_read_nogeoref():

    tests = [ (None, True, True, False, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              (None, True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              (None, False, True, True, 'OSGB_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
              (None, True, False, False, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              (None, False, True, False, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              (None, False, False, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('INTERNAL', True, True, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('INTERNAL,PAM', True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              ('INTERNAL,WORLDFILE', True, True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('INTERNAL,PAM,WORLDFILE', True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              ('INTERNAL,WORLDFILE,PAM', True, True, True, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM,INTERNAL', False, False, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('PAM,WORLDFILE,INTERNAL', False, False, True, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ('TABFILE,WORLDFILE,INTERNAL', True, True, True, 'OSGB_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
              ('PAM', True, True, False, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              ('PAM,WORLDFILE', True, True, False, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              ('WORLDFILE', True, True, False, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM', True, True, False, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,INTERNAL', True, True, False, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,PAM,INTERNAL', True, True, False, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('WORLDFILE,INTERNAL,PAM', True, True, False, 'LOCAL_CS["PAM"]', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('NONE', True, True, False, '', (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)),
              ]

    for (config_option_value, copy_pam, copy_worldfile, copy_tabfile, expected_srs, expected_gt) in tests:
        for iteration in range(2):
            gdal.SetConfigOption('GDAL_GEOREF_SOURCES', config_option_value)
            gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.tif', open('data/byte_nogeoref.tif', 'rb').read())
            if copy_pam:
                gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.tif.aux.xml', open('data/byte_nogeoref.tif.aux.xml', 'rb').read())
            if copy_worldfile:
                gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.tfw', open('data/byte_nogeoref.tfw', 'rb').read())
            if copy_tabfile:
                gdal.FileFromMemBuffer('/vsimem/byte_nogeoref.tab', open('data/byte_nogeoref.tab', 'rb').read())

            ds = gdal.Open('/vsimem/byte_nogeoref.tif')
            if iteration == 0:
                gt = ds.GetGeoTransform()
                srs_wkt = ds.GetProjectionRef()
            else:
                srs_wkt = ds.GetProjectionRef()
                gt = ds.GetGeoTransform()
            ds = None
            gdal.SetConfigOption('GDAL_GEOREF_SOURCES', None)
            gdal.Unlink('/vsimem/byte_nogeoref.tif')
            gdal.Unlink('/vsimem/byte_nogeoref.tif.aux.xml')
            gdal.Unlink('/vsimem/byte_nogeoref.tfw')
            gdal.Unlink('/vsimem/byte_nogeoref.tab')

            if gt != expected_gt:
                gdaltest.post_reason('Iteration %d, did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration,config_option_value,str(copy_pam),str(copy_worldfile),str(copy_tabfile)))
                print('Got ' + str(gt))
                print('Expected ' + str(expected_gt))
                return 'fail'

            if (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and srs_wkt.find(expected_srs) < 0):
                gdaltest.post_reason('Iteration %d, did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration,config_option_value,str(copy_pam),str(copy_worldfile),str(copy_tabfile)))
                print('Got ' + srs_wkt)
                print('Expected ' + expected_srs)
                return 'fail'

    return 'success'

###############################################################################
# Test GDAL_GEOREF_SOURCES

def tiff_read_inconsistent_georef():

    tests = [ (None, True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              (None, False, True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              (None, False, False, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              (None, False, True, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              (None, False, False, False, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              (None, True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              (None, True, False, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              (None, True, True, False, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              (None, True, False, False, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              ('INTERNAL', True, True, True, '26711', (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)),
              ('PAM', True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              ('PAM,TABFILE', True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
              ('WORLDFILE', True, True, True, '', (99.5, 1.0, 0.0, 200.5, 0.0, -1.0)),
              ('TABFILE', True, True, True, 'OSGB_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
              ('TABFILE,PAM', True, True, True, 'OSGB_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
              ]

    for (config_option_value, copy_pam, copy_worldfile, copy_tabfile, expected_srs, expected_gt) in tests:
        for iteration in range(2):
            gdal.SetConfigOption('GDAL_GEOREF_SOURCES', config_option_value)
            gdal.FileFromMemBuffer('/vsimem/byte_inconsistent_georef.tif', open('data/byte_inconsistent_georef.tif', 'rb').read())
            if copy_pam:
                gdal.FileFromMemBuffer('/vsimem/byte_inconsistent_georef.tif.aux.xml', open('data/byte_inconsistent_georef.tif.aux.xml', 'rb').read())
            if copy_worldfile:
                gdal.FileFromMemBuffer('/vsimem/byte_inconsistent_georef.tfw', open('data/byte_inconsistent_georef.tfw', 'rb').read())
            if copy_tabfile:
                gdal.FileFromMemBuffer('/vsimem/byte_inconsistent_georef.tab', open('data/byte_inconsistent_georef.tab', 'rb').read())
            ds = gdal.Open('/vsimem/byte_inconsistent_georef.tif')
            if iteration == 0:
                gt = ds.GetGeoTransform()
                srs_wkt = ds.GetProjectionRef()
            else:
                srs_wkt = ds.GetProjectionRef()
                gt = ds.GetGeoTransform()
            ds = None
            gdal.SetConfigOption('GDAL_GEOREF_SOURCES', None)
            gdal.Unlink('/vsimem/byte_inconsistent_georef.tif')
            gdal.Unlink('/vsimem/byte_inconsistent_georef.tif.aux.xml')
            gdal.Unlink('/vsimem/byte_inconsistent_georef.tfw')
            gdal.Unlink('/vsimem/byte_inconsistent_georef.tab')

            if gt != expected_gt:
                gdaltest.post_reason('Iteration %d, did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration,config_option_value,str(copy_pam),str(copy_worldfile),str(copy_tabfile)))
                print('Got ' + str(gt))
                print('Expected ' + str(expected_gt))
                return 'fail'

            if (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and srs_wkt.find(expected_srs) < 0):
                gdaltest.post_reason('Iteration %d, did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration,config_option_value,str(copy_pam),str(copy_worldfile),str(copy_tabfile)))
                print('Got ' + srs_wkt)
                print('Expected ' + expected_srs)
                return 'fail'

    return 'success'

###############################################################################
# Test GDAL_GEOREF_SOURCES

def tiff_read_gcp_internal_and_auxxml():

    tests = [ (None, True, 'LOCAL_CS["PAM"]', 1),
              (None, False, '4326', 2),
              ('INTERNAL', True, '4326', 2),
              ('INTERNAL', False, '4326', 2),
              ('INTERNAL,PAM', True, '4326', 2),
              ('INTERNAL,PAM', False, '4326', 2),
              ('PAM', True, 'LOCAL_CS["PAM"]', 1),
              ('PAM', False, '', 0),
              ('PAM,INTERNAL', True, 'LOCAL_CS["PAM"]', 1),
              ('PAM,INTERNAL', False, '4326', 2),
              ]

    for (config_option_value, copy_pam, expected_srs, expected_gcp_count) in tests:
        for iteration in range(2):
            gdal.FileFromMemBuffer('/vsimem/byte_gcp.tif', open('data/byte_gcp.tif', 'rb').read())
            if copy_pam:
                gdal.FileFromMemBuffer('/vsimem/byte_gcp.tif.aux.xml', open('data/byte_gcp.tif.aux.xml', 'rb').read())
            open_options = []
            if config_option_value is not None:
                open_options += [ 'GEOREF_SOURCES=' + config_option_value ]
            ds = gdal.OpenEx('/vsimem/byte_gcp.tif', open_options = open_options)
            if iteration == 0:
                gcp_count = ds.GetGCPCount()
                srs_wkt = ds.GetGCPProjection()
            else:
                srs_wkt = ds.GetGCPProjection()
                gcp_count= ds.GetGCPCount()
            ds = None
            gdal.Unlink('/vsimem/byte_gcp.tif')
            gdal.Unlink('/vsimem/byte_gcp.tif.aux.xml')

            if gcp_count != expected_gcp_count:
                gdaltest.post_reason('Iteration %d, did not get expected gcp count for %s,copy_pam=%s' % (iteration,config_option_value,str(copy_pam)))
                print('Got ' + str(gcp_count))
                print('Expected ' + str(expected_gcp_count))
                return 'fail'

            if (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and srs_wkt.find(expected_srs) < 0):
                gdaltest.post_reason('Iteration %d, did not get expected SRS for %s,copy_pam=%s' % (iteration,config_option_value,str(copy_pam)))
                print('Got ' + srs_wkt)
                print('Expected ' + expected_srs)
                return 'fail'

    return 'success'

###############################################################################
# Test reading .tif + .aux

class myHandlerClass:
    def __init__(self):
        self.msg = None

    def handler(self, eErrClass, err_no, msg):
        if msg.find('File open of') >= 0:
            self.msg = msg

def tiff_read_aux():

    gdal.ErrorReset()
    ds = gdal.Open('data/f2r23.tif')
    handler = myHandlerClass()
    gdal.PushErrorHandler(handler.handler)
    ds.GetFileList()
    gdal.PopErrorHandler()
    if handler.msg is not None:
        gdaltest.post_reason('Got message that indicate recursive calls: %s' % handler.msg)
        return 'fail'

    return 'success'


def tiff_read_one_band_from_two_bands():

    gdal.Translate('/vsimem/tiff_read_one_band_from_two_bands.tif', 'data/byte.tif', options = '-b 1 -b 1')
    gdal.Translate('/vsimem/tiff_read_one_band_from_two_bands_dst.tif', '/vsimem/tiff_read_one_band_from_two_bands.tif', options = '-b 1')

    ds = gdal.Open('/vsimem/tiff_read_one_band_from_two_bands_dst.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/tiff_read_one_band_from_two_bands.tif')
    gdal.Unlink('/vsimem/tiff_read_one_band_from_two_bands_dst.tif')

    return 'success'


def tiff_read_jpeg_cloud_optimized():

    for i in range(4):
        ds = gdal.Open('data/byte_ovr_jpeg_tablesmode%d.tif' % i)
        cs0 = ds.GetRasterBand(1).Checksum()
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        if cs0 != 4743 or cs1 != 1133:
            gdaltest.post_reason('failure')
            print(i)
            print(cs0)
            print(cs1)
            return 'fail'
        ds = None

    return 'success'

# This one was generated with a buggy code that emit JpegTables with mode == 1
# when creating the overview directory but failed to properly set this mode while
# writing the imagery. libjpeg-6b emits a 'JPEGLib:Huffman table 0x00 was not defined'
# error while jpeg-8 works fine
def tiff_read_corrupted_jpeg_cloud_optimized():

    ds = gdal.Open('data/byte_ovr_jpeg_tablesmode_not_correctly_set_on_ovr.tif')
    cs0 = ds.GetRasterBand(1).Checksum()
    if cs0 != 4743:
        gdaltest.post_reason('failure')
        print(cs0)
        return 'fail'

    with gdaltest.error_handler():
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs1 == 0:
        print('Expected error while writing overview with libjpeg-6b')
    elif cs1 != 1133:
        gdaltest.post_reason('failure')
        print(cs1)
        return 'fail'

    return 'success'

###############################################################################
# Test reading YCbCr images with LZW compression

def tiff_read_ycbcr_lzw():

    tests = [ ('ycbcr_11_lzw.tif', 13459, 12939, 12414),
              ('ycbcr_12_lzw.tif', 13565, 13105, 12660),
              ('ycbcr_14_lzw.tif', 0, 0, 0), # not supported
              ('ycbcr_21_lzw.tif', 13587, 13297, 12760),
              ('ycbcr_22_lzw.tif', 13393, 13137, 12656),
              ('ycbcr_24_lzw.tif', 0, 0, 0), # not supported
              ('ycbcr_41_lzw.tif', 13218, 12758, 12592),
              ('ycbcr_42_lzw.tif', 13277, 12779, 12614),
              ('ycbcr_42_lzw_optimized.tif', 19918, 20120, 19087),
              ('ycbcr_44_lzw.tif', 12994, 13229, 12149),
              ('ycbcr_44_lzw_optimized.tif', 19666, 19860, 18836) ]

    for (filename, cs1, cs2, cs3) in tests:
        ds = gdal.Open('data/' + filename)
        if cs1 == 0:
            gdal.PushErrorHandler()
        got_cs1 = ds.GetRasterBand(1).Checksum()
        got_cs2 = ds.GetRasterBand(2).Checksum()
        got_cs3 = ds.GetRasterBand(3).Checksum()
        if cs1 == 0:
            gdal.PopErrorHandler()
        if got_cs1 != cs1 or got_cs2 != cs2 or got_cs3 != cs3:
            gdaltest.post_reason('failure')
            print(filename, got_cs1, got_cs2, got_cs3)
            return 'fail'

    return 'success'

###############################################################################
# Test reading YCbCr images with nbits > 8

def tiff_read_ycbcr_int12():

    with gdaltest.error_handler():
        ds = gdal.Open('data/int12_ycbcr_contig.tif')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Cannot open TIFF file with') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test reading band unit from VERT_CS unit (#6675)

def tiff_read_unit_from_srs():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_read_unit_from_srs.tif', 1, 1)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('EPSG:4326+3855')
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('/vsimem/tiff_read_unit_from_srs.tif')
    unit = ds.GetRasterBand(1).GetUnitType()
    if unit != 'metre':
        gdaltest.post_reason('fail')
        print(unit)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tiff_read_unit_from_srs.tif')

    return 'success'

###############################################################################
# Test reading ArcGIS 9.3 .aux.xml

def tiff_read_arcgis93_geodataxform_gcp():

    ds = gdal.Open('data/arcgis93_geodataxform_gcp.tif')
    if ds.GetGCPProjection().find('26712') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetGCPCount() != 16:
        gdaltest.post_reason('fail')
        return 'fail'
    gcp = ds.GetGCPs()[0]
    if abs(gcp.GCPPixel - 565) > 1e-5 or \
       abs(gcp.GCPLine - 11041) > 1e-5 or \
       abs(gcp.GCPX - 500000) > 1e-5 or \
       abs(gcp.GCPY - 4705078.79016612) > 1e-5 or \
       abs(gcp.GCPZ - 0) > 1e-5:
        gdaltest.post_reason('fail')
        return 'fail'
    return 'success'

###############################################################################
# Test reading file with block size > signed int 32 bit

def tiff_read_block_width_above_32bit():

    with gdaltest.error_handler():
        ds = gdal.Open('data/block_width_above_32bit.tif')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    return 'success'

###############################################################################
# Test reading file with image size > signed int 32 bit

def tiff_read_image_width_above_32bit():

    with gdaltest.error_handler():
        ds = gdal.Open('data/image_width_above_32bit.tif')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    return 'success'

###############################################################################
# Test reading file with image size > signed int 32 bit

def tiff_read_second_image_width_above_32bit():

    ds = gdal.Open('data/second_image_width_above_32bit.tif')
    with gdaltest.error_handler():
        if ds.GetMetadata("SUBDATASETS") != {}:
            gdaltest.post_reason('fail')
            return 'fail'

    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:2:data/second_image_width_above_32bit.tif')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test reading file with minimal number of warnings without warning

def tiff_read_minimum_tiff_tags_no_warning():

    gdal.ErrorReset()
    ds = gdal.Open('data/minimum_tiff_tags_no_warning.tif')
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds.GetRasterBand(1).Checksum()
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test reading file with minimal number of warnings but warning

def tiff_read_minimum_tiff_tags_with_warning():

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('data/minimum_tiff_tags_with_warning.tif')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    ds.GetRasterBand(1).Checksum()
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'


###############################################################################

def check_libtiff_internal_or_greater(expected_maj,expected_min,expected_micro):

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] == 'INTERNAL':
        return True
    if md['LIBTIFF'].startswith('LIBTIFF, Version '):
        version = md['LIBTIFF'][len('LIBTIFF, Version '):]
        version = version[0:version.find('\n')]
        got_maj, got_min, got_micro = version.split('.')
        got_maj = int(got_maj)
        got_min = int(got_min)
        got_micro = int(got_micro)
        if got_maj > expected_maj:
            return True
        if got_maj < expected_maj:
            return False
        if got_min > expected_min:
            return True
        if got_min < expected_min:
            return False
        return got_micro >= expected_micro
    return False

###############################################################################

def tiff_read_leak_ZIPSetupDecode():

    if not check_libtiff_internal_or_greater(4,0,8):
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/leak-ZIPSetupDecode.tif')
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i+1).Checksum()

    return 'success'

###############################################################################

def tiff_read_excessive_memory_TIFFFillStrip():

    if not check_libtiff_internal_or_greater(4,0,8):
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/excessive-memory-TIFFFillStrip.tif')
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i+1).Checksum()

    return 'success'

###############################################################################

def tiff_read_excessive_memory_TIFFFillStrip2():

    if not check_libtiff_internal_or_greater(4,0,8):
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/excessive-memory-TIFFFillStrip2.tif')
        ds.GetRasterBand(1).Checksum()

    return 'success'

###############################################################################

def tiff_read_big_strip():

    if not check_libtiff_internal_or_greater(4,0,8):
        return 'skip'

    gdal.Translate('/vsimem/test.tif', 'data/byte.tif', options = '-co compress=lzw -outsize 10000 2000  -co blockysize=2000 -r bilinear -ot float32')
    ds = gdal.Open('/vsimem/test.tif')
    if ds.GetRasterBand(1).Checksum() != 2676:
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/test.tif')

    return 'success'

###############################################################################

def tiff_read_big_tile():

    if not check_libtiff_internal_or_greater(4,0,8):
        return 'skip'

    gdal.Translate('/vsimem/test.tif', 'data/byte.tif', options = '-co compress=lzw -outsize 10000 2000 -co tiled=yes -co blockxsize=10000 -co blockysize=2000 -r bilinear -ot float32')
    ds = gdal.Open('/vsimem/test.tif')
    if ds.GetRasterBand(1).Checksum() != 2676:
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/test.tif')

    return 'success'

###############################################################################

def tiff_read_huge_number_strips():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/huge-number-strips.tif')
        ds.GetRasterBand(1).Checksum()

    return 'success'

###############################################################################

def tiff_read_many_blocks():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        return 'skip'

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/test.tif', 1, 2000000, options = ['BLOCKYSIZE=1'])
    ds = None
    ds = gdal.Open('/vsimem/test.tif')
    if ds.GetRasterBand(1).Checksum() != 0:
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/test.tif')

    return 'success'

###############################################################################
# Test reading  images with nbits > 32

def tiff_read_uint33():

    with gdaltest.error_handler():
        ds = gdal.Open('data/uint33.tif')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Unsupported TIFF configuration') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1545
def tiff_read_corrupted_deflate_singlestrip():

    if not check_libtiff_internal_or_greater(4,0,8):
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/corrupted_deflate_singlestrip.tif')
        ds.GetRasterBand(1).Checksum()

    return 'success'

###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1563

def tiff_read_packbits_not_enough_data():

    if not check_libtiff_internal_or_greater(4,0,8):
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/packbits-not-enough-data.tif')
        ds.GetRasterBand(1).Checksum()

    return 'success'

###############################################################################
# Test reading images with more than 2billion blocks

def tiff_read_toomanyblocks():

    with gdaltest.error_handler():
        ds = gdal.Open('data/toomanyblocks.tif')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################

for item in init_list:
    ut = gdaltest.GDALTest( 'GTiff', item[0], item[1], item[2] )
    if ut is None:
        print( 'GTiff tests skipped' )
        sys.exit()
    gdaltest_list.append( (ut.testOpen, item[0]) )
gdaltest_list.append( (tiff_read_off) )
gdaltest_list.append( (tiff_check_alpha) )
gdaltest_list.append( (tiff_read_cmyk_rgba) )
gdaltest_list.append( (tiff_read_cmyk_raw) )
gdaltest_list.append( (tiff_read_ojpeg) )
gdaltest_list.append( (tiff_read_gzip) )
gdaltest_list.append( (tiff_read_zip_1) )
gdaltest_list.append( (tiff_read_zip_2) )
gdaltest_list.append( (tiff_read_zip_3) )
gdaltest_list.append( (tiff_read_zip_4) )
gdaltest_list.append( (tiff_read_zip_5) )
gdaltest_list.append( (tiff_read_tar_1) )
gdaltest_list.append( (tiff_read_tar_2) )
gdaltest_list.append( (tiff_read_tgz_1) )
gdaltest_list.append( (tiff_read_tgz_2) )
gdaltest_list.append( (tiff_grads) )
gdaltest_list.append( (tiff_citation) )
gdaltest_list.append( (tiff_linearparmunits) )
gdaltest_list.append( (tiff_linearparmunits2) )
gdaltest_list.append( (tiff_g4_split) )
gdaltest_list.append( (tiff_multi_images) )
gdaltest_list.append( (tiff_vsimem) )
gdaltest_list.append( (tiff_vsizip_and_mem) )
gdaltest_list.append( (tiff_ProjectedCSTypeGeoKey_only) )
gdaltest_list.append( (tiff_GTModelTypeGeoKey_only) )
gdaltest_list.append( (tiff_12bitjpeg) )
gdaltest_list.append( (tiff_read_stats_from_pam) )
gdaltest_list.append( (tiff_read_from_tab) )
gdaltest_list.append( (tiff_read_pixelispoint) )
gdaltest_list.append( (tiff_read_geomatrix) )
gdaltest_list.append( (tiff_read_corrupted_gtiff) )
gdaltest_list.append( (tiff_read_tag_without_null_byte) )
gdaltest_list.append( (tiff_read_buggy_packbits) )
gdaltest_list.append( (tiff_read_rpc_txt) )
gdaltest_list.append( (tiff_read_rpc_tif) )
gdaltest_list.append( (tiff_small) )
gdaltest_list.append( (tiff_dos_strip_chop) )
gdaltest_list.append( (tiff_read_exif_and_gps) )
gdaltest_list.append( (tiff_jpeg_rgba_pixel_interleaved) )
gdaltest_list.append( (tiff_jpeg_rgba_band_interleaved) )
gdaltest_list.append( (tiff_read_huge4GB) )
gdaltest_list.append( (tiff_read_bigtiff) )
gdaltest_list.append( (tiff_read_tiff_metadata) )
gdaltest_list.append( (tiff_read_irregular_tile_size_jpeg_in_tiff) )
gdaltest_list.append( (tiff_direct_and_virtual_mem_io) )
gdaltest_list.append( (tiff_read_empty_nodata_tag) )
gdaltest_list.append( (tiff_read_strace_check) )
gdaltest_list.append( (tiff_read_readdir_limit_on_open) )
gdaltest_list.append( (tiff_read_minisblack_as_rgba) )
gdaltest_list.append( (tiff_read_colortable_as_rgba) )
gdaltest_list.append( (tiff_read_logl_as_rgba) )
gdaltest_list.append( (tiff_read_scanline_more_than_2GB) )
gdaltest_list.append( (tiff_read_wrong_number_extrasamples) )
gdaltest_list.append( (tiff_read_one_strip_no_bytecount) )

gdaltest_list.append( (tiff_read_md1) )
gdaltest_list.append( (tiff_read_md2) )
gdaltest_list.append( (tiff_read_md3) )
gdaltest_list.append( (tiff_read_md4) )
gdaltest_list.append( (tiff_read_md5) )
gdaltest_list.append( (tiff_read_md6) )
gdaltest_list.append( (tiff_read_md7) )
gdaltest_list.append( (tiff_read_md8) )
gdaltest_list.append( (tiff_read_md9) )
gdaltest_list.append( (tiff_read_md10) )
gdaltest_list.append( (tiff_read_md11) )
gdaltest_list.append( (tiff_read_md12) )

gdaltest_list.append( (tiff_read_nogeoref) )
gdaltest_list.append( (tiff_read_inconsistent_georef) )
gdaltest_list.append( (tiff_read_gcp_internal_and_auxxml) )

gdaltest_list.append( (tiff_read_aux) )

gdaltest_list.append( (tiff_read_one_band_from_two_bands) )

gdaltest_list.append( (tiff_read_jpeg_cloud_optimized) )
gdaltest_list.append( (tiff_read_corrupted_jpeg_cloud_optimized) )

gdaltest_list.append( (tiff_read_ycbcr_lzw) )
gdaltest_list.append( (tiff_read_ycbcr_int12) )

gdaltest_list.append( (tiff_read_unit_from_srs) )
gdaltest_list.append( (tiff_read_arcgis93_geodataxform_gcp) )
gdaltest_list.append( (tiff_read_block_width_above_32bit) )
gdaltest_list.append( (tiff_read_image_width_above_32bit) )
gdaltest_list.append( (tiff_read_second_image_width_above_32bit) )
gdaltest_list.append( (tiff_read_minimum_tiff_tags_no_warning) )
gdaltest_list.append( (tiff_read_minimum_tiff_tags_with_warning) )
gdaltest_list.append( (tiff_read_leak_ZIPSetupDecode) )
gdaltest_list.append( (tiff_read_excessive_memory_TIFFFillStrip) )
gdaltest_list.append( (tiff_read_excessive_memory_TIFFFillStrip2) )
gdaltest_list.append( (tiff_read_big_strip) )
gdaltest_list.append( (tiff_read_big_tile) )
gdaltest_list.append( (tiff_read_huge_number_strips) )
gdaltest_list.append( (tiff_read_many_blocks) )
gdaltest_list.append( (tiff_read_uint33) )
gdaltest_list.append( (tiff_read_corrupted_deflate_singlestrip) )
gdaltest_list.append( (tiff_read_packbits_not_enough_data) )
gdaltest_list.append( (tiff_read_toomanyblocks) )

gdaltest_list.append( (tiff_read_online_1) )
gdaltest_list.append( (tiff_read_online_2) )

# gdaltest_list = [ tiff_read_online_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
