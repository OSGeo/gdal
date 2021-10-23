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
import os
import sys
import argparse
import shlex
from abc import ABC, abstractmethod
from gettext import gettext
from typing import Union
from warnings import warn


class ExtendAction(argparse.Action):

    def __call__(self, parser, namespace, values, option_string=None):
        items = getattr(namespace, self.dest) or []
        items.extend(values)
        setattr(namespace, self.dest, items)


class GDALArgumentParser(argparse.ArgumentParser):

    def __init__(self, title=None, description=None, formatter_class=None,
                 fromfile_prefix_chars='@', add_help: Union[str, bool] = True, **kwargs):
        custom_help = isinstance(add_help, str)
        if title:
            if not description:
                description = title
            else:
                if formatter_class is None:
                    formatter_class = argparse.RawDescriptionHelpFormatter
                description = f'{title}\n{"-"*(2+len(title))}\n{description}'

        super().__init__(fromfile_prefix_chars=fromfile_prefix_chars, description=description,
                         formatter_class=formatter_class,
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


class GDALScript(ABC):
    def __init__(self, **kwargs):
        self.prog = None
        self.title = None
        self.description = None
        self.examples = None
        self.add_help = True
        self.optfile_arg = None
        self._parser = None
        self.examples = []
        self.epilog = None
        self.kwargs = kwargs

    def add_example(self, title, arguments):
        example = (title, arguments)
        self.examples.append(example)

    @property
    def parser(self):
        if self._parser is None:
            self._parser = GDALArgumentParser(
                prog=self.prog, title=self.title, description=self.description,
                add_help=self.add_help, epilog=self.get_epilog(), **self.kwargs)
        return self._parser

    @parser.setter
    def parser(self, value):
        self._parser = value

    @abstractmethod
    def get_parser(self, argv) -> GDALArgumentParser:
        pass

    @abstractmethod
    def doit(self, **kwargs):
        pass

    def augment_kwargs(self, kwargs) -> dict:
        return kwargs

    def parse(self, argv) -> dict:
        parser = self.get_parser(argv)
        args = parser.parse_args(argv, optfile_arg=self.optfile_arg)
        kwargs = vars(args)
        kwargs = self.augment_kwargs(kwargs)
        return kwargs

    def main(self, argv) -> int:
        kwargs = self.parse(argv[1:])
        try:
            self.doit(**kwargs)
            return 0
        except Exception as e:
            print(e)
            return 1

    def get_epilog(self):
        prog = self.prog
        if prog is None:
            prog = os.path.basename(sys.argv[0])
        example_list = []
        for idx, (title, args) in enumerate(self.examples):
            example_list.append(f'example #{idx+1}: {title}\n{prog} {args}')
        epilog = '\n\n'.join(example_list)
        if self.epilog:
            epilog = epilog + '\n\n' + self.epilog
        return epilog or None
