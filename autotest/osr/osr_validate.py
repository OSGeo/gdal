#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test (error cases of) OSRValidate
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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


import pytest

from osgeo import osr

###############################################################################
# No root pointer


def test_osr_validate_1():

    empty_srs = osr.SpatialReference()
    with pytest.raises(Exception):
        empty_srs.Validate()


###############################################################################
# Unrecognized root node


def test_osr_validate_2():

    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("FOO[]")
    with pytest.raises(Exception):
        srs.Validate()


###############################################################################
# COMPD_CS errors


def test_osr_validate_3():

    # No DATUM child in GEOGCS
    srs = osr.SpatialReference()

    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""COMPD_CS[]""")
    with pytest.raises(Exception):
        srs.Validate()

    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""COMPD_CS["MYNAME",GEOGCS[]]""")
    with pytest.raises(Exception):
        srs.Validate()

    # AUTHORITY has wrong number of children (1), not 2.
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""COMPD_CS["MYNAME",AUTHORITY[]]""")
    with pytest.raises(Exception):
        srs.Validate()

    # Unexpected child for COMPD_CS `FOO'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""COMPD_CS["MYNAME",FOO[]]""")
    with pytest.raises(Exception):
        srs.Validate()


###############################################################################
# VERT_CS errors


def test_osr_validate_4():

    # Invalid number of children : 1
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""VERT_CS["MYNAME",VERT_DATUM[]]""")
    with pytest.raises(Exception):
        srs.Validate()

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""VERT_CS["MYNAME",UNIT[]]""")
    with pytest.raises(Exception):
        srs.Validate()

    # AXIS has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""VERT_CS["MYNAME",AXIS[]]""")
    with pytest.raises(Exception):
        srs.Validate()

    # AUTHORITY has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""VERT_CS["MYNAME",AUTHORITY[]]""")
    with pytest.raises(Exception):
        srs.Validate()

    # Unexpected child for VERT_CS `FOO'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""VERT_CS["MYNAME",FOO[]]""")
    with pytest.raises(Exception):
        srs.Validate()

    # No VERT_DATUM child in VERT_CS
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt("""VERT_CS["MYNAME"]""")
    with pytest.raises(Exception):
        srs.Validate()

    # No UNIT child in VERT_CS
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            """VERT_CS["MYNAME",VERT_DATUM["MYNAME",2005,AUTHORITY["EPSG","0"]]]"""
        )
    with pytest.raises(Exception):
        srs.Validate()

    # Too many AXIS children in VERT_CS
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            """VERT_CS["MYNAME",VERT_DATUM["MYNAME",2005,AUTHORITY["EPSG","0"]],UNIT["metre",1],AXIS["foo",foo],AXIS["bar",bar]]"""
        )
    with pytest.raises(Exception):
        srs.Validate()


###############################################################################
# GEOCCS errors


def test_osr_validate_5():

    # srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]')

    # PRIMEM has wrong number of children (1),not 2 or 3 as expected
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM[],UNIT["meter",1]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT[]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # AXIS has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],AXIS[],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # AUTHORITY has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1],AUTHORITY[]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # Unexpected child for GEOCCS `FOO'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('GEOCCS["My Geocentric",FOO[]]')
    with pytest.raises(Exception):
        srs.Validate()

    # No DATUM child in GEOCCS
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('GEOCCS["My Geocentric"]')
    with pytest.raises(Exception):
        srs.Validate()

    # No PRIMEM child in GEOCCS
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # No UNIT child in GEOCCS
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]],PRIMEM["Greenwich",0]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # Wrong number of AXIS children in GEOCCS
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],AXIS["foo",foo],UNIT["meter",1]]'
        )
    with pytest.raises(Exception):
        srs.Validate()


###############################################################################
# PROJCS errors


def test_osr_validate_6():

    # srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32631"]]')

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",UNIT[]]')
    with pytest.raises(Exception):
        srs.Validate()

    # PARAMETER has wrong number of children (1),not 2 as expected
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PARAMETER[]]')
    with pytest.raises(Exception):
        srs.Validate()

    # Unrecognized PARAMETER `foo'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PARAMETER["foo",0]]')
    with pytest.raises(Exception):
        srs.Validate()

    # PROJECTION has wrong number of children (0),not 1 or 2 as expected
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION]')
    with pytest.raises(Exception):
        srs.Validate()

    # Unrecognized PROJECTION `foo'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["foo"]]')
    with pytest.raises(Exception):
        srs.Validate()

    # Unsupported, but recognised PROJECTION `Tunisia_Mining_Grid'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Tunisia_Mining_Grid"]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # Unexpected child for PROJECTION `FOO'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Transverse_Mercator", FOO]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # AUTHORITY has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt(
            'PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Transverse_Mercator", AUTHORITY]]'
        )
    with pytest.raises(Exception):
        srs.Validate()

    # AUTHORITY has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",AUTHORITY]]')
    with pytest.raises(Exception):
        srs.Validate()

    # AXIS has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",AXIS]]')
    with pytest.raises(Exception):
        srs.Validate()

    # Unexpected child for PROJCS `FOO'
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",FOO]]')
    with pytest.raises(Exception):
        srs.Validate()

    # PROJCS does not have PROJECTION subnode.
    srs = osr.SpatialReference()
    with osr.ExceptionMgr(useExceptions=False):
        srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N"]')
    with pytest.raises(Exception):
        srs.Validate()


###############################################################################
