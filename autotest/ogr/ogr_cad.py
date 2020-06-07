#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
################################################################################
#  Project: OGR CAD Driver
#  Purpose: Tests OGR CAD Driver capabilities
#  Author: Alexandr Borzykh, mush3d at gmail.com
#  Author: Dmitry Baryshnikov, polimax@mail.ru
#  Language: Python
################################################################################
#  The MIT License (MIT)
#
#  Copyright (c) 2016 Alexandr Borzykh
#  Copyright (c) 2016-2019, NextGIS <info@nextgis.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
################################################################################


import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
import pytest


pytestmark = pytest.mark.require_driver('CAD')

###############################################################################
# Check driver properly opens simple file, reads correct feature (ellipse).


def test_ogr_cad_2():

    ds = gdal.OpenEx('data/cad/ellipse_r2000.dwg', allowed_drivers=['CAD'])

    assert ds is not None

    assert ds.GetLayerCount() == 1, 'expected exactly one layer.'

    layer = ds.GetLayer(0)

    assert layer.GetName() == '0', \
        'layer name is expected to be default = 0.'

    defn = layer.GetLayerDefn()
    assert defn.GetFieldCount() == 5, \
        ('did not get expected number of fields in defn. got %d'
                             % defn.GetFieldCount())

    fc = layer.GetFeatureCount()
    assert fc == 1, ('did not get expected feature count, got %d' % fc)

    layer.ResetReading()

    feat = layer.GetNextFeature()

    assert feat is not None, 'cad feature 0 get failed.'

    assert feat.cadgeom_type == 'CADEllipse', \
        ('cad geometry type is wrong. Expected CADEllipse, got: %s'
                             % feat.cadgeom_type)

    assert feat.GetFID() == 0, 'did not get expected FID for feature 0.'

    assert feat.thickness == 0, ('did not get expected thickness. expected 0, got: %f'
                             % feat.thickness)

    assert feat.extentity_data is None, \
        'expected feature ExtendedEntityData to be null.'

    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    assert feat.GetStyleString() == expected_style, \
        ('got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                             % (feat.GetStyleString(), expected_style))

    geom = feat.GetGeometryRef()
    assert geom is not None, 'cad geometry is None.'

    assert geom.GetGeometryType() == ogr.wkbLineString25D, \
        'did not get expected geometry type.'

    assert geom.GetPointCount() > 2, 'cad geometry is invalid'

###############################################################################
# Check proper read of 3 layers (one circle on each) with different parameters.


def test_ogr_cad_3():

    ds = gdal.OpenEx('data/cad/triple_circles_r2000.dwg', allowed_drivers=['CAD'])

    assert ds is not None

    assert ds.GetLayerCount() == 3, 'expected 3 layers.'

    # test first layer and circle
    layer = ds.GetLayer(0)

    assert layer.GetName() == '0', \
        'layer name is expected to be default = 0.'

    defn = layer.GetLayerDefn()
    assert defn.GetFieldCount() == 5, \
        ('did not get expected number of fields in defn. got %d'
                             % defn.GetFieldCount())

    fc = layer.GetFeatureCount()
    assert fc == 1, ('did not get expected feature count, got %d' % fc)

    layer.ResetReading()

    feat = layer.GetNextFeature()

    assert feat.cadgeom_type == 'CADCircle', \
        ('cad geometry type is wrong. Expected CADCircle, got: %s'
                             % feat.cadgeom_type)

    assert feat.thickness == 1.2, \
        ('did not get expected thickness. expected 1.2, got: %f'
                             % feat.thickness)

    assert feat.extentity_data is None, \
        'expected feature ExtendedEntityData to be null.'

    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    assert feat.GetStyleString() == expected_style, \
        ('Got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                             % (feat.GetStyleString(), expected_style))

    geom = feat.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbCircularStringZ, \
        'did not get expected geometry type.'

    # test second layer and circle
    layer = ds.GetLayer(1)

    assert layer.GetName() == '1', 'layer name is expected to be 1.'

    defn = layer.GetLayerDefn()
    assert defn.GetFieldCount() == 5, \
        ('did not get expected number of fields in defn. got %d'
                             % defn.GetFieldCount())

    fc = layer.GetFeatureCount()
    assert fc == 1, ('did not get expected feature count, got %d' % fc)

    layer.ResetReading()

    feat = layer.GetNextFeature()

    assert feat.cadgeom_type == 'CADCircle', \
        ('cad geometry type is wrong. Expected CADCircle, got: %s'
                             % feat.cadgeom_type)

    assert feat.thickness == 0.8, \
        ('did not get expected thickness. expected 0.8, got: %f'
                             % feat.thickness)

    assert feat.extentity_data is None, \
        'expected feature ExtendedEntityData to be null.'

    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    assert feat.GetStyleString() == expected_style, \
        ('Got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                             % (feat.GetStyleString(), expected_style))

    geom = feat.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbCircularStringZ, \
        'did not get expected geometry type.'

    # test third layer and circle
    layer = ds.GetLayer(2)

    assert layer.GetName() == '2', 'layer name is expected to be 2.'

    defn = layer.GetLayerDefn()
    assert defn.GetFieldCount() == 5, \
        ('did not get expected number of fields in defn. got %d'
                             % defn.GetFieldCount())

    fc = layer.GetFeatureCount()
    assert fc == 1, ('did not get expected feature count, got %d' % fc)

    layer.ResetReading()

    feat = layer.GetNextFeature()

    assert feat.cadgeom_type == 'CADCircle', \
        ('cad geometry type is wrong. Expected CADCircle, got: %s'
                             % feat.cadgeom_type)

    assert feat.thickness == 1.8, \
        ('did not get expected thickness. expected 1.8, got: %f'
                             % feat.thickness)

    assert feat.extentity_data is None, \
        'expected feature ExtendedEntityData to be null.'

    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    assert feat.GetStyleString() == expected_style, \
        ('Got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                             % (feat.GetStyleString(), expected_style))

    geom = feat.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbCircularStringZ, \
        'did not get expected geometry type.'


###############################################################################
# Check reading of a single point.


def test_ogr_cad_4():

    ds = gdal.OpenEx('data/cad/point2d_r2000.dwg', allowed_drivers=['CAD'])

    assert ds.GetLayerCount() == 1, 'expected exactly one layer.'

    layer = ds.GetLayer(0)

    assert layer.GetFeatureCount() == 1, 'expected exactly one feature.'

    feat = layer.GetNextFeature()

    assert not ogrtest.check_feature_geometry(feat, 'POINT (50 50 0)'), \
        'got feature which does not fit expectations.'

###############################################################################
# Check reading of a simple line.


def test_ogr_cad_5():

    ds = gdal.OpenEx('data/cad/line_r2000.dwg', allowed_drivers=['CAD'])

    assert ds.GetLayerCount() == 1, 'expected exactly one layer.'

    layer = ds.GetLayer(0)

    assert layer.GetFeatureCount() == 1, 'expected exactly one feature.'

    feat = layer.GetNextFeature()

    assert not ogrtest.check_feature_geometry(feat, 'LINESTRING (50 50 0,100 100 0)'), \
        'got feature which does not fit expectations.'

###############################################################################
# Check reading of a text (point with attached 'text' attribute, and set up
# OGR feature style string to LABEL.


def test_ogr_cad_6():

    ds = gdal.OpenEx('data/cad/text_mtext_attdef_r2000.dwg', allowed_drivers=['CAD'])

    assert ds.GetLayerCount() == 1, 'expected exactly one layer.'

    layer = ds.GetLayer(0)

    assert layer.GetFeatureCount() == 3, ('expected 3 features, got: %d'
                             % layer.GetFeatureCount())

    feat = layer.GetNextFeature()

    assert not ogrtest.check_feature_geometry(feat, 'POINT(0.7413 1.7794 0)')

    expected_style = 'LABEL(f:"Arial",t:"Русские буквы",c:#FFFFFFFF)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason('Got unexpected style string:\n%s\ninstead of:\n%s.'
                             % (feat.GetStyleString(), expected_style))
        return 'expected_fail'  # cannot sure iconv is buildin

###############################################################################
# Check MTEXT as TEXT geometry.


def test_ogr_cad_7():

    ds = gdal.OpenEx('data/cad/text_mtext_attdef_r2000.dwg', allowed_drivers=['CAD'])
    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()
    feat = layer.GetNextFeature()

    assert not ogrtest.check_feature_geometry(feat, 'POINT(2.8139 5.7963 0)')

    expected_style = 'LABEL(f:"Arial",t:"English letters",c:#FFFFFFFF)'
    assert feat.GetStyleString() == expected_style, \
        ('Got unexpected style string:\n%s\ninstead of:\n%s.'
                             % (feat.GetStyleString(), expected_style))

###############################################################################
# Check ATTDEF as TEXT geometry.


def test_ogr_cad_8():

    ds = gdal.OpenEx('data/cad/text_mtext_attdef_r2000.dwg', allowed_drivers=['CAD'])
    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()
    feat = layer.GetNextFeature()
    feat = layer.GetNextFeature()

    assert not ogrtest.check_feature_geometry(feat, 'POINT(4.98953601938918 2.62670161690571 0)')

    expected_style = 'LABEL(f:"Arial",t:"TESTTAG",c:#FFFFFFFF)'
    assert feat.GetStyleString() == expected_style, \
        ('Got unexpected style string:\n%s\ninstead of:\n%s.'
                             % (feat.GetStyleString(), expected_style))

###############################################################################
# Open a not handled DWG version


def test_ogr_cad_9():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/cad/AC1018_signature.dwg', allowed_drivers=['CAD'])
    assert ds is None
    msg = gdal.GetLastErrorMsg()
    assert 'does not support this version' in msg
