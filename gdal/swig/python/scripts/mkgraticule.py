#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Produce a graticule (grid) dataset.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
    from osgeo import osr
    from osgeo import ogr
except ImportError:
    import osr
    import ogr

import sys

#############################################################################
def float_range(*args):
    start = 0.0
    step = 1.0
    if (len(args) == 1):
        (stop,) = args
    elif (len(args) == 2):
        (start, stop) = args
    elif (len(args) == 3):
        (start, stop, step) = args
    else:
        raise TypeError("float_range needs 1-3 float arguments")

    the_range = []
    steps = (stop-start)/step
    if steps != int(steps):
        steps = steps + 1.0
    for i in range(int(steps)):
        the_range.append(i*step+start)

    return the_range


#############################################################################
def Usage():
    print ('Usage: mkgraticule [-connected] [-s stepsize] [-substep substepsize]')
    print ('         [-t_srs srs] [-range xmin ymin xmax ymax] outfile')
    print('')
    sys.exit(1)

#############################################################################
# Argument processing.

t_srs = None
stepsize = 5.0
substepsize = 5.0
connected = 0
outfile = None

xmin = -180
xmax = 180
ymin = -90
ymax = 90

i = 1
while i < len(sys.argv):
    if sys.argv[i] == '-connected':
        connected = 1
    elif sys.argv[i] == '-t_srs':
        i = i + 1
        t_srs = sys.argv[i]
    elif sys.argv[i] == '-s':
        i = i + 1
        stepsize = float(sys.argv[i])
    elif sys.argv[i] == '-substep':
        i = i + 1
        substepsize = float(sys.argv[i])
    elif sys.argv[i] == '-range':
        xmin = float(sys.argv[i+1])
        ymin = float(sys.argv[i+2])
        xmax = float(sys.argv[i+3])
        ymax = float(sys.argv[i+4])
        i = i + 4
    elif sys.argv[i][0] == '-':
        Usage()
    elif outfile is None:
        outfile = sys.argv[i]
    else:
        Usage()

    i = i + 1
    
if outfile is None:
    outfile = "graticule.shp"


if substepsize > stepsize:
    substepsize = stepsize
    
#############################################################################-
# Do we have an alternate SRS?

ct = None

if t_srs is not None:
    t_srs_o = osr.SpatialReference()
    t_srs_o.SetFromUserInput( t_srs )

    s_srs_o = osr.SpatialReference()
    s_srs_o.SetFromUserInput( 'WGS84' )

    ct = osr.CoordinateTransformation( s_srs_o, t_srs_o )
else:
    t_srs_o = osr.SpatialReference()
    t_srs_o.SetFromUserInput( 'WGS84' )
    
#############################################################################-
# Create graticule file.

drv = ogr.GetDriverByName( 'ESRI Shapefile' )

try:
    drv.DeleteDataSource( outfile )
except:
    pass

ds = drv.CreateDataSource( outfile )
layer = ds.CreateLayer( 'out', geom_type = ogr.wkbLineString,
                        srs = t_srs_o )

#########################################################################
# Not connected case.  Produce individual segments are these are going to
# be more resilent in the face of reprojection errors.

if not connected:
    #########################################################################
    # Generate lines of latitude.

    feat = ogr.Feature( feature_def = layer.GetLayerDefn() )
    geom = ogr.Geometry( type = ogr.wkbLineString )

    for lat in float_range(ymin,ymax+stepsize/2,stepsize):
        for long_ in float_range(xmin,xmax-substepsize/2,substepsize):

            geom.SetPoint( 0, long_, lat )
            geom.SetPoint( 1, long_+substepsize, lat )

            err = 0
            if ct is not None:
                err = geom.Transform( ct )
              
            if err is 0:
                feat.SetGeometry( geom )
                layer.CreateFeature( feat )

    #########################################################################
    # Generate lines of longitude

    for long_ in float_range(xmin,xmax+stepsize/2,stepsize):
        for lat in float_range(ymin,ymax-substepsize/2,substepsize):
            geom.SetPoint( 0, long_, lat )
            geom.SetPoint( 1, long_, lat+substepsize )

            err = 0
            if ct is not None:
                err = geom.Transform( ct )

            if err is 0:
                feat.SetGeometry( geom )
                layer.CreateFeature( feat )

                
#########################################################################
# Connected case - produce one polyline for each complete line of latitude
# or longitude.

if connected:
    #########################################################################
    # Generate lines of latitude.

    feat = ogr.Feature( feature_def = layer.GetLayerDefn() )

    for lat in float_range(ymin,ymax+stepsize/2,stepsize):

        geom = ogr.Geometry( type = ogr.wkbLineString )
        
        for long_ in float_range(xmin,xmax+substepsize/2,substepsize):
            geom.AddPoint( long_, lat )

        err = 0
        if ct is not None:
            err = geom.Transform( ct )
              
        if err is 0:
            feat.SetGeometry( geom )
            layer.CreateFeature( feat )

    #########################################################################
    # Generate lines of longitude

    for long_ in float_range(xmin,xmax+stepsize/2,stepsize):
        
        geom = ogr.Geometry( type = ogr.wkbLineString )
        
        for lat in float_range(ymin,ymax+substepsize/2,substepsize):
            geom.AddPoint( long_, lat )

        err = 0
        if ct is not None:
            err = geom.Transform( ct )

        if err is 0:
            feat.SetGeometry( geom )
            layer.CreateFeature( feat )
                
#############################################################################
# Cleanup

feat = None
geom = None

ds.Destroy()
ds = None

