# -*- coding: utf-8 -*-
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
import socket
import os
import sys
from sys import version_info
from Queue import Queue
from threading import Thread

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

def urlescape(url):
    # Escape any non-ASCII characters
    try:
        import urllib
        url = urllib.quote(url)
    except:
        pass
    return url

def gdalurlopen(url):
    timeout = 10
    old_timeout = socket.getdefaulttimeout()
    socket.setdefaulttimeout(timeout)

    if 'GDAL_HTTP_PROXY' in os.environ:
        proxy = os.environ['GDAL_HTTP_PROXY']

        if 'GDAL_HTTP_PROXYUSERPWD' in os.environ:
            proxyuserpwd = os.environ['GDAL_HTTP_PROXYUSERPWD']
            proxyHandler = urllib2.ProxyHandler({"http" : \
                "http://%s@%s" % (proxyuserpwd, proxy)})
        else:
            proxyuserpwd = None
            proxyHandler = urllib2.ProxyHandler({"http" : \
                "http://%s" % (proxy)})

        opener = urllib2.build_opener(proxyHandler, urllib2.HTTPHandler)

        urllib2.install_opener(opener)

    try:
        handle = urllib2.urlopen(url)
        socket.setdefaulttimeout(old_timeout)
        return handle
    except urllib2.HTTPError, e:
        print('HTTP service for %s is down (HTTP Error: %d)' % (url, e.code))
        socket.setdefaulttimeout(old_timeout)
        return None
    except urllib2.URLError, e:
        print('HTTP service for %s is down (HTTP Error: %s)' % (url, e.reason))
        socket.setdefaulttimeout(old_timeout)
        return None
    except:
        print('HTTP service for %s is down.' %(url))
        socket.setdefaulttimeout(old_timeout)
        return None

def warn_if_memleak(cmd, out_str):

    # If DEBUG_VSIMALLOC_STATS is defined, this is an easy way
    # to catch some memory leaks
    if cmd.find('--utility_version') == -1 and \
       out_str.find('VSIMalloc + VSICalloc - VSIFree') != -1 and \
       out_str.find('VSIMalloc + VSICalloc - VSIFree : 0') == -1:
        print('memory leak detected')
        print(out_str)

def spawn_async26(cmd):
    import shlex
    import subprocess
    command = shlex.split(cmd)
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE)
        return (process, process.stdout)
    except:
        return (None, None)

def spawn_async(cmd):
    if version_info >= (2,6,0):
        return spawn_async26(cmd)

    import popen2
    try:
        process = popen2.Popen3(cmd)
    except:
        return (None, None)
    if process is None:
        return (None, None)
    process.tochild.close()
    return (process, process.fromchild)

def wait_process(process):
    process.wait()

def runexternal(cmd, strin = None, check_memleak = True, display_live_on_parent_stdout = False):
    if strin is None:
        ret_stdout = os.popen(cmd)
    else:
        (ret_stdin, ret_stdout) = os.popen2(cmd)
        ret_stdin.write(strin)
        ret_stdin.close()

    if display_live_on_parent_stdout:
        out_str = ''
        while True:
            c = ret_stdout.read(1)
            if c == '':
                break
            out_str = out_str + c
            sys.stdout.write(c)
        ret_stdout.close()
    else:
        out_str = ret_stdout.read()
    ret_stdout.close()

    if check_memleak:
        warn_if_memleak(cmd, out_str)

    return out_str

def read_in_thread(f, q):
    q.put(f.read())
    f.close()
    
def runexternal_out_and_err(cmd, check_memleak = True):
    (ret_stdin, ret_stdout, ret_stderr) = os.popen3(cmd)
    ret_stdin.close()
    
    q_stdout = Queue()
    t_stdout = Thread(target=read_in_thread, args=(ret_stdout, q_stdout))
    q_stderr = Queue()
    t_stderr = Thread(target=read_in_thread, args=(ret_stderr, q_stderr))
    t_stdout.start()
    t_stderr.start()
    
    out_str = q_stdout.get()
    err_str = q_stderr.get()

    if check_memleak:
        warn_if_memleak(cmd, out_str)

    return (out_str, err_str)
