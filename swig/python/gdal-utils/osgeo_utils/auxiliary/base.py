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
from numbers import Real, Number
from pathlib import Path
from typing import Sequence, Union, List, Tuple, TypeVar, Optional
from enum import Enum

T = TypeVar("T")
MaybeSequence = Union[T, Sequence[T]]
PathLikeOrStr = Union[str, os.PathLike]
SequenceNotString = Union[List, Tuple]
Real2D = Tuple[Real, Real]
OptionalBoolStr = Optional[Union[str, bool]]


def enum_to_str(enum_or_str: Union[Enum, str]) -> str:
    return enum_or_str.name if isinstance(enum_or_str, Enum) else str(enum_or_str)


def is_path_like(s) -> bool:
    return isinstance(s, PathLikeOrStr.__args__)


def get_suffix(filename: PathLikeOrStr) -> str:
    return Path(filename).suffix  # same as os.path.splitext(filename)[1]


def get_extension(filename: PathLikeOrStr) -> str:
    """
    returns the suffix without the leading dot.
    special case for shp.zip
    """
    if os.fspath(filename).lower().endswith('.shp.zip'):
        return 'shp.zip'
    ext = get_suffix(filename)
    if ext.startswith('.'):
        ext = ext[1:]
    return ext


def get_byte(number: int, i: int):
    """ returns the i-th byte from an integer"""
    return (number & (0xff << (i * 8))) >> (i * 8)


def path_join(*args) -> str:
    return os.path.join(*(str(arg) for arg in args))


def num(s: Union[Number, str]) -> Number:
    if isinstance(s, Number):
        return s
    else:
        try:
            return int(s)
        except ValueError:
            return float(s)


def num_or_none(s: Optional[Union[Number, str]]) -> Optional[Number]:
    try:
        return num(s)
    except Exception:
        return None


def is_true(b: OptionalBoolStr, accept_none: bool = False,
            case_insensitive=True,
            false_str=('NO', 'FALSE', 'OFF'),
            true_str=('YES', 'TRUE', 'ON')) -> Optional[bool]:
    """
    Returns a boolean value that is represented by a string or a bool
    correlated to the c++ implementation:
    https://github.com/OSGeo/gdal/blob/362541e961cf8cab046dfbae1bde8ce559b4cb40/gdal/gcore/gdaldriver.cpp#L1973
    """
    if isinstance(b, bool):
        return b
    if not b and accept_none:
        return None
    if isinstance(b, str):
        if case_insensitive:
            b = b.upper()
        if b in false_str:
            return False
        if b in true_str:
            return True
    raise Exception(f'{b} is not accepted as a valid boolean-like value')

