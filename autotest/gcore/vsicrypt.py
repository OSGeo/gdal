#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicrypt/
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

import ctypes
import os
import struct

from osgeo import gdal

import gdaltest
import pytest

from gcore.testnonboundtoswig import setup as testnonboundtoswig_setup  # noqa
testnonboundtoswig_setup; # to please pyflakes

###############################################################################
# Use common test for /vsicrypt


def test_vsicrypt_1():

    gdaltest.has_vsicrypt = False
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb+')
    if fp is None:
        pytest.skip()
    gdal.VSIFCloseL(fp)
    gdal.Unlink('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin')
    gdaltest.has_vsicrypt = True

    import vsifile
    return vsifile.vsifile_generic('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin')

###############################################################################
# Test various error cases


def test_vsicrypt_2():

    if not gdaltest.has_vsicrypt:
        pytest.skip()

    # Missing key
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    assert fp is None

    # Invalid file
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'wb')
    assert fp is None

    # Invalid file
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'rb')
    assert fp is None

    # Invalid file
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'ab')
    assert fp is None

    # Invalid access
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'foo')
    assert fp is None

    # Key to short
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=a,file=/vsimem/file.bin', 'wb+')
    assert fp is None

    # Invalid signature
    gdal.FileFromMemBuffer('/vsimem/file.bin', 'foo')
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    assert fp is None

    # Generate empty file
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb')
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    assert fp is not None
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsimem/file.bin', 'rb')
    header = gdal.VSIFReadL(1, 1000, fp)
    gdal.VSIFCloseL(fp)

    assert len(header) == 46

    # Test shortening header
    for i in range(46):
        fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
        gdal.VSIFWriteL(header, 1, 46 - 1 - i, fp)
        gdal.VSIFCloseL(fp)

        with gdaltest.error_handler():
            fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
        assert fp is None

    # Test corrupting all bytes of header
    for i in range(46):
        for val in (0, 127, 255):
            fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
            try:
                new_byte = chr(val).encode('latin1')
            except (UnicodeDecodeError, UnicodeEncodeError):
                new_byte = chr(val)
            header_new = header[0:i] + new_byte + header[i + 1:]
            gdal.VSIFWriteL(header_new, 1, 46, fp)
            gdal.VSIFCloseL(fp)

            with gdaltest.error_handler():
                fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file='
                                    '/vsimem/file.bin', 'rb')
            if fp is not None:
                gdal.VSIFCloseL(fp)

    gdal.SetConfigOption('VSICRYPT_IV', 'TOO_SHORT')
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file='
                            '/vsimem/file.bin', 'wb')
    gdal.SetConfigOption('VSICRYPT_IV', None)
    if fp is not None:
        gdal.VSIFCloseL(fp)

    # Inconsistent initial vector.
    header = struct.pack('B' * 38,
                         86, 83, 73, 67, 82, 89, 80, 84,  # signature
                         38, 0,  # header size
                         1,  # major
                         0,  # minor
                         0, 2,  # sector size
                         0,  # alg
                         0,  # mode
                         8,  # size of IV (should be 16)
                         32, 13, 169, 71, 154, 208, 22, 32,  # IV
                         0, 0,  # size of free text
                         0,  # size of key check
                         0, 0, 0, 0, 0, 0, 0, 0,  # size of unencrypted file
                         0, 0  # size of extra content
                        )
    fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL(header, 1, len(header), fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL(
            '/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    assert fp is None

    # Inconsistent initial vector with key check.
    header = struct.pack('B' * 39,
                         86, 83, 73, 67, 82, 89, 80, 84,  # signature
                         39, 0,  # header size
                         1,  # major
                         0,  # minor
                         0, 2,  # sector size
                         0,  # alg
                         0,  # mode
                         8,  # size of IV (should be 16)
                         32, 13, 169, 71, 154, 208, 22, 32,  # IV
                         0, 0,  # size of free text
                         1,  # size of key check
                         0,  # key check
                         0, 0, 0, 0, 0, 0, 0, 0,  # size of unencrypted file
                         0, 0  # size of extra content
                        )
    fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL(header, 1, len(header), fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    assert fp is None

    # Test reading with wrong key
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsicrypt/key=dont_use_in_prod,file=/vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    assert content != 'hello'

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=short_key,file=/vsimem/file.bin', 'ab')
    assert fp is None

    # Test reading with wrong key with add_key_check
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,add_key_check=yes,file=/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=dont_use_in_prod,file=/vsimem/file.bin', 'rb')
    assert fp is None

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=short_key,file=/vsimem/file.bin', 'ab')
    assert fp is None

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=dont_use_in_prod,file=/vsimem/file.bin', 'ab')
    assert fp is None

    # Test creating with potentially not built-in alg:
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/alg=blowfish,key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb')
    if fp is not None:
        gdal.VSIFCloseL(fp)

    # Invalid sector_size
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,sector_size=1,file=/vsimem/file.bin', 'wb')
    assert fp is None

    # Sector size (16) should be at least twice larger than the block size (16) in CBC_CTS
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,sector_size=16,mode=CBC_CTS,file=/vsimem/file.bin', 'wb')
    assert fp is None

    gdal.Unlink('/vsimem/file.bin')

###############################################################################
# Test various options

@pytest.mark.skipif(
    os.environ.get('BUILD_NAME', '') == 's390x',
    reason='Fails randomly on that platform'
)
def test_vsicrypt_3():

    if not gdaltest.has_vsicrypt:
        pytest.skip()

    for options in ['sector_size=16', 'alg=AES', 'alg=DES_EDE2', 'alg=DES_EDE3', 'alg=SKIPJACK', 'alg=invalid',
                    'mode=CBC', 'mode=CFB', 'mode=OFB', 'mode=CTR', 'mode=CBC_CTS', 'mode=invalid',
                    'freetext=my_free_text',
                    'add_key_check=yes']:

        gdal.Unlink('/vsimem/file.bin')

        if options == 'alg=invalid' or options == 'mode=invalid':
            with gdaltest.error_handler():
                fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PRODDONT_USE_IN_PROD,%s,file=/vsimem/file.bin' % options, 'wb')
        else:
            fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PRODDONT_USE_IN_PROD,%s,file=/vsimem/file.bin' % options, 'wb')
        assert fp is not None, options
        gdal.VSIFWriteL('hello', 1, 5, fp)
        gdal.VSIFCloseL(fp)

        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PRODDONT_USE_IN_PROD,file=/vsimem/file.bin', 'r')
        content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
        gdal.VSIFCloseL(fp)

        assert content == 'hello', options

    # Some of those algs might be missing
    for options in ['alg=Blowfish', 'alg=Camellia', 'alg=CAST256', 'alg=MARS', 'alg=IDEA', 'alg=RC5', 'alg=RC6', 'alg=Serpent', 'alg=SHACAL2', 'alg=Twofish', 'alg=XTEA']:

        gdal.Unlink('/vsimem/file.bin')

        with gdaltest.error_handler():
            fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,%s,file=/vsimem/file.bin' % options, 'wb')
        if fp is not None:
            gdal.VSIFWriteL('hello', 1, 5, fp)
            gdal.VSIFCloseL(fp)

            fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
            content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
            gdal.VSIFCloseL(fp)

            assert content == 'hello', options

    # Test key generation

    # Do NOT set VSICRYPT_CRYPTO_RANDOM=NO in production. This is just to speed up tests !
    gdal.SetConfigOption("VSICRYPT_CRYPTO_RANDOM", "NO")
    fp = gdal.VSIFOpenL('/vsicrypt/key=GENERATE_IT,add_key_check=yes,file=/vsimem/file.bin', 'wb')
    gdal.SetConfigOption("VSICRYPT_CRYPTO_RANDOM", None)

    # Get the generated random key
    key_b64 = gdal.GetConfigOption('VSICRYPT_KEY_B64')
    assert key_b64 is not None

    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    assert content == 'hello', options

    gdal.SetConfigOption('VSICRYPT_KEY_B64', None)

    fp = gdal.VSIFOpenL('/vsicrypt/key_b64=%s,file=/vsimem/file.bin' % key_b64, 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    assert content == 'hello', options

    with gdaltest.error_handler():
        statRes = gdal.VSIStatL('/vsicrypt//vsimem/file.bin')
    assert statRes is None

    ret = gdal.Rename('/vsicrypt//vsimem/file.bin', '/vsicrypt//vsimem/subdir_crypt/file.bin')
    assert ret == 0

    ret = gdal.Rename('/vsicrypt//vsimem/subdir_crypt/file.bin', '/vsimem/subdir_crypt/file2.bin')
    assert ret == 0

    dir_content = gdal.ReadDir('/vsicrypt//vsimem/subdir_crypt')
    assert dir_content == ['file2.bin']

    gdal.Unlink('/vsimem/subdir_crypt/file2.bin')

###############################################################################
# Test "random" operations against reference filesystem


def test_vsicrypt_4():

    if not gdaltest.has_vsicrypt:
        pytest.skip()

    test_file = '/vsicrypt/key=DONT_USE_IN_PROD,sector_size=32,file=/vsimem/file_enc.bin'
    ref_file = '/vsimem/file.bin'

    for seed in range(1000):

        gdal.Unlink(test_file)
        gdal.Unlink(ref_file)

        test_f = gdal.VSIFOpenL(test_file, 'wb+')
        ref_f = gdal.VSIFOpenL(ref_file, 'wb+')

        import random
        random.seed(seed)

        for _ in range(20):
            random_offset = random.randint(0, 1000)
            gdal.VSIFSeekL(test_f, random_offset, 0)
            gdal.VSIFSeekL(ref_f, random_offset, 0)

            random_size = random.randint(1, 80)
            random_content = ''.join([chr(40 + int(10 * random.random())) for _ in range(random_size)])
            gdal.VSIFWriteL(random_content, 1, random_size, test_f)
            gdal.VSIFWriteL(random_content, 1, random_size, ref_f)

            if random.randint(0, 1) == 0:
                random_offset = random.randint(0, 1500)
                gdal.VSIFSeekL(test_f, random_offset, 0)
                gdal.VSIFSeekL(ref_f, random_offset, 0)

                random_size = random.randint(1, 80)
                test_content = gdal.VSIFReadL(1, random_size, test_f)
                ref_content = gdal.VSIFReadL(1, random_size, ref_f)
                if test_content != ref_content:
                    print(seed)
                    print('Test content (%d):' % len(test_content))
                    print('')
                    pytest.fail('Ref content (%d):' % len(ref_content))

        gdal.VSIFSeekL(test_f, 0, 0)
        gdal.VSIFSeekL(ref_f, 0, 0)
        test_content = gdal.VSIFReadL(1, 100000, test_f)
        ref_content = gdal.VSIFReadL(1, 100000, ref_f)

        gdal.VSIFCloseL(test_f)
        gdal.VSIFCloseL(ref_f)

        if test_content != ref_content:
            print(seed)
            print('Test content (%d):' % len(test_content))
            print('')
            pytest.fail('Ref content (%d):' % len(ref_content))

    gdal.Unlink(test_file)
    gdal.Unlink(ref_file)

###############################################################################
# Test random filling of last sector


def test_vsicrypt_5():

    if not gdaltest.has_vsicrypt:
        pytest.skip()

    test_file = '/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file_enc.bin'

    f = gdal.VSIFOpenL(test_file, 'wb+')
    gdal.VSIFWriteL('ab', 1, 2, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL(test_file, 'rb+')
    gdal.VSIFSeekL(f, 3, 0)
    gdal.VSIFWriteL('d', 1, 1, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL(test_file, 'rb')
    content = gdal.VSIFReadL(1, 4, f)
    content = struct.unpack('B' * len(content), content)
    gdal.VSIFCloseL(f)
    assert content == (97, 98, 0, 100)

    f = gdal.VSIFOpenL(test_file, 'rb+')
    gdal.VSIFReadL(1, 1, f)
    gdal.VSIFSeekL(f, 5, 0)
    gdal.VSIFWriteL('f', 1, 1, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL(test_file, 'rb')
    content = gdal.VSIFReadL(1, 6, f)
    content = struct.unpack('B' * len(content), content)
    gdal.VSIFCloseL(f)
    assert content == (97, 98, 0, 100, 0, 102)

    f = gdal.VSIFOpenL(test_file, 'rb+')
    gdal.VSIFReadL(1, 1, f)
    gdal.VSIFSeekL(f, 512, 0)
    gdal.VSIFWriteL('Z', 1, 1, f)
    gdal.VSIFSeekL(f, 7, 0)
    gdal.VSIFWriteL('h', 1, 1, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL(test_file, 'rb')
    content = gdal.VSIFReadL(1, 8, f)
    content = struct.unpack('B' * len(content), content)
    gdal.VSIFCloseL(f)
    assert content == (97, 98, 0, 100, 0, 102, 0, 104)

    gdal.Unlink(test_file)

###############################################################################
# Test VSISetCryptKey


def test_vsicrypt_6(testnonboundtoswig_setup):  # noqa

    testnonboundtoswig_setup.VSISetCryptKey.argtypes = [ctypes.c_char_p, ctypes.c_int]
    testnonboundtoswig_setup.VSISetCryptKey.restype = None

    # Set a valid key
    testnonboundtoswig_setup.VSISetCryptKey('DONT_USE_IN_PROD'.encode('ASCII'), 16)

    if not gdaltest.has_vsicrypt:
        pytest.skip()

    fp = gdal.VSIFOpenL('/vsicrypt/add_key_check=yes,file=/vsimem/file.bin', 'wb+')
    assert fp is not None
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    assert content == 'hello'

    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    assert fp is not None
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    assert content == 'hello'

    # Set a too short key
    testnonboundtoswig_setup.VSISetCryptKey('bbc'.encode('ASCII'), 3)
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    assert fp is None

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    assert fp is None

    # Erase key
    testnonboundtoswig_setup.VSISetCryptKey(None, 0)
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    assert fp is None

    gdal.Unlink('/vsimem/file.bin')



