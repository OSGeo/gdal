#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  High level test executive ... it runs sub test scripts.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import sys
import os

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != '':
        os.chdir(os.path.dirname(sys.argv[0]))

sys.path.append( 'pymod' )
import gdaltest
from osgeo import gdal

all_test_list = [ 'ogr', 'gcore', 'gdrivers', 'osr' , 'alg', 'gnm', 'utilities', 'pyscripts' ]

run_as_external = False

test_list = []

for arg in gdaltest.argv[1:]:
    if arg  == '-l':
        print('List of GDAL Autotest modules')
        for test in all_test_list:
            print('* ' + test)
        sys.exit(0)
    elif arg == '-run_as_external':
        run_as_external = True
    elif arg == '-h' or arg[0] == '-':
        print('Usage: ' + sys.argv[0] + ' [OPTION]')
        print('\t<tests>          - list of test modules to run, run all if none specified')
        print('\t-l               - list available test modules')
        print('\t-h               - print this usage message')
        print('\t-run_as_external - run each test script in a dedicated Python instance')
        sys.exit(0)
    else:
        test_list.append( arg )

if len(test_list) == 0:
    test_list = all_test_list

# we set ECW to not resolve projection and datum strings to get 3.x behavior.
gdal.SetConfigOption("ECW_DO_NOT_RESOLVE_DATUM_PROJECTION", "YES")

gdaltest.setup_run( 'gdalautotest_all' )

gdaltest.run_all( test_list, run_as_external = run_as_external )

errors = gdaltest.summarize()

sys.exit( errors )

