# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Python Library supporting GDAL/OGR Test Suite
# Author:   James McClain <james.mcclain@gmail.com>
#
###############################################################################
# Copyright (c) 2018, Dr. James McClain <james.mcclain@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import os
from osgeo import gdal


def uffd_compare(filename):
    """Compare the same file opened with and without the userfaultfd.

    This function reads a file using the standard filesystem-based
    mechanism and also using VSIL (and the userfaultfd mechanism for
    drivers which cannot use VSIL except with it, which are the ones
    of interest).  The two datasets are compared to each other in
    various ways.

    Args:
        filename: The name of the file within the
             'autotest/gdrivers/data' directory to use for testing.

    Returns:
         `None` if the dataset could not be opened through the VSIL
         mechanism. `True` is returned if the datasets were found to
         be identical after various comparisons.  `False` otherwise.

    """
    ext = os.path.splitext(filename)[1]
    vsimem = '/vsimem/file{}'.format(ext)
    filename2 = './data/{}'.format(filename)
    gdal.FileFromMemBuffer(vsimem, open(filename2, 'rb').read())

    dataset1 = gdal.Open(filename2)
    dataset2 = gdal.Open(vsimem)
    if dataset2 is None:
        gdal.Unlink(vsimem)
        return None
    if (dataset1.GetMetadata() != dataset2.GetMetadata()):
        gdal.Unlink(vsimem)
        return False
    if (dataset1.RasterCount != dataset2.RasterCount):
        gdal.Unlink(vsimem)
        return False
    for i in range(dataset1.RasterCount):
        checksum1 = dataset1.GetRasterBand(i+1).Checksum()
        checksum2 = dataset2.GetRasterBand(i+1).Checksum()
        if not checksum1 == checksum2:
            gdal.Unlink(vsimem)
            return False

    gdal.Unlink(vsimem)
    return True
