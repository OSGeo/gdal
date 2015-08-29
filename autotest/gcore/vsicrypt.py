#!/usr/bin/env python
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

from osgeo import gdal
import struct
import sys

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Use common test for /vsicrypt

def vsicrypt_1():
    
    gdaltest.has_vsicrypt = False
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb+')
    if fp is None:
        return 'skip'
    gdal.VSIFCloseL(fp)
    gdal.Unlink('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin')
    gdaltest.has_vsicrypt = True
    
    import vsifile
    return vsifile.vsifile_generic('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin')

###############################################################################
# Test various error cases

def vsicrypt_2():
    
    if not gdaltest.has_vsicrypt:
        return 'skip'

    # Missing key
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid file
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'wb')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid file
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'rb')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid file
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'ab')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid access
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/not_existing/not_existing', 'foo')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Key to short
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=a,file=/vsimem/file.bin', 'wb+')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid signature
    gdal.FileFromMemBuffer('/vsimem/file.bin', 'foo')
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Generate empty file
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb')
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    if fp is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(fp)
    
    fp = gdal.VSIFOpenL('/vsimem/file.bin', 'rb')
    header = gdal.VSIFReadL(1, 1000, fp)
    gdal.VSIFCloseL(fp)

    if len(header) != 46:
        gdaltest.post_reason('fail')
        print(len(header))
        return 'fail'

    # Test shortening header
    for i in range(46):
        fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
        gdal.VSIFWriteL(header, 1, 46 - 1 - i, fp)
        gdal.VSIFCloseL(fp)

        with gdaltest.error_handler():
            fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
        if fp is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test corrupting all bytes of header
    for i in range(46):
        for val in (0, 127, 255):
            fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
            try:
                new_byte = chr(val).encode('latin1')
            except:
                new_byte = chr(val)
            header_new = header[0:i] + new_byte + header[i+1:]
            gdal.VSIFWriteL(header_new, 1, 46, fp)
            gdal.VSIFCloseL(fp)
            
            with gdaltest.error_handler():
                fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
            if fp is not None:
                gdal.VSIFCloseL(fp)


    gdal.SetConfigOption('VSICRYPT_IV', 'TOO_SHORT')
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb')
    gdal.SetConfigOption('VSICRYPT_IV', None)
    if fp is not None:
        gdal.VSIFCloseL(fp)

    # Inconsistant initial vector
    header = struct.pack('B' * 38,
                       86, 83, 73, 67, 82, 89, 80, 84, # signature
                       38, 0, # header size
                       1, # major
                       0, # minor
                       0, 2, # sector size
                       0, # alg
                       0, # mode
                       8, #size of IV (should be 16)
                       32, 13, 169, 71, 154, 208, 22, 32, #IV
                       0, 0, # size of free text
                       0, # size of key check
                       0, 0, 0, 0, 0, 0, 0, 0, # size of unencrypted file
                       0, 0 # size of extra content
                       )
    fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL(header, 1, len(header), fp)
    gdal.VSIFCloseL(fp)
    
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Inconsistant initial vector with key check
    header = struct.pack('B' * 39,
                       86, 83, 73, 67, 82, 89, 80, 84, # signature
                       39, 0, # header size
                       1, # major
                       0, # minor
                       0, 2, # sector size
                       0, # alg
                       0, # mode
                       8, #size of IV (should be 16)
                       32, 13, 169, 71, 154, 208, 22, 32, #IV
                       0, 0, # size of free text
                       1, # size of key check
                       0, # key check
                       0, 0, 0, 0, 0, 0, 0, 0, # size of unencrypted file
                       0, 0 # size of extra content
                       )
    fp = gdal.VSIFOpenL('/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL(header, 1, len(header), fp)
    gdal.VSIFCloseL(fp)
    
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'rb')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'
        
    # Test reading with wrong key
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)
    
    fp = gdal.VSIFOpenL('/vsicrypt/key=dont_use_in_prod,file=/vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    if content == 'hello':
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=short_key,file=/vsimem/file.bin', 'ab')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test reading with wrong key with add_key_check
    fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,add_key_check=yes,file=/vsimem/file.bin', 'wb')
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)
    
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=dont_use_in_prod,file=/vsimem/file.bin', 'rb')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=short_key,file=/vsimem/file.bin', 'ab')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=dont_use_in_prod,file=/vsimem/file.bin', 'ab')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test creating with potentially not build-in alg:
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/alg=blowfish,key=DONT_USE_IN_PROD,file=/vsimem/file.bin', 'wb')
    if fp is not None:
        gdal.VSIFCloseL(fp)

    # Invalid sector_size
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,sector_size=1,file=/vsimem/file.bin', 'wb')
    if fp is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Sector size (16) should be at least twice larger than the block size (16) in CBC_CTS
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PROD,sector_size=16,mode=CBC_CTS,file=/vsimem/file.bin', 'wb')
    if fp is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    gdal.Unlink('/vsimem/file.bin')

    return 'success'

###############################################################################
# Test various options

def vsicrypt_3():

    if not gdaltest.has_vsicrypt:
        return 'skip'

    for options in ['sector_size=16', 'alg=AES', 'alg=DES_EDE2', 'alg=DES_EDE3', 'alg=SKIPJACK', 'alg=invalid',
                    'mode=CBC', 'mode=CFB', 'mode=OFB', 'mode=CTR', 'mode=CBC_CTS', 'mode=invalid',
                    'freetext=my_free_text',
                    'add_key_check=yes' ]:

        gdal.Unlink('/vsimem/file.bin')

        if options == 'alg=invalid' or options == 'mode=invalid':
            with gdaltest.error_handler():
                fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PRODDONT_USE_IN_PROD,%s,file=/vsimem/file.bin' % options, 'wb')
        else:
            fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PRODDONT_USE_IN_PROD,%s,file=/vsimem/file.bin' % options, 'wb')
        if fp is None:
            gdaltest.post_reason('fail')
            print(options)
            return 'fail'
        gdal.VSIFWriteL('hello', 1, 5, fp)
        gdal.VSIFCloseL(fp)

        fp = gdal.VSIFOpenL('/vsicrypt/key=DONT_USE_IN_PRODDONT_USE_IN_PROD,file=/vsimem/file.bin', 'r')
        content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
        gdal.VSIFCloseL(fp)

        if content != 'hello':
            gdaltest.post_reason('fail')
            print(options)
            return 'fail'

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

            if content != 'hello':
                gdaltest.post_reason('fail')
                print(options)
                return 'fail'

    # Test key generation

    # Do NOT set VSICRYPT_CRYPTO_RANDOM=NO in production. This is just to speed up tests !
    gdal.SetConfigOption("VSICRYPT_CRYPTO_RANDOM", "NO")
    fp = gdal.VSIFOpenL('/vsicrypt/key=GENERATE_IT,add_key_check=yes,file=/vsimem/file.bin', 'wb')
    gdal.SetConfigOption("VSICRYPT_CRYPTO_RANDOM", None)

    # Get the generated random key
    key_b64 = gdal.GetConfigOption('VSICRYPT_KEY_B64')
    if key_b64 is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)

    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    if content != 'hello':
        gdaltest.post_reason('fail')
        print(options)
        return 'fail'

    gdal.SetConfigOption('VSICRYPT_KEY_B64', None)
    
    fp = gdal.VSIFOpenL('/vsicrypt/key_b64=%s,file=/vsimem/file.bin' % key_b64, 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    if content != 'hello':
        gdaltest.post_reason('fail')
        print(options)
        return 'fail'

    with gdaltest.error_handler():
        statRes = gdal.VSIStatL('/vsicrypt//vsimem/file.bin')
    if statRes is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = gdal.Rename('/vsicrypt//vsimem/file.bin' , '/vsicrypt//vsimem/subdir_crypt/file.bin')
    if ret != 0:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    ret = gdal.Rename('/vsicrypt//vsimem/subdir_crypt/file.bin' , '/vsimem/subdir_crypt/file2.bin')
    if ret != 0:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    dir_content = gdal.ReadDir('/vsicrypt//vsimem/subdir_crypt')
    if dir_content != ['file2.bin']:
        gdaltest.post_reason('fail')
        print(dir_content)
        return 'fail'

    gdal.Unlink('/vsimem/subdir_crypt/file2.bin')

    return 'success'

###############################################################################
# Test "random" operations against reference filesystem

def vsicrypt_4():

    if not gdaltest.has_vsicrypt:
        return 'skip'

    test_file = '/vsicrypt/key=DONT_USE_IN_PROD,sector_size=32,file=/vsimem/file_enc.bin'
    ref_file = '/vsimem/file.bin'
    
    for seed in range(1000):

        gdal.Unlink(test_file)
        gdal.Unlink(ref_file)

        test_f = gdal.VSIFOpenL(test_file, 'wb+')
        ref_f = gdal.VSIFOpenL(ref_file, 'wb+')

        import random
        random.seed(seed)

        for i in range(20):
            random_offset = random.randint(0,1000)
            gdal.VSIFSeekL(test_f, random_offset, 0)
            gdal.VSIFSeekL(ref_f, random_offset, 0)
            
            random_size = random.randint(1,80)
            random_content = ''.join([ chr(40 + int(10 * random.random()) ) for i in range(random_size) ])
            gdal.VSIFWriteL(random_content, 1, random_size, test_f)
            gdal.VSIFWriteL(random_content, 1, random_size, ref_f)

            if random.randint(0,1) == 0:
                random_offset = random.randint(0,1500)
                gdal.VSIFSeekL(test_f, random_offset, 0)
                gdal.VSIFSeekL(ref_f, random_offset, 0)

                random_size = random.randint(1,80)
                test_content = gdal.VSIFReadL(1, random_size, test_f)
                ref_content = gdal.VSIFReadL(1, random_size, ref_f)
                if test_content != ref_content:
                    print(seed)
                    print('Test content (%d):' % len(test_content))
                    print(test_content)
                    print('')
                    print('Ref content (%d):' % len(ref_content))
                    print(ref_content)
                    return 'fail'

        gdal.VSIFSeekL(test_f, 0, 0)
        gdal.VSIFSeekL(ref_f, 0, 0)
        test_content = gdal.VSIFReadL(1, 100000, test_f)
        ref_content = gdal.VSIFReadL(1, 100000, ref_f)

        if test_content != ref_content:
            print(seed)
            print('Test content (%d):' % len(test_content))
            print(test_content)
            print('')
            print('Ref content (%d):' % len(ref_content))
            print(ref_content)
            return 'fail'

    gdal.Unlink(test_file)
    gdal.Unlink(ref_file)

    return 'success'

###############################################################################
# Test random filling of last sector

def vsicrypt_5():

    if not gdaltest.has_vsicrypt:
        return 'skip'

    test_file = '/vsicrypt/key=DONT_USE_IN_PROD,file=/vsimem/file_enc.bin'
    
    f = gdal.VSIFOpenL(test_file, 'wb+')
    gdal.VSIFWriteL('ab', 1, 2, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL(test_file, 'rb+')
    gdal.VSIFSeekL(f, 3,0)
    gdal.VSIFWriteL('d', 1, 1, f)
    gdal.VSIFCloseL(f)
    
    f = gdal.VSIFOpenL(test_file, 'rb')
    content = gdal.VSIFReadL(1, 4, f)
    content = struct.unpack('B' * len(content), content)
    gdal.VSIFCloseL(f)
    if content != (97, 98, 0, 100):
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    f = gdal.VSIFOpenL(test_file, 'rb+')
    gdal.VSIFReadL(1,1,f)
    gdal.VSIFSeekL(f, 5,0)
    gdal.VSIFWriteL('f', 1, 1, f)
    gdal.VSIFCloseL(f)
    
    f = gdal.VSIFOpenL(test_file, 'rb')
    content = gdal.VSIFReadL(1, 6, f)
    content = struct.unpack('B' * len(content), content)
    gdal.VSIFCloseL(f)
    if content != (97, 98, 0, 100, 0, 102):
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    f = gdal.VSIFOpenL(test_file, 'rb+')
    gdal.VSIFReadL(1,1,f)
    gdal.VSIFSeekL(f, 512,0)
    gdal.VSIFWriteL('Z', 1, 1, f)
    gdal.VSIFSeekL(f, 7,0)
    gdal.VSIFWriteL('h', 1, 1, f)
    gdal.VSIFCloseL(f)
        
    f = gdal.VSIFOpenL(test_file, 'rb')
    content = gdal.VSIFReadL(1, 8, f)
    content = struct.unpack('B' * len(content), content)
    gdal.VSIFCloseL(f)
    if content != (97, 98, 0, 100, 0, 102, 0, 104):
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'
        
    gdal.Unlink(test_file)
    
    return 'success'

###############################################################################
# Test VSISetCryptKey

def vsicrypt_6():

    try:
        import ctypes
    except:
        return 'skip'
    import testnonboundtoswig

    testnonboundtoswig.testnonboundtoswig_init()

    if testnonboundtoswig.gdal_handle is None:
        return 'skip'

    testnonboundtoswig.gdal_handle.VSISetCryptKey.argtypes = [ ctypes.c_char_p, ctypes.c_int]
    testnonboundtoswig.gdal_handle.VSISetCryptKey.restype = None

    # Set a valid key
    testnonboundtoswig.gdal_handle.VSISetCryptKey('DONT_USE_IN_PROD'.encode('ASCII'), 16)
    
    if not gdaltest.has_vsicrypt:
        return 'skip'
        
    fp = gdal.VSIFOpenL('/vsicrypt/add_key_check=yes,file=/vsimem/file.bin', 'wb+')
    if fp is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)
        
    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    if content != 'hello':
        gdaltest.post_reason('fail')
        return 'fail'

    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    if fp is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFWriteL('hello', 1, 5, fp)
    gdal.VSIFCloseL(fp)
    
    fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    content = gdal.VSIFReadL(1, 5, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    if content != 'hello':
        gdaltest.post_reason('fail')
        return 'fail'

    # Set a too short key
    testnonboundtoswig.gdal_handle.VSISetCryptKey('bbc'.encode('ASCII'), 3)
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'rb')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'
        
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Erase key
    testnonboundtoswig.gdal_handle.VSISetCryptKey(None, 0)
    with gdaltest.error_handler():
        fp = gdal.VSIFOpenL('/vsicrypt//vsimem/file.bin', 'wb+')
    if fp is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    
    gdal.Unlink('/vsimem/file.bin')

    return 'success'

gdaltest_list = [ vsicrypt_1,
                  vsicrypt_2,
                  vsicrypt_3,
                  vsicrypt_4,
                  vsicrypt_5,
                  vsicrypt_6 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsicrypt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
