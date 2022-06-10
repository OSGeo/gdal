#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some PROJ.4 specific translation issues.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2014, Kyle Shannon <kyle at pobox dot com>
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

from osgeo import gdal, osr
import gdaltest
import pytest

###############################################################################
# Test the +k_0 flag works as well as +k when consuming PROJ.4 format.
# This is from Bugzilla bug 355.
#


def test_osr_proj4_1():

    srs = osr.SpatialReference()
    srs.ImportFromProj4('+proj=tmerc +lat_0=53.5000000000 +lon_0=-8.0000000000 +k_0=1.0000350000 +x_0=200000.0000000000 +y_0=250000.0000000000 +a=6377340.189000 +rf=299.324965 +towgs84=482.530,-130.596,564.557,-1.042,-0.214,-0.631,8.15')

    assert srs.GetProjParm(osr.SRS_PP_SCALE_FACTOR) == pytest.approx(1.000035, abs=0.0000005), \
        '+k_0 not supported on import from PROJ.4?'

###############################################################################
# Verify that we can import strings with parameter values that are exponents
# and contain a plus sign.  As per bug 355 in GDAL/OGR's bugzilla.
#


def test_osr_proj4_2():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=lcc +x_0=0.6096012192024384e+06 +y_0=0 +lon_0=90dw +lat_0=42dn +lat_1=44d4'n +lat_2=42d44'n +a=6378206.400000 +rf=294.978698 +nadgrids=conus,ntv1_can.dat +units=m")

    assert srs.GetProjParm(osr.SRS_PP_FALSE_EASTING) == pytest.approx(609601.219, abs=0.0005), \
        'Parsing exponents not supported?'

    if srs.Validate() != 0:
        print(srs.ExportToPrettyWkt())
        pytest.fail('does not validate')

###############################################################################
# Verify that unrecognized projections return an error, not those
# annoying ellipsoid-only results.
#


def test_osr_proj4_4():

    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=utm +zone=11 +datum=WGS84')
    srs.SetAttrValue('PROJCS|PROJECTION', 'FakeTransverseMercator')

    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        srs.ExportToProj4()
        gdal.PopErrorHandler()

    except RuntimeError:
        gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('Unsupported conversion method') != -1:
        return

    pytest.fail('unknown srs not handled properly')


###############################################################################
# Verify that prime meridians are preserved when round tripping. (#1940)
#


def test_osr_proj4_5():

    srs = osr.SpatialReference()

    input_p4 = '+proj=lcc +lat_1=46.8 +lat_0=46.8 +lon_0=0 +k_0=0.99987742 +x_0=600000 +y_0=2200000 +ellps=clrk80ign +pm=paris +towgs84=-168,-60,320,0,0,0,0 +units=m +no_defs'
    srs.ImportFromProj4(input_p4)

    assert float(srs.GetAttrValue('PRIMEM', 1)) == pytest.approx(2.3372291667, abs=0.00000001), \
        'prime meridian lost?'

    assert abs(srs.GetProjParm('central_meridian')) == 0.0, 'central meridian altered?'

    p4 = srs.ExportToProj4()

    if p4 != input_p4:
        gdaltest.post_reason('round trip via PROJ.4 damaged srs?')
        print(p4)
        return 'fail'


###############################################################################
# Confirm handling of non-zero latitude of origin mercator (#3026)
#


def test_osr_proj4_6():

    wkt = """PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",46.1333331],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",1000],
    PARAMETER["false_northing",2000],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""

    srs = osr.SpatialReference()
    srs.ImportFromWkt(wkt)
    proj4 = srs.ExportToProj4()

    expect_proj4 = '+proj=merc +lat_ts=46.1333331 +lon_0=0 +x_0=1000 +y_0=2000 +datum=WGS84 +units=m +no_defs'
    assert proj4 == expect_proj4


###############################################################################
# Confirm handling of somerc (#3032).
#


def test_osr_proj4_7():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(23700)

    proj4 = srs.ExportToProj4()
    assert '+proj=somerc +lat_0=47.1443937222222 +lon_0=19.0485717777778 +k_0=0.99993 +x_0=650000 +y_0=200000 +ellps=GRS67' in proj4
    expected = proj4

    srs.ImportFromProj4(proj4)
    proj4 = srs.ExportToProj4()
    assert proj4 == expected


###############################################################################
# Check EPSG:3857, confirm Google Mercator hackery.


def test_osr_proj4_8():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)

    proj4 = srs.ExportToProj4()
    expected = '+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs'
    assert proj4 == expected, 'did not get expected EPSG:3857 (google mercator) result.'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3785)

    proj4 = srs.ExportToProj4()
    assert proj4 == expected, 'did not get expected EPSG:3785 (google mercator) result.'

###############################################################################
# NAD27 is a bit special - make sure no towgs84 values come through.
#


def test_osr_proj4_9():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4267)

    proj4 = srs.ExportToProj4()
    expected = '+proj=longlat +datum=NAD27 +no_defs'
    assert proj4 == expected, 'did not get expected EPSG:4267 (NAD27)'

    srs = osr.SpatialReference()
    srs.SetFromUserInput('NAD27')

    proj4 = srs.ExportToProj4()
    assert proj4 == expected, 'did not get expected "NAD27"'

###############################################################################
# Does geocentric work okay?
#


def test_osr_proj4_10():

    srs = osr.SpatialReference()
    srs.ImportFromProj4('+proj=geocent +ellps=WGS84 +towgs84=0,0,0,0,0,0,0 ')

    wkt_expected = 'GEOCCS["unknown",DATUM["Unknown_based_on_WGS84_ellipsoid",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Geocentric X",OTHER],AXIS["Geocentric Y",OTHER],AXIS["Geocentric Z",NORTH]]'

    wkt = srs.ExportToWkt()
    # PROJ >= 9.0.1 returns 'Unknown based on WGS84 ellipsoid using towgs84=0,0,0,0,0,0,0'
    wkt = wkt.replace('Unknown based on WGS84 ellipsoid using towgs84=0,0,0,0,0,0,0', 'Unknown_based_on_WGS84_ellipsoid')

    assert gdaltest.equal_srs_from_wkt(wkt_expected, wkt), \
        'did not get expected wkt.'

    p4 = srs.ExportToProj4()
    srs2 = osr.SpatialReference()
    srs2.ImportFromProj4(p4)

    if not srs.IsSame(srs2):
        print(srs.ExportToPrettyWkt())
        print(srs2.ExportToPrettyWkt())
        pytest.fail('round trip via PROJ.4 damaged srs?')


###############################################################################
# Test round-tripping of all supported projection methods
#


def test_osr_proj4_11():

    proj4strlist = ['+proj=bonne +lat_1=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=cass +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=nzmg +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=cea +lat_ts=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=tmerc +lat_0=1 +lon_0=2 +k=5 +x_0=3 +y_0=4',
                    '+proj=utm +zone=31 +south',
                    '+proj=merc +lat_ts=45 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=merc +lon_0=2 +k=5 +x_0=3 +y_0=4',
                    '+proj=stere +lat_0=90 +lat_ts=90 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=stere +lat_0=-90 +lat_ts=-90 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=stere +lat_0=90 +lon_0=2 +k=0.99 +x_0=3 +y_0=4',
                    '+proj=sterea +lat_0=45 +lon_0=2 +k=2 +x_0=3 +y_0=4',
                    '+proj=stere +lat_0=1 +lon_0=2 +k=1 +x_0=3 +y_0=4',
                    '+proj=eqc +lat_ts=0 +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=gstmerc +lat_0=1 +lon_0=2 +k_0=5 +x_0=3 +y_0=4',
                    '+proj=gnom +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=ortho +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=laea +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=aeqd +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=eqdc +lat_0=1 +lon_0=2 +lat_1=-2 +lat_2=-1 +x_0=3 +y_0=4',
                    '+proj=mill +R_A +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=moll +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=eck1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=eck2 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=eck3 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=eck4 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=eck5 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=eck6 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=poly +lat_0=1 +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=aea +lat_0=1 +lon_0=2 +lat_1=-2 +lat_2=-1 +x_0=3 +y_0=4',
                    '+proj=robin +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=vandg +R_A +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=sinu +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=gall +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=goode +lon_0=2 +x_0=3 +y_0=4',
                    '+proj=igh +lon_0=0 +x_0=0 +y_0=0',
                    '+proj=geos +lon_0=2 +h=1 +x_0=3 +y_0=4',
                    '+proj=lcc +lat_1=1 +lat_0=1 +lon_0=2 +k_0=2 +x_0=3 +y_0=4',
                    '+proj=lcc +lat_0=60 +lon_0=2 +lat_1=-10 +lat_2=30 +x_0=3 +y_0=4',
                    '+proj=lcc +lat_0=-10 +lon_0=2 +lat_1=-10 +lat_2=30 +x_0=3 +y_0=4',
                    '+proj=omerc +lat_0=1 +lonc=2 +alpha=-1 +gamma=-2 +k=2.5 +x_0=3 +y_0=4',
                    '+proj=omerc +lat_0=1 +lat_1=3 +lon_1=2 +lat_2=5 +lon_2=4 +k=2.5 +x_0=3 +y_0=4',
                    '+proj=somerc +lat_0=1 +lon_0=2 +k_0=2 +x_0=3 +y_0=4',
                    '+proj=krovak +lat_0=1 +lon_0=2 +alpha=30.2881397222222 +k=2 +x_0=3 +y_0=4',
                    '+proj=imw_p +lon_0=2 +lat_1=-2 +lat_2=-1 +x_0=3 +y_0=4',
                    '+proj=wag1 +lon_0=0 +x_0=3 +y_0=4',
                    '+proj=wag2 +lon_0=0 +x_0=3 +y_0=4',
                    '+proj=wag3 +lat_ts=1 +lon_0=0 +x_0=3 +y_0=4',
                    '+proj=wag4 +lon_0=0 +x_0=3 +y_0=4',
                    '+proj=wag5 +lon_0=0 +x_0=3 +y_0=4',
                    '+proj=wag6 +lon_0=0 +x_0=3 +y_0=4',
                    '+proj=wag7 +lon_0=0 +x_0=3 +y_0=4',
                    '+proj=tpeqd +lat_1=1 +lon_1=2 +lat_2=3 +lon_2=4 +x_0=5 +y_0=6',

                    #'+proj=utm +zone=31 +south +ellps=WGS84 +units=us-ft +no_defs ',
                    #'+proj=utm +zone=31 +south +ellps=WGS84 +units=ft +no_defs ',
                    #'+proj=utm +zone=31 +south +ellps=WGS84 +units=yd +no_defs ',
                    #'+proj=utm +zone=31 +south +ellps=WGS84 +units=us-yd +no_defs ',

                    ['+proj=etmerc +lat_0=0 +lon_0=9 +k=0.9996 +units=m +x_0=500000 +datum=WGS84 +no_defs', '+proj=utm +zone=32 +datum=WGS84 +units=m +no_defs'],

                    '+proj=qsc +lat_0=0 +lon_0=0 +x_0=0 +y_0=0',
                    '+proj=sch +plat_0=1 +plon_0=2 +phdg_0=3 +h_0=4'
                   ]

    for input_ in proj4strlist:

        if isinstance(input_, list):
            proj4str, expected = input_
        else:
            proj4str = input_
            if proj4str.find("+no_defs") == -1:
                proj4str = proj4str + " +ellps=WGS84 +units=m +no_defs"
            expected = proj4str

        srs = osr.SpatialReference()
        srs.ImportFromProj4(proj4str)
        if srs.Validate() != 0:
            print(proj4str)
            print(srs.ExportToPrettyWkt())
            pytest.fail('does not validate')
        out = srs.ExportToProj4()

        assert out == expected, 'round trip via PROJ.4 failed'

###############################################################################
# Test importing +init=epsg:XXX
#


def test_osr_proj4_12():

    expect_wkt = """GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9108"]],
    AUTHORITY["EPSG","4326"]]"""

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+init=epsg:4326")
    wkt = srs.ExportToPrettyWkt()

    if not wkt.startswith("""GEOGCS["WGS 84"""):
        print('Got:%s' % wkt)
        print('Expected:%s' % expect_wkt)
        pytest.fail('Did not get expected result.')


###############################################################################
# Test error cases
#


def test_osr_proj4_13():

    proj4strlist = ['',
                    # None,
                    'foo',
                    '+a=5',
                    '+proj=foo',
                    '+proj=longlat +ellps=wgs72 +towgs84=3']

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    for proj4str in proj4strlist:
        srs = osr.SpatialReference()
        gdal.ErrorReset()
        if srs.ImportFromProj4(proj4str) == 0 and gdal.GetLastErrorMsg() == '':
            gdal.PopErrorHandler()
            print(proj4str)
            pytest.fail()

    gdal.PopErrorHandler()

###############################################################################
# Test etmerc (#4853)
#


def test_osr_proj4_14():

    proj4str = '+proj=etmerc +lat_0=1 +lon_0=2 +k=0.9996 +x_0=3 +y_0=4 +datum=WGS84 +nodefs'

    # Test importing etmerc
    srs = osr.SpatialReference()
    srs.ImportFromProj4(proj4str)
    wkt = srs.ExportToPrettyWkt()
    expect_wkt = """PROJCS["unknown",
    GEOGCS["unknown",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",1],
    PARAMETER["central_meridian",2],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",3],
    PARAMETER["false_northing",4],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    if wkt != expect_wkt:
        print('Got:%s' % wkt)
        print('Expected:%s' % expect_wkt)
        pytest.fail('Did not get expected result.')

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32600 + 32)

    # Test exporting standard Transverse_Mercator, without any particular option
    proj4str = srs.ExportToProj4()
    expect_proj4str = '+proj=utm +zone=32 +datum=WGS84 +units=m +no_defs'
    assert proj4str == expect_proj4str

    srs = osr.SpatialReference()
    proj4str = '+proj=etmerc +lat_0=1 +lon_0=2 +k=0.9996 +x_0=3 +y_0=4 +datum=WGS84 +units=m +no_defs'
    srs.ImportFromProj4(proj4str)

    # Test exporting standard Transverse_Mercator, with OSR_USE_APPROX_TMERC=YES
    with gdaltest.config_option('OSR_USE_APPROX_TMERC', 'YES'):
        got_proj4str = srs.ExportToProj4()
    gdal.SetConfigOption('OSR_USE_APPROX_TMERC', None)
    assert got_proj4str == '+proj=tmerc +approx +lat_0=1 +lon_0=2 +k=0.9996 +x_0=3 +y_0=4 +datum=WGS84 +units=m +no_defs'

    # Test exporting standard Transverse_Mercator, with OSR_USE_APPROX_TMERC=YES
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32600 + 32)

    with gdaltest.config_option('OSR_USE_APPROX_TMERC', 'YES'):
        got_proj4str = srs.ExportToProj4()
    assert got_proj4str == '+proj=utm +approx +zone=32 +datum=WGS84 +units=m +no_defs'

###############################################################################
# Test unit parsing
#


def test_osr_proj4_16():

    def almost(a, b):
        if a != pytest.approx(b, abs=0.000000000001):
            return False
        return True
    units = (('km', 1000.),
             ('m', 1.),
             ('dm', 1. / 10.),
             ('cm', 1. / 100.),
             ('mm', 1. / 1000.),
             ('kmi', 1852.0),
             ('in', 0.0254),
             ('ft', 0.3048),
             ('yd', 0.9144),
             ('mi', 1609.344),
             ('fath', 1.8288),
             ('ch', 20.1168),
             ('link', 0.201168),
             ('us-in', 1. / 39.37),
             ('us-ft', 0.304800609601219),
             ('us-yd', 0.914401828803658),
             ('us-ch', 20.11684023368047),
             ('us-mi', 1609.347218694437),
             ('ind-yd', 0.91439523),
             ('ind-ft', 0.30479841),
             ('ind-ch', 20.11669506))

    srs = osr.SpatialReference()
    for u in units:
        assert srs.ImportFromProj4('+proj=utm +zone=11 +datum=WGS84 +units=%s' % u[0]) == 0
        to_met = srs.GetLinearUnits()
        assert almost(to_met, u[1]), \
            ('Did not get expected units for %s: %.12f vs %.12f' % (u[0], u[1], to_met))

###############################################################################
# Test unit parsing for name assignment
#


def test_osr_proj4_17():

    units = (('km', 'kilometre'),
             ('m', 'metre'),
             ('dm', 'decimetre'),
             ('cm', 'centimetre'),
             ('mm', 'millimetre'),
             ('kmi', 'nautical mile'),
             ('in', 'inch'),
             ('ft', 'foot'),
             ('yd', 'yard'),
             ('mi', 'Statute mile'),
             ('fath', 'fathom'),
             ('ch', 'chain'),
             ('link', 'link'),
             ('us-in', 'US survey inch'),
             ('us-ft', 'US survey foot'),
             ('us-yd', 'US survey yard'),
             ('us-ch', 'US survey chain'),
             ('us-mi', 'US survey mile'),
             ('ind-yd', 'Indian yard (1937)'),
             ('ind-ft', 'Indian foot (1937)'),
             ('ind-ch', 'Indian chain'))

    srs = osr.SpatialReference()
    for u in units:
        assert srs.ImportFromProj4('+proj=utm +zone=11 +datum=WGS84 +units=%s' % u[0]) == 0
        unit_name = srs.GetLinearUnitsName()
        assert unit_name == u[1], \
            ('Did not get expected unit name: %s vs %s' % (str(u), str(unit_name)))

###############################################################################
# Test fix for #5511
#


def test_osr_proj4_18():

    for p in ['no_off', 'no_uoff']:
        srs = osr.SpatialReference()
        srs.ImportFromProj4('+proj=omerc +lat_0=57 +lonc=-133 +alpha=-36 +k=0.9999 +x_0=5000000 +y_0=-5000000 +%s +datum=NAD83 +units=m +no_defs' % p)
        if srs.Validate() != 0:
            print(srs.ExportToPrettyWkt())
            pytest.fail('does not validate')
        out = srs.ExportToProj4()
        proj4str = '+proj=omerc +no_uoff +lat_0=57 +lonc=-133 +alpha=-36 +gamma=-36 +k=0.9999 +x_0=5000000 +y_0=-5000000 +datum=NAD83 +units=m +no_defs'
        if out != proj4str:
            print(p)
            pytest.fail('round trip via PROJ.4 failed')


###############################################################################
# Test EXTENSION and AUTHORITY in DATUM


def test_osr_proj4_19():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=longlat +datum=WGS84 +nadgrids=@null")

    assert srs.ExportToWkt().find('DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],EXTENSION["PROJ4_GRIDS","@null"],AUTHORITY["EPSG","6326"]]') > 0

    if srs.Validate() != 0:
        print(srs.ExportToPrettyWkt())
        pytest.fail('does not validate')


###############################################################################
# Test EXTENSION in GEOGCS


def test_osr_proj4_20():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=longlat +lon_0=90")

    assert srs.ExportToWkt() == 'GEOGCS["unknown",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Longitude",EAST],AXIS["Latitude",NORTH],EXTENSION["PROJ4","+proj=longlat +lon_0=90"]]'

    if srs.Validate() != 0:
        print(srs.ExportToPrettyWkt())
        pytest.fail('does not validate')


###############################################################################
# Test importing datum other than WGS84, WGS72, NAD27 or NAD83


def test_osr_proj4_21():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=longlat +datum=nzgd49")

    got = srs.ExportToProj4()

    assert got.startswith('+proj=longlat +ellps=intl')

###############################################################################
# Test importing ellipsoid defined with +R


def test_osr_proj4_22():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=longlat +R=1")
    got = srs.ExportToProj4()

    assert got.startswith('+proj=longlat +R=1')

###############################################################################
# Test importing ellipsoid defined with +a and +f


def test_osr_proj4_23():

    # +f=0 particular case
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=longlat +a=1 +f=0")
    got = srs.ExportToProj4()

    assert got.startswith('+proj=longlat +R=1')

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=longlat +a=2 +f=0.5")
    got = srs.ExportToProj4()

    assert got.startswith('+proj=longlat +a=2 +rf=2')

###############################################################################
# Test importing linear units defined with +to_meter


def test_osr_proj4_24():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +to_meter=1.0")
    got = srs.ExportToProj4()

    assert '+units=m' in got

    # Intl foot
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +to_meter=0.3048")
    got = srs.ExportToProj4()

    assert '+units=ft' in got

    # US foot
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +to_meter=0.3048006096012192")
    got = srs.ExportToProj4()

    assert '+units=us-ft' in got

    # unknown
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +to_meter=0.4")
    got = srs.ExportToProj4()

    assert '+to_meter=0.4' in got

###############################################################################
# Test importing linear units defined with +vto_meter


def test_osr_proj4_25():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +geoidgrids=foo +vto_meter=1.0")
    got = srs.ExportToProj4()

    assert '+vunits=m' in got

    # Intl foot
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +geoidgrids=foo +vto_meter=0.3048")
    got = srs.ExportToProj4()

    assert '+vunits=ft' in got

    # US foot
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +geoidgrids=foo +vto_meter=0.3048006096012192")
    got = srs.ExportToProj4()

    assert '+vunits=us-ft' in got

    # Unknown
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +geoidgrids=foo +vto_meter=0.4")
    got = srs.ExportToProj4()

    assert '+vto_meter=0.4' in got

###############################################################################
# Test importing linear units defined with +vunits


def test_osr_proj4_26():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +geoidgrids=foo +vunits=m")
    got = srs.ExportToProj4()

    assert '+vunits=m' in got

    # Intl foot
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +geoidgrids=foo +vunits=ft")
    got = srs.ExportToProj4()

    assert '+vunits=ft' in got

    # US yard
    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=merc +geoidgrids=foo +vunits=us-yd")
    got = srs.ExportToProj4()

    assert '+vunits=us-yd' in got

###############################################################################
# Test geostationary +sweep (#6030)


def test_osr_proj4_27():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+proj=geos +sweep=x +lon_0=0 +h=35785831 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs")
    got = srs.ExportToProj4()

    assert '+proj=geos +sweep=x +lon_0=0 +h=35785831 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs' in got

###############################################################################
# Test importing +init=epsg: with an override


def test_osr_proj4_28():

    srs = osr.SpatialReference()
    srs.ImportFromProj4("+init=epsg:32631 +units=cm")
    got = srs.ExportToWkt()

    assert '32631' not in got
    assert 'Transverse_Mercator' in got
    assert 'UNIT["centimetre",0.01' in got


def test_osr_proj4_error_cases_export_mercator():

    srs = osr.SpatialReference()

    # latitude_of_origin != 0.0 and scale != 1.0
    srs.SetFromUserInput('WGS84')
    srs.SetMercator(30.0, 0.0, 0.99, 0.0, 0.0)
    srs.SetLinearUnits('metre', 1)
    with gdaltest.error_handler():
        got = srs.ExportToProj4()
    assert got == ''

    # latitude_of_origin != 0.0

    srs = osr.SpatialReference()
    srs.SetFromUserInput('WGS84')
    srs.SetMercator2SP(0.0, 40.0, 0.0, 0.0, 0.0)
    srs.SetLinearUnits('metre', 1)
    with gdaltest.error_handler():
        got = srs.ExportToProj4()
    assert got == ''


def test_osr_unknown_member_id_in_datum_ensemble():

    if not(osr.GetPROJVersionMajor() > 6 or osr.GetPROJVersionMinor() >= 2):
        pytest.skip()

    # Test workaround fix for https://github.com/OSGeo/PROJ/pull/3221

    projjson = '{"$schema":"https://proj.org/schemas/v0.4/projjson.schema.json","type":"GeographicCRS","name":"WGS 84","datum_ensemble":{"name":"World Geodetic System 1984 ensemble","members":[{"name":"World Geodetic System 1984 (Transit)","id":{"authority":"EPSG","code":1166}},{"name":"World Geodetic System 1984 (G730)","id":{"authority":"EPSG","code":1152}},{"name":"World Geodetic System 1984 (G873)","id":{"authority":"EPSG","code":1153}},{"name":"World Geodetic System 1984 (G1150)","id":{"authority":"EPSG","code":1154}},{"name":"World Geodetic System 1984 (G1674)","id":{"authority":"EPSG","code":1155}},{"name":"World Geodetic System 1984 (G1762)","id":{"authority":"EPSG","code":1156}},{"name":"World Geodetic System 1984 (G2139)","id":{"authority":"EPSG","code":1309}},{"name":"unknown datum","id":{"authority":"UNKNOW?","code":1234}}],"ellipsoid":{"name":"WGS 84","semi_major_axis":6378137,"inverse_flattening":298.257223563},"accuracy":"2.0","id":{"authority":"EPSG","code":6326}},"coordinate_system":{"subtype":"ellipsoidal","axis":[{"name":"Geodetic latitude","abbreviation":"Lat","direction":"north","unit":"degree"},{"name":"Geodetic longitude","abbreviation":"Lon","direction":"east","unit":"degree"}]},"scope":"Horizontal component of 3D system.","area":"World.","bbox":{"south_latitude":-90,"west_longitude":-180,"north_latitude":90,"east_longitude":180},"id":{"authority":"EPSG","code":4326}}'
    sr = osr.SpatialReference()
    assert sr.SetFromUserInput(projjson) == 0
