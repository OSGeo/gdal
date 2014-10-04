#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
def compare_metadata(golden_md, new_md, id, options=[]):
  if golden_md is None and new_md is None:
    return 0

  found_diff = 0

  if len(list(golden_md.keys())) != len(list(new_md.keys())):
    print('Difference in %s metadata key count' % id)
    print('  Golden Keys: ' + str(list(golden_md.keys())))
    print('  New Keys: ' + str(list(new_md.keys())))
    found_diff += 1

  for key in list(golden_md.keys()):
    if key not in new_md:
      print('New %s metadata lacks key \"%s\"' % (id, key))
      found_diff += 1
    elif new_md[key] != golden_md[key]:
      print('Metadata value difference for key "' + key + '"')
      print('  Golden: "' + golden_md[key] + '"')
      print('  New:    "' + new_md[key] + '"')
      found_diff += 1

  return found_diff


#######################################################
# Review and report on the actual image pixels that differ.
def compare_image_pixels(golden_band, new_band, id, options=[]):
  diff_count = 0
  max_diff = 0

  for line in range(golden_band.YSize):
    golden_line = golden_band.ReadAsArray(0, line, golden_band.XSize, 1)[0]
    new_line = new_band.ReadAsArray(0, line, golden_band.XSize, 1)[0]
    diff_line = golden_line.astype(float) - new_line.astype(float)
    max_diff = max(max_diff,abs(diff_line).max())
    diff_count += len(diff_line.nonzero()[0])

  print('  Pixels Differing: ' + str(diff_count))
  print('  Maximum Pixel Difference: ' + str(max_diff))

#######################################################
def compare_band(golden_band, new_band, id, options=[]):
  found_diff = 0

  if golden_band.DataType != new_band.DataType:
    print('Band %s pixel types differ.' % id)
    print('  Golden: ' + gdal.GetDataTypeName(golden_band.DataType))
    print('  New:    ' + gdal.GetDataTypeName(new_band.DataType))
    found_diff += 1

  if golden_band.GetNoDataValue() != new_band.GetNoDataValue():
    print('Band %s nodata values differ.' % id)
    print('  Golden: ' + str(golden_band.GetNoDataValue()))
    print('  New:    ' + str(new_band.GetNoDataValue()))
    found_diff += 1

  if golden_band.GetColorInterpretation() != new_band.GetColorInterpretation():
    print('Band %s color interpretation values differ.' % id)
    print('  Golden: ' +  gdal.GetColorInterpretationName(golden_band.GetColorInterpretation()))
    print('  New:    ' + gdal.GetColorInterpretationName(new_band.GetColorInterpretation()))
    found_diff += 1

  if golden_band.Checksum() != new_band.Checksum():
    print('Band %s checksum difference:' % id)
    print('  Golden: ' + str(golden_band.Checksum()))
    print('  New:    ' + str(new_band.Checksum()))
    found_diff += 1
    compare_image_pixels(golden_band,new_band, id, options)

  # Check overviews
  if golden_band.GetOverviewCount() != new_band.GetOverviewCount():
    print('Band %s overview count difference:' % id)
    print('  Golden: ' + str(golden_band.GetOverviewCount()))
    print('  New:    ' + str(new_band.GetOverviewCount()))
    found_diff += 1
  else:
    for i in range(golden_band.GetOverviewCount()):
      found_diff += compare_band(golden_band.GetOverview(i),
                   new_band.GetOverview(i),
                   id + ' overview ' + str(i), 
                   options)

  # Metadata
  if 'SKIP_METADATA' not in options:
      found_diff += compare_metadata(golden_band.GetMetadata(),
                                     new_band.GetMetadata(),
                                     'Band ' + id, options)

  # TODO: Color Table, gain/bias, units, blocksize, mask, min/max

  return found_diff

#######################################################
def compare_srs(golden_wkt, new_wkt):
  if golden_wkt == new_wkt:
    return 0

  print('Difference in SRS!')

  golden_srs = osr.SpatialReference(golden_wkt)
  new_srs = osr.SpatialReference(new_wkt)

  if golden_srs.IsSame(new_srs):
    print('  * IsSame() reports them as equivelent.')
  else:
    print('  * IsSame() reports them as different.')

  print('  Golden:')
  print('  ' + golden_srs.ExportToPrettyWkt())
  print('  New:')
  print('  ' + new_srs.ExportToPrettyWkt())

  return 1

#######################################################
def compare_db(golden_db, new_db, options=[]):
  found_diff = 0

  # SRS
  if 'SKIP_SRS' not in options:
      found_diff += compare_srs(golden_db.GetProjection(),
                                new_db.GetProjection())

  # GeoTransform
  if 'SKIP_GEOTRANSFORM' not in options:
      golden_gt = golden_db.GetGeoTransform()
      new_gt = new_db.GetGeoTransform()
      if golden_gt != new_gt:
          print('GeoTransforms Differ:')
          print('  Golden: ' + str(golden_gt))
          print('  New:    ' + str(new_gt))
          found_diff += 1

  # Metadata
  if 'SKIP_METADATA' not in options:
      found_diff += compare_metadata(golden_db.GetMetadata(),
                                     new_db.GetMetadata(),
                                     'Dataset', options)

  # Bands
  if golden_db.RasterCount != new_db.RasterCount:
    print('Band count mismatch (golden=%d, new=%d)' \
        % (golden_db.RasterCount, new_db.RasterCount))
    found_diff += 1
  
  # Dimensions
  for i in range(golden_db.RasterCount):
      gSzX = golden_db.GetRasterBand(i+1).XSize
      nSzX = new_db.GetRasterBand(i+1).XSize
      gSzY = golden_db.GetRasterBand(i+1).YSize
      nSzY = new_db.GetRasterBand(i+1).YSize
      
      if gSzX != nSzX or gSzY != nSzY:
          print('Band size mismatch (band=%d golden=[%d,%d], new=[%d,%d])' %
                (i, gSzX, gSzY, nSzX, nSzY))
          found_diff += 1

  # If so-far-so-good, then compare pixels
  if found_diff == 0:
    for i in range(golden_db.RasterCount):
      found_diff += compare_band(golden_db.GetRasterBand(i+1),
                                 new_db.GetRasterBand(i+1),
                                 str(i+1),
                                 options)

  return found_diff

#######################################################
def compare_sds(golden_db, new_db, options=[]):
  found_diff = 0
  
  golden_sds = golden_db.GetMetadata('SUBDATASETS')
  new_sds = new_db.GetMetadata('SUBDATASETS')

  count = len(list(golden_sds.keys())) / 2
  for i in range(count):
    key = 'SUBDATASET_%d_NAME' % (i+1)

    sub_golden_db = gdal.Open(golden_sds[key])
    sub_new_db = gdal.Open(new_sds[key])

    sds_diff = compare_db(sub_golden_db, sub_new_db, options)
    found_diff += sds_diff
    if sds_diff > 0:
      print('%d differences found between:\n  %s\n  %s' \
            % (sds_diff, golden_sds[key],new_sds[key]))

  return found_diff
  
#######################################################
def Usage():
  print('Usage: gdalcompare.py [-sds] <golden_file> <new_file>')
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
  check_sds = 0

  i = 1
  while  i < len(argv):

    if argv[i] == '-sds':
      check_sds = 1

    elif golden_file is None:
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
  try:
    os.stat(golden_file)
    
    if not filecmp.cmp(golden_file,new_file):
      print('Files differ at the binary level.')
      found_diff += 1
  except:
    print('Skipped binary file comparison, golden file not in filesystem.')

  # compare as GDAL Datasets.
  golden_db = gdal.Open(golden_file)
  new_db = gdal.Open(new_file)
  found_diff += compare_db(golden_db, new_db)

  if check_sds:
    found_diff += compare_sds(golden_db, new_db)
    
  print('Differences Found: ' + str(found_diff))

  sys.exit(found_diff)
