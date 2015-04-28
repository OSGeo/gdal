#!/usr/bin/env python
# -*- coding: utf-8 -*-
#******************************************************************************
#  $Id$
# 
#  Project:  GDAL
#  Purpose:  Dump JPEG2000 file structure
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
# 
#******************************************************************************
#  Copyright (c) 2015, European Union (European Environment Agency)
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

import sys
from osgeo import gdal

def Usage():
    print('Usage:  dump_jp2 [-dump_gmljp2 out.txt|-] [-dump_crsdictionary out.txt|-] test.jp2')
    print('')
    print('Options:')
    print('-dump_gmljp2: Writes the content of the GMLJP2 box in the specified')
    print('              file, or on the console if "-" syntax is used.')
    print('-dump_crsdictionary: Writes the content of the GML CRS dictionary box in the specified')
    print('                     file, or on the console if "-" syntax is used.')
    return 1

def dump_gmljp2(filename, out_gmljp2):
    ds = gdal.Open(filename)
    if ds is None:
        print('Cannot open %s' % filename)
        return 1
    mdd = ds.GetMetadata('xml:gml.root-instance')
    if mdd is None:
        print('No GMLJP2 content found in %s' % filename)
        return 1
    if out_gmljp2 == '-':
        print(mdd[0])
    else:
        f = open(out_gmljp2, 'wt')
        f.write(mdd[0])
        f.close()
        print('INFO: %s written with content of GMLJP2 box' % out_gmljp2)
    return 0

def dump_crsdictionary(filename, out_crsdictionary):
    ds = gdal.Open(filename)
    if ds is None:
        print('Cannot open %s' % filename)
        return 1
    mdd_list = ds.GetMetadataDomainList()
    for domain in mdd_list:
        if domain.startswith('xml:'):
            mdd_item = ds.GetMetadata(domain)[0]
            if mdd_item.find('<Dictionary') >= 0 or mdd_item.find('<gml:Dictionary') >= 0:
                if out_crsdictionary == '-':
                    print(mdd_item)
                else:
                    f = open(out_crsdictionary, 'wt')
                    f.write(mdd_item)
                    f.close()
                    print('INFO: %s written with content of CRS dictionary box (%s)' % (out_crsdictionary, domain[4:]))
                return 0

    print('No CRS dictionary content found in %s' % filename)
    return 1

def main():
    i = 1
    out_gmljp2 = None
    out_crsdictionary = None
    filename = None
    while i < len(sys.argv):
        if sys.argv[i] == "-dump_gmljp2":
            if i >= len(sys.argv) - 1:
                return Usage()
            out_gmljp2 = sys.argv[i+1]
            i = i + 1
        elif sys.argv[i] == "-dump_crsdictionary":
            if i >= len(sys.argv) - 1:
                return Usage()
            out_crsdictionary = sys.argv[i+1]
            i = i + 1
        elif sys.argv[i][0] == '-':
            return Usage()
        elif filename is None:
            filename = sys.argv[i]
        else:
            return Usage()

        i = i + 1

    if filename is None:
        return Usage()

    if out_gmljp2 or out_crsdictionary:
        if out_gmljp2:
            if dump_gmljp2(filename, out_gmljp2) != 0:
                return 1
        if out_crsdictionary:
            if dump_crsdictionary(filename, out_crsdictionary) != 0:
                return 1
    else:
        s = gdal.GetJPEG2000StructureAsString(filename, ['ALL=YES'])
        print(s)

    return 0

if __name__ == '__main__':
    sys.exit(main())
