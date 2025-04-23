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
# SPDX-License-Identifier: MIT
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

        parser.add_argument(
            "--stop-on-error",
            dest="stop_on_error",
            action="store_true",
            help="whether an error when opening a source file should stop the whole process (by default processing continues skipping it)",
        )

        parser.add_argument(
            "--intermediate-vrt-add-overviews",
            dest="intermediate_vrt_add_overviews",
            action="store_true",
            help="whether overviews should be generated on intermediate VRTs (overview factors automatically determined)",
        )

        def list_of_ints(arg):
            return list(map(int, arg.split(",")))

        parser.add_argument(
            "--intermediate-vrt-overview-factors",
            dest="intermediate_vrt_overview_factors",
            type=list_of_ints,
            metavar="<factor1>[,<factor2>]...",
            help="Ask for overviews to be generated on intermediated VRTs and specify overview factor(s)",
        )

        def list_of_ints(arg):
            return list(map(int, arg.split(",")))

        parser.add_argument(
            "--overview-compression",
            dest="overview_compression",
            type=str,
            choices=("NONE", "LZW", "DEFLATE", "ZSTD", "JPEG", "LERC", "JXL"),
            default="LZW",
            help="overview compression algorithm",
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

            def deal_with_error(msg):
                if kwargs["stop_on_error"]:
                    raise Exception(f"Error on {in_file}: {msg}")
                else:
                    print(f"Skipping {in_file}. {msg}")

            try:
                ds = gdal.Open(in_file)
            except Exception as e:
                deal_with_error(f"{e}")
                continue
            gt = ds.GetGeoTransform(can_return_null=True)
            if gt is None:
                deal_with_error("No geotransform.")
                continue
            if gt[5] > 0:
                deal_with_error("South-up rasters are not supported.")
                continue
            if gt[2] != 0 or gt[4] != 0:
                deal_with_error("Rotated rasters are not supported.")
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

                    if kwargs["intermediate_vrt_overview_factors"]:
                        print(f"Building {vrt_filename}.ovr (%.02f %%)..." % pct)
                        ds = gdal.Open(vrt_filename)
                        with gdal.config_option(
                            "COMPRESS_OVERVIEW", kwargs["overview_compression"]
                        ):
                            ds.BuildOverviews(
                                kwargs["resampling_alg"],
                                kwargs["intermediate_vrt_overview_factors"],
                            )
                    elif kwargs["intermediate_vrt_add_overviews"]:
                        ds = gdal.Open(vrt_filename)
                        factors = []
                        max_dim = max(ds.RasterXSize, ds.RasterYSize)
                        factor = 2
                        while max_dim > 256:
                            factors.append(factor)
                            max_dim = max_dim // 2
                            factor *= 2
                        if factors:
                            print(f"Building {vrt_filename}.ovr (%.02f %%)..." % pct)
                            with gdal.config_option(
                                "COMPRESS_OVERVIEW", kwargs["overview_compression"]
                            ):
                                ds.BuildOverviews(kwargs["resampling_alg"], factors)

        print(f"Building {out_vrtfile} (100 %)...")
        gdal.BuildVRT(out_vrtfile, vrt_files, options=vrt_options)


def main(argv=sys.argv):
    return GDALBuildVRTOfVRT().main(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
