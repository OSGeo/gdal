#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test COMPD_CS support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
from osgeo import osr
import pytest

example_compd_wkt = 'COMPD_CS["OSGB36 / British National Grid + ODN",PROJCS["OSGB 1936 / British National Grid",GEOGCS["OSGB 1936",DATUM["OSGB_1936",SPHEROID["Airy 1830",6377563.396,299.3249646,AUTHORITY["EPSG","7001"]],TOWGS84[375,-111,431,0,0,0,0],AUTHORITY["EPSG","6277"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["DMSH",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4277"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",49],PARAMETER["central_meridian",-2],PARAMETER["scale_factor",0.999601272],PARAMETER["false_easting",400000],PARAMETER["false_northing",-100000],UNIT["metre_1",1,AUTHORITY["EPSG","9001"]],AXIS["E",EAST],AXIS["N",NORTH],AUTHORITY["EPSG","27700"]],VERT_CS["Newlyn",VERT_DATUM["Ordnance Datum Newlyn",2005,AUTHORITY["EPSG","5101"]],UNIT["metre_2",1,AUTHORITY["EPSG","9001"]],AXIS["Up",UP],AUTHORITY["EPSG","5701"]],AUTHORITY["EPSG","7405"]]'

###############################################################################
# Test parsing and a few operations on a compound coordinate system.


def test_osr_compd_1():

    srs = osr.SpatialReference()
    srs.ImportFromWkt(example_compd_wkt)

    assert srs.IsProjected(), 'Projected COMPD_CS not recognised as projected.'

    assert not srs.IsGeographic(), 'projected COMPD_CS misrecognised as geographic.'

    assert not srs.IsLocal(), 'projected COMPD_CS misrecognised as local.'

    assert srs.IsCompound(), 'COMPD_CS not recognised as compound.'

    expected_proj4 = '+proj=tmerc +lat_0=49 +lon_0=-2 +k=0.999601272 +x_0=400000 +y_0=-100000 +ellps=airy +towgs84=375,-111,431,0,0,0,0 +units=m +vunits=m +no_defs'
    got_proj4 = srs.ExportToProj4()

    if expected_proj4 != got_proj4:
        print('Got:      %s' % got_proj4)
        print('Expected: %s' % expected_proj4)
        pytest.fail('did not get expected proj.4 translation of compd_cs')

    assert srs.GetLinearUnitsName() == 'metre_1', 'Did not get expected linear units.'

    assert srs.Validate() == 0, 'Validate() failed.'

###############################################################################
# Test SetFromUserInput()


def test_osr_compd_2():

    srs = osr.SpatialReference()
    srs.SetFromUserInput(example_compd_wkt)

    assert srs.Validate() == 0, 'Does not validate'

    assert srs.IsProjected(), 'Projected COMPD_CS not recognised as projected.'

###############################################################################
# Test expansion of compound coordinate systems from EPSG definition.


def test_osr_compd_3():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(7401)

    assert srs.Validate() == 0, 'Does not validate'

    exp_wkt = """COMPD_CS["NTF (Paris) / France II + NGF Lallemand",
    PROJCS["NTF (Paris) / France II (deprecated)",
        GEOGCS["NTF (Paris)",
            DATUM["Nouvelle_Triangulation_Francaise_Paris",
                SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936265,
                    AUTHORITY["EPSG","7011"]],
                AUTHORITY["EPSG","6807"]],
            PRIMEM["Paris",2.33722917,
                AUTHORITY["EPSG","8903"]],
            UNIT["grad",0.01570796326794897,
                AUTHORITY["EPSG","9105"]],
            AUTHORITY["EPSG","4807"]],
        PROJECTION["Lambert_Conformal_Conic_1SP"],
        PARAMETER["latitude_of_origin",52],
        PARAMETER["central_meridian",0],
        PARAMETER["scale_factor",0.99987742],
        PARAMETER["false_easting",600000],
        PARAMETER["false_northing",2200000],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["X",EAST],
        AXIS["Y",NORTH],
        AUTHORITY["EPSG","27582"]],
    VERT_CS["NGF Lallemand height",
        VERT_DATUM["Nivellement General de la France - Lallemand",2005,
            AUTHORITY["EPSG","5118"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Gravity-related height",UP],
        AUTHORITY["EPSG","5719"]],
    AUTHORITY["EPSG","7401"]]"""
    wkt = srs.ExportToPrettyWkt()
    assert gdaltest.equal_srs_from_wkt(exp_wkt, wkt) != 0, \
        'did not get expected compound cs for EPSG:7401'

###############################################################################
# Test expansion of GCS+VERTCS compound coordinate system.


def test_osr_compd_4():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(7400)

    assert srs.Validate() == 0, 'Does not validate'

    exp_wkt = """COMPD_CS["NTF (Paris) + NGF IGN69 height",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936265,
                AUTHORITY["EPSG","7011"]],
            AUTHORITY["EPSG","6807"]],
        PRIMEM["Paris",2.33722917,
            AUTHORITY["EPSG","8903"]],
        UNIT["grad",0.01570796326794897,
            AUTHORITY["EPSG","9105"]],
        AUTHORITY["EPSG","4807"]],
    VERT_CS["NGF-IGN69 height",
        VERT_DATUM["Nivellement General de la France - IGN69",2005,
            AUTHORITY["EPSG","5119"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Gravity-related height",UP],
        AUTHORITY["EPSG","5720"]],
    AUTHORITY["EPSG","7400"]]"""
    wkt = srs.ExportToPrettyWkt()

    assert gdaltest.equal_srs_from_wkt(exp_wkt, wkt) != 0, \
        'did not get expected compound cs for EPSG:7400'

###############################################################################
# Test EPGS:x+y syntax


def test_osr_compd_5():

    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:26911+5703')

    assert srs.Validate() == 0, 'Does not validate'

    exp_wkt = """COMPD_CS["NAD83 / UTM zone 11N + NAVD88 height",
    PROJCS["NAD83 / UTM zone 11N",
        GEOGCS["NAD83",
            DATUM["North_American_Datum_1983",
                SPHEROID["GRS 1980",6378137,298.257222101,
                    AUTHORITY["EPSG","7019"]],
                AUTHORITY["EPSG","6269"]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433,
                AUTHORITY["EPSG","9122"]],
            AUTHORITY["EPSG","4269"]],
        PROJECTION["Transverse_Mercator"],
        PARAMETER["latitude_of_origin",0],
        PARAMETER["central_meridian",-117],
        PARAMETER["scale_factor",0.9996],
        PARAMETER["false_easting",500000],
        PARAMETER["false_northing",0],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH],
        AUTHORITY["EPSG","26911"]],
    VERT_CS["NAVD88 height",
        VERT_DATUM["North American Vertical Datum 1988",2005,
            AUTHORITY["EPSG","5103"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Gravity-related height",UP],
        AUTHORITY["EPSG","5703"]]]"""
    wkt = srs.ExportToPrettyWkt()

    if gdaltest.equal_srs_from_wkt(exp_wkt, wkt) == 0:
        pytest.fail()
    elif exp_wkt != wkt:
        print('warning they are equivalent, but not completely the same')
        print(wkt)

    exp_proj4 = '+proj=utm +zone=11 +datum=NAD83 +units=m +vunits=m +no_defs'
    proj4 = srs.ExportToProj4()
    assert proj4 == exp_proj4, ('Did not get expected proj.4 string, got:' + proj4)

###############################################################################
# Test conversion from PROJ.4 to WKT including vertical units.


def test_osr_compd_6():

    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=utm +zone=11 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +geoidgrids=g2003conus.gtx,g2003alaska.gtx,g2003h01.gtx,g2003p01.gtx +vunits=us-ft +no_defs ')

    assert srs.Validate() == 0, 'Does not validate'

    exp_wkt = """COMPD_CS["unknown",
    PROJCS["unknown",
        GEOGCS["unknown",
            DATUM["Unknown_based_on_GRS80_ellipsoid",
                SPHEROID["GRS 1980",6378137,298.257222101,
                    AUTHORITY["EPSG","7019"]],
                TOWGS84[0,0,0,0,0,0,0]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433,
                AUTHORITY["EPSG","9122"]]],
        PROJECTION["Transverse_Mercator"],
        PARAMETER["latitude_of_origin",0],
        PARAMETER["central_meridian",-117],
        PARAMETER["scale_factor",0.9996],
        PARAMETER["false_easting",500000],
        PARAMETER["false_northing",0],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH]],
    VERT_CS["unknown",
        VERT_DATUM["unknown",2005,
            EXTENSION["PROJ4_GRIDS","g2003conus.gtx,g2003alaska.gtx,g2003h01.gtx,g2003p01.gtx"]],
        UNIT["US survey foot",0.304800609601219,
            AUTHORITY["EPSG","9003"]],
        AXIS["Gravity-related height",UP]]]"""

    wkt = srs.ExportToPrettyWkt()
    # PROJ >= 9.0.1 returns 'Unknown based on GRS80 ellipsoid using towgs84=0,0,0,0,0,0,0'
    wkt = wkt.replace('Unknown based on GRS80 ellipsoid using towgs84=0,0,0,0,0,0,0', 'Unknown_based_on_GRS80_ellipsoid')
    wkt = wkt.replace('unknown using geoidgrids=g2003conus.gtx,g2003alaska.gtx,g2003h01.gtx,g2003p01.gtx', 'unknown')

    if gdaltest.equal_srs_from_wkt(exp_wkt, wkt) == 0:
        pytest.fail()
    elif exp_wkt != wkt:
        print('warning they are equivalent, but not completely the same')
        print(wkt)

    assert wkt.find('g2003conus.gtx') != -1, 'Did not get PROJ4_GRIDS EXTENSION node'

    exp_proj4 = '+proj=utm +zone=11 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +geoidgrids=g2003conus.gtx,g2003alaska.gtx,g2003h01.gtx,g2003p01.gtx +vunits=us-ft +no_defs'
    proj4 = srs.ExportToProj4()
    # PROJ >= 9.1 adds a ' +geoid_crs=WGS84'
    proj4 = proj4.replace(' +geoid_crs=WGS84', '')
    assert proj4 == exp_proj4, ('Did not get expected proj.4 string, got:' + proj4)

###############################################################################
# Test SetCompound()


def test_osr_compd_7():

    srs_horiz = osr.SpatialReference()
    srs_horiz.ImportFromEPSG(4326)

    srs_vert = osr.SpatialReference()
    srs_vert.ImportFromEPSG(5703)
    srs_vert.SetTargetLinearUnits('VERT_CS', 'foot', 0.304800609601219)

    srs = osr.SpatialReference()
    srs.SetCompoundCS('My Compound SRS', srs_horiz, srs_vert)

    assert srs.Validate() == 0, 'Does not validate'

    exp_wkt = """COMPD_CS["My Compound SRS",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    VERT_CS["NAVD88 height",
        VERT_DATUM["North American Vertical Datum 1988",2005,
            AUTHORITY["EPSG","5103"]],
        UNIT["foot",0.304800609601219],
        AXIS["Gravity-related height",UP]]]"""

    wkt = srs.ExportToPrettyWkt()

    if gdaltest.equal_srs_from_wkt(exp_wkt, wkt) == 0:
        pytest.fail()
    elif exp_wkt != wkt:
        print('warning they are equivalent, but not completely the same')
        print(wkt)

    assert srs.GetTargetLinearUnits('VERT_CS') == pytest.approx(0.304800609601219, 1e-15)

    assert srs.GetTargetLinearUnits(None) == pytest.approx(0.304800609601219, 1e-15)

###############################################################################
# Test ImportFromURN()


def test_osr_compd_8():

    srs = osr.SpatialReference()
    srs.SetFromUserInput('urn:ogc:def:crs,crs:EPSG::27700,crs:EPSG::5701')

    assert srs.Validate() == 0, 'Does not validate'

    wkt = srs.ExportToWkt()
    assert wkt.startswith('COMPD_CS'), 'COMPD_CS not recognised as compound.'


###############################################################################
# Test COMPD_CS with a VERT_DATUM type = 2002 (Ellipsoid height)

def test_osr_compd_vert_datum_2002():

    if osr.GetPROJVersionMajor() * 10000 + osr.GetPROJVersionMinor() * 100 < 70100:
        # Not supported before PROJ 7.1
        pytest.skip()

    sr = osr.SpatialReference()
    sr.SetFromUserInput('COMPD_CS["NAD83 / Alabama West + Ellipsoidal height",PROJCS["NAD83 / Alabama West",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",30],PARAMETER["central_meridian",-87.5],PARAMETER["scale_factor",0.999933333],PARAMETER["false_easting",600000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["X",EAST],AXIS["Y",NORTH],AUTHORITY["EPSG","26930"]],VERT_CS["Ellipsoidal height",VERT_DATUM["Ellipsoid",2002],UNIT["metre",1.0,AUTHORITY["EPSG","9001"]],AXIS["Up",UP]]]')
    assert sr.IsProjected()
    assert sr.GetAuthorityCode('PROJCS') == '26930'
    assert sr.GetAuthorityName('PROJCS') == 'EPSG'
    assert sr.GetAuthorityCode(None) is None
    assert sr.GetAuthorityName(None) is None
