#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 30 (UTF filename handling) support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2010 Frank Warmerdam
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
# Try opening a file with a chinese name using the Python utf-8 string.

def rfc30_1():

    filename =  u'data/xx\u4E2D\u6587.\u4E2D\u6587'

    filename = filename.encode( 'utf-8' )
    
    ds = gdal.Open( filename )

    if ds is not None:
        return 'success'

    else:
        gdaltest.post_reason( 'failed to open utf filename.' )
        return 'failure'

###############################################################################
# Try creating, then renaming a utf-8 named file.

def rfc30_2():

    filename =  u'tmp/yy\u4E2D\u6587.\u4E2D\u6587'

    filename = filename.encode( 'utf-8' )

    fd = gdal.VSIFOpenL( filename, 'w' )
    if fd is None:
        gdaltest.post_reason( 'failed to create utf-8 named file.' )
        return 'failure'

    gdal.VSIFWriteL( 'abc', 3, 1, fd )
    gdal.VSIFCloseL( fd )

    # rename

    new_filename = u'tmp/yy\u4E2D\u6587.\u4E2D\u6587'
    new_filename = new_filename.encode( 'utf-8' )

    if gdal.Rename( filename, new_filename ) != 0:
        gdaltest.post_reason( 'utf-8 rename failed.' )
        return 'failure'

    fd = gdal.VSIFOpenL( new_filename, 'r' )
    if fd is None:
        gdaltest.post_reason( 'reopen failed with utf8' )
        return 'failure'
    
    data = gdal.VSIFReadL( 3, 1, fd )
    gdal.VSIFCloseL( fd )

    if data != 'abc':
        gdaltest.post_reason( 'did not get expected data.' )
        return 'failure'

    gdal.Unlink( new_filename )

    fd = gdal.VSIFOpenL( new_filename, 'r' )
    if fd is not None:
        gdaltest.post_reason( 'did unlink fail on utf8 filename?' )
        return 'failure'
    
    return 'success'


gdaltest_list = [ rfc30_1,
                  rfc30_2
                  ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rfc30' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

