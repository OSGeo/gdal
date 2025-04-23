#!/usr/bin/env python3
# ******************************************************************************
#
#  Name:     gdalimport
#  Project:  GDAL Python Interface
#  Purpose:  Import a GDAL supported file to Tiled GeoTIFF, and build overviews
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2000, Frank Warmerdam
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import os.path
import sys

from osgeo import gdal


def progress_cb(complete, message, cb_data):
    print("%s %d" % (cb_data, complete))


def main(argv=sys.argv):
    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    if len(argv) < 2:
        print("Usage: gdalimport.py [--help-general] source_file [newfile]")
        return 2

    filename = argv[1]
    dataset = gdal.Open(filename)
    if dataset is None:
        print("Unable to open %s" % filename)
        return 1

    geotiff = gdal.GetDriverByName("GTiff")
    if geotiff is None:
        print("GeoTIFF driver not registered.")
        return 1

    if len(argv) < 3:
        newbase, ext = os.path.splitext(os.path.basename(filename))
        newfile = newbase + ".tif"
        i = 0
        while os.path.isfile(newfile):
            i = i + 1
            newfile = newbase + "_" + str(i) + ".tif"
    else:
        newfile = argv[2]

    print("Importing to Tiled GeoTIFF file: %s" % newfile)
    new_dataset = geotiff.CreateCopy(
        newfile,
        dataset,
        0,
        [
            "TILED=YES",
        ],
        callback=progress_cb,
        callback_data="Translate: ",
    )
    dataset = None

    print("Building overviews")
    new_dataset.BuildOverviews(
        "average", callback=progress_cb, callback_data="Overviews: "
    )
    new_dataset = None

    print("Done")


if __name__ == "__main__":
    sys.exit(main(sys.argv))
