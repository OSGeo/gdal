#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  gdal utility functions
#  Author:   Even Rouault <even.rouault at spatialys.com>
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
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
import os
from numbers import Real
from typing import Optional, Union, Sequence, Tuple, Dict, Any, Iterator, List

from osgeo import gdal
from osgeo_utils.auxiliary.base import get_extension, is_path_like, PathLikeOrStr, enum_to_str, OptionalBoolStr, \
    is_true, \
    MaybeSequence, T

PathOrDS = Union[PathLikeOrStr, gdal.Dataset]
DataTypeOrStr = Union[str, int]
CreationOptions = Optional[Dict[str, Any]]


def DoesDriverHandleExtension(drv: gdal.Driver, ext: str) -> bool:
    exts = drv.GetMetadataItem(gdal.DMD_EXTENSIONS)
    return exts is not None and exts.lower().find(ext.lower()) >= 0


def GetOutputDriversFor(filename: PathLikeOrStr, is_raster=True) -> List[str]:
    drv_list = []
    ext = get_extension(filename)
    if ext.lower() == 'vrt':
        return ['VRT']
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        if (drv.GetMetadataItem(gdal.DCAP_CREATE) is not None or
            drv.GetMetadataItem(gdal.DCAP_CREATECOPY) is not None) and \
            drv.GetMetadataItem(gdal.DCAP_RASTER if is_raster else gdal.DCAP_VECTOR) is not None:
            if ext and DoesDriverHandleExtension(drv, ext):
                drv_list.append(drv.ShortName)
            else:
                prefix = drv.GetMetadataItem(gdal.DMD_CONNECTION_PREFIX)
                if prefix is not None and filename.lower().startswith(prefix.lower()):
                    drv_list.append(drv.ShortName)

    # GMT is registered before netCDF for opening reasons, but we want
    # netCDF to be used by default for output.
    if ext.lower() == 'nc' and not drv_list and \
        drv_list[0].upper() == 'GMT' and drv_list[1].upper() == 'NETCDF':
        drv_list = ['NETCDF', 'GMT']

    return drv_list


def GetOutputDriverFor(filename: PathLikeOrStr, is_raster=True, default_raster_format='GTiff',
                       default_vector_format='ESRI Shapefile') -> str:
    if not filename:
        return 'MEM'
    drv_list = GetOutputDriversFor(filename, is_raster)
    ext = get_extension(filename)
    if not drv_list:
        if not ext:
            return default_raster_format if is_raster else default_vector_format
        else:
            raise Exception("Cannot guess driver for %s" % filename)
    elif len(drv_list) > 1:
        print("Several drivers matching %s extension. Using %s" % (ext if ext else '', drv_list[0]))
    return drv_list[0]


def open_ds(filename_or_ds: MaybeSequence[PathOrDS], *args, **kwargs) -> MaybeSequence[gdal.Dataset]:
    if not isinstance(filename_or_ds, PathOrDS.__args__):
        return [open_ds(f) for f in filename_or_ds]
    ods = OpenDS(filename_or_ds, *args, **kwargs)
    return ods.__enter__()


def get_ovr_count(filename_or_ds: PathOrDS) -> int:
    with OpenDS(filename_or_ds) as ds:
        bnd = ds.GetRasterBand(1)
        return bnd.GetOverviewCount()


def get_pixel_size(filename_or_ds: PathOrDS) -> Tuple[Real, Real]:
    ds = open_ds(filename_or_ds)
    geo_transform = ds.GetGeoTransform(can_return_null=True)
    if geo_transform is not None:
        return geo_transform[1], geo_transform[5]
    else:
        return 1, 1


ListOfTupleTT_OrT = List[Union[T, Tuple[T, T]]]


def get_sizes_factors_resolutions(filename_or_ds: PathOrDS, dim: Optional[int] = 0) -> \
     Tuple[ListOfTupleTT_OrT[int], ListOfTupleTT_OrT[Real], ListOfTupleTT_OrT[Real]]:

    ds = open_ds(filename_or_ds)
    bnd = ds.GetRasterBand(1)
    ovr_count = bnd.GetOverviewCount()
    r0 = get_pixel_size(ds)
    s0 = ds.RasterXSize, ds.RasterYSize
    f0 = 1, 1
    sizes = [s0]
    factors = [f0]
    resolutions = [r0]
    for i_overview in range(ovr_count):
        h_overview = bnd.GetOverview(i_overview)
        if h_overview is not None:
            s = h_overview.XSize, h_overview.YSize
            f = s0[0] / s[0], s0[1] / s[1]
            r = r0[0] * f[0], r0[1] * f[1]
            sizes.append(s)
            factors.append(f)
            resolutions.append(r)
    if dim is not None:
        sizes = [x[dim] for x in sizes]
        factors = [x[dim] for x in factors]
        resolutions = [x[dim] for x in resolutions]
    return sizes, factors, resolutions


def get_best_ovr_by_resolutions(requested_res: Real, resolutions: Sequence[Real]):
    for ovr, res in enumerate(resolutions):
        if res > requested_res:
            return max(0, ovr-1)
    return len(resolutions)-1


def get_ovr_idx(filename_or_ds: PathOrDS,
                ovr_idx: Optional[int] = None,
                ovr_res: Optional[Union[int, float]] = None) -> int:
    """
    returns a non-negative ovr_idx, from given mutually exclusive ovr_idx (index) or ovr_res (resolution)
    ovr_idx == None and ovr_res == None => returns 0
    ovr_idx: int >= 0 => returns the given ovr_idx
    ovr_idx: int < 0 => -1 is the last overview; -2 is the one before the last and so on
    ovr_res: float|int => returns the best suitable overview for a given resolution
             meaning the ovr with the lowest resolution which is higher then the request
    ovr_idx: float = x => same as (ovr_idx=None, ovr_res=x)
    """
    if ovr_res is not None:
        if ovr_idx is not None:
            raise Exception(f'ovr_idx({ovr_idx}) and ovr_res({ovr_res}) are mutually exclusive both were set')
        ovr_idx = float(ovr_res)
    if ovr_idx is None:
        return 0
    if isinstance(ovr_idx, Sequence):
        ovr_idx = ovr_idx[0]  # in case resolution in both axis were given we'll consider only x resolution
    if isinstance(ovr_idx, int):
        if ovr_idx < 0:
            overview_count = get_ovr_count(filename_or_ds)
            ovr_idx = max(0, overview_count + 1 + ovr_idx)
    elif isinstance(ovr_idx, float):
        _sizes, _factors, resolutions = get_sizes_factors_resolutions(filename_or_ds)
        ovr_idx = get_best_ovr_by_resolutions(ovr_idx, resolutions)
    else:
        raise Exception(f'Got an unexpected overview: {ovr_idx}')
    return ovr_idx


class OpenDS:
    __slots__ = ['filename', 'ds', 'args', 'kwargs', 'own', 'silent_fail']

    def __init__(self, filename_or_ds: PathOrDS, silent_fail=False, *args, **kwargs):
        self.ds: Optional[gdal.Dataset] = None
        self.filename: Optional[PathLikeOrStr] = None
        if is_path_like(filename_or_ds):
            self.filename = os.fspath(filename_or_ds)
        else:
            self.ds = filename_or_ds
        self.args = args
        self.kwargs = kwargs
        self.own = False
        self.silent_fail = silent_fail

    def __enter__(self) -> gdal.Dataset:

        if self.ds is None:
            self.ds = self._open_ds(self.filename, *self.args, **self.kwargs)
            if self.ds is None and not self.silent_fail:
                raise IOError('could not open file "{}"'.format(self.filename))
            self.own = True
        return self.ds

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.own:
            self.ds = False

    @staticmethod
    def _open_ds(
        filename: PathLikeOrStr,
        access_mode=gdal.GA_ReadOnly,
        ovr_idx: Optional[Union[int, float]] = None,
        open_options: Optional[Union[Dict[str, str], Sequence[str]]] = None,
        logger=None,
    ):
        if not open_options:
            open_options = dict()
        elif isinstance(open_options, Sequence):
            open_options = {k: v for k, v in (s.split('=', 1) for s in open_options)}
        else:
            open_options = dict(open_options)
        ovr_idx = get_ovr_idx(filename, ovr_idx)
        if ovr_idx > 0:
            open_options["OVERVIEW_LEVEL"] = ovr_idx - 1  # gdal overview 0 is the first overview (after the base layer)
        if logger is not None:
            s = 'opening file: "{}"'.format(filename)
            if open_options:
                s = s + " with options: {}".format(str(open_options))
            logger.debug(s)
        open_options = ["{}={}".format(k, v) for k, v in open_options.items()]

        return gdal.OpenEx(str(filename), access_mode, open_options=open_options)


def get_data_type(data_type: Optional[DataTypeOrStr]):
    if data_type is None:
        return None
    if isinstance(data_type, str):
        return gdal.GetDataTypeByName(data_type)
    else:
        return data_type


def get_raster_bands(ds: gdal.Dataset) -> Iterator[gdal.Band]:
    return (ds.GetRasterBand(i + 1) for i in range(ds.RasterCount))


def get_band_types(filename_or_ds: PathOrDS):
    with OpenDS(filename_or_ds) as ds:
        return [band.DataType for band in get_raster_bands(ds)]


def get_band_minimum(band: gdal.Band):
    ret = band.GetMinimum()
    if ret is None:
        band.ComputeStatistics(0)
    return band.GetMinimum()


def get_raster_band(filename_or_ds: PathOrDS, bnd_index: int = 1, ovr_index: Optional[int] = None):
    with OpenDS(filename_or_ds) as ds:
        bnd = ds.GetRasterBand(bnd_index)
        if ovr_index is not None:
            bnd = bnd.GetOverview(ovr_index)
        return bnd


def get_raster_minimum(filename_or_ds: PathOrDS, bnd_index: Optional[int] = 1):
    with OpenDS(filename_or_ds) as ds:
        if bnd_index is None:
            return min(get_band_minimum(bnd) for bnd in get_raster_bands(ds))
        else:
            bnd = ds.GetRasterBand(bnd_index)
            return get_band_minimum(bnd)


def get_raster_min_max(filename_or_ds: PathOrDS, bnd_index: int = 1):
    with OpenDS(filename_or_ds) as ds:
        bnd = ds.GetRasterBand(bnd_index)
        bnd.ComputeStatistics(0)
        min_val = bnd.GetMinimum()
        max_val = bnd.GetMaximum()
        return min_val, max_val


def get_nodatavalue(filename_or_ds: PathOrDS):
    with OpenDS(filename_or_ds) as ds:
        band = next(get_raster_bands(ds))
        return band.GetNoDataValue()


def unset_nodatavalue(filename_or_ds: PathOrDS):
    with OpenDS(filename_or_ds, access_mode=gdal.GA_Update) as ds:
        for b in get_raster_bands(ds):
            b.DeleteNoDataValue()


def get_metadata_item(filename_or_ds: PathOrDS, key: str, domain: str, default: Any = None):
    key = key.strip()
    domain = domain.strip()
    with OpenDS(filename_or_ds) as ds:
        metadata_item = ds.GetMetadataItem(key, domain)
        return metadata_item if metadata_item is not None else default


def get_image_structure_metadata(filename_or_ds: PathOrDS, key: str, default: Any = None):
    return get_metadata_item(filename_or_ds, key=key, domain="IMAGE_STRUCTURE", default=default)


def get_bigtiff_creation_option_value(big_tiff: OptionalBoolStr):
    return "IF_SAFER" if big_tiff is None \
        else big_tiff if bool(big_tiff) and isinstance(big_tiff, str) \
        else str(is_true(big_tiff))


def get_ext_by_of(of: str):
    ext = enum_to_str(of).lower()
    if ext in ['gtiff', 'cog', 'mem']:
        ext = 'tif'
    return '.' + ext


def get_band_nums(ds: gdal.Dataset, band_nums: MaybeSequence[int] = None):
    if not band_nums:
        band_nums = list(range(1, ds.RasterCount + 1))
    elif isinstance(band_nums, int):
        band_nums = [band_nums]
    return band_nums


def get_bands(filename_or_ds: PathOrDS, band_nums: MaybeSequence[int], ovr_idx: Optional[int] = None) -> List[gdal.Band]:
    ds = open_ds(filename_or_ds)
    band_nums = get_band_nums(ds, band_nums)
    bands = []
    for band_num in band_nums:
        band: gdal.Band = ds.GetRasterBand(band_num)
        if band is None:
            raise Exception(f'Could not get band {band_num} from file {filename_or_ds}')
        if ovr_idx:
            band = band.GetOverview(ovr_idx-1)
            if band is None:
                raise Exception(f'Could not get overview {ovr_idx} from band {band_num} of file {filename_or_ds}')
        bands.append(band)
    return bands


def get_scales_and_offsets(bands: MaybeSequence[gdal.Band]) -> Tuple[bool, MaybeSequence[Real], MaybeSequence[Real]]:
    single_band = not isinstance(bands, Sequence)
    if single_band:
        bands = [bands]
    scales = [bnd.GetScale() or 1 for bnd in bands]
    offsets = [bnd.GetOffset() or 0 for bnd in bands]
    is_scaled = any(scale != 1 for scale in scales) or any(offset != 0 for offset in offsets)
    if single_band:
        scales, offsets = scales[0], offsets[0]
    return is_scaled, scales, offsets

