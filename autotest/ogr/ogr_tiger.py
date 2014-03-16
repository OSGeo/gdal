#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR TIGER driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal, ogr, osr

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
            try:
                os.stat('tmp/cache/TGR01001')
            except:
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
        gdaltest.post_reason('fail')
        return 'fail'

    ogrtest.tiger_ds = None
    # also test opening with a filename (#4443)
    ogrtest.tiger_ds = ogr.Open('tmp/cache/TGR01001/TGR01001.RT1')
    if ogrtest.tiger_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check a few features.
    cc_layer = ogrtest.tiger_ds.GetLayerByName('CompleteChain')
    if cc_layer.GetFeatureCount() != 19289:
        gdaltest.post_reason( 'wrong cc feature count' )
        return 'fail'
    
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()

    if feat.TLID != 2833200 or feat.FRIADDL != None or feat.BLOCKL != 5000:
        gdaltest.post_reason( 'wrong attribute on cc feature.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-86.4402 32.504137,-86.440313 32.504009,-86.440434 32.503884,-86.440491 32.503805,-86.44053 32.503757,-86.440578 32.503641,-86.440593 32.503515,-86.440588 32.503252,-86.440596 32.50298)', max_error = 0.000001 ) != 0:
        return 'fail'

    feat = ogrtest.tiger_ds.GetLayerByName('TLIDRange').GetNextFeature()
    if feat.MODULE != 'TGR01001' or feat.TLMINID != 2822718:
        gdaltest.post_reason( 'got wrong TLIDRange attributes' )
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
# Load into a /vsimem instance to test virtualization.

def ogr_tiger_4():
    
    if ogrtest.tiger_ds is None:
        return 'skip'

    # load all the files into memory.
    for file in gdal.ReadDir('tmp/cache/TGR01001'):

        if file[0] == '.':
            continue

        data = open('tmp/cache/TGR01001/'+file,'r').read()
        
        f = gdal.VSIFOpenL('/vsimem/tigertest/'+file, 'wb')
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

    # Try reading.
    ogrtest.tiger_ds = ogr.Open('/vsimem/tigertest/TGR01001.RT1')
    if ogrtest.tiger_ds is None:
        gdaltest.post_reason('fail to open.')
        return 'fail'

    ogrtest.tiger_ds = None
    # also test opening with a filename (#4443)
    ogrtest.tiger_ds = ogr.Open('tmp/cache/TGR01001/TGR01001.RT1')
    if ogrtest.tiger_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check a few features.
    cc_layer = ogrtest.tiger_ds.GetLayerByName('CompleteChain')
    if cc_layer.GetFeatureCount() != 19289:
        gdaltest.post_reason( 'wrong cc feature count' )
        return 'fail'
    
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()

    if feat.TLID != 2833200 or feat.FRIADDL != None or feat.BLOCKL != 5000:
        gdaltest.post_reason( 'wrong attribute on cc feature.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-86.4402 32.504137,-86.440313 32.504009,-86.440434 32.503884,-86.440491 32.503805,-86.44053 32.503757,-86.440578 32.503641,-86.440593 32.503515,-86.440588 32.503252,-86.440596 32.50298)', max_error = 0.000001 ) != 0:
        return 'fail'

    feat = ogrtest.tiger_ds.GetLayerByName('TLIDRange').GetNextFeature()
    if feat.MODULE != 'TGR01001' or feat.TLMINID != 2822718:
        gdaltest.post_reason( 'got wrong TLIDRange attributes' )
        return 'fail'
    
    # Try to recover memory from /vsimem.
    for file in gdal.ReadDir('tmp/cache/TGR01001'):

        if file[0] == '.':
            continue

        gdal.Unlink( '/vsimem/tigertest/'+file )

    return 'success'

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
    ogr_tiger_4,
    ogr_tiger_cleanup]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_tiger' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

