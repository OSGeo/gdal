#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test reprojection of points of many different projections.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
import os

import pytest

from osgeo import osr

bonne = 'PROJCS["bonne",GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["bonne"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Standard_Parallel_1",60.0],UNIT["Meter",1.0]]'

###############################################################################
# Table of transformations, inputs and expected results (with a threshold)
#
# Each entry in the list should have a tuple with:
#
# - src_srs: any form that SetFromUserInput() will take.
# - (src_x, src_y, src_z): location in src_srs.
# - src_error: threshold for error when srs_x/y is transformed into dst_srs and
#              then back into srs_src.
# - dst_srs: destination srs.
# - (dst_x,dst_y,dst_z): point that src_x/y should transform to.
# - dst_error: acceptable error threshold for comparing to dst_x/y.
# - unit_name: the display name for this unit test.
# - options: eventually we will allow a list of special options here (like one
#   way transformation).  For now just put None.
# - grid_req: string with grid name or None
# - proj_version_req: string with minimum proj version required or None

transform_list = [

    # Simple straight forward reprojection.
    ('+proj=utm +zone=11 +datum=WGS84', (398285.45, 2654587.59, 0.0), 0.02,
     'WGS84', (24.0, -118.0, 0.0), 0.00001,
     'UTM_WGS84', None, None, None),

    # Ensure that prime meridian *and* axis orientation changes are applied.
    # Note that this test will fail with PROJ.4 4.7 or earlier, it requires
    # axis support in PROJ 4.8.0.
    #    ('EPSG:27391', (40000, 20000, 0.0), 0.02,
    #     'EPSG:4273', (6.397933,58.358709,0.000000), 0.00001,
    #     'NGO_Oslo_zone1_NGO', None, None, None ),

    # Test Bonne projection.
    ('WGS84', (65.0, 1.0, 0.0), 0.00001,
     bonne, (47173.75, 557621.30, 0.0), 0.02,
     'Bonne_WGS84', None, None, None),

    # Test Two Point Equidistant
    ('+proj=tpeqd +a=3396000  +b=3396000  +lat_1=36.3201218 +lon_1=-179.1566925 +lat_2=45.8120651 +lon_2=179.3727570 +no_defs', (4983568.76, 2092902.61, 0.0), 0.1,
     '+proj=latlong +a=3396000 +b=3396000', (-140.0, 40.0, 0.0), 0.000001,
     'TPED_Mars', None, None, None),

    # test scale factor precision (per #1970)
    ('data/wkt_rt90.def', (1572570.342, 6728429.67, 0.0), 0.001,
     ' +proj=utm +zone=33 +ellps=GRS80 +units=m +no_defs', (616531.1155, 6727527.5682, 0.0), 0.001,
     'ScalePrecision(#1970)', None, None, None),

    # Test Google Mercator (EPSG:3785)
    ('EPSG:3785', (1572570.342, 6728429.67, 0.0), 0.001,
     'WGS84', (51.601722482149995, 14.126639735716626, 0.0), 0.0000001,
     'GoogleMercator(#3136)', None, None, None),

    # Test Equirectangular with all parameters
    ('+proj=eqc +ellps=sphere  +lat_0=-2 +lat_ts=1 +lon_0=-10', (-14453132.04, 4670184.72, 0.0), 0.1,
     '+proj=latlong +ellps=sphere', (-140.0, 40.0, 0.0), 0.000001,
     'Equirectangular(#2706)', None, None, None),

    # Test Geocentric
    ('+proj=latlong +datum=WGS84', (-140.0, 40.0, 0.0), 0.000001,
     'EPSG:4328', (-3748031.46884168, -3144971.82314589, 4077985.57220038), 0.1,
     'Geocentric', None, None, None),

    # Test Vertical Datum Shift with a change of horizontal units.
    ('+proj=utm +zone=11 +datum=WGS84', (100000.0, 3500000.0, 0.0), 0.1,
     '+proj=utm +zone=11 +datum=WGS84 +geoidgrids=egm96_15.gtx +units=us-ft', (328083.333225467, 11482916.6665952, 41.4697855726348), 0.01,
     'EGM 96 Conversion', None, "egm96_15.gtx", '6.2.1'),

    # Test optimization in case of identical projections (projected)
    ('+proj=utm +zone=11 +datum=NAD27 +units=m', (440720.0, 3751260.0, 0.0), 0,
     '+proj=utm +zone=11 +datum=NAD27 +units=m', (440720.0, 3751260.0, 0.0), 0,
     'No-op Optimization (projected)', None, None, None),

    # Test optimization in case of identical projections (geodetic)
    ('+proj=longlat +datum=WGS84', (2, 49, 0.0), 0,
     '+proj=longlat +datum=WGS84', (2, 49, 0.0), 0,
     'No-op Optimization (geodetic)', None, None, None),

    # Test GRS80 -> EPSG:3857
    ('+proj=longlat +ellps=GRS80 +towgs84=0,0,0 +no_defs', (2, 49, 0.0), 1e-8,
     '+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs', (222638.981586547, 6274861.39384813, 0), 1e-3,
     'GRS80 -> EPSG:3857', None, None, None),

    # Test GRS80 -> EPSG:3857
    ('+proj=longlat +ellps=GRS80 +towgs84=0,0,0 +no_defs', (2, 49, 0.0), 1e-8,
     '+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs', (222638.981586547, 6274861.39384813, 0), 1e-3,
     'GRS80 -> EPSG:3857', None, None, None),

    ('EPSG:4314', (50, 10, 0.0), 1e-8,
     'EPSG:4326', (49.9988573027651,9.99881145557889, 0.0), 1e-8,
     'DHDN -> WGS84 using BETA2007', None, 'BETA2007.gsb', None),

    ("""GEOGCS["DHDN",
    DATUM["Deutsches_Hauptdreiecksnetz",
        SPHEROID["Bessel 1841",6377397.155,299.1528128,
            AUTHORITY["EPSG","7004"]],
        TOWGS84[598.1,73.7,418.2,0.202,0.045,-2.455,6.7],
        AUTHORITY["EPSG","6314"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AUTHORITY["EPSG","4314"]]""", (50, 10, 0.0), 1e-8,
     'EPSG:4326', (49.9988572643058,9.99881392529464,0), 1e-8,
     'DHDN -> WGS84 using TOWGS84 automatically set', 'OSR_CT_USE_DEFAULT_EPSG_TOWGS84=YES', None, None),

    ('+proj=longlat +ellps=bessel +towgs84=598.1,73.7,418.2,0.202,0.045,-2.455,6.7 +no_defs', (10, 50, 0.0), 1e-8,
     'EPSG:4326', (49.9988572643058,9.99881392529464,0), 1e-8,
     'DHDN -> WGS84 using explicit TOWGS84', None, None, None),

    ('EPSG:27562', (136555.58288992, 463344.51894296, 0.0), 1e-8,
     'EPSG:4258', (49, -4, 0), 1e-8,
     'EPSG:27562 -> EPSG:4258 using OGR_CT_OP_SELECTION=BEST_ACCURACY',
     'OGR_CT_OP_SELECTION=BEST_ACCURACY',
     'ntf_r93.gsb', '6.3.2'), # not sure 6.3.2 is the actual min version
]

map_old_grid_name_to_tif_ones = {
    'ntf_r93.gsb': 'fr_ign_ntf_r93.tif',
    'BETA2007.gsb': 'de_adv_BETA2007.tif',
    'egm96_15.gtx': 'us_nga_egm96_15.tif'
}

###############################################################################
# When imported build a list of units based on the files available.


@pytest.mark.parametrize(
    'src_srs,src_xyz,src_error,dst_srs,dst_xyz,dst_error,unit_name,options,grid_req,proj_version_req',
    transform_list,
    ids=[row[6] for row in transform_list]
)
def test_proj(src_srs, src_xyz, src_error,
             dst_srs, dst_xyz, dst_error, unit_name, options, grid_req, proj_version_req):

    if grid_req is not None:
        grid_name = grid_req
        assert grid_name in map_old_grid_name_to_tif_ones
        search_paths = osr.GetPROJSearchPaths()
        found = False
        if search_paths:
            for path in search_paths:
                if os.path.exists(os.path.join(path, grid_name)) or \
                   os.path.exists(os.path.join(path, map_old_grid_name_to_tif_ones[grid_name])):
                    found = True
                    break
        if not found:
            pytest.skip(f'Did not find GRID:{grid_name}')

    if proj_version_req is not None:
        major, minor, micro = proj_version_req.split('.')
        major, minor, micro = int(major), int(minor), int(micro)
        if osr.GetPROJVersionMajor() * 10000 + osr.GetPROJVersionMinor() * 100 + osr.GetPROJVersionMicro() <= major * 10000 + minor * 100 + micro:
            pytest.skip(f'PROJ version < {proj_version_req}')

    src = osr.SpatialReference()
    assert src.SetFromUserInput(src_srs) == 0, \
        ('SetFromUserInput(%s) failed.' % src_srs)

    dst = osr.SpatialReference()
    assert dst.SetFromUserInput(dst_srs) == 0, \
        ('SetFromUserInput(%s) failed.' % dst_srs)

    has_built_ct = False
    if options and '=' in options:
        tokens = options.split('=')
        if len(tokens) == 2:
            key = tokens[0]
            value = tokens[1]
            with gdaltest.config_option(key, value):
                has_built_ct = True
                ct = osr.CoordinateTransformation(src, dst)
    if not has_built_ct:
        ct = osr.CoordinateTransformation(src, dst)

    ######################################################################
    # Transform source point to destination SRS.

    result = ct.TransformPoint(src_xyz[0], src_xyz[1], src_xyz[2])

    error = abs(result[0] - dst_xyz[0]) \
        + abs(result[1] - dst_xyz[1]) \
        + abs(result[2] - dst_xyz[2])

    assert error <= dst_error, \
        ('Dest error is %g, got (%.15g,%.15g,%.15g)'
                             % (error, result[0], result[1], result[2]))

    ######################################################################
    # Now transform back.

    has_built_ct = False
    if options and '=' in options:
        tokens = options.split('=')
        if len(tokens) == 2:
            key = tokens[0]
            value = tokens[1]
            with gdaltest.config_option(key, value):
                has_built_ct = True
                ct = osr.CoordinateTransformation(dst, src)
    if not has_built_ct:
        ct = osr.CoordinateTransformation(dst, src)

    result = ct.TransformPoint(result[0], result[1], result[2])

    error = abs(result[0] - src_xyz[0]) \
        + abs(result[1] - src_xyz[1]) \
        + abs(result[2] - src_xyz[2])

    assert error <= src_error, \
        ('Back to source error is %g got (%.15g,%.15g,%.15g)'
                            % (error, result[0], result[1], result[2]))
