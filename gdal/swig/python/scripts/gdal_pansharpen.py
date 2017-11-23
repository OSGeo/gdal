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
import os.path
import sys
from osgeo import gdal

def DoesDriverHandleExtension(drv, ext):
    exts = drv.GetMetadataItem(gdal.DMD_EXTENSIONS)
    return exts is not None and exts.lower().find(ext.lower()) >= 0

def GetExtension(filename):
    ext = os.path.splitext(filename)[1]
    if ext.startswith('.'):
        ext = ext[1:]
    return ext

def GetOutputDriversFor(filename):
    drv_list = []
    ext = GetExtension(filename)
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        if (drv.GetMetadataItem(gdal.DCAP_CREATE) is not None or \
            drv.GetMetadataItem(gdal.DCAP_CREATECOPY) is not None) and \
           drv.GetMetadataItem(gdal.DCAP_RASTER) is not None:
            if len(ext) > 0 and DoesDriverHandleExtension(drv, ext):
                drv_list.append( drv.ShortName )
            else:
                prefix = drv.GetMetadataItem(gdal.DMD_CONNECTION_PREFIX)
                if prefix is not None and filename.lower().startswith(prefix.lower()):
                    drv_list.append( drv.ShortName )

    # GMT is registered before netCDF for opening reasons, but we want
    # netCDF to be used by default for output.
    if ext.lower() == 'nc' and len(drv_list) == 0 and \
       drv_list[0].upper() == 'GMT' and drv_list[1].upper() == 'NETCDF':
           drv_list = [ 'NETCDF', 'GMT' ]

    return drv_list

def GetOutputDriverFor(filename):
    drv_list = GetOutputDriversFor(filename)
    if len(drv_list) == 0:
        ext = GetExtension(filename)
        if len(ext) == 0:
            return 'GTiff'
        else:
            raise Exception("Cannot guess driver for %s" % filename)
    elif len(drv_list) > 1:
        print("Several drivers matching %s extension. Using %s" % (ext, drv_list[0]))
    return drv_list[0]

def Usage():
    print('Usage: gdal_pansharpen [--help-general] pan_dataset {spectral_dataset[,band=num]}+ out_dataset')
    print('                       [-of format] [-b band]* [-w weight]*')
    print('                       [-r {nearest,bilinear,cubic,cubicspline,lanczos,average}]')
    print('                       [-threads {ALL_CPUS|number}] [-bitdepth val] [-nodata val]')
    print('                       [-spat_adjust {union,intersection,none,nonewithoutwarning}]')
    print('                       [-verbose_vrt] [-co NAME=VALUE]* [-q]')
    print('')
    print('Create a dataset resulting from a pansharpening operation.')
    return -1

def gdal_pansharpen(argv):

    argv = gdal.GeneralCmdLineProcessor( argv )
    if argv is None:
        return -1

    pan_name = None
    last_name = None
    spectral_ds = []
    spectral_bands = []
    out_name = None
    bands = []
    weights = []
    format = None
    creation_options = []
    callback = gdal.TermProgress
    resampling = None
    spat_adjust = None
    verbose_vrt = False
    num_threads = None
    bitdepth = None
    nodata = None

    i = 1
    argc = len(argv)
    while i < argc:
        if (argv[i] == '-of' or argv[i] == '-f') and i < len(argv)-1:
            format = argv[i+1]
            i = i + 1
        elif argv[i] == '-r' and i < len(argv)-1:
            resampling = argv[i+1]
            i = i + 1
        elif argv[i] == '-spat_adjust' and i < len(argv)-1:
            spat_adjust = argv[i+1]
            i = i + 1
        elif argv[i] == '-b' and i < len(argv)-1:
            bands.append(int(argv[i+1]))
            i = i + 1
        elif argv[i] == '-w' and i < len(argv)-1:
            weights.append(float(argv[i+1]))
            i = i + 1
        elif argv[i] == '-co' and i < len(argv)-1:
            creation_options.append(argv[i+1])
            i = i + 1
        elif argv[i] == '-threads' and i < len(argv)-1:
            num_threads = argv[i+1]
            i = i + 1
        elif argv[i] == '-bitdepth' and i < len(argv)-1:
            bitdepth = argv[i+1]
            i = i + 1
        elif argv[i] == '-nodata' and i < len(argv)-1:
            nodata = argv[i+1]
            i = i + 1
        elif argv[i] == '-q':
            callback = None
        elif argv[i] == '-verbose_vrt':
            verbose_vrt = True
        elif argv[i][0] == '-':
            sys.stderr.write('Unrecognized option : %s\n' % argv[i])
            return Usage()
        elif pan_name is None:
            pan_name = argv[i]
            pan_ds = gdal.Open(pan_name)
            if pan_ds is None:
                return 1
        else:
            if last_name is not None:
                pos = last_name.find(',band=')
                if pos > 0:
                    spectral_name = last_name[0:pos]
                    ds = gdal.Open(spectral_name)
                    if ds is None:
                        return 1
                    band_num = int(last_name[pos+len(',band='):])
                    band = ds.GetRasterBand(band_num)
                    spectral_ds.append(ds)
                    spectral_bands.append(band)
                else:
                    spectral_name = last_name
                    ds = gdal.Open(spectral_name)
                    if ds is None:
                        return 1
                    for j in range(ds.RasterCount):
                        spectral_ds.append(ds)
                        spectral_bands.append(ds.GetRasterBand(j+1))

            last_name = argv[i]

        i = i + 1

    if pan_name is None or len(spectral_bands) == 0:
        return Usage()
    out_name = last_name

    if format is None:
        format = GetOutputDriverFor(out_name)

    if len(bands) == 0:
        bands = [ j+1 for j in range(len(spectral_bands)) ]
    else:
        for i in range(len(bands)):
            if bands[i] < 0 or bands[i] > len(spectral_bands):
                print('Invalid band number in -b: %d' % bands[i])
                return 1

    if len(weights) != 0 and len(weights) != len(spectral_bands):
        print('There must be as many -w values specified as input spectral bands')
        return 1

    vrt_xml = """<VRTDataset subClass="VRTPansharpenedDataset">\n"""
    if bands != [ j+1 for j in range(len(spectral_bands)) ]:
        for i in range(len(bands)):
            band = spectral_bands[bands[i]-1]
            datatype = gdal.GetDataTypeName(band.DataType)
            colorname = gdal.GetColorInterpretationName(band.GetColorInterpretation())
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

    if num_threads is not None:
        vrt_xml += '      <NumThreads>%s</NumThreads>\n' % num_threads

    if bitdepth is not None:
        vrt_xml += '      <BitDepth>%s</BitDepth>\n' % bitdepth

    if nodata is not None:
        vrt_xml += '      <NoData>%s</NoData>\n' % nodata

    if spat_adjust is not None:
        vrt_xml += '      <SpatialExtentAdjustment>%s</SpatialExtentAdjustment>\n' % spat_adjust

    pan_relative='0'
    if format.upper() == 'VRT':
        if not os.path.isabs(pan_name):
            pan_relative='1'
            pan_name = os.path.relpath(pan_name, os.path.dirname(out_name))

    vrt_xml += """    <PanchroBand>
      <SourceFilename relativeToVRT="%s">%s</SourceFilename>
      <SourceBand>1</SourceBand>
    </PanchroBand>\n""" % (pan_relative, pan_name)

    for i in range(len(spectral_bands)):
        dstband = ''
        for j in range(len(bands)):
            if i + 1 == bands[j]:
                dstband = ' dstBand="%d"' % (j+1)
                break

        ms_relative='0'
        ms_name = spectral_ds[i].GetDescription()
        if format.upper() == 'VRT':
            if not os.path.isabs(ms_name):
                ms_relative='1'
                ms_name = os.path.relpath(ms_name, os.path.dirname(out_name))

        vrt_xml += """    <SpectralBand%s>
      <SourceFilename relativeToVRT="%s">%s</SourceFilename>
      <SourceBand>%d</SourceBand>
    </SpectralBand>\n""" % (dstband, ms_relative, ms_name, spectral_bands[i].GetBand())

    vrt_xml += """  </PansharpeningOptions>\n"""
    vrt_xml += """</VRTDataset>\n"""

    if format.upper() == 'VRT':
        f = gdal.VSIFOpenL(out_name, 'wb')
        if f is None:
            print('Cannot create %s' % out_name)
            return 1
        gdal.VSIFWriteL(vrt_xml, 1, len(vrt_xml), f)
        gdal.VSIFCloseL(f)
        if verbose_vrt:
            vrt_ds = gdal.Open(out_name, gdal.GA_Update)
            vrt_ds.SetMetadata(vrt_ds.GetMetadata())
        else:
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
