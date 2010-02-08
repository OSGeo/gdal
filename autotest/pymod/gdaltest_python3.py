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

import urllib.request, urllib.error, urllib.parse
import socket
import subprocess
import shlex

def run_func(func):
    try:
        result = func()
        print(result)
        return result
    except SystemExit as x:
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
    timeout = 10
    old_timeout = socket.getdefaulttimeout()
    socket.setdefaulttimeout(timeout)
    try:
        handle = urllib.request.urlopen(url)
        socket.setdefaulttimeout(old_timeout)
        return handle
    except urllib.error.HTTPError as e:
        print('HTTP service for %s is down (HTTP Error: %d)' % (url, e.code))
        socket.setdefaulttimeout(old_timeout)
        return None
    except urllib.error.URLError as e:
        print('HTTP service for %s is down (URL Error: %s)' % (url, e.reason))
        socket.setdefaulttimeout(old_timeout)
        return None
    except:
        print('HTTP service for %s is down.' %(url))
        socket.setdefaulttimeout(old_timeout)
        return None

def runexternal(cmd, strin = None):
    command = shlex.split(cmd)
    if strin is None:
        p = subprocess.Popen(command, stdout=subprocess.PIPE)
    else:
        p = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        p.stdin.write(bytes(strin, 'ascii'))
        p.stdin.close()

    if p.stdout is not None:
        ret = p.stdout.read().decode('ascii')
        p.stdout.close()
    else:
        ret = ''

    p.wait()

    return ret

def runexternal_out_and_err(cmd):
    command = shlex.split(cmd)
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if p.stdout is not None:
        ret_stdout = p.stdout.read().decode('ascii')
        p.stdout.close()
    else:
        ret_stdout = ''

    if p.stderr is not None:
        ret_stderr = p.stderr.read().decode('ascii')
        p.stderr.close()
    else:
        ret_stderr = ''

    p.wait()

    return (ret_stdout, ret_stderr)
