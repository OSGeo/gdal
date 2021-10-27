#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR
# Purpose:  Fix invalid GeoPackage files
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
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

import glob
import sys
from osgeo import ogr


def fix(filename, verbose = 1):
    something_done = False

    if verbose:
        print('Analyzing ' + filename)
    # Fixes issue https://github.com/opengeospatial/geopackage/pull/463
    ds = ogr.Open(filename)
    lyr = ds.ExecuteSQL("SELECT sql FROM sqlite_master WHERE " +
                        "name = 'gpkg_metadata_reference_column_name_update' AND " +
                        "sql LIKE '%NEW.column_nameIS NOT NULL%'")
    buggy_sql = None
    f = lyr.GetNextFeature()
    if f:
        buggy_sql = f.GetField(0)
    ds.ReleaseResultSet(lyr)
    ds = None
    if buggy_sql:
        if verbose:
            print('  Fixing invalid gpkg_metadata_reference_column_name_update trigger')
        ds = ogr.Open(filename, update=1)
        ds.ExecuteSQL(
            'DROP TRIGGER gpkg_metadata_reference_column_name_update')
        ds.ExecuteSQL(buggy_sql.replace('NEW.column_nameIS NOT NULL',
                                        'NEW.column_name IS NOT NULL'))
        ds = None
        something_done = True

    if verbose and not something_done:
        print('  Nothing to change')


def main(argv):
    if len(argv) != 2:
        print('Usage: fix_gpkg.py my.gpkg|*.gpkg')
        return 1

    filename = argv[1]
    if '*' in filename:
        for filename in glob.glob(filename):
            fix(filename)
    else:
        fix(filename)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
