#!/usr/bin/env python
# -*- coding: utf-8 -*- 
# Copyright (c) 2000, Atlantis Scientific Inc. (www.atlsci.com)
# Copyright (C) 2005  Gabriel Ebner <ge@gabrielebner.at>
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
#
# modified output of geotransform to retain same numerical
# precision as the gdal 'C' utilities
# Norman Vine  nhv@cape.com  03-Oct-2005 6:23:45 am

try:
    from osgeo import gdal
except ImportError:
    import gdal

import sys

# =============================================================================
def names_to_fileinfos( names ):
    """
    Translate a list of GDAL filenames, into file_info objects.

    names -- list of valid GDAL dataset names.

    Returns a list of file_info objects.  There may be less file_info objects
    than names if some of the names could not be opened as GDAL files.
    """
    
    file_infos = []
    for name in names:
        fi = file_info()
        if fi.init_from_name(name):
            file_infos.append(fi)
        else:
            print ('Can not open dataset "%s", skipped' % name)

    return file_infos

# *****************************************************************************
class file_info:
    """A class holding information about a GDAL file."""

    def init_from_name(self, filename):
        """
        Initialize file_info from filename

        filename -- Name of file to read.

        Returns 1 on success or 0 if the file can't be opened.
        """
        fh = gdal.Open( filename )
        if fh is None:
            return False

        self.filename = filename
        self.bands = fh.RasterCount
        self.xsize = fh.RasterXSize
        self.ysize = fh.RasterYSize
        self.projection = fh.GetProjection()
        self.geotransform = fh.GetGeoTransform()
        self.ulx = self.geotransform[0]
        self.uly = self.geotransform[3]
        self.lrx = self.ulx + self.geotransform[1] * self.xsize
        self.lry = self.uly + self.geotransform[5] * self.ysize

        self.band_types = [None]
        self.block_sizes = [None] 
        self.nodata = [None]
        self.cts = [None]
        self.color_interps = [None]
        for i in range(1, fh.RasterCount+1):
            band = fh.GetRasterBand(i)
            self.band_types.append(band.DataType)
            self.block_sizes.append(band.GetBlockSize()) 
            if band.GetNoDataValue() != None:
                self.nodata.append(band.GetNoDataValue())
            self.color_interps.append(band.GetRasterColorInterpretation())
            ct = band.GetRasterColorTable()
            if ct is not None:
                self.cts.append(ct.Clone())
            else:
                self.cts.append(None)

        return True

    def write_source(self, t_fh, t_geotransform, xsize, ysize, s_band):
        t_ulx = t_geotransform[0]
        t_uly = t_geotransform[3]
        t_lrx = t_geotransform[0] + xsize * t_geotransform[1]
        t_lry = t_geotransform[3] + ysize * t_geotransform[5]

        # figure out intersection region
        tgw_ulx = max(t_ulx,self.ulx)
        tgw_lrx = min(t_lrx,self.lrx)
        if t_geotransform[5] < 0:
            tgw_uly = min(t_uly,self.uly)
            tgw_lry = max(t_lry,self.lry)
        else:
            tgw_uly = max(t_uly,self.uly)
            tgw_lry = min(t_lry,self.lry)
        
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
        tw_xsize = int((tgw_lrx - t_geotransform[0])/t_geotransform[1] + 0.5) \
                   - tw_xoff
        tw_ysize = int((tgw_lry - t_geotransform[3])/t_geotransform[5] + 0.5) \
                   - tw_yoff

        if tw_xsize < 1 or tw_ysize < 1:
            return 1

        # Compute source window in pixel coordinates.
        sw_xoff = int((tgw_ulx - self.geotransform[0]) / self.geotransform[1])
        sw_yoff = int((tgw_uly - self.geotransform[3]) / self.geotransform[5])
        sw_xsize = int((tgw_lrx - self.geotransform[0]) \
                       / self.geotransform[1] + 0.5) - sw_xoff
        sw_ysize = int((tgw_lry - self.geotransform[3]) \
                       / self.geotransform[5] + 0.5) - sw_yoff

        if sw_xsize < 1 or sw_ysize < 1:
            return 1

        t_fh.write('\t\t<SimpleSource>\n')
        t_fh.write(('\t\t\t<SourceFilename relativeToVRT="1">%s' + 
            '</SourceFilename>\n') % self.filename)
        t_fh.write('\t\t\t<SourceBand>%i</SourceBand>\n' % s_band)
        data_type_name = gdal.GetDataTypeName(self.band_types[s_band]) 
        block_size_x, block_size_y = self.block_sizes[s_band] 
        t_fh.write('\t\t\t<SourceProperties RasterXSize="%i" RasterYSize="%i"' \
                    ' DataType="%s" BlockXSize="%i" BlockYSize="%i"/>\n' \
                    % (self.xsize, self.ysize, data_type_name, block_size_x, block_size_y))
        t_fh.write('\t\t\t<SrcRect xOff="%i" yOff="%i" xSize="%i" ySize="%i"/>\n' \
            % (sw_xoff, sw_yoff, sw_xsize, sw_ysize))
        t_fh.write('\t\t\t<DstRect xOff="%i" yOff="%i" xSize="%i" ySize="%i"/>\n' \
            % (tw_xoff, tw_yoff, tw_xsize, tw_ysize))
        t_fh.write('\t\t</SimpleSource>\n')

# =============================================================================
def Usage():
    print ('Usage: gdal_vrtmerge.py [-o out_filename] [-separate] [-pct]')
    print ('           [-ul_lr ulx uly lrx lry] [-ot datatype] [-i input_file_list')
    print ('           | input_files]')

# =============================================================================
#
# Program mainline.
#

if __name__ == '__main__':

    names = []
    out_file = 'out.vrt'

    ulx = None
    psize_x = None
    separate = False
    pre_init = None

    argv = gdal.GeneralCmdLineProcessor( sys.argv )
    if argv is None:
        sys.exit( 0 )

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-o':
            i = i + 1
            out_file = argv[i]
            
        elif arg == '-i': 
            i = i + 1 
            in_file_list = open(argv[i]) 
            names.extend(in_file_list.read().split()) 

        elif arg == '-separate':
            separate = True

        elif arg == '-ul_lr':
            ulx = float(argv[i+1])
            uly = float(argv[i+2])
            lrx = float(argv[i+3])
            lry = float(argv[i+4])
            i = i + 4

        elif arg[:1] == '-':
            print ('Unrecognised command option: ', arg)
            Usage()
            sys.exit( 1 )

        else:
            names.append( arg )
            
        i = i + 1

    if len(names) == 0:
        print ('No input files selected.')
        Usage()
        sys.exit( 1 )

    # Collect information on all the source files.
    file_infos = names_to_fileinfos( names )
    if len(file_infos) == 0:
        print ('Nothing to process, exiting.')
        sys.exit(1)

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
    
    projection = file_infos[0].projection
    
    for fi in file_infos:
        if fi.geotransform[1] != psize_x or fi.geotransform[5] != psize_y:
            print ("All files must have the same scale; %s does not" \
                % fi.filename)
            sys.exit(1)

        if fi.geotransform[2] != 0 or fi.geotransform[4] != 0:
            print ("No file must be rotated; %s is" % fi.filename)
            sys.exit(1)

        if fi.projection != projection:
            print ("All files must be in the same projection; %s is not" \
                % fi.filename)
            sys.exit(1)

    geotransform = (ulx, psize_x, 0.0, uly, 0.0, psize_y)

    xsize = int(((lrx - ulx) / geotransform[1]) + 0.5)
    ysize = int(((lry - uly) / geotransform[5]) + 0.5)

    if separate:
        bands = len(file_infos)
    else:
        bands = file_infos[0].bands

    t_fh = open(out_file, 'w')
    t_fh.write('<VRTDataset rasterXSize="%i" rasterYSize="%i">\n'
        % (xsize, ysize))
    t_fh.write('\t<GeoTransform>%24.16f, %24.16f, %24.16f, %24.16f, %24.16f, %24.16f</GeoTransform>\n'
        % geotransform)

    if len(projection) > 0:
        t_fh.write('\t<SRS>%s</SRS>\n' % projection)

    if separate:
        band_n = 0
        for fi in file_infos:
            band_n = band_n + 1
            if len(fi.band_types) != 2:
                print ('File %s has %d bands. Only first band will be taken into accout' % (fi.filename, len(fi.band_types)-1))
            dataType = gdal.GetDataTypeName(fi.band_types[1])

            t_fh.write('\t<VRTRasterBand dataType="%s" band="%i">\n'
                % (dataType, band_n))
            t_fh.write('\t\t<ColorInterp>%s</ColorInterp>\n' %
                gdal.GetColorInterpretationName(fi.color_interps[1]))
            fi.write_source(t_fh, geotransform, xsize, ysize, 1)
            t_fh.write('\t</VRTRasterBand>\n')
    else:
        for band in range(1, bands+1):
            dataType = gdal.GetDataTypeName(file_infos[0].band_types[band])

            t_fh.write('\t<VRTRasterBand dataType="%s" band="%i">\n'
                % (dataType, band))
            if file_infos[0].nodata != [None]:
                t_fh.write('\t\t<NoDataValue>%f</NoDataValue>\n' %
                    file_infos[0].nodata[band])
            t_fh.write('\t\t<ColorInterp>%s</ColorInterp>\n' %
                gdal.GetColorInterpretationName(
                    file_infos[0].color_interps[band]))

            ct = file_infos[0].cts[band]
            if ct != None:
                t_fh.write('\t\t<ColorTable>\n')
                for i in range(ct.GetCount()):
                    t_fh.write(
                        '\t\t\t<Entry c1="%i" c2="%i" c3="%i" c4="%i"/>\n'
                            % ct.GetColorEntry(i))
                t_fh.write('\t\t</ColorTable>\n')

            for fi in file_infos:
                fi.write_source(t_fh, geotransform, xsize, ysize, band)

            t_fh.write('\t</VRTRasterBand>\n')

    t_fh.write('</VRTDataset>\n')
