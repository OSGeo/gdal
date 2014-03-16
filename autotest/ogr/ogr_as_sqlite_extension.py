#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDAL as a SQLite3 dynamically loaded extension
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

# This file is meant at being run by ogr_virtualogr_3()
# This is a bit messy with heavy use of ctypes. The sqlite3 python module
# is rarely compiled with support of extension loading, so we just simulate
# what a tiny C program would do

import sys

def do(sqlite3name, gdalname):
    try:
        import ctypes
    except:
        print('skip')
        sys.exit(0)

    sqlite_handle = ctypes.cdll.LoadLibrary(sqlite3name)
    if sqlite_handle is None:
        print('skip')
        sys.exit(0)

    db = ctypes.c_void_p(0)
    pdb = ctypes.pointer(db)
    if hasattr(sqlite_handle, 'sqlite3_open'):
        ret = sqlite_handle.sqlite3_open(':memory:', pdb)
    elif hasattr(sqlite_handle, 'SPLite3_open'):
        ret = sqlite_handle.SPLite3_open(':memory:', pdb)
    else:
        print('skip')
        sys.exit(0)
    if ret != 0:
        print('Error sqlite3_open ret = %d' % ret)
        sys.exit(1)

    if hasattr(sqlite_handle, 'sqlite3_enable_load_extension'):
        ret = sqlite_handle.sqlite3_enable_load_extension(db, 1)
    elif hasattr(sqlite_handle, 'SPLite3_enable_load_extension'):
        ret = sqlite_handle.SPLite3_enable_load_extension(db, 1)
    else:
        print('skip')
        sys.exit(0)
    if ret != 0:
        print('skip')
        sys.exit(0)

    gdalname = gdalname.encode('ascii')

    if hasattr(sqlite_handle, 'sqlite3_load_extension'):
        ret = sqlite_handle.sqlite3_load_extension(db, gdalname, None, None)
    else:
        ret = sqlite_handle.SPLite3_load_extension(db, gdalname, None, None)
    if ret != 0:
        print('Error sqlite3_load_extension ret = %d' % ret)
        sys.exit(1)

    tab = ctypes.c_void_p()
    ptab = ctypes.pointer(tab)
    nrow = ctypes.c_int(0)
    pnrow = ctypes.pointer(nrow)
    ncol = ctypes.c_int(0)
    pncol = ctypes.pointer(ncol)

    if hasattr(sqlite_handle, 'sqlite3_get_table'):
        ret = sqlite_handle.sqlite3_get_table(db, 'SELECT ogr_version()'.encode('ascii'), ptab, pnrow, pncol, None)
    else:
        ret = sqlite_handle.SPLite3_get_table(db, 'SELECT ogr_version()'.encode('ascii'), ptab, pnrow, pncol, None)
    if ret != 0:
        print('Error sqlite3_get_table ret = %d' % ret)
        sys.exit(1)

    cast_tab = ctypes.cast(tab, ctypes.POINTER(ctypes.c_char_p))
    sys.stdout.write(cast_tab[1].decode('ascii'))
    sys.stdout.flush()

    if hasattr(sqlite_handle, 'sqlite3_close'):
        ret = sqlite_handle.sqlite3_close(db)
    else:
        ret = sqlite_handle.SPLite3_close(db)
    if ret != 0:
        sys.exit(1)

gdaltest_list = []

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('python ogr_as_sqlite_extension name_of_libsqlite3 name_of_libgdal')
        sys.exit(1)

    do(sys.argv[1], sys.argv[2])
