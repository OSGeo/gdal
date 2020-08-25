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
import ogrtest
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

    assert lyr.GetLayerDefn().GetFieldCount() == 19

    assert lyr.GetLayerDefn().GetGeomFieldCount() == 1

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef().lower() == 'nummeraanduidingref' and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef().lower() == 'lvid' and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetNameRef().lower() == 'namespace' and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetNameRef().lower() == 'lokaalid' and \
       lyr.GetLayerDefn().GetFieldDefn(4).GetNameRef().lower() == 'versie' and \
       lyr.GetLayerDefn().GetFieldDefn(5).GetNameRef().lower() == 'status' and \
       lyr.GetLayerDefn().GetFieldDefn(6).GetNameRef().lower() == 'geconstateerd' and \
       lyr.GetLayerDefn().GetFieldDefn(7).GetNameRef().lower() == 'documentdatum' and \
       lyr.GetLayerDefn().GetFieldDefn(8).GetNameRef().lower() == 'documentnummer' and \
       lyr.GetLayerDefn().GetFieldDefn(9).GetNameRef().lower() == 'voorkomenidentificatie' and \
       lyr.GetLayerDefn().GetFieldDefn(10).GetNameRef().lower() == 'begingeldigheid' and \
       lyr.GetLayerDefn().GetFieldDefn(11).GetNameRef().lower() == 'eindgeldigheid' and \
       lyr.GetLayerDefn().GetFieldDefn(12).GetNameRef().lower() == 'tijdstipregistratie' and \
       lyr.GetLayerDefn().GetFieldDefn(13).GetNameRef().lower() == 'eindregistratie' and \
       lyr.GetLayerDefn().GetFieldDefn(14).GetNameRef().lower() == 'tijdstipinactief' and \
       lyr.GetLayerDefn().GetFieldDefn(15).GetNameRef().lower() == 'tijdstipregistratielv' and \
       lyr.GetLayerDefn().GetFieldDefn(16).GetNameRef().lower() == 'tijdstipeindregistratielv' and \
       lyr.GetLayerDefn().GetFieldDefn(17).GetNameRef().lower() == 'tijdstipinactieflv' and \
       lyr.GetLayerDefn().GetFieldDefn(18).GetNameRef().lower() == 'tijdstipnietbaglv')

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(5).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(6).GetType() == ogr.OFTInteger and \
       lyr.GetLayerDefn().GetFieldDefn(7).GetType() == ogr.OFTDate and \
       lyr.GetLayerDefn().GetFieldDefn(8).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(9).GetType() == ogr.OFTInteger and \
       lyr.GetLayerDefn().GetFieldDefn(10).GetType() == ogr.OFTDate and \
       lyr.GetLayerDefn().GetFieldDefn(11).GetType() == ogr.OFTDate and \
       lyr.GetLayerDefn().GetFieldDefn(12).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(13).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(14).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(15).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(16).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(17).GetType() == ogr.OFTDateTime and \
       lyr.GetLayerDefn().GetFieldDefn(18).GetType() == ogr.OFTDateTime)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'NL.IMBAG.NUMMERAANDUIDING.0106200000005333' or \
       feat.GetFieldAsString(1) != 'NL.IMBAG.LIGPLAATS.0106020000000003' or \
       feat.GetFieldAsString(2) != 'NL.IMBAG.Ligplaats' or \
       feat.GetFieldAsString(3) != '0106020000000003' or \
       feat.GetField(4) != None or \
       feat.GetFieldAsString(5) != 'Plaats aangewezen' or \
       feat.GetFieldAsInteger(6) != 0 or \
       feat.GetFieldAsString(7) != '2009/05/26' or \
       feat.GetFieldAsString(8) != '2009-01000' or \
       feat.GetFieldAsInteger(9) != 1 or \
       feat.GetFieldAsString(10) != '2009/05/26' or \
       feat.GetField(11) != None or \
       feat.GetFieldAsString(12) != '2009/11/06 13:37:22' or \
       feat.GetField(13) != None or \
       feat.GetField(14) != None or \
       feat.GetFieldAsString(15) != '2009/11/06 14:07:51.498' or \
       feat.GetField(16) != None or \
       feat.GetField(17) != None or \
       feat.GetField(18) != None:
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

    assert lyr.GetLayerDefn().GetFieldCount() == 24

    feat = lyr.GetNextFeature()
    if feat.GetField('namespace') != 'NL.IMBAG.Nummeraanduiding' or \
       feat.GetField('lokaalID') != '0106200000002798' or \
       feat.GetField('lvID') != 'NL.IMBAG.NUMMERAANDUIDING.0106200000002798' or \
       feat.GetFieldAsInteger('huisnummer') != 23 or \
       feat.GetField('postcode') != '9403KB' or \
       feat.GetField('typeAdresseerbaarObject') != 'Verblijfsobject' or \
       feat.GetField('openbareruimteRef') != 'NL.IMBAG.OPENBARERUIMTE.0106300000002560' or \
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
    assert lyr.GetLayerDefn().GetFieldCount() == 21

def test_ogr_lvbag_dataset_pnd():

    ds = ogr.Open('data/lvbag/pnd.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 6
    assert lyr.GetLayerDefn().GetFieldCount() == 19

    sr = lyr.GetSpatialRef()

    assert sr.GetAuthorityName(None) == 'EPSG'
    assert sr.GetAuthorityCode(None) == '28992'

    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != '2009/01/01':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != '2007/01/01':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != '1975/01/01':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != '2001/01/01':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    assert feat is None

def test_ogr_lvbag_dataset_sta():

    ds = ogr.Open('data/lvbag/sta.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Standplaats', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetLayerDefn().GetFieldCount() == 19

def test_ogr_lvbag_dataset_vbo():

    ds = ogr.Open('data/lvbag/vbo.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Verblijfsobject', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbPoint, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 3
    assert lyr.GetLayerDefn().GetFieldCount() == 22

def test_ogr_lvbag_dataset_wpl():

    ds = ogr.Open('data/lvbag/wpl.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Woonplaats', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetLayerDefn().GetFieldCount() == 19

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

def test_ogr_lvbag_invalid_polygon():

    pytest.skip()

    if not ogrtest.have_geos() and not ogrtest.have_sfcgal():
        pytest.skip()

    ds = ogr.Open('data/lvbag/inval_polygon.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'
    
    lyr = ds.GetLayer(0)
    
    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).IsValid()

    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).IsValid()

    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).IsValid()

    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).IsValid()

    feat = lyr.GetNextFeature()
    assert feat is None

def test_ogr_lvbag_read_errors():

    ds = ogr.Open('data/lvbag/inval_pnd.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'
    
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        assert lyr.GetName() == ''
        assert lyr.GetFeatureCount() == 0

def test_ogr_lvbag_fix_lokaalid():

    ds = ogr.Open('data/lvbag/pnd2.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'
    
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() == 1

    feat = lyr.GetNextFeature()
    assert len(feat.GetField('lokaalID')) == 16

###############################################################################
# Run test_ogrsf


def test_ogr_lvbag_test_ogrsf_wpl():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    import gdaltest
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/lvbag/wpl.xml')

    assert 'INFO' in ret and 'ERROR' not in ret

def test_ogr_lvbag_test_ogrsf_pnd():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    import gdaltest
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/lvbag/pnd.xml')

    assert 'INFO' in ret and 'ERROR' not in ret

def test_ogr_lvbag_test_ogrsf_num():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    import gdaltest
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/lvbag/num.xml')

    assert 'INFO' in ret and 'ERROR' not in ret
