#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic support for ICC profile in TIFF file.
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

import base64
import os


from osgeo import gdal
import pytest


###############################################################################
# Test writing and reading of ICC profile in Create() options

def test_tiff_write_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)

    # Check with dataset from Create()
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.tiff.aux.xml')
    

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with dataset from Open()
    ds = gdal.Open('tmp/icc_test.tiff')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.tiff.aux.xml')
    

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with GetMetadataItem()
    ds = gdal.Open('tmp/icc_test.tiff')
    source_icc_profile = ds.GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE")
    ds = None

    with pytest.raises(OSError):
        os.stat('tmp/icc_test.tiff.aux.xml')
    

    assert source_icc_profile == icc

    driver.Delete('tmp/icc_test.tiff')

###############################################################################
# Test writing and reading of ICC profile in CreateCopy()


def test_tiff_copy_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds)

    # Check with dataset from CreateCopy()
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test2.tiff')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')

###############################################################################
# Test writing and reading of ICC profile in CreateCopy() options


def test_tiff_copy_options_icc():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    # Create dummy file
    options = ['SOURCE_ICC_PROFILE=' + icc]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds, options=options)

    # Check with dataset from CreateCopy()
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test2.tiff')
    md = ds2.GetMetadata("COLOR_PROFILE")
    ds = None
    ds2 = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')


def cvtTuple2String(t):
    return str(t).lstrip('([').rstrip(')]')

###############################################################################
# Test writing and reading of ICC colorimetric data from options


def test_tiff_copy_options_colorimetric_data():
    # sRGB values
    source_primaries = [(0.64, 0.33, 1.0), (0.3, 0.6, 1.0), (0.15, 0.06, 1.0)]
    source_whitepoint = (0.31271, 0.32902, 1.0)
    tifftag_transferfunction = (list(range(1, 256 * 4, 4)), list(range(2, 256 * 4 + 1, 4)), list(range(3, 256 * 4 + 2, 4)))

    options = ['SOURCE_PRIMARIES_RED=' + cvtTuple2String(source_primaries[0]),
               'SOURCE_PRIMARIES_GREEN=' + cvtTuple2String(source_primaries[1]),
               'SOURCE_PRIMARIES_BLUE=' + cvtTuple2String(source_primaries[2]),
               'SOURCE_WHITEPOINT=' + cvtTuple2String(source_whitepoint),
               'TIFFTAG_TRANSFERFUNCTION_RED=' + cvtTuple2String(tifftag_transferfunction[0]),
               'TIFFTAG_TRANSFERFUNCTION_GREEN=' + cvtTuple2String(tifftag_transferfunction[1]),
               'TIFFTAG_TRANSFERFUNCTION_BLUE=' + cvtTuple2String(tifftag_transferfunction[2])
              ]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds, options=options)
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

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']'))

    assert tifftag_transferfunction2 == tifftag_transferfunction

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test2.tiff')
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

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']'))

    assert tifftag_transferfunction2 == tifftag_transferfunction

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')

###############################################################################
# Test writing and reading of ICC colorimetric data in the file


def test_tiff_copy_colorimetric_data():
    # sRGB values
    source_primaries = [(0.64, 0.33, 1.0), (0.3, 0.6, 1.0), (0.15, 0.06, 1.0)]
    source_whitepoint = (0.31271, 0.32902, 1.0)
    tifftag_transferfunction = (list(range(1, 256 * 4, 4)), list(range(2, 256 * 4 + 1, 4)), list(range(3, 256 * 4 + 2, 4)))

    options = ['SOURCE_PRIMARIES_RED=' + cvtTuple2String(source_primaries[0]),
               'SOURCE_PRIMARIES_GREEN=' + cvtTuple2String(source_primaries[1]),
               'SOURCE_PRIMARIES_BLUE=' + cvtTuple2String(source_primaries[2]),
               'SOURCE_WHITEPOINT=' + cvtTuple2String(source_whitepoint),
               'TIFFTAG_TRANSFERFUNCTION_RED=' + cvtTuple2String(tifftag_transferfunction[0]),
               'TIFFTAG_TRANSFERFUNCTION_GREEN=' + cvtTuple2String(tifftag_transferfunction[1]),
               'TIFFTAG_TRANSFERFUNCTION_BLUE=' + cvtTuple2String(tifftag_transferfunction[2])
              ]

    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte, options)
    ds = None
    ds = gdal.Open('tmp/icc_test.tiff')

    # Check with dataset from CreateCopy()
    ds2 = driver.CreateCopy('tmp/icc_test2.tiff', ds)
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

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']'))

    assert tifftag_transferfunction2 == tifftag_transferfunction

    # Check again with dataset from Open()
    ds2 = gdal.Open('tmp/icc_test2.tiff')
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

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']'))

    assert tifftag_transferfunction2 == tifftag_transferfunction

    driver.Delete('tmp/icc_test.tiff')
    driver.Delete('tmp/icc_test2.tiff')

###############################################################################
# Test updating ICC profile


def test_tiff_update_icc():

    with open('data/sRGB.icc', 'rb') as f:
        icc = base64.b64encode(f.read()).decode('ascii')

    # Create dummy file
    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)
    ds = None

    ds = gdal.Open('tmp/icc_test.tiff', gdal.GA_Update)

    ds.SetMetadataItem('SOURCE_ICC_PROFILE', icc, 'COLOR_PROFILE')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    # Reopen the file to verify it was written.
    ds = gdal.Open('tmp/icc_test.tiff')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None

    assert md['SOURCE_ICC_PROFILE'] == icc

    driver.Delete('tmp/icc_test.tiff')

###############################################################################
# Test updating colorimetric options


def test_tiff_update_colorimetric():
    source_primaries = [(0.234, 0.555, 1.0), (0.2, 0, 1), (2, 3.5, 1)]
    source_whitepoint = (0.31271, 0.32902, 1.0)
    tifftag_transferfunction = (list(range(1, 256 * 4, 4)), list(range(2, 256 * 4 + 1, 4)), list(range(3, 256 * 4 + 2, 4)))

    # Create dummy file
    driver = gdal.GetDriverByName('GTiff')
    ds = driver.Create('tmp/icc_test.tiff', 64, 64, 3, gdal.GDT_Byte)
    ds = None

    ds = gdal.Open('tmp/icc_test.tiff', gdal.GA_Update)

    ds.SetMetadataItem('SOURCE_PRIMARIES_RED', cvtTuple2String(source_primaries[0]), 'COLOR_PROFILE')
    ds.SetMetadataItem('SOURCE_PRIMARIES_GREEN', cvtTuple2String(source_primaries[1]), 'COLOR_PROFILE')
    ds.SetMetadataItem('SOURCE_PRIMARIES_BLUE', cvtTuple2String(source_primaries[2]), 'COLOR_PROFILE')
    ds.SetMetadataItem('SOURCE_WHITEPOINT', cvtTuple2String(source_whitepoint), 'COLOR_PROFILE')
    ds.SetMetadataItem('TIFFTAG_TRANSFERFUNCTION_RED', cvtTuple2String(tifftag_transferfunction[0]), 'COLOR_PROFILE')
    ds.SetMetadataItem('TIFFTAG_TRANSFERFUNCTION_GREEN', cvtTuple2String(tifftag_transferfunction[1]), 'COLOR_PROFILE')
    ds.SetMetadataItem('TIFFTAG_TRANSFERFUNCTION_BLUE', cvtTuple2String(tifftag_transferfunction[2]), 'COLOR_PROFILE')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None

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

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']'))

    assert tifftag_transferfunction2 == tifftag_transferfunction

    # Reopen the file to verify it was written.
    ds = gdal.Open('tmp/icc_test.tiff')
    md = ds.GetMetadata("COLOR_PROFILE")
    ds = None

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

    tifftag_transferfunction2 = (
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_RED'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_GREEN'] + ']'),
        eval('[' + md['TIFFTAG_TRANSFERFUNCTION_BLUE'] + ']'))

    assert tifftag_transferfunction2 == tifftag_transferfunction

    driver.Delete('tmp/icc_test.tiff')

############################################################################



