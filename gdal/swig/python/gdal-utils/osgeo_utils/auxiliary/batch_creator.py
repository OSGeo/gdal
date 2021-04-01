#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Create Windows batch wrappers to run the python scripts
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
import sys
import pkgutil
from pathlib import Path
from typing import Sequence, Optional, List

import osgeo_utils
from osgeo_utils.auxiliary.base import PathLike


def batch_creator(filename_list: Sequence[PathLike], batch_content: str = r'@python "%~dp0\%~n0.py" %*'):
    """
    :param filename_list: list of file names (full path)
    :param batch_content: contents of the wrapper batch file
    :return: 0 if succeeded, 1 otherwise
    The function create a wrapper batch file for each existing python file for invoking as a script
    It is useful on Windows if the file association of *.py files is not python.exe (but some IDE for instance)
    """
    try:
        for script_name in filename_list:
            py_name = Path(script_name).with_suffix('.py')
            if os.path.exists(py_name):
                batch_name = py_name.with_suffix('.bat')
                print(f'Creating: {batch_name}...')
                with open(batch_name, "w") as file:
                    file.write(batch_content)
        return 0
    except Exception:
        return 1


def get_sub_modules(module) -> List[str]:
    sub_modules = []
    for sub_module in pkgutil.walk_packages(module.__path__):
        _, sub_module_name, _ = sub_module
        sub_modules.append(sub_module_name)
    return sub_modules


def batch_creator_by_modules(script_names: Sequence[str] = None, root: Optional[PathLike] = None):
    if root is None:
        root = Path(sys.executable).parents[0] / 'Scripts'
    if script_names is None:
        script_names = get_sub_modules(osgeo_utils)
    scripts = [root / Path(s).name for s in script_names]
    return batch_creator(scripts)


def main(argv):
    scripts_list = None if len(argv) <= 1 else argv[1:]
    return batch_creator_by_modules(scripts_list)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
