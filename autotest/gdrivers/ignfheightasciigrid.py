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


gdaltest_list = [
    ignfheightasciigrid_1,
    ignfheightasciigrid_2,
    ignfheightasciigrid_3,
    ignfheightasciigrid_4,
    ignfheightasciigrid_5,
    ignfheightasciigrid_6,
    ignfheightasciigrid_7,
]


if __name__ == '__main__':

    gdaltest.setup_run('IGNFHeightASCIIGrid')

    gdaltest.run_tests(gdaltest_list)

    sys.exit(gdaltest.summarize())
