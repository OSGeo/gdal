#!/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  GDAL Python Samples
# Purpose:  Report histogram from file.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam, warmerdam@pobox.com
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal

# =============================================================================


def Usage():
    print("Usage: histrep.py [-force] input_file")
    print("   or")
    print("       histrep.py -req <min> <max> <buckets> [-force] [-approxok]")
    print("                  [-ioor] input_file")
    print("")
    return 2


def main(argv=sys.argv):
    argv = gdal.GeneralCmdLineProcessor(argv)

    req = None
    force = 0
    approxok = 0
    ioor = 0

    filename = None

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == "-req":
            req = (float(argv[i + 1]), float(argv[i + 2]), int(argv[i + 3]))
            i = i + 3

        elif arg == "-ioor":
            ioor = 1

        elif arg == "-approxok":
            approxok = 1

        elif arg == "-force":
            force = 1

        elif filename is None:
            filename = arg

        else:
            return Usage()

        i = i + 1

    if filename is None:
        return Usage()

    # -----------------------------------------------------------------------
    ds = gdal.Open(filename)

    if req is None:
        hist = ds.GetRasterBand(1).GetDefaultHistogram(force=force)

        if hist is None:
            print("No default histogram.")
        else:
            print("Default Histogram:")
            print("Min: ", hist[0])
            print("Max: ", hist[1])
            print("Buckets: ", hist[2])
            print("Histogram: ", hist[3])

    else:
        hist = ds.GetRasterBand(1).GetHistogram(req[0], req[1], req[2], ioor, approxok)

        if hist is not None:
            print("Histogram: ", hist)

    ds = None
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
