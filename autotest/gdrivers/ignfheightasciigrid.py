#!/usr/bin/env python
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


def ignfheightasciigrid_1():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid_ar1.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def ignfheightasciigrid_2():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid_ar2.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def ignfheightasciigrid_3():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid_ar3.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def ignfheightasciigrid_4():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid_ar4.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def ignfheightasciigrid_5():

    tst = gdaltest.GDALTest('IGNFHeightASCIIGrid',
                            'ignfheightasciigrid_ar1_nocoords.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def ignfheightasciigrid_6():

    tst = gdaltest.GDALTest(
        'IGNFHeightASCIIGrid', 'ignfheightasciigrid_ar1_nocoords_noprec.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def ignfheightasciigrid_7():

    tst = gdaltest.GDALTest(
        'IGNFHeightASCIIGrid', 'ignfheightasciigrid_ar1_noprec.mnt', 1, 21)
    gt = (-152.125, 0.25, 0.0, -16.375, 0.0, -0.25)
    return tst.testOpen(check_gt=gt, check_prj='WGS84')


def ignfheightasciigrid_invalid():

    filename = '/vsimem/ignfheightasciigrid_invalid'
    ok_content = '2 3 49 50 1 1 1 0 1 0 -0. DESC\r1 2 3 4'
    gdal.FileFromMemBuffer(filename, ok_content)
    ds = gdal.OpenEx(filename)
    if not ds:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetUnitType() != 'm':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink(filename)

    contents = ['0 0 0 0 0 0 0 0 0 0 0 0\r',
                '                   \r',
                '2 3 49 50 1 1 1 0 1 0 0 DESC', # no newline
                '2 3 49 50 1 1 1 0 1 0 0\r1 2 3 4', # missing description in header
                '2 3 49 50 1 1 1 a 1 0 0 DESC\r1 2 3 4', # not a number in numeric header section
                '2 3 49 50 1 1 1 0 1 0 0 DESC\ra 2 3 4', # not a number in value section
                '2 3 49 50 1 1 1 0 1 0 0 DES\xC3\xA8C\r1 2 3 4',  # invalid character in comment
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
            if gdal.OpenEx(filename, gdal.OF_RASTER):
                gdaltest.post_reason('fail')
                print(content)
                return 'fail'
        gdal.Unlink(filename)
    return 'success'


gdaltest_list = [
    ignfheightasciigrid_1,
    ignfheightasciigrid_2,
    ignfheightasciigrid_3,
    ignfheightasciigrid_4,
    ignfheightasciigrid_5,
    ignfheightasciigrid_6,
    ignfheightasciigrid_7,
    ignfheightasciigrid_invalid,
]


if __name__ == '__main__':

    gdaltest.setup_run('IGNFHeightASCIIGrid')

    gdaltest.run_tests(gdaltest_list)

    sys.exit(gdaltest.summarize())
