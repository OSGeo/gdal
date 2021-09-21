#!/usr/bin/env pytest
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
import threading
from osgeo import gdal


import gdaltest
import pytest


def _xmlsearch(root, nodetype, name):
    for node in root[2:]:
        if node[0] == nodetype and node[1] == name:
            return node

###############################################################################
# Verify raster band subClass


def test_vrtderived_1():
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
        '  <SourceBand>1</SourceBand>\n')
    assert expected_md_read in md_read['source_0']

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, 'VRTRasterBand')
    node = _xmlsearch(node, gdal.CXT_Attribute, 'subClass')
    node = _xmlsearch(node, gdal.CXT_Text, 'VRTDerivedRasterBand')
    assert node is not None, 'invalid subclass'

###############################################################################
# Verify derived raster band pixel function type


def test_vrtderived_2():
    filename = 'tmp/derived.vrt'
    vrt_ds = gdal.GetDriverByName('VRT').Create(filename, 50, 50, 0)

    options = [
        'subClass=VRTDerivedRasterBand',
        'PixelFunctionType=dummy',
        'PixelFunctionLanguage=Python',
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)

    simpleSourceXML = '''    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>'''

    md = {}
    md['source_0'] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, 'vrt_sources')
    with gdaltest.error_handler():
        cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 0
    with gdaltest.error_handler():
        ret = vrt_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, ' ')
    assert ret != 0
    vrt_ds = None

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, 'VRTRasterBand')
    pixelfunctiontype = _xmlsearch(node, gdal.CXT_Element, 'PixelFunctionType')
    pixelfunctiontype = _xmlsearch(pixelfunctiontype, gdal.CXT_Text, 'dummy')
    assert pixelfunctiontype is not None, 'incorrect PixelFunctionType value'
    pixelfunctionlanguage = _xmlsearch(node, gdal.CXT_Element, 'PixelFunctionLanguage')
    pixelfunctionlanguage = _xmlsearch(pixelfunctionlanguage, gdal.CXT_Text, 'Python')
    assert pixelfunctionlanguage is not None, 'incorrect PixelFunctionLanguage value'

###############################################################################
# Verify derived raster band transfer type


def test_vrtderived_3():
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

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, 'VRTRasterBand')
    node = _xmlsearch(node, gdal.CXT_Element, 'SourceTransferType')
    node = _xmlsearch(node, gdal.CXT_Text, 'Byte')
    assert node is not None, 'incorrect SourceTransferType value'

###############################################################################
# Check handling of invalid derived raster band transfer type


def test_vrtderived_4():
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
    assert ret != 0, 'invalid SourceTransferType value not detected'

###############################################################################
# Check Python derived function with BufferRadius=1


def test_vrtderived_5():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    ds = gdal.Open('data/vrt/n43_hillshade.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    assert cs == 50577, 'invalid checksum'

###############################################################################
# Check Python derived function with BufferRadius=0 and no source


def test_vrtderived_6():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    ds = gdal.Open('data/vrt/python_ones.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    assert cs == 10000, 'invalid checksum'

###############################################################################
# Check Python derived function with no started Python interpreter


def test_vrtderived_7():

    import test_cli_utilities
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES')
    if gdal.GetConfigOption('CPL_DEBUG') is not None:
        print(err)
    # Either we cannot find a Python library, either it works
    if 'Checksum=0' in ret:
        print('Did not manage to find a Python library')
    elif 'Checksum=50577' not in ret:
        print(err)
        pytest.fail(ret)

    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config VRT_ENABLE_PYTHON_PATH NO')
    if gdal.GetConfigOption('CPL_DEBUG') is not None:
        print(err)
# Either we cannot find a Python library, either it works
    if 'Checksum=0' in ret:
        print('Did not manage to find a Python library')
    elif 'Checksum=50577' not in ret:
        print(err)
        pytest.fail(ret)

    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config VRT_ENABLE_PYTHON_SYMLINK NO')
    if gdal.GetConfigOption('CPL_DEBUG') is not None:
        print(err)
# Either we cannot find a Python library, either it works
    if 'Checksum=0' in ret:
        print('Did not manage to find a Python library')
    elif 'Checksum=50577' not in ret:
        print(err)
        pytest.fail(ret)

    # Invalid shared object name
    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config PYTHONSO foo')
    if gdal.GetConfigOption('CPL_DEBUG') is not None:
        print(err)
    assert 'Checksum=0' in ret, err

    # Valid shared object name, but without Python symbols
    libgdal_so = gdaltest.find_lib('gdal')
    if libgdal_so is not None:
        ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config PYTHONSO "%s"' % libgdal_so)
        if gdal.GetConfigOption('CPL_DEBUG') is not None:
            print(err)
        assert 'Checksum=0' in ret, err


###############################################################################
# Check that GDAL_VRT_ENABLE_PYTHON=NO or undefined is honored


def test_vrtderived_8():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'NO')
    ds = gdal.Open('data/vrt/n43_hillshade.vrt')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    assert cs == 0, 'invalid checksum'

    ds = gdal.Open('data/vrt/n43_hillshade.vrt')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == 0, 'invalid checksum'

###############################################################################
# Check various failure modes with Python functions


def test_vrtderived_9():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    # Missing PixelFunctionType
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # Unsupported PixelFunctionLanguage
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>foo</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # PixelFunctionCode can only be used with Python
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    syntax_error
]]>
     </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # BufferRadius can only be used with Python
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <BufferRadius>1</BufferRadius>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # Invalid BufferRadius
    with gdaltest.error_handler():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <BufferRadius>-1</BufferRadius>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # Error at Python code compilation (indentation error)
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # Error at run time (in global code)
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # Error at run time (in pixel function)
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # User exception
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # unknown_function
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # uncallable object
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # unknown_module
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')



def vrtderived_code_that_only_makes_sense_with_GDAL_VRT_ENABLE_PYTHON_equal_IF_SAFE_but_that_is_now_disabled():

    # untrusted import
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # untrusted function
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
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
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # GDAL_VRT_ENABLE_PYTHON not set to YES
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')


###############################################################################
# Check Python function in another module


def one_pix_func(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    # pylint: disable=unused-argument
    out_ar.fill(1)


def test_vrtderived_10():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    content = """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
"""

    ds = gdal.Open(content)
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    if cs != 100:
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # GDAL_VRT_TRUSTED_MODULES not defined
    ds = gdal.Open(content)
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        print(gdal.GetLastErrorMsg())
        pytest.fail('invalid checksum')

    # GDAL_VRT_PYTHON_TRUSTED_MODULES *NOT* matching our module
    for val in ['vrtderive',
                'vrtderivedX',
                'vrtderivedX*',
                'vrtderive.*'
                'vrtderivedX.*']:
        ds = gdal.Open(content)
        gdal.SetConfigOption('GDAL_VRT_PYTHON_TRUSTED_MODULES', val)
        with gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GDAL_VRT_PYTHON_TRUSTED_MODULES', None)
        if cs != 0:
            print(gdal.GetLastErrorMsg())
            pytest.fail('invalid checksum')

    # GDAL_VRT_PYTHON_TRUSTED_MODULES matching our module
    for val in ['foo,vrtderived,bar',
                '*',
                'foo,vrtderived*,bar',
                'foo,vrtderived.*,bar',
                'foo,vrtderi*,bar']:
        ds = gdal.Open(content)
        gdal.SetConfigOption('GDAL_VRT_PYTHON_TRUSTED_MODULES', val)
        cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GDAL_VRT_PYTHON_TRUSTED_MODULES', None)
        if cs != 100:
            print(gdal.GetLastErrorMsg())
            pytest.fail('invalid checksum')


###############################################################################
# Test serializing with python code


def test_vrtderived_11():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/n43_hillshade.vrt', open('data/vrt/n43_hillshade.vrt', 'rb').read().decode('UTF-8').replace('../', '').encode('UTF-8'))
    gdal.FileFromMemBuffer('/vsimem/n43.dt0', open('data/n43.dt0', 'rb').read())
    ds = gdal.Open('/vsimem/n43_hillshade.vrt', gdal.GA_Update)
    ds.SetMetadataItem('foo', 'bar')
    ds = None
    ds = gdal.Open('/vsimem/n43_hillshade.vrt')
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', 'YES')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    ds = None

    gdal.Unlink('/vsimem/n43_hillshade.vrt')
    gdal.Unlink('/vsimem/n43.dt0')

    assert cs == 50577, 'invalid checksum'

###############################################################################
# Test all data types with python code


def test_vrtderived_12():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    for dt in ["Byte", "UInt16", "Int16", "UInt32", "Int32",
               "Float32", "Float64",
               "CInt16", "CInt32", "CFloat32", "CFloat64"]:
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
<VRTRasterBand dataType="%s" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
</VRTRasterBand>
</VRTDataset>""" % dt)

        gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")
        with gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
        # CInt16/CInt32 do not map to native numpy types
        if dt == 'CInt16' or dt == 'CInt32':
            expected_cs = 0  # error
        else:
            expected_cs = 100
        if cs != expected_cs:
            print(dt)
            print(gdal.GetLastErrorMsg())
            pytest.fail('invalid checksum')

    # Same for SourceTransferType
    for dt in ["CInt16", "CInt32"]:
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
<VRTRasterBand dataType="%s" band="1" subClass="VRTDerivedRasterBand">
    <SourceTransferType>Byte</SourceTransferType>
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
</VRTRasterBand>
</VRTDataset>""" % dt)

        gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")
        with gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
        if cs != 0:
            print(dt)
            print(gdal.GetLastErrorMsg())
            pytest.fail('invalid checksum')


###############################################################################
# Test translating a Python derived VRT


def test_vrtderived_13():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")
    # Will test the VRTDerivedRasterBand::IGetDataCoverageStatus() interface
    ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrtderived_13.tif', gdal.Open('data/vrt/python_ones.vrt'))
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink('/vsimem/vrtderived_13.tif')

    assert cs == 10000, 'invalid checksum'

###############################################################################
# Test statistics functions


def test_vrtderived_14():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")
    ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtderived_14.vrt', gdal.Open('data/vrt/python_ones.vrt'))
    (my_min, my_max) = ds.GetRasterBand(1).ComputeRasterMinMax()
    (my_min2, my_max2, mean, stddev) = ds.GetRasterBand(1).ComputeStatistics(False)
    hist = ds.GetRasterBand(1).GetHistogram()
    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)

    assert (my_min, my_max) == (1.0, 1.0), 'invalid ComputeRasterMinMax'

    assert (my_min2, my_max2, mean, stddev) == (1.0, 1.0, 1.0, 0.0), \
        'invalid ComputeStatistics'

    assert hist[1] == 10000, 'invalid GetHistogram'

    ds = None
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrtderived_14.vrt')

###############################################################################
# Test threading


def vrtderived_15_worker(args_dict):

    content = """<VRTDataset rasterXSize="2000" rasterYSize="2000">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
"""
    ds = gdal.Open(content)
    for _ in range(5):
        cs = ds.GetRasterBand(1).Checksum()
        if cs != 2304:
            print(cs)
            args_dict['ret'] = False
        ds.FlushCache()


def test_vrtderived_15():

    try:
        import numpy
        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', "YES")

    threads = []
    args_array = []
    for i in range(4):
        args_dict = {'ret': True}
        t = threading.Thread(target=vrtderived_15_worker, args=(args_dict,))
        args_array.append(args_dict)
        threads.append(t)
        t.start()

    ret = 'success'
    for i in range(4):
        threads[i].join()
        if not args_array[i]:
            ret = 'fail'

    gdal.SetConfigOption('GDAL_VRT_ENABLE_PYTHON', None)

    return ret


###############################################################################
# Cleanup.

def test_vrtderived_cleanup():
    try:
        os.remove('tmp/derived.vrt')
    except OSError:
        pass



