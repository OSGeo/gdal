#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id: ogr_sxf.py 26513 2013-10-02 11:59:50Z bishop $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR SXF driver functionality.
# Author:   Dmitry Baryshnikov <polimax@mail.ru>
#
###############################################################################
# Copyright (c) 2013, NextGIS
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import ogr

###############################################################################
# Open SXF datasource.

def ogr_sxf_1():

    gdaltest.sxf_ds = None
    gdaltest.sxf_ds = ogr.Open( 'data/100_test.sxf' )

    if gdaltest.sxf_ds is not None:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Run test_ogrsf

def ogr_sxf_2():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' data/100_test.sxf')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
#

def ogr_sxf_cleanup():

    if gdaltest.sxf_ds is None:
        return 'skip'

    gdaltest.sxf_ds = None

    return 'success'

gdaltest_list = [
    ogr_sxf_1,
    ogr_sxf_2,
    ogr_sxf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sxf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
