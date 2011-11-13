#!/usr/bin/env python
#******************************************************************************
#  $Id$
# 
#  Name:     gdalcopyproj.py
#  Project:  GDAL Python Interface
#  Purpose:  Duplicate the geotransform and projection metadata from
#	     one raster dataset to another, which can be useful after
#	     performing image manipulations with other software that
#	     ignores or discards georeferencing metadata.
#  Author:   Schuyler Erle, schuyler@nocat.net
# 
#******************************************************************************
#  Copyright (c) 2005, Frank Warmerdam
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

try:
    from osgeo import gdal
except ImportError:
    import gdal

import sys
import os.path

if len(sys.argv) < 3:
    print("Usage: gdalcopyproj.py source_file dest_file")
    sys.exit(1)

input = sys.argv[1]
dataset = gdal.Open( input )
if dataset is None:
    print('Unable to open', input, 'for reading')
    sys.exit(1)

projection   = dataset.GetProjection()
geotransform = dataset.GetGeoTransform()

if projection is None and geotransform is None:
    print('No projection or geotransform found on file' + input)
    sys.exit(1)

output = sys.argv[2]
dataset2 = gdal.Open( output, gdal.GA_Update )

if dataset2 is None:
    print('Unable to open', output, 'for writing')
    sys.exit(1)

if geotransform is not None and geotransform != (0,1,0,0,0,1):
    dataset2.SetGeoTransform( geotransform )

if projection is not None and projection != '':
    dataset2.SetProjection( projection )

gcp_count = dataset.GetGCPs()
if gcp_count != 0:
    dataset2.SetGCPs( gcp_count, dataset.GetGCPProjection() )

dataset = None
dataset2 = None
