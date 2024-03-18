#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Create a 2-level hierarchy of VRTs for very large collections
#  Author:   Even Rouault <even dot rouault at spatialys.com>
#
# ******************************************************************************
#  Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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
# ******************************************************************************

import glob
import math
import os
import sys

from osgeo import gdal, ogr
from osgeo_utils.auxiliary.gdal_argparse import GDALArgumentParser, GDALScript

gdal.UseExceptions()


class GDALBuildVRTOfVRT(GDALScript):
    def __init__(self):
        super().__init__()
        self.title = "Build a VRT of VRTs"
        self.description = (
            "Create a 2-level hierarchy of VRTs for very large collections"
        )

        self.optfile_arg = "--optfile"

    def get_parser(self, argv) -> GDALArgumentParser:
        parser = self.parser

        parser.add_argument("out_vrtfile", help="output VRT file")
        parser.add_argument("in_files", nargs="+", help="input files")

        parser.add_argument(
            "--max-files-per-vrt",
            dest="max_files_per_vrt",
            type=int,
            default=1000,
            metavar="number",
            help="maximum number of files per VRT",
        )

        parser.add_argument(
            "--intermediate-vrt-path",
            dest="intermediate_vrt_path",
            type=str,
            help="path where to put intermediate VRTs",
        )

        parser.add_argument(
            "-addalpha",
            action="store_true",
            help="whether to add an alpha channel",
        )

        parser.add_argument(
            "-tr",
            type=float,
            metavar="<res>",
            nargs=2,
            help="target resolution",
        )

        parser.add_argument(
            "-r",
            dest="resampling_alg",
            type=str,
            metavar="{nearest|bilinear|cubic|cubicspline|lanczos|average|mode}",
            help="resampling algorithm",
        )

        return parser

    def doit(self, **kwargs):
        out_vrtfile = kwargs["out_vrtfile"]
        assert out_vrtfile.endswith(".vrt")
        max_files_per_vrt = kwargs["max_files_per_vrt"]

        global_srs = "invalid"
        global_minx = float("inf")
        global_miny = float("inf")
        global_maxx = float("-inf")
        global_maxy = float("-inf")
        catalog = []

        tmp_index_ds = gdal.GetDriverByName("GPKG").Create(
            ":memory:", 0, 0, gdal.GDT_Unknown
        )
        lyr = tmp_index_ds.CreateLayer("index")
        lyr.CreateField(ogr.FieldDefn("filename", ogr.OFTString))

        print("Initial pass...")
        source_files = []
        for in_file in kwargs["in_files"]:
            if "*" in in_file:
                source_files += glob.glob(in_file)
            else:
                source_files.append(in_file)
        for in_file in source_files:
            ds = gdal.Open(in_file)
            gt = ds.GetGeoTransform(can_return_null=True)
            if gt is None:
                print(f"Skipping {in_file}. No geotransform.")
                continue
            if gt[5] > 0:
                print(f"Skipping {in_file}. South-up rasters are not supported.")
                continue
            if gt[2] != 0 or gt[4] != 0:
                print(f"Skipping {in_file}. Rotated rasters are not supported.")
                continue

            srs = ds.GetSpatialRef()
            if global_srs == "invalid":
                global_srs = srs
            else:
                if global_srs is not None and srs is None:
                    print(
                        f"Skipping {in_file}. It has no CRS, whereas other files have one."
                    )
                    continue
                if global_srs is None and srs is not None:
                    print(
                        f"Skipping {in_file}. It has a CRS, whereas other files have not one."
                    )
                    continue
                if (
                    global_srs is not None
                    and srs is not None
                    and not global_srs.IsSame(srs)
                ):
                    print(
                        f"Skipping {in_file}. It has a different CRS compared to the other files."
                    )
                    continue

            minx = gt[0]
            maxx = gt[0] + gt[1] * ds.RasterXSize
            maxy = gt[3]
            miny = gt[3] + gt[5] * ds.RasterYSize
            catalog.append((in_file, minx, miny, maxx, maxy))

            global_minx = min(global_minx, minx)
            global_miny = min(global_miny, miny)
            global_maxx = max(global_maxx, maxx)
            global_maxy = max(global_maxy, maxy)

            f = ogr.Feature(lyr.GetLayerDefn())
            f["filename"] = in_file
            g = ogr.CreateGeometryFromWkt(
                f"POLYGON(({minx} {miny},{minx} {maxy},{maxx} {maxy},{maxx} {miny},{minx} {miny}))"
            )
            f.SetGeometry(g)
            lyr.CreateFeature(f)

        number_of_files = len(catalog)
        if number_of_files == 0:
            raise Exception("No source files have been found!")

        number_of_vrts = int(math.ceil(number_of_files / max_files_per_vrt))
        sqrt_number_of_vrts = int(math.ceil(number_of_vrts**0.5))
        ratio_x_over_y = (global_maxx - global_minx) / (global_maxy - global_miny)
        num_vrt_along_x = int(math.ceil(sqrt_number_of_vrts / (ratio_x_over_y**0.5)))
        num_vrt_along_y = int(math.ceil(sqrt_number_of_vrts * (ratio_x_over_y**0.5)))

        stepx = (global_maxx - global_minx) / num_vrt_along_x
        stepy = (global_maxy - global_miny) / num_vrt_along_y

        vrt_options = ""

        if kwargs["addalpha"]:
            vrt_options += " -addalpha"

        tr = kwargs["tr"]
        if tr:
            vrt_options += f" -tr {tr[0]} {tr[1]}"

        resampling_alg = kwargs["resampling_alg"]
        if resampling_alg:
            vrt_options += f" -r {resampling_alg}"

        intermediate_vrt_path = kwargs["intermediate_vrt_path"]

        vrt_files = []
        for j in range(num_vrt_along_y):
            for i in range(num_vrt_along_x):
                if intermediate_vrt_path:
                    vrt_filename = os.path.join(
                        intermediate_vrt_path,
                        os.path.basename(out_vrtfile)[0:-4] + f"_{i}_{j}.vrt",
                    )
                else:
                    vrt_filename = out_vrtfile[0:-4] + f"_{i}_{j}.vrt"
                minx = global_minx + i * stepx
                maxx = global_minx + (i + 1) * stepx
                miny = global_miny + j * stepy
                maxy = global_miny + (j + 1) * stepy
                lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
                subvrt_files = [f["filename"] for f in lyr]
                if subvrt_files:
                    pct = (
                        100.0
                        * (j * num_vrt_along_x + i)
                        / (num_vrt_along_x * num_vrt_along_y)
                    )
                    print(f"Building {vrt_filename} (%.02f %%)..." % pct)
                    vrt_files.append(vrt_filename)
                    gdal.BuildVRT(vrt_filename, subvrt_files, options=vrt_options)

        print(f"Building {out_vrtfile} (100 %)...")
        gdal.BuildVRT(out_vrtfile, vrt_files, options=vrt_options)


def main(argv=sys.argv):
    return GDALBuildVRTOfVRT().main(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
