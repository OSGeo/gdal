#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  NAS Reading Driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr, osr

# Other test data :
# http://www.lv-bw.de/alkis.info/nas-bsp.html
# http://www.lv-bw.de/lvshop2/Produktinfo/AAA/AAA.html
# http://www.gll.niedersachsen.de/live/live.php?navigation_id=10640&article_id=51644&_psmand=34

###############################################################################
# Test reading a NAS file
#

def ogr_nas_1():

    try:
        drv = ogr.GetDriverByName('NAS')
    except:
        drv = None

    if drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.geodatenzentrum.de/gdz1/abgabe/testdaten/vektor/nas_testdaten_peine.zip', 'nas_testdaten_peine.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/BKG_NAS_Peine.xml')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'tmp/cache/nas_testdaten_peine.zip')
            try:
                os.stat('tmp/cache/BKG_NAS_Peine.xml')
            except:
                return 'skip'
        except:
            return 'skip'

    ds = ogr.Open('tmp/cache/BKG_NAS_Peine.xml')
    if ds is None:
        gdaltest.post_reason('could not open dataset')
        return 'fail'

    if ds.GetLayerCount() != 41:
        gdaltest.post_reason('did not get expected layer count')
        return 'fail'

    lyr = ds.GetLayerByName('AX_Wohnplatz')
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if feat.GetField('name') != 'Ziegelei' or geom.ExportToWkt() != 'POINT (3575300 5805100)':
        feat.DumpReadable()
        return 'fail'

    relation_lyr = ds.GetLayerByName('ALKIS_beziehungen')
    feat = relation_lyr.GetNextFeature()
    if feat.GetField('beziehung_von') != 'DENIBKG1000001UG' or \
       feat.GetField('beziehungsart') != 'istTeilVon' or \
       feat.GetField('beziehung_zu') != 'DENIBKG1000000T6':
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test reading a sample NAS file from PostNAS
#

def ogr_nas_2():

    try:
        drv = ogr.GetDriverByName('NAS')
    except:
        drv = None

    if drv is None:
        return 'skip'

    if not gdaltest.download_file('http://trac.wheregroup.com/PostNAS/browser/trunk/demodaten/lverm_geo_rlp/gid-6.0/gm2566-testdaten-gid60-2008-11-11.xml.zip?format=raw', 'gm2566-testdaten-gid60-2008-11-11.xml.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/gm2566-testdaten-gid60-2008-11-11.xml')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'tmp/cache/gm2566-testdaten-gid60-2008-11-11.xml.zip')
            try:
                os.stat('tmp/cache/gm2566-testdaten-gid60-2008-11-11.xml')
            except:
                return 'skip'
        except:
            return 'skip'

    ds = ogr.Open('tmp/cache/gm2566-testdaten-gid60-2008-11-11.xml')
    if ds is None:
        gdaltest.post_reason('could not open dataset')
        return 'fail'

    if ds.GetLayerCount() != 85:
        gdaltest.post_reason('did not get expected layer count')
        print(ds.GetLayerCount())
        return 'fail'

    lyr = ds.GetLayerByName('AX_Flurstueck')

    # Loop until a feature that has a complex geometry including <gml:Arc>
    feat = lyr.GetNextFeature()
    while feat is not None and feat.GetField('identifier') != 'urn:adv:oid:DERP1234000002Iz':
        feat = lyr.GetNextFeature()
    if feat is None:
        return 'fail'

    expected_geom = 'POLYGON ((350821.045 5532031.37,350924.309 5532029.513,350938.493 5532026.622,350951.435 5532021.471,350978.7 5532007.18,351026.406 5531971.088,351032.251 5531951.162,351080.623 5531942.67,351154.886 5531963.718,351207.689 5532019.797,351211.063 5532044.067,351203.83 5532074.034,351165.959 5532114.315,351152.85 5532135.774,351141.396 5532140.355,351110.659 5532137.542,351080.17 5532132.742,351002.887 5532120.75,350925.682 5532108.264,350848.556 5532095.285,350771.515 5532081.814,350769.548 5532071.196,350812.194 5532034.716,350821.045 5532031.37))'
    if ogrtest.check_feature_geometry(feat, expected_geom) != 0:
        geom = feat.GetGeometryRef()
        print(geom)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test that we can open and read empty files successfully.
#

def ogr_nas_3():

    try:
        drv = ogr.GetDriverByName('NAS')
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = ogr.Open('data/empty_nas.xml')
    if ds is None:
        gdaltest.post_reason('could not open dataset')
        return 'fail'

    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('did not get expected layer count')
        print(ds.GetLayerCount())
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test that we can read files with wfs:Delete transactions in them properly.
#

def ogr_nas_4():

    try:
        drv = ogr.GetDriverByName('NAS')
    except:
        drv = None

    if drv is None:
        return 'skip'

    try:
        os.remove( 'data/delete_nas.gfs' )
    except:
        pass

    ds = ogr.Open('data/delete_nas.xml')
    if ds is None:
        gdaltest.post_reason('could not open dataset')
        return 'fail'

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('did not get expected layer count')
        print(ds.GetLayerCount())
        return 'fail'

    del_lyr = ds.GetLayerByName( 'Delete' )

    if del_lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'did not get expected number of features' )
        return 'fail'

    del_lyr.ResetReading()
    feat = del_lyr.GetNextFeature()

    if feat.GetField('context') != 'Delete':
        gdaltest.post_reason( 'did not get expected context' )
        return 'fail'

    if feat.GetField('typeName') != 'AX_Namensnummer':
        gdaltest.post_reason( 'did not get expected typeName' )
        return 'fail'

    if feat.GetField('FeatureId') != 'DENW44AL00000HJU20100730T092847Z':
        gdaltest.post_reason( 'did not get expected FeatureId' )
        return 'fail'

    del_lyr = None
    ds = None

    try:
        os.remove( 'data/delete_nas.gfs' )
    except:
        pass

    return 'success'

###############################################################################
# Test that we can read files with wfsext:Replace transactions properly
#

def ogr_nas_5():

    try:
        drv = ogr.GetDriverByName('NAS')
    except:
        drv = None

    if drv is None:
        return 'skip'

    try:
        os.remove( 'data/replace_nas.gfs' )
    except:
        pass

    ds = ogr.Open('data/replace_nas.xml')
    if ds is None:
        gdaltest.post_reason('could not open dataset')
        return 'fail'

    if ds.GetLayerCount() != 3:
        gdaltest.post_reason('did not get expected layer count')
        print(ds.GetLayerCount())
        return 'fail'

    # Check the delete operation created for the replace
    
    del_lyr = ds.GetLayerByName( 'Delete' )

    if del_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'did not get expected number of features' )
        return 'fail'

    del_lyr.ResetReading()
    feat = del_lyr.GetNextFeature()

    if feat.GetField('context') != 'Replace':
        gdaltest.post_reason( 'did not get expected context' )
        return 'fail'

    if feat.GetField('replacedBy') != 'DENW44AL00003IkM20110429T070635Z':
        gdaltest.post_reason( 'did not get expected replacedBy' )
        return 'fail'

    if feat.GetField('safeToIgnore') != 'false':
        gdaltest.post_reason( 'did not get expected safeToIgnore' )
        return 'fail'

    if feat.GetField('typeName') != 'AX_Flurstueck':
        gdaltest.post_reason( 'did not get expected typeName' )
        return 'fail'

    if feat.GetField('FeatureId') != 'DENW44AL00003IkM20100809T071726Z':
        gdaltest.post_reason( 'did not get expected FeatureId' )
        return 'fail'

    del_lyr = None

    # Check also the feature created by the Replace
    
    lyr = ds.GetLayerByName( 'AX_Flurstueck' )

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'did not get expected number of features' )
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    if feat.GetField('gml_id') != 'DENW44AL00003IkM20110429T070635Z':
        gdaltest.post_reason( 'did not get expected gml_id' )
        return 'fail'

    if feat.GetField('stelle') != 5212:
        gdaltest.post_reason( 'did not get expected stelle' )
        return 'fail'

    lyr = None
    
    ds = None

    try:
        os.remove( 'data/replace_nas.gfs' )
    except:
        pass

    return 'success'

gdaltest_list = [ 
    ogr_nas_1,
    ogr_nas_2,
    ogr_nas_3,
    ogr_nas_4,
    ogr_nas_5 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_nas' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

