#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PDF driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

###############################################################################
# Test write support

def ogr_pdf_1(name = 'tmp/ogr_pdf_1.pdf', write_attributes = 'YES'):

    if ogr.GetDriverByName('PDF') is None:
        return 'skip'

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    ds = ogr.GetDriverByName('PDF').CreateDataSource(name, options = ['MARGIN=10', 'OGR_WRITE_ATTRIBUTES=%s' % write_attributes, 'OGR_LINK_FIELD=linkfield'])

    lyr = ds.CreateLayer('first_layer', srs = sr)

    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('linkfield', ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    feat.SetField('strfield', 'super tex !')
    feat.SetField('linkfield', 'http://gdal.org/')
    feat.SetStyleString('LABEL(t:{strfield},dx:5,dy:10)')
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

    return 'success'

###############################################################################
# Test read support

def ogr_pdf_2(name = 'tmp/ogr_pdf_1.pdf', has_attributes = True):

    if ogr.GetDriverByName('PDF') is None:
        return 'skip'

    # Check read support
    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    md = gdal_pdf_drv.GetMetadata()
    if not 'HAVE_POPPLER' in md and not 'HAVE_PODOFO' in md and not 'HAVE_PDFIUM' in md:
        return 'skip'

    ds = ogr.Open(name)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('first_layer')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    if has_attributes:
        if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('strfield')).GetType() != ogr.OFTString:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('intfield')).GetType() != ogr.OFTInteger:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('realfield')).GetType() != ogr.OFTReal:
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if lyr.GetLayerDefn().GetFieldCount() != 0:
            gdaltest.post_reason('fail')
            return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(2 49)')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING(2 48,3 50)')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if has_attributes:
        if feat.GetField('strfield') != 'str':
            gdaltest.post_reason('fail')
            return 'fail'
        if feat.GetField('intfield') != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        if abs(feat.GetFieldAsDouble('realfield') - 2.34) > 1e-10:
            gdaltest.post_reason('fail')
            print(feat.GetFieldAsDouble('realfield'))
            return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48))')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48),(2.25 48.25,2.25 48.75,2.75 48.75,2.75 48.25,2.25 48.25))')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    for i in range(10):
        feat = lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(%f 49.1)' % (2 + i * 0.05))) != 0:
            gdaltest.post_reason('fail with ogr-sym-%d' % i)
            return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(2.5 49.1)')) != 0:
        gdaltest.post_reason('fail with raster icon')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test write support without writing attributes

def ogr_pdf_3():
    return ogr_pdf_1('tmp/ogr_pdf_2.pdf', 'NO')

###############################################################################
# Check read support without writing attributes

def ogr_pdf_4():
    return ogr_pdf_2('tmp/ogr_pdf_2.pdf', False)


###############################################################################
# Switch from poppler to podofo if both are available

def ogr_pdf_4_podofo():

    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    if gdal_pdf_drv is None:
        return 'skip'

    md = gdal_pdf_drv.GetMetadata()
    if 'HAVE_POPPLER' in md and 'HAVE_PODOFO' in md:
        gdal.SetConfigOption("GDAL_PDF_LIB", "PODOFO")
        print('Using podofo now')
        ret = ogr_pdf_4()
        gdal.SetConfigOption("GDAL_PDF_LIB", None)
        return ret
    else:
        return 'skip'

###############################################################################
# Test read support with OGR_PDF_READ_NON_STRUCTURED=YES

def ogr_pdf_5():

    if ogr.GetDriverByName('PDF') is None:
        return 'skip'

    # Check read support
    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    md = gdal_pdf_drv.GetMetadata()
    if not 'HAVE_POPPLER' in md and not 'HAVE_PODOFO' in md and not 'HAVE_PDFIUM' in md:
        return 'skip'

    with gdaltest.config_option('OGR_PDF_READ_NON_STRUCTURED', 'YES'):
        ds = ogr.Open('data/drawing.pdf')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Note: the circle is wrongly drawned as a diamond
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 8:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test read support with a non-OGR datasource

def ogr_pdf_online_1():

    if ogr.GetDriverByName('PDF') is None:
        return 'skip'

    # Check read support
    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    md = gdal_pdf_drv.GetMetadata()
    if not 'HAVE_POPPLER' in md and not 'HAVE_PODOFO' in md and not 'HAVE_PDFIUM' in md:
        return 'skip'

    if not gdaltest.download_file('http://www.terragotech.com/system/files/geopdf/webmap_urbansample.pdf', 'webmap_urbansample.pdf'):
        return 'skip'

    expected_layers = [
        [ "Cadastral Boundaries", ogr.wkbPolygon ],
        [ "Water Lines", ogr.wkbLineString ],
        [ "Sewerage Lines", ogr.wkbLineString ],
        [ "Sewerage Jump-Ups", ogr.wkbLineString ],
        [ "Roads", ogr.wkbUnknown ],
        [ "Water Points", ogr.wkbPoint ],
        [ "Sewerage Pump Stations", ogr.wkbPoint ],
        [ "Sewerage Man Holes", ogr.wkbPoint ],
        [ "BPS - Buildings", ogr.wkbPolygon ],
        [ "BPS - Facilities", ogr.wkbPolygon ],
        [ "BPS - Water Sources", ogr.wkbPoint ],
    ]

    ds = ogr.Open( 'tmp/cache/webmap_urbansample.pdf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != len(expected_layers):
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'

    for i in range(ds.GetLayerCount()):
        if ds.GetLayer(i).GetName() != expected_layers[i][0]:
            gdaltest.post_reason('fail')
            print('%d : %s' % (i, ds.GetLayer(i).GetName()))
            return 'fail'

        if ds.GetLayer(i).GetGeomType() != expected_layers[i][1]:
            gdaltest.post_reason('fail')
            print('%d : %d' % (i, ds.GetLayer(i).GetGeomType()))
            return 'fail'

    lyr = ds.GetLayerByName('Water Points')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (724431.316665166523308 7672947.212302438914776)')) != 0:
        feat.DumpReadable()
        return 'fail'
    if feat.GetField('ID') != 'VL46':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def ogr_pdf_cleanup():

    if ogr.GetDriverByName('PDF') is None:
        return 'skip'

    ogr.GetDriverByName('PDF').DeleteDataSource('tmp/ogr_pdf_1.pdf')
    ogr.GetDriverByName('PDF').DeleteDataSource('tmp/ogr_pdf_2.pdf')

    return 'success'

gdaltest_list = [
    ogr_pdf_1,
    ogr_pdf_2,
    ogr_pdf_3,
    ogr_pdf_4,
    ogr_pdf_4_podofo,
    ogr_pdf_5,
    ogr_pdf_online_1,
    ogr_pdf_cleanup
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pdf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

