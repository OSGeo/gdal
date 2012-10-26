#!/usr/bin/env python
#******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Compare two files for differences and report.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
#******************************************************************************
#  Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
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

import os
import sys
import filecmp

from osgeo import gdal, osr

#######################################################
def compare_metadata(golden_md, new_md, id):
  if golden_md is None and new_md is None:
    return 0

  found_diff = 0

  if len(golden_md.keys()) != len(new_md.keys()):
    print 'Difference in %s metadata key count' % id
    print '  Golden Keys:', golden_md.keys()
    print '  New Keys:', new_md.keys()
    found_diff += 1

  for key in golden_md.keys():
    if not new_md.has_key(key):
      print 'New %s metadata lacks key' % (id, key)
      found_diff += 1
    elif new_md[key] != golden_md[key]:
      print 'Metadata value difference for key "' + key + '"'
      print '  Golden:"' + golden_md[key] + '"'
      print '  New:   "' + new_md[key] + '"'
      found_diff += 1

  return found_diff


#######################################################
# Review and report on the actual image pixels that differ.
def compare_image_pixels(golden_band, new_band, id):
  diff_count = 0
  max_diff = 0

  for line in range(golden_band.YSize):
    golden_line = golden_band.ReadAsArray(0, line, golden_band.XSize, 1)[0]
    new_line = new_band.ReadAsArray(0, line, golden_band.XSize, 1)[0]
    for pixel in range(golden_band.XSize):
      diff = new_line[pixel] - golden_line[pixel]
      if diff != 0:
        diff_count += 1
        if abs(diff) > max_diff:
          max_diff = abs(diff)

  print '  Pixels Differing:', diff_count
  print '  Maximum Pixel Difference: ', max_diff

#######################################################
def compare_band(golden_band, new_band, id):
  found_diff = 0

  if golden_band.DataType != new_band.DataType:
    print 'Band %s pixel types differ.'
    print '  Golden:', gdal.GetDataTypeName(golden_band.DataType)
    print '  New:   ', gdal.GetDataTypeName(new_band.DataType)
    found_diff += 1

  if golden_band.GetNoDataValue() != new_band.GetNoDataValue():
    print 'Band %s nodata values differ.'
    print '  Golden:', golden_band.GetNoDataValue()
    print '  New:   ', new_band.GetNoDataValue()
    found_diff += 1

  if golden_band.GetColorInterpretation() != new_band.GetColorInterpretation():
    print 'Band %s color interpretation values differ.'
    print '  Golden:', gdal.GetColorInterpretationName(golden_band.GetColorInterpretation())
    print '  New:   ', gdal.GetColorInterpretationName(new_band.GetColorInterpretation())
    found_diff += 1

  if golden_band.Checksum() != new_band.Checksum():
    print 'Band %s checksum difference:' % id
    print '  Golden:', golden_band.Checksum()
    print '  New:   ', new_band.Checksum()
    found_diff += 1
    compare_image_pixels(golden_band,new_band, id)

  # Check overviews
  if golden_band.GetOverviewCount() != new_band.GetOverviewCount():
    print 'Band %s overview count difference:' % id
    print '  Golden:', golden_band.GetOverviewCount()
    print '  New:   ', new_band.GetOverviewCount()
    found_diff += 1
  else:
    for i in range(golden_band.GetOverviewCount()):
      compare_band(golden_band.GetOverview(i),
                   new_band.GetOverview(i),
                   id + ' overview ' + str(i))

  # Metadata
  found_diff += compare_metadata(golden_band.GetMetadata(),
                                 new_band.GetMetadata(),
                                 'Band ' + id)

  # TODO: Color Table, gain/bias, units, blocksize, mask, min/max

  return found_diff

#######################################################
def compare_srs(golden_wkt, new_wkt):
  if golden_wkt == new_wkt:
    return 0

  print 'Difference in SRS!'

  golden_srs = osr.SpatialReference(golden_wkt)
  new_srs = osr.SpatialReference(new_wkt)

  if golden_srs.IsSame(new_srs):
    print '  * IsSame() reports them as equivelent.'
  else:
    print '  * IsSame() reports them as different.'

  print '  Golden:'
  print '  ' + golden_srs.ExportToPrettyWkt()
  print '  New:'
  print '  ' + new_srs.ExportToPrettyWkt()

  return 1

#######################################################
def compare_db(golden_db, new_db):
  found_diff = 0

  # SRS
  found_diff += compare_srs(golden_db.GetProjection(),
                            new_db.GetProjection())

  # GeoTransform
  golden_gt = golden_db.GetGeoTransform()
  new_gt = new_db.GetGeoTransform()
  if golden_gt != new_gt:
    print 'GeoTransforms Differ:'
    print '  Golden:', golden_gt
    print '  New:   ', new_gt
    found_diff += 1

  # Metadata
  found_diff += compare_metadata(golden_db.GetMetadata(),
                                 new_db.GetMetadata(),
                                 'Dataset')

  # Bands
  if golden_db.RasterCount != new_db.RasterCount:
    print 'Band count mismatch (golden=%d, new=%d)' \
        % (golden_db.RasterCount, new_db.RasterCount)
    found_diff += 1

  else:
    for i in range(golden_db.RasterCount):
      found_diff += compare_band(golden_db.GetRasterBand(i+1),
                                 new_db.GetRasterBand(i+1),
                                 str(i+1))

  return found_diff

#######################################################
def Usage():
  print 'Usage: gdalcompare.py <golden_file> <new_file>'
  sys.exit(1)

#######################################################
#
# Mainline
#

if __name__ == '__main__':

  # Default GDAL argument parsing.
  argv = gdal.GeneralCmdLineProcessor( sys.argv )
  if argv is None:
    sys.exit( 0 )

  if len(argv) == 1:
    Usage()

  # Script argument parsing.
  golden_file = None
  new_file = None

  i = 1
  while  i < len(argv):

    if golden_file is None:
      golden_file = argv[i]

    elif new_file is None:
      new_file = argv[i]

    else:
      print('Urecognised argument: ' + argv[i])
      Usage()

    i = i + 1
    # next argument

  #### Compare Files ####

  found_diff = 0

  # compare raw binary files.
  if not filecmp.cmp(golden_file,new_file):
    print 'Files differ at the binary level.'
    found_diff += 1

  # compare as GDAL Datasets.
  golden_db = gdal.Open(golden_file)
  new_db = gdal.Open(new_file)
  found_diff += compare_db(golden_db, new_db)

  print 'Differences Found:',found_diff

  sys.exit(found_diff)
