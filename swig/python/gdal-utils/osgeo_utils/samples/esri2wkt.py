#!/usr/bin/env python3
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Simple command line program for translating ESRI .prj files
#            into WKT.
#  Author:   Frank Warmerdam, warmerda@home.com
#
# ******************************************************************************
#  Copyright (c) 2000, Frank Warmerdam
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import sys

from osgeo import osr
from osgeo_utils.auxiliary.base import MaybeSequence, PathLikeOrStr
from osgeo_utils.auxiliary.gdal_argparse import GDALArgumentParser, GDALScript


def Usage():
    print(
        f"Usage: {sys.argv[0]} -- This is a sample. Read source to know how to use. --"
    )
    return 2


def esri2wkt(prj_filename: PathLikeOrStr):
    prj_fd = open(prj_filename)
    prj_lines = prj_fd.readlines()
    prj_fd.close()

    for i, prj_line in enumerate(prj_lines):
        prj_lines[i] = prj_line.rstrip()

    prj_srs = osr.SpatialReference()
    err = prj_srs.ImportFromESRI(prj_lines)
    if err != 0:
        print("Error = %d" % err)
        return 1
    else:
        print(prj_srs.ExportToPrettyWkt())
        return 0


def esri2wkt_multi(filenames: MaybeSequence[PathLikeOrStr]):
    if isinstance(filenames, PathLikeOrStr):
        return esri2wkt(filenames)
    else:
        res = 1
        for filename in filenames:
            res = esri2wkt(filename)
        return res


class ESRI2WKT(GDALScript):
    def __init__(self):
        super().__init__()
        self.title = "Transforms files from ESRI prj format into WKT format"

    def get_parser(self, argv) -> GDALArgumentParser:
        parser = self.parser

        parser.add_argument(
            "filenames", metavar="filename", type=str, nargs="*", help="esri .prj file"
        )

        return parser

    def doit(self, **kwargs):
        return esri2wkt_multi(**kwargs)


def main(argv=sys.argv):
    if len(sys.argv) < 2:
        return Usage()
    return ESRI2WKT().main(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
