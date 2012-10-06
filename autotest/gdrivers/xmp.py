#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
    def __init__( self, drivername, filename, expect_xmp ):
        self.drivername = drivername
        self.filename = filename
        self.expect_xmp = expect_xmp

    def test( self ):
        try:
            drv = gdal.GetDriverByName(self.drivername)
        except:
            drv = None

        if drv is None:
            return 'skip'

        if self.drivername == 'PDF':
            md = drv.GetMetadata()
            if not 'HAVE_POPPLER' in md and not 'HAVE_PODOFO' in md:
                return 'skip'

        if self.filename == 'data/byte_with_xmp.jp2':
            gdaltest.deregister_all_jpeg2000_drivers_but(self.drivername)

        ret = 'success'
        ds = gdal.Open(self.filename)
        if ds is None:
            # Old libwebp don't support VP8X containers
            if self.filename == 'data/rgbsmall_with_xmp.webp':
                ret = 'skip'
            else:
                gdaltest.post_reason('open failed')
                ret = 'failure'
        else:
            xmp_md = ds.GetMetadata('xml:XMP')
            if ds.GetDriver().ShortName != self.drivername:
                gdaltest.post_reason('opened with wrong driver')
                print(ds.GetDriver().ShortName)
                ret = 'failure'
            elif self.expect_xmp and len(xmp_md) == 0:
                gdaltest.post_reason('did not find xml:XMP metadata')
                ret = 'failure'
            elif (not self.expect_xmp) and xmp_md is not None and len(xmp_md) != 0:
                gdaltest.post_reason('found unexpected xml:XMP metadata')
                ret = 'failure'
        ds = None

        if self.filename == 'data/byte_with_xmp.jp2':
            gdaltest.reregister_all_jpeg2000_drivers()

        return ret


gdaltest_list = []

list = [ [ "GTiff", "data/byte_with_xmp.tif", True ],
         [ "GTiff", "data/byte.tif", False ],
         [ "GIF", "data/byte_with_xmp.gif", True ],
         [ "BIGGIF", "data/fakebig.gif", False ],
         [ "JPEG", "data/byte_with_xmp.jpg", True ],
         [ "JPEG", "data/rgbsmall_rgb.jpg", False ],
         [ "PNG", "data/byte_with_xmp.png", True ],
         [ "PNG", "data/test.png", False ],
         [ "JP2ECW", "data/byte_with_xmp.jp2", True ],
         [ "JP2ECW", "data/byte.jp2", False ],
         [ "JP2MrSID", "data/byte_with_xmp.jp2", True ],
         [ "JP2MrSID", "data/byte.jp2", False ],
         [ "JPEG2000", "data/byte_with_xmp.jp2", True ],
         [ "JPEG2000", "data/byte.jp2", False ],
         [ "JP2OpenJPEG", "data/byte_with_xmp.jp2", True ],
         [ "JP2OpenJPEG", "data/byte.jp2", False ],
         [ "JP2KAK", "data/byte_with_xmp.jp2", True ],
         [ "JP2KAK", "data/byte.jp2", False ],
         [ "PDF", "data/adobe_style_geospatial_with_xmp.pdf", True ],
         [ "PDF", "data/adobe_style_geospatial.pdf", False ],
         [ "WEBP", "data/rgbsmall_with_xmp.webp", True ],
         [ "WEBP", "data/rgbsmall.webp", False ],
]

for item in list:
    drivername = item[0]
    filename = item[1]
    expect_xmp = item[2]

    ut = TestXMPRead( drivername, filename, expect_xmp )
    gdaltest_list.append( (ut.test, "xmp_read_%s_%s" % (drivername, "true" if expect_xmp is True else "false")) )

if __name__ == '__main__':

    gdaltest.setup_run( 'xmp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
