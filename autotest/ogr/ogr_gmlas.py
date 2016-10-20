#!/usr/bin/env python
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
#******************************************************************************
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import webserver

from osgeo import gdal
from osgeo import ogr

###############################################################################
# Basic test

def ogr_gmlas_basic():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', 'YES')

    # FileGDB embedded libxml2 cause random crashes with CPLValidateXML() use of external libxml2
    ogrtest.old_val_GDAL_XML_VALIDATION = gdal.GetConfigOption('GDAL_XML_VALIDATION')
    if ogr.GetDriverByName('FileGDB') is not None and ogrtest.old_val_GDAL_XML_VALIDATION is None:
        gdal.SetConfigOption('GDAL_XML_VALIDATION', 'NO')

    ds = ogr.Open('GMLAS:data/gmlas_test1.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # Skip tests when -fsanitize is used
    if 'TRAVIS_BRANCH' in os.environ and \
       os.environ['TRAVIS_BRANCH'].find('sanitize') >= 0:
       print('Skipping because of -sanitize')
       return 'skip'

    import test_cli_utilities

    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al GMLAS:data/gmlas_test1.xml -oo EXPOSE_METADATA_LAYERS=YES')
    ret = ret.replace('\r\n', '\n')
    expected = open('data/gmlas_test1.txt', 'rb').read().decode('utf-8')
    expected = expected.replace('\r\n', '\n')
    if ret != expected:
        gdaltest.post_reason('fail')
        print('Got:')
        print(ret)
        open('tmp/ogr_gmlas_1.txt', 'wb').write(ret.encode('utf-8'))
        print('Diff:')
        os.system('diff -u data/gmlas_test1.txt tmp/ogr_gmlas_1.txt')
        #os.unlink('tmp/ogr_gmlas_1.txt')
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_gmlas_test_ogrsf():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    # Skip tests when -fsanitize is used
    if 'TRAVIS_BRANCH' in os.environ and \
       os.environ['TRAVIS_BRANCH'].find('sanitize') >= 0:
       print('Skipping because of -sanitize')
       return 'skip'

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro GMLAS:data/gmlas_test1.xml')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test virtual file support

def ogr_gmlas_virtual_file():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_8.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_8.xsd')

    return 'success'

###############################################################################
# Test opening with XSD option

def ogr_gmlas_datafile_with_xsd_option():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = ['XSD=data/gmlas_test1.xsd'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test opening with just XSD option

def ogr_gmlas_no_datafile_with_xsd_option():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=data/gmlas_test1.xsd'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test opening with just XSD option but pointing to a non-xsd filename

def ogr_gmlas_no_datafile_xsd_which_is_not_xsd():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=data/gmlas_test1.xml'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find("invalid content in 'schema' element") < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test opening with nothing

def ogr_gmlas_no_datafile_no_xsd():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('XSD open option must be provided when no XML data file is passed') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test opening an inexisting GML file

def ogr_gmlas_non_existing_gml():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/i_dont_exist.gml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Cannot open /vsimem/i_dont_exist.gml') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test opening with just XSD option but pointing to a non existing file

def ogr_gmlas_non_existing_xsd():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=/vsimem/i_dont_exist.xsd'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Cannot resolve /vsimem/i_dont_exist.xsd') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test opening a GML file without schemaLocation

def ogr_gmlas_gml_without_schema_location():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_gml_without_schema_location.xml',
"""<MYNS:main_elt xmlns:MYNS="http://myns"/>""")

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_gml_without_schema_location.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('No schema locations found when analyzing data file: XSD open option must be provided') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_gml_without_schema_location.xml')

    return 'success'

###############################################################################
# Test invalid schema

def ogr_gmlas_invalid_schema():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_invalid_schema.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_invalid_schema.xsd"/>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_invalid_schema.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:foo/>
</xs:schema>""")

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_invalid_schema.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('invalid content') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_invalid_schema.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_invalid_schema.xsd')

    return 'success'

###############################################################################
# Test invalid XML

def ogr_gmlas_invalid_xml():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_invalid_xml.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_invalid_xml.xsd">
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_invalid_xml.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_invalid_xml.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('input ended before all started tags were ended') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_invalid_xml.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_invalid_xml.xsd')

    return 'success'

###############################################################################
# Test links with gml:ReferenceType

def ogr_gmlas_gml_Reference():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    ds = ogr.Open('GMLAS:data/gmlas_test_targetelement.xml')
    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'

    lyr = ds.GetLayerByName('main_elt')
    f = lyr.GetNextFeature()
    if f['reference_existing_target_elt_href'] != '#BAZ' or \
       f['reference_existing_target_elt_pkid'] != 'BAZ' or \
       f['reference_existing_abstract_target_elt_href'] != '#BAW':
           gdaltest.post_reason('fail')
           f.DumpReadable()
           return 'fail'

    return 'success'

###############################################################################
# Test that we fix ambiguities in class names

def ogr_gmlas_same_element_in_different_ns():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_same_element_in_different_ns.xml',
"""<myns:elt xmlns:myns="http://myns"
             xmlns:other_ns="http://other_ns" 
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_same_element_in_different_ns.xsd">
    <other_ns:realizationOfAbstractElt>
        <other_ns:foo>bar</other_ns:foo>
    </other_ns:realizationOfAbstractElt>
</myns:elt>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_same_element_in_different_ns.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns"
           xmlns:other_ns="http://other_ns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:import namespace="http://other_ns" schemaLocation="ogr_gmlas_same_element_in_different_ns_other_ns.xsd"/>
<xs:element name="elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element ref="other_ns:abstractElt"/>
        <xs:element name="elt2">
            <xs:complexType>
                <xs:sequence>
                    <xs:element ref="other_ns:abstractElt" maxOccurs="unbounded"/>
                </xs:sequence>
            </xs:complexType>
        </xs:element>
    </xs:sequence>
  </xs:complexType>
</xs:element>
<xs:element name="realizationOfAbstractElt" substitutionGroup="other_ns:abstractElt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="bar" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_same_element_in_different_ns_other_ns.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:other_ns="http://other_ns" 
           targetNamespace="http://other_ns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="abstractElt" abstract="true"/>
<xs:element name="realizationOfAbstractElt" substitutionGroup="other_ns:abstractElt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_same_element_in_different_ns.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    #for i in range(ds.GetLayerCount()):
    #    print(ds.GetLayer(i).GetName())

    if ds.GetLayerCount() != 5:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'
    lyr = ds.GetLayerByName('elt')
    f = lyr.GetNextFeature()
    if f.IsFieldSet('abstractElt_other_ns_realizationOfAbstractElt_pkid') == 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if ds.GetLayerByName('myns_realizationOfAbstractElt') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerByName('other_ns_realizationOfAbstractElt') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerByName('elt_elt2_abstractElt_myns_realizationOfAbstractElt') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerByName('elt_elt2_abstractElt_other_ns_realizationOfAbstractElt') is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_same_element_in_different_ns.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_same_element_in_different_ns.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_same_element_in_different_ns_other_ns.xsd')

    return 'success'

###############################################################################
# Test a corner case of relative path resolution

def ogr_gmlas_corner_case_relative_path():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    ds = ogr.Open('GMLAS:../ogr/data/gmlas_test1.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test unexpected repeated element

def ogr_gmlas_unexpected_repeated_element():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_unexpected_repeated_element.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_unexpected_repeated_element.xsd">
    <myns:foo>foo_first</myns:foo>
    <myns:foo>foo_again</myns:foo>
</myns:main_elt>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_unexpected_repeated_element.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_unexpected_repeated_element.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is None or f['foo'] != 'foo_again': # somewhat arbitrary to keep the latest one!
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if gdal.GetLastErrorMsg().find('Unexpected element myns:main_elt/myns:foo') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gmlas_unexpected_repeated_element.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_unexpected_repeated_element.xsd')

    return 'success'

###############################################################################
# Test unexpected repeated element

def ogr_gmlas_unexpected_repeated_element_variant():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_unexpected_repeated_element.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_unexpected_repeated_element.xsd">
    <myns:foo>foo_first</myns:foo>
    <myns:bar>bar</myns:bar>
    <myns:foo>foo_again</myns:foo>
</myns:main_elt>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_unexpected_repeated_element.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string" minOccurs="0"/>
        <xs:element name="bar" type="xs:string" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_unexpected_repeated_element.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is None or f['foo'] != 'foo_again': # somewhat arbitrary to keep the latest one!
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if gdal.GetLastErrorMsg().find('Unexpected element myns:main_elt/myns:foo') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gmlas_unexpected_repeated_element.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_unexpected_repeated_element.xsd')

    return 'success'

###############################################################################
# Test reading geometries embedded in a geometry property element

def ogr_gmlas_geometryproperty():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    ds = gdal.OpenEx('GMLAS:data/gmlas_geometryproperty_gml32.gml', open_options = [
            'CONFIG_FILE=<Configuration><LayerBuildingRules><GML><IncludeGeometryXML>true</IncludeGeometryXML></GML></LayerBuildingRules></Configuration>'])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        geom_field_count = lyr.GetLayerDefn().GetGeomFieldCount()
    if geom_field_count != 14:
        gdaltest.post_reason('fail')
        print(geom_field_count)
        return 'fail'
    f = lyr.GetNextFeature()
    if f['geometryProperty_xml'] != ' <gml:Point gml:id="poly.geom.Geometry" srsName="urn:ogc:def:crs:EPSG::4326"> <gml:pos>49 2</gml:pos> </gml:Point> ':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if f.IsFieldSet('geometryPropertyEmpty_xml'):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if f['pointProperty_xml'] != '<gml:Point gml:id="poly.geom.Point" srsName="http://www.opengis.net/def/crs/EPSG/0/4326"><gml:pos>50 3</gml:pos></gml:Point>':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if f['pointPropertyRepeated_xml'] != [
            '<gml:Point gml:id="poly.geom.pointPropertyRepeated.1"><gml:pos>0 1</gml:pos></gml:Point>',
            '<gml:Point gml:id="poly.geom.pointPropertyRepeated.2"><gml:pos>1 2</gml:pos></gml:Point>',
            '<gml:Point gml:id="poly.geom.pointPropertyRepeated.3"><gml:pos>3 4</gml:pos></gml:Point>']:
        gdaltest.post_reason('fail')
        print(f['pointPropertyRepeated_xml'])
        f.DumpReadable()
        return 'fail'
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    sr = lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetSpatialRef()
    if sr is None or sr.ExportToWkt().find('4326') < 0 or sr.ExportToWkt().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(sr)
        return 'fail'
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryPropertyEmpty')
    if f.GetGeomFieldRef(geom_idx) is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('pointProperty')
    sr = lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetSpatialRef()
    if sr is None or sr.ExportToWkt().find('4326') < 0 or sr.ExportToWkt().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(sr)
        return 'fail'
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('lineStringProperty')
    sr = lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetSpatialRef()
    if sr is None or sr.ExportToWkt().find('4326') < 0 or sr.ExportToWkt().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(sr)
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetType())
        return 'fail'
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'LINESTRING (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('pointPropertyRepeated')
    if lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetGeomFieldDefn(geom_idx).GetType())
        return 'fail'
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'GEOMETRYCOLLECTION (POINT (0 1),POINT (1 2),POINT (3 4))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test that on-the-fly reprojection works
    f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    geom = f.GetGeomFieldRef(geom_idx)
    if ogrtest.check_feature_geometry(geom, 'POINT (3.0 0.0)') != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Failed reprojection
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    if f.GetGeomFieldRef(geom_idx) is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test SWAP_COORDINATES=NO
    ds = gdal.OpenEx('GMLAS:data/gmlas_geometryproperty_gml32.gml',
                     open_options= ['SWAP_COORDINATES=NO'])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'POINT (49 2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('lineStringProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'LINESTRING (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test SWAP_COORDINATES=YES
    ds = gdal.OpenEx('GMLAS:data/gmlas_geometryproperty_gml32.gml',
                     open_options= ['SWAP_COORDINATES=YES'])
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('lineStringProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    # Axis swapping
    if wkt != 'LINESTRING (49 2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test reading geometries referenced by a AbstractGeometry element

def ogr_gmlas_abstractgeometry():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    ds = gdal.OpenEx('GMLAS:data/gmlas_abstractgeometry_gml32.gml', open_options = [
            'CONFIG_FILE=<Configuration><LayerBuildingRules><GML><IncludeGeometryXML>true</IncludeGeometryXML></GML></LayerBuildingRules></Configuration>'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetGeomFieldCount() != 2:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetGeomFieldCount())
        return 'fail'
    f = lyr.GetNextFeature()
    if f['AbstractGeometry_xml'] != '<gml:Point gml:id="test.geom.0"><gml:pos>0 1</gml:pos></gml:Point>':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if f['repeated_AbstractGeometry_xml'] != [
            '<gml:Point gml:id="test.geom.repeated.1"><gml:pos>0 1</gml:pos>',
            '<gml:Point gml:id="test.geom.repeated.2"><gml:pos>1 2</gml:pos>']:
        gdaltest.post_reason('fail')
        print(f['repeated_AbstractGeometry_xml'])
        f.DumpReadable()
        return 'fail'
    wkt = f.GetGeomFieldRef(0).ExportToWkt()
    if wkt != 'POINT (0 1)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    wkt = f.GetGeomFieldRef(1).ExportToWkt()
    if wkt != 'GEOMETRYCOLLECTION (POINT (0 1),POINT (1 2))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test validation against schema

class MyHandler:
    def __init__(self):
        self.error_list = []

    def error_handler(self, err_type, err_no, err_msg):
        if err_type != 1: # 1 == Debug
            self.error_list.append((err_type, err_no, err_msg))

def ogr_gmlas_validate():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_validate.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_validate.xsd">
    <myns:bar baz="bow">bar</myns:bar>
</myns:main_elt>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_validate.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    # By default check we are silent about validation error
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_validate.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', None)
    lyr = ds.GetLayer(0)
    lyr.GetFeatureCount()
    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', 'YES')
    gdal.PopErrorHandler()
    if len(myhandler.error_list) != 0:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_validate.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    lyr = ds.GetLayer(0)
    lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    # Unexpected element with xpath=myns:main_elt/myns:bar (subxpath=myns:main_elt/myns:bar) found
    if len(myhandler.error_list) != 2:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'

    # Enable validation on a doc without validation errors
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = ['VALIDATE=YES'])
    gdal.PopErrorHandler()
    if ds is None:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'
    if len(myhandler.error_list) != 0:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'

    # Enable validation on a doc without validation error, and with explicit XSD
    gdal.FileFromMemBuffer('/vsimem/gmlas_test1.xml',
                           open('data/gmlas_test1.xml').read() )
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:/vsimem/gmlas_test1.xml', open_options = [
                'XSD=' + os.getcwd() + '/data/gmlas_test1.xsd', 'VALIDATE=YES'])
    gdal.PopErrorHandler()
    gdal.Unlink('/vsimem/gmlas_test1.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'
    if len(myhandler.error_list) != 0:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'

    # Validation errors, but do not prevent dataset opening
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_validate.xml', open_options = ['VALIDATE=YES'])
    gdal.PopErrorHandler()
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if len(myhandler.error_list) != 5:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        print(len(myhandler.error_list))
        return 'fail'

    # Validation errors and do prevent dataset opening
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_validate.xml', open_options = ['VALIDATE=YES', 'FAIL_IF_VALIDATION_ERROR=YES'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if len(myhandler.error_list) != 6:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        print(len(myhandler.error_list))
        return 'fail'

    # Test that validation without doc doesn't crash
    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=data/gmlas_test1.xsd', 'VALIDATE=YES'])
    gdal.PopErrorHandler()
    if ds is None:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'
    if len(myhandler.error_list) != 0:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_validate.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_validate.xsd')

    return 'success'

###############################################################################
# Test correct namespace prefix handling

def ogr_gmlas_test_ns_prefix():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    # The schema doesn't directly import xlink, but indirectly references it
    ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=data/gmlas_test_targetelement.xsd'])

    lyr = ds.GetLayerByName('_ogr_fields_metadata')
    f = lyr.GetNextFeature()
    if f['field_xpath'] != 'myns:main_elt/myns:reference_missing_target_elt/@xlink:href':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test parsing documents without namespace

def ogr_gmlas_no_namespace():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_no_namespace.xml',
"""<main_elt xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:noNamespaceSchemaLocation="ogr_gmlas_no_namespace.xsd">
    <foo>bar</foo>
</main_elt>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_no_namespace.xsd',
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

    ds = ogr.Open('GMLAS:/vsimem/ogr_gmlas_no_namespace.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_no_namespace.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_no_namespace.xsd')

    return 'success'

###############################################################################
# Test CONFIG_FILE

def ogr_gmlas_conf():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    # Non existing file
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = ['CONFIG_FILE=not_existing'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Broken conf file
    gdal.FileFromMemBuffer('/vsimem/my_conf.xml', "<broken>")
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = ['CONFIG_FILE=/vsimem/my_conf.xml'])
    gdal.Unlink('/vsimem/my_conf.xml')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Valid XML, but not validating
    gdal.FileFromMemBuffer('/vsimem/my_conf.xml', "<not_validating/>")
    with gdaltest.error_handler():
        gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = ['CONFIG_FILE=/vsimem/my_conf.xml'])
    gdal.Unlink('/vsimem/my_conf.xml')

    # Inlined conf file + UseArrays = false
    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = [
            'CONFIG_FILE=<Configuration><LayerBuildingRules><UseArrays>false</UseArrays></LayerBuildingRules></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('main_elt_string_array')
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    # AlwaysGenerateOGRId = true
    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = [
            'CONFIG_FILE=<Configuration><LayerBuildingRules><AlwaysGenerateOGRId>true</AlwaysGenerateOGRId></LayerBuildingRules></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('main_elt')
    f = lyr.GetNextFeature()
    if f['ogr_pkid'].find('main_elt_1') < 0 or \
       f['otherns_id'] != 'otherns_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # IncludeGeometryXML = false
    ds = gdal.OpenEx('GMLAS:data/gmlas_geometryproperty_gml32.gml', open_options = [
            'CONFIG_FILE=<Configuration><LayerBuildingRules><GML><IncludeGeometryXML>false</IncludeGeometryXML></GML></LayerBuildingRules></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        if lyr.GetLayerDefn().GetFieldIndex('geometryProperty_xml') >= 0:
            gdaltest.post_reason('fail')
            return 'fail'
    f = lyr.GetNextFeature()
    geom_idx = lyr.GetLayerDefn().GetGeomFieldIndex('geometryProperty')
    wkt = f.GetGeomFieldRef(geom_idx).ExportToWkt()
    if wkt != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # ExposeMetadataLayers = true
    ds = gdal.OpenEx('GMLAS:data/gmlas_abstractgeometry_gml32.gml', open_options = [
            'CONFIG_FILE=<Configuration><ExposeMetadataLayers>true</ExposeMetadataLayers></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 4:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'
    # Test override with open option
    ds = gdal.OpenEx('GMLAS:data/gmlas_abstractgeometry_gml32.gml', open_options = [
            'EXPOSE_METADATA_LAYERS=NO',
            'CONFIG_FILE=<Configuration><ExposeMetadataLayers>true</ExposeMetadataLayers></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_conf_invalid.xml',
"""<main_elt xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:noNamespaceSchemaLocation="ogr_gmlas_conf_invalid.xsd">
    <foo>bar</foo>
</main_elt>
""" )

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_conf_invalid.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string"/>
        <xs:element name="missing" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    # Turn on validation and error on validation
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_conf_invalid.xml', open_options = [
            'CONFIG_FILE=<Configuration><Validation enabled="true"><FailIfError>true</FailIfError></Validation></Configuration>'])
    if ds is not None or gdal.GetLastErrorMsg().find('Validation') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Cleanup
    gdal.Unlink('/vsimem/ogr_gmlas_conf_invalid.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_conf_invalid.xsd')

    return 'success'

###############################################################################
# Test IgnoredXPaths aspect of config file

def ogr_gmlas_conf_ignored_xpath():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    # Test unsupported and invalid XPaths
    for xpath in [ '',
                   '1',
                   '@',
                   '@/',
                   '.',
                   ':',
                   '/:',
                   'a:',
                   'a:1',
                   'foo[1]',
                   "foo[@bar='baz']" ]:
        with gdaltest.error_handler():
            gdal.OpenEx('GMLAS:', open_options = [
                    'XSD=data/gmlas_test1.xsd',
                    """CONFIG_FILE=<Configuration>
                        <IgnoredXPaths>
                            <WarnIfIgnoredXPathFoundInDocInstance>true</WarnIfIgnoredXPathFoundInDocInstance>
                            <XPath>%s</XPath>
                        </IgnoredXPaths>
                    </Configuration>""" % xpath])
        if gdal.GetLastErrorMsg().find('XPath syntax') < 0:
            gdaltest.post_reason('fail')
            print(xpath)
            print(gdal.GetLastErrorMsg())
            return 'fail'

    # Test duplicating mapping
    with gdaltest.error_handler():
        gdal.OpenEx('GMLAS:', open_options = [
                    'XSD=data/gmlas_test1.xsd',
                """CONFIG_FILE=<Configuration>
                    <IgnoredXPaths>
                        <Namespaces>
                            <Namespace prefix="ns" uri="http://ns1"/>
                            <Namespace prefix="ns" uri="http://ns2"/>
                        </Namespaces>
                    </IgnoredXPaths>
                </Configuration>"""])
    if gdal.GetLastErrorMsg().find('Prefix ns was already mapped') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Test XPath with implicit namespace, and warning
    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = [
            """CONFIG_FILE=<Configuration>
                <IgnoredXPaths>
                    <WarnIfIgnoredXPathFoundInDocInstance>true</WarnIfIgnoredXPathFoundInDocInstance>
                    <XPath>@otherns:id</XPath>
                </IgnoredXPaths>
            </Configuration>"""])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('main_elt')
    if lyr.GetLayerDefn().GetFieldIndex('otherns_id') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        lyr.GetNextFeature()
    if gdal.GetLastErrorMsg().find('Attribute with xpath=myns:main_elt/@otherns:id found in document but ignored') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Test XPath with explicit namespace, and warning suppression
    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = [
            """CONFIG_FILE=<Configuration>
                <IgnoredXPaths>
                    <Namespaces>
                        <Namespace prefix="other_ns" uri="http://other_ns"/>
                    </Namespaces>
                    <XPath warnIfIgnoredXPathFoundInDocInstance="false">@other_ns:id</XPath>
                </IgnoredXPaths>
            </Configuration>"""])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('main_elt')
    lyr.GetNextFeature()
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Test various XPath syntaxes
    ds = gdal.OpenEx('GMLAS:', open_options = [
                    'XSD=data/gmlas_test1.xsd',
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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('main_elt')

    # Ignored fields
    if lyr.GetLayerDefn().GetFieldIndex('optionalStrAttr') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldIndex('fixedValUnset') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldIndex('base_int') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldIndex('string') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldIndex('string_array') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Present fields
    if lyr.GetLayerDefn().GetFieldIndex('int_array') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldIndex('long') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test schema caching

def ogr_gmlas_cache():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    (webserver_process, webserver_port) = webserver.launch(fork_process = False)
    if webserver_port == 0:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_cache.xml',
"""<main_elt xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:noNamespaceSchemaLocation="http://localhost:%d/vsimem/ogr_gmlas_cache.xsd">
    <foo>bar</foo>
</main_elt>
""" % webserver_port )

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
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'CONFIG_FILE=<Configuration><AllowRemoteSchemaDownload>false</AllowRemoteSchemaDownload><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is not None or gdal.GetLastErrorMsg().find('Cannot resolve') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Test invalid cache directory
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/inexisting_directory/not/exist</Directory></SchemaCache></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    # Will create the directory and download and cache
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    gdal.Unlink('/vsimem/my/gmlas_cache/' + gdal.ReadDir('/vsimem/my/gmlas_cache')[0])

    # Will reuse the directory and download and cache
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    # With XSD open option
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'XSD=http://localhost:%d/vsimem/ogr_gmlas_cache.xsd' % webserver_port,
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    webserver.server_stop(webserver_process, webserver_port)

    # Now re-open with the webserver turned off
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Re try but ask for refresh
    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'REFRESH_CACHE=YES',
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is not None or gdal.GetLastErrorMsg().find('Cannot resolve') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    # Re try with non existing cached schema
    gdal.Unlink('/vsimem/my/gmlas_cache/' + gdal.ReadDir('/vsimem/my/gmlas_cache')[0])

    with gdaltest.error_handler():
        ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_cache.xml', open_options = [
            'CONFIG_FILE=<Configuration><SchemaCache><Directory>/vsimem/my/gmlas_cache</Directory></SchemaCache></Configuration>'])
    if ds is not None or gdal.GetLastErrorMsg().find('Cannot resolve') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Cleanup
    gdal.Unlink('/vsimem/ogr_gmlas_cache.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_cache.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_cache_2.xsd')

    files = gdal.ReadDir('/vsimem/my/gmlas_cache')
    for my_file in files:
        gdal.Unlink('/vsimem/my/gmlas_cache/' + my_file)
    gdal.Rmdir('/vsimem/my/gmlas_cache')
    gdal.Rmdir('/vsimem/my')

    return 'success'


###############################################################################
# Test good working of linking to a child through its id attribute

def ogr_gmlas_link_nested_independant_child():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_link_nested_independant_child.xml',
"""<first xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:noNamespaceSchemaLocation="ogr_gmlas_link_nested_independant_child.xsd">
    <second my_id="second_id"/>
</first>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_link_nested_independant_child.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="first">
  <xs:complexType>
    <xs:sequence>
        <xs:element ref="second"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>

<xs:element name="second">
  <xs:complexType>
    <xs:sequence/>
    <xs:attribute name="my_id" type="xs:ID" use="required"/>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    ds = ogr.Open('GMLAS:/vsimem/ogr_gmlas_link_nested_independant_child.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['second_my_id'] != 'second_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_link_nested_independant_child.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_link_nested_independant_child.xsd')

    return 'success'

###############################################################################
# Test some pattern found in geosciml schemas

def ogr_gmlas_composition_compositionPart():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_composition_compositionPart.xml',
"""<first xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:noNamespaceSchemaLocation="ogr_gmlas_composition_compositionPart.xsd">
    <composition>
        <CompositionPart my_id="id1">
            <a>a1</a>
        </CompositionPart>
    </composition>
    <composition>
        <CompositionPart my_id="id2">
            <a>a2</a>
        </CompositionPart>
    </composition>
</first>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_composition_compositionPart.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           elementFormDefault="qualified" attributeFormDefault="unqualified">

<xs:element name="first">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="composition" maxOccurs="unbounded" minOccurs="0" type="CompositionPartPropertyType"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>

<xs:complexType name="CompositionPartPropertyType">
    <xs:sequence>
        <xs:element ref="CompositionPart"/>
    </xs:sequence>
</xs:complexType>

<xs:element name="CompositionPart">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="a" type="xs:string"/>
    </xs:sequence>
    <xs:attribute name="my_id" type="xs:ID" use="required"/>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    ds = ogr.Open('GMLAS:/vsimem/ogr_gmlas_composition_compositionPart.xml')

    lyr = ds.GetLayerByName('first_composition')
    f = lyr.GetNextFeature()
    if f.IsFieldSet('parent_ogr_pkid') == 0 or f.IsFieldSet('CompositionPart_pkid') == 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.IsFieldSet('parent_ogr_pkid') == 0 or f.IsFieldSet('CompositionPart_pkid') == 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr = ds.GetLayerByName('CompositionPart')
    f = lyr.GetNextFeature()
    if f.IsFieldSet('my_id') == 0 or f.IsFieldSet('a') == 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.IsFieldSet('my_id') == 0 or f.IsFieldSet('a') == 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'


    gdal.Unlink('/vsimem/ogr_gmlas_composition_compositionPart.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_composition_compositionPart.xsd')

    return 'success'

###############################################################################
# Test that when importing GML we expose by default only elements deriving
# from _Feature/AbstractFeature

def ogr_gmlas_instantiate_only_gml_feature():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/subdir/gmlas_fake_gml32.xsd',
                           open('data/gmlas_fake_gml32.xsd', 'rb').read())

    gdal.FileFromMemBuffer('/vsimem/subdir/ogr_gmlas_instantiate_only_gml_feature.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              xmlns:gml="http://fake_gml32"
              elementFormDefault="qualified" attributeFormDefault="unqualified">

<xs:import namespace="http://fake_gml32" schemaLocation="../subdir/gmlas_fake_gml32.xsd"/>

<!--
    Xerces correctly detects circular dependencies

  <xs:complexType name="typeA">
    <xs:complexContent>
      <xs:extension base="typeB">
        <xs:sequence/>
      </xs:extension> 
    </xs:complexContent>
  </xs:complexType>

  <xs:complexType name="typeB">
    <xs:complexContent>
      <xs:extension base="typeA">
        <xs:sequence/>
      </xs:extension> 
    </xs:complexContent>
  </xs:complexType>

<xs:element name="A" substitutionGroup="B" type="typeA"/>
<xs:element name="B" substitutionGroup="A" type="typeB"/>
-->

<xs:element name="nonFeature">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="a" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>

<xs:element name="someFeature" substitutionGroup="gml:AbstractFeature">
  <xs:complexType>
    <xs:complexContent>
      <xs:extension base="gml:AbstractFeatureType">
        <xs:sequence>
            <xs:element ref="nonFeature"/>
        </xs:sequence>
      </xs:extension> 
    </xs:complexContent>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:',
        open_options = ['XSD=/vsimem/subdir/ogr_gmlas_instantiate_only_gml_feature.xsd'])
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/subdir/ogr_gmlas_instantiate_only_gml_feature.xsd')
    gdal.Unlink('/vsimem/subdir/gmlas_fake_gml32.xsd')

    return 'success'

###############################################################################
# Test that WFS style timeStamp are ignored for hash generation

def ogr_gmlas_timestamp_ignored_for_hash():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_timestamp_ignored_for_hash.xml',
"""<first xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          timeStamp="foo"
        xsi:noNamespaceSchemaLocation="ogr_gmlas_timestamp_ignored_for_hash.xsd">
</first>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_timestamp_ignored_for_hash.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="first">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string" minOccurs="0"/>
    </xs:sequence>
    <xs:attribute name="timeStamp" type="xs:string"/>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = ogr.Open('GMLAS:/vsimem/ogr_gmlas_timestamp_ignored_for_hash.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    pkid = f['ogr_pkid']

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_timestamp_ignored_for_hash.xml',
"""<first xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          timeStamp="bar"
        xsi:noNamespaceSchemaLocation="ogr_gmlas_timestamp_ignored_for_hash.xsd">
</first>
""")

    ds = ogr.Open('GMLAS:/vsimem/ogr_gmlas_timestamp_ignored_for_hash.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['ogr_pkid'] != pkid:
        gdaltest.post_reason('fail')
        print(pkid)
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_timestamp_ignored_for_hash.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_timestamp_ignored_for_hash.xsd')

    return 'success'

###############################################################################
# Test dataset GetNextFeature()

def ogr_gmlas_dataset_getnextfeature():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml')

    if ds.TestCapability(ogr.ODsCRandomLayerRead) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    count = 0
    last_l = None
    while True:
        f, l = ds.GetNextFeature()
        if f is None:
            if l is not None:
                gdaltest.post_reason('fail')
                return 'fail'
            break
        count += 1
        last_l = l

    if count != 53:
        gdaltest.post_reason('fail')
        print(count)
        return 'fail'

    if last_l.GetName() != 'main_elt':
        gdaltest.post_reason('fail')
        print(last_l.GetName())
        return 'fail'

    f, l = ds.GetNextFeature()
    if f is not None or l is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.ResetReading()
    last_pct =  0
    while True:
        f, l, pct = ds.GetNextFeature( include_pct = True )
        last_pct = pct
        if f is None:
            if l is not None:
                gdaltest.post_reason('fail')
                return 'fail'
            break
    if last_pct != 1.0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = ['EXPOSE_METADATA_LAYERS=YES'])
    fc_map = {}
    for layer_name in ('_ogr_fields_metadata', '_ogr_layers_metadata', '_ogr_layer_relationships' ):
        fc_map[layer_name] = ds.GetLayerByName(layer_name).GetFeatureCount()
    ds = None

    ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml', open_options = ['EXPOSE_METADATA_LAYERS=YES'])
    count = 0
    while True:
        f, l = ds.GetNextFeature()
        if f is None:
            if l is not None:
                gdaltest.post_reason('fail')
                return 'fail'
            break
        count += 1

    expected_count = 53
    expected_count += fc_map['_ogr_fields_metadata']
    expected_count += fc_map['_ogr_layers_metadata']
    expected_count += fc_map['_ogr_layer_relationships']
    if count != expected_count:
        gdaltest.post_reason('fail')
        print(count)
        return 'fail'

    f, l = ds.GetNextFeature()
    if f is not None or l is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.ResetReading()

    count = 0
    while True:
        f, l = ds.GetNextFeature()
        if f is None:
            if l is not None:
                gdaltest.post_reason('fail')
                return 'fail'
            break
        count += 1

    if count != expected_count:
        gdaltest.post_reason('fail')
        print(count)
        print(expected_count)
        return 'fail'


    for layers in [ ['_ogr_fields_metadata'],
                    ['_ogr_layers_metadata'],
                    ['_ogr_layer_relationships'],
                    ['_ogr_fields_metadata', '_ogr_layers_metadata'],
                    ['_ogr_fields_metadata', '_ogr_layer_relationships'],
                    ['_ogr_layers_metadata', '_ogr_layer_relationships'],
                  ]:

        ds = gdal.OpenEx('GMLAS:data/gmlas_test1.xml')
        expected_count = 53
        for layer in layers:
            ds.GetLayerByName(layer)
            expected_count += fc_map[layer]

        count = 0
        while True:
            f, l = ds.GetNextFeature()
            if f is None:
                if l is not None:
                    gdaltest.post_reason('fail')
                    return 'fail'
                break
            count += 1

        if count != expected_count:
            gdaltest.post_reason('fail')
            print(count)
            print(expected_count)
            return 'fail'

        f, l = ds.GetNextFeature()
        if f is not None or l is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
#  Test that with schemas that have a structure like a base:identifier, we
# will inline it.

def ogr_gmlas_inline_identifier():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/gmlas_fake_gml32.xsd',
                           open('data/gmlas_fake_gml32.xsd', 'rb').read())

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_inline_identifier.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              xmlns:gml="http://fake_gml32"
           elementFormDefault="qualified" attributeFormDefault="unqualified">

<xs:import namespace="http://fake_gml32" schemaLocation="gmlas_fake_gml32.xsd"/>

<xs:element name="identifier">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="foo" type="xs:string" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>

<xs:element name="first" substitutionGroup="gml:AbstractFeature">
  <xs:complexType>
    <xs:complexContent>
      <xs:extension base="gml:AbstractFeatureType">
        <xs:sequence>
            <xs:element ref="identifier" minOccurs="0"/>
        </xs:sequence>
      </xs:extension> 
    </xs:complexContent>
  </xs:complexType>
</xs:element>

<xs:element name="second" substitutionGroup="gml:AbstractFeature">
  <xs:complexType>
    <xs:complexContent>
      <xs:extension base="gml:AbstractFeatureType">
        <xs:sequence>
            <xs:element ref="identifier" minOccurs="0"/>
        </xs:sequence>
      </xs:extension> 
    </xs:complexContent>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=/vsimem/ogr_gmlas_inline_identifier.xsd'])
    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('fail')
        print( ds.GetLayerCount() )
        return 'fail'
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldIndex('identifier_foo') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_inline_identifier.xsd')
    gdal.Unlink('/vsimem/gmlas_fake_gml32.xsd')

    return 'success'

###############################################################################
#  Test that we can handle things like gml:name and au:name

def ogr_gmlas_avoid_same_name_inlined_classes():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_avoid_same_name_inlined_classes_ns1.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              xmlns:ns1="http://ns1"
              targetNamespace="http://ns1"
              elementFormDefault="qualified" attributeFormDefault="unqualified">

<xs:element name="dt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="val" type="xs:dateTime" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_avoid_same_name_inlined_classes_ns2.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              xmlns:ns1="http://ns2"
              targetNamespace="http://ns2"
              elementFormDefault="qualified" attributeFormDefault="unqualified">

<xs:element name="dt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="val" type="xs:dateTime" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_avoid_same_name_inlined_classes.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              xmlns:ns1="http://ns1"
              xmlns:ns2="http://ns2"
              elementFormDefault="qualified" attributeFormDefault="unqualified">

<xs:import namespace="http://ns1" schemaLocation="ogr_gmlas_avoid_same_name_inlined_classes_ns1.xsd"/>
<xs:import namespace="http://ns2" schemaLocation="ogr_gmlas_avoid_same_name_inlined_classes_ns2.xsd"/>

<xs:group name="mygroup">
    <xs:sequence>
        <xs:element ref="ns2:dt" minOccurs="0" maxOccurs="2"/>
    </xs:sequence>
</xs:group>

<xs:element name="myFeature">
  <xs:complexType>
        <xs:sequence>
            <xs:element ref="ns1:dt" minOccurs="0" maxOccurs="2"/>
            <xs:group ref="mygroup"/>
        </xs:sequence>
  </xs:complexType>
</xs:element>

</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:', open_options=['XSD=/vsimem/ogr_gmlas_avoid_same_name_inlined_classes.xsd'])
    if ds.GetLayerCount() != 3:
        gdaltest.post_reason('fail')
        print( ds.GetLayerCount() )
        return 'fail'
    lyr = ds.GetLayerByName('myFeature_ns1_dt')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('myFeature_ns2_dt')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_avoid_same_name_inlined_classes.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_avoid_same_name_inlined_classes_ns1.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_avoid_same_name_inlined_classes_ns2.xsd')

    return 'success'


###############################################################################
#  Test validation with an optional fixed attribute that is ignored

def ogr_gmlas_validate_ignored_fixed_attribute():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_validate_ignored_fixed_attribute.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_validate_ignored_fixed_attribute.xsd">
</myns:main_elt>
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_validate_ignored_fixed_attribute.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
  <xs:complexType>
        <xs:sequence>
            <xs:element name="foo" type="xs:string" minOccurs="0"/>
        </xs:sequence>
        <xs:attribute name="bar" type="xs:string" fixed="fixed_val"/>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    myhandler = MyHandler()
    gdal.PushErrorHandler(myhandler.error_handler)
    gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_validate_ignored_fixed_attribute.xml',
        open_options = [ 'VALIDATE=YES',
                         'CONFIG_FILE=<Configuration><IgnoredXPaths><XPath>@bar</XPath></IgnoredXPaths></Configuration>'])
    gdal.PopErrorHandler()
    if len(myhandler.error_list) != 0:
        gdaltest.post_reason('fail')
        print(myhandler.error_list)
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_validate_ignored_fixed_attribute.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_validate_ignored_fixed_attribute.xsd')

    return 'success'


###############################################################################
#  Test REMOVE_UNUSED_LAYERS and REMOVE_UNUSED_FIELDS options

def ogr_gmlas_remove_unused_layers_and_fields():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_remove_unused_layers_and_fields.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              elementFormDefault="qualified"
              attributeFormDefault="unqualified">

<xs:element name="main_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="unused1" type="xs:string" minOccurs="0"/>
        <xs:element name="used1" type="xs:string" minOccurs="0"/>
        <xs:element name="unused2" type="xs:string" minOccurs="0"/>
        <xs:element name="unused3" type="xs:string" minOccurs="0"/>
        <xs:element name="used2" type="xs:string" minOccurs="0"/>
        <xs:element name="nillable" nillable="true">
            <xs:complexType>
                <xs:simpleContent>
                    <xs:extension base="xs:integer">
                        <xs:attribute name="nilReason" type="xs:string"/>
                    </xs:extension>
                </xs:simpleContent>
            </xs:complexType>
        </xs:element>
    </xs:sequence>
  </xs:complexType>
</xs:element>

<xs:element name="unused_elt">
  <xs:complexType>
    <xs:sequence>
        <xs:element name="unused1" type="xs:dateTime" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:element>
</xs:schema>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_remove_unused_layers_and_fields.xml',
"""
<main_elt xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          xsi:noNamespaceSchemaLocation="ogr_gmlas_remove_unused_layers_and_fields.xsd">
    <used1>foo</used1>
    <used2>bar</used2>
    <nillable xsi:nil="true" nilReason="unknown"/>
</main_elt>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_remove_unused_layers_and_fields.xml',
                     open_options = ['REMOVE_UNUSED_LAYERS=YES',
                                     'REMOVE_UNUSED_FIELDS=YES'])
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if lyr.GetLayerDefn().GetFieldCount() != 4:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        f.DumpReadable()
        return 'fail'
    if f['used1'] != 'foo' or f['used2'] != 'bar' or f['nillable_nilReason'] != 'unknown':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_remove_unused_layers_and_fields.xsd')
    gdal.Unlink('/vsimem/ogr_gmlas_remove_unused_layers_and_fields.xml')

    return 'success'

###############################################################################
#  Test xlink resolution

def ogr_gmlas_xlink_resolver():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    (webserver_process, webserver_port) = webserver.launch(fork_process = False)
    if webserver_port == 0:
        return 'skip'

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
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
              targetNamespace="http://fake_xlink"
              elementFormDefault="qualified"
              attributeFormDefault="unqualified">
<xs:attribute name="href" type="xs:anyURI"/>
</xs:schema>""")

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
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    ds = None

    # Enable resolution, but only from local cache
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>false</AllowRemoteDownload>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""] )
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldIndex('my_link_rawcontent') < 0:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    f = lyr.GetNextFeature()
    if f.IsFieldSet('my_link_rawcontent'):
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    # Try again but this time with the cached file
    cached_file = '/vsimem/gmlas_xlink_cache/localhost_%d_vsimem_resource.xml' % webserver_port
    gdal.FileFromMemBuffer(cached_file, 'foo')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] != 'foo':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    ds = None
    gdal.Unlink(cached_file)

    # Enable remote resolution (but local caching disabled)
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'bar')
    gdal.FileFromMemBuffer('/vsimem/resource2.xml', 'baz')
    gdal.SetConfigOption('GMLAS_XLINK_RAM_CACHE_SIZE', '5')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""] )
    gdal.SetConfigOption('GMLAS_XLINK_RAM_CACHE_SIZE', None)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] != 'bar':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    # Check that the content is not cached
    if gdal.VSIStatL(cached_file) is not None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    # Delete the remote file and check that we can retrieve it from RAM cache
    gdal.Unlink('/vsimem/resource.xml')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] != 'bar':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] != 'baz':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    gdal.Unlink('/vsimem/resource2.xml')
    lyr.ResetReading()
    # /vsimem/resource.xml has been evicted from the cache
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] is not None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] != 'baz':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    ds = None

    # Enable remote resolution and caching
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'bar')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <CacheResults>true</CacheResults>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""] )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] != 'bar':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    # Check that the content is cached
    if gdal.VSIStatL(cached_file) is None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    ds = None

    # Enable remote resolution and caching and REFRESH_CACHE
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'baz')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ['REFRESH_CACHE=YES', """CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <CacheResults>true</CacheResults>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""] )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_rawcontent' ] != 'baz':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    # Check that the content is cached
    if gdal.VSIStatL(cached_file) is None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
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
        open_options = ["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""] )
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f.IsFieldSet('my_link_rawcontent'):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    ds = None

    # Test file size limit
    gdal.Unlink(cached_file)
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <MaxFileSize>1</MaxFileSize>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <DefaultResolution enabled="true">
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <CacheResults>true</CacheResults>
            </DefaultResolution>
        </XLinkResolution></Configuration>"""] )
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    # Check that the content is not cached
    if gdal.VSIStatL(cached_file) is not None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    ds = None

    # Test with URL specific rule with RawContent resolution
    gdal.FileFromMemBuffer('/vsimem/resource.xml', 'bar')
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ["""CONFIG_FILE=<Configuration>
        <XLinkResolution>
            <CacheDirectory>/vsimem/gmlas_xlink_cache</CacheDirectory>
            <URLSpecificResolution>
                <URLPrefix>http://localhost:%d/vsimem/</URLPrefix>
                <AllowRemoteDownload>true</AllowRemoteDownload>
                <ResolutionMode>RawContent</ResolutionMode>
                <CacheResults>true</CacheResults>
            </URLSpecificResolution>
        </XLinkResolution></Configuration>""" % webserver_port] )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['my_link_attr_before'] != 'a' or \
       f['my_link_href'] != 'http://localhost:%d/vsimem/resource.xml' % webserver_port or \
       f['my_link_rawcontent'] != 'bar' or \
       f['my_link_attr_after'] != 'b':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
    # Check that the content is cached
    if gdal.VSIStatL(cached_file) is None:
        gdaltest.post_reason('fail')
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'
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
</FeatureCollection>""" % (webserver_port, webserver_port,webserver_port, webserver_port))

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
        </XLinkResolution></Configuration>""" % (webserver_port,webserver_port)

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ['CONFIG_FILE=' + config_file] )
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
        gdaltest.post_reason('fail')
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    f = lyr.GetNextFeature()
    if f['my_link2_bar'] != 123:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

    gdal.Unlink('/vsimem/subdir1/resource.xml')
    gdal.Unlink('/vsimem/subdir2/resource2_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_super_very_long.xml')

    # Test caching
    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_xlink_resolver.xml',
        open_options = ['CONFIG_FILE=' + config_file] )
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
        gdaltest.post_reason('fail')
        f.DumpReadable()
        webserver.server_stop(webserver_process, webserver_port)
        return 'fail'

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

    return 'success'

###############################################################################
# Test UTF-8 support

def ogr_gmlas_recoding():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    if sys.version_info >= (3,0,0):
        accent = '\u00e9'
    else:
        exec("accent = u'\\u00e9'")
        accent = accent.encode('UTF-8')

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_recoding.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_recoding.xsd">
<myns:attr>""" + accent + '</myns:attr></myns:main_elt>')

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_recoding.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="attr" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_recoding.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['attr'] != accent:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_recoding.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_recoding.xsd')

    return 'success'

###############################################################################
# Test schema without namespace prefix

def ogr_gmlas_schema_without_namespace_prefix():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    # Generic http:// namespace URI
    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://my/ns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="attr" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd'])
    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    f = lyr.GetNextFeature()
    if f['layer_xpath'] != 'my_ns:main_elt':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd')

    # http://www.opengis.net/ namespace URI
    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="http://www.opengis.net/fake/3.0"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="attr" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd'])
    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    f = lyr.GetNextFeature()
    if f['layer_xpath'] != 'fake_3_0:main_elt':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd')

    # Non http:// namespace URI
    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           targetNamespace="my_namespace"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="attr" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:', open_options = ['XSD=/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd'])
    lyr = ds.GetLayerByName('_ogr_layers_metadata')
    f = lyr.GetNextFeature()
    if f['layer_xpath'] != 'my_namespace:main_elt':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_schema_without_namespace_prefix.xsd')


    return 'success'

###############################################################################
# Test parsing truncated XML

def ogr_gmlas_truncated_xml():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_truncated_xml.xml',
"""<myns:main_elt xmlns:myns="http://myns"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xsi:schemaLocation="http://myns ogr_gmlas_truncated_xml.xsd">
<myns:attr>foo""")

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_truncated_xml.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="main_elt">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="attr" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:/vsimem/ogr_gmlas_truncated_xml.xml')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gmlas_truncated_xml.xml')
    gdal.Unlink('/vsimem/ogr_gmlas_truncated_xml.xsd')

    return 'success'

###############################################################################
# Test identifier truncation

def ogr_gmlas_identifier_truncation():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_identifier_truncation.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="very_long_identifier_class">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="very_long_idenTifier" type="xs:string"/>
            <xs:element name="another_long_identifier" type="xs:string"/>
            <xs:element name="another_long_identifierbis" type="xs:string"/>
            <xs:element name="x" type="xs:string"/>
            <xs:element name="noTCAMELCaseAndLong" type="xs:string"/>
            <xs:element name="suuuuuuuuuuuperlong" type="xs:string"/>
            <xs:element name="s_u_u_u_u_u_u_u_u_u_u_u_p_e_r_l_o_n_g" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
<xs:element name="another_long_identifier_class">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="x" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
<xs:element name="another_long_identifier_classbis">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="x" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
<xs:element name="y">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="x" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:', open_options = [
            'XSD=/vsimem/ogr_gmlas_identifier_truncation.xsd',
            'CONFIG_FILE=<Configuration><LayerBuildingRules><IdentifierMaxLength>10</IdentifierMaxLength></LayerBuildingRules></Configuration>'])
    lyr = ds.GetLayerByName('v_l_i_clas')
    if lyr is None:
        gdaltest.post_reason('fail')
        print(ds.GetLayer(0).GetName())
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(1).GetName()
    if s != 'v_l_idTifi':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(2).GetName()
    if s != 'an_lo_ide1':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(3).GetName()
    if s != 'an_lo_ide2':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(4).GetName()
    if s != 'x':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(5).GetName()
    if s != 'noTCAMELCa':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(6).GetName()
    if s != 'suuuuuuuuu':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(7).GetName()
    if s != '_r_l_o_n_g':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    lyr = ds.GetLayerByName('a_l_i_cla1')
    if lyr is None:
        gdaltest.post_reason('fail')
        print(ds.GetLayer(1).GetName())
        return 'fail'
    lyr = ds.GetLayerByName('a_l_i_cla2')
    if lyr is None:
        gdaltest.post_reason('fail')
        print(ds.GetLayer(2).GetName())
        return 'fail'
    lyr = ds.GetLayerByName('y')
    if lyr is None:
        gdaltest.post_reason('fail')
        print(ds.GetLayer(3).GetName())
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gmlas_identifier_truncation.xsd')

    return 'success'

###############################################################################
# Test behaviour when identifiers have same case

def ogr_gmlas_identifier_case_ambiguity():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_gmlas_identifier_case_ambiguity.xsd',
"""<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
           xmlns:myns="http://myns" 
           targetNamespace="http://myns"
           elementFormDefault="qualified" attributeFormDefault="unqualified">
<xs:element name="differentcase">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="differentcase" type="xs:string"/>
            <xs:element name="DifferentCASE" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
<xs:element name="DifferentCASE">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="x" type="xs:string"/>
        </xs:sequence>
    </xs:complexType>
</xs:element>
</xs:schema>""")

    ds = gdal.OpenEx('GMLAS:', open_options = [
            'XSD=/vsimem/ogr_gmlas_identifier_case_ambiguity.xsd'])
    lyr = ds.GetLayerByName('differentcase1')
    if lyr is None:
        gdaltest.post_reason('fail')
        print(ds.GetLayer(0).GetName())
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(1).GetName()
    if s != 'differentcase1':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    s = lyr.GetLayerDefn().GetFieldDefn(2).GetName()
    if s != 'DifferentCASE2':
        gdaltest.post_reason('fail')
        print(s)
        return 'fail'
    lyr = ds.GetLayerByName('DifferentCASE2')
    if lyr is None:
        gdaltest.post_reason('fail')
        print(ds.GetLayer(0).GetName())
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gmlas_identifier_case_ambiguity.xsd')

    return 'success'

###############################################################################
#  Cleanup

def ogr_gmlas_cleanup():

    if ogr.GetDriverByName('GMLAS') is None:
        return 'skip'

    files = gdal.ReadDir('/vsimem/')
    if files is not None:
        print('Remaining files: ' + str(files))

    gdal.SetConfigOption('GMLAS_WARN_UNEXPECTED', None)
    gdal.SetConfigOption('GDAL_XML_VALIDATION', ogrtest.old_val_GDAL_XML_VALIDATION)

    return 'success'

gdaltest_list = [
    ogr_gmlas_basic,
    ogr_gmlas_test_ogrsf,
    ogr_gmlas_virtual_file,
    ogr_gmlas_datafile_with_xsd_option,
    ogr_gmlas_no_datafile_with_xsd_option,
    ogr_gmlas_no_datafile_xsd_which_is_not_xsd,
    ogr_gmlas_no_datafile_no_xsd,
    ogr_gmlas_non_existing_gml,
    ogr_gmlas_non_existing_xsd,
    ogr_gmlas_gml_without_schema_location,
    ogr_gmlas_invalid_schema,
    ogr_gmlas_invalid_xml,
    ogr_gmlas_gml_Reference,
    ogr_gmlas_same_element_in_different_ns,
    ogr_gmlas_corner_case_relative_path,
    ogr_gmlas_unexpected_repeated_element,
    ogr_gmlas_unexpected_repeated_element_variant,
    ogr_gmlas_geometryproperty,
    ogr_gmlas_abstractgeometry,
    ogr_gmlas_validate,
    ogr_gmlas_test_ns_prefix,
    ogr_gmlas_no_namespace,
    ogr_gmlas_conf,
    ogr_gmlas_conf_ignored_xpath,
    ogr_gmlas_cache,
    ogr_gmlas_link_nested_independant_child,
    ogr_gmlas_composition_compositionPart,
    ogr_gmlas_instantiate_only_gml_feature,
    ogr_gmlas_timestamp_ignored_for_hash,
    ogr_gmlas_dataset_getnextfeature,
    ogr_gmlas_inline_identifier,
    ogr_gmlas_avoid_same_name_inlined_classes,
    ogr_gmlas_validate_ignored_fixed_attribute,
    ogr_gmlas_remove_unused_layers_and_fields,
    ogr_gmlas_xlink_resolver,
    ogr_gmlas_recoding,
    ogr_gmlas_schema_without_namespace_prefix,
    ogr_gmlas_truncated_xml,
    ogr_gmlas_identifier_truncation,
    ogr_gmlas_identifier_case_ambiguity,
    ogr_gmlas_cleanup ]

# gdaltest_list = [ ogr_gmlas_basic, ogr_gmlas_identifier_truncation ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gmlas' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
