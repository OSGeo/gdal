# !/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  GDAL utils
# Purpose:  Query information about a pixel given its location
#           A direct port of apps/gdallocationinfo.cpp
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even@spatialys.com>
# Copyright (c) 2021, Idan Miara <idan@miara.com>
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
import textwrap

from enum import Enum, auto
from numbers import Real
from typing import Union, Sequence, Optional, Tuple

import numpy as np
from osgeo import gdalconst, osr, gdal
from osgeo.gdal_array import BandRasterIONumPy

from osgeo_utils.auxiliary.base import is_path_like
from osgeo_utils.auxiliary.array_util import ArrayLike, ArrayOrScalarLike
from osgeo_utils.auxiliary.numpy_util import GDALTypeCodeAndNumericTypeCodeFromDataSet
from osgeo_utils.auxiliary.osr_util import transform_points, AnySRS, get_transform, get_srs, \
    get_axis_order_from_gis_order, OAMS_AXIS_ORDER
from osgeo_utils.auxiliary.util import PathOrDS, open_ds, get_bands, get_scales_and_offsets, get_band_nums
from osgeo_utils.auxiliary.gdal_argparse import GDALArgumentParser, GDALScript


class LocationInfoSRS(Enum):
    PixelLine = auto()
    SameAsDS_SRS = auto()
    SameAsDS_SRS_GeogCS = auto()


class LocationInfoOutput(Enum):
    PixelLineVal = auto()
    PixelLineValVerbose = auto()
    ValOnly = auto()
    XML = auto()
    LifOnly = auto()
    Quiet = auto()


CoordinateTransformationOrSRS = Optional[
    Union[osr.CoordinateTransformation, LocationInfoSRS, AnySRS]]


def gdallocationinfo(filename_or_ds: PathOrDS,
                     x: ArrayOrScalarLike, y: ArrayOrScalarLike,
                     srs: CoordinateTransformationOrSRS = None,
                     axis_order: Optional[OAMS_AXIS_ORDER] = None,
                     open_options: Optional[dict] = None,
                     ovr_idx: Optional[int] = None,
                     band_nums: Optional[Sequence[int]] = None,
                     inline_xy_replacement: bool = False,
                     return_ovr_pixel_line: bool = False,
                     transform_round_digits: Optional[float] = None,
                     allow_xy_outside_extent: bool = True,
                     pixel_offset: Real = -0.5, line_offset: Real = -0.5,
                     resample_alg=gdalconst.GRIORA_NearestNeighbour,
                     quiet_mode: bool = True,
                     ) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    ds = open_ds(filename_or_ds, open_options=open_options)
    filename = filename_or_ds if is_path_like(filename_or_ds) else ''
    if ds is None:
        raise Exception(f'Could not open {filename}.')
    if not isinstance(x, ArrayLike.__args__):
        x = [x]
    if not isinstance(y, ArrayLike.__args__):
        y = [y]
    if len(x) != len(y):
        raise Exception(f'len(x)={len(x)} should be the same as len(y)={len(y)}')
    point_count = len(x)

    dtype = np.float64
    if not isinstance(x, np.ndarray) or not isinstance(y, np.ndarray):
        x = np.array(x, dtype=dtype)
        y = np.array(y, dtype=dtype)
        inline_xy_replacement = True

    if srs is None:
        srs = LocationInfoSRS.PixelLine

    # Build Spatial Reference object based on coordinate system, fetched from the opened dataset
    if srs != LocationInfoSRS.PixelLine:
        if srs != LocationInfoSRS.SameAsDS_SRS:
            ds_srs = ds.GetSpatialRef()
            ct = None
            if isinstance(srs, osr.CoordinateTransformation):
                ct = srs
            else:
                if srs == LocationInfoSRS.SameAsDS_SRS_GeogCS:
                    points_srs = ds_srs.CloneGeogCS()
                else:
                    points_srs = get_srs(srs, axis_order=axis_order)
                ct = get_transform(points_srs, ds_srs)
            if ct is not None:
                if not inline_xy_replacement:
                    x = x.copy()
                    y = y.copy()
                    inline_xy_replacement = True
                transform_points(ct, x, y)
                if transform_round_digits is not None:
                    x.round(transform_round_digits, out=x)
                    y.round(transform_round_digits, out=y)

        # Read geotransform matrix and calculate corresponding pixel coordinates
        geotransform = ds.GetGeoTransform()
        inv_geotransform = gdal.InvGeoTransform(geotransform)
        if inv_geotransform is None:
            raise Exception("Failed InvGeoTransform()")

        # can we inline this transformation ?
        x, y = \
            (inv_geotransform[0] + inv_geotransform[1] * x + inv_geotransform[2] * y), \
            (inv_geotransform[3] + inv_geotransform[4] * x + inv_geotransform[5] * y)
        inline_xy_replacement = True

    xsize, ysize = ds.RasterXSize, ds.RasterYSize
    bands = get_bands(ds, band_nums, ovr_idx=ovr_idx)
    ovr_xsize, ovr_ysize = bands[0].XSize, bands[0].YSize
    pixel_fact, line_fact = (ovr_xsize / xsize, ovr_ysize / ysize) if ovr_idx else (1, 1)
    bnd_count = len(bands)

    shape = (bnd_count, point_count)
    np_dtype, np_dtype = GDALTypeCodeAndNumericTypeCodeFromDataSet(ds)
    results = np.empty(shape=shape, dtype=np_dtype)

    check_outside = not quiet_mode or not allow_xy_outside_extent
    if check_outside and (np.any(x < 0) or np.any(x >= xsize) or np.any(y < 0) or np.any(y >= ysize)):
        msg = 'Passed coordinates are not in dataset extent!'
        if not allow_xy_outside_extent:
            raise Exception(msg)
        elif not quiet_mode:
            print(msg)

    if pixel_fact == 1:
        pixels_q = x
    elif return_ovr_pixel_line:
        x *= pixel_fact
        pixels_q = x
    else:
        pixels_q = x * pixel_fact

    if line_fact == 1:
        lines_q = y
    elif return_ovr_pixel_line:
        y *= line_fact
        lines_q = y
    else:
        lines_q = y * line_fact

    buf_xsize = buf_ysize = 1
    buf_type, typecode = GDALTypeCodeAndNumericTypeCodeFromDataSet(ds)
    buf_obj = np.empty([buf_ysize, buf_xsize], dtype=typecode)

    for idx, (pixel, line) in enumerate(zip(pixels_q, lines_q)):
        for bnd_idx, band in enumerate(bands):
            if BandRasterIONumPy(band, 0, pixel-0.5, line-0.5, 1, 1, buf_obj, buf_type, resample_alg, None, None) == 0:
                results[bnd_idx][idx] = buf_obj[0][0]

    is_scaled, scales, offsets = get_scales_and_offsets(bands)
    if is_scaled:
        for bnd_idx, (scale, offset) in enumerate(zip(scales, offsets)):
            results[bnd_idx] = results[bnd_idx] * scale + offset

    return x, y, results


def gdallocationinfo_util(filename_or_ds: PathOrDS,
                          x: ArrayOrScalarLike, y: ArrayOrScalarLike,
                          open_options: Optional[dict] = None,
                          band_nums: Optional[Sequence[int]] = None,
                          resample_alg=gdalconst.GRIORA_NearestNeighbour,
                          output_mode: Optional[LocationInfoOutput] = None, **kwargs):
    if output_mode is None:
        output_mode = LocationInfoOutput.Quiet
    if output_mode in [LocationInfoOutput.XML, LocationInfoOutput.LifOnly]:
        raise Exception(f'Sorry, output mode {output_mode} is not implemented yet. you may use the c++ version.')

    quiet_mode = output_mode == LocationInfoOutput.Quiet
    print_mode = output_mode in \
                 [LocationInfoOutput.ValOnly, LocationInfoOutput.PixelLineVal, LocationInfoOutput.PixelLineValVerbose]
    inline_xy_replacement = not print_mode

    ds = open_ds(filename_or_ds, open_options=open_options)
    band_nums = get_band_nums(ds, band_nums)
    x, y, results = gdallocationinfo(
        filename_or_ds=ds, x=x, y=y,
        band_nums=band_nums, resample_alg=resample_alg,
        inline_xy_replacement=inline_xy_replacement, quiet_mode=quiet_mode, **kwargs)
    xsize, ysize = ds.RasterXSize, ds.RasterYSize

    is_nearest_neighbour = resample_alg == gdal.GRIORA_NearestNeighbour
    if print_mode:
        if output_mode == LocationInfoOutput.PixelLineValVerbose:
            print('Report:')
        for idx, (pixel, line) in enumerate(zip(x, y)):
            if not quiet_mode and pixel < 0 or pixel >= xsize or line < 0 or line >= ysize:
                print(f'Location {pixel} {line} is off this file!')
            else:
                if is_nearest_neighbour:
                    pixel, line = int(pixel), int(line)
                if output_mode == LocationInfoOutput.PixelLineValVerbose:
                    print(f'  Location: ({pixel}P,{line}L)')
                for bnd_idx, band_num in enumerate(band_nums):
                    val = results[bnd_idx][idx]
                    if output_mode == LocationInfoOutput.ValOnly:
                        print(val)
                    elif output_mode == LocationInfoOutput.PixelLineVal:
                        print(f'{pixel} {line} {val}')
                    elif output_mode == LocationInfoOutput.PixelLineValVerbose:
                        print(f'  Band {band_num}:')
                        print(f'    Value: {val}')

    return results


def val_at_coord(filename: str,
                 longitude: Real, latitude: Real, coordtype_georef: bool,
                 print_xy: bool, print_values: bool):
    """
    val_at_coord is a simplified version of gdallocationinfo. It accepts a single point and has less options.
    """

    ds = gdal.Open(filename, gdal.GA_ReadOnly)
    if ds is None:
        raise Exception('Cannot open %s' % filename)

    # Build Spatial Reference object based on coordinate system, fetched from the opened dataset
    if coordtype_georef:
        X = longitude
        Y = latitude
    else:
        srs = osr.SpatialReference()
        srs.ImportFromWkt(ds.GetProjection())

        srsLatLong = srs.CloneGeogCS()
        # Convert from (longitude,latitude) to projected coordinates
        ct = osr.CoordinateTransformation(srsLatLong, srs)
        (X, Y, height) = ct.TransformPoint(longitude, latitude)

    # Read geotransform matrix and calculate corresponding pixel coordinates
    geotransform = ds.GetGeoTransform()
    (success, inv_geotransform) = gdal.InvGeoTransform(geotransform)
    x = int(inv_geotransform[0] + inv_geotransform[1] * X + inv_geotransform[2] * Y)
    y = int(inv_geotransform[3] + inv_geotransform[4] * X + inv_geotransform[5] * Y)

    if print_xy:
        print('x=%d, y=%d' % (x, y))

    if x < 0 or x >= ds.RasterXSize or y < 0 or y >= ds.RasterYSize:
        raise Exception('Passed coordinates are not in dataset extent')

    res = ds.ReadAsArray(x, y, 1, 1)
    if print_values:
        if len(res.shape) == 2:
            print(res[0][0])
        else:
            for val in res:
                print(val[0][0])

    return res


class GDALLocationInfo(GDALScript):
    def __init__(self):
        super().__init__()
        self.title = 'Raster query tool'
        self.description = textwrap.dedent('''\
            The gdallocationinfo utility provide a mechanism to query information about a pixel given its location
            in one of a variety of coordinate systems. Several reporting options are provided.''')
        self.interactive_mode = None

    def get_parser(self, argv) -> GDALArgumentParser:
        parser = self.parser

        group = parser.add_mutually_exclusive_group()
        group.add_argument("-xml", dest="xml", action="store_true",
                           help="The output report will be XML formatted for convenient post processing.")
        group.add_argument("-lifonly", dest="lifonly", action="store_true",
                           help="The only output is filenames production from the LocationInfo request against "
                                "the database (i.e. for identifying impacted file from VRT).")
        group.add_argument("-valonly", dest="valonly", action="store_true",
                           help="The only output is the pixel values of the selected pixel on each of the selected bands.")
        parser.add_argument("-plb", dest="plb", action="store_true",
                            help="The output will be a series of lines in the form of Pixel,Line,band0,band1... "
                                 "for each of the selected bands.")
        group.add_argument("-quiet", dest="quiet", action="store_true",
                           help="No output will be printed.")

        group = parser.add_mutually_exclusive_group()
        group.add_argument("-l_srs", dest="srs", metavar='srs_def', type=str,
                           help="The coordinate system of the input x, y location.")
        group.add_argument("-geoloc", dest="geoloc", action="store_true",
                           help="Indicates input x,y points are in the georeferencing system of the image.")
        group.add_argument("-llgeoloc", dest="llgeoloc", action="store_true",
                           help="Indicates input x,y points are in the long, lat (geographic) "
                                "based on the georeferencing system of the image.")
        group.add_argument("-wgs84", dest="wgs84", action="store_true",
                           help="Indicates input x,y points are WGS84 long, lat.")

        parser.add_argument("-extent_strict", dest="allow_xy_outside_extent", action="store_false",
                            help="If set, input points outside the raster extent will raise an exception, "
                                 "otherwise a warning will be issued (unless quiet).")

        parser.add_argument("-interp", dest="resample_alg", action="store_true",
                            help="If set, a Bilinear interpolation would be used, otherwise the NearestNeighbour sampling.")

        parser.add_argument("-b", dest="band_nums", metavar="band", type=int, nargs='+',
                            help="Selects a band to query. Multiple bands can be listed. By default all bands are queried.")

        parser.add_argument('-overview', dest="ovr_idx", metavar="overview_level", type=int,
                            help="Query the (overview_level)th overview (overview_level=1 is the 1st overview), "
                                 "instead of the base band. Note that the x,y location (if the coordinate system is "
                                 "pixel/line) must still be given with respect to the base band.")

        parser.add_argument("-axis_order", dest="axis_order", choices=['gis', 'authority'], type=str, default=None,
                            help="X, Y Axis order: Traditional GIS, Authority complaint or otherwise utility default.")

        parser.add_argument("-oo", dest="open_options", metavar="NAME=VALUE",
                            help="Dataset open option (format specific).",
                            nargs='+')

        parser.add_argument("filename_or_ds", metavar="filename", type=str,
                            help="The source GDAL raster datasource name.")

        parser.add_argument("xy", metavar="x y", nargs='*', type=float,
                            help="series of X Y pairs of location of target pixel. "
                                 "By default the coordinate system "
                                 "is pixel/line unless -l_srs, -wgs84 or -geoloc supplied.")

        return parser

    def augment_kwargs(self, kwargs) -> dict:
        self.interactive_mode = len(kwargs['xy']) <= 1
        if not self.interactive_mode:
            kwargs['x'] = np.array(kwargs['xy'][0::2])
            kwargs['y'] = np.array(kwargs['xy'][1::2])

        if kwargs['geoloc']:
            kwargs['srs'] = LocationInfoSRS.SameAsDS_SRS
        elif kwargs['llgeoloc']:
            kwargs['srs'] = LocationInfoSRS.SameAsDS_SRS_GeogCS
        elif kwargs['wgs84']:
            kwargs['srs'] = 4326

        axis_order = kwargs['axis_order']
        if isinstance(axis_order, OAMS_AXIS_ORDER):
            pass
        else:
            if isinstance(axis_order, str):
                gis_order = axis_order.lower() == 'gis'
            elif axis_order is None:
                gis_order = kwargs['srs'] is not None and (kwargs['srs'] != LocationInfoSRS.SameAsDS_SRS)
            elif isinstance(axis_order, bool):
                gis_order = axis_order
            else:
                raise Exception(f'Unknown axis order {axis_order}')
            axis_order = get_axis_order_from_gis_order(gis_order)
        kwargs['axis_order'] = axis_order

        if kwargs['xml']:
            kwargs['output_mode'] = LocationInfoOutput.XML
        elif kwargs['lifonly']:
            kwargs['output_mode'] = LocationInfoOutput.LifOnly
        elif kwargs['valonly']:
            kwargs['output_mode'] = LocationInfoOutput.ValOnly
        elif kwargs['plb']:
            kwargs['output_mode'] = LocationInfoOutput.PixelLineVal
        elif kwargs['quiet']:
            kwargs['output_mode'] = LocationInfoOutput.Quiet
        else:
            kwargs['output_mode'] = LocationInfoOutput.PixelLineValVerbose

        kwargs['resample_alg'] = gdal.GRIORA_Bilinear if kwargs['resample_alg'] else gdal.GRIORA_NearestNeighbour

        del kwargs["xy"]

        del kwargs["geoloc"]
        del kwargs["llgeoloc"]
        del kwargs["wgs84"]

        del kwargs["xml"]
        del kwargs["lifonly"]
        del kwargs["valonly"]
        del kwargs["plb"]
        del kwargs["quiet"]

        return kwargs

    def doit(self, **kwargs):
        if self.interactive_mode:
            is_pixel_line = kwargs['srs'] == LocationInfoSRS.PixelLine
            while True:
                xy = input(f"Enter {'pixel line' if is_pixel_line else 'X Y'} "
                           f"values separated by space, and press Return.\n")
                xy = xy.strip().split(' ', 1)
                kwargs['x'], kwargs['y'] = float(xy[0]), float(xy[1])
                gdallocationinfo_util(**kwargs)
        else:
            return gdallocationinfo_util(**kwargs)


def main(argv=sys.argv):
    return GDALLocationInfo().main(argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
