#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
#
#  Project:  GDAL samples
#  Purpose:  Edit in place various information of an existing GDAL dataset
#  Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

from osgeo import gdal, osr
from osgeo_utils.auxiliary.util import enable_gdal_exceptions


def Usage(isError):
    f = sys.stderr if isError else sys.stdout
    print("Usage: gdal_edit [--help] [--help-general] [-ro] [-a_srs <srs_def>]", file=f)
    print(
        "                 [-a_ullr <ulx> <uly> <lrx> <lry>] [-a_ulurll <ulx> <uly> <urx> <ury> <llx> <lly>]",
        file=f,
    )
    print(
        "                 [-tr <xres> <yres>] [-unsetgt] [-unsetrpc] [-a_nodata <value>] [-unsetnodata]",
        file=f,
    )
    print(
        "                 [-offset <value>] [-scale <value>] [-units <value>]", file=f
    )
    print(
        "                 [-colorinterp_<X> {red|green|blue|alpha|gray|undefined|pan|coastal|rededge|nir|swir|mwir|lwir|...]]...",
        file=f,
    )
    print("                 [-a_coord_epoch <epoch>] [-unsetepoch]", file=f)
    print("                 [-unsetstats] [-stats] [-approx_stats]", file=f)
    print("                 [-setstats <min> <max> <mean> <stddev>]", file=f)
    print(
        "                 [-gcp <pixel> <line> <easting> <northing> [<elevation>]]...",
        file=f,
    )
    print(
        "                 [-unsetmd] [-oo <NAME>=<VALUE>]... [-mo <META-TAG>=<VALUE>]...",
        file=f,
    )
    print("                 <dataset_name>", file=f)
    print("", file=f)
    print("Edit in place various information of an existing GDAL dataset.", file=f)
    return 2 if isError else 0


def ArgIsNumeric(s):
    i = 0

    while i < len(s):
        if (
            (s[i] < "0" or s[i] > "9")
            and s[i] != "."
            and s[i] != "e"
            and s[i] != "+"
            and s[i] != "-"
        ):
            return False
        i = i + 1

    return True


@enable_gdal_exceptions
def gdal_edit(argv):

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    datasetname = None
    srs = None
    ulx = None
    uly = None
    urx = None
    ury = None
    llx = None
    lly = None
    lrx = None
    lry = None
    nodata = None
    unsetnodata = False
    units = None
    xres = None
    yres = None
    unsetgt = False
    epoch = None
    unsetepoch = False
    unsetstats = False
    stats = False
    setstats = False
    approx_stats = False
    unsetmd = False
    ro = False
    molist = []
    gcp_list = []
    open_options = []
    offset = []
    scale = []
    colorinterp = {}
    unsetrpc = False

    i = 1
    argc = len(argv)
    while i < argc:
        if argv[i] == "--help":
            return Usage(isError=False)
        elif argv[i] == "-ro":
            ro = True
        elif argv[i] == "-a_srs" and i < len(argv) - 1:
            srs = argv[i + 1]
            i = i + 1
        elif argv[i] == "-a_coord_epoch" and i < len(argv) - 1:
            epoch = float(argv[i + 1])
            i = i + 1
        elif argv[i] == "-a_ullr" and i < len(argv) - 4:
            ulx = float(argv[i + 1])
            i = i + 1
            uly = float(argv[i + 1])
            i = i + 1
            lrx = float(argv[i + 1])
            i = i + 1
            lry = float(argv[i + 1])
            i = i + 1
        elif argv[i] == "-a_ulurll" and i < len(argv) - 6:
            ulx = float(argv[i + 1])
            i = i + 1
            uly = float(argv[i + 1])
            i = i + 1
            urx = float(argv[i + 1])
            i = i + 1
            ury = float(argv[i + 1])
            i = i + 1
            llx = float(argv[i + 1])
            i = i + 1
            lly = float(argv[i + 1])
            i = i + 1
        elif argv[i] == "-tr" and i < len(argv) - 2:
            xres = float(argv[i + 1])
            i = i + 1
            yres = float(argv[i + 1])
            i = i + 1
        elif argv[i] == "-a_nodata" and i < len(argv) - 1:
            try:
                nodata = int(argv[i + 1])
            except Exception:
                nodata = float(argv[i + 1])
            i = i + 1
        elif argv[i] == "-scale" and i < len(argv) - 1:
            scale.append(float(argv[i + 1]))
            i = i + 1
            while i < len(argv) - 1 and ArgIsNumeric(argv[i + 1]):
                scale.append(float(argv[i + 1]))
                i = i + 1
        elif argv[i] == "-offset" and i < len(argv) - 1:
            offset.append(float(argv[i + 1]))
            i = i + 1
            while i < len(argv) - 1 and ArgIsNumeric(argv[i + 1]):
                offset.append(float(argv[i + 1]))
                i = i + 1
        elif argv[i] == "-mo" and i < len(argv) - 1:
            molist.append(argv[i + 1])
            i = i + 1
        elif argv[i] == "-gcp" and i + 4 < len(argv):
            pixel = float(argv[i + 1])
            i = i + 1
            line = float(argv[i + 1])
            i = i + 1
            x = float(argv[i + 1])
            i = i + 1
            y = float(argv[i + 1])
            i = i + 1
            if i + 1 < len(argv) and ArgIsNumeric(argv[i + 1]):
                z = float(argv[i + 1])
                i = i + 1
            else:
                z = 0
            gcp = gdal.GCP(x, y, z, pixel, line)
            gcp_list.append(gcp)
        elif argv[i] == "-unsetgt":
            unsetgt = True
        elif argv[i] == "-unsetrpc":
            unsetrpc = True
        elif argv[i] == "-unsetepoch":
            unsetepoch = True
        elif argv[i] == "-unsetstats":
            unsetstats = True
        elif argv[i] == "-approx_stats":
            stats = True
            approx_stats = True
        elif argv[i] == "-stats":
            stats = True
        elif argv[i] == "-setstats" and i < len(argv) - 4:
            stats = True
            setstats = True
            if argv[i + 1] != "None":
                statsmin = float(argv[i + 1])
            else:
                statsmin = None
            i = i + 1
            if argv[i + 1] != "None":
                statsmax = float(argv[i + 1])
            else:
                statsmax = None
            i = i + 1
            if argv[i + 1] != "None":
                statsmean = float(argv[i + 1])
            else:
                statsmean = None
            i = i + 1
            if argv[i + 1] != "None":
                statsdev = float(argv[i + 1])
            else:
                statsdev = None
            i = i + 1
        elif argv[i] == "-units" and i < len(argv) - 1:
            units = argv[i + 1]
            i = i + 1
        elif argv[i] == "-unsetmd":
            unsetmd = True
        elif argv[i] == "-unsetnodata":
            unsetnodata = True
        elif argv[i] == "-oo" and i < len(argv) - 1:
            open_options.append(argv[i + 1])
            i = i + 1
        elif argv[i].startswith("-colorinterp_") and i < len(argv) - 1:
            band = int(argv[i][len("-colorinterp_") :])
            val_str = argv[i + 1]
            if val_str.lower() == "undefined":
                val = gdal.GCI_Undefined
            else:
                val = gdal.GetColorInterpretationByName(val_str)
                if val == gdal.GCI_Undefined:
                    print(
                        "Unsupported color interpretation %s.\n" % val_str,
                        file=sys.stderr,
                    )
                    return Usage(isError=True)
            colorinterp[band] = val
            i = i + 1
        elif argv[i][0] == "-":
            print("Unrecognized option : %s\n" % argv[i], file=sys.stderr)
            return Usage(isError=True)
        elif datasetname is None:
            datasetname = argv[i]
        else:
            print("Unexpected option : %s\n" % argv[i], file=sys.stderr)
            return Usage(isError=True)

        i = i + 1

    if datasetname is None:
        return Usage(isError=True)

    if (
        srs is None
        and epoch is None
        and lry is None
        and yres is None
        and not unsetgt
        and not unsetepoch
        and not unsetstats
        and not stats
        and not setstats
        and nodata is None
        and not units
        and not molist
        and not unsetmd
        and not gcp_list
        and not unsetnodata
        and not colorinterp
        and scale is None
        and offset is None
        and not unsetrpc
    ):
        print("No option specified", file=sys.stderr)
        print("", file=sys.stderr)
        return Usage(isError=True)

    exclusive_option = 0
    if lry is not None:
        exclusive_option = exclusive_option + 1
    if lly is not None:  # -a_ulurll
        exclusive_option = exclusive_option + 1
    if yres is not None:
        exclusive_option = exclusive_option + 1
    if unsetgt:
        exclusive_option = exclusive_option + 1
    if exclusive_option > 1:
        print(
            "-a_ullr, -a_ulurll, -tr and -unsetgt options are exclusive.",
            file=sys.stderr,
        )
        print("", file=sys.stderr)
        return Usage(isError=True)

    if unsetstats and stats:
        print(
            "-unsetstats and either -stats or -approx_stats options are exclusive.",
            file=sys.stderr,
        )
        print("", file=sys.stderr)
        return Usage(isError=True)

    if unsetnodata and nodata:
        print("-unsetnodata and -nodata options are exclusive.", file=sys.stderr)
        print("", file=sys.stderr)
        return Usage(isError=True)

    if unsetepoch and epoch:
        print("-unsetepoch and -a_coord_epoch options are exclusive.", file=sys.stderr)
        print("", file=sys.stderr)
        return Usage(isError=True)

    try:
        if ro:
            ds = gdal.OpenEx(datasetname, gdal.OF_RASTER, open_options=open_options)
        else:
            ds = gdal.OpenEx(
                datasetname, gdal.OF_RASTER | gdal.OF_UPDATE, open_options=open_options
            )
    except Exception as e:
        print(str(e), file=sys.stderr)
        return -1

    if scale:
        if len(scale) == 1:
            scale = scale * ds.RasterCount
        elif len(scale) != ds.RasterCount:
            print(
                "If more than one scale value is provided, their number must match the number of bands.",
                file=sys.stderr,
            )
            print("", file=sys.stderr)
            return Usage(isError=True)

    if offset:
        if len(offset) == 1:
            offset = offset * ds.RasterCount
        elif len(offset) != ds.RasterCount:
            print(
                "If more than one offset value is provided, their number must match the number of bands.",
                file=sys.stderr,
            )
            print("", file=sys.stderr)
            return Usage(isError=True)

    wkt = None
    if srs == "" or srs == "None":
        ds.SetProjection("")
    elif srs is not None:
        sr = osr.SpatialReference()
        if sr.SetFromUserInput(srs) != 0:
            print("Failed to process SRS definition: %s" % srs, file=sys.stderr)
            return -1
        wkt = sr.ExportToWkt()
        if not gcp_list:
            ds.SetProjection(wkt)

    if epoch is not None:
        sr = ds.GetSpatialRef()
        if sr is None:
            print(
                "Dataset SRS is undefined, cannot set epoch. See the -a_srs option.",
                file=sys.stderr,
            )
            return -1
        sr.SetCoordinateEpoch(epoch)
        ds.SetSpatialRef(sr)

    if unsetepoch:
        sr = ds.GetSpatialRef()
        if sr is None:
            print("Dataset SRS is undefined, with no epoch specified.", file=sys.stderr)
            return -1
        # Set to 0.0, which is what GetCoordinateEpoch() returns for SRS with no epoch defined
        sr.SetCoordinateEpoch(0.0)
        ds.SetSpatialRef(sr)

    if lry is not None:
        gt = [
            ulx,
            (lrx - ulx) / ds.RasterXSize,
            0,
            uly,
            0,
            (lry - uly) / ds.RasterYSize,
        ]
        ds.SetGeoTransform(gt)

    elif lly is not None:  # -a_ulurll
        gt = [
            ulx,
            (urx - ulx) / ds.RasterXSize,
            (llx - ulx) / ds.RasterYSize,
            uly,
            (ury - uly) / ds.RasterXSize,
            (lly - uly) / ds.RasterYSize,
        ]
        ds.SetGeoTransform(gt)

    if yres is not None:
        gt = ds.GetGeoTransform()
        # Doh ! why is gt a tuple and not an array...
        gt = [gt[j] for j in range(6)]
        gt[1] = xres
        gt[5] = yres
        ds.SetGeoTransform(gt)

    if unsetgt:
        # For now only the GTiff drivers understands full-zero as a hint
        # to unset the geotransform
        if ds.GetDriver().ShortName == "GTiff":
            ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
        else:
            ds.SetGeoTransform([0, 1, 0, 0, 0, 1])

    if gcp_list:
        if wkt is None:
            wkt = ds.GetGCPProjection()
        if wkt is None:
            wkt = ""
        ds.SetGCPs(gcp_list, wkt)

    if nodata is not None:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).SetNoDataValue(nodata)
    elif unsetnodata:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).DeleteNoDataValue()

    if scale:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).SetScale(scale[i])

    if offset:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).SetOffset(offset[i])

    if units:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).SetUnitType(units)

    if unsetstats:
        for i in range(ds.RasterCount):
            band = ds.GetRasterBand(i + 1)
            for key in band.GetMetadata().keys():
                if key.startswith("STATISTICS_"):
                    band.SetMetadataItem(key, None)

    if stats:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i + 1).ComputeStatistics(approx_stats)

    if setstats:
        for i in range(ds.RasterCount):
            if (
                statsmin is None
                or statsmax is None
                or statsmean is None
                or statsdev is None
            ):
                ds.GetRasterBand(i + 1).ComputeStatistics(approx_stats)
                min, max, mean, stdev = ds.GetRasterBand(i + 1).GetStatistics(
                    approx_stats, True
                )
                if statsmin is None:
                    statsmin = min
                if statsmax is None:
                    statsmax = max
                if statsmean is None:
                    statsmean = mean
                if statsdev is None:
                    statsdev = stdev
            ds.GetRasterBand(i + 1).SetStatistics(
                statsmin, statsmax, statsmean, statsdev
            )

    if molist:
        if unsetmd:
            md = {}
        else:
            md = ds.GetMetadata()
        for moitem in molist:
            equal_pos = moitem.find("=")
            if equal_pos > 0:
                md[moitem[0:equal_pos]] = moitem[equal_pos + 1 :]
        ds.SetMetadata(md)
    elif unsetmd:
        ds.SetMetadata({})

    for band in colorinterp:
        ds.GetRasterBand(band).SetColorInterpretation(colorinterp[band])

    if unsetrpc:
        ds.SetMetadata(None, "RPC")

    ds = band = None

    return 0


def main(argv=sys.argv):
    return gdal_edit(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
