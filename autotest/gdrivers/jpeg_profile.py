#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic support for ICC profile in JPEG file.
#
###############################################################################
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

###############################################################################
# This unit test uses a free ICC profile by Marti Maria (littleCMS)
# <http://www.littlecms.com>
# sRGB.icc uses the zlib license.
# Part of a free package of ICC profile found at:
# http://sourceforge.net/projects/openicc/files/OpenICC-Profiles/

import os
import base64


from osgeo import gdal
import pytest


###############################################################################
# Test writing and reading of ICC profile in CreateCopy()

def test_jpeg_copy_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('JPEG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.jpg', ds)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.jpg')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.jpg')

###############################################################################
# Test writing and reading of ICC profile in CreateCopy() options


def test_jpeg_copy_options_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('JPEG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.jpg', ds, options=options)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.jpg')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.jpg')

###############################################################################
# Test writing and reading of 64K+ ICC profile in CreateCopy()


def test_jpeg_copy_icc_64K():

    # In JPEG, APP2 chunks can only be 64K, so they would be split up.
    # It will still work, but need to test that the segmented ICC profile
    # is put back together correctly.
    # We will simply use the same profile multiple times.
    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    while len(data) < 200000:
        data += data
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('JPEG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.jpg', ds, options=['COMMENT=foo'])
    ds = None
    md = ds2.GetMetadata("COLOR_PROFILE")
    comment = ds2.GetMetadataItem('COMMENT')
    ds2 = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.jpg.aux.xml')
    

    assert comment == 'foo'

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.jpg')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds2 = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.jpg.aux.xml')
    

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with GetMetadataItem()
    ds2 = gdal.Open('tmp/icc_test.jpg')
    source_icc_profile = ds2.GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE")
    ds2 = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.jpg.aux.xml')
    

    assert source_icc_profile == icc

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.jpg')

###############################################################################################



