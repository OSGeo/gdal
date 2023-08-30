#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write round-tripping of SRS for HFA/Imagine format.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
import pytest

from osgeo import gdal, osr

###############################################################################
# Write a HFA/Imagine and read it back to check its SRS
crs_list = [
    [3814, False, False],  # tmerc
    [
        28991,
        False,
        True,
    ],  # sterea / Oblique Stereographic   # requires ESRI PE as we have no mapping for it
    # [2046, False, False],  # tmerc south oriented DISABLED. Not sure about the axis
    [3031, False, False],  # stere, "Polar Stereographic (variant B)"
    [
        5041,
        True,
        False,
    ],  # stere, "Polar Stereographic (variant A)". failure_expected because we ignore the scale factor
    [6931, False, False],  # laea
    [
        2062,
        False,
        True,
    ],  # lcc "Lambert Conic Conformal (1SP)". We morph it to LCC_2SP in the Imagine representation
    [3943, False, False],  # lcc "Lambert Conic Conformal (2SP)"
    # [2065, True, False],  # krovak South-West
    [5221, True, False],  # krovak east-north
    [2066, False, True],  # cass  # issue with Clarke's link unit
    [2964, False, False],  # aea
    [3410, False, False],  # cea
    [3786, False, False],  # eqc
    [3000, False, False],  # merc, "Mercator (variant A)" with scale_factor!=1
    [3832, False, False],  # merc, "Mercator (variant A)" with scale_factor==1
    [
        5641,
        False,
        True,
    ],  # merc, "Mercator (variant B)". We morph it to Mercator Variant A in the Imagine representation
    [27200, False, False],  # nzmg
    [6842, False, False],  # omerc, "Hotine Oblique Mercator (variant A)"
    [2057, False, False],  # omerc, "Hotine Oblique Mercator (variant B)"
    [29100, False, False],  # poly
    [2056, False, False],  # somerc
    [2027, False, False],  # utm
    [4326, False, False],  # longlat
    ["ESRI:54049", False, False],  # Vertical Perspective
    [5472, False, False],  # Polyconic
    ["ESRI:102010", False, False],  # eqdc
    ["ESRI:102237", False, False],  # aeqd
    ["ESRI:102034", False, False],  # Gnomonic
    [
        "ESRI:102035",
        True,
        False,
    ],  # Orthographic  # failure_expected because we return Orthographic whereas input is Orthographic (Spherical)
    [
        "+proj=ortho +lat_0=90 +lon_0=1 +x_0=2 +y_0=3 +datum=WGS84 +units=m +no_defs",
        False,
        False,
    ],  # Orthographic
    ["ESRI:102011", False, False],  # Sinusoidal
    ["ESRI:53003", False, False],  # Miller
    ["ESRI:53029", False, False],  # Van Der Grinten
    ["ESRI:53030", False, False],  # Robinson
    ["ESRI:53009", False, False],  # Mollweide
    ["ESRI:53010", False, False],  # Sphere_Eckert_VI
    ["ESRI:53011", False, False],  # Sphere_Eckert_V
    ["ESRI:53012", False, False],  # Sphere_Eckert_IV
    ["ESRI:53013", False, False],  # Sphere_Eckert_III
    ["ESRI:53014", False, False],  # Sphere_Eckert_II
    ["ESRI:53015", False, False],  # Sphere_Eckert_I
    ["ESRI:53016", False, False],  # Sphere_Gall_Stereographic
    [28191, False, False],  # Cassini Soldner
    ["ESRI:53031", False, False],  # Two_Point_Equidistant
    ["ESRI:102163", False, False],  # Bonne
    ["ESRI:53023", False, False],  # Sphere_Loximuthal
    ["ESRI:53022", False, False],  # Sphere_Quartic_Authalic
    ["ESRI:53018", False, False],  # Sphere_Winkel_I
    ["ESRI:53019", False, False],  # Sphere_Winkel_II
    [
        "ESRI:102421",
        True,
        False,
    ],  # Equidistant Cylindrical   failure_expected because we return Equidistant Cylindrical whereas input is Equidistant Cylindrical (Spherical)
    [
        "+proj=eqc +lat_ts=22.94791772 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs",
        False,
        False,
    ],  # Same as above but really with Equidistant Cylindrical
    ["ESRI:53043", False, False],  # Sphere_Aitoff
    ["ESRI:53046", False, False],  # Sphere_Craster_Parabolic
    ["ESRI:53045", False, False],  # Flat_Polar_Quartic
    ["ESRI:53048", False, False],  # Sphere_Times
    ["ESRI:53042", False, False],  # Sphere_Winkel_Tripel_NGS
    ["ESRI:53044", False, False],  # Sphere_Hammer_Aitoff
    [
        "ESRI:53025",
        False,
        False,
    ],  # Sphere_Hotine, Hotine_Oblique_Mercator_Two_Point_Natural_Origin
]


@pytest.mark.parametrize(
    "srs_def,failure_expected,read_with_pe_string",
    crs_list,
    ids=[str(r[0]) for r in crs_list],
)
def test_hfa_srs(srs_def, failure_expected, read_with_pe_string):
    sr = osr.SpatialReference()
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    if isinstance(srs_def, str):
        sr.SetFromUserInput(srs_def)
    else:
        sr.ImportFromEPSG(srs_def)

    ds = gdal.GetDriverByName("HFA").Create("/vsimem/TestHFASRS.img", 1, 1)
    ds.SetSpatialRef(sr)
    ds = None

    with gdaltest.config_options(
        {} if read_with_pe_string else {"HFA_USE_ESRI_PE_STRING": "NO"}
    ):
        ds = gdal.Open("/vsimem/TestHFASRS.img")
        sr2 = ds.GetSpatialRef()
        ds = None

    gdal.Unlink("/vsimem/TestHFASRS.img")

    # For EPSG:2065. Those 2 datums are translated into D_S_JTSK in ESRI WKT... So for the purpose of
    # comparison, substitute one for another
    if (
        sr.ExportToWkt().find(
            '"System_Jednotne_Trigonometricke_Site_Katastralni_Ferro"'
        )
        != -1
        and sr2.ExportToWkt().find('"System_Jednotne_Trigonometricke_Site_Katastralni"')
        != -1
    ):
        wkt2 = sr2.ExportToWkt().replace(
            '"System_Jednotne_Trigonometricke_Site_Katastralni"',
            '"System_Jednotne_Trigonometricke_Site_Katastralni_Ferro"',
        )
        sr2.SetFromUserInput(wkt2)

    if (
        not isinstance(srs_def, str)
        and srs_def == 4326
        and sr2.GetAuthorityCode(None) != "4326"
    ) or sr.IsSame(sr2) != 1:

        if osr.GetPROJVersionMajor() * 100 + osr.GetPROJVersionMinor() < 630:
            if srs_def == "ESRI:54049":
                if (
                    sr2.ExportToProj4()
                    != "+proj=nsper +lat_0=0 +lon_0=0 +h=35800000 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs"
                ):
                    print(sr)
                    print(sr2)
                    print(sr2.ExportToProj4())
                    assert False, "did not get expected SRS"
                else:
                    return
            else:
                sr.ImportFromWkt(sr.ExportToWkt())
                sr2.ImportFromWkt(sr2.ExportToWkt())

        if sr.IsSame(sr2) != 1:
            if failure_expected:
                pytest.xfail(
                    "did not get expected SRS. known to be broken currently. FIXME!"
                )

            print(sr)
            print(sr2)
            assert False, "did not get expected SRS"


def test_hfa_srs_wisconsin_tmerc():

    ds = gdal.Open(
        "data/esri_103300_NAD_1983_HARN_WISCRS_Adams_County_Meters_transverse_mercator.img"
    )
    wkt = ds.GetProjectionRef()
    sr = osr.SpatialReference()
    sr.SetFromUserInput(wkt)
    assert sr.GetAuthorityCode(None) == "103300"


def test_hfa_srs_NAD83_UTM():
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(26915)

    ds = gdal.GetDriverByName("HFA").Create("/vsimem/TestHFASRS.img", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open("/vsimem/TestHFASRS.img")
    wkt = ds.GetProjectionRef()
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26915"
    ds = None

    gdal.Unlink("/vsimem/TestHFASRS.img")

    assert "TOWGS84" not in wkt


def test_hfa_srs_NAD83_CORS96_UTM():
    sr = osr.SpatialReference()
    sr.SetFromUserInput(
        'PROJCS["NAD_1983_CORS96_UTM_Zone_11N",GEOGCS["NAD83(CORS96)",DATUM["NAD83_Continuously_Operating_Reference_Station_1996",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","1133"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","6783"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["ESRI","102411"]]'
    )

    ds = gdal.GetDriverByName("HFA").Create("/vsimem/TestHFASRS.img", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open("/vsimem/TestHFASRS.img")
    srs_got = ds.GetSpatialRef()
    assert srs_got.GetAuthorityName(None) is None
    assert srs_got.IsSame(sr), srs_got.ExportToWkt()
    ds = None

    gdal.Unlink("/vsimem/TestHFASRS.img")


def test_hfa_srs_esri_54049_pe_string_only_broken():

    with gdal.quiet_errors():
        ds = gdal.Open("../gdrivers/data/hfa/esri_54049_pe_string_only_broken.img")
    assert gdal.GetLastErrorType() == gdal.CE_Warning
    srs_got = ds.GetSpatialRef()
    srs_ref = osr.SpatialReference()
    srs_ref.SetFromUserInput("ESRI:54049")
    assert srs_got.IsSame(srs_ref), srs_got.ExportToWkt()


def test_hfa_srs_DISABLEPESTRING():
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(7844)

    filename = "/vsimem/test_hfa_srs_DISABLEPESTRING.img"
    ds = gdal.GetDriverByName("HFA").Create(
        filename, 1, 1, options=["DISABLEPESTRING=YES"]
    )
    ds.SetSpatialRef(sr)
    ds = None

    ds = gdal.Open(filename)
    srs_got = ds.GetSpatialRef()
    # without DISABLEPESTRING, we'd get GCS_GDA2020
    assert srs_got.GetName() == "Geocentric_Datum_of_Australia_2020"
    ds = None

    gdal.Unlink(filename)
