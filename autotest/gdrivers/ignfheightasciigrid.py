#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  IGNFHeightASCIIGrid Testing.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault<even dot rouault at spatialys dot com>
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

import sys

sys.path.append('../pymod')

from osgeo import gdal
import gdaltest


def test_ignfheightasciigrid_1():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid/ignfheightasciigrid_ar1.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def test_ignfheightasciigrid_2():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid/ignfheightasciigrid_ar2.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def test_ignfheightasciigrid_3():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid/ignfheightasciigrid_ar3.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def test_ignfheightasciigrid_4():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid/ignfheightasciigrid_ar4.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def test_ignfheightasciigrid_5():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid/ignfheightasciigrid_ar1_nocoords.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def test_ignfheightasciigrid_6():

    tst = gdaltest.GDALTest(
        'IGNFHeightASCIIGrid', 'ignfheightasciigrid/ignfheightasciigrid_ar1_nocoords_noprec.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def test_ignfheightasciigrid_7():

    tst = gdaltest.GDALTest(
        'IGNFHeightASCIIGrid', 'ignfheightasciigrid/ignfheightasciigrid_ar1_noprec.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def test_ignfheightasciigrid_description_multiword_and_lf():

    filename = '/vsimem/ignfheightasciigrid_invalid'
    ok_content = b'2 3 49 50 1 1 1 0 1 0 -0. MULTI WORD\xC3\xA9\xC3\xA8\n1 2 3 4'
    gdal.FileFromMemBuffer(filename, ok_content)
    ds = gdal.OpenEx(filename)
    desc = ds.GetMetadataItem('DESCRIPTION')
    assert desc == 'MULTI WORDee'


def test_ignfheightasciigrid_invalid():

    filename = '/vsimem/ignfheightasciigrid_invalid'
    ok_content = '2 3 49 50 1 1 1 0 1 0 -0. DESC\r1 2 3 4'
    gdal.FileFromMemBuffer(filename, ok_content)
    ds = gdal.OpenEx(filename)
    assert ds
    assert ds.GetRasterBand(1).GetUnitType() == 'm'
    gdal.Unlink(filename)

    contents = ['0 0 0 0 0 0 0 0 0 0 0 0\r',  # a lot of invalid values
                '                   \r',  # all spaces
                '2 3 49 50 1 1 1 0 1 0 0 DESC',  # no newline
                '2 3 49 50 1 1 1 0 1 0 0 \r',  # missing  description in header
                '2 3 49 50 1 1 1 0 1 0 0\r1 2 3 4',  # missing description in header
                # not a number in numeric header section
                '2 3 49 50 1 1 1 a 1 0 0 DESC\r1 2 3 4',
                '2 3 49 50 1 1 1 0 1 0 0 DESC\ra 2 3 4',  # not a number in value section
                '2 3 49 50 1 1 1 0 1 0 0 DES\xC3\xA7C\r1 2 3 4',  # invalid character in comment
                '-200 3 49 50 1 1 1 0 1 0 0 DESC\r1 2 3 4',  # invalid longmin
                '2 300 49 50 1 1 1 0 1 0 0 DESC\r1 2 3 4',  # invalid longmax
                '2 3 -149 50 1 1 1 0 1 0 0 DESC\r1 2 3 4',  # invalid latmin
                '2 3 49 150 1 1 1 0 1 0 0 DESC\r1 2 3 4',  # invalid latmax
                '3 2 49 50 1 1 1 0 1 0 0 DESC\r1 2 3 4',  # longmin > longmax
                '2 3 50 49 1 1 1 0 1 0 0 DESC\r1 2 3 4',  # latmin > lamax
                '2 3 49 50 0 1 1 0 1 0 0 DESC\r1 2 3 4',  # invalid steplong
                '2 3 49 50 1 0 1 0 1 0 0 DESC\r1 2 3 4',  # invalid steplat
                '2 3 49 50 .000001 1 1 0 1 0 0 DESC\r1 2 3 4',  # too many samples in x
                '2 3 49 50 1 .000001 1 0 1 0 0 DESC\r1 2 3 4',  # too many samples in y
                '2 3 49 50 .0002 .0002 1 0 1 0 0 DESC\r1 2 3 4',  # too many samples in x and y
                '2 3 49 50 1 1 0 0 1 0 0 DESC\r1 2 3 4',  # wrong arrangement
                '2 3 49 50 1 1 1 2 1 0 0 DESC\r1 2 3 4',  # wrong coordinates at node
                '2 3 49 50 1 1 1 0 2 0 0 DESC\r1 2 3 4',  # wrong values per node
                '2 3 49 50 1 1 1 0 1 2 0 DESC\r1 2 3 4',  # wrong precision code
                '2 3 49 50 1 1 1 0 1 0 2 DESC\r1 2 3 4',  # wrong translation
                '2 3 49 50 1 1 1 0 1 0 0 DESC\r1 2 3',  # Missing value
                '2 3 49 50 1 1 1 0 1 0 0 DESC\r1 2 3 4 5',  # Too many values
                ]
    for content in contents:
        gdal.FileFromMemBuffer(filename, content)
        with gdaltest.error_handler():
            assert not gdal.OpenEx(filename, gdal.OF_RASTER), content
        gdal.Unlink(filename)
    

def test_ignfheightasciigrid_huge():

    filename = '/vsimem/ignfheightasciigrid_huge'
    ok_content = '2 3 49 50 1 1 1 0 1 0 -0. MULTI WORD\r1 2 3 4'
    gdal.FileFromMemBuffer(filename, ok_content)
    f = gdal.VSIFOpenL(filename, 'rb+')
    gdal.VSIFSeekL(f, 0, 2)
    padding = ' ' * (10 * 1024 * 1024)
    gdal.VSIFWriteL(padding, 1, len(padding), f)
    gdal.VSIFCloseL(f)

    ds = gdal.OpenEx(filename, gdal.OF_RASTER)
    gdal.Unlink(filename)
    assert ds is None


def test_ignfheightasciigrid_gra():

    tst = gdaltest.GDALTest(
        'IGNFHeightASCIIGrid', 'ignfheightasciigrid/ignfheightasciigrid.gra', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    tst.testOpen(check_gt=gt, check_prj='WGS84')

    ds = gdal.OpenEx('data/ignfheightasciigrid/ignfheightasciigrid.gra', gdal.OF_RASTER)
    assert ds.GetRasterBand(1).GetNoDataValue() == 9999


def test_ignfheightasciigrid_gra_invalid():

    contents = ['49 50\r\n2\r\n',  # missing values
                '49 50\r\n2 3\r\n',  # missing line
                '49 50\r\n2 3\r\nx 1\r\n',  # non numeric value
                '-200 50\r\n2 3\r\n1 1\r\n',  # invalid value
                ]

    filename = '/vsimem/ignfheightasciigrid_invalid.gra'
    for content in contents:
        gdal.FileFromMemBuffer(filename, content)
        with gdaltest.error_handler():
            assert not gdal.OpenEx(filename, gdal.OF_RASTER), content
        gdal.Unlink(filename)
    




