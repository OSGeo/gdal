#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Python threading
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
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

import threading

try:
    import numpy  # noqa
    numpy_available = True
except ImportError:
    numpy_available = False

from osgeo import gdal
import pytest


def my_error_handler(err_type, err_no, err_msg):
    # pylint: disable=unused-argument
    pass


def thread_test_1_worker(args_dict):
    for i in range(1000):
        ds = gdal.Open('data/byte.tif')
        if (i % 2) == 0:
            if ds.GetRasterBand(1).Checksum() != 4672:
                args_dict['ret'] = False
        else:
            ds.GetRasterBand(1).ReadAsArray()
    for i in range(1000):
        gdal.PushErrorHandler(my_error_handler)
        ds = gdal.Open('i_dont_exist')
        gdal.PopErrorHandler()


def test_thread_test_1():

    if not numpy_available:
        pytest.skip()

    threads = []
    args_array = []
    for i in range(4):
        args_dict = {'ret': True}
        t = threading.Thread(target=thread_test_1_worker, args=(args_dict,))
        args_array.append(args_dict)
        threads.append(t)
        t.start()

    ret = 'success'
    for i in range(4):
        threads[i].join()
        if not args_array[i]:
            ret = 'fail'

    return ret




