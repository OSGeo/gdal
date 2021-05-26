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



from osgeo import ogr, gdal
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

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef().lower() == 'hoofdadresnummeraanduidingref' and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef().lower() == 'nevenadresnummeraanduidingref' and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetNameRef().lower() == 'identificatie' and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetNameRef().lower() == 'status' and \
       lyr.GetLayerDefn().GetFieldDefn(4).GetNameRef().lower() == 'geconstateerd' and \
       lyr.GetLayerDefn().GetFieldDefn(5).GetNameRef().lower() == 'documentdatum' and \
       lyr.GetLayerDefn().GetFieldDefn(6).GetNameRef().lower() == 'documentnummer' and \
       lyr.GetLayerDefn().GetFieldDefn(7).GetNameRef().lower() == 'voorkomenidentificatie' and \
       lyr.GetLayerDefn().GetFieldDefn(8).GetNameRef().lower() == 'begingeldigheid' and \
       lyr.GetLayerDefn().GetFieldDefn(9).GetNameRef().lower() == 'eindgeldigheid' and \
       lyr.GetLayerDefn().GetFieldDefn(10).GetNameRef().lower() == 'tijdstipregistratie' and \
       lyr.GetLayerDefn().GetFieldDefn(11).GetNameRef().lower() == 'eindregistratie' and \
       lyr.GetLayerDefn().GetFieldDefn(12).GetNameRef().lower() == 'tijdstipinactief' and \
       lyr.GetLayerDefn().GetFieldDefn(13).GetNameRef().lower() == 'tijdstipregistratielv' and \
       lyr.GetLayerDefn().GetFieldDefn(14).GetNameRef().lower() == 'tijdstipeindregistratielv' and \
       lyr.GetLayerDefn().GetFieldDefn(15).GetNameRef().lower() == 'tijdstipinactieflv' and \
       lyr.GetLayerDefn().GetFieldDefn(16).GetNameRef().lower() == 'tijdstipnietbaglv')

    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTStringList and \
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
    if feat.GetFieldAsString(0) != 'NL.IMBAG.Nummeraanduiding.0106200000005333' or \
       feat.GetFieldAsString(1) != '' or \
       feat.GetFieldAsString(2) != 'NL.IMBAG.Ligplaats.0106020000000003' or \
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
    # This next line is not a duplicate. When the features are counted the SRS could have changed
    assert lyr.GetSpatialRef() is None, 'bad spatial ref'
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1

    assert lyr.GetLayerDefn().GetFieldCount() == 21

    feat = lyr.GetNextFeature()
    if feat.GetField('identificatie') != 'NL.IMBAG.Nummeraanduiding.0106200000002798' or \
       feat.GetFieldAsInteger('huisnummer') != 23 or \
       feat.GetField('postcode') != '9403KB' or \
       feat.GetField('typeAdresseerbaarObject') != 'Verblijfsobject' or \
       feat.GetField('openbareruimteRef') != 'NL.IMBAG.Openbareruimte.0106300000002560' or \
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
    assert lyr.GetName() == 'Openbareruimte', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbUnknown, 'bad layer geometry type'
    assert lyr.GetSpatialRef() is None, 'bad spatial ref'
    assert lyr.GetFeatureCount() == 3
    assert lyr.GetLayerDefn().GetFieldCount() == 18

def test_ogr_lvbag_dataset_pnd():

    ds = ogr.Open('data/lvbag/pnd.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 6
    assert lyr.GetLayerDefn().GetFieldCount() == 16

    sr = lyr.GetSpatialRef()

    assert sr.GetAuthorityName(None) == 'EPSG'
    assert sr.GetAuthorityCode(None) == '28992'

    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != 2009:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != 2007:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != 1975:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('oorspronkelijkBouwjaar') != 2001:
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
    assert lyr.GetLayerDefn().GetFieldCount() == 17

def test_ogr_lvbag_dataset_vbo():

    ds = ogr.Open('data/lvbag/vbo.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Verblijfsobject', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbPoint, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 3
    assert lyr.GetLayerDefn().GetFieldCount() == 20

def test_ogr_lvbag_dataset_wpl():

    ds = ogr.Open('data/lvbag/wpl.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Woonplaats', 'bad layer name'
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon, 'bad layer geometry type'
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetLayerDefn().GetFieldCount() == 16

    feat = lyr.GetNextFeature()
    if feat.GetField('naam') != 'Assen' or \
       feat.GetField('identificatie') != 'NL.IMBAG.Woonplaats.2391':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('naam') != 'Loon' or \
       feat.GetField('identificatie') != 'NL.IMBAG.Woonplaats.2392':
        feat.DumpReadable()
        pytest.fail()

def test_ogr_lvbag_read_zip_1():

    ds = ogr.Open('/vsizip/./data/lvbag/archive_pnd.zip/9999PND08102020-000001.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() == 2

def test_ogr_lvbag_read_zip_2():

    ds = ogr.Open('/vsizip/./data/lvbag/archive_pnd.zip')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() == 4

def test_ogr_lvbag_read_zip_3():

    ds = ogr.Open('/vsizip/./data/lvbag/archive_mixed.zip')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 2, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Standplaats', 'bad layer name'
    assert lyr.GetFeatureCount() > 0

    lyr = ds.GetLayer(1)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() > 0

def test_ogr_lvbag_read_zip_4():

    ds = ogr.Open('/vsizip/./data/lvbag/archive_single.zip')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Woonplaats', 'bad layer name'
    assert lyr.GetFeatureCount() > 0

def test_ogr_lvbag_fix_invalid_polygon():

    _test = ogr.CreateGeometryFromWkt('POLYGON ((0 0,1 1,0 1,1 0,0 0))')
    if _test.MakeValid() is None:
        pytest.skip("MakeValid() not available")

    ds = gdal.OpenEx('data/lvbag/inval_polygon.xml', gdal.OF_VECTOR, open_options=['AUTOCORRECT_INVALID_DATA=YES'])
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

def test_ogr_lvbag_fix_invalid_polygon_to_polygon():

    _test = ogr.CreateGeometryFromWkt('POLYGON ((0 0,1 1,0 1,1 0,0 0))')
    if _test.MakeValid() is None:
        pytest.skip("MakeValid() not available")

    ds = gdal.OpenEx('data/lvbag/inval_polygon2.xml', gdal.OF_VECTOR, open_options=['AUTOCORRECT_INVALID_DATA=YES'])
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).GetGeometryType() == ogr.wkbPolygon

    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).GetGeometryType() == ogr.wkbPolygon

    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).GetGeometryType() == ogr.wkbPolygon

    feat = lyr.GetNextFeature()
    assert feat.GetGeomFieldRef(0).GetGeometryType() == ogr.wkbPolygon

def test_ogr_lvbag_read_errors():

    ds = ogr.Open('data/lvbag/inval_pnd.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        assert lyr.GetName() == ''
        assert lyr.GetFeatureCount() == 0

def test_ogr_lvbag_fix_identificatie():

    ds = ogr.Open('data/lvbag/pnd2.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Pand', 'bad layer name'
    assert lyr.GetFeatureCount() == 1

    feat = lyr.GetNextFeature()
    assert feat.GetField('identificatie') == 'NL.IMBAG.Pand.0571100000003518'

def test_ogr_lvbag_old_schema():

    ds = ogr.Open('data/lvbag/lig_old.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 0, 'bad layer count'
    ds = None
    gdal.Unlink('data/lvbag/lig_old.gfs')

def test_ogr_lvbag_stringlist_feat():

    ds = ogr.Open('data/lvbag/vbo2.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetField('gebruiksdoel') == ['woonfunctie', 'gezondheidszorgfunctie'], 'expecting two items'

def test_ogr_lvbag_secondary_address():

    ds = ogr.Open('data/lvbag/vbo3.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert (lyr.GetLayerDefn().GetFieldDefn(2).GetNameRef().lower() == 'hoofdadresnummeraanduidingref' and \
       lyr.GetLayerDefn().GetFieldDefn(3).GetNameRef().lower() == 'nevenadresnummeraanduidingref')

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString(2) == 'NL.IMBAG.Nummeraanduiding.0518200000692257', 'bad hoofdadres'
    assert feat.GetField(3) == ['NL.IMBAG.Nummeraanduiding.0518200000692258', 'NL.IMBAG.Nummeraanduiding.0518200000692259', 'NL.IMBAG.Nummeraanduiding.0518200000692260'], 'bad nevenadres'
    assert feat.GetFieldAsString(5) == 'NL.IMBAG.Verblijfsobject.0518010000692261', 'bad identifier'

def test_ogr_lvbag_secondary_pandref():

    ds = ogr.Open('data/lvbag/vbo4.xml')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(4).GetNameRef().lower() == 'pandref'

    feat = lyr.GetNextFeature()
    assert feat.GetField(4) == ['NL.IMBAG.Pand.0048100000002999', 'NL.IMBAG.Pand.1950100000100293'], 'bad nevenadres'

def test_ogr_lvbag_file_extension():

    ds = ogr.Open('data/lvbag/file4.vbo')
    assert ds is not None, 'cannot open dataset'
    assert ds.GetLayerCount() == 1, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(4).GetNameRef().lower() == 'pandref'

    feat = lyr.GetNextFeature()
    assert feat.GetField(4) == ['NL.IMBAG.Pand.0048100000002999', 'NL.IMBAG.Pand.1950100000100293'], 'bad nevenadres'

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
