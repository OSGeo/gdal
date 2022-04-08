#!/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Apply a transformation to all OGR geometries.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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

import sys

from osgeo import ogr

#############################################################################


def Usage():
    print('Usage: vec_tr_spat.py [-spat xmin ymin xmax ymax] infile outfile [layer]')
    print('')
    return 2


def main(argv=sys.argv):
    infile = None
    outfile = None
    layer_name = None

    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-spat':
            s_minx = int(argv[i + 1])
            s_miny = int(argv[i + 2])
            s_maxx = int(argv[i + 3])
            s_maxy = int(argv[i + 4])
            i = i + 4

        elif infile is None:
            infile = arg

        elif outfile is None:
            outfile = arg

        elif layer_name is None:
            layer_name = arg

        else:
            return Usage()

        i = i + 1

    if outfile is None:
        return Usage()

    #############################################################################
    # Open the datasource to operate on.

    in_ds = ogr.Open(infile, update=0)

    if layer_name is not None:
        in_layer = in_ds.GetLayerByName(layer_name)
    else:
        in_layer = in_ds.GetLayer(0)

    in_defn = in_layer.GetLayerDefn()

    #############################################################################
    # Create output file with similar information.

    shp_driver = ogr.GetDriverByName('ESRI Shapefile')
    shp_driver.DeleteDataSource(outfile)

    shp_ds = shp_driver.CreateDataSource(outfile)

    shp_layer = shp_ds.CreateLayer(in_defn.GetName(),
                                   geom_type=in_defn.GetGeomType(),
                                   srs=in_layer.GetSpatialRef())

    in_field_count = in_defn.GetFieldCount()

    for fld_index in range(in_field_count):
        src_fd = in_defn.GetFieldDefn(fld_index)

        fd = ogr.FieldDefn(src_fd.GetName(), src_fd.GetType())
        fd.SetWidth(src_fd.GetWidth())
        fd.SetPrecision(src_fd.GetPrecision())
        shp_layer.CreateField(fd)

    #############################################################################
    # Apply spatial query.

    in_layer.SetSpatialFilterRect(s_minx, s_miny, s_maxx, s_maxy)

    #############################################################################
    # Setup rect geometry to try intersect with.

    ring = ogr.Geometry(ogr.wkbLinearRing)
    ring.AddPoint_2D(s_minx, s_miny)
    ring.AddPoint_2D(s_maxx, s_miny)
    ring.AddPoint_2D(s_maxx, s_maxy)
    ring.AddPoint_2D(s_minx, s_maxy)
    ring.AddPoint_2D(s_minx, s_miny)

    filt_geom = ogr.Geometry(ogr.wkbPolygon)
    filt_geom.AddGeometry(ring)

    #############################################################################
    # Process all features in input layer.

    in_feat = in_layer.GetNextFeature()
    while in_feat is not None:

        geom = in_feat.GetGeometryRef()

        if geom.Intersect(filt_geom) != 0:
            out_feat = ogr.Feature(feature_def=shp_layer.GetLayerDefn())
            out_feat.SetFrom(in_feat)

            shp_layer.CreateFeature(out_feat)
            out_feat.Destroy()

        in_feat.Destroy()
        in_feat = in_layer.GetNextFeature()

    #############################################################################
    # Cleanup

    shp_ds.Destroy()
    in_ds.Destroy()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
