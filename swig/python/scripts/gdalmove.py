#!/usr/bin/env python
#******************************************************************************
# 
#  Project:  GDAL Python Interface
#  Purpose:  Application for "warping" an image by just updating it's SRS
#            and geotransform.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 2012, Frank Warmerdam
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
#******************************************************************************

from osgeo import gdal, osr
import sys
import math

###############################################################################
def fmt_loc( srs_obj, loc):
    if srs_obj.IsProjected():
        return '%12.3f %12.3f' % (loc[0], loc[1])
    else:
        return '%12.8f %12.8f' % (loc[0], loc[1])
    
###############################################################################
def move( filename, t_srs, s_srs=None, pixel_threshold = None ):

    # -------------------------------------------------------------------------
    # Open the file. 
    # -------------------------------------------------------------------------
    ds = gdal.Open( filename )

    # -------------------------------------------------------------------------
    # Compute the current (s_srs) locations of the four corners and center
    # of the image. 
    # -------------------------------------------------------------------------
    corners_names = [
        'Upper Left',
        'Lower Left',
        'Upper Right',
        'Lower Right',
        'Center' ]
    
    corners_pixel_line = [
        (0, 0, 0),
        (0, ds.RasterYSize,0),
        (ds.RasterXSize, 0, 0),
        (ds.RasterXSize, ds.RasterYSize, 0),
        (ds.RasterXSize/2.0, ds.RasterYSize/2.0, 0.0) ]

    orig_gt = ds.GetGeoTransform()


    corners_s_geo = []
    for item in corners_pixel_line:
        corners_s_geo.append( \
            (orig_gt[0] + item[0] * orig_gt[1] + item[1] * orig_gt[2],
             orig_gt[3] + item[0] * orig_gt[4] + item[1] * orig_gt[5],
             item[2]) )
        
    # -------------------------------------------------------------------------
    # Prepare a transformation from source to destination srs. 
    # -------------------------------------------------------------------------
    if s_srs is None:
        s_srs = ds.GetProjectionRef()

    s_srs_obj = osr.SpatialReference()
    s_srs_obj.SetFromUserInput( s_srs )
    
    t_srs_obj = osr.SpatialReference()
    t_srs_obj.SetFromUserInput( t_srs )

    tr = osr.CoordinateTransformation( s_srs_obj, t_srs_obj )

    # -------------------------------------------------------------------------
    # Transform the corners
    # -------------------------------------------------------------------------
    
    corners_t_geo = tr.TransformPoints( corners_s_geo )
    
    # -------------------------------------------------------------------------
    #  Compute a new geotransform for the image in the target SRS.  For now
    #  we just use the top left, top right, and bottom left to produce the
    #  geotransform.  The result will be exact at these three points by
    #  definition, but if the underlying transformation is not affine it will
    #  be wrong at the center and bottom right.  It would be better if we
    #  used all five points for a least squares fit but that is a bit beyond
    #  me for now. 
    # -------------------------------------------------------------------------
    ul = corners_t_geo[0]
    ur = corners_t_geo[2]
    ll = corners_t_geo[1]
    
    new_gt = (ul[0],
              (ur[0] - ul[0]) / ds.RasterXSize,
              (ll[0] - ul[0]) / ds.RasterYSize,
              ul[1],
              (ur[1] - ul[1]) / ds.RasterXSize,
              (ll[1] - ul[1]) / ds.RasterYSize)

    (x,inv_new_gt) = gdal.InvGeoTransform( new_gt )
    
    # -------------------------------------------------------------------------
    #  Report results for the five locations.
    # -------------------------------------------------------------------------

    corners_t_new_geo = []
    error_geo = []
    error_pixel_line = []
    corners_pixel_line_new = []
    
    print('___Corner___ ________Original________  _______Adjusted_________   ______ Err (geo) ______ _Err (pix)_')

    for i in range(len(corners_s_geo)):

        item = corners_pixel_line[i]
        corners_t_new_geo.append( 
            (new_gt[0] + item[0] * new_gt[1] + item[1] * new_gt[2],
             new_gt[3] + item[0] * new_gt[4] + item[1] * new_gt[5],
             item[2]) )

        error_geo.append( (corners_t_new_geo[i][0] - corners_t_geo[i][0],
                           corners_t_new_geo[i][1] - corners_t_geo[i][1],
                           0.0) )


        item = corners_t_geo[i]
        corners_pixel_line_new.append(
            (inv_new_gt[0] + item[0] * inv_new_gt[1] + item[1] * inv_new_gt[2],
             inv_new_gt[3] + item[0] * inv_new_gt[4] + item[1] * inv_new_gt[5],
             item[2]) )

        error_pixel_line.append(
            (corners_pixel_line_new[i][0] - corners_pixel_line[i][0],
             corners_pixel_line_new[i][1] - corners_pixel_line[i][1],
             corners_pixel_line_new[i][2] - corners_pixel_line[i][2]) )

        print( '%-11s %s %s %s %5.2f %5.2f' % \
            (corners_names[i],
             fmt_loc(s_srs_obj, corners_s_geo[i]),
             fmt_loc(t_srs_obj, corners_t_geo[i]),
             fmt_loc(t_srs_obj, error_geo[i]),
             error_pixel_line[i][0],
             error_pixel_line[i][1]))

    print('')

    # -------------------------------------------------------------------------
    # Do we want to update the file?
    # -------------------------------------------------------------------------
    max_error = 0
    for err_item in error_pixel_line:
        this_error = math.sqrt(err_item[0] * err_item[0] + err_item[1] * err_item[1])
        if this_error > max_error:
            max_error = this_error

    update = 0
    if pixel_threshold is not None:
        if pixel_threshold > max_error:
            update = 1

    # -------------------------------------------------------------------------
    # Apply the change coordinate system and geotransform.
    # -------------------------------------------------------------------------
    if update:
        ds = None
        ds = gdal.Open( filename, gdal.GA_Update )

        print('Updating file...')
        ds.SetGeoTransform( new_gt )
        ds.SetProjection( t_srs_obj.ExportToWkt() )
        print('Done.')

    elif pixel_threshold is None:
        print('No error threshold in pixels selected with -et, file not updated.')

    else:
        print("""Maximum check point error is %.5f pixels which exceeds the
error threshold so the file has not been updated.""" % max_error)
    
    ds = None

###############################################################################
def Usage():
    print("""
gdalmove.py [-s_srs <srs_defn>] -t_srs <srs_defn>
            [-et <max_pixel_err>] target_file
""")
    sys.exit(1)
    
#############################################################################
# Main

if __name__ == '__main__':

    # Default GDAL argument parsing.
    
    argv = gdal.GeneralCmdLineProcessor( sys.argv )
    if argv is None:
        sys.exit( 0 )

    if len(argv) == 1:
        Usage()
        
    # Script argument defaults
    s_srs = None
    t_srs = None
    update=0
    filename = None
    pixel_threshold = None

    # Script argument parsing.

    i = 1
    while  i < len(argv):

        if argv[i] == '-s_srs' and i < len(argv)-1:
            s_srs = argv[i+1]
            i += 1

        elif argv[i] == '-t_srs' and i < len(argv)-1:
            t_srs = argv[i+1]
            i += 1

        elif argv[i] == '-et' and i < len(argv)-1:
            pixel_threshold = float(argv[i+1])
            i += 1

        elif filename == None:
            filename = argv[i]

        else:
            print('Urecognised argument: ' + argv[i])
            Usage()

        i = i + 1
        # next argument


    if filename is None:
        print('Missing name of file to operate on, but required.')
        Usage()

    if t_srs is None:
        print('Target SRS (-t_srs) missing, but required.')
        Usage()


    move( filename, t_srs, s_srs, pixel_threshold )
