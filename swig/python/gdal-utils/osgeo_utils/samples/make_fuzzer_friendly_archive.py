#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Make fuzzer friendly archive (only works in DEBUG mode)
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# ******************************************************************************
#  Copyright (c) 2016 Even Rouault, <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import os
import sys


def Usage():
    print(
        f"Usage: {sys.argv[0]} -- This is a sample. Read source to know how to use. --"
    )
    return 2


def main(argv=sys.argv):
    if len(sys.argv) < 2:
        return Usage()
    fout = open(argv[1], "wb")
    fout.write("FUZZER_FRIENDLY_ARCHIVE\n".encode("ascii"))
    for filename in argv[2:]:
        fout.write(("***NEWFILE***:%s\n" % os.path.basename(filename)).encode("ascii"))
        fout.write(open(filename, "rb").read())
    fout.close()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
