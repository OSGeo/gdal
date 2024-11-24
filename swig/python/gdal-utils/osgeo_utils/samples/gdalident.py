#!/usr/bin/env python3
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Application to identify files by format.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************
#

import os
import stat
import sys

from osgeo import gdal


def Usage():
    print("Usage: gdalident.py [-r] file(s)")
    return 2


def ProcessTarget(target, recursive, report_failure, filelist=None):
    if filelist is not None:
        driver = gdal.IdentifyDriver(target, filelist)
    else:
        driver = gdal.IdentifyDriver(target)

    if driver is not None:
        print("%s: %s" % (target, driver.ShortName))
    elif report_failure:
        print("%s: unrecognized" % target)

    if recursive and driver is None:
        try:
            mode = os.stat(target)[stat.ST_MODE]
        except OSError:
            mode = 0

        if stat.S_ISDIR(mode):
            subfilelist = os.listdir(target)
            for item in subfilelist:
                subtarget = os.path.join(target, item)
                ProcessTarget(subtarget, 1, report_failure, subfilelist)


def main(argv=sys.argv):
    recursive = 0
    report_failure = 0
    files = []

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == "-r":
            recursive = 1

        elif arg == "-f":
            report_failure = 1

        else:
            files.append(arg)

        i = i + 1

    if not files:
        return Usage()

    for f in files:
        ProcessTarget(f, recursive, report_failure)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
