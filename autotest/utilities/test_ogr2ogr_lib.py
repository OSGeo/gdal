#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  librarified ogr2ogr testing
# Author:   Faza Mahamood <fazamhd @ gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import collections
import json
import pathlib
import tempfile

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr, osr

###############################################################################
# Simple test


def test_ogr2ogr_lib_1():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="MEM")
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
        format="MEM",
        SQLStatement="select * from poly",
        SQLDialect="OGRSQL",
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


def test_ogr2ogr_lib_2a(tmp_vsimem):

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")

    # Test @filename syntax
    gdal.FileFromMemBuffer(
        tmp_vsimem / "sql.txt",
        "-- initial comment\nselect * from poly\n-- trailing comment",
    )
    ds = gdal.VectorTranslate(
        "", srcDS, format="MEM", SQLStatement=f"@{tmp_vsimem}/sql.txt"
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


def test_ogr2ogr_lib_2b(tmp_vsimem):

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")

    # Test @filename syntax with a UTF-8 BOM
    gdal.FileFromMemBuffer(
        tmp_vsimem / "sql.txt", "\xef\xbb\xbfselect * from poly".encode("LATIN1")
    )
    ds = gdal.VectorTranslate(
        "", srcDS, format="MEM", SQLStatement=f"@{tmp_vsimem}/sql.txt"
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test WHERE


def test_ogr2ogr_lib_3(tmp_vsimem):

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="MEM", where="EAS_ID=171")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1

    # Test @filename syntax
    gdal.FileFromMemBuffer(tmp_vsimem / "filter.txt", "EAS_ID=171")
    ds = gdal.VectorTranslate(
        "", srcDS, format="MEM", where=f"@{tmp_vsimem}/filter.txt"
    )
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1


###############################################################################
# Test accessMode


def test_ogr2ogr_lib_4(tmp_vsimem):

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(tmp_vsimem / "poly.shp", srcDS)
    assert ds.GetLayer(0).GetFeatureCount() == 10, "wrong feature count"
    ds = None

    ds = gdal.VectorTranslate(tmp_vsimem / "poly.shp", srcDS, accessMode="append")
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


###############################################################################
# Test dstSRS


def test_ogr2ogr_lib_5():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="MEM", dstSRS="EPSG:4326")
    assert str(ds.GetLayer(0).GetSpatialRef()).find("1984") != -1


###############################################################################
# Test selectFields


def test_ogr2ogr_lib_6():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    # Voluntary don't use the exact case of the source field names (#4502)
    ds = gdal.VectorTranslate(
        "", srcDS, format="MEM", selectFields=["eas_id", "prfedea"]
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsDouble("EAS_ID") == 168
    assert feat.GetFieldAsString("PRFEDEA") == "35043411"


###############################################################################
# Test selectFields with arrow optimization


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_selectFields_gpkg(tmp_vsimem):

    srcDS = gdal.VectorTranslate(tmp_vsimem / "in.gpkg", "../ogr/data/poly.shp")
    gdal.VectorTranslate(
        tmp_vsimem / "out.gpkg", srcDS, selectFields=["eas_id", "prfedea"]
    )
    ds = ogr.Open(tmp_vsimem / "out.gpkg")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsDouble("EAS_ID") == 168
    assert feat.GetFieldAsString("PRFEDEA") == "35043411"


###############################################################################
# Test selectFields to []


def test_ogr2ogr_lib_sel_fields_empty():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="MEM", selectFields=[])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef() is not None


###############################################################################
# Test selectFields to []


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_sel_fields_empty_with_arow_optimization(tmp_vsimem):

    srcDS = gdal.VectorTranslate(tmp_vsimem / "in.gpkg", "../ogr/data/poly.shp")
    gdal.VectorTranslate(tmp_vsimem / "out.gpkg", srcDS, selectFields=[])
    ds = ogr.Open(tmp_vsimem / "out.gpkg")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef() is not None


###############################################################################
# Test selectFields with field names with spaces


def test_ogr2ogr_lib_sel_fields_with_space():

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = srcDS.CreateLayer("test")
    lyr.CreateField(ogr.FieldDefn("with space"))
    lyr.CreateField(ogr.FieldDefn("with,comma"))
    lyr.CreateField(ogr.FieldDefn("unselected"))
    lyr.CreateField(ogr.FieldDefn('with,double"quote'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["with space"] = "a"
    f["with,comma"] = "b"
    f['with,double"quote'] = "c"
    lyr.CreateFeature(f)
    ds = gdal.VectorTranslate(
        "",
        srcDS,
        format="MEM",
        selectFields=["with space", "with,comma", 'with,double"quote'],
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 3
    f = lyr.GetNextFeature()
    assert f["with space"] == "a"
    assert f["with,comma"] == "b"
    assert f['with,double"quote'] == "c"


###############################################################################
# Test LCO


def test_ogr2ogr_lib_7(tmp_vsimem):

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(
        tmp_vsimem / "poly.shp", srcDS, layerCreationOptions=["SHPT=POLYGONZ"]
    )
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D

    ds = None


###############################################################################
# Add explicit source layer name


def test_ogr2ogr_lib_8():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="MEM", layers=["poly"])
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    # Test also with just a string and not an array
    ds = gdal.VectorTranslate("", srcDS, format="MEM", layers="poly")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test -segmentize


def test_ogr2ogr_lib_9():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate("", srcDS, format="MEM", segmentizeMaxDist=100)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryRef(0).GetPointCount() == 36


###############################################################################
# Test overwrite with a shapefile


def test_ogr2ogr_lib_10(tmp_vsimem):

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(tmp_vsimem / "tmp/poly.shp", srcDS)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds = None

    # Overwrite
    ds = gdal.VectorTranslate(tmp_vsimem / "tmp", srcDS, accessMode="overwrite")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds = None


###############################################################################
# Test filter


def test_ogr2ogr_lib_11():

    srcDS = gdal.OpenEx("../ogr/data/poly.shp")
    ds = gdal.VectorTranslate(
        "", srcDS, format="MEM", spatFilter=[479609, 4764629, 479764, 4764817]
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
        format="MEM",
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
            format="MEM",
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
    with gdal.quiet_errors():
        ds = gdal.VectorTranslate("", srcDS, format="MEM", zField="foo")
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
    for wkt_before, dim, wkt_after in tests:
        srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
        geom = ogr.CreateGeometryFromWkt(wkt_before)
        lyr = srcDS.CreateLayer("test", geom_type=geom.GetGeometryType())
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)

        ds = gdal.VectorTranslate("", srcDS, format="MEM", dim=dim)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToIsoWkt() != wkt_after:
            print(wkt_before)
            pytest.fail(dim)


###############################################################################
# Test gdal.VectorTranslate(dst_ds, ...) without accessMode specified (#6612)


def test_ogr2ogr_lib_17():

    ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx("../ogr/data/poly.shp"))
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None


###############################################################################
# Test -limit


def test_ogr2ogr_lib_18():

    ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx("../ogr/data/poly.shp"), limit=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################
# Test -addFields + -select


def test_ogr2ogr_lib_19():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("foo"))
    lyr.CreateField(ogr.FieldDefn("bar"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f["bar"] = "foo"
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="MEM", selectFields=["foo"])
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
def test_ogr2ogr_lib_20(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("foo"))

    ds = gdal.VectorTranslate(tmp_vsimem / "out.gpkg", src_ds, format="GPKG")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "foo"
    ds = None


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_20a(tmp_vsimem):
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("foo"))
    lyr.CreateGeomField(ogr.GeomFieldDefn("bar"))

    ds = gdal.VectorTranslate(
        tmp_vsimem / "out.gpkg", src_ds, format="GPKG", selectFields=["bar"]
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "bar"
    ds = None


###############################################################################
# Verify -append and -select options are an invalid combination


def test_ogr2ogr_lib_21():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("foo"))
    lyr.CreateField(ogr.FieldDefn("bar"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f["bar"] = "foo"
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="MEM")
    with pytest.raises(Exception):
        gdal.VectorTranslate(ds, src_ds, accessMode="append", selectFields=["foo"])

    ds = None
    src_ds = None


###############################################################################


@pytest.mark.require_geos
def test_ogr2ogr_clipsrc_wkt_no_dst_geom():

    wkt = "POLYGON ((479461 4764494,479461 4764196,480012 4764196,480012 4764494,479461 4764494))"
    ds = gdal.VectorTranslate("", "../ogr/data/poly.shp", format="MEM", clipSrc=wkt)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 1
    ds = None


###############################################################################
# Check that ogr2ogr does data axis to CRS axis mapping adaptations in case
# of the output driver not following the mapping of the input dataset.


def test_ogr2ogr_axis_mapping_swap(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test_ogr2ogr_axis_mapping_swap.gml",
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
        tmp_vsimem / "test_ogr2ogr_axis_mapping_swap.gfs",
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
            tmp_vsimem / "test_ogr2ogr_axis_mapping_swap.gml",
            open_options=["INVERT_AXIS_ORDER_IF_LAT_LONG=NO"],
        )
    if ds is None:
        pytest.skip("GML reader not available")
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [1, 2]
    ds = None
    ds = gdal.VectorTranslate(
        tmp_vsimem / "test_ogr2ogr_axis_mapping_swap.shp",
        tmp_vsimem / "test_ogr2ogr_axis_mapping_swap.gml",
    )

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, "POINT (2 49)")


###############################################################################
# Test -ct


def test_ogr2ogr_lib_ct():

    ds = gdal.VectorTranslate(
        "",
        "../ogr/data/poly.shp",
        format="MEM",
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
        format="MEM",
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

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CIRCULARSTRING(0 0,1 0,0 0)"))
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="MEM", geometryType=geometryType)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbMultiLineString


###############################################################################
# Test -makevalid


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_lib_makevalid(tmp_vsimem):

    make_valid_available = ogrtest.have_geos()

    tmpfilename = tmp_vsimem / "tmp.csv"
    with gdaltest.tempfile(
        tmpfilename,
        """id,WKT
1,"POLYGON ((0 0,10 10,0 10,10 0,0 0))"
2,"POLYGON ((0 0,0 1,0.5 1,0.5 0.75,0.5 1,1 1,1 0,0 0))"
3,"GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(3 4,5 6))"
""",
    ):
        if make_valid_available:
            ds = gdal.VectorTranslate("", tmpfilename, format="MEM", makeValid=True)
        else:
            with pytest.raises(Exception):
                gdal.VectorTranslate("", tmpfilename, format="MEM", makeValid=True)
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


def test_ogr2ogr_lib_sql_filename(tmp_vsimem):

    with gdaltest.tempfile(
        tmp_vsimem / "my.sql",
        """-- initial comment\nselect\n'--''--' as literalfield,* from --comment\npoly\n-- trailing comment""",
    ):
        ds = gdal.VectorTranslate(
            "", "../ogr/data/poly.shp", options=f"-f MEM -sql @{tmp_vsimem}/my.sql"
        )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    assert lyr.GetLayerDefn().GetFieldIndex("literalfield") == 0


###############################################################################
# Verify -emptyStrAsNull


def test_ogr2ogr_emptyStrAsNull():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    lyr = src_ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = ""
    lyr.CreateFeature(f)

    ds = gdal.VectorTranslate("", src_ds, format="MEM", emptyStrAsNull=True)

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()

    assert f["foo"] is None, "expected empty string to be transformed to null"


###############################################################################
# Verify propagation of field domains


def test_ogr2ogr_fielddomain_():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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

    ds = gdal.VectorTranslate("", src_ds, format="MEM")
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetDomainName() == "my_domain"
    domain = ds.GetFieldDomain("my_domain")
    assert domain is not None
    assert domain.GetDomainType() == ogr.OFDT_GLOB

    # Test -resolveDomains
    ds = gdal.VectorTranslate("", src_ds, format="MEM", resolveDomains=True)
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

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("layer")

    ds = gdal.VectorTranslate(
        "", src_ds, options="-f MEM -a_srs EPSG:7665 -a_coord_epoch 2021.3"
    )
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3


###############################################################################
# Test -s_coord_epoch


@pytest.mark.require_proj(7, 2)
def test_ogr2ogr_s_coord_epoch():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(120 -40)"))
    src_lyr.CreateFeature(f)

    # ITRF2014 to GDA2020
    ds = gdal.VectorTranslate(
        "",
        src_ds,
        options="-f MEM -s_srs EPSG:9000 -s_coord_epoch 2030 -t_srs EPSG:7844",
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

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(120 -40)"))
    src_lyr.CreateFeature(f)

    # GDA2020 to ITRF2014
    ds = gdal.VectorTranslate(
        "",
        src_ds,
        options="-f MEM -t_srs EPSG:9000 -t_coord_epoch 2030 -s_srs EPSG:7844",
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetX(0) != 120 and abs(g.GetX(0) - 120) < 1e-5
    assert g.GetY(0) != -40 and abs(g.GetY(0) - -40) < 1e-5


###############################################################################
# Test laundering of geometry column name when outputting to PostgreSQL (#6261)


@pytest.mark.require_driver("PGDump")
def test_ogr2ogr_launder_geometry_column_name(tmp_vsimem):

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = srcDS.CreateLayer("test", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("SHAPE", ogr.wkbPoint))
    out_filename = tmp_vsimem / "test_ogr2ogr_launder_geometry_column_name.sql"
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
@pytest.mark.parametrize("output_format", ["GPKG", "SQLite"])
def test_ogr2ogr_upsert(tmp_vsimem, output_format):

    filename = tmp_vsimem / (
        "test_ogr_gpkg_upsert_without_fid." + output_format.lower()
    )

    def create_gpkg_file():
        ds = gdal.GetDriverByName(output_format).Create(
            filename, 0, 0, 0, gdal.GDT_Unknown
        )
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

    def create_src_file():
        src_filename = tmp_vsimem / "test_ogr_gpkg_upsert_src.gpkg"
        srcDS = gdal.GetDriverByName("GPKG").Create(
            src_filename, 0, 0, 0, gdal.GDT_Unknown
        )
        lyr = srcDS.CreateLayer("foo")
        assert lyr.CreateField(ogr.FieldDefn("other", ogr.OFTString)) == ogr.OGRERR_NONE
        unique_field = ogr.FieldDefn("unique_field", ogr.OFTString)
        unique_field.SetUnique(True)
        assert lyr.CreateField(unique_field) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("unique_field", "2")
        f.SetField("other", "foo")
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (10 10)"))
        lyr.CreateFeature(f)
        return srcDS

    if output_format == "SQLite":
        with pytest.raises(Exception, match="SQLite driver doest not support upsert"):
            gdal.VectorTranslate(filename, create_src_file(), accessMode="upsert")
    else:
        assert (
            gdal.VectorTranslate(filename, create_src_file(), accessMode="upsert")
            is not None
        )

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        f = lyr.GetFeature(2)
        assert f["unique_field"] == "2"
        assert f["other"] == "foo"
        assert f.GetGeometryRef().ExportToWkt() == "POINT (10 10)"


###############################################################################
# Test -t_srs to a driver that automatically reprojects to WGS 84


@pytest.mark.require_driver("GeoJSONSeq")
def test_ogr2ogr_lib_t_srs_ignored(tmp_vsimem):

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 49)"))
    srcLayer.CreateFeature(f)

    got_msg = []

    def my_handler(errorClass, errno, msg):
        if errorClass != gdal.CE_Debug:
            got_msg.append(msg)
        return

    with gdaltest.error_handler(my_handler):
        assert (
            gdal.VectorTranslate(
                tmp_vsimem / "out.txt",
                srcDS,
                format="GeoJSONSeq",
                dstSRS="EPSG:32631",
                reproject=True,
            )
            is not None
        )

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

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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
        format="MEM",
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

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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
        "", srcDS, format="MEM", spatFilter=[-180, 80, 180, 90], spatSRS="EPSG:4326"
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test -clipsrc with a clip datasource


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos
def test_ogr2ogr_lib_clipsrc_datasource(tmp_vsimem):

    # Prepare the data layer to clip
    src_filename = tmp_vsimem / "clip_src.gpkg"
    srcDS = gdal.GetDriverByName("GPKG").Create(src_filename, 0, 0, 0, gdal.GDT_Unknown)
    srcLayer = srcDS.CreateLayer("test", geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0, 2 2)"))
    srcLayer.CreateFeature(f)
    f = None
    srcDS = None

    # Prepare the data layers to clip with
    clip_path = tmp_vsimem / "clip_test.gpkg"
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
    # Clip geometry envelope contains envelope of input geometry, but does not intersect it
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField(
        "filter_field", "clip_geometry_envelope_contains_envelope_but_no_intersect"
    )
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((-2 -1,-2 4,4 4,4 -1,3 -1,3 3,-1 3,-1 -1,-2 -1))"
        )
    )
    clip_layer.CreateFeature(f)
    clip_ds = None

    # Test clip with 'half_overlap_line_result' using sql statement
    sql = "SELECT * FROM cliptest WHERE filter_field = 'half_overlap_line_result'"
    dst_filename = tmp_vsimem / "clip_dst.gpkg"
    dst_ds = gdal.VectorTranslate(
        dst_filename, src_filename, format="GPKG", clipSrc=clip_path, clipSrcSQL=sql
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_lyr.ResetReading()
    dst_feature = dst_lyr.GetNextFeature()
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2)"
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Test clip with the "exact_overlap_full_result" using clipSrcLayer + clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        dst_filename,
        src_filename,
        format="GPKG",
        clipSrc=clip_path,
        clipSrcLayer="cliptest",
        clipSrcWhere="filter_field = 'exact_overlap_full_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_lyr.ResetReading()
    dst_feature = dst_lyr.GetNextFeature()
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,2 2)"
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Test clip with the "no_overlap_no_result" using only clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        dst_filename,
        src_filename,
        format="GPKG",
        clipSrc=clip_path,
        clipSrcWhere="filter_field = 'no_overlap_no_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 0
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Test clip with the "clip_geometry_envelope_contains_envelope_but_no_intersect" using only clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        dst_filename,
        src_filename,
        format="GPKG",
        clipSrc=clip_path,
        clipSrcWhere="filter_field = 'clip_geometry_envelope_contains_envelope_but_no_intersect'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 0
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Cleanup
    gdal.Unlink(clip_path)


###############################################################################
# Test -clipsrc and intersection being of a lower dimensionality


@pytest.mark.require_geos
def test_ogr2ogr_lib_clipsrc_discard_lower_dimensionality():

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0, 1 1)"))
    srcLayer.CreateFeature(f)

    # Intersection of above geometry with clipSrc bounding box is a point
    ds = gdal.VectorTranslate("", srcDS, format="MEM", clipSrc=[-1, -1, 0, 0])
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0
    ds = None


###############################################################################
# Test -clipsrc/-clipdst with a clip layer with an invalid polygon (specified "inline" as WKT)


@pytest.mark.require_geos
@gdaltest.enable_exceptions()
@pytest.mark.parametrize("clipSrc", [True, False])
def test_ogr2ogr_lib_clip_invalid_polygon_inline(tmp_vsimem, clipSrc):

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0.25 0.25)"))
    srcLayer.CreateFeature(f)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-0.5 0.5)"))
    srcLayer.CreateFeature(f)

    # Intersection of above geometry with clipSrc bounding box is a point
    with pytest.raises(Exception, match="geometry is invalid"):
        gdal.VectorTranslate(
            "",
            srcDS,
            format="MEM",
            clipSrc="POLYGON((0 0,1 1,0 1,1 0,0 0))" if clipSrc else None,
            clipDst="POLYGON((0 0,1 1,0 1,1 0,0 0))" if not clipSrc else None,
        )

    with gdal.quiet_errors():
        ds = gdal.VectorTranslate(
            "",
            srcDS,
            format="MEM",
            makeValid=True,
            clipSrc="POLYGON((0 0,1 1,0 1,1 0,0 0))" if clipSrc else None,
            clipDst="POLYGON((0 0,1 1,0 1,1 0,0 0))" if not clipSrc else None,
        )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################
# Test -clipsrc with a clip layer with an invalid polygon (in a dataset)


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos
@gdaltest.enable_exceptions()
@pytest.mark.parametrize("clipSrc", [True, False])
def test_ogr2ogr_lib_clip_invalid_polygon(tmp_vsimem, clipSrc):

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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
    clip_path = tmp_vsimem / "clip_test.gpkg"
    clip_ds = gdal.GetDriverByName("GPKG").Create(clip_path, 0, 0, 0, gdal.GDT_Unknown)
    clip_layer = clip_ds.CreateLayer("cliptest", geom_type=ogr.wkbPolygon)
    f = ogr.Feature(clip_layer.GetLayerDefn())
    # Invalid polygon with self crossing
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,1 1,0 1,1 0,0 0))"))
    clip_layer.CreateFeature(f)
    clip_ds = None

    # Intersection of above geometry with clipSrc bounding box is a point
    with pytest.raises(Exception, match=r"cannot load.*clip geometry"):
        gdal.VectorTranslate(
            "",
            srcDS,
            format="MEM",
            clipSrc=clip_path if clipSrc else None,
            clipDst=clip_path if not clipSrc else None,
        )

    with gdal.quiet_errors():
        ds = gdal.VectorTranslate(
            "",
            srcDS,
            format="MEM",
            makeValid=True,
            clipSrc=clip_path if clipSrc else None,
            clipDst=clip_path if not clipSrc else None,
        )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################
# Test -clipsrc with 3d clip layer


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos
def test_ogr2ogr_lib_clipsrc_3d_polygon(tmp_vsimem):

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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
    clip_path = tmp_vsimem / "clip_test.gpkg"
    clip_ds = gdal.GetDriverByName("GPKG").Create(clip_path, 0, 0, 0, gdal.GDT_Unknown)
    clip_layer = clip_ds.CreateLayer("cliptest", geom_type=ogr.wkbPolygon)
    f = ogr.Feature(clip_layer.GetLayerDefn())
    # 3d polygon
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON Z ((0 0 0, 10 0 10, 10 5 10, 0 5 0, 0 0 0))")
    )
    clip_layer.CreateFeature(f)
    clip_ds = None

    with gdal.quiet_errors():
        ds = gdal.VectorTranslate("", srcDS, format="MEM", clipSrc=clip_path)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (0 0 0, 5 5 5)")

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (5 5 5, 10 0 10)")

    ds = None


###############################################################################
# Test -clipsrc argument errors


@pytest.mark.require_geos
def test_ogr2ogr_lib_clipsrc_argument_errors():

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srcDS.CreateLayer("test")

    with pytest.raises(Exception, match="-clipsrc option requires 1 argument"):
        gdal.VectorTranslate("", srcDS, options="-f MEM -clipsrc")

    with pytest.raises(Exception):
        gdal.VectorTranslate("", srcDS, options="-f MEM -clipsrc 1 2 3")

    with pytest.raises(Exception, match="Duplicate argument -clipsrc"):
        gdal.VectorTranslate("", srcDS, options="-f MEM -clipsrc 1 2 3 4 -clipsrc foo")


###############################################################################
# Test -clipdst with a clip datasource


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_geos
def test_ogr2ogr_lib_clipdst_datasource(tmp_vsimem):

    # Prepare the data layer to clip
    src_filename = tmp_vsimem / "clip_src.gpkg"
    srcDS = gdal.GetDriverByName("GPKG").Create(src_filename, 0, 0, 0, gdal.GDT_Unknown)
    srcLayer = srcDS.CreateLayer("test", geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0, 2 2)"))
    srcLayer.CreateFeature(f)
    f = None
    srcDS = None

    # Prepare the data layers to clip with
    clip_path = tmp_vsimem / "clip_test.gpkg"
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
    # Clip geometry envelope contains envelope of input geometry, but does not intersect it
    f = ogr.Feature(clip_layer.GetLayerDefn())
    f.SetField(
        "filter_field", "clip_geometry_envelope_contains_envelope_but_no_intersect"
    )
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((-2 -1,-2 4,4 4,4 -1,3 -1,3 3,-1 3,-1 -1,-2 -1))"
        )
    )
    clip_layer.CreateFeature(f)
    clip_ds = None

    # Test clip with 'half_overlap_line_result' using sql statement
    sql = "SELECT * FROM cliptest WHERE filter_field = 'half_overlap_line_result'"
    dst_filename = tmp_vsimem / "clip_dst.gpkg"
    dst_ds = gdal.VectorTranslate(
        dst_filename, src_filename, format="GPKG", clipDst=clip_path, clipDstSQL=sql
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_lyr.ResetReading()
    dst_feature = dst_lyr.GetNextFeature()
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,2 2)"
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Test clip with the "exact_overlap_full_result" using clipDstLayer + clipDstWhere
    dst_ds = gdal.VectorTranslate(
        dst_filename,
        src_filename,
        format="GPKG",
        clipDst=clip_path,
        clipDstLayer="cliptest",
        clipDstWhere="filter_field = 'exact_overlap_full_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1
    dst_lyr.ResetReading()
    dst_feature = dst_lyr.GetNextFeature()
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,2 2)"
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Test clip with the "no_overlap_no_result" using only clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        dst_filename,
        src_filename,
        format="GPKG",
        clipDst=clip_path,
        clipDstWhere="filter_field = 'no_overlap_no_result'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 0
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Test clip with the "clip_geometry_envelope_contains_envelope_but_no_intersect" using only clipSrcWhere
    dst_ds = gdal.VectorTranslate(
        dst_filename,
        src_filename,
        format="GPKG",
        clipDst=clip_path,
        clipDstWhere="filter_field = 'clip_geometry_envelope_contains_envelope_but_no_intersect'",
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 0
    dst_ds = None
    gdal.Unlink(dst_filename)

    # Cleanup
    gdal.Unlink(clip_path)


###############################################################################
# Test -clipdst and intersection being of a lower dimensionality


@pytest.mark.require_geos
def test_ogr2ogr_lib_clipdst_discard_lower_dimensionality():

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", srs=srs, geom_type=ogr.wkbLineString)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0, 1 1)"))
    srcLayer.CreateFeature(f)

    # Intersection of above geometry with clipDst bounding box is a point -> no result
    ds = gdal.VectorTranslate("", srcDS, format="MEM", clipDst=[-1, -1, 0, 0])
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0


###############################################################################
# Test -clipsrc / -clipdst with reprojection


@pytest.mark.require_geos
@pytest.mark.parametrize("clipSrc", [True, False])
def test_ogr2ogr_lib_clip_datasource_reprojection(tmp_vsimem, clipSrc):

    # Prepare the data layer to clip
    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs_4326 = osr.SpatialReference()
    srs_4326.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs_4326.ImportFromEPSG(4326)
    srcLayer = srcDS.CreateLayer("test", geom_type=ogr.wkbLineString, srs=srs_4326)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 49)"))
    srcLayer.CreateFeature(f)

    # Prepare the data layers to clip with
    clip_path = tmp_vsimem / "clip_test.shp"
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
        format="MEM",
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
# Test -clipdst argument errors


@pytest.mark.require_geos
def test_ogr2ogr_lib_clipdst_argument_errors():

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srcDS.CreateLayer("test")

    with pytest.raises(Exception, match="-clipdst option requires 1 argument"):
        gdal.VectorTranslate("", srcDS, options="-f MEM -clipdst")

    with pytest.raises(Exception):
        gdal.VectorTranslate("", srcDS, options="-f MEM -clipdst 1 2 3")

    with pytest.raises(Exception, match="Duplicate argument -clipdst"):
        gdal.VectorTranslate("", srcDS, options="-f MEM -clipdst 1 2 3 4 -clipdst foo")


###############################################################################
# Test using explodecollections


def test_ogr2ogr_lib_explodecollections():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    wkt = "MULTIPOLYGON (((0 0,5 5,10 0,0 0)),((5 5,0 10,10 10,5 5)))"
    f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
    src_lyr.CreateFeature(f)

    dst_ds = gdal.VectorTranslate("", src_ds, format="MEM", explodeCollections=True)

    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 2, "wrong feature count"


###############################################################################
# Test converting a layer with a fid string to GPKG


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_fid_string_to_gpkg():

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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
        "", src_path, format="MEM", mapFieldType=["Integer64=String"]
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
        format="MEM",
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

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0, 1 0, 10 0)"))
    src_lyr.CreateFeature(f)

    with gdaltest.enable_exceptions(), pytest.raises(
        Exception, match="Failed to parse"
    ):
        gdal.VectorTranslate("", src_ds, format="MEM", simplifyTolerance="reasonable")

    dst_ds = gdal.VectorTranslate("", src_ds, format="MEM", simplifyTolerance=5)

    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1, "wrong feature count"
    dst_feature = dst_lyr.GetFeature(0)
    assert dst_feature.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,10 0)"


###############################################################################
# Test using transactionSize


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("transaction_size", [0, 10, "unlimited"])
@gdaltest.disable_exceptions()
def test_ogr2ogr_lib_transaction_size(tmp_vsimem, transaction_size):

    ds = gdal.VectorTranslate(
        tmp_vsimem / "out.gpkg",
        "../ogr/data/poly.shp",
        format="GPKG",
        transactionSize=transaction_size,
    )

    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10, "wrong feature count"


###############################################################################
# Test -dateTimeTo


def test_ogr2ogr_lib_dateTimeTo():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo foo")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTCx")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTCx12")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC+15")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC+12:")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC+12:3")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC+12:34")
        with pytest.raises(Exception):
            gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC+12:345")

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC+03:00")
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

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 23:19:56.789+00"

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC-13:15")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 10:04:56.789-1315"

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC-13:30")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 09:49:56.789-1330"

    dst_ds = gdal.VectorTranslate("", src_ds, options="-f MEM -dateTimeTo UTC-13:45")
    dst_lyr = dst_ds.GetLayer(0)
    f = dst_lyr.GetNextFeature()
    assert f["dt"] == "2023/01/31 09:34:56.789-1345"


###############################################################################
# Test converting a list type to JSON (#7397)


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_convert_list_type_to_JSON(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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

    out_filename = tmp_vsimem / "test_ogr2ogr_lib_convert_list_type_to_JSON.gpkg"
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


###############################################################################
def test_ogr2ogr_lib_nlt_GEOMETRY_nlt_CURVE_TO_LINEAR():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbMultiCurve)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTICURVE(CIRCULARSTRING(0 0,1 1,2 0))"))
    src_lyr.CreateFeature(f)
    f = None

    dst_ds = gdal.VectorTranslate(
        "", src_ds, format="MEM", geometryType=["GEOMETRY", "CONVERT_TO_LINEAR"]
    )
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetGeomType() == ogr.wkbUnknown
    f = dst_lyr.GetNextFeature()
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiLineString


###############################################################################
@pytest.mark.parametrize(
    "nlt_value",
    [
        ["NONE", "POINT"],
        ["GEOMETRY", "POINT"],
        ["CONVERT_TO_LINEAR", "CONVERT_TO_CURVE"],
        ["CONVERT_TO_CURVE", "CONVERT_TO_LINEAR"],
        # Not supported but could likely make sense
        ["PROMOTE_TO_MULTI", "CONVERT_TO_CURVE"],
        # Not supported but could likely make sense
        ["CONVERT_TO_CURVE", "PROMOTE_TO_MULTI"],
        ["POINT", "LINESTRING"],
    ],
)
def test_ogr2ogr_lib_invalid_nlt_combinations(nlt_value):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("test", geom_type=ogr.wkbMultiCurve)
    with pytest.raises(Exception, match="Unsupported combination of -nlt arguments"):
        gdal.VectorTranslate("", src_ds, format="MEM", geometryType=nlt_value)


###############################################################################
# Just check we don't reject them


@pytest.mark.parametrize(
    "nlt_value",
    [
        ["GEOMETRY", "CONVERT_TO_LINEAR"],
        ["CONVERT_TO_LINEAR", "GEOMETRY"],
        ["LINESTRING", "CONVERT_TO_LINEAR"],
        ["CONVERT_TO_CURVE", "MULTICURVE"],
        ["MULTICURVE", "CONVERT_TO_CURVE"],
        ["CONVERT_TO_LINEAR", "PROMOTE_TO_MULTI"],
        ["PROMOTE_TO_MULTI", "CONVERT_TO_LINEAR"],
    ],
)
def test_ogr2ogr_lib_valid_nlt_combinations(nlt_value):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("test", geom_type=ogr.wkbMultiCurve)
    assert (
        gdal.VectorTranslate("", src_ds, format="MEM", geometryType=nlt_value)
        is not None
    )


###############################################################################
# Check fix for https://github.com/OSGeo/gdal/issues/8033


@pytest.mark.require_driver("GeoJSON")
@pytest.mark.skipif(
    test_cli_utilities.get_ogrinfo_path() is None, reason="ogrinfo not available"
)
def test_ogr2ogr_lib_geojson_output(tmp_path):

    tmpfilename = tmp_path / "out.geojson"
    out_ds = gdal.VectorTranslate(tmpfilename, pathlib.Path("../ogr/data/poly.shp"))

    # Check that the file can be read at that point. Use an external process
    # to check flushes are done correctly
    ogrinfo_path = test_cli_utilities.get_ogrinfo_path()
    ret = gdaltest.runexternal(ogrinfo_path + f" {tmpfilename} -al -so -json")
    assert json.loads(ret)["layers"][0]["featureCount"] == 10

    # Add a new feature after synchronization has been done
    out_lyr = out_ds.GetLayer(0)
    out_lyr.CreateFeature(ogr.Feature(out_lyr.GetLayerDefn()))

    # Now close output dataset
    del out_ds

    with ogr.Open(tmpfilename) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 11


###############################################################################
# Test option argument handling


def test_ogr2ogr_lib_dict_arguments():

    opt = gdal.VectorTranslateOptions(
        "__RETURN_OPTION_LIST__",
        datasetCreationOptions=collections.OrderedDict(
            (("GEOMETRY_ENCODING", "WKT"), ("FORMAT", "NC4"))
        ),
        layerCreationOptions=collections.OrderedDict(
            (("RECORD_DIM_NAME", "record"), ("STRING_DEFAULT_WIDTH", 10))
        ),
    )

    dsco_idx = opt.index("-dsco")

    assert opt[dsco_idx : dsco_idx + 4] == [
        "-dsco",
        "GEOMETRY_ENCODING=WKT",
        "-dsco",
        "FORMAT=NC4",
    ]

    lco_idx = opt.index("-lco")

    assert opt[lco_idx : lco_idx + 4] == [
        "-lco",
        "RECORD_DIM_NAME=record",
        "-lco",
        "STRING_DEFAULT_WIDTH=10",
    ]


###############################################################################
# Test reprojection of curve geometries (#8332)


@pytest.mark.require_geos
@pytest.mark.require_driver("CSV")
def test_ogr2ogr_lib_reprojection_curve_geometries_output_supports_curve():

    ds = gdal.VectorTranslate(
        "", "data/curvepolygon_epsg_32632.csv", format="MEM", dstSRS="EPSG:4326"
    )
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().IsValid()
    ogrtest.check_feature_geometry(
        f,
        "CURVEPOLYGON ((6.32498053379256 51.2471810297313,6.32502606593923 51.2471797414429,6.32508564301579 51.2471779468342,6.32508086959122 51.2471782154378,6.32507806889532 51.2471785371375,6.32507530829615 51.2471789745452,6.3250725999168 51.2471795257444,6.32506995565012 51.2471801883109,6.32506738710751 51.2471809593375,6.32506490556829 51.2471818354388,6.32506252192976 51.2471828127661,6.32506024665859 51.2471838870276,6.32505808974621 51.2471850535068,6.32505606066436 51.2471863070808,6.32505416832314 51.247187642245,6.32505242103235 51.2471890531379,6.32505082646468 51.2471905335619,6.32504939162233 51.2471920770166,6.32504812280579 51.2471936767262,6.32504702558728 51.2471953256621,6.3250461047848 51.2471970165858,6.3250453644417 51.2471987420737,6.32504480780908 51.2472004945467,6.32504443733128 51.2472022663088,6.32504425463539 51.2472040495786,6.3250442605232 51.2472058365305,6.32504445496953 51.247207619311,6.32504483711994 51.2472093900971,6.32504540529669 51.2472111411082,6.32504615700475 51.2472128646583,6.32504708894311 51.2472145531778,6.32515225866748 51.2473898992154,6.32518998816495 51.2474610608792,6.32522098154919 51.2475319875545,6.32522171616485 51.2475341449796,6.32524595411351 51.2476051690166,6.32526068947727 51.2476643642114,6.32526288212522 51.2476671379154,6.32526538680004 51.2476698055208,6.32526819054188 51.2476723532251,6.32527127884371 51.2476747678412,6.32527463572543 51.2476770368784,6.32527824381772 51.2476791485935,6.32528208445102 51.2476810920627,6.32528613775304 51.247682857227,6.32529038275035 51.2476844349552,6.32529479747832 51.2476858170827,6.32529935909332 51.2476869964598,6.32526584396464 51.2476849349575,6.32525050731474 51.2476839906581,6.32522156837336 51.2476820967396,6.32518488225128 51.2476798547758,6.3251875627635 51.2476793509009,6.32519018534573 51.2476787386101,6.32519273887938 51.2476780205053,6.32519521253848 51.2476771996278,6.32519759583568 51.2476762794572,6.32519987866684 51.2476752638949,6.32520205135352 51.2476741572484,6.32520410468458 51.247672964207,6.32520602995446 51.2476716898299,6.32520781900116 51.2476703395207,6.32520946423966 51.2476689190031,6.32521095869482 51.2476674342999,6.32521229603083 51.2476658917059,6.32521347057799 51.2476642977583,6.32521447735633 51.2476626592193,6.32521531209805 51.247660983034,6.32521597126403 51.2476592763064,6.32520359424617 51.247610064665,6.32517934697498 51.2475392023018,6.32514850453603 51.247468638836,6.32511179831814 51.2473993533697,6.32499141364508 51.2471992490109,6.3249886054451 51.2471969248155,6.32498555431837 51.2471947245403,6.32498227410828 51.2471926581675,6.32497877969637 51.2471907350727,6.32497508693738 51.2471889639805,6.32497121258528 51.2471873529274,6.32496717421784 51.2471859092223,6.32496299015727 51.247184639413,6.32495867938638 51.2471835492613,6.32495426146351 51.2471826437167,6.32494975643255 51.2471819268857,6.32498053379256 51.2471810297313))",
        1e-10,
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (4.50229717432855 0.0,4.51125606025315 0.009019375808399,4.52021516761635 0.0,4.51125606025315 -0.009019375808399,4.50229717432855 0.0)))",
        1e-10,
    )


###############################################################################
# Test reprojection of curve geometries (#8332)


@pytest.mark.parametrize("geometryType", ["POLYGON", "CONVERT_TO_LINEAR"])
@pytest.mark.require_driver("CSV")
def test_ogr2ogr_lib_reprojection_curve_geometries_forced_geom_type(geometryType):

    ds = gdal.VectorTranslate(
        "",
        "data/curvepolygon_epsg_32632.csv",
        format="MEM",
        dstSRS="EPSG:4326",
        geometryType=geometryType,
    )
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((6.32498053379256 51.2471810297313,6.32502606593923 51.2471797414429,6.32508564301579 51.2471779468342,6.32508086959122 51.2471782154378,6.32507806889532 51.2471785371375,6.32507530829615 51.2471789745452,6.3250725999168 51.2471795257444,6.32506995565012 51.2471801883109,6.32506738710751 51.2471809593375,6.32506490556829 51.2471818354388,6.32506252192976 51.2471828127661,6.32506024665859 51.2471838870276,6.32505808974621 51.2471850535068,6.32505606066436 51.2471863070808,6.32505416832314 51.247187642245,6.32505242103235 51.2471890531379,6.32505082646468 51.2471905335619,6.32504939162233 51.2471920770166,6.32504812280579 51.2471936767262,6.32504702558728 51.2471953256621,6.3250461047848 51.2471970165858,6.3250453644417 51.2471987420737,6.32504480780908 51.2472004945467,6.32504443733128 51.2472022663088,6.32504425463539 51.2472040495786,6.3250442605232 51.2472058365305,6.32504445496953 51.247207619311,6.32504483711994 51.2472093900971,6.32504540529669 51.2472111411082,6.32504615700475 51.2472128646583,6.32504708894311 51.2472145531778,6.32515225866748 51.2473898992154,6.32518998816495 51.2474610608792,6.32522098154919 51.2475319875545,6.32522171616485 51.2475341449796,6.32524595411351 51.2476051690166,6.32526068947727 51.2476643642114,6.32526288212522 51.2476671379154,6.32526538680004 51.2476698055208,6.32526819054188 51.2476723532251,6.32527127884371 51.2476747678412,6.32527463572543 51.2476770368784,6.32527824381772 51.2476791485935,6.32528208445102 51.2476810920627,6.32528613775304 51.247682857227,6.32529038275035 51.2476844349552,6.32529479747832 51.2476858170827,6.32529935909332 51.2476869964598,6.32526584396464 51.2476849349575,6.32525050731474 51.2476839906581,6.32522156837336 51.2476820967396,6.32518488225128 51.2476798547758,6.3251875627635 51.2476793509009,6.32519018534573 51.2476787386101,6.32519273887938 51.2476780205053,6.32519521253848 51.2476771996278,6.32519759583568 51.2476762794572,6.32519987866684 51.2476752638949,6.32520205135352 51.2476741572484,6.32520410468458 51.247672964207,6.32520602995446 51.2476716898299,6.32520781900116 51.2476703395207,6.32520946423966 51.2476689190031,6.32521095869482 51.2476674342999,6.32521229603083 51.2476658917059,6.32521347057799 51.2476642977583,6.32521447735633 51.2476626592193,6.32521531209805 51.247660983034,6.32521597126403 51.2476592763064,6.32520359424617 51.247610064665,6.32517934697498 51.2475392023018,6.32514850453603 51.247468638836,6.32511179831814 51.2473993533697,6.32499141364508 51.2471992490109,6.3249886054451 51.2471969248155,6.32498555431837 51.2471947245403,6.32498227410828 51.2471926581675,6.32497877969637 51.2471907350727,6.32497508693738 51.2471889639805,6.32497121258528 51.2471873529274,6.32496717421784 51.2471859092223,6.32496299015727 51.247184639413,6.32495867938638 51.2471835492613,6.32495426146351 51.2471826437167,6.32494975643255 51.2471819268857,6.32498053379256 51.2471810297313))",
        1e-10,
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((4.50229717432855 0.0,4.50231899745463 0.000629152087677,4.50238436052069 0.001255239123029,4.50249294510796 0.001875210984143,4.50264422224195 0.002486047337241,4.50283745496877 0.003084772349354,4.50307170194448 0.003668469184327,4.50334582201992 0.00423429421156,4.50365847979874 0.004779490858295,4.50400815214157 0.005301403038001,4.50439313558467 0.005797488089458,4.50481155463705 0.006265329163519,4.50526137091544 0.006702646997227,4.50574039307301 0.007107311017932,4.50624628747307 0.007477349723307,4.50677658955623 0.007810960286703,4.50732871584539 0.008106517341033,4.50789997653015 0.008362580898371,4.50848758856955 0.008577903366675,4.50908868924909 0.008751435629417,4.5097003501262 0.008882332158473,4.51031959129618 0.00896995513533,4.51094339590908 0.009013877560499,4.51156872486693 0.009013885335939,4.51219253162961 0.00896997831031,4.51281177705729 0.008882370281917,4.51342344421717 0.008751487959297,4.51402455308226 0.008577968884449,4.51461217505065 0.00836265832881,4.51518344721461 0.008106605177041,4.51573558630972 0.007811056818652,4.51626590227636 0.007477453072313,4.51677181136717 0.007107419172424,4.51725084873684 0.006702757852101,4.5177006804526 0.00626544056111,4.51811911486713 0.005797597861539,4.51850411329818 0.005301509047984,4.518853799963 0.004779591042816,4.51916647111912 0.004234386620641,4.51944060336679 0.003668552019331,4.51967486107274 0.00308484399799,4.51986810287894 0.002486106404952,4.52001938726475 0.001875256321243,4.52012797713514 0.001255269847082,4.52019334341276 0.000629167600675,4.52021516761635 0.0,4.52019334341276 -0.000629167600675,4.52012797713514 -0.001255269847082,4.52001938726475 -0.001875256321243,4.51986810287894 -0.002486106404952,4.51967486107274 -0.00308484399799,4.51944060336679 -0.003668552019331,4.51916647111912 -0.004234386620641,4.518853799963 -0.004779591042816,4.51850411329818 -0.005301509047984,4.51811911486713 -0.005797597861539,4.5177006804526 -0.00626544056111,4.51725084873684 -0.006702757852101,4.51677181136717 -0.007107419172424,4.51626590227636 -0.007477453072313,4.51573558630972 -0.007811056818652,4.51518344721461 -0.008106605177041,4.51461217505065 -0.00836265832881,4.51402455308226 -0.008577968884449,4.51342344421717 -0.008751487959297,4.51281177705729 -0.008882370281917,4.51219253162961 -0.00896997831031,4.51156872486693 -0.009013885335939,4.51094339590908 -0.009013877560499,4.51031959129618 -0.00896995513533,4.5097003501262 -0.008882332158473,4.50908868924909 -0.008751435629417,4.50848758856955 -0.008577903366675,4.50789997653015 -0.008362580898371,4.50732871584539 -0.008106517341033,4.50677658955623 -0.007810960286703,4.50624628747307 -0.007477349723307,4.50574039307301 -0.007107311017932,4.50526137091544 -0.006702646997227,4.50481155463705 -0.006265329163519,4.50439313558467 -0.005797488089458,4.50400815214157 -0.005301403038001,4.50365847979874 -0.004779490858295,4.50334582201992 -0.00423429421156,4.50307170194448 -0.003668469184327,4.50283745496877 -0.003084772349354,4.50264422224195 -0.002486047337241,4.50249294510796 -0.001875210984143,4.50238436052069 -0.001255239123029,4.50231899745463 -0.000629152087677,4.50229717432855 0.0))",
        1e-10,
    )


###############################################################################
# Test reprojection of curve geometries (#8332)


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GeoJSON")
def test_ogr2ogr_lib_reprojection_curve_geometries_output_does_not_support_curve(
    tmp_vsimem,
):

    out_filename = str(tmp_vsimem / "out.geojson")
    gdal.VectorTranslate(
        out_filename,
        "data/curvepolygon_epsg_32632.csv",
        format="GeoJSON",
        dstSRS="EPSG:4326",
    )
    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((6.32498053379256 51.2471810297313,6.32502606593923 51.2471797414429,6.32508564301579 51.2471779468342,6.32508086959122 51.2471782154378,6.32507806889532 51.2471785371375,6.32507530829615 51.2471789745452,6.3250725999168 51.2471795257444,6.32506995565012 51.2471801883109,6.32506738710751 51.2471809593375,6.32506490556829 51.2471818354388,6.32506252192976 51.2471828127661,6.32506024665859 51.2471838870276,6.32505808974621 51.2471850535068,6.32505606066436 51.2471863070808,6.32505416832314 51.247187642245,6.32505242103235 51.2471890531379,6.32505082646468 51.2471905335619,6.32504939162233 51.2471920770166,6.32504812280579 51.2471936767262,6.32504702558728 51.2471953256621,6.3250461047848 51.2471970165858,6.3250453644417 51.2471987420737,6.32504480780908 51.2472004945467,6.32504443733128 51.2472022663088,6.32504425463539 51.2472040495786,6.3250442605232 51.2472058365305,6.32504445496953 51.247207619311,6.32504483711994 51.2472093900971,6.32504540529669 51.2472111411082,6.32504615700475 51.2472128646583,6.32504708894311 51.2472145531778,6.32515225866748 51.2473898992154,6.32518998816495 51.2474610608792,6.32522098154919 51.2475319875545,6.32522171616485 51.2475341449796,6.32524595411351 51.2476051690166,6.32526068947727 51.2476643642114,6.32526288212522 51.2476671379154,6.32526538680004 51.2476698055208,6.32526819054188 51.2476723532251,6.32527127884371 51.2476747678412,6.32527463572543 51.2476770368784,6.32527824381772 51.2476791485935,6.32528208445102 51.2476810920627,6.32528613775304 51.247682857227,6.32529038275035 51.2476844349552,6.32529479747832 51.2476858170827,6.32529935909332 51.2476869964598,6.32526584396464 51.2476849349575,6.32525050731474 51.2476839906581,6.32522156837336 51.2476820967396,6.32518488225128 51.2476798547758,6.3251875627635 51.2476793509009,6.32519018534573 51.2476787386101,6.32519273887938 51.2476780205053,6.32519521253848 51.2476771996278,6.32519759583568 51.2476762794572,6.32519987866684 51.2476752638949,6.32520205135352 51.2476741572484,6.32520410468458 51.247672964207,6.32520602995446 51.2476716898299,6.32520781900116 51.2476703395207,6.32520946423966 51.2476689190031,6.32521095869482 51.2476674342999,6.32521229603083 51.2476658917059,6.32521347057799 51.2476642977583,6.32521447735633 51.2476626592193,6.32521531209805 51.247660983034,6.32521597126403 51.2476592763064,6.32520359424617 51.247610064665,6.32517934697498 51.2475392023018,6.32514850453603 51.247468638836,6.32511179831814 51.2473993533697,6.32499141364508 51.2471992490109,6.3249886054451 51.2471969248155,6.32498555431837 51.2471947245403,6.32498227410828 51.2471926581675,6.32497877969637 51.2471907350727,6.32497508693738 51.2471889639805,6.32497121258528 51.2471873529274,6.32496717421784 51.2471859092223,6.32496299015727 51.247184639413,6.32495867938638 51.2471835492613,6.32495426146351 51.2471826437167,6.32494975643255 51.2471819268857,6.32498053379256 51.2471810297313))",
        1e-10,
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((4.50229717432855 0.0,4.50231899745463 0.000629152087677,4.50238436052069 0.001255239123029,4.50249294510796 0.001875210984143,4.50264422224195 0.002486047337241,4.50283745496877 0.003084772349354,4.50307170194448 0.003668469184327,4.50334582201992 0.00423429421156,4.50365847979874 0.004779490858295,4.50400815214157 0.005301403038001,4.50439313558467 0.005797488089458,4.50481155463705 0.006265329163519,4.50526137091544 0.006702646997227,4.50574039307301 0.007107311017932,4.50624628747307 0.007477349723307,4.50677658955623 0.007810960286703,4.50732871584539 0.008106517341033,4.50789997653015 0.008362580898371,4.50848758856955 0.008577903366675,4.50908868924909 0.008751435629417,4.5097003501262 0.008882332158473,4.51031959129618 0.00896995513533,4.51094339590908 0.009013877560499,4.51156872486693 0.009013885335939,4.51219253162961 0.00896997831031,4.51281177705729 0.008882370281917,4.51342344421717 0.008751487959297,4.51402455308226 0.008577968884449,4.51461217505065 0.00836265832881,4.51518344721461 0.008106605177041,4.51573558630972 0.007811056818652,4.51626590227636 0.007477453072313,4.51677181136717 0.007107419172424,4.51725084873684 0.006702757852101,4.5177006804526 0.00626544056111,4.51811911486713 0.005797597861539,4.51850411329818 0.005301509047984,4.518853799963 0.004779591042816,4.51916647111912 0.004234386620641,4.51944060336679 0.003668552019331,4.51967486107274 0.00308484399799,4.51986810287894 0.002486106404952,4.52001938726475 0.001875256321243,4.52012797713514 0.001255269847082,4.52019334341276 0.000629167600675,4.52021516761635 0.0,4.52019334341276 -0.000629167600675,4.52012797713514 -0.001255269847082,4.52001938726475 -0.001875256321243,4.51986810287894 -0.002486106404952,4.51967486107274 -0.00308484399799,4.51944060336679 -0.003668552019331,4.51916647111912 -0.004234386620641,4.518853799963 -0.004779591042816,4.51850411329818 -0.005301509047984,4.51811911486713 -0.005797597861539,4.5177006804526 -0.00626544056111,4.51725084873684 -0.006702757852101,4.51677181136717 -0.007107419172424,4.51626590227636 -0.007477453072313,4.51573558630972 -0.007811056818652,4.51518344721461 -0.008106605177041,4.51461217505065 -0.00836265832881,4.51402455308226 -0.008577968884449,4.51342344421717 -0.008751487959297,4.51281177705729 -0.008882370281917,4.51219253162961 -0.00896997831031,4.51156872486693 -0.009013885335939,4.51094339590908 -0.009013877560499,4.51031959129618 -0.00896995513533,4.5097003501262 -0.008882332158473,4.50908868924909 -0.008751435629417,4.50848758856955 -0.008577903366675,4.50789997653015 -0.008362580898371,4.50732871584539 -0.008106517341033,4.50677658955623 -0.007810960286703,4.50624628747307 -0.007477349723307,4.50574039307301 -0.007107311017932,4.50526137091544 -0.006702646997227,4.50481155463705 -0.006265329163519,4.50439313558467 -0.005797488089458,4.50400815214157 -0.005301403038001,4.50365847979874 -0.004779490858295,4.50334582201992 -0.00423429421156,4.50307170194448 -0.003668469184327,4.50283745496877 -0.003084772349354,4.50264422224195 -0.002486047337241,4.50249294510796 -0.001875210984143,4.50238436052069 -0.001255239123029,4.50231899745463 -0.000629152087677,4.50229717432855 0.0))",
        1e-10,
    )


###############################################################################
# Test issue (#8523) when -preserve_fid was set even if -explodecollections was
# set with a GPKG MULTI layer


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("preserveFID", (True, False))
def test_translate_explodecollections_preserve_fid(tmp_vsimem, preserveFID):
    """Test issue #8523 when -preserve_fid was set even if -explodecollections was set"""

    with gdal.ExceptionMgr(useExceptions=True):

        src = tmp_vsimem / "test_collection.gpkg"
        dst = tmp_vsimem / "test_collection_exploded.gpkg"

        ds = ogr.GetDriverByName("GPKG").CreateDataSource(src)
        lyr = ds.CreateLayer("test_collection", None, ogr.wkbMultiPoint)

        wkt_geom = "MULTIPOINT((0 0), (1 1))"

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))

        lyr.CreateFeature(feat)

        del lyr
        del ds

        options = gdal.VectorTranslateOptions(
            explodeCollections=True, preserveFID=preserveFID
        )

        if preserveFID:
            with pytest.raises(
                RuntimeError,
                match="cannot use -preserve_fid and -explodecollections at the same time",
            ):
                gdal.VectorTranslate(srcDS=src, destNameOrDestDS=dst, options=options)

        else:
            ds_output = gdal.VectorTranslate(
                srcDS=src, destNameOrDestDS=dst, options=options
            )
            lyr = ds_output.GetLayerByName("test_collection")
            assert lyr.GetFeatureCount() == 2
            del lyr
            del ds_output


###############################################################################
# Test forced use of the Arrow interface


@pytest.mark.parametrize("limit", [None, 1])
def test_ogr2ogr_lib_OGR2OGR_USE_ARROW_API_YES(limit):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("str_field"))
    fld_defn = ogr.FieldDefn("json_field")
    fld_defn.SetSubType(ogr.OFSTJSON)
    src_lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("field_with_alternative_name")
    fld_defn.SetAlternativeName("alias")
    src_lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("field_with_comment")
    fld_defn.SetComment("my_comment")
    src_lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("field_with_default")
    fld_defn.SetDefault("'default_val'")
    src_lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("field_with_width")
    fld_defn.SetWidth(10)
    src_lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("field_unique")
    fld_defn.SetUnique(True)
    src_lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("field_with_domain", ogr.OFTInteger)
    fld_defn.SetDomainName("my_domain")
    src_lyr.CreateField(fld_defn)
    for i in range(2):
        f = ogr.Feature(src_lyr.GetLayerDefn())
        f["str_field"] = "foo%d" % i
        f["json_field"] = '{"foo":"bar"}'
        f["field_with_domain"] = 1 + i
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (%d 2)" % i))
        src_lyr.CreateFeature(f)

    assert src_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            "my_domain",
            "desc",
            ogr.OFTString,
            ogr.OFSTNone,
            {1: "one", 2: "two", 3: None},
        )
    )

    got_msg = []

    def my_handler(errorClass, errno, msg):
        got_msg.append(msg)
        return

    with gdaltest.error_handler(my_handler), gdaltest.config_options(
        {"CPL_DEBUG": "ON", "OGR2OGR_USE_ARROW_API": "YES"}
    ):
        out_ds = gdal.VectorTranslate(
            "",
            src_ds,
            format="MEM",
            limit=limit,
        )

    assert "OGR2OGR: Using WriteArrowBatch()" in got_msg

    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "str_field"
    assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTNone
    assert out_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "json_field"
    assert out_lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
    assert out_lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTJSON
    assert (
        out_lyr.GetLayerDefn().GetFieldDefn(2).GetName()
        == "field_with_alternative_name"
    )
    assert out_lyr.GetLayerDefn().GetFieldDefn(2).GetAlternativeName() == "alias"
    assert out_lyr.GetLayerDefn().GetFieldDefn(3).GetName() == "field_with_comment"
    assert out_lyr.GetLayerDefn().GetFieldDefn(3).GetComment() == "my_comment"
    assert out_lyr.GetLayerDefn().GetFieldDefn(4).GetName() == "field_with_default"
    assert out_lyr.GetLayerDefn().GetFieldDefn(4).GetDefault() == "'default_val'"
    assert out_lyr.GetLayerDefn().GetFieldDefn(5).GetName() == "field_with_width"
    assert out_lyr.GetLayerDefn().GetFieldDefn(5).GetWidth() == 10
    assert out_lyr.GetLayerDefn().GetFieldDefn(6).GetName() == "field_unique"
    assert out_lyr.GetLayerDefn().GetFieldDefn(6).IsUnique()
    assert out_lyr.GetLayerDefn().GetFieldDefn(7).GetName() == "field_with_domain"
    assert out_lyr.GetLayerDefn().GetFieldDefn(7).GetType() == ogr.OFTInteger
    assert out_lyr.GetLayerDefn().GetFieldDefn(7).GetDomainName() == "my_domain"
    assert out_lyr.GetFeatureCount() == (limit if limit else src_lyr.GetFeatureCount())

    f = out_lyr.GetNextFeature()
    assert f["str_field"] == "foo0"
    assert f["json_field"] == '{"foo":"bar"}'
    assert f["field_with_domain"] == 1
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (0 2)"

    if not limit:
        f = out_lyr.GetNextFeature()
        assert f["str_field"] == "foo1"
        assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"

    # Test append
    got_msg = []
    with gdaltest.error_handler(my_handler), gdaltest.config_options(
        {"CPL_DEBUG": "ON", "OGR2OGR_USE_ARROW_API": "YES"}
    ):
        gdal.VectorTranslate(
            out_ds,
            src_ds,
            accessMode="append",
        )

    assert "OGR2OGR: Using WriteArrowBatch()" in got_msg

    out_lyr = out_ds.GetLayer(0)
    assert (
        out_lyr.GetFeatureCount() == (limit if limit else src_lyr.GetFeatureCount()) + 2
    )


###############################################################################
# Test JSON types roundtrip


@pytest.mark.require_driver("GeoJSON")
@pytest.mark.require_driver("GPKG")
def test_json_types(tmp_vsimem):
    """Test JSON types"""

    def test_extended_types(lyr):
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()

        fd = f.GetFieldDefnRef(0)
        assert fd.GetType() == ogr.OFTString
        assert fd.GetSubType() == ogr.OFSTNone

        fd = f.GetFieldDefnRef(1)
        assert fd.GetType() == ogr.OFTIntegerList
        assert fd.GetSubType() == ogr.OFSTNone

        fd = f.GetFieldDefnRef(2)
        assert fd.GetType() == ogr.OFTString
        assert fd.GetSubType() == ogr.OFSTJSON

        fd = f.GetFieldDefnRef(3)
        assert fd.GetType() == ogr.OFTInteger
        assert fd.GetSubType() == ogr.OFSTNone

    def test_types(lyr):
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()

        fd = f.GetFieldDefnRef(0)
        assert fd.GetType() == ogr.OFTString
        assert fd.GetSubType() == ogr.OFSTNone

        fd = f.GetFieldDefnRef(1)
        assert fd.GetType() == ogr.OFTString
        assert fd.GetSubType() == ogr.OFSTJSON

        fd = f.GetFieldDefnRef(2)
        assert fd.GetType() == ogr.OFTString
        assert fd.GetSubType() == ogr.OFSTJSON

        fd = f.GetFieldDefnRef(3)
        assert fd.GetType() == ogr.OFTInteger
        assert fd.GetSubType() == ogr.OFSTNone

    with gdal.ExceptionMgr(useExceptions=True):

        src = str(tmp_vsimem / "test_json.geojson")
        dst = str(tmp_vsimem / "test_json.gpkg")

        data = """{
                "type": "FeatureCollection",
                "features": [
                    { "type": "Feature", "properties": { "str": "[5]", "int_list": [5], "map": {"foo": "bar", "baz": 5}, "int_lit": 5 }, "geometry": {"type": "Point", "coordinates": [ 1, 2 ]} }
                ]
            }"""
        f = gdal.VSIFOpenL(src, "wb")
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

        with gdal.OpenEx(src, gdal.OF_VECTOR | gdal.OF_READONLY) as ds:
            lyr = ds.GetLayer(0)
            test_extended_types(lyr)

        options = gdal.VectorTranslateOptions(layerName="test")

        ds_output = gdal.VectorTranslate(
            srcDS=src, destNameOrDestDS=dst, options=options
        )
        lyr = ds_output.GetLayerByName("test")

        test_types(lyr)

        # Write it back to json
        round_trip_dst = str(tmp_vsimem / "test_json_back.geojson")

        options = gdal.VectorTranslateOptions(
            layerCreationOptions={"AUTODETECT_JSON_STRINGS": "FALSE"}
        )
        gdal.VectorTranslate(
            srcDS=dst, destNameOrDestDS=round_trip_dst, options=options
        )

        with gdal.OpenEx(round_trip_dst, gdal.OF_VECTOR | gdal.OF_READONLY) as ds:
            lyr = ds.GetLayer(0)
            test_extended_types(lyr)


###############################################################################


@pytest.mark.parametrize("enable_exceptions", [True, False])
@pytest.mark.parametrize("enable_debug", [True, False])
@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_accumulerated_errors(tmp_vsimem, enable_exceptions, enable_debug):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetFID(1)
    src_lyr.CreateFeature(f)

    out_filename = str(tmp_vsimem / "test_ogr2ogr_lib_accumulerated_errors.gpkg")
    gdal.VectorTranslate(out_filename, src_ds)

    def my_handler(errorClass, errno, msg):
        pass

    with gdaltest.error_handler(my_handler if enable_debug else None):
        with gdaltest.config_option("CPL_DEBUG", "ON" if enable_debug else "OFF"):
            with gdal.ExceptionMgr(useExceptions=enable_exceptions):
                if enable_exceptions:
                    with pytest.raises(
                        Exception,
                        match=r"Unable to write feature 1 from layer test\.\nMay be caused by: failed to execute insert : UNIQUE constraint failed: test\.fid",
                    ):
                        gdal.VectorTranslate(
                            out_filename, src_ds, options="-preserve_fid -append"
                        )
                else:
                    assert (
                        gdal.VectorTranslate(
                            out_filename, src_ds, options="-preserve_fid -append"
                        )
                        is None
                    )


###############################################################################


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_gpkg_to_shp_preserved_fid(tmp_vsimem):

    src_filename = str(tmp_vsimem / "test_ogr2ogr_lib_gpkg_to_shp_preserved_fid.gpkg")
    src_ds = gdal.GetDriverByName("GPKG").Create(
        src_filename, 0, 0, 0, gdal.GDT_Unknown
    )
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("foo", "bar")
    src_lyr.CreateFeature(f)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("foo", "baz")
    src_lyr.CreateFeature(f)
    src_ds.Close()

    out_filename = str(tmp_vsimem / "test_ogr2ogr_lib_accumulerated_errors.shp")

    got_msg = []

    def my_handler(errorClass, errno, msg):
        if errorClass != gdal.CE_Debug:
            got_msg.append(msg)
        return

    with gdaltest.error_handler(my_handler):
        out_ds = gdal.VectorTranslate(out_filename, src_filename, preserveFID=True)
    assert got_msg == ["Feature id 1 not preserved", "Feature id 2 not preserved"]
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    assert f.GetFID() == 0
    assert f["foo"] == "bar"
    f = out_lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert f["foo"] == "baz"


###############################################################################


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_gpkg_to_shp_truncated_field_names(tmp_vsimem):

    src_filename = str(
        tmp_vsimem / "test_ogr2ogr_lib_gpkg_to_shp_truncated_field_names.gpkg"
    )
    src_ds = gdal.GetDriverByName("GPKG").Create(
        src_filename, 0, 0, 0, gdal.GDT_Unknown
    )
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("shortname"))
    src_lyr.CreateField(ogr.FieldDefn("too_long_for_shapefile"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("shortname", "foo")
    f.SetField("too_long_for_shapefile", "bar")
    src_lyr.CreateFeature(f)
    src_ds.Close()

    out_filename = str(
        tmp_vsimem / "test_ogr2ogr_lib_gpkg_to_shp_truncated_field_names.shp"
    )

    got_msg = []

    def my_handler(errorClass, errno, msg):
        if errorClass != gdal.CE_Debug:
            got_msg.append(msg)
        return

    with gdaltest.error_handler(my_handler):
        out_ds = gdal.VectorTranslate(out_filename, src_filename)
    assert got_msg == [
        "Normalized/laundered field name: 'too_long_for_shapefile' to 'too_long_f'"
    ]
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    assert f["shortname"] == "foo"
    assert f["too_long_f"] == "bar"


###############################################################################


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_coordinate_precision(tmp_vsimem):

    # Source layer without SRS
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("test")

    out_filename = str(tmp_vsimem / "test_ogr2ogr_lib_coordinate_precision.gpkg")

    with pytest.raises(
        Exception,
        match="Invalid value for -xyRes",
    ):
        gdal.VectorTranslate(out_filename, src_ds, xyRes="invalid")

    with pytest.raises(
        Exception,
        match="Invalid value for -xyRes",
    ):
        gdal.VectorTranslate(out_filename, src_ds, xyRes="1 invalid")

    with pytest.raises(
        Exception,
        match="Unit suffix for -xyRes cannot be used with an unknown destination SRS",
    ):
        gdal.VectorTranslate(out_filename, src_ds, xyRes="1e-2 m")

    with pytest.raises(
        Exception,
        match="Invalid value for -zRes",
    ):
        gdal.VectorTranslate(out_filename, src_ds, zRes="invalid")

    with pytest.raises(
        Exception,
        match="Invalid value for -zRes",
    ):
        gdal.VectorTranslate(out_filename, src_ds, zRes="1 invalid")

    with pytest.raises(
        Exception,
        match="Unit suffix for -zRes cannot be used with an unknown destination SRS",
    ):
        gdal.VectorTranslate(out_filename, src_ds, zRes="1e-2 m")

    with pytest.raises(
        Exception,
        match="Failed to parse 'invalid' as number",
    ):
        gdal.VectorTranslate(out_filename, src_ds, mRes="invalid")

    with pytest.raises(
        Exception,
        match="Failed to parse '1 invalid' as number",
    ):
        gdal.VectorTranslate(out_filename, src_ds, mRes="1 invalid")

    gdal.VectorTranslate(out_filename, src_ds, xyRes=1e-2, zRes=1e-3, mRes=1e-4)
    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-2
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-4
    ds.Close()

    out_filename2 = str(
        tmp_vsimem / "test_ogr2ogr_lib_coordinate_precision2.gpkg",
    )
    gdal.VectorTranslate(out_filename2, out_filename)
    ds = ogr.Open(out_filename2)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-2
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-4
    ds.Close()

    gdal.VectorTranslate(out_filename2, out_filename, setCoordPrecision=False)
    ds = ogr.Open(out_filename2)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 0
    ds.Close()

    # Source layer with a geographic SRS
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    src_ds.CreateLayer("test", srs=srs)

    gdal.VectorTranslate(out_filename, src_ds, xyRes="1e-2 m", zRes="1e-3 m", mRes=1e-4)
    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == pytest.approx(8.98315e-08)
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-4
    ds.Close()

    gdal.VectorTranslate(out_filename, src_ds, xyRes="10 mm", zRes="1 mm", mRes=1e-4)
    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == pytest.approx(8.98315e-08)
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-4
    ds.Close()

    gdal.VectorTranslate(out_filename, src_ds, xyRes="1e-7 deg")
    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-7
    assert prec.GetZResolution() == 0
    assert prec.GetMResolution() == 0
    ds.Close()

    # Source layer with a projected SRS
    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_ds.CreateLayer("test", srs=srs)

    gdal.VectorTranslate(out_filename, src_ds, xyRes="1e-7 deg")
    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == pytest.approx(0.0111319)
    assert prec.GetZResolution() == 0
    assert prec.GetMResolution() == 0
    ds.Close()

    # Test conversion of coordinate precision while reprojecting
    gdal.VectorTranslate(out_filename2, out_filename, dstSRS="EPSG:4326")
    ds = ogr.Open(out_filename2)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == pytest.approx(1e-7)
    assert prec.GetZResolution() == 0
    assert prec.GetMResolution() == 0
    ds.Close()


###############################################################################


def test_ogr2ogr_lib_coordinate_precision_with_geom():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (1 1,9 9)"))
    src_lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate("", src_ds, format="MEM", xyRes=10)
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    if ogr.GetGEOSVersionMajor() > 0:
        assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,10 10)"
    else:
        assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 1,9 9)"


###############################################################################


def test_ogr2ogr_lib_not_enough_gcp():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("test")

    with pytest.raises(
        Exception, match="Failed to compute GCP transform: Not enough points available"
    ):
        gdal.VectorTranslate("", src_ds, options="-f MEM -gcp 0 0 0 0")


###############################################################################


def test_ogr2ogr_lib_two_gcps():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 3)"))
    src_lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate(
        "", src_ds, options="-f MEM -gcp 1 2 200 300 -gcp 3 4 300 400"
    )
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    assert f.GetGeometryRef().GetX(0) == pytest.approx(250)
    assert f.GetGeometryRef().GetY(0) == pytest.approx(350)


###############################################################################
# Test -skipInvalid


@pytest.mark.require_geos
@gdaltest.enable_exceptions()
def test_ogr2ogr_lib_skip_invalid(tmp_vsimem):

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srcLayer = srcDS.CreateLayer("test")
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    srcLayer.CreateFeature(f)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,1 1,0 1,1 0,0 0))"))
    srcLayer.CreateFeature(f)

    with gdal.quiet_errors():
        ds = gdal.VectorTranslate("", srcDS, format="MEM", skipInvalid=True)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################
# Test -t_srs in Arrow code path


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("force_reproj_threading", [False, True])
@pytest.mark.parametrize("source_driver", ["GPKG", "Parquet"])
def test_ogr2ogr_lib_reproject_arrow(tmp_vsimem, source_driver, force_reproj_threading):

    src_driver = gdal.GetDriverByName(source_driver)
    if src_driver is None:
        pytest.skip(f"{source_driver} is not available")
    src_filename = str(tmp_vsimem / ("in." + source_driver.lower()))
    with src_driver.Create(src_filename, 0, 0, 0, gdal.GDT_Unknown) as srcDS:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        srcLayer = srcDS.CreateLayer("test", srs=srs)
        f = ogr.Feature(srcLayer.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(500000 4500000)"))
        srcLayer.CreateFeature(f)
        f = ogr.Feature(srcLayer.GetLayerDefn())
        srcLayer.CreateFeature(f)
        f = ogr.Feature(srcLayer.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(500000 4000000)"))
        srcLayer.CreateFeature(f)

    config_options = {"CPL_DEBUG": "ON", "OGR2OGR_USE_ARROW_API": "YES"}
    if force_reproj_threading:
        config_options["OGR2OGR_MIN_FEATURES_FOR_THREADED_REPROJ"] = "0"

    with gdal.OpenEx(src_filename) as src_ds:
        for i in range(2):

            got_msg = []

            def my_handler(errorClass, errno, msg):
                got_msg.append(msg)
                return

            with gdaltest.error_handler(my_handler), gdaltest.config_options(
                config_options
            ):
                ds = gdal.VectorTranslate("", src_ds, format="MEM", dstSRS="EPSG:4326")

            assert "OGR2OGR: Using WriteArrowBatch()" in got_msg

            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 3
            f = lyr.GetNextFeature()
            ogrtest.check_feature_geometry(f, "POINT(3 40.65085651557158)")
            f = lyr.GetNextFeature()
            assert f.GetGeometryRef() is None
            f = lyr.GetNextFeature()
            ogrtest.check_feature_geometry(f, "POINT(3 36.14471809881776)")


###############################################################################
# Test -t_srs in Arrow code path in a situation where it cannot be triggered
# currently (source CRS is crossing anti-meridian)


@gdaltest.enable_exceptions()
@pytest.mark.require_geos
@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_reproject_arrow_optim_cannot_trigger(tmp_vsimem):

    src_filename = str(tmp_vsimem / "in.gpkg")
    with gdal.GetDriverByName("GPKG").Create(
        src_filename, 0, 0, 0, gdal.GDT_Unknown
    ) as srcDS:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32660)
        srcLayer = srcDS.CreateLayer("test", srs=srs)
        f = ogr.Feature(srcLayer.GetLayerDefn())
        f.SetGeometry(
            ogr.CreateGeometryFromWkt(
                "LINESTRING(657630.64 4984896.17,815261.43 4990738.26)"
            )
        )
        srcLayer.CreateFeature(f)

    got_msg = []

    def my_handler(errorClass, errno, msg):
        got_msg.append(msg)
        return

    config_options = {"CPL_DEBUG": "ON", "OGR2OGR_USE_ARROW_API": "YES"}
    with gdaltest.error_handler(my_handler), gdaltest.config_options(config_options):
        ds = gdal.VectorTranslate("", src_filename, format="MEM", dstSRS="EPSG:4326")

    assert "OGR2OGR: Using WriteArrowBatch()" not in got_msg

    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiLineString
    assert f.GetGeometryRef().GetGeometryCount() == 2


###############################################################################
# Test -ct in Arrow code path
# Cf https://github.com/OSGeo/gdal/issues/11438


@gdaltest.enable_exceptions()
def test_ogr2ogr_lib_reproject_arrow_optim_ct(tmp_vsimem):

    srcDS = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32632)
    srcLayer = srcDS.CreateLayer("test", srs=srs)
    f = ogr.Feature(srcLayer.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    srcLayer.CreateFeature(f)

    got_msg = []

    def my_handler(errorClass, errno, msg):
        got_msg.append(msg)
        return

    config_options = {"CPL_DEBUG": "ON", "OGR2OGR_USE_ARROW_API": "YES"}
    with gdaltest.error_handler(my_handler), gdaltest.config_options(config_options):
        ds = gdal.VectorTranslate(
            "",
            srcDS,
            format="MEM",
            reproject=True,
            coordinateOperation="+proj=affine +s11=-1",
        )

    assert "OGR2OGR: Using WriteArrowBatch()" in got_msg

    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32632"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (-1 2)"


###############################################################################
# Test -explodecollections on empty geometries


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "input_wkt,expected_output_wkt",
    [
        ("MULTIPOINT EMPTY", "POINT EMPTY"),
        ("MULTIPOINT Z EMPTY", "POINT Z EMPTY"),
        ("MULTIPOINT M EMPTY", "POINT M EMPTY"),
        ("MULTIPOINT ZM EMPTY", "POINT ZM EMPTY"),
        ("MULTILINESTRING EMPTY", "LINESTRING EMPTY"),
        ("MULTIPOLYGON EMPTY", "POLYGON EMPTY"),
        ("MULTICURVE EMPTY", "COMPOUNDCURVE EMPTY"),
        ("MULTISURFACE EMPTY", "CURVEPOLYGON EMPTY"),
        ("GEOMETRYCOLLECTION EMPTY", "GEOMETRYCOLLECTION EMPTY"),
    ],
)
def test_ogr2ogr_lib_explodecollections_empty_geoms(input_wkt, expected_output_wkt):

    with gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown) as src_ds:
        src_lyr = src_ds.CreateLayer("test")
        f = ogr.Feature(src_lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(input_wkt))
        src_lyr.CreateFeature(f)

        out_ds = gdal.VectorTranslate("", src_ds, explodeCollections=True, format="MEM")
        out_lyr = out_ds.GetLayer(0)
        f = out_lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToIsoWkt() == expected_output_wkt


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_arrow_datetime_as_string(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.gpkg")
    with ogr.GetDriverByName("GPKG").CreateDataSource(src_filename) as src_ds:
        src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone)

        field = ogr.FieldDefn("dt", ogr.OFTDateTime)
        src_lyr.CreateField(field)

        f = ogr.Feature(src_lyr.GetLayerDefn())
        src_lyr.CreateFeature(f)

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f.SetField("dt", "2022-05-31T12:34:56.789Z")
        src_lyr.CreateFeature(f)

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f.SetField("dt", "2022-05-31T12:34:56")
        src_lyr.CreateFeature(f)

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f.SetField("dt", "2022-05-31T12:34:56+12:30")
        src_lyr.CreateFeature(f)

    got_msg = []

    def my_handler(errorClass, errno, msg):
        got_msg.append(msg)
        return

    with gdaltest.error_handler(my_handler), gdaltest.config_options(
        {"CPL_DEBUG": "ON", "OGR2OGR_USE_ARROW_API": "YES"}
    ):
        dst_ds = gdal.VectorTranslate("", src_filename, format="MEM")

    assert "OGR2OGR: Using WriteArrowBatch()" in got_msg

    dst_lyr = dst_ds.GetLayer(0)
    assert [f.GetField("dt") for f in dst_lyr] == [
        None,
        "2022/05/31 12:34:56.789+00",
        "2022/05/31 12:34:56",
        "2022/05/31 12:34:56+1230",
    ]


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_transfer_gpkg_relationships(tmp_vsimem):

    out_filename = str(tmp_vsimem / "relationships.gpkg")
    gdal.VectorTranslate(out_filename, "../ogr/data/gpkg/relation_mapping_table.gpkg")

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRelationshipNames() == ["a_b_attributes"]
        relationship = ds.GetRelationship("a_b_attributes")
        assert relationship.GetLeftTableName() == "a"
        assert relationship.GetRightTableName() == "b"
        assert relationship.GetMappingTableName() == "my_mapping_table"

    gdal.VectorTranslate(
        out_filename,
        "../ogr/data/gpkg/relation_mapping_table.gpkg",
        layers=["a", "my_mapping_table"],
    )
    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRelationshipNames() is None

    gdal.VectorTranslate(
        out_filename,
        "../ogr/data/gpkg/relation_mapping_table.gpkg",
        layers=["b", "my_mapping_table"],
    )
    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRelationshipNames() is None

    gdal.VectorTranslate(
        out_filename, "../ogr/data/gpkg/relation_mapping_table.gpkg", layers=["a", "b"]
    )
    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRelationshipNames() is None


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("OpenFileGDB")
def test_ogr2ogr_lib_transfer_filegdb_relationships(tmp_vsimem):

    out_filename = str(tmp_vsimem / "relationships.gdb")
    gdal.VectorTranslate(
        out_filename, "../ogr/data/filegdb/relationships.gdb", format="OpenFileGDB"
    )

    with gdal.OpenEx(out_filename) as ds:
        assert set(ds.GetRelationshipNames()) == set(
            [
                "composite_many_to_many",
                "composite_one_to_one",
                "points__ATTACHREL",
                "simple_attributed",
                "simple_backward_message_direction",
                "simple_both_message_direction",
                "simple_forward_message_direction",
                "simple_many_to_many",
                "simple_one_to_many",
                "simple_relationship_one_to_one",
            ]
        )
        relationship = ds.GetRelationship("composite_many_to_many")
        assert relationship.GetLeftTableName() == "table6"
        assert relationship.GetRightTableName() == "table7"
        assert relationship.GetMappingTableName() == "composite_many_to_many"


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("OGR2OGR_USE_ARROW_API", ["YES", "NO"])
def test_ogr2ogr_lib_datetime_in_shapefile(tmp_vsimem, OGR2OGR_USE_ARROW_API):

    src_filename = str(tmp_vsimem / "src.gpkg")
    with ogr.GetDriverByName("GPKG").CreateDataSource(src_filename) as src_ds:
        src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone)

        field = ogr.FieldDefn("dt", ogr.OFTDateTime)
        src_lyr.CreateField(field)
        f = ogr.Feature(src_lyr.GetLayerDefn())
        f.SetField("dt", "2022-05-31T12:34:56.789+05:30")
        src_lyr.CreateFeature(f)

    got_msg = []

    def my_handler(errorClass, errno, msg):
        got_msg.append(msg)
        return

    out_filename = str(tmp_vsimem / "out.dbf")
    with gdaltest.error_handler(my_handler), gdaltest.config_options(
        {"CPL_DEBUG": "ON", "OGR2OGR_USE_ARROW_API": OGR2OGR_USE_ARROW_API}
    ):
        gdal.VectorTranslate(out_filename, src_filename)

    if OGR2OGR_USE_ARROW_API == "YES":
        assert "OGR2OGR: Using WriteArrowBatch()" in got_msg
    else:
        assert "OGR2OGR: Using WriteArrowBatch()" not in got_msg

    assert "Field dt created as String field, though DateTime requested." in got_msg

    with ogr.Open(out_filename) as dst_ds:
        dst_lyr = dst_ds.GetLayer(0)
        assert [f.GetField("dt") for f in dst_lyr] == ["2022-05-31T12:34:56.789+05:30"]


###############################################################################
# Test that we warn if different coordinate operations are used


@gdaltest.disable_exceptions()
@pytest.mark.require_proj(9, 1)
@pytest.mark.parametrize("source_format", ["MEM", "GPKG"])
def test_ogr2ogr_lib_warn_different_coordinate_operations(tmp_vsimem, source_format):

    if gdal.GetDriverByName(source_format) is None:
        pytest.skip(f"Skipping as {source_format} is not available")

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4267)  # NAD27
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_lyr = src_ds.CreateLayer("test", srs=srs)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(-100 60)"))
    src_lyr.CreateFeature(f)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(-70 40)"))
    src_lyr.CreateFeature(f)

    if source_format == "GPKG":
        src_ds = gdal.VectorTranslate(tmp_vsimem / "tmp.gpkg", src_ds)

    with gdal.quiet_errors():
        gdal.VectorTranslate(
            "",
            src_ds,
            format="MEM",
            dstSRS="EPSG:4326",  # WGS 84
        )
        assert gdal.GetLastErrorMsg().startswith(
            "Several coordinate operations have been used to transform layer test."
        )

    gdal.ErrorReset()
    gdal.VectorTranslate(
        "",
        src_ds,
        format="MEM",
        dstSRS="EPSG:4326",  # WGS 84
        coordinateOperationOptions={"WARN_ABOUT_DIFFERENT_COORD_OP": "NO"},
    )
    assert gdal.GetLastErrorMsg() == ""

    # Try ALLOW_BALLPARK and ONLY_BEST options
    with gdal.quiet_errors():
        gdal.VectorTranslate(
            "",
            src_ds,
            format="MEM",
            dstSRS="EPSG:4326",  # WGS 84
            coordinateOperationOptions=["ALLOW_BALLPARK=NO", "ONLY_BEST=YES"],
        )


###############################################################################
# Test callback with erroneously big feature count


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_lib_progress_huge_feature_count():

    tab = [0]
    # Just check it does not crash
    gdal.VectorTranslate(
        "",
        "../ogr/data/gpkg/huge_feature_count.gpkg",
        format="MEM",
        callback=mycallback,
        callback_data=tab,
    )


###############################################################################


@pytest.mark.require_geos
def test_ogr2ogr_lib_clip_promote_poly_to_multipoly():

    src_ds = gdaltest.wkt_ds(
        "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))", geom_type=ogr.wkbMultiPolygon
    )
    out_ds = gdal.VectorTranslate(
        "", src_ds, format="MEM", clipSrc="POLYGON((1 1,2 1,2 2,1 2,1 1))"
    )
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiPolygon


###############################################################################


@pytest.mark.require_geos
def test_ogr2ogr_lib_clip_promote_poly_to_geometry_collection():

    src_ds = gdaltest.wkt_ds(
        "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))", geom_type=ogr.wkbGeometryCollection
    )
    out_ds = gdal.VectorTranslate(
        "", src_ds, format="MEM", clipSrc="POLYGON((1 1,2 1,2 2,1 2,1 1))"
    )
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbGeometryCollection
    assert f.GetGeometryRef().GetGeometryRef(0).GetGeometryType() == ogr.wkbPolygon


###############################################################################


@pytest.mark.require_geos
def test_ogr2ogr_lib_clip_demote_multipoly_to_poly():

    src_ds = gdaltest.wkt_ds(
        "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))", geom_type=ogr.wkbPolygon
    )
    out_ds = gdal.VectorTranslate(
        "", src_ds, format="MEM", clipSrc="POLYGON((1 1,2 1,2 2,1 2,1 1))"
    )
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon


###############################################################################


@pytest.mark.require_geos
def test_ogr2ogr_lib_clip_promote_poly_to_geometry_collection_bis():

    src_ds = gdaltest.wkt_ds(
        "GEOMETRYCOLLECTION(POINT(0 0),POINT(10 10))",
        geom_type=ogr.wkbGeometryCollection,
    )
    out_ds = gdal.VectorTranslate(
        "", src_ds, format="MEM", clipSrc="POLYGON((0 0,0 10,10 10,10 0,0 0))"
    )
    out_lyr = out_ds.GetLayer(0)
    f = out_lyr.GetNextFeature()
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbGeometryCollection
    assert f.GetGeometryRef().GetGeometryCount() == 2


###############################################################################


@pytest.mark.require_geos
def test_ogr2ogr_lib_wrapdateline_useless():

    src_ds = gdaltest.wkt_ds(
        "POLYGON ((-83.0556348903452 8.26039022660911,-83.0556348903452 8.25855541870704,-83.0536624718505 8.25852920716559,-83.0536821305066 8.26037712083838,-83.0556348903452 8.26039022660911))",
        geom_type=ogr.wkbPolygon,
        epsg=4326,
    )
    with gdaltest.error_raised(gdal.CE_None):
        out_ds = gdal.VectorTranslate(
            "", src_ds, options="-of MEM -t_srs EPSG:32617 -wrapdateline"
        )
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((273569.876923437 913668.344183491,273568.830352505 913465.374678854,273786.170063323 913461.355034812,273785.056779618 913665.785238482,273569.876923437 913668.344183491))",
    )
