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
import time

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Generic test

def vsifile_generic(filename):

    start_time = time.time()

    fp = gdal.VSIFOpenL(filename, 'wb+')
    if fp is None:
        gdaltest.post_reason('failure')
        return 'fail'

    if gdal.VSIFWriteL('0123456789', 1, 10, fp) != 10:
        gdaltest.post_reason('failure')
        return 'fail'

    if gdal.VSIFTruncateL(fp, 20) != 0:
        gdaltest.post_reason('failure')
        return 'fail'

    if gdal.VSIFTellL(fp) != 10:
        gdaltest.post_reason('failure')
        return 'fail'

    if gdal.VSIFTruncateL(fp, 5) != 0:
        gdaltest.post_reason('failure')
        return 'fail'

    if gdal.VSIFTellL(fp) != 10:
        gdaltest.post_reason('failure')
        return 'fail'

    if gdal.VSIFSeekL(fp, 0, 2) != 0:
        gdaltest.post_reason('failure')
        return 'fail'

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
    if abs(start_time - statBuf.mtime) > 2:
        gdaltest.post_reason('failure')
        print(statBuf.mtime)
        return 'fail'

    fp = gdal.VSIFOpenL(filename, 'rb')
    buf = gdal.VSIFReadL(1, 7, fp)
    if gdal.VSIFWriteL('a', 1, 1, fp) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFTruncateL(fp, 0) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(fp)

    if buf.decode('ascii') != '01234XX':
        gdaltest.post_reason('failure')
        print(buf.decode('ascii'))
        return 'fail'

    # Test append mode on existing file
    fp = gdal.VSIFOpenL(filename, 'ab')
    gdal.VSIFWriteL('XX', 1, 2, fp)
    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(filename, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    if statBuf.size != 9:
        gdaltest.post_reason('failure')
        print(statBuf.size)
        return 'fail'

    if gdal.Unlink(filename) != 0:
        gdaltest.post_reason('failure')
        return 'fail'

    statBuf = gdal.VSIStatL(filename, gdal.VSI_STAT_EXISTS_FLAG)
    if statBuf is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test append mode on non existing file
    fp = gdal.VSIFOpenL(filename, 'ab')
    gdal.VSIFWriteL('XX', 1, 2, fp)
    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(filename, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    if statBuf.size != 2:
        gdaltest.post_reason('failure')
        print(statBuf.size)
        return 'fail'

    if gdal.Unlink(filename) != 0:
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
    gdal.VSIFCloseL(fp)

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

    ref_data = 'abcd'.encode('ascii')
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
# Test limit cases on /vsimem

def vsifile_7():

    if gdal.GetConfigOption('SKIP_MEM_INTENSIVE_TEST') is not None:
        return 'skip'

    # Test extending file beyond reasonable limits in write mode
    fp = gdal.VSIFOpenL('/vsimem/vsifile_7.bin', 'wb')
    if gdal.VSIFSeekL(fp, 0x7FFFFFFFFFFFFFFF, 0) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIStatL('/vsimem/vsifile_7.bin').size != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = gdal.VSIFWriteL('a', 1, 1, fp)
    gdal.PopErrorHandler()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIStatL('/vsimem/vsifile_7.bin').size != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(fp)

    # Test seeking  beyond file size in read-only mode
    fp = gdal.VSIFOpenL('/vsimem/vsifile_7.bin', 'rb')
    if gdal.VSIFSeekL(fp, 0x7FFFFFFFFFFFFFFF, 0) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFTellL(fp) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(fp)
    gdal.Unlink('tmp/vsifile_7.bin')

    return 'success'

###############################################################################
# Test renaming directory in /vsimem

def vsifile_8():

    # octal 0666 = decimal 438
    gdal.Mkdir('/vsimem/mydir', 438)
    fp = gdal.VSIFOpenL('/vsimem/mydir/a', 'wb')
    gdal.VSIFCloseL(fp)
    gdal.Rename('/vsimem/mydir', '/vsimem/newdir'.encode('ascii').decode('ascii'))
    if gdal.VSIStatL('/vsimem/newdir') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIStatL('/vsimem/newdir/a') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/newdir/a')
    gdal.Rmdir('/vsimem/newdir')

    return 'success'

###############################################################################
# Test ReadDir()

def vsifile_9():

    lst = gdal.ReadDir('.')
    if len(lst) < 4:
        gdaltest.post_reason('fail')
        return 'fail'
    # Test truncation
    lst_truncated = gdal.ReadDir('.', int(len(lst)/2))
    if len(lst_truncated) <= int(len(lst)/2):
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Mkdir('/vsimem/mydir', 438)
    for i in range(10):
        fp = gdal.VSIFOpenL('/vsimem/mydir/%d' % i, 'wb')
        gdal.VSIFCloseL(fp)

    lst = gdal.ReadDir('/vsimem/mydir')
    if len(lst) < 4:
        gdaltest.post_reason('fail')
        return 'fail'
    # Test truncation
    lst_truncated = gdal.ReadDir('/vsimem/mydir', int(len(lst)/2))
    if len(lst_truncated) <= int(len(lst)/2):
        gdaltest.post_reason('fail')
        return 'fail'

    for i in range(10):
        gdal.Unlink('/vsimem/mydir/%d' % i)
    gdal.Rmdir('/vsimem/newdir')

    return 'success'

###############################################################################
# Test fuzzer friendly archive

def vsifile_10():

    gdal.FileFromMemBuffer('/vsimem/vsifile_10.tar',
"""FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:test.txt
abc***NEWFILE***:huge.txt
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
0123456789012345678901234567890123456789012345678901234567890123456789012345678X
***NEWFILE***:small.txt
a""")
    contents = gdal.ReadDir('/vsitar//vsimem/vsifile_10.tar')
    if contents is None:
        gdal.Unlink('/vsimem/vsifile_10.tar')
        return 'skip'
    if contents != ['test.txt', 'huge.txt', 'small.txt']:
        gdaltest.post_reason('fail')
        print(contents)
        return 'fail'
    if gdal.VSIStatL('/vsitar//vsimem/vsifile_10.tar/test.txt').size != 3:
        gdaltest.post_reason('fail')
        print(gdal.VSIStatL('/vsitar//vsimem/vsifile_10.tar/test.txt').size)
        return 'fail'
    if gdal.VSIStatL('/vsitar//vsimem/vsifile_10.tar/huge.txt').size != 3888:
        gdaltest.post_reason('fail')
        print(gdal.VSIStatL('/vsitar//vsimem/vsifile_10.tar/huge.txt').size)
        return 'fail'
    if gdal.VSIStatL('/vsitar//vsimem/vsifile_10.tar/small.txt').size != 1:
        gdaltest.post_reason('fail')
        print(gdal.VSIStatL('/vsitar//vsimem/vsifile_10.tar/small.txt').size)
        return 'fail'


    gdal.FileFromMemBuffer('/vsimem/vsifile_10.tar',
"""FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:x
abc""")
    contents = gdal.ReadDir('/vsitar//vsimem/vsifile_10.tar')
    if contents != ['x']:
        gdaltest.post_reason('fail')
        print(contents)
        return 'fail'


    gdal.FileFromMemBuffer('/vsimem/vsifile_10.tar',
"""FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:x
abc***NEWFILE***:""")
    contents = gdal.ReadDir('/vsitar//vsimem/vsifile_10.tar')
    if contents != ['x']:
        gdaltest.post_reason('fail')
        print(contents)
        return 'fail'

    gdal.Unlink('/vsimem/vsifile_10.tar')

    return 'success'

###############################################################################
# Test generic Truncate implementation for file extension

def vsifile_11():
    f = gdal.VSIFOpenL('/vsimem/vsifile_11', 'wb')
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL('/vsisubfile/0_,/vsimem/vsifile_11', 'wb')
    gdal.VSIFWriteL('0123456789', 1, 10, f)
    if gdal.VSIFTruncateL(f, 10+4096+2) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFTellL(f) != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFTruncateL(f, 0) != -1:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL('/vsimem/vsifile_11', 'rb')
    data = gdal.VSIFReadL(1, 10+4096+2, f)
    gdal.VSIFCloseL(f)
    import struct
    data = struct.unpack('B' * len(data), data)
    if data[0] != 48 or data[9] != 57 or data[10] != 0 or data[10+4096+2-1] != 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.Unlink('/vsimem/vsifile_11')

    return 'success'

###############################################################################
# Test regular file system sparse file support

def vsifile_12():

    target_dir = 'tmp'

    if gdal.VSISupportsSparseFiles(target_dir) == 0:
        return 'skip'

    # Minimum value to make it work on NTFS
    block_size = 65536
    f = gdal.VSIFOpenL(target_dir+'/vsifile_12', 'wb')
    gdal.VSIFWriteL('a', 1, 1, f)
    if gdal.VSIFTruncateL(f, block_size*2) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFGetRangeStatusL(f, 0, 1)
    # We could get unknown on nfs
    if ret == gdal.VSI_RANGE_STATUS_UNKNOWN:
        print('Range status unknown')
    else:
        if ret != gdal.VSI_RANGE_STATUS_DATA:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'
        ret = gdal.VSIFGetRangeStatusL(f, block_size*2-1,1)
        if ret != gdal.VSI_RANGE_STATUS_HOLE:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'
    gdal.VSIFCloseL(f)

    gdal.Unlink(target_dir+'/vsifile_12')

    return 'success'

###############################################################################
# Test reading filename with prefixes without terminating slash

def vsifile_13():

    gdal.VSIFOpenL('/vsigzip', 'rb')
    gdal.VSIFOpenL('/vsizip', 'rb')
    gdal.VSIFOpenL('/vsitar', 'rb')
    gdal.VSIFOpenL('/vsimem', 'rb')
    gdal.VSIFOpenL('/vsisparse', 'rb')
    gdal.VSIFOpenL('/vsisubfile', 'rb')
    gdal.VSIFOpenL('/vsicurl', 'rb')
    gdal.VSIFOpenL('/vsis3', 'rb')
    gdal.VSIFOpenL('/vsicurl_streaming', 'rb')
    gdal.VSIFOpenL('/vsis3_streaming', 'rb')
    gdal.VSIFOpenL('/vsistdin', 'rb')

    fp = gdal.VSIFOpenL('/vsistdout', 'wb')
    if fp is not None:
        gdal.VSIFCloseL(fp)

    gdal.VSIStatL('/vsigzip')
    gdal.VSIStatL('/vsizip')
    gdal.VSIStatL('/vsitar')
    gdal.VSIStatL('/vsimem')
    gdal.VSIStatL('/vsisparse')
    gdal.VSIStatL('/vsisubfile')
    gdal.VSIStatL('/vsicurl')
    gdal.VSIStatL('/vsis3')
    gdal.VSIStatL('/vsicurl_streaming')
    gdal.VSIStatL('/vsis3_streaming')
    gdal.VSIStatL('/vsistdin')
    gdal.VSIStatL('/vsistdout')

    return 'success'

###############################################################################
# Check performance issue (https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1673)

def vsifile_14():

    with gdaltest.error_handler():
        gdal.VSIFOpenL('/vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar/a.tgzb.tgzc.tgzd.tgze.tgzf.tgz.h.tgz.i.tgz', 'rb')
    return 'success'

###############################################################################
# Test issue with Eof() not detecting end of corrupted gzip stream (#6944)

def vsifile_15():

    fp = gdal.VSIFOpenL('/vsigzip/data/corrupted_z_buf_error.gz', 'rb')
    if fp is None:
        return 'fail'
    while not gdal.VSIFEofL(fp):
        with gdaltest.error_handler():
            gdal.VSIFReadL(1,4,fp)
    gdal.VSIFCloseL(fp)

    return 'success'

gdaltest_list = [ vsifile_1,
                  vsifile_2,
                  vsifile_3,
                  vsifile_4,
                  vsifile_5,
                  vsifile_6,
                  vsifile_7,
                  vsifile_8,
                  vsifile_9,
                  vsifile_10,
                  vsifile_11,
                  vsifile_12,
                  vsifile_13,
                  vsifile_14,
                  vsifile_15 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsifile' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
