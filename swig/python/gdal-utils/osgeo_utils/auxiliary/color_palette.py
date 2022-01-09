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
from numbers import Real
from pathlib import Path
from xml.dom import minidom
from collections import OrderedDict
import tempfile
from typing import Sequence, Union, Optional, Tuple

from osgeo_utils.auxiliary import base
from osgeo_utils.auxiliary.base import PathLikeOrStr

PathOrStrings = Union[base.PathLikeOrStr, Sequence[str]]
ColorPaletteOrPathOrStrings = Union['ColorPalette', PathOrStrings]


class ColorPalette:
    __slots__ = ['pal', 'ndv', '_all_numeric']
    ndv_keys = [None, 'nv', 'ndv']

    def __init__(self):
        self.pal = OrderedDict()
        self.ndv = None
        self._all_numeric = True

    def __repr__(self):
        return str(self.pal)

    def __eq__(self, other):
        return self.pal == other.pal and self.ndv == other.ndv

    def is_numeric(self):
        return self._all_numeric

    def get_color(self, key):
        return self.ndv if key in self.ndv_keys else self.pal[key]

    def set_color(self, key, color):
        if key in self.ndv_keys:
            self.ndv = color
        else:
            self.pal[key] = color

    def get_txt_key(self, key):
        if key in self.ndv_keys:
            key = 'nv'
        return key

    def get_all_keys(self, with_ndv: bool = True):
        keys = self.pal.keys()
        if with_ndv and self.ndv is not None:
            keys = ['nv', *keys]
        return keys

    def replace_absolute_values_with_percent(self, ndv=True):
        new_pal = ColorPalette()
        for num, val in self.pal.items():
            if not isinstance(num, str):
                if num < 0:
                    num = 0
                elif num > 100:
                    num = 100
                num = str(num) + '%'
            new_pal.pal[num] = val
        new_pal._all_numeric = False
        if ndv:
            new_pal.ndv = 0
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

    def apply_percent(self, min_val: Real, max_val: Real):
        if min_val is None or max_val is None:
            raise Exception('no min or max values to apply')
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
    def get_supported_extenstions() -> Sequence[str]:
        return [
            'txt',  # GDAL Text-based color configuration file
            'qlr',  # QGIS Layer Definition File (qlr)
            'qml',  # QGIS Layer Style File (qml)
        ]

    def is_supported_format(self, filename: PathLikeOrStr):
        if base.is_path_like(filename):
            ext = base.get_extension().lower()
            return ext in self.get_supported_extenstions()
        return False

    def set_ndv(self, ndv: Optional[int], override: bool = True):
        if override or (self.ndv is None):
            self.ndv = ndv

    def read(self, filename_or_strings: Optional[ColorPaletteOrPathOrStrings]):
        if filename_or_strings is None:
            self.pal.clear()
            return
        elif isinstance(filename_or_strings, ColorPalette):
            self.assign(filename_or_strings)
        elif base.is_path_like(filename_or_strings):
            self.read_file(filename_or_strings)
        elif isinstance(filename_or_strings, Sequence):
            self.read_file_txt(lines=filename_or_strings)
        else:
            raise Exception('Unknown input {}'.format(filename_or_strings))

    def read_file(self, filename: PathLikeOrStr):
        ext = base.get_extension(filename).lower()
        if ext in ['qlr', 'qml']:
            self.read_file_qml(filename)
        else:
            self.read_file_txt(filename)

    def read_file_qml(self, qml_filename: PathLikeOrStr, tag_name=None, type=None):
        """ Read QGIS Layer Style File (qml) or QGIS Layer Definition File (qlr) """
        qlr = minidom.parse(str(qml_filename))
        if tag_name is None:
            if type is None:
                renderer = qlr.getElementsByTagName('rasterrenderer')
                if renderer is None:
                    raise Exception(f'Cannot find "rasterrenderer" in {qml_filename}')
                type = renderer[0].getAttribute("type")
                type_to_tag_name = {
                    # <rasterrenderer type="paletted" opacity="1" alphaBand="-1" band="1" nodataColor="">
                    # <paletteEntry color="#ffffff" alpha="0" label="0" value="0"/>
                    "paletted": "paletteEntry",
                    # <rasterrenderer type="singlebandpseudocolor" opacity="1" alphaBand="-1" band="1" classificationMax="100" classificationMin="0" nodataColor="">
                    # <item label="-373" color="#d7191c" alpha="255" value="-373"/>
                    "singlebandpseudocolor": "item",
                }
                if type not in type_to_tag_name:
                    raise Exception(f'Unknown type: {type} in {qml_filename}')
                tag_name = type_to_tag_name[type]

        self.pal.clear()
        color_palette = qlr.getElementsByTagName(tag_name)
        for palette_entry in color_palette:
            color = palette_entry.getAttribute("color")
            if str(color).startswith('#'):
                color = int(color[1:], 16)
            alpha = palette_entry.getAttribute("alpha")
            alpha = int(alpha)
            color = color + (alpha << 24)
            key = palette_entry.getAttribute("value")
            key = base.num(key)
            self.pal[key] = color

    def read_file_txt(self, filename: Optional[PathLikeOrStr] = None, lines: Optional[Sequence[str]] = None):
        """ Read GDAL Text-based color configuration file """
        if filename is not None:
            lines = open(filename).readlines()
        if not isinstance(lines, Sequence):
            raise Exception('unknown input {}'.format(lines))

        self.pal.clear()
        for line in lines:
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
                if key.lower() in self.ndv_keys:
                    self.ndv = color
                    continue
                else:
                    # maybe percent
                    self._all_numeric = False
            self.pal[key] = color

    def get_txt_color_entry(self, key):
        color = self.get_color(key)
        key = self.get_txt_key(key)
        color_entry = self.color_to_color_entry(color)
        color_entry = ' '.join(str(c) for c in color_entry)
        s = '{} {}\n'.format(key, color_entry)
        return s

    def write_file(self, color_filename: Optional[PathLikeOrStr] = None, with_ndv: bool = True):
        tmp_fd = None
        if color_filename is None:
            tmp_fd, color_filename = tempfile.mkstemp(suffix='.txt')
        os.makedirs(os.path.dirname(color_filename), exist_ok=True)
        with open(color_filename, mode='w') as fp:
            for key in self.get_all_keys(with_ndv):
                fp.write(self.get_txt_color_entry(key))
        if tmp_fd:
            os.close(tmp_fd)
        return color_filename

    def to_mem_buffer(self, with_ndv: bool = True) -> str:
        s = ''
        for key in self.get_all_keys(with_ndv):
            line = self.get_txt_color_entry(key)
            s = s + line
        return s

    @staticmethod
    def from_string_list(color_palette_strings: Optional[ColorPaletteOrPathOrStrings]) -> 'ColorPalette':
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
    def color_to_color_entry(color, with_alpha: Optional[bool]=None):
        b = base.get_byte(color, 0)
        g = base.get_byte(color, 1)
        r = base.get_byte(color, 2)
        a = base.get_byte(color, 3)

        if with_alpha or (with_alpha is None and (a < 255)):
            return r, g, b, a
        else:
            return r, g, b

    @staticmethod
    def color_entry_to_color(r, g, b, a):
        return (a << 24) + (r << 16) + (g << 8) + b

    @staticmethod
    def pal_color_to_rgb(cc: str) -> int:
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
    def from_color_list(color_list: Sequence[int]):
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

    # alias names for backwards compatibility
    read_color_file = read
    write_color_file = write_file
    read_file_xml = read_file_qlr = read_file_qml


def xml_to_color_file(xml_filename: Path, **kwargs) -> Tuple[ColorPalette, Path]:
    xml_filename = xml_filename
    pal = ColorPalette()
    pal.read_file_xml(xml_filename, **kwargs)
    color_filename = xml_filename.with_suffix('.txt')
    pal.write_color_file(color_filename)
    return pal, color_filename


def get_file_from_strings(color_palette: ColorPaletteOrPathOrStrings) -> Tuple[str, str]:
    temp_color_filename = None
    tmp_fd = None
    if isinstance(color_palette, ColorPalette):
        tmp_fd, temp_color_filename = tempfile.mkstemp(suffix='.txt')
        color_filename = temp_color_filename
        color_palette.write_color_file(temp_color_filename)
    elif base.is_path_like(color_palette):
        color_filename = color_palette
    elif isinstance(color_palette, Sequence):
        tmp_fd, temp_color_filename = tempfile.mkstemp(suffix='.txt')
        color_filename = temp_color_filename
        with open(temp_color_filename, 'w') as f:
            for item in color_palette:
                f.write(item + '\n')
    else:
        raise Exception('Unknown color palette type {}'.format(color_palette))
    if tmp_fd:
        os.close(tmp_fd)
    return color_filename, temp_color_filename


def get_color_palette(color_palette_or_path_or_strings: ColorPaletteOrPathOrStrings) -> Optional[ColorPalette]:
    if color_palette_or_path_or_strings is None:
        return None
    if isinstance(color_palette_or_path_or_strings, ColorPalette):
        pal = color_palette_or_path_or_strings
    else:
        pal = ColorPalette()
        pal.read(color_palette_or_path_or_strings)
    return pal
