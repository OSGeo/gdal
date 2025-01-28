#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR
# Purpose:  Fix invalid GeoPackage files
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import glob
import sys

from osgeo import ogr


def fix(filename, verbose=1):
    something_done = False

    if verbose:
        print("Analyzing " + filename)
    # Fixes issue https://github.com/opengeospatial/geopackage/pull/463
    ds = ogr.Open(filename)
    lyr = ds.ExecuteSQL(
        "SELECT sql FROM sqlite_master WHERE "
        + "name = 'gpkg_metadata_reference_column_name_update' AND "
        + "sql LIKE '%NEW.column_nameIS NOT NULL%'"
    )
    buggy_sql = None
    f = lyr.GetNextFeature()
    if f:
        buggy_sql = f.GetField(0)
    ds.ReleaseResultSet(lyr)
    ds = None
    if buggy_sql:
        if verbose:
            print("  Fixing invalid gpkg_metadata_reference_column_name_update trigger")
        ds = ogr.Open(filename, update=1)
        ds.ExecuteSQL("DROP TRIGGER gpkg_metadata_reference_column_name_update")
        ds.ExecuteSQL(
            buggy_sql.replace(
                "NEW.column_nameIS NOT NULL", "NEW.column_name IS NOT NULL"
            )
        )
        ds = None
        something_done = True

    if verbose and not something_done:
        print("  Nothing to change")


def main(argv=sys.argv):
    if len(argv) != 2:
        print("Usage: fix_gpkg.py my.gpkg|*.gpkg")
        return 2

    filename = argv[1]
    if "*" in filename:
        for filename in glob.glob(filename):
            fix(filename)
    else:
        fix(filename)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
