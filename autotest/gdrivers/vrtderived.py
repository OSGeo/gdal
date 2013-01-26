#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test AddBand() with VRTDerivedRasterBand.
# Author:   Antonio Valentino <a_valentino@users.sf.net>
#
###############################################################################
# Copyright (c) 2011, Antonio Valentino
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
import gdal

sys.path.append( '../pymod' )

import gdaltest

def _xmlsearch(root, nodetype, name):
    for node in root[2:]:
        if node[0] == nodetype and node[1] == name:
            return node
    else:
        None

###############################################################################
# Verify raster band subClass

def vrtderived_1():
    filename = 'tmp/derived.vrt'
    vrt_ds = gdal.GetDriverByName('VRT').Create(filename, 50, 50, 0)

    options = [
        'subClass=VRTDerivedRasterBand',
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)

    simpleSourceXML = '''    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>'''

    md = {}
    md['source_0'] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, 'vrt_sources')
    md_read = vrt_ds.GetRasterBand(1).GetMetadata('vrt_sources')
    vrt_ds = None
    
    expected_md_read = '<SimpleSource>\n  <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>\n  <SourceBand>1</SourceBand>\n  <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />\n</SimpleSource>\n'
    if md_read['source_0'] != expected_md_read:
        gdaltest.post_reason('fail')
        print(md_read['source_0'])
        return 'fail'

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString( xmlstring )
    node = _xmlsearch(node, gdal.CXT_Element, 'VRTRasterBand')
    node = _xmlsearch(node, gdal.CXT_Attribute, 'subClass')
    node = _xmlsearch(node, gdal.CXT_Text, 'VRTDerivedRasterBand')
    if node is None:
        gdaltest.post_reason( 'invalid subclass' )
        return 'fail'

    return 'success'

###############################################################################
# Verify derived raster band pixel function type

def vrtderived_2():
    filename = 'tmp/derived.vrt'
    vrt_ds = gdal.GetDriverByName('VRT').Create(filename, 50, 50, 0)

    options = [
        'subClass=VRTDerivedRasterBand',
        'PixelFunctionType=dummy',
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)

    simpleSourceXML = '''    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>'''

    md = {}
    md['source_0'] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, 'vrt_sources')
    vrt_ds = None

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString( xmlstring )
    node = _xmlsearch(node, gdal.CXT_Element, 'VRTRasterBand')
    node = _xmlsearch(node, gdal.CXT_Element, 'PixelFunctionType')
    node = _xmlsearch(node, gdal.CXT_Text, 'dummy')
    if node is None:
        gdaltest.post_reason( 'incorrect PixelFunctionType value' )
        return 'fail'

    return 'success'

###############################################################################
# Verify derived raster band transfer type

def vrtderived_3():
    filename = 'tmp/derived.vrt'
    vrt_ds = gdal.GetDriverByName('VRT').Create(filename, 50, 50, 0)

    options = [
        'subClass=VRTDerivedRasterBand',
        'PixelFunctionType=dummy',
        'SourceTransferType=Byte',
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)

    simpleSourceXML = '''    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>'''

    md = {}
    md['source_0'] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, 'vrt_sources')
    vrt_ds = None

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString( xmlstring )
    node = _xmlsearch(node, gdal.CXT_Element, 'VRTRasterBand')
    node = _xmlsearch(node, gdal.CXT_Element, 'SourceTransferType')
    node = _xmlsearch(node, gdal.CXT_Text, 'Byte')
    if node is None:
        gdaltest.post_reason( 'incorrect SourceTransferType value' )
        return 'fail'

    return 'success'

###############################################################################
# Check handling of invalid derived raster band transfer type

def vrtderived_4():
    filename = 'tmp/derived.vrt'
    vrt_ds = gdal.GetDriverByName('VRT').Create(filename, 50, 50, 0)

    options = [
        'subClass=VRTDerivedRasterBand',
        'PixelFunctionType=dummy',
        'SourceTransferType=Invalid',
    ]
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = vrt_ds.AddBand(gdal.GDT_Byte, options)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason( 'invalid SourceTransferType value not detected' )
        return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def vrtderived_cleanup():
    try:
        os.remove( 'tmp/derived.vrt' )
    except:
        pass
    return 'success'

gdaltest_list = [
    vrtderived_1,
    vrtderived_2,
    vrtderived_3,
    vrtderived_4,
    vrtderived_cleanup,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtderived' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
