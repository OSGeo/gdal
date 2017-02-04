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


def validate(filename, check_tiled=True):
    """ Return if the passed file is a (Geo)TIFF file with a cloud
        optimized compatible structure, otherwise raise a
        ValidateCloudOptimizedGeoTIFFException exception. """

    if int(gdal.VersionInfo('VERSION_NUM')) < 2020000:
        raise ValidateCloudOptimizedGeoTIFFException(
            "GDAL 2.2 or above required")

    gdal.PushErrorHandler()
    ds = gdal.Open(filename)
    gdal.PopErrorHandler()
    if ds is None:
        raise ValidateCloudOptimizedGeoTIFFException(
            "Invalid file : %s" % gdal.GetLastErrorMsg())
    if ds.GetDriver().ShortName != 'GTiff':
        raise ValidateCloudOptimizedGeoTIFFException(
            "The file is not a GeoTIFF")

    main_band = ds.GetRasterBand(1)
    ovr_count = main_band.GetOverviewCount()
    if filename + '.ovr' in ds.GetFileList():
        raise ValidateCloudOptimizedGeoTIFFException(
            "Overviews should be internal")

    if check_tiled:
        block_size = main_band.GetBlockSize()
        if block_size[0] == main_band.XSize:
            raise ValidateCloudOptimizedGeoTIFFException(
                "Full resolution image is not tiled")

    if main_band.XSize >= 512 or main_band.YSize >= 512:
        if ovr_count == 0:
            raise ValidateCloudOptimizedGeoTIFFException(
                "The file should have overviews")

    ifd_offset = \
        [int(main_band.GetMetadataItem('IFD_OFFSET', 'TIFF'))]
    if ifd_offset[0] != 8:
        raise ValidateCloudOptimizedGeoTIFFException(
            "The offset of the main IFD should be 8. It is %d instead" %
            ifd_offset[0])

    for i in range(ovr_count):
        # Check that overviews are by descending sizes
        ovr_band = ds.GetRasterBand(1).GetOverview(i)
        if i == 0:
            if ovr_band.XSize > main_band.XSize or \
               ovr_band.YSize > main_band.YSize:
                    raise ValidateCloudOptimizedGeoTIFFException(
                       "First overview has larger dimension than main band")
        else:
            prev_ovr_band = ds.GetRasterBand(1).GetOverview(i-1)
            if ovr_band.XSize > prev_ovr_band.XSize or \
               ovr_band.YSize > prev_ovr_band.YSize:
                    raise ValidateCloudOptimizedGeoTIFFException(
                       "Overview of index %d has larger dimension than "
                       "overview of index %d" % (i, i-1))

        if check_tiled:
            block_size = ovr_band.GetBlockSize()
            if block_size[0] == ovr_band.XSize:
                raise ValidateCloudOptimizedGeoTIFFException(
                    "Overview of index %d is not tiled" % i)

        # Check that the IFD of descending overviews are sorted by increasing
        # offsets
        ifd_offset.append(int(ovr_band.GetMetadataItem('IFD_OFFSET', 'TIFF')))
        if ifd_offset[-1] < ifd_offset[-2]:
            if i == 0:
                raise ValidateCloudOptimizedGeoTIFFException(
                    "The offset of the IFD for overview of index %d is %d, "
                    "whereas it should be greater than the one of the main "
                    "image, which is at byte %d" %
                    (i, ifd_offset[-1], ifd_offset[-2]))
            else:
                raise ValidateCloudOptimizedGeoTIFFException(
                    "The offset of the IFD for overview of index %d is %d, "
                    "whereas it should be greater than the one of index %d, "
                    "which is at byte %d" %
                    (i, ifd_offset[-1], i-1, ifd_offset[-2]))

    # Check that the imagery starts by the smallest overview and ends with
    # the main resolution dataset
    data_offset = [int(main_band.GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF'))]
    for i in range(ovr_count):
        ovr_band = ds.GetRasterBand(1).GetOverview(i)
        data_offset.append(int(
            ovr_band.GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF')))

    if data_offset[-1] < ifd_offset[-1]:
        if ovr_count > 0:
            raise ValidateCloudOptimizedGeoTIFFException(
                "The offset of the first block of the smallest overview "
                "should be after its IFD")
        else:
            raise ValidateCloudOptimizedGeoTIFFException(
                "The offset of the first block of the image should "
                "be after its IFD")
    for i in range(len(data_offset)-2, 0, -1):
        if data_offset[i] < data_offset[i+1]:
            raise ValidateCloudOptimizedGeoTIFFException(
                "The offset of the first block of overview of index %d should "
                "be after the one of the overview of index %d" %
                (i-1, i))
    if len(data_offset) >= 2 and data_offset[0] < data_offset[1]:
        raise ValidateCloudOptimizedGeoTIFFException(
            "The offset of the first block of the main resolution image "
            "should be after the one of the overview of index %d" %
            (ovr_count - 1))


def main():
    """ Returns 0 in case of success """

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

        i = i + 1

    if filename is None:
        return Usage()

    try:
        validate(filename)
        ret = 0
        if not quiet:
            print('%s is a valid cloud optimized GeoTIFF' % filename)
    except ValidateCloudOptimizedGeoTIFFException as e:
        if not quiet:
            print('%s is NOT a valid cloud optimized GeoTIFF : %s' %
                  (filename, str(e)))
        ret = 1

    return ret


if __name__ == '__main__':
    sys.exit(main())
