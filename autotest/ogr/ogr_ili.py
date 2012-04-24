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
              'Bodenbedeckung__BoFlaechen__Art']
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

    ds = ogr.Open( 'data/ili/enum-test.itf,data/ili/enum-test.ili' )

    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen__Art')
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

    #Nested enum (domain)
    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen__NestedEnum')
    #TODO: should be on domain level: EnumTest__Enum
    if lyr is None:
        gdaltest.post_reason( 'Enumeration layer not available.' )
        return 'fail'

    if lyr.GetFeatureCount() != 4:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()
    while feat:
        #print("Id %d -> %s" % (feat.GetFieldAsInteger("id"), feat.GetFieldAsString(1)) )
        feat = lyr.GetNextFeature()
    #Id 0 -> Enum0
    #Id 0 -> Enum1
    #Id 1 -> Enum2
    #Id 2 -> Enum3

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
# Check arc segmentation

def ogr_interlis_arc1():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )

    length_0_1_deg = 72.7181992353 # Line length with 0.1 degree segments

    #Read Area lines
    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen_Form')
    if lyr.GetFeatureCount() != 4:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    #Get 3rd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.001: #72.7177335946
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if geom.ExportToWkt() != 'MULTILINESTRING ((186.38 206.82 0,186.460486424448106 206.327529546107257 0,186.532365795904212 205.833729416057821 0,186.595616219215486 205.338750026154884 0,186.650218427690277 204.84274215191553 0,186.696155788966905 204.345856882143238 0,186.733414310080178 203.848245572904574 0,186.761982641723563 203.350059801424834 0,186.781852081706546 202.851451319916038 0,186.793016577605215 202.352572009351746 0,186.795472728605944 201.853573833202773 0,186.789219786541395 201.354608791147427 0,186.774259656118232 200.855828872771127 0,186.75059689433715 200.357386011268773 0,186.718238709104583 199.859432037164538 0,186.677194957037244 199.362118632062675 0,186.627478140459601 198.865597282444014 0,186.569103403595591 198.370019233521532 0,186.502088527955578 197.875535443169724 0,186.426453926919834 197.382296535941265 0,186.342222639520543 196.890452757185443 0,186.249420323423806 196.400153927281849 0,186.148075247114093 195.911549396003693 0,186.038218281283434 195.424787997024453 0,185.919882889427811 194.940018002581581 0,185.793105117653909 194.457387078311513 0,185.65792358369913 193.977042238269064 0,185.514379465168304 193.499129800145766 0,185.362516486990415 193.023795340699877 0,185.202380908099769 192.551183651412458 0,185.034021507344988 192.081438694382314 0,184.857489568630456 191.614703558473906 0,184.672838865294779 191.151120415730986 0,184.480125643730986 190.690830478069671 0,184.279408606253213 190.233973954263888 0,184.070748893215466 189.780690007236473 0,183.854210064387644 189.331116711668756 0,183.629858079594698 188.885391011941635 0,183.397761278624529 188.44364868042112 0,183.26 188.19 0,183.157990360411077 188.006024276100675 0,182.910618361498763 187.572651103613168 0,182.655720633794772 187.143661172625173 0,182.393374821616248 186.719185157625333 0,182.123660838038973 186.299352358119819 0,181.846660840555074 185.884290659246403 0,181.562459206047208 185.474126492819352 0,181.271142505086345 185.06898479881707 0,180.972799475561658 184.668988987324269 0,180.667520995650023 184.274260900939993 0,180.355400056133732 183.884920777663154 0,180.036531732074565 183.501087214266903 0,179.711013153852946 183.122877130172981 0,179.378943477581203 182.750405731836764 0,179.040423854899558 182.383786477654411 0,178.695557402164354 182.023131043402287 0,178.344449169037915 181.668549288219367 0,177.987206106489339 181.320149221143225 0,177.623937034216141 180.978036968209295 0,177.254752607496783 180.642316740123988 0,176.879765283484062 180.313090800520911 0,176.499089286949413 179.990459434810589 0,176.112840575489088 179.674520919632386 0,175.721136804202303 179.365371492918626 0,175.324097289852261 179.063105324579453 0,174.921842974521269 178.767814487817873 0,174.514496388770482 178.479588931083327 0,174.102181614316009 178.198516450672486 0,173.685024246232274 177.924682663985692 0,173.26315135469477 177.658170983447064 0,172.836691446272965 177.399062591096254 0,172.405774424786216 177.147436413859566 0,171.970531551733643 176.903369099508211 0,171.531095406310641 176.666934993310434 0,171.087599845024073 176.438206115385384 0,170.64017996091809 176.217252138765019 0,170.188972042423671 176.004140368171022 0,170.18 176.0 0,170.18 176.0 0,140.69 156.63 0))':
        gdaltest.post_reason( 'Ili curve not correctly read' )
        print(geom.ExportToWkt())
        return 'fail'
    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    if len(points) != 80:
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    #Get 4th feature
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    if abs(length-98.0243498288) > 0.0000000001:
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if geom.ExportToWkt() != 'MULTILINESTRING ((186.38 206.82 0,194.26 208.19 0,194.363742877465398 207.560666258887778 0,194.456486566153643 206.929617805490125 0,194.538202815438581 206.297046863278979 0,194.608866733759072 205.663146119491216 0,194.66845679620107 205.028108666434122 0,194.716954851054254 204.392127942667628 0,194.754346125341328 203.755397674081081 0,194.780619229317949 203.118111814882468 0,194.795766159942076 202.48046448851801 0,194.799782303311986 201.842649928540311 0,194.792666436071414 201.204862419443032 0,194.774420725782534 200.567296237479837 0,194.745050730265405 199.930145591485967 0,194.704565395905178 199.293604563720407 0,194.652977054926907 198.657867050746546 0,194.590301421638998 198.023126704369332 0,194.516557587646503 197.389576872647183 0,194.431768016035619 196.757410540996148 0,194.335958534531215 196.126820273404775 0,194.229158327629534 195.497998153777246 0,194.111399927708163 194.87113572742274 0,193.982719205116467 194.246423942708901 0,193.843155357249145 193.624053092897014 0,193.692750896606185 193.004212758177033 0,193.531551637843307 192.387091747919413 0,193.359606683816367 191.772878043162052 0,193.176968410623942 191.161758739349466 0,192.983692451653383 190.553919989341608 0,192.779837680634046 189.949546946710058 0,192.565466193703969 189.348823709338234 0,192.340643290494768 188.751933263343574 0,192.105437454240644 188.159057427338212 0,191.859920330917817 187.570376797045014 0,191.604166707420404 186.986070690286709 0,191.338254488779711 186.406317092363707 0,191.062264674433465 185.831292601838129 0,190.776281333552674 185.261172376740149 0,190.75 185.21 0,190.480391579433388 184.696130081213255 0,190.174685542961015 184.136337832614316 0,189.859256345155728 183.581966149085105 0,189.534200068806825 183.033183897610741 0,189.199615729204936 182.490158242581202 0,188.855605243981131 181.953054594871418 0,188.502273402061718 181.422036561455485 0,188.139727831748502 180.897265895570513 0,187.768078967934287 180.378902447444887 0,187.387440018463224 179.867104115606367 0,186.997926929646695 179.362026798784797 0,186.599658350944793 178.863824348423634 0,186.192755598824732 178.372648521815478 0,185.777342619806575 177.88864893587521 0,185.35354595270789 177.411973021565359 0,184.921494690098939 176.94276597898704 0,184.481320438979651 176.481170733150748 0,184.033157280690972 176.027327890439949 0,183.577141730072384 175.58137569578102 0,183.113412693878161 175.143449990532446 0,182.642111428464915 174.713684171106365 0,182.163381496763719 174.292209148334592 0,181.677368724549325 173.879153307591992 0,181.184221156020271 173.47464246968903 0,180.684089008703126 173.078799852545501 0,180.177124627694894 172.691746033657182 0,179.663482439257081 172.313598913366803 0,179.143318903776049 171.944473678950402 0,178.616792468103654 171.584482769530155 0,178.084063517292776 171.233735841824398 0,177.545294325742475 170.8923397367451 0,177.000649007767692 170.560398446852986 0,176.450293467608361 170.238013084680574 0,175.894395348893454 169.925281851932226 0,175.333123983575007 169.622300009570893 0,174.766650340348065 169.329159848800828 0,174.195146972571933 169.045950662954681 0,174.1 169.0 0,174.1 169.0 0,145.08 149.94 0,140.69 156.63 0))':
        gdaltest.post_reason( 'Ili curve not correctly read' )
        print(geom.ExportToWkt())
        return 'fail'
    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    if len(points) != 81:
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    ds.Destroy()

    #45 deg instead of default (1 deg)
    os.environ['ARC_DEGREES'] = '45'
    #GML: gdal.SetConfigOption('OGR_ARC_STEPSIZE','45')
    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )

    #Read Area lines
    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen_Form')
    #Get 3rd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.001: #72.7173476755
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if geom.ExportToWkt() != 'MULTILINESTRING ((186.38 206.82 0,186.487019781076668 206.152175213279918 0,186.578212845729098 205.482005828528003 0,186.653528162938358 204.809866867975131 0,186.7129235867651 204.136134456013338 0,186.756365879933952 203.461185608719205 0,186.783830732433074 202.785398022878354 0,186.79530277511779 202.109149864628677 0,186.790775588310993 201.432819557841356 0,186.770251705395623 200.756785572357245 0,186.73374261139702 200.081426212197925 0,186.681268736555921 199.407119403869103 0,186.612859444895946 198.73424248487575 0,186.528553017791467 198.063171992566396 0,186.428396632545883 197.394283453425544 0,186.312446335991382 196.72795117293154 0,186.180767013125433 196.064548026097725 0,186.033432350801831 195.404445248814227 0,185.870524796495715 194.748012230106724 0,185.6921355121668 194.095616305429047 0,185.498364323245625 193.447622551104587 0,185.289319662771931 192.804393580032183 0,185.065118510716331 192.166289338770326 0,184.825886328518976 191.533666906113581 0,184.571756988882385 190.906880293273701 0,184.302872700857051 190.286280245777306 0,184.019383930262222 189.672214047191147 0,183.721449315486296 189.065025324784528 0,183.409235578713918 188.465053857237933 0,183.26 188.19 0,183.082917432629188 187.872635384504946 0,182.742677482647878 187.288101419934748 0,182.388706124732494 186.711779064759327 0,182.021201438848038 186.143990825049855 0,181.640369078117828 185.585054431244515 0,181.246422153741491 185.03528266034877 0,180.839581115739151 184.494983160907395 0,180.420073629589353 183.964458280846401 0,179.988134448828873 183.444004898281378 0,179.544005283686204 182.933914255386298 0,179.087934665822331 182.434471795416499 0,178.620177809253846 181.945957002976598 0,178.14099646753715 181.468643247622708 0,177.650658787292798 181.002797630886818 0,177.149439158152632 180.548680836808472 0,176.63761805921294 180.106546986057822 0,176.115481902080546 179.676643493731433 0,175.583322870598636 179.259210930900394 0,175.041438757342803 178.854482889988503 0,174.490132796978202 178.462685854055508 0,173.929713496571594 178.084039070058594 0,173.360494462952857 177.718754426163486 0,172.782794227222752 177.367036333173075 0,172.196936066505032 177.029081610140679 0,171.603247823042864 176.705079374231389 0,171.002061720740443 176.395210934893242 0,170.393714179252783 176.099649692398003 0,170.18 176.0 0,170.18 176.0 0,140.69 156.63 0))':
        gdaltest.post_reason( 'Ili curve not correctly read' )
        print(geom.ExportToWkt())
        return 'fail'
    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    if len(points) != 60:
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    ds.Destroy()

    #0.1 deg instead of default (1 deg)
    os.environ['ARC_DEGREES'] = '0.1'
    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )

    #Read Area lines
    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen_Form')
    #Get 3rd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.0000000001:
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    if len(points) != 755: #80 for 1 deg
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    ds.Destroy()

    #Compare with GML segmentation
    gml = """<gml:Curve xmlns:gml="http://www.opengis.net/gml" srsName="foo">
             <gml:segments>
              <gml:Arc interpolation="circularArc3Points" numArc="1">
               <gml:pos>186.38 206.82</gml:pos>
               <gml:pos>183.26 188.19</gml:pos>
               <gml:pos>170.18 176.00</gml:pos>
              </gml:Arc>
              <gml:LineStringSegment interpolation="linear">
               <gml:pos>170.18 176.00</gml:pos>
               <gml:pos>140.69 156.63</gml:pos>
              </gml:LineStringSegment>
             </gml:segments>
            </gml:Curve>"""
    gdal.SetConfigOption('OGR_ARC_STEPSIZE','45')
    geom = ogr.CreateGeometryFromGML( gml )
    gdal.SetConfigOption('OGR_ARC_STEPSIZE',None)
    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.7: #72.0516477473
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if geom.ExportToWkt() != 'LINESTRING (186.38 206.82,183.26 188.19,170.18 176.0,140.69 156.63)':
        gdaltest.post_reason( '<gml:Curve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'
    points = geom.GetPoints()
    if len(points) != 4:
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    gdal.SetConfigOption('OGR_ARC_STEPSIZE','1')
    geom = ogr.CreateGeometryFromGML( gml )
    gdal.SetConfigOption('OGR_ARC_STEPSIZE',None)

    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.01: #72.710798961
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if geom.ExportToWkt() != 'LINESTRING (186.38 206.82,186.650218427690248 204.84274215191553,186.781852081706546 202.851451319916009,186.774259656118232 200.85582887277107,186.627478140459601 198.865597282443957,186.342222639520514 196.890452757185358,185.919882889427782 194.940018002581496,185.362516486990387 193.023795340699792,184.67283886529475 191.151120415730901,183.854210064387615 189.331116711668642,183.26 188.19,182.238255485026571 186.475764093656664,181.099420836346837 184.836977274255958,179.849044341893887 183.281623546451669,178.493217710261348 181.817280437262127,177.038546392511279 180.451082079119971,175.492117401080719 179.189684453096334,173.861464782569385 178.039232961629978,172.154532912622102 177.005332488744273,170.37963779172901 176.093020093615877,170.18 176.0,140.69 156.63)':
        gdaltest.post_reason( '<gml:Curve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'
    points = geom.GetPoints()
    if len(points) != 22:
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

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
    ogr_interlis_arc1,
    ogr_interlis_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ili' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

