###############################################################################
# $Id$
# 
# Project:  GDAL/OGR Test Suite
# Purpose:  Python Library supporting GDAL/OGR Test Suite
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

import urllib2
import os

def run_func(func):
    try:
        result = func()
        print(result)
        return result
    except SystemExit, x:
        import traceback
        traceback.print_exc()
        
        raise x
    except:
        result = 'fail (blowup)'
        print(result)
        
        import traceback
        traceback.print_exc()    
        return result
        
def gdalurlopen(url):
    try:
        handle = urllib2.urlopen(url)
        return handle
    except urllib2.HTTPError, e:
        print('HTTP service for %s is down (HTTP Error: %d)' % (url, e.code))
        return None
    except urllib2.URLError, e:
        print('HTTP service for %s is down (HTTP Error: %s)' % (url, e.reason))
        return None
    except:
        print('HTTP service for %s is down.' %(url))
        return None

def runexternal(cmd, strin = None):
    if strin is None:
        return os.popen(cmd).read()
    else:
        (ret_stdin, ret_stdout) = os.popen2(cmd)
        ret_stdin.write(strin)
        ret_stdin.close()
        return ret_stdout.read()

def runexternal_out_and_err(cmd):
    (ret_stdin, ret_stdout, ret_stderr) = os.popen3(cmd)
    return (ret_stdout.read(), ret_stderr.read())
