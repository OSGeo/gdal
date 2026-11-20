#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
#
#  Project:  GDAL scripts
#  Purpose:  Perform a pansharpening operation
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import os.path
import sys
from numbers import Real
from typing import List, Optional, Sequence, Union

from osgeo import gdal
from osgeo_utils.auxiliary.util import GetOutputDriverFor, enable_gdal_exceptions


def Usage(isError):
    f = sys.stderr if isError else sys.stdout
    print("Usage: gdal_pansharpen [--help] [--help-general]", file=f)
    print(
        "                       <pan_dataset> {<spectral_dataset>[,band=<num>]} {<spectral_dataset>[,band=<num>]}... <out_dataset>",
        file=f,
    )
    print(
        "                       [-of <format>] [-b <band>]... [-w <weight>]...", file=f
    )
    print(
        "                       [-r {nearest|bilinear|cubic|cubicspline|lanczos|average}]",
        file=f,
    )
    print(
        "                       [-threads {ALL_CPUS|<number>}] [-bitdepth <val>] [-nodata <val>]",
        file=f,
    )
    print(
        "                       [-spat_adjust {union|intersection|none|nonewithoutwarning}]",
        file=f,
    )
    print("                       [-verbose_vrt] [-co <NAME>=<VALUE>]... [-q]", file=f)
    print("", file=f)
    print("Create a dataset resulting from a pansharpening operation.", file=f)
    return 2 if isError else 0


@enable_gdal_exceptions
def main(argv=sys.argv):

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    pan_name = None
    spectral_names = []
    spectral_ds = []
    spectral_bands = []
    band_nums = []
    weights = []
    driver_name = None
    creation_options = []
    progress_callback = gdal.TermProgress_nocb
    resampling = None
    spat_adjust = None
    verbose_vrt = False
    num_threads = None
    bitdepth = None
    nodata_value = None

    i = 1
    argc = len(argv)
    while i < argc:
        if (argv[i] == "-of" or argv[i] == "-f") and i < len(argv) - 1:
            driver_name = argv[i + 1]
            i = i + 1
        elif argv[i] == "-r" and i < len(argv) - 1:
            resampling = argv[i + 1]
            i = i + 1
        elif argv[i] == "-spat_adjust" and i < len(argv) - 1:
            spat_adjust = argv[i + 1]
            i = i + 1
        elif argv[i] == "-b" and i < len(argv) - 1:
            band_nums.append(int(argv[i + 1]))
            i = i + 1
        elif argv[i] == "-w" and i < len(argv) - 1:
            weights.append(float(argv[i + 1]))
            i = i + 1
        elif argv[i] == "-co" and i < len(argv) - 1:
            creation_options.append(argv[i + 1])
            i = i + 1
        elif argv[i] == "-threads" and i < len(argv) - 1:
            num_threads = argv[i + 1]
            i = i + 1
        elif argv[i] == "-bitdepth" and i < len(argv) - 1:
            bitdepth = argv[i + 1]
            i = i + 1
        elif argv[i] == "-nodata" and i < len(argv) - 1:
            nodata_value = argv[i + 1]
            i = i + 1
        elif argv[i] == "-q":
            progress_callback = None
        elif argv[i] == "-verbose_vrt":
            verbose_vrt = True
        elif argv[i] == "--help":
            return Usage(isError=False)
        elif argv[i][0] == "-":
            sys.stderr.write("Unrecognized option : %s\n" % argv[i])
            return Usage(isError=True)
        elif pan_name is None:
            pan_name = argv[i]
        else:
            spectral_names.append(argv[i])

        i = i + 1

    if pan_name is None or len(spectral_names) < 2:
        return Usage(isError=True)

    dst_filename = spectral_names.pop()
    return gdal_pansharpen(
        argv=None,
        pan_name=pan_name,
        spectral_names=spectral_names,
        spectral_ds=spectral_ds,
        spectral_bands=spectral_bands,
        band_nums=band_nums,
        weights=weights,
        dst_filename=dst_filename,
        driver_name=driver_name,
        creation_options=creation_options,
        resampling=resampling,
        spat_adjust=spat_adjust,
        num_threads=num_threads,
        bitdepth=bitdepth,
        nodata_value=nodata_value,
        verbose_vrt=verbose_vrt,
        progress_callback=progress_callback,
    )


def gdal_pansharpen(
    argv: Optional[Sequence[str]] = None,
    pan_name: Optional[str] = None,
    spectral_names: Optional[Sequence[str]] = None,
    spectral_ds: Optional[List[gdal.Dataset]] = None,
    spectral_bands: Optional[List[gdal.Band]] = None,
    band_nums: Optional[Sequence[int]] = None,
    weights: Optional[Sequence[float]] = None,
    dst_filename: Optional[str] = None,
    driver_name: Optional[str] = None,
    creation_options: Optional[Sequence[str]] = None,
    resampling: Optional[str] = None,
    spat_adjust: Optional[str] = None,
    num_threads: Optional[Union[int, str]] = None,
    bitdepth: Optional[Union[int, str]] = None,
    nodata_value: Optional[Union[Real, str]] = None,
    verbose_vrt: bool = False,
    progress_callback: Optional = gdal.TermProgress_nocb,
):
    if argv:
        # this is here for backwards compatibility
        return main(argv)

    spectral_names = spectral_names or []
    spectral_ds = spectral_ds or []
    spectral_bands = spectral_bands or []
    band_nums = band_nums or []
    weights = weights or []
    creation_options = creation_options or []

    if spectral_names:
        parse_spectral_names(
            spectral_names=spectral_names,
            spectral_ds=spectral_ds,
            spectral_bands=spectral_bands,
        )

    if pan_name is None or not spectral_bands:
        return 1

    pan_ds = gdal.Open(pan_name)
    if pan_ds is None:
        return 1

    if driver_name is None:
        driver_name = GetOutputDriverFor(dst_filename)

    if not band_nums:
        band_nums = [j + 1 for j in range(len(spectral_bands))]
    else:
        for band in band_nums:
            if band < 0 or band > len(spectral_bands):
                print("Invalid band number in -b: %d" % band)
                return 1

    if weights and len(weights) != len(spectral_bands):
        print("There must be as many -w values specified as input spectral bands")
        return 1

    vrt_xml = """<VRTDataset subClass="VRTPansharpenedDataset">\n"""
    if band_nums != [j + 1 for j in range(len(spectral_bands))]:
        for i, band in enumerate(band_nums):
            sband = spectral_bands[band - 1]
            datatype = gdal.GetDataTypeName(sband.DataType)
            colorname = gdal.GetColorInterpretationName(sband.GetColorInterpretation())
            vrt_xml += """  <VRTRasterBand dataType="%s" band="%d" subClass="VRTPansharpenedRasterBand">
      <ColorInterp>%s</ColorInterp>
  </VRTRasterBand>\n""" % (
                datatype,
                i + 1,
                colorname,
            )

    vrt_xml += """  <PansharpeningOptions>\n"""

    if weights:
        vrt_xml += """      <AlgorithmOptions>\n"""
        vrt_xml += """        <Weights>"""
        for i, weight in enumerate(weights):
            if i > 0:
                vrt_xml += ","
            vrt_xml += "%.16g" % weight
        vrt_xml += "</Weights>\n"
        vrt_xml += """      </AlgorithmOptions>\n"""

    if resampling is not None:
        vrt_xml += f"      <Resampling>{resampling}</Resampling>\n"

    if num_threads is not None:
        vrt_xml += f"      <NumThreads>{num_threads}</NumThreads>\n"

    if bitdepth is not None:
        vrt_xml += f"      <BitDepth>{bitdepth}</BitDepth>\n"

    if nodata_value is not None:
        vrt_xml += f"      <NoData>{nodata_value}</NoData>\n"

    if spat_adjust is not None:
        vrt_xml += (
            f"      <SpatialExtentAdjustment>{spat_adjust}</SpatialExtentAdjustment>\n"
        )

    pan_relative = "0"
    if driver_name.upper() == "VRT":
        if not os.path.isabs(pan_name):
            pan_relative = "1"
            pan_name = os.path.relpath(pan_name, os.path.dirname(dst_filename))

    vrt_xml += """    <PanchroBand>
      <SourceFilename relativeToVRT="%s">%s</SourceFilename>
      <SourceBand>1</SourceBand>
    </PanchroBand>\n""" % (
        pan_relative,
        pan_name,
    )

    for i, sband in enumerate(spectral_bands):
        dstband = ""
        for j, band in enumerate(band_nums):
            if i + 1 == band:
                dstband = ' dstBand="%d"' % (j + 1)
                break

        ms_relative = "0"
        ms_name = spectral_ds[i].GetDescription()
        if driver_name.upper() == "VRT":
            if not os.path.isabs(ms_name):
                try:
                    ms_name = os.path.relpath(ms_name, os.path.dirname(dst_filename))
                    ms_relative = "1"
                except ValueError:
                    # Thrown if generating a relative path is not possible, e.g. if
                    # ms_name is on a different Windows drive from dst_filename
                    pass

        vrt_xml += """    <SpectralBand%s>
      <SourceFilename relativeToVRT="%s">%s</SourceFilename>
      <SourceBand>%d</SourceBand>
    </SpectralBand>\n""" % (
            dstband,
            ms_relative,
            ms_name,
            sband.GetBand(),
        )

    vrt_xml += """  </PansharpeningOptions>\n"""
    vrt_xml += """</VRTDataset>\n"""

    if driver_name.upper() == "VRT":
        f = gdal.VSIFOpenL(dst_filename, "wb")
        if f is None:
            print("Cannot create %s" % dst_filename)
            return 1
        gdal.VSIFWriteL(vrt_xml, 1, len(vrt_xml), f)
        gdal.VSIFCloseL(f)
        if verbose_vrt:
            vrt_ds = gdal.Open(dst_filename, gdal.GA_Update)
            vrt_ds.SetMetadata(vrt_ds.GetMetadata())
        else:
            vrt_ds = gdal.Open(dst_filename)
        if vrt_ds is None:
            return 1

        return 0

    vrt_ds = gdal.Open(vrt_xml)
    out_ds = gdal.GetDriverByName(driver_name).CreateCopy(
        dst_filename, vrt_ds, 0, creation_options, callback=progress_callback
    )
    if out_ds is None:
        return 1
    return 0


def parse_spectral_names(
    spectral_names: Sequence[str],
    spectral_ds: List[gdal.Dataset],
    spectral_bands: List[gdal.Band],
):
    for spectral_arg in spectral_names:
        # add selected bands
        pos = spectral_arg.find(",band=")
        if pos > 0:
            spectral_name = spectral_arg[0:pos]
            ds = gdal.Open(spectral_name)
            if ds is None:
                return 1
            band_num = int(spectral_arg[pos + len(",band=") :])
            band = ds.GetRasterBand(band_num)
            spectral_ds.append(ds)
            spectral_bands.append(band)
        else:
            # add all bands
            spectral_name = spectral_arg
            ds = gdal.Open(spectral_name)
            if ds is None:
                return 1
            for j in range(ds.RasterCount):
                spectral_ds.append(ds)
                spectral_bands.append(ds.GetRasterBand(j + 1))


if __name__ == "__main__":
    sys.exit(main(sys.argv))
