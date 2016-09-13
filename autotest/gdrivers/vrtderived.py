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
import shutil
import sys
from osgeo import gdal

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

    expected_md_read = (
        '<SimpleSource>\n'
        '  <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>\n'
        '  <SourceBand>1</SourceBand>\n'
        '  <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" '
        'BlockXSize="20" BlockYSize="20" />\n'
        '</SimpleSource>\n')
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
# Check Python derived function with BufferRadius=1

def vrtderived_5():

    try:
        import numpy
        numpy.ones
    except:
        return 'skip'

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    ds = gdal.Open('data/n43_hillshade.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 50577:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Check Python derived function with BufferRadius=0 and no source

def vrtderived_6():

    try:
        import numpy
        numpy.ones
    except:
        return 'skip'

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    ds = gdal.Open('data/python_ones.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 10000:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Check Python derived function with no started Python interpreter

def vrtderived_7():

    import test_cli_utilities
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES')
    # Either we cannot find a Python library, either it works
    if ret.find('Checksum=0') >= 0:
        print('Did not manage to find a Python library')
    elif ret.find('Checksum=50577') < 0:
        gdaltest.post_reason( 'fail' )
        print(ret)
        print(err)
        return 'fail'

    # Invalid shared object name
    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config PYTHONSO foo')
    if ret.find('Checksum=0') < 0:
        gdaltest.post_reason( 'fail' )
        print(ret)
        print(err)
        return 'fail'

    # Valid shared object name, but without Python symbos
    libgdal_so = gdaltest.find_lib('gdal')
    if libgdal_so is not None:
        ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config PYTHONSO "%s"' % libgdal_so)
        if ret.find('Checksum=0') < 0:
            gdaltest.post_reason( 'fail' )
            print(ret)
            print(err)
            return 'fail'

    return 'success'

###############################################################################
# Check that GDAL_VRT_ENABLE_PYTHON=NO or undefined is honored

def vrtderived_8():

    try:
        import numpy
        numpy.ones
    except:
        return 'skip'

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'NO')
    ds = gdal.Open('data/n43_hillshade.vrt')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        return 'fail'

    ds = gdal.Open('data/n43_hillshade.vrt')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Check various failure modes with Python functions

def vrtderived_9():

    try:
        import numpy
        numpy.ones
    except:
        return 'skip'

    # Unsupported PixelFunctionLanguage
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>foo</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    if ds is not None:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    # PixelFunctionCode can only be used with Python
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    syntax_error
]]>
     </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    if ds is not None:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    # PixelFunctionArguments can only be used with Python
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionArguments foo="bar"/>
  </VRTRasterBand>
</VRTDataset>
""")
    if ds is not None:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    # BufferRadius can only be used with Python
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <BufferRadius>1</BufferRadius>
  </VRTRasterBand>
</VRTDataset>
""")
    if ds is not None:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    # Error at Python code compilation (indentation error)
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
syntax_error
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Error at run time (in global code)
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
runtime_error
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    pass
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Error at run time (in pixel function)
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    runtime_error
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # User exception
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    raise Exception('my exception')
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # unknown_function
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>unknown_function</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    pass
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # uncallable object
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>uncallable_object</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
uncallable_object = True
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # unknown_module
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>unknown_module.unknown_function</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    with gdaltest.error_handler():
        gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")
        cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

def vrtderived_code_that_only_makes_sense_with_GDAL_VRT_ENABLE_PYTHON_equal_IF_SAFE_but_that_is_now_disabled():

    # untrusted import
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>my_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
def my_func(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    import foo
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # untrusted function
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>my_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
def my_func(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    open('/etc/passwd').read()
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'


    # GDAL_VRT_ENABLE_PYTHON not set to YES
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Check Python function in another module

def one_pix_func(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    out_ar.fill(1)

def vrtderived_10():

    try:
        import numpy
        numpy.ones
    except:
        return 'skip'

    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 100:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test serializing with python code

def vrtderived_11():

    try:
        import numpy
        numpy.ones
    except:
        return 'skip'

    shutil.copy('data/n43_hillshade.vrt', 'tmp/n43_hillshade.vrt')
    shutil.copy('data/n43.dt0', 'tmp/n43.dt0')
    ds = gdal.Open('tmp/n43_hillshade.vrt', gdal.GA_Update)
    ds.SetMetadataItem('foo', 'bar')
    ds = None
    ds = gdal.Open('tmp/n43_hillshade.vrt')
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    ds = None

    os.unlink('tmp/n43_hillshade.vrt')
    os.unlink('tmp/n43.dt0')

    if cs != 50577:
        gdaltest.post_reason( 'invalid checksum' )
        print(cs)
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
    vrtderived_5,
    vrtderived_6,
    vrtderived_7,
    vrtderived_8,
    vrtderived_9,
    vrtderived_10,
    vrtderived_11,
    vrtderived_cleanup,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtderived' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
