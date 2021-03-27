#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  module for handling color palettes
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
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
import re
from xml.dom import minidom
from collections import OrderedDict
import tempfile
from typing import Sequence, Union

from osgeo_utils.auxiliary import base

PathOrStrings = Union[base.PathLike, Sequence[str]]
ColorPaletteOrPathOrStrings = Union['ColorPalette', PathOrStrings]


class ColorPalette:
    __slots__ = ['pal', '_all_numeric']

    def __init__(self):
        self.pal = OrderedDict()
        self._all_numeric = True

    def __repr__(self):
        return str(self.pal)

    def replace_absolute_values_with_percent(self, ndv=True):
        new_pal = ColorPalette()
        for num, val in self.pal.items():
            if not isinstance(num, str):
                if num < 0:
                    num = 0
                elif num > 100:
                    num = 100
                num = str(num)+'%'
            new_pal.pal[num] = val
        new_pal._all_numeric = False
        if ndv:
            new_pal.pal['nv'] = 0
        return new_pal

    def to_serial_values(self, first=0):
        keys = list(self.pal.keys())
        i = first
        for key in keys:
            if not isinstance(key, str):
                self.pal[i] = self.pal.pop(key)
                i += 1

    def has_percents(self):
        if self._all_numeric:
            return False
        for num in self.pal.keys():
            if not isinstance(num, str):
                continue
            is_percent = num.endswith('%')
            if is_percent:
                return True
        return False

    def apply_percent(self, min_val, max_val):
        if self._all_numeric:
            # nothing to do
            return
        all_numeric = True
        new_pal = self.pal.copy()
        for num in self.pal.keys():
            if not isinstance(num, str):
                continue
            is_percent = num.endswith('%')
            if is_percent:
                new_num = num.rstrip('%')
                try:
                    new_num = base.num(new_num)
                    if is_percent:
                        new_num = (max_val - min_val) * new_num * 0.01 + min_val
                    new_pal[new_num] = new_pal.pop(num)
                except ValueError:
                    all_numeric = False
            else:
                all_numeric = False
                continue
        if all_numeric:
            self._all_numeric = True
        self.pal = new_pal

    def assign(self, other: 'ColorPalette'):
        self.pal = other.pal.copy()
        self._all_numeric = other._all_numeric

    @staticmethod
    def get_supported_extenstions():
        return ['txt', 'qlr']

    def is_supported_format(self, filename):
        if base.is_path_like(filename):
            ext = base.get_extension().lower()
            return ext in self.get_supported_extenstions()
        return False

    def read(self, filename_or_strings: ColorPaletteOrPathOrStrings):
        if isinstance(filename_or_strings, ColorPalette):
            self.assign(filename_or_strings)
        else:
            filename, temp_filename = get_file_from_strings(filename_or_strings)
            ext = base.get_extension(filename).lower()
            if ext == 'qlr':
                self.read_qlr(filename)
            elif ext == 'txt':
                self.read_color_file(filename)
            else:
                return False
            if temp_filename:
                os.remove(temp_filename)
        return True

    def read_color_file(self, color_filename_or_lines):
        if isinstance(color_filename_or_lines, ColorPalette):
            return self
        elif color_filename_or_lines is None:
            self.pal.clear()
            return self
        elif base.is_path_like(color_filename_or_lines):
            color_filename_or_lines = open(str(color_filename_or_lines)).readlines()
        elif not base.is_sequence(color_filename_or_lines):
            raise Exception('unknown input {}'.format(color_filename_or_lines))

        self.pal.clear()
        for line in color_filename_or_lines:
            split_line = line.strip().split(' ', 1)
            if len(split_line) < 2:
                continue
            try:
                color = self.pal_color_to_rgb(split_line[1])
                key = split_line[0].strip()
            except:
                raise Exception('Error reading palette line: {}'.format(line))
            try:
                key = base.num(key)
            except ValueError:
                # should be percent
                self._all_numeric = False
                pass
            self.pal[key] = color

    def write_color_file(self, color_filename=None):
        if color_filename is None:
            color_filename = tempfile.mktemp(suffix='.txt')
        os.makedirs(os.path.dirname(str(color_filename)), exist_ok=True)
        with open(str(color_filename), mode='w') as fp:
            for key, color in self.pal.items():
                color_entry = self.color_to_color_entry(color)
                color_entry = ' '.join(str(c) for c in color_entry)
                fp.write('{} {}\n'.format(key, color_entry))
        return color_filename

    def to_mem_buffer(self):
        s = ''
        for key, color in self.pal.items():
            cc = self.color_to_color_entry(color)
            cc = ' '.join(str(c) for c in cc)
            s = s + '{} {}\n'.format(key, cc)
        return s

    def read_xml(self, xml_filename, type=None, tag_name=None):
        if tag_name is None:
            if type is None:
                type = base.get_suffix(xml_filename)
            type = type.lstrip('.').lower()
            if type == 'qlr':
                #             <paletteEntry color="#ffffff" alpha="0" label="0" value="0"/>
                tag_name = "paletteEntry"
            elif type == 'qml':
                #           <item label="-373" color="#d7191c" alpha="255" value="-373"/>
                tag_name = "item"
            else:
                raise Exception('Unknown file type {}'.format(xml_filename))
        self.pal.clear()
        qlr = minidom.parse(str(xml_filename))
        #             <paletteEntry color="#ffffff" alpha="0" label="0" value="0"/>
        color_palette = qlr.getElementsByTagName(tag_name)
        for palette_entry in color_palette:
            color = palette_entry.getAttribute("color")
            if str(color).startswith('#'):
                color = int(color[1:], 16)
            alpha = palette_entry.getAttribute("alpha")
            alpha = int(alpha)
            color = color + (alpha << 8*3)  # * 256**3
            key = palette_entry.getAttribute("value")
            key = base.num(key)
            self.pal[key] = color

    def read_qlr(self, qlr_filename):
        return self.read_xml(qlr_filename, type='qlr')

    def read_qml(self, qml_filename):
        return self.read_xml(qml_filename, type='qml')

    @staticmethod
    def from_string_list(color_palette_strings):
        res = ColorPalette()
        res.read_color_file(color_palette_strings)
        return res

    @staticmethod
    def format_number(num):
        return num if isinstance(num, str) else '{:.2f}'.format(num)

    @staticmethod
    def format_color(col):
        return col if isinstance(col, str) else '#{:06X}'.format(col)

    @staticmethod
    def color_to_color_entry(color):
        # if color < 256:
        #     return color
        # else:
            b = base.get_byte(color, 0)
            g = base.get_byte(color, 1)
            r = base.get_byte(color, 2)
            a = base.get_byte(color, 3)

            if a < 255:
                return r, g, b, a
            else:
                return r, g, b

    @staticmethod
    def pal_color_to_rgb(cc):
        # r g b a -> argb
        # todo: support color names as implemented in the cpp version of this function...
        # cc = color components
        cc = re.findall(r'\d+', cc)
        try:
            # if not rgb_colors:
            #     return (*(int(c) for c in cc),)
            if len(cc) == 1:
                return int(cc[0])
            elif len(cc) == 3:
                return (((((255 << 8) + int(cc[0])) << 8) + int(cc[1])) << 8) + int(cc[2])
            elif len(cc) == 4:
                return (((((int(cc[3]) << 8) + int(cc[0])) << 8) + int(cc[1])) << 8) + int(cc[2])
            else:
                return 0
        except:
            return 0

    @staticmethod
    def pas_color_to_rgb(col):
        # $CC00FF80
        # $AARRGGBB
        if isinstance(col, str):
            col = str(col).strip('$')
        return int(col, 16)

    @staticmethod
    def from_color_list(color_list):
        res = ColorPalette()
        res.pal.clear()
        res._all_numeric = True
        for key, color in enumerate(color_list):
            res.pal[key] = color
        return res

    @staticmethod
    def from_mcd(mcd_color_list):
        color_list = [int(color.lstrip('#'), 16) for color in mcd_color_list]
        return ColorPalette.from_color_list(color_list)

    @staticmethod
    def get_css4_palette():
        from matplotlib._color_data import CSS4_COLORS as color_dict
        return ColorPalette.from_mcd(color_dict.values())

    @staticmethod
    def get_tableau_palette():
        from matplotlib._color_data import TABLEAU_COLORS as color_dict
        return ColorPalette.from_mcd(color_dict.values())

    @staticmethod
    def get_xkcd_palette():
        from matplotlib._color_data import XKCD_COLORS as color_dict
        return ColorPalette.from_mcd(color_dict.values())


def xml_to_color_file(xml_filename, **kwargs):
    # def xml_to_color_file(xml_filename: Path, **kwargs) -> ColorPalette, Path:
    xml_filename = xml_filename
    pal = ColorPalette()
    pal.read_xml(xml_filename, **kwargs)
    color_filename = xml_filename.with_suffix('.txt')
    pal.write_color_file(color_filename)
    return pal, color_filename


def get_file_from_strings(color_palette: ColorPaletteOrPathOrStrings):
    temp_color_filename = None
    if isinstance(color_palette, ColorPalette):
        temp_color_filename = tempfile.mktemp(suffix='.txt')
        color_filename = temp_color_filename
        color_palette.write_color_file(temp_color_filename)
    elif base.is_path_like(color_palette):
        color_filename = color_palette
    elif base.is_sequence(color_palette):
        temp_color_filename = tempfile.mktemp(suffix='.txt')
        color_filename = temp_color_filename
        with open(temp_color_filename, 'w') as f:
            for item in color_palette:
                f.write(item+'\n')
    else:
        raise Exception('Unknown color palette type {}'.format(color_palette))
    return color_filename, temp_color_filename


def get_color_palette(color_palette_or_path_or_strings: ColorPaletteOrPathOrStrings):
    if color_palette_or_path_or_strings is None:
        return None
    if isinstance(color_palette_or_path_or_strings, ColorPalette):
        pal = color_palette_or_path_or_strings
    else:
        pal = ColorPalette()
        if not pal.read(color_palette_or_path_or_strings):
            return None
    return pal

