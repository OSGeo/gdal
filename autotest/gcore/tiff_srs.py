#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write round-tripping of SRS for GeoTIFF format.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
from osgeo import osr


###############################################################################
# Test fix for #4677:


def test_tiff_srs_without_linear_units():

    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=vandg +datum=WGS84')

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_without_linear_units.tif', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('/vsimem/tiff_srs_without_linear_units.tif')
    wkt = ds.GetProjectionRef()
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink('/vsimem/tiff_srs_without_linear_units.tif')

    assert sr.IsSame(sr2) == 1, 'did not get expected SRS'

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

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_compd_cs.tif', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS', 'YES')
    ds = gdal.Open('/vsimem/tiff_srs_compd_cs.tif')
    wkt = ds.GetProjectionRef()
    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS', None)
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink('/vsimem/tiff_srs_compd_cs.tif')

    assert sr.IsSame(sr2) == 1, wkt

###############################################################################
# Test reading a COMPDCS without VerticalCSType


def test_srs_read_compd_cs():

    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS', 'YES')
    ds = gdal.Open('data/vertcs_user_defined.tif')
    wkt = ds.GetProjectionRef()
    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS', None)

    assert wkt == 'COMPD_CS["NAD27 / UTM zone 11N + EGM2008 height",PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]],VERT_CS["EGM2008 height",VERT_DATUM["EGM2008 geoid",2005,AUTHORITY["EPSG","1027"]],UNIT["foot",0.3048,AUTHORITY["EPSG","9002"]],AXIS["Up",UP]]]'

###############################################################################
# Test reading a GeoTIFF with both StdParallel1 and ScaleAtNatOrigin defined (#5791)


def test_tiff_srs_weird_mercator_2sp():

    ds = gdal.Open('data/weird_mercator_2sp.tif')
    gdal.PushErrorHandler()
    wkt = ds.GetProjectionRef()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'warning expected'
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

    assert sr.IsSame(sr2) == 1, 'did not get expected SRS'

###############################################################################
# Test reading ESRI WGS_1984_Web_Mercator_Auxiliary_Sphere


def test_tiff_srs_WGS_1984_Web_Mercator_Auxiliary_Sphere():

    ds = gdal.Open('data/WGS_1984_Web_Mercator_Auxiliary_Sphere.tif')
    wkt = ds.GetProjectionRef()
    sr = osr.SpatialReference()
    sr.SetFromUserInput(wkt)
    wkt = sr.ExportToPrettyWkt()
    ds = None

    assert wkt == """PROJCS["WGS 84 / Pseudo-Mercator",
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

###############################################################################
# Test reading ESRI:102113 WGS_1984_Web_Mercator


def test_tiff_srs_WGS_1984_Web_Mercator():

    ds = gdal.Open('data/WGS_1984_Web_Mercator.tif')
    sr = ds.GetSpatialRef()
    ds = None

    assert sr.GetAuthorityName(None) == 'ESRI'
    assert sr.GetAuthorityCode(None) == '102113'

###############################################################################
# Test writing and reading various angular units


def test_tiff_srs_angular_units():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_angular_units.tif', 1, 1)
    ds.SetProjection("""GEOGCS["WGS 84 (arc-second)",
    DATUM["WGS_1984 (arc-second)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["arc-second",4.848136811095361e-06]]""")
    ds = None
    ds = gdal.Open('/vsimem/tiff_srs_angular_units.tif')
    wkt = ds.GetProjectionRef()
    assert 'UNIT["arc-second",4.84813681109536E-06' in wkt
    ds = None

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_angular_units.tif', 1, 1)
    ds.SetProjection("""GEOGCS["WGS 84 (arc-minute)",
    DATUM["WGS_1984 (arc-minute)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["arc-minute",0.0002908882086657216]]""")
    ds = None
    ds = gdal.Open('/vsimem/tiff_srs_angular_units.tif')
    wkt = ds.GetProjectionRef()
    assert 'UNIT["arc-minute",0.000290888208665722]' in wkt
    ds = None

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_angular_units.tif', 1, 1)
    ds.SetProjection("""GEOGCS["WGS 84 (grad)",
    DATUM["WGS_1984 (grad)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["grad",0.01570796326794897]]""")
    ds = None
    ds = gdal.Open('/vsimem/tiff_srs_angular_units.tif')
    wkt = ds.GetProjectionRef()
    assert 'UNIT["grad",0.015707963267949' in wkt
    ds = None

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_angular_units.tif', 1, 1)
    ds.SetProjection("""GEOGCS["WGS 84 (gon)",
    DATUM["WGS_1984 (gon)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["gon",0.01570796326794897]]""")
    ds = None
    ds = gdal.Open('/vsimem/tiff_srs_angular_units.tif')
    wkt = ds.GetProjectionRef()
    assert 'UNIT["gon",0.015707963267949]' in wkt
    ds = None

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_angular_units.tif', 1, 1)
    ds.SetProjection("""GEOGCS["WGS 84 (radian)",
    DATUM["WGS_1984 (radian)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["radian",1]]""")
    ds = None
    ds = gdal.Open('/vsimem/tiff_srs_angular_units.tif')
    wkt = ds.GetProjectionRef()
    assert 'UNIT["radian",1]' in wkt
    ds = None

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_angular_units.tif', 1, 1)
    ds.SetProjection("""GEOGCS["WGS 84 (custom)",
    DATUM["WGS_1984 (custom)",
        SPHEROID["WGS 84",6378137,298.257223563]],
    PRIMEM["Greenwich",0],
    UNIT["custom",1.23]]""")
    ds = None
    ds = gdal.Open('/vsimem/tiff_srs_angular_units.tif')
    wkt = ds.GetProjectionRef()
    assert 'UNIT["custom",1.23]' in wkt
    ds = None

    gdal.Unlink('/vsimem/tiff_srs_angular_units.tif')

###############################################################################
# Test writing and reading a unknown datum but with a known ellipsoid


def test_tiff_custom_datum_known_ellipsoid():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_custom_datum_known_ellipsoid.tif', 1, 1)
    ds.SetProjection("""GEOGCS["WGS 84 based",
    DATUM["WGS_1984_based",
        SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]],
    PRIMEM["Greenwich",0],
    UNIT["degree",1]]""")
    ds = None
    ds = gdal.Open('/vsimem/tiff_custom_datum_known_ellipsoid.tif')
    wkt = ds.GetProjectionRef()
    assert wkt == 'GEOGCS["WGS 84 based",DATUM["WGS_1984_based",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'
    ds = None

    gdal.Unlink('/vsimem/tiff_custom_datum_known_ellipsoid.tif')

###############################################################################
# Test reading a GeoTIFF file with only PCS set, but with a ProjLinearUnitsGeoKey
# override to another unit (us-feet) ... (#6210)


def test_tiff_srs_epsg_2853_with_us_feet():

    old_val = gdal.GetConfigOption('GTIFF_IMPORT_FROM_EPSG')
    gdal.SetConfigOption('GTIFF_IMPORT_FROM_EPSG', 'YES')
    ds = gdal.Open('data/epsg_2853_with_us_feet.tif')
    gdal.SetConfigOption('GTIFF_IMPORT_FROM_EPSG', old_val)
    wkt = ds.GetProjectionRef()
    assert 'PARAMETER["false_easting",11482916.66' in wkt and 'UNIT["us_survey_feet",0.3048006' in wkt and '2853' not in wkt

    gdal.SetConfigOption('GTIFF_IMPORT_FROM_EPSG', 'NO')
    ds = gdal.Open('data/epsg_2853_with_us_feet.tif')
    gdal.SetConfigOption('GTIFF_IMPORT_FROM_EPSG', old_val)
    wkt = ds.GetProjectionRef()
    assert 'PARAMETER["false_easting",11482916.66' in wkt and 'UNIT["us_survey_feet",0.3048006' in wkt and '2853' not in wkt

###############################################################################
# Test reading a SRS with a PCSCitationGeoKey = "LUnits = ..."


def test_tiff_srs_PCSCitationGeoKey_LUnits():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_PCSCitationGeoKey_LUnits.tif', 1, 1)
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
    ds = gdal.Open('/vsimem/tiff_srs_PCSCitationGeoKey_LUnits.tif')
    wkt = ds.GetProjectionRef()
    assert wkt == 'PROJCS["UTM Zone 32, Northern Hemisphere",GEOGCS["GRS 1980(IUGG, 1980)",DATUM["unknown",SPHEROID["GRS80",6378137,298.257222101],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",9],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",50000000],PARAMETER["false_northing",0],UNIT["Centimeter",0.01],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    ds = None

    gdal.Unlink('/vsimem/tiff_srs_PCSCitationGeoKey_LUnits.tif')

###############################################################################
# Test reading a geotiff key ProjectionGeoKey (Short,1): Unknown-3856


def test_tiff_srs_projection_3856():

    ds = gdal.Open('data/projection_3856.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    assert 'EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs' in wkt

###############################################################################
# Test reading a geotiff with a LOCAL_CS and a Imagine citation


def test_tiff_srs_imagine_localcs_citation():

    ds = gdal.Open('data/imagine_localcs_citation.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    assert wkt.startswith('LOCAL_CS["Projection Name = UTM Units = meters GeoTIFF Units = meters"')

###############################################################################
# Test reading a geotiff with a EPSG code and a TOWGS84 key that must
# override the default coming from EPSG


def test_tiff_srs_towgs84_override_OSR_STRIP_TOWGS84_NO():

    with gdaltest.config_option('OSR_STRIP_TOWGS84', 'NO'):
        ds = gdal.Open('data/gtiff_towgs84_override.tif')
        wkt = ds.GetProjectionRef()
    ds = None

    assert 'TOWGS84[584.8,67,400.3,0.105,0.013,-2.378,10.29]' in wkt, wkt


def test_tiff_srs_towgs84_override_OSR_STRIP_TOWGS84_default():

    ds = gdal.Open('data/gtiff_towgs84_override.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    assert 'TOWGS84' not in wkt

###############################################################################
# Test reading PCSCitationGeoKey (#7199)


def test_tiff_srs_pcscitation():

    ds = gdal.Open('data/pcscitation.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    assert wkt.startswith('PROJCS["mycitation",')

###############################################################################
# Test reading file with ProjectedCSTypeGeoKey and GeographicTypeGeoKey


def test_tiff_srs_ProjectedCSTypeGeoKey_GeographicTypeGeoKey():

    ds = gdal.Open('data/utmsmall.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "26711"


def _test_tiff_srs(sr, expect_fail):
    """
    This is not a test by itself; it gets called by the tests below.
    """
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/TestTiffSRS.tif', 1, 1)
    ds.SetSpatialRef(sr)
    ds = None

    # The GeoTIFF driver is smart enough to figure out that a CRS with
    # '+proj=longlat +datum=WGS84' is EPSG:4326
    if sr.GetAuthorityCode(None) is None and '+proj=longlat +datum=WGS84' in sr.ExportToProj4():
        sr.ImportFromEPSG(4326)

    ds = gdal.Open('/vsimem/TestTiffSRS.tif')
    sr2 = ds.GetSpatialRef()
    if 'Miller' in sr2.ExportToWkt():
        # Trick so that the EXTENSION node with a PROJ string including +R_A is added
        sr2.ImportFromProj4(sr2.ExportToProj4())
    ds = None

    gdal.Unlink('/vsimem/TestTiffSRS.tif')

    if sr.IsSame(sr2) != 1:
        if expect_fail:
            pytest.xfail('did not get expected SRS. known to be broken currently. FIXME!')

        #print(sr.ExportToWkt(['FORMAT=WKT2_2019']))
        #print(sr2.ExportToWkt(['FORMAT=WKT2_2019']))
        print(sr)
        print(sr2)
        assert False, 'did not get expected SRS'
    else:
        if expect_fail:
            print('Succeeded but expected fail...')


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
    #[2065, True],  # krovak South-West
    [5221, True],  # krovak east-north
    [2066, False],  # cass
    [2964, False],  # aea
    #[3410, False],  # cea spherical, method=9834. EPSG:3410 is now deprecated
    [6933, False],  # cea ellipsoidal, method=9835
    #[3786, False],  # eqc spherical, method=9823. EPSG:3786 is now deprecated
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


@pytest.mark.parametrize('use_epsg_code', [0, 1])
@pytest.mark.parametrize(
    'epsg_code,epsg_proj4_broken',
    epsg_list,
    ids=[str(r[0]) for r in epsg_list],
)
def test_tiff_srs(use_epsg_code, epsg_code, epsg_proj4_broken):
    sr = osr.SpatialReference()
    if epsg_code > 32767:
        sr.SetFromUserInput('ESRI:' + str(epsg_code))
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
    'proj4',
    [
        '+proj=eqdc +lat_0=%.16g +lon_0=%.16g +lat_1=%.16g +lat_2=%.16g" +x_0=%.16g +y_0=%.16g' % (1, 2, 3, 4, 5, 6),
        '+proj=mill +R_A +lon_0=2 +x_0=3 +y_0=4 +datum=WGS84 +units=m +no_defs',
        '+proj=gnom +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g' % (1, 2, 3, 4),
        '+proj=robin +lon_0=%.16g +x_0=%.16g +y_0=%.16g' % (1, 2, 3),
        '+proj=sinu +lon_0=%.16g +x_0=%.16g +y_0=%.16g' % (1, 2, 3),
    ],
    ids=[
        'eqdc',
        'mill',
        'gnom',
        'robin',
        'sinu',
    ]
)
def test_tiff_srs_proj4(proj4):
    sr = osr.SpatialReference()
    sr.SetFromUserInput(proj4)
    _test_tiff_srs(sr, False)


def _create_geotiff1_1_from_copy_and_compare(srcfilename, options = []):
    if int(gdal.GetDriverByName('GTiff').GetMetadataItem('LIBGEOTIFF')) < 1600:
        pytest.skip()

    src_ds = gdal.Open(srcfilename)
    tmpfile = '/vsimem/tmp.tif'
    gdal.GetDriverByName('GTiff').CreateCopy(tmpfile, src_ds, options = options + ['ENDIANNESS=LITTLE'])
    f = gdal.VSIFOpenL(tmpfile, 'rb')
    data = gdal.VSIFReadL(1, 100000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink(tmpfile)
    assert data == open(src_ds.GetDescription(), 'rb').read()


def test_tiff_srs_read_epsg4326_geotiff1_1():
    ds = gdal.Open('data/epsg4326_geotiff1_1.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == '4326'


def test_tiff_srs_write_epsg4326_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare('data/epsg4326_geotiff1_1.tif',
                                             options = ['GEOTIFF_VERSION=1.1'])


def test_tiff_srs_read_epsg26711_geotiff1_1():
    ds = gdal.Open('data/epsg26711_geotiff1_1.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == '26711'


def test_tiff_srs_write_epsg26711_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare('data/epsg26711_geotiff1_1.tif',
                                             options = ['GEOTIFF_VERSION=1.1'])


def test_tiff_srs_read_epsg4326_3855_geotiff1_1():
    ds = gdal.Open('data/epsg4326_3855_geotiff1_1.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetName() == 'WGS 84 + EGM2008 height'
    assert sr.GetAuthorityCode('COMPD_CS|GEOGCS') == '4326'
    assert sr.GetAuthorityCode('COMPD_CS|VERT_CS') == '3855'


def test_tiff_srs_write_epsg4326_3855_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare('data/epsg4326_3855_geotiff1_1.tif')


def test_tiff_srs_read_epsg4979_geotiff1_1():
    ds = gdal.Open('data/epsg4979_geotiff1_1.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == '4979'


def test_tiff_srs_write_epsg4979_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare('data/epsg4979_geotiff1_1.tif')


def test_tiff_srs_write_epsg4937_etrs89_3D_geotiff1_1():
    if int(gdal.GetDriverByName('GTiff').GetMetadataItem('LIBGEOTIFF')) < 1600:
        pytest.skip()
    tmpfile = '/vsimem/tmp.tif'
    ds = gdal.GetDriverByName('GTiff').Create(tmpfile, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4937)
    ds.SetSpatialRef(sr)
    ds = None
    ds = gdal.Open(tmpfile)
    assert sr.GetName() == 'ETRS89'
    assert sr.GetAuthorityCode(None) == '4937'
    ds = None
    gdal.Unlink(tmpfile)


# Deprecated way of conveying GeographicCRS 3D
def test_tiff_srs_read_epsg4326_5030_geotiff1_1():
    ds = gdal.Open('data/epsg4326_5030_geotiff1_1.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == '4979'


def test_tiff_srs_read_epsg26711_3855_geotiff1_1():
    ds = gdal.Open('data/epsg26711_3855_geotiff1_1.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetName() == 'NAD27 / UTM zone 11N + EGM2008 height'
    assert sr.GetAuthorityCode('COMPD_CS|PROJCS') == '26711'
    assert sr.GetAuthorityCode('COMPD_CS|VERT_CS') == '3855'


def test_tiff_srs_write_epsg26711_3855_geotiff1_1():
    _create_geotiff1_1_from_copy_and_compare('data/epsg26711_3855_geotiff1_1.tif')


# ProjectedCRS 3D not really defined yet in GeoTIFF 1.1, but this is
# a natural extension
def test_tiff_srs_read_epsg32631_4979_geotiff1_1():
    ds = gdal.Open('data/epsg32631_4979_geotiff1_1.tif')
    sr = ds.GetSpatialRef()
    assert sr.IsProjected()
    assert sr.GetAxesCount() == 3
    assert sr.GetName() == 'WGS 84 / UTM zone 31N'
    sr_geog = osr.SpatialReference()
    sr_geog.CopyGeogCSFrom(sr)
    assert sr_geog.GetAuthorityCode(None) == '4979'


def test_tiff_srs_write_vertical_perspective():

    if osr.GetPROJVersionMajor() * 100 + osr.GetPROJVersionMinor() < 700:
        pytest.skip('requires PROJ 7 or later')

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src.tif', 1, 1)
    sr = osr.SpatialReference()
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 3000000, 0)
    sr.SetVerticalPerspective(1, 2, 0, 1000, 0, 0)
    gdal.ErrorReset()
    ds.SetSpatialRef(sr)
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    src_ds = gdal.Open('/vsimem/src.tif')
    # First is PROJ 7
    assert src_ds.GetSpatialRef().ExportToProj4() in ('+proj=nsper +lat_0=1 +lon_0=2 +h=1000 +x_0=0 +y_0=0 +R=3000000 +units=m +no_defs', '+proj=nsper +R=3000000 +lat_0=1 +lon_0=2 +h=1000 +x_0=0 +y_0=0 +wktext +no_defs')
    gdal.ErrorReset()
    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/dst.tif', src_ds)
    assert gdal.GetLastErrorMsg() == ''

    ds = gdal.Open('/vsimem/dst.tif')
    assert ds.GetSpatialRef().ExportToProj4() == src_ds.GetSpatialRef().ExportToProj4()

    src_ds = None
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/src.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/dst.tif')


def test_tiff_srs_write_ob_tran_eqc():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src.tif', 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromProj4( '+proj=ob_tran +o_proj=eqc +o_lon_p=-90 +o_lat_p=180 +lon_0=0 +R=3396190 +units=m +no_defs' )
    ds.SetSpatialRef(sr)
    ds = None

    assert gdal.VSIStatL('/vsimem/src.tif.aux.xml')

    ds = gdal.Open('/vsimem/src.tif')
    assert ds.GetSpatialRef().ExportToProj4() == '+proj=ob_tran +o_proj=eqc +o_lon_p=-90 +o_lat_p=180 +lon_0=0 +R=3396190 +units=m +no_defs'
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/src.tif')


def test_tiff_srs_towgs84_from_epsg_do_not_write_it():

    filename = '/vsimem/test.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    srs_in = osr.SpatialReference()
    srs_in.ImportFromEPSG(31468)
    srs_in.AddGuessedTOWGS84()
    assert srs_in.HasTOWGS84()
    ds.SetSpatialRef(srs_in)
    ds = None

    ds = gdal.Open(filename)
    with gdaltest.config_option('OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG', 'NO'):
        srs = ds.GetSpatialRef()
    assert not srs.HasTOWGS84()


def test_tiff_srs_towgs84_from_epsg_force_write_it():

    filename = '/vsimem/test.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
    srs_in = osr.SpatialReference()
    srs_in.ImportFromEPSG(31468)
    srs_in.AddGuessedTOWGS84()
    assert srs_in.HasTOWGS84()
    with gdaltest.config_option('GTIFF_WRITE_TOWGS84', 'YES'):
        ds.SetSpatialRef(srs_in)
        ds = None

    with gdaltest.config_option('OSR_STRIP_TOWGS84', 'NO'):
        ds = gdal.Open(filename)
        with gdaltest.config_option('OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG', 'NO'):
            srs = ds.GetSpatialRef()
    assert srs.HasTOWGS84()


def test_tiff_srs_towgs84_with_epsg_code_but_non_default_TOWGS84():

    filename = '/vsimem/test.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 1, 1)
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

    with gdaltest.config_option('OSR_STRIP_TOWGS84', 'NO'):
        ds = gdal.Open(filename)
        srs = ds.GetSpatialRef()
    assert srs.GetTOWGS84() == (1,2,3,4,5,6,7)


def test_tiff_srs_write_epsg3857():
    tmpfile = '/vsimem/tmp.tif'
    ds = gdal.GetDriverByName('GTiff').Create(tmpfile, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    ds.SetSpatialRef(sr)
    ds = None
    ds = gdal.Open(tmpfile)
    assert sr.GetName() == 'WGS 84 / Pseudo-Mercator'
    assert sr.GetAuthorityCode(None) == '3857'
    f = gdal.VSIFOpenL(tmpfile, 'rb')
    data = gdal.VSIFReadL(1, 100000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink(tmpfile)
    assert b"ESRI PE String" not in data


def test_tiff_srs_read_epsg26730_with_linear_units_set():
    ds = gdal.Open('data/epsg26730_with_linear_units_set.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == '26730'


def test_tiff_srs_read_user_defined_geokeys():
    if int(gdal.GetDriverByName('GTiff').GetMetadataItem('LIBGEOTIFF')) < 1600:
        pytest.skip()

    gdal.ErrorReset()
    ds = gdal.Open('data/byte_user_defined_geokeys.tif')
    sr = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == ''
    assert sr is not None


def test_tiff_srs_read_compoundcrs_without_gtcitation():
    if int(gdal.GetDriverByName('GTiff').GetMetadataItem('LIBGEOTIFF')) < 1600:
        pytest.skip()

    ds = gdal.Open('data/gtiff/compdcrs_without_gtcitation.tif')
    sr = ds.GetSpatialRef()
    assert sr.GetName() == 'WGS 84 / UTM zone 32N + EGM08_Geoid'


def test_tiff_srs_read_getspatialref_getgcpspatialref():

    ds = gdal.Open('data/byte.tif')
    assert ds.GetSpatialRef() is not None
    assert ds.GetGCPSpatialRef() is None

    ds = gdal.Open('data/byte.tif')
    assert ds.GetGCPSpatialRef() is None
    assert ds.GetSpatialRef() is not None

    ds = gdal.Open('data/byte.tif')
    assert ds.GetSpatialRef() is not None
    assert ds.GetSpatialRef() is not None
    assert ds.GetGCPSpatialRef() is None
    assert ds.GetGCPSpatialRef() is None

    ds = gdal.Open('data/byte_gcp_pixelispoint.tif')
    assert ds.GetSpatialRef() is None
    assert ds.GetGCPSpatialRef() is not None

    ds = gdal.Open('data/byte_gcp_pixelispoint.tif')
    assert ds.GetGCPSpatialRef() is not None
    assert ds.GetSpatialRef() is None

    ds = gdal.Open('data/byte_gcp_pixelispoint.tif')
    assert ds.GetGCPSpatialRef() is not None
    assert ds.GetGCPSpatialRef() is not None
    assert ds.GetSpatialRef() is None
    assert ds.GetSpatialRef() is None


def test_tiff_srs_read_VerticalUnitsGeoKey_private_range():
    ds = gdal.Open('data/gtiff/VerticalUnitsGeoKey_private_range.tif')
    with gdaltest.error_handler():
        sr = ds.GetSpatialRef()
    assert sr.GetName() == "NAD83 / UTM zone 16N"
    assert gdal.GetLastErrorMsg() != ''


def test_tiff_srs_read_invalid_semimajoraxis_compound():
    ds = gdal.Open('data/gtiff/invalid_semimajoraxis_compound.tif')
    # Check that it doesn't crash. PROJ >= 8.2.0 will return a NULL CRS
    # whereas previous versions will return a non-NULL one
    with gdaltest.error_handler():
        ds.GetSpatialRef()


def test_tiff_srs_try_write_derived_geographic():

    if osr.GetPROJVersionMajor() < 7:
        pytest.skip()

    tmpfile = '/vsimem/tmp.tif'
    ds = gdal.GetDriverByName('GTiff').Create(tmpfile, 1, 1)
    wkt = 'GEOGCRS["Coordinate System imported from GRIB file",BASEGEOGCRS["Coordinate System imported from GRIB file",DATUM["unnamed",ELLIPSOID["Sphere",6367470,0,LENGTHUNIT["metre",1,ID["EPSG",9001]]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]]],DERIVINGCONVERSION["Pole rotation (GRIB convention)",METHOD["Pole rotation (GRIB convention)"],PARAMETER["Latitude of the southern pole (GRIB convention)",-30,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]],PARAMETER["Longitude of the southern pole (GRIB convention)",-15,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]],PARAMETER["Axis rotation (GRIB convention)",0,ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]]],CS[ellipsoidal,2],AXIS["latitude",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]],AXIS["longitude",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433,ID["EPSG",9122]]]]'
    ds.SetProjection(wkt)
    ds = None

    assert gdal.VSIStatL(tmpfile + '.aux.xml')
    ds = gdal.Open(tmpfile)
    srs = ds.GetSpatialRef()
    assert srs is not None
    assert srs.IsDerivedGeographic()
    ds = None

    gdal.Unlink(tmpfile + '.aux.xml')
    ds = gdal.Open(tmpfile)
    assert ds.GetSpatialRef() is None
    ds = None

    gdal.Unlink(tmpfile)


def test_tiff_srs_read_GeogGeodeticDatumGeoKey_reserved_range():
    ds = gdal.Open('data/gtiff/GeogGeodeticDatumGeoKey_reserved.tif')
    with gdaltest.error_handler():
        sr = ds.GetSpatialRef()
    assert sr.GetName() == "WGS 84 / Pseudo-Mercator"
    assert gdal.GetLastErrorMsg() != ''
    assert gdal.GetLastErrorType() == gdal.CE_Warning


def test_tiff_srs_read_buggy_sentinel1_ellipsoid_code_4326():
    # That file has GeogEllipsoidGeoKey=4326, instead of 7030
    ds = gdal.Open('data/gtiff/buggy_sentinel1_ellipsoid_code_4326.tif')
    sr = ds.GetSpatialRef()
    assert gdal.GetLastErrorMsg() == ''
    assert sr.GetAuthorityCode('GEOGCS|DATUM|SPHEROID') == '7030'

