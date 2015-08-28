#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Various test of GDAL core.
# Author:   Even Rouault <even dot rouault at mines dash parid dot org>
# 
###############################################################################
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

from osgeo import gdal
import sys
import os
import shutil

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Test that the constructor of GDALDataset() behaves well with a big number of
# opened/created datasets

def misc_1():

    tab_ds = [None for i in range(5000)]
    drv = gdal.GetDriverByName('MEM')
    for i in range(len(tab_ds)):
        name = 'mem_%d' % i
        tab_ds[i] = drv.Create( name, 1, 1, 1 )
        if tab_ds[i] is None:
            return 'fail'

    for i in range(len(tab_ds)):
        tab_ds[i] = None

    return 'success'

###############################################################################
# Test that OpenShared() works as expected by opening a big number of times
# the same dataset with it. If it didn't work, that would exhaust the system limit
# of maximum file descriptors opened at the same time

def misc_2():

    tab_ds = [None for i in range(5000)]
    for i in range(len(tab_ds)):
        tab_ds[i] = gdal.OpenShared('data/byte.tif')
        if tab_ds[i] is None:
            return 'fail'

    for i in range(len(tab_ds)):
        tab_ds[i] = None

    return 'success'

###############################################################################
# Test OpenShared() with a dataset whose filename != description (#2797)

def misc_3():

    with gdaltest.error_handler():
        ds = gdal.OpenShared('../gdrivers/data/small16.aux')
    ds.GetRasterBand(1).Checksum()
    cache_size = gdal.GetCacheUsed()

    with gdaltest.error_handler():
        ds2 = gdal.OpenShared('../gdrivers/data/small16.aux')
    ds2.GetRasterBand(1).Checksum()
    cache_size2 = gdal.GetCacheUsed()

    if cache_size != cache_size2:
        print("--> OpenShared didn't work as expected")

    ds = None
    ds2 = None

    return 'success'

###############################################################################
# Test Create() with invalid arguments

def misc_4():

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    # Test a few invalid argument
    drv = gdal.GetDriverByName('GTiff')
    drv.Create('tmp/foo', 0, 100, 1)
    drv.Create('tmp/foo', 100, 1, 1)
    drv.Create('tmp/foo', 100, 100, -1)
    drv.Delete('tmp/foo')

    gdal.PopErrorHandler()

    return 'success'

###############################################################################
# Test Create() with various band numbers (including 0) and datatype

def misc_5_internal(drv, datatype, nBands):

    dirname = 'tmp/tmp/tmp_%s_%d_%s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
    #print('drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype)))
    try:
        os.mkdir(dirname)
    except:
        try:
            os.stat(dirname)
            # Hum the directory already exists... Not expected, but let's try to go on
        except:
            reason = 'Cannot create %s for drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
            gdaltest.post_reason(reason)
            return 0

    filename = '%s/foo' % dirname
    if drv.ShortName == 'GTX':
        filename = filename + '.gtx'
    elif drv.ShortName == 'RST':
        filename = filename + '.rst'
    elif drv.ShortName == 'SAGA':
        filename = filename + '.sdat'
    elif drv.ShortName == 'ADRG':
        filename = '%s/ABCDEF01.GEN' % dirname
    elif drv.ShortName == 'SRTMHGT':
        filename = '%s/N48E002.HGT' % dirname
    elif drv.ShortName == 'ECW':
        filename = filename + '.ecw'
    elif drv.ShortName == 'KMLSUPEROVERLAY':
        filename = filename + '.kmz'
    ds = drv.Create(filename, 100, 100, nBands, datatype)
    if ds is not None and not (drv.ShortName == 'GPKG' and nBands == 0):
        set_gt = (2,1.0/10,0,49,0,-1.0/10)
        ds.SetGeoTransform(set_gt)
        ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]')

        # PNM and MFF have no SetGeoTransform() method implemented
        if drv.ShortName not in ['PNM', 'MFF']:
            got_gt = ds.GetGeoTransform()
            for i in range(6):
                if abs(got_gt[i] - set_gt[i]) > 1e-10:
                    print('Did not get expected GT for drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype)))
                    print(got_gt)
                    return -1
        
        #if ds.RasterCount > 0:
        #    ds.GetRasterBand(1).Fill(255)
    ds = None
    ds = gdal.Open(filename)
    if ds is None:
        # reason = 'Cannot reopen %s for drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        # gdaltest.post_reason(reason)
        # TODO: Why not return -1?
        pass
    #else:
    #    if ds.RasterCount > 0:
    #        print ds.GetRasterBand(1).Checksum()
    ds = None

    try:
        shutil.rmtree(dirname)
    except:
        reason = 'Cannot remove %s for drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        gdaltest.post_reason(reason)
        return 0

    return 1

def misc_5():

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    try:
        shutil.rmtree('tmp/tmp')
    except:
        pass
        
    try:
        os.mkdir('tmp/tmp')
    except:
        try:
            os.stat('tmp/tmp')
            # Hum the directory already exists... Not expected, but let's try to go on
        except:
            gdaltest.post_reason('Cannot create tmp/tmp')
            return 'fail'

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

    ret = 'success'

    # Test Create() with various band numbers, including 0
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        md = drv.GetMetadata()
        if drv.ShortName == 'PDF':
            # PDF Create() is vector-only
            continue
        if 'DCAP_CREATE' in md and 'DCAP_RASTER' in md:
            datatype = gdal.GDT_Byte
            for nBands in range(6):
                if misc_5_internal(drv, datatype, nBands) < 0:
                    ret = 'fail'

            for nBands in [1,3]:
                for datatype in (gdal.GDT_UInt16,
                                gdal.GDT_Int16,
                                gdal.GDT_UInt32,
                                gdal.GDT_Int32,
                                gdal.GDT_Float32,
                                gdal.GDT_Float64,
                                gdal.GDT_CInt16,
                                gdal.GDT_CInt32,
                                gdal.GDT_CFloat32,
                                gdal.GDT_CFloat64):
                    if misc_5_internal(drv, datatype, nBands) < 0:
                        ret = 'fail'

    gdal.PopErrorHandler()

    return ret


###############################################################################
class misc_6_interrupt_callback_class:
    def __init__(self):
        pass

    def cbk(self, pct, message, user_data):
        if pct > 0.5:
            return 0 # to stop
        else:
            return 1 # to continue

###############################################################################
# Test CreateCopy() with a source dataset with various band numbers (including 0) and datatype

def misc_6_internal(datatype, nBands):
    if nBands == 0:
        ds = gdal.GetDriverByName('ILWIS').Create('tmp/tmp.mpl', 100, 100, nBands, datatype)
    else:
        ds = gdal.GetDriverByName('MEM').Create('', 10, 10, nBands, datatype)
        ds.GetRasterBand(1).Fill(255)
        ds.SetGeoTransform([2,1.0/10,0,49,0,-1.0/10])
        ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]')
        ds.SetMetadata(['a'])
    if ds is not None:
        for i in range(gdal.GetDriverCount()):
            drv = gdal.GetDriver(i)
            md = drv.GetMetadata()
            if 'DCAP_CREATECOPY' in md or 'DCAP_CREATE' in md and 'DCAP_RASTER' in md:
                #print ('drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype)))

                skip = False
                # FIXME: A few cases that crashes and should be investigated
                if drv.ShortName == 'JPEG2000':
                    if (nBands == 2 or nBands >= 5) or \
                        not (datatype == gdal.GDT_Byte or datatype == gdal.GDT_Int16 or datatype == gdal.GDT_UInt16):
                            skip = True
                if drv.ShortName == 'JP2ECW' and datatype == gdal.GDT_Float64:
                    skip = True

                if skip is False:
                    dirname = 'tmp/tmp/tmp_%s_%d_%s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
                    try:
                        os.mkdir(dirname)
                    except:
                        try:
                            os.stat(dirname)
                            # Hum the directory already exists... Not expected, but let's try to go on
                        except:
                            reason = 'Cannot create %s before drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
                            gdaltest.post_reason(reason)
                            return 'fail'

                    filename = '%s/foo' % dirname
                    if drv.ShortName == 'GTX':
                        filename = filename + '.gtx'
                    elif drv.ShortName == 'RST':
                        filename = filename + '.rst'
                    elif drv.ShortName == 'SAGA':
                        filename = filename + '.sdat'
                    elif drv.ShortName == 'ADRG':
                        filename = '%s/ABCDEF01.GEN' % dirname
                    elif drv.ShortName == 'SRTMHGT':
                        filename = '%s/N48E002.HGT' % dirname
                    elif drv.ShortName == 'ECW':
                        filename = filename + '.ecw'
                    elif drv.ShortName == 'KMLSUPEROVERLAY':
                        filename = filename + '.kmz'

                    dst_ds = drv.CreateCopy(filename, ds)
                    has_succeeded = dst_ds is not None
                    dst_ds = None

                    try:
                        shutil.rmtree(dirname)
                    except:
                        reason = 'Cannot remove %s after drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
                        gdaltest.post_reason(reason)
                        return 'fail'

                    if has_succeeded and not drv.ShortName in ['ECW', 'JP2ECW', 'VRT', 'XPM', 'JPEG2000', 'FIT', 'RST', 'INGR', 'USGSDEM', 'KMLSUPEROVERLAY', 'GMT']:
                        dst_ds = drv.CreateCopy(filename, ds, callback = misc_6_interrupt_callback_class().cbk)
                        if dst_ds is not None:
                            gdaltest.post_reason('interruption did not work with drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype)))
                            dst_ds = None

                            try:
                                shutil.rmtree(dirname)
                            except:
                                pass

                            return 'fail'

                        dst_ds = None

                        try:
                            shutil.rmtree(dirname)
                        except:
                            pass
                        try:
                            os.mkdir(dirname)
                        except:
                            reason = 'Cannot create %s before drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
                            gdaltest.post_reason(reason)
                            return 'fail'
        ds = None
        if nBands == 0:
            gdal.GetDriverByName('ILWIS').Delete('tmp/tmp.mpl')
    return 'success'

def misc_6():

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    try:
        shutil.rmtree('tmp/tmp')
    except:
        pass

    try:
        os.mkdir('tmp/tmp')
    except:
        try:
            os.stat('tmp/tmp')
            # Hum the directory already exists... Not expected, but let's try to go on
        except:
            gdaltest.post_reason('Cannot create tmp/tmp')
            return 'fail'

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

    datatype = gdal.GDT_Byte
    for nBands in range(6):
        ret = misc_6_internal(datatype, nBands)
        if ret != 'success':
            return ret

    nBands = 1
    for datatype in (gdal.GDT_UInt16,
                     gdal.GDT_Int16,
                     gdal.GDT_UInt32,
                     gdal.GDT_Int32,
                     gdal.GDT_Float32,
                     gdal.GDT_Float64,
                     gdal.GDT_CInt16,
                     gdal.GDT_CInt32,
                     gdal.GDT_CFloat32,
                     gdal.GDT_CFloat64):
        ret = misc_6_internal(datatype, nBands)
        if ret != 'success':
            return ret

    gdal.PopErrorHandler()

    return 'success'

###############################################################################
# Test gdal.InvGeoTransform()

def misc_7():

    try:
        gdal.InvGeoTransform
    except:
        return 'skip'

    gt = (10, 0.1, 0, 20, 0, -1.0)
    res = gdal.InvGeoTransform(gt)
    expected_inv_gt = (-100.0, 10.0, 0.0, 20.0, 0.0, -1.0)
    for i in range(6):
        if abs(res[i] - expected_inv_gt[i]) > 1e-6:
            print(res)
            return 'fail'

    return 'success'

###############################################################################
# Test gdal.ApplyGeoTransform()

def misc_8():

    try:
        gdal.ApplyGeoTransform
    except:
        return 'skip'

    gt = (10, 0.1, 0, 20, 0, -1.0)
    res = gdal.ApplyGeoTransform(gt, 10, 1)
    if res != [11.0, 19.0]:
        return 'fail'

    return 'success'

###############################################################################
# Test setting and retrieving > 2 GB values for GDAL max cache (#3689)

def misc_9():

    old_val = gdal.GetCacheMax()
    gdal.SetCacheMax(3000000000)
    ret_val = gdal.GetCacheMax()
    gdal.SetCacheMax(old_val)

    if ret_val != 3000000000:
        gdaltest.post_reason('did not get expected value')
        print(ret_val)
        return 'fail'

    return 'success'


###############################################################################
# Test VSIBufferedReaderHandle (fix done in r21358)

def misc_10():

    f = gdal.VSIFOpenL('/vsigzip/./data/byte.tif.gz', 'rb')
    gdal.VSIFReadL(1, 1, f)
    gdal.VSIFSeekL(f, 0, 2)
    gdal.VSIFSeekL(f, 0, 0)
    data = gdal.VSIFReadL(1, 4, f)
    gdal.VSIFCloseL(f)

    import struct
    ar = struct.unpack('B' * 4, data)
    if ar != (73, 73, 42, 0):
        return 'fail'

    return 'success'


###############################################################################
# Test that we can open a symlink whose pointed filename isn't a real
# file, but a filename that GDAL recognizes

def misc_11():

    if not gdaltest.support_symlink():
        return 'skip'

    try:
        os.unlink('tmp/symlink.tif')
    except:
        pass
    os.symlink('GTIFF_DIR:1:data/byte.tif', 'tmp/symlink.tif')

    ds = gdal.Open('tmp/symlink.tif')
    if ds is None:
        os.remove('tmp/symlink.tif')
        return 'fail'
    desc = ds.GetDescription()
    ds = None

    os.remove('tmp/symlink.tif')

    if desc != 'GTIFF_DIR:1:data/byte.tif':
        gdaltest.post_reason('did not get expected description')
        print(desc)
        return 'fail'

    return 'success'

###############################################################################
# Test CreateCopy() with a target filename in a non-existing dir

def misc_12():

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would crash')
        return 'skip'

    import test_cli_utilities
    gdal_translate_path = test_cli_utilities.get_gdal_translate_path()

    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        #if drv.ShortName == 'ECW' or drv.ShortName == 'JP2ECW':
        #    continue
        md = drv.GetMetadata()
        if 'DCAP_CREATECOPY' in md or 'DCAP_CREATE' in md and 'DCAP_RASTER' in md:

            ext = ''
            if drv.ShortName == 'GTX':
                ext = '.gtx'
            elif drv.ShortName == 'RST':
                ext = '.rst'
            elif drv.ShortName == 'SAGA':
                ext = '.sdat'
            elif drv.ShortName == 'ECW':
                ext = '.ecw'
            elif drv.ShortName == 'KMLSUPEROVERLAY':
                ext = '.kmz'
            elif drv.ShortName == 'ADRG':
                ext = '/ABCDEF01.GEN'
            elif drv.ShortName == 'SRTMHGT':
                ext = '/N48E002.HGT'

            nbands = 1
            if drv.ShortName == 'WEBP' or drv.ShortName == 'ADRG':
                nbands = 3

            datatype = gdal.GDT_Byte
            if drv.ShortName == 'BT' or drv.ShortName == 'BLX':
                datatype = gdal.GDT_Int16
            elif drv.ShortName == 'GTX' or drv.ShortName == 'NTv2' or drv.ShortName == 'Leveller' :
                datatype = gdal.GDT_Float32

            size = 1201
            if drv.ShortName == 'BLX':
                size = 128

            src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/misc_12_src.tif', size, size, nbands, datatype)
            set_gt = (2,1.0/size,0,49,0,-1.0/size)
            src_ds.SetGeoTransform(set_gt)
            src_ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]')

            # Test to detect crashes
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ds = drv.CreateCopy('/nonexistingpath/nonexistingfile' + ext, src_ds)
            gdal.PopErrorHandler()
            if ds is None and gdal.GetLastErrorMsg() == '':
                gdaltest.post_reason('failure')
                print('CreateCopy() into non existing dir fails without error message for driver %s' % drv.ShortName)
                return 'fail'
            ds = None

            if gdal_translate_path is not None:
                # Test to detect memleaks
                ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/misc_12.vrt', src_ds)
                (out, err) = gdaltest.runexternal_out_and_err(gdal_translate_path + ' -of ' + drv.ShortName + ' tmp/misc_12.vrt /nonexistingpath/nonexistingfile' + ext, check_memleak = False)
                del ds
                gdal.Unlink('tmp/misc_12.vrt')

                # If DEBUG_VSIMALLOC_STATS is defined, this is an easy way
                # to catch some memory leaks
                if out.find('VSIMalloc + VSICalloc - VSIFree') != -1 and \
                out.find('VSIMalloc + VSICalloc - VSIFree : 0') == -1:
                    if drv.ShortName == 'Rasterlite' and out.find('VSIMalloc + VSICalloc - VSIFree : 1') != -1:
                        pass
                    else:
                        print('memleak detected for driver %s' % drv.ShortName)

            src_ds = None

            gdal.Unlink('/vsimem/misc_12_src.tif')

    return 'success'

###############################################################################
# Test CreateCopy() with incompatible driver types (#5912)

def misc_13():

    # Raster-only -> vector-only
    ds = gdal.Open('data/byte.tif')
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('ESRI Shapefile').CreateCopy('/vsimem/out.shp', ds)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Raster-only -> vector-only
    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_VECTOR)
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/out.tif', ds)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
def misc_cleanup():

    try:
        shutil.rmtree('tmp/tmp')
    except:
        pass

    return 'success'
    
gdaltest_list = [ misc_1,
                  misc_2,
                  misc_3,
                  misc_4,
                  misc_5,
                  misc_6,
                  misc_7,
                  misc_8,
                  misc_9,
                  misc_10,
                  misc_11,
                  misc_12,
                  misc_13,
                  misc_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'misc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
