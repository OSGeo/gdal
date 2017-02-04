#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR OGDI driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import ogr

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
def ogr_ogdi_1():

    ogrtest.ogdi_ds = None

    # Skip tests when -fsanitize is used because of memleaks in libogdi
    if gdaltest.is_travis_branch('sanitize'):
       print('Skipping because of memory leaks in OGDI')
       ogrtest.ogdi_drv = None
       return 'skip'

    try:
        ogrtest.ogdi_drv = ogr.GetDriverByName('OGDI')
    except:
        ogrtest.ogdi_drv = None

    if ogrtest.ogdi_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://freefr.dl.sourceforge.net/project/ogdi/OGDI_Test_Suite/3.1/ogdits-3.1.0.zip', 'ogdits-3.1.0.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/ogdits-3.1')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'tmp/cache/ogdits-3.1.0.zip')
            try:
                os.stat('tmp/cache/ogdits-3.1')
            except:
                return 'skip'
        except:
            return 'skip'

    url_name ='gltp:/vrf/' + os.getcwd()+ '/tmp/cache/ogdits-3.1/data/vpf/vm2alv2/texash'

    ds = ogr.Open(url_name)
    ogrtest.ogdi_ds = ds
    if ds is None:
        gdaltest.post_reason('cannot open ' + url_name)
        return 'fail'
    if ds.GetLayerCount() != 57:
        print(ds.GetLayerCount())
        gdaltest.post_reason('did not get expected layer count')
        return 'fail'

    layers = [ ('libref@libref(*)_line', ogr.wkbLineString, 15),
               ('libreft@libref(*)_text', ogr.wkbPoint, 4),
               ('markersp@bnd(*)_point', ogr.wkbPoint, 40),
               ('polbnda@bnd(*)_area', ogr.wkbPolygon, 6)]

    for l in layers:
        lyr = ds.GetLayerByName(l[0])
        if lyr.GetLayerDefn().GetGeomType() != l[1]:
            return 'fail'
        if lyr.GetFeatureCount() != l[2]:
            print(lyr.GetFeatureCount())
            return 'fail'
        #if l[1] != ogr.wkbNone:
        #    if lyr.GetSpatialRef().ExportToWkt().find('WGS 84') == -1:
        #        return 'fail'

    lyr = ds.GetLayerByName('libref@libref(*)_line')
    feat = lyr.GetNextFeature()

    wkt = 'LINESTRING (-97.570159912109375 31.242000579833984,-97.569938659667969 31.242116928100586,-97.562828063964844 31.245765686035156,-97.558868408203125 31.247797012329102,-97.555778503417969 31.249361038208008,-97.55413818359375 31.250171661376953)'
    ref_geom = ogr.CreateGeometryFromWkt(wkt)

    if ogrtest.check_feature_geometry(feat, ref_geom) != 0:
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_ogdi_2():

    if ogrtest.ogdi_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    url_name ='gltp:/vrf/' + os.getcwd()+ '/tmp/cache/ogdits-3.1/data/vpf/vm2alv2/texash'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' --config OGR_OGDI_LAUNDER_LAYER_NAMES YES -ro "' + url_name + '" markersp_bnd contourl_elev polbnda_bnd extractp_ind')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test GetFeature()

def ogr_ogdi_3():

    if ogrtest.ogdi_ds is None:
        return 'skip'

    lyr0 = ogrtest.ogdi_ds.GetLayer(0)
    lyr0.ResetReading()
    feat00_ref = lyr0.GetNextFeature()
    feat01_ref = lyr0.GetNextFeature()
    feat02_ref = lyr0.GetNextFeature()

    lyr1 = ogrtest.ogdi_ds.GetLayer(1)
    lyr1.ResetReading()
    feat10_ref = lyr1.GetNextFeature()
    feat11_ref = lyr1.GetNextFeature()

    feat02 = lyr0.GetFeature(2)
    feat00 = lyr0.GetFeature(0)
    feat01 = lyr0.GetFeature(1)
    feat10 = lyr1.GetFeature(0)
    feat11 = lyr1.GetFeature(1)

    if not feat00.Equal(feat00_ref):
        gdaltest.post_reason('features not equal')
        return 'fail'

    if not feat01.Equal(feat01_ref):
        gdaltest.post_reason('features not equal')
        return 'fail'

    if not feat02.Equal(feat02_ref):
        gdaltest.post_reason('features not equal')
        return 'fail'

    if not feat10.Equal(feat10_ref):
        gdaltest.post_reason('features not equal')
        return 'fail'

    if not feat11.Equal(feat11_ref):
        gdaltest.post_reason('features not equal')
        return 'fail'

    return 'success'

###############################################################################
# Extract of full dataset

def ogr_ogdi_4():

    if ogrtest.ogdi_drv is None:
        return 'skip'

    url_name ='gltp:/vrf/' + os.getcwd()+ '/data/vm2alv2_texash/texash'
    ds = ogr.Open(url_name)
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.GetLayerCount() != 6:
        print(ds.GetLayerCount())
        gdaltest.post_reason('did not get expected layer count')
        return 'fail'

    layers = [ ('polbnda@bnd(*)_area', ogr.wkbPolygon, 6)]

    for l in layers:
        lyr = ds.GetLayerByName(l[0])
        if lyr.GetLayerDefn().GetGeomType() != l[1]:
            return 'fail'
        if lyr.GetFeatureCount() != l[2]:
            print(lyr.GetFeatureCount())
            return 'fail'

    lyr = ds.GetLayerByName('polbnda@bnd(*)_area')
    feat = lyr.GetNextFeature()

    if feat['id'] != 1 or feat['f_code'] != 'FA001' or feat['acc'] != 1:
        gdaltest.post_reason('bad attributes')
        feat.DumpReadable()
        return 'fail'

    wkt = 'POLYGON ((-97.6672973632812 31.250171661377,-97.5832977294922 31.250171661377,-97.5780029296875 31.250171661377,-97.5780029296875 31.250171661377,-97.5780944824219 31.2494583129883,-97.5779724121094 31.2492084503174,-97.577751159668 31.24880027771,-97.5776443481445 31.2484683990479,-97.5775451660156 31.2482070922852,-97.5774078369141 31.2479457855225,-97.5772705078125 31.2477989196777,-97.5771331787109 31.2477321624756,-97.5768661499023 31.2476787567139,-97.5766830444336 31.2476959228516,-97.5763168334961 31.2477016448975,-97.576042175293 31.247673034668,-97.5757141113281 31.2475509643555,-97.5754852294922 31.2473278045654,-97.5752792358398 31.2470207214356,-97.5751190185547 31.2467250823975,-97.5750122070312 31.2465076446533,-97.5748443603516 31.2462825775147,-97.5746002197266 31.2460918426514,-97.5742874145508 31.2459144592285,-97.5739288330078 31.2458171844482,-97.5736083984375 31.2457542419434,-97.5731201171875 31.2456817626953,-97.5728302001953 31.245641708374,-97.5724792480469 31.2455806732178,-97.5721817016602 31.2454471588135,-97.5719223022461 31.2453022003174,-97.5717086791992 31.2450218200684,-97.5715408325195 31.2446899414062,-97.5713882446289 31.2445201873779,-97.5711669921875 31.2442722320557,-97.5710678100586 31.2440910339355,-97.5711975097656 31.2438926696777,-97.5713577270508 31.2437191009521,-97.5718154907227 31.2434253692627,-97.5724258422852 31.2431831359863,-97.5726470947266 31.2430419921875,-97.5728530883789 31.2427291870117,-97.5728759765625 31.2424869537354,-97.57275390625 31.2423858642578,-97.5727996826172 31.2423534393311,-97.5712738037109 31.2422771453857,-97.5710067749023 31.2422466278076,-97.5707092285156 31.2421951293945,-97.5702285766602 31.2420444488525,-97.5701599121094 31.242000579834,-97.5701599121094 31.242000579834,-97.5794296264648 31.2372093200684,-97.5909194946289 31.2314224243164,-97.6050415039062 31.2241363525391,-97.6213302612305 31.2157878875732,-97.6490707397461 31.201566696167,-97.6662445068359 31.1928386688232,-97.6803207397461 31.1855792999268,-97.6936721801758 31.1787204742432,-97.7042617797852 31.1732997894287,-97.7107391357422 31.1699485778809,-97.7178192138672 31.1663246154785,-97.7325134277344 31.1587982177734,-97.7502975463867 31.1499614715576,-97.7502975463867 31.1499614715576,-97.7502975463867 31.1671733856201,-97.7502975463867 31.1671733856201,-97.7502975463867 31.250171661377,-97.6672973632812 31.250171661377))'
    ref_geom = ogr.CreateGeometryFromWkt(wkt)

    if ogrtest.check_feature_geometry(feat, ref_geom) != 0:
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds = None

    # Test opening one single layer
    ds = ogr.Open(url_name +':polbnda@bnd(*):area')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'
    if ds.GetLayerCount() != 1:
        print(ds.GetLayerCount())
        gdaltest.post_reason('did not get expected layer count')
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_ogdi_5():

    if ogrtest.ogdi_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    url_name ='gltp:/vrf/' + os.getcwd()+ '/data/vm2alv2_texash/texash'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' --config OGR_OGDI_LAUNDER_LAYER_NAMES YES -ro "' + url_name + '"')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################

def ogr_ogdi_cleanup():

    if ogrtest.ogdi_ds is None:
        return 'skip'

    ogrtest.ogdi_ds = None
    return 'success'


gdaltest_list = [
    ogr_ogdi_1,
    ogr_ogdi_2,
    ogr_ogdi_3,
    ogr_ogdi_4,
    ogr_ogdi_5,
    ogr_ogdi_cleanup]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ogdi' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

