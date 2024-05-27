#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Helper functions for testing python utilities
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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

import importlib
import os
import shlex
import sys

import gdaltest

###############################################################################
# Return the path in which the Python script is found
#

# path relative to gdal root
scripts_subdir = "swig/python/gdal-utils/scripts"
utils_subdir = "swig/python/gdal-utils/osgeo_utils"
samples_subdir = utils_subdir + "/samples"
samples_path = "../../" + samples_subdir


def get_data_path(dir):
    return f"../{dir}/data/"


def get_py_script(script_name):
    build_dir = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))

    # now we need to look for the script in the utils or samples subdirs...
    for subdir in [scripts_subdir, samples_subdir]:
        try:
            test_path = os.path.join(build_dir, subdir)
            test_file_path = os.path.join(test_path, script_name + ".py")
            os.stat(test_file_path)
            return test_path
        except OSError:
            pass

    return None


###############################################################################
# Runs a Python script
# Alias of run_py_script_as_external_script()
#
def run_py_script(
    script_path: str,
    script_name: str,
    concatenated_argv: str,
    run_as_script: bool = True,
    run_as_module: bool = False,
    return_stderr: bool = False,
):
    if run_as_module:
        try:
            module = importlib.import_module("osgeo_utils." + script_name)
        except ImportError:
            module = importlib.import_module("osgeo_utils.samples." + script_name)
        argv = [module.__file__] + shlex.split(concatenated_argv)
        return module.main(argv)

    if run_as_script:
        return run_py_script_as_external_script(
            script_path, script_name, concatenated_argv, return_stderr=return_stderr
        )

    raise Exception("either run_as_script or run_as_module should be specified")


###############################################################################
# Runs a Python script in a new process
#
def run_py_script_as_external_script(
    script_path,
    script_name,
    concatenated_argv,
    display_live_on_parent_stdout=False,
    return_stderr: bool = False,
):

    script_file_path = os.path.join(script_path, script_name + ".py")

    # print(script_file_path + ' ' + concatenated_argv)

    python_exe = sys.executable
    if sys.platform == "win32":
        python_exe = python_exe.replace("\\", "/")
        script_file_path = script_file_path.replace("\\", "/")

    if return_stderr:
        assert (
            not display_live_on_parent_stdout
        ), "display_live_on_parent_stdout=True and return_stderr=True aren't supported at the same time"
        return gdaltest.runexternal_out_and_err(
            python_exe + ' "' + script_file_path + '" ' + concatenated_argv
        )
    return gdaltest.runexternal(
        python_exe + ' "' + script_file_path + '" ' + concatenated_argv,
        display_live_on_parent_stdout=display_live_on_parent_stdout,
    )
