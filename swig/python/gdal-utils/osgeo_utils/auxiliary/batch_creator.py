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
# SPDX-License-Identifier: MIT
# ******************************************************************************

import os
import pkgutil
import sys
from pathlib import Path
from typing import List, Optional, Sequence

import osgeo_utils
from osgeo_utils.auxiliary.base import PathLikeOrStr


def batch_creator(
    filename_list: Sequence[PathLikeOrStr],
    batch_content: str = r'@python "%~dp0\%~n0.py" %*',
):
    """
    :param filename_list: list of file names (full path)
    :param batch_content: contents of the wrapper batch file
    :return: 0 if succeeded, 1 otherwise
    The function create a wrapper batch file for each existing python file for invoking as a script
    It is useful on Windows if the file association of *.py files is not python.exe (but some IDE for instance)
    """
    try:
        for script_name in filename_list:
            py_name = Path(script_name).with_suffix(".py")
            if os.path.exists(py_name):
                batch_name = py_name.with_suffix(".bat")
                print(f"Creating: {batch_name}...")
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


def batch_creator_by_modules(
    script_names: Sequence[str] = None, root: Optional[PathLikeOrStr] = None
):
    if root is None:
        root = Path(sys.executable).parents[0] / "Scripts"
    if script_names is None:
        script_names = get_sub_modules(osgeo_utils)
    scripts = [Path(root) / Path(s).name for s in script_names]
    return batch_creator(scripts)


def main(argv=sys.argv):
    scripts_list = None if len(argv) <= 1 else argv[1:]
    return batch_creator_by_modules(scripts_list)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
