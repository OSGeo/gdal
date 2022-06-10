#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Dump JPEG2000 file structure
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# ******************************************************************************
#  Copyright (c) 2015, European Union (European Environment Agency)
#  Copyright (c) 2015, European Union Satellite Centre
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
# ******************************************************************************

import sys
from osgeo import gdal


def Usage():
    print('Usage:  dump_jp2 [-dump_gmljp2 out.xml|-] [-dump_crsdictionary out.xml|-]')
    print('                 [-extract_all_xml_boxes filename_prefix]')
    print('                 test.jp2')
    print('')
    print('Options (all are exclusive of the regular dump):')
    print('')
    print('-dump_gmljp2: Writes the content of the GMLJP2 box in the specified')
    print('              file, or on the console if "-" syntax is used.')
    print('-dump_crsdictionary: Writes the content of the GML CRS dictionary box in the specified')
    print('                     file, or on the console if "-" syntax is used.')
    print('-extract_all_xml_boxes: Extract all XML boxes in separate files, and prefix each filename')
    print('                        with the supplied prefix. gmljp2://xml/ link will be replaced by')
    print('                        links to on-disk files.')

    return 2


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
    if mdd_list is None:
        mdd_list = []
    for domain in mdd_list:
        if domain.startswith('xml:'):
            mdd_item = ds.GetMetadata(domain)[0]
            if '<Dictionary' in mdd_item or '<gml:Dictionary' in mdd_item:
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


def extract_all_xml_boxes(filename, prefix):
    ds = gdal.Open(filename)
    if ds is None:
        print('Cannot open %s' % filename)
        return 1
    mdd_list = ds.GetMetadataDomainList()
    if mdd_list is None:
        mdd_list = []
    for domain in mdd_list:
        if domain.startswith('xml:'):
            mdd_item = ds.GetMetadata(domain)[0]
            boxname = domain[4:]
            if boxname == 'gml.root-instance':
                boxname = 'gml_root_instance.gml'
            out_filename = prefix + boxname

            # Correct references to gmljp2://xml/foo to prefix_foo
            out_content = ''
            pos = 0
            while True:
                new_pos = mdd_item.find('gmljp2://xml/', pos)
                if new_pos < 0:
                    out_content += mdd_item[pos:]
                    break

                # Check that the referenced box really exists
                end_gmljp2_link_space = mdd_item.find(' ', new_pos)
                end_gmljp2_link_double_quote = mdd_item.find('"', new_pos)
                end_gmljp2_link = -1
                if end_gmljp2_link_space >= 0 and end_gmljp2_link_double_quote >= 0:
                    if end_gmljp2_link_space < end_gmljp2_link_double_quote:
                        end_gmljp2_link = end_gmljp2_link_space
                    else:
                        end_gmljp2_link = end_gmljp2_link_double_quote
                elif end_gmljp2_link_space >= 0:
                    end_gmljp2_link = end_gmljp2_link_space
                elif end_gmljp2_link_double_quote >= 0:
                    end_gmljp2_link = end_gmljp2_link_double_quote
                if end_gmljp2_link >= 0:
                    referenced_box = mdd_item[new_pos + len('gmljp2://xml/'):end_gmljp2_link]
                    if ('xml:' + referenced_box) not in mdd_list:
                        print('Warning: box %s reference box %s, but the latter is not found' % (boxname, referenced_box))

                out_content += mdd_item[pos:new_pos]
                out_content += prefix
                pos = new_pos + len('gmljp2://xml/')

            f = open(out_filename, 'wt')
            f.write(out_content)
            f.close()
            print('INFO: %s written' % out_filename)

    if not mdd_list:
        print('No XML box found')
        return 1
    return 0


def main(argv=sys.argv):
    i = 1
    out_gmljp2 = None
    out_crsdictionary = None
    extract_all_xml_boxes_prefix = None
    filename = None
    while i < len(argv):
        if argv[i] == "-dump_gmljp2":
            if i >= len(argv) - 1:
                return Usage()
            out_gmljp2 = argv[i + 1]
            i = i + 1
        elif argv[i] == "-dump_crsdictionary":
            if i >= len(argv) - 1:
                return Usage()
            out_crsdictionary = argv[i + 1]
            i = i + 1
        elif argv[i] == "-extract_all_xml_boxes":
            if i >= len(argv) - 1:
                return Usage()
            extract_all_xml_boxes_prefix = argv[i + 1]
            i = i + 1
        elif argv[i][0] == '-':
            return Usage()
        elif filename is None:
            filename = argv[i]
        else:
            return Usage()

        i = i + 1

    if filename is None:
        return Usage()

    if out_gmljp2 or out_crsdictionary or extract_all_xml_boxes_prefix:
        if out_gmljp2:
            if dump_gmljp2(filename, out_gmljp2) != 0:
                return 1
        if out_crsdictionary:
            if dump_crsdictionary(filename, out_crsdictionary) != 0:
                return 1
        if extract_all_xml_boxes_prefix:
            if extract_all_xml_boxes(filename, extract_all_xml_boxes_prefix) != 0:
                return 1
    else:
        s = gdal.GetJPEG2000StructureAsString(filename, ['ALL=YES'])
        print(s)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
