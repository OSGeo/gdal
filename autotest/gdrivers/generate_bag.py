#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Generate test files
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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


import h5py
import numpy as np

f = h5py.File('test_georef_metadata.bag','w')
bag_root = f.create_group("BAG_root")
bag_root.attrs["Bag Version"] = "2.0.0"
xml = """<?xml version="1.0" encoding="UTF-8"?>
<gmi:MI_Metadata xmlns:gmi="http://www.isotc211.org/2005/gmi" xmlns:gmd="http://www.isotc211.org/2005/gmd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:gco="http://www.isotc211.org/2005/gco" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:bag="http://www.opennavsurf.org/schema/bag">
  <gmd:fileIdentifier>
    <gco:CharacterString/>
  </gmd:fileIdentifier>
  <gmd:language>
    <gmd:LanguageCode codeList="http://www.loc.gov/standards/iso639-2/" codeListValue=""/>
  </gmd:language>
  <gmd:characterSet>
    <gmd:MD_CharacterSetCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_CharacterSetCode" codeListValue=""/>
  </gmd:characterSet>
  <gmd:hierarchyLevel>
    <gmd:MD_ScopeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_ScopeCode" codeListValue=""/>
  </gmd:hierarchyLevel>
  <gmd:contact>
    <gmd:CI_ResponsibleParty>
      <gmd:individualName>
        <gco:CharacterString>unknown</gco:CharacterString>
      </gmd:individualName>
      <gmd:positionName>
        <gco:CharacterString>unknown</gco:CharacterString>
      </gmd:positionName>
      <gmd:role>
        <gmd:CI_RoleCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#CI_RoleCode" codeListValue="author">author</gmd:CI_RoleCode>
      </gmd:role>
    </gmd:CI_ResponsibleParty>
  </gmd:contact>
  <gmd:dateStamp>
    <gco:Date>2018-08-08</gco:Date>
  </gmd:dateStamp>
  <gmd:metadataStandardName>
    <gco:CharacterString/>
  </gmd:metadataStandardName>
  <gmd:metadataStandardVersion>
    <gco:CharacterString/>
  </gmd:metadataStandardVersion>
  <gmd:spatialRepresentationInfo>
    <gmd:MD_Georectified>
      <gmd:numberOfDimensions>
        <gco:Integer>2</gco:Integer>
      </gmd:numberOfDimensions>
      <gmd:axisDimensionProperties>
        <gmd:MD_Dimension>
          <gmd:dimensionName>
            <gmd:MD_DimensionNameTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_DimensionNameTypeCode" codeListValue="row">row</gmd:MD_DimensionNameTypeCode>
          </gmd:dimensionName>
          <gmd:dimensionSize>
            <gco:Integer>4</gco:Integer>
          </gmd:dimensionSize>
          <gmd:resolution>
            <gco:Measure uom="m">32</gco:Measure>
          </gmd:resolution>
        </gmd:MD_Dimension>
      </gmd:axisDimensionProperties>
      <gmd:axisDimensionProperties>
        <gmd:MD_Dimension>
          <gmd:dimensionName>
            <gmd:MD_DimensionNameTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_DimensionNameTypeCode" codeListValue="column">column</gmd:MD_DimensionNameTypeCode>
          </gmd:dimensionName>
          <gmd:dimensionSize>
            <gco:Integer>6</gco:Integer>
          </gmd:dimensionSize>
          <gmd:resolution>
            <gco:Measure uom="m">30</gco:Measure>
          </gmd:resolution>
        </gmd:MD_Dimension>
      </gmd:axisDimensionProperties>
      <gmd:cellGeometry>
        <gmd:MD_CellGeometryCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_CellGeometryCode" codeListValue="point">point</gmd:MD_CellGeometryCode>
      </gmd:cellGeometry>
      <gmd:transformationParameterAvailability>
        <gco:Boolean>1</gco:Boolean>
      </gmd:transformationParameterAvailability>
      <gmd:checkPointAvailability>
        <gco:Boolean>0</gco:Boolean>
      </gmd:checkPointAvailability>
      <gmd:cornerPoints>
        <gml:Point gml:id="id1">
          <gml:coordinates decimal="." cs="," ts=" ">100.000000000000,500000.000000000000 250.000000000000,500096.000000000000</gml:coordinates>
        </gml:Point>
      </gmd:cornerPoints>
      <gmd:pointInPixel>
        <gmd:MD_PixelOrientationCode>center</gmd:MD_PixelOrientationCode>
      </gmd:pointInPixel>
    </gmd:MD_Georectified>
  </gmd:spatialRepresentationInfo>
  <gmd:referenceSystemInfo>
    <gmd:MD_ReferenceSystem>
      <gmd:referenceSystemIdentifier>
        <gmd:RS_Identifier>
          <gmd:code>
            <gco:CharacterString>PROJCS["NAD83 / UTM zone 10N",
    GEOGCS["NAD83",
        DATUM["North American Datum 1983",
            SPHEROID["GRS 1980",6378137,298.2572221010041,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Transverse_Mercator",
        AUTHORITY["EPSG","16010"]],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-123],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","26910"]]</gco:CharacterString>
          </gmd:code>
          <gmd:codeSpace>
            <gco:CharacterString>WKT</gco:CharacterString>
          </gmd:codeSpace>
        </gmd:RS_Identifier>
      </gmd:referenceSystemIdentifier>
    </gmd:MD_ReferenceSystem>
  </gmd:referenceSystemInfo>
  <gmd:referenceSystemInfo>
    <gmd:MD_ReferenceSystem>
      <gmd:referenceSystemIdentifier>
        <gmd:RS_Identifier>
          <gmd:code>
            <gco:CharacterString>VERT_CS["MLLW", VERT_DATUM["MLLW", 2000]]</gco:CharacterString>
          </gmd:code>
          <gmd:codeSpace>
            <gco:CharacterString>WKT</gco:CharacterString>
          </gmd:codeSpace>
        </gmd:RS_Identifier>
      </gmd:referenceSystemIdentifier>
    </gmd:MD_ReferenceSystem>
  </gmd:referenceSystemInfo>
  <gmd:identificationInfo>
    <bag:BAG_DataIdentification>
      <gmd:citation/>
      <gmd:abstract>
        <gco:CharacterString/>
      </gmd:abstract>
      <gmd:spatialRepresentationType>
        <gmd:MD_SpatialRepresentationTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_SpatialRepresentationTypeCode" codeListValue="grid">grid</gmd:MD_SpatialRepresentationTypeCode>
      </gmd:spatialRepresentationType>
      <gmd:language>
        <gmd:LanguageCode codeList="http://www.loc.gov/standards/iso639-2/" codeListValue="eng">eng</gmd:LanguageCode>
      </gmd:language>
      <gmd:characterSet>
        <gmd:MD_CharacterSetCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_CharacterSetCode" codeListValue=""/>
      </gmd:characterSet>
      <gmd:topicCategory>
        <gmd:MD_TopicCategoryCode>elevation</gmd:MD_TopicCategoryCode>
      </gmd:topicCategory>
      <gmd:extent>
        <gmd:EX_Extent>
          <gmd:geographicElement>
            <gmd:EX_GeographicBoundingBox>
              <gmd:westBoundLongitude>
                <gco:Decimal>0</gco:Decimal>
              </gmd:westBoundLongitude>
              <gmd:eastBoundLongitude>
                <gco:Decimal>0</gco:Decimal>
              </gmd:eastBoundLongitude>
              <gmd:southBoundLatitude>
                <gco:Decimal>0</gco:Decimal>
              </gmd:southBoundLatitude>
              <gmd:northBoundLatitude>
                <gco:Decimal>0</gco:Decimal>
              </gmd:northBoundLatitude>
            </gmd:EX_GeographicBoundingBox>
          </gmd:geographicElement>
        </gmd:EX_Extent>
      </gmd:extent>
      <bag:verticalUncertaintyType>
        <bag:BAG_VertUncertCode codeList="http://www.opennavsurf.org/schema/bag/bagCodelists.xml#BAG_VertUncertCode" codeListValue="unknown">unknown</bag:BAG_VertUncertCode>
      </bag:verticalUncertaintyType>
    </bag:BAG_DataIdentification>
  </gmd:identificationInfo>
  <gmd:dataQualityInfo>
    <gmd:DQ_DataQuality>
      <gmd:scope>
        <gmd:DQ_Scope>
          <gmd:level>
            <gmd:MD_ScopeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_ScopeCode" codeListValue="dataset">dataset</gmd:MD_ScopeCode>
          </gmd:level>
        </gmd:DQ_Scope>
      </gmd:scope>
      <gmd:lineage>
        <gmd:LI_Lineage>
          <gmd:processStep>
            <bag:BAG_ProcessStep>
              <gmd:description>
                <gco:CharacterString>Generated by bag_vr_create</gco:CharacterString>
              </gmd:description>
              <gmd:dateTime>
                <gco:DateTime>2018-08-08T12:34:56</gco:DateTime>
              </gmd:dateTime>
              <bag:trackingId>
                <gco:CharacterString/>
              </bag:trackingId>
            </bag:BAG_ProcessStep>
          </gmd:processStep>
        </gmd:LI_Lineage>
      </gmd:lineage>
    </gmd:DQ_DataQuality>
  </gmd:dataQualityInfo>
  <gmd:metadataConstraints>
    <gmd:MD_LegalConstraints>
      <gmd:useConstraints>
        <gmd:MD_RestrictionCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_RestrictionCode" codeListValue=""/>
      </gmd:useConstraints>
      <gmd:otherConstraints>
        <gco:CharacterString/>
      </gmd:otherConstraints>
    </gmd:MD_LegalConstraints>
  </gmd:metadataConstraints>
  <gmd:metadataConstraints>
    <gmd:MD_SecurityConstraints>
      <gmd:classification>
        <gmd:MD_ClassificationCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_ClassificationCode" codeListValue="unclassified">unclassified</gmd:MD_ClassificationCode>
      </gmd:classification>
      <gmd:userNote>
        <gco:CharacterString/>
      </gmd:userNote>
    </gmd:MD_SecurityConstraints>
  </gmd:metadataConstraints>
</gmi:MI_Metadata>
"""
metadata = bag_root.create_dataset("metadata", (len(xml),), data = [x for x in xml], dtype = 'S1')

elevation = bag_root.create_dataset("elevation", (4, 6), dtype = 'float32', fillvalue = 0)
data = np.array([i for i in range(24) ]).reshape(elevation.shape)
elevation[...] = data

uncertainty = bag_root.create_dataset("uncertainty", (4, 6), dtype = 'float32')

varres_metadata_struct_type = np.dtype([('index', 'I'), ('dimensions_x', 'I'), ('dimensions_y', 'I'), ('resolution_x', 'f4'), ('resolution_y', 'f4'), ('sw_corner_x', 'f4'), ('sw_corner_y', 'f4')])
varres_metadata = bag_root.create_dataset("varres_metadata", (4, 6), dtype = varres_metadata_struct_type)
varres_metadata.attrs["min_resolution_x"] = 1.0
varres_metadata.attrs["min_resolution_y"] = 1.0
varres_metadata.attrs["max_resolution_x"] = 8.0
varres_metadata.attrs["max_resolution_y"] = 8.0
data = np.array( [(0, 2, 2, 2, 2, 0, 0) for i in range(24)] , dtype = varres_metadata_struct_type).reshape(elevation.shape)
data[0][1] = np.array((4, 2, 2, 2, 2, 0, 0) , dtype = varres_metadata_struct_type)
varres_metadata[...] = data

varres_refinements_struct_type = np.dtype([('depth', 'f4'), ('depth_uncrt', 'f4')])
varres_refinements = bag_root.create_dataset("varres_refinements", (1, 8), dtype = varres_refinements_struct_type)
varres_refinements[...] = np.array([[0, 2, 3, 4, 5, 6, 7, 0]])

Georef_metadata = bag_root.create_group("Georef_metadata")

layer = Georef_metadata.create_group("layer_with_keys_values")

keys = layer.create_dataset("keys", (4, 6), dtype = 'uint32', chunks=(2,3))
keys[...] = np.array([np.arange(6) for i in range(4)]).reshape(elevation.shape)

varres_keys = layer.create_dataset("varres_keys", (1, 8), dtype = 'uint32')
varres_keys[...] = np.array([1, 1, 1, 0,   0, 1, 1, 1])

comp_type = np.dtype([('int', 'i'), ('str', np.str_, 6), ('float64', 'f8')])
values = layer.create_dataset("values", (6,), dtype = comp_type)
data = np.array([(i, "Val   ", i + 1.25) for i in range(6) ], dtype = comp_type)
values[...] = data

layer = Georef_metadata.create_group("layer_with_values_only")
values = layer.create_dataset("values", (2,), dtype = comp_type)
data = np.array([(i, "Val   ", i + 1.25) for i in range(2) ], dtype = comp_type)
values[...] = data

tracking_list_struct_type = np.dtype([('row', 'I'), ('col', 'I'), ('depth', 'f4'), ('uncertainty', 'f4'), ('track_code', 'B'), ('list_series', 'h')])
tracking_list = bag_root.create_dataset("tracking_list", (2,), dtype = tracking_list_struct_type)
tracking_list[...] = np.array([(0,1,2.5,3.5,4,5),(6,7,8.5,9.5,10,11)], dtype = tracking_list_struct_type)
