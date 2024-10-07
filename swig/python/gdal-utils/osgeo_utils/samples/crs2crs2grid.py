#!/usr/bin/env python3
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Use HTDP to generate PROJ.4 compatible datum grid shift files.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# See also: http://www.ngs.noaa.gov/TOOLS/Htdp/Htdp.shtml
#           http://trac.osgeo.org/proj/wiki/HTDPGrids
#
# ******************************************************************************
#  Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import os
import sys

import numpy

from osgeo import gdal, gdal_array

# Input looks like this:


def points_in_file(fd):
    """Iterate through file grid.

    After the header, lines look like (using HTDP v. 3.5.0):
    50.0000015988  127.0000219914    -0.160    "PNT_0_0"
    """
    for line in fd:
        if line.strip() == "":
            continue
        elif line.find("PNT_") == -1:
            # Report the file header defining the transformation
            print(line)
            continue

        tokens = line.split()
        if len(tokens) != 4:
            print(
                "Found unexpected grid line format with %d tokens (expected 4)"
                % len(tokens)
            )
            print("Expected Format: dec_lat    dec_lon    elip_hgt    pt_id")
            print("Actual Line: " + line)
            break

        lat, lon, _, pt_id = tokens
        name_tokens = pt_id.replace('"', "").split("_")

        yield int(name_tokens[1]), int(name_tokens[2]), float(lat), float(lon)


def read_grid_crs_to_crs(input_grid, output_filename):
    grid = numpy.zeros(input_grid.shape)
    points_found = 0

    with open(output_filename, "r") as fd:
        for output in points_in_file(fd):
            i, j, lat_out, lon_out = output
            lon_in = input_grid[0, j, i]
            lat_in = input_grid[1, j, i]
            points_found += 1

            grid[0, j, i] = lon_out - lon_in
            grid[1, j, i] = lat_out - lat_in

    if points_found < grid.shape[0] * grid.shape[1]:
        print("Error - did not find results for every point in the input grid.")
        print("points found:   ", points_found)
        print("points expected:", grid.shape[1] * grid.shape[2])
        return None

    return grid


###############################################################################
# This function creates a regular grid of lat/long values with one
# "band" for latitude, and one for longitude.
#


def new_create_grid(griddef):

    lon_start = -1 * griddef[0]
    lon_end = -1 * griddef[2]

    lat_start = griddef[1]
    lat_end = griddef[3]

    lon_steps = griddef[4]
    lat_steps = griddef[5]

    lat_axis = numpy.linspace(lat_start, lat_end, lat_steps)
    lon_axis = numpy.linspace(lon_start, lon_end, lon_steps)

    lon_list = [lon_axis] * lat_steps
    lon_band = numpy.vstack(lon_list)

    lat_list = [lat_axis] * lon_steps
    lat_band = numpy.column_stack(lat_list)

    return numpy.array([lon_band, lat_band])


##############################################################################
# This function writes a grid out in form suitable to use as input to the
# htdp program.


def write_grid(grid, out_filename):
    fd_out = open(out_filename, "w")
    for i in range(grid.shape[2]):
        for j in range(grid.shape[1]):
            fd_out.write('%f %f 0 "PNT_%d_%d"\n' % (grid[1, j, i], grid[0, j, i], i, j))
    fd_out.close()


##############################################################################
# Write the resulting grid out in GeoTIFF format.


def write_gdal_grid(filename, grid, griddef):

    ps_x = (griddef[2] - griddef[0]) / (griddef[4] - 1)
    ps_y = (griddef[3] - griddef[1]) / (griddef[5] - 1)
    geotransform = (
        griddef[0] - ps_x * 0.5,
        ps_x,
        0.0,
        griddef[1] - ps_y * 0.5,
        0.0,
        ps_y,
    )

    grid = grid.astype(numpy.float32)
    ds = gdal_array.SaveArray(grid, filename, format="CTable2")
    ds.SetGeoTransform(geotransform)


#############################################################################


def write_control(
    control_fn,
    out_grid_fn,
    in_grid_fn,
    src_crs_id,
    src_crs_date,
    dst_crs_id,
    dst_crs_date,
):

    # start_date, end_date should be something like "2011.0"

    control_template = """
4
%s
%d
%d
2
%s
2
%s
3
%s
0
0
"""

    control_filled = control_template % (
        out_grid_fn,
        src_crs_id,
        dst_crs_id,
        src_crs_date,
        dst_crs_date,
        in_grid_fn,
    )

    open(control_fn, "w").write(control_filled)


#############################################################################


def Usage(brief=1):
    print(
        """
Usage: crs2crs2grid.py
        <src_crs_id> <src_crs_date> <dst_crs_id> <dst_crs_year>
        [-griddef <ul_lon> <ul_lat> <ll_lon> <ll_lat> <lon_count> <lat_count>]
        [-htdp <path_to_exe>] [-wrkdir <dirpath>] [-kwf]
        -o <output_grid_name>

 -griddef: by default the following values for roughly the continental USA
           at a six minute step size are used:
           -127 50 -66 25 251 611
 -kwf: keep working files in the working directory for review.

eg.
 crs2crs2grid.py 29 2002.0 8 2002.0 -o nad83_2002.ct2
 """
    )

    if brief == 0:
        print(
            """
The output file will be in CTable2 format suitable for use with PROJ.4
+nadgrids= directive.

Format dates like 2002.0 (for the start of 2002)

CRS Ids
-------
  1...NAD_83(2011) (North America tectonic plate fixed)
  29...NAD_83(CORS96)  (NAD_83(2011) will be used)
  30...NAD_83(2007)    (NAD_83(2011) will be used)
  2...NAD_83(PA11) (Pacific tectonic plate fixed)
  31...NAD_83(PACP00)  (NAD 83(PA11) will be used)
  3...NAD_83(MA11) (Mariana tectonic plate fixed)
  32...NAD_83(MARP00)  (NAD_83(MA11) will be used)

  4...WGS_72                             16...ITRF92
  5...WGS_84(transit) = NAD_83(2011)     17...ITRF93
  6...WGS_84(G730) = ITRF92              18...ITRF94 = ITRF96
  7...WGS_84(G873) = ITRF96              19...ITRF96
  8...WGS_84(G1150) = ITRF2000           20...ITRF97
  9...PNEOS_90 = ITRF90                  21...IGS97 = ITRF97
 10...NEOS_90 = ITRF90                   22...ITRF2000
 11...SIO/MIT_92 = ITRF91                23...IGS00 = ITRF2000
 12...ITRF88                             24...IGb00 = ITRF2000
 13...ITRF89                             25...ITRF2005
 14...ITRF90                             26...IGS05 = ITRF2005
 15...ITRF91                             27...ITRF2008
                                         28...IGS08 = ITRF2008
"""
        )

    return 2


#############################################################################
# Main


def main(argv=sys.argv):
    # Default GDAL argument parsing.

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    if len(argv) == 1:
        return Usage(brief=0)

    # Script argument defaults
    src_crs_id = None
    src_crs_date = None
    dst_crs_id = None
    dst_crs_date = None

    # Decent representation of continental US
    griddef = (-127.0, 50.0, -66.0, 25.0, 611, 251)

    htdp_path = "htdp"
    wrkdir = "."
    kwf = 0
    output_grid_name = None

    # Script argument parsing.

    i = 1
    while i < len(argv):

        if argv[i] == "-griddef" and i < len(argv) - 6:
            griddef = (
                float(argv[i + 1]),
                float(argv[i + 2]),
                float(argv[i + 3]),
                float(argv[i + 4]),
                float(argv[i + 5]),
                float(argv[i + 6]),
            )
            i = i + 6

        elif argv[i] == "-htdp" and i < len(argv) - 1:
            htdp_path = argv[i + 1]
            i = i + 1

        elif argv[i] == "-kwf":
            kwf = 1

        elif argv[i] == "-wrkdir" and i < len(argv) - 1:
            wrkdir = argv[i + 1]
            i = i + 1

        elif argv[i] == "-o" and i < len(argv) - 1:
            output_grid_name = argv[i + 1]
            i = i + 1

        elif argv[i] == "-h" or argv[i] == "--help":
            return Usage(brief=0)

        elif argv[i][0] == "-":
            print("Unrecognized argument: " + argv[i])
            return Usage()

        elif src_crs_id is None:
            src_crs_id = int(argv[i])

        elif src_crs_date is None:
            src_crs_date = argv[i]

        elif dst_crs_id is None:
            dst_crs_id = int(argv[i])

        elif dst_crs_date is None:
            dst_crs_date = argv[i]

        else:
            print("Unrecognized argument: " + argv[i])
            return Usage()

        i = i + 1
        # next argument

    if output_grid_name is None:
        print("Missing output grid name (-o)")
        return Usage()

    if dst_crs_date is None:
        print(
            "Source and Destination CRS Ids and Dates are mandatory, "
            "not all provided."
        )
        return Usage()

    # Do a bit of validation of parameters.
    if src_crs_id < 1 or src_crs_id > 32 or dst_crs_id < 1 or dst_crs_id > 32:
        print(
            "Invalid source or destination CRS Id %d and %d." % (src_crs_id, dst_crs_id)
        )
        return Usage(brief=0)

    if (
        float(src_crs_date) < 1700.0
        or float(src_crs_date) > 2300.0
        or float(dst_crs_date) < 1700.0
        or float(dst_crs_date) > 2300.0
    ):
        print(
            "Source or destination CRS date seems odd %s and %s."
            % (src_crs_date, dst_crs_date)
        )
        return Usage(brief=0)

    # Prepare out set of working file names.

    in_grid_fn = wrkdir + "/crs2crs_input.txt"
    out_grid_fn = wrkdir + "/crs2crs_output.txt"
    control_fn = wrkdir + "/crs2crs_control.txt"

    # Write out the source grid file.
    grid = new_create_grid(griddef)

    write_grid(grid, in_grid_fn)
    write_control(
        control_fn,
        out_grid_fn,
        in_grid_fn,
        src_crs_id,
        src_crs_date,
        dst_crs_id,
        dst_crs_date,
    )

    # Run htdp to transform the data.
    try:
        os.unlink(out_grid_fn)
    except OSError:
        pass

    rc = os.system(htdp_path + " < " + control_fn)
    if rc != 0:
        print("htdp run failed!")
        return 1

    print("htdp run complete.")

    adjustment = read_grid_crs_to_crs(grid, out_grid_fn)
    if adjustment is None:
        return 1

    # Convert shifts to radians
    adjustment = adjustment * (3.14159265358979323846 / 180.0)

    # write to output output grid file.
    write_gdal_grid(output_grid_name, adjustment, griddef)

    # cleanup working files unless they have been requested to remain.
    if kwf == 0:
        os.unlink(in_grid_fn)
        os.unlink(out_grid_fn)
        os.unlink(control_fn)

    print("Processing complete: see " + output_grid_name)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
