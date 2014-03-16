#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VSI file primitives
# Author:   Even Rouault <even dot rouault at mines dash parid dot org>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Generic test

def vsifile_generic(filename):

    fp = gdal.VSIFOpenL(filename, 'wb+')
    gdal.VSIFWriteL('0123456789', 1, 10, fp)
    gdal.VSIFTruncateL(fp, 5)

    if gdal.VSIFTellL(fp) != 10:
        gdaltest.post_reason('failure')
        return 'fail'

    gdal.VSIFSeekL(fp, 0, 2)

    if gdal.VSIFTellL(fp) != 5:
        gdaltest.post_reason('failure')
        return 'fail'

    gdal.VSIFWriteL('XX', 1, 2, fp)
    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(filename, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    if statBuf.size != 7:
        gdaltest.post_reason('failure')
        print(statBuf.size)
        return 'fail'

    fp = gdal.VSIFOpenL(filename, 'rb')
    buf = gdal.VSIFReadL(1, 7, fp)
    gdal.VSIFCloseL(fp)

    if buf.decode('ascii') != '01234XX':
        gdaltest.post_reason('failure')
        return 'fail'

    gdal.Unlink(filename)

    statBuf = gdal.VSIStatL(filename, gdal.VSI_STAT_EXISTS_FLAG)
    if statBuf is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test /vsimem

def vsifile_1():
    return vsifile_generic('/vsimem/vsifile_1.bin')

###############################################################################
# Test regular file system

def vsifile_2():

    ret = vsifile_generic('tmp/vsifile_2.bin')
    if ret != 'success' and gdaltest.skip_on_travis():
        # FIXME
        # Fails on Travis with 17592186044423 (which is 0x10 00 00 00 00 07 instead of 7) at line 63
        # Looks like a 32/64bit issue with Python bindings of VSIStatL()
        return 'skip'
    return ret

###############################################################################
# Test ftruncate >= 32 bit

def vsifile_3():

    if (gdaltest.filesystem_supports_sparse_files('tmp') == False):
        return 'skip'

    filename = 'tmp/vsifile_3'

    fp = gdal.VSIFOpenL(filename, 'wb+')
    gdal.VSIFTruncateL(fp, 10 * 1024 * 1024 * 1024)
    gdal.VSIFSeekL(fp, 0, 2)
    pos = gdal.VSIFTellL(fp)
    if pos != 10 * 1024 * 1024 * 1024:
        gdaltest.post_reason('failure')
        gdal.VSIFCloseL(fp)
        gdal.Unlink(filename)
        print(pos)
        return 'fail'
    gdal.VSIFSeekL(fp, 0, 0)
    gdal.VSIFSeekL(fp, pos, 0)
    pos = gdal.VSIFTellL(fp)
    if pos != 10 * 1024 * 1024 * 1024:
        gdaltest.post_reason('failure')
        gdal.VSIFCloseL(fp)
        gdal.Unlink(filename)
        print(pos)
        return 'fail'

    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(filename, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    gdal.Unlink(filename)

    if statBuf.size != 10 * 1024 * 1024 * 1024:
        gdaltest.post_reason('failure')
        print(statBuf.size)
        return 'fail'

    return 'success'

###############################################################################
# Test fix for #4583 (short reads)

def vsifile_4():

    fp = gdal.VSIFOpenL('vsifile.py', 'rb')
    data = gdal.VSIFReadL(1000000, 1, fp)
    #print(len(data))
    gdal.VSIFSeekL(fp, 0, 0)
    data = gdal.VSIFReadL(1, 1000000, fp)
    if len(data) == 0:
        return 'fail'

    return 'success'

###############################################################################
# Test vsicache

def vsifile_5():

    fp = gdal.VSIFOpenL('tmp/vsifile_5.bin', 'wb')
    ref_data = ''.join(['%08X' % i for i in range(5*32768)])
    gdal.VSIFWriteL(ref_data, 1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    gdal.SetConfigOption('VSI_CACHE', 'YES')

    for i in range(3):
        if i == 0:
            gdal.SetConfigOption('VSI_CACHE_SIZE', '0')
        elif i == 1:
            gdal.SetConfigOption('VSI_CACHE_SIZE', '65536')
        else:
            gdal.SetConfigOption('VSI_CACHE_SIZE', None)

        fp = gdal.VSIFOpenL('tmp/vsifile_5.bin', 'rb')

        gdal.VSIFSeekL(fp, 50000, 0)
        if gdal.VSIFTellL(fp) != 50000:
            gdaltest.post_reason('fail')
            gdal.SetConfigOption('VSI_CACHE_SIZE', None)
            gdal.SetConfigOption('VSI_CACHE', None)
            return 'fail'

        gdal.VSIFSeekL(fp, 50000, 1)
        if gdal.VSIFTellL(fp) != 100000:
            gdaltest.post_reason('fail')
            gdal.SetConfigOption('VSI_CACHE_SIZE', None)
            gdal.SetConfigOption('VSI_CACHE', None)
            return 'fail'

        gdal.VSIFSeekL(fp, 0, 2)
        if gdal.VSIFTellL(fp) != 5*32768*8:
            gdaltest.post_reason('fail')
            gdal.SetConfigOption('VSI_CACHE_SIZE', None)
            gdal.SetConfigOption('VSI_CACHE', None)
            return 'fail'
        gdal.VSIFReadL(1, 1, fp)

        gdal.VSIFSeekL(fp, 0, 0)
        data = gdal.VSIFReadL(1,3*32768,fp)
        if data.decode('ascii') != ref_data[0:3*32768]:
            gdaltest.post_reason('fail')
            gdal.SetConfigOption('VSI_CACHE_SIZE', None)
            gdal.SetConfigOption('VSI_CACHE', None)
            return 'fail'

        gdal.VSIFSeekL(fp, 16384, 0)
        data = gdal.VSIFReadL(1,5*32768,fp)
        if data.decode('ascii') != ref_data[16384:16384+5*32768]:
            gdaltest.post_reason('fail')
            gdal.SetConfigOption('VSI_CACHE_SIZE', None)
            gdal.SetConfigOption('VSI_CACHE', None)
            return 'fail'

        data = gdal.VSIFReadL(1,50*32768,fp)
        if data[0:1130496].decode('ascii') != ref_data[16384+5*32768:]:
            gdaltest.post_reason('fail')
            gdal.SetConfigOption('VSI_CACHE_SIZE', None)
            gdal.SetConfigOption('VSI_CACHE', None)
            return 'fail'

        gdal.VSIFCloseL(fp)

    gdal.SetConfigOption('VSI_CACHE_SIZE', None)
    gdal.SetConfigOption('VSI_CACHE', None)
    gdal.Unlink('tmp/vsifile_5.bin')

    return 'success'

###############################################################################
# Test vsicache above 2 GB

def vsifile_6():

    if (gdaltest.filesystem_supports_sparse_files('tmp') == False):
        return 'skip'

    offset = 4 * 1024 * 1024 * 1024

    ref_data = 'abcd'
    fp = gdal.VSIFOpenL('tmp/vsifile_6.bin', 'wb')
    gdal.VSIFSeekL(fp, offset, 0)
    gdal.VSIFWriteL(ref_data, 1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    # Sanity check without VSI_CACHE
    fp = gdal.VSIFOpenL('tmp/vsifile_6.bin', 'rb')
    gdal.VSIFSeekL(fp, offset, 0)
    got_data = gdal.VSIFReadL(1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    if ref_data != got_data:
        print(got_data)
        return 'fail'

    # Real test now
    gdal.SetConfigOption('VSI_CACHE', 'YES')
    fp = gdal.VSIFOpenL('tmp/vsifile_6.bin', 'rb')
    gdal.SetConfigOption('VSI_CACHE', None)
    gdal.VSIFSeekL(fp, offset, 0)
    got_data = gdal.VSIFReadL(1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    if ref_data != got_data:
        print(got_data)
        return 'fail'

    gdal.Unlink('tmp/vsifile_6.bin')

    return 'success'

###############################################################################
# Test vsicache above 2 GB

gdaltest_list = [ vsifile_1,
                  vsifile_2,
                  vsifile_3,
                  vsifile_4,
                  vsifile_5,
                  vsifile_6 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsifile' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
