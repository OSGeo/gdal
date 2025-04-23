#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Outputs GDAL GCPs as OGR points
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# ******************************************************************************
#  Copyright (c) 2015, Even Rouault, <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import sys

from osgeo import gdal, ogr, osr


def Usage():
    print("Usage: gcp2ogr.py [-f ogr_drv_name] gdal_in_dataset ogr_out_dataset")
    return 2


def main(argv=sys.argv):
    out_format = "ESRI Shapefile"
    in_dataset = None
    out_dataset = None
    i = 1
    while i < len(argv):
        if argv[i] == "-f":
            i += 1
            out_format = argv[i]
        elif argv[i][0] == "-":
            Usage()
        elif in_dataset is None:
            in_dataset = argv[i]
        elif out_dataset is None:
            out_dataset = argv[i]
        else:
            Usage()
        i += 1

    if out_dataset is None:
        return Usage()

    ds = gdal.Open(in_dataset)
    out_ds = ogr.GetDriverByName(out_format).CreateDataSource(out_dataset)
    sr = None
    wkt = ds.GetGCPProjection()
    if wkt != "":
        sr = osr.SpatialReference(wkt)
    out_lyr = out_ds.CreateLayer("gcps", geom_type=ogr.wkbPoint, srs=sr)
    out_lyr.CreateField(ogr.FieldDefn("Id", ogr.OFTString))
    out_lyr.CreateField(ogr.FieldDefn("Info", ogr.OFTString))
    out_lyr.CreateField(ogr.FieldDefn("X", ogr.OFTReal))
    out_lyr.CreateField(ogr.FieldDefn("Y", ogr.OFTReal))
    gcps = ds.GetGCPs()
    for gcp in gcps:
        f = ogr.Feature(out_lyr.GetLayerDefn())
        f.SetField("Id", gcp.Id)
        f.SetField("Info", gcp.Info)
        f.SetField("X", gcp.GCPPixel)
        f.SetField("Y", gcp.GCPLine)
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(%f %f)" % (gcp.GCPX, gcp.GCPY)))
        out_lyr.CreateFeature(f)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
