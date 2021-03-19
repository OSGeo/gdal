#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  librarified ogr2ogr testing
# Author:   Faza Mahamood <fazamhd @ gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
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

from osgeo import gdal, gdalconst, ogr
import gdaltest
import ogrtest
import pytest

###############################################################################
# Simple test


def test_ogr2ogr_lib_1():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format='Memory')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    feat0 = ds.GetLayer(0).GetFeature(0)
    assert feat0.GetFieldAsDouble('AREA') == 215229.266, \
        'Did not get expected value for field AREA'
    assert feat0.GetFieldAsString('PRFEDEA') == '35043411', \
        'Did not get expected value for field PRFEDEA'

###############################################################################
# Test SQLStatement


def test_ogr2ogr_lib_2():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', SQLStatement='select * from poly', SQLDialect='OGRSQL')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    # Test @filename syntax
    gdal.FileFromMemBuffer('/vsimem/sql.txt', '-- initial comment\nselect * from poly\n-- trailing comment')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', SQLStatement='@/vsimem/sql.txt')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    gdal.Unlink('/vsimem/sql.txt')

    # Test @filename syntax with a UTF-8 BOM
    gdal.FileFromMemBuffer('/vsimem/sql.txt', '\xEF\xBB\xBFselect * from poly'.encode('LATIN1'))
    ds = gdal.VectorTranslate('', srcDS, format='Memory', SQLStatement='@/vsimem/sql.txt')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    gdal.Unlink('/vsimem/sql.txt')

###############################################################################
# Test WHERE


def test_ogr2ogr_lib_3():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', where='EAS_ID=171')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1

    # Test @filename syntax
    gdal.FileFromMemBuffer('/vsimem/filter.txt', 'EAS_ID=171')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', where='@/vsimem/filter.txt')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    gdal.Unlink('/vsimem/filter.txt')

###############################################################################
# Test accessMode


def test_ogr2ogr_lib_4():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/poly.shp', srcDS)
    assert ds.GetLayer(0).GetFeatureCount() == 10, 'wrong feature count'
    ds = None

    ds = gdal.VectorTranslate('/vsimem/poly.shp', srcDS, accessMode='append')
    assert ds is not None, 'ds is None'
    assert ds.GetLayer(0).GetFeatureCount() == 20, 'wrong feature count'

    ret = gdal.VectorTranslate(ds, srcDS, accessMode='append')
    assert ret == 1, 'ds is None'
    assert ds.GetLayer(0).GetFeatureCount() == 30, 'wrong feature count'

    feat10 = ds.GetLayer(0).GetFeature(10)
    assert feat10.GetFieldAsDouble('AREA') == 215229.266, \
        'Did not get expected value for field AREA'
    assert feat10.GetFieldAsString('PRFEDEA') == '35043411', \
        'Did not get expected value for field PRFEDEA'

    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/poly.shp')

###############################################################################
# Test dstSRS


def test_ogr2ogr_lib_5():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', dstSRS='EPSG:4326')
    assert str(ds.GetLayer(0).GetSpatialRef()).find('1984') != -1

###############################################################################
# Test selFields


def test_ogr2ogr_lib_6():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    # Voluntary don't use the exact case of the source field names (#4502)
    ds = gdal.VectorTranslate('', srcDS, format='Memory', selectFields=['eas_id', 'prfedea'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    feat = lyr.GetNextFeature()
    ret = 'success'
    if feat.GetFieldAsDouble('EAS_ID') != 168:
        gdaltest.post_reason('did not get expected value for EAS_ID')
        print(feat.GetFieldAsDouble('EAS_ID'))
        ret = 'fail'
    elif feat.GetFieldAsString('PRFEDEA') != '35043411':
        gdaltest.post_reason('did not get expected value for PRFEDEA')
        print(feat.GetFieldAsString('PRFEDEA'))
        ret = 'fail'

    return ret

###############################################################################
# Test LCO


def test_ogr2ogr_lib_7():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/poly.shp', srcDS, layerCreationOptions=['SHPT=POLYGONZ'])
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D

    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/poly.shp')

###############################################################################
# Add explicit source layer name


def test_ogr2ogr_lib_8():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', layers=['poly'])
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    # Test also with just a string and not an array
    ds = gdal.VectorTranslate('', srcDS, format='Memory', layers='poly')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

###############################################################################
# Test -segmentize


def test_ogr2ogr_lib_9():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', segmentizeMaxDist=100)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryRef(0).GetPointCount() == 36

###############################################################################
# Test overwrite with a shapefile


def test_ogr2ogr_lib_10():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/tmp/poly.shp', srcDS)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds = None

    # Overwrite
    ds = gdal.VectorTranslate('/vsimem/tmp', srcDS, accessMode='overwrite')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/tmp')

###############################################################################
# Test filter


def test_ogr2ogr_lib_11():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format='Memory', spatFilter=[479609, 4764629, 479764, 4764817])
    if ogrtest.have_geos():
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4
    else:
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 5


###############################################################################
# Test callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_ogr2ogr_lib_12():

    tab = [0]
    ds = gdal.VectorTranslate('', '../ogr/data/poly.shp', format='Memory', callback=mycallback, callback_data=tab)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    assert tab[0] == 1.0, 'Bad percentage'

###############################################################################
# Test callback with failure


def mycallback_with_failure(pct, msg, user_data):
    # pylint: disable=unused-argument
    if pct > 0.5:
        return 0
    return 1


def test_ogr2ogr_lib_13():

    with gdaltest.error_handler():
        ds = gdal.VectorTranslate('', '../ogr/data/poly.shp', format='Memory', callback=mycallback_with_failure)
    assert ds is None

###############################################################################
# Test internal wrappers


def test_ogr2ogr_lib_14():

    # Null dest name and no option
    try:
        gdal.wrapper_GDALVectorTranslateDestName(None, gdal.OpenEx('../ogr/data/poly.shp'), None)
    except RuntimeError:
        pass


###############################################################################
# Test non existing zfield


def test_ogr2ogr_lib_15():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    with gdaltest.error_handler():
        ds = gdal.VectorTranslate('', srcDS, format='Memory', zField='foo')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon

###############################################################################
# Test -dim


def test_ogr2ogr_lib_16():

    tests = [['POINT M (1 2 3)', None, 'POINT M (1 2 3)'],
             ['POINT M (1 2 3)', 'XY', 'POINT (1 2)'],
             ['POINT M (1 2 3)', 'XYZ', 'POINT Z (1 2 0)'],
             ['POINT M (1 2 3)', 'XYM', 'POINT M (1 2 3)'],
             ['POINT M (1 2 3)', 'XYZM', 'POINT ZM (1 2 0 3)'],
             ['POINT M (1 2 3)', 'layer_dim', 'POINT M (1 2 3)'],
             ['POINT ZM (1 2 3 4)', None, 'POINT ZM (1 2 3 4)'],
             ['POINT ZM (1 2 3 4)', 'XY', 'POINT (1 2)'],
             ['POINT ZM (1 2 3 4)', 'XYZ', 'POINT Z (1 2 3)'],
             ['POINT ZM (1 2 3 4)', 'XYM', 'POINT M (1 2 4)'],
             ['POINT ZM (1 2 3 4)', 'XYZM', 'POINT ZM (1 2 3 4)'],
             ['POINT ZM (1 2 3 4)', 'layer_dim', 'POINT ZM (1 2 3 4)'],
            ]
    for (wkt_before, dim, wkt_after) in tests:
        srcDS = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
        geom = ogr.CreateGeometryFromWkt(wkt_before)
        lyr = srcDS.CreateLayer('test', geom_type=geom.GetGeometryType())
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)

        ds = gdal.VectorTranslate('', srcDS, format='Memory', dim=dim)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToIsoWkt() != wkt_after:
            print(wkt_before)
            pytest.fail(dim)


###############################################################################
# Test gdal.VectorTranslate(dst_ds, ...) without accessMode specified (#6612)


def test_ogr2ogr_lib_17():

    ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx('../ogr/data/poly.shp'))
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None

###############################################################################
# Test -limit


def test_ogr2ogr_lib_18():

    ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx('../ogr/data/poly.shp'), limit=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

###############################################################################
# Test -addFields + -select


def test_ogr2ogr_lib_19():

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    lyr = src_ds.CreateLayer('layer')
    lyr.CreateField(ogr.FieldDefn('foo'))
    lyr.CreateField(ogr.FieldDefn('bar'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = 'bar'
    f['bar'] = 'foo'
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate('', src_ds, format='Memory', selectFields=['foo'])
    gdal.VectorTranslate(ds, src_ds, accessMode='append', addFields=True, selectFields=['bar'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar' or f.IsFieldSet('bar'):
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f['bar'] != 'foo' or f.IsFieldSet('foo'):
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test preservation of source geometry field name

def test_ogr2ogr_lib_20():

    if ogr.GetDriverByName('GPKG') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    lyr = src_ds.CreateLayer('layer', geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn('foo'))

    ds = gdal.VectorTranslate('/vsimem/out.gpkg', src_ds, format='GPKG')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == 'foo'
    ds = None
    gdal.Unlink('/vsimem/out.gpkg')

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    lyr = src_ds.CreateLayer('layer', geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn('foo'))
    lyr.CreateGeomField(ogr.GeomFieldDefn('bar'))

    ds = gdal.VectorTranslate('/vsimem/out.gpkg', src_ds, format='GPKG', selectFields=['bar'])
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == 'bar'
    ds = None
    gdal.Unlink('/vsimem/out.gpkg')


###############################################################################
# Verify -append and -select options are an invalid combination

def test_ogr2ogr_lib_21():

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    lyr = src_ds.CreateLayer('layer')
    lyr.CreateField(ogr.FieldDefn('foo'))
    lyr.CreateField(ogr.FieldDefn('bar'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = 'bar'
    f['bar'] = 'foo'
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate('', src_ds, format='Memory')
    with gdaltest.error_handler():
        gdal.VectorTranslate(ds, src_ds, accessMode='append',
                             selectFields=['foo'])

    ds = None
    f.Destroy()
    src_ds = None

    assert gdal.GetLastErrorNo() == gdalconst.CPLE_IllegalArg, \
        'expected use of -select and -append together to be invalid'


###############################################################################


def test_ogr2ogr_clipsrc_no_dst_geom():

    if not ogrtest.have_geos():
        pytest.skip()

    tmpfilename = '/vsimem/out.csv'
    wkt = 'POLYGON ((479461 4764494,479461 4764196,480012 4764196,480012 4764494,479461 4764494))'
    ds = gdal.VectorTranslate(tmpfilename, '../ogr/data/poly.shp',
                              options='-f CSV -clipsrc "%s"' % wkt)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 1
    ds = None

    gdal.Unlink(tmpfilename)


###############################################################################
# Check that ogr2ogr does data axis to CRS axis mapping adaptations in case
# of the output driver not following the mapping of the input dataset.

def test_ogr2ogr_axis_mapping_swap():

    gdal.FileFromMemBuffer("/vsimem/test_ogr2ogr_axis_mapping_swap.gml",
"""<ogr:FeatureCollection
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ out.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml">
  <ogr:featureMember>
    <ogr:test gml:id="test.0">
      <ogr:geometryProperty><gml:Point srsName="urn:ogc:def:crs:EPSG::4326">
        <gml:pos>49 2</gml:pos></gml:Point></ogr:geometryProperty>
    </ogr:test>
  </ogr:featureMember>
</ogr:FeatureCollection>""")
    gdal.FileFromMemBuffer("/vsimem/test_ogr2ogr_axis_mapping_swap.gfs",
""""<GMLFeatureClassList>
  <GMLFeatureClass>
    <Name>test</Name>
    <ElementPath>test</ElementPath>
    <SRSName>urn:ogc:def:crs:EPSG::4326</SRSName>
  </GMLFeatureClass>
</GMLFeatureClassList>""")

    ds = gdal.OpenEx('/vsimem/test_ogr2ogr_axis_mapping_swap.gml',
                     open_options = ['INVERT_AXIS_ORDER_IF_LAT_LONG=NO'])
    if ds is None:
        gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gml")
        gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gfs")
        pytest.skip()
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [1,2]
    ds = None
    ds = gdal.VectorTranslate('/vsimem/test_ogr2ogr_axis_mapping_swap.shp',
                              '/vsimem/test_ogr2ogr_axis_mapping_swap.gml')
    gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gml")
    gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gfs")

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    ret = ogrtest.check_feature_geometry(feat, "POINT (2 49)")
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource(
        '/vsimem/test_ogr2ogr_axis_mapping_swap.shp')

    assert ret == 0


###############################################################################
# Test -ct

def test_ogr2ogr_lib_ct():

    ds = gdal.VectorTranslate('', '../ogr/data/poly.shp',
                              format='Memory', dstSRS='EPSG:32630',
                              reproject=True,
                              coordinateOperation="+proj=affine +s11=-1")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    #f.DumpReadable()
    assert ogrtest.check_feature_geometry(f, "POLYGON ((-479819.84375 4765180.5,-479690.1875 4765259.5,-479647.0 4765369.5,-479730.375 4765400.5,-480039.03125 4765539.5,-480035.34375 4765558.5,-480159.78125 4765610.5,-480202.28125 4765482.0,-480365.0 4765015.5,-480389.6875 4764950.0,-480133.96875 4764856.5,-480080.28125 4764979.5,-480082.96875 4765049.5,-480088.8125 4765139.5,-480059.90625 4765239.5,-480019.71875 4765319.5,-479980.21875 4765409.5,-479909.875 4765370.0,-479859.875 4765270.0,-479819.84375 4765180.5))") == 0


###############################################################################
# Test -ct without SRS specification

def test_ogr2ogr_lib_ct_no_srs():

    ds = gdal.VectorTranslate('', '../ogr/data/poly.shp',
                              format='Memory',
                              coordinateOperation="+proj=affine +s11=-1")
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == '27700'
    f = lyr.GetNextFeature()
    #f.DumpReadable()
    assert ogrtest.check_feature_geometry(f, "POLYGON ((-479819.84375 4765180.5,-479690.1875 4765259.5,-479647.0 4765369.5,-479730.375 4765400.5,-480039.03125 4765539.5,-480035.34375 4765558.5,-480159.78125 4765610.5,-480202.28125 4765482.0,-480365.0 4765015.5,-480389.6875 4764950.0,-480133.96875 4764856.5,-480080.28125 4764979.5,-480082.96875 4765049.5,-480088.8125 4765139.5,-480059.90625 4765239.5,-480019.71875 4765319.5,-479980.21875 4765409.5,-479909.875 4765370.0,-479859.875 4765270.0,-479819.84375 4765180.5))") == 0


###############################################################################
# Test -nlt CONVERT_TO_LINEAR -nlt PROMOTE_TO_MULTI

@pytest.mark.parametrize('geometryType',
                         [
                            ['PROMOTE_TO_MULTI', 'CONVERT_TO_LINEAR'],
                            ['CONVERT_TO_LINEAR', 'PROMOTE_TO_MULTI']
                         ])
def test_ogr2ogr_lib_convert_to_linear_promote_to_multi(geometryType):

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    lyr = src_ds.CreateLayer('layer')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate('', src_ds,
                              format='Memory',
                              geometryType=geometryType)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbMultiLineString


###############################################################################
# Test -makevalid

def test_ogr2ogr_lib_makevalid():

    # Check if MakeValid() is available
    g = ogr.CreateGeometryFromWkt('POLYGON ((0 0,10 10,0 10,10 0,0 0))')
    with gdaltest.error_handler():
        make_valid_available = g.MakeValid() is not None

    tmpfilename = '/vsimem/tmp.csv'
    with gdaltest.tempfile(tmpfilename,"""id,WKT
1,"POLYGON ((0 0,10 10,0 10,10 0,0 0))"
2,"POLYGON ((0 0,0 1,0.5 1,0.5 0.75,0.5 1,1 1,1 0,0 0))"
"""):
        if make_valid_available:
            ds = gdal.VectorTranslate('', tmpfilename, format='Memory', makeValid=True)
        else:
            with gdaltest.error_handler():
                with pytest.raises(Exception):
                    gdal.VectorTranslate('', tmpfilename, format='Memory', makeValid=True)
                return

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((0 0,5 5,10 0,0 0)),((5 5,0 10,10 10,5 5)))") == 0
    f = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(f, "POLYGON ((0 0,0 1,0.5 1.0,1 1,1 0,0 0))") == 0



###############################################################################
# Test SQLStatement with -sql @filename syntax


def test_ogr2ogr_lib_sql_filename():

    with gdaltest.tempfile('/vsimem/my.sql', """-- initial comment\nselect\n'--''--' as literalfield,* from --comment\npoly\n-- trailing comment"""):
        ds = gdal.VectorTranslate('', '../ogr/data/poly.shp', options = '-f Memory -sql @/vsimem/my.sql')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    assert lyr.GetLayerDefn().GetFieldIndex('literalfield') == 0


###############################################################################
# Verify -emptyStrAsNull

def test_ogr2ogr_emptyStrAsNull():

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    lyr = src_ds.CreateLayer('layer')
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = ''
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate('', src_ds, format='Memory', emptyStrAsNull=True)

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()

    assert f['foo'] is None, 'expected empty string to be transformed to null'
