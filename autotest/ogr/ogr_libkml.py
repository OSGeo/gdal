#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  LIBKML Driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import pytest

pytestmark = pytest.mark.require_driver('LIBKML')


###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    kml_drv = ogr.GetDriverByName('KML')
    # Unregister KML driver if present as it's behaviour is not identical
    # to new LIBKML driver
    if kml_drv is not None:
        print('Unregister KML driver')
        kml_drv.Deregister()

    ogrtest.have_read_libkml = ogr.Open('data/kml/samples.kml') is not None

    yield

    gdal.Unlink('/vsimem/libkml.kml')
    gdal.Unlink('/vsimem/libkml.kmz')
    gdal.Unlink('/vsimem/libkml_use_doc_off.kmz')
    gdal.Unlink("/vsimem/ogr_libkml_camera.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_layer_lookat.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_layer_camera.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_multigeometry.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_snippet.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_atom_author.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_atom_link.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_phonenumber.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_region.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_screenoverlay.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_model.kml")
    gdal.Unlink("/vsimem/ogr_libkml_read_write_style_read.kml")
    gdal.Unlink("/vsimem/ogr_libkml_read_write_style_write.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_update.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_update.kmz")
    gdal.Unlink("/vsimem/ogr_libkml_write_update_dir/doc.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_update_dir")
    gdal.Unlink("/vsimem/ogr_libkml_write_networklinkcontrol.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_networklinkcontrol.kmz")
    gdal.Unlink("/vsimem/ogr_libkml_write_networklinkcontrol_dir/doc.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_networklinkcontrol_dir")
    gdal.Unlink("/vsimem/ogr_libkml_write_liststyle.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_networklink.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_photooverlay.kml")
    gdal.Unlink("/vsimem/ogr_libkml_read_write_data.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_folder.kml")
    gdal.Unlink("/vsimem/ogr_libkml_write_container_properties.kml")

    # Re-register LIBKML driver if necessary
    if kml_drv is not None:
        print('Re-register KML driver')
        kml_drv.Register()

###############################################################################
# Test reading attributes for first layer (point).
#


def test_ogr_libkml_attributes_1():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    kml_ds = ogr.Open('data/kml/samples.kml')

    lyr = kml_ds.GetLayerByName('Placemarks')
    feat = lyr.GetNextFeature()

    assert feat.GetField('Name') == 'Simple placemark', 'Wrong name field value'

    if feat.GetField('description')[:23] != 'Attached to the ground.':
        print('got: ', feat.GetField('description')[:23])
        pytest.fail('Wrong description field value')

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    assert feat.GetField('Name') == 'Floating placemark', 'Wrong name field value'

    if feat.GetField('description')[:25] != 'Floats a defined distance':
        print('got: ', feat.GetField('description')[:25])
        pytest.fail('Wrong description field value')

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    assert feat.GetField('Name') == 'Extruded placemark', 'Wrong name field value'

    if feat.GetField('description') != 'Tethered to the ground by a customizable\n          \"tail\"':
        print('got: ', feat.GetField('description'))
        pytest.fail('Wrong description field value')


###############################################################################
# Test reading attributes for another layer (point).
#


def test_ogr_libkml_attributes_2():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    kml_ds = ogr.Open('data/kml/samples.kml')

    lyr = kml_ds.GetLayerByName('Highlighted Icon')
    feat = lyr.GetNextFeature()

    assert feat.GetField('Name') == 'Roll over this icon', 'Wrong name field value'

    if feat.GetField('description') is not None:
        print("'%s'" % feat.GetField('description'))
        pytest.fail('Wrong description field value')

    feat = lyr.GetNextFeature()
    assert feat is None, 'unexpected feature found.'

###############################################################################
# Test reading attributes for another layer (linestring).
#


def test_ogr_libkml_attributes_3():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    kml_ds = ogr.Open('data/kml/samples.kml')

    lyr = kml_ds.GetLayerByName('Paths')
    feat = lyr.GetNextFeature()

    assert feat.GetField('Name') == 'Tessellated', 'Wrong name field value'

    assert feat.GetField('description') == 'If the <tessellate> tag has a value of 1, the line will contour to the underlying terrain', \
        'Wrong description field value'

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    assert feat.GetField('Name') == 'Untessellated', 'Wrong name field value'

    assert feat.GetField('description') == 'If the <tessellate> tag has a value of 0, the line follow a simple straight-line path from point to point', \
        'Wrong description field value'

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

###############################################################################
# Test reading attributes for another layer (polygon).
#


def test_ogr_libkml_attributes_4():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    kml_ds = ogr.Open('data/kml/samples.kml')

    lyr = kml_ds.GetLayerByName('Google Campus')
    feat = lyr.GetNextFeature()

    i = 40
    while feat is not None:
        name = 'Building %d' % i
        if feat.GetField('Name') != name:
            print('Got: "%s"' % feat.GetField('name'))
            pytest.fail('Wrong name field value')

        assert feat.GetField('description') is None, 'Wrong description field value'

        i = i + 1
        feat = lyr.GetNextFeature()


###############################################################################
# Test reading of KML point geometry
#


def test_ogr_libkml_point_read():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    kml_ds = ogr.Open('data/kml/samples.kml')

    lyr = kml_ds.GetLayerByName('Placemarks')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    wkt = 'POINT(-122.0822035425683 37.42228990140251)'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    wkt = 'POINT(-122.084075 37.4220033612141 50)'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    wkt = 'POINT(-122.0857667006183 37.42156927867553 50)'

    assert not ogrtest.check_feature_geometry(feat, wkt)

###############################################################################
# Test reading of KML linestring geometry
#


def test_ogr_libkml_linestring_read():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    kml_ds = ogr.Open('data/kml/samples.kml')

    lyr = kml_ds.GetLayerByName('Paths')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    wkt = 'LINESTRING (-112.081423783034495 36.106778704771372 0, -112.087026775269294 36.0905099328766 0)'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    wkt = 'LINESTRING (-112.080622229594994 36.106734600079953 0,-112.085242575314993 36.090495986124218 0)'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    wkt = 'LINESTRING (-112.265654928602004 36.094476726025462 2357,-112.266038452823807 36.093426088386707 2357,-112.266813901345301 36.092510587768807 2357,-112.267782683444494 36.091898273579957 2357,-112.268855751095202 36.091313794118697 2357,-112.269481071721899 36.090367720752099 2357,-112.269526855561097 36.089321714872852 2357,-112.269014456727604 36.088509160604723 2357,-112.268152881533894 36.087538135979557 2357,-112.2670588176031 36.086826852625677 2357,-112.265737458732104 36.086463123013033 2357)'
    assert not ogrtest.check_feature_geometry(feat, wkt)

###############################################################################
# Test reading of KML polygon geometry
#


def test_ogr_libkml_polygon_read():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    kml_ds = ogr.Open('data/kml/samples.kml')

    lyr = kml_ds.GetLayerByName('Google Campus')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    wkt = 'POLYGON ((-122.084893845961204 37.422571240447859 17,-122.084958097919795 37.422119226268563 17,-122.084746957304702 37.42207183952619 17,-122.084572538096197 37.422090067296757 17,-122.084595488672306 37.422159327008949 17,-122.0838521118269 37.422272785643713 17,-122.083792243334997 37.422035391120843 17,-122.0835076656616 37.422090069571063 17,-122.083470946415204 37.422009873951609 17,-122.083122108574798 37.422104649494599 17,-122.082924737457205 37.422265039903863 17,-122.082933916938501 37.422312428430942 17,-122.083383735973698 37.422250460876178 17,-122.083360785424802 37.422341592287452 17,-122.083420455164202 37.42237075460644 17,-122.083659133885007 37.422512920110009 17,-122.083975843895203 37.422658730937812 17,-122.084237474333094 37.422651439725207 17,-122.0845036949503 37.422651438643499 17,-122.0848020460801 37.422611339163147 17,-122.084788275051494 37.422563950551208 17,-122.084893845961204 37.422571240447859 17))'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    wkt = 'POLYGON ((-122.085741277148301 37.422270331552568 17,-122.085816976848093 37.422314088323461 17,-122.085852582875006 37.422303374697442 17,-122.085879994563896 37.422256861387893 17,-122.085886010140896 37.422231107613797 17,-122.085806915728796 37.422202501738553 17,-122.085837954265301 37.42214027058678 17,-122.085673264051906 37.422086902144081 17,-122.085602292640701 37.42214885429042 17,-122.085590277843593 37.422128290487002 17,-122.085584167223701 37.422081719672462 17,-122.085485206574106 37.42210455874995 17,-122.085506726435199 37.422142679498243 17,-122.085443071291493 37.422127838461719 17,-122.085099071490404 37.42251282407603 17,-122.085676981863202 37.422818153236513 17,-122.086016227378295 37.422449188587223 17,-122.085726032700407 37.422292396042529 17,-122.085741277148301 37.422270331552568 17))'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    wkt = 'POLYGON ((-122.085786228724203 37.421362088869692 25,-122.085731299060299 37.421369359894811 25,-122.085731299291794 37.421409349109027 25,-122.085607707367899 37.421383901665649 25,-122.085580242651602 37.42137299550869 25,-122.085218622197104 37.421372995043157 25,-122.085227776563897 37.421616565082651 25,-122.085259818934702 37.421605658944031 25,-122.085259818549901 37.421682001560001 25,-122.085236931147804 37.421700178603459 25,-122.085264395782801 37.421761979825753 25,-122.085323903274599 37.421761980139067 25,-122.085355945432397 37.421852864451999 25,-122.085410875246296 37.421889218237339 25,-122.085479537935697 37.42189285337048 25,-122.085543622981902 37.421889217975462 25,-122.085626017804202 37.421860134999257 25,-122.085937287963006 37.421860134536047 25,-122.085942871866607 37.42160898590042 25,-122.085965546986102 37.421579927591438 25,-122.085864046234093 37.421471150029568 25,-122.0858548911215 37.421405713261841 25,-122.085809116276806 37.4214057134039 25,-122.085786228724203 37.421362088869692 25))'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'expected feature not found.'

    wkt = 'POLYGON ((-122.084437112828397 37.421772530030907 19,-122.084511885574599 37.421911115428962 19,-122.0850470999805 37.421787551215353 19,-122.085071991339106 37.421436630231611 19,-122.084916406231997 37.421372378221157 19,-122.084219386816699 37.421372378016258 19,-122.084219386589993 37.421476171614962 19,-122.083808641999099 37.4214613409357 19,-122.083789972856394 37.421313064107963 19,-122.083279653469802 37.421293288405927 19,-122.083260981920702 37.421392139442979 19,-122.082937362173695 37.421372363998763 19,-122.082906242566693 37.421515697788713 19,-122.082850226966499 37.421762825764652 19,-122.082943578863507 37.421767769696352 19,-122.083217411188002 37.421792485526858 19,-122.0835970430103 37.421748007445601 19,-122.083945555677104 37.421693642376027 19,-122.084007789463698 37.421762838158529 19,-122.084113587521003 37.421748011043917 19,-122.084076247378405 37.421713412923751 19,-122.084144704773905 37.421678815345693 19,-122.084144704222993 37.421817206601972 19,-122.084250333307395 37.421817070044597 19,-122.084437112828397 37.421772530030907 19))'
    assert not ogrtest.check_feature_geometry(feat, wkt)

###############################################################################
# Write test


def ogr_libkml_write(filename):

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource(filename)

    if filename != '/vsimem/libkml_use_doc_off.kmz':
        srs = osr.SpatialReference()
        srs.SetWellKnownGeogCS('WGS72')
        lyr = ds.CreateLayer('test_wgs72', srs=srs)

        assert lyr.TestCapability(ogr.OLCSequentialWrite) == 1
        assert lyr.TestCapability(ogr.OLCRandomWrite) == 0

        dst_feat = ogr.Feature(lyr.GetLayerDefn())
        dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
        assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'
        assert dst_feat.GetGeometryRef().ExportToWkt() == 'POINT (2 49)', \
            'CreateFeature changed the geometry.'

    lyr = ds.CreateLayer('test_wgs84')

    fielddefn = ogr.FieldDefn('name', ogr.OFTString)
    lyr.CreateField(fielddefn)
    fielddefn = ogr.FieldDefn('description', ogr.OFTString)
    lyr.CreateField(fielddefn)
    fielddefn = ogr.FieldDefn('foo', ogr.OFTString)
    lyr.CreateField(fielddefn)

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('name', 'my_name')
    dst_feat.SetField('description', 'my_description')
    dst_feat.SetField('foo', 'bar')
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49 1)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (0 1,2 3)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0),(0.25 0.25 0,0.25 0.75 0,0.75 0.75 0,0.75 0.25 0,0.25 0.25 0))'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT (2 49,2 49)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 1,2 3),(0 1,2 3))'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0),(0.25 0.25 0,0.25 0.75 0,0.75 0.75 0,0.75 0.25 0,0.25 0.25 0)),((-0.25 0.25 0,-0.25 0.75 0,-0.75 0.75 0,-0.75 0.25 0,-0.25 0.25 0)))'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (2 49 1),LINESTRING (0 1,2 3))'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    ds = None

###############################################################################
# Check previous test


def ogr_libkml_check_write(filename):

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open(filename)
    if filename != '/vsimem/libkml_use_doc_off.kmz':
        lyr = ds.GetLayerByName('test_wgs84')
    else:
        lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 8, 'Bad feature count.'

    feat = lyr.GetNextFeature()
    assert feat.GetField('name') == 'my_name', 'Unexpected name.'
    assert feat.GetField('description') == 'my_description', 'Unexpected description.'
    assert feat.GetField('foo') == 'bar', 'Unexpected foo.'
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (2 49 0)', \
        'Unexpected geometry.'

    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (2 49 1)', \
        'Unexpected geometry.'

    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'LINESTRING (0 1 0,2 3 0)', \
        'Unexpected geometry.'

    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'POLYGON ((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0),(0.25 0.25 0,0.25 0.75 0,0.75 0.75 0,0.75 0.25 0,0.25 0.25 0))', \
        'Unexpected geometry.'

    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'MULTIPOINT (2 49 0,2 49 0)', \
        'Unexpected geometry.'

    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'MULTILINESTRING ((0 1 0,2 3 0),(0 1 0,2 3 0))', \
        'Unexpected geometry.'

    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'MULTIPOLYGON (((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0),(0.25 0.25 0,0.25 0.75 0,0.75 0.75 0,0.75 0.25 0,0.25 0.25 0)),((-0.25 0.25 0,-0.25 0.75 0,-0.75 0.75 0,-0.75 0.25 0,-0.25 0.25 0)))', \
        'Unexpected geometry.'

    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'GEOMETRYCOLLECTION (POINT (2 49 1),LINESTRING (0 1 0,2 3 0))', \
        'Unexpected geometry.'

    ds = None

###############################################################################


def test_ogr_libkml_write_kml():
    return ogr_libkml_write('/vsimem/libkml.kml')


def test_ogr_libkml_check_write_kml():
    return ogr_libkml_check_write('/vsimem/libkml.kml')


def test_ogr_libkml_write_kmz():
    return ogr_libkml_write('/vsimem/libkml.kmz')


def test_ogr_libkml_check_write_kmz():
    return ogr_libkml_check_write('/vsimem/libkml.kmz')


def test_ogr_libkml_write_kmz_use_doc_off():
    gdal.SetConfigOption("LIBKML_USE_DOC.KML", "NO")
    ret = ogr_libkml_write('/vsimem/libkml_use_doc_off.kmz')
    gdal.SetConfigOption("LIBKML_USE_DOC.KML", None)
    return ret


def test_ogr_libkml_check_write_kmz_use_doc_off():
    return ogr_libkml_check_write('/vsimem/libkml_use_doc_off.kmz')


def test_ogr_libkml_write_dir():
    return ogr_libkml_write('/vsimem/libkmldir')


def test_ogr_libkml_check_write_dir():
    if not ogrtest.have_read_libkml:
        pytest.skip()

    ret = ogr_libkml_check_write('/vsimem/libkmldir')
    files = gdal.ReadDir('/vsimem/libkmldir')
    for filename in files:
        gdal.Unlink('/vsimem/libkmldir/' + filename)
    gdal.Rmdir('/vsimem/libkmldir')
    return ret

###############################################################################
# Test reading attributes with XML content in them
#


def test_ogr_libkml_xml_attributes():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/description_with_xml.kml')

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if feat.GetField('description').find('Description<br></br><i attr="val">Interesting</i><br></br>') != 0:
        print('got: %s ' % feat.GetField('description'))
        pytest.fail('Wrong description field value')

    ds = None

###############################################################################
# Test reading all geometry types (#3558)
#


def test_ogr_libkml_read_geometries():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/geometries.kml')

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    while feat is not None:
        feat = lyr.GetNextFeature()

    ds = None

###############################################################################
# Run test_ogrsf


def test_ogr_libkml_test_ogrsf():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' --config OGR_SKIP KML -ro data/kml/samples.kml')

    assert not (ret.find("using driver `LIBKML'") == -1 or ret.find('INFO') == -1 or ret.find('ERROR') != -1)

###############################################################################
# Test reading KML with only Placemark


def test_ogr_libkml_read_placemark():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/placemark.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None

    ds = None

###############################################################################
# Test reading KML without any layer


def test_ogr_libkml_read_empty():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/empty.kml')
    assert ds.GetLayerCount() == 0

    ds = None

###############################################################################
# Test reading KML with empty layers


def test_ogr_libkml_read_emptylayers():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/emptylayers.kml')
    assert ds.GetLayerCount() == 2

    # --> One difference with the old KML driver
    assert ds.GetLayer(0).GetFeatureCount() == 1

    assert ds.GetLayer(1).GetFeatureCount() == 0

    ds = None

###############################################################################
# Test reading KML with empty layers

###############################################################################
# Test reading KML with empty layers without folder


def test_ogr_libkml_read_emptylayers_without_folder():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/emptylayers_without_folder.kml')
    assert ds.GetLayerCount() == 1

    # --> One difference with the old KML driver
    assert ds.GetLayer(0).GetName() == 'Test', \
        ("Layer name must be '" + ds.GetLayer(0).GetName() + "'.")

    ds = None

###############################################################################
# Test reading KML with empty layers without_folder


def test_ogr_libkml_read_schema():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/test_schema.kml')
    assert ds.GetLayerCount() == 4

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('foo') != 'bar':
        feat.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(1)
    feat = lyr.GetNextFeature()
    if feat.GetField('foo') != 'baz':
        feat.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(2)
    assert lyr.GetLayerDefn().GetFieldIndex('foo') == -1

    lyr = ds.GetLayer(3)
    assert lyr.GetLayerDefn().GetFieldIndex('foo') == -1

    ds = None

###############################################################################
# Test reading KML with <Data> elements of <ExtendedData> in case
# <ExtendedData> doesn't use a <SchemaData> (test changeset r22127)


def test_ogr_libkml_extended_data_without_schema_data():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/extended_data_without_schema_data.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('field1') != '1_1':
        feat.DumpReadable()
        pytest.fail()
    if feat.GetField('field2') != '1_2':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('field1') != '2_1':
        feat.DumpReadable()
        pytest.fail()
    if feat.IsFieldSet('field2'):
        feat.DumpReadable()
        pytest.fail()

    ds = None

###############################################################################
# Test reading KML with <gx:Track> element (#5095)


def test_ogr_libkml_gxtrack():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/gxtrack.kml')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if feat.GetField('begin') != '2013/05/28 12:00:00' or \
       feat.GetField('end') != '2013/05/28 13:00:00' or \
       feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (2 49,3 50)':
        feat.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
# Test reading KML with <gx:MultiTrack> element


def test_ogr_libkml_gxmultitrack():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/gxmultitrack.kml')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if feat.GetField('begin') != '2013/05/28 12:00:00' or \
       feat.GetField('end') != '2013/05/28 13:00:00' or \
       feat.GetGeometryRef().ExportToWkt() != 'MULTILINESTRING ((2 49,3 50))':
        feat.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
# Test generating and reading KML with <Camera> element


def test_ogr_libkml_camera():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_camera.kml")
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn("heading", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("tilt", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("roll", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("altitudeMode", ogr.OFTString))
    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    dst_feat.SetField("heading", 70)
    dst_feat.SetField("tilt", 75)
    dst_feat.SetField("roll", 10)
    with gdaltest.error_handler():
        lyr.CreateFeature(dst_feat)

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 50 1)'))
    dst_feat.SetField("heading", -70)
    dst_feat.SetField("altitudeMode", "relativeToGround")
    lyr.CreateFeature(dst_feat)

    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_camera.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<Camera>') == -1 or \
       data.find('<longitude>2</longitude>') == -1 or \
       data.find('<latitude>49</latitude>') == -1 or \
       data.find('<heading>70</heading>') == -1 or \
       data.find('<tilt>75</tilt>') == -1 or \
       data.find('<roll>10</roll>') == -1 or \
       data.find('<altitudeMode>relativeToGround</altitudeMode>') == -1))

    ds = ogr.Open('/vsimem/ogr_libkml_camera.kml')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if (feat.GetGeometryRef().ExportToWkt() != 'POINT (2 49 0)' or
        feat.GetField("heading") != 70.0 or
        feat.GetField("tilt") != 75.0 or
            feat.GetField("roll") != 10.0):
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if (feat.GetGeometryRef().ExportToWkt() != 'POINT (3 50 1)' or
        feat.GetField("heading") != -70.0 or
        feat.IsFieldSet("tilt") or
        feat.IsFieldSet("roll") or
            feat.GetField("altitudeMode") != 'relativeToGround'):
        feat.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
# Test generating a LookAt element at Document level


def test_ogr_libkml_write_layer_lookat():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_layer_lookat.kml")
    options = ['LOOKAT_LONGITUDE=2', 'LOOKAT_LATITUDE=49', 'LOOKAT_RANGE=150']
    ds.CreateLayer('test', options=options)
    options = ['LOOKAT_LONGITUDE=3', 'LOOKAT_LATITUDE=50', 'LOOKAT_RANGE=250',
               'LOOKAT_ALTITUDE=100', 'LOOKAT_HEADING=70', 'LOOKAT_TILT=50', 'LOOKAT_ALTITUDEMODE=relativeToGround']
    ds.CreateLayer('test2', options=options)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_layer_lookat.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<LookAt>') == -1 or \
       data.find('<longitude>2</longitude>') == -1 or \
       data.find('<latitude>49</latitude>') == -1 or \
       data.find('<range>150</range>') == -1))

    assert (not (data.find('<LookAt>') == -1 or \
       data.find('<longitude>3</longitude>') == -1 or \
       data.find('<latitude>50</latitude>') == -1 or \
       data.find('<altitude>100</altitude>') == -1 or \
       data.find('<heading>70</heading>') == -1 or \
       data.find('<tilt>50</tilt>') == -1 or \
       data.find('<range>150</range>') == -1 or \
       data.find('<altitudeMode>relativeToGround</altitudeMode>') == -1))


###############################################################################
# Test generating a Camera element at Document level

def test_ogr_libkml_write_layer_camera():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_layer_camera.kml")
    options = ['CAMERA_LONGITUDE=3', 'CAMERA_LATITUDE=50', 'CAMERA_ALTITUDE=100',
               'CAMERA_HEADING=70', 'CAMERA_TILT=50', 'CAMERA_ROLL=10', 'CAMERA_ALTITUDEMODE=relativeToGround']
    ds.CreateLayer('test', options=options)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_layer_camera.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<Camera>') == -1 or \
       data.find('<longitude>3</longitude>') == -1 or \
       data.find('<latitude>50</latitude>') == -1 or \
       data.find('<altitude>100</altitude>') == -1 or \
       data.find('<heading>70</heading>') == -1 or \
       data.find('<tilt>50</tilt>') == -1 or \
       data.find('<roll>10</roll>') == -1 or \
       data.find('<altitudeMode>relativeToGround</altitudeMode>') == -1))

###############################################################################
# Test writing MultiGeometry


def test_ogr_libkml_write_multigeometry():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_multigeometry.kml")
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    # Transformed into POINT per ATC 66
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(0 1)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    # Warning emitted per ATC 66
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT EMPTY'))
    with gdaltest.error_handler():
        lyr.CreateFeature(feat)

    ds = None

    ds = ogr.Open("/vsimem/ogr_libkml_write_multigeometry.kml")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1 0)':
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'GEOMETRYCOLLECTION EMPTY':
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test writing <snippet>


def test_ogr_libkml_write_snippet():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_snippet.kml")
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn("snippet", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('snippet', 'test_snippet')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    lyr.CreateFeature(feat)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_snippet.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.find('<snippet>test_snippet</snippet>') != -1

    ds = ogr.Open("/vsimem/ogr_libkml_write_snippet.kml")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('snippet') != 'test_snippet':
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1 0)':
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test writing <atom:author>


def test_ogr_libkml_write_atom_author():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    filepath = '/vsimem/ogr_libkml_write_atom_author.kml'
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource(filepath,
                                                        options=['author_name=name', 'author_uri=http://foo', 'author_email=foo@bar.com'])
    assert ds is not None, ('Unable to create %s.' % filepath)
    ds = None

    f = gdal.VSIFOpenL(filepath, 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:atom="http://www.w3.org/2005/Atom">') == -1 or \
       data.find('<atom:name>name</atom:name>') == -1 or \
       data.find('<atom:uri>http://foo</atom:uri>') == -1 or \
       data.find('<atom:email>foo@bar.com</atom:email>') == -1)), \
        'failure to find an atom string'

###############################################################################
# Test writing <atom:link>


def test_ogr_libkml_write_atom_link():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    filepath = '/vsimem/ogr_libkml_write_atom_link.kml'
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource(filepath,
                                                        options=['link=http://foo'])
    assert ds is not None, ('Unable to create %s.' % filepath)
    ds = None

    f = gdal.VSIFOpenL(filepath, 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:atom="http://www.w3.org/2005/Atom">') == -1 or \
       data.find('<atom:link href="http://foo" rel="related"/>') == -1))

###############################################################################
# Test writing <phoneNumber>


def test_ogr_libkml_write_phonenumber():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    filepath = '/vsimem/ogr_libkml_write_phonenumber.kml'
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource(filepath,
                                                        options=['phonenumber=tel:911'])
    assert ds is not None, ('Unable to create %s.' % filepath)
    ds = None

    f = gdal.VSIFOpenL(filepath, 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.find('<phoneNumber>tel:911</phoneNumber>') != -1

###############################################################################
# Test writing Region


def test_ogr_libkml_write_region():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_region.kml")
    lyr = ds.CreateLayer('auto', options=['ADD_REGION=YES'])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48))'))
    lyr.CreateFeature(feat)
    lyr = ds.CreateLayer('manual', options=['ADD_REGION=YES', 'REGION_XMIN=-180',
                                            'REGION_XMAX=180', 'REGION_YMIN=-90', 'REGION_YMAX=90',
                                            'REGION_MIN_LOD_PIXELS=128', 'REGION_MAX_LOD_PIXELS=10000000',
                                            'REGION_MIN_FADE_EXTENT=1', 'REGION_MAX_FADE_EXTENT=2'])
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_region.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<north>49</north>') == -1 or \
       data.find('<south>48</south>') == -1 or \
       data.find('<east>3</east>') == -1 or \
       data.find('<west>2</west>') == -1 or \
       data.find('<minLodPixels>256</minLodPixels>') == -1 or \
       data.find('<maxLodPixels>-1</maxLodPixels>') == -1))

    assert (not (data.find('<north>90</north>') == -1 or \
       data.find('<south>-90</south>') == -1 or \
       data.find('<east>180</east>') == -1 or \
       data.find('<west>-180</west>') == -1 or \
       data.find('<minLodPixels>128</minLodPixels>') == -1 or \
       data.find('<maxLodPixels>10000000</maxLodPixels>') == -1 or \
       data.find('<minFadeExtent>1</minFadeExtent>') == -1 or \
       data.find('<maxFadeExtent>2</maxFadeExtent>') == -1))

###############################################################################
# Test writing ScreenOverlay


def test_ogr_libkml_write_screenoverlay():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_screenoverlay.kml")
    ds.CreateLayer('auto', options=['SO_HREF=http://foo'])
    ds.CreateLayer('manual', options=['SO_HREF=http://bar',
                                      'SO_NAME=name',
                                      'SO_DESCRIPTION=description',
                                      'SO_OVERLAY_X=10',
                                      'SO_OVERLAY_Y=20',
                                      'SO_OVERLAY_XUNITS=pixels',
                                      'SO_OVERLAY_YUNITS=pixels',
                                      'SO_SCREEN_X=0.4',
                                      'SO_SCREEN_Y=0.5',
                                      'SO_SCREEN_XUNITS=fraction',
                                      'SO_SCREEN_YUNITS=fraction',
                                      'SO_SIZE_X=1.1',
                                      'SO_SIZE_Y=1.2',
                                      'SO_SIZE_XUNITS=fraction',
                                      'SO_SIZE_YUNITS=fraction'])
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_screenoverlay.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<href>http://foo</href>') == -1 or \
       data.find('<screenXY x="0.05" xunits="fraction" y="0.05" yunits="fraction"/>') == -1))

    assert (not (data.find('<overlayXY x="10" xunits="pixels" y="20" yunits="pixels"/>') == -1 or \
       data.find('<screenXY x="0.4" xunits="fraction" y="0.5" yunits="fraction"/>') == -1 or \
       data.find('<size x="1.1" xunits="fraction" y="1.2" yunits="fraction"/>') == -1 or \
       data.find('<name>name</name>') == -1 or \
       data.find('<description>description</description>') == -1))

###############################################################################
# Test writing Model


def test_ogr_libkml_write_model():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_model.kml")
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn("model", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("heading", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("tilt", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("roll", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("altitudeMode", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("scale_x", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("scale_y", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("scale_z", ogr.OFTReal))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49 10)'))
    feat.SetField("tilt", 75)
    feat.SetField("roll", 10)
    feat.SetField("heading", -70)
    feat.SetField("scale_x", 2)
    feat.SetField("scale_y", 3)
    feat.SetField("scale_z", 4)
    feat.SetField("altitudeMode", "relativeToGround")
    feat.SetField("model", "http://makc.googlecode.com/svn/trunk/flash/sandy_flar2/cube.dae")
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    feat.SetField("model", "http://foo")
    lyr.CreateFeature(feat)

    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_model.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<longitude>2</longitude>') == -1 or \
       data.find('<latitude>49</latitude>') == -1 or \
       data.find('<altitude>10</altitude>') == -1 or \
       data.find('<altitudeMode>relativeToGround</altitudeMode>') == -1 or \
       data.find('<heading>-70</heading>') == -1 or \
       data.find('<tilt>75</tilt>') == -1 or \
       data.find('<roll>10</roll>') == -1 or \
       data.find('<x>2</x>') == -1 or \
       data.find('<y>3</y>') == -1 or \
       data.find('<z>4</z>') == -1 or \
       data.find('<x>1</x>') == -1 or \
       data.find('<y>1</y>') == -1 or \
       data.find('<z>1</z>') == -1 or \
       data.find('<href>http://makc.googlecode.com/svn/trunk/flash/sandy_flar2/cube.dae</href>') == -1 or \
       data.find('<href>http://foo</href>') == -1))

    # This can only appear if HTTP resource is available and GDAL is built with curl/http support
    if gdal.GetDriverByName('HTTP') is not None and \
       (data.find('<targetHref>http://makc.googlecode.com/svn/trunk/flash/sandy_flar2/cube.gif</targetHref>') == -1 or
            data.find('<sourceHref>cube.gif</sourceHref>') == -1):

        assert gdaltest.gdalurlopen('http://makc.googlecode.com/svn/trunk/flash/sandy_flar2/cube.dae') is None, \
            data


###############################################################################
# Test read / write of style


def test_ogr_libkml_read_write_style():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_read_write_style_read.kml', 'wb')

    styles = """<Style id="style1">
            <IconStyle>
                <color>01234567</color>
                <scale>1.1</scale>
                <heading>50</heading>
                <Icon>
                    <href>http://style1</href>
                </Icon>
                <hotSpot x="15" y="20"/>
            </IconStyle>
            <LabelStyle>
                <color>01234567</color>
                <scale>1.1</scale>
            </LabelStyle>
            <BalloonStyle>
                <bgColor>ff00ffff</bgColor>
                <text><![CDATA[This is $[name], whose description is:<br/>$[description]]]></text>
            </BalloonStyle>
        </Style>
        <Style id="style2">
            <LineStyle>
                <color>01234567</color>
                <width>1</width>
            </LineStyle>
            <PolyStyle>
                <color>01234567</color>
            </PolyStyle>
        </Style>"""

    content = """<kml xmlns="http://www.opengis.net/kml/2.2">
    <Document>
        %s
        <StyleMap id="styleMapExample">
            <Pair>
                <key>normal</key>
                <Style id="inline_style">
                    <IconStyle>
                        <Icon>
                            <href>http://inline_style</href>
                        </Icon>
                    </IconStyle>
                </Style>
            </Pair>
            <Pair>
                <key>highlight</key>
                <styleUrl>#style2</styleUrl>
            </Pair>
        </StyleMap>
    </Document>
    </kml>""" % styles

    resolved_stylemap = """<Style id="styleMapExample">
      <IconStyle>
        <Icon>
          <href>http://inline_style</href>
        </Icon>
      </IconStyle>
    </Style>"""

    resolved_stylemap_highlight = """<Style id="styleMapExample">
        <LineStyle>
            <color>01234567</color>
            <width>1</width>
        </LineStyle>
        <PolyStyle>
            <color>01234567</color>
        </PolyStyle>
    </Style>"""
    gdal.VSIFWriteL(content, 1, len(content), f)
    gdal.VSIFCloseL(f)

    src_ds = ogr.Open('/vsimem/ogr_libkml_read_write_style_read.kml')
    style_table = src_ds.GetStyleTable()

    options = ['style1_balloonstyle_bgcolor=#FFFF00',
               'style1_balloonstyle_text=This is $[name], whose description is:<br/>$[description]']
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource('/vsimem/ogr_libkml_read_write_style_write.kml', options=options)
    ds.SetStyleTable(style_table)
    ds = None
    src_ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_read_write_style_write.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)
    lines = [l.strip() for l in data.split('\n')]

    lines_got = lines[lines.index('<Style id="style1">'):lines.index('<Style id="styleMapExample">')]
    lines_ref = [l.strip() for l in styles.split('\n')]
    if lines_got != lines_ref:
        print(data)
        pytest.fail(styles)

    lines_got = lines[lines.index('<Style id="styleMapExample">'):lines.index('</Document>')]
    lines_ref = [l.strip() for l in resolved_stylemap.split('\n')]
    if lines_got != lines_ref:
        print(data)
        pytest.fail(resolved_stylemap)

    # Test reading highlight style in StyleMap
    gdal.SetConfigOption('LIBKML_STYLEMAP_KEY', 'HIGHLIGHT')
    src_ds = ogr.Open('/vsimem/ogr_libkml_read_write_style_read.kml')
    style_table = src_ds.GetStyleTable()
    gdal.SetConfigOption('LIBKML_STYLEMAP_KEY', None)

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource('/vsimem/ogr_libkml_read_write_style_write.kml')
    ds.SetStyleTable(style_table)
    ds = None
    src_ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_read_write_style_write.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)
    lines = [l.strip() for l in data.split('\n')]

    lines_got = lines[lines.index('<Style id="styleMapExample">'):lines.index('</Document>')]
    lines_ref = [l.strip() for l in resolved_stylemap_highlight.split('\n')]
    if lines_got != lines_ref:
        print(data)
        pytest.fail(resolved_stylemap_highlight)

    # Test writing feature style
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource('/vsimem/ogr_libkml_read_write_style_write.kml')
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetStyleString('@unknown_style')
    lyr.CreateFeature(feat)
    feat = None
    feat = ogr.Feature(lyr.GetLayerDefn())
    style_string = 'PEN(c:#01234567,w:5.000000px);BRUSH(fc:#01234567);SYMBOL(id:"http://foo",a:50.000000,c:#01234567,s:1.100000);LABEL(c:#01234567,w:150.000000)'
    feat.SetStyleString(style_string)
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_libkml_read_write_style_write.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetStyleString() == '@unknown_style'
    feat = lyr.GetNextFeature()
    assert feat.GetStyleString() == style_string
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_read_write_style_write.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    expected_style = """<Style>
          <IconStyle>
            <color>67452301</color>
            <scale>1.1</scale>
            <heading>50</heading>
            <Icon>
              <href>http://foo</href>
            </Icon>
          </IconStyle>
          <LabelStyle>
            <color>67452301</color>
            <scale>1.5</scale>
          </LabelStyle>
          <LineStyle>
            <color>67452301</color>
            <width>5</width>
          </LineStyle>
          <PolyStyle>
            <color>67452301</color>
          </PolyStyle>
        </Style>"""
    lines = [l.strip() for l in data.split('\n')]

    lines_got = lines[lines.index('<Style>'):lines.index('</Style>') + 1]
    lines_ref = [l.strip() for l in expected_style.split('\n')]
    if lines_got != lines_ref:
        print(data)
        pytest.fail(resolved_stylemap_highlight)

    # Automatic StyleMap creation testing
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource('/vsimem/ogr_libkml_read_write_style_write.kml')
    style_table = ogr.StyleTable()
    style_table.AddStyle('style1_normal', 'SYMBOL(id:"http://style1_normal",c:#67452301)')
    style_table.AddStyle('style1_highlight', 'SYMBOL(id:"http://style1_highlight",c:#10325476)')
    ds.SetStyleTable(style_table)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_read_write_style_write.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)
    lines = [l.strip() for l in data.split('\n')]

    expected_styles = """<Style id="style1_normal">
      <IconStyle>
        <color>01234567</color>
        <Icon>
          <href>http://style1_normal</href>
        </Icon>
      </IconStyle>
    </Style>
    <Style id="style1_highlight">
      <IconStyle>
        <color>76543210</color>
        <Icon>
          <href>http://style1_highlight</href>
        </Icon>
      </IconStyle>
    </Style>
    <StyleMap id="style1">
      <Pair>
        <key>normal</key>
        <styleUrl>#style1_normal</styleUrl>
      </Pair>
      <Pair>
        <key>highlight</key>
        <styleUrl>#style1_highlight</styleUrl>
      </Pair>
    </StyleMap>"""

    lines_got = lines[lines.index('<Style id="style1_normal">'):lines.index('</StyleMap>') + 1]
    lines_ref = [l.strip() for l in expected_styles.split('\n')]
    if lines_got != lines_ref:
        print(data)
        pytest.fail(styles)


###############################################################################
# Test writing Update


def test_ogr_libkml_write_update():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    for i in range(3):

        if i == 0:
            name = "/vsimem/ogr_libkml_write_update.kml"
        elif i == 1:
            name = "/vsimem/ogr_libkml_write_update.kmz"
        else:
            name = "/vsimem/ogr_libkml_write_update_dir"

        ds = ogr.GetDriverByName('LIBKML').CreateDataSource(name,
                                                            options=['UPDATE_TARGETHREF=http://foo'])
        lyr = ds.CreateLayer('layer_to_edit')
        feat = ogr.Feature(lyr.GetLayerDefn())
        with gdaltest.error_handler():
            lyr.CreateFeature(feat)
        feat.SetFID(10)
        assert lyr.CreateFeature(feat) == 0
        feat.SetFID(2)
        assert lyr.TestCapability(ogr.OLCRandomWrite) == 1
        assert lyr.SetFeature(feat) == 0
        assert lyr.DeleteFeature(3) == 0
        ds = None

        if i == 0:
            f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_update.kml', 'rb')
        elif i == 1:
            f = gdal.VSIFOpenL('/vsizip//vsimem/ogr_libkml_write_update.kmz', 'rb')
        else:
            f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_update_dir/doc.kml', 'rb')
        assert f is not None, 'Unable to open the write_update file.'
        data = gdal.VSIFReadL(1, 2048, f)
        data = data.decode('ascii')
        gdal.VSIFCloseL(f)

        assert (not (data.find('<NetworkLinkControl>') == -1 or \
                data.find('<Update>') == -1 or \
                data.find('<targetHref>http://foo</targetHref>') == -1 or \
                data.find('<Placemark/>') == -1 or \
                data.find('<Placemark id="layer_to_edit.10"/>') == -1 or \
                data.find('<Create>') == -1 or \
                data.find('<Document targetId="layer_to_edit">') == -1 or \
                data.find('<Change>') == -1 or \
                data.find('<Placemark targetId="layer_to_edit.2"/>') == -1 or \
                data.find('<Delete>') == -1 or \
                data.find('<Placemark targetId="layer_to_edit.3"/>') == -1))


###############################################################################
# Test writing NetworkLinkControl


def test_ogr_libkml_write_networklinkcontrol():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    options = ['NLC_MINREFRESHPERIOD=3600',
               'NLC_MAXSESSIONLENGTH=-1',
               'NLC_COOKIE=cookie',
               'NLC_MESSAGE=message',
               'NLC_LINKNAME=linkname',
               'NLC_LINKDESCRIPTION=linkdescription',
               'NLC_LINKSNIPPET=linksnippet',
               'NLC_EXPIRES=2014-12-31T23:59:59Z']

    for i in range(3):

        if i == 0:
            name = "/vsimem/ogr_libkml_write_networklinkcontrol.kml"
        elif i == 1:
            name = "/vsimem/ogr_libkml_write_networklinkcontrol.kmz"
        else:
            name = "/vsimem/ogr_libkml_write_networklinkcontrol_dir"

        ds = ogr.GetDriverByName('LIBKML').CreateDataSource(name, options=options)
        assert ds is not None, ('Unable to create %s.' % name)
        ds = None

        if i == 0:
            f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_networklinkcontrol.kml', 'rb')
        elif i == 1:
            f = gdal.VSIFOpenL('/vsizip//vsimem/ogr_libkml_write_networklinkcontrol.kmz', 'rb')
        else:
            f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_networklinkcontrol_dir/doc.kml', 'rb')
        assert f is not None
        data = gdal.VSIFReadL(1, 2048, f)
        data = data.decode('ascii')
        gdal.VSIFCloseL(f)

        assert (not (data.find('<minRefreshPeriod>3600</minRefreshPeriod>') == -1 or \
                data.find('<maxSessionLength>-1</maxSessionLength>') == -1 or \
                data.find('<cookie>cookie</cookie>') == -1 or \
                data.find('<message>message</message>') == -1 or \
                data.find('<linkName>linkname</linkName>') == -1 or \
                data.find('<linkDescription>linkdescription</linkDescription>') == -1 or \
                data.find('<linkSnippet>linksnippet</linkSnippet>') == -1 or \
                data.find('<expires>2014-12-31T23:59:59Z</expires>') == -1))


###############################################################################
# Test writing ListStyle


def test_ogr_libkml_write_liststyle():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    options = ['LISTSTYLE_ICON_HREF=http://www.gdal.org/gdalicon.png']
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_liststyle.kml", options=options)
    ds.CreateLayer('test', options=['LISTSTYLE_ICON_HREF=http://foo'])
    ds.CreateLayer('test_check', options=['LISTSTYLE_TYPE=check'])
    ds.CreateLayer('test_radioFolder', options=['LISTSTYLE_TYPE=radioFolder'])
    ds.CreateLayer('test_checkOffOnly', options=['LISTSTYLE_TYPE=checkOffOnly'])
    ds.CreateLayer('test_checkHideChildren', options=['LISTSTYLE_TYPE=checkHideChildren'])
    with gdaltest.error_handler():
        ds.CreateLayer('test_error', options=['LISTSTYLE_TYPE=error'])
        ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_liststyle.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<styleUrl>#root_doc_liststyle</styleUrl>') == -1 or \
       data.find('<Style id="root_doc_liststyle">') == -1 or \
       data.find('<href>http://www.gdal.org/gdalicon.png</href>') == -1 or \
       data.find('<styleUrl>#test_liststyle</styleUrl>') == -1 or \
       data.find('<Style id="test_liststyle">') == -1 or \
       data.find('<href>http://foo</href>') == -1 or \
       data.find('<listItemType>check</listItemType>') == -1 or \
       data.find('<listItemType>radioFolder</listItemType>') == -1 or \
       data.find('<listItemType>checkOffOnly</listItemType>') == -1 or \
       data.find('<listItemType>checkHideChildren</listItemType>') == -1))

###############################################################################
# Test writing NetworkLink


def test_ogr_libkml_write_networklink():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_networklink.kml")
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('name', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_refreshvisibility", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("networklink_flytoview", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("networklink_refreshMode", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_refreshInterval", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("networklink_viewRefreshMode", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_viewRefreshTime", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("networklink_viewBoundScale", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("networklink_viewFormat", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_httpQuery", ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("name", "a network link")
    feat.SetField("networklink", "http://developers.google.com/kml/documentation/Point.kml")
    feat.SetField("networklink_refreshVisibility", 1)
    feat.SetField("networklink_flyToView", 1)
    feat.SetField("networklink_refreshInterval", 60)
    feat.SetField("networklink_httpQuery", "[clientVersion]")
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("networklink", "http://developers.google.com/kml/documentation/Point.kml")
    feat.SetField("networklink_viewRefreshTime", 30)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("networklink", "http://developers.google.com/kml/documentation/Point.kml")
    feat.SetField("networklink_refreshMode", 'onExpire')
    feat.SetField("networklink_viewRefreshMode", 'onRegion')
    feat.SetField("networklink_viewBoundScale", 0.5)
    feat.SetField("networklink_viewFormat", 'BBOX=[bboxWest],[bboxSouth],[bboxEast],[bboxNorth]')
    lyr.CreateFeature(feat)

    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_networklink.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<name>a network link</name>') == -1 or \
       data.find('<refreshVisibility>1</refreshVisibility>') == -1 or \
       data.find('<flyToView>1</flyToView>') == -1 or \
       data.find('<href>http://developers.google.com/kml/documentation/Point.kml</href>') == -1 or \
       data.find('<refreshMode>onInterval</refreshMode>') == -1 or \
       data.find('<refreshInterval>60</refreshInterval>') == -1 or \
       data.find('<httpQuery>[clientVersion]</httpQuery>') == -1 or \
       data.find('<viewRefreshMode>onStop</viewRefreshMode>') == -1 or \
       data.find('<viewRefreshTime>30</viewRefreshTime>') == -1 or \
       data.find('<refreshMode>onExpire</refreshMode>') == -1 or \
       data.find('<viewRefreshMode>onRegion</viewRefreshMode>') == -1 or \
       data.find('<viewBoundScale>0.5</viewBoundScale>') == -1 or \
       data.find('<viewFormat>BBOX=[bboxWest],[bboxSouth],[bboxEast],[bboxNorth]</viewFormat>') == -1))

###############################################################################
# Test writing PhotoOverlay


def test_ogr_libkml_write_photooverlay():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_photooverlay.kml")
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('name', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("heading", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("tilt", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("roll", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("camera_longitude", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("camera_latitude", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("camera_altitude", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("camera_altitudemode", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("photooverlay", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("leftfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("rightfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("bottomfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("topfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("near", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("photooverlay_shape", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("imagepyramid_tilesize", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("imagepyramid_maxwidth", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("imagepyramid_maxheight", ogr.OFTInteger))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("name", "a photo overlay")
    feat.SetField("photooverlay", "http://www.gdal.org/gdalicon.png")
    feat.SetField("camera_longitude", 2.2946)
    feat.SetField("camera_latitude", 48.8583)
    feat.SetField("camera_altitude", 20)
    feat.SetField("camera_altitudemode", "relativeToGround")
    feat.SetField("leftfov", -60)
    feat.SetField("rightfov", 59)
    feat.SetField("bottomfov", -58)
    feat.SetField("topfov", 57)
    feat.SetField("near", 100)
    feat.SetField("heading", 0)
    feat.SetField("tilt", 90)
    feat.SetField("roll", 0)
    feat.SetField("photooverlay_shape", "rectangle")
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.2945 48.85825)'))
    lyr.CreateFeature(feat)

    feat.SetField("photooverlay", "http://tile.openstreetmap.org/$[level]/$[x]/$[y].png")
    feat.SetField("imagepyramid_tilesize", 256)
    feat.SetField("imagepyramid_maxwidth", 512)
    feat.SetField("imagepyramid_maxheight", 512)
    lyr.CreateFeature(feat)

    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_photooverlay.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<Camera>') == -1 or \
       data.find('<longitude>2.2946</longitude>') == -1 or \
       data.find('<latitude>48.8583</latitude>') == -1 or \
       data.find('<altitude>20</altitude>') == -1 or \
       data.find('<heading>0</heading>') == -1 or \
       data.find('<tilt>90</tilt>') == -1 or \
       data.find('<roll>0</roll>') == -1 or \
       data.find('<altitudeMode>relativeToGround</altitudeMode>') == -1 or \
       data.find('<href>http://www.gdal.org/gdalicon.png</href>') == -1 or \
       data.find('<leftFov>-60</leftFov>') == -1 or \
       data.find('<rightFov>59</rightFov>') == -1 or \
       data.find('<bottomFov>-58</bottomFov>') == -1 or \
       data.find('<topFov>57</topFov>') == -1 or \
       data.find('<near>100</near>') == -1 or \
       data.find('2.2945,48.85825,0') == -1 or \
       data.find('<shape>rectangle</shape>') == -1 or \
       data.find('<href>http://tile.openstreetmap.org/$[level]/$[x]/$[y].png</href>') == -1 or \
       data.find('<tileSize>256</tileSize>') == -1 or \
       data.find('<maxWidth>512</maxWidth>') == -1 or \
       data.find('<maxHeight>512</maxHeight>') == -1 or \
       data.find('<gridOrigin>upperLeft</gridOrigin>') == -1))

###############################################################################
# Test writing and reading Data element


def test_ogr_libkml_read_write_data():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_read_write_data.kml")
    gdal.SetConfigOption('LIBKML_USE_SIMPLEFIELD', 'NO')
    lyr = ds.CreateLayer('test')
    gdal.SetConfigOption('LIBKML_USE_SIMPLEFIELD', None)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("foo", "bar")
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.2945 48.85825)'))
    lyr.CreateFeature(feat)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_read_write_data.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<Data name="foo">') == -1 or \
       data.find('<value>bar</value>') == -1))

    ds = ogr.Open("/vsimem/ogr_libkml_read_write_data.kml")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetField('foo') == 'bar'

###############################################################################
# Test writing layer as Folder


def test_ogr_libkml_write_folder():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_folder.kml")
    ds.CreateLayer('test', options=['LISTSTYLE_ICON_HREF=http://foo', 'FOLDER=YES'])
    ds.CreateLayer('test2', options=['FOLDER=YES'])
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_folder.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<Style id="test_liststyle">') == -1 or \
       data.find('<href>http://foo</href>') == -1 or \
       data.find('<Folder id="test">') == -1 or \
       data.find('<styleUrl>#test_liststyle</styleUrl>') == -1 or \
       data.find('<Folder id="test2">') == -1))

###############################################################################
# Test writing datasource and layer container propreties


def test_ogr_libkml_write_container_properties():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource("/vsimem/ogr_libkml_write_container_properties.kml",
                                                        options=['NAME=ds_name', 'DESCRIPTION=ds_description', 'OPEN=1', 'VISIBILITY=1', 'SNIPPET=ds_snippet'])
    ds.CreateLayer('test', options=['NAME=lyr_name', 'DESCRIPTION=lyr_description', 'OPEN=0', 'VISIBILITY=0', 'SNIPPET=lyr_snippet'])
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_libkml_write_container_properties.kml', 'rb')
    data = gdal.VSIFReadL(1, 2048, f)
    data = data.decode('ascii')
    gdal.VSIFCloseL(f)

    assert (not (data.find('<name>ds_name</name>') == -1 or \
       data.find('<visibility>1</visibility>') == -1 or \
       data.find('<open>1</open>') == -1 or \
       data.find('<snippet>ds_snippet</snippet>') == -1 or \
       data.find('<description>ds_description</description>') == -1 or \
       data.find('<name>lyr_name</name>') == -1 or \
       data.find('<visibility>0</visibility>') == -1 or \
       data.find('<open>0</open>') == -1 or \
       data.find('<snippet>lyr_snippet</snippet>') == -1 or \
       data.find('<description>lyr_description</description>') == -1))

###############################################################################
# Test reading gx:TimeStamp and gx:TimeSpan


def test_ogr_libkml_read_gx_timestamp():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/gxtimestamp.kml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['timestamp'] != '2016/02/13 12:34:56+00':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f['begin'] != '2016/02/13 00:00:00+00' or f['end'] != '2016/02/14 00:00:00+00':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test reading KML with kml: prefix


def test_ogr_libkml_read_placemark_with_kml_prefix():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/placemark_with_kml_prefix.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None

###############################################################################
# Test reading KML with duplicated folder name


def test_ogr_libkml_read_duplicate_folder_name():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/duplicate_folder_name.kml')
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'layer'
    lyr = ds.GetLayer(1)
    assert lyr.GetName() == 'layer (#2)'

###############################################################################
# Test reading KML with a placemark in root document, and a subfolder (#7221)


def test_ogr_libkml_read_placemark_in_root_and_subfolder():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/placemark_in_root_and_subfolder.kml')
    lyr = ds.GetLayerByName('TopLevel')
    assert lyr is not None
    assert lyr.GetFeatureCount() == 1

    lyr = ds.GetLayerByName('SubFolder1')
    assert lyr is not None
    assert lyr.GetFeatureCount() == 1

###############################################################################
# Test reading KML with coordinate tuples separated by tabulations (#7231)


def test_ogr_libkml_read_tab_separated_coord_triplet():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/tab_separated_coord_triplet.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    wkt = 'LINESTRING Z (1 2 3,4 5 6)'

    assert not ogrtest.check_feature_geometry(feat, wkt)

###############################################################################
# Test reading KML with coordinate with space only content (#7232)


def test_ogr_libkml_read_kml_with_space_content_in_coordinates():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/kml_with_space_content_in_coordinates.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    wkt = 'LINESTRING EMPTY'

    assert not ogrtest.check_feature_geometry(feat, wkt)

###############################################################################
# Test reading a layer referring several schema (github #826)


def test_ogr_libkml_read_several_schema():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/several_schema_in_layer.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat['fieldA'] != 'fieldAValue' or feat['common'] != 'commonAValue':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat['fieldB'] != 'fieldBValue' or feat['common'] != 'commonBValue':
        feat.DumpReadable()
        pytest.fail()

    ds = ogr.Open('data/kml/several_schema_outside_layer.kml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat['fieldA'] != 'fieldAValue' or feat['common'] != 'commonAValue':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat['fieldB'] != 'fieldBValue' or feat['common'] != 'commonBValue':
        feat.DumpReadable()
        pytest.fail()


###############################################################################


def test_ogr_libkml_update_existing_kml():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    filename = '/vsimem/ogr_libkml_update_existing_kml.kml'
    gdal.FileFromMemBuffer(filename, open('data/kml/several_schema_in_layer.kml', 'rb').read())
    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    fc_before = lyr.GetFeatureCount()
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    fc_after = lyr.GetFeatureCount()
    assert fc_after == fc_before + 1

    gdal.Unlink(filename)


###############################################################################
# Test reading KML with non-conformant MultiPolygon, MultiLineString, MultiPoint (#4031)


def test_ogr_libkml_read_non_conformant_multi_geometries():

    if not ogrtest.have_read_libkml:
        pytest.skip()

    ds = ogr.Open('data/kml/non_conformant_multi.kml')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    wkt = 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    wkt = 'MULTILINESTRING ((0 0,1 1))'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    wkt = 'MULTIPOINT ((0 0))'
    assert not ogrtest.check_feature_geometry(feat, wkt)
