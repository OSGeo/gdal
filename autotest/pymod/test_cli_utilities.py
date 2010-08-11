#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Helper functions for testing CLI utilities
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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

import os
import sys

try:
    from osgeo import gdal
except ImportError:
    import gdal

import gdaltest

cli_exe_path = { }

###############################################################################
# 
def get_cli_utility_path_internal(cli_utility_name):

    if sys.platform == 'win32':
        cli_utility_name = cli_utility_name + '.exe'

    # First try : in the apps directory of the GDAL source tree
    # This is the case for the buildbot directory tree
    try:
        cli_utility_path = os.path.join(os.getcwd(), '..', '..', 'gdal', 'apps', cli_utility_name)
        if os.path.isfile(cli_utility_path):
            ret = gdaltest.runexternal(cli_utility_path + ' --utility_version')

            if ret.find('GDAL') != -1:
                return cli_utility_path
    except:
        pass

    # Second try : the autotest directory is a subdirectory of gdal/ (FrankW's layout)
    try:
        cli_utility_path = os.path.join(os.getcwd(), '..', '..', 'apps', cli_utility_name)
        if os.path.isfile(cli_utility_path):
            ret = gdaltest.runexternal(cli_utility_path + ' --utility_version')

            if ret.find('GDAL') != -1:
                return cli_utility_path
    except:
        pass

    # Otherwise look up in the system path
    try:
        cli_utility_path = cli_utility_name
        ret = gdaltest.runexternal(cli_utility_path + ' --utility_version')

        if ret.find('GDAL') != -1:
            return cli_utility_path
    except:
        pass

    return None

###############################################################################
# 
def get_cli_utility_path(cli_utility_name):
    global cli_exe_path
    if cli_utility_name in cli_exe_path:
        return cli_exe_path[cli_utility_name]
    else:
        cli_exe_path[cli_utility_name] = get_cli_utility_path_internal(cli_utility_name)
        return cli_exe_path[cli_utility_name]

###############################################################################
# 
def get_gdalinfo_path():
    return get_cli_utility_path('gdalinfo')

###############################################################################
# 
def get_gdal_translate_path():
    return get_cli_utility_path('gdal_translate')

###############################################################################
# 
def get_gdalwarp_path():
    return get_cli_utility_path('gdalwarp')

###############################################################################
# 
def get_gdaladdo_path():
    return get_cli_utility_path('gdaladdo')

###############################################################################
# 
def get_gdaltransform_path():
    return get_cli_utility_path('gdaltransform')

###############################################################################
# 
def get_gdaltindex_path():
    return get_cli_utility_path('gdaltindex')

###############################################################################
# 
def get_gdal_grid_path():
    return get_cli_utility_path('gdal_grid')

###############################################################################
# 
def get_ogrinfo_path():
    return get_cli_utility_path('ogrinfo')

###############################################################################
# 
def get_ogr2ogr_path():
    return get_cli_utility_path('ogr2ogr')

###############################################################################
# 
def get_ogrtindex_path():
    return get_cli_utility_path('ogrtindex')

###############################################################################
# 
def get_gdalbuildvrt_path():
    return get_cli_utility_path('gdalbuildvrt')

###############################################################################
# 
def get_gdal_contour_path():
    return get_cli_utility_path('gdal_contour')

###############################################################################
# 
def get_gdaldem_path():
    return get_cli_utility_path('gdaldem')

###############################################################################
# 
def get_gdal_rasterize_path():
    return get_cli_utility_path('gdal_rasterize')

###############################################################################
# 
def get_nearblack_path():
    return get_cli_utility_path('nearblack')

###############################################################################
#
def get_test_ogrsf_path():
    return get_cli_utility_path('test_ogrsf')
