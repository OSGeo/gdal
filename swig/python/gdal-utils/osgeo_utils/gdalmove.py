#!/usr/bin/env python3
# ******************************************************************************
#
#  Project:  GDAL Python Interface
#  Purpose:  Application for "warping" an image by just updating it's SRS
#            and geotransform.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2012, Frank Warmerdam
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import math
import sys
from typing import Optional

from osgeo import gdal, osr
from osgeo_utils.auxiliary.util import enable_gdal_exceptions

###############################################################################


def fmt_loc(srs_obj, loc):
    if srs_obj.IsProjected():
        return "%12.3f %12.3f" % (loc[0], loc[1])
    return "%12.8f %12.8f" % (loc[0], loc[1])


###############################################################################


@enable_gdal_exceptions
def move(
    filename: str,
    t_srs: str,
    s_srs: Optional[str] = None,
    pixel_threshold: Optional[float] = None,
):

    # -------------------------------------------------------------------------
    # Open the file.
    # -------------------------------------------------------------------------
    ds = gdal.Open(filename)

    # -------------------------------------------------------------------------
    # Compute the current (s_srs) locations of the four corners and center
    # of the image.
    # -------------------------------------------------------------------------
    corners_names = ["Upper Left", "Lower Left", "Upper Right", "Lower Right", "Center"]

    corners_pixel_line = [
        (0, 0, 0),
        (0, ds.RasterYSize, 0),
        (ds.RasterXSize, 0, 0),
        (ds.RasterXSize, ds.RasterYSize, 0),
        (ds.RasterXSize / 2.0, ds.RasterYSize / 2.0, 0.0),
    ]

    orig_gt = ds.GetGeoTransform()

    corners_s_geo = []
    for item in corners_pixel_line:
        corners_s_geo.append(
            (
                orig_gt[0] + item[0] * orig_gt[1] + item[1] * orig_gt[2],
                orig_gt[3] + item[0] * orig_gt[4] + item[1] * orig_gt[5],
                item[2],
            )
        )

    # -------------------------------------------------------------------------
    # Prepare a transformation from source to destination srs.
    # -------------------------------------------------------------------------
    if s_srs is None:
        s_srs = ds.GetProjectionRef()

    s_srs_obj = osr.SpatialReference()
    s_srs_obj.SetFromUserInput(s_srs)

    t_srs_obj = osr.SpatialReference()
    t_srs_obj.SetFromUserInput(t_srs)

    tr = osr.CoordinateTransformation(s_srs_obj, t_srs_obj)

    # -------------------------------------------------------------------------
    # Transform the corners
    # -------------------------------------------------------------------------

    corners_t_geo = tr.TransformPoints(corners_s_geo)

    # -------------------------------------------------------------------------
    #  Compute a new geotransform for the image in the target SRS.  For now
    #  we just use the top left, top right, and bottom left to produce the
    #  geotransform.  The result will be exact at these three points by
    #  definition, but if the underlying transformation is not affine it will
    #  be wrong at the center and bottom right.  It would be better if we
    #  used all five points for a least squares fit but that is a bit beyond
    #  me for now.
    # -------------------------------------------------------------------------
    ul = corners_t_geo[0]
    ur = corners_t_geo[2]
    ll = corners_t_geo[1]

    new_gt = (
        ul[0],
        (ur[0] - ul[0]) / ds.RasterXSize,
        (ll[0] - ul[0]) / ds.RasterYSize,
        ul[1],
        (ur[1] - ul[1]) / ds.RasterXSize,
        (ll[1] - ul[1]) / ds.RasterYSize,
    )

    inv_new_gt = gdal.InvGeoTransform(new_gt)

    # -------------------------------------------------------------------------
    #  Report results for the five locations.
    # -------------------------------------------------------------------------

    corners_t_new_geo = []
    error_geo = []
    error_pixel_line = []
    corners_pixel_line_new = []

    print(
        "___Corner___ ________Original________  _______Adjusted_________   ______ Err (geo) ______ _Err (pix)_"
    )

    for i in range(len(corners_s_geo)):  # pylint: disable=consider-using-enumerate

        item = corners_pixel_line[i]
        corners_t_new_geo.append(
            (
                new_gt[0] + item[0] * new_gt[1] + item[1] * new_gt[2],
                new_gt[3] + item[0] * new_gt[4] + item[1] * new_gt[5],
                item[2],
            )
        )

        error_geo.append(
            (
                corners_t_new_geo[i][0] - corners_t_geo[i][0],
                corners_t_new_geo[i][1] - corners_t_geo[i][1],
                0.0,
            )
        )

        item = corners_t_geo[i]
        corners_pixel_line_new.append(
            (
                inv_new_gt[0] + item[0] * inv_new_gt[1] + item[1] * inv_new_gt[2],
                inv_new_gt[3] + item[0] * inv_new_gt[4] + item[1] * inv_new_gt[5],
                item[2],
            )
        )

        error_pixel_line.append(
            (
                corners_pixel_line_new[i][0] - corners_pixel_line[i][0],
                corners_pixel_line_new[i][1] - corners_pixel_line[i][1],
                corners_pixel_line_new[i][2] - corners_pixel_line[i][2],
            )
        )

        print(
            "%-11s %s %s %s %5.2f %5.2f"
            % (
                corners_names[i],
                fmt_loc(s_srs_obj, corners_s_geo[i]),
                fmt_loc(t_srs_obj, corners_t_geo[i]),
                fmt_loc(t_srs_obj, error_geo[i]),
                error_pixel_line[i][0],
                error_pixel_line[i][1],
            )
        )

    print("")

    # -------------------------------------------------------------------------
    # Do we want to update the file?
    # -------------------------------------------------------------------------
    max_error = 0
    for err_item in error_pixel_line:
        this_error = math.sqrt(err_item[0] * err_item[0] + err_item[1] * err_item[1])
        if this_error > max_error:
            max_error = this_error

    update = False
    if pixel_threshold is not None:
        if pixel_threshold > max_error:
            update = True

    # -------------------------------------------------------------------------
    # Apply the change coordinate system and geotransform.
    # -------------------------------------------------------------------------
    if update:
        ds = None
        ds = gdal.Open(filename, gdal.GA_Update)

        print("Updating file...")
        ds.SetGeoTransform(new_gt)
        ds.SetProjection(t_srs_obj.ExportToWkt())
        print("Done.")

    elif pixel_threshold is None:
        print("No error threshold in pixels selected with -et, file not updated.")

    else:
        print(
            f"""Maximum check point error is {max_error:.5f} pixels which exceeds the
                error threshold so the file has not been updated."""
        )

    ds = None


###############################################################################


def Usage(isError=True):
    f = sys.stderr if isError else sys.stdout
    print(
        """Usage: gdalmove [--help] [--help-general]
                   [-s_srs <srs_defn>] -t_srs <srs_defn>
                   [-et <max_pixel_err>] <target_file>""",
        file=f,
    )
    return 2 if isError else 0


def main(argv=sys.argv):

    # Default GDAL argument parsing.
    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Script argument defaults
    s_srs = None
    t_srs = None
    filename = None
    pixel_threshold = None

    # Script argument parsing.

    i = 1
    while i < len(argv):

        if argv[i] == "--help":
            return Usage(isError=False)

        elif argv[i] == "-s_srs" and i < len(argv) - 1:
            s_srs = argv[i + 1]
            i += 1

        elif argv[i] == "-t_srs" and i < len(argv) - 1:
            t_srs = argv[i + 1]
            i += 1

        elif argv[i] == "-et" and i < len(argv) - 1:
            pixel_threshold = float(argv[i + 1])
            i += 1

        elif filename is None:
            filename = argv[i]

        else:
            print("Urecognised argument: " + argv[i])
            return Usage()

        i = i + 1
        # next argument

    if len(argv) == 1:
        return Usage()

    if filename is None:
        print("Missing name of file to operate on, but required.")
        return Usage()

    if t_srs is None:
        print("Target SRS (-t_srs) missing, but required.")
        return Usage()

    move(filename, t_srs=t_srs, s_srs=s_srs, pixel_threshold=pixel_threshold)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
