#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR INDEX support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import ogr
import ogrtest

###############################################################################
# Create a MIF file to be our primary table. 

def ogr_index_1():

    from osgeo import gdal
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    try:
        ogr.GetDriverByName( 'MapInfo File' ).DeleteDataSource( 'index_p.mif' )
    except:
        pass
    try:
        ogr.GetDriverByName( 'ESRI Shapefile' ).DeleteDataSource( 'join_t.dbf')
    except:
        pass

    gdal.PopErrorHandler()
    
    drv = ogr.GetDriverByName('MapInfo File')
    gdaltest.p_ds = drv.CreateDataSource( 'index_p.mif' )
    gdaltest.p_lyr = gdaltest.p_ds.CreateLayer( 'index_p' )

    ogrtest.quick_create_layer_def( gdaltest.p_lyr,[('PKEY', ogr.OFTInteger)] )
    ogrtest.quick_create_feature( gdaltest.p_lyr, [5], None )
    ogrtest.quick_create_feature( gdaltest.p_lyr, [10], None )
    ogrtest.quick_create_feature( gdaltest.p_lyr, [9], None )
    ogrtest.quick_create_feature( gdaltest.p_lyr, [4], None )
    ogrtest.quick_create_feature( gdaltest.p_lyr, [3], None )
    ogrtest.quick_create_feature( gdaltest.p_lyr, [1], None )

# It turns out mapinfo format doesn't allow GetFeatureCount() calls while
# writing ... it just blows an assert!
#    if gdaltest.p_lyr.GetFeatureCount() != 7:
#        gdaltest.post_reason( 'FeatureCount wrong' )
#        return 'failure'

    # Close and reopen, since it seems the .mif driver does not allow reading
    # from a newly created (updatable) file.

    gdaltest.p_ds = None
    gdaltest.p_ds = ogr.OpenShared( 'index_p.mif', update = 0 )
    gdaltest.p_lyr = gdaltest.p_ds.GetLayerByName( 'index_p' )

    return 'success'

###############################################################################
# Create a dbf file to be our secondary table.  Close it, and reopen shared.

def ogr_index_2():

    drv = ogr.GetDriverByName('ESRI Shapefile')
    gdaltest.s_ds = drv.CreateDataSource( 'join_t.dbf' )
    gdaltest.s_lyr = gdaltest.s_ds.CreateLayer( 'join_t',
                                                geom_type = ogr.wkbNone )

    ogrtest.quick_create_layer_def( gdaltest.s_lyr,
                                    [('SKEY', ogr.OFTInteger),
                                     ('VALUE', ogr.OFTString, 16)] )

    for i in range(20):
        ogrtest.quick_create_feature( gdaltest.s_lyr, [i,'Value '+str(i)],None)

    if gdaltest.s_lyr.GetFeatureCount() != 20:
        gdaltest.post_reason( 'FeatureCount wrong' )
        return 'failure'

    gdaltest.s_ds.Release()
    gdaltest.s_lyr = None
    gdaltest.s_ds = None
    
    gdaltest.s_ds = ogr.OpenShared( 'join_t.dbf', update = 1 )
    gdaltest.s_lyr = gdaltest.s_ds.GetLayerByName( 'join_t' )
                                  
    return 'success'

###############################################################################
# Verify a simple join without indexing.

def ogr_index_3():

    expect = [ 'Value 5', 'Value 10', 'Value 9', 'Value 4', 'Value 3',
               'Value 1' ]
    
    sql_lyr = gdaltest.p_ds.ExecuteSQL( \
        'SELECT * FROM index_p p ' \
        + 'LEFT JOIN "join_t.dbf".join_t j ON p.PKEY = j.SKEY ' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'VALUE', expect )

    gdaltest.p_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create an INDEX on the SKEY and VALUE field in the join table.

def ogr_index_4():

    gdaltest.s_ds.ExecuteSQL( 'CREATE INDEX ON join_t USING value' )
    gdaltest.s_ds.ExecuteSQL( 'CREATE INDEX ON join_t USING skey' )

    return 'success'

###############################################################################
# Check that indexable single int lookup works.

def ogr_index_5():

    gdaltest.s_lyr.SetAttributeFilter( 'SKEY = 5' )
    
    expect = [ 'Value 5' ]

    tr = ogrtest.check_features_against_list( gdaltest.s_lyr, 'VALUE', expect )
    if tr:
        return 'success'
    else:
        return 'fail'
        
###############################################################################
# Check that indexable single string lookup works.
#
# We also close the datasource and reopen to ensure that reloaded indexes
# work OK too.

def ogr_index_6():

    gdaltest.s_ds.Release()
    gdaltest.s_ds = ogr.OpenShared( 'join_t.dbf', update = 1 )
    gdaltest.s_lyr = gdaltest.s_ds.GetLayerByName( 'join_t' )
                                  
    gdaltest.s_lyr.SetAttributeFilter( "VALUE='Value 5'" )
    
    expect = [ 5 ]
    
    tr = ogrtest.check_features_against_list( gdaltest.s_lyr, 'SKEY', expect )
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Check that range query that isn't currently implemented using index works.

def ogr_index_7():

    gdaltest.s_lyr.SetAttributeFilter( 'SKEY < 3' )
    
    expect = [ 0, 1, 2 ]
    
    tr = ogrtest.check_features_against_list( gdaltest.s_lyr, 'SKEY', expect )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Try join again.

def ogr_index_8():

    expect = [ 'Value 5', 'Value 10', 'Value 9', 'Value 4', 'Value 3',
               'Value 1' ]
    
    sql_lyr = gdaltest.p_ds.ExecuteSQL( \
        'SELECT * FROM index_p p ' \
        + 'LEFT JOIN "join_t.dbf".join_t j ON p.PKEY = j.SKEY ' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'VALUE', expect )

    gdaltest.p_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that dropping both indexes gets rid of them, and that results still
# work.

def ogr_index_9():

    gdaltest.s_ds.ExecuteSQL( 'DROP INDEX ON join_t USING value' )
    gdaltest.s_ds.ExecuteSQL( 'DROP INDEX ON join_t USING skey' )

    gdaltest.s_lyr.SetAttributeFilter( 'SKEY = 5' )
    
    expect = [ 'Value 5' ]

    tr = ogrtest.check_features_against_list( gdaltest.s_lyr, 'VALUE', expect )
    if not tr:
        return 'fail'
    
    gdaltest.s_ds.Release()
    
    # After dataset closing, check that the index files do not exist after
    # dropping the index
    for filename in ['join_t.idm','join_t.ind']:
        try:
            os.stat(filename)
            gdaltest.post_reason("%s shouldn't exist" % filename)
            return 'fail'
        except:
            pass

    # Re-create an index
    gdaltest.s_ds = ogr.OpenShared( 'join_t.dbf', update = 1 )
    gdaltest.s_ds.ExecuteSQL( 'CREATE INDEX ON join_t USING value' )
    gdaltest.s_ds.Release()
    
    for filename in ['join_t.idm','join_t.ind']:
        try:
            os.stat(filename)
        except:
            gdaltest.post_reason("%s should exist" % filename)
            return 'fail'
            pass

    f = open('join_t.idm', 'rt')
    xml = f.read()
    f.close()
    if xml.find('VALUE') == -1:
        gdaltest.post_reason('VALUE column is not indexed (1)')
        print(xml)
        return 'fail'

    # Close the dataset and re-open
    gdaltest.s_ds = ogr.OpenShared( 'join_t.dbf', update = 1 )
    # At this point the .ind was opened in read-only. Now it
    # will be re-opened in read-write mode
    gdaltest.s_ds.ExecuteSQL( 'CREATE INDEX ON join_t USING skey' )

    gdaltest.s_ds.Release()

    f = open('join_t.idm', 'rt')
    xml = f.read()
    f.close()
    if xml.find('VALUE') == -1:
        gdaltest.post_reason('VALUE column is not indexed (2)')
        print(xml)
        return 'fail'
    if xml.find('SKEY') == -1:
        gdaltest.post_reason('SKEY column is not indexed (2)')
        print(xml)
        return 'fail'

    return 'success'

###############################################################################
# Test fix for #4326

def ogr_index_10():

    ds = ogr.GetDriverByName( 'ESRI Shapefile' ).CreateDataSource('tmp/ogr_index_10.shp')
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
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('intfield = 1')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('intfield IN (2)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('intfield IN (1.0)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('intfield = 1.0')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('intfield IN (1.1)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('failed')
        return 'fail'
        
    lyr.SetAttributeFilter("intfield IN ('1')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'


    lyr.SetAttributeFilter('realfield IN (1.0)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('realfield = 1.0')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('realfield IN (1.1)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('realfield IN (1)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('realfield = 1')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter('realfield IN (2)')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter("realfield IN ('1')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'


    lyr.SetAttributeFilter("strfield IN ('foo')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter("strfield = 'foo'")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.SetAttributeFilter("strfield IN ('bar')")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('failed')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test support for OR and AND expression

def ogr_index_11_check(lyr, expected_fids):

    lyr.ResetReading()
    for i in range(len(expected_fids)):
        feat = lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('failed')
            return 'fail'
        if feat.GetFID() != expected_fids[i]:
            gdaltest.post_reason('failed')
            return 'fail'

    return 'success'

def ogr_index_11():

    ds = ogr.GetDriverByName( 'ESRI Shapefile' ).CreateDataSource('tmp/ogr_index_11.dbf')
    lyr = ds.CreateLayer('ogr_index_11', geom_type = ogr.wkbNone)
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
    ret = ogr_index_11_check(lyr, [ 0, 1, 3 ])
    if ret != 'success':
        return ret

    lyr.SetAttributeFilter("intfield = 1 AND strfield = 'bar'")
    ret = ogr_index_11_check(lyr, [ 1 ])
    if ret != 'success':
        return ret

    lyr.SetAttributeFilter("intfield = 1 AND strfield = 'foo'")
    ret = ogr_index_11_check(lyr, [ 0 ])
    if ret != 'success':
        return ret

    lyr.SetAttributeFilter("intfield = 3 AND strfield = 'foo'")
    ret = ogr_index_11_check(lyr, [ ])
    if ret != 'success':
        return ret

    ds = None

    return 'success'

###############################################################################

def ogr_index_cleanup():
    try:
        gdaltest.p_ds.Release()
    except:
        pass

    gdaltest.p_ds = None
    gdaltest.s_ds = None

    gdaltest.p_lyr = None
    gdaltest.s_lyr = None

    ogr.GetDriverByName( 'MapInfo File' ).DeleteDataSource( 'index_p.mif' )
    ogr.GetDriverByName( 'ESRI Shapefile' ).DeleteDataSource( 'join_t.dbf' )

    for filename in ['join_t.idm','join_t.ind']:
        try:
            os.stat(filename)
            gdaltest.post_reason("%s shouldn't exist" % filename)
            return 'fail'
        except:
            pass

    ogr.GetDriverByName( 'ESRI Shapefile' ).DeleteDataSource( 'tmp/ogr_index_10.shp' )
    ogr.GetDriverByName( 'ESRI Shapefile' ).DeleteDataSource( 'tmp/ogr_index_11.dbf' )

    return 'success'

gdaltest_list = [ 
    ogr_index_1,
    ogr_index_2,
    ogr_index_3,
    ogr_index_4,
    ogr_index_5,
    ogr_index_6,
    ogr_index_7,
    ogr_index_8,
    ogr_index_9,
    ogr_index_10,
    ogr_index_11,
    ogr_index_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_index_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
