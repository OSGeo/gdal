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

import os
import sys
import shlex

import gdaltest
import importlib

###############################################################################
# Return the path in which the Python script is found
#

# path relative to gdal root
scripts_subdir = 'swig/python/gdal-utils/scripts'
utils_subdir = 'swig/python/gdal-utils/osgeo_utils'
samples_subdir = utils_subdir + '/samples'
samples_path = '../../' + samples_subdir


def get_data_path(dir):
    return f'../{dir}/data/'


def get_py_script(script_name):
    # how to get to {root_dir}/gdal from {root_dir}/autotest/X
    base_gdal_path = os.path.join(os.getcwd(), '..', '..')
    # now we need to look for the script in the utils or samples subdirs...
    for subdir in [scripts_subdir, samples_subdir]:
        try:
            test_path = os.path.join(base_gdal_path, subdir)
            test_file_path = os.path.join(test_path, script_name + '.py')
            os.stat(test_file_path)
            return test_path
        except OSError:
            pass

    return None


###############################################################################
# Runs a Python script
# Alias of run_py_script_as_external_script()
#
def run_py_script(script_path: str, script_name: str, concatenated_argv: str,
                  run_as_script: bool = True, run_as_module: bool = False):
    result = None
    if run_as_module:
        try:
            module = importlib.import_module('osgeo_utils.' + script_name)
        except ImportError:
            module = importlib.import_module('osgeo_utils.samples.' + script_name)
        argv = [module.__file__] + shlex.split(concatenated_argv)
        result = module.main(argv)
    if run_as_script:
        result = run_py_script_as_external_script(script_path, script_name, concatenated_argv)
    return result


###############################################################################
# Runs a Python script in a new process
#
def run_py_script_as_external_script(script_path, script_name, concatenated_argv, display_live_on_parent_stdout=False):

    script_file_path = os.path.join(script_path, script_name + '.py')

    # print(script_file_path + ' ' + concatenated_argv)

    python_exe = sys.executable
    if sys.platform == 'win32':
        python_exe = python_exe.replace('\\', '/')
        script_file_path = script_file_path.replace('\\', '/')

    return gdaltest.runexternal(python_exe + ' ' + script_file_path + ' ' + concatenated_argv, display_live_on_parent_stdout=display_live_on_parent_stdout)
