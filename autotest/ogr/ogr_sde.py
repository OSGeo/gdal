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
    ds.Destroy()

    ds = ogr.Open(base, update=1)
    ds.Destroy()    

    return 'success'

def ogr_sde_2():
    "Test creation of a layer"
    if gdaltest.sde_dr is None:
        return 'skip'
    base = 'SDE:%s,%s,%s,%s,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password)
    ds = ogr.Open(base, update=1)
    lyr = ds.CreateLayer( 'SDE.TPOLY' ,geom_type=ogr.wkbPolygon, options = [ 'OVERWRITE=YES' ] )
#    lyr = ds.CreateLayer( 'SDE.TPOLY' ,geom_type=ogr.wkbPolygon)

    ogrtest.quick_create_layer_def( lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('WHEN', ogr.OFTDate) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
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
    version_name = 'TESTING'
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'TRUE' )

    base = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password, version_name)
    ds = ogr.Open(base, update=1)
    print "Layer Count: ", ds.GetLayerCount()
    
    for i in range(ds.GetLayerCount()):
        print ds.GetLayer(i).GetName()
    l1 = ds.GetLayerByName('SDE.TPOLY')

    f1 = l1.GetNextFeature()
    f1.SetField("PRFEDEA",'SDE.TESTING')
    l1.SetFeature(f1)
    
    ds.Destroy()

    default = 'DEFAULT'
    gdal.SetConfigOption( 'SDE_VERSIONOVERWRITE', 'TRUE' )

    default = 'SDE:%s,%s,%s,%s,%s,SDE.TPOLY,SDE.DEFAULT,%s' % (sde_server, sde_port, sde_db, sde_user, sde_password, default)
    ds2 = ogr.Open(base, update=1)

    l2 = ds.GetLayerByName('SDE.TPOLY')

    f2 = l2.GetNextFeature()
    f2.SetField("PRFEDEA",'SDE.DEFAULT')
    l2.SetFeature(f2)

    ds = ogr.Open(base)
    l1 = ds.GetLayerByName('SDE.TPOLY')
    f1 = l1.GetNextFeature()
    print f1.GetFID(), f1.GetField("PRFEDEA")
    ds.Destroy()

    ds = ogr.Open(default)
    l1 = ds.GetLayerByName('SDE.TPOLY')
    f1 = l1.GetNextFeature()
    print f1.GetFID(), f1.GetField("PRFEDEA")
    ds.Destroy()

        
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
    ogr_sde_cleanup 
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sde' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

