import os
import sys
from sys import version_info

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
from osgeo import gdal
from osgeo import ogr

# first test is to read 1 ellipse.
def ogr_cad_1():
    gdaltest.cad_ds = ogr.Open( 'data/cad/ellipse_r2000.dwg' )

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
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d' %defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat.cadgeom_type != 'CADEllipse':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADEllipse, got: %s' %feat.cadgeom_type )
        return 'fail'

    if feat.GetFID() != 0:
        gdaltest.post_reason( 'did not get expected FID for feature 0.' )
        return 'fail'

    if feat.thickness != 0:
        gdaltest.post_reason( 'did not get expected thickness. expected 0, got: %f' %feat.thickness )
        return 'fail'

    if feat.extentity_data != None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#ffffff,w:5px)':
        gdaltest.post_reason( 'did not get expected style string on feature 0.' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    return 'success'

# second test is to read a pretty large file with mixed geometries
def ogr_cad_2():
    gdaltest.cad_ds = ogr.Open( 'data/cad/24127_circles_128_lines_r2000.dwg' )

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
        gdaltest.post_reason( 'did not get expected number of fields in defn.' )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 24255:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    return 'success'

# read 3 circles (each one is on own layer)
def ogr_cad_3():
    gdaltest.cad_ds = ogr.Open( 'data/cad/triple_circles_r2000.dwg' )

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
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d' %defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat.cadgeom_type != 'CADCircle':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADCircle, got: %s' %feat.cadgeom_type )
        return 'fail'

    if feat.thickness != 1.2:
        gdaltest.post_reason( 'did not get expected thickness. expected 1.2, got: %f' %feat.thickness )
        return 'fail'

    if feat.extentity_data != None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#ffffff,w:5px)':
        gdaltest.post_reason( 'did not get expected style string on feature 0.' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

# test second layer and circle
    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(1)

    if gdaltest.cad_layer.GetName() != '1':
        gdaltest.post_reason( 'layer name is expected to be 1.' )
        return 'fail'

    defn = gdaltest.cad_layer.GetLayerDefn()
    if defn.GetFieldCount() != 5:
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d' %defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat.cadgeom_type != 'CADCircle':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADCircle, got: %s' %feat.cadgeom_type )
        return 'fail'

    if feat.thickness != 0.8:
        gdaltest.post_reason( 'did not get expected thickness. expected 0.8, got: %f' %feat.thickness )
        return 'fail'

    if feat.extentity_data != None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#ffffff,w:5px)':
        gdaltest.post_reason( 'did not get expected style string on feature 0.' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'
        
# test third layer and circle
    gdaltest.cad_layer = gdaltest.cad_ds.GetLayer(2)

    if gdaltest.cad_layer.GetName() != '2':
        gdaltest.post_reason( 'layer name is expected to be 2.' )
        return 'fail'

    defn = gdaltest.cad_layer.GetLayerDefn()
    if defn.GetFieldCount() != 5:
        gdaltest.post_reason( 'did not get expected number of fields in defn. got %d' %defn.GetFieldCount() )
        return 'fail'

    fc = gdaltest.cad_layer.GetFeatureCount()
    if fc != 1:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc )
        return 'fail'

    gdaltest.cad_layer.ResetReading()

    feat = gdaltest.cad_layer.GetNextFeature()

    if feat.cadgeom_type != 'CADCircle':
        gdaltest.post_reason( 'cad geometry type is wrong. Expected CADCircle, got: %s' %feat.cadgeom_type )
        return 'fail'

    if feat.thickness != 1.8:
        gdaltest.post_reason( 'did not get expected thickness. expected 1.8, got: %f' %feat.thickness )
        return 'fail'

    if feat.extentity_data != None:
        gdaltest.post_reason( 'expected feature ExtendedEntityData to be null.' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#ffffff,w:5px)':
        gdaltest.post_reason( 'did not get expected style string on feature 0.' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    return 'success'

# cleanup
def ogr_cad_cleanup():
    gdaltest.cad_layer = None
    gdaltest.cad_ds = None

    return 'success'

gdaltest_list = [
    ogr_cad_1,
    ogr_cad_2,
    ogr_cad_3,
    ogr_cad_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_cad' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
