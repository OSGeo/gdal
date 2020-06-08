#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test driver matadata xml
# Author:   Rene Buffat <buffat at gmail dot com>
#
###############################################################################
# Copyright (c) 2020, Rene Buffat <buffat at gmail dot com>
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


import pytest
from osgeo import gdal
from lxml import etree

all_driver_names = [gdal.GetDriver(i).GetDescription() for i in range(gdal.GetDriverCount())]
ogr_driver_names = [driver_name for driver_name in all_driver_names
                    if gdal.GetDriverByName(driver_name).GetMetadataItem('DCAP_VECTOR') == 'YES']
gdal_driver_names = [driver_name for driver_name in all_driver_names
                     if gdal.GetDriverByName(driver_name).GetMetadataItem('DCAP_RASTER') == 'YES']


schema_openoptionslist = etree.XML(r"""
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
    <xs:element name="Value">
    <xs:complexType>
      <xs:simpleContent>
        <xs:extension base="xs:string">
          <xs:attribute type="xs:string" name="alias" use="optional"/>
          <xs:attribute type="xs:string" name="aliasOf" use="optional"/>
        </xs:extension>
      </xs:simpleContent>
    </xs:complexType>
  </xs:element>
  <xs:element name="Option">
    <xs:complexType mixed="true">
      <xs:sequence>
        <xs:element ref="Value" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
      <xs:attribute name="name" use="required">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:pattern value="[^\s]*"/>
          </xs:restriction>
        </xs:simpleType>
      </xs:attribute>
      <xs:attribute type="xs:string" name="type" use="required"/>
      <xs:attribute type="xs:string" name="description" use="optional"/>
      <xs:attribute type="xs:string" name="default" use="optional"/>
      <xs:attribute type="xs:string" name="scope" use="optional"/>
      <xs:attribute type="xs:string" name="required" use="optional"/>
      <xs:attribute type="xs:string" name="deprecated_alias" use="optional"/>
      <xs:attribute type="xs:string" name="alias" use="optional"/>
      <xs:attribute type="xs:string" name="min" use="optional"/>
      <xs:attribute type="xs:string" name="max" use="optional"/>
      <xs:attribute type="xs:string" name="aliasOf" use="optional"/>
      <xs:attribute type="xs:string" name="maxsize" use="optional"/>
      <xs:attribute type="xs:string" name="alt_config_option" use="optional"/>
    </xs:complexType>
  </xs:element>
  <xs:element name="OpenOptionList">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>
""")

schema_creationoptionslist_xml = etree.XML(r"""
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
    <xs:element name="Value">
    <xs:complexType>
      <xs:simpleContent>
        <xs:extension base="xs:string">
          <xs:attribute type="xs:string" name="alias" use="optional"/>
          <xs:attribute type="xs:string" name="aliasOf" use="optional"/>
        </xs:extension>
      </xs:simpleContent>
    </xs:complexType>
  </xs:element>
  <xs:element name="Option">
    <xs:complexType mixed="true">
      <xs:sequence>
        <xs:element ref="Value" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
      <xs:attribute name="name" use="required">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:pattern value="[^\s]*"/>
          </xs:restriction>
        </xs:simpleType>
      </xs:attribute>
      <xs:attribute type="xs:string" name="type" use="required"/>
      <xs:attribute type="xs:string" name="description" use="optional"/>
      <xs:attribute type="xs:string" name="default" use="optional"/>
      <xs:attribute type="xs:string" name="scope" use="optional"/>
      <xs:attribute type="xs:string" name="required" use="optional"/>
      <xs:attribute type="xs:string" name="deprecated_alias" use="optional"/>
      <xs:attribute type="xs:string" name="alias" use="optional"/>
      <xs:attribute type="xs:string" name="min" use="optional"/>
      <xs:attribute type="xs:string" name="max" use="optional"/>
      <xs:attribute type="xs:string" name="aliasOf" use="optional"/>
      <xs:attribute type="xs:string" name="maxsize" use="optional"/>
      <xs:attribute type="xs:string" name="alt_config_option" use="optional"/>
    </xs:complexType>
  </xs:element>
  <xs:element name="CreationOptionList">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>
""")

schema_layer_creationoptionslist_xml = etree.XML(r"""
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
    <xs:element name="Value">
    <xs:complexType>
      <xs:simpleContent>
        <xs:extension base="xs:string">
          <xs:attribute type="xs:string" name="alias" use="optional"/>
          <xs:attribute type="xs:string" name="aliasOf" use="optional"/>
        </xs:extension>
      </xs:simpleContent>
    </xs:complexType>
  </xs:element>
  <xs:element name="Option">
    <xs:complexType mixed="true">
      <xs:sequence>
        <xs:element ref="Value" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
      <xs:attribute name="name" use="required">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:pattern value="[^\s]*"/>
          </xs:restriction>
        </xs:simpleType>
      </xs:attribute>
      <xs:attribute type="xs:string" name="type" use="required"/>
      <xs:attribute type="xs:string" name="description" use="optional"/>
      <xs:attribute type="xs:string" name="default" use="optional"/>
      <xs:attribute type="xs:string" name="scope" use="optional"/>
      <xs:attribute type="xs:string" name="required" use="optional"/>
      <xs:attribute type="xs:string" name="deprecated_alias" use="optional"/>
      <xs:attribute type="xs:string" name="alias" use="optional"/>
      <xs:attribute type="xs:string" name="min" use="optional"/>
      <xs:attribute type="xs:string" name="max" use="optional"/>
      <xs:attribute type="xs:string" name="aliasOf" use="optional"/>
      <xs:attribute type="xs:string" name="maxsize" use="optional"/>
      <xs:attribute type="xs:string" name="alt_config_option" use="optional"/>
    </xs:complexType>
  </xs:element>
  <xs:element name="LayerCreationOptionList">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>
""")


@pytest.mark.parametrize('driver_name', all_driver_names)
def test_metadata_openoptionlist(driver_name):
    """ Test if DMD_OPENOPTIONLIST metadataitem is present and can be parsed """

    schema = etree.XMLSchema(schema_openoptionslist)

    driver = gdal.GetDriverByName(driver_name)
    openoptionlist_xml = driver.GetMetadataItem('DMD_OPENOPTIONLIST')

    if openoptionlist_xml is not None and len(openoptionlist_xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(openoptionlist_xml, parser)
        except:
            print(openoptionlist_xml)
            raise


@pytest.mark.parametrize('driver_name', all_driver_names)
def test_metadata_creationoptionslist(driver_name):
    """ Test if DMD_CREATIONOPTIONLIST metadataitem is present and can be parsed """

    schema = etree.XMLSchema(schema_creationoptionslist_xml)

    driver = gdal.GetDriverByName(driver_name)
    creationoptionslist_xml = driver.GetMetadataItem('DMD_CREATIONOPTIONLIST')

    if creationoptionslist_xml is not None and len(creationoptionslist_xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(creationoptionslist_xml, parser)
        except:
            print(creationoptionslist_xml)
            raise


@pytest.mark.parametrize('driver_name', ogr_driver_names)
def test_metadata_layer_creationoptionslist(driver_name):
    """ Test if DS_LAYER_CREATIONOPTIONLIST metadataitem is present and can be parsed """

    schema = etree.XMLSchema(schema_layer_creationoptionslist_xml)

    driver = gdal.GetDriverByName(driver_name)
    creationoptionslist_xml = driver.GetMetadataItem('DS_LAYER_CREATIONOPTIONLIST')

    if creationoptionslist_xml is not None and len(creationoptionslist_xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(creationoptionslist_xml, parser)
        except:
            print(creationoptionslist_xml)
            raise
