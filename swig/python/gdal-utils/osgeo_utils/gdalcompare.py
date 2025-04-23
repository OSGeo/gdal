#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Compare two files for differences and report.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import array
import filecmp
import math
import os
import sys

from osgeo import gdal, osr

#######################################################
from osgeo_utils.auxiliary.base import PathLikeOrStr
from osgeo_utils.auxiliary.util import enable_gdal_exceptions

my_print = print


def compare_metadata(golden_md, new_md, md_id, options=None):

    if golden_md is None and new_md is None:
        return 0

    found_diff = 0

    golden_keys = list(golden_md.keys())
    new_keys = list(new_md.keys())
    dont_care_keys = ["backend", "ERR_BIAS", "ERR_RAND"]

    for key in dont_care_keys:
        if key in golden_keys:
            golden_keys.remove(key)
        if key in new_keys:
            new_keys.remove(key)

    if len(golden_keys) != len(new_keys):
        my_print("Difference in %s metadata key count" % md_id)
        my_print("  Golden Keys: " + str(golden_keys))
        my_print("  New Keys: " + str(new_keys))
        found_diff += 1

    for key in golden_keys:
        if key not in new_keys:
            my_print('New %s metadata lacks key "%s"' % (md_id, key))
            found_diff += 1
        elif md_id == "RPC" and new_md[key].strip() != golden_md[key].strip():
            # The strip above is because _RPC.TXT files and in-file have a difference
            # in white space that is not otherwise meaningful.
            my_print('RPC Metadata value difference for key "' + key + '"')
            my_print('  Golden: "' + golden_md[key] + '"')
            my_print('  New:    "' + new_md[key] + '"')
            found_diff += 1
        elif md_id != "RPC" and new_md[key] != golden_md[key]:
            if key == "NITF_FDT":
                # this will always have the current date set
                continue
            my_print('Metadata value difference for key "' + key + '"')
            my_print('  Golden: "' + golden_md[key] + '"')
            my_print('  New:    "' + new_md[key] + '"')
            found_diff += 1

    return found_diff


#######################################################
# Review and report on the actual image pixels that differ.
def compare_image_pixels(golden_band, new_band, id, options=None):

    diff_count = 0
    max_diff = 0

    out_db = None
    if "DUMP_DIFFS" in options:
        prefix = ""
        for opt in options:
            if opt.startswith("DUMP_DIFFS_PREFIX="):
                prefix = opt[len("DUMP_DIFFS_PREFIX=") :]
                break
        diff_fn = prefix + id.replace(" ", "_") + ".tif"
        out_db = gdal.GetDriverByName("GTiff").Create(
            diff_fn, golden_band.XSize, golden_band.YSize, 1, gdal.GDT_Float32
        )

    xsize = golden_band.XSize
    for line in range(golden_band.YSize):
        golden_line = array.array(
            "d", golden_band.ReadRaster(0, line, xsize, 1, buf_type=gdal.GDT_Float64)
        )
        new_line = array.array(
            "d", new_band.ReadRaster(0, line, xsize, 1, buf_type=gdal.GDT_Float64)
        )
        diff_line = [golden_line[i] - new_line[i] for i in range(xsize)]
        max_diff_this_line = max([abs(x) for x in diff_line])
        max_diff = max(max_diff, max_diff_this_line)
        if max_diff_this_line:
            diff_count += sum([(1 if x else 0) for x in diff_line])
        if out_db is not None:
            out_db.GetRasterBand(1).WriteRaster(
                0,
                line,
                xsize,
                1,
                array.array("d", diff_line).tobytes(),
                buf_type=gdal.GDT_Float64,
            )

    my_print("  Pixels Differing: " + str(diff_count))
    my_print("  Maximum Pixel Difference: " + str(max_diff))
    if out_db is not None:
        my_print("  Wrote Diffs to: %s" % diff_fn)


#######################################################


def compare_band(golden_band, new_band, id, options=None):
    found_diff = 0

    options = [] if options is None else options

    if golden_band.XSize != new_band.XSize or golden_band.YSize != new_band.YSize:
        my_print(
            "Band size mismatch (band=%s golden=[%d,%d], new=[%d,%d])"
            % (id, golden_band.XSize, golden_band.YSize, new_band.XSize, new_band.YSize)
        )
        found_diff += 1

    if golden_band.DataType != new_band.DataType:
        my_print("Band %s pixel types differ." % id)
        my_print("  Golden: " + gdal.GetDataTypeName(golden_band.DataType))
        my_print("  New:    " + gdal.GetDataTypeName(new_band.DataType))
        found_diff += 1

    golden_nodata = golden_band.GetNoDataValue()
    new_nodata = new_band.GetNoDataValue()

    # Two 'nan' values are _never_ equal, but bands that both use 'nan' as
    # nodata value do in fact use the same nodata value. Same for 'inf' and
    # '-inf'. These checks are kind of gross, but are unavoidable since 'None'
    # has to be accounted for. The reader might be tempted to simplify these
    # checks with a couple of 'set()'s, however a set containing two 'nan'
    # values has a length of 2, not 1.
    if None not in (golden_nodata, new_nodata) and (
        math.isnan(golden_nodata) and math.isnan(new_nodata)
    ):
        pass
    elif None not in (golden_nodata, new_nodata) and (
        math.isinf(golden_nodata) and math.isinf(new_nodata)
    ):
        pass
    elif golden_nodata != new_nodata:
        my_print("Band %s nodata values differ." % id)
        my_print("  Golden: " + str(golden_nodata))
        my_print("  New:    " + str(new_nodata))
        found_diff += 1

    if golden_band.GetColorInterpretation() != new_band.GetColorInterpretation():
        my_print("Band %s color interpretation values differ." % id)
        my_print(
            "  Golden: "
            + gdal.GetColorInterpretationName(golden_band.GetColorInterpretation())
        )
        my_print(
            "  New:    "
            + gdal.GetColorInterpretationName(new_band.GetColorInterpretation())
        )
        found_diff += 1

    golden_band_checksum = golden_band.Checksum()
    new_band_checksum = new_band.Checksum()
    if golden_band_checksum != new_band_checksum:
        my_print("Band %s checksum difference:" % id)
        my_print("  Golden: " + str(golden_band_checksum))
        my_print("  New:    " + str(new_band_checksum))
        if found_diff == 0:
            compare_image_pixels(golden_band, new_band, id, options)
        found_diff += 1
    else:
        # check a bit deeper in case of Float data type for which the Checksum() function is not reliable
        if golden_band.DataType in (gdal.GDT_Float32, gdal.GDT_Float64):
            if golden_band.ComputeRasterMinMax(
                can_return_none=True
            ) != new_band.ComputeRasterMinMax(can_return_none=True):
                my_print("Band %s statistics difference:" % 1)
                my_print("  Golden: " + str(golden_band.ComputeBandStats()))
                my_print("  New:    " + str(new_band.ComputeBandStats()))
                compare_image_pixels(golden_band, new_band, id, {})

    # Check overviews
    if "SKIP_OVERVIEWS" not in options:
        if golden_band.GetOverviewCount() != new_band.GetOverviewCount():
            my_print("Band %s overview count difference:" % id)
            my_print("  Golden: " + str(golden_band.GetOverviewCount()))
            my_print("  New:    " + str(new_band.GetOverviewCount()))
            found_diff += 1
        else:
            for i in range(golden_band.GetOverviewCount()):
                found_diff += compare_band(
                    golden_band.GetOverview(i),
                    new_band.GetOverview(i),
                    id + " overview " + str(i),
                    options,
                )

    # Mask band
    if golden_band.GetMaskFlags() != new_band.GetMaskFlags():
        my_print("Band %s mask flags difference:" % id)
        my_print("  Golden: " + str(golden_band.GetMaskFlags()))
        my_print("  New:    " + str(new_band.GetMaskFlags()))
        found_diff += 1
    elif golden_band.GetMaskFlags() == gdal.GMF_PER_DATASET:
        # Check mask band if it's GMF_PER_DATASET
        found_diff += compare_band(
            golden_band.GetMaskBand(),
            new_band.GetMaskBand(),
            id + " mask band",
            options,
        )

    # Metadata
    if "SKIP_METADATA" not in options:
        found_diff += compare_metadata(
            golden_band.GetMetadata(), new_band.GetMetadata(), "Band " + id, options
        )

    # Band Description - currently this is opt in since we have not
    # been tracking this in the past.  It would be nice to make it the
    # default at some point.
    if "CHECK_BAND_DESC" in options:
        if golden_band.GetDescription() != new_band.GetDescription():
            my_print("Band %s descriptions difference:" % id)
            my_print("  Golden: " + str(golden_band.GetDescription()))
            my_print("  New:    " + str(new_band.GetDescription()))
            found_diff += 1

    # TODO: Color Table, gain/bias, units, blocksize, mask, min/max

    return found_diff


#######################################################


def compare_srs(golden_wkt, new_wkt):
    if golden_wkt == new_wkt:
        return 0

    my_print("Difference in SRS!")

    golden_srs = osr.SpatialReference(golden_wkt)
    new_srs = osr.SpatialReference(new_wkt)

    if golden_srs.IsSame(new_srs):
        my_print("  * IsSame() reports them as equivalent.")
    else:
        my_print("  * IsSame() reports them as different.")

    my_print("  Golden:")
    my_print("  " + (golden_srs.ExportToPrettyWkt() if golden_wkt else "None"))
    my_print("  New:")
    my_print("  " + (new_srs.ExportToPrettyWkt() if new_wkt else "None"))

    return 1


#######################################################


def compare_db(golden_db, new_db, options=None):
    found_diff = 0

    options = [] if options is None else options

    # Comparisons are done per-band, so an image with 'INTERLEAVE=PIXEL' and a
    # lot of bands will take hours to complete.
    if "SKIP_INTERLEAVE_CHECK" not in options:
        maxbands = 10
        interleave = golden_db.GetMetadata("IMAGE_STRUCTURE").get("INTERLEAVE", "")
        if golden_db.RasterCount > maxbands and interleave.lower() == "pixel":
            raise ValueError(
                f"Golden file has more than {maxbands} and INTERLEAVE={interleave} - this"
                f" check will eventually succeed but will take hours due to the"
                f" amount of I/O required for per-band comparisons. Recommend"
                f" testing image encoding directly in your test, and then"
                f" translating to a band interleaved format before calling this"
                f" method: {golden_db.GetDescription()}"
            )

    # SRS
    if "SKIP_SRS" not in options:
        found_diff += compare_srs(golden_db.GetProjection(), new_db.GetProjection())

    # GeoTransform
    if "SKIP_GEOTRANSFORM" not in options:
        golden_gt = golden_db.GetGeoTransform()
        new_gt = new_db.GetGeoTransform()
        if golden_gt != new_gt:
            my_print("GeoTransforms Differ:")
            my_print("  Golden: " + str(golden_gt))
            my_print("  New:    " + str(new_gt))
            found_diff += 1

    # Metadata
    if "SKIP_METADATA" not in options:
        found_diff += compare_metadata(
            golden_db.GetMetadata(), new_db.GetMetadata(), "Dataset", options
        )

    if "SKIP_RPC" not in options:
        found_diff += compare_metadata(
            golden_db.GetMetadata("RPC"), new_db.GetMetadata("RPC"), "RPC", options
        )

    if "SKIP_GEOLOCATION" not in options:
        found_diff += compare_metadata(
            golden_db.GetMetadata("GEOLOCATION"),
            new_db.GetMetadata("GEOLOCATION"),
            "GEOLOCATION",
            options,
        )

    # Bands
    if golden_db.RasterCount != new_db.RasterCount:
        my_print(
            "Band count mismatch (golden=%d, new=%d)"
            % (golden_db.RasterCount, new_db.RasterCount)
        )
        found_diff += 1
        return found_diff

    # Dimensions
    for i in range(golden_db.RasterCount):
        gSzX = golden_db.GetRasterBand(i + 1).XSize
        nSzX = new_db.GetRasterBand(i + 1).XSize
        gSzY = golden_db.GetRasterBand(i + 1).YSize
        nSzY = new_db.GetRasterBand(i + 1).YSize

        if gSzX != nSzX or gSzY != nSzY:
            my_print(
                "Band size mismatch (band=%d golden=[%d,%d], new=[%d,%d])"
                % (i, gSzX, gSzY, nSzX, nSzY)
            )
            found_diff += 1

    # If so-far-so-good, then compare pixels
    if found_diff == 0:
        for i in range(golden_db.RasterCount):
            found_diff += compare_band(
                golden_db.GetRasterBand(i + 1),
                new_db.GetRasterBand(i + 1),
                str(i + 1),
                options,
            )

    return found_diff


#######################################################


def compare_sds(golden_db, new_db, options=None):
    found_diff = 0

    options = [] if options is None else options

    golden_sds = golden_db.GetMetadata("SUBDATASETS")
    new_sds = new_db.GetMetadata("SUBDATASETS")

    count = len(list(golden_sds.keys())) // 2
    for i in range(count):
        key = "SUBDATASET_%d_NAME" % (i + 1)

        sub_golden_db = gdal.Open(golden_sds[key])
        sub_new_db = gdal.Open(new_sds[key])

        sds_diff = compare_db(sub_golden_db, sub_new_db, options)
        found_diff += sds_diff
        if sds_diff > 0:
            my_print(
                "%d differences found between:\n  %s\n  %s"
                % (sds_diff, golden_sds[key], new_sds[key])
            )

    return found_diff


#######################################################


def find_diff(
    golden_file: PathLikeOrStr,
    new_file: PathLikeOrStr,
    check_sds: bool = False,
    options=None,
):
    # Compare Files
    found_diff = 0

    options = [] if options is None else options

    if "SKIP_BINARY" not in options:
        # compare raw binary files.
        try:
            os.stat(golden_file)
            os.stat(new_file)

            if not filecmp.cmp(golden_file, new_file):
                my_print("Files differ at the binary level.")
                found_diff += 1
        except OSError:
            stat_golden = gdal.VSIStatL(str(golden_file))
            stat_new = gdal.VSIStatL(str(new_file))
            if stat_golden and stat_new:
                if stat_golden.size != stat_new.size:
                    my_print("Files differ at the binary level.")
                    found_diff += 1
                else:
                    f_golden = gdal.VSIFOpenL(str(golden_file), "rb")
                    f_new = gdal.VSIFOpenL(str(new_file), "rb")
                    if f_golden and f_new:
                        off = 0
                        while off < stat_golden.size:
                            to_read = min(stat_golden.size - off, 1024 * 1024)
                            golden_chunk = gdal.VSIFReadL(1, to_read, f_golden)
                            if len(golden_chunk) < to_read:
                                my_print(
                                    "Binary file comparison failed: not enough bytes read in golden file"
                                )
                                break
                            new_chunk = gdal.VSIFReadL(1, to_read, f_new)
                            if golden_chunk != new_chunk:
                                my_print("Files differ at the binary level.")
                                found_diff += 1
                                break
                            off += to_read
                    if f_golden:
                        gdal.VSIFCloseL(f_golden)
                    if f_new:
                        gdal.VSIFCloseL(f_new)
            else:
                if not stat_golden:
                    my_print(
                        "Skipped binary file comparison, golden file not in filesystem."
                    )
                elif not new_file:
                    my_print(
                        "Skipped binary file comparison, new file not in filesystem."
                    )

    # compare as GDAL Datasets.
    golden_db = gdal.Open(golden_file)
    new_db = gdal.Open(new_file)
    found_diff += compare_db(golden_db, new_db, options)

    if check_sds:
        found_diff += compare_sds(golden_db, new_db, options)

    return found_diff


#######################################################


def Usage(isError=True):
    f = sys.stderr if isError else sys.stdout
    print("Usage: gdalcompare [--help] [--help-general]", file=f)
    print("                      [-dumpdiffs] [-skip_binary] [-skip_overviews]", file=f)
    print("                      [-skip_geolocation] [-skip_geotransform]", file=f)
    print("                      [-skip_metadata] [-skip_rpc] [-skip_srs]", file=f)
    print("                      [-sds] <golden_file> <new_file>", file=f)
    return 2 if isError else 0


#######################################################
#
# Mainline
#


@enable_gdal_exceptions
def main(argv=sys.argv):

    # Default GDAL argument parsing.
    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Script argument parsing.
    golden_file = None
    new_file = None
    check_sds = 0
    options = []

    i = 1
    while i < len(argv):

        if argv[i] == "--help":
            return Usage(isError=False)

        elif argv[i] == "-sds":
            check_sds = 1

        elif argv[i] == "-dumpdiffs":
            options.append("DUMP_DIFFS")

        elif argv[i] == "-skip_binary":
            options.append("SKIP_BINARY")

        elif argv[i] == "-skip_overviews":
            options.append("SKIP_OVERVIEWS")

        elif argv[i] == "-skip_geolocation":
            options.append("SKIP_GEOLOCATION")

        elif argv[i] == "-skip_geotransform":
            options.append("SKIP_GEOTRANSFORM")

        elif argv[i] == "-skip_metadata":
            options.append("SKIP_METADATA")

        elif argv[i] == "-skip_rpc":
            options.append("SKIP_RPC")

        elif argv[i] == "-skip_srs":
            options.append("SKIP_SRS")

        elif golden_file is None:
            golden_file = argv[i]

        elif new_file is None:
            new_file = argv[i]

        else:
            my_print("Unrecognised argument: " + argv[i])
            return Usage()

        i = i + 1
        # next argument

    if len(argv) == 1:
        return Usage()

    found_diff = find_diff(golden_file, new_file, check_sds, options)
    print("Differences Found: " + str(found_diff))
    sys.exit(found_diff)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
