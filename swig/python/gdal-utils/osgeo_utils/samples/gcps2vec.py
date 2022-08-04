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
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
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
    return 2


def main(argv=sys.argv):
    src_filename = None
    dst_filename = None
    driver_name = 'GML'
    pixel_out = False

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-of':
            i = i + 1
            driver_name = argv[i]

        elif arg == '-p':
            pixel_out = True

        elif src_filename is None:
            src_filename = argv[i]

        elif dst_filename is None:
            dst_filename = argv[i]

        else:
            return Usage()

        i = i + 1

    if dst_filename is None:
        return Usage()

    return gcps2vec(src_filename=src_filename, dst_filename=dst_filename, driver_name=driver_name, pixel_out=pixel_out)


def gcps2vec(src_filename: str, dst_filename: str, driver_name: str, pixel_out: bool = False):
    # ----------------------------------------------------------------------------
    # Open input file, and fetch GCPs.
    # ----------------------------------------------------------------------------
    ds = gdal.Open(src_filename)
    if ds is None:
        print(f'Unable to open {src_filename}')
        return 1

    gcp_srs = ds.GetGCPProjection()
    gcps = ds.GetGCPs()

    ds = None

    if gcps is None or not gcps:
        print(f'No GCPs on file {src_filename}!')
        return 1

    # ----------------------------------------------------------------------------
    # Create output file, and layer.
    # ----------------------------------------------------------------------------

    driver = ogr.GetDriverByName(driver_name)
    if driver is None:
        print(f'No driver named {driver_name} available.')
        return 1

    ds = driver.CreateDataSource(dst_filename)

    if not pixel_out and gcp_srs != "":
        srs = osr.SpatialReference()
        srs.ImportFromWkt(gcp_srs)
    else:
        srs = None

    if not pixel_out:
        geom_type = ogr.wkbPoint25D
    else:
        geom_type = ogr.wkbPoint

    layer = ds.CreateLayer('gcps', srs, geom_type=geom_type)

    if not pixel_out:
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

        if not pixel_out:
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
