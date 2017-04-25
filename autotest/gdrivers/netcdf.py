#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NetCDF driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2016, Even Rouault <even.rouault at spatialys.com>
# Copyright (c) 2010, Kyle Shannon <kyle at pobox dot com>
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
from osgeo import ogr
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

import test_cli_utilities

###############################################################################
# Netcdf Functions
###############################################################################

###############################################################################
# Get netcdf version and test for supported files

def netcdf_setup():

    gdaltest.netcdf_drv_version = 'unknown'
    gdaltest.netcdf_drv_has_nc2 = False
    gdaltest.netcdf_drv_has_nc4 = False
    gdaltest.netcdf_drv_has_hdf4 = False
    gdaltest.netcdf_drv_silent = False

    gdaltest.netcdf_drv = gdal.GetDriverByName( 'NETCDF' )

    if gdaltest.netcdf_drv is None:
        print('NOTICE: netcdf not supported, skipping checks')
        return 'skip'

    #get capabilities from driver
    metadata = gdaltest.netcdf_drv.GetMetadata()
    if metadata is None:
        print('NOTICE: netcdf metadata not found, skipping checks')
        return 'skip'

    #netcdf library version "3.6.3" of Dec 22 2009 06:10:17 $
    #netcdf library version 4.1.1 of Mar  4 2011 12:52:19 $
    if 'NETCDF_VERSION' in metadata:
        v = metadata['NETCDF_VERSION']
        v = v[ 0 : v.find(' ') ].strip('"')
        gdaltest.netcdf_drv_version = v

    if 'NETCDF_HAS_NC2' in metadata \
       and metadata['NETCDF_HAS_NC2'] == 'YES':
        gdaltest.netcdf_drv_has_nc2 = True

    if 'NETCDF_HAS_NC4' in metadata \
       and metadata['NETCDF_HAS_NC4'] == 'YES':
        gdaltest.netcdf_drv_has_nc4 = True

    if 'NETCDF_HAS_HDF4' in metadata \
       and metadata['NETCDF_HAS_HDF4'] == 'YES':
        gdaltest.netcdf_drv_has_hdf4 = True

    print( 'NOTICE: using netcdf version ' + gdaltest.netcdf_drv_version + \
               '  has_nc2: '+str(gdaltest.netcdf_drv_has_nc2)+'  has_nc4: ' + \
               str(gdaltest.netcdf_drv_has_nc4) )

    return 'success'

###############################################################################
# test file copy
# helper function needed so we can call Process() on it from netcdf_test_copy_timeout()
def netcdf_test_copy( ifile, band, checksum, ofile, opts=[], driver='NETCDF' ):
    test = gdaltest.GDALTest( 'NETCDF', '../'+ifile, band, checksum, options=opts )
    return test.testCreateCopy(check_gt=0, check_srs=0, new_filename=ofile, delete_copy = 0, check_minmax = 0)

###############################################################################
#test file copy, optional timeout arg
def netcdf_test_copy_timeout( ifile, band, checksum, ofile, opts=[], driver='NETCDF', timeout=None ):

    from multiprocessing import Process

    result = 'success'

    drv = gdal.GetDriverByName( driver )

    if os.path.exists( ofile ):
        drv.Delete( ofile )

    if timeout is None:
        result = netcdf_test_copy( ifile, band, checksum, ofile, opts, driver )

    else:
        sys.stdout.write('.')
        sys.stdout.flush()

        proc = Process( target=netcdf_test_copy, args=(ifile, band, checksum, ofile, opts ) )
        proc.start()
        proc.join( timeout )

        # if proc is alive after timeout we must terminate it, and return fail
        # valgrind detects memory leaks when this occurs (although it should never happen)
        if proc.is_alive():
            proc.terminate()
            if os.path.exists( ofile ):
                drv.Delete( ofile )
            print('testCreateCopy() for file %s has reached timeout limit of %d seconds' % (ofile, timeout) )
            result = 'fail'

    return result

###############################################################################
#check support for DEFLATE compression, requires HDF5 and zlib
def netcdf_test_deflate( ifile, checksum, zlevel=1, timeout=None ):

    try:
        from multiprocessing import Process
        Process.is_alive
    except:
        print('from multiprocessing import Process failed')
        return 'skip'

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    ofile1 = 'tmp/' + os.path.basename(ifile) + '-1.nc'
    ofile1_opts = [ 'FORMAT=NC4C', 'COMPRESS=NONE']
    ofile2 = 'tmp/' + os.path.basename(ifile) + '-2.nc'
    ofile2_opts = [ 'FORMAT=NC4C', 'COMPRESS=DEFLATE', 'ZLEVEL='+str(zlevel) ]

    if not os.path.exists( ifile ):
        gdaltest.post_reason( 'ifile %s does not exist' % ifile )
        return 'fail'

    result1 = netcdf_test_copy_timeout( ifile, 1, checksum, ofile1, ofile1_opts, 'NETCDF', timeout )

    result2 = netcdf_test_copy_timeout( ifile, 1, checksum, ofile2, ofile2_opts, 'NETCDF', timeout )

    if result1 == 'fail' or result2 == 'fail':
        return 'fail'

    # make sure compressed file is smaller than uncompressed files
    try:
        size1 = os.path.getsize( ofile1 )
        size2 = os.path.getsize( ofile2 )
    except:
        gdaltest.post_reason( 'Error getting file sizes.' )
        return 'fail'

    if  size2 >= size1:
        gdaltest.post_reason( 'Compressed file is not smaller than reference, check your netcdf-4, HDF5 and zlib installation' )
        return 'fail'

    return 'success'

###############################################################################
# check support for reading attributes (single values and array values)
def netcdf_check_vars( ifile, vals_global=None, vals_band=None ):

    src_ds = gdal.Open( ifile )

    if src_ds is None:
        gdaltest.post_reason( 'could not open dataset ' + ifile )
        return 'fail'

    metadata_global = src_ds.GetMetadata()
    if metadata_global is None:
        gdaltest.post_reason( 'could not get global metadata from ' + ifile )
        return 'fail'

    missval = src_ds.GetRasterBand(1).GetNoDataValue()
    if missval != 1:
        gdaltest.post_reason( 'got invalid nodata value %s for Band' % str(missval) )
        return 'fail'

    metadata_band = src_ds.GetRasterBand(1).GetMetadata()
    if metadata_band is None:
        gdaltest.post_reason( 'could not get Band metadata' )
        return 'fail'


    metadata = metadata_global
    vals = vals_global
    if vals is None:
        vals = dict()
    for k, v in vals.items():
        if not k in metadata:
            gdaltest.post_reason("missing metadata [%s]" % (str(k)))
            return 'fail'
        # strip { and } as new driver uses these for array values
        mk = metadata[k].lstrip('{ ').rstrip('} ')
        if mk != v:
            gdaltest.post_reason("invalid value [%s] for metadata [%s]=[%s]" \
                                     % (str(mk),str(k),str(v)))
            return 'fail'

    metadata = metadata_band
    vals = vals_band
    if vals is None:
        vals = dict()
    for k, v in vals.items():
        if not k in metadata:
            gdaltest.post_reason("missing metadata [%s]" % (str(k)))
            return 'fail'
        # strip { and } as new driver uses these for array values
        mk = metadata[k].lstrip('{ ').rstrip('} ')
        if mk != v:
            gdaltest.post_reason("invalid value [%s] for metadata [%s]=[%s]" \
                                     % (str(mk),str(k),str(v)))
            return 'fail'

    return 'success'


###############################################################################
# Netcdf Tests
###############################################################################

###############################################################################
# Perform simple read test.

def netcdf_1():

    #setup netcdf environment
    netcdf_setup()

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
# operation because the new file will only be accessible via subdatasets.

def netcdf_2():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    src_ds = gdal.Open( 'data/byte.tif' )

    gdaltest.netcdf_drv.CreateCopy( 'tmp/netcdf2.nc', src_ds)

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

    # Test that in raster-only mode, update isn't supported (not sure what would be missing for that...)
    with gdaltest.error_handler():
        ds = gdal.Open( 'tmp/netcdf2.nc', gdal.GA_Update )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    #don't test for checksum (see bug #4284)
    result = tst.testOpen(skip_checksum = True)
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
    #don't test for checksum (see bug #4284)
    result = tst.testOpen(skip_checksum = True)
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
# Previous version compared entire wkt, which varies slightly among driver versions
# now just look for PROJECTION=Albers_Conic_Equal_Area and some parameters
def netcdf_8():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/cf_aea2sp_invf.nc' )

    srs = osr.SpatialReference( )
    srs.ImportFromWkt( ds.GetProjection( ) )

    proj = srs.GetAttrValue( 'PROJECTION' )
    if  proj != 'Albers_Conic_Equal_Area':
        gdaltest.post_reason( 'Projection does not match expected : ' + proj )
        return 'fail'

    param = srs.GetProjParm('latitude_of_center')
    if param != 37.5:
        gdaltest.post_reason( 'Got wrong parameter value (%g)' % param )
        return 'fail'

    param = srs.GetProjParm('longitude_of_center')
    if param != -96:
        gdaltest.post_reason( 'Got wrong parameter value (%g)' % param )
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
#check if km pixel size makes it through to gt
def netcdf_10():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/cf_no_sphere.nc' )

    prj = ds.GetProjection( )

    gt = ds.GetGeoTransform( )

    gt1 = ( -1897186.0290038721, 5079.3608398440065,
            0.0,2674684.0244560046,
            0.0,-5079.4721679684635 )
    gt2 = ( -1897.186029003872, 5.079360839844003,
             0.0, 2674.6840244560044,
             0.0,-5.079472167968456 )

    if gt != gt1:
        sr = osr.SpatialReference()
        sr.ImportFromWkt( prj )
        #new driver uses UNIT vattribute instead of scaling values
        if not (sr.GetAttrValue("PROJCS|UNIT",1)=="1000" and gt == gt2) :
            gdaltest.post_reason( 'Incorrect geotransform, got '+str(gt) )
            return 'fail'

    ds = None

    return 'success'

###############################################################################
#check if ll gets caught in km pixel size check
def netcdf_11():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/cf_geog.nc' )

    gt = ds.GetGeoTransform( )

    if gt != (-0.5, 1.0, 0.0, 10.5, 0.0, -1.0):

        gdaltest.post_reason( 'Incorrect geotransform' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
#check for scale/offset set/get.
def netcdf_12():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/scale_offset.nc' )

    scale = ds.GetRasterBand( 1 ).GetScale()
    offset = ds.GetRasterBand( 1 ).GetOffset()

    if scale != 0.01 or offset != 1.5:
        gdaltest.post_reason( 'Incorrect scale(%f) or offset(%f)' % ( scale, offset ) )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
#check for scale/offset = None if no scale or offset is available
def netcdf_13():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/no_scale_offset.nc' )

    scale = ds.GetRasterBand( 1 ).GetScale()
    offset = ds.GetRasterBand( 1 ).GetOffset()

    if scale != None or offset != None:
        gdaltest.post_reason( 'Incorrect scale or offset' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
#check for scale/offset for two variables
def netcdf_14():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'NETCDF:data/two_vars_scale_offset.nc:z' )

    scale = ds.GetRasterBand( 1 ).GetScale()
    offset = ds.GetRasterBand( 1 ).GetOffset()

    if scale != 0.01 or offset != 1.5:
        gdaltest.post_reason( 'Incorrect scale(%f) or offset(%f)' % ( scale, offset ) )
        return 'fail'

    ds = None

    ds = gdal.Open( 'NETCDF:data/two_vars_scale_offset.nc:q' )

    scale = ds.GetRasterBand( 1 ).GetScale()
    offset = ds.GetRasterBand( 1 ).GetOffset()

    scale = ds.GetRasterBand( 1 ).GetScale()
    offset = ds.GetRasterBand( 1 ).GetOffset()

    if scale != 0.1 or offset != 2.5:
        gdaltest.post_reason( 'Incorrect scale(%f) or offset(%f)' % ( scale, offset ) )
        return 'fail'

    return 'success'

###############################################################################
#check support for netcdf-2 (64 bit)
# This test fails in 1.8.1, because the driver does not support NC2 (bug #3890)
def netcdf_15():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if gdaltest.netcdf_drv_has_nc2:
        ds = gdal.Open( 'data/trmm-nc2.nc' )
        if ds is None:
            return 'fail'
        else:
            ds = None
            return 'success'
    else:
        return 'skip'

    return 'success'

###############################################################################
#check support for netcdf-4
def netcdf_16():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/trmm-nc4.nc'

    if gdaltest.netcdf_drv_has_nc4:

        # test with Open()
        ds = gdal.Open( ifile )
        if ds is None:
            gdaltest.post_reason('GDAL did not open file')
            return 'fail'
        else:
            name = ds.GetDriver().GetDescription()
            ds = None
            #return fail if did not open with the netCDF driver (i.e. HDF5Image)
            if name != 'netCDF':
                gdaltest.post_reason('netcdf driver did not open file')
                return 'fail'

        # test with Identify()
        name = gdal.IdentifyDriver( ifile ).GetDescription()
        if name != 'netCDF':
            gdaltest.post_reason('netcdf driver did not identify file')
            return 'fail'

    else:
        return 'skip'

    return 'success'

###############################################################################
#check support for netcdf-4 - make sure hdf5 is not read by netcdf driver
def netcdf_17():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/groups.h5'

    #skip test if Hdf5 is not enabled
    if gdal.GetDriverByName( 'HDF5' ) is None and \
            gdal.GetDriverByName( 'HDF5Image' ) is None:
        return 'skip'

    if gdaltest.netcdf_drv_has_nc4:

        #test with Open()
        ds = gdal.Open( ifile )
        if ds is None:
            gdaltest.post_reason('GDAL did not open hdf5 file')
            return 'fail'
        else:
            name = ds.GetDriver().GetDescription()
            ds = None
                #return fail if opened with the netCDF driver
            if name == 'netCDF':
                gdaltest.post_reason('netcdf driver opened hdf5 file')
                return 'fail'

        # test with Identify()
        name = gdal.IdentifyDriver( ifile ).GetDescription()
        if name == 'netCDF':
            gdaltest.post_reason('netcdf driver was identified for hdf5 file')
            return 'fail'

    else:
        return 'skip'

    return 'success'

###############################################################################
#check support for netcdf-4 classic (NC4C)
def netcdf_18():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/trmm-nc4c.nc'

    if gdaltest.netcdf_drv_has_nc4:

        # test with Open()
        ds = gdal.Open( ifile )
        if ds is None:
            return 'fail'
        else:
            name = ds.GetDriver().GetDescription()
            ds = None
            #return fail if did not open with the netCDF driver (i.e. HDF5Image)
            if name != 'netCDF':
                return 'fail'

        # test with Identify()
        name = gdal.IdentifyDriver( ifile ).GetDescription()
        if name != 'netCDF':
            return 'fail'

    else:
        return 'skip'

    return 'success'

###############################################################################
#check support for reading with DEFLATE compression, requires NC4
def netcdf_19():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    tst =  gdaltest.GDALTest( 'NetCDF', 'data/trmm-nc4z.nc', 1, 50235,
                              filename_absolute = 1 )

    result = tst.testOpen(skip_checksum = True)

    return result

###############################################################################
#check support for writing with DEFLATE compression, requires NC4
def netcdf_20():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    #simple test with tiny file
    return netcdf_test_deflate( 'data/utm.tif', 50235 )


###############################################################################
#check support for writing large file with DEFLATE compression
#if chunking is not defined properly within the netcdf driver, this test can take 1h
def netcdf_21():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    if not gdaltest.run_slow_tests():
        return 'skip'

    bigfile = 'tmp/cache/utm-big.tif'

    sys.stdout.write('.')
    sys.stdout.flush()

    #create cache dir if absent
    if not os.path.exists( 'tmp/cache' ):
        os.mkdir( 'tmp/cache' )

    #look for large gtiff in cache
    if not os.path.exists( bigfile ):

        #create large gtiff
        if test_cli_utilities.get_gdalwarp_path() is None:
            gdaltest.post_reason('gdalwarp not found')
            return 'skip'

        warp_cmd = test_cli_utilities.get_gdalwarp_path() +\
            ' -q -overwrite -r bilinear -ts 7680 7680 -of gtiff ' +\
            'data/utm.tif ' + bigfile

        try:
            (ret, err) = gdaltest.runexternal_out_and_err( warp_cmd )
        except:
            gdaltest.post_reason('gdalwarp execution failed')
            return 'fail'

        if ( err != '' or ret != '' ):
            gdaltest.post_reason('gdalwarp returned error\n'+str(ret)+' '+str(err))
            return 'fail'

    # test compression of the file, with a conservative timeout of 60 seconds
    return netcdf_test_deflate( bigfile, 26695, 6, 60 )


###############################################################################
#check support for hdf4
def netcdf_22():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_hdf4:
        return 'skip'

    ifile = 'data/hdifftst2.hdf'

    #suppress warning
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open( 'NETCDF:' + ifile )
    gdal.PopErrorHandler()

    if ds is None:
        gdaltest.post_reason('netcdf driver did not open hdf4 file')
        return 'fail'
    else:
        ds = None

    return 'success'

###############################################################################
#check support for hdf4 - make sure  hdf4 file is not read by netcdf driver
def netcdf_23():

    #don't skip if netcdf is not enabled in GDAL
    #if gdaltest.netcdf_drv is None:
    #    return 'skip'
    #if not gdaltest.netcdf_drv_has_hdf4:
    #    return 'skip'

    #skip test if Hdf4 is not enabled in GDAL
    if gdal.GetDriverByName( 'HDF4' ) is None and \
            gdal.GetDriverByName( 'HDF4Image' ) is None:
        return 'skip'

    ifile = 'data/hdifftst2.hdf'

    #test with Open()
    ds = gdal.Open( ifile )
    if ds is None:
        gdaltest.post_reason('GDAL did not open hdf4 file')
        return 'fail'
    else:
        name = ds.GetDriver().GetDescription()
        ds = None
        #return fail if opened with the netCDF driver
        if name == 'netCDF':
            gdaltest.post_reason('netcdf driver opened hdf4 file')
            return 'fail'

    # test with Identify()
    name = gdal.IdentifyDriver( ifile ).GetDescription()
    if name == 'netCDF':
        gdaltest.post_reason('netcdf driver was identified for hdf4 file')
        return 'fail'

    return 'success'

###############################################################################
# check support for reading attributes (single values and array values)
def netcdf_24():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    vals_global = {'NC_GLOBAL#test': 'testval',
                   'NC_GLOBAL#valid_range_i': '0,255',
                   'NC_GLOBAL#valid_min': '10.1',
                   'NC_GLOBAL#test_b': '1'}
    vals_band = {'_Unsigned': 'true',
                 'valid_min': '10.1',
                 'valid_range_b': '1,10',
                 'valid_range_d': '0.1111112222222,255.555555555556',
                 'valid_range_f': '0.1111111,255.5556',
                 'valid_range_s': '0,255'}

    return netcdf_check_vars( 'data/nc_vars.nc', vals_global, vals_band )

###############################################################################
# check support for NC4 reading attributes (single values and array values)
def netcdf_24_nc4():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    vals_global = {'NC_GLOBAL#test': 'testval',
                   'NC_GLOBAL#test_string': 'testval_string',
                   'NC_GLOBAL#valid_range_i': '0,255',
                   'NC_GLOBAL#valid_min': '10.1',
                   'NC_GLOBAL#test_b': '-100',
                   'NC_GLOBAL#test_ub': '200',
                   'NC_GLOBAL#test_s': '-16000',
                   'NC_GLOBAL#test_us': '32000',
                   'NC_GLOBAL#test_l': '-2000000000',
                   'NC_GLOBAL#test_ul': '4000000000'}
    vals_band = {'test_string_arr': 'test,string,arr',
                 'valid_min': '10.1',
                 'valid_range_b': '1,10',
                 'valid_range_ub': '1,200',
                 'valid_range_s': '0,255',
                 'valid_range_us': '0,32000',
                 'valid_range_l': '0,255',
                 'valid_range_ul': '0,4000000000',
                 'valid_range_d': '0.1111112222222,255.555555555556',
                 'valid_range_f': '0.1111111,255.5556',
                 'valid_range_s': '0,255'}

    return netcdf_check_vars( 'data/nc4_vars.nc', vals_global, vals_band )

###############################################################################
# check support for writing attributes (single values and array values)
def netcdf_25():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    result = netcdf_test_copy( 'data/nc_vars.nc', 1, None, 'tmp/netcdf_25.nc' )
    if result != 'success':
        return result

    vals_global = {'NC_GLOBAL#test': 'testval',
                   'NC_GLOBAL#valid_range_i': '0,255',
                   'NC_GLOBAL#valid_min': '10.1',
                   'NC_GLOBAL#test_b': '1'}
    vals_band = {'_Unsigned': 'true',
                 'valid_min': '10.1',
                 'valid_range_b': '1,10',
                 'valid_range_d': '0.1111112222222,255.555555555556',
                 'valid_range_f': '0.1111111,255.5556',
                 'valid_range_s': '0,255'}

    return netcdf_check_vars( 'tmp/netcdf_25.nc', vals_global, vals_band )

###############################################################################
# check support for NC4 writing attributes (single values and array values)
def netcdf_25_nc4():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    result = netcdf_test_copy( 'data/nc4_vars.nc', 1, None, 'tmp/netcdf_25_nc4.nc', [ 'FORMAT=NC4' ] )
    if result != 'success':
        return result

    vals_global = {'NC_GLOBAL#test': 'testval',
                   'NC_GLOBAL#test_string': 'testval_string',
                   'NC_GLOBAL#valid_range_i': '0,255',
                   'NC_GLOBAL#valid_min': '10.1',
                   'NC_GLOBAL#test_b': '-100',
                   'NC_GLOBAL#test_ub': '200',
                   'NC_GLOBAL#test_s': '-16000',
                   'NC_GLOBAL#test_us': '32000',
                   'NC_GLOBAL#test_l': '-2000000000',
                   'NC_GLOBAL#test_ul': '4000000000'}
    vals_band = {'test_string_arr': 'test,string,arr',
                 'valid_min': '10.1',
                 'valid_range_b': '1,10',
                 'valid_range_ub': '1,200',
                 'valid_range_s': '0,255',
                 'valid_range_us': '0,32000',
                 'valid_range_l': '0,255',
                 'valid_range_ul': '0,4000000000',
                 'valid_range_d': '0.1111112222222,255.555555555556',
                 'valid_range_f': '0.1111111,255.5556',
                 'valid_range_s': '0,255'}

    return netcdf_check_vars( 'tmp/netcdf_25_nc4.nc', vals_global, vals_band )

###############################################################################
# check support for WRITE_BOTTOMUP file creation option
# use a dummy file with no lon/lat info to force a different checksum
# depending on y-axis order
def netcdf_26():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    #test default config
    test = gdaltest.GDALTest( 'NETCDF', '../data/int16-nogeo.nc', 1, 4672 )
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = test.testCreateCopy(check_gt=0, check_srs=0, check_minmax = 0)
    gdal.PopErrorHandler()

    if result != 'success':
        print('failed create copy without WRITE_BOTTOMUP')
        return result

    #test WRITE_BOTTOMUP=NO
    test = gdaltest.GDALTest( 'NETCDF', '../data/int16-nogeo.nc', 1, 4855,
                              options=['WRITE_BOTTOMUP=NO'] )
    result = test.testCreateCopy(check_gt=0, check_srs=0, check_minmax = 0)

    if result != 'success':
        print('failed create copy with WRITE_BOTTOMUP=NO')
        return result

    return 'success'

###############################################################################
# check support for GDAL_NETCDF_BOTTOMUP configuration option
def netcdf_27():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    #test default config
    test = gdaltest.GDALTest( 'NETCDF', '../data/int16-nogeo.nc', 1, 4672 )
    config_bak = gdal.GetConfigOption( 'GDAL_NETCDF_BOTTOMUP' )
    gdal.SetConfigOption( 'GDAL_NETCDF_BOTTOMUP', None )
    result = test.testOpen()
    gdal.SetConfigOption( 'GDAL_NETCDF_BOTTOMUP', config_bak )

    if result != 'success':
        print('failed open without GDAL_NETCDF_BOTTOMUP')
        return result

    #test GDAL_NETCDF_BOTTOMUP=NO
    test = gdaltest.GDALTest( 'NETCDF', '../data/int16-nogeo.nc', 1, 4855 )
    config_bak = gdal.GetConfigOption( 'GDAL_NETCDF_BOTTOMUP' )
    gdal.SetConfigOption( 'GDAL_NETCDF_BOTTOMUP', 'NO' )
    result = test.testOpen()
    gdal.SetConfigOption( 'GDAL_NETCDF_BOTTOMUP', config_bak )

    if result != 'success':
        print('failed open with GDAL_NETCDF_BOTTOMUP')
        return result

    return 'success'

###############################################################################
# check support for writing multi-dimensional files (helper function)
def netcdf_test_4dfile( ofile ):

    # test result file has 8 bands and 0 subdasets (instead of 0 bands and 8 subdatasets)
    ds = gdal.Open( ofile )
    if ds is None:
        gdaltest.post_reason( 'open of copy failed' )
        return 'fail'
    md = ds.GetMetadata( 'SUBDATASETS' )
    subds_count = 0
    if not md is None:
        subds_count = len(md) / 2
    if ds.RasterCount != 8 or subds_count != 0:
        gdaltest.post_reason( 'copy has %d bands (expected 8) and has %d subdatasets'\
                                  ' (expected 0)' % (ds.RasterCount, subds_count ) )
        return 'fail'
    ds is None

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except:
        print('NOTICE: ncdump not found')
        return 'success'
    if err == None or not 'netcdf library version' in err:
        print('NOTICE: ncdump not found')
        return 'success'
    (ret, err) = gdaltest.runexternal_out_and_err( 'ncdump -h '+ ofile )
    if ret == '' or err != '':
        gdaltest.post_reason( 'ncdump failed' )
        return 'fail'

    # simple dimension tests using ncdump output
    err = ""
    if not 'int t(time, levelist, lat, lon) ;' in ret:
        err = err + 'variable (t) has wrong dimensions or is missing\n'
    if not 'levelist = 2 ;' in ret:
        err = err + 'levelist dimension is missing or incorrect\n'
    if not 'int levelist(levelist) ;' in ret:
        err = err + 'levelist variable is missing or incorrect\n'
    if not 'time = 4 ;' in ret:
        err = err + 'time dimension is missing or incorrect\n'
    if not 'double time(time) ;' in ret:
        err = err + 'time variable is missing or incorrect\n'
    # uncomment this to get full header in output
    #if err != '':
    #    err = err + ret
    if err != '':
        gdaltest.post_reason( err )
        return 'fail'

    return 'success'

###############################################################################
# check support for writing multi-dimensional files using CreateCopy()
def netcdf_28():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/netcdf-4d.nc'
    ofile = 'tmp/netcdf_28.nc'

    # copy file
    result = netcdf_test_copy( ifile, 0, None, ofile )
    if result != 'success':
        return 'fail'

    # test file
    return netcdf_test_4dfile( ofile )

###############################################################################
# Check support for writing multi-dimensional files using gdalwarp.
# Requires metadata copy support in gdalwarp (see bug #3898).
# First create a vrt file using gdalwarp, then copy file to netcdf.
# The workaround is (currently ??) necessary because dimension rolling code is
# in netCDFDataset::CreateCopy() and necessary dimension metadata
# is not saved to netcdf when using gdalwarp (as the driver does not write
# metadata to netcdf file with SetMetadata() and SetMetadataItem()).
def netcdf_29():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    # create tif file using gdalwarp
    if test_cli_utilities.get_gdalwarp_path() is None:
        gdaltest.post_reason('gdalwarp not found')
        return 'skip'

    ifile = 'data/netcdf-4d.nc'
    ofile1 = 'tmp/netcdf_29.vrt'
    ofile = 'tmp/netcdf_29.nc'

    warp_cmd = '%s -q -overwrite -of vrt %s %s' %\
        ( test_cli_utilities.get_gdalwarp_path(), ifile, ofile1 )
    try:
        (ret, err) = gdaltest.runexternal_out_and_err( warp_cmd )
    except:
        gdaltest.post_reason('gdalwarp execution failed')
        return 'fail'

    if ( err != '' or ret != '' ):
        gdaltest.post_reason('gdalwarp returned error\n'+str(ret)+' '+str(err))
        return 'fail'

    # copy vrt to netcdf, with proper dimension rolling
    result = netcdf_test_copy( ofile1, 0, None, ofile )
    if result != 'success':
        return 'fail'

    # test file
    result = netcdf_test_4dfile( ofile )
    if result == 'fail':
        print('test failed - does gdalwarp support metadata copying?')

    return result

###############################################################################
# check support for file with nan values (bug #4705)
def netcdf_30():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'NetCDF', 'trmm-nan.nc', 1, 62519 )

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    result = tst.testOpen()
    gdal.PopErrorHandler()

    return result

###############################################################################
#check if 2x2 file has proper geotransform
#1 pixel (in width or height) still unsupported because we can't get the pixel dimensions
def netcdf_31():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/trmm-2x2.nc' )

    ds.GetProjection( )

    gt = ds.GetGeoTransform( )

    gt1 = ( -80.0, 0.25, 0.0, -19.5, 0.0, -0.25 )

    if gt != gt1:
        gdaltest.post_reason( 'Incorrect geotransform, got '+str(gt) )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test NC_UBYTE write/read - netcdf-4 (FORMAT=NC4) only (#5053)

def netcdf_32():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    ifile = 'data/byte.tif'
    ofile = 'tmp/netcdf_32.nc'

    #gdal.SetConfigOption('CPL_DEBUG', 'ON')

    # test basic read/write
    result = netcdf_test_copy( ifile, 1, 4672, ofile, [ 'FORMAT=NC4' ] )
    if result != 'success':
        return 'fail'
    result = netcdf_test_copy( ifile, 1, 4672, ofile, [ 'FORMAT=NC4C' ] )
    if result != 'success':
        return 'fail'

    return 'success'

###############################################################################
# TEST NC_UBYTE metadata read - netcdf-4 (FORMAT=NC4) only (#5053)
def netcdf_33():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/nc_vars.nc'
    ofile = 'tmp/netcdf_33.nc'

    result = netcdf_test_copy( ifile, 1, None, ofile, [ 'FORMAT=NC4' ] )
    if result != 'success':
        return result

    return netcdf_check_vars( 'tmp/netcdf_33.nc' )

###############################################################################
# check support for reading large file with chunking and DEFLATE compression
# if chunking is not supported within the netcdf driver, this test can take very long
def netcdf_34():

    filename = 'utm-big-chunks.nc'
    # this timeout is more than enough - on my system takes <1s with fix, about 25 seconds without
    timeout = 5

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    if not gdaltest.run_slow_tests():
        return 'skip'

    try:
        from multiprocessing import Process
    except:
        print('from multiprocessing import Process failed')
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/netcdf/'+filename,filename):
        return 'skip'

    sys.stdout.write('.')
    sys.stdout.flush()

    tst = gdaltest.GDALTest( 'NetCDF', '../tmp/cache/'+filename, 1, 31621 )
    #tst.testOpen()

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    proc = Process( target=tst.testOpen )
    proc.start()
    proc.join( timeout )
    gdal.PopErrorHandler()

    # if proc is alive after timeout we must terminate it, and return fail
    # valgrind detects memory leaks when this occurs (although it should never happen)
    if proc.is_alive():
        proc.terminate()
        print('testOpen() for file %s has reached timeout limit of %d seconds' % (filename, timeout) )
        return 'fail'

    return 'success'

###############################################################################
# test writing a long metadata > 8196 chars (bug #5113)
def netcdf_35():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/netcdf_fixes.nc'
    ofile = 'tmp/netcdf_35.nc'

    # copy file
    result = netcdf_test_copy( ifile, 0, None, ofile )
    if result != 'success':
        return 'fail'

    # test long metadata is copied correctly
    ds = gdal.Open( ofile )
    if ds is None:
        gdaltest.post_reason( 'open of copy failed' )
        return 'fail'
    md = ds.GetMetadata( '' )
    if not 'U#bla' in md:
        gdaltest.post_reason( 'U#bla metadata absent' )
        return 'fail'
    bla = md['U#bla']
    if not len(bla) == 9591:
        gdaltest.post_reason( 'U#bla metadata is of length %d, expecting %d' % (len(bla),9591) )
        return 'fail'
    if not bla[-4:] == '_bla':
        gdaltest.post_reason( 'U#bla metadata ends with [%s], expecting [%s]' % (bla[-4:], '_bla') )
        return 'fail'

    return 'success'

###############################################################################
# test for correct geotransform (bug #5114)
def netcdf_36():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/netcdf_fixes.nc'

    ds = gdal.Open( ifile )
    if ds is None:
        gdaltest.post_reason( 'open failed' )
        return 'fail'

    gt = ds.GetGeoTransform( )
    if gt is None:
        gdaltest.post_reason( 'got no GeoTransform' )
        return 'fail'
    gt_expected = (-3.498749944898817, 0.0025000042385525173, 0.0, 46.61749818589952, 0.0, -0.001666598849826389)
    if gt != gt_expected:
        gdaltest.post_reason( 'got GeoTransform %s, expected %s' % (str(gt), str(gt_expected)) )
        return 'fail'

    return 'success'

###############################################################################
# test for reading gaussian grid (bugs #4513 and #5118)
def netcdf_37():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/reduce-cgcms.nc'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open( ifile )
    gdal.PopErrorHandler()
    if ds is None:
        gdaltest.post_reason( 'open failed' )
        return 'fail'

    gt = ds.GetGeoTransform( )
    if gt is None:
        gdaltest.post_reason( 'got no GeoTransform' )
        return 'fail'
    gt_expected = (-1.875, 3.75, 0.0, 89.01354337620016, 0.0, -3.7088976406750063)
    if gt != gt_expected:
        gdaltest.post_reason( 'got GeoTransform %s, expected %s' % (str(gt), str(gt_expected)) )
        return 'fail'

    md = ds.GetMetadata( 'GEOLOCATION2' )
    if not md or not 'Y_VALUES' in md:
        gdaltest.post_reason( 'did not get 1D geolocation' )
        return 'fail'
    y_vals = md['Y_VALUES']
    if not y_vals.startswith('{-87.15909455586265,-83.47893666931698,') \
            or not  y_vals.endswith(',83.47893666931698,87.15909455586265}'):
        gdaltest.post_reason( 'got incorrect values in 1D geolocation' )
        return 'fail'

    return 'success'

###############################################################################
# test for correct geotransform of projected data in km units (bug #5118)
def netcdf_38():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ifile = 'data/bug5118.nc'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open( ifile )
    gdal.PopErrorHandler()
    if ds is None:
        gdaltest.post_reason( 'open failed' )
        return 'fail'

    gt = ds.GetGeoTransform( )
    if gt is None:
        gdaltest.post_reason( 'got no GeoTransform' )
        return 'fail'
    gt_expected = (-1659.3478178136488, 13.545000861672793, 0.0, 2330.054725283668, 0.0, -13.54499744233631)
    if gt != gt_expected:
        gdaltest.post_reason( 'got GeoTransform %s, expected %s' % (str(gt), str(gt_expected)) )
        return 'fail'

    return 'success'

###############################################################################
# Test VRT and NETCDF:

def netcdf_39():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    shutil.copy('data/two_vars_scale_offset.nc', 'tmp')
    src_ds = gdal.Open('NETCDF:tmp/two_vars_scale_offset.nc:z')
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/netcdf_39.vrt', src_ds)
    out_ds = None
    src_ds = None

    ds = gdal.Open('tmp/netcdf_39.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/two_vars_scale_offset.nc')
    gdal.Unlink('tmp/netcdf_39.vrt')
    if cs != 65463:
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    shutil.copy('data/two_vars_scale_offset.nc', 'tmp')
    src_ds = gdal.Open('NETCDF:"tmp/two_vars_scale_offset.nc":z')
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/netcdf_39.vrt', src_ds)
    out_ds = None
    src_ds = None

    ds = gdal.Open('tmp/netcdf_39.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/two_vars_scale_offset.nc')
    gdal.Unlink('tmp/netcdf_39.vrt')
    if cs != 65463:
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    shutil.copy('data/two_vars_scale_offset.nc', 'tmp')
    src_ds = gdal.Open('NETCDF:"%s/tmp/two_vars_scale_offset.nc":z' % os.getcwd())
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('%s/tmp/netcdf_39.vrt' % os.getcwd(), src_ds)
    out_ds = None
    src_ds = None

    ds = gdal.Open('tmp/netcdf_39.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/two_vars_scale_offset.nc')
    gdal.Unlink('tmp/netcdf_39.vrt')
    if cs != 65463:
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    src_ds = gdal.Open('NETCDF:"%s/data/two_vars_scale_offset.nc":z' % os.getcwd())
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/netcdf_39.vrt', src_ds)
    del out_ds
    src_ds = None

    ds = gdal.Open('tmp/netcdf_39.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/netcdf_39.vrt')
    if cs != 65463:
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Check support of reading of chunked bottom-up files.

def netcdf_40():

    if gdaltest.netcdf_drv is None or not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    return netcdf_test_copy( 'data/bug5291.nc', 0, None, 'tmp/netcdf_40.nc' )

###############################################################################
# Test support for georeferenced file without CF convention

def netcdf_41():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.Open('data/byte_no_cf.nc')
    if ds.GetGeoTransform() != (440720, 60, 0, 3751320, 0, -60):
        gdaltest.post_reason('failure')
        print(ds.GetGeoTransform())
        return 'fail'
    if ds.GetProjectionRef().find('26711') < 0:
        gdaltest.post_reason('failure')
        print(ds.GetGeoTransform())
        return 'fail'

    return 'success'

###############################################################################
# Test writing & reading GEOLOCATION array

def netcdf_42():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('', 60, 39, 1)
    src_ds.SetMetadata( [
  'LINE_OFFSET=0',
  'LINE_STEP=1',
  'PIXEL_OFFSET=0',
  'PIXEL_STEP=1',
  'SRS=GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4326"]]',
  'X_BAND=1',
  'X_DATASET=../gcore/data/sstgeo.tif',
  'Y_BAND=2',
  'Y_DATASET=../gcore/data/sstgeo.tif'], 'GEOLOCATION' )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    src_ds.SetProjection(sr.ExportToWkt())

    gdaltest.netcdf_drv.CreateCopy('tmp/netcdf_42.nc', src_ds)

    ds = gdal.Open('tmp/netcdf_42.nc')
    if ds.GetMetadata('GEOLOCATION') != {
        'LINE_OFFSET': '0',
        'X_DATASET': 'NETCDF:"tmp/netcdf_42.nc":lon',
        'PIXEL_STEP': '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]',
        'PIXEL_OFFSET': '0',
        'X_BAND': '1',
        'LINE_STEP': '1',
        'Y_DATASET': 'NETCDF:"tmp/netcdf_42.nc":lat',
        'Y_BAND': '1'}:
        gdaltest.post_reason('failure')
        print(ds.GetMetadata('GEOLOCATION'))
        return 'fail'

    ds = gdal.Open('NETCDF:"tmp/netcdf_42.nc":lon')
    if ds.GetRasterBand(1).Checksum() != 36043:
        gdaltest.post_reason('failure')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    ds = gdal.Open('NETCDF:"tmp/netcdf_42.nc":lat')
    if ds.GetRasterBand(1).Checksum() != 33501:
        gdaltest.post_reason('failure')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    return 'success'

###############################################################################
# Test reading GEOLOCATION array from geotransform (non default)

def netcdf_43():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    gdaltest.netcdf_drv.CreateCopy('tmp/netcdf_43.nc', src_ds, options = ['WRITE_LONLAT=YES'] )

    ds = gdal.Open('tmp/netcdf_43.nc')
    if ds.GetMetadata('GEOLOCATION') != {
        'LINE_OFFSET': '0',
        'X_DATASET': 'NETCDF:"tmp/netcdf_43.nc":lon',
        'PIXEL_STEP': '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]',
        'PIXEL_OFFSET': '0',
        'X_BAND': '1',
        'LINE_STEP': '1',
        'Y_DATASET': 'NETCDF:"tmp/netcdf_43.nc":lat',
        'Y_BAND': '1'}:
        gdaltest.post_reason('failure')
        print(ds.GetMetadata('GEOLOCATION'))
        return 'fail'

    return 'success'

###############################################################################
# Test NC_USHORT/UINT read/write - netcdf-4 only (#6337)

def netcdf_44():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    for f, md5 in ('data/ushort.nc', 18), ('data/uint.nc', 10):
        if (netcdf_test_copy( f, 1, md5, 'tmp/netcdf_44.nc', [ 'FORMAT=NC4' ] )
            != 'success'):
            return 'fail'

    return 'success'

###############################################################################
# Test reading a vector NetCDF 3 file

def netcdf_45():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    # Test that a vector cannot be opened in raster-only mode
    ds = gdal.OpenEx( 'data/test_ogr_nc3.nc', gdal.OF_RASTER )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test that a raster cannot be opened in vector-only mode
    ds = gdal.OpenEx( 'data/cf-bug636.nc', gdal.OF_VECTOR )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx( 'data/test_ogr_nc3.nc', gdal.OF_VECTOR )

    with gdaltest.error_handler():
        gdal.VectorTranslate( '/vsimem/netcdf_45.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_45.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string1char,string3chars,twodimstringchar,date,datetime_explicit_fillValue,datetime,int64var,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,x,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,1234567890123,1,1,1.2,1.2,123,12,5,-125
"POINT (1 2)",,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_45.csvt', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(1),String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'
    gdal.Unlink('/vsimem/netcdf_45.csv')
    gdal.Unlink('/vsimem/netcdf_45.csvt')

    return 'success'

###############################################################################
# Test reading a vector NetCDF 3 file

def netcdf_46():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test_ogr_nc3.nc')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test reading a vector NetCDF 4 file

def netcdf_47():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    # Test that a vector cannot be opened in raster-only mode
    with gdaltest.error_handler():
        ds = gdal.OpenEx( 'data/test_ogr_nc4.nc', gdal.OF_RASTER )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx( 'data/test_ogr_nc4.nc', gdal.OF_VECTOR )

    with gdaltest.error_handler():
        gdal.VectorTranslate( '/vsimem/netcdf_47.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_47.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string3chars,twodimstringchar,date,datetime,datetime_explicit_fillValue,int64,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field,ubyte_field,ubyte_field_explicit_fillValue,ushort_field,ushort_field_explicit_fillValue,uint_field,uint_field_explicit_fillValue,uint64_field,uint64_field_explicit_fillValue
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,,1,1,1.2,1.2,123,12,5,-125,254,255,65534,65535,4000000000,4294967295,1234567890123,
"POINT (1 2)",,,,,,,,,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,,,,,,,,
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_47.csvt', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer,Integer,Integer,Integer,Integer,Integer64,Integer64,Real,Real
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'
    gdal.Unlink('/vsimem/netcdf_47.csv')
    gdal.Unlink('/vsimem/netcdf_47.csvt')

    return 'success'

###############################################################################
# Test reading a vector NetCDF 3 file without any geometry

def netcdf_48():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.OpenEx( 'data/test_ogr_no_xyz_var.nc', gdal.OF_VECTOR )
    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('failure')
        return 'fail'
    f = lyr.GetNextFeature()
    if f['int32'] != 1:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test reading a vector NetCDF 3 file with X,Y,Z vars as float

def netcdf_49():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.OpenEx( 'data/test_ogr_xyz_float.nc', gdal.OF_VECTOR )
        gdal.VectorTranslate( '/vsimem/netcdf_49.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_49.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32
"POINT Z (1 2 3)",1
"POINT (1 2)",
,,
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/netcdf_49.csv')

    return 'success'

###############################################################################
# Test creating a vector NetCDF 3 file with WKT geometry field

def netcdf_50():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.OpenEx( '../ogr/data/poly.shp', gdal.OF_VECTOR )
    out_ds = gdal.VectorTranslate( 'tmp/netcdf_50.nc', ds, format = 'netCDF', layerCreationOptions = [ 'WKT_DEFAULT_WIDTH=1'] )
    src_lyr = ds.GetLayer(0)
    src_lyr.ResetReading()
    out_lyr = out_ds.GetLayer(0)
    out_lyr.ResetReading()
    src_f = src_lyr.GetNextFeature()
    out_f = out_lyr.GetNextFeature()
    src_f.SetFID(-1)
    out_f.SetFID(-1)
    src_json = src_f.ExportToJson()
    out_json = out_f.ExportToJson()
    if src_json != out_json:
        gdaltest.post_reason('failure')
        print(src_json)
        print(out_json)
        return 'fail'
    out_ds = None

    out_ds = gdal.OpenEx( 'tmp/netcdf_50.nc', gdal.OF_VECTOR )
    out_lyr = out_ds.GetLayer(0)
    srs = out_lyr.GetSpatialRef().ExportToWkt()
    if srs.find('PROJCS["OSGB 1936') < 0:
        gdaltest.post_reason('failure')
        print(srs)
        return 'fail'
    out_f = out_lyr.GetNextFeature()
    out_f.SetFID(-1)
    out_json = out_f.ExportToJson()
    if src_json != out_json:
        gdaltest.post_reason('failure')
        print(src_json)
        print(out_json)
        return 'fail'
    out_ds = None

    gdal.Unlink('tmp/netcdf_50.nc')

    return 'success'

###############################################################################
# Test creating a vector NetCDF 3 file with X,Y,Z fields

def netcdf_51():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.OpenEx( 'data/test_ogr_nc3.nc', gdal.OF_VECTOR )
    # Test autogrow of string fields
    gdal.VectorTranslate( 'tmp/netcdf_51.nc', ds, format = 'netCDF', layerCreationOptions = [ 'STRING_DEFAULT_WIDTH=1'] )

    with gdaltest.error_handler():
        ds = gdal.OpenEx( 'tmp/netcdf_51.nc', gdal.OF_VECTOR )
        gdal.VectorTranslate( '/vsimem/netcdf_51.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT'] )
        ds = None

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_51.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string1char,string3chars,twodimstringchar,date,datetime_explicit_fillValue,datetime,int64var,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,x,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,1234567890123,1,1,1.2,1.2,123,12,5,-125
"POINT Z (1 2 0)",,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_51.csvt', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(1),String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    ds = gdal.OpenEx( 'tmp/netcdf_51.nc', gdal.OF_VECTOR | gdal.OF_UPDATE )
    lyr = ds.GetLayer(0)
    lyr.CreateField( ogr.FieldDefn('extra', ogr.OFTInteger) )
    lyr.CreateField( ogr.FieldDefn('extra_str', ogr.OFTString) )
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('failure')
        return 'fail'
    f['extra'] = 5
    f['extra_str'] = 'foobar'
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    ds = gdal.OpenEx( 'tmp/netcdf_51.nc', gdal.OF_VECTOR )
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    if f['int32'] != 1 or f['extra'] != 5 or f['extra_str'] != 'foobar':
        gdaltest.post_reason('failure')
        return 'fail'
    f = None
    ds = None

    import netcdf_cf
    if netcdf_cf.netcdf_cf_setup() == 'success' and \
       gdaltest.netcdf_cf_method is not None:
        result_cf = netcdf_cf.netcdf_cf_check_file( 'tmp/netcdf_51.nc','auto',False )
        if result_cf != 'success':
            gdaltest.post_reason('failure')
            return 'fail'

    gdal.Unlink('tmp/netcdf_51.nc')
    gdal.Unlink('tmp/netcdf_51.csv')
    gdal.Unlink('tmp/netcdf_51.csvt')

    return 'success'

###############################################################################
# Test creating a vector NetCDF 3 file with X,Y,Z fields with WRITE_GDAL_TAGS=NO

def netcdf_51_no_gdal_tags():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.OpenEx( 'data/test_ogr_nc3.nc', gdal.OF_VECTOR )
    gdal.VectorTranslate( 'tmp/netcdf_51_no_gdal_tags.nc', ds, format = 'netCDF', datasetCreationOptions = [ 'WRITE_GDAL_TAGS=NO'] )

    with gdaltest.error_handler():
        ds = gdal.OpenEx( 'tmp/netcdf_51_no_gdal_tags.nc', gdal.OF_VECTOR )
        gdal.VectorTranslate( '/vsimem/netcdf_51_no_gdal_tags.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT'] )
        ds = None

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_51_no_gdal_tags.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string1char,string3chars,twodimstringchar,date,datetime_explicit_fillValue,datetime,int64var,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x1,byte_field
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,x,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,1234567890123,1,1,1.2,1.2,123,12,5,-125
"POINT Z (1 2 0)",,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_51_no_gdal_tags.csvt', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(1),String(3),String(10),Date,DateTime,DateTime,Real,Real,Integer,Integer,Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('tmp/netcdf_51_no_gdal_tags.nc')
    gdal.Unlink('tmp/netcdf_51_no_gdal_tags.csv')
    gdal.Unlink('tmp/netcdf_51_no_gdal_tags.csvt')

    return 'success'

###############################################################################
# Test creating a vector NetCDF 4 file with X,Y,Z fields

def netcdf_52():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    ds = gdal.OpenEx( 'data/test_ogr_nc4.nc', gdal.OF_VECTOR )
    gdal.VectorTranslate( 'tmp/netcdf_52.nc', ds, format = 'netCDF', datasetCreationOptions = ['FORMAT=NC4'] )

    with gdaltest.error_handler():
        ds = gdal.OpenEx( 'tmp/netcdf_52.nc', gdal.OF_VECTOR )
        gdal.VectorTranslate( '/vsimem/netcdf_52.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT'] )
        ds = None

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_52.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string3chars,twodimstringchar,date,datetime,datetime_explicit_fillValue,int64,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field,ubyte_field,ubyte_field_explicit_fillValue,ushort_field,ushort_field_explicit_fillValue,uint_field,uint_field_explicit_fillValue,uint64_field,uint64_field_explicit_fillValue
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,,1,1,1.2,1.2,123,12,5,-125,254,255,65534,65535,4000000000,4294967295,1234567890123,
"POINT Z (1 2 0)",,,,,,,,,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,,,,,,,,
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_52.csvt', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer,Integer,Integer,Integer,Integer,Integer64,Integer64,Real,Real
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    ds = gdal.OpenEx( 'tmp/netcdf_52.nc', gdal.OF_VECTOR | gdal.OF_UPDATE )
    lyr = ds.GetLayer(0)
    lyr.CreateField( ogr.FieldDefn('extra', ogr.OFTInteger) )
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('failure')
        return 'fail'
    f['extra'] = 5
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    ds = gdal.OpenEx( 'tmp/netcdf_52.nc', gdal.OF_VECTOR )
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    if f['int32'] != 1 or f['extra'] != 5:
        gdaltest.post_reason('failure')
        return 'fail'
    f = None
    ds = None

    import netcdf_cf
    if netcdf_cf.netcdf_cf_setup() == 'success' and \
       gdaltest.netcdf_cf_method is not None:
        result_cf = netcdf_cf.netcdf_cf_check_file( 'tmp/netcdf_52.nc','auto',False )
        if result_cf != 'success':
            gdaltest.post_reason('failure')
            return 'fail'

    gdal.Unlink('tmp/netcdf_52.nc')
    gdal.Unlink('tmp/netcdf_52.csv')
    gdal.Unlink('tmp/netcdf_52.csvt')

    return 'success'

###############################################################################
# Test creating a vector NetCDF 4 file with WKT geometry field

def netcdf_53():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    ds = gdal.OpenEx( '../ogr/data/poly.shp', gdal.OF_VECTOR )
    out_ds = gdal.VectorTranslate( 'tmp/netcdf_53.nc', ds, format = 'netCDF', datasetCreationOptions = ['FORMAT=NC4'] )
    src_lyr = ds.GetLayer(0)
    src_lyr.ResetReading()
    out_lyr = out_ds.GetLayer(0)
    out_lyr.ResetReading()
    src_f = src_lyr.GetNextFeature()
    out_f = out_lyr.GetNextFeature()
    src_f.SetFID(-1)
    out_f.SetFID(-1)
    src_json = src_f.ExportToJson()
    out_json = out_f.ExportToJson()
    if src_json != out_json:
        gdaltest.post_reason('failure')
        print(src_json)
        print(out_json)
        return 'fail'
    out_ds = None

    out_ds = gdal.OpenEx( 'tmp/netcdf_53.nc', gdal.OF_VECTOR )
    out_lyr = out_ds.GetLayer(0)
    srs = out_lyr.GetSpatialRef().ExportToWkt()
    if srs.find('PROJCS["OSGB 1936') < 0:
        gdaltest.post_reason('failure')
        print(srs)
        return 'fail'
    out_f = out_lyr.GetNextFeature()
    out_f.SetFID(-1)
    out_json = out_f.ExportToJson()
    if src_json != out_json:
        gdaltest.post_reason('failure')
        print(src_json)
        print(out_json)
        return 'fail'
    out_ds = None

    gdal.Unlink('tmp/netcdf_53.nc')

    return 'success'

###############################################################################
# Test appending to a vector NetCDF 4 file with unusual types (ubyte, ushort...)

def netcdf_54():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    shutil.copy( 'data/test_ogr_nc4.nc', 'tmp/netcdf_54.nc')

    ds = gdal.OpenEx( 'tmp/netcdf_54.nc', gdal.OF_VECTOR | gdal.OF_UPDATE )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('failure')
        return 'fail'
    f['int32'] += 1
    f.SetFID(-1)
    f.ExportToJson()
    src_json = f.ExportToJson()
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    ds = gdal.OpenEx( 'tmp/netcdf_54.nc', gdal.OF_VECTOR )
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    f.SetFID(-1)
    out_json = f.ExportToJson()
    f = None
    ds = None

    gdal.Unlink('tmp/netcdf_54.nc')

    if src_json != out_json:
        gdaltest.post_reason('failure')
        print(src_json)
        print(out_json)
        return 'fail'

    return 'success'

###############################################################################
# Test auto-grow of bidimensional char variables in a vector NetCDF 4 file

def netcdf_55():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    shutil.copy( 'data/test_ogr_nc4.nc', 'tmp/netcdf_55.nc')

    ds = gdal.OpenEx( 'tmp/netcdf_55.nc', gdal.OF_VECTOR | gdal.OF_UPDATE )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('failure')
        return 'fail'
    f['twodimstringchar'] = 'abcd'
    f.SetFID(-1)
    f.ExportToJson()
    src_json = f.ExportToJson()
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    ds = gdal.OpenEx( 'tmp/netcdf_55.nc', gdal.OF_VECTOR )
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    f.SetFID(-1)
    out_json = f.ExportToJson()
    f = None
    ds = None

    gdal.Unlink('tmp/netcdf_55.nc')

    if src_json != out_json:
        gdaltest.post_reason('failure')
        print(src_json)
        print(out_json)
        return 'fail'

    return 'success'

###############################################################################
# Test truncation of bidimensional char variables and WKT in a vector NetCDF 3 file

def netcdf_56():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_56.nc')
    # Test auto-grow of WKT field
    lyr = ds.CreateLayer('netcdf_56', options = [ 'AUTOGROW_STRINGS=NO', 'STRING_DEFAULT_WIDTH=5', 'WKT_DEFAULT_WIDTH=5' ] )
    lyr.CreateField(ogr.FieldDefn('txt'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['txt'] = '0123456789'
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    with gdaltest.error_handler():
        ret = lyr.CreateFeature(f)
    if ret != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    ds = gdal.OpenEx( 'tmp/netcdf_56.nc', gdal.OF_VECTOR )
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    if f['txt'] != '01234' or f.GetGeometryRef() is not None:
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('tmp/netcdf_56.nc')

    return 'success'

###############################################################################
# Test one layer per file creation

def netcdf_57():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    try:
        shutil.rmtree('tmp/netcdf_57')
    except:
        pass

    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('netCDF').CreateDataSource('/not_existing_dir/invalid_subdir', options = ['MULTIPLE_LAYERS=SEPARATE_FILES'])
    if ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    open('tmp/netcdf_57', 'wb').close()

    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('netCDF').CreateDataSource('/not_existing_dir/invalid_subdir', options = ['MULTIPLE_LAYERS=SEPARATE_FILES'])
    if ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    os.unlink('tmp/netcdf_57')

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_57', options = ['MULTIPLE_LAYERS=SEPARATE_FILES'])
    for ilayer in range(2):
        lyr = ds.CreateLayer('lyr%d' % ilayer)
        lyr.CreateField(ogr.FieldDefn('lyr_id', ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['lyr_id'] = ilayer
        lyr.CreateFeature(f)
    ds = None

    for ilayer in range(2):
        ds = ogr.Open('tmp/netcdf_57/lyr%d.nc' % ilayer)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        if f['lyr_id'] != ilayer:
            gdaltest.post_reason('failure')
            return 'fail'
        ds = None

    shutil.rmtree('tmp/netcdf_57')

    return 'success'

###############################################################################
# Test one layer per group (NC4)

def netcdf_58():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_58.nc', options = ['FORMAT=NC4', 'MULTIPLE_LAYERS=SEPARATE_GROUPS'])
    for ilayer in range(2):
        # Make sure auto-grow will happen to test this works well with multiple groups
        lyr = ds.CreateLayer('lyr%d' % ilayer, geom_type = ogr.wkbNone, options = ['USE_STRING_IN_NC4=NO', 'STRING_DEFAULT_WIDTH=1' ])
        lyr.CreateField(ogr.FieldDefn('lyr_id', ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['lyr_id'] = 'lyr_%d' % ilayer
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('tmp/netcdf_58.nc')
    for ilayer in range(2):
        lyr = ds.GetLayer(ilayer)
        f = lyr.GetNextFeature()
        if f['lyr_id'] != 'lyr_%d' % ilayer:
            gdaltest.post_reason('failure')
            return 'fail'
    ds = None

    gdal.Unlink('tmp/netcdf_58.nc')

    return 'success'

###############################################################################
#check for UnitType set/get.
def netcdf_59():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    # get
    ds = gdal.Open( 'data/unittype.nc' )

    unit = ds.GetRasterBand( 1 ).GetUnitType()

    if unit != 'm/s':
        gdaltest.post_reason( 'Incorrect unit(%s)' % unit )
        return 'fail'

    ds = None

    # set
    tst = gdaltest.GDALTest( 'NetCDF', 'unittype.nc', 1, 4672 )

    return tst.testSetUnitType()

###############################################################################
# Test reading a "Indexed ragged array representation of profiles" v1.6.0 H3.5
# http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html#_indexed_ragged_array_representation_of_profiles

def netcdf_60():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    # Test that a vector cannot be opened in raster-only mode
    ds = gdal.OpenEx( 'data/profile.nc', gdal.OF_RASTER )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx( 'data/profile.nc', gdal.OF_VECTOR)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        gdal.VectorTranslate( '/vsimem/netcdf_60.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_60.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,profile,id,station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/netcdf_60.csv')

    return 'success'

###############################################################################
# Test appending to a "Indexed ragged array representation of profiles" v1.6.0 H3.5

def netcdf_61():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    shutil.copy('data/profile.nc', 'tmp/netcdf_61.nc')
    ds = gdal.VectorTranslate( 'tmp/netcdf_61.nc', 'data/profile.nc', accessMode = 'append' )
    gdal.VectorTranslate( '/vsimem/netcdf_61.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_61.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,profile,id,station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/netcdf_61.csv')
    gdal.Unlink('/vsimem/netcdf_61.nc')

    return 'success'

###############################################################################
# Test creating a "Indexed ragged array representation of profiles" v1.6.0 H3.5

def netcdf_62():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.VectorTranslate( 'tmp/netcdf_62.nc', 'data/profile.nc', format = 'netCDF', layerCreationOptions = ['FEATURE_TYPE=PROFILE', 'PROFILE_DIM_INIT_SIZE=1', 'PROFILE_VARIABLES=station'] )
    gdal.VectorTranslate( '/vsimem/netcdf_62.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_62.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile,id,station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/netcdf_62.csv')

    return 'success'

def netcdf_62_ncdump_check():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except:
        err = None
    if err is not None and 'netcdf library version' in err:
        (ret, err) = gdaltest.runexternal_out_and_err( 'ncdump -h tmp/netcdf_62.nc' )
        if ret.find('profile = 2') < 0 or \
           ret.find('record = UNLIMITED') < 0 or \
           ret.find('profile:cf_role = "profile_id"') < 0 or \
           ret.find('parentIndex:instance_dimension = "profile"') < 0 or \
           ret.find(':featureType = "profile"') < 0 or \
           ret.find('char station(profile') < 0 or \
           ret.find('char foo(record') < 0:
            gdaltest.post_reason('failure')
            print(ret)
            return 'fail'
    else:
        return 'skip'

    return 'success'

def netcdf_62_cf_check():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    import netcdf_cf
    if netcdf_cf.netcdf_cf_setup() == 'success' and \
       gdaltest.netcdf_cf_method is not None:
        result_cf = netcdf_cf.netcdf_cf_check_file( 'tmp/netcdf_62.nc','auto',False )
        if result_cf != 'success':
            gdaltest.post_reason('failure')
            return 'fail'

    gdal.Unlink('/vsimem/netcdf_62.nc')

    return 'success'

###############################################################################
# Test creating a NC4 "Indexed ragged array representation of profiles" v1.6.0 H3.5

def netcdf_63():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    shutil.copy('data/profile.nc', 'tmp/netcdf_63.nc')
    ds = gdal.VectorTranslate( 'tmp/netcdf_63.nc', 'data/profile.nc', format = 'netCDF', datasetCreationOptions = ['FORMAT=NC4'], layerCreationOptions = ['FEATURE_TYPE=PROFILE', 'USE_STRING_IN_NC4=NO', 'STRING_DEFAULT_WIDTH=1' ] )
    gdal.VectorTranslate( '/vsimem/netcdf_63.csv', ds, format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_63.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile,id,station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/netcdf_63.csv')

    return 'success'

def netcdf_63_ncdump_check():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except:
        err = None
    if err is not None and 'netcdf library version' in err:
        (ret, err) = gdaltest.runexternal_out_and_err( 'ncdump -h tmp/netcdf_63.nc' )
        if ret.find('profile = UNLIMITED') < 0 or \
           ret.find('record = UNLIMITED') < 0 or \
           ret.find('profile:cf_role = "profile_id"') < 0 or \
           ret.find('parentIndex:instance_dimension = "profile"') < 0 or \
           ret.find(':featureType = "profile"') < 0 or \
           ret.find('char station(record') < 0:
            gdaltest.post_reason('failure')
            print(ret)
            return 'fail'
    else:
        gdal.Unlink('/vsimem/netcdf_63.nc')
        return 'skip'

    gdal.Unlink('/vsimem/netcdf_63.nc')

    return 'success'

###############################################################################
# Test creating a "Indexed ragged array representation of profiles" v1.6.0 H3.5
# but without a profile field.

def netcdf_64():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    gdal.VectorTranslate( 'tmp/netcdf_64.nc', 'data/profile.nc', format = 'netCDF', selectFields = ['id,station,foo'], layerCreationOptions = ['FEATURE_TYPE=PROFILE', 'PROFILE_DIM_NAME=profile_dim', 'PROFILE_DIM_INIT_SIZE=1'] )
    gdal.VectorTranslate( '/vsimem/netcdf_64.csv', 'tmp/netcdf_64.nc', format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_64.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile_dim,id,station,foo
"POINT Z (2 49 100)",0,1,Palo Alto,bar
"POINT Z (3 50 50)",1,2,Santa Fe,baz
"POINT Z (2 49 200)",0,3,Palo Alto,baw
"POINT Z (3 50 100)",1,4,Santa Fe,baz2
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/netcdf_64.csv')
    gdal.Unlink('/vsimem/netcdf_64.nc')

    return 'success'

###############################################################################
# Test creating a NC4 file with empty string fields / WKT fields
# (they must be filled as empty strings to avoid crashes in netcdf lib)

def netcdf_65():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_65.nc', options = ['FORMAT=NC4'])
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('tmp/netcdf_65.nc')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['str'] != '':
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('tmp/netcdf_65.nc')

    return 'success'

###############################################################################
# Test creating a "Indexed ragged array representation of profiles" v1.6.0 H3.5
# from a config file

def netcdf_66():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    # First trying with no so good configs

    with gdaltest.error_handler():
        gdal.VectorTranslate( 'tmp/netcdf_66.nc', 'data/profile.nc', format = 'netCDF', datasetCreationOptions = ['CONFIG_FILE=not_existing'] )

    with gdaltest.error_handler():
        gdal.VectorTranslate( 'tmp/netcdf_66.nc', 'data/profile.nc', format = 'netCDF', datasetCreationOptions = ['CONFIG_FILE=<Configuration>'] )

    myconfig = \
"""<Configuration>
    <!-- comment -->
    <unrecognized_elt/>
    <DatasetCreationOption/>
    <DatasetCreationOption name="x"/>
    <DatasetCreationOption value="x"/>
    <LayerCreationOption/>
    <LayerCreationOption name="x"/>
    <LayerCreationOption value="x"/>
    <Attribute/>
    <Attribute name="foo"/>
    <Attribute value="foo"/>
    <Attribute name="foo" value="bar" type="unsupported"/>
    <Field/>
    <Field name="x">
        <!-- comment -->
        <unrecognized_elt/>
    </Field>
    <Field name="station" main_dim="non_existing"/>
    <Layer/>
    <Layer name="x">
        <!-- comment -->
        <unrecognized_elt/>
        <LayerCreationOption/>
        <LayerCreationOption name="x"/>
        <LayerCreationOption value="x"/>
        <Attribute/>
        <Attribute name="foo"/>
        <Attribute value="foo"/>
        <Attribute name="foo" value="bar" type="unsupported"/>
        <Field/>
    </Layer>
</Configuration>
"""

    with gdaltest.error_handler():
        gdal.VectorTranslate( 'tmp/netcdf_66.nc', 'data/profile.nc', format = 'netCDF', datasetCreationOptions = ['CONFIG_FILE=' + myconfig] )

    # Now with a correct configuration
    myconfig = \
"""<Configuration>
    <DatasetCreationOption name="WRITE_GDAL_TAGS" value="NO"/>
    <LayerCreationOption name="STRING_DEFAULT_WIDTH" value="1"/>
    <Attribute name="foo" value="bar"/>
    <Attribute name="foo2" value="bar2"/>
    <Field name="id">
        <Attribute name="my_extra_attribute" value="5.23" type="double"/>
    </Field>
    <Field netcdf_name="lon"> <!-- edit predefined variable -->
        <Attribute name="my_extra_lon_attribute" value="foo"/>
    </Field>
    <Layer name="profile" netcdf_name="my_profile">
        <LayerCreationOption name="FEATURE_TYPE" value="PROFILE"/>
        <LayerCreationOption name="RECORD_DIM_NAME" value="obs"/>
        <Attribute name="foo" value="123" type="integer"/> <!-- override global one -->
        <Field name="station" netcdf_name="my_station" main_dim="obs">
            <Attribute name="long_name" value="my station attribute"/>
        </Field>
        <Field netcdf_name="lat"> <!-- edit predefined variable -->
            <Attribute name="long_name" value=""/> <!-- remove predefined attribute -->
        </Field>
    </Layer>
</Configuration>
"""

    gdal.VectorTranslate( 'tmp/netcdf_66.nc', 'data/profile.nc', format = 'netCDF', datasetCreationOptions = ['CONFIG_FILE=' + myconfig] )
    gdal.VectorTranslate( '/vsimem/netcdf_66.csv', 'tmp/netcdf_66.nc', format = 'CSV', layerCreationOptions = ['LINEFORMAT=LF', 'GEOMETRY=AS_WKT'] )

    fp = gdal.VSIFOpenL( '/vsimem/netcdf_66.csv', 'rb' )
    if fp is not None:
        content = gdal.VSIFReadL( 1, 10000, fp ).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile,id,my_station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    if content != expected_content:
        gdaltest.post_reason('failure')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/netcdf_66.csv')

    return 'success'

def netcdf_66_ncdump_check():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except:
        err = None
    if err is not None and 'netcdf library version' in err:
        (ret, err) = gdaltest.runexternal_out_and_err( 'ncdump -h tmp/netcdf_66.nc' )
        if ret.find('char my_station(obs, my_station_max_width)') < 0 or \
           ret.find('my_station:long_name = "my station attribute"') < 0 or \
           ret.find('lon:my_extra_lon_attribute = "foo"') < 0 or \
           ret.find('lat:long_name') >= 0 or \
           ret.find('id:my_extra_attribute = 5.23') < 0 or \
           ret.find('profile:cf_role = "profile_id"') < 0 or \
           ret.find('parentIndex:instance_dimension = "profile"') < 0 or \
           ret.find(':featureType = "profile"') < 0:
            gdaltest.post_reason('failure')
            print(ret)
            return 'fail'
    else:
        gdal.Unlink('/vsimem/netcdf_66.nc')
        return 'skip'

    gdal.Unlink('/vsimem/netcdf_66.nc')

    return 'success'

###############################################################################
# ticket #5950: optimize IReadBlock() and CheckData() handling of partial
# blocks in the x axischeck for partial block reading.
def netcdf_67():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    try:
        import numpy
    except:
        return 'skip'

    # disable bottom-up mode to use the real file's blocks size
    gdal.SetConfigOption( 'GDAL_NETCDF_BOTTOMUP', 'NO' )
    # for the moment the next test using check_stat does not work, seems like
    # the last pixel (9) of the image is not handled by stats...
#    tst = gdaltest.GDALTest( 'NetCDF', 'partial_block_ticket5950.nc', 1, 45 )
#    result = tst.testOpen( check_stat=(1, 9, 5, 2.582) )
    # so for the moment compare the full image
    ds = gdal.Open( 'data/partial_block_ticket5950.nc', gdal.GA_ReadOnly )
    ref = numpy.arange(1, 10).reshape((3, 3))
    if numpy.array_equal(ds.GetRasterBand(1).ReadAsArray(), ref):
        result = 'success'
    else:
        result = 'fail'
    ds = None
    gdal.SetConfigOption( 'GDAL_NETCDF_BOTTOMUP', None )

    return result


###############################################################################
# Test reading SRS from srid attribute (#6613)

def netcdf_68():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open('data/srid.nc')
    wkt = ds.GetProjectionRef()
    if wkt.find('6933') < 0:
        gdaltest.post_reason('failure')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a dataset with a 1D variable with 0 record (#6645)

def netcdf_69():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open('data/test6645.nc')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test that we don't erroneously identify non-longitude axis as longitude (#6759)

def netcdf_70():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open('data/test6759.nc')
    gt = ds.GetGeoTransform()
    expected_gt = [304250.0, 250.0, 0.0, 4952500.0, 0.0, -250.0]
    if max(abs(gt[i] - expected_gt[i]) for i in range(6)) > 1e-3:
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# Test that we take into account x and y offset and scaling
# (https://github.com/OSGeo/gdal/pull/200)

def netcdf_71():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open('data/test_coord_scale_offset.nc')
    gt = ds.GetGeoTransform()
    expected_gt = (-690769.999174516, 1015.8812500000931, 0.0, 2040932.1838741193, 0.0, 1015.8812499996275)
    if max(abs(gt[i] - expected_gt[i]) for i in range(6)) > 1e-3:
        print(gt)
        return 'fail'

    return 'success'

###############################################################################
# test int64 attributes / dim

def netcdf_72():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    if not gdaltest.netcdf_drv_has_nc4:
        return 'skip'

    ds = gdal.Open('data/int64dim.nc')
    mdi = ds.GetRasterBand(1).GetMetadataItem('NETCDF_DIM_TIME')
    if mdi != '123456789012':
        print(mdi)
        return 'fail'

    return 'success'

###############################################################################

###############################################################################
# main tests list

gdaltest_list = [
    netcdf_1,
    netcdf_2,
    netcdf_3,
    netcdf_4,
    netcdf_5,
    netcdf_6,
    netcdf_7,
    netcdf_8,
    netcdf_9,
    netcdf_10,
    netcdf_11,
    netcdf_12,
    netcdf_13,
    netcdf_14,
    netcdf_15,
    netcdf_16,
    netcdf_17,
    netcdf_18,
    netcdf_19,
    netcdf_20,
    netcdf_21,
    netcdf_22,
    netcdf_23,
    netcdf_24,
    netcdf_25,
    netcdf_26,
    netcdf_27,
    netcdf_28,
    netcdf_29,
    netcdf_30,
    netcdf_31,
    netcdf_32,
    netcdf_33,
    netcdf_34,
    netcdf_35,
    netcdf_36,
    netcdf_37,
    netcdf_38,
    netcdf_39,
    netcdf_40,
    netcdf_41,
    netcdf_42,
    netcdf_43,
    netcdf_44,
    netcdf_45,
    netcdf_46,
    netcdf_47,
    netcdf_48,
    netcdf_49,
    netcdf_50,
    netcdf_51,
    netcdf_51_no_gdal_tags,
    netcdf_52,
    netcdf_53,
    netcdf_54,
    netcdf_55,
    netcdf_56,
    netcdf_57,
    netcdf_58,
    netcdf_59,
    netcdf_60,
    netcdf_61,
    netcdf_62,
    netcdf_62_ncdump_check,
    netcdf_62_cf_check,
    netcdf_63,
    netcdf_63_ncdump_check,
    netcdf_64,
    netcdf_65,
    netcdf_66,
    netcdf_66_ncdump_check,
    netcdf_67,
    netcdf_68,
    netcdf_69,
    netcdf_70,
    netcdf_71,
    netcdf_72
]

###############################################################################
#  basic file creation tests

init_list = [ \
    ('byte.tif', 1, 4672, None, []),
    ('byte_signed.tif', 1, 4672, None, ['PIXELTYPE=SIGNEDBYTE']),
    ('int16.tif', 1, 4672, None, []),
    ('int32.tif', 1, 4672, None, []),
    ('float32.tif', 1, 4672, None, []),
    ('float64.tif', 1, 4672, None, [])
]

# Some tests we don't need to do for each type.
item = init_list[0]
ut = gdaltest.GDALTest( 'netcdf', item[0], item[1], item[2], options=item[4] )

#test geotransform and projection
gdaltest_list.append( (ut.testSetGeoTransform, item[0]) )
gdaltest_list.append( (ut.testSetProjection, item[0]) )

#SetMetadata() not supported
#gdaltest_list.append( (ut.testSetMetadata, item[0]) )

# Others we do for each pixel type.
for item in init_list:
    ut = gdaltest.GDALTest( 'netcdf', item[0], item[1], item[2], options=item[4] )
    if ut is None:
        print( 'GTiff tests skipped' )
    gdaltest_list.append( (ut.testCreateCopy, item[0]) )
    gdaltest_list.append( (ut.testCreate, item[0]) )
    gdaltest_list.append( (ut.testSetNoDataValue, item[0]) )


###############################################################################
#  other tests

if __name__ == '__main__':

    gdaltest.setup_run( 'netcdf' )

    gdaltest.run_tests( gdaltest_list )

    #make sure we cleanup
    gdaltest.clean_tmp()

    gdaltest.summarize()

