#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test unsafe OSR operations
# Author:   Andrew Sudorgin (drons [a] list dot ru)
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
