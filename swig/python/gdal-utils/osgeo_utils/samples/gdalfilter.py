#!/usr/bin/env python3
###############################################################################
#
# Project:  OGR Python samples
# Purpose:  Filter an input file, producing an output file.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

from osgeo import gdal

gdal.TermProgress = gdal.TermProgress_nocb


def Usage():
    print(
        "Usage: gdalfilter.py [-n] [-size n] [-coefs ...] [-f format] [-co NAME=VALUE]\n"
        "                     in_file out_file"
    )
    return 2


def main(argv=sys.argv):
    # srcwin = None
    # bands = []

    srcfile = None
    dstfile = None
    size = 3
    coefs = None
    normalized = 0

    out_format = None
    create_options = []

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == "-size":
            size = int(argv[i + 1])
            i = i + 1

        elif arg == "-n":
            normalized = 1

        elif arg == "-f":
            out_format = int(argv[i + 1])
            i = i + 1

        elif arg == "-co":
            create_options.append(argv[i + 1])
            i = i + 1

        elif arg == "-coefs":
            coefs = []
            for iCoef in range(size * size):
                try:
                    coefs.append(float(argv[iCoef + i + 1]))
                except Exception:
                    print(
                        "Didn't find enough valid kernel coefficients, need ",
                        size * size,
                    )
                    return 1
            i = i + size * size

        elif srcfile is None:
            srcfile = argv[i]

        elif dstfile is None:
            dstfile = argv[i]

        else:
            return Usage()

        i = i + 1

    if dstfile is None:
        return Usage()

    if out_format is None and dstfile[-4:].lower() == ".vrt":
        out_format = "VRT"
    else:
        out_format = "GTiff"

    # =============================================================================
    #   Open input file.
    # =============================================================================

    src_ds = gdal.Open(srcfile)

    # =============================================================================
    #   Create a virtual file in memory only which matches the configuration of
    #   the input file.
    # =============================================================================

    vrt_driver = gdal.GetDriverByName("VRT")
    vrt_ds = vrt_driver.CreateCopy("", src_ds)

    # =============================================================================
    #   Prepare coefficient list.
    # =============================================================================
    coef_list_size = size * size

    if coefs is None:
        coefs = []
        for i in range(coef_list_size):
            coefs.append(1.0 / coef_list_size)

    coefs_string = ""
    for i in range(coef_list_size):
        coefs_string = coefs_string + ("%.8g " % coefs[i])

    # =============================================================================
    #   Prepare template for XML description of the filtered source.
    # =============================================================================

    filt_template = """<KernelFilteredSource>
      <SourceFilename>%s</SourceFilename>
      <SourceBand>%%d</SourceBand>
      <Kernel normalized="%d">
        <Size>%d</Size>
        <Coefs>%s</Coefs>
      </Kernel>
    </KernelFilteredSource>""" % (
        srcfile,
        normalized,
        size,
        coefs_string,
    )

    # =============================================================================
    # Go through all the bands replacing the SimpleSource with a filtered source.
    # =============================================================================

    for iBand in range(vrt_ds.RasterCount):
        band = vrt_ds.GetRasterBand(iBand + 1)

        src_xml = filt_template % (iBand + 1)

        band.SetMetadata({"source_0": src_xml}, "vrt_sources")

    # =============================================================================
    # copy the results to a new file.
    # =============================================================================

    if out_format == "VRT":
        vrt_ds.SetDescription(dstfile)
        vrt_ds = None
        return 0

    out_driver = gdal.GetDriverByName(out_format)
    if out_driver is None:
        print("Output driver %s does not appear to exist." % out_format)
        return 0

    out_ds = out_driver.CreateCopy(
        dstfile, vrt_ds, options=create_options, callback=gdal.TermProgress
    )
    if out_ds is None:
        return 1
    else:
        out_ds = None
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
