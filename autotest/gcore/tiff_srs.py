#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write round-tripping of SRS for GeoTIFF format.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Test fix for #4677:


def test_tiff_srs_without_linear_units():

    sr = osr.SpatialReference()
    sr.ImportFromProj4("+proj=vandg +datum=WGS84")

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_without_linear_units.tif", 1, 1
    )
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open("/vsimem/tiff_srs_without_linear_units.tif")
    wkt = ds.GetProjectionRef()
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink("/vsimem/tiff_srs_without_linear_units.tif")

    assert sr.IsSame(sr2) == 1, "did not get expected SRS"


###############################################################################
# Test writing a COMPDCS without VerticalCSType


def test_srs_write_compd_cs():

    sr = osr.SpatialReference()
    # EPSG:7400 without the Authority
    sr.SetFromUserInput("""COMPD_CS["unknown",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936265,
                AUTHORITY["EPSG","7011"]],
            AUTHORITY["EPSG","6807"]],
        PRIMEM["Paris",2.5969213],
        UNIT["grad",0.01570796326794897],
        AUTHORITY["EPSG","4807"]],
    VERT_CS["NGF-IGN69 height",
        VERT_DATUM["Nivellement General de la France - IGN69",2005,
            AUTHORITY["EPSG","5119"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Up",UP]]]""")

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/tiff_srs_compd_cs.tif", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    with gdal.config_option("GTIFF_REPORT_COMPD_CS", "YES"):
        ds = gdal.Open("/vsimem/tiff_srs_compd_cs.tif")
        gdal.ErrorReset()
        wkt = ds.GetProjectionRef()
        assert gdal.GetLastErrorMsg() == ""

    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink("/vsimem/tiff_srs_compd_cs.tif")

    assert sr.IsSame(sr2) == 1, wkt


###############################################################################
# Test reading a COMPDCS without VerticalCSType


def test_srs_read_compd_cs():

    with gdal.config_option("GTIFF_REPORT_COMPD_CS", "YES"):
        ds = gdal.Open("data/vertcs_user_defined.tif")
        wkt = ds.GetProjectionRef()

    assert (
        wkt
        == 'COMPD_CS["NAD27 / UTM zone 11N + EGM2008 height",PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]],VERT_CS["EGM2008 height",VERT_DATUM["EGM2008 geoid",2005,AUTHORITY["EPSG","1027"]],UNIT["foot",0.3048,AUTHORITY["EPSG","9002"]],AXIS["Up",UP]]]'
    )


###############################################################################
# Test reading a GeoTIFF with both StdParallel1 and ScaleAtNatOrigin defined (#5791)


def test_tiff_srs_weird_mercator_2sp():

    ds = gdal.Open("data/weird_mercator_2sp.tif")
    with gdal.quiet_errors():
        wkt = ds.GetProjectionRef()
    assert gdal.GetLastErrorMsg() != "", "warning expected"
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    sr = osr.SpatialReference()
    # EPSG:7400 without the Authority
    sr.SetFromUserInput("""PROJCS["Global Mercator",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.2572221010002,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["standard_parallel_1",47.667],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]""")

    assert sr.IsSame(sr2) == 1, "did not get expected SRS"


###############################################################################
# Test reading ESRI WGS_1984_Web_Mercator_Auxiliary_Sphere


def test_tiff_srs_WGS_1984_Web_Mercator_Auxiliary_Sphere():

    ds = gdal.Open("data/WGS_1984_Web_Mercator_Auxiliary_Sphere.tif")
    wkt = ds.GetProjectionRef()
    sr = osr.SpatialReference()
    sr.SetFromUserInput(wkt)
    wkt = sr.ExportToPrettyWkt()
    ds = None

    assert (
        wkt
        == """PROJCS["WGS 84 / Pseudo-Mercator",
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
    PROJECTION["Mercator_1SP"],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH],
    EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs"]]"""
    )


###############################################################################
# Test reading ESRI:102113 WGS_1984_Web_Mercator


def test_tiff_srs_WGS_1984_Web_Mercator():

    ds = gdal.Open("data/WGS_1984_Web_Mercator.tif")
    sr = ds.GetSpatialRef()
    ds = None

    assert sr.GetAuthorityName(None) == "ESRI"
    assert sr.GetAuthorityCode(None) == "102113"


###############################################################################
# Test writing and reading various angular units


def test_tiff_srs_angular_units():

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_angular_units.tif", 1, 1
    )
    ds.SetProjection("""GEOGCS["WGS 84 (arc-second)",
    DATUM["WGS_1984 (arc-second)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["arc-second",4.848136811095361e-06]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_srs_angular_units.tif")
    wkt = ds.GetProjectionRef()
    assert 'UNIT["arc-second",4.84813681109536E-06' in wkt
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_angular_units.tif", 1, 1
    )
    ds.SetProjection("""GEOGCS["WGS 84 (arc-minute)",
    DATUM["WGS_1984 (arc-minute)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["arc-minute",0.0002908882086657216]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_srs_angular_units.tif")
    wkt = ds.GetProjectionRef()
    assert 'UNIT["arc-minute",0.000290888208665722]' in wkt
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_angular_units.tif", 1, 1
    )
    ds.SetProjection("""GEOGCS["WGS 84 (grad)",
    DATUM["WGS_1984 (grad)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["grad",0.01570796326794897]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_srs_angular_units.tif")
    wkt = ds.GetProjectionRef()
    assert 'UNIT["grad",0.015707963267949' in wkt
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_angular_units.tif", 1, 1
    )
    ds.SetProjection("""GEOGCS["WGS 84 (gon)",
    DATUM["WGS_1984 (gon)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["gon",0.01570796326794897]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_srs_angular_units.tif")
    wkt = ds.GetProjectionRef()
    assert 'UNIT["gon",0.015707963267949]' in wkt
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_angular_units.tif", 1, 1
    )
    ds.SetProjection("""GEOGCS["WGS 84 (radian)",
    DATUM["WGS_1984 (radian)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["radian",1]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_srs_angular_units.tif")
    wkt = ds.GetProjectionRef()
    assert 'UNIT["radian",1]' in wkt
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_angular_units.tif", 1, 1
    )
    ds.SetProjection("""GEOGCS["WGS 84 (custom)",
    DATUM["WGS_1984 (custom)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["custom",1.23]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_srs_angular_units.tif")
    wkt = ds.GetProjectionRef()
    assert 'UNIT["custom",1.23]' in wkt
    ds = None

    gdal.Unlink("/vsimem/tiff_srs_angular_units.tif")


###############################################################################
# Test writing and reading a unknown datum but with a known ellipsoid


def test_tiff_custom_datum_known_ellipsoid():

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_custom_datum_known_ellipsoid.tif", 1, 1
    )
    ds.SetProjection("""GEOGCS["WGS 84 based",
    DATUM["WGS_1984_based",
        SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]],
    PRIMEM["Greenwich",0],
    UNIT["degree",1]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_custom_datum_known_ellipsoid.tif")
    wkt = ds.GetProjectionRef()
    assert (
        wkt
        == 'GEOGCS["WGS 84 based",DATUM["WGS_1984_based",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'
    )
    ds = None

    gdal.Unlink("/vsimem/tiff_custom_datum_known_ellipsoid.tif")


###############################################################################
# Test reading a GeoTIFF file with only PCS set, but with a ProjLinearUnitsGeoKey
# override to another unit (us-feet) ... (#6210)


@pytest.mark.parametrize("gtiff_import_from_epsg", ("YES", "NO"))
def test_tiff_srs_epsg_2853_with_us_feet(gtiff_import_from_epsg):

    with gdal.config_option("GTIFF_IMPORT_FROM_EPSG", gtiff_import_from_epsg):
        ds = gdal.Open("data/epsg_2853_with_us_feet.tif")
    wkt = ds.GetProjectionRef()
    assert (
        'PARAMETER["false_easting",11482916.66' in wkt
        and 'UNIT["us_survey_feet",0.3048006' in wkt
        and "2853" not in wkt
    )


###############################################################################
# Test reading a SRS with a PCSCitationGeoKey = "LUnits = ..."


def test_tiff_srs_PCSCitationGeoKey_LUnits():

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_srs_PCSCitationGeoKey_LUnits.tif", 1, 1
    )
    ds.SetProjection("""PROJCS["UTM Zone 32, Northern Hemisphere",
    GEOGCS["GRS 1980(IUGG, 1980)",
        DATUM["unknown",
            SPHEROID["GRS80",6378137,298.257222101],
            TOWGS84[0,0,0,0,0,0,0]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",9],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",50000000],
    PARAMETER["false_northing",0],
    UNIT["Centimeter",0.01]]""")
    ds = None
    ds = gdal.Open("/vsimem/tiff_srs_PCSCitationGeoKey_LUnits.tif")
    wkt = ds.GetProjectionRef()
    assert (
        wkt
        == 'PROJCS["UTM Zone 32, Northern Hemisphere",GEOGCS["GRS 1980(IUGG, 1980)",DATUM["unknown",SPHEROID["GRS80",6378137,298.257222101],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",9],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",50000000],PARAMETER["false_northing",0],UNIT["Centimeter",0.01],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    )
    ds = None

    gdal.Unlink("/vsimem/tiff_srs_PCSCitationGeoKey_LUnits.tif")


###############################################################################
# Test reading a geotiff key ProjectionGeoKey (Short,1): Unknown-3856


def test_tiff_srs_projection_3856():

    ds = gdal.Open("data/projection_3856.tif")
    wkt = ds.GetProjectionRef()
    ds = None

    assert (
        'EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs'
        in wkt
    )


###############################################################################
# Test reading a geotiff with a LOCAL_CS and a Imagine citation


def test_tiff_srs_imagine_localcs_citation():

    ds = gdal.Open("data/imagine_localcs_citation.tif")
    wkt = ds.GetProjectionRef()
    ds = None

    assert wkt.startswith(
        'LOCAL_CS["Projection Name = UTM Units = meters GeoTIFF Units = meters"'
    )


###############################################################################
# Test reading a geotiff with a EPSG code and a TOWGS84 key that must
# override the default coming from EPSG


def test_tiff_srs_towgs84_override_OSR_STRIP_TOWGS84_NO():

    with gdaltest.config_option("OSR_STRIP_TOWGS84", "NO"):
        ds = gdal.Open("data/gtiff_towgs84_override.tif")
        wkt = ds.GetProjectionRef()
    ds = None

    assert "TOWGS84[584.8,67,400.3,0.105,0.013,-2.378,10.29]" in wkt, wkt


def test_tiff_srs_towgs84_override_OSR_STRIP_TOWGS84_default():

    ds = gdal.Open("data/gtiff_towgs84_override.tif")
    wkt = ds.GetProjectionRef()
    ds = None

    assert "TOWGS84" not in wkt


###############################################################################
# Test reading PCSCitationGeoKey (#7199)


def test_tiff_srs_pcscitation():

    ds = gdal.Open("data/pcscitation.tif")
    wkt = ds.GetProjectionRef()
    ds = None

    assert wkt.startswith('PROJCS["mycitation",')


###############################################################################
# Test reading file with ProjectedCSTypeGeoKey and GeographicTypeGeoKey


def test_tiff_srs_ProjectedCSTypeGeoKey_GeographicTypeGeoKey():

    ds = gdal.Open("data/utmsmall.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "26711"


def _test_tiff_srs(sr, expect_fail):
    """
    This is not a test by itself; it gets called by the tests below.
    """
    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/TestTiffSRS.tif", 1, 1)
    ds.SetSpatialRef(sr)
    ds = None

    # The GeoTIFF driver is smart enough to figure out that a CRS with
    # '+proj=longlat +datum=WGS84' is EPSG:4326
    if (
        sr.GetAuthorityCode(None) is None
        and "+proj=longlat +datum=WGS84" in sr.ExportToProj4()
    ):
        sr.ImportFromEPSG(4326)

    ds = gdal.Open("/vsimem/TestTiffSRS.tif")
    sr2 = ds.GetSpatialRef()
    if "Miller" in sr2.ExportToWkt():
        # Trick so that the EXTENSION node with a PROJ string including +R_A is added
        sr2.ImportFromProj4(sr2.ExportToProj4())
    ds = None

    gdal.Unlink("/vsimem/TestTiffSRS.tif")

    if sr.IsSame(sr2) != 1:
        if expect_fail:
            pytest.xfail(
                "did not get expected SRS. known to be broken currently. FIXME!"
            )

        # print(sr.ExportToWkt(['FORMAT=WKT2_2019']))
        # print(sr2.ExportToWkt(['FORMAT=WKT2_2019']))
        print(sr)
        print(sr2)
        assert False, "did not get expected SRS"
    else:
        if expect_fail:
            print("Succeeded but expected fail...")


###############################################################################
# Write a geotiff and read it back to check its SRS

epsg_list = [
    [3814, False],  # tmerc
    [28991, False],  # sterea
    # [2046, False],  # tmerc south oriented DISABLED. Not sure about the axis
    [3031, False],  # polar stere (ticket #3220)
    [3032, False],  # polar stere (ticket #3220)
    [32661, False],  # stere
    [3408, False],  # laea
    [2062, False],  # lcc 1SP
    # [2065, True],  # krovak South-West
    [5221, True],  # krovak east-north
    [2066, False],  # cass
    [2964, False],  # aea
    # [3410, False],  # cea spherical, method=9834. EPSG:3410 is now deprecated
    [6933, False],  # cea ellipsoidal, method=9835
    # [3786, False],  # eqc spherical, method=9823. EPSG:3786 is now deprecated
    [32663, False],  # eqc elliptical, method=9842
    [4087, False],  # eqc WGS 84 / World Equidistant Cylindrical method=1028
    [4088, False],  # eqc World Equidistant Cylindrical (Sphere) method=1029
    [2934, False],  # merc
    [27200, False],  # nzmg
    [2057, False],  # omerc Hotine_Oblique_Mercator_Azimuth_Center
    [3591, False],  # omerc Hotine_Oblique_Mercator
    [29100, False],  # poly
    [2056, False],  # somerc
    [2027, False],  # utm
    [4326, False],  # longlat
    [26943, False],  # lcc 2SP,
    [4328, False],  # geocentric
    [3994, False],  # mercator 2SP
    [26920, False],  # UTM NAD83 special case
    [26720, False],  # UTM NAD27 special case
    [32630, False],  # UTM WGS84 north special case
    [32730, False],  # UTM WGS84 south special case
    [22700, False],  # unknown datum 'Deir_ez_Zor'
    [3857, True],  # Web Mercator
    [102113, True],  # ESRI WGS_1984_Web_Mercator
]


@pytest.mark.parametrize("use_epsg_code", [0, 1])
@pytest.mark.parametrize(
    "epsg_code,epsg_proj4_broken",
    epsg_list,
    ids=[str(r[0]) for r in epsg_list],
)
def test_tiff_srs(use_epsg_code, epsg_code, epsg_proj4_broken):
    sr = osr.SpatialReference()
    if epsg_code > 32767:
        sr.SetFromUserInput("ESRI:" + str(epsg_code))
    else:
        sr.ImportFromEPSG(epsg_code)
        sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    expect_fail = False
    if use_epsg_code == 0:
        proj4str = sr.ExportToProj4()
        # print(proj4str)
        sr.SetFromUserInput(proj4str)
        expect_fail = epsg_proj4_broken

    _test_tiff_srs(sr, expect_fail)


@pytest.mark.parametrize(
    "proj4",
    [
        '+proj=eqdc +lat_0=%.16g +lon_0=%.16g +lat_1=%.16g +lat_2=%.16g" +x_0=%.16g +y_0=%.16g'
        % (1, 2, 3, 4, 5, 6),
        "+proj=mill +R_A +lon_0=2 +x_0=3 +y_0=4 +datum=WGS84 +units=m +no_defs",
        "+proj=gnom +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g" % (1, 2, 3, 4),
        "+proj=robin +lon_0=%.16g +x_0=%.16g +y_0=%.16g" % (1, 2, 3),
        "+proj=sinu +lon_0=%.16g +x_0=%.16g +y_0=%.16g" % (1, 2, 3),
    ],
    ids=[
        "eqdc",
        "mill",
        "gnom",
        "robin",
        "sinu",
    ],
)
def test_tiff_srs_proj4(proj4):
    sr = osr.SpatialReference()
    sr.SetFromUserInput(proj4)
    _test_tiff_srs(sr, False)


def _create_geotiff1_1_from_copy_and_compare(srcfilename, options=[]):
    if int(gdal.GetDriverByName("GTiff").GetMetadataItem("LIBGEOTIFF")) < 1600:
        pytest.skip("libgeotiff >= 1.6.0 required")

    src_ds = gdal.Open(srcfilename)
    tmpfile = "/vsimem/tmp.tif"
    gdal.GetDriverByName("GTiff").CreateCopy(
        tmpfile, src_ds, options=options + ["ENDIANNESS=LITTLE"]
    )
    f = gdal.VSIFOpenL(tmpfile, "rb")
    data = gdal.VSIFReadL(1, 100000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink(tmpfile)
    assert data == open(src_ds.GetDescription(), "rb").read()


def test_tiff_srs_read_epsg4326_geotiff1_1():
    ds = gdal.Open("data/epsg4326_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "4326"


def test_tiff_srs_write_epsg4326_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare(
        "data/epsg4326_geotiff1_1.tif", options=["GEOTIFF_VERSION=1.1"]
    )


def test_tiff_srs_read_epsg26711_geotiff1_1():
    ds = gdal.Open("data/epsg26711_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "26711"


def test_tiff_srs_write_epsg26711_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare(
        "data/epsg26711_geotiff1_1.tif", options=["GEOTIFF_VERSION=1.1"]
    )


def test_tiff_srs_read_epsg4326_3855_geotiff1_1():
    ds = gdal.Open("data/epsg4326_3855_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetName() == "WGS 84 + EGM2008 height"
    assert sr.GetAuthorityCode("COMPD_CS|GEOGCS") == "4326"
    assert sr.GetAuthorityCode("COMPD_CS|VERT_CS") == "3855"


def test_tiff_srs_write_epsg4326_3855_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare("data/epsg4326_3855_geotiff1_1.tif")


def test_tiff_srs_read_epsg4979_geotiff1_1():
    ds = gdal.Open("data/epsg4979_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "4979"


def test_tiff_srs_write_epsg4979_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare("data/epsg4979_geotiff1_1.tif")


def test_tiff_srs_write_epsg4937_etrs89_3D_geotiff1_1():
    if int(gdal.GetDriverByName("GTiff").GetMetadataItem("LIBGEOTIFF")) < 1600:
        pytest.skip()
    tmpfile = "/vsimem/tmp.tif"
    ds = gdal.GetDriverByName("GTiff").Create(tmpfile, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4937)
    ds.SetSpatialRef(sr)
    ds = None
    ds = gdal.Open(tmpfile)
    assert sr.GetName() == "ETRS89"
    assert sr.GetAuthorityCode(None) == "4937"
    ds = None
    gdal.Unlink(tmpfile)


# Deprecated way of conveying GeographicCRS 3D
def test_tiff_srs_read_epsg4326_5030_geotiff1_1():
    ds = gdal.Open("data/epsg4326_5030_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "4979"


def test_tiff_srs_read_epsg26711_3855_geotiff1_1():
    ds = gdal.Open("data/epsg26711_3855_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetName() == "NAD27 / UTM zone 11N + EGM2008 height"
    assert sr.GetAuthorityCode("COMPD_CS|PROJCS") == "26711"
    assert sr.GetAuthorityCode("COMPD_CS|VERT_CS") == "3855"


def test_tiff_srs_write_epsg26711_3855_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare("data/epsg26711_3855_geotiff1_1.tif")


# ProjectedCRS 3D not really defined yet in GeoTIFF 1.1, but this is
# a natural extension
def test_tiff_srs_read_epsg32631_4979_geotiff1_1():
    ds = gdal.Open("data/epsg32631_4979_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.IsProjected()
    assert sr.GetAxesCount() == 3
    assert sr.GetName() == "WGS 84 / UTM zone 31N"
    sr_geog = osr.SpatialReference()
    sr_geog.CopyGeogCSFrom(sr)
    assert sr_geog.GetAuthorityCode(None) == "4979"


def test_tiff_srs_write_vertical_perspective():

    if osr.GetPROJVersionMajor() * 100 + osr.GetPROJVersionMinor() < 700:
        pytest.skip("requires PROJ 7 or later")

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/src.tif", 1, 1)
    sr = osr.SpatialReference()
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 3000000, 0)
    sr.SetVerticalPerspective(1, 2, 0, 1000, 0, 0)
    gdal.ErrorReset()
    ds.SetSpatialRef(sr)
    assert gdal.GetLastErrorMsg() == ""
    ds = None

    src_ds = gdal.Open("/vsimem/src.tif")
    # First is PROJ 7
    assert src_ds.GetSpatialRef().ExportToProj4() in (
        "+proj=nsper +lat_0=1 +lon_0=2 +h=1000 +x_0=0 +y_0=0 +R=3000000 +units=m +no_defs",
        "+proj=nsper +R=3000000 +lat_0=1 +lon_0=2 +h=1000 +x_0=0 +y_0=0 +wktext +no_defs",
    )
    gdal.ErrorReset()
    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/dst.tif", src_ds)
    assert gdal.GetLastErrorMsg() == ""

    ds = gdal.Open("/vsimem/dst.tif")
    assert ds.GetSpatialRef().ExportToProj4() == src_ds.GetSpatialRef().ExportToProj4()

    src_ds = None
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/src.tif")
    gdal.GetDriverByName("GTiff").Delete("/vsimem/dst.tif")


def test_tiff_srs_write_ob_tran_eqc():

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/src.tif", 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromProj4(
        "+proj=ob_tran +o_proj=eqc +o_lon_p=-90 +o_lat_p=180 +lon_0=0 +R=3396190 +units=m +no_defs"
    )
    ds.SetSpatialRef(sr)
    ds = None

    assert gdal.VSIStatL("/vsimem/src.tif.aux.xml")

    ds = gdal.Open("/vsimem/src.tif")
    assert (
        ds.GetSpatialRef().ExportToProj4()
        == "+proj=ob_tran +o_proj=eqc +o_lon_p=-90 +o_lat_p=180 +lon_0=0 +R=3396190 +units=m +no_defs"
    )
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/src.tif")


def test_tiff_srs_towgs84_from_epsg_do_not_write_it():

    filename = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    srs_in = osr.SpatialReference()
    srs_in.ImportFromEPSG(31468)
    srs_in.AddGuessedTOWGS84()
    assert srs_in.HasTOWGS84()
    ds.SetSpatialRef(srs_in)
    ds = None

    ds = gdal.Open(filename)
    with gdaltest.config_option("OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG", "NO"):
        srs = ds.GetSpatialRef()
    assert not srs.HasTOWGS84()


def test_tiff_srs_towgs84_from_epsg_force_write_it():

    filename = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    srs_in = osr.SpatialReference()
    srs_in.ImportFromEPSG(31468)
    srs_in.AddGuessedTOWGS84()
    assert srs_in.HasTOWGS84()
    with gdaltest.config_option("GTIFF_WRITE_TOWGS84", "YES"):
        ds.SetSpatialRef(srs_in)
        ds = None

    with gdaltest.config_option("OSR_STRIP_TOWGS84", "NO"):
        ds = gdal.Open(filename)
        with gdaltest.config_option("OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG", "NO"):
            srs = ds.GetSpatialRef()
    assert srs.HasTOWGS84()


def test_tiff_srs_towgs84_with_epsg_code_but_non_default_TOWGS84():

    filename = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    srs_in = osr.SpatialReference()
    srs_in.SetFromUserInput("""PROJCS["DHDN / 3-degree Gauss-Kruger zone 4",
    GEOGCS["DHDN",
        DATUM["Deutsches_Hauptdreiecksnetz",
            SPHEROID["Bessel 1841",6377397.155,299.1528128,
                AUTHORITY["EPSG","7004"]],
            TOWGS84[1,2,3,4,5,6,7],
            AUTHORITY["EPSG","6314"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4314"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",12],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",4500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["Northing",NORTH],
    AXIS["Easting",EAST],
    AUTHORITY["EPSG","31468"]]""")
    ds.SetSpatialRef(srs_in)
    ds = None

    with gdaltest.config_option("OSR_STRIP_TOWGS84", "NO"):
        ds = gdal.Open(filename)
        srs = ds.GetSpatialRef()
    assert srs.GetTOWGS84() == (1, 2, 3, 4, 5, 6, 7)


def test_tiff_srs_write_epsg3857():
    tmpfile = "/vsimem/tmp.tif"
    ds = gdal.GetDriverByName("GTiff").Create(tmpfile, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    ds.SetSpatialRef(sr)
    ds = None
    ds = gdal.Open(tmpfile)
    assert sr.GetName() == "WGS 84 / Pseudo-Mercator"
    assert sr.GetAuthorityCode(None) == "3857"
    f = gdal.VSIFOpenL(tmpfile, "rb")
    data = gdal.VSIFReadL(1, 100000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink(tmpfile)
    assert b"ESRI PE String" not in data


def test_tiff_srs_read_epsg26730_with_linear_units_set():
    ds = gdal.Open("data/epsg26730_with_linear_units_set.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "26730"


def test_tiff_srs_read_user_defined_geokeys():
    if int(gdal.GetDriverByName("GTiff").GetMetadataItem("LIBGEOTIFF")) < 1600:
        pytest.skip()

    gdal.ErrorReset()
    ds = gdal.Open("data/byte_user_defined_geokeys.tif")
    sr = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == ""
    assert sr is not None


def test_tiff_srs_read_compoundcrs_without_gtcitation():
    if int(gdal.GetDriverByName("GTiff").GetMetadataItem("LIBGEOTIFF")) < 1600:
        pytest.skip()

    ds = gdal.Open("data/gtiff/compdcrs_without_gtcitation.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetName() == "WGS 84 / UTM zone 32N + EGM08_Geoid"


def test_tiff_srs_read_getspatialref_getgcpspatialref():

    ds = gdal.Open("data/byte.tif")
    assert ds.GetSpatialRef() is not None
    assert ds.GetGCPSpatialRef() is None

    ds = gdal.Open("data/byte.tif")
    assert ds.GetGCPSpatialRef() is None
    assert ds.GetSpatialRef() is not None

    ds = gdal.Open("data/byte.tif")
    assert ds.GetSpatialRef() is not None
    assert ds.GetSpatialRef() is not None
    assert ds.GetGCPSpatialRef() is None
    assert ds.GetGCPSpatialRef() is None

    ds = gdal.Open("data/byte_gcp_pixelispoint.tif")
    assert ds.GetSpatialRef() is None
    assert ds.GetGCPSpatialRef() is not None

    ds = gdal.Open("data/byte_gcp_pixelispoint.tif")
    assert ds.GetGCPSpatialRef() is not None
    assert ds.GetSpatialRef() is None

    ds = gdal.Open("data/byte_gcp_pixelispoint.tif")
    assert ds.GetGCPSpatialRef() is not None
    assert ds.GetGCPSpatialRef() is not None
    assert ds.GetSpatialRef() is None
    assert ds.GetSpatialRef() is None


def test_tiff_srs_read_VerticalUnitsGeoKey_private_range():
    ds = gdal.Open("data/gtiff/VerticalUnitsGeoKey_private_range.tif")
    with gdal.quiet_errors():
        sr = ds.GetSpatialRef()
    assert sr.GetName() == "NAD83 / UTM zone 16N"
    assert gdal.GetLastErrorMsg() != ""


def test_tiff_srs_read_invalid_semimajoraxis_compound():
    ds = gdal.Open("data/gtiff/invalid_semimajoraxis_compound.tif")
    # Check that it doesn't crash. PROJ >= 8.2.0 will return a NULL CRS
    # whereas previous versions will return a non-NULL one
    with gdal.quiet_errors():
        ds.GetSpatialRef()


def test_tiff_srs_try_write_derived_geographic():

    if osr.GetPROJVersionMajor() < 7:
        pytest.skip()

    tmpfile = "/vsimem/tmp.tif"
    ds = gdal.GetDriverByName("GTiff").Create(tmpfile, 1, 1)
    wkt = 'GEOGCRS["Coordinate System imported from GRIB file",BASEGEOGCRS["Coordinate System imported from GRIB file",DATUM["unnamed",ELLIPSOID["Sphere",6367470,0,LENGTHUNIT["metre",1,ID["EPSG",9001]]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]]],DERIVINGCONVERSION["Pole rotation (GRIB convention)",METHOD["Pole rotation (GRIB convention)"],PARAMETER["Latitude of the southern pole (GRIB convention)",-30,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]],PARAMETER["Longitude of the southern pole (GRIB convention)",-15,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]],PARAMETER["Axis rotation (GRIB convention)",0,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]]],CS[ellipsoidal,2],AXIS["latitude",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]],AXIS["longitude",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]]]'
    ds.SetProjection(wkt)
    ds = None

    assert gdal.VSIStatL(tmpfile + ".aux.xml")
    ds = gdal.Open(tmpfile)
    srs = ds.GetSpatialRef()
    assert srs is not None
    assert srs.IsDerivedGeographic()
    ds = None

    gdal.Unlink(tmpfile + ".aux.xml")
    ds = gdal.Open(tmpfile)
    assert ds.GetSpatialRef() is None
    ds = None

    gdal.Unlink(tmpfile)


def test_tiff_srs_read_GeogGeodeticDatumGeoKey_reserved_range():
    ds = gdal.Open("data/gtiff/GeogGeodeticDatumGeoKey_reserved.tif")
    with gdal.quiet_errors():
        sr = ds.GetSpatialRef()
    assert sr.GetName() == "WGS 84 / Pseudo-Mercator"
    assert gdal.GetLastErrorMsg() != ""
    assert gdal.GetLastErrorType() == gdal.CE_Warning


def test_tiff_srs_read_buggy_sentinel1_ellipsoid_code_4326():
    # That file has GeogEllipsoidGeoKey=4326, instead of 7030
    ds = gdal.Open("data/gtiff/buggy_sentinel1_ellipsoid_code_4326.tif")
    sr = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == ""
    assert sr.GetAuthorityCode("GEOGCS|DATUM|SPHEROID") == "7030"


def test_tiff_srs_read_invalid_GeogAngularUnitSizeGeoKey():
    # That file has GeogAngularUnitSizeGeoKey = 0
    ds = gdal.Open("data/gtiff/invalid_GeogAngularUnitSizeGeoKey.tif")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() != ""


def test_tiff_srs_read_inconsistent_invflattening():
    # That file has GeogSemiMinorAxisGeoKey / GeogInvFlatteningGeoKey values
    # which are inconsistent with the ones from the ellipsoid of the datum
    ds = gdal.Open("data/gtiff/inconsistent_invflattening.tif")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() != ""
    assert srs.GetAuthorityCode(None) == "28992"
    assert srs.GetAuthorityCode("GEOGCS") == "4289"
    assert srs.GetInvFlattening() == pytest.approx(
        299.1528131, abs=1e-7
    )  #  wrong value w.r.t Bessel 1841 official definition

    ds = gdal.Open("data/gtiff/inconsistent_invflattening.tif")
    gdal.ErrorReset()
    with gdaltest.config_option("GTIFF_SRS_SOURCE", "GEOKEYS"):
        srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == ""
    assert srs.GetAuthorityCode(None) is None
    assert srs.GetAuthorityCode("GEOGCS") is None
    assert srs.GetInvFlattening() == pytest.approx(
        299.1528131, abs=1e-7
    )  #  wrong value w.r.t Bessel 1841 official definition

    ds = gdal.Open("data/gtiff/inconsistent_invflattening.tif")
    gdal.ErrorReset()
    with gdaltest.config_option("GTIFF_SRS_SOURCE", "EPSG"):
        srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == ""
    assert srs.GetAuthorityCode(None) == "28992"
    assert srs.GetAuthorityCode("GEOGCS") == "4289"
    assert srs.GetInvFlattening() == pytest.approx(
        299.1528128, abs=1e-7
    )  #  Bessel 1841 official definition


def test_tiff_srs_dynamic_geodetic_crs():

    if osr.GetPROJVersionMajor() < 8:
        pytest.skip()

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(8999)  # ITRF2008
    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/test_tiff_srs_dynamic_geodetic_crs.tif", 1, 1
    )
    ds.SetSpatialRef(srs)
    ds = None
    ds = gdal.Open("/vsimem/test_tiff_srs_dynamic_geodetic_crs.tif")
    gdal.ErrorReset()
    srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == "", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert srs.GetAuthorityCode(None) == "8999"
    assert srs.IsDynamic()
    ds = None
    gdal.Unlink("/vsimem/test_tiff_srs_dynamic_geodetic_crs.tif")


@pytest.mark.parametrize("geotiff_version", ["1.0", "1.1"])
def test_tiff_srs_geographic_crs_3D(geotiff_version):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4959)  # NZGD2000 3D
    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/test_tiff_srs_geographic_crs_3D.tif",
        1,
        1,
        options=["GEOTIFF_VERSION=" + geotiff_version],
    )
    ds.SetSpatialRef(srs)
    ds = None
    ds = gdal.Open("/vsimem/test_tiff_srs_geographic_crs_3D.tif")
    gdal.ErrorReset()
    srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == "", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    if geotiff_version == "1.1":
        assert srs.GetAuthorityCode(None) == "4959"
    ds = None
    gdal.Unlink("/vsimem/test_tiff_srs_geographic_crs_3D.tif")


def test_tiff_srs_datum_name_with_space():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4312)  # MGI with datum name = 'Militar-Geographische Institut""
    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/test_tiff_srs_datum_name_with_space.tif", 1, 1
    )
    ds.SetSpatialRef(srs)
    ds = None
    ds = gdal.Open("/vsimem/test_tiff_srs_datum_name_with_space.tif")
    gdal.ErrorReset()
    srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == "", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert srs.GetAuthorityCode(None) == "4312"
    ds = None
    gdal.Unlink("/vsimem/test_tiff_srs_datum_name_with_space.tif")


def test_tiff_srs_compound_crs_with_local_cs():

    if int(gdal.GetDriverByName("GTiff").GetMetadataItem("LIBGEOTIFF")) < 1600:
        pytest.skip("libgeotiff >= 1.6 required")

    filename = "/vsimem/test_tiff_srs_compound_crs_with_local_cs.tif"
    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        'COMPD_CS["COMPD_CS_name",LOCAL_CS["None",LOCAL_DATUM["None",32767],UNIT["Foot_US",0.304800609601219],AXIS["None",OTHER]],VERT_CS["VERT_CS_Name",VERT_DATUM["Local",2005],UNIT["Meter",1,AUTHORITY["EPSG","9001"]],AXIS["Gravity-related height",UP]]]'
    )
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    assert srs.IsCompound()
    assert srs.GetLinearUnits() == pytest.approx(0.304800609601219)
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ""

    ds = gdal.Open(filename)
    gdal.ErrorReset()
    srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == "", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert srs.GetName() == "COMPD_CS_name", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert srs.IsCompound(), srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert srs.GetLinearUnits() == pytest.approx(0.304800609601219), srs.ExportToWkt(
        ["FORMAT=WKT2_2019"]
    )
    assert srs.GetAttrValue("COMPD_CS|VERT_CS") == "VERT_CS_Name", srs.ExportToWkt(
        ["FORMAT=WKT2_2019"]
    )
    assert srs.GetAttrValue("COMPD_CS|VERT_CS|UNIT") in (
        "Meter",
        "metre",
    ), srs.ExportToWkt(["FORMAT=WKT2_2019"])
    ds = None
    gdal.Unlink(filename)


def test_tiff_srs_read_esri_pcs_gcs_ellipsoid_names():

    ds = gdal.Open("data/gtiff/esri_pcs_gcs_ellipsoid_names.tif")
    srs = ds.GetSpatialRef()
    # Check that the names of the SRS components are morphed from ESRI names
    # to EPSG WKT2 names
    wkt = srs.ExportToWkt(["FORMAT=WKT2"])
    assert 'PROJCRS["RT90 5 gon O"' in wkt
    assert 'BASEGEOGCRS["RT90"' in wkt
    assert 'DATUM["Rikets koordinatsystem 1990"' in wkt
    assert 'ELLIPSOID["Bessel 1841"' in wkt


@pytest.mark.require_proj(9, 0)
def test_tiff_srs_write_projected_3d():

    filename = "/vsimem/test_tiff_srs_write_projected_3d.tif"
    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        'BOUNDCRS[SOURCECRS[PROJCRS["unknown",BASEGEOGCRS["unknown",DATUM["Unknown based on Bessel 1841 ellipsoid",ELLIPSOID["Bessel 1841",6377397.155,299.1528128,LENGTHUNIT["metre",1,ID["EPSG",9001]]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8901]]],CONVERSION["unknown",METHOD["Oblique Stereographic",ID["EPSG",9809]],PARAMETER["Latitude of natural origin",52.1561605555556,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8801]],PARAMETER["Longitude of natural origin",5.38763888888889,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8802]],PARAMETER["Scale factor at natural origin",0.9999079,SCALEUNIT["unity",1],ID["EPSG",8805]],PARAMETER["False easting",155000,LENGTHUNIT["metre",1],ID["EPSG",8806]],PARAMETER["False northing",463000,LENGTHUNIT["metre",1],ID["EPSG",8807]]],CS[Cartesian,3],AXIS["(E)",east,ORDER[1],LENGTHUNIT["metre",1,ID["EPSG",9001]]],AXIS["(N)",north,ORDER[2],LENGTHUNIT["metre",1,ID["EPSG",9001]]],AXIS["ellipsoidal height (h)",up,ORDER[3],LENGTHUNIT["metre",1,ID["EPSG",9001]]]]],TARGETCRS[GEOGCRS["WGS 84",DATUM["World Geodetic System 1984",ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],CS[ellipsoidal,2],AXIS["latitude",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433]],AXIS["longitude",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433]],ID["EPSG",4326]]],ABRIDGEDTRANSFORMATION["Transformation from unknown to WGS84",METHOD["Position Vector transformation (geog3D domain)",ID["EPSG",1037]],PARAMETER["X-axis translation",565.2369,ID["EPSG",8605]],PARAMETER["Y-axis translation",50.0087,ID["EPSG",8606]],PARAMETER["Z-axis translation",465.658,ID["EPSG",8607]],PARAMETER["X-axis rotation",-0.406857330322,ID["EPSG",8608]],PARAMETER["Y-axis rotation",0.350732676543,ID["EPSG",8609]],PARAMETER["Z-axis rotation",-1.87034738361,ID["EPSG",8610]],PARAMETER["Scale difference",1.0000040812,ID["EPSG",8611]]]]'
    )
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ""
    assert gdal.VSIStatL(filename + ".aux.xml") is not None

    ds = gdal.Open(filename)
    gdal.ErrorReset()
    got_srs = ds.GetSpatialRef()
    assert got_srs.IsSame(srs)
    ds = None

    gdal.Unlink(filename)


@pytest.mark.require_proj(9, 0)
def test_tiff_srs_write_projected_3d_built_as_pseudo_compound():

    filename = "/vsimem/test_tiff_srs_write_projected_3d_built_as_pseudo_compound.tif"
    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:6340+6319")

    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ""
    assert gdal.VSIStatL(filename + ".aux.xml") is not None

    ds = gdal.Open(filename)
    gdal.ErrorReset()
    got_srs = ds.GetSpatialRef()
    assert got_srs.IsSame(srs)
    ds = None

    gdal.Unlink(filename)


def test_tiff_srs_try_write_derived_projected():

    tmpfile = "/vsimem/tmp.tif"
    ds = gdal.GetDriverByName("GTiff").Create(tmpfile, 1, 1)
    wkt = 'DERIVEDPROJCRS["Site Localization",BASEPROJCRS["Transverse Mercator centered in area of interest",BASEGEOGCRS["ETRS89",ENSEMBLE["European Terrestrial Reference System 1989 ensemble",MEMBER["European Terrestrial Reference Frame 1989"],MEMBER["European Terrestrial Reference Frame 1990"],MEMBER["European Terrestrial Reference Frame 1991"],MEMBER["European Terrestrial Reference Frame 1992"],MEMBER["European Terrestrial Reference Frame 1993"],MEMBER["European Terrestrial Reference Frame 1994"],MEMBER["European Terrestrial Reference Frame 1996"],MEMBER["European Terrestrial Reference Frame 1997"],MEMBER["European Terrestrial Reference Frame 2000"],MEMBER["European Terrestrial Reference Frame 2005"],MEMBER["European Terrestrial Reference Frame 2014"],ELLIPSOID["GRS 1980",6378137,298.257222101,LENGTHUNIT["metre",1]],ENSEMBLEACCURACY[0.1]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]]],CONVERSION["Transverse Mercator",METHOD["Transverse Mercator",ID["EPSG",9807]],PARAMETER["Latitude of natural origin",46.5289246891571,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8801]],PARAMETER["Longitude of natural origin",6.70096726335249,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8802]],PARAMETER["Scale factor at natural origin",1,SCALEUNIT["unity",1],ID["EPSG",8805]],PARAMETER["False easting",0,LENGTHUNIT["metre",1],ID["EPSG",8806]],PARAMETER["False northing",0,LENGTHUNIT["metre",1],ID["EPSG",8807]]]],DERIVINGCONVERSION["Affine transformation as PROJ-based",METHOD["PROJ-based operation method: +proj=pipeline  +step +proj=affine +xoff=6018.77495 +yoff=614.23698 +zoff=-399.91220 +s11=0.998812215508 +s12=0.0303546283772 +s13=-0.00129655684858 +s21=-0.0303565906348 +s22=0.998811840177 +s23=-0.00152042684237 +s31=0.00124977142441 +s32=0.00155911155192 +s33=0.999272201962"]],CS[Cartesian,3],AXIS["site east (x)",east,ORDER[1],LENGTHUNIT["metre",1,ID["EPSG",9001]]],AXIS["site north (y)",north,ORDER[2],LENGTHUNIT["metre",1,ID["EPSG",9001]]],AXIS["site up (z)",up,ORDER[3],LENGTHUNIT["metre",1,ID["EPSG",9001]]]]'
    srs_ref = osr.SpatialReference()
    srs_ref.ImportFromWkt(wkt)
    ds.SetSpatialRef(srs_ref)
    ds = None

    assert gdal.VSIStatL(tmpfile + ".aux.xml")
    ds = gdal.Open(tmpfile)
    srs = ds.GetSpatialRef()
    assert srs is not None
    assert srs.IsSame(srs_ref)
    ds = None

    gdal.Unlink(tmpfile + ".aux.xml")
    ds = gdal.Open(tmpfile)
    assert ds.GetSpatialRef() is None
    ds = None

    gdal.Unlink(tmpfile)


def test_tiff_srs_epsg_2193_override():

    ds = gdal.Open("data/gtiff/epsg_2193_override.tif")
    gdal.ErrorReset()
    srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == "", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert srs.GetAuthorityCode(None) == "2193"


def test_tiff_srs_projected_GTCitationGeoKey_with_underscore_and_GeogTOWGS84GeoKey():
    """Test bugfix for https://lists.osgeo.org/pipermail/gdal-dev/2023-March/057011.html"""

    ds = gdal.Open(
        "data/gtiff/projected_GTCitationGeoKey_with_underscore_and_GeogTOWGS84GeoKey.tif"
    )
    gdal.ErrorReset()
    srs = ds.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "2039"
    assert "+proj=tmerc" in srs.ExportToProj4()
    if osr.GetPROJVersionMajor() >= 9:  # not necessarily the minimum version
        assert srs.GetName() == "Israel 1993 / Israeli TM Grid"


def test_tiff_srs_write_compound_with_non_epsg_vert_crs():
    """Test bugfix for https://github.com/OSGeo/gdal/issues/7833"""

    if int(gdal.GetDriverByName("GTiff").GetMetadataItem("LIBGEOTIFF")) < 1600:
        pytest.skip("libgeotiff >= 1.6.0 required")

    filename = "/vsimem/test_tiff_srs_write_compound_with_non_epsg_vert_crs.tif"
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""COMPD_CS["TestMS",
    GEOGCS["NAD83(2011)",
        DATUM["NAD83_National_Spatial_Reference_System_2011",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            AUTHORITY["EPSG","1116"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AXIS["Latitude",NORTH],
        AXIS["Longitude",EAST],
        AUTHORITY["EPSG","6318"]],
    VERT_CS["Mississippi_River_ERTDM_TCARI_MLLW_Riley2023",
        VERT_DATUM["MLLW_Riley2023",2005,
            AUTHORITY["NOAA","799"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Gravity-related height",UP],
        AUTHORITY["NOAA","800"]],
    AUTHORITY["NOAA","2000"]]""")

    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ""

    ds = gdal.Open(filename)
    srs = ds.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert gdal.GetLastErrorMsg() == ""

    gdal.Unlink(filename)

    assert (
        wkt
        == """COMPD_CS["TestMS",GEOGCS["NAD83(2011)",DATUM["NAD83_National_Spatial_Reference_System_2011",SPHEROID["GRS 1980",6378137,298.257222101004,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","1116"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","6318"]],VERT_CS["Mississippi_River_ERTDM_TCARI_MLLW_Riley2023",VERT_DATUM["unknown",2005],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Up",UP]]]"""
    )


def test_tiff_srs_read_compound_with_VerticalCitationGeoKey_only():
    """Test bugfix for https://github.com/OSGeo/gdal/issues/7833"""

    ds = gdal.Open("data/gtiff/compound_with_VerticalCitationGeoKey_only.tif")
    srs = ds.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert gdal.GetLastErrorMsg() == ""

    assert (
        wkt
        == """COMPD_CS["TestMS",GEOGCS["NAD83(2011)",DATUM["NAD83_National_Spatial_Reference_System_2011",SPHEROID["GRS 1980",6378137,298.257222101004,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","1116"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","6318"]],VERT_CS["NAVD88 height",VERT_DATUM["North American Vertical Datum 1988",2005,AUTHORITY["EPSG","5103"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Up",UP]]]"""
    )


@pytest.mark.parametrize(
    "code",
    [
        7415,  # Amersfoort / RD New + NAP height
        9707,  # WGS 84 + EGM96 height
    ],
)
@pytest.mark.require_proj(
    7, 2
)  # not necessarily the minimum version, but 9707 doesn't exist in PROJ 6.x
def test_tiff_srs_read_compound_with_EPSG_code(code):
    """Test bugfix for https://github.com/OSGeo/gdal/issues/7982"""

    filename = "/vsimem/test_tiff_srs_read_compound_with_EPSG_code.tif"
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(code)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    ds = None
    ds = gdal.Open(filename)
    gdal.ErrorReset()
    got_srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == "", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert got_srs.GetAuthorityCode(None) == str(code)
    assert got_srs.IsSame(srs)
    ds = None
    gdal.Unlink(filename)


def test_tiff_srs_read_compound_without_EPSG_code():
    """Test case where identification of code for CompoundCRS (added for
    bugfix of https://github.com/OSGeo/gdal/issues/7982) doesn't trigger"""

    if int(gdal.GetDriverByName("GTiff").GetMetadataItem("LIBGEOTIFF")) < 1600:
        pytest.skip("libgeotiff >= 1.6.0 required")

    filename = "/vsimem/test_tiff_srs_read_compound_without_EPSG_code.tif"
    srs = osr.SpatialReference()
    # WGS 84 + NAP height, unlikely to have a EPSG code ever
    srs.SetFromUserInput("EPSG:4326+5709")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    ds = None
    ds = gdal.Open(filename)
    gdal.ErrorReset()
    got_srs = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == "", srs.ExportToWkt(["FORMAT=WKT2_2019"])
    assert got_srs.GetAuthorityCode(None) is None
    assert got_srs.GetAuthorityCode("GEOGCS") == "4326"
    assert got_srs.GetAuthorityCode("VERT_CS") == "5709"
    assert got_srs.IsSame(srs)
    ds = None
    gdal.Unlink(filename)


def test_tiff_srs_projection_method_unknown_of_geotiff_with_crs_code():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(8857)  # "WGS 84 / Equal Earth Greenwich"
    filename = (
        "/vsimem/test_tiff_srs_projection_method_unknown_of_geotiff_with_crs_code.tif"
    )
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    ds = None
    assert gdal.VSIStatL(filename + ".aux.xml") is None
    ds = gdal.Open(filename)
    gdal.ErrorReset()
    srs = ds.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "8857"
    ds = None
    gdal.Unlink(filename)


@pytest.mark.require_proj(9, 0)
def test_tiff_srs_projection_method_unknown_of_geotiff_without_crs_code():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""PROJCS["WGS_1984_Equal_Earth_Greenwich",
    GEOGCS["GCS_WGS_1984",
        DATUM["D_WGS_1984",
            SPHEROID["WGS_1984",6378137.0,298.257223563]],
        PRIMEM["Greenwich",0.0],
        UNIT["Degree",0.0174532925199433]],
    PROJECTION["Equal_Earth"],
    PARAMETER["False_Easting",0.0],
    PARAMETER["False_Northing",0.0],
    PARAMETER["Central_Meridian",0.0],
    UNIT["Meter",1.0]]""")
    filename = "/vsimem/test_tiff_srs_projection_method_unknown_of_geotiff_without_crs_code.tif"
    ds = gdal.GetDriverByName("GTiff").Create(
        filename, 1, 1, options=["GEOTIFF_KEYS_FLAVOR=ESRI_PE"]
    )
    ds.SetSpatialRef(srs)
    ds = None
    assert gdal.VSIStatL(filename + ".aux.xml") is None
    ds = gdal.Open(filename)
    gdal.ErrorReset()
    got_srs = ds.GetSpatialRef()
    assert got_srs.IsSame(srs), got_srs.ExportToWkt()
    ds = None
    gdal.Unlink(filename)


def test_tiff_srs_build_compd_crs_name_without_citation():

    ds = gdal.Open("data/gtiff/compdcrs_no_citation.tif")
    assert ds.GetSpatialRef().GetName() == "WGS 84 / UTM zone 17N + EGM2008 height"


def test_tiff_srs_read_epsg_27563_allgeokeys():

    ds = gdal.Open("data/gtiff/epsg_27563_allgeokeys.tif")
    srs = ds.GetSpatialRef()
    wkt = srs.ExportToWkt(["FORMAT=WKT2_2019"])
    # deal with differences of precision according to PROJ version
    wkt = wkt.replace("49.0000000000001", "49")
    wkt = wkt.replace("49.0000000000002", "49")
    assert 'PARAMETER["Latitude of natural origin",49,ANGLEUNIT["grad"' in wkt
    assert (
        srs.ExportToProj4()
        == "+proj=lcc +lat_1=44.1 +lat_0=44.1 +lon_0=0 +k_0=0.999877499 +x_0=600000 +y_0=200000 +ellps=clrk80ign +pm=paris +towgs84=-168,-60,320,0,0,0,0 +units=m +no_defs"
    )


def test_tiff_srs_write_read_epsg_27563_only_code(tmp_vsimem):

    filename = str(tmp_vsimem / "test.tif")
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(27563)
    ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
    ds.SetSpatialRef(srs)
    ds = None

    ds = gdal.Open(filename)
    srs = ds.GetSpatialRef()
    assert (
        'PARAMETER["Latitude of natural origin",49,ANGLEUNIT["grad"'
        in srs.ExportToWkt(["FORMAT=WKT2_2019"])
    )
    assert (
        srs.ExportToProj4()
        == "+proj=lcc +lat_1=44.1 +lat_0=44.1 +lon_0=0 +k_0=0.999877499 +x_0=600000 +y_0=200000 +ellps=clrk80ign +pm=paris +towgs84=-168,-60,320,0,0,0,0 +units=m +no_defs"
    )


@pytest.mark.parametrize(
    "config_options",
    [
        {},
        {
            "GTIFF_WRITE_ANGULAR_PARAMS_IN_DEGREE": "YES",
            "GTIFF_READ_ANGULAR_PARAMS_IN_DEGREE": "YES",
        },
    ],
)
def test_tiff_srs_write_read_epsg_27563_full_def(tmp_vsimem, config_options):

    with gdal.config_options(config_options):
        filename = str(tmp_vsimem / "test.tif")
        srs = osr.SpatialReference()
        srs.SetFromUserInput("""PROJCRS["NTF (Paris) / Lambert Sud France",
        BASEGEOGCRS["NTF (Paris)",
            DATUM["Nouvelle Triangulation Francaise (Paris)",
                ELLIPSOID["Clarke 1880 (IGN)",6378249.2,293.466021293627,
                    LENGTHUNIT["metre",1]]],
            PRIMEM["Paris",2.5969213,
                ANGLEUNIT["grad",0.0157079632679489]],
            ID["EPSG",4807]],
        CONVERSION["Lambert Sud France",
            METHOD["Lambert Conic Conformal (1SP)",
                ID["EPSG",9801]],
            PARAMETER["Latitude of natural origin",49,
                ANGLEUNIT["grad",0.0157079632679489],
                ID["EPSG",8801]],
            PARAMETER["Longitude of natural origin",0,
                ANGLEUNIT["grad",0.0157079632679489],
                ID["EPSG",8802]],
            PARAMETER["Scale factor at natural origin",0.999877499,
                SCALEUNIT["unity",1],
                ID["EPSG",8805]],
            PARAMETER["False easting",600000,
                LENGTHUNIT["metre",1],
                ID["EPSG",8806]],
            PARAMETER["False northing",200000,
                LENGTHUNIT["metre",1],
                ID["EPSG",8807]]],
        CS[Cartesian,2],
            AXIS["easting (X)",east,
                ORDER[1],
                LENGTHUNIT["metre",1]],
            AXIS["northing (Y)",north,
                ORDER[2],
                LENGTHUNIT["metre",1]]]""")
        ds = gdal.GetDriverByName("GTiff").Create(filename, 1, 1)
        ds.SetSpatialRef(srs)
        ds = None

        ds = gdal.Open(filename)
        srs = ds.GetSpatialRef()
        wkt = srs.ExportToWkt(["FORMAT=WKT2_2019"])
        # deal with differences of precision according to PROJ version
        wkt = wkt.replace("49.0000000000001", "49")
        wkt = wkt.replace("49.0000000000002", "49")
        assert 'PARAMETER["Latitude of natural origin",49,ANGLEUNIT["grad"' in wkt
        assert (
            srs.ExportToProj4()
            == "+proj=lcc +lat_1=44.1 +lat_0=44.1 +lon_0=0 +k_0=0.999877499 +x_0=600000 +y_0=200000 +ellps=clrk80ign +pm=paris +units=m +no_defs"
        )


@pytest.mark.require_proj(9, 0)
def test_tiff_srs_infer_iau_code_from_srs_name():

    ds = gdal.Open("data/gtiff/tiff_srs_iau_2015_30110.tif")
    srs = ds.GetSpatialRef()
    assert srs.GetAuthorityName(None) == "IAU_2015"
    assert srs.GetAuthorityCode(None) == "30110"
