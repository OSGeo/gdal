#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CF1 spatial reference implementation.
# Author:   Even Rouault, <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault, <even.rouault at spatialys.com>
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

###############################################################################


def test_osr_cf1_transverse_mercator():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    d = sr.ExportToCF1()
    assert d["grid_mapping_name"] == "transverse_mercator"
    assert d["longitude_of_central_meridian"] == 3
    assert d["false_easting"] == 500000
    assert d["false_northing"] == 0
    assert d["latitude_of_projection_origin"] == 0
    assert d["scale_factor_at_central_meridian"] == 0.9996
    assert d["longitude_of_prime_meridian"] == 0
    assert d["semi_major_axis"] == 6378137
    assert d["inverse_flattening"] == 298.257223563
    assert d["long_name"] == "CRS definition"
    assert "crs_wkt" in d

    assert sr.ExportToCF1Units() == "m"

    sr2 = osr.SpatialReference()
    del d["crs_wkt"]
    assert sr2.ImportFromCF1(d, "m") == ogr.OGRERR_NONE
    got_d = sr2.ExportToCF1()
    del got_d["crs_wkt"]
    assert got_d == d


###############################################################################


def test_osr_cf1_lcc_2sp():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3949)
    d = sr.ExportToCF1()
    assert d["grid_mapping_name"] == "lambert_conformal_conic"
    assert d["longitude_of_central_meridian"] == 3
    assert d["false_easting"] == 1700000
    assert d["false_northing"] == 8200000
    assert d["latitude_of_projection_origin"] == 49
    assert d["standard_parallel"] == [48.25, 49.75]
    assert d["longitude_of_prime_meridian"] == 0
    assert d["semi_major_axis"] == 6378137
    assert d["inverse_flattening"] == 298.257222101
    assert d["long_name"] == "CRS definition"
    assert "crs_wkt" in d

    sr2 = osr.SpatialReference()
    del d["crs_wkt"]
    assert sr2.ImportFromCF1(d) == ogr.OGRERR_NONE
    got_d = sr2.ExportToCF1()
    del got_d["crs_wkt"]
    assert got_d == d


###############################################################################


def test_osr_cf1_import_from_spatial_ref_attribute():

    wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]"""
    sr = osr.SpatialReference()
    assert sr.ImportFromCF1({"spatial_ref": wkt}) == ogr.OGRERR_NONE
    assert sr.GetAuthorityCode(None) == "4326"


###############################################################################


def test_osr_cf1_import_from_crs_wkt_attribute():

    wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]"""
    sr = osr.SpatialReference()
    assert sr.ImportFromCF1({"crs_wkt": wkt}) == ogr.OGRERR_NONE
    assert sr.GetAuthorityCode(None) == "4326"
