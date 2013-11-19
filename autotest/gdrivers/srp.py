#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for SRP driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read USRP dataset with PCB=0

def srp_1(filename = 'USRP_PCB0/FKUSRP01.IMG'):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32600 + 17)

    tst = gdaltest.GDALTest( 'SRP', filename, 1, 24576 )
    ret = tst.testOpen( check_prj = srs.ExportToWkt(), check_gt = (500000.0, 5.0, 0.0, 5000000.0, 0.0, -5.0) )
    if ret != 'success':
        return ret

    ds = gdal.Open('data/' + filename)
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_PaletteIndex:
        gdaltest.post_reason('fail')
        return 'fail'

    ct = ds.GetRasterBand(1).GetColorTable()
    if ct.GetCount() != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    if ct.GetColorEntry(0) != (0,0,0,255):
        gdaltest.post_reason('fail')
        print(ct.GetColorEntry(0))
        return 'fail'

    if ct.GetColorEntry(1) != (255,0,0,255):
        gdaltest.post_reason('fail')
        print(ct.GetColorEntry(1))
        return 'fail'

    expected_md = [ 'SRP_CLASSIFICATION=U',
            'SRP_CREATIONDATE=20120505',
            'SRP_EDN=0',
            'SRP_NAM=FKUSRP',
            'SRP_PRODUCT=USRP',
            'SRP_REVISIONDATE=20120505',
            'SRP_SCA=50000',
            'SRP_ZNA=17' ]

    got_md = ds.GetMetadata()
    for md in expected_md:
        (key,value) = md.split('=')
        if not key in got_md or got_md[key] != value:
            gdaltest.post_reason('did not find %s' % md)
            print(got_md)
            return 'fail'

    return 'success'

###############################################################################
# Read USRP dataset with PCB=4

def srp_2():
    return srp_1('USRP_PCB4/FKUSRP01.IMG')

###############################################################################
# Read USRP dataset with PCB=8

def srp_3():
    return srp_1('USRP_PCB8/FKUSRP01.IMG')
    
###############################################################################
# Read from TRANSH01.THF file.

def srp_4():

    tst = gdaltest.GDALTest( 'SRP', 'USRP_PCB0/TRANSH01.THF', 1, 24576 )
    return tst.testOpen()

###############################################################################
# Read from TRANSH01.THF file (without "optimization" for single GEN in THF)

def srp_5():

    gdal.SetConfigOption('SRP_SINGLE_GEN_IN_THF_AS_DATASET', 'FALSE')
    ds = gdal.Open('data/USRP_PCB0/TRANSH01.THF')
    gdal.SetConfigOption('SRP_SINGLE_GEN_IN_THF_AS_DATASET', None)
    subdatasets = ds.GetMetadata('SUBDATASETS')
    if subdatasets['SUBDATASET_1_NAME'] != 'SRP:data/USRP_PCB0/FKUSRP01.GEN,data/USRP_PCB0/FKUSRP01.IMG' and \
       subdatasets['SUBDATASET_1_NAME'] != 'SRP:data/USRP_PCB0\\FKUSRP01.GEN,data/USRP_PCB0\\FKUSRP01.IMG':
        gdaltest.post_reason('fail')
        print(subdatasets)
        return 'fail'
    if subdatasets['SUBDATASET_1_DESC'] != 'SRP:data/USRP_PCB0/FKUSRP01.GEN,data/USRP_PCB0/FKUSRP01.IMG' and \
       subdatasets['SUBDATASET_1_DESC'] != 'SRP:data/USRP_PCB0\\FKUSRP01.GEN,data/USRP_PCB0\\FKUSRP01.IMG':
        gdaltest.post_reason('fail')
        print(subdatasets)
        return 'fail'

    expected_md = [ 'SRP_CLASSIFICATION=U',
            'SRP_CREATIONDATE=20120505',
            'SRP_EDN=1',
            'SRP_VOO=           ' ]

    got_md = ds.GetMetadata()
    for md in expected_md:
        (key,value) = md.split('=')
        if not key in got_md or got_md[key] != value:
            gdaltest.post_reason('did not find %s' % md)
            print(got_md)
            return 'fail'

    return 'success'

###############################################################################
# Read with subdataset syntax

def srp_6():

    tst = gdaltest.GDALTest( 'SRP', 'SRP:data/USRP_PCB4/FKUSRP01.GEN,data/USRP_PCB4/FKUSRP01.IMG', 1, 24576, filename_absolute = 1 )
    return tst.testOpen()

###############################################################################

gdaltest_list = [
    srp_1,
    srp_2,
    srp_3,
    srp_4,
    srp_5,
    srp_6
 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'srp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

