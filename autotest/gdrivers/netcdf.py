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
    gdaltest.netcdf_drv_silent = False;

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
        v = v[ 0 : v.find(' ') ].strip('"');
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
# operation because the new file will only be accessable via subdatasets!

def netcdf_2():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    src_ds = gdal.Open( 'data/byte.tif' )
    
    base_ds = gdaltest.netcdf_drv.CreateCopy( 'tmp/netcdf2.nc', src_ds)
    base_ds = None

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

    scale = ds.GetRasterBand( 1 ).GetScale();
    offset = ds.GetRasterBand( 1 ).GetOffset()

    if scale != 0.01 or offset != 1.5:
        gdaltest.post_reason( 'Incorrect scale(%f) or offset(%f)' % ( scale, offset ) )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
#check for scale/offset = 1.0/0.0 if no scale or offset is available
def netcdf_13():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/no_scale_offset.nc' )

    scale = ds.GetRasterBand( 1 ).GetScale();
    offset = ds.GetRasterBand( 1 ).GetOffset()

    if scale != 1.0 or offset != 0.0:
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

    scale = ds.GetRasterBand( 1 ).GetScale();
    offset = ds.GetRasterBand( 1 ).GetOffset()

    if scale != 0.01 or offset != 1.5:
        gdaltest.post_reason( 'Incorrect scale(%f) or offset(%f)' % ( scale, offset ) )
        return 'fail'
    
    ds = None

    ds = gdal.Open( 'NETCDF:data/two_vars_scale_offset.nc:q' )

    scale = ds.GetRasterBand( 1 ).GetScale();
    offset = ds.GetRasterBand( 1 ).GetOffset()

    scale = ds.GetRasterBand( 1 ).GetScale();
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
            return 'fail'
    
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

    vals_global = {'NC_GLOBAL#test' : 'testval', 'NC_GLOBAL#valid_range_i': '0,255',\
                       'NC_GLOBAL#valid_min' : '10.1' }
    vals_band = { '_Unsigned' : 'true', 'valid_min' : '10.1', 'valid_range_b' : '1,10', \
                      'valid_range_d' : '0.1111112222222,255.555555555556', \
                      'valid_range_f' : '0.1111111,255.5556', \
                      'valid_range_s' : '0,255' }

    return netcdf_check_vars( 'data/nc_vars.nc', vals_global, vals_band )

###############################################################################
# check support for writing attributes (single values and array values)
def netcdf_25():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    result = netcdf_test_copy( 'data/nc_vars.nc', 1, None, 'tmp/netcdf_25.nc' ) 
    if result != 'success':
        return result

    vals_global = {'NC_GLOBAL#test' : 'testval', 'NC_GLOBAL#valid_range_i': '0,255',\
                       'NC_GLOBAL#valid_min' : '10.1' }
    vals_band = { '_Unsigned' : 'true', 'valid_min' : '10.1', 'valid_range_b' : '1,10', \
                      'valid_range_d' : '0.1111112222222,255.555555555556', \
                      'valid_range_f' : '0.1111111,255.5556', \
                      'valid_range_s' : '0,255' }

    return netcdf_check_vars( 'tmp/netcdf_25.nc', vals_global, vals_band )
 
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

    ifile = 'data/netcdf-4d.nc'
    ofile1 = 'tmp/netcdf_29.vrt'
    ofile = 'tmp/netcdf_29.nc'

    # create tif file using gdalwarp
    if test_cli_utilities.get_gdalwarp_path() is None:
        gdaltest.post_reason('gdalwarp not found')
        return 'fail'

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

    prj = ds.GetProjection( )

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
    netcdf_35
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

