#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PDF driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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



import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest


def has_read_support():

    if ogr.GetDriverByName('PDF') is None:
        return False

    # Check read support
    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    md = gdal_pdf_drv.GetMetadata()
    if 'HAVE_POPPLER' not in md and 'HAVE_PODOFO' not in md and 'HAVE_PDFIUM' not in md:
        return False

    return True

###############################################################################
# Test write support


def test_ogr_pdf_1(name='tmp/ogr_pdf_1.pdf', write_attributes='YES'):

    if ogr.GetDriverByName('PDF') is None:
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    ds = ogr.GetDriverByName('PDF').CreateDataSource(name, options=['STREAM_COMPRESS=NONE', 'MARGIN=10', 'OGR_WRITE_ATTRIBUTES=%s' % write_attributes, 'OGR_LINK_FIELD=linkfield'])

    lyr = ds.CreateLayer('first_layer', srs=sr)

    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('linkfield', ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    feat.SetField('strfield', 'super tex !')
    feat.SetField('linkfield', 'http://gdal.org/')
    feat.SetStyleString('LABEL(t:{strfield},dx:5,dy:10,a:45,p:4)')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(2 48,3 50)'))
    feat.SetField('strfield', 'str')
    feat.SetField('intfield', 1)
    feat.SetField('realfield', 2.34)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48))'))
    feat.SetField('linkfield', 'http://gdal.org/')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48),(2.25 48.25,2.25 48.75,2.75 48.75,2.75 48.25,2.25 48.25))'))
    lyr.CreateFeature(feat)

    for i in range(10):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetStyleString('SYMBOL(c:#FF0000,id:"ogr-sym-%d",s:10)' % i)
        feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%f 49.1)' % (2 + i * 0.05)))
        lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetStyleString('SYMBOL(c:#000000,id:"../gcore/data/byte.tif")')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.5 49.1)'))
    lyr.CreateFeature(feat)

    ds = None

    # Do a quick test to make sure the text came out OK.
    wantedstream = 'BT\n' + \
        '362.038672 362.038672 -362.038672 362.038672 18.039040 528.960960 Tm\n' + \
        '0.000000 0.000000 0.000000 rg\n' + \
        '/F1 0.023438 Tf\n' + \
        '(super tex !) Tj\n' + \
        'ET'

    with open(name, 'rb') as f:
        data = f.read(8192)
        assert wantedstream.encode('utf-8') in data, \
            'Wrong text data in written PDF stream'

    
###############################################################################
# Test read support


def test_ogr_pdf_2(name='tmp/ogr_pdf_1.pdf', has_attributes=True):

    if not has_read_support():
        pytest.skip()

    ds = ogr.Open(name)
    assert ds is not None

    lyr = ds.GetLayerByName('first_layer')
    assert lyr is not None

    if has_attributes:
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('strfield')).GetType() == ogr.OFTString
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('intfield')).GetType() == ogr.OFTInteger
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('realfield')).GetType() == ogr.OFTReal
    else:
        assert lyr.GetLayerDefn().GetFieldCount() == 0

    if has_attributes:
        feat = lyr.GetNextFeature()
    # This won't work properly until text support is added to the
    # PDF vector feature reader
    # if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(2 49)')) != 0:
    #    feat.DumpReadable()
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING(2 48,3 50)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    if has_attributes:
        if feat.GetField('strfield') != 'str':
            feat.DumpReadable()
            pytest.fail()
        if feat.GetField('intfield') != 1:
            feat.DumpReadable()
            pytest.fail()
        if abs(feat.GetFieldAsDouble('realfield') - 2.34) > 1e-10:
            feat.DumpReadable()
            pytest.fail()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48))')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48),(2.25 48.25,2.25 48.75,2.75 48.75,2.75 48.25,2.25 48.25))')) != 0:
        feat.DumpReadable()
        pytest.fail()

    for i in range(10):
        feat = lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(%f 49.1)' % (2 + i * 0.05))) != 0:
            feat.DumpReadable()
            pytest.fail('fail with ogr-sym-%d' % i)

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(2.5 49.1)')) != 0:
        feat.DumpReadable()
        pytest.fail('fail with raster icon')

    ds = None

###############################################################################
# Test write support without writing attributes


def test_ogr_pdf_3():
    return test_ogr_pdf_1('tmp/ogr_pdf_2.pdf', 'NO')

###############################################################################
# Check read support without writing attributes


def test_ogr_pdf_4():
    return test_ogr_pdf_2('tmp/ogr_pdf_2.pdf', False)


###############################################################################
# Switch from poppler to podofo if both are available

def test_ogr_pdf_4_podofo():

    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    if gdal_pdf_drv is None:
        pytest.skip()

    md = gdal_pdf_drv.GetMetadata()
    if 'HAVE_POPPLER' in md and 'HAVE_PODOFO' in md:
        gdal.SetConfigOption("GDAL_PDF_LIB", "PODOFO")
        print('Using podofo now')
        ret = test_ogr_pdf_4()
        gdal.SetConfigOption("GDAL_PDF_LIB", None)
        return ret
    pytest.skip()

###############################################################################
# Test read support with OGR_PDF_READ_NON_STRUCTURED=YES


def test_ogr_pdf_5():

    if not has_read_support():
        pytest.skip()

    with gdaltest.config_option('OGR_PDF_READ_NON_STRUCTURED', 'YES'):
        ds = ogr.Open('data/drawing.pdf')
    assert ds is not None

    # Note: the circle is wrongly drawned as a diamond
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 8

###############################################################################
# Test read support with a non-OGR datasource


def test_ogr_pdf_online_1():

    if not has_read_support():
        pytest.skip()

    if not gdaltest.download_file('http://www.terragotech.com/images/pdf/webmap_urbansample.pdf', 'webmap_urbansample.pdf'):
        pytest.skip()

    expected_layers = [
        ["Cadastral Boundaries", ogr.wkbPolygon],
        ["Water Lines", ogr.wkbLineString],
        ["Sewerage Lines", ogr.wkbLineString],
        ["Sewerage Jump-Ups", ogr.wkbLineString],
        ["Roads", ogr.wkbUnknown],
        ["Water Points", ogr.wkbPoint],
        ["Sewerage Pump Stations", ogr.wkbPoint],
        ["Sewerage Man Holes", ogr.wkbPoint],
        ["BPS - Buildings", ogr.wkbPolygon],
        ["BPS - Facilities", ogr.wkbPolygon],
        ["BPS - Water Sources", ogr.wkbPoint],
    ]

    ds = ogr.Open('tmp/cache/webmap_urbansample.pdf')
    assert ds is not None

    assert ds.GetLayerCount() == len(expected_layers)

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() == expected_layers[i][0], \
            ('%d : %s' % (i, ds.GetLayer(i).GetName()))

        assert ds.GetLayer(i).GetGeomType() == expected_layers[i][1], \
            ('%d : %d' % (i, ds.GetLayer(i).GetGeomType()))

    lyr = ds.GetLayerByName('Water Points')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (724431.316665166523308 7672947.212302438914776)')) != 0:
        feat.DumpReadable()
        pytest.fail()
    assert feat.GetField('ID') == 'VL46'

###############################################################################
# Test read support of non-structured content


def test_ogr_pdf_online_2():

    if not has_read_support():
        pytest.skip()

    if not gdaltest.download_file('https://download.osgeo.org/gdal/data/pdf/340711752_Azusa_FSTopo.pdf', '340711752_Azusa_FSTopo.pdf'):
        pytest.skip()

    expected_layers = [
        ['Other_5', 0],
        ['Quadrangle_Extent_Other_4', 0],
        ['Quadrangle_Extent_State_Outline', 0],
        ['Adjacent_Quadrangle_Diagram_Other_3', 0],
        ['Adjacent_Quadrangle_Diagram_Quadrangle_Extent', 0],
        ['Adjacent_Quadrangle_Diagram_Quad_Outlines', 0],
        ['Quadrangle_Other', 0],
        ['Quadrangle_Labels_Unplaced_Labels_Road_Shields_-_Vertical', 0],
        ['Quadrangle_Labels_Road_Shields_-_Horizontal', 0],
        ['Quadrangle_Labels_Road_Shields_-_Vertical', 0],
        ['Quadrangle_Neatline/Mask_Neatline', 0],
        ['Quadrangle_Neatline/Mask_Mask', 0],
        ['Quadrangle_Culture_Features', 0],
        ['Quadrangle_Large_Tanks', 0],
        ['Quadrangle_Linear_Transportation_Features', 0],
        ['Quadrangle_Railroads_', 0],
        ['Quadrangle_Linear_Culture_Features', 0],
        ['Quadrangle_Linear_Landform_Features', 0],
        ['Quadrangle_Boundaries', 0],
        ['Quadrangle_PLSS', 0],
        ['Quadrangle_Survey_Lines', 0],
        ['Quadrangle_Linear_Drainage_Features', 0],
        ['Quadrangle_Contour_Labels', 0],
        ['Quadrangle_Contours', 0],
        ['Quadrangle_2_5`_Tics_Interior_Grid_Intersections', 0],
        ['Quadrangle_2_5`_Tics_Grid_Tics_along_Neatline', 0],
        ['Quadrangle_UTM_Grid_Interior_Grid_Intersections', 0],
        ['Quadrangle_UTM_Grid_Grid_Tics_along_Neatline', 0],
        ['Quadrangle_UTM_Grid_UTM_Grid_Lines', 0],
        ['Quadrangle_Large_Buildings', 0],
        ['Quadrangle_Drainage_Polygons', 0],
        ['Quadrangle_Ownership', 0],
        ['Quadrangle_Builtup_Areas', 0],
        ['Quadrangle_WoodlandUSGS_P', 0],
    ]

    ds = ogr.Open('tmp/cache/340711752_Azusa_FSTopo.pdf')
    assert ds is not None

    if ds.GetLayerCount() != len(expected_layers):
        for lyr in ds:
            print(lyr.GetName(), lyr.GetGeomType())
        pytest.fail(ds.GetLayerCount())

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() == expected_layers[i][0], \
            ('%d : %s' % (i, ds.GetLayer(i).GetName()))

        assert ds.GetLayer(i).GetGeomType() == expected_layers[i][1], \
            ('%d : %d' % (i, ds.GetLayer(i).GetGeomType()))

    
###############################################################################
# Cleanup


def test_ogr_pdf_cleanup():

    if ogr.GetDriverByName('PDF') is None:
        pytest.skip()

    ogr.GetDriverByName('PDF').DeleteDataSource('tmp/ogr_pdf_1.pdf')
    ogr.GetDriverByName('PDF').DeleteDataSource('tmp/ogr_pdf_2.pdf')



