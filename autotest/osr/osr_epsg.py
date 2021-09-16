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



from osgeo import ogr, osr
import gdaltest
import pytest

###############################################################################
# Verify that deprecated EPSG:26591 ends up picking non-deprecated EPSG:3003


def test_osr_epsg_1():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(26591)
    assert srs.GetAuthorityCode(None) == '3003'

###############################################################################
# Check that EPSG:4312 w.r.t towgs84 values

def test_osr_epsg_2():

    srs = osr.SpatialReference()
    with gdaltest.config_option('OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG', 'YES'):
        srs.ImportFromEPSG(4312)

    if float(srs.GetAttrValue('TOWGS84', 6)) != pytest.approx(2.4232, abs=0.0005):
        print(srs.ExportToPrettyWkt())
        pytest.fail('Wrong TOWGS84, override missed?')

###############################################################################
#   Check that EPSG:4326 is considered as lat/long


def test_osr_epsg_4():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    assert srs.EPSGTreatsAsLatLong(), 'supposed to be treated as lat/long'

    assert srs.ExportToWkt().find('AXIS') != -1, 'should  have AXIS node'

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

    assert srs.ExportToWkt().find('TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489]') == -1, \
        '# We do not expect a datum shift'

###############################################################################
#   Check working of EPSGTreatsAsNorthingEasting

@pytest.mark.parametrize('epsg_code,is_northing_easting',
                         [(2193, True), # NZGD2000 / New Zealand Transverse Mercator 2000
                          (32631, False), # WGS 84 / UTM zone 31N
                          (32661, True), # WGS 84 / UPS North (N,E)
                          (5041, False), # WGS 84 / UPS North (E,N)
                          (32761, True), # WGS 84 / UPS South (N,E)
                          (5042, False), # WGS 84 / UPS South (E,N)
                          (3031, False), # WGS 84 / Antarctic Polar Stereographic
                          (5482, True), # RSRGD2000 / RSPS2000
                          ]
                         )
def test_osr_epsg_treats_as_northing_easting(epsg_code, is_northing_easting):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(epsg_code)
    assert srs.EPSGTreatsAsNorthingEasting() == is_northing_easting


###############################################################################
#   Check EPSG:3857


def test_osr_epsg_9():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)

    assert srs.ExportToWkt() == 'PROJCS["WGS 84 / Pseudo-Mercator",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs"],AUTHORITY["EPSG","3857"]]'

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
    assert srs.IsSame(srs_ref) != 0, "%s vs %s" % (srs.ExportToPrettyWkt(), srs_ref.ExportToPrettyWkt())

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

    # The override is no longer in proj.db
    #assert srs.ExportToWkt().find('TOWGS84[570.8,85.7,462.8,4.998,1.587,5.261,3.56]') != -1, \
    #    'did not get expected TOWGS84'

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

    # One exact match
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["ETRS89 / UTM zone 32N (N-E)",
    GEOGCS["ETRS89",
        DATUM["European_Terrestrial_Reference_System_1989",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6258"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4258"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",9],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Northing",NORTH],
    AXIS["Easting",EAST]]""")
    matches = sr.FindMatches()
    assert len(matches) == 1 and matches[0][1] == 100
    assert matches[0][0].IsSame(sr)

    # Another one
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3044)
    sr.MorphToESRI()
    sr.SetFromUserInput(sr.ExportToWkt())
    matches = sr.FindMatches()
    assert len(matches) == 1 and matches[0][1] == 100
    assert not matches[0][0].IsSame(sr)

    # Two matches (and test GEOGCS)
    # This will now match with 4126 (which is deprecated), since the datum
    # is identified to 6126 and GetEPSGGeogCS has logic to subtract 2000 to it.
    #sr.SetFromUserInput("""GEOGCS["myLKS94",
    #DATUM["Lithuania_1994_ETRS89",
    #    SPHEROID["GRS 1980",6378137,298.257222101],
    #    TOWGS84[0,0,0,0,0,0,0]],
    #PRIMEM["Greenwich",0],
    #UNIT["degree",0.0174532925199433]]""")
    #matches = sr.FindMatches()
    #if len(matches) != 2:
    #    gdaltest.post_reason('fail')
    #    print(matches)
    #    return 'fail'
    #if matches[0][0].GetAuthorityCode(None) != '4126' or matches[0][1] != 90:
    #    gdaltest.post_reason('fail')
    #    print(matches)
    #    return 'fail'
    #if matches[1][0].GetAuthorityCode(None) != '4669' or matches[1][1] != 90:
    #    gdaltest.post_reason('fail')
    #    print(matches)
    #    return 'fail'

    # Very approximate matches
    sr.SetFromUserInput("""GEOGCS["myGEOGCS",
    DATUM["my_datum",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]
""")
    matches = sr.FindMatches()
    assert matches

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
    assert len(matches) == 1 and matches[0][1] == 25
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
    assert len(matches) == 1 and matches[0][1] == 25
    assert matches[0][0].IsSame(sr) != 1

###############################################################################


def test_osr_epsg_gcs_deprecated():

    sr = osr.SpatialReference()
    with gdaltest.config_option('OSR_USE_NON_DEPRECATED', 'NO'):
        sr.ImportFromEPSG(4268)
    assert sr.ExportToWkt().find('NAD27 Michigan (deprecated)') >= 0

###############################################################################


def test_osr_epsg_geoccs_deprecated():

    sr = osr.SpatialReference()
    with gdaltest.config_option('OSR_USE_NON_DEPRECATED', 'NO'):
        sr.ImportFromEPSG(4346)
    assert sr.ExportToWkt().find('ETRS89 (geocentric) (deprecated)') >= 0

###############################################################################


def test_osr_epsg_area_of_use():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2154)
    area = srs.GetAreaOfUse()
    assert area.west_lon_degree == -9.86
    assert area.south_lat_degree == 41.15
    assert area.east_lon_degree == 10.38
    assert area.north_lat_degree == 51.56
    assert 'France' in area.name

###############################################################################


def test_osr_GetCRSInfoListFromDatabase():

    l = osr.GetCRSInfoListFromDatabase('EPSG')
    found = False
    for record in l:
        if record.auth_name == 'EPSG' and record.code == '2154':
            assert 'Lambert-93' in record.name
            assert record.type == osr.OSR_CRS_TYPE_PROJECTED
            assert not record.deprecated
            assert record.bbox_valid
            assert record.west_lon_degree == -9.86
            assert record.south_lat_degree == 41.15
            assert record.east_lon_degree == 10.38
            assert record.north_lat_degree == 51.56
            assert 'France' in record.area_name
            assert record.projection_method == 'Lambert Conic Conformal (2SP)'
            found = True
    assert found

###############################################################################
#   Test AutoIdentifyEPSG() on NAD83(CORS96)


def test_osr_epsg_auto_identify_epsg_nad83_cors96():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCRS["NAD83(CORS96)",
    DATUM["NAD83 (Continuously Operating Reference Station 1996)",
        ELLIPSOID["GRS 1980",6378137,298.257222101,
            LENGTHUNIT["metre",1]]],
    PRIMEM["Greenwich",0,
        ANGLEUNIT["degree",0.0174532925199433]],
    CS[ellipsoidal,2],
        AXIS["geodetic latitude (Lat)",north,
            ORDER[1],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
            ORDER[2],
            ANGLEUNIT["degree",0.0174532925199433]]]""")
    srs.AutoIdentifyEPSG()
    assert srs.GetAuthorityCode(None) == '6783'


###############################################################################
#   Test AutoIdentifyEPSG() on a somewhat odd WKT


def test_osr_epsg_auto_identify_epsg_odd_wkt():

    # https://github.com/OSGeo/gdal/issues/3915

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""PROJCS["GDA94 / MGA zone 56 / AUSGeoid09_GDA94_V1",GEOGCS["GDA94 / MGA zone 56 / AUSGeoid09_GDA94_V1",DATUM["GDA94",SPHEROID["GRS 1980",6378137.000,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6283"]],PRIMEM["Greenwich",0.0000000000000000,AUTHORITY["EPSG","8901"]],UNIT["Degree",0.01745329251994329547,AUTHORITY["EPSG","9102"]],AUTHORITY["EPSG","28356"]],UNIT["Meter",1.00000000000000000000,AUTHORITY["EPSG","9001"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0.0000000000000000],PARAMETER["central_meridian",153.0000000000000000],PARAMETER["scale_factor",0.9996000000000000],PARAMETER["false_easting",500000.000],PARAMETER["false_northing",10000000.000],AXIS["Easting",EAST],AXIS["Northing",NORTH],AXIS["Height",UP],AUTHORITY["EPSG","28356"]]""")
    srs.AutoIdentifyEPSG()

###############################################################################
#   Test bugfix for https://github.com/OSGeo/gdal/issues/4038


def test_osr_epsg_auto_identify_epsg_projcrs_with_geogcrs_without_axis_roder():

    srs = osr.SpatialReference()
    assert srs.SetFromUserInput("""PROJCS["GDA_1994_MGA_Zone_55",GEOGCS["GCS_GDA_1994",DATUM["D_GDA_1994",SPHEROID["GRS_1980",6378137.0,298.257222101]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["False_Easting",500000.0],PARAMETER["False_Northing",10000000.0],PARAMETER["Central_Meridian",147.0],PARAMETER["Scale_Factor",0.9996],PARAMETER["Latitude_Of_Origin",0.0],UNIT["Meter",1.0]]""") == 0
    assert srs.AutoIdentifyEPSG() != ogr.OGRERR_NONE
    assert srs.CloneGeogCS().GetAuthorityCode(None) is None
