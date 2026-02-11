#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NetCDF driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

from gdrivers.netcdf import netcdf_setup, netcdf_test_copy  # noqa

netcdf_setup
import gdaltest
import pytest

# to please pyflakes
from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("netCDF")

###############################################################################
# Netcdf CF compliance Functions
###############################################################################

###############################################################################
# check for necessary software


def cfchecks_available():
    try:
        _, err = gdaltest.runexternal_out_and_err("cfchecks --help")
        return err == ""
    except OSError:
        return False


@pytest.fixture(scope="module", autouse=True)
def netcdf_cf_setup():

    # skip if on windows
    if os.name != "posix":
        pytest.skip("OS is not posix")

    if not cfchecks_available():
        pytest.skip(
            "cfchecks not available; see https://github.com/cedadev/cf-checker#installation"
        )


###############################################################################
# Check a file for CF compliance
def netcdf_cf_check_file(ifile, version="auto"):
    __tracebackhide__ = True

    if not os.path.exists(ifile):
        pytest.skip(f"File does not exist: {ifile}")

    command = f"cfchecks -v {version} {ifile}"

    ret, err = gdaltest.runexternal_out_and_err(command)

    # There should be a ERRORS detected summary
    if "ERRORS detected" not in ret:
        if "urlopen error" in err:
            pytest.skip("cfchecks network failure: " + err.strip()[:200])
        print(err)
        pytest.fail("ERROR with command - " + command)

    errors = []
    warnings = []

    for line in ret.splitlines():
        if "ERROR:" in line:
            errors.append(line)
        elif "WARNING:" in line:
            warnings.append(line)

    assert errors == [], f"CF check ERRORS for file {ifile}"
    assert warnings == [], f"CF check WARNINGS for file {ifile}"


###############################################################################
# Netcdf CF projection Functions and data
###############################################################################

###############################################################################
# Definitions to test projections that are supported by CF

# Tuple structure:
#  0: Short code (e.g. AEA) - (no GDAL significance, just for filenames etc.)
#  1: official name from CF-1 conventions
#  2: EPSG code, or WKT, to tell GDAL to do reprojection
#  3: Actual attribute official name of grid mapping
#  4: List of required attributes to define projection
#  5: List of required coordinate variable standard name attributes

netcdf_cfproj_tuples = {
    "AEA": (
        "AEA",
        "Albers Equal Area",
        "EPSG:3577",
        "albers_conical_equal_area",
        [
            "standard_parallel",
            "longitude_of_central_meridian",
            "latitude_of_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    "AZE": (
        "AZE",
        "Azimuthal Equidistant",
        # Didn't have EPSG suitable for AU
        "+proj=aeqd +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "azimuthal_equidistant",
        [
            "longitude_of_projection_origin",
            "latitude_of_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    "LAZEA": (
        "LAZEA",
        "Lambert azimuthal equal area",
        # Specify proj4 since no appropriate LAZEA for AU.
        # "+proj=laea +lat_0=0 +lon_0=134 +x_0=0 +y_0=0 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs",
        "+proj=laea +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "lambert_azimuthal_equal_area",
        [
            "longitude_of_projection_origin",
            "latitude_of_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    "LC_2SP": (
        "LC_2SP",
        "Lambert conformal",
        "EPSG:3112",
        "lambert_conformal_conic",
        [
            "standard_parallel",
            "longitude_of_central_meridian",
            "latitude_of_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    # TODO: Test LCC with 1SP
    "LCEA": (
        "LCEA",
        "Lambert Cylindrical Equal Area",
        "+proj=cea +lat_ts=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "lambert_cylindrical_equal_area",
        [
            "longitude_of_central_meridian",
            "standard_parallel",  # TODO: OR 'scale_factor_at_projection_origin'
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    # 2 entries for Mercator, since attribs different for 1SP or 2SP
    "M-1SP": (
        "M-1SP",
        "Mercator",
        "+proj=merc +lon_0=145 +k_0=1 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "mercator",
        [
            "longitude_of_projection_origin",
            "scale_factor_at_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    # Commented out as it seems GDAL itself's support of Mercator with 2SP
    #  is a bit dodgy
    "M-2SP": (
        "M-2SP",
        "Mercator",
        "+proj=merc +lat_ts=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        # Trying with full WKT:
        # """PROJCS["unnamed", GEOGCS["WGS 84", DATUM["WGS_1984", SPHEROID["WGS 84",6378137,298.257223563, AUTHORITY["EPSG","7030"]], AUTHORITY["EPSG","6326"]], PRIMEM["Greenwich",0], UNIT["degree",0.0174532925199433], AUTHORITY["EPSG","4326"]], PROJECTION["Mercator_2SP"], PARAMETER["central_meridian",146], PARAMETER["standard_parallel_1",-37], PARAMETER["latitude_of_origin",0], PARAMETER["false_easting",0], PARAMETER["false_northing",0], UNIT["metre",1, AUTHORITY["EPSG","9001"]]]""",
        "mercator",
        [
            "longitude_of_projection_origin",
            "standard_parallel",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    "Ortho": (
        "Ortho",
        "Orthographic",
        "+proj=ortho +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "orthographic",
        [
            "longitude_of_projection_origin",
            "latitude_of_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    # Seems GDAL may have problems with Polar stereographic, as it
    #  considers these "local coordinate systems"
    "PSt": (
        "PSt",
        "Polar stereographic",
        "+proj=stere +lat_ts=-37 +lat_0=-90 +lon_0=145 +k_0=1.0 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "polar_stereographic",
        [
            "straight_vertical_longitude_from_pole",
            "latitude_of_projection_origin",
            "standard_parallel",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    "St": (
        "St",
        "Stereographic",
        "+proj=stere +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        # 'PROJCS["unnamed", GEOGCS["WGS 84", DATUM["WGS_1984", SPHEROID["WGS 84",6378137,298.257223563, AUTHORITY["EPSG","7030"]], AUTHORITY["EPSG","6326"]], PRIMEM["Greenwich",0], UNIT["degree",0.0174532925199433], AUTHORITY["EPSG","4326"]], PROJECTION["Stereographic"], PARAMETER["latitude_of_origin",-37.5], PARAMETER["central_meridian",145], PARAMETER["scale_factor",1], PARAMETER["false_easting",0], PARAMETER["false_northing",0], UNIT["metre",1, AUTHORITY["EPSG","9001"]]]',
        "stereographic",
        [
            "longitude_of_projection_origin",
            "latitude_of_projection_origin",
            "scale_factor_at_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    # Note: Rotated Pole not in this list, as seems not GDAL-supported
    "TM": (
        "TM",
        "Transverse Mercator",
        "EPSG:32655",  # UTM Zone 55N
        "transverse_mercator",
        [
            "scale_factor_at_central_meridian",
            "longitude_of_central_meridian",
            "latitude_of_projection_origin",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
    "GEOS": (
        "GEOS",
        "Geostationary_satellite",
        "+proj=geos +h=35785831 +lon_0=145 +datum=WGS84 +sweep=y +units=m",
        "geostationary",
        [
            "longitude_of_projection_origin",
            "perspective_point_height",
            "sweep_angle_axis",
            "false_easting",
            "false_northing",
        ],
        ["projection_x_coordinate", "projection_y_coordinate"],
    ),
}

# By default, we will use GeoTIFF as the 'intermediate' raster format
# for gdalwarp'ing into before gdal_translate to NetCDF.
# But since Gratify can't act as a storage format for certain projections
# (e.g. Mercator-2SP), we will choose other intermediate formats for certain
# projection.
# The following array maps projection short code, to driver format to use

netcdf_cfproj_def_int_format = "GTiff"

netcdf_cfproj_int_fmt_maps = {"M-2SP": "HFA"}

netcdf_cfproj_format_fnames = {
    "HFA": "img",
    "GTiff": "tif",
    "NITF": "nitf",
    "ERS": "ers",
}

###############################################################################
# Test an NC file has valid conventions according to passed-in proj tuple
# Note: current testing strategy is a fairly simple attribute search.
# this could use GDAL NetCDF driver for getting attribs instead.


def netcdf_cfproj_test_cf(proj, projNc) -> None:
    command = "ncdump -h " + projNc
    dumpStr, err = gdaltest.runexternal_out_and_err(command)

    assert err == ""

    assert f':grid_mapping_name = "{proj[3]}"' in dumpStr

    # Check attributes in the projection are included.
    for attrib in proj[4]:
        # The ':' prefix and ' ' suffix is to help check for exact name,
        # e.g. to catch the standard_parallel_1 and 2 issue.
        assert f":{attrib} " in dumpStr

    # Now we check the required X and Y attributes are included (e.g. Rotated
    # Pole has special names required here.
    for coordVarStdName in proj[5]:
        assert coordVarStdName in dumpStr

    # Final check use the cf-checker.
    netcdf_cf_check_file(projNc, "auto")


###############################################################################
# Netcdf CF Tests
###############################################################################


###############################################################################
# test copy and CF compliance for lat/lon (no datum, no GEOGCS) file, tif->nc->tif
def test_netcdf_cf_1(netcdf_setup):  # noqa
    netcdf_test_copy("data/netcdf/trmm.nc", 1, 14, "tmp/netcdf_cf_1.nc")

    netcdf_cf_check_file("tmp/netcdf_cf_1.nc")

    netcdf_test_copy("tmp/netcdf_cf_1.nc", 1, 14, "tmp/netcdf_cf_1.tif", [], "GTIFF")


###############################################################################
# test copy and CF compliance for lat/lon (no datum, no GEOGCS) file, nc->nc
def test_netcdf_cf_2():

    netcdf_test_copy("data/netcdf/trmm.nc", 1, 14, "tmp/netcdf_cf_2.nc")

    netcdf_cf_check_file("tmp/netcdf_cf_2.nc", "auto")


###############################################################################
# test copy and CF compliance for lat/lon (W*S84) file, tif->nc->tif
# note: this test fails in trunk (before r23246)
def test_netcdf_cf_3():

    netcdf_test_copy("data/netcdf/trmm-wgs84.tif", 1, 14, "tmp/netcdf_cf_3.nc")

    netcdf_test_copy("tmp/netcdf_cf_3.nc", 1, 14, "tmp/netcdf_cf_3.tif", [], "GTIFF")

    netcdf_cf_check_file("tmp/netcdf_cf_3.nc", "auto")


###############################################################################
# test support for various CF projections


@gdaltest.disable_exceptions()
@pytest.mark.parametrize("proj_key", netcdf_cfproj_tuples.keys())
def test_netcdf_cf_4(proj_key):
    # Test a Geotiff file can be converted to NetCDF, and projection in
    # CF-1 conventions can be successfully maintained.
    # Warp the original file and then create a netcdf

    if proj_key == "GEOS":
        pytest.xfail('grid_mapping_name = "geostationary" not CF-compliant')

    inPath = "data"
    outPath = "tmp"
    origTiff = "netcdf/melb-small.tif"
    proj = netcdf_cfproj_tuples[proj_key]

    # Test if ncdump is available
    try:
        _, err = gdaltest.runexternal_out_and_err("ncdump -h")

        if "netcdf library version " not in err:
            pytest.skip("NOTICE: netcdf version not found")
    except OSError:
        # nothing is supported as ncdump not found
        pytest.skip("ncdump not available")

    if not os.path.exists(outPath):
        os.makedirs(outPath)

    dsTiff = gdal.Open(os.path.join(inPath, origTiff), gdal.GA_ReadOnly)
    s_srs_wkt = dsTiff.GetProjection()

    intFmt = netcdf_cfproj_int_fmt_maps.get(proj[0], netcdf_cfproj_def_int_format)
    intExt = netcdf_cfproj_format_fnames[intFmt]

    projRaster = os.path.join(
        outPath,
        "%s_%s.%s" % (os.path.basename(origTiff).rstrip(".tif"), proj[0], intExt),
    )
    srs = osr.SpatialReference()
    srs.SetFromUserInput(proj[2])
    t_srs_wkt = srs.ExportToWkt()

    dswarp = gdal.AutoCreateWarpedVRT(
        dsTiff, s_srs_wkt, t_srs_wkt, gdal.GRA_NearestNeighbour, 0
    )
    dsw = gdal.GetDriverByName(intFmt).CreateCopy(projRaster, dswarp, 0)
    projNc = os.path.join(
        outPath, "%s_%s.nc" % (os.path.basename(origTiff).rstrip(".tif"), proj[0])
    )

    # Force GDAL tags to be written to make testing easier, with preserved datum etc
    dst = gdal.GetDriverByName("NETCDF").CreateCopy(
        projNc, dsw, 0, ["WRITE_GDAL_TAGS=YES"]
    )

    assert dst is not None, "failed to translate from intermediate format to netCDF"

    # For drivers like HFA, line below ESSENTIAL so that all info is
    # saved to new raster file - which we'll reopen later and want
    # to be fully updated.
    dsw = None
    del dst

    netcdf_cfproj_test_cf(proj, projNc)

    # test file copy
    # We now copy to a new file, just to be safe
    projNc2 = projNc.rstrip(".nc") + "2.nc"
    projRaster2 = os.path.join(
        outPath,
        "%s_%s2.%s" % (os.path.basename(origTiff).rstrip(".tif"), proj[0], intExt),
    )

    netcdf_test_copy(projRaster, 1, None, projNc2, [], "NETCDF")
    netcdf_test_copy(projNc2, 1, None, projRaster2, [], intFmt)


###############################################################################
# test CF support for dims and variables in different groups


@pytest.mark.parametrize(
    "ifile",
    (
        "data/netcdf/cf_dimsindiff_4326.nc",
        "NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/data/temp",
        "NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/imagingGeometry/lookAngle",
    ),
)
def test_netcdf_cf_6(ifile):
    ds = gdal.Open(ifile)
    prj = ds.GetProjection()
    sr = osr.SpatialReference()
    sr.ImportFromWkt(prj)
    proj_out = sr.ExportToProj4()

    assert proj_out in (
        "+proj=longlat +ellps=WGS84 +no_defs",
        "+proj=longlat +datum=WGS84 +no_defs",
    )


###############################################################################
# test check sums
@pytest.mark.parametrize(
    "infile,band,checksum",
    (
        ("data/netcdf/cf_dimsindiff_4326.nc", 1, 2041),
        ("NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/data/temp", 1, 2041),
        (
            "NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/imagingGeometry/lookAngle",
            1,
            476,
        ),
        (
            "NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/imagingGeometry/lookAngle",
            4,
            476,
        ),
    ),
)
def test_netcdf_cf_7(netcdf_setup, infile, band, checksum):  # noqa
    ds = gdal.Open(infile, gdal.GA_ReadOnly)
    assert ds.GetRasterBand(band).Checksum() == checksum
