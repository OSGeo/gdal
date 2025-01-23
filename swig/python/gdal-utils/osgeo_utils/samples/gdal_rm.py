#!/usr/bin/env python3
###############################################################################
#
#  Project:  GDAL samples
#  Purpose:  Delete a virtual file
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
    print("Usage: gdal_rm [-r] filename")
    return 2


def gdal_rm_recurse(filename, simulate=False):

    delete_self = True
    if filename.endswith("/*"):
        delete_self = False
        filename = filename[0:-2]

    dir_contents = gdal.ReadDir(filename)
    if dir_contents:
        for f in dir_contents:
            if f not in (".", ".."):
                ret = gdal_rm_recurse(filename + "/" + f, simulate=simulate)
                if ret != 0:
                    return ret
        if not delete_self:
            return 0
        elif simulate:
            print("Rmdir(%s)" % filename)
            return 0
        else:
            ret = gdal.Rmdir(filename)
            # Some filesystems, like /vsiaz/ don't have a real directory
            # implementation. As soon as you remove the last file in the dir,
            # the dir "disappears".
            if ret < 0:
                if gdal.VSIStatL(filename) is None:
                    ret = 0
            return ret
    else:
        if simulate:
            print("Unlink(%s)" % filename)
            return 0
        return gdal.Unlink(filename)


def gdal_rm(argv, progress=None):
    # pylint: disable=unused-argument
    filename = None
    recurse = False
    simulate = False

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    for i in range(1, len(argv)):
        if not argv[i]:
            return Usage()

        if argv[i] == "-r":
            recurse = True
        elif argv[i] == "-simulate":
            simulate = True
        elif argv[i][0] == "-":
            print("Unexpected option : %s" % argv[i])
            return Usage()
        elif filename is None:
            filename = argv[i]
        else:
            print("Unexpected option : %s" % argv[i])
            return Usage()

    if filename is None:
        return Usage()

    if filename == "/":
        user_input = input("Please confirm with YES your action: ")
        if user_input != "YES":
            print("Aborted")
            return 1

    if recurse:
        ret = gdal_rm_recurse(filename, simulate=simulate)
    else:
        if simulate:
            print("gdal.Unlink(%s)" % filename)
            ret = 0
        else:
            ret = gdal.Unlink(filename)
    if ret != 0:
        print("Deletion failed")
    return ret


def main(argv=sys.argv):
    return gdal_rm(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
