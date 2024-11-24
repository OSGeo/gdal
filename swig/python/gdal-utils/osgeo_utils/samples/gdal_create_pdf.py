#!/usr/bin/env python3
###############################################################################
#
#  Project:  GDAL samples
#  Purpose:  Create a PDF from a XML composition file
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2019, Even Rouault<even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

from osgeo import gdal


def Usage():
    print("Usage: gdal_create_pdf composition.xml out.pdf")
    return 2


def gdal_create_pdf(argv):
    srcfile = None
    targetfile = None

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return -1

    for i in range(1, len(argv)):
        if argv[i][0] == "-":
            print("Unrecognized option : %s" % argv[i])
            return Usage()
        elif srcfile is None:
            srcfile = argv[i]
        elif targetfile is None:
            targetfile = argv[i]
        else:
            print("Unexpected option : %s" % argv[i])
            return Usage()

    if srcfile is None or targetfile is None:
        return Usage()

    out_ds = gdal.GetDriverByName("PDF").Create(
        targetfile, 0, 0, 0, gdal.GDT_Unknown, options=["COMPOSITION_FILE=" + srcfile]
    )
    return 0 if out_ds else 1


def main(argv=sys.argv):
    return gdal_create_pdf(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
