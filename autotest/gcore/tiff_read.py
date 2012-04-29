#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a TIFF file.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
import string
import shutil

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal, osr

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

init_list = [ \
    ('byte.tif', 1, 4672, None),
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
    ('seperate_strip.tif', 2, 15234, None),
    ('contig_tiled.tif', 2, 15234, None),
    ('contig_strip.tif', 2, 15234, None),
    ('empty1bit.tif', 1, 0, None)]

###############################################################################
# Test absolute/offset && index directory access

def tiff_read_off():

    # Test absolute/offset directory access 
    ds = gdal.Open('GTIFF_DIR:off:408:data/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'

    # Test index directory access
    ds = gdal.Open('GTIFF_DIR:1:data/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'

    # Check that georeferencing is read properly when accessing "GTIFF_DIR" subdatasets (#3478)
    gt = ds.GetGeoTransform()
    if gt != (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0):
        gdaltest.post_reason('did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'


###############################################################################
# Confirm we interprete bands as alpha when we should, and not when we
# should not.

def tiff_check_alpha():

    # Grey + alpha
    
    ds = gdal.Open('data/stefan_full_greyalpha.tif')

    if ds.GetRasterBand(2).GetRasterColorInterpretation()!= gdal.GCI_AlphaBand:
        gdaltest.post_reason( 'Wrong color interpretation (stefan_full_greyalpha).')
        print(ds.GetRasterBand(2).GetRasterColorInterpretation())
        return 'fail'

    ds = None

    # RGB + alpha
    
    ds = gdal.Open('data/stefan_full_rgba.tif')

    if ds.GetRasterBand(4).GetRasterColorInterpretation()!= gdal.GCI_AlphaBand:
        gdaltest.post_reason( 'Wrong color interpretation (stefan_full_rgba).')
        print(ds.GetRasterBand(4).GetRasterColorInterpretation())
        return 'fail'

    ds = None

    # RGB + undefined
    
    ds = gdal.Open('data/stefan_full_rgba_photometric_rgb.tif')

    if ds.GetRasterBand(4).GetRasterColorInterpretation()!= gdal.GCI_Undefined:
        gdaltest.post_reason( 'Wrong color interpretation (stefan_full_rgba_photometric_rgb).')
        print(ds.GetRasterBand(4).GetRasterColorInterpretation())
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

    # Implicitely get the content of the first image (backward compatibility)
    ds = gdal.Open('data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'

    md = ds.GetMetadata('SUBDATASETS')
    if md['SUBDATASET_1_NAME'] != 'GTIFF_DIR:1:data/twoimages.tif':
        print(md)
        gdaltest.post_reason( 'did not get expected subdatasets metadata.' )
        return 'fail'
    
    ds = None

    # Explicitely get the content of the first image
    ds = gdal.Open('GTIFF_DIR:1:data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
            return 'fail'
    ds = None

    # Explicitely get the content of the second image
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
    
    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

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
    
    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

    return 'success'

###############################################################################
# Test that we don't crash when reading a TIFF with corrupted GeoTIFF tags

def tiff_read_corrupted_gtiff():

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('data/corrupted_gtiff_tags.tif')
    gdal.PopErrorHandler()
    ds = None

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

    ds = gdal.Open('data/tag_without_null_byte.tif')
    if gdal.GetLastErrorType() != 0:
        gdaltest.post_reason( 'should have not emitted a warning, but only a CPLDebug() message' )
        return 'fail'
    ds = None

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

    if not 'HEIGHT_OFF' in rpc_md:
        return 'fail'

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
    
    if cs != 62628:
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

    gdal.SetConfigOption('GTIFF_DIRECT_IO', 'YES')
    gdal.SetConfigOption('CPL_VSIL_CURL_ALLOWED_EXTENSIONS', '.tif')
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'EMPTY_DIR')
    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/gtiff/utm.tif')
    gdal.SetConfigOption('GTIFF_DIRECT_IO', None)
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

###############################################################################################

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
gdaltest_list.append( (tiff_12bitjpeg) )
gdaltest_list.append( (tiff_read_stats_from_pam) )
gdaltest_list.append( (tiff_read_from_tab) )
gdaltest_list.append( (tiff_read_pixelispoint) )
gdaltest_list.append( (tiff_read_geomatrix) )
gdaltest_list.append( (tiff_read_corrupted_gtiff) )
gdaltest_list.append( (tiff_read_tag_without_null_byte) )
gdaltest_list.append( (tiff_read_buggy_packbits) )
gdaltest_list.append( (tiff_read_rpc_txt) )
gdaltest_list.append( (tiff_read_online_1) )
gdaltest_list.append( (tiff_read_online_2) )

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

