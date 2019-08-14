#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic support for ICC profile in PNG file.
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

def test_png_copy_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('PNG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.png', ds)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.png.aux.xml')
    

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.png')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds2 = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.png.aux.xml')
    

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with GetMetadataItem()
    ds2 = gdal.Open('tmp/icc_test.png')
    source_icc_profile = ds2.GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE")
    ds2 = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.png.aux.xml')
    

    assert source_icc_profile == icc

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.png')


def cvtTuple2String(t):
    return str(t).lstrip('([').rstrip(')]')

###############################################################################
# Test writing and reading of ICC profile in CreateCopy() options


def test_png_copy_options_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('PNG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.png', ds, options=options)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.png')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.png')

###############################################################################
# Test writing and reading of ICC colorimetric data from options


def test_png_copy_options_colorimetric_data():
    # sRGB values
    source_primaries = [(0.64, 0.33, 1.0), (0.3, 0.6, 1.0), (0.15, 0.06, 1.0)]
    source_whitepoint = (0.31271, 0.32902, 1.0)

    options = ['SOURCE_PRIMARIES_RED=' + cvtTuple2String(source_primaries[0]),
               'SOURCE_PRIMARIES_GREEN=' + cvtTuple2String(source_primaries[1]),
               'SOURCE_PRIMARIES_BLUE=' + cvtTuple2String(source_primaries[2]),
               'SOURCE_WHITEPOINT=' + cvtTuple2String(source_whitepoint),
               'PNG_GAMMA=1.5']

    driver = gdal.GetDriverByName('PNG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.png', ds, options=options)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')

    for i in range(0, 3):
        assert source_whitepoint2[i] == pytest.approx(source_whitepoint[i], abs=0.0001)

    source_primaries2 = [
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')')]

    for j in range(0, 3):
        for i in range(0, 3):
            assert source_primaries2[j][i] == pytest.approx(source_primaries[j][i], abs=0.0001)

    assert float(md['PNG_GAMMA']) == 1.5

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.png')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')

    for i in range(0, 3):
        assert source_whitepoint2[i] == pytest.approx(source_whitepoint[i], abs=0.0001)

    source_primaries2 = [
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')')]

    for j in range(0, 3):
        for i in range(0, 3):
            assert source_primaries2[j][i] == pytest.approx(source_primaries[j][i], abs=0.0001)

    assert float(md['PNG_GAMMA']) == 1.5

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.png')

###############################################################################
# Test writing and reading of ICC colorimetric data in the file


def test_png_copy_colorimetric_data():
    # sRGB values
    source_primaries = [(0.64, 0.33, 1.0), (0.3, 0.6, 1.0), (0.15, 0.06, 1.0)]
    source_whitepoint = (0.31271, 0.32902, 1.0)

    options = ['SOURCE_PRIMARIES_RED=' + cvtTuple2String(source_primaries[0]),
               'SOURCE_PRIMARIES_GREEN=' + cvtTuple2String(source_primaries[1]),
               'SOURCE_PRIMARIES_BLUE=' + cvtTuple2String(source_primaries[2]),
               'SOURCE_WHITEPOINT=' + cvtTuple2String(source_whitepoint)]

    options2 = ['PNG_GAMMA=1.5']

    driver = gdal.GetDriverByName('PNG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)
    ds = None
    ds = gdal.Open('tmp/icc_test.tiff')

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.png', ds, options=options2)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')

    for i in range(0, 3):
        assert source_whitepoint2[i] == pytest.approx(source_whitepoint[i], abs=0.0001)

    source_primaries2 = [
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')')]

    for j in range(0, 3):
        for i in range(0, 3):
            assert source_primaries2[j][i] == pytest.approx(source_primaries[j][i], abs=0.0001)

    assert float(md['PNG_GAMMA']) == 1.5

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.png')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    source_whitepoint2 = eval('(' + md['SOURCE_WHITEPOINT'] + ')')

    for i in range(0, 3):
        assert source_whitepoint2[i] == pytest.approx(source_whitepoint[i], abs=0.0001)

    source_primaries2 = [
        eval('(' + md['SOURCE_PRIMARIES_RED'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_GREEN'] + ')'),
        eval('(' + md['SOURCE_PRIMARIES_BLUE'] + ')')]

    for j in range(0, 3):
        for i in range(0, 3):
            assert source_primaries2[j][i] == pytest.approx(source_primaries[j][i], abs=0.0001)

    assert float(md['PNG_GAMMA']) == 1.5

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.png')

###############################################################################
# Test sRGB


def test_png_sRGB():
    # Create dummy file
    options = ['SOURCE_ICC_PROFILE_NAME=sRGB']

    driver = gdal.GetDriverByName('PNG')
    driver_tiff = gdal.GetDriverByName('GTiff')
    ds = driver_tiff.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test.png', ds, options=options)
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE_NAME'] == 'sRGB'

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test.png')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE_NAME'] == 'sRGB'

    driver_tiff.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test.png')

############################################################################



