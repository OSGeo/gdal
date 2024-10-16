#!/usr/bin/env python3
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Example doing range based classification
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import sys

import numpy as np

from osgeo import gdal, gdal_array


def Usage():
    print("Usage: classify.py src_filename dst_filename")
    print("")
    print("  classify.py utm.tif classes.tif")
    return 2


def doit(src_filename, dst_filename):
    class_defs = [(1, 10, 20), (2, 20, 30), (3, 128, 255)]

    src_ds = gdal.Open(src_filename)
    xsize = src_ds.RasterXSize
    ysize = src_ds.RasterYSize

    src_image = gdal_array.LoadFile(src_filename)

    dst_image = np.zeros((ysize, xsize))

    for class_info in class_defs:
        class_id = class_info[0]
        class_start = class_info[1]
        class_end = class_info[2]

        class_value = np.ones((ysize, xsize)) * class_id

        mask = np.bitwise_and(
            np.greater_equal(src_image, class_start),
            np.less_equal(src_image, class_end),
        )

        dst_image = np.choose(mask, (dst_image, class_value))

    gdal_array.SaveArray(dst_image, dst_filename)


def main(argv=sys.argv, src_filename=None, dst_filename=None):
    if len(argv) > 1:
        src_filename = argv[1]
    if len(argv) > 2:
        dst_filename = argv[2]
    if not src_filename or not dst_filename:
        return Usage()
    return doit(src_filename, dst_filename)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
