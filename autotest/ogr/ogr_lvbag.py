#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test LVBAG driver functionality.
# Author:   Laixer B.V., info at laixer dot com
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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



from osgeo import ogr
import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('LVBAG')

###############################################################################
# Basic tests


def test_ogr_lvbag_dataset_lig():

    ds = ogr.Open('data/lvbag/lig.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.TestCapability("foo") == 0
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Ligplaats', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 3
    assert lyr.TestCapability("foo") == 0
    assert lyr.TestCapability("StringsAsUTF8") == 1

    assert 'Amersfoort' in lyr.GetSpatialRef().ExportToWkt()

    assert lyr.GetLayerDefn().GetFieldCount() == 17

    assert lyr.GetLayerDefn().GetGeomFieldCount() == 1

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() == 'namespace' and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef() == 'lokaalID' and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetNameRef() == 'versie' and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetNameRef() == 'status' and \
       lyr.GetLayerDefn().GetFieldDefn(4).GetNameRef() == 'geconstateerd' and \
       lyr.GetLayerDefn().GetFieldDefn(5).GetNameRef() == 'documentdatum' and \
       lyr.GetLayerDefn().GetFieldDefn(6).GetNameRef() == 'documentnummer' and \
       lyr.GetLayerDefn().GetFieldDefn(7).GetNameRef() == 'voorkomenidentificatie' and \
       lyr.GetLayerDefn().GetFieldDefn(8).GetNameRef() == 'beginGeldigheid' and \
       lyr.GetLayerDefn().GetFieldDefn(9).GetNameRef() == 'eindGeldigheid' and \
       lyr.GetLayerDefn().GetFieldDefn(10).GetNameRef() == 'tijdstipRegistratie' and \
       lyr.GetLayerDefn().GetFieldDefn(11).GetNameRef() == 'eindRegistratie' and \
       lyr.GetLayerDefn().GetFieldDefn(12).GetNameRef() == 'tijdstipInactief' and \
       lyr.GetLayerDefn().GetFieldDefn(13).GetNameRef() == 'tijdstipRegistratieLV' and \
       lyr.GetLayerDefn().GetFieldDefn(14).GetNameRef() == 'tijdstipEindRegistratieLV' and \
       lyr.GetLayerDefn().GetFieldDefn(15).GetNameRef() == 'tijdstipInactiefLV' and \
       lyr.GetLayerDefn().GetFieldDefn(16).GetNameRef() == 'tijdstipNietBagLV')

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTInteger and \
       lyr.GetLayerDefn().GetFieldDefn(5).GetType() == ogr.OFTDate and \
       lyr.GetLayerDefn().GetFieldDefn(6).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(7).GetType() == ogr.OFTInteger and \
       lyr.GetLayerDefn().GetFieldDefn(8).GetType() == ogr.OFTDate and \
       lyr.GetLayerDefn().GetFieldDefn(9).GetType() == ogr.OFTDate and \
       lyr.GetLayerDefn().GetFieldDefn(10).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(11).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(12).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(13).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(14).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(15).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(16).GetType() == ogr.OFTDateTime)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'NL.IMBAG.Ligplaats' or \
       feat.GetFieldAsString(1) != '0106020000000003' or \
       feat.GetField(2) != None or \
       feat.GetFieldAsString(3) != 'Plaats aangewezen' or \
       feat.GetFieldAsInteger(4) != 0 or \
       feat.GetFieldAsString(5) != '2009/05/26' or \
       feat.GetFieldAsString(6) != '2009-01000' or \
       feat.GetFieldAsInteger(7) != 1 or \
       feat.GetFieldAsString(8) != '2009/05/26' or \
       feat.GetField(9) != None or \
       feat.GetFieldAsString(10) != '2009/11/06 13:37:22' or \
       feat.GetField(11) != None or \
       feat.GetField(12) != None or \
       feat.GetFieldAsString(13) != '2009/11/06 14:07:51.498' or \
       feat.GetField(14) != None or \
       feat.GetField(15) != None or \
       feat.GetField(16) != None:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert feat is None

def test_ogr_lvbag_dataset_num():

    ds = ogr.Open('data/lvbag/num.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'
    assert ds.GetLayer(1) is None

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Nummeraanduiding', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbUnknown, 'bad layer geometry type'
    assert lyr.GetSpatialRef() is None, 'bad spatial ref'
    assert lyr.GetFeatureCount() == 3
    assert lyr.GetSpatialRef() is None, 'bad spatial ref'
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1

    assert lyr.GetLayerDefn().GetFieldCount() == 22

    feat = lyr.GetNextFeature()
    if feat.GetField('namespace') != 'NL.IMBAG.Nummeraanduiding' or \
       feat.GetField('lokaalID') != '0106200000002798' or \
       feat.GetFieldAsInteger('huisnummer') != 23 or \
       feat.GetField('postcode') != '9403KB' or \
       feat.GetField('typeAdresseerbaarObject') != 'Verblijfsobject' or \
       feat.GetField('status') != 'Naamgeving uitgegeven' or \
       feat.GetFieldAsInteger('geconstateerd') != 0 or \
       feat.GetFieldAsString('documentdatum') != '2009/09/14' or \
       feat.GetFieldAsString('documentnummer') != '2009-BB01570' or \
       feat.GetFieldAsInteger('voorkomenidentificatie') != 1 or \
       feat.GetField('beginGeldigheid') != '2009/09/24' or \
       feat.GetField('tijdstipRegistratie') != '2009/11/06 12:21:37' or \
       feat.GetField('tijdstipRegistratieLV') != '2009/11/06 12:38:46.603':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert feat is None

def test_ogr_lvbag_dataset_opr():

    ds = ogr.Open('data/lvbag/opr.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'OpenbareRuimte', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbUnknown, 'bad layer geometry type'
    assert lyr.GetSpatialRef() is None, 'bad spatial ref'
    assert lyr.GetFeatureCount() == 3
    assert lyr.GetLayerDefn().GetFieldCount() == 19

def test_ogr_lvbag_dataset_pnd():

    ds = ogr.Open('data/lvbag/pnd.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbPolygon25D, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 6
    assert lyr.GetLayerDefn().GetFieldCount() == 18

    sr = lyr.GetSpatialRef()

    assert sr.GetAuthorityName(None) == 'EPSG'
    assert sr.GetAuthorityCode(None) == '28992'

def test_ogr_lvbag_dataset_sta():

    ds = ogr.Open('data/lvbag/sta.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Standplaats', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetLayerDefn().GetFieldCount() == 17

def test_ogr_lvbag_dataset_vbo():

    ds = ogr.Open('data/lvbag/vbo.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Verblijfsobject', 'bad layer name'
    # assert lyr.GetGeomType() == ogr.wkbUnknown, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 3
    assert lyr.GetLayerDefn().GetFieldCount() == 19

def test_ogr_lvbag_dataset_wpl():

    ds = ogr.Open('data/lvbag/wpl.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Woonplaats', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetLayerDefn().GetFieldCount() == 18

    feat = lyr.GetNextFeature()
    if feat.GetField('naam') != 'Assen' or \
       feat.GetField('lokaalID') != '2391':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('naam') != 'Loon' or \
       feat.GetField('lokaalID') != '2392':
        feat.DumpReadable()
        pytest.fail()

def test_ogr_lvbag_read_zip_1():

    ds = ogr.Open('/vsizip/./data/lvbag/archive_pnd.zip/0453PND01052020_000001.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'
    
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() == 4

def test_ogr_lvbag_read_zip_2():

    ds = ogr.Open('/vsizip/./data/lvbag/archive_pnd.zip')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'
    
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() == 10

def test_ogr_lvbag_read_zip_3():

    ds = ogr.Open('/vsizip/./data/lvbag/archive_mixed.zip')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 2, 'bad layer count'
    
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Standplaats', 'bad layer name'
    assert lyr.GetFeatureCount() == 5

    lyr = ds.GetLayer(1)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() == 9

def test_ogr_lvbag_read_errors():

    ds = ogr.Open('data/lvbag/inval_pnd.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'
    
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        assert lyr.GetName() == ''
        assert lyr.GetFeatureCount() == 0

###############################################################################
# Run test_ogrsf


def test_ogr_lvbag_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    import gdaltest
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/lvbag/wpl.xml')

    assert 'INFO' in ret and 'ERROR' not in ret
