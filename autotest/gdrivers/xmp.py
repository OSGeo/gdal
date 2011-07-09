#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test XMP metadata reading.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
#
class TestXMPRead:
    def __init__( self, drivername, filename ):
        self.drivername = drivername
        self.filename = filename

    def test( self ):
        try:
            drv = gdal.GetDriverByName(self.drivername)
        except:
            drv = None

        if drv is None:
            return 'skip'

        if self.filename == 'data/byte_with_xmp.jp2':
            gdaltest.deregister_all_jpeg2000_drivers_but(self.drivername)

        ret = 'success'
        ds = gdal.Open(self.filename)
        if ds is None:
            gdaltest.post_reason('open failed')
            ret = 'failure'
        else:
            if ds.GetDriver().ShortName != self.drivername:
                gdaltest.post_reason('opened with wrong driver')
                print(ds.GetDriver().ShortName)
                ret = 'failure'
            elif len(ds.GetMetadata('xml:XMP')) == 0:
                gdaltest.post_reason('did not find xml:XMP metadata')
                ret = 'failure'
        ds = None

        if self.filename == 'data/byte_with_xmp.jp2':
            gdaltest.reregister_all_jpeg2000_drivers()

        return ret


gdaltest_list = []

list = [ [ "GTiff", "data/byte_with_xmp.tif" ],
         [ "GIF", "data/byte_with_xmp.gif" ],
         [ "JPEG", "data/byte_with_xmp.jpg" ],
         [ "PNG", "data/byte_with_xmp.png" ],
         [ "JP2ECW", "data/byte_with_xmp.jp2" ],
         [ "JP2MrSID", "data/byte_with_xmp.jp2" ],
         [ "JPEG2000", "data/byte_with_xmp.jp2" ],
         [ "JP2OpenJPEG", "data/byte_with_xmp.jp2" ],
         [ "JP2KAK", "data/byte_with_xmp.jp2" ],
         [ "PDF", "data/adobe_style_geospatial_with_xmp.pdf" ],
]

for item in list:
    drivername = item[0]
    filename = item[1]

    ut = TestXMPRead( drivername, filename )
    gdaltest_list.append( (ut.test, "xmp_read_%s" % drivername) )

if __name__ == '__main__':

    gdaltest.setup_run( 'xmp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
