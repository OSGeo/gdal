#!/usr/bin/env python3
###############################################################################
#
# Project:  OGR Python samples
# Purpose:  Produce a graticule (grid) dataset.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

from osgeo import ogr, osr


#############################################################################
def float_range(*args):
    start = 0.0
    step = 1.0
    if len(args) == 1:
        (stop,) = args
    elif len(args) == 2:
        (start, stop) = args
    elif len(args) == 3:
        (start, stop, step) = args
    else:
        raise TypeError("float_range needs 1-3 float arguments")

    the_range = []
    steps = (stop - start) / step
    if steps != int(steps):
        steps = steps + 1.0
    for i in range(int(steps)):
        the_range.append(i * step + start)

    return the_range


#############################################################################
def Usage():
    print("Usage: mkgraticule [-connected] [-s stepsize] [-substep substepsize]")
    print("         [-t_srs srs] [-range xmin ymin xmax ymax] outfile")
    print("")
    print(
        "Defaults to: not connected, step & substep of 5.0, no SRS, range -180,-90 180,90"
    )
    return 2


def main(argv=sys.argv):
    # Argument processing.
    t_srs = None
    stepsize = 5.0
    substepsize = 5.0
    connected = 0
    outfile = None

    xmin = -180
    xmax = 180
    ymin = -90
    ymax = 90

    i = 1
    while i < len(argv):
        if argv[i] == "-connected":
            connected = 1
        elif argv[i] == "-t_srs":
            i = i + 1
            t_srs = argv[i]
        elif argv[i] == "-s":
            i = i + 1
            stepsize = float(argv[i])
        elif argv[i] == "-substep":
            i = i + 1
            substepsize = float(argv[i])
        elif argv[i] == "-range":
            xmin = float(argv[i + 1])
            ymin = float(argv[i + 2])
            xmax = float(argv[i + 3])
            ymax = float(argv[i + 4])
            i = i + 4
        elif argv[i][0] == "-":
            return Usage()
        elif outfile is None:
            outfile = argv[i]
        else:
            return Usage()

        i = i + 1

    if outfile is None:
        print("""\nNo outfile specified, e.g. 'graticule.shp'.\n""")
        return Usage()

    if substepsize > stepsize:
        substepsize = stepsize

    # -
    # Do we have an alternate SRS?

    ct = None

    if t_srs is not None:
        t_srs_o = osr.SpatialReference()
        t_srs_o.SetFromUserInput(t_srs)

        s_srs_o = osr.SpatialReference()
        s_srs_o.SetFromUserInput("WGS84")

        ct = osr.CoordinateTransformation(s_srs_o, t_srs_o)
    else:
        t_srs_o = osr.SpatialReference()
        t_srs_o.SetFromUserInput("WGS84")

    # -
    # Create graticule file.

    drv = ogr.GetDriverByName("ESRI Shapefile")

    try:
        drv.DeleteDataSource(outfile)
    except Exception:
        pass

    ds = drv.CreateDataSource(outfile)
    layer = ds.CreateLayer("out", geom_type=ogr.wkbLineString, srs=t_srs_o)

    #########################################################################
    # Not connected case.  Produce individual segments are these are going to
    # be more resilient in the face of reprojection errors.

    if not connected:
        #########################################################################
        # Generate lines of latitude.

        feat = ogr.Feature(feature_def=layer.GetLayerDefn())
        geom = ogr.Geometry(type=ogr.wkbLineString)

        for lat in float_range(ymin, ymax + stepsize / 2, stepsize):
            for long_ in float_range(xmin, xmax - substepsize / 2, substepsize):

                geom.SetPoint(0, long_, lat)
                geom.SetPoint(1, long_ + substepsize, lat)

                err = 0
                if ct is not None:
                    err = geom.Transform(ct)

                if err == 0:
                    feat.SetGeometry(geom)
                    layer.CreateFeature(feat)

        #########################################################################
        # Generate lines of longitude

        for long_ in float_range(xmin, xmax + stepsize / 2, stepsize):
            for lat in float_range(ymin, ymax - substepsize / 2, substepsize):
                geom.SetPoint(0, long_, lat)
                geom.SetPoint(1, long_, lat + substepsize)

                err = 0
                if ct is not None:
                    err = geom.Transform(ct)

                if err == 0:
                    feat.SetGeometry(geom)
                    layer.CreateFeature(feat)

    #########################################################################
    # Connected case - produce one polyline for each complete line of latitude
    # or longitude.

    if connected:
        #########################################################################
        # Generate lines of latitude.

        feat = ogr.Feature(feature_def=layer.GetLayerDefn())

        for lat in float_range(ymin, ymax + stepsize / 2, stepsize):

            geom = ogr.Geometry(type=ogr.wkbLineString)

            for long_ in float_range(xmin, xmax + substepsize / 2, substepsize):
                geom.AddPoint(long_, lat)

            err = 0
            if ct is not None:
                err = geom.Transform(ct)

            if err == 0:
                feat.SetGeometry(geom)
                layer.CreateFeature(feat)

        #########################################################################
        # Generate lines of longitude

        for long_ in float_range(xmin, xmax + stepsize / 2, stepsize):

            geom = ogr.Geometry(type=ogr.wkbLineString)

            for lat in float_range(ymin, ymax + substepsize / 2, substepsize):
                geom.AddPoint(long_, lat)

            err = 0
            if ct is not None:
                err = geom.Transform(ct)

            if err == 0:
                feat.SetGeometry(geom)
                layer.CreateFeature(feat)

    #############################################################################
    # Cleanup

    feat = None
    geom = None

    ds.Destroy()
    ds = None

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
