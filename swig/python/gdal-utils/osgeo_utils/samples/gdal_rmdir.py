#!/usr/bin/env python3
###############################################################################
#
#  Project:  GDAL samples
#  Purpose:  Delete a virtual directory
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

from osgeo import gdal


def Usage():
    print("Usage: gdal_rmdir filename")
    return 2


def gdal_rm(argv, progress=None):
    # pylint: disable=unused-argument
    filename = None
    recursive = False

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    for i in range(1, len(argv)):
        if argv[i] == "-r":
            recursive = True
        elif filename is None:
            filename = argv[i]
        elif argv[i][0] == "-":
            print("Unexpected option : %s" % argv[i])
            return Usage()
        else:
            print("Unexpected option : %s" % argv[i])
            return Usage()

    if filename is None:
        return Usage()

    if recursive:
        ret = gdal.RmdirRecursive(filename)
    else:
        ret = gdal.Rmdir(filename)
    if ret != 0:
        print("Deletion failed")
    return ret


def main(argv=sys.argv):
    return gdal_rm(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
