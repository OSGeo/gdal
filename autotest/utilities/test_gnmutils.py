#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic GNMGdalNetwork class functionality.
# Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
#           Dmitry Baryshnikov, polimax@mail.ru
#
###############################################################################
# Copyright (c) 2014, Mikhail Gusev
# Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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
import os

sys.path.append( '../pymod' )

import gdaltest
import test_cli_utilities

###############################################################################
# Test create
#gnmmanage create -f GNMFile -t_srs EPSG:4326 -dsco net_name=test_gnm -dsco net_description="Test file based GNM" /home/bishop/tmp/ --config CPL_DEBUG ON

def test_gnmmanage_1():
    if test_cli_utilities.get_gnmmanage_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gnmmanage_path() + ' create -f GNMFile -t_srs EPSG:4326 -dsco net_name=test_gnm -dsco net_description="Test file based GNM" tmp')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    try:
        os.stat('tmp/test_gnm')
        pass
    except:
        gdaltest.post_reason('Expected create tmp/test_gnm')
        return 'fail'

    return 'success'

###############################################################################
# Test import
# gnmmanage import /home/bishop/tmp/data/pipes.shp /home/bishop/tmp/test_gnm --config CPL_DEBUG ON
# gnmmanage import /home/bishop/tmp/data/wells.shp /home/bishop/tmp/test_gnm --config CPL_DEBUG ON

def test_gnmmanage_2():
    if test_cli_utilities.get_gnmmanage_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gnmmanage_path() + ' import ../gnm/data/pipes.shp tmp/test_gnm')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gnmmanage_path() + ' import ../gnm/data/wells.shp tmp/test_gnm')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test info
# gnmmanage info /home/bishop/tmp/test_gnm

def test_gnmmanage_3():
    if test_cli_utilities.get_gnmmanage_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gnmmanage_path() + ' info tmp/test_gnm')

    if ret.find('Network version: 1.0.') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ret.find('Network name: test_gnm.') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ret.find('Network description') == -1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test autoconect
# gnmmanage autoconnect 0.000001 /home/bishop/tmp/test_gnm --config CPL_DEBUG ON

def test_gnmmanage_4():
    if test_cli_utilities.get_gnmmanage_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gnmmanage_path() + ' autoconnect 0.000001 tmp/test_gnm')
    if ret.find('success') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test dijkstra
# gnmanalyse dijkstra 61 50 -alo "fetch_vertex=OFF" -ds /home/bishop/tmp/di.shp -lco "SHPT=ARC" /home/bishop/tmp/test_gnm --config CPL_DEBUG ON

def test_gnmanalyse_1():
    if test_cli_utilities.get_gnmanalyse_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gnmanalyse_path() + ' dijkstra 61 50 tmp/test_gnm')
    if ret.find('Feature Count: 19') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test kpaths
# gnmanalyse kpaths 61 50 3 -alo "fetch_vertex=OFF" -ds /home/bishop/tmp/kp.shp -lco "SHPT=ARC" /home/bishop/tmp/test_gnm --config CPL_DEBUG ON

def test_gnmanalyse_2():
    if test_cli_utilities.get_gnmanalyse_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gnmanalyse_path() + ' kpaths 61 50 3 tmp/test_gnm')
    if ret.find('Feature Count: 61') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test cleanup

def test_gnm_cleanup():
    if test_cli_utilities.get_gnmmanage_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gnmmanage_path() + ' delete tmp/test_gnm')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    try:
        os.stat('tmp/test_gnm')
        gdaltest.post_reason('Expected delete tmp/test_gnm')
        return 'fail'
    except:
        pass

    return 'success'



gdaltest_list = [
    test_gnmmanage_1,
    test_gnmmanage_2,
    test_gnmmanage_3,
    test_gnmmanage_4,
    test_gnmanalyse_1,
    test_gnmanalyse_2,
    test_gnm_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gnmutils' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





