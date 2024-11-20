# !/usr/bin/env python3
###############################################################################
# Project:  GDAL utils
# Purpose:  Get min/max location
# Author:   Even Rouault <even@spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys
import textwrap
from typing import Optional

from osgeo import gdal, osr
from osgeo_utils.auxiliary.gdal_argparse import GDALArgumentParser, GDALScript
from osgeo_utils.auxiliary.util import PathOrDS, open_ds


def gdalminmaxlocation_util(
    filename_or_ds: PathOrDS,
    band_num: int,
    open_options: Optional[dict] = None,
    **kwargs,
):
    ds = open_ds(filename_or_ds, open_options=open_options)
    band = ds.GetRasterBand(band_num)
    ret = band.ComputeMinMaxLocation()
    if ret is None:
        print("No valid pixels")
        return 1
    gt = ds.GetGeoTransform(can_return_null=True)
    if gt:
        srs = ds.GetSpatialRef()
        if srs:
            wgs84 = osr.SpatialReference()
            wgs84.SetFromUserInput("WGS84")
            wgs84.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
            ct = osr.CreateCoordinateTransformation(srs, wgs84)
            georefX, georefY = gdal.ApplyGeoTransform(
                gt, ret.minX + 0.5, ret.minY + 0.5
            )
            long, lat, _ = ct.TransformPoint(georefX, georefY)
            print(
                f"Minimum={ret.min} at (col,line)=({ret.minX},{ret.minY}), (X,Y)_georef=({georefX},{georefY}), (long,lat)_WGS84=({long:.7f},{lat:.7f})"
            )
            georefX, georefY = gdal.ApplyGeoTransform(
                gt, ret.maxX + 0.5, ret.maxY + 0.5
            )
            long, lat, _ = ct.TransformPoint(georefX, georefY)
            print(
                f"Maximum={ret.max} at (col,line)=({ret.maxX},{ret.maxY}), (X,Y)_georef=({georefX},{georefY}), (long,lat)_WGS84=({long:.7f},{lat:.7f})"
            )
        else:
            georefX, georefY = gdal.ApplyGeoTransform(
                gt, ret.minX + 0.5, ret.minY + 0.5
            )
            print(
                f"Minimum={ret.min} at (col,line)=({ret.minX},{ret.minY}), (X,Y)_georef=({georefX},{georefY})"
            )
            georefX, georefY = gdal.ApplyGeoTransform(
                gt, ret.maxX + 0.5, ret.maxY + 0.5
            )
            print(
                f"Maximum={ret.max} at (col,line)=({ret.maxX},{ret.maxY}), (X,Y)_georef=({georefX},{georefY})"
            )
    else:
        print(f"Minimum={ret.min} at (col,line)=({ret.minX},{ret.minY})")
        print(f"Maximum={ret.max} at (col,line)=({ret.maxX},{ret.maxY})")

    return 0


class GDALMinMaxLocation(GDALScript):
    def __init__(self):
        super().__init__()
        self.title = "Raster min/max location query tool"
        self.description = textwrap.dedent(
            """\
            The gdal_minmax_location utility returns the location where min/max values of a raster are hit."""
        )
        self.interactive_mode = None

    def get_parser(self, argv) -> GDALArgumentParser:
        parser = self.parser

        parser.add_argument(
            "-b",
            dest="band_num",
            metavar="band",
            type=int,
            default=1,
            help="Selects a band to query (default: first one).",
        )

        parser.add_argument(
            "-oo",
            dest="open_options",
            metavar="NAME=VALUE",
            help="Dataset open option (format specific).",
            nargs="+",
        )

        parser.add_argument(
            "filename_or_ds",
            metavar="filename",
            type=str,
            help="The source GDAL raster datasource name.",
        )

        return parser

    def augment_kwargs(self, kwargs) -> dict:
        return kwargs

    def doit(self, **kwargs):
        return gdalminmaxlocation_util(**kwargs)


def main(argv=sys.argv):
    gdal.UseExceptions()
    return GDALMinMaxLocation().main(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
