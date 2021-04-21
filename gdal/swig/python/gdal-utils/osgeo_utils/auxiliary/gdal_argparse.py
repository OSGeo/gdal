#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  an extended argparse.ArgumentParser to be used with all gdal-utils
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

import sys
import argparse
import shlex
from gettext import gettext
from typing import Union
from warnings import warn


class ExtendAction(argparse.Action):

    def __call__(self, parser, namespace, values, option_string=None):
        items = getattr(namespace, self.dest) or []
        items.extend(values)
        setattr(namespace, self.dest, items)


class GDALArgumentParser(argparse.ArgumentParser):

    def __init__(self, fromfile_prefix_chars='@', add_help: Union[str, bool] = True, **kwargs):
        custom_help = isinstance(add_help, str)
        super().__init__(fromfile_prefix_chars=fromfile_prefix_chars,
                         add_help=add_help and not custom_help, **kwargs)
        if custom_help:
            self.add_argument(add_help, action='help', default=argparse.SUPPRESS,
                              help=gettext('show this help message and exit'))
        if sys.version_info < (3, 8):
            # extend was introduced to the stdlib in Python 3.8
            self.register('action', 'extend', ExtendAction)

    def parse_args(self, args=None, optfile_arg=None, **kwargs):
        if (args is not None
           and optfile_arg in args
           and self.fromfile_prefix_chars is not None
           and optfile_arg is not None):

            # Replace '--optfile x' with the standard '@x'
            prefix = self.fromfile_prefix_chars[0]
            count = len(args)
            new_args = []
            i = 0
            while i < count:
                arg = args[i]
                if arg == optfile_arg:
                    if i == count - 1:
                        raise Exception(f'Missing filename argument following {optfile_arg}: ')
                    arg = prefix + args[i + 1]
                    warn(f'"{optfile_arg} {args[i + 1]}" is deprecated. '
                         f'Please use "{arg}" instead.', DeprecationWarning)
                    i += 1
                i += 1
                new_args.append(arg)
            args = new_args
        return super().parse_args(args=args, **kwargs)

    def convert_arg_line_to_args(self, arg_line):
        return shlex.split(arg_line, comments=True)
