#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalsrsinfo testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
import test_cli_utilities

###############################################################################
# Simple test

def test_gdalsrsinfo_1():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalsrsinfo_path() + ' ../gcore/data/byte.tif')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    if ret.find('PROJ.4 :') == -1:
        return 'fail'
    if ret.find('OGC WKT :') == -1:
        return 'fail'

    return 'success'
 
###############################################################################
# Test -o proj4 option

def test_gdalsrsinfo_2():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o proj4 ../gcore/data/byte.tif')

    if ret.strip() != "'+proj=utm +zone=11 +datum=NAD27 +units=m +no_defs '":
        return 'fail'

    return 'success'

###############################################################################
# Test -o wkt option

def test_gdalsrsinfo_3():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o wkt ../gcore/data/byte.tif')

    first_val =  'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982139006,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","26711"]]'
    second_val = 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","26711"]]'
    if ret.strip() != first_val and ret.strip() != second_val:
        print(ret.strip())
        return 'fail'


    return 'success'

###############################################################################
# Test -o wkt_esri option

def test_gdalsrsinfo_4():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o wkt_esri ../gcore/data/byte.tif')

    if ret.strip() != 'PROJCS["NAD_1927_UTM_Zone_11N",GEOGCS["GCS_North_American_1927",DATUM["D_North_American_1927",SPHEROID["Clarke_1866",6378206.4,294.9786982]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]':
        return 'fail'

    return 'success'

###############################################################################
# Test -o wkt_old option

def test_gdalsrsinfo_5():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o wkt_noct ../gcore/data/byte.tif')

    first_val =  'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982139006]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1]]'
    second_val = 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1]]'
    if ret.strip() != first_val and ret.strip() != second_val:
        print(ret.strip())
        return 'fail'

    return 'success'

###############################################################################
# Test -o wkt_simple option

def test_gdalsrsinfo_6():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o wkt_simple ../gcore/data/byte.tif')
    ret = ret.replace('\r\n', '\n')

    first_val =  """PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982139006]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1]]"""
    second_val =  """PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1]]"""

    if ret.strip() != first_val and ret.strip() != second_val:
        print(ret.strip())
        return 'fail'


    return 'success'

###############################################################################
# Test -o mapinfo option

def test_gdalsrsinfo_7():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o mapinfo ../gcore/data/byte.tif')

    if ret.strip() != """'Earth Projection 8, 62, "m", -117, 0, 0.9996, 500000, 0'""":
        return 'fail'

    return 'success'


###############################################################################
# Test -p option

def test_gdalsrsinfo_8():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o wkt -p EPSG:4326')
    ret = ret.replace('\r\n', '\n')

    if ret.strip() != """GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AUTHORITY["EPSG","4326"]]""":
        return 'fail'

    return 'success'


###############################################################################
# Test inexistent file

def test_gdalsrsinfo_9():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    (ret,err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' inexistent_file')

    if err.strip() != "ERROR 1: ERROR - failed to load SRS definition from inexistent_file":
        return 'fail'

    return 'success'


###############################################################################
# Test -V option - valid

def test_gdalsrsinfo_10():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'
    
    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]'
    if sys.platform == 'win32':
        # Win32 shell quoting oddities
        wkt = wkt.replace('"', '\\"')
        ret = gdaltest.runexternal( test_cli_utilities.get_gdalsrsinfo_path() + \
                                   " -V -o proj4 \"" + wkt + "\"" )
    else:
        ret = gdaltest.runexternal( test_cli_utilities.get_gdalsrsinfo_path() + \
                                   " -V -o proj4 '" + wkt + "'" )

    if ret.find('Validate Succeeds') == -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test -V option - invalid

def test_gdalsrsinfo_11():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'
    
    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],BADAUTHORITY["EPSG","4326"]]'
    if sys.platform == 'win32':
        # Win32 shell quoting oddities
        wkt = wkt.replace('"', '\\"')
        ret = gdaltest.runexternal( test_cli_utilities.get_gdalsrsinfo_path() + \
                                   " -V -o proj4 \"" + wkt + "\"" )
    else:
        ret = gdaltest.runexternal( test_cli_utilities.get_gdalsrsinfo_path() + \
                                   " -V -o proj4 '" + wkt + "'" )

    if ret.find('Validate Fails') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test EPSG:epsg format

def test_gdalsrsinfo_12():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o wkt EPSG:4326')

    if ret.strip() != """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]""":
        return 'fail'

    return 'success'


###############################################################################
# Test proj4 format

def test_gdalsrsinfo_13():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o wkt "+proj=longlat +datum=WGS84 +no_defs"')
 
    if ret.strip() != """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]""":
        return 'fail'

    return 'success'

###############################################################################
# Test VSILFILE format

def test_gdalsrsinfo_14():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o proj4 /vsizip/../gcore/data/byte.tif.zip')
 
    if ret.strip() != "'+proj=utm +zone=11 +datum=NAD27 +units=m +no_defs '":
        return 'fail'

    return 'success'

###############################################################################
# Test .shp format

def test_gdalsrsinfo_14bis():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o proj4 ../ogr/data/Stacks.shp')
 
    if ret.strip() != "'+proj=lcc +lat_1=28.38333333333333 +lat_2=30.28333333333334 +lat_0=27.83333333333333 +lon_0=-99 +x_0=600000.0000000001 +y_0=4000000 +datum=NAD83 +units=us-ft +no_defs '":
        return 'fail'

    return 'success'

###############################################################################
# Test .prj format

def test_gdalsrsinfo_15():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalsrsinfo_path() + \
                                   ' -o proj4 ../osr/data/lcc_esri.prj')
 
    if ret.strip() != "'+proj=lcc +lat_1=34.33333333333334 +lat_2=36.16666666666666 +lat_0=33.75 +lon_0=-79 +x_0=609601.22 +y_0=0 +datum=NAD83 +units=m +no_defs '":
        return 'fail'

    return 'success'

###############################################################################
# Test DRIVER:file syntax (bug #4493) -  similar test should be done with OGR

def test_gdalsrsinfo_16():
    if test_cli_utilities.get_gdalsrsinfo_path() is None:
        return 'skip'

    cmd = test_cli_utilities.get_gdalsrsinfo_path() +\
        ' GTIFF_RAW:../gcore/data/byte.tif'

    try:
        (ret, err) = gdaltest.runexternal_out_and_err( cmd )
    except:
        gdaltest.post_reason('gdalsrsinfo execution failed')
        return 'fail'

    if err != '':
        return 'fail'

    return 'success'
 

###############################################################################
#

gdaltest_list = [
    test_gdalsrsinfo_1,
    test_gdalsrsinfo_2,
    test_gdalsrsinfo_3,
    test_gdalsrsinfo_4,
    test_gdalsrsinfo_5,
    test_gdalsrsinfo_6,
    test_gdalsrsinfo_7,
    test_gdalsrsinfo_8,
    test_gdalsrsinfo_9,
    test_gdalsrsinfo_10,
    test_gdalsrsinfo_11,
    test_gdalsrsinfo_12,
    test_gdalsrsinfo_13,
    test_gdalsrsinfo_14,
    test_gdalsrsinfo_14bis,
    test_gdalsrsinfo_15,
    test_gdalsrsinfo_16,
    None
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalsrsinfo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





