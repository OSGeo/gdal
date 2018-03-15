#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GRIB driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
import struct
import shutil
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )
sys.path.append( '../osr' )

import gdaltest

def has_jp2kdrv():
    for i in range(gdal.GetDriverCount()):
        if gdal.GetDriver(i).ShortName.startswith('JP2'):
            return True
    return False

###############################################################################
# Do a simple checksum on our test file

def grib_1():

    gdaltest.grib_drv = gdal.GetDriverByName('GRIB')
    if gdaltest.grib_drv is None:
        return 'skip'

    # Test proj4 presence
    import osr_ct
    osr_ct.osr_ct_1()

    tst = gdaltest.GDALTest( 'GRIB', 'grib/ds.mint.bin', 2, 46927 )
    return tst.testOpen()


###############################################################################
# Test a small GRIB 1 sample file.

def grib_2():

    if gdaltest.grib_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'GRIB', 'grib/Sample_QuikSCAT.grb', 4, 50714 )
    return tst.testOpen()

###############################################################################
# This file has different raster sizes for some of the products, which
# we sort-of-support per ticket Test a small GRIB 1 sample file.

def grib_read_different_sizes_messages():

    if gdaltest.grib_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'GRIB', 'grib/bug3246.grb', 4, 4081 )
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = tst.testOpen()
    gdal.PopErrorHandler()

    msg = gdal.GetLastErrorMsg()
    if msg.find('data access may be incomplete') == -1 \
       or gdal.GetLastErrorType() != 2:
        gdaltest.post_reason( 'did not get expected warning.' )

    return result

###############################################################################
# Check nodata

def grib_grib2_read_nodata():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/grib/ds.mint.bin')
    if ds.GetRasterBand(1).GetNoDataValue() != 9999:
        return 'fail'
    if ds.GetRasterBand(2).GetNoDataValue() != 9999:
        return 'fail'
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_REF_TIME': '  1203613200 sec UTC', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES': '0 5 2 0 0 255 255 1 19 1 0 0 255 -1 -2147483647 2008 2 22 12 0 0 1 0 3 255 1 12 1 0', 'GRIB_VALID_TIME': '  1203681600 sec UTC', 'GRIB_FORECAST_SECONDS': '68400 sec', 'GRIB_UNIT': '[C]', 'GRIB_PDS_TEMPLATE_NUMBERS': '0 5 2 0 0 0 255 255 1 0 0 0 19 1 0 0 0 0 0 255 129 255 255 255 255 7 216 2 22 12 0 0 1 0 0 0 0 3 255 1 0 0 0 12 1 0 0 0 0', 'GRIB_DISCIPLINE': '0(Meteorological)', 'GRIB_PDS_PDTN': '8', 'GRIB_COMMENT': 'Minimum temperature [C]', 'GRIB_SHORT_NAME': '0-SFC', 'GRIB_ELEMENT': 'MinT'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'

    return 'success'

###############################################################################
# Check grib units (#3606)

def grib_read_units():

    if gdaltest.grib_drv is None:
        return 'skip'

    try:
        os.unlink('tmp/ds.mint.bin.aux.xml')
    except:
        pass

    shutil.copy('data/grib/ds.mint.bin', 'tmp/ds.mint.bin')
    ds = gdal.Open('tmp/ds.mint.bin')
    md = ds.GetRasterBand(1).GetMetadata()
    if md['GRIB_UNIT'] != '[C]' or md['GRIB_COMMENT'] != 'Minimum temperature [C]':
        gdaltest.post_reason('fail')
        print(md)
        return 'success'
    ds.GetRasterBand(1).ComputeStatistics(False)
    if abs(ds.GetRasterBand(1).GetMinimum() - 13) > 1:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMinimum())
        return 'success'
    ds = None

    os.unlink('tmp/ds.mint.bin.aux.xml')

    gdal.SetConfigOption('GRIB_NORMALIZE_UNITS', 'NO')
    ds = gdal.Open('tmp/ds.mint.bin')
    gdal.SetConfigOption('GRIB_NORMALIZE_UNITS', None)
    md = ds.GetRasterBand(1).GetMetadata()
    if md['GRIB_UNIT'] != '[K]' or md['GRIB_COMMENT'] != 'Minimum temperature [K]':
        gdaltest.post_reason('fail')
        print(md)
        return 'success'
    ds.GetRasterBand(1).ComputeStatistics(False)
    if abs(ds.GetRasterBand(1).GetMinimum() - 286) > 1:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMinimum())
        return 'success'
    ds = None

    gdal.GetDriverByName('GRIB').Delete('tmp/ds.mint.bin')

    return 'success'

###############################################################################
# Handle geotransform for 1xn or nx1 grids.  The geotransform was faulty when
# grib files had one cell in either direction for geographic projections.  See
# ticket #5532

def grib_read_geotransform_one_n_or_n_one():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/grib/one_one.grib2')
    egt = (-114.25, 0.5, 0.0, 47.250, 0.0, -0.5)
    gt = ds.GetGeoTransform()
    ds = None
    if gt != egt:
        print(gt, '!=', egt)
        gdaltest.post_reason('Invalid geotransform')
        return 'fail'
    return 'success'

###############################################################################
# This is more a /vsizip/ file test than a GRIB one, but could not easily
# come up with a pure /vsizip/ test case, so here's a real world use
# case (#5530).

def grib_read_vsizip():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('/vsizip/data/grib/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Write PDS numbers to all bands

def grib_grib2_test_grib_pds_all_bands():

    if gdaltest.grib_drv is None:
        return 'skip'
    ds = gdal.Open('/vsizip/data/grib/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'fail'
    band = ds.GetRasterBand(2)
    md = band.GetMetadataItem('GRIB_PDS_TEMPLATE_NUMBERS')
    ds = None
    if md is None:
        gdaltest.post_reason('Failed to fetch pds numbers (#5144)')
        return 'fail'

    gdal.SetConfigOption('GRIB_PDS_ALL_BANDS', 'OFF')
    ds = gdal.Open('/vsizip/data/grib/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'fail'
    band = ds.GetRasterBand(2)
    md = band.GetMetadataItem('GRIB_PDS_TEMPLATE_NUMBERS')
    ds = None

    if md is not None:
        gdaltest.post_reason('Got pds numbers, when disabled (#5144)')
        return 'fail'
    return 'success'

###############################################################################
# Test support for template 4.15 (#5768)

def grib_grib2_read_template_4_15():

    if gdaltest.grib_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret, err = gdaltest.runexternal_out_and_err (test_cli_utilities.get_gdalinfo_path() + ' data/grib/template_4_15.grb2 -checksum')

    # This is a JPEG2000 compressed file, so just check we can open it or that we get a message saying there's no JPEG2000 driver available
    if ret.find('Checksum=') < 0 and err.find('Is the JPEG2000 driver available?') < 0:
        gdaltest.post_reason('Could not open file')
        print(ret)
        print(err)
        return 'fail'

    #ds = gdal.Open('data/template4_15.grib')
    #if ds is None:
    #    return 'fail'

    return 'success'

###############################################################################
# Test support for PNG compressed

def grib_grib2_read_png():

    if gdaltest.grib_drv is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    ds = gdal.Open('data/grib/MRMS_EchoTop_18_00.50_20161015-133230.grib2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 41854:
        gdaltest.post_reason('Could not open file')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test support for GRIB2 Section 4 Template 32, Analysis or forecast at a horizontal level or in a horizontal layer at a point in time for synthetic satellite data.

def grib_grib2_read_template_4_32():

    if gdaltest.grib_drv is None:
        return 'skip'

    # First band extracted from http://nomads.ncep.noaa.gov/pub/data/nccf/com/hur/prod/hwrf.2017102006/twenty-se27w.2017102006.hwrfsat.core.0p02.f000.grb2
    ds = gdal.Open('data/grib/twenty-se27w.2017102006.hwrfsat.core.0p02.f000_truncated.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 19911:
        gdaltest.post_reason('Could not open file')
        print(cs)
        return 'fail'
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_REF_TIME': '  1508479200 sec UTC', 'GRIB_VALID_TIME': '  1508479200 sec UTC', 'GRIB_FORECAST_SECONDS': '0 sec', 'GRIB_UNIT': '[C]', 'GRIB_PDS_TEMPLATE_NUMBERS': '5 7 2 0 0 0 0 0 1 0 0 0 0 1 0 31 1 29 67 140 2 0 0 238 217', 'GRIB_PDS_PDTN': '32', 'GRIB_COMMENT': 'Brightness Temperature [C]', 'GRIB_SHORT_NAME': '0 undefined', 'GRIB_ELEMENT': 'BRTEMP', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES' :'5 7 2 0 0 0 0 1 0 1 31 285 17292 2 61145'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'

    return 'success'

###############################################################################
# GRIB2 file with all 0 data

def grib_grib2_read_all_zero_data():

    if gdaltest.grib_drv is None:
        return 'skip'

    # From http://dd.weather.gc.ca/model_wave/great_lakes/erie/grib2/00/CMC_rdwps_lake-erie_ICEC_SFC_0_latlon0.05x0.05_2017111800_P000.grib2
    ds = gdal.Open('data/grib/CMC_rdwps_lake-erie_ICEC_SFC_0_latlon0.05x0.05_2017111800_P000.grib2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason('Could not open file')
        print(cs)
        return 'fail'
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_REF_TIME': '  1510963200 sec UTC', 'GRIB_VALID_TIME': '  1510963200 sec UTC', 'GRIB_FORECAST_SECONDS': '0 sec', 'GRIB_UNIT': '[Proportion]', 'GRIB_PDS_TEMPLATE_NUMBERS': '2 0 0 0 0 0 0 0 1 0 0 0 0 1 0 0 0 0 0 255 255 255 255 255 255', 'GRIB_PDS_PDTN': '0', 'GRIB_COMMENT': 'Ice cover [Proportion]', 'GRIB_SHORT_NAME': '0-SFC', 'GRIB_ELEMENT': 'ICEC'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'

    return 'success'

###############################################################################
# GRIB1 file with rotate pole lonlat

def grib_grib2_read_rotated_pole_lonlat():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('/vsisparse/data/grib/rotated_pole.grb.xml')

    if ds.RasterXSize != 726 or ds.RasterYSize != 550:
        gdaltest.post_reason('Did not get expected dimensions')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["unknown",SPHEROID["Sphere",6367470,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Rotated_pole"],EXTENSION["PROJ4","+proj=ob_tran +lon_0=-15 +o_proj=longlat +o_lon_p=0 +o_lat_p=30 +a=6367470 +b=6367470 +to_meter=0.0174532925199 +wktext"]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (-30.25, 0.1, 0.0, 24.15, 0.0, -0.1)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_REF_TIME': '  1503295200 sec UTC', 'GRIB_VALID_TIME': '  1503295200 sec UTC', 'GRIB_FORECAST_SECONDS': '0 sec', 'GRIB_UNIT': '[m^2/s^2]', 'GRIB_COMMENT': 'Geopotential [m^2/s^2]', 'GRIB_SHORT_NAME': '0-HTGL', 'GRIB_ELEMENT': 'GP'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'

    return 'success'

###############################################################################
# Test support for GRIB2 Section 4 Template 40, Analysis or forecast at a horizontal level or in a horizontal layer at a point in time for atmospheric chemical constituents

def grib_grib2_read_template_4_40():

    if gdaltest.grib_drv is None:
        return 'skip'

    # We could use some other encoding that JP2K...
    if not has_jp2kdrv():
        return 'skip'

    # First band extracted from https://download.regional.atmosphere.copernicus.eu/services/CAMS50?token=__M0bChV6QsoOFqHz31VRqnpr4GhWPtcpaRy3oeZjBNSg__&grid=0.1&model=ENSEMBLE&package=ANALYSIS_PM10_SURFACE&time=-24H-1H&referencetime=2017-09-12T00:00:00Z&format=GRIB2&licence=yes
    # with data nullified
    ds = gdal.Open('data/grib/template_4_40.grb2')
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_REF_TIME': '  1505088000 sec UTC', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES': '20 0 40008 0 255 99 0 0 1 0 1 -127 -2147483647 255 -127 -2147483647', 'GRIB_VALID_TIME': '  1505088000 sec UTC', 'GRIB_FORECAST_SECONDS': '0 sec', 'GRIB_UNIT': '[kg/(m^3)]', 'GRIB_PDS_TEMPLATE_NUMBERS': '20 0 156 72 0 255 99 0 0 0 1 0 0 0 0 1 255 255 255 255 255 255 255 255 255 255 255', 'GRIB_PDS_PDTN': '40', 'GRIB_COMMENT': 'Mass Density (Concentration) [kg/(m^3)]', 'GRIB_SHORT_NAME': '0-SFC', 'GRIB_ELEMENT': 'MASSDEN'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'

    return 'success'

###############################################################################
# Test support for a unhandled GRIB2 Section 4 Template 

def grib_grib2_read_template_4_unhandled():

    if gdaltest.grib_drv is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/grib/template_4_65535.grb2')
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_TEMPLATE_NUMBERS': '0 1 2 3 4 5', 'GRIB_PDS_PDTN': '65535'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 Transverse Mercator grid

def grib_grib2_read_transverse_mercator():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/grib/transverse_mercator.grb2')

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["unknown",SPHEROID["Sphere",6367470,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 Mercator grid

def grib_grib2_read_mercator():

    if gdaltest.grib_drv is None:
        return 'skip'

    if gdaltest.have_proj4 == 0:
        return 'skip'

    ds = gdal.Open('data/grib/mercator.grb2')

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (-13095853.598139772, 72.237, 0.0, 3991876.4600486886, 0.0, -72.237)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 Mercator grid

def grib_grib2_read_mercator_2sp():

    if gdaltest.grib_drv is None:
        return 'skip'

    if gdaltest.have_proj4 == 0:
        return 'skip'

    ds = gdal.Open('data/grib/mercator_2sp.grb2')

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_2SP"],PARAMETER["standard_parallel_1",33.5],PARAMETER["central_meridian",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (-10931598.94836207, 60.299, 0.0, 3332168.629121481, 0.0, -60.299)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 Lambert Conformal Conic grid

def grib_grib2_read_lcc():

    if gdaltest.grib_drv is None:
        return 'skip'

    if gdaltest.have_proj4 == 0:
        return 'skip'

    ds = gdal.Open('data/grib/lambert_conformal_conic.grb2')

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",33],PARAMETER["standard_parallel_2",34],PARAMETER["latitude_of_origin",33.5],PARAMETER["central_meridian",117],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (8974734.737685828, 60.021, 0.0, 6235918.9698001575, 0.0, -60.021)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 Polar Stereographic grid

def grib_grib2_read_polar_stereo():

    if gdaltest.grib_drv is None:
        return 'skip'

    if gdaltest.have_proj4 == 0:
        return 'skip'

    ds = gdal.Open('data/grib/polar_stereographic.grb2')

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_origin",60],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (-5621962.072511509, 71.86, 0.0, 2943991.8007649644, 0.0, -71.86)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 Albers Equal Area grid

def grib_grib2_read_aea():

    if gdaltest.grib_drv is None:
        return 'skip'

    if gdaltest.have_proj4 == 0:
        return 'skip'

    ds = gdal.Open('data/grib/albers_equal_area.grb2')

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Albers_Conic_Equal_Area"],PARAMETER["standard_parallel_1",33],PARAMETER["standard_parallel_2",34],PARAMETER["latitude_of_center",33.5],PARAMETER["longitude_of_center",117],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (8974979.714292033, 60.022, 0.0, 6235686.52464211, 0.0, -60.022)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 Lambert Azimuthal Equal Area grid

def grib_grib2_read_laea():

    if gdaltest.grib_drv is None:
        return 'skip'

    if gdaltest.have_proj4 == 0:
        return 'skip'

    ds = gdal.Open('data/grib/lambert_azimuthal_equal_area.grb2')

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Azimuthal_Equal_Area"],PARAMETER["latitude_of_center",33.5],PARAMETER["longitude_of_center",243],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]"""
    if projection != expected_projection:
        gdaltest.post_reason('Did not get expected projection')
        print(projection)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (-59384.01063035424, 60.021, 0.0, 44812.5792223211, 0.0, -60.021)
    if max([abs(gt[i] - expected_gt[i]) for i in range(6)]) > 1e-3:
        gdaltest.post_reason('Did not get expected geotransform')
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 with Grid point data - IEEE Floating Point Data (template 5.4)

def grib_grib2_read_template_5_4_grid_point_ieee_floating_point():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/grib/ieee754_single.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4727:
        gdaltest.post_reason('Did not get expected checksum')
        print(cs)
        return 'fail'

    ds = gdal.Open('data/grib/ieee754_double.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4727:
        gdaltest.post_reason('Did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 with NBITS=0 and DECIMAL_SCALE !=0

def grib_grib2_read_section_5_nbits_zero_decimal_scaled():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/grib/simple_packing_nbits_zero_decimal_scaled.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 5:
        gdaltest.post_reason('Did not get expected checksum')
        print(cs)
        return 'fail'

    if gdal.GetDriverByName('PNG') is not None:
        ds = gdal.Open('data/grib/png_nbits_zero_decimal_scaled.grb2')
        cs = ds.GetRasterBand(1).Checksum()
        if cs != 5:
            gdaltest.post_reason('Did not get expected checksum')
            print(cs)
            return 'fail'

    if has_jp2kdrv():
        ds = gdal.Open('data/grib/jpeg2000_nbits_zero_decimal_scaled.grb2')
        cs = ds.GetRasterBand(1).Checksum()
        if cs != 5:
            gdaltest.post_reason('Did not get expected checksum')
            print(cs)
            return 'fail'

    return 'success'

###############################################################################
# Test reading GRIB2 with complex packing and spatial differencing of order 1

def grib_grib2_read_spatial_differencing_order_1():

    if gdaltest.grib_drv is None:
        return 'skip'

    ds = gdal.Open('data/grib/spatial_differencing_order_1.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 46650:
        gdaltest.post_reason('Did not get expected checksum')
        print(cs)

    return 'success'

###############################################################################
# Test GRIB2 creation options

def grib_grib2_write_creation_options():

    if gdaltest.grib_drv is None:
        return 'skip'

    tmpfilename = '/vsimem/out.grb2'
    gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "DISCIPLINE=1",
        "IDS=CENTER=85(Toulouse) SUBCENTER=2 MASTER_TABLE=5 LOCAL_TABLE=0 SIGNF_REF_TIME=0(Analysis) REF_TIME=2017-09-11T12:34:56Z PROD_STATUS=2(Research) TYPE=0(Analysis)",
        "IDS_SUBCENTER=3", # Test that it overrides IDS
        "PDS_PDTN=30",
        "BAND_1_PDS_PDTN=40", # Test that it overrides PDS_PDTN
        "PDS_TEMPLATE_NUMBERS=20 0 156 72 0 255 99 0 0 0 1 0 0 0 0 1 255 255 255 255 255 255 255 255 255 255 255"
                   ])
    ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_IDS': 'CENTER=85(Toulouse) SUBCENTER=3 MASTER_TABLE=5 LOCAL_TABLE=0 SIGNF_REF_TIME=0(Analysis) REF_TIME=2017-09-11T12:34:56Z PROD_STATUS=2(Research) TYPE=0(Analysis)', 'GRIB_PDS_TEMPLATE_NUMBERS': '20 0 156 72 0 255 99 0 0 0 1 0 0 0 0 1 255 255 255 255 255 255 255 255 255 255 255', 'GRIB_DISCIPLINE': '1(Hydrological)', 'GRIB_PDS_PDTN': '40'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_NUMBERS and more elements than needed (warning)
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                    creationOptions = [
            "PDS_PDTN=40",
            "PDS_TEMPLATE_NUMBERS=20 0 156 72 0 255 99 0 0 0 1 0 0 0 0 1 255 255 255 255 255 255 255 255 255 255 255 0extra"
                    ])
    if out_ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    out_ds = None
    ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_PDTN': '40', 'GRIB_PDS_TEMPLATE_NUMBERS': '20 0 156 72 0 255 99 0 0 0 1 0 0 0 0 1 255 255 255 255 255 255 255 255 255 255 255 0'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_ASSEMBLED_VALUES and insufficient number of elements
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                    creationOptions = [
            "PDS_PDTN=40",
            "PDS_TEMPLATE_NUMBERS=20 0 156 72 0 255 99 0 0 0 1 0 0 0 0 1 255 255 255 255 255 255 255 255 255 255"
                    ])
    if out_ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_ASSEMBLED_VALUES
    gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=40",
        "PDS_TEMPLATE_ASSEMBLED_VALUES=20 0 40008 0 255 99 0 0 1 0 1 -127 -2147483647 255 -127 -2147483647"
                   ])
    ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_PDTN': '40', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES' : '20 0 40008 0 255 99 0 0 1 0 1 -127 -2147483647 255 -127 -2147483647'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_ASSEMBLED_VALUES and more elements than needed (warning)
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                    creationOptions = [
            "PDS_PDTN=40",
            "PDS_TEMPLATE_ASSEMBLED_VALUES=20 0 40008 0 255 99 0 0 1 0 1 -127 -2147483647 255 -127 -2147483647 0extra"
                    ])
    if out_ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    out_ds = None
    ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_PDTN': '40', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES' : '20 0 40008 0 255 99 0 0 1 0 1 -127 -2147483647 255 -127 -2147483647'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_ASSEMBLED_VALUES and insufficient number of elements
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                    creationOptions = [
            "PDS_PDTN=40",
            "PDS_TEMPLATE_ASSEMBLED_VALUES=20 0 40008 0 255 99 0 0 1 0 1 -127 -2147483647 255 -127"
                    ])
    if out_ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_ASSEMBLED_VALUES with variable number of elements
    gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=32",
        "PDS_TEMPLATE_ASSEMBLED_VALUES=5 7 2 0 0 0 0 1 0 2 31 285 17292 2 61145 31 285 17292 2 61145"
                   ])
    ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_PDTN': '32', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES' : '5 7 2 0 0 0 0 1 0 2 31 285 17292 2 61145 31 285 17292 2 61145'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_ASSEMBLED_VALUES with variable number of elements, and insufficient number of elements in the variable section
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=32",
        "PDS_TEMPLATE_ASSEMBLED_VALUES=5 7 2 0 0 0 0 1 0 2 31 285 17292 2 61145 31 285 17292 2"
                   ])
    if out_ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_ASSEMBLED_VALUES with variable number of elements, and extra elements
    with gdaltest.error_handler():
        gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=32",
        "PDS_TEMPLATE_ASSEMBLED_VALUES=5 7 2 0 0 0 0 1 0 2 31 285 17292 2 61145 31 285 17292 2 61145 0extra"
                   ])
    ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_PDTN': '32', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES' : '5 7 2 0 0 0 0 1 0 2 31 285 17292 2 61145 31 285 17292 2 61145'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    # Test with PDS_TEMPLATE_NUMBERS with variable number of elements
    gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=32",
        "PDS_TEMPLATE_NUMBERS=5 7 2 0 0 0 0 0 1 0 0 0 0 2 0 31 1 29 67 140 2 0 0 238 217 0 31 1 29 67 140 2 0 0 238 217"
                   ])
    ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_PDTN': '32', 'GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES' : '5 7 2 0 0 0 0 1 0 2 31 285 17292 2 61145 31 285 17292 2 61145'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    # Test with unknown PDS_PDTN with PDS_TEMPLATE_NUMBERS
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=65535",
        "PDS_TEMPLATE_NUMBERS=1 2 3 4 5"
                   ])
    if out_ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    out_ds = None
    with gdaltest.error_handler():
        ds = gdal.Open(tmpfilename)
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_PDS_PDTN': '65535', 'GRIB_PDS_TEMPLATE_NUMBERS' : '1 2 3 4 5'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

     # Test with unknown PDS_PDTN with PDS_TEMPLATE_ASSEMBLED_VALUES
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=65535",
        "PDS_TEMPLATE_ASSEMBLED_VALUES=1 2 3 4 5"
                   ])
    if out_ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    gdal.Unlink(tmpfilename)

    # Test with PDS_PDTN != 0 without template numbers
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=32"
                   ])
    if out_ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    gdal.Unlink(tmpfilename)

    # Test with invalid values in PDS_TEMPLATE_NUMBERS
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=254",
        "PDS_TEMPLATE_NUMBERS=-1 256 0 0 0 0"
                   ])
    if out_ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    out_ds = None
    gdal.Unlink(tmpfilename)

    # Test with invalid values in PDS_TEMPLATE_ASSEMBLED_VALUES
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=44",
        # {44,21,0,                    {1,  1, 2,1,-1,      -4,   -1,-4,1,1,1,  2,  1,1,-2   ,1,-1,  -4,1,-1,-4} },
        "PDS_TEMPLATE_ASSEMBLED_VALUES=-1 256 -1 1 128 4000000000 -1 -4 1 1 1 65536 1 1 32768 1 -129 -4 1 -1 -4"
                   ])
    if out_ds is None:
        gdaltest.post_reason('failure')
        return 'fail'
    out_ds = None
    gdal.Unlink(tmpfilename)

    # Test with both PDS_TEMPLATE_NUMBERS and PDS_TEMPLATE_ASSEMBLED_VALUES
    with gdaltest.error_handler():
        out_ds = gdal.Translate(tmpfilename, 'data/byte.tif', format = 'GRIB',
                   creationOptions = [
        "PDS_PDTN=40",
        "PDS_TEMPLATE_NUMBERS=20 0 156 72 0 255 99 0 0 0 1 0 0 0 0 1 255 255 255 255 255 255 255 255 255 255 255",
        "PDS_TEMPLATE_ASSEMBLED_VALUES=20 0 40008 0 255 99 0 0 1 0 1 -127 -2147483647 255 -127 -2147483647"
                   ])
    if out_ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    gdal.Unlink(tmpfilename)

    return 'success'

###############################################################################
# Test GRIB2 write support for projections

def grib_grib2_write_projections():

    if gdaltest.grib_drv is None:
        return 'skip'

    filenames = [ 'albers_equal_area.grb2',
                  'lambert_azimuthal_equal_area.grb2',
                  'lambert_conformal_conic.grb2',
                  'mercator.grb2',
                  'mercator_2sp.grb2',
                  'polar_stereographic.grb2',
                  'ieee754_single.grb2' # Longitude latitude
                ]
    for filename in filenames:
        filename = 'data/grib/' + filename
        src_ds = gdal.Open(filename)
        tmpfilename = '/vsimem/out.grb2'
        gdal.Translate( tmpfilename, filename, format = 'GRIB' )
        out_ds = gdal.Open(tmpfilename)

        if src_ds.GetProjectionRef() != out_ds.GetProjectionRef():
            gdaltest.post_reason('did not get expected projection for %s' % filename)
            print(out_ds.GetProjectionRef())
            print(src_ds.GetProjectionRef())
            return 'fail'

        expected_gt = src_ds.GetGeoTransform()
        got_gt = out_ds.GetGeoTransform()
        if max([abs(expected_gt[i]-got_gt[i]) for i in range(6)]) > 1e-5:
            gdaltest.post_reason('did not get expected geotransform for %s' % filename)
            print(got_gt)
            print(expected_gt)
            return 'fail'

        out_ds = None
        gdal.Unlink(tmpfilename)

    # Test writing GRS80
    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 1, gdal.GDT_Float32)
    src_ds.SetGeoTransform([2,1,0,49,0,-1])
    src_ds.SetProjection("""GEOGCS["GRS 1980(IUGG, 1980)",
    DATUM["unknown",
        SPHEROID["GRS80",6378137,298.257222101]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]""")
    tmpfilename = '/vsimem/out.grb2'
    out_ds = gdaltest.grib_drv.CreateCopy(tmpfilename, src_ds)
    wkt = out_ds.GetProjectionRef()
    out_ds = None
    gdal.Unlink(tmpfilename)
    if wkt.find('SPHEROID["GRS80",6378137,298.257222101]') < 0:
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    # Test writing Mercator_1SP with scale != 1 (will be read as Mercator_2SP)
    src_ds = gdal.Warp('', 'data/byte.tif', format = 'MEM', dstSRS = """PROJCS["unnamed",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",0.8347374126019634],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]""")
    tmpfilename = '/vsimem/out.grb2'
    gdal.Translate( tmpfilename, src_ds, format = 'GRIB' )
    out_ds = gdal.Open(tmpfilename)
    expected_wkt = 'PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["unknown",SPHEROID["Spheroid imported from GRIB file",6378206.4,294.9786982139109]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_2SP"],PARAMETER["standard_parallel_1",33.500986],PARAMETER["central_meridian",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]'
    got_sr = osr.SpatialReference()
    got_sr.SetFromUserInput(out_ds.GetProjectionRef())
    expected_sr = osr.SpatialReference()
    expected_sr.SetFromUserInput(expected_wkt)
    if got_sr.IsSame(expected_sr) == 0:
        gdaltest.post_reason('did not get expected projection for Mercator_1SP')
        print(out_ds.GetProjectionRef())
        return 'fail'
    expected_gt=(-10931635.565066436, 60.297, 0.0, 3331982.221608528, 0.0, -60.297)
    got_gt = out_ds.GetGeoTransform()
    if max([abs(expected_gt[i]-got_gt[i]) for i in range(6)]) > 1e-5:
        gdaltest.post_reason('did not get expected geotransform for Mercator_1SP')
        print(got_gt)
        return 'fail'
    out_ds = None
    gdal.Unlink(tmpfilename)

    # Test writing LCC_1SP (will be read as LCC_2SP)
    src_ds = gdal.Warp('', 'data/byte.tif', format = 'MEM', dstSRS = """PROJCS["unnamed",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",33.5],
    PARAMETER["central_meridian",117],
    PARAMETER["scale_factor",0.9999],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]""")
    tmpfilename = '/vsimem/out.grb2'
    gdal.Translate( tmpfilename, src_ds, format = 'GRIB' )
    out_ds = gdal.Open(tmpfilename)
    expected_wkt = 'PROJCS["unnamed",GEOGCS["Coordinate System imported from GRIB file",DATUM["unknown",SPHEROID["Spheroid imported from GRIB file",6378206.4,294.9786982139109]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",34.310911],PARAMETER["standard_parallel_2",32.686501],PARAMETER["latitude_of_origin",33.5],PARAMETER["central_meridian",117],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]'
    got_sr = osr.SpatialReference()
    got_sr.SetFromUserInput(out_ds.GetProjectionRef())
    expected_sr = osr.SpatialReference()
    expected_sr.SetFromUserInput(expected_wkt)
    if got_sr.IsSame(expected_sr) == 0:
        gdaltest.post_reason('did not get expected projection for LCC_1SP')
        print(out_ds.GetProjectionRef())
        return 'fail'
    expected_gt=(8974472.884926716, 60.017, 0.0, 6235685.688523474, 0.0, -60.017)
    got_gt = out_ds.GetGeoTransform()
    if max([abs(expected_gt[i]-got_gt[i]) for i in range(6)]) > 1e-5:
        gdaltest.post_reason('did not get expected geotransform for LCC_1SP')
        print(got_gt)
        return 'fail'
    out_ds = None
    gdal.Unlink(tmpfilename)

    return 'success'

###############################################################################
def _grib_read_section(filename, sect_num_to_read):

    f = gdal.VSIFOpenL(filename, 'rb')
    if f is None:
        return None
    # Ignore Sect 0
    gdal.VSIFReadL(1, 16, f)
    ret = None
    while True:
        sect_size_bytes = gdal.VSIFReadL(1, 4, f)
        if len(sect_size_bytes) == 0:
            break
        sect_size_num = struct.unpack('>I', sect_size_bytes)[0]
        sect_num_bytes = gdal.VSIFReadL(1, 1, f)
        sect_num = ord(sect_num_bytes)
        if sect_num == sect_num_to_read:
            ret = sect_size_bytes + sect_num_bytes + gdal.VSIFReadL(1, sect_size_num - 5, f)
            break
        gdal.VSIFSeekL(f, sect_size_num - 5, 1)

    gdal.VSIFCloseL(f)
    return ret

###############################################################################
# Test GRIB2 write support for data encodings

def grib_grib2_write_data_encodings():

    if gdaltest.grib_drv is None:
        return 'skip'

    # Template 5 numbers
    GS5_SIMPLE = 0
    GS5_CMPLX = 2
    GS5_CMPLXSEC = 3
    GS5_IEEE = 4
    GS5_JPEG2000 = 40
    GS5_PNG = 41

    tests = []
    tests += [ [ 'data/byte.tif', [], 4672, GS5_SIMPLE ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING' ], 4672, GS5_SIMPLE ] ]

    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=IEEE_FLOATING_POINT' ], 4672, GS5_IEEE ] ]

    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'NBITS=8' ], 4672, GS5_SIMPLE ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'NBITS=9' ], 4672, GS5_SIMPLE ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'NBITS=7' ], 4484, GS5_SIMPLE ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'DECIMAL_SCALE_FACTOR=-1' ], 4820, GS5_SIMPLE ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'NBITS=5', 'DECIMAL_SCALE_FACTOR=-1' ], 4820, GS5_SIMPLE ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'NBITS=8', 'DECIMAL_SCALE_FACTOR=-1' ], 4855, GS5_SIMPLE ] ]

    tests += [ [ 'data/grib/ds.mint.bin', [ 'PDS_PDTN=8', 'PDS_TEMPLATE_ASSEMBLED_VALUES=0 5 2 0 0 255 255 1 19 1 0 0 255 -1 -2147483647 2008 2 22 12 0 0 1 0 3 255 1 12 1 0'  ], 46650, GS5_CMPLX ] ] # has nodata, hence complex packing
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=COMPLEX_PACKING' ], 4672, GS5_CMPLX ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=COMPLEX_PACKING', 'SPATIAL_DIFFERENCING_ORDER=0' ], 4672, GS5_CMPLX ] ]
    tests += [ [ 'data/byte.tif', [ 'SPATIAL_DIFFERENCING_ORDER=1' ], 4672, GS5_CMPLXSEC ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=COMPLEX_PACKING', 'SPATIAL_DIFFERENCING_ORDER=1' ], 4672, GS5_CMPLXSEC ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=COMPLEX_PACKING', 'SPATIAL_DIFFERENCING_ORDER=2' ], 4672, GS5_CMPLXSEC ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=COMPLEX_PACKING', 'DECIMAL_SCALE_FACTOR=-1' ], 4820, GS5_CMPLX ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=COMPLEX_PACKING', 'NBITS=7' ], 4484, GS5_CMPLX ] ]

    if gdal.GetDriverByName('PNG') is not None:
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG' ], 4672, GS5_PNG ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=8' ], 4672, GS5_PNG ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'DECIMAL_SCALE_FACTOR=-1' ], 4820, GS5_PNG ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=8', 'DECIMAL_SCALE_FACTOR=-1' ], 4855, GS5_PNG ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=9' ], 4672, GS5_PNG ] ] # rounded to 16 bit
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=7' ], 4672, GS5_PNG ] ] # rounded to 8 bit
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=4' ], 5103, GS5_PNG ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=3' ], 5103, GS5_PNG ] ] # rounded to 4 bit
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=2' ], 5103, GS5_PNG ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=PNG', 'NBITS=1' ], 5103, GS5_PNG ] ]
        tests += [ [ '../gcore/data/float32.tif', [ 'DATA_ENCODING=PNG' ], 4672, GS5_PNG ] ]

    found_j2k_drivers = []
    for drvname in [ 'JP2KAK', 'JP2OPENJPEG', 'JPEG2000', 'JP2ECW' ]:
        if gdal.GetDriverByName(drvname) is not None:
            if drvname != 'JP2ECW':
                found_j2k_drivers.append(drvname)
            else:
                import ecw
                if ecw.has_write_support():
                    found_j2k_drivers.append(drvname)

    if len(found_j2k_drivers) > 0:
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=JPEG2000' ], 4672, GS5_JPEG2000 ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=JPEG2000', 'COMPRESSION_RATIO=2' ], 4672, GS5_JPEG2000 ] ] # COMPRESSION_RATIO ignored in that case
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=JPEG2000', 'NBITS=8' ], 4672, GS5_JPEG2000 ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=JPEG2000', 'DECIMAL_SCALE_FACTOR=-1' ], 4820, GS5_JPEG2000 ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=JPEG2000', 'NBITS=8', 'DECIMAL_SCALE_FACTOR=-1' ], 4855, GS5_JPEG2000 ] ]
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=JPEG2000', 'NBITS=9' ], 4672, GS5_JPEG2000 ] ]
        # 4899 for JP2ECW, 4440 for JP2OPENJPEG
        tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=JPEG2000', 'NBITS=7' ], (4484, 4899, 4440), GS5_JPEG2000 ] ]
        for drvname in found_j2k_drivers:
            tests += [ [ 'data/byte.tif', [ 'JPEG2000_DRIVER=' + drvname ], 4672, GS5_JPEG2000 ] ]
        tests += [ [ '../gcore/data/float32.tif', [ 'DATA_ENCODING=JPEG2000' ], 4672, GS5_JPEG2000 ] ]

    tests += [ [ '../gcore/data/int16.tif', [], 4672, GS5_SIMPLE ] ]
    tests += [ [ '../gcore/data/uint16.tif', [], 4672, GS5_SIMPLE ] ]
    tests += [ [ '../gcore/data/int32.tif', [], 4672, GS5_SIMPLE ] ]
    tests += [ [ '../gcore/data/uint32.tif', [], 4672, GS5_SIMPLE ] ]
    tests += [ [ '../gcore/data/float32.tif', [], 4672, GS5_IEEE ] ]
    tests += [ [ '../gcore/data/float64.tif', [], 4672, GS5_IEEE ] ]
    tests += [ [ '../gcore/data/float32.tif', [ 'DATA_ENCODING=IEEE_FLOATING_POINT' ], 4672, GS5_IEEE ] ]
    tests += [ [ '../gcore/data/float64.tif', [ 'DATA_ENCODING=IEEE_FLOATING_POINT' ], 4672, GS5_IEEE ] ]
    tests += [ [ '../gcore/data/float32.tif', [ 'DATA_ENCODING=COMPLEX_PACKING' ], 4672, GS5_CMPLX ] ]

    one_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    one_ds.SetGeoTransform([2,1,0,49,0,-1])
    sr = osr.SpatialReference()
    sr.SetFromUserInput('WGS84')
    one_ds.SetProjection( sr.ExportToWkt() )
    one_ds.GetRasterBand(1).Fill(1)

    tests += [ [ one_ds, [], 1, GS5_SIMPLE ] ]
    tests += [ [ one_ds, [ 'DATA_ENCODING=COMPLEX_PACKING' ], 1, GS5_CMPLX ] ]
    if gdal.GetDriverByName('PNG') is not None:
        tests += [ [ one_ds, [ 'DATA_ENCODING=PNG' ], 1, GS5_PNG ] ]
    if len(found_j2k_drivers) > 0:
        tests += [ [ one_ds, [ 'DATA_ENCODING=JPEG2000' ], 1, GS5_JPEG2000 ] ]

    nodata_never_hit_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    nodata_never_hit_ds.SetGeoTransform([2,1,0,49,0,-1])
    nodata_never_hit_ds.SetProjection( sr.ExportToWkt() )
    nodata_never_hit_ds.GetRasterBand(1).SetNoDataValue(1)

    tests += [ [ nodata_never_hit_ds, [], 0, GS5_SIMPLE ] ]

    all_nodata_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    all_nodata_ds.SetGeoTransform([2,1,0,49,0,-1])
    all_nodata_ds.SetProjection( sr.ExportToWkt() )
    all_nodata_ds.GetRasterBand(1).SetNoDataValue(0)

    tests += [ [ all_nodata_ds, [ 'DATA_ENCODING=COMPLEX_PACKING'  ], 0, GS5_CMPLX ] ]

    for (filename, options, expected_cs, expected_section5_template_number) in tests:
        tmpfilename = '/vsimem/out.grb2'
        gdal.ErrorReset()
        gdal.Translate( tmpfilename, filename, format = 'GRIB',
                       creationOptions = options )
        error_msg = gdal.GetLastErrorMsg()
        if error_msg != '':
            gdaltest.post_reason('did not expect error for %s, %s' % (str(filename), str(options)))
            return 'fail'

        section5 = _grib_read_section(tmpfilename, 5)
        section5_template_number = struct.unpack('>h', section5[9:11])[0]
        if section5_template_number != expected_section5_template_number:
            gdaltest.post_reason('did not get expected section 5 template number for %s, %s' % (str(filename), str(options)))
            print(section5_template_number, expected_section5_template_number)
            return 'fail'

        out_ds = gdal.Open(tmpfilename)
        cs = out_ds.GetRasterBand(1).Checksum()
        nd = out_ds.GetRasterBand(1).GetNoDataValue()
        out_ds = None
        gdal.Unlink(tmpfilename)
        if type(expected_cs) != type((1,)):
            expected_cs = (expected_cs,)
        if cs not in expected_cs:
            gdaltest.post_reason('did not get expected checksum for %s, %s' % (str(filename), str(options)))
            print(cs, expected_cs)
            return 'fail'

        if section5_template_number in (GS5_CMPLX, GS5_CMPLXSEC):
            if type(filename) == type(''):
                ref_ds = gdal.Open(filename)
            else:
                ref_ds = filename
            expected_nd = ref_ds.GetRasterBand(1).GetNoDataValue()
            if nd != expected_nd:
                gdaltest.post_reason('did not get expected nodata for %s, %s' % (str(filename), str(options)))
                print(nd, expected_nd)
                return 'fail'

    # Test floating point data with dynamic < 1
    test_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 1, gdal.GDT_Float32)
    test_ds.SetGeoTransform([2,1,0,49,0,-1])
    test_ds.SetProjection( sr.ExportToWkt() )
    test_ds.WriteRaster(0, 0, 2, 2, struct.pack(4 * 'f', 1.23, 1.45, 1.56, 1.78))

    encodings = [ 'SIMPLE_PACKING', 'COMPLEX_PACKING', 'IEEE_FLOATING_POINT' ]
    if gdal.GetDriverByName('PNG') is not None:
        encodings += [ 'PNG' ]
    # JPEG2000 doesn't result in an appropriate result
    if len(found_j2k_drivers) > 0 and found_j2k_drivers != [ 'JPEG2000' ] and found_j2k_drivers != [ 'JPEG2000', 'JP2ECW' ]:
        encodings += [ 'JPEG2000' ]

    for encoding in encodings:
        tmpfilename = '/vsimem/out.grb2'
        gdal.ErrorReset()
        gdal.Translate( tmpfilename, test_ds, format = 'GRIB', creationOptions = ['DATA_ENCODING=' + encoding] )
        error_msg = gdal.GetLastErrorMsg()
        if error_msg != '':
            gdaltest.post_reason('did not expect error for %s, %s' % (str(filename), str(options)))
            return 'fail'
        out_ds = gdal.Open(tmpfilename)
        got_vals = struct.unpack(4 * 'd', out_ds.ReadRaster())
        out_ds = None
        gdal.Unlink(tmpfilename)
        if encoding == 'IEEE_FLOATING_POINT':
            expected_vals = (1.23, 1.45, 1.56, 1.78)
        else:
            expected_vals = (1.2300000190734863, 1.4487500190734863, 1.5581250190734863, 1.7807812690734863)
        if max([abs(got_vals[i] - expected_vals[i]) for i in range(4)]) > 1e-7:
            gdaltest.post_reason('did not get expected values')
            print(got_vals)
            return 'fail'
    test_ds = None

    # Test floating point data with very large dynamic
    test_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 1, gdal.GDT_Float32)
    test_ds.SetGeoTransform([2,1,0,49,0,-1])
    test_ds.SetProjection( sr.ExportToWkt() )
    test_ds.WriteRaster(0, 0, 2, 2, struct.pack(4 * 'f', 1.23e10, -2.45e10, 1.23e10, -2.45e10))

    encodings = [ 'SIMPLE_PACKING' ]
    if gdal.GetDriverByName('PNG') is not None:
        encodings += [ 'PNG' ]
    # JP2ECW doesn't manage to compress such a small file
    if len(found_j2k_drivers) > 0 and found_j2k_drivers != [ 'JP2ECW' ] and found_j2k_drivers != [ 'JPEG2000', 'JP2ECW' ]:
        encodings += [ 'JPEG2000' ]

    for encoding in encodings:
        tmpfilename = '/vsimem/out.grb2'
        if encoding != 'SIMPLE_PACKING':
            gdal.PushErrorHandler()
        gdal.Translate( tmpfilename, test_ds, format = 'GRIB', creationOptions = ['DATA_ENCODING=' + encoding] )
        if encoding != 'SIMPLE_PACKING':
            gdal.PopErrorHandler()
        out_ds = gdal.Open(tmpfilename)
        if out_ds is None:
            gdaltest.post_reason('failed to re-open dataset for ' + encoding)
            return 'fail'
        got_vals = struct.unpack(4 * 'd', out_ds.ReadRaster())
        out_ds = None
        gdal.Unlink(tmpfilename)
        expected_vals = (1.23e10, -2.45e10,1.23e10, -2.45e10)
        if max([abs((got_vals[i] - expected_vals[i])/expected_vals[i]) for i in range(4)]) > 1e-4:
            gdaltest.post_reason('did not get expected values for ' + encoding)
            print(got_vals)
            print(max([abs((got_vals[i] - expected_vals[i])/expected_vals[i]) for i in range(2)]))
            return 'fail'
    test_ds = None

    # Test lossy J2K compression
    for drvname in found_j2k_drivers:
        tmpfilename = '/vsimem/out.grb2'
        gdal.ErrorReset()
        gdal.Translate( tmpfilename, 'data/utm.tif', format = 'GRIB',
                        creationOptions = ['JPEG2000_DRIVER='+drvname,
                                           'COMPRESSION_RATIO=20'] )
        error_msg = gdal.GetLastErrorMsg()
        if error_msg != '':
            gdaltest.post_reason('did not expect error for %s, %s' % (str(filename), str(options)))
            return 'fail'
        out_ds = gdal.Open(tmpfilename)
        cs = out_ds.GetRasterBand(1).Checksum()
        out_ds = None
        gdal.Unlink(tmpfilename)
        if cs == 0 or cs == 50235: # 50235: lossless checksum
            gdaltest.post_reason('did not get expected checksum for lossy JPEG2000 with ' + drvname)
            print(cs)

    return 'success'

###############################################################################
# Test GRIB2 write support with warnings/errors

def grib_grib2_write_data_encodings_warnings_and_errors():

    if gdaltest.grib_drv is None:
        return 'skip'

    # Cases where warnings are expected
    tests = []
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'DECIMAL_SCALE_FACTOR=1' ], 4672 ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'JPEG2000_DRIVER=FOO' ], 4672 ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'JPEG2000_DRIVER=FOO' ], 4672 ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=SIMPLE_PACKING', 'SPATIAL_DIFFERENCING_ORDER=1' ], 4672 ] ]
    tests += [ [ 'data/grib/ds.mint.bin', [ 'DATA_ENCODING=SIMPLE_PACKING' ], 41640 ] ] # should warn since simple packing doesn't support nodata
    tests += [ [ 'data/byte.tif', [ 'NBITS=32' ], 4672 ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=IEEE_FLOATING_POINT', 'NBITS=8' ], 4672 ] ]
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=IEEE_FLOATING_POINT', 'DECIMAL_SCALE_FACTOR=-1' ], 4672 ] ]
    for (filename, options, expected_cs) in tests:
        tmpfilename = '/vsimem/out.grb2'
        src_ds = gdal.Open(filename)
        gdal.ErrorReset()
        with gdaltest.error_handler():
            out_ds = gdaltest.grib_drv.CreateCopy( tmpfilename, src_ds, options = options )

        error_msg = gdal.GetLastErrorMsg()
        if error_msg == '':
            gdaltest.post_reason('expected warning for %s, %s' % (str(filename), str(options)))
            return 'fail'
        if out_ds is None:
            gdaltest.post_reason('did not expect null return for %s, %s' % (str(filename), str(options)))
            return 'fail'

        cs = out_ds.GetRasterBand(1).Checksum()

        out_ds = None
        gdal.Unlink(tmpfilename)
        if type(expected_cs) != type((1,)):
            expected_cs = (expected_cs,)
        if cs not in expected_cs:
            gdaltest.post_reason('did not get expected checksum for %s, %s' % (str(filename), str(options)))
            print(cs, expected_cs)
            return 'fail'

    # Cases where errors are expected
    tests = []
    tests += [ [ 'data/byte.tif', [ 'DATA_ENCODING=FOO' ] ] ] # invalid DATA_ENCODING
    tests += [ [ 'data/byte.tif', [ 'JPEG2000_DRIVER=FOO', 'SPATIAL_DIFFERENCING_ORDER=BAR' ] ] ] # both cannot be used together
    tests += [ [ 'data/byte.tif', [ 'SPATIAL_DIFFERENCING_ORDER=3' ] ] ]
    tests += [ [ 'data/byte.tif', [ 'JPEG2000_DRIVER=THIS_IS_NOT_A_J2K_DRIVER' ] ] ] # non-existing driver
    tests += [ [ 'data/byte.tif', [ 'JPEG2000_DRIVER=DERIVED' ] ] ] # Read-only driver
    tests += [ [ '../gcore/data/cfloat32.tif', [] ] ] # complex data type
    tests += [ [ 'data/float64.asc', [] ] ] # no projection
    tests += [ [ 'data/byte.sgi', [] ] ] # no geotransform
    tests += [ [ 'data/rotation.img', [] ] ] # geotransform with rotation terms
    gdal.GetDriverByName('GTiff').Create('/vsimem/huge.tif', 65535, 65535, 1, options = ['SPARSE_OK=YES'])
    tests += [ [ '/vsimem/huge.tif', [] ] ] # too many pixels

    for (filename, options,) in tests:
        tmpfilename = '/vsimem/out.grb2'
        src_ds = gdal.Open(filename)
        gdal.ErrorReset()
        with gdaltest.error_handler():
            out_ds = gdaltest.grib_drv.CreateCopy( tmpfilename, src_ds, options = options )

        error_msg = gdal.GetLastErrorMsg()
        if error_msg == '':
            gdaltest.post_reason('expected warning for %s, %s' % (str(filename), str(options)))
            return 'fail'
        if out_ds is not None:
            gdaltest.post_reason('expected null return for %s, %s' % (str(filename), str(options)))
            return 'fail'
        out_ds = None
        gdal.Unlink(tmpfilename)

    gdal.Unlink('/vsimem/huge.tif')

    with gdaltest.error_handler():
        out_ds = gdal.Translate('/i/do_not/exist.grb2', 'data/byte.tif', format = 'GRIB')
    if out_ds is not None:
        gdaltest.post_reason('expected null return')
        return 'fail'

    return 'success'

###############################################################################
# Test writing temperatures with automatic Celcius -> Kelvin conversion

def grib_grib2_write_temperatures():

    for (src_type, data_encoding, input_unit) in [
            (gdal.GDT_Float32, 'IEEE_FLOATING_POINT', None),
            (gdal.GDT_Float32, 'IEEE_FLOATING_POINT', 'C'),
            (gdal.GDT_Float32, 'IEEE_FLOATING_POINT', 'K'),
            (gdal.GDT_Float64, 'IEEE_FLOATING_POINT', None),
            (gdal.GDT_Float32, 'SIMPLE_PACKING', None) ]:

        src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 1, src_type)
        src_ds.SetGeoTransform([2,1,0,49,0,-1])
        sr = osr.SpatialReference()
        sr.SetFromUserInput('WGS84')
        src_ds.SetProjection( sr.ExportToWkt() )
        src_ds.WriteRaster(0, 0, 2, 2,
                           struct.pack(4 * 'f', 25.0, 25.1, 25.1, 25.2),
                           buf_type = gdal.GDT_Float32)

        tmpfilename = '/vsimem/out.grb2'
        options = [
            'DATA_ENCODING=' + data_encoding,
            'PDS_PDTN=8',
            'PDS_TEMPLATE_NUMBERS=0 5 2 0 0 0 255 255 1 0 0 0 43 1 0 0 0 0 0 255 129 255 255 255 255 7 216 2 23 12 0 0 1 0 0 0 0 3 255 1 0 0 0 12 1 0 0 0 0'
        ]
        if input_unit is not None:
            options += [ 'INPUT_UNIT=' + input_unit ]
        gdaltest.grib_drv.CreateCopy( tmpfilename, src_ds, options = options)

        out_ds = gdal.Open(tmpfilename)
        got_vals = struct.unpack(4 * 'd', out_ds.ReadRaster())
        out_ds = None
        gdal.Unlink(tmpfilename)
        if input_unit != 'K':
            expected_vals = (25.0, 25.1, 25.1, 25.2)
        else:
            expected_vals = (25.0 - 273.15, 25.1 - 273.15, 25.1 - 273.15, 25.2 - 273.15)
        if max([abs((got_vals[i] - expected_vals[i])/expected_vals[i]) for i in range(4)]) > 1e-4:
            gdaltest.post_reason('fail with data_encoding = %s and type = %s' % (data_encoding, str(src_type)))
            print(got_vals)
            print(max([abs((got_vals[i] - expected_vals[i])/expected_vals[i]) for i in range(2)]))
            return 'fail'

    return 'success'

###############################################################################
# Test GRIB2 file with JPEG2000 codestream on a single line (#6719)
def grib_online_grib2_jpeg2000_single_line():

    if gdaltest.grib_drv is None:
        return 'skip'

    if not has_jp2kdrv():
        return 'skip'

    filename = 'CMC_hrdps_continental_PRATE_SFC_0_ps2.5km_2017111712_P001-00.grib2'
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/grib/' + filename):
        return 'skip'

    ds = gdal.Open('tmp/cache/' + filename)
    cs = ds.GetRasterBand(1).Checksum()
    if cs == 0:
        gdaltest.post_reason('Could not open file')
        print(cs)
        return 'fail'
    nd = ds.GetRasterBand(1).GetNoDataValue()
    if nd != 9999:
        gdaltest.post_reason('Bad nodata value')
        print(nd)
        return 'fail'
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {'GRIB_REF_TIME': '  1510920000 sec UTC', 'GRIB_VALID_TIME': '  1510923600 sec UTC', 'GRIB_FORECAST_SECONDS': '3600 sec', 'GRIB_UNIT': '[kg/(m^2 s)]', 'GRIB_PDS_TEMPLATE_NUMBERS': '1 7 2 50 50 0 0 0 0 0 0 0 60 1 0 0 0 0 0 255 255 255 255 255 255', 'GRIB_PDS_PDTN': '0', 'GRIB_COMMENT': 'Precipitation rate [kg/(m^2 s)]', 'GRIB_SHORT_NAME': '0-SFC', 'GRIB_ELEMENT': 'PRATE'}
    for k in expected_md:
        if k not in md or md[k] != expected_md[k]:
            gdaltest.post_reason('Did not get expected metadata')
            print(md)
            return 'fail'

    return 'success'

gdaltest_list = [
    grib_1,
    grib_2,
    grib_read_different_sizes_messages,
    grib_grib2_read_nodata,
    grib_read_units,
    grib_read_geotransform_one_n_or_n_one,
    grib_read_vsizip,
    grib_grib2_test_grib_pds_all_bands,
    grib_grib2_read_template_4_15,
    grib_grib2_read_png,
    grib_grib2_read_template_4_32,
    grib_grib2_read_all_zero_data,
    grib_grib2_read_rotated_pole_lonlat,
    grib_grib2_read_template_4_40,
    grib_grib2_read_template_4_unhandled,
    grib_grib2_read_transverse_mercator,
    grib_grib2_read_mercator,
    grib_grib2_read_mercator_2sp,
    grib_grib2_read_lcc,
    grib_grib2_read_polar_stereo,
    grib_grib2_read_aea,
    grib_grib2_read_laea,
    grib_grib2_read_template_5_4_grid_point_ieee_floating_point,
    grib_grib2_read_section_5_nbits_zero_decimal_scaled,
    grib_grib2_read_spatial_differencing_order_1,
    grib_grib2_write_creation_options,
    grib_grib2_write_projections,
    grib_grib2_write_data_encodings,
    grib_grib2_write_data_encodings_warnings_and_errors,
    grib_grib2_write_temperatures,
    grib_online_grib2_jpeg2000_single_line
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'grib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

