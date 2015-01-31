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
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import os
import sys
from queue import Queue
from threading import Thread

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

def urlescape(url):
    # Escape any non-ASCII characters
    try:
        import urllib
        url = urllib.parse.quote(url)
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
            proxyHandler = urllib.request.ProxyHandler({"http" : \
                "http://%s@%s" % (proxyuserpwd, proxy)})
        else:
            proxyuserpwd = None
            proxyHandler = urllib.request.ProxyHandler({"http" : \
                "http://%s" % (proxy)})

        opener = urllib.request.build_opener(proxyHandler, urllib.request.HTTPHandler)

        urllib.request.install_opener(opener)

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

def spawn_async(cmd):
    command = shlex.split(cmd)
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE)
        return (process, process.stdout)
    except:
        return (None, None)

def wait_process(process):
    process.wait()

def runexternal(cmd, strin = None, check_memleak = True, display_live_on_parent_stdout = False):
    command = shlex.split(cmd)
    if strin is None:
        p = subprocess.Popen(command, stdout=subprocess.PIPE)
    else:
        p = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        p.stdin.write(bytes(strin, 'ascii'))
        p.stdin.close()

    if p.stdout is not None:
        if display_live_on_parent_stdout:
            ret = ''
            ret_stdout = p.stdout
            while True:
                c = ret_stdout.read(1).decode('latin1')
                if c == '':
                    break
                ret = ret + c
                sys.stdout.write(c)
        else:
            ret = p.stdout.read().decode('latin1')
        p.stdout.close()
    else:
        ret = ''

    p.wait()

    return ret

def read_in_thread(f, q):
    q.put(f.read())
    f.close()
    
def runexternal_out_and_err(cmd, check_memleak = True):
    command = shlex.split(cmd)
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if p.stdout is not None:
        q_stdout = Queue()
        t_stdout = Thread(target=read_in_thread, args=(p.stdout, q_stdout))
        t_stdout.start()
    else:
        q_stdout = None
        ret_stdout = ''

    if p.stderr is not None:
        q_stderr = Queue()
        t_stderr = Thread(target=read_in_thread, args=(p.stderr, q_stderr))
        t_stderr.start()
    else:
        q_stderr = None
        ret_stderr = ''
        
    if q_stdout is not None:
        ret_stdout = q_stdout.get().decode('ascii')
    if q_stderr is not None:
        ret_stderr = q_stderr.get().decode('ascii')

    p.wait()

    return (ret_stdout, ret_stderr)
