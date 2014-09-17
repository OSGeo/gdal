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
# Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import random
import sys
import string
import shutil

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
# (https://github.com/mapgears/mitab/issues/1)

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
            print(filename)
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
# Check that we correctly round coordinate to the appropriate precision
# (https://github.com/mapgears/mitab/issues/2)

def ogr_mitab_19():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.Open('data/utm31.TAB')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    # Strict text comparison to check precision
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (485248.12 2261.45)':
        feat.DumpReadable()
        return 'fail'

    return 'success'


###############################################################################
# Check that we take into account the user defined bound file
# (https://github.com/mapgears/mitab/issues/3)
# Also test BOUNDS layer creation option (http://trac.osgeo.org/gdal/ticket/5642)

def ogr_mitab_20():

    if gdaltest.mapinfo_drv is None:
        return 'skip'

    # Pass i==0: without MITAB_BOUNDS_FILE
    # Pass i==1: with MITAB_BOUNDS_FILE : first load
    # Pass i==2: with MITAB_BOUNDS_FILE : should use already loaded file
    # Pass i==3: without MITAB_BOUNDS_FILE : should unload the file
    # Pass i==4: use BOUNDS layer creation option
    for i in range(5):
        if i == 1 or i == 2:
            gdal.SetConfigOption('MITAB_BOUNDS_FILE', 'data/mitab_bounds.txt')
        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/ogr_mitab_20.tab')
        sr = osr.SpatialReference()
        sr.ImportFromEPSG(2154)
        if i == 4:
            lyr = ds.CreateLayer('test', srs = sr, options = ['BOUNDS=75000,6000000,1275000,7200000'])
        else:
            lyr = ds.CreateLayer('test', srs = sr)
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (700000.001 6600000.001)"))
        lyr.CreateFeature(feat)
        ds = None
        gdal.SetConfigOption('MITAB_BOUNDS_FILE', None)
        
        ds = ogr.Open('/vsimem/ogr_mitab_20.tab')
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        # Strict text comparison to check precision
        if i == 1 or i == 2 or i == 4:
            if feat.GetGeometryRef().ExportToWkt() != 'POINT (700000.001 6600000.001)':
                print(i)
                feat.DumpReadable()
                return 'fail'
        else:
            if feat.GetGeometryRef().ExportToWkt() == 'POINT (700000.001 6600000.001)':
                print(i)
                feat.DumpReadable()
                return 'fail'

        ds = None

        ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/ogr_mitab_20.tab')

    return 'success'

###############################################################################
# Create .tab without explicit field

def ogr_mitab_21():

    if gdaltest.mapinfo_drv is None:
        return 'skip'
        
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/ogr_mitab_21.tab')
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    ds = None
    
    ds = ogr.Open('/vsimem/ogr_mitab_21.tab')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('FID') != 1:
        feat.DumpReadable()
        return 'fail'
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/ogr_mitab_21.tab')

    return 'success'

###############################################################################
# Test append in update mode

def ogr_mitab_22():
    
    filename = '/vsimem/ogr_mitab_22.tab'
    for nb_features in (2, 1000):
        if nb_features == 2:
            nb_runs = 2
        else:
            nb_runs = 1

        # When doing 2 runs, in the second one, we create an empty
        # .tab and then open it for update. This can trigger specific bugs
        for j in range(nb_runs):
            ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
            lyr = ds.CreateLayer('test')
            lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
            if j == 0:
                i = 0
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetField('ID', i+1)
                feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
                if lyr.CreateFeature(feat) != 0:
                    print(i)
                    print(nb_features)
                    gdaltest.post_reason('fail')
                    return 'fail'
            ds = None

            for i in range(nb_features - (1-j)):
                ds = ogr.Open(filename, update = 1)
                lyr = ds.GetLayer(0)
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetField('ID', i+1+(1-j))
                feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i+(1-j), i+(1-j))))
                if lyr.CreateFeature(feat) != 0:
                    print(i)
                    print(nb_features)
                    gdaltest.post_reason('fail')
                    return 'fail'
                ds = None

            ds = ogr.Open(filename)
            lyr = ds.GetLayer(0)
            for i in range(nb_features):
                f = lyr.GetNextFeature()
                if f is None or f.GetField('ID') != i+1:
                    print(nb_features)
                    print(i)
                    gdaltest.post_reason('fail')
                    return 'fail'
            ds = None

    return 'success'

###############################################################################
# Test creating features then reading

def ogr_mitab_23():

    filename = '/vsimem/ogr_mitab_23.tab'

    for nb_features in (0, 1, 2, 100, 1000):
        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
        lyr = ds.CreateLayer('test')
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        for i in range(nb_features):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', i+1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
            lyr.CreateFeature(feat)

        lyr.ResetReading()
        for i in range(nb_features):
            f = lyr.GetNextFeature()
            if f is None or f.GetField('ID') != i+1:
                print(nb_features)
                print(i)
                gdaltest.post_reason('fail')
                return 'fail'
        f = lyr.GetNextFeature()
        if f is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None
        
        ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


    return 'success'

###############################################################################
# Test creating features then reading then creating again then reading

def ogr_mitab_24():

    filename = '/vsimem/ogr_mitab_24.tab'

    for nb_features in (2, 100, 1000):
        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
        lyr = ds.CreateLayer('test')
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        for i in range(nb_features/2):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', i+1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
            lyr.CreateFeature(feat)

        lyr.ResetReading()
        for i in range(nb_features/2):
            f = lyr.GetNextFeature()
            if f is None or f.GetField('ID') != i+1:
                print(nb_features)
                print(i)
                gdaltest.post_reason('fail')
                return 'fail'
        f = lyr.GetNextFeature()
        if f is not None:
            gdaltest.post_reason('fail')
            return 'fail'

        for i in range(nb_features/2):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', nb_features / 2 + i+1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
            lyr.CreateFeature(feat)
            
        lyr.ResetReading()
        for i in range(nb_features):
            f = lyr.GetNextFeature()
            if f is None or f.GetField('ID') != i+1:
                print(nb_features)
                print(i)
                gdaltest.post_reason('fail')
                return 'fail'
        f = lyr.GetNextFeature()
        if f is not None:
            gdaltest.post_reason('fail')
            return 'fail'

        ds = None
        
        ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


    return 'success'

###############################################################################
# Test that opening in update mode without doing any change does not alter
# file

def ogr_mitab_25():

    filename = 'tmp/ogr_mitab_25.tab'

    for nb_features in (2, 1000):
        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
        lyr = ds.CreateLayer('test')
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        for i in range(nb_features/2):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', i+1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
            lyr.CreateFeature(feat)
        ds = None

        mtime_dict = {}
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime_dict[ext] = os.stat(filename[0:-3] + ext).st_mtime

        import time
        time.sleep(1)

        # Try without doing anything
        ds = ogr.Open(filename, update = 1)
        ds = None
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime = os.stat(filename[0:-3] + ext).st_mtime
            if mtime_dict[ext] != mtime:
                print('mtime of .%s has changed !' % ext)
                gdaltest.post_reason('fail')
                return 'fail'

        # Try by reading all features
        ds = ogr.Open(filename, update = 1)
        lyr = ds.GetLayer(0)
        lyr.GetFeatureCount(1)
        ds = None
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime = os.stat(filename[0:-3] + ext).st_mtime
            if mtime_dict[ext] != mtime:
                print('mtime of .%s has changed !' % ext)
                gdaltest.post_reason('fail')
                return 'fail'

        # Try by reading all features with a spatial index
        ds = ogr.Open(filename, update = 1)
        lyr = ds.GetLayer(0)
        lyr.SetSpatialFilterRect(0.5, 0.5, 1.5, 1.5)
        lyr.GetFeatureCount(1)
        ds = None
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime = os.stat(filename[0:-3] + ext).st_mtime
            if mtime_dict[ext] != mtime:
                print('mtime of .%s has changed !' % ext)
                gdaltest.post_reason('fail')
                return 'fail'
                
        import test_cli_utilities
        if test_cli_utilities.get_test_ogrsf_path() is not None:
            ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro -fsf ' + filename)
            if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
                print(ret)
                return 'fail'

        ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

    return 'success'

###############################################################################
# Test DeleteFeature()

def ogr_mitab_26():

    filename = '/vsimem/ogr_mitab_26.tab'

    for nb_features in (2, 1000):
        if nb_features == 2:
            nb_runs = 2
        else:
            nb_runs = 1
        for j in range(nb_runs):
            ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
            lyr = ds.CreateLayer('test')
            lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
            for i in range(nb_features):
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetField('ID', i+1)
                feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
                lyr.CreateFeature(feat)

            if nb_features == 2:
                if lyr.DeleteFeature(nb_features/2) != 0:
                    print(j)
                    print(nb_features)
                    gdaltest.post_reason('fail')
                    return 'fail'
            else:
                for k in range(nb_features/2):
                    if lyr.DeleteFeature(nb_features/4 + k) != 0:
                        print(j)
                        print(k)
                        print(nb_features)
                        gdaltest.post_reason('fail')
                        return 'fail'

            if j == 1:
                # Expected failure : already deleted feature
                gdal.ErrorReset()
                gdal.PushErrorHandler('CPLQuietErrorHandler')
                ret = lyr.DeleteFeature(nb_features/2)
                gdal.PopErrorHandler()
                if ret == 0 or gdal.GetLastErrorMsg() == '':
                    print(j)
                    print(nb_features)
                    gdaltest.post_reason('fail')
                    return 'fail'

                feat = lyr.GetFeature(nb_features/2)
                if feat is not None:
                    print(j)
                    print(nb_features)
                    gdaltest.post_reason('fail')
                    return 'fail'

                # Expected failure : illegal feature id
                gdal.PushErrorHandler('CPLQuietErrorHandler')
                ret = lyr.DeleteFeature(nb_features+1)
                gdal.PopErrorHandler()
                if ret == 0 or gdal.GetLastErrorMsg() == '':
                    print(j)
                    print(nb_features)
                    gdaltest.post_reason('fail')
                    return 'fail'

            ds = None

            ds = ogr.Open(filename)
            lyr = ds.GetLayer(0)
            if lyr.GetFeatureCount() != nb_features / 2:
                print(nb_features)
                gdaltest.post_reason('fail')
                return 'fail'
            ds = None

            # This used to trigger a bug in DAT record deletion during implementation...
            if nb_features == 1000:
                ds = ogr.Open(filename, update = 1)
                lyr = ds.GetLayer(0)
                lyr.DeleteFeature(245)
                ds = None

                ds = ogr.Open(filename)
                lyr = ds.GetLayer(0)
                if lyr.GetFeatureCount() != nb_features / 2 - 1:
                    print(nb_features)
                    gdaltest.post_reason('fail')
                    return 'fail'
                ds = None

            ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

    return 'success'

###############################################################################
# Test SetFeature()

def ogr_mitab_27():

    filename = '/vsimem/ogr_mitab_27.tab'
    
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('stringfield', ogr.OFTString))
    
    # Invalid call : feature without FID
    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid call : feature with FID <= 0
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('intfield', 1)
    f.SetField('realfield', 2.34)
    f.SetField('stringfield', "foo")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)
    fid = f.GetFID()

    # Invalid call : feature with FID > feature_count
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Update previously created object with blank feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(fid)
    lyr.SetFeature(f)

    ds = None
    
    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('intfield') != 0 or f.GetField('realfield') != 0 or f.GetField('stringfield') != '' or \
       f.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f.SetField('intfield', 1)
    f.SetField('realfield', 2.34)
    f.SetField('stringfield', "foo")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (2 3)'))
    lyr.SetFeature(f)
    ds = None
    
    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('intfield') != 1 or f.GetField('realfield') != 2.34 or f.GetField('stringfield') != 'foo' or \
       f.GetGeometryRef() is None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.DeleteFeature(f.GetFID())
    ds = None
    
    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)
    # SetFeature() on a deleted feature
    lyr.SetFeature(f)

    f = lyr.GetFeature(1)
    if f.GetField('intfield') != 1 or f.GetField('realfield') != 2.34 or f.GetField('stringfield') != 'foo' or \
       f.GetGeometryRef() is None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(1)
    # SetFeature() with identical feature : no-op
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    stat = gdal.VSIStatL(filename[0:-3]+"map")
    old_size = stat.size

    # This used to trigger a bug: when using SetFeature() repeatly, we
    # can create object blocks in the .map that are made only of deleted
    # objects.
    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(1)
    for i in range(100):
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (2 3)'))
        if lyr.SetFeature(f) != 0:
            gdaltest.post_reason('fail')
            return 'fail'
    ds = None

    stat = gdal.VSIStatL(filename[0:-3]+"map")
    if stat.size != old_size:
        gdaltest.post_reason('fail')
        return 'fail'
    
    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(1)
    # SetFeature() with identical geometry : rewrite only attributes
    f.SetField('intfield', -1)
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetFeature(1)
    if f.GetField('intfield') != -1 or f.GetField('realfield') != 2.34 or f.GetField('stringfield') != 'foo' or \
       f.GetGeometryRef() is None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

    return 'success'

###############################################################################
def generate_permutation(n):
    tab = [ i for i in range(n) ]
    for i in range(10*n):
        ind = random.randint(0,n-1)
        tmp = tab[0]
        tab[0] = tab[ind]
        tab[ind] = tmp
    return tab

###############################################################################
# Test updating object blocks with deleted objects

def ogr_mitab_28():

    filename = '/vsimem/ogr_mitab_28.tab'

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)
    # Generate 10x10 grid
    N2 = 10
    N = N2*N2
    for n in generate_permutation(N):
        x = int(n / N2)
        y = n % N2
        f = ogr.Feature(lyr.GetLayerDefn())
        #f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%d %d)' % (x,y)))
        f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(%d %d,%f %f,%f %f)' % (x,y,x+0.1,y,x+0.2,y)))
        lyr.CreateFeature(f)

    # Delete all features
    for i in range(N):
        lyr.DeleteFeature(i+1)

    # Set deleted features
    i = 0
    permutation = generate_permutation(N)
    for n in permutation:
        x = int(n / N2)
        y = n % N2
        f = ogr.Feature(lyr.GetLayerDefn())
        #f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%d %d)' % (x,y)))
        f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(%d %d,%f %f,%f %f)' % (x,y,x+0.1,y,x+0.2,y)))
        f.SetFID(i+1)
        i = i + 1
        lyr.SetFeature(f)
        
    ds = None
    
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    i = 0
    # Check sequential enumeration
    for f in lyr:
        g = f.GetGeometryRef()
        (x, y, z) = g.GetPoint(0)
        n = permutation[i]
        x_ref = int(n / N2)
        y_ref = n % N2
        if abs(x - x_ref) + abs(y - y_ref) > 0.1:
            gdaltest.post_reason('fail')
            return 'fail'
        i = i + 1

    # Check spatial index integrity
    for n in range(N):
        x = int(n / N2)
        y = n % N2
        lyr.SetSpatialFilterRect(x-0.5,y-0.5,x+0.5,y+0.5)
        if lyr.GetFeatureCount() != 1:
            gdaltest.post_reason('fail')
            return 'fail'

    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

    return 'success'


###############################################################################
# Test updating a file with compressed geometries.

def ogr_mitab_29():
    try:
        os.stat('tmp/cache/compr_symb_deleted_records.tab')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'data/compr_symb_deleted_records.zip')
            try:
                os.stat('tmp/cache/compr_symb_deleted_records.tab')
            except:
                return 'skip'
        except:
            return 'skip'

    shutil.copy('tmp/cache/compr_symb_deleted_records.tab', 'tmp')
    shutil.copy('tmp/cache/compr_symb_deleted_records.dat', 'tmp')
    shutil.copy('tmp/cache/compr_symb_deleted_records.id', 'tmp')
    shutil.copy('tmp/cache/compr_symb_deleted_records.map', 'tmp')

    # Is a 100x100 point grid with only the 4 edge lines left (compressed points)
    ds = ogr.Open('tmp/compr_symb_deleted_records.tab', update = 1)
    lyr = ds.GetLayer(0)
    # Re-add the 98x98 interior points
    i = 0
    N2 = 98
    N = N2 * N2
    permutation = generate_permutation(N)
    for n in permutation:
        x = 1 + int(n / N2)
        y = 1 + n % N2
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%d %d)' % (x,y)))
        lyr.CreateFeature(f)
    ds = None

    # Check grid integrity that after reopening
    ds = ogr.Open('tmp/compr_symb_deleted_records.tab')
    lyr = ds.GetLayer(0)
    N2 = 100
    N = N2 * N2
    for n in range(N):
        x = int(n / N2)
        y = n % N2
        lyr.SetSpatialFilterRect(x-0.01,y-0.01,x+0.01,y+0.01)
        if lyr.GetFeatureCount() != 1:
            gdaltest.post_reason('fail')
            return 'fail'
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/compr_symb_deleted_records.tab')

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
    ogr_mitab_19,
    ogr_mitab_20,
    ogr_mitab_21,
    ogr_mitab_22,
    ogr_mitab_23,
    ogr_mitab_24,
    ogr_mitab_25,
    ogr_mitab_26,
    ogr_mitab_27,
    ogr_mitab_28,
    ogr_mitab_29,
    ogr_mitab_cleanup
    ]
#gdaltest_list = [ ogr_mitab_1, ogr_mitab_27, ogr_mitab_28, ogr_mitab_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mitab' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

