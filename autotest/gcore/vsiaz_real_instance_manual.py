#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiaz
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017 Even Rouault <even dot rouault at spatialys dot com>
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

import stat
from osgeo import gdal


import gdaltest
import pytest

pytestmark = pytest.mark.skipif(not gdaltest.built_against_curl(), reason="GDAL not built against curl")


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################
# Nominal cases (require valid credentials)


def test_vsiaz_extra_1():

    if not gdaltest.built_against_curl():
        pytest.skip()

    az_resource = gdal.GetConfigOption('AZ_RESOURCE')
    if az_resource is None:
        pytest.skip('Missing AZ_RESOURCE')

    if '/' not in az_resource:
        path = '/vsiaz/' + az_resource
        statres = gdal.VSIStatL(path)
        assert statres is not None and stat.S_ISDIR(statres.mode), \
            ('%s is not a valid bucket' % path)

        readdir = gdal.ReadDir(path)
        assert readdir is not None, 'ReadDir() should not return empty list'
        for filename in readdir:
            if filename != '.':
                subpath = path + '/' + filename
                assert gdal.VSIStatL(subpath) is not None, \
                    ('Stat(%s) should not return an error' % subpath)

        unique_id = 'vsiaz_test'
        subpath = path + '/' + unique_id
        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, ('Mkdir(%s) should not return an error' % subpath)

        readdir = gdal.ReadDir(path)
        assert unique_id in readdir, \
            ('ReadDir(%s) should contain %s' % (path, unique_id))

        ret = gdal.Mkdir(subpath, 0)
        assert ret != 0, ('Mkdir(%s) repeated should return an error' % subpath)

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, ('Rmdir(%s) should not return an error' % subpath)

        readdir = gdal.ReadDir(path)
        assert unique_id not in readdir, \
            ('ReadDir(%s) should not contain %s' % (path, unique_id))

        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, ('Mkdir(%s) should not return an error' % subpath)

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'wb')
        assert f is not None
        gdal.VSIFWriteL('hello', 1, 5, f)
        gdal.VSIFCloseL(f)

        ret = gdal.Rmdir(subpath)
        assert ret != 0, \
            ('Rmdir(%s) on non empty directory should return an error' % subpath)

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'rb')
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode('utf-8')
        assert data == 'hello'
        gdal.VSIFCloseL(f)

        md = gdal.GetFileMetadata(subpath + '/test.txt', 'HEADERS')
        assert 'x-ms-blob-type' in md

        metadata_md = gdal.GetFileMetadata(subpath + '/test.txt', 'METADATA')
        assert 'ETag' in metadata_md or 'etag' in metadata_md
        assert metadata_md != md

        md = gdal.GetFileMetadata(subpath + '/test.txt', 'TAGS')
        assert md == {}

        # Change properties
        assert gdal.SetFileMetadata(subpath + '/test.txt', {'x-ms-blob-content-type' : 'foo'}, 'PROPERTIES')
        md = gdal.GetFileMetadata(subpath + '/test.txt', 'HEADERS')
        assert md.get('Content-Type', '') == 'foo' or md.get('content-type', '') == 'foo', md

        # Change metadata
        assert gdal.SetFileMetadata(subpath + '/test.txt', {'x-ms-meta-FOO' : 'BAR'}, 'METADATA')
        md = gdal.GetFileMetadata(subpath + '/test.txt', 'METADATA')
        assert md['x-ms-meta-FOO'] == 'BAR'

        # Change tags (doesn't seem to work with Azurite)
        if ':10000/devstoreaccount1' not in gdal.GetConfigOption('AZURE_STORAGE_CONNECTION_STRING', ''):
            assert gdal.SetFileMetadata(subpath + '/test.txt', {'BAR' : 'BAZ'}, 'TAGS')
            md = gdal.GetFileMetadata(subpath + '/test.txt', 'TAGS')
            assert md['BAR'] == 'BAZ'

        assert gdal.Rename(subpath + '/test.txt', subpath + '/test2.txt') == 0

        f = gdal.VSIFOpenL(subpath + '/test2.txt', 'rb')
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode('utf-8')
        assert data == 'hello'
        gdal.VSIFCloseL(f)

        ret = gdal.Unlink(subpath + '/test2.txt')
        assert ret >= 0, \
            ('Unlink(%s) should not return an error' % (subpath + '/test2.txt'))

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, ('Rmdir(%s) should not return an error' % subpath)

        return

    f = open_for_read('/vsiaz/' + az_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    # Same with /vsiaz_streaming/
    f = open_for_read('/vsiaz_streaming/' + az_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    if False:  # pylint: disable=using-constant-test
        # we actually try to read at read() time and bSetError = false
        # Invalid bucket : "The specified bucket does not exist"
        gdal.ErrorReset()
        f = open_for_read('/vsiaz/not_existing_bucket/foo')
        with gdaltest.error_handler():
            gdal.VSIFReadL(1, 1, f)
        gdal.VSIFCloseL(f)
        assert gdal.VSIGetLastErrorMsg() != ''

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsiaz_streaming/' + az_resource + '/invalid_resource.baz')
    assert f is None, gdal.VSIGetLastErrorMsg()

    # Test GetSignedURL()
    signed_url = gdal.GetSignedURL('/vsiaz/' + az_resource)
    f = open_for_read('/vsicurl_streaming/' + signed_url)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1
