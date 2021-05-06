#!/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  GDAL
# Purpose:  Handles progress callback functions
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2020, Idan Miara <idan@miara.com>
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
from enum import Enum, auto
from numbers import Real
from typing import Optional, Union, Callable

from osgeo.gdal import TermProgress_nocb


class PredefinedProgressCallback(Enum):
    TermProgress = auto()  # default gdal core term progress
    TermProgressPy = auto()  # use this option to be able to put a breakpoint inside the callback
    TermProgressSimple = auto()  # simple print progress


ProgressCallback = Optional[Callable[[Real], None]]
OptionalProgressCallback = Union[type(...), PredefinedProgressCallback, ProgressCallback]


def simple_term_progress(progress: Real):
    print(str(round(progress * 100)) + '%', end=" ")


def term_progress_from_to(r0: Optional[Real], r1: Real):
    """ prints the progress from r0 to r1 """
    i0 = 0 if (r0 is None) or (r0 > r1) else round(r0 * 100) + 1
    i1 = round(r1 * 100) + 1
    for i in range(i0, i1):
        print(str(i) if i % 5 == 0 else ".", end="", flush=True)
    if r1 >= 1:
        print("% done!")


def get_py_term_progress_callback():
    term_progress_py_last = None

    def py_term_progress(progress: Real):
        nonlocal term_progress_py_last

        r0 = term_progress_py_last
        r1 = progress
        term_progress_from_to(r0, r1)
        term_progress_py_last = progress
    return py_term_progress


def get_progress_callback(callback: OptionalProgressCallback) -> ProgressCallback:
    """ returns a predefined callback or the given callback """
    if callback is None:
        return None
    elif callback == PredefinedProgressCallback.TermProgress or callback == ...:
        return TermProgress_nocb
    elif callback == PredefinedProgressCallback.TermProgressPy:
        return get_py_term_progress_callback()
    elif callback == PredefinedProgressCallback.TermProgressSimple:
        return simple_term_progress

    return callback
