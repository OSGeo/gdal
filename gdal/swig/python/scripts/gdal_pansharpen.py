#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
#  Project:  GDAL scripts
#  Purpose:  Perform a pansharpening operation
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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
###############################################################################

import os
import sys
from osgeo import gdal

def Usage():
    print('Usage: gdal_pansharpen [--help-general] pan_dataset ms_dataset out_dataset')
    print('                       [-of format] [-b band]* [-ib band]* [-w weight_val]*')
    print('                       [-r {nearest,bilinear,cubic,cubicspline,lanczos,average}]')
    print('                       [-co NAME=VALUE]* [-q]')
    print('')
    print('Create a dataset resulting from a pansharpening operation.')
    return -1

def gdal_pansharpen(argv):

    argv = gdal.GeneralCmdLineProcessor( argv )
    if argv is None:
        return -1

    pan_name = None
    ms_name = None
    out_name = None
    bands = []
    input_bands = []
    weights = []
    format = 'GTiff'
    creation_options = []
    callback = gdal.TermProgress
    resampling = None

    i = 1
    argc = len(argv)
    while i < argc:
        if argv[i] == '-of' and i < len(argv)-1:
            format = argv[i+1]
            i = i + 1
        elif argv[i] == '-r' and i < len(argv)-1:
            resampling = argv[i+1]
            i = i + 1
        elif argv[i] == '-b' and i < len(argv)-1:
            bands.append(int(argv[i+1]))
            i = i + 1
        elif argv[i] == '-ib' and i < len(argv)-1:
            input_bands.append(int(argv[i+1]))
            i = i + 1
        elif argv[i] == '-w' and i < len(argv)-1:
            weights.append(float(argv[i+1]))
            i = i + 1
        elif argv[i] == '-co' and i < len(argv)-1:
            creation_options.append(argv[i+1])
            i = i + 1
        elif argv[i] == '-q':
            callback = None
        elif argv[i][0] == '-':
            sys.stderr.write('Unrecognized option : %s\n' % argv[i])
            return Usage()
        elif pan_name is None:
            pan_name = argv[i]
        elif ms_name is None:
            ms_name = argv[i]
        elif out_name is None:
            out_name = argv[i]
        else:
            sys.stderr.write('Unexpected option : %s\n' % argv[i])
            return Usage()

        i = i + 1

    if pan_name is None or ms_name is None or out_name is None:
        return Usage()

    pan_ds = gdal.Open(pan_name)
    if pan_ds is None:
        return 1
    ms_ds = gdal.Open(ms_name)
    if ms_ds is None:
        return 1

    if len(input_bands) != 0 and len(bands) == 0:
        print('-b must be explicitely specified when -ib is used')
        return 1

    if len(bands) == 0:
        bands = [ j+1 for j in range(ms_ds.RasterCount) ]
        input_bands = bands
    else:
        for i in range(len(bands)):
            if bands[i] < 0 or bands[i] > ms_ds.RasterCount:
                print('Invalid band number in -b: %d' % bands[i])
                return 1
        if len(input_bands) == 0:
            input_bands = bands
        else:
            for i in range(len(input_bands)):
                if input_bands[i] < 0 or input_bands[i] > ms_ds.RasterCount:
                    print('Invalid band number in -ib: %d' % input_bands[i])
                    return 1
            for i in range(len(bands)):
                if bands[i] not in input_bands:
                    print('-b %d specified, but not listed as input band' % bands[i])
                    return 1

    if len(weights) != 0 and len(weights) != len(input_bands):
        print('There must be as many -w values specified as -ib values')
        return 1

    pan_relative='0'
    ms_relative='0'
    if format.upper() == 'VRT':
        if not os.path.isabs(pan_name):
            pan_relative='1'
            pan_name = os.path.relpath(pan_name, os.path.dirname(out_name))
        if not os.path.isabs(ms_name):
            ms_relative='1'
            ms_name = os.path.relpath(ms_name, os.path.dirname(out_name))

    vrt_xml = """<VRTDataset subClass="VRTPansharpenedDataset">\n"""
    if input_bands != bands:
        for i in range(len(bands)):
            datatype = gdal.GetDataTypeName(ms_ds.GetRasterBand(bands[i]).DataType)
            colorname = gdal.GetColorInterpretationName(ms_ds.GetRasterBand(bands[i]).GetColorInterpretation())
            vrt_xml += """  <VRTRasterBand dataType="%s" band="%d" subClass="VRTPansharpenedRasterBand">
      <ColorInterp>%s</ColorInterp>
  </VRTRasterBand>\n""" % (datatype, i+1, colorname)

    vrt_xml += """  <PansharpeningOptions>\n"""

    if len(weights) != 0:
        vrt_xml += """      <AlgorithmOptions>\n"""
        vrt_xml += """        <Weights>"""
        for i in range(len(weights)):
            if i > 0: vrt_xml += ","
            vrt_xml += "%.16g" % weights[i]
        vrt_xml += "</Weights>\n"
        vrt_xml += """      </AlgorithmOptions>\n"""

    if resampling is not None:
        vrt_xml += '      <Resampling>%s</Resampling>\n' % resampling

    vrt_xml += """      <PanchroBand>
      <SourceFilename relativeToVRT="%s">%s</SourceFilename>
      <SourceBand>1</SourceBand>
    </PanchroBand>\n""" % (pan_relative, pan_name)

    for i in range(len(input_bands)):
        dstband = ''
        for j in range(len(bands)):
            if input_bands[i] == bands[j]:
                dstband = ' dstBand="%d"' % (j+1)
                break

        vrt_xml += """    <SpectralBand%s>
      <SourceFilename relativeToVRT="%s">%s</SourceFilename>
      <SourceBand>%d</SourceBand>
    </SpectralBand>\n""" % (dstband, ms_relative, ms_name, input_bands[i])

    vrt_xml += """  </PansharpeningOptions>\n"""
    vrt_xml += """</VRTDataset>\n"""

    if format.upper() == 'VRT':
        f = gdal.VSIFOpenL(out_name, 'wb')
        if f is None:
            print('Cannot create %s' % out_name)
            return 1
        gdal.VSIFWriteL(vrt_xml, 1, len(vrt_xml), f)
        gdal.VSIFCloseL(f)
        vrt_ds = gdal.Open(out_name)
        if vrt_ds is None:
            return 1

        return 0

    vrt_ds = gdal.Open(vrt_xml)
    out_ds = gdal.GetDriverByName(format).CreateCopy(out_name, vrt_ds, 0, creation_options, callback = callback)
    if out_ds is None:
        return 1
    return 0

def main():
    return gdal_pansharpen(sys.argv)

if __name__ == '__main__':
    sys.exit(gdal_pansharpen(sys.argv))
