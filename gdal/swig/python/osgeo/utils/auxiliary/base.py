#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  general use utility functions
#  Author:   Even Rouault <even.rouault at spatialys.com>
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

import os.path
from numbers import Number
from pathlib import Path
from typing import Sequence, Union, List, Tuple
from enum import Enum

PathLike = Union[str, Path]
SequanceNotString = Union[List, Tuple]


def enum_to_str(enum_or_str: Union[Enum, str]) -> str:
    return enum_or_str.name if isinstance(enum_or_str, Enum) else str(enum_or_str)


def is_path_like(s) -> bool:
    return isinstance(s, PathLike.__args__)


def get_suffix(filename: PathLike) -> str:
    return Path(filename).suffix  # same as os.path.splitext(filename)[1]


def get_extension(filename: PathLike) -> str:
    """
    returns the suffix without the leading dot.
    special case for shp.zip
    """
    if str(filename).lower().endswith('.shp.zip'):
        return 'shp.zip'
    ext = get_suffix(filename)
    if ext.startswith('.'):
        ext = ext[1:]
    return ext


def is_sequence(f) -> bool:
    return isinstance(f, Sequence)


def get_byte(number: int, i: int):
    """ returns the i-th byte from an integer"""
    return (number & (0xff << (i * 8))) >> (i * 8)


def path_join(*args):
    return os.path.join(*(str(arg) for arg in args))


def num(s: str) -> Number:
    try:
        return int(s)
    except ValueError:
        return float(s)
