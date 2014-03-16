#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR S-57 driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import string
import shutil

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################
# Verify we can open the test file.

def ogr_s57_1():

    gdaltest.s57_ds = None

    # Clear S57 options if set or our results will be messed up.
    if gdal.GetConfigOption( 'OGR_S57_OPTIONS', '' ) != '':
        gdal.SetConfigOption( 'OGR_S57_OPTIONS', '' )
        
    gdaltest.s57_ds = ogr.Open( 'data/1B5X02NE.000' )    
    if gdaltest.s57_ds is None:
        gdaltest.post_reason( 'failed to open test file.' )
        return 'fail'

    return 'success'

###############################################################################
# Verify we have the set of expected layers and that some rough information
# matches our expectations. 

def ogr_s57_2():
    if gdaltest.s57_ds is None:
        return 'skip'

    layer_list = [ ('DSID', ogr.wkbNone, 1),
                   ('COALNE', ogr.wkbUnknown, 1),
                   ('DEPARE', ogr.wkbUnknown, 4),
                   ('DEPCNT', ogr.wkbUnknown, 4),
                   ('LNDARE', ogr.wkbUnknown, 1),
                   ('LNDELV', ogr.wkbUnknown, 2),
                   ('SBDARE', ogr.wkbUnknown, 2),
                   ('SLCONS', ogr.wkbUnknown, 1),
                   ('SLOTOP', ogr.wkbUnknown, 1),
                   ('SOUNDG', ogr.wkbMultiPoint25D, 2),
                   ('M_COVR', ogr.wkbPolygon, 1),
                   ('M_NSYS', ogr.wkbPolygon, 1),
                   ('M_QUAL', ogr.wkbPolygon, 1) ]


    if gdaltest.s57_ds.GetLayerCount() != len(layer_list):
        gdaltest.post_reason( 'Did not get expected number of layers, likely cant find support files.' )
        return 'fail'
    
    for i in range(len(layer_list)):
        lyr = gdaltest.s57_ds.GetLayer( i )
        lyr_info = layer_list[i]
        
        if lyr.GetName() != lyr_info[0]:
            gdaltest.post_reason( 'Expected layer %d to be %s but it was %s.'\
                                  % (i+1, lyr_info[0], lyr.GetName()) )
            return 'fail'

        count = lyr.GetFeatureCount(force=1)
        if count != lyr_info[2]:
            gdaltest.post_reason( 'Expected %d features in layer %s, but got %d.' % (lyr_info[2], lyr_info[0], count) )
            return 'fail'

        if lyr.GetLayerDefn().GetGeomType() != lyr_info[1]:
            gdaltest.post_reason( 'Expected %d layer type in layer %s, but got %d.' % (lyr_info[1], lyr_info[0], lyr.GetLayerDefn().GetGeomType()) )
            return 'fail'
            
    return 'success'

###############################################################################
# Check the COALNE feature. 

def ogr_s57_3():
    if gdaltest.s57_ds is None:
        return 'skip'

    feat = gdaltest.s57_ds.GetLayerByName('COALNE').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected COALNE feature at all.' )
        return 'fail'
    
    if feat.GetField( 'RCID' ) != 1 \
           or feat.GetField( 'LNAM' ) != 'FFFF7F4F0FB002D3' \
           or feat.GetField( 'OBJL' ) != 30 \
           or feat.GetField( 'AGEN' ) != 65535:
        gdaltest.post_reason( 'COALNE: did not get expected attributes' )
        return 'fail'

    wkt = 'LINESTRING (60.97683400 -32.49442600,60.97718200 -32.49453800,60.97742400 -32.49477400,60.97774800 -32.49504000,60.97791600 -32.49547200,60.97793000 -32.49581800,60.97794400 -32.49617800,60.97804400 -32.49647600,60.97800200 -32.49703800,60.97800200 -32.49726600,60.97805800 -32.49749400,60.97812800 -32.49773200,60.97827000 -32.49794800,60.97910200 -32.49848600,60.97942600 -32.49866600)'

    if ogrtest.check_feature_geometry( feat, wkt ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Check the M_QUAL feature.

def ogr_s57_4():
    if gdaltest.s57_ds is None:
        return 'skip'

    feat = gdaltest.s57_ds.GetLayerByName('M_QUAL').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected M_QUAL feature at all.' )
        return 'fail'
    
    if feat.GetField( 'RCID' ) != 15 \
           or feat.GetField( 'OBJL' ) != 308 \
           or feat.GetField( 'AGEN' ) != 65535:
        gdaltest.post_reason( 'M_QUAL: did not get expected attributes' )
        return 'fail'

    wkt = 'POLYGON ((60.97683400 -32.49534000,60.97683400 -32.49762000,60.97683400 -32.49866600,60.97869000 -32.49866600,60.97942600 -32.49866600,60.98215200 -32.49866600,60.98316600 -32.49866600,60.98316600 -32.49755800,60.98316600 -32.49477000,60.98316600 -32.49350000,60.98146800 -32.49350000,60.98029800 -32.49350000,60.97947400 -32.49350000,60.97901600 -32.49350000,60.97683400 -32.49350000,60.97683400 -32.49442600,60.97683400 -32.49469800,60.97683400 -32.49534000))'

    if ogrtest.check_feature_geometry( feat, wkt ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Check the SOUNDG feature.

def ogr_s57_5():
    if gdaltest.s57_ds is None:
        return 'skip'

    feat = gdaltest.s57_ds.GetLayerByName('SOUNDG').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected SOUNDG feature at all.' )
        return 'fail'
    
    if feat.GetField( 'RCID' ) != 20 \
           or feat.GetField( 'OBJL' ) != 129 \
           or feat.GetField( 'AGEN' ) != 65535:
        gdaltest.post_reason( 'SOUNDG: did not get expected attributes' )
        return 'fail'

    wkt = 'MULTIPOINT (60.98164400 -32.49449000 3.400,60.98134400 -32.49642400 1.400,60.97814200 -32.49487400 -3.200,60.98071200 -32.49519600 1.200)'

    if ogrtest.check_feature_geometry( feat, wkt ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Test reading features from dataset with some double byte attributes. (#1526)

def ogr_s57_6():
    if gdaltest.s57_ds is None:
        return 'skip'

    ds = ogr.Open( 'data/bug1526.000' )
    
    feat = ds.GetLayerByName('FOGSIG').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected FOGSIG feature at all.' )
        return 'fail'
    
    if feat.GetField( 'INFORM' ) != 'During South winds nautophone is not always heard in S direction from lighthouse' \
       or len(feat.GetField( 'NINFOM' )) < 1:
        gdaltest.post_reason( 'FOGSIG: did not get expected attributes' )
        return 'fail'

    feat.Destroy()
    ds = None

    return 'success'

###############################################################################
# Test handling of a dataset with a multilinestring feature (#2147).

def ogr_s57_7():
    if gdaltest.s57_ds is None:
        return 'skip'

    ds = ogr.Open( 'data/bug2147_3R7D0889.000' )
    
    feat = ds.GetLayerByName('ROADWY').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected feature at all.' )
        return 'fail'

    exp_wkt = 'MULTILINESTRING ((22.5659615 44.5541942,22.5652045 44.5531651,22.5654315 44.5517774,22.5663008 44.5510096,22.5656187 44.5500822,22.5654462 44.5495941,22.5637522 44.5486793,22.563408 44.5477286,22.5654087 44.5471198,22.5670327 44.5463937,22.5667729 44.5456512,22.5657613 44.544027,22.5636273 44.5411638,22.5623421 44.5400398,22.559403 44.5367489,22.5579112 44.534544,22.5566466 44.5309514,22.5563888 44.5295231,22.5549946 44.5285915,22.5541939 44.5259331,22.5526434 44.5237888),(22.5656187 44.5500822,22.5670219 44.5493519,22.5684077 44.5491452),(22.5350702 44.4918838,22.5329111 44.4935825,22.5318719 44.4964337,22.5249608 44.5027089,22.5254709 44.5031914,22.5295138 44.5052214,22.5331359 44.5077711,22.5362468 44.5092751,22.5408091 44.5115306,22.5441312 44.5127374,22.5461053 44.5132675,22.5465694 44.5149956),(22.5094658 44.4989464,22.5105135 44.4992481,22.5158217 44.4994216,22.5206067 44.4998907,22.523096 44.5009452,22.5249608 44.5027089),(22.5762962 44.4645734,22.5767653 44.4773213,22.5769802 44.4796618,22.5775485 44.4815858,22.5762434 44.4842544,22.5765836 44.4855091,22.5775087 44.4865991,22.5769145 44.4879336,22.5708196 44.4910838,22.5694028 44.4930833,22.5692354 44.4958977),(22.5763768 44.5029527,22.5799605 44.501315,22.5831172 44.5007428,22.584524 44.4999964,22.5848604 44.4999039),(22.5731362 44.5129105,22.5801378 44.5261859,22.5825748 44.5301187),(22.5093748 44.5311182,22.5107969 44.5285258,22.5108905 44.5267978,22.5076679 44.5223309))'

    if ogrtest.check_feature_geometry( feat, exp_wkt ):
        return 'fail'
    
    feat.Destroy()
    ds = None

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_s57_8():
    if gdaltest.s57_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/1B5X02NE.000')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test S57 to S57 conversion

def ogr_s57_9():
    if gdaltest.s57_ds is None:
        return 'skip'

    try:
        os.unlink('tmp/ogr_s57_9.000')
    except:
        pass

    gdal.SetConfigOption('OGR_S57_OPTIONS', 'RETURN_PRIMITIVES=ON,RETURN_LINKAGES=ON,LNAM_REFS=ON')
    ds = ogr.GetDriverByName('S57').CreateDataSource('tmp/ogr_s57_9.000')
    src_ds = ogr.Open('data/1B5X02NE.000')
    gdal.SetConfigOption('OGR_S57_OPTIONS', None)
    for src_lyr in src_ds:
        if src_lyr.GetName() == 'DSID':
            continue
        lyr = ds.GetLayerByName(src_lyr.GetName())
        for src_feat in src_lyr:
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetFrom(src_feat)
            lyr.CreateFeature(feat)
            feat = None
    src_ds = None
    ds = None

    ds = ogr.Open( 'tmp/ogr_s57_9.000' )
    if ds is None:
        return 'fail'

    gdaltest.s57_ds = ds
    if ogr_s57_2() != 'success':
        return 'fail'
    if ogr_s57_3() != 'success':
        return 'fail'
    if ogr_s57_4() != 'success':
        return 'fail'
    if ogr_s57_5() != 'success':
        return 'fail'

    try:
        os.unlink('tmp/ogr_s57_9.000')
    except:
        pass

    return 'success'

###############################################################################
# Test decoding of Dutch inland ENCs (#3881).

def ogr_s57_online_1():
    if gdaltest.s57_ds is None:
        return 'skip'

    if not gdaltest.download_file('ftp://sdg.ivs90.nl/ENC/1R5MK050.000', '1R5MK050.000'):
        return 'skip'

    ds = ogr.Open( 'tmp/cache/1R5MK050.000' )
    if ds is None:
        return 'fail'

    lyr = ds.GetLayerByName('BUISGL')
    feat = lyr.GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected feature at all.' )
        return 'fail'

    exp_wkt = 'POLYGON ((5.6666667 53.0279027,5.6666667 53.0281667,5.6667012 53.0281685,5.666673 53.0282377,5.666788 53.0282616,5.6669018 53.0281507,5.6668145 53.0281138,5.6668121 53.0280649,5.6666686 53.0280248,5.6666713 53.0279647,5.6667572 53.0279713,5.6667568 53.0279089,5.6666667 53.0279027))'

    if ogrtest.check_feature_geometry( feat, exp_wkt ):
        return 'fail'

    feat = None

    ds = None

    return 'success'

###############################################################################
# Test with ENC 3.0 TDS - tile without updates.

def ogr_s57_online_2():
    if gdaltest.s57_ds is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/s57/enctds/GB5X01SW.000', 'GB5X01SW.000'):
        return 'skip'

    gdaltest.clean_tmp()
    shutil.copy( 'tmp/cache/GB5X01SW.000', 'tmp/GB5X01SW.000' )
    ds = ogr.Open( 'tmp/GB5X01SW.000' )
    if ds is None:
        return 'fail'

    lyr = ds.GetLayerByName('LIGHTS')
    feat = lyr.GetFeature(542)

    if feat is None:
        gdaltest.post_reason( 'Did not get expected feature at all.' )
        return 'fail'

    if feat.rver != 1:
        gdaltest.post_reason( 'Did not get expected RVER value (%d).' % feat.rver )
        return 'fail'

    lyr = ds.GetLayerByName('BOYCAR')
    feat = lyr.GetFeature(975)
    if feat is not None:
        gdaltest.post_reason( 'unexpected got feature id 975 before update!' )
        return 'fail'

    feat = None

    ds = None

    return 'success'

###############################################################################
# Test with ENC 3.0 TDS - tile with updates.

def ogr_s57_online_3():
    if gdaltest.s57_ds is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/s57/enctds/GB5X01SW.001', 'GB5X01SW.001'):
        return 'skip'

    shutil.copy( 'tmp/cache/GB5X01SW.001', 'tmp/GB5X01SW.001' )
    ds = ogr.Open( 'tmp/GB5X01SW.000' )
    if ds is None:
        return 'fail'

    lyr = ds.GetLayerByName('LIGHTS')
    feat = lyr.GetFeature(542)

    if feat is None:
        gdaltest.post_reason( 'Did not get expected feature at all.' )
        return 'fail'

    if feat.rver != 2:
        gdaltest.post_reason( 'Did not get expected RVER value (%d).' % feat.rver )
        return 'fail'

    lyr = ds.GetLayerByName('BOYCAR')
    feat = lyr.GetFeature(975)
    if feat is None:
        gdaltest.post_reason( 'unexpected dit not get feature id 975 after update!' )
        return 'fail'

    feat = None

    ds = None

    gdaltest.clean_tmp()

    return 'success'

###############################################################################
# Test ENC LL2 (#5048)

def ogr_s57_online_4():
    if gdaltest.s57_ds is None:
        return 'skip'

    if not gdaltest.download_file('http://www1.kaiho.mlit.go.jp/KOKAI/ENC/images/sample/sample.zip', 'sample.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/ENC_ROOT/JP34NC94.000')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'tmp/cache/sample.zip')
            try:
                os.stat('tmp/cache/ENC_ROOT/JP34NC94.000')
            except:
                return 'skip'
        except:
            return 'skip'

    gdal.SetConfigOption('OGR_S57_OPTIONS', 'RETURN_PRIMITIVES=ON,RETURN_LINKAGES=ON,LNAM_REFS=ON,RECODE_BY_DSSI=ON')
    ds = ogr.Open('tmp/cache/ENC_ROOT/JP34NC94.000')
    gdal.SetConfigOption('OGR_S57_OPTIONS', None)
    lyr = ds.GetLayerByName('LNDMRK')
    for feat in lyr:
        mystr = feat.NOBJNM
        if mystr and sys.version_info < (3,0,0):
            mystr.decode('UTF-8').encode('UTF-8')

    return 'success'

###############################################################################
#  Cleanup

def ogr_s57_cleanup():

    gdaltest.s57_ds = None

    return 'success'

gdaltest_list = [ 
    ogr_s57_1,
    ogr_s57_2,
    ogr_s57_3,
    ogr_s57_4,
    ogr_s57_5,
    ogr_s57_6,
    ogr_s57_7,
    ogr_s57_8,
    ogr_s57_9,
    ogr_s57_online_1,
    ogr_s57_online_2,
    ogr_s57_online_3,
    ogr_s57_online_4,
    ogr_s57_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_s57' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

