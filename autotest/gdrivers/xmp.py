#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test XMP metadata reading.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

import pytest

from osgeo import gdal


import gdaltest

###############################################################################
#


lst = [
    ["GTiff", "data/gtiff/byte_with_xmp.tif", True],
    ["GTiff", "data/byte.tif", False],
    ["GIF", "data/gif/byte_with_xmp.gif", True],
    ["BIGGIF", "data/gif/fakebig.gif", False],
    ["JPEG", "data/jpeg/byte_with_xmp.jpg", True],
    ["JPEG", "data/jpeg/rgbsmall_rgb.jpg", False],
    ["PNG", "data/png/byte_with_xmp.png", True],
    ["PNG", "data/png/test.png", False],
    ["JP2ECW", "data/jpeg2000/byte_with_xmp.jp2", True],
    ["JP2ECW", "data/jpeg2000/byte.jp2", False],
    ["JP2MrSID", "data/jpeg2000/byte_with_xmp.jp2", True],
    ["JP2MrSID", "data/jpeg2000/byte.jp2", False],
    ["JPEG2000", "data/jpeg2000/byte_with_xmp.jp2", True],
    ["JPEG2000", "data/jpeg2000/byte.jp2", False],
    ["JP2OpenJPEG", "data/jpeg2000/byte_with_xmp.jp2", True],
    ["JP2OpenJPEG", "data/jpeg2000/byte.jp2", False],
    ["JP2KAK", "data/jpeg2000/byte_with_xmp.jp2", True],
    ["JP2KAK", "data/jpeg2000/byte.jp2", False],
    ["PDF", "data/pdf/adobe_style_geospatial_with_xmp.pdf", True],
    ["PDF", "data/pdf/adobe_style_geospatial.pdf", False],
    ["WEBP", "data/webp/rgbsmall_with_xmp.webp", True],
    ["WEBP", "data/webp/rgbsmall.webp", False],
]


@pytest.mark.parametrize(
    'drivername,filename,expect_xmp',
    lst,
    ids=[
        "xmp_read_%s_%s" % (drivername, str(expect_xmp))
        for (drivername, filename, expect_xmp) in lst
    ]
)
def test_xmp(drivername, filename, expect_xmp):
    drv = gdal.GetDriverByName(drivername)
    if drv is None:
        pytest.skip()

    if drivername == 'PDF':
        md = drv.GetMetadata()
        if 'HAVE_POPPLER' not in md and 'HAVE_PODOFO' not in md:
            pytest.skip()

    # we set ECW to not resolve projection and datum strings to get 3.x behavior.
    gdal.SetConfigOption("ECW_DO_NOT_RESOLVE_DATUM_PROJECTION", "YES")

    if '.jp2' in filename:
        gdaltest.deregister_all_jpeg2000_drivers_but(drivername)

    try:
        ds = gdal.Open(filename)
        if filename == 'data/rgbsmall_with_xmp.webp':
            if ds is None:
                pytest.skip("Old libwebp don't support VP8X containers")
        else:
            assert ds is not None, 'open failed'

        xmp_md = ds.GetMetadata('xml:XMP')

        assert ds.GetDriver().ShortName == drivername, 'opened with wrong driver'
        assert not (expect_xmp and not xmp_md), 'did not find xml:XMP metadata'
        assert not (expect_xmp and 'xml:XMP' not in ds.GetMetadataDomainList()), 'did not find xml:XMP metadata domain'
        assert expect_xmp or not xmp_md, 'found unexpected xml:XMP metadata'

        ds = None
    finally:
        if '.jp2' in filename:
            gdaltest.reregister_all_jpeg2000_drivers()
