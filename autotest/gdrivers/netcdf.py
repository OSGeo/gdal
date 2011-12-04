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
    ofile1_opts = [ 'FILETYPE=NC4C', 'COMPRESS=NONE']
    ofile2 = 'tmp/' + os.path.basename(ifile) + '-2.nc'
    ofile2_opts = [ 'FILETYPE=NC4C', 'COMPRESS=DEFLATE', 'ZLEVEL='+str(zlevel) ]

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
def netcdf_check_vars( ifile, vals_global, vals_band ):

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
    for k, v in vals.iteritems():
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
    for k, v in vals.iteritems():
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

    if gt != (-1897186.0290038721, 
               5079.3608398440065, 
               0.0, 
               2674684.0244560046, 
               0.0, 
               -5079.4721679684635):

        gdaltest.post_reason( 'Incorrect geotransform' )
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

    #look for large gtiff in cache
    if not os.path.exists( bigfile ):

        #create large gtiff
        if test_cli_utilities.get_gdalwarp_path() is None:
            gdaltest.post_reason('gdalwarp failed')
            return 'fail'
    
        warp_cmd = test_cli_utilities.get_gdalwarp_path() +\
            ' -q -overwrite -r bilinear -ts 7680 7680 -of gtiff ' +\
            'data/utm.tif ' + bigfile

        try:
            (ret, err) = gdaltest.runexternal_out_and_err( warp_cmd )
        except:
            gdaltest.post_reason('gdalwarp failed')
            return 'fail'
        
        if ( err != '' or ret != '' ):
            gdaltest.post_reason('gdalwarp failed')
        #print(ret)
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

    vals_global = {'NC_GLOBAL#test' : 'testval', 'NC_GLOBAL#valid_range_i': '0, 255',\
                       'NC_GLOBAL#valid_min' : '10.1' }
    vals_band = { '_Unsigned' : 'true', 'valid_min' : '10.1', 'valid_range_b' : '1, 10', \
                      'valid_range_d' : '0.1111112222222, 255.555555555556', \
                      'valid_range_f' : '0.1111111, 255.5556', \
                      'valid_range_s' : '0, 255' }

    return netcdf_check_vars( 'data/nc_vars.nc', vals_global, vals_band )

###############################################################################
# check support for writing attributes (single values and array values)
def netcdf_25():

    if gdaltest.netcdf_drv is None:
        return 'skip'

    result = netcdf_test_copy( 'data/nc_vars.nc', 1, None, 'tmp/netcdf_23.nc' ) 
    if result != 'success':
        return result

    vals_global = {'NC_GLOBAL#test' : 'testval', 'NC_GLOBAL#valid_range_i': '0, 255',\
                       'NC_GLOBAL#valid_min' : '10.1' }
    vals_band = { '_Unsigned' : 'true', 'valid_min' : '10.1', 'valid_range_b' : '1, 10', \
                      'valid_range_d' : '0.1111112222222, 255.555555555556', \
                      'valid_range_f' : '0.1111111, 255.5556', \
                      'valid_range_s' : '0, 255' }

    return netcdf_check_vars( 'tmp/netcdf_23.nc', vals_global, vals_band )

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
 ]

###############################################################################
#  file creation tests

init_list = [ \
    ('byte.tif', 1, 4672, None),
    ('int16.tif', 1, 4672, None),
    ('int32.tif', 1, 4672, None),
    ('float32.tif', 1, 4672, None),
    ('float64.tif', 1, 4672, None)
]

# Some tests we don't need to do for each type.
item = init_list[0]
ut = gdaltest.GDALTest( 'netcdf', item[0], item[1], item[2] )
gdaltest_list.append( (ut.testSetGeoTransform, item[0]) )
gdaltest_list.append( (ut.testSetProjection, item[0]) )
#SetMetadata() not supported 
#gdaltest_list.append( (ut.testSetMetadata, item[0]) )

# Others we do for each pixel type. 
for item in init_list:
    ut = gdaltest.GDALTest( 'netcdf', item[0], item[1], item[2] )
    if ut is None:
        print( 'GTiff tests skipped' )
    gdaltest_list.append( (ut.testCreateCopy, item[0]) )
    gdaltest_list.append( (ut.testCreate, item[0]) )
    gdaltest_list.append( (ut.testSetNoDataValue, item[0]) )


if __name__ == '__main__':

    gdaltest.setup_run( 'netcdf' )

    gdaltest.run_tests( gdaltest_list )

    #make sure we cleanup
    gdaltest.clean_tmp()

    gdaltest.summarize()

