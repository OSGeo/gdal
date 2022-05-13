#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Build a JPEG2000 file from the XML structure dumped by dump_jp2.py
#            Mostly useful to build non-conformant files
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# ******************************************************************************
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
# ******************************************************************************

import os
import struct
import sys

from osgeo import gdal

XML_TYPE_IDX = 0
XML_VALUE_IDX = 1
XML_FIRST_CHILD_IDX = 2


def Usage():
    print('Usage: build_jp2_from_xml in.xml out.jp2')
    return 1


def find_xml_node(ar, element_name, immediate_child=False, only_attributes=False):
    # type = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if not immediate_child and value == element_name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        if only_attributes and child[XML_TYPE_IDX] != gdal.CXT_Attribute:
            continue
        if immediate_child:
            if child[XML_VALUE_IDX] == element_name:
                return child
        else:
            found = find_xml_node(child, element_name)
            if found is not None:
                return found
    return None


def get_attribute_val(ar, attr_name):
    node = find_xml_node(ar, attr_name, True)
    if node is None or node[XML_TYPE_IDX] != gdal.CXT_Attribute:
        return None
    if len(ar) > XML_FIRST_CHILD_IDX and \
            node[XML_FIRST_CHILD_IDX][XML_TYPE_IDX] == gdal.CXT_Text:
        return node[XML_FIRST_CHILD_IDX][XML_VALUE_IDX]
    return None


def get_node_content(node):
    if node is None:
        return None
    for child_idx in range(XML_FIRST_CHILD_IDX, len(node)):
        child = node[child_idx]
        if child[XML_TYPE_IDX] == gdal.CXT_Text:
            return child[XML_VALUE_IDX]
    return None


def hex_letter_to_number(ch):
    val = 0
    if ch >= '0' and ch <= '9':
        val = (ord(ch) - ord('0'))
    elif ch >= 'a' and ch <= 'f':
        val = (ord(ch) - ord('a')) + 10
    elif ch >= 'A' and ch <= 'F':
        val = (ord(ch) - ord('A')) + 10
    return val


def write_hexstring_as_binary(hex_binary_content, out_f):
    for i in range(int(len(hex_binary_content) / 2)):
        val = hex_letter_to_number(hex_binary_content[2 * i]) * 16 + \
            hex_letter_to_number(hex_binary_content[2 * i + 1])
        out_f.write(chr(val).encode('latin1'))


def parse_field(xml_tree, out_f, src_jp2file):
    # pylint: disable=unused-argument
    if not(xml_tree[XML_TYPE_IDX] == gdal.CXT_Element and xml_tree[XML_VALUE_IDX] == 'Field'):
        print('Not a Field element')
        return False
    field_name = get_attribute_val(xml_tree, 'name')
    if field_name is None:
        print('Cannot find Field.name attribute')
        # return False
    field_type = get_attribute_val(xml_tree, 'type')
    if field_type is None:
        print('Cannot find Field.type attribute')
        return False
    val = get_node_content(xml_tree)
    if val is None:
        print('Cannot find Field content')
        return False
    if field_type == 'uint8':
        out_f.write(struct.pack('>B' * 1, int(val)))
    elif field_type == 'uint16':
        out_f.write(struct.pack('>H' * 1, int(val)))
    elif field_type == 'uint32':
        out_f.write(struct.pack('>I' * 1, int(val)))
    elif field_type == 'string':
        field_size = get_attribute_val(xml_tree, 'size')
        if field_size is not None:
            assert len(val) == int(field_size)
        out_f.write(val.encode('latin1'))
    elif field_type == 'hexint':
        field_size = get_attribute_val(xml_tree, 'size')
        if field_size is not None:
            assert len(val) == 2 + 2 * int(field_size)
        write_hexstring_as_binary(val[2:], out_f)
    else:
        print('Unhandled type %s' % field_type)
        return False
    return True


marker_map = {
    "SOC": 0xFF4F,
    "EOC": 0xFFD9,
    "SOT": 0xFF90,
    "SIZ": 0xFF51,
    "COD": 0xFF52,
    "COC": 0xFF53,
    "TLM": 0xFF55,
    "PLM": 0xFF57,
    "PLT": 0xFF58,
    "QCD": 0xFF5C,
    "QCC": 0xFF5D,
    "RGN": 0xFF5E,
    "POC": 0xFF5F,
    "PPM": 0xFF60,
    "PPT": 0xFF61,
    "CRG": 0xFF63,
    "COM": 0xFF64,
}


def parse_jpc_marker(xml_tree, out_f, src_jp2file):
    if not(xml_tree[XML_TYPE_IDX] == gdal.CXT_Element and xml_tree[XML_VALUE_IDX] == 'Marker'):
        print('Not a Marker element')
        return False
    marker_name = get_attribute_val(xml_tree, 'name')
    if marker_name is None:
        print('Cannot find Marker.name attribute')
        return False
    if find_xml_node(xml_tree, 'Field', immediate_child=True):
        if marker_name not in marker_map:
            print('Cannot find marker signature for %s' % marker_name)
            return False
        marker_signature = marker_map[marker_name]
        out_f.write(struct.pack('>H' * 1, marker_signature))
        pos = out_f.tell()
        out_f.write(struct.pack('>H' * 1, 0))

        for child_idx in range(XML_FIRST_CHILD_IDX, len(xml_tree)):
            child = xml_tree[child_idx]
            if child[XML_TYPE_IDX] != gdal.CXT_Element:
                continue
            if not parse_field(child, out_f, src_jp2file):
                return False

        new_pos = out_f.tell()
        out_f.seek(pos, 0)
        out_f.write(struct.pack('>H' * 1, new_pos - pos))
        out_f.seek(new_pos, 0)
    else:
        offset = get_attribute_val(xml_tree, 'offset')
        if offset is None:
            if marker_name == 'SOC' or marker_name == 'EOC':
                marker_signature = marker_map[marker_name]
                out_f.write(struct.pack('>H' * 1, marker_signature))
                return True

            print('Cannot find Marker.offset attribute')
            return False
        offset = int(offset)

        length = get_attribute_val(xml_tree, 'length')
        if length is None:
            print('Cannot find Marker.length attribute')
            return False
        length = int(length)

        src_jp2file.seek(offset, 0)
        data = src_jp2file.read(length)

        out_f.write(data)

    return True


def parse_jp2codestream(inpath, xml_tree, out_f, src_jp2file=None):

    if src_jp2file is None:
        src_jp2filename = get_attribute_val(xml_tree, 'filename')
        if src_jp2filename is None:
            print('Cannot find JP2KCodeStream.filename attribute')
            return False
        if os.path.exists(src_jp2filename):
            src_jp2file = open(src_jp2filename, 'rb')
        else:
            src_jp2file = open(os.path.join(inpath, src_jp2filename), 'rb')

    for child_idx in range(XML_FIRST_CHILD_IDX, len(xml_tree)):
        child = xml_tree[child_idx]
        if child[XML_TYPE_IDX] != gdal.CXT_Element:
            continue
        if not parse_jpc_marker(child, out_f, src_jp2file):
            return False
    return True


def parse_jp2_box(xml_tree, out_f, src_jp2file):
    if not(xml_tree[XML_TYPE_IDX] == gdal.CXT_Element and xml_tree[XML_VALUE_IDX] == 'JP2Box'):
        print('Not a JP2Box element')
        return False
    jp2box_name = get_attribute_val(xml_tree, 'name')
    if jp2box_name is None:
        print('Cannot find JP2Box.name attribute')
        return False
    if len(jp2box_name) != 4:
        print('Invalid JP2Box.name : %s' % jp2box_name)
        return False
    hex_binary_content = get_node_content(find_xml_node(xml_tree, 'BinaryContent', immediate_child=True))
    decoded_content = find_xml_node(xml_tree, 'DecodedContent', immediate_child=True)
    decoded_geotiff = find_xml_node(xml_tree, 'DecodedGeoTIFF', immediate_child=True)
    text_content = get_node_content(find_xml_node(xml_tree, 'TextContent', immediate_child=True))
    xml_content = find_xml_node(xml_tree, 'XMLContent', immediate_child=True)
    jp2box = find_xml_node(xml_tree, 'JP2Box', immediate_child=True)
    jp2codestream = find_xml_node(xml_tree, 'JP2KCodeStream', immediate_child=True)

    if hex_binary_content:
        if decoded_content or decoded_geotiff or text_content or xml_content or jp2box:
            print('BinaryContent found, and one of DecodedContent/DecodedGeoTIFF/TextContent/XMLContent/JP2Box. The latter will be ignored')
        if jp2box_name == 'uuid':
            uuid = get_node_content(find_xml_node(xml_tree, 'UUID', immediate_child=True))
            if uuid is None:
                print('Cannot find JP2Box.UUID element')
                return False
        else:
            uuid = ''
        out_f.write(struct.pack('>I' * 1, 8 + int(len(hex_binary_content) / 2) + int(len(uuid) / 2)))
        out_f.write(jp2box_name.encode('ascii'))
        write_hexstring_as_binary(uuid, out_f)
        write_hexstring_as_binary(hex_binary_content, out_f)

    elif decoded_content:
        if decoded_geotiff or text_content or xml_content or jp2box:
            print('DecodedContent found, and one of DecodedGeoTIFF/TextContent/XMLContent/JP2Box. The latter will be ignored')
        pos = out_f.tell()
        out_f.write(struct.pack('>I' * 1, 0))
        out_f.write(jp2box_name.encode('ascii'))
        for child_idx in range(XML_FIRST_CHILD_IDX, len(decoded_content)):
            child = decoded_content[child_idx]
            if child[XML_TYPE_IDX] == gdal.CXT_Element and child[XML_VALUE_IDX] == 'Field':
                if not parse_field(child, out_f, src_jp2file):
                    return False
        new_pos = out_f.tell()
        out_f.seek(pos, 0)
        out_f.write(struct.pack('>I' * 1, new_pos - pos))
        out_f.seek(new_pos, 0)

    elif text_content:
        if decoded_geotiff or xml_content or jp2box:
            print('TextContent found, and one of DecodedGeoTIFF/XMLContent/JP2Box. The latter will be ignored')
        out_f.write(struct.pack('>I' * 1, 8 + len(text_content)))
        out_f.write(jp2box_name.encode('ascii'))
        out_f.write(text_content.encode('latin1'))

    elif xml_content:
        if decoded_geotiff or jp2box:
            print('XMLContent found, and one of DecodedGeoTIFF/JP2Box. The latter will be ignored')
        serialized_xml_content = gdal.SerializeXMLTree(xml_content[XML_FIRST_CHILD_IDX])
        out_f.write(struct.pack('>I' * 1, 8 + len(serialized_xml_content)))
        out_f.write(jp2box_name.encode('ascii'))
        out_f.write(serialized_xml_content.encode('latin1'))

    elif jp2box:
        if decoded_geotiff:
            print('JP2Box found, and one of DecodedGeoTIFF. The latter will be ignored')
        pos = out_f.tell()
        out_f.write(struct.pack('>I' * 1, 0))
        out_f.write(jp2box_name.encode('ascii'))
        for child_idx in range(XML_FIRST_CHILD_IDX, len(xml_tree)):
            child = xml_tree[child_idx]
            if child[XML_TYPE_IDX] == gdal.CXT_Element and child[XML_VALUE_IDX] == 'JP2Box':
                if not parse_jp2_box(child, out_f, src_jp2file):
                    return False
        new_pos = out_f.tell()
        out_f.seek(pos, 0)
        out_f.write(struct.pack('>I' * 1, new_pos - pos))
        out_f.seek(new_pos, 0)

    elif decoded_geotiff:
        serialized_xml_content = gdal.SerializeXMLTree(decoded_geotiff[XML_FIRST_CHILD_IDX])

        vrt_ds = gdal.Open(serialized_xml_content)
        if vrt_ds is None:
            print('Cannot decode VRTDataset. Outputting empty content')
            binary_content = ''
        else:
            tmpfilename = '/vsimem/build_jp2_from_xml_tmp.tif'
            gdal.GetDriverByName('GTiff').CreateCopy(tmpfilename, vrt_ds)
            tif_f = gdal.VSIFOpenL(tmpfilename, 'rb')
            binary_content = gdal.VSIFReadL(1, 10000, tif_f)
            gdal.VSIFCloseL(tif_f)
            gdal.Unlink(tmpfilename)

        uuid = get_node_content(find_xml_node(xml_tree, 'UUID', immediate_child=True))
        if uuid is None:
            uuid = 'B14BF8BD083D4B43A5AE8CD7D5A6CE03'

        out_f.write(struct.pack('>I' * 1, 8 + len(binary_content) + int(len(uuid) / 2)))
        out_f.write(jp2box_name.encode('ascii'))
        write_hexstring_as_binary(uuid, out_f)
        out_f.write(binary_content)

    elif jp2codestream:
        pos = out_f.tell()
        out_f.write(struct.pack('>I' * 1, 0))
        out_f.write(jp2box_name.encode('ascii'))
        if not parse_jp2codestream(None, jp2codestream, out_f, src_jp2file):
            return False
        new_pos = out_f.tell()
        out_f.seek(pos, 0)
        out_f.write(struct.pack('>I' * 1, new_pos - pos))
        out_f.seek(new_pos, 0)

    else:
        data_offset = get_attribute_val(xml_tree, 'data_offset')
        if data_offset is None:
            print('Cannot find JP2Box.data_offset attribute')
            return False
        data_offset = int(data_offset)

        data_length = get_attribute_val(xml_tree, 'data_length')
        if data_length is None:
            print('Cannot find JP2Box.data_length attribute')
            return False
        data_length = int(data_length)

        src_jp2file.seek(data_offset, 0)
        data = src_jp2file.read(data_length)

        out_f.write(struct.pack('>I' * 1, 8 + data_length))
        out_f.write(jp2box_name.encode('ascii'))
        out_f.write(data)

    return True


def parse_jp2file(inpath, xml_tree, out_f):
    src_jp2filename = get_attribute_val(xml_tree, 'filename')
    if src_jp2filename is None:
        print('Cannot find JP2File.filename attribute')
        return False
    if os.path.exists(src_jp2filename):
        src_jp2file = open(src_jp2filename, 'rb')
    else:
        src_jp2file = open(os.path.join(inpath, src_jp2filename), 'rb')
    for child_idx in range(XML_FIRST_CHILD_IDX, len(xml_tree)):
        child = xml_tree[child_idx]
        if child[XML_TYPE_IDX] != gdal.CXT_Element:
            continue
        if not parse_jp2_box(child, out_f, src_jp2file):
            return False
    return True

# Wrapper class for GDAL VSI*L API with class Python file interface


class VSILFile(object):
    def __init__(self, filename, access):
        self.f = gdal.VSIFOpenL(filename, access)

    def write(self, data):
        gdal.VSIFWriteL(data, 1, len(data), self.f)

    def tell(self):
        return gdal.VSIFTellL(self.f)

    def seek(self, pos, ref):
        gdal.VSIFSeekL(self.f, pos, ref)

    def close(self):
        gdal.VSIFCloseL(self.f)
        self.f = None


def build_file(inname, outname):

    inpath = os.path.dirname(inname)
    xml_tree = gdal.ParseXMLString(open(inname).read())
    if xml_tree is None:
        print('Cannot parse %s' % inname)
        return False

    # out_f = open(outname, 'wb+')
    out_f = VSILFile(outname, 'wb+')
    if xml_tree[XML_TYPE_IDX] == gdal.CXT_Element and xml_tree[XML_VALUE_IDX] == 'JP2File':
        ret = parse_jp2file(inpath, xml_tree, out_f)
    elif xml_tree[XML_TYPE_IDX] == gdal.CXT_Element and xml_tree[XML_VALUE_IDX] == 'JP2KCodeStream':
        ret = parse_jp2codestream(inpath, xml_tree, out_f)
    else:
        print('Unexpected node: %s' % xml_tree[XML_VALUE_IDX])
        ret = False
    out_f.close()

    return ret


def main(argv=sys.argv):
    i = 1
    inname = None
    outname = None
    while i < len(argv):
        if sys.argv[i][0] == '-':
            return Usage()
        elif inname is None:
            inname = sys.argv[i]
        elif outname is None:
            outname = sys.argv[i]
        else:
            return Usage()

        i = i + 1

    if inname is None or outname is None:
        return Usage()

    if build_file(inname, outname):
        return 0
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
