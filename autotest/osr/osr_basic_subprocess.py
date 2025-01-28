#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test unsafe OSR operations
# Author:   Andrew Sudorgin (drons [a] list dot ru)
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

from osgeo import gdal, osr

gdal.UseExceptions()

if __name__ == "__main__":
    if sys.argv[1] == "aux_db_test":
        # This test use auxiliary database created with proj 6.3.2
        # (tested up to 8.0.0) and can be sensitive to future
        # database structure change.
        #
        # See PR https://github.com/OSGeo/gdal/pull/3590
        # Starting with sqlite 3.41, and commit
        # https://github.com/sqlite/sqlite/commit/ed07d0ea765386c5bdf52891154c70f048046e60
        # we must use the same exact table definition in the auxiliary db, otherwise
        # SQLite3 is confused regarding column types. Hence this PROJ >= 9 check,
        # to use a table structure identical to proj.db of PROJ 9.
        if osr.GetPROJVersionMajor() >= 9:
            osr.SetPROJAuxDbPath("../cpp/data/test_aux_proj_9.db")
        else:
            osr.SetPROJAuxDbPath("../cpp/data/test_aux.db")
        sr = osr.SpatialReference()
        assert sr.ImportFromEPSG(4326) == 0
        assert sr.ImportFromEPSG(111111) == 0
        exit(0)

    if sys.argv[1] == "config_option_ok":
        gdal.SetConfigOption("PROJ_DATA", sys.argv[2])
        sr = osr.SpatialReference()
        assert sr.ImportFromEPSG(4326) == 0
        exit(0)

    if sys.argv[1] == "config_option_ko":
        gdal.SetConfigOption("PROJ_DATA", sys.argv[2])
        sr = osr.SpatialReference()
        try:
            sr.ImportFromEPSG(4326)
            print("Expected exception")
            sys.exit(1)
        except Exception:
            pass
        exit(0)
