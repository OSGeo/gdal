#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR INDEX support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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
import contextlib

from osgeo import ogr
import ogrtest
import pytest

###############################################################################


@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():
    yield

    for filename in ['join_t.idm', 'join_t.ind']:
        assert not os.path.exists(filename)

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource(
        'tmp/ogr_index_10.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource(
        'tmp/ogr_index_11.dbf')


@contextlib.contextmanager
def create_index_p_test_file():
    drv = ogr.GetDriverByName('MapInfo File')
    p_ds = drv.CreateDataSource('index_p.mif')
    p_lyr = p_ds.CreateLayer('index_p')

    ogrtest.quick_create_layer_def(p_lyr, [('PKEY', ogr.OFTInteger)])
    ogrtest.quick_create_feature(p_lyr, [5], None)
    ogrtest.quick_create_feature(p_lyr, [10], None)
    ogrtest.quick_create_feature(p_lyr, [9], None)
    ogrtest.quick_create_feature(p_lyr, [4], None)
    ogrtest.quick_create_feature(p_lyr, [3], None)
    ogrtest.quick_create_feature(p_lyr, [1], None)

    p_ds.Release()

    yield

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('index_p.mif')


@contextlib.contextmanager
def create_join_t_test_file(create_index=False):
    drv = ogr.GetDriverByName('ESRI Shapefile')
    s_ds = drv.CreateDataSource('join_t.dbf')
    s_lyr = s_ds.CreateLayer('join_t',
                             geom_type=ogr.wkbNone)

    ogrtest.quick_create_layer_def(s_lyr,
                                   [('SKEY', ogr.OFTInteger),
                                    ('VALUE', ogr.OFTString, 16)])

    for i in range(20):
        ogrtest.quick_create_feature(s_lyr, [i, 'Value ' + str(i)], None)

    if create_index:
        s_ds.ExecuteSQL('CREATE INDEX ON join_t USING value')
        s_ds.ExecuteSQL('CREATE INDEX ON join_t USING skey')

    s_ds.Release()

    yield

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('join_t.dbf')


###############################################################################
# Verify a simple join without indexing.


def test_ogr_index_can_join_without_index():
    expect = ['Value 5', 'Value 10', 'Value 9', 'Value 4', 'Value 3',
              'Value 1']

    with create_index_p_test_file(), create_join_t_test_file():
        p_ds = ogr.OpenShared('index_p.mif', update=0)

        sql_lyr = p_ds.ExecuteSQL(
            'SELECT * FROM index_p p ' +
            'LEFT JOIN "join_t.dbf".join_t j ON p.PKEY = j.SKEY ')

        tr = ogrtest.check_features_against_list(sql_lyr, 'VALUE', expect)

        p_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Create an INDEX on the SKEY and VALUE field in the join table.


def test_ogr_index_creating_index_causes_index_files_to_be_created():
    with create_join_t_test_file(create_index=True):
        for filename in ['join_t.idm', 'join_t.ind']:
            assert os.path.exists(filename)

###############################################################################
# Check that indexable single int lookup works.


def test_ogr_index_indexed_single_integer_lookup_works():
    expect = ['Value 5']

    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)

        s_lyr = s_ds.GetLayerByName('join_t')
        s_lyr.SetAttributeFilter('SKEY = 5')

        tr = ogrtest.check_features_against_list(s_lyr, 'VALUE', expect)
    assert tr

###############################################################################
# Check that indexable single string lookup works.


def test_ogr_index_indexed_single_string_works():
    expect = [5]

    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)
        s_lyr = s_ds.GetLayerByName('join_t')

        s_lyr.SetAttributeFilter("VALUE='Value 5'")

        tr = ogrtest.check_features_against_list(s_lyr, 'SKEY', expect)
    assert tr


###############################################################################
# Check that range query that isn't currently implemented using index works.


def test_ogr_index_unimplemented_range_query_works():
    expect = [0, 1, 2]

    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)
        s_lyr = s_ds.GetLayerByName('join_t')
        s_lyr.SetAttributeFilter('SKEY < 3')

        tr = ogrtest.check_features_against_list(s_lyr, 'SKEY', expect)

    assert tr

###############################################################################
# Try join again.


def test_ogr_index_indexed_join_works():
    expect = ['Value 5', 'Value 10', 'Value 9', 'Value 4', 'Value 3',
              'Value 1']

    with create_index_p_test_file(), \
            create_join_t_test_file(create_index=True):
        p_ds = ogr.OpenShared('index_p.mif', update=0)
        sql_lyr = p_ds.ExecuteSQL(
            'SELECT * FROM index_p p ' +
            'LEFT JOIN "join_t.dbf".join_t j ON p.PKEY = j.SKEY ')

        tr = ogrtest.check_features_against_list(sql_lyr, 'VALUE', expect)

        p_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test that dropping the index causes index files to be removed


def test_ogr_index_drop_index_removes_files():
    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)

        s_ds.ExecuteSQL('DROP INDEX ON join_t USING value')
        s_ds.ExecuteSQL('DROP INDEX ON join_t USING skey')

        s_ds.Release()

        # After dataset closing, check that the index files do not exist after
        # dropping the index
        for filename in ['join_t.idm', 'join_t.ind']:
            assert not os.path.exists(filename)

###############################################################################
# Test that attribute filters work after an index on that attribute is dropped


def test_ogr_index_attribute_filter_works_after_drop_index():
    expect = ['Value 5']

    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)
        s_lyr = s_ds.GetLayerByName('join_t')

        s_ds.ExecuteSQL('DROP INDEX ON join_t USING value')
        s_ds.ExecuteSQL('DROP INDEX ON join_t USING skey')

        s_lyr.SetAttributeFilter('SKEY = 5')

        tr = ogrtest.check_features_against_list(s_lyr, 'VALUE', expect)
        assert tr

###############################################################################
# Test that dropping, then re-creating the index causes index files to be
# opened


def test_ogr_index_recreating_index_causes_index_files_to_be_created():

    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)

        s_ds.ExecuteSQL('DROP INDEX ON join_t USING value')
        s_ds.ExecuteSQL('DROP INDEX ON join_t USING skey')

        s_ds.Release()

        # Re-create an index
        s_ds = ogr.OpenShared('join_t.dbf', update=1)
        s_ds.ExecuteSQL('CREATE INDEX ON join_t USING value')
        s_ds.Release()

        for filename in ['join_t.idm', 'join_t.ind']:
            try:
                os.stat(filename)
            except (OSError, FileNotFoundError):
                pytest.fail("%s should exist" % filename)

###############################################################################
# Test that dropping, then re-creating the index causes the correct index data
# to be placed in the re-created index file


def test_ogr_index_recreating_index_causes_index_to_be_populated():

    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)

        s_ds.ExecuteSQL('DROP INDEX ON join_t USING value')
        s_ds.ExecuteSQL('DROP INDEX ON join_t USING skey')

        s_ds.Release()

        # Re-create an index
        s_ds = ogr.OpenShared('join_t.dbf', update=1)
        s_ds.ExecuteSQL('CREATE INDEX ON join_t USING value')
        s_ds.Release()

        with open('join_t.idm', 'rt') as f:
            xml = f.read()
        assert xml.find('VALUE') != -1, 'VALUE column is not indexed (1)'

###############################################################################
# Text that adding an index to different columns in separate calls causes both
# columns to be indexed.


def test_ogr_index_creating_index_in_separate_steps_works():

    with create_join_t_test_file(create_index=True):
        s_ds = ogr.OpenShared('join_t.dbf', update=1)

        s_ds.ExecuteSQL('DROP INDEX ON join_t USING value')
        s_ds.ExecuteSQL('DROP INDEX ON join_t USING skey')

        s_ds.Release()

        # Re-create an index
        s_ds = ogr.OpenShared('join_t.dbf', update=1)
        s_ds.ExecuteSQL('CREATE INDEX ON join_t USING value')
        s_ds.Release()

        # Close the dataset and re-open
        s_ds = ogr.OpenShared('join_t.dbf', update=1)
        # At this point the .ind was opened in read-only. Now it
        # will be re-opened in read-write mode
        s_ds.ExecuteSQL('CREATE INDEX ON join_t USING skey')

        s_ds.Release()

        with open('join_t.idm', 'rt') as f:
            xml = f.read()
        assert xml.find('VALUE') != -1, 'VALUE column is not indexed (2)'
        assert xml.find('SKEY') != -1, 'SKEY column is not indexed (2)'

###############################################################################
# Test fix for #4326


def test_ogr_index_10():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource(
        'tmp/ogr_index_10.shp'
    )
    lyr = ds.CreateLayer('ogr_index_10')
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(1, 1)
    feat.SetField(2, "foo")
    lyr.CreateFeature(feat)
    feat = None
    ds.ExecuteSQL('create index on ogr_index_10 using intfield')
    ds.ExecuteSQL('create index on ogr_index_10 using realfield')

    lyr.SetAttributeFilter('intfield IN (1)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('intfield = 1')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('intfield IN (2)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    lyr.SetAttributeFilter('intfield IN (1.0)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('intfield = 1.0')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('intfield IN (1.1)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    lyr.SetAttributeFilter("intfield IN ('1')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('realfield IN (1.0)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('realfield = 1.0')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('realfield IN (1.1)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    lyr.SetAttributeFilter('realfield IN (1)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('realfield = 1')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter('realfield IN (2)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    lyr.SetAttributeFilter("realfield IN ('1')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter("strfield IN ('foo')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter("strfield = 'foo'")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None

    lyr.SetAttributeFilter("strfield IN ('bar')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None

    ds = None

###############################################################################
# Test support for OR and AND expression


def ogr_index_11_check(lyr, expected_fids):

    lyr.ResetReading()
    for expected_fid in expected_fids:
        feat = lyr.GetNextFeature()
        assert feat is not None
        assert feat.GetFID() == expected_fid


def test_ogr_index_11():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource(
        'tmp/ogr_index_11.dbf'
    )
    lyr = ds.CreateLayer('ogr_index_11', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))

    ogrtest.quick_create_feature(lyr, [1, "foo"], None)
    ogrtest.quick_create_feature(lyr, [1, "bar"], None)
    ogrtest.quick_create_feature(lyr, [2, "foo"], None)
    ogrtest.quick_create_feature(lyr, [2, "bar"], None)
    ogrtest.quick_create_feature(lyr, [3, "bar"], None)

    ds.ExecuteSQL('CREATE INDEX ON ogr_index_11 USING intfield')
    ds.ExecuteSQL('CREATE INDEX ON ogr_index_11 USING strfield')

    lyr.SetAttributeFilter("intfield = 1 OR strfield = 'bar'")
    ogr_index_11_check(lyr, [0, 1, 3])

    lyr.SetAttributeFilter("intfield = 1 AND strfield = 'bar'")
    ogr_index_11_check(lyr, [1])

    lyr.SetAttributeFilter("intfield = 1 AND strfield = 'foo'")
    ogr_index_11_check(lyr, [0])

    lyr.SetAttributeFilter("intfield = 3 AND strfield = 'foo'")
    ogr_index_11_check(lyr, [])

    lyr.SetAttributeFilter("intfield IN (1, 2, 3)")
    ogr_index_11_check(lyr, [0, 1, 2, 3, 4])

    ds = None
