#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GMLAS driver testing.
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# Initial development funded by the European Earth observation programme
# Copernicus
#
# ******************************************************************************
# Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

from http.server import BaseHTTPRequestHandler
import os
import os.path


import gdaltest
import ogrtest
import webserver

from osgeo import gdal
from osgeo import ogr
import pytest


pytestmark = pytest.mark.require_driver('GMLAS')

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', 'YES')

    # FileGDB embedded libxml2 cause random crashes with CPLValidateXML() use of external libxml2
    old_val_GDAL_XML_VALIDATION = gdal.GetConfigOption('GDAL_XML_VALIDATION')
    if ogr.GetDriverByName('FileGDB') is not None and old_val_GDAL_XML_VALIDATION is None:
        gdal.SetConfigOption('GDAL_XML_VALIDATION', 'NO')

    yield

    files = gdal.ReadDir('/vsimem/')
    if files is not None:
        print('Remaining files: ' + str(files))

    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', None)
    gdal.SetConfigOption('GDAL_XML_VALIDATION', old_val_GDAL_XML_VALIDATION)

###############################################################################

def compare_ogrinfo_output(gmlfile, reffile, options=''):

    import test_cli_utilities

    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    tmpfilename = 'tmp/' + os.path.basename(gmlfile) + '.txt'
    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() +
                               ' -ro -al GMLAS:' + gmlfile +
                               ' -oo EXPOSE_METADATA_LAYERS=YES ' +
                               '-oo @KEEP_RELATIVE_PATHS_FOR_METADATA=YES ' +
                               '-oo @EXPOSE_SCHEMAS_NAME_IN_METADATA=NO ' +
                               '-oo @EXPOSE_CONFIGURATION_IN_METADATA=NO' + ' ' + options,
                               encoding='utf-8')
    ret = ret.replace('\r\n', '\n')
    ret = ret.replace('data\\gmlas\\', 'data/gmlas/')  # Windows
    expected = open(reffile, 'rb').read().decode('utf-8')
    expected = expected.replace('\r\n', '\n')
    if ret != expected:
        print(ret.encode('utf-8'))
        open(tmpfilename, 'wb').write(ret.encode('utf-8'))
        print('Diff:')
        os.system('diff -u ' + reffile + ' ' + tmpfilename)
        # os.unlink(tmpfilename)
        pytest.fail('Got:')

###############################################################################
# Basic test


def test_ogr_gmlas_basic():

    ds = ogr.Open('GMLAS:data/gmlas/gmlas_test1.xml')
    assert ds is not None
    ds = None

    # Skip tests when -fsanitize is used
    if gdaltest.is_travis_branch('sanitize'):
        pytest.skip('Skipping because of -sanitize')

    return compare_ogrinfo_output('data/gmlas/gmlas_test1.xml',
                                  'data/gmlas/gmlas_test1.txt')

###############################################################################
# Run test_ogrsf


def test_ogr_gmlas_test_ogrsf():

    # Skip tests when -fsanitize is used
    if gdaltest.is_travis_branch('sanitize'):
        pytest.skip('Skipping because of -sanitize')

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro GMLAS:data/gmlas/gmlas_test1.xml')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test virtual file support


def test_ogr_gmlas_virtual_file():

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_8.xml',
                           """<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_8.xsd"/>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_8.xsd',
                           """<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns"
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt" type="xs:string"/>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_8.xml')
    assert ds is not None

    gdal.Unlink('/vsimem/ogr_gmlas_8.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_8.xsd')

###############################################################################
# Test opening with XSD option


def test_ogr_gmlas_datafile_with_xsd_option():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['XSD=data/gmlas/gmlas_test1.xsd'])
    assert ds is not None

###############################################################################
# Test opening with just XSD option


def test_ogr_gmlas_no_datafile_with_xsd_option():

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_test1.xsd'])
    assert ds is not None

###############################################################################
# Test opening with just XSD option but pointing to a non-xsd filename


def test_ogr_gmlas_no_datafile_xsd_which_is_not_xsd():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_test1.xml'])
    assert ds is None
    assert gdal.GetLastErrorMsg().find("invalid content in 'schema' element") >= 0

###############################################################################
# Test opening with nothing


def test_ogr_gmlas_no_datafile_no_xsd():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:')
    assert ds is None
    assert gdal.GetLastErrorMsg().find('XSD open option must be provided when no XML data file is passed') >= 0

###############################################################################
# Test opening an inexisting GML file


def test_ogr_gmlas_non_existing_gml():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/i_do_not_exist.gml')
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot open /vsimem/i_do_not_exist.gml') >= 0

###############################################################################
# Test opening with just XSD option but pointing to a non existing file


def test_ogr_gmlas_non_existing_xsd():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:', open_options=['XSD=/vsimem/i_do_not_exist.xsd'])
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot resolve /vsimem/i_do_not_exist.xsd') >= 0

###############################################################################
# Test opening a GML file without schemaLocation


def test_ogr_gmlas_gml_without_schema_location():

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_gml_without_schema_location.xml',
                           """<MYNS:main_elt xmlns:MYNS="http://myns"/>""")

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_gml_without_schema_location.xml')
    assert ds is None
    assert gdal.GetLastErrorMsg().find('No schema locations found when analyzing data file: XSD open option must be provided') >= 0

    gdal.Unlink('/vsimem/ogr_gmlas_gml_without_schema_location.xml')

###############################################################################
# Test invalid schema


def test_ogr_gmlas_invalid_schema():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_invalid_schema.xml')
    assert ds is None
    assert gdal.GetLastErrorMsg().find('invalid content') >= 0

###############################################################################
# Test invalid XML


def test_ogr_gmlas_invalid_xml():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_invalid_xml.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert f is None
    assert gdal.GetLastErrorMsg().find('input ended before all started tags were ended') >= 0

###############################################################################
# Test links with gml:ReferenceType


def test_ogr_gmlas_gml_Reference():

    ds = ogr.Open('GMLAS:data/gmlas/gmlas_test_targetelement.xml')
    assert ds.GetLayerCount() == 3

    lyr = ds.GetLayerByName('main_elt')
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f['reference_existing_target_elt_with_required_id_href'] != '#BAZ' or \
       f['reference_existing_target_elt_with_required_id_pkid'] != 'BAZ' or \
       f['reference_existing_target_elt_with_optional_id_href'] != '#BAZ2' or \
       f['refe_exis_targ_elt_with_opti_id_targe_elt_with_optio_id_pkid'] != 'F36BAD21BD2F14DDCA8852DBF8C90DBC_target_elt_with_optional_id_1' or \
       f['reference_existing_abstract_target_elt_href'] != '#BAW' or \
       f.IsFieldSet('reference_existing_abstract_target_elt_nillable_href') or \
       f['reference_existing_abstract_target_elt_nillable_nil'] != 1:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test that we fix ambiguities in class names


def test_ogr_gmlas_same_element_in_different_ns():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_same_element_in_different_ns.xml')
    assert ds is not None
    # for i in range(ds.GetLayerCount()):
    #    print(ds.GetLayer(i).GetName())

    assert ds.GetLayerCount() == 5
    lyr = ds.GetLayerByName('elt')
    f = lyr.GetNextFeature()
    if f.IsFieldSet('abstractElt_other_ns_realizationOfAbstractElt_pkid') == 0:
        f.DumpReadable()
        pytest.fail()
    assert ds.GetLayerByName('myns_realizationOfAbstractElt') is not None
    assert ds.GetLayerByName('other_ns_realizationOfAbstractElt') is not None
    assert ds.GetLayerByName('elt_elt2_abstractElt_myns_realizationOfAbstractElt') is not None
    assert ds.GetLayerByName('elt_elt2_abstractElt_other_ns_realizationOfAbstractElt') is not None

###############################################################################
# Test a corner case of relative path resolution


def test_ogr_gmlas_corner_case_relative_path():

    ds = ogr.Open('GMLAS:../ogr/data/gmlas/gmlas_test1.xml')
    assert ds is not None

###############################################################################
# Test unexpected repeated element


def test_ogr_gmlas_unexpected_repeated_element():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_unexpected_repeated_element.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is None or f['foo'] != 'foo_again':  # somewhat arbitrary to keep the latest one!
        f.DumpReadable()
        pytest.fail()
    assert gdal.GetLastErrorMsg().find('Unexpected element myns:main_elt/myns:foo') >= 0
    f = lyr.GetNextFeature()
    assert f is None
    ds = None

###############################################################################
# Test unexpected repeated element


def test_ogr_gmlas_unexpected_repeated_element_variant():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_unexpected_repeated_element_variant.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is None or f['foo'] != 'foo_again':  # somewhat arbitrary to keep the latest one!
        f.DumpReadable()
        pytest.fail()
    assert gdal.GetLastErrorMsg().find('Unexpected element myns:main_elt/myns:foo') >= 0
    f = lyr.GetNextFeature()
    assert f is None
    ds = None

###############################################################################
# Test reading geometries embedded in a geometry property element


def test_ogr_gmlas_geometryproperty():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32.gml', open_options=[
        'CONFIG_FILE=<Configuration><LayerBuildingRules><GML><IncludeGeometryXML>true</IncludeGeometryXML></GML></LayerBuildingRules></Configuration>'])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        geom_field_count = lyr.GetLayerDefn().GetGeomFieldCount()
    assert geom_field_count == 15
    f = lyr.GetNextFeature()
    if f['geometryProperty_xml'] != ' <gml:Point gml:id="poly.geom.Geometry" srsName="urn:ogc:def:crs:EPSG::4326"> <gml:pos>49 2</gml:pos> </gml:Point> ':
        f.DumpReadable()
        pytest.fail()
    if not f.IsFieldNull('geometryPropertyEmpty_xml'):
        f.DumpReadable()
        pytest.fail()
    if f['pointProperty_xml'] != '<gml:Point gml:id="poly.geom.Point"><gml:pos srsName="http://www.opengis.net/def/crs/EPSG/0/4326">50 3</gml:pos></gml:Point>':
        f.DumpReadable()
        pytest.fail()
    if f['pointPropertyRepeated_xml'] != [
            '<gml:Point gml:id="poly.geom.pointPropertyRepeated.1"><gml:pos>0 1</gml:pos></gml:Point>',
            '<gml:Point gml:id="poly.geom.pointPropertyRepeated.2"><gml:pos>1 2</gml:pos></gml:Point>',
            '<gml:Point gml:id="poly.geom.pointPropertyRepeated.3"><gml:pos>3 4</gml:pos></gml:Point>']:
        f.DumpReadable()
        pytest.fail(f['pointPropertyRepeated_xml'])
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    sr = lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetSpatialRef()
    assert not (sr is None or sr.ExportToWkt().find('4326') < 0)
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryPropertyEmpty')
    if f.GetGeomFieldRef(geom_idx) is not None:
        f.DumpReadable()
        pytest.fail()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('pointProperty')
    sr = lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetSpatialRef()
    assert not (sr is None or sr.ExportToWkt().find('4326') < 0)
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'POINT (3 50)':
        f.DumpReadable()
        pytest.fail()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('lineStringProperty')
    sr = lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetSpatialRef()
    assert not (sr is None or sr.ExportToWkt().find('4326') < 0)
    assert lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetType() == ogr.wkbLineString
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'LINESTRING (2 49)':
        f.DumpReadable()
        pytest.fail()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('pointPropertyRepeated')
    assert lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetType() == ogr.wkbUnknown
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'GEOMETRYCOLLECTION (POINT (0 1),POINT (1 2),POINT (3 4))':
        f.DumpReadable()
        pytest.fail()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('mycustompointproperty_point')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'POINT (5 6)':
        f.DumpReadable()
        pytest.fail()

    # Test that on-the-fly reprojection works
    f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    geom = f.GetGeomFieldRef(geom_idx)
    if ogrtest.check_feature_geometry(geom, 'POINT (3.0 0.0)') != 0:
        f.DumpReadable()
        pytest.fail()

    # Failed reprojection
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    if f.GetGeomFieldRef(geom_idx) is not None:
        f.DumpReadable()
        pytest.fail()

    # Test SWAP_COORDINATES=NO
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32.gml',
                     open_options=['SWAP_COORDINATES=NO'])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'POINT (49 2)':
        f.DumpReadable()
        pytest.fail()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('lineStringProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'LINESTRING (2 49)':
        f.DumpReadable()
        pytest.fail()

    # Test SWAP_COORDINATES=YES
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32.gml',
                     open_options=['SWAP_COORDINATES=YES'])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('lineStringProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'LINESTRING (49 2)':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test reading geometries referenced by a AbstractGeometry element


def test_ogr_gmlas_abstractgeometry():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_abstractgeometry_gml32.gml', open_options=[
        'CONFIG_FILE=<Configuration><LayerBuildingRules><GML><IncludeGeometryXML>true</IncludeGeometryXML></GML></LayerBuildingRules></Configuration>'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 2
    f = lyr.GetNextFeature()
    if f['AbstractGeometry_xml'] != '<gml:Point gml:id="test.geom.0"><gml:pos>0 1</gml:pos></gml:Point>':
        f.DumpReadable()
        pytest.fail()
    if f['repeated_AbstractGeometry_xml'] != [
            '<gml:Point gml:id="test.geom.repeated.1"><gml:pos>0 1</gml:pos>',
            '<gml:Point gml:id="test.geom.repeated.2"><gml:pos>1 2</gml:pos>']:
        f.DumpReadable()
        pytest.fail(f['repeated_AbstractGeometry_xml'])
    wkt = f.GetGeomFieldRef(0).ExportToWkt()
    if wkt != 'POINT (0 1)':
        f.DumpReadable()
        pytest.fail()
    wkt = f.GetGeomFieldRef(1).ExportToWkt()
    if wkt != 'GEOMETRYCOLLECTION (POINT (0 1),POINT (1 2))':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test validation against schema


class MyHandler(object):
    def __init__(self):
        self.error_list = []

    def error_handler(self, err_type, err_no, err_msg):
        if err_type != 1:  # 1 == Debug
            self.error_list.append((err_type, err_no, err_msg))


def test_ogr_gmlas_validate():

    # By default check we are silent about validation error
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_validate.xml')
    assert ds is not None
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', None)
    lyr = ds.GetLayer(0)
    lyr.GetFeatureCount()
    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', 'YES')
    gdal.PopErrorHandler()
    assert not myhandler.error_list

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_validate.xml')
    assert ds is not None
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    lyr = ds.GetLayer(0)
    lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    # Unexpected element with xpath=myns:main_elt/myns:bar (subxpath=myns:main_elt/myns:bar) found
    assert len(myhandler.error_list) >= 2

    # Enable validation on a doc without validation errors
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['VALIDATE=YES'])
    gdal.PopErrorHandler()
    assert ds is not None, myhandler.error_list
    assert not myhandler.error_list

    # Enable validation on a doc without validation error, and with explicit XSD
    gdal.FileFromMemBuffer('/vsimem/gmlas_test1.xml',
                           open('data/gmlas/gmlas_test1.xml').read())
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:/vsimem/gmlas_test1.xml', open_options=[
        'XSD=' + os.getcwd() + '/data/gmlas/gmlas_test1.xsd', 'VALIDATE=YES'])
    gdal.PopErrorHandler()
    gdal.Unlink('/vsimem/gmlas_test1.xml')
    assert ds is not None, myhandler.error_list
    assert not myhandler.error_list

    # Validation errors, but do not prevent dataset opening
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_validate.xml', open_options=['VALIDATE=YES'])
    gdal.PopErrorHandler()
    assert ds is not None
    assert len(myhandler.error_list) == 5

    # Validation errors and do prevent dataset opening
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_validate.xml', open_options=['VALIDATE=YES', 'FAIL_IF_VALIDATION_ERROR=YES'])
    gdal.PopErrorHandler()
    assert ds is None
    assert len(myhandler.error_list) == 6

    # Test that validation without doc doesn't crash
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_test1.xsd', 'VALIDATE=YES'])
    gdal.PopErrorHandler()
    assert ds is not None, myhandler.error_list
    assert not myhandler.error_list

###############################################################################
# Test correct namespace prefix handling


def test_ogr_gmlas_test_ns_prefix():

    # The schema doesn't directly import xlink, but indirectly references it
    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_test_targetelement.xsd'])

    lyr = ds.GetLayerByName('_ogr_fields_metadata')
    f = lyr.GetNextFeature()
    if f['field_xpath'] != 'myns:main_elt/myns:reference_missing_target_elt/@xlink:href':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test parsing documents without namespace


def test_ogr_gmlas_no_namespace():

    ds = ogr.Open('GMLAS:data/gmlas/gmlas_no_namespace.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test CONFIG_FILE


def test_ogr_gmlas_conf():

    # Non existing file
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['CONFIG_FILE=not_existing'])
    assert ds is None

    # Broken conf file
    gdal.FileFromMemBuffer('/vsimem/my_conf.xml', "<broken>")
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['CONFIG_FILE=/vsimem/my_conf.xml'])
    gdal.Unlink('/vsimem/my_conf.xml')
    assert ds is None

    # Valid XML, but not validating
    gdal.FileFromMemBuffer('/vsimem/my_conf.xml', "<not_validating/>")
    with gdaltest.error_handler():
        gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['CONFIG_FILE=/vsimem/my_conf.xml'])
    gdal.Unlink('/vsimem/my_conf.xml')

    # Inlined conf file + UseArrays = false
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=[
        'CONFIG_FILE=<Configuration><LayerBuildingRules><UseArrays>false</UseArrays></LayerBuildingRules></Configuration>'])
    assert ds is not None
    lyr = ds.GetLayerByName('main_elt_string_array')
    assert lyr.GetFeatureCount() == 2

    # AlwaysGenerateOGRId = true
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=[
        'CONFIG_FILE=<Configuration><LayerBuildingRules><AlwaysGenerateOGRId>true</AlwaysGenerateOGRId></LayerBuildingRules></Configuration>'])
    assert ds is not None
    lyr = ds.GetLayerByName('main_elt')
    f = lyr.GetNextFeature()
    if f['ogr_pkid'].find('main_elt_1') < 0 or \
       f['otherns_id'] != 'otherns_id':
        f.DumpReadable()
        pytest.fail()

    # IncludeGeometryXML = false
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32.gml', open_options=[
        'CONFIG_FILE=<Configuration><LayerBuildingRules><GML><IncludeGeometryXML>false</IncludeGeometryXML></GML></LayerBuildingRules></Configuration>'])
    assert ds is not None
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        assert lyr.GetLayerDefn().GetFieldIndex('geometryProperty_xml') < 0
    f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()

    # ExposeMetadataLayers = true
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_abstractgeometry_gml32.gml', open_options=[
        'CONFIG_FILE=<Configuration><ExposeMetadataLayers>true</ExposeMetadataLayers></Configuration>'])
    assert ds is not None
    assert ds.GetLayerCount() == 5
    # Test override with open option
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_abstractgeometry_gml32.gml', open_options=[
        'EXPOSE_METADATA_LAYERS=NO',
        'CONFIG_FILE=<Configuration><ExposeMetadataLayers>true</ExposeMetadataLayers></Configuration>'])
    assert ds is not None
    assert ds.GetLayerCount() == 1

    # Turn on validation and error on validation
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_validate.xml', open_options=[
            'CONFIG_FILE=<Configuration><Validation enabled="true"><FailIfError>true</FailIfError></Validation></Configuration>'])
    assert ds is None and gdal.GetLastErrorMsg().find('Validation') >= 0

###############################################################################
# Test IgnoredXPaths aspect of config file


def test_ogr_gmlas_conf_ignored_xpath():

    # Test unsupported and invalid XPaths
    for xpath in ['',
                  '1',
                  '@',
                  '@/',
                  '.',
                  ':',
                  '/:',
                  'a:',
                  'a:1',
                  'foo[1]',
                  "foo[@bar='baz']"]:
        with gdaltest.error_handler():
            gdal.OpenEx('GMLAS:', open_options=[
                'XSD=data/gmlas/gmlas_test1.xsd',
                """CONFIG_FILE=<Configuration>
                        <IgnoredXPaths>
                            <WarnIfIgnoredXPathFoundInDocInstance>true</WarnIfIgnoredXPathFoundInDocInstance>
                            <XPath>%s</XPath>
                        </IgnoredXPaths>
                    </Configuration>""" % xpath])
        assert gdal.GetLastErrorMsg().find('XPath syntax') >= 0, xpath

    # Test duplicating mapping
    with gdaltest.error_handler():
        gdal.OpenEx('GMLAS:', open_options=[
                    'XSD=data/gmlas/gmlas_test1.xsd',
                    """CONFIG_FILE=<Configuration>
                    <IgnoredXPaths>
                        <Namespaces>
                            <Namespace prefix="ns" uri="http://ns1"/>
                            <Namespace prefix="ns" uri="http://ns2"/>
                        </Namespaces>
                    </IgnoredXPaths>
                </Configuration>"""])
    assert gdal.GetLastErrorMsg().find('Prefix ns was already mapped') >= 0

    # Test XPath with implicit namespace, and warning
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=[
        """CONFIG_FILE=<Configuration>
                <IgnoredXPaths>
                    <WarnIfIgnoredXPathFoundInDocInstance>true</WarnIfIgnoredXPathFoundInDocInstance>
                    <XPath>@otherns:id</XPath>
                </IgnoredXPaths>
            </Configuration>"""])
    assert ds is not None
    lyr = ds.GetLayerByName('main_elt')
    assert lyr.GetLayerDefn().GetFieldIndex('otherns_id') < 0
    with gdaltest.error_handler():
        lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg().find('Attribute with xpath=myns:main_elt/@otherns:id found in document but ignored') >= 0

    # Test XPath with explicit namespace, and warning suppression
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=[
        """CONFIG_FILE=<Configuration>
                <IgnoredXPaths>
                    <Namespaces>
                        <Namespace prefix="other_ns" uri="http://other_ns"/>
                    </Namespaces>
                    <XPath warnIfIgnoredXPathFoundInDocInstance="false">@other_ns:id</XPath>
                </IgnoredXPaths>
            </Configuration>"""])
    assert ds is not None
    lyr = ds.GetLayerByName('main_elt')
    lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == ''

    # Test various XPath syntaxes
    ds = gdal.OpenEx('GMLAS:', open_options=[
        'XSD=data/gmlas/gmlas_test1.xsd',
        """CONFIG_FILE=<Configuration>
                <IgnoredXPaths>
                    <WarnIfIgnoredXPathFoundInDocInstance>false</WarnIfIgnoredXPathFoundInDocInstance>
                    <XPath>myns:main_elt/@optionalStrAttr</XPath>
                    <XPath>myns:main_elt//@fixedValUnset</XPath>
                    <XPath>myns:main_elt/myns:base_int</XPath>
                    <XPath>//myns:string</XPath>
                    <XPath>myns:main_elt//myns:string_array</XPath>

                    <XPath>a</XPath> <!-- no match -->
                    <XPath>unknown_ns:foo</XPath> <!-- no match -->
                    <XPath>myns:main_elt/myns:int_arra</XPath> <!-- no match -->
                    <XPath>foo/myns:long</XPath> <!-- no match -->
                </IgnoredXPaths>
            </Configuration>"""])
    assert ds is not None
    lyr = ds.GetLayerByName('main_elt')

    # Ignored fields
    assert lyr.GetLayerDefn().GetFieldIndex('optionalStrAttr') < 0
    assert lyr.GetLayerDefn().GetFieldIndex('fixedValUnset') < 0
    assert lyr.GetLayerDefn().GetFieldIndex('base_int') < 0
    assert lyr.GetLayerDefn().GetFieldIndex('string') < 0
    assert lyr.GetLayerDefn().GetFieldIndex('string_array') < 0

    # Present fields
    assert lyr.GetLayerDefn().GetFieldIndex('int_array') >= 0
    assert lyr.GetLayerDefn().GetFieldIndex('long') >= 0

###############################################################################


do_log = False


class GMLASHTTPHandler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        pass

    def do_GET(self):

        try:
            if do_log:
                f = open('/tmp/log.txt', 'a')
                f.write('GET %s\n' % self.path)
                f.close()

            if self.path.startswith('/vsimem/'):
                f = gdal.VSIFOpenL(self.path, "rb")
                if f is None:
                    self.send_response(404)
                    self.end_headers()
                else:
                    gdal.VSIFSeekL(f, 0, 2)
                    size = gdal.VSIFTellL(f)
                    gdal.VSIFSeekL(f, 0, 0)
                    content = gdal.VSIFReadL(1, size, f)
                    gdal.VSIFCloseL(f)
                    self.protocol_version = 'HTTP/1.0'
                    self.send_response(200)
                    self.end_headers()
                    self.wfile.write(content)
                return

            return
        except IOError:
            pass

        self.send_error(404, 'File Not Found: %s' % self.path)

###############################################################################
# Test schema caching


def test_ogr_gmlas_cache():

    drv = gdal.GetDriverByName('HTTP')

    if drv is None:
        pytest.skip()

    (webserver_process, webserver_port) = webserver.launch(handler=GMLASHTTPHandler)
    if webserver_port == 0:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_cache.xml',
                           """<main_elt xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:noNamespaceSchemaLocation="http://localhost:%d/vsimem/ogr_gmlas_cache.xsd">
    <foo>bar</foo>
</main_elt>
""" % webserver_port)

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_cache.xsd',
                           """<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:include schemaLocation="ogr_gmlas_cache_2.xsd"/>
</xs:schema>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_cache_2.xsd',
                           """<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    # First try with remote schema download disabled
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
            'CONFIG_FILE=<Configuration><AllowRemoteSchemaDownload>false</AllowRemoteSchemaDownload><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    assert ds is None and gdal.GetLastErrorMsg().find('Cannot resolve') >= 0

    # Test invalid cache directory
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/inexisting_directory/not/exist</Directory></SchemaCache></Configuration>'])
    if ds is None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    if ds.GetLayerCount() != 1:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail(ds.GetLayerCount())

    # Will create the directory and download and cache
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
        'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    if ds.GetLayerCount() != 1:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail(ds.GetLayerCount())

    gdal.Unlink('/vsimem/my/gmlas_cache/' + gdal.ReadDir('/vsimem/my/gmlas_cache')[0])

    # Will reuse the directory and download and cache
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
        'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    if ds.GetLayerCount() != 1:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    # With XSD open option
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
        'XSD=http://localhost:%d/vsimem/ogr_gmlas_cache.xsd' % webserver_port,
        'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    if ds.GetLayerCount() != 1:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    webserver.server_stop(webserver_process, webserver_port)

    # Now re-open with the webserver turned off
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
        'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    assert ds is not None
    assert ds.GetLayerCount() == 1

    # Re try but ask for refresh
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
            'REFRESH_CACHE=YES',
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is not None or gdal.GetLastErrorMsg().find('Cannot resolve') < 0:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail(gdal.GetLastErrorMsg())

    # Re try with non existing cached schema
    gdal.Unlink('/vsimem/my/gmlas_cache/' + gdal.ReadDir('/vsimem/my/gmlas_cache')[0])

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options=[
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    assert ds is None and gdal.GetLastErrorMsg().find('Cannot resolve') >= 0

    # Cleanup
    gdal.Unlink('/vsimem/ogr_gmlas_cache.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_cache.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_cache_2.xsd')

    files = gdal.ReadDir('/vsimem/my/gmlas_cache')
    for my_file in files:
        gdal.Unlink('/vsimem/my/gmlas_cache/' + my_file)
    gdal.Rmdir('/vsimem/my/gmlas_cache')
    gdal.Rmdir('/vsimem/my')


###############################################################################
# Test good working of linking to a child through its id attribute

def test_ogr_gmlas_link_nested_independant_child():

    ds = ogr.Open('GMLAS:data/gmlas/gmlas_link_nested_independant_child.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['second_my_id'] != 'second_id':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test some pattern found in geosciml schemas


def test_ogr_gmlas_composition_compositionPart():

    ds = ogr.Open('GMLAS:data/gmlas/gmlas_composition_compositionPart.xml')

    lyr = ds.GetLayerByName('first_composition')
    f = lyr.GetNextFeature()
    if f.IsFieldSet('parent_ogr_pkid') == 0 or f.IsFieldSet('CompositionPart_pkid') == 0:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.IsFieldSet('parent_ogr_pkid') == 0 or f.IsFieldSet('CompositionPart_pkid') == 0:
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('CompositionPart')
    f = lyr.GetNextFeature()
    if f.IsFieldSet('my_id') == 0 or f.IsFieldSet('a') == 0:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.IsFieldSet('my_id') == 0 or f.IsFieldSet('a') == 0:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test that when importing GML we expose by default only elements deriving
# from _Feature/AbstractFeature


def test_ogr_gmlas_instantiate_only_gml_feature():

    with gdaltest.tempfile('/vsimem/with space/gmlas_instantiate_only_gml_feature.xsd',
                       open('data/gmlas/gmlas_instantiate_only_gml_feature.xsd', 'rb').read()):
        with gdaltest.tempfile('/vsimem/with space/gmlas_fake_gml32.xsd',
                       open('data/gmlas/gmlas_fake_gml32.xsd', 'rb').read()):
            ds = gdal.OpenEx('GMLAS:',
                            open_options=['XSD=/vsimem/with space/gmlas_instantiate_only_gml_feature.xsd'])
    assert ds.GetLayerCount() == 1
    ds = None

###############################################################################
# Test that WFS style timeStamp are ignored for hash generation


def test_ogr_gmlas_timestamp_ignored_for_hash():

    ds = ogr.Open('GMLAS:data/gmlas/gmlas_timestamp_ignored_for_hash_foo.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    pkid = f['ogr_pkid']

    ds = ogr.Open('GMLAS:data/gmlas/gmlas_timestamp_ignored_for_hash_bar.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['ogr_pkid'] != pkid:
        f.DumpReadable()
        pytest.fail(pkid)


###############################################################################
# Test dataset GetNextFeature()


def test_ogr_gmlas_dataset_getnextfeature():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml')

    assert ds.TestCapability(ogr.ODsCRandomLayerRead) == 1

    count = 0
    last_l = None
    while True:
        f, lyr = ds.GetNextFeature()
        if f is None:
            assert lyr is None
            break
        count += 1
        last_l = lyr

    base_count = 59
    assert count == base_count

    assert last_l.GetName() == 'main_elt'

    f, lyr = ds.GetNextFeature()
    assert f is None and lyr is None

    ds.ResetReading()
    last_pct = 0
    while True:
        f, l, pct = ds.GetNextFeature(include_pct=True)
        last_pct = pct
        if f is None:
            assert l is None
            break
    assert last_pct == 1.0

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['EXPOSE_METADATA_LAYERS=YES'])
    fc_map = {}
    for layer_name in ('_ogr_fields_metadata',
                       '_ogr_layers_metadata',
                       '_ogr_layer_relationships',
                       '_ogr_other_metadata'):
        fc_map[layer_name] = ds.GetLayerByName(layer_name).GetFeatureCount()
    ds = None

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['EXPOSE_METADATA_LAYERS=YES'])
    count = 0
    while True:
        f, lyr = ds.GetNextFeature()
        if f is None:
            assert lyr is None
            break
        count += 1

    expected_count = base_count
    expected_count += fc_map['_ogr_fields_metadata']
    expected_count += fc_map['_ogr_layers_metadata']
    expected_count += fc_map['_ogr_layer_relationships']
    expected_count += fc_map['_ogr_other_metadata']
    assert count == expected_count

    f, lyr = ds.GetNextFeature()
    assert f is None and lyr is None

    ds.ResetReading()

    count = 0
    while True:
        f, lyr = ds.GetNextFeature()
        if f is None:
            assert lyr is None
            break
        count += 1

    assert count == expected_count

    for layers in [['_ogr_fields_metadata'],
                   ['_ogr_layers_metadata'],
                   ['_ogr_layer_relationships'],
                   ['_ogr_fields_metadata', '_ogr_layers_metadata'],
                   ['_ogr_fields_metadata', '_ogr_layer_relationships'],
                   ['_ogr_layers_metadata', '_ogr_layer_relationships'],
                  ]:

        ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml')
        expected_count = base_count
        for layer in layers:
            ds.GetLayerByName(layer)
            expected_count += fc_map[layer]

        count = 0
        while True:
            f, lyr = ds.GetNextFeature()
            if f is None:
                assert lyr is None
                break
            count += 1

        assert count == expected_count

        f, lyr = ds.GetNextFeature()
        assert f is None and lyr is None

    # Test iterating over metadata layers on XSD-only based dataset
    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_test1.xsd', 'EXPOSE_METADATA_LAYERS=YES'])
    count = 0
    last_l = None
    while True:
        f, lyr = ds.GetNextFeature()
        if f is None:
            assert lyr is None
            break
        count += 1
        last_l = lyr

    assert count != 0

###############################################################################
#  Test that with schemas that have a structure like a base:identifier, we
# will inline it.


def test_ogr_gmlas_inline_identifier():

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_inline_identifier.xsd'])
    if ds.GetLayerCount() != 2:
        for i in range(ds.GetLayerCount()):
            print(ds.GetLayer(i).GetName())
        pytest.fail(ds.GetLayerCount())
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldIndex('identifier_foo') >= 0

###############################################################################
#  Test that we can handle things like gml:name and au:name


def test_ogr_gmlas_avoid_same_name_inlined_classes():

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_avoid_same_name_inlined_classes.xsd'])
    assert ds.GetLayerCount() == 3
    lyr = ds.GetLayerByName('myFeature_ns1_dt')
    assert lyr is not None
    lyr = ds.GetLayerByName('myFeature_ns2_dt')
    assert lyr is not None


###############################################################################
#  Test validation with an optional fixed attribute that is ignored

def test_ogr_gmlas_validate_ignored_fixed_attribute():

    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    gdal.OpenEx('GMLAS:data/gmlas/gmlas_validate_ignored_fixed_attribute.xml',
                open_options=['VALIDATE=YES',
                              'CONFIG_FILE=<Configuration><IgnoredXPaths><XPath>@bar</XPath></IgnoredXPaths></Configuration>'])
    gdal.PopErrorHandler()
    assert not myhandler.error_list


###############################################################################
#  Test REMOVE_UNUSED_LAYERS and REMOVE_UNUSED_FIELDS options

def test_ogr_gmlas_remove_unused_layers_and_fields():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_remove_unused_layers_and_fields.xml',
                     open_options=['REMOVE_UNUSED_LAYERS=YES',
                                   'REMOVE_UNUSED_FIELDS=YES'])
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if lyr.GetLayerDefn().GetFieldCount() != 4:
        f.DumpReadable()
        pytest.fail(lyr.GetLayerDefn().GetFieldCount())
    assert f['used1'] == 'foo' and f['used2'] == 'bar' and f['nillable_nilReason'] == 'unknown'

    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    if lyr.GetFeatureCount() != 1:
        for f in lyr:
            f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('_ogr_fields_metadata')
    if lyr.GetFeatureCount() != 7:
        for f in lyr:
            f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('_ogr_layer_relationships')
    if lyr.GetFeatureCount() != 0:
        for f in lyr:
            f.DumpReadable()
        pytest.fail()


###############################################################################
#  Test xlink resolution


def test_ogr_gmlas_xlink_resolver():

    drv = gdal.GetDriverByName('HTTP')
    if drv is None:
        pytest.skip()

    (webserver_process, webserver_port) = webserver.launch(handler=GMLASHTTPHandler)
    if webserver_port == 0:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_xlink_resolver.xsd',
                           """<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              xmlns:xlink="http://fake_xlink"
              elementFormDefault="qualified"
              attributeFormDefault="unqualified">

<xs:import namespace="http://fake_xlink" schemaLocation="ogr_gmlas_xlink_resolver_fake_xlink.xsd"/>

<xs:element name="FeatureCollection">
  <xs:complexType>
    <xs:sequence>
        <xs:element ref="main_elt" minOccurs="0" maxOccurs="unbounded"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>

<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="my_link">
            <xs:complexType>
                <xs:sequence/>
                <xs:attribute name="attr_before" type="xs:string"/>
                <xs:attribute ref="xlink:href"/>
                <xs:attribute name="attr_after" type="xs:string"/>
            </xs:complexType>
        </xs:element>
        <xs:element name="my_link2" minOccurs="0">
            <xs:complexType>
                <xs:sequence/>
                <xs:attribute name="attr_before" type="xs:string"/>
                <xs:attribute ref="xlink:href"/>
                <xs:attribute name="attr_after" type="xs:string"/>
            </xs:complexType>
        </xs:element>
    </xs:sequence>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_xlink_resolver_fake_xlink.xsd',
                           open('data/gmlas/gmlas_fake_xlink.xsd', 'rb').read())

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_xlink_resolver.xml',
                           """
<FeatureCollection xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          xmlns:xlink="http://fake_xlink"
          xsi:noNamespaceSchemaLocation="ogr_gmlas_xlink_resolver.xsd">
  <main_elt>
    <my_link attr_before="a" xlink:href="http://localhost:%d/vsimem/resource.xml" attr_after="b"/>
  </main_elt>
  <main_elt>
    <my_link xlink:href="http://localhost:%d/vsimem/resource2.xml"/>
  </main_elt>
</FeatureCollection>""" % (webserver_port, webserver_port))

    # By default, no resolution
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldIndex('my_link_rawcontent') >= 0:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    ds = None

    # Enable resolution, but only from local cache
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>false</AllowRemoteDownload>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldIndex('my_link_rawcontent') < 0:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.IsFieldSet('my_link_rawcontent'):
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    # Try again but this time with the cached file
    cached_file = '/vsimem/gmlas_xlink_cache/localhost_%d_vsimem_resource.xml' % webserver_port
    gdal.FileFromMemBuffer(cached_file, 'foo')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] != 'foo':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    ds = None
    gdal.Unlink(cached_file)

    # Enable remote resolution (but local caching disabled)
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'bar')
    gdal.FileFromMemBuffer('/vsimem/resource2.xml', 'baz')
    gdal.SetConfigOption('GMLAS_XLINK_RAM_CACHE_SIZE', '5')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""])
    gdal.SetConfigOption('GMLAS_XLINK_RAM_CACHE_SIZE', None)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] != 'bar':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    # Check that the content is not cached
    if gdal.VSIStatL(cached_file) is not None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    # Delete the remote file and check that we can retrieve it from RAM cache
    gdal.Unlink('/vsimem/resource.xml')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] != 'bar':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] != 'baz':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    gdal.Unlink('/vsimem/resource2.xml')
    lyr.ResetReading()
    # /vsimem/resource.xml has been evicted from the cache
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] is not None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] != 'baz':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    ds = None

    # Enable remote resolution and caching
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'bar')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <CacheResults>true</CacheResults>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] != 'bar':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    # Check that the content is cached
    if gdal.VSIStatL(cached_file) is None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    ds = None

    # Enable remote resolution and caching and REFRESH_CACHE
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'baz')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=['REFRESH_CACHE=YES', """CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <CacheResults>true</CacheResults>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent'] != 'baz':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    # Check that the content is cached
    if gdal.VSIStatL(cached_file) is None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    ds = None

    # Test absent remote resource
    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_xlink_resolver_absent_resource.xml',
                           """
<main_elt xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          xmlns:xlink="http://fake_xlink"
          xsi:noNamespaceSchemaLocation="ogr_gmlas_xlink_resolver.xsd">
    <my_link xlink:href="http://localhost:%d/vsimem/resource_not_existing.xml"/>
</main_elt>""" % webserver_port)
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver_absent_resource.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f.IsFieldSet('my_link_rawcontent'):
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    ds = None

    # Test file size limit
    gdal.Unlink(cached_file)
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <MaxFileSize>1</MaxFileSize>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <CacheResults>true</CacheResults>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if gdal.GetLastErrorMsg() == '':
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    # Check that the content is not cached
    if gdal.VSIStatL(cached_file) is not None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    ds = None

    # Test with URL specific rule with RawContent resolution
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'bar')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <URLSpecificResolution>
                <URLPrefix>http://localhost:%d/vsimem/</URLPrefix>
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <ResolutionMode>RawContent</ResolutionMode>
                <CacheResults>true</CacheResults>
            </URLSpecificResolution>
        </XLinkResolution></Configuration>""" % webserver_port])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_attr_before'] != 'a' or \
       f['my_link_href'] != 'http://localhost:%d/vsimem/resource.xml' % webserver_port or \
       f['my_link_rawcontent'] != 'bar' or \
       f['my_link_attr_after'] != 'b':
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    # Check that the content is cached
    if gdal.VSIStatL(cached_file) is None:
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()
    ds = None

    # Test with URL specific rule with FieldsFromXPath resolution
    gdal.FileFromMemBuffer('/vsimem/subdir1/resource.xml', """
<?xml version='1.0' encoding='UTF-8'?>
<myns:top>
    <myns:foo>fooVal</myns:foo>
    <myns:bar>123</myns:bar>
</myns:top>""")
    gdal.FileFromMemBuffer('/vsimem/subdir2/resource2_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_very_long.xml', """
<?xml version='1.0' encoding='UTF-8'?>
<myns:top>
    <myns:foo>fooVal2</myns:foo>
    <myns:foo>fooVal3</myns:foo>
    <myns:baz val="345"/>
    <myns:xml_blob>foo<blob/>bar</myns:xml_blob>
    <long>1234567890123</long>
    <double>1.25</double>
    <datetime>2016-10-07T12:34:56Z</datetime>
</myns:top>""")
    gdal.FileFromMemBuffer('/vsimem/non_matching_resource.xml', 'foo')

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_xlink_resolver.xml',
                           """
<FeatureCollection xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          xmlns:xlink="http://fake_xlink"
          xsi:noNamespaceSchemaLocation="ogr_gmlas_xlink_resolver.xsd">
  <main_elt>
    <my_link attr_before="a" xlink:href="http://localhost:%d/vsimem/subdir1/resource.xml" attr_after="b"/>
    <my_link2 attr_before="a2" xlink:href="http://localhost:%d/vsimem/subdir2/resource2_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_very_long.xml" attr_after="b2"/>
  </main_elt>
  <main_elt>
    <my_link attr_before="a" xlink:href="http://localhost:%d/vsimem/non_matching_resource.xml" attr_after="b"/>
    <my_link2 attr_before="a2" xlink:href="http://localhost:%d/vsimem/subdir1/resource.xml" attr_after="b2"/>
  </main_elt>
</FeatureCollection>""" % (webserver_port, webserver_port, webserver_port, webserver_port))

    config_file = """<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <URLSpecificResolution>
                <URLPrefix>http://localhost:%d/vsimem/subdir1</URLPrefix>
                <HTTPHeader>
                    <Name>Accept</Name>
                    <Value>application/x-iso19135+xml</Value>
                </HTTPHeader>
                <HTTPHeader>
                    <Name>Accept-Language</Name>
                    <Value>en</Value>
                </HTTPHeader>
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <ResolutionMode>FieldsFromXPath</ResolutionMode>
                <CacheResults>true</CacheResults>
                <Field>
                    <Name>foo</Name>
                    <Type>string</Type>
                    <XPath>myns:top/myns:foo</XPath>
                </Field>
                <Field>
                    <Name>bar</Name>
                    <Type>integer</Type>
                    <XPath>myns:top/myns:bar</XPath>
                </Field>
            </URLSpecificResolution>
            <URLSpecificResolution>
                <URLPrefix>http://localhost:%d/vsimem/subdir2</URLPrefix>
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <ResolutionMode>FieldsFromXPath</ResolutionMode>
                <CacheResults>true</CacheResults>
                <Field>
                    <Name>foo</Name>
                    <Type>string</Type>
                    <XPath>myns:top/myns:foo</XPath>
                </Field>
                <Field>
                    <Name>baz</Name>
                    <Type>integer</Type>
                    <XPath>/myns:top/myns:baz/@val</XPath>
                </Field>
                <Field>
                    <Name>xml_blob</Name>
                    <Type>string</Type>
                    <XPath>//myns:xml_blob</XPath>
                </Field>
                <Field>
                    <Name>long</Name>
                    <Type>long</Type>
                    <XPath>//long</XPath>
                </Field>
                <Field>
                    <Name>double</Name>
                    <Type>double</Type>
                    <XPath>//double</XPath>
                </Field>
                <Field>
                    <Name>datetime</Name>
                    <Type>dateTime</Type>
                    <XPath>//datetime</XPath>
                </Field>
            </URLSpecificResolution>
        </XLinkResolution></Configuration>""" % (webserver_port, webserver_port)

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=['CONFIG_FILE=' + config_file])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_attr_before'] != 'a' or \
       f['my_link_href'] != 'http://localhost:%d/vsimem/subdir1/resource.xml' % webserver_port or \
       f['my_link_foo'] != 'fooVal' or \
       f['my_link_bar'] != 123 or \
       f['my_link_attr_after'] != 'b' or \
       f['my_link2_attr_before'] != 'a2' or \
       f['my_link2_href'] != 'http://localhost:%d/vsimem/subdir2/resource2_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_very_long.xml' % webserver_port or \
       f['my_link2_foo'] != 'fooVal2 fooVal3' or \
       f['my_link2_baz'] != 345 or \
       f['my_link2_xml_blob'] != """foo<blob />
bar""" or \
       f['my_link2_long'] != 1234567890123 or \
       f['my_link2_double'] != 1.25 or \
       f['my_link2_datetime'] != '2016/10/07 12:34:56+00' or \
       f['my_link2_bar'] is not None or \
       f['my_link2_attr_after'] != 'b2':
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['my_link2_bar'] != 123:
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    gdal.Unlink('/vsimem/subdir1/resource.xml')
    gdal.Unlink('/vsimem/subdir2/resource2_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_very_long.xml')

    # Test caching
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
                     open_options=['CONFIG_FILE=' + config_file])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_attr_before'] != 'a' or \
       f['my_link_href'] != 'http://localhost:%d/vsimem/subdir1/resource.xml' % webserver_port or \
       f['my_link_foo'] != 'fooVal' or \
       f['my_link_bar'] != 123 or \
       f['my_link_attr_after'] != 'b' or \
       f['my_link2_attr_before'] != 'a2' or \
       f['my_link2_href'] != 'http://localhost:%d/vsimem/subdir2/resource2_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_very_long.xml' % webserver_port or \
       f['my_link2_foo'] != 'fooVal2 fooVal3' or \
       f['my_link2_baz'] != 345 or \
       f['my_link2_bar'] is not None or \
       f['my_link2_attr_after'] != 'b2':
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        pytest.fail()

    ds = None

    webserver.server_stop(webserver_process, webserver_port)

    gdal.Unlink('/vsimem/ogr_gmlas_xlink_resolver.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_xlink_resolver_fake_xlink.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_xlink_resolver.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_xlink_resolver_absent_resource.xml')
    fl = gdal.ReadDir('/vsimem/gmlas_xlink_cache')
    if fl is not None:
        for filename in fl:
            gdal.Unlink('/vsimem/gmlas_xlink_cache/' + filename)
    gdal.Unlink('/vsimem/gmlas_xlink_cache')
    gdal.Unlink('/vsimem/resource.xml')
    gdal.Unlink('/vsimem/resource2.xml')
    gdal.Unlink('/vsimem/subdir1/resource.xml')
    gdal.Unlink('/vsimem/subdir2/resource2_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_very_long.xml')
    gdal.Unlink('/vsimem/non_matching_resource.xml')

###############################################################################
# Test UTF-8 support


def test_ogr_gmlas_recoding():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_recoding.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['attr'] != '\u00e9':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test schema without namespace prefix


def test_ogr_gmlas_schema_without_namespace_prefix():

    # Generic http:// namespace URI
    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_schema_without_namespace_prefix_generic_http_uri.xsd'])
    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    f = lyr.GetNextFeature()
    if f['layer_xpath'] != 'my_ns:main_elt':
        f.DumpReadable()
        pytest.fail()

    gdal.Unlink('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd')

    # http://www.opengis.net/ namespace URI

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_schema_without_namespace_prefix_opengis_uri.xsd'])
    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    f = lyr.GetNextFeature()
    if f['layer_xpath'] != 'fake_3_0:main_elt':
        f.DumpReadable()
        pytest.fail()

    gdal.Unlink('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd')

    # Non http:// namespace URI

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_schema_without_namespace_prefix_non_http_uri.xsd'])
    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    f = lyr.GetNextFeature()
    if f['layer_xpath'] != 'my_namespace:main_elt':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test parsing truncated XML


def test_ogr_gmlas_truncated_xml():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_truncated_xml.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test identifier truncation


def test_ogr_gmlas_identifier_truncation():

    ds = gdal.OpenEx('GMLAS:', open_options=[
        'XSD=data/gmlas/gmlas_identifier_truncation.xsd',
        'CONFIG_FILE=<Configuration><LayerBuildingRules><IdentifierMaxLength>10</IdentifierMaxLength><PostgreSQLIdentifierLaundering>false</PostgreSQLIdentifierLaundering></LayerBuildingRules></Configuration>'])
    lyr = ds.GetLayerByName('v_l_i_clas')
    assert lyr is not None, ds.GetLayer(0).GetName()
    s = lyr.GetLayerDefn().GetFieldDefn(1).GetName()
    assert s == 'v_l_idTifi'
    s = lyr.GetLayerDefn().GetFieldDefn(2).GetName()
    assert s == 'an_lo_ide1'
    s = lyr.GetLayerDefn().GetFieldDefn(3).GetName()
    assert s == 'an_lo_ide2'
    s = lyr.GetLayerDefn().GetFieldDefn(4).GetName()
    assert s == 'x'
    s = lyr.GetLayerDefn().GetFieldDefn(5).GetName()
    assert s == 'noTCAMELCa'
    s = lyr.GetLayerDefn().GetFieldDefn(6).GetName()
    assert s == 'suuuuuuuuu'
    s = lyr.GetLayerDefn().GetFieldDefn(7).GetName()
    assert s == '_r_l_o_n_g'
    lyr = ds.GetLayerByName('a_l_i_cla1')
    assert lyr is not None, ds.GetLayer(1).GetName()
    lyr = ds.GetLayerByName('a_l_i_cla2')
    assert lyr is not None, ds.GetLayer(2).GetName()
    lyr = ds.GetLayerByName('y')
    assert lyr is not None, ds.GetLayer(3).GetName()
    ds = None

###############################################################################
# Test behaviour when identifiers have same case


def test_ogr_gmlas_identifier_case_ambiguity():

    ds = gdal.OpenEx('GMLAS:', open_options=[
        'XSD=data/gmlas/gmlas_identifier_case_ambiguity.xsd',
        'CONFIG_FILE=<Configuration><LayerBuildingRules><PostgreSQLIdentifierLaundering>false</PostgreSQLIdentifierLaundering></LayerBuildingRules></Configuration>'])
    lyr = ds.GetLayerByName('differentcase1')
    assert lyr is not None, ds.GetLayer(0).GetName()
    s = lyr.GetLayerDefn().GetFieldDefn(1).GetName()
    assert s == 'differentcase1'
    s = lyr.GetLayerDefn().GetFieldDefn(2).GetName()
    assert s == 'DifferentCASE2'
    lyr = ds.GetLayerByName('DifferentCASE2')
    assert lyr is not None, ds.GetLayer(0).GetName()
    ds = None

###############################################################################
# Test writing support


def test_ogr_gmlas_writer():

    if ogr.GetDriverByName('SQLite') is None:
        pytest.skip()

    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['EXPOSE_METADATA_LAYERS=YES'])
    tmp_ds = gdal.VectorTranslate('/vsimem/ogr_gmlas_writer.db', src_ds, format='SQLite')
    src_ds = None
    ret_ds = gdal.VectorTranslate('tmp/gmlas_test1_generated.xml', tmp_ds,
                                  format='GMLAS',
                                  datasetCreationOptions=['WRAPPING=GMLAS_FEATURECOLLECTION'])
    tmp_ds = None
    gdal.Unlink('/vsimem/ogr_gmlas_writer.db')

    assert ret_ds is not None

###############################################################################
# Check the generated .xml and .xsd


def test_ogr_gmlas_writer_check_xml_xsd():

    if ogr.GetDriverByName('SQLite') is None:
        pytest.skip()

    got = open('tmp/gmlas_test1_generated.xml', 'rt').read()
    got = got.replace('\r\n', '\n')
    pos = got.find('http://myns ') + len('http://myns ')
    pos_end = got.find('"', pos)
    absolute_xsd = got[pos:pos_end]
    assert absolute_xsd.endswith('gmlas_test1.xsd') and os.path.exists(absolute_xsd)
    got = got.replace(absolute_xsd, 'gmlas_test1.xsd')

    expected = open('data/gmlas/gmlas_test1_generated.xml', 'rt').read()
    expected = expected.replace('\r\n', '\n')

    if got != expected:
        print(got)
        print('')

        print('Diff:')
        os.system('diff -u data/gmlas/gmlas_test1_generated.xml tmp/gmlas_test1_generated.xml')
        pytest.fail('Got:')

    got = open('tmp/gmlas_test1_generated.xsd', 'rt').read()
    got = got.replace('\r\n', '\n')
    pos = got.find('schemaLocation="') + len('schemaLocation="')
    pos_end = got.find('"', pos)
    absolute_xsd = got[pos:pos_end]
    assert absolute_xsd.endswith('gmlas_test1.xsd') and os.path.exists(absolute_xsd)
    got = got.replace(absolute_xsd, 'gmlas_test1.xsd')

    expected = open('data/gmlas/gmlas_test1_generated.xsd', 'rt').read()
    expected = expected.replace('\r\n', '\n')

    if got != expected:
        print(got)
        print('')

        print('Diff:')
        os.system('diff -u data/gmlas/gmlas_test1_generated.xsd tmp/gmlas_test1_generated.xsd')
        pytest.fail('Got:')


###############################################################################
# Check that the .xml read back by the GMLAS driver has the same content
# as the original one.


def test_ogr_gmlas_writer_check_xml_read_back():

    if ogr.GetDriverByName('SQLite') is None:
        pytest.skip()

    # Skip tests when -fsanitize is used
    if gdaltest.is_travis_branch('sanitize'):
        pytest.skip('Skipping because of -sanitize')

    import test_cli_utilities

    if test_cli_utilities.get_ogrinfo_path() is None:
        gdal.Unlink('tmp/gmlas_test1_generated.xml')
        gdal.Unlink('tmp/gmlas_test1_generated.xsd')
        pytest.skip()

    # Compare the ogrinfo dump of the generated .xml with a reference one
    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() +
                               ' -ro -al GMLAS:tmp/gmlas_test1_generated.xml -oo VALIDATE=YES ' +
                               '-oo EXPOSE_METADATA_LAYERS=YES ' +
                               '-oo @KEEP_RELATIVE_PATHS_FOR_METADATA=YES ' +
                               '-oo @EXPOSE_SCHEMAS_NAME_IN_METADATA=NO ' +
                               '-oo @EXPOSE_CONFIGURATION_IN_METADATA=NO -oo @HASH=fake_hash')
    expected = open('data/gmlas/gmlas_test1.txt', 'rt').read()
    expected = expected.replace('\r\n', '\n')
    expected = expected.replace('data/gmlas/gmlas_test1.xml', 'tmp/gmlas_test1_generated.xml')
    expected = expected.replace('data/gmlas/gmlas_test1.xsd', os.path.join(os.getcwd(), 'data/gmlas/gmlas_test1.xsd'))
    expected = expected.replace('\\', '/')
    ret_for_comparison = ret.replace('\r\n', '\n')
    ret_for_comparison = ret_for_comparison.replace('\\', '/')
    ret_for_comparison = ret_for_comparison.replace('fake_hash', '3CF9893502A592E8CF5EA6EF3D8F8C7B')

    if ret_for_comparison != expected:
        print(open('tmp/gmlas_test1_generated.xml', 'rt').read())
        print('')

        print('XSD:')
        print(open('tmp/gmlas_test1_generated.xsd', 'rt').read())
        print('')

        print('ogrinfo dump:')
        print(ret)
        print('')

        open('tmp/gmlas_test1_generated_got.txt', 'wt').write(ret_for_comparison)
        open('tmp/gmlas_test1_generated_expected.txt', 'wt').write(expected)
        print('Diff:')
        os.system('diff -u tmp/gmlas_test1_generated_expected.txt tmp/gmlas_test1_generated_got.txt')

        os.unlink('tmp/gmlas_test1_generated_expected.txt')
        os.unlink('tmp/gmlas_test1_generated_got.txt')
        pytest.fail('XML:')

    gdal.Unlink('tmp/gmlas_test1_generated.xml')
    gdal.Unlink('tmp/gmlas_test1_generated.xsd')

###############################################################################
# Test writing support with geometries


def test_ogr_gmlas_writer_gml():

    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml',
                         open_options=['EXPOSE_METADATA_LAYERS=YES', '@HASH=hash'])
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None
    # Test also with GMLAS: prefix as it is likely people might use it
    # as it is needed for the read side.
    ret_ds = gdal.VectorTranslate('GMLAS:/vsimem/ogr_gmlas_writer_gml.xml', tmp_ds,
                                  format='GMLAS',
                                  datasetCreationOptions=['WRAPPING=GMLAS_FEATURECOLLECTION',
                                                          'LAYERS={SPATIAL_LAYERS}'])
    tmp_ds = None

    assert ret_ds is not None

    f = gdal.VSIFOpenL('/vsimem/ogr_gmlas_writer_gml.xml', 'rb')
    assert f is not None
    content = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/ogr_gmlas_writer_gml.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_writer_gml.xsd')

    assert 'xmlns:gml="http://fake_gml32"' in content

    assert '<ogr:geometryProperty><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="hash_test_1.geom0"><gml:pos>49 2</gml:pos></gml:Point></ogr:geometryProperty>' in content

    assert '<ogr:pointProperty><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326" gml:id="hash_test_1.geom2"><gml:pos>50 3</gml:pos></gml:Point></ogr:pointProperty>' in content

    assert '      <ogr:pointPropertyRepeated><gml:Point gml:id="hash_test_1.geom13.0"><gml:pos>0 1</gml:pos></gml:Point></ogr:pointPropertyRepeated>' in content

    assert '      <ogr:pointPropertyRepeated><gml:Point gml:id="hash_test_1.geom13.1"><gml:pos>1 2</gml:pos></gml:Point></ogr:pointPropertyRepeated>' in content

###############################################################################
# Test writing support with geometries and -a_srs


def test_ogr_gmlas_writer_gml_assign_srs():

    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml',
                         open_options=['EXPOSE_METADATA_LAYERS=YES', '@HASH=hash'])
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None

    ret_ds = gdal.VectorTranslate('/vsimem/ogr_gmlas_writer_gml.xml', tmp_ds,
                                  format='GMLAS',
                                  dstSRS='EPSG:32631',
                                  reproject=False)
    tmp_ds = None

    assert ret_ds is not None

    f = gdal.VSIFOpenL('/vsimem/ogr_gmlas_writer_gml.xml', 'rb')
    assert f is not None
    content = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/ogr_gmlas_writer_gml.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_writer_gml.xsd')

    assert 'http://www.opengis.net/def/crs/EPSG/0/32631' in content

    # No geometry, but to test that the proxied ExecuteSQL() works

    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_test1.xml', open_options=['EXPOSE_METADATA_LAYERS=YES'])
    tmp_ds = gdal.VectorTranslate('/vsimem/ogr_gmlas_writer.db', src_ds, format='SQLite')
    src_ds = None
    gdal.VectorTranslate('/vsimem/gmlas_test1_generated_ref0.xml', tmp_ds,
                         format='GMLAS',
                         dstSRS='EPSG:32631',
                         reproject=False,
                         datasetCreationOptions=['WRAPPING=GMLAS_FEATURECOLLECTION'])
    gdal.VectorTranslate('/vsimem/gmlas_test1_generated_asrs.xml', tmp_ds,
                         format='GMLAS',
                         dstSRS='EPSG:32631',
                         reproject=False,
                         datasetCreationOptions=['WRAPPING=GMLAS_FEATURECOLLECTION'])
    tmp_ds = None
    gdal.Unlink('/vsimem/ogr_gmlas_writer.db')

    assert gdal.VSIStatL('/vsimem/gmlas_test1_generated_ref0.xml').size == gdal.VSIStatL('/vsimem/gmlas_test1_generated_asrs.xml').size

    gdal.Unlink('/vsimem/gmlas_test1_generated_ref0.xml')
    gdal.Unlink('/vsimem/gmlas_test1_generated_ref0.xsd')
    gdal.Unlink('/vsimem/gmlas_test1_generated_asrs.xml')
    gdal.Unlink('/vsimem/gmlas_test1_generated_asrs.xsd')

###############################################################################
# Test writing support with geometries with original XML content preserved


def test_ogr_gmlas_writer_gml_original_xml():

    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml',
                         open_options=['EXPOSE_METADATA_LAYERS=YES',
                                       'CONFIG_FILE=<Configuration><LayerBuildingRules><GML><IncludeGeometryXML>true</IncludeGeometryXML></GML></LayerBuildingRules></Configuration>'])
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None
    ret_ds = gdal.VectorTranslate('/vsimem/ogr_gmlas_writer_gml.xml', tmp_ds, format='GMLAS',
                                  datasetCreationOptions=['WRAPPING=GMLAS_FEATURECOLLECTION'])
    tmp_ds = None

    assert ret_ds is not None
    ret_ds = None

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_writer_gml.xml', open_options=['VALIDATE=YES'])
    assert ds is not None and gdal.GetLastErrorMsg() == ''
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_gmlas_writer_gml.xml', 'rb')
    assert f is not None
    content = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/ogr_gmlas_writer_gml.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_writer_gml.xsd')

    assert '<ogr:geometryProperty> <gml:Point gml:id="poly.geom.Geometry" srsName="urn:ogc:def:crs:EPSG::4326"> <gml:pos>49 2</gml:pos> </gml:Point> </ogr:geometryProperty>' in content

    assert '      <ogr:pointPropertyRepeated><gml:Point gml:id="poly.geom.pointPropertyRepeated.1"><gml:pos>0 1</gml:pos></gml:Point></ogr:pointPropertyRepeated>' in content

    assert '      <ogr:pointPropertyRepeated><gml:Point gml:id="poly.geom.pointPropertyRepeated.2"><gml:pos>1 2</gml:pos></gml:Point></ogr:pointPropertyRepeated>' in content

###############################################################################
# Test writing support with XSD, INDENT_SIZE, COMMENT, OUTPUT_XSD_FILENAME, TIMESTAMP options


def test_ogr_gmlas_writer_options():

    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml', open_options=['@HASH=hash'])
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None
    ret_ds = gdal.VectorTranslate('/vsimem/ogr_gmlas_writer_options.xml', tmp_ds, format='GMLAS',
                                  datasetCreationOptions=['LAYERS=test',
                                                          'WRAPPING=GMLAS_FEATURECOLLECTION',
                                                          'INPUT_XSD=data/gmlas/gmlas_geometryproperty_gml32.xsd',
                                                          'INDENT_SIZE=4',
                                                          'COMMENT=---a comment---',
                                                          'SRSNAME_FORMAT=OGC_URN',
                                                          'OUTPUT_XSD_FILENAME=/vsimem/my_schema.xsd'])
    tmp_ds = None

    assert ret_ds is not None

    f = gdal.VSIFOpenL('/vsimem/ogr_gmlas_writer_options.xml', 'rb')
    assert f is not None
    content = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/ogr_gmlas_writer_options.xml')

    assert gdal.VSIStatL('/vsimem/my_schema.xsd') is not None

    gdal.Unlink('/vsimem/my_schema.xsd')

    # Test indentation size
    assert '\n        <ogr:test gml:id="poly.0">' in content

    # Test comment
    assert '\n<!-- - - -a comment- - - -->' in content

    # Test OUTPUT_XSD_FILENAME
    assert '/vsimem/my_schema.xsd' in content

    # Test SRSNAME_FORMAT=OGC_URN
    assert '<ogr:geometryProperty><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="hash_test_1.geom0"><gml:pos>49 2</gml:pos></gml:Point></ogr:geometryProperty>' in content

    # Test TIMESTAMP option
    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml',
                         open_options=['@HASH=hash', 'EXPOSE_METADATA_LAYERS=YES'])
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None
    ret_ds = gdal.VectorTranslate('/vsimem/ogr_gmlas_writer_options.xml', tmp_ds, format='GMLAS',
                                  datasetCreationOptions=['TIMESTAMP=1970-01-01T12:34:56Z', '@REOPEN_DATASET_WITH_GMLAS=NO'])
    tmp_ds = None

    assert ret_ds is not None

    f = gdal.VSIFOpenL('/vsimem/ogr_gmlas_writer_options.xml', 'rb')
    assert f is not None
    content = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/ogr_gmlas_writer_options.xml')

    assert gdal.VSIStatL('/vsimem/my_schema.xsd') is None

    assert ('timeStamp="1970-01-01T12:34:56Z"' in content and \
       'xsi:schemaLocation="http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd ' in content)

    # Test WFS20_SCHEMALOCATION option
    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml',
                         open_options=['@HASH=hash', 'EXPOSE_METADATA_LAYERS=YES'])
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None
    ret_ds = gdal.VectorTranslate('/vsimem/ogr_gmlas_writer_options.xml', tmp_ds, format='GMLAS',
                                  datasetCreationOptions=['WFS20_SCHEMALOCATION=/vsimem/fake_wfs.xsd'])
    tmp_ds = None

    assert ret_ds is not None

    gdal.FileFromMemBuffer('/vsimem/fake_wfs.xsd',
                           """
<!-- fake wfs schema enough for our purposes -->
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://www.opengis.net/wfs/2.0"
           elementFormDefault="qualified">
    <xs:element name="FeatureCollection">
        <xs:complexType>
            <xs:sequence>
                <xs:element name="member" minOccurs="0" maxOccurs="unbounded"/>
            </xs:sequence>
            <xs:attribute name="timeStamp" type="xs:dateTime" use="required"/>
            <xs:attribute name="numberMatched" type="xs:string" fixed="unknown" use="required"/>
            <xs:attribute name="numberReturned" type="xs:nonNegativeInteger" use="required"/>
        </xs:complexType>
    </xs:element>
</xs:schema>
""")
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_writer_options.xml', open_options=['VALIDATE=YES'])
    gdal.Unlink('/vsimem/fake_wfs.xsd')

    assert ds is not None and gdal.GetLastErrorMsg() == ''
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_gmlas_writer_options.xml', 'rb')
    assert f is not None
    content = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/ogr_gmlas_writer_options.xml')

    assert gdal.VSIStatL('/vsimem/my_schema.xsd') is None

    assert 'xsi:schemaLocation="http://www.opengis.net/wfs/2.0 /vsimem/fake_wfs.xsd ' in content

###############################################################################
# Test writing support error handle


def test_ogr_gmlas_writer_errors():

    # Source dataset is empty
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', gdal.GetDriverByName('Memory').Create('', 0, 0, 0, 0), format='GMLAS')
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Source dataset has no layers') >= 0

    # Missing input schemas
    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml')
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', tmp_ds, format='GMLAS')
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Cannot establish schema since no INPUT_XSD creation option specified and no _ogr_other_metadata found in source dataset') >= 0

    # Invalid input schema
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', tmp_ds, format='GMLAS',
                                      datasetCreationOptions=['INPUT_XSD=/i_do_not/exist.xsd'])
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Cannot resolve /i_do_not/exist.xsd') >= 0

    # Invalid output .xml name
    src_ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometryproperty_gml32_no_error.gml',
                         open_options=['EXPOSE_METADATA_LAYERS=YES'])
    tmp_ds = gdal.VectorTranslate('', src_ds, format='Memory')
    src_ds = None
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/i_am/not/valid.xml', tmp_ds, format='GMLAS',
                                      datasetCreationOptions=['GENERATE_XSD=NO'])
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Cannot create /i_am/not/valid.xml') >= 0

    # .xsd extension not allowed
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/i_am/not/valid.xsd', tmp_ds, format='GMLAS',
                                      datasetCreationOptions=['GENERATE_XSD=NO'])
    assert ret_ds is None and gdal.GetLastErrorMsg().find('.xsd extension is not valid') >= 0

    # Invalid output .xsd name
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', tmp_ds, format='GMLAS',
                                      datasetCreationOptions=['WRAPPING=GMLAS_FEATURECOLLECTION',
                                                              'OUTPUT_XSD_FILENAME=/i_am/not/valid.xsd'])
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Cannot create /i_am/not/valid.xsd') >= 0
    gdal.Unlink('/vsimem/valid.xml')

    # Invalid CONFIG_FILE
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', tmp_ds, format='GMLAS',
                                      datasetCreationOptions=['CONFIG_FILE=/i/do_not/exist'])
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Loading of configuration failed') >= 0

    # Invalid layer name
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', tmp_ds, format='GMLAS',
                                      datasetCreationOptions=['LAYERS=foo'])
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Layer foo specified in LAYERS option does not exist') >= 0
    gdal.Unlink('/vsimem/valid.xml')

    # _ogr_layers_metadata not found
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, 0)
    src_ds.CreateLayer('_ogr_other_metadata')
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', src_ds, format='GMLAS')
    assert ret_ds is None and gdal.GetLastErrorMsg().find('_ogr_layers_metadata not found') >= 0

    # _ogr_fields_metadata not found
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, 0)
    src_ds.CreateLayer('_ogr_other_metadata')
    src_ds.CreateLayer('_ogr_layers_metadata')
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', src_ds, format='GMLAS')
    assert ret_ds is None and gdal.GetLastErrorMsg().find('_ogr_fields_metadata not found') >= 0

    # _ogr_layer_relationships not found
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, 0)
    src_ds.CreateLayer('_ogr_other_metadata')
    src_ds.CreateLayer('_ogr_layers_metadata')
    src_ds.CreateLayer('_ogr_fields_metadata')
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', src_ds, format='GMLAS')
    assert ret_ds is None and gdal.GetLastErrorMsg().find('_ogr_layer_relationships not found') >= 0

    # Cannot find field layer_name in _ogr_layers_metadata layer
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, 0)
    src_ds.CreateLayer('_ogr_other_metadata')
    src_ds.CreateLayer('_ogr_layers_metadata')
    src_ds.CreateLayer('_ogr_fields_metadata')
    src_ds.CreateLayer('_ogr_layer_relationships')
    with gdaltest.error_handler():
        ret_ds = gdal.VectorTranslate('/vsimem/valid.xml', src_ds, format='GMLAS')
    assert ret_ds is None and gdal.GetLastErrorMsg().find('Cannot find field layer_name in _ogr_layers_metadata layer') >= 0
    gdal.Unlink('/vsimem/valid.xml')
    gdal.Unlink('/vsimem/valid.xsd')

###############################################################################
# Test reading a particular construct with group, etc... that could cause
# crashes


def test_ogr_gmlas_read_fake_gmljp2():

    ds = gdal.OpenEx('GMLAS:data/gmlas/fake_gmljp2.xml')

    count = 0
    while True:
        f, lyr = ds.GetNextFeature()
        if f is None:
            assert lyr is None
            break
        count += 1

    assert count == 5

###############################################################################
#  Test TypingConstraints


def test_ogr_gmlas_typing_constraints():

    # One substitution, no repetition
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_typing_constraints_one_subst_no_repetition.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
<TypingConstraints>
        <Namespaces>
            <Namespace prefix="myns_modified_for_fun" uri="http://myns"/>
        </Namespaces>
        <ChildConstraint>
            <ContainerXPath>myns_modified_for_fun:main_elt/myns_modified_for_fun:foo</ContainerXPath>
            <ChildrenElements>
                <Element>myns_modified_for_fun:bar</Element>
            </ChildrenElements>
        </ChildConstraint>
    </TypingConstraints>
</Configuration>"""])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if not f.IsFieldSetAndNotNull('foo_bar_pkid'):
        f.DumpReadable()
        pytest.fail()
    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if f.GetField('value') != 'baz':
        f.DumpReadable()
        pytest.fail()
    ds = None

    # One substitution, with repetition

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_typing_constraints_one_subst_with_repetition.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
<TypingConstraints>
        <Namespaces>
            <Namespace prefix="myns_modified_for_fun" uri="http://myns"/>
        </Namespaces>
        <ChildConstraint>
            <ContainerXPath>myns_modified_for_fun:main_elt/myns_modified_for_fun:foo</ContainerXPath>
            <ChildrenElements>
                <Element>myns_modified_for_fun:bar</Element>
            </ChildrenElements>
        </ChildConstraint>
    </TypingConstraints>
</Configuration>"""])
    lyr = ds.GetLayer('main_elt_foo_bar')
    assert lyr.GetFeatureCount() == 2
    lyr = ds.GetLayer('bar')
    f = lyr.GetNextFeature()
    if f.GetField('value') != 'baz':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('value') != 'baz2':
        f.DumpReadable()
        pytest.fail()
    ds = None

    # 2 substitutions
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_typing_constraints_two_subst.xml',
                     open_options=["""CONFIG_FILE=<Configuration>
<TypingConstraints>
        <Namespaces>
            <Namespace prefix="myns_modified_for_fun" uri="http://myns"/>
        </Namespaces>
        <ChildConstraint>
            <ContainerXPath>myns_modified_for_fun:main_elt/myns_modified_for_fun:foo</ContainerXPath>
            <ChildrenElements>
                <Element>myns_modified_for_fun:bar</Element>
                <Element>myns_modified_for_fun:baz</Element>
            </ChildrenElements>
        </ChildConstraint>
    </TypingConstraints>
</Configuration>"""])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if not f.IsFieldSetAndNotNull('foo_bar_pkid'):
        f.DumpReadable()
        pytest.fail()
    if f.IsFieldSetAndNotNull('foo_baz_pkid'):
        f.DumpReadable()
        pytest.fail()
    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if f.GetField('value') != 'baz':
        f.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
#  Test swe:DataArray


def test_ogr_gmlas_swe_dataarray():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_swe_dataarray.xml')

    lyr = ds.GetLayerByName('dataarray_1_components')
    f = lyr.GetNextFeature()
    if not f.IsFieldSetAndNotNull('parent_ogr_pkid') or \
       f.GetField('myTime') != '2016/09/01 00:00:00+01' or \
       f.GetField('myCategory') != '1' or \
       f.GetField('myQuantity') != 2.34 or \
       f.GetField('myCount') != 3 or \
       f.GetField('myText') != 'foo' or \
       f.GetField('myBoolean') is False:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('myTime') != '2017/09/01 00:00:00' or \
       f.GetField('myCategory') != '2' or \
       f.GetField('myQuantity') != 3.45:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('dataarray_2')
    f = lyr.GetNextFeature()
    if f.GetField('myTime') != '2016/09/01 00:00:00+01' or \
       f.GetField('myCategory') != '1' or \
       f.GetField('myQuantity') != 2.34:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('myTime') != '2017/09/01 00:00:00' or \
       f.GetField('myCategory') is not None or \
       f.GetField('myQuantity') != 3.45:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('dataarray_3')
    f = lyr.GetNextFeature()
    if f.GetField('myTime') != '2016/09/01 00:00:00+01' or \
       f.GetField('myCategory') != '1' or \
       f.GetField('myQuantity') != 2.34:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('myTime') != '2017/09/01 00:00:00' or \
       f.GetField('myCategory') is not None or \
       f.GetField('myQuantity') != 3.45:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    ds = None


###############################################################################
#  Test swe:DataRecord

def test_ogr_gmlas_swe_datarecord():

    gdal.ErrorReset()
    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_swe_datarecord.xml', open_options=['VALIDATE=YES'])
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_swe_datarecord.xml')
    lyr = ds.GetLayerByName('main_elt_foo')
    assert lyr.GetLayerDefn().GetFieldCount() == 12
    f = lyr.GetNextFeature()
    if f.GetField('mytime_value') != '2017/09/01 00:00:00' or \
       f.GetField('mycategory_value') != 'myvalue' or \
       f.GetField('mycategory_identifier') != 'myidentifier' or \
       f.GetField('mycategory_codespace_href') != 'http://example.com' or \
       f.GetField('myquantity_value') != 1.23 or \
       f.GetField('mycount_value') != 2 or \
       f.GetField('mytext_value') != 'foo' or \
       f.GetField('myboolean_value') is False:
        f.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
#  Test a xs:any field at end of a type declaration


def test_ogr_gmlas_any_field_at_end_of_declaration():

    # Simplified test case for
    # http://schemas.earthresourceml.org/earthresourceml-lite/1.0/erml-lite.xsd
    # http://services.ga.gov.au/earthresource/ows?service=wfs&version=2.0.0&request=GetFeature&typenames=erl:CommodityResourceView&count=10

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_any_field_at_end_of_declaration.xml')
    lyr = ds.GetLayerByName('main_elt')
    # Will warn about 'Unexpected element with xpath=main_elt/extra (subxpath=main_elt/extra) found'
    # This should be fixed at some point
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() != ''
    if f.GetField('foo') != 'bar':
        f.DumpReadable()
        pytest.fail()
    if f.GetField('value') != '<something>baz</something>':
        print('Expected fail: value != <something>baz</something>')


###############################################################################
#  Test auxiliary schema without namespace prefix


def test_ogr_gmlas_aux_schema_without_namespace_prefix():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_aux_schema_without_namespace_prefix.xml')
    lyr = ds.GetLayerByName('main_elt')
    f = lyr.GetNextFeature()
    if not f.IsFieldSetAndNotNull('generic_pkid'):
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test importing a GML geometry that is in an element that is a substitutionGroup
# of another one (#6990)


def test_ogr_gmlas_geometry_as_substitutiongroup():

    ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_geometry_as_substitutiongroup.xml')
    lyr = ds.GetLayerByName('foo')
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is None:
        f.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################


@pytest.mark.require_run_on_demand
def test_ogr_gmlas_extra_piezometre():

    return compare_ogrinfo_output('data/gmlas/real_world/Piezometre.06512X0037.STREMY.2.gml',
                                  'data/gmlas/real_world/output/Piezometre.06512X0037.STREMY.2.txt',
                                  options='-oo REMOVE_UNUSED_LAYERS=YES')

###############################################################################


@pytest.mark.require_run_on_demand
def test_ogr_gmlas_extra_eureg():

    return compare_ogrinfo_output('data/gmlas/real_world/EUReg.example.gml',
                                  'data/gmlas/real_world/output/EUReg.example.txt',
                                  options='-oo REMOVE_UNUSED_LAYERS=YES')


###############################################################################
# Test a schema that has nothing interesting in it but imports another
# schema

def test_ogr_gmlas_no_element_in_first_choice_schema():

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=data/gmlas/gmlas_no_element_in_first_choice_schema.xsd'])
    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    f = lyr.GetNextFeature()
    if f['layer_xpath'] != 'my_ns:main_elt':
        f.DumpReadable()
        pytest.fail()



###############################################################################
# Test cross-layer links with xlink:href="#my_id"

def test_ogr_gmlas_internal_xlink_href():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_internal_xlink_href.xml')
        lyr = ds.GetLayerByName('main_elt')
        f = lyr.GetNextFeature()
    if f['link_to_second_or_third_elt_href'] != '#does_not_exist' or \
       f.IsFieldSet('link_to_second_or_third_elt_second_elt_pkid') or \
       f.IsFieldSet('link_to_second_or_third_elt_third_elt_pkid') or \
       f.IsFieldSet('link_to_third_elt_third_elt_pkid'):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['link_to_second_or_third_elt_href'] != '#id2' or \
       f['link_to_second_or_third_elt_second_elt_pkid'] != 'id2' or \
       f.IsFieldSet('link_to_second_or_third_elt_third_elt_pkid') or \
       f.IsFieldSet('link_to_third_elt_third_elt_pkid'):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['link_to_second_or_third_elt_href'] != '#id3' or \
       f['link_to_second_or_third_elt_second_elt_pkid'] != 'id3' or \
       f.IsFieldSet('link_to_second_or_third_elt_third_elt_pkid') or \
       f.IsFieldSet('link_to_third_elt_third_elt_pkid'):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['link_to_second_or_third_elt_href'] != '#id4' or \
       f.IsFieldSet('link_to_second_or_third_elt_second_elt_pkid') or \
       f['link_to_second_or_third_elt_third_elt_pkid'] != 'D1013B7E44F28C976B976A4314FA4A09_third_elt_1' or \
       f.IsFieldSet('link_to_third_elt_third_elt_pkid'):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['link_to_third_elt_href'] != '#id4' or \
       f.IsFieldSet('link_to_second_or_third_elt_second_elt_pkid') or \
       f.IsFieldSet('link_to_second_or_third_elt_third_elt_pkid') or \
       f['link_to_third_elt_third_elt_pkid'] != 'D1013B7E44F28C976B976A4314FA4A09_third_elt_1':
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('_ogr_fields_metadata')
    f = lyr.GetNextFeature()
    if f['layer_name'] != 'main_elt' or f['field_index'] != 1 or \
       f['field_name'] != 'link_to_second_or_third_elt_href':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['layer_name'] != 'main_elt' or f['field_index'] != 2 or \
       f['field_name'] != 'link_to_second_or_third_elt_second_elt_pkid' or \
       f['field_xpath'] != 'main_elt/link_to_second_or_third_elt/second_elt' or \
       f['field_type'] != 'string' or \
       f['field_is_list'] != 0 or \
       f['field_min_occurs'] != 0 or \
       f['field_max_occurs'] != 1 or \
       f['field_category'] != 'PATH_TO_CHILD_ELEMENT_WITH_LINK' or \
       f['field_related_layer'] != 'second_elt':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['layer_name'] != 'main_elt' or f['field_index'] != 3 or \
       f['field_name'] != 'link_to_second_or_third_elt_third_elt_pkid' or \
       f['field_xpath'] != 'main_elt/link_to_second_or_third_elt/third_elt' or \
       f['field_type'] != 'string' or \
       f['field_is_list'] != 0 or \
       f['field_min_occurs'] != 0 or \
       f['field_max_occurs'] != 1 or \
       f['field_category'] != 'PATH_TO_CHILD_ELEMENT_WITH_LINK' or \
       f['field_related_layer'] != 'third_elt':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['layer_name'] != 'main_elt' or f['field_index'] != 4 or \
       f['field_name'] != 'link_to_second_or_third_elt':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['layer_name'] != 'main_elt' or f['field_index'] != 5 or \
       f['field_name'] != 'link_to_third_elt_href':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['layer_name'] != 'main_elt' or f['field_index'] != 6 or \
       f['field_name'] != 'link_to_third_elt_third_elt_pkid':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['layer_name'] != 'third_elt' or f['field_index'] != 1 or \
       f['field_name'] != 'id':
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('_ogr_layer_relationships')
    f = lyr.GetNextFeature()
    if f['parent_layer'] != 'main_elt' or \
       f['parent_pkid'] != 'ogr_pkid' or \
       f['parent_element_name'] != 'link_to_third_elt_third_elt_pkid' or \
       f['child_layer'] != 'third_elt' or \
       f['child_pkid'] != 'ogr_pkid':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['parent_layer'] != 'main_elt' or \
       f['parent_pkid'] != 'ogr_pkid' or \
       f['parent_element_name'] != 'link_to_second_or_third_elt_second_elt_pkid' or \
       f['child_layer'] != 'second_elt' or \
       f['child_pkid'] != 'id':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['parent_layer'] != 'main_elt' or \
       f['parent_pkid'] != 'ogr_pkid' or \
       f['parent_element_name'] != 'link_to_second_or_third_elt_third_elt_pkid' or \
       f['child_layer'] != 'third_elt' or \
       f['child_pkid'] != 'ogr_pkid':
        f.DumpReadable()
        pytest.fail()

###############################################################################
# Test opening a file whose .xsd has a bad version attribute in the <?xml processing instruction


def test_ogr_gmlas_invalid_version_xsd():

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas/gmlas_invalid_version_xsd.xml')
    assert ds is None
