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



from osgeo import osr

###############################################################################
# No root pointer


def test_osr_validate_1():

    empty_srs = osr.SpatialReference()
    assert empty_srs.Validate() != 0

###############################################################################
# Unrecognized root node


def test_osr_validate_2():

    srs = osr.SpatialReference()
    srs.ImportFromWkt("FOO[]")
    assert srs.Validate() != 0

###############################################################################
# COMPD_CS errors


def test_osr_validate_3():

    # No DATUM child in GEOGCS
    srs = osr.SpatialReference()

    srs.ImportFromWkt("""COMPD_CS[]""")
    # 5 is OGRERR_CORRUPT_DATA.
    assert srs.Validate() != 0

    srs.ImportFromWkt("""COMPD_CS["MYNAME",GEOGCS[]]""")
    assert srs.Validate() != 0

    # AUTHORITY has wrong number of children (1), not 2.
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""COMPD_CS["MYNAME",AUTHORITY[]]""")
    assert srs.Validate() != 0

    # Unexpected child for COMPD_CS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""COMPD_CS["MYNAME",FOO[]]""")
    assert srs.Validate() != 0

###############################################################################
# VERT_CS errors


def test_osr_validate_4():

    # Invalid number of children : 1
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",VERT_DATUM[]]""")
    assert srs.Validate() != 0

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",UNIT[]]""")
    assert srs.Validate() != 0

    # AXIS has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",AXIS[]]""")
    assert srs.Validate() != 0

    # AUTHORITY has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",AUTHORITY[]]""")
    assert srs.Validate() != 0

    # Unexpected child for VERT_CS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",FOO[]]""")
    assert srs.Validate() != 0

    # No VERT_DATUM child in VERT_CS
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME"]""")
    assert srs.Validate() != 0

    # No UNIT child in VERT_CS
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",VERT_DATUM["MYNAME",2005,AUTHORITY["EPSG","0"]]]""")
    assert srs.Validate() != 0

    # Too many AXIS children in VERT_CS
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""VERT_CS["MYNAME",VERT_DATUM["MYNAME",2005,AUTHORITY["EPSG","0"]],UNIT["metre",1],AXIS["foo",foo],AXIS["bar",bar]]""")
    assert srs.Validate() != 0

###############################################################################
# GEOCCS errors


def test_osr_validate_5():

    # srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]')

    # PRIMEM has wrong number of children (1),not 2 or 3 as expected
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM[],UNIT["meter",1]]')
    assert srs.Validate() != 0

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT[]]')
    assert srs.Validate() != 0

    # AXIS has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],AXIS[],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1]]')
    assert srs.Validate() != 0

    # AUTHORITY has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["meter",1],AUTHORITY[]]')
    assert srs.Validate() != 0

    # Unexpected child for GEOCCS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",FOO[]]')
    assert srs.Validate() != 0

    # No DATUM child in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric"]')
    assert srs.Validate() != 0

    # No PRIMEM child in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]]]')
    assert srs.Validate() != 0

    # No UNIT child in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]],PRIMEM["Greenwich",0]]')
    assert srs.Validate() != 0

    # Wrong number of AXIS children in GEOCCS
    srs = osr.SpatialReference()
    srs.ImportFromWkt('GEOCCS["My Geocentric",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],AXIS["foo",foo],UNIT["meter",1]]')
    assert srs.Validate() != 0

###############################################################################
# PROJCS errors


def test_osr_validate_6():

    # srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32631"]]')

    # UNIT has wrong number of children (1), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",UNIT[]]')
    assert srs.Validate() != 0

    # PARAMETER has wrong number of children (1),not 2 as expected
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PARAMETER[]]')
    assert srs.Validate() != 0

    # Unrecognized PARAMETER `foo'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PARAMETER["foo",0]]')
    assert srs.Validate() != 0

    # PROJECTION has wrong number of children (0),not 1 or 2 as expected
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION]')
    assert srs.Validate() != 0

    # Unrecognized PROJECTION `foo'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["foo"]]')
    assert srs.Validate() != 0

    # Unsupported, but recognised PROJECTION `Tunisia_Mining_Grid'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Tunisia_Mining_Grid"]]')
    assert srs.Validate() != 0

    # Unexpected child for PROJECTION `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Transverse_Mercator", FOO]]')
    assert srs.Validate() != 0

    # AUTHORITY has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",PROJECTION["Transverse_Mercator", AUTHORITY]]')
    assert srs.Validate() != 0

    # AUTHORITY has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",AUTHORITY]]')
    assert srs.Validate() != 0

    # AXIS has wrong number of children (0), not 2
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",AXIS]]')
    assert srs.Validate() != 0

    # Unexpected child for PROJCS `FOO'
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N",FOO]]')
    assert srs.Validate() != 0

    # PROJCS does not have PROJECTION subnode.
    srs = osr.SpatialReference()
    srs.ImportFromWkt('PROJCS["WGS 84 / UTM zone 31N"]')
    assert srs.Validate() != 0

###############################################################################



