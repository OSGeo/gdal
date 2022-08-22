#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  array and scalar related types functions
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
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
import array
from typing import TYPE_CHECKING, Sequence, Union

ScalarLike = Union[
    int,
    float,
]

if TYPE_CHECKING:
    # avoid "TypeError: Subscripted generics cannot be used with class and instance checks" while using isinstance()
    ArrayLike = Union[
        ScalarLike,
        Sequence[ScalarLike],
        array.array,
    ]
else:
    ArrayLike = Union[
        ScalarLike,
        Sequence,
        array.array,
    ]

try:
    from numpy import ndarray

    ArrayLike = Union[ArrayLike, ndarray]
except ImportError:
    pass

ArrayOrScalarLike = Union[ArrayLike, ScalarLike]


def array_dist(
    x: ArrayOrScalarLike, y: ArrayOrScalarLike, is_max: bool = True
) -> ScalarLike:
    if isinstance(x, ScalarLike.__args__):
        return abs(x - y)
    try:
        from osgeo_utils.auxiliary.numpy_util import array_dist as np_array_dist

        return np_array_dist(x=x, y=y, is_max=is_max)
    except ImportError:
        return (
            max(abs(a - b) for a, b in zip(x, y))
            if is_max
            else max(abs(a - b) for a, b in zip(x, y))
        )
