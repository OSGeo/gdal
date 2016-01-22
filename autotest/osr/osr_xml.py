#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OSR XML imoprt/export
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import osr

import re

###############################################################################
# Test the osr.SpatialReference.ImportFromXML() function.
#

def osr_xml_1():

    gdaltest.srs_xml = """<gml:ProjectedCRS gml:id="ogrcrs1">
  <gml:srsName>WGS 84 / UTM zone 31N</gml:srsName>
  <gml:srsID>
    <gml:name gml:codeSpace="urn:ogc:def:crs:EPSG::">32631</gml:name>
  </gml:srsID>
  <gml:baseCRS>
    <gml:GeographicCRS gml:id="ogrcrs2">
      <gml:srsName>WGS 84</gml:srsName>
      <gml:srsID>
        <gml:name gml:codeSpace="urn:ogc:def:crs:EPSG::">4326</gml:name>
      </gml:srsID>
      <gml:usesEllipsoidalCS>
        <gml:EllipsoidalCS gml:id="ogrcrs3">
          <gml:csName>ellipsoidal</gml:csName>
          <gml:csID>
            <gml:name gml:codeSpace="urn:ogc:def:cs:EPSG::">6402</gml:name>
          </gml:csID>
          <gml:usesAxis>
            <gml:CoordinateSystemAxis gml:id="ogrcrs4" gml:uom="urn:ogc:def:uom:EPSG::9102">
              <gml:name>Geodetic latitude</gml:name>
              <gml:axisID>
                <gml:name gml:codeSpace="urn:ogc:def:axis:EPSG::">9901</gml:name>
              </gml:axisID>
              <gml:axisAbbrev>Lat</gml:axisAbbrev>
              <gml:axisDirection>north</gml:axisDirection>
            </gml:CoordinateSystemAxis>
          </gml:usesAxis>
          <gml:usesAxis>
            <gml:CoordinateSystemAxis gml:id="ogrcrs5" gml:uom="urn:ogc:def:uom:EPSG::9102">
              <gml:name>Geodetic longitude</gml:name>
              <gml:axisID>
                <gml:name gml:codeSpace="urn:ogc:def:axis:EPSG::">9902</gml:name>
              </gml:axisID>
              <gml:axisAbbrev>Lon</gml:axisAbbrev>
              <gml:axisDirection>east</gml:axisDirection>
            </gml:CoordinateSystemAxis>
          </gml:usesAxis>
        </gml:EllipsoidalCS>
      </gml:usesEllipsoidalCS>
      <gml:usesGeodeticDatum>
        <gml:GeodeticDatum gml:id="ogrcrs6">
          <gml:datumName>WGS_1984</gml:datumName>
          <gml:datumID>
            <gml:name gml:codeSpace="urn:ogc:def:datum:EPSG::">6326</gml:name>
          </gml:datumID>
          <gml:usesPrimeMeridian>
            <gml:PrimeMeridian gml:id="ogrcrs7">
              <gml:meridianName>Greenwich</gml:meridianName>
              <gml:meridianID>
                <gml:name gml:codeSpace="urn:ogc:def:meridian:EPSG::">8901</gml:name>
              </gml:meridianID>
              <gml:greenwichLongitude>
                <gml:angle gml:uom="urn:ogc:def:uom:EPSG::9102">0</gml:angle>
              </gml:greenwichLongitude>
            </gml:PrimeMeridian>
          </gml:usesPrimeMeridian>
          <gml:usesEllipsoid>
            <gml:Ellipsoid gml:id="ogrcrs8">
              <gml:ellipsoidName>WGS 84</gml:ellipsoidName>
              <gml:ellipsoidID>
                <gml:name gml:codeSpace="urn:ogc:def:ellipsoid:EPSG::">7030</gml:name>
              </gml:ellipsoidID>
              <gml:semiMajorAxis gml:uom="urn:ogc:def:uom:EPSG::9001">6378137</gml:semiMajorAxis>
              <gml:secondDefiningParameter>
                <gml:inverseFlattening gml:uom="urn:ogc:def:uom:EPSG::9201">298.257223563</gml:inverseFlattening>
              </gml:secondDefiningParameter>
            </gml:Ellipsoid>
          </gml:usesEllipsoid>
        </gml:GeodeticDatum>
      </gml:usesGeodeticDatum>
    </gml:GeographicCRS>
  </gml:baseCRS>
  <gml:definedByConversion>
    <gml:Conversion gml:id="ogrcrs9">
      <gml:usesMethod xlink:href="urn:ogc:def:method:EPSG::9807" />
      <gml:usesParameterValue>
        <gml:value gml:uom="urn:ogc:def:uom:EPSG::9102">0</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8801" />
      </gml:usesParameterValue>
      <gml:usesParameterValue>
        <gml:value gml:uom="urn:ogc:def:uom:EPSG::9102">3</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8802" />
      </gml:usesParameterValue>
      <gml:usesParameterValue>
        <gml:value gml:uom="urn:ogc:def:uom:EPSG::9001">0.9996</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8805" />
      </gml:usesParameterValue>
      <gml:usesParameterValue>
        <gml:value gml:uom="urn:ogc:def:uom:EPSG::9001">500000</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8806" />
      </gml:usesParameterValue>
      <gml:usesParameterValue>
        <gml:value gml:uom="urn:ogc:def:uom:EPSG::9001">0</gml:value>
        <gml:valueOfParameter xlink:href="urn:ogc:def:parameter:EPSG::8807" />
      </gml:usesParameterValue>
    </gml:Conversion>
  </gml:definedByConversion>
  <gml:usesCartesianCS>
    <gml:CartesianCS gml:id="ogrcrs10">
      <gml:csName>Cartesian</gml:csName>
      <gml:csID>
        <gml:name gml:codeSpace="urn:ogc:def:cs:EPSG::">4400</gml:name>
      </gml:csID>
      <gml:usesAxis>
        <gml:CoordinateSystemAxis gml:id="ogrcrs11" gml:uom="urn:ogc:def:uom:EPSG::9001">
          <gml:name>Easting</gml:name>
          <gml:axisID>
            <gml:name gml:codeSpace="urn:ogc:def:axis:EPSG::">9906</gml:name>
          </gml:axisID>
          <gml:axisAbbrev>E</gml:axisAbbrev>
          <gml:axisDirection>east</gml:axisDirection>
        </gml:CoordinateSystemAxis>
      </gml:usesAxis>
      <gml:usesAxis>
        <gml:CoordinateSystemAxis gml:id="ogrcrs12" gml:uom="urn:ogc:def:uom:EPSG::9001">
          <gml:name>Northing</gml:name>
          <gml:axisID>
            <gml:name gml:codeSpace="urn:ogc:def:axis:EPSG::">9907</gml:name>
          </gml:axisID>
          <gml:axisAbbrev>N</gml:axisAbbrev>
          <gml:axisDirection>north</gml:axisDirection>
        </gml:CoordinateSystemAxis>
      </gml:usesAxis>
    </gml:CartesianCS>
  </gml:usesCartesianCS>
</gml:ProjectedCRS>
"""

    gdaltest.srs_wkt = """PROJCS["WGS 84 / UTM zone 31N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1],AUTHORITY["EPSG","32631"]]"""

    srs = osr.SpatialReference()
    srs.ImportFromXML(gdaltest.srs_xml)

    got = srs.ExportToWkt()
    expected = gdaltest.srs_wkt

    if got != expected:
        print(got)
        return 'fail'

    return 'success'

###############################################################################
# Test the osr.SpatialReference.ExportToXML() function.
#

def osr_xml_2():

    srs = osr.SpatialReference()
    srs.ImportFromWkt(gdaltest.srs_wkt)

    expected = gdaltest.srs_xml

    got = srs.ExportToXML()

    # Strip the gml:id tags
    got = re.sub(r' gml:id="[^"]*"', '', got, 0 )
    expected = re.sub(r' gml:id="[^"]*"', '', expected, 0 )

    if got != expected:
        print(got)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    osr_xml_1,
    osr_xml_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_xml' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

