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

import math
import os
import sys
import threading

import gdaltest
import pytest

from osgeo import gdal, ogr

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


def test_vrtderived_1(tmp_vsimem):
    filename = tmp_vsimem / "derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
    ]
    vrt_ds.AddBand(gdal.GDT_UInt8, options)

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

    xmlstring = gdal.VSIFile(filename, "r").read()

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, "VRTRasterBand")
    node = _xmlsearch(node, gdal.CXT_Attribute, "subClass")
    node = _xmlsearch(node, gdal.CXT_Text, "VRTDerivedRasterBand")
    assert node is not None, "invalid subclass"


###############################################################################
# Verify derived raster band pixel function type


def test_vrtderived_2(tmp_vsimem):
    filename = tmp_vsimem / "derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
        "PixelFunctionType=dummy",
        "PixelFunctionLanguage=Python",
    ]
    vrt_ds.AddBand(gdal.GDT_UInt8, options)

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

    xmlstring = gdal.VSIFile(filename, "r").read()

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


def test_vrtderived_3(tmp_vsimem):
    filename = tmp_vsimem / "derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
        "PixelFunctionType=dummy",
        "SourceTransferType=Byte",
    ]
    vrt_ds.AddBand(gdal.GDT_UInt8, options)

    simpleSourceXML = """    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>"""

    md = {}
    md["source_0"] = simpleSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources")
    vrt_ds = None

    xmlstring = gdal.VSIFile(filename, "r").read()

    node = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(node, gdal.CXT_Element, "VRTRasterBand")
    node = _xmlsearch(node, gdal.CXT_Element, "SourceTransferType")
    node = _xmlsearch(node, gdal.CXT_Text, "Byte")
    assert node is not None, "incorrect SourceTransferType value"


###############################################################################
# Check handling of invalid derived raster band transfer type


def test_vrtderived_4(tmp_vsimem):
    filename = tmp_vsimem / "derived.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)

    options = [
        "subClass=VRTDerivedRasterBand",
        "PixelFunctionType=dummy",
        "SourceTransferType=Invalid",
    ]
    with gdal.quiet_errors():
        ret = vrt_ds.AddBand(gdal.GDT_UInt8, options)
    assert ret != 0, "invalid SourceTransferType value not detected"


###############################################################################
# Check handling of pixel function without correct subclass


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_wrong_subclass():

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Byte" band="1">
        <PixelFunctionType>inv</PixelFunctionType>
        <SimpleSource>
           <SourceFilename>data/byte.tif</SourceFilename>
           <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with pytest.raises(
        RuntimeError, match="may only be used with subClass=VRTDerivedRasterBand"
    ):
        gdal.Open(xml)


###############################################################################
# Check Python derived function with BufferRadius=1


def test_vrtderived_5():

    try:
        import numpy

        numpy.ones
    except (ImportError, AttributeError):
        pytest.skip()

    def worker(args_dict):
        ds = gdal.Open("data/vrt/n43_hillshade.vrt")
        for i in range(20):
            ds.FlushCache()
            with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
                cs = ds.GetRasterBand(1).Checksum()
            if cs != 50577:
                print("Got wrong cs", cs)
                args_dict["ret"] = False

    import threading

    threads = []
    args_array = []
    num_threads = gdal.GetNumCPUs()
    for i in range(num_threads):
        args_dict = {"ret": True}
        t = threading.Thread(target=worker, args=(args_dict,))
        args_array.append(args_dict)
        threads.append(t)
        t.start()

    for i in range(len(threads)):
        threads[i].join()
        if not args_array[i]:
            assert False


###############################################################################
# Check Python derived function with BufferRadius=0 and no source


def test_vrtderived_6():

    pytest.importorskip("numpy")

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

    pytest.importorskip("numpy")

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

    pytest.importorskip("numpy")

    # Missing PixelFunctionType
    with gdal.quiet_errors():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # Unsupported PixelFunctionLanguage
    with gdal.quiet_errors():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <PixelFunctionLanguage>foo</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # PixelFunctionCode can only be used with Python
    with gdal.quiet_errors():
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
    with gdal.quiet_errors():
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>identity</PixelFunctionType>
    <BufferRadius>1</BufferRadius>
  </VRTRasterBand>
</VRTDataset>
""")
    assert ds is None

    # Invalid BufferRadius
    with gdal.quiet_errors():
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
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

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
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

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
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

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
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

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
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

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
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # unknown_module
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>unknown_module.unknown_function</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")


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
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

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
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    if cs != -1:
        print(gdal.GetLastErrorMsg())
        pytest.fail("invalid checksum")

    # GDAL_VRT_ENABLE_PYTHON not set to YES
    ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
  </VRTRasterBand>
</VRTDataset>
""")
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

    pytest.importorskip("numpy")

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

    pytest.importorskip("numpy")

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

    pytest.importorskip("numpy")

    for dt in [
        "Byte",
        "Int8",
        "UInt16",
        "Int16",
        "UInt32",
        "Int32",
        "UInt64",
        "Int64",
        "Float16",
        "Float32",
        "Float64",
        "CInt16",
        "CInt32",
        "CFloat16",
        "CFloat32",
        "CFloat64",
    ]:
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
<VRTRasterBand dataType="%s" band="1" subClass="VRTDerivedRasterBand">
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
</VRTRasterBand>
</VRTDataset>""" % dt)

        with gdal.config_option(
            "GDAL_VRT_ENABLE_PYTHON", "YES"
        ), gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        # CInt16/CInt32/CFloat16 do not map to native numpy types
        if dt == "CInt16" or dt == "CInt32" or dt == "CFloat16":
            expected_cs = [-1]  # error
        elif dt == "Float16":
            # Might or might not be supported by GDAL
            expected_cs = [-1, 100]
        else:
            expected_cs = [100]
        if cs not in expected_cs:
            print(dt)
            print(gdal.GetLastErrorMsg())
            if len(expected_cs) == 1:
                pytest.fail(
                    "invalid checksum, datatype %s, have %d, expected %d"
                    % (dt, cs, expected_cs[0])
                )
            else:
                pytest.fail(
                    "invalid checksum, datatype %s, have %d, expected one of [%d, %d]"
                    % (dt, cs, expected_cs[0], expected_cs[1])
                )

    # Same for SourceTransferType
    for dt in ["CInt16", "CInt32", "CFloat16"]:
        ds = gdal.Open("""<VRTDataset rasterXSize="10" rasterYSize="10">
<VRTRasterBand dataType="%s" band="1" subClass="VRTDerivedRasterBand">
    <SourceTransferType>Byte</SourceTransferType>
    <ColorInterp>Gray</ColorInterp>
    <PixelFunctionType>vrtderived.one_pix_func</PixelFunctionType>
    <PixelFunctionLanguage>Python</PixelFunctionLanguage>
</VRTRasterBand>
</VRTDataset>""" % dt)

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

    pytest.importorskip("numpy")

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

    pytest.importorskip("numpy")

    with gdal.config_option("GDAL_VRT_ENABLE_PYTHON", "YES"):
        ds = gdal.GetDriverByName("VRT").CreateCopy(
            "/vsimem/vrtderived_14.vrt", gdal.Open("data/vrt/python_ones.vrt")
        )
        my_min, my_max = ds.GetRasterBand(1).ComputeRasterMinMax()
        my_min2, my_max2, mean, stddev = ds.GetRasterBand(1).ComputeStatistics(False)
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

    pytest.importorskip("numpy")

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


def test_vrtderived_skip_non_contributing_sources(tmp_path):

    pytest.importorskip("numpy")

    def create_vrt(SkipNonContributingSources):
        Trace = ""
        if SkipNonContributingSources:
            Trace = f'open(r"{tmp_path}/num_sources_skip_true.txt", "wt").write(str(len(in_ar)))'
        else:
            Trace = f'open(r"{tmp_path}/num_sources_skip_false.txt", "wt").write(str(len(in_ar)))'
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

        assert int(open(tmp_path / "num_sources_skip_true.txt", "rt").read()) == 2
        os.unlink(tmp_path / "num_sources_skip_true.txt")

        assert ds.ReadRaster(0, 0, 1, 1) == ref_ds.ReadRaster(0, 0, 1, 1)

        assert int(open(tmp_path / "num_sources_skip_true.txt", "rt").read()) == 1
        os.unlink(tmp_path / "num_sources_skip_true.txt")

        assert ds.ReadRaster(10, 0, 10, 10) == ref_ds.ReadRaster(10, 0, 10, 10)

        assert int(open(tmp_path / "num_sources_skip_true.txt", "rt").read()) == 1
        os.unlink(tmp_path / "num_sources_skip_true.txt")

        assert ds.ReadRaster(0, 10, 1, 1) == ref_ds.ReadRaster(0, 10, 1, 1)

        assert not os.path.exists(tmp_path / "num_sources_skip_true.txt")

        assert int(open(tmp_path / "num_sources_skip_false.txt", "rt").read()) == 2
        os.unlink(tmp_path / "num_sources_skip_false.txt")

    xml = ds.GetMetadata("xml:VRT")[0]
    assert "<SkipNonContributingSources>true</SkipNonContributingSources>" in xml
    _validate(xml)


###############################################################################


@pytest.mark.parametrize("dtype", range(1, gdal.GDT_TypeCount))
def test_vrt_derived_dtype(tmp_vsimem, dtype):

    gdaltest.importorskip_gdal_array()
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
            # The complex int/float types are not available in numpy.
            # Float16 may or may not be supported by GDAL.
            if dtype not in {
                gdal.GDT_CInt16,
                gdal.GDT_CInt32,
                gdal.GDT_Float16,
                gdal.GDT_CFloat16,
            }:
                assert arr[0, 0] == 1
            assert vrt_ds.GetRasterBand(1).DataType == dtype


###############################################################################
#


@gdaltest.enable_exceptions()
def test_vrt_expression_missing_expression_arg():

    ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <PixelFunctionType>expression</PixelFunctionType>
  </VRTRasterBand>
</VRTDataset>""")
    with pytest.raises(Exception, match="Missing 'expression' pixel function argument"):
        ds.GetRasterBand(1).Checksum()


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
            "isnan(B3) ? B1 : B2",
            (5, 9, float("nan")),
            5,
            ["muparser"],
            id="muparser isnan",
        ),
        pytest.param(
            "if (B1 > 5) B1",
            (1,),
            float("nan"),
            ["exprtk"],
            id="expression returns nodata",
        ),
        pytest.param(
            "ZB[1] + B[1]",
            [("ZB[1]", 7), ("B[1]", 3)],
            10,
            ["muparser"],
            id="index substitution works correctly",
        ),
        pytest.param(
            "fmod(B, A)",
            [("A", 2.2), ("B", 7.3)],
            0.7,
            ["muparser"],
            id="fmod works correctly",
        ),
    ],
)
@pytest.mark.parametrize("dialect", ("exprtk", "muparser"))
def test_vrt_pixelfn_expression(
    tmp_vsimem, expression, sources, result, dialect, dialects
):
    gdaltest.importorskip_gdal_array()
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
        pytest.param(
            "B@1",
            [("B@1", 3)],
            "muparser",
            "Invalid variable name",
            id="invalid variable name",
        ),
    ],
)
def test_vrt_pixelfn_expression_invalid(
    tmp_vsimem, expression, sources, dialect, exception
):
    gdaltest.importorskip_gdal_array()
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


def test_vrt_pixelfn_expression_coordinates():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("muparser not available")

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    with gdal.Open("data/byte.tif") as ds:
        gt = ds.GetGeoTransform()
        nx = ds.RasterXSize
        ny = ds.RasterYSize

    xml = f"""
    <VRTDataset rasterXSize="{nx}" rasterYSize="{ny}">
      <GeoTransform>{",".join(str(x) for x in gt)}</GeoTransform>
      <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>expression</PixelFunctionType>
        <PixelFunctionArguments expression="1.7*_CENTER_X_ + _CENTER_Y_"/>
        <SimpleSource>
           <SourceFilename>data/byte.tif</SourceFilename>
           <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    actual = gdal.Open(xml).ReadAsArray()

    expected = np.zeros((ny, nx), np.float32)
    for row in range(ny):
        for col in range(nx):
            x, y = gdal.ApplyGeoTransform(gt, col + 0.5, row + 0.5)
            expected[row, col] = 1.7 * x + y

    np.testing.assert_array_equal(actual, expected)


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_expression_coordinates_no_geotransform():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("muparser not available")

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>expression</PixelFunctionType>
        <PixelFunctionArguments expression="1.7*_CENTER_X_ + _CENTER_Y_"/>
        <SimpleSource>
           <SourceFilename>data/byte.tif</SourceFilename>
           <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with pytest.raises(Exception, match="VRTDataset must have a <GeoTransform>"):
        gdal.Open(xml).ReadRaster()


###############################################################################
# Test multiplication / summation by a constant factor


@pytest.mark.parametrize("fn", ["sum", "mul"])
def test_vrt_pixelfn_constant_factor(tmp_vsimem, fn):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    k = 7

    xml = f"""
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>{fn}</PixelFunctionType>
        <PixelFunctionArguments k="{k}" />
        <SimpleSource>
          <SourceFilename>data/byte.tif</SourceFilename>
          <SourceBand>1</SourceBand>
          <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
          <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    src = gdal.Open("data/byte.tif").ReadAsArray().astype(np.float32)
    dst = gdal.Open(xml).ReadAsArray()

    if fn == "sum":
        np.testing.assert_array_equal(dst, src + k)
    elif fn == "mul":
        np.testing.assert_array_equal(dst, src * k)


###############################################################################
# Test "area" pixel function


def test_vrt_pixelfn_area_geographic(tmp_vsimem):

    pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="10">
      <GeoTransform>-72, 0.1, 0, 44, 0, -0.1</GeoTransform>
      <SRS>EPSG:4326</SRS>
      <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>area</PixelFunctionType>
        <PixelFunctionArguments />
      </VRTRasterBand>
    </VRTDataset>"""

    ds = gdal.Open(xml)
    result = ds.ReadAsArray()

    poly_ul = ogr.CreateGeometryFromWkt(
        "POLYGON ((-72 43.9, -71.9 43.9, -71.9 44, -72 44, -72 43.9))",
        reference=ds.GetSpatialRef(),
    )
    assert result[0, 0] == poly_ul.GeodesicArea()

    poly_lr = ogr.CreateGeometryFromWkt(
        "POLYGON ((-70.1 43, -70 43, -70 43.1, -70.1 43.1, -70.1 43))",
        reference=ds.GetSpatialRef(),
    )
    assert result[-1, -1] == poly_lr.GeodesicArea()


def test_vrt_pixelfn_area_projected(tmp_vsimem):

    pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    # the <PixelFunctionArguments crs="3" /> tag is just to test that a user
    # cannot override a built-in argument. Since "crs" is a pointer, allowing
    # a user to override it would permit abitrary memory access.
    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="10">
      <GeoTransform>441500, 10, 0, 216600, 0, -10</GeoTransform>
      <SRS>EPSG:32145</SRS>
      <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>area</PixelFunctionType>
        <PixelFunctionArguments crs="3" />
      </VRTRasterBand>
    </VRTDataset>"""

    ds = gdal.Open(xml)
    result = ds.ReadAsArray()

    poly_ul = ogr.CreateGeometryFromWkt(
        "POLYGON ((441500 216590, 441510 216590, 441510 216600, 441500 216600, 441500 216590))",
        reference=ds.GetSpatialRef(),
    )
    assert result[0, 0] == poly_ul.GeodesicArea()

    poly_lr = ogr.CreateGeometryFromWkt(
        "POLYGON ((441690 216500, 441700 216500, 441700 216510, 441690 216510, 441690 216500))",
        reference=ds.GetSpatialRef(),
    )
    assert result[-1, -1] == poly_lr.GeodesicArea()


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_area_missing_crs():

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <GeoTransform>-72, 0.1, 0, 44, 0, -0.1</GeoTransform>
      <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>area</PixelFunctionType>
        <PixelFunctionArguments />
      </VRTRasterBand>
    </VRTDataset>"""

    ds = gdal.Open(xml)

    with pytest.raises(Exception, match="has no .SRS"):
        ds.ReadRaster()


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_area_missing_geotransform():

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <SRS>EPSG:4326</SRS>
      <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>area</PixelFunctionType>
        <PixelFunctionArguments />
      </VRTRasterBand>
    </VRTDataset>"""

    ds = gdal.Open(xml)

    with pytest.raises(Exception, match="has no .GeoTransform"):
        ds.ReadRaster()


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_area_unexpected_source():

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <GeoTransform>-72, 0.1, 0, 44, 0, -0.1</GeoTransform>
      <SRS>EPSG:4326</SRS>
      <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>area</PixelFunctionType>
        <PixelFunctionArguments />
        <SimpleSource>
           <SourceFilename>data/byte.tif</SourceFilename>
           <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    ds = gdal.Open(xml)

    with pytest.raises(Exception, match="unexpected source band"):
        ds.ReadRaster()


###############################################################################
# Test "quantile" pixel function


@pytest.mark.parametrize(
    "values,quantile",
    [
        ([6, 3, 5], 0.4),
        ([4, 5, 11], 0.5),
        ([9, 2], 0),
        ([9, 2], 1),
        ([100], 0),
        ([100], 1),
    ],
)
def test_vrt_pixelfn_quantile(tmp_vsimem, values, quantile):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 1, 1, len(values), gdal.GDT_Float64
    ) as src:
        for i in range(len(values)):
            src.GetRasterBand(i + 1).Fill(values[i])

    xml = f"""
    <VRTDataset rasterXSize="1" rasterYSize="1">
      <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>quantile</PixelFunctionType>
        <PixelFunctionArguments q="{quantile}" />
        {"".join(f'<SimpleSource><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></SimpleSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    with gdal.Open(xml) as ds:
        result = ds.ReadAsArray()[0, 0]
        assert result == np.quantile(values, quantile)


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_quantile_missing():

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>quantile</PixelFunctionType>
        <SimpleSource>
           <SourceFilename>data/byte.tif</SourceFilename>
           <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with pytest.raises(Exception, match="q must be specified"):
        with gdal.Open(xml) as ds:
            ds.ReadRaster()


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_quantile_complex_input():

    xml = """
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>quantile</PixelFunctionType>
        <PixelFunctionArguments q="0.3"/>
        <SimpleSource>
           <SourceFilename>../gcore/data/cfloat32.tif</SourceFilename>
           <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with pytest.raises(Exception, match="Complex data types not supported"):
        with gdal.Open(xml) as ds:
            ds.ReadRaster()


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("q", (-3, float("nan"), float("inf"), "0.12.1"))
def test_vrt_pixelfn_quantile_invalid(q):

    xml = f"""
    <VRTDataset rasterXSize="20" rasterYSize="20">
      <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
        <PixelFunctionType>quantile</PixelFunctionType>
        <PixelFunctionArguments q="{q}"/>
        <SimpleSource>
           <SourceFilename>data/byte.tif</SourceFilename>
           <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with pytest.raises(Exception, match="q must be between 0 and 1"):
        with gdal.Open(xml) as ds:
            ds.ReadRaster()


###############################################################################
# Test reclassification


@pytest.mark.parametrize("default", (7, "NO_DATA", 200, "PASS_THROUGH"))
def test_vrt_pixelfn_reclassify(tmp_vsimem, default):
    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    nx = 3
    ny = 5

    data = np.arange(nx * ny).reshape(ny, nx)

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", nx, ny, 1) as src:
        src.WriteArray(data)

    xml = f"""
    <VRTDataset rasterXSize="{nx}" rasterYSize="{ny}">
      <VRTRasterBand dataType="Float32" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>reclassify</PixelFunctionType>
        <PixelFunctionArguments mapping=" (-inf, 1)=8; 2=9 ; (3,5]=4; NO_DATA=123; [8,9]=PASS_THROUGH; 10=NO_DATA; [11, Inf] = 11; default={default}"/>
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
        <NoDataValue>7</NoDataValue>
      </VRTRasterBand>
    </VRTDataset>"""

    dst = gdal.Open(xml).ReadAsArray()

    if default == 200:
        np.testing.assert_array_equal(
            dst,
            np.array(
                [[8, 200, 9], [200, 4, 4], [200, 123, 8], [9, 7, 11], [11, 11, 11]]
            ),
        )
    elif default in (7, "NO_DATA"):
        np.testing.assert_array_equal(
            dst, np.array([[8, 7, 9], [7, 4, 4], [7, 123, 8], [9, 7, 11], [11, 11, 11]])
        )
    elif default == "PASS_THROUGH":
        np.testing.assert_array_equal(
            dst,
            np.array([[8, 1, 9], [3, 4, 4], [6, 123, 8], [9, 7, 11], [11, 11, 11]]),
        )
    else:
        pytest.fail()


@gdaltest.enable_exceptions()
def test_vrt_pixelfn_reclassify_no_default(tmp_vsimem):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    nx = 2
    ny = 3

    data = np.arange(nx * ny).reshape(ny, nx)

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", nx, ny, 1) as src:
        src.WriteArray(data)

    xml = f"""
    <VRTDataset rasterXSize="{nx}" rasterYSize="{ny}">
      <VRTRasterBand dataType="Float32" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>reclassify</PixelFunctionType>
        <PixelFunctionArguments mapping="1=2;3=4"/>
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with pytest.raises(
        Exception, match="Encountered value .* with no specified mapping"
    ):
        gdal.Open(xml).ReadAsArray()


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "mapping,error",
    [
        ("1=2;3", "expected '='"),
        ("1=2;3=4g", "expected ';' or end"),
        ("1=2;q", "Interval must start with"),
        ("1=3;3=256", "cannot be represented"),
        ("(1, }=3;3=4,", "Interval must end with"),
        ("(1,22k}=3;3=4,", "Interval must end with"),
        ("3= ", "expected number or NO_DATA"),
        ("1=NO_DATA", "NoData value is not set"),
        ("NO_DATA=15", "NoData value is not set"),
        ("[1,3]=7;[3, 5]=8", "Interval .* overlaps"),
        ("[1,3]=7;[2, 4]=8", "Interval .* overlaps"),
        ("[1,NaN]=0", "NaN is not a valid value for bounds of interval"),
        ("[NaN,1]=0", "NaN is not a valid value for bounds of interval"),
        ("[2,1]", "Lower bound of interval must be lower or equal to upper bound"),
    ],
)
def test_vrt_pixelfn_reclassify_invalid_mapping(tmp_vsimem, mapping, error):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    nx = 2
    ny = 3

    data = np.arange(nx * ny).reshape(ny, nx)

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", nx, ny, 1) as src:
        src.WriteArray(data)

    xml = f"""
    <VRTDataset rasterXSize="{nx}" rasterYSize="{ny}">
      <VRTRasterBand dataType="Byte" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>reclassify</PixelFunctionType>
        <PixelFunctionArguments mapping="{mapping}" default="7" />
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with pytest.raises(Exception, match=error):
        gdal.Open(xml).ReadAsArray()


def test_vrt_pixelfn_reclassify_nan(tmp_vsimem):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 2, 1, 1, gdal.GDT_Float32
    ) as src:
        src.WriteArray(np.array([[0, float("nan")]]))

    xml = f"""
    <VRTDataset rasterXSize="2" rasterYSize="1">
      <VRTRasterBand dataType="Float32" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>reclassify</PixelFunctionType>
        <PixelFunctionArguments mapping="0=1 ; nan=2" />
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    dst = gdal.Open(xml).ReadAsArray()
    np.testing.assert_array_equal(
        dst,
        np.array([[1, 2]]),
    )


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "pixelfn,values,nodata_value,pixelfn_args,expected",
    [
        ("argmax", [3, 7, 9, 2], 7, {}, 3),
        ("argmax", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("argmax", [7, 7, 7], 7, {}, 7),
        ("argmin", [3, 1, 7], 7, {}, 2),
        ("argmin", [3, 1, 7], 7, {"propagateNoData": True}, 7),
        ("argmin", [7, 7, 7], 7, {}, 7),
        ("dB", [7], 7, {}, 7),
        ("diff", [3, 7], 7, {}, 7),
        ("diff", [7, 3], 7, {}, 7),
        ("div", [3, 7], 7, {}, 7),
        ("div", [7, 3], 7, {}, 7),
        ("exp", [7], 7, {}, 7),
        ("geometric_mean", [3, 7, 9], 7, {}, math.sqrt(3 * 9)),
        ("geometric_mean", [7, 7, 7], 7, {}, 7),
        ("geometric_mean", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("harmonic_mean", [3, 7, 9], 7, {}, 2 / (1 / 3 + 1 / 9)),
        ("harmonic_mean", [3, 7, 0], 7, {}, 7),  # divide by zero => NoData
        ("harmonic_mean", [3, 7, 0], 7, {"propagateZero": True}, 0),
        ("harmonic_mean", [7, 7, 7], 7, {}, 7),
        ("harmonic_mean", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("interpolate_linear", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 5}, 7),
        ("interpolate_linear", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": -1}, 7),
        ("interpolate_linear", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 10}, 10),
        ("interpolate_linear", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 11}, 11),
        ("interpolate_linear", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 20}, 20),
        ("interpolate_exp", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 5}, 7),
        ("interpolate_exp", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": -1}, 7),
        ("interpolate_exp", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 10}, 10),
        ("interpolate_exp", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 11}, 10.717734),
        ("interpolate_exp", [7, 10, 20, 7], 7, {"t0": 0, "dt": 10, "t": 20}, 20),
        ("inv", [7], 7, {}, 7),
        ("inv", [float("nan")], 7, {}, float("nan")),
        ("inv", [float("nan")], float("nan"), {}, float("nan")),
        ("log10", [7], 7, {}, 7),
        ("max", [3, 7, 9], 7, {}, 9),
        ("max", [3, 7, 9], 7, {"k": 10}, 10),
        ("max", [3, 7, 9], 7, {"k": 5}, 9),
        ("max", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("mean", [3, 7, 9], 7, {}, (3 + 9) / 2),
        ("mean", [7, 7, 7], 7, {}, 7),
        ("mean", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("median", [3, 7, 11], 7, {}, (3 + 11) / 2),
        ("median", [3, 7, 9, 11], 7, {}, 9),
        ("median", [7, 7, 7], 7, {}, 7),
        ("median", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("min", [3, 7, 9], 7, {}, 3),
        ("min", [3, 7, 9], 7, {"k": 5}, 3),
        ("min", [3, 7, 9], 7, {"k": 2}, 2),
        ("min", [7, 7, 7], 7, {"k": 2}, 2),
        ("min", [7, 7, 7], 7, {"k": 2, "propagateNoData": True}, 7),
        ("min", [7, 7, 7], 7, {}, 7),
        ("min", [3, float("nan"), 9], 7, {}, 3),
        ("min", [3, float("nan"), 9], 7, {"propagateNoData": True}, 7),  # should be 3?
        ("min", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("mode", [3, 7, 9, 9, 9], 7, {"propagateNoData": True}, 7),
        ("mode", [3, 7, 9, 9, 9], 7, {}, 9),
        ("mode", [3, 7, float("nan"), float("nan")], 7, {}, float("nan")),
        ("mul", [3, 7, 9], 7, {}, 27),
        ("mul", [3, 7, 9], 7, {"propagateNoData": True}, 7),
        ("mul", [3, 7, float("nan")], 7, {}, float("nan")),
        ("mul", [3, 7, float("nan")], 7, {"propagateNoData": True}, 7),
        ("mul", [3, float("nan"), 9], float("nan"), {}, 27),
        (
            "mul",
            [3, float("nan"), 9],
            float("nan"),
            {"propagateNoData": True},
            float("nan"),
        ),
        ("norm_diff", [3, 7], 7, {}, 7),
        ("norm_diff", [7, 3], 7, {}, 7),
        ("pow", [7], 7, {"power": 10}, 7),
        ("quantile", [7], 7, {"q": 0}, 7),
        ("quantile", [7], 7, {"q": 1}, 7),
        ("quantile", [7, 6, 1, 2], 7, {"q": 0.5}, 2),
        ("quantile", [7, 6, 1, 2], 7, {"q": 0.5, "propagateNoData": True}, 7),
        ("round", [7.1], 7.1, {}, 7.1),
        ("round", [7.1], 7.2, {}, 7),
        ("round", [3.14159], 7, {"digits": 3}, 3.142),
        ("round", [6253], 7, {"digits": -2}, 6300),
        ("round", [6253], 7, {"digits": "invalid"}, "Failed to parse .* digits"),
        ("round", [3, 4], 7, {}, "input must be a single band"),
        ("scale", [7], 7, {"scale": 5, "offset": 10}, 7),
        ("sqrt", [7], 7, {}, 7),
        ("sum", [3, 7, 9], 7, {}, 12),
        ("sum", [3, 7, 9], 7, {"propagateNoData": True}, 7),
    ],
)
def test_vrt_pixelfn_nodata(
    tmp_vsimem, pixelfn, values, nodata_value, pixelfn_args, expected
):

    pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 1, 1, len(values), gdal.GDT_Float64
    ) as src:
        for i in range(len(values)):
            src.GetRasterBand(i + 1).Fill(values[i])

    xml = f"""
    <VRTDataset rasterXSize="2" rasterYSize="1">
      <VRTRasterBand dataType="Float32" band="1" subclass="VRTDerivedRasterBand">
        <NoDataValue>{nodata_value}</NoDataValue>
        <PixelFunctionType>{pixelfn}</PixelFunctionType>
        <PixelFunctionArguments {" ".join(f'{k}="{v}"' for k, v in pixelfn_args.items())} />
        {"".join(f'<SimpleSource><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></SimpleSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    ds = gdal.Open(xml)

    if type(expected) is str:
        with pytest.raises(Exception, match=expected):
            ds.ReadAsArray()
    else:
        result = ds.ReadAsArray()[0, 0]

        assert result == pytest.approx(expected, nan_ok=True)


@pytest.mark.parametrize("propagate", (True, False))
def test_vrt_pixelfn_complexsource_nodata(tmp_vsimem, propagate):

    pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    values = [1, 2, 3, 4, 5]

    src_nodata = 4
    dst_nodata = 99

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 1, 1, len(values)
    ) as src:
        for i, value in enumerate(values):
            src.GetRasterBand(i + 1).Fill(value)
            src.GetRasterBand(i + 1).SetNoDataValue(src_nodata)

    xml = f"""
    <VRTDataset rasterXSize="1" rasterYSize="1">
      <VRTRasterBand dataType="Float32" band="1" subclass="VRTDerivedRasterBand">
        <NoDataValue>{dst_nodata}</NoDataValue>
        <PixelFunctionType>sum</PixelFunctionType>
        <PixelFunctionArguments propagateNoData="{propagate}" />
        {"".join(f'<ComplexSource><NODATA>{src_nodata}</NODATA><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></ComplexSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    result = gdal.Open(xml).ReadAsArray()[0, 0]

    if propagate:
        assert result == dst_nodata
    else:
        assert result == 1 + 2 + 3 + 5


@pytest.mark.parametrize(
    "values",
    [
        [1],
        [255],
        [1, 255],
        [128, 255],
        [1, 3, 5],
        [1, 3, 6, 10, 19, 20, 21],
        [255] * 128,
        [255] * 32768,
    ],
)
def test_vrt_pixelfn_mean_byte(tmp_vsimem, values):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 63, 1, len(values), gdal.GDT_UInt8
    ) as src:
        for i in range(len(values)):
            src.GetRasterBand(i + 1).Fill(values[i])

    xml = f"""
    <VRTDataset rasterXSize="63" rasterYSize="1">
      <VRTRasterBand dataType="Byte" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        {"".join(f'<SimpleSource><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></SimpleSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    import struct

    assert (
        gdal.Open(xml).ReadRaster()
        == struct.pack("B", (sum(values) + len(values) // 2) // len(values)) * 63
    )


def test_vrt_pixelfn_mean_byte_image():

    xml = """
    <VRTDataset rasterXSize="400" rasterYSize="200">
      <VRTRasterBand dataType="Byte" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        <SimpleSource><SourceFilename>../gdrivers/data/small_world.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/small_world.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    src_array = src_ds.GetRasterBand(1).ReadRaster()
    result = gdal.Open(xml).ReadRaster()
    assert result == src_array

    xml = """
    <VRTDataset rasterXSize="400" rasterYSize="200">
      <VRTRasterBand dataType="Byte" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        <SimpleSource><SourceFilename>../gdrivers/data/small_world.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/small_world.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/small_world.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    result = gdal.Open(xml).ReadRaster()
    assert result == src_array


@pytest.mark.parametrize(
    "values", [[65535], [65534, 65535], [1, 65534, 65535], [65535] * 32768]
)
def test_vrt_pixelfn_mean_uint16(tmp_vsimem, values):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 31, 1, len(values), gdal.GDT_UInt16
    ) as src:
        for i in range(len(values)):
            src.GetRasterBand(i + 1).Fill(values[i])

    xml = f"""
    <VRTDataset rasterXSize="31" rasterYSize="1">
      <VRTRasterBand dataType="UInt16" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        {"".join(f'<SimpleSource><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></SimpleSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    import struct

    assert (
        gdal.Open(xml).ReadRaster()
        == struct.pack("H", (sum(values) + len(values) // 2) // len(values)) * 31
    )


def test_vrt_pixelfn_mean_uint16_image():

    xml = """
    <VRTDataset rasterXSize="50" rasterYSize="50">
      <VRTRasterBand dataType="UInt16" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        <SourceTransferType>UInt16</SourceTransferType>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    src_array = src_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)
    result = gdal.Open(xml).ReadRaster()
    assert result == src_array


@pytest.mark.parametrize(
    "values",
    [
        [32767],
        [32767, 32766],
        [-32768, -32766],
        [-32768, -32767, -32766],
        [-32768, 2, 32767],
        [-32768] * 32768,
    ],
)
def test_vrt_pixelfn_mean_int16(tmp_vsimem, values):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 31, 1, len(values), gdal.GDT_Int16
    ) as src:
        for i in range(len(values)):
            src.GetRasterBand(i + 1).Fill(values[i])

    xml = f"""
    <VRTDataset rasterXSize="31" rasterYSize="1">
      <VRTRasterBand dataType="Int16" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        {"".join(f'<SimpleSource><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></SimpleSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    import struct

    assert (
        gdal.Open(xml).ReadRaster()
        == struct.pack("h", (sum(values) + len(values) // 2) // len(values)) * 31
    )


def test_vrt_pixelfn_mean_int16_image():

    xml = """
    <VRTDataset rasterXSize="50" rasterYSize="50">
      <VRTRasterBand dataType="Int16" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        <SourceTransferType>Int16</SourceTransferType>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    src_array = src_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16)
    result = gdal.Open(xml).ReadRaster()
    assert result == src_array


@pytest.mark.parametrize(
    "values",
    [
        [12.5],
        [-12.5, 1],
        [1.5, 2.5, 3.5],
        [1.7014118346046923e38, 1.7014118346046923e38],
        [-1.7014118346046923e38, 1.7014118346046923e38],
        [float("inf"), float("inf")],
        [float("inf"), float("nan"), 0],
        [float("-inf"), float("-inf")],
        [float("inf"), 1e30, float("inf")],
        [float("inf"), float("inf"), float("inf")],
        [float("inf"), float("inf"), 1e30],
        [float("-inf"), 1e30, float("-inf")],
        [float("inf"), float("-inf")],
        [float("inf"), 1e30, float("-inf")],
        [0, float("nan")],
        [float("nan"), 0],
        [0, 0, float("nan")],
        [float("inf"), float("inf"), float("nan")],
    ],
)
def test_vrt_pixelfn_mean_float32(tmp_vsimem, values):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 31, 1, len(values), gdal.GDT_Float32
    ) as src:
        for i in range(len(values)):
            src.GetRasterBand(i + 1).Fill(values[i])

    xml = f"""
    <VRTDataset rasterXSize="31" rasterYSize="1">
      <VRTRasterBand dataType="Float32" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        {"".join(f'<SimpleSource><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></SimpleSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    import struct

    expected_mean = sum(values) / len(values)

    contiguous_data = gdal.Open(xml).ReadRaster()

    data = gdal.Open(xml).ReadRaster(buf_pixel_space=8)
    assert len(data) == 8 * (31 - 1) + 4
    data = b"".join([data[8 * i : 8 * i + 4] for i in range(31)])

    if math.isnan(expected_mean):
        for x in struct.unpack("f" * 31, contiguous_data):
            assert math.isnan(x)
        for x in struct.unpack("f" * 31, data):
            assert math.isnan(x)
    else:
        assert contiguous_data == struct.pack("f", expected_mean) * 31
        assert data == struct.pack("f", expected_mean) * 31


def test_vrt_pixelfn_mean_float32_image():

    xml = """
    <VRTDataset rasterXSize="50" rasterYSize="50">
      <VRTRasterBand dataType="Float32" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        <SourceTransferType>Float32</SourceTransferType>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    src_array = src_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Float32)
    result = gdal.Open(xml).ReadRaster()
    assert result == src_array


@pytest.mark.parametrize(
    "values",
    [
        [12.5],
        [-12.5, 1],
        [1.5, 2.5, 3.5],
        [sys.float_info.max, sys.float_info.max],
        [-sys.float_info.max, sys.float_info.max],
        [
            sys.float_info.max,
            sys.float_info.max,
            -sys.float_info.max,
            -sys.float_info.max,
        ],
        [float("inf"), float("inf")],
        [float("inf"), float("nan"), 0],
        [float("-inf"), float("-inf")],
        [float("inf"), sys.float_info.max, float("inf")],
        [float("inf"), float("inf"), float("inf")],
        [float("inf"), float("inf"), sys.float_info.max],
        [float("-inf"), sys.float_info.max, float("-inf")],
        [float("inf"), float("-inf")],
        [float("inf"), sys.float_info.max, float("-inf")],
        [0, float("nan")],
        [float("nan"), 0],
        [0, 0, float("nan")],
        [float("inf"), float("inf"), float("nan")],
    ],
)
def test_vrt_pixelfn_mean_float64(tmp_vsimem, values):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 31, 1, len(values), gdal.GDT_Float64
    ) as src:
        for i in range(len(values)):
            src.GetRasterBand(i + 1).Fill(values[i])

    xml = f"""
    <VRTDataset rasterXSize="31" rasterYSize="1">
      <VRTRasterBand dataType="Float64" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        {"".join(f'<SimpleSource><SourceFilename>{tmp_vsimem / "src.tif"}</SourceFilename><SourceBand>{i + 1}</SourceBand></SimpleSource>' for i in range(len(values)))}
      </VRTRasterBand>
    </VRTDataset>"""

    import struct

    if (
        len(values) == 2
        and values[0] == sys.float_info.max
        and values[1] == sys.float_info.max
    ):
        expected_mean = values[0]
    elif (
        len(values) == 4
        and values[0] == sys.float_info.max
        and values[1] == sys.float_info.max
        and values[2] == -sys.float_info.max
        and values[3] == -sys.float_info.max
    ):
        expected_mean = 0
    else:
        expected_mean = sum(values) / len(values)
    if math.isnan(expected_mean):
        for x in struct.unpack("d" * 31, gdal.Open(xml).ReadRaster()):
            assert math.isnan(x)
    else:
        assert gdal.Open(xml).ReadRaster() == struct.pack("d", expected_mean) * 31


def test_vrt_pixelfn_mean_float64_image():

    xml = """
    <VRTDataset rasterXSize="50" rasterYSize="50">
      <VRTRasterBand dataType="Float64" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>mean</PixelFunctionType>
        <SourceTransferType>Float64</SourceTransferType>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    src_array = src_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Float64)
    result = gdal.Open(xml).ReadRaster()
    assert result == src_array


@pytest.mark.parametrize("dt", ["Byte", "UInt16", "Int16", "Float32", "Float64"])
@pytest.mark.parametrize("function", ["min", "max"])
def test_vrt_pixelfn_min_max_image(dt, function):

    if function == "min":
        insert1 = "<ComplexSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand><ScaleOffset>255</ScaleOffset><ScaleRatio>0</ScaleRatio></ComplexSource>"
        insert2 = ""
    else:
        insert1 = ""
        insert2 = "<ComplexSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand><ScaleOffset>0</ScaleOffset><ScaleRatio>0</ScaleRatio></ComplexSource>"

    xml = f"""
    <VRTDataset rasterXSize="50" rasterYSize="50">
      <VRTRasterBand dataType="{dt}" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>{function}</PixelFunctionType>
        <SourceTransferType>{dt}</SourceTransferType>
        {insert1}
        <SimpleSource><SourceFilename>../gdrivers/data/rgbsmall.tif</SourceFilename><SourceBand>1</SourceBand></SimpleSource>
        {insert2}
      </VRTRasterBand>
    </VRTDataset>"""

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    src_array = src_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GetDataTypeByName(dt))
    result = gdal.Open(xml).ReadRaster()
    assert result == src_array


@pytest.mark.parametrize(
    "src_type",
    [
        "Byte",
        "Int8",
        "UInt16",
        "Int16",
        "UInt32",
        "Int32",
        "UInt64",
        "Int64",
        "Float16",
        "Float32",
        "Float64",
        "CInt16",
        "CInt32",
        "CFloat16",
        "CFloat32",
        "CFloat64",
    ],
)
@pytest.mark.parametrize(
    "dst_type", ["Byte", "UInt16", "Int16", "UInt32", "Int32", "Float32", "Float64"]
)
@pytest.mark.parametrize(
    "transfer_type",
    [
        None,
        "Byte",
        "UInt16",
        "Int16",
        "UInt32",
        "Int32",
        "Float32",
        "Float64",
    ],
)
def test_vrt_pixelfn_sum_optimization(tmp_vsimem, src_type, dst_type, transfer_type):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    width = 64 + 3
    height = 2

    ar1 = np.array([np.arange(width), np.arange(width) + 10])
    ar2 = np.array([np.arange(width) + 5, np.arange(width) + 15])
    if (
        src_type.startswith("Float")
        and dst_type.startswith("Float")
        and (not transfer_type or transfer_type.startswith("Float"))
    ):
        ar1 = ar1 / 2
        ar2 = ar2 / 4
        constant = 3.5
        constant_res = 3.5
    elif (
        not src_type.startswith("Float")
        and not dst_type.startswith("Float")
        and (not transfer_type or not transfer_type.startswith("Float"))
    ):
        constant = 3.2
        constant_res = 3
    else:
        constant = 3
        constant_res = 3

    big_constant = transfer_type is None
    if big_constant:
        constant = 255
        constant_res = 255

    src_type = gdal.GetDataTypeByName(src_type)

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src1.tif", width, height, 1, src_type
    ) as src:
        src.WriteArray(ar1)

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src2.tif", width, height, 1, src_type
    ) as src:
        src.WriteArray(ar2)

    if transfer_type:
        transfer = f"<SourceTransferType>{transfer_type}</SourceTransferType>"
    else:
        transfer = ""

    xml = f"""
    <VRTDataset rasterXSize="{width}" rasterYSize="{height}">
      <VRTRasterBand dataType="{dst_type}" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>sum</PixelFunctionType>
        <PixelFunctionArguments k="{constant}" />
        {transfer}
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src1.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src2.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    dst = gdal.Open(xml).ReadAsArray()
    if dst_type == "Byte" and big_constant:
        np.testing.assert_array_equal(dst, np.ones((height, width)) * constant)
    else:
        np.testing.assert_array_equal(dst, constant_res + ar1 + ar2)


def test_vrt_split_in_halves(tmp_vsimem):

    width = 1000
    height = 500

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src1.tif", width, height, 1, gdal.GDT_UInt8
    ) as src:
        src.WriteRaster(0, 0, width, height, b"\x01" * (width * height))

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src2.tif", width, height, 1, gdal.GDT_UInt8
    ) as src:
        src.WriteRaster(0, 0, width, height, b"\x02" * (width * height))

    xml = f"""
    <VRTDataset rasterXSize="{width}" rasterYSize="{height}">
      <VRTRasterBand dataType="Byte" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>sum</PixelFunctionType>
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src1.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src2.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    with gdal.config_option("VRT_DERIVED_DATASET_ALLOWED_RAM_USAGE", "1000"):
        got = gdal.Open(xml).ReadRaster()
        assert got == b"\x03" * (width * height)


def test_vrt_derived_virtual_overviews(tmp_vsimem):

    width = 2
    height = 2

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src1.tif", width, height, 1, gdal.GDT_UInt8
    ) as src:
        src.WriteRaster(0, 0, width, height, b"\x01" * (width * height))

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src2.tif", width, height, 1, gdal.GDT_UInt8
    ) as src:
        src.WriteRaster(0, 0, width, height, b"\x02" * (width * height))

    xml = f"""
    <VRTDataset rasterXSize="{width}" rasterYSize="{height}">
      <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src1.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
        </SimpleSource>
      </VRTRasterBand>
      <MaskBand>
          <VRTRasterBand dataType="Byte" subclass="VRTDerivedRasterBand">
            <PixelFunctionType>sum</PixelFunctionType>
            <SimpleSource>
              <SourceFilename>{tmp_vsimem / "src2.tif"}</SourceFilename>
              <SourceBand>1</SourceBand>
            </SimpleSource>
          </VRTRasterBand>
      </MaskBand>
      <OverviewList>2</OverviewList>
    </VRTDataset>"""

    ds = gdal.Open(xml)
    mask_band = ds.GetRasterBand(1).GetMaskBand()
    with gdaltest.error_raised(gdal.CE_None):
        got = mask_band.ReadRaster(0, 0, width, height, 1, 1)
    assert got == b"\x02"


def test_vrt_derived_zero_initialization(tmp_vsimem):

    vrt_w = 20
    vrt_h = 10
    tile_w = vrt_w // 2
    tile_h = vrt_h
    tile_offset = vrt_w - tile_w
    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src1.tif", tile_w, tile_h, 1, gdal.GDT_UInt8
    ) as src:
        src.WriteRaster(0, 0, tile_w, tile_h, b"\x01" * (tile_w * tile_h))

    xml = f"""
    <VRTDataset rasterXSize="{vrt_w}" rasterYSize="{vrt_h}">
      <VRTRasterBand dataType="Byte" band="1" subclass="VRTDerivedRasterBand">
        <PixelFunctionType>sum</PixelFunctionType>
        <SimpleSource>
          <SourceFilename>{tmp_vsimem / "src1.tif"}</SourceFilename>
          <SourceBand>1</SourceBand>
          <SrcRect xOff="0" yOff="0" xSize="{tile_w}" ySize="{tile_h}" />
          <DstRect xOff="{tile_offset}" yOff="0" xSize="{tile_w}" ySize="{tile_h}" />
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>"""

    buf_obj = bytearray(b"\xff" * ((vrt_h - 1) * vrt_w + tile_offset))
    got = gdal.Open(xml).ReadRaster(
        0, 0, tile_offset, vrt_h, buf_obj=buf_obj, buf_line_space=vrt_w
    )
    assert (
        got
        == (b"\x00" * tile_offset + b"\xff" * tile_w) * (vrt_h - 1)
        + b"\x00" * tile_offset
    )
