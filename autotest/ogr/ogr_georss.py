#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id: ogr_georss.py 15604 2008-10-26 11:21:34Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GeoRSS driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import os


import gdaltest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import pytest

pytestmark = pytest.mark.require_driver('GeoRSS')

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    ds = ogr.Open('data/georss/atom_rfc_sample.xml')
    if ds is None:
        gdaltest.georss_read_support = 0
    else:
        gdaltest.georss_read_support = 1
        ds = None

    gdaltest.have_gml_reader = 0
    try:
        ds = ogr.Open('data/gml/ionic_wfs.gml')
        if ds is not None:
            gdaltest.have_gml_reader = 1
            ds = None
    except:
        pass

    gdaltest.atom_field_values = [('title', 'Atom draft-07 snapshot', ogr.OFTString),
                                  ('link_rel', 'alternate', ogr.OFTString),
                                  ('link_type', 'text/html', ogr.OFTString),
                                  ('link_href', 'http://example.org/2005/04/02/atom', ogr.OFTString),
                                  ('link2_rel', 'enclosure', ogr.OFTString),
                                  ('link2_type', 'audio/mpeg', ogr.OFTString),
                                  ('link2_length', '1337', ogr.OFTInteger),
                                  ('link2_href', 'http://example.org/audio/ph34r_my_podcast.mp3', ogr.OFTString),
                                  ('id', 'tag:example.org,2003:3.2397', ogr.OFTString),
                                  ('updated', '2005/07/31 12:29:29+00', ogr.OFTDateTime),
                                  ('published', '2003/12/13 08:29:29-04', ogr.OFTDateTime),
                                  ('author_name', 'Mark Pilgrim', ogr.OFTString),
                                  ('author_uri', 'http://example.org/', ogr.OFTString),
                                  ('author_email', 'f8dy@example.com', ogr.OFTString),
                                  ('contributor_name', 'Sam Ruby', ogr.OFTString),
                                  ('contributor2_name', 'Joe Gregorio', ogr.OFTString),
                                  ('content_type', 'xhtml', ogr.OFTString),
                                  ('content_xml_lang', 'en', ogr.OFTString),
                                  ('content_xml_base', 'http://diveintomark.org/', ogr.OFTString)]

    yield

    list_files = ['tmp/test_rss2.xml', 'tmp/test_atom.xml', 'tmp/test32631.rss', 'tmp/broken.rss', 'tmp/nonstandard.rss']
    for filename in list_files:
        try:
            os.remove(filename)
        except OSError:
            pass

    files = os.listdir('data')
    for filename in files:
        if len(filename) > 13 and filename[-13:] == '.resolved.gml':
            os.unlink('data/georss/' + filename)


###############################################################################
# Used by ogr_georss_1 and ogr_georss_1ter


def ogr_georss_test_atom(filename):

    if not gdaltest.georss_read_support:
        pytest.skip()

    ds = ogr.Open(filename)
    lyr = ds.GetLayerByName('georss')

    assert lyr.GetSpatialRef() is None, 'No spatial ref expected'

    feat = lyr.GetNextFeature()

    for field_value in gdaltest.atom_field_values:
        assert feat.GetFieldAsString(field_value[0]) == field_value[1], \
            ('For field "%s", got "%s" instead of "%s"' % (field_value[0], feat.GetFieldAsString(field_value[0]), field_value[1]))

    assert feat.GetFieldAsString('content').find('<div xmlns="http://www.w3.org/1999/xhtml">') != -1, \
        ('For field "%s", got "%s"' % ('content', feat.GetFieldAsString('content')))

###############################################################################
# Test reading an ATOM document without any geometry


def test_ogr_georss_1():

    return ogr_georss_test_atom('data/georss/atom_rfc_sample.xml')

###############################################################################
# Test reading an ATOM document with atom: prefiw


def test_ogr_georss_1_atom_ns():

    return ogr_georss_test_atom('data/georss/atom_rfc_sample_atom_ns.xml')

###############################################################################
# Test writing a Atom 1.0 document (doesn't need read support)


def test_ogr_georss_1bis():

    try:
        os.remove('tmp/test_atom.xml')
    except OSError:
        pass

    ds = ogr.GetDriverByName('GeoRSS').CreateDataSource('tmp/test_atom.xml', options=['FORMAT=ATOM'])
    lyr = ds.CreateLayer('georss')

    for field_value in gdaltest.atom_field_values:
        lyr.CreateField(ogr.FieldDefn(field_value[0], field_value[2]))
    lyr.CreateField(ogr.FieldDefn('content', ogr.OFTString))

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    for field_value in gdaltest.atom_field_values:
        dst_feat.SetField(field_value[0], field_value[1])
    dst_feat.SetField('content', '<div xmlns="http://www.w3.org/1999/xhtml"><p><i>[Update: The Atom draft is finished.]</i></p></div>')

    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    ds = None


###############################################################################
# Test reading document created at previous step

def test_ogr_georss_1ter():

    return ogr_georss_test_atom('tmp/test_atom.xml')


###############################################################################
# Common for ogr_georss_2 and ogr_georss_3

def ogr_georss_test_rss(filename, only_first_feature):

    if not gdaltest.georss_read_support:
        pytest.skip()

    ds = ogr.Open(filename)
    assert ds is not None

    lyr = ds.GetLayer(0)

    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')

    assert lyr.GetSpatialRef() is not None and lyr.GetSpatialRef().IsSame(srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), \
        'SRS is not the one expected.'

    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2, 1]

    feat = lyr.GetNextFeature()
    expected_wkt = 'POINT (2 49)'
    assert feat.GetGeometryRef().ExportToWkt() == expected_wkt, \
        ('%s' % feat.GetGeometryRef().ExportToWkt())
    assert feat.GetFieldAsString('title') == 'A point'
    assert feat.GetFieldAsString('author') == 'Author'
    assert feat.GetFieldAsString('link') == 'http://gdal.org'
    assert feat.GetFieldAsString('pubDate') == '2008/12/07 20:13:00+02'
    assert feat.GetFieldAsString('category') == 'First category'
    assert feat.GetFieldAsString('category_domain') == 'first_domain'
    assert feat.GetFieldAsString('category2') == 'Second category'
    assert feat.GetFieldAsString('category2_domain') == 'second_domain'

    feat = lyr.GetNextFeature()
    expected_wkt = 'LINESTRING (2 48,2.1 48.1,2.2 48.0)'
    assert only_first_feature is not False or feat.GetGeometryRef().ExportToWkt() == expected_wkt, \
        ('%s' % feat.GetGeometryRef().ExportToWkt())
    assert feat.GetFieldAsString('title') == 'A line'

    feat = lyr.GetNextFeature()
    expected_wkt = 'POLYGON ((2 50,2.1 50.1,2.2 48.1,2.1 46.1,2 50))'
    assert only_first_feature is not False or feat.GetGeometryRef().ExportToWkt() == expected_wkt, \
        ('%s' % feat.GetGeometryRef().ExportToWkt())
    assert feat.GetFieldAsString('title') == 'A polygon'

    feat = lyr.GetNextFeature()
    expected_wkt = 'POLYGON ((2 49,2.0 49.5,2.2 49.5,2.2 49.0,2 49))'
    assert only_first_feature is not False or feat.GetGeometryRef().ExportToWkt() == expected_wkt, \
        ('%s' % feat.GetGeometryRef().ExportToWkt())
    assert feat.GetFieldAsString('title') == 'A box'

###############################################################################
# Test reading a RSS 2.0 document with GeoRSS simple geometries


def test_ogr_georss_2():

    return ogr_georss_test_rss('data/georss/test_georss_simple.xml', False)

###############################################################################
# Test reading a RSS 2.0 document with GeoRSS GML geometries


def test_ogr_georss_3():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    return ogr_georss_test_rss('data/georss/test_georss_gml.xml', False)

###############################################################################
# Test writing a RSS 2.0 document (doesn't need read support)


def ogr_georss_create(filename, options):

    try:
        os.remove(filename)
    except OSError:
        pass
    ds = ogr.GetDriverByName('GeoRSS').CreateDataSource(filename, options=options)
    lyr = ds.CreateLayer('georss')

    lyr.CreateField(ogr.FieldDefn('title', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('author', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('link', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('pubDate', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('description', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('category', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('category_domain', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('category2', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('category2_domain', ogr.OFTString))

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetField('title', 'A point')
    dst_feat.SetField('author', 'Author')
    dst_feat.SetField('link', 'http://gdal.org')
    dst_feat.SetField('pubDate', '2008/12/07 20:13:00+02')
    dst_feat.SetField('category', 'First category')
    dst_feat.SetField('category_domain', 'first_domain')
    dst_feat.SetField('category2', 'Second category')
    dst_feat.SetField('category2_domain', 'second_domain')
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))

    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetField('title', 'A line')
    dst_feat.SetField('author', 'Author')
    dst_feat.SetField('link', 'http://gdal.org')
    dst_feat.SetField('pubDate', '2008/12/07 20:13:00+02')
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (2 48,2.1 48.1,2.2 48.0)'))

    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetField('title', 'A polygon')
    dst_feat.SetField('author', 'Author')
    dst_feat.SetField('link', 'http://gdal.org')
    dst_feat.SetField('pubDate', '2008/12/07 20:13:00+02')
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((2 50,2.1 50.1,2.2 48.1,2.1 46.1,2 50))'))

    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetField('title', 'A box')
    dst_feat.SetField('author', 'Author')
    dst_feat.SetField('link', 'http://gdal.org')
    dst_feat.SetField('pubDate', '2008/12/07 20:13:00+02')
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((2 49,2.0 49.5,2.2 49.5,2.2 49.0,2 49))'))

    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    ds = None

###############################################################################
# Test writing a RSS 2.0 document in Simple dialect (doesn't need read support)


def test_ogr_georss_4():

    ogr_georss_create('tmp/test_rss2.xml', [])

    content = open('tmp/test_rss2.xml').read()
    assert content.find('<georss:point>49 2') != -1, ('%s' % content)

###############################################################################
# Test reading document created at previous step


def test_ogr_georss_5():

    return ogr_georss_test_rss('tmp/test_rss2.xml', False)

###############################################################################
# Test writing a RSS 2.0 document in GML dialect (doesn't need read support)


def test_ogr_georss_6():

    ogr_georss_create('tmp/test_rss2.xml', ['GEOM_DIALECT=GML'])

    content = open('tmp/test_rss2.xml').read()
    assert content.find('<georss:where><gml:Point><gml:pos>49 2') != -1, \
        ('%s' % content)

###############################################################################
# Test reading document created at previous step


def test_ogr_georss_7():
    if not gdaltest.have_gml_reader:
        pytest.skip()

    return ogr_georss_test_rss('tmp/test_rss2.xml', False)

###############################################################################
# Test writing a RSS 2.0 document in W3C Geo dialect (doesn't need read support)


def test_ogr_georss_8():

    ogr_georss_create('tmp/test_rss2.xml', ['GEOM_DIALECT=W3C_GEO'])

    content = open('tmp/test_rss2.xml').read()
    assert not (content.find('<geo:lat>49') == -1 or content.find('<geo:long>2') == -1), \
        ('%s' % content)

###############################################################################
# Test reading document created at previous step


def test_ogr_georss_9():

    return ogr_georss_test_rss('tmp/test_rss2.xml', True)

###############################################################################
# Test writing a RSS 2.0 document in GML dialect with EPSG:32631


def test_ogr_georss_10():
    try:
        os.remove('tmp/test32631.rss')
    except OSError:
        pass

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)

    ds = ogr.GetDriverByName('GeoRSS').CreateDataSource('tmp/test32631.rss')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        lyr = ds.CreateLayer('georss', srs=srs)
    except:
        lyr = None
    gdal.PopErrorHandler()
    assert lyr is None, 'should not have accepted EPSG:32631 with GEOM_DIALECT != GML'

    ds = None

    try:
        os.remove('tmp/test32631.rss')
    except OSError:
        pass

    ds = ogr.GetDriverByName('GeoRSS').CreateDataSource('tmp/test32631.rss', options=['GEOM_DIALECT=GML'])
    lyr = ds.CreateLayer('georss', srs=srs)

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (500000 4000000)'))

    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    ds = None

    content = open('tmp/test32631.rss').read()
    assert content.find('<georss:where><gml:Point srsName="urn:ogc:def:crs:EPSG::32631"><gml:pos>500000 4000000') != -1, \
        ('%s' % content)

###############################################################################
# Test reading document created at previous step


def test_ogr_georss_11():

    if not gdaltest.georss_read_support:
        pytest.skip()
    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('tmp/test32631.rss')
    lyr = ds.GetLayer(0)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)

    assert lyr.GetSpatialRef() is not None and lyr.GetSpatialRef().IsSame(srs), \
        'SRS is not the one expected.'

    if lyr.GetSpatialRef().ExportToWkt().find('AXIS["Easting",EAST],AXIS["Northing",NORTH]') == -1:
        print(('%s' % lyr.GetSpatialRef().ExportToWkt()))
        pytest.fail('AXIS definition expected is AXIS["Easting",EAST],AXIS["Northing",NORTH]!')

    feat = lyr.GetNextFeature()
    expected_wkt = 'POINT (500000 4000000)'
    assert feat.GetGeometryRef().ExportToWkt() == expected_wkt, \
        ('%s' % feat.GetGeometryRef().ExportToWkt())

###############################################################################
# Test various broken documents


def test_ogr_georss_12():

    if not gdaltest.georss_read_support:
        pytest.skip()

    open('tmp/broken.rss', 'wt').write('<?xml version="1.0"?><rss><item><a></item></rss>')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('tmp/broken.rss')
    gdal.PopErrorHandler()
    assert ds is None

    open('tmp/broken.rss', 'wt').write('<?xml version="1.0"?><rss><channel><item><georss:box>49 2 49.5</georss:box></item></channel></rss>')
    ds = ogr.Open('tmp/broken.rss')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = ds.GetLayer(0).GetNextFeature()
    gdal.PopErrorHandler()
    assert feat.GetGeometryRef() is None

    open('tmp/broken.rss', 'wt').write('<?xml version="1.0"?><rss><channel><item><georss:where><gml:LineString><gml:posList>48 2 48.1 2.1 48</gml:posList></gml:LineString></georss:where></item></channel></rss>')
    ds = ogr.Open('tmp/broken.rss')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = ds.GetLayer(0).GetNextFeature()
    gdal.PopErrorHandler()
    assert feat.GetGeometryRef() is None

###############################################################################
# Test writing non standard fields


def test_ogr_georss_13():
    try:
        os.remove('tmp/nonstandard.rss')
    except OSError:
        pass
    ds = ogr.GetDriverByName('GeoRSS').CreateDataSource('tmp/nonstandard.rss', options=['USE_EXTENSIONS=YES'])
    lyr = ds.CreateLayer('georss')

    lyr.CreateField(ogr.FieldDefn('myns_field', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('field2', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('ogr_field3', ogr.OFTString))

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetField('myns_field', 'val')
    dst_feat.SetField('field2', 'val2')
    dst_feat.SetField('ogr_field3', 'val3')

    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    ds = None

    content = open('tmp/nonstandard.rss').read()
    assert content.find('<myns:field>val</myns:field>') != -1, ('%s' % content)
    assert content.find('<ogr:field2>val2</ogr:field2>') != -1, ('%s' % content)
    assert content.find('<ogr:field3>val3</ogr:field3>') != -1, ('%s' % content)

###############################################################################
# Test reading document created at previous step


def test_ogr_georss_14():

    if not gdaltest.georss_read_support:
        pytest.skip()

    ds = ogr.Open('tmp/nonstandard.rss')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    assert feat.GetFieldAsString('myns_field') == 'val', \
        ('Expected %s. Got %s' % ('val', feat.GetFieldAsString('myns_field')))
    assert feat.GetFieldAsString('ogr_field2') == 'val2', \
        ('Expected %s. Got %s' % ('val2', feat.GetFieldAsString('ogr_field2')))
    assert feat.GetFieldAsString('ogr_field3') == 'val3', \
        ('Expected %s. Got %s' % ('val3', feat.GetFieldAsString('ogr_field3')))

###############################################################################
# Test reading an in memory file (#2931)


def test_ogr_georss_15():

    if not gdaltest.georss_read_support:
        pytest.skip()

    try:
        gdal.FileFromMemBuffer
    except AttributeError:
        pytest.skip()

    content = """<?xml version="1.0" encoding="UTF-8"?>
    <rss version="2.0" xmlns:georss="http://www.georss.org/georss" xmlns:gml="http://www.opengis.net/gml">
    <channel>
        <link>http://mylink.com</link>
        <title>channel title</title>
        <item>
            <guid isPermaLink="false">0</guid>
            <pubDate>Thu, 2 Apr 2009 23:03:00 +0000</pubDate>
            <title>item title</title>
            <georss:point>49 2</georss:point>
        </item>
    </channel>
    </rss>"""

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/georssinmem', content)

    ds = ogr.Open('/vsimem/georssinmem')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    assert feat.GetFieldAsString('title') == 'item title', \
        ('Expected %s. Got %s' % ('item title', feat.GetFieldAsString('title')))

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/georssinmem')
