#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic OGR translation of WKT and WKB geometries.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os

import gdaltest
import pytest

from osgeo import gdal, ogr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        f
        for f in os.listdir(os.path.join(os.path.dirname(__file__), "data/wkb_wkt"))
        if f[-4:] == ".wkb"
    ],
)
def test_wkbwkt_geom(filename):
    raw_wkb = open("data/wkb_wkt/" + filename, "rb").read()
    raw_wkt = open("data/wkb_wkt/" + os.path.splitext(filename)[0] + ".wkt").read()

    ######################################################################
    # Compare the WKT derived from the WKB file to the WKT provided
    # but reformatted (normalized).

    geom_wkb = ogr.CreateGeometryFromWkb(raw_wkb)
    wkb_wkt = geom_wkb.ExportToWkt()

    geom_wkt = ogr.CreateGeometryFromWkt(raw_wkt)
    normal_wkt = geom_wkt.ExportToWkt()

    # print(wkb_wkt)
    # print(normal_wkt)
    # print(raw_wkt)
    assert wkb_wkt == normal_wkt, "WKT from WKB (%s) does not match clean WKT (%s)." % (
        wkb_wkt,
        normal_wkt,
    )

    ######################################################################
    # Verify that the geometries appear to be the same.   This is
    # intended to catch problems with the encoding too WKT that might
    # cause passes above but that are mistaken.
    assert (
        geom_wkb.GetCoordinateDimension() == geom_wkt.GetCoordinateDimension()
    ), "Coordinate dimension differs!"

    assert (
        geom_wkb.GetGeometryType() == geom_wkt.GetGeometryType()
    ), "Geometry type differs!"

    assert (
        geom_wkb.GetGeometryName() == geom_wkt.GetGeometryName()
    ), "Geometry name differs!"

    # It turns out this test is too picky about coordinate precision. skip.
    #       if geom_wkb.Equal( geom_wkt ) == 0:
    #           gdaltest.post_reason( 'Geometries not equal!' )
    #           print geom_wkb.ExportToWkt()
    #           print geom_wkt.ExportToWkt()
    #           return 'fail'

    ######################################################################
    # Convert geometry to WKB and back to verify that WKB encoding is
    # working smoothly.

    wkb_xdr = geom_wkt.ExportToWkb(ogr.wkbXDR)
    geom_wkb = ogr.CreateGeometryFromWkb(wkb_xdr)

    assert str(geom_wkb) == str(geom_wkt), "XDR WKB encoding/decoding failure."

    wkb_ndr = geom_wkt.ExportToWkb(ogr.wkbNDR)
    geom_wkb = ogr.CreateGeometryFromWkb(wkb_ndr)

    assert str(geom_wkb) == str(geom_wkt), "NDR WKB encoding/decoding failure."


###############################################################################
# Test geometry with very large exponents of coordinate values.


def test_ogr_wkbwkt_geom_bigexponents():

    bigx = -1.79769313486e308
    bigy = -1.12345678901e308

    geom = ogr.Geometry(ogr.wkbPoint)
    geom.SetPoint(0, bigx, bigy)

    expect = "POINT (-1.79769313486E+308 -1.12345678901E+308 0)"
    wkt = geom.ExportToWkt()

    assert str(wkt) == str(expect), "trimming long float numbers failed."


###############################################################################
# Test importing broken/unhandled WKT.


def test_ogr_wkbwkt_test_broken_geom():

    list_broken = [
        "POINT",
        "POINT UNKNOWN",
        "POINT(",
        "POINT()",
        "POINT(,)",
        "POINT(EMPTY",
        "POINT(A)",
        "POINT(0)",
        "POINT(A 0)",
        "POINT(0 A)",
        "POINT(0 1",
        "POINT(0 1,",
        "POINT((0 1))",
        "POINT Z",
        "POINT Z UNKNOWN",
        "POINT Z(",
        "POINT Z()",
        "POINT Z(EMPTY)",
        "POINT Z(A)",
        "POINT Z(0 1",
        "POINTZ M EMPTY",
        "LINESTRING",
        "LINESTRING UNKNOWN",
        "LINESTRING(",
        "LINESTRING()",
        "LINESTRING(,)",
        "LINESTRING(())",
        "LINESTRING(EMPTY",
        "LINESTRING(A)",
        "LINESTRING(0 1,",
        "LINESTRING(0 1,2 3",
        "LINESTRING(0 1,,2 3)",
        "LINESTRING((0 1,2 3))",
        "LINESTRING Z",
        "LINESTRING Z UNKNOWN",
        "LINESTRING Z(",
        "LINESTRING Z()",
        "LINESTRING Z(EMPTY)",
        "LINESTRING Z(A)",
        "LINESTRING Z(0 1",
        "LINESTRING Z(0 1,2 3",
        "POLYGON",
        "POLYGON UNKNOWN",
        "POLYGON(",
        "POLYGON()",
        "POLYGON(,)",
        "POLYGON(())",
        "POLYGON(EMPTY",
        "POLYGON(A)",
        "POLYGON(0 1)",
        "POLYGON(0 1,2 3",
        "POLYGON((0 1,2 3",
        "POLYGON((0 1,2 3,",
        "POLYGON((0 1,2 3)",
        "POLYGON((0 1,2 3),",
        "POLYGON((0 1,2 3),EMPTY",
        "POLYGON(((0 1,2 3)))",
        "POLYGON Z",
        "POLYGON Z UNKNOWN",
        "POLYGON Z(",
        "POLYGON Z()",
        "POLYGON Z(EMPTY",
        "POLYGON Z(A)",
        "POLYGON Z(0 1",
        "POLYGON Z(0 1,2 3",
        "POLYGON Z((0 1,2 3",
        "POLYGON Z((0 1,2 3)",
        "POLYGON Z(((0 1,2 3)))",
        "MULTIPOINT",
        "MULTIPOINT UNKNOWN",
        "MULTIPOINT(",
        "MULTIPOINT()",
        "MULTIPOINT(())",
        "MULTIPOINT(EMPTY",
        "MULTIPOINT(EMPTY,",
        "MULTIPOINT(EMPTY,(0 1)",
        "MULTIPOINT(A)",
        "MULTIPOINT(0 1",
        "MULTIPOINT(0 1,",
        "MULTIPOINT(0 1,2 3",
        "MULTIPOINT((0 1),,(2 3))",
        "MULTIPOINT(0 1,EMPTY",
        "MULTIPOINT((0 1),EMPTY",
        "MULTIPOINT((0 1)",
        # 'MULTIPOINT(0 1,2 3)', # This one is not SF compliant but supported for legacy
        "MULTIPOINT((0 1),(2 3)",
        "MULTIPOINT Z",
        "MULTIPOINT Z UNKNOWN",
        "MULTIPOINT Z(",
        "MULTIPOINT Z()",
        "MULTIPOINT Z(EMPTY",
        "MULTIPOINT Z(A)",
        "MULTIPOINT Z(0 1",
        "MULTIPOINT Z((0 1)",
        # 'MULTIPOINT Z(0 1,2 3)',  # Not SF compliant (and rejected by PostGIS), but we accept it
        "MULTILINESTRING",
        "MULTILINESTRING UNKNOWN",
        "MULTILINESTRING(",
        "MULTILINESTRING()",
        "MULTILINESTRING(,)",
        "MULTILINESTRING(())",
        "MULTILINESTRING(EMPTY",
        "MULTILINESTRING(EMPTY,",
        "MULTILINESTRING(A)",
        "MULTILINESTRING(0 1",
        "MULTILINESTRING(0 1,",
        "MULTILINESTRING(0 1,2 3)",
        "MULTILINESTRING((0 1,2 3",
        "MULTILINESTRING((0 1,2 3),)",
        "MULTILINESTRING((0 1)",
        "MULTILINESTRING((0 1),",
        "MULTILINESTRING((0 1),EMPTY",
        "MULTILINESTRING((0 1),(2 3)",
        "MULTILINESTRING Z",
        "MULTILINESTRING Z UNKNOWN",
        "MULTILINESTRING Z(",
        "MULTILINESTRING Z()",
        "MULTILINESTRING Z(EMPTY",
        "MULTILINESTRING Z(A)",
        "MULTILINESTRING Z(0 1",
        "MULTILINESTRING Z((0 1)",
        "MULTILINESTRING Z((0 1),(2 3)",
        "MULTIPOLYGON",
        "MULTIPOLYGON UNKNOWN",
        "MULTIPOLYGON(",
        "MULTIPOLYGON()",
        "MULTIPOLYGON(,)",
        "MULTIPOLYGON(())",
        "MULTIPOLYGON((()))",
        "MULTIPOLYGON(EMPTY",
        "MULTIPOLYGON(EMPTY,",
        "MULTIPOLYGON(A)",
        "MULTIPOLYGON(0 1",
        "MULTIPOLYGON(0 1,",
        "MULTIPOLYGON(0 1,2 3)",
        "MULTIPOLYGON((0 1,2 3",
        "MULTIPOLYGON((0 1,2 3),)",
        "MULTIPOLYGON((0 1)",
        "MULTIPOLYGON((0 1),",
        "MULTIPOLYGON((0 1),EMPTY",
        "MULTIPOLYGON((0 1),(2 3)",
        "MULTIPOLYGON((0 1),(2 3))",
        "MULTIPOLYGON(((0 1))",
        "MULTIPOLYGON(((0 1)),",
        "MULTIPOLYGON(((0 1)),,",
        "MULTIPOLYGON(((0 1),(2 3))",
        "MULTIPOLYGON(((0 1),EMPTY",
        "MULTIPOLYGON(((0 1),EMPTY,",
        "MULTIPOLYGON((((0 1)),)",
        "MULTIPOLYGON Z",
        "MULTIPOLYGON Z UNKNOWN",
        "MULTIPOLYGON Z(",
        "MULTIPOLYGON Z()",
        "MULTIPOLYGON Z(EMPTY",
        "MULTIPOLYGON Z(A)",
        "MULTIPOLYGON Z(0 1",
        "MULTIPOLYGON Z((0 1)",
        "MULTIPOLYGON Z((0 1),(2 3)",
        "GEOMETRYCOLLECTION",
        "GEOMETRYCOLLECTION UNKNOWN",
        "GEOMETRYCOLLECTION(",
        "GEOMETRYCOLLECTION()",
        "GEOMETRYCOLLECTION(,)",
        "GEOMETRYCOLLECTION(())",
        "GEOMETRYCOLLECTION(EMPTY",
        "GEOMETRYCOLLECTION(EMPTY,",
        "GEOMETRYCOLLECTION(A)",
        "GEOMETRYCOLLECTION(POINT(0 1)",
        "GEOMETRYCOLLECTION(POINT(0 1),",
        "GEOMETRYCOLLECTION(POINT(0 1),)",
        "GEOMETRYCOLLECTION(POINT(0 1),UNKNOWN)",
        "GEOMETRYCOLLECTION Z",
        "GEOMETRYCOLLECTION Z(",
        "GEOMETRYCOLLECTION Z()",
        "GEOMETRYCOLLECTION Z(EMPTY",
        "GEOMETRYCOLLECTION Z(POINT(0 1)",
        "COMPOUNDCURVE",
        "COMPOUNDCURVE UNKNOWN",
        "COMPOUNDCURVE(",
        "COMPOUNDCURVE()",
        "COMPOUNDCURVE(,)",
        "COMPOUNDCURVE(())",
        "COMPOUNDCURVE(EMPTY",
        "COMPOUNDCURVE(EMPTY,",
        "COMPOUNDCURVE(A)",
        "COMPOUNDCURVE((0 1,2 3",
        "COMPOUNDCURVE((0 1,2 3)",
        "COMPOUNDCURVE((0 1,2 3)",
        "COMPOUNDCURVE((0 1,2 3),",
        "COMPOUNDCURVE((0 1,2 3),)",
        "COMPOUNDCURVE((0 1,2 3),UNKNOWN)",
        "COMPOUNDCURVE Z",
        "COMPOUNDCURVE Z(",
        "COMPOUNDCURVE Z()",
        "COMPOUNDCURVE Z(EMPTY",
        "COMPOUNDCURVE Z((0 1,2 3)",
        "CURVEPOLYGON",
        "CURVEPOLYGON UNKNOWN",
        "CURVEPOLYGON(",
        "CURVEPOLYGON()",
        "CURVEPOLYGON(,)",
        "CURVEPOLYGON(())",
        "CURVEPOLYGON(EMPTY",
        "CURVEPOLYGON(EMPTY,",
        "CURVEPOLYGON(A)",
        "CURVEPOLYGON((0 1,2 3",
        "CURVEPOLYGON((0 1,2 3)",
        "CURVEPOLYGON((0 1,2 3)",
        "CURVEPOLYGON((0 1,2 3),",
        "CURVEPOLYGON((0 1,2 3),)",
        "CURVEPOLYGON((0 1,2 3),UNKNOWN)",
        "CURVEPOLYGON Z",
        "CURVEPOLYGON Z(",
        "CURVEPOLYGON Z()",
        "CURVEPOLYGON Z(EMPTY",
        "CURVEPOLYGON Z((0 1,2 3)",
    ]
    for wkt in list_broken:
        with gdal.quiet_errors():
            geom = ogr.CreateGeometryFromWkt(wkt)
        assert geom is None, "geom %s instantiated but not expected" % wkt


###############################################################################
# Test importing WKT SF1.2


@pytest.mark.parametrize(
    "input_wkt,expected_output_wkt",
    [
        ("POINT EMPTY", "POINT EMPTY"),
        ("POINT Z EMPTY", "POINT Z EMPTY"),
        ("POINT M EMPTY", "POINT M EMPTY"),
        ("POINT ZM EMPTY", "POINT ZM EMPTY"),
        ("POINT (0 1)", "POINT (0 1)"),
        ("POINT Z (0 1 2)", "POINT Z (0 1 2)"),
        ("POINT M (0 1 2)", "POINT M (0 1 2)"),
        ("POINT ZM (0 1 2 3)", "POINT ZM (0 1 2 3)"),
        ("LINESTRING EMPTY", "LINESTRING EMPTY"),
        ("LINESTRING Z EMPTY", "LINESTRING Z EMPTY"),
        ("LINESTRING M EMPTY", "LINESTRING M EMPTY"),
        ("LINESTRING ZM EMPTY", "LINESTRING ZM EMPTY"),
        ("LINESTRING (0 1,2 3)", "LINESTRING (0 1,2 3)"),
        ("LINESTRING Z (0 1 2,3 4 5)", "LINESTRING Z (0 1 2,3 4 5)"),
        ("LINESTRING M (0 1 2,3 4 5)", "LINESTRING M (0 1 2,3 4 5)"),
        ("LINESTRING ZM (0 1 2 3,4 5 6 7)", "LINESTRING ZM (0 1 2 3,4 5 6 7)"),
        ("POLYGON EMPTY", "POLYGON EMPTY"),
        ("POLYGON (EMPTY)", "POLYGON EMPTY"),
        ("POLYGON Z EMPTY", "POLYGON Z EMPTY"),
        ("POLYGON Z (EMPTY)", "POLYGON Z EMPTY"),
        ("POLYGON M EMPTY", "POLYGON M EMPTY"),
        ("POLYGON ZM EMPTY", "POLYGON ZM EMPTY"),
        ("POLYGON ((0 1,2 3,4 5,0 1))", "POLYGON ((0 1,2 3,4 5,0 1))"),
        ("POLYGON ((0 1,2 3,4 5,0 1),EMPTY)", "POLYGON ((0 1,2 3,4 5,0 1))"),
        ("POLYGON (EMPTY,(0 1,2 3,4 5,0 1))", "POLYGON EMPTY"),
        ("POLYGON (EMPTY,(0 1,2 3,4 5,0 1),EMPTY)", "POLYGON EMPTY"),
        (
            "POLYGON Z ((0 1 10,2 3 20,4 5 30,0 1 10),(0 1 10,2 3 20,4 5 30,0 1 10))",
            "POLYGON Z ((0 1 10,2 3 20,4 5 30,0 1 10),(0 1 10,2 3 20,4 5 30,0 1 10))",
        ),
        (
            "POLYGON M ((0 1 10,2 3 20,4 5 30,0 1 10))",
            "POLYGON M ((0 1 10,2 3 20,4 5 30,0 1 10))",
        ),
        (
            "POLYGON ZM ((0 1 10 100,2 3 20 200,4 5 30 300,0 1 10 10))",
            "POLYGON ZM ((0 1 10 100,2 3 20 200,4 5 30 300,0 1 10 10))",
        ),
        ("MULTIPOINT EMPTY", "MULTIPOINT EMPTY"),
        ("MULTIPOINT (EMPTY)", "MULTIPOINT EMPTY"),
        ("MULTIPOINT Z EMPTY", "MULTIPOINT Z EMPTY"),
        ("MULTIPOINT Z (EMPTY)", "MULTIPOINT Z EMPTY"),
        ("MULTIPOINT M EMPTY", "MULTIPOINT M EMPTY"),
        ("MULTIPOINT ZM EMPTY", "MULTIPOINT ZM EMPTY"),
        (
            "MULTIPOINT (0 1,2 3)",
            "MULTIPOINT ((0 1),(2 3))",
        ),  # Not SF1.2 compliant but recognized
        ("MULTIPOINT ((0 1),(2 3))", "MULTIPOINT ((0 1),(2 3))"),
        (
            "MULTIPOINT ((0 1),EMPTY)",
            "MULTIPOINT ((0 1))",
        ),  # We don't output empty points in multipoint
        (
            "MULTIPOINT (EMPTY,(0 1))",
            "MULTIPOINT ((0 1))",
        ),  # We don't output empty points in multipoint
        (
            "MULTIPOINT (EMPTY,(0 1),EMPTY)",
            "MULTIPOINT ((0 1))",
        ),  # We don't output empty points in multipoint
        ("MULTIPOINT Z ((0 1 2),(3 4 5))", "MULTIPOINT Z ((0 1 2),(3 4 5))"),
        ("MULTIPOINT M ((0 1 2),(3 4 5))", "MULTIPOINT M ((0 1 2),(3 4 5))"),
        ("MULTIPOINT ZM ((0 1 2 3),(4 5 6 7))", "MULTIPOINT ZM ((0 1 2 3),(4 5 6 7))"),
        ("MULTILINESTRING EMPTY", "MULTILINESTRING EMPTY"),
        ("MULTILINESTRING (EMPTY)", "MULTILINESTRING EMPTY"),
        ("MULTILINESTRING Z EMPTY", "MULTILINESTRING Z EMPTY"),
        ("MULTILINESTRING Z (EMPTY)", "MULTILINESTRING Z EMPTY"),
        ("MULTILINESTRING M EMPTY", "MULTILINESTRING M EMPTY"),
        ("MULTILINESTRING ZM EMPTY", "MULTILINESTRING ZM EMPTY"),
        ("MULTILINESTRING ((0 1,2 3,4 5,0 1))", "MULTILINESTRING ((0 1,2 3,4 5,0 1))"),
        (
            "MULTILINESTRING ((0 1,2 3,4 5,0 1),EMPTY)",
            "MULTILINESTRING ((0 1,2 3,4 5,0 1))",
        ),
        (
            "MULTILINESTRING (EMPTY,(0 1,2 3,4 5,0 1))",
            "MULTILINESTRING ((0 1,2 3,4 5,0 1))",
        ),
        (
            "MULTILINESTRING (EMPTY,(0 1,2 3,4 5,0 1),EMPTY)",
            "MULTILINESTRING ((0 1,2 3,4 5,0 1))",
        ),
        (
            "MULTILINESTRING Z ((0 1 10,2 3 20,4 5 30,0 1 10),(0 1 10,2 3 20,4 5 30,0 1 10))",
            "MULTILINESTRING Z ((0 1 10,2 3 20,4 5 30,0 1 10),(0 1 10,2 3 20,4 5 30,0 1 10))",
        ),
        (
            "MULTILINESTRING M ((0 1 10,2 3 20,4 5 30,0 1 10))",
            "MULTILINESTRING M ((0 1 10,2 3 20,4 5 30,0 1 10))",
        ),
        (
            "MULTILINESTRING ZM ((0 1 10 100,2 3 20 200,4 5 30 300,0 1 10 10))",
            "MULTILINESTRING ZM ((0 1 10 100,2 3 20 200,4 5 30 300,0 1 10 10))",
        ),
        ("MULTIPOLYGON EMPTY", "MULTIPOLYGON EMPTY"),
        ("MULTIPOLYGON (EMPTY)", "MULTIPOLYGON EMPTY"),
        ("MULTIPOLYGON Z EMPTY", "MULTIPOLYGON Z EMPTY"),
        ("MULTIPOLYGON Z (EMPTY)", "MULTIPOLYGON Z EMPTY"),
        ("MULTIPOLYGON M EMPTY", "MULTIPOLYGON M EMPTY"),
        ("MULTIPOLYGON ZM EMPTY", "MULTIPOLYGON ZM EMPTY"),
        ("MULTIPOLYGON ((EMPTY))", "MULTIPOLYGON EMPTY"),
        ("MULTIPOLYGON (((0 1,2 3,4 5,0 1)))", "MULTIPOLYGON (((0 1,2 3,4 5,0 1)))"),
        (
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)),((2 3,4 5,6 7,2 3)))",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)),((2 3,4 5,6 7,2 3)))",
        ),
        (
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1),(2 3,4 5,6 7,2 3)))",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1),(2 3,4 5,6 7,2 3)))",
        ),
        (
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)),EMPTY)",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)))",
        ),
        (
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1),EMPTY))",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)))",
        ),
        ("MULTIPOLYGON ((EMPTY,(0 1,2 3,4 5,0 1)))", "MULTIPOLYGON EMPTY"),
        (
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1),EMPTY,(2 3,4 5,6 7,2 3)))",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1),(2 3,4 5,6 7,2 3)))",
        ),
        (
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)),((0 1,2 3,4 5,0 1),(2 3,4 5,6 7,2 3)))",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)),((0 1,2 3,4 5,0 1),(2 3,4 5,6 7,2 3)))",
        ),
        (
            "MULTIPOLYGON (EMPTY,((0 1,2 3,4 5,0 1)))",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)))",
        ),
        (
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)),EMPTY)",
            "MULTIPOLYGON (((0 1,2 3,4 5,0 1)))",
        ),
        (
            "MULTIPOLYGON Z (((0 1 10,2 3 20,4 5 30,0 1 10)),((0 1 10,2 3 20,4 5 30,0 1 10)))",
            "MULTIPOLYGON Z (((0 1 10,2 3 20,4 5 30,0 1 10)),((0 1 10,2 3 20,4 5 30,0 1 10)))",
        ),
        (
            "MULTIPOLYGON M (((0 1 10,2 3 20,4 5 30,0 1 10)))",
            "MULTIPOLYGON M (((0 1 10,2 3 20,4 5 30,0 1 10)))",
        ),
        (
            "MULTIPOLYGON ZM (((0 1 10 100,2 3 20 200,4 5 30 300,0 1 10 10)))",
            "MULTIPOLYGON ZM (((0 1 10 100,2 3 20 200,4 5 30 300,0 1 10 10)))",
        ),
        ("GEOMETRYCOLLECTION EMPTY", "GEOMETRYCOLLECTION EMPTY"),
        ("GEOMETRYCOLLECTION Z EMPTY", "GEOMETRYCOLLECTION Z EMPTY"),
        ("GEOMETRYCOLLECTION M EMPTY", "GEOMETRYCOLLECTION M EMPTY"),
        ("GEOMETRYCOLLECTION ZM EMPTY", "GEOMETRYCOLLECTION ZM EMPTY"),
        (
            "GEOMETRYCOLLECTION Z (POINT Z (0 1 2),LINESTRING Z (0 1 2,3 4 5))",
            "GEOMETRYCOLLECTION Z (POINT Z (0 1 2),LINESTRING Z (0 1 2,3 4 5))",
        ),
        (
            "GEOMETRYCOLLECTION M (POINT M (0 1 2),LINESTRING M (0 1 2,3 4 5))",
            "GEOMETRYCOLLECTION M (POINT M (0 1 2),LINESTRING M (0 1 2,3 4 5))",
        ),
        (
            "GEOMETRYCOLLECTION ZM (POINT ZM (0 1 2 10),LINESTRING ZM (0 1 2 10,3 4 5 20))",
            "GEOMETRYCOLLECTION ZM (POINT ZM (0 1 2 10),LINESTRING ZM (0 1 2 10,3 4 5 20))",
        ),
        (
            "GEOMETRYCOLLECTION (POINT EMPTY,LINESTRING EMPTY,POLYGON EMPTY,MULTIPOINT EMPTY,MULTILINESTRING EMPTY,MULTIPOLYGON EMPTY,GEOMETRYCOLLECTION EMPTY)",
            "GEOMETRYCOLLECTION (POINT EMPTY,LINESTRING EMPTY,POLYGON EMPTY,MULTIPOINT EMPTY,MULTILINESTRING EMPTY,MULTIPOLYGON EMPTY,GEOMETRYCOLLECTION EMPTY)",
        ),
        (
            "GEOMETRYCOLLECTION (POINT Z EMPTY,LINESTRING Z EMPTY,POLYGON Z EMPTY,MULTIPOINT Z EMPTY,MULTILINESTRING Z EMPTY,MULTIPOLYGON Z EMPTY,GEOMETRYCOLLECTION Z EMPTY)",
            "GEOMETRYCOLLECTION Z (POINT Z EMPTY,LINESTRING Z EMPTY,POLYGON Z EMPTY,MULTIPOINT Z EMPTY,MULTILINESTRING Z EMPTY,MULTIPOLYGON Z EMPTY,GEOMETRYCOLLECTION Z EMPTY)",
        ),
        # Not SF1.2 compliant but recognized
        (
            "GEOMETRYCOLLECTION (POINT(EMPTY),LINESTRING(EMPTY),POLYGON(EMPTY),MULTIPOINT(EMPTY),MULTILINESTRING(EMPTY),MULTIPOLYGON(EMPTY),GEOMETRYCOLLECTION(EMPTY))",
            "GEOMETRYCOLLECTION (POINT EMPTY,LINESTRING EMPTY,POLYGON EMPTY,MULTIPOINT EMPTY,MULTILINESTRING EMPTY,MULTIPOLYGON EMPTY,GEOMETRYCOLLECTION EMPTY)",
        ),
        ("CURVEPOLYGON EMPTY", "CURVEPOLYGON EMPTY"),
        ("CURVEPOLYGON (EMPTY)", "CURVEPOLYGON EMPTY"),
        ("MULTICURVE EMPTY", "MULTICURVE EMPTY"),
        ("MULTICURVE (EMPTY)", "MULTICURVE EMPTY"),
        ("MULTISURFACE EMPTY", "MULTISURFACE EMPTY"),
        ("MULTISURFACE (EMPTY)", "MULTISURFACE EMPTY"),
    ],
)
def test_ogr_wkbwkt_test_import_wkt_sf12(input_wkt, expected_output_wkt):

    geom = ogr.CreateGeometryFromWkt(input_wkt)
    assert geom is not None
    out_wkt = geom.ExportToIsoWkt()
    assert out_wkt == expected_output_wkt

    # Test with input in lower case
    geom = ogr.CreateGeometryFromWkt(input_wkt.lower())
    assert geom is not None
    out_wkt = geom.ExportToIsoWkt()
    assert out_wkt == expected_output_wkt


###############################################################################
# Test importing non-conformant WKT with Z/M modifier directly appended to
# geometry type name


@pytest.mark.parametrize(
    "input_wkt,expected_output_wkt",
    [
        ("POINTZ EMPTY", "POINT Z EMPTY"),
        ("POINTM EMPTY", "POINT M EMPTY"),
        ("POINTZM EMPTY", "POINT ZM EMPTY"),
        ("POINTZ (0 1 2)", "POINT Z (0 1 2)"),
        ("POINTM (0 1 2)", "POINT M (0 1 2)"),
        ("POINTZM (0 1 2 3)", "POINT ZM (0 1 2 3)"),
    ],
)
def test_ogr_wkbwkt_test_import_wkt_z_m_modifier_without_space(
    input_wkt, expected_output_wkt
):

    geom = ogr.CreateGeometryFromWkt(input_wkt)
    assert geom is not None
    out_wkt = geom.ExportToIsoWkt()
    assert out_wkt == expected_output_wkt

    # Test with input in lower case
    geom = ogr.CreateGeometryFromWkt(input_wkt.lower())
    assert geom is not None
    out_wkt = geom.ExportToIsoWkt()
    assert out_wkt == expected_output_wkt


###############################################################################
# Test that importing the wkb that would be equivalent to MULTIPOINT(POLYGON((0 0))
# doesn't work


def test_ogr_wkbwkt_test_import_bad_multipoint_wkb():

    import struct

    wkb = struct.pack(
        "B" * 30,
        0,
        0,
        0,
        0,
        6,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        64,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        64,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    )
    with gdal.quiet_errors():
        geom = ogr.CreateGeometryFromWkb(wkb)
    assert geom is None


###############################################################################
# Test WKT -> WKB -> WKT roundtripping for GEOMETRYCOLLECTION


def test_ogr_wkbwkt_test_geometrycollection_wktwkb():

    wkt_list = [
        "GEOMETRYCOLLECTION (POINT (0 1))",
        "GEOMETRYCOLLECTION (LINESTRING (0 1,2 3))",
        "GEOMETRYCOLLECTION (POLYGON ((0 0,0 1,1 1,0 0)))",
        "GEOMETRYCOLLECTION (MULTIPOINT (0 1))",
        "GEOMETRYCOLLECTION (MULTILINESTRING ((0 1,2 3)))",
        "GEOMETRYCOLLECTION (MULTIPOLYGON (((0 0,0 1,1 1,0 0))))",
        "GEOMETRYCOLLECTION (GEOMETRYCOLLECTION (POINT (0 1)))",
        "GEOMETRYCOLLECTION (CIRCULARSTRING (0 0,1 0,0 0))",
        "GEOMETRYCOLLECTION (COMPOUNDCURVE ((0 0,1 0,0 0)))",
        "GEOMETRYCOLLECTION (CURVEPOLYGON ((0 0,0 1,1 1,0 0)))",
        "GEOMETRYCOLLECTION (MULTICURVE ((0 0,0 1,1 1,0 0)))",
        "GEOMETRYCOLLECTION (MULTISURFACE (((0 0,0 1,1 1,0 0))))",
    ]
    for wkt in wkt_list:
        g = ogr.CreateGeometryFromWkt(wkt)
        wkb = g.ExportToWkb()
        g = ogr.CreateGeometryFromWkb(wkb)
        wkt2 = g.ExportToWkt()
        assert wkt == wkt2, "fail for %s" % wkt


###############################################################################
# Test that importing too nested WKT doesn't cause stack overflows


def test_ogr_wkbwkt_test_geometrycollection_wkt_recursion():

    wkt = "GEOMETRYCOLLECTION (" * 31 + "GEOMETRYCOLLECTION EMPTY" + ")" * 31

    geom = ogr.CreateGeometryFromWkt(wkt)
    assert geom.ExportToWkt() == wkt, "expected %s" % wkt

    wkt = "GEOMETRYCOLLECTION (" * 32 + "GEOMETRYCOLLECTION EMPTY" + ")" * 32

    with gdal.quiet_errors():
        geom = ogr.CreateGeometryFromWkt(wkt)
    assert geom is None, "expected None"


###############################################################################
# Test that importing too nested WKB doesn't cause stack overflows


def test_ogr_wkbwkt_test_geometrycollection_wkb_recursion():

    import struct

    wkb_repeat = struct.pack("B" * 9, 0, 0, 0, 0, 7, 0, 0, 0, 1)
    wkb_end = struct.pack("B" * 9, 0, 0, 0, 0, 7, 0, 0, 0, 0)

    wkb = wkb_repeat * 31 + wkb_end

    geom = ogr.CreateGeometryFromWkb(wkb)
    assert geom is not None, "expected a geometry"

    wkb = struct.pack("B" * 0) + wkb_repeat * 32 + wkb_end

    with gdal.quiet_errors():
        geom = ogr.CreateGeometryFromWkb(wkb)
    assert geom is None, "expected None"


###############################################################################
# Test ISO WKT compliant export of MULTIPOINT


def test_ogr_wkbwkt_export_wkt_iso_multipoint():

    wkt = "MULTIPOINT ((0 0),(1 1))"
    g = ogr.CreateGeometryFromWkt(wkt)
    out_wkt = g.ExportToIsoWkt()
    assert out_wkt == wkt


###############################################################################
# Test exporting WKT with non finite values (#6319)


def test_ogr_wkt_inf_nan():

    g = ogr.Geometry(ogr.wkbPoint)
    g.AddPoint(float("inf"), float("-inf"), float("nan"))
    out_wkt = g.ExportToWkt()
    assert out_wkt == "POINT (inf -inf nan)"


###############################################################################
# Test corrupted WKT


def test_ogr_wkt_multicurve_compoundcurve_corrupted():

    with gdal.quiet_errors():
        g = ogr.CreateGeometryFromWkt("MULTICURVE(COMPOUNDCURVE")
    assert g is None


###############################################################################
# Test corrupted WKT


def test_ogr_wkt_multipolygon_corrupted():

    with gdal.quiet_errors():
        g = ogr.CreateGeometryFromWkt("MULTIPOLYGON(POLYGON((N")
    assert g is None


###############################################################################
# Test multiline WKT


def test_ogr_wkt_multiline():
    g = ogr.CreateGeometryFromWkt("""GEOMETRYCOLLECTION(
    POINT (1 2),
    LINESTRING (3 3, 4 4))
    """)
    assert g is not None


###############################################################################
# Test multipoint Z WKT non bracketed (like what PostGIS outputs)


def test_ogr_wkt_multipoint_postgis():
    g = ogr.CreateGeometryFromWkt("MULTIPOINT Z (1 2 3,4 5 6)")
    assert g is not None
    assert g.ExportToIsoWkt() == "MULTIPOINT Z ((1 2 3),(4 5 6))"


###############################################################################
# Test accepting nan which is an extension to the WKT spec, but both
# GEOS (https://github.com/libgeos/geos/pull/817) and PostGIS
# (https://trac.osgeo.org/postgis/ticket/4827) can now output such things...
# (actually GDAL too could output, but not ingest it back...)


@pytest.mark.parametrize(
    "wkt",
    [
        "POINT (nan nan)",
        "POINT (1.5 nan)",
        "POINT (nan 1.5)",
        "POINT Z (nan nan nan)",
        "POINT Z (1.5 1.5 nan)",
        "POINT M (nan nan nan)",
        "POINT M (1.5 1.5 nan)",
        "POINT ZM (nan nan nan nan)",
        "POINT ZM (1.5 1.5 nan nan)",
        "LINESTRING ZM (nan nan nan nan)",
    ],
)
def test_ogr_wkt_nan(wkt):
    g = ogr.CreateGeometryFromWkt(wkt)
    assert g is not None
    assert g.ExportToIsoWkt() == wkt


###############################################################################
# When imported build a list of units based on the files available.

# print 'hit enter'
# sys.stdin.readline()
