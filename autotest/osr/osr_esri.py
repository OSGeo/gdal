#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some ESRI specific translation issues.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
# Copyright (c) 2013, Kyle Shannon <kyle at pobox dot com>
# Copyright (c) 2014, Google
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
import csv
import gzip

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal, osr

###############################################################################
# This test verifies that morphToESRI() translates ideosyncratic datum names
# from "EPSG" form to ESRI from when the exception list comes from the
# gdal_datum.csv file.

def osr_esri_1():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4202 )

    if srs.GetAttrValue( 'DATUM' ) != 'Australian_Geodetic_Datum_1966':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after EPSG import.' %\
                              srs.GetAttrValue( 'DATUM' ) )
        return 'fail'

    srs.MorphToESRI()

    if srs.GetAttrValue( 'DATUM' ) != 'D_Australian_1966':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'DATUM' ) )
        return 'fail'

    srs.MorphFromESRI()

    if srs.GetAttrValue( 'DATUM' ) != 'Australian_Geodetic_Datum_1966':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after ESRI unmorph.' %\
                              srs.GetAttrValue( 'DATUM' ) )
        return 'fail'

    return 'success'

###############################################################################
# Verify that exact correct form of UTM names is established when
# translating certain GEOGCSes to ESRI format.

def osr_esri_2():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( '+proj=utm +zone=11 +south +datum=WGS84' )

    srs.MorphToESRI()

    if srs.GetAttrValue( 'GEOGCS' ) != 'GCS_WGS_1984':
        gdaltest.post_reason( 'Got wrong GEOGCS name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'GEOGCS' ) )
        return 'fail'

    if srs.GetAttrValue( 'PROJCS' ) != 'WGS_1984_UTM_Zone_11S':
        gdaltest.post_reason( 'Got wrong PROJCS name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJCS' ) )
        return 'fail'

    return 'success'

###############################################################################
# Verify that Unnamed is changed to Unknown in morphToESRI().

def osr_esri_3():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( '+proj=mill +datum=WGS84' )

    srs.MorphToESRI()

    if srs.GetAttrValue( 'PROJCS' ) != 'Miller_Cylindrical':
        gdaltest.post_reason( 'Got wrong PROJCS name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJCS' ) )
        return 'fail'

    return 'success'

###############################################################################
# Verify Polar Stereographic translations work properly OGR to ESRI.

def osr_esri_4():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'PROJCS["PS Test",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_origin",-80.2333],PARAMETER["central_meridian",171],PARAMETER["scale_factor",0.9999],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]' )

    srs.MorphToESRI()

    if srs.GetAttrValue( 'PROJECTION' ) != 'Stereographic_South_Pole':
        gdaltest.post_reason( 'Got wrong PROJECTION name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJECTION' ) )
        return 'fail'

    if srs.GetProjParm('standard_parallel_1') != -80.2333:
        gdaltest.post_reason( 'Got wrong parameter value (%g) after ESRI morph.' %\
                              srs.GetProjParm('standard_parallel_1') )
        return 'fail'

    return 'success'

###############################################################################
# Verify Polar Stereographic translations work properly ESRI to OGR.

def osr_esri_5():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'PROJCS["PS Test",GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Stereographic_South_Pole"],PARAMETER["standard_parallel_1",-80.2333],PARAMETER["central_meridian",171],PARAMETER["scale_factor",0.9999],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]' )

    srs.MorphFromESRI()

    if srs.GetAttrValue( 'PROJECTION' ) != 'Polar_Stereographic':
        gdaltest.post_reason( 'Got wrong PROJECTION name (%s) after ESRI morph.' %\
                              srs.GetAttrValue( 'PROJECTION' ) )
        return 'fail'

    if srs.GetProjParm('latitude_of_origin') != -80.2333:
        gdaltest.post_reason( 'Got wrong parameter value (%g) after ESRI morph.' %\
                              srs.GetProjParm('latitude_of_origin') )
        return 'fail'

    return 'success'

###############################################################################
# Verify Lambert 2SP with a 1.0 scale factor still gets translated to 2SP
# per bug 187.

def osr_esri_6():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'PROJCS["Texas Centric Mapping System/Lambert Conformal",GEOGCS["GCS_North_American_1983",DATUM["D_North_American_1983",SPHEROID["GRS_1980",6378137.0,298.257222101]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic"],PARAMETER["False_Easting",1500000.0],PARAMETER["False_Northing",5000000.0],PARAMETER["Central_Meridian",-100.0],PARAMETER["Standard_Parallel_1",27.5],PARAMETER["Standard_Parallel_2",35.0],PARAMETER["Scale_Factor",1.0],PARAMETER["Latitude_Of_Origin",18.0],UNIT["Meter",1.0]]' )

    srs.MorphFromESRI()

    if srs.GetAttrValue( 'PROJECTION' ) != 'Lambert_Conformal_Conic_2SP':
        gdaltest.post_reason( \
            'Got wrong PROJECTION name (%s) after ESRI morph, expected 2SP' %\
            srs.GetAttrValue( 'PROJECTION' ) )
        return 'fail'

    return 'success'

###############################################################################
# Verify that FEET is treated as US survey feet per bug #1533.

def osr_esri_7():

    prj = [ 'Projection    STATEPLANE',
            'Fipszone      903',
            'Datum         NAD83',
            'Spheroid      GRS80',
            'Units         FEET',
            'Zunits        NO',
            'Xshift        0.0',
            'Yshift        0.0',
            'Parameters    ',
            '' ]

    srs_prj = osr.SpatialReference()
    srs_prj.ImportFromESRI( prj )

    wkt = """PROJCS["NAD83 / Florida North",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.01745329251994328,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4269"]],
    UNIT["Foot_US",0.3048006096012192],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",30.75],
    PARAMETER["standard_parallel_2",29.58333333333333],
    PARAMETER["latitude_of_origin",29],
    PARAMETER["central_meridian",-84.5],
    PARAMETER["false_easting",1968500],
    PARAMETER["false_northing",0],
    AXIS["X",EAST],
    AXIS["Y",NORTH]]"""

    srs_wkt = osr.SpatialReference(wkt = wkt)

    if not srs_prj.IsSame( srs_wkt ):
        print('got: ', srs_prj.ExportToPrettyWkt())
        gdaltest.post_reason( 'old style ESRI projection imported wrong, perhaps linear units?' )
        return 'fail'

    return 'success'

###############################################################################
# Verify that handling of numerically specified units (see bug #1533)

def osr_esri_8():

    prj = [ 'Projection    STATEPLANE',
            'Fipszone      903',
            'Datum         NAD83',
            'Spheroid      GRS80',
            'Units         3.280839895013123',
            'Zunits        NO',
            'Xshift        0.0',
            'Yshift        0.0',
            'Parameters    ',
            '' ]

    srs_prj = osr.SpatialReference()
    srs_prj.ImportFromESRI( prj )

    wkt = """PROJCS["NAD83 / Florida North",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.01745329251994328,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",30.75],
    PARAMETER["standard_parallel_2",29.58333333333333],
    PARAMETER["latitude_of_origin",29],
    PARAMETER["central_meridian",-84.5],
    PARAMETER["false_easting",1968503.937007874],
    PARAMETER["false_northing",0],
    UNIT["user-defined",0.3048],
    AUTHORITY["EPSG","26960"]]"""

    srs_wkt = osr.SpatialReference(wkt = wkt)

    if not srs_prj.IsSame( srs_wkt ):
        gdaltest.post_reason( 'old style ESRI projection imported wrong, perhaps linear units?' )
        return 'fail'

    return 'success'

###############################################################################
# Verify Equidistant Conic handling.

def osr_esri_9():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'PROJCS["edc",GEOGCS["GCS_North_American_1983",DATUM["D_North_American_1983",SPHEROID["GRS_1980",6378137.0,298.257222101]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Equidistant_Conic"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-96.0],PARAMETER["Standard_Parallel_1",29.5],PARAMETER["Standard_Parallel_2",45.5],PARAMETER["Latitude_Of_Origin",37.5],UNIT["Meter",1.0]]' )

    expected = 'PROJCS["edc",GEOGCS["GCS_North_American_1983",DATUM["North_American_Datum_1983",SPHEROID["GRS_1980",6378137.0,298.257222101]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Equidistant_Conic"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["longitude_of_center",-96.0],PARAMETER["Standard_Parallel_1",29.5],PARAMETER["Standard_Parallel_2",45.5],PARAMETER["latitude_of_center",37.5],UNIT["Meter",1.0]]'

    srs.MorphFromESRI()
    wkt = srs.ExportToWkt()
    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected Equidistant Conic SRS after morphFromESRI' )
        return 'fail'

    expected = 'PROJCS["edc",GEOGCS["GCS_North_American_1983",DATUM["D_North_American_1983",SPHEROID["GRS_1980",6378137.0,298.257222101]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.017453292519943295]],PROJECTION["Equidistant_Conic"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["central_meridian",-96.0],PARAMETER["Standard_Parallel_1",29.5],PARAMETER["Standard_Parallel_2",45.5],PARAMETER["latitude_of_origin",37.5],UNIT["Meter",1.0]]'

    srs.MorphToESRI()
    wkt = srs.ExportToWkt()
    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected Equidistant Conic SRS after morphToESRI' )
        return 'fail'

    return 'success'

###############################################################################
# Verify Plate_Carree handling.

def osr_esri_10():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'PROJCS["Sphere_Plate_Carree",GEOGCS["GCS_Sphere",DATUM["D_Sphere",SPHEROID["Sphere",6371000.0,0.0]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Plate_Carree"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],UNIT["Meter",1.0]]' )

    expected = 'PROJCS["Sphere_Plate_Carree",GEOGCS["GCS_Sphere",DATUM["Not_specified_based_on_Authalic_Sphere",SPHEROID["Sphere",6371000.0,0.0]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],UNIT["Meter",1.0]]'

    srs.MorphFromESRI()
    wkt = srs.ExportToWkt()
    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected Equirectangular SRS after morphFromESRI' )
        return 'fail'

    expected = 'PROJCS["Sphere_Plate_Carree",GEOGCS["GCS_Sphere",DATUM["D_Sphere",SPHEROID["Sphere",6371000.0,0.0]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.017453292519943295]],PROJECTION["Equidistant_Cylindrical"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],UNIT["Meter",1.0]]'

    srs.MorphToESRI()
    wkt = srs.ExportToWkt()
    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected Equidistant_Cylindrical SRS after morphToESRI' )
        return 'fail'

    return 'success'

###############################################################################
# Verify arc/info style TM handling.

def osr_esri_11():

    srs = osr.SpatialReference()
    srs.ImportFromESRI( [ 'Projection    TRANSVERSE',
                          'Datum         NAD27',
                          'Spheroid      CLARKE1866',
                          'Units         METERS',
                          'Zunits        NO',
                          'Xshift        0.0',
                          'Yshift        0.0',
                          'Parameters   ',
                          '1.0 /* scale factor at central meridian',
                          '-106 56  0.5 /* longitude of central meridian',
                          '  39 33 30 /* latitude of origin',
                          '0.0 /* false easting (meters)',
                          '0.0 /* false northing (meters)' ] )

    expected = 'PROJCS["unnamed",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",39.55833333333333],PARAMETER["central_meridian",-106.9334722222222],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["METERS",1]]'

    srs.MorphFromESRI()
    wkt = srs.ExportToWkt()
    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected TM SRS after morphFromESRI' )
        return 'fail'

    return 'success'

###############################################################################
# Test automatic morphing of ESRI-style LCC WKT prefixed with 'ESRI::'

def osr_esri_12():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'ESRI::PROJCS["Lambert Conformal Conic",GEOGCS["grs80",DATUM["D_North_American_1983",SPHEROID["Geodetic_Reference_System_1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Lambert_Conformal_Conic"],PARAMETER["standard_parallel_1",34.33333333333334],PARAMETER["standard_parallel_2",36.16666666666666],PARAMETER["latitude_of_origin",33.75],PARAMETER["central_meridian",-79],PARAMETER["false_easting",609601.22],PARAMETER["false_northing",0],UNIT["Meter",1]]' )

    # No MorphFromESRI() is required

    if srs.GetAttrValue( 'PROJECTION' ) != 'Lambert_Conformal_Conic_2SP':
        gdaltest.post_reason( 'Got wrong PROJECTION name (%s) after ESRI morph.' % \
                              srs.GetAttrValue( 'PROJECTION' ) )
        return 'fail'

    if abs( srs.GetProjParm('standard_parallel_1') - 34.333333333 ) > 0.00001:
        gdaltest.post_reason( 'Got wrong parameter value (%g) after ESRI morph.' % \
                              srs.GetProjParm('standard_parallel_1') )
        return 'fail'

    if srs.GetAttrValue( 'DATUM' ) != 'North_American_Datum_1983':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after ESRI morph.' % \
                              srs.GetAttrValue( 'DATUM' ) )

    if srs.GetAttrValue( 'UNIT' ) != 'Meter':
        gdaltest.post_reason( 'Got wrong UNIT name (%s) after ESRI morph.' % \
                              srs.GetAttrValue( 'UNIT' ) )
        return 'fail'

    return 'success'

###############################################################################
# Test automatic morphing of ESRI-style LCC WKT prefixed with 'ESRI::'
# but read directly from file.

def osr_esri_13():

    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'data/lcc_esri.prj' )

    # No MorphFromESRI() is required

    if srs.GetAttrValue( 'PROJECTION' ) != 'Lambert_Conformal_Conic_2SP':
        gdaltest.post_reason( 'Got wrong PROJECTION name (%s) after ESRI morph.' % \
                              srs.GetAttrValue( 'PROJECTION' ) )
        return 'fail'

    if abs( srs.GetProjParm('standard_parallel_1') - 34.333333333 ) > 0.00001:
        gdaltest.post_reason( 'Got wrong parameter value (%g) after ESRI morph.' % \
                              srs.GetProjParm('standard_parallel_1') )
        return 'fail'

    if srs.GetAttrValue( 'DATUM' ) != 'North_American_Datum_1983':
        gdaltest.post_reason( 'Got wrong DATUM name (%s) after ESRI morph.' % \
                              srs.GetAttrValue( 'DATUM' ) )

    if srs.GetAttrValue( 'UNIT' ) != 'Meter':
        gdaltest.post_reason( 'Got wrong UNIT name (%s) after ESRI morph.' % \
                              srs.GetAttrValue( 'UNIT' ) )
        return 'fail'

    return 'success'


###############################################################################
# Verify that state plane epsg authority values are not applied if the
# linear units are changed for old style .prj files (bug #1697)

def osr_esri_14():

    srs = osr.SpatialReference()
    srs.ImportFromESRI( [ 'PROJECTION STATEPLANE',
                          'UNITS feet',
                          'FIPSZONE 2600',
                          'DATUM NAD83',
                          'PARAMETERS' ] )
    if srs.GetAuthorityCode( 'PROJCS' ) != None:
        print(srs.GetAuthorityCode( 'PROJCS' ))
        gdaltest.post_reason( 'Get epsg authority code inappropriately.' )
        return 'fail'

    srs = osr.SpatialReference()
    srs.ImportFromESRI( [ 'PROJECTION STATEPLANE',
                          'UNITS meter',
                          'FIPSZONE 2600',
                          'DATUM NAD83',
                          'PARAMETERS' ] )
    if srs.GetAuthorityCode( 'PROJCS' ) != '32104':
        print(srs.GetAuthorityCode( 'PROJCS' ))
        gdaltest.post_reason( 'Did not get epsg authority code when expected.')
        return 'fail'

    return 'success'

###############################################################################
# Verify hotine oblique mercator handling, particularly handling
# of the rectified_grid_angle parameter.

def osr_esri_15():

    srs = osr.SpatialReference()
    srs.SetFromUserInput('PROJCS["Bern_1898_Bern_LV03C",GEOGCS["GCS_Bern_1898_Bern",DATUM["D_Bern_1898",SPHEROID["Bessel_1841",6377397.155,299.1528128]],PRIMEM["Bern",7.439583333333333],UNIT["Degree",0.0174532925199433]],PROJECTION["Hotine_Oblique_Mercator_Azimuth_Center"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Scale_Factor",1.0],PARAMETER["Azimuth",90.0],PARAMETER["Longitude_Of_Center",0.0],PARAMETER["Latitude_Of_Center",46.95240555555556],UNIT["Meter",1.0]]' )

    expected = 'PROJCS["Bern_1898_Bern_LV03C",GEOGCS["GCS_Bern_1898_Bern",DATUM["D_Bern_1898",SPHEROID["Bessel_1841",6377397.155,299.1528128]],PRIMEM["Bern",7.439583333333333],UNIT["Degree",0.017453292519943295]],PROJECTION["Hotine_Oblique_Mercator_Azimuth_Center"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Scale_Factor",1.0],PARAMETER["Azimuth",90.0],PARAMETER["Longitude_Of_Center",0.0],PARAMETER["Latitude_Of_Center",46.95240555555556],UNIT["Meter",1.0]]'


    srs.MorphFromESRI()
    wkt = srs.ExportToWkt()

    if wkt.find('rectified_grid_angle') == -1:
        print(wkt)
        gdaltest.post_reason( 'Did not get rectified_grid_angle as expected.')
        return 'fail'

    srs.MorphToESRI()
    wkt = srs.ExportToWkt()
    if wkt.find('rectified_grid_angle') != -1:
        gdaltest.post_reason('did not get rectified_grid_angle removed as expected.' )
        return 'fail'

    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected HOM projection after morphing' )
        return 'fail'

    return 'success'

###############################################################################
# Verify translation of equirectngular to equidistant cylindrical with
# cleanup of parameters.

def osr_esri_16():

    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=eqc +lat_0=0 +lat_ts=-10 +lon_0=2 +x=100000 +y_0=200000 +ellps=sphere')

    expected = 'PROJCS["Equidistant_Cylindrical",GEOGCS["GCS_Normal Sphere (r=6370997)",DATUM["D_unknown",SPHEROID["sphere",6370997,0]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Equidistant_Cylindrical"],PARAMETER["central_meridian",2],PARAMETER["standard_parallel_1",-10],PARAMETER["false_easting",0],PARAMETER["false_northing",200000],UNIT["Meter",1]]'

    srs.MorphToESRI()
    wkt = srs.ExportToWkt()

    if expected != wkt:
        print(wkt)
        gdaltest.post_reason( 'Did not get expected equidistant cylindrical.' )
        return 'fail'

    return 'success'


###############################################################################
# Test LAEA support (#3017)

def osr_esri_17():

    original = 'PROJCS["ETRS89 / ETRS-LAEA",GEOGCS["ETRS89",DATUM["European_Terrestrial_Reference_System_1989",SPHEROID["GRS 1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]],UNIT["metre",1],PROJECTION["Lambert_Azimuthal_Equal_Area"],PARAMETER["latitude_of_center",52],PARAMETER["longitude_of_center",10],PARAMETER["false_easting",4321000],PARAMETER["false_northing",3210000]]'
    srs = osr.SpatialReference()
    srs.SetFromUserInput( original )

    expected = 'PROJCS["ETRS89_ETRS_LAEA",GEOGCS["GCS_ETRS_1989",DATUM["D_ETRS_1989",SPHEROID["GRS_1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Lambert_Azimuthal_Equal_Area"],PARAMETER["latitude_of_origin",52],PARAMETER["central_meridian",10],PARAMETER["false_easting",4321000],PARAMETER["false_northing",3210000],UNIT["Meter",1]]'

    srs.MorphToESRI()
    wkt = srs.ExportToWkt()
    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected LAEA SRS after morphToESRI' )
        return 'fail'

    expected = 'PROJCS["ETRS89_ETRS_LAEA",GEOGCS["GCS_ETRS_1989",DATUM["European_Terrestrial_Reference_System_1989",SPHEROID["GRS_1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Lambert_Azimuthal_Equal_Area"],PARAMETER["latitude_of_center",52],PARAMETER["longitude_of_center",10],PARAMETER["false_easting",4321000],PARAMETER["false_northing",3210000],UNIT["Meter",1]]'

    srs.MorphFromESRI()
    wkt = srs.ExportToWkt()
    if wkt != expected:
        print('')
        print('Got:      ', wkt)
        print('Expected: ', expected)
        gdaltest.post_reason( 'Did not get expected LAEA SRS after morphFromESRI' )
        return 'fail'

    return 'success'

###############################################################################
# Test EC morphing.

def osr_esri_18():

    original = """PROJCS["World_Equidistant_Cylindrical",
    GEOGCS["GCS_WGS_1984",
      DATUM["D_WGS_1984",
        SPHEROID["WGS_1984",6378137,298.257223563]],
      PRIMEM["Greenwich",0],
      UNIT["Degree",0.017453292519943295]],
    PROJECTION["Equidistant_Cylindrical"],
    PARAMETER["False_Easting",0],
    PARAMETER["False_Northing",0],
    PARAMETER["Central_Meridian",0],
    PARAMETER["Standard_Parallel_1",60],
    UNIT["Meter",1]]"""

    srs = osr.SpatialReference()
    srs.SetFromUserInput( original )

    expected = 'PROJCS["World_Equidistant_Cylindrical",GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_1984",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Equirectangular"],PARAMETER["False_Easting",0],PARAMETER["False_Northing",0],PARAMETER["Central_Meridian",0],PARAMETER["standard_parallel_1",60],UNIT["Meter",1]]'

    srs.MorphFromESRI()

    srs_expected = osr.SpatialReference( wkt = expected )

    if not srs.IsSame(srs_expected):
        print('')
        print('Got:      ', srs.ExportToPrettyWkt())
        print('Expected: ', srs_expected.ExportToPrettyWkt())
        gdaltest.post_reason( 'Did not get expected EC SRS after morphFromESRI' )
        return 'fail'

    srs.MorphToESRI()
    srs_expected = osr.SpatialReference( wkt = original )

    if not srs.IsSame(srs_expected):
        print('')
        print('Got:      ', srs.ExportToPrettyWkt())
        print('Expected: ', srs_expected.ExportToPrettyWkt())
        gdaltest.post_reason( 'Did not get expected EC SRS after morphToESRI' )
        return 'fail'

    return 'success'

###############################################################################
# Test spheroid remapping (per #3904)

def osr_esri_19():

    original = """GEOGCS["GCS_South_American_1969",DATUM["D_South_American_1969",SPHEROID["GRS_1967_Truncated",6378160.0,298.25]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]]"""

    srs = osr.SpatialReference()
    srs.SetFromUserInput( original )

    srs.MorphFromESRI()

    expected = 'GRS_1967_Modified'
    if srs.GetAttrValue( 'SPHEROID' ) != expected:
        print('')
        print('Got:      ', srs.ExportToPrettyWkt())
        gdaltest.post_reason( 'Did not get expected spheroid name after morphFromESRI' )
        return 'fail'

    srs.MorphToESRI()

    expected = 'GRS_1967_Truncated'
    if srs.GetAttrValue( 'SPHEROID' ) != expected:
        print('')
        print('Got:      ', srs.ExportToPrettyWkt())
        gdaltest.post_reason( 'Did not get expected spheroid name after morphToESRI' )
        return 'fail'

    return 'success'

###############################################################################
# Test esri->ogc, esri->proj / ogc->esri, ogc->proj / proj->esri, proj->ogc
def osr_ersi_test( wkt_esri, wkt_ogc, proj4 ):

    silent = True
    #silent = False

    result = 'success'
    srs_esri = osr.SpatialReference()
    srs_ogc = osr.SpatialReference()

    if not silent:
        print( 'osr_esri_test( ) \nwkt_esri='+wkt_esri+'\nwkt_ogc= '+wkt_ogc+'\nproj4='+proj4 )

    #esri->ogc, esri->proj
    if not silent:
        print( '\nesri->ogc, esri->proj\n' )
    srs_esri.SetFromUserInput( wkt_esri )
    srs_esri.MorphFromESRI()
    srs_esri.SetAttrValue( 'PROJCS|GEOGCS|DATUM','unknown' )
    srs_ogc.SetFromUserInput( wkt_ogc )
    srs_ogc.SetAttrValue( 'PROJCS|GEOGCS|DATUM','unknown' )
    wkt_esri_to_ogc = srs_esri.ExportToWkt()
    wkt_esri_to_proj4 = srs_esri.ExportToProj4()
    if not silent:
        print( 'wkt_ogc: '+srs_ogc.ExportToWkt() )
        print( 'wkt_esri_to_ogc: '+wkt_esri_to_ogc )
        print( 'wkt_esri_to_proj4: '+wkt_esri_to_proj4 )

    if not srs_esri.IsSame(srs_ogc):
        print( 'wkt_esri_to_ogc failed for '+proj4 )
        result = 'fail'
    if wkt_esri_to_proj4 != proj4:
        print( 'wkt_esri_to_proj4 failed for '+proj4 )
        result = 'fail'

    #ogc->esri, ogc->proj
    if not silent:
        print( '\nogc->esri, ogc->proj\n' )
    srs_esri.SetFromUserInput( wkt_esri )
    srs_esri.SetAttrValue( 'PROJCS|GEOGCS|DATUM','unknown' )
    srs_ogc.SetFromUserInput( wkt_ogc )
    srs_ogc.SetAttrValue( 'PROJCS|GEOGCS|DATUM','unknown' )
    wkt_ogc_to_proj4 = srs_ogc.ExportToProj4()
    srs_ogc.MorphToESRI()
    srs_ogc.SetAttrValue( 'PROJCS|GEOGCS|DATUM','unknown' )
    wkt_ogc_to_esri = srs_ogc.ExportToWkt()
    if not silent:
        print( 'wkt_ogc_to_esri: '+wkt_ogc_to_esri )
        print( 'wkt_ogc_to_proj4: '+wkt_ogc_to_proj4 )

    if not srs_esri.IsSame(srs_ogc):
        print( 'wkt_ogc_to_esri failed for '+proj4 )
        result = 'fail'
    if wkt_ogc_to_proj4 != proj4:
        print( 'wkt_ogc_to_proj4 failed for '+proj4 )
        result = 'fail'

    #proj->esri, proj->ogc
    if not silent:
        print( '\nproj->esri, proj->ogc\n' )
    srs_esri.SetFromUserInput( proj4 )
    srs_esri.MorphFromESRI()
    srs_esri.SetAttrValue( 'PROJCS|GEOGCS|DATUM','unknown' )
    proj4_to_esri = srs_esri.ExportToProj4()
    srs_ogc.SetFromUserInput( proj4 )
    srs_ogc.SetAttrValue( 'PROJCS|GEOGCS|DATUM','unknown' )
    proj4_to_ogc = srs_ogc.ExportToProj4()

    if proj4_to_ogc != proj4:
        print( 'proj4_to_ogc failed: proj4='+proj4+', proj4_to_ogc='+proj4_to_ogc  )
        result = 'fail'

    if proj4_to_esri != proj4:
        print( 'proj4_to_esri failed: proj4='+proj4+', proj4_to_esri='+proj4_to_esri  )
        result = 'fail'

    return result

###############################################################################
# Test for various stereographic projection remappings (ESRI / OGC / PROJ.4)
# Stereographic
# Double_Stereographic / Oblique_Stereographic (#1428 and #4267)
# Stereographic_North_Pole / Polar_Stereographic
# Orthographics (#4249)

def osr_esri_20():

    result = 'success'

    # Stereographic / Stereographic / +proj=stere +lat_0=0 +lon_0=0 ...
    #modified definitions from ESRI 'Stereographic (world).prj'
    stere_esri='PROJCS["World_Stereographic",GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Stereographic"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Scale_Factor",1.0],PARAMETER["Latitude_Of_Origin",0.0],UNIT["Meter",1.0]]'
    stere_ogc='PROJCS["World_Stereographic",GEOGCS["GCS_WGS_1984",DATUM["WGS_84",SPHEROID["WGS_84",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Stereographic"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Scale_Factor",1.0],PARAMETER["Latitude_Of_Origin",0.0],UNIT["Meter",1.0]]'
    stere_proj4='+proj=stere +lat_0=0 +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs '
    #result1 = 'success'
    result1 = osr_ersi_test(stere_esri, stere_ogc, stere_proj4)

    # Double_Stereographic / Oblique_Stereographic / +proj=sterea +lat_0=46 +lon_0=25 ...
    #modified definitions from ESRI 'Stereo 1970.prj'
    sterea_esri='PROJCS["Stereo_70",GEOGCS["GCS_Dealul_Piscului_1970",DATUM["D_Dealul_Piscului_1970",SPHEROID["Krasovsky_1940",6378245.0,298.3]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Double_Stereographic"],PARAMETER["False_Easting",500000.0],PARAMETER["False_Northing",500000.0],PARAMETER["Central_Meridian",25.0],PARAMETER["Scale_Factor",0.99975],PARAMETER["Latitude_Of_Origin",46.0],UNIT["Meter",1.0]]'
    sterea_ogc='PROJCS["Stereo_70",GEOGCS["GCS_Dealul_Piscului_1970",DATUM["Dealul_Piscului_1970",SPHEROID["Krasovsky_1940",6378245.0,298.3]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Oblique_Stereographic"],PARAMETER["False_Easting",500000.0],PARAMETER["False_Northing",500000.0],PARAMETER["Central_Meridian",25.0],PARAMETER["Scale_Factor",0.99975],PARAMETER["Latitude_Of_Origin",46.0],UNIT["Meter",1.0]]'
    sterea_proj4='+proj=sterea +lat_0=46 +lon_0=25 +k=0.99975 +x_0=500000 +y_0=500000 +ellps=krass +units=m +no_defs '
    result2 = osr_ersi_test(sterea_esri, sterea_ogc, sterea_proj4)

    # Stereographic_North_Pole / Polar_Stereographic / +proj=stere +lat_0=90 +lat_ts=70 ...
    #modified definitions from ESRI 'WGS 1984 NSIDC Sea Ice Polar Stereographic North.prj'
    sterep_esri='PROJCS["WGS_1984_NSIDC_Sea_Ice_Polar_Stereographic_North",GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Stereographic_North_Pole"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-45.0],PARAMETER["Standard_Parallel_1",70.0],UNIT["Meter",1.0]]'
    sterep_ogc='PROJCS["WGS_1984_NSIDC_Sea_Ice_Polar_Stereographic_North",GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_84",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",-45.0],PARAMETER["latitude_of_origin",70.0],UNIT["Meter",1.0]]'
    sterep_proj4='+proj=stere +lat_0=90 +lat_ts=70 +lon_0=-45 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs '
    result3 = osr_ersi_test(sterep_esri, sterep_ogc, sterep_proj4)

    # Orthographic (#4249)
    ortho_esri='PROJCS["unnamed",GEOGCS["GCS_WGS_1984",DATUM["unknown",SPHEROID["WGS84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Orthographic"],PARAMETER["Latitude_Of_Center",-37],PARAMETER["Longitude_Of_Center",145],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]'
    ortho_ogc='PROJCS["unnamed",GEOGCS["WGS 84",DATUM["unknown",SPHEROID["WGS84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Orthographic"],PARAMETER["latitude_of_origin",-37],PARAMETER["central_meridian",145],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]'
    ortho_proj4='+proj=ortho +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs '
    result4 = osr_ersi_test(ortho_esri, ortho_ogc, ortho_proj4)

    if ( result1 != 'success' or result2 != 'success' or result3 != 'success' or result4 != 'success'):
        result = 'fail'

    return result


###############################################################################
# Test round-trip WKT ESRI->OGC->ESRI
#
# data from bug #4345 and ESRI pages below
# ifile must be in csv format (; seperator) with the following header:
# COORD_REF_SYS_CODE;ESRI_DATUM_NAME;WKT
# http://help.arcgis.com/en/arcims/10.0/mainhelp/mergedProjects/ArcXMLGuide/elements/gcs.htm
# http://help.arcgis.com/en/arcims/10.0/mainhelp/mergedProjects/ArcXMLGuide/elements/pcs.htm
# http://help.arcgis.com/en/arcims/10.0/mainhelp/mergedProjects/ArcXMLGuide/elements/dattrans.htm

def osr_esri_test_esri_ogc_esri( ifile, ofile_base, fix_config='NO', check_epsg=False ):

    if not os.path.exists( ifile ):
        print('input file '+ifile+' does not exist')
        return 'fail'

    result = 'success'
    check_srs = True
    check_wkt = False
    failed_epsg_count = 0
    failed_srs_count = 0
    failed_wkt_count = 0
    ofile_epsg = 'tmp/'+ofile_base+'_epsg.txt'
    ofile_srs = 'tmp/'+ofile_base+'_srs.txt'
    ofile_wkt = 'tmp/'+ofile_base+'_wkt.txt'

    #initialise output files
    if not os.path.exists('tmp'):
        os.mkdir('tmp')
    if os.path.exists(ofile_epsg):
        os.unlink(ofile_epsg)
    if check_epsg:
        epsg_ne=''
        epsg_none=''
        epsg_other=''
        of_epsg = open(ofile_epsg,'w')
    if os.path.exists(ofile_srs):
        os.unlink(ofile_srs)
    if check_srs:
        of_srs = open(ofile_srs,'w')
    if os.path.exists(ofile_wkt):
        os.unlink(ofile_wkt)
    if check_wkt:
        of_wkt= open(ofile_wkt,'w')

    #open input file
    if os.path.splitext(ifile)[1] == '.gz':
        f = gzip.open(ifile, 'rb')
    else:
        f = open(ifile,'rt')
    csv_reader = csv.DictReader(f,delimiter=';')

    csv.DictReader(gdal.FindFile('gdal','gcs.csv'), 'epsg_gcs2', 'GEOGCS', True)

    #set GDAL_FIX_ESRI_WKT option
    fix_config_bak = gdal.GetConfigOption('GDAL_FIX_ESRI_WKT')
    gdal.SetConfigOption('GDAL_FIX_ESRI_WKT', fix_config)

    #need to be quiet because some codes raise errors
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )

    #parse all lines
    for iline in csv_reader:

        epsg_code = int(iline['COORD_REF_SYS_CODE'])
        if iline['WKT'] is None or iline['WKT']=='':
            continue

        #read wkt and morph from ESRI
        srs1 = osr.SpatialReference()
        srs1.ImportFromWkt( iline['WKT'] )
        wkt1 = srs1.ExportToWkt()
        srs2 = srs1.Clone()
        srs2.MorphFromESRI()
        #wkt2 = srs2.ExportToWkt()

        #morph back to ESRI
        srs3 = srs2.Clone()
        srs3.MorphToESRI()
        wkt3 = srs3.ExportToWkt()

        #manage special cases of PROJECTION parameters that have multiple mappings
        remap_proj=dict([ ['Transverse_Mercator','Gauss_Kruger'], ['Equidistant_Cylindrical', 'Plate_Carree'], \
                              ['Hotine_Oblique_Mercator_Azimuth_Natural_Origin','Hotine_Oblique_Mercator_Azimuth_Center'] ] )
        proj1=srs1.GetAttrValue( 'PROJCS|PROJECTION' )
        proj3=srs3.GetAttrValue( 'PROJCS|PROJECTION' )
        if proj3 in remap_proj and proj1==remap_proj[proj3]:
            srs3.SetAttrValue( 'PROJCS|PROJECTION', remap_proj[proj3] )
            wkt3 = srs3.ExportToWkt()

        #check epsg
        if check_epsg:
            epsg2 = srs2.GetAuthorityCode('GEOGCS')
            if epsg2 is None or int(epsg2)!=epsg_code:
                #check why epsg codes conflict
                srs4 = osr.SpatialReference()
                #check if EPSG code imports ok
                if srs4.ImportFromEPSG( epsg_code ) != 0:
                    #of_epsg.write( 'ERROR: #'+str(epsg_code)+', EPSG does not exist\n')
                    epsg_ne = epsg_ne+' '+str(epsg_code)
                else:
                    if epsg2 is None:
                        of_epsg.write( 'ERROR: #'+str(epsg_code)+', did not get EPSG code\n' )
                        epsg_none = epsg_none+' '+str(epsg_code)
                    else:
                        of_epsg.write( 'ERROR: EPSG not matching for # '+str(epsg_code)+', got EPSG:'+str(epsg2)+'\n' )
                        epsg_other = epsg_other+' '+str(epsg_code)
                    failed_epsg_count = failed_epsg_count + 1
                    of_epsg.write( 'wkt1: '+wkt1+'\n'+'wkt3: '+wkt3+'\n' )

        #check srs
        if check_srs and not srs1.IsSame(srs3):
            failed_srs_count = failed_srs_count + 1
            of_srs.write( 'ERROR: SRS not matching for # '+iline['COORD_REF_SYS_CODE']+'\n' )
            of_srs.write( 'wkt1: '+wkt1+'\n'+'wkt3: '+wkt3+'\n' )

        #check wkt
        if check_wkt and wkt1 != wkt3:
            failed_wkt_count = failed_wkt_count + 1
            of_wkt.write( 'WARNING: WKT not matching for # '+iline['COORD_REF_SYS_CODE']+'\n' )
            of_wkt.write( 'wkt1: '+wkt1+'\n'+'wkt3: '+wkt3+'\n' )

    #revert
    gdal.SetConfigOption('GDAL_FIX_ESRI_WKT', fix_config_bak)
    gdal.PopErrorHandler()

    #close files and report
    if check_epsg:
        of_epsg.close()
        if failed_epsg_count > 0:
            print('ERROR: Failed %d EPSG tests, see file %s' % (failed_epsg_count,ofile_epsg) )
            #print('epsg_ne: '+epsg_ne)
            #print('epsg_none: '+epsg_none)
            #print('epsg_other: '+epsg_other)
            result='fail'
        else:
            os.unlink(ofile_epsg)

    if check_srs:
        of_srs.close()
        if failed_srs_count > 0:
            print('ERROR: Failed %d SRS tests, see file %s' % (failed_srs_count,ofile_srs) )
            result='fail'
        else:
            os.unlink(ofile_srs)

    if check_wkt:
        of_wkt.close()
        if failed_wkt_count > 0 :
            print('WARNING: Failed %d WKT tests, see file %s' % (failed_wkt_count,ofile_wkt) )
        else:
            os.unlink(ofile_wkt)

    return result


def osr_esri_21():

    # FIXME ?
    if sys.version_info >= (3,0,0):
        return 'skip'

    result = 'success'

    # Test GEOGCSCS defs
    result1 = osr_esri_test_esri_ogc_esri('data/esri_gcs.csv.gz', 'esri_gcs')
    if result1 == 'fail':
        result = 'fail'

    # Test PROJCS defs
    result2 = osr_esri_test_esri_ogc_esri('data/esri_pcs.csv.gz', 'esri_pcs')
    if result2 == 'fail':
        result = 'fail'

    # Test other defs (collected elsewhere)
    result3 = osr_esri_test_esri_ogc_esri('data/esri_extra.csv', 'esri_extra')
    if result3 == 'fail':
        result = 'fail'

    # Test GEOGCSCS defs - check if can test import from EPSG code
    result4 = osr_esri_test_esri_ogc_esri('data/esri_gcs.csv.gz', 'esri_gcs2', 'GEOGCS', True)
    if result4 == 'fail':
        result = 'expected_fail'

    return result


###############################################################################
# Test round-trip WKT OGC->ESRI->OGC from EPSG code
#
# ifile must be in csv format and contain a COORD_REF_SYS_CODE
# which will be used in ImportFromEPSG()

def osr_esri_test_ogc_esri_ogc( ifile, ofile_base, fix_config='NO', check_epsg=False ):

    if not os.path.exists( ifile ):
        print('input file '+ifile+' does not exist')
        return 'fail'

    result = 'success'
    check_srs = True
    check_wkt = False
    failed_epsg_count = 0
    failed_srs_count = 0
    failed_wkt_count = 0
    ofile_epsg = 'tmp/'+ofile_base+'_epsg.txt'
    ofile_srs = 'tmp/'+ofile_base+'_srs.txt'
    ofile_wkt = 'tmp/'+ofile_base+'_wkt.txt'

    #initialise output files
    if not os.path.exists('tmp'):
        os.mkdir('tmp')
    if os.path.exists(ofile_epsg):
        os.unlink(ofile_epsg)
    if check_epsg:
        epsg_error=''
        of_epsg = open(ofile_epsg,'w')
    if os.path.exists(ofile_srs):
        os.unlink(ofile_srs)
    if check_srs:
        of_srs = open(ofile_srs,'w')
    if os.path.exists(ofile_wkt):
        os.unlink(ofile_wkt)
    if check_wkt:
        of_wkt = open(ofile_wkt,'w')

    #open input file
    if os.path.splitext(ifile)[1] == '.gz':
        f = gzip.open(ifile, 'rb')
    else:
        f = open(ifile,'rt')
    csv_reader = csv.DictReader(f,delimiter=',')

    #set GDAL_FIX_ESRI_WKT option
    fix_config_bak = gdal.GetConfigOption('GDAL_FIX_ESRI_WKT')
    gdal.SetConfigOption('GDAL_FIX_ESRI_WKT', fix_config)

    #need to be quiet because some codes raise errors
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )

    #parse all lines
    for iline in csv_reader:
        epsg_code = int(iline['COORD_REF_SYS_CODE'])

        #import from EPSG code
        srs1 = osr.SpatialReference()
        if srs1.ImportFromEPSG( epsg_code ) != 0:
            continue
        wkt1 = srs1.ExportToWkt()

        #morph to ESRI
        srs2 = srs1.Clone()
        srs2.MorphToESRI()
        #wkt2 = srs2.ExportToWkt()

        #morph back from ESRI
        srs3 = srs2.Clone()
        srs3.MorphFromESRI()
        wkt3 = srs3.ExportToWkt()

        #manage special cases
        #missing rectified_grid_angle (e.g. EPSG:2057)
        if srs1.GetProjParm( 'rectified_grid_angle' ) != 0:
            srs3.SetProjParm( 'rectified_grid_angle', srs1.GetProjParm( 'rectified_grid_angle' ) )
            wkt3 = srs3.ExportToWkt()
        #missing scale_factor for Mercator_1SP (e.g. EPSG:2934) and Polar_Stereographic (e.g. EPSG:3031)
        proj1 = srs1.GetAttrValue( 'PROJCS|PROJECTION' )
        if proj1 == 'Mercator_1SP' or proj1 == 'Polar_Stereographic':
            if srs1.GetProjParm( 'scale_factor' ) != 0:
                srs3.SetProjParm( 'scale_factor', srs1.GetProjParm( 'scale_factor' ) )
                wkt3 = srs3.ExportToWkt()

        #do checks

        #check epsg
        if check_epsg:
            epsg3 = srs3.GetAuthorityCode('GEOGCS')
            if epsg3 is None or int(epsg3)!=epsg_code:
                failed_epsg_count = failed_epsg_count + 1
                epsg_error = epsg_error+' '+str(epsg_code)
                of_epsg.write( 'ERROR: EPSG not matching for EPSG:'+str(epsg_code)+', got EPSG:'+str(epsg3)+'\n' )
                of_epsg.write( 'wkt1: '+wkt1+'\n'+'wkt3: '+wkt3+'\n' )
                #of_epsg.write( srs1.ExportToPrettyWkt()+'\n'+srs3.ExportToPrettyWkt()+'\n' )

        #strip CT - add option for this and make more tests
        srs1.StripCTParms()
        wkt1 = srs1.ExportToWkt()
        srs3.StripCTParms()
        wkt3 = srs3.ExportToWkt()

        #check srs
        if check_srs and not srs1.IsSame(srs3):
            failed_srs_count = failed_srs_count + 1
            of_srs.write( 'ERROR: SRS not matching for EPSG:'+str(epsg_code)+'\n' )
            of_srs.write( 'wkt1: '+wkt1+'\n'+'wkt3: '+wkt3+'\n' )

        #check wkt
        if check_wkt and wkt1 != wkt3:
            failed_wkt_count = failed_wkt_count + 1
            of_wkt.write( 'WARNING: WKT not matching for EPSG:'+str(epsg_code)+'\n' )
            of_wkt.write( 'wkt1: '+wkt1+'\n'+'wkt3: '+wkt3+'\n' )

    #revert
    gdal.SetConfigOption('GDAL_FIX_ESRI_WKT', fix_config_bak)
    gdal.PopErrorHandler()

    #close files and report
    if check_epsg:
        of_epsg.close()
        if failed_epsg_count > 0 :
            print('ERROR: Failed %d EPSG tests, see file %s' % (failed_epsg_count,ofile_epsg) )
            #print(epsg_error)
            result = 'fail'
        else:
            os.unlink(ofile_epsg)
    if check_srs:
        of_srs.close()
        if failed_srs_count > 0 :
            print('ERROR: Failed %d SRS tests, see file %s' % (failed_srs_count,ofile_srs) )
            result = 'fail'
        else:
            os.unlink(ofile_srs)
    if check_wkt:
        of_wkt.close()
        if failed_wkt_count > 0 :
            print('WARNING: Failed %d WKT tests, see file %s' % (failed_wkt_count,ofile_wkt) )
        else:
            os.unlink(ofile_wkt)

    return result

###############################################################################
# Test EPSG->OGC->ESRI->OGC

def osr_esri_22():

    result = 'success'

    # Test GEOGCSCS defs
    result1 = osr_esri_test_ogc_esri_ogc(gdal.FindFile('gdal','gcs.csv'), 'epsg_gcs')
    if result1 == 'fail':
        result = 'expected_fail'

    # Test PROJCS defs
    result2 = osr_esri_test_ogc_esri_ogc(gdal.FindFile('gdal','pcs.csv'), 'epsg_pcs')
    if result2 == 'fail':
        result = 'expected_fail'

    return result

###############################################################################
# Test EPSG->OGC->ESRI->OGC
# set GDAL_FIX_ESRI_WKT=DATUM (bugs #4378 and #4345), don't expect to fail

def osr_esri_23():

    result = 'success'

    # Test GEOGCSCS defs
    result1 = osr_esri_test_ogc_esri_ogc(gdal.FindFile('gdal','gcs.csv'), 'epsg_gcs2', 'GEOGCS', True)
    if result1 == 'fail':
        result = 'expected_fail'

    # Test PROJCS defs
    result2 = osr_esri_test_ogc_esri_ogc(gdal.FindFile('gdal','pcs.csv'), 'epsg_pcs2', 'DATUM', False)
    if result2 == 'fail':
        result = 'fail'

    return result

###############################################################################
# Test LCC_2 Central_Parallel <--> latitude_of_origin issue (#3191)
#

def osr_esri_24():

    srs = osr.SpatialReference()
    srs.ImportFromWkt('''PROJCS["Custom",
                             GEOGCS["GCS_WGS_1984",
                                 DATUM["WGS_1984",
                                     SPHEROID["WGS_1984",6378137.0,298.257223563]],
                                 PRIMEM["Greenwich",0.0],
                                 UNIT["Degree",0.017453292519943295]],
                             PROJECTION["Lambert_Conformal_Conic_2SP"],
                             PARAMETER["False_Easting",0.0],
                             PARAMETER["False_Northing",0.0],
                             PARAMETER["Central_Meridian",10.5],
                             PARAMETER["Standard_Parallel_1",48.66666666666666],
                             PARAMETER["Standard_Parallel_2",53.66666666666666],
                             PARAMETER["Central_Parallel",51.0],
                             UNIT["Meter",1.0]]''')
    srs.MorphFromESRI()
    if srs.GetProjParm(osr.SRS_PP_LATITUDE_OF_ORIGIN, 1000.0) == 1000.0:
        gdaltest.post_reason('Failed to set latitude_of_origin')
        return 'fail'

    return 'success'

###############################################################################
# Test Pseudo-Mercator (#3962)
#
def osr_esri_25():
    srs = osr.SpatialReference()
    srs.SetFromUserInput(
        'PROJCS["WGS_1984_Web_Mercator_Auxiliary_Sphere",'
        '    GEOGCS["GCS_WGS_1984",'
        '        DATUM["D_WGS_1984",'
        '            SPHEROID["WGS_1984",6378137.0,298.257223563]],'
        '        PRIMEM["Greenwich",0.0],'
        '        UNIT["Degree",0.017453292519943295]],'
        '    PROJECTION["Mercator_Auxiliary_Sphere"],'
        '    PARAMETER["False_Easting",0.0],'
        '    PARAMETER["False_Northing",0.0],'
        '    PARAMETER["Central_Meridian",0.0],'
        '    PARAMETER["Standard_Parallel_1",0.0],'
        '    PARAMETER["Auxiliary_Sphere_Type",0.0],'
        '    UNIT["Meter",1.0]]'
    )

    target_srs = osr.SpatialReference()
    target_srs.ImportFromEPSG(4326)
    transformer = osr.CoordinateTransformation(srs, target_srs)
    expected_proj4_string  = ('+a=6378137 +b=6378137 +proj=merc +lat_ts=0'
                              ' +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +no_defs')
    proj4_string = srs.ExportToProj4()
    if not (expected_proj4_string.split(' ').sort() == proj4_string.split(' ').sort()):
      print('Got: %s' % expected_proj4_string)
      print('Expected: %s' % proj4_string)
      return 'fail'

    # test an actual conversion
    (x, y, z) = transformer.TransformPoint(7000000, 7000000, 0)
    (exp_x, exp_y, exp_z) = (62.882069888366, 53.091818769596, 0.0)
    if (abs(exp_x - x) > 0.00001 or
        abs(exp_y - y) > 0.00001 or
        abs(exp_z - z) > 0.00001):
      print('Got:      (%f, %f, %f)' % (x, y, z))
      print('Expected: (%f, %f, %f)' % (exp_x, exp_y, exp_z))
      return 'fail'

    return 'success'
###############################################################################
#

gdaltest_list = [
    osr_esri_1,
    osr_esri_2,
    osr_esri_3,
    osr_esri_4,
    osr_esri_5,
    osr_esri_6,
    osr_esri_7,
    osr_esri_8,
    osr_esri_9,
    osr_esri_10,
    osr_esri_11,
    osr_esri_12,
    osr_esri_13,
    osr_esri_14,
    osr_esri_15,
    osr_esri_16,
    osr_esri_17,
    osr_esri_18,
    osr_esri_19,
    osr_esri_20,
    osr_esri_21,
    osr_esri_22,
    osr_esri_23,
    osr_esri_24,
    osr_esri_25,
   None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_esri' )

    #make sure GDAL_FIX_ESRI_WKT does not interfere with tests
    gdal.SetConfigOption('GDAL_FIX_ESRI_WKT', 'NO')
    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
