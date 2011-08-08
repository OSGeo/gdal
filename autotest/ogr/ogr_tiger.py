#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR TIGER driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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
import ogr
import osr

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
def ogr_tiger_1():

    ogrtest.tiger_ds = None

    if not gdaltest.download_file('http://www2.census.gov/geo/tiger/tiger2006se/AL/TGR01001.ZIP', 'TGR01001.ZIP'):
        return 'skip'

    try:
        os.stat('tmp/cache/TGR01001/TGR01001.MET')
    except:
        try:
            os.mkdir('tmp/cache/TGR01001')
            gdaltest.unzip( 'tmp/cache/TGR01001', 'tmp/cache/TGR01001.ZIP')
            try:
                os.stat('tmp/cache/TGR01001/TGR01001.MET')
            except:
                return 'skip'
        except:
            return 'skip'

    ogrtest.tiger_ds = ogr.Open('tmp/cache/TGR01001')
    if ogrtest.tiger_ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_tiger_2():

    if ogrtest.tiger_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/cache/TGR01001')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test TIGER writing

def ogr_tiger_3():

    if ogrtest.tiger_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        shutil.rmtree('tmp/outtiger')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f TIGER tmp/outtiger tmp/cache/TGR01001 -dsco VERSION=1006')

    ret = 'success'

    filelist = os.listdir('tmp/cache/TGR01001')
    exceptions = [ 'TGR01001.RTA', 'TGR01001.RTC', 'TGR01001.MET', 'TGR01001.RTZ', 'TGR01001.RTS']
    for filename in filelist:
        if filename in exceptions:
            continue
        f = open('tmp/cache/TGR01001/' + filename, 'rb')
        data1 = f.read()
        f.close()
        try:
            f = open('tmp/outtiger/' + filename, 'rb')
            data2 = f.read()
            f.close()
            if data1 != data2:
                #gdaltest.post_reason('%s is different' % filename)
                print('%s is different' % filename)
                ret = 'fail'
        except:
            #gdaltest.post_reason('could not find %s' % filename)
            print('could not find %s' % filename)
            ret = 'fail'

    try:
        shutil.rmtree('tmp/outtiger')
    except:
        pass

    return ret

###############################################################################

def ogr_tiger_cleanup():

    if ogrtest.tiger_ds is None:
        return 'skip'

    ogrtest.tiger_ds = None
    return 'success'


gdaltest_list = [
    ogr_tiger_1,
    ogr_tiger_2,
    ogr_tiger_3,
    ogr_tiger_cleanup]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_tiger' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

