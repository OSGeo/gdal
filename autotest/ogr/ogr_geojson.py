#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GeoJSON driver testing.
# Author:   Mateusz Loskot <mateusz@loskot.net>
# 
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
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

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal

###############################################################################
# Find GeoJSON driver

def ogr_geojson_1():

    geojson_drv = ogr.GetDriverByName('GeoJSON')
    
    if geojson_drv is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# 

def ogr_geojson_cleanup():

    return 'success'

gdaltest_list = [ 
    ogr_geojson_1,
    ogr_geojson_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geojson' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

