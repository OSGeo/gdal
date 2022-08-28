#!/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Extract SOUNDGings from an S-57 dataset, and write them to
#           Shapefile format, creating one feature for each sounding, and
#           adding the elevation as an attribute for easier use.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

from osgeo import ogr

#############################################################################


def Usage():
    print("Usage: get_soundg.py <s57file> <shapefile>")
    print("")
    return 2


def main(argv=sys.argv):
    if len(argv) != 3:
        return Usage()

    s57filename = argv[1]
    shpfilename = argv[2]

    # -
    # Open the S57 file, and find the SOUNDG layer.

    ds = ogr.Open(s57filename)
    src_soundg = ds.GetLayerByName("SOUNDG")

    # -
    # Create the output shapefile.

    shp_driver = ogr.GetDriverByName("ESRI Shapefile")
    shp_driver.DeleteDataSource(shpfilename)

    shp_ds = shp_driver.CreateDataSource(shpfilename)

    shp_layer = shp_ds.CreateLayer("out", geom_type=ogr.wkbPoint25D)

    src_defn = src_soundg.GetLayerDefn()
    field_count = src_defn.GetFieldCount()

    # -
    # Copy the SOUNDG schema, and add an ELEV field.

    out_mapping = []
    for fld_index in range(field_count):
        src_fd = src_defn.GetFieldDefn(fld_index)

        fd = ogr.FieldDefn(src_fd.GetName(), src_fd.GetType())
        fd.SetWidth(src_fd.GetWidth())
        fd.SetPrecision(src_fd.GetPrecision())
        if shp_layer.CreateField(fd) != 0:
            out_mapping.append(-1)
        else:
            out_mapping.append(shp_layer.GetLayerDefn().GetFieldCount() - 1)

    fd = ogr.FieldDefn("ELEV", ogr.OFTReal)
    fd.SetWidth(12)
    fd.SetPrecision(4)
    shp_layer.CreateField(fd)

    #############################################################################
    # Process all SOUNDG features.

    feat = src_soundg.GetNextFeature()
    while feat is not None:

        multi_geom = feat.GetGeometryRef()

        for iPnt in range(multi_geom.GetGeometryCount()):
            pnt = multi_geom.GetGeometryRef(iPnt)

            feat2 = ogr.Feature(feature_def=shp_layer.GetLayerDefn())

            for fld_index in range(field_count):
                feat2.SetField(out_mapping[fld_index], feat.GetField(fld_index))

            feat2.SetField("ELEV", pnt.GetZ(0))
            feat2.SetGeometry(pnt)
            shp_layer.CreateFeature(feat2)
            feat2.Destroy()

        feat.Destroy()

        feat = src_soundg.GetNextFeature()

    #############################################################################
    # Cleanup

    shp_ds.Destroy()
    ds.Destroy()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
