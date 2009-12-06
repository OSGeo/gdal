#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR ArcSDE driver.
# Author:   Howard Butler <hobu.inc@gmail.com>
# 
###############################################################################
# Copyright (c) 2008, Howard Butler <hobu.inc@gmail.com>
# 
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
###############################################################################

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

try:
    from osgeo import ogr
    from osgeo import osr
    from osgeo import gdal
except ImportError:
    import ogr
    import osr
    import gdal

###############################################################################
# Open ArcSDE datasource.

sde_server = '172.16.1.193'
sde_port = '5151'
sde_db = 'sde'
sde_user = 'sde'
sde_password = 'sde'

gdaltest.sde_dr = None
try:
    gdaltest.sde_dr = ogr.GetDriverByName( 'SDE' )
except:
    pass
def ogr_sde_1():
    "Test basic opening of a database"


    if gdaltest.sde_dr is None:
        return 'skip'
        
    base = 'SDE:%s,%s,%s,%s,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password)
    ds = ogr.Open(base)
    if ds is None:
        print("Couldn't open %s" % base)
        gdaltest.sde_dr = None
        return 'skip'
    ds.Destroy()

    ds = ogr.Open(base, update=1)
    ds.Destroy()    

    return 'success'

def ogr_sde_2():
    "Test creation of a layer"
    if gdaltest.sde_dr is None:
        return 'skip'
    base = 'SDE:%s,%s,%s,%s,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password)

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    ds = ogr.Open(base, update=1)
    lyr = ds.CreateLayer( 'SDE.TPOLY' ,geom_type=ogr.wkbPolygon, srs=shp_lyr.GetSpatialRef(),options = [ 'OVERWRITE=YES' ] )
#    lyr = ds.CreateLayer( 'SDE.TPOLY' ,geom_type=ogr.wkbPolygon)

    ogrtest.quick_create_layer_def( lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('WHEN', ogr.OFTDateTime) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )


    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
    return 'success'
    

def ogr_sde_3():
    "Test basic version locking"
    if gdaltest.sde_dr is None:
        return 'skip'
        
    base = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT' % (sde_server, sde_port, sde_db, sde_user, sde_password)
    ds = ogr.Open(base, update=1)

    ds2 = ogr.Open(base, update=1)
    if ds2 is not None:
        gdaltest.post_reason('A locked version was able to be opened')
        return 'fail'
        
    ds.Destroy()

    return 'success'


def ogr_sde_4():
    "Test basic version creation"


    if gdaltest.sde_dr is None:
        return 'skip'
    version_name = 'TESTING'
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'TRUE' )

    base = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password, version_name)
    ds = ogr.Open(base, update=1)
    ds.Destroy()
    
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'FALSE' )

    base = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password, version_name)
    ds = ogr.Open(base, update=1)
    ds.Destroy()


    return 'success'

def ogr_sde_5():
    "Test versioned editing"

    if gdaltest.sde_dr is None:
        return 'skip'
    version_name = 'TESTING'
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'TRUE' )

    base = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password, version_name)
    ds = ogr.Open(base, update=1)

    l1 = ds.GetLayerByName('SDE.TPOLY')

    f1 = l1.GetFeature(1)
    f1.SetField("PRFEDEA",'SDE.TESTING')
    l1.SetFeature(f1)
    
    ds.Destroy()
    del ds
    
    default = 'DEFAULT'
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'FALSE' )

    default = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password, default)
#    print default
    ds2 = ogr.Open(default, update=1)

    l2 = ds2.GetLayerByName('SDE.TPOLY')

    f2 = l2.GetFeature(1)
    
    f2.SetField("PRFEDEA",'SDE.DEFAULT')
    f2.SetField("WHEN", 2008, 3, 19, 16, 15, 00, 0)

    l2.SetFeature(f2)
    ds2.Destroy()
    del ds2
    
    ds3 = ogr.Open(base)
    l3 = ds3.GetLayerByName('SDE.TPOLY')
    f3 = l3.GetFeature(1)
    if f3.GetField("PRFEDEA") != "SDE.TESTING":
        gdaltest.post_reason('versioned editing failed for child version SDE.TESTING')
        return 'fail'


    ds3.Destroy()
    del ds3

    ds4 = ogr.Open(default)
    l4 = ds4.GetLayerByName('SDE.TPOLY')
    f4 = l4.GetFeature(1)
    if f4.GetField("PRFEDEA") != "SDE.DEFAULT":
        gdaltest.post_reason('versioned editing failed for parent version SDE.DEFAULT')
        return 'fail'


    idx = f4.GetFieldIndex('WHEN')
    df = f4.GetField(idx)
    if df != '2008/03/19 16:15:00':
        gdaltest.post_reason("datetime handling did not work -- expected '2008/03/19 16:15:00' got '%s' "% df)
    ds4.Destroy()
    del ds4
    return 'success'

def ogr_sde_6():
    "Extent fetching"

    if gdaltest.sde_dr is None:
        return 'skip'

    base = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT' % (sde_server, sde_port, sde_db, sde_user, sde_password)
    ds = ogr.Open(base, update=1)

    l1 = ds.GetLayerByName('SDE.TPOLY')
    extent = l1.GetExtent(force=0)
    if extent != (0.0, 2147483645.0, 0.0, 2147483645.0):
        gdaltest.post_reason("unforced extent did not equal expected value")
        

    extent = l1.GetExtent(force=1)
    if extent !=     (478316.0, 481645.0, 4762881.0, 4765611.0):
        gdaltest.post_reason("forced extent did not equal expected value")
    return 'success'

def ogr_sde_7():
    "Bad layer test"

    if gdaltest.sde_dr is None:
        return 'skip'

    base = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT' % (sde_server, sde_port, sde_db, sde_user, sde_password)
    ds = ogr.Open(base, update=1)

    l1 = ds.GetLayerByName('SDE.TPOLY2')
    if l1:
        gdaltest.post_reason("we got a layer when we shouldn't have")

    ds.Destroy()

    default = 'DEFAULT'
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'FALSE' )

    default = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password, default)
    ds = ogr.Open(default, update=1)

    l1 = ds.GetLayerByName('SDE.TPOLY2')
    if l1:
        gdaltest.post_reason("we got a layer when we shouldn't have")
    ds.Destroy()

    default = 'DEFAULT'
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'FALSE' )

    default = 'SDE:%s,%s,%s,%s,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password)
    ds = ogr.Open(default)

    l1 = ds.GetLayerByName('SDE.TPOLY2')
    if l1:
        gdaltest.post_reason("we got a layer when we shouldn't have")
    ds.Destroy()


    return 'success'

def ogr_sde_8():
    "Test spatial references"
    if gdaltest.sde_dr is None:
        return 'skip'
    base = 'SDE:%s,%s,%s,%s,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password)

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    ref = osr.SpatialReference()
    ref.ImportFromWkt('LOCAL_CS["IMAGE"]')
    
    ds = ogr.Open(base, update=1)
    lyr = ds.CreateLayer( 'SDE.TPOLY' ,geom_type=ogr.wkbPolygon, srs=ref,options = [ 'OVERWRITE=YES' ] )
    ref.ImportFromEPSG(4326)
    lyr = ds.CreateLayer( 'SDE.TPOLY' ,geom_type=ogr.wkbPolygon, srs=ref,options = [ 'OVERWRITE=YES' ] )
    ogrtest.quick_create_layer_def( lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('WHEN', ogr.OFTDateTime) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )


    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
    return 'success'
    
def ogr_sde_cleanup():
    if gdaltest.sde_dr is None:
        return 'skip'
    base = 'SDE:%s,%s,%s,%s,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password)
    ds = ogr.Open(base, update=1)
    ds.DeleteLayer('%s.%s'%(sde_user.upper(),'TPOLY'))
    ds.Destroy()    


    return 'success'

gdaltest_list = [ 
    ogr_sde_1,
    ogr_sde_2,
    ogr_sde_3,
    ogr_sde_4,
    ogr_sde_5,
    ogr_sde_6,
    ogr_sde_7,
    ogr_sde_8,
    
    ogr_sde_cleanup 
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sde' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

