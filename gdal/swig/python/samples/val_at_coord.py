#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL Python samples
# Purpose:  Outputs the value of the raster bands at a given
#           (longitude, latitude) or (X, Y) location.
# Author:   Even Rouault
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

try:
    from osgeo import gdal
    from osgeo import osr
except ImportError:
    import gdal

import sys

# =============================================================================
def Usage():
    print('Usage: val_at_coord.py [-display_xy] [longitude latitude | -coordtype=georef X Y] filename')
    print('')
    print('By default, the 2 first arguments are supposed to be the location')
    print('in longitude, latitude order. If -coordtype=georef is specified before')
    print('the next 2 values will be interpretated as the X and Y coordinates')
    print('in the dataset spatial reference system.')
    sys.exit( 1 )

# =============================================================================

display_xy = False
coordtype_georef = False
longitude = None
latitude = None
filename = None

# =============================================================================
# Parse command line arguments.
# =============================================================================
i = 1
while i < len(sys.argv):
    arg = sys.argv[i]

    if arg == '-coordtype=georef':
        coordtype_georef = True

    elif arg == '-display_xy':
        display_xy = True

    elif longitude is None:
        longitude = float(arg)

    elif latitude is None:
        latitude = float(arg)

    elif filename is None:
        filename = arg

    else:
        Usage()

    i = i + 1

if longitude is None:
    Usage()
if latitude is None:
    Usage()
if filename is None:
    filename()

# Open input dataset
ds = gdal.Open( filename, gdal.GA_ReadOnly )
if ds is None:
    print('Cannot open %s' % filename)
    sys.exit(1)

# Build Spatial Reference object based on coordinate system, fetched from the
# opened dataset
if coordtype_georef:
    X = longitude
    Y = latitude
else:
    srs = osr.SpatialReference()
    srs.ImportFromWkt(ds.GetProjection())

    srsLatLong = srs.CloneGeogCS()
    # Convert from (longitude,latitude) to projected coordinates
    ct = osr.CoordinateTransformation(srsLatLong, srs)
    (X, Y, height) = ct.TransformPoint(longitude, latitude)

# Read geotransform matrix and calculate corresponding pixel coordinates
geomatrix = ds.GetGeoTransform()
(success, inv_geometrix) = gdal.InvGeoTransform(geomatrix)
x = int(inv_geometrix[0] + inv_geometrix[1] * X + inv_geometrix[2] * Y)
y = int(inv_geometrix[3] + inv_geometrix[4] * X + inv_geometrix[5] * Y)

if display_xy:
    print('x=%d, y=%d' % (x, y))

if x < 0 or x >= ds.RasterXSize or y < 0 or y >= ds.RasterYSize:
    print('Passed coordinates are not in dataset extent')
    sys.exit(1)

res = ds.ReadAsArray(x,y,1,1)
if len(res.shape) == 2:
    print(res[0][0])
else:
    for val in res:
        print(val[0][0])
