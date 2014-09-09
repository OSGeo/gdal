#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR VFK driver functionality.
# Author:   Martin Landa <landa.martin gmail.com>
#
###############################################################################
# Copyright (c) 2009, 2012 Martin Landa <landa.martin gmail.com>
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
 	
import ogrtest
import gdaltest
from osgeo import ogr

###############################################################################
# Open file, check number of layers, get first layer,
# check number of fields and features

def ogr_vfk_1():
    
    try:
       gdaltest.vfk_drv = ogr.GetDriverByName('VFK')
    except:
       gdaltest.vfk_drv = None
    
    if gdaltest.vfk_drv is None:
       return 'skip'
       
    try:
        os.remove('data/bylany.vfk.db')
    except:
        pass
    
    gdaltest.vfk_ds = ogr.Open('data/bylany.vfk')
    
    if gdaltest.vfk_ds is None:
        return 'fail'
    
    if gdaltest.vfk_ds.GetLayerCount() != 61:
        gdaltest.post_reason('expected exactly 61 layers!')
        return 'fail'
    
    gdaltest.vfk_layer_par = gdaltest.vfk_ds.GetLayer(0)
    
    if gdaltest.vfk_layer_par is None:
        gdaltest.post_reason('cannot get first layer')
        return 'fail'
    
    if gdaltest.vfk_layer_par.GetName() != 'PAR':
        gdaltest.post_reason('did not get expected layer name "PAR"')
        return 'fail'
    
    defn = gdaltest.vfk_layer_par.GetLayerDefn()
    if defn.GetFieldCount() != 28:
        gdaltest.post_reason('did not get expected number of fields, got %d' % defn.GetFieldCount())
        return 'fail'
    
    fc = gdaltest.vfk_layer_par.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason('did not get expected feature count, got %d' % fc)
        return 'fail'
    
    return 'success'

###############################################################################
# Read the first feature from layer 'PAR', check envelope

def ogr_vfk_2():
    
    if gdaltest.vfk_drv is None:
       return 'skip'
    
    gdaltest.vfk_layer_par.ResetReading()
    
    feat = gdaltest.vfk_layer_par.GetNextFeature()
    
    if feat.GetFID() != 1:
        gdaltest.post_reason('did not get expected fid for feature 1')
        return 'fail'
    
    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbPolygon:
        gdaltest.post_reason('did not get expected geometry type.')
        return 'fail'
    
    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 2010.5
    
    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason('envelope area not as expected, got %g.' % area)
        return 'fail'
        
    feat.Destroy()
    
    return 'success'

###############################################################################
# Read features from layer 'SOBR', test attribute query

def ogr_vfk_3():
    
    if gdaltest.vfk_drv is None:
       return 'skip'
    
    gdaltest.vfk_layer_sobr = gdaltest.vfk_ds.GetLayer(43)
    
    if gdaltest.vfk_layer_sobr.GetName() != 'SOBR':
        gdaltest.post_reason('did not get expected layer name "SOBR"')
        return 'fail'
    
    gdaltest.vfk_layer_sobr.SetAttributeFilter("CISLO_BODU = '55'")
    
    gdaltest.vfk_layer_sobr.ResetReading()
    
    feat = gdaltest.vfk_layer_sobr.GetNextFeature()
    count = 0
    while feat:
        feat = gdaltest.vfk_layer_sobr.GetNextFeature()
        count += 1
    
    if count != 1:
        gdaltest.post_reason('did not get expected number of features, got %d' % count)
        return 'fail'
    
    return 'success'
    
###############################################################################
# Read features from layer 'SBP', test random access, check length

def ogr_vfk_4():
    
    if gdaltest.vfk_drv is None:
       return 'skip'
    
    gdaltest.vfk_layer_sbp = gdaltest.vfk_ds.GetLayerByName('SBP')
    
    if not gdaltest.vfk_layer_sbp:
        gdaltest.post_reason('did not get expected layer name "SBP"')
        return 'fail'
    
    feat = gdaltest.vfk_layer_sbp.GetFeature(5)
    length = int (feat.geometry().Length())
        
    if length != 10:
        gdaltest.post_reason('did not get expected length, got %d' % length)
        return 'fail'
    
    return 'success'

###############################################################################
# Read features from layer 'HP', check geometry type

def ogr_vfk_5():
    
    if gdaltest.vfk_drv is None:
       return 'skip'
    
    gdaltest.vfk_layer_hp = gdaltest.vfk_ds.GetLayerByName('HP')
    
    if not gdaltest.vfk_layer_hp != 'HP':
        gdaltest.post_reason('did not get expected layer name "HP"')
        return 'fail'
    
    geom_type = gdaltest.vfk_layer_hp.GetGeomType()

    if geom_type != ogr.wkbLineString:
        gdaltest.post_reason('did not get expected geometry type, got %d' % geom_type)
        return 'fail'
    
    return 'success'
 
###############################################################################
# Re-Open file (test .db persistance)

def ogr_vfk_6():

    if gdaltest.vfk_drv is None:
       return 'skip'

    gdaltest.vfk_layer_par = None
    gdaltest.vfk_layer_sobr = None
    gdaltest.vfk_ds = None
    gdaltest.vfk_ds = ogr.Open('data/bylany.vfk')
    
    if gdaltest.vfk_ds is None:
        return 'fail'
    
    if gdaltest.vfk_ds.GetLayerCount() != 61:
        gdaltest.post_reason('expected exactly 61 layers!')
        return 'fail'
    
    gdaltest.vfk_layer_par = gdaltest.vfk_ds.GetLayer(0)
    
    if gdaltest.vfk_layer_par is None:
        gdaltest.post_reason('cannot get first layer')
        return 'fail'
    
    if gdaltest.vfk_layer_par.GetName() != 'PAR':
        gdaltest.post_reason('did not get expected layer name "PAR"')
        return 'fail'
    
    defn = gdaltest.vfk_layer_par.GetLayerDefn()
    if defn.GetFieldCount() != 28:
        gdaltest.post_reason('did not get expected number of fields, got %d' % defn.GetFieldCount())
        return 'fail'
    
    fc = gdaltest.vfk_layer_par.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason('did not get expected feature count, got %d' % fc)
        return 'fail'
    
    return 'success'

###############################################################################
# cleanup

def ogr_vfk_cleanup():

    if gdaltest.vfk_drv is None:
       return 'skip'

    gdaltest.vfk_layer_par = None
    gdaltest.vfk_layer_sobr = None
    gdaltest.vfk_ds = None

    try:
        os.remove('data/bylany.db')
    except:
        pass

    return 'success'

###############################################################################
#

gdaltest_list = [
    ogr_vfk_1,
    ogr_vfk_2,
    ogr_vfk_3,
    ogr_vfk_4,
    ogr_vfk_5,
    ogr_vfk_6,
    ogr_vfk_cleanup ]

if __name__ == '__main__':
    gdaltest.setup_run('ogr_vfk')
    
    gdaltest.run_tests(gdaltest_list)
    
    gdaltest.summarize()
