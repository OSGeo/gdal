#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test driver metadata
# Author:   Rene Buffat <buffat at gmail dot com>
#
###############################################################################
# Copyright (c) 2020, Rene Buffat <buffat at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

# For unknown reason on mingw64 CI, this now crashes
# on File "D:/a/gdal/gdal/build/autotest/gcore/test_driver_metadata.py", line 396 in test_metadata_openoptionlist
# This used to work previously, so likely related to some update in a
# mingw64 component, although the diff between a broken and working CI run
# does not show an obvious culprit. And this works in a local VM...
# And now this breaks on build-windows-conda too
pytestmark = pytest.mark.skipif(
    gdaltest.is_travis_branch("mingw64")
    or gdaltest.is_travis_branch("build-windows-conda")
    or gdaltest.is_travis_branch("build-windows-minimum"),
    reason="Crashes for unknown reason",
)


all_driver_names = [
    gdal.GetDriver(i).GetDescription() for i in range(gdal.GetDriverCount())
]
ogr_driver_names = [
    driver_name
    for driver_name in all_driver_names
    if gdal.GetDriverByName(driver_name).GetMetadataItem("DCAP_VECTOR") == "YES"
]
gdal_driver_names = [
    driver_name
    for driver_name in all_driver_names
    if gdal.GetDriverByName(driver_name).GetMetadataItem("DCAP_RASTER") == "YES"
]
multidim_driver_name = [
    driver_name
    for driver_name in gdal_driver_names
    if gdal.GetDriverByName(driver_name).GetMetadataItem("DCAP_MULTIDIM_RASTER")
    == "YES"
]


def get_schema_openoptionslist():

    from lxml import etree

    return etree.XML(
        r"""
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
      <xs:attribute name="type" use="required">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:enumeration value="INT" />
            <xs:enumeration value="INTEGER" />
            <xs:enumeration value="UNSIGNED INT" />
            <xs:enumeration value="FLOAT" />
            <xs:enumeration value="BOOLEAN" />
            <xs:enumeration value="STRING-SELECT" />
            <xs:enumeration value="STRING" />
            <xs:enumeration value="int" />
            <xs:enumeration value="integer" />
            <xs:enumeration value="unsigned int" />
            <xs:enumeration value="float" />
            <xs:enumeration value="boolean" />
            <xs:enumeration value="string-select" />
            <xs:enumeration value="string" />
          </xs:restriction>
        </xs:simpleType>
      </xs:attribute>
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
"""
    )


def get_schema_creationoptionslist_xml(root_element="CreationOptionList"):

    from lxml import etree

    return etree.XML(
        rf"""
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
    <xs:element name="Value">
    <xs:complexType>
      <xs:simpleContent>
        <xs:extension base="xs:string">
          <xs:attribute type="xs:string" name="alias" use="optional"/>
          <xs:attribute type="xs:string" name="aliasOf" use="optional"/>
          <xs:attribute type="xs:string" name="remark" use="optional"/>
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
      <xs:attribute name="type" use="required">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:enumeration value="INT" />
            <xs:enumeration value="INTEGER" />
            <xs:enumeration value="UNSIGNED INT" />
            <xs:enumeration value="FLOAT" />
            <xs:enumeration value="BOOLEAN" />
            <xs:enumeration value="STRING-SELECT" />
            <xs:enumeration value="STRING" />
            <xs:enumeration value="int" />
            <xs:enumeration value="integer" />
            <xs:enumeration value="unsigned int" />
            <xs:enumeration value="float" />
            <xs:enumeration value="boolean" />
            <xs:enumeration value="string-select" />
            <xs:enumeration value="string" />
          </xs:restriction>
        </xs:simpleType>
      </xs:attribute>
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
  <xs:element name="{root_element}">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>
"""
    )


def get_schema_layer_creationoptionslist_xml():

    from lxml import etree

    return etree.XML(
        r"""
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
      <xs:attribute name="type" use="required">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:enumeration value="INT" />
            <xs:enumeration value="INTEGER" />
            <xs:enumeration value="UNSIGNED INT" />
            <xs:enumeration value="FLOAT" />
            <xs:enumeration value="BOOLEAN" />
            <xs:enumeration value="STRING-SELECT" />
            <xs:enumeration value="STRING" />
            <xs:enumeration value="int" />
            <xs:enumeration value="integer" />
            <xs:enumeration value="unsigned int" />
            <xs:enumeration value="float" />
            <xs:enumeration value="boolean" />
            <xs:enumeration value="string-select" />
            <xs:enumeration value="string" />
          </xs:restriction>
        </xs:simpleType>
      </xs:attribute>
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
"""
    )


def get_schema_multidim_array_creationoptionslist_xml():

    from lxml import etree

    return etree.XML(
        r"""
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
      <xs:attribute type="xs:string" name="name" use="optional"/>
      <xs:attribute type="xs:string" name="type" use="optional"/>
      <xs:attribute type="xs:string" name="description" use="optional"/>
      <xs:attribute type="xs:string" name="default" use="optional"/>
      <xs:attribute type="xs:string" name="min" use="optional"/>
      <xs:attribute type="xs:string" name="max" use="optional"/>
    </xs:complexType>
  </xs:element>
  <xs:element name="MultiDimArrayCreationOptionList">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>
"""
    )


def get_schema_multidim_attribute_creationoptionslist_xml():

    from lxml import etree

    return etree.XML(
        r"""
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="Value" type="xs:string"/>
  <xs:element name="Option">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Value" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
      <xs:attribute type="xs:string" name="name"/>
      <xs:attribute type="xs:string" name="type"/>
      <xs:attribute type="xs:string" name="default"/>
    </xs:complexType>
  </xs:element>
  <xs:element name="MultiDimAttributeCreationOptionList">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>"""
    )


def get_schema_multidim_dataset_creationoptionslist_xml():

    from lxml import etree

    return etree.XML(
        r"""
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="Value" type="xs:string"/>
  <xs:element name="Option">
    <xs:complexType mixed="true">
      <xs:sequence>
        <xs:element ref="Value" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
      <xs:attribute type="xs:string" name="name" use="required"/>
      <xs:attribute type="xs:string" name="alias" use="optional"/>
      <xs:attribute type="xs:string" name="type" use="optional"/>
      <xs:attribute type="xs:string" name="default" use="optional"/>
      <xs:attribute type="xs:string" name="description" use="optional"/>
    </xs:complexType>
  </xs:element>
  <xs:element name="MultiDimDatasetCreationOptionList">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option" maxOccurs="unbounded" minOccurs="0"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>"""
    )


def get_schema_multidim_dimension_creationoptionslist_xml():

    from lxml import etree

    return etree.XML(
        r"""
<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="Option">
    <xs:complexType>
      <xs:simpleContent>
        <xs:extension base="xs:string">
          <xs:attribute type="xs:string" name="name"/>
          <xs:attribute type="xs:string" name="type"/>
          <xs:attribute type="xs:string" name="description"/>
          <xs:attribute type="xs:string" name="default"/>
        </xs:extension>
      </xs:simpleContent>
    </xs:complexType>
  </xs:element>
  <xs:element name="MultiDimDimensionCreationOptionList">
    <xs:complexType>
      <xs:sequence>
        <xs:element ref="Option"/>
      </xs:sequence>
    </xs:complexType>
  </xs:element>
</xs:schema>"""
    )


@pytest.mark.parametrize("driver_name", all_driver_names)
def test_metadata_has_long_name(driver_name):

    driver = gdal.GetDriverByName(driver_name)
    assert driver.GetMetadataItem(gdal.DMD_LONGNAME) is not None


@pytest.mark.parametrize("driver_name", all_driver_names)
def test_metadata_dcap_yes(driver_name):
    """Test that the only value of DCAP_ elements is YES"""

    driver = gdal.GetDriverByName(driver_name)
    md = driver.GetMetadata()
    for key in md:
        if key.startswith("DCAP_"):
            value = md[key]
            assert value == "YES", (key, value)


@pytest.mark.parametrize("driver_name", all_driver_names)
def test_metadata_openoptionlist(driver_name):
    """Test if DMD_OPENOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(get_schema_openoptionslist())

    driver = gdal.GetDriverByName(driver_name)
    openoptionlist_xml = driver.GetMetadataItem("DMD_OPENOPTIONLIST")

    if openoptionlist_xml is not None and len(openoptionlist_xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(openoptionlist_xml, parser)
        except Exception:
            print(openoptionlist_xml)
            raise


@pytest.mark.parametrize("driver_name", all_driver_names)
def test_metadata_creationoptionslist(driver_name):
    """Test if DMD_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(get_schema_creationoptionslist_xml())

    driver = gdal.GetDriverByName(driver_name)
    creationoptionslist_xml = driver.GetMetadataItem("DMD_CREATIONOPTIONLIST")

    if creationoptionslist_xml is not None and len(creationoptionslist_xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(creationoptionslist_xml, parser)
        except Exception:
            print(creationoptionslist_xml)
            raise


@pytest.mark.parametrize("driver_name", all_driver_names)
def test_metadata_overview_creationoptionslist(driver_name):
    """Test if DMD_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(
        get_schema_creationoptionslist_xml("OverviewCreationOptionList")
    )

    driver = gdal.GetDriverByName(driver_name)
    creationoptionslist_xml = driver.GetMetadataItem(
        gdal.DMD_OVERVIEW_CREATIONOPTIONLIST
    )

    if creationoptionslist_xml is not None and len(creationoptionslist_xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(creationoptionslist_xml, parser)
        except Exception:
            print(creationoptionslist_xml)
            raise


@pytest.mark.parametrize("driver_name", ogr_driver_names)
def test_metadata_layer_creationoptionslist(driver_name):
    """Test if DS_LAYER_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(get_schema_layer_creationoptionslist_xml())

    driver = gdal.GetDriverByName(driver_name)
    creationoptionslist_xml = driver.GetMetadataItem("DS_LAYER_CREATIONOPTIONLIST")

    if creationoptionslist_xml is not None and len(creationoptionslist_xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(creationoptionslist_xml, parser)
        except Exception:
            print(creationoptionslist_xml)
            raise


@pytest.mark.parametrize("driver_name", multidim_driver_name)
def test_metadata_multidim_array_creationoptionslist(driver_name):
    """Test if DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(get_schema_multidim_array_creationoptionslist_xml())

    driver = gdal.GetDriverByName(driver_name)
    xml = driver.GetMetadataItem("DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST")

    if xml is not None and len(xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(xml, parser)
        except Exception:
            print(xml)
            raise


@pytest.mark.parametrize("driver_name", multidim_driver_name)
def test_metadata_multidim_attribute_creationoptionslist(driver_name):
    """Test if DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(get_schema_multidim_attribute_creationoptionslist_xml())

    driver = gdal.GetDriverByName(driver_name)
    xml = driver.GetMetadataItem("DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST")

    if xml is not None and len(xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(xml, parser)
        except Exception:
            print(xml)
            raise


@pytest.mark.parametrize("driver_name", multidim_driver_name)
def test_metadata_multidim_dataset_creationoptionslist(driver_name):
    """Test if DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(get_schema_multidim_dataset_creationoptionslist_xml())

    driver = gdal.GetDriverByName(driver_name)
    xml = driver.GetMetadataItem("DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST")

    if xml is not None and len(xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(xml, parser)
        except Exception:
            print(xml)
            raise


@pytest.mark.parametrize("driver_name", multidim_driver_name)
def test_metadata_multidim_dimension_creationoptionslist(driver_name):
    """Test if DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    schema = etree.XMLSchema(get_schema_multidim_dimension_creationoptionslist_xml())

    driver = gdal.GetDriverByName(driver_name)
    xml = driver.GetMetadataItem("DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST")

    if xml is not None and len(xml) > 0:
        # do not fail
        try:
            parser = etree.XMLParser(schema=schema)
            etree.fromstring(xml, parser)
        except Exception:
            print(xml)
            raise


@pytest.mark.parametrize("driver_name", multidim_driver_name)
def test_metadata_multidim_group_creationoptionslist(driver_name):
    """Test if DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST metadataitem is present and can be parsed"""

    from lxml import etree

    # TODO: create schema if xml is available
    # schema = etree.XMLSchema(schema_multidim_group_creationoptionslist_xml)

    driver = gdal.GetDriverByName(driver_name)
    xml = driver.GetMetadataItem("DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST")

    if xml is not None and len(xml) > 0:
        # do not fail
        try:
            # parser = etree.XMLParser(schema=schema)
            # etree.fromstring(xml, parser)
            etree.fromstring(xml)
        except Exception:
            print(xml)
            raise


@pytest.mark.parametrize("driver_name", all_driver_names)
def test_metadata_creation_field_datatypes(driver_name):
    """Test if DMD_CREATIONFIELDDATATYPES metadataitem returns valid datatypes"""

    valid_datatypes = {
        "Integer",
        "Integer64",
        "Real",
        "String",
        "Date",
        "DateTime",
        "Time",
        "IntegerList",
        "Integer64List",
        "RealList",
        "StringList",
        "Binary",
    }

    driver = gdal.GetDriverByName(driver_name)
    datatypes_str = driver.GetMetadataItem("DMD_CREATIONFIELDDATATYPES")
    if datatypes_str is not None:
        for datatype in datatypes_str.split(" "):
            assert datatype in valid_datatypes


@pytest.mark.parametrize("driver_name", all_driver_names)
def test_metadata_creation_sub_field_datatypes(driver_name):
    """Test if DMD_CREATIONFIELDDATASUBTYPES metadataitem returns valid data subtypes"""

    valid_datatypes = {"Boolean", "Float32", "Int16", "JSON", "UUID"}

    driver = gdal.GetDriverByName(driver_name)
    datatypes_str = driver.GetMetadataItem("DMD_CREATIONFIELDDATASUBTYPES")
    if datatypes_str is not None:
        for datatype in datatypes_str.split(" "):
            assert datatype in valid_datatypes


@pytest.mark.parametrize("driver_name", ogr_driver_names)
def test_metadata_alter_geom_field_defn_flags(driver_name):
    """Test if GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS metadataitem returns valid flags"""

    supported_flags = {"Name", "Type", "Nullable", "SRS", "CoordinateEpoch"}

    driver = gdal.GetDriverByName(driver_name)
    flags_str = driver.GetMetadataItem(gdal.DMD_ALTER_GEOM_FIELD_DEFN_FLAGS)
    if flags_str is not None:
        for flag in flags_str.split(" "):
            assert flag in supported_flags


@pytest.mark.parametrize("driver_name", ogr_driver_names)
def test_metadata_alter_field_defn_flags(driver_name):
    """Test if GDAL_DMD_ALTER_FIELD_DEFN_FLAGS metadataitem returns valid flags"""

    supported_flags = {
        "Name",
        "Type",
        "WidthPrecision",
        "Nullable",
        "Default",
        "Unique",
        "AlternativeName",
        "Comment",
        "Domain",
    }

    driver = gdal.GetDriverByName(driver_name)
    flags_str = driver.GetMetadataItem(gdal.DMD_ALTER_FIELD_DEFN_FLAGS)
    if flags_str is not None:
        for flag in flags_str.split(" "):
            assert flag in supported_flags


@pytest.mark.parametrize("driver_name", ogr_driver_names)
def test_metadata_creation_field_defn_flags(driver_name):
    """Test if GDAL_DMD_CREATION_FIELD_DEFN_FLAGS metadataitem returns valid flags"""

    supported_flags = {
        "WidthPrecision",
        "Nullable",
        "Default",
        "Unique",
        "AlternativeName",
        "Comment",
        "Domain",
    }

    driver = gdal.GetDriverByName(driver_name)
    flags_str = driver.GetMetadataItem(gdal.DMD_CREATION_FIELD_DEFN_FLAGS)
    if flags_str is not None:
        for flag in flags_str.split(" "):
            assert flag in supported_flags


@pytest.mark.parametrize("driver_name", ogr_driver_names)
def test_metadata_update_items(driver_name):
    """Test if DMD_UPDATE_ITEMS metadataitem returns valid flags"""

    supported_flags = {
        "GeoTransform",
        "SRS",
        "GCPs",
        "NoData",
        "ColorInterpretation",
        "RasterValues",
        "DatasetMetadata",
        "BandMetadata",
        "Features",
        "LayerMetadata",
    }

    driver = gdal.GetDriverByName(driver_name)
    flags_str = driver.GetMetadataItem(gdal.DMD_UPDATE_ITEMS)
    if flags_str is not None:
        assert driver.GetMetadataItem(gdal.DCAP_UPDATE)
        for flag in flags_str.split(" "):
            assert flag in supported_flags
    else:
        assert not driver.GetMetadataItem(gdal.DCAP_UPDATE)
