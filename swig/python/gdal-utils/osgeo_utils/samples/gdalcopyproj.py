#!/usr/bin/env python3
# ******************************************************************************
#
#  Name:     gdalcopyproj.py
#  Project:  GDAL Python Interface
#  Purpose:  Duplicate the geotransform and projection metadata from
#            one raster dataset to another, which can be useful after
#            performing image manipulations with other software that
#            ignores or discards georeferencing metadata.
#  Author:   Schuyler Erle, schuyler@nocat.net
#
# ******************************************************************************
#  Copyright (c) 2005, Frank Warmerdam
#  Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import sys

from osgeo import gdal


def main(argv=sys.argv):
    if len(argv) < 3:
        print("Usage: gdalcopyproj.py source_file dest_file")
        return 2

    inp = argv[1]
    dataset = gdal.Open(inp)
    if dataset is None:
        print("Unable to open", inp, "for reading")
        return 1

    projection = dataset.GetProjection()
    geotransform = dataset.GetGeoTransform()

    if projection is None and geotransform is None:
        print("No projection or geotransform found on file" + input)
        return 1

    output = argv[2]
    dataset2 = gdal.Open(output, gdal.GA_Update)

    if dataset2 is None:
        print("Unable to open", output, "for writing")
        return 1

    if geotransform is not None and geotransform != (0, 1, 0, 0, 0, 1):
        dataset2.SetGeoTransform(geotransform)

    if projection is not None and projection != "":
        dataset2.SetProjection(projection)

    gcp_count = dataset.GetGCPCount()
    if gcp_count != 0:
        dataset2.SetGCPs(dataset.GetGCPs(), dataset.GetGCPProjection())

    dataset = None
    dataset2 = None
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
