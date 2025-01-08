#!/usr/bin/env python3
###############################################################################
#
# Project:  InSAR Peppers
# Purpose:  Module to extract data from many rasters into one output.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2000, Atlantis Scientific Inc. (www.atlsci.com)
# Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################
# changes 29Apr2011
# If the input image is a multi-band one, use all the channels in
# building the stack.
# anssi.pekkarinen@fao.org

import math
import sys
import time

from osgeo import gdal
from osgeo_utils.auxiliary.util import GetOutputDriverFor

progress = gdal.TermProgress_nocb

__version__ = "$id$"[5:-1]


# =============================================================================
def raster_copy(
    s_fh,
    s_xoff,
    s_yoff,
    s_xsize,
    s_ysize,
    s_band_n,
    t_fh,
    t_xoff,
    t_yoff,
    t_xsize,
    t_ysize,
    t_band_n,
    nodata=None,
    verbose=0,
):

    if verbose != 0:
        print(
            "Copy %d,%d,%d,%d to %d,%d,%d,%d."
            % (s_xoff, s_yoff, s_xsize, s_ysize, t_xoff, t_yoff, t_xsize, t_ysize)
        )

    if nodata is not None:
        return raster_copy_with_nodata(
            s_fh,
            s_xoff,
            s_yoff,
            s_xsize,
            s_ysize,
            s_band_n,
            t_fh,
            t_xoff,
            t_yoff,
            t_xsize,
            t_ysize,
            t_band_n,
            nodata,
        )

    s_band = s_fh.GetRasterBand(s_band_n)
    m_band = None
    # Works only in binary mode and doesn't take into account
    # intermediate transparency values for compositing.
    if s_band.GetMaskFlags() != gdal.GMF_ALL_VALID:
        m_band = s_band.GetMaskBand()
    elif s_band.GetColorInterpretation() == gdal.GCI_AlphaBand:
        m_band = s_band
    if m_band is not None:
        return raster_copy_with_mask(
            s_fh,
            s_xoff,
            s_yoff,
            s_xsize,
            s_ysize,
            s_band_n,
            t_fh,
            t_xoff,
            t_yoff,
            t_xsize,
            t_ysize,
            t_band_n,
            m_band,
        )

    s_band = s_fh.GetRasterBand(s_band_n)
    t_band = t_fh.GetRasterBand(t_band_n)

    data = s_band.ReadRaster(
        s_xoff, s_yoff, s_xsize, s_ysize, t_xsize, t_ysize, t_band.DataType
    )
    t_band.WriteRaster(
        t_xoff, t_yoff, t_xsize, t_ysize, data, t_xsize, t_ysize, t_band.DataType
    )

    return 0


# =============================================================================


def raster_copy_with_nodata(
    s_fh,
    s_xoff,
    s_yoff,
    s_xsize,
    s_ysize,
    s_band_n,
    t_fh,
    t_xoff,
    t_yoff,
    t_xsize,
    t_ysize,
    t_band_n,
    nodata,
):
    import numpy as np

    s_band = s_fh.GetRasterBand(s_band_n)
    t_band = t_fh.GetRasterBand(t_band_n)

    data_src = s_band.ReadAsArray(s_xoff, s_yoff, s_xsize, s_ysize, t_xsize, t_ysize)
    data_dst = t_band.ReadAsArray(t_xoff, t_yoff, t_xsize, t_ysize)

    if not np.isnan(nodata):
        nodata_test = np.equal(data_src, nodata)
    else:
        nodata_test = np.isnan(data_src)

    to_write = np.choose(nodata_test, (data_src, data_dst))

    t_band.WriteArray(to_write, t_xoff, t_yoff)

    return 0


# =============================================================================


def raster_copy_with_mask(
    s_fh,
    s_xoff,
    s_yoff,
    s_xsize,
    s_ysize,
    s_band_n,
    t_fh,
    t_xoff,
    t_yoff,
    t_xsize,
    t_ysize,
    t_band_n,
    m_band,
):
    import numpy as np

    s_band = s_fh.GetRasterBand(s_band_n)
    t_band = t_fh.GetRasterBand(t_band_n)

    data_src = s_band.ReadAsArray(s_xoff, s_yoff, s_xsize, s_ysize, t_xsize, t_ysize)
    data_mask = m_band.ReadAsArray(s_xoff, s_yoff, s_xsize, s_ysize, t_xsize, t_ysize)
    data_dst = t_band.ReadAsArray(t_xoff, t_yoff, t_xsize, t_ysize)

    mask_test = np.equal(data_mask, 0)
    to_write = np.choose(mask_test, (data_src, data_dst))

    t_band.WriteArray(to_write, t_xoff, t_yoff)

    return 0


# =============================================================================


def names_to_fileinfos(names):
    """
    Translate a list of GDAL filenames, into file_info objects.

    names -- list of valid GDAL dataset names.

    Returns a list of file_info objects.  There may be less file_info objects
    than names if some of the names could not be opened as GDAL files.
    """

    file_infos = []
    for name in names:
        fi = file_info()
        if fi.init_from_name(name) == 1:
            file_infos.append(fi)

    return file_infos


# *****************************************************************************


class file_info:
    """A class holding information about a GDAL file."""

    def __init__(self):
        self.band_type = None
        self.bands = None
        self.ct = None
        self.filename = None
        self.geotransform = None
        self.lrx = None
        self.lry = None
        self.projection = None
        self.ulx = None
        self.uly = None
        self.xsize = None
        self.ysize = None

    def init_from_name(self, filename):
        """
        Initialize file_info from filename

        filename -- Name of file to read.

        Returns 1 on success or 0 if the file can't be opened.
        """
        fh = gdal.Open(filename)
        if fh is None:
            return 0

        self.filename = filename
        self.bands = fh.RasterCount
        self.xsize = fh.RasterXSize
        self.ysize = fh.RasterYSize
        self.band_type = fh.GetRasterBand(1).DataType
        self.projection = fh.GetProjection()
        self.geotransform = fh.GetGeoTransform()
        self.ulx = self.geotransform[0]
        self.uly = self.geotransform[3]
        self.lrx = self.ulx + self.geotransform[1] * self.xsize
        self.lry = self.uly + self.geotransform[5] * self.ysize

        ct = fh.GetRasterBand(1).GetRasterColorTable()
        if ct is not None:
            self.ct = ct.Clone()
        else:
            self.ct = None

        return 1

    def report(self):
        print("Filename: " + self.filename)
        print("File Size: %dx%dx%d" % (self.xsize, self.ysize, self.bands))
        print("Pixel Size: %f x %f" % (self.geotransform[1], self.geotransform[5]))
        print("UL:(%f,%f)   LR:(%f,%f)" % (self.ulx, self.uly, self.lrx, self.lry))

    def copy_into(self, t_fh, s_band=1, t_band=1, nodata_arg=None, verbose=0):
        """
        Copy this files image into target file.

        This method will compute the overlap area of the file_info objects
        file, and the target gdal.Dataset object, and copy the image data
        for the common window area.  It is assumed that the files are in
        a compatible projection ... no checking or warping is done.  However,
        if the destination file is a different resolution, or different
        image pixel type, the appropriate resampling and conversions will
        be done (using normal GDAL promotion/demotion rules).

        t_fh -- gdal.Dataset object for the file into which some or all
        of this file may be copied.

        Returns 1 on success (or if nothing needs to be copied), and zero one
        failure.
        """
        t_geotransform = t_fh.GetGeoTransform()
        t_ulx = t_geotransform[0]
        t_uly = t_geotransform[3]
        t_lrx = t_geotransform[0] + t_fh.RasterXSize * t_geotransform[1]
        t_lry = t_geotransform[3] + t_fh.RasterYSize * t_geotransform[5]

        # figure out intersection region
        tgw_ulx = max(t_ulx, self.ulx)
        tgw_lrx = min(t_lrx, self.lrx)
        if t_geotransform[5] < 0:
            tgw_uly = min(t_uly, self.uly)
            tgw_lry = max(t_lry, self.lry)
        else:
            tgw_uly = max(t_uly, self.uly)
            tgw_lry = min(t_lry, self.lry)

        # do they even intersect?
        if tgw_ulx >= tgw_lrx:
            return 1
        if t_geotransform[5] < 0 and tgw_uly <= tgw_lry:
            return 1
        if t_geotransform[5] > 0 and tgw_uly >= tgw_lry:
            return 1

        # compute target window in pixel coordinates.
        tw_xoff = int((tgw_ulx - t_geotransform[0]) / t_geotransform[1] + 0.1)
        tw_yoff = int((tgw_uly - t_geotransform[3]) / t_geotransform[5] + 0.1)
        tw_xsize = (
            int((tgw_lrx - t_geotransform[0]) / t_geotransform[1] + 0.5) - tw_xoff
        )
        tw_ysize = (
            int((tgw_lry - t_geotransform[3]) / t_geotransform[5] + 0.5) - tw_yoff
        )

        if tw_xsize < 1 or tw_ysize < 1:
            return 1

        # Compute source window in pixel coordinates.
        sw_xoff = int((tgw_ulx - self.geotransform[0]) / self.geotransform[1] + 0.1)
        sw_yoff = int((tgw_uly - self.geotransform[3]) / self.geotransform[5] + 0.1)
        sw_xsize = (
            int((tgw_lrx - self.geotransform[0]) / self.geotransform[1] + 0.5) - sw_xoff
        )
        sw_ysize = (
            int((tgw_lry - self.geotransform[3]) / self.geotransform[5] + 0.5) - sw_yoff
        )

        if sw_xsize < 1 or sw_ysize < 1:
            return 1

        # Open the source file, and copy the selected region.
        s_fh = gdal.Open(self.filename)

        return raster_copy(
            s_fh,
            sw_xoff,
            sw_yoff,
            sw_xsize,
            sw_ysize,
            s_band,
            t_fh,
            tw_xoff,
            tw_yoff,
            tw_xsize,
            tw_ysize,
            t_band,
            nodata_arg,
            verbose,
        )


# =============================================================================
def Usage(isError):
    f = sys.stderr if isError else sys.stdout
    print("Usage: gdal_merge [--help] [--help-general]", file=f)
    print(
        "                     [-o <out_filename>] [-of <out_format>] [-co <NAME>=<VALUE>]...",
        file=f,
    )
    print(
        "                     [-ps <pixelsize_x> <pixelsize_y>] [-tap] [-separate] [-q] [-v] [-pct]",
        file=f,
    )
    print(
        '                     [-ul_lr <ulx> <uly> <lrx> <lry>] [-init "<value>[ <value>]..."]',
        file=f,
    )
    print(
        "                     [-n <nodata_value>] [-a_nodata <output_nodata_value>]",
        file=f,
    )
    print(
        "                     [-ot <datatype>] [-createonly] <input_file> [<input_file>]...",
        file=f,
    )
    print("                     [--help-general]", file=f)
    print("", file=f)
    return 2 if isError else 0


def gdal_merge(argv=None):
    with gdal.ExceptionMgr():
        return _gdal_merge(argv=argv)


def _gdal_merge(argv=None):
    verbose = 0
    quiet = 0
    names = []
    driver_name = None
    out_file = "out.tif"

    ulx = None
    psize_x = None
    separate = 0
    copy_pct = 0
    nodata = None
    a_nodata = None
    create_options = []
    pre_init = []
    band_type = None
    createonly = 0
    bTargetAlignedPixels = False
    start_time = time.time()

    if argv is None:
        argv = argv
    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == "--help":
            return Usage(isError=False)

        elif arg == "-o":
            i = i + 1
            out_file = argv[i]

        elif arg == "-v":
            verbose = 1

        elif arg == "-q" or arg == "-quiet":
            quiet = 1

        elif arg == "-createonly":
            createonly = 1

        elif arg == "-separate":
            separate = 1

        elif arg == "-seperate":
            separate = 1

        elif arg == "-pct":
            copy_pct = 1

        elif arg == "-ot":
            i = i + 1
            band_type = gdal.GetDataTypeByName(argv[i])
            if band_type == gdal.GDT_Unknown:
                print("Unknown GDAL data type: %s" % argv[i])
                return 1

        elif arg == "-init":
            i = i + 1
            str_pre_init = argv[i].split()
            for x in str_pre_init:
                pre_init.append(float(x))

        elif arg == "-n":
            i = i + 1
            nodata = float(argv[i])

        elif arg == "-a_nodata":
            i = i + 1
            a_nodata = float(argv[i])

        elif arg == "-f" or arg == "-of":
            i = i + 1
            driver_name = argv[i]

        elif arg == "-co":
            i = i + 1
            create_options.append(argv[i])

        elif arg == "-ps":
            psize_x = float(argv[i + 1])
            psize_y = -1 * abs(float(argv[i + 2]))
            i = i + 2

        elif arg == "-tap":
            bTargetAlignedPixels = True

        elif arg == "-ul_lr":
            ulx = float(argv[i + 1])
            uly = float(argv[i + 2])
            lrx = float(argv[i + 3])
            lry = float(argv[i + 4])
            i = i + 4

        elif arg[:1] == "-":
            print("Unrecognized command option: %s" % arg)
            return Usage(isError=True)

        else:
            names.append(arg)

        i = i + 1

    if not names:
        print("No input files selected.")
        return Usage(isError=True)

    if driver_name is None:
        driver_name = GetOutputDriverFor(out_file)

    driver = gdal.GetDriverByName(driver_name)
    if driver is None:
        print("Format driver %s not found, pick a supported driver." % driver_name)
        return 1

    DriverMD = driver.GetMetadata()
    if "DCAP_CREATE" not in DriverMD:
        print(
            "Format driver %s does not support creation and piecewise writing.\nPlease select a format that does, such as GTiff (the default) or HFA (Erdas Imagine)."
            % driver_name
        )
        return 1

    # Collect information on all the source files.
    file_infos = names_to_fileinfos(names)

    if ulx is None:
        ulx = file_infos[0].ulx
        uly = file_infos[0].uly
        lrx = file_infos[0].lrx
        lry = file_infos[0].lry

        for fi in file_infos:
            ulx = min(ulx, fi.ulx)
            uly = max(uly, fi.uly)
            lrx = max(lrx, fi.lrx)
            lry = min(lry, fi.lry)

    if psize_x is None:
        psize_x = file_infos[0].geotransform[1]
        psize_y = file_infos[0].geotransform[5]

    if band_type is None:
        band_type = file_infos[0].band_type

    # Try opening as an existing file.
    try:
        t_fh = gdal.Open(out_file, gdal.GA_Update)
    except Exception:
        t_fh = None

    # Create output file if it does not already exist.
    if t_fh is None:

        if bTargetAlignedPixels:
            ulx = math.floor(ulx / psize_x) * psize_x
            lrx = math.ceil(lrx / psize_x) * psize_x
            lry = math.floor(lry / -psize_y) * -psize_y
            uly = math.ceil(uly / -psize_y) * -psize_y

        geotransform = [ulx, psize_x, 0, uly, 0, psize_y]

        xsize = int((lrx - ulx) / geotransform[1] + 0.5)
        ysize = int((lry - uly) / geotransform[5] + 0.5)

        if separate != 0:
            bands = 0

            for fi in file_infos:
                bands = bands + fi.bands
        else:
            bands = file_infos[0].bands

        t_fh = driver.Create(out_file, xsize, ysize, bands, band_type, create_options)
        if t_fh is None:
            print("Creation failed, terminating gdal_merge.")
            return 1

        t_fh.SetGeoTransform(geotransform)
        t_fh.SetProjection(file_infos[0].projection)

        if copy_pct:
            t_fh.GetRasterBand(1).SetRasterColorTable(file_infos[0].ct)
    else:
        if separate != 0:
            bands = 0
            for fi in file_infos:
                bands = bands + fi.bands
            if t_fh.RasterCount < bands:
                print(
                    "Existing output file has less bands than the input files. You should delete it before. Terminating gdal_merge."
                )
                return 1
        else:
            bands = min(file_infos[0].bands, t_fh.RasterCount)

    # Do we need to set nodata value ?
    if a_nodata is not None:
        for i in range(t_fh.RasterCount):
            t_fh.GetRasterBand(i + 1).SetNoDataValue(a_nodata)

    # Do we need to pre-initialize the whole mosaic file to some value?
    if pre_init is not None:
        if t_fh.RasterCount <= len(pre_init):
            for i in range(t_fh.RasterCount):
                t_fh.GetRasterBand(i + 1).Fill(pre_init[i])
        elif len(pre_init) == 1:
            for i in range(t_fh.RasterCount):
                t_fh.GetRasterBand(i + 1).Fill(pre_init[0])

    # Copy data from source files into output file.
    t_band = 1

    if quiet == 0 and verbose == 0:
        progress(0.0)
    fi_processed = 0

    for fi in file_infos:
        if createonly != 0:
            continue

        if verbose != 0:
            print("")
            print(
                "Processing file %5d of %5d, %6.3f%% completed in %d minutes."
                % (
                    fi_processed + 1,
                    len(file_infos),
                    fi_processed * 100.0 / len(file_infos),
                    int(round((time.time() - start_time) / 60.0)),
                )
            )
            fi.report()

        if separate == 0:
            for band in range(1, bands + 1):
                fi.copy_into(t_fh, band, band, nodata, verbose)
        else:
            for band in range(1, fi.bands + 1):
                fi.copy_into(t_fh, band, t_band, nodata, verbose)
                t_band = t_band + 1

        fi_processed = fi_processed + 1
        if quiet == 0 and verbose == 0:
            progress(fi_processed / float(len(file_infos)))

    # Force file to be closed.
    t_fh = None


def main(argv=sys.argv):
    return gdal_merge(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
