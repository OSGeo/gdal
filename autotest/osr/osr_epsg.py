#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test aspects of EPSG code lookup.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import pytest

###############################################################################
# Verify that EPSG:26591 picks up the entry from the pcs.override.csv
# file with the adjusted central_meridian.


def test_osr_epsg_1():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(26591)

    if abs(srs.GetProjParm('central_meridian') - -3.4523333333333) > 0.000005:
        print(srs.ExportToPrettyWkt())
        pytest.fail('Wrong central meridian, override missed?')

    
###############################################################################
# Check that EPSG:4312 lookup has the towgs84 values set properly
# from gcs.override.csv.


def test_osr_epsg_2():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4312)

    if abs(float(srs.GetAttrValue('TOWGS84', 6)) -
           2.4232) > 0.0005:
        print(srs.ExportToPrettyWkt())
        pytest.fail('Wrong TOWGS84, override missed?')

    
###############################################################################
# Check that various EPSG lookups based on Pulvoko 1942 have the
# towgs84 values set properly (#3579)


def test_osr_epsg_3():

    for epsg in [3120, 2172, 2173, 2174, 2175, 3333, 3334, 3335, 3329, 3330, 3331, 3332, 3328, 4179]:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(epsg)

        expected_towgs84 = [33.4, -146.6, -76.3, -0.359, -0.053, 0.844, -0.84]

        for i in range(6):
            if abs(float(srs.GetAttrValue('TOWGS84', i)) -
                   expected_towgs84[i]) > 0.0005:
                print(srs.ExportToPrettyWkt())
                pytest.fail('For EPSG:%d. Wrong TOWGS84, override missed?' % epsg)

    
###############################################################################
#   Check that EPSG:4326 is *not* considered as lat/long (#3813)


def test_osr_epsg_4():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    assert not srs.EPSGTreatsAsLatLong(), 'not supposed to be treated as lat/long'

    assert srs.ExportToWkt().find('AXIS') == -1, 'should not have AXIS node'

###############################################################################
#   Check that EPSGA:4326 is considered as lat/long


def test_osr_epsg_5():

    srs = osr.SpatialReference()
    srs.ImportFromEPSGA(4326)

    assert srs.EPSGTreatsAsLatLong(), 'supposed to be treated as lat/long'

    assert srs.ExportToWkt().find('AXIS') != -1, 'should  have AXIS node'

###############################################################################
#   Test datum shift for OSGB 36


def test_osr_epsg_6():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4277)

    assert srs.ExportToWkt().find('TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489]') != -1, \
        'did not get expected TOWGS84'

###############################################################################
#   Check that EPSG:2193 is *not* considered as N/E


def test_osr_epsg_7():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2193)

    assert not srs.EPSGTreatsAsNorthingEasting(), 'not supposed to be treated as n/e'

    assert srs.ExportToWkt().find('AXIS') == -1, 'should not have AXIS node'

###############################################################################
#   Check that EPSGA:2193 is considered as N/E


def test_osr_epsg_8():

    srs = osr.SpatialReference()
    srs.ImportFromEPSGA(2193)

    assert srs.EPSGTreatsAsNorthingEasting(), 'supposed to be treated as n/e'

    assert srs.ExportToWkt().find('AXIS') != -1, 'should  have AXIS node'

###############################################################################
#   Check EPSG:3857


def test_osr_epsg_9():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)

    assert srs.ExportToWkt() == 'PROJCS["WGS 84 / Pseudo-Mercator",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["X",EAST],AXIS["Y",NORTH],EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs"],AUTHORITY["EPSG","3857"]]'

    assert srs.Validate() == 0, 'Does not validate'

###############################################################################
#   Test AutoIdentifyEPSG() on Polar Stereographic


def test_osr_epsg_10():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""PROJCS["PS         WGS84",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Polar_Stereographic"],
    PARAMETER["latitude_of_origin",-71],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]""")

    assert srs.AutoIdentifyEPSG() == 0

    assert srs.GetAuthorityCode(None) == '3031', srs.ExportToWkt()

    srs_ref = osr.SpatialReference()
    srs_ref.ImportFromEPSG(3031)
    assert srs.IsSame(srs_ref) != 0

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""PROJCS["PS         WGS84",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Polar_Stereographic"],
    PARAMETER["latitude_of_origin",71],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]""")

    assert srs.AutoIdentifyEPSG() == 0

    assert srs.GetAuthorityCode(None) == '3995', srs.ExportToWkt()

    srs_ref = osr.SpatialReference()
    srs_ref.ImportFromEPSG(3995)
    assert srs.IsSame(srs_ref) != 0

###############################################################################
# Test datum shift for EPSG:2065 (PCS based override)


def test_osr_epsg_11():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2065)

    assert srs.ExportToWkt().find('TOWGS84[570.8,85.7,462.8,4.998,1.587,5.261,3.56]') != -1, \
        'did not get expected TOWGS84'

###############################################################################
# Test IsSame() on SRS that differs only by their PROJ4 EXTENSION (besides
# different EPSG codes)


def test_osr_epsg_12():

    sr1 = osr.SpatialReference()
    sr1.ImportFromEPSG(3857)

    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(3395)

    assert not sr1.IsSame(sr2)

###############################################################################
# Test FindMatches()


def test_osr_epsg_13():

    # One exact match (and test PROJCS)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3044)
    sr.MorphToESRI()
    sr.MorphFromESRI()
    matches = sr.FindMatches()
    assert len(matches) == 1 and matches[0][1] == 100
    assert matches[0][0].IsSame(sr) != 0

    # Two matches (and test GEOGCS)
    sr.SetFromUserInput("""GEOGCS["myLKS94",
    DATUM["Lithuania_1994_ETRS89",
        SPHEROID["GRS 1980",6378137,298.257222101],
        TOWGS84[0,0,0,0,0,0,0]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]""")
    matches = sr.FindMatches()
    assert len(matches) == 2
    assert matches[0][0].GetAuthorityCode(None) == '4126' and matches[0][1] == 90
    assert matches[1][0].GetAuthorityCode(None) == '4669' and matches[1][1] == 90

    # Zero match
    sr.SetFromUserInput("""GEOGCS["myGEOGCS",
    DATUM["my_datum",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]
""")
    matches = sr.FindMatches()
    assert not matches

    # One single match, but not similar according to IsSame()
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["WGS 84 / UTM zone 32N",
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
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",9],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",999999999],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]
""")
    matches = sr.FindMatches()
    assert len(matches) == 1 and matches[0][1] == 50
    assert matches[0][0].IsSame(sr) != 1

    # WKT has EPSG code but the definition doesn't match with the official
    # one (namely linear units are different)
    # https://github.com/OSGeo/gdal/issues/990
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["NAD83 / Ohio North",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",41.7],
    PARAMETER["standard_parallel_2",40.43333333333333],
    PARAMETER["latitude_of_origin",39.66666666666666],
    PARAMETER["central_meridian",-82.5],
    PARAMETER["false_easting",1968503.937007874],
    PARAMETER["false_northing",0],
    UNIT["International Foot",0.3048,
        AUTHORITY["EPSG","9002"]],
    AXIS["X",EAST],
    AXIS["Y",NORTH],
    AUTHORITY["EPSG","32122"]]
""")
    matches = sr.FindMatches()
    assert len(matches) == 1 and matches[0][1] == 50
    assert matches[0][0].IsSame(sr) != 1

###############################################################################


def test_osr_epsg_gcs_deprecated():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4268)
    assert sr.ExportToWkt().find('NAD27 Michigan (deprecated)') >= 0

###############################################################################


def test_osr_epsg_geoccs_deprecated():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4346)
    assert sr.ExportToWkt().find('ETRS89 (geocentric) (deprecated)') >= 0

###############################################################################



