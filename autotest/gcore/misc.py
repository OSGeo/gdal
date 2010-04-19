#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Various test of GDAL core.
# Author:   Even Rouault <even dot rouault at mines dash parid dot org>
# 
###############################################################################
# Copyright (c) 2009 Even Rouault <even dot rouault at mines dash parid dot org>
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

import gdal
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

    ds = gdal.OpenShared('../gdrivers/data/small16.aux')
    ds.GetRasterBand(1).Checksum()
    cache_size = gdal.GetCacheUsed()

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
    print('drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype)))
    try:
        os.mkdir(dirname)
    except:
        try:
            os.stat(dirname)
            # Hum the directory already exists... Not expected, but let's try to go on
        except:
            reason = 'Cannot create %s for drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
            gdaltest.post_reason(reason)
            return

    filename = '%s/foo' % dirname
    if drv.ShortName == 'GTX':
        filename = filename + '.gtx'
    elif drv.ShortName == 'RST':
        filename = filename + '.rst'
    elif drv.ShortName == 'SAGA':
        filename = filename + '.sdat'
    elif drv.ShortName == 'ADRG':
        filename = '%s/ABCDEF01.GEN' % dirname
    ds = drv.Create(filename, 100, 100, nBands, datatype)
    if ds is not None:
        ds.SetGeoTransform([2,1.0/10,0,49,0,-1.0/10])
        ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]')
        #if ds.RasterCount > 0:
        #    ds.GetRasterBand(1).Fill(255)
    ds = None
    ds = gdal.Open(filename)
    if ds is None:
        reason = 'Cannot reopen %s for drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        gdaltest.post_reason(reason)
    #else:
    #    if ds.RasterCount > 0:
    #        print ds.GetRasterBand(1).Checksum()
    ds = None

    try:
        shutil.rmtree(dirname)
    except:
        reason = 'Cannot remove %s for drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        gdaltest.post_reason(reason)
        return

    return

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
            os.stat(dirname)
            # Hum the directory already exists... Not expected, but let's try to go on
        except:
            gdaltest.post_reason('Cannot create tmp/tmp')
            return 'fail'
            
    # Test Create() with various band numbers, including 0
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        md = drv.GetMetadata()
        if 'DCAP_CREATE' in md:
            datatype = gdal.GDT_Byte
            for nBands in range(6):
                misc_5_internal(drv, datatype, nBands)

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
                    misc_5_internal(drv, datatype, nBands)

    gdal.PopErrorHandler()

    return 'success'

###############################################################################
# Test CreateCopy() with a source dataset with various band numbers (including 0) and datatype

def misc_6_internal(datatype, nBands):
    if nBands == 0:
        ds = gdal.GetDriverByName('ILWIS').Create('tmp/tmp.mpl', 100, 100, nBands, datatype)
    else:
        ds = gdal.GetDriverByName('GTiff').Create('tmp/tmp.tif', 10, 10, nBands, datatype)
        ds.GetRasterBand(1).Fill(255)
        ds.SetGeoTransform([2,1.0/10,0,49,0,-1.0/10])
        ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]')
        ds = None
        ds = gdal.Open('tmp/tmp.tif')
    if ds is not None:
        for i in range(gdal.GetDriverCount()):
            drv = gdal.GetDriver(i)
            md = drv.GetMetadata()
            if 'DCAP_CREATECOPY' in md or 'DCAP_CREATE' in md:
                print ('drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype)))

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
                    dst_ds = drv.CreateCopy(filename, ds)
                    dst_ds = None

                    try:
                        shutil.rmtree(dirname)
                    except:
                        reason = 'Cannot remove %s after drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
                        gdaltest.post_reason(reason)
                        return 'fail'
        ds = None
        if nBands == 0:
            gdal.GetDriverByName('ILWIS').Delete('tmp/tmp.mpl')
        else:
            gdal.GetDriverByName('GTiff').Delete('tmp/tmp.tif')
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
            os.stat(dirname)
            # Hum the directory already exists... Not expected, but let's try to go on
        except:
            gdaltest.post_reason('Cannot create tmp/tmp')
            return 'fail'
        
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
    if res[0] != 1:
        print(res)
        return 'fail'

    expected_inv_gt = (-100.0, 10.0, 0.0, 20.0, 0.0, -1.0)
    for i in range(6):
        if abs(res[1][i] - expected_inv_gt[i]) > 1e-6:
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
                  misc_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'misc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

