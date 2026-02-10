#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FlatGeobuf driver test suite.
# Author:   Björn Harrtell <bjorn@wololo.org>
#
###############################################################################
# Copyright (c) 2018-2019, Björn Harrtell <bjorn@wololo.org>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
from http.server import BaseHTTPRequestHandler

import gdaltest
import ogrtest
import pytest
import webserver

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("FlatGeobuf")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():
    yield
    gdaltest.clean_tmp()


### utils


def verify_flatgeobuf_copy(name, fids, names):

    assert gdaltest.features is not None, "Missing features collection"

    fname = os.path.join("tmp", name + ".fgb")
    ds = ogr.Open(fname)
    assert ds is not None, f"Can not open '{fname}'"

    lyr = ds.GetLayer(0)
    assert lyr is not None, "Missing layer"

    ######################################################
    # Test attributes
    ogrtest.check_features_against_list(lyr, "FID", fids)

    lyr.ResetReading()
    ogrtest.check_features_against_list(lyr, "NAME", names)

    ######################################################
    # Test geometries
    lyr.ResetReading()
    for i in range(len(gdaltest.features)):

        orig_feat = gdaltest.features[i]
        feat = lyr.GetNextFeature()

        assert feat is not None, "Failed trying to read feature"

        ogrtest.check_feature_geometry(
            feat, orig_feat.GetGeometryRef(), max_error=0.001
        )

    gdaltest.features = None

    lyr = None


def copy_shape_to_flatgeobuf(name, wkbType, compress=None, options=[]):

    if compress is not None:
        if compress[0:5] == "/vsig":
            dst_name = os.path.join("/vsigzip/", "tmp", name + ".fgb" + ".gz")
        elif compress[0:4] == "/vsiz":
            dst_name = os.path.join("/vsizip/", "tmp", name + ".fgb" + ".zip")
        elif compress == "/vsistdout/":
            dst_name = compress
        else:
            return False
    else:
        dst_name = os.path.join("tmp", name + ".fgb")

    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource(dst_name)
    if ds is None:
        return False

    ######################################################
    # Create layer
    lyr = ds.CreateLayer(name, None, wkbType, options)
    if lyr is None:
        return False

    ######################################################
    # Setup schema (all test shapefiles use common schema)
    ogrtest.quick_create_layer_def(lyr, [("FID", ogr.OFTReal), ("NAME", ogr.OFTString)])

    ######################################################
    # Copy in shp

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    src_name = os.path.join("data", "shp", name + ".shp")
    shp_ds = ogr.Open(src_name)
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.features = []

    while feat is not None:
        gdaltest.features.append(feat)

        dst_feat.SetFrom(feat)
        lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    shp_lyr = None
    lyr = None

    ds = None

    return True


### tests


def test_ogr_flatgeobuf_2():
    fgb_ds = ogr.Open("data/testfgb/poly.fgb")
    fgb_lyr = fgb_ds.GetLayer(0)

    assert fgb_lyr.TestCapability(ogr.OLCFastGetExtent)
    assert fgb_lyr.GetExtent() == (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    # test expected spatial filter feature count consistency
    assert fgb_lyr.TestCapability(ogr.OLCFastFeatureCount)
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(
        478315.531250, 4762880.500000, 481645.312500, 4765610.500000
    )
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(
        878315.531250, 4762880.500000, 881645.312500, 4765610.500000
    )
    c = fgb_lyr.GetFeatureCount()
    assert c == 0
    c = fgb_lyr.SetSpatialFilterRect(479586.0, 4764618.6, 479808.2, 4764797.8)
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5

    # check that ResetReading does not affect subsequent enumeration or filtering
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5
    fgb_lyr.ResetReading()
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5
    fgb_lyr.ResetReading()
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5


def test_ogr_flatgeobuf_2_1():
    fgb_ds = ogr.Open("data/testfgb/poly_no_index.fgb")
    fgb_lyr = fgb_ds.GetLayer(0)

    assert fgb_lyr.TestCapability(ogr.OLCFastGetExtent) is False
    assert fgb_lyr.GetExtent() == (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    # test expected spatial filter feature count consistency
    assert fgb_lyr.TestCapability(ogr.OLCFastFeatureCount) is False
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(
        478315.531250, 4762880.500000, 481645.312500, 4765610.500000
    )
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(
        878315.531250, 4762880.500000, 881645.312500, 4765610.500000
    )
    c = fgb_lyr.GetFeatureCount()
    assert c == 0
    c = fgb_lyr.SetSpatialFilterRect(479586.0, 4764618.6, 479808.2, 4764797.8)
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5

    # check that ResetReading does not affect subsequent enumeration or filtering
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5
    fgb_lyr.ResetReading()
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5
    fgb_lyr.ResetReading()
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5


def wktRoundtrip(in_wkt, expected_wkt):
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    g = ogr.CreateGeometryFromWkt(in_wkt)
    lyr = ds.CreateLayer("test", None, g.GetGeometryType(), [])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    actual = g.ExportToIsoWkt()
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")

    assert actual == expected_wkt


def test_ogr_flatgeobuf_3():
    wkts = ogrtest.get_wkt_data_series(
        with_z=True, with_m=True, with_gc=True, with_circular=True, with_surface=True
    )
    for wkt in wkts:
        wktRoundtrip(wkt, wkt)


# Run test_ogrsf
def test_ogr_flatgeobuf_8():
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/testfgb/poly.fgb"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


def test_ogr_flatgeobuf_9():

    gdaltest.tests = [
        ["gjpoint", [1], ["Point 1"], ogr.wkbPoint],
        ["gjline", [1], ["Line 1"], ogr.wkbLineString],
        ["gjpoly", [1], ["Polygon 1"], ogr.wkbPolygon],
        ["gjmultipoint", [1], ["MultiPoint 1"], ogr.wkbMultiPoint],
        ["gjmultiline", [2], ["MultiLine 1"], ogr.wkbMultiLineString],
        ["gjmultipoly", [2], ["MultiPoly 1"], ogr.wkbMultiPolygon],
    ]

    for i in range(len(gdaltest.tests)):
        test = gdaltest.tests[i]

        rc = copy_shape_to_flatgeobuf(test[0], test[3])
        assert rc, "Failed making copy of " + test[0] + ".shp"

        verify_flatgeobuf_copy(test[0], test[1], test[2])

    for i in range(len(gdaltest.tests)):
        test = gdaltest.tests[i]

        rc = copy_shape_to_flatgeobuf(test[0], test[3], None, ["SPATIAL_INDEX=NO"])
        assert rc, "Failed making copy of " + test[0] + ".shp"

        verify_flatgeobuf_copy(test[0], test[1], test[2])


# Test support for multiple layers in a directory


def test_ogr_flatgeobuf_directory():

    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/multi_layer")
    with gdal.quiet_errors():  # name will be laundered
        ds.CreateLayer("foo<", geom_type=ogr.wkbPoint)
    ds.CreateLayer("bar", geom_type=ogr.wkbPoint)
    ds = None

    ds = gdal.OpenEx("/vsimem/multi_layer")
    assert set(ds.GetFileList()) == set(
        ["/vsimem/multi_layer/bar.fgb", "/vsimem/multi_layer/foo_.fgb"]
    )
    assert ds.GetLayer("foo<")
    assert ds.GetLayer("bar")
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/multi_layer")
    assert not gdal.VSIStatL("/vsimem/multi_layer")


def test_ogr_flatgeobuf_srs_epsg():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbPoint)
    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    srs_got = lyr.GetSpatialRef()
    assert srs_got.IsSame(srs)
    assert srs_got.GetAuthorityName(None) == "EPSG"
    assert srs_got.GetAuthorityCode(None) == "32631"
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


def test_ogr_flatgeobuf_srs_other_authority():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    srs = osr.SpatialReference()
    srs.SetFromUserInput("ESRI:104009")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbPoint)
    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    srs_got = lyr.GetSpatialRef()
    assert srs_got.IsSame(srs)
    assert srs_got.GetAuthorityName(None) == "ESRI"
    assert srs_got.GetAuthorityCode(None) == "104009"
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


def test_ogr_flatgeobuf_srs_no_authority():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=longlat +ellps=clrk66")
    ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbPoint)
    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    srs_got = lyr.GetSpatialRef()
    assert srs_got.IsSame(srs)
    assert srs_got.GetAuthorityName(None) is None
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


def test_ogr_flatgeobuf_datatypes():
    ds = ogr.Open("data/testfgb/testdatatypes.fgb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["int"] == 1
    assert f["int64"] == 1234567890123
    assert f["double"] == 1.25
    assert f["string"] == "my string"
    assert f["datetime"] == "2019/10/15 12:34:56.789+00"


def test_ogr_flatgeobuf_alldatatypes():
    ds = ogr.Open("data/testfgb/alldatatypes.fgb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["byte"] == -1
    assert f["ubyte"] == 255
    assert f["bool"] == 1
    assert f["short"] == -1
    assert f["ushort"] == 65535
    assert f["int"] == -1
    assert f["uint"] == 4294967295
    assert f["long"] == -1
    assert f["ulong"] == float(2**64 - 1)
    assert f["float"] == 0
    assert f["double"] == 0
    assert f["string"] == "X"
    assert f["json"] == "X"
    assert f["datetime"] == "2020/02/29 12:34:56+00"
    assert f.GetFieldAsBinary("binary") == b"\x58"


def test_ogr_flatgeobuf_mixed():
    srcDS = gdal.OpenEx("data/testfgb/testmixed.geojson")
    destDS = gdal.VectorTranslate(
        "/vsimem/test.fgb",
        srcDS=srcDS,
        format="FlatGeobuf",
        layerCreationOptions=["SPATIAL_INDEX=NO"],
    )
    srcDS = None
    destDS = None
    srcDS = ogr.Open("data/testfgb/testmixed.geojson")
    destDS = ogr.Open("/vsimem/test.fgb")
    srcLyr = srcDS.GetLayer(0)
    destLyr = destDS.GetLayer(0)
    assert destLyr.TestCapability(ogr.OLCFastFeatureCount)
    assert destLyr.TestCapability(ogr.OLCFastGetExtent)
    assert destLyr.GetFeatureCount(force=0) == srcLyr.GetFeatureCount()
    assert destLyr.GetExtent(force=0) == srcLyr.GetExtent()
    ogrtest.compare_layers(srcLyr, destLyr)

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


###############################################################################
do_log = False


class WFSHTTPHandler(BaseHTTPRequestHandler):
    def log_request(self, code="-", size="-"):
        pass

    def do_GET(self):

        try:
            if do_log:
                f = open("/tmp/log.txt", "a")
                f.write("GET %s\n" % self.path)
                f.close()

            if self.path.find("/fakewfs") != -1:

                if (
                    self.path == "/fakewfs?SERVICE=WFS&REQUEST=GetCapabilities"
                    or self.path
                    == "/fakewfs?SERVICE=WFS&REQUEST=GetCapabilities&ACCEPTVERSIONS=1.1.0,1.0.0"
                ):
                    self.send_response(200)
                    self.send_header("Content-type", "application/xml")
                    self.end_headers()
                    f = open("data/testfgb/wfs/get_capabilities.xml", "rb")
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if (
                    self.path
                    == "/fakewfs?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=topp:tasmania_water_bodies"
                ):
                    self.send_response(200)
                    self.send_header("Content-type", "application/xml")
                    self.end_headers()
                    f = open("data/testfgb/wfs/describe_feature_type.xml", "rb")
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if (
                    self.path
                    == "/fakewfs?OUTPUTFORMAT=application/flatgeobuf&SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=topp:tasmania_water_bodies"
                ):
                    self.send_response(200)
                    self.send_header("Content-type", "application/flatgeobuf")
                    self.end_headers()
                    f = open("data/testfgb/wfs/get_feature.fgb", "rb")
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

            return
        except IOError:
            pass

        self.send_error(404, "File Not Found: %s" % self.path)


@pytest.fixture(autouse=True, scope="module")
def ogr_wfs_init():
    gdaltest.wfs_drv = ogr.GetDriverByName("WFS")


def test_ogr_wfs_fake_wfs_server():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    process, port = webserver.launch(handler=WFSHTTPHandler)
    if port == 0:
        pytest.skip()

    try:
        with gdal.config_option("OGR_WFS_LOAD_MULTIPLE_LAYER_DEFN", "NO"):
            ds = ogr.Open(
                "WFS:http://127.0.0.1:%d/fakewfs?OUTPUTFORMAT=application/flatgeobuf"
                % port
            )
        assert ds is not None

        lyr = ds.GetLayerByName("topp:tasmania_water_bodies")
        assert lyr is not None

        assert lyr.GetName() == "topp:tasmania_water_bodies"

        feat = lyr.GetNextFeature()
        assert feat.GetField("CONTINENT") == "Australia"
        ogrtest.check_feature_geometry(
            feat,
            "MULTIPOLYGON (((146.232727 -42.157501,146.238007 -42.16111,146.24411 -42.169724,146.257202 -42.193329,146.272217 -42.209442,146.274689 -42.214165,146.27832 -42.21833,146.282471 -42.228882,146.282745 -42.241943,146.291351 -42.255836,146.290253 -42.261948,146.288025 -42.267502,146.282471 -42.269997,146.274994 -42.271111,146.266663 -42.270279,146.251373 -42.262505,146.246918 -42.258057,146.241333 -42.256111,146.23468 -42.257782,146.221344 -42.269165,146.210785 -42.274445,146.20163 -42.27417,146.196075 -42.271385,146.186646 -42.258057,146.188568 -42.252785,146.193298 -42.249443,146.200806 -42.248055,146.209137 -42.249168,146.217468 -42.248611,146.222473 -42.245277,146.22525 -42.240555,146.224121 -42.22805,146.224396 -42.221382,146.228302 -42.217216,146.231354 -42.212502,146.231628 -42.205559,146.219421 -42.186943,146.21637 -42.17028,146.216644 -42.16333,146.219696 -42.158607,146.225525 -42.156105,146.232727 -42.157501)))",
            max_error=0.00001,
        )

    finally:
        webserver.server_stop(process, port)


def test_ogr_flatgeobuf_bool_short_float_binary():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

    fld_defn = ogr.FieldDefn("bool", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("short", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("float", ogr.OFTReal)
    fld_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld_defn)

    lyr.CreateField(ogr.FieldDefn("bin", ogr.OFTBinary))

    f = ogr.Feature(lyr.GetLayerDefn())
    f["bool"] = True
    f["short"] = -32768
    f["float"] = 1.5
    f["bin"] = b"\x01\xff"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)

    # Field of size 0
    f = ogr.Feature(lyr.GetLayerDefn())
    f["bin"] = b""
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTInt16
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTReal
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetSubType() == ogr.OFSTFloat32
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTBinary
    f = lyr.GetNextFeature()
    assert f["bool"] == True
    assert f["short"] == -32768
    assert f["float"] == 1.5
    assert f.GetFieldAsBinary("bin") == b"\x01\xff"
    f = lyr.GetNextFeature()
    assert f.GetFieldAsBinary("bin") == b""
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


@pytest.mark.parametrize(
    "options", [[], ["SPATIAL_INDEX=NO"]], ids=["spatial_index", "no_spatial_index"]
)
def test_ogr_flatgeobuf_write_to_vsizip(options):

    srcDS = gdal.OpenEx("data/poly.shp")
    destDS = gdal.VectorTranslate(
        "/vsizip//vsimem/test.fgb.zip/test.fgb",
        srcDS=srcDS,
        format="FlatGeobuf",
        layerCreationOptions=options,
    )
    assert destDS is not None
    destDS = None
    destDS = ogr.Open("/vsizip//vsimem/test.fgb.zip/test.fgb")
    srcLyr = srcDS.GetLayer(0)
    dstLyr = destDS.GetLayer(0)
    assert dstLyr.GetFeatureCount() == srcLyr.GetFeatureCount()
    dstF = dstLyr.GetNextFeature()
    assert dstF is not None
    destDS = None
    gdal.Unlink("/vsimem/test.fgb.zip")


def test_ogr_flatgeobuf_huge_number_of_columns():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    for i in range(65536):
        assert (
            lyr.CreateField(ogr.FieldDefn("col%d" % i, ogr.OFTInteger))
            == ogr.OGRERR_NONE
        ), i
    with pytest.raises(
        Exception, match="Cannot create features with more than 65536 columns"
    ):
        lyr.CreateField(ogr.FieldDefn("col65536", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    for i in range(65536):
        f.SetField(i, i)
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    for i in range(65536):
        assert f.GetField(i) == i
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")


def test_ogr_flatgeobuf_column_metadata():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

    fld_defn = ogr.FieldDefn("int", ogr.OFTInteger)
    fld_defn.SetAlternativeName("an integer")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("str1", ogr.OFTString)
    fld_defn.SetComment("a comment")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("str2", ogr.OFTString)
    fld_defn.SetWidth(2)
    fld_defn.SetNullable(False)
    fld_defn.SetUnique(True)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("float1", ogr.OFTReal)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("float2", ogr.OFTReal)
    fld_defn.SetWidth(5)
    fld_defn.SetPrecision(3)
    lyr.CreateField(fld_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["int"] = 1
    f["str1"] = "test1"
    f["str2"] = "test2"
    f["float1"] = 1.1234
    f["float2"] = 12.123
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == "an integer"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetWidth() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(1).IsNullable() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(1).IsUnique() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetComment() == "a comment"
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetWidth() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(2).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(2).IsUnique() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTReal
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetWidth() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetPrecision() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTReal
    assert lyr.GetLayerDefn().GetFieldDefn(4).GetWidth() == 5
    assert lyr.GetLayerDefn().GetFieldDefn(4).GetPrecision() == 3
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


def test_ogr_flatgeobuf_editing():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    fld_defn = ogr.FieldDefn("int", ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("str1", ogr.OFTString)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    f["int"] = 1
    f["str1"] = "test1"
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 1)"))
    f["int"] = 2
    f["str1"] = "test2"
    lyr.CreateFeature(f)

    c = lyr.GetFeatureCount()
    assert c == 2

    ds = None

    ds = ogr.Open("/vsimem/test.fgb", update=1)
    lyr = ds.GetLayer(0)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 1)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    assert lyr.TestCapability(ogr.OLCDeleteFeature) == 1
    assert lyr.DeleteFeature(1) == 0
    with pytest.raises(Exception, match="Non existing feature"):
        lyr.DeleteFeature(1)
    assert lyr.TestCapability(ogr.OLCReorderFields) == 1
    # assert lyr.ReorderFields([0, 1]) == 0
    assert lyr.DeleteField(1) == 0
    f = lyr.GetFeature(0)
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (2 2)"))
    assert lyr.SetFeature(f) == 0
    lyr.ResetReading()

    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)

    c = lyr.GetFeatureCount()
    assert c == 2

    f = lyr.GetNextFeature()
    assert f is not None
    assert f.GetGeometryRef().ExportToWkt() == "POINT (2 2)"
    assert f[0] == 2
    assert f.GetFieldCount() == 1

    f = lyr.GetNextFeature()
    assert f is not None
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 1)"

    f = lyr.GetNextFeature()
    assert f is None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 1)"))
    with pytest.raises(Exception, match="not supported on read-only layer"):
        lyr.CreateFeature(f)

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


@pytest.mark.parametrize(
    "in_wkt,expected_wkt",
    [
        ("MULTIPOINT ((0 0), EMPTY)", "MULTIPOINT ((0 0))"),
        ("MULTILINESTRING ((0 0,1 1), EMPTY)", "MULTILINESTRING ((0 0,1 1))"),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)), EMPTY)",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
        ),
        (
            "GEOMETRYCOLLECTION (POINT (0 0), POINT EMPTY)",
            "GEOMETRYCOLLECTION (POINT (0 0))",
        ),
    ],
)
def test_ogr_flatgeobuf_multi_geometries_with_empty(in_wkt, expected_wkt):
    wktRoundtrip(in_wkt, expected_wkt)


def test_ogr_flatgeobuf_ossfuzz_bug_29462():
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

    fld_defn = ogr.FieldDefn("str", ogr.OFTString)
    lyr.CreateField(fld_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "X" * 100000
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "X"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["str"] == "X" * 100000
    f = lyr.GetNextFeature()
    assert f["str"] == "X"
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


###############################################################################
# Check that we don't crash or leak


@pytest.mark.parametrize(
    "filename",
    [
        "data/flatgeobuf/invalid_polyhedralsurface_of_curvepolygon.fgb",
        "data/flatgeobuf/invalid_compoundcurve_non_contiguous_curves.fgb",
        "data/flatgeobuf/invalid_curvepolygon_linestring_three_points.fgb",
        "data/flatgeobuf/invalid_multisurface_of_polyhedralsurface.fgb",
    ],
)
def test_ogr_flatgeobuf_read_invalid_geometries(filename):
    with gdal.quiet_errors():
        ds = gdal.OpenEx(filename)
        lyr = ds.GetLayer(0)
        with pytest.raises(Exception, match="Fatal error parsing feature"):
            for f in lyr:
                pass


###############################################################################
# Check that we can read multilinestrings with a single part, without the
# "ends" array (cf https://github.com/OSGeo/gdal/issues/10774)


@pytest.mark.parametrize(
    "filename",
    [
        "data/flatgeobuf/test_ogr_flatgeobuf_singlepart_mls_new.fgb",
    ],
)
def test_ogr_flatgeobuf_read_singlepart_mls_new(filename):
    with gdal.OpenEx(filename) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, "MULTILINESTRING ((0 0,1 1))")


###############################################################################


def test_ogr_flatgeobuf_read_coordinate_metadata_wkt():

    ds = gdal.OpenEx("data/flatgeobuf/test_ogr_flatgeobuf_coordinate_epoch.fgb")
    lyr = ds.GetLayer(0)
    got_srs = lyr.GetSpatialRef()
    assert got_srs is not None
    assert got_srs.IsGeographic()


###############################################################################


def test_ogr_flatgeobuf_coordinate_epoch():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(2021.3)

    filename = "/vsimem/test_ogr_flatgeobuf_coordinate_epoch.fgb"
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource(filename)
    ds.CreateLayer("foo", srs=srs)
    ds = None

    ds = gdal.OpenEx(filename)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4326"
    assert srs.GetCoordinateEpoch() == 2021.3
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource(filename)


###############################################################################


def test_ogr_flatgeobuf_coordinate_epoch_custom_wkt():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCRS["myTRF2021",
    DYNAMIC[
        FRAMEEPOCH[2010]],
    DATUM["myTRF2021",
        ELLIPSOID["GRS 1980",6378137,298.257222101,
            LENGTHUNIT["metre",1]]],
    PRIMEM["Greenwich",0,
        ANGLEUNIT["degree",0.0174532925199433]],
    CS[ellipsoidal,2],
        AXIS["geodetic latitude (Lat)",north,
            ORDER[1],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
            ORDER[2],
            ANGLEUNIT["degree",0.0174532925199433]]]""")
    srs.SetCoordinateEpoch(2021.3)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    filename = "/vsimem/test_ogr_flatgeobuf_coordinate_epoch.fgb"
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource(filename)
    ds.CreateLayer("foo", srs=srs)
    ds = None

    ds = gdal.OpenEx(filename)
    lyr = ds.GetLayer(0)
    got_srs = lyr.GetSpatialRef()
    assert got_srs.IsSame(srs)
    assert got_srs.GetCoordinateEpoch() == 2021.3
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource(filename)


###############################################################################


def test_ogr_flatgeobuf_invalid_output_filename():

    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/i_do/not_exist/my.fgb")
    with pytest.raises(Exception, match="Failed to create"):
        ds.CreateLayer("foo")


###############################################################################


@pytest.mark.parametrize(
    "layer_creation_options",
    [[], ["SPATIAL_INDEX=NO"]],
    ids=["regular", "no_spatial_index"],
)
def test_ogr_flatgeobuf_arrow_stream_numpy(layer_creation_options):
    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    ds = ogr.GetDriverByName("FlatGeoBuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=layer_creation_options)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1

    field = ogr.FieldDefn("str", ogr.OFTString)
    lyr.CreateField(field)

    field = ogr.FieldDefn("bool", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int16", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int32", ogr.OFTInteger)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int64", ogr.OFTInteger64)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float32", ogr.OFTReal)
    field.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float64", ogr.OFTReal)
    lyr.CreateField(field)

    field = ogr.FieldDefn("datetime", ogr.OFTDateTime)
    lyr.CreateField(field)

    field = ogr.FieldDefn("binary", ogr.OFTBinary)
    lyr.CreateField(field)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("bool", 1)
    f.SetField("int16", -12345)
    f.SetField("int32", 12345678)
    f.SetField("int64", 12345678901234)
    f.SetField("float32", 1.25)
    f.SetField("float64", 1.250123)
    f.SetField("str", "abc")
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    f.SetField("binary", b"\xde\xad")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)

    f2 = ogr.Feature(lyr.GetLayerDefn())
    f2.SetField("bool", 0)
    f2.SetField("int16", -123)
    f2.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(-1 2)"))
    lyr.CreateFeature(f2)

    ds = None
    ds = ogr.Open("/vsimem/test.fgb")
    lyr = ds.GetLayer(0)

    try:
        import pyarrow

        pyarrow.__version__
        has_pyarrow = True
    except ImportError:
        has_pyarrow = False
    if has_pyarrow:
        stream = lyr.GetArrowStreamAsPyArrow()
        batches = [batch for batch in stream]
        # print(batches)

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]

    assert batch.keys() == {
        "OGC_FID",
        "str",
        "bool",
        "int16",
        "int32",
        "int64",
        "float32",
        "float64",
        "datetime",
        "binary",
        "wkb_geometry",
    }

    assert batch["OGC_FID"][0] == 0
    for fieldname in ("bool", "int16", "int32", "int64", "float32", "float64"):
        assert batch[fieldname][0] == f.GetField(fieldname)
    assert batch["str"][0] == f.GetField("str").encode("utf-8")
    assert batch["datetime"][0] == numpy.datetime64("2022-05-31T12:34:56.789")
    assert bytes(batch["binary"][0]) == b"\xde\xad"
    assert len(bytes(batch["wkb_geometry"][0])) == 21

    assert batch["OGC_FID"][1] == 1
    assert batch["bool"][1] == False

    for options in ([], ["MAX_FEATURES_IN_BATCH=1"]):
        # Test attribute filter
        lyr.SetAttributeFilter("int16 = -123")
        stream = lyr.GetArrowStreamAsNumPy(options)
        batches = [batch for batch in stream]
        lyr.SetAttributeFilter(None)
        assert len(batches) == 1
        assert len(batches[0]["OGC_FID"]) == 1
        assert batches[0]["OGC_FID"][0] == 1
        assert batches[0]["int16"][0] == -123

        # Test spatial filter
        lyr.SetSpatialFilterRect(0, 0, 10, 10)
        stream = lyr.GetArrowStreamAsNumPy(options)
        batches = [batch for batch in stream]
        lyr.SetSpatialFilter(None)
        assert len(batches) == 1
        assert len(batches[0]["OGC_FID"]) == 1
        assert batches[0]["OGC_FID"][0] == 0
        assert batches[0]["int16"][0] == -12345

        # Test attribute + spatial filter: no result
        lyr.SetAttributeFilter("int16 = -123")
        lyr.SetSpatialFilterRect(0, 0, 10, 10)
        stream = lyr.GetArrowStreamAsNumPy(options)
        batches = [batch for batch in stream]
        lyr.SetAttributeFilter(None)
        lyr.SetSpatialFilter(None)
        assert len(batches) == 0

        # Test attribute + spatial filter: result
        lyr.SetAttributeFilter("int16 = -123")
        lyr.SetSpatialFilterRect(-1, 2, -1, 2)
        stream = lyr.GetArrowStreamAsNumPy(options)
        batches = [batch for batch in stream]
        lyr.SetAttributeFilter(None)
        lyr.SetSpatialFilter(None)
        assert len(batches) == 1
        assert len(batches[0]["int16"]) == 1
        assert batches[0]["OGC_FID"][0] == 1
        assert batches[0]["int16"][0] == -123

    # Test fast FID filtering
    lyr.SetAttributeFilter("FID IN (1, -2, 0)")
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetAttributeFilter(None)
    assert len(batches) == 1
    batch = batches[0]
    assert len(batch["OGC_FID"]) == 2
    assert set(batch["OGC_FID"]) == set([0, 1])

    lyr.SetAttributeFilter("FID = 2")
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetAttributeFilter(None)
    assert len(batches) == 0

    # Test ignored fields
    assert lyr.SetIgnoredFields(["OGR_GEOMETRY", "int16"]) == ogr.OGRERR_NONE
    stream = lyr.GetArrowStreamAsNumPy(options=["INCLUDE_FID=NO"])
    batches = [batch for batch in stream]
    lyr.SetIgnoredFields([])
    batch = batches[0]
    assert batch.keys() == {
        "str",
        "bool",
        "int32",
        "int64",
        "float32",
        "float64",
        "datetime",
        "binary",
    }

    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")


###############################################################################
# Test reading an empty file with GetArrowStream()


def test_ogr_flatgeobuf_arrow_stream_empty_file():

    ds = ogr.GetDriverByName("FlatGeoBuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    assert lyr.TestCapability(ogr.OLCFastGetArrowStream) == 1
    stream = lyr.GetArrowStream()
    assert stream.GetNextRecordBatch() is None
    del stream
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")


def test_ogr_flatgeobuf_issue_7401():
    # Verify null geom handling without spatial index
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["SPATIAL_INDEX=NO"])

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    ds = None

    ds = gdal.OpenEx("/vsimem/test.fgb", open_options=["VERIFY_BUFFERS=YES"])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert f is not None
    assert g is not None
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert f is not None
    assert g is None

    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")

    # Verify null geom handling with spatial index (not supported should error)
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource("/vsimem/test.fgb")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["SPATIAL_INDEX=YES"])

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    with pytest.raises(
        Exception, match="NULL geometry not supported with spatial index"
    ):
        lyr.CreateFeature(f)
    ds = None

    ogr.GetDriverByName("FlatGeobuf").DeleteDataSource("/vsimem/test.fgb")
    assert not gdal.VSIStatL("/vsimem/test.fgb")


###############################################################################
# Test reading and writing layer title, description and metadata


def test_ogr_flatgeobuf_title_description_metadata(tmp_vsimem):

    filename = str(tmp_vsimem / "test.fgb")
    ds = ogr.GetDriverByName("FlatGeobuf").CreateDataSource(filename)
    lyr = ds.CreateLayer(
        "test",
        geom_type=ogr.wkbPoint,
        options=["SPATIAL_INDEX=NO", "TITLE=title", "DESCRIPTION=description"],
    )
    lyr.SetMetadata({"foo": "bar", "bar": "baz"})
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata_Dict() == {
        "TITLE": "title",
        "DESCRIPTION": "description",
        "foo": "bar",
        "bar": "baz",
    }

    # Test that on FlatGeoBuf -> FlatGeoBuf TITLE and DESCRIPTION get properly propagated.
    filename2 = str(tmp_vsimem / "test2.fgb")
    ds = gdal.VectorTranslate(filename2, filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata_Dict() == {
        "TITLE": "title",
        "DESCRIPTION": "description",
        "foo": "bar",
        "bar": "baz",
    }


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_flatgeobuf_write_arrow(tmp_vsimem):

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = ds.CreateLayer("src_lyr")

    field_def = ogr.FieldDefn("field_bool", ogr.OFTInteger)
    field_def.SetSubType(ogr.OFSTBoolean)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integer", ogr.OFTInteger)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_int16", ogr.OFTInteger)
    field_def.SetSubType(ogr.OFSTInt16)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integer64", ogr.OFTInteger64)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_float32", ogr.OFTReal)
    field_def.SetSubType(ogr.OFSTFloat32)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_real", ogr.OFTReal)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_string", ogr.OFTString)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_binary", ogr.OFTBinary)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_date", ogr.OFTDate)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_time", ogr.OFTTime)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_datetime", ogr.OFTDateTime)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_boollist", ogr.OFTIntegerList)
    field_def.SetSubType(ogr.OFSTBoolean)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integerlist", ogr.OFTIntegerList)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_int16list", ogr.OFTIntegerList)
    field_def.SetSubType(ogr.OFSTInt16)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integer64list", ogr.OFTInteger64List)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_float32list", ogr.OFTRealList)
    field_def.SetSubType(ogr.OFSTFloat32)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_reallist", ogr.OFTRealList)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_stringlist", ogr.OFTStringList)
    src_lyr.CreateField(field_def)

    src_feature = ogr.Feature(src_lyr.GetLayerDefn())
    src_feature.SetField("field_bool", True)
    src_feature.SetField("field_integer", 17)
    src_feature.SetField("field_int16", -17)
    src_feature.SetField("field_integer64", 9876543210)
    src_feature.SetField("field_float32", 1.5)
    src_feature.SetField("field_real", 18.4)
    src_feature.SetField("field_string", "abc def")
    src_feature.SetFieldBinary("field_binary", b"\x00\x01")
    src_feature.SetField("field_binary", b"\x01\x23\x46\x57\x89\xab\xcd\xef")
    src_feature.SetField("field_date", "2011/11/11")
    src_feature.SetField("field_time", "14:10:35")
    src_feature.SetField("field_datetime", 2011, 11, 11, 14, 10, 35.123, 0)
    src_feature.field_boollist = [False, True]
    src_feature.field_integerlist = [10, 20, 30]
    src_feature.field_int16list = [10, -20, 30]
    src_feature.field_integer64list = [9876543210]
    src_feature.field_float32list = [1.5, -1.5]
    src_feature.field_reallist = [123.5, 567.0]
    src_feature.field_stringlist = ["abc", "def"]
    src_feature.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))

    src_lyr.CreateFeature(src_feature)

    filename = str(tmp_vsimem / "temp.fgb")
    dst_ds = ogr.GetDriverByName("FlatGeoBuf").CreateDataSource(filename)
    dst_lyr = dst_ds.CreateLayer("dst_lyr")

    stream = src_lyr.GetArrowStream(["INCLUDE_FID=NO"])
    schema = stream.GetSchema()

    success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
    assert success, error_msg

    for i in range(schema.GetChildrenCount()):
        if schema.GetChild(i).GetName() != "wkb_geometry":
            dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    dst_ds.Close()
    dst_ds = ogr.Open(filename)
    dst_lyr = dst_ds.GetLayer(0)
    dst_feature = dst_lyr.GetNextFeature()
    assert str(dst_feature) == """OGRFeature(dst_lyr):0
  field_bool (Integer(Boolean)) = 1
  field_integer (Integer) = 17
  field_int16 (Integer(Int16)) = -17
  field_integer64 (Integer64) = 9876543210
  field_float32 (Real(Float32)) = 1.5
  field_real (Real) = 18.4
  field_string (String) = abc def
  field_binary (Binary) = 0123465789ABCDEF
  field_date (DateTime) = 2011/11/11 00:00:00
  field_time (String) = 14:10:35
  field_datetime (DateTime) = 2011/11/11 14:10:35.123
  field_boollist (String) = [ 0, 1 ]
  field_integerlist (String) = [ 10, 20, 30 ]
  field_int16list (String) = [ 10, -20, 30 ]
  field_integer64list (String) = [ 9876543210 ]
  field_float32list (String) = [ 1.5, -1.5 ]
  field_reallist (String) = [ 123.5, 567.0 ]
  field_stringlist (String) = [ "abc", "def" ]
  POINT (1 2)

"""


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_flatgeobuf_write_mismatch_geom_type(tmp_vsimem):

    filename = str(tmp_vsimem / "temp.fgb")
    ds = ogr.GetDriverByName("FlatGeoBuf").CreateDataSource(filename)
    lyr = ds.CreateLayer("src_lyr", geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (1 2,3 4)"))
    with pytest.raises(
        Exception,
        match="ICreateFeature: Mismatched geometry type. Feature geometry type is Line String, expected layer geometry type is Point",
    ):
        lyr.CreateFeature(f)


###############################################################################
# Test OGRGenSQLResultLayer::GetArrowStream() implementation.
# There isn't much specific of the FlatGeoBuf driver, except it is the
# only one in a default build that implements OLCFastGetArrowStream and doesn't
# have a specialized ExecuteSQL() implementation.


@gdaltest.enable_exceptions()
def test_ogr_flatgeobuf_sql_arrow(tmp_vsimem):

    filename = str(tmp_vsimem / "temp.fgb")
    with ogr.GetDriverByName("FlatGeoBuf").CreateDataSource(filename) as ds:
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn("foo"))
        lyr.CreateField(ogr.FieldDefn("bar"))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["foo"] = "bar"
        f["bar"] = "baz"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f["foo"] = "bar2"
        f["bar"] = "baz2"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
        lyr.CreateFeature(f)

    with ogr.Open(filename) as ds:
        with ds.ExecuteSQL("SELECT 'a' FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

            tmp_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
            tmp_lyr = tmp_ds.CreateLayer("test")
            tmp_lyr.WriteArrow(lyr)
            f = tmp_lyr.GetNextFeature()
            assert f["FIELD_1"] == "a"

        with ds.ExecuteSQL("SELECT foo, foo FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT CONCAT(foo, 'x') FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT foo AS renamed, foo FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT bar, foo FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT CAST(foo AS float) FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT MIN(foo) FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT COUNT(*) FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT * FROM test a JOIN test b ON a.foo = b.foo") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT * FROM test OFFSET 1") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT * FROM test ORDER BY foo") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT *, OGR_STYLE HIDDEN FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT DISTINCT foo FROM test") as lyr:
            assert not lyr.TestCapability(ogr.OLCFastGetArrowStream)

        with ds.ExecuteSQL("SELECT * FROM test") as lyr:
            try:
                stream = lyr.GetArrowStreamAsNumPy()
            except ImportError:
                stream = None
        if stream:
            with pytest.raises(
                Exception,
                match=r"Calling get_next\(\) on a freed OGRLayer is not supported",
            ):
                [batch for batch in stream]

        sql = "SELECT foo, bar AS bar_renamed FROM test"
        with ds.ExecuteSQL(sql) as lyr:
            assert lyr.TestCapability(ogr.OLCFastGetArrowStream)

            tmp_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
            tmp_lyr = tmp_ds.CreateLayer("test")
            tmp_lyr.WriteArrow(lyr)
            assert tmp_lyr.GetLayerDefn().GetFieldCount() == 2
            assert tmp_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
            assert tmp_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "bar_renamed"
            assert tmp_lyr.GetFeatureCount() == 2
            f = tmp_lyr.GetNextFeature()
            assert f["foo"] == "bar2"
            assert f["bar_renamed"] == "baz2"
            assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"
            f = tmp_lyr.GetNextFeature()
            assert f["foo"] == "bar"
            assert f["bar_renamed"] == "baz"
            assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        sql = "SELECT bar FROM test LIMIT 1"
        with ds.ExecuteSQL(sql) as lyr:
            assert lyr.TestCapability(ogr.OLCFastGetArrowStream)

            tmp_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
            tmp_lyr = tmp_ds.CreateLayer("test")
            tmp_lyr.WriteArrow(lyr)
            assert tmp_lyr.GetLayerDefn().GetFieldCount() == 1
            assert tmp_lyr.GetFeatureCount() == 1
            f = tmp_lyr.GetNextFeature()
            assert f["bar"] == "baz2"
            assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"

        sql = "SELECT * EXCLUDE (\"_ogr_geometry_\") FROM test WHERE foo = 'bar'"
        with ds.ExecuteSQL(sql) as lyr:
            assert lyr.TestCapability(ogr.OLCFastGetArrowStream)

            tmp_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
            tmp_lyr = tmp_ds.CreateLayer("test")
            tmp_lyr.WriteArrow(lyr)
            assert tmp_lyr.GetFeatureCount() == 1
            f = tmp_lyr.GetNextFeature()
            assert f["foo"] == "bar"
            assert f["bar"] == "baz"
            assert f.GetGeometryRef() is None

        sql = "SELECT * FROM test"
        with ds.ExecuteSQL(sql) as lyr:
            lyr.SetSpatialFilterRect(1, 2, 1, 2)
            assert lyr.TestCapability(ogr.OLCFastGetArrowStream)

            tmp_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
            tmp_lyr = tmp_ds.CreateLayer("test")
            tmp_lyr.WriteArrow(lyr)
            assert tmp_lyr.GetLayerDefn().GetFieldCount() == 2
            assert tmp_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "foo"
            assert tmp_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "bar"
            assert tmp_lyr.GetFeatureCount() == 1
            f = tmp_lyr.GetNextFeature()
            assert f["foo"] == "bar"
            assert f["bar"] == "baz"
            assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
            f = tmp_lyr.GetNextFeature()


###############################################################################
# Test DATETIME_AS_STRING=YES GetArrowStream() option


def test_ogr_flatgeobuf_arrow_stream_numpy_datetime_as_string(tmp_vsimem):
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    filename = str(tmp_vsimem / "datetime_as_string.fgb")
    with ogr.GetDriverByName("FlatGeoBuf").CreateDataSource(filename) as ds:
        lyr = ds.CreateLayer("test")

        field = ogr.FieldDefn("datetime", ogr.OFTDateTime)
        lyr.CreateField(field)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("datetime", "2022-05-31T12:34:56.789Z")
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("datetime", "2022-05-31T12:34:56")
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("datetime", "2022-05-31T12:34:56+12:30")
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr.CreateFeature(f)

    with ogr.Open(filename) as ds:
        lyr = ds.GetLayer(0)
        stream = lyr.GetArrowStreamAsNumPy(
            options=["USE_MASKED_ARRAYS=NO", "DATETIME_AS_STRING=YES"]
        )
        batches = [batch for batch in stream]
        assert len(batches) == 1
        batch = batches[0]
        assert len(batch["datetime"]) == 4
        assert batch["datetime"][0] == b""
        assert batch["datetime"][1] == b"2022-05-31T12:34:56.789Z"
        assert batch["datetime"][2] == b"2022-05-31T12:34:56"
        assert batch["datetime"][3] == b"2022-05-31T12:34:56+12:30"


def test_ogr_flatgeobuf_write_empty_geometries_no_spatial_index(tmp_vsimem):
    # Verify empty geom handling without spatial index
    out_filename = str(tmp_vsimem / "out.fgb")
    with ogr.GetDriverByName("FlatGeobuf").CreateDataSource(out_filename) as ds:
        lyr = ds.CreateLayer(
            "test", geom_type=ogr.wkbPolygon, options=["SPATIAL_INDEX=NO"]
        )

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON EMPTY"))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)

    with gdal.OpenEx(out_filename) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        assert g is None


def test_ogr_flatgeobuf_write_empty_file_no_spatial_index(tmp_vsimem):
    # Verify empty geom handling without spatial index
    out_filename = str(tmp_vsimem / "out.fgb")
    with ogr.GetDriverByName("FlatGeobuf").CreateDataSource(out_filename) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        ds.CreateLayer(
            "test", geom_type=ogr.wkbPolygon, srs=srs, options=["SPATIAL_INDEX=NO"]
        )

    with gdal.OpenEx(out_filename) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 0
        assert lyr.GetExtent(can_return_null=True) is None
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
