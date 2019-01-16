#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  CSW driver testing.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

import gdaltest
from osgeo import ogr
from osgeo import gdal

###############################################################################
# Test underlying OGR drivers
#

pytestmark = pytest.mark.require_driver('CSW')


@pytest.fixture(autouse=True, scope='module')
def ogr_csw_init():
    gdaltest.csw_drv = ogr.GetDriverByName('CSW')

    gml_ds = ogr.Open('data/ionic_wfs.gml')
    if gml_ds is None:
        gdaltest.csw_drv = None
        if gdal.GetLastErrorMsg().find('Xerces') != -1:
            pytest.skip()
        pytest.skip('failed to open test file.')

    
###############################################################################
# Test reading a pyCSW server


@pytest.mark.skip()
def test_ogr_csw_pycsw():
    ds = ogr.Open('CSW:http://catalog.data.gov/csw')
    if ds is None:
        if gdaltest.gdalurlopen('http://catalog.data.gov/csw') is None:
            pytest.skip('cannot open URL')
        pytest.skip('did not managed to open CSW datastore')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None, 'did not get expected layer name'

###############################################################################


def test_ogr_csw_vsimem_fail_because_not_enabled():
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    assert ds is None


###############################################################################
def test_ogr_csw_vsimem_fail_because_no_get_capabilities():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################


def test_ogr_csw_vsimem_fail_because_empty_response():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Empty content returned by server') >= 0

###############################################################################


def test_ogr_csw_vsimem_fail_because_no_CSW_Capabilities():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '<foo/>')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot find Capabilities.version') >= 0

###############################################################################


def test_ogr_csw_vsimem_fail_because_exception():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '<ServiceExceptionReport/>')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Error returned by server : <ServiceExceptionReport/>') >= 0

###############################################################################


def test_ogr_csw_vsimem_fail_because_invalid_xml_capabilities():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '<invalid_xml')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Invalid XML content : <invalid_xml') >= 0

###############################################################################


def test_ogr_csw_vsimem_fail_because_missing_version():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           """<Capabilities>
</Capabilities>
""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot find Capabilities.version') >= 0

###############################################################################


def test_ogr_csw_vsimem_csw_minimal_instance():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    # Invalid response, but enough for use
    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           """
<Capabilities version="2.0.2"/>
""")
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    assert ds is not None
    ds.TestCapability('foo')
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(-1) is None and ds.GetLayer(1) is None

    lyr = ds.GetLayer(0)
    lyr.TestCapability('foo')
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 0

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Empty content returned by server') >= 0

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<invalid_xml""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Error: cannot parse') >= 0

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<dummy_xml/>""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Error: cannot parse') >= 0

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<ServiceExceptionReport/>""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Error returned by server') >= 0

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="3" numberOfRecordsMatched="2" numberOfRecordsReturned="3" recordSchema="http://www.opengis.net/cat/csw/2.0.2" elementSet="full">
    <csw:Record>
      <dc:identifier>an_identifier</dc:identifier>
      <dc:identifier>another_identifier</dc:identifier>
      <dc:title>a_title</dc:title>
      <dc:type>dataset</dc:type>
      <dc:subject>a_subject</dc:subject>
      <dc:subject>another_subject</dc:subject>
      <dct:references scheme="None">http://foo/</dct:references>
      <dct:references scheme="None">http://bar/</dct:references>
      <dct:modified>2015-04-27</dct:modified>
      <dct:abstract>an_abstract</dct:abstract>
      <dc:date>2015-04-27</dc:date>
      <dc:language>eng</dc:language>
      <dc:format>a_format</dc:format>
      <dc:format>another_format</dc:format>
      <ows:BoundingBox crs="urn:x-ogc:def:crs:EPSG:6.11:4326" dimensions="2">
        <ows:LowerCorner>-90 -180</ows:LowerCorner>
        <ows:UpperCorner>90 180</ows:UpperCorner>
      </ows:BoundingBox>
    </csw:Record>
    <csw:Record>
    </csw:Record>
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    if f['identifier'] != 'an_identifier' or f['other_identifiers'] != ['another_identifier'] or \
       f['subject'] != 'a_subject' or f['other_subjects'] != ['another_subject'] or \
       f['references'] != 'http://foo/' or f['other_references'] != ['http://bar/'] or \
       f['format'] != 'a_format' or f['other_formats'] != ['another_format'] or \
       f['boundingbox'].ExportToWkt() != 'POLYGON ((-180 -90,-180 90,180 90,180 -90,-180 -90))':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    assert f is not None
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 2

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="3" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="3" numberOfRecordsReturned="1" recordSchema="http://www.opengis.net/cat/csw/2.0.2" elementSet="full">
    <csw:Record>
    </csw:Record>
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")
    f = lyr.GetNextFeature()
    assert f is not None
    f = lyr.GetNextFeature()
    assert f is not None

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="4" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="3" numberOfRecordsReturned="0" recordSchema="http://www.opengis.net/cat/csw/2.0.2" elementSet="full">
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")
    f = lyr.GetNextFeature()
    assert f is None

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 3, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<dummy_xml/>""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 3, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<invalid_xml>""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 3, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<ServiceExceptionReport/>""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 3, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="200" numberOfRecordsReturned="0" recordSchema="http://www.opengis.net/cat/csw/2.0.2" elementSet="full">
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")
    fc = lyr.GetFeatureCount()
    assert fc == 200

    lyr.SetAttributeFilter("identifier = 'an_identifier'")
    lyr.SetSpatialFilterRect(-180, -90, 180, 90)
    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName><csw:Constraint version="1.1.0"><ogc:Filter><ogc:And><ogc:BBOX><ogc:PropertyName>ows:BoundingBox</ogc:PropertyName><gml:Envelope srsName="urn:ogc:def:crs:EPSG::4326"><gml:lowerCorner>-90 -180</gml:lowerCorner><gml:upperCorner>90 180</gml:upperCorner></gml:Envelope></ogc:BBOX><ogc:PropertyIsEqualTo><ogc:PropertyName>dc:identifier</ogc:PropertyName><ogc:Literal>an_identifier</ogc:Literal></ogc:PropertyIsEqualTo></ogc:And></ogc:Filter></csw:Constraint></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="3" numberOfRecordsReturned="1" recordSchema="http://www.opengis.net/cat/csw/2.0.2" elementSet="full">
    <csw:Record>
    </csw:Record>
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")
    f = lyr.GetNextFeature()
    assert f is not None

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName><csw:Constraint version="1.1.0"><ogc:Filter><ogc:And><ogc:BBOX><ogc:PropertyName>ows:BoundingBox</ogc:PropertyName><gml:Envelope srsName="urn:ogc:def:crs:EPSG::4326"><gml:lowerCorner>-90 -180</gml:lowerCorner><gml:upperCorner>90 180</gml:upperCorner></gml:Envelope></ogc:BBOX><ogc:PropertyIsEqualTo><ogc:PropertyName>dc:identifier</ogc:PropertyName><ogc:Literal>an_identifier</ogc:Literal></ogc:PropertyIsEqualTo></ogc:And></ogc:Filter></csw:Constraint></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="300" numberOfRecordsReturned="0" recordSchema="http://www.opengis.net/cat/csw/2.0.2" elementSet="full">
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")
    fc = lyr.GetFeatureCount()
    assert fc == 300

    lyr.SetAttributeFilter("identifier = 'an_identifier' AND " +
                           "references = 'http://foo/' AND " +
                           "anytext LIKE '%%foo%%' AND " +
                           "other_identifiers = '' AND " +
                           "other_subjects = '' AND " +
                           "other_formats = '' AND " +
                           "other_references = '' AND " +
                           "ST_Intersects(boundingbox, ST_MakeEnvelope(2,49,2,49,4326))")
    lyr.SetAttributeFilter(None)
    lyr.SetSpatialFilter(None)

###############################################################################


def test_ogr_csw_vsimem_csw_output_schema_csw():
    ds = gdal.OpenEx('CSW:/vsimem/csw_endpoint', open_options=['OUTPUT_SCHEMA=CSW'])
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" outputSchema="http://www.opengis.net/cat/csw/2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<invalid_xml
""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" outputSchema="http://www.opengis.net/cat/csw/2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<csw:GetRecordsResponse/>""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" outputSchema="http://www.opengis.net/cat/csw/2.0.2" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="1" numberOfRecordsReturned="1" recordSchema="http://www.isotc211.org/2005/gmd" elementSet="full">
    <csw:Record>
    <!-- lots of missing stuff -->
        <ows:BoundingBox crs="urn:x-ogc:def:crs:EPSG:6.11:4326" dimensions="2">
            <ows:LowerCorner>-90 -180</ows:LowerCorner>
            <ows:UpperCorner>90 180</ows:UpperCorner>
        </ows:BoundingBox>
    <!-- lots of missing stuff -->
    </csw:Record>
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f['raw_xml'].find('<csw:Record') != 0 or \
       f['boundingbox'].ExportToWkt() != 'POLYGON ((-180 -90,-180 90,180 90,180 -90,-180 -90))':
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_csw_vsimem_csw_output_schema_gmd():
    ds = gdal.OpenEx('CSW:/vsimem/csw_endpoint', open_options=['OUTPUT_SCHEMA=GMD'])
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" outputSchema="http://www.isotc211.org/2005/gmd" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="1" numberOfRecordsReturned="1" recordSchema="http://www.isotc211.org/2005/gmd" elementSet="full">
    <gmd:MD_Metadata xmlns:gmd="http://www.isotc211.org/2005/gmd" xmlns:gsr="http://www.isotc211.org/2005/gsr" xmlns:gss="http://www.isotc211.org/2005/gss" xmlns:gts="http://www.isotc211.org/2005/gts" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:gmx="http://www.isotc211.org/2005/gmx" xmlns:geonet="http://www.fao.org/geonetwork" xsi:schemaLocation="http://www.isotc211.org/2005/gmd http://www.isotc211.org/2005/gmd/gmd.xsd">
    <!-- lots of missing stuff -->
        <gmd:extent><gmd:EX_Extent><gmd:geographicElement><gmd:EX_GeographicBoundingBox>
            <gmd:westBoundLongitude><gco:Decimal>-180</gco:Decimal></gmd:westBoundLongitude>
            <gmd:eastBoundLongitude><gco:Decimal>180</gco:Decimal></gmd:eastBoundLongitude>
            <gmd:southBoundLatitude><gco:Decimal>-90</gco:Decimal></gmd:southBoundLatitude>
            <gmd:northBoundLatitude><gco:Decimal>90</gco:Decimal></gmd:northBoundLatitude>
        </gmd:EX_GeographicBoundingBox></gmd:geographicElement></gmd:EX_Extent></gmd:extent>
    <!-- lots of missing stuff -->
    </gmd:MD_Metadata>
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")

    f = lyr.GetNextFeature()
    if f['raw_xml'].find('<gmd:MD_Metadata') != 0 or \
       f['boundingbox'].ExportToWkt() != 'POLYGON ((-180 -90,-180 90,180 90,180 -90,-180 -90))':
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_csw_vsimem_csw_output_schema_fgdc():
    ds = gdal.OpenEx('CSW:/vsimem/csw_endpoint', open_options=['OUTPUT_SCHEMA=http://www.opengis.net/cat/csw/csdgm'])
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" outputSchema="http://www.opengis.net/cat/csw/csdgm" startPosition="1" maxRecords="500" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
                           """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<csw:GetRecordsResponse
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dct="http://purl.org/dc/terms/"
    xmlns:ows="http://www.opengis.net/ows"
    xmlns:csw="http://www.opengis.net/cat/csw/2.0.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    version="2.0.2"
    xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd">
  <csw:SearchStatus timestamp="2015-04-27T00:46:35Z"/>
  <csw:SearchResults nextRecord="0" numberOfRecordsMatched="1" numberOfRecordsReturned="1" recordSchema="http://www.opengis.net/cat/csw/csdgm" elementSet="full">
    <metadata xsi:noNamespaceSchemaLocation="http://www.fgdc.gov/metadata/fgdc-std-001-1998.xsd">
    <!-- lots of missing stuff -->
        <spdom>
            <bounding>
                <westbc>-180</westbc>
                <eastbc>180</eastbc>
                <northbc>90</northbc>
                <southbc>-90</southbc>
            </bounding>
        </spdom>
    <!-- lots of missing stuff -->
    </metadata>
  </csw:SearchResults>
</csw:GetRecordsResponse>
""")

    f = lyr.GetNextFeature()
    if f['raw_xml'].find('<metadata') != 0 or \
       f['boundingbox'].ExportToWkt() != 'POLYGON ((-180 -90,-180 90,180 90,180 -90,-180 -90))':
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_csw_vsimem_cleanup():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)

    for f in gdal.ReadDir('/vsimem/'):
        gdal.Unlink('/vsimem/' + f)

    


