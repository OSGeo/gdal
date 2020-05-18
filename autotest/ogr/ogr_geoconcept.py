#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Geoconcept driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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


import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
import pytest

###############################################################################
# Simple read test of known file.


def test_ogr_gxt_1():

    gdaltest.gxt_ds = ogr.Open('data/geoconcept/expected_000_GRD.gxt')

    assert gdaltest.gxt_ds is not None

    assert gdaltest.gxt_ds.GetLayerCount() == 1, 'Got wrong layer count.'

    lyr = gdaltest.gxt_ds.GetLayer(0)
    assert lyr.GetName() == '000_GRD.000_GRD', 'got unexpected layer name.'

    assert lyr.GetFeatureCount() == 10, 'got wrong feature count.'

    expect = ['000-2007-0050-7130-LAMB93',
              '000-2007-0595-7130-LAMB93',
              '000-2007-0595-6585-LAMB93',
              '000-2007-1145-6250-LAMB93',
              '000-2007-0050-6585-LAMB93',
              '000-2007-0050-7130-LAMB93',
              '000-2007-0595-7130-LAMB93',
              '000-2007-0595-6585-LAMB93',
              '000-2007-1145-6250-LAMB93',
              '000-2007-0050-6585-LAMB93']

    tr = ogrtest.check_features_against_list(lyr, 'idSel', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()

    assert (ogrtest.check_feature_geometry(feat,
                                      'MULTIPOLYGON (((50000 7130000,600000 7130000,600000 6580000,50000 6580000,50000 7130000)))',
                                      max_error=0.000000001) == 0)

    srs = osr.SpatialReference()
    srs.SetFromUserInput('PROJCS["Lambert 93",GEOGCS["unnamed",DATUM["ITRS-89",SPHEROID["GRS 80",6378137,298.257222099657],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",44],PARAMETER["standard_parallel_2",49],PARAMETER["latitude_of_origin",46.5],PARAMETER["central_meridian",3],PARAMETER["false_easting",700000],PARAMETER["false_northing",6600000],UNIT["metre",1]]')

    assert lyr.GetSpatialRef().IsSame(srs), 'SRS is not the one expected.'

###############################################################################
# Similar test than previous one with TAB separator.


def test_ogr_gxt_2():

    gdaltest.gxt_ds = ogr.Open('data/geoconcept/expected_000_GRD_TAB.txt')

    assert gdaltest.gxt_ds is not None

    assert gdaltest.gxt_ds.GetLayerCount() == 1, 'Got wrong layer count.'

    lyr = gdaltest.gxt_ds.GetLayer(0)
    assert lyr.GetName() == '000_GRD.000_GRD', 'got unexpected layer name.'

    assert lyr.GetFeatureCount() == 5, 'got wrong feature count.'

    expect = ['000-2007-0050-7130-LAMB93',
              '000-2007-0595-7130-LAMB93',
              '000-2007-0595-6585-LAMB93',
              '000-2007-1145-6250-LAMB93',
              '000-2007-0050-6585-LAMB93']

    tr = ogrtest.check_features_against_list(lyr, 'idSel', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()

    assert (ogrtest.check_feature_geometry(feat,
                                      'MULTIPOLYGON (((50000 7130000,600000 7130000,600000 6580000,50000 6580000,50000 7130000)))',
                                      max_error=0.000000001) == 0)

###############################################################################
# Read a GXT file containing 2 points, duplicate it, and check the newly written file


def test_ogr_gxt_3():

    gdaltest.gxt_ds = None

    src_ds = ogr.Open('data/geoconcept/points.gxt')

    try:
        os.remove('tmp/tmp.gxt')
    except OSError:
        pass

    # Duplicate all the points from the source GXT
    src_lyr = src_ds.GetLayerByName('points.points')

    gdaltest.gxt_ds = ogr.GetDriverByName('Geoconcept').CreateDataSource('tmp/tmp.gxt')

    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')

    gxt_lyr = gdaltest.gxt_ds.CreateLayer('points', srs, geom_type=ogr.wkbPoint)

    src_lyr.ResetReading()

    for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
        gxt_lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=gxt_lyr.GetLayerDefn())

    feat = src_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gxt_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

        feat = src_lyr.GetNextFeature()

    gdaltest.gxt_ds = None

    # Read the newly written GXT file and check its features and geometries
    gdaltest.gxt_ds = ogr.Open('tmp/tmp.gxt')
    gxt_lyr = gdaltest.gxt_ds.GetLayerByName('points.points')

    assert gxt_lyr.GetSpatialRef().IsSame(srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), 'Output SRS is not the one expected.'

    expect = ['PID1', 'PID2']

    tr = ogrtest.check_features_against_list(gxt_lyr, 'Primary_ID', expect)
    assert tr

    gxt_lyr.ResetReading()

    expect = ['SID1', 'SID2']

    tr = ogrtest.check_features_against_list(gxt_lyr, 'Secondary_ID', expect)
    assert tr

    gxt_lyr.ResetReading()

    expect = ['TID1', None]

    tr = ogrtest.check_features_against_list(gxt_lyr, 'Third_ID', expect)
    assert tr

    gxt_lyr.ResetReading()

    feat = gxt_lyr.GetNextFeature()

    assert (ogrtest.check_feature_geometry(feat, 'POINT(0 1)',
                                      max_error=0.000000001) == 0)

    feat = gxt_lyr.GetNextFeature()

    assert (ogrtest.check_feature_geometry(feat, 'POINT(2 3)',
                                      max_error=0.000000001) == 0)

###############################################################################
#


def test_ogr_gxt_multipolygon_singlepart_nohole():

    ds = ogr.Open('data/geoconcept/geoconcept_multipolygon_singlepart_nohole.txt')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))',
                                      max_error=0.000000001) != 0:
        feat.DumpReadable()
        pytest.fail()

    
###############################################################################
#


def test_ogr_gxt_multipolygon_singlepart_hole():

    if not ogrtest.have_geos():
        pytest.skip()

    ds = ogr.Open('data/geoconcept/geoconcept_multipolygon_singlepart_hole.txt')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.1 0.1,0.1 0.9,0.9 0.9,0.1 0.1)))',
                                      max_error=0.000000001) != 0:
        feat.DumpReadable()
        pytest.fail()

    
###############################################################################
#


def test_ogr_gxt_multipolygon_twoparts_second_with_hole():

    if not ogrtest.have_geos():
        pytest.skip()

    ds = ogr.Open('data/geoconcept/geoconcept_multipolygon_twoparts_second_with_hole.txt')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((-10 -10,-10 -9,-9 -9,-10 -10)),((0 0,0 1,1 1,1 0,0 0),(0.1 0.1,0.1 0.9,0.9 0.9,0.1 0.1)))',
                                      max_error=0.000000001) != 0:
        feat.DumpReadable()
        pytest.fail()

    
###############################################################################
#


def test_ogr_gxt_line():

    if not ogrtest.have_geos():
        pytest.skip()

    ds = ogr.Open('data/geoconcept/line.gxt')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat, 'LINESTRING (440720 3751320,441920 3750120)',
                                      max_error=0.000000001) != 0:
        feat.DumpReadable()
        pytest.fail()

    
###############################################################################
#


def test_ogr_gxt_cleanup():

    gdaltest.gxt_ds = None
    try:
        os.remove('tmp/tmp.gxt')
    except OSError:
        pass
    


