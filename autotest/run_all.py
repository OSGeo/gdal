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
sys.path.append( 'pymod' )
import gdaltest
from osgeo import gdal

test_list = []
for i in range(1,len(gdaltest.argv)):
    test_list.append( gdaltest.argv[i] )

if len(test_list) == 0:
    test_list = [ 'ogr', 'gcore', 'gdrivers', 'osr' , 'warp', 'alg', 'utilities' ]

gdaltest.setup_run( 'gdalautotest_all' )

gdaltest.run_all( test_list, [] )

errors = gdaltest.summarize()

sys.exit( errors )
