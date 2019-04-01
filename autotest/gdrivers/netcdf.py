#!/usr/bin/env pytest
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
import struct
from osgeo import gdal
from osgeo import ogr
from osgeo import osr


import pytest

import gdaltest

import test_cli_utilities

from uffd import uffd_compare

###############################################################################
# Netcdf Functions
###############################################################################

###############################################################################
# Get netcdf version and test for supported files


@pytest.fixture(autouse=True, scope='module')
def netcdf_setup():
    # NOTE: this is also used by netcdf_cf.py

    gdaltest.netcdf_drv_version = 'unknown'
    gdaltest.netcdf_drv_has_nc2 = False
    gdaltest.netcdf_drv_has_nc4 = False
    gdaltest.netcdf_drv_has_hdf4 = False
    gdaltest.netcdf_drv_silent = False

    gdaltest.netcdf_drv = gdal.GetDriverByName('NETCDF')

    if gdaltest.netcdf_drv is None:
        pytest.skip('NOTICE: netcdf not supported, skipping checks')

    # get capabilities from driver
    metadata = gdaltest.netcdf_drv.GetMetadata()
    if metadata is None:
        pytest.skip('NOTICE: netcdf metadata not found, skipping checks')

    # netcdf library version "3.6.3" of Dec 22 2009 06:10:17 $
    # netcdf library version 4.1.1 of Mar  4 2011 12:52:19 $
    if 'NETCDF_VERSION' in metadata:
        v = metadata['NETCDF_VERSION']
        v = v[0: v.find(' ')].strip('"')
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

    print('NOTICE: using netcdf version ' + gdaltest.netcdf_drv_version +
          '  has_nc2: ' + str(gdaltest.netcdf_drv_has_nc2) + '  has_nc4: ' +
          str(gdaltest.netcdf_drv_has_nc4))

    gdaltest.count_opened_files = len(gdaltest.get_opened_files())


@pytest.fixture(autouse=True, scope='module')
def netcdf_teardown():
    diff = len(gdaltest.get_opened_files()) - gdaltest.count_opened_files
    assert diff == 0, 'Leak of file handles: %d leaked' % diff


###############################################################################
# test file copy
# helper function needed so we can call Process() on it from netcdf_test_copy_timeout()


def netcdf_test_copy(ifile, band, checksum, ofile, opts=None, driver='NETCDF'):
    # pylint: disable=unused-argument
    opts = [] if opts is None else opts
    test = gdaltest.GDALTest('NETCDF', '../' + ifile, band, checksum, options=opts)
    return test.testCreateCopy(check_gt=0, check_srs=0, new_filename=ofile, delete_copy=0, check_minmax=0)

###############################################################################
# test file copy, optional timeout arg


def netcdf_test_copy_timeout(ifile, band, checksum, ofile, opts=None, driver='NETCDF', timeout=None):

    from multiprocessing import Process

    drv = gdal.GetDriverByName(driver)

    if os.path.exists(ofile):
        drv.Delete(ofile)

    if timeout is None:
        netcdf_test_copy(ifile, band, checksum, ofile, opts, driver)

    else:
        sys.stdout.write('.')
        sys.stdout.flush()

        proc = Process(target=netcdf_test_copy, args=(ifile, band, checksum, ofile, opts))
        proc.start()
        proc.join(timeout)

        # if proc is alive after timeout we must terminate it, and return fail
        # valgrind detects memory leaks when this occurs (although it should never happen)
        if proc.is_alive():
            proc.terminate()
            if os.path.exists(ofile):
                drv.Delete(ofile)
            print('testCreateCopy() for file %s has reached timeout limit of %d seconds' % (ofile, timeout))
            pytest.fail()

###############################################################################
# check support for DEFLATE compression, requires HDF5 and zlib


def netcdf_test_deflate(ifile, checksum, zlevel=1, timeout=None):

    try:
        from multiprocessing import Process
        Process.is_alive
    except (ImportError, AttributeError):
        pytest.skip('from multiprocessing import Process failed')

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    ofile1 = 'tmp/' + os.path.basename(ifile) + '-1.nc'
    ofile1_opts = ['FORMAT=NC4C', 'COMPRESS=NONE']
    ofile2 = 'tmp/' + os.path.basename(ifile) + '-2.nc'
    ofile2_opts = ['FORMAT=NC4C', 'COMPRESS=DEFLATE', 'ZLEVEL=' + str(zlevel)]

    assert os.path.exists(ifile), ('ifile %s does not exist' % ifile)

    netcdf_test_copy_timeout(ifile, 1, checksum, ofile1, ofile1_opts, 'NETCDF', timeout)

    netcdf_test_copy_timeout(ifile, 1, checksum, ofile2, ofile2_opts, 'NETCDF', timeout)

    # make sure compressed file is smaller than uncompressed files
    try:
        size1 = os.path.getsize(ofile1)
        size2 = os.path.getsize(ofile2)
    except OSError:
        pytest.fail('Error getting file sizes.')

    assert size2 < size1, \
        'Compressed file is not smaller than reference, check your netcdf-4, HDF5 and zlib installation'

###############################################################################
# check support for reading attributes (single values and array values)


def netcdf_check_vars(ifile, vals_global=None, vals_band=None):

    src_ds = gdal.Open(ifile)

    assert src_ds is not None, ('could not open dataset ' + ifile)

    metadata_global = src_ds.GetMetadata()
    assert metadata_global is not None, ('could not get global metadata from ' + ifile)

    missval = src_ds.GetRasterBand(1).GetNoDataValue()
    assert missval == 1, ('got invalid nodata value %s for Band' % str(missval))

    metadata_band = src_ds.GetRasterBand(1).GetMetadata()
    assert metadata_band is not None, 'could not get Band metadata'

    metadata = metadata_global
    vals = vals_global
    if vals is None:
        vals = dict()
    for k, v in vals.items():
        assert k in metadata, ("missing metadata [%s]" % (str(k)))
        # strip { and } as new driver uses these for array values
        mk = metadata[k].lstrip('{ ').rstrip('} ')
        assert mk == v, ("invalid value [%s] for metadata [%s]=[%s]"
                                 % (str(mk), str(k), str(v)))

    metadata = metadata_band
    vals = vals_band
    if vals is None:
        vals = dict()
    for k, v in vals.items():
        assert k in metadata, ("missing metadata [%s]" % (str(k)))
        # strip { and } as new driver uses these for array values
        mk = metadata[k].lstrip('{ ').rstrip('} ')
        assert mk == v, ("invalid value [%s] for metadata [%s]=[%s]"
                                 % (str(mk), str(k), str(v)))

    

###############################################################################
# Netcdf Tests
###############################################################################

###############################################################################
# Perform simple read test.

def test_netcdf_1():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('NetCDF', 'NETCDF:"data/bug636.nc":tas', 1, 31621,
                            filename_absolute=1)

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    tst.testOpen()
    gdal.PopErrorHandler()

###############################################################################
# Verify a simple createcopy operation.  We can't do the trivial gdaltest
# operation because the new file will only be accessible via subdatasets.


def test_netcdf_2():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')

    gdaltest.netcdf_drv.CreateCopy('tmp/netcdf2.nc', src_ds)

    tst = gdaltest.GDALTest('NetCDF', 'tmp/netcdf2.nc',
                            1, 4672,
                            filename_absolute=1)

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

    tst.testOpen(check_prj=wkt)

    # Test that in raster-only mode, update isn't supported (not sure what would be missing for that...)
    with gdaltest.error_handler():
        ds = gdal.Open('tmp/netcdf2.nc', gdal.GA_Update)
    assert ds is None

    gdaltest.clean_tmp()

###############################################################################


def test_netcdf_3():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/sombrero.grd')
    bnd = ds.GetRasterBand(1)
    minmax = bnd.ComputeRasterMinMax()

    assert abs(minmax[0] - (-0.675758)) <= 0.000001 and abs(minmax[1] - 1.0) <= 0.000001, \
        'Wrong min or max.'

    bnd = None
    ds = None

###############################################################################
# In #2582 5dimensional files were causing problems.  Verify use ok.


def test_netcdf_4():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('NetCDF',
                            'NETCDF:data/foo_5dimensional.nc:temperature',
                            3, 1218, filename_absolute=1)

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # don't test for checksum (see bug #4284)
    result = tst.testOpen(skip_checksum=True)
    gdal.PopErrorHandler()

    return result

###############################################################################
# In #2583 5dimensional files were having problems unrolling the highest
# dimension - check handling now on band 7.


def test_netcdf_5():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('NetCDF',
                            'NETCDF:data/foo_5dimensional.nc:temperature',
                            7, 1227, filename_absolute=1)

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # don't test for checksum (see bug #4284)
    result = tst.testOpen(skip_checksum=True)
    gdal.PopErrorHandler()

    return result

###############################################################################
# ticket #3324 check spatial reference reading for cf-1.4 lambert conformal
# 1 standard parallel.


def test_netcdf_6():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/cf_lcc1sp.nc')
    prj = ds.GetProjection()

    sr = osr.SpatialReference()
    sr.ImportFromWkt(prj)
    lat_origin = sr.GetProjParm('latitude_of_origin')

    assert lat_origin == 25, ('Latitude of origin does not match expected:\n%f'
                             % lat_origin)

    ds = None

###############################################################################
# ticket #3324 check spatial reference reading for cf-1.4 lambert conformal
# 2 standard parallels.


def test_netcdf_7():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/cf_lcc2sp.nc')
    prj = ds.GetProjection()

    sr = osr.SpatialReference()
    sr.ImportFromWkt(prj)
    std_p1 = sr.GetProjParm('standard_parallel_1')
    std_p2 = sr.GetProjParm('standard_parallel_2')

    assert std_p1 == 33.0 and std_p2 == 45.0, \
        ('Standard Parallels do not match expected:\n%f,%f'
                             % (std_p1, std_p2))

    ds = None
    sr = None

###############################################################################
# check for cf convention read of albers equal area
# Previous version compared entire wkt, which varies slightly among driver versions
# now just look for PROJECTION=Albers_Conic_Equal_Area and some parameters


def test_netcdf_8():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/cf_aea2sp_invf.nc')

    srs = osr.SpatialReference()
    srs.ImportFromWkt(ds.GetProjection())

    proj = srs.GetAttrValue('PROJECTION')
    assert proj == 'Albers_Conic_Equal_Area', \
        ('Projection does not match expected : ' + proj)

    param = srs.GetProjParm('latitude_of_center')
    assert param == 37.5, ('Got wrong parameter value (%g)' % param)

    param = srs.GetProjParm('longitude_of_center')
    assert param == -96, ('Got wrong parameter value (%g)' % param)

    ds = None

###############################################################################
# check to see if projected systems default to wgs84 if no spheroid def


def test_netcdf_9():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/cf_no_sphere.nc')

    prj = ds.GetProjection()

    sr = osr.SpatialReference()
    sr.ImportFromWkt(prj)
    spheroid = sr.GetAttrValue('SPHEROID')

    assert spheroid == 'WGS 84', ('Incorrect spheroid read from file\n%s'
                             % (spheroid))

    ds = None
    sr = None

###############################################################################
# check if km pixel size makes it through to gt


def test_netcdf_10():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/cf_no_sphere.nc')

    prj = ds.GetProjection()

    gt = ds.GetGeoTransform()

    gt1 = (-1897186.0290038721, 5079.3608398440065,
           0.0, 2674684.0244560046,
           0.0, -5079.4721679684635)
    gt2 = (-1897.186029003872, 5.079360839844003,
           0.0, 2674.6840244560044,
           0.0, -5.079472167968456)

    if gt != gt1:
        sr = osr.SpatialReference()
        sr.ImportFromWkt(prj)
        # new driver uses UNIT vattribute instead of scaling values
        assert (sr.GetAttrValue("PROJCS|UNIT", 1) == "1000" and gt == gt2), \
            ('Incorrect geotransform, got ' + str(gt))

    ds = None

###############################################################################
# check if ll gets caught in km pixel size check


def test_netcdf_11():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/cf_geog.nc')

    gt = ds.GetGeoTransform()

    assert gt == (-0.5, 1.0, 0.0, 10.5, 0.0, -1.0), 'Incorrect geotransform'

    ds = None

###############################################################################
# check for scale/offset set/get.


def test_netcdf_12():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/scale_offset.nc')

    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()

    assert scale == 0.01 and offset == 1.5, \
        ('Incorrect scale(%f) or offset(%f)' % (scale, offset))

    ds = None

###############################################################################
# check for scale/offset = None if no scale or offset is available


def test_netcdf_13():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/no_scale_offset.nc')

    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()

    assert scale is None and offset is None, 'Incorrect scale or offset'

    ds = None

###############################################################################
# check for scale/offset for two variables


def test_netcdf_14():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('NETCDF:data/two_vars_scale_offset.nc:z')

    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()

    assert scale == 0.01 and offset == 1.5, \
        ('Incorrect scale(%f) or offset(%f)' % (scale, offset))

    ds = None

    ds = gdal.Open('NETCDF:data/two_vars_scale_offset.nc:q')

    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()

    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()

    assert scale == 0.1 and offset == 2.5, \
        ('Incorrect scale(%f) or offset(%f)' % (scale, offset))

###############################################################################
# check support for netcdf-2 (64 bit)
# This test fails in 1.8.1, because the driver does not support NC2 (bug #3890)


def test_netcdf_15():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if gdaltest.netcdf_drv_has_nc2:
        ds = gdal.Open('data/trmm-nc2.nc')
        assert ds is not None
        ds = None
        return
    else:
        pytest.skip()

    
###############################################################################
# check support for netcdf-4


def test_netcdf_16():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/trmm-nc4.nc'

    if gdaltest.netcdf_drv_has_nc4:

        # test with Open()
        ds = gdal.Open(ifile)
        if ds is None:
            pytest.fail('GDAL did not open file')
        else:
            name = ds.GetDriver().GetDescription()
            ds = None
            # return fail if did not open with the netCDF driver (i.e. HDF5Image)
            assert name == 'netCDF', 'netcdf driver did not open file'

        # test with Identify()
        name = gdal.IdentifyDriver(ifile).GetDescription()
        assert name == 'netCDF', 'netcdf driver did not identify file'

    else:
        pytest.skip()

    
###############################################################################
# check support for netcdf-4 - make sure hdf5 is not read by netcdf driver


def test_netcdf_17():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/groups.h5'

    # skip test if Hdf5 is not enabled
    if gdal.GetDriverByName('HDF5') is None and \
            gdal.GetDriverByName('HDF5Image') is None:
        pytest.skip()

    if gdaltest.netcdf_drv_has_nc4:

        # test with Open()
        ds = gdal.Open(ifile)
        if ds is None:
            pytest.fail('GDAL did not open hdf5 file')
        else:
            name = ds.GetDriver().GetDescription()
            ds = None
            # return fail if opened with the netCDF driver
            assert name != 'netCDF', 'netcdf driver opened hdf5 file'

        # test with Identify()
        name = gdal.IdentifyDriver(ifile).GetDescription()
        assert name != 'netCDF', 'netcdf driver was identified for hdf5 file'

    else:
        pytest.skip()

    
###############################################################################
# check support for netcdf-4 classic (NC4C)


def test_netcdf_18():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/trmm-nc4c.nc'

    if gdaltest.netcdf_drv_has_nc4:

        # test with Open()
        ds = gdal.Open(ifile)
        if ds is None:
            pytest.fail()
        else:
            name = ds.GetDriver().GetDescription()
            ds = None
            # return fail if did not open with the netCDF driver (i.e. HDF5Image)
            assert name == 'netCDF'

        # test with Identify()
        name = gdal.IdentifyDriver(ifile).GetDescription()
        assert name == 'netCDF'

    else:
        pytest.skip()

    
###############################################################################
# check support for reading with DEFLATE compression, requires NC4


def test_netcdf_19():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    tst = gdaltest.GDALTest('NetCDF', 'data/trmm-nc4z.nc', 1, 50235,
                            filename_absolute=1)

    result = tst.testOpen(skip_checksum=True)

    return result

###############################################################################
# check support for writing with DEFLATE compression, requires NC4


def test_netcdf_20():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    # simple test with tiny file
    return netcdf_test_deflate('data/utm.tif', 50235)


###############################################################################
# check support for writing large file with DEFLATE compression
# if chunking is not defined properly within the netcdf driver, this test can take 1h
def test_netcdf_21():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    if not gdaltest.run_slow_tests():
        pytest.skip()

    bigfile = 'tmp/cache/utm-big.tif'

    sys.stdout.write('.')
    sys.stdout.flush()

    # create cache dir if absent
    if not os.path.exists('tmp/cache'):
        os.mkdir('tmp/cache')

    # look for large gtiff in cache
    if not os.path.exists(bigfile):

        # create large gtiff
        if test_cli_utilities.get_gdalwarp_path() is None:
            pytest.skip('gdalwarp not found')

        warp_cmd = test_cli_utilities.get_gdalwarp_path() +\
            ' -q -overwrite -r bilinear -ts 7680 7680 -of gtiff ' +\
            'data/utm.tif ' + bigfile

        try:
            (ret, err) = gdaltest.runexternal_out_and_err(warp_cmd)
        except OSError:
            pytest.fail('gdalwarp execution failed')

        assert not (err != '' or ret != ''), \
            ('gdalwarp returned error\n' + str(ret) + ' ' + str(err))

    # test compression of the file, with a conservative timeout of 60 seconds
    return netcdf_test_deflate(bigfile, 26695, 6, 60)


###############################################################################
# check support for hdf4
def test_netcdf_22():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_hdf4:
        pytest.skip()

    ifile = 'data/hdifftst2.hdf'

    # suppress warning
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('NETCDF:' + ifile)
    gdal.PopErrorHandler()

    if ds is None:
        pytest.fail('netcdf driver did not open hdf4 file')
    else:
        ds = None

    
###############################################################################
# check support for hdf4 - make sure  hdf4 file is not read by netcdf driver


def test_netcdf_23():

    # don't skip if netcdf is not enabled in GDAL
    # if gdaltest.netcdf_drv is None:
    #    return 'skip'
    # if not gdaltest.netcdf_drv_has_hdf4:
    #    return 'skip'

    # skip test if Hdf4 is not enabled in GDAL
    if gdal.GetDriverByName('HDF4') is None and \
            gdal.GetDriverByName('HDF4Image') is None:
        pytest.skip()

    ifile = 'data/hdifftst2.hdf'

    # test with Open()
    ds = gdal.Open(ifile)
    if ds is None:
        pytest.fail('GDAL did not open hdf4 file')
    else:
        name = ds.GetDriver().GetDescription()
        ds = None
        # return fail if opened with the netCDF driver
        assert name != 'netCDF', 'netcdf driver opened hdf4 file'

    # test with Identify()
    name = gdal.IdentifyDriver(ifile).GetDescription()
    assert name != 'netCDF', 'netcdf driver was identified for hdf4 file'

###############################################################################
# check support for reading attributes (single values and array values)


def test_netcdf_24():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

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

    return netcdf_check_vars('data/nc_vars.nc', vals_global, vals_band)

###############################################################################
# check support for NC4 reading attributes (single values and array values)


def netcdf_24_nc4():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

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
                 'valid_range_f': '0.1111111,255.5556'}

    return netcdf_check_vars('data/nc4_vars.nc', vals_global, vals_band)

###############################################################################
# check support for writing attributes (single values and array values)


def test_netcdf_25():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    netcdf_test_copy('data/nc_vars.nc', 1, None, 'tmp/netcdf_25.nc')

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

    return netcdf_check_vars('tmp/netcdf_25.nc', vals_global, vals_band)

###############################################################################
# check support for NC4 writing attributes (single values and array values)


def netcdf_25_nc4():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    netcdf_test_copy('data/nc4_vars.nc', 1, None, 'tmp/netcdf_25_nc4.nc', ['FORMAT=NC4'])

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
                 'valid_range_us': '0,32000',
                 'valid_range_l': '0,255',
                 'valid_range_ul': '0,4000000000',
                 'valid_range_d': '0.1111112222222,255.555555555556',
                 'valid_range_f': '0.1111111,255.5556',
                 'valid_range_s': '0,255'}

    return netcdf_check_vars('tmp/netcdf_25_nc4.nc', vals_global, vals_band)

###############################################################################
# check support for WRITE_BOTTOMUP file creation option
# use a dummy file with no lon/lat info to force a different checksum
# depending on y-axis order


def test_netcdf_26():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # test default config
    test = gdaltest.GDALTest('NETCDF', '../data/int16-nogeo.nc', 1, 4672)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    test.testCreateCopy(check_gt=0, check_srs=0, check_minmax=0)
    gdal.PopErrorHandler()

    # test WRITE_BOTTOMUP=NO
    test = gdaltest.GDALTest('NETCDF', '../data/int16-nogeo.nc', 1, 4855,
                             options=['WRITE_BOTTOMUP=NO'])
    test.testCreateCopy(check_gt=0, check_srs=0, check_minmax=0)


###############################################################################
# check support for GDAL_NETCDF_BOTTOMUP configuration option


def test_netcdf_27():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # test default config
    test = gdaltest.GDALTest('NETCDF', '../data/int16-nogeo.nc', 1, 4672)
    config_bak = gdal.GetConfigOption('GDAL_NETCDF_BOTTOMUP')
    gdal.SetConfigOption('GDAL_NETCDF_BOTTOMUP', None)
    test.testOpen()
    gdal.SetConfigOption('GDAL_NETCDF_BOTTOMUP', config_bak)

    # test GDAL_NETCDF_BOTTOMUP=NO
    test = gdaltest.GDALTest('NETCDF', '../data/int16-nogeo.nc', 1, 4855)
    config_bak = gdal.GetConfigOption('GDAL_NETCDF_BOTTOMUP')
    gdal.SetConfigOption('GDAL_NETCDF_BOTTOMUP', 'NO')
    test.testOpen()
    gdal.SetConfigOption('GDAL_NETCDF_BOTTOMUP', config_bak)


###############################################################################
# check support for writing multi-dimensional files (helper function)


def netcdf_test_4dfile(ofile):

    # test result file has 8 bands and 0 subdasets (instead of 0 bands and 8 subdatasets)
    ds = gdal.Open(ofile)
    assert ds is not None, 'open of copy failed'
    md = ds.GetMetadata('SUBDATASETS')
    subds_count = 0
    if md is not None:
        subds_count = len(md) / 2
    assert ds.RasterCount == 8 and subds_count == 0, \
        ('copy has %d bands (expected 8) and has %d subdatasets'
                             ' (expected 0)' % (ds.RasterCount, subds_count))
    ds = None

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except OSError:
        print('NOTICE: ncdump not found')
        return
    if err is None or 'netcdf library version' not in err:
        print('NOTICE: ncdump not found')
        return
    (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h ' + ofile)
    assert ret != '' and err == '', 'ncdump failed'

    # simple dimension tests using ncdump output
    err = ""
    if 'int t(time, levelist, lat, lon) ;' not in ret:
        err = err + 'variable (t) has wrong dimensions or is missing\n'
    if 'levelist = 2 ;' not in ret:
        err = err + 'levelist dimension is missing or incorrect\n'
    if 'int levelist(levelist) ;' not in ret:
        err = err + 'levelist variable is missing or incorrect\n'
    if 'time = 4 ;' not in ret:
        err = err + 'time dimension is missing or incorrect\n'
    if 'double time(time) ;' not in ret:
        err = err + 'time variable is missing or incorrect\n'
    # uncomment this to get full header in output
    # if err != '':
    #    err = err + ret
    assert err == ''

###############################################################################
# check support for writing multi-dimensional files using CreateCopy()


def test_netcdf_28():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/netcdf-4d.nc'
    ofile = 'tmp/netcdf_28.nc'

    # copy file
    netcdf_test_copy(ifile, 0, None, ofile)

    # test file
    return netcdf_test_4dfile(ofile)

###############################################################################
# Check support for writing multi-dimensional files using gdalwarp.
# Requires metadata copy support in gdalwarp (see bug #3898).
# First create a vrt file using gdalwarp, then copy file to netcdf.
# The workaround is (currently ??) necessary because dimension rolling code is
# in netCDFDataset::CreateCopy() and necessary dimension metadata
# is not saved to netcdf when using gdalwarp (as the driver does not write
# metadata to netcdf file with SetMetadata() and SetMetadataItem()).


def test_netcdf_29():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # create tif file using gdalwarp
    if test_cli_utilities.get_gdalwarp_path() is None:
        pytest.skip('gdalwarp not found')

    ifile = 'data/netcdf-4d.nc'
    ofile1 = 'tmp/netcdf_29.vrt'
    ofile = 'tmp/netcdf_29.nc'

    warp_cmd = '%s -q -overwrite -of vrt %s %s' %\
        (test_cli_utilities.get_gdalwarp_path(), ifile, ofile1)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err(warp_cmd)
    except OSError:
        pytest.fail('gdalwarp execution failed')

    assert not (err != '' or ret != ''), \
        ('gdalwarp returned error\n' + str(ret) + ' ' + str(err))

    # copy vrt to netcdf, with proper dimension rolling
    netcdf_test_copy(ofile1, 0, None, ofile)

    # test file
    netcdf_test_4dfile(ofile)

###############################################################################
# check support for file with nan values (bug #4705)


def test_netcdf_30():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('NetCDF', 'trmm-nan.nc', 1, 62519)

    # We don't want to gum up the test stream output with the
    # 'Warning 1: No UNIDATA NC_GLOBAL:Conventions attribute' message.
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    result = tst.testOpen()
    gdal.PopErrorHandler()

    return result

###############################################################################
# check if 2x2 file has proper geotransform
# 1 pixel (in width or height) still unsupported because we can't get the pixel dimensions


def test_netcdf_31():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/trmm-2x2.nc')

    ds.GetProjection()

    gt = ds.GetGeoTransform()

    gt1 = (-80.0, 0.25, 0.0, -19.5, 0.0, -0.25)

    assert gt == gt1, ('Incorrect geotransform, got ' + str(gt))

    ds = None

###############################################################################
# Test NC_UBYTE write/read - netcdf-4 (FORMAT=NC4) only (#5053)


def test_netcdf_32():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    ifile = 'data/byte.tif'
    ofile = 'tmp/netcdf_32.nc'

    # gdal.SetConfigOption('CPL_DEBUG', 'ON')

    # test basic read/write
    netcdf_test_copy(ifile, 1, 4672, ofile, ['FORMAT=NC4'])
    netcdf_test_copy(ifile, 1, 4672, ofile, ['FORMAT=NC4C'])

###############################################################################
# TEST NC_UBYTE metadata read - netcdf-4 (FORMAT=NC4) only (#5053)


def test_netcdf_33():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/nc_vars.nc'
    ofile = 'tmp/netcdf_33.nc'

    netcdf_test_copy(ifile, 1, None, ofile, ['FORMAT=NC4'])

    return netcdf_check_vars('tmp/netcdf_33.nc')

###############################################################################
# check support for reading large file with chunking and DEFLATE compression
# if chunking is not supported within the netcdf driver, this test can take very long


def test_netcdf_34():

    filename = 'utm-big-chunks.nc'
    # this timeout is more than enough - on my system takes <1s with fix, about 25 seconds without
    timeout = 5

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    if not gdaltest.run_slow_tests():
        pytest.skip()

    try:
        from multiprocessing import Process
    except ImportError:
        pytest.skip('from multiprocessing import Process failed')

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/netcdf/' + filename, filename):
        pytest.skip()

    sys.stdout.write('.')
    sys.stdout.flush()

    tst = gdaltest.GDALTest('NetCDF', '../tmp/cache/' + filename, 1, 31621)
    # tst.testOpen()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    proc = Process(target=tst.testOpen)
    proc.start()
    proc.join(timeout)
    gdal.PopErrorHandler()

    # if proc is alive after timeout we must terminate it, and return fail
    # valgrind detects memory leaks when this occurs (although it should never happen)
    if proc.is_alive():
        proc.terminate()
        pytest.fail('testOpen() for file %s has reached timeout limit of %d seconds' % (filename, timeout))

    
###############################################################################
# test writing a long metadata > 8196 chars (bug #5113)


def test_netcdf_35():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/netcdf_fixes.nc'
    ofile = 'tmp/netcdf_35.nc'

    # copy file
    netcdf_test_copy(ifile, 0, None, ofile)

    # test long metadata is copied correctly
    ds = gdal.Open(ofile)
    assert ds is not None, 'open of copy failed'
    md = ds.GetMetadata('')
    assert 'U#bla' in md, 'U#bla metadata absent'
    bla = md['U#bla']
    assert len(bla) == 9591, \
        ('U#bla metadata is of length %d, expecting %d' % (len(bla), 9591))
    assert bla[-4:] == '_bla', \
        ('U#bla metadata ends with [%s], expecting [%s]' % (bla[-4:], '_bla'))

###############################################################################
# test for correct geotransform (bug #5114)


def test_netcdf_36():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/netcdf_fixes.nc'

    ds = gdal.Open(ifile)
    assert ds is not None, 'open failed'

    gt = ds.GetGeoTransform()
    assert gt is not None, 'got no GeoTransform'
    gt_expected = (-3.498749944898817, 0.0025000042385525173, 0.0, 46.61749818589952, 0.0, -0.001666598849826389)
    assert gt == gt_expected, \
        ('got GeoTransform %s, expected %s' % (str(gt), str(gt_expected)))


###############################################################################
# test for correct geotransform with longitude wrap


def test_netcdf_36_lonwrap():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/nc_lonwrap.nc'

    ds = gdal.Open(ifile)
    assert ds is not None, 'open failed'

    gt = ds.GetGeoTransform()
    assert gt is not None, 'got no GeoTransform'
    gt_expected = (-2.25, 2.5, 0.0, 16.25, 0.0, -2.5)
    assert gt == gt_expected, \
        ('got GeoTransform %s, expected %s' % (str(gt), str(gt_expected)))


###############################################################################
# test for reading gaussian grid (bugs #4513 and #5118)


def test_netcdf_37():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/reduce-cgcms.nc'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open(ifile)
    gdal.PopErrorHandler()
    assert ds is not None, 'open failed'

    gt = ds.GetGeoTransform()
    assert gt is not None, 'got no GeoTransform'
    gt_expected = (-1.875, 3.75, 0.0, 89.01354337620016, 0.0, -3.7088976406750063)
    assert gt == gt_expected, \
        ('got GeoTransform %s, expected %s' % (str(gt), str(gt_expected)))

    md = ds.GetMetadata('GEOLOCATION2')
    assert md and 'Y_VALUES' in md, 'did not get 1D geolocation'
    y_vals = md['Y_VALUES']
    assert y_vals.startswith('{-87.15909455586265,-83.47893666931698,') and y_vals.endswith(',83.47893666931698,87.15909455586265}'), \
        'got incorrect values in 1D geolocation'

###############################################################################
# test for correct geotransform of projected data in km units (bug #5118)


def test_netcdf_38():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifile = 'data/bug5118.nc'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open(ifile)
    gdal.PopErrorHandler()
    assert ds is not None, 'open failed'

    gt = ds.GetGeoTransform()
    assert gt is not None, 'got no GeoTransform'
    gt_expected = (-1659.3478178136488, 13.545000861672793, 0.0, 2330.054725283668, 0.0, -13.54499744233631)
    assert gt == gt_expected, \
        ('got GeoTransform %s, expected %s' % (str(gt), str(gt_expected)))

###############################################################################
# Test VRT and NETCDF:


def test_netcdf_39():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

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
    assert cs == 65463

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
    assert cs == 65463

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
    assert cs == 65463

    src_ds = gdal.Open('NETCDF:"%s/data/two_vars_scale_offset.nc":z' % os.getcwd())
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/netcdf_39.vrt', src_ds)
    del out_ds
    src_ds = None

    ds = gdal.Open('tmp/netcdf_39.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('tmp/netcdf_39.vrt')
    assert cs == 65463

###############################################################################
# Check support of reading of chunked bottom-up files.


def test_netcdf_40():

    if gdaltest.netcdf_drv is None or not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    return netcdf_test_copy('data/bug5291.nc', 0, None, 'tmp/netcdf_40.nc')

###############################################################################
# Test support for georeferenced file without CF convention


def test_netcdf_41():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/byte_no_cf.nc')
    assert ds.GetGeoTransform() == (440720, 60, 0, 3751320, 0, -60)
    assert ds.GetProjectionRef().find('26711') >= 0, ds.GetGeoTransform()

###############################################################################
# Test writing & reading GEOLOCATION array


def test_netcdf_42():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('MEM').Create('', 60, 39, 1)
    src_ds.SetMetadata([
        'LINE_OFFSET=0',
        'LINE_STEP=1',
        'PIXEL_OFFSET=0',
        'PIXEL_STEP=1',
        'SRS=GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4326"]]',
        'X_BAND=1',
        'X_DATASET=../gcore/data/sstgeo.tif',
        'Y_BAND=2',
        'Y_DATASET=../gcore/data/sstgeo.tif'], 'GEOLOCATION')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    src_ds.SetProjection(sr.ExportToWkt())

    gdaltest.netcdf_drv.CreateCopy('tmp/netcdf_42.nc', src_ds)

    ds = gdal.Open('tmp/netcdf_42.nc')
    assert (ds.GetMetadata('GEOLOCATION') == {
        'LINE_OFFSET': '0',
        'X_DATASET': 'NETCDF:"tmp/netcdf_42.nc":lon',
        'PIXEL_STEP': '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        'PIXEL_OFFSET': '0',
        'X_BAND': '1',
        'LINE_STEP': '1',
        'Y_DATASET': 'NETCDF:"tmp/netcdf_42.nc":lat',
            'Y_BAND': '1'})

    ds = gdal.Open('NETCDF:"tmp/netcdf_42.nc":lon')
    assert ds.GetRasterBand(1).Checksum() == 36043

    ds = gdal.Open('NETCDF:"tmp/netcdf_42.nc":lat')
    assert ds.GetRasterBand(1).Checksum() == 33501

###############################################################################
# Test reading GEOLOCATION array from geotransform (non default)


def test_netcdf_43():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    gdaltest.netcdf_drv.CreateCopy('tmp/netcdf_43.nc', src_ds, options=['WRITE_LONLAT=YES'])

    ds = gdal.Open('tmp/netcdf_43.nc')
    assert (ds.GetMetadata('GEOLOCATION') == {
        'LINE_OFFSET': '0',
        'X_DATASET': 'NETCDF:"tmp/netcdf_43.nc":lon',
        'PIXEL_STEP': '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        'PIXEL_OFFSET': '0',
        'X_BAND': '1',
        'LINE_STEP': '1',
        'Y_DATASET': 'NETCDF:"tmp/netcdf_43.nc":lat',
            'Y_BAND': '1'})

    tmp_ds = gdal.Warp('', 'tmp/netcdf_43.nc', options = '-f MEM -geoloc')
    gt = tmp_ds.GetGeoTransform()
    assert abs(gt[0] - -117.3) < 1, gt
    assert abs(gt[3] - 33.9) < 1, gt


###############################################################################
# Test NC_USHORT/UINT read/write - netcdf-4 only (#6337)


def test_netcdf_44():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    for f, md5 in ('data/ushort.nc', 18), ('data/uint.nc', 10):
        netcdf_test_copy(f, 1, md5, 'tmp/netcdf_44.nc', ['FORMAT=NC4'])

###############################################################################
# Test reading a vector NetCDF 3 file


def test_netcdf_45():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # Test that a vector cannot be opened in raster-only mode
    ds = gdal.OpenEx('data/test_ogr_nc3.nc', gdal.OF_RASTER)
    assert ds is None

    # Test that a raster cannot be opened in vector-only mode
    ds = gdal.OpenEx('data/cf-bug636.nc', gdal.OF_VECTOR)
    assert ds is None

    ds = gdal.OpenEx('data/test_ogr_nc3.nc', gdal.OF_VECTOR)

    with gdaltest.error_handler():
        gdal.VectorTranslate('/vsimem/netcdf_45.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_45.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string1char,string3chars,twodimstringchar,date,datetime_explicit_fillValue,datetime,int64var,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,x,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,1234567890123,1,1,1.2,1.2,123,12,5,-125
"POINT (1 2)",,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,
"""
    assert content == expected_content

    fp = gdal.VSIFOpenL('/vsimem/netcdf_45.csvt', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(1),String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer
"""
    assert content == expected_content
    gdal.Unlink('/vsimem/netcdf_45.csv')
    gdal.Unlink('/vsimem/netcdf_45.csvt')
    gdal.Unlink('/vsimem/netcdf_45.prj')

###############################################################################
# Test reading a vector NetCDF 3 file


def test_netcdf_46():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test_ogr_nc3.nc')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test reading a vector NetCDF 4 file


def test_netcdf_47():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    # Test that a vector cannot be opened in raster-only mode
    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_ogr_nc4.nc', gdal.OF_RASTER)
    assert ds is None

    ds = gdal.OpenEx('data/test_ogr_nc4.nc', gdal.OF_VECTOR)

    with gdaltest.error_handler():
        gdal.VectorTranslate('/vsimem/netcdf_47.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_47.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string3chars,twodimstringchar,date,datetime,datetime_explicit_fillValue,int64,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field,ubyte_field,ubyte_field_explicit_fillValue,ushort_field,ushort_field_explicit_fillValue,uint_field,uint_field_explicit_fillValue,uint64_field,uint64_field_explicit_fillValue
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,,1,1,1.2,1.2,123,12,5,-125,254,255,65534,65535,4000000000,4294967295,1234567890123,
"POINT (1 2)",,,,,,,,,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,,,,,,,,
"""
    assert content == expected_content

    fp = gdal.VSIFOpenL('/vsimem/netcdf_47.csvt', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer,Integer,Integer,Integer,Integer,Integer64,Integer64,Real,Real
"""
    assert content == expected_content
    gdal.Unlink('/vsimem/netcdf_47.csv')
    gdal.Unlink('/vsimem/netcdf_47.csvt')
    gdal.Unlink('/vsimem/netcdf_47.prj')

###############################################################################
# Test reading a vector NetCDF 3 file without any geometry


def test_netcdf_48():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_ogr_no_xyz_var.nc', gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbNone
    f = lyr.GetNextFeature()
    assert f['int32'] == 1

###############################################################################
# Test reading a vector NetCDF 3 file with X,Y,Z vars as float


def test_netcdf_49():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/test_ogr_xyz_float.nc', gdal.OF_VECTOR)
        gdal.VectorTranslate('/vsimem/netcdf_49.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_49.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32
"POINT Z (1 2 3)",1
"POINT (1 2)",
,,
"""
    assert content == expected_content

    gdal.Unlink('/vsimem/netcdf_49.csv')

###############################################################################
# Test creating a vector NetCDF 3 file with WKT geometry field


def test_netcdf_50():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_VECTOR)
    out_ds = gdal.VectorTranslate('tmp/netcdf_50.nc', ds, format='netCDF', layerCreationOptions=['WKT_DEFAULT_WIDTH=1'])
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
    assert src_json == out_json
    out_ds = None

    out_ds = gdal.OpenEx('tmp/netcdf_50.nc', gdal.OF_VECTOR)
    out_lyr = out_ds.GetLayer(0)
    srs = out_lyr.GetSpatialRef().ExportToWkt()
    assert 'PROJCS["OSGB 1936' in srs
    out_f = out_lyr.GetNextFeature()
    out_f.SetFID(-1)
    out_json = out_f.ExportToJson()
    assert src_json == out_json
    out_ds = None

    gdal.Unlink('tmp/netcdf_50.nc')

###############################################################################
# Test creating a vector NetCDF 3 file with X,Y,Z fields


def test_netcdf_51():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('data/test_ogr_nc3.nc', gdal.OF_VECTOR)
    # Test autogrow of string fields
    gdal.VectorTranslate('tmp/netcdf_51.nc', ds, format='netCDF', layerCreationOptions=['STRING_DEFAULT_WIDTH=1'])

    with gdaltest.error_handler():
        ds = gdal.OpenEx('tmp/netcdf_51.nc', gdal.OF_VECTOR)
        gdal.VectorTranslate('/vsimem/netcdf_51.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])
        ds = None

    fp = gdal.VSIFOpenL('/vsimem/netcdf_51.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string1char,string3chars,twodimstringchar,date,datetime_explicit_fillValue,datetime,int64var,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,x,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,1234567890123,1,1,1.2,1.2,123,12,5,-125
"POINT Z (1 2 0)",,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,
"""
    assert content == expected_content

    fp = gdal.VSIFOpenL('/vsimem/netcdf_51.csvt', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(1),String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer
"""
    assert content == expected_content

    ds = gdal.OpenEx('tmp/netcdf_51.nc', gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn('extra', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('extra_str', ogr.OFTString))
    f = lyr.GetNextFeature()
    assert f is not None
    f['extra'] = 5
    f['extra_str'] = 'foobar'
    assert lyr.CreateFeature(f) == 0
    ds = None

    ds = gdal.OpenEx('tmp/netcdf_51.nc', gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    assert f['int32'] == 1 and f['extra'] == 5 and f['extra_str'] == 'foobar'
    f = None
    ds = None

    import netcdf_cf
    netcdf_cf.netcdf_cf_setup()
    if gdaltest.netcdf_cf_method is not None:
        netcdf_cf.netcdf_cf_check_file('tmp/netcdf_51.nc', 'auto', False)

    gdal.Unlink('tmp/netcdf_51.nc')
    gdal.Unlink('tmp/netcdf_51.csv')
    gdal.Unlink('tmp/netcdf_51.csvt')
    gdal.Unlink('/vsimem/netcdf_51.csv')
    gdal.Unlink('/vsimem/netcdf_51.csvt')
    gdal.Unlink('/vsimem/netcdf_51.prj')

###############################################################################
# Test creating a vector NetCDF 3 file with X,Y,Z fields with WRITE_GDAL_TAGS=NO


def test_netcdf_51_no_gdal_tags():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('data/test_ogr_nc3.nc', gdal.OF_VECTOR)
    gdal.VectorTranslate('tmp/netcdf_51_no_gdal_tags.nc', ds, format='netCDF', datasetCreationOptions=['WRITE_GDAL_TAGS=NO'])

    with gdaltest.error_handler():
        ds = gdal.OpenEx('tmp/netcdf_51_no_gdal_tags.nc', gdal.OF_VECTOR)
        gdal.VectorTranslate('/vsimem/netcdf_51_no_gdal_tags.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])
        ds = None

    fp = gdal.VSIFOpenL('/vsimem/netcdf_51_no_gdal_tags.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string1char,string3chars,twodimstringchar,date,datetime_explicit_fillValue,datetime,int64var,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x1,byte_field
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,x,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,1234567890123,1,1,1.2,1.2,123,12,5,-125
"POINT Z (1 2 0)",,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,
"""
    assert content == expected_content

    fp = gdal.VSIFOpenL('/vsimem/netcdf_51_no_gdal_tags.csvt', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(1),String(3),String(10),Date,DateTime,DateTime,Real,Real,Integer,Integer,Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer
"""
    assert content == expected_content

    gdal.Unlink('tmp/netcdf_51_no_gdal_tags.nc')
    gdal.Unlink('tmp/netcdf_51_no_gdal_tags.csv')
    gdal.Unlink('tmp/netcdf_51_no_gdal_tags.csvt')
    gdal.Unlink('/vsimem/netcdf_51_no_gdal_tags.csv')
    gdal.Unlink('/vsimem/netcdf_51_no_gdal_tags.csvt')
    gdal.Unlink('/vsimem/netcdf_51_no_gdal_tags.prj')

###############################################################################
# Test creating a vector NetCDF 4 file with X,Y,Z fields


def test_netcdf_52():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    ds = gdal.OpenEx('data/test_ogr_nc4.nc', gdal.OF_VECTOR)
    gdal.VectorTranslate('tmp/netcdf_52.nc', ds, format='netCDF', datasetCreationOptions=['FORMAT=NC4'])

    with gdaltest.error_handler():
        ds = gdal.OpenEx('tmp/netcdf_52.nc', gdal.OF_VECTOR)
        gdal.VectorTranslate('/vsimem/netcdf_52.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'CREATE_CSVT=YES', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])
        ds = None

    fp = gdal.VSIFOpenL('/vsimem/netcdf_52.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,int32,int32_explicit_fillValue,float64,float64_explicit_fillValue,string3chars,twodimstringchar,date,datetime,datetime_explicit_fillValue,int64,int64var_explicit_fillValue,boolean,boolean_explicit_fillValue,float32,float32_explicit_fillValue,int16,int16_explicit_fillValue,x,byte_field,ubyte_field,ubyte_field_explicit_fillValue,ushort_field,ushort_field_explicit_fillValue,uint_field,uint_field_explicit_fillValue,uint64_field,uint64_field_explicit_fillValue
"POINT Z (1 2 3)",1,1,1.23456789012,1.23456789012,STR,STR,1970/01/02,2016/02/06 12:34:56.789,2016/02/06 12:34:56.789,1234567890123,,1,1,1.2,1.2,123,12,5,-125,254,255,65534,65535,4000000000,4294967295,1234567890123,
"POINT Z (1 2 0)",,,,,,,,,,,,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,,,,,,,,,,,,
"""
    assert content == expected_content

    fp = gdal.VSIFOpenL('/vsimem/netcdf_52.csvt', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,Integer,Integer,Real,Real,String(3),String,Date,DateTime,DateTime,Integer64,Integer64,Integer(Boolean),Integer(Boolean),Real(Float32),Real(Float32),Integer(Int16),Integer(Int16),Real,Integer,Integer,Integer,Integer,Integer,Integer64,Integer64,Real,Real
"""
    assert content == expected_content

    ds = gdal.OpenEx('tmp/netcdf_52.nc', gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn('extra', ogr.OFTInteger))
    f = lyr.GetNextFeature()
    assert f is not None
    f['extra'] = 5
    assert lyr.CreateFeature(f) == 0
    ds = None

    ds = gdal.OpenEx('tmp/netcdf_52.nc', gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    assert f['int32'] == 1 and f['extra'] == 5
    f = None
    ds = None

    import netcdf_cf
    netcdf_cf.netcdf_cf_setup()
    if gdaltest.netcdf_cf_method is not None:
        netcdf_cf.netcdf_cf_check_file('tmp/netcdf_52.nc', 'auto', False)

    gdal.Unlink('tmp/netcdf_52.nc')
    gdal.Unlink('tmp/netcdf_52.csv')
    gdal.Unlink('tmp/netcdf_52.csvt')
    gdal.Unlink('/vsimem/netcdf_52.csv')
    gdal.Unlink('/vsimem/netcdf_52.csvt')
    gdal.Unlink('/vsimem/netcdf_52.prj')

###############################################################################
# Test creating a vector NetCDF 4 file with WKT geometry field


def test_netcdf_53():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_VECTOR)
    out_ds = gdal.VectorTranslate('tmp/netcdf_53.nc', ds, format='netCDF', datasetCreationOptions=['FORMAT=NC4'])
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
    assert src_json == out_json
    out_ds = None

    out_ds = gdal.OpenEx('tmp/netcdf_53.nc', gdal.OF_VECTOR)
    out_lyr = out_ds.GetLayer(0)
    srs = out_lyr.GetSpatialRef().ExportToWkt()
    assert 'PROJCS["OSGB 1936' in srs
    out_f = out_lyr.GetNextFeature()
    out_f.SetFID(-1)
    out_json = out_f.ExportToJson()
    assert src_json == out_json
    out_ds = None

    gdal.Unlink('tmp/netcdf_53.nc')

###############################################################################
# Test appending to a vector NetCDF 4 file with unusual types (ubyte, ushort...)


def test_netcdf_54():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    shutil.copy('data/test_ogr_nc4.nc', 'tmp/netcdf_54.nc')

    ds = gdal.OpenEx('tmp/netcdf_54.nc', gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None
    f['int32'] += 1
    f.SetFID(-1)
    f.ExportToJson()
    src_json = f.ExportToJson()
    assert lyr.CreateFeature(f) == 0
    ds = None

    ds = gdal.OpenEx('tmp/netcdf_54.nc', gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    f.SetFID(-1)
    out_json = f.ExportToJson()
    f = None
    ds = None

    gdal.Unlink('tmp/netcdf_54.nc')

    assert src_json == out_json

###############################################################################
# Test auto-grow of bidimensional char variables in a vector NetCDF 4 file


def test_netcdf_55():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    shutil.copy('data/test_ogr_nc4.nc', 'tmp/netcdf_55.nc')

    ds = gdal.OpenEx('tmp/netcdf_55.nc', gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None
    f['twodimstringchar'] = 'abcd'
    f.SetFID(-1)
    f.ExportToJson()
    src_json = f.ExportToJson()
    assert lyr.CreateFeature(f) == 0
    ds = None

    ds = gdal.OpenEx('tmp/netcdf_55.nc', gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    f.SetFID(-1)
    out_json = f.ExportToJson()
    f = None
    ds = None

    gdal.Unlink('tmp/netcdf_55.nc')

    assert src_json == out_json

###############################################################################
# Test truncation of bidimensional char variables and WKT in a vector NetCDF 3 file


def test_netcdf_56():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_56.nc')
    # Test auto-grow of WKT field
    lyr = ds.CreateLayer('netcdf_56', options=['AUTOGROW_STRINGS=NO', 'STRING_DEFAULT_WIDTH=5', 'WKT_DEFAULT_WIDTH=5'])
    lyr.CreateField(ogr.FieldDefn('txt'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['txt'] = '0123456789'
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    with gdaltest.error_handler():
        ret = lyr.CreateFeature(f)
    assert ret == 0
    ds = None

    ds = gdal.OpenEx('tmp/netcdf_56.nc', gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(lyr.GetFeatureCount())
    if f['txt'] != '01234' or f.GetGeometryRef() is not None:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('tmp/netcdf_56.nc')

###############################################################################
# Test one layer per file creation


def test_netcdf_57():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    try:
        shutil.rmtree('tmp/netcdf_57')
    except OSError:
        pass

    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('netCDF').CreateDataSource('/not_existing_dir/invalid_subdir', options=['MULTIPLE_LAYERS=SEPARATE_FILES'])
    assert ds is None

    open('tmp/netcdf_57', 'wb').close()

    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('netCDF').CreateDataSource('/not_existing_dir/invalid_subdir', options=['MULTIPLE_LAYERS=SEPARATE_FILES'])
    assert ds is None

    os.unlink('tmp/netcdf_57')

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_57', options=['MULTIPLE_LAYERS=SEPARATE_FILES'])
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
        assert f['lyr_id'] == ilayer
        ds = None

    shutil.rmtree('tmp/netcdf_57')

###############################################################################
# Test one layer per group (NC4)


def test_netcdf_58():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_58.nc', options=['FORMAT=NC4', 'MULTIPLE_LAYERS=SEPARATE_GROUPS'])
    for ilayer in range(2):
        # Make sure auto-grow will happen to test this works well with multiple groups
        lyr = ds.CreateLayer('lyr%d' % ilayer, geom_type=ogr.wkbNone, options=['USE_STRING_IN_NC4=NO', 'STRING_DEFAULT_WIDTH=1'])
        lyr.CreateField(ogr.FieldDefn('lyr_id', ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['lyr_id'] = 'lyr_%d' % ilayer
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('tmp/netcdf_58.nc')
    for ilayer in range(2):
        lyr = ds.GetLayer(ilayer)
        f = lyr.GetNextFeature()
        assert f['lyr_id'] == 'lyr_%d' % ilayer
    ds = None

    gdal.Unlink('tmp/netcdf_58.nc')

###############################################################################
# check for UnitType set/get.


def test_netcdf_59():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # get
    ds = gdal.Open('data/unittype.nc')

    unit = ds.GetRasterBand(1).GetUnitType()

    assert unit == 'm/s', ('Incorrect unit(%s)' % unit)

    ds = None

    # set
    tst = gdaltest.GDALTest('NetCDF', 'unittype.nc', 1, 4672)

    return tst.testSetUnitType()

###############################################################################
# Test reading a "Indexed ragged array representation of profiles" v1.6.0 H3.5
# http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html#_indexed_ragged_array_representation_of_profiles


def test_netcdf_60():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # Test that a vector cannot be opened in raster-only mode
    ds = gdal.OpenEx('data/profile.nc', gdal.OF_RASTER)
    assert ds is None

    ds = gdal.OpenEx('data/profile.nc', gdal.OF_VECTOR)
    assert ds is not None

    with gdaltest.error_handler():
        gdal.VectorTranslate('/vsimem/netcdf_60.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_60.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    expected_content = """WKT,profile,id,station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    assert content == expected_content

    gdal.Unlink('/vsimem/netcdf_60.csv')

###############################################################################
# Test appending to a "Indexed ragged array representation of profiles" v1.6.0 H3.5


def test_netcdf_61():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    shutil.copy('data/profile.nc', 'tmp/netcdf_61.nc')
    ds = gdal.VectorTranslate('tmp/netcdf_61.nc', 'data/profile.nc', accessMode='append')
    gdal.VectorTranslate('/vsimem/netcdf_61.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_61.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
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
    assert content == expected_content

    gdal.Unlink('/vsimem/netcdf_61.csv')
    gdal.Unlink('/vsimem/netcdf_61.nc')

###############################################################################
# Test creating a "Indexed ragged array representation of profiles" v1.6.0 H3.5


def test_netcdf_62():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.VectorTranslate('tmp/netcdf_62.nc', 'data/profile.nc', format='netCDF', layerCreationOptions=['FEATURE_TYPE=PROFILE', 'PROFILE_DIM_INIT_SIZE=1', 'PROFILE_VARIABLES=station'])
    gdal.VectorTranslate('/vsimem/netcdf_62.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_62.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile,id,station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    assert content == expected_content

    gdal.Unlink('/vsimem/netcdf_62.csv')


def test_netcdf_62_ncdump_check():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except OSError:
        err = None
    if err is not None and 'netcdf library version' in err:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h tmp/netcdf_62.nc')
        assert ('profile = 2' in ret and \
           'record = UNLIMITED' in ret and \
           'profile:cf_role = "profile_id"' in ret and \
           'parentIndex:instance_dimension = "profile"' in ret and \
           ':featureType = "profile"' in ret and \
           'char station(profile' in ret and \
           'char foo(record' in ret)
    else:
        pytest.skip()

    

def test_netcdf_62_cf_check():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    import netcdf_cf
    netcdf_cf.netcdf_cf_setup()
    if gdaltest.netcdf_cf_method is not None:
        netcdf_cf.netcdf_cf_check_file('tmp/netcdf_62.nc', 'auto', False)

    gdal.Unlink('/vsimem/netcdf_62.nc')

###############################################################################
# Test creating a NC4 "Indexed ragged array representation of profiles" v1.6.0 H3.5


def test_netcdf_63():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    shutil.copy('data/profile.nc', 'tmp/netcdf_63.nc')
    ds = gdal.VectorTranslate('tmp/netcdf_63.nc', 'data/profile.nc', format='netCDF', datasetCreationOptions=['FORMAT=NC4'], layerCreationOptions=['FEATURE_TYPE=PROFILE', 'USE_STRING_IN_NC4=NO', 'STRING_DEFAULT_WIDTH=1'])
    gdal.VectorTranslate('/vsimem/netcdf_63.csv', ds, format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_63.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile,id,station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    assert content == expected_content

    gdal.Unlink('/vsimem/netcdf_63.csv')


def test_netcdf_63_ncdump_check():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except OSError:
        err = None
    if err is not None and 'netcdf library version' in err:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h tmp/netcdf_63.nc')
        assert ('profile = UNLIMITED' in ret and \
           'record = UNLIMITED' in ret and \
           'profile:cf_role = "profile_id"' in ret and \
           'parentIndex:instance_dimension = "profile"' in ret and \
           ':featureType = "profile"' in ret and \
           'char station(record' in ret)
    else:
        gdal.Unlink('/vsimem/netcdf_63.nc')
        pytest.skip()

    gdal.Unlink('/vsimem/netcdf_63.nc')

###############################################################################
# Test creating a "Indexed ragged array representation of profiles" v1.6.0 H3.5
# but without a profile field.


def test_netcdf_64():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    gdal.VectorTranslate('tmp/netcdf_64.nc', 'data/profile.nc', format='netCDF', selectFields=['id,station,foo'], layerCreationOptions=['FEATURE_TYPE=PROFILE', 'PROFILE_DIM_NAME=profile_dim', 'PROFILE_DIM_INIT_SIZE=1'])
    gdal.VectorTranslate('/vsimem/netcdf_64.csv', 'tmp/netcdf_64.nc', format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_64.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile_dim,id,station,foo
"POINT Z (2 49 100)",0,1,Palo Alto,bar
"POINT Z (3 50 50)",1,2,Santa Fe,baz
"POINT Z (2 49 200)",0,3,Palo Alto,baw
"POINT Z (3 50 100)",1,4,Santa Fe,baz2
"""
    assert content == expected_content

    gdal.Unlink('/vsimem/netcdf_64.csv')
    gdal.Unlink('/vsimem/netcdf_64.nc')

###############################################################################
# Test creating a NC4 file with empty string fields / WKT fields
# (they must be filled as empty strings to avoid crashes in netcdf lib)


def test_netcdf_65():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    ds = ogr.GetDriverByName('netCDF').CreateDataSource('tmp/netcdf_65.nc', options=['FORMAT=NC4'])
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('tmp/netcdf_65.nc')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['str'] != '':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('tmp/netcdf_65.nc')

###############################################################################
# Test creating a "Indexed ragged array representation of profiles" v1.6.0 H3.5
# from a config file


def test_netcdf_66():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # First trying with no so good configs

    with gdaltest.error_handler():
        gdal.VectorTranslate('tmp/netcdf_66.nc', 'data/profile.nc', format='netCDF', datasetCreationOptions=['CONFIG_FILE=not_existing'])

    with gdaltest.error_handler():
        gdal.VectorTranslate('tmp/netcdf_66.nc', 'data/profile.nc', format='netCDF', datasetCreationOptions=['CONFIG_FILE=<Configuration>'])

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
        gdal.VectorTranslate('tmp/netcdf_66.nc', 'data/profile.nc', format='netCDF', datasetCreationOptions=['CONFIG_FILE=' + myconfig])

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

    gdal.VectorTranslate('tmp/netcdf_66.nc', 'data/profile.nc', format='netCDF', datasetCreationOptions=['CONFIG_FILE=' + myconfig])
    gdal.VectorTranslate('/vsimem/netcdf_66.csv', 'tmp/netcdf_66.nc', format='CSV', layerCreationOptions=['LINEFORMAT=LF', 'GEOMETRY=AS_WKT', 'STRING_QUOTING=IF_NEEDED'])

    fp = gdal.VSIFOpenL('/vsimem/netcdf_66.csv', 'rb')
    if fp is not None:
        content = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)

    expected_content = """WKT,profile,id,my_station,foo
"POINT Z (2 49 100)",1,1,Palo Alto,bar
"POINT Z (3 50 50)",2,2,Santa Fe,baz
"POINT Z (2 49 200)",1,3,Palo Alto,baw
"POINT Z (3 50 100)",2,4,Santa Fe,baz2
"""
    assert content == expected_content

    gdal.Unlink('/vsimem/netcdf_66.csv')


def test_netcdf_66_ncdump_check():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # get file header with ncdump (if available)
    try:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except OSError:
        err = None
    if err is not None and 'netcdf library version' in err:
        (ret, err) = gdaltest.runexternal_out_and_err('ncdump -h tmp/netcdf_66.nc')
        assert ('char my_station(obs, my_station_max_width)' in ret and \
           'my_station:long_name = "my station attribute"' in ret and \
           'lon:my_extra_lon_attribute = "foo"' in ret and \
           'lat:long_name' not in ret and \
           'id:my_extra_attribute = 5.23' in ret and \
           'profile:cf_role = "profile_id"' in ret and \
           'parentIndex:instance_dimension = "profile"' in ret and \
           ':featureType = "profile"' in ret)
    else:
        gdal.Unlink('/vsimem/netcdf_66.nc')
        pytest.skip()

    gdal.Unlink('/vsimem/netcdf_66.nc')

###############################################################################
# ticket #5950: optimize IReadBlock() and CheckData() handling of partial
# blocks in the x axischeck for partial block reading.


def test_netcdf_67():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    try:
        import numpy
    except ImportError:
        pytest.skip()

    # disable bottom-up mode to use the real file's blocks size
    gdal.SetConfigOption('GDAL_NETCDF_BOTTOMUP', 'NO')
    # for the moment the next test using check_stat does not work, seems like
    # the last pixel (9) of the image is not handled by stats...
#    tst = gdaltest.GDALTest( 'NetCDF', 'partial_block_ticket5950.nc', 1, 45 )
#    result = tst.testOpen( check_stat=(1, 9, 5, 2.582) )
    # so for the moment compare the full image
    ds = gdal.Open('data/partial_block_ticket5950.nc', gdal.GA_ReadOnly)
    ref = numpy.arange(1, 10).reshape((3, 3))
    if not numpy.array_equal(ds.GetRasterBand(1).ReadAsArray(), ref):
        pytest.fail()
    ds = None
    gdal.SetConfigOption('GDAL_NETCDF_BOTTOMUP', None)


###############################################################################
# Test reading SRS from srid attribute (#6613)

def test_netcdf_68():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/srid.nc')
    wkt = ds.GetProjectionRef()
    assert '6933' in wkt

###############################################################################
# Test opening a dataset with a 1D variable with 0 record (#6645)


def test_netcdf_69():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/test6645.nc')
    assert ds is not None

###############################################################################
# Test that we don't erroneously identify non-longitude axis as longitude (#6759)


def test_netcdf_70():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/test6759.nc')
    gt = ds.GetGeoTransform()
    expected_gt = [304250.0, 250.0, 0.0, 4952500.0, 0.0, -250.0]
    assert max(abs(gt[i] - expected_gt[i]) for i in range(6)) <= 1e-3

###############################################################################
# Test that we take into account x and y offset and scaling
# (https://github.com/OSGeo/gdal/pull/200)


def test_netcdf_71():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/test_coord_scale_offset.nc')
    gt = ds.GetGeoTransform()
    expected_gt = (-690769.999174516, 1015.8812500000931, 0.0, 2040932.1838741193, 0.0, 1015.8812499996275)
    assert max(abs(gt[i] - expected_gt[i]) for i in range(6)) <= 1e-3

###############################################################################
# test int64 attributes / dim


def test_netcdf_72():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if not gdaltest.netcdf_drv_has_nc4:
        pytest.skip()

    ds = gdal.Open('data/int64dim.nc')
    mdi = ds.GetRasterBand(1).GetMetadataItem('NETCDF_DIM_TIME')
    assert mdi == '123456789012'

###############################################################################
# test geostationary with radian units (https://github.com/OSGeo/gdal/pull/220)


def test_netcdf_73():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/geos_rad.nc')
    gt = ds.GetGeoTransform()
    expected_gt = (-5979486.362104082, 1087179.4077774752, 0.0, -5979487.123448145, 0.0, 1087179.4077774752)
    assert max([abs(gt[i] - expected_gt[i]) for i in range(6)]) <= 1

###############################################################################
# test geostationary with microradian units (https://github.com/OSGeo/gdal/pull/220)


def test_netcdf_74():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/geos_microradian.nc')
    gt = ds.GetGeoTransform()
    expected_gt = (-5739675.119757546, 615630.8078590936, 0.0, -1032263.7666924844, 0.0, 615630.8078590936)
    assert max([abs(gt[i] - expected_gt[i]) for i in range(6)]) <= 1

###############################################################################
# test opening a ncdump file


def test_netcdf_75():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if gdaltest.netcdf_drv.GetMetadataItem("ENABLE_NCDUMP") != 'YES':
        pytest.skip()

    tst = gdaltest.GDALTest('NetCDF', 'byte.nc.txt',
                            1, 4672)

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

    return tst.testOpen(check_prj=wkt)

###############################################################################
# test opening a vector ncdump file


def test_netcdf_76():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if gdaltest.netcdf_drv.GetMetadataItem("ENABLE_NCDUMP") != 'YES':
        pytest.skip()

    ds = ogr.Open('data/poly.nc.txt')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None or f.GetGeometryRef() is None:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################
# test opening a raster file that used to be confused with a vector file (#6974)


def test_netcdf_77():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/fake_Oa01_radiance.nc')
    subdatasets = ds.GetMetadata('SUBDATASETS')
    assert len(subdatasets) == 2 * 2

    ds = gdal.Open('NETCDF:"data/fake_Oa01_radiance.nc":Oa01_radiance')
    assert not ds.GetMetadata('GEOLOCATION')


###############################################################################
# test we handle correctly valid_range={0,255} for a byte dataset with
# negative nodata value

def test_netcdf_78():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/byte_with_valid_range.nc')
    assert ds.GetRasterBand(1).GetNoDataValue() == 240
    data = ds.GetRasterBand(1).ReadRaster()
    data = struct.unpack('B' * 4, data)
    assert data == (128, 129, 126, 127)

###############################################################################
# test we handle correctly _Unsigned="true" for a byte dataset with
# negative nodata value


def test_netcdf_79():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/byte_with_neg_fillvalue_and_unsigned_hint.nc')
    assert ds.GetRasterBand(1).GetNoDataValue() == 240
    data = ds.GetRasterBand(1).ReadRaster()
    data = struct.unpack('B' * 4, data)
    assert data == (128, 129, 126, 127)

###############################################################################
# Test creating and opening with accent


def test_netcdf_80():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    test = gdaltest.GDALTest('NETCDF', '../data/byte.tif', 1, 4672)
    return test.testCreateCopy(new_filename='test\xc3\xa9.nc', check_gt=0, check_srs=0, check_minmax=0)

###############################################################################
# netCDF file in rotated_pole projection


def test_netcdf_81():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/rotated_pole.nc')

    assert ds.RasterXSize == 137 and ds.RasterYSize == 108, \
        'Did not get expected dimensions'

    projection = ds.GetProjectionRef()
    expected_projection = """PROJCS["unnamed",GEOGCS["unknown",DATUM["unnamed",SPHEROID["Spheroid",6367470,594.313048347956]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Rotated_pole"],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],EXTENSION["PROJ4","+proj=ob_tran +o_proj=longlat +lon_0=18 +o_lon_p=0 +o_lat_p=39.25 +a=6367470 +b=6367470 +to_meter=0.0174532925199 +wktext"]]"""
    assert projection == expected_projection, 'Did not get expected projection'

    gt = ds.GetGeoTransform()
    expected_gt = (-35.47, 0.44, 0.0, 23.65, 0.0, -0.44)
    assert max([abs(gt[i] - expected_gt[i]) for i in range(6)]) <= 1e-3, \
        'Did not get expected geotransform'

###############################################################################
# netCDF file with extra dimensions that are oddly indexed (1D variable
# corresponding to the dimension but with a different name, no corresponding
# 1D variable, several corresponding variables)


def test_netcdf_82():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/oddly_indexed_extra_dims.nc')
    md = ds.GetMetadata()
    expected_md = {
        'NETCDF_DIM_extra_dim_with_var_of_different_name_VALUES': '{100,200}',
        'NETCDF_DIM_EXTRA': '{extra_dim_with_several_variables,extra_dim_without_variable,extra_dim_with_var_of_different_name}',
        'x#standard_name': 'projection_x_coordinate',
        'NC_GLOBAL#Conventions': 'CF-1.5',
        'y#standard_name': 'projection_y_coordinate',
        'NETCDF_DIM_extra_dim_with_var_of_different_name_DEF': '{2,6}'
    }
    assert md == expected_md, 'Did not get expected metadata'

    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {
        'NETCDF_DIM_extra_dim_with_several_variables': '1',
        'NETCDF_DIM_extra_dim_with_var_of_different_name': '100',
        'NETCDF_DIM_extra_dim_without_variable': '1',
        'NETCDF_VARNAME': 'data'
    }
    assert md == expected_md, 'Did not get expected metadata'

###############################################################################
# Test complex data subsets


def test_netcdf_83():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/complex.nc')
    sds_list = ds.GetMetadata('SUBDATASETS')

    assert len(sds_list) == 6, 'Did not get expected complex subdataset count.'

    assert sds_list['SUBDATASET_1_NAME'] == 'NETCDF:"data/complex.nc":f32' and sds_list['SUBDATASET_2_NAME'] == 'NETCDF:"data/complex.nc":f64' and sds_list['SUBDATASET_3_NAME'] == 'NETCDF:"data/complex.nc":/group/fmul', \
        'did not get expected subdatasets.'

    ds = None

    assert not gdaltest.is_file_open('data/complex.nc'), 'file still opened.'

###############################################################################
# Confirm complex subset data access and checksum
# Start with Float32


def test_netcdf_84():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('NETCDF:"data/complex.nc":f32')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_CFloat32

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 523, 'did not get expected checksum'

# Repeat for Float64


def test_netcdf_85():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('NETCDF:"data/complex.nc":f64')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_CFloat64

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 511, 'did not get expected checksum'


# Check for groups support

def test_netcdf_86():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('NETCDF:"data/complex.nc":/group/fmul')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_CFloat32

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 453, 'did not get expected checksum for band 1'

    cs = ds.GetRasterBand(2).Checksum()
    assert cs == 629, 'did not get expected checksum for band 2'

    cs = ds.GetRasterBand(3).Checksum()
    assert cs == 473, 'did not get expected checksum for band 3'

###############################################################################
def test_netcdf_uffd():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    if uffd_compare('orog_CRCM1.nc') is None:
        pytest.skip()

    netcdf_files = [
        'orog_CRCM1.nc',
        'orog_CRCM2.nc',
        'cf-bug636.nc',
        'bug636.nc',
        'rotated_pole.nc',
        'reduce-cgcms.nc'
    ]
    for netcdf_file in netcdf_files:
        assert uffd_compare(netcdf_file) is True

###############################################################################
# netCDF file containing both rasters and vectors

def test_netcdf_mixed_raster_vector():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('NETCDF:data/nc_mixed_raster_vector.nc:Band1')
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = ogr.Open('data/nc_mixed_raster_vector.nc')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f['PRFEDEA'] == '35043411'


###############################################################################
# Test opening a file with an empty double attribute
# https://github.com/OSGeo/gdal/issues/1303

def test_netcdf_open_empty_double_attr():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/empty_double_attr.nc')
    assert ds


###############################################################################
# Test writing and reading a file with huge block size

def test_netcdf_huge_block_size():

    if gdaltest.netcdf_drv is None:
        pytest.skip()
    if not gdaltest.run_slow_tests():
        pytest.skip()
    if sys.maxsize < 2**32:
        pytest.skip('Test not available on 32 bit')

    import psutil
    if psutil.virtual_memory().available < 2 * 50000 * 50000:
        pytest.skip("Not enough virtual memory available")

    tmpfilename = 'tmp/test_netcdf_huge_block_size.nc'
    with gdaltest.SetCacheMax(50000 * 50000 + 100000):
        with gdaltest.config_option('BLOCKYSIZE', '50000'):
            gdal.Translate(tmpfilename,
                        '../gcore/data/byte.tif',
                        options='-f netCDF -outsize 50000 50000 -co WRITE_BOTTOMUP=NO -co COMPRESS=DEFLATE -co FORMAT=NC4')

    ds = gdal.Open(tmpfilename)
    data  = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buf_xsize = 20, buf_ysize = 20)
    assert data
    ref_ds = gdal.Open('../gcore/data/byte.tif')
    assert data == ref_ds.ReadRaster()
    ds = None

    gdal.Unlink(tmpfilename)


###############################################################################
# Test reading a netCDF file whose fastest varying dimension is Latitude, and
# slowest one is Longitude
# https://lists.osgeo.org/pipermail/gdal-dev/2019-March/049931.html
# Currently we expose it in a 'raw' way, but make sure that geotransform and
# geoloc arrays reflect the georeferencing correctly

def test_netcdf_swapped_x_y_dimension():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ds = gdal.Open('data/swapedxy.nc')
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 8
    assert ds.GetGeoTransform() == (90.0, -45.0, 0, -180, 0.0, 45.0)
    data = ds.GetRasterBand(1).ReadRaster()
    data = struct.unpack('h' * 4 * 8, data)
    assert data == (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ,13 ,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31)
    md = ds.GetMetadata('GEOLOCATION')
    assert md == {
        'LINE_OFFSET': '0',
        'X_DATASET': 'NETCDF:"data/swapedxy.nc":Latitude',
        'SWAP_XY': 'YES',
        'PIXEL_STEP': '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        'PIXEL_OFFSET': '0',
        'X_BAND': '1',
        'LINE_STEP': '1',
        'Y_DATASET': 'NETCDF:"data/swapedxy.nc":Longitude',
        'Y_BAND': '1'}, md

    ds = gdal.Open(md['X_DATASET'])
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 1
    data = ds.GetRasterBand(1).ReadRaster()
    data = struct.unpack('f' * 4, data)
    assert data == (67.5, 22.5, -22.5, -67.5)

    ds = gdal.Open(md['Y_DATASET'])
    assert ds.RasterXSize == 8
    assert ds.RasterYSize == 1
    data = ds.GetRasterBand(1).ReadRaster()
    data = struct.unpack('f' * 8, data)
    assert data == (-157.5, -112.5, -67.5, -22.5, 22.5, 67.5, 112.5, 157.5)

    ds = gdal.Warp('', 'data/swapedxy.nc', options = '-f MEM -geoloc')
    assert ds.RasterXSize == 8
    assert ds.RasterYSize == 4
    assert ds.GetGeoTransform() == (-157.5, 38.3161193233344, 0.0, 67.5, 0.0, -38.3161193233344)
    data = ds.GetRasterBand(1).ReadRaster()
    data = struct.unpack('h' * 4 * 8, data)
    # not exactly the transposed aray, but not so far
    assert data == (4, 8, 8, 12, 16, 20, 20, 24, 5, 9, 9, 13, 17, 21, 21, 25, 6, 10, 10, 14, 18, 22, 22, 26, 7, 11, 11, 15, 19, 23, 23, 27)

###############################################################################

###############################################################################
# main tests list



###############################################################################
#  basic file creation tests

init_list = [
    ('byte.tif', 4672, []),
    ('byte_signed.tif', 4672, ['PIXELTYPE=SIGNEDBYTE']),
    ('int16.tif', 4672, []),
    ('int32.tif', 4672, []),
    ('float32.tif', 4672, []),
    ('float64.tif', 4672, [])
]


# Some tests we don't need to do for each type.

@pytest.mark.parametrize(
    'testfunction', [
        'testSetGeoTransform',
        'testSetProjection',
        # SetMetadata() not supported
        # 'testSetMetadata'
    ]
)
@pytest.mark.require_driver('netcdf')
def test_netcdf_functions_1(testfunction):
    ut = gdaltest.GDALTest('netcdf', 'byte.tif', 1, 4672, options=[])
    getattr(ut, testfunction)()


# Others we do for each pixel type.

@pytest.mark.parametrize(
    'filename,checksum,options',
    init_list,
    ids=[tup[0].split('.')[0] for tup in init_list],
)
@pytest.mark.parametrize(
    'testfunction', [
        'testCreateCopy',
        'testCreate',
        'testSetNoDataValue'
    ]
)
@pytest.mark.require_driver('netcdf')
def test_netcdf_functions_2(filename, checksum, options, testfunction):
    ut = gdaltest.GDALTest('netcdf', filename, 1, checksum, options=options)
    getattr(ut, testfunction)()

###############################################################################
#  other tests

