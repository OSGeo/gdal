#!/usr/bin/env python
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
#  Copyright (c) 2016, NextGIS <info@nextgis.com>
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
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Check driver existence.
def ogr_cad_1():

    gdaltest.cad_ds = None
    gdaltest.cad_dr = None

    try:
        gdaltest.cad_dr = ogr.GetDriverByName( 'CAD' )
        if gdaltest.cad_dr is None:
            return 'skip'
    except:
        return 'skip'

    return 'success'

###############################################################################
# Check driver properly opens simple file, reads correct feature (ellipse).
def ogr_cad_2():
    if gdaltest.cad_dr is None:
        return 'skip'

    gdaltest.cad_ds = gdal.OpenEx( 'data/cad/ellipse_r2000.dwg', allowed_drivers = ['CAD'] )

    if gdaltest.cad_ds is None:
        return 'fail'

    if gdaltest.cad_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'expected exactly one layer.' )
        return 'fail'

    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(0)

    if gdaltest.cad_layer.GetName() != '0':
        gdaltest.post_reason( 'layer name is expected to be default = 0.' )
        return 'fail'

    defn = gdaltest.cad_layer.GetLayerDefn()
    if defn.GetFieldCount() != 5:
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d'
                              % defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'cad feature 0 get failed.' )
        return 'fail'

    if feat.cadgeom_type != 'CADEllipse':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADEllipse, got: %s'
                              % feat.cadgeom_type )
        return 'fail'

    if feat.GetFID() != 0:
        gdaltest.post_reason( 'did not get expected FID for feature 0.' )
        return 'fail'

    if feat.thickness != 0:
        gdaltest.post_reason( 'did not get expected thickness. expected 0, got: %f'
                              % feat.thickness )
        return 'fail'

    if feat.extentity_data is not None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'

    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                              % ( feat.GetStyleString(), expected_style ) )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom is None:
        gdaltest.post_reason( 'cad geometry is None.' )
        return 'fail'

    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    gdaltest.cad_ds = None
    return 'success'

###############################################################################
# Check proper read of 3 layers (one circle on each) with different parameters.
def ogr_cad_3():
    if gdaltest.cad_dr is None:
        return 'skip'

    gdaltest.cad_ds = gdal.OpenEx( 'data/cad/triple_circles_r2000.dwg', allowed_drivers = ['CAD'] )

    if gdaltest.cad_ds is None:
        return 'fail'

    if gdaltest.cad_ds.GetLayerCount() != 3:
        gdaltest.post_reason( 'expected 3 layers.' )
        return 'fail'

    # test first layer and circle
    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(0)

    if gdaltest.cad_layer.GetName() != '0':
        gdaltest.post_reason( 'layer name is expected to be default = 0.' )
        return 'fail'

    defn = gdaltest.cad_layer.GetLayerDefn()
    if defn.GetFieldCount() != 5:
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d'
                              % defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat.cadgeom_type != 'CADCircle':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADCircle, got: %s'
                              % feat.cadgeom_type )
        return 'fail'

    if feat.thickness != 1.2:
        gdaltest.post_reason( 'did not get expected thickness. expected 1.2, got: %f'
                              % feat.thickness )
        return 'fail'

    if feat.extentity_data is not None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'


    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                              % ( feat.GetStyleString(), expected_style ) )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbCircularStringZ:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    # test second layer and circle
    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(1)

    if gdaltest.cad_layer.GetName() != '1':
        gdaltest.post_reason( 'layer name is expected to be 1.' )
        return 'fail'

    defn = gdaltest.cad_layer.GetLayerDefn()
    if defn.GetFieldCount() != 5:
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d'
                              % defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat.cadgeom_type != 'CADCircle':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADCircle, got: %s'
                              % feat.cadgeom_type )
        return 'fail'

    if feat.thickness != 0.8:
        gdaltest.post_reason( 'did not get expected thickness. expected 0.8, got: %f'
                              % feat.thickness )
        return 'fail'

    if feat.extentity_data is not None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'

    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                              % ( feat.GetStyleString(), expected_style ) )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbCircularStringZ:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    # test third layer and circle
    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(2)

    if gdaltest.cad_layer.GetName() != '2':
        gdaltest.post_reason( 'layer name is expected to be 2.' )
        return 'fail'

    defn = gdaltest.cad_layer.GetLayerDefn()
    if defn.GetFieldCount() != 5:
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d'
                              % defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat.cadgeom_type != 'CADCircle':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADCircle, got: %s'
                              % feat.cadgeom_type )
        return 'fail'

    if feat.thickness != 1.8:
        gdaltest.post_reason( 'did not get expected thickness. expected 1.8, got: %f'
                              % feat.thickness )
        return 'fail'

    if feat.extentity_data is not None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'


    expected_style = 'PEN(c:#FFFFFFFF,w:5px)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string on feature 0:\n%s\ninstead of:\n%s.'
                              % ( feat.GetStyleString(), expected_style ) )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbCircularStringZ:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    gdaltest.cad_ds = None
    return 'success'

###############################################################################
# Check reading of a single point.
def ogr_cad_4():
    if gdaltest.cad_dr is None:
        return 'skip'

    gdaltest.cad_ds = gdal.OpenEx( 'data/cad/point2d_r2000.dwg', allowed_drivers = ['CAD'] )

    if gdaltest.cad_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'expected exactly one layer.' )
        return 'fail'

    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(0)

    if gdaltest.cad_layer.GetFeatureCount() != 1:
        gdaltest.post_reason( 'expected exactly one feature.' )
        return 'fail'

    feat = gdaltest.cad_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT (50 50 0)' ):
        gdaltest.post_reason( 'got feature which doesnot fit expectations.' )
        return 'fail'

    gdaltest.cad_ds = None
    return 'success'

###############################################################################
# Check reading of a simple line.
def ogr_cad_5():
    if gdaltest.cad_dr is None:
        return 'skip'

    gdaltest.cad_ds = gdal.OpenEx( 'data/cad/line_r2000.dwg', allowed_drivers = ['CAD'] )

    if gdaltest.cad_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'expected exactly one layer.' )
        return 'fail'

    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(0)

    if gdaltest.cad_layer.GetFeatureCount() != 1:
        gdaltest.post_reason( 'expected exactly one feature.' )
        return 'fail'

    feat = gdaltest.cad_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (50 50 0,100 100 0)' ):
        gdaltest.post_reason( 'got feature which doesnot fit expectations.' )
        return 'fail'

    gdaltest.cad_ds = None
    return 'success'

###############################################################################
# Check reading of a text (point with attached 'text' attribute, and set up
# OGR feature style string to LABEL.
def ogr_cad_6():
    if gdaltest.cad_dr is None:
        return 'skip'

    gdaltest.cad_ds = gdal.OpenEx( 'data/cad/text_mtext_attdef_r2000.dwg', allowed_drivers = ['CAD'] )

    if gdaltest.cad_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'expected exactly one layer.' )
        return 'fail'

    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(0)

    if gdaltest.cad_layer.GetFeatureCount() != 3:
        gdaltest.post_reason( 'expected 3 features, got: %d'
                              % gdaltest.cad_layer.GetFeatureCount() )
        return 'fail'

    feat = gdaltest.cad_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT(0.7413 1.7794 0)' ):
        return 'fail'

    expected_style = 'LABEL(f:"Arial",t:"Русские буквы",c:#FFFFFFFF)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.'
                              % ( feat.GetStyleString(), expected_style ) )
        return 'expected_fail' # cannot sure iconv is buildin

    return 'success'

###############################################################################
# Check MTEXT as TEXT geometry.
def ogr_cad_7():
    if gdaltest.cad_dr is None:
        return 'skip'

    feat = gdaltest.cad_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT(2.8139 5.7963 0)' ):
        return 'fail'

    expected_style = 'LABEL(f:"Arial",t:"English letters",c:#FFFFFFFF)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.'
                              % ( feat.GetStyleString(), expected_style ) )
        return 'fail'

    return 'success'

###############################################################################
# Check ATTDEF as TEXT geometry.
def ogr_cad_8():
    if gdaltest.cad_dr is None:
        return 'skip'

    feat = gdaltest.cad_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT(4.98953601938918 2.62670161690571 0)' ):
        return 'fail'

    expected_style = 'LABEL(f:"Arial",t:"TESTTAG",c:#FFFFFFFF)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.'
                              % ( feat.GetStyleString(), expected_style ) )
        return 'fail'

    return 'success'

###############################################################################
# Open a not handled DWG version

def ogr_cad_9():
    if gdaltest.cad_dr is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/AC1018_signature.dwg', allowed_drivers = ['CAD'] )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    msg = gdal.GetLastErrorMsg()
    if msg.find('does not support this version') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Cleanup
def ogr_cad_cleanup():
    gdaltest.cad_layer = None
    gdaltest.cad_ds = None

    return 'success'

gdaltest_list = [
    ogr_cad_1,
    ogr_cad_2,
    ogr_cad_3,
    ogr_cad_4,
    ogr_cad_5,
    ogr_cad_6,
    ogr_cad_7,
    ogr_cad_8,
    ogr_cad_9,
    ogr_cad_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_cad' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
