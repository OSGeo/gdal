#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for SRP driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal
from osgeo import osr


import gdaltest

###############################################################################
# Read USRP dataset with PCB=0


def test_srp_1(filename='srp/USRP_PCB0/FKUSRP01.IMG'):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32600 + 17)

    tst = gdaltest.GDALTest('SRP', filename, 1, 24576)
    tst.testOpen(check_prj=srs.ExportToWkt(), check_gt=(500000.0, 5.0, 0.0, 5000000.0, 0.0, -5.0))

    ds = gdal.Open('data/' + filename)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex

    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct.GetCount() == 4

    assert ct.GetColorEntry(0) == (0, 0, 0, 255)

    assert ct.GetColorEntry(1) == (255, 0, 0, 255)

    expected_md = ['SRP_CLASSIFICATION=U',
                   'SRP_CREATIONDATE=20120505',
                   'SRP_EDN=0',
                   'SRP_NAM=FKUSRP',
                   'SRP_PRODUCT=USRP',
                   'SRP_REVISIONDATE=20120505',
                   'SRP_SCA=50000',
                   'SRP_ZNA=17']

    got_md = ds.GetMetadata()
    for md in expected_md:
        (key, value) = md.split('=')
        assert key in got_md and got_md[key] == value, ('did not find %s' % md)

    
###############################################################################
# Read USRP dataset with PCB=4


def test_srp_2():
    return test_srp_1('srp/USRP_PCB4/FKUSRP01.IMG')

###############################################################################
# Read USRP dataset with PCB=8


def test_srp_3():
    return test_srp_1('srp/USRP_PCB8/FKUSRP01.IMG')

###############################################################################
# Read from TRANSH01.THF file.


def test_srp_4():

    tst = gdaltest.GDALTest('SRP', 'srp/USRP_PCB0/TRANSH01.THF', 1, 24576)
    ret = tst.testOpen()
    return ret

###############################################################################
# Read from TRANSH01.THF file (without "optimization" for single GEN in THF)


def test_srp_5():

    gdal.SetConfigOption('SRP_SINGLE_GEN_IN_THF_AS_DATASET', 'FALSE')
    ds = gdal.Open('data/srp/USRP_PCB0/TRANSH01.THF')
    gdal.SetConfigOption('SRP_SINGLE_GEN_IN_THF_AS_DATASET', None)
    subdatasets = ds.GetMetadata('SUBDATASETS')
    assert subdatasets['SUBDATASET_1_NAME'].replace('\\', '/') == 'SRP:data/srp/USRP_PCB0/FKUSRP01.GEN,data/srp/USRP_PCB0/FKUSRP01.IMG'
    assert subdatasets['SUBDATASET_1_DESC'].replace('\\', '/') == 'SRP:data/srp/USRP_PCB0/FKUSRP01.GEN,data/srp/USRP_PCB0/FKUSRP01.IMG'

    expected_md = ['SRP_CLASSIFICATION=U',
                   'SRP_CREATIONDATE=20120505',
                   'SRP_EDN=1',
                   'SRP_VOO=           ']

    got_md = ds.GetMetadata()
    for md in expected_md:
        (key, value) = md.split('=')
        assert key in got_md and got_md[key] == value, ('did not find %s' % md)

    
###############################################################################
# Read with subdataset syntax


def test_srp_6():

    tst = gdaltest.GDALTest('SRP', 'SRP:data/srp/USRP_PCB4/FKUSRP01.GEN,data/srp/USRP_PCB4/FKUSRP01.IMG', 1, 24576, filename_absolute=1)
    tst.testOpen()


###############################################################################
# Cleanup

def test_srp_cleanup():

    # FIXME ?
    os.unlink('data/srp/USRP_PCB0/TRANSH01.THF.aux.xml')

###############################################################################
