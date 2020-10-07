#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Convert GCPs to a point layer.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
# ******************************************************************************

import sys

from osgeo import gdal
from osgeo import ogr
from osgeo import osr


def Usage():
    print('Usage: gcps2vec.py [-of <ogr_drivername>] [-p] <raster_file> <vector_file>')
    sys.exit(1)


def main(argv):
    out_format = 'GML'
    in_file = None
    out_file = None
    pixel_out = 0

    gdal.AllRegister()
    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        sys.exit(0)

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-of':
            i = i + 1
            out_format = argv[i]

        elif arg == '-p':
            pixel_out = 1

        elif in_file is None:
            in_file = argv[i]

        elif out_file is None:
            out_file = argv[i]

        else:
            Usage()

        i = i + 1

    if out_file is None:
        Usage()

    # ----------------------------------------------------------------------------
    # Open input file, and fetch GCPs.
    # ----------------------------------------------------------------------------
    ds = gdal.Open(in_file)
    if ds is None:
        print('Unable to open %s' % in_file)
        sys.exit(1)

    gcp_srs = ds.GetGCPProjection()
    gcps = ds.GetGCPs()

    ds = None

    if gcps is None or not gcps:
        print('No GCPs on file %s!' % in_file)
        sys.exit(1)

    # ----------------------------------------------------------------------------
    # Create output file, and layer.
    # ----------------------------------------------------------------------------

    drv = ogr.GetDriverByName(out_format)
    if drv is None:
        print('No driver named %s available.' % out_format)
        sys.exit(1)

    ds = drv.CreateDataSource(out_file)

    if pixel_out == 0 and gcp_srs != "":
        srs = osr.SpatialReference()
        srs.ImportFromWkt(gcp_srs)
    else:
        srs = None

    if pixel_out == 0:
        geom_type = ogr.wkbPoint25D
    else:
        geom_type = ogr.wkbPoint

    layer = ds.CreateLayer('gcps', srs, geom_type=geom_type)

    if pixel_out == 0:
        fd = ogr.FieldDefn('Pixel', ogr.OFTReal)
        layer.CreateField(fd)

        fd = ogr.FieldDefn('Line', ogr.OFTReal)
        layer.CreateField(fd)
    else:
        fd = ogr.FieldDefn('X', ogr.OFTReal)
        layer.CreateField(fd)

        fd = ogr.FieldDefn('Y', ogr.OFTReal)
        layer.CreateField(fd)

        fd = ogr.FieldDefn('Z', ogr.OFTReal)
        layer.CreateField(fd)

    fd = ogr.FieldDefn('Id', ogr.OFTString)
    layer.CreateField(fd)

    fd = ogr.FieldDefn('Info', ogr.OFTString)
    layer.CreateField(fd)

    # ----------------------------------------------------------------------------
    # Write GCPs.
    # ----------------------------------------------------------------------------

    for gcp in gcps:

        feat = ogr.Feature(layer.GetLayerDefn())

        if pixel_out == 0:
            geom = ogr.Geometry(geom_type)
            feat.SetField('Pixel', gcp.GCPPixel)
            feat.SetField('Line', gcp.GCPLine)
            geom.SetPoint(0, gcp.GCPX, gcp.GCPY, gcp.GCPZ)
        else:
            geom = ogr.Geometry(geom_type)
            feat.SetField('X', gcp.GCPX)
            feat.SetField('Y', gcp.GCPY)
            feat.SetField('Z', gcp.GCPZ)
            geom.SetPoint(0, gcp.GCPPixel, gcp.GCPLine)

        feat.SetField('Id', gcp.Id)
        feat.SetField('Info', gcp.Info)

        feat.SetGeometryDirectly(geom)
        layer.CreateFeature(feat)

    feat = None
    ds.Destroy()


if __name__ == '__main__':
    sys.exit(main(sys.argv))
