#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

import gdaltest
from osgeo import ogr
from osgeo import gdal

###############################################################################
# Test underlying OGR drivers
#

def ogr_csw_init():

    gdaltest.csw_drv = None

    try:
        gdaltest.csw_drv = ogr.GetDriverByName('CSW')
    except:
        pass
        
    if gdaltest.csw_drv is None:
        return 'skip'

    try:
        gml_ds = ogr.Open( 'data/ionic_wfs.gml' )
    except:
        gml_ds = None

    if gml_ds is None:
        gdaltest.csw_drv = None
        if gdal.GetLastErrorMsg().find('Xerces') != -1:
            return 'skip'
        else:
            gdaltest.post_reason( 'failed to open test file.' )
            return 'skip'

    return 'success'

###############################################################################
# Test reading a pyCSW server

def ogr_csw_pycsw():
    if gdaltest.csw_drv is None:
        return 'skip'

    ds = ogr.Open('CSW:http://catalog.data.gov/csw')
    if ds is None:
        if gdaltest.gdalurlopen('http://catalog.data.gov/csw') is None:
            print('cannot open URL')
            return 'skip'
        gdaltest.post_reason('did not managed to open CSW datastore')
        return 'skip'
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('did not get expected layer name')
        return 'fail'

    return 'success'

###############################################################################
def ogr_csw_vsimem_fail_because_not_enabled():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    if ds is not None:
        return 'fail'

    return 'success'


###############################################################################
def ogr_csw_vsimem_fail_because_no_get_capabilities():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
def ogr_csw_vsimem_fail_because_empty_response():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Empty content returned by server') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def ogr_csw_vsimem_fail_because_no_CSW_Capabilities():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '<foo/>')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Cannot find Capabilities.version') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def ogr_csw_vsimem_fail_because_exception():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '<ServiceExceptionReport/>')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Error returned by server : <ServiceExceptionReport/>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'
    
###############################################################################
def ogr_csw_vsimem_fail_because_invalid_xml_capabilities():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
                           '<invalid_xml')
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Invalid XML content : <invalid_xml') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'
    
###############################################################################
def ogr_csw_vsimem_fail_because_missing_version():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
"""<Capabilities>
</Capabilities>
""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('Cannot find Capabilities.version') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def ogr_csw_vsimem_csw_minimal_instance():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    # Invalid response, but enough for use
    gdal.FileFromMemBuffer('/vsimem/csw_endpoint?SERVICE=CSW&REQUEST=GetCapabilities',
"""
<Capabilities version="2.0.2"/>
""")
    ds = ogr.Open('CSW:/vsimem/csw_endpoint')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.TestCapability('foo')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayer(-1) is not None or ds.GetLayer(1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(0)
    lyr.TestCapability('foo')
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None or gdal.GetLastErrorMsg().find('Empty content returned by server') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""<invalid_xml""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None or gdal.GetLastErrorMsg().find('Error: cannot parse') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""<dummy_xml/>""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None or gdal.GetLastErrorMsg().find('Error: cannot parse') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""<ServiceExceptionReport/>""")
    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None or gdal.GetLastErrorMsg().find('Error returned by server') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
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
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if f['identifier'] != 'an_identifier' or f['other_identifiers'] != ['another_identifier'] or \
       f['subject'] != 'a_subject' or f['other_subjects'] != ['another_subject'] or \
       f['references'] != 'http://foo/' or f['other_references'] != ['http://bar/'] or \
       f['format'] != 'a_format' or f['other_formats'] != ['another_format'] or \
       f['boundingbox'].ExportToWkt() != 'POLYGON ((-180 -90,-180 90,180 90,180 -90,-180 -90))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="3" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
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
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="4" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
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
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 3:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""<dummy_xml/>""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 3:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""<invalid_xml>""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 3:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="hits" service="CSW" version="2.0.2" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName></csw:Query></csw:GetRecords>""",
"""<ServiceExceptionReport/>""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 3:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

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
    if fc != 200:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetAttributeFilter("identifier = 'an_identifier'")
    lyr.SetSpatialFilterRect(-180,-90,180,90)
    gdal.FileFromMemBuffer("""/vsimem/csw_endpoint&POSTFIELDS=<?xml version="1.0" encoding="UTF-8"?><csw:GetRecords resultType="results" service="CSW" version="2.0.2" startPosition="1" xmlns:csw="http://www.opengis.net/cat/csw/2.0.2" xmlns:gml="http://www.opengis.net/gml" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dct="http://purl.org/dc/terms/" xmlns:ogc="http://www.opengis.net/ogc" xmlns:ows="http://www.opengis.net/ows" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd"><csw:Query typeNames="csw:Record"><csw:ElementSetName>full</csw:ElementSetName><csw:Constraint version="1.1.0"><ogc:Filter><ogc:And><ogc:BBOX><ogc:PropertyName>ows:BoundingBox</ogc:PropertyName><gml:Envelope srsName="urn:ogc:def:crs:EPSG::4326"><gml:lowerCorner>-90 -180</gml:lowerCorner><gml:upperCorner>90 180</gml:upperCorner></gml:Envelope></ogc:BBOX><ogc:PropertyIsEqualTo><ogc:PropertyName>dc:identifier</ogc:PropertyName><ogc:Literal>an_identifier</ogc:Literal></ogc:PropertyIsEqualTo></ogc:And></ogc:Filter></csw:Constraint></csw:Query></csw:GetRecords>""",
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
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if fc != 300:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetAttributeFilter("identifier = 'an_identifier' AND " +
                           "references = 'http://foo/' AND " + 
                           "anytext LIKE '%%foo%%' AND " +
                           "other_identifiers = '' AND " +
                           "other_subjects = '' AND " +
                           "other_formats = '' AND " +
                           "other_references = '' AND " +
                           "ST_Intersects(boundingbox, ST_MakeEnvelope(2,49,2,49,4326))")
        
    return 'success'

###############################################################################

def ogr_csw_vsimem_cleanup():

    if gdaltest.csw_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)

    for f in gdal.ReadDir('/vsimem/'):
        gdal.Unlink('/vsimem/' + f)

    return 'success'

gdaltest_live_list = [ 
    ogr_csw_pycsw
    ]

gdaltest_vsimem_list = [ 
    ogr_csw_vsimem_fail_because_not_enabled,
    ogr_csw_vsimem_fail_because_no_get_capabilities,
    ogr_csw_vsimem_fail_because_empty_response,
    ogr_csw_vsimem_fail_because_no_CSW_Capabilities,
    ogr_csw_vsimem_fail_because_exception,
    ogr_csw_vsimem_fail_because_invalid_xml_capabilities,
    ogr_csw_vsimem_fail_because_missing_version,
    ogr_csw_vsimem_csw_minimal_instance,
    ogr_csw_vsimem_cleanup,
]

gdaltest_list = [ ogr_csw_init ]
gdaltest_list += gdaltest_vsimem_list
gdaltest_list += gdaltest_live_list

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_csw' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

