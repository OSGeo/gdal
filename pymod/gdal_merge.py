#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  InSAR Peppers
# Purpose:  Module to extract data from many rasters into one output.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2000, Atlantis Scientific Inc. (www.atlsci.com)
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
# 
#  $Log$
#  Revision 1.3  2002/04/03 21:12:05  warmerda
#  added -separate flag for Gerald Buckmaster
#
#  Revision 1.2  2000/11/29 20:36:18  warmerda
#  allow output file to be preexisting
#
#  Revision 1.1  2000/11/29 20:23:13  warmerda
#  New
#
#

import gdal
import sys

verbose = 0

# =============================================================================
def raster_copy( s_fh, s_xoff, s_yoff, s_xsize, s_ysize, s_band_n,
                 t_fh, t_xoff, t_yoff, t_xsize, t_ysize, t_band_n ):

    if verbose != 0:
        print 'Copy %d,%d,%d,%d to %d,%d,%d,%d.' \
              % (s_xoff, s_yoff, s_xsize, s_ysize,
             t_xoff, t_yoff, t_xsize, t_ysize )

    s_band = s_fh.GetRasterBand( s_band_n )
    t_band = t_fh.GetRasterBand( t_band_n )

    data = s_band.ReadRaster( s_xoff, s_yoff, s_xsize, s_ysize,
                              t_xsize, t_ysize, t_band.DataType )
    t_band.WriteRaster( t_xoff, t_yoff, t_xsize, t_ysize,
                        data, t_xsize, t_ysize, t_band.DataType )
        

    return 0
    
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
        if fi.init_from_name( name ) == 1:
            file_infos.append( fi )

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

        return 1

    def report( self ):
        print 'Filename: '+ self.filename
        print 'File Size: %dx%dx%d' \
              % (self.xsize, self.ysize, self.bands)
        print 'Pixel Size: %f x %f' \
              % (self.geotransform[1],self.geotransform[5])
        print 'UL:(%f,%f)   LR:(%f,%f)' \
              % (self.ulx,self.uly,self.lrx,self.lry)

    def copy_into( self, t_fh, s_band = 1, t_band = 1 ):
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
        tgw_ulx = max(t_ulx,self.ulx)
        tgw_uly = min(t_uly,self.uly)
        tgw_lrx = min(t_lrx,self.lrx)
        tgw_lry = max(t_lry,self.lry)
        
        # do they even intersect?
        if tgw_ulx >= tgw_lrx or tgw_uly <= tgw_lry:
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

        # Open the source file, and copy the selected region.
        s_fh = gdal.Open( self.filename )
        
        return \
            raster_copy( s_fh, sw_xoff, sw_yoff, sw_xsize, sw_ysize, s_band,
                         t_fh, tw_xoff, tw_yoff, tw_xsize, tw_ysize, t_band )


# =============================================================================
def Usage():
    print 'Usage: gdal_merge.py [-o out_filename] [-f out_format] [-v]'
    print '                     [-ps pixelsize_x pixelsize_y] [-separate]'
    print '                     [-ul_lr ulx uly lrx lry] input_files'
    print

# =============================================================================
#
# Program mainline.
#

if __name__ == '__main__':

    names = []
    format = 'GTiff'
    out_file = 'out.tif'

    ulx = None
    psize_x = None
    separate = 0

    # Parse command line arguments.
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]

        if arg == '-o':
            i = i + 1
            out_file = sys.argv[i]

        elif arg == '-v':
            verbose = 1

        elif arg == '-separate':
            separate = 1

        elif arg == '-f':
            i = i + 1
            format = sys.argv[i]

        elif arg == '-ps':
            psize_x = float(sys.argv[i+1])
            psize_y = -1 * abs(float(sys.argv[i+1]))
            i = i + 2

        elif arg == '-ul_lr':
            ulx = float(sys.argv[i+1])
            uly = float(sys.argv[i+2])
            lrx = float(sys.argv[i+3])
            lry = float(sys.argv[i+4])
            i = i + 4

        elif arg[:1] == '-':
            Usage()
            sys.exit( 1 )

        else:
            names.append( arg )
            
        i = i + 1

    if len(names) == 0:
        Usage()
        sys.exit( 1 )

    Driver = gdal.GetDriverByName(format)
    if Driver is None:
        print 'Format driver %s not found, pick a supported driver.' % format
        sys.exit( 1 )

    # Collect information on all the source files.
    file_infos = names_to_fileinfos( names )

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

    # Try opening as an existing file.
    t_fh = gdal.Open( out_file, gdal.GA_ReadOnly )
    
    # Create output file if it does not already exist.
    if t_fh is None:
        geotransform = [ulx, psize_x, 0, uly, 0, psize_y]

        xsize = int((lrx - ulx) / geotransform[1])
        ysize = int((lry - uly) / geotransform[5])

        if separate != 0:
            bands = len(file_infos)
        else:
            bands = 1

        t_fh = Driver.Create( out_file, xsize, ysize, bands,
                              file_infos[0].band_type, '' )
        t_fh.SetGeoTransform( geotransform )

    # Copy data from source files into output file.
    t_band = 1
    for fi in file_infos:
        if verbose != 0:
            print
            fi.report()

        if separate == 0 :
            fi.copy_into( t_fh )
        else:
            fi.copy_into( t_fh, 1, t_band )
            t_band = t_band+1
            
    # Force file to be closed.
    t_fh = None
