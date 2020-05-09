#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR DODS driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal
import pytest

###############################################################################
# Open DODS datasource.


@pytest.mark.skip()
def test_ogr_dods_1():
    gdaltest.dods_ds = None
    ogrtest.dods_drv = ogr.GetDriverByName('DODS')

    if ogrtest.dods_drv is None:
        pytest.skip()

    gdal.SetConfigOption('DODS_AIS_FILE', 'data/dods/ais.xml')

    srv = 'http://www.epic.noaa.gov:10100/dods/wod2001/natl_prof_bot.cdp?&_id=1'
    if gdaltest.gdalurlopen(srv) is None:
        gdaltest.dods_ds = None
        pytest.skip()

    gdaltest.dods_ds = ogr.Open('DODS:' + srv)

    assert gdaltest.dods_ds is not None

    try:
        gdaltest.dods_profiles = gdaltest.dods_ds.GetLayerByName('profiles')
        gdaltest.dods_normalized = gdaltest.dods_ds.GetLayerByName('normalized')
        gdaltest.dods_lines = gdaltest.dods_ds.GetLayerByName('lines')
    except:
        gdaltest.dods_profiles = None
        gdaltest.dods_normalized = None

    if gdaltest.dods_profiles is None:
        gdaltest.dods_ds = None
        pytest.fail('profiles layer missing, likely AIS stuff not working.')
    
###############################################################################
# Read a single feature from the profiles layer and verify a few things.
#


@pytest.mark.skip()
def test_ogr_dods_2():

    if gdaltest.dods_ds is None:
        pytest.skip()

    gdaltest.dods_profiles.ResetReading()
    feat = gdaltest.dods_profiles.GetNextFeature()

    assert feat.GetField('time') == -1936483200000, 'time wrong'

    assert feat.GetField('profile.depth') == [0, 10, 20, 30, 39], 'depth wrong'

    assert ogrtest.check_feature_geometry(feat, 'POINT (4.30000019 5.36999989)') == 0

    feat.Destroy()

    feat = gdaltest.dods_profiles.GetNextFeature()
    if feat is not None:
        feat.Destroy()
        pytest.fail('got more than expected number of features.')

    
###############################################################################
# Read the normalized form of the same profile, and verify some values.
#


@pytest.mark.skip()
def test_ogr_dods_3():

    if gdaltest.dods_ds is None:
        pytest.skip()

    gdaltest.dods_normalized.ResetReading()
    expect = [0, 10, 20, 30, 39]
    tr = ogrtest.check_features_against_list(gdaltest.dods_normalized,
                                             'depth', expect)
    assert tr != 0

    expected = [14.8100004196167, 14.8100004196167, 14.8100004196167, 14.60999965667725, 14.60999965667725]

    gdaltest.dods_normalized.ResetReading()
    for i in range(5):
        feat = gdaltest.dods_normalized.GetNextFeature()

        assert feat.GetField('time') == -1936483200000, 'time wrong'

        assert feat.GetField('T_20') == pytest.approx(expected[i], abs=0.001), 'T_20 wrong'

        assert ogrtest.check_feature_geometry(feat, 'POINT (4.30000019 5.36999989)') == 0

        feat.Destroy()
        feat = None

    feat = gdaltest.dods_normalized.GetNextFeature()
    if feat is not None:
        feat.Destroy()
        pytest.fail('got more than expected number of features.')

    
###############################################################################
# Read the "lines" from from the same server and verify some values.
#


@pytest.mark.skip()
def test_ogr_dods_4():

    if gdaltest.dods_ds is None:
        pytest.skip()

    gdaltest.dods_lines.ResetReading()
    feat = gdaltest.dods_lines.GetNextFeature()

    assert feat.GetField('time') == -1936483200000, 'time wrong'

    assert feat.GetField('profile.depth') == [0, 10, 20, 30, 39], 'depth wrong'

    wkt_geom = 'LINESTRING (0.00000000 14.81000042,10.00000000 14.81000042,20.00000000 14.81000042,30.00000000 14.60999966,39.00000000 14.60999966)'

    assert ogrtest.check_feature_geometry(feat, wkt_geom) == 0, \
        feat.GetGeometryRef().ExportToWkt()

    feat.Destroy()

    feat = gdaltest.dods_lines.GetNextFeature()
    if feat is not None:
        feat.Destroy()
        pytest.fail('got more than expected number of features.')

    

###############################################################################
# Simple 1D Grid.
#

@pytest.mark.skip()
def test_ogr_dods_5():

    if ogrtest.dods_drv is None:
        pytest.skip()

    srv = 'http://uhslc1.soest.hawaii.edu/cgi-bin/nph-nc/fast/m004.nc.dds'
    if gdaltest.gdalurlopen(srv) is None:
        pytest.skip()

    grid_ds = ogr.Open('DODS:' + srv)
    assert grid_ds is not None

    lat_lyr = grid_ds.GetLayerByName('latitude')

    expect = [-0.53166663646698]
    tr = ogrtest.check_features_against_list(lat_lyr, 'latitude', expect)
    assert tr != 0

###############################################################################
#


@pytest.mark.skip()
def test_ogr_dods_cleanup():

    if gdaltest.dods_ds is None:
        pytest.skip()

    gdaltest.dods_profiles = None
    gdaltest.dods_lines = None
    gdaltest.dods_normalized = None
    gdaltest.dods_ds.Destroy()
    gdaltest.dods_ds = None
