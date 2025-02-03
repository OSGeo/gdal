#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test AddBand() with VRTDerivedRasterBand.
# Author:   Antonio Valentino <a_valentino@users.sf.net>
#
###############################################################################
# Copyright (c) 2011, Antonio Valentino
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import threading

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


def _xmlsearch(root, nodetype, name):
    for node in root[2:]:
        if node[0] == nodetype and node[1] == name:
            return node


def _validate(content):

    try:
        from lxml import etree
    except ImportError:
        return

    import os

    gdal_data = gdal.GetConfigOption("GDAL_DATA")
    if gdal_data is None:
        print("GDAL_DATA not defined")
        return

    doc = etree.XML(content)
    try:
        schema_content = open(os.path.join(gdal_data, "gdalvrt.xsd"), "rb").read()
    except IOError:
        print("Cannot read gdalvrt.xsd schema")
        return
    schema = etree.XMLSchema(etree.XML(schema_content))
    schema.assertValid(doc)


###############################################################################
# Verify raster band subClass


def test_vrtderived_1():
    filename = "tmp/derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)

    simpleSourceXML = """    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>"""

    md = {}
    md["source_0"] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources")
    md_read = vrt_ds.GetRasterBand(1).GetMetadata("vrt_sources")
    vrt_ds = None

    expected_md_read = (
        "<SimpleSource>\n"
        '  <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>\n'
        "  <SourceBand>1</SourceBand>\n"
    )
    assert expected_md_read in md_read["source_0"]

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, "VRTRasterBand")
    node = _xmlsearch(node, gdal.CXT_Attribute, "subClass")
    node = _xmlsearch(node, gdal.CXT_Text, "VRTDerivedRasterBand")
    assert node is not None, "invalid subclass"


###############################################################################
# Verify derived raster band pixel function type


def test_vrtderived_2():
    filename = "tmp/derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
        "PixelFunctionType=dummy",
        "PixelFunctionLanguage=Python",
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)

    simpleSourceXML = """    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>"""

    md = {}
    md["source_0"] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources")
    with gdal.quiet_errors():
        cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == -1
    with gdal.quiet_errors():
        ret = vrt_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, " ")
    assert ret != 0
    vrt_ds = None

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, "VRTRasterBand")
    pixelfunctiontype = _xmlsearch(node, gdal.CXT_Element, "PixelFunctionType")
    pixelfunctiontype = _xmlsearch(pixelfunctiontype, gdal.CXT_Text, "dummy")
    assert pixelfunctiontype is not None, "incorrect PixelFunctionType value"
    pixelfunctionlanguage = _xmlsearch(node, gdal.CXT_Element, "PixelFunctionLanguage")
    pixelfunctionlanguage = _xmlsearch(pixelfunctionlanguage, gdal.CXT_Text, "Python")
    assert pixelfunctionlanguage is not None, "incorrect PixelFunctionLanguage value"


###############################################################################
# Verify derived raster band transfer type


def test_vrtderived_3():
    filename = "tmp/derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
        "PixelFunctionType=dummy",
        "SourceTransferType=Byte",
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)

    simpleSourceXML = """    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>"""

    md = {}
    md["source_0"] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources")
    vrt_ds = None

    xmlstring = open(filename).read()
    gdal.Unlink(filename)

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, "VRTRasterBand")
    node = _xmlsearch(node, gdal.CXT_Element, "SourceTransferType")
    node = _xmlsearch(node, gdal.CXT_Text, "Byte")
    assert node is not None, "incorrect SourceTransferType value"


###############################################################################
# Check handling of invalid derived raster band transfer type


def test_vrtderived_4():
    filename = "tmp/derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
        "PixelFunctionType=dummy",
        "SourceTransferType=Invalid",
    ]
    with gdal.quiet_errors():
        ret = vrt_ds.AddBand(gdal.GDT_Byte, options)
    assert ret != 0, "invalid SourceTransferType value not detected"


###############################################################################
# Check Python derived function with BufferRadius=1


def test_vrtderived_5():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        ds = gdal.Open("data/vrt/n43_hillshade.vrt")
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50577, "invalid checksum"


###############################################################################
# Check Python derived function with BufferRadius=0 and no source


def test_vrtderived_6():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        ds = gdal.Open("data/vrt/python_ones.vrt")
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == 10000, "invalid checksum"


###############################################################################
# Check Python derived function with no started Python interpreter


def test_vrtderived_7():

    import test_cli_utilities

    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    ret, err = gdaltest.runexternal_out_and_err(
        test_cli_utilities.get_gdalinfo_path()
        + " -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES"
    )
    if gdal.GetConfigOption("CPL_DEBUG") is not None:
        print(err)
    # Either we cannot find a Python library, either it works
    if "Checksum=-1" in ret:
        print("Did not manage to find a Python library")
    elif "Checksum=50577" not in ret:
        print(err)
        pytest.fail(ret)

    ret, err = gdaltest.runexternal_out_and_err(
        test_cli_utilities.get_gdalinfo_path()
        + " -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config VRT_ENABLE_PYTHON_PATH NO"
    )
    if gdal.GetConfigOption("CPL_DEBUG") is not None:
        print(err)
    # Either we cannot find a Python library, either it works
    if "Checksum=-1" in ret:
        print("Did not manage to find a Python library")
    elif "Checksum=50577" not in ret:
        print(err)
        pytest.fail(ret)

    ret, err = gdaltest.runexternal_out_and_err(
        test_cli_utilities.get_gdalinfo_path()
        + " -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config VRT_ENABLE_PYTHON_SYMLINK NO"
    )
    if gdal.GetConfigOption("CPL_DEBUG") is not None:
        print(err)
    # Either we cannot find a Python library, either it works
    if "Checksum=-1" in ret:
        print("Did not manage to find a Python library")
    elif "Checksum=50577" not in ret:
        print(err)
        pytest.fail(ret)

    # Invalid shared object name
    ret, err = gdaltest.runexternal_out_and_err(
        test_cli_utilities.get_gdalinfo_path()
        + " -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config PYTHONSO foo"
    )
    if gdal.GetConfigOption("CPL_DEBUG") is not None:
        print(err)
    assert "Checksum=-1" in ret, err

    # Valid shared object name, but without Python symbols
    libgdal_so = gdaltest.find_lib("gdal")
    if libgdal_so is not None:
        ret, err = gdaltest.runexternal_out_and_err(
            test_cli_utilities.get_gdalinfo_path()
            + ' -checksum data/vrt/n43_hillshade.vrt --config GDAL_VRT_ENABLE_PYTHON YES --config PYTHONSO "%s"'
            % libgdal_so
        )
        if gdal.GetConfigOption("CPL_DEBUG") is not None:
            print(err)
        assert "Checksum=-1" in ret, err


###############################################################################
# Check that GDAL_VRT_ENABLE_PYTHON=NO or undefined is honored


def test_vrtderived_8():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "NO"):
        ds = gdal.Open("data/vrt/n43_hillshade.vrt")
        with gdal.quiet_errors():
            cs = ds.GetRasterBand(1).Checksum()
    assert cs == -1, "invalid checksum"

    ds = gdal.Open("data/vrt/n43_hillshade.vrt")
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == -1, "invalid checksum"


###############################################################################
# Check various failure modes with Python functions


def test_vrtderived_9():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    # Missing PixelFunctionType
    with gdal.quiet_errors():
        ds = gdal.Open(
            """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
"""
        )
    assert ds is None

    # Unsupported PixelFunctionLanguage
    with gdal.quiet_errors():
        ds = gdal.Open(
            """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>foo</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
"""
        )
    assert ds is None

    # PixelFunctionCode can only be used with Python
    with gdal.quiet_errors():
        ds = gdal.Open(
            """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    syntax_error
]]>
     </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
"""
        )
    assert ds is None

    # BufferRadius can only be used with Python
    with gdal.quiet_errors():
        ds = gdal.Open(
            """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <BufferRadius>1</BufferRadius>
  </VRTRasterBand>
</VRTDataset>
"""
        )
    assert ds is None

    # Invalid BufferRadius
    with gdal.quiet_errors():
        ds = gdal.Open(
            """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <BufferRadius>-1</BufferRadius>
  </VRTRasterBand>
</VRTDataset>
"""
        )
    assert ds is None

    # Error at Python code compilation (indentation error)
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
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
"""
    )
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # Error at run time (in global code)
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
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
"""
    )
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # Error at run time (in pixel function)
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
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
"""
    )
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # User exception
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
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
"""
    )
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # unknown_function
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
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
"""
    )
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # uncallable object
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>uncallable_object</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
uncallable_object = True
]]>
    </PixelFunctionCode>
  </VRTRasterBand>
</VRTDataset>
"""
    )
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # unknown_module
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>unknown_module.unknown_function</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
"""
    )
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")


def vrtderived_code_that_only_makes_sense_with_GDAL_VRT_ENABLE_PYTHON_equal_IF_SAFE_but_that_is_now_disabled():

    # untrusted import
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
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
"""
    )
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # untrusted function
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
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
"""
    )
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # GDAL_VRT_ENABLE_PYTHON not set to YES
    ds = gdal.Open(
        """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
"""
    )
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")


###############################################################################
# Check Python function in another module


def one_pix_func(
    in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs
):
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
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        cs = ds.GetRasterBand(1).Checksum()
    if cs != 100:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # GDAL_VRT_TRUSTED_MODULES not defined
    ds = gdal.Open(content)
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # GDAL_VRT_PYTHON_TRUSTED_MODULES *NOT* matching our module
    for val in [
        "vrtderive",
        "vrtderivedX",
        "vrtderivedX*",
        "vrtderive.*" "vrtderivedX.*",
    ]:
        ds = gdal.Open(content)
        with gdal.config_option(
            "GDAL_VRT_PYTHON_TRUSTED_MODULES", val
        ), gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        if cs != -1:
            print(gdal.GetLastErrorMsg())
            pytest.fail("invalid checksum")

    # GDAL_VRT_PYTHON_TRUSTED_MODULES matching our module
    for val in [
        "foo,vrtderived,bar",
        "*",
        "foo,vrtderived*,bar",
        "foo,vrtderived.*,bar",
        "foo,vrtderi*,bar",
    ]:
        ds = gdal.Open(content)
        with gdal.config_option("GDAL_VRT_PYTHON_TRUSTED_MODULES", val):
            cs = ds.GetRasterBand(1).Checksum()
        if cs != 100:
            print(gdal.GetLastErrorMsg())
            pytest.fail("invalid checksum")


###############################################################################
# Test serializing with python code


def test_vrtderived_11():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.FileFromMemBuffer(
        "/vsimem/n43_hillshade.vrt",
        open("data/vrt/n43_hillshade.vrt", "rb")
        .read()
        .decode("UTF-8")
        .replace("../", "")
        .encode("UTF-8"),
    )
    gdal.FileFromMemBuffer("/vsimem/n43.tif", open("data/n43.tif", "rb").read())
    ds = gdal.Open("/vsimem/n43_hillshade.vrt", gdal.GA_Update)
    ds.SetMetadataItem("foo", "bar")
    ds = None
    ds = gdal.Open("/vsimem/n43_hillshade.vrt")
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink("/vsimem/n43_hillshade.vrt")
    gdal.Unlink("/vsimem/n43.tif")

    assert cs == 50577, "invalid checksum"


###############################################################################
# Test all data types with python code


def test_vrtderived_12():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    for dt in [
        "Byte",
        "UInt16",
        "Int16",
        "UInt32",
        "Int32",
        "Float32",
        "Float64",
        "CInt16",
        "CInt32",
        "CFloat32",
        "CFloat64",
    ]:
        ds = gdal.Open(
            """<VRTDataset rasterXSize="10" rasterYSize="10">
<VRTRasterBand dataType="%s" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
</VRTRasterBand>
</VRTDataset>"""
            % dt
        )

        with gdal.config_option(
            "GDAL_VRT_ENABLE_PYTHON", "YES"
        ), gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        # CInt16/CInt32 do not map to native numpy types
        if dt == "CInt16" or dt == "CInt32":
            expected_cs = -1  # error
        else:
            expected_cs = 100
        if cs != expected_cs:
            print(dt)
            print(gdal.GetLastErrorMsg())
            pytest.fail("invalid checksum")

    # Same for SourceTransferType
    for dt in ["CInt16", "CInt32"]:
        ds = gdal.Open(
            """<VRTDataset rasterXSize="10" rasterYSize="10">
<VRTRasterBand dataType="%s" band="1" subClass="VRTDerivedRasterBand">
    <SourceTransferType>Byte</SourceTransferType>
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
</VRTRasterBand>
</VRTDataset>"""
            % dt
        )

        with gdal.config_option(
            "GDAL_VRT_ENABLE_PYTHON", "YES"
        ), gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        if cs != -1:
            print(dt)
            print(gdal.GetLastErrorMsg())
            pytest.fail("invalid checksum")


###############################################################################
# Test translating a Python derived VRT


def test_vrtderived_13():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        # Will test the VRTDerivedRasterBand::IGetDataCoverageStatus() interface
        ds = gdal.GetDriverByName("GTiff").CreateCopy(
            "/vsimem/vrtderived_13.tif", gdal.Open("data/vrt/python_ones.vrt")
        )
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink("/vsimem/vrtderived_13.tif")

    assert cs == 10000, "invalid checksum"


###############################################################################
# Test statistics functions


def test_vrtderived_14():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        ds = gdal.GetDriverByName("VRT").CreateCopy(
            "/vsimem/vrtderived_14.vrt", gdal.Open("data/vrt/python_ones.vrt")
        )
        (my_min, my_max) = ds.GetRasterBand(1).ComputeRasterMinMax()
        (my_min2, my_max2, mean, stddev) = ds.GetRasterBand(1).ComputeStatistics(False)
        hist = ds.GetRasterBand(1).GetHistogram()

    assert (my_min, my_max) == (1.0, 1.0), "invalid ComputeRasterMinMax"

    assert (my_min2, my_max2, mean, stddev) == (
        1.0,
        1.0,
        1.0,
        0.0,
    ), "invalid ComputeStatistics"

    assert hist[1] == 10000, "invalid GetHistogram"

    ds = None
    gdal.GetDriverByName("VRT").Delete("/vsimem/vrtderived_14.vrt")


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
            args_dict["ret"] = False
        ds.FlushCache()


def test_vrtderived_15():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    gdal.SetConfigOption("GDAL_VRT_ENABLE_PYTHON", "YES")

    try:
        threads = []
        args_array = []
        for i in range(4):
            args_dict = {"ret": True}
            t = threading.Thread(target=vrtderived_15_worker, args=(args_dict,))
            args_array.append(args_dict)
            threads.append(t)
            t.start()

        ret = True
        for i in range(4):
            threads[i].join()
            if not args_array[i]:
                ret = False

    finally:
        gdal.SetConfigOption("GDAL_VRT_ENABLE_PYTHON", None)

    assert ret


###############################################################################
# Check the effect of the SkipNonContributingSources element


def test_vrtderived_skip_non_contributing_sources():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    def create_vrt(SkipNonContributingSources):
        Trace = ""
        if SkipNonContributingSources:
            Trace = 'open("tmp/num_sources_skip_true.txt", "wt").write(str(len(in_ar)))'
        else:
            Trace = (
                'open("tmp/num_sources_skip_false.txt", "wt").write(str(len(in_ar)))'
            )
        SkipNonContributingSources = "true" if SkipNonContributingSources else "false"
        ret = f"""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
    <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize, r, gt, **kwargs):
    {Trace}
    out_ar[:] = sum(in_ar)
]]>
    </PixelFunctionCode>
    <SkipNonContributingSources>{SkipNonContributingSources}</SkipNonContributingSources>
    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10" ySize="10" />
      <DstRect xOff="0" yOff="0" xSize="10" ySize="10" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="10" yOff="0" xSize="10" ySize="10" />
      <DstRect xOff="10" yOff="0" xSize="10" ySize="10" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>
"""
        # print(ret)
        return ret

    ds = gdal.Open(create_vrt(True))
    ref_ds = gdal.Open(create_vrt(False))

    with gdaltest.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        assert ds.ReadRaster(0, 0, 20, 20) == ref_ds.ReadRaster(0, 0, 20, 20)

        assert int(open("tmp/num_sources_skip_true.txt", "rt").read()) == 2
        os.unlink("tmp/num_sources_skip_true.txt")

        assert ds.ReadRaster(0, 0, 1, 1) == ref_ds.ReadRaster(0, 0, 1, 1)

        assert int(open("tmp/num_sources_skip_true.txt", "rt").read()) == 1
        os.unlink("tmp/num_sources_skip_true.txt")

        assert ds.ReadRaster(10, 0, 10, 10) == ref_ds.ReadRaster(10, 0, 10, 10)

        assert int(open("tmp/num_sources_skip_true.txt", "rt").read()) == 1
        os.unlink("tmp/num_sources_skip_true.txt")

        assert ds.ReadRaster(0, 10, 1, 1) == ref_ds.ReadRaster(0, 10, 1, 1)

        assert not os.path.exists("tmp/num_sources_skip_true.txt")

        assert int(open("tmp/num_sources_skip_false.txt", "rt").read()) == 2
        os.unlink("tmp/num_sources_skip_false.txt")

    xml = ds.GetMetadata("xml:VRT")[0]
    assert "<SkipNonContributingSources>true</SkipNonContributingSources>" in xml
    _validate(xml)


###############################################################################


@pytest.mark.parametrize("dtype", range(1, gdal.GDT_TypeCount))
def test_vrt_derived_dtype(tmp_vsimem, dtype):
    pytest.importorskip("numpy")

    input_fname = tmp_vsimem / "input.tif"

    nx = 1
    ny = 1

    with gdal.GetDriverByName("GTiff").Create(
        input_fname, nx, ny, 1, eType=gdal.GDT_Int8
    ) as input_ds:
        input_ds.GetRasterBand(1).Fill(1)
        gt = input_ds.GetGeoTransform()

    vrt_xml = f"""
        <VRTDataset rasterXSize="{nx}" rasterYSize="{ny}">
          <GeoTransform>{', '.join([str(x) for x in gt])}</GeoTransform>
          <VRTRasterBand dataType="{gdal.GetDataTypeName(dtype)}" band="1" subClass="VRTDerivedRasterBand">
            <PixelFunctionLanguage>Python</PixelFunctionLanguage>
            <PixelFunctionType>identity</PixelFunctionType>
            <PixelFunctionCode><![CDATA[
def identity(in_ar, out_ar, *args, **kwargs):
    out_ar[:] = in_ar[0]
]]>
    </PixelFunctionCode>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">{input_fname}</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="{nx}" ySize="{ny}" />
      <DstRect xOff="0" yOff="0" xSize="{nx}" ySize="{ny}" />
    </SimpleSource>
    </VRTRasterBand></VRTDataset>"""

    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        with gdal.Open(vrt_xml) as vrt_ds:
            arr = vrt_ds.ReadAsArray()
            if dtype not in {gdal.GDT_CInt16, gdal.GDT_CInt32}:
                assert arr[0, 0] == 1
            assert vrt_ds.GetRasterBand(1).DataType == dtype


###############################################################################
# Test arbitrary expression pixel functions


def vrt_expression_xml(tmpdir, expression, dialect, sources):

    drv = gdal.GetDriverByName("GTiff")

    nx = 1
    ny = 1

    expression = expression.replace("<", "&lt;").replace(">", "&gt;")

    xml = f"""<VRTDataset rasterXSize="{nx}" rasterYSize="{ny}">
              <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
                 <PixelFunctionType>expression</PixelFunctionType>
                 <PixelFunctionArguments expression="{expression}" dialect="{dialect}" />"""

    for i, source in enumerate(sources):
        if type(source) is tuple:
            source_name, source_value = source
        else:
            source_name = ""
            source_value = source

        src_fname = tmpdir / f"source_{i}.tif"

        with drv.Create(src_fname, 1, 1, 1, gdal.GDT_Float64) as ds:
            ds.GetRasterBand(1).Fill(source_value)

        xml += f"""<SimpleSource name="{source_name}">
                     <SourceFilename relativeToVRT="0">{src_fname}</SourceFilename>
                     <SourceBand>1</SourceBand>
                   </SimpleSource>"""

    xml += "</VRTRasterBand></VRTDataset>"

    return xml


@pytest.mark.parametrize(
    "expression,sources,result,dialects",
    [
        pytest.param("A", [("A", 77)], 77, None, id="identity"),
        pytest.param(
            "(NIR-R)/(NIR+R)",
            [("NIR", 77), ("R", 63)],
            (77 - 63) / (77 + 63),
            None,
            id="simple expression",
        ),
        pytest.param(
            "if (A > B) 1.5*C ; else A",
            [("A", 77), ("B", 63), ("C", 18)],
            27,
            ["exprtk"],
            id="exprtk conditional (explicit)",
        ),
        pytest.param(
            "(A > B) ? 1.5*C : A",
            [("A", 77), ("B", 63), ("C", 18)],
            27,
            ["muparser"],
            id="muparser conditional (explicit)",
        ),
        pytest.param(
            "(A > B)*(1.5*C) + (A <= B)*(A)",
            [("A", 77), ("B", 63), ("C", 18)],
            27,
            None,
            id="conditional (implicit)",
        ),
        pytest.param(
            "B2 * PopDensity",
            [("PopDensity", 3), ("", 7)],
            21,
            None,
            id="implicit source name",
        ),
        pytest.param(
            "B1 / sum(BANDS)",
            [("", 3), ("", 5), ("", 31)],
            3 / (3 + 5 + 31),
            None,
            id="use of BANDS variable",
        ),
        pytest.param(
            "B1 / sum(B2, B3) ",
            [("", 3), ("", 5), ("", 31)],
            3 / (5 + 31),
            None,
            id="aggregate specified inputs",
        ),
        pytest.param(
            "var q[2] := {B2, B3}; B1 * q",
            [("", 3), ("", 5), ("", 31)],
            15,  # First value in returned vector. This behavior doesn't seem desirable
            # but I haven't figured out how to detect a vector return.
            ["exprtk"],
            id="return vector",
        ),
        pytest.param(
            "B1 + B2 + B3",
            (5, 9, float("nan")),
            float("nan"),
            None,
            id="nan propagated via arithmetic",
        ),
        pytest.param(
            "if (B3) B1 ; else B2",
            (5, 9, float("nan")),
            5,
            ["exprtk"],
            id="exprtk nan = truth in conditional?",
        ),
        pytest.param(
            "B3 ? B1 : B2",
            (5, 9, float("nan")),
            5,
            ["muparser"],
            id="muparser nan = truth in conditional?",
        ),
        pytest.param(
            "if (B3 > 0) B1 ; else B2",
            (5, 9, float("nan")),
            9,
            ["exprtk"],
            id="exprtk nan comparison is false in conditional",
        ),
        pytest.param(
            "(B3 > 0) ? B1 : B2",
            (5, 9, float("nan")),
            9,
            ["muparser"],
            id="muparser nan comparison is false in conditional",
        ),
        pytest.param(
            "if (B1 > 5) B1",
            (1,),
            float("nan"),
            ["exprtk"],
            id="expression returns nodata",
        ),
    ],
)
@pytest.mark.parametrize("dialect", ("exprtk", "muparser"))
def test_vrt_pixelfn_expression(
    tmp_vsimem, expression, sources, result, dialect, dialects
):
    pytest.importorskip("numpy")

    if not gdaltest.gdal_has_vrt_expression_dialect(dialect):
        pytest.skip(f"Expression dialect {dialect} is not available")

    if dialects and dialect not in dialects:
        pytest.skip(f"Expression not supported for dialect {dialect}")

    xml = vrt_expression_xml(tmp_vsimem, expression, dialect, sources)

    with gdal.Open(xml) as ds:
        assert pytest.approx(ds.ReadAsArray()[0][0], nan_ok=True) == result


@pytest.mark.parametrize(
    "expression,sources,dialect,exception",
    [
        pytest.param(
            "A*B + C",
            [("A", 77), ("B", 63)],
            "exprtk",
            "Undefined symbol",
            id="exprtk undefined variable",
        ),
        pytest.param(
            "A*B + C",
            [("A", 77), ("B", 63)],
            "muparser",
            "Unexpected token",
            id="muparser undefined variable",
        ),
        pytest.param(
            "(".join(["asin", "sin", "acos", "cos"] * 100) + "(X" + 100 * 4 * ")",
            [("X", 0.5)],
            "exprtk",
            "exceeds maximum allowed stack depth",
            id="expression is too complex",
        ),
        pytest.param(
            " ".join(["sin(x) + cos(x)"] * 10000),
            [("x", 0.5)],
            "exprtk",
            "exceeds maximum of 100000 set by GDAL_EXPRTK_MAX_EXPRESSION_LENGTH",
            id="expression is too long",
        ),
    ],
)
def test_vrt_pixelfn_expression_invalid(
    tmp_vsimem, expression, sources, dialect, exception
):
    pytest.importorskip("numpy")

    if not gdaltest.gdal_has_vrt_expression_dialect(dialect):
        pytest.skip(f"Expression dialect {dialect} is not available")

    messages = []

    def handle(ecls, ecode, emsg):
        messages.append(emsg)

    xml = vrt_expression_xml(tmp_vsimem, expression, dialect, sources)

    with gdaltest.error_handler(handle):
        ds = gdal.Open(xml)
        if ds:
            assert ds.ReadAsArray() is None

    assert exception in "".join(messages)


###############################################################################
# Cleanup.


def test_vrtderived_cleanup():
    try:
        os.remove("tmp/derived.vrt")
    except OSError:
        pass
