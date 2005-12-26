#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Load an OGR layer into a MySQL datastore.  Uses direct SQL 
#           since the MySQL driver is read-only for OGR.  Derived from 
#           load2odbc.py
# Author:   Howard Butler, <hobu@hobu.net>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2005, Howard Butler <hobu@hobu.net>
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
# 
#  $Log$
#  Revision 1.1  2005/12/26 23:54:07  hobu
#  New tool based on load2odbc.py to load an OGR layer into a MySQL table and store it as MySQL geometry
#



import sys

sys.path.insert(0,'/Users/hobu/cvs/gdal/swig/python/build/lib.darwin-8.2.0-Power_Macintosh-2.3')


import ogr
import osr

#############################################################################
def Usage():
    print 'Usage: load2mysql.py infile mysql_dsn layer'
    print
    print '     Example: load2mysql.py world_borders.shp MySQL:test,user=root world_borders'
    sys.exit(1)

#############################################################################
# Argument processing.

extents_flag = 0
infile = None
outfile = None

if len(sys.argv) != 4:
    Usage()

infile = sys.argv[1]
mysql_dsn = sys.argv[2]
layername = sys.argv[3]

#############################################################################
# Open the datasource to operate on.

in_ds = ogr.Open( infile, update = 0 )

in_layer = in_ds.GetLayerByName( layername )

if in_layer is None:
    print 'Did not find layer: ', layername
    sys.exit( 1 )

out_ds = ogr.Open( mysql_dsn )

if out_ds is None:
    print 'Unable to connect to ' + mysql_dsn 
    sys.exit(1)

#############################################################################
#	Attempt to drop the table from the datasource if it already exists.

try:
    out_ds.ExecuteSQL( 'drop table ' + layername )
except:
    pass

#############################################################################
#	Fetch layer definition, and defined output table on the same basis.
#   MySQL requires that the geometry column be not null to have spatial 
#   indexes on it.  Make the OGC_FID unique just for the hell of it.

defn = in_layer.GetLayerDefn()

cmd = 'CREATE TABLE ' + layername + '( OGC_FID INT UNIQUE NOT NULL, WKT_GEOMETRY GEOMETRY NOT NULL' 

for iField in range(defn.GetFieldCount()):
    fielddef = defn.GetFieldDefn(iField)
    width = fielddef.GetWidth() 
    precision = fielddef.GetPrecision()
    type = fielddef.GetType()
    cmd = cmd + ', ' + fielddef.GetName()
    if type == ogr.OFTInteger:
        cmd = cmd + ' INT' 
    if type == ogr.OFTString:
        cmd = cmd + ' VARCHAR(%s)' % width
    if type == ogr.OFTReal:
        cmd = cmd + ' NUMERIC(%s,%s)' % (width, precision)

    
cmd = cmd + ')'

print 'ExecuteSQL: ', cmd
result = out_ds.ExecuteSQL( cmd )
if result is not None:
    out_ds.ReleaseResultSet( result )


in_layer.ResetReading()
feat = in_layer.GetNextFeature()

counter = 0
while feat is not None:
    cmd_start = 'INSERT INTO ' + layername + ' ( OGC_FID '
    cmd_end = ') VALUES (%d' % feat.GetFID()

    geom = feat.GetGeometryRef()
    geom_statement = ''
    if geom:
        cmd_start = cmd_start + ', WKT_GEOMETRY '
        
        geom_statement = ", GeomFromText('%s',4326)" % geom.ExportToWkt()
        
    cmd_end = cmd_end +  geom_statement

    for iField in range(defn.GetFieldCount()):
        fielddef = defn.GetFieldDefn(iField)
        if feat.IsFieldSet( iField ) != 0:
            cmd_start = cmd_start + ', ' + fielddef.GetName()

        if fielddef.GetType() == ogr.OFTInteger:
            cmd_end = cmd_end + ', ' + feat.GetFieldAsString(iField)
        elif fielddef.GetType() == ogr.OFTString:
            cmd_end = cmd_end + ", '" + feat.GetFieldAsString(iField) + "'"
        elif fielddef.GetType() == ogr.OFTReal:
            cmd_end = cmd_end + ', ' + feat.GetFieldAsString(iField)
        else:
            cmd_end = cmd_end + ", '" + feat.GetFieldAsString(iField) + "'"

    cmd = cmd_start + cmd_end + ')'

    #print 'ExecuteSQL: ', cmd
    out_ds.ExecuteSQL( cmd )
    if counter % 1000 == 0:
        sys.stdout.write('.')
        sys.stdout.flush()
    feat.Destroy()
    feat = in_layer.GetNextFeature()
    counter += 1

cmd = 'ALTER TABLE %s ADD SPATIAL INDEX(WKT_GEOMETRY)' % layername
print cmd
result = out_ds.ExecuteSQL( cmd )
print result
#############################################################################
# Cleanup

in_ds.Destroy()
out_ds.Destroy()
