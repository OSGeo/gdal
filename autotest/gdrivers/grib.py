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
import shutil
from osgeo import gdal

sys.path.append( '../pymod' )
sys.path.append( '../osr' )

import gdaltest

def has_jp2kdrv():
    for i in range(gdal.GetDriverCount()):
        if gdal.GetDriver(i).ShortName.startswith('JP2'):
            return True
    return False

###############################################################################
# Do a simple checksum on our test file (with a faked imagery.tif).

def grib_1():

    gdaltest.grib_drv = gdal.GetDriverByName('GRIB')
    if gdaltest.grib_drv is None:
        return 'skip'

    # Test proj4 presence
    import osr_ct
    osr_ct.osr_ct_1()

    tst = gdaltest.GDALTest( 'GRIB', 'ds.mint.bin', 2, 46927 )
    return tst.testOpen()


###############################################################################
# Test a small GRIB 1 sample file.

def grib_2():

    if gdaltest.grib_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'GRIB', 'Sample_QuikSCAT.grb', 4, 50714 )
    return tst.testOpen()

###############################################################################
# This file has different raster sizes for some of the products, which
# we sort-of-support per ticket Test a small GRIB 1 sample file.

def grib_read_different_sizes_messages():

    if gdaltest.grib_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'GRIB', 'bug3246.grb', 4, 4081 )
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

    ds = gdal.Open('data/ds.mint.bin')
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

    shutil.copy('data/ds.mint.bin', 'tmp/ds.mint.bin')
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

    ds = gdal.Open('data/one_one.grib2')
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

    ds = gdal.Open('/vsizip/data/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Write PDS numbers to all bands

def grib_grib2_test_grib_pds_all_bands():

    if gdaltest.grib_drv is None:
        return 'skip'
    ds = gdal.Open('/vsizip/data/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
    if ds is None:
        return 'fail'
    band = ds.GetRasterBand(2)
    md = band.GetMetadataItem('GRIB_PDS_TEMPLATE_NUMBERS')
    ds = None
    if md is None:
        gdaltest.post_reason('Failed to fetch pds numbers (#5144)')
        return 'fail'

    gdal.SetConfigOption('GRIB_PDS_ALL_BANDS', 'OFF')
    ds = gdal.Open('/vsizip/data/gfs.t00z.mastergrb2f03.zip/gfs.t00z.mastergrb2f03')
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

    ret, err = gdaltest.runexternal_out_and_err (test_cli_utilities.get_gdalinfo_path() + ' data/template4_15.grib -checksum')

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

    ds = gdal.Open('data/MRMS_EchoTop_18_00.50_20161015-133230.grib2')
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
    ds = gdal.Open('data/twenty-se27w.2017102006.hwrfsat.core.0p02.f000_truncated.grb2')
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
    ds = gdal.Open('data/CMC_rdwps_lake-erie_ICEC_SFC_0_latlon0.05x0.05_2017111800_P000.grib2')
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

    ds = gdal.Open('/vsisparse/data/rotated_pole.grb.xml')

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
    ds = gdal.Open('data/template_4_40.grb2')
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
        ds = gdal.Open('data/template_4_65535.grb2')
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

    ds = gdal.Open('data/transverse_mercator.grb2')

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

    ds = gdal.Open('data/mercator.grb2')

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

    ds = gdal.Open('data/mercator_2sp.grb2')

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

    ds = gdal.Open('data/lambert_conformal_conic.grb2')

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

    ds = gdal.Open('data/polar_stereographic.grb2')

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

    ds = gdal.Open('data/albers_equal_area.grb2')

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

    ds = gdal.Open('data/lambert_azimuthal_equal_area.grb2')

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

    ds = gdal.Open('data/ieee754_single.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4727:
        gdaltest.post_reason('Did not get expected checksum')
        print(cs)
        return 'fail'

    ds = gdal.Open('data/ieee754_double.grb2')
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

    ds = gdal.Open('data/simple_packing_nbits_zero_decimal_scaled.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 5:
        gdaltest.post_reason('Did not get expected checksum')
        print(cs)
        return 'fail'

    if gdal.GetDriverByName('PNG') is not None:
        ds = gdal.Open('data/png_nbits_zero_decimal_scaled.grb2')
        cs = ds.GetRasterBand(1).Checksum()
        if cs != 5:
            gdaltest.post_reason('Did not get expected checksum')
            print(cs)
            return 'fail'

    if has_jp2kdrv():
        ds = gdal.Open('data/jpeg2000_nbits_zero_decimal_scaled.grb2')
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

    ds = gdal.Open('data/spatial_differencing_order_1.grb2')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 46650:
        gdaltest.post_reason('Did not get expected checksum')
        print(cs)
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
    grib_online_grib2_jpeg2000_single_line
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'grib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

