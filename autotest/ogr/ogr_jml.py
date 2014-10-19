#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JML driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot org>
# 
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot org>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

def ogr_jml_init():

    try:
        ds = ogr.Open( 'data/test.jml' )
    except:
        ds = None

    if ds is None:
        gdaltest.jml_read_support = 0
    else:
        gdaltest.jml_read_support = 1
        ds = None

    return 'success'

###############################################################################
# Test reading

def ogr_jml_1():

    if not gdaltest.jml_read_support:
        return 'skip'

    ds = ogr.Open( 'data/test.jml' )
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayer(1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.TestCapability(ogr.ODsCCreateLayer) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.TestCapability(ogr.ODsCDeleteLayer) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayer(0)
    fields = [ ('first_property', ogr.OFTString),
               ('another_property', ogr.OFTString),
               ('objectAttr', ogr.OFTString),
               ('attr2', ogr.OFTString),
               ('attr3', ogr.OFTString),
               ('int', ogr.OFTInteger),
               ('double', ogr.OFTReal),
               ('date', ogr.OFTDateTime),
               ('datetime', ogr.OFTDateTime),
               ('R_G_B', ogr.OFTString),
               ('not_ignored', ogr.OFTString) ]
    if lyr.GetLayerDefn().GetFieldCount() != len(fields):
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(len(fields)):
        field_defn = lyr.GetLayerDefn().GetFieldDefn(i)
        if field_defn.GetName() != fields[i][0] or field_defn.GetType() != fields[i][1]:
            print(i)
            gdaltest.post_reason('fail')
            return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('first_property') != 'even' or \
       feat.GetField('another_property') != 'rouault' or \
       feat.GetField('objectAttr') != 'foo' or \
       feat.GetField('attr2') != 'bar' or \
       feat.GetField('attr3') != 'baz' or \
       feat.GetField('int') != 123 or \
       feat.GetField('double') != 1.23 or \
       feat.GetFieldAsString('date') != '2014/10/18 00:00:00' or \
       feat.GetFieldAsString('datetime') != '2014/10/18 21:36:45' or \
       feat.GetField('R_G_B') != '0000FF' or \
       feat.IsFieldSet('not_ignored') or \
       feat.GetStyleString() != 'BRUSH(fc:#0000FF)' or \
       feat.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 10,10 10,10 0,0 0))':
            feat.DumpReadable()
            gdaltest.post_reason('fail')
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('datetime') != '2014/10/18 21:36:45+02' or \
       feat.GetField('R_G_B') != 'FF00FF' or \
       feat.GetStyleString() != 'PEN(c:#FF00FF)' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (-1 -1)':
            feat.DumpReadable()
            gdaltest.post_reason('fail')
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
            feat.DumpReadable()
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
# Test creating a file

def ogr_jml_2():
    
    # Invalid filename
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('JML').CreateDataSource('/foo/ogr_jml.jml')
    gdal.PopErrorHandler()
    ds = None
    
    # Empty layer
    ds = ogr.GetDriverByName('JML').CreateDataSource('/vsimem/ogr_jml.jml')
    lyr = ds.CreateLayer('foo')
    lyr.ResetReading()
    lyr.GetNextFeature()
    ds = None
    
    f = gdal.VSIFOpenL('/vsimem/ogr_jml.jml', 'rb')
    data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)
    
    if data != """<?xml version='1.0' encoding='UTF-8'?>
<JCSDataFile xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2000/10/XMLSchema-instance" >
<JCSGMLInputTemplate>
<CollectionElement>featureCollection</CollectionElement>
<FeatureElement>feature</FeatureElement>
<GeometryElement>geometry</GeometryElement>
<ColumnDefinitions>
</ColumnDefinitions>
</JCSGMLInputTemplate>
<featureCollection>
</featureCollection>
</JCSDataFile>
""":
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.Unlink('/vsimem/ogr_jml.jml')
    
    # Test all data types
    ds = ogr.GetDriverByName('JML').CreateDataSource('/vsimem/ogr_jml.jml')
    lyr = ds.CreateLayer('foo')
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('double', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('date', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('datetime', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('datetime2', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('time_as_str', ogr.OFTTime))
    if lyr.TestCapability(ogr.OLCCreateField) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCSequentialWrite) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCRandomWrite) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # empty feature
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    
    if lyr.TestCapability(ogr.OLCCreateField) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.CreateField(ogr.FieldDefn('that_wont_work')) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('str', 'fo<o')
    f.SetField('int', 1)
    f.SetField('double', 1.23)
    f.SetField('date', '2014-10-19')
    f.SetField('datetime', '2014-10-19 12:34:56')
    f.SetField('datetime2', '2014-10-19 12:34:56+02')
    f.SetField('time_as_str', '12:34:56')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    f.SetStyleString('PEN(c:#112233)')
    lyr.CreateFeature(f)
    
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 10,10 10,10 0,0 0))'))
    f.SetStyleString('BRUSH(fc:#112233)')
    lyr.CreateFeature(f)

    ds = None
    
    f = gdal.VSIFOpenL('/vsimem/ogr_jml.jml', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    
    if data != """<?xml version='1.0' encoding='UTF-8'?>
<JCSDataFile xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2000/10/XMLSchema-instance" >
<JCSGMLInputTemplate>
<CollectionElement>featureCollection</CollectionElement>
<FeatureElement>feature</FeatureElement>
<GeometryElement>geometry</GeometryElement>
<ColumnDefinitions>
     <column>
          <name>str</name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>int</name>
          <type>INTEGER</type>
          <valueElement elementName="property" attributeName="name" attributeValue="int"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>double</name>
          <type>DOUBLE</type>
          <valueElement elementName="property" attributeName="name" attributeValue="double"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>date</name>
          <type>DATE</type>
          <valueElement elementName="property" attributeName="name" attributeValue="date"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>datetime</name>
          <type>DATE</type>
          <valueElement elementName="property" attributeName="name" attributeValue="datetime"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>datetime2</name>
          <type>DATE</type>
          <valueElement elementName="property" attributeName="name" attributeValue="datetime2"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>time_as_str</name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" attributeValue="time_as_str"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>R_G_B</name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" attributeValue="R_G_B"/>
          <valueLocation position="body"/>
     </column>
</ColumnDefinitions>
</JCSGMLInputTemplate>
<featureCollection>
     <feature>
          <geometry>
                <gml:MultiGeometry></gml:MultiGeometry>
          </geometry>
          <property name="str"></property>
          <property name="int"></property>
          <property name="double"></property>
          <property name="date"></property>
          <property name="datetime"></property>
          <property name="datetime2"></property>
          <property name="time_as_str"></property>
          <property name="R_G_B"></property>
     </feature>
     <feature>
          <geometry>
                <gml:Point><gml:coordinates>1,2</gml:coordinates></gml:Point>
          </geometry>
          <property name="str">fo&lt;o</property>
          <property name="int">1</property>
          <property name="double">1.23</property>
          <property name="date">2014/10/19</property>
          <property name="datetime">2014-10-19T12:34:56</property>
          <property name="datetime2">2014-10-19T12:34:56.000+0200</property>
          <property name="time_as_str">12:34:56</property>
          <property name="R_G_B">112233</property>
     </feature>
     <feature>
          <geometry>
                <gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:coordinates>0,0 0,10 10,10 10,0 0,0</gml:coordinates></gml:LinearRing></gml:outerBoundaryIs></gml:Polygon>
          </geometry>
          <property name="str"></property>
          <property name="int"></property>
          <property name="double"></property>
          <property name="date"></property>
          <property name="datetime"></property>
          <property name="datetime2"></property>
          <property name="time_as_str"></property>
          <property name="R_G_B">112233</property>
     </feature>
</featureCollection>
</JCSDataFile>
""":
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    
    gdal.Unlink('/vsimem/ogr_jml.jml')

    # Test with an explicit R_G_B field
    ds = ogr.GetDriverByName('JML').CreateDataSource('/vsimem/ogr_jml.jml')
    lyr = ds.CreateLayer('foo')
    lyr.CreateField(ogr.FieldDefn('R_G_B', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('R_G_B', '112233')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)
    
    # Test that R_G_B is not overriden by feature style
    f.SetField('R_G_B', '445566')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    f.SetStyleString('PEN(c:#778899)')
    lyr.CreateFeature(f)
    
    ds = None
    
    f = gdal.VSIFOpenL('/vsimem/ogr_jml.jml', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    
    if data.find('112233') < 0 or data.find('445566') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    
    gdal.Unlink('/vsimem/ogr_jml.jml')

    # Test CREATE_R_G_B_FIELD=NO
    ds = ogr.GetDriverByName('JML').CreateDataSource('/vsimem/ogr_jml.jml')
    lyr = ds.CreateLayer('foo', options = ['CREATE_R_G_B_FIELD=NO'] )
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None
    
    f = gdal.VSIFOpenL('/vsimem/ogr_jml.jml', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    
    if data.find('R_G_B') >= 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    
    gdal.Unlink('/vsimem/ogr_jml.jml')

    # Test CREATE_OGR_STYLE_FIELD=YES
    ds = ogr.GetDriverByName('JML').CreateDataSource('/vsimem/ogr_jml.jml')
    lyr = ds.CreateLayer('foo', options = ['CREATE_OGR_STYLE_FIELD=YES'] )
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetStyleString('PEN(c:#445566)')
    lyr.CreateFeature(f)
    ds = None
    
    f = gdal.VSIFOpenL('/vsimem/ogr_jml.jml', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    
    if data.find('OGR_STYLE') < 0 or data.find('PEN(c:#445566)') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    
    gdal.Unlink('/vsimem/ogr_jml.jml')

    # Test CREATE_OGR_STYLE_FIELD=YES with a R_G_B field
    ds = ogr.GetDriverByName('JML').CreateDataSource('/vsimem/ogr_jml.jml')
    lyr = ds.CreateLayer('foo', options = ['CREATE_OGR_STYLE_FIELD=YES'] )
    lyr.CreateField(ogr.FieldDefn('R_G_B', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('R_G_B', '112233')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    f.SetStyleString('PEN(c:#445566)')
    lyr.CreateFeature(f)
    ds = None
    
    f = gdal.VSIFOpenL('/vsimem/ogr_jml.jml', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    
    if data.find('OGR_STYLE') < 0 or data.find('PEN(c:#445566)') < 0 or data.find('112233') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    
    gdal.Unlink('/vsimem/ogr_jml.jml')

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_jml_3():

    if not gdaltest.jml_read_support:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.jml')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test a few error cases

def ogr_jml_4():

    if not gdaltest.jml_read_support:
        return 'skip'

    # Missing CollectionElement, FeatureElement or GeometryElement
    gdal.FileFromMemBuffer('/vsimem/ogr_jml.jml',"""<?xml version='1.0' encoding='UTF-8'?>
<JCSDataFile xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2000/10/XMLSchema-instance" >
<JCSGMLInputTemplate>
<MISSING_CollectionElement>featureCollection</MISSING_CollectionElement>
<MISSING_FeatureElement>feature</MISSING_FeatureElement>
<MISSING_GeometryElement>geometry</MISSING_GeometryElement>
<ColumnDefinitions>
</ColumnDefinitions>
</JCSGMLInputTemplate>
<featureCollection>
</featureCollection>
</JCSDataFile>""")

    ds = ogr.Open('/vsimem/ogr_jml.jml')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # XML malformed in JCSGMLInputTemplate
    gdal.FileFromMemBuffer('/vsimem/ogr_jml.jml',"""<?xml version='1.0' encoding='UTF-8'?>
<JCSDataFile xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2000/10/XMLSchema-instance" >
<JCSGMLInputTemplate>
<CollectionElement>featureCollection</CollectionElement>
<FeatureElement>feature</FeatureElement>
<GeometryElement>geometry</GeometryElement>
<ColumnDefinitions>
</INVALID_ColumnDefinitions>
</JCSGMLInputTemplate>
<featureCollection>
</featureCollection>
</JCSDataFile>""")

    ds = ogr.Open('/vsimem/ogr_jml.jml')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    # XML malformed in featureCollection
    gdal.FileFromMemBuffer('/vsimem/ogr_jml.jml',"""<?xml version='1.0' encoding='UTF-8'?>
<JCSDataFile xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2000/10/XMLSchema-instance" >
<JCSGMLInputTemplate>
<CollectionElement>featureCollection</CollectionElement>
<FeatureElement>feature</FeatureElement>
<GeometryElement>geometry</GeometryElement>
<ColumnDefinitions>
     <column>
          <name>str1</name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
</ColumnDefinitions>
</JCSGMLInputTemplate>
<featureCollection>
</INVALID_featureCollection>
</JCSDataFile>""")

    ds = ogr.Open('/vsimem/ogr_jml.jml')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    field_count = lyr.GetLayerDefn().GetFieldCount()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    # XML malformed in featureCollection
    gdal.FileFromMemBuffer('/vsimem/ogr_jml.jml',"""<?xml version='1.0' encoding='UTF-8'?>
<JCSDataFile xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2000/10/XMLSchema-instance" >
<JCSGMLInputTemplate>
<CollectionElement>featureCollection</CollectionElement>
<FeatureElement>feature</FeatureElement>
<GeometryElement>geometry</GeometryElement>
<ColumnDefinitions>
     <column>
          <name>str1</name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
</ColumnDefinitions>
</JCSGMLInputTemplate>
<featureCollection>
<feature>
</featureCollection>
</JCSDataFile>""")

    ds = ogr.Open('/vsimem/ogr_jml.jml')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    field_count = lyr.GetLayerDefn().GetFieldCount()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    # Invalid column definitions
    gdal.FileFromMemBuffer('/vsimem/ogr_jml.jml',"""<?xml version='1.0' encoding='UTF-8'?>
<JCSDataFile xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2000/10/XMLSchema-instance" >
<JCSGMLInputTemplate>
<CollectionElement>featureCollection</CollectionElement>
<FeatureElement>feature</FeatureElement>
<GeometryElement>geometry</GeometryElement>
<ColumnDefinitions>
     <column>
          <MISSING_name>str1</MISSING_name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>str2</name>
          <MISSING_type>STRING</MISSING_type>
          <valueElement elementName="property" attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>str3</name>
          <type>STRING</type>
          <MISSING_valueElement elementName="property" attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>str4</name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" attributeValue="str"/>
          <MISSING_valueLocation position="body"/>
     </column>
     <column>
          <name>str5</name>
          <type>STRING</type>
          <valueElement elementName="property" attributeName="name" MISSING_attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>str6</name>
          <type>STRING</type>
          <valueElement elementName="property" MISSING_attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
     <column>
          <name>str7</name>
          <type>STRING</type>
          <valueElement elementName="property" MISSING_attributeName="name"/>
          <valueLocation position="attribute"/>
     </column>
     <column>
          <name>str8</name>
          <type>STRING</type>
          <valueElement MISSING_elementName="property" attributeName="name" attributeValue="str"/>
          <valueLocation position="body"/>
     </column>
</ColumnDefinitions>
</JCSGMLInputTemplate>
<featureCollection>
<feature> <MISSING_geometry/> </feature>
</featureCollection>
</JCSDataFile>""")

    ds = ogr.Open('/vsimem/ogr_jml.jml')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    ds = None

    return 'success'
###############################################################################
# 

def ogr_jml_cleanup():
    
    gdal.Unlink('/vsimem/ogr_jml.jml')

    return 'success'

gdaltest_list = [ 
    ogr_jml_init,
    ogr_jml_1,
    ogr_jml_2,
    ogr_jml_3,
    ogr_jml_4,
    ogr_jml_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_jml' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

