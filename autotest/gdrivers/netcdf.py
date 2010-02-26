#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NetCDF driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
import gdal
import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.

def netcdf_1():

    gdaltest.netcdf_drv = gdal.GetDriverByName( 'NETCDF' )

    if gdaltest.netcdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'NetCDF', 'NETCDF:"data/bug636.nc":tas', 1, 31621,
                             filename_absolute = 1 )

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = tst.testOpen()
    gdal.PopErrorHandler()

    return result

###############################################################################
# Verify a simple createcopy operation.  We can't do the trivial gdaltest
# operation because the new file will only be accessable via subdatasets!

def netcdf_2():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    src_ds = gdal.Open( 'data/byte.tif' )
    
    base_ds = gdaltest.netcdf_drv.CreateCopy( 'tmp/netcdf2.nc', src_ds)

    tst = gdaltest.GDALTest( 'NetCDF', 'tmp/netcdf2.nc',
                             1, 4672,
                             filename_absolute = 1 )

    wkt = """PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982139006,
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
    AUTHORITY["EPSG","26711"]]"""

    result = tst.testOpen( check_prj = wkt )

    if result != 'success':
        return result

    base_ds = None
    gdaltest.clean_tmp()

    return 'success'

###############################################################################

def netcdf_3():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/sombrero.grd' )
    bnd = ds.GetRasterBand(1)
    minmax = bnd.ComputeRasterMinMax()

    if abs(minmax[0] - (-0.675758)) > 0.000001 or abs(minmax[1] - 1.0) > 0.000001:
        gdaltest.post_reason( 'Wrong min or max.' )
        return 'fail'

    bnd = None
    ds = None

    return 'success'
    
###############################################################################
# In #2582 5dimensional files were causing problems.  Verify use ok.

def netcdf_4():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'NetCDF',
                             'NETCDF:data/foo_5dimensional.nc:temperature',
                             3, 1218, filename_absolute = 1 )

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = tst.testOpen()
    gdal.PopErrorHandler()

    return result
    
###############################################################################
# In #2583 5dimensional files were having problems unrolling the highest
# dimension - check handling now on band 7.

def netcdf_5():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'NetCDF',
                             'NETCDF:data/foo_5dimensional.nc:temperature',
                             7, 1227, filename_absolute = 1 )

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = tst.testOpen()
    gdal.PopErrorHandler()

    return result

###############################################################################
#ticket #3324 check spatial reference reading for cf-1.4 lambert conformal
#1 standard parallel.
def netcdf_6():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/cf_lcc1sp.nc' )
    prj = ds.GetProjection( )

    sr = osr.SpatialReference( )
    sr.ImportFromWkt( prj )
    lat_origin = sr.GetProjParm( 'latitude_of_origin' )

    if lat_origin != 25:
        gdaltest.post_reason( 'Latitude of origin does not match expected:\n%f' 
                              % lat_origin )
        return 'fail'

    ds = None

    return 'success'
    
###############################################################################
#ticket #3324 check spatial reference reading for cf-1.4 lambert conformal
#2 standard parallels.
def netcdf_7():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/cf_lcc2sp.nc' )
    prj = ds.GetProjection( )

    sr = osr.SpatialReference( )
    sr.ImportFromWkt( prj )
    std_p1 = sr.GetProjParm( 'standard_parallel_1' )
    std_p2 = sr.GetProjParm( 'standard_parallel_2' )

    if std_p1 != 33.0 or std_p2 != 45.0:
        gdaltest.post_reason( 'Standard Parallels do not match expected:\n%f,%f' 
                              % ( std_p1, std_p2 ) )
        return 'fail'

    ds = None
    sr = None

    return 'success'
    
###############################################################################
#check for cf convention read of albers equal area
def netcdf_8():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/cf_aea2sp_invf.nc' )
    prj = ds.GetProjection( )

    if prj != 'PROJCS["unnamed",GEOGCS["unknown",DATUM["unknown",SPHEROID["Spheroid",6378140,298.257]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Albers_Conic_Equal_Area"],PARAMETER["standard_parallel_1",29.8333],PARAMETER["standard_parallel_2",45.8333],PARAMETER["latitude_of_center",37.5],PARAMETER["longitude_of_center",-96],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]':

        gdaltest.post_reason( 'Projection does not match expected:\n%s' % ( prj ) )
        return 'fail'

    ds = None

    return 'success'
    
###############################################################################
#check to see if projected systems default to wgs84 if no spheroid def
def netcdf_9():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/cf_no_sphere.nc' )

    prj = ds.GetProjection( )

    sr = osr.SpatialReference( )
    sr.ImportFromWkt( prj )
    spheroid = sr.GetAttrValue( 'SPHEROID' )

    if spheroid != 'WGS 84':
        gdaltest.post_reason( 'Incorrect spheroid read from file\n%s' 
                              % ( spheroid ) )
        return 'fail'

    ds = None
    sr = None

    return 'success'
    
###############################################################################


gdaltest_list = [
    netcdf_1,
    netcdf_2,
    netcdf_3,
    netcdf_4,
    netcdf_5,
    netcdf_6,
    netcdf_7,
    netcdf_8,
    netcdf_9 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'netcdf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

