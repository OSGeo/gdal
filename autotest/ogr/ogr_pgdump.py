#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PGDump driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################
# Create table from data/poly.shp

def ogr_pgdump_1():

    try:
        os.remove('tmp/tpoly.sql')
    except:
        pass

    ds = ogr.GetDriverByName('PGDump').CreateDataSource('tmp/tpoly.sql')

    ######################################################
    # Create Layer
    lyr = ds.CreateLayer( 'tpoly', options = [ 'DIM=3' ] )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_lyr = shp_ds.GetLayer(0)
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
    ds.Destroy()
    
    f = open('tmp/tpoly.sql')
    sql = f.read()
    f.close()
    
    
    if sql.find("""DROP TABLE IF EXISTS "public"."tpoly" CASCADE;""") == -1 or \
       sql.find("""DELETE FROM geometry_columns WHERE f_table_name = 'tpoly' AND f_table_schema = 'public';""") == -1 or \
       sql.find("""BEGIN;""") == -1 or \
       sql.find("""CREATE TABLE "public"."tpoly" ( OGC_FID SERIAL, CONSTRAINT "tpoly_pk" PRIMARY KEY (OGC_FID) );""") == -1 or \
       sql.find("""SELECT AddGeometryColumn('public','tpoly','wkb_geometry',-1,'GEOMETRY',3);""") == -1 or \
       sql.find("""CREATE INDEX "tpoly_wkb_geometry_geom_idx" ON "public"."tpoly" USING GIST ("wkb_geometry");""") == -1 or \
       sql.find("""ALTER TABLE "public"."tpoly" ADD COLUMN "area" FLOAT8;""") == -1 or \
       sql.find("""ALTER TABLE "public"."tpoly" ADD COLUMN "eas_id" INTEGER;""") == -1 or \
       sql.find("""ALTER TABLE "public"."tpoly" ADD COLUMN "prfedea" VARCHAR;""") == -1 or \
       sql.find("""ALTER TABLE "public"."tpoly" ADD COLUMN "shortname" VARCHAR(8);""") == -1 or \
       sql.find("""INSERT INTO "public"."tpoly" ("wkb_geometry" , "area", "eas_id", "prfedea") VALUES ('01030000800100000005000000000000C01A481D4100000080072D5241000000000000000000000060AA461D4100000080FF2C524100000000000000000000006060461D41000000400C2D52410000000000000000000000A0DF471D4100000000142D52410000000000000000000000C01A481D4100000080072D52410000000000000000', 5268.813, 170, '35043413');""") == -1 or \
       sql.find("""COMMIT;""") == -1 :
        print(sql)
        return 'fail'
        
    return 'success'

###############################################################################
# Create table from data/poly.shp with PG_USE_COPY=YES

def ogr_pgdump_2():

    try:
        os.remove('tmp/tpoly.sql')
    except:
        pass

    gdal.SetConfigOption( 'PG_USE_COPY', 'YES' )

    ds = ogr.GetDriverByName('PGDump').CreateDataSource('tmp/tpoly.sql', options = [ 'LINEFORMAT=CRLF' ] )

    ######################################################
    # Create Layer
    lyr = ds.CreateLayer( 'tpoly', geom_type = ogr.wkbPolygon, options = [ 'SCHEMA=another_schema', 'SRID=4326', 'GEOMETRY_NAME=the_geom' ] )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_lyr = shp_ds.GetLayer(0)
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
    ds.Destroy()

    gdal.SetConfigOption( 'PG_USE_COPY', 'NO' )

    f = open('tmp/tpoly.sql')
    sql = f.read()
    f.close()
    
    if sql.find("""DROP TABLE IF EXISTS "another_schema"."tpoly" CASCADE;""") == -1 or \
       sql.find("""DELETE FROM geometry_columns WHERE f_table_name = 'tpoly' AND f_table_schema = 'another_schema';""") == -1 or \
       sql.find("""BEGIN;""") == -1 or \
       sql.find("""CREATE TABLE "another_schema"."tpoly" ( OGC_FID SERIAL, CONSTRAINT "tpoly_pk" PRIMARY KEY (OGC_FID) );""") == -1 or \
       sql.find("""SELECT AddGeometryColumn('another_schema','tpoly','the_geom',4326,'POLYGON',2);""") == -1 or \
       sql.find("""CREATE INDEX "tpoly_the_geom_geom_idx" ON "another_schema"."tpoly" USING GIST ("the_geom");""") == -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "area" FLOAT8;""") == -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "eas_id" INTEGER;""") == -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "prfedea" VARCHAR;""") == -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "shortname" VARCHAR(8);""") == -1 or \
       sql.find("""COPY "another_schema"."tpoly" ("the_geom", "area", "eas_id", "prfedea", "shortname") FROM STDIN;""") == -1 or \
       sql.find("0103000020E61000000100000005000000000000C01A481D4100000080072D524100000060AA461D4100000080FF2C52410000006060461D41000000400C2D5241000000A0DF471D4100000000142D5241000000C01A481D4100000080072D5241	5268.813	170	35043413	\\N") == -1 or \
       sql.find("""\.""") == -1 or \
       sql.find("""COMMIT;""") == -1 :
        print(sql)
        return 'fail'
        
    return 'success'


###############################################################################
# Create table from data/poly.shp without any geometry

def ogr_pgdump_3():

    try:
        os.remove('tmp/tpoly.sql')
    except:
        pass

    gdal.SetConfigOption( 'PG_USE_COPY', 'YES' )

    ds = ogr.GetDriverByName('PGDump').CreateDataSource('tmp/tpoly.sql', options = [ 'LINEFORMAT=LF' ] )

    ######################################################
    # Create Layer
    lyr = ds.CreateLayer( 'tpoly', geom_type = ogr.wkbNone, options = [ 'SCHEMA=another_schema' ] )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( lyr,
                                    [ ('EMPTYCHAR', ogr.OFTString),
                                      ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_lyr = shp_ds.GetLayer(0)
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    i = 0
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        if i == 0:
            # Be perverse and test the case where a feature has a geometry
            # even if it's a wkbNone layer ! (#4040)
            dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
        elif i == 1:
            # Field with 0 character (not empty!) (#4040)
            dst_feat.SetField(0, '')
        i = i + 1
        lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
    ds.Destroy()

    gdal.SetConfigOption( 'PG_USE_COPY', 'NO' )

    f = open('tmp/tpoly.sql')
    sql = f.read()
    f.close()
    
    if sql.find("""DROP TABLE IF EXISTS "another_schema"."tpoly" CASCADE;""") == -1 or \
       sql.find("""DELETE FROM geometry_columns""") != -1 or \
       sql.find("""BEGIN;""") == -1 or \
       sql.find("""CREATE TABLE "another_schema"."tpoly" (    OGC_FID SERIAL,    CONSTRAINT "tpoly_pk" PRIMARY KEY (OGC_FID) );""") == -1 or \
       sql.find("""SELECT AddGeometryColumn""") != -1 or \
       sql.find("""CREATE INDEX "tpoly_wkb_geometry_geom_idx""") != -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "area" FLOAT8;""") == -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "eas_id" INTEGER;""") == -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "prfedea" VARCHAR;""") == -1 or \
       sql.find("""ALTER TABLE "another_schema"."tpoly" ADD COLUMN "shortname" VARCHAR(8);""") == -1 or \
       sql.find("""COPY "another_schema"."tpoly" ("emptychar", "area", "eas_id", "prfedea", "shortname") FROM STDIN;""") == -1 or \
       sql.find("""\\N	215229.266	168	35043411	\\N""") == -1 or \
       sql.find("""	5268.813	170	35043413	\\N""") == -1 or \
       sql.find("""\\.""") == -1 or \
       sql.find("""COMMIT;""") == -1 :
        print(sql)
        return 'fail'
        
    return 'success'

###############################################################################
# Test multi-geometry support

def ogr_pgdump_4():

    ds = ogr.GetDriverByName('PGDump').CreateDataSource('tmp/ogr_pgdump_4.sql', options = [ 'LINEFORMAT=LF' ] )
    if ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
        
    ######################################################
    # Create Layer
    lyr = ds.CreateLayer( 'test', geom_type = ogr.wkbNone, options = [ 'WRITE_EWKT_GEOM=YES' ] )
    if lyr.TestCapability(ogr.OLCCreateGeomField) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gfld_defn = ogr.GeomFieldDefn("point_nosrs", ogr.wkbPoint)
    lyr.CreateGeomField(gfld_defn)

    gfld_defn = ogr.GeomFieldDefn("poly", ogr.wkbPolygon25D)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(srs)
    lyr.CreateGeomField(gfld_defn)
    
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomFieldDirectly("point_nosrs", ogr.CreateGeometryFromWkt('POINT (1 2)'))
    feat.SetGeomFieldDirectly("poly", ogr.CreateGeometryFromWkt('POLYGON Z ((0 0 0,0 1 0,1 1 0,1 0 0, 0 0 0))'))
    lyr.CreateFeature(feat)
    
    ds = None

    f = open('tmp/ogr_pgdump_4.sql')
    sql = f.read()
    f.close()
    
    if sql.find("""CREATE TABLE "public"."test" (    OGC_FID SERIAL,    CONSTRAINT "test_pk" PRIMARY KEY (OGC_FID) )""") == -1 or \
       sql.find("""SELECT AddGeometryColumn('public','test','point_nosrs',-1,'POINT',2)""") == -1 or \
       sql.find("""CREATE INDEX "test_point_nosrs_geom_idx" ON "public"."test" USING GIST ("point_nosrs")""") == -1 or \
       sql.find("""SELECT AddGeometryColumn('public','test','poly',4326,'POLYGON',3)""") == -1 or \
       sql.find("""CREATE INDEX "test_poly_geom_idx" ON "public"."test" USING GIST ("poly")""") == -1 or \
       sql.find("""INSERT INTO "public"."test" DEFAULT VALUES""") == -1 or \
       sql.find("""INSERT INTO "public"."test" ("point_nosrs" , "poly" ) VALUES (GeomFromEWKT('SRID=-1;POINT (1 2)'::TEXT) , GeomFromEWKT('SRID=4326;POLYGON ((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0))'::TEXT) )""") == -1 :
        print(sql)
        return 'fail'
        
    return 'success'
    
###############################################################################
# Cleanup

def ogr_pgdump_cleanup():

    try:
        os.remove('tmp/tpoly.sql')
    except:
        pass
    try:
        os.remove('tmp/ogr_pgdump_4.sql')
    except:
        pass
    return 'success'

gdaltest_list = [ 
    ogr_pgdump_1,
    ogr_pgdump_2,
    ogr_pgdump_3,
    ogr_pgdump_4,
    ogr_pgdump_cleanup ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pgdump' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

