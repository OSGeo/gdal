#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Some DGN Driver features.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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



import gdaltest
import ogrtest
from osgeo import ogr
import pytest

###############################################################################
# Verify we can open the test file.


def test_ogr_dgn_1():

    gdaltest.dgn_ds = ogr.Open('data/smalltest.dgn')
    assert gdaltest.dgn_ds is not None, 'failed to open test file.'

    gdaltest.dgn_lyr = gdaltest.dgn_ds.GetLayer(0)

###############################################################################
# Check first feature, a text element.


def test_ogr_dgn_2():
    if gdaltest.dgn_ds is None:
        pytest.skip()

    feat = gdaltest.dgn_lyr.GetNextFeature()
    assert feat.GetField('Type') == 17 and feat.GetField('Level') == 1, \
        'feature 1: expected attributes'

    assert feat.GetField('Text') == 'Demo Text', 'feature 1: expected text'

    assert not ogrtest.check_feature_geometry(feat, 'POINT (0.73650000 4.21980000)')

    assert feat.GetStyleString() == 'LABEL(t:"Demo Text",c:#ffffff,s:1.000g,f:ENGINEERING)', \
        'Style string different than expected.'

###############################################################################
# Check second feature, a circle.


def test_ogr_dgn_3():
    if gdaltest.dgn_ds is None:
        pytest.skip()

    feat = gdaltest.dgn_lyr.GetNextFeature()
    assert feat.GetField('Type') == 15 and feat.GetField('Level') == 2, \
        'feature 2: expected attributes'

    geom = feat.GetGeometryRef()
    assert geom.GetCoordinateDimension() == 2, 'expected 2d circle.'

    assert geom.GetGeometryName() == 'LINESTRING', \
        'Expected circle to be translated as LINESTRING.'

    assert geom.GetPointCount() >= 15, \
        'Unexpected small number of circle interpolation points.'

    genvelope = geom.GetEnvelope()
    assert genvelope[0] >= 0.328593 and genvelope[0] <= 0.328594 and genvelope[1] >= 9.68780 and genvelope[1] <= 9.68781 and genvelope[2] >= -0.09611 and genvelope[2] <= -0.09610 and genvelope[3] >= 9.26310 and genvelope[3] <= 9.26311, \
        'geometry extents seem odd'

###############################################################################
# Check third feature, a polygon with fill styling.


def test_ogr_dgn_4():
    if gdaltest.dgn_ds is None:
        pytest.skip()

    feat = gdaltest.dgn_lyr.GetNextFeature()
    assert feat.GetField('Type') == 6 and feat.GetField('Level') == 2 and feat.GetField('ColorIndex') == 83, \
        'feature 3: expected attributes'

    wkt = 'POLYGON ((4.53550000 3.31700000,4.38320000 2.65170000,4.94410000 2.52350000,4.83200000 3.33310000,4.53550000 3.31700000))'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    assert feat.GetStyleString() == 'BRUSH(fc:#b40000,id:"ogr-brush-0")', \
        'Style string different than expected.'

    gdaltest.dgn_lyr.ResetReading()

###############################################################################
# Use attribute query to pick just the type 15 level 2 object.


def test_ogr_dgn_5():

    if gdaltest.dgn_ds is None:
        pytest.skip()

    gdaltest.dgn_lyr.SetAttributeFilter('Type = 15 and Level = 2')
    tr = ogrtest.check_features_against_list(gdaltest.dgn_lyr, 'Type', [15])
    gdaltest.dgn_lyr.SetAttributeFilter(None)

    assert tr

###############################################################################
# Use spatial filter to just pick the big circle.


def test_ogr_dgn_6():

    if gdaltest.dgn_ds is None:
        pytest.skip()

    geom = ogr.CreateGeometryFromWkt('LINESTRING(1.0 8.55, 2.5 6.86)')
    gdaltest.dgn_lyr.SetSpatialFilter(geom)
    geom.Destroy()

    tr = ogrtest.check_features_against_list(gdaltest.dgn_lyr, 'Type', [15])
    gdaltest.dgn_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Copy our small dgn file to a new dgn file.


def test_ogr_dgn_7():

    if gdaltest.dgn_ds is None:
        pytest.skip()

    co_opts = ['UOR_PER_SUB_UNIT=100', 'SUB_UNITS_PER_MASTER_UNIT=100',
               'ORIGIN=-50,-50,0']

    dgn2_ds = ogr.GetDriverByName('DGN').CreateDataSource('tmp/dgn7.dgn',
                                                          options=co_opts)

    dgn2_lyr = dgn2_ds.CreateLayer('elements')

    gdaltest.dgn_lyr.ResetReading()

    dst_feat = ogr.Feature(feature_def=dgn2_lyr.GetLayerDefn())

    feat = gdaltest.dgn_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert dgn2_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

        feat = gdaltest.dgn_lyr.GetNextFeature()

    dgn2_lyr = None
    dgn2_ds = None

###############################################################################
# Verify that our copy is pretty similar.
#
# Currently the styling information is not well preserved.  Eventually
# this should be fixed up and the test made more stringent.
#


def test_ogr_dgn_8():

    if gdaltest.dgn_ds is None:
        pytest.skip()

    dgn2_ds = ogr.Open('tmp/dgn7.dgn')

    dgn2_lyr = dgn2_ds.GetLayerByName('elements')

    # Test first first, a text element.
    feat = dgn2_lyr.GetNextFeature()
    assert feat.GetField('Type') == 17 and feat.GetField('Level') == 1, \
        'feature 1: expected attributes'

    assert feat.GetField('Text') == 'Demo Text', 'feature 1: expected text'

    assert not ogrtest.check_feature_geometry(feat, 'POINT (0.73650000 4.21980000)')

    assert feat.GetStyleString() == 'LABEL(t:"Demo Text",c:#ffffff,s:1.000g,f:ENGINEERING)', \
        'feature 1: Style string different than expected.'

    # Check second element, a circle.

    feat = dgn2_lyr.GetNextFeature()
    assert feat.GetField('Type') == 12 and feat.GetField('Level') == 2, \
        'feature 2: expected attributes'

    geom = feat.GetGeometryRef()
    assert geom.GetCoordinateDimension() == 2, 'feature 2: expected 2d circle.'

    assert geom.GetGeometryName() == 'MULTILINESTRING', \
        'feature 2: Expected MULTILINESTRING.'

    genvelope = geom.GetEnvelope()
    assert genvelope[0] >= 0.3285 and genvelope[0] <= 0.3287 and genvelope[1] >= 9.6878 and genvelope[1] <= 9.6879 and genvelope[2] >= -0.0962 and genvelope[2] <= -0.0960 and genvelope[3] >= 9.26310 and genvelope[3] <= 9.2632, \
        'feature 2: geometry extents seem odd'

    # Check 3rd feature, a polygon

    feat = dgn2_lyr.GetNextFeature()
    assert feat.GetField('Type') == 6 and feat.GetField('Level') == 2 and feat.GetField('ColorIndex') == 83, \
        'feature 3: expected attributes'

    wkt = 'POLYGON ((4.53550000 3.31700000,4.38320000 2.65170000,4.94410000 2.52350000,4.83200000 3.33310000,4.53550000 3.31700000))'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    # should be: 'BRUSH(fc:#b40000,id:"ogr-brush-0")'
    assert feat.GetStyleString() == 'PEN(id:"ogr-pen-0",c:#b40000)', \
        ('feature 3: Style string different than expected: ' + feat.GetStyleString())

    dgn2_ds = None

###############################################################################
# Test delta encoding (#6806)


def test_ogr_dgn_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/dgn/DGNSample_v7.dgn', 'DGNSample_v7.dgn'):
        pytest.skip()

    ds = ogr.Open('tmp/cache/DGNSample_v7.dgn')
    assert ds is not None
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(35)
    wkt = 'LINESTRING (82.9999500717185 23.2084166997284,83.0007450788903 23.2084495986816,83.00081490524 23.2068095339824,82.9999503769036 23.2067737968078)'

    assert not ogrtest.check_feature_geometry(feat, wkt)

###############################################################################
#  Cleanup


def test_ogr_dgn_cleanup():

    if gdaltest.dgn_ds is not None:
        gdaltest.dgn_lyr = None
        gdaltest.dgn_ds = None

    gdaltest.clean_tmp()



