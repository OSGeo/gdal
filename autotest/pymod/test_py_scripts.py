#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Helper functions for testing python utilities
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault @ mines-paris dot org>
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

import gdal
import gdaltest
import os

###############################################################################
# Return the path in which the Python script is found
#
def get_py_script(script_name):
    
    for subdir in [ 'scripts', 'samples' ]:
        try:
            # Test subversion layout : {root_dir}/gdal, {root_dir}/autotest
            test_path = os.path.join(os.getcwd(), '..', '..', 'gdal', 'swig', 'python', subdir)
            test_file_path = os.path.join(test_path, script_name + '.py')
            os.stat(test_file_path)
            return test_path
        except:
            try:
                # Test FrankW's directory layout : {root_dir}/gdal, {root_dir}/gdal/autotest
                test_path = os.path.join(os.getcwd(), '..', '..', 'swig', 'python', subdir)
                test_file_path = os.path.join(test_path, script_name + '.py')
                os.stat(test_file_path)
                return test_path
            except:
                pass

    return None

###############################################################################
# Utility function of run_py_script_as_py_module()
#
def find_main_in_module(names):
    global has_main
    has_main = 'main' in names


###############################################################################
# Runs a Python script
#
def run_py_script(script_path, script_name, concatenated_argv):

    run_as_external_script = gdal.GetConfigOption('RUN_AS_EXTERNAL_SCRIPT', 'NO')

    if run_as_external_script == 'yes' or run_as_external_script == 'YES':
        return run_py_script_as_external_script(script_path, script_name, concatenated_argv)
    else:
        return run_py_script_as_py_module(script_path, script_name, concatenated_argv)


###############################################################################
# Runs a Python script in a new process
#
def run_py_script_as_external_script(script_path, script_name, concatenated_argv):

    script_file_path = os.path.join(script_path, script_name + '.py')

    #print(script_file_path + ' ' + concatenated_argv)

    py_interpreter = gdal.GetConfigOption('PYTHON_INTERPRETER', None)
    if py_interpreter is None:
        return gdaltest.runexternal(script_file_path + ' ' + concatenated_argv)
    else:
        return gdaltest.runexternal(py_interpreter + ' ' + script_file_path + ' ' + concatenated_argv)

###############################################################################
# Runs a Python script as a py module
#
# This function is an interesting concentrate of dirty hacks to run python
# scripts without forking a new process. This way we don't need to know the
# name and path of the python interpreter.
#
def run_py_script_as_py_module(script_path, script_name, concatenated_argv):
    import sys

    # Save original sys variables
    saved_syspath = sys.path
    saved_sysargv = sys.argv

    sys.path.append(script_path)

    # Replace argv by user provided one
    # Add first a fake first arg that we set to be the script
    # name but which could be any arbitrary name
    sys.argv = [ script_name + '.py' ]

    import shlex
    sys.argv.extend(shlex.split(concatenated_argv))

    has_imported_module = False

    ret = None

    # Redirect stdout to file
    fout = open('tmp/stdout.txt', 'wt')
    ori_stdout = sys.stdout
    sys.stdout = fout

    #try:
    if True:
        exec('import ' + script_name)
        has_imported_module = True

        # Detect if the script has a main() function
        exec('find_main_in_module(dir(' + script_name + '))')

        # If so, run it (otherwise the import has already run the script)
        if has_main:
            exec(script_name + '.main()')

    #except:
    #    pass

    # Restore original stdout
    fout.close()
    sys.stdout = ori_stdout

    fout = open('tmp/stdout.txt', 'rt')
    ret = fout.read()
    fout.close()

    os.remove('tmp/stdout.txt')

    # Restore original sys variables
    sys.path = saved_syspath
    sys.argv = saved_sysargv

    if has_imported_module:
        # Unload the module so that it gets imported again next time
        # (usefull if wanting to run a script without main() function
        # several time)
        del sys.modules[script_name]

    return ret


