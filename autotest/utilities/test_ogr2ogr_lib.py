#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  librarified ogr2ogr testing
# Author:   Faza Mahamood <fazamhd @ gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
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

import tempfile

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################
# Simple test


def test_ogr2ogr_lib_1():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="Memory")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    feat0 = ds.GetLayer(0).GetFeature(0)
    assert (
        feat0.GetFieldAsDouble("AREA") == 215229.266
    ), "Did not get expected value for field AREA"
    assert (
        feat0.GetFieldAsString("PRFEDEA") == "35043411"
    ), "Did not get expected value for field PRFEDEA"


###############################################################################
# Test SQLStatement


def test_ogr2ogr_lib_2():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="Memory",
        SQLStatement="select * from poly",
        SQLDialect="OGRSQL",
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    # Test @filename syntax
    gdal.FileFromMemBuffer(
        "/vsimem/sql.txt", "-- initial comment\nselect * from poly\n-- trailing comment"
    )
    ds = gdal.VectorTranslate(
        "", srcDS, format="Memory", SQLStatement="@/vsimem/sql.txt"
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    gdal.Unlink("/vsimem/sql.txt")

    # Test @filename syntax with a UTF-8 BOM
    gdal.FileFromMemBuffer(
        "/vsimem/sql.txt", "\xEF\xBB\xBFselect * from poly".encode("LATIN1")
    )
    ds = gdal.VectorTranslate(
        "", srcDS, format="Memory", SQLStatement="@/vsimem/sql.txt"
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    gdal.Unlink("/vsimem/sql.txt")


###############################################################################
# Test WHERE


def test_ogr2ogr_lib_3():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="Memory", where="EAS_ID=171")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1

    # Test @filename syntax
    gdal.FileFromMemBuffer("/vsimem/filter.txt", "EAS_ID=171")
    ds = gdal.VectorTranslate("", srcDS, format="Memory", where="@/vsimem/filter.txt")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    gdal.Unlink("/vsimem/filter.txt")


###############################################################################
# Test accessMode


def test_ogr2ogr_lib_4():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("/vsimem/poly.shp", srcDS)
    assert ds.GetLayer(0).GetFeatureCount() == 10, "wrong feature count"
    ds = None

    ds = gdal.VectorTranslate("/vsimem/poly.shp", srcDS, accessMode="append")
    assert ds is not None, "ds is None"
    assert ds.GetLayer(0).GetFeatureCount() == 20, "wrong feature count"

    ret = gdal.VectorTranslate(ds, srcDS, accessMode="append")
    assert ret == 1, "ds is None"
    assert ds.GetLayer(0).GetFeatureCount() == 30, "wrong feature count"

    feat10 = ds.GetLayer(0).GetFeature(10)
    assert (
        feat10.GetFieldAsDouble("AREA") == 215229.266
    ), "Did not get expected value for field AREA"
    assert (
        feat10.GetFieldAsString("PRFEDEA") == "35043411"
    ), "Did not get expected value for field PRFEDEA"

    ds = None
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource("/vsimem/poly.shp")


###############################################################################
# Test dstSRS


def test_ogr2ogr_lib_5():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="Memory", dstSRS="EPSG:4326")
    assert str(ds.GetLayer(0).GetSpatialRef()).find("1984") != -1


###############################################################################
# Test selFields


def test_ogr2ogr_lib_6():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    # Voluntary don't use the exact case of the source field names (#4502)
    ds = gdal.VectorTranslate(
        "", srcDS, format="Memory", selectFields=["eas_id", "prfedea"]
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsDouble("EAS_ID") == 168
    assert feat.GetFieldAsString("PRFEDEA") == "35043411"


###############################################################################
# Test LCO


def test_ogr2ogr_lib_7():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(
        "/vsimem/poly.shp", srcDS, layerCreationOptions=["SHPT=POLYGONZ"]
    )
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D

    ds = None
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource("/vsimem/poly.shp")


###############################################################################
# Add explicit source layer name


def test_ogr2ogr_lib_8():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="Memory", layers=["poly"])
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    # Test also with just a string and not an array
    ds = gdal.VectorTranslate("", srcDS, format="Memory", layers="poly")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test -segmentize


def test_ogr2ogr_lib_9():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="Memory", segmentizeMaxDist=100)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryRef(0).GetPointCount() == 36


###############################################################################
# Test overwrite with a shapefile


def test_ogr2ogr_lib_10():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("/vsimem/tmp/poly.shp", srcDS)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds = None

    # Overwrite
    ds = gdal.VectorTranslate("/vsimem/tmp", srcDS, accessMode="overwrite")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource("/vsimem/tmp")


###############################################################################
# Test filter


def test_ogr2ogr_lib_11():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(
        "", srcDS, format="Memory", spatFilter=[479609, 4764629, 479764, 4764817]
    )
    if ogrtest.have_geos():
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4
    else:
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 5


###############################################################################
# Test callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_ogr2ogr_lib_12():

    tab = [0]
    ds = gdal.VectorTranslate(
        "",
        "../ogr/data/poly.shp",
        format="Memory",
        callback=mycallback,
        callback_data=tab,
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    assert tab[0] == 1.0, "Bad percentage"


###############################################################################
# Test callback with failure


def mycallback_with_failure(pct, msg, user_data):
    # pylint: disable=unused-argument
    if pct > 0.5:
        return 0
    return 1


def test_ogr2ogr_lib_13():

    with pytest.raises(Exception):
        gdal.VectorTranslate(
            "",
            "../ogr/data/poly.shp",
            format="Memory",
            callback=mycallback_with_failure,
        )


###############################################################################
# Test internal wrappers


def test_ogr2ogr_lib_14():

    # Null dest name and no option
    with pytest.raises(Exception):
        gdal.wrapper_GDALVectorTranslateDestName(
            None, gdal.OpenEx("../ogr/data/poly.shp"), None
        )


###############################################################################
# Test non existing zfield


def test_ogr2ogr_lib_15():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    with gdaltest.error_handler():
        ds = gdal.VectorTranslate("", srcDS, format="Memory", zField="foo")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon


###############################################################################
# Test -dim


def test_ogr2ogr_lib_16():

    tests = [
        ["POINT M (1 2 3)", None, "POINT M (1 2 3)"],
        ["POINT M (1 2 3)", "XY", "POINT (1 2)"],
        ["POINT M (1 2 3)", "XYZ", "POINT Z (1 2 0)"],
        ["POINT M (1 2 3)", "XYM", "POINT M (1 2 3)"],
        ["POINT M (1 2 3)", "XYZM", "POINT ZM (1 2 0 3)"],
        ["POINT M (1 2 3)", "layer_dim", "POINT M (1 2 3)"],
        ["POINT ZM (1 2 3 4)", None, "POINT ZM (1 2 3 4)"],
        ["POINT ZM (1 2 3 4)", "XY", "POINT (1 2)"],
        ["POINT ZM (1 2 3 4)", "XYZ", "POINT Z (1 2 3)"],
        ["POINT ZM (1 2 3 4)", "XYM", "POINT M (1 2 4)"],
        ["POINT ZM (1 2 3 4)", "XYZM", "POINT ZM (1 2 3 4)"],
        ["POINT ZM (1 2 3 4)", "layer_dim", "POINT ZM (1 2 3 4)"],
    ]
    for (wkt_before, dim, wkt_after) in tests:
        srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
        geom = ogr.CreateGeometryFromWkt(wkt_before)
        lyr = srcDS.CreateLayer("test", geom_type=geom.GetGeometryType())
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)

        ds = gdal.VectorTranslate("", srcDS, format="Memory", dim=dim)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToIsoWkt() != wkt_after:
            print(wkt_before)
            pytest.fail(dim)


###############################################################################
# Test gdal.VectorTranslate(dst_ds, ...) without accessMode specified (#6612)


def test_ogr2ogr_lib_17():

    ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx("../ogr/data/poly.shp"))
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None


###############################################################################
# Test -limit


def test_ogr2ogr_lib_18():

    ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx("../ogr/data/poly.shp"), limit=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################
# Test -addFields + -select


def test_ogr2ogr_lib_19():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("foo"))
    lyr.CreateField(ogr.FieldDefn("bar"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f["bar"] = "foo"
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="Memory", selectFields=["foo"])
    gdal.VectorTranslate(
        ds, src_ds, accessMode="append", addFields=True, selectFields=["bar"]
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f["foo"] != "bar" or f.IsFieldSet("bar"):
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f["bar"] != "foo" or f.IsFieldSet("foo"):
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test preservation of source geometry field name


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_20():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("foo"))

    ds = gdal.VectorTranslate("/vsimem/out.gpkg", src_ds, format="GPKG")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "foo"
    ds = None
    gdal.Unlink("/vsimem/out.gpkg")

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("foo"))
    lyr.CreateGeomField(ogr.GeomFieldDefn("bar"))

    ds = gdal.VectorTranslate(
        "/vsimem/out.gpkg", src_ds, format="GPKG", selectFields=["bar"]
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "bar"
    ds = None
    gdal.Unlink("/vsimem/out.gpkg")


###############################################################################
# Verify -append and -select options are an invalid combination


def test_ogr2ogr_lib_21():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("foo"))
    lyr.CreateField(ogr.FieldDefn("bar"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f["bar"] = "foo"
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="Memory")
    with pytest.raises(Exception):
        gdal.VectorTranslate(ds, src_ds, accessMode="append", selectFields=["foo"])

    ds = None
    f.Destroy()
    src_ds = None


###############################################################################


@pytest.mark.require_geos
def test_ogr2ogr_clipsrc_wkt_no_dst_geom():

    wkt = "POLYGON ((479461 4764494,479461 4764196,480012 4764196,480012 4764494,479461 4764494))"
    ds = gdal.VectorTranslate("", "../ogr/data/poly.shp", format="Memory", clipSrc=wkt)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 1
    ds = None


###############################################################################
# Check that ogr2ogr does data axis to CRS axis mapping adaptations in case
# of the output driver not following the mapping of the input dataset.


def test_ogr2ogr_axis_mapping_swap():

    gdal.FileFromMemBuffer(
        "/vsimem/test_ogr2ogr_axis_mapping_swap.gml",
        """<ogr:FeatureCollection
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ out.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml">
  <ogr:featureMember>
    <ogr:test gml:id="test.0">
      <ogr:geometryProperty><gml:Point srsName="urn:ogc:def:crs:EPSG::4326">
        <gml:pos>49 2</gml:pos></gml:Point></ogr:geometryProperty>
    </ogr:test>
  </ogr:featureMember>
</ogr:FeatureCollection>""",
    )
    gdal.FileFromMemBuffer(
        "/vsimem/test_ogr2ogr_axis_mapping_swap.gfs",
        """"<GMLFeatureClassList>
  <GMLFeatureClass>
    <Name>test</Name>
    <ElementPath>test</ElementPath>
    <SRSName>urn:ogc:def:crs:EPSG::4326</SRSName>
  </GMLFeatureClass>
</GMLFeatureClassList>""",
    )

    with gdaltest.disable_exceptions():
        ds = gdal.OpenEx(
            "/vsimem/test_ogr2ogr_axis_mapping_swap.gml",
            open_options=["INVERT_AXIS_ORDER_IF_LAT_LONG=NO"],
        )
    if ds is None:
        gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gml")
        gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gfs")
        pytest.skip("GML reader not available")
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [1, 2]
    ds = None
    ds = gdal.VectorTranslate(
        "/vsimem/test_ogr2ogr_axis_mapping_swap.shp",
        "/vsimem/test_ogr2ogr_axis_mapping_swap.gml",
    )
    gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gml")
    gdal.Unlink("/vsimem/test_ogr2ogr_axis_mapping_swap.gfs")

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    try:
        ogrtest.check_feature_geometry(feat, "POINT (2 49)")
    finally:
        ds = None

        ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
            "/vsimem/test_ogr2ogr_axis_mapping_swap.shp"
        )


###############################################################################
# Test -ct


def test_ogr2ogr_lib_ct():

    ds = gdal.VectorTranslate(
        "",
        "../ogr/data/poly.shp",
        format="Memory",
        dstSRS="EPSG:32630",
        reproject=True,
        coordinateOperation="+proj=affine +s11=-1",
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    # f.DumpReadable()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((-479819.84375 4765180.5,-479690.1875 4765259.5,-479647.0 4765369.5,-479730.375 4765400.5,-480039.03125 4765539.5,-480035.34375 4765558.5,-480159.78125 4765610.5,-480202.28125 4765482.0,-480365.0 4765015.5,-480389.6875 4764950.0,-480133.96875 4764856.5,-480080.28125 4764979.5,-480082.96875 4765049.5,-480088.8125 4765139.5,-480059.90625 4765239.5,-480019.71875 4765319.5,-479980.21875 4765409.5,-479909.875 4765370.0,-479859.875 4765270.0,-479819.84375 4765180.5))",
    )


###############################################################################
# Test -ct without SRS specification


def test_ogr2ogr_lib_ct_no_srs():

    ds = gdal.VectorTranslate(
        "",
        "../ogr/data/poly.shp",
        format="Memory",
        coordinateOperation="+proj=affine +s11=-1",
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "27700"
    f = lyr.GetNextFeature()
    # f.DumpReadable()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((-479819.84375 4765180.5,-479690.1875 4765259.5,-479647.0 4765369.5,-479730.375 4765400.5,-480039.03125 4765539.5,-480035.34375 4765558.5,-480159.78125 4765610.5,-480202.28125 4765482.0,-480365.0 4765015.5,-480389.6875 4764950.0,-480133.96875 4764856.5,-480080.28125 4764979.5,-480082.96875 4765049.5,-480088.8125 4765139.5,-480059.90625 4765239.5,-480019.71875 4765319.5,-479980.21875 4765409.5,-479909.875 4765370.0,-479859.875 4765270.0,-479819.84375 4765180.5))",
    )


###############################################################################
# Test -nlt CONVERT_TO_LINEAR -nlt PROMOTE_TO_MULTI


@pytest.mark.parametrize(
    "geometryType",
    [
        ["PROMOTE_TO_MULTI", "CONVERT_TO_LINEAR"],
        ["CONVERT_TO_LINEAR", "PROMOTE_TO_MULTI"],
    ],
)
def test_ogr2ogr_lib_convert_to_linear_promote_to_multi(geometryType):

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CIRCULARSTRING(0 0,1 0,0 0)"))
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="Memory", geometryType=geometryType)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbMultiLineString


###############################################################################
# Test -makevalid


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_lib_makevalid():

    # Check if MakeValid() is available
    g = ogr.CreateGeometryFromWkt("POLYGON ((0 0,10 10,0 10,10 0,0 0))")
    with gdaltest.error_handler(), gdaltest.disable_exceptions():
        make_valid_available = g.MakeValid() is not None

    tmpfilename = "/vsimem/tmp.csv"
    with gdaltest.tempfile(
        tmpfilename,
        """id,WKT
1,"POLYGON ((0 0,10 10,0 10,10 0,0 0))"
2,"POLYGON ((0 0,0 1,0.5 1,0.5 0.75,0.5 1,1 1,1 0,0 0))"
3,"GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(3 4,5 6))"
""",
    ):
        if make_valid_available:
            ds = gdal.VectorTranslate("", tmpfilename, format="Memory", makeValid=True)
        else:
            with pytest.raises(Exception):
                gdal.VectorTranslate("", tmpfilename, format="Memory", makeValid=True)
            return

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "MULTIPOLYGON (((0 0,5 5,10 0,0 0)),((5 5,0 10,10 10,5 5)))"
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POLYGON ((0 0,0 1,0.5 1.0,1 1,1 0,0 0))")
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(3 4,5 6))"
    )


###############################################################################
# Test SQLStatement with -sql @filename syntax


def test_ogr2ogr_lib_sql_filename():

    with gdaltest.tempfile(
        "/vsimem/my.sql",
        """-- initial comment\nselect\n'--''--' as literalfield,* from --comment\npoly\n-- trailing comment""",
    ):
        ds = gdal.VectorTranslate(
            "", "../ogr/data/poly.shp", options="-f Memory -sql @/vsimem/my.sql"
        )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    assert lyr.GetLayerDefn().GetFieldIndex("literalfield") == 0


###############################################################################
# Verify -emptyStrAsNull


def test_ogr2ogr_emptyStrAsNull():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = ""
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="Memory", emptyStrAsNull=True)

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()

    assert f["foo"] is None, "expected empty string to be transformed to null"


###############################################################################
# Verify propagation of field domains


def test_ogr2ogr_fielddomain_():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")

    src_fld_defn = ogr.FieldDefn("foo")
    src_fld_defn.SetDomainName("my_domain")
    src_lyr.CreateField(src_fld_defn)
    assert src_ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("my_domain", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    )

    src_fld_defn = ogr.FieldDefn("bar", ogr.OFTInteger)
    src_fld_defn.SetDomainName("coded_domain")
    src_lyr.CreateField(src_fld_defn)
    assert src_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "coded_domain",
            "desc",
            ogr.OFTString,
            ogr.OFSTNone,
            {1: "one", 2: "two", 3: None},
        )
    )

    src_fld_defn = ogr.FieldDefn("baz", ogr.OFTInteger)
    src_fld_defn.SetDomainName("non_existant_coded_domain")
    src_lyr.CreateField(src_fld_defn)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("foo", "foo_content")
    f.SetField("bar", 2)
    f.SetField("baz", 0)
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("bar", -1)  # does not exist in dictionary
    src_lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="Memory")
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetDomainName() == "my_domain"
    domain = ds.GetFieldDomain("my_domain")
    assert domain is not None
    assert domain.GetDomainType() == ogr.OFDT_GLOB

    # Test -resolveDomains
    ds = gdal.VectorTranslate("", src_ds, format="Memory", resolveDomains=True)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 4

    fld_defn = lyr_defn.GetFieldDefn(0)
    assert fld_defn.GetDomainName() == "my_domain"
    domain = ds.GetFieldDomain("my_domain")
    assert domain is not None
    assert domain.GetDomainType() == ogr.OFDT_GLOB

    fld_defn = lyr_defn.GetFieldDefn(1)
    assert fld_defn.GetName() == "bar"
    assert fld_defn.GetType() == ogr.OFTInteger

    fld_defn = lyr_defn.GetFieldDefn(2)
    assert fld_defn.GetName() == "bar_resolved"
    assert fld_defn.GetType() == ogr.OFTString

    fld_defn = lyr_defn.GetFieldDefn(3)
    assert fld_defn.GetName() == "baz"
    assert fld_defn.GetType() == ogr.OFTInteger

    f = lyr.GetNextFeature()
    assert f["foo"] == "foo_content"
    assert f["bar"] == 2
    assert f["bar_resolved"] == "two"
    assert f["baz"] == 0

    f = lyr.GetNextFeature()
    assert f["bar"] == -1
    assert not f.IsFieldSet("bar_resolved")


###############################################################################
# Test -a_coord_epoch


def test_ogr2ogr_assign_coord_epoch():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("layer")

    ds = gdal.VectorTranslate(
        "", src_ds, options="-f Memory -a_srs EPSG:7665 -a_coord_epoch 2021.3"
    )
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3


###############################################################################
# Test -s_coord_epoch


@pytest.mark.require_proj(7, 2)
def test_ogr2ogr_s_coord_epoch():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(120 -40)"))
    src_lyr.CreateFeature(f)

    # ITRF2014 to GDA2020
    ds = gdal.VectorTranslate(
        "",
        src_ds,
        options="-f Memory -s_srs EPSG:9000 -s_coord_epoch 2030 -t_srs EPSG:7844",
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetX(0) != 120 and abs(g.GetX(0) - 120) < 1e-5
    assert g.GetY(0) != -40 and abs(g.GetY(0) - -40) < 1e-5


###############################################################################
# Test -t_coord_epoch


@pytest.mark.require_proj(7, 2)
def test_ogr2ogr_t_coord_epoch():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(120 -40)"))
    src_lyr.CreateFeature(f)

    # GDA2020 to ITRF2014
    ds = gdal.VectorTranslate(
        "",
        src_ds,
        options="-f Memory -t_srs EPSG:9000 -t_coord_epoch 2030 -s_srs EPSG:7844",
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetX(0) != 120 and abs(g.GetX(0) - 120) < 1e-5
    assert g.GetY(0) != -40 and abs(g.GetY(0) - -40) < 1e-5


###############################################################################
# Test laundering of geometry column name when outputting to PostgreSQL (#6261)


@pytest.mark.require_driver("PGDump")
def test_ogr2ogr_launder_geometry_column_name():

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = srcDS.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("SHAPE", ogr.wkbPoint))
    out_filename = "/vsimem/test_ogr2ogr_launder_geometry_column_name.sql"
    assert gdal.VectorTranslate(out_filename, srcDS, format="PGDump") is not None
    f = gdal.VSIFOpenL(out_filename, "rb")
    assert f
    sql = gdal.VSIFReadL(1, 10000, f).decode("utf-8")
    gdal.VSIFCloseL(f)
    gdal.Unlink(out_filename)
    assert "SHAPE" not in sql
    assert "shape" in sql


###############################################################################


def get_sqlite_version():

    if gdal.GetDriverByName("GPKG") is None:
        return (0, 0, 0)

    ds = ogr.Open(":memory:")
    sql_lyr = ds.ExecuteSQL("SELECT sqlite_version()")
    f = sql_lyr.GetNextFeature()
    version = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    return tuple([int(x) for x in version.split(".")[0:3]])


###############################################################################


@pytest.mark.skipif(
    get_sqlite_version() < (3, 24, 0),
    reason="sqlite >= 3.24 needed",
)
def test_ogr2ogr_upsert():

    filename = "/vsimem/test_ogr_gpkg_upsert_without_fid.gpkg"

    def create_gpkg_file():
        ds = gdal.GetDriverByName("GPKG").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
        lyr = ds.CreateLayer("foo")
        assert lyr.CreateField(ogr.FieldDefn("other", ogr.OFTString)) == ogr.OGRERR_NONE
        unique_field = ogr.FieldDefn("unique_field", ogr.OFTString)
        unique_field.SetUnique(True)
        assert lyr.CreateField(unique_field) == ogr.OGRERR_NONE
        for i in range(5):
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField("unique_field", i + 1)
            f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        ds = None

    create_gpkg_file()

    def create_mem_file():
        srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
        mem_lyr = srcDS.CreateLayer("foo")
        assert (
            mem_lyr.CreateField(ogr.FieldDefn("other", ogr.OFTString))
            == ogr.OGRERR_NONE
        )
        unique_field = ogr.FieldDefn("unique_field", ogr.OFTString)
        unique_field.SetUnique(True)
        assert mem_lyr.CreateField(unique_field) == ogr.OGRERR_NONE

        f = ogr.Feature(mem_lyr.GetLayerDefn())
        f.SetField("unique_field", "2")
        f.SetField("other", "foo")
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (10 10)"))
        mem_lyr.CreateFeature(f)
        return srcDS

    assert (
        gdal.VectorTranslate(filename, create_mem_file(), accessMode="upsert")
        is not None
    )

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(2)
    assert f["unique_field"] == "2"
    assert f["other"] == "foo"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (10 10)"
    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test -t_srs to a driver that automatically reprojects to WGS 84


@pytest.mark.require_driver("GeoJSONSeq")
def test_ogr2ogr_lib_t_srs_ignored():

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 49)"))
    srcLayer.CreateFeature(f)

    got_msg = []

    def my_handler(errorClass, errno, msg):
        got_msg.append(msg)
        return

    gdal.PushErrorHandler(my_handler)
    assert (
        gdal.VectorTranslate(
            "/vsimem/out.txt",
            srcDS,
            format="GeoJSONSeq",
            dstSRS="EPSG:32631",
            reproject=True,
        )
        is not None
    )
    gdal.PopErrorHandler()
    gdal.Unlink("/vsimem/out.txt")
    assert got_msg == [
        "Target SRS WGS 84 / UTM zone 31N not taken into account as target driver "
        "likely implements on-the-fly reprojection to WGS 84"
    ]


###############################################################################
# Test spatSRS


@pytest.mark.require_geos
def test_ogr2ogr_lib_spat_srs_projected():

    # Check that we densify spatial filter geometry when not expressed in
    # the layer CRS

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POINT(20.56403717640477 60.367519337232835)")
    )
    # Reprojection of this point to EPSG:3067 is POINT (145388.398 6709681.065)
    # and thus falls in the below spatial filter rectangle
    # But if we don't densify the geometry enough, post processing would
    # discard the point.
    srcLayer.CreateFeature(f)

    ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="Memory",
        spatFilter=[130036.75, 6697405.5, 145400.4, 6756013.0],
        spatSRS="EPSG:3067",
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test spatSRS


@pytest.mark.require_geos
def test_ogr2ogr_lib_spat_srs_geographic():

    # Check that we densify spatial filter geometry when not expressed in
    # the layer CRS

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32661)
    srcLayer = srcDS.CreateLayer("test", srs=srs)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    # Reprojects as -90 89.099 in EPSG:4326
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1900000 2000000)"))
    srcLayer.CreateFeature(f)

    # Naive reprojection of the below bounding box to EPSG:32661 would
    # be [2000000, 2000000, 2000000, 3112951.14]
    ds = gdal.VectorTranslate(
        "", srcDS, format="Memory", spatFilter=[-180, 80, 180, 90], spatSRS="EPSG:4326"
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test -clipsrc with a clip datasource


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos
def test_ogr2ogr_lib_clipsrc_datasource():

    # Prepare the data layer to clip
    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srcLayer = srcDS.CreateLayer("test", geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0, 2 2)"))
    srcLayer.CreateFeature(f)

    # Prepare the data layers to clip with
    clip_path = "/vsimem/clip_test.gpkg"
    clip_ds = gdal.GetDriverByName("GPKG").Create(clip_path, 0, 0, 0, gdal.GDT_Unknown)
    clip_layer = clip_ds.CreateLayer("cliptest", geom_type=ogr.wkbPolygon)
    clip_layer.CreateField(ogr.FieldDefn("filter_field", ogr.OFTString))
    # Overlaps with half the src line
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField("filter_field", "half_overlap_line_result")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((1 1, 1 2, 2 2, 2 1, 1 1))"))
    clip_layer.CreateFeature(f)
    # Doesn't overlap at all
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField("filter_field", "no_overlap_no_result")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((5 5, 5 6, 6 6, 6 5, 5 5))"))
    clip_layer.CreateFeature(f)
    # Feature not to clip with
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField("filter_field", "exact_overlap_full_result")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0, 0 2, 2 2, 2 0, 0 0))"))
    clip_layer.CreateFeature(f)
    clip_ds = None

    # Test clip with 'half_overlap_line_result' using sql statement
    sql = "SELECT * FROM cliptest WHERE filter_field = 'half_overlap_line_result'"
    dst_ds = gdal.VectorTranslate(
        "", srcDS, format="Memory", clipSrc=clip_path, clipSrcSQL=sql
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_feature = dst_lyr.GetFeature(0)
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2)"
    dst_ds = None

    # Test clip with the "exact_overlap_full_result" using clipSrcLayer + clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="Memory",
        clipSrc=clip_path,
        clipSrcLayer="cliptest",
        clipSrcWhere="filter_field = 'exact_overlap_full_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_feature = dst_lyr.GetFeature(0)
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,2 2)"
    dst_ds = None

    # Test clip with the "no_overlap_no_result" using only clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="Memory",
        clipSrc=clip_path,
        clipSrcWhere="filter_field = 'no_overlap_no_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 0
    dst_ds = None

    # Cleanup
    gdal.Unlink(clip_path)


###############################################################################
# Test -clipsrc and intersection being of a lower dimensionality


@pytest.mark.require_geos
def test_ogr2ogr_lib_clipsrc_discard_lower_dimensionality():

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0, 1 1)"))
    srcLayer.CreateFeature(f)

    # Intersection of above geometry with clipSrc bounding box is a point
    ds = gdal.VectorTranslate("", srcDS, format="Memory", clipSrc=[-1, -1, 0, 0])
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0
    ds = None


###############################################################################
# Test -clipsrc with a clip layer with an invalid polygon


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos(3, 8)
def test_ogr2ogr_lib_clipsrc_invalid_polygon():

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0.25 0.25)"))
    srcLayer.CreateFeature(f)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-0.5 0.5)"))
    srcLayer.CreateFeature(f)

    # Prepare the data layers to clip with
    clip_path = "/vsimem/clip_test.gpkg"
    clip_ds = gdal.GetDriverByName("GPKG").Create(clip_path, 0, 0, 0, gdal.GDT_Unknown)
    clip_layer = clip_ds.CreateLayer("cliptest", geom_type=ogr.wkbPolygon)
    f = ogr.Feature(clip_layer.GetLayerDefn())
    # Invalid polygon with self crossing
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,1 1,0 1,1 0,0 0))"))
    clip_layer.CreateFeature(f)
    clip_ds = None

    # Intersection of above geometry with clipSrc bounding box is a point
    with gdaltest.error_handler():
        ds = gdal.VectorTranslate("", srcDS, format="Memory", clipSrc=clip_path)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    # Cleanup
    gdal.Unlink(clip_path)


###############################################################################
# Test -clipsrc with 3d clip layer


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos(3, 8)
def test_ogr2ogr_lib_clipsrc_3d_polygon():

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0, 10 10)"))
    srcLayer.CreateFeature(f)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 10, 10 0)"))
    srcLayer.CreateFeature(f)

    # Prepare the data layers to clip with
    clip_path = "/vsimem/clip_test.gpkg"
    clip_ds = gdal.GetDriverByName("GPKG").Create(clip_path, 0, 0, 0, gdal.GDT_Unknown)
    clip_layer = clip_ds.CreateLayer("cliptest", geom_type=ogr.wkbPolygon)
    f = ogr.Feature(clip_layer.GetLayerDefn())
    # 3d polygon
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON Z ((0 0 0, 10 0 10, 10 5 10, 0 5 0, 0 0 0))")
    )
    clip_layer.CreateFeature(f)
    clip_ds = None

    with gdaltest.error_handler():
        ds = gdal.VectorTranslate("", srcDS, format="Memory", clipSrc=clip_path)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (0 0 0, 5 5 5)")

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (5 5 5, 10 0 10)")

    ds = None


###############################################################################
# Test -clipdst with a clip datasource


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos
def test_ogr2ogr_lib_clipdst_datasource():

    # Prepare the data layer to clip
    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srcLayer = srcDS.CreateLayer("test", geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0, 2 2)"))
    srcLayer.CreateFeature(f)

    # Prepare the data layers to clip with
    clip_path = "/vsimem/clip_test.gpkg"
    clip_ds = gdal.GetDriverByName("GPKG").Create(clip_path, 0, 0, 0, gdal.GDT_Unknown)
    clip_layer = clip_ds.CreateLayer("cliptest", geom_type=ogr.wkbPolygon)
    clip_layer.CreateField(ogr.FieldDefn("filter_field", ogr.OFTString))
    # Overlaps with half the src line
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField("filter_field", "half_overlap_line_result")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((1 1, 1 2, 2 2, 2 1, 1 1))"))
    clip_layer.CreateFeature(f)
    # Doesn't overlap at all
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField("filter_field", "no_overlap_no_result")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((5 5, 5 6, 6 6, 6 5, 5 5))"))
    clip_layer.CreateFeature(f)
    # Feature not to clip with
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField("filter_field", "exact_overlap_full_result")
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0, 0 2, 2 2, 2 0, 0 0))"))
    clip_layer.CreateFeature(f)
    clip_ds = None

    # Test clip with 'half_overlap_line_result' using sql statement
    sql = "SELECT * FROM cliptest WHERE filter_field = 'half_overlap_line_result'"
    dst_ds = gdal.VectorTranslate(
        "", srcDS, format="Memory", clipDst=clip_path, clipDstSQL=sql
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_feature = dst_lyr.GetFeature(0)
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2)"
    dst_ds = None

    # Test clip with the "exact_overlap_full_result" using clipDstLayer + clipDstWhere
    dst_ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="Memory",
        clipDst=clip_path,
        clipDstLayer="cliptest",
        clipDstWhere="filter_field = 'exact_overlap_full_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_feature = dst_lyr.GetFeature(0)
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,2 2)"
    dst_ds = None

    # Test clip with the "no_overlap_no_result" using only clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="Memory",
        clipDst=clip_path,
        clipDstWhere="filter_field = 'no_overlap_no_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 0
    dst_ds = None

    # Cleanup
    gdal.Unlink(clip_path)


###############################################################################
# Test -clipdst and intersection being of a lower dimensionality


@pytest.mark.require_geos
def test_ogr2ogr_lib_clipdst_discard_lower_dimensionality():

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0, 1 1)"))
    srcLayer.CreateFeature(f)

    # Intersection of above geometry with clipDst bounding box is a point -> no result
    ds = gdal.VectorTranslate("", srcDS, format="Memory", clipDst=[-1, -1, 0, 0])
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0


###############################################################################
# Test /-clipsrc-clipdst with reprojection


@pytest.mark.require_geos
@pytest.mark.parametrize("clipSrc", [True, False])
def test_ogr2ogr_lib_clip_datasource_reprojection(clipSrc):

    # Prepare the data layer to clip
    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs_4326 = osr.SpatialReference()
    srs_4326.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs_4326.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", geom_type=ogr.wkbLineString, srs=srs_4326)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 49)"))
    srcLayer.CreateFeature(f)

    # Prepare the data layers to clip with
    clip_path = "/vsimem/clip_test.shp"
    clip_ds = gdal.GetDriverByName("ESRI Shapefile").Create(
        clip_path, 0, 0, 0, gdal.GDT_Unknown
    )
    srs_32631 = osr.SpatialReference()
    srs_32631.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs_32631.ImportFromEPSG(32631)
    clip_layer = clip_ds.CreateLayer(
        "clip_test", geom_type=ogr.wkbPolygon, srs=srs_32631
    )
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((426857 5427937,426857 5427938,426858 5427938,426858 5427937,426857 5427937))"
        )
    )
    clip_layer.CreateFeature(f)
    clip_ds = None

    dst_ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="Memory",
        clipSrc=clip_path if clipSrc else None,
        clipDst=clip_path if not clipSrc else None,
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_feature = dst_lyr.GetFeature(0)
    assert dst_feature.GetGeometryRef().ExportToWkt() == "POINT (2 49)"
    dst_ds = None

    # Cleanup
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(clip_path)


###############################################################################
# Test using explodecollections


def test_ogr2ogr_lib_explodecollections():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    wkt = "MULTIPOLYGON (((0 0,5 5,10 0,0 0)),((5 5,0 10,10 10,5 5)))"
    f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
    src_lyr.CreateFeature(f)

    dst_ds = gdal.VectorTranslate("", src_ds, format="Memory", explodeCollections=True)

    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 2, "wrong feature count"


###############################################################################
# Test converting a layer with a fid string to GPKG


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_fid_string_to_gpkg():

    srcDS = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srcLayer = srcDS.CreateLayer("test")
    srcLayer.CreateField(ogr.FieldDefn("fid"))

    ds = gdal.VectorTranslate(":memory:", srcDS, options="-f GPKG")
    lyr = ds.GetLayer(0)
    assert lyr.GetFIDColumn() == "gpkg_fid"


###############################################################################
# Test using mapfieldtype parameter


def test_ogr2ogr_lib_mapfieldtype():

    src_path = "../ogr/data/poly.shp"
    dst_ds = gdal.VectorTranslate(
        "", src_path, format="Memory", mapFieldType=["Integer64=String"]
    )
    assert dst_ds is not None

    src_ds = gdal.OpenEx(src_path)
    src_lyr = src_ds.GetLayer(0)
    src_lyr_defn = src_lyr.GetLayerDefn()
    dst_lyr = dst_ds.GetLayer(0)
    dst_lyr_defn = dst_lyr.GetLayerDefn()
    for i in range(src_lyr_defn.GetFieldCount()):
        src_typename = src_lyr_defn.GetFieldDefn(i).GetTypeName()
        dst_typename = dst_lyr_defn.GetFieldDefn(i).GetTypeName()
        if src_typename == "Integer64":
            assert dst_typename == "String"
        else:
            assert dst_typename == src_typename


###############################################################################
# Test using combination of arguments and a "raw" options list


def test_ogr2ogr_lib_options_and_args():

    raw_options_list = ["-limit", "1"]
    ds = gdal.VectorTranslate(
        "",
        "../ogr/data/poly.shp",
        format="Memory",
        selectFields=["eas_id", "prfedea"],
        options=raw_options_list,
    )

    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1, "wrong feature count"
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsDouble("EAS_ID") == 168
    assert feat.GetFieldAsString("PRFEDEA") == "35043411"


###############################################################################
# Test using simplify


@pytest.mark.require_geos
def test_ogr2ogr_lib_simplify():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0, 1 0, 10 0)"))
    src_lyr.CreateFeature(f)

    dst_ds = gdal.VectorTranslate("", src_ds, format="Memory", simplifyTolerance=5)

    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1, "wrong feature count"
    dst_feature = dst_lyr.GetFeature(0)
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,10 0)"


###############################################################################
# Test using transactionSize


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("transaction_size", [0, 10, "unlimited"])
@gdaltest.disable_exceptions()
def test_ogr2ogr_lib_transaction_size(transaction_size):

    ds = gdal.VectorTranslate(
        "/vsimem/out.gpkg",
        "../ogr/data/poly.shp",
        format="GPKG",
        transactionSize=transaction_size,
    )

    try:
        # A transaction size of 0 is invalid
        if transaction_size == 0:
            assert ds is None
            return

        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 10, "wrong feature count"
    finally:
        ds = None
        gdal.Unlink("/vsimem/out.gpkg")


###############################################################################
# Test -dateTimeTo


def test_ogr2ogr_lib_dateTimeTo():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    src_lyr.CreateField(ogr.FieldDefn("dt", ogr.OFTDateTime))
    src_lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["dt"] = "2023/02/01 02:34:56.789+0315"
    f["int"] = 1
    src_lyr.CreateFeature(f)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["dt"] = "2023/02/01 01:34:56.789+0300"
    src_lyr.CreateFeature(f)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["dt"] = "2023/02/01 00:34:56.789"
    src_lyr.CreateFeature(f)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    with gdaltest.error_handler():
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo foo")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTCx")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTCx12")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC+15")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC+12:")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC+12:3")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC+12:34")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC+12:345")

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC+03:00")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/02/01 02:19:56.789+03"
    assert f["int"] == 1
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/02/01 01:34:56.789+03"
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/02/01 00:34:56.789"
    f = dst_lyr.GetNextFeature()
    assert not f.IsFieldSet("dt")

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 23:19:56.789+00"

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC-13:15")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 10:04:56.789-1315"

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC-13:30")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 09:49:56.789-1330"

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f Memory -dateTimeTo UTC-13:45")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 09:34:56.789-1345"


###############################################################################
# Test converting a list type to JSON (#7397)


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_convert_list_type_to_JSON():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    src_lyr.CreateField(ogr.FieldDefn("strlist", ogr.OFTStringList))
    src_lyr.CreateField(ogr.FieldDefn("intlist", ogr.OFTIntegerList))
    src_lyr.CreateField(ogr.FieldDefn("int64list", ogr.OFTInteger64List))
    src_lyr.CreateField(ogr.FieldDefn("reallist", ogr.OFTRealList))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["strlist"] = ["one", "two"]
    f["intlist"] = [1, 2]
    f["int64list"] = [1234567890123, 1]
    f["reallist"] = [1.5, 2.5]
    src_lyr.CreateFeature(f)

    out_filename = "/vsimem/test_ogr2ogr_lib_convert_list_type_to_JSON.gpkg"
    dst_ds = gdal.VectorTranslate(out_filename, src_ds)
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    f = dst_lyr.GetNextFeature()
    assert f["strlist"] == '[ "one", "two" ]'
    assert f["intlist"] == "[ 1, 2 ]"
    assert f["int64list"] == "[ 1234567890123, 1 ]"
    assert f["reallist"] == "[ 1.5, 2.5 ]"
    dst_ds = None
    gdal.Unlink(out_filename)


@pytest.mark.parametrize(
    "src_driver, src_ext, dest_driver, dest_ext, expected_dest_width, expected_dest_precision, exp_src_decimal_flag, exp_src_minus_flag",
    [
        (
            "ESRI Shapefile",
            ".shp",
            "ESRI Shapefile",
            ".shp",
            5,
            2,
            True,
            True,
        ),  # round trip
        ("ESRI Shapefile", ".shp", "GML", ".gml", 4, 2, True, True),
        ("ESRI Shapefile", ".shp", "CSV", ".csv", 5, 2, True, True),
        ("ESRI Shapefile", ".shp", "PGDump", ".sql", 4, 2, True, True),
        ("GML", ".gml", "GML", ".gml", 4, 2, False, False),  # round trip
        ("GML", ".gml", "ESRI Shapefile", ".shp", 6, 2, False, False),
        ("GML", ".gml", "PGDump", ".sql", 4, 2, False, False),
        ("GML", ".gml", "CSV", ".csv", 6, 2, False, False),
        ("CSV", ".csv", "CSV", ".csv", 5, 2, True, True),  # round trip
        ("CSV", ".csv", "ESRI Shapefile", ".shp", 5, 2, True, True),
        ("CSV", ".csv", "GML", ".gml", 4, 2, True, True),
        ("CSV", ".shp", "PGDump", ".sql", 4, 2, True, True),
        # Note: MapInfo test does not pass because the driver aborts
        #       writing on overflow error while other drivers keep writing
        # ("ESRI Shapefile", ".shp", "MapInfo File", ".tab", 4, 2, True, True),
        # ("MapInfo File", ".tab", "ESRI Shapefile", ".shp", 4, 2, True, True),
        # ("GML", ".gml", "MapInfo File", ".tab", 4, 2, False, False),
    ],
)
def test_width_precision_flags(
    src_driver,
    src_ext,
    dest_driver,
    dest_ext,
    expected_dest_width,
    expected_dest_precision,
    exp_src_decimal_flag,
    exp_src_minus_flag,
):
    """Test precision/width (scale) conversions for numeric type"""

    if not gdal.GetDriverByName(src_driver):
        pytest.skip(reason=f"Source driver {src_driver} not available.")

    if not gdal.GetDriverByName(dest_driver):
        pytest.skip(reason=f"Destination driver {dest_driver} not available.")

    # Here is a list of drivers unreliable on precision: they respect precision in their "schema"
    # but they can actually store values with higher precision, with or without warning depending
    # on the driver.
    # For instance: GML correctly sets width to 4 and precision to 2 in the XSD
    # but it writes the full precision value in the GML without warning
    PRECISION_UNRELIABLE_DRIVERS = ("GML", "CSV")

    src_file_base_name = "ogr_precision_flags_test"
    src_file_name = tempfile.mktemp(src_ext, src_file_base_name)

    dest_file_base_name = "ogr_precision_flags_test"
    dest_file_name = tempfile.mktemp(dest_ext, dest_file_base_name)

    dr = ogr.GetDriverByName(src_driver)
    src_width_includes_sign = (
        dr.GetMetadata_Dict().get("DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN", "NO")
        == "YES"
    )
    src_width_includes_point = (
        dr.GetMetadata_Dict().get(
            "DMD_NUMERIC_FIELD_WIDTH_INCLUDES_DECIMAL_SEPARATOR", "NO"
        )
        == "YES"
    )

    assert src_width_includes_sign == exp_src_minus_flag
    assert src_width_includes_point == exp_src_decimal_flag

    # Assume width=5 includes sign and point to store 12.34 or -1.23 (but not -12.34)
    # For SQL it would be precision=4, scale=2
    width = 5
    precision = 2

    # If the source does not include decimal separator, we can reduce width by one
    if not src_width_includes_point:
        width -= 1

    ds = dr.CreateDataSource(src_file_name)

    # For CSV we want csvt to support width and precision
    options = []
    if src_driver == "CSV":
        options.append("CREATE_CSVT=YES")
        options.append("GEOMETRY=AS_WKT")

    lyr = ds.CreateLayer(src_file_base_name, geom_type=ogr.wkbPoint, options=options)
    field_def = ogr.FieldDefn("num_5_2", ogr.OFTReal)
    field_def.SetWidth(width)
    field_def.SetPrecision(precision)
    lyr.CreateField(field_def)

    lyr_def = lyr.GetLayerDefn()
    field_idx = lyr_def.GetFieldIndex("num_5_2")
    f = ogr.Feature(lyr_def)
    f.SetField(field_idx, 12.34)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr_def)
    f.SetField(field_idx, -1.23)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)

    # This does not fit if minus sign is included in width
    f = ogr.Feature(lyr_def)
    f.SetField(field_idx, -12.34)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    lyr.CreateFeature(f)
    f = None
    lyr = None
    ds = None

    # Verify source
    ds = ogr.Open(src_file_name)
    lyr = ds.GetLayer(0)
    assert lyr is not None
    lyr_def = lyr.GetLayerDefn()
    field_idx = lyr_def.GetFieldIndex("num_5_2")
    field_def = lyr_def.GetFieldDefn(field_idx)
    assert field_def.GetWidth() == width
    assert field_def.GetPrecision() == precision

    features = [f for f in lyr]
    f = features[0]
    assert f.GetFieldAsDouble(field_idx) == 12.34
    f = features[1]
    assert f.GetFieldAsDouble(field_idx) == -1.23
    f = features[2]

    if src_driver in PRECISION_UNRELIABLE_DRIVERS:
        assert f.GetFieldAsDouble(field_idx) == -12.34
    else:
        assert f.GetFieldAsDouble(field_idx) == -12.3

    ds = None

    options = gdal.VectorTranslateOptions()

    # For CSV we want csvt to support width and precision
    layerCreationOptions = []

    if dest_driver == "CSV":
        layerCreationOptions.append("CREATE_CSVT=YES")

    options = gdal.VectorTranslateOptions(
        format=dest_driver, layerCreationOptions=layerCreationOptions
    )

    # Source is ok, convert with default options
    dst_ds = gdal.VectorTranslate(dest_file_name, src_file_name, options=options)

    dst_ds = None

    # Verify destination

    # PGDump has no read capabilities, test for SQL
    if dest_driver == "PGDump":
        sql = ""
        with open(dest_file_name, "r") as f:
            sql = f.read()
        assert "NUMERIC(4,2)" in sql
    else:
        dst_ds = ogr.Open(dest_file_name)
        lyr = dst_ds.GetLayer(0)
        assert lyr is not None
        lyr_def = lyr.GetLayerDefn()
        field_idx = lyr_def.GetFieldIndex("num_5_2")
        field_def = lyr_def.GetFieldDefn(field_idx)
        assert field_def.GetWidth() == expected_dest_width
        assert field_def.GetPrecision() == expected_dest_precision

        features = [f for f in lyr]
        f = features[0]
        assert f.GetFieldAsDouble(field_idx) == 12.34
        f = features[1]
        assert f.GetFieldAsDouble(field_idx) == -1.23
        f = features[2]
        if expected_dest_width == 6 or (
            src_driver in PRECISION_UNRELIABLE_DRIVERS
            and dest_driver in PRECISION_UNRELIABLE_DRIVERS
        ):
            assert f.GetFieldAsDouble(field_idx) == -12.34
        else:
            assert f.GetFieldAsDouble(field_idx) == -12.3
