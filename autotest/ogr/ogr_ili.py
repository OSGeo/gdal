#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Interlis driver testing.
# Author:   Pirmin Kalberer <pka(at)sourcepole.ch>
# 
###############################################################################
# Copyright (c) 2012, Pirmin Kalberer <pka(at)sourcepole.ch>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import shutil
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr, osr, gdal

def cpl_debug_on():
    gdaltest.cpl_debug = gdal.GetConfigOption('CPL_DEBUG')
    gdal.SetConfigOption('CPL_DEBUG', 'ON')

def cpl_debug_reset():
    gdal.SetConfigOption('CPL_DEBUG', gdaltest.cpl_debug)

###############################################################################
# Open Driver

def ogr_interlis1_1():

    gdaltest.have_ili_reader = 0
    try:
        driver = ogr.GetDriverByName( 'Interlis 1' )
        if driver is None:
            return 'skip'
    except:
        return 'skip'

    # Check ili2c.jar
    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )
    if gdal.GetLastErrorMsg().find('iom_compileIli failed.') != -1:
        gdaltest.post_reason( 'skipping test: ili2c.jar not found in PATH' )
        return 'skip'

    ds.Destroy()    
    gdaltest.have_ili_reader = 1
    
    return 'success'

###############################################################################
# Check that Ili1 point layer is properly read.

def ogr_interlis1_2():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )
    layers = ['Bodenbedeckung__BoFlaechen',
              'Bodenbedeckung__BoFlaechen_Form',
              'Bodenbedeckung__BoFlaechen__Areas',
              'Bodenbedeckung__Strasse',
              'Bodenbedeckung__Gebaeude',
              'BoFlaechen__Art']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen')

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    #Get 2nd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    field_values = [20, 1, 168.27, 170.85, 'POINT (168.27 170.85)']

    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetCoordinateDimension() != 2:
        gdaltest.post_reason( 'dimension wrong.' )
        return 'fail'

    if geom.GetGeometryName() != 'POINT':
        gdaltest.post_reason( 'Geometry of wrong type.' )
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Ili1 FORMAT DEFAULT test.

def ogr_interlis1_3():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-default.itf,data/ili/format-default.ili' )

    layers = ['FormatTests__FormatTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('FormatTests__FormatTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = [0, 'aa bb', 'cc^dd', '', 1]

    if feat.GetFieldCount() != len(field_values):
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Ili1 FORMAT test.

def ogr_interlis1_4():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-test.itf,data/ili/format-test.ili' )

    layers = ['FormatTests__FormatTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('FormatTests__FormatTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = [0, 'aa_bb', 'cc dd', '', 1]

    if feat.GetFieldCount() != len(field_values):
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Write Ili1 transfer file.

def ogr_interlis1_5():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-default.itf,data/ili/format-default.ili' )
    lyr = ds.GetLayerByName('FormatTests__FormatTable')
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName( 'Interlis 1' )
    dst_ds = driver.CreateDataSource( 'tmp/interlis1_5.itf' )

    dst_lyr = dst_ds.CreateLayer( 'FormatTests__FormatTable' )

    layer_defn = lyr.GetLayerDefn()
    for i in range( layer_defn.GetFieldCount() ):
        dst_lyr.CreateField( layer_defn.GetFieldDefn( i ) )
    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )
    dst_feat.SetFrom( feat )
    dst_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    ds.Destroy()

    return 'success'

###############################################################################
# Write Ili1 transfer file using a model.

def ogr_interlis1_6():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-default.itf,data/ili/format-default.ili' )
    lyr = ds.GetLayerByName('FormatTests__FormatTable')
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName( 'Interlis 1' )
    dst_ds = driver.CreateDataSource( 'tmp/interlis1_6.itf,data/ili/format-default.ili' )

    dst_lyr = dst_ds.CreateLayer( 'test' )

    layer_defn = lyr.GetLayerDefn()
    for i in range( layer_defn.GetFieldCount() ):
        dst_lyr.CreateField( layer_defn.GetFieldDefn( i ) )
    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )
    dst_feat.SetFrom( feat )
    dst_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    ds.Destroy()

    return 'success'

###############################################################################
# Ili1 character encding test.

def ogr_interlis1_7():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/encoding-test.itf,data/ili/format-default.ili' )

    layers = ['FormatTests__FormatTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('FormatTests__FormatTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    #Interlis 1 Encoding is ISO 8859-1 (Latin1)
    #Pyton source code is UTF-8 encoded
    field_values = [0, 'äöü', 'ÄÖÜ', '', 1]

    if feat.GetFieldCount() != len(field_values):
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    #Write back
    driver = ogr.GetDriverByName( 'Interlis 1' )
    dst_ds = driver.CreateDataSource( 'tmp/interlis1_7.itf' )

    dst_lyr = dst_ds.CreateLayer( 'FormatTests__FormatTable' )

    layer_defn = lyr.GetLayerDefn()
    for i in range( layer_defn.GetFieldCount() ):
        dst_lyr.CreateField( layer_defn.GetFieldDefn( i ) )
    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )
    dst_feat.SetFrom( feat )
    dst_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    ds.Destroy()

    return 'success'

###############################################################################
# Ili1 enumeration test.

def ogr_interlis1_8():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )

    lyr = ds.GetLayerByName('BoFlaechen__Art')
    if lyr is None:
        gdaltest.post_reason( 'Enumeration layer not available.' )
        return 'fail'

    if lyr.GetFeatureCount() != 6:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()
    while feat and feat.GetFieldAsInteger("id") != 0:
      feat = lyr.GetNextFeature()

    field_values = [0, 'Gebaeude', -1]
    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          print str(field_values[i])
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Ili1 VRT rename

def ogr_interlis1_9():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel-rename.vrt' )
    layers = ['BoGebaeude']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('BoGebaeude')

    if lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() != 'AssekuranzNr':
        gdaltest.post_reason( 'Wrong field name: ' +  lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef())
        return 'fail'

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = ['958', 10, 'POINT (148.41 175.96)']

    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Ili1 VRT join

def ogr_interlis1_10():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel-join.vrt' )
    layers = ['BoFlaechenJoined']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('BoFlaechenJoined')

    #TODO: Test that attribute filters are passed through to an underlying layer.
    #lyr.SetAttributeFilter( 'other = "Second"' )
    #lyr.ResetReading()

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = ['10', 0, 148.2, 183.48,'Gebaeude',  -1]

    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Reading Ili2 without model

def ogr_interlis2_1():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/RoadsExdm2ien.xml' )
    if ds is None:
        return 'fail'

    layers = ['RoadsExdm2ben.Roads.LandCover',
              'RoadsExdm2ben.Roads.Street',
              'RoadsExdm2ien.RoadsExtended.StreetAxis',
              'RoadsExdm2ben.Roads.StreetNamePosition',
              'RoadsExdm2ien.RoadsExtended.RoadSign']
    #ILI 2.2 Example
    #layers = ['RoadsExdm2ben_10.Roads.LandCover',
    #          'RoadsExdm2ben_10.Roads.Street',
    #          'RoadsExdm2ien_10.RoadsExtended.StreetAxis',
    #          'RoadsExdm2ben_10.Roads.StreetNamePosition',
    #          'RoadsExdm2ien_10.RoadsExtended.RoadSign']

    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Reading Ili2

def ogr_interlis2_2():

    if not gdaltest.have_ili_reader:
        return 'skip'

    cpl_debug_on()
    ds = ogr.Open( 'data/ili/RoadsExdm2ien.xml,data/ili/RoadsExdm2ben.ili,data/ili/RoadsExdm2ien.ili' )
    if ds is None:
        return 'fail'

    if ds.GetLayerCount() != 4:
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    layers = ['Bodenbedeckung__BoFlaechen',
              'Bodenbedeckung__BoFlaechen_Form',
              'Bodenbedeckung__BoFlaechen__Areas'
              'Bodenbedeckung__Gebaeude']
    for i in range(ds.GetLayerCount()):
      if ds.GetLayer(i).GetName() != layers[i]:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'
    ds.Destroy()

    return 'success'



###############################################################################
# 

def ogr_interlis_cleanup():

    if not gdaltest.have_ili_reader:
        return 'skip'

    return 'success'

gdaltest_list = [ 
    ogr_interlis1_1,
    ogr_interlis1_2,
    ogr_interlis1_3,
    ogr_interlis1_4,
    ogr_interlis1_5,
    ogr_interlis1_6,
    ogr_interlis1_7,
    ogr_interlis1_8,
    ogr_interlis1_9,
    ogr_interlis1_10,
    ogr_interlis2_1,
    #ogr_interlis2_2,
    ogr_interlis_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ili' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

