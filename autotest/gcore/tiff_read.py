#!/usr/bin/env pytest
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
# Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
import struct

import pytest

import webserver

import gdaltest
from osgeo import gdal, osr

init_list = [
    ('byte.tif', 1, 4672),
    ('uint16_sgilog.tif', 1, 4672),
    ('next_literalrow.tif', 1, 4),
    ('next_literalspan.tif', 1, 4),
    ('next_default_case.tif', 1, 4),
    ('thunder.tif', 1, 3),
    ('int10.tif', 1, 4672),
    ('int12.tif', 1, 4672),
    ('int16.tif', 1, 4672),
    ('uint16.tif', 1, 4672),
    ('int24.tif', 1, 4672),
    ('int32.tif', 1, 4672),
    ('uint32.tif', 1, 4672),
    ('float16.tif', 1, 4672),
    ('float24.tif', 1, 4672),
    ('float32.tif', 1, 4672),
    ('float32_minwhite.tif', 1, 1),
    ('float64.tif', 1, 4672),
    ('cint16.tif', 1, 5028),
    ('cint32.tif', 1, 5028),
    ('cfloat32.tif', 1, 5028),
    ('cfloat64.tif', 1, 5028),
    # The following four related partial final strip/tiles (#1179)
    ('separate_tiled.tif', 2, 15234),
    ('seperate_strip.tif', 2, 15234),  # TODO: Spelling.
    ('contig_tiled.tif', 2, 15234),
    ('contig_strip.tif', 2, 15234),
    ('empty1bit.tif', 1, 0),
    ('gtiff/int64_full_range.tif', 1, 65535),
    ('gtiff/uint64_full_range.tif', 1, 1),
]


@pytest.mark.parametrize(
    'filename,band,checksum',
    init_list,
    ids=[tup[0].split('.')[0] for tup in init_list],
)
@pytest.mark.require_driver('GTiff')
def test_tiff_open(filename, band, checksum):
    ut = gdaltest.GDALTest('GTiff', filename, band, checksum)
    ut.testOpen()


###############################################################################
# Test absolute/offset && index directory access


def test_tiff_read_off():

    # Test absolute/offset directory access.
    ds = gdal.Open('GTIFF_DIR:off:408:data/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672

    # Same with GTIFF_RAW: prefix
    ds = gdal.Open('GTIFF_RAW:GTIFF_DIR:off:408:data/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672

    # Test index directory access
    ds = gdal.Open('GTIFF_DIR:1:data/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672

    # Check that georeferencing is read properly when accessing
    # "GTIFF_DIR" subdatasets (#3478)
    gt = ds.GetGeoTransform()
    assert gt == (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0), \
        'did not get expected geotransform'

    # Error cases
    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:1')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:1:')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:1:/vsimem/i_dont_exist.tif')
    assert ds is None

    # Requested directory not found
    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:2:data/byte.tif')
    assert ds is None

    # Opening a specific TIFF directory is not supported in update mode.
    # Switching to read-only
    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:1:data/byte.tif', gdal.GA_Update)
    assert ds is not None


###############################################################################
# Confirm we interpret bands as alpha when we should, and not when we
# should not.

def test_tiff_check_alpha():

    # Grey + alpha

    ds = gdal.Open('data/stefan_full_greyalpha.tif')

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_AlphaBand, \
        'Wrong color interpretation (stefan_full_greyalpha).'

    ds = None

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/stefan_full_greyalpha.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    gdaltest.supports_force_rgba = False
    if ds.RasterCount == 4:
        gdaltest.supports_force_rgba = True
    if gdaltest.supports_force_rgba:
        got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
        assert got_cs == [1970, 1970, 1970, 10807]
        ds = None

    # RGB + alpha

    ds = gdal.Open('data/stefan_full_rgba.tif')

    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand, \
        'Wrong color interpretation (stefan_full_rgba).'

    ds = None

    if gdaltest.supports_force_rgba:
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
        ds = gdal.Open('data/stefan_full_rgba.tif')
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
        got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
        # FIXME? Not the same as without GTIFF_FORCE_RGBA=YES
        assert got_cs == [11547, 57792, 35643, 10807]
        ds = None

    # RGB + undefined

    ds = gdal.Open('data/stefan_full_rgba_photometric_rgb.tif')

    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_Undefined, \
        'Wrong color interpretation (stefan_full_rgba_photometric_rgb).'

    ds = None

    if gdaltest.supports_force_rgba:
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
        ds = gdal.Open('data/stefan_full_rgba_photometric_rgb.tif')
        gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
        got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
        assert got_cs == [12603, 58561, 36064, 10807]
        ds = None



###############################################################################
# Test reading a CMYK tiff as RGBA image

def test_tiff_read_cmyk_rgba():

    ds = gdal.Open('data/rgbsmall_cmyk.tif')

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert 'SOURCE_COLOR_SPACE' in md and md['SOURCE_COLOR_SPACE'] == 'CMYK', \
        'bad value for IMAGE_STRUCTURE[SOURCE_COLOR_SPACE]'

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand, \
        'Wrong color interpretation.'

    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand, \
        'Wrong color interpretation (alpha).'

    assert ds.GetRasterBand(1).Checksum() == 23303, \
        ('Expected checksum = %d. Got = %d' % (23303, ds.GetRasterBand(1).Checksum()))

###############################################################################
# Test reading a CMYK tiff as a raw image


def test_tiff_read_cmyk_raw():

    ds = gdal.Open('GTIFF_RAW:data/rgbsmall_cmyk.tif')

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_CyanBand, \
        'Wrong color interpretation.'

    assert ds.GetRasterBand(1).Checksum() == 29430, \
        ('Expected checksum = %d. Got = %d' % (29430, ds.GetRasterBand(1).Checksum()))

###############################################################################
# Test reading a OJPEG image


def test_tiff_read_ojpeg():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/zackthecat.tif')
    gdal.PopErrorHandler()
    if ds is None:
        if gdal.GetLastErrorMsg().find('Cannot open TIFF file due to missing codec') == 0:
            pytest.skip()
        pytest.fail(gdal.GetLastErrorMsg())

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    got_cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    expected_cs = 61570
    assert got_cs == expected_cs, \
        ('Expected checksum = %d. Got = %d' % (expected_cs, got_cs))

    #
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/zackthecat_corrupted.tif')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if cs != 0:
        print('Should be 0 with internal libtiff')


###############################################################################
# Read a .tif.gz file


def test_tiff_read_gzip():

    try:
        os.remove('data/byte.tif.gz.properties')
    except OSError:
        pass

    ds = gdal.Open('/vsigzip/./data/byte.tif.gz')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

    try:
        os.stat('data/byte.tif.gz.properties')
        pytest.fail('did not expect data/byte.tif.gz.properties')
    except OSError:
        return

###############################################################################
# Read a .tif.zip file (with explicit filename)


def test_tiff_read_zip_1():

    ds = gdal.Open('/vsizip/./data/byte.tif.zip/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Read a .tif.zip file (with implicit filename)


def test_tiff_read_zip_2():

    ds = gdal.Open('/vsizip/./data/byte.tif.zip')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Read a .tif.zip file with a single file in a subdirectory (with explicit filename)


def test_tiff_read_zip_3():

    ds = gdal.Open('/vsizip/./data/onefileinsubdir.zip/onefileinsubdir/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Read a .tif.zip file with a single file in a subdirectory(with implicit filename)


def test_tiff_read_zip_4():

    ds = gdal.Open('/vsizip/./data/onefileinsubdir.zip')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Read a .tif.zip file with 2 files in a subdirectory


def test_tiff_read_zip_5():

    ds = gdal.Open('/vsizip/./data/twofileinsubdir.zip/twofileinsubdir/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Read a .tar file (with explicit filename)


def test_tiff_read_tar_1():

    ds = gdal.Open('/vsitar/./data/byte.tar/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Read a .tar file (with implicit filename)


def test_tiff_read_tar_2():

    ds = gdal.Open('/vsitar/./data/byte.tar')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Read a .tgz file (with explicit filename)


def test_tiff_read_tgz_1():

    ds = gdal.Open('/vsitar/./data/byte.tgz/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

    gdal.Unlink('data/byte.tgz.properties')

###############################################################################
# Read a .tgz file (with implicit filename)


def test_tiff_read_tgz_2():

    ds = gdal.Open('/vsitar/./data/byte.tgz')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

    gdal.Unlink('data/byte.tgz.properties')

###############################################################################
# Check handling of non-degree angular units (#601)


def test_tiff_grads():

    ds = gdal.Open('data/test_gf.tif')
    srs = ds.GetProjectionRef()

    assert srs.find('PARAMETER["latitude_of_origin",52]') != -1, \
        ('Did not get expected latitude of origin: wkt=%s' % srs)

###############################################################################
# Check Erdas Citation Parsing for coordinate system.


def test_tiff_citation():

    build_info = gdal.VersionInfo('BUILD_INFO')
    if build_info.find('ESRI_BUILD=YES') == -1:
        pytest.skip()

    ds = gdal.Open('data/citation_mixedcase.tif')
    wkt = ds.GetProjectionRef()

    expected_wkt = """PROJCS["NAD_1983_HARN_StatePlane_Oregon_North_FIPS_3601_Feet_Intl",GEOGCS["GCS_North_American_1983_HARN",DATUM["NAD83_High_Accuracy_Reference_Network",SPHEROID["GRS_1980",6378137.0,298.257222101]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["False_Easting",8202099.737532808],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-120.5],PARAMETER["Standard_Parallel_1",44.33333333333334],PARAMETER["Standard_Parallel_2",46.0],PARAMETER["Latitude_Of_Origin",43.66666666666666],UNIT["Foot",0.3048]]"""

    if wkt != expected_wkt:
        print('got: ', wkt)
        pytest.fail('Erdas citation processing failing?')


###############################################################################
# Check that we can read linear projection parameters properly (#3901)


def test_tiff_linearparmunits():

    # Test the file with the correct formulation.

    ds = gdal.Open('data/spaf27_correct.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference(wkt)

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    assert fe == pytest.approx(2000000.0, abs=0.001), 'did not get expected false easting (1)'

    # Test the file with the old (broken) GDAL formulation.

    ds = gdal.Open('data/spaf27_brokengdal.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference(wkt)

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    assert fe == pytest.approx(609601.219202438, abs=0.001), 'did not get expected false easting (2)'

    # Test the file when using an EPSG code.

    ds = gdal.Open('data/spaf27_epsg.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference(wkt)

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    assert fe == pytest.approx(2000000.0, abs=0.001), 'did not get expected false easting (3)'

###############################################################################
# Check that the GTIFF_LINEAR_UNITS handling works properly (#3901)


def test_tiff_linearparmunits2():

    gdal.SetConfigOption('GTIFF_LINEAR_UNITS', 'BROKEN')

    # Test the file with the correct formulation.

    ds = gdal.Open('data/spaf27_correct.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference(wkt)

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    assert fe == pytest.approx(6561666.66667, abs=0.001), 'did not get expected false easting (1)'

    # Test the file with the correct formulation that is marked as correct.

    ds = gdal.Open('data/spaf27_markedcorrect.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference(wkt)

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    assert fe == pytest.approx(2000000.0, abs=0.001), 'did not get expected false easting (2)'

    # Test the file with the old (broken) GDAL formulation.

    ds = gdal.Open('data/spaf27_brokengdal.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference(wkt)

    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    assert fe == pytest.approx(2000000.0, abs=0.001), 'did not get expected false easting (3)'

    gdal.SetConfigOption('GTIFF_LINEAR_UNITS', 'DEFAULT')

###############################################################################
# Test GTiffSplitBitmapBand to treat one row 1bit files as scanline blocks (#2622)


def test_tiff_g4_split():

    ds = gdal.Open('data/slim_g4.tif')

    (_, blocky) = ds.GetRasterBand(1).GetBlockSize()

    assert blocky == 1, 'Did not get scanline sized blocks.'

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 3322, 'Got wrong checksum'

###############################################################################
# Test reading a tiff with multiple images in it


def test_tiff_multi_images():

    # Implicitly get the content of the first image (backward compatibility).
    ds = gdal.Open('data/twoimages.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, ('Expected checksum = %d. Got = %d' %
              (4672, ds.GetRasterBand(1).Checksum()))

    md = ds.GetMetadata('SUBDATASETS')
    assert md['SUBDATASET_1_NAME'] == 'GTIFF_DIR:1:data/twoimages.tif', \
        'did not get expected subdatasets metadata.'

    ds = None

    # Explicitly get the content of the first image.
    ds = gdal.Open('GTIFF_DIR:1:data/twoimages.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

    # Explicitly get the content of the second image.
    ds = gdal.Open('GTIFF_DIR:2:data/twoimages.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds = None

###############################################################################
# Test reading a tiff from a memory buffer (#2931)


def test_tiff_vsimem():

    try:
        gdal.FileFromMemBuffer
    except AttributeError:
        pytest.skip()

    content = open('data/byte.tif', mode='rb').read()

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/tiffinmem', content)

    ds = gdal.Open('/vsimem/tiffinmem', gdal.GA_Update)
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open('/vsimem/tiffinmem')
    assert ds.GetRasterBand(1).Checksum() == 0, \
        ('Expected checksum = %d. Got = %d' % (0, ds.GetRasterBand(1).Checksum()))
    ds = None

    # Also test with anti-slash
    ds = gdal.Open('/vsimem\\tiffinmem')
    assert ds.GetRasterBand(1).Checksum() == 0, \
        ('Expected checksum = %d. Got = %d' % (0, ds.GetRasterBand(1).Checksum()))
    ds = None

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/tiffinmem')

###############################################################################
# Test reading a tiff from inside a zip in a memory buffer !


def test_tiff_vsizip_and_mem():

    try:
        gdal.FileFromMemBuffer
    except AttributeError:
        pytest.skip()

    content = open('data/byte.tif.zip', mode='rb').read()

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/tiffinmem.zip', content)

    ds = gdal.Open('/vsizip/vsimem/tiffinmem.zip/byte.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672, \
        ('Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum()))

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/tiffinmem.zip')

###############################################################################
# Test reading a GeoTIFF with only ProjectedCSTypeGeoKey defined (ticket #3019)


def test_tiff_ProjectedCSTypeGeoKey_only():

    ds = gdal.Open('data/ticket3019.tif')
    assert ds.GetProjectionRef().find('WGS 84 / UTM zone 31N') != -1
    ds = None

###############################################################################
# Test reading a GeoTIFF with only GTModelTypeGeoKey defined


def test_tiff_GTModelTypeGeoKey_only():

    ds = gdal.Open('data/GTModelTypeGeoKey_only.tif')
    assert ds.GetProjectionRef() in ('LOCAL_CS["unnamed"]', 'LOCAL_CS["unnamed",UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]')
    ds = None

###############################################################################
# Test reading a 12bit jpeg compressed geotiff.


@pytest.mark.skipif('SKIP_TIFF_JPEG12' in os.environ, reason='Crashes on build-windows-msys2-mingw')
def test_tiff_12bitjpeg():

    old_accum = gdal.GetConfigOption('CPL_ACCUM_ERROR_MSG', 'OFF')
    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'ON')
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    gdal.Unlink('data/mandrilmini_12bitjpeg.tif.aux.xml')

    try:
        ds = gdal.Open('data/mandrilmini_12bitjpeg.tif')
        ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    except:
        ds = None

    gdal.PopErrorHandler()
    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', old_accum)

    if gdal.GetLastErrorMsg().find('Unsupported JPEG data precision 12') != -1:
        pytest.skip('12bit jpeg not available')
    elif ds is None:
        pytest.fail('failed to open 12bit jpeg file with unexpected error')

    try:
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    except:
        pass

    assert not (stats[2] < 2150 or stats[2] > 2180 or str(stats[2]) == 'nan'), \
        'did not get expected mean for band1.'
    ds = None

    os.unlink('data/mandrilmini_12bitjpeg.tif.aux.xml')

###############################################################################
# Test that statistics for TIFF files are stored and correctly read from .aux.xml


def test_tiff_read_stats_from_pam():

    try:
        os.remove('data/byte.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/byte.tif')
    md = ds.GetRasterBand(1).GetMetadata()
    assert 'STATISTICS_MINIMUM' not in md, 'Unexpected presence of STATISTICS_MINIMUM'

    # Force statistics computation
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[0] == 74.0 and stats[1] == 255.0

    ds = None
    try:
        os.stat('data/byte.tif.aux.xml')
    except OSError:
        pytest.fail('Expected generation of data/byte.tif.aux.xml')

    ds = gdal.Open('data/byte.tif')
    # Just read statistics (from PAM) without forcing their computation
    stats = ds.GetRasterBand(1).GetStatistics(0, 0)
    assert stats[0] == 74.0 and stats[1] == 255.0
    ds = None

    try:
        os.remove('data/byte.tif.aux.xml')
    except OSError:
        pass


###############################################################################
# Test extracting georeferencing from a .TAB file


def test_tiff_read_from_tab():

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

    assert not os.path.exists('tmp/tiff_read_from_tab.tab')

    assert gt == (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0), \
        'did not get expected geotransform'

    assert '_1936' in wkt, 'did not get expected SRS'

###############################################################################
# Test reading PixelIsPoint file.


def test_tiff_read_pixelispoint():

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', 'FALSE')

    ds = gdal.Open('data/byte_point.tif')
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, 'did not get expected geotransform'

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', 'TRUE')

    ds = gdal.Open('data/byte_point.tif')
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    assert gt == gt_expected, \
        'did not get expected geotransform with GTIFF_POINT_GEO_IGNORE TRUE'

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', None)

###############################################################################
# Test reading a GeoTIFF file with a geomatrix in PixelIsPoint format.


def test_tiff_read_geomatrix():

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', 'FALSE')

    ds = gdal.Open('data/geomatrix.tif')
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (1841001.75, 1.5, -5.0, 1144003.25, -5.0, -1.5)

    assert gt == gt_expected, 'did not get expected geotransform'

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', 'TRUE')

    ds = gdal.Open('data/geomatrix.tif')
    gt = ds.GetGeoTransform()
    ds = None

    gt_expected = (1841000.0, 1.5, -5.0, 1144000.0, -5.0, -1.5)

    assert gt == gt_expected, \
        'did not get expected geotransform with GTIFF_POINT_GEO_IGNORE TRUE'

    gdal.SetConfigOption('GTIFF_POINT_GEO_IGNORE', None)

###############################################################################
# Test reading a GeoTIFF file with tiepoints in PixelIsPoint format.


def test_tiff_read_tiepoints_pixelispoint():

    ds = gdal.Open('data/byte_gcp_pixelispoint.tif')
    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Point'
    assert ds.GetGCPCount() == 4
    gcp = ds.GetGCPs()[0]
    assert (gcp.GCPPixel == pytest.approx(0.5, abs=1e-5) and \
       gcp.GCPLine == pytest.approx(0.5, abs=1e-5) and \
       gcp.GCPX == pytest.approx(-180, abs=1e-5) and \
       gcp.GCPY == pytest.approx(90, abs=1e-5) and \
       gcp.GCPZ == pytest.approx(0, abs=1e-5))

    with gdaltest.config_option('GTIFF_POINT_GEO_IGNORE', 'YES'):
        ds = gdal.Open('data/byte_gcp_pixelispoint.tif')
        assert ds.GetMetadataItem('AREA_OR_POINT') == 'Point'
        assert ds.GetGCPCount() == 4
        gcp = ds.GetGCPs()[0]
        assert (gcp.GCPPixel == pytest.approx(0, abs=1e-5) and \
        gcp.GCPLine == pytest.approx(0, abs=1e-5) and \
        gcp.GCPX == pytest.approx(-180, abs=1e-5) and \
        gcp.GCPY == pytest.approx(90, abs=1e-5) and \
        gcp.GCPZ == pytest.approx(0, abs=1e-5))

###############################################################################
# Test that we don't crash when reading a TIFF with corrupted GeoTIFF tags


def test_tiff_read_corrupted_gtiff():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/corrupted_gtiff_tags.tif')
    gdal.PopErrorHandler()
    del ds

    err_msg = gdal.GetLastErrorMsg()
    assert (not (err_msg.find('IO error during') == -1 and \
       err_msg.find('Error fetching data for field') == -1)), \
        'did not get expected error message'

###############################################################################
# Test that we don't crash when reading a TIFF with corrupted GeoTIFF tags


def test_tiff_read_tag_without_null_byte():

    gdal.ErrorReset()
    oldval = gdal.GetConfigOption('CPL_DEBUG')
    gdal.SetConfigOption('CPL_DEBUG', 'OFF')
    ds = gdal.Open('data/tag_without_null_byte.tif')
    gdal.SetConfigOption('CPL_DEBUG', oldval)
    assert gdal.GetLastErrorType() == 0, \
        'should have not emitted a warning, but only a CPLDebug() message'
    del ds


###############################################################################
# Test the effect of the GTIFF_IGNORE_READ_ERRORS configuration option (#3994)

def test_tiff_read_buggy_packbits():

    old_val = gdal.GetConfigOption('GTIFF_IGNORE_READ_ERRORS')
    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', None)
    ds = gdal.Open('data/byte_buggy_packbits.tif')
    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', old_val)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ReadRaster(0, 0, 20, 20)
    gdal.PopErrorHandler()
    assert ret is None, 'did not expected a valid result'
    ds = None

    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', 'YES')
    ds = gdal.Open('data/byte_buggy_packbits.tif')
    gdal.SetConfigOption('GTIFF_IGNORE_READ_ERRORS', old_val)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ReadRaster(0, 0, 20, 20)
    gdal.PopErrorHandler()
    assert ret is not None, 'expected a valid result'
    ds = None


###############################################################################
# Test reading a GeoEye _rpc.txt (#3639)

def test_tiff_read_rpc_txt():

    shutil.copy('data/byte.tif', 'tmp/test.tif')
    shutil.copy('data/test_rpc.txt', 'tmp/test_rpc.txt')
    ds = gdal.Open('tmp/test.tif')
    rpc_md = ds.GetMetadata('RPC')
    ds = None
    os.remove('tmp/test.tif')
    os.remove('tmp/test_rpc.txt')

    assert rpc_md['HEIGHT_OFF'] == '+0300.000 meters', \
        ('HEIGHT_OFF wrong:"' + rpc_md['HEIGHT_OFF'] + '"')

    assert (rpc_md['LINE_DEN_COEFF'].find(
            '+1.000000000000000E+00 -5.207696939454288E-03') == 0), \
        'LINE_DEN_COEFF wrong'

###############################################################################
# Test reading a TIFF with the RPC tag per
#  http://geotiff.maptools.org/rpc_prop.html


def test_tiff_read_rpc_tif():

    ds = gdal.Open('data/byte_rpc.tif')
    rpc_md = ds.GetMetadata('RPC')
    ds = None

    assert rpc_md['HEIGHT_OFF'] == '300', ('HEIGHT_OFF wrong:' + rpc_md['HEIGHT_OFF'])

    assert rpc_md['LINE_DEN_COEFF'].find('1 -0.00520769693945429') == 0, \
        'LINE_DEN_COEFF wrong'

###############################################################################
# Test a very small TIFF with only 4 tags :
# Magic: 0x4949 <little-endian> Version: 0x2a
# Directory 0: offset 8 (0x8) next 0 (0)
# ImageWidth (256) SHORT (3) 1<1>
# ImageLength (257) SHORT (3) 1<1>
# StripOffsets (273) LONG (4) 1<0>
# StripByteCounts (279) LONG (4) 1<1>


def test_tiff_small():

    content = '\x49\x49\x2A\x00\x08\x00\x00\x00\x04\x00\x00\x01\x03\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x01\x03\x00\x01\x00\x00\x00\x01\x00\x00\x00\x11\x01\x04\x00\x01\x00\x00\x00\x00\x00\x00\x00\x17\x01\x04\x00\x01\x00\x00\x00\x01\x00\x00\x00'

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/small.tif', content)

    ds = gdal.Open('/vsimem/small.tif')
    assert ds.GetRasterBand(1).Checksum() == 0, \
        ('Expected checksum = %d. Got = %d' % (0, ds.GetRasterBand(1).Checksum()))

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/small.tif')

###############################################################################
# Test that we can workaround a DoS with


def test_tiff_dos_strip_chop():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/tiff_dos_strip_chop.tif')
    gdal.PopErrorHandler()
    del ds

###############################################################################
# Test reading EXIF and GPS metadata


def test_tiff_read_exif_and_gps():

    ds = gdal.Open('data/exif_and_gps.tif')
    exif_md = ds.GetMetadata('EXIF')
    ds = None

    assert exif_md is not None and exif_md

    ds = gdal.Open('data/exif_and_gps.tif')
    EXIF_GPSVersionID = ds.GetMetadataItem('EXIF_GPSVersionID', 'EXIF')
    ds = None

    assert EXIF_GPSVersionID is not None

    # We should not get any EXIF metadata with that file
    ds = gdal.Open('data/byte.tif')
    exif_md = ds.GetMetadata('EXIF')
    ds = None

    assert (exif_md is None or not exif_md)

###############################################################################
# Test reading a pixel interleaved RGBA JPEG-compressed TIFF


def test_tiff_jpeg_rgba_pixel_interleaved():
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    ds = gdal.Open('data/stefan_full_rgba_jpeg_contig.tif')
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['INTERLEAVE'] == 'PIXEL'

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == expected_cs[i]

        assert ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == gdal.GCI_RedBand + i

    ds = None

###############################################################################
# Test reading a band interleaved RGBA JPEG-compressed TIFF


def test_tiff_jpeg_rgba_band_interleaved():
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    ds = gdal.Open('data/stefan_full_rgba_jpeg_separate.tif')
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['INTERLEAVE'] == 'BAND'

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == expected_cs[i]

        assert ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == gdal.GCI_RedBand + i

    ds = None

###############################################################################
# Test reading a YCbCr JPEG all-in-one-strip multiband TIFF (#3259, #3894)


def test_tiff_read_online_1():
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    if not gdaltest.download_file('http://trac.osgeo.org/gdal/raw-attachment/ticket/3259/imgpb17.tif', 'imgpb17.tif'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/imgpb17.tif')
    gdal.ErrorReset()
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert gdal.GetLastErrorMsg() == ''

    assert cs == 62628 or cs == 28554

###############################################################################
# Use GTIFF_DIRECT_IO=YES option combined with /vsicurl to test for multi-range
# support


def test_tiff_read_vsicurl_multirange():

    if gdal.GetDriverByName('HTTP') is None:
        pytest.skip()

    webserver_process = None
    webserver_port = 0

    (webserver_process, webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    try:
        filesize = 262976
        handler = webserver.SequentialHandler()
        handler.add('HEAD', '/utm.tif', 200, {'Content-Length': '%d' % filesize})
        def method(request):
            #sys.stderr.write('%s\n' % str(request.headers))

            if request.headers['Range'].startswith('bytes='):
                rng = request.headers['Range'][len('bytes='):]
                assert len(rng.split('-')) == 2
                start = int(rng.split('-')[0])
                end = int(rng.split('-')[1])

                request.protocol_version = 'HTTP/1.1'
                request.send_response(206)
                request.send_header('Content-type', 'application/octet-stream')
                request.send_header('Content-Range', 'bytes %d-%d/%d' % (start, end, filesize))
                request.send_header('Content-Length', end - start + 1)
                request.send_header('Connection', 'close')
                request.end_headers()
                with open('../gdrivers/data/utm.tif', 'rb') as f:
                    f.seek(start, 0)
                    request.wfile.write(f.read(end - start + 1))

        for i in range(6):
            handler.add('GET', '/utm.tif', custom_method=method)

        with webserver.install_http_handler(handler):
            with gdaltest.config_options({ 'GTIFF_DIRECT_IO': 'YES',
                                           'CPL_VSIL_CURL_ALLOWED_EXTENSIONS': '.tif',
                                           'GDAL_DISABLE_READDIR_ON_OPEN': 'EMPTY_DIR' }):
                ds = gdal.Open('/vsicurl/http://127.0.0.1:%d/utm.tif' % webserver_port)
                assert ds is not None, 'could not open dataset'

                # Read subsampled data
                subsampled_data = ds.ReadRaster(0, 0, 512, 32, 128, 4)
                ds = None

                ds = gdal.GetDriverByName('MEM').Create('', 128, 4)
                ds.WriteRaster(0, 0, 128, 4, subsampled_data)
                cs = ds.GetRasterBand(1).Checksum()
                ds = None

                assert cs == 6429, 'wrong checksum'

    finally:
        webserver.server_stop(webserver_process, webserver_port)

        gdal.VSICurlClearCache()

###############################################################################
# Test reading a TIFF made of a single-strip that is more than 2GB (#5403)


def test_tiff_read_huge4GB():

    if not gdaltest.filesystem_supports_sparse_files('tmp'):
        ds = gdal.Open('data/huge4GB.tif')
        assert ds is not None
    else:
        shutil.copy('data/huge4GB.tif', 'tmp/huge4GB.tif')
        f = open('tmp/huge4GB.tif', 'rb+')
        f.seek(65535 * 65535 + 401)
        f.write(' '.encode('ascii'))
        f.close()
        ds = gdal.Open('tmp/huge4GB.tif')
        if ds is None:
            os.remove('tmp/huge4GB.tif')
            pytest.fail()
        ds = None
        os.remove('tmp/huge4GB.tif')


###############################################################################
# Test reading a (small) BigTIFF. Tests GTiffCacheOffsetOrCount8()


def test_tiff_read_bigtiff():

    ds = gdal.Open('data/byte_bigtiff_strip5lines.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4672

###############################################################################
# Test reading in TIFF metadata domain


def test_tiff_read_tiff_metadata():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    ds = gdal.Open('data/stefan_full_rgba_jpeg_contig.tif')
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') == '254'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_0', 'TIFF') == '770'
    assert ds.GetRasterBand(1).GetMetadataItem('JPEGTABLES', 'TIFF').find('FFD8') == 0
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_100_0', 'TIFF') is None
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_100', 'TIFF') is None
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_100_0', 'TIFF') is None
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_100', 'TIFF') is None

    ds = gdal.Open('data/stefan_full_rgba_jpeg_separate.tif')
    assert ds.GetRasterBand(4).GetMetadataItem('BLOCK_OFFSET_0_2', 'TIFF') == '11071'
    assert ds.GetRasterBand(4).GetMetadataItem('BLOCK_SIZE_0_2', 'TIFF') == '188'

###############################################################################
# Test reading a JPEG-in-TIFF with tiles of irregular size (corrupted image)


def test_tiff_read_irregular_tile_size_jpeg_in_tiff():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    ds = gdal.Open('data/irregular_tile_size_jpeg_in_tiff.tif')
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetRasterBand(1).GetOverview(0).Checksum()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorType() != 0
    gdal.ErrorReset()

###############################################################################
# Test GTIFF_DIRECT_IO and GTIFF_VIRTUAL_MEM_IO optimizations


def test_tiff_direct_and_virtual_mem_io():

    # Test with pixel-interleaved and band-interleaved datasets
    for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_CInt16]:

        src_ds = gdal.Open('data/stefan_full_rgba.tif')
        dt_size = 1
        if dt == gdal.GDT_Int16:
            dt_size = 2
            mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, dt)
            data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, buf_type=dt)
            new_vals = []
            for i in range(4 * src_ds.RasterXSize * src_ds.RasterYSize):
                new_vals.append(chr(data[2 * i]).encode('latin1'))
                new_vals.append(chr(255 - data[2 * i]).encode('latin1'))
            data = b''.join(new_vals)
            mem_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data, buf_type=dt)
            src_ds = mem_ds
        elif dt == gdal.GDT_CInt16:
            dt_size = 4
            mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, dt)
            data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, buf_type=dt)
            new_vals = []
            for i in range(4 * src_ds.RasterXSize * src_ds.RasterYSize):
                new_vals.append(chr(data[4 * i]).encode('latin1'))
                new_vals.append(chr(data[4 * i]).encode('latin1'))
                new_vals.append(chr(255 - data[4 * i]).encode('latin1'))
                new_vals.append(chr(255 - data[4 * i]).encode('latin1'))
            data = b''.join(new_vals)
            mem_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data, buf_type=dt)
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
                            creation_options += ['ENDIANNESS=INVERTED']
                        out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options=creation_options)
                        out_ds.FlushCache()
                        out_ds = None
                    elif i == 1:
                        filename = '%s/tiff_direct_io_separate.tif' % prefix
                        out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options=['INTERLEAVE=BAND'])
                        out_ds.FlushCache()
                        out_ds = None
                    elif i == 2:
                        filename = '%s/tiff_direct_io_tiled_contig.tif' % prefix
                        creation_options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16']
                        if (dt == gdal.GDT_CInt16 or dt == gdal.GDT_Int16):
                            creation_options += ['ENDIANNESS=INVERTED']
                        if option == 'GTIFF_VIRTUAL_MEM_IO' and prefix == '/vsimem':
                            gdal.Translate(filename, src_ds, bandList=[1, 2, 3], creationOptions=creation_options)
                        else:
                            out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options=creation_options)
                            out_ds.FlushCache()
                            out_ds = None
                    elif i == 3:
                        filename = '%s/tiff_direct_io_tiled_separate.tif' % prefix
                        out_ds = gdal.GetDriverByName('GTiff').CreateCopy(filename, src_ds, options=['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16', 'INTERLEAVE=BAND'])
                        out_ds.FlushCache()
                        out_ds = None
                    elif i == 4:
                        filename = '%s/tiff_direct_io_sparse.tif' % prefix
                        out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options=['SPARSE_OK=YES'])
                        out_ds.FlushCache()
                        out_ds = None
                    elif i == 5:
                        filename = '%s/tiff_direct_io_sparse_separate.tif' % prefix
                        out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options=['SPARSE_OK=YES', 'INTERLEAVE=BAND'])
                        out_ds.FlushCache()
                        out_ds = None
                    elif i == 6:
                        filename = '%s/tiff_direct_io_sparse_tiled.tif' % prefix
                        out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options=['SPARSE_OK=YES', 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16'])
                        out_ds.FlushCache()
                        out_ds = None
                    else:
                        filename = '%s/tiff_direct_io_sparse_tiled_separate.tif' % prefix
                        out_ds = gdal.GetDriverByName('GTiff').Create(filename, 165, 150, 4, dt, options=['SPARSE_OK=YES', 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=16', 'INTERLEAVE=BAND'])
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
                            if(nxsize % nblockxsize) != 0:
                                padding += nblockxsize - (nxsize % nblockxsize)
                            padding *= dt_size
                            if not band_interleaved:
                                padding *= nbands
                            padding = int(padding)

                        to_remove = 1
                        if not band_interleaved:
                            to_remove += (nbands - 1) * dt_size

                        f = gdal.VSIFOpenL(filename, 'rb')
                        data = gdal.VSIFReadL(1, 1000000, f)
                        gdal.VSIFCloseL(f)
                        f = gdal.VSIFOpenL(filename, 'wb')
                        gdal.VSIFWriteL(data, 1, len(data) - padding - to_remove, f)
                        gdal.VSIFCloseL(f)

                    ds = gdal.Open(filename)
                    xoff = int(ds.RasterXSize / 4)
                    yoff = int(ds.RasterYSize / 4)
                    xsize = int(ds.RasterXSize / 2)
                    ysize = int(ds.RasterXSize / 2)
                    nbands = ds.RasterCount
                    sizeof_float = 4

                    if truncated:
                        gdal.PushErrorHandler()
                    ref_data_native_type = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize)
                    ref_data_native_type_whole = ds.GetRasterBand(1).ReadRaster()
                    ref_data_native_type_downsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2))
                    ref_data_native_type_downsampled_not_nearest = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2), resample_alg=gdal.GRIORA_Bilinear)
                    ref_data_native_type_upsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=nbands * xsize, buf_ysize=nbands * ysize)
                    ref_data_native_type_custom_spacings = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=nbands * dt_size)
                    ref_data_float32 = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_type=gdal.GDT_Float32)
                    ref_nbands_data_native_type = ds.ReadRaster(xoff, yoff, xsize, ysize)
                    ref_nbands_data_native_type_whole = ds.ReadRaster()
                    ref_nbands_data_native_type_downsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2))
                    ref_nbands_data_native_type_downsampled_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2), buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    ref_nbands_data_native_type_downsampled_not_nearest = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2), resample_alg=gdal.GRIORA_Bilinear)
                    ref_nbands_data_native_type_upsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=4 * xsize, buf_ysize=4 * ysize)
                    ref_nbands_data_native_type_downsampled_x_upsampled_y = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=32 * ysize)
                    ref_nbands_data_native_type_unordered_list = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list=[nbands - i for i in range(nbands)])
                    ref_nbands_data_native_type_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    ref_nbands_data_native_type_pixel_interleaved_whole = ds.ReadRaster(buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    ref_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list=[i + 1 for i in range(nbands - 1)], buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    ref_nbands_data_float32 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type=gdal.GDT_Float32)
                    ref_nbands_data_float32_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type=gdal.GDT_Float32, buf_pixel_space=nbands * sizeof_float, buf_band_space=1 * sizeof_float)
                    ref_nbands_data_native_type_custom_spacings = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=2 * nbands * dt_size, buf_band_space=dt_size)
                    if nbands == 3:
                        ref_nbands_data_native_type_custom_spacings_2 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=4 * dt_size, buf_band_space=dt_size)
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
                    got_data_native_type_downsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2))
                    got_data_native_type_downsampled_not_nearest = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2), resample_alg=gdal.GRIORA_Bilinear)
                    got_data_native_type_upsampled = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=nbands * xsize, buf_ysize=nbands * ysize)
                    got_data_native_type_custom_spacings = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=nbands * dt_size)
                    got_data_float32 = ds.GetRasterBand(1).ReadRaster(xoff, yoff, xsize, ysize, buf_type=gdal.GDT_Float32)
                    got_nbands_data_native_type = ds.ReadRaster(xoff, yoff, xsize, ysize)
                    got_nbands_data_native_type_whole = ds.ReadRaster()
                    got_nbands_data_native_type_bottom_right_downsampled = ds.ReadRaster(ds.RasterXSize - 2, ds.RasterYSize - 1, 2, 1, buf_xsize=1, buf_ysize=1, buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    got_nbands_data_native_type_downsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2))
                    got_nbands_data_native_type_downsampled_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2), buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    got_nbands_data_native_type_downsampled_not_nearest = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=int(ysize / 2), resample_alg=gdal.GRIORA_Bilinear)
                    got_nbands_data_native_type_upsampled = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=4 * xsize, buf_ysize=4 * ysize)
                    got_nbands_data_native_type_downsampled_x_upsampled_y = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_xsize=int(xsize / 2), buf_ysize=32 * ysize)
                    got_nbands_data_native_type_unordered_list = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list=[nbands - i for i in range(nbands)])
                    got_nbands_data_native_type_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    got_nbands_data_native_type_pixel_interleaved_whole = ds.ReadRaster(buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    got_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space = ds.ReadRaster(xoff, yoff, xsize, ysize, band_list=[i + 1 for i in range(nbands - 1)], buf_pixel_space=nbands * dt_size, buf_band_space=dt_size)
                    got_nbands_data_float32 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type=gdal.GDT_Float32)
                    got_nbands_data_float32_pixel_interleaved = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_type=gdal.GDT_Float32, buf_pixel_space=nbands * sizeof_float, buf_band_space=1 * sizeof_float)
                    got_nbands_data_native_type_custom_spacings = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=2 * nbands * dt_size, buf_band_space=dt_size)
                    if nbands == 3:
                        got_nbands_data_native_type_custom_spacings_2 = ds.ReadRaster(xoff, yoff, xsize, ysize, buf_pixel_space=4 * dt_size, buf_band_space=dt_size)
                    ds = None
                    gdal.SetConfigOption(option, old_val)
                    if truncated:
                        gdal.PopErrorHandler()

                    gdal.Unlink(filename)

                    if ref_data_native_type != got_data_native_type:
                        print(option)
                        pytest.fail(i)

                    if truncated and not band_interleaved:
                        if got_data_native_type_whole is not None:
                            print(truncated)
                            print(band_interleaved)
                            print(option)
                            print(i)
                            pytest.fail(gdal.GetDataTypeName(dt))
                    elif ref_data_native_type_whole != got_data_native_type_whole:
                        print(i)
                        pytest.fail(option)

                    if ref_data_native_type_downsampled != got_data_native_type_downsampled:
                        print(option)
                        pytest.fail(i)

                    if not truncated and ref_data_native_type_downsampled_not_nearest != got_data_native_type_downsampled_not_nearest:
                        print(band_interleaved)
                        print(option)
                        pytest.fail(i)

                    if ref_data_native_type_upsampled != got_data_native_type_upsampled:
                        print(option)
                        pytest.fail(i)

                    for y in range(ysize):
                        for x in range(xsize):
                            for k in range(dt_size):
                                if ref_data_native_type_custom_spacings[(y * xsize + x) * nbands * dt_size + k] != got_data_native_type_custom_spacings[(y * xsize + x) * nbands * dt_size + k]:
                                    print(gdal.GetDataTypeName(dt))
                                    print(option)
                                    pytest.fail(i)
                                if not truncated:
                                    for band in range(nbands):
                                        if ref_nbands_data_native_type_custom_spacings[(y * xsize + x) * 2 * nbands * dt_size + band * dt_size + k] != got_nbands_data_native_type_custom_spacings[(y * xsize + x) * 2 * nbands * dt_size + band * dt_size + k]:
                                            print(gdal.GetDataTypeName(dt))
                                            print(option)
                                            pytest.fail(i)
                                    if nbands == 3:
                                        for band in range(nbands):
                                            if ref_nbands_data_native_type_custom_spacings_2[(y * xsize + x) * 4 * dt_size + band * dt_size + k] != got_nbands_data_native_type_custom_spacings_2[(y * xsize + x) * 4 * dt_size + band * dt_size + k]:
                                                print(gdal.GetDataTypeName(dt))
                                                print(option)
                                                pytest.fail(i)

                    if ref_data_float32 != got_data_float32:
                        print(gdal.GetDataTypeName(dt))
                        print(option)
                        pytest.fail(i)

                    if not truncated and ref_nbands_data_native_type != got_nbands_data_native_type:
                        print(band_interleaved)
                        print(option)
                        pytest.fail(i)

                    if truncated:
                        if got_nbands_data_native_type_whole is not None:
                            print(gdal.GetDataTypeName(dt))
                            print(option)
                            pytest.fail(i)
                    elif ref_nbands_data_native_type_whole != got_nbands_data_native_type_whole:
                        print(option)
                        print(i)
                        pytest.fail(gdal.GetDataTypeName(dt))

                    if truncated:
                        if got_nbands_data_native_type_pixel_interleaved_whole is not None:
                            print(option)
                            pytest.fail(i)
                    elif ref_nbands_data_native_type_pixel_interleaved_whole != got_nbands_data_native_type_pixel_interleaved_whole:
                        print(i)
                        pytest.fail(option)

                    if truncated and got_nbands_data_native_type_bottom_right_downsampled is not None:
                        print(gdal.GetDataTypeName(dt))
                        print(option)
                        pytest.fail(i)

                    if truncated:
                        continue

                    if ref_nbands_data_native_type_downsampled != got_nbands_data_native_type_downsampled:
                        print(option)
                        pytest.fail(i)

                    if ref_nbands_data_native_type_downsampled_interleaved != got_nbands_data_native_type_downsampled_interleaved:
                        print(option)
                        pytest.fail(i)

                    if ref_nbands_data_native_type_downsampled_not_nearest != got_nbands_data_native_type_downsampled_not_nearest:
                        print(option)
                        pytest.fail(i)

                    if ref_nbands_data_native_type_upsampled != got_nbands_data_native_type_upsampled:
                        print(option)
                        # import struct
                        # f1 = open('out1.txt', 'wb')
                        # f2 = open('out2.txt', 'wb')
                        # for b in range(nbands):
                        #    for y in range(4 * ysize):
                        #        f1.write('%s\n' % str(struct.unpack('B' * 4 * xsize, ref_nbands_data_native_type_upsampled[(b * 4 * ysize + y) * 4 * xsize : (b * 4 * ysize + y + 1) * 4 * xsize])))
                        #        f2.write('%s\n' % str(struct.unpack('B' * 4 * xsize, got_nbands_data_native_type_upsampled[(b * 4 * ysize + y) * 4 * xsize : (b * 4 * ysize + y + 1) * 4 * xsize])))
                        pytest.fail(i)

                    if ref_nbands_data_native_type_downsampled_x_upsampled_y != got_nbands_data_native_type_downsampled_x_upsampled_y:
                        print(option)
                        # import struct
                        # f1 = open('out1.txt', 'wb')
                        # f2 = open('out2.txt', 'wb')
                        # for b in range(nbands):
                        #    for y in range(32 * ysize):
                        #        f1.write('%s\n' % str(struct.unpack('B' * int(xsize/2), ref_nbands_data_native_type_downsampled_x_upsampled_y[(b * 32 * ysize + y) * int(xsize/2) : (b * 32 * ysize + y + 1) * int(xsize/2)])))
                        #        f2.write('%s\n' % str(struct.unpack('B' * int(xsize/2), got_nbands_data_native_type_downsampled_x_upsampled_y[(b * 32 * ysize + y) * int(xsize/2) : (b * 32 * ysize + y + 1) * int(xsize/2)])))
                        pytest.fail(i)

                    if ref_nbands_data_native_type_unordered_list != got_nbands_data_native_type_unordered_list:
                        print(option)
                        pytest.fail(i)

                    if ref_nbands_data_native_type_pixel_interleaved != got_nbands_data_native_type_pixel_interleaved:
                        print(option)
                        pytest.fail(i)

                    for y in range(ysize):
                        for x in range(xsize):
                            for b in range(nbands - 1):
                                for k in range(dt_size):
                                    if ref_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space[((y * xsize + x) * nbands + b) * dt_size + k] != got_nbands_m_1_data_native_type_pixel_interleaved_with_extra_space[((y * xsize + x) * nbands + b) * dt_size + k]:
                                        print(option)
                                        pytest.fail(i)

                    if ref_nbands_data_float32 != got_nbands_data_float32:
                        print(option)
                        pytest.fail(i)

                    if ref_nbands_data_float32_pixel_interleaved != got_nbands_data_float32_pixel_interleaved:
                        print(option)
                        pytest.fail(i)

    ds = gdal.Open('data/byte.tif')  # any GTiff file will do
    unreached = ds.GetMetadataItem('UNREACHED_VIRTUALMEMIO_CODE_PATH', '_DEBUG_')
    ds = None
    if unreached:
        print('unreached = %s' % unreached)
        pytest.fail('missing code coverage in VirtualMemIO()')


###############################################################################
# Check read Digital Globe metadata IMD & RPB format


def test_tiff_read_md1():

    try:
        os.remove('data/md_dg.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_dg.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2010-04-01 12:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_dg.tif.aux.xml')

###############################################################################
# Test CPLKeywordParser on non-conformant .IMD files
# See https://github.com/OSGeo/gdal/issues/4037


def test_tiff_read_non_conformant_imd():

    gdal.FileFromMemBuffer('/vsimem/test.imd',
                           """BEGIN_GROUP = foo\n\tkey = value with space ' not quoted;\n\tkey2 = another one ;\r\nEND_GROUP\nEND\n""");
    gdal.FileFromMemBuffer('/vsimem/test.tif', open('data/byte.tif', 'rb').read())
    ds = gdal.Open('/vsimem/test.tif')
    md = ds.GetMetadata('IMD')
    gdal.Unlink('/vsimem/test.imd')
    gdal.Unlink('/vsimem/test.tif')
    assert md == { 'foo.key': "value with space ' not quoted",
                   'foo.key2': "another one" }

###############################################################################
# Check read Digital Globe metadata XML format


def test_tiff_read_md2():

    try:
        os.remove('data/md_dg_2.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_dg_2.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 2, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2011-05-01 13:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_dg_2.tif.aux.xml')


###############################################################################
# Check read GeoEye metadata format


def test_tiff_read_md3():

    try:
        os.remove('data/md_ge_rgb_0010000.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_ge_rgb_0010000.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2012-06-01 14:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_ge_rgb_0010000.tif.aux.xml')


###############################################################################
# Check read OrbView metadata format


def test_tiff_read_md4():

    try:
        os.remove('data/md_ov.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_ov.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2013-07-01 15:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_ov.tif.aux.xml')


###############################################################################
# Check read Resurs-DK1 metadata format


def test_tiff_read_md5():

    try:
        os.remove('data/md_rdk1.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_rdk1.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 2, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 5, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2014-08-01 16:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_rdk1.tif.aux.xml')


###############################################################################
# Check read Landsat metadata format


def test_tiff_read_md6():

    try:
        os.remove('data/md_ls_b1.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_ls_b1.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 2, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 5, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2015-09-01 17:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_ls_b1.tif.aux.xml')


###############################################################################
# Check read Spot metadata format


def test_tiff_read_md7():

    try:
        os.remove('data/spot/md_spot.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/spot/md_spot.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 2, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 5, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2001-03-01 00:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/spot/md_spot.tif.aux.xml')


###############################################################################
# Check read RapidEye metadata format


def test_tiff_read_md8():

    try:
        os.remove('data/md_re.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_re.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 2, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 5, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2010-02-01 12:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_re.tif.aux.xml')


###############################################################################
# Check read Alos metadata format


def test_tiff_read_md9():

    try:
        os.remove('data/alos/IMG-md_alos.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/alos/IMG-md_alos.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2010-07-01 00:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/alos/IMG-md_alos.tif.aux.xml')


###############################################################################
# Check read Eros metadata format


def test_tiff_read_md10():

    try:
        os.remove('data/md_eros.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_eros.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2013-04-01 11:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_eros.tif.aux.xml')


###############################################################################
# Check read Kompsat metadata format


def test_tiff_read_md11():

    try:
        os.remove('data/md_kompsat.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/md_kompsat.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2007-05-01 07:00:00', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    ds = None

    assert not os.path.exists('data/md_kompsat.tif.aux.xml')


###############################################################################
# Check read Dimap metadata format


def test_tiff_read_md12():

    ds = gdal.Open('../gdrivers/data/dimap2/single_component/IMG_foo_R2C1.TIF', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    metadata = ds.GetMetadataDomainList()
    assert len(metadata) == 6, 'did not get expected metadata list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    # Test UTC date
    assert md['ACQUISITIONDATETIME'] == '2016-06-17 12:34:56', \
        'bad value for IMAGERY[ACQUISITIONDATETIME]'

    # Test RPC and that we have a LINE_OFF shift
    rpc = ds.GetMetadata('RPC')
    assert rpc['LINE_OFF'] == '-11', 'RPC wrong.'

    ds = None

    assert not os.path.exists('data/md_kompsat.tif.aux.xml')

    # Test not valid DIMAP product [https://github.com/OSGeo/gdal/issues/431]
    shutil.copy('../gdrivers/data/dimap2/single_component/IMG_foo_R2C1.TIF', 'tmp/IMG_foo_temp.TIF')
    shutil.copy('../gdrivers/data/dimap2/single_component/DIM_foo.XML', 'tmp/DIM_foo.XML')
    shutil.copy('../gdrivers/data/dimap2/single_component/RPC_foo.XML', 'tmp/RPC_foo.XML')
    ds = gdal.Open('tmp/IMG_foo_temp.TIF', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()
    ds = None
    gdal.Unlink('tmp/IMG_foo_temp.TIF')
    gdal.Unlink('tmp/DIM_foo.XML')
    gdal.Unlink('tmp/RPC_foo.XML')

    assert len(filelist) <= 1, 'did not get expected file list.'

###############################################################################
# Test reading a TIFFTAG_GDAL_NODATA with empty text


def test_tiff_read_empty_nodata_tag():

    ds = gdal.Open('data/empty_nodata.tif')
    assert ds.GetRasterBand(1).GetNoDataValue() is None


###############################################################################
# Check that no auxiliary files are read with a simple Open(), reading
# imagery and getting IMAGE_STRUCTURE metadata
@pytest.mark.skipif(sys.platform != 'linux', reason='Incorrect platform')
def test_tiff_read_strace_check():

    python_exe = sys.executable
    cmd = "strace -f %s -c \"from osgeo import gdal; " % python_exe + (
        "gdal.SetConfigOption('CPL_DEBUG', 'OFF');"
        "ds = gdal.Open('../gcore/data/byte.tif');"
        "ds.ReadRaster();"
        "ds.GetMetadata('IMAGE_STRUCTURE');"
        "ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE');"
        " \" ")
    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd)
    except:
        # strace not available
        pytest.skip()

    lines_with_dotdot_gcore = []
    for line in err.split('\n'):
        if '../gcore' in line:
            lines_with_dotdot_gcore += [line]

    assert len(lines_with_dotdot_gcore) == 1

###############################################################################
# Test GDAL_READDIR_LIMIT_ON_OPEN


def test_tiff_read_readdir_limit_on_open():

    gdal.SetConfigOption('GDAL_READDIR_LIMIT_ON_OPEN', '1')

    ds = gdal.Open('data/md_kompsat.tif', gdal.GA_ReadOnly)
    filelist = ds.GetFileList()

    gdal.SetConfigOption('GDAL_READDIR_LIMIT_ON_OPEN', None)

    assert len(filelist) == 3, 'did not get expected file list.'

###############################################################################
#


def test_tiff_read_minisblack_as_rgba():

    if not gdaltest.supports_force_rgba:
        pytest.skip()

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/byte.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == [4672, 4672, 4672, 4873]
    ds = None

###############################################################################
#


def test_tiff_read_colortable_as_rgba():

    if not gdaltest.supports_force_rgba:
        pytest.skip()

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/test_average_palette.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == [2433, 2433, 2433, 4873]
    ds = None

###############################################################################
#


def test_tiff_read_logl_as_rgba():

    if not gdaltest.supports_force_rgba:
        pytest.skip()

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('data/uint16_sgilog.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    # I'm pretty sure this isn't the expected result...
    assert got_cs == [0, 0, 0, 4873]
    ds = None

###############################################################################
#


def test_tiff_read_strip_separate_as_rgba():

    if not gdaltest.supports_force_rgba:
        pytest.skip()

    # 3 band
    gdal.Translate('/vsimem/tiff_read_strip_separate_as_rgba.tif',
                   'data/rgbsmall.tif', options='-co INTERLEAVE=BAND')

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('/vsimem/tiff_read_strip_separate_as_rgba.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == [21212, 21053, 21349, 30658]
    ds = None

    gdal.Unlink('/vsimem/tiff_read_strip_separate_as_rgba.tif')

    # 3 band with PHOTOMETRIC_MINISBLACK to trigger gtStripSeparate() to
    # use the single band code path
    gdal.Translate('/vsimem/tiff_read_strip_separate_as_rgba.tif',
                   'data/rgbsmall.tif', options='-co INTERLEAVE=BAND -co PHOTOMETRIC=MINISBLACK')

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('/vsimem/tiff_read_strip_separate_as_rgba.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == [21212, 21212, 21212, 30658]
    ds = None

    gdal.Unlink('/vsimem/tiff_read_strip_separate_as_rgba.tif')

###############################################################################
#


def test_tiff_read_tiled_separate_as_rgba():

    if not gdaltest.supports_force_rgba:
        pytest.skip()

    # 3 band
    gdal.Translate('/vsimem/tiff_read_tiled_separate_as_rgba.tif',
                   'data/rgbsmall.tif', options='-co TILED=YES -co INTERLEAVE=BAND')

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('/vsimem/tiff_read_tiled_separate_as_rgba.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == [21212, 21053, 21349, 30658]
    ds = None

    gdal.Unlink('/vsimem/tiff_read_tiled_separate_as_rgba.tif')

    # Single band
    gdal.Translate('/vsimem/tiff_read_tiled_separate_as_rgba.tif',
                   'data/byte.tif', options='-co TILED=YES -co INTERLEAVE=BAND')

    gdal.SetConfigOption('GTIFF_FORCE_RGBA', 'YES')
    ds = gdal.Open('/vsimem/tiff_read_tiled_separate_as_rgba.tif')
    gdal.SetConfigOption('GTIFF_FORCE_RGBA', None)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == [4672, 4672, 4672, 4873]
    ds = None

    gdal.Unlink('/vsimem/tiff_read_tiled_separate_as_rgba.tif')

###############################################################################
#


def test_tiff_read_scanline_more_than_2GB():

    with gdaltest.error_handler():
        ds = gdal.Open('data/scanline_more_than_2GB.tif')
    if sys.maxsize > 2**32:
        assert ds is not None
    else:
        assert ds is None

###############################################################################
# Test that we are at least robust to wrong number of ExtraSamples and warn
# about it


def test_tiff_read_wrong_number_extrasamples():

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('data/6band_wrong_number_extrasamples.tif')
    assert gdal.GetLastErrorMsg().find('Wrong number of ExtraSamples') >= 0
    assert ds.GetRasterBand(6).GetRasterColorInterpretation() == gdal.GCI_AlphaBand

###############################################################################
# Test that we can read a one-trip TIFF without StripByteCounts tag


def test_tiff_read_one_strip_no_bytecount():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/one_strip_nobytecount.tif')
    gdal.PopErrorHandler()
    assert ds.GetRasterBand(1).Checksum() == 1

###############################################################################
# Test GDAL_GEOREF_SOURCES


def test_tiff_read_nogeoref():

    tests = [(None, True, True, False, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
             (None, True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
             (None, False, True, True, '_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
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
             ('TABFILE,WORLDFILE,INTERNAL', True, True, True, '_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
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
                print('Got ' + str(gt))
                print('Expected ' + str(expected_gt))
                pytest.fail('Iteration %d, did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration, config_option_value, str(copy_pam), str(copy_worldfile), str(copy_tabfile)))

            if expected_srs == 'LOCAL_CS["PAM"]' and srs_wkt == 'LOCAL_CS["PAM",UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]':
                pass # ok
            elif (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and expected_srs not in srs_wkt):
                print('Got ' + srs_wkt)
                print('Expected ' + expected_srs)
                pytest.fail('Iteration %d, did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration, config_option_value, str(copy_pam), str(copy_worldfile), str(copy_tabfile)))


###############################################################################
# Test GDAL_GEOREF_SOURCES


def test_tiff_read_inconsistent_georef():

    tests = [(None, True, True, True, 'LOCAL_CS["PAM"]', (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
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
             ('TABFILE', True, True, True, '_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
             ('TABFILE,PAM', True, True, True, '_1936', (400000.0, 25.0, 0.0, 1300000.0, 0.0, -25.0)),
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
                print('Got ' + str(gt))
                print('Expected ' + str(expected_gt))
                pytest.fail('Iteration %d, did not get expected gt for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration, config_option_value, str(copy_pam), str(copy_worldfile), str(copy_tabfile)))

            if expected_srs == 'LOCAL_CS["PAM"]' and srs_wkt == 'LOCAL_CS["PAM",UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]':
                pass # ok
            elif (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and expected_srs not in srs_wkt):
                print('Got ' + srs_wkt)
                print('Expected ' + expected_srs)
                pytest.fail('Iteration %d, did not get expected SRS for %s,copy_pam=%s,copy_worldfile=%s,copy_tabfile=%s' % (iteration, config_option_value, str(copy_pam), str(copy_worldfile), str(copy_tabfile)))


###############################################################################
# Test GDAL_GEOREF_SOURCES


def test_tiff_read_gcp_internal_and_auxxml():

    tests = [(None, True, 'LOCAL_CS["PAM"]', 1),
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
                open_options += ['GEOREF_SOURCES=' + config_option_value]
            ds = gdal.OpenEx('/vsimem/byte_gcp.tif', open_options=open_options)
            if iteration == 0:
                gcp_count = ds.GetGCPCount()
                srs_wkt = ds.GetGCPProjection()
            else:
                srs_wkt = ds.GetGCPProjection()
                gcp_count = ds.GetGCPCount()
            ds = None
            gdal.Unlink('/vsimem/byte_gcp.tif')
            gdal.Unlink('/vsimem/byte_gcp.tif.aux.xml')

            if gcp_count != expected_gcp_count:
                print('Got ' + str(gcp_count))
                print('Expected ' + str(expected_gcp_count))
                pytest.fail('Iteration %d, did not get expected gcp count for %s,copy_pam=%s' % (iteration, config_option_value, str(copy_pam)))

            if expected_srs == 'LOCAL_CS["PAM"]' and srs_wkt == 'LOCAL_CS["PAM",UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]':
                pass # ok
            elif (expected_srs == '' and srs_wkt != '') or (expected_srs != '' and expected_srs not in srs_wkt):
                print('Got ' + srs_wkt)
                print('Expected ' + expected_srs)
                pytest.fail('Iteration %d, did not get expected SRS for %s,copy_pam=%s' % (iteration, config_option_value, str(copy_pam)))


###############################################################################
# Test reading .tif + .aux


class myHandlerClass(object):
    def __init__(self):
        self.msg = None

    def handler(self, eErrClass, err_no, msg):
        # pylint: disable=unused-argument
        if 'File open of' in msg:
            self.msg = msg


def test_tiff_read_aux():

    gdal.ErrorReset()
    ds = gdal.Open('data/f2r23.tif')
    handler = myHandlerClass()
    gdal.PushErrorHandler(handler.handler)
    ds.GetFileList()
    gdal.PopErrorHandler()
    assert handler.msg is None, \
        ('Got message that indicate recursive calls: %s' % handler.msg)


def test_tiff_read_one_band_from_two_bands():

    gdal.Translate('/vsimem/tiff_read_one_band_from_two_bands.tif', 'data/byte.tif', options='-b 1 -b 1')
    gdal.Translate('/vsimem/tiff_read_one_band_from_two_bands_dst.tif', '/vsimem/tiff_read_one_band_from_two_bands.tif', options='-b 1')

    ds = gdal.Open('/vsimem/tiff_read_one_band_from_two_bands_dst.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.Unlink('/vsimem/tiff_read_one_band_from_two_bands.tif')
    gdal.Unlink('/vsimem/tiff_read_one_band_from_two_bands.tif.aux.xml')
    gdal.Unlink('/vsimem/tiff_read_one_band_from_two_bands_dst.tif')


def test_tiff_read_jpeg_cloud_optimized():

    for i in range(4):
        ds = gdal.Open('data/byte_ovr_jpeg_tablesmode%d.tif' % i)
        cs0 = ds.GetRasterBand(1).Checksum()
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs0 == 4743 and cs1 == 1133, i
        ds = None


# This one was generated with a buggy code that emit JpegTables with mode == 1
# when creating the overview directory but failed to properly set this mode while
# writing the imagery. libjpeg-6b emits a 'JPEGLib:Huffman table 0x00 was not defined'
# error while jpeg-8 works fine


def test_tiff_read_corrupted_jpeg_cloud_optimized():

    ds = gdal.Open('data/byte_ovr_jpeg_tablesmode_not_correctly_set_on_ovr.tif')
    cs0 = ds.GetRasterBand(1).Checksum()
    assert cs0 == 4743

    with gdaltest.error_handler():
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs1 == 0:
        print('Expected error while writing overview with libjpeg-6b')
    elif cs1 != 1133:
        pytest.fail(cs1)


###############################################################################
# Test reading YCbCr images with LZW compression


def test_tiff_read_ycbcr_lzw():

    tests = [('ycbcr_11_lzw.tif', 13459, 12939, 12414),
             ('ycbcr_12_lzw.tif', 13565, 13105, 12660),
             ('ycbcr_14_lzw.tif', 0, 0, 0),  # not supported
             ('ycbcr_21_lzw.tif', 13587, 13297, 12760),
             ('ycbcr_22_lzw.tif', 13393, 13137, 12656),
             ('ycbcr_24_lzw.tif', 0, 0, 0),  # not supported
             ('ycbcr_41_lzw.tif', 13218, 12758, 12592),
             ('ycbcr_42_lzw.tif', 13277, 12779, 12614),
             ('ycbcr_42_lzw_optimized.tif', 19918, 20120, 19087),
             ('ycbcr_44_lzw.tif', 12994, 13229, 12149),
             ('ycbcr_44_lzw_optimized.tif', 19666, 19860, 18836)]

    for (filename, cs1, cs2, cs3) in tests:
        ds = gdal.Open('data/' + filename)
        if cs1 == 0:
            gdal.PushErrorHandler()
        got_cs1 = ds.GetRasterBand(1).Checksum()
        got_cs2 = ds.GetRasterBand(2).Checksum()
        got_cs3 = ds.GetRasterBand(3).Checksum()
        if cs1 == 0:
            gdal.PopErrorHandler()
        assert got_cs1 == cs1 and got_cs2 == cs2 and got_cs3 == cs3, \
            (filename, got_cs1, got_cs2, got_cs3)


###############################################################################
# Test reading YCbCr images with nbits > 8


def test_tiff_read_ycbcr_int12():

    with gdaltest.error_handler():
        ds = gdal.Open('data/int12_ycbcr_contig.tif')
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot open TIFF file with') >= 0

###############################################################################
# Test reading band unit from VERT_CS unit (#6675)


def test_tiff_read_unit_from_srs():

    filename = '/vsimem/tiff_read_unit_from_srs.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('EPSG:4326+3855')
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open(filename)
    unit = ds.GetRasterBand(1).GetUnitType()
    assert unit == 'metre'
    ds = None

    gdal.Unlink(filename)

###############################################################################
# Test reading ArcGIS 9.3 .aux.xml


def test_tiff_read_arcgis93_geodataxform_gcp():

    ds = gdal.Open('data/arcgis93_geodataxform_gcp.tif')
    assert ds.GetGCPProjection().find('26712') >= 0
    assert ds.GetGCPCount() == 16
    gcp = ds.GetGCPs()[0]
    assert (gcp.GCPPixel == pytest.approx(565, abs=1e-5) and \
       gcp.GCPLine == pytest.approx(11041, abs=1e-5) and \
       gcp.GCPX == pytest.approx(500000, abs=1e-5) and \
       gcp.GCPY == pytest.approx(4705078.79016612, abs=1e-5) and \
       gcp.GCPZ == pytest.approx(0, abs=1e-5))

###############################################################################
# Test reading file with block size > signed int 32 bit


def test_tiff_read_block_width_above_32bit():

    with gdaltest.error_handler():
        ds = gdal.Open('data/block_width_above_32bit.tif')
    assert ds is None

###############################################################################
# Test reading file with image size > signed int 32 bit


def test_tiff_read_image_width_above_32bit():

    with gdaltest.error_handler():
        ds = gdal.Open('data/image_width_above_32bit.tif')
    assert ds is None

###############################################################################
# Test reading file with image size > signed int 32 bit


def test_tiff_read_second_image_width_above_32bit():

    ds = gdal.Open('data/second_image_width_above_32bit.tif')
    with gdaltest.error_handler():
        assert ds.GetMetadata("SUBDATASETS") == {}

    with gdaltest.error_handler():
        ds = gdal.Open('GTIFF_DIR:2:data/second_image_width_above_32bit.tif')
    assert ds is None

###############################################################################
# Test reading file with minimal number of warnings without warning


def test_tiff_read_minimum_tiff_tags_no_warning():

    gdal.ErrorReset()
    ds = gdal.Open('data/minimum_tiff_tags_no_warning.tif')
    assert gdal.GetLastErrorMsg() == ''
    ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ''

###############################################################################
# Test reading file with minimal number of warnings but warning


def test_tiff_read_minimum_tiff_tags_with_warning():

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('data/minimum_tiff_tags_with_warning.tif')
    assert gdal.GetLastErrorMsg() != ''
    gdal.ErrorReset()
    ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ''


###############################################################################

def check_libtiff_internal_or_at_least(expected_maj, expected_min, expected_micro):

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


def test_tiff_read_unknown_compression():

    with gdaltest.error_handler():
        ds = gdal.Open('data/unknown_compression.tif')
    assert ds is None

###############################################################################


def test_tiff_read_leak_ZIPSetupDecode():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/leak-ZIPSetupDecode.tif')
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).Checksum()


###############################################################################


def test_tiff_read_excessive_memory_TIFFFillStrip():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/excessive-memory-TIFFFillStrip.tif')
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).Checksum()


###############################################################################


def test_tiff_read_excessive_memory_TIFFFillStrip2():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/excessive-memory-TIFFFillStrip2.tif')
        ds.GetRasterBand(1).Checksum()


###############################################################################


def test_tiff_read_excessive_memory_TIFFFillTile():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/excessive-memory-TIFFFillTile.tif')
        ds.GetRasterBand(1).Checksum()


###############################################################################


def test_tiff_read_big_strip():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    gdal.Translate('/vsimem/test.tif', 'data/byte.tif', options='-co compress=lzw -outsize 10000 2000  -co blockysize=2000 -r bilinear -ot float32')
    if gdal.GetLastErrorMsg().find('cannot allocate') >= 0:
        pytest.skip()
    ds = gdal.Open('/vsimem/test.tif')
    assert ds.GetRasterBand(1).Checksum() == 2676
    ds = None
    gdal.Unlink('/vsimem/test.tif')

###############################################################################
# (Potentially) test libtiff CHUNKY_STRIP_READ_SUPPORT


def test_tiff_read_big_strip_chunky_way():

    gdal.Translate('/vsimem/test.tif', 'data/byte.tif', options='-co compress=lzw -outsize 1000 2001  -co blockysize=2001 -r bilinear')
    ds = gdal.Open('/vsimem/test.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 38337
    ds = None
    gdal.Unlink('/vsimem/test.tif')

###############################################################################


def test_tiff_read_big_tile():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    gdal.Translate('/vsimem/test.tif', 'data/byte.tif', options='-co compress=lzw -outsize 10000 2000 -co tiled=yes -co blockxsize=10000 -co blockysize=2000 -r bilinear -ot float32')
    if gdal.GetLastErrorMsg().find('cannot allocate') >= 0:
        pytest.skip()
    ds = gdal.Open('/vsimem/test.tif')
    assert ds.GetRasterBand(1).Checksum() == 2676
    ds = None
    gdal.Unlink('/vsimem/test.tif')

###############################################################################


def test_tiff_read_huge_tile():

    with gdaltest.error_handler():
        ds = gdal.Open('data/hugeblocksize.tif')
    assert ds is None

###############################################################################


def test_tiff_read_huge_number_strips():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/huge-number-strips.tif')
        ds.GetRasterBand(1).Checksum()


###############################################################################


def test_tiff_read_huge_implied_number_strips():

    if not check_libtiff_internal_or_at_least(4, 0, 10):
        pytest.skip()

    with gdaltest.error_handler():
        gdal.Open('data/huge-implied-number-strips.tif')


###############################################################################


def test_tiff_read_many_blocks():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        pytest.skip()

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/test.tif', 1, 2000000, options=['BLOCKYSIZE=1'])
    ds = None
    ds = gdal.Open('/vsimem/test.tif')
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None
    gdal.Unlink('/vsimem/test.tif')

###############################################################################


def test_tiff_read_many_blocks_truncated():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] != 'INTERNAL':
        pytest.skip()

    ds = gdal.Open('data/many_blocks_truncated.tif')
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_2000000', 'TIFF')
    assert gdal.GetLastErrorMsg() != ''

###############################################################################
# Test reading  images with nbits > 32


def test_tiff_read_uint33():

    with gdaltest.error_handler():
        ds = gdal.Open('data/uint33.tif')
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Unsupported TIFF configuration') >= 0

###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1545


def test_tiff_read_corrupted_deflate_singlestrip():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/corrupted_deflate_singlestrip.tif')
        ds.GetRasterBand(1).Checksum()


###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1563


def test_tiff_read_packbits_not_enough_data():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/packbits-not-enough-data.tif')
        ds.GetRasterBand(1).Checksum()


###############################################################################
# Test reading images with more than 2billion blocks for a single band


def test_tiff_read_toomanyblocks():

    with gdaltest.error_handler():
        ds = gdal.Open('data/toomanyblocks.tif')
    assert ds is None


###############################################################################
# Test reading images with more than 2billion blocks for all bands

def test_tiff_read_toomanyblocks_separate():

    with gdaltest.error_handler():
        ds = gdal.Open('data/toomanyblocks_separate.tif')
    assert ds is None

###############################################################################
# Test reading images where the number of items in StripByteCounts/StripOffsets
# tag is lesser than the number of strips


def test_tiff_read_size_of_stripbytecount_lower_than_stripcount():

    ds = gdal.Open('data/size_of_stripbytecount_lower_than_stripcount.tif')
    # There are 3 strips but StripByteCounts has just two elements;
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_1', 'TIFF') == '171'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_1', 'TIFF') == '1'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_2', 'TIFF') is None
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_2', 'TIFF') is None

    ds = gdal.Open('data/size_of_stripbytecount_at_1_and_lower_than_stripcount.tif')
    # There are 3 strips but StripByteCounts has just one element;
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_0', 'TIFF') == '1'


###############################################################################
# Test different datatypes for StripOffsets tag with little/big, classic/bigtiff

def test_tiff_read_stripoffset_types():

    tests = [
        ('data/classictiff_one_block_byte.tif', []),  # unsupported
        ('data/classictiff_one_block_long.tif', [158]),
        ('data/classictiff_one_block_be_long.tif', [158]),
        ('data/classictiff_one_strip_long.tif', [146]),
        ('data/classictiff_one_strip_be_long.tif', [146]),
        ('data/classictiff_two_strip_short.tif', [162, 163]),
        ('data/classictiff_two_strip_be_short.tif', [162, 163]),
        ('data/classictiff_four_strip_short.tif', [178, 179, 180, 181]),
        ('data/classictiff_four_strip_be_short.tif', [178, 179, 180, 181]),
        ('data/bigtiff_four_strip_short.tif', [316, 317, 318, 319]),
        ('data/bigtiff_four_strip_be_short.tif', [316, 317, 318, 319]),
        ('data/bigtiff_one_block_long8.tif', [272]),
        ('data/bigtiff_one_block_be_long8.tif', [272]),
        ('data/bigtiff_one_strip_long.tif', [252]),
        ('data/bigtiff_one_strip_be_long.tif', [252]),
        ('data/bigtiff_one_strip_long8.tif', [252]),
        ('data/bigtiff_one_strip_be_long8.tif', [252]),
        ('data/bigtiff_two_strip_long.tif', [284, 285]),
        ('data/bigtiff_two_strip_be_long.tif', [284, 285]),
        ('data/bigtiff_two_strip_long8.tif', [284, 285]),
        ('data/bigtiff_two_strip_be_long8.tif', [284, 285]),
    ]

    for (filename, expected_offsets) in tests:

        # Only when built against internal libtiff we reject byte datatype
        if not expected_offsets and \
           gdal.GetDriverByName('GTiff').GetMetadataItem('LIBTIFF') != 'INTERNAL':
            continue

        ds = gdal.Open(filename)
        offsets = []
        for row in range(4):
            with gdaltest.error_handler():
                mdi = ds.GetRasterBand(1).GetMetadataItem(
                    'BLOCK_OFFSET_0_%d' % row, 'TIFF')
            if mdi is None:
                break
            offsets.append(int(mdi))
        if offsets != expected_offsets:
            print(filename, expected_offsets, offsets)


###############################################################################
# Test reading a JPEG-in-TIFF file that contains the 2 denial of service
# vulnerabilities listed in
# http://www.libjpeg-turbo.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf


def test_tiff_read_progressive_jpeg_denial_of_service():

    if not check_libtiff_internal_or_at_least(4, 0, 9):
        pytest.skip()

    # Should error out with 'JPEGPreDecode:Reading this strip would require
    # libjpeg to allocate at least...'
    gdal.ErrorReset()
    with gdaltest.error_handler():
        os.environ['JPEGMEM'] = '10M'
        os.environ['LIBTIFF_JPEG_MAX_ALLOWED_SCAN_NUMBER'] = '1000'
        ds = gdal.Open('/vsizip/data/eofloop_valid_huff.tif.zip')
        del os.environ['LIBTIFF_JPEG_MAX_ALLOWED_SCAN_NUMBER']
        del os.environ['JPEGMEM']
        cs = ds.GetRasterBand(1).Checksum()
        assert cs == 0 and gdal.GetLastErrorMsg() != ''

    # Should error out with 'TIFFjpeg_progress_monitor:Scan number...
    gdal.ErrorReset()
    ds = gdal.Open('/vsizip/data/eofloop_valid_huff.tif.zip')
    with gdaltest.error_handler():
        os.environ['LIBTIFF_ALLOW_LARGE_LIBJPEG_MEM_ALLOC'] = 'YES'
        os.environ['LIBTIFF_JPEG_MAX_ALLOWED_SCAN_NUMBER'] = '10'
        cs = ds.GetRasterBand(1).Checksum()
        del os.environ['LIBTIFF_ALLOW_LARGE_LIBJPEG_MEM_ALLOC']
        del os.environ['LIBTIFF_JPEG_MAX_ALLOWED_SCAN_NUMBER']
        assert cs == 0 and gdal.GetLastErrorMsg() != ''


###############################################################################
# Test reading old-style LZW

def test_tiff_read_old_style_lzw():

    if not check_libtiff_internal_or_at_least(4, 0, 8):
        pytest.skip()

    ds = gdal.Open('data/quad-lzw-old-style.tif')
    # Shut down warning about old style LZW
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == 34282

###############################################################################
# Test libtiff mmap interface (actually not using mmap, but our /vsimem
# mmap emulation)


def test_tiff_read_mmap_interface():

    src_ds = gdal.Open('data/byte.tif')
    tmpfile = '/vsimem/tiff_read_mmap_interface.tif'
    for options in [[], ['TILED=YES'],
                    ['COMPRESS=LZW'], ['COMPRESS=LZW', 'TILED=YES']]:
        gdal.GetDriverByName('GTiff').CreateCopy(tmpfile,
                                                 src_ds,
                                                 options=options)
        gdal.SetConfigOption('GTIFF_USE_MMAP', 'YES')
        ds = gdal.Open(tmpfile)
        cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GTIFF_USE_MMAP', None)
        assert cs == 4672, (options, cs)

        f = gdal.VSIFOpenL(tmpfile, "rb")
        data = gdal.VSIFReadL(1, gdal.VSIStatL(tmpfile).size - 1, f)
        gdal.VSIFCloseL(f)
        f = gdal.VSIFOpenL(tmpfile, "wb")
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)
        gdal.SetConfigOption('GTIFF_USE_MMAP', 'YES')
        with gdaltest.error_handler():
            ds = gdal.Open(tmpfile)
            cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GTIFF_USE_MMAP', None)
        assert cs == 0, (options, cs)
        gdal.Unlink(tmpfile)

    gdal.Unlink(tmpfile)


###############################################################################
# Test reading JPEG compressed file whole last strip height is the full
# strip height, instead of just the number of lines needed to reach the
# image height.

def test_tiff_read_jpeg_too_big_last_stripe():

    if not check_libtiff_internal_or_at_least(4, 0, 9):
        pytest.skip()

    ds = gdal.Open('data/tif_jpeg_too_big_last_stripe.tif')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4557

    ds = gdal.Open('data/tif_jpeg_ycbcr_too_big_last_stripe.tif')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4557


###############################################################################
# Test reading GeoTIFF file with negative ScaleY in GeoPixelScale tag

def test_tiff_read_negative_scaley():

    ds = gdal.Open('data/negative_scaley.tif')
    with gdaltest.error_handler():
        assert ds.GetGeoTransform()[5] == -60

    ds = gdal.Open('data/negative_scaley.tif')
    with gdaltest.config_option('GTIFF_HONOUR_NEGATIVE_SCALEY', 'NO'):
        assert ds.GetGeoTransform()[5] == -60

    ds = gdal.Open('data/negative_scaley.tif')
    with gdaltest.config_option('GTIFF_HONOUR_NEGATIVE_SCALEY', 'YES'):
        assert ds.GetGeoTransform()[5] == 60


###############################################################################
# Test ZSTD compression


def test_tiff_read_zstd():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('ZSTD') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte_zstd.tif', 1, 4672)
    return ut.testOpen()

###############################################################################
# Test ZSTD compression


def test_tiff_read_zstd_corrupted():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('ZSTD') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte_zstd_corrupted.tif', 1, 0)
    with gdaltest.error_handler():
        return ut.testOpen()

###############################################################################
# Test ZSTD compression


def test_tiff_read_zstd_corrupted2():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('ZSTD') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte_zstd_corrupted2.tif', 1, 0)
    with gdaltest.error_handler():
        return ut.testOpen()


###############################################################################
# Test WEBP compression


def test_tiff_read_webp():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('WEBP') == -1:
        pytest.skip()
    stats = (0, 215, 66.38, 47.186)
    ut = gdaltest.GDALTest('GTiff', 'tif_webp.tif', 1, None)
    success = ut.testOpen(check_approx_stat=stats, stat_epsilon=1)
    gdal.Unlink('data/tif_webp.tif.aux.xml')
    return success

###############################################################################
# Test WEBP compression


def test_tiff_read_webp_huge_single_strip():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('WEBP') == -1:
        pytest.skip()
    ds = gdal.Open('data/tif_webp_huge_single_strip.tif')
    assert ds.GetRasterBand(1).Checksum() != 0

###############################################################################


def test_tiff_read_1bit_2bands():
    ds = gdal.Open('data/1bit_2bands.tif')
    cs = (ds.GetRasterBand(1).Checksum(), ds.GetRasterBand(2).Checksum())
    assert cs == (200, 824)

###############################################################################
# Test LERC compression


def test_tiff_read_lerc():

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LERC') == -1:
        pytest.skip()

    ut = gdaltest.GDALTest('GTiff', 'byte_lerc.tif', 1, 4672)
    return ut.testOpen()

###############################################################################


def test_tiff_read_overview_of_external_mask():

    filename = '/vsimem/tiff_read_overview_of_external_mask.tif'
    gdal.Translate(filename, 'data/byte.tif', options='-b 1 -mask 1')
    ds = gdal.Open(filename, gdal.GA_Update)
    ds.BuildOverviews('CUBIC', overviewlist=[2])
    ds = None
    ds = gdal.Open(filename + '.msk', gdal.GA_Update)
    ds.BuildOverviews('NEAREST', overviewlist=[2])
    ds = None
    ds = gdal.Open(filename)
    cs1 = ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
    cs2 = ds.GetRasterBand(1).GetMaskBand().GetOverview(0).Checksum()
    flags1 = ds.GetRasterBand(1).GetOverview(0).GetMaskFlags()
    ds = None

    gdal.Unlink(filename)
    gdal.Unlink(filename + '.msk')

    assert cs1 == cs2
    assert flags1 == gdal.GMF_PER_DATASET

###############################################################################
# Test reading GeoTIFF file ModelTiepointTag(z) != 0 and ModelPixelScaleTag(z) = 0
# Test https://issues.qgis.org/issues/20493

def test_tiff_read_ModelTiepointTag_z_non_zero_but_ModelPixelScaleTag_z_zero():

    ds = gdal.Open('data/ModelTiepointTag_z_non_zero_but_ModelPixelScaleTag_z_zero.tif')
    assert not ds.GetRasterBand(1).GetScale()
    assert not ds.GetRasterBand(1).GetOffset()


###############################################################################
# Test strip chopping on uncompressed fies with strips larger than 2 GB

def test_tiff_read_strip_larger_than_2GB():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    ds = gdal.Open('data/strip_larger_than_2GB_header.tif')
    assert ds
    assert ds.GetRasterBand(1).GetBlockSize() == [50000, 10737]
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') == '264'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_0', 'TIFF') == '536850000'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_1', 'TIFF') == '536850264'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_1', 'TIFF') == '536850000'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_5', 'TIFF') == '2684250264'
    assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_5', 'TIFF') == '65750000'

###############################################################################
# Test reading a deflate compressed file with a uncompressed strip larger than 4 GB

def test_tiff_read_deflate_4GB():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    ds = gdal.Open('/vsizip/data/test_deflate_4GB.tif.zip/test_deflate_4GB.tif')
    if sys.maxsize < 2**32:
        assert ds is None
        return
    assert ds is not None

    if not gdaltest.run_slow_tests():
        pytest.skip()

    data  = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buf_xsize = 20, buf_ysize = 20)
    ref_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    ref_ds.GetRasterBand(1).Fill(127)
    assert data == ref_ds.ReadRaster()

###############################################################################
# Check that our use of TileByteCounts is minimal for COG (only for last tile)
# and for interleaved mask that we also hardly use TileOffsets.

def test_tiff_read_cog_strile_arrays_zeroified_when_possible():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    # The file has been produced with:
    # gdal_translate ../autotest/gcore/data/rgba.tif -b 1 -b 2 -b 3 -mask 4 in.tif
    # gdal_translate in.tif cog.tif -co COPY_SRC_OVERVIEWS=YES -co COMPRESS=LZW -co TILED=YES -co BLOCKXSIZE=16 -co BLOCKYSIZE=16 --config GDAL_TIFF_INTERNAL_MASK YES
    # and then with an hex editor, zeroify all entries of TileByteCounts except the last tile for both IFDs
    # and zeroify all entries of TileOffsets for 2nd IFD (mask) except the last tile.

    with gdaltest.config_option('GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE', 'YES'):
        ds = gdal.Open('data/cog_strile_arrays_zeroified_when_possible.tif')
        cs = ds.GetRasterBand(1).Checksum()
        cs_mask = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 4873
    assert cs_mask == 1222

###############################################################################
# Check that our reading of a COG with /vsicurl is efficient

def test_tiff_read_cog_vsicurl():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    gdal.VSICurlClearCache()

    webserver_process = None
    webserver_port = 0

    (webserver_process, webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if webserver_port == 0:
        pytest.skip()

    in_filename = 'tmp/test_tiff_read_cog_vsicurl_in.tif'
    cog_filename = 'tmp/test_tiff_read_cog_vsicurl_out.tif'

    try:
        src_ds = gdal.GetDriverByName('GTIFF').Create(in_filename, 1024, 1024, options = ['BIGTIFF=YES', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16', 'SPARSE_OK=YES'])
        src_ds.BuildOverviews('NEAR', [256])
        gdal.GetDriverByName('GTIFF').CreateCopy(cog_filename, src_ds, options = ['BIGTIFF=YES', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16', 'COPY_SRC_OVERVIEWS=YES', 'COMPRESS=LZW'])

        filesize = gdal.VSIStatL(cog_filename).size

        handler = webserver.SequentialHandler()
        handler.add('HEAD', '/cog.tif', 200, {'Content-Length': '%d' % filesize})
        def method(request):
            #sys.stderr.write('%s\n' % request.headers['Range'])
            if request.headers['Range'] == 'bytes=0-16383':
                request.protocol_version = 'HTTP/1.1'
                request.send_response(200)
                request.send_header('Content-type', 'text/plain')
                request.send_header('Content-Range', 'bytes 0-16383/%d' % filesize)
                request.send_header('Content-Length', 16384)
                request.send_header('Connection', 'close')
                request.end_headers()
                request.wfile.write(open(cog_filename, 'rb').read(16384))
            else:
                request.send_response(404)
                request.send_header('Content-Length', 0)
                request.end_headers()
        handler.add('GET', '/cog.tif', custom_method=method)
        with webserver.install_http_handler(handler):
            ds = gdal.Open('/vsicurl/http://localhost:%d/cog.tif' % webserver_port)
        assert(ds)

        handler = webserver.SequentialHandler()
        def method(request):
            #sys.stderr.write('%s\n' % request.headers['Range'])
            if request.headers['Range'] == 'bytes=32768-49151':
                request.protocol_version = 'HTTP/1.1'
                request.send_response(200)
                request.send_header('Content-type', 'text/plain')
                request.send_header('Content-Range', 'bytes 32768-49151/%d' % filesize)
                request.send_header('Content-Length', 16384)
                request.send_header('Connection', 'close')
                request.end_headers()
                with open(cog_filename, 'rb') as f:
                    f.seek(32768, 0)
                    request.wfile.write(f.read(16384))
            else:
                request.send_response(404)
                request.send_header('Content-Length', 0)
                request.end_headers()
        handler.add('GET', '/cog.tif', custom_method=method)
        def method(request):
            #sys.stderr.write('%s\n' % request.headers['Range'])
            if request.headers['Range'] == 'bytes=180224-193497':
                request.protocol_version = 'HTTP/1.1'
                request.send_response(200)
                request.send_header('Content-type', 'text/plain')
                request.send_header('Content-Range', 'bytes 180224-193497/%d' % filesize)
                request.send_header('Content-Length', 13274)
                request.send_header('Connection', 'close')
                request.end_headers()
                with open(cog_filename, 'rb') as f:
                    f.seek(180224, 0)
                    request.wfile.write(f.read(13274))
            else:
                request.send_response(404)
                request.send_header('Content-Length', 0)
                request.end_headers()
        handler.add('GET', '/cog.tif', custom_method=method)
        with webserver.install_http_handler(handler):
            ret = ds.ReadRaster(1024 - 32,1024 - 32,16,16)
        assert ret

    finally:
        webserver.server_stop(webserver_process, webserver_port)

        gdal.VSICurlClearCache()

        gdal.GetDriverByName('GTIFF').Delete(in_filename)
        gdal.GetDriverByName('GTIFF').Delete(cog_filename)

###############################################################################
# Check that our reading of a COG with /vsicurl is efficient

def test_tiff_read_cog_with_mask_vsicurl():

    if not check_libtiff_internal_or_at_least(4, 0, 11):
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    gdal.VSICurlClearCache()

    webserver_process = None
    webserver_port = 0

    (webserver_process, webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if webserver_port == 0:
        pytest.skip()

    in_filename = 'tmp/test_tiff_read_cog_with_mask_vsicurl_in.tif'
    cog_filename = 'tmp/test_tiff_read_cog_with_mask_vsicurl_out.tif'

    try:
        src_ds = gdal.GetDriverByName('GTIFF').Create(in_filename, 1024, 1024, options = ['BIGTIFF=YES', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16', 'SPARSE_OK=YES'])
        src_ds.BuildOverviews('NEAR', [256])
        with gdaltest.config_options({'GDAL_TIFF_INTERNAL_MASK': 'YES', 'GDAL_TIFF_DEFLATE_SUBCODEC': 'ZLIB'}):
            src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
            gdal.GetDriverByName('GTIFF').CreateCopy(cog_filename, src_ds, options = ['BIGTIFF=YES', 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16', 'COPY_SRC_OVERVIEWS=YES', 'COMPRESS=LZW'])

        filesize = gdal.VSIStatL(cog_filename).size

        handler = webserver.SequentialHandler()
        handler.add('HEAD', '/cog.tif', 200, {'Content-Length': '%d' % filesize})
        def method(request):
            #sys.stderr.write('%s\n' % request.headers['Range'])
            if request.headers['Range'] == 'bytes=0-16383':
                request.protocol_version = 'HTTP/1.1'
                request.send_response(200)
                request.send_header('Content-type', 'text/plain')
                request.send_header('Content-Range', 'bytes 0-16383/%d' % filesize)
                request.send_header('Content-Length', 16384)
                request.send_header('Connection', 'close')
                request.end_headers()
                request.wfile.write(open(cog_filename, 'rb').read(16384))
            else:
                request.send_response(404)
                request.send_header('Content-Length', 0)
                request.end_headers()
        handler.add('GET', '/cog.tif', custom_method=method)
        with webserver.install_http_handler(handler):
            ds = gdal.Open('/vsicurl/http://localhost:%d/cog.tif' % webserver_port)
        assert(ds)

        handler = webserver.SequentialHandler()
        def method(request):
            #sys.stderr.write('%s\n' % request.headers['Range'])
            if request.headers['Range'] == 'bytes=32768-49151':
                request.protocol_version = 'HTTP/1.1'
                request.send_response(200)
                request.send_header('Content-type', 'text/plain')
                request.send_header('Content-Range', 'bytes 32768-49151/%d' % filesize)
                request.send_header('Content-Length', 16384)
                request.send_header('Connection', 'close')
                request.end_headers()
                with open(cog_filename, 'rb') as f:
                    f.seek(32768, 0)
                    request.wfile.write(f.read(16384))
            else:
                request.send_response(404)
                request.send_header('Content-Length', 0)
                request.end_headers()
        handler.add('GET', '/cog.tif', custom_method=method)
        def method(request):
            #sys.stderr.write('%s\n' % request.headers['Range'])
            if request.headers['Range'] == 'bytes=294912-311295':
                request.protocol_version = 'HTTP/1.1'
                request.send_response(200)
                request.send_header('Content-type', 'text/plain')
                request.send_header('Content-Range', 'bytes 294912-311295/%d' % filesize)
                request.send_header('Content-Length', 32768)
                request.send_header('Connection', 'close')
                request.end_headers()
                with open(cog_filename, 'rb') as f:
                    f.seek(294912, 0)
                    request.wfile.write(f.read(32768))
            else:
                request.send_response(404)
                request.send_header('Content-Length', 0)
                request.end_headers()
        handler.add('GET', '/cog.tif', custom_method=method)
        with webserver.install_http_handler(handler):
            ret = ds.ReadRaster(1024 - 32,1024 - 32,16,16)
        assert ret

        ret = ds.GetRasterBand(1).GetMaskBand().ReadRaster(1024 - 32,1024 - 32,16,16)
        assert ret

    finally:
        webserver.server_stop(webserver_process, webserver_port)

        gdal.VSICurlClearCache()

        gdal.GetDriverByName('GTIFF').Delete(in_filename)
        gdal.GetDriverByName('GTIFF').Delete(cog_filename)

###############################################################################
# Check that GetMetadataDomainList() works properly


def test_tiff_GetMetadataDomainList():

    ds = gdal.Open('data/byte.tif')
    mdd1_set = set([x for x in ds.GetMetadataDomainList()])
    assert mdd1_set == set(['', 'DERIVED_SUBDATASETS', 'IMAGE_STRUCTURE'])
    mdd2_set = set([x for x in ds.GetMetadataDomainList()])
    assert mdd1_set == mdd2_set


###############################################################################
# Test reading a file with SLONG8 data type for StripOffsets


def test_tiff_read_bigtiff_invalid_slong8_for_stripoffsets():

    if not check_libtiff_internal_or_at_least(4, 1, 1):
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/byte_bigtiff_invalid_slong8_for_stripoffsets.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672


###############################################################################
# Test reading a file with a single band, and WhitePoint and PrimaryChromaticities
# tags


def test_tiff_read_tiff_single_band_with_whitepoint_primarychroma_tags():

    ds = gdal.Open('data/tiff_single_band_with_whitepoint_primarychroma_tags.tif')
    # Check that it doesn't crash. We could perhaps return something more
    # useful
    assert ds.GetMetadata('COLOR_PROFILE') == {}


###############################################################################
# Test that subdataset names for Geodetic TIFF grids (GTG)
# (https://proj.org/specifications/geodetictiffgrids.html)
# include the grid_name


def test_tiff_read_geodetic_tiff_grid():

    ds = gdal.Open('data/test_hgrid_with_subgrid.tif')
    assert ds.GetSubDatasets()[0][1] == 'Page 1 (10P x 10L x 2B): CAwest'



###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/2903
# related to precomposed vs decomposed UTF-8 filenames on MacOSX

def test_tiff_read_utf8_encoding_issue_2903():

    if gdaltest.is_travis_branch('mingw_w64'):
        pytest.skip()

    precomposed_utf8 = b'\xc3\xa4'.decode('utf-8')
    tmp_tif_filename = 'tmp/%s.tif' % precomposed_utf8
    tmp_tfw_filename = 'tmp/%s.tfw' % precomposed_utf8
    open(tmp_tif_filename, 'wb').write(open('data/byte_nogeoref.tif', 'rb').read())
    open(tmp_tfw_filename, 'wb').write(open('data/byte_nogeoref.tfw', 'rb').read())
    ds = gdal.Open(tmp_tif_filename)
    assert ds.GetGeoTransform()[0] != 0
    ds = None
    os.unlink(tmp_tif_filename)
    os.unlink(tmp_tfw_filename)

###############################################################################
# Check over precision issue with nodata and Float32 (#3791)


def test_tiff_read_overprecision_nodata_float32():

    filename = '/vsimem/test_tiff_read_overprecision_nodata_float32.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1, 1, gdal.GDT_Float32)
    ds.GetRasterBand(1).SetNoDataValue(-3.4e38)
    ds.GetRasterBand(1).Fill(-3.4e38)
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == struct.unpack('f', struct.pack('f', -3.4e38))[0]
    assert struct.unpack('f', ds.GetRasterBand(1).ReadRaster())[0] == ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test reading a file with a unhandled codec of a known name


def test_tiff_read_unhandled_codec_known_name():

    gdal.ErrorReset()
    with gdaltest.error_handler():
        assert gdal.Open('data/gtiff/unsupported_codec_jp2000.tif') is None
    assert 'missing codec JP2000' in gdal.GetLastErrorMsg()


###############################################################################
# Test reading a file with a unhandled codec of a unknown name


def test_tiff_read_unhandled_codec_unknown_name():

    gdal.ErrorReset()
    with gdaltest.error_handler():
        assert gdal.Open('data/gtiff/unsupported_codec_unknown.tif') is None
    assert 'missing codec of code 44510' in gdal.GetLastErrorMsg()
