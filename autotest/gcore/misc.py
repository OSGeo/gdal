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
        print "--> OpenShared didn't work as expected"

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
    #print 'drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
    try:
        os.mkdir('tmp/tmp')
    except:
        reason = 'Cannot create tmp/tmp for drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        gdaltest.post_reason(reason)
        return

    ds = drv.Create('tmp/tmp/foo', 100, 100, nBands, datatype)
    ds = None

    try:
        shutil.rmtree('tmp/tmp')
    except:
        reason = 'Cannot remove tmp/tmp for drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        gdaltest.post_reason(reason)
        return

    return

def misc_5():

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    try:
        shutil.rmtree('tmp/tmp')
    except:
        pass

    # Test Create() with various band numbers, including 0
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        md = drv.GetMetadata()
        if md.has_key('DCAP_CREATE'):
            datatype = gdal.GDT_Byte
            for nBands in range(6):
                misc_5_internal(drv, datatype, nBands)

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
            if md.has_key('DCAP_CREATECOPY') or md.has_key('DCAP_CREATE'):
                #print 'drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))

                skip = False
                # FIXME: A few cases that crashes and should be investigated
                if drv.ShortName == 'JPEG2000':
                    if (nBands == 2 or nBands >= 5) or \
                        not (datatype == gdal.GDT_Byte or datatype == gdal.GDT_Int16 or datatype == gdal.GDT_UInt16):
                            skip = True
                if drv.ShortName == 'JP2ECW' and datatype == gdal.GDT_Float64:
                    skip = True
                if drv.ShortName == 'NITF' and gdal.DataTypeIsComplex(datatype) == 1:
                    skip = True

                if skip is False:
                    os.mkdir('tmp/tmp')
                    dst_ds = drv.CreateCopy('tmp/tmp/foo', ds)
                    dst_ds = None
                    try:
                        shutil.rmtree('tmp/tmp')
                    except:
                        reason = 'Cannot remove tmp/tmp after drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
                        gdaltest.post_reason(reason)
                        return False
        ds = None
        if nBands == 0:
            gdal.GetDriverByName('ILWIS').Delete('tmp/tmp.mpl')
        else:
            gdal.GetDriverByName('GTiff').Delete('tmp/tmp.tif')
    return True

def misc_6():

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    try:
        shutil.rmtree('tmp/tmp')
    except:
        pass

    datatype = gdal.GDT_Byte
    for nBands in range(6):
        if misc_6_internal(datatype, nBands) is not True:
            return 'fail'

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
        if misc_6_internal(datatype, nBands) is not True:
            return 'fail'

    gdal.PopErrorHandler()

    return 'success'

gdaltest_list = [ misc_1,
                  misc_2,
                  misc_3,
                  misc_4,
                  misc_5,
                  misc_6 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'misc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

