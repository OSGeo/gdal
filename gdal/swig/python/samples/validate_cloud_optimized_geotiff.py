#!/usr/bin/env python
# -*- coding: utf-8 -*-
# *****************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Validate Cloud Optimized GeoTIFF file structure
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# *****************************************************************************
#  Copyright (c) 2017, Even Rouault
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
# *****************************************************************************

import sys
from osgeo import gdal


def Usage():
    print('Usage: validate_cloud_optimized_geotiff.py [-q] test.tif')
    print('')
    return 1


class ValidateCloudOptimizedGeoTIFFException(Exception):
    pass


def validate(ds, check_tiled=True):
    """Check if a file is a (Geo)TIFF with cloud optimized compatible structure.

    Args:
      ds: GDAL Dataset for the file to inspect.
      check_tiled: Set to False to ignore missing tiling.

    Returns:
      A tuple, whose first element is an array of error messages
      (empty if there is no error), and the second element, a dictionary
      with the structure of the GeoTIFF file.

    Raises:
      ValidateCloudOptimizedGeoTIFFException: Unable to open the file or the
        file is not a Tiff.
    """

    if int(gdal.VersionInfo('VERSION_NUM')) < 2020000:
        raise ValidateCloudOptimizedGeoTIFFException(
            'GDAL 2.2 or above required')

    unicode_type = type(''.encode('utf-8').decode('utf-8'))
    if isinstance(ds, (str, unicode_type)):
        gdal.PushErrorHandler()
        ds = gdal.Open(ds)
        gdal.PopErrorHandler()
        if ds is None:
            raise ValidateCloudOptimizedGeoTIFFException(
                'Invalid file : %s' % gdal.GetLastErrorMsg())
        if ds.GetDriver().ShortName != 'GTiff':
            raise ValidateCloudOptimizedGeoTIFFException(
                'The file is not a GeoTIFF')

    details = {}
    errors = []
    warnings = []
    filename = ds.GetDescription()
    main_band = ds.GetRasterBand(1)
    ovr_count = main_band.GetOverviewCount()
    filelist = ds.GetFileList()
    if filelist is not None and filename + '.ovr' in filelist:
        errors += [
            'Overviews found in external .ovr file. They should be internal']

    if main_band.XSize > 512 or main_band.YSize > 512:
        if check_tiled:
            block_size = main_band.GetBlockSize()
            if block_size[0] == main_band.XSize and block_size[0] > 1024:
                errors += [
                    'The file is greater than 512xH or Wx512, but is not tiled']

        if ovr_count == 0:
            warnings += [
                'The file is greater than 512xH or Wx512, it is recommended '
                'to include internal overviews']

    ifd_offset = int(main_band.GetMetadataItem('IFD_OFFSET', 'TIFF'))
    ifd_offsets = [ifd_offset]
    if ifd_offset not in (8, 16):
        errors += [
            'The offset of the main IFD should be 8 for ClassicTIFF '
            'or 16 for BigTIFF. It is %d instead' % ifd_offsets[0]]
    details['ifd_offsets'] = {}
    details['ifd_offsets']['main'] = ifd_offset

    for i in range(ovr_count):
        # Check that overviews are by descending sizes
        ovr_band = ds.GetRasterBand(1).GetOverview(i)
        if i == 0:
            if (ovr_band.XSize > main_band.XSize or
                    ovr_band.YSize > main_band.YSize):
                errors += [
                    'First overview has larger dimension than main band']
        else:
            prev_ovr_band = ds.GetRasterBand(1).GetOverview(i - 1)
            if (ovr_band.XSize > prev_ovr_band.XSize or
                    ovr_band.YSize > prev_ovr_band.YSize):
                errors += [
                    'Overview of index %d has larger dimension than '
                    'overview of index %d' % (i, i - 1)]

        if check_tiled:
            block_size = ovr_band.GetBlockSize()
            if block_size[0] == ovr_band.XSize and block_size[0] > 1024:
                errors += [
                    'Overview of index %d is not tiled' % i]

        # Check that the IFD of descending overviews are sorted by increasing
        # offsets
        ifd_offset = int(ovr_band.GetMetadataItem('IFD_OFFSET', 'TIFF'))
        ifd_offsets.append(ifd_offset)
        details['ifd_offsets']['overview_%d' % i] = ifd_offset
        if ifd_offsets[-1] < ifd_offsets[-2]:
            if i == 0:
                errors += [
                    'The offset of the IFD for overview of index %d is %d, '
                    'whereas it should be greater than the one of the main '
                    'image, which is at byte %d' %
                    (i, ifd_offsets[-1], ifd_offsets[-2])]
            else:
                errors += [
                    'The offset of the IFD for overview of index %d is %d, '
                    'whereas it should be greater than the one of index %d, '
                    'which is at byte %d' %
                    (i, ifd_offsets[-1], i - 1, ifd_offsets[-2])]

    # Check that the imagery starts by the smallest overview and ends with
    # the main resolution dataset
    block_offset = main_band.GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF')
    if not block_offset:
        errors += ['Missing BLOCK_OFFSET_0_0']
    data_offset = int(block_offset) if block_offset else None
    data_offsets = [data_offset]
    details['data_offsets'] = {}
    details['data_offsets']['main'] = data_offset
    for i in range(ovr_count):
        ovr_band = ds.GetRasterBand(1).GetOverview(i)
        data_offset = int(ovr_band.GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))
        data_offsets.append(data_offset)
        details['data_offsets']['overview_%d' % i] = data_offset

    if data_offsets[-1] < ifd_offsets[-1]:
        if ovr_count > 0:
            errors += [
                'The offset of the first block of the smallest overview '
                'should be after its IFD']
        else:
            errors += [
                'The offset of the first block of the image should '
                'be after its IFD']
    for i in range(len(data_offsets) - 2, 0, -1):
        if data_offsets[i] < data_offsets[i + 1]:
            errors += [
                'The offset of the first block of overview of index %d should '
                'be after the one of the overview of index %d' %
                (i - 1, i)]
    if len(data_offsets) >= 2 and data_offsets[0] < data_offsets[1]:
        errors += [
            'The offset of the first block of the main resolution image'
            'should be after the one of the overview of index %d' %
            (ovr_count - 1)]

    return warnings, errors, details


def main():
    """Return 0 in case of success, 1 for failure."""

    i = 1
    filename = None
    quiet = False
    while i < len(sys.argv):
        if sys.argv[i] == '-q':
            quiet = True
        elif sys.argv[i][0] == '-':
            return Usage()
        elif filename is None:
            filename = sys.argv[i]
        else:
            return Usage()

        i += 1

    if filename is None:
        return Usage()

    try:
        ret = 0
        warnings, errors, details = validate(filename)
        if warnings:
            if not quiet:
                print('The following warnings were found:')
                for warning in warnings:
                    print(' - ' + warning)
                print('')
        if errors:
            if not quiet:
                print('%s is NOT a valid cloud optimized GeoTIFF.' % filename)
                print('The following errors were found:')
                for error in errors:
                    print(' - ' + error)
                print('')
            ret = 1
        else:
            if not quiet:
                print('%s is a valid cloud optimized GeoTIFF' % filename)

        if not quiet and not warnings and not errors:
            print('\nThe size of all IFD headers is %d bytes' %
                  min(details['data_offsets'][k] for k in details['data_offsets']))
    except ValidateCloudOptimizedGeoTIFFException as e:
        if not quiet:
            print('%s is NOT a valid cloud optimized GeoTIFF : %s' %
                  (filename, str(e)))
        ret = 1

    return ret


if __name__ == '__main__':
    sys.exit(main())
