#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OSR XML import/export
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import re

import gdaltest
import pytest

from osgeo import osr

###############################################################################
# Test the osr.SpatialReference.ImportFromXML() function.
#


def test_osr_xml_1():

    gdaltest.srs_xml = """<gml:ProjectedCRS>
  <gml:srsName>WGS 84 / UTM zone 31N</gml:srsName>
  <gml:srsID>
    <gml:name codeSpace="urn:ogc:def:crs:EPSG::">32631</gml:name>
  </gml:srsID>
  <gml:baseCRS>
    <gml:GeographicCRS>
      <gml:srsName>WGS 84</gml:srsName>
      <gml:srsID>
        <gml:name codeSpace="urn:ogc:def:crs:EPSG::">4326</gml:name>
      </gml:srsID>
      <gml:usesEllipsoidalCS>
        <gml:EllipsoidalCS>
          <gml:csName>ellipsoidal</gml:csName>
          <gml:csID>
            <gml:name codeSpace="urn:ogc:def:cs:EPSG::">6402</gml:name>
          </gml:csID>
          <gml:usesAxis>
            <gml:CoordinateSystemAxis gml:uom="urn:ogc:def:uom:EPSG::9102">
              <gml:name>Geodetic latitude</gml:name>
              <gml:axisID>
                <gml:name codeSpace="urn:ogc:def:axis:EPSG::">9901</gml:name>
              </gml:axisID>
              <gml:axisAbbrev>Lat</gml:axisAbbrev>
              <gml:axisDirection>north</gml:axisDirection>
            </gml:CoordinateSystemAxis>
          </gml:usesAxis>
          <gml:usesAxis>
            <gml:CoordinateSystemAxis gml:uom="urn:ogc:def:uom:EPSG::9102">
              <gml:name>Geodetic longitude</gml:name>
              <gml:axisID>
                <gml:name codeSpace="urn:ogc:def:axis:EPSG::">9902</gml:name>
              </gml:axisID>
              <gml:axisAbbrev>Lon</gml:axisAbbrev>
              <gml:axisDirection>east</gml:axisDirection>
            </gml:CoordinateSystemAxis>
          </gml:usesAxis>
        </gml:EllipsoidalCS>
      </gml:usesEllipsoidalCS>
      <gml:usesGeodeticDatum>
        <gml:GeodeticDatum>
          <gml:datumName>WGS_1984</gml:datumName>
          <gml:datumID>
            <gml:name codeSpace="urn:ogc:def:datum:EPSG::">6326</gml:name>
          </gml:datumID>
          <gml:usesPrimeMeridian>
            <gml:PrimeMeridian>
              <gml:meridianName>Greenwich</gml:meridianName>
              <gml:meridianID>
                <gml:name codeSpace="urn:ogc:def:meridian:EPSG::">8901</gml:name>
              </gml:meridianID>
              <gml:greenwichLongitude>
                <gml:angle uom="urn:ogc:def:uom:EPSG::9102">0</gml:angle>
              </gml:greenwichLongitude>
            </gml:PrimeMeridian>
          </gml:usesPrimeMeridian>
          <gml:usesEllipsoid>
            <gml:Ellipsoid>
              <gml:ellipsoidName>WGS 84</gml:ellipsoidName>
              <gml:ellipsoidID>
                <gml:name codeSpace="urn:ogc:def:ellipsoid:EPSG::">7030</gml:name>
              </gml:ellipsoidID>
              <gml:semiMajorAxis uom="urn:ogc:def:uom:EPSG::9001">6378137</gml:semiMajorAxis>
              <gml:secondDefiningParameter>
                <gml:inverseFlattening uom="urn:ogc:def:uom:EPSG::9201">298.257223563</gml:inverseFlattening>
              </gml:secondDefiningParameter>
            </gml:Ellipsoid>
          </gml:usesEllipsoid>
        </gml:GeodeticDatum>
      </gml:usesGeodeticDatum>
    </gml:GeographicCRS>
  </gml:baseCRS>
  <gml:definedByConversion>
    <gml:Conversion>
      <gml:coordinateOperationName>Transverse_Mercator</gml:coordinateOperationName>
      <gml:usesMethod xlink:href="urn:ogc:def:method:EPSG::9807" />
      <gml:usesValue>
        <gml:value uom="urn:ogc:def:uom:EPSG::9102">0</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8801" />
      </gml:usesValue>
      <gml:usesValue>
        <gml:value uom="urn:ogc:def:uom:EPSG::9102">3</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8802" />
      </gml:usesValue>
      <gml:usesValue>
        <gml:value uom="urn:ogc:def:uom:EPSG::9001">0.9996</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8805" />
      </gml:usesValue>
      <gml:usesValue>
        <gml:value uom="urn:ogc:def:uom:EPSG::9001">500000</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8806" />
      </gml:usesValue>
      <gml:usesValue>
        <gml:value uom="urn:ogc:def:uom:EPSG::9001">0</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8807" />
      </gml:usesValue>
    </gml:Conversion>
  </gml:definedByConversion>
  <gml:usesCartesianCS>
    <gml:CartesianCS>
      <gml:csName>Cartesian</gml:csName>
      <gml:csID>
        <gml:name codeSpace="urn:ogc:def:cs:EPSG::">4400</gml:name>
      </gml:csID>
      <gml:usesAxis>
        <gml:CoordinateSystemAxis gml:uom="urn:ogc:def:uom:EPSG::9001">
          <gml:name>Easting</gml:name>
          <gml:axisID>
            <gml:name codeSpace="urn:ogc:def:axis:EPSG::">9906</gml:name>
          </gml:axisID>
          <gml:axisAbbrev>E</gml:axisAbbrev>
          <gml:axisDirection>east</gml:axisDirection>
        </gml:CoordinateSystemAxis>
      </gml:usesAxis>
      <gml:usesAxis>
        <gml:CoordinateSystemAxis gml:uom="urn:ogc:def:uom:EPSG::9001">
          <gml:name>Northing</gml:name>
          <gml:axisID>
            <gml:name codeSpace="urn:ogc:def:axis:EPSG::">9907</gml:name>
          </gml:axisID>
          <gml:axisAbbrev>N</gml:axisAbbrev>
          <gml:axisDirection>north</gml:axisDirection>
        </gml:CoordinateSystemAxis>
      </gml:usesAxis>
    </gml:CartesianCS>
  </gml:usesCartesianCS>
</gml:ProjectedCRS>
"""

    gdaltest.srs_wkt = """PROJCS["WGS 84 / UTM zone 31N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32631"]]"""

    srs = osr.SpatialReference()
    srs.ImportFromXML(gdaltest.srs_xml)

    got = srs.ExportToWkt()
    expected = gdaltest.srs_wkt

    assert got == expected


###############################################################################
# Test the osr.SpatialReference.ExportToXML() function.
#


def test_osr_xml_2():

    srs = osr.SpatialReference()
    srs.ImportFromWkt(gdaltest.srs_wkt)

    expected = gdaltest.srs_xml

    got = srs.ExportToXML()

    # Strip the gml:id tags
    got = re.sub(r' gml:id="[^"]*"', "", got)
    expected = re.sub(r' gml:id="[^"]*"', "", expected)

    assert got == expected


###############################################################################
# Test the osr.SpatialReference.ExportToXML() function.
#


def test_osr_xml_export_failure():

    srs = osr.SpatialReference()
    srs.ImportFromWkt("""PROJCRS["Africa_Albers_Equal_Area_Conic",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["Albers Equal Area",
        METHOD["Albers Equal Area",
            ID["EPSG",9822]],
        PARAMETER["Latitude of false origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8821]],
        PARAMETER["Longitude of false origin",25,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8822]],
        PARAMETER["Latitude of 1st standard parallel",20,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8823]],
        PARAMETER["Latitude of 2nd standard parallel",-23,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8824]],
        PARAMETER["Easting at false origin",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8826]],
        PARAMETER["Northing at false origin",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8827]]],
    CS[Cartesian,2],
        AXIS["easting",east,
            ORDER[1],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]],
        AXIS["northing",north,
            ORDER[2],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]]]""")

    with pytest.raises(
        Exception, match="Unhandled projection method Albers_Conic_Equal_Area"
    ):
        srs.ExportToXML()
