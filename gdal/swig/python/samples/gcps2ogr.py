#!/usr/bin/env python
# -*- coding: utf-8 -*-
#******************************************************************************
#  $Id$
# 
#  Project:  GDAL
#  Purpose:  Outputs GDAL GCPs as OGR points
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
# 
#******************************************************************************
#  Copyright (c) 2015, Even Rouault, <even dot rouault at spatialys dot com>
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
#******************************************************************************

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import sys


def Usage():
    print('Usage: gcp2ogr.py [-f ogr_drv_name] gdal_in_dataset ogr_out_dataset')
    sys.exit(1)

out_format = 'ESRI Shapefile'
in_dataset = None
out_dataset = None
i = 1
while i < len(sys.argv):
    if sys.argv[i] == '-f':
        i+=1
        out_format = sys.argv[i]
    elif sys.argv[i][0] == '-':
        Usage()
    elif in_dataset is None:
        in_dataset = sys.argv[i]
    elif out_dataset is None:
        out_dataset = sys.argv[i]
    else:
        Usage()
    i+= 1

if out_dataset is None:
    Usage()

ds = gdal.Open(in_dataset)
out_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource(out_dataset)
sr = None
wkt = ds.GetGCPProjection()
if wkt != '':
    sr = osr.SpatialReference(wkt)
out_lyr = out_ds.CreateLayer('gcps', geom_type = ogr.wkbPoint, srs = sr)
out_lyr.CreateField(ogr.FieldDefn('Id', ogr.OFTString))
out_lyr.CreateField(ogr.FieldDefn('Info', ogr.OFTString))
out_lyr.CreateField(ogr.FieldDefn('X', ogr.OFTReal))
out_lyr.CreateField(ogr.FieldDefn('Y', ogr.OFTReal))
gcps = ds.GetGCPs()
for i in range(len(gcps)):
    f = ogr.Feature(out_lyr.GetLayerDefn())
    f.SetField('Id', gcps[i].Id)
    f.SetField('Info', gcps[i].Info)
    f.SetField('X', gcps[i].GCPPixel)
    f.SetField('Y', gcps[i].GCPLine)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%f %f)' % (gcps[i].GCPX, gcps[i].GCPY)))
    out_lyr.CreateFeature(f)

