#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR MiraMon vector driver.
# Author:   Abel Pau <a dot pau at creaf.uab.cat>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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

# import os
# import pdb

import gdaltest

# import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("MiraMonVector")

###############################################################################
# basic point test


def check_simple_point(ds):

    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 3
    assert lyr.GetGeomType() == ogr.wkbPoint

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert (
        f.GetGeometryRef().ExportToWkt() == "POINT (513.488106565226 848.806850618409)"
    )

    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("LOGICALY"))
        .GetSubType()
        == ogr.OFSTBoolean
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("LOGICALN"))
        .GetSubType()
        == ogr.OFSTBoolean
    )

    assert f.GetField("ID_GRAFIC") == 0
    assert f.GetFieldAsString("ATT1") == "A"
    assert f.GetFieldAsString("ATTRIBUTE_2") == "B"

    assert f.GetField("LOGICALY") == 1
    assert f.GetField("LOGICALN") == 0

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert (
        f.GetGeometryRef().ExportToWkt() == "POINT (342.325404376834 715.680304471881)"
    )
    assert f.GetField("ID_GRAFIC") == 1
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATTRIBUTE_2") == "D"
    assert f.GetField("LOGICALY") == 1
    assert f.GetField("LOGICALN") == 0

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert (
        f.GetGeometryRef().ExportToWkt() == "POINT (594.503182156354 722.692543360232)"
    )
    assert f.GetField("ID_GRAFIC") == 2
    assert f.GetFieldAsString("ATT1") == ""
    assert f.GetFieldAsString("ATTRIBUTE_2") == ""
    assert f.GetField("LOGICALY") == 1
    assert f.GetField("LOGICALN") == 0


def test_ogr_miramon_read_simple_point():

    ds = gdal.OpenEx("data/miramon/Points/SimplePoints/SimplePointsFile.pnt")
    assert ds is not None, "Failed to get dataset"

    check_simple_point(ds)


def test_ogr_miramon_write_simple_point_EmptyVersion(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_point(ds)


@pytest.mark.parametrize(
    "version",
    [
        "V1.1",
        "V2.0",
        "last_version",
        "VX.0",
    ],
)
def test_ogr_miramon_write_simple_point_V11(tmp_vsimem, version):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        format="MiraMonVector",
        options="-lco Version=" + version,
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_point(ds)


###############################################################################
# basic linestring test


def check_simple_arc(ds):

    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 4
    assert lyr.GetGeomType() == ogr.wkbLineString

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (351.333967649907 610.58039961936,474.450999048575 824.784015223546,758.721217887776 838.797335870549,1042.99143672698 610.58039961936,1369.30161750719 562.534728829636)"
    )
    assert f.GetField("ID_GRAFIC") == 0
    assert f.GetField("N_VERTEXS") == 5
    assert f.GetField("LONG_ARC") == pytest.approx(1226.052754666, abs=1e-5)
    assert f.GetField("NODE_INI") == 0
    assert f.GetField("NODE_FI") == 1
    assert f.GetFieldAsString("ATT1") == "A"
    assert f.GetFieldAsString("ATT2") == "B"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 1
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (794.755470980069 442.420551855326,613.583254043818 399.379638439531,642.61084681261 212.201712654565,861.819219790726 201.191246431919,1041.99048525219 460.437678401472,598.568981922029 591.562321598428,1109.05423406285 931.88582302564)"
    )
    assert f.GetField("ID_GRAFIC") == 1
    assert f.GetField("N_VERTEXS") == 7
    assert f.GetField("LONG_ARC") == pytest.approx(1986.750568, abs=1e-5)
    assert f.GetField("NODE_INI") == 2
    assert f.GetField("NODE_FI") == 3
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 2
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (887.843958135159 858.816365366268,989.941008563323 767.729781160749)"
    )
    assert f.GetField("ID_GRAFIC") == 2
    assert f.GetField("N_VERTEXS") == 2
    assert f.GetField("LONG_ARC") == pytest.approx(136.823147, abs=1e-5)
    assert f.GetField("NODE_INI") == 4
    assert f.GetField("NODE_FI") == 5
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 3
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (537.510941960088 719.684110371025,496.471931493865 633.602283539436,432.411037107567 572.544243577495,415.394862036206 631.600380589864,492.468125594722 642.610846812509,564.536631779308 630.599429115078)"
    )
    assert f.GetField("ID_GRAFIC") == 3
    assert f.GetField("N_VERTEXS") == 6
    assert f.GetField("LONG_ARC") == pytest.approx(396.238966, abs=1e-5)
    assert f.GetField("NODE_INI") == 6
    assert f.GetField("NODE_FI") == 7
    assert f.GetFieldAsString("ATT1") == "E"
    assert f.GetFieldAsString("ATT2") == "FÈÊ"


def test_ogr_miramon_read_simple_arc():

    ds = gdal.OpenEx("data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc")
    assert ds is not None, "Failed to get dataset"
    check_simple_arc(ds)


def test_ogr_miramon_write_simple_arc_EmptyVersion(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_arc(ds)
    del ds


def test_ogr_miramon_write_simple_arc_V11(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        format="MiraMonVector",
        options="-lco Version=V1.1",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_arc(ds)
    del ds


def test_ogr_miramon_write_simple_arc_V20(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        format="MiraMonVector",
        options="-lco Version=V2.0",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_arc(ds)
    del ds


def test_ogr_miramon_write_simple_arc_last_version(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        format="MiraMonVector",
        options="-lco Version=last_version",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_arc(ds)
    del ds


###############################################################################
# basic polygon test


def check_simple_polygon(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 3
    assert lyr.GetGeomType() == ogr.wkbPolygon

    # going to the first polygon
    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((335.318744053333 769.731684110321,552.525214081877 856.814462416696,775.737392959137 707.672692673594,648.616555661325 493.469077069408,386.367269267414 498.473834443337,335.318744053333 769.731684110321))"
    )
    assert f.GetField("ID_GRAFIC") == 1
    assert f.GetField("N_VERTEXS") == 6
    assert f.GetField("PERIMETRE") == pytest.approx(1289.866489495, abs=1e-5)
    assert f.GetField("AREA") == pytest.approx(112471.221989, abs=1e-5)
    assert f.GetField("N_ARCS") == 1
    assert f.GetField("N_POLIG") == 1
    assert f.GetFieldAsString("ATT1") == "A"
    assert f.GetFieldAsString("ATT2") == "B"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 1
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((1068.01522359662 849.807802093194,1160.10275927693 795.756422454755,1224.16365366323 682.648905803946,1156.09895337779 525.499524262557,962.915318744103 489.465271170264,830.789724072362 617.587059942862,924.879162702239 740.704091341529,1068.01522359662 849.807802093194))"
    )
    assert f.GetField("ID_GRAFIC") == 2
    assert f.GetField("N_VERTEXS") == 8
    assert f.GetField("PERIMETRE") == pytest.approx(1123.514024, abs=1e-5)
    assert f.GetField("AREA") == pytest.approx(88563.792204, abs=1e-5)
    assert f.GetField("N_ARCS") == 1
    assert f.GetField("N_POLIG") == 1
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 2
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((636.605137963894 390.371075166458,580.551855375883 575.547098001853,723.687916270269 594.565176022785,796.757373929641 475.451950523261,744.707897240773 396.376784015173,636.605137963894 390.371075166458))"
    )
    assert f.GetField("ID_GRAFIC") == 3
    assert f.GetField("N_VERTEXS") == 6
    assert f.GetField("PERIMETRE") == pytest.approx(680.544697, abs=1e-5)
    assert f.GetField("AREA") == pytest.approx(30550.052343, abs=1e-5)
    assert f.GetField("N_ARCS") == 1
    assert f.GetField("N_POLIG") == 1
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"


def test_ogr_miramon_read_simple_polygon():

    ds = gdal.OpenEx(
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol", gdal.OF_VECTOR
    )
    assert ds is not None, "Failed to get dataset"
    check_simple_polygon(ds)


# testing a polygon where the reference to arc has no extension
# the result has to be the same than if it has extension
def test_ogr_miramon_read_simple_polygon_no_ext():

    ds = gdal.OpenEx(
        "data/miramon/Polygons/SimplePolygonsCycleNoExt/SimplePolFile.pol",
        gdal.OF_VECTOR,
    )
    assert ds is not None, "Failed to get dataset"
    check_simple_polygon(ds)


def test_ogr_miramon_write_simple_polygon_EmptyVersion(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_polygon(ds)


def test_ogr_miramon_write_simple_polygon__V11(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol",
        format="MiraMonVector",
        options="-lco Version=V1.1",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_polygon(ds)


def test_ogr_miramon_write_simple_polygon_V20(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol",
        format="MiraMonVector",
        options="-lco Version=V2.0",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_polygon(ds)


def test_ogr_miramon_write_simple_polygon_last_version(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol",
        format="MiraMonVector",
        options="-lco Version=last_version",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_polygon(ds)


###############################################################################
# basic multipolygon test


def check_multi_polygon(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 1
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon

    # going to the first polygon
    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((32.699999937575 36.072500062925,31.959999937575 36.532500062925,30.899999937575 36.902500062925,30.509999937575 36.492500062925,29.859999937575 36.192500062925,28.789999937575 36.502500062925,27.619999937575 38.012500062925,27.399999937575 39.872500062925,31.899999937575 41.312500062925,36.079999937575 41.662500062925,37.489999937575 41.182500062925,40.329999937575 40.932500062925,41.589999937575 41.562500062925,43.929999937575 39.382500062925,44.099999937575 36.542500062925,39.489999937575 34.192500062925,35.729999937575 34.312500062925,36.129999937575 34.942500062925,35.959999937575 35.942500062925,36.339999937575 36.862500062925,35.639999937575 36.942500062925,34.719999937575 36.622500062925,34.109999937575 36.702500062925,33.549999937575 36.172500062925,32.839999937575 36.062500062925,32.699999937575 36.072500062925),(42.449999937575 38.462500062925,43.079999937575 38.402500062925,43.389999937575 38.382500062925,43.289999937575 38.722500062925,43.699999937575 38.962500062925,43.449999937575 39.102500062925,43.009999937575 38.892500062925,42.339999937575 38.772500062925,42.449999937575 38.462500062925),(37.929999937575 36.832500062925,38.139999937575 36.422500062925,37.889999937575 35.962500062925,38.469999937575 35.702500062925,38.829999937575 35.982500062925,38.229999937575 36.122500062925,38.439999937575 36.662500062925,38.019999937575 36.932500062925,37.929999937575 36.832500062925)),((34.269999937575 35.602500062925,34.779999937575 35.762500062925,34.669999937575 35.582500062925,33.919999937575 35.172500062925,33.889999937575 34.812500062925,32.819999937575 34.612500062925,32.299999937575 34.892500062925,32.409999937575 35.182500062925,32.909999937575 35.242500062925,32.939999937575 35.412500062925,33.599999937575 35.282500062925,34.269999937575 35.602500062925)))"
    )
    assert f.GetFieldAsString("ID_GRAFIC") == "(2:1,1)"
    assert f.GetFieldAsString("N_VERTEXS") == "(2:56,56)"
    assert f.GetFieldAsString("N_ARCS") == "(2:4,4)"
    assert f.GetFieldAsString("N_POLIG") == "(2:4,4)"
    assert f.GetFieldAsString("TEXT") == "(2:Multip 1,Multip 2)"
    assert f.GetFieldAsString("NUMBER") == "(2:1,2)"
    assert f.GetFieldAsString("DATA") == "2024/04/18"


def test_ogr_miramon_read_multi_polygon():

    ds = gdal.OpenEx(
        "data/miramon/Polygons/Multipolygons/Multipolygons.pol", gdal.OF_VECTOR
    )
    assert ds is not None, "Failed to get dataset"
    check_multi_polygon(ds)


def test_ogr_miramon_write_multi_polygon_EmptyVersion(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/Multipolygons/Multipolygons.pol",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_multi_polygon(ds)


###############################################################################
# testing empty layers


def test_ogr_miramon_empty_point_layers():

    ds = gdal.OpenEx("data/miramon/Points/EmptyPoints/Empty_PNT.pnt", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 0

    f = lyr.GetNextFeature()
    assert f is None, "Failed to get empty feature"

    ds = None


def test_ogr_miramon_empty_arc_layers():

    ds = gdal.OpenEx("data/miramon/Arcs/EmptyArcs/Empty_ARC.arc", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 0

    f = lyr.GetNextFeature()
    assert f is None, "Failed to get empty feature"

    ds = None


def test_ogr_miramon_empty_pol_layers():

    ds = gdal.OpenEx(
        "data/miramon/Polygons/EmptyPolygons/Empty_POL.pol", gdal.OF_VECTOR
    )
    assert ds is not None, "Failed to get dataset"

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    # The layer has no features
    assert lyr.GetFeatureCount() == 0

    f = lyr.GetNextFeature()
    assert f is None, "Failed to get empty feature"

    ds = None


###############################################################################
# testing 3d part


def check_3d_point(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 32
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0

    assert (
        f.GetGeometryRef().ExportToWkt() == "POINT (440551.66 4635315.3 619.9599609375)"
    )

    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    assert g.GetZ() == 619.9599609375

    f = lyr.GetFeature(30)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetZ() == 619.77


def test_ogr_miramon_read_3d_point(tmp_vsimem):

    ds = gdal.OpenEx("data/miramon/Points/3dpoints/Some3dPoints.pnt", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"
    check_3d_point(ds)


@pytest.mark.parametrize(
    "Height,expected_height",
    [
        ("First", 250.0),
        ("Lowest", 250.0),
        ("Highest", 277.0),
    ],
)
def test_ogr_miramon_read_multi_3d_point(Height, expected_height):

    ds = gdal.OpenEx(
        "data/miramon/Points/3dpoints/Some3dPoints.pnt",
        gdal.OF_VECTOR,
        open_options=["Height=" + Height],
    )

    assert ds is not None, "Failed to get dataset"
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 32
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    f = lyr.GetFeature(31)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetZ() == expected_height


def test_ogr_miramon_write_3d_point(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/3dpoints/Some3dPoints.pnt",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_3d_point(ds)


def check_3d_arc(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 6
    assert lyr.GetGeomType() == ogr.wkbLineString25D

    f = lyr.GetFeature(0)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    assert g.GetPointCount() == 4
    p = g.GetPoint(0)
    assert p[2] == 595.1063842773438
    p = g.GetPoint(1)
    assert p[2] == 326.656005859375
    p = g.GetPoint(2)
    assert p[2] == 389.99432373046875
    p = g.GetPoint(3)
    assert p[2] == 716.6224975585938

    f = lyr.GetFeature(5)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    assert g.GetPointCount() == 2
    p = g.GetPoint(0)
    assert p[2] == 233.82064819335938
    p = g.GetPoint(1)
    assert p[2] == 794.5372314453125

    ds = None


def test_ogr_miramon_read_3d_arc(tmp_vsimem):

    ds = gdal.OpenEx("data/miramon/Arcs/3dArcs/linies_3d_WGS84.arc", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"
    check_3d_arc(ds)


def test_ogr_miramon_write_3d_arc(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/3dArcs/linies_3d_WGS84.arc",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_3d_arc(ds)
    del ds


def check_3d_pol(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 5
    assert lyr.GetGeomType() == ogr.wkbPolygon25D

    f = lyr.GetFeature(0)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    r = g.GetGeometryRef(0)
    assert r is not None, "Failed to get geometry"
    assert r.GetPointCount() == 4
    p = r.GetPoint(0)
    assert p[2] == 11.223576545715332
    p = r.GetPoint(1)
    assert p[2] == 9.221868515014648
    p = r.GetPoint(2)
    assert p[2] == 21.929399490356445
    p = r.GetPoint(3)
    assert p[2] == 11.223576545715332

    f = lyr.GetFeature(4)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    r = g.GetGeometryRef(0)
    assert r is not None, "Failed to get geometry"
    assert r.GetPointCount() == 4
    p = r.GetPoint(0)
    assert p[2] == 18.207277297973633
    p = r.GetPoint(1)
    assert p[2] == 21.929399490356445
    p = r.GetPoint(2)
    assert p[2] == 5.746463775634766
    p = r.GetPoint(3)
    assert p[2] == 18.207277297973633


def test_ogr_miramon_read_3d_pol():

    ds = gdal.OpenEx("data/miramon/Polygons/3dPolygons/tin_3d.pol", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"
    check_3d_pol(ds)


def test_ogr_miramon_write_3d_pol(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/3dPolygons/tin_3d.pol",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_3d_pol(ds)
    del ds


###############################################################################
# ogrsf test in some files


@pytest.mark.parametrize(
    "filename",
    [
        "Points/3dpoints/Some3dPoints.pnt",
        "Points/SimplePoints/SimplePointsFile.pnt",
        "Points/EmptyPoints/Empty_PNT.pnt",
        "Arcs/SimpleArcs/SimpleArcFile.arc",
        "Arcs/EmptyArcs/Empty_ARC.arc",
        "Arcs/3dArcs/linies_3d_WGS84.arc",
        "Polygons/SimplePolygons/SimplePolFile.pol",
        "Polygons/EmptyPolygons/Empty_POL.pol",
        "Polygons/3dPolygons/tin_3d.pol",
        "Polygons/Multipolygons/Multipolygons.pol",
    ],
)
def test_ogr_miramon_test_ogrsf(filename):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip("test_ogrsf not available")

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/miramon/" + filename
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# -lco tests: CreationLanguage


@pytest.mark.parametrize(
    "Language, expected_description",
    [
        ("CAT", "Identificador Gràfic intern"),
        ("SPA", "Identificador Gráfico interno"),
        ("ENG", "Internal Graphic identifier"),
    ],
)
def test_ogr_miramon_CreationLanguage(tmp_vsimem, Language, expected_description):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        format="MiraMonVector",
        options="-lco CreationLanguage=" + Language,
    )

    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    layer_def = lyr.GetLayerDefn()
    field_index = layer_def.GetFieldIndex("ID_GRAFIC")
    assert field_index >= 0

    field_def = layer_def.GetFieldDefn(field_index)
    field_description = field_def.GetAlternativeNameRef()
    assert field_description == expected_description


###############################################################################
# -lco tests: CreationLanguage


@pytest.mark.parametrize(
    "Language,expected_description",
    [
        ("CAT", "Identificador Gràfic intern"),
        ("SPA", "Identificador Gráfico interno"),
        ("ENG", "Internal Graphic identifier"),
    ],
)
def test_ogr_miramon_OpenLanguagePoint(Language, expected_description):

    ds = gdal.OpenEx(
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        gdal.OF_VECTOR,
        open_options=["OpenLanguage=" + Language],
    )
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    layer_def = lyr.GetLayerDefn()
    field_index = layer_def.GetFieldIndex("ID_GRAFIC")
    assert field_index >= 0

    field_def = layer_def.GetFieldDefn(field_index)
    field_description = field_def.GetAlternativeNameRef()
    assert field_description == expected_description


@pytest.mark.parametrize(
    "Language,expected_description",
    [
        ("CAT", "Node inicial"),
        ("SPA", "Nodo inicial"),
        ("ENG", "Initial node"),
    ],
)
def test_ogr_miramon_OpenLanguageArc(Language, expected_description):

    ds = gdal.OpenEx(
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        gdal.OF_VECTOR,
        open_options=["OpenLanguage=" + Language],
    )
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    layer_def = lyr.GetLayerDefn()
    field_index = layer_def.GetFieldIndex("NODE_INI")
    assert field_index >= 0

    field_def = layer_def.GetFieldDefn(field_index)
    field_description = field_def.GetAlternativeNameRef()
    assert field_description == expected_description


###############################################################################
# unexisting file, file shorter than expected, wrong version, no sidecar files


@pytest.mark.parametrize(
    "name,message",
    [
        (
            "data/miramon/CorruptedFiles/ShortFile/ShortFile.pnt",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/CorruptedFiles/WrongVersion/WrongVersion.pnt",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/CorruptedFiles/WrongDBF/WrongDBF.pnt",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/CorruptedFiles/NoDBF/NoDBF.pnt",
            "Error reading the format in the DBF file",
        ),
        ("data/miramon/CorruptedFiles/NoREL/NoREL.pnt", "rel must exist."),
        ("data/miramon/CorruptedFiles/NoNode/SimpleArcFile.arc", "Cannot open file"),
        ("data/miramon/CorruptedFiles/NoArcRel/SimpleArcFile.arc", "rel must exist"),
        ("data/miramon/CorruptedFiles/NoPolRel/SimplePolFile.pol", "rel must exist"),
        ("data/miramon/CorruptedFiles/BadCycle/SimplePolFile.pol", "Cannot open file"),
        (
            "data/miramon/CorruptedFiles/InexistentCycle1/SimplePolFile.pol",
            "Cannot open file",
        ),
        (
            "data/miramon/CorruptedFiles/InexistentCycle2/SimplePolFile.pol",
            "Error reading the ARC file in the metadata file",
        ),
    ],
)
def test_ogr_miramon_corrupted_files(name, message):
    with pytest.raises(Exception, match=message):
        gdal.OpenEx(
            name,
            gdal.OF_VECTOR,
        )


###############################################################################
# features test: unexisting coordinates, unexpected polygon construction


@pytest.mark.parametrize(
    "name,message",
    [
        (
            "data/miramon/CorruptedFiles/CorruptedCoordinates/CorruptedCoordinatesPoint.pnt",
            "Wrong file format",
        ),
        (
            "data/miramon/CorruptedFiles/CorruptedCoordinates/CorruptedCoordinates.arc",
            "Wrong file format",
        ),
        (
            "data/miramon/CorruptedFiles/CorruptedCoordinates/CorruptedCoordinates.pol",
            "Wrong file format",
        ),
        (
            "data/miramon/CorruptedFiles/CorruptedPolygon/Multipolygons.pol",
            "Wrong polygon format",
        ),
    ],
)
def test_ogr_miramon_corrupted_features(name, message):

    ds = gdal.OpenEx(
        name,
        gdal.OF_VECTOR,
    )
    assert ds is not None, "Failed to get dataset"
    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"
    with pytest.raises(Exception, match=message):
        for f in lyr:
            pass


###############################################################################
# multiregister test


@pytest.mark.parametrize(
    "expected_MultiRecordIndex,textField,expectedResult",
    [
        ("0", "TEXT", "Multip 1"),
        ("1", "TEXT", "Multip 2"),
        ("Last", "TEXT", "Multip 2"),
        ("JSON", "TEXT", "[Multip 1,Multip 2]"),
        ("0", "NUMBER", "1"),
        ("1", "NUMBER", "2"),
        ("Last", "NUMBER", "2"),
        ("JSON", "NUMBER", "[1,2]"),
        ("0", "DOUBLE", "22.558"),
        ("1", "DOUBLE", "22.000"),
        ("Last", "DOUBLE", "22.000"),
        ("JSON", "DOUBLE", "[22.558,22.000]"),
    ],
)
def test_multiregister(expected_MultiRecordIndex, textField, expectedResult):
    ds = gdal.OpenEx(
        "data/miramon/Polygons/Multipolygons/Multipolygons.pol",
        gdal.OF_VECTOR,
        open_options=["MultiRecordIndex=" + expected_MultiRecordIndex],
    )
    assert ds is not None, "Failed to get dataset"

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 1
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon

    # going to the first polygon
    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert f.GetFieldAsString(textField) == expectedResult


###############################################################################
# basic writing test


def create_common_attributes(lyr):
    lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("doublefield", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("strlistfield", ogr.OFTStringList))
    lyr.CreateField(ogr.FieldDefn("intlistfield", ogr.OFTIntegerList))
    lyr.CreateField(ogr.FieldDefn("int64listfield", ogr.OFTInteger64List))
    lyr.CreateField(ogr.FieldDefn("doulistfield", ogr.OFTRealList))
    lyr.CreateField(ogr.FieldDefn("datefield", ogr.OFTDate))
    f = ogr.FieldDefn("boolfield", ogr.OFTInteger)
    f.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(f)


def assign_common_attributes(f):
    f["strfield"] = "foo"
    f["intfield"] = 123456789
    f["int64field"] = 12345678912345678
    f["doublefield"] = 1.5
    f["strlistfield"] = ["foo", "bar"]
    f["intlistfield"] = [123456789]
    f["int64listfield"] = [12345678912345678]
    f["doulistfield"] = [1.5, 4.2]
    f["datefield"] = "2024/04/24"
    f["boolfield"] = 1


def check_common_attributes(f):
    assert f["strfield"] == ["foo", ""]
    assert f["intfield"] == [123456789]
    assert f["int64field"] == [12345678912345678]
    assert f["doublefield"] == [1.5]
    assert f["strlistfield"] == ["foo", "bar"]
    assert f["intlistfield"] == [123456789]
    assert f["int64listfield"] == [12345678912345678]
    assert f["doulistfield"] == [1.5, 4.2]
    assert f["datefield"] == "2024/04/24"
    assert f["boolfield"] == [True]


def open_ds_lyr_0_feature_0(layername):
    ds = ogr.Open(layername)
    assert ds is not None, "Failed to get dataset"
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"
    f = lyr.GetNextFeature()
    return ds, lyr, f


def test_ogr_miramon_write_basic_polygon(tmp_path):

    filename = str(tmp_path / "DataSetPOL")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    layername = filename + "/test.pol"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [1, 1]
    assert f["N_VERTEXS"] == [4, 4]
    assert f["PERIMETRE"] == [3.414, 3.414]
    assert f["AREA"] == [0.500000000000, 0.500000000000]
    assert f["N_ARCS"] == [1, 1]
    assert f["N_POLIG"] == [1, 1]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POLYGON ((0 0,0 1,1 1,0 0))"
    ds = None


def test_ogr_miramon_write_basic_multipolygon(tmp_path):

    filename = str(tmp_path / "DataSetMULTIPOL")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON (((0 0,0 5,5 5,5 0,0 0), (1 1,2 1,2 2,1 2,1 1), (3 3,4 3,4 4,3 4,3 3)),((5 6,5 7,6 7,6 6,5 6)))"
        )
    )

    lyr.CreateFeature(f)
    f = None
    ds = None

    layername = filename + "/test.pol"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [1, 1]
    assert f["N_VERTEXS"] == [20, 20]
    assert f["PERIMETRE"] == [32, 32]
    assert f["AREA"] == [24, 24]
    assert f["N_ARCS"] == [4, 4]
    assert f["N_POLIG"] == [4, 4]
    check_common_attributes(f)
    assert (
        f.GetGeometryRef().ExportToIsoWkt()
        == "MULTIPOLYGON (((0 0,0 5,5 5,5 0,0 0),(1 1,2 1,2 2,1 2,1 1),(3 3,4 3,4 4,3 4,3 3)),((5 6,5 7,6 7,6 6,5 6)))"
    )
    ds = None


def test_ogr_miramon_write_basic_multipolygon_3d(tmp_path):

    filename = str(tmp_path / "DataSetMULTIPOL3d")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON Z (((0 0 3,0 5 3,5 5 4,5 0 5,0 0 3), (1 1 6,2 1 3,2 2 9.2,1 2 3.14,1 1 6), (3 3 1,4 3 12,4 4 21,3 4 2,3 3 1)),((5 6 2,5 7 2,6 7 3,6 6 3,5 6 2)))"
        )
    )

    lyr.CreateFeature(f)
    f = None
    ds = None

    layername = filename + "/test.pol"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [1, 1]
    assert f["N_VERTEXS"] == [20, 20]
    assert f["PERIMETRE"] == [32, 32]
    assert f["AREA"] == [24, 24]
    assert f["N_ARCS"] == [4, 4]
    assert f["N_POLIG"] == [4, 4]
    check_common_attributes(f)
    assert (
        f.GetGeometryRef().ExportToIsoWkt()
        == "MULTIPOLYGON Z (((0 0 3,0 5 3,5 5 4,5 0 5,0 0 3),(1 1 6,2 1 3,2 2 9.2,1 2 3.14,1 1 6),(3 3 1,4 3 12,4 4 21,3 4 2,3 3 1)),((5 6 2,5 7 2,6 7 3,6 6 3,5 6 2)))"
    )
    ds = None


def test_ogr_miramon_write_basic_linestring(tmp_path):

    filename = str(tmp_path / "DataSetLINESTRING")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0,0 1,1 1)"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    layername = filename + "/test.arc"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    assert f["N_VERTEXS"] == [3, 3]
    assert f["LONG_ARC"] == [2.0, 2.0]
    assert f["NODE_INI"] == [0, 0]
    assert f["NODE_FI"] == [1, 1]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "LINESTRING (0 0,0 1,1 1)"
    ds = None


# There are to ways of writing/reading repeated Z's in a linestring file.
# So let's test both ways (different Z's in each vertice or the same Z for all of them)
@pytest.mark.parametrize(
    "LinestringZ",
    [
        "LINESTRING Z (0 0 4,0 1 3,1 1 2)",
        "LINESTRING Z (0 0 4,0 1 4,1 1 4)",
    ],
)
def test_ogr_miramon_write_basic_linestringZ(tmp_path, LinestringZ):

    filename = str(tmp_path / "DataSetLINESTRING")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt(LinestringZ))
    lyr.CreateFeature(f)
    f = None
    ds = None

    layername = filename + "/test.arc"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    assert f["N_VERTEXS"] == [3, 3]
    assert f["LONG_ARC"] == [2.0, 2.0]
    assert f["NODE_INI"] == [0, 0]
    assert f["NODE_FI"] == [1, 1]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == LinestringZ
    ds = None


def test_ogr_miramon_write_basic_multilinestring(tmp_path):

    filename = str(tmp_path / "DataSetMULTILINESTRING")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(
        ogr.CreateGeometryFromWkt("MULTILINESTRING ((0 0,0 1,1 1),(0 0,0 3))")
    )
    lyr.CreateFeature(f)
    f = None
    ds = None

    layername = filename + "/test.arc"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    assert f["N_VERTEXS"] == [3, 3]
    assert f["LONG_ARC"] == [2.0, 2.0]
    assert f["NODE_INI"] == [0, 0]
    assert f["NODE_FI"] == [1, 1]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "LINESTRING (0 0,0 1,1 1)"

    f = lyr.GetNextFeature()

    assert f["ID_GRAFIC"] == [1, 1]
    assert f["N_VERTEXS"] == [2, 2]
    assert f["LONG_ARC"] == [3.0, 3.0]
    assert f["NODE_INI"] == [2, 2]
    assert f["NODE_FI"] == [3, 3]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "LINESTRING (0 0,0 3)"

    ds = None


@pytest.mark.parametrize(
    "DBFEncoding",
    [
        "UTF8",
        "ANSI",
    ],
)
def test_ogr_miramon_write_basic_point(tmp_path, DBFEncoding):

    filename = str(tmp_path / "DataSetPOINT")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    options = ["DBFEncoding=" + DBFEncoding]
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown, options=options)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 0)"))
    lyr.CreateFeature(f)

    f = None
    ds = None

    layername = filename + "/test.pnt"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (0 0)"

    f = lyr.GetNextFeature()

    assert f["ID_GRAFIC"] == [1, 1]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 0)"

    ds = None


def test_ogr_miramon_write_basic_pointZ(tmp_path):

    filename = str(tmp_path / "DataSetPOINT")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT Z (0 0 6)"))
    lyr.CreateFeature(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT Z (1 0 5)"))
    lyr.CreateFeature(f)

    f = None
    ds = None

    layername = filename + "/test.pnt"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT Z (0 0 6)"

    f = lyr.GetNextFeature()

    assert f["ID_GRAFIC"] == [1, 1]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT Z (1 0 5)"

    ds = None


def test_ogr_miramon_write_basic_multipoint(tmp_path):

    filename = str(tmp_path / "DataSetMULTIPOINT")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT (0 0, 1 0)"))
    lyr.CreateFeature(f)

    f = None
    ds = None

    layername = filename + "/test.pnt"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (0 0)"

    f = lyr.GetNextFeature()

    assert f["ID_GRAFIC"] == [1, 1]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 0)"

    ds = None


def test_ogr_miramon_write_basic_multigeometry(tmp_path):

    filename = str(tmp_path / "DataSetMULTIGEOM")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    lyr.CreateFeature(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (0 0,0 1,1 1)"))
    lyr.CreateFeature(f)

    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON (((0 0,0 5,5 5,5 0,0 0), (1 1,2 1,2 2,1 2,1 1), (3 3,4 3,4 4,3 4,3 3)),((5 6,5 7,6 7,6 6,5 6)))"
        )
    )
    lyr.CreateFeature(f)

    f = None
    ds = None

    layername = filename + "/test.pnt"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (0 0)"

    ds = None

    layername = filename + "/test.arc"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [0, 0]
    check_common_attributes(f)
    assert f.GetGeometryRef().ExportToIsoWkt() == "LINESTRING (0 0,0 1,1 1)"

    ds = None

    layername = filename + "/test.pol"
    ds, lyr, f = open_ds_lyr_0_feature_0(layername)

    assert f["ID_GRAFIC"] == [1, 1]
    check_common_attributes(f)
    assert (
        f.GetGeometryRef().ExportToIsoWkt()
        == "MULTIPOLYGON (((0 0,0 5,5 5,5 0,0 0),(1 1,2 1,2 2,1 2,1 1),(3 3,4 3,4 4,3 4,3 3)),((5 6,5 7,6 7,6 6,5 6)))"
    )

    ds = None


def test_ogr_miramon_create_field_after_feature(tmp_path):

    filename = str(tmp_path / "DataSetMULTIPOINT")
    ds = ogr.GetDriverByName("MiramonVector").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
    create_common_attributes(lyr)
    f = ogr.Feature(lyr.GetLayerDefn())
    assign_common_attributes(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT (0 0, 1 0)"))
    lyr.CreateFeature(f)

    # MiraMon doesn't allow that
    with pytest.raises(
        Exception,
        match="Cannot create fields to a layer with already existing features in it",
    ):
        create_common_attributes(lyr)
