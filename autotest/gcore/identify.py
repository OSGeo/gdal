#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functioning of the IdentifyDriver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


from osgeo import gdal

###############################################################################
# Simple try of identify driver on a tiff file.


def test_identify_1():

    file_list = gdal.ReadDir("data")

    dr = gdal.IdentifyDriver("data/byte.tif", file_list)
    assert (
        dr is not None and dr.GetDescription() == "GTiff"
    ), "Got wrong driver for byte.tif"


###############################################################################
# Test a file that won't be recognised.


def test_identify_2():

    file_list = gdal.ReadDir("data")

    dr = gdal.IdentifyDriver("data/byte.pnm.aux.xml", file_list)
    assert dr is None, "Got a driver for byte.pnm.aux.xml!"


###############################################################################
# Try identify on a directory.


def test_identify_3():

    dr = gdal.IdentifyDriver("data")
    assert dr is None, "Got a driver for data directory!"


###############################################################################
# Try IdentifyDriverEx


def test_identify_4():

    dr = gdal.IdentifyDriverEx("data/byte.tif")
    assert (
        dr is not None and dr.GetDescription() == "GTiff"
    ), "Got wrong driver for byte.tif"

    dr = gdal.IdentifyDriverEx("data/byte.tif", gdal.OF_RASTER)
    assert (
        dr is not None and dr.GetDescription() == "GTiff"
    ), "Got wrong driver for byte.tif"

    dr = gdal.IdentifyDriverEx("data/byte.tif", gdal.OF_VECTOR)
    assert dr is None, "Got wrong driver for byte.tif"

    if gdal.GetDriverByName("HFA") is not None:
        dr = gdal.IdentifyDriverEx("data/byte.tif", allowed_drivers=["HFA"])
        assert dr is None, "Got wrong driver for byte.tif"

    if gdal.GetDriverByName("ENVI") is not None:
        dr = gdal.IdentifyDriverEx(
            "../gdrivers/data/envi/aea.dat", sibling_files=["aea.dat"]
        )
        assert dr is None, "Got a driver, which was not expected!"

        dr = gdal.IdentifyDriverEx(
            "../gdrivers/data/envi/aea.dat", sibling_files=["aea.dat", "aea.hdr"]
        )
        assert dr is not None, "Did not get a driver!"
