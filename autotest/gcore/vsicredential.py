#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VSI credentials
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2022 Even Rouault <even dot rouault at spatialys dot com>
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

from osgeo import gdal

import pytest

def test_vsicredential():

    with pytest.raises(Exception):
        assert gdal.GetCredential(None, 'key')

    with pytest.raises(Exception):
        assert gdal.GetCredential('prefix', None)

    assert gdal.GetCredential('prefix', 'key') is None

    assert gdal.GetCredential('prefix', 'key', 'default') == 'default'

    with pytest.raises(Exception):
        gdal.SetCredential(None, 'key', 'value')

    with pytest.raises(Exception):
        gdal.SetCredential('prefix', None, 'value')

    gdal.SetCredential('prefix', 'key', 'value')
    assert gdal.GetCredential('prefix', 'key') == 'value'
    assert gdal.GetCredential('prefix/object', 'key') == 'value'
    assert gdal.GetCredential('prefix', 'key', 'default') == 'value'
    assert gdal.GetCredential('another_prefix', 'key') is None

    gdal.SetCredential('prefix', 'key', None)
    assert gdal.GetCredential('prefix', 'key') is None

    gdal.SetCredential('prefix', 'key', 'value')
    gdal.ClearCredentials('prefix')
    assert gdal.GetCredential('prefix', 'key') is None

    gdal.SetCredential('prefix', 'key', 'value')
    gdal.ClearCredentials('another_prefix')
    assert gdal.GetCredential('prefix', 'key') == 'value'
    gdal.ClearCredentials()
    assert gdal.GetCredential('prefix', 'key') is None
