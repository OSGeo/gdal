#!/usr/bin/env python3
# ******************************************************************************
#
#  Name:     gcps2wld
#  Project:  GDAL Python Interface
#  Purpose:  Translate the set of GCPs on a file into first order approximation
#            in world file format.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2002, Frank Warmerdam
#  Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import sys

from osgeo import gdal


def main(argv=sys.argv):
    if len(argv) < 2:
        print("Usage: gcps2wld.py source_file")
        return 2

    filename = argv[1]
    dataset = gdal.Open(filename)
    if dataset is None:
        print("Unable to open %s" % filename)
        return 1

    gcps = dataset.GetGCPs()

    if gcps is None or not gcps:
        print("No GCPs found on file " + filename)
        return 1

    geotransform = gdal.GCPsToGeoTransform(gcps)

    if geotransform is None:
        print("Unable to extract a geotransform.")
        return 1

    print(geotransform[1])
    print(geotransform[4])
    print(geotransform[2])
    print(geotransform[5])
    print(geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2])
    print(geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
