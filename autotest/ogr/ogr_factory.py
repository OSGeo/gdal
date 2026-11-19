#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some geometry factory methods, like arc stroking.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr


def save_as_csv(geom, filename):
    csv = 'ID,WKT\n0,"%s"\n' % geom.ExportToWkt()
    open("/home/warmerda/" + filename, "w").write(csv)


###############################################################################
# 30 degree rotated ellipse, just one quarter.


def test_ogr_factory_1():

    geom = ogr.ApproximateArcAngles(20, 30, 40, 7, 3.5, 30.0, 270.0, 360.0, 6.0)

    expected_geom = "LINESTRING (21.75 33.031088913245533 40,22.374083449152831 32.648634669593925 40,22.972155943227843 32.237161430239802 40,23.537664874825239 31.801177382099848 40,24.064414409750082 31.345459257641004 40,24.546633369868303 30.875 40,24.979038463342047 30.394954059253475 40,25.356892169480634 29.910580919184319 40,25.676054644008637 29.427187473276717 40,25.933029076066084 28.95006988128063 40,26.125 28.484455543377237 40,26.249864142195264 28.035445827688662 40,26.306253464980482 27.607960178621322 40,26.293550155134998 27.206682218403525 40,26.211893392779814 26.836008432340218 40,26.062177826491073 26.5 40)"

    ogrtest.check_feature_geometry(geom, expected_geom)


###############################################################################
# Test forceToPolygon()


def test_ogr_factory_2():

    src_wkt = "MULTIPOLYGON (((0 0,100 0,100 100,0 0)))"
    exp_wkt = "POLYGON((0 0,100 0,100 100,0 0))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTISURFACE (((0 0,100 0,100 100,0 0)))"
    exp_wkt = "POLYGON((0 0,100 0,100 100,0 0))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "CURVEPOLYGON ((0 0,100 0,100 100,0 0))"
    exp_wkt = "POLYGON((0 0,100 0,100 100,0 0))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "CURVEPOLYGON (CIRCULARSTRING(0 0,0 1,0 2,1 2,2 2,2 1,2 0,1 0,0 0))"
    exp_wkt = "POLYGON ((0 0,0 1,0 2,1 2,2 2,2 1,2 0,1 0,0 0))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################
# Test forceToMultiPolygon()


def test_ogr_factory_3():

    src_wkt = "POLYGON((0 0,100 0,100 100,0 0))"
    exp_wkt = "MULTIPOLYGON (((0 0,100 0,100 100,0 0)))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "GEOMETRYCOLLECTION(POLYGON((0 0,100 0,100 100,0 0)))"
    exp_wkt = "MULTIPOLYGON (((0 0,100 0,100 100,0 0)))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "CURVEPOLYGON ((0 0,100 0,100 100,0 0))"
    exp_wkt = "MULTIPOLYGON (((0 0,100 0,100 100,0 0)))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTISURFACE (((0 0,100 0,100 100,0 0)))"
    exp_wkt = "MULTIPOLYGON (((0 0,100 0,100 100,0 0)))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiPolygon(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################
# Test forceToMultiPoint()


def test_ogr_factory_4():

    src_wkt = "POINT(2 5 3)"
    exp_wkt = "MULTIPOINT(2 5 3)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiPoint(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "GEOMETRYCOLLECTION(POINT(2 5 3),POINT(4 5 5))"
    exp_wkt = "MULTIPOINT(2 5 3,4 5 5)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiPoint(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################
# Test forceToMultiLineString()


def test_ogr_factory_5():

    src_wkt = "LINESTRING(2 5,10 20)"
    exp_wkt = "MULTILINESTRING((2 5,10 20))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "GEOMETRYCOLLECTION(LINESTRING(2 5,10 20),LINESTRING(0 0,10 10))"
    exp_wkt = "MULTILINESTRING((2 5,10 20),(0 0,10 10))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "POLYGON((2 5,10 20),(0 0,10 10))"
    exp_wkt = "MULTILINESTRING((2 5,10 20),(0 0,10 10))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTIPOLYGON(((2 5,10 20),(0 0,10 10)),((2 5,10 20)))"
    exp_wkt = "MULTILINESTRING((2 5,10 20),(0 0,10 10),(2 5,10 20))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToMultiLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################
# Test robustness of forceToXXX() primitives with various inputs (#3504)


@gdaltest.disable_exceptions()
def test_ogr_factory_6():

    src_wkt_list = [
        None,
        "POINT EMPTY",
        "LINESTRING EMPTY",
        "POLYGON EMPTY",
        "MULTIPOINT EMPTY",
        "MULTILINESTRING EMPTY",
        "MULTIPOLYGON EMPTY",
        "GEOMETRYCOLLECTION EMPTY",
        "POINT(0 0)",
        "LINESTRING(0 0)",
        "POLYGON((0 0))",
        "POLYGON(EMPTY,(0 0),EMPTY,(1 1))",
        "MULTIPOINT(EMPTY,(0 0),EMPTY,(1 1))",
        "MULTILINESTRING(EMPTY,(0 0),EMPTY,(1 1))",
        "MULTIPOLYGON(((0 0),EMPTY,(1 1)),EMPTY,((2 2)))",
        "GEOMETRYCOLLECTION(POINT EMPTY)",
        "GEOMETRYCOLLECTION(LINESTRING EMPTY)",
        "GEOMETRYCOLLECTION(POLYGON EMPTY)",
        "GEOMETRYCOLLECTION(MULTIPOINT EMPTY)",
        "GEOMETRYCOLLECTION(MULTILINESTRING EMPTY)",
        "GEOMETRYCOLLECTION(MULTIPOLYGON EMPTY)",
        "GEOMETRYCOLLECTION(GEOMETRYCOLLECTION EMPTY)",
        "GEOMETRYCOLLECTION(POINT(0 0))",
        "GEOMETRYCOLLECTION(LINESTRING(0 0),LINESTRING(1 1))",
        "GEOMETRYCOLLECTION(POLYGON((0 0),EMPTY,(2 2)), POLYGON((1 1)))",
        "CURVEPOLYGON EMPTY",
        "CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))",
        "CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))",
        "COMPOUNDCURVE EMPTY",
        "COMPOUNDCURVE ((0 0,0 1,1 1,1 0,0 0))",
        "COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))",
        "CIRCULARSTRING EMPTY",
        "CIRCULARSTRING (0 0,1 0,0 0)",
        "MULTISURFACE EMPTY",
        "MULTISURFACE (((0 0,0 1,1 1,1 0,0 0)))",
        "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,1 0,0 0)))",
        "MULTICURVE EMPTY",
        "MULTICURVE ((0 0,0 1))",
        "MULTICURVE (COMPOUNDCURVE((0 0,0 1)))",
        "MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))",
    ]

    for src_wkt in src_wkt_list:
        if src_wkt is None:
            src_geom = None
        else:
            src_geom = ogr.CreateGeometryFromWkt(src_wkt)

        ogr.ForceToPolygon(src_geom)
        ogr.ForceToMultiPolygon(src_geom)
        ogr.ForceToMultiPoint(src_geom)
        ogr.ForceToMultiLineString(src_geom)
        ogr.ForceToLineString(src_geom)
        for target_type in range(ogr.wkbMultiSurface):
            with gdal.quiet_errors():
                ogr.ForceTo(src_geom, 1 + target_type)
        # print(src_geom.ExportToWkt(), dst_geom1.ExportToWkt(), dst_geom2.ExportToWkt(), dst_geom3.ExportToWkt(), dst_geom4.ExportToWkt())


###############################################################################
# Test forceToLineString()


def test_ogr_factory_7():

    src_wkt = "LINESTRING(2 5,10 20)"
    exp_wkt = "LINESTRING(2 5,10 20)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTILINESTRING((2 5,10 20))"
    exp_wkt = "LINESTRING(2 5,10 20)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTICURVE((2 5,10 20))"
    exp_wkt = "LINESTRING(2 5,10 20)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTICURVE(COMPOUNDCURVE((2 5,10 20)))"
    exp_wkt = "LINESTRING(2 5,10 20)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTILINESTRING((2 5,10 20),(3 4,30 40))"
    exp_wkt = "MULTILINESTRING((2 5,10 20),(3 4,30 40))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTILINESTRING((2 5,10 20),(10 20,30 40))"
    exp_wkt = "LINESTRING (2 5,10 20,30 40)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "GEOMETRYCOLLECTION(LINESTRING(2 5,10 20),LINESTRING(10 20,30 40))"
    exp_wkt = "LINESTRING (2 5,10 20,30 40)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTILINESTRING((2 5,10 20),(10 20))"
    exp_wkt = "MULTILINESTRING((2 5,10 20),(10 20))"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "MULTILINESTRING((2 5,10 20),(10 20,30 40),(30 40,50 60))"
    exp_wkt = "LINESTRING (2 5,10 20,30 40,50 60)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "POLYGON ((0 0,0 1,1 1,1 0,0 0))"
    exp_wkt = "LINESTRING (0 0,0 1,1 1,1 0,0 0)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))"
    exp_wkt = "LINESTRING (0 0,0 1,1 1,1 0,0 0)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)

    src_wkt = "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,1 0,0 0)))"
    exp_wkt = "LINESTRING (0 0,0 1,1 1,1 0,0 0)"

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceToLineString(src_geom)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################
# Test forceTo()


@pytest.mark.parametrize(
    "src_wkt,exp_wkt,target_type",
    [
        ("POINT (2 5)", "POINT ZM (2 5 0 0)", ogr.wkbPointZM),
        ("POINT ZM (2 5 3 4)", "POINT ZM (2 5 3 4)", ogr.wkbPointZM),
        ("POINT ZM (2 5 3 4)", "MULTIPOINT ZM ((2 5 3 4))", ogr.wkbMultiPointZM),
        ("POINT EMPTY", "POINT EMPTY", ogr.wkbPoint),
        ("POINT EMPTY", "POINT ZM EMPTY", ogr.wkbPointZM),
        ("POINT EMPTY", "MULTIPOINT EMPTY", ogr.wkbMultiPoint),
        ("POINT EMPTY", "MULTIPOINT ZM EMPTY", ogr.wkbMultiPointZM),
        ("POINT ZM EMPTY", "POINT EMPTY", ogr.wkbPoint),
        ("POINT ZM EMPTY", "POINT ZM EMPTY", ogr.wkbPointZM),
        ("POINT ZM EMPTY", "MULTIPOINT EMPTY", ogr.wkbMultiPoint),
        ("POINT ZM EMPTY", "MULTIPOINT ZM EMPTY", ogr.wkbMultiPointZM),
        ("MULTIPOINT ZM EMPTY", "POINT EMPTY", ogr.wkbPoint),
        ("MULTIPOINT ZM EMPTY", "POINT ZM EMPTY", ogr.wkbPointZM),
        ("MULTIPOINT ZM EMPTY", "MULTIPOINT EMPTY", ogr.wkbMultiPoint),
        ("MULTIPOINT ZM EMPTY", "MULTIPOINT ZM EMPTY", ogr.wkbMultiPointZM),
        ("POINT(2 5)", "MULTIPOINT (2 5)", ogr.wkbMultiPoint),
        ("LINESTRING(2 5,10 20)", "LINESTRING(2 5,10 20)", ogr.wkbLineString),
        ("LINESTRING(2 5,10 20)", "COMPOUNDCURVE ((2 5,10 20))", ogr.wkbCompoundCurve),
        (
            "LINESTRING(2 5,10 20)",
            "MULTILINESTRING ((2 5,10 20))",
            ogr.wkbMultiLineString,
        ),
        ("LINESTRING(2 5,10 20)", "MULTICURVE ((2 5,10 20))", ogr.wkbMultiCurve),
        ("LINESTRING(2 5,10 20)", None, ogr.wkbPolygon),
        ("LINESTRING(2 5,10 20)", None, ogr.wkbCurvePolygon),
        ("LINESTRING(2 5,10 20)", None, ogr.wkbMultiSurface),
        ("LINESTRING(2 5,10 20)", None, ogr.wkbMultiPolygon),
        ("LINESTRING(0 0,0 1,1 1,0 0)", "POLYGON ((0 0,0 1,1 1,0 0))", ogr.wkbPolygon),
        (
            "LINESTRING(0 0,0 1,1 1,0 0)",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "LINESTRING(0 0,0 1,1 1,0 0)",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "LINESTRING(0 0,0 1,1 1,0 0)",
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiSurface,
        ),
        ("LINESTRING EMPTY", "COMPOUNDCURVE EMPTY", ogr.wkbCompoundCurve),
        ("LINESTRING EMPTY", "MULTILINESTRING EMPTY", ogr.wkbMultiLineString),
        ("LINESTRING EMPTY", "MULTICURVE EMPTY", ogr.wkbMultiCurve),
        ("MULTILINESTRING ((2 5,10 20))", "LINESTRING(2 5,10 20)", ogr.wkbLineString),
        (
            "MULTILINESTRING ((2 5,10 20))",
            "COMPOUNDCURVE ((2 5,10 20))",
            ogr.wkbCompoundCurve,
        ),
        (
            "MULTILINESTRING ((2 5,10 20))",
            "MULTICURVE ((2 5,10 20))",
            ogr.wkbMultiCurve,
        ),
        ("MULTILINESTRING ((2 5,10 20))", None, ogr.wkbPolygon),
        ("MULTILINESTRING ((2 5,10 20))", None, ogr.wkbCurvePolygon),
        ("MULTILINESTRING ((2 5,10 20))", None, ogr.wkbMultiPolygon),
        ("MULTILINESTRING ((2 5,10 20))", None, ogr.wkbMultiSurface),
        (
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiSurface,
        ),
        ("MULTILINESTRING EMPTY", "LINESTRING EMPTY", ogr.wkbLineString),
        ("MULTILINESTRING EMPTY", "COMPOUNDCURVE EMPTY", ogr.wkbCompoundCurve),
        ("MULTILINESTRING EMPTY", "MULTICURVE EMPTY", ogr.wkbMultiCurve),
        (
            "CIRCULARSTRING(0 0,1 0,0 0)",
            "COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "CIRCULARSTRING(0 0,1 0,0 0)",
            "MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))",
            ogr.wkbMultiCurve,
        ),
        (
            "CIRCULARSTRING(0 0,1 0,0 0)",
            "CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "CIRCULARSTRING(0 0,1 0,0 0)",
            "POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "CIRCULARSTRING(0 0,1 0,0 0)",
            "MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "CIRCULARSTRING(0 0,1 0,0 0)",
            "MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))",
            ogr.wkbMultiSurface,
        ),
        (
            "CIRCULARSTRING(0 0,1 0,0 0)",
            "LINESTRING (0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)",
            ogr.wkbLineString,
        ),
        # Degenerated case
        ("CIRCULARSTRING(0 0,0 0,0 0)", "LINESTRING (0 0,0 0,0 0)", ogr.wkbLineString),
        ("CIRCULARSTRING(0 0,1 1,2 2)", "LINESTRING (0 0,1 1,2 2)", ogr.wkbLineString),
        (
            "CIRCULARSTRING(0 0,1 1,2 2)",
            "MULTILINESTRING ((0 0,1 1,2 2))",
            ogr.wkbMultiLineString,
        ),
        ("CIRCULARSTRING(0 0,1 1,2 2)", None, ogr.wkbPolygon),
        ("CIRCULARSTRING(0 0,1 1,2 2)", None, ogr.wkbCurvePolygon),
        ("CIRCULARSTRING(0 0,1 1,2 2)", None, ogr.wkbMultiSurface),
        ("CIRCULARSTRING(0 0,1 1,2 2)", None, ogr.wkbMultiPolygon),
        ("COMPOUNDCURVE ((2 5,10 20))", "LINESTRING(2 5,10 20)", ogr.wkbLineString),
        (
            "COMPOUNDCURVE (CIRCULARSTRING(0 0,1 1,2 2))",
            "LINESTRING (0 0,1 1,2 2)",
            ogr.wkbLineString,
        ),
        (
            "COMPOUNDCURVE ((2 5,10 20),(10 20,30 40))",
            "LINESTRING(2 5,10 20,30 40)",
            ogr.wkbLineString,
        ),
        (
            "COMPOUNDCURVE ((2 5,10 20),(10 20,30 40))",
            "MULTILINESTRING((2 5,10 20,30 40))",
            ogr.wkbMultiLineString,
        ),
        (
            "COMPOUNDCURVE ((2 5,10 20),(10 20,30 40))",
            "MULTICURVE (COMPOUNDCURVE ((2 5,10 20),(10 20,30 40)))",
            ogr.wkbMultiCurve,
        ),
        (
            "COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))",
            "CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0)))",
            ogr.wkbCurvePolygon,
        ),
        (
            "COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))",
            "POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))",
            "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))))",
            ogr.wkbMultiSurface,
        ),
        (
            "COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))",
            "MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "COMPOUNDCURVE (CIRCULARSTRING(0 0,1 0,0 0))",
            "LINESTRING (0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)",
            ogr.wkbLineString,
        ),
        (
            "COMPOUNDCURVE((0 0,0 1,1 1,0 0))",
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "COMPOUNDCURVE((0 0,0 1,1 1,0 0))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "COMPOUNDCURVE((0 0,0 1,1 1,0 0))",
            "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1,0 0))))",
            ogr.wkbMultiSurface,
        ),
        (
            "COMPOUNDCURVE((0 0,0 1,1 1,0 0))",
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            ogr.wkbCurvePolygon,
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiSurface,
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))",
            ogr.wkbCurvePolygon,
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            "LINESTRING (0 0,0 1,1 1,0 0)",
            ogr.wkbLineString,
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            "COMPOUNDCURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiSurface,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "LINESTRING (0 0,0 1,1 1,0 0)",
            ogr.wkbLineString,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "COMPOUNDCURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiLineString,
        ),
        (
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiCurve,
        ),
        (
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1,0 0))))",
            ogr.wkbMultiSurface,
        ),
        (
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            ogr.wkbCurvePolygon,
        ),
        (
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "LINESTRING (0 0,0 1,1 1,0 0)",
            ogr.wkbLineString,
        ),
        (
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "COMPOUNDCURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "CURVEPOLYGON (COMPOUNDCURVE((0 0,0 1),(0 1,1 1,0 0)))",
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))",
            "POLYGON ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))",
            "MULTISURFACE (CURVEPOLYGON ( CIRCULARSTRING (0 0,1 0,0 0)))",
            ogr.wkbMultiSurface,
        ),
        (
            "CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))",
            "MULTIPOLYGON (((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))",
            "COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))",
            "MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))",
            ogr.wkbMultiCurve,
        ),
        (
            "CURVEPOLYGON (CIRCULARSTRING(0 0,1 0,0 0))",
            "MULTILINESTRING ((0 0,0.116977778440514 -0.321393804843282,0.413175911166547 -0.49240387650611,0.75 -0.433012701892224,0.969846310392967 -0.171010071662835,0.969846310392967 0.171010071662835,0.75 0.433012701892224,0.413175911166547 0.49240387650611,0.116977778440514 0.321393804843282,0 0))",
            ogr.wkbMultiLineString,
        ),
        ("MULTICURVE ((2 5,10 20))", "LINESTRING(2 5,10 20)", ogr.wkbLineString),
        (
            "MULTICURVE ((2 5,10 20))",
            "COMPOUNDCURVE ((2 5,10 20))",
            ogr.wkbCompoundCurve,
        ),
        (
            "MULTICURVE ((2 5,10 20))",
            "MULTILINESTRING ((2 5,10 20))",
            ogr.wkbMultiLineString,
        ),
        (
            "MULTICURVE (COMPOUNDCURVE((2 5,10 20)))",
            "LINESTRING(2 5,10 20)",
            ogr.wkbLineString,
        ),
        (
            "MULTICURVE (COMPOUNDCURVE((2 5,10 20)))",
            "COMPOUNDCURVE ((2 5,10 20))",
            ogr.wkbCompoundCurve,
        ),
        (
            "MULTICURVE (COMPOUNDCURVE((2 5,10 20)))",
            "MULTILINESTRING ((2 5,10 20))",
            ogr.wkbMultiLineString,
        ),
        (
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbPolygon,
        ),
        (
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiSurface,
        ),
        (
            "MULTICURVE (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "MULTICURVE (COMPOUNDCURVE((0 0,0 1,1 1,0 0)))",
            "MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1,0 0))))",
            ogr.wkbMultiSurface,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbUnknown,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiSurface,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25)))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "LINESTRING (0 0,0 1,1 1,0 0)",
            ogr.wkbLineString,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "COMPOUNDCURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiLineString,
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiCurve,
        ),
        (
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTISURFACE (((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25)))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "LINESTRING (0 0,0 1,1 1,0 0)",
            ogr.wkbLineString,
        ),
        (
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "COMPOUNDCURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiLineString,
        ),
        (
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiCurve,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))",
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25)))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.25 0.25))",
            ogr.wkbCurvePolygon,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))",
            "LINESTRING (0 0,0 1,1 1,0 0)",
            ogr.wkbLineString,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))",
            "COMPOUNDCURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON(CIRCULARSTRING(0 0,1 0,0 0)))",
            "COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0))",
            ogr.wkbCompoundCurve,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))",
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiLineString,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,0 0)))",
            "MULTICURVE ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiCurve,
        ),
        (
            "MULTISURFACE (CURVEPOLYGON(CIRCULARSTRING(0 0,1 0,0 0)))",
            "MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0))",
            ogr.wkbMultiCurve,
        ),
        ("MULTIPOINT (2 5)", "POINT(2 5)", ogr.wkbPoint),
    ],
)
def test_ogr_factory_force_to(src_wkt, exp_wkt, target_type):

    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    with gdal.config_option("OGR_ARC_STEPSIZE", "45"):
        dst_geom = ogr.ForceTo(src_geom, target_type)

    if exp_wkt is None:
        exp_wkt = src_wkt
    elif target_type != ogr.wkbUnknown and dst_geom.GetGeometryType() != target_type:
        print(target_type)
        print(dst_geom.ExportToIsoWkt())
        assert False, (src_wkt, exp_wkt, target_type)

    ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################
# Test forceTo() to wkbUnknown


@pytest.mark.parametrize(
    "src_wkt,target_type,exp_wkt",
    [
        (
            "POINT (1 2)",
            ogr.wkbUnknown,
            "POINT (1 2)",
        ),
        (
            "POINT EMPTY",
            ogr.wkbUnknown,
            "POINT EMPTY",
        ),
    ],
)
def test_ogr_factory_forceTo_unknown(src_wkt, target_type, exp_wkt):
    src_geom = ogr.CreateGeometryFromWkt(src_wkt)
    dst_geom = ogr.ForceTo(src_geom, target_type)
    ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################
# Test forceTo()


def test_ogr_factory_failed_forceTo():

    tests = [
        (
            "MULTICURVE ZM ((0.0 0.0,0 0,0 0,0 0,0.0 0.0))",
            ogr.wkbTINM,
            "POLYGON M ((0 0 0,0 0 0,0 0 0,0 0 0,0 0 0))",
        ),
    ]
    for src_wkt, target_type, exp_wkt in tests:
        src_geom = ogr.CreateGeometryFromWkt(src_wkt)
        dst_geom = ogr.ForceTo(src_geom, target_type)

        ogrtest.check_feature_geometry(dst_geom, exp_wkt)


###############################################################################


@pytest.mark.parametrize(
    "input_wkt,output_type,expected_wkt",
    [
        ("POINT EMPTY", ogr.wkbPoint, "POINT EMPTY"),
        ("POINT EMPTY", ogr.wkbPoint25D, "POINT Z EMPTY"),
        ("POINT Z EMPTY", ogr.wkbPoint, "POINT EMPTY"),
        ("POINT Z EMPTY", ogr.wkbPoint25D, "POINT Z EMPTY"),
        ("POINT (1 2)", ogr.wkbPoint, "POINT (1 2)"),
        ("POINT (1 2)", ogr.wkbPoint25D, "POINT Z (1 2 0)"),
        ("POINT (1 2 3)", ogr.wkbPoint25D, "POINT Z (1 2 3)"),
        ("POINT (1 2 3)", ogr.wkbPoint, "POINT (1 2)"),
        ("POINT (1 2)", ogr.wkbMultiPoint, "MULTIPOINT ((1 2))"),
        ("POINT (1 2)", ogr.wkbMultiPoint25D, "MULTIPOINT Z ((1 2 0))"),
        ("POINT (1 2 3)", ogr.wkbMultiPoint, "MULTIPOINT ((1 2))"),
        ("POINT (1 2 3)", ogr.wkbMultiPoint25D, "MULTIPOINT Z ((1 2 3))"),
        ("MULTIPOINT EMPTY", ogr.wkbPoint, "POINT EMPTY"),
        ("MULTIPOINT EMPTY", ogr.wkbPoint25D, "POINT Z EMPTY"),
        ("MULTIPOINT Z EMPTY", ogr.wkbPoint, "POINT EMPTY"),
        ("MULTIPOINT Z EMPTY", ogr.wkbPoint25D, "POINT Z EMPTY"),
        ("MULTIPOINT ((1 2))", ogr.wkbPoint, "POINT (1 2)"),
        ("MULTIPOINT ((1 2))", ogr.wkbPoint25D, "POINT Z (1 2 0)"),
        ("MULTIPOINT ((1 2 3))", ogr.wkbPoint25D, "POINT Z (1 2 3)"),
        ("MULTIPOINT ((1 2 3))", ogr.wkbPoint, "POINT (1 2)"),
        ("MULTIPOINT ((1 2))", ogr.wkbMultiPoint, "MULTIPOINT ((1 2))"),
        ("MULTIPOINT ((1 2))", ogr.wkbMultiPoint25D, "MULTIPOINT Z ((1 2 0))"),
        ("MULTIPOINT ((1 2 3))", ogr.wkbMultiPoint, "MULTIPOINT ((1 2))"),
        ("MULTIPOINT ((1 2 3))", ogr.wkbMultiPoint25D, "MULTIPOINT Z ((1 2 3))"),
        ("LINESTRING EMPTY", ogr.wkbLineString, "LINESTRING EMPTY"),
        ("LINESTRING EMPTY", ogr.wkbLineString25D, "LINESTRING Z EMPTY"),
        ("LINESTRING Z EMPTY", ogr.wkbLineString, "LINESTRING EMPTY"),
        ("LINESTRING Z EMPTY", ogr.wkbLineString25D, "LINESTRING Z EMPTY"),
        ("LINESTRING (1 2)", ogr.wkbLineString, "LINESTRING (1 2)"),
        ("LINESTRING (1 2)", ogr.wkbLineString25D, "LINESTRING Z (1 2 0)"),
        ("LINESTRING (1 2 3)", ogr.wkbLineString25D, "LINESTRING Z (1 2 3)"),
        ("LINESTRING (1 2 3)", ogr.wkbLineString, "LINESTRING (1 2)"),
        ("LINESTRING (1 2)", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2))"),
        ("LINESTRING (1 2)", ogr.wkbMultiLineString25D, "MULTILINESTRING Z ((1 2 0))"),
        ("LINESTRING (1 2 3)", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2))"),
        (
            "LINESTRING (1 2 3)",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((1 2 3))",
        ),
        ("MULTILINESTRING EMPTY", ogr.wkbLineString, "LINESTRING EMPTY"),
        ("MULTILINESTRING EMPTY", ogr.wkbLineString25D, "LINESTRING Z EMPTY"),
        ("MULTILINESTRING Z EMPTY", ogr.wkbLineString, "LINESTRING EMPTY"),
        ("MULTILINESTRING Z EMPTY", ogr.wkbLineString25D, "LINESTRING Z EMPTY"),
        ("MULTILINESTRING ((1 2))", ogr.wkbLineString, "LINESTRING (1 2)"),
        ("MULTILINESTRING ((1 2))", ogr.wkbLineString25D, "LINESTRING Z (1 2 0)"),
        ("MULTILINESTRING ((1 2 3))", ogr.wkbLineString25D, "LINESTRING Z (1 2 3)"),
        ("MULTILINESTRING ((1 2 3))", ogr.wkbLineString, "LINESTRING (1 2)"),
        ("MULTILINESTRING ((1 2))", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2))"),
        (
            "MULTILINESTRING ((1 2))",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((1 2 0))",
        ),
        (
            "MULTILINESTRING ((1 2 3))",
            ogr.wkbMultiLineString,
            "MULTILINESTRING ((1 2))",
        ),
        (
            "MULTILINESTRING ((1 2 3))",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((1 2 3))",
        ),
        ("POLYGON EMPTY", ogr.wkbPolygon, "POLYGON EMPTY"),
        ("POLYGON EMPTY", ogr.wkbPolygon25D, "POLYGON Z EMPTY"),
        ("POLYGON Z EMPTY", ogr.wkbPolygon, "POLYGON EMPTY"),
        ("POLYGON Z EMPTY", ogr.wkbPolygon25D, "POLYGON Z EMPTY"),
        ("POLYGON ((0 0,0 1,1 1,0 0))", ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,0 0))"),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbPolygon25D,
            "POLYGON Z ((0 0 0,0 1 0,1 1 0,0 0 0))",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbPolygon,
            "POLYGON ((0 0,0 1,1 1,0 0))",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbPolygon25D,
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiPolygon,
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiPolygon25D,
            "MULTIPOLYGON Z (((0 0 0,0 1 0,1 1 0,0 0 0)))",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbMultiPolygon,
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbMultiPolygon25D,
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbLineString,
            "LINESTRING (0 0,0 1,1 1,0 0)",
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbLineString25D,
            "LINESTRING Z (0 0 0,0 1 0,1 1 0,0 0 0)",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbLineString,
            "LINESTRING (0 0,0 1,1 1,0 0)",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbLineString25D,
            "LINESTRING Z (0 0 10,0 1 10,1 1 10,0 0 10)",
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiLineString,
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
        ),
        (
            "POLYGON ((0 0,0 1,1 1,0 0))",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((0 0 0,0 1 0,1 1 0,0 0 0))",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbMultiLineString,
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
        ),
        (
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
        ),
        ("MULTIPOLYGON EMPTY", ogr.wkbPolygon, "POLYGON EMPTY"),
        ("MULTIPOLYGON EMPTY", ogr.wkbPolygon25D, "POLYGON Z EMPTY"),
        ("MULTIPOLYGON Z EMPTY", ogr.wkbPolygon, "POLYGON EMPTY"),
        ("MULTIPOLYGON Z EMPTY", ogr.wkbPolygon25D, "POLYGON Z EMPTY"),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbPolygon,
            "POLYGON ((0 0,0 1,1 1,0 0))",
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbPolygon25D,
            "POLYGON Z ((0 0 0,0 1 0,1 1 0,0 0 0))",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbPolygon,
            "POLYGON ((0 0,0 1,1 1,0 0))",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbPolygon25D,
            "POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon,
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiPolygon25D,
            "MULTIPOLYGON Z (((0 0 0,0 1 0,1 1 0,0 0 0)))",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbMultiPolygon,
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbMultiPolygon25D,
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbLineString,
            "LINESTRING (0 0,0 1,1 1,0 0)",
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbLineString25D,
            "LINESTRING Z (0 0 0,0 1 0,1 1 0,0 0 0)",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbLineString,
            "LINESTRING (0 0,0 1,1 1,0 0)",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbLineString25D,
            "LINESTRING Z (0 0 10,0 1 10,1 1 10,0 0 10)",
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiLineString,
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
        ),
        (
            "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((0 0 0,0 1 0,1 1 0,0 0 0))",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbMultiLineString,
            "MULTILINESTRING ((0 0,0 1,1 1,0 0))",
        ),
        (
            "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING Z ((0 0 10,0 1 10,1 1 10,0 0 10))",
        ),
    ],
)
def test_ogr_factory_ForceTo(input_wkt, output_type, expected_wkt):

    g = ogr.CreateGeometryFromWkt(input_wkt)
    g = ogr.ForceTo(g, output_type)
    assert g.ExportToIsoWkt() == expected_wkt
