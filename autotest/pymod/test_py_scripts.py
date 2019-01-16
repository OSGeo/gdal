#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Helper functions for testing python utilities
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
import gdaltest

###############################################################################
# Return the path in which the Python script is found
#


def get_py_script(script_name):

    for subdir in ['scripts', 'samples']:
        try:
            # Test subversion layout : {root_dir}/gdal, {root_dir}/autotest
            test_path = os.path.join(os.getcwd(), '..', '..', 'gdal', 'swig', 'python', subdir)
            test_file_path = os.path.join(test_path, script_name + '.py')
            os.stat(test_file_path)
            return test_path
        except OSError:
            try:
                # Test FrankW's directory layout : {root_dir}/gdal, {root_dir}/gdal/autotest
                test_path = os.path.join(os.getcwd(), '..', '..', 'swig', 'python', subdir)
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
def run_py_script(script_path, script_name, concatenated_argv):
    return run_py_script_as_external_script(script_path, script_name, concatenated_argv)


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
