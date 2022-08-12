#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Remove TOWGS84[] clause from dataset SRS definitions
#  Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
###############################################################################

import os
import subprocess
import sys

from osgeo import gdal, osr


def Usage():
    print("Usage: gdal_remove_towgs84.py [-q] filename")
    print("")
    print("(Try to) remove TOWGS84 clause from the dataset SRS definition, ")
    print("when it contains a EPSG code.")
    return 2


def main(argv=sys.argv):
    quiet = False
    filename = None
    for arg in argv[1:]:
        if arg == "-q":
            quiet = True
        elif arg[0] == "-":
            return Usage()
        elif filename is None:
            filename = arg
        else:
            return Usage()

    if filename is None:
        print("Missing filename.")
        return Usage()

    ds = gdal.Open(filename, gdal.GA_Update)
    if ds is None:
        return 1

    wkt = ds.GetProjectionRef()
    towgs84_pos = wkt.find("TOWGS84[")
    sr = osr.SpatialReference()
    sr.ImportFromWkt(wkt)

    version = gdal.VersionInfo("RELEASE_NAME")
    version_major = int(version[0 : version.find(".")])

    if ds.GetDriver().ShortName == "GTiff" and version_major < 3:
        output = None
        try:
            output = subprocess.check_output(["listgeo", filename]).decode("LATIN1")
        except Exception:
            pass
        if output and output.find("GeogTOWGS84GeoKey") < 0:
            # Check if listgeo is recent enough
            tmp_filename = "tmp_gdal_remove_towgs84.tif"
            tmp_ds = gdal.GetDriverByName("GTiff").Create(tmp_filename, 1, 1)
            tmp_ds.SetProjection(
                """GEOGCS["test",
            DATUM["test",
                SPHEROID["test",1,0],
                TOWGS84[1,2,3,4,5,6,7]],
            PRIMEM["Greenwich",0],
            UNIT["degree",0.0174532925199433]]"""
            )
            tmp_ds = None
            output = subprocess.check_output(["listgeo", tmp_filename]).decode("LATIN1")
            os.remove(tmp_filename)
            if output.find("GeogTOWGS84GeoKey") > 0:
                # The TOWGS84[] has been added by GDAL 2 GTiff driver, but is not
                # in the file
                towgs84_pos = -1

    if towgs84_pos > 0 and sr.GetAuthorityName(None) == "EPSG":
        endpos = wkt.find("],", towgs84_pos)
        assert endpos > 0
        wkt = wkt[0:towgs84_pos] + wkt[endpos + 2 :]
        assert ds.SetProjection(wkt) == 0
        if not quiet:
            print("%s patched to remove TOWGS84[] clause" % filename)
            if ds.GetDriver().ShortName == "GTiff" and version_major < 3:
                print(
                    "Note: Running gdalinfo on the patched file with GDAL < 3 will still show the TOWGS84[] clause, but it will not be seen by GDAL >= 3"
                )
    else:
        if not quiet:
            print("%s does not need patching" % filename)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
