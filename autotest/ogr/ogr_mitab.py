#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MapInfo driver testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import test_cli_utilities

###############################################################################
# Create TAB file.

def ogr_mitab_1():

    gdaltest.mapinfo_drv = ogr.GetDriverByName('MapInfo File')
    gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource( 'tmp' )

    if gdaltest.mapinfo_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_mitab_2():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    # This should convert to MapInfo datum name 'New_Zealand_GD49'
    WEIRD_SRS = 'PROJCS["NZGD49 / UTM zone 59S",GEOGCS["NZGD49",DATUM["NZGD49",SPHEROID["International 1924",6378388,297,AUTHORITY["EPSG","7022"]],TOWGS84[59.47,-5.04,187.44,0.47,-0.1,1.024,-4.5993],AUTHORITY["EPSG","6272"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4272"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",171],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","27259"]]'
    gdaltest.mapinfo_srs = osr.SpatialReference()
    gdaltest.mapinfo_srs.ImportFromWkt(WEIRD_SRS)

    #######################################################
    # Create memory Layer
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.CreateLayer('tpoly', gdaltest.mapinfo_srs)

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.mapinfo_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.mapinfo_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.mapinfo_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    #######################################################
    # Close file.

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.
#
# Note that we allow a fairly significant error since projected
# coordinates are not stored with much precision in Mapinfo format.

def ogr_mitab_3():
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    gdaltest.mapinfo_ds = ogr.Open( 'tmp' )
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]

    gdaltest.mapinfo_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.mapinfo_lyr,
                                              'eas_id', expect )
    gdaltest.mapinfo_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mapinfo_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.02 ) != 0:
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason( 'Attribute %d does not match' % fld )
                return 'fail'

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None
    gdaltest.shp_ds.Destroy()

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_mitab_4():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    sql_lyr = gdaltest.mapinfo_ds.ExecuteSQL( \
        'select * from tpoly where prfedea = "35043413"' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '35043413' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))', max_error = 0.02 ) != 0:
            tr = 0
        feat_read.Destroy()

    gdaltest.mapinfo_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test spatial filtering.

def ogr_mitab_5():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    gdaltest.mapinfo_lyr.SetAttributeFilter( None )

    gdaltest.mapinfo_lyr.SetSpatialFilterRect( 479505, 4763195,
                                               480526, 4762819 )

    tr = ogrtest.check_features_against_list( gdaltest.mapinfo_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.mapinfo_lyr.SetSpatialFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that Non-WGS84 datums are populated correctly

def ogr_mitab_6():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    srs = gdaltest.mapinfo_lyr.GetSpatialRef()
    datum_name = srs.GetAttrValue('PROJCS|GEOGCS|DATUM')

    if datum_name != "New_Zealand_GD49":
        gdaltest.post_reason("Datum name does not match (expected 'New_Zealand_GD49', got '%s')" % datum_name)
        return 'fail'

    return 'success'

###############################################################################
# Create MIF file.

def ogr_mitab_7():

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource( 'tmp' )

    gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource( 'tmp/wrk.mif' )

    if gdaltest.mapinfo_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_mitab_8():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    #######################################################
    # Create memory Layer
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.CreateLayer( 'tpoly' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.mapinfo_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.mapinfo_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.mapinfo_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    #######################################################
    # Close file.

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_mitab_9():
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    gdaltest.mapinfo_ds = ogr.Open( 'tmp' )
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]

    gdaltest.mapinfo_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.mapinfo_lyr,
                                              'eas_id', expect )
    gdaltest.mapinfo_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mapinfo_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.000000001 ) != 0:
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason( 'Attribute %d does not match' % fld )
                return 'fail'

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None
    gdaltest.shp_ds.Destroy()

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Read mif file with 2 character .mid delimeter and verify operation.

def ogr_mitab_10():
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/small.mif' )
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    if feat.NAME != " S. 11th St.":
        gdaltest.post_reason( 'name attribute wrong.' )
        return 'fail'

    if feat.FLOODZONE != 10:
        gdaltest.post_reason( 'FLOODZONE attribute wrong.' )
        return 'fail'

    if ogrtest.check_feature_geometry(feat,
                                      'POLYGON ((407131.721 155322.441,407134.468 155329.616,407142.741 155327.242,407141.503 155322.467,407140.875 155320.049,407131.721 155322.441))',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.OWNER != 'Guarino "Chucky" Sandra':
        gdaltest.post_reason( 'owner attribute wrong.' )
        return 'fail'

    lyr = None
    ds.Destroy()

    return 'success'

###############################################################################
# Verify support for NTF datum with non-greenwich datum per
#    http://trac.osgeo.org/gdal/ticket/1416
#
# This test also excercises srs reference counting as described in issue:
#    http://trac.osgeo.org/gdal/ticket/1680

def ogr_mitab_11():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/small_ntf.mif' )
    srs = ds.GetLayer(0).GetSpatialRef()
    ds = None

    pm_value = srs.GetAttrValue( 'PROJCS|GEOGCS|PRIMEM',1)
    if pm_value[:6] != '2.3372':
        gdaltest.post_reason( 'got unexpected prime meridian, not paris: ' + pm_value )
        return 'fail'

    return 'success'

###############################################################################
# Verify that a newly created mif layer returns a non null layer definition

def ogr_mitab_12():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = gdaltest.mapinfo_drv.CreateDataSource( 'tmp', options = ['FORMAT=MIF'] )
    lyr = ds.CreateLayer( 'testlyrdef' )
    defn = lyr.GetLayerDefn()

    if defn is None:
        return 'fail'

    ogrtest.quick_create_layer_def( lyr, [ ('AREA', ogr.OFTReal) ] )

    ds.Destroy()
    return 'success'

###############################################################################
# Verify that field widths and precisions are propogated correctly in TAB

def ogr_mitab_13():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    import ogr_gml_read
    if ogr_gml_read.ogr_gml_1() != 'success':
        return 'skip'

    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        os.stat('tmp/testlyrdef.tab')
        ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.tab')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f "MapInfo File" tmp/testlyrdef.tab ../ogr/data/testlyrdef.gml')

    ds = ogr.Open('tmp/testlyrdef.tab')

    #Check if the width and precision are as preserved.
    lyr = ds.GetLayer('testlyrdef')
    if lyr is None:
        gdaltest.post_reason( 'Layer missing.' )
        return 'fail'

    defn = lyr.GetLayerDefn()

    data = [['AREA',  ogr.OFTReal,   7, 4],
            ['VOLUME',ogr.OFTReal,   0, 0],
            ['LENGTH',ogr.OFTInteger,10,0],
            ['WIDTH', ogr.OFTInteger, 4,0]]

    for field in data:
        fld = defn.GetFieldDefn(defn.GetFieldIndex(field[0]))
        if fld.GetType() != field[1] or fld.GetWidth() != field[2] or fld.GetPrecision() != field[3]:
            gdaltest.post_reason( field[0] + ' field definition wrong.' )
            return 'fail'

    ds.Destroy()

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.tab')

    return 'success'

###############################################################################
# Verify that field widths and precisions are propogated correctly in MIF

def ogr_mitab_14():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    import ogr_gml_read
    if ogr_gml_read.ogr_gml_1() != 'success':
        return 'skip'

    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        os.stat('tmp/testlyrdef.mif')
        ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.mif')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f "MapInfo File" -dsco FORMAT=MIF tmp/testlyrdef.mif ../ogr/data/testlyrdef.gml')

    ds = ogr.Open('tmp/testlyrdef.mif')

    #Check if the width and precision are as preserved.
    lyr = ds.GetLayer('testlyrdef')
    if lyr is None:
        gdaltest.post_reason( 'Layer missing.' )
        return 'fail'

    defn = lyr.GetLayerDefn()

    data = [['AREA',  ogr.OFTReal,   7, 4],
            ['VOLUME',ogr.OFTReal,   0, 0],
            ['LENGTH',ogr.OFTInteger,254,0],
            ['WIDTH', ogr.OFTInteger,254,0]]

    for field in data:
        fld = defn.GetFieldDefn(defn.GetFieldIndex(field[0]))
        expected_with = field[2]
        if fld.GetType() == ogr.OFTInteger:
            expected_with = 0
        if fld.GetType() != field[1] or fld.GetWidth() != expected_with or fld.GetPrecision() != field[3]:
            gdaltest.post_reason( field[0] + ' field definition wrong.' )
            return 'fail'

    ds.Destroy()

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.mif')

    return 'success'

###############################################################################
# Test .mif without .mid (#5141)

def ogr_mitab_15():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.Open('data/nomid.mif')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat is None:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test empty .mif

def ogr_mitab_16():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('tmp/empty.mif')
    lyr = ds.CreateLayer('empty')
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    ds = ogr.Open('tmp/empty.mif')
    if ds is None:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_mitab_17():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    import ogr_gml_read
    if ogr_gml_read.ogr_gml_1() != 'success':
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp')
    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/wrk.mif')
    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test EPSG:2154

def ogr_mitab_18():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/ogr_mitab_18.tab')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(2154)
    lyr = ds.CreateLayer('test', srs = sr)
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    # Test with our generated file, and with one generated by MapInfo
    for filename in [ '/vsimem/ogr_mitab_18.tab', 'data/lambert93_francais.TAB' ]:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        sr_got = lyr.GetSpatialRef()
        wkt = sr_got.ExportToWkt()
        if wkt.find('2154') < 0:
            gdaltest.post_reason('failure')
            print(sr_got)
            return 'fail'
        proj4 = sr_got.ExportToProj4()
        if proj4.find('+proj=lcc +lat_1=49 +lat_2=44 +lat_0=46.5 +lon_0=3 +x_0=700000 +y_0=6600000 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs') != 0:
            gdaltest.post_reason('failure')
            print(proj4)
            return 'fail'
        ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/ogr_mitab_18.tab')

    return 'success'
###############################################################################
#

def ogr_mitab_cleanup():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource( 'tmp' )

    return 'success'

gdaltest_list = [
    ogr_mitab_1,
    ogr_mitab_2,
    ogr_mitab_3,
    ogr_mitab_4,
    ogr_mitab_5,
    ogr_mitab_6,
    ogr_mitab_7,
    ogr_mitab_8,
    ogr_mitab_9,
    ogr_mitab_10,
    ogr_mitab_11,
    ogr_mitab_12,
    ogr_mitab_13,
    ogr_mitab_14,
    ogr_mitab_15,
    ogr_mitab_16,
    ogr_mitab_17,
    ogr_mitab_18,
    ogr_mitab_cleanup
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mitab' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

